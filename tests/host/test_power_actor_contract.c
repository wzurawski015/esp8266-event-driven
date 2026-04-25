#include <assert.h>

#include "ev/dispose.h"
#include "ev/power_actor.h"
#include "fakes/fake_system_port.h"

static ev_msg_t make_sleep_cmd(uint32_t duration_ms)
{
    ev_msg_t msg = EV_MSG_INITIALIZER;
    ev_sys_goto_sleep_cmd_t cmd = {0};

    cmd.duration_ms = duration_ms;
    assert(ev_msg_init_publish(&msg, EV_SYS_GOTO_SLEEP_CMD, ACT_APP) == EV_OK);
    assert(ev_msg_set_inline_payload(&msg, &cmd, sizeof(cmd)) == EV_OK);
    return msg;
}

static void test_prepare_then_deep_sleep(void)
{
    fake_system_port_t fake_system;
    ev_system_port_t system_port = {0};
    ev_power_actor_ctx_t power = {0};
    ev_msg_t msg = make_sleep_cmd(1234U);

    fake_system_port_init(&fake_system);
    fake_system_port_bind(&system_port, &fake_system);

    assert(ev_power_actor_init(&power, &system_port, NULL, "test_power") == EV_OK);
    assert(ev_power_actor_handle(&power, &msg) == EV_OK);
    assert(fake_system.prepare_for_sleep_calls == 1U);
    assert(fake_system.deep_sleep_calls == 1U);
    assert(fake_system.last_prepare_duration_us == 1234000ULL);
    assert(fake_system.last_duration_us == 1234000ULL);
    assert(power.sleep_requests_seen == 1U);
    assert(power.sleep_requests_accepted == 1U);
    assert(power.sleep_requests_rejected == 0U);
    assert(power.prepare_for_sleep_failures == 0U);

    assert(ev_msg_dispose(&msg) == EV_OK);
}

static void test_prepare_failure_rejects_sleep(void)
{
    fake_system_port_t fake_system;
    ev_system_port_t system_port = {0};
    ev_power_actor_ctx_t power = {0};
    ev_msg_t msg = make_sleep_cmd(10U);

    fake_system_port_init(&fake_system);
    fake_system.next_prepare_result = EV_ERR_STATE;
    fake_system_port_bind(&system_port, &fake_system);

    assert(ev_power_actor_init(&power, &system_port, NULL, "test_power") == EV_OK);
    assert(ev_power_actor_handle(&power, &msg) == EV_ERR_STATE);
    assert(fake_system.prepare_for_sleep_calls == 1U);
    assert(fake_system.deep_sleep_calls == 0U);
    assert(power.sleep_requests_seen == 1U);
    assert(power.sleep_requests_accepted == 0U);
    assert(power.sleep_requests_rejected == 1U);
    assert(power.prepare_for_sleep_failures == 1U);

    assert(ev_msg_dispose(&msg) == EV_OK);
}

static void test_unsupported_port_is_counted_without_sleep(void)
{
    ev_power_actor_ctx_t power = {0};
    ev_msg_t msg = make_sleep_cmd(1U);

    assert(ev_power_actor_init(&power, NULL, NULL, "test_power") == EV_OK);
    assert(ev_power_actor_handle(&power, &msg) == EV_OK);
    assert(power.sleep_requests_seen == 1U);
    assert(power.sleep_requests_unsupported == 1U);
    assert(power.sleep_requests_accepted == 0U);

    assert(ev_msg_dispose(&msg) == EV_OK);
}

int main(void)
{
    test_prepare_then_deep_sleep();
    test_prepare_failure_rejects_sleep();
    test_unsupported_port_is_counted_without_sleep();
    return 0;
}
