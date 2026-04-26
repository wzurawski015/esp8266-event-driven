#ifndef EV_ESP8266_PORT_ADAPTERS_H
#define EV_ESP8266_PORT_ADAPTERS_H

#include "ev/port_clock.h"
#include "ev/port_i2c.h"
#include "ev/port_irq.h"
#include "ev/port_gpio_irq.h"
#include "ev/port_onewire.h"
#include "ev/port_log.h"
#include "ev/port_reset.h"
#include "ev/port_uart.h"
#include "ev/system_port.h"

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
 * The adapter owns a zero-heap GPIO open-drain software I2C master and creates
 * one bootstrap-time global bus mutex used to serialize all bounded
 * transactions.  Runtime calls do not allocate SDK command links.
 *
 * @param out_port Destination public contract populated on success.
 * @param sda_pin GPIO number used for SDA.
 * @param scl_pin GPIO number used for SCL.
 * @return EV_OK on success or an error code.
 */
ev_result_t ev_esp8266_i2c_port_init(ev_i2c_port_t *out_port, int sda_pin, int scl_pin);

/**
 * @brief Scan one initialized I2C controller and log detected slaves.
 *
 * The scan performs bounded address-only probes across the public 7-bit I2C
 * address range and annotates the known Stage 1 motherboard devices in the
 * diagnostic log stream.
 *
 * @param port_num Logical I2C controller identifier.
 * @return EV_OK when the scan completed, even if no device acknowledged.
 */
ev_result_t ev_i2c_scan(ev_i2c_port_num_t port_num);

/**
 * @brief Initialize the ESP8266-backed GPIO interrupt ingress adapter.
 *
 * The adapter owns one static ring buffer populated from a very small ISR. The
 * public Core-facing contract exposes normalized interrupt samples through
 * ev_irq_port_t::pop and explicit per-line arming through ev_irq_port_t::enable.
 *
 * @param out_port Destination public contract populated on success.
 * @param line_cfgs Static line-to-GPIO mappings owned by the caller.
 * @param line_count Number of entries in @p line_cfgs.
 * @return EV_OK on success or an error code.
 */
ev_result_t ev_esp8266_irq_port_init(ev_irq_port_t *out_port,
                                     const ev_gpio_irq_line_config_t *line_cfgs,
                                     size_t line_count);

/**
 * @brief Initialize the ESP8266-backed 1-Wire adapter.
 *
 * The adapter configures one open-drain GPIO line used later by the DS18B20
 * actor through the portable 1-Wire contract.
 *
 * @param out_port Destination public contract populated on success.
 * @param data_pin GPIO number used for the shared 1-Wire data line.
 * @return EV_OK on success or an error code.
 */
ev_result_t ev_esp8266_onewire_port_init(ev_onewire_port_t *out_port, int data_pin);

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
 * @brief Initialize the ESP8266-backed system-control adapter.
 *
 * @param out_port Destination contract populated on success.
 * @return EV_OK on success or an error code.
 */
ev_result_t ev_esp8266_system_port_init(ev_system_port_t *out_port);

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
