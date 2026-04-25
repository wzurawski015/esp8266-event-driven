#ifndef EV_POWER_ACTOR_H
#define EV_POWER_ACTOR_H

#include <stdint.h>

#include "ev/compiler.h"
#include "ev/msg.h"
#include "ev/port_log.h"
#include "ev/system_port.h"
#include "ev/result.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    uint32_t duration_ms;
} ev_sys_goto_sleep_cmd_t;

EV_STATIC_ASSERT(sizeof(ev_sys_goto_sleep_cmd_t) <= EV_MSG_INLINE_CAPACITY,
                 "Deep sleep command payload must fit in one inline event payload");

typedef struct {
    ev_system_port_t *system_port;
    ev_log_port_t *log_port;
    const char *log_tag;
    uint32_t sleep_requests_seen;
    uint32_t sleep_requests_accepted;
    uint32_t sleep_requests_unsupported;
    uint32_t sleep_requests_rejected;
    uint32_t prepare_for_sleep_failures;
    uint64_t last_duration_us;
} ev_power_actor_ctx_t;

ev_result_t ev_power_actor_init(ev_power_actor_ctx_t *ctx,
                                ev_system_port_t *system_port,
                                ev_log_port_t *log_port,
                                const char *log_tag);

ev_result_t ev_power_actor_handle(void *actor_context, const ev_msg_t *msg);

#ifdef __cplusplus
}
#endif

#endif /* EV_POWER_ACTOR_H */
