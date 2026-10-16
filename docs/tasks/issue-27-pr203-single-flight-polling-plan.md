# Issue #27 / PR #203 – Single-Flight-Polling der Web-Shell

Stand: 2026-10-16, `PLAN_PENDING_OWNER_APPROVAL`. Vor Produkt-/Testcodeaenderungen
ist die ausdrueckliche Ownerfreigabe dieses exakten Plan-Commits erforderlich.
Dieser begrenzte Zusatzplan ersetzt keine RES-01- oder Hardwareabnahme.

## 1. Auftrag, Ausgangslage und Grenzen

Ownerauftrag: „PR #203: Web-Polling ohne Ueberlappung“ vom 2026-10-16.
Umsetzung nach Planfreigabe im bestehenden Issue #27 / PR #203 und Branch
`agent/issue-27-r1-web-dashboard-plan`; kein neuer Produkt-/Diagnosebranch.

```text
CONTEXT_BASELINE_BRANCH: agent/issue-27-r1-web-dashboard-plan
CONTEXT_BASELINE_SHA: c10b90be5c09e9d3ed71dd49fb1ea83eb94694f4
CONTEXT_HEAD_SHA: c10b90be5c09e9d3ed71dd49fb1ea83eb94694f4 (Planungsbeginn)
CONTEXT_PLAN_SHA: 8ce57d78f1544728bf1868d965551d3de7c302f0 (RES-01, Ownerfreigabe)
CONTEXT_REFRESH_MODE: INCREMENTAL
CONTEXT_DELTA: Offline-Befund c10b90b gegen Browser-Evidence 4091be8;
              betroffene Shell, Konsumententests und Live-GitHub-Evidence gelesen
SOURCE_OF_TRUTH_CONFLICT: NONE
```

Live-Pruefung: PR #203 offen/Draft, HEAD gleich lokaler Baseline, Issue #27 offen,
Arbeitsbaum sauber; kanonischer Handover Kommentar `6128392407` gelesen.
Massgeblich sind Root-/App-AGENTS, [Workflow](../AGENT_WORKFLOW.md),
[Engineering](../ENGINEERING_PRINCIPLES.md), [CI-Vertrag](../CI_AND_QUALITY_GATES.md),
[Roadmap](../ROADMAP.md), [RES-01-Plan](issue-27-res01-heap-hardening-plan.md)
und der bestehende Frische-/Ordnungsvertrag SIM-27-W-05/06 im
[Dashboardplan](issue-27-r1-web-dashboard-plan.md).
Nach Freigabe ersetzt dieser Zusatzplan dort ausschliesslich die Annahme
parallel laufender Polling-Generationen durch Serialisierung/Abbruch sowie die
bisher nur optionale Scratch-Ausfuehrung durch den lokalen versionierten JS-Test.
Frische-, Aktions- und Auslieferungsvertraege gelten weiter.

Der [Offline-Befund](../audits/PR203_B4_OFFLINE_EINGRENZUNG_20261016.md)
beweist fehlende In-flight-Begrenzung und fehlenden Geschwisterabbruch.
Erfolgreiche API-Antworten bis 23,99 s sind im
[Browserbericht](../audits/PR203_BROWSER_MANUAL_20261016/README.md) dokumentiert.
Eine Verursachung des spaeteren B4-Dokumentfehlers ist **nicht belegt**.

Ziel: hoechstens eine logische Gruppe aus `status`, `temperatures`, `alerts`
einschliesslich vollstaendiger JSON-Verarbeitung. Keine neue Bibliothek,
Schedulerklasse oder Netzwerkarchitektur. Programmkatalog `lp()` und Mutationen
sind keine Polling-Gruppen; ihre vorhandenen Requests werden nicht als Teil
einer globalen Drei-Request- oder Socketgarantie ausgegeben.

Unveraendert: API-Pfade, sofortiger Erstabruf, 5000-ms-Takt, Auth/Session/CSRF/
Replay/Bestaetigung, F1-Auslieferung mit statischem Suffix, HTTP-Adapter,
F4-Schwelle/-Gate, TCP/WLAN/HTTPD/sdkconfig. Keine Geraeteanfrage,
Browserwiederholung, UART-Portoeffnung, Flash, Reset, G2 oder Pre-Ready.

## 2. Begrenztes Implementierungsdelta nach Freigabe

Produktdatei: `lib/fermentation_app/src/web_application_routes.cpp`, bestehendes
eingebettetes Browser-JavaScript und unmittelbar erforderliche lokale Bezeichner.

1. **Eine aktive Gruppe:** Die Shell haelt lokal das aktive Gruppen-Promise
   und den gemeinsamen `AbortController`. Beim normalen Timeraufruf wird zuerst
   die bestehende Alters-/Stale-Anzeige mit `ui()`/`draw()` aktualisiert. Ist eine
   Gruppe aktiv, endet dieser Tick ohne Fetch und ohne aufgestauten Folgeauftrag.
   Sonst starten genau drei GETs mit demselben Signal. Die Sperre bleibt bis zum
   Settlement aller drei Fetch-/Body-Ketten bestehen, nicht nur bis zu Headern.
2. **Gemeinsame 30 000-ms-Browserdeadline:** Ein Timer pro Gruppe deckt Fetch
   und JSON-Body ab. Timeout, Transportfehler, HTTP-Nicht-OK oder JSON-Fehler
   brechen die Geschwister ueber denselben Controller ab. Fehler sofort in der
   bestehenden Anzeige behandeln; alle gestarteten Ketten danach geordnet
   einsammeln, beispielsweise mit `Promise.allSettled`, bevor die Sperre faellt.
   `finally` raeumt Timer/Controller/Gruppenzustand in jedem Ausgang auf;
   nach Abbruch kein automatischer Sofort-Retry. Der naechste regulaere Tick darf
   wieder starten. Bei ausbleibender Abort-Bestaetigung keine zweite Gruppe
   freigeben und damit unkontrolliert weiterlaufende Requests ueberlappen lassen.
3. **Snapshot nach Mutation:** Der bisherige explizite Refresh nach dem
   abgeschlossenen, akzeptierten Mutationsantwortpfad bleibt ein abgewarteter
   frischer Abruf. Sobald eine verwertbare Mutationsantwort vorliegt, `P` vor
   Freigabe von `B` invalidieren und `ui()`/`draw()` aktualisieren; `LP` fuer
   die gedimmte Anzeige behalten. Alle Run-Aktionsbuttons bleiben gesperrt,
   bis ein nach dieser Mutation gestarteter Snapshot vollstaendig und frisch
   abgeschlossen ist; erst dann zustandsabhaengig freigeben. Ein fehlgeschlagener,
   zu alter oder abgebrochener Refresh darf keine alte Aktionsgrundlage
   reaktivieren. Das gilt fuer alle verwertbaren Outcomes, auch `stale`, `busy`
   und `confirmation-required`. Keine zweite Aktions-/Queue-Abstraktion.
   Der frische Abruf wird mit einem kleinen lokalen Parameter am bestehenden `rf()`
   von Timer-Ticks unterschieden. Er invalidiert gegebenenfalls die vor der
   Mutation gestartete Gruppe, bricht sie kontrolliert ab und wartet deren
   vollstaendige Bereinigung ab. Anschliessend startet er eine neue Gruppe.
   Normale Ticks duerfen weder diese Bereinigung noch den reservierten frischen
   Start ueberholen. Auch bei Randfaellen am Gruppenabschluss bleibt maximal
   eine Gruppe aktiv; Timer-Ticks erzeugen keine Warteschlange. Es wird niemals
   einfach das alte Promise als nach der Mutation frischen Snapshot geliefert.
   Wiederholte explizite Refreshanforderungen duerfen ebenfalls keine unbegrenzte
   Warteschlange bilden: hoechstens einen vorgemerkten frischen Start halten;
   zusammengefasste Aufrufer nur durch einen nach ihrer Mutation gestarteten
   Snapshot erfuellen. Dies bleibt lokaler Zustand in `rf()`, kein Scheduler.
   Die POST-/Sequenz-/Replay-/Bestaetigungslogik selbst bleibt unveraendert;
   insbesondere keine neue Wiederholung des POST.
4. **Zeit und Anzeige:** Frische bleibt 15 s seit Gruppenstart, nicht seit
   Antwortankunft. `LP` bleibt letzte Anzeige, `P` die frische Aktionsgrundlage;
   Stale sperrt Aktionen weiter. Uebersprungene Ticks aktualisieren das Alter.
   Spaete/invalidierte Erfolge und Fehler duerfen einen neueren Zustand nicht
   ueberschreiben; den bestehenden Generationenschutz entsprechend erhalten.
   Ein 24-s-Erfolg ist kein Timeout, bleibt wegen seines Alters aber stale.

**Entscheidung zur Freigabe:** 30 s sind ein expliziter Client-Deadlinevorschlag
oberhalb der beobachteten 23,99 s, um haengende Browsergruppen zu begrenzen.
Dies garantiert keine erfolgreiche Antwort unter beliebigen Netzbedingungen
und definiert weder eine neue R1-Latenzschranke noch eine nachtraegliche
Abnahme historischer Langlaeufer. Bei blockiertem/suspendiertem Browser koennen
Web-Timer spaeter zugestellt werden; keine harte Echtzeitzusage. Ein Browserabort
beweist ausserdem nicht sofortige Freigabe aller serverseitigen Sockets/Puffer.

**Shellbudget:** Die bestehenden 8192 B und mindestens 128 B Reserve bleiben
bindend, einschliesslich der 19-B-Reserve fuer die breiteste Sequenz. Historisch
wurden 8048 B ausgeliefert: fuer zusaetzlichen Code ist kaum Reserve vorhanden.
Zulaessig ist eng begrenzte, semantikerhaltende Kurzschreibung vorhandener
JS-Bezeichner/Ausdruecke derselben Shell, soweit fuer dieses Delta notwendig.
Keine Texte, Sprachen, Bedienfunktionen oder Guards entfernen und keine externe
Script-Route einfuehren. Volle Shelllaenge in allen bestehenden Zustaenden
pruefen. Falls das Delta so nicht unter Beibehaltung der Reserve lieferbar ist:
`HOLD`, konkreten Fehlbetrag vorlegen; keine automatische Budgeterhoehung oder
breite Shell-Neuimplementierung.

## 3. Gezielte Verifikation und Nachweisgrenzen

Ein kleiner versionierter lokaler Test `scripts/test_web_shell_polling.mjs`
nutzt vorhandenes Node (Planungsumgebung v20.19.2), `node:vm`, `node:assert`,
kontrollierte Uhr/Timer und Fetch-/DOM-Fakes, ohne Paketinstallation. Er fuehrt
das aus der Produkt-Shell extrahierte JavaScript aus, keine nachgebaute
Pollingfunktion. Keine Node-/Browser-/CI-Toolchainumstellung. Testuhr vorwaerts
setzen statt reale 5-/30-s-Wartezyklen. Folgende unabhaengige Assertions:

| Fall | Erforderlicher Nachweis |
|---|---|
| Start und Normalfall | Sofort drei GETs; bei 5-s-Ticks neue Gruppe nur nach Abschluss; Online bei frischem vollstaendigem Erfolg |
| Langsame Gruppe | Header geliefert, JSON noch offen ueber mehrere Ticks: weiterhin nur drei Requests, maximal eine aktive Gruppe |
| Stale waehrend Warten | Nach letztem Erfolg Altersgrenze ueberschreiten: auch auf ausgelassenem Tick stale/Alter aktualisiert, Aktionen gesperrt; erster haengender Abruf bleibt offline |
| Langer Erfolg | Alle Bodies erst bei 24 s: kein kuenstlicher 15-s-Abbruch, Anzeige wegen Startalter stale; naechste frische Gruppe stellt Online wieder her |
| Deadline | Vor 30 s kein Timeout; bei zugestellter 30-s-Deadline gemeinsamer Abort auch fuer offene Bodies, keine Restrequests/Timer, kein Sofort-Retry |
| Frueher Teilfehler | Transport-, HTTP- und JSON-Fehler jeweils: Geschwister abgebrochen, Fehleranzeige, vollstaendiger Cleanup; verzoegertes Abort-Settlement haelt Sperre |
| Erholung | Mehrere ausgelassene Ticks erzeugen nach Erfolg/Abbruch keinen kumulierten Stau; erst naechster normaler Tick startet drei GETs |
| Mutation | Echten `send()`-Antwortpfad mit Fake-POST ausfuehren: alte Gruppe invalidieren/abbrechen/abwickeln, neuer Snapshotstart strikt nach Mutationsabschluss; Refresh wird abgewartet, nie zwei Gruppen |
| Aktionssperre nach Mutation | Echten `send()`-Pfad mit zuvor gueltigem `P` und erfolgreicher POST-Antwort ausfuehren, fuer alle verwertbaren Outcomes einschliesslich `stale`, `busy`, `confirmation-required`: anschliessenden Refresh verzoegern/abbrechen/fehlschlagen/zu alt abschliessen lassen. Waehrend des gesamten Fensters alle Run-Aktionsbuttons deaktiviert, `LP` gedimmt erhalten; alte Antworten reaktivieren `P` nicht. Erst nach vollstaendigem frischem, nach der Mutation gestartetem Snapshot Buttons zustandsabhaengig freigeben |
| Abschlussrennen | Mutation waehrend Header-/Bodyphase sowie am Gruppenende, Tick waehrend Cleanup; spaete alte Erfolge/Fehler aendern Anzeige/Aktionsgrundlage nicht |

Direkt betroffene native Konsumenten seriell ausfuehren:
`pio test -e native -f test_web_application_routes -f test_web_browser_policy
-f test_web_session -f test_authentication_records -f test_http_server_lifecycle`.
In Shell-Strukturtests SIM-27-W-05 nur durch die neue Serialisierung veraenderte
Textpins anpassen; wirksame Frische-/Generations-/Aktionssperren weiter pruefen.
F1-Golden-Erwartung fuer den beabsichtigten JS-Inhaltsunterschied gezielt
aktualisieren und Herkunft kennzeichnen, nicht blind aus Produktcode erzeugen.
Budget-/20-stellige-Sequenztests und vollstaendige 503-/Suffixfreiheit beibehalten.

Bestehende reale HTTP-Adapter-Hostregression gemaess
`test/esp_idf_http_server_adapter_host/README.md` (nur Host-Loopback, kein Board).
Beide Firmwareprofile mit `python3 scripts/build_esp_idf_profiles.py all
--require-clean-source-tree` seriell auf sauberem Implementierungscommit bauen;
Quell-/Image-/ELF-Provenienz dokumentieren, **nicht** flashen. Builder-Self-Check
gemaess CI-Vertrag auf dem Implementierungs-HEAD, kein voller Pre-Ready-Lauf.

Node-Fakes beweisen JS-Orchestrierung, keine echte Browser-/Netzwerk- oder
Hardwarequalifikation. Abbruch-/Socketwirkung am ESP32 und B4-Wiederverbindung
bleiben ohne gesonderten spaeteren Hardwareauftrag `NOT_PROVEN`.

## 4. Code-Scanning-Triage – ausschliesslich vorhandene GitHub-Evidence

Am 2026-10-16 `code-scanning/alerts/{1,2}` und jeweilige `/instances` via GitHub
API lesbar, beide **open**, Tool CodeQL 2.26.2. Jeweils eine gemeldete Instanz:
`refs/heads/main`, Commit `17ab3f5399a066465298ac6871b965d176a38d32`;
`created_at`/`updated_at` beide `2026-08-12T06:13:20Z`.

| Owner-Link | Regel / GitHub-Severity | Gemeldete Stelle | Triage zu #203 |
|---|---|---|---|
| [Alert 1](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/security/code-scanning/1) | `py/clear-text-logging-sensitive-data`, `error`, security `high` | `scripts/check_secrets.py:201`, Spalten 17–77 | Bestehender Pruefskriptbefund auf main, keine Web-Shell-/Firmwarestelle im #203-Diff |
| [Alert 2](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/security/code-scanning/2) | `py/redos`, `error`, security `high` | `scripts/check_architecture_boundaries.py:117`, Spalten 12–15 | Bestehender Pruefskriptbefund auf main, keine Web-Shell-/Firmwarestelle im #203-Diff |

GitHub meldet bei 1 moegliche Klartextprotokollierung eines Secrets; bei 2
moegliches exponentielles Regex-Backtracking fuer bestimmte Eingaben.
Die beiden Dateien sind im Diff `origin/main...c10b90b` unveraendert.
Damit **keine dieser gemeldeten Stellen dem geaenderten #203-Code zugeordnet**;
keine neue produktive Schwachstelle oder Ausnutzbarkeit hier behauptet.
Die APIs enthalten keinen eigenen #203-Instanznachweis; dessen Fehlen ist kein
Security-PASS fuer den gesamten PR. Empfehlung: gesondertes Issue fuer die
unabhaengige Bewertung der beiden offenen Altbefunde. Kein automatisches Issue,
Dismissal oder Zusatzfix in diesem Auftrag, keine Secrets/Codeflows publizieren.

## 5. Commit-Schnitte und Uebergabe

1. **Jetzt:** nur diesen Plan versionieren, Diff-/Link-/Dokumentationspruefung,
   auf bestehendem PR-Branch pushen, kanonischen Handover aktualisieren und fuer
   Ownerfreigabe der exakten Plan-SHA anhalten. Keine Implementierung/Builds.
2. **Erst nach Freigabe:** erneute HEAD-/Diff-Pruefung; kleinster Produktfix und
   direkt zugehoerige Tests/Golden-Anpassung in einem zusammenhaengenden Commit;
   gezielte Tests, saubere Builds und Builder-Self-Check gemaess Abschnitt 3.
3. Ergebnis unter `docs/audits/` mit Quell-/Test-/Buildstand und PASS/FAIL/
   NOT_PROVEN dokumentieren, keine historischen Evidencepakete umschreiben.
   Handover aktualisieren, fuer Independent Review anhalten. Keine behauptete
   B4-Root-Cause-Loesung und keine automatische Abnahme-/Hardwarefortsetzung.

Unveraendert: `B4_RECONNECT=FAIL_OBSERVED`, `RES-01=BLOCKER`,
`PR203_FINAL_GO=HOLD`, `G1_QUALIFICATION=NOT_PROVEN`, `ACTUATOR_RELEASE=NO`.
