import json
from pathlib import Path
import signal
from types import SimpleNamespace
import struct
import tempfile
import unittest
from unittest.mock import patch
import g1_pcap

DEVICE = '10.0.0.2'
HOST = '10.0.0.1'
SYN, FIN, RST, ACK = 0x02, 0x01, 0x04, 0x10


def ip(text):
    return bytes(int(x) for x in text.split('.'))


def frame(src, dst, sport, dport, flags, seq=0, ack=0, window=5760, length=0, snap=54,
          words=5, frag=0, total=None):
    """Ethernet/IPv4/TCP headers only; IP total length announces `length` payload bytes."""
    tcp = struct.pack('!HHIIHHHH', sport, dport, seq, ack, (words << 12) | flags, window, 0, 0)
    total = 20 + words * 4 + length if total is None else total
    ipv4 = struct.pack('!BBHHHBBH', 0x45, 0, total, 0, frag, 64, 6, 0) + ip(src) + ip(dst)
    return (b'\0' * 12 + b'\x08\x00' + ipv4 + tcp)[:snap]


def pcap(items, linktype=1, trailing=b'', original=None):
    out = struct.pack('<IHHiIII', 0xA1B2C3D4, 2, 4, 0, 0, 65535, linktype)
    for t, data in items:
        wire = original if original is not None else max(60, 14 + struct.unpack('!H', data[16:18])[0])
        out += struct.pack('<IIII', int(t), int((t % 1) * 1e6), len(data), wire) + data
    return out + trailing


class PayloadFreeTests(unittest.TestCase):
    def write(self, folder, items, **kw):
        path = Path(folder) / 'x.pcap'
        path.write_bytes(pcap(items, **kw))
        return path

    def assert_rejected(self, items, **kw):
        with tempfile.TemporaryDirectory() as d:
            with self.assertRaises(g1_pcap.NotPayloadFree):
                g1_pcap.verify(self.write(d, items, **kw))

    def test_header_only_capture_is_accepted(self):
        with tempfile.TemporaryDirectory() as d:
            path = self.write(d, [(1.0, frame(HOST, DEVICE, 4000, 80, SYN)),
                                  (1.1, frame(HOST, DEVICE, 4000, 80, ACK, length=300)),
                                  (1.2, frame(HOST, DEVICE, 4000, 80, ACK, words=8))])
            self.assertEqual(3, g1_pcap.verify(path))

    def test_any_capture_longer_than_the_header_limit_fails_closed(self):
        self.assert_rejected([(1.0, frame(HOST, DEVICE, 4000, 80, ACK, length=300, snap=70) + b'GET /' * 3)])

    def test_vlan_udp_wrong_linktype_and_ip_options_fail_closed(self):
        udp = bytearray(frame(HOST, DEVICE, 1, 2, ACK)); udp[23] = 17
        vlan = bytearray(frame(HOST, DEVICE, 1, 2, ACK)); vlan[12:14] = b'\x81\x00'
        options = bytearray(frame(HOST, DEVICE, 1, 2, ACK)); options[14] = 0x46
        for data in (bytes(udp), bytes(vlan), bytes(options)):
            self.assert_rejected([(1.0, data)])
        self.assert_rejected([(1.0, frame(HOST, DEVICE, 1, 2, ACK))], linktype=113)

    def test_fragmented_ipv4_is_rejected(self):
        for frag in (0x2000, 0x0005, 0x4000 | 0x2000):  # MF, non-first fragment, MF+DF
            with self.subTest(frag=hex(frag)):
                self.assert_rejected([(1.0, frame(HOST, DEVICE, 1, 2, ACK, frag=frag, length=100))])

    def test_invalid_or_inconsistent_headers_are_rejected(self):
        self.assert_rejected([(1.0, frame(HOST, DEVICE, 1, 2, ACK, words=4))])          # TCP header < 20 B
        self.assert_rejected([(1.0, frame(HOST, DEVICE, 1, 2, ACK, words=15, total=30))])  # total < IP+TCP
        self.assert_rejected([(1.0, frame(HOST, DEVICE, 1, 2, ACK, length=500))], original=100)  # total > wire

    def test_truncated_trailing_and_empty_captures_are_rejected(self):
        good = [(1.0, frame(HOST, DEVICE, 1, 2, ACK))]
        full = pcap(good)
        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / 'x.pcap'
            for label, blob in (('trailing-bytes', full + b'\x01\x02\x03'),
                                ('trailing-partial-record-header', full + b'\0' * 15),
                                ('truncated-record-data', full[:-10]),
                                ('file-too-short', full[:20]),
                                ('header-only-empty', full[:24]),
                                ('zero-bytes', b'')):
                with self.subTest(label):
                    path.write_bytes(blob)
                    with self.assertRaises(g1_pcap.NotPayloadFree):
                        g1_pcap.verify(path)

    def test_record_whose_captured_length_is_not_min_of_wire_and_snaplen_is_rejected(self):
        short = frame(HOST, DEVICE, 1, 2, ACK)[:50]
        self.assert_rejected([(1.0, short)], original=60)


class Process:
    """tcpdump stand-in. `alive_polls` = how many poll() calls report it still running."""
    def __init__(self, folder, items=None, stderr=b'', alive_polls=10 ** 9, returncode=0,
                 ignore_signal=False, hang_on_wait=False, output=None):
        self.output, self.items, self.alive_polls = output, items, alive_polls
        self.stderr = SimpleNamespace(read=lambda: stderr)
        self.returncode, self.final_returncode = None, returncode
        self.ignore_signal, self.hang_on_wait = ignore_signal, hang_on_wait
        self.signals, self.terminated, self.killed = [], False, False

    def poll(self):
        if self.returncode is None:
            self.alive_polls -= 1
            if self.alive_polls < 0:
                self.returncode = self.final_returncode
        return self.returncode

    def send_signal(self, number):
        self.signals.append(number)
        if not self.ignore_signal:
            self.returncode = self.final_returncode

    def terminate(self):
        self.terminated = True

    def kill(self):
        self.killed = True
        self.returncode = -9

    def wait(self, timeout):
        if self.returncode is None or (self.hang_on_wait and not self.killed):
            raise g1_pcap.subprocess.TimeoutExpired('tcpdump', timeout)
        return self.returncode


class CaptureLifecycleTests(unittest.TestCase):
    def run_capture(self, process_factory, *, stop=None, max_seconds=100.0, ticks=None, create=None):
        with tempfile.TemporaryDirectory() as d:
            folder = Path(d)
            output = folder / 'cap.pcap'
            abort = folder / 'ABORT'
            if create is not None:
                output.write_bytes(create)
            process = process_factory(output)
            args = SimpleNamespace(run=True, gate_id='OFFLINE_TEST_ONLY', output=output, interface='lo',
                                   device_ip=DEVICE, stop_file=[abort], max_seconds=max_seconds)
            clock = iter(ticks) if ticks else None
            patches = [patch.object(g1_pcap.subprocess, 'Popen', return_value=process),
                       patch.object(g1_pcap.time, 'sleep', side_effect=lambda _s: stop and stop(abort)),
                       patch.object(g1_pcap, 'inside_git_worktree', return_value=False)]
            if clock:
                patches.append(patch.object(g1_pcap.time, 'monotonic', side_effect=lambda: next(clock)))
            for p in patches:
                p.start()
            try:
                code = g1_pcap.capture(args)
            finally:
                for p in patches:
                    p.stop()
            result = json.loads((folder / 'pcap_result.json').read_text())
            return code, result, process, output.exists()

    GOOD = pcap([(1.0, frame(HOST, DEVICE, 4000, 80, SYN)), (1.1, frame(HOST, DEVICE, 4000, 80, ACK))])

    def test_gate_is_required_and_a_git_worktree_destination_is_refused(self):
        with self.assertRaises(SystemExit):
            g1_pcap.capture(SimpleNamespace(run=False, gate_id=None))
        with tempfile.TemporaryDirectory() as d:
            (Path(d) / '.git').mkdir()
            args = SimpleNamespace(run=True, gate_id='X', output=Path(d) / 'a' / 'cap.pcap',
                                   stop_file=[], max_seconds=1.0, interface='lo', device_ip=DEVICE)
            with self.assertRaises(SystemExit):
                g1_pcap.capture(args)

    def test_the_run_abort_file_ends_the_capture_and_a_complete_file_passes(self):
        create_abort = lambda abort: abort.write_text('uart-anomaly\n')
        code, result, process, exists = self.run_capture(
            lambda out: Process(None, stderr=b'2 packets captured\n2 packets received by filter\n'),
            stop=create_abort, create=self.GOOD)
        self.assertEqual(0, code)
        self.assertEqual('PASS', result['status'])
        self.assertEqual(2, result['packets_verified'])
        self.assertEqual([signal.SIGINT], process.signals)
        self.assertEqual(g1_pcap.SNAPLEN, result['snaplen'])
        self.assertEqual(64, len(result['tool_sha256']))

    def test_permission_or_start_failures_are_a_hold(self):
        code, result, _, _ = self.run_capture(
            lambda out: Process(None, alive_polls=0, returncode=1, stderr=b'tcpdump: Operation not permitted'))
        self.assertEqual((3, 'HOLD', 'tcpdump-exited-at-start'), (code, result['status'], result['reason']))
        with patch.object(g1_pcap.subprocess, 'Popen', side_effect=FileNotFoundError), \
             patch.object(g1_pcap, 'inside_git_worktree', return_value=False), tempfile.TemporaryDirectory() as d:
            args = SimpleNamespace(run=True, gate_id='X', output=Path(d) / 'c.pcap', stop_file=[],
                                   max_seconds=1.0, interface='lo', device_ip=DEVICE)
            self.assertEqual(3, g1_pcap.capture(args))

    def test_process_dying_during_the_run_is_a_hold_even_with_a_valid_file(self):
        code, result, _, exists = self.run_capture(
            lambda out: Process(None, alive_polls=2, returncode=0, stderr=b'2 packets captured\n'),
            create=self.GOOD)
        self.assertEqual((3, 'tcpdump-exited-during-run'), (code, result['reason']))
        self.assertTrue(exists)  # valid and payload-free: kept for the review, run still HOLD

    def test_hard_time_limit_is_a_hold(self):
        code, result, process, _ = self.run_capture(
            lambda out: Process(None, stderr=b'2 packets captured\n'), create=self.GOOD,
            max_seconds=5.0, ticks=[0.0, 1.0, 2.0, 9.0, 9.0, 9.0])
        self.assertEqual((6, 'hard-time-limit'), (code, result['reason']))
        self.assertEqual([signal.SIGINT], process.signals)

    def test_interrupt_stops_tcpdump_and_is_a_hold(self):
        def interrupt(_abort):
            raise KeyboardInterrupt
        code, result, process, _ = self.run_capture(
            lambda out: Process(None, stderr=b'2 packets captured\n'), stop=interrupt, create=self.GOOD)
        self.assertEqual((5, 'interrupted'), (code, result['reason']))
        self.assertEqual([signal.SIGINT], process.signals)

    def test_a_tcpdump_that_ignores_sigint_is_terminated_then_killed(self):
        code, result, process, _ = self.run_capture(
            lambda out: Process(None, ignore_signal=True, hang_on_wait=True, stderr=b''),
            stop=lambda abort: abort.write_text('x'), create=self.GOOD)
        self.assertTrue(process.terminated and process.killed)
        self.assertEqual((3, 'tcpdump-not-terminating'), (code, result['reason']))

    def test_nonzero_exit_after_the_stop_signal_is_a_hold(self):
        code, result, _, _ = self.run_capture(
            lambda out: Process(None, returncode=1, stderr=b'2 packets captured\n'),
            stop=lambda abort: abort.write_text('x'), create=self.GOOD)
        self.assertEqual((3, 'tcpdump-nonzero-exit'), (code, result['reason']))

    def test_missing_empty_unreported_or_miscounted_files_are_never_a_pass(self):
        stop = lambda abort: abort.write_text('x')
        cases = (('capture-file-missing', None, b'0 packets captured\n'),
                 ('capture-empty', pcap([]), b'0 packets captured\n'),
                 ('capture-incomplete-or-unreported', self.GOOD, b''),
                 ('capture-incomplete-or-unreported', self.GOOD, b'5 packets captured\n'))
        for reason, blob, err in cases:
            with self.subTest(reason=reason, err=err):
                code, result, _, _ = self.run_capture(lambda out: Process(None, stderr=err), stop=stop, create=blob)
                self.assertEqual((4, 'HOLD', reason), (code, result['status'], result['reason']))

    def test_a_file_that_is_not_payload_free_is_deleted(self):
        bad = pcap([(1.0, frame(HOST, DEVICE, 4000, 80, ACK, length=300, snap=70) + b'GET /')])
        code, result, _, exists = self.run_capture(
            lambda out: Process(None, stderr=b'1 packets captured\n'),
            stop=lambda abort: abort.write_text('x'), create=bad)
        self.assertEqual(2, code)
        self.assertFalse(exists)
        self.assertTrue(result['reason'].startswith('not-payload-free'))


class AnalyzeTests(unittest.TestCase):
    def analyze(self, items):
        with tempfile.TemporaryDirectory() as d:
            path = Path(d) / 'x.pcap'
            path.write_bytes(pcap(items))
            g1_pcap.verify(path)
            return g1_pcap.analyze(path, DEVICE)

    def test_counters_for_retransmits_dupacks_zero_window_and_syn_repeats(self):
        h = lambda *a, **k: frame(HOST, DEVICE, 4000, 80, *a, **k)
        d = lambda *a, **k: frame(DEVICE, HOST, 80, 4000, *a, **k)
        result = self.analyze([
            (0.0, h(SYN)), (1.0, h(SYN)),                      # SYN repeated after 1 s
            (1.2, d(SYN | ACK, seq=100, ack=1)),
            (1.3, h(ACK, seq=1, ack=101, length=200)),          # request
            (1.8, d(ACK, seq=101, ack=201, length=1440)),       # first reply after 500 ms
            (1.9, d(ACK, seq=1541, ack=201, length=1440)),
            (2.0, h(ACK, seq=201, ack=1541)),
            (2.1, h(ACK, seq=201, ack=1541)),                   # duplicate ack
            (3.5, d(ACK, seq=101, ack=201, length=1440)),       # retransmission of old data
            (3.6, d(ACK, seq=2981, ack=201, window=0)),         # zero window from device
            (3.7, d(RST | ACK, seq=2981, ack=201)),
        ])
        self.assertTrue(result['payload_free_verified'])
        self.assertEqual(1, result['connections'])
        self.assertEqual(1, result['syn_retransmits'])
        self.assertEqual(1, result['host_pure_dupacks'])
        self.assertEqual(1, result['device_retransmitted_segments'])
        self.assertEqual(3, result['device_data_segments'])
        self.assertEqual(1, result['zero_window_from_device'])
        self.assertEqual(1, result['device_rst'])
        self.assertEqual(500.0, result['time_to_first_reply_ms']['median'])
        self.assertEqual(1200.0, result['synack_delay_ms']['median'])
        self.assertEqual(0, result['connections_without_synack'])
        self.assertEqual(1, result['bins']['0']['syn_retransmits'])
        self.assertEqual(1, result['bins']['0']['host_pure_dupacks'])

    def test_unanswered_connect_and_request_are_visible_and_output_has_no_addresses(self):
        h = lambda *a, **k: frame(HOST, DEVICE, 4001, 80, *a, **k)
        result = self.analyze([(0.0, h(SYN)), (1.0, h(SYN)), (3.0, h(SYN))])
        self.assertEqual(2, result['syn_retransmits'])
        self.assertEqual(1, result['connections_without_synack'])
        text = repr(result)
        self.assertNotIn(HOST, text)
        self.assertNotIn(DEVICE, text)
        self.assertNotIn('4001', text)


if __name__ == '__main__':
    unittest.main()
