# PR #205 / Issue #27 – Web-Mutationsheader, 2026-10-19

**Builder-Verifikation PASS; Independent Review PENDING.**
Der Owner hat die Implementierung auf exakt Plan
[`912d37e976114eedd5bb305aabbfc630659c3e56`](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/blob/912d37e976114eedd5bb305aabbfc630659c3e56/docs/tasks/issue-27-web-mutation-header-fix-plan.md)
freigegeben. Quell-/Testcommit:
`fd9fdccbcdf3211b6b16123d7ea2eb35e82d48a9`, auf `main=288a6f5`.
Dieser nachfolgende Dokumentationscommit aendert keine getesteten Quellen.

Die Shell sendet jetzt den vom unveraenderten ESP-IDF-Adapter gelesenen
`X-UI-Mutation-Seq`. Der gesamte Produktdiff ist genau diese Ersetzung (+3 B).
Sequenzwert, CSRF, Body, Session, Replay, POST-Retry, Bestaetigung und
Single-Flight/Aktionssperre bleiben unveraendert. Native Textassertion und
Frozen-Golden wurden ausschliesslich fuer den Header korrigiert, Herkunft
im Golden vermerkt. Keine Planabweichung.

Der bestehende JS-Test fuehrt den echten extrahierten `send()`-Pfad zweimal
aus und prueft tatsaechliche URL, POST-Methode, Sequenzheaderwerte `1`/`2`,
CSRF-Fixturewert, Content-Type und Abwesenheit des alten Headernamens.
Die neue Regression schlaegt gegen die Produktquelle des Planstands fehl;
der finale Test wurde dazu mit derselben JS-Datei in einer temporaeren
Offline-Kopie gegen die alte Produktquelle ausgefuehrt.

| Nachweis | Ergebnis |
|---|---|
| Ausfuehrender JS-Test, Node 20.19.2 | PASS, 41/41; neue Headerregression und alle 40 bestehenden Szenarien |
| Native Routes / Browserpolicy / Sessions / HTTP-Lifecycle | PASS, 129/129 (100 + 4 + 20 + 5) |
| Shellbudget, Golden, Zustands-/503-/Suffixfreiheit | PASS; Routes fuer die vom normalen Runner unterdrueckten Byte-Metriken einmal mit `-v` wiederholt, 100/100 |
| Reale IDF-6.1-HTTP-Adapter-Hostregression | PASS, 25 Fallmarker und finaler Dispatch-PASS; vorhandene kanonische Header-/Metadatenextraktion unveraendert |
| Kanonischer Builder-Self-Check auf `fd9fdcc` | PASS; Boardprofilguard, clang-format und clang-tidy 21.1.3 |
| Produktdiff-/Scopepruefung | PASS; Adapter, Build-/Netzwerkkonfiguration, Fach-/Authvertraege unveraendert |

Gemessener vollstaendiger Shellbody (Kopf plus Suffix):

| Zustand | Bytes bei Sequenz `1` | Reserve zu 8192 B | Mit 19 B fuer maximale Sequenz |
|---|---:|---:|---:|
| Angemeldet | 8059 | 133 | 8078 |
| Passwortschutz aus | 8058 | 134 | 8077 |

Die letzte Spalte ist der durch den bestehenden nativen Test abgesicherte
Breitenaufschlag, kein separater Lauf mit einem realen maximalen Sessionzaehler.
Die vorhandene 128-B-Reserve bei normaler Sequenz bleibt eingehalten;
Provisionierung, anonymer Schutz und Recovery bleiben ebenfalls budgetgeprueft.

Befehle stehen im freigegebenen Plan. Der HTTP-Hosttest lief wegen des bereits
belegten Hostports 8080 in einem eigenen User-/Netzwerk-Namespace mit nur
aktiviertem Loopback; fremde Prozesse und Hostnetz blieben unangetastet.
JS/native Tests liefen vor dem Quellcommit gegen bytegleiche Inhalte;
Budgetwiederholung, Adapterbuild/-lauf und Self-Check auf dem sauberen
Implementierungscommit. Dateihashes und bereinigte Ergebniszeilen:
[VERIFICATION.txt](PR205_WEB_MUTATION_HEADER_20261019/VERIFICATION.txt).
Lokale Rohlogs bleiben unveraendert unter `build/issue27_header_validation/`.
Der Auszug laesst Buildausgaben, lokale Toolpfade und Systemheaderdiagnosen
kenntlich aus; keine privaten Browser-/UART-/Flashdaten verwendet.

Keine Firmwareprofil-Builds, Hardware-/Browseraktionen oder Pre-Ready-Ausfuehrung.
Die lokalen JS-/Adapterbelege sind kein echter Browser-zu-ESP32-Mutationslauf,
kein Nachweis behobener WLAN-/TCP-Probleme und keine volle R1-Inbetriebnahme.
Historische Evidence und F4 PROVISIONAL bleiben unveraendert.
Naechstes Gate: unabhaengiger Review dieses kleinen Diffs; Draft bleibt Draft,
Issue #27 OPEN, `ACTUATOR_RELEASE=NO`.
