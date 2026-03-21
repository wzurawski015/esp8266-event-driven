#include <assert.h>
#include <stdbool.h>
#include <stddef.h>

#include "ev/actor_catalog.h"
#include "ev/event_catalog.h"
#include "ev/route_table.h"

int main(void)
{
    size_t i;
    size_t j;

#ifdef EV_HOST_BUILD
    assert(ev_route_count() == 11U);
    assert(ev_route_count_for_event(EV_BOOT_COMPLETED) == 2U);
    assert(!ev_route_exists(EV_BOOT_COMPLETED, ACT_DS18B20));
    assert(!ev_route_exists(EV_TICK_1S, ACT_DS18B20));
#else
    assert(ev_route_count() == 17U);
    assert(ev_route_count_for_event(EV_BOOT_COMPLETED) == 4U);
    assert(ev_route_exists(EV_BOOT_COMPLETED, ACT_OLED));
    assert(ev_route_exists(EV_BOOT_COMPLETED, ACT_DS18B20));
    assert(ev_route_exists(EV_TICK_1S, ACT_RTC));
    assert(ev_route_exists(EV_TICK_1S, ACT_DS18B20));
#endif
    assert(ev_route_count_for_event(EV_BOOT_STARTED) == 1U);
    assert(ev_route_count_for_event(EV_DIAG_SNAPSHOT_RSP) == 1U);
    assert(ev_route_count_for_event(EV_TICK_100MS) == 1U);
    assert(ev_route_count_for_event(EV_GPIO_IRQ) == 1U);
    assert(ev_route_count_for_event(EV_TIME_UPDATED) == 1U);
    assert(ev_route_count_for_event(EV_TEMP_UPDATED) == 1U);
    assert(ev_route_exists(EV_BOOT_COMPLETED, ACT_DIAG));
    assert(ev_route_exists(EV_BOOT_COMPLETED, ACT_APP));
    assert(ev_route_exists(EV_DIAG_SNAPSHOT_RSP, ACT_APP));
    assert(ev_route_exists(EV_TICK_100MS, ACT_DIAG));
    assert(ev_route_exists(EV_GPIO_IRQ, ACT_DIAG));
    assert(ev_route_exists(EV_TIME_UPDATED, ACT_APP));
    assert(ev_route_exists(EV_TEMP_UPDATED, ACT_APP));
    assert(!ev_route_exists(EV_BOOT_STARTED, ACT_STREAM));

    for (i = 0U; i < ev_route_count(); ++i) {
        const ev_route_t *route = ev_route_at(i);
        assert(route != NULL);
        assert(ev_event_id_is_valid(route->event_id));
        assert(ev_actor_id_is_valid(route->target_actor));
    }

    for (i = 0U; i < ev_route_count(); ++i) {
        const ev_route_t *lhs = ev_route_at(i);
        assert(lhs != NULL);
        for (j = i + 1U; j < ev_route_count(); ++j) {
            const ev_route_t *rhs = ev_route_at(j);
            assert(rhs != NULL);
            assert(!((lhs->event_id == rhs->event_id) && (lhs->target_actor == rhs->target_actor)));
        }
    }

    return 0;
}
