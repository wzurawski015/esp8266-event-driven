# esp8266_generic_dev target

This is the first SDK-native target project used for Stage 2 bring-up.

Scope of this target:

- validate the pinned ESP8266 RTOS SDK image,
- validate `sdkconfig.defaults -> defconfig -> build`,
- provide a minimal heartbeat firmware under Docker-first workflow,
- create a stable landing zone for later BSP and adapter integration.

Canonical commands from the repository root:

```bash
./tools/fw sdk-defconfig
./tools/fw sdk-build

FW_ESPPORT=/dev/ttyUSB0 ./tools/fw sdk-flash
FW_ESPPORT=/dev/ttyUSB0 ./tools/fw sdk-monitor
```

Notes:

- generated `sdkconfig` is intentionally ignored by Git,
- build artifacts remain inside this project-local target directory,
- this project is not yet the final framework firmware image,
- future Stage 2 steps will wire public `ports/` contracts to concrete ESP8266 RTOS SDK adapters.
