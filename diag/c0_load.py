#!/usr/bin/env python3
"""C0 P-A runner. Explicit --run after G0 approval; never opens UART or flashes.
Four logins, idle reference, read-only load, socket close, four-session recovery.
Only statuses/timings/resource context are written; secrets remain in memory.
"""
import argparse
import concurrent.futures
import http.client
import json
from pathlib import Path
import re
import socket
import threading
import time

BAD = re.compile(r"heap_alloc_failed|abort\(\)|panic|Guru Meditation|task_wdt|Brownout|stack overflow|rst:0x", re.I)
HEARTBEAT = re.compile(r"heartbeat: safe test mode, uptime_ms=\d+\b")


class Guard:
    def __init__(self):
        self.stopped = threading.Event()
        self.lock = threading.Lock()
        self.connections = set()
        self.reason = None

    def close_sockets(self):
        with self.lock:
            connections = list(self.connections)
        for connection in connections:
            try:
                active_socket = connection.sock
                if active_socket is not None:
                    active_socket.shutdown(socket.SHUT_RDWR)
            except OSError:
                pass
            connection.close()

    def abort(self, reason):
        with self.lock:
            if self.reason is None:
                self.reason = reason
            self.stopped.set()
        self.close_sockets()

    def request(self, host, method, path, body=None, cookie=None):
        if self.stopped.is_set():
            return None
        connection = http.client.HTTPConnection(host, timeout=3.0)
        started = time.monotonic()
        try:
            connection.connect()
            with self.lock:
                self.connections.add(connection)
            if self.stopped.is_set():
                return None
            headers = {"Host": host, "Origin": "http://" + host, "Connection": "close"}
            if cookie:
                headers["Cookie"] = cookie
            if body is not None:
                headers["Content-Type"] = "application/json"
            connection.request(method, path, body=body, headers=headers)
            response = connection.getresponse()
            payload = response.read()
            return response.status, payload, response.getheader("Set-Cookie"), (time.monotonic() - started) * 1000
        except (OSError, http.client.HTTPException):
            self.abort("request-failure")
            return -1, b"", None, (time.monotonic() - started) * 1000
        finally:
            with self.lock:
                self.connections.discard(connection)
            connection.close()


def monitor_uart(path, offset, guard, finished, heartbeat_ready, *,
                 heartbeat_timeout=5.0, monotonic=time.monotonic):
    last_heartbeat = monotonic()
    try:
        with path.open(errors="replace") as stream:
            stream.seek(offset)
            pending = ""
            while not guard.stopped.is_set():
                # Check before reading: a late heartbeat cannot forgive a gap.
                if monotonic() - last_heartbeat > heartbeat_timeout:
                    guard.abort("uart-heartbeat-timeout")
                    return
                if finished.is_set():
                    return
                pending += stream.read()
                lines = pending.split("\n")
                pending = lines.pop()
                if any(BAD.search(line) for line in lines) or BAD.search(pending):
                    guard.abort("uart-anomaly")
                    return
                if any(HEARTBEAT.search(line) for line in lines):
                    last_heartbeat = monotonic()
                    heartbeat_ready.set()
                finished.wait(0.05)
    except OSError:
        guard.abort("uart-monitor-failure")


def run(args):
    if not args.run or not args.gate_id:
        raise SystemExit("No run: --run and approved --gate-id are required (G0).")
    if not args.uart.is_file():
        raise SystemExit("Existing live UART log required; this driver never opens a serial port.")
    args.output.mkdir(parents=True, exist_ok=False)
    guard = Guard()
    finished = threading.Event()
    heartbeat_ready = threading.Event()
    samples = []
    samples_lock = threading.Lock()
    marks = []

    def mark(name):
        marks.append({"kind": name, "epoch": time.time(), "monotonic": time.monotonic()})

    def request(kind, method, path, index, cookie=None, body=None):
        result = guard.request(args.host, method, path, body, cookie)
        if result is None:
            return None
        status, payload, set_cookie, milliseconds = result
        with samples_lock:
            samples.append({"epoch": time.time(), "kind": kind, "session": index,
                            "status": status, "milliseconds": milliseconds})
        if status != 200:
            guard.abort("http-status-failure")
        return status, payload, set_cookie

    monitor = threading.Thread(target=monitor_uart,
                               args=(args.uart, args.uart.stat().st_size, guard,
                                     finished, heartbeat_ready))
    monitor.start()
    sessions = []
    try:
        mark("uart-heartbeat-wait")
        while not heartbeat_ready.wait(0.05):
            if guard.stopped.is_set():
                break
        if guard.stopped.is_set():
            return 1
        mark("uart-heartbeat-ready")
        password = (Path.home() / "pw").read_text().splitlines()[0]
        mark("login-start")
        for index in range(4):
            result = request("login", "POST", "/api/v1/login", index,
                             body=json.dumps({"password": password}))
            if result is None or guard.stopped.is_set():
                break
            status, payload, cookie = result
            if status != 200 or not cookie:
                guard.abort("login-failure")
                break
            try:
                if not json.loads(payload).get("csrfToken"):
                    guard.abort("login-metadata-failure")
                    break
            except (ValueError, TypeError):
                guard.abort("login-metadata-failure")
                break
            sessions.append(cookie.split(";")[0])
            if guard.stopped.wait(1.0):
                break
        password = None
        mark("four-session-idle-reference-start")
        if len(sessions) == 4 and not guard.stopped.wait(60.0):
            mark("four-session-idle-reference-end-load-start")
            base = time.monotonic() + 2.0
            stop = base + 1200.0

            def poll(index, cookie):
                tick = 0
                with concurrent.futures.ThreadPoolExecutor(max_workers=4) as requests:
                    while not guard.stopped.is_set():
                        due = base + index * 1.2 + tick * 5.0
                        if due >= stop or guard.stopped.wait(max(0, due - time.monotonic())):
                            return
                        if time.monotonic() >= stop:
                            return
                        batch = [requests.submit(request, kind, "GET", "/api/v1/" + kind, index, cookie)
                                 for kind in ("status", "temperatures", "alerts")]
                        if tick % 6 == 1:
                            batch.append(requests.submit(request, "shell", "GET", "/", index, cookie))
                        for future in batch:
                            future.result()
                        tick += 1
            with concurrent.futures.ThreadPoolExecutor(max_workers=4) as workers:
                futures = [workers.submit(poll, index, cookie) for index, cookie in enumerate(sessions)]
                for future in futures:
                    future.result()
            guard.close_sockets()
            mark("load-ended-client-sockets-closed-no-logout")
            if not guard.stopped.is_set():
                guard.stopped.wait(300.0)
                mark("four-session-recovery-end")
    except Exception:
        # Never serialize exceptions that could contain a cookie or password.
        guard.abort("runner-error")
    finally:
        guard.close_sockets()
        finished.set()
        monitor.join(timeout=1.0)
        document = {"profile": "P-A", "gate": args.gate_id,
                    "sessions_created": len(sessions), "duration_load_seconds": 1200,
                    "idle_reference_seconds": 60, "recovery_seconds": 300,
                    "session_policy": "no logout; no re-login; same four sessions",
                    "connection_policy": "new per request; Connection close; 3 s timeout",
                    "monitoring_policy": "fresh heartbeat before login; max 5 s gap throughout all phases",
                    "status": "FAIL" if guard.stopped.is_set() else "COMPLETED_PENDING_REVIEW",
                    "evidence_gate": "HOLD" if guard.stopped.is_set() else "NOT_EVALUATED",
                    "abort": guard.reason, "marks": marks, "samples": samples}
        (args.output / "load.json").write_text(json.dumps(document, indent=2))
        sessions.clear()
    return 1 if guard.stopped.is_set() else 0


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--run", action="store_true")
    parser.add_argument("--gate-id", help="Owner-approved G0 identifier; never implies approval by itself")
    parser.add_argument("--host", required=True)
    parser.add_argument("--uart", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    return parser.parse_args()


if __name__ == "__main__":
    raise SystemExit(run(parse_args()))
