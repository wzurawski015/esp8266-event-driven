#ifndef EV_ACTOR_RUNTIME_H
#define EV_ACTOR_RUNTIME_H

#include <stddef.h>

#include "ev/actor_id.h"
#include "ev/delivery.h"
#include "ev/mailbox.h"
#include "ev/result.h"

/**
 * @brief Handler invoked when one message is drained from an actor mailbox.
 *
 * @param actor_context Caller-provided actor state.
 * @param msg Drained runtime envelope.
 * @return EV_OK on success or an error code.
 */
typedef ev_result_t (*ev_actor_handler_fn_t)(void *actor_context, const ev_msg_t *msg);

/**
 * @brief Runtime wiring for one actor instance.
 */
typedef struct {
    ev_actor_id_t actor_id;
    ev_mailbox_t *mailbox;
    ev_actor_handler_fn_t handler;
    void *actor_context;
} ev_actor_runtime_t;

/**
 * @brief Fixed-size registry mapping actor identifiers to runtimes.
 */
typedef struct {
    ev_actor_runtime_t *slots[EV_ACTOR_COUNT];
} ev_actor_registry_t;

/**
 * @brief Initialize one actor runtime.
 *
 * @param runtime Runtime to initialize.
 * @param actor_id Actor identifier.
 * @param mailbox Mailbox bound to this actor.
 * @param handler Message handler.
 * @param actor_context Caller-owned actor context.
 * @return EV_OK on success or an error code.
 */
ev_result_t ev_actor_runtime_init(
    ev_actor_runtime_t *runtime,
    ev_actor_id_t actor_id,
    ev_mailbox_t *mailbox,
    ev_actor_handler_fn_t handler,
    void *actor_context);

/**
 * @brief Initialize an empty actor registry.
 *
 * @param registry Registry to initialize.
 * @return EV_OK on success or an error code.
 */
ev_result_t ev_actor_registry_init(ev_actor_registry_t *registry);

/**
 * @brief Bind one runtime into a registry slot.
 *
 * @param registry Registry to update.
 * @param runtime Runtime to bind.
 * @return EV_OK on success or an error code.
 */
ev_result_t ev_actor_registry_bind(ev_actor_registry_t *registry, ev_actor_runtime_t *runtime);

/**
 * @brief Look up a runtime by actor identifier.
 *
 * @param registry Registry to inspect.
 * @param actor_id Actor identifier.
 * @return Bound runtime or NULL when absent.
 */
ev_actor_runtime_t *ev_actor_registry_find(ev_actor_registry_t *registry, ev_actor_id_t actor_id);

/**
 * @brief Delivery callback that enqueues into actor mailboxes.
 *
 * @param target_actor Target actor selected by send/publish.
 * @param msg Message to enqueue.
 * @param context Pointer to ev_actor_registry_t.
 * @return EV_OK on success or an error code.
 */
ev_result_t ev_actor_registry_delivery(ev_actor_id_t target_actor, const ev_msg_t *msg, void *context);

/**
 * @brief Drain at most one pending message from one actor runtime.
 *
 * The drained envelope is disposed after the handler returns.
 *
 * @param runtime Runtime to step.
 * @return EV_OK on success, EV_ERR_EMPTY when no message is pending, or an error code.
 */
ev_result_t ev_actor_runtime_step(ev_actor_runtime_t *runtime);

/**
 * @brief Return the number of pending messages for one runtime.
 *
 * @param runtime Runtime to inspect.
 * @return Pending message count.
 */
size_t ev_actor_runtime_pending(const ev_actor_runtime_t *runtime);

#endif /* EV_ACTOR_RUNTIME_H */
