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
        self.check_frames([b'heartbeat: safe test mode, uptime_ms=1000\n',b'rst:0x1\n'], 'uart-unexpected-reset')

    def test_uptime_regression_detects_reset_even_without_rom_line(self):
        self.check_frames([b'heartbeat: safe test mode, uptime_ms=1000\n',b'heartbeat: safe test mode, uptime_ms=10\n'], 'uart-uptime-regression')

    def test_host_interrupt_is_fail_hold(self):
        self.check_frames([KeyboardInterrupt()], 'host-interrupt')
