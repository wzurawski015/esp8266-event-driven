#ifndef EV_ESP8266_RUNTIME_APP_H
#define EV_ESP8266_RUNTIME_APP_H

#include "ev/esp8266_boot_diag.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Run the first real ESP8266 event-driven firmware loop.
 *
 * The ATNEL board target uses this helper to exercise the framework core,
 * static routing, deterministic lease payloads, and the cooperative system
 * pump on hardware instead of staying on the boot/diagnostic heartbeat path.
 *
 * @param cfg Immutable board/runtime configuration.
 */
void ev_esp8266_runtime_app_run(const ev_boot_diag_config_t *cfg);

#ifdef __cplusplus
}
#endif

#endif /* EV_ESP8266_RUNTIME_APP_H */
