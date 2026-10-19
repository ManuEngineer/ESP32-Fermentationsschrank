#!/usr/bin/env python3
"""Append-only UART capture for PR203. Usage: capture.py DIR NAME [--reset]
Passive by default (DTR/RTS released before open). Stops when DIR/STOP_NAME exists."""
import os, sys, time, serial
d, name = sys.argv[1], sys.argv[2]
reset = "--reset" in sys.argv
stop = os.path.join(d, "STOP_" + name)
raw = open(os.path.join(d, f"uart_{name}_raw.bin"), "ab", buffering=0)
log = open(os.path.join(d, f"uart_{name}.log"), "ab", buffering=0)
t0 = time.monotonic()
def mark(m): log.write(f"[host {time.strftime('%H:%M:%S')} +{time.monotonic()-t0:8.3f}s] ### {m}\n".encode())
ser = serial.Serial(); ser.port = "/dev/ttyUSB0"; ser.baudrate = 115200; ser.timeout = 0.5
ser.dtr = False; ser.rts = False
ser.open(); mark("port opened (DTR/RTS released before open)")
if reset:
    ser.dtr = False; ser.rts = True; time.sleep(0.15); ser.rts = False
    mark("EN reset pulse via RTS (IO0 high) - deliberate, documented")
buf = b""
while not os.path.exists(stop):
    data = ser.read(4096)
    if not data: continue
    raw.write(data); buf += data
    while b"\n" in buf:
        line, buf = buf.split(b"\n", 1)
        log.write(f"[+{time.monotonic()-t0:8.3f}s] ".encode() + line.rstrip(b"\r") + b"\n")
mark("STOP seen"); ser.close()
