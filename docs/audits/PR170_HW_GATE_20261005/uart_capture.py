#!/usr/bin/env python3
"""Detached, append-only UART capture. Stops ONLY when the STOP file exists."""
import os, sys, time, fcntl
import serial

DIR = "/tmp/pr170-evidence"
PORT = "/dev/ttyUSB0"
RAW = os.path.join(DIR, "uart_raw.bin")
STAMPED = os.path.join(DIR, "uart_stamped.log")
STOP = os.path.join(DIR, "STOP")
LOCK = os.path.join(DIR, "capture.lock")

lock = open(LOCK, "w")
try:
    fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
except OSError:
    sys.exit("capture already running")
lock.write(str(os.getpid())); lock.flush()

raw = open(RAW, "ab", buffering=0)
stamped = open(STAMPED, "ab", buffering=0)
t0 = time.monotonic()

def mark(msg):
    stamped.write(f"[host {time.strftime('%H:%M:%S')} +{time.monotonic()-t0:9.3f}s] ### {msg}\n".encode())

mark("capture start (opening the port resets the board)")
buf = b""
did_reset = False
while not os.path.exists(STOP):
    try:
        ser = serial.Serial()
        ser.port = PORT; ser.baudrate = 115200; ser.timeout = 0.5
        ser.open()
        mark("port opened")
        if not did_reset:
            ser.dtr = False; ser.rts = True; time.sleep(0.15); ser.rts = False
            mark("EN reset pulse issued via RTS (IO0 high)")
            did_reset = True
        while not os.path.exists(STOP):
            data = ser.read(4096)
            if not data:
                continue
            raw.write(data)
            buf += data
            while b"\n" in buf:
                line, buf = buf.split(b"\n", 1)
                stamped.write(f"[+{time.monotonic()-t0:9.3f}s] ".encode() + line.rstrip(b"\r") + b"\n")
    except Exception as exc:  # reconnect, never exit
        mark(f"port error: {exc!r}; retrying")
        time.sleep(1.0)
mark("STOP file seen, capture ended by owner request")
