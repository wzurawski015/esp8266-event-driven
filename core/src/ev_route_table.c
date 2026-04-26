#include "ev/route_table.h"

#include "ev/actor_catalog.h"
#include "ev/compiler.h"
#include "ev/event_catalog.h"

/* Compile-time duplicate-route guard: repeating the same (event_id, target_actor)
 * pair generates the same enumerator name and fails the build immediately. */
enum {
#define EV_ROUTE(event_id, target_actor) EV_ROUTE_UNIQUE__##event_id##__##target_actor = 1,
#include "routes.def"
#undef EV_ROUTE
    EV_ROUTE_UNIQUE__SENTINEL = 0
};

static const ev_route_t k_route_table[] = {
    { EV_BOOT_STARTED, ACT_DIAG },
    { EV_BOOT_COMPLETED, ACT_DIAG },
    { EV_BOOT_COMPLETED, ACT_APP },
    { EV_BOOT_COMPLETED, ACT_MCP23008 },
    { EV_BOOT_COMPLETED, ACT_RTC },
    { EV_BOOT_COMPLETED, ACT_DS18B20 },
    { EV_BOOT_COMPLETED, ACT_OLED },
    { EV_BOOT_COMPLETED, ACT_SUPERVISOR },
    { EV_TICK_1S, ACT_DIAG },
    { EV_TICK_1S, ACT_APP },
    { EV_TICK_1S, ACT_RTC },
    { EV_TICK_1S, ACT_DS18B20 },
    { EV_TICK_1S, ACT_OLED },
    { EV_TICK_1S, ACT_SUPERVISOR },
    { EV_STREAM_CHUNK_READY, ACT_STREAM },
    { EV_DIAG_SNAPSHOT_REQ, ACT_DIAG },
    { EV_DIAG_SNAPSHOT_RSP, ACT_APP },
    { EV_OLED_DISPLAY_TEXT_CMD, ACT_OLED },
    { EV_OLED_COMMIT_FRAME, ACT_OLED },
    { EV_TIME_UPDATED, ACT_APP },
    { EV_TEMP_UPDATED, ACT_APP },
    { EV_TICK_100MS, ACT_DIAG },
    { EV_TICK_100MS, ACT_PANEL },
    { EV_TICK_100MS, ACT_MCP23008 },
    { EV_GPIO_IRQ, ACT_DIAG },
    { EV_GPIO_IRQ, ACT_RTC },
    { EV_MCP23008_INPUT_CHANGED, ACT_PANEL },
    { EV_BUTTON_EVENT, ACT_APP },
    { EV_PANEL_LED_SET_CMD, ACT_MCP23008 },
    { EV_MCP23008_READY, ACT_DIAG },
    { EV_MCP23008_READY, ACT_RTC },
    { EV_MCP23008_READY, ACT_SUPERVISOR },
    { EV_RTC_READY, ACT_SUPERVISOR },
    { EV_OLED_READY, ACT_SUPERVISOR },
    { EV_DS18B20_READY, ACT_SUPERVISOR },
    { EV_SYSTEM_READY, ACT_APP },
    { EV_SYS_GOTO_SLEEP_CMD, ACT_POWER },
};

static const ev_route_span_t k_route_spans[EV_EVENT_COUNT] = {
    [EV_BOOT_STARTED] = { 0U, 1U },
    [EV_BOOT_COMPLETED] = { 1U, 7U },
    [EV_TICK_1S] = { 8U, 6U },
    [EV_STREAM_CHUNK_READY] = { 14U, 1U },
    [EV_DIAG_SNAPSHOT_REQ] = { 15U, 1U },
    [EV_DIAG_SNAPSHOT_RSP] = { 16U, 1U },
    [EV_OLED_DISPLAY_TEXT_CMD] = { 17U, 1U },
    [EV_OLED_COMMIT_FRAME] = { 18U, 1U },
    [EV_TIME_UPDATED] = { 19U, 1U },
    [EV_TEMP_UPDATED] = { 20U, 1U },
    [EV_TICK_100MS] = { 21U, 3U },
    [EV_GPIO_IRQ] = { 24U, 2U },
    [EV_MCP23008_INPUT_CHANGED] = { 26U, 1U },
    [EV_BUTTON_EVENT] = { 27U, 1U },
    [EV_PANEL_LED_SET_CMD] = { 28U, 1U },
    [EV_MCP23008_READY] = { 29U, 3U },
    [EV_RTC_READY] = { 32U, 1U },
    [EV_OLED_READY] = { 33U, 1U },
    [EV_DS18B20_READY] = { 34U, 1U },
    [EV_SYSTEM_READY] = { 35U, 1U },
    [EV_SYS_GOTO_SLEEP_CMD] = { 36U, 1U },
};

EV_STATIC_ASSERT(EV_ARRAY_LEN(k_route_table) > 0U, "route table must not be empty");
EV_STATIC_ASSERT(EV_ARRAY_LEN(k_route_spans) == EV_EVENT_COUNT, "route span table must cover all events");
EV_STATIC_ASSERT(EV_ARRAY_LEN(k_route_table) == 37U, "route table must match config/routes.def");

static bool ev_route_span_is_valid(ev_route_span_t span)
{
    return (span.start_index <= EV_ARRAY_LEN(k_route_table)) &&
           (span.count <= (EV_ARRAY_LEN(k_route_table) - span.start_index));
}

size_t ev_route_count(void)
{
    return EV_ARRAY_LEN(k_route_table);
}

const ev_route_t *ev_route_at(size_t index)
{
    if (index >= EV_ARRAY_LEN(k_route_table)) {
        return NULL;
    }

    return &k_route_table[index];
}

ev_route_span_t ev_route_span_for_event(ev_event_id_t event_id)
{
    ev_route_span_t empty_span = {0U, 0U};

    if (!ev_event_id_is_valid(event_id)) {
        return empty_span;
    }

    if (!ev_route_span_is_valid(k_route_spans[event_id])) {
        return empty_span;
    }

    return k_route_spans[event_id];
}

size_t ev_route_count_for_event(ev_event_id_t event_id)
{
    return ev_route_span_for_event(event_id).count;
}

bool ev_route_exists(ev_event_id_t event_id, ev_actor_id_t target_actor)
{
    ev_route_span_t span;
    size_t i;

    if (!ev_actor_id_is_valid(target_actor)) {
        return false;
    }

    span = ev_route_span_for_event(event_id);
    for (i = 0U; i < span.count; ++i) {
        const ev_route_t *route = &k_route_table[span.start_index + i];
        if (route->target_actor == target_actor) {
            return true;
        }
    }

    return false;
}
