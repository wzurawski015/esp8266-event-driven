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
