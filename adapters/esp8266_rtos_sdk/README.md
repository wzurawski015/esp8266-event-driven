# adapters/esp8266_rtos_sdk

This directory will host the concrete ESP8266 RTOS SDK implementations of the
public platform contracts declared in `ports/include/ev/`.

Rules:

- adapters may include ESP8266 RTOS SDK headers,
- adapters may translate SDK-native errors into `ev_result_t`,
- adapters must not export SDK-native types through public port contracts,
- adapters must keep ownership semantics explicit,
- adapters should remain replaceable by host-side fakes in tests.
