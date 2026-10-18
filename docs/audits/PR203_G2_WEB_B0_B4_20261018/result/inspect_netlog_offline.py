"""Adapted from the prior first-GET evidence derivation; offline allowlist only."""
import argparse
from collections import defaultdict
from datetime import datetime, timezone
import hashlib
import json
from pathlib import Path
import re
from urllib.parse import urlsplit

ALLOWED = {'/', '/favicon.ico', '/api/v1/status', '/api/v1/temperatures',
           '/api/v1/alerts', '/api/v1/login', '/api/v1/programs', '/internal/ui/run'}


def inspect(file, target):
    raw = file.read_bytes()
    net = json.loads(raw.decode('utf-8-sig'))
    constants = net['constants']
    names = {v: k for k, v in constants['logEventTypes'].items()}
    phases = {v: k for k, v in constants['logEventPhase'].items()}
    types = {v: k for k, v in constants['logSourceType'].items()}
    errors = {v: k for k, v in constants.get('netError', {}).items()}
    offset = int(constants['timeTickOffset'])
    by_source = defaultdict(list)
    for event in net['events']:
        by_source[event['source']['id']].append(event)

    def utc(ticks):
        return datetime.fromtimestamp((float(ticks) + offset) / 1000, timezone.utc).isoformat()

    def safe(event):
        p = event.get('params', {})
        result = {'ticks_ms': float(event['time']), 'utc_client': utc(event['time']),
                  'source_id': event['source']['id'], 'source_type': types[event['source']['type']],
                  'event': names[event['type']], 'phase': phases[event['phase']]}
        for key in ('net_error', 'byte_count'):
            if key in p:
                assert isinstance(p[key], (float, int))
                result[key] = p[key]
        if 'source_dependency' in p:
            result['source_dependency'] = {key: p['source_dependency'][key] for key in ('id', 'type')}
        if names[event['type']] == 'HTTP_TRANSACTION_READ_RESPONSE_HEADERS':
            result['http_statuses'] = [int(match[1]) for header in p.get('headers', [])
                                      if (match := re.match(r'HTTP/\S+ (\d{3})', header))]
        return result

    requests, roots, diagnostic_requests, omitted = [], [], [], 0
    all_sockets = set()
    for event in net['events']:
        if names[event['type']] != 'URL_REQUEST_START_JOB' or phases[event['phase']] != 'PHASE_BEGIN':
            continue
        p = event.get('params', {})
        url = urlsplit(p.get('url', ''))
        if url.hostname != target:
            continue
        if url.path not in ALLOWED or url.query or url.fragment:
            omitted += 1
            continue
        events = by_source[event['source']['id']]
        jobs = [by_source[e['params']['source_dependency']['id']] for e in events
                if names[e['type']] == 'HTTP_STREAM_REQUEST_BOUND_TO_JOB']
        sockets = sorted({e['params']['source_dependency']['id'] for job in jobs for e in job
                          if names[e['type']] == 'SOCKET_POOL_BOUND_TO_SOCKET'})
        all_sockets.update(sockets)
        statuses = []
        for e in events:
            if names[e['type']] == 'HTTP_TRANSACTION_READ_RESPONSE_HEADERS':
                for header in e.get('params', {}).get('headers', []):
                    match = re.match(r'HTTP/\S+ (\d{3})', header)
                    if match:
                        statuses.append(int(match[1]))
        net_errors = sorted({e['params']['net_error'] for e in events
                             if 'net_error' in e.get('params', {})})
        row = {'source_id': event['source']['id'], 'path': url.path, 'method': p.get('method'),
               'start_ticks_ms': float(event['time']), 'start_utc_client': utc(event['time']),
               'last_source_ticks_ms': float(events[-1]['time']),
               'last_source_utc_client': utc(events[-1]['time']),
               'statuses_received': statuses, 'socket_ids': sockets,
               'cancelled_event': any(names[e['type']] == 'CANCELLED' for e in events),
               'net_error_codes': net_errors,
               'net_error_classes': [errors.get(code, 'UNKNOWN_NUMERIC_CODE') for code in net_errors],
               'filtered_response_bytes': sum(e.get('params', {}).get('byte_count', 0) for e in events
                                              if names[e['type']] == 'URL_REQUEST_JOB_FILTERED_BYTES_READ'),
               'socket_pool_reuse_event': any(names[e['type']] == 'SOCKET_POOL_REUSED_AN_EXISTING_SOCKET'
                                              for e in events + [e for job in jobs for e in job])}
        requests.append(row)
        if url.path == '/internal/ui/run' or any(code < 0 for code in net_errors):
            diagnostic_requests.append({'request': row, 'events': [safe(e) for e in events],
                                        'stream_jobs': [[safe(e) for e in job] for job in jobs]})
        if url.path == '/':
            end = float(events[-1]['time'])
            roots.append({'request': row, 'events': [safe(e) for e in events],
                          'stream_jobs': [[safe(e) for e in job] for job in jobs],
                          'sockets_until_request_end': [{'id': sid,
                              'events': [safe(e) for e in by_source[sid] if float(e['time']) <= end]}
                              for sid in sockets]})
    socket_events = sorted([safe(e) for sid in all_sockets for e in by_source[sid]
                            if names[e['type']] in ('SOCKET_CLOSED', 'SOCKET_POOL_CLOSING_SOCKET',
                                                    'TCP_READ_ERROR', 'TCP_WRITE_ERROR')],
                           key=lambda e: (e['ticks_ms'], e['source_id']))
    return {'input_bytes': len(raw), 'input_sha256': hashlib.sha256(raw).hexdigest(),
            'capture_mode': constants['logCaptureMode'], 'time_tick_offset_ms': offset,
            'browser': {k: constants['clientInfo'].get(k) for k in ('name', 'version', 'official')},
            'unapproved_device_paths_omitted': omitted,
            'requests': requests, 'roots': roots, 'diagnostic_requests': diagnostic_requests,
            'socket_events': socket_events,
            'distinct_socket_ids_lifetime': len(all_sockets),
            'limits': 'Default NetLog: byte counts, no raw chunk framing. Numeric socket IDs are browser lifetime IDs, not simultaneous server sockets/TX buffers. No private headers, payloads, endpoints, or unrelated browser traffic.'}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--netlog', type=Path, required=True)
    parser.add_argument('--target', required=True)
    parser.add_argument('--out', type=Path, required=True)
    args = parser.parse_args()
    assert args.netlog.resolve() != args.out.resolve()
    args.out.write_text(json.dumps(inspect(args.netlog, args.target), indent=2) + '\n')
