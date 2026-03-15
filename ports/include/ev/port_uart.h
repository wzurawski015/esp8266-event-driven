#ifndef EV_PORT_UART_H
#define EV_PORT_UART_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "ev/result.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint8_t ev_uart_port_num_t;

typedef struct ev_uart_config {
    uint32_t baud_rate;
    uint8_t data_bits;
    uint8_t stop_bits;
    bool parity_enable;
    bool parity_odd;
} ev_uart_config_t;

typedef struct ev_uart_port {
    void *ctx;
    ev_result_t (*init)(void *ctx, ev_uart_port_num_t port, const ev_uart_config_t *cfg);
    ev_result_t (*write)(void *ctx, ev_uart_port_num_t port, const void *data, size_t len, size_t *out_written);
    ev_result_t (*read)(void *ctx, ev_uart_port_num_t port, void *data, size_t len, size_t *out_read);
} ev_uart_port_t;

#ifdef __cplusplus
}
#endif

#endif /* EV_PORT_UART_H */
