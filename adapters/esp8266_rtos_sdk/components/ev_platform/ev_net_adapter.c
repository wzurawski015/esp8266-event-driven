#include "ev/esp8266_port_adapters.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "esp_err.h"
#include "esp_event_loop.h"
#include "esp_wifi.h"
#include "mqtt_client.h"
#include "tcpip_adapter.h"

#define EV_ESP8266_NET_WIFI_AUTH_OPEN 0U
#define EV_ESP8266_NET_WIFI_AUTH_WPA2_PSK 1U

/*
 * ESP8266 physical network adapter for the portable HSHA Network Airlock.
 *
 * SDK callbacks never call core, actors, mailboxes, or ev_publish(). They copy
 * bounded event metadata into this static ingress ring. The synchronous app poll
 * loop later drains the ring through ev_net_port_t::poll_ingress and publishes
 * EV_NET_* events into the actor graph.
 */

typedef struct ev_esp8266_net_ctx {
    ev_net_ingress_event_t ring[EV_NET_INGRESS_RING_CAPACITY];
    uint32_t write_seq;
    uint32_t read_seq;
    uint32_t high_watermark;
    uint32_t dropped_events;
    uint32_t dropped_oversize;
    uint32_t dropped_no_payload_slot;
    uint32_t tx_attempts;
    uint32_t tx_ok;
    uint32_t tx_failed;
    bool initialized;
    bool wifi_started;
    bool wifi_connected;
    bool mqtt_supported;
    bool mqtt_started;
    bool mqtt_connected;
    ev_esp8266_net_config_t cfg;
    esp_mqtt_client_handle_t mqtt_client;
} ev_esp8266_net_ctx_t;

static ev_esp8266_net_ctx_t g_ev_esp8266_net_ctx;

static uint32_t ev_esp8266_net_pending_unsafe(const ev_esp8266_net_ctx_t *net)
{
    return (net != NULL) ? (net->write_seq - net->read_seq) : 0U;
}

static void ev_esp8266_net_record_high_watermark_unsafe(ev_esp8266_net_ctx_t *net)
{
    const uint32_t pending = ev_esp8266_net_pending_unsafe(net);

    if ((net != NULL) && (pending > net->high_watermark)) {
        net->high_watermark = pending;
    }
}

static bool ev_esp8266_net_string_is_empty(const char *value)
{
    return (value == NULL) || (value[0] == '\0');
}

static void ev_esp8266_net_push_event(ev_esp8266_net_ctx_t *net, const ev_net_ingress_event_t *event)
{
    uint32_t index;

    if ((net == NULL) || (event == NULL)) {
        return;
    }

    portENTER_CRITICAL();
    if (ev_esp8266_net_pending_unsafe(net) >= EV_NET_INGRESS_RING_CAPACITY) {
        ++net->dropped_events;
        portEXIT_CRITICAL();
        return;
    }

    index = net->write_seq & EV_NET_INGRESS_RING_MASK;
    net->ring[index] = *event;
    ++net->write_seq;
    ev_esp8266_net_record_high_watermark_unsafe(net);
    portEXIT_CRITICAL();
}

static void ev_esp8266_net_push_kind(ev_esp8266_net_ctx_t *net, ev_net_event_kind_t kind)
{
    ev_net_ingress_event_t event;

    memset(&event, 0, sizeof(event));
    event.kind = kind;
    ev_esp8266_net_push_event(net, &event);
}

static void ev_esp8266_net_push_mqtt_data(ev_esp8266_net_ctx_t *net,
                                          const char *topic,
                                          int topic_len,
                                          const char *payload,
                                          int payload_len)
{
    ev_net_ingress_event_t event;

    if ((net == NULL) || (topic == NULL) || (topic_len < 0) || (payload_len < 0) ||
        ((payload_len > 0) && (payload == NULL))) {
        return;
    }

    if (((size_t)topic_len > EV_NET_MAX_TOPIC_BYTES) ||
        ((size_t)payload_len > EV_NET_MAX_INLINE_PAYLOAD_BYTES)) {
        portENTER_CRITICAL();
        ++net->dropped_oversize;
        portEXIT_CRITICAL();
        return;
    }

    memset(&event, 0, sizeof(event));
    event.kind = EV_NET_EVENT_MQTT_MSG_RX;
    event.topic_len = (uint8_t)topic_len;
    event.payload_len = (uint8_t)payload_len;
    if (topic_len > 0) {
        memcpy(event.topic, topic, (size_t)topic_len);
    }
    if (payload_len > 0) {
        memcpy(event.payload, payload, (size_t)payload_len);
    }

    ev_esp8266_net_push_event(net, &event);
}

static wifi_auth_mode_t ev_esp8266_net_auth_mode(uint32_t auth_mode)
{
    if (auth_mode == EV_ESP8266_NET_WIFI_AUTH_WPA2_PSK) {
        return WIFI_AUTH_WPA2_PSK;
    }
    return WIFI_AUTH_OPEN;
}

static void ev_esp8266_net_start_mqtt_if_ready(ev_esp8266_net_ctx_t *net)
{
    if ((net == NULL) || !net->mqtt_supported || (net->mqtt_client == NULL) || net->mqtt_started) {
        return;
    }

    if (esp_mqtt_client_start(net->mqtt_client) == ESP_OK) {
        net->mqtt_started = true;
    }
}

static esp_err_t ev_esp8266_wifi_event_handler(void *ctx, system_event_t *event)
{
    ev_esp8266_net_ctx_t *net = (ev_esp8266_net_ctx_t *)ctx;

    if ((net == NULL) || (event == NULL)) {
        return ESP_OK;
    }

    switch (event->event_id) {
    case SYSTEM_EVENT_STA_GOT_IP:
        net->wifi_connected = true;
        ev_esp8266_net_push_kind(net, EV_NET_EVENT_WIFI_UP);
        ev_esp8266_net_start_mqtt_if_ready(net);
        break;
    case SYSTEM_EVENT_STA_DISCONNECTED:
        net->wifi_connected = false;
        net->mqtt_connected = false;
        ev_esp8266_net_push_kind(net, EV_NET_EVENT_WIFI_DOWN);
        ev_esp8266_net_push_kind(net, EV_NET_EVENT_MQTT_DOWN);
        (void)esp_wifi_connect();
        break;
    default:
        break;
    }

    return ESP_OK;
}

static esp_err_t ev_esp8266_mqtt_event_handler(esp_mqtt_event_handle_t event)
{
    ev_esp8266_net_ctx_t *net;

    if (event == NULL) {
        return ESP_OK;
    }

    net = (ev_esp8266_net_ctx_t *)event->user_context;
    if (net == NULL) {
        return ESP_OK;
    }

    switch (event->event_id) {
    case MQTT_EVENT_CONNECTED:
        net->mqtt_connected = true;
        ev_esp8266_net_push_kind(net, EV_NET_EVENT_MQTT_UP);
        break;
    case MQTT_EVENT_DISCONNECTED:
    case MQTT_EVENT_ERROR:
        net->mqtt_connected = false;
        ev_esp8266_net_push_kind(net, EV_NET_EVENT_MQTT_DOWN);
        break;
    case MQTT_EVENT_DATA:
        ev_esp8266_net_push_mqtt_data(net,
                                      event->topic,
                                      event->topic_len,
                                      event->data,
                                      event->data_len);
        break;
    default:
        break;
    }

    return ESP_OK;
}

static ev_result_t ev_esp8266_net_config_is_valid(const ev_esp8266_net_config_t *cfg)
{
    if ((cfg == NULL) || ev_esp8266_net_string_is_empty(cfg->wifi_ssid) || (cfg->wifi_password == NULL)) {
        return EV_ERR_INVALID_ARG;
    }
    return EV_OK;
}

static ev_result_t ev_esp8266_net_init_fn(void *ctx)
{
    ev_esp8266_net_ctx_t *net = (ev_esp8266_net_ctx_t *)ctx;
    wifi_init_config_t wifi_init_cfg = WIFI_INIT_CONFIG_DEFAULT();
    wifi_config_t wifi_cfg;
    esp_mqtt_client_config_t mqtt_cfg;
    esp_err_t err;

    if (net == NULL) {
        return EV_ERR_INVALID_ARG;
    }
    if (net->initialized) {
        return EV_OK;
    }
    if (ev_esp8266_net_config_is_valid(&net->cfg) != EV_OK) {
        return EV_ERR_INVALID_ARG;
    }

    tcpip_adapter_init();
    err = esp_event_loop_init(ev_esp8266_wifi_event_handler, net);
    if (err != ESP_OK) {
        return EV_ERR_STATE;
    }
    if (esp_wifi_init(&wifi_init_cfg) != ESP_OK) {
        return EV_ERR_STATE;
    }
    if (esp_wifi_set_storage(WIFI_STORAGE_RAM) != ESP_OK) {
        return EV_ERR_STATE;
    }
    if (esp_wifi_set_mode(WIFI_MODE_STA) != ESP_OK) {
        return EV_ERR_STATE;
    }

    memset(&wifi_cfg, 0, sizeof(wifi_cfg));
    (void)strncpy((char *)wifi_cfg.sta.ssid, net->cfg.wifi_ssid, sizeof(wifi_cfg.sta.ssid) - 1U);
    (void)strncpy((char *)wifi_cfg.sta.password, net->cfg.wifi_password, sizeof(wifi_cfg.sta.password) - 1U);
    wifi_cfg.sta.threshold.authmode = ev_esp8266_net_auth_mode(net->cfg.wifi_auth_mode);
    if (esp_wifi_set_config(WIFI_IF_STA, &wifi_cfg) != ESP_OK) {
        return EV_ERR_STATE;
    }

    net->mqtt_supported = !ev_esp8266_net_string_is_empty(net->cfg.mqtt_broker_uri);
    if (net->mqtt_supported) {
        memset(&mqtt_cfg, 0, sizeof(mqtt_cfg));
        mqtt_cfg.uri = net->cfg.mqtt_broker_uri;
        mqtt_cfg.client_id = net->cfg.mqtt_client_id;
        mqtt_cfg.event_handle = ev_esp8266_mqtt_event_handler;
        mqtt_cfg.user_context = net;
        net->mqtt_client = esp_mqtt_client_init(&mqtt_cfg);
        if (net->mqtt_client == NULL) {
            net->mqtt_supported = false;
            return EV_ERR_STATE;
        }
    }

    net->initialized = true;
    return EV_OK;
}

static ev_result_t ev_esp8266_net_start_fn(void *ctx)
{
    ev_esp8266_net_ctx_t *net = (ev_esp8266_net_ctx_t *)ctx;

    if (net == NULL) {
        return EV_ERR_INVALID_ARG;
    }
    if (!net->initialized) {
        return EV_ERR_STATE;
    }
    if (!net->wifi_started) {
        if (esp_wifi_start() != ESP_OK) {
            return EV_ERR_STATE;
        }
        net->wifi_started = true;
    }
    if (esp_wifi_connect() != ESP_OK) {
        return EV_ERR_STATE;
    }
    return EV_OK;
}

static ev_result_t ev_esp8266_net_stop_fn(void *ctx)
{
    ev_esp8266_net_ctx_t *net = (ev_esp8266_net_ctx_t *)ctx;

    if (net == NULL) {
        return EV_ERR_INVALID_ARG;
    }
    if ((net->mqtt_client != NULL) && net->mqtt_started) {
        (void)esp_mqtt_client_stop(net->mqtt_client);
        net->mqtt_started = false;
        net->mqtt_connected = false;
    }
    if (net->wifi_started) {
        (void)esp_wifi_disconnect();
        (void)esp_wifi_stop();
        net->wifi_started = false;
        net->wifi_connected = false;
    }
    return EV_OK;
}

static ev_result_t ev_esp8266_net_publish_mqtt_fn(void *ctx, const ev_net_mqtt_publish_cmd_t *cmd)
{
    ev_esp8266_net_ctx_t *net = (ev_esp8266_net_ctx_t *)ctx;
    char topic[EV_NET_MAX_TOPIC_BYTES + 1U];
    int msg_id;

    if ((net == NULL) || (cmd == NULL)) {
        return EV_ERR_INVALID_ARG;
    }
    ++net->tx_attempts;
    if (!net->mqtt_supported || (net->mqtt_client == NULL)) {
        ++net->tx_failed;
        return EV_ERR_UNSUPPORTED;
    }
    if (!net->mqtt_connected) {
        ++net->tx_failed;
        return EV_ERR_STATE;
    }
    if ((cmd->topic_len > EV_NET_MAX_TOPIC_BYTES) ||
        (cmd->payload_len > EV_NET_MAX_INLINE_PAYLOAD_BYTES) ||
        (cmd->topic_len == 0U)) {
        ++net->tx_failed;
        return EV_ERR_INVALID_ARG;
    }

    memset(topic, 0, sizeof(topic));
    memcpy(topic, cmd->topic, cmd->topic_len);
    msg_id = esp_mqtt_client_publish(net->mqtt_client,
                                     topic,
                                     (const char *)cmd->payload,
                                     (int)cmd->payload_len,
                                     (int)cmd->qos,
                                     (int)cmd->retain);
    if (msg_id < 0) {
        ++net->tx_failed;
        return EV_ERR_STATE;
    }

    ++net->tx_ok;
    return EV_OK;
}

static ev_result_t ev_esp8266_net_poll_ingress_fn(void *ctx, ev_net_ingress_event_t *out_event)
{
    ev_esp8266_net_ctx_t *net = (ev_esp8266_net_ctx_t *)ctx;
    uint32_t index;

    if ((net == NULL) || (out_event == NULL)) {
        return EV_ERR_INVALID_ARG;
    }

    portENTER_CRITICAL();
    if (ev_esp8266_net_pending_unsafe(net) == 0U) {
        portEXIT_CRITICAL();
        return EV_ERR_EMPTY;
    }

    index = net->read_seq & EV_NET_INGRESS_RING_MASK;
    *out_event = net->ring[index];
    ++net->read_seq;
    portEXIT_CRITICAL();
    return EV_OK;
}

static ev_result_t ev_esp8266_net_get_stats_fn(void *ctx, ev_net_stats_t *out_stats)
{
    ev_esp8266_net_ctx_t *net = (ev_esp8266_net_ctx_t *)ctx;

    if ((net == NULL) || (out_stats == NULL)) {
        return EV_ERR_INVALID_ARG;
    }

    portENTER_CRITICAL();
    memset(out_stats, 0, sizeof(*out_stats));
    out_stats->write_seq = net->write_seq;
    out_stats->read_seq = net->read_seq;
    out_stats->pending_events = ev_esp8266_net_pending_unsafe(net);
    out_stats->dropped_events = net->dropped_events;
    out_stats->dropped_oversize = net->dropped_oversize;
    out_stats->dropped_no_payload_slot = net->dropped_no_payload_slot;
    out_stats->high_watermark = net->high_watermark;
    out_stats->tx_attempts = net->tx_attempts;
    out_stats->tx_ok = net->tx_ok;
    out_stats->tx_failed = net->tx_failed;
    portEXIT_CRITICAL();
    return EV_OK;
}

ev_result_t ev_esp8266_net_port_init(ev_net_port_t *out_port, const ev_esp8266_net_config_t *cfg)
{
    if ((out_port == NULL) || (cfg == NULL)) {
        return EV_ERR_INVALID_ARG;
    }
    if (ev_esp8266_net_config_is_valid(cfg) != EV_OK) {
        return EV_ERR_INVALID_ARG;
    }

    memset(&g_ev_esp8266_net_ctx, 0, sizeof(g_ev_esp8266_net_ctx));
    g_ev_esp8266_net_ctx.cfg = *cfg;
    memset(out_port, 0, sizeof(*out_port));
    out_port->ctx = &g_ev_esp8266_net_ctx;
    out_port->init = ev_esp8266_net_init_fn;
    out_port->start = ev_esp8266_net_start_fn;
    out_port->stop = ev_esp8266_net_stop_fn;
    out_port->publish_mqtt = ev_esp8266_net_publish_mqtt_fn;
    out_port->poll_ingress = ev_esp8266_net_poll_ingress_fn;
    out_port->get_stats = ev_esp8266_net_get_stats_fn;
    return EV_OK;
}
