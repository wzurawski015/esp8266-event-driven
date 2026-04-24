#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"

#include "driver/gpio.h"
#include "esp_err.h"
#include "rom/ets_sys.h"

#include "ev/esp8266_port_adapters.h"

#define EV_ESP8266_ONEWIRE_RESET_LOW_US 480U
#define EV_ESP8266_ONEWIRE_PRESENCE_SAMPLE_US 70U
#define EV_ESP8266_ONEWIRE_RESET_RELEASE_US 410U
#define EV_ESP8266_ONEWIRE_WRITE_1_LOW_US 6U
#define EV_ESP8266_ONEWIRE_WRITE_1_RELEASE_US 64U
#define EV_ESP8266_ONEWIRE_WRITE_0_LOW_US 60U
#define EV_ESP8266_ONEWIRE_WRITE_0_RELEASE_US 10U
#define EV_ESP8266_ONEWIRE_READ_INIT_LOW_US 6U
#define EV_ESP8266_ONEWIRE_READ_SAMPLE_US 9U
#define EV_ESP8266_ONEWIRE_READ_RELEASE_US 55U

typedef struct ev_esp8266_onewire_adapter_ctx {
    int data_pin;
    bool configured;
} ev_esp8266_onewire_adapter_ctx_t;

static ev_esp8266_onewire_adapter_ctx_t g_ev_onewire0_ctx = {
    .data_pin = -1,
    .configured = false,
};

static bool ev_esp8266_onewire_pin_is_valid(int pin)
{
    return (pin >= 0) && (pin <= 16);
}

static void ev_esp8266_onewire_drive_low(const ev_esp8266_onewire_adapter_ctx_t *ctx)
{
    gpio_set_direction((gpio_num_t)ctx->data_pin, GPIO_MODE_OUTPUT_OD);
    gpio_set_level((gpio_num_t)ctx->data_pin, 0);
}

static void ev_esp8266_onewire_release_bus(const ev_esp8266_onewire_adapter_ctx_t *ctx)
{
    gpio_set_direction((gpio_num_t)ctx->data_pin, GPIO_MODE_DEF_INPUT);
}

static bool ev_esp8266_onewire_sample_bus(const ev_esp8266_onewire_adapter_ctx_t *ctx)
{
    return gpio_get_level((gpio_num_t)ctx->data_pin) != 0;
}

static bool ev_esp8266_onewire_ctx_is_valid(const ev_esp8266_onewire_adapter_ctx_t *ctx)
{
    return (ctx != NULL) && ctx->configured && ev_esp8266_onewire_pin_is_valid(ctx->data_pin);
}

static __attribute__((noinline)) ev_onewire_status_t ev_esp8266_onewire_bus_error_slowpath(void)
{
    return EV_ONEWIRE_ERR_BUS;
}

static __attribute__((noinline)) ev_onewire_status_t ev_esp8266_onewire_no_device_slowpath(void)
{
    return EV_ONEWIRE_ERR_NO_DEVICE;
}


static uint8_t ev_esp8266_onewire_bit_io(const ev_esp8266_onewire_adapter_ctx_t *ctx, uint8_t bit_value)
{
    uint8_t sampled = 0U;

    portENTER_CRITICAL();

    ev_esp8266_onewire_drive_low(ctx);
    ets_delay_us(1U);
    if (bit_value != 0U) {
        ev_esp8266_onewire_release_bus(ctx);
    }
    ets_delay_us(15U);
    sampled = ev_esp8266_onewire_sample_bus(ctx) ? 1U : 0U;
    ets_delay_us(45U);
    ev_esp8266_onewire_release_bus(ctx);

    portEXIT_CRITICAL();

    return sampled;
}

static ev_onewire_status_t ev_esp8266_onewire_reset(void *ctx)
{
    ev_esp8266_onewire_adapter_ctx_t *adapter = (ev_esp8266_onewire_adapter_ctx_t *)ctx;
    bool presence_high;
    bool stuck_low;

    if (!ev_esp8266_onewire_ctx_is_valid(adapter)) {
        return ev_esp8266_onewire_bus_error_slowpath();
    }

    portENTER_CRITICAL();

    ev_esp8266_onewire_drive_low(adapter);
    ets_delay_us(EV_ESP8266_ONEWIRE_RESET_LOW_US);
    ev_esp8266_onewire_release_bus(adapter);
    ets_delay_us(EV_ESP8266_ONEWIRE_PRESENCE_SAMPLE_US);
    presence_high = ev_esp8266_onewire_sample_bus(adapter);
    ets_delay_us(EV_ESP8266_ONEWIRE_RESET_RELEASE_US);
    stuck_low = !ev_esp8266_onewire_sample_bus(adapter);

    portEXIT_CRITICAL();

    if (stuck_low) {
        return ev_esp8266_onewire_bus_error_slowpath();
    }
    if (presence_high) {
        return ev_esp8266_onewire_no_device_slowpath();
    }

    return EV_ONEWIRE_OK;
}

static ev_onewire_status_t ev_esp8266_onewire_write_byte(void *ctx, uint8_t value)
{
    ev_esp8266_onewire_adapter_ctx_t *adapter = (ev_esp8266_onewire_adapter_ctx_t *)ctx;
    uint8_t bit_index;

    if (!ev_esp8266_onewire_ctx_is_valid(adapter)) {
        return ev_esp8266_onewire_bus_error_slowpath();
    }

    for (bit_index = 0U; bit_index < 8U; ++bit_index) {
        portENTER_CRITICAL();

        ev_esp8266_onewire_drive_low(adapter);
        if ((value & 0x01U) != 0U) {
            ets_delay_us(EV_ESP8266_ONEWIRE_WRITE_1_LOW_US);
            ev_esp8266_onewire_release_bus(adapter);
            ets_delay_us(EV_ESP8266_ONEWIRE_WRITE_1_RELEASE_US);
        } else {
            ets_delay_us(EV_ESP8266_ONEWIRE_WRITE_0_LOW_US);
            ev_esp8266_onewire_release_bus(adapter);
            ets_delay_us(EV_ESP8266_ONEWIRE_WRITE_0_RELEASE_US);
        }

        portEXIT_CRITICAL();

        value = (uint8_t)(value >> 1U);
    }

    return EV_ONEWIRE_OK;
}

static ev_onewire_status_t ev_esp8266_onewire_read_byte(void *ctx, uint8_t *out_value)
{
    ev_esp8266_onewire_adapter_ctx_t *adapter = (ev_esp8266_onewire_adapter_ctx_t *)ctx;
    uint8_t value = 0U;
    uint8_t bit_index;

    if (!ev_esp8266_onewire_ctx_is_valid(adapter) || (out_value == NULL)) {
        return ev_esp8266_onewire_bus_error_slowpath();
    }

    for (bit_index = 0U; bit_index < 8U; ++bit_index) {
        const uint8_t sampled = ev_esp8266_onewire_bit_io(adapter, 1U);
        value = (uint8_t)(value >> 1U);
        if (sampled != 0U) {
            value = (uint8_t)(value | 0x80U);
        }
    }

    *out_value = value;
    return EV_ONEWIRE_OK;
}

ev_result_t ev_esp8266_onewire_port_init(ev_onewire_port_t *out_port, int data_pin)
{
    gpio_config_t sdk_cfg = {0};
    esp_err_t sdk_rc;

    if ((out_port == NULL) || !ev_esp8266_onewire_pin_is_valid(data_pin)) {
        return EV_ERR_INVALID_ARG;
    }

    sdk_cfg.pin_bit_mask = (1UL << (unsigned)data_pin);
    sdk_cfg.mode = GPIO_MODE_OUTPUT_OD;
    sdk_cfg.pull_up_en = GPIO_PULLUP_DISABLE;
    sdk_cfg.intr_type = GPIO_INTR_DISABLE;

    sdk_rc = gpio_config(&sdk_cfg);
    if (sdk_rc != ESP_OK) {
        return EV_ERR_STATE;
    }

    gpio_set_level((gpio_num_t)data_pin, 1);

    g_ev_onewire0_ctx.data_pin = data_pin;
    g_ev_onewire0_ctx.configured = true;

    out_port->ctx = &g_ev_onewire0_ctx;
    out_port->reset = ev_esp8266_onewire_reset;
    out_port->write_byte = ev_esp8266_onewire_write_byte;
    out_port->read_byte = ev_esp8266_onewire_read_byte;

    return EV_OK;
}
