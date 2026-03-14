#include "ev/actor_runtime.h"

#include <string.h>

#include "ev/actor_catalog.h"
#include "ev/dispose.h"

ev_result_t ev_actor_runtime_init(
    ev_actor_runtime_t *runtime,
    ev_actor_id_t actor_id,
    ev_mailbox_t *mailbox,
    ev_actor_handler_fn_t handler,
    void *actor_context)
{
    const ev_actor_meta_t *meta;

    if ((runtime == NULL) || (mailbox == NULL) || (handler == NULL)) {
        return EV_ERR_INVALID_ARG;
    }
    if (!ev_actor_id_is_valid(actor_id)) {
        return EV_ERR_OUT_OF_RANGE;
    }

    meta = ev_actor_meta(actor_id);
    if (meta == NULL) {
        return EV_ERR_OUT_OF_RANGE;
    }
    if (mailbox->kind != meta->mailbox_kind) {
        return EV_ERR_CONTRACT;
    }

    memset(runtime, 0, sizeof(*runtime));
    runtime->actor_id = actor_id;
    runtime->mailbox = mailbox;
    runtime->handler = handler;
    runtime->actor_context = actor_context;
    return EV_OK;
}

ev_result_t ev_actor_registry_init(ev_actor_registry_t *registry)
{
    if (registry == NULL) {
        return EV_ERR_INVALID_ARG;
    }

    memset(registry, 0, sizeof(*registry));
    return EV_OK;
}

ev_actor_runtime_t *ev_actor_registry_find(ev_actor_registry_t *registry, ev_actor_id_t actor_id)
{
    if ((registry == NULL) || !ev_actor_id_is_valid(actor_id)) {
        return NULL;
    }

    return registry->slots[actor_id];
}

ev_result_t ev_actor_registry_bind(ev_actor_registry_t *registry, ev_actor_runtime_t *runtime)
{
    if ((registry == NULL) || (runtime == NULL)) {
        return EV_ERR_INVALID_ARG;
    }
    if (!ev_actor_id_is_valid(runtime->actor_id)) {
        return EV_ERR_OUT_OF_RANGE;
    }
    if (registry->slots[runtime->actor_id] != NULL) {
        return EV_ERR_STATE;
    }

    registry->slots[runtime->actor_id] = runtime;
    return EV_OK;
}

ev_result_t ev_actor_registry_delivery(ev_actor_id_t target_actor, const ev_msg_t *msg, void *context)
{
    ev_actor_registry_t *registry = (ev_actor_registry_t *)context;
    ev_actor_runtime_t *runtime;

    if (registry == NULL) {
        return EV_ERR_INVALID_ARG;
    }

    runtime = ev_actor_registry_find(registry, target_actor);
    if (runtime == NULL) {
        return EV_ERR_NOT_FOUND;
    }

    return ev_mailbox_push(runtime->mailbox, msg);
}

ev_result_t ev_actor_runtime_step(ev_actor_runtime_t *runtime)
{
    ev_result_t rc;
    ev_msg_t msg;

    if ((runtime == NULL) || (runtime->mailbox == NULL) || (runtime->handler == NULL)) {
        return EV_ERR_INVALID_ARG;
    }

    rc = ev_mailbox_pop(runtime->mailbox, &msg);
    if (rc != EV_OK) {
        return rc;
    }

    rc = runtime->handler(runtime->actor_context, &msg);
    if (ev_msg_dispose(&msg) != EV_OK) {
        return (rc == EV_OK) ? EV_ERR_STATE : rc;
    }

    return rc;
}

size_t ev_actor_runtime_pending(const ev_actor_runtime_t *runtime)
{
    return ((runtime != NULL) && (runtime->mailbox != NULL)) ? ev_mailbox_count(runtime->mailbox) : 0U;
}
