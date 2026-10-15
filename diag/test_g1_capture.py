import json
import tempfile
from pathlib import Path
from types import SimpleNamespace
import unittest
from unittest.mock import patch
import g1_capture

BOOT = b'rst:0x1 (POWERON_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)\n'


class G1CaptureTest(unittest.TestCase):
    def test_missing_gate_cannot_open_uart_or_create_capture(self):
        args=SimpleNamespace(run=False,gate_id=None,allow_port_open_boot=False)
        with self.assertRaises(SystemExit):
            g1_capture.capture(args)

    def test_silent_uart_is_fail_closed_without_real_wait_or_device(self):
        class Serial:
            is_open=False
            def open(self): self.is_open=True
            def read(self,n): return b''
            def close(self): self.is_open=False
        serial=Serial()
        with tempfile.TemporaryDirectory() as d:
            output=Path(d)/'run'
            args=SimpleNamespace(run=True,gate_id='OFFLINE_TEST_ONLY',allow_port_open_boot=True,
                port='OFFLINE_FAKE',output=output,load=False,expected_source='0'*40,host='OFFLINE_FAKE')
            clock=iter([0,0,6])
            with patch.dict('sys.modules',{'serial':SimpleNamespace(Serial=lambda **kw:serial)}), patch.object(g1_capture.time,'monotonic',side_effect=lambda:next(clock,6)):
                result=g1_capture.capture(args)
            self.assertEqual(1,result)
            self.assertEqual('uart-heartbeat-timeout\n',(output/'ABORT').read_text())
            self.assertFalse(serial.is_open)


class G1CaptureFaultTest(unittest.TestCase):
    def check_frames(self,frames,reason):
        class Serial:
            is_open=False
            def open(self): self.is_open=True
            def read(self,n):
                item=frames.pop(0)
                if isinstance(item,BaseException):raise item
                return item
            def close(self): self.is_open=False
        serial=Serial()
        with tempfile.TemporaryDirectory() as d:
            output=Path(d)/'run'
            args=SimpleNamespace(run=True,gate_id='OFFLINE_TEST_ONLY',allow_port_open_boot=True,
                port='OFFLINE_FAKE',output=output,load=False,expected_source='0'*40,host='OFFLINE_FAKE')
            with patch.dict('sys.modules',{'serial':SimpleNamespace(Serial=lambda **kw:serial)}), patch.object(g1_capture.time,'monotonic',return_value=1), patch.object(g1_capture.subprocess,'run',return_value=SimpleNamespace(returncode=0,stdout='OFFLINE_EMPTY')):
                self.assertEqual(1,g1_capture.capture(args))
            self.assertEqual(reason+'\n',(output/'ABORT').read_text())
            self.assertFalse(serial.is_open)
            self.assertEqual([],frames)

    def test_partial_anomaly_stops_without_waiting_for_newline(self):
        self.check_frames([b'heartbeat: safe test mode, uptime_ms=1000\n',b'heap_alloc_failed'], 'uart-anomaly')

    def test_reset_between_heartbeats_stops(self):
        self.check_frames([b'heartbeat: safe test mode, uptime_ms=1000\n',b'rst:0x1\n'], 'uart-unexpected-reset')

    def test_uptime_regression_detects_reset_even_without_rom_line(self):
        self.check_frames([BOOT,b'heartbeat: safe test mode, uptime_ms=1000\n',b'heartbeat: safe test mode, uptime_ms=10\n'], 'uart-uptime-regression')

    def test_host_interrupt_is_fail_hold(self):
        self.check_frames([KeyboardInterrupt()], 'host-interrupt')


class G1CaptureTailTest(unittest.TestCase):
    def run_capture(self,frames):
        class Serial:
            is_open=False
            def open(self): self.is_open=True
            def read(self,n):
                if not frames:return b''
                item=frames.pop(0)
                if isinstance(item,BaseException):raise item
                return item
            def close(self): self.is_open=False
        serial=Serial()
        with tempfile.TemporaryDirectory() as d:
            output=Path(d)/'run'
            args=SimpleNamespace(run=True,gate_id='OFFLINE_TEST_ONLY',allow_port_open_boot=True,
                port='OFFLINE_FAKE',output=output,load=False,expected_source='0'*40,host='OFFLINE_FAKE')
            with patch.dict('sys.modules',{'serial':SimpleNamespace(Serial=lambda **kw:serial)}), patch.object(g1_capture.subprocess,'run',return_value=SimpleNamespace(returncode=0,stdout='OFFLINE_EMPTY')):
                code=g1_capture.capture(args)
            files={n:(output/n).read_bytes() for n in ('uart_raw.bin','ABORT','capture.json') if (output/n).exists()}
            tail=(output/'uart_tail.log').read_bytes() if (output/'uart_tail.log').exists() else None
            chunks=[json.loads(l) for l in (output/'uart_chunks.jsonl').read_text().splitlines()]
            return code,files,tail,chunks,serial

    def test_partial_anomaly_keeps_reading_the_follow_up_and_stamps_every_chunk(self):
        code,files,tail,chunks,serial=self.run_capture([
            b'heartbeat: safe test mode, uptime_ms=1000\n',b'E heap_alloc_failed: size=1626 caps=0x0000080c function=heap_caps_mall',
            b'oc\ndiag_alloc_fail: n=1 isr=0 core=0 pc: 400d0000\n',b'rst:0x'])
        self.assertEqual(1,code)
        self.assertEqual(b'uart-anomaly\n',files['ABORT'])
        # the unterminated frame is completed by the tail and nothing is lost
        self.assertIn(b'function=heap_caps_malloc\n',tail)
        self.assertIn(b'diag_alloc_fail: n=1',tail)
        self.assertIn(b'[tail-unterminated] rst:0x',tail)
        self.assertTrue(files['uart_raw.bin'].endswith(b'rst:0x'))
        self.assertEqual(4,len(chunks))
        self.assertEqual([False,False,True,True],[bool(c.get('tail')) for c in chunks])
        for c in chunks:
            self.assertIn('epoch',c);self.assertIn('monotonic',c)
        summary=json.loads(files['capture.json'])
        self.assertEqual({'g1_capture.py','c0_load.py'},set(summary['tool_sha256']))
        self.assertGreater(summary['post_anomaly_tail_bytes'],0)
        self.assertFalse(serial.is_open)

    def test_host_interrupt_has_no_tail(self):
        code,files,tail,chunks,serial=self.run_capture([KeyboardInterrupt()])
        self.assertEqual(1,code)
        self.assertIsNone(tail)
        self.assertNotIn('post_anomaly_tail_seconds',json.loads(files['capture.json']))


IDENTITY = [b'I (1) app_main: profile: esp32_release\n',
            b'I (2) app_main: source git sha: ' + b'0' * 40 + b'\n',
            b'I (3) app_main: real actuators: disabled\n',
            b'I (4) app_main: application: ready\n',
            b'I (5) app_main: network_mode=HOME_WIFI network_state=HomeConnected\n',
            b'I (6) app_main: heartbeat: safe test mode, uptime_ms=200000\n']
FRESH_BOOT = [BOOT] + IDENTITY


def run_capture_frames(frames, observe=False, on_read=None, max_raw=None, clock=None, load=True,
                       expected_source='0' * 40):
    """Drive g1_capture with scripted UART chunks. `clock` is a one-element list read by the fake monotonic
    clock; on_read(output, call_number, clock) may advance it or create marker files. When the script is
    exhausted the host STOP marker is created so successful runs end."""
    queue = list(frames)
    state = {'calls': 0, 'output': None}

    class Serial:
        is_open = False
        def open(self): self.is_open = True
        def read(self, n):
            state['calls'] += 1
            if on_read:on_read(state['output'], state['calls'], clock)
            if not queue:
                if state['output'] is not None:(state['output'] / 'STOP').write_text('x')
                return b''
            return queue.pop(0)
        def close(self): self.is_open = False
    serial = Serial()
    recorded = []
    with tempfile.TemporaryDirectory() as d:
        output = Path(d) / 'run'
        state['output'] = output
        args = SimpleNamespace(run=True, gate_id='OFFLINE_TEST_ONLY', allow_port_open_boot=True,
            port='OFFLINE_FAKE', output=output, load=load, expected_source=expected_source,
            host='OFFLINE_FAKE', observe_network_timeouts=observe)
        def fake_load(load_args):
            recorded.append(load_args)
            return 0
        monotonic = (lambda: clock[0]) if clock is not None else (lambda: 1)
        patches = [patch.dict('sys.modules', {'serial': SimpleNamespace(Serial=lambda **kw: serial)}),
                   patch.object(g1_capture.time, 'monotonic', side_effect=monotonic),
                   patch.object(g1_capture.subprocess, 'run', return_value=SimpleNamespace(returncode=0, stdout='OFFLINE_EMPTY')),
                   patch.object(g1_capture, 'load_run', side_effect=fake_load)]
        if max_raw:patches.append(patch.object(g1_capture, 'MAX_RAW_BYTES', max_raw))
        for item in patches:item.start()
        try:
            code = g1_capture.capture(args)
        finally:
            for item in patches:item.stop()
        abort = (output / 'ABORT').read_text() if (output / 'ABORT').exists() else None
        log = (output / 'uart.log').read_bytes()
        return code, json.loads((output / 'capture.json').read_text()), abort, recorded, serial, log


class G1CaptureObservationTest(unittest.TestCase):
    def test_the_observation_flag_reaches_the_load_driver_and_the_summary(self):
        def start_load(output, call, clock):
            if call == 2:(output / 'START_LOAD').write_text('x')
        code, summary, abort, recorded, serial, _ = run_capture_frames(FRESH_BOOT, observe=True, on_read=start_load)
        self.assertTrue(summary['observation_mode'])
        self.assertEqual(1, len(recorded))
        self.assertTrue(recorded[0].observe)
        self.assertEqual((0, None), (code, abort))

    def test_default_mode_does_not_enable_observation(self):
        code, summary, abort, recorded, _, _ = run_capture_frames(
            [BOOT, b'heartbeat: safe test mode, uptime_ms=1\n', b'rst:0x3 (SW_RESET)\n'], observe=False)
        self.assertFalse(summary['observation_mode'])
        self.assertEqual('uart-unexpected-reset\n', abort)

    def test_safety_relevant_uart_events_stay_immediate_stops_in_observation_mode(self):
        for frame, reason in ((b'E heap_alloc_failed: size=1696 caps=0x00001800 function=heap_caps_malloc', 'uart-anomaly'),
                              (b"Guru Meditation Error: Core 0 panic'ed\n", 'uart-anomaly'),
                              (b'rst:0x3 (SW_RESET)\n', 'uart-unexpected-reset'),
                              (b'I (9) app_main: heartbeat: safe test mode, uptime_ms=5\n', 'uart-uptime-regression')):
            with self.subTest(reason=reason, frame=frame[:20]):
                code, summary, abort, _, serial, _ = run_capture_frames(FRESH_BOOT + [frame], observe=True)
                self.assertEqual(1, code)
                self.assertEqual(reason + '\n', abort)
                self.assertTrue(summary['observation_mode'])
                self.assertFalse(serial.is_open)

    def test_a_volume_overrun_stops_fail_closed_instead_of_dropping_lines(self):
        code, summary, abort, _, serial, _ = run_capture_frames(FRESH_BOOT, observe=True, max_raw=20)
        self.assertEqual((1, 'log-volume-limit\n'), (code, abort))
        self.assertFalse(serial.is_open)


STALE = [b'p_main: heartbeat: safe test mode, uptime_ms=3209930\n'] * 3 + [
    b'I (1) app_main: profile: esp32_release\n',
    b'I (2) app_main: source git sha: ' + b'f' * 40 + b'\n',   # old image: must never count for this boot
    b'I (3) app_main: real actuators: disabled\n', b'I (4) app_main: application: ready\n',
    b'I (5) app_main: network_mode=HOME_WIFI network_state=HomeConnected\n']


class G1CaptureBootMarkerTest(unittest.TestCase):
    """The first expected POWERON_RESET within 10 s of the port open is the test boot; old buffered
    lines before it are logged but are no evidence of this boot."""

    def fresh_boot_ends_cleanly(self, frames):
        code, summary, abort, recorded, serial, log = run_capture_frames(frames)
        self.assertEqual((0, None, 'CAPTURE_COMPLETE_PENDING_REVIEW'), (code, abort, summary['status']))
        self.assertTrue(all(summary[k] for k in ('source_proven', 'ready', 'actors_disabled', 'release_profile', 'network_home_connected')))
        return summary, log

    def test_a_buffered_heartbeats_and_old_identity_before_the_expected_reset_are_tolerated(self):
        summary, log = self.fresh_boot_ends_cleanly(STALE + FRESH_BOOT)
        self.assertEqual({'accepted': True, 'cause': 'POWERON_RESET', 'seconds_after_open': 0, 'lines_before_marker': len(STALE)},
                         summary['boot_marker'])
        self.assertEqual(len(STALE), summary['lines_before_marker'])
        for old in (b'uptime_ms=3209930', b'f' * 40):  # nothing is dropped from the log
            self.assertIn(old, log)

    def test_a2_old_identity_never_proves_the_fresh_boot(self):
        # old identity lines but a fresh boot that never shows its own identity: no readiness
        clock = [0.0]
        def advance(output, call, clk):
            if call >= 3:clk[0] = 31.0
        code, summary, abort, recorded, _, _ = run_capture_frames(STALE + [BOOT, b'I (1) x\n'], clock=clock, on_read=advance)
        self.assertEqual((1, 'uart-heartbeat-timeout\n'), (code, abort))  # fail-closed: no fresh heartbeat either
        self.assertEqual([], recorded)
        self.assertFalse(summary['source_proven'])

    def test_a4_a_fresh_boot_that_never_shows_its_identity_is_not_ready(self):
        clock = [0.0]
        def tick(output, call, clk):
            clk[0] += 2.0
            if call == 2:(output / 'START_LOAD').write_text('x')
        frames = [BOOT] + [b'heartbeat: safe test mode, uptime_ms=%d\n' % (200000 + 1000 * i) for i in range(1, 30)]
        code, summary, abort, recorded, _, _ = run_capture_frames(frames, clock=clock, on_read=tick)
        self.assertEqual((1, 'boot-identity-state-not-proven\n'), (code, abort))
        self.assertEqual([], recorded)  # no readiness, so START_LOAD is never acted on
        self.assertTrue(summary['boot_marker']['accepted'])

    def test_a3_a_reset_marker_split_across_chunks_is_not_mistaken_for_an_anomaly(self):
        self.fresh_boot_ends_cleanly([b'junk\n', b'rst:0x1 (POWER', b'ON_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)\n'] + IDENTITY)

    def test_b_a_second_reset_stops(self):
        for frames in (STALE + FRESH_BOOT + [BOOT],                      # complete second marker
                       STALE + FRESH_BOOT + [b'rst:0x1 (POWER', b'ON_RESET),boot:0x13\n']):  # split second marker
            with self.subTest(frames=len(frames)):
                code, summary, abort, _, _, _ = run_capture_frames(frames)
                self.assertEqual((1, 'uart-unexpected-reset\n'), (code, abort))

    def test_c_the_first_reset_after_ten_seconds_stops(self):
        clock = [0.0]
        def late(output, call, clk):
            if call == 2:clk[0] = 10.5
        code, summary, abort, _, _, _ = run_capture_frames([b'old\n'] + FRESH_BOOT, clock=clock, on_read=late)
        self.assertEqual((1, 'uart-unexpected-reset\n'), (code, abort))
        self.assertIsNone(summary['boot_marker'])

    def test_c2_a_marker_exactly_inside_the_window_is_accepted(self):
        clock = [0.0]
        def edge(output, call, clk):
            if call == 2:clk[0] = 9.9
        code, summary, abort, _, _, _ = run_capture_frames([b'old\n'] + FRESH_BOOT, clock=clock, on_read=edge)
        self.assertEqual((0, None), (code, abort))
        self.assertTrue(summary['boot_marker']['accepted'])

    def test_d_other_reset_causes_stop_even_inside_the_window(self):
        for marker in (b'rst:0x3 (SW_RESET),boot:0x13\n', b'rst:0xc (SW_CPU_RESET),boot:0x13\n',
                       b'rst:0x7 (TG0WDT_SYS_RESET),boot:0x13\n', b'rst:0x1\n', b'rst:0x10 (RTCWDT_RTC_RESET)\n'):
            with self.subTest(marker=marker[:22]):
                code, _, abort, _, _, _ = run_capture_frames(STALE + [marker] + IDENTITY)
                self.assertEqual((1, 'uart-unexpected-reset\n'), (code, abort))

    def test_e_uptime_regression_after_the_recognised_boot_stops(self):
        code, _, abort, _, _, _ = run_capture_frames(
            STALE + [BOOT, b'heartbeat: safe test mode, uptime_ms=5000\n', b'heartbeat: safe test mode, uptime_ms=4000\n'])
        self.assertEqual((1, 'uart-uptime-regression\n'), (code, abort))

    def test_f_without_a_reset_marker_there_is_no_fresh_boot_proof(self):
        clock = [0.0]
        def advance(output, call, clk):
            if call == 2:(output / 'START_LOAD').write_text('x')
            if call >= 3:clk[0] = 31.0
        code, summary, abort, recorded, _, _ = run_capture_frames(IDENTITY, clock=clock, on_read=advance)
        self.assertEqual((1, 'uart-heartbeat-timeout\n'), (code, abort))  # old heartbeats never refresh the liveness
        self.assertEqual([], recorded)  # START_LOAD present but never acted on
        self.assertIsNone(summary['boot_marker'])

    def test_g_safety_anomalies_stop_before_and_after_the_boot_marker(self):
        cases = (([b'E heap_alloc_failed: size=1696 caps=0x00001800 function=heap_caps_malloc\n'] + FRESH_BOOT, 'uart-anomaly'),
                 ([b'old\n', BOOT, b"Guru Meditation Error: Core 0 panic'ed\n"], 'uart-anomaly'),
                 ([b'Task watchdog got triggered\n'] + FRESH_BOOT, 'uart-anomaly'),
                 ([b'I (9) app_main: resources: stack_hwm_bytes=100\n'] + FRESH_BOOT, 'main-stack-reserve'),
                 ([BOOT, b'W (9) res01_http: phase=pre httpd_stack_hwm=100\n'], 'httpd-stack-reserve'))
        for frames, reason in cases:
            with self.subTest(reason=reason, first=frames[0][:24]):
                code, _, abort, _, _, log = run_capture_frames(frames)
                self.assertEqual((1, reason + '\n'), (code, abort))
                self.assertIn(frames[0].strip()[:20], log)  # the offending line is in the log
