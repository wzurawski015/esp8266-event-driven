#ifndef EV_PORT_IRQ_H
#define EV_PORT_IRQ_H

#include <stdint.h>

#include "ev/result.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Stable logical identifier of one adapter-owned interrupt line.
 */
typedef uint8_t ev_irq_line_id_t;

/**
 * @brief Normalized interrupt edge classification.
 */
typedef uint8_t ev_irq_edge_t;

#define EV_IRQ_EDGE_RISING ((ev_irq_edge_t)1U)
#define EV_IRQ_EDGE_FALLING ((ev_irq_edge_t)2U)

/**
 * @brief One interrupt sample popped from an adapter-owned ring buffer.
 *
 * The contract intentionally exposes only normalized logical data required by
 * the portable runtime. Concrete platform adapters remain responsible for GPIO
 * numbering, ISR bookkeeping, edge filtering, and timestamp capture.
 */
typedef struct {
    uint8_t line_id;
    ev_irq_edge_t edge;
    uint8_t level;
    uint32_t timestamp_us;
} ev_irq_sample_t;

/**
 * @brief Pop one pending interrupt sample from the adapter buffer.
 *
 * @param ctx Adapter-owned context bound into the public port object.
 * @param out_sample Destination sample populated on success.
 * @return EV_OK when one sample was returned, EV_ERR_EMPTY when no sample is
 *         pending, or another error code when the adapter is not usable.
 */
typedef ev_result_t (*ev_irq_pop_fn_t)(void *ctx, ev_irq_sample_t *out_sample);

/**
 * @brief Platform interrupt-ingress contract.
 */
typedef struct ev_irq_port {
    void *ctx;
    ev_irq_pop_fn_t pop;
} ev_irq_port_t;

#ifdef __cplusplus
}
#endif

#endif /* EV_PORT_IRQ_H */
