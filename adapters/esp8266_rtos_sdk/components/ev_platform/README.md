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


Behavioral guarantees in the current stage:

- the UART adapter currently supports UART0 only and rejects other port numbers with `EV_ERR_UNSUPPORTED`,
- invalid UART settings are rejected explicitly instead of being silently coerced,
- log flush is best-effort and does not depend on UART driver installation order,
- wall-clock time is still intentionally unsupported,
- USB modem-control boot/reset choreography stays an operator workflow concern and is not part of the public UART contract.
