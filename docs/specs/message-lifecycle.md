# Message Lifecycle

## Scope

This document describes the canonical lifecycle of a runtime message from construction to disposal.
It is intentionally stricter than a generic event-bus model.

## Lifecycle states

A message conceptually moves through these states:

1. **Zero / reset**
   - The object exists but carries no valid event.

2. **Constructed**
   - Header fields are valid.
   - Payload representation matches the event catalog contract.

3. **Posted**
   - The sender has requested `ev_send()` or `ev_publish()`.

4. **Enqueued**
   - The message has been accepted by one or more destination mailboxes.

5. **Consumed**
   - A destination actor has removed it from its mailbox and processed it.

6. **Disposed**
   - Cleanup has completed.
   - The message is no longer a valid owner or carrier of resources.

## Direct-send path

```text
producer
  -> construct message
  -> ev_send(target, msg)
  -> target mailbox accepts or rejects
  -> target consumes
  -> target disposes
```

### Direct-send invariants

- exactly one target mailbox participates,
- mailbox acceptance is explicit,
- rejection returns an error to the caller,
- the caller remains responsible for cleanup if enqueue fails.

## Publish path

```text
producer
  -> construct message with target = EV_ACTOR_NONE
  -> ev_publish(msg)
  -> static route table resolves targets
  -> each target mailbox accepts or rejects according to policy
  -> each consumer disposes its delivered message view
```

### Publish invariants

- target set is resolved only from SSOT routes,
- fan-out is bounded,
- publish does not imply broadcast,
- resource ownership across fan-out must be explicit and auditable.

## Failure paths

### Construction failure

If construction fails, no message enters the system and no further cleanup is required beyond local rollback.

### Enqueue failure

If a direct send fails before mailbox acceptance, the sender remains responsible for disposal.

If publish fails partway through fan-out, the framework must define a deterministic rollback or partial-delivery accounting rule.
This rule must be documented by implementation tests before production use.

### Consumer failure

A consumer that aborts processing early must still dispose the message.
No error path is allowed to skip cleanup.

## Review checklist

Every code review touching message flow should answer:

- who constructs the message,
- who owns the payload,
- what happens if enqueue fails,
- what function performs disposal,
- whether fan-out changes resource lifetime.
