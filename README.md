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

`./tools/fw docs` is intentionally strict for the documented public contract surface: if Doxygen emits warnings, the command fails.

### ESP8266 SDK baseline

```bash
./tools/fw sdk-check
./tools/fw shell-sdk
```

### Generic ESP8266 golden-reference target

```bash
./tools/fw sdk-defconfig
./tools/fw sdk-build
./tools/fw sdk-clean-target
./tools/fw sdk-distclean
./tools/fw sdk-build

FW_ESPPORT=/dev/ttyUSB0 ./tools/fw sdk-flash
FW_ESPPORT=/dev/ttyUSB0 ./tools/fw sdk-flash-manual
FW_ESPPORT=/dev/ttyUSB0 ./tools/fw sdk-simple-monitor
```

### ATNEL AIR ESP motherboard board target

```bash
FW_SDK_PROJECT_DIR=adapters/esp8266_rtos_sdk/targets/atnel_air_esp_motherboard ./tools/fw sdk-defconfig
FW_SDK_PROJECT_DIR=adapters/esp8266_rtos_sdk/targets/atnel_air_esp_motherboard ./tools/fw sdk-build
FW_SDK_PROJECT_DIR=adapters/esp8266_rtos_sdk/targets/atnel_air_esp_motherboard ./tools/fw sdk-clean-target
FW_SDK_PROJECT_DIR=adapters/esp8266_rtos_sdk/targets/atnel_air_esp_motherboard ./tools/fw sdk-distclean
FW_SDK_PROJECT_DIR=adapters/esp8266_rtos_sdk/targets/atnel_air_esp_motherboard ./tools/fw sdk-build

FW_SDK_PROJECT_DIR=adapters/esp8266_rtos_sdk/targets/atnel_air_esp_motherboard FW_ESPPORT=/dev/ttyUSB0 ./tools/fw sdk-flash
FW_SDK_PROJECT_DIR=adapters/esp8266_rtos_sdk/targets/atnel_air_esp_motherboard FW_ESPPORT=/dev/ttyUSB0 ./tools/fw sdk-flash-manual
FW_SDK_PROJECT_DIR=adapters/esp8266_rtos_sdk/targets/atnel_air_esp_motherboard FW_ESPPORT=/dev/ttyUSB0 ./tools/fw sdk-simple-monitor
```

These commands are the canonical local entry points.
Do not validate the framework by invoking host toolchains directly.
For Docker + WSL2 serial work, `sdk-simple-monitor` is the canonical runtime path for the current boot/diagnostic targets.
`tools/fw` now chooses the monitor baud from target context by default; the current Stage 2 ESP8266 targets default to `115200`.
Use `sdk-monitor` only when you explicitly need the SDK-native monitor behavior.
If `sdk-flash` fails with a DTR/RTS I/O error under Docker + WSL2, use `sdk-flash-manual` after placing the board into ROM bootloader mode manually. That path now calls `esptool.py` directly with `--before no-reset --after no-reset`, so neither pre-flash auto-reset nor post-flash reset is attempted by the wrapper. Press **RESET** after a successful manual flash.
Cleanup symmetry is part of the supported operator surface: `./tools/fw sdk-clean-target` and `./tools/fw sdk-distclean` must remain usable for both the generic and ATNEL targets.
See [`docs/specs/stage2-foundation-quality-gate.md`](docs/specs/stage2-foundation-quality-gate.md) for the frozen Stage 2 acceptance bar.

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
- Stage 2A3 adds the first concrete board profile under `bsp/atnel_air_esp_motherboard/`,
- Stage 2A4 adds the first concrete ESP8266 RTOS SDK adapters for clock/log/reset/uart,
- `esp8266_generic_dev` and `atnel_air_esp_motherboard` now share a common framework-backed boot/diagnostic harness,
- CI now verifies host docs/tests plus `sdk-clean-target -> sdk-distclean -> sdk-build` symmetry for both current SDK targets.

Next step:

- widen the adapter surface with GPIO/I2C/1-Wire,
- keep the generic and ATNEL targets on one shared bring-up path while BSP scope grows,
- only add board peripherals once the current operator workflow stays green.

## Non-negotiable constraints

- no runtime `malloc/free` in hot paths,
- no platform leakage from adapters into domain/core,
- no hidden ownership transfer,
- no undocumented public APIs,
- no uncontrolled broadcast event bus,
- no "clever" optimizations without measurement.

## License

Use the repository `LICENSE` file chosen for the project root.
