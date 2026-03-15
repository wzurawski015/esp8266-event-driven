#include <inttypes.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

#include "ev/port_clock.h"

static const char *TAG = "ev_stage2a2";

void app_main(void)
{
    uint32_t heartbeat = 0U;

    ESP_LOGI(TAG, "esp8266-event-driven target skeleton boot");
    ESP_LOGI(TAG, "board profile: esp8266_generic_dev");
    ESP_LOGI(TAG, "clock port contract size: %u", (unsigned)sizeof(ev_clock_port_t));

    for (;;) {
        ESP_LOGI(TAG, "heartbeat=%" PRIu32, heartbeat++);
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}
