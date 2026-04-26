COMPONENT_SRCDIRS := . \
    ../../../../core/src \
    ../../../../app
# ev_freertos_static_hooks.c is compiled from the local ev_platform source
# directory above. It provides FreeRTOS Idle/Timer static task memory hooks when
# the ESP8266 SDK FreeRTOSConfig.h enables configSUPPORT_STATIC_ALLOCATION.
COMPONENT_ADD_INCLUDEDIRS := include \
    ../../../../ports/include \
    ../../../../core/include \
    ../../../../core/generated/include \
    ../../../../app/include \
    ../../../../config
