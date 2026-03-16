# esp8266-event-driven

Production-grade, event-driven clean architecture framework in pure C for ESP8266.

## Vision

This repository is the foundation for a long-lived embedded framework focused on:

- deterministic runtime behavior,
- zero-heap hot path after bootstrap,
- static event routing instead of blind broadcast pub/sub,
- streaming-first design,
- clean architecture boundaries,
- living documentation generated from single sources of truth,
- host-side verification before hardware integration,
- Docker-based reproducibility for all developer-facing validation.

## Core design principles

1. **Pure C, disciplined interfaces**
   Public APIs must be small, explicit, and ownership-aware.

2. **Static routing over dynamic magic**
   Events are described once and routed through compile-time tables.

3. **Actor boundaries, not task explosion**
   Actors own state. Tasks are a deployment choice, not a design crutch.

4. **Zero heap in the hot path**
   Memory policy is explicit: pools, rings, leases, scratch arenas.

5. **Streaming-first**
   Large payloads must move through views, rings, and leases, not ad-hoc copies.

6. **Living documentation**
   SSOT files in `config/*.def` drive code, catalogs, and architecture diagrams.

## Repository layout

```text
app/        composition root and system wiring
core/       platform-agnostic kernel, catalogs, memory model, result codes
domain/     use cases, state machines, policies
ports/      stable interfaces crossing into infrastructure
adapters/   ESP8266 RTOS SDK bindings, target skeletons, and external integrations
bsp/        board-specific configuration and pin maps
diag/       diagnostics, metrics, CLI, tracing
config/     single sources of truth
docs/       architecture, ADRs, generated documentation
tests/      host, contract, soak
tools/      scripts for build, docs, validation
docker/     reproducible build images
```

## Quick start

Prerequisite: Docker must be installed and usable without interactive elevation.

### Host-side foundation

```bash
./tools/fw host-test
./tools/fw docgen
./tools/fw docs
```

### ESP8266 SDK baseline

```bash
./tools/fw sdk-image
./tools/fw sdk-check
./tools/fw shell-sdk
```

### Minimal target firmware skeleton

```bash
./tools/fw sdk-defconfig
./tools/fw sdk-build
./tools/fw sdk-clean-target
./tools/fw sdk-distclean

FW_ESPPORT=/dev/ttyUSB0 ./tools/fw sdk-flash
FW_ESPPORT=/dev/ttyUSB0 ./tools/fw sdk-monitor
```

### ATNEL AIR ESP motherboard board target

```bash
FW_SDK_PROJECT_DIR=adapters/esp8266_rtos_sdk/targets/atnel_air_esp_motherboard ./tools/fw sdk-defconfig
FW_SDK_PROJECT_DIR=adapters/esp8266_rtos_sdk/targets/atnel_air_esp_motherboard ./tools/fw sdk-build

FW_SDK_PROJECT_DIR=adapters/esp8266_rtos_sdk/targets/atnel_air_esp_motherboard FW_ESPPORT=/dev/ttyUSB0 ./tools/fw sdk-flash

FW_SDK_PROJECT_DIR=adapters/esp8266_rtos_sdk/targets/atnel_air_esp_motherboard FW_ESPPORT=/dev/ttyUSB0 FW_MONITOR_BAUD=115200 ./tools/fw sdk-simple-monitor
```

These commands are the canonical local entry points.
Do not validate the framework by invoking host toolchains directly.
For Docker + WSL2 serial work, `sdk-simple-monitor` is the canonical ATNEL runtime path.
Use `sdk-monitor` only when you explicitly need the SDK-native monitor behavior.

## Current status

Stage 1 foundation is complete on `main`.

Implemented in the current codebase:

- repository structure and coding conventions,
- Docker-first validation workflow,
- Doxygen + Graphviz documentation pipeline,
- SSOT-generated event, actor, route and metric catalogs,
- message / send / publish / dispose contracts,
- static route table,
- mailbox abstraction,
- actor runtime skeleton,
- lease-aware transport,
- deterministic lease pool,
- publish failure policy and fan-out accounting,
- runtime diagnostics,
- bounded actor pump,
- cooperative domain pump,
- multi-domain system pump.

Stage 2 progress:

- Stage 2A1 freezes the ESP8266 SDK image and first platform-contract set,
- Stage 2A2 adds the first SDK-native target skeleton under `adapters/esp8266_rtos_sdk/targets/esp8266_generic_dev`,
- `esp8266_generic_dev` now acts as the golden reference bring-up target for ESP8266 target-side validation,
- `./tools/fw` now exposes target-side `sdk-defconfig`, `sdk-menuconfig`, `sdk-build`, `sdk-clean-target`, `sdk-distclean`, `sdk-flash`, and `sdk-monitor`,
- CI now verifies the pinned SDK image and the generic target build without requiring hardware.

Stage 2A3 progress:

- the first concrete board profile now lives under `bsp/atnel_air_esp_motherboard/`,
- `esp8266_generic_dev` stays the board-neutral golden reference target,
- the ATNEL profile freezes a safe jumper baseline before board-specific adapter work.

Next step:

- Stage 2A4 adds the first concrete ESP8266 RTOS SDK adapters for clock/log/reset/uart,
- Stage 2A4 also adds the first board-scoped bring-up target for `bsp/atnel_air_esp_motherboard/`,
- later Stage 2 steps will extend this with GPIO/I2C/1-Wire and board peripherals.

## Non-negotiable constraints

- no runtime `malloc/free` in hot paths,
- no platform leakage from adapters into domain/core,
- no hidden ownership transfer,
- no undocumented public APIs,
- no uncontrolled broadcast event bus,
- no "clever" optimizations without measurement.

## License

Use the repository `LICENSE` file chosen for the project root.
