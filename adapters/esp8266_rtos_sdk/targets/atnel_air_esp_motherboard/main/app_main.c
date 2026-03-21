#include <stddef.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"

#include "driver/uart.h"
#include "esp_log.h"

#include "ev/compiler.h"
#include "ev/esp8266_boot_diag.h"
#include "ev/esp8266_port_adapters.h"
#include "ev/esp8266_runtime_app.h"

#define EV_BOARD_TAG "ev_atnel"
#define EV_BOARD_NAME "atnel_air_esp_motherboard"
#define EV_BOARD_I2C_SCL_GPIO 4
#define EV_BOARD_I2C_SDA_GPIO 5
#define EV_BOARD_ONEWIRE_GPIO 12
#define EV_BOARD_IRQ_IR0_GPIO 13U
#define EV_BOARD_IRQ_INT0_GPIO 14U

EV_STATIC_ASSERT(EV_BOARD_ONEWIRE_GPIO != EV_BOARD_IRQ_INT0_GPIO,
                 "1-Wire and RTC/INT0 ingress must stay on distinct GPIOs");

static ev_i2c_port_t s_board_i2c_port;
static ev_irq_port_t s_board_irq_port;
static ev_onewire_port_t s_board_onewire_port;

static const ev_gpio_irq_line_config_t k_board_irq_lines[] = {
    {
        .line_id = 0U,
        .gpio_num = EV_BOARD_IRQ_INT0_GPIO,
        .trigger = EV_GPIO_IRQ_TRIGGER_FALLING,
        .pull_mode = EV_GPIO_IRQ_PULL_UP,
    },
    {
        .line_id = 1U,
        .gpio_num = EV_BOARD_IRQ_IR0_GPIO,
        .trigger = EV_GPIO_IRQ_TRIGGER_ANYEDGE,
        .pull_mode = EV_GPIO_IRQ_PULL_UP,
    },
};

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
    ev_irq_port_t *runtime_irq_port = NULL;
    ev_onewire_port_t *runtime_onewire_port = NULL;
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

    rc = ev_esp8266_onewire_port_init(&s_board_onewire_port, EV_BOARD_ONEWIRE_GPIO);
    if (rc != EV_OK) {
        ESP_LOGE(EV_BOARD_TAG, "onewire adapter init failed rc=%d", (int)rc);
    } else {
        runtime_onewire_port = &s_board_onewire_port;
    }

    rc = ev_esp8266_irq_port_init(&s_board_irq_port,
                                  k_board_irq_lines,
                                  (sizeof(k_board_irq_lines) / sizeof(k_board_irq_lines[0])));
    if (rc != EV_OK) {
        ESP_LOGE(EV_BOARD_TAG, "irq adapter init failed rc=%d", (int)rc);
    } else {
        runtime_irq_port = &s_board_irq_port;
    }

    ev_esp8266_runtime_app_run(&k_boot_diag, runtime_i2c_port, runtime_irq_port, runtime_onewire_port);
}
