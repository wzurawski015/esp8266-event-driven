# ESP8266 Network Adapter

The ESP8266 network adapter connects verified ESP8266 RTOS SDK WiFi/MQTT callbacks to the existing HSHA Network Airlock.

## Safety contract

SDK callbacks are asynchronous ingress. They must not call core actors, mailboxes, `ev_publish()`, or domain/system pump APIs directly. Callback code copies bounded metadata into the adapter-owned static ingress ring. The portable app poll loop drains the ring through `ev_net_port_t::poll_ingress()` and publishes `EV_NET_*` events synchronously.

## Bounds and backpressure

The adapter owns a fixed-size power-of-two ingress ring using `EV_NET_INGRESS_RING_CAPACITY` and `EV_NET_INGRESS_RING_MASK`. If the ring is full, the newest event is dropped and `dropped_events` is incremented. MQTT topics and payloads are copied into fixed-size fields in `ev_net_ingress_event_t`; oversize data is dropped and counted in `dropped_oversize`. No SDK pointer is retained in the ring.

## Configuration

The ATNEL target passes WiFi/MQTT configuration from `bsp/atnel_air_esp_motherboard/board_profile.h` into the adapter through `ev_esp8266_net_config_t`. The adapter does not include `board_profile.h` directly, so the ev_platform component remains target-agnostic.

If `EV_BOARD_NET_MQTT_BROKER_URI` is empty, WiFi may still be started but the MQTT client remains disabled and `publish_mqtt` returns unsupported/state errors.

## MQTT foundation policy

This adapter does not route sensor telemetry and does not execute remote
commands. `EV_NET_TX_CMD` is rejected unless an SDK MQTT session is explicitly
connected. MQTT RX events are copied into bounded inline fields and delivered
to `ACT_NETWORK` only as inert ingress events; topic parsing for commands is a
future, separately reviewed commit. Oversize topics or payloads are dropped and
counted.

## Verification status

This document describes code-level integration only. ESP8266 SDK build, WiFi association, MQTT broker connection, and physical network HIL must be run on hardware before production network readiness is claimed.

## Callback/poll shared-state hardening

The ESP8266 adapter protects callback-updated connection state and app-polled
statistics with short critical sections.  SDK callbacks may update local adapter
state and push bounded ingress events, but they must not call `ev_publish()`,
actor handlers, mailbox APIs, or application logic.  The adapter must never hold
its internal critical section while calling SDK functions such as
`esp_wifi_connect()`, `esp_wifi_start()`, `esp_mqtt_client_start()`, or
`esp_mqtt_client_publish()`.

## Reconnect storm policy

WiFi disconnect callbacks are treated as state transitions.  A duplicate
`WIFI_DOWN` event while the adapter is already disconnected is suppressed and
counted in `duplicate_wifi_down_suppressed`.  Reconnect attempts are rate-limited
by `EV_ESP8266_NET_RECONNECT_MIN_INTERVAL_MS`; disconnect callbacks inside that
window increment `reconnect_suppressed` instead of calling `esp_wifi_connect()`.
This bounds reconnect storms and prevents unbounded ingress ring pressure.

## Event-loop ownership

The current ESP8266 adapter owns `esp_event_loop_init()` for this target.  Unknown
event-loop initialization failures are not silently ignored; they increment
`event_loop_init_failures` and fail initialization.  If a future SDK exposes a
verified "already initialized" error code, ownership sharing may be handled in a
separate reviewed commit.

## WiFi reconnect HIL

The physical WiFi adapter is qualified by the dedicated ATNEL HIL target
`atnel_air_esp_motherboard_wifi_hil`. It validates association, AP loss,
recovery, reconnect diagnostics, WDT diagnostic visibility, and callback/poll
heap stability through UART markers parsed by `tools/hil/wifi_reconnect_monitor.py`.

This HIL does not validate MQTT connectivity, telemetry, or remote commands.
Those require later acceptance suites.

## Private network credentials

Tracked board profiles intentionally keep WiFi/MQTT credentials empty.  The
ATNEL profile may include a developer-local `board_secrets.local.h` only when
`EV_BOARD_INCLUDE_LOCAL_SECRETS` is defined by the build.  The local secrets file
is ignored by git and should be created from
`bsp/atnel_air_esp_motherboard/board_secrets.example.h`.

Without local secrets or compile-time overrides, `EV_BOARD_HAS_NET` defaults to
`0U`, so normal host/CI builds do not require private WiFi credentials.  To run
physical WiFi tests, provide local definitions for `EV_BOARD_HAS_NET`,
`EV_BOARD_NET_WIFI_SSID`, and `EV_BOARD_NET_WIFI_PASSWORD`.  MQTT may remain
disabled by leaving `EV_BOARD_NET_MQTT_BROKER_URI` empty.
