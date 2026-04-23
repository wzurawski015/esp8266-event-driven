#include "fake_irq_port.h"

#include <string.h>

static ev_result_t fake_irq_pop(void *ctx, ev_irq_sample_t *out_sample)
{
    fake_irq_port_t *fake = (fake_irq_port_t *)ctx;

    if ((fake == NULL) || (out_sample == NULL)) {
        return EV_ERR_INVALID_ARG;
    }

    ++fake->pop_calls;
    if (fake->count == 0U) {
        return EV_ERR_EMPTY;
    }

    *out_sample = fake->ring[fake->head];
    fake->head = (fake->head + 1U) % (sizeof(fake->ring) / sizeof(fake->ring[0]));
    --fake->count;
    return EV_OK;
}

static ev_result_t fake_irq_enable(void *ctx, ev_irq_line_id_t line_id, bool enabled)
{
    fake_irq_port_t *fake = (fake_irq_port_t *)ctx;

    if ((fake == NULL) || (line_id >= (ev_irq_line_id_t)(sizeof(fake->enabled) / sizeof(fake->enabled[0])))) {
        return EV_ERR_INVALID_ARG;
    }

    ++fake->enable_calls;
    fake->enabled[line_id] = enabled;
    return EV_OK;
}

void fake_irq_port_init(fake_irq_port_t *fake)
{
    if (fake != NULL) {
        memset(fake, 0, sizeof(*fake));
    }
}

void fake_irq_port_bind(ev_irq_port_t *out_port, fake_irq_port_t *fake)
{
    if (out_port != NULL) {
        memset(out_port, 0, sizeof(*out_port));
        out_port->ctx = fake;
        out_port->pop = fake_irq_pop;
        out_port->enable = fake_irq_enable;
    }
}

ev_result_t fake_irq_port_push(fake_irq_port_t *fake, const ev_irq_sample_t *sample)
{
    size_t capacity;

    if ((fake == NULL) || (sample == NULL)) {
        return EV_ERR_INVALID_ARG;
    }

    capacity = sizeof(fake->ring) / sizeof(fake->ring[0]);
    if (fake->count >= capacity) {
        return EV_ERR_FULL;
    }

    fake->ring[fake->tail] = *sample;
    fake->tail = (fake->tail + 1U) % capacity;
    ++fake->count;
    return EV_OK;
}

bool fake_irq_port_is_enabled(const fake_irq_port_t *fake, ev_irq_line_id_t line_id)
{
    if ((fake == NULL) || (line_id >= (ev_irq_line_id_t)(sizeof(fake->enabled) / sizeof(fake->enabled[0])))) {
        return false;
    }

    return fake->enabled[line_id];
}
