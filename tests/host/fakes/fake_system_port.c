#include "fake_system_port.h"

#include <string.h>

static ev_result_t fake_system_prepare_for_sleep(void *ctx, uint64_t duration_us)
{
    fake_system_port_t *fake = (fake_system_port_t *)ctx;

    if (fake == NULL) {
        return EV_ERR_INVALID_ARG;
    }

    ++fake->prepare_for_sleep_calls;
    fake->last_prepare_duration_us = duration_us;
    return fake->next_prepare_result;
}

static ev_result_t fake_system_deep_sleep(void *ctx, uint64_t duration_us)
{
    fake_system_port_t *fake = (fake_system_port_t *)ctx;

    if (fake == NULL) {
        return EV_ERR_INVALID_ARG;
    }

    ++fake->deep_sleep_calls;
    fake->last_duration_us = duration_us;
    return fake->next_result;
}

void fake_system_port_init(fake_system_port_t *fake)
{
    if (fake != NULL) {
        memset(fake, 0, sizeof(*fake));
        fake->next_prepare_result = EV_OK;
        fake->next_result = EV_OK;
    }
}

void fake_system_port_bind(ev_system_port_t *out_port, fake_system_port_t *fake)
{
    if (out_port != NULL) {
        out_port->ctx = fake;
        out_port->prepare_for_sleep = fake_system_prepare_for_sleep;
        out_port->deep_sleep = fake_system_deep_sleep;
    }
}
