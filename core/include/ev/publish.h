#ifndef EV_PUBLISH_H
#define EV_PUBLISH_H

#include <stddef.h>

#include "ev/delivery.h"
#include "ev/result.h"

/**
 * @brief Deliver a message through the static publish route table.
 *
 * @param msg Message to publish. Its target actor must be EV_ACTOR_NONE.
 * @param deliver Callback representing the mailbox insertion contract.
 * @param context Caller-provided callback context.
 * @param delivered_count Optional output receiving the number of deliveries.
 * @return EV_OK on success or an error code.
 */
ev_result_t ev_publish(
    const ev_msg_t *msg,
    ev_delivery_fn_t deliver,
    void *context,
    size_t *delivered_count);

#endif /* EV_PUBLISH_H */
