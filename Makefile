CC ?= cc
PYTHON ?= python3

CFLAGS ?= -std=c11 -Wall -Wextra -Werror -pedantic -Icore/include -Iconfig
LDFLAGS ?=

BUILD_DIR := build/host
DOC_SITE_DIR := docs/generated/site

CORE_SRCS := \
    core/src/ev_version.c \
    core/src/ev_event_catalog.c \
    core/src/ev_actor_catalog.c

TEST_SRCS := tests/host/test_catalog.c

.PHONY: all host-test docgen docs clean

all: host-test

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/test_catalog: $(CORE_SRCS) $(TEST_SRCS) | $(BUILD_DIR)
	$(CC) $(CFLAGS) $(CORE_SRCS) $(TEST_SRCS) $(LDFLAGS) -o $@

host-test: $(BUILD_DIR)/test_catalog
	./$(BUILD_DIR)/test_catalog

docgen:
	$(PYTHON) tools/docgen/docgen.py

docs: docgen
	doxygen Doxyfile

clean:
	rm -rf build docs/generated/*
	mkdir -p docs/generated
	touch docs/generated/.gitkeep
