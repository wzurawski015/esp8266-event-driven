COMPONENT_SRCDIRS := .
COMPONENT_ADD_INCLUDEDIRS := . ../../../../../bsp/atnel_air_esp_motherboard
COMPONENT_REQUIRES := ev_platform
CFLAGS += -DEV_HIL_WIFI_RECONNECT=1
