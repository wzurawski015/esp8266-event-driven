#ifndef EV_ESP8266_GENERIC_DEV_BOARD_PROFILE_H
#define EV_ESP8266_GENERIC_DEV_BOARD_PROFILE_H

#include "ev/compiler.h"

#define EV_BOARD_PROFILE_TAG "ev_generic"
#define EV_BOARD_PROFILE_NAME "esp8266_generic_dev"
#define EV_BOARD_HAS_I2C0 0U
#define EV_BOARD_HAS_ONEWIRE0 0U
#define EV_BOARD_HAS_GPIO_IRQ 0U
#define EV_BOARD_HAS_DEEP_SLEEP_WAKE_GPIO16 0U

enum {
#define EV_BSP_PIN(name, gpio, desc) EV_BOARD_##name = (gpio),
#define EV_BSP_PIN_ANALOG(name, desc)
#include "pins.def"
#undef EV_BSP_PIN
#undef EV_BSP_PIN_ANALOG
};

#endif /* EV_ESP8266_GENERIC_DEV_BOARD_PROFILE_H */
