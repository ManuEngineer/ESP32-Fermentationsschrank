"""Offline runner tests; no ESP32/serial interaction."""
import http.server
from pathlib import Path
import tempfile
import threading
import time
import unittest
from types import SimpleNamespace
from unittest.mock import patch
from c0_load import Guard, monitor_uart


class C0RunnerTests(unittest.TestCase):
    def test_abort_shuts_down_inflight_request_and_stops_new_requests(self):
        entered = threading.Event()
        release = threading.Event()

        class Handler(http.server.BaseHTTPRequestHandler):
            def do_GET(self):
                entered.set()
                release.wait(5)
            def log_message(self, *_):
                pass

        server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        server.daemon_threads = True
        serving = threading.Thread(target=server.serve_forever)
        serving.start()
        guard = Guard()
        host = "127.0.0.1:" + str(server.server_port)
        requesting = threading.Thread(target=guard.request, args=(host, "GET", "/"))
        requesting.start()
        try:
            self.assertTrue(entered.wait(1))
            started = time.monotonic()
            guard.abort("uart-anomaly")
            requesting.join(0.5)
            self.assertFalse(requesting.is_alive())
            self.assertLess(time.monotonic() - started, 0.5)
            self.assertEqual(guard.reason, "uart-anomaly")
            self.assertIsNone(guard.request(host, "POST", "/api/v1/logout"))
            self.assertFalse(guard.connections)
        finally:
            release.set()
            server.shutdown()
            server.server_close()
            serving.join()

    def test_uart_monitor_failure_is_fail_closed(self):
        with tempfile.TemporaryDirectory() as folder:
            guard = Guard()
            monitor_uart(Path(folder) / "missing.log", 0, guard, threading.Event(), threading.Event())
            self.assertTrue(guard.stopped.is_set())
            self.assertEqual(guard.reason, "uart-monitor-failure")

    def test_uart_anomaly_stops_immediately_between_request_ticks(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "uart.log"
            path.write_text("old documented boot rst:0x1\n")
            guard = Guard()
            finished = threading.Event()
            monitoring = threading.Thread(target=monitor_uart, args=(path, path.stat().st_size, guard, finished, threading.Event()))
            monitoring.start()
            try:
                with path.open("a") as stream:
                    stream.write("heap_alloc_failed: size=1696\n")
                self.assertTrue(guard.stopped.wait(0.5))
                self.assertEqual(guard.reason, "uart-anomaly")
            finally:
                finished.set()
                monitoring.join(1)
                self.assertFalse(monitoring.is_alive())


HEARTBEAT_LINE = "I application: heartbeat: safe test mode, uptime_ms=1000\n"


class ControlledTimeline:
    """Advance monotonic host time and append capture bytes without real sleeps."""
    def __init__(self, path, steps):
        self.path = path
        self.steps = iter(steps)
        self.now = 0.0
        self.done = False

    def monotonic(self):
        return self.now

    def is_set(self):
        return self.done

    def wait(self, _timeout):
        step = next(self.steps, None)
        if step is None:
            self.done = True
        else:
            self.now, text = step
            with self.path.open("a") as stream:
                stream.write(text)
        return self.done


class C0HeartbeatTests(unittest.TestCase):
    def check_timeline(self, steps):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "uart.log"
            # An old, perfectly valid heartbeat must never satisfy readiness.
            path.write_text(HEARTBEAT_LINE)
            offset = path.stat().st_size
            guard = Guard()
            ready = threading.Event()
            timeline = ControlledTimeline(path, steps)
            monitor_uart(path, offset, guard, timeline, ready,
                         monotonic=timeline.monotonic)
            return guard, ready, timeline.now

    def assert_timeout(self, guard):
        self.assertTrue(guard.stopped.is_set())
        self.assertEqual(guard.reason, "uart-heartbeat-timeout")

    def test_existing_capture_without_new_writes_cannot_supply_first_heartbeat(self):
        guard, ready, _ = self.check_timeline([(5.0, ""), (5.001, "")])
        self.assert_timeout(guard)
        self.assertFalse(ready.is_set())

    def test_capture_stops_after_first_fresh_heartbeat(self):
        guard, ready, _ = self.check_timeline([(1.0, HEARTBEAT_LINE), (6.001, "")])
        self.assert_timeout(guard)
        self.assertTrue(ready.is_set())

    def test_non_heartbeat_lines_do_not_extend_deadline(self):
        guard, ready, _ = self.check_timeline(
            [(1.0, HEARTBEAT_LINE)] + [(t, "I application: diag_30s free=12345\n")
                                     for t in (2.0, 3.0, 4.0, 5.0, 6.001)])
        self.assert_timeout(guard)
        self.assertTrue(ready.is_set())

    def test_fresh_regular_heartbeats_have_no_false_alarm(self):
        guard, ready, _ = self.check_timeline([(float(t), HEARTBEAT_LINE) for t in range(1, 13)])
        self.assertFalse(guard.stopped.is_set())
        self.assertIsNone(guard.reason)
        self.assertTrue(ready.is_set())

    def test_anomaly_between_heartbeats_aborts_without_waiting_for_timeout(self):
        guard, ready, now = self.check_timeline(
            [(1.0, HEARTBEAT_LINE), (1.1, "E heap_alloc_failed: size=1696\n"),
             (2.0, HEARTBEAT_LINE)])
        self.assertTrue(guard.stopped.is_set())
        self.assertEqual(guard.reason, "uart-anomaly")
        self.assertTrue(ready.is_set())
        self.assertEqual(now, 1.1)

    def test_late_first_heartbeat_cannot_forgive_expired_deadline(self):
        guard, ready, _ = self.check_timeline([(5.001, HEARTBEAT_LINE)])
        self.assert_timeout(guard)
        self.assertFalse(ready.is_set())

    def test_run_waits_for_fresh_heartbeat_before_any_login_request(self):
        import c0_load
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            uart = root / "uart.log"
            uart.write_text(HEARTBEAT_LINE)
            (root / "pw").write_text("offline-test-placeholder\n")
            args = SimpleNamespace(run=True, gate_id="OFFLINE_TEST_ONLY",
                                   uart=uart, output=root / "run", host="unused.invalid")
            started = threading.Event()
            result = []
            original_monitor = monitor_uart

            def observe_monitor(*monitor_args):
                started.set()
                original_monitor(*monitor_args)

            with patch("c0_load.monitor_uart", side_effect=observe_monitor), \
                 patch("c0_load.Path.home", return_value=root), \
                 patch.object(c0_load.Guard, "request", return_value=(503, b"", None, 0.0)) as request:
                running = threading.Thread(target=lambda: result.append(c0_load.run(args)))
                running.start()
                try:
                    self.assertTrue(started.wait(1.0))
                    time.sleep(0.08)
                    self.assertTrue(running.is_alive())
                    request.assert_not_called()
                    with uart.open("a") as stream:
                        stream.write(HEARTBEAT_LINE)
                    running.join(1.0)
                    self.assertFalse(running.is_alive())
                    request.assert_called_once()
                    self.assertEqual(result, [1])
                finally:
                    # A fresh anomaly also terminates the thread on assertion failure.
                    with uart.open("a") as stream:
                        stream.write("E heap_alloc_failed: offline-test\n")
                    running.join(1.0)

    def test_run_timeout_records_fail_hold_without_requests_or_password_read(self):
        import c0_load
        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            uart = root / "uart.log"
            uart.write_text(HEARTBEAT_LINE)
            args = SimpleNamespace(run=True, gate_id="OFFLINE_TEST_ONLY",
                                   uart=uart, output=root / "run", host="unused.invalid")

            def controlled_monitor(path, offset, guard, _finished, ready):
                timeline = ControlledTimeline(path, [(5.001, "")])
                monitor_uart(path, offset, guard, timeline, ready,
                             monotonic=timeline.monotonic)

            with patch("c0_load.monitor_uart", side_effect=controlled_monitor), \
                 patch("c0_load.Path.home") as home, \
                 patch.object(c0_load.Guard, "request") as request:
                self.assertEqual(c0_load.run(args), 1)
                request.assert_not_called()
                home.assert_not_called()
            import json
            evidence = json.loads((args.output / "load.json").read_text())
            self.assertEqual(evidence["abort"], "uart-heartbeat-timeout")
            self.assertEqual(evidence["status"], "FAIL")
            self.assertEqual(evidence["evidence_gate"], "HOLD")
            self.assertEqual(evidence["sessions_created"], 0)
            self.assertEqual(evidence["samples"], [])


if __name__ == "__main__":
    unittest.main()
