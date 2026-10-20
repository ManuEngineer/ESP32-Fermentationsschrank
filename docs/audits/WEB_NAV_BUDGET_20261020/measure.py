#!/usr/bin/env python3
"""Misst die Optionen des Navigations-Plans (Plans issue-27-r1-web-remaining-scope-plan.md (fruehere Revision: reiner Navigationsplan)).

Aufruf aus dem Repo-Root: python3 -I docs/audits/WEB_NAV_BUDGET_20261020/measure.py
Basisabgleich: kWebShellSuffix aus web_application_routes.cpp = 9797 B, Shell 11312 B
(Kopf mit Sequenz '1' 1515 B), Limit 11456 B, Normalreserve 128 B => 16 B Spielraum.
terser ist optional (Zahlen 'n/a' ohne)."""
import json, pathlib, re, shutil, subprocess
root = pathlib.Path(__file__).resolve().parents[3]
here = pathlib.Path(__file__).parent
src = (root / 'lib/fermentation_app/src/web_application_routes.cpp').read_text(encoding='utf8')
m = re.search(r'constexpr char kWebShellSuffix\[\] =([\s\S]*?);\n\nstd::string provisioningShell', src)
base = ''.join(json.loads(x.group(0)) for x in re.finditer(r'"(?:[^"\\]|\\.)*"', m.group(1)))
b = lambda s: len(s.encode('utf8'))
HEAD, LIMIT, RESERVE, OPS_BUTTONS = 1515, 11456, 128, b('<button data-a="ack"></button> <button data-a="mute"></button> ')
assert b(base) == 9797 and HEAD + b(base) == 11312, 'Basisabgleich fehlgeschlagen'
spare = LIMIT - RESERVE - (HEAD + b(base))
def terse(suffix):
    if not shutil.which('terser'): return None
    body = suffix.split('</script>')[0]
    js = 'let S=window.a,A=window.b,U=window.c,Q=window.d' + body
    out = subprocess.run(['terser', '-c', '-m', 'reserved=[S,A,U,Q]', '--toplevel', '--ecma', '2020'],
                         input=js, capture_output=True, text=True).stdout
    return b(out) - b('let S=window.a,A=window.b,U=window.c,Q=window.d') + b('</script></body></html>') if out else None
tb = terse(base)
print(f'Basis: Suffix {b(base)} B, Shell {HEAD + b(base)} B, Spielraum ueber Normalreserve {spare} B; terser(Basis) = {tb}')
for f in sorted((here / 'candidates').glob('*.suffix.txt')):
    s = f.read_text(encoding='utf8'); d = b(s) - b(base)
    head = -OPS_BUTTONS if 'no_ops' in f.name else 0
    t = terse(s)
    td = None if (t is None or tb is None) else t - b(base)
    print(f'{f.name:62s} Suffix {d:+5d}  Kopf {head:+4d}  Shell {HEAD + head + b(base) + d} B  Spielraum {spare - d - head:+5d}'
          + (f'  | mit terser Suffix {td:+5d}, Spielraum {spare - td - head:+5d}' if td is not None else ''))
asset = (here / 'candidates/A_asset_app.js').read_text(encoding='utf8')
TAIL = '</script><script src=/assets/app.01234567.js></script></body></html>'
print(f'Option A: Asset {b(asset)} B (reines JS), Shell {HEAD + b(TAIL)} B (Kopf {HEAD} B + Anhang {b(TAIL)} B)')
