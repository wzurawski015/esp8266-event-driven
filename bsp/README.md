# bsp

Board support policy and per-board pin maps.
Framework code must not hardcode board-level GPIO assignments outside this boundary.

Stage 2A1 begins by freezing the BSP boundary and adding a first non-production
ESP8266 board placeholder under `bsp/esp8266_generic_dev/`.


Stage 2A3 adds the first concrete board profile under `bsp/atnel_air_esp_motherboard/` for the ATNEL AIR ESP module mounted on ATB WIFI ESP Motherboard.

All concrete board profiles must use the unified `EV_BSP_PIN(...)` / `EV_BSP_PIN_ANALOG(...)`
DSL so that targets can consume one board-scoped source of truth without per-board macro dialects.


All production targets must consume board-scoped constants through a sibling `board_profile.h`
that expands the unified DSL into target-facing `EV_BOARD_*` definitions instead of duplicating GPIO
or device address macros inside target-local `app_main.c` files.

- `bsp/wemos_esp_wroom_02_18650/` - Wemos ESP-WROOM-02 with integrated 18650 battery holder.


## Runtime SSoT rule

Production targets must derive `ev_demo_app_board_profile_t` from their
`board_profile.h`. Device addresses, hardware-present masks, and supervisor
required/optional masks belong to the BSP, not to `app/` defaults.

A target with no declared runtime hardware may pass a no-hardware profile and
still run the portable app in degraded mode; a target that declares RTC,
MCP23008, OLED, or DS18B20 must provide the matching port contracts.
