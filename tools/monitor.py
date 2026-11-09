#!/usr/bin/env python3
"""Minimal serial monitor for Ripple.

Unlike `pio device monitor` it discards the stale bytes some USB-serial
drivers replay when the port is opened, and it also appends everything to
.pio/monitor.log so the output can be inspected later.

    python tools/monitor.py [port]        # default: /dev/cu.usbserial-310

Keys are sent to the board as you type them (no Enter needed).
Quit with Ctrl+C.
"""
import os
import select
import sys
import time

import serial

port = sys.argv[1] if len(sys.argv) > 1 else "/dev/cu.usbserial-310"
log_path = os.path.join(os.path.dirname(__file__), "..", ".pio", "monitor.log")
os.makedirs(os.path.dirname(log_path), exist_ok=True)

ser = serial.Serial(port, 115200, timeout=0)
time.sleep(0.3)
ser.reset_input_buffer()  # drop whatever the driver replayed on open

interactive = sys.stdin.isatty()
if interactive:
    import termios
    import tty

    fd = sys.stdin.fileno()
    saved = termios.tcgetattr(fd)
    tty.setcbreak(fd)

print(f"--- Ripple monitor on {port} (Ctrl+C to quit) ---", flush=True)
try:
    with open(log_path, "a") as log:
        log.write(f"\n=== monitor opened {time.strftime('%Y-%m-%d %H:%M:%S')} ===\n")
        while True:
            watch = [ser] + ([sys.stdin] if interactive else [])
            ready, _, _ = select.select(watch, [], [], 0.2)
            if ser in ready:
                data = ser.read(ser.in_waiting or 1)
                text = data.decode(errors="replace")
                sys.stdout.write(text)
                sys.stdout.flush()
                log.write(text)
                log.flush()
            if interactive and sys.stdin in ready:
                ch = os.read(fd, 1)
                if ch == b"\x03":
                    break
                ser.write(ch)
except KeyboardInterrupt:
    pass
finally:
    if interactive:
        termios.tcsetattr(fd, termios.TCSADRAIN, saved)
    ser.close()
