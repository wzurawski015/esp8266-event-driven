CC ?= cc
PYTHON ?= python3

CFLAGS ?= -std=c11 -Wall -Wextra -Werror -pedantic -Icore/include -Iconfig
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
    core/src/ev_lease_pool.c

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
    test_domain_pump_contract

HOST_TEST_BINS := $(addprefix $(BUILD_DIR)/,$(HOST_TESTS))

.PHONY: all host-test docgen docs clean

all: host-test

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%: tests/host/%.c $(CORE_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(CORE_SRCS) $< $(LDFLAGS) -o $@

host-test: $(HOST_TEST_BINS)
	@set -e; for t in $(HOST_TEST_BINS); do ./$$t; done

docgen:
	$(PYTHON) tools/docgen/docgen.py

docs: docgen
	doxygen Doxyfile

clean:
	rm -rf build docs/generated/*
	mkdir -p docs/generated
	touch docs/generated/.gitkeep
