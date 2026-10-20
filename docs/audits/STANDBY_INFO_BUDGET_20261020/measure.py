#!/usr/bin/env python3
"""Misst die Shell-Bytegroesse der Kandidaten (Suffix + Kopf 1397 B).

Aufruf aus dem Repo-Root: python3 -I docs/audits/STANDBY_INFO_BUDGET_20261020/measure.py
Basisabgleich: extrahiert kWebShellSuffix aus web_application_routes.cpp und
muss 9013 B ergeben (10410 B gesamt mit Kopf 1397 B)."""
import json, re, pathlib
root = pathlib.Path(__file__).resolve().parents[3]
src = (root / 'lib/fermentation_app/src/web_application_routes.cpp').read_text(encoding='utf8')
m = re.search(r'constexpr char kWebShellSuffix\[\] =([\s\S]*?);\n\nstd::string provisioningShell', src)
base = ''.join(json.loads(x.group(0)) for x in re.finditer(r'"(?:[^"\\]|\\.)*"', m.group(1)))
HEAD, LIMIT, RESERVE = 1397, 10752, 128
NEW_LIMIT = 11264  # Ownerentscheid D1 = D (Plan-Revision 2), nur fuer F2
b = lambda s: len(s.encode('utf8'))
assert b(base) == 9013 and HEAD + b(base) == 10410, 'Basisabgleich fehlgeschlagen'
print(f"{'Kandidat':8s} {'Delta':>6s} {'Gesamt':>7s} {'Spielraum ueber Normalreserve':>26s} {'Limit noetig':>13s}")
print(f"{'BASIS':8s} {0:6d} {HEAD+b(base):7d} {LIMIT-RESERVE-HEAD-b(base):26d} {HEAD+b(base)+RESERVE:13d}")
for f in sorted((pathlib.Path(__file__).parent / 'candidates').glob('*.txt')):
    s = f.read_text(encoding='utf8'); d = b(s) - b(base); t = HEAD + b(s)
    print(f"{f.stem:8s} {d:6d} {t:7d} {LIMIT-RESERVE-t:26d} {t+RESERVE:13d}")

f2 = HEAD + b((pathlib.Path(__file__).parent / 'candidates' / 'F2.txt').read_text(encoding='utf8'))
print(f'F2 gegen Limit {NEW_LIMIT}: Reserve {NEW_LIMIT - f2} B, davon Normalreserve {RESERVE} B, Spielraum {NEW_LIMIT - f2 - RESERVE} B')
