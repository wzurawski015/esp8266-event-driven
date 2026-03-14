# esp8266-event-driven

Production-grade event-driven clean architecture framework in **pure C** for the
[ESP8266 RTOS SDK](https://github.com/espressif/ESP8266_RTOS_SDK).

Key design goals:

- **Deterministic runtime** — bounded worst-case execution time on every hot path.
- **Zero-heap hot path** — all event dispatch and routing uses statically allocated
  structures; `malloc` is forbidden after initialisation.
- **Static routing** — the event-dispatch table is resolved at compile/link time;
  no runtime registration overhead.
- **Streaming-first** — sensors and actuators are modelled as typed byte streams;
  framing, back-pressure, and flow-control are first-class concerns.
- **Living documentation** — Doxygen, Architecture Decision Records (ADRs), and CI
  checks keep docs in sync with code.

## Repository layout

```
.
├── adapters/          # I/O adapters (UART, GPIO, SPI, …) — ports implementations
├── core/              # Framework kernel: scheduler, event bus, memory pools
├── domain/            # Pure business logic, no hardware dependencies
├── ports/             # Abstract interfaces (port contracts) between layers
├── docs/
│   ├── adr/           # Architecture Decision Records
│   └── index.md       # Documentation entry point
├── .github/
│   └── workflows/     # CI/CD pipeline definitions
├── Dockerfile         # Reproducible build environment
├── docker-compose.yml # Multi-service development environment
└── Doxyfile           # Doxygen configuration
```

## Architecture overview

The project follows a **Hexagonal Architecture** (Ports & Adapters):

```
          ┌───────────────────────────────────────────┐
          │                   domain/                 │
          │   pure business rules, no I/O, no SDK     │
          └──────────────┬───────────────┬────────────┘
                         │               │
                    ports/in          ports/out
                  (driving)         (driven)
                         │               │
          ┌──────────────▼───────────────▼────────────┐
          │                    core/                  │
          │   event bus · scheduler · memory pools    │
          └──────────────────────┬─────────────────────┘
                                 │
          ┌──────────────────────▼─────────────────────┐
          │                  adapters/                  │
          │   UART · GPIO · SPI · Wi-Fi · NVS · …      │
          └─────────────────────────────────────────────┘
```

## Getting started

### Prerequisites

| Tool | Minimum version |
|------|----------------|
| ESP8266 RTOS SDK | v3.4 |
| xtensa-lx106-elf GCC toolchain | 8.4.0 |
| CMake | 3.16 |
| Docker (optional) | 24.0 |

### Build with Docker (recommended)

```bash
docker compose run --rm build
```

### Build natively

```bash
# export IDF_PATH and add toolchain to PATH first
cmake -B build -DCMAKE_TOOLCHAIN_FILE=toolchain-esp8266.cmake
cmake --build build
```

## Documentation

- Full API reference: run `doxygen Doxyfile` → open `docs/html/index.html`
- Architecture decisions: [`docs/adr/`](docs/adr/)
- Documentation index: [`docs/index.md`](docs/index.md)

## Contributing

1. Open an issue describing the problem or feature.
2. Create a feature branch from `main`.
3. Write an ADR under `docs/adr/` for any significant decision (use
   [`docs/adr/0000-template.md`](docs/adr/0000-template.md)).
4. Submit a pull request; all CI checks must pass.

## License

See [LICENSE](LICENSE).

