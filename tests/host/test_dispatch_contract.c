#include <assert.h>
#include <stddef.h>

#include "ev/msg.h"
#include "ev/publish.h"
#include "ev/send.h"

typedef struct {
    size_t call_count;
    size_t fail_after;
    ev_actor_id_t last_target;
    ev_event_id_t last_event;
} trace_t;

static ev_result_t trace_delivery(ev_actor_id_t target_actor, const ev_msg_t *msg, void *context)
{
    trace_t *trace = (trace_t *)context;

    trace->last_target = target_actor;
    trace->last_event = msg->event_id;
    ++trace->call_count;

    if ((trace->fail_after > 0U) && (trace->call_count >= trace->fail_after)) {
        return EV_ERR_NOT_FOUND;
    }

    return EV_OK;
}

int main(void)
{
    ev_msg_t msg;
    trace_t trace = {0};
    size_t delivered = 0U;

    assert(ev_msg_init_publish(&msg, EV_BOOT_STARTED, ACT_BOOT) == EV_OK);
    assert(ev_publish(&msg, trace_delivery, &trace, &delivered) == EV_OK);
    assert(delivered == 1U);
    assert(trace.call_count == 1U);
    assert(trace.last_target == ACT_DIAG);
    assert(trace.last_event == EV_BOOT_STARTED);

    trace = (trace_t){0};
    assert(ev_msg_init_send(&msg, EV_DIAG_SNAPSHOT_REQ, ACT_APP, ACT_DIAG) == EV_OK);
    assert(ev_send(ACT_DIAG, &msg, trace_delivery, &trace) == EV_OK);
    assert(trace.call_count == 1U);
    assert(trace.last_target == ACT_DIAG);
    assert(trace.last_event == EV_DIAG_SNAPSHOT_REQ);

    trace = (trace_t){0};
    assert(ev_send(ACT_APP, &msg, trace_delivery, &trace) == EV_ERR_CONTRACT);
    assert(trace.call_count == 0U);

    trace = (trace_t){0};
    assert(ev_publish(&msg, trace_delivery, &trace, &delivered) == EV_ERR_CONTRACT);
    assert(trace.call_count == 0U);

    trace = (trace_t){0};
    trace.fail_after = 1U;
    assert(ev_msg_init_publish(&msg, EV_BOOT_STARTED, ACT_BOOT) == EV_OK);
    assert(ev_publish(&msg, trace_delivery, &trace, &delivered) == EV_ERR_NOT_FOUND);
    assert(delivered == 0U);
    assert(trace.call_count == 1U);

    return 0;
}
