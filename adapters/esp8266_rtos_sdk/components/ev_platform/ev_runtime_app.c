#include <stdbool.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#include "ev/demo_app.h"
#include "ev/esp8266_port_adapters.h"
#include "ev/esp8266_runtime_app.h"

#define EV_RUNTIME_APP_DEFAULT_BAUD_RATE 115200U
#define EV_RUNTIME_APP_DEFAULT_TICK_MS 1000U
#define EV_RUNTIME_APP_IDLE_DELAY_MS 10U

static void ev_runtime_app_logf(ev_log_port_t *log_port,
                                ev_log_level_t level,
                                const char *tag,
                                const char *fmt,
                                ...)
{
    char buffer[160];
    va_list ap;
    int len;

    if ((log_port == NULL) || (log_port->write == NULL) || (tag == NULL) || (fmt == NULL)) {
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

static bool ev_runtime_app_config_is_valid(const ev_boot_diag_config_t *cfg)
{
    return (cfg != NULL) && (cfg->board_tag != NULL) && (cfg->board_name != NULL);
}

void ev_esp8266_runtime_app_run(const ev_boot_diag_config_t *cfg)
{
    ev_clock_port_t clock_port;
    ev_log_port_t log_port;
    ev_reset_port_t reset_port;
    ev_uart_port_t uart_port;
    ev_uart_config_t uart_cfg;
    ev_reset_reason_t reset_reason;
    ev_demo_app_t app;
    ev_demo_app_config_t app_cfg;
    ev_result_t rc;

    if (!ev_runtime_app_config_is_valid(cfg)) {
        return;
    }

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

    uart_cfg.baud_rate = (cfg->uart_baud_rate == 0U) ? EV_RUNTIME_APP_DEFAULT_BAUD_RATE : cfg->uart_baud_rate;
    uart_cfg.data_bits = 8U;
    uart_cfg.stop_bits = 1U;
    uart_cfg.parity_enable = false;
    uart_cfg.parity_odd = false;

    if (uart_port.init(uart_port.ctx, cfg->uart_port, &uart_cfg) != EV_OK) {
        ev_runtime_app_logf(&log_port, EV_LOG_ERROR, cfg->board_tag, "uart adapter init failed");
        (void)log_port.flush(log_port.ctx);
        return;
    }

    if (reset_port.get_reason(reset_port.ctx, &reset_reason) != EV_OK) {
        reset_reason = EV_RESET_REASON_UNKNOWN;
    }

    ev_runtime_app_logf(&log_port, EV_LOG_INFO, cfg->board_tag, "uart adapter ready");
    ev_runtime_app_logf(&log_port, EV_LOG_INFO, cfg->board_tag, "framework event runtime boot");
    ev_runtime_app_logf(&log_port, EV_LOG_INFO, cfg->board_tag, "board profile: %s", cfg->board_name);
    ev_runtime_app_logf(
        &log_port,
        EV_LOG_INFO,
        cfg->board_tag,
        "reset reason: %s",
        ev_reset_reason_to_cstr(reset_reason));
    (void)log_port.flush(log_port.ctx);

    app_cfg.app_tag = cfg->board_tag;
    app_cfg.board_name = cfg->board_name;
    app_cfg.tick_period_ms = (cfg->heartbeat_period_ms == 0U) ? EV_RUNTIME_APP_DEFAULT_TICK_MS : cfg->heartbeat_period_ms;
    app_cfg.clock_port = &clock_port;
    app_cfg.log_port = &log_port;

    rc = ev_demo_app_init(&app, &app_cfg);
    if (rc == EV_OK) {
        rc = ev_demo_app_publish_boot(&app);
    }
    if (rc != EV_OK) {
        ev_runtime_app_logf(&log_port, EV_LOG_FATAL, cfg->board_tag, "demo runtime init failed rc=%d", (int)rc);
        (void)log_port.flush(log_port.ctx);
        (void)reset_port.restart(reset_port.ctx);
        return;
    }

    for (;;) {
        rc = ev_demo_app_poll(&app);
        if (rc != EV_OK) {
            ev_runtime_app_logf(&log_port, EV_LOG_FATAL, cfg->board_tag, "demo runtime poll failed rc=%d", (int)rc);
            (void)log_port.flush(log_port.ctx);
            (void)reset_port.restart(reset_port.ctx);
            return;
        }

        if (clock_port.delay_ms != NULL) {
            (void)clock_port.delay_ms(clock_port.ctx, EV_RUNTIME_APP_IDLE_DELAY_MS);
        }
    }
}
