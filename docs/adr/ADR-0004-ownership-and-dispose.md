# ADR-0004: Ownership and dispose contract

- Status: Accepted
- Date: 2026-03-14

## Context

The framework must support high-performance message passing without runtime heap allocation in hot paths.
That requires explicit lifetime management and a cleanup rule that is simple enough to apply everywhere.

## Decision

We standardize on a single cleanup idiom:

- every received message must eventually go through `ev_msg_dispose()`,
- disposal is mandatory even for paths that terminate early,
- for payload kinds that do not own external resources, disposal is a safe no-op,
- for leased or stream-backed resources, disposal performs the required release action.

## Ownership classes

### Inline

- Data lives entirely inside the message envelope.
- No external release is needed.
- `ev_msg_dispose()` is a no-op after validation/reset.

### Lease

- The message references a buffer or object owned by a pool.
- Delivery may increase the number of outstanding references.
- Queueing or publish fan-out must acquire retained shares explicitly.
- Disposal decrements the reference count and returns the resource to the pool when it reaches zero.

### View

- The message carries a read-only non-owning view.
- The backing storage lifetime must be guaranteed by a separate owner.
- Views must never be used to smuggle mutable shared state.

### Stream

- The message describes a chunk, slice, or cursor over streaming storage.
- Disposal releases the stream reservation or cursor ownership as defined by the stream backend.

## Core rules

- Ownership must be explicit in constructors and API names.
- Consumers must not assume that a lease or stream survives past disposal.
- Consumers must not retain raw pointers after disposal.
- Mutation is allowed only by the current owner.
- Shared readable access must be represented as lease or view semantics, never by undocumented aliasing.

## Failure-path rule

If any step after message creation fails, the creator or the current owner must dispose the message before returning the error.
There must be no "best effort" cleanup conventions.

## Consequences

### Positive

- One universal cleanup path.
- Easier code review for memory safety.
- Better fit for host-side leak detection and pool accounting.

### Trade-offs

- Slightly more boilerplate at message boundaries.
- The API must make partial initialization and moved-from states well-defined.

## Follow-up

Implementation should provide:

- message constructors with explicit ownership names,
- `ev_msg_dispose()` that is safe on zero-initialized messages,
- host-side tests for normal and failure-path cleanup,
- pool diagnostics for lease and stream pressure.
