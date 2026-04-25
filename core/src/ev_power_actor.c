#include "ev/power_actor.h"

#include <stddef.h>
#include <string.h>

static ev_result_t ev_power_actor_log(ev_power_actor_ctx_t *ctx, const char *message)
{
    const char *tag;

    if ((ctx == NULL) || (message == NULL) || (ctx->log_port == NULL) ||
        (ctx->log_port->write == NULL)) {
        return EV_OK;
    }

    tag = (ctx->log_tag != NULL) ? ctx->log_tag : "ev_power";
    return ctx->log_port->write(ctx->log_port->ctx, EV_LOG_INFO, tag, message, strlen(message));
}

static ev_result_t ev_power_actor_flush_log(ev_power_actor_ctx_t *ctx)
{
    if ((ctx == NULL) || (ctx->log_port == NULL) || (ctx->log_port->flush == NULL)) {
        return EV_OK;
    }

    return ctx->log_port->flush(ctx->log_port->ctx);
}

ev_result_t ev_power_actor_init(ev_power_actor_ctx_t *ctx,
                                ev_system_port_t *system_port,
                                ev_log_port_t *log_port,
                                const char *log_tag)
{
    if (ctx == NULL) {
        return EV_ERR_INVALID_ARG;
    }

    memset(ctx, 0, sizeof(*ctx));
    ctx->system_port = system_port;
    ctx->log_port = log_port;
    ctx->log_tag = log_tag;
    return EV_OK;
}

ev_result_t ev_power_actor_handle(void *actor_context, const ev_msg_t *msg)
{
    ev_power_actor_ctx_t *ctx = (ev_power_actor_ctx_t *)actor_context;
    const ev_sys_goto_sleep_cmd_t *payload;
    uint64_t duration_us;
    ev_result_t rc;

    if ((ctx == NULL) || (msg == NULL)) {
        return EV_ERR_INVALID_ARG;
    }

    switch (msg->event_id) {
    case EV_SYS_GOTO_SLEEP_CMD:
        payload = (const ev_sys_goto_sleep_cmd_t *)ev_msg_payload_data(msg);
        if ((payload == NULL) || (ev_msg_payload_size(msg) != sizeof(*payload))) {
            return EV_ERR_CONTRACT;
        }

        ++ctx->sleep_requests_seen;
        duration_us = (uint64_t)payload->duration_ms * 1000ULL;
        ctx->last_duration_us = duration_us;

        (void)ev_power_actor_log(ctx, "System entering Deep Sleep");
        (void)ev_power_actor_flush_log(ctx);

        if ((ctx->system_port == NULL) || (ctx->system_port->deep_sleep == NULL)) {
            ++ctx->sleep_requests_unsupported;
            return EV_OK;
        }

        ++ctx->sleep_requests_accepted;
        rc = ctx->system_port->deep_sleep(ctx->system_port->ctx, duration_us);
        return (rc == EV_OK) ? EV_OK : rc;

    default:
        return EV_ERR_CONTRACT;
    }
}
