# ---------------------------------------------------------------------------
# Dockerfile — reproducible build environment for esp8266-event-driven
# (placeholder — fill in SDK/toolchain installation steps)
# ---------------------------------------------------------------------------

# Base image: use a pinned digest in production for reproducibility
FROM ubuntu:22.04

LABEL org.opencontainers.image.title="esp8266-event-driven build environment"
LABEL org.opencontainers.image.description="Reproducible build environment for the esp8266-event-driven framework"
LABEL org.opencontainers.image.source="https://github.com/wzurawski015/esp8266-event-driven"

# Prevent interactive prompts during package installation
ENV DEBIAN_FRONTEND=noninteractive

# ---------------------------------------------------------------------------
# System dependencies placeholder
# ---------------------------------------------------------------------------
RUN apt-get update && apt-get install -y --no-install-recommends \
    # Build tools
    build-essential \
    cmake \
    ninja-build \
    git \
    wget \
    curl \
    # Documentation
    doxygen \
    graphviz \
    # Clean up
    && rm -rf /var/lib/apt/lists/*

# ---------------------------------------------------------------------------
# ESP8266 toolchain placeholder
# ---------------------------------------------------------------------------
# TODO: Download and install the xtensa-lx106-elf toolchain here, e.g.:
# RUN wget -q https://dl.espressif.com/dl/xtensa-lx106-elf-gcc8_4_0-esp-2020r3-linux-amd64.tar.gz \
#     && tar -xzf xtensa-lx106-elf-*.tar.gz -C /opt \
#     && rm xtensa-lx106-elf-*.tar.gz
# ENV PATH="/opt/xtensa-lx106-elf/bin:${PATH}"

# ---------------------------------------------------------------------------
# ESP8266 RTOS SDK placeholder
# ---------------------------------------------------------------------------
# TODO: Clone (or COPY) the SDK, e.g.:
# RUN git clone --depth 1 --branch v3.4 \
#     https://github.com/espressif/ESP8266_RTOS_SDK /opt/esp8266-rtos-sdk
# ENV IDF_PATH="/opt/esp8266-rtos-sdk"

WORKDIR /workspace

# Default command — show help text until real build targets are defined
CMD ["bash", "-c", "echo 'Build environment placeholder — add toolchain and SDK to complete setup'"]
