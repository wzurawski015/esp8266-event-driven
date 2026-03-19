#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "driver/i2c.h"
#include "esp_err.h"
#include "esp_log.h"

#include "ev/esp8266_port_adapters.h"

#define EV_ESP8266_I2C_PORT_NUM I2C_NUM_0
#define EV_ESP8266_I2C_CLK_STRETCH_TICK 300U

static const char *const k_ev_i2c_tag = "ev_i2c";

typedef struct ev_esp8266_i2c_adapter_ctx {
    ev_i2c_port_num_t port_num;
    int sda_pin;
    int scl_pin;
    bool configured;
} ev_esp8266_i2c_adapter_ctx_t;

static SemaphoreHandle_t g_ev_i2c_bus_mutex = NULL;
static ev_esp8266_i2c_adapter_ctx_t g_ev_i2c0_ctx = {
    .port_num = EV_I2C_PORT_NUM_0,
    .sda_pin = -1,
    .scl_pin = -1,
    .configured = false,
};
static bool g_ev_i2c_driver_ready = false;

static bool ev_esp8266_i2c_pin_is_valid(int pin)
{
    return (pin >= 0) && (pin <= 16);
}

static bool ev_esp8266_i2c_txn_is_valid(const ev_esp8266_i2c_adapter_ctx_t *ctx,
                                        ev_i2c_port_num_t port_num,
                                        uint8_t device_address_7bit)
{
    return (ctx != NULL) && ctx->configured && (ctx->port_num == port_num) && (device_address_7bit <= 0x7FU);
}

static ev_i2c_status_t ev_esp8266_i2c_status_from_esp_err(esp_err_t sdk_rc)
{
    switch (sdk_rc) {
    case ESP_OK:
        return EV_I2C_OK;
    case ESP_ERR_TIMEOUT:
        return EV_I2C_ERR_TIMEOUT;
    case ESP_FAIL:
        return EV_I2C_ERR_NACK;
    case ESP_ERR_INVALID_ARG:
    case ESP_ERR_INVALID_STATE:
    default:
        return EV_I2C_ERR_BUS_LOCKED;
    }
}

static ev_i2c_status_t ev_esp8266_i2c_take_bus(void)
{
    if (g_ev_i2c_bus_mutex == NULL) {
        return EV_I2C_ERR_BUS_LOCKED;
    }

    if (xSemaphoreTake(g_ev_i2c_bus_mutex, pdMS_TO_TICKS(15)) != pdTRUE) {
        return EV_I2C_ERR_BUS_LOCKED;
    }

    return EV_I2C_OK;
}

static void ev_esp8266_i2c_give_bus(void)
{
    if (g_ev_i2c_bus_mutex != NULL) {
        (void)xSemaphoreGive(g_ev_i2c_bus_mutex);
    }
}

static ev_i2c_status_t ev_esp8266_i2c_write_stream(void *ctx,
                                                    ev_i2c_port_num_t port_num,
                                                    uint8_t device_address_7bit,
                                                    const uint8_t *data,
                                                    size_t data_len)
{
    ev_esp8266_i2c_adapter_ctx_t *adapter = (ev_esp8266_i2c_adapter_ctx_t *)ctx;
    i2c_cmd_handle_t cmd = NULL;
    ev_i2c_status_t status;
    esp_err_t sdk_rc = ESP_FAIL;

    if (!ev_esp8266_i2c_txn_is_valid(adapter, port_num, device_address_7bit)) {
        return EV_I2C_ERR_BUS_LOCKED;
    }
    if ((data_len > 0U) && (data == NULL)) {
        return EV_I2C_ERR_BUS_LOCKED;
    }

    status = ev_esp8266_i2c_take_bus();
    if (status != EV_I2C_OK) {
        return status;
    }

    cmd = i2c_cmd_link_create();
    if (cmd == NULL) {
        status = EV_I2C_ERR_BUS_LOCKED;
        goto cleanup;
    }

    sdk_rc = i2c_master_start(cmd);
    if (sdk_rc == ESP_OK) {
        sdk_rc = i2c_master_write_byte(cmd, (uint8_t)((device_address_7bit << 1U) | I2C_MASTER_WRITE), true);
    }
    if ((sdk_rc == ESP_OK) && (data_len > 0U)) {
        sdk_rc = i2c_master_write(cmd, (uint8_t *)data, data_len, true);
    }
    if (sdk_rc == ESP_OK) {
        sdk_rc = i2c_master_stop(cmd);
    }
    if (sdk_rc == ESP_OK) {
        sdk_rc = i2c_master_cmd_begin(EV_ESP8266_I2C_PORT_NUM, cmd, pdMS_TO_TICKS(15));
    }

    status = ev_esp8266_i2c_status_from_esp_err(sdk_rc);

cleanup:
    if (cmd != NULL) {
        i2c_cmd_link_delete(cmd);
    }
    ev_esp8266_i2c_give_bus();
    return status;
}

static ev_i2c_status_t ev_esp8266_i2c_write_regs(void *ctx,
                                                  ev_i2c_port_num_t port_num,
                                                  uint8_t device_address_7bit,
                                                  uint8_t first_reg,
                                                  const uint8_t *data,
                                                  size_t data_len)
{
    ev_esp8266_i2c_adapter_ctx_t *adapter = (ev_esp8266_i2c_adapter_ctx_t *)ctx;
    i2c_cmd_handle_t cmd = NULL;
    ev_i2c_status_t status;
    esp_err_t sdk_rc = ESP_FAIL;

    if (!ev_esp8266_i2c_txn_is_valid(adapter, port_num, device_address_7bit)) {
        return EV_I2C_ERR_BUS_LOCKED;
    }
    if ((data_len > 0U) && (data == NULL)) {
        return EV_I2C_ERR_BUS_LOCKED;
    }

    status = ev_esp8266_i2c_take_bus();
    if (status != EV_I2C_OK) {
        return status;
    }

    cmd = i2c_cmd_link_create();
    if (cmd == NULL) {
        status = EV_I2C_ERR_BUS_LOCKED;
        goto cleanup;
    }

    sdk_rc = i2c_master_start(cmd);
    if (sdk_rc == ESP_OK) {
        sdk_rc = i2c_master_write_byte(cmd, (uint8_t)((device_address_7bit << 1U) | I2C_MASTER_WRITE), true);
    }
    if (sdk_rc == ESP_OK) {
        sdk_rc = i2c_master_write_byte(cmd, first_reg, true);
    }
    if ((sdk_rc == ESP_OK) && (data_len > 0U)) {
        sdk_rc = i2c_master_write(cmd, (uint8_t *)data, data_len, true);
    }
    if (sdk_rc == ESP_OK) {
        sdk_rc = i2c_master_stop(cmd);
    }
    if (sdk_rc == ESP_OK) {
        sdk_rc = i2c_master_cmd_begin(EV_ESP8266_I2C_PORT_NUM, cmd, pdMS_TO_TICKS(15));
    }

    status = ev_esp8266_i2c_status_from_esp_err(sdk_rc);

cleanup:
    if (cmd != NULL) {
        i2c_cmd_link_delete(cmd);
    }
    ev_esp8266_i2c_give_bus();
    return status;
}

static ev_i2c_status_t ev_esp8266_i2c_read_regs(void *ctx,
                                                 ev_i2c_port_num_t port_num,
                                                 uint8_t device_address_7bit,
                                                 uint8_t first_reg,
                                                 uint8_t *data,
                                                 size_t data_len)
{
    ev_esp8266_i2c_adapter_ctx_t *adapter = (ev_esp8266_i2c_adapter_ctx_t *)ctx;
    i2c_cmd_handle_t cmd = NULL;
    ev_i2c_status_t status;
    esp_err_t sdk_rc = ESP_FAIL;

    if (!ev_esp8266_i2c_txn_is_valid(adapter, port_num, device_address_7bit)) {
        return EV_I2C_ERR_BUS_LOCKED;
    }
    if ((data == NULL) || (data_len == 0U)) {
        return EV_I2C_ERR_BUS_LOCKED;
    }

    status = ev_esp8266_i2c_take_bus();
    if (status != EV_I2C_OK) {
        return status;
    }

    cmd = i2c_cmd_link_create();
    if (cmd == NULL) {
        status = EV_I2C_ERR_BUS_LOCKED;
        goto cleanup;
    }

    sdk_rc = i2c_master_start(cmd);
    if (sdk_rc == ESP_OK) {
        sdk_rc = i2c_master_write_byte(cmd, (uint8_t)((device_address_7bit << 1U) | I2C_MASTER_WRITE), true);
    }
    if (sdk_rc == ESP_OK) {
        sdk_rc = i2c_master_write_byte(cmd, first_reg, true);
    }
    if (sdk_rc == ESP_OK) {
        sdk_rc = i2c_master_start(cmd);
    }
    if (sdk_rc == ESP_OK) {
        sdk_rc = i2c_master_write_byte(cmd, (uint8_t)((device_address_7bit << 1U) | I2C_MASTER_READ), true);
    }
    if ((sdk_rc == ESP_OK) && (data_len > 1U)) {
        sdk_rc = i2c_master_read(cmd, data, data_len - 1U, I2C_MASTER_ACK);
    }
    if (sdk_rc == ESP_OK) {
        sdk_rc = i2c_master_read_byte(cmd, &data[data_len - 1U], I2C_MASTER_NACK);
    }
    if (sdk_rc == ESP_OK) {
        sdk_rc = i2c_master_stop(cmd);
    }
    if (sdk_rc == ESP_OK) {
        sdk_rc = i2c_master_cmd_begin(EV_ESP8266_I2C_PORT_NUM, cmd, pdMS_TO_TICKS(15));
    }

    status = ev_esp8266_i2c_status_from_esp_err(sdk_rc);

cleanup:
    if (cmd != NULL) {
        i2c_cmd_link_delete(cmd);
    }
    ev_esp8266_i2c_give_bus();
    return status;
}

static const char *ev_esp8266_i2c_known_device_name(uint8_t device_address_7bit)
{
    if (device_address_7bit == 0x68U) {
        return "rtc";
    }
    if ((device_address_7bit >= 0x20U) && (device_address_7bit <= 0x27U)) {
        return "mcp23008";
    }
    if ((device_address_7bit == 0x3CU) || (device_address_7bit == 0x3DU)) {
        return "oled";
    }
    return NULL;
}

static void ev_esp8266_i2c_scan(const ev_i2c_port_t *port)
{
    bool found_any = false;
    uint8_t device_address_7bit;

    if ((port == NULL) || (port->write_stream == NULL)) {
        return;
    }

    ESP_LOGI(k_ev_i2c_tag, "i2c-scan: start on SDA=%d SCL=%d", g_ev_i2c0_ctx.sda_pin, g_ev_i2c0_ctx.scl_pin);

    for (device_address_7bit = 0x03U; device_address_7bit < 0x78U; ++device_address_7bit) {
        ev_i2c_status_t status = port->write_stream(port->ctx,
                                                    EV_I2C_PORT_NUM_0,
                                                    device_address_7bit,
                                                    NULL,
                                                    0U);
        if (status == EV_I2C_OK) {
            const char *device_name = ev_esp8266_i2c_known_device_name(device_address_7bit);
            found_any = true;
            if (device_name != NULL) {
                ESP_LOGI(k_ev_i2c_tag,
                         "i2c-scan: found 0x%02X (%s)",
                         (unsigned)device_address_7bit,
                         device_name);
            } else {
                ESP_LOGI(k_ev_i2c_tag, "i2c-scan: found 0x%02X", (unsigned)device_address_7bit);
            }
        }
    }

    if (!found_any) {
        ESP_LOGW(k_ev_i2c_tag, "i2c-scan: no devices acknowledged");
    }
}

static void ev_esp8266_i2c_probe_known_devices(const ev_i2c_port_t *port)
{
    ev_i2c_status_t status;
    bool mcp_found = false;
    uint8_t device_address_7bit;

    if ((port == NULL) || (port->write_stream == NULL)) {
        return;
    }

    status = port->write_stream(port->ctx, EV_I2C_PORT_NUM_0, 0x68U, NULL, 0U);
    if (status == EV_I2C_OK) {
        ESP_LOGI(k_ev_i2c_tag, "rtc-probe: detected device at 0x68");
    } else {
        ESP_LOGW(k_ev_i2c_tag, "rtc-probe: no response at 0x68 status=%d", (int)status);
    }

    for (device_address_7bit = 0x20U; device_address_7bit <= 0x27U; ++device_address_7bit) {
        status = port->write_stream(port->ctx, EV_I2C_PORT_NUM_0, device_address_7bit, NULL, 0U);
        if (status == EV_I2C_OK) {
            ESP_LOGI(k_ev_i2c_tag, "mcp23008-probe: detected device at 0x%02X", (unsigned)device_address_7bit);
            mcp_found = true;
        }
    }
    if (!mcp_found) {
        ESP_LOGW(k_ev_i2c_tag, "mcp23008-probe: no response in range 0x20-0x27");
    }

    status = port->write_stream(port->ctx, EV_I2C_PORT_NUM_0, 0x3CU, NULL, 0U);
    if (status == EV_I2C_OK) {
        ESP_LOGI(k_ev_i2c_tag, "oled-probe: detected device at 0x3C");
    } else {
        status = port->write_stream(port->ctx, EV_I2C_PORT_NUM_0, 0x3DU, NULL, 0U);
        if (status == EV_I2C_OK) {
            ESP_LOGI(k_ev_i2c_tag, "oled-probe: detected device at 0x3D");
        } else {
            ESP_LOGI(k_ev_i2c_tag, "oled-probe: optional OLED not detected");
        }
    }
}

ev_result_t ev_esp8266_i2c_port_init(ev_i2c_port_t *out_port, int sda_pin, int scl_pin)
{
    i2c_config_t sdk_cfg;
    esp_err_t sdk_rc;

    if ((out_port == NULL) || !ev_esp8266_i2c_pin_is_valid(sda_pin) || !ev_esp8266_i2c_pin_is_valid(scl_pin) ||
        (sda_pin == scl_pin)) {
        return EV_ERR_INVALID_ARG;
    }

    if (g_ev_i2c_bus_mutex == NULL) {
        g_ev_i2c_bus_mutex = xSemaphoreCreateMutex();
        if (g_ev_i2c_bus_mutex == NULL) {
            ESP_LOGE(k_ev_i2c_tag, "bus mutex allocation failed");
            return EV_ERR_STATE;
        }
    }

    if (!g_ev_i2c_driver_ready) {
        sdk_rc = i2c_driver_install(EV_ESP8266_I2C_PORT_NUM, I2C_MODE_MASTER);
        if ((sdk_rc != ESP_OK) && (sdk_rc != ESP_ERR_INVALID_STATE)) {
            ESP_LOGE(k_ev_i2c_tag, "i2c_driver_install failed rc=%d", (int)sdk_rc);
            return EV_ERR_STATE;
        }
        g_ev_i2c_driver_ready = true;
    }

    sdk_cfg.mode = I2C_MODE_MASTER;
    sdk_cfg.sda_io_num = sda_pin;
    sdk_cfg.sda_pullup_en = GPIO_PULLUP_ENABLE;
    sdk_cfg.scl_io_num = scl_pin;
    sdk_cfg.scl_pullup_en = GPIO_PULLUP_ENABLE;
    sdk_cfg.clk_stretch_tick = EV_ESP8266_I2C_CLK_STRETCH_TICK;

    sdk_rc = i2c_param_config(EV_ESP8266_I2C_PORT_NUM, &sdk_cfg);
    if (sdk_rc != ESP_OK) {
        ESP_LOGE(k_ev_i2c_tag, "i2c_param_config failed rc=%d", (int)sdk_rc);
        return EV_ERR_STATE;
    }

    g_ev_i2c0_ctx.port_num = EV_I2C_PORT_NUM_0;
    g_ev_i2c0_ctx.sda_pin = sda_pin;
    g_ev_i2c0_ctx.scl_pin = scl_pin;
    g_ev_i2c0_ctx.configured = true;

    out_port->ctx = &g_ev_i2c0_ctx;
    out_port->write_stream = ev_esp8266_i2c_write_stream;
    out_port->write_regs = ev_esp8266_i2c_write_regs;
    out_port->read_regs = ev_esp8266_i2c_read_regs;

    ev_esp8266_i2c_scan(out_port);
    ev_esp8266_i2c_probe_known_devices(out_port);
    return EV_OK;
}
