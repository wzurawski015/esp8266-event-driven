# Runtime diagnostics

## Purpose

The contract-stage runtime must already expose enough counters to reason about:

- accepted versus failed deliveries,
- missing actor bindings,
- mailbox pressure per actor,
- handler failures,
- empty runtime steps.

These counters are not a replacement for tracing, but they provide deterministic
observability without introducing platform dependencies.

## Registry delivery counters

`ev_actor_registry_t` owns cumulative transport counters for mailbox delivery
through `ev_actor_registry_delivery()`.

Tracked fields:

- `delivery_attempted`
- `delivery_succeeded`
- `delivery_failed`
- `delivery_target_missing`
- `last_target_actor`
- `last_result`

A delivery is **attempted** whenever `ev_actor_registry_delivery()` is called.
If the registry has no binding for the selected actor, the operation counts as
a failed delivery and also increments `delivery_target_missing`.

## Per-actor runtime counters

`ev_actor_runtime_t` owns cumulative counters for one logical actor instance.

Tracked fields:

- `enqueued`
- `enqueue_failed`
- `steps_ok`
- `steps_empty`
- `handler_errors`
- `dispose_errors`
- `pending_high_watermark`
- `last_result`

`pending_high_watermark` is sampled after successful mailbox enqueue.
The counter therefore reflects the highest pending depth actually accepted by
that runtime.

## Reset semantics

Counter reset is explicit and separate from queue reset.

- `ev_actor_registry_reset_stats()` clears only registry counters.
- `ev_actor_runtime_reset_stats()` clears only runtime counters.
- `ev_mailbox_reset()` clears queue state and mailbox-local counters.

This separation allows tests and future production code to reset transport and
execution diagnostics without implicitly discarding queued work.

## Relationship to `config/metrics.def`

The public diagnostics API is the executable contract.
`config/metrics.def` is the catalog-level SSOT that names which counters should
remain stable as the framework grows into an ESP8266 runtime.

## Non-goals

At this stage we do **not** yet expose:

- time-series history,
- per-event counters,
- platform timestamps,
- CLI rendering,
- persistence across reset.

Those belong to later diagnostic layers and should be built on top of these
small deterministic counters.
