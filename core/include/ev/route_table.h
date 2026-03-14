#ifndef EV_ROUTE_TABLE_H
#define EV_ROUTE_TABLE_H

#include <stdbool.h>
#include <stddef.h>

#include "ev/actor_id.h"
#include "ev/event_id.h"

/**
 * @brief Single static publish route entry.
 */
typedef struct {
    ev_event_id_t event_id;
    ev_actor_id_t target_actor;
} ev_route_t;

/**
 * @brief Return the number of declared static routes.
 *
 * @return Number of route entries.
 */
size_t ev_route_count(void);

/**
 * @brief Return a route entry by index.
 *
 * @param index Zero-based route index.
 * @return Route pointer or NULL when out of range.
 */
const ev_route_t *ev_route_at(size_t index);

/**
 * @brief Count static routes for a given event.
 *
 * @param event_id Event identifier.
 * @return Number of route entries for the event.
 */
size_t ev_route_count_for_event(ev_event_id_t event_id);

/**
 * @brief Test whether a specific event-to-actor route exists.
 *
 * @param event_id Event identifier.
 * @param target_actor Target actor identifier.
 * @return true when the route exists, otherwise false.
 */
bool ev_route_exists(ev_event_id_t event_id, ev_actor_id_t target_actor);

#endif /* EV_ROUTE_TABLE_H */
