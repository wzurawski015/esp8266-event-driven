#include <stdint.h>

#include "freertos/FreeRTOS.h"

#include "driver/uart.h"
#include "esp_log.h"

#include "ev/esp8266_boot_diag.h"
#include "ev/esp8266_port_adapters.h"
#include "ev/esp8266_runtime_app.h"

#define EV_BOARD_TAG "ev_atnel"
#define EV_BOARD_NAME "atnel_air_esp_motherboard"
#define EV_BOARD_I2C_SCL_GPIO 4
#define EV_BOARD_I2C_SDA_GPIO 5

static ev_i2c_port_t s_board_i2c_port;

void app_main(void)
{
    static const ev_boot_diag_config_t k_boot_diag = {
        .board_tag = EV_BOARD_TAG,
        .board_name = EV_BOARD_NAME,
        .uart_port = 0U,
        .uart_baud_rate = 115200U,
        .heartbeat_period_ms = 1000U,
    };
    ev_i2c_port_t *runtime_i2c_port = NULL;
    ev_result_t rc;

    (void)uart_set_baudrate(UART_NUM_0, 115200U);

    rc = ev_esp8266_i2c_port_init(&s_board_i2c_port, EV_BOARD_I2C_SDA_GPIO, EV_BOARD_I2C_SCL_GPIO);
    if (rc != EV_OK) {
        ESP_LOGE(EV_BOARD_TAG, "i2c adapter init failed rc=%d", (int)rc);
    } else {
        runtime_i2c_port = &s_board_i2c_port;
        rc = ev_i2c_scan(EV_I2C_PORT_NUM_0);
        if (rc != EV_OK) {
            ESP_LOGE(EV_BOARD_TAG, "i2c self-test scan failed rc=%d", (int)rc);
        }
    }

    ev_esp8266_runtime_app_run(&k_boot_diag, runtime_i2c_port);
}
