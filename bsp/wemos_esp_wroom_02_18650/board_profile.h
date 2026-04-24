#ifndef EV_WEMOS_ESP_WROOM_02_18650_BOARD_PROFILE_H
#define EV_WEMOS_ESP_WROOM_02_18650_BOARD_PROFILE_H

#include "ev/compiler.h"

#define EV_BOARD_PROFILE_TAG "ev_wroom02"
#define EV_BOARD_PROFILE_NAME "wemos_esp_wroom_02_18650"

enum {
#define EV_BSP_PIN(name, gpio, desc) EV_BOARD_##name = (gpio),
#define EV_BSP_PIN_ANALOG(name, desc)
#include "pins.def"
#undef EV_BSP_PIN
#undef EV_BSP_PIN_ANALOG
};

#endif /* EV_WEMOS_ESP_WROOM_02_18650_BOARD_PROFILE_H */
