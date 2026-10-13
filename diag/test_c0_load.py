"""Offline runner tests; no ESP32/serial interaction."""
import http.server
from pathlib import Path
import tempfile
import threading
import time
import unittest
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
            monitor_uart(Path(folder) / "missing.log", 0, guard, threading.Event())
            self.assertTrue(guard.stopped.is_set())
            self.assertEqual(guard.reason, "uart-monitor-failure")

    def test_uart_anomaly_stops_immediately_between_request_ticks(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder) / "uart.log"
            path.write_text("old documented boot rst:0x1\n")
            guard = Guard()
            finished = threading.Event()
            monitoring = threading.Thread(target=monitor_uart, args=(path, path.stat().st_size, guard, finished))
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


if __name__ == "__main__":
    unittest.main()
