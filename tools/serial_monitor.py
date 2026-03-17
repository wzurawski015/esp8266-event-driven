#!/usr/bin/env python3
"""Simple raw serial monitor for Docker-first ESP8266 workflows.

This helper intentionally avoids the SDK's interactive monitor stack.
It streams raw bytes to stdout and exits cleanly on SIGINT/SIGTERM without an
extra shell layer between Docker and Python.
"""

from __future__ import annotations

import os
import signal
import sys
from typing import Any

import serial

_STOP = False
_SIGINT_COUNT = 0


def _request_stop(signum: int, _frame: Any) -> None:
    global _STOP, _SIGINT_COUNT
    _STOP = True
    if signum == signal.SIGINT:
        _SIGINT_COUNT += 1
        if _SIGINT_COUNT >= 3:
            raise KeyboardInterrupt


def _open_serial(port: str, baud: int) -> serial.Serial:
    kwargs: dict[str, Any] = {
        "port": port,
        "baudrate": baud,
        "timeout": 0.20,
        "xonxoff": False,
        "rtscts": False,
        "dsrdtr": False,
    }

    try:
        kwargs["exclusive"] = True
        return serial.Serial(**kwargs)
    except TypeError:
        kwargs.pop("exclusive", None)
        return serial.Serial(**kwargs)


def main() -> int:
    if len(sys.argv) != 3:
        print("usage: serial_monitor.py <port> <baud>", file=sys.stderr)
        return 2

    port = sys.argv[1]
    baud = int(sys.argv[2])

    signal.signal(signal.SIGINT, _request_stop)
    signal.signal(signal.SIGTERM, _request_stop)

    try:
        ser = _open_serial(port, baud)
    except Exception as exc:  # pragma: no cover - direct operator feedback
        print(f"error: could not open serial port {port}: {exc}", file=sys.stderr)
        return 1

    try:
        with ser:
            while not _STOP:
                chunk = ser.read(max(1, ser.in_waiting))
                if chunk:
                    os.write(sys.stdout.fileno(), chunk)
    except KeyboardInterrupt:
        pass
    finally:
        os.write(sys.stderr.fileno(), b"\n--- exit ---\n")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
