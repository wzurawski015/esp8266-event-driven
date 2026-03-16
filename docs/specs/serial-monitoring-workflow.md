# Serial monitoring workflow

This document freezes the current Docker-first serial monitoring policy for ESP8266 bring-up.

## Two monitor paths exist on purpose

`./tools/fw sdk-monitor` remains the SDK-native path.
It is still useful when the SDK monitor behavior itself is under investigation.

`./tools/fw sdk-simple-monitor` is the hardened fallback for day-to-day runtime diagnostics on real hardware.
It avoids the SDK monitor stack, runs without a pseudo-TTY, and opens the serial device directly inside the SDK container.

## Canonical Docker-first flow

From the repository root:

```bash
FW_SDK_PROJECT_DIR=adapters/esp8266_rtos_sdk/targets/atnel_air_esp_motherboard ./tools/fw sdk-build

FW_SDK_PROJECT_DIR=adapters/esp8266_rtos_sdk/targets/atnel_air_esp_motherboard \
FW_ESPPORT=/dev/ttyUSB0 \
./tools/fw sdk-flash

FW_SDK_PROJECT_DIR=adapters/esp8266_rtos_sdk/targets/atnel_air_esp_motherboard \
FW_ESPPORT=/dev/ttyUSB0 \
FW_MONITOR_BAUD=115200 \
./tools/fw sdk-simple-monitor
```

## When to use each command

Use `sdk-monitor` when:

- you want the SDK monitor UX,
- you need symbol-aware crash decoding later,
- pseudo-TTY handling is known to work in the current environment.

Use `sdk-simple-monitor` when:

- Docker + WSL2 interactive monitor behavior is unstable,
- the SDK monitor exits with TTY-related errors,
- you only need a clean runtime serial stream.

## Baud-rate policy

For the current ATNEL target, the application runtime logs are expected at `115200`.

Boot ROM output from ESP8266 may still appear at a different baud rate during the earliest boot window.
That noise is acceptable as long as the post-boot runtime log stream is stable and readable.

## Runtime log formatting policy

For the current ESP8266 bring-up stage, runtime heartbeat logs print `mono_now_ms`
as a diagnostic 32-bit millisecond value.

This is intentional. The public clock contract remains 64-bit in microseconds, but
the serial diagnostics path avoids target-side `printf` length modifiers that are
not stable on the current ESP8266 runtime path.

A runtime symptom of this portability problem is a line such as `mono_now_ms=lu`
instead of a numeric value. When that appears, the target should be treated as a
firmware formatting bug, not as a serial-line baud mismatch.

## WSL2 note

On WSL2, serial access still depends on `usbipd` attach flow from Windows into WSL.
After attach, validate that `/dev/ttyUSB0` or `/dev/ttyACM0` exists before invoking `./tools/fw`.

When the SDK-native monitor path fails under pseudo-TTY handling, prefer `sdk-simple-monitor` instead of bypassing Docker.
The simple monitor intentionally streams raw serial bytes and does not rely on `make simple_monitor`.

## Definition of done for target-side serial bring-up

The serial workflow is considered healthy when:

- `sdk-build` succeeds,
- `sdk-flash` succeeds,
- `sdk-simple-monitor` produces a stable runtime log stream,
- post-boot heartbeat logs are readable and monotonic,
- Docker remains the canonical operator path.
