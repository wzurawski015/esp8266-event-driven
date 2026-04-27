#include "ev/esp8266_port_adapters.h"

#include <string.h>

/*
 * Network adapter scaffold:
 * The repository does not currently contain verifiable ESP8266 WiFi/MQTT SDK
 * headers for a production adapter.  This file intentionally exposes an
 * unsupported, non-blocking ev_net_port_t so portable code and host fakes can
 * exercise the HSHA network airlock without inventing SDK symbols.
 */

typedef struct ev_esp8266_net_ctx {
    ev_net_stats_t stats;
} ev_esp8266_net_ctx_t;

static ev_esp8266_net_ctx_t g_ev_esp8266_net_ctx;

static ev_result_t ev_esp8266_net_init_fn(void *ctx)
{
    if (ctx == NULL) {
        return EV_ERR_INVALID_ARG;
    }
    return EV_ERR_UNSUPPORTED;
}

static ev_result_t ev_esp8266_net_start_fn(void *ctx)
{
    if (ctx == NULL) {
        return EV_ERR_INVALID_ARG;
    }
    return EV_ERR_UNSUPPORTED;
}

static ev_result_t ev_esp8266_net_stop_fn(void *ctx)
{
    if (ctx == NULL) {
        return EV_ERR_INVALID_ARG;
    }
    return EV_ERR_UNSUPPORTED;
}

static ev_result_t ev_esp8266_net_publish_mqtt_fn(void *ctx, const ev_net_mqtt_publish_cmd_t *cmd)
{
    ev_esp8266_net_ctx_t *net = (ev_esp8266_net_ctx_t *)ctx;

    if ((net == NULL) || (cmd == NULL)) {
        return EV_ERR_INVALID_ARG;
    }
    ++net->stats.tx_attempts;
    ++net->stats.tx_failed;
    return EV_ERR_UNSUPPORTED;
}

static ev_result_t ev_esp8266_net_poll_ingress_fn(void *ctx, ev_net_ingress_event_t *out_event)
{
    if ((ctx == NULL) || (out_event == NULL)) {
        return EV_ERR_INVALID_ARG;
    }
    (void)ctx;
    memset(out_event, 0, sizeof(*out_event));
    return EV_ERR_EMPTY;
}

static ev_result_t ev_esp8266_net_get_stats_fn(void *ctx, ev_net_stats_t *out_stats)
{
    ev_esp8266_net_ctx_t *net = (ev_esp8266_net_ctx_t *)ctx;

    if ((net == NULL) || (out_stats == NULL)) {
        return EV_ERR_INVALID_ARG;
    }
    *out_stats = net->stats;
    return EV_OK;
}

ev_result_t ev_esp8266_net_port_init(ev_net_port_t *out_port)
{
    if (out_port == NULL) {
        return EV_ERR_INVALID_ARG;
    }

    memset(&g_ev_esp8266_net_ctx, 0, sizeof(g_ev_esp8266_net_ctx));
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
