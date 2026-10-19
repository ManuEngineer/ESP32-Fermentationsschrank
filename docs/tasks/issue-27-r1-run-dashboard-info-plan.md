# Issue #27 – R1-Laufinformationen im Web-Dashboard

**2026-10-19 · PLAN_ONLY / PLAN_REVIEW_PENDING.** [Issue #27](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/issues/27)
bleibt OPEN; [Draft-PR #206](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/pull/206),
Branch `agent/issue-27-r1-run-dashboard-info-plan`. Kein Implementierungsauftrag.
Vor Umsetzung braucht es die ausdrueckliche Freigabe der exakten Plan-SHA
**einschliesslich D1 und D2** unten.

## 1. Basis, Ziel und Schnitt

Live-Basis `main=79eb9b953434330cbd652aca18778791872eadb6`: #203 gemergt
(`288a6f5`), #205 gemergt (`79eb9b9` aus `e5294ff`). #205-Review laut Owner OK,
OPEN_BLOCKERS=0, Pre-Ready host+esp PASS auf e5294ff; kein Nachweis dieses neuen
Slices. Sein letzter Handover ist eine historische Vor-Merge-Momentaufnahme.
Die fuenf frueheren Diagnosebranches sind remote entfernt, ihre Spitzen durch
`archive/pr203-*`-Tags gesichert. Tags, lokale Worktrees und Evidence bleiben
unangetastet. Erste Branch-Aenderung `a4f41ae`: #205-Post-Merge-Status in der
[Roadmap](../ROADMAP.md) nachgefuehrt.

Ziel: **eine** lesende Erweiterung der vorhandenen Startseite fuer laufgebundene
Informationen: unveraenderlicher Programmname, verstaendliche Phase, wirksame
Sollvorgabe, Abschlussverhalten und ehrlich bezeichnete Zeitangaben. Produkt-
und Schrank-Isttemperatur werden aus ihrem vorhandenen Qualitaetsvertrag
weiterverwendet. Keine weitere Seite und keine neue Bedienaktion.

Fachquellen: [WEB_UI](../WEB_UI.md), insbesondere laufender Prozess,
[urspruenglicher Slice / D4](issue-27-r1-web-dashboard-plan.md),
[LOCAL_RUNTIME_UI](../LOCAL_RUNTIME_UI.md), [PROGRAMS](../PROGRAMS.md),
[RUN_COMMANDS](../RUN_COMMANDS.md), [RUN_PERSISTENCE](../RUN_PERSISTENCE.md).
Governance: Root-/Modul-AGENTS, [Workflow](../AGENT_WORKFLOW.md),
[Engineering](../ENGINEERING_PRINCIPLES.md), [CI/Gates](../CI_AND_QUALITY_GATES.md),
[Spezifikationsprioritaet](../SPECIFICATION_REVIEW.md),
[ADR-013](../ADR-013_REUSABLE_DEVICE_PLATFORM.md). Keine neue Domainentscheidung
in Browser, Codec oder HTTP-Adapter; die Application bleibt fachlicher Owner.

## 2. Nachgewiesene Datenkette auf 79eb9b9

Pfade nachfolgend relativ zu `lib/fermentation_app/src/`, ausser explizit anders
angegeben. `web_json_codec.cpp` / `encodeWebApiStatus` uebertraegt heute nur
version/ready/homeMode/processState/networkMode/selectionRequired/revisions.

| Anzeige | Vorhandene kanonische Quelle | Snapshot / Transport / Shell heute | Kleinstes erforderliches Delta |
|---|---|---|---|
| Programmname | `RunCommandState.activeProgramRun->snapshot().source`, darin `ProgramDocument.program.name`; eingefrorener Lauf, nicht heutiger Katalog | Home hat nur activeRunId/effectiveValues; `/programs` liefert nur aktuell installierte/aktive id/name-Paare, `/status` keinen Namen | Begrenzte Namenskopie im gemeinsamen Home-View, dann `/status`; kein Browser-Join mit Auswahlkatalog |
| Phase | `processState.state`, `processStateTextKey()` und DE/EN/ES-Texte in `fermentation_ui_text.cpp` | Home.processState und `/status.processState` vorhanden; Shell zeigt nur groben homeMode | Bestehenden Token verstaendlich anzeigen; keine neue Phasenlogik/Statusquelle |
| Produkt/Schrank | `refreshUiSnapshot()` projiziert SensorQuality samt Wert; `encodeWebApiTemperatures` liefert nur bei Valid/finite einen Zahlenwert | Bereits vollstaendig transportiert und mit Fehlend/Stale/Fehler angezeigt | Unveraendert wiederverwenden; kein Sensorersatz und kein 0-Default |
| Sollvorgabe | `resolveEffectiveControlContext(state)` in `control_context.*` besitzt die verbindliche Matrix fuer Fermentations-, Kuehl- und manuelles Ziel | Home.effectiveValues enthaelt nur Fermentationsziel; `/status` transportiert auch dieses nicht | Aktuell aufgeloestes gueltiges Ziel projizieren; insbesondere in Cooling/CoolHolding nicht das Fermentationsziel anzeigen |
| Abschluss | `ProgramCompletion` im unveraenderlichen RunProgramSnapshot; `ProcessRunSnapshot` bestaetigt Modus/Dauer | Weder Home-Detail noch API-Feld | Modus, ggf. Kuehlziel und Haltedauer als kleine lesende Projektion |
| Verstrichene/Restzeit | `ProcessRuntimeState.stateEnteredAtMillis`, ProcessRunSnapshot-Dauern, exakt getaggtes priorBootPhaseElapsed; `deriveFermentingSecondsDelta` und FSM `elapsedWithPrior` belegen Zeitsemantik | Kein Anzeige-/API-Zeitfeld; UI-ProjectionInput hat noch keinen aktuellen Monotonzeitwert | Eine lesende Zeitprojektion mit expliziter Verfuegbarkeit, siehe D2; kein Browserzaehler |
| Gesamtlaufzeit | `runProgress.observedRunSeconds` zaehlt ausschliesslich beobachtete Fermenting-Segmente, separat von Ausfall/Prior; `KnownTotal/PartialUnknownHistory` beschreibt diese Fortschrittsbasis | Keine fertige fachliche Gesamtdauer einschliesslich Vorheizen/Warten/Kuehlen/Ausfall | **Keine vorhandene Gesamt-Uhr behaupten.** Ein kompletter Run-Wallclock-Vertrag waere ein eigener Domain-/Persistenz-Schnitt, hier ausgeschlossen |

Belege: `fermentation_ui_models.hpp` / `FermentationHomeView`,
`fermentation_ui_projector.cpp::projectInto`,
`fermentation_application.cpp::refreshUiSnapshot`/`uiPresentationSource`,
`run_snapshot.hpp`, `run_commands.hpp/.cpp`, `process_state_machine.hpp/.cpp`,
`run_recovery_types.hpp`, `web_application_routes.cpp::WebReadOnlyApiHandler`,
`web_json_codec.cpp::encodeWebApiPrograms`. Auch das Display zeigt heute laut
`main/fermentation_ui_renderer.cpp` die ownerseitige restDuration-Vorgabe;
diese ist **kein** bereits laufend dekrementierter Countdown.

## 3. Vorgeschlagener Anzeige-/Datenvertrag

**Eine rendererunabhaengige Projektion.** `FermentationHomeView` erhaelt eine
kleine optionale Laufdetail-Struktur. Der bestehende Projector liest nur den
aktuellen RunCommandState; die Application liefert ihm unter demselben
ApplicationCallSerializer-Guard einmalig einen optionalen aktuellen Wert aus
`ITimeSource::monotonicMillis()`. Ohne Zeitquelle `nullopt`, nie ein gueltiges
kuenstliches `0`. Keine Kopie des gesamten ActiveRun/ProgramCatalog, kein neuer
Service/Port, kein direkter Runtime-Zugriff aus dem Webhandler.

Nur begrenzte besessene Strings verwenden (Programmname <=96 UTF-8-Bytes,
bestehende Run-ID <=48 B); Kapazitaet wie bei activeRunId in projectInto behalten.
Keine string_views in mutable Run-/Katalogspeicher ueber die Sperrgrenze halten.
Neue semantische Felder in `equalFermentationUiSemanticSnapshot` beruecksichtigen;
reine Erfassungszeit nicht als fachliche Revision behandeln. Zeitfortschritt darf
keine runRevision/stateSequence/Commandrevision erhoehen; bestehende UI-Refresh-
Revision darf auf geaenderte Anzeigewerte reagieren, keine Extra-Render-Schleife.

**Nur `/api/v1/status` additiv erweitern, version 1 beibehalten:**

| Neues Feld unter `run` | Typ und Herkunft |
|---|---|
| `id`, `sourceKind`, `programName` | Bestehende Run-ID; factory-catalog/user-program/manual-timed aus ProgramSourceKind; bei reinem ManualHolding sourceKind/name null. Name des eingefrorenen Programms; bei namenlosem ManualTimed null (Shell bezeichnet die bekannte Laufart), kein Katalog-/Standby-Fallback |
| `targetCelsius` | endliche Zahl aus gueltigem EffectiveControlContext, sonst null |
| `completion` | null oder Objekt `{mode,coolingTargetCelsius,holdDurationMinutes}` aus derselben unveraenderlichen Runquelle; unzutreffende optionale Werte null |
| `time` | `{scope:'phase',elapsedSeconds,remainingSeconds,remainingKind}` gemaess D2; nullable Zahlen, remainingKind = timed/unknown/unbounded/not-applicable |

`completion.mode` verwendet finish/cool-finish/cool-hold-duration/cool-hold-manual,
entsprechend dem bestehenden `completionModeTextKey`; diese vier Enumwerte
werden erschoepfend abgebildet, unbekannte Werte nicht als Finish ausgeben.
Bei keinem vorhandenen kanonischen Lauf ist `run:null`; Felder werden bei
Quellenverlust oder neuem Standby geloescht. Bei Completed Details nur behalten,
solange der Owner den Lauf noch haelt; nach Quittierung/Abort ohne Quelle null,
keine neue Historie. Bei Recovery kann der gesicherte Name/Abschluss als
Konfiguration sichtbar sein; keine laufende Sollregelung daraus behaupten.
Ein Sollwert ist eine Vorgabe, **keine** Sensor-/Aktorfreigabe; fehlende Istwerte
werden weiterhin gesondert als fehlend dargestellt.

Zeitzahlen sind nichtnegative JSON-Safe-Integer <=9007199254740991; checked
uint64-Arithmetik vor JSON-Konvertierung, sonst null/unknown. Kein Wrap/Narrowing
oder NaN/Infinity. Die maximale Hold-Dauer (uint32-Minuten) passt nach checked
Multiplikation mit 60 in diesen Transportbereich. Namen nur textContent,
UTF-8-/Laengenvertrag aus konfigurativ validierter Laufquelle; keine HTML-Ausgabe.
cJSON bleibt privat im vorhandenen Codec; keine neue Bibliothek oder JSON-Typen
im Viewmodell. Fehlerhafte Enum-/Bereichsdaten oder zu grosser Body ergeben wie heute komplett
503 JSON ohne statischen Suffix, nie abgeschnittene/teilweise 200-Antworten.

`/programs` bleibt Auswahlkatalog, unveraendert an programCatalog-Revision
gebunden; sein spaeterer Name/Loeschung/Deaktivierung darf einen laufenden Namen
nicht umdeuten. Laufdetails, Phase und Revisionen kommen aus **einem** Status-
Snapshot unter derselben Sperre. Die drei getrennten API-Requests bleiben keine
behauptete globale atomare Sensormessung.

**Browser:** vorhandenes dr() erweitern, Daten aus LP nur anzeigen, P bleibt
alleinige bestehende Aktionsgrundlage. Single-Flight, 5-s-Takt, 30-s-Deadline,
15-s-Frische, nach Mutation frischer Snapshot, CSRF/Replay/Bestaetigung sowie
X-UI-Mutation-Seq unveraendert. Kein vierter Pollrequest, Countdown-Timer,
Sofort-Retry oder Aktion aus reinen Anzeigedaten. Fehlende neue Felder sperren
nicht durch neue Domainregeln und erzeugen insbesondere keine Freigabe.
LP bleibt bei Fehler/Stale nur gedimmte Vergangenheit mit Abrufalter.
DE/EN/ES-Tokens mit den bestehenden Fachtexten abgleichen; Benutzerprogrammnamen
unuebersetzt lassen; Englisch/technischer Schluessel als sichtbarer Fallback.

## 4. D2 – notwendige Zeitentscheidung

**Empfehlung D2-A: ausdruecklich phasenbezogene Zeit fuer diesen kleinen Slice.**
Beschriftung beispielsweise „Phase verstrichen“ / „Rest dieser Phase“; kein
unbeschriftetes „Laufzeit/Rest bis Programmende“. Die vollstaendige Gesamtlaufzeit
bleibt als offener R1-Punkt erkennbar, sofern der Owner sie nicht neu begrenzt.

- Bei aktiver kanonischer Phase: elapsed = (now - stateEnteredAtMillis)/1000
  plus nur ein zu **dieser Phase** passender **exakter** Priorwert. Fehlende
  aktuelle Zeit, Ruecklauf/fremde Bootbasis, falsches Tag, unklare Bounds,
  Overflow, RecoveryEvaluation, Fault/SafeBoot oder Completed: Zeit unbekannt,
  nicht Sekunden seit Fehler/Abschluss als Laufdauer zeigen. Ohne vorherige
  Recovery ist Prior 0; ein vorhandener Prior gilt nur bei lower==upper.
- Rest nur fuer Fermenting und zeitbegrenztes CoolHolding: checked Dauer aus
  ProcessRunSnapshot *60 minus exakte Phasenzeit, nach unten bei 0 begrenzen.
  Dieselbe neutrale Phasenbasis wie die FSM; keine nominalRecoveryAdjustment-
  oder gewichtete/biologische Gutschrift. Verstrichene Zeit nach AdjustRun ist
  die neu baselinierte Phase, keine behauptete Gesamtzeit.
- ReachingTarget/QualifyingTarget/Preheating/Waiting/Cooling: keine erfundene
  ETA aus Maximal-Warte-/Qualifikationsgrenzen oder Sensorwerten. Rest unknown.
  CoolHolding bis manueller Beendigung und ManualHolding: remainingKind
  unbounded („bis manuell beendet“), Zahl null. Standby/Completed: not-applicable.
- Endzustand erst aus processState, niemals aus Rest==0. Ohne Livefortschritt
  oder bei altem Snapshot kein Browserweiterzaehlen. Minutenanzeige: elapsed
  abrunden, positive Restzeit aufrunden; Stunden nicht bei 24 umbrechen.

**D2-B (hier nicht zur Umsetzung vorgeschlagen):** Soll der Slice stattdessen
volle Run-Gesamtzeit/Programmende-ETA liefern, fehlt der gepruefte gemeinsame
Zeitvertrag. Dann anhalten und genau diesen Domain-/Recovery-Vertrag gesondert
entscheiden; keine Persistenzfelder/Browseruhr im vorliegenden Slice vorbauen.
D2-A ist eine begrenzte Slice-Abnahme, keine heimliche Erfuellung aller R1-Zeiten.

## 5. Ressourcenbefund und D1 – Budgetentscheidung

#205-Nachweis: authentifiziert **8059 B**, Passwortschutz aus 8058 B, mit
20-stelliger Sequenz 8078/8077 B; heutige harte Grenze **8192 B**. Die bestehende
128-B-Reserve im Normalfall enthaelt 19 B Sequenzaufschlag: **nur 5 B** netto
additiv frei. Read-only-Analyse der C++-Literale auf 79eb9b9: statischer Suffix
**6662 UTF-8-B**, kein neuer Lauf-/Buildbeweis. Allein die bestehenden Texte der
acht aktiven Phasen in DE/EN/ES enthalten **336 UTF-8-B** Werte, ohne Schluessel,
HTML/JS oder die weiteren Fachlabels. Ein unveraendertes additiv vergroessertes
Layout mit diesen Texten passt somit nicht. **Ein kompakt umgestalteter Kandidat
ist noch nicht implementiert/gemessen: BUDGET_FIT=NOT_PROVEN.**

| Ownerentscheidung vor Umsetzung | Begrenzung / Kosten / Abbruch |
|---|---|
| **D1-A, empfohlen: 8192 B behalten** | Zuerst C1: nur bestehende DE/EN/ES-Tabelle durch gemeinsame Schluessel statt dreifacher Schluessel und dr()-Zeilenrendering lokal kompakter schreiben; alle Texte/Funktionen/Guards erhalten, kein Minifier/Buildframework. Den vollstaendigen geplanten Inhalt und maximale Sequenz messen. >=128 B Normalreserve beibehalten. Nicht hinreichend: STOP mit Byte-Fehlbetrag, kein halber Backend-Fix und keine breite Optimierungskampagne. Ein Fit ist nicht zugesagt. |
| **D1-B, nur ausdruecklich alternativ freigebbar: 10240 B Gesamtbody** | Einmalige, sichtbare neue Obergrenze mit unveraenderter 128-B-Normalreserve und 19-B-Sequenztest; keine automatische zweite Erhoehung. Budgetobergrenze +2048 B; konkrete Mehruebertragung/Flashbytes gegen die gemessenen 8059 B erst messen; F1 belassen, besessener Kopf weiterhin <=1536 B auch mit maximaler Sequenz. Keine direkte Schlussfolgerung auf Peak-Heap/Sendelaufzeit. Konstanten, Golden/Budgetassertions und WEB_UI ausschliesslich fuer diesen freigegebenen Wert nachfuehren. Passt der komplette Inhalt auch dann nicht: HOLD. |

Eine getrennte statische JS-Datei koennte mit dem heutigen F1-Suffix ohne
vollstaendige Heap-Kopie ausgeliefert werden (der alte #203-Vor-F1-Befund gilt
hier nicht mehr). Sie braeuchte aber einen zusaetzlichen GET/Route-, Fehler-,
Caching-/Auth- und Gesamtasset-Budgetvertrag und umgeht die heutige Gesamtbody-
Schranke nicht ohne Entscheidung. Deshalb **keine** solche Umsetzung in diesem
Plan, keine vorsorgliche Netzwerk-/HTTPD-/Architekturaenderung.

**API/Peak:** 3072-B-Grenzen fuer status/programs/temperatures/alerts bleiben.
Programmname maximal 96 B kann bei JSON-Escaping bis 576 B, Run-ID 48 B bis
288 B benoetigen. Mit Enums, Zahlen und Schluesseln ist der neue Status **noch
zu messen**, nicht aus dieser Teilrechnung als passend erklaert. Keine ganze
Programm-/Kataloguebertragung. cJSON-Baum plus Strings, existierender 3078-B-
Printpuffer und besessener Antwortbody existieren zeitweise gleichzeitig;
3072 B Bodylimit begrenzen nicht allein den Peak-Heap oder Stack.
Neue Viewstrings und Tracker-Kopien erhoehen ggf. den UI-Speicherbedarf.

C2/C3 erfassen sizeof-/String-/Body-/Builddeltas und verfuegbare gezielte native
Allokations-/Fehlernachweise. Daraus folgt **kein** realer ESP32-Peaknachweis;
dieser bleibt NOT_PROVEN. Keine PSRAM-Annahme, F4-Schwellenaenderung oder
allgemeine OOM-Zusage. Statische Daten bleiben im Flashsuffix, Fehler loeschen
Suffix; keine zusammengesetzte Shellkopie oder grosser Stack-Temporary.

## 6. Umsetzungsschnitte nach exakter Freigabe

1. **C1 – Budget zuerst:** freigegebene D1-Variante mit kompletter Anzeige und
   DE/EN/ES in vorhandener Shell/Test-Fixture pruefen; Synthesedaten, keine
   Domainnachbildung/Netzwerkaktion. Ausfuehrenden JS-Test wiederverwenden.
   Alle bisherigen Guards/Header/Sprachen und native Budget-/Golden-Regression
   halten. Bei Budgetfehler HOLD vor Backend-Erweiterung; Bytebefund vorlegen.
2. **C2 – gemeinsame Projektion und Transport:** `fermentation_ui_models.*`,
   `fermentation_ui_projector.*`, `fermentation_application.cpp` um begrenzte
   Anzeigeprojektion/D2-A-Zeitinput ergaenzen; bestehende control_context-
   Resolver verwenden. `web_json_codec.*` um das additive run-Objekt, bestehende
   Routen unveraendert. Keine Aenderung an FSM-/Run-/Recovery-/Persistenzlogik,
   Plattformports, Adapter, Mutations- oder Sessionvertraegen.
3. **C3 – integrierte Anzeige und Nachweis:** C1-Shell an reale API-Projektion
   binden; `scripts/test_web_shell_polling.mjs`, vorhandene native UI-/Webtests
   und Frozen-Golden gezielt ergaenzen. WEB_UI fuer gelieferte Felder, Grenzen
   und ausdruecklich gewaehltes Budget aktualisieren, passende SIM-Akzeptanzfaelle
   in ACCEPTANCE_TESTS/Status in ROADMAP nachfuehren. Ergebnis auf exakten
   Quell-/Testcommit binden; Builder-Self-Check, dann Independent Review.

## 7. Gezielte Verifikation und verbleibender R1-Scope

| Nachweis mit bestehender Infrastruktur | Erforderliche Faelle |
|---|---|
| UI-Projektion / Application-Snapshot | Name aus eingefrorenem Lauf trotz Katalogaenderung; fehlende/manuelle Quelle; Sourceverlust/Standby/Ack/Abort; Ziel in Fermenting vs. Cooling/CoolHolding ueber echten Resolver; keine Sensor-/Aktorfreigabe durch Sollanzeige |
| Zeit-/Zustandsmatrix | Phasenwechsel, AdjustRun-Neubaseline, Dauergrenze/0, unbefristetes Halten; fehlende Zeit/Ruecklauf/falsches Prior-Tag/Basis/Bounds/Overflow; exakte Recovery vs. unklare Recovery; Completed kein weiterlaufender Timer. Vergleich mit vorhandenen FSM-Vertraegen statt zweiter Domainfunktion |
| Codec/produktiver GET /status | nullable Typen, alle vier Abschlussmodi, neue Felder mit bestehenden Revisionen aus einem Snapshot; maximale Namen inkl. UTF-8/Escaping, sichere Zahlengrenzen, 3072 B und kompletter 503-Fehler ohne Suffix. Auth/Same-Origin/GET-only unveraendert |
| Echter extrahierter JS-Shelltest | DE/EN/ES, fremder Name als Text, unbekannte Tokens/Fallback; vorhandener/neuer/fehlender run-Block; alte/verzoegerte Antworten, Mutation/Refresh, langsame Gruppe, Abort, Stale/Offline und Recovery. LP-Details allein geben niemals Aktionen frei; alle bestehenden 41 Szenarien erhalten |
| Shell/F1/Abhaengige Konsumenten | Alle Auth-/Provisionierungs-/Recoveryzustaende plus maximale Sequenz, D1-Grenze/Reserve, kompletter Kopf+Suffix; alle bisherigen Funktionen/Texte/CSRF-/Sequenzheader erhalten; native Renderer-/UI-Revision-Regression wegen gemeinsamem Viewmodell |

Geplante Befehle nach Umsetzung (nicht jetzt):

```bash
node scripts/test_web_shell_polling.mjs
pio test -e native -f test_fermentation_ui_models -f test_control_context -f test_process_state_machine -f test_web_application_routes -f test_renderer_boundary
python3 scripts/build_esp_idf_profiles.py all --require-clean-source-tree
export PRE_READY_EXPECTED_HEAD="$(git rev-parse HEAD)"
bash scripts/run_pre_ready_gates.sh self-check
```

Builds seriell auf sauberem Implementierungscommit, beide Profile wegen
geaendertem gemeinsamem UI-Modell. Bestehende HTTP-Adapter-Hostregression nach
ihrem README nur zusaetzlich, falls C1 den F1-Auslieferungs-/Fehlerpfad tatsaechlich
beruehrt; keine vorsorgliche Vollsuite und kein Pre-Ready ohne spaeteres Gate.
Ein natives Application-Fixture mit gueltigen Daten ist kein realer Programmstart
oder Hardwarebeweis; keine versuchsweise Sensor-/Commissioning-Simulation auf
Geraet. Reale Browser-/ESP32- und Peak-Heap-Abnahme bleiben NOT_RUN/NOT_PROVEN,
kein pauschaler neuer G1'/G2-Stresslauf als Voraussetzung dieser Planung.

Weiter getrennt gegen den genehmigten R1-Vertrag abarbeiten: Geraetename und
kompakter Netzwerk-/Zeitstatus (Standby), technische Sensor-/Aktorinformationen,
Details/Meldungsnavigation, manuelle Sprachwahl, sonstige Navigation/Service/
System sowie benoetigte Diagnose/Diagramme. Keine dieser Anforderungen wird
hier still aus R1 gestrichen oder als erledigt bezeichnet; #28-/spaetere
Funktionsanteile erst gegen Spezifikationsreview abgrenzen. Vollstaendige
Lauf-/Gesamtzeit nur nach D2-Entscheidung; Commissioning/D9/Regelung/Aktoren
separat. Issue #27 erst nach gesamtem genehmigtem R1-Umfang oder ausdruecklicher
Owner-Neuabgrenzung schliessen.

**Jetzt:** nur dieser eine Plan und Statusdokumentation; keine Produkt-/Test-/
Build-/Toolchainaenderung, kein Testlauf/Pre-Ready oder Geraetezugriff. Exakte
Plan-SHA zur unabhaengigen Pruefung vorlegen, danach STOP. D1 und D2 offen;
`ACTUATOR_RELEASE=NO`, G1' storniert/NOT_RUN, F4 PROVISIONAL und saemtliche
historischen FAIL/HOLDs unveraendert.
