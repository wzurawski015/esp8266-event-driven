# Target firmware skeleton

Stage 2A2 introduces the first SDK-native ESP8266 RTOS SDK project skeleton.

Current target path:

```text
adapters/esp8266_rtos_sdk/targets/esp8266_generic_dev/
```

This target is intentionally minimal and now acts as the golden reference target for ESP8266 target-side validation.

Goals of this stage:

- prove that the pinned SDK image can build a real target project,
- establish the canonical `make defconfig -> make -> make flash -> make monitor` workflow inside Docker,
- keep the first target independent from unfinished adapter work,
- keep one board-neutral target permanently green in CI,
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
./tools/fw sdk-clean-target
./tools/fw sdk-distclean

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


## Finalization policy for `esp8266_generic_dev`

`esp8266_generic_dev` is not the final board support package.

It is the smallest target that must remain:

- reproducible,
- board-neutral,
- CI-buildable without attached hardware,
- stable enough to separate framework/toolchain regressions from board-specific wiring issues.

Board-specific peripherals such as OLED, RTC, MCP23008, 1-Wire sensor networks, IR receivers, or WS2812-driven helper hardware belong in dedicated BSP profiles rather than this generic target.
