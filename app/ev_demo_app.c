#include "ev/demo_app.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ev/compiler.h"
#include "ev/dispose.h"
#include "ev/msg.h"
#include "ev/publish.h"

#define EV_DEMO_APP_DEFAULT_TICK_MS 1000U
#define EV_DEMO_APP_TURN_BUDGET 4U

typedef struct {
    uint32_t sequence;
    uint32_t ticks_seen;
    uint32_t last_tick_ms;
    uint32_t boot_completions;
} ev_demo_snapshot_t;

EV_STATIC_ASSERT(sizeof(ev_demo_snapshot_t) == EV_DEMO_APP_SNAPSHOT_BYTES, "demo snapshot ABI mismatch");

static void ev_demo_app_logf(ev_demo_app_t *app, ev_log_level_t level, const char *fmt, ...)
{
    char buffer[192];
    va_list ap;
    int len;

    if ((app == NULL) || (app->log_port == NULL) || (app->log_port->write == NULL) || (app->app_tag == NULL) ||
        (fmt == NULL)) {
        return;
    }

    va_start(ap, fmt);
    len = vsnprintf(buffer, sizeof(buffer), fmt, ap);
    va_end(ap);

    if (len < 0) {
        return;
    }
    if ((size_t)len >= sizeof(buffer)) {
        len = (int)(sizeof(buffer) - 1U);
        buffer[len] = '\0';
    }

    (void)app->log_port->write(app->log_port->ctx, level, app->app_tag, buffer, (size_t)len);
}

static ev_result_t ev_demo_app_now_ms(ev_demo_app_t *app, uint32_t *out_now_ms)
{
    ev_time_mono_us_t now_us;
    ev_result_t rc;

    if ((app == NULL) || (out_now_ms == NULL) || (app->clock_port == NULL) || (app->clock_port->mono_now_us == NULL)) {
        return EV_ERR_INVALID_ARG;
    }

    rc = app->clock_port->mono_now_us(app->clock_port->ctx, &now_us);
    if (rc != EV_OK) {
        return rc;
    }

    *out_now_ms = (uint32_t)(now_us / 1000ULL);
    return EV_OK;
}

static ev_result_t ev_demo_app_publish_owned(ev_demo_app_t *app, ev_msg_t *msg)
{
    ev_result_t rc;
    ev_result_t dispose_rc;

    if ((app == NULL) || (msg == NULL)) {
        return EV_ERR_INVALID_ARG;
    }

    rc = ev_publish(msg, ev_actor_registry_delivery, &app->registry, NULL);
    if (rc != EV_OK) {
        ++app->stats.publish_errors;
    }

    dispose_rc = ev_msg_dispose(msg);
    if ((rc == EV_OK) && (dispose_rc != EV_OK)) {
        return dispose_rc;
    }

    return rc;
}

static ev_result_t ev_demo_app_publish_snapshot(ev_demo_diag_actor_state_t *state)
{
    ev_demo_app_t *app;
    ev_lease_handle_t handle = {0};
    ev_demo_snapshot_t *snapshot = NULL;
    ev_msg_t msg;
    void *data = NULL;
    ev_result_t rc;

    if ((state == NULL) || (state->app == NULL)) {
        return EV_ERR_INVALID_ARG;
    }

    app = state->app;
    rc = ev_lease_pool_acquire(&app->lease_pool, sizeof(*snapshot), &handle, &data);
    if (rc != EV_OK) {
        ++app->stats.publish_errors;
        ev_demo_app_logf(app, EV_LOG_ERROR, "snapshot acquire failed rc=%d", (int)rc);
        return rc;
    }

    snapshot = (ev_demo_snapshot_t *)data;
    snapshot->sequence = state->snapshots_sent + 1U;
    snapshot->ticks_seen = state->ticks_seen;
    snapshot->last_tick_ms = state->last_tick_ms;
    snapshot->boot_completions = app->stats.boot_completions;

    rc = ev_msg_init_publish(&msg, EV_DIAG_SNAPSHOT_RSP, ACT_DIAG);
    if (rc == EV_OK) {
        rc = ev_lease_pool_attach_msg(&msg, &handle);
    }
    if (rc == EV_OK) {
        rc = ev_demo_app_publish_owned(app, &msg);
    } else {
        ++app->stats.publish_errors;
        (void)ev_msg_dispose(&msg);
    }

    (void)ev_lease_pool_release(&handle);
    if (rc != EV_OK) {
        return rc;
    }

    ++state->snapshots_sent;
    ++app->stats.snapshots_published;
    return EV_OK;
}

static ev_result_t ev_demo_app_publish_diag_request(ev_demo_app_actor_state_t *state)
{
    ev_demo_app_t *app;
    ev_msg_t msg;
    ev_result_t rc;

    if ((state == NULL) || (state->app == NULL)) {
        return EV_ERR_INVALID_ARG;
    }

    app = state->app;
    rc = ev_msg_init_publish(&msg, EV_DIAG_SNAPSHOT_REQ, ACT_APP);
    if (rc != EV_OK) {
        return rc;
    }

    return ev_demo_app_publish_owned(app, &msg);
}

static ev_result_t ev_demo_app_publish_tick(ev_demo_app_t *app)
{
    ev_msg_t msg;
    ev_result_t rc;

    if (app == NULL) {
        return EV_ERR_INVALID_ARG;
    }

    rc = ev_msg_init_publish(&msg, EV_TICK_1S, ACT_BOOT);
    if (rc != EV_OK) {
        return rc;
    }

    rc = ev_demo_app_publish_owned(app, &msg);
    if (rc == EV_OK) {
        ++app->stats.ticks_published;
    }

    return rc;
}

static ev_result_t ev_demo_app_actor_handler(void *actor_context, const ev_msg_t *msg)
{
    const ev_demo_snapshot_t *snapshot;
    ev_demo_app_actor_state_t *state = (ev_demo_app_actor_state_t *)actor_context;
    ev_demo_app_t *app;

    if ((state == NULL) || (state->app == NULL) || (msg == NULL)) {
        return EV_ERR_INVALID_ARG;
    }

    app = state->app;

    switch (msg->event_id) {
    case EV_BOOT_COMPLETED:
        ++app->stats.boot_completions;
        ev_demo_app_logf(app, EV_LOG_INFO, "app actor: boot complete -> requesting diag snapshot");
        return ev_demo_app_publish_diag_request(state);

    case EV_DIAG_SNAPSHOT_RSP:
        snapshot = (const ev_demo_snapshot_t *)ev_msg_payload_data(msg);
        if ((snapshot == NULL) || (ev_msg_payload_size(msg) != sizeof(*snapshot))) {
            return EV_ERR_CONTRACT;
        }
        state->last_snapshot_sequence = snapshot->sequence;
        state->last_diag_ticks_seen = snapshot->ticks_seen;
        ++app->stats.snapshots_received;
        ev_demo_app_logf(app,
                         EV_LOG_INFO,
                         "app actor: snapshot seq=%u diag_ticks=%u last_tick_ms=%u",
                         (unsigned)snapshot->sequence,
                         (unsigned)snapshot->ticks_seen,
                         (unsigned)snapshot->last_tick_ms);
        return EV_OK;

    default:
        return EV_ERR_CONTRACT;
    }
}

static ev_result_t ev_demo_diag_actor_handler(void *actor_context, const ev_msg_t *msg)
{
    ev_demo_diag_actor_state_t *state = (ev_demo_diag_actor_state_t *)actor_context;
    ev_demo_app_t *app;
    ev_result_t rc;
    uint32_t now_ms = 0U;

    if ((state == NULL) || (state->app == NULL) || (msg == NULL)) {
        return EV_ERR_INVALID_ARG;
    }

    app = state->app;

    switch (msg->event_id) {
    case EV_BOOT_STARTED:
        ev_demo_app_logf(app, EV_LOG_INFO, "diag actor: observed boot start on %s", app->board_name);
        return EV_OK;

    case EV_BOOT_COMPLETED:
        ev_demo_app_logf(app, EV_LOG_INFO, "diag actor: observed boot completion");
        return EV_OK;

    case EV_TICK_1S:
        rc = ev_demo_app_now_ms(app, &now_ms);
        if (rc != EV_OK) {
            return rc;
        }
        state->last_tick_ms = now_ms;
        ++state->ticks_seen;
        ++app->stats.diag_ticks_seen;
        ev_demo_app_logf(app,
                         EV_LOG_INFO,
                         "diag actor: tick=%u mono_now_ms=%u",
                         (unsigned)state->ticks_seen,
                         (unsigned)state->last_tick_ms);
        return ev_demo_app_publish_snapshot(state);

    case EV_DIAG_SNAPSHOT_REQ:
        return ev_demo_app_publish_snapshot(state);

    default:
        return EV_ERR_CONTRACT;
    }
}

static bool ev_demo_app_config_is_valid(const ev_demo_app_config_t *cfg)
{
    return (cfg != NULL) && (cfg->app_tag != NULL) && (cfg->board_name != NULL) && (cfg->clock_port != NULL) &&
           (cfg->clock_port->mono_now_us != NULL) && (cfg->log_port != NULL) && (cfg->log_port->write != NULL);
}

static ev_result_t ev_demo_app_drain_until_idle(ev_demo_app_t *app)
{
    ev_system_pump_report_t report;
    ev_result_t rc;

    if (app == NULL) {
        return EV_ERR_INVALID_ARG;
    }

    while (ev_system_pump_pending(&app->system_pump) > 0U) {
        rc = ev_system_pump_run(&app->system_pump, EV_DEMO_APP_TURN_BUDGET, &report);
        if (rc == EV_OK) {
            continue;
        }
        if (rc == EV_ERR_EMPTY) {
            return EV_OK;
        }

        ++app->stats.pump_errors;
        ev_demo_app_logf(app,
                         EV_LOG_ERROR,
                         "system pump rc=%d turns=%u messages=%u pending_after=%u",
                         (int)rc,
                         (unsigned)report.turns_processed,
                         (unsigned)report.messages_processed,
                         (unsigned)report.pending_after);
        return rc;
    }

    return EV_OK;
}

ev_result_t ev_demo_app_init(ev_demo_app_t *app, const ev_demo_app_config_t *cfg)
{
    ev_result_t rc;
    uint32_t now_ms;

    if ((app == NULL) || !ev_demo_app_config_is_valid(cfg)) {
        return EV_ERR_INVALID_ARG;
    }

    memset(app, 0, sizeof(*app));
    app->clock_port = cfg->clock_port;
    app->log_port = cfg->log_port;
    app->app_tag = cfg->app_tag;
    app->board_name = cfg->board_name;
    app->tick_period_ms = (cfg->tick_period_ms == 0U) ? EV_DEMO_APP_DEFAULT_TICK_MS : cfg->tick_period_ms;
    app->app_actor.app = app;
    app->diag_actor.app = app;

    rc = ev_demo_app_now_ms(app, &now_ms);
    if (rc != EV_OK) {
        return rc;
    }
    app->next_tick_ms = now_ms + app->tick_period_ms;

    rc = ev_mailbox_init(
        &app->app_mailbox,
        EV_MAILBOX_FIFO_8,
        app->app_storage,
        EV_ARRAY_LEN(app->app_storage));
    if (rc != EV_OK) {
        return rc;
    }
    rc = ev_mailbox_init(
        &app->diag_mailbox,
        EV_MAILBOX_FIFO_8,
        app->diag_storage,
        EV_ARRAY_LEN(app->diag_storage));
    if (rc != EV_OK) {
        return rc;
    }
    rc = ev_actor_runtime_init(&app->app_runtime, ACT_APP, &app->app_mailbox, ev_demo_app_actor_handler, &app->app_actor);
    if (rc != EV_OK) {
        return rc;
    }
    rc = ev_actor_runtime_init(
        &app->diag_runtime,
        ACT_DIAG,
        &app->diag_mailbox,
        ev_demo_diag_actor_handler,
        &app->diag_actor);
    if (rc != EV_OK) {
        return rc;
    }
    rc = ev_actor_registry_init(&app->registry);
    if (rc != EV_OK) {
        return rc;
    }
    rc = ev_actor_registry_bind(&app->registry, &app->app_runtime);
    if (rc != EV_OK) {
        return rc;
    }
    rc = ev_actor_registry_bind(&app->registry, &app->diag_runtime);
    if (rc != EV_OK) {
        return rc;
    }
    rc = ev_domain_pump_init(&app->fast_domain, &app->registry, EV_DOMAIN_FAST_LOOP);
    if (rc != EV_OK) {
        return rc;
    }
    rc = ev_domain_pump_init(&app->slow_domain, &app->registry, EV_DOMAIN_SLOW_IO);
    if (rc != EV_OK) {
        return rc;
    }
    rc = ev_system_pump_init(&app->system_pump);
    if (rc != EV_OK) {
        return rc;
    }
    rc = ev_system_pump_bind(&app->system_pump, &app->fast_domain);
    if (rc != EV_OK) {
        return rc;
    }
    rc = ev_system_pump_bind(&app->system_pump, &app->slow_domain);
    if (rc != EV_OK) {
        return rc;
    }
    rc = ev_lease_pool_init(
        &app->lease_pool,
        app->lease_slots,
        app->lease_storage,
        EV_DEMO_APP_LEASE_SLOTS,
        EV_DEMO_APP_SNAPSHOT_BYTES);
    if (rc != EV_OK) {
        return rc;
    }

    ev_demo_app_logf(app,
                     EV_LOG_INFO,
                     "demo runtime ready board=%s tick_period_ms=%u",
                     app->board_name,
                     (unsigned)app->tick_period_ms);
    return EV_OK;
}

ev_result_t ev_demo_app_publish_boot(ev_demo_app_t *app)
{
    ev_msg_t msg;
    ev_result_t rc;

    if (app == NULL) {
        return EV_ERR_INVALID_ARG;
    }
    if (app->boot_published) {
        return EV_ERR_STATE;
    }

    rc = ev_msg_init_publish(&msg, EV_BOOT_STARTED, ACT_BOOT);
    if (rc != EV_OK) {
        return rc;
    }
    rc = ev_demo_app_publish_owned(app, &msg);
    if (rc != EV_OK) {
        return rc;
    }

    rc = ev_msg_init_publish(&msg, EV_BOOT_COMPLETED, ACT_BOOT);
    if (rc != EV_OK) {
        return rc;
    }
    rc = ev_demo_app_publish_owned(app, &msg);
    if (rc != EV_OK) {
        return rc;
    }

    app->boot_published = true;
    return EV_OK;
}

ev_result_t ev_demo_app_poll(ev_demo_app_t *app)
{
    ev_result_t rc;
    uint32_t now_ms;

    if (app == NULL) {
        return EV_ERR_INVALID_ARG;
    }
    if (!app->boot_published) {
        return EV_ERR_STATE;
    }

    rc = ev_demo_app_drain_until_idle(app);
    if (rc != EV_OK) {
        return rc;
    }

    rc = ev_demo_app_now_ms(app, &now_ms);
    if (rc != EV_OK) {
        return rc;
    }

    while ((int32_t)(now_ms - app->next_tick_ms) >= 0) {
        rc = ev_demo_app_publish_tick(app);
        if (rc != EV_OK) {
            return rc;
        }
        app->next_tick_ms += app->tick_period_ms;
    }

    return ev_demo_app_drain_until_idle(app);
}

size_t ev_demo_app_pending(const ev_demo_app_t *app)
{
    return (app != NULL) ? ev_system_pump_pending(&app->system_pump) : 0U;
}

const ev_demo_app_stats_t *ev_demo_app_stats(const ev_demo_app_t *app)
{
    return (app != NULL) ? &app->stats : NULL;
}

const ev_system_pump_stats_t *ev_demo_app_system_pump_stats(const ev_demo_app_t *app)
{
    return (app != NULL) ? ev_system_pump_stats(&app->system_pump) : NULL;
}
