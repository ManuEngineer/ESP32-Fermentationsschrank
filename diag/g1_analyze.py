"""Offline, conservative G1 UI checkpoint summary. Never a hardware PASS oracle."""
import argparse
import json
from pathlib import Path
import re


def analyze(text):
    records=[]
    invalid=0
    for line in text.splitlines():
        if 'g1_ui:' not in line:
            continue
        row={k:int(v) for k,v in re.findall(r'(\w+)=(\d+)',line)}
        required={'phase','page','locale','home','hold','kind','fresh','skip','held','release','allowed','n','pre','post','low','block_pre','block_post','block_low','demand','max_text_bytes','max_text_seen'}
        if not required.issubset(row) or row['demand']!=max(0,row['pre']-row['low']):
            invalid+=1
            continue
        records.append(row)
    measured=[r for r in records if not r['skip']]
    maximum=max((r['demand'] for r in measured),default=None)
    missing_touch=not any(r['phase']==1 and r['held'] and r['fresh'] for r in measured)
    coverage=sorted({(r['phase'],r['page'],r['locale'],r['home'],r['hold'],r['kind']) for r in measured})
    skipped=sorted({(r['page'],r['locale']) for r in records if r['skip']})
    rendered={(r['page'],r['locale']) for r in measured if r['phase']==2}
    return {'status':'HOLD_PENDING_INDEPENDENT_RESULT_REVIEW_AND_COVERAGE',
            'threshold_status':'NOT_QUALIFIED','valid_aggregates':len(records),
            'invalid_ui_lines':invalid,'observed_max_checkpoint_demand_bytes':maximum,
            'vector_bound_bytes':6656,
            'arithmetic_candidate_bytes_NOT_QUALIFIED':None if maximum is None else 2*max(6656,maximum),
            'touch_contact_evidence':'NOT_PROVEN' if missing_touch else 'OBSERVED_REQUIRES_CORRELATION',
            'physical_hold_skip_release_new_contact':'NOT_PROVEN_REQUIRES_REAL_SEQUENCE',
            'measured_phase_page_locale_home_hold_kind':coverage,
            'skipped_page_locale_without_measured_render':sorted(set(skipped)-rendered),
            'variant_coverage':'NOT_PROVEN_REQUIRES_MANUAL_CASE_CORRELATION',
            'probe_overflow':'g1_probe_error' in text or any(int(v)>0 for v in re.findall(r'dropped=(\d+)',text)),
            'limits':['checkpoints miss between-sample transients; not exact peak',
                      'parallel WLAN/HTTP/diagnostic activity contributes',
                      'free and largest-block queries are not atomic together',
                      'boot/per-region low watermarks are not window minima',
                      'render argument allocations precede render scope; step includes them',
                      'skip context is cached, not a fresh snapshot; never demand=0 evidence',
                      'in-flight measurements on abort/reset may never flush',
                      'board/source/load/idle/stack/recovery and manual coverage reviewed separately']}


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--uart',type=Path,required=True)
    parser.add_argument('--output',type=Path,required=True)
    args=parser.parse_args()
    args.output.write_text(json.dumps(analyze(args.uart.read_text(errors='replace')),indent=2)+'\n')
