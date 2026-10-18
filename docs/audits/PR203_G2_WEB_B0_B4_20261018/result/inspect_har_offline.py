"""Read-only HAR metadata/body validation. Never contacts the device.

Allowlisted output: no URLs, cookies, request payloads, response bodies,
network identities or private paths. Input originals are not changed.
"""
import argparse
import base64
from collections import Counter
from datetime import datetime
import hashlib
import json
from pathlib import Path
import re
from urllib.parse import urlsplit

POLL = {'/api/v1/status', '/api/v1/temperatures', '/api/v1/alerts'}
ALLOWED = POLL | {'/', '/favicon.ico', '/api/v1/login', '/api/v1/programs', '/internal/ui/run'}


def inspect(file):
    raw = file.read_bytes()
    data = json.loads(raw.decode('utf-8-sig'))
    entries = data['log']['entries']
    rows, omitted = [], 0
    target = next((urlsplit(e['request']['url']).hostname for e in entries
                   if urlsplit(e['request']['url']).path == '/api/v1/status'), None)
    if not target:
        target = next((urlsplit(e['request']['url']).hostname for e in entries
                       if e.get('_resourceType') == 'document'), None)
    for index, entry in enumerate(entries):
        request, response = entry['request'], entry['response']
        url = urlsplit(request['url'])
        if url.hostname != target or url.path not in ALLOWED or url.query or url.fragment:
            omitted += 1
            continue
        method = request['method']
        assert method in ('GET', 'POST')
        error = response.get('_error')
        assert error is None or re.fullmatch(r'net::ERR_[A-Z0-9_]+', error)
        content = response.get('content', {})
        present = isinstance(content.get('text'), str)
        body = (base64.b64decode(content['text']) if content.get('encoding') == 'base64'
                else content.get('text', '').encode())
        row = {'index': index, 'path': url.path, 'method': method,
               'type': entry.get('_resourceType'),
               'start_utc_client': entry['startedDateTime'],
               'duration_ms': entry['time'], 'status': response['status'],
               'error': error, 'body_in_export': present,
               'decoded_body_bytes': len(body) if present else None,
               'timings_ms': {k: v for k, v in entry.get('timings', {}).items()
                              if k in ('blocked', 'dns', 'connect', 'ssl', 'send', 'wait', 'receive')},
               'response_headers': {h['name'].lower(): h['value'] for h in response.get('headers', [])
                                    if h['name'].lower() in ('content-type', 'transfer-encoding', 'content-length')}}
        initiator = entry.get('_initiator', {})
        row['initiator_type'] = initiator.get('type') if initiator.get('type') in ('other', 'script', 'parser', 'preload') else 'UNEXTRACTED'
        row['initiator_function_names'] = [frame.get('functionName') if frame.get('functionName') in ('send', 'rf', '') else 'UNEXTRACTED'
                                           for frame in initiator.get('stack', {}).get('callFrames', [])]
        if url.path == '/':
            row['html_ends_correctly'] = bool(present and body.rstrip().endswith(b'</html>'))
            row['html_sha256'] = hashlib.sha256(body).hexdigest() if present else None
        if url.path == '/internal/ui/run':
            try:
                intent = json.loads(request.get('postData', {}).get('text', '{}')).get('i', {}).get('t')
            except (ValueError, TypeError, AttributeError):
                intent = None
            row['intent_type'] = intent if intent in ('start-program', 'stop-run', 'complete-run', 'ack-message', 'mute-message') else 'UNEXTRACTED'
            try:
                code = json.loads(body).get('error')
            except (ValueError, TypeError, AttributeError):
                code = None
            row['response_error_code'] = code if code in ('sequence-required',) else 'UNEXTRACTED'
        if url.path in POLL:
            try:
                obj = json.loads(body) if present else None
                schema = (isinstance(obj, dict) and
                          (isinstance(obj.get('revisions'), dict) and isinstance(obj.get('homeMode'), str)
                           if url.path.endswith('/status') else
                           isinstance(obj.get('temperatures' if url.path.endswith('/temperatures') else 'alerts'), list)))
                row['json_parse_valid'] = obj is not None
                row['expected_snapshot_shape'] = bool(schema)
            except (ValueError, TypeError):
                row['json_parse_valid'] = False
                row['expected_snapshot_shape'] = False
        rows.append(row)
    polls = sorted((r for r in rows if r['path'] in POLL), key=lambda r: r['start_utc_client'])
    groups, unmatched = [], []
    for i in range(0, len(polls), 3):
        group = polls[i:i + 3]
        if len(group) != 3 or {r['path'] for r in group} != POLL:
            unmatched.extend(r['index'] for r in group)
            continue
        starts = [datetime.fromisoformat(r['start_utc_client'].replace('Z', '+00:00')).timestamp() for r in group]
        if max(starts) - min(starts) > 0.25:
            unmatched.extend(r['index'] for r in group)
            continue
        ends = [s + max(r['duration_ms'], 0) / 1000 for s, r in zip(starts, group)]
        groups.append({'indices': [r['index'] for r in group], 'start_epoch_client': min(starts),
                       'last_transport_end_epoch_client': max(ends),
                       'duration_ms': (max(ends) - min(starts)) * 1000,
                       'all_200': all(r['status'] == 200 for r in group),
                       'all_bodies_valid_json': all(r.get('json_parse_valid') for r in group),
                       'all_snapshot_shapes_valid': all(r.get('expected_snapshot_shape') for r in group),
                       'errors': [r['error'] for r in group if r['error']]})
    gaps = [(b['start_epoch_client'] - a['last_transport_end_epoch_client']) * 1000
            for a, b in zip(groups, groups[1:])]
    return {'input_sha256': hashlib.sha256(raw).hexdigest(), 'input_bytes': len(raw),
            'input_entries': len(entries), 'unrelated_or_nonallowlisted_entries_omitted': omitted,
            'status_counts': dict(Counter(str(r['status']) for r in rows)), 'requests': rows,
            'poll_transport_groups': groups, 'unmatched_poll_indices': unmatched,
            'transport_overlap_count': sum(g < -0.01 for g in gaps),
            'minimum_intergroup_transport_gap_ms': min(gaps, default=None),
            'limit': 'HAR transport end/body availability is not an instrumented timestamp of browser JSON promise settlement. Grouping uses consecutive allowlisted triads within250ms; unmatched entries remain unqualified. Private auth payloads and all response bodies omitted.'}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('har', type=Path)
    parser.add_argument('out', type=Path)
    args = parser.parse_args()
    assert args.har.resolve() != args.out.resolve()
    args.out.write_text(json.dumps(inspect(args.har), indent=2) + '\n')
