# Docker images

This directory hosts the pinned, reproducible images for:

- host-side test execution,
- documentation generation,
- future ESP8266 RTOS SDK firmware builds.

## Policy

All developer-facing validation must run through `./tools/fw` and Docker.
Do not treat the host machine toolchain as part of the trusted build contract.

## Current images

- `Dockerfile.host` — host-side compilation, smoke tests, and documentation helpers.
- `Dockerfile.docs` — Doxygen + Graphviz documentation generation.

The SDK image will be added once the exact ESP8266 RTOS SDK and toolchain contract is frozen.
