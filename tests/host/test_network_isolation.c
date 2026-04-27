#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ev/demo_app.h"
#include "ev/dispose.h"
#include "ev/msg.h"
#include "ev/network_actor.h"
#include "ev/publish.h"
#include "fakes/fake_irq_port.h"
#include "fakes/fake_log_port.h"
#include "fakes/fake_net_port.h"

#define TEST_DRAIN_LIMIT 24U

typedef struct fake_clock {
    ev_time_mono_us_t now_us;
} fake_clock_t;

static ev_result_t fake_mono_now_us(void *ctx, ev_time_mono_us_t *out_now)
{
    fake_clock_t *clock = (fake_clock_t *)ctx;

    assert(clock != NULL);
    assert(out_now != NULL);
    *out_now = clock->now_us;
    return EV_OK;
}

static ev_result_t fake_wall_now_us(void *ctx, ev_time_wall_us_t *out_now)
{
    (void)ctx;
    (void)out_now;
    return EV_ERR_UNSUPPORTED;
}

static ev_result_t fake_delay_ms(void *ctx, uint32_t delay_ms)
{
    fake_clock_t *clock = (fake_clock_t *)ctx;

    assert(clock != NULL);
    clock->now_us += ((ev_time_mono_us_t)delay_ms * 1000ULL);
    return EV_OK;
}

static void bind_clock(ev_clock_port_t *out_port, fake_clock_t *clock)
{
    memset(out_port, 0, sizeof(*out_port));
    out_port->ctx = clock;
    out_port->mono_now_us = fake_mono_now_us;
    out_port->wall_now_us = fake_wall_now_us;
    out_port->delay_ms = fake_delay_ms;
}

static ev_demo_app_config_t make_network_cfg(ev_clock_port_t *clock_port,
                                             ev_log_port_t *log_port,
                                             ev_net_port_t *net_port,
                                             const ev_demo_app_board_profile_t *profile)
{
    ev_demo_app_config_t cfg;

    memset(&cfg, 0, sizeof(cfg));
    cfg.app_tag = "test";
    cfg.board_name = "network-test";
    cfg.tick_period_ms = 1000U;
    cfg.clock_port = clock_port;
    cfg.log_port = log_port;
    cfg.net_port = net_port;
    cfg.board_profile = profile;
    return cfg;
}

static void drain_app(ev_demo_app_t *app)
{
    unsigned i;

    for (i = 0U; i < TEST_DRAIN_LIMIT; ++i) {
        ev_result_t rc = ev_demo_app_poll(app);
        if ((rc != EV_OK) && (rc != EV_ERR_PARTIAL)) {
            fprintf(stderr, "network drain rc=%d iteration=%u pending=%zu\n", (int)rc, i, ev_demo_app_pending(app));
        }
        assert((rc == EV_OK) || (rc == EV_ERR_PARTIAL));
        if ((rc == EV_OK) && (ev_demo_app_pending(app) == 0U)) {
            return;
        }
    }

    assert(!"network app did not drain within bounded test limit");
}

static void fill_event(ev_net_ingress_event_t *event, ev_net_event_kind_t kind)
{
    memset(event, 0, sizeof(*event));
    event->kind = kind;
}

static void test_fake_net_ring_backpressure_and_oversize(void)
{
    fake_net_port_t fake;
    ev_net_port_t port;
    ev_net_ingress_event_t event;
    ev_net_ingress_event_t out_event;
    size_t i;
    const uint8_t payload[EV_NET_MAX_INLINE_PAYLOAD_BYTES + 1U] = {0};

    fake_net_port_init(&fake);
    fake_net_port_bind(&port, &fake);
    fill_event(&event, EV_NET_EVENT_WIFI_UP);

    for (i = 0U; i < EV_NET_INGRESS_RING_CAPACITY; ++i) {
        assert(fake_net_port_callback_push(&fake, &event) == EV_OK);
    }
    assert(fake_net_port_pending(&fake) == EV_NET_INGRESS_RING_CAPACITY);
    assert(fake.high_watermark == EV_NET_INGRESS_RING_CAPACITY);
    assert(fake.wifi_up_events == EV_NET_INGRESS_RING_CAPACITY);
    assert(fake_net_port_callback_push(&fake, &event) == EV_ERR_FULL);
    assert(fake.dropped_events == 1U);

    {
        ev_net_stats_t stats;
        assert(port.get_stats(port.ctx, &stats) == EV_OK);
        assert(stats.wifi_up_events == EV_NET_INGRESS_RING_CAPACITY);
        assert(stats.dropped_events == 1U);
        assert(stats.high_watermark == EV_NET_INGRESS_RING_CAPACITY);
    }

    assert(port.poll_ingress(port.ctx, &out_event) == EV_OK);
    assert(fake_net_port_pending(&fake) == (EV_NET_INGRESS_RING_CAPACITY - 1U));
    assert(fake_net_port_callback_push_mqtt(&fake,
                                            "oversize!",
                                            EV_NET_MAX_TOPIC_BYTES + 1U,
                                            payload,
                                            sizeof(payload)) == EV_ERR_OUT_OF_RANGE);
    assert(fake.dropped_oversize == 1U);
}


static void test_fake_net_reconnect_storm_suppression(void)
{
    fake_net_port_t fake;
    ev_net_port_t port;
    ev_net_stats_t stats;
    ev_net_ingress_event_t event;
    unsigned i;

    fake_net_port_init(&fake);
    fake_net_port_bind(&port, &fake);

    assert(fake_net_port_callback_wifi_up(&fake) == EV_OK);
    assert(fake_net_port_callback_wifi_up(&fake) == EV_ERR_STATE);
    assert(fake_net_port_callback_wifi_down(&fake) == EV_OK);

    for (i = 0U; i < 8U; ++i) {
        assert(fake_net_port_callback_wifi_down(&fake) == EV_ERR_STATE);
    }

    assert(fake_net_port_pending(&fake) == 2U);
    assert(port.get_stats(port.ctx, &stats) == EV_OK);
    assert(stats.wifi_up_events == 1U);
    assert(stats.wifi_down_events == 1U);
    assert(stats.reconnect_attempts == 1U);
    assert(stats.reconnect_suppressed == 8U);
    assert(stats.duplicate_wifi_up_suppressed == 1U);
    assert(stats.duplicate_wifi_down_suppressed == 8U);
    assert(stats.callback_state_updates == 2U);

    assert(port.poll_ingress(port.ctx, &event) == EV_OK);
    assert(event.kind == EV_NET_EVENT_WIFI_UP);
    assert(port.poll_ingress(port.ctx, &event) == EV_OK);
    assert(event.kind == EV_NET_EVENT_WIFI_DOWN);
    assert(port.poll_ingress(port.ctx, &event) == EV_ERR_EMPTY);

    assert(fake_net_port_callback_wifi_up(&fake) == EV_OK);
    assert(port.poll_ingress(port.ctx, &event) == EV_OK);
    assert(event.kind == EV_NET_EVENT_WIFI_UP);
}

static void test_network_actor_state_machine_and_tx_policy(void)
{
    fake_net_port_t fake;
    ev_net_port_t port;
    ev_network_actor_ctx_t actor;
    ev_msg_t msg = {0};
    ev_net_mqtt_publish_cmd_t tx_cmd;

    fake_net_port_init(&fake);
    fake_net_port_bind(&port, &fake);
    assert(ev_network_actor_init(&actor, &port) == EV_OK);

    assert(ev_msg_init_publish(&msg, EV_NET_TX_CMD, ACT_APP) == EV_OK);
    memset(&tx_cmd, 0, sizeof(tx_cmd));
    tx_cmd.topic_len = 1U;
    tx_cmd.payload_len = 1U;
    tx_cmd.topic[0] = 'a';
    tx_cmd.payload[0] = 1U;
    assert(ev_msg_set_inline_payload(&msg, &tx_cmd, sizeof(tx_cmd)) == EV_OK);
    assert(ev_network_actor_handle(&actor, &msg) == EV_OK);
    assert(fake.publish_mqtt_calls == 0U);
    assert(actor.stats.tx_rejected_not_connected == 1U);
    assert(ev_msg_dispose(&msg) == EV_OK);

    assert(ev_msg_init_publish(&msg, EV_NET_WIFI_UP, ACT_RUNTIME) == EV_OK);
    assert(ev_network_actor_handle(&actor, &msg) == EV_OK);
    assert(actor.state == EV_NETWORK_STATE_WIFI_UP);
    assert(ev_msg_dispose(&msg) == EV_OK);

    assert(ev_msg_init_publish(&msg, EV_NET_MQTT_UP, ACT_RUNTIME) == EV_OK);
    assert(ev_network_actor_handle(&actor, &msg) == EV_OK);
    assert(actor.state == EV_NETWORK_STATE_MQTT_CONNECTED);
    assert(ev_msg_dispose(&msg) == EV_OK);

    assert(ev_msg_init_publish(&msg, EV_NET_TX_CMD, ACT_APP) == EV_OK);
    assert(ev_msg_set_inline_payload(&msg, &tx_cmd, sizeof(tx_cmd)) == EV_OK);
    assert(ev_network_actor_handle(&actor, &msg) == EV_OK);
    assert(fake.publish_mqtt_calls == 1U);
    assert(actor.stats.tx_ok == 1U);
    assert(ev_msg_dispose(&msg) == EV_OK);

    {
        ev_net_ingress_event_t rx_event;
        memset(&rx_event, 0, sizeof(rx_event));
        rx_event.kind = EV_NET_EVENT_MQTT_MSG_RX;
        rx_event.topic_len = 5U;
        memcpy(rx_event.topic, "cmd/x", 5U);
        rx_event.payload_len = 1U;
        rx_event.payload[0] = 1U;
        assert(ev_msg_init_publish(&msg, EV_NET_MQTT_MSG_RX, ACT_RUNTIME) == EV_OK);
        assert(ev_msg_set_inline_payload(&msg, &rx_event, sizeof(rx_event)) == EV_OK);
        assert(ev_network_actor_handle(&actor, &msg) == EV_OK);
        assert(actor.stats.mqtt_rx_events == 1U);
        assert(actor.stats.mqtt_rx_ignored_foundation == 1U);
        assert(fake.publish_mqtt_calls == 1U);
        assert(ev_msg_dispose(&msg) == EV_OK);
    }

    fake.next_publish_result = EV_ERR_UNSUPPORTED;
    assert(ev_msg_init_publish(&msg, EV_NET_TX_CMD, ACT_APP) == EV_OK);
    assert(ev_msg_set_inline_payload(&msg, &tx_cmd, sizeof(tx_cmd)) == EV_OK);
    assert(ev_network_actor_handle(&actor, &msg) == EV_ERR_UNSUPPORTED);
    assert(actor.stats.tx_failed == 1U);
    assert(fake.tx_rejected_state == 1U);
    assert(ev_msg_dispose(&msg) == EV_OK);
}

static void test_demo_app_network_ingress_and_irq_budget(void)
{
    static const ev_demo_app_board_profile_t profile = {
        .capabilities_mask = EV_DEMO_APP_BOARD_CAP_NET | EV_DEMO_APP_BOARD_CAP_GPIO_IRQ,
        .hardware_present_mask = 0U,
        .supervisor_required_mask = 0U,
        .supervisor_optional_mask = 0U,
        .i2c_port_num = EV_I2C_PORT_NUM_0,
        .rtc_sqw_line_id = 0U,
        .mcp23008_addr_7bit = 0U,
        .rtc_addr_7bit = 0U,
        .oled_addr_7bit = 0U,
        .oled_controller = EV_OLED_CONTROLLER_SSD1306,
        .watchdog_timeout_ms = 0U,
    };
    fake_clock_t clock = {0};
    ev_clock_port_t clock_port;
    fake_log_port_t fake_log;
    ev_log_port_t log_port;
    fake_net_port_t fake_net;
    ev_net_port_t net_port;
    fake_irq_port_t fake_irq;
    ev_irq_port_t irq_port;
    ev_demo_app_config_t cfg;
    ev_demo_app_t app;
    ev_net_ingress_event_t net_event;
    ev_irq_sample_t irq_sample = {0};
    const ev_demo_app_stats_t *stats;
    const ev_network_actor_stats_t *net_stats;
    size_t i;

    bind_clock(&clock_port, &clock);
    fake_log_port_init(&fake_log);
    fake_log_port_bind(&log_port, &fake_log);
    fake_net_port_init(&fake_net);
    fake_net_port_bind(&net_port, &fake_net);
    fake_irq_port_init(&fake_irq);
    fake_irq_port_bind(&irq_port, &fake_irq);

    cfg = make_network_cfg(&clock_port, &log_port, &net_port, &profile);
    cfg.irq_port = &irq_port;

    assert(ev_demo_app_init(&app, &cfg) == EV_OK);
    assert(ev_demo_app_publish_boot(&app) == EV_OK);
    drain_app(&app);

    fill_event(&net_event, EV_NET_EVENT_WIFI_UP);
    for (i = 0U; i < EV_NET_INGRESS_RING_CAPACITY; ++i) {
        assert(fake_net_port_callback_push(&fake_net, &net_event) == EV_OK);
    }
    assert(fake_net_port_callback_push(&fake_net, &net_event) == EV_ERR_FULL);

    irq_sample.line_id = 0U;
    irq_sample.edge = EV_IRQ_EDGE_RISING;
    irq_sample.level = 1U;
    irq_sample.timestamp_us = 42U;
    assert(fake_irq_port_push(&fake_irq, &irq_sample) == EV_OK);

    (void)ev_demo_app_poll(&app);
    stats = ev_demo_app_stats(&app);
    net_stats = ev_demo_app_network_stats(&app);
    assert(stats != NULL);
    assert(net_stats != NULL);
    assert(stats->irq_samples_drained > 0U);
    assert(stats->net_ingress_drained > 0U);
    assert(stats->net_events_dropped_observed == 1U);
    assert(stats->net_ring_high_watermark_observed == EV_NET_INGRESS_RING_CAPACITY);
    assert(stats->max_net_samples_per_poll <= 16U);
    drain_app(&app);
    assert(net_stats->wifi_up_events > 0U);
}

static void test_network_capability_required_for_port(void)
{
    static const ev_demo_app_board_profile_t profile = {
        .capabilities_mask = EV_DEMO_APP_BOARD_CAP_NET,
        .hardware_present_mask = 0U,
        .supervisor_required_mask = 0U,
        .supervisor_optional_mask = 0U,
        .i2c_port_num = EV_I2C_PORT_NUM_0,
        .rtc_sqw_line_id = 0U,
        .mcp23008_addr_7bit = 0U,
        .rtc_addr_7bit = 0U,
        .oled_addr_7bit = 0U,
        .oled_controller = EV_OLED_CONTROLLER_SSD1306,
        .watchdog_timeout_ms = 0U,
    };
    fake_clock_t clock = {0};
    ev_clock_port_t clock_port;
    fake_log_port_t fake_log;
    ev_log_port_t log_port;
    ev_demo_app_config_t cfg;
    ev_demo_app_t app;

    bind_clock(&clock_port, &clock);
    fake_log_port_init(&fake_log);
    fake_log_port_bind(&log_port, &fake_log);
    cfg = make_network_cfg(&clock_port, &log_port, NULL, &profile);
    assert(ev_demo_app_init(&app, &cfg) == EV_ERR_INVALID_ARG);
}

int main(void)
{
    test_fake_net_ring_backpressure_and_oversize();
    test_fake_net_reconnect_storm_suppression();
    test_network_actor_state_machine_and_tx_policy();
    test_demo_app_network_ingress_and_irq_budget();
    test_network_capability_required_for_port();
    puts("network isolation tests passed");
    return 0;
}
