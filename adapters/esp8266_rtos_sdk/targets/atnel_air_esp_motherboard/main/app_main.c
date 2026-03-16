#include <stdint.h>

#include "ev/esp8266_boot_diag.h"

#define EV_BOARD_TAG "ev_atnel"
#define EV_BOARD_NAME "atnel_air_esp_motherboard"

void app_main(void)
{
    static const ev_boot_diag_config_t k_boot_diag = {
        .board_tag = EV_BOARD_TAG,
        .board_name = EV_BOARD_NAME,
        .uart_port = 0U,
        .uart_baud_rate = 115200U,
        .heartbeat_period_ms = 1000U,
    };

    ev_esp8266_boot_diag_run(&k_boot_diag);
}
