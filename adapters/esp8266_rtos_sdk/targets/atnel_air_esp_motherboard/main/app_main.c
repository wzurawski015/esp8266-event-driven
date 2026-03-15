#include <inttypes.h>
#include <stdbool.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "ev/esp8266_port_adapters.h"

#define EV_BOARD_TAG "ev_atnel"
#define EV_BOARD_NAME "atnel_air_esp_motherboard"

static void ev_diag_logf(ev_log_port_t *log_port,
                         ev_log_level_t level,
                         const char *tag,
                         const char *fmt,
                         ...)
{
    char buffer[160];
    va_list ap;
    int len;

    if (log_port == NULL || log_port->write == NULL || tag == NULL || fmt == NULL) {
        return;
    }

    va_start(ap, fmt);
    len = vsnprintf(buffer, sizeof(buffer), fmt, ap);
    va_end(ap);

    if (len < 0) {
        return;
    }

    if ((size_t)len >= sizeof(buffer)) {
        len = (int)(sizeof(buffer) - 1U);
        buffer[len] = '\0';
    }

    (void)log_port->write(log_port->ctx, level, tag, buffer, (size_t)len);
}

static uint32_t ev_time_mono_us_to_ms(ev_time_mono_us_t mono_now_us)
{
    return (uint32_t)((uint64_t)mono_now_us / 1000ULL);
}

void app_main(void)
{
    ev_clock_port_t clock_port;
    ev_log_port_t log_port;
    ev_reset_port_t reset_port;
    ev_uart_port_t uart_port;
    ev_uart_config_t uart_cfg;
    ev_reset_reason_t reset_reason;
    ev_time_mono_us_t mono_now_us;
    uint32_t heartbeat = 0U;
    const char banner[] = "[ev] uart adapter ready\r\n";
    size_t written = 0U;

    if (ev_esp8266_clock_port_init(&clock_port) != EV_OK) {
        return;
    }
    if (ev_esp8266_log_port_init(&log_port) != EV_OK) {
        return;
    }
    if (ev_esp8266_reset_port_init(&reset_port) != EV_OK) {
        return;
    }
    if (ev_esp8266_uart_port_init(&uart_port) != EV_OK) {
        return;
    }

    uart_cfg.baud_rate = 115200U;
    uart_cfg.data_bits = 8U;
    uart_cfg.stop_bits = 1U;
    uart_cfg.parity_enable = false;
    uart_cfg.parity_odd = false;

    (void)uart_port.init(uart_port.ctx, 0U, &uart_cfg);
    (void)uart_port.write(uart_port.ctx, 0U, banner, sizeof(banner) - 1U, &written);

    if (reset_port.get_reason(reset_port.ctx, &reset_reason) != EV_OK) {
        reset_reason = EV_RESET_REASON_UNKNOWN;
    }

    if (clock_port.mono_now_us(clock_port.ctx, &mono_now_us) != EV_OK) {
        mono_now_us = 0U;
    }

    ev_diag_logf(&log_port, EV_LOG_INFO, EV_BOARD_TAG, "framework boot");
    ev_diag_logf(&log_port, EV_LOG_INFO, EV_BOARD_TAG, "board profile: %s", EV_BOARD_NAME);
    ev_diag_logf(&log_port, EV_LOG_INFO, EV_BOARD_TAG, "reset reason: %s", ev_reset_reason_to_cstr(reset_reason));
    ev_diag_logf(&log_port, EV_LOG_INFO, EV_BOARD_TAG, "mono_now_ms=%" PRIu32, ev_time_mono_us_to_ms(mono_now_us));
    ev_diag_logf(&log_port, EV_LOG_INFO, EV_BOARD_TAG, "clock port contract size: %u", (unsigned)sizeof(ev_clock_port_t));
    (void)log_port.flush(log_port.ctx);

    for (;;) {
        if (clock_port.mono_now_us(clock_port.ctx, &mono_now_us) != EV_OK) {
            mono_now_us = 0U;
        }
        ev_diag_logf(&log_port,
                     EV_LOG_INFO,
                     EV_BOARD_TAG,
                     "heartbeat=%" PRIu32 " mono_now_ms=%" PRIu32,
                     heartbeat++,
                     ev_time_mono_us_to_ms(mono_now_us));
        (void)log_port.flush(log_port.ctx);
        (void)clock_port.delay_ms(clock_port.ctx, 1000U);
    }
}
