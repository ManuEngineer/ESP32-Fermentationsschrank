"""G1b host-side, payload-free TCP header capture and analysis (diagnosis only).

Never talks to the device: `capture` wraps the host's tcpdump on the host NIC, `verify`
proves the file holds no payload, `analyze` derives counters. Importing opens nothing.

Payload-free by construction: snaplen 54 = Ethernet 14 + IPv4 20 + fixed TCP header 20, so the
first payload byte (offset >= 54) can never be stored. `verify` additionally fails closed on any
packet that is not untagged Ethernet/IPv4/TCP or whose captured length exceeds 54; `capture`
deletes a file that fails it. The pcap itself stays private; only derived counters are published
and they contain neither addresses nor ports.
"""
import argparse
import hashlib
import json
from pathlib import Path
import signal
import re
import statistics
import struct
import subprocess
import time

SNAPLEN = 54
ETHERNET = 1
MAGICS = {b'\xd4\xc3\xb2\xa1': ('<', 1e-6), b'\xa1\xb2\xc3\xd4': ('>', 1e-6),
          b'\x4d\x3c\xb2\xa1': ('<', 1e-9), b'\xa1\xb2\x3c\x4d': ('>', 1e-9)}
SYN, FIN, RST, ACK = 0x02, 0x01, 0x04, 0x10


class NotPayloadFree(Exception):
    pass


def packets(path):
    """Yield (timestamp, captured_bytes, original_length). Fail closed on every irregularity:
    unknown format or link type, truncated or trailing bytes, a record whose captured length is
    not exactly min(original, snaplen)."""
    data = Path(path).read_bytes()
    if len(data) < 24 or data[:4] not in MAGICS:
        raise NotPayloadFree('not a classic pcap file')
    order, scale = MAGICS[data[:4]]
    if struct.unpack(order + 'I', data[20:24])[0] != ETHERNET:
        raise NotPayloadFree('link type is not Ethernet')
    position = 24
    while position < len(data):
        if position + 16 > len(data):
            raise NotPayloadFree('trailing bytes after the last complete record')
        seconds, fraction, captured, original = struct.unpack(order + 'IIII', data[position:position + 16])
        position += 16
        if captured != min(original, SNAPLEN):
            raise NotPayloadFree('captured length is not min(original, snaplen)')
        if position + captured > len(data):
            raise NotPayloadFree('record data truncated')
        yield seconds + fraction * scale, data[position:position + captured], original
        position += captured


def parse(timestamp, frame, original):
    """Only untagged Ethernet / IPv4 (IHL 5, unfragmented) / TCP with a consistent header set."""
    if len(frame) != SNAPLEN or struct.unpack('!H', frame[12:14])[0] != 0x0800:
        raise NotPayloadFree('packet is not untagged Ethernet/IPv4 with full headers')
    ip = frame[14:]
    if ip[0] >> 4 != 4 or ip[9] != 6:
        raise NotPayloadFree('packet is not IPv4/TCP')
    if ip[0] & 0x0F != 5:
        raise NotPayloadFree('IP options present or invalid IHL')
    total = struct.unpack('!H', ip[2:4])[0]
    if struct.unpack('!H', ip[6:8])[0] & 0x3FFF:
        raise NotPayloadFree('fragmented IPv4 (MF or fragment offset set)')
    source, destination = ip[12:16], ip[16:20]
    sport, dport, seq, ack, offset_flags, window = struct.unpack('!HHIIHH', ip[20:36])
    words = offset_flags >> 12
    if words < 5:
        raise NotPayloadFree('invalid TCP header length')
    header = words * 4
    if total < 20 + header or total > original - 14:
        raise NotPayloadFree('IP total length inconsistent with the TCP header or the wire length')
    if len(frame) > 14 + 20 + header:
        raise NotPayloadFree('stored bytes extend past the TCP header')
    return {'t': timestamp, 'src': source, 'dst': destination, 'sport': sport, 'dport': dport,
            'seq': seq, 'ack': ack, 'flags': offset_flags & 0x3F, 'window': window,
            'length': total - 20 - header}


def verify(path):
    """Number of verified packets; an empty capture is invalid, never a PASS."""
    count = 0
    for timestamp, frame, original in packets(path):
        parse(timestamp, frame, original)
        count += 1
    if count == 0:
        raise NotPayloadFree('capture holds no packets')
    return count


def analyze(path, device_ip, bin_seconds=10.0):
    device = bytes(int(part) for part in device_ip.split('.'))
    flows = {}
    first = None
    totals = dict(packets=0, device_data_segments=0, device_retransmitted_segments=0,
                  host_pure_dupacks=0, zero_window_from_device=0, zero_window_from_host=0,
                  syn_retransmits=0, device_rst=0, host_rst=0)
    bins = {}

    def bump(t, key):
        index = int((t - first) // bin_seconds)
        bins.setdefault(index, dict.fromkeys(
            ('device_retransmitted_segments', 'host_pure_dupacks', 'zero_window_from_device',
             'syn_retransmits'), 0))[key] += 1

    for timestamp, frame, original in packets(path):
        p = parse(timestamp, frame, original)
        if device not in (p['src'], p['dst']):
            continue
        first = timestamp if first is None else first
        totals['packets'] += 1
        from_device = p['src'] == device
        port = p['dport'] if from_device else p['sport']
        flow = flows.setdefault(port, {'syn': [], 'synack': None, 'request': None, 'reply': None,
                                       'dev_end': None, 'last_host_ack': None})
        if p['flags'] & RST:
            totals['device_rst' if from_device else 'host_rst'] += 1
        if p['window'] == 0 and not p['flags'] & (SYN | RST):
            totals['zero_window_from_device' if from_device else 'zero_window_from_host'] += 1
            if from_device:
                bump(timestamp, 'zero_window_from_device')
        if not from_device:
            if p['flags'] & SYN and not p['flags'] & ACK:
                if flow['syn']:
                    totals['syn_retransmits'] += 1
                    bump(timestamp, 'syn_retransmits')
                flow['syn'].append(timestamp)
            elif p['length'] and flow['request'] is None:
                flow['request'] = timestamp
            elif (p['flags'] & ACK and not p['length'] and not p['flags'] & (SYN | FIN | RST)
                  and flow['last_host_ack'] == (p['ack'], p['window'])):
                totals['host_pure_dupacks'] += 1
                bump(timestamp, 'host_pure_dupacks')
            if p['flags'] & ACK:
                flow['last_host_ack'] = (p['ack'], p['window'])
        else:
            if p['flags'] & SYN and p['flags'] & ACK and flow['synack'] is None and flow['syn']:
                flow['synack'] = timestamp
            if p['length']:
                totals['device_data_segments'] += 1
                end = p['seq'] + p['length']
                if flow['dev_end'] is not None and end <= flow['dev_end']:
                    totals['device_retransmitted_segments'] += 1
                    bump(timestamp, 'device_retransmitted_segments')
                flow['dev_end'] = end if flow['dev_end'] is None else max(flow['dev_end'], end)
                if flow['reply'] is None and flow['request'] is not None:
                    flow['reply'] = timestamp

    def milliseconds(pairs):
        values = sorted((b - a) * 1000 for a, b in pairs)
        if not values:
            return None
        return {'n': len(values), 'median': round(statistics.median(values), 1),
                'max': round(values[-1], 1)}
    return {'format': 'g1b-pcap-derived-v1', 'payload_free_verified': True, **totals,
            'connections': len(flows),
            'connections_without_synack': sum(1 for f in flows.values() if f['syn'] and f['synack'] is None),
            'connections_without_reply': sum(1 for f in flows.values() if f['request'] and f['reply'] is None),
            'synack_delay_ms': milliseconds([(f['syn'][0], f['synack']) for f in flows.values()
                                             if f['syn'] and f['synack']]),
            'time_to_first_reply_ms': milliseconds([(f['request'], f['reply']) for f in flows.values()
                                                    if f['request'] and f['reply']]),
            'bin_seconds': bin_seconds, 'bins': {str(k): v for k, v in sorted(bins.items())}}


def inside_git_worktree(path):
    return any((parent / '.git').exists() for parent in [Path(path).resolve(), *Path(path).resolve().parents])


def capture(args):
    """Exit codes: 0 verified capture; 2 not payload-free (file deleted); 3 tcpdump failed
    (start, permission, exit code, kill); 4 file missing/empty/incomplete; 5 interrupted;
    6 hard time limit reached without a stop signal (the run did not end). Anything but 0 is a
    HOLD for this run's wire evidence; a valid payload-free file of a non-zero run is kept."""
    if not args.run or not args.gate_id:
        raise SystemExit('No capture without --run and an approved --gate-id')
    if inside_git_worktree(args.output):
        raise SystemExit('Refusing to store a private capture inside a git work tree')
    args.output.parent.mkdir(parents=True, exist_ok=True)
    result = {'tool_sha256': hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
              'snaplen': SNAPLEN, 'max_seconds': args.max_seconds}
    stops = [args.output.parent / 'PCAP_STOP', *args.stop_file]
    command = ['tcpdump', '-i', args.interface, '-n', '-U', '-s', str(SNAPLEN), '-w', str(args.output),
               'tcp and port 80 and host ' + args.device_ip]
    try:
        process = subprocess.Popen(command, stdout=subprocess.DEVNULL, stderr=subprocess.PIPE)
    except OSError:
        return finish(args, result, 3, 'tcpdump-not-startable')
    code, reason = None, 'stop-signal'
    began = time.monotonic()
    try:
        time.sleep(1.0)
        if process.poll() is not None:
            code, reason = 3, 'tcpdump-exited-at-start'  # typically a missing capture permission
        else:
            while True:
                if process.poll() is not None:
                    code, reason = 3, 'tcpdump-exited-during-run'
                    break
                if any(path.exists() for path in stops):
                    break
                if time.monotonic() - began >= args.max_seconds:
                    code, reason = 6, 'hard-time-limit'
                    break
                time.sleep(0.2)
    except KeyboardInterrupt:
        code, reason = 5, 'interrupted'
    finally:
        stderr = stop_process(process, result)
    result['tcpdump_returncode'] = process.returncode
    if code is None and (result.get('kill_needed') or result.get('wait_failed')):
        code, reason = 3, 'tcpdump-not-terminating'
    elif code is None and process.returncode != 0:
        code, reason = 3, 'tcpdump-nonzero-exit'
    def hold(default_code, why):
        # The first failure stays the reason; a later finding is recorded beside it.
        if code:
            result['also'] = why
        return finish(args, result, code or default_code, reason if code else why)
    try:
        packets_verified = verify(args.output)
    except FileNotFoundError:
        return hold(4, 'capture-file-missing')
    except NotPayloadFree as error:
        if 'no packets' in str(error):
            return hold(4, 'capture-empty')
        args.output.unlink(missing_ok=True)
        result['also'] = 'not-payload-free: ' + str(error)
        return finish(args, result, 2, result['also'])
    result['packets_verified'] = packets_verified
    reported = re.search(r'(\d+) packets? captured', stderr)
    result['packets_reported_by_tcpdump'] = int(reported[1]) if reported else None
    if not reported or int(reported[1]) != packets_verified:
        return hold(4, 'capture-incomplete-or-unreported')
    return finish(args, result, code or 0, reason)


def stop_process(process, result):
    """Always ends tcpdump: SIGINT, then terminate, then kill; returns its stderr text."""
    if process.poll() is None:
        try:
            process.send_signal(signal.SIGINT)
        except OSError:
            pass
    for step in ('interrupt', 'terminate', 'kill'):
        try:
            process.wait(timeout=10 if step == 'interrupt' else 5)
            break
        except subprocess.TimeoutExpired:
            result['kill_needed'] = True
            (process.terminate if step == 'interrupt' else process.kill)()
        except Exception:
            result['wait_failed'] = True
            try:
                process.kill()
            except OSError:
                pass
            break
    try:
        return (process.stderr.read() or b'').decode(errors='replace') if process.stderr else ''
    except Exception:
        return ''


def finish(args, result, code, reason):
    result.update(status='PASS' if code == 0 else 'HOLD', exit_code=code, reason=reason)
    (args.output.parent / 'pcap_result.json').write_text(json.dumps(result, indent=2) + '\n')
    if code:
        print('HOLD:', reason)
    return code


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest='command', required=True)
    cap = sub.add_parser('capture')
    cap.add_argument('--run', action='store_true')
    cap.add_argument('--gate-id')
    cap.add_argument('--interface', required=True)
    cap.add_argument('--device-ip', required=True)
    cap.add_argument('--output', type=Path, required=True)
    cap.add_argument('--stop-file', type=Path, action='append', default=[],
                     help='ends the capture when it appears (pass the run ABORT/STOP files)')
    cap.add_argument('--max-seconds', type=float, default=1800.0, help='hard upper limit')
    ver = sub.add_parser('verify')
    ver.add_argument('pcap', type=Path)
    ana = sub.add_parser('analyze')
    ana.add_argument('pcap', type=Path)
    ana.add_argument('--device-ip', required=True)
    options = parser.parse_args()
    if options.command == 'capture':
        raise SystemExit(capture(options))
    if options.command == 'verify':
        print(verify(options.pcap), 'packets, payload-free')
    else:
        verify(options.pcap)
        print(json.dumps(analyze(options.pcap, options.device_ip), indent=2))
