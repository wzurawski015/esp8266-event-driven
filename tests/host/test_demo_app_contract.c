#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "ev/demo_app.h"

typedef struct {
    ev_time_mono_us_t now_us;
    ev_time_mono_us_t auto_increment_us;
    uint32_t delay_calls;
} fake_clock_t;

typedef struct {
    uint32_t writes;
    char last_message[192];
} fake_log_t;

static ev_result_t fake_mono_now_us(void *ctx, ev_time_mono_us_t *out_now)
{
    fake_clock_t *clock = (fake_clock_t *)ctx;
    assert(clock != NULL);
    assert(out_now != NULL);
    *out_now = clock->now_us;
    clock->now_us += clock->auto_increment_us;
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
    (void)delay_ms;
    ++clock->delay_calls;
    return EV_OK;
}

static ev_result_t fake_log_write(void *ctx,
                                  ev_log_level_t level,
                                  const char *tag,
                                  const char *message,
                                  size_t message_len)
{
    fake_log_t *log = (fake_log_t *)ctx;
    (void)level;
    (void)tag;
    assert(log != NULL);
    assert(message != NULL);
    assert(message_len < sizeof(log->last_message));
    memcpy(log->last_message, message, message_len);
    log->last_message[message_len] = '\0';
    ++log->writes;
    return EV_OK;
}

static ev_result_t fake_log_flush(void *ctx)
{
    (void)ctx;
    return EV_OK;
}

int main(void)
{
    fake_clock_t clock = {0};
    fake_log_t log = {0};
    ev_clock_port_t clock_port = {
        .ctx = &clock,
        .mono_now_us = fake_mono_now_us,
        .wall_now_us = fake_wall_now_us,
        .delay_ms = fake_delay_ms,
    };
    ev_log_port_t log_port = {
        .ctx = &log,
        .write = fake_log_write,
        .flush = fake_log_flush,
    };
    ev_demo_app_t app;
    ev_demo_app_config_t cfg = {
        .app_tag = "host_demo",
        .board_name = "host",
        .tick_period_ms = 1000U,
        .clock_port = &clock_port,
        .log_port = &log_port,
    };
    const ev_demo_app_stats_t *stats;
    const ev_system_pump_stats_t *pump_stats;

    clock.auto_increment_us = 1000ULL;

    assert(ev_demo_app_init(&app, &cfg) == EV_OK);
    assert(ev_demo_app_publish_boot(&app) == EV_OK);
    assert(ev_demo_app_pending(&app) == 3U);

    assert(ev_demo_app_poll(&app) == EV_OK);
    assert(ev_demo_app_pending(&app) == 0U);
    stats = ev_demo_app_stats(&app);
    assert(stats != NULL);
    assert(stats->boot_completions == 1U);
    assert(stats->ticks_published == 0U);
    assert(stats->diag_ticks_seen == 0U);
    assert(stats->snapshots_published == 1U);
    assert(stats->snapshots_received == 1U);
    assert(stats->publish_errors == 0U);
    assert(stats->pump_errors == 0U);
    assert(stats->max_pending_before_poll >= 1U);
    assert(stats->max_pump_calls_per_poll >= 1U);
    assert(stats->max_turns_per_poll >= 1U);
    assert(stats->max_messages_per_poll >= 1U);
    assert(stats->max_poll_elapsed_ms > 0U);
    assert(app.app_actor.last_snapshot_sequence == 1U);
    assert(app.app_actor.last_diag_ticks_seen == 0U);

    clock.now_us = 1000ULL * 1000ULL;
    assert(ev_demo_app_poll(&app) == EV_OK);
    stats = ev_demo_app_stats(&app);
    assert(stats->ticks_published == 1U);
    assert(stats->diag_ticks_seen == 1U);
    assert(stats->snapshots_published == 2U);
    assert(stats->snapshots_received == 2U);
    assert(app.app_actor.last_snapshot_sequence == 2U);
    assert(app.app_actor.last_diag_ticks_seen == 1U);

    clock.now_us = 3000ULL * 1000ULL;
    assert(ev_demo_app_poll(&app) == EV_OK);
    stats = ev_demo_app_stats(&app);
    assert(stats->ticks_published == 3U);
    assert(stats->diag_ticks_seen == 3U);
    assert(stats->snapshots_published == 4U);
    assert(stats->snapshots_received == 4U);
    assert(app.app_actor.last_snapshot_sequence == 4U);
    assert(app.app_actor.last_diag_ticks_seen == 3U);
    assert(ev_demo_app_pending(&app) == 0U);

    pump_stats = ev_demo_app_system_pump_stats(&app);
    assert(pump_stats != NULL);
    assert(pump_stats->run_calls >= 1U);
    assert(pump_stats->messages_processed >= 10U);
    assert(pump_stats->last_result == EV_OK);
    assert(log.writes >= 6U);
    assert(strlen(log.last_message) > 0U);

    return 0;
}
