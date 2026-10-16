import json
import tempfile
from pathlib import Path
from types import SimpleNamespace
import unittest
from unittest.mock import patch
import g1_capture


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
        self.check_frames([b'heartbeat: safe test mode, uptime_ms=1000\n',b'rst:0x1 (POWERON_RESET)\n'], 'uart-unexpected-reset')

    def test_second_reset_stops(self):
        self.check_frames([b'rst:0x1 (POWERON_RESET)\n',b'rst:0x1 (POWERON_RESET)\n'], 'uart-unexpected-reset')

    def test_initial_other_or_malformed_reset_cause_stops(self):
        for reset in (b'rst:0xc (SW_CPU_RESET)\n',
                      b'rst:0x10 (RTCWDT_RTC_RESET)\n',
                      b'rst:0x1 (SW_CPU_RESET)\n',
                      b'rst:0xc (POWERON_RESET)\n',
                      b'rst:0x1\n'):
            with self.subTest(reset=reset):
                self.check_frames([reset], 'uart-unexpected-reset')

    def test_partial_panic_stops_without_waiting_for_newline(self):
        self.check_frames([b"Guru Meditation: Core 1 panic'ed"], 'uart-anomaly')

    def test_all_other_partial_hard_stop_markers_remain_active(self):
        for marker in (b'heap_alloc_failed',b'abort()',b'task_wdt',b'Brownout',
                       b'stack overflow',b'wdt timeout',b'g1_probe_error'):
            with self.subTest(marker=marker):self.check_frames([marker], 'uart-anomaly')

    def test_uptime_regression_detects_reset_even_without_rom_line(self):
        self.check_frames([b'heartbeat: safe test mode, uptime_ms=1000\n',b'heartbeat: safe test mode, uptime_ms=10\n'], 'uart-uptime-regression')

    def test_host_interrupt_is_fail_hold(self):
        self.check_frames([KeyboardInterrupt()], 'host-interrupt')

    def run_complete_boot(self,frames,main_stack_policy='strict',first_frame_delay=0):
        clock=[1.0]
        class Serial:
            is_open=False
            reads=0
            def open(self): self.is_open=True
            def read(self,n):
                if self.reads==0:clock[0]+=first_frame_delay
                self.reads+=1
                item=frames.pop(0) if frames else b''
                if not frames:(output/'STOP').touch(exist_ok=True)
                return item
            def close(self): self.is_open=False
        serial=Serial()
        with tempfile.TemporaryDirectory() as d:
            output=Path(d)/'run'
            args=SimpleNamespace(run=True,gate_id=g1_capture.G2_WEB_GATE_ID if main_stack_policy=='g2-trend' else 'OFFLINE_TEST_ONLY',
                allow_port_open_boot=True,port='OFFLINE_FAKE',output=output,load=False,
                expected_source='f'*40,host='OFFLINE_FAKE',main_stack_policy=main_stack_policy)
            with patch.dict('sys.modules',{'serial':SimpleNamespace(Serial=lambda **kw:serial)}), \
                    patch.object(g1_capture.subprocess,'run',return_value=SimpleNamespace(returncode=0,stdout='OFFLINE_EMPTY')), \
                    patch.object(g1_capture.time,'monotonic',side_effect=lambda:clock[0]):
                code=g1_capture.capture(args)
            summary=json.loads((output/'capture.json').read_text())
            return code,summary,(output/'ABORT').read_text() if (output/'ABORT').exists() else None,serial

    def boot_lines(self,reset):
        return [reset,
            b'app_main: source git sha: '+b'f'*40+b'\n',
            b'app_main: profile: esp32_release\n',
            b'app_main: real actuators: disabled\n',
            b'app_main: application: ready\n',
            b'app_main: network_mode=HOME_WIFI network_state=HomeConnected\n',
            b'app_main: heartbeat: safe test mode, uptime_ms=1000\n']

    def test_split_initial_poweron_reset_line_is_accepted(self):
        frames=[b'rst:0x',b'1 (POWERON_RESET),boot:0x13\n']+self.boot_lines(b'')[1:]
        # The reset banner is split across reads; all subsequent boot evidence is complete.
        code,summary,abort,serial=self.run_complete_boot(frames)
        self.assertEqual(0,code)
        self.assertIsNone(abort)
        self.assertEqual(1,summary['expected_initial_reset_lines'])
        self.assertEqual('CAPTURE_COMPLETE_PENDING_REVIEW',summary['status'])
        self.assertFalse(serial.is_open)

    def test_complete_initial_poweron_reset_line_is_accepted(self):
        code,summary,abort,_=self.run_complete_boot(self.boot_lines(b'rst:0x1 (POWERON_RESET),boot:0x13\n'))
        self.assertEqual(0,code)
        self.assertIsNone(abort)
        self.assertEqual(1,summary['expected_initial_reset_lines'])

    def test_initial_poweron_reset_at_first_heartbeat_deadline_is_accepted(self):
        frames=self.boot_lines(b'rst:0x1 (POWERON_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)\r\n')
        code,summary,abort,_=self.run_complete_boot(frames,first_frame_delay=5)
        self.assertEqual(0,code)
        self.assertIsNone(abort)
        self.assertEqual(1,summary['expected_initial_reset_lines'])

    def test_initial_poweron_reset_after_first_heartbeat_deadline_stops(self):
        frames=self.boot_lines(b'rst:0x1 (POWERON_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)\r\n')
        # The read starts in time but the full reset line arrives outside the
        # window; classify the received reset itself as unexpected.
        code,summary,abort,_=self.run_complete_boot(frames,first_frame_delay=5.001)
        self.assertEqual(1,code)
        self.assertEqual('uart-unexpected-reset',summary['failure'])
        self.assertEqual('uart-unexpected-reset\n',abort)

    def test_g2_main_stack_hwm_is_recorded_as_trend_only(self):
        frames=self.boot_lines(b'rst:0x1 (POWERON_RESET),boot:0x13\n')
        frames.insert(1,b'app_main: stack_hwm_bytes=10000\n')
        code,summary,abort,_=self.run_complete_boot(frames,'g2-trend')
        self.assertEqual(0,code)
        self.assertIsNone(abort)
        self.assertEqual('g2-trend',summary['main_stack_policy'])
        self.assertEqual('record-only',summary['main_stack_action'])
        self.assertEqual(11160,summary['main_stack_hwm_threshold_bytes'])
        self.assertEqual(10000,summary['main_stack_hwm_min_bytes'])
        self.assertTrue(summary['main_stack_below_threshold'])

    def test_g1_main_stack_hwm_remains_hard_stop(self):
        frames=self.boot_lines(b'rst:0x1 (POWERON_RESET),boot:0x13\n')
        frames.insert(1,b'app_main: stack_hwm_bytes=10000\n')
        code,summary,abort,_=self.run_complete_boot(frames)
        self.assertEqual(1,code)
        self.assertEqual('main-stack-reserve',summary['failure'])
        self.assertEqual(10000,summary['main_stack_hwm_min_bytes'])
        self.assertTrue(summary['main_stack_below_threshold'])
        self.assertEqual('strict',summary['main_stack_policy'])

    def test_httpd_stack_hwm_remains_hard_stop_in_g2_trend_mode(self):
        frames=self.boot_lines(b'rst:0x1 (POWERON_RESET),boot:0x13\n')
        frames.insert(1,b'httpd_stack_hwm=2000\n')
        code,summary,abort,_=self.run_complete_boot(frames,'g2-trend')
        self.assertEqual(1,code)
        self.assertEqual('httpd-stack-reserve',summary['failure'])

    def test_g2_stack_trend_policy_is_restricted_to_exact_gate(self):
        args=SimpleNamespace(run=True,gate_id='OTHER_GATE',allow_port_open_boot=True,main_stack_policy='g2-trend')
        with self.assertRaises(SystemExit):g1_capture.capture(args)


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
