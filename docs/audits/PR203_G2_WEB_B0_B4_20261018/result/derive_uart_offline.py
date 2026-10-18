"""Derive allowlisted UART evidence from a CLOSED private capture, offline only."""
import argparse
import csv
from datetime import datetime, timezone
import gzip
import hashlib
import io
import json
from pathlib import Path
import re


def derive(run, out):
    capdir = run / 'uart_capture'
    capture = json.loads((capdir / 'capture.json').read_text())
    assert not (capdir / 'ABORT').exists() and capture['failure'] is None
    lines = (capdir / 'uart.log').read_text(errors='replace').splitlines()
    reset = [i for i, line in enumerate(lines) if 'rst:0x' in line]
    assert len(reset) == 1 and 'rst:0x1 (POWERON_RESET)' in lines[reset[0]]
    post = lines[reset[0]:]
    offset = capture['start_epoch'] - capture['start_monotonic']
    heartbeats = []
    for line in post:
        match = re.search(r'\[host_mono=([\d.]+)\].*heartbeat: safe test mode, uptime_ms=(\d+)', line)
        if match:
            heartbeats.append((float(match[1]), int(match[2])))
    assert len(heartbeats) == capture['qualified_heartbeats']
    assert all(b[1] > a[1] for a, b in zip(heartbeats, heartbeats[1:]))
    chunks = [json.loads(line) for line in (capdir / 'uart_chunks.jsonl').read_text().splitlines()]
    assert all(set(chunk) == {'epoch', 'monotonic', 'bytes'} for chunk in chunks)
    assert sum(c['bytes'] for c in chunks) == (capdir / 'uart_raw.bin').stat().st_size
    out.mkdir(parents=True, exist_ok=True)

    def write(name, data):
        (out / name).write_text(json.dumps(data, indent=2) + '\n')

    compressed = {}
    for name, header, rows in (
        ('HEARTBEATS.csv.gz', ['host_monotonic', 'board_uptime_ms'], heartbeats),
        ('UART_CHUNKS.csv.gz', ['host_epoch', 'host_monotonic', 'byte_count'],
         [(c['epoch'], c['monotonic'], c['bytes']) for c in chunks]),
    ):
        stream = io.StringIO(newline='')
        writer = csv.writer(stream, lineterminator='\n')
        writer.writerow(header)
        writer.writerows(rows)
        raw = stream.getvalue().encode()
        packed = gzip.compress(raw, mtime=0)
        (out / name).write_bytes(packed)
        compressed[name] = {'rows': len(rows), 'uncompressed_bytes': len(raw),
                            'uncompressed_sha256': hashlib.sha256(raw).hexdigest(),
                            'compressed_bytes': len(packed),
                            'compressed_sha256': hashlib.sha256(packed).hexdigest(),
                            'transformation': 'CSV selected numeric fields; gzip mtime0; no raw bytes'}
    gaps = [b[0] - a[0] for a, b in zip(heartbeats, heartbeats[1:])]
    chunkgaps = [b['monotonic'] - a['monotonic'] for a, b in zip(chunks, chunks[1:])]
    epochgaps = [b['epoch'] - a['epoch'] for a, b in zip(chunks, chunks[1:])]
    offsets = [c['epoch'] - c['monotonic'] for c in chunks]
    write('CAPTURE.json', capture)
    write('UART_CONTINUITY.json', {
        'capture_start_utc': datetime.fromtimestamp(capture['start_epoch'], timezone.utc).isoformat(),
        'capture_end_utc': datetime.fromtimestamp(capture['end_epoch'], timezone.utc).isoformat(),
        'capture_duration_monotonic_s': capture['end_monotonic'] - capture['start_monotonic'],
        'epoch_minus_monotonic_at_start_s': offset,
        'expected_initial_reset_count': len(reset), 'qualified_heartbeat_count': len(heartbeats),
        'pre_boot_lines_ignored': capture['pre_boot_lines_ignored'],
        'strictly_increasing_uptime': True,
        'max_post_boot_heartbeat_gap_s': max(gaps),
        'post_boot_gaps_above5s': sum(gap > 5 for gap in gaps),
        'max_uptime_step_ms': max(b[1] - a[1] for a, b in zip(heartbeats, heartbeats[1:])),
        'chunk_monotonic_max_gap_s': max(chunkgaps), 'chunk_epoch_max_gap_s': max(epochgaps),
        'chunk_epoch_minus_monotonic_range_s': max(offsets) - min(offsets),
        'chunk_byte_count_matches_raw_bytes': True,
        'guard_failure': capture['failure'], 'compressed_numeric_evidence': compressed,
        'limit': 'UTC is host receipt/parsing time; no assertion of exact browser or UniFi clock offset. Capture max gap also includes initial boot.'})
    resources = [line for line in post if 'resources: point=' in line]
    (out / 'UART_RESOURCES.txt').write_text('\n'.join(resources) + '\n')
    markers = ('rst:0x1 (POWERON_RESET)', 'source git sha:', 'profile: esp32_release',
               'real actuators: disabled', 'application: ready')
    boot = [line for line in post if any(m in line for m in markers)]
    boot += [next(line for line in resources if 'network_state=HomeConnected' in line)]
    boot += [line for line in post if 'heartbeat: safe test mode, uptime_ms=' in line][:2]
    boot.sort(key=lambda line: float(re.search(r'host_mono=([\d.]+)', line)[1]))
    (out / 'UART_BOOT_PROOF.txt').write_text('\n'.join(boot) + '\n')
    contexts = []
    for i, line in enumerate(post):
        if 'httpd_sock_err:' not in line and 'httpd_resp_send_err:' not in line:
            continue
        before = [s for s in post[max(0, i-10):i] if 'heartbeat: safe test mode' in s][-2:]
        after = [s for s in post[i+1:i+11] if 'heartbeat: safe test mode' in s][:2]
        contexts += ['[selected context; unrelated lines omitted]', *before, line, *after, '']
    (out / 'UART_HTTP_WARNINGS.txt').write_text('\n'.join(contexts).rstrip() + '\n')
    write('PRETEST_24H_VALIDATION.json', json.loads((run / 'PRETEST_24H_VALIDATION.json').read_text()))
    write('PHASE_TIMING.json', {name: json.loads((run / (name + '_TIMING.json')).read_text())
                              for name in ('B2', 'B3', 'B4')})


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--run-dir', type=Path, required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    derive(args.run_dir, args.out)
