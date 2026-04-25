#ifndef EV_WEMOS_ESP_WROOM_02_18650_BOARD_PROFILE_H
#define EV_WEMOS_ESP_WROOM_02_18650_BOARD_PROFILE_H

#include "ev/compiler.h"

#define EV_BOARD_PROFILE_TAG "ev_wroom02"
#define EV_BOARD_PROFILE_NAME "wemos_esp_wroom_02_18650"
#define EV_BOARD_HAS_I2C0 1U
#define EV_BOARD_HAS_ONEWIRE0 0U
#define EV_BOARD_HAS_GPIO_IRQ 0U
#define EV_BOARD_HAS_DEEP_SLEEP_WAKE_GPIO16 1U

enum {
#define EV_BSP_PIN(name, gpio, desc) EV_BOARD_##name = (gpio),
#define EV_BSP_PIN_ANALOG(name, desc)
#include "pins.def"
#undef EV_BSP_PIN
#undef EV_BSP_PIN_ANALOG
};

#define EV_BOARD_WAKE_GPIO EV_BOARD_PIN_STATUS_LED

#endif /* EV_WEMOS_ESP_WROOM_02_18650_BOARD_PROFILE_H */
