# bsp

Board support policy and per-board pin maps.
Framework code must not hardcode board-level GPIO assignments outside this boundary.

Stage 2A1 begins by freezing the BSP boundary and adding a first non-production
ESP8266 board placeholder under `bsp/esp8266_generic_dev/`.


Stage 2A3 adds the first concrete board profile under `bsp/atnel_air_esp_motherboard/` for the ATNEL AIR ESP module mounted on ATB WIFI ESP Motherboard.
