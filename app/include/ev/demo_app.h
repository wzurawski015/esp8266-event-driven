#ifndef EV_DEMO_APP_H
#define EV_DEMO_APP_H

#include <stdbool.h>
#include <stdint.h>

#include "ev/actor_runtime.h"
#include "ev/domain_pump.h"
#include "ev/lease_pool.h"
#include "ev/port_clock.h"
#include "ev/port_log.h"
#include "ev/system_pump.h"

/* Wstrzykiwane kontrakty i Aktorzy dodani w Stage 2 */
#include "ev/port_i2c.h"
#include "ev/port_irq.h"
#include "ev/port_onewire.h"
#include "ev/ds18b20_actor.h"
#include "ev/mcp23008_actor.h"
#include "ev/oled_actor.h"
#include "ev/panel_actor.h"
#include "ev/rtc_actor.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EV_DEMO_APP_MAILBOX_CAPACITY 8U
#define EV_DEMO_APP_LEASE_SLOTS 4U
#define EV_DEMO_APP_SNAPSHOT_BYTES 16U

/**
 * @brief Immutable wiring required by the portable demo runtime.
 */
typedef struct {
    const char *app_tag;
    const char *board_name;
    uint32_t tick_period_ms;
    ev_clock_port_t *clock_port;
    ev_log_port_t *log_port;
    ev_irq_port_t *irq_port; /* Opcjonalny kontrakt wejścia IRQ dla przyszłych aktorów asynchronicznych */
    ev_i2c_port_t *i2c_port; /* Wstrzyknięty kontrakt magistrali I2C dla aktorów sprzętowych */
    ev_onewire_port_t *onewire_port; /* Wstrzyknięty kontrakt 1-Wire dla aktorów sprzętowych */
} ev_demo_app_config_t;

/**
 * @brief High-level counters emitted by the portable demo runtime.
 */
typedef struct {
    uint32_t boot_completions;
    uint32_t ticks_published;
    uint32_t diag_ticks_seen;
    uint32_t snapshots_published;
    uint32_t snapshots_received;
    uint32_t publish_errors;
    uint32_t pump_errors;
} ev_demo_app_stats_t;

typedef struct ev_demo_app ev_demo_app_t;

/**
 * @brief Actor-local APP state carried inside the demo runtime object.
 */
typedef struct {
    ev_demo_app_t *app;
    uint32_t last_snapshot_sequence;
    uint32_t last_diag_ticks_seen;
    ev_time_payload_t last_time;
    ev_temp_payload_t last_temp;
    bool time_valid;
    bool temp_valid;
    bool oled_frame_visible;
    bool screensaver_paused;
    uint8_t panel_led_mask;
    uint8_t current_page_offset;
    uint8_t current_column_offset;
    uint8_t last_page_offset;
    uint8_t last_column_offset;
    int8_t direction_x;
    int8_t direction_y;
} ev_demo_app_actor_state_t;

/**
 * @brief Actor-local DIAG state carried inside the demo runtime object.
 */
typedef struct {
    ev_demo_app_t *app;
    uint32_t ticks_seen;
    uint32_t snapshots_sent;
    uint32_t last_tick_ms;
} ev_demo_diag_actor_state_t;

/**
 * @brief Fully static portable event-driven demo runtime.
 */
struct ev_demo_app {
    ev_clock_port_t *clock_port;
    ev_log_port_t *log_port;
    const char *app_tag;
    const char *board_name;
    uint32_t tick_period_ms;
    uint32_t next_tick_ms;
    uint32_t next_tick_100ms_ms;
    bool boot_published;
    ev_irq_port_t *irq_port;

    ev_actor_registry_t registry;
    ev_mailbox_t app_mailbox;
    ev_mailbox_t diag_mailbox;
    ev_mailbox_t panel_mailbox; /* Skrzynka pocztowa dla logicznego panelu */
    ev_mailbox_t rtc_mailbox; /* Skrzynka pocztowa dla RTC */
    ev_mailbox_t mcp23008_mailbox; /* Skrzynka pocztowa dla MCP23008 */
    ev_mailbox_t ds18b20_mailbox; /* Skrzynka pocztowa dla DS18B20 */
    ev_mailbox_t oled_mailbox; /* Skrzynka pocztowa dla OLED */

    ev_msg_t app_storage[EV_DEMO_APP_MAILBOX_CAPACITY];
    ev_msg_t diag_storage[EV_DEMO_APP_MAILBOX_CAPACITY];
    ev_msg_t panel_storage[EV_DEMO_APP_MAILBOX_CAPACITY]; /* Bufor wiadomości dla logicznego panelu */
    ev_msg_t rtc_storage[EV_DEMO_APP_MAILBOX_CAPACITY]; /* Bufor wiadomości dla RTC */
    ev_msg_t mcp23008_storage[EV_DEMO_APP_MAILBOX_CAPACITY]; /* Bufor wiadomości dla MCP23008 */
    ev_msg_t ds18b20_storage[EV_DEMO_APP_MAILBOX_CAPACITY]; /* Bufor wiadomości dla DS18B20 */
    ev_msg_t oled_storage[EV_DEMO_APP_MAILBOX_CAPACITY]; /* Bufor wiadomości dla OLED */

    ev_actor_runtime_t app_runtime;
    ev_actor_runtime_t diag_runtime;
    ev_actor_runtime_t panel_runtime; /* Wątek logiczny Aktora Panelu */
    ev_actor_runtime_t rtc_runtime; /* Wątek logiczny Aktora RTC */
    ev_actor_runtime_t mcp23008_runtime; /* Wątek logiczny Aktora MCP23008 */
    ev_actor_runtime_t ds18b20_runtime; /* Wątek logiczny Aktora DS18B20 */
    ev_actor_runtime_t oled_runtime; /* Wątek logiczny Aktora OLED */

    ev_domain_pump_t fast_domain;
    ev_domain_pump_t slow_domain;
    ev_system_pump_t system_pump;

    ev_lease_pool_t lease_pool;
    ev_lease_slot_t lease_slots[EV_DEMO_APP_LEASE_SLOTS];
    unsigned char lease_storage[EV_DEMO_APP_LEASE_SLOTS * EV_DEMO_APP_SNAPSHOT_BYTES];

    ev_demo_app_actor_state_t app_actor;
    ev_demo_diag_actor_state_t diag_actor;
    ev_panel_actor_ctx_t panel_ctx; /* Stan logicznego Aktora Panelu */
    ev_rtc_actor_ctx_t rtc_ctx; /* Fizyczny stan i konfiguracja Aktora RTC */
    ev_mcp23008_actor_ctx_t mcp23008_ctx; /* Fizyczny stan i konfiguracja Aktora MCP23008 */
    ev_ds18b20_actor_ctx_t ds18b20_ctx; /* Fizyczny stan i konfiguracja Aktora DS18B20 */
    ev_oled_actor_ctx_t oled_ctx; /* Fizyczny stan i bufor ekranu OLED */

    ev_demo_app_stats_t stats;
};

/**
 * @brief Initialize the portable event-driven demo runtime.
 *
 * @param app Runtime object to initialize.
 * @param cfg Immutable runtime wiring.
 * @return EV_OK on success or an error code.
 */
ev_result_t ev_demo_app_init(ev_demo_app_t *app, const ev_demo_app_config_t *cfg);

/**
 * @brief Publish the startup boot events into the runtime.
 *
 * This is intentionally separated from init so tests and targets can control
 * when the first work enters the cooperative pump.
 *
 * @param app Runtime object.
 * @return EV_OK on success or an error code.
 */
ev_result_t ev_demo_app_publish_boot(ev_demo_app_t *app);

/**
 * @brief Run one non-blocking poll iteration.
 *
 * The poll drains all currently pending work, publishes any due periodic tick
 * events, and then drains the resulting work again.
 *
 * @param app Runtime object.
 * @return EV_OK on success or an error code.
 */
ev_result_t ev_demo_app_poll(ev_demo_app_t *app);

/**
 * @brief Return the currently pending message count across all bound domains.
 *
 * @param app Runtime object.
 * @return Pending message count.
 */
size_t ev_demo_app_pending(const ev_demo_app_t *app);

/**
 * @brief Return a stable pointer to high-level demo counters.
 *
 * @param app Runtime object.
 * @return Pointer to counters or NULL when @p app is NULL.
 */
const ev_demo_app_stats_t *ev_demo_app_stats(const ev_demo_app_t *app);

/**
 * @brief Return a stable pointer to the underlying system-pump counters.
 *
 * @param app Runtime object.
 * @return Pointer to counters or NULL when @p app is NULL.
 */
const ev_system_pump_stats_t *ev_demo_app_system_pump_stats(const ev_demo_app_t *app);

#ifdef __cplusplus
}
#endif

#endif /* EV_DEMO_APP_H */

