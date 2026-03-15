# esp8266_generic_dev target

This is the golden reference SDK-native target project used for Stage 2 bring-up.

Scope of this target:

- validate the pinned ESP8266 RTOS SDK image,
- validate `sdkconfig.defaults -> defconfig -> build`,
- provide a minimal heartbeat firmware under Docker-first workflow,
- stay intentionally board-neutral,
- create a stable landing zone for later BSP and adapter integration.

Canonical commands from the repository root:

```bash
./tools/fw sdk-defconfig
./tools/fw sdk-build
./tools/fw sdk-clean-target
./tools/fw sdk-distclean

FW_ESPPORT=/dev/ttyUSB0 ./tools/fw sdk-flash
FW_ESPPORT=/dev/ttyUSB0 ./tools/fw sdk-monitor
```

Notes:

- generated `sdkconfig` is intentionally ignored by Git,
- build artifacts remain inside this project-local target directory,
- this project is not yet the final framework firmware image,
- future Stage 2 steps will wire public `ports/` contracts to concrete ESP8266 RTOS SDK adapters.


CI policy:

- `esp8266_generic_dev` must stay buildable without hardware attached,
- hardware-specific features belong in dedicated BSP profiles,
- this target should remain the smallest trustworthy target-side reference.
