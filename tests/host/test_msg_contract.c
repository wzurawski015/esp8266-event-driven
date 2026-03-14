#include <assert.h>
#include <stddef.h>
#include <string.h>

#include "ev/dispose.h"
#include "ev/msg.h"

static void release_counter(void *release_ctx, const void *payload, size_t payload_size)
{
    size_t *counter = (size_t *)release_ctx;
    (void)payload;
    (void)payload_size;
    ++(*counter);
}

int main(void)
{
    ev_msg_t msg = {0};
    const unsigned char ok_bytes[] = {0x4fU, 0x4bU};
    const unsigned char lease_bytes[] = {0x01U, 0x02U, 0x03U};
    size_t releases = 0U;

    assert(ev_msg_dispose(&msg) == EV_OK);
    assert(ev_msg_is_disposed(&msg));
    assert(ev_msg_dispose(&msg) == EV_OK);

    assert(ev_msg_init_publish(&msg, EV_BOOT_STARTED, ACT_BOOT) == EV_OK);
    assert(msg.target_actor == EV_ACTOR_NONE);
    assert(ev_msg_set_inline_payload(&msg, ok_bytes, sizeof(ok_bytes)) == EV_OK);
    assert(ev_msg_validate(&msg) == EV_OK);
    assert(ev_msg_payload_size(&msg) == sizeof(ok_bytes));
    assert(memcmp(ev_msg_payload_data(&msg), ok_bytes, sizeof(ok_bytes)) == 0);

    assert(ev_msg_init_publish(&msg, EV_STREAM_CHUNK_READY, ACT_STREAM) == EV_OK);
    assert(ev_msg_set_inline_payload(&msg, ok_bytes, sizeof(ok_bytes)) == EV_ERR_CONTRACT);
    assert(ev_msg_set_external_payload(&msg, lease_bytes, sizeof(lease_bytes), NULL, NULL) == EV_ERR_CONTRACT);
    assert(ev_msg_set_external_payload(&msg, lease_bytes, sizeof(lease_bytes), release_counter, &releases) == EV_OK);
    assert(ev_msg_validate(&msg) == EV_OK);
    assert(ev_msg_dispose(&msg) == EV_OK);
    assert(releases == 1U);
    assert(ev_msg_dispose(&msg) == EV_OK);
    assert(releases == 1U);

    assert(ev_msg_init_send(&msg, EV_DIAG_SNAPSHOT_REQ, ACT_APP, ACT_DIAG) == EV_OK);
    assert(msg.target_actor == ACT_DIAG);
    assert(ev_msg_validate(&msg) == EV_OK);

    assert(ev_msg_init_send(&msg, EV_DIAG_SNAPSHOT_REQ, ACT_APP, EV_ACTOR_NONE) == EV_ERR_OUT_OF_RANGE);

    return 0;
}
