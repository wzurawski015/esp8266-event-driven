#include "ev/route_table.h"

#include "ev/compiler.h"


/* Compile-time duplicate-route guard: repeating the same (event_id, target_actor)
 * pair generates the same enumerator name and fails the build immediately. */
enum {
#define EV_ROUTE(event_id, target_actor) EV_ROUTE_UNIQUE__##event_id##__##target_actor = 1,
#include "routes.def"
#undef EV_ROUTE
    EV_ROUTE_UNIQUE__SENTINEL = 0
};

static const ev_route_t k_route_table[] = {
#define EV_ROUTE(event_id, target_actor) { event_id, target_actor },
#include "routes.def"
#undef EV_ROUTE
};

EV_STATIC_ASSERT(EV_ARRAY_LEN(k_route_table) > 0U, "route table must not be empty");

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

size_t ev_route_count_for_event(ev_event_id_t event_id)
{
    size_t count = 0U;
    size_t i;

    for (i = 0U; i < EV_ARRAY_LEN(k_route_table); ++i) {
        if (k_route_table[i].event_id == event_id) {
            ++count;
        }
    }

    return count;
}

bool ev_route_exists(ev_event_id_t event_id, ev_actor_id_t target_actor)
{
    size_t i;

    for (i = 0U; i < EV_ARRAY_LEN(k_route_table); ++i) {
        if ((k_route_table[i].event_id == event_id) && (k_route_table[i].target_actor == target_actor)) {
            return true;
        }
    }

    return false;
}
