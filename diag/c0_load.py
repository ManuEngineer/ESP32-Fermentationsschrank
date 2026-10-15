#!/usr/bin/env python3
"""G1 P-A runner derived from cfcf282 C0 runner. Explicit --run after G1 approval; never opens UART or flashes.
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

BAD = re.compile(r"heap_alloc_failed|abort\(\)|panic|Guru Meditation|task_wdt|Brownout|stack overflow|watchdog|interrupt wdt|wdt timeout|rst:0x|g1_probe_error", re.I)
HEARTBEAT = re.compile(r"heartbeat: safe test mode, uptime_ms=\d+\b")
LOGIN_TIMEOUT_SECONDS = 8.0
REQUEST_TIMEOUT_SECONDS = 3.0
# Explicitly enabled observation mode (diagnosis only, not a product safety or acceptance threshold):
# after the first genuine network timeout during the load the run is irreversibly FAIL/HOLD but keeps
# observing for at most OBSERVE_WINDOW_SECONDS, and stops hard at the OBSERVE_MAX_TIMEOUTS-th genuine
# timeout. The default hard-stop behaviour is unchanged.
OBSERVE_WINDOW_SECONDS = 20.0
OBSERVE_MAX_TIMEOUTS = 8
MAX_SAMPLES = 20000  # fail-closed volume limit for the evidence file


class Guard:
    def __init__(self, observe_window_seconds=None, observe_max_timeouts=OBSERVE_MAX_TIMEOUTS):
        self.observe_window = observe_window_seconds  # None = default hard-stop mode
        self.observe_max = observe_max_timeouts
        self.first_failure = None
        self.network_timeouts = []
        self._timer = None
        self.stopped = threading.Event()
        self.lock = threading.Lock()
        self.connections = set()
        self.shut_down = set()
        self.reason = None
        self.abort_epoch = None
        self.abort_monotonic = None

    def close_sockets(self):
        with self.lock:
            connections = list(self.connections)
        for connection in connections:
            try:
                active_socket = connection.sock
                if active_socket is not None:
                    # Recorded before the shutdown: the failing request thread may
                    # classify itself the instant the shutdown takes effect.
                    with self.lock:
                        self.shut_down.add(connection)
                    try:
                        active_socket.shutdown(socket.SHUT_RDWR)
                    except OSError:
                        with self.lock:
                            self.shut_down.discard(connection)
                        raise
            except OSError:
                pass
            connection.close()

    def abort(self, reason):
        with self.lock:
            if self.reason is None:
                self.reason = reason
                self.abort_epoch = time.time()
                self.abort_monotonic = time.monotonic()
            self.stopped.set()
            timer = self._timer
        if timer is not None:
            timer.cancel()
        self.close_sockets()

    def observe_timeout(self, started_epoch, phase):
        """Record a genuine (not abort-induced) network timeout of a load request.
        Returns True while the run is still being observed, False when it is stopped."""
        event = {"epoch": time.time(), "start_epoch": started_epoch, "phase": phase,
                 "after_stop": self.stopped.is_set()}
        with self.lock:
            self.network_timeouts.append(event)
            if event["after_stop"]:
                return False
            if self.first_failure is None:
                self.first_failure = dict(event)
                self._timer = threading.Timer(self.observe_window,
                                              lambda: self.abort("observation-window-end"))
                self._timer.daemon = True
                self._timer.start()
            count = sum(1 for item in self.network_timeouts if not item["after_stop"])
        if count >= self.observe_max:
            self.abort("observation-timeout-limit")
        return True

    def finish_observation(self):
        """The planned load ended after an observed failure: no recovery claim, stop."""
        if self.first_failure is not None and not self.stopped.is_set():
            self.abort("observation-load-end")

    def observation_summary(self):
        first = self.first_failure
        return {"enabled": self.observe_window is not None,
                "window_seconds": self.observe_window, "max_network_timeouts": self.observe_max,
                "first_network_timeout": first,
                "network_timeouts": self.network_timeouts,
                "network_timeout_count_before_stop": sum(1 for e in self.network_timeouts if not e["after_stop"]),
                "stop_reason": self.reason,
                "observation_seconds": (round(self.abort_epoch - first["epoch"], 3)
                                        if first and self.abort_epoch else None),
                "final_state": "FAIL/HOLD" if first or self.stopped.is_set() else None,
                "note": "diagnosis window only; no acceptance or recovery claim after a first failure"}

    def request(self, host, method, path, body=None, cookie=None):
        if self.stopped.is_set():
            return None
        timeout = (LOGIN_TIMEOUT_SECONDS if method == "POST" and path == "/api/v1/login"
                   else REQUEST_TIMEOUT_SECONDS)
        connection = http.client.HTTPConnection(host, timeout=timeout)
        started = time.monotonic()
        started_epoch = time.time()
        phase = "connect"
        try:
            connection.connect()
            phase = "io"
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
            return (response.status, payload, response.getheader("Set-Cookie"),
                    (time.monotonic() - started) * 1000,
                    None if response.status == 200 else "http-status-error",
                    started_epoch, phase, False, False)
        except (OSError, http.client.HTTPException) as error:
            # Fixed classes only: never serialize the exception or its message.
            error_class = ("timeout" if isinstance(error, TimeoutError) else
                           "http-protocol-error" if isinstance(error, http.client.HTTPException)
                           else "connection-error")
            # Only a connection that close_sockets() shut down fails *because of*
            # an abort; a connect()-phase failure is a client finding of its own.
            with self.lock:
                abort_induced = connection in self.shut_down
            observed = False
            if (self.observe_window is not None and error_class == "timeout" and not abort_induced
                    and not (method == "POST" and path == "/api/v1/login")):
                observed = self.observe_timeout(started_epoch, phase)
            if not observed:
                self.abort("request-failure")
            return (-1, b"", None, (time.monotonic() - started) * 1000, error_class,
                    started_epoch, phase, abort_induced, observed)
        finally:
            with self.lock:
                self.connections.discard(connection)
            connection.close()


def monitor_uart(path, offset, guard, finished, heartbeat_ready, *,
                 heartbeat_timeout=5.0, monotonic=time.monotonic, abort_file=None):
    last_heartbeat = monotonic()
    try:
        with path.open(errors="replace") as stream:
            stream.seek(offset)
            pending = ""
            while not guard.stopped.is_set():
                if abort_file is not None and abort_file.exists():
                    guard.abort("g1-supervisor-stop")
                    return
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
        raise SystemExit("No run: --run and approved --gate-id are required (G1).")
    if not args.uart.is_file():
        raise SystemExit("Existing live UART log required; this driver never opens a serial port.")
    args.output.mkdir(parents=True, exist_ok=False)
    observe = bool(getattr(args, "observe", False))
    guard = Guard(observe_window_seconds=getattr(args, "observe_window_seconds", OBSERVE_WINDOW_SECONDS) if observe else None,
                  observe_max_timeouts=getattr(args, "observe_max_timeouts", OBSERVE_MAX_TIMEOUTS))
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
        (status, payload, set_cookie, milliseconds, error_class,
         started_epoch, error_phase, abort_induced, observed) = result
        with samples_lock:
            # "epoch" stays the request END (compatible with earlier evidence).
            samples.append({"epoch": time.time(), "start_epoch": started_epoch,
                            "kind": kind, "session": index,
                            "status": status, "milliseconds": milliseconds,
                            "error_class": error_class,
                            "error_phase": error_phase if error_class else None,
                            "abort_induced": abort_induced,
                            "observed_network_timeout": observed})
            too_many = len(samples) > MAX_SAMPLES
        if too_many:
            guard.abort("sample-volume-limit")  # fail closed instead of dropping evidence
        if status != 200 and not observed:
            guard.abort("http-status-failure")
        return status, payload, set_cookie

    monitor = threading.Thread(target=monitor_uart,
                               args=(args.uart, args.uart.stat().st_size, guard,
                                     finished, heartbeat_ready),
                               kwargs={"abort_file": getattr(args,"abort_file",None)})
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
            guard.finish_observation()
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
        failed = guard.stopped.is_set() or guard.first_failure is not None
        document = {"profile": "P-A", "gate": args.gate_id,
                    "sessions_created": len(sessions), "duration_load_seconds": 1200,
                    "idle_reference_seconds": 60, "recovery_seconds": 300,
                    "session_policy": "no logout; no re-login; same four sessions",
                    "connection_policy": "new per request; Connection close; no retry",
                    "client_socket_timeout_seconds": {
                        "POST /api/v1/login": LOGIN_TIMEOUT_SECONDS,
                        "other requests (including API/shell GETs)": REQUEST_TIMEOUT_SECONDS},
                    "monitoring_policy": "fresh heartbeat before login; max 5 s gap throughout all phases",
                    "status": "FAIL" if failed else "COMPLETED_PENDING_REVIEW",
                    "evidence_gate": "HOLD" if failed else "NOT_EVALUATED",
                    "observation": guard.observation_summary(),
                    "abort": guard.reason, "abort_epoch": guard.abort_epoch,
                    "abort_monotonic": guard.abort_monotonic,
                    "marks": marks, "samples": samples}
        (args.output / "load.json").write_text(json.dumps(document, indent=2))
        sessions.clear()
    return 1 if (guard.stopped.is_set() or guard.first_failure is not None) else 0


def parse_args():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--run", action="store_true")
    parser.add_argument("--gate-id", help="Owner-approved G1 identifier; never implies approval by itself")
    parser.add_argument("--host", required=True)
    parser.add_argument("--uart", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--abort-file",type=Path)
    parser.add_argument("--observe-network-timeouts", dest="observe", action="store_true",
                        help="diagnosis only: keep observing at most 20 s after the first genuine network timeout "
                             "(hard stop at the 8th); default is the unchanged hard stop")
    return parser.parse_args()


if __name__ == "__main__":
    raise SystemExit(run(parse_args()))
