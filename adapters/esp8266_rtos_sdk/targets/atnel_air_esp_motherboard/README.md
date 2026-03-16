# atnel_air_esp_motherboard target

This target is the first board-scoped firmware image for the ATNEL AIR ESP module
mounted on the ATB WIFI ESP Motherboard.

Current scope:

- boot and diagnostics over UART0 / FT231X,
- validation of the first concrete ESP8266 RTOS SDK port adapters,
- validation of the shared boot/diagnostic harness also used by the generic target,
- board identification and reset-reason reporting,
- monotonic-time heartbeat through the public clock port.

Canonical commands from the repository root:

```bash
FW_SDK_PROJECT_DIR=adapters/esp8266_rtos_sdk/targets/atnel_air_esp_motherboard ./tools/fw sdk-defconfig
FW_SDK_PROJECT_DIR=adapters/esp8266_rtos_sdk/targets/atnel_air_esp_motherboard ./tools/fw sdk-build
FW_SDK_PROJECT_DIR=adapters/esp8266_rtos_sdk/targets/atnel_air_esp_motherboard ./tools/fw sdk-clean-target
FW_SDK_PROJECT_DIR=adapters/esp8266_rtos_sdk/targets/atnel_air_esp_motherboard ./tools/fw sdk-distclean
FW_SDK_PROJECT_DIR=adapters/esp8266_rtos_sdk/targets/atnel_air_esp_motherboard ./tools/fw sdk-build

FW_SDK_PROJECT_DIR=adapters/esp8266_rtos_sdk/targets/atnel_air_esp_motherboard FW_ESPPORT=/dev/ttyUSB0 ./tools/fw sdk-flash
FW_SDK_PROJECT_DIR=adapters/esp8266_rtos_sdk/targets/atnel_air_esp_motherboard FW_ESPPORT=/dev/ttyUSB0 ./tools/fw sdk-flash-manual
FW_SDK_PROJECT_DIR=adapters/esp8266_rtos_sdk/targets/atnel_air_esp_motherboard FW_ESPPORT=/dev/ttyUSB0 FW_MONITOR_BAUD=115200 ./tools/fw sdk-simple-monitor
```

Recommended early jumper baseline:

- JP2 open
- JP4 open
- JP14 open
- JP16 open
- JP19 open
- JP1 open

`115200` is the expected runtime baud for the current target image.
If `sdk-monitor` is unstable under Docker or WSL2, use `sdk-simple-monitor` as the canonical fallback.
If `sdk-flash` fails with a DTR/RTS I/O error under Docker or WSL2, use `sdk-flash-manual` after putting the board into ROM bootloader mode manually, then press **RESET** once flashing finishes.
If you need the log from the first application line, start `sdk-simple-monitor` first and then press **RESET** on the board.
The frozen operator acceptance bar for this target lives in `docs/specs/stage2-foundation-quality-gate.md`.
