from pathlib import Path
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


def frame(src, dst, sport, dport, flags, seq=0, ack=0, window=5760, length=0, snap=54):
    """Ethernet/IPv4/TCP headers only; IP total length announces `length` payload bytes."""
    tcp = struct.pack('!HHIIHHHH', sport, dport, seq, ack, (5 << 12) | flags, window, 0, 0)
    total = 20 + 20 + length
    ipv4 = struct.pack('!BBHHHBBH', 0x45, 0, total, 0, 0, 64, 6, 0) + ip(src) + ip(dst)
    return (b'\0' * 12 + b'\x08\x00' + ipv4 + tcp)[:snap]


def pcap(items, linktype=1):
    out = struct.pack('<IHHiIII', 0xA1B2C3D4, 2, 4, 0, 0, 65535, linktype)
    for t, data in items:
        out += struct.pack('<IIII', int(t), int((t % 1) * 1e6), len(data), len(data)) + data
    return out


class PayloadFreeTests(unittest.TestCase):
    def write(self, folder, items, **kw):
        path = Path(folder) / 'x.pcap'
        path.write_bytes(pcap(items, **kw))
        return path

    def test_header_only_capture_is_accepted(self):
        with tempfile.TemporaryDirectory() as d:
            path = self.write(d, [(1.0, frame(HOST, DEVICE, 4000, 80, SYN)),
                                  (1.1, frame(HOST, DEVICE, 4000, 80, ACK, length=300))])
            self.assertEqual(2, g1_pcap.verify(path))

    def test_any_capture_longer_than_the_header_limit_fails_closed(self):
        with tempfile.TemporaryDirectory() as d:
            path = self.write(d, [(1.0, frame(HOST, DEVICE, 4000, 80, ACK, length=300, snap=70) + b'GET /' * 3)])
            with self.assertRaises(g1_pcap.NotPayloadFree):
                g1_pcap.verify(path)

    def test_vlan_non_ip_udp_and_wrong_linktype_fail_closed(self):
        udp = bytearray(frame(HOST, DEVICE, 1, 2, ACK)); udp[23] = 17
        vlan = bytearray(frame(HOST, DEVICE, 1, 2, ACK)); vlan[12:14] = b'\x81\x00'
        with tempfile.TemporaryDirectory() as d:
            for data in (bytes(udp), bytes(vlan)):
                with self.assertRaises(g1_pcap.NotPayloadFree):
                    g1_pcap.verify(self.write(d, [(1.0, data)]))
            with self.assertRaises(g1_pcap.NotPayloadFree):
                g1_pcap.verify(self.write(d, [(1.0, frame(HOST, DEVICE, 1, 2, ACK))], linktype=113))

    def test_capture_requires_gate_and_deletes_a_file_that_fails_verification(self):
        with self.assertRaises(SystemExit):
            g1_pcap.capture(SimpleNamespace(run=False, gate_id=None))
        with tempfile.TemporaryDirectory() as d:
            output = Path(d) / 'cap.pcap'
            output.write_bytes(pcap([(1.0, frame(HOST, DEVICE, 1, 2, ACK, snap=60) + b'x' * 10)]))
            process = SimpleNamespace(poll=lambda: 0, wait=lambda timeout: 0)
            args = SimpleNamespace(run=True, gate_id='OFFLINE_TEST_ONLY', output=output,
                                   interface='lo', device_ip=DEVICE)
            with patch.object(g1_pcap.subprocess, 'Popen', return_value=process) as popen:
                self.assertEqual(2, g1_pcap.capture(args))
            command = popen.call_args[0][0]
            self.assertEqual('54', command[command.index('-s') + 1])
            self.assertFalse(output.exists())


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
