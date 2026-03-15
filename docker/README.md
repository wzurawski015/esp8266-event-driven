# Docker images

This directory hosts the pinned, reproducible images for:

- host-side test execution,
- documentation generation,
- ESP8266 RTOS SDK build-toolchain validation.

## Policy

All developer-facing validation must run through `./tools/fw` and Docker.
Do not treat the host machine toolchain as part of the trusted build contract.

## Current images

- `Dockerfile.host` — host-side compilation, smoke tests, and documentation helpers.
- `Dockerfile.docs` — Doxygen + Graphviz documentation generation.
- `Dockerfile.sdk` — pinned ESP8266 RTOS SDK toolchain baseline for target-side work.

## Stage 2A policy

For ESP8266 target work, the canonical baseline remains the SDK-native GNU Make
workflow inside the SDK image. CMake may exist in the image for investigation
and future migration evaluation, but it is not yet the release-defining target
build path.

The SDK image also contains a narrow compatibility workaround for the broken
legacy tinydtls submodule URL still present in ESP8266 RTOS SDK v3.4. The image
rewrites that exact dead URL to the active GitHub mirror before recursive
submodule checkout.

## Canonical entry points

```bash
./tools/fw host-test
./tools/fw docgen
./tools/fw docs
./tools/fw sdk-image
./tools/fw sdk-check
./tools/fw shell-sdk
```
