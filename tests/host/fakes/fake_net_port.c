#include "fake_net_port.h"

#include <string.h>

static size_t fake_net_pending_count(const fake_net_port_t *fake)
{
    if (fake == NULL) {
        return 0U;
    }
    return (size_t)(fake->write_seq - fake->read_seq);
}

static void fake_net_record_high_watermark(fake_net_port_t *fake)
{
    const size_t pending = fake_net_pending_count(fake);
    if ((fake != NULL) && (pending > fake->high_watermark)) {
        fake->high_watermark = (uint32_t)pending;
    }
}

static ev_result_t fake_net_init_fn(void *ctx)
{
    fake_net_port_t *fake = (fake_net_port_t *)ctx;
    if (fake == NULL) {
        return EV_ERR_INVALID_ARG;
    }
    ++fake->init_calls;
    return EV_OK;
}

static ev_result_t fake_net_start_fn(void *ctx)
{
    fake_net_port_t *fake = (fake_net_port_t *)ctx;
    if (fake == NULL) {
        return EV_ERR_INVALID_ARG;
    }
    ++fake->start_calls;
    return EV_OK;
}

static ev_result_t fake_net_stop_fn(void *ctx)
{
    fake_net_port_t *fake = (fake_net_port_t *)ctx;
    if (fake == NULL) {
        return EV_ERR_INVALID_ARG;
    }
    ++fake->stop_calls;
    return EV_OK;
}

static ev_result_t fake_net_publish_mqtt_fn(void *ctx, const ev_net_mqtt_publish_cmd_t *cmd)
{
    fake_net_port_t *fake = (fake_net_port_t *)ctx;
    if ((fake == NULL) || (cmd == NULL)) {
        return EV_ERR_INVALID_ARG;
    }
    ++fake->publish_mqtt_calls;
    if (fake->next_publish_result == EV_OK) {
        ++fake->publish_mqtt_ok;
    } else {
        ++fake->publish_mqtt_failed;
    }
    return fake->next_publish_result;
}

static ev_result_t fake_net_poll_ingress_fn(void *ctx, ev_net_ingress_event_t *out_event)
{
    fake_net_port_t *fake = (fake_net_port_t *)ctx;
    uint32_t index;

    if ((fake == NULL) || (out_event == NULL)) {
        return EV_ERR_INVALID_ARG;
    }
    ++fake->poll_calls;
    if (fake_net_pending_count(fake) == 0U) {
        return EV_ERR_EMPTY;
    }

    index = fake->read_seq & EV_NET_INGRESS_RING_MASK;
    *out_event = fake->ring[index];
    ++fake->read_seq;
    return EV_OK;
}

static ev_result_t fake_net_get_stats_fn(void *ctx, ev_net_stats_t *out_stats)
{
    fake_net_port_t *fake = (fake_net_port_t *)ctx;

    if ((fake == NULL) || (out_stats == NULL)) {
        return EV_ERR_INVALID_ARG;
    }
    ++fake->get_stats_calls;
    memset(out_stats, 0, sizeof(*out_stats));
    out_stats->write_seq = fake->write_seq;
    out_stats->read_seq = fake->read_seq;
    out_stats->pending_events = (uint32_t)fake_net_pending_count(fake);
    out_stats->dropped_events = fake->dropped_events;
    out_stats->dropped_oversize = fake->dropped_oversize;
    out_stats->dropped_no_payload_slot = fake->dropped_no_payload_slot;
    out_stats->high_watermark = fake->high_watermark;
    out_stats->tx_attempts = fake->publish_mqtt_calls;
    out_stats->tx_ok = fake->publish_mqtt_ok;
    out_stats->tx_failed = fake->publish_mqtt_failed;
    return EV_OK;
}

void fake_net_port_init(fake_net_port_t *fake)
{
    if (fake != NULL) {
        memset(fake, 0, sizeof(*fake));
        fake->next_publish_result = EV_OK;
    }
}

void fake_net_port_bind(ev_net_port_t *out_port, fake_net_port_t *fake)
{
    if (out_port != NULL) {
        memset(out_port, 0, sizeof(*out_port));
        out_port->ctx = fake;
        out_port->init = fake_net_init_fn;
        out_port->start = fake_net_start_fn;
        out_port->stop = fake_net_stop_fn;
        out_port->publish_mqtt = fake_net_publish_mqtt_fn;
        out_port->poll_ingress = fake_net_poll_ingress_fn;
        out_port->get_stats = fake_net_get_stats_fn;
    }
}

ev_result_t fake_net_port_callback_push(fake_net_port_t *fake, const ev_net_ingress_event_t *event)
{
    uint32_t index;

    if ((fake == NULL) || (event == NULL)) {
        return EV_ERR_INVALID_ARG;
    }
    ++fake->callback_push_calls;
    if (fake_net_pending_count(fake) >= EV_NET_INGRESS_RING_CAPACITY) {
        ++fake->dropped_events;
        return EV_ERR_FULL;
    }

    index = fake->write_seq & EV_NET_INGRESS_RING_MASK;
    fake->ring[index] = *event;
    ++fake->write_seq;
    fake_net_record_high_watermark(fake);
    return EV_OK;
}

ev_result_t fake_net_port_callback_push_mqtt(fake_net_port_t *fake,
                                             const char *topic,
                                             size_t topic_len,
                                             const uint8_t *payload,
                                             size_t payload_len)
{
    ev_net_ingress_event_t event;

    if ((fake == NULL) || (topic == NULL) || ((payload_len > 0U) && (payload == NULL))) {
        return EV_ERR_INVALID_ARG;
    }
    if ((topic_len > EV_NET_MAX_TOPIC_BYTES) || (payload_len > EV_NET_MAX_INLINE_PAYLOAD_BYTES)) {
        ++fake->dropped_oversize;
        return EV_ERR_OUT_OF_RANGE;
    }

    memset(&event, 0, sizeof(event));
    event.kind = EV_NET_EVENT_MQTT_MSG_RX;
    event.topic_len = (uint8_t)topic_len;
    event.payload_len = (uint8_t)payload_len;
    if (topic_len > 0U) {
        memcpy(event.topic, topic, topic_len);
    }
    if (payload_len > 0U) {
        memcpy(event.payload, payload, payload_len);
    }
    return fake_net_port_callback_push(fake, &event);
}

size_t fake_net_port_pending(const fake_net_port_t *fake)
{
    return fake_net_pending_count(fake);
}
