#ifndef EV_SYSTEM_PORT_H
#define EV_SYSTEM_PORT_H

#include <stdint.h>

#include "ev/result.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Platform system-control contract.
 *
 * The core uses this port for operations that change the global machine
 * power state. Implementations must preserve bounded semantics until the
 * final handoff to the platform power-management primitive.
 */
typedef struct ev_system_port {
    void *ctx; /**< Caller-owned adapter context. */
    ev_result_t (*deep_sleep)(void *ctx, uint64_t duration_us); /**< Enter deep sleep for the requested duration. */
} ev_system_port_t;

#ifdef __cplusplus
}
#endif

#endif /* EV_SYSTEM_PORT_H */
