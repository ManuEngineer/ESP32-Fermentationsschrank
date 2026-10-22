# PR #209 – kombinierte Q4-Machbarkeitsprobe

**Ergebnis: Q4_FEASIBILITY_HOLD.** 2026-10-22, Basis/Plan-HEAD
`47b782e46b496027074b0775d7102b23034cbc43`, Gesamtplan Revision2.4 §13.
Produkt-/Firmwarestand bleibt `095f393f0eed4eb95944c164a8e967ce973c3fc9`.
Nur Delta zur [vorhandenen Diagnose](PR209_RUN_STACK_DIAGNOSIS_20261022.md).
Q3/C0–C5, Produkt-/Testcode und Budgets unveraendert. Keine Produktfreigabe.

## Genau eine kombinierte temporaere Quellvariante

Eine vollstaendige Kopie von `lib/fermentation_app/src` unter
`/tmp/pr209-q4-combined/src`, gemeinsame Header fuer alle Translation Units.
A/B/C wurden dort zusammengefuehrt:

- Application: ein transienter aligned/nothrow-Owner des bestehenden
  CommandDecision; direkter Aufruf des non-null In-place-Bridge-Eintritts.
- Bridge: Destination per Referenz im Visitor erfasst, keine Closurekopie der
  Decision; alle sieben Web-erreichbaren Typvarianten rufen Into-Kerne auf.
  Kompatibler Nullable-/Value-Eintritt delegiert. Kein zweiter Algorithmus.
- Commandkerne: vorhandene Start-Into-Kerne plus gleichartige Into-Kerne fuer
  Stop, Completion, Adjustment, Ack/Mute; Value-APIs delegieren. Kandidaten
  verwenden `decision.after`, Restore aus `before` auf abgelehntem Ruecklauf,
  keine Selbst-Move-Zuweisung. Keine Mutation des echten Current.
- `web_application_routes.cpp` ist in dieser gemeinsamen Variante **bytegleich
  zum Produkt** und wird mit denselben temporaeren Headern mituebersetzt.
  Kein weiteres Webframe-/Factory-Optimierungsexperiment nach dem FAIL-Befund.

[Einziger kombinierter Patch](PR209_Q4_COMBINED_20261022.patch), nur als
Analyseartefakt mit null Kontextzeilen (`--unidiff-zero`);
**nicht auf das Produkt angewendet**.
[Quellmanifest](PR209_Q4_COMBINED_SOURCES_20261022.tsv), Bundle-SHA-256
`b67157f9c6068efbae76469dddcddc81ac1197886e2000e201d846813190bd89`
(je sortiertem Dateinamen UTF-8: Name, NUL, Bytes, NUL).

Acht erfolgreiche Compile-Aufrufe: Application, Bridge, Run-Commands und Web-
Routen jeweils mit den vorhandenen Release-/Bring-up-Xtensa-Befehlen.
Originale Includes/Defines/ABI/CXX-Flags, GCC15.2.0 `esp-15.2.0_20251204`,
IDF v6.1; nur `-c`/`-o` nach `/tmp`, temporaeres Includeverzeichnis zuerst,
zusaetzlich RTL-/Tree-Dumps. Kein Linken, Ausfuehren oder Firmwarebuild.
Die vorige fixe Decision-Ownerbelegung wurde **nicht erneut gemessen**.

## Frames der kombinierten Variante, beide Profile

[Rohe gemeinsame Framewerte](PR209_Q4_COMBINED_FRAMES_20261022.tsv), alle static.
Die folgenden Werte stammen gemeinsam aus dieser Variante; keine Addition
isolierter A/B/C-Gewinne.

| Web-Typvariante / zugehoeriger Into-Kern | Visitor B | Into-Kern B |
|---|---:|---:|
| ProgramStart (stored und manual-timed) | 32 | 3104 |
| ManualStart (holding) | 32 | 320 |
| Stop (inkl. AbortAndCool) | 32 | 240 |
| Completion (inkl. Kuehl-Ersatzlauf) | 32 | 240 |
| Adjustment | 32 | 400 |
| AcknowledgeMessage | 32 | 32 |
| MuteMessage | 32 | 32 |

Application `applyConfirmedPrepared`784 B, non-null Bridge48 B,
`applyRunMutation` **2048 B**, gemeinsamer Mutationshandler544 B,
`applyRunCommand`32 B. Die RecoveryTimeCorrection-/SensorSelection-Visitoren
bleiben10144 B, sind aber im geschlossenen Web-Codec nicht erzeugbar; sie
werden nicht als Webvarianten oder als erfolgreich korrigiert dargestellt.
Die temporaere Application ruft den neuen kleinen Eintritt direkt auf;
der grosse kompatible Nullable-Fallback liegt nicht auf diesem Webpfad.

## Phasenbilanz und nachgewiesene Restluecke

[Ausgewaehlte Compiler-Callgraphpfade](PR209_Q4_PHASE_PATHS_20261022.tsv):
Einzelwerte mit Quelle; fehlende Zielmetadaten explizit **NOT_PROVEN**.
Die Pfade kombinieren die vier gemeinsamen TUs mit quellgleichen, unveraenderten
Callee-Artefakten des Produktbuilds095f393. Die Summe bekannter Knoten ist
keine Obergrenze; indirekte Targets, Konstruktoraliase, Rekursion und SDK-/NVS-
Rahmen bleiben ohne belastbare Aufloesung offen. Nicht als0 B modellieren.

HTTPD-Taskeintritt/SDK-Praefix: **NOT_PROVEN**. Bekannte nachfolgende
Einzelframes: Adapter704 B, Dispatcher96 B, handleRun32 B, Web-Mutation544 B,
applyRunMutation2048 B. Nicht alle Praefix-/Wrapperframes werden fuer den
folgenden FAIL-Untergrenzennachweis benoetigt.

| Phase | Ausgewaehlte bekannte, gleichzeitig lebende Frames B | Teil-Summe B | Vollkette |
|---|---|---:|---|
| Prepare Program | Mutation544 + Web2048 + prepareEnvelope48 + prepareStartProgram2592 + makePreparedRequest1680 | 6912 | NOT_PROVEN; SDK/Praefix/Visit-/Konstruktorpfade fehlen |
| Confirm | Mutation544 + Web2048 + confirmPrepared896 + revalidatePreparedRequest560 + Program-Revalidatevisitor528 | 4576 | NOT_PROVEN; weitere Wrapper/Targets fehlen |
| Program-Decision bis Factory | Mutation544 + Web2048 + Application784 + Visitor32 + StartInto3104 + ActiveRun::start3136 | **9648** | **FAIL**, noch ohne Bridge/Praefix/Validierung |
| Manual-Decision, nur Eintritt | Mutation544 + Web2048 + Application784 + Visitor32 + Into320 | 3728 | NOT_PROVEN; Domainunterpfade fehlen |
| Stop/Completion, nur Eintritt | Mutation544 + Web2048 + Application784 + Visitor32 + Into240 | 3648 | NOT_PROVEN; u.a. Ersatzlaufvalidierung fehlt |
| Adjustment, nur Eintritt | Mutation544 + Web2048 + Application784 + Visitor32 + Into400 | 3808 | NOT_PROVEN; Run-/Snapshotvalidierung fehlt |
| Ack/Mute-Decision, nur Eintritt | Mutation544 + Web2048 + Application784 + Visitor32 + Into32 | 3440 | NOT_PROVEN; Kopier-/Validierungsunterpfade fehlen |
| Ack/Mute-RAM-Apply | Mutation544 + Web2048 + Application784 + applyRunCommand32 | 3408 | NOT_PROVEN; kein persistierender Pfad |
| FreshStart-Persistenz/Validierung | Mutation544 + Web2048 + Application784 + FreshStart48 + Internal720 + writeSnapshot208 + Core1168 + Encode112 + Snapshotvalidierung128 + Programmsnapshotvalidierung96 + validTargetTemperature384 | 6240 | NOT_PROVEN; weitere Validierungs-/SDK-Targets fehlen |
| Andere Persistenz/Validierung | wie zuvor, persistCommand160 statt FreshStart48 | 6352 | NOT_PROVEN |
| Schreiben/Readback, vor Storetarget | Mutation544 + Web2048 + Application784 + FreshStart48/Command160 + Internal720 + writeSnapshot208 + Core1168 | 5520/5632 | NOT_PROVEN; Store-/NVS-/SDK-Targets fehlen |

Prepare, Confirm, Decision und nachfolgende Persistence/Readback werden nicht
als gleichzeitig addiert. Die Bridge-/Domain-Decisionframes sind beim
Persistence-Aufruf bereits verlassen. Auch Validierung und Storewrite werden
nicht als unabhaengige Peaks aufeinander gestapelt. Settings bleiben aus Q3
unveraendert: Teilframes2928 B, **Settings-Vollkette NOT_PROVEN**.

### Konkrete gemessene Ursache fuer HOLD

Die kombinierte Program-StartInto-Funktion3104 B ruft weiterhin die bestehende
`ActiveRun::start(const ProgramRunSource&, ...)` aus `run_snapshot.cpp:346`
mit **3136 B** auf. Direkte Compilerkante in beiden Profilen bestaetigt;
der Caller wertet das zurueckgegebene Optional aus und baut danach den Snapshot.
Die Factory erstellt weiterhin ein grosses ActiveRun-Aggregat als Value-
Rueckgabe fuer das Optional. Der Caller haelt seinerseits das grosse
ActiveRun-Optional. Die bereits bekannte Container-/Rueckgabeduplizierung ist
somit unterhalb des gemeinsam korrigierten Decisionpfads noch vorhanden.

**9648−8192 =1456 B Ueberschreitung**, bereits in sechs nachgewiesenen Frames.
Keine fehlende SDK-/NVS-Kante kann diese Untergrenze verbessern. Gegenueber der
vorigen isolierten Betrachtung6512 B wird hier der tatsaechlich aufgerufene
3136-B-Factoryframe mitgezaehlt. Die alte arithmetische1680-B-Restangabe war
keine Reserve und wird durch die kombinierte Kette widerlegt.

### Was am 2048-B-Webframe lokal absehbar ist

Tree/RTL der kombinierten Web-TU zeigt zwei lebende const
`FermentationApplicationRequestResult` (prepared und confirmed, je856 B
laut vorhandener ABI-Evidence), Context und UI-Return-Temporary208 B.
Diese verschachtelte Bestaetigung ist im Produktpfad unveraendert.
Den ersten Request nach Confirm zu zerstoeren verkleinert nicht automatisch
den am Funktionseintritt reservierten Maximalframe; waehrend Confirm sind
Input und neues Ergebnis gemeinsam noetig. Nur const zu entfernen oder eine
Rueckgabe in denselben Wert zuzuweisen beweist keine Return-Slot-Elimination.
Kein entsprechender Bytegewinn wird gutgeschrieben.

Eine explizite In-place-Confirmation/Requestkonstruktion waere eine gesondert
zu pruefende lokale API-/Alias-Aenderung. Selbst ein hypothetisch entfallener
856-B-Request liesse im obigen Kern8792 B uebrig, noch vor Praefix/SDK.
Staerkerer Gegenbeweis: selbst bei **vollstaendig weggedachtem** Webframe
blieben9648−2048+Adapter704 = **8304 B >8192 B**, noch ohne weitere Aufrufer.
Das ist eine Unmoeglichkeits-Gegenrechnung, **kein gemessener Reduktionswert**.
Eine reine Webframekorrektur kann den kombinierten Program-Pfad nicht retten.

## Dynamische Kopien/Buffer und bestehende Owner

Keine erneute Vermessung der bekannten fixen10104-B-Decision-/10256-B-
Allocator-Komponente; keine Empfehlung, sie jetzt produktiv zu allokieren.

Im kombinierten normalen Decisionpfad ist kein neuer expliziter Deepcopy
gegenueber dem Produkt erkennbar: die bestehenden before/after-Kopien bleiben,
Visitor erfasst Destination per Referenz, Value-Return-Decisionkopien entfallen,
die dritte RunCommandState-Kandidatenkopie wird durch vorhandenes after ersetzt.
Request-/Confirmed-Programmdaten und ActiveRun-Quellkopien bleiben dagegen
lebend. Das Verschieben von Containern auf den Heap macht deren vorhandene
String-/Vektorpuffer nicht kostenlos.

Auf Ablehnungs-/unconfirmed-Pfaden kann der Restore `after = before` nach
teilweisem Aufbau bereits veraenderte Puffer neu zuweisen, waehrend before,
Prepared/Confirmed und ggf. ActiveRun-Temporary noch leben. Physischer Peak
und groesster Block dieser dynamischen Unterpuffer sind **NOT_PROVEN**;
es gab keinen Allokations-/Laufzeittest. Keine ungemessene0-B-Zusatzpeakzusage.
Post-Decision leben Owner und before/after noch ueber Validation/Serialisierung/
Store-Readback; Payload-/Record-/Readbuffer koennen dazu ueberlappen.
Das ist derselbe verbleibende Ressourcenrisikobereich, kein Heap-PASS.

Der vorhandene12752-B-Coordinator-WorkingSet ist nicht exklusiv frei:
`persistCommandInternal` befuellt `workingSet_.candidate`, bildet Snapshot/
Record und verwendet gleichzeitig die Decision fuer Stale-/Before-/Effects-
und Applypruefung. Application-Serialisierung beweist keine gegenseitige
Lebensdauerexklusivitaet dieser gleichzeitig benoetigten Objekte. Keine
Ueberlagerung und kein zweiter WorkingSet angenommen.

Da das Stackgate bereits eindeutig scheitert, werden weitere Peakmessungen
oder ein neuer Allokationsvorschlag nicht als Voraussetzung einer jetzigen
Implementierungsempfehlung nachgeschoben. Keine solche Empfehlung besteht.

## OOM-Vertragsentscheidung, noch ohne Enum-/API-Implementierung

Bevorzugt fuer einen spaeter ueberhaupt tragfaehigen Kandidaten:
`FermentationUiDetailStatus::ResourceUnavailable`, Phase **DecisionOnly**,
Kategorie **Unavailable**, Web-Outcome weiter `unavailable`. Kein
`fromRunPersistenceResult(Busy)`/OwningOutcome vor Eintritt in den Persistence-
Owner; CapacityReached bleibt fachliche Kapazitaetsgrenze/TooLarge und ist
kein OOM-Ersatz. Die schon reservierte HTTP-Sequenz wird wie bisher mit genau
diesem Unavailable-Ergebnis abgeschlossen/gecached; gleicher Replay fuehrt
keine erneute Allokation/Mutation aus. Keine neue Retry-/CommandID-Regel.

Die kombinierte Compile-only-Variante hat lediglich den ausgewiesenen leeren
OOM-Returnstub, **nicht** diese neue Enum oder deren API-/Consumerabbildung.
Sie liefert weder OOM-Semantik- noch Laufzeitkorrektheitsbeweise.

## Knappes naechstes technisches Vorgehen / Ownerentscheidung

**STOP: Q4_FEASIBILITY_HOLD**, nicht Implementierungsfreigabe.
Die vorhandene Richtung genuegt selbst kombiniert nicht dem8192-B-Gate.
Naechste begrenzte technische Option: eigene Diagnose der bestehenden
`run_snapshot.*`-ActiveRun-Factory und ihrer Caller-/Return-Temporaries,
bevorzugt direkte Konstruktion im vorhandenen Decision-After-Storage ohne
weiteren Heapowner; dabei Snapshot-/Ablehnungs-/Bestaetigungsvertrag erhalten.
Zusaetzlich den nahen Prepare-Program-Pfad (2592/1680-B-Requestkonstruktion)
mitbetrachten, damit nicht nur ein Decision-Teilpfad verbessert wird.
Web-2048-B-Lebensdauern koennen dabei bewertet, aber nicht als schon reduzierte
Reserve eingerechnet werden. Diese weitere Quellvariante wurde **nicht** gebaut.

Ownerentscheidung: diese eng benannte weitere Diagnose gesondert beauftragen
oder HOLD beibehalten. Kein automatischer Scope-/Budgetwechsel, kein neuer
Worker/Queue/Persistenzpfad, keine Produkt-/Testaenderung aus diesem Auftrag.
D9/Hardware-/HWM-/Heapqualifikation getrennt NOT_RUN; Q3 und dessen bisherige
Regressionen bleiben unveraendert. Keine Governance-/Testlaeufe wiederholt.


## Verifikation des Diagnose-Deltas

Acht gemeinsame Compile-Aufrufe PASS, Frameparitaet beider Profile PASS;
[22 direkte Kanten](PR209_Q4_COMBINED_EDGES_20261022.tsv) (11 je Profil) vom
Web-Handler ueber Application/non-null Bridge und alle sieben Visitoren bis
zu ihren Kernen sowie StartInto → bestehende Factory bestaetigt. Diese Kanten
begrenzen keine anderen indirekten/SDK-/NVS-Pfade.
Produkt-/Q3-/Testdateien gegen Basis47b782e sowie beide vorhandenen BIN-Digests
von095f393 unveraendert. Dokument-/Artefaktlinks und Diff geprueft; keine
Governance-, Regressionstest- oder Builderlaeufe wiederholt.

Reproduktion: genau eine Sourcekopie einschliesslich ihrer Header anlegen,
Patch dort mit `--unidiff-zero` anwenden und Quellmanifest pruefen. Die vier
originalen Profileintraege aus `compile_commands.json` via `shlex.split`
uebernehmen, `-c`/`-o` nach `/tmp` und das gemeinsame Includeverzeichnis zuerst
setzen. Vorhandene `-fstack-usage -fcallgraph-info=su` beibehalten, optional
`-fdump-rtl-expand -fdump-tree-optimized`. Kein Owner-sizeof-Probe, Link oder
neuer Buildtreiber. Unveraenderte Callee-Frames stammen aus demselben095f393-
Profilbuild; offene Kanten in den Phasenpfaden sind NOT_PROVEN.
