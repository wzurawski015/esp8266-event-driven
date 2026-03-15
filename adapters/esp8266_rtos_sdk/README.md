# adapters/esp8266_rtos_sdk

This directory hosts ESP8266 RTOS SDK-specific integration assets for the framework.

Current contents:

- SDK-oriented notes and constraints for adapter work,
- reusable ESP8266 RTOS SDK-backed port adapters in `components/ev_platform/`,
- the golden-reference target under `targets/esp8266_generic_dev/`,
- the first board-scoped target under `targets/atnel_air_esp_motherboard/`.

Rules:

- adapters may include ESP8266 RTOS SDK headers,
- adapters may translate SDK-native errors into `ev_result_t`,
- adapters must not export SDK-native types through public port contracts,
- adapters must keep ownership semantics explicit,
- adapters should remain replaceable by host-side fakes in tests,
- SDK-native project scaffolding belongs here until a stable BSP/adapter split is fully operational on hardware.


The first board-scoped target uses the public port adapters to validate boot and
diagnostics flow without yet enabling higher-level peripherals.
