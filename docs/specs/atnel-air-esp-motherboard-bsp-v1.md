# ATNEL AIR ESP motherboard BSP v1

Stage 2A3 freezes the first board-specific BSP profile for the following stack:

- ATNEL AIR ESP module,
- ATB WIFI ESP Motherboard,
- ESP8266 target path through the FT231X USB-UART interface.

## Scope of BSP v1

This stage does **not** yet claim full board integration.

A Stage 2A4 companion target now provides the first board-scoped boot/diag bring-up
using this BSP, but only through clock/log/reset/uart.

It only freezes:

- the primary GPIO ownership map,
- the safe jumper baseline for early framework bring-up,
- the enablement order for later adapter work.

## Primary board map

| Logical function | GPIO / path | Notes |
| --- | --- | --- |
| UART0 TX | GPIO1 | via FT231X |
| UART0 RX | GPIO3 | via FT231X |
| I2C SCL | GPIO4 | shared bus |
| I2C SDA | GPIO5 | shared bus |
| 1-Wire DQ | GPIO12 | via JP1 |
| Optional IR input | GPIO13 | via JP4 |
| Optional interrupt line | GPIO14 | MCP23008 INT via JP2 or RTC INT via JP19 |
| Bootstrap / optional Magic SCK | GPIO15 | bootstrap-sensitive |
| Optional Magic DIN | GPIO16 | helper path |

## Safe jumper baseline

Recommended early baseline:

- keep JP2 open,
- keep JP4 open,
- keep JP14 open,
- keep JP16 open,
- keep JP19 open,
- keep JP1 open until the dedicated 1-Wire stage.

This isolates boot, diagnostics, and early adapter work from optional board-side couplings.

## Why the board is not treated as generic

This board already couples multiple peripherals to the same ESP8266 pins:

- the module OLED and motherboard devices share the I2C bus,
- the motherboard offers DS18B20 headers on the same 1-Wire line,
- GPIO13 / GPIO14 / GPIO15 / GPIO16 may be repurposed by optional peripherals through jumpers.

Because of that, board-specific policy belongs in this BSP rather than in
`esp8266_generic_dev`.

## Planned enablement order after BSP v1

1. UART boot + diagnostics
2. clock / log / reset / uart public adapters
3. board-scoped boot and diagnostics target
4. I2C bus bring-up
5. OLED + RTC
6. MCP23008
7. 1-Wire / DS18B20
8. IR and Magic Hercules helpers
