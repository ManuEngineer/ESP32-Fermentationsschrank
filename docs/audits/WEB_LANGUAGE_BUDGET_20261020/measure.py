#!/usr/bin/env python3
"""Misst die Shell-Bytegroesse der Sprachwahl-Kandidaten.

Aufruf aus dem Repo-Root: python3 -I docs/audits/WEB_LANGUAGE_BUDGET_20261020/measure.py
Basisabgleich: kWebShellSuffix aus web_application_routes.cpp muss 9619 B ergeben
(Kopf 1397 B, Gesamt 11016 B, Limit 11264 B, Normalreserve 128 B).
Eine Kandidatendatei enthaelt in Zeile 1 die zusaetzliche Kopf-Markup (<select>),
ab Zeile 2 den vollstaendigen Suffix."""
import json, pathlib, re
root = pathlib.Path(__file__).resolve().parents[3]
src = (root / 'lib/fermentation_app/src/web_application_routes.cpp').read_text(encoding='utf8')
m = re.search(r'constexpr char kWebShellSuffix\[\] =([\s\S]*?);\n\nstd::string provisioningShell', src)
base = ''.join(json.loads(x.group(0)) for x in re.finditer(r'"(?:[^"\\]|\\.)*"', m.group(1)))
HEAD, LIMIT, RESERVE, HEAD_CAP, SEQ = 1397, 11264, 128, 1536, 19
b = lambda s: len(s.encode('utf8'))
assert b(base) == 9619 and HEAD + b(base) == 11016, 'Basisabgleich fehlgeschlagen'
print(f"{'Kandidat':34s} {'Kopf+':>5s} {'Suffix+':>7s} {'Gesamt':>6s} {'Kopf+Seq':>8s} {'Spielraum@11264':>15s} {'Limit noetig':>12s}")
for f in sorted((pathlib.Path(__file__).parent / 'candidates').glob('*__*.txt')):
    head, suffix = f.read_text(encoding='utf8').split('\n', 1)
    total = HEAD + b(head) + b(suffix)
    print(f"{f.stem:34s} {b(head):5d} {b(suffix)-b(base):7d} {total:6d} {HEAD+b(head)+SEQ:8d} {LIMIT-RESERVE-total:15d} {total+RESERVE:12d}")
