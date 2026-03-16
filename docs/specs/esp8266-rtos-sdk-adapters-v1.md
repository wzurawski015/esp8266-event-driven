# ESP8266 RTOS SDK adapters v1

Stage 2A4 introduces the first concrete ESP8266 RTOS SDK implementations of the
public platform ports.

Current adapters live in:

```text
adapters/esp8266_rtos_sdk/components/ev_platform/
```

## Implemented ports

- `ev_clock_port_t`
- `ev_log_port_t`
- `ev_reset_port_t`
- `ev_uart_port_t`

## Intentional limits of v1

This stage focuses on boot and diagnostics only.

The adapters are intentionally narrow:

- the clock adapter exposes monotonic time using SDK log timestamp granularity,
- wall-clock time is still reported as unsupported,
- the log adapter is task-context only and not ISR-safe,
- the UART adapter is tuned for UART0 boot/diagnostic flow,
- no GPIO/I2C/1-Wire adapter is claimed yet.

## Runtime log portability note

The public clock contract stays 64-bit and reports monotonic microseconds through
`ev_time_mono_us_t`.

Target-side bring-up logs intentionally do **not** print that value with 64-bit
`printf` specifiers. On the current ESP8266 RTOS SDK v3.4 runtime path used for
early diagnostics, format strings that depend on platform-specific length
modifiers are not reliable enough for a clean serial stream.

For that reason, the ATNEL boot target projects monotonic time into a diagnostic
32-bit millisecond value (`mono_now_ms`) for runtime logs only, and prints it
with `%u` plus an explicit cast to `unsigned`. This keeps the serial heartbeat
readable without weakening the public platform contract.

If long-uptime logging becomes a requirement later, the next step should be an
explicit high/low-part log format rather than reintroducing target-side 64-bit
`printf` dependence.

## Adapter hardening rules

The current adapter layer follows these invariants:

- unsupported UART ports fail explicitly with `EV_ERR_UNSUPPORTED`,
- invalid UART configuration fails explicitly with `EV_ERR_INVALID_ARG`,
- the UART adapter does not silently coerce unsupported line settings,
- log flushing is best-effort even before the UART driver is installed,
- target diagnostics may project rich public data into smaller runtime-only views when serial portability requires it.

## Why these four come first

These ports are enough to establish a framework-backed bring-up path on target
hardware:

1. identify the board,
2. identify the reset reason,
3. emit logs through the public logging contract,
4. keep a monotonic heartbeat through the public clock contract,
5. prove that UART console usage can be expressed through a public port.

## What follows next

Later Stage 2 steps should extend this with:

- GPIO
- I2C
- 1-Wire
- RTC / DS1337 integration
- MCP23008 integration
- IR and Magic Hercules helpers
