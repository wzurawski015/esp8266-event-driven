# Architecture Overview

## Layering rule

Dependencies may only point inward:

```text
app -> domain -> core
app -> ports -> core
adapters -> ports
bsp -> adapters
diag -> core/domain/ports
```

`core/` and `domain/` must remain independent of ESP8266 RTOS SDK details.

## Runtime model

The framework uses an **active-object-inspired** design, but an actor is not automatically a task.

- Actors own state and define message handling boundaries.
- Execution domains decide where work runs.
- Static routing decides where events flow.
- Mailbox semantics are explicit and bounded.

## Event model

Two messaging forms are planned:

- `ev_send(target, msg)` for commands and request/response flows,
- `ev_publish(msg)` for true events with statically declared routes.

## Memory model

Runtime allocation policy is strict:

- bootstrap may use temporary allocation if needed,
- steady state must use pools, fixed mailboxes, rings, and scratch arenas,
- ownership transfer must be explicit and auditable.

## Documentation model

The documentation system is part of the architecture:

- `config/*.def` are the single sources of truth,
- `tools/docgen/` produces catalogs and graphs,
- Doxygen publishes API and architecture pages,
- Graphviz renders canonical diagrams,
- GitHub Pages publishes the generated site.
