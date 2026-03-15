#ifndef EV_ESP8266_PORT_ADAPTERS_H
#define EV_ESP8266_PORT_ADAPTERS_H

#include "ev/port_clock.h"
#include "ev/port_log.h"
#include "ev/port_reset.h"
#include "ev/port_uart.h"

#ifdef __cplusplus
extern "C" {
#endif

ev_result_t ev_esp8266_clock_port_init(ev_clock_port_t *out_port);
ev_result_t ev_esp8266_log_port_init(ev_log_port_t *out_port);
ev_result_t ev_esp8266_reset_port_init(ev_reset_port_t *out_port);
ev_result_t ev_esp8266_uart_port_init(ev_uart_port_t *out_port);

const char *ev_reset_reason_to_cstr(ev_reset_reason_t reason);

#ifdef __cplusplus
}
#endif

#endif /* EV_ESP8266_PORT_ADAPTERS_H */
