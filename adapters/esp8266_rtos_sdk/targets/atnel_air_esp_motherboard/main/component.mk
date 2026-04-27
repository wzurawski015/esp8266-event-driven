COMPONENT_SRCDIRS := .
EV_BOARD_PROFILE_DIR := $(COMPONENT_PATH)/../../../../../bsp/atnel_air_esp_motherboard
COMPONENT_ADD_INCLUDEDIRS := . $(EV_BOARD_PROFILE_DIR)
COMPONENT_REQUIRES := ev_platform

# Developer-local WiFi/MQTT credentials are intentionally kept out of git.
# When board_secrets.local.h is present, enable the opt-in include in
# board_profile.h without requiring fragile manual CFLAGS overrides.
ifneq ($(wildcard $(EV_BOARD_PROFILE_DIR)/board_secrets.local.h),)
CFLAGS += -DEV_BOARD_INCLUDE_LOCAL_SECRETS=1
endif
