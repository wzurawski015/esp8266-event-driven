#include <assert.h>
#include <stddef.h>

#include "ev/mailbox.h"
#include "ev/msg.h"

static void release_noop(void *ctx, const void *payload, size_t payload_size)
{
    (void)ctx;
    (void)payload;
    (void)payload_size;
}

static ev_msg_t make_tick_msg(void)
{
    ev_msg_t msg;
    const unsigned char payload[] = {0x01U};

    assert(ev_msg_init_publish(&msg, EV_TICK_1S, ACT_BOOT) == EV_OK);
    assert(ev_msg_set_inline_payload(&msg, payload, sizeof(payload)) == EV_OK);
    assert(ev_msg_validate(&msg) == EV_OK);
    return msg;
}

int main(void)
{
    ev_msg_t fifo_storage[8] = {{0}};
    ev_msg_t fifo16_storage[16] = {{0}};
    ev_msg_t one_storage[1] = {{0}};
    ev_msg_t lossy_storage[8] = {{0}};
    ev_msg_t flag_storage[1] = {{0}};
    ev_mailbox_t mailbox;
    ev_msg_t out;
    ev_msg_t msg = make_tick_msg();
    size_t i;

    assert(ev_mailbox_kind_capacity(EV_MAILBOX_FIFO_8) == 8U);
    assert(ev_mailbox_kind_capacity(EV_MAILBOX_FIFO_16) == 16U);
    assert(ev_mailbox_kind_capacity(EV_MAILBOX_MAILBOX_1) == 1U);
    assert(ev_mailbox_kind_capacity(EV_MAILBOX_LOSSY_RING_8) == 8U);
    assert(ev_mailbox_kind_capacity(EV_MAILBOX_COALESCED_FLAG) == 1U);
    assert(ev_mailbox_kind_is_lossy(EV_MAILBOX_LOSSY_RING_8));
    assert(ev_mailbox_kind_is_coalescing(EV_MAILBOX_COALESCED_FLAG));

    assert(ev_mailbox_init(&mailbox, EV_MAILBOX_FIFO_8, fifo_storage, 8U) == EV_OK);
    for (i = 0U; i < 8U; ++i) {
        assert(ev_mailbox_push(&mailbox, &msg) == EV_OK);
    }
    assert(ev_mailbox_push(&mailbox, &msg) == EV_ERR_FULL);
    assert(ev_mailbox_is_full(&mailbox));
    assert(ev_mailbox_stats(&mailbox)->rejected == 1U);
    for (i = 0U; i < 8U; ++i) {
        assert(ev_mailbox_pop(&mailbox, &out) == EV_OK);
        assert(out.event_id == EV_TICK_1S);
    }
    assert(ev_mailbox_pop(&mailbox, &out) == EV_ERR_EMPTY);

    assert(ev_mailbox_init(&mailbox, EV_MAILBOX_FIFO_16, fifo16_storage, 16U) == EV_OK);
    assert(ev_mailbox_capacity(&mailbox) == 16U);

    assert(ev_mailbox_init(&mailbox, EV_MAILBOX_MAILBOX_1, one_storage, 1U) == EV_OK);
    assert(ev_mailbox_push(&mailbox, &msg) == EV_OK);
    assert(ev_mailbox_push(&mailbox, &msg) == EV_OK);
    assert(ev_mailbox_count(&mailbox) == 1U);
    assert(ev_mailbox_stats(&mailbox)->replaced == 1U);

    assert(ev_mailbox_init(&mailbox, EV_MAILBOX_LOSSY_RING_8, lossy_storage, 8U) == EV_OK);
    for (i = 0U; i < 9U; ++i) {
        assert(ev_mailbox_push(&mailbox, &msg) == EV_OK);
    }
    assert(ev_mailbox_count(&mailbox) == 8U);
    assert(ev_mailbox_stats(&mailbox)->dropped == 1U);

    assert(ev_mailbox_init(&mailbox, EV_MAILBOX_COALESCED_FLAG, flag_storage, 1U) == EV_OK);
    assert(ev_msg_init_publish(&msg, EV_BOOT_STARTED, ACT_BOOT) == EV_OK);
    assert(ev_mailbox_push(&mailbox, &msg) == EV_OK);
    assert(ev_mailbox_push(&mailbox, &msg) == EV_OK);
    assert(ev_mailbox_stats(&mailbox)->coalesced == 1U);
    assert(ev_mailbox_count(&mailbox) == 1U);
    assert(ev_msg_init_publish(&msg, EV_BOOT_COMPLETED, ACT_BOOT) == EV_OK);
    assert(ev_mailbox_push(&mailbox, &msg) == EV_ERR_CONTRACT);

    /* External storage remains unsupported for mailbox enqueue in this stage. */
    assert(ev_msg_init_publish(&msg, EV_DIAG_SNAPSHOT_RSP, ACT_DIAG) == EV_OK);
    {
        const unsigned char lease_bytes[] = {0xAAU, 0xBBU};
        assert(ev_msg_set_external_payload(&msg, lease_bytes, sizeof(lease_bytes), release_noop, NULL) == EV_OK);
    }
    assert(ev_mailbox_init(&mailbox, EV_MAILBOX_FIFO_8, fifo_storage, 8U) == EV_OK);
    assert(ev_mailbox_push(&mailbox, &msg) == EV_ERR_UNSUPPORTED);

    return 0;
}
