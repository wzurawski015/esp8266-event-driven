#ifndef EV_ACTOR_ID_H
#define EV_ACTOR_ID_H

#include "ev/execution_domain.h"
#include "ev/mailbox_kind.h"

/**
 * @brief Actor identifiers generated from config/actors.def.
 */
typedef enum {
#define EV_ACTOR(name, execution_domain, mailbox_kind, summary) name,
#include "actors.def"
#undef EV_ACTOR
    EV_ACTOR_COUNT
} ev_actor_id_t;

#endif /* EV_ACTOR_ID_H */
