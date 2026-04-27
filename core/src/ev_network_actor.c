#include "ev/network_actor.h"

#include <string.h>

static bool ev_network_message_has_exact_payload(const ev_msg_t *msg, size_t payload_size)
{
    return (msg != NULL) && (ev_msg_payload_size(msg) == payload_size) &&
           ((payload_size == 0U) || (ev_msg_payload_data(msg) != NULL));
}

ev_result_t ev_network_actor_init(ev_network_actor_ctx_t *ctx, ev_net_port_t *net_port)
{
    if (ctx == NULL) {
        return EV_ERR_INVALID_ARG;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->net_port = net_port;
    ctx->state = EV_NETWORK_STATE_DISCONNECTED;
    return EV_OK;
}

static ev_result_t ev_network_actor_handle_wifi_up(ev_network_actor_ctx_t *ctx)
{
    ++ctx->stats.wifi_up_events;
    if (ctx->state == EV_NETWORK_STATE_DISCONNECTED) {
        ctx->state = EV_NETWORK_STATE_WIFI_UP;
    }
    return EV_OK;
}

static ev_result_t ev_network_actor_handle_wifi_down(ev_network_actor_ctx_t *ctx)
{
    ++ctx->stats.wifi_down_events;
    ctx->state = EV_NETWORK_STATE_DISCONNECTED;
    return EV_OK;
}

static ev_result_t ev_network_actor_handle_mqtt_up(ev_network_actor_ctx_t *ctx)
{
    ++ctx->stats.mqtt_up_events;
    ctx->state = EV_NETWORK_STATE_MQTT_CONNECTED;
    return EV_OK;
}

static ev_result_t ev_network_actor_handle_mqtt_down(ev_network_actor_ctx_t *ctx)
{
    ++ctx->stats.mqtt_down_events;
    ctx->state = EV_NETWORK_STATE_WIFI_UP;
    return EV_OK;
}

static ev_result_t ev_network_actor_handle_mqtt_rx(ev_network_actor_ctx_t *ctx, const ev_msg_t *msg)
{
    if (!ev_network_message_has_exact_payload(msg, sizeof(ev_net_ingress_event_t))) {
        ++ctx->stats.bad_payloads;
        return EV_ERR_CONTRACT;
    }
    ++ctx->stats.mqtt_rx_events;
    return EV_OK;
}

static ev_result_t ev_network_actor_handle_tx(ev_network_actor_ctx_t *ctx, const ev_msg_t *msg)
{
    const ev_net_mqtt_publish_cmd_t *cmd;
    ev_result_t rc;

    if (!ev_network_message_has_exact_payload(msg, sizeof(ev_net_mqtt_publish_cmd_t))) {
        ++ctx->stats.bad_payloads;
        return EV_ERR_CONTRACT;
    }

    ++ctx->stats.tx_commands_seen;
    if (ctx->state != EV_NETWORK_STATE_MQTT_CONNECTED) {
        ++ctx->stats.tx_rejected_not_connected;
        return EV_OK;
    }
    if ((ctx->net_port == NULL) || (ctx->net_port->publish_mqtt == NULL)) {
        ++ctx->stats.tx_failed;
        return EV_ERR_UNSUPPORTED;
    }

    cmd = (const ev_net_mqtt_publish_cmd_t *)ev_msg_payload_data(msg);
    ++ctx->stats.tx_attempts;
    rc = ctx->net_port->publish_mqtt(ctx->net_port->ctx, cmd);
    if (rc == EV_OK) {
        ++ctx->stats.tx_ok;
    } else {
        ++ctx->stats.tx_failed;
    }
    return rc;
}

ev_result_t ev_network_actor_handle(void *actor_context, const ev_msg_t *msg)
{
    ev_network_actor_ctx_t *ctx = (ev_network_actor_ctx_t *)actor_context;

    if ((ctx == NULL) || (msg == NULL)) {
        return EV_ERR_INVALID_ARG;
    }

    switch (msg->event_id) {
    case EV_NET_WIFI_UP:
        return ev_network_actor_handle_wifi_up(ctx);
    case EV_NET_WIFI_DOWN:
        return ev_network_actor_handle_wifi_down(ctx);
    case EV_NET_MQTT_UP:
        return ev_network_actor_handle_mqtt_up(ctx);
    case EV_NET_MQTT_DOWN:
        return ev_network_actor_handle_mqtt_down(ctx);
    case EV_NET_MQTT_MSG_RX:
        return ev_network_actor_handle_mqtt_rx(ctx, msg);
    case EV_NET_TX_CMD:
        return ev_network_actor_handle_tx(ctx, msg);
    default:
        ++ctx->stats.bad_events;
        return EV_ERR_CONTRACT;
    }
}

const ev_network_actor_stats_t *ev_network_actor_stats(const ev_network_actor_ctx_t *ctx)
{
    return (ctx != NULL) ? &ctx->stats : NULL;
}
