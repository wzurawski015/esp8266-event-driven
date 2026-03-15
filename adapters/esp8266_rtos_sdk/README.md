# adapters/esp8266_rtos_sdk

This directory hosts ESP8266 RTOS SDK-specific integration assets for the framework.

Current contents:

- SDK-oriented notes and constraints for adapter work,
- the first SDK-native target skeleton under `targets/esp8266_generic_dev/`.

Rules:

- adapters may include ESP8266 RTOS SDK headers,
- adapters may translate SDK-native errors into `ev_result_t`,
- adapters must not export SDK-native types through public port contracts,
- adapters must keep ownership semantics explicit,
- adapters should remain replaceable by host-side fakes in tests,
- SDK-native project scaffolding belongs here until a stable BSP/adapter split is fully operational on hardware.
