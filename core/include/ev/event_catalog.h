#ifndef EV_EVENT_CATALOG_H
#define EV_EVENT_CATALOG_H

#include <stddef.h>

#include "ev/event_id.h"
#include "ev/payload_kind.h"

/**
 * @brief Metadata associated with a declared event.
 */
typedef struct {
    ev_event_id_t id;
    const char *name;
    ev_payload_kind_t payload_kind;
    const char *summary;
} ev_event_meta_t;

/**
 * @brief Return the number of declared events.
 *
 * @return Number of items in the event catalog.
 */
size_t ev_event_count(void);

/**
 * @brief Look up metadata for a declared event.
 *
 * @param id Event identifier.
 * @return Pointer to catalog entry or NULL if out of range.
 */
const ev_event_meta_t *ev_event_meta(ev_event_id_t id);

/**
 * @brief Return a stable textual name for an event identifier.
 *
 * @param id Event identifier.
 * @return Constant string or NULL if out of range.
 */
const char *ev_event_name(ev_event_id_t id);

#endif /* EV_EVENT_CATALOG_H */
