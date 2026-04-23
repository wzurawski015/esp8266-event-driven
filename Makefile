CC ?= cc
PYTHON ?= python3

CFLAGS ?= -std=c11 -Wall -Wextra -g -O0  -pedantic -DEV_HOST_BUILD -Icore/include -Iports/include -Iapp/include -Iconfig
LDFLAGS ?= 

BUILD_DIR := build/host
DOC_SITE_DIR := docs/generated/site

CORE_SRCS := \
    core/src/ev_version.c \
    core/src/ev_event_catalog.c \
    core/src/ev_actor_catalog.c \
    core/src/ev_msg.c \
    core/src/ev_route_table.c \
    core/src/ev_send.c \
    core/src/ev_publish.c \
    core/src/ev_dispose.c \
    core/src/ev_mailbox.c \
    core/src/ev_actor_runtime.c \
    core/src/ev_domain_pump.c \
    core/src/ev_system_pump.c \
    core/src/ev_lease_pool.c \
    core/src/ev_rtc_actor.c \
    core/src/ev_ds18b20_actor.c \
    core/src/ev_mcp23008_actor.c \
    core/src/ev_panel_actor.c \
    core/src/ev_oled_actor.c

APP_SRCS := \
    app/ev_demo_app.c

TEST_SUPPORT_SRCS := \
    tests/host/fakes/fake_i2c_port.c \
    tests/host/fakes/fake_irq_port.c \
    tests/host/fakes/fake_onewire_port.c

COMMON_SRCS := $(CORE_SRCS) $(APP_SRCS) $(TEST_SUPPORT_SRCS)

HOST_TESTS := \
    test_catalog \
    test_msg_contract \
    test_route_table \
    test_dispatch_contract \
    test_mailbox_contract \
    test_actor_runtime \
    test_lease_pool_contract \
    test_runtime_diagnostics \
    test_actor_pump_contract \
    test_domain_pump_contract \
    test_system_pump_contract \
    test_demo_app_contract \
    test_demo_app_fault_contract

HOST_TEST_BINS := $(addprefix $(BUILD_DIR)/,$(HOST_TESTS))

.PHONY: all host-test docgen docs clean

all: host-test

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%: tests/host/%.c $(COMMON_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(COMMON_SRCS) $< $(LDFLAGS) -o $@

host-test: $(HOST_TEST_BINS)
	@set -e; for t in $(HOST_TEST_BINS); do ./$$t; done

docgen:
	$(PYTHON) tools/docgen/docgen.py

docs: docgen
	rm -f docs/generated/doxygen-warnings.log
	doxygen Doxyfile
	@if [ -s docs/generated/doxygen-warnings.log ]; then \
		echo "error: Doxygen warnings detected; see docs/generated/doxygen-warnings.log" >&2; \
		cat docs/generated/doxygen-warnings.log >&2; \
		exit 1; \
	fi

clean:
	rm -rf build docs/generated/*
	mkdir -p docs/generated
	touch docs/generated/.gitkeep
