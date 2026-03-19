#ifndef EV_ESP8266_PORT_ADAPTERS_H
#define EV_ESP8266_PORT_ADAPTERS_H

#include "ev/port_clock.h"
#include "ev/port_i2c.h"
#include "ev/port_log.h"
#include "ev/port_reset.h"
#include "ev/port_uart.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize the ESP8266-backed clock adapter.
 *
 * @param out_port Destination contract populated on success.
 * @return EV_OK on success or an error code.
 */
ev_result_t ev_esp8266_clock_port_init(ev_clock_port_t *out_port);

/**
 * @brief Initialize the ESP8266-backed I2C adapter.
 *
 * The adapter installs the SDK master driver for `I2C_NUM_0`, creates one
 * global bus mutex, and performs the current board self-test scan.
 *
 * @param out_port Destination public contract populated on success.
 * @param sda_pin GPIO number used for SDA.
 * @param scl_pin GPIO number used for SCL.
 * @return EV_OK on success or an error code.
 */
ev_result_t ev_esp8266_i2c_port_init(ev_i2c_port_t *out_port, int sda_pin, int scl_pin);

/**
 * @brief Initialize the ESP8266-backed log adapter.
 *
 * @param out_port Destination contract populated on success.
 * @return EV_OK on success or an error code.
 */
ev_result_t ev_esp8266_log_port_init(ev_log_port_t *out_port);

/**
 * @brief Initialize the ESP8266-backed reset adapter.
 *
 * @param out_port Destination contract populated on success.
 * @return EV_OK on success or an error code.
 */
ev_result_t ev_esp8266_reset_port_init(ev_reset_port_t *out_port);

/**
 * @brief Initialize the ESP8266-backed UART adapter.
 *
 * @param out_port Destination contract populated on success.
 * @return EV_OK on success or an error code.
 */
ev_result_t ev_esp8266_uart_port_init(ev_uart_port_t *out_port);

/**
 * @brief Convert one normalized reset reason into a stable diagnostic string.
 *
 * @param reason Reset reason to render.
 * @return Stable string literal.
 */
const char *ev_reset_reason_to_cstr(ev_reset_reason_t reason);

#ifdef __cplusplus
}
#endif

#endif /* EV_ESP8266_PORT_ADAPTERS_H */
