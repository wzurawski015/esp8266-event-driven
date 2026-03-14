#include "ev/publish.h"

#include "ev/msg.h"
#include "ev/route_table.h"

ev_result_t ev_publish(const ev_msg_t *msg, ev_delivery_fn_t deliver, void *context, size_t *delivered_count)
{
    ev_result_t rc;
    size_t delivered = 0U;
    size_t i;

    if ((msg == NULL) || (deliver == NULL)) {
        return EV_ERR_INVALID_ARG;
    }

    rc = ev_msg_validate(msg);
    if (rc != EV_OK) {
        return rc;
    }
    if (msg->target_actor != EV_ACTOR_NONE) {
        return EV_ERR_CONTRACT;
    }

    for (i = 0U; i < ev_route_count(); ++i) {
        const ev_route_t *route = ev_route_at(i);
        if ((route != NULL) && (route->event_id == msg->event_id)) {
            rc = deliver(route->target_actor, msg, context);
            if (rc != EV_OK) {
                if (delivered_count != NULL) {
                    *delivered_count = delivered;
                }
                return rc;
            }
            ++delivered;
        }
    }

    if (delivered_count != NULL) {
        *delivered_count = delivered;
    }

    return (delivered > 0U) ? EV_OK : EV_ERR_NOT_FOUND;
}
