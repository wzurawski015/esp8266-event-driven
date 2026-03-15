#ifndef EV_PORT_CLOCK_H
#define EV_PORT_CLOCK_H

#include <stdint.h>

#include "ev/result.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint64_t ev_time_mono_us_t;
typedef uint64_t ev_time_wall_us_t;

typedef struct ev_clock_port {
    void *ctx;
    ev_result_t (*mono_now_us)(void *ctx, ev_time_mono_us_t *out_now);
    ev_result_t (*wall_now_us)(void *ctx, ev_time_wall_us_t *out_now);
    ev_result_t (*delay_ms)(void *ctx, uint32_t delay_ms);
} ev_clock_port_t;

#ifdef __cplusplus
}
#endif

#endif /* EV_PORT_CLOCK_H */
