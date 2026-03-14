# Event Model

## Principles

- Events exist only when declared in `config/events.def`.
- Actors exist only when declared in `config/actors.def`.
- Routes exist only when declared in `config/routes.def`.
- Routing is deterministic and reviewable.

## Why static routing

Blind broadcast pub/sub is easy to write and hard to control in constrained embedded systems.
Static routing is preferable here because it gives:

- bounded fan-out,
- better auditability,
- clearer ownership,
- simpler performance analysis,
- more reliable memory accounting.

## Planned mailbox semantics

Mailbox semantics should be attached to actor inboxes or routes, not treated as vague event-level wishes.

Candidate forms:

- `FIFO_N`
- `MAILBOX_1`
- `LOSSY_RING_N`
- `COALESCED_FLAG`
- `REQRESP_SLOT`
