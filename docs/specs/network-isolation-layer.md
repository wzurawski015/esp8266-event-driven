# Network Isolation Layer

Status: initial bounded HSHA airlock scaffold.  This document describes the
portable contract, host fakes, actor policy, and adapter boundary.  It does not
claim ESP8266 WiFi/MQTT hardware validation.

## HSHA boundary

The network layer must keep SDK callbacks out of the synchronous actor graph.
The only allowed flow is:

```text
SDK WiFi/MQTT callback
  -> bounded adapter ingress ring / fixed-size payload copy
  -> app poll ingress budget
  -> EV_NET_* publication
  -> ACT_NETWORK
```

Callbacks must not call `ev_publish`, actor handlers, mailbox APIs, or domain
logic directly.

## Bounded ingress and drop policy

`ev_net_port_t` exposes a bounded `poll_ingress` operation.  Adapter and fake
callback paths must push only into a power-of-two ring.  If the ring is full,
the newest event is dropped and `dropped_events` is incremented.  Oversize MQTT
topics or payloads are dropped and counted as `dropped_oversize`.

Current fixed limits are intentionally small for inline-message safety:

- `EV_NET_INGRESS_RING_CAPACITY`
- `EV_NET_MAX_TOPIC_BYTES`
- `EV_NET_MAX_INLINE_PAYLOAD_BYTES`

No heap allocation is allowed in callback, poll, actor, or publish paths.

## Actor policy

`ACT_NETWORK` lives in `EV_DOMAIN_SLOW_IO`.  The actor implements only a small
state machine in this commit:

```text
DISCONNECTED -> WIFI_UP -> MQTT_CONNECTED
```

`EV_NET_TX_CMD` calls `ev_net_port_t::publish_mqtt` only when MQTT is connected.
Reconnect loops, TLS, broker sessions, and production SDK integration are out of
scope for this commit.

## ESP8266 adapter status

The current ESP8266 adapter is a non-blocking unsupported scaffold because this
repository does not contain verified ESP8266 WiFi/MQTT SDK headers for a safe
implementation.  A future hardware commit must replace the scaffold with a
verified SDK callback bridge and HIL tests.

## Verification requirements for future hardware adapter

A production adapter must prove:

- SDK callbacks never block,
- SDK callbacks never call core directly,
- RX flood increments drop counters instead of starving the pump,
- WDT health does not remain true under network-induced stalls,
- no heap is used in callback or actor hot paths,
- WiFi/MQTT HIL stress passes with stored logs.
