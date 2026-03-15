#include "ev/actor_runtime.h"

#include <string.h>

#include "ev/actor_catalog.h"
#include "ev/dispose.h"

static void ev_actor_runtime_record_pending_high_watermark(ev_actor_runtime_t *runtime)
{
    size_t pending;

    if ((runtime == NULL) || (runtime->mailbox == NULL)) {
        return;
    }

    pending = ev_mailbox_count(runtime->mailbox);
    if (pending > runtime->stats.pending_high_watermark) {
        runtime->stats.pending_high_watermark = pending;
    }
}

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
    runtime->stats.last_result = EV_OK;
    return EV_OK;
}

ev_result_t ev_actor_registry_init(ev_actor_registry_t *registry)
{
    if (registry == NULL) {
        return EV_ERR_INVALID_ARG;
    }

    memset(registry, 0, sizeof(*registry));
    registry->stats.last_target_actor = EV_ACTOR_NONE;
    registry->stats.last_result = EV_OK;
    return EV_OK;
}

ev_result_t ev_actor_registry_reset_stats(ev_actor_registry_t *registry)
{
    if (registry == NULL) {
        return EV_ERR_INVALID_ARG;
    }

    memset(&registry->stats, 0, sizeof(registry->stats));
    registry->stats.last_target_actor = EV_ACTOR_NONE;
    registry->stats.last_result = EV_OK;
    return EV_OK;
}

const ev_actor_registry_stats_t *ev_actor_registry_stats(const ev_actor_registry_t *registry)
{
    return (registry != NULL) ? &registry->stats : NULL;
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
    ev_result_t rc;

    if (registry == NULL) {
        return EV_ERR_INVALID_ARG;
    }

    ++registry->stats.delivery_attempted;
    registry->stats.last_target_actor = target_actor;

    runtime = ev_actor_registry_find(registry, target_actor);
    if (runtime == NULL) {
        ++registry->stats.delivery_failed;
        ++registry->stats.delivery_target_missing;
        registry->stats.last_result = EV_ERR_NOT_FOUND;
        return EV_ERR_NOT_FOUND;
    }

    rc = ev_mailbox_push(runtime->mailbox, msg);
    runtime->stats.last_result = rc;
    if (rc == EV_OK) {
        ++registry->stats.delivery_succeeded;
        registry->stats.last_result = EV_OK;
        ++runtime->stats.enqueued;
        ev_actor_runtime_record_pending_high_watermark(runtime);
        return EV_OK;
    }

    ++registry->stats.delivery_failed;
    registry->stats.last_result = rc;
    ++runtime->stats.enqueue_failed;
    return rc;
}

ev_result_t ev_actor_runtime_step(ev_actor_runtime_t *runtime)
{
    ev_result_t rc;
    ev_result_t dispose_rc;
    ev_msg_t msg;

    if ((runtime == NULL) || (runtime->mailbox == NULL) || (runtime->handler == NULL)) {
        return EV_ERR_INVALID_ARG;
    }

    rc = ev_mailbox_pop(runtime->mailbox, &msg);
    if (rc != EV_OK) {
        if (rc == EV_ERR_EMPTY) {
            ++runtime->stats.steps_empty;
        }
        runtime->stats.last_result = rc;
        return rc;
    }

    rc = runtime->handler(runtime->actor_context, &msg);
    if (rc != EV_OK) {
        ++runtime->stats.handler_errors;
    }

    dispose_rc = ev_msg_dispose(&msg);
    if (dispose_rc != EV_OK) {
        ++runtime->stats.dispose_errors;
        runtime->stats.last_result = (rc == EV_OK) ? EV_ERR_STATE : rc;
        return (rc == EV_OK) ? EV_ERR_STATE : rc;
    }

    if (rc == EV_OK) {
        ++runtime->stats.steps_ok;
    }
    runtime->stats.last_result = rc;
    return rc;
}

ev_result_t ev_actor_runtime_reset_stats(ev_actor_runtime_t *runtime)
{
    if (runtime == NULL) {
        return EV_ERR_INVALID_ARG;
    }

    memset(&runtime->stats, 0, sizeof(runtime->stats));
    runtime->stats.last_result = EV_OK;
    return EV_OK;
}

const ev_actor_runtime_stats_t *ev_actor_runtime_stats(const ev_actor_runtime_t *runtime)
{
    return (runtime != NULL) ? &runtime->stats : NULL;
}

size_t ev_actor_runtime_pending(const ev_actor_runtime_t *runtime)
{
    return ((runtime != NULL) && (runtime->mailbox != NULL)) ? ev_mailbox_count(runtime->mailbox) : 0U;
}
