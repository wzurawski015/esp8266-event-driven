# ev_platform component

Reusable ESP8266 RTOS SDK-backed implementations of the public framework
platform contracts.

Current scope of this component:

- monotonic clock adapter
- log sink adapter
- reset/restart adapter
- UART adapter

This component is intentionally narrow.
It exists to prove the `ports/` contracts on target hardware before wider BSP
peripheral work begins.
