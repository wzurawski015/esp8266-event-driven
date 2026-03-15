# Target firmware skeleton

Stage 2A2 introduces the first SDK-native ESP8266 RTOS SDK project skeleton.

Current target path:

```text
adapters/esp8266_rtos_sdk/targets/esp8266_generic_dev/
```

This target is intentionally minimal.

Goals of this stage:

- prove that the pinned SDK image can build a real target project,
- establish the canonical `make defconfig -> make -> make flash -> make monitor` workflow inside Docker,
- keep the first target independent from unfinished adapter work,
- provide a stable place where later board-specific bring-up can land.

## Project structure

```text
adapters/esp8266_rtos_sdk/targets/esp8266_generic_dev/
├── Makefile
├── README.md
├── sdkconfig.defaults
└── main/
    ├── app_main.c
    └── component.mk
```

## Why this project is intentionally small

The first target project is not yet the full framework firmware.

It is a controlled bring-up skeleton used to verify:

- SDK image reproducibility,
- project layout correctness,
- `sdkconfig.defaults` and `defconfig` flow,
- compile/flash/monitor wiring through `./tools/fw`,
- board-scoped build outputs and generated `sdkconfig`.

## Canonical commands

```bash
./tools/fw sdk-image
./tools/fw sdk-check
./tools/fw sdk-defconfig
./tools/fw sdk-build

FW_ESPPORT=/dev/ttyUSB0 ./tools/fw sdk-flash
FW_ESPPORT=/dev/ttyUSB0 ./tools/fw sdk-monitor
```

`sdk-defconfig` uses `sdkconfig.defaults` as the project default configuration seed.
`sdk-build` will auto-materialize `sdkconfig` via `defconfig` if it is missing.

## Current firmware behavior

The current `app_main()` only proves target build and runtime viability:

- boot log banner,
- board profile banner,
- compile-time visibility of public repo headers,
- periodic heartbeat log.

This is deliberate.
Framework-backed actors and adapter wiring will be layered in later Stage 2 steps.
