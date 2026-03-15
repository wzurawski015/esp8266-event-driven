#ifndef EV_PORT_GPIO_H
#define EV_PORT_GPIO_H

#include <stdbool.h>
#include <stdint.h>

#include "ev/result.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t ev_gpio_num_t;

typedef enum ev_gpio_direction {
    EV_GPIO_INPUT = 0,
    EV_GPIO_OUTPUT = 1
} ev_gpio_direction_t;

typedef enum ev_gpio_pull {
    EV_GPIO_PULL_FLOATING = 0,
    EV_GPIO_PULL_UP = 1,
    EV_GPIO_PULL_DOWN = 2
} ev_gpio_pull_t;

typedef struct ev_gpio_config {
    ev_gpio_direction_t direction;
    ev_gpio_pull_t pull;
    bool open_drain;
    bool initial_high;
} ev_gpio_config_t;

typedef struct ev_gpio_port {
    void *ctx;
    ev_result_t (*configure)(void *ctx, ev_gpio_num_t pin, const ev_gpio_config_t *cfg);
    ev_result_t (*write)(void *ctx, ev_gpio_num_t pin, bool high);
    ev_result_t (*read)(void *ctx, ev_gpio_num_t pin, bool *out_high);
} ev_gpio_port_t;

#ifdef __cplusplus
}
#endif

#endif /* EV_PORT_GPIO_H */
