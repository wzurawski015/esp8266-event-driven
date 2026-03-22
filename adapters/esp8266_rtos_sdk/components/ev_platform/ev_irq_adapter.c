#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include "freertos/FreeRTOS.h"

#include "driver/gpio.h"
#include "esp_attr.h"
#include "esp_err.h"
#include "esp_system.h"
#include "esp_timer.h"
#include "esp8266/gpio_register.h"

#include "ev/esp8266_port_adapters.h"

#define EV_ESP8266_IRQ_MAX_LINES 4U
#define EV_ESP8266_IRQ_RING_CAPACITY 32U

typedef struct {
    ev_irq_line_id_t line_id;
    uint8_t gpio_num;
    ev_gpio_irq_trigger_t trigger;
    uint8_t last_level;
    bool configured;
} ev_esp8266_irq_line_t;

typedef struct {
    ev_irq_sample_t ring[EV_ESP8266_IRQ_RING_CAPACITY];
    ev_esp8266_irq_line_t lines[EV_ESP8266_IRQ_MAX_LINES];
    volatile uint8_t head;
    volatile uint8_t tail;
    volatile uint8_t count;
    size_t line_count;
    bool configured;
} ev_esp8266_irq_adapter_ctx_t;

static ev_esp8266_irq_adapter_ctx_t g_ev_irq_ctx;

static bool ev_esp8266_irq_gpio_is_valid(uint8_t gpio_num)
{
    return gpio_num <= 15U;
}

static bool ev_esp8266_irq_trigger_is_valid(ev_gpio_irq_trigger_t trigger)
{
    return (trigger == EV_GPIO_IRQ_TRIGGER_RISING) || (trigger == EV_GPIO_IRQ_TRIGGER_FALLING) ||
           (trigger == EV_GPIO_IRQ_TRIGGER_ANYEDGE);
}

static bool ev_esp8266_irq_pull_mode_is_valid(ev_gpio_irq_pull_mode_t pull_mode)
{
    return (pull_mode == EV_GPIO_IRQ_PULL_NONE) || (pull_mode == EV_GPIO_IRQ_PULL_UP);
}

static gpio_int_type_t ev_esp8266_gpio_intr_type_from_cfg(ev_gpio_irq_trigger_t trigger)
{
    switch (trigger) {
    case EV_GPIO_IRQ_TRIGGER_RISING:
        return GPIO_INTR_POSEDGE;
    case EV_GPIO_IRQ_TRIGGER_FALLING:
        return GPIO_INTR_NEGEDGE;
    case EV_GPIO_IRQ_TRIGGER_ANYEDGE:
    default:
        return GPIO_INTR_ANYEDGE;
    }
}

static ev_irq_edge_t ev_esp8266_irq_edge_from_sample(const ev_esp8266_irq_line_t *line, uint8_t level)
{
    if (line == NULL) {
        return EV_IRQ_EDGE_FALLING;
    }

    switch (line->trigger) {
    case EV_GPIO_IRQ_TRIGGER_RISING:
        return EV_IRQ_EDGE_RISING;
    case EV_GPIO_IRQ_TRIGGER_FALLING:
        return EV_IRQ_EDGE_FALLING;
    case EV_GPIO_IRQ_TRIGGER_ANYEDGE:
    default:
        if (level != line->last_level) {
            return (level != 0U) ? EV_IRQ_EDGE_RISING : EV_IRQ_EDGE_FALLING;
        }
        return (level != 0U) ? EV_IRQ_EDGE_RISING : EV_IRQ_EDGE_FALLING;
    }
}

static bool ev_esp8266_irq_line_configs_are_valid(const ev_gpio_irq_line_config_t *line_cfgs, size_t line_count)
{
    size_t i;
    size_t j;

    if ((line_cfgs == NULL) || (line_count == 0U) || (line_count > EV_ESP8266_IRQ_MAX_LINES)) {
        return false;
    }

    for (i = 0U; i < line_count; ++i) {
        if (!ev_esp8266_irq_gpio_is_valid(line_cfgs[i].gpio_num) ||
            !ev_esp8266_irq_trigger_is_valid(line_cfgs[i].trigger) ||
            !ev_esp8266_irq_pull_mode_is_valid(line_cfgs[i].pull_mode)) {
            return false;
        }

        for (j = i + 1U; j < line_count; ++j) {
            if ((line_cfgs[i].line_id == line_cfgs[j].line_id) || (line_cfgs[i].gpio_num == line_cfgs[j].gpio_num)) {
                return false;
            }
        }
    }

    return true;
}

static void ev_esp8266_irq_push_isr(ev_esp8266_irq_adapter_ctx_t *adapter, const ev_irq_sample_t *sample)
{
    uint8_t head;

    if ((adapter == NULL) || (sample == NULL)) {
        return;
    }

    /*
     * The producer already runs in GPIO ISR context on the single-core ESP8266.
     * Task-side consumers guard the shared ring with portENTER_CRITICAL(), so
     * no additional ISR-local interrupt masking is required here.
     */

    if (adapter->count < EV_ESP8266_IRQ_RING_CAPACITY) {
        head = adapter->head;
        adapter->ring[head] = *sample;
        adapter->head = (uint8_t)((head + 1U) % EV_ESP8266_IRQ_RING_CAPACITY);
        ++adapter->count;
    }

}

static void IRAM_ATTR ev_esp8266_irq_isr(void *arg)
{
    ev_esp8266_irq_adapter_ctx_t *adapter = (ev_esp8266_irq_adapter_ctx_t *)arg;
    const uint32_t status = (uint32_t)(GPIO_REG_READ(GPIO_STATUS_ADDRESS) & GPIO_STATUS_INTERRUPT_DATA_MASK);
    size_t i;

    if ((adapter == NULL) || (status == 0U)) {
        return;
    }

    GPIO_REG_WRITE(GPIO_STATUS_W1TC_ADDRESS, status);

    for (i = 0U; i < adapter->line_count; ++i) {
        ev_esp8266_irq_line_t *line = &adapter->lines[i];

        if (!line->configured) {
            continue;
        }

        if ((status & (1UL << line->gpio_num)) != 0U) {
            ev_irq_sample_t sample = {0};
            const uint8_t level = (uint8_t)((gpio_get_level((gpio_num_t)line->gpio_num) != 0) ? 1U : 0U);

            sample.line_id = line->line_id;
            sample.edge = ev_esp8266_irq_edge_from_sample(line, level);
            sample.level = level;
            sample.timestamp_us = (uint32_t)esp_timer_get_time();
            line->last_level = level;
            ev_esp8266_irq_push_isr(adapter, &sample);
        }
    }
}

static ev_result_t ev_esp8266_irq_pop(void *ctx, ev_irq_sample_t *out_sample)
{
    ev_esp8266_irq_adapter_ctx_t *adapter = (ev_esp8266_irq_adapter_ctx_t *)ctx;
    uint8_t tail;

    if ((adapter == NULL) || (out_sample == NULL)) {
        return EV_ERR_INVALID_ARG;
    }
    if (!adapter->configured) {
        return EV_ERR_STATE;
    }

    portENTER_CRITICAL();

    if (adapter->count == 0U) {
        portEXIT_CRITICAL();
        return EV_ERR_EMPTY;
    }

    tail = adapter->tail;
    *out_sample = adapter->ring[tail];
    adapter->tail = (uint8_t)((tail + 1U) % EV_ESP8266_IRQ_RING_CAPACITY);
    --adapter->count;

    portEXIT_CRITICAL();
    return EV_OK;
}

ev_result_t ev_esp8266_irq_port_init(ev_irq_port_t *out_port,
                                     const ev_gpio_irq_line_config_t *line_cfgs,
                                     size_t line_count)
{
    gpio_config_t sdk_cfg = {0};
    esp_err_t sdk_rc;
    size_t i;

    if ((out_port == NULL) || !ev_esp8266_irq_line_configs_are_valid(line_cfgs, line_count)) {
        return EV_ERR_INVALID_ARG;
    }

    memset(&g_ev_irq_ctx, 0, sizeof(g_ev_irq_ctx));

    for (i = 0U; i < line_count; ++i) {
        const ev_gpio_irq_line_config_t *src = &line_cfgs[i];
        ev_esp8266_irq_line_t *dst = &g_ev_irq_ctx.lines[i];

        sdk_cfg.pin_bit_mask = (1UL << src->gpio_num);
        sdk_cfg.mode = GPIO_MODE_INPUT;
        /* Shared open-drain interrupt sources must be able to return to
         * HIGH between falling edges. Keep the internal pull-up enabled
         * for GPIO-backed IRQ ingress lines on ESP8266. */
        sdk_cfg.pull_up_en = GPIO_PULLUP_ENABLE;
        sdk_cfg.pull_down_en = GPIO_PULLDOWN_DISABLE;
        sdk_cfg.intr_type = GPIO_INTR_DISABLE;

        sdk_rc = gpio_config(&sdk_cfg);
        if (sdk_rc != ESP_OK) {
            return EV_ERR_STATE;
        }

        dst->line_id = src->line_id;
        dst->gpio_num = src->gpio_num;
        dst->trigger = src->trigger;
        dst->last_level = (uint8_t)((gpio_get_level((gpio_num_t)src->gpio_num) != 0) ? 1U : 0U);
        dst->configured = true;
    }

    sdk_rc = gpio_isr_register(ev_esp8266_irq_isr, &g_ev_irq_ctx, 0, NULL);
    if (sdk_rc != ESP_OK) {
        return EV_ERR_STATE;
    }

    for (i = 0U; i < line_count; ++i) {
        const ev_esp8266_irq_line_t *line = &g_ev_irq_ctx.lines[i];

        sdk_rc = gpio_set_intr_type((gpio_num_t)line->gpio_num, ev_esp8266_gpio_intr_type_from_cfg(line->trigger));
        if (sdk_rc != ESP_OK) {
            return EV_ERR_STATE;
        }
    }

    g_ev_irq_ctx.line_count = line_count;
    g_ev_irq_ctx.configured = true;

    out_port->ctx = &g_ev_irq_ctx;
    out_port->pop = ev_esp8266_irq_pop;
    return EV_OK;
}
