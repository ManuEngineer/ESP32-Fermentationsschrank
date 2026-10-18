"""Offline, allowlisted evidence extraction. Never opens UART or a network socket.

Original inputs remain private and unchanged. Output contains no HTTP bodies,
cookies, command lines, host addresses, network names or private file paths.
"""
import argparse
import base64
from collections import Counter, defaultdict
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
import shutil
from urllib.parse import urlsplit


def utc(epoch):
    return datetime.fromtimestamp(epoch, timezone.utc).isoformat()


def digest(path):
    data = path.read_bytes()
    return {'bytes': len(data), 'sha256': hashlib.sha256(data).hexdigest()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('har_initial', 'har_extended', 'netlog', 'run_dir', 'out'):
        parser.add_argument('--' + name.replace('_', '-'), type=Path, required=True)
    args = parser.parse_args()
    out = args.out
    out.mkdir(parents=True, exist_ok=True)

    def write(name, data):
        (out / name).write_text(json.dumps(data, indent=2, ensure_ascii=False) + '\n')

    h1, h2, net = [json.loads(p.read_text(encoding='utf-8-sig')) for p in
                   (args.har_initial, args.har_extended, args.netlog)]
    entries = h2['log']['entries']
    roots = [e for e in entries if e.get('_resourceType') == 'document']
    assert len(roots) == 1 and roots[0]['request']['method'] == 'GET'
    root = roots[0]
    target = urlsplit(root['request']['url']).hostname
    assert urlsplit(root['request']['url']).path == '/'
    assert h1['log']['entries'] == entries[:len(h1['log']['entries'])]
    paths = {'/', '/api/v1/status', '/api/v1/temperatures', '/api/v1/alerts', '/favicon.ico'}

    def path(url):
        parsed = urlsplit(url)
        assert parsed.hostname == target and parsed.path in paths
        assert not parsed.query and not parsed.fragment
        return parsed.path

    headers = {'content-type', 'transfer-encoding', 'content-length',
               'connection', 'keep-alive', 'cache-control', 'date'}
    response = root['response']
    content = response['content']
    body = (base64.b64decode(content['text']) if content.get('encoding') == 'base64'
            else content.get('text', '').encode())
    assert response['status'] == 200 and not response.get('_error')
    assert body.rstrip().endswith(b'</html>')
    har_rows = []
    for i, e in enumerate(entries):
        error = e['response'].get('_error')
        assert error is None or re.fullmatch(r'net::ERR_[A-Z_]+', error)
        har_rows.append({'index': i, 'path': path(e['request']['url']),
                         'method': e['request']['method'],
                         'start_utc_client': e['startedDateTime'], 'duration_ms': e['time'],
                         'status': e['response']['status'], 'error': error})
    epoch = lambda e: datetime.fromisoformat(e['startedDateTime'].replace('Z', '+00:00')).timestamp()
    groups = [entries[i:i + 3] for i in range(1, len(entries), 3)]
    assert all(len(g) == 3 and {path(e['request']['url']) for e in g} == paths - {'/', '/favicon.ico'} for g in groups)
    starts = [min(epoch(e) for e in g) for g in groups]
    ends = [max(epoch(e) + e['time'] / 1000 for e in g) for g in groups]
    write('HAR_SUMMARY.json', {
        'initial_entries': len(h1['log']['entries']), 'extended_entries': len(entries),
        'initial_is_byte_value_identical_entry_prefix': True, 'document_navigations': 1,
        'post_requests': sum(e['request']['method'] == 'POST' for e in entries),
        'root': {'start_utc_client': root['startedDateTime'], 'duration_ms': root['time'],
                 'status': response['status'], 'error': response.get('_error'),
                 'http_version': response['httpVersion'], 'timings_ms': root['timings'],
                 'headers': [h for h in response['headers'] if h['name'].lower() in headers],
                 'headers_bytes': response['headersSize'], 'encoded_body_bytes': response['bodySize'],
                 'transfer_bytes': response['_transferSize'], 'decoded_html_bytes': len(body),
                 'html_sha256': hashlib.sha256(body).hexdigest(), 'ends_html': True},
        'anonymous_groups': {'count': len(groups),
            'maximum_duration_ms': max((b - a) * 1000 for a, b in zip(starts, ends)),
            'minimum_intergroup_gap_ms': min((starts[i + 1] - ends[i]) * 1000 for i in range(len(groups) - 1)),
            'overlap_count': sum(starts[i + 1] < ends[i] for i in range(len(groups) - 1)),
            'qualification_limit': 'Anonymous 401/abort groups only; no authenticated JSON snapshot qualification'},
        'status_counts': dict(Counter(str(e['response']['status']) for e in entries)),
        'entries': har_rows})

    constants = net['constants']
    event_names = {v: k for k, v in constants['logEventTypes'].items()}
    phase_names = {v: k for k, v in constants['logEventPhase'].items()}
    source_names = {v: k for k, v in constants['logSourceType'].items()}
    tick_offset = int(constants['timeTickOffset'])
    by_source = defaultdict(list)
    for e in net['events']:
        by_source[e['source']['id']].append(e)

    def safe_event(e):
        p = e.get('params', {})
        result = {'time_ticks_ms': float(e['time']),
                  'utc_client': utc((float(e['time']) + tick_offset) / 1000),
                  'source_id': e['source']['id'], 'source_type': source_names[e['source']['type']],
                  'event': event_names[e['type']], 'phase': phase_names[e['phase']]}
        result.update({k: p[k] for k in ('net_error', 'byte_count') if k in p})
        if 'source_dependency' in p:
            result['source_dependency'] = {k: p['source_dependency'][k] for k in ('id', 'type')}
        if p.get('reason') == 'Connection was closed when it was returned to the pool':
            result['reason'] = p['reason']
        return result

    requests = []
    socket_ids = set()
    root_events = root_jobs = root_sockets = None
    root_end = None
    for e in net['events']:
        if event_names[e['type']] != 'URL_REQUEST_START_JOB' or phase_names[e['phase']] != 'PHASE_BEGIN':
            continue
        p = e.get('params', {})
        if urlsplit(p.get('url', '')).hostname != target:
            continue
        evs = by_source[e['source']['id']]
        jobs = [by_source[q['params']['source_dependency']['id']] for q in evs
                if event_names[q['type']] == 'HTTP_STREAM_REQUEST_BOUND_TO_JOB']
        ids = sorted({q['params']['source_dependency']['id'] for j in jobs for q in j
                      if event_names[q['type']] == 'SOCKET_POOL_BOUND_TO_SOCKET'})
        socket_ids.update(ids)
        statuses = []
        for q in evs:
            if event_names[q['type']] == 'HTTP_TRANSACTION_READ_RESPONSE_HEADERS':
                for line in q.get('params', {}).get('headers', []):
                    match = re.match(r'HTTP/\S+ (\d{3})', line)
                    if match:
                        statuses.append(int(match[1]))
        requests.append({'source_id': e['source']['id'], 'path': path(p['url']),
            'method': p.get('method'), 'start_utc_client': utc((float(e['time']) + tick_offset) / 1000),
            'last_source_event_utc_client': utc((float(evs[-1]['time']) + tick_offset) / 1000),
            'statuses_received': statuses, 'socket_ids': ids,
            'cancelled_event': any(event_names[q['type']] == 'CANCELLED' for q in evs),
            'net_errors': [q['params']['net_error'] for q in evs if 'net_error' in q.get('params', {})]})
        if path(p['url']) == '/':
            assert root_events is None
            root_events, root_jobs, root_sockets = evs, jobs, ids
            root_end = float(evs[-1]['time'])
    assert root_events is not None and len(root_sockets) == 1
    filtered = sum(e.get('params', {}).get('byte_count', 0) for e in root_events
                   if event_names[e['type']] == 'URL_REQUEST_JOB_FILTERED_BYTES_READ')
    assert filtered == len(body)
    write('NETLOG_REQUESTS.json', {'capture_mode': constants['logCaptureMode'],
        'browser': {k: constants['clientInfo'][k] for k in ('name', 'version', 'official')},
        'time_tick_offset_ms': tick_offset, 'requests': requests,
        'distinct_socket_ids_over_entire_trace': len(socket_ids),
        'socket_count_limit': 'Lifetime IDs, not simultaneous server sockets or TX buffer count'})
    write('NETLOG_ROOT.json', {'filtered_html_bytes': filtered,
        'events': [safe_event(e) for e in root_events],
        'jobs': [[safe_event(e) for e in j] for j in root_jobs],
        'sockets_through_root_completion': [{
            'id': sid, 'events': [safe_event(e) for e in by_source[sid] if float(e['time']) <= root_end]}
            for sid in root_sockets],
        'raw_wire_bytes_available': False,
        'limit': 'Default NetLog records byte counts; no literal chunk framing dump'})
    closes = sorted([safe_event(e) for sid in socket_ids for e in by_source[sid]
        if event_names[e['type']] in ('SOCKET_CLOSED', 'SOCKET_POOL_CLOSING_SOCKET', 'TCP_READ_ERROR', 'TCP_WRITE_ERROR')],
        key=lambda e: (e['time_ticks_ms'], e['source_id'], e['event']))
    write('NETLOG_SOCKET_EVENTS.json', closes)

    run = args.run_dir
    capture_dir = run / 'uart_capture'
    capture = json.loads((capture_dir / 'capture.json').read_text())
    runtime = json.loads((run / 'RUNTIME.json').read_text())
    lines = (capture_dir / 'uart.log').read_text(errors='replace').splitlines()
    reset_index = next(i for i, line in enumerate(lines) if 'rst:0x1 (POWERON_RESET)' in line)
    post = lines[reset_index:]
    host_offset = capture['start_epoch'] - capture['start_monotonic']
    heartbeats = []
    for line in post:
        m = re.search(r'\[host_mono=([\d.]+)\].*heartbeat: safe test mode, uptime_ms=(\d+)', line)
        if m:
            heartbeats.append({'host_monotonic': float(m[1]), 'uptime_ms': int(m[2])})
    assert len(heartbeats) == capture['qualified_heartbeats']
    assert all(b['uptime_ms'] > a['uptime_ms'] for a, b in zip(heartbeats, heartbeats[1:]))
    write('HEARTBEATS.json', heartbeats)
    shutil.copyfile(capture_dir / 'capture.json', out / 'CAPTURE.json')
    shutil.copyfile(capture_dir / 'uart_chunks.jsonl', out / 'UART_CHUNKS.jsonl')
    resource_lines = [line for line in post if 'resources: point=' in line]
    (out / 'UART_RESOURCES.txt').write_text('\n'.join(resource_lines) + '\n')
    markers = ('rst:0x1 (POWERON_RESET)', 'source git sha:', 'profile: esp32_release',
               'real actuators: disabled', 'application: ready')
    boot = [line for line in post if any(marker in line for marker in markers)]
    boot += [next(line for line in post if 'network_state=HomeConnected' in line and 'resources: point=' in line)]
    boot += [line for line in post if 'heartbeat: safe test mode, uptime_ms=' in line][:2]
    boot.sort(key=lambda line: float(re.search(r'host_mono=([\d.]+)', line)[1]))
    (out / 'UART_BOOT_PROOF.txt').write_text('\n'.join(boot) + '\n')
    contexts, correlations = [], []
    for i, line in enumerate(post):
        if 'httpd_sock_err:' not in line and 'httpd_resp_send_err:' not in line:
            continue
        before = [s for s in post[:i] if 'heartbeat: safe test mode' in s][-2:]
        after = [s for s in post[i + 1:] if 'heartbeat: safe test mode' in s][:2]
        contexts += ['[selected context; unrelated lines omitted]', *before, line, *after, '']
        stamp = float(re.search(r'host_mono=([\d.]+)', line)[1])
        ep = stamp + host_offset
        distance = min(abs((e['time_ticks_ms'] + tick_offset) / 1000 - ep) for e in closes)
        nearest = [e for e in closes if abs(abs((e['time_ticks_ms'] + tick_offset) / 1000 - ep) - distance) < 0.000001]
        correlations.append({'uart_line': line, 'host_utc_nominal': utc(ep),
            'nearest_nominal_browser_socket_events': nearest,
            'nearest_absolute_nominal_distance_ms': distance * 1000,
            'qualification': 'Clock offset unmeasured; nearest-time association is not socket identity or causality'})
    (out / 'UART_HTTP_WARNINGS.txt').write_text('\n'.join(contexts))
    write('CORRELATION.json', correlations)
    write('TIMELINE.json', {
        'host_capture_start_utc': utc(capture['start_epoch']), 'host_capture_end_utc': utc(capture['end_epoch']),
        'host_capture_start_monotonic': capture['start_monotonic'], 'host_capture_end_monotonic': capture['end_monotonic'],
        'capture_duration_s': capture['end_monotonic'] - capture['start_monotonic'],
        'host_epoch_minus_monotonic_start': host_offset,
        'host_mapping_drift_s': (capture['end_epoch'] - capture['end_monotonic']) - host_offset,
        'idle_start_utc': runtime['utc_start'], 'idle_start_monotonic': runtime['monotonic_start'],
        'idle_minimum_s': 1200, 'measured_idle_before_go_s': runtime['idle_verified_seconds'],
        'go_epoch_host': runtime['first_get_authorization_host_epoch'],
        'go_monotonic_host': runtime['first_get_authorization_host_monotonic'],
        'first_get_utc_client': root['startedDateTime'],
        'netlog_first_event_utc_client': utc((float(net['events'][0]['time']) + tick_offset) / 1000),
        'netlog_last_event_utc_client': utc((float(net['events'][-1]['time']) + tick_offset) / 1000),
        'windows_manual_utc_report': '2026-10-17T07:26:05.3613506Z',
        'host_ntp_synchronized_reported': True,
        'windows_host_clock_offset': 'NOT_MEASURED; message latency is not clock offset',
        'unifi_timezone_and_clock_offset': 'NOT_PROVEN',
        'qualification': 'Browser UTC uses client clock. Host UART times are host receipt/parse times. No sub-second cross-device causality assertion.'})
    environment = json.loads((run / 'BROWSER_ENVIRONMENT.json').read_text())
    ready = json.loads((run / 'FIRST_GET_READY.json').read_text())
    write('UNIFI_OBSERVATIONS.json', {'during_idle': environment['unifi'],
        'before_get': ready['unifi_owner_observation'],
        'interpretation': 'Owner screenshots, private originals retained; no RF cause established; connected-since is not current time or proof across boot'})
    inputs = {'HAR_INITIAL': args.har_initial, 'HAR_EXTENDED': args.har_extended, 'NETLOG': args.netlog,
              'UART_RAW': capture_dir / 'uart_raw.bin', 'UART_LOG': capture_dir / 'uart.log',
              'UART_CHUNKS': capture_dir / 'uart_chunks.jsonl', 'CAPTURE': capture_dir / 'capture.json',
              'MONITOR_SOURCE': run / 'SOURCE_g1_capture.py', 'LOAD_SOURCE_NOT_RUN': run / 'SOURCE_c0_load.py',
              'UNIFI_IDLE_ORIGINAL': run / 'UNIFI_IDLE_OWNER_SCREENSHOT.png',
              'UNIFI_PRE_GET_ORIGINAL': run / 'UNIFI_PRE_GET_OWNER_SCREENSHOT.png'}
    write('ARTIFACT_MANIFEST.json', {
        'product_pr_head_before_evidence': '2443dc96e40cc056bd38e7a932563b106ba5ca9d',
        'product_source': 'a9a1156a1d44d4679957526bbc34a30be6422e4e',
        'monitor_commit': '893f3bb73a50b484ce79b3b80722873452d998fc',
        'app_sha256': '2d069d2db793359177379f492559c69fa949cb470351d57f60dd095889619305',
        'elf_sha256': 'dca3cc664f3c143797f1caf29ca51bd3b9afe88eb081f914bec595f065523601',
        'c5_fallback_sha256': 'ebb296c08b79045e6e081f328c9f5fd7e576e20722670968b131e722a8b36633',
        'private_inputs': {k: digest(v) for k, v in inputs.items()},
        'derivation_script': digest(Path(__file__)),
        'sanitization': 'Allowlisted paths, timing, status/error codes, numeric source IDs, byte counts, safe content/connection headers and fixed safe event text. No credential headers, HTTP bodies, network identities, private paths or unrelated NetLog traffic. Selected UART lines LF-normalized; omissions marked. CAPTURE and chunk metadata byte-exact.',
        'limitations': 'Raw inputs private; public extracts cannot replace independent private raw-data access. No raw chunk terminator bytes in Default NetLog.'})
    print('Offline allowlisted extraction complete; original inputs unchanged.')


if __name__ == '__main__':
    main()
