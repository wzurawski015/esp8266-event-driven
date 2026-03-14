#include <assert.h>
#include <stddef.h>

#include "ev/actor_runtime.h"
#include "ev/dispose.h"
#include "ev/msg.h"
#include "ev/publish.h"
#include "ev/send.h"

typedef struct {
    size_t calls;
    ev_event_id_t last_event;
    ev_actor_id_t last_source;
} handler_trace_t;

typedef struct {
    size_t retains;
    size_t releases;
} lease_trace_t;

static ev_result_t retain_count(void *ctx, const void *payload, size_t payload_size)
{
    lease_trace_t *trace = (lease_trace_t *)ctx;
    (void)payload;
    (void)payload_size;
    ++trace->retains;
    return EV_OK;
}

static void release_count(void *ctx, const void *payload, size_t payload_size)
{
    lease_trace_t *trace = (lease_trace_t *)ctx;
    (void)payload;
    (void)payload_size;
    ++trace->releases;
}

static ev_result_t trace_handler(void *actor_context, const ev_msg_t *msg)
{
    handler_trace_t *trace = (handler_trace_t *)actor_context;

    ++trace->calls;
    trace->last_event = msg->event_id;
    trace->last_source = msg->source_actor;
    return EV_OK;
}

int main(void)
{
    ev_msg_t diag_storage[8] = {{0}};
    ev_msg_t app_storage[8] = {{0}};
    ev_mailbox_t diag_mailbox;
    ev_mailbox_t app_mailbox;
    ev_actor_runtime_t diag_runtime;
    ev_actor_runtime_t app_runtime;
    ev_actor_registry_t registry;
    ev_actor_registry_t partial_registry;
    handler_trace_t diag_trace = {0};
    handler_trace_t app_trace = {0};
    lease_trace_t lease_trace = {0};
    ev_msg_t msg;
    ev_publish_report_t report;
    size_t delivered = 0U;
    static const unsigned char lease_bytes[] = {0x10U, 0x20U, 0x30U};

    assert(ev_mailbox_init(&diag_mailbox, EV_MAILBOX_FIFO_8, diag_storage, 8U) == EV_OK);
    assert(ev_mailbox_init(&app_mailbox, EV_MAILBOX_FIFO_8, app_storage, 8U) == EV_OK);

    assert(ev_actor_runtime_init(&diag_runtime, ACT_DIAG, &diag_mailbox, trace_handler, &diag_trace) == EV_OK);
    assert(ev_actor_runtime_init(&app_runtime, ACT_APP, &app_mailbox, trace_handler, &app_trace) == EV_OK);

    assert(ev_actor_registry_init(&registry) == EV_OK);
    assert(ev_actor_registry_bind(&registry, &diag_runtime) == EV_OK);
    assert(ev_actor_registry_bind(&registry, &app_runtime) == EV_OK);
    assert(ev_actor_registry_bind(&registry, &diag_runtime) == EV_ERR_STATE);

    assert(ev_msg_init_publish(&msg, EV_BOOT_STARTED, ACT_BOOT) == EV_OK);
    assert(ev_publish(&msg, ev_actor_registry_delivery, &registry, &delivered) == EV_OK);
    assert(delivered == 1U);
    assert(ev_actor_runtime_pending(&diag_runtime) == 1U);
    assert(ev_actor_runtime_step(&diag_runtime) == EV_OK);
    assert(diag_trace.calls == 1U);
    assert(diag_trace.last_event == EV_BOOT_STARTED);
    assert(diag_trace.last_source == ACT_BOOT);
    assert(ev_actor_runtime_step(&diag_runtime) == EV_ERR_EMPTY);

    assert(ev_msg_init_publish(&msg, EV_BOOT_COMPLETED, ACT_BOOT) == EV_OK);
    assert(ev_publish(&msg, ev_actor_registry_delivery, &registry, &delivered) == EV_OK);
    assert(delivered == 2U);
    assert(ev_actor_runtime_pending(&diag_runtime) == 1U);
    assert(ev_actor_runtime_pending(&app_runtime) == 1U);
    assert(ev_actor_runtime_step(&diag_runtime) == EV_OK);
    assert(ev_actor_runtime_step(&app_runtime) == EV_OK);
    assert(diag_trace.calls == 2U);
    assert(diag_trace.last_event == EV_BOOT_COMPLETED);
    assert(app_trace.calls == 1U);
    assert(app_trace.last_event == EV_BOOT_COMPLETED);

    assert(ev_actor_registry_init(&partial_registry) == EV_OK);
    assert(ev_actor_registry_bind(&partial_registry, &diag_runtime) == EV_OK);
    ev_publish_report_reset(&report);
    assert(ev_msg_init_publish(&msg, EV_BOOT_COMPLETED, ACT_BOOT) == EV_OK);
    assert(ev_publish_ex(&msg, ev_actor_registry_delivery, &partial_registry, EV_PUBLISH_BEST_EFFORT, &report) == EV_ERR_PARTIAL);
    assert(report.matched_routes == 2U);
    assert(report.delivered_count == 1U);
    assert(report.failed_count == 1U);
    assert(report.first_failed_actor == ACT_APP);
    assert(report.first_error == EV_ERR_NOT_FOUND);
    assert(ev_actor_runtime_pending(&diag_runtime) == 1U);
    assert(ev_actor_runtime_pending(&app_runtime) == 0U);
    assert(ev_actor_runtime_step(&diag_runtime) == EV_OK);
    assert(diag_trace.calls == 3U);

    assert(ev_msg_init_send(&msg, EV_DIAG_SNAPSHOT_REQ, ACT_APP, ACT_DIAG) == EV_OK);
    assert(ev_send(ACT_DIAG, &msg, ev_actor_registry_delivery, &registry) == EV_OK);
    assert(ev_actor_runtime_pending(&diag_runtime) == 1U);
    assert(ev_actor_runtime_step(&diag_runtime) == EV_OK);
    assert(diag_trace.calls == 4U);
    assert(diag_trace.last_event == EV_DIAG_SNAPSHOT_REQ);
    assert(diag_trace.last_source == ACT_APP);

    /* Lease payload must retain once for the queued copy and release once per owner. */
    assert(ev_msg_init_publish(&msg, EV_DIAG_SNAPSHOT_RSP, ACT_DIAG) == EV_OK);
    assert(ev_msg_set_external_payload(
               &msg,
               lease_bytes,
               sizeof(lease_bytes),
               retain_count,
               release_count,
               &lease_trace) == EV_OK);
    assert(ev_publish(&msg, ev_actor_registry_delivery, &registry, &delivered) == EV_OK);
    assert(delivered == 1U);
    assert(lease_trace.retains == 1U);
    assert(ev_actor_runtime_pending(&app_runtime) == 1U);
    assert(ev_actor_runtime_step(&app_runtime) == EV_OK);
    assert(app_trace.calls == 2U);
    assert(app_trace.last_event == EV_DIAG_SNAPSHOT_RSP);
    assert(lease_trace.releases == 1U);
    assert(ev_msg_dispose(&msg) == EV_OK);
    assert(lease_trace.releases == 2U);

    return 0;
}
