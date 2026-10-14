# G1 IMAGE01 Ergebnis – PR #203 / RES-01

**Status: FAIL/HOLD.** Der eine freigegebene Kandidatenlauf wurde beim ersten erkannten UART-/Heap-Anomalieereignis gestoppt. Keine weitere Boardabfrage, kein Reset, Flash, Rollback oder Folgelauf wurde ausgeführt. `F4_THRESHOLD=NOT_QUALIFIED`; die vorläufige Schwelle bleibt unverändert. `RES-01=BLOCKER`, `PR203_FINAL_GO=HOLD`, `G2=NOT_AUTHORIZED`, `ACTUATOR_RELEASE=NO`.

## Gebundener Stand

- Produktbasis: `d35ee85edf4f93f20e9edbe13f8851e406d1b82b`
- Diagnosequelle: `bb4f43a69740f234d26a951707991f792cc3ffde`
- Review-Gate/Testblatt: Doku-Commit `9b8cc8f66b0a8ac511091e34440cccd82f16f290`
- App SHA-256: `da9e868874649bd274d21a4f1c5715afd9293288497027004c460efc46df8cf0`
- ELF SHA-256: `24d8557f965053d566ba6ef3c4bfe745044a505994835e5831b85ff0dbe8c369`
- Gate: `PR203_RES01_G1_20261014_IMAGE01`; Hardware: erwartete FT232R A5069RR4 / MAC `20:50:0d:1b:2f:34`; vor Ort actor-free bestätigt und UART `real actuators: disabled` beobachtet.

## Preflight und Flash

- Vollständiger privater Flash-Vorherdump: 4,194,304 B; aktueller C0-App-Prefix, MAC und Partitionstabelle frisch bestätigt. Boot-/NVS-/PHY-Region `0x000000–0x00FFFF` sowie `state_store` `0x300000–0x3FFFFF` nach dem Schreiben byteidentisch.
- Ein App-only-Schreibvorgang auf `0x10000`: 1,760,560 B; esptool Hashverification erfolgreich. Kandidaten-App-Prefix nach Readback stimmt exakt mit dem gepinnten Image überein. Keine anderen Flashbereiche geschrieben.
- Kandidatenboot: Quellcommit, MAC, Release-Profil, `application: ready`, `HomeConnected` und deaktivierte Aktoren bestätigt; Heartbeat-Lücke im frischen Boot maximal 3.45 s. Ein erwarteter Portöffnungs-Bootreset; danach kein weiterer Resetmarker.

## Last und Abbruch

- Vier frische Sessions erstellt; 60 s Vier-Session-Idle erfasst. Eine vergleichbare finale `diag_30s`-Stichprobe in den letzten 30 s des Idle: internal 8-bit free **41,132 B**, grösster Block **38,912 B**.
- P-A-Last lief **311.879 s** der geplanten 1,200 s. 777 Antworten mit HTTP 200 und sieben fehlgeschlagene Requests: sechs `http-protocol-error`, ein `timeout`. Es gab 784 Sampleeinträge insgesamt (Login 4, Status 246, Temperatures 246, Alerts 246, Shell 42). Keine Retries.
- Der Capture-Supervisor meldete `uart-anomaly`; der unvollständige letzte private UART-Rohframe enthält `heap_alloc_failed`. Zeitlich nahebei traten die HTTP-Transport-/Timeoutfehler auf. Die Evidence belegt den Heapmarker und die Requestfehler, aber nicht deren Reihenfolge auf Subsekundenebene. Abbruchzeitpunkt des Supervisors: 2026-10-14T14:12:14.453243+00:00.
- Der Lasttreiber beendete neue Requests und schloss aktive Sockets; Status `FAIL/HOLD`. Recovery-Phase 300 s **nicht ausgeführt**, keine Recoveryaussage.
- Teilmessung Latenz P-A (ms; nearest-rank p50/p95/p99/max):

| Request | n | p50 | p95 | p99 | max |
|---|---:|---:|---:|---:|---:|
| alerts | 246 | 289.6 | 1113.3 | 2982.5 | 3792.6 |
| login | 4 | 3030.1 | 3141.3 | 3141.3 | 3141.3 |
| shell | 42 | 590.3 | 1915.8 | 3425.0 | 3425.0 |
| status | 246 | 270.8 | 1134.8 | 2729.8 | 5149.0 |
| temperatures | 246 | 292.9 | 1478.0 | 2983.1 | 3264.9 |

Diese sind partielle Befunde aus dem abgebrochenen Profil, keine Abnahme; GET-Gesamtdauern >3 s sind sichtbar. Es wurde keine nachträgliche Latenzschwelle festgelegt.

## Heap-, Gate-, UI- und Stackbefunde

- In 23 `diag_30s`-Stichproben lag das beobachtete Minimum internal-8-bit-free bei **35,368 B**, der kleinste gemessene grösste Block bei **27,648 B**. Das Boot-Minimum-Watermark betrug **13,048 B** und ist nur als Berichtswert enthalten. Die Stichproben messen keine transienten exakten Peaks.
- Native/firmware UI-Auswertung: 41 valide Aggregate, beobachteter maximaler Checkpointbedarf **8688 B**; `2 × max(6,656 B, 8688 B) = 17376 B` ist nur die rechnerische Kandidatenschwelle, **nicht qualifiziert und nicht angewendet**. UI-Records decken nur Home, Locale-IDs 1/2, Phase 0/2 ab; keine Touchphase.
- Tatsächliche Gate-Blöcke wurden instrumentiert; über 733 Gate-Zeilen variierten sie von 0 bis 40,960 B, zuletzt 13,824 B. 396 kumulative Skips und bis zu 7 Resumes wurden bis zum Stop beobachtet. Das ersetzt keine Touch-/Hold-Coverage.
- Main-Stack Minimum **11,688 B** (Stopgrenze 11,160 B); HTTPD-Stack Minimum **2,304 B** (Stopgrenze 2,080 B), jeweils beobachtet bis zum Abbruch.
- Der Owner stellte nachträglich klar, dass keine Touchsequenz stattfand und Home normal erreichbar war. Die angefragte Uhr-öffnen/Zurück-Sequenz wurde nicht ausgeführt; im gesicherten UART-/Probe-Datensatz gibt es keine Touchphase. Physischer Kontakt-/Loslasszeitpunkt, Hold-A/B, Skip→Resume mit gehaltenem Finger und neuer Kontakt bleiben **NOT_PROVEN**. Kein Werksreset, kein Kalibrierungs- oder NVS-Schreibzugriff.

## Nachträglicher UI-Smoke-Test (separat von G1)

Der Owner beobachtete am 14.10.2026 gegen 18:06 Uhr Europe/Zurich (gegen 16:06 UTC): Home war normal sichtbar; Touch-Navigation in Menüs funktionierte. Der Owner beobachtete Touch und Gerätezustand während des früheren G1-Lasttests nicht. Das ist ein **PASS für den separaten, owner-beobachteten manuellen UI-Smoke-Test**, ohne monotone Zeit oder korrelierte UART-/Probe-Rohdaten. G1-01 (Uhr öffnen/Zurück) fand nicht statt und bleibt `NOT_PROVEN`; Hold-/Skip-Coverage bleibt ebenfalls `NOT_PROVEN`. Details stehen in [POST_RUN_UI_SMOKE.md](POST_RUN_UI_SMOKE.md).

## Ergebnisstatusmatrix

| Bereich | Status | Beleg / Grenze |
|---|---|---|
| Gepinnte App-/ELF-Identität, aktueller C0-Preflight, MAC/Partition | **PASS** | Frische Readback-/UART-Prüfung; Hashes in Manifest und privaten Readbacks. |
| Ein App-only-Flash auf `0x10000`; Image verifiziert; Nicht-App-Bereiche unverändert | **PASS** | Vorher-/Nachher-Readback der 4-MiB-Flashregion; 0 Unterschiede in `0..0xFFFF` und `0x300000..0x3FFFFF`. |
| Kandidatenboot, Source, HomeConnected, Release-Profil, Aktoren deaktiviert | **PASS** | Candidate UART; Heartbeat-Lücke bis Stop höchstens 3.45 s. |
| Resetfreiheit bis zum Stop | **PASS (beobachteter Zeitraum)** | Ein erwarteter Bootreset; danach kein weiterer Resetmarker im erfassten UART. Zustand nach Stop nicht erneut abgefragt. |
| Vier-Session-Idle-Referenz | **PASS** | 60 s; ein finaler 30-s-Stichprobenpunkt: 41,132 B frei / 38,912 B grösster Block. |
| P-A-HTTP-Ergebnis | **FAIL** | 777 HTTP 200, sieben fehlgeschlagene Requests (6 Protokollfehler, 1 Timeout); Last nur 311.879/1,200 s. |
| Heap-/Allokationsverhalten | **FAIL** | UART-Rohframe am Laufende enthält `heap_alloc_failed`; genauer Zeitpunkt im unvollständigen Frame nicht messbar. |
| Stackreserve | **PASS (beobachtete Stichproben)** | Main min 11,688 B; HTTPD min 2,304 B; jeweils Stopgrenzen eingehalten bis zum Abbruch. Keine Aussage nach Stop. |
| Heap-/Block-Stichproben | **PARTIELL / NOT_PROVEN als Abnahme** | 23 diag_30s-Punkte; beobachtete Minima 35,368 B frei / 27,648 B grösster Block; keine exakten transienten Peaks. |
| 300-s-Recovery | **NOT_PROVEN** | Nach erster Anomalie nicht ausgeführt. |
| Home-Render in G1 | **PASS (Instrumentierung)** | Phase-2 Home-Aggregate vorhanden; keine manuelle Pfad-/Variantenabdeckung. |
| G1-01 / Touchphase im Lastlauf | **NOT_PROVEN** | Sequenz fand nicht statt; keine Phase-1-Rohmessung. |
| Hold-A/B, physischer Loslass-/Neukontakt, Skip→Resume unter physischem Kontakt | **NOT_PROVEN** | Skip-/Resume-Zähler allein belegen keine physische Coverage. |
| Manueller UI-Smoke nach G1 | **PASS (Owner-beobachtet, separat)** | Home sichtbar und Touch-Navigation in Menüs funktionierte, ca. 18:06 Europe/Zurich; nicht mit G1-Rohdaten korreliert. |
| Finale F4-Schwelle / OOM-Sicherheit | **NOT_PROVEN** | Stichproben-Formel liefert keinen qualifizierten Wert; Schwelle bleibt vorläufig. |

## UTC-/monotone Zeitzuordnung

`TIMELINE.json` enthält die Ereignisse und Quellen. Candidate Capture erfasste einen Anker von UTC 13:59:52.049535 / monotonic 177597.011505 s. Daraus sind Host-monotone UART-Zeiten in UTC umgerechnet. Die `load.json`-Phasenmarken enthalten jeweils UTC und monotonic; einzelne HTTP-Samples enthalten UTC, ihre monotonic-Werte sind nur aus dem Capture-Anker rekonstruiert. Die sieben Fehler liegen zwischen 14:12:12.552 und 14:12:13.771 UTC; der letzte vollständige UART-Frame wurde gegen 14:12:12.516 UTC protokolliert und der Capture endete 14:12:14.453 UTC. Der Heapmarker liegt im unvollständigen UART-Rohframe zwischen dem letzten vollständigen Frame und Capture-Ende; die Reihenfolge gegenüber den HTTP-Fehlern ist **NOT_PROVEN**.

Die esptool-Protokolle geben für Vorher-Readback (102.2 s), App-Flash (26.8 s) und Nachher-Readback (102.7 s) Dauer und Reihenfolge an, jedoch keinen Host-UTC-/monotonic-Zeitstempel. Diese konkreten Phasenzeiten sind **NOT_PROVEN**; sie wurden nicht aus Dateimodifikationszeiten zurückdatiert. Die nachträgliche Owner-UI-Beobachtung besitzt nur die ungefähre lokale Uhrzeit, keinen monotonic-Zeitstempel.

## Empfehlung zur unabhängigen Prüfung

**C0 bleibt GO; G1 IMAGE01 ist FAIL/HOLD.** Der Kandidatenlauf löste unter P-A vor Ablauf der Last `heap_alloc_failed` aus und zeigte zugleich HTTP-Fehler. Damit ist die G1-Qualifikation fehlgeschlagen, die vorläufige F4-Schwelle bleibt `PROVISIONAL_PENDING_G1`, und es gibt keine ausreichende Evidenz für eine finale Schwelle oder OOM-Sicherheit. Touch-/Hold-/Pfadcoverage und Recovery fehlen. Keine automatische Korrektur, erneute Geräteaktion, Schwellenänderung oder G2-Fortsetzung. Unabhängiger Review dieser Evidence erforderlich; jede Hardwarefortsetzung braucht eine neue konkrete Ownerfreigabe.

## Lokale Evidence-Dateien

Die vollständigen Rohmitschnitte und 4-MiB-Flash-Readbacks bleiben lokal und unverändert. Veröffentlicht sind nur die für die Prüfung nötigen bereinigten Auszüge und abgeleiteten Tabellen unter `evidence/`; insbesondere enthält das Paket keine Flash-Readbacks, HTTP-Bodies, Cookies oder Zugangsdaten. `SHA256SUMS` bindet die veröffentlichten Dateien ausser sich selbst. Auslassungen und Ableitungen sind in `EVIDENCE_PROVENANCE.md` beschrieben.
