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
            with patch.dict('sys.modules',{'serial':SimpleNamespace(Serial=lambda **kw:serial)}), patch.object(g1_capture.time,'monotonic',side_effect=[0,6]):
                result=g1_capture.capture(args)
            self.assertEqual(1,result)
            self.assertEqual('uart-heartbeat-timeout\n',(output/'ABORT').read_text())
            self.assertFalse(serial.is_open)
