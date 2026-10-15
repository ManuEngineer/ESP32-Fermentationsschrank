"""Offline runner tests; no ESP32/serial interaction."""
import http.client
import http.server
import json
from pathlib import Path
import tempfile
import threading
import time
import unittest
from types import SimpleNamespace
from unittest.mock import Mock, patch
import c0_load
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

    def test_reset_indicator_between_heartbeats_aborts_immediately(self):
        guard, ready, now = self.check_timeline(
            [(1.0, HEARTBEAT_LINE), (1.1, "rst:0xc (SW_CPU_RESET)\n"),
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

            def observe_monitor(*monitor_args, **monitor_kwargs):
                started.set()
                original_monitor(*monitor_args, **monitor_kwargs)

            with patch("c0_load.monitor_uart", side_effect=observe_monitor), \
                 patch("c0_load.Path.home", return_value=root), \
                 patch.object(c0_load.Guard, "request", return_value=(503, b"", None, 0.0, "http-status-error")) as request:
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

            def controlled_monitor(path, offset, guard, _finished, ready, **_kwargs):
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


class C0LoginBudgetTests(unittest.TestCase):
    SENSITIVE_MARKER = "offline-password-cookie-csrf-body-marker"

    def run_script(self, steps):
        """Model socket deadlines with virtual time, through the real Guard/run."""
        clock = [0.0]
        connections = []
        script = iter(steps)

        def connection_factory(_host, timeout):
            duration, outcome = next(script)
            connection = Mock(sock=None)
            connection.timeout = timeout
            connections.append(connection)

            def getresponse():
                clock[0] += min(duration, timeout)
                if duration > timeout:
                    raise TimeoutError(self.SENSITIVE_MARKER)
                if isinstance(outcome, Exception):
                    raise outcome
                return SimpleNamespace(status=outcome,
                                       read=lambda: json.dumps({"csrfToken": self.SENSITIVE_MARKER}).encode(),
                                       getheader=lambda _name: self.SENSITIVE_MARKER + "; HttpOnly")
            connection.getresponse.side_effect = getresponse
            return connection

        def ready_monitor(_path, _offset, _guard, finished, ready, **_kwargs):
            ready.set()
            finished.wait()

        with tempfile.TemporaryDirectory() as folder:
            root = Path(folder)
            uart = root / "uart.log"
            uart.write_text("")
            (root / "pw").write_text(self.SENSITIVE_MARKER)
            args = SimpleNamespace(run=True, gate_id="OFFLINE_TEST_ONLY",
                                   uart=uart, output=root / "run", host="unused.invalid")
            with patch("c0_load.monitor_uart", side_effect=ready_monitor), \
                 patch("c0_load.Path.home", return_value=root), \
                 patch("c0_load.http.client.HTTPConnection", side_effect=connection_factory), \
                 patch("c0_load.time.monotonic", side_effect=lambda: clock[0]):
                exit_code = c0_load.run(args)
            raw = (args.output / "load.json").read_text()
            self.assertNotIn(self.SENSITIVE_MARKER, raw)
            return exit_code, json.loads(raw), connections

    def test_login_over_three_seconds_within_eight_is_accepted(self):
        # The second login times out deliberately to stop before real idle/load waits.
        exit_code, evidence, connections = self.run_script([(3.2, 200), (8.001, 200)])
        self.assertEqual(evidence["sessions_created"], 1)
        self.assertEqual(evidence["samples"][0]["status"], 200)
        self.assertAlmostEqual(evidence["samples"][0]["milliseconds"], 3200)
        self.assertIsNone(evidence["samples"][0]["error_class"])
        self.assertEqual([c.timeout for c in connections], [8.0, 8.0])
        self.assertEqual(exit_code, 1)
        self.assertEqual(len(evidence["samples"]), 2)  # no retry after the failure

    def test_login_over_eight_seconds_records_timeout_fail_hold_without_retry(self):
        exit_code, evidence, connections = self.run_script([(8.001, 200)])
        self.assertEqual(exit_code, 1)
        self.assertEqual(evidence["status"], "FAIL")
        self.assertEqual(evidence["evidence_gate"], "HOLD")
        self.assertEqual(evidence["abort"], "request-failure")
        self.assertEqual(evidence["sessions_created"], 0)
        self.assertEqual(evidence["samples"][0]["error_class"], "timeout")
        self.assertAlmostEqual(evidence["samples"][0]["milliseconds"], 8000)
        self.assertEqual(len(connections), 1)
        self.assertEqual(evidence["client_socket_timeout_seconds"], {
            "POST /api/v1/login": 8.0,
            "other requests (including API/shell GETs)": 3.0})

    def test_only_exact_login_post_gets_eight_seconds(self):
        for method, path in (("GET", "/api/v1/status"), ("GET", "/api/v1/temperatures"),
                             ("GET", "/api/v1/alerts"), ("GET", "/"),
                             ("GET", "/api/v1/login"), ("POST", "/api/v1/logout")):
            with self.subTest(method=method, path=path):
                connection = Mock(sock=None)
                connection.getresponse.side_effect = TimeoutError(self.SENSITIVE_MARKER)
                with patch("c0_load.http.client.HTTPConnection", return_value=connection) as factory:
                    guard = Guard()
                    result = guard.request("unused.invalid", method, path)
                    factory.assert_called_once_with("unused.invalid", timeout=3.0)
                    self.assertEqual(result[4], "timeout")
                    self.assertEqual(guard.reason, "request-failure")
                    self.assertIsNone(guard.request("unused.invalid", method, path))

    def test_transport_error_classes_are_fixed_and_secret_free(self):
        for error, expected in ((ConnectionResetError(self.SENSITIVE_MARKER), "connection-error"),
                                (http.client.BadStatusLine(self.SENSITIVE_MARKER), "http-protocol-error")):
            with self.subTest(error_class=expected):
                exit_code, evidence, connections = self.run_script([(0.1, error)])
                self.assertEqual(exit_code, 1)
                self.assertEqual(evidence["abort"], "request-failure")
                self.assertEqual(evidence["status"], "FAIL")
                self.assertEqual(evidence["evidence_gate"], "HOLD")
                self.assertEqual(evidence["samples"][0]["error_class"], expected)
                self.assertEqual(len(connections), 1)

    def test_connect_timeout_is_distinguished_from_read_timeout_and_not_abort_induced(self):
        connect_stall = Mock(sock=None)
        connect_stall.connect.side_effect = TimeoutError(self.SENSITIVE_MARKER)
        read_stall = Mock(sock=None)
        read_stall.getresponse.side_effect = TimeoutError(self.SENSITIVE_MARKER)
        for connection, phase in ((connect_stall, "connect"), (read_stall, "io")):
            with self.subTest(phase=phase), \
                 patch("c0_load.http.client.HTTPConnection", return_value=connection):
                guard = Guard()
                result = guard.request("unused.invalid", "GET", "/api/v1/status")
                self.assertEqual(result[4], "timeout")
                self.assertEqual(result[6], phase)
                self.assertFalse(result[7])  # own finding, no abort had shut it down
                self.assertIsInstance(result[5], float)  # start epoch

    def test_connection_shut_down_by_abort_is_marked_abort_induced(self):
        guard = Guard()
        connection = Mock(sock=Mock())
        started = threading.Event()

        def stall():
            started.set()
            guard.stopped.wait(5.0)
            raise http.client.RemoteDisconnected(self.SENSITIVE_MARKER)
        connection.getresponse.side_effect = stall
        results = []
        with patch("c0_load.http.client.HTTPConnection", return_value=connection):
            worker = threading.Thread(
                target=lambda: results.append(guard.request("unused.invalid", "GET", "/")))
            worker.start()
            self.assertTrue(started.wait(5.0))
            guard.abort("g1-supervisor-stop")
            worker.join(5.0)
        self.assertEqual(guard.reason, "g1-supervisor-stop")
        self.assertIsNotNone(guard.abort_epoch)
        self.assertIsNotNone(guard.abort_monotonic)
        self.assertEqual(results[0][4], "http-protocol-error")
        self.assertTrue(results[0][7])
        # the first reason stays: the request's own failure must not replace it
        self.assertEqual(guard.reason, "g1-supervisor-stop")

    def test_abort_induced_needs_a_socket_the_guard_actually_shut_down(self):
        for label, sock in (("inactive", None),
                            ("shutdown-fails", Mock(shutdown=Mock(side_effect=OSError("closed"))))):
            with self.subTest(label):
                guard = Guard()
                connection = Mock(sock=sock)
                guard.connections.add(connection)
                guard.close_sockets()
                self.assertNotIn(connection, guard.shut_down)
        guard = Guard()
        active = Mock(sock=Mock())
        guard.connections.add(active)
        guard.close_sockets()
        active.sock.shutdown.assert_called_once()
        self.assertIn(active, guard.shut_down)

    def test_load_json_records_abort_time_and_sample_start(self):
        exit_code, evidence, _ = self.run_script([(8.001, 200)])
        self.assertEqual(exit_code, 1)
        self.assertIsNotNone(evidence["abort_epoch"])
        self.assertIsNotNone(evidence["abort_monotonic"])
        sample = evidence["samples"][0]
        self.assertIn("start_epoch", sample)
        self.assertLessEqual(sample["start_epoch"], sample["epoch"])
        self.assertIn(sample["error_phase"], ("connect", "io"))
        self.assertFalse(sample["abort_induced"])

    def test_real_http_503_stops_without_retry_or_secret_evidence(self):
        requests = []
        secret = self.SENSITIVE_MARKER

        class Handler(http.server.BaseHTTPRequestHandler):
            def do_POST(self):
                requests.append(self.path)
                self.rfile.read(int(self.headers["Content-Length"]))
                self.send_response(503)
                self.send_header("Set-Cookie", secret)
                self.end_headers()
                self.wfile.write(secret.encode())
            def log_message(self, *_):
                pass

        def ready_monitor(_path, _offset, _guard, finished, ready, **_kwargs):
            ready.set()
            finished.wait()

        server = http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler)
        serving = threading.Thread(target=server.serve_forever)
        serving.start()
        try:
            with tempfile.TemporaryDirectory() as folder:
                root = Path(folder)
                uart = root / "uart.log"
                uart.write_text("")
                (root / "pw").write_text(secret)
                args = SimpleNamespace(run=True, gate_id="OFFLINE_TEST_ONLY",
                                       uart=uart, output=root / "run",
                                       host="127.0.0.1:" + str(server.server_port))
                with patch("c0_load.monitor_uart", side_effect=ready_monitor), \
                     patch("c0_load.Path.home", return_value=root):
                    self.assertEqual(c0_load.run(args), 1)
                raw = (args.output / "load.json").read_text()
                self.assertNotIn(secret, raw)
                evidence = json.loads(raw)
                self.assertEqual(evidence["abort"], "http-status-failure")
                self.assertEqual(evidence["status"], "FAIL")
                self.assertEqual(evidence["evidence_gate"], "HOLD")
                self.assertEqual(evidence["samples"][0]["status"], 503)
                self.assertEqual(evidence["samples"][0]["error_class"], "http-status-error")
                self.assertEqual(requests, ["/api/v1/login"])
        finally:
            server.shutdown()
            server.server_close()
            serving.join()


class G1ProbeFailureTest(unittest.TestCase):
    def test_g1_probe_error_is_uart_anomaly(self):
        self.assertIsNotNone(c0_load.BAD.search("E g1_probe_error: aggregate_overflow dropped=1"))

class G1SupervisorTest(unittest.TestCase):
    def test_abort_file_closes_guard_without_any_uart_or_request(self):
        with tempfile.TemporaryDirectory() as d:
            path=Path(d)/"uart";path.write_text("")
            abort=Path(d)/"ABORT";abort.write_text("stop")
            guard=Guard()
            monitor_uart(path,0,guard,threading.Event(),threading.Event(),abort_file=abort)
            self.assertTrue(guard.stopped.is_set())
            self.assertEqual("g1-supervisor-stop",guard.reason)

    def test_all_watchdog_and_probe_markers_stop(self):
        for line in ("watchdog", "Interrupt wdt timeout", "g1_probe_error"):
            self.assertIsNotNone(c0_load.BAD.search(line))

if __name__ == "__main__":
    unittest.main()


class ObservationModeTests(unittest.TestCase):
    """Explicitly enabled diagnosis mode: default hard stop stays unchanged (see the tests above)."""
    SECRET = "offline-secret-marker"

    def timeout_request(self, guard, phase="io", method="GET", path="/api/v1/status"):
        connection = Mock(sock=None)
        if phase == "connect":
            connection.connect.side_effect = TimeoutError(self.SECRET)
        else:
            connection.getresponse.side_effect = TimeoutError(self.SECRET)
        with patch("c0_load.http.client.HTTPConnection", return_value=connection):
            return guard.request("unused.invalid", method, path)

    def assert_no_timer_threads(self):
        deadline = time.monotonic() + 2.0
        while time.monotonic() < deadline and any(
                isinstance(t, threading.Timer) and t.is_alive() for t in threading.enumerate()):
            time.sleep(0.01)
        self.assertEqual([], [t for t in threading.enumerate() if isinstance(t, threading.Timer) and t.is_alive()])

    def test_default_mode_is_the_unchanged_hard_stop(self):
        guard = Guard()
        result = self.timeout_request(guard)
        self.assertIsNone(guard.observe_window)
        self.assertEqual((-1, "timeout", False), (result[0], result[4], result[8]))
        self.assertTrue(guard.stopped.is_set())
        self.assertEqual("request-failure", guard.reason)
        self.assertIsNone(guard.first_failure)

    def test_single_timeout_is_observed_for_the_window_then_fail_hold(self):
        for phase in ("io", "connect"):
            with self.subTest(phase=phase):
                guard = Guard(observe_window_seconds=0.2)
                result = self.timeout_request(guard, phase)
                self.assertTrue(result[8])  # observed, not aborted
                self.assertFalse(result[7])
                self.assertFalse(guard.stopped.is_set())
                self.assertEqual(phase, guard.first_failure["phase"])
                self.assertTrue(guard.stopped.wait(2.0))
                self.assertEqual("observation-window-end", guard.reason)
                summary = guard.observation_summary()
                self.assertEqual("FAIL/HOLD", summary["final_state"])
                self.assertTrue(0.15 <= summary["observation_seconds"] < 1.5)
                self.assertEqual(1, summary["network_timeout_count_before_stop"])
                self.assertNotIn(self.SECRET, json.dumps(summary))
        self.assert_no_timer_threads()

    def test_timeout_storm_stops_hard_at_the_eighth_genuine_timeout(self):
        guard = Guard(observe_window_seconds=60.0)
        for count in range(1, 8):
            self.timeout_request(guard, "connect")
            self.assertFalse(guard.stopped.is_set(), count)
        self.timeout_request(guard, "io")
        self.assertTrue(guard.stopped.is_set())
        self.assertEqual("observation-timeout-limit", guard.reason)
        self.assertEqual(8, guard.observation_summary()["network_timeout_count_before_stop"])
        self.assert_no_timer_threads()  # the 60 s timer was cancelled by the abort

    def test_everything_but_a_genuine_load_timeout_still_stops_immediately(self):
        cases = ((ConnectionResetError(self.SECRET), "GET", "/api/v1/status"),
                 (http.client.BadStatusLine(self.SECRET), "GET", "/"),
                 (TimeoutError(self.SECRET), "POST", "/api/v1/login"))  # login/auth is never observed
        for error, method, path in cases:
            with self.subTest(error=type(error).__name__, path=path):
                guard = Guard(observe_window_seconds=60.0)
                connection = Mock(sock=None)
                connection.getresponse.side_effect = error
                with patch("c0_load.http.client.HTTPConnection", return_value=connection):
                    result = guard.request("unused.invalid", method, path)
                self.assertFalse(result[8])
                self.assertTrue(guard.stopped.is_set())
                self.assertEqual("request-failure", guard.reason)
                self.assertIsNone(guard.first_failure)
        self.assert_no_timer_threads()

    def test_uart_or_supervisor_stop_during_the_window_wins_and_cancels_the_timer(self):
        guard = Guard(observe_window_seconds=0.3)
        self.timeout_request(guard)
        guard.abort("uart-anomaly")  # heap_alloc_failed, panic, watchdog, ... from the monitor
        self.assertEqual("uart-anomaly", guard.reason)
        time.sleep(0.5)
        self.assertEqual("uart-anomaly", guard.reason)  # the window end did not overwrite it
        self.assertEqual("FAIL/HOLD", guard.observation_summary()["final_state"])
        self.assert_no_timer_threads()

    def test_abort_induced_errors_are_not_counted_as_network_timeouts(self):
        guard = Guard(observe_window_seconds=60.0)
        self.timeout_request(guard)
        victim = Mock(sock=Mock())
        guard.connections.add(victim)
        victim.getresponse.side_effect = TimeoutError(self.SECRET)
        guard.close_sockets()  # marks the connection as shut down by the guard
        with patch("c0_load.http.client.HTTPConnection", return_value=victim):
            result = guard.request("unused.invalid", "GET", "/")
        self.assertTrue(result[7])  # abort-induced
        self.assertFalse(result[8])
        self.assertEqual(1, len(guard.network_timeouts))
        guard.abort("observation-window-end")
        self.assert_no_timer_threads()

    def test_timeouts_after_the_stop_are_recorded_but_do_not_count(self):
        guard = Guard(observe_window_seconds=60.0)
        self.timeout_request(guard)
        guard.abort("g1-supervisor-stop")
        result = self.timeout_request(guard, "connect")
        self.assertIsNone(result)  # stopped guards issue no further request
        guard.observe_timeout(time.time(), "connect")
        summary = guard.observation_summary()
        self.assertEqual(1, summary["network_timeout_count_before_stop"])
        self.assertEqual(2, len(summary["network_timeouts"]))
        self.assertTrue(summary["network_timeouts"][1]["after_stop"])

    def test_load_end_after_an_observed_failure_stops_without_a_recovery_claim(self):
        quiet = Guard(observe_window_seconds=60.0)
        quiet.finish_observation()
        self.assertFalse(quiet.stopped.is_set())
        guard = Guard(observe_window_seconds=60.0)
        self.timeout_request(guard)
        guard.finish_observation()
        self.assertTrue(guard.stopped.is_set())
        self.assertEqual("observation-load-end", guard.reason)
        self.assert_no_timer_threads()
