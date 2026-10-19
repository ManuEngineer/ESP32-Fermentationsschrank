# Issue #27 – R1-Laufinformationen im Web-Dashboard

**2026-10-19 · PLAN-REVISION 2 · FREIGEGEBEN (Owner, D1-C, H-RAM-1 = M) · UMGESETZT C1–C3.**
[Issue #27](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/issues/27)
bleibt OPEN; [Draft-PR #206](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/pull/206),
Branch `agent/issue-27-r1-run-dashboard-info-plan`.

**Freigabestand.** Diese Revision (Plan-SHA `3376a19c43abdd6507b51aef5566de4aa428ae58`)
ist vom Owner freigegeben, **D1-C = JA** (einmalig 10 752 B) und **H-RAM-1 = M**
(Mess-Overlay-Image). Umsetzungsstand: **C1, C2 und C3 implementiert**, Builder-
Self-Check PASS, Independent Implementation Review `GO` (`OPEN_BLOCKERS=0`).
**H-RAM-1 wurde am 2026-10-19 ausgefuehrt** (Mess-Overlay M, Standby/Ein-Client-HTTP,
gescriptetes Replay und echter headless Chromium; Rueckfall auf `a9a1156` verifiziert):
begrenzter positiver Teilnachweis, [Evidence](../audits/PR206_H_RAM_1_EVIDENCE_20261019.md).
`RAM_PASS=NOT_PROVEN` fuer das vollstaendige R1-/Laufprofil, S4 `NOT_RUN`, Produktimage
nicht hardwarequalifiziert. Weitere Ressourcenevidence:
[C1–C3 offline](../audits/PR206_C1_C3_RESOURCE_EVIDENCE_20261019.md). Der fruehere
C1-Befund bei 8192 B bleibt als [Evidence](../audits/PR206_C1_BUDGET_20261019.md)
erhalten. D2=A bleibt bestehen.

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

## 4. D2 – Zeitentscheidung (A bestaetigt)

**D2-A (Owner bestaetigt auf `f013ec0`): ausdruecklich phasenbezogene Zeit fuer diesen kleinen Slice.**
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

**D2-B (nicht gewaehlt, nicht Teil dieses Slices):** Soll der Slice stattdessen
volle Run-Gesamtzeit/Programmende-ETA liefern, fehlt der gepruefte gemeinsame
Zeitvertrag. Dann anhalten und genau diesen Domain-/Recovery-Vertrag gesondert
entscheiden; keine Persistenzfelder/Browseruhr im vorliegenden Slice vorbauen.
D2-A ist eine begrenzte Slice-Abnahme, keine heimliche Erfuellung aller R1-Zeiten.

## 5. Ressourcenbefund und D1 – Budgetentscheidung

**Basis (#205):** authentifiziert 8059 B, Passwortschutz aus 8058 B, mit
20-stelliger Sequenz 8078/8077 B; harte Grenze 8192 B, Normalreserve 128 B
(enthaelt 19 B Sequenzaufschlag). Statischer Suffix 6662 UTF-8-B, unveraenderter
Kopf 1397 B.

**C1-Ergebnis (MEASURED, D1-A ausgefuehrt):** vollstaendige Anzeige DE/EN/ES im
begrenzten Kandidaten = **10 410 B** (10 429 B mit maximaler Sequenz), Fehlbetrag
2218 B hart / 2346 B mit Reserve. `BUDGET_FIT` fuer 8192 B ist damit
**MEASURED_FAIL**. Kein weiterer Kompaktierungsversuch; D1-B (10 240 B) verfehlt
den Kandidaten mit Reserve um 298 B und wird nicht vorgeschlagen.

**Ressourcenpruefung ([Detailbilanz](../audits/PR206_RAM_FEASIBILITY_20261019.md)):**

| Posten | Ergebnis | Status |
|---|---|---|
| Suffix-Zuwachs +2351 B (nur isolierter C1-Suffixkandidat) | `.flash.rodata` +2352 B; `.dram0.data/.bss`, `.iram0.text`, `.flash.text` je 0 B; Image +2352 B (57 % der 3 080 192-B-Partition); gilt nicht fuer C2/C3 | **MEASURED** (Offline-Sektionsprobe, `esp32_release`) |
| Suffix-Sendepfad | Chunk ohne Heap-Kopie; `TCP_SND_BUF=5760` ist nur die Sendepuffer-Konfiguration, **keine belegte Obergrenze** des gesamten HTTP-/WLAN-Heaps; +29 % bezieht sich auf Shell-Bytes, **keine Sendedauer gemessen**; Vor-F1-Sendeanteil gemessen rund −6,9 KB | ESTIMATED (Code/Konfiguration), Messung in H-RAM-1 nicht ersetzt |
| C2/C3 dauerhaft, getrennt | Dauerzustand ca. 152 B (Tracker 136 + Input 16); Main-Stack 136 B (`UiRenderGate`); Heap bis ca. 210 B (zwei lange Namenskopien); Summe der Beispielzahlen 498 B, kein belastbarer Hoechstwert | ESTIMATED bis zur Messung des echten C2-Layouts |
| C2/C3 transient pro `/status` | Heap-Spitze +1,2 … +2,6 KB (cJSON-Baum und String, Baum vor dem Senden frei) | ESTIMATED |
| HTTPD-Stack | +0,24 … +0,34 KB gegen zuletzt 2336 B frei (frueherer Testfloor 2080 B) | ESTIMATED (Planstand); **gemessen in H-RAM-1 (Messbild M): 2 196 B frei**, 116 B ueber dem historischen Testfloor; im Produktimage P `NOT_PROVEN` |
| Main-Stack | +0,14 … 0,5 KB gegen mind. 11 752 B frei (Floor 11 160 B) | ESTIMATED (Planstand); **gemessen in H-RAM-1: 11 272 B frei**, 112 B ueber dem historischen Testfloor |
| Heap-/Stack-Spitzen, Erholung, F4-Skips | Planstand: keine Messung. **H-RAM-1 (Standby/Ein-Client, Messbild M): min. Heap 19 896 B, Erholung ca. 99 %, keine OOM-/Skip-/Reset-Ereignisse**; Produktimage P und aktiver Lauf (S4) | P/S4: **NOT_PROVEN** |

`sizeof`/Bytebilanz allein ist kein `RAM_PASS`; Abschlusswertung nach C3 ist
hoechstens `RAM_OFFLINE_ESTIMATED` bis zum Gate H-RAM-1.

| Ownerentscheidung | Begrenzung / Kosten / Abbruch |
|---|---|
| **D1-A (ausgefuehrt)** | 8192 B halten: C1=FAIL_BUDGET / HOLD. Bleibt der Befund, wenn der Owner weder D1-C noch einen anderen Weg entscheidet; dann kein C2/C3. |
| **D1-C (neu, empfohlen, nicht freigegeben):** einmalige neue Obergrenze **10 752 B** fuer Kopf plus Suffix | Spielraum: 342 B Gesamtreserve zum 10 410-B-Kandidaten, davon **214 B** ueber der 128-B-Reserve (konservativ 195 B ueber 10 429 B und Reserve) fuer die C3-Bindung. Kopf bleibt <= 1536 B auch mit maximaler Sequenz, F1 unveraendert. Genau eine Erhoehung: ueberschreitet C3 den Normalbody um mehr als 214 B (konservativ 195 B) gegenueber dem C1-Messstand, folgt **HOLD**, keine zweite Erhoehung, keine Kompaktierungsrunde. Konstante `kMaximumWebShellBytes`, Budget-/Golden-Assertions und WEB_UI nur fuer diesen Wert nachfuehren; 19-B-Sequenztest bleibt. |
| **D1-B (10 240 B)** | nicht vorgeschlagen (ungenuegend, 298 B Fehlbetrag). |

**Variantenwahl:** Variante A (F1-Flashsuffix, schlichte Anzeige, D1-C) ist
bevorzugt. Ein separates cachebares Asset (B) braeuchte zusaetzliche Route-,
Auth-, Cache- und Gesamtasset-Vertraege und bringt keinen nachgewiesenen RAM-
Vorteil; es wird **nur** bei einem Shell-spezifischen RAM-/Uebertragungsbefund aus
H-RAM-1 neu vorgelegt. Keine Gzip-/Socket-/WLAN-/Minifier-Massnahme vorsorglich;
keine Architektur- oder Buildumbauten.

**API/Peak:** 3072-B-Grenzen fuer status/programs/temperatures/alerts bleiben.
Programmname maximal 96 B kann bei JSON-Escaping bis 576 B, Run-ID 48 B bis 288 B
benoetigen; Bodygrenze 3072 B ergibt sonst 503 ohne Suffix. cJSON-Baum plus Strings
und Ausgabestring existieren gleichzeitig, der Baum wird vor dem Senden freigegeben;
das 3079-B-Druckarray liegt bereits heute im HTTPD-Stack. Programmname und Run-ID
liegen in je einer Kopie im Tracker, im `UiRenderGate` (Main-Stack, lokale Variable
in `app_main`) und im Request-Snapshot (HTTPD-Stack). Keine PSRAM-Annahme, keine
F4-Schwellenaenderung, keine allgemeine OOM-Zusage. Statische Daten bleiben im
Flashsuffix, Fehler loeschen den Suffix; keine zusammengesetzte Shellkopie oder
grosser Stack-Temporary.

## 6. Umsetzungsschnitte nach exakter Freigabe

Voraussetzung fuer jeden Schnitt: Ownerfreigabe der exakten SHA dieser Revision
**einschliesslich D1-C**. Ohne D1-C bleibt C1 `FAIL_BUDGET / HOLD`.

1. **C1 – Budget und Anzeige:** Der gesicherte Messkandidat
   ([CANDIDATE.patch](../audits/PR206_C1_BUDGET_20261019/CANDIDATE.patch), 48/48
   JS-Test) ist die Ausgangsbasis, kein freigegebenes Image. Limit auf den
   freigegebenen Wert setzen, Budget-/Golden-/Overflow-Assertions und WEB_UI
   nachfuehren, native Budget-/Golden-/Overflowregression und den ausfuehrenden
   JS-Test auf dem Umsetzungscommit wiederholen. Alle bisherigen Guards, Header und
   Sprachen bleiben. Synthetische Daten, keine Domainnachbildung, keine
   Netzwerkaktion.
2. **C2 – gemeinsame Projektion und Transport:** `fermentation_ui_models.*`,
   `fermentation_ui_projector.*`, `fermentation_application.cpp` um die begrenzte
   Anzeigeprojektion und den D2-A-Zeitinput ergaenzen; bestehende control_context-
   Resolver verwenden. `web_json_codec.*` um das additive run-Objekt, bestehende
   Routen unveraendert. Keine Aenderung an FSM-/Run-/Recovery-/Persistenzlogik,
   Plattformports, Adapter, Mutations- oder Sessionvertraegen. **Ressourcen-
   nachweis in diesem Schnitt (offline, ohne Geraet):** `static_assert`/Xtensa-
   `sizeof` fuer das reale Laufdetail gegen die Schaetzung; Allokationsnachweis im
   nativen `/status`-Worst-Case-Test ueber **beide** Wege: der vorhandene
   `operator new`-Zaehler sieht cJSON-`malloc`/`free` nicht, daher zusaetzlich
   eine isolierte Testinstrumentierung ueber `cJSON_InitHooks` (nur im Test, keine
   Produktions-Allocator-Abstraktion). Ausweisen: Allokationsanzahl, angeforderte
   Bytes, groesster Einzelblock und maximal gleichzeitig lebende Bytes inklusive
   cJSON-Baum und Ausgabestring (Worst-Case-Body mit 96-B-/Escape-Name, Run-ID
   48 B, alle vier Abschlussmodi); Messfenster und Aussagegrenzen (Host, keine
   Blockkoepfe/Fragmentierung/WLAN-Nebenlaeufigkeit) im Ergebnis dokumentieren.
   Der vorhandene `operator new`-0-Allokations-Test des UI-Steady-State bleibt
   separat unveraendert; dort nur ein zusaetzlicher 96-B-Namensfall; statische
   Framegroesse (`-fstack-usage`) fuer Status-Handler und Codec, als Zahl berichten,
   nicht als Geraetebeweis.
3. **C3 – integrierte Anzeige und Nachweis:** C1-Shell an reale API-Projektion
   binden; `scripts/test_web_shell_polling.mjs`, vorhandene native UI-/Webtests
   und Frozen-Golden gezielt ergaenzen. Der C1-Kandidat verwendet bereits die
   Feldnamen aus §3; die Bindung darf den Normalbody um hoechstens 214 B
   (konservativ 195 B) ueber den C1-Messstand vergroessern, sonst **HOLD**. WEB_UI
   fuer gelieferte Felder, Grenzen und das gewaehlte Budget aktualisieren, passende
   SIM-Akzeptanzfaelle in ACCEPTANCE_TESTS und Status in ROADMAP nachfuehren.
   Ergebnis auf exakten Quell-/Testcommit binden; Builder-Self-Check, dann
   Independent Review. Das Hardware-Gate H-RAM-1 (§7) ist ein eigener,
   ownerautorisierter Schritt **nach** dem Builder-Self-Check und kein Teil von C1–C3.

## 7. Gezielte Verifikation und verbleibender R1-Scope

| Nachweis mit bestehender Infrastruktur | Erforderliche Faelle |
|---|---|
| UI-Projektion / Application-Snapshot | Name aus eingefrorenem Lauf trotz Katalogaenderung; fehlende/manuelle Quelle; Sourceverlust/Standby/Ack/Abort; Ziel in Fermenting vs. Cooling/CoolHolding ueber echten Resolver; keine Sensor-/Aktorfreigabe durch Sollanzeige |
| Zeit-/Zustandsmatrix | Phasenwechsel, AdjustRun-Neubaseline, Dauergrenze/0, unbefristetes Halten; fehlende Zeit/Ruecklauf/falsches Prior-Tag/Basis/Bounds/Overflow; exakte Recovery vs. unklare Recovery; Completed kein weiterlaufender Timer. Vergleich mit vorhandenen FSM-Vertraegen statt zweiter Domainfunktion |
| Codec/produktiver GET /status | nullable Typen, alle vier Abschlussmodi, neue Felder mit bestehenden Revisionen aus einem Snapshot; maximale Namen inkl. UTF-8/Escaping, sichere Zahlengrenzen, 3072 B und kompletter 503-Fehler ohne Suffix. Auth/Same-Origin/GET-only unveraendert |
| Echter extrahierter JS-Shelltest | DE/EN/ES, fremder Name als Text, unbekannte Tokens/Fallback; vorhandener/neuer/fehlender run-Block; alte/verzoegerte Antworten, Mutation/Refresh, langsame Gruppe, Abort, Stale/Offline und Recovery. LP-Details allein geben niemals Aktionen frei; alle bestehenden 41 Szenarien erhalten |
| Shell/F1/Abhaengige Konsumenten | Alle Auth-/Provisionierungs-/Recoveryzustaende plus maximale Sequenz, D1-Grenze/Reserve, kompletter Kopf+Suffix; alle bisherigen Funktionen/Texte/CSRF-/Sequenzheader erhalten; native Renderer-/UI-Revision-Regression wegen gemeinsamem Viewmodell |
| Ressourcen offline (C2/C3) | Xtensa-`sizeof` des realen Laufdetails gegen die Schaetzung; Allokationsnachweis ueber beide Wege (`operator new` und cJSON-Hooks) im nativen Codec-Worst-Case: Anzahl, angeforderte Bytes, groesster Block, maximal gleichzeitig lebende Bytes (96-B-/Escape-Name, vier Abschlussmodi, 3072 B, 503); statische Framegroessen von Status-Handler und Codec; ELF-Sektions- und Symbolvergleich gegen die Basis: tatsaechliche Deltas aller Sektionen (`.dram0.*`, `.iram0.*`, `.flash.*`) berichten und ihren Grund nennen; die Nulldifferenz des C1-Suffixkandidaten wird fuer C2/C3 **nicht** vorausgesetzt. Ergebnis hoechstens `RAM_OFFLINE_ESTIMATED`, nie `RAM_PASS` |

Geplante Befehle nach Umsetzung (nicht jetzt):

```bash
node scripts/test_web_shell_polling.mjs
pio test -e native -f test_fermentation_ui_models -f test_control_context -f test_process_state_machine -f test_web_application_routes -f test_renderer_boundary -f test_ui_steady_state_allocations
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

**H-RAM-1 – Hardware-Gate (Vorschlag, am 2026-10-19 mit Ownerfreigabe ausgefuehrt, Ergebnis siehe Evidence; der folgende Text ist der damalige Vorschlag):**
aktorfreies Board mit dem vollstaendigen C1–C3-Image (ein vergroesserter Shell-
Prototyp beweist den JSON-/Projektions-Heap nicht), ein normaler Browser/1 Client,
Login, Shellabruf, bestehendes 5-s-Polling, Schliessen und Erholung; keine
Vier-Client-/G1'-Kampagne. Bei Bedarf Image-Wahl M (benanntes Mess-Overlay mit
ausgewiesenem Image-Unterschied, weil das Produktimage Spitzen und HTTPD-HWM nicht
liefert) oder P (Produktimage, Spitzen/HTTPD-HWM bleiben NOT_PROVEN), und ein
aktiver Lauf nur, wenn er auf dem aktorfreien Board darstellbar ist (NOT_CONFIRMED,
Ownerklaerung). Messwerte: freier/minimaler interner 8-Bit-Heap, groesster Block
vor/waehrend/nach Shell und Statuscodec, Main-/HTTPD-Stack-HWM,
`ui_skipped_low_memory`, `heap_alloc_failed`, Reset/Panic/Watchdog, HTTP-Fehler und
Erholung gegen den eigenen Idle-Bezug; Referenz G0-R3 mit benannten
Unterschieden, keine neuen Schwellen. Flash, UART, Browserzugriff und Reset
brauchen je eine gesonderte Ownerfreigabe. Details und Schrittprofil:
[RAM-Bilanz §7](../audits/PR206_RAM_FEASIBILITY_20261019.md).

**Stand nach Umsetzung:** C1–C3 sind umgesetzt, der Builder-Self-Check und die
Ressourcenevidence stehen in der [Offline-Evidence](../audits/PR206_C1_C3_RESOURCE_EVIDENCE_20261019.md),
die Hardwaremessung in der [H-RAM-1-Evidence](../audits/PR206_H_RAM_1_EVIDENCE_20261019.md).
Der Independent Implementation Review ist `GO`. Der lokale Pre-Ready-Lauf ist vom
Owner ausdruecklich autorisiert; sein Ergebnis steht im PR-Handover. Kein
Ready/Merge durch den Builder. `ACTUATOR_RELEASE=NO`, G1' storniert/NOT_RUN, F4
PROVISIONAL, offene R1-U/D9-Abnahme und saemtliche historischen FAIL/HOLDs
unveraendert.
