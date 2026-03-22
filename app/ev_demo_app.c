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
#define EV_DEMO_APP_FAST_TICK_MS 100U
#define EV_DEMO_APP_TURN_BUDGET 4U
#define EV_DEMO_APP_RTC_SQW_LINE_ID 0U
#define EV_DEMO_APP_BUTTON_TOGGLE_SCREENSAVER 0U
#define EV_DEMO_APP_LED_SCREENSAVER_PAUSED 0x01U
#define EV_DEMO_APP_OLED_TITLE_TEXT "ATNEL AIR"
#define EV_DEMO_APP_OLED_TITLE_PAGES 3U
#define EV_DEMO_APP_OLED_TITLE_PAGE_OFFSET 0U
#define EV_DEMO_APP_OLED_TIME_PAGE_OFFSET 1U
#define EV_DEMO_APP_OLED_TEMP_PAGE_OFFSET 2U
#define EV_DEMO_APP_OLED_TEXT_CELL_ADVANCE 6U
#define EV_DEMO_APP_OLED_BLOCK_WIDTH_PX ((uint8_t)((sizeof(EV_DEMO_APP_OLED_TITLE_TEXT) - 1U) * EV_DEMO_APP_OLED_TEXT_CELL_ADVANCE))
#define EV_DEMO_APP_OLED_MAX_COLUMN_OFFSET ((uint8_t)(EV_OLED_WIDTH - EV_DEMO_APP_OLED_BLOCK_WIDTH_PX))
#define EV_DEMO_APP_OLED_MAX_PAGE_OFFSET ((uint8_t)(EV_OLED_PAGE_COUNT - EV_DEMO_APP_OLED_TITLE_PAGES))

EV_STATIC_ASSERT(EV_DEMO_APP_OLED_BLOCK_WIDTH_PX <= EV_OLED_WIDTH, "OLED block width must fit the panel");
EV_STATIC_ASSERT(EV_DEMO_APP_OLED_TITLE_PAGES <= EV_OLED_PAGE_COUNT, "OLED block height must fit the panel");

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

static ev_result_t ev_demo_app_publish_system_event(ev_demo_app_t *app,
                                                 ev_event_id_t event_id,
                                                 const void *payload,
                                                 size_t payload_size)
{
    ev_msg_t msg;
    ev_result_t rc;

    if ((app == NULL) || ((payload == NULL) && (payload_size != 0U))) {
        return EV_ERR_INVALID_ARG;
    }

    rc = ev_msg_init_publish(&msg, event_id, ACT_BOOT);
    if (rc != EV_OK) {
        return rc;
    }

    if (payload_size > 0U) {
        rc = ev_msg_set_inline_payload(&msg, payload, payload_size);
        if (rc != EV_OK) {
            (void)ev_msg_dispose(&msg);
            return rc;
        }
    }

    return ev_demo_app_publish_owned(app, &msg);
}

static ev_result_t ev_demo_app_publish_tick(ev_demo_app_t *app)
{
    ev_result_t rc;

    if (app == NULL) {
        return EV_ERR_INVALID_ARG;
    }

    rc = ev_demo_app_publish_system_event(app, EV_TICK_1S, NULL, 0U);
    if (rc == EV_OK) {
        ++app->stats.ticks_published;
    }

    return rc;
}

static ev_result_t ev_demo_app_publish_tick_100ms(ev_demo_app_t *app)
{
    return ev_demo_app_publish_system_event(app, EV_TICK_100MS, NULL, 0U);
}

static ev_result_t ev_demo_app_publish_panel_led_command(ev_demo_app_t *app, uint8_t value_mask, uint8_t valid_mask)
{
#ifndef EV_HOST_BUILD
    ev_msg_t msg = {0};
    ev_panel_led_set_cmd_t cmd = {0};
    ev_result_t rc;

    if (app == NULL) {
        return EV_ERR_INVALID_ARG;
    }

    cmd.value_mask = (uint8_t)(value_mask & EV_MCP23008_LED_MASK);
    cmd.valid_mask = (uint8_t)(valid_mask & EV_MCP23008_LED_MASK);

    rc = ev_msg_init_publish(&msg, EV_PANEL_LED_SET_CMD, ACT_APP);
    if (rc == EV_OK) {
        rc = ev_msg_set_inline_payload(&msg, &cmd, sizeof(cmd));
    }
    if (rc != EV_OK) {
        (void)ev_msg_dispose(&msg);
        return rc;
    }

    return ev_demo_app_publish_owned(app, &msg);
#else
    (void)app;
    (void)value_mask;
    (void)valid_mask;
    return EV_OK;
#endif
}

#ifndef EV_HOST_BUILD
static ev_result_t ev_demo_app_publish_irq_sample(ev_demo_app_t *app, const ev_irq_sample_t *sample)
{
    if ((app == NULL) || (sample == NULL)) {
        return EV_ERR_INVALID_ARG;
    }

    return ev_demo_app_publish_system_event(app, EV_GPIO_IRQ, sample, sizeof(*sample));
}

static ev_result_t ev_demo_app_publish_oled_text(ev_demo_app_t *app, uint8_t page, uint8_t column, const char *text)
{
    ev_msg_t text_msg = {0};
    ev_oled_display_text_cmd_t cmd = {0};
    ev_result_t rc;

    if ((app == NULL) || (text == NULL)) {
        return EV_ERR_INVALID_ARG;
    }
    if ((page >= EV_OLED_PAGE_COUNT) || (column >= EV_OLED_WIDTH)) {
        return EV_ERR_OUT_OF_RANGE;
    }

    cmd.page = page;
    cmd.column = column;
    (void)snprintf(cmd.text, sizeof(cmd.text), "%s", text);

    rc = ev_msg_init_publish(&text_msg, EV_OLED_DISPLAY_TEXT_CMD, ACT_APP);
    if (rc != EV_OK) {
        return rc;
    }

    rc = ev_msg_set_inline_payload(&text_msg, &cmd, sizeof(cmd));
    if (rc != EV_OK) {
        (void)ev_msg_dispose(&text_msg);
        return rc;
    }

    return ev_demo_app_publish_owned(app, &text_msg);
}

static void ev_demo_app_format_time_text(const ev_demo_app_actor_state_t *state, char *out_text, size_t out_text_size)
{
    if ((state == NULL) || (out_text == NULL) || (out_text_size == 0U)) {
        return;
    }

    if (!state->time_valid) {
        (void)snprintf(out_text, out_text_size, "--:--:--");
        return;
    }

    (void)snprintf(out_text,
                   out_text_size,
                   "%02u:%02u:%02u",
                   (unsigned)state->last_time.hours,
                   (unsigned)state->last_time.minutes,
                   (unsigned)state->last_time.seconds);
}

static void ev_demo_app_format_temp_text(const ev_demo_app_actor_state_t *state, char *out_text, size_t out_text_size)
{
    int32_t centi_celsius;
    uint32_t abs_centi_celsius;
    const char *sign;

    if ((state == NULL) || (out_text == NULL) || (out_text_size == 0U)) {
        return;
    }

    if (!state->temp_valid) {
        (void)snprintf(out_text, out_text_size, "--.-- C");
        return;
    }

    centi_celsius = (int32_t)state->last_temp.centi_celsius;
    abs_centi_celsius = (centi_celsius < 0) ? (uint32_t)(-centi_celsius) : (uint32_t)centi_celsius;
    sign = (centi_celsius < 0) ? "-" : "";

    (void)snprintf(out_text,
                   out_text_size,
                   "%s%lu.%02lu C",
                   sign,
                   (unsigned long)(abs_centi_celsius / 100U),
                   (unsigned long)(abs_centi_celsius % 100U));
}

static ev_result_t ev_demo_app_clear_previous_oled_frame(ev_demo_app_actor_state_t *state)
{
    ev_demo_app_t *app;
    ev_result_t rc;

    if ((state == NULL) || (state->app == NULL)) {
        return EV_ERR_INVALID_ARG;
    }
    if (!state->oled_frame_visible) {
        return EV_OK;
    }

    app = state->app;
    rc = ev_demo_app_publish_oled_text(app,
                                       (uint8_t)(state->last_page_offset + EV_DEMO_APP_OLED_TITLE_PAGE_OFFSET),
                                       state->last_column_offset,
                                       "");
    if (rc != EV_OK) {
        return rc;
    }

    rc = ev_demo_app_publish_oled_text(app,
                                       (uint8_t)(state->last_page_offset + EV_DEMO_APP_OLED_TIME_PAGE_OFFSET),
                                       state->last_column_offset,
                                       "");
    if (rc != EV_OK) {
        return rc;
    }

    return ev_demo_app_publish_oled_text(app,
                                         (uint8_t)(state->last_page_offset + EV_DEMO_APP_OLED_TEMP_PAGE_OFFSET),
                                         state->last_column_offset,
                                         "");
}

static ev_result_t ev_demo_app_render_oled_frame(ev_demo_app_actor_state_t *state)
{
    ev_demo_app_t *app;
    ev_result_t rc;
    char time_text[EV_OLED_TEXT_MAX_CHARS] = {0};
    char temp_text[EV_OLED_TEXT_MAX_CHARS] = {0};

    if ((state == NULL) || (state->app == NULL)) {
        return EV_ERR_INVALID_ARG;
    }

    app = state->app;
    ev_demo_app_format_time_text(state, time_text, sizeof(time_text));
    ev_demo_app_format_temp_text(state, temp_text, sizeof(temp_text));

    rc = ev_demo_app_clear_previous_oled_frame(state);
    if (rc != EV_OK) {
        return rc;
    }

    rc = ev_demo_app_publish_oled_text(app,
                                       (uint8_t)(state->current_page_offset + EV_DEMO_APP_OLED_TITLE_PAGE_OFFSET),
                                       state->current_column_offset,
                                       EV_DEMO_APP_OLED_TITLE_TEXT);
    if (rc != EV_OK) {
        return rc;
    }

    rc = ev_demo_app_publish_oled_text(app,
                                       (uint8_t)(state->current_page_offset + EV_DEMO_APP_OLED_TIME_PAGE_OFFSET),
                                       state->current_column_offset,
                                       time_text);
    if (rc != EV_OK) {
        return rc;
    }

    rc = ev_demo_app_publish_oled_text(app,
                                       (uint8_t)(state->current_page_offset + EV_DEMO_APP_OLED_TEMP_PAGE_OFFSET),
                                       state->current_column_offset,
                                       temp_text);
    if (rc != EV_OK) {
        return rc;
    }

    state->last_page_offset = state->current_page_offset;
    state->last_column_offset = state->current_column_offset;
    state->oled_frame_visible = true;
    return EV_OK;
}
#endif

static void ev_demo_app_screensaver_step_axis(uint8_t *value, int8_t *direction, uint8_t max_value)
{
    if ((value == NULL) || (direction == NULL)) {
        return;
    }

    if (max_value == 0U) {
        *value = 0U;
        *direction = (int8_t)1;
        return;
    }

    if (*direction >= 0) {
        if (*value >= max_value) {
            *direction = (int8_t)-1;
            if (*value > 0U) {
                --(*value);
            }
        } else {
            ++(*value);
        }
    } else if (*value == 0U) {
        *direction = (int8_t)1;
        ++(*value);
    } else {
        --(*value);
    }
}

static ev_result_t ev_demo_app_handle_tick_for_oled(ev_demo_app_actor_state_t *state)
{
    if ((state == NULL) || (state->app == NULL)) {
        return EV_ERR_INVALID_ARG;
    }

    if (!state->screensaver_paused) {
        ev_demo_app_screensaver_step_axis(&state->current_column_offset,
                                          &state->direction_x,
                                          EV_DEMO_APP_OLED_MAX_COLUMN_OFFSET);
        ev_demo_app_screensaver_step_axis(&state->current_page_offset,
                                          &state->direction_y,
                                          EV_DEMO_APP_OLED_MAX_PAGE_OFFSET);
    }

#ifndef EV_HOST_BUILD
    return ev_demo_app_render_oled_frame(state);
#else
    return EV_OK;
#endif
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
        {
            ev_result_t rc;

            ++app->stats.boot_completions;
            state->current_page_offset = 0U;
            state->current_column_offset = 0U;
            state->last_page_offset = 0U;
            state->last_column_offset = 0U;
            state->direction_x = (int8_t)1;
            state->direction_y = (int8_t)1;
            state->oled_frame_visible = false;
            state->screensaver_paused = false;
            state->panel_led_mask = 0U;
            ev_demo_app_logf(app, EV_LOG_INFO, "app actor: boot complete -> requesting diag snapshot");

            rc = ev_demo_app_publish_panel_led_command(app, 0U, EV_MCP23008_LED_MASK);
            if (rc != EV_OK) {
                return rc;
            }

            return ev_demo_app_publish_diag_request(state);
        }

    case EV_TICK_1S:
        return ev_demo_app_handle_tick_for_oled(state);

    case EV_TEMP_UPDATED:
        {
            const ev_temp_payload_t *temp_payload = (const ev_temp_payload_t *)ev_msg_payload_data(msg);

            if ((temp_payload == NULL) || (ev_msg_payload_size(msg) != sizeof(*temp_payload))) {
                return EV_ERR_CONTRACT;
            }

            state->last_temp = *temp_payload;
            state->temp_valid = true;
            return EV_OK;
        }

    case EV_TIME_UPDATED:
        {
            const ev_time_payload_t *time_payload = (const ev_time_payload_t *)ev_msg_payload_data(msg);

            if ((time_payload == NULL) || (ev_msg_payload_size(msg) != sizeof(*time_payload))) {
                return EV_ERR_CONTRACT;
            }

            state->last_time = *time_payload;
            state->time_valid = true;
            return EV_OK;
        }

    case EV_BUTTON_EVENT:
        {
            const ev_button_event_payload_t *button_payload =
                (const ev_button_event_payload_t *)ev_msg_payload_data(msg);

            if ((button_payload == NULL) || (ev_msg_payload_size(msg) != sizeof(*button_payload))) {
                return EV_ERR_CONTRACT;
            }

            if ((button_payload->button_id == EV_DEMO_APP_BUTTON_TOGGLE_SCREENSAVER) &&
                (button_payload->action == EV_BUTTON_ACTION_SHORT)) {
                ev_result_t rc;

                state->screensaver_paused = !state->screensaver_paused;
                if (state->screensaver_paused) {
                    state->panel_led_mask = (uint8_t)(state->panel_led_mask | EV_DEMO_APP_LED_SCREENSAVER_PAUSED);
                } else {
                    state->panel_led_mask =
                        (uint8_t)(state->panel_led_mask & (uint8_t)(~EV_DEMO_APP_LED_SCREENSAVER_PAUSED));
                }

                rc = ev_demo_app_publish_panel_led_command(app,
                                                           state->panel_led_mask,
                                                           EV_DEMO_APP_LED_SCREENSAVER_PAUSED);
                if (rc != EV_OK) {
                    return rc;
                }

                ev_demo_app_logf(app,
                                 EV_LOG_INFO,
                                 "app actor: screensaver %s",
                                 state->screensaver_paused ? "paused" : "resumed");
            }

            return EV_OK;
        }

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

    case EV_TICK_100MS:
        return EV_OK;

    case EV_GPIO_IRQ:
        {
            const ev_irq_sample_t *sample = (const ev_irq_sample_t *)ev_msg_payload_data(msg);

            if ((sample == NULL) || (ev_msg_payload_size(msg) != sizeof(*sample))) {
                return EV_ERR_CONTRACT;
            }

            return EV_OK;
        }

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
#ifndef EV_HOST_BUILD
    ev_i2c_port_t *active_i2c;
    ev_onewire_port_t *active_onewire;
#endif

    if ((app == NULL) || !ev_demo_app_config_is_valid(cfg)) {
        return EV_ERR_INVALID_ARG;
    }

    memset(app, 0, sizeof(*app));
    app->clock_port = cfg->clock_port;
    app->log_port = cfg->log_port;
    app->irq_port = cfg->irq_port;
    app->app_tag = cfg->app_tag;
    app->board_name = cfg->board_name;
    app->tick_period_ms = (cfg->tick_period_ms == 0U) ? EV_DEMO_APP_DEFAULT_TICK_MS : cfg->tick_period_ms;
    app->app_actor.app = app;
    app->app_actor.direction_x = (int8_t)1;
    app->app_actor.direction_y = (int8_t)1;
    app->diag_actor.app = app;

#ifndef EV_HOST_BUILD
    active_i2c = cfg->i2c_port;
    active_onewire = cfg->onewire_port;
    if ((active_i2c == NULL) || (active_onewire == NULL)) {
        return EV_ERR_INVALID_ARG;
    }
#endif

    rc = ev_demo_app_now_ms(app, &now_ms);
    if (rc != EV_OK) return rc;
    app->next_tick_ms = now_ms + app->tick_period_ms;
    app->next_tick_100ms_ms = now_ms + EV_DEMO_APP_FAST_TICK_MS;

    /* Inicjalizacja skrzynek pocztowych */
    rc = ev_mailbox_init(&app->app_mailbox, EV_MAILBOX_FIFO_8, app->app_storage, EV_ARRAY_LEN(app->app_storage));
    if (rc != EV_OK) return rc;

    rc = ev_mailbox_init(&app->diag_mailbox, EV_MAILBOX_FIFO_8, app->diag_storage, EV_ARRAY_LEN(app->diag_storage));
    if (rc != EV_OK) return rc;

    rc = ev_mailbox_init(&app->panel_mailbox, EV_MAILBOX_FIFO_8, app->panel_storage, EV_ARRAY_LEN(app->panel_storage));
    if (rc != EV_OK) return rc;

#ifndef EV_HOST_BUILD
    rc = ev_mailbox_init(&app->rtc_mailbox, EV_MAILBOX_FIFO_8, app->rtc_storage, EV_ARRAY_LEN(app->rtc_storage));
    if (rc != EV_OK) return rc;

    rc = ev_mailbox_init(&app->mcp23008_mailbox,
                         EV_MAILBOX_FIFO_8,
                         app->mcp23008_storage,
                         EV_ARRAY_LEN(app->mcp23008_storage));
    if (rc != EV_OK) return rc;

    rc = ev_mailbox_init(&app->ds18b20_mailbox,
                         EV_MAILBOX_FIFO_8,
                         app->ds18b20_storage,
                         EV_ARRAY_LEN(app->ds18b20_storage));
    if (rc != EV_OK) return rc;

    rc = ev_mailbox_init(&app->oled_mailbox, EV_MAILBOX_FIFO_8, app->oled_storage, EV_ARRAY_LEN(app->oled_storage));
    if (rc != EV_OK) return rc;
#endif

    /* Inicjalizacja Wątków Aktorów (Runtimes) */
    rc = ev_actor_runtime_init(&app->app_runtime, ACT_APP, &app->app_mailbox, ev_demo_app_actor_handler, &app->app_actor);
    if (rc != EV_OK) return rc;

    rc = ev_actor_runtime_init(&app->diag_runtime, ACT_DIAG, &app->diag_mailbox, ev_demo_diag_actor_handler, &app->diag_actor);
    if (rc != EV_OK) return rc;

    rc = ev_panel_actor_init(&app->panel_ctx, ev_actor_registry_delivery, &app->registry);
    if (rc != EV_OK) return rc;

    rc = ev_actor_runtime_init(&app->panel_runtime, ACT_PANEL, &app->panel_mailbox, ev_panel_actor_handle, &app->panel_ctx);
    if (rc != EV_OK) return rc;

#ifndef EV_HOST_BUILD
    rc = ev_mcp23008_actor_init(&app->mcp23008_ctx,
                                  active_i2c,
                                  EV_I2C_PORT_NUM_0,
                                  EV_MCP23008_DEFAULT_ADDR_7BIT,
                                  ev_actor_registry_delivery,
                                  &app->registry);
    if (rc != EV_OK) return rc;

    rc = ev_actor_runtime_init(&app->mcp23008_runtime,
                               ACT_MCP23008,
                               &app->mcp23008_mailbox,
                               ev_mcp23008_actor_handle,
                               &app->mcp23008_ctx);
    if (rc != EV_OK) return rc;

    rc = ev_rtc_actor_init(&app->rtc_ctx,
                           active_i2c,
                           app->irq_port,
                           EV_I2C_PORT_NUM_0,
                           EV_RTC_DEFAULT_ADDR_7BIT,
                           EV_DEMO_APP_RTC_SQW_LINE_ID,
                           ev_actor_registry_delivery,
                           &app->registry);
    if (rc != EV_OK) return rc;

    rc = ev_actor_runtime_init(&app->rtc_runtime, ACT_RTC, &app->rtc_mailbox, ev_rtc_actor_handle, &app->rtc_ctx);
    if (rc != EV_OK) return rc;

    rc = ev_ds18b20_actor_init(&app->ds18b20_ctx, active_onewire, ev_actor_registry_delivery, &app->registry);
    if (rc != EV_OK) return rc;

    rc = ev_actor_runtime_init(&app->ds18b20_runtime,
                               ACT_DS18B20,
                               &app->ds18b20_mailbox,
                               ev_ds18b20_actor_handle,
                               &app->ds18b20_ctx);
    if (rc != EV_OK) return rc;

    rc = ev_oled_actor_init(&app->oled_ctx,
                            active_i2c,
                            EV_I2C_PORT_NUM_0,
                            EV_OLED_DEFAULT_ADDR_7BIT,
                            EV_OLED_CONTROLLER_SSD1306);
    if (rc != EV_OK) return rc;

    rc = ev_actor_runtime_init(&app->oled_runtime, ACT_OLED, &app->oled_mailbox, ev_oled_actor_handle, &app->oled_ctx);
    if (rc != EV_OK) return rc;
#endif

    /* Rejestracja w Systemie Aktorów */
    rc = ev_actor_registry_init(&app->registry);
    if (rc != EV_OK) return rc;

    rc = ev_actor_registry_bind(&app->registry, &app->app_runtime);
    if (rc != EV_OK) return rc;

    rc = ev_actor_registry_bind(&app->registry, &app->diag_runtime);
    if (rc != EV_OK) return rc;

    rc = ev_actor_registry_bind(&app->registry, &app->panel_runtime);
    if (rc != EV_OK) return rc;

#ifndef EV_HOST_BUILD
    rc = ev_actor_registry_bind(&app->registry, &app->mcp23008_runtime);
    if (rc != EV_OK) return rc;

    rc = ev_actor_registry_bind(&app->registry, &app->rtc_runtime);
    if (rc != EV_OK) return rc;

    rc = ev_actor_registry_bind(&app->registry, &app->ds18b20_runtime);
    if (rc != EV_OK) return rc;

    rc = ev_actor_registry_bind(&app->registry, &app->oled_runtime);
    if (rc != EV_OK) return rc;
#endif

    /* Inicjalizacja pomp zdarzeń */
    rc = ev_domain_pump_init(&app->fast_domain, &app->registry, EV_DOMAIN_FAST_LOOP);
    if (rc != EV_OK) return rc;

    rc = ev_domain_pump_init(&app->slow_domain, &app->registry, EV_DOMAIN_SLOW_IO);
    if (rc != EV_OK) return rc;

    rc = ev_system_pump_init(&app->system_pump);
    if (rc != EV_OK) return rc;

    rc = ev_system_pump_bind(&app->system_pump, &app->fast_domain);
    if (rc != EV_OK) return rc;

    rc = ev_system_pump_bind(&app->system_pump, &app->slow_domain);
    if (rc != EV_OK) return rc;

    rc = ev_lease_pool_init(&app->lease_pool, app->lease_slots, app->lease_storage, EV_DEMO_APP_LEASE_SLOTS, EV_DEMO_APP_SNAPSHOT_BYTES);
    if (rc != EV_OK) return rc;

    ev_demo_app_logf(app, EV_LOG_INFO, "demo runtime ready board=%s tick_period_ms=%u", app->board_name, (unsigned)app->tick_period_ms);
    return EV_OK;
}

ev_result_t ev_demo_app_publish_boot(ev_demo_app_t *app)
{
    ev_msg_t msg;
    ev_result_t rc;

    if (app == NULL) return EV_ERR_INVALID_ARG;
    if (app->boot_published) return EV_ERR_STATE;

    rc = ev_msg_init_publish(&msg, EV_BOOT_STARTED, ACT_BOOT);
    if (rc != EV_OK) return rc;
    
    rc = ev_demo_app_publish_owned(app, &msg);
    if (rc != EV_OK) return rc;

    rc = ev_msg_init_publish(&msg, EV_BOOT_COMPLETED, ACT_BOOT);
    if (rc != EV_OK) return rc;
    
    rc = ev_demo_app_publish_owned(app, &msg);
    if (rc != EV_OK) return rc;

    app->boot_published = true;
    return EV_OK;
}

ev_result_t ev_demo_app_poll(ev_demo_app_t *app)
{
    ev_result_t rc;
    uint32_t now_ms;

    if (app == NULL) return EV_ERR_INVALID_ARG;
    if (!app->boot_published) return EV_ERR_STATE;

    rc = ev_demo_app_drain_until_idle(app);
    if (rc != EV_OK) return rc;

#ifndef EV_HOST_BUILD
    if ((app->irq_port != NULL) && (app->irq_port->pop != NULL)) {
        ev_irq_sample_t sample = {0};

        for (;;) {
            rc = app->irq_port->pop(app->irq_port->ctx, &sample);
            if (rc == EV_ERR_EMPTY) {
                break;
            }
            if (rc != EV_OK) {
                return rc;
            }

            rc = ev_demo_app_publish_irq_sample(app, &sample);
            if (rc != EV_OK) {
                return rc;
            }

            rc = ev_demo_app_drain_until_idle(app);
            if (rc != EV_OK) {
                return rc;
            }
        }
    }
#endif

    rc = ev_demo_app_now_ms(app, &now_ms);
    if (rc != EV_OK) return rc;

    for (;;) {
        const bool tick_100ms_due = ((int32_t)(now_ms - app->next_tick_100ms_ms) >= 0);
        const bool tick_1s_due = ((int32_t)(now_ms - app->next_tick_ms) >= 0);

        if (!tick_100ms_due && !tick_1s_due) {
            break;
        }

        if (tick_100ms_due && (!tick_1s_due || ((int32_t)(app->next_tick_100ms_ms - app->next_tick_ms) <= 0))) {
            rc = ev_demo_app_publish_tick_100ms(app);
            if (rc != EV_OK) return rc;
            app->next_tick_100ms_ms += EV_DEMO_APP_FAST_TICK_MS;

            rc = ev_demo_app_drain_until_idle(app);
            if (rc != EV_OK) return rc;
            continue;
        }

        rc = ev_demo_app_publish_tick(app);
        if (rc != EV_OK) return rc;
        app->next_tick_ms += app->tick_period_ms;

        rc = ev_demo_app_drain_until_idle(app);
        if (rc != EV_OK) return rc;
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

