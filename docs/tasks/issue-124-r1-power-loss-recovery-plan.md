# Issue #124 – R1-Stromausfall-Recovery auf einfachen Zeitvertrag konsolidieren

## Planstatus und unveraenderliche Grenzen

```text
PLAN_ONLY=YES
IMPLEMENTATION=NO
PRODUCTION_CODE_CHANGE=NO
TEST_CODE_CHANGE=NO
SCHEMA_CHANGE=NO
ISSUE=124
BASE_BRANCH=integration/r1-development
BASE_SHA=7642739c8a05fb08615ff04e2d0770c5a381b23d
PLAN_BRANCH=agent/issue-124-r1-power-loss-recovery-plan
POWER_LOSS_ALONE_REQUIRES_USER_CONFIRMATION=NO
TRUSTED_CURRENT_FERMENTING_LOGICAL_RECOVERY=AUTOMATIC
ACTUATOR_RELEASE_FROM_RECOVERY_ALONE=NO
FERMENTATION_DURATION_REACHED_DURING_OUTAGE=USE_NORMAL_FSM_COMPLETION_SEMANTICS
USER_CONFIRMATION_ONLY_BECAUSE_OF_OUTAGE=NO
NO_DOUBLE_COUNTING=YES
NO_NEW_PERSISTENCE_FIELD=YES
NEW_GENERIC_TIME_QUALITY_ENUM=NO
NEW_APP_SPECIFIC_DEVICE_PLATFORM_TIME_TYPE=NO
SECOND_RECOVERY_COORDINATOR_PLANNED=NO
SECOND_PERSISTENCE_COORDINATOR_PLANNED=NO
OLDER_VALID_CHECKPOINT_AUTO_PROMOTION=NO
OLDER_VALID_CHECKPOINT_AUTO_ACTIVATION=NO
DELETE_ALL_C2_FILES_AS_GOAL=NO
R1_ACTIVE_CALL_GRAPH_TO_WEIGHTED_RECOVERY=NONE
R1_ACTIVE_CALL_GRAPH_TO_BIOLOGICAL_MODEL=NONE
OWNER_DECISIONS_REQUIRED=NONE
REPLACES_R1_NO_FERMENTING_RESUME_POLICY=YES
REOPENS_ISSUE18=NO
REOPENS_ISSUE24=NO
TEMPERATURE_WEIGHTED_RECOVERY_R1=NO
```

Dieser Plan ist die vollstaendige aktuelle Fassung fuer Issue #124. Er ist
kein Zusatz zu einem frueheren R1- oder C2-Plan. Die fachliche Umsetzung
beginnt erst nach Owner-Freigabe dieses exakten Plan-Commits. Dieser PR
enthaelt nur diesen Plan und die minimale Reihenfolgesynchronisierung in
`docs/ROADMAP.md`.

Issue #18 bleibt abgeschlossene historische Provenienz der frueheren
temperaturgewichteten C2-/Future-Recovery. Issue #24 bleibt abgeschlossene
KISS-/fail-closed-Safety-Provenienz. Beide Issues werden nicht wiedereroeffnet.
Die konkrete #24-Entscheidung „kein FERMENTING-Resume“ wird fuer R1 durch
Issue #124 ersetzt, ohne das Safety-Fundament oder die Abbruch-/Aktorregeln
aufzugeben.

## 1. Anlass und Source-of-Truth-Befund

Der live verifizierte Arbeitsstand ist:

```text
BRANCH_TOPOLOGY=
main
  -> integration/r1-development
EXPECTED_R1_DEVELOPMENT_HEAD=7642739c8a05fb08615ff04e2d0770c5a381b23d
ACTUAL_R1_DEVELOPMENT_HEAD=7642739c8a05fb08615ff04e2d0770c5a381b23d
OPEN_PULL_REQUESTS=0
```

Der aktuelle R1-Vertrag in
`docs/RECOVERY_AND_INTERRUPTION.md` und `docs/RUN_PERSISTENCE.md` verwirft
einen technisch vertrauenswuerdigen, aber semantisch nicht explizit
resumierbaren Lauf als `NoActiveRun`. Fuer `FERMENTING` gibt es dort keine
Wall-Clock-Ausfallzeitgutschrift und keine R1-Fortsetzung. Das steht im
Widerspruch zur verbindlichen Owner-Zielsetzung dieses Auftrags:

```text
Strom aus != Fermentation aus
```

Damit ist der aktuelle fachliche R1-Recoveryvertrag als Konflikt bestaetigt.
Die normative Dokumentbereinigung erfolgt erst in der Umsetzung nach diesem
Plan. In dieser Planrunde werden keine widerspruechlichen Recoverydokumente
umgeschrieben.

Die Architekturquelle ist dagegen konsistent: ADR-013 und die abgeschlossene
#121-Baseline verlangen weiterhin `device_platform` fuer app-neutrale Ports,
`device_platform_esp_idf` fuer konkrete Adapter, `fermentation_app` fuer die
Fachlogik und `main/app_main.cpp` als Composition Root. Issue #124 benoetigt
keine neue Architekturgrenze und keinen zweiten Recovery- oder
Persistence-Coordinator.

## 2. Vollstaendiger Ist-Audit

### 2.1 Normative und historische Dokumente

Geprueft wurden die vollstaendigen aktuellen Recovery-, Persistenz-, FSM-,
Safety- und ADR-Dokumente sowie die vier geforderten historischen Plaene.
Der relevante Iststand ist:

| Quelle | Aktueller Vertrag/Befund | Bedeutung fuer #124 |
|---|---|---|
| `docs/RECOVERY_AND_INTERRUPTION.md` | R1 all-off; `FERMENTING` nicht fortsetzbar; kein R1-UTC-/Ausfallzeitkredit; `RECOVERY_TIME_PENDING` bisher nur als C2-Legacy-Kontext; #90-Fallback nicht aktivierend | Muss nach Owner-Freigabe auf den einfachen Zeitvertrag aktualisiert werden |
| `docs/RUN_PERSISTENCE.md` | `rh0`/`rc0`/`rc1`, vollstaendige Head-/Slot-/CRC-/Epoch-/Referenzvalidierung; Checkpoint 1/5/60 Minuten; aktive Schema-3-C2-Felder lesbar; aktuelle `Current`-Recovery fachlich nicht resumierbar | Persistenzgrenze und Write-before-Apply bleiben; kein unnoetiger Schemaumbau |
| `docs/STATE_MACHINE.md` | FSM ist deterministisch sowie frei von Hardware und Persistenz; `ProcessState::RecoveryEvaluation` existiert; `RECOVERY_TIME_PENDING` ist bisher nur ein Kontext; Boot und direkte GPIO-Ausgabe bleiben getrennt | Bestehenden Prozesszustand nutzen, kleine Recovery-Disposition ergaenzen, keine FSM-/Hardwarevermischung |
| `docs/SYSTEM_SAFETY_AND_RECOVERY.md` | #24-KISS: all-off, kein automatischer Restart, untrusted bleibt `SAFE_BOOT`, frische Sensor-/Safety-Evidenz vor Freigabe | Wall-Clock-Berechnung darf keine Aktorfreigabe oder Safety-Abkuerzung erzeugen |
| `docs/DECISIONS.md` | ADR-013 Plattform-/App-Grenzen, ADR-014 FSM, ADR-016 IStateStore/Envelope/Schema; kein neuer Recovery-ADR | Bestehende Contracts wiederverwenden; ADR-Aenderung erst bei nachweislich neuer Entscheidung |
| `docs/tasks/issue-18-restart-weighted-progress-plan.md` | Historischer C2-Plan mit `RecoveryOutageBounds`, Pending Anchor, UTC-Intervall, Temperaturgewichtung und `RunRecoveryCoordinator` | Provenienz und Negativgrenze; nicht reaktivieren |
| `docs/tasks/issue-24-safety-core-replan.md` | Abgeschlossener KISS-Plan: kein Charge-Rescue, keine gewichtete Recovery, untrusted `SAFE_BOOT`, trusted nicht resumierbar -> `NoActiveRun` | Safety-Grundlage behalten; FERMENTING-Policy gezielt ersetzen |
| `docs/tasks/issue-90-clean-restart-plan-r5.9.md` | `rh0`/`rc0`/`rc1`, strikte Callback-12-/Cutpoint-Orakel, `OLDER_VALID_CHECKPOINT_RESUME` als nicht aktivierendes Fallback-Angebot | #90 nicht aufweichen; Fallback separat und fail-closed behandeln |
| `docs/tasks/issue-121-lifecycle-safety-simplification-plan.md` | Aktuelle Application-/Composition-Baseline; `ConfigurationRecoveryService::boot()` -> `ConfigurationService::acquireRuntime()` als Trust-Gate; ein `RunPersistenceCoordinator`; alter C2-Recoverypfad nicht aktiv | Neue Zeitquelle am bestehenden app-neutralen Port ueber Composition Root zufuehren |

### 2.2 Produktionscode, Header und direkte Konsumenten

Geprueft wurden die geforderten Dateien mit ihren zugehoerigen Headern und
direkten Konsumenten:

- `boot_classification.cpp/.hpp`: R1-Resume ist aktuell nur fuer
  `PREHEATING`, `COOLING` und `MANUAL_HOLDING` zulaessig. `FERMENTING`,
  `RecoveryEvaluation`, C2-Pending-/Episode-/Gewichtungsfelder und
  `PartialUnknownHistory` werden aktuell als `NoActiveRun` beziehungsweise
  technisch unsicher klassifiziert.
- `run_recovery.cpp/.hpp`, `run_recovery_time.cpp/.hpp`,
  `run_recovery_types.hpp` und `run_progress_weighting.cpp/.hpp`: enthalten
  noch den historischen C2-`RunRecoveryCoordinator`, UTC-Intervall- und
  temperaturgewichteten Modellcode. Der Pfad ist in der #121-Produktkomposition
  nicht der aktive R1-Weg, wird aber von Headern, Orchestrator und Tests
  referenziert. Die Umsetzung muss ihn aus dem aktiven Vertrag entfernen oder
  nachweislich stilllegen, statt einen zweiten Coordinator zu bauen.
- `run_persistence_coordinator.cpp/.hpp`: ist der bestehende alleinige
  Persistenzbesitzer. Er validiert Head, Slots, CRC, Schema, Epoch und
  Referenzen, implementiert Write-before-Apply sowie den indeterminierten
  Zustand und besitzt sowohl die aktuelle R1-Aktivierung als auch den alten
  C2-Recovery-/Recovery-Candidate-Pfad. Die neue Entscheidung muss hier oder
  in einer kleinen direkt zugeordneten `fermentation_app`-Disposition liegen,
  nicht in einer zweiten Persistenzengine.
- `run_persistence_contract.hpp/.cpp` und `run_persistence_codec.cpp`: Schema
  3, `RunCheckpointTime` mit optionalem UTC-Wert, monotone
  `checkpointMonotonicMillis`, `runProgress.observedRunSeconds`,
  `nominalDurationSeconds` und die alten C2-Felder sind vorhanden. Die
  Codec-/Envelope-Validierung ist strikt.
- `fermentation_application.cpp/.hpp`: konfiguriert zuerst den Trust-Gate-
  Pfad, laedt danach genau einen `RunPersistenceCoordinator`, klassifiziert
  den Bootzustand und haelt `pendingResume_`. Aktuell wird ein leerer
  `RunCheckpointTime` mit Nullzeit verwendet, es gibt keine `ITimeSource`-
  Abhaengigkeit und `update()` evaluiert Recovery nicht erneut.
- `temperature_control_orchestrator.cpp/.hpp`: ist die bestehende
  Application-Grenze fuer Persistenz-/Runtime-Handoffs, enthaelt aber noch
  eine `RunRecoveryCoordinator`-Abhaengigkeit fuer den historischen Pfad.
  Diese Abhaengigkeit darf nicht zum neuen Recovery-Owner werden.
- `process_state_machine.cpp/.hpp`: enthaelt `ProcessState::RecoveryEvaluation`,
  Recovery-Events und explizite Command-/Transition-Pfade. Die FSM darf
  weder absolute Zeit lesen noch Persistenz laden oder Aktoren schalten.
- `device_platform/src/time_source.hpp`: bietet bereits den app-neutralen
  `ITimeSource`-Port mit monotoner Zeit und `optional<int64_t>` UTC-Zeit.
- `device_platform_esp_idf/src/esp_timer_time_source.*`: liefert derzeit nur
  monotone ESP-Timer-Zeit; `unixTimeSeconds()` liefert immer `nullopt`. Die
  konkrete asynchrone absolute Zeitquelle ist somit noch nicht produktiv
  angebunden. Eine Umsetzung darf dafuer nur den bestehenden Port und einen
  kleinen ESP-IDF-Adapterpfad verwenden, keinen fachlichen NTP-Coordinator in
  `device_platform`.
- `main/app_main.cpp`: ist Composition Root, erstellt die konkreten NVS-,
  Reset- und Time-Adapter jedoch aktuell erst nach `begin()`; die Zeitquelle
  wird nur fuer den Heartbeat verwendet. Die spaetere Umsetzung muss die
  bestehende Quelle vor Recovery-Bewertung uebergeben und NTP/WLAN dennoch
  asynchron lassen.
- `main/issue_90_slice7_harness.*` und `main/issue_29_bringup_probe.cpp`:
  sind getrennte Bring-up-/Oraclesichten. Sie duerfen weder als aktuelle
  physische Recovery-Abnahme noch als neue Composition verwendet werden.

### 2.3 Vollstaendige Verwendungssuche

Die geforderten Verwendungen wurden in `lib`, `main`, `test` und den
Recovery-/Persistenzdokumenten gesucht. Besonders betroffen sind:

```text
FERMENTING
PriorBootPhaseElapsed
pendingRecoveryAnchor
recoveryBootAnchorMonotonicMillis
nominalRecoveryAdjustment
weightedProgress
RecoveryEvaluation
ResumeOffer
RunLoadDisposition
OLDER_VALID_CHECKPOINT_RESUME
UTC
NTP
```

Die Verwendung verteilt sich nicht nur auf die vier Boot-/Recoverydateien:

- Produktionsvertraege und Codec serialisieren und validieren weiterhin die
  Schema-3-Felder.
- `run_persistence_coordinator` und die Orchestrator-Grenze enthalten den
  historischen C2-API-Pfad.
- `process_state_machine`, `run_commands`, `control_context` und
  `actuation_interlock` behandeln `RecoveryEvaluation` beziehungsweise
  fail-closed-Ausgaenge.
- `test_boot_classification`, `test_run_persistence_coordinator`,
  `test_run_recovery_time`, `test_run_progress_weighting`,
  `test_process_state_machine`, `test_run_checkpoint_codec`,
  `test_run_snapshots`, `test_run_checkpoint_schedule`, `test_time_source`
  und das #90-Recovery-Oracle decken die alte und die aktuelle Umgebung ab.
- `issue_29_bringup_probe` und der #90-Runner pruefen bewusst technische
  Bring-up-/Fallbackbedingungen und sind nicht automatisch R1-Produktbeweis.

Die Umsetzung muss deshalb alle genannten Konsumenten aktualisieren oder
begruendet als Legacy-/Kompatibilitaetspruefung behalten. Eine reine Aenderung
von `boot_classification.cpp` waere unvollstaendig.

## 3. Verbindlicher R1-Zielvertrag

### 3.1 Persistenz- und Checkpointgrundlage

Der bestehende Checkpointvertrag bleibt unveraendert:

```text
MINIMUM=1 minute
DEFAULT=5 minutes
MAXIMUM=60 minutes
EVENT_WRITES=IMMEDIATE
SENSOR_CYCLE_WRITE=NO
```

Die Umsetzung muss die bestehende `rh0`/`rc0`/`rc1`-Validierung, Epoch-, CRC-,
Referenz- und Write-before-Apply-Semantik unveraendert beibehalten. Ein
unbekannter Write-Ausgang bleibt indeterminiert und darf nicht als
Erfolg interpretiert werden.

### 3.2 Vertrauenswuerdiges FERMENTING

Ein `FERMENTING`-Checkpoint wird nur dann automatisch logisch fortgesetzt,
wenn alle folgenden Bedingungen vorliegen:

1. `Current` beziehungsweise der vollstaendig validierte Checkpoint ist
   technisch und fachlich konsistent geladen.
2. Run-, Config-, Schema-, Epoch-, CRC- und Referenzvalidierung sind vollstaendig
   erfolgreich.
3. Die persistierte Prozessphase ist `FERMENTING`.
4. Ein bestehender, absoluter und vertrauenswuerdiger Checkpoint-Zeitanker ist
   vorhanden.
5. Nach Boot liefert die bestehende app-neutrale Zeitquelle eine
   vertrauenswuerdige aktuelle absolute Zeit.
6. Es gibt keinen Widerspruch, Ruecksprung, Overflow, unaufgeloesten
   Indeterminiertheitsstatus oder sonstigen untrusted Recoverygrund.

Die exakte schemafreie Rechnung ist in Abschnitt 4.2 festgelegt. Sie
verwendet den UTC-Wert desselben validierten Checkpointrecords und den
persistierten Fortschritt plus den beim Checkpoint noch offenen Live-Abschnitt.
Eine fachlich aequivalente Darstellung ist nur zulaessig, wenn sie exakt diese
kanonischen vorhandenen Felder verwendet. Die Formel ist keine Temperatur-,
Biologie- oder Zustandsrekonstruktion.

Wenn die berechnete Gesamtzeit unter der nominalen Fermentationsdauer liegt,
wird der Kandidat ohne Benutzerbestaetigung logisch wieder als
`FERMENTING` fortgesetzt. Der Stromausfall allein erzeugt keinen Dialog und
keine fachliche Ablehnung.

Wenn die nominale Dauer erreicht oder ueberschritten ist, wird die normale
`FermentationCompleted`-Semantik der FSM verwendet; die verbindliche
Abgrenzung zu `Cooling` steht in Abschnitt 6.3. Auch dabei entsteht keine
Benutzerbestaetigung nur wegen des Stromausfalls.

### 3.3 Kein biologisches Modell

R1 verwendet keine temperaturgewichtete Ausfallrekonstruktion, keine
geschaetzte Temperaturkurve, keine biologische Gewichtungsfunktion, keine
modellbasierte Gutschrift und keine Approximation aus Vor-/Nach-
Ausfalltemperaturen. `weightedProgress`, `nominalRecoveryAdjustment`,
`PendingRecoveryAnchor`, Recovery-Episode- und Temperatur-Evidenz erzeugen
keine R1-Entscheidung. Sie duerfen nur aus Kompatibilitaetsgruenden lesbar
bleiben, sofern ihre Integritaetsvalidierung das zulaesst.

### 3.4 Boot, WLAN/NTP und TimePending

```text
RECOVERY_CONTRACT_IMPLEMENTED=PLANNED
REAL_NETWORK_TIME_PRODUCER_AVAILABLE=NOT_PROVEN_IN_CURRENT_REPOSITORY
WLAN_ONBOARDING_SCOPE=NOT_OWNED_BY_ISSUE124
CONNECTIVITY_UI_SCOPE=NOT_OWNED_BY_ISSUE124
```

Der verbindliche Ablauf ist nicht blockierend:

1. Reset/Boot setzt alle Aktorausgaenge auf `ACTORS_OFF`; kein elektrischer
   Zustand wird restauriert.
2. Konfiguration wird ueber
   `ConfigurationRecoveryService::boot()` und
   `ConfigurationService::acquireRuntime()` als einzigem Konfigurations-
   Trust-Gate bewertet.
3. Der bestehende Persistenzbesitzer laedt und validiert die aktuellste
   Evidenz samt Fallbackstatus.
4. Ein vertrauenswuerdiger `FERMENTING`-Run ohne aktuell verfuegbare absolute
   Zeit wird nicht als `NoActiveRun` verworfen. Er bleibt in
   `RecoveryEvaluation` mit einer kleinen, expliziten Disposition sinngemaess
   `TimePending`.
5. In `TimePending` bleiben Aktoren aus, es wird keine Restzeit geraten und
   kein Resume behauptet. Es gibt keine Persistenzmutation nur wegen des
   Wartens und keinen `NoActiveRun`-Tombstone. WLAN/NTP darf parallel arbeiten;
   Boot wartet weder synchron noch in einer Endlosschleife.
6. Sobald dieselbe app-neutrale Zeitquelle vertrauenswuerdige absolute Zeit
   liefert, wird genau dieselbe validierte Persistenzevidenz erneut bewertet.
   Die Bewertung darf nicht durch eine inzwischen neu erfundene oder
   unvollstaendig validierte Momentaufnahme ersetzt werden.
7. Eine gueltige Zeitrechnung fuehrt bei noch nicht erreichter nominaler
   Dauer automatisch zur logischen `FERMENTING`-Fortsetzung. Bei erreichter
   Dauer wird die normale `FermentationCompleted`-Semantik angewendet.
   Runtime/FSM, frische Config-/Sensor-/Hardware-/Safety-Evidenz und der
   bestehende Command-/Transition-Write-before-Apply-Pfad bleiben vor jeder
   eventuellen Aktorfreigabe.

`TimePending` ist ein RAM-/Application-Status unter dem bestehenden
`ProcessState::RecoveryEvaluation`, keine neue FSM-Hauptphase, kein neuer
persistierter Prozesszustand und keine Recoveryengine. Die typisierte
Disposition heisst semantisch `WaitingForTrustedTime`; der konkrete Enumname
darf den bestehenden Namenskonventionen folgen.

```text
FSM_STATE=RecoveryEvaluation
RECOVERY_DISPOSITION=WaitingForTrustedTime
ACTORS_OFF=YES
PERSISTENCE_MUTATION_WHILE_ONLY_WAITING_FOR_TIME=NO
NO_ACTIVE_RUN_TOMBSTONE=NO
NO_RESUME_CLAIM=YES
```

### 3.5 Unklare Lage und Aktoren

Bei fehlender oder widerspruechlicher Persistenz, fehlendem oder ungueltigem
Zeitanker, unklarer aktueller Zeit, ungueltiger Zeitordnung, Schema-/CRC-/Epoch-
Fehler oder unbekanntem Runzustand gilt:

```text
NO_GUESS
NO_AUTOMATIC_RESUME
FAIL_CLOSED
```

Der technische unbekannte Zustand darf nicht als Factory-New oder
`NoActiveRun` umetikettiert werden, um die Unsicherheit zu verstecken.

Eine berechenbare Wall-Clock-Zeit ist keine Aktorfreigabe. Die Kette bleibt:

```text
validierte Persistenz
-> Recovery-Disposition
-> gueltige Runtime/FSM
-> frische Config-/Sensor-/Hardware-/Safety-Evidenz
-> erst dann eventuelle Aktorfreigabe
```

Boot, Reset, Fehler, `TimePending`, Recovery-Angebot und unbekannter Zustand
bleiben all-off. Es gibt keine Wiederherstellung letzter GPIO-, H-Bruecken-
oder MOSFET-Zustaende.

Ein vertrauenswuerdig logisch fortgesetzter Current-Run ist davon getrennt:
`ACTUATOR_RELEASE_FROM_RECOVERY_ALONE=NO`. Aktoren bleiben aus, bis die
bestehende frische Configuration-/Sensor-/Hardware-/Safety-/Planner-Kette
erfolgreich durchlaufen ist.

## 4. Persistierte Felder, Wiederverwendung und Legacygrenze

### 4.1 Kein Schema- oder Wire-Break

```text
PERSISTENCE_SCHEMA_CHANGE_PLANNED=NO
```

Die Umsetzung darf keine neue Schema-Version, keinen neuen Pflicht-Key und
keinen unnoetigen Migrationsschnitt einfuehren. Der exakte bestehende
Feldvertrag ist in Abschnitt 4.2 festgelegt; eine neue Ownerentscheidung ist
dafuer nicht erforderlich.

### 4.2 Exakter schemafreier FERMENTING-Zeitvertrag

Der UTC-Wert des exakt validierten `RunPersistenceRawRecord` ist der
Checkpointrahmen. Er wird zusammen mit Checkpoint-Payload, Checkpointrevision
und den Zeitfeldern validiert. Keine spaeter beobachtete globale UTC und kein
anderer Record darf diesen Anker ersetzen.

Fuer einen `FERMENTING`-Checkpoint gilt:

```text
checkpoint_live_segment_seconds =
    (checkpointMonotonicMillis - processState.stateEnteredAtMillis) / 1000

persisted_elapsed_at_checkpoint =
    runProgress.observedRunSeconds
    + checkpoint_live_segment_seconds

outage_seconds =
    trusted_current_utc
    - checkpoint_record_utc

recovered_elapsed_seconds =
    persisted_elapsed_at_checkpoint
    + outage_seconds
```

Dabei sind die bestehenden Felder gemeint:

```text
RunPersistenceSnapshot::checkpointMonotonicMillis
RunPersistenceSnapshot::processState.stateEnteredAtMillis
RunProgressState::observedRunSeconds
RunPersistenceRawRecord::utcUnixSeconds
```

`RunCheckpointTime::utcUnixSeconds` ist der bestehende Eingang, aus dem der
Record-UTC-Anker beim Schreiben entsteht. Die monotone Zeit ist nur fuer den
noch offenen Live-Abschnitt innerhalb der aktuellen Boot-/Prozessbasis gueltig;
sie wird nicht selbst als bootuebergreifende Ausfallzeit verwendet.

Die Mindestvorbedingungen sind:

```text
processState.state == FERMENTING
processState.stateEnteredAtMillis <= checkpointMonotonicMillis
runProgress.basis == KnownTotal
checkpoint_record_utc present and trusted
trusted_current_utc >= checkpoint_record_utc
no arithmetic overflow
Current/record graph fully validated
```

Subtraktionen, Additionen, Divisionen und Saturierungen werden checked
ausgefuehrt. Bei `PartialUnknownHistory`, fehlender exakter Zeitbasis,
negativer Zeitdifferenz, Timestampwiderspruch, Revisionstausch oder Overflow
gilt `NO_EXACT_WALL_CLOCK_RECOVERY` und `FAIL_CLOSED`; es wird nicht geraten.

Wenn `recovered_elapsed_seconds` kleiner als die nominelle
Fermentationsdauer ist, wird der logisch fortgesetzte Kandidat vor dem
Write-before-Apply-Handoff so neu verankert:

```text
candidate.runProgress.observedRunSeconds = recovered_elapsed_seconds
candidate.processState.state = FERMENTING
candidate.processState.stateEnteredAtMillis = current_monotonic_millis
```

Der neue Boot misst danach nur noch den Live-Abschnitt ab dem neuen
`stateEnteredAtMillis`. Beim spaeteren normalen Verlassen von `FERMENTING`
addiert der bestehende `persistTransition()`-Pfad genau diesen neuen
Live-Abschnitt wieder zu `observedRunSeconds`. Bereits angerechnete Zeit wird
nicht in ein zweites Feld kopiert und nicht nochmals zur naechsten Recovery
addiert.

### 4.3 Alte Schema-3-/#18-Felder

Die historischen Felder bleiben, soweit die bestehende Codec-Kompatibilitaet
es verlangt, lesbar:

```text
pendingRecoveryAnchor
recoveryBootAnchorMonotonicMillis
recoveryTemperatureEvidence
lastRecoveryEpisodeEvidence
priorBootPhaseElapsed
nominalRecoveryAdjustment
recoveryEpisodeRevision
runProgress.weightedProgress
```

Sie werden fuer die neue R1-Wall-Clock-Entscheidung aktiv ignoriert. Ihre
blosse Anwesenheit macht einen ansonsten vollstaendig validierten aktuellen
Run nicht zu `NoActiveRun`; widerspruechliche oder nicht validierbare Daten
bleiben jedoch fail-closed. Der alte gewichtete Berechnungspfad darf nicht
ueber einen indirekten Fallback wieder in R1 gelangen.

Die Umsetzung entfernt oder entkoppelt den historischen
`RunRecoveryCoordinator`-API-Pfad aus der aktiven Produktkomposition und den
Orchestrator, sobald die vorhandenen Tests auf den neuen alleinigen
`RunPersistenceCoordinator`-Pfad migriert sind. Ein blosses Umbenennen in
einen zweiten RecoveryCoordinator ist ausgeschlossen. Ob die rein historische
Datei danach vollstaendig entfaellt, wird durch die Nutzungs-/Buildpruefung
des Umsetzungsslices entschieden; ihre bloesse Existenz ist kein aktiver
Vertrag.

## 5. Architektur- und Datenfluss

```text
main/app_main.cpp (Composition Root)
  -> bestehende device_platform::ITimeSource
  -> FermentationApplication
       -> Config-Recovery und acquireRuntime() als Trust-Gate
       -> einziger RunPersistenceCoordinator: load/validate
       -> Recovery-Disposition fuer Current/Fallback/Unknown
            -> WaitingForTrustedTime, solange trusted UTC fehlt
            -> Wall-Clock-Kandidat bei trusted UTC
            -> automatische logische FERMENTING-Fortsetzung oder normale FSM-Abschlusssemantik
       -> bestehende Process-FSM fuer logische Zustandsentscheidung
       -> frische Sensor-/Hardware-/Safety-Evidenz
       -> bestehende Aktor-/Planner-Grenze; Boot bleibt all-off
```

Die Fachlogik verwendet nur `device_platform::ITimeSource`. Ein
ESP-IDF-Adapter darf die absolute Zeit aus dem bereits vorgesehenen
System-/NTP-Zustand liefern, darf aber keine Fermentationsbegriffe in
`device_platform` einfuehren. `main/app_main.cpp` bleibt fuer Erstellung,
Verdrahtung und Lebensdauer der Quelle verantwortlich. WLAN/NTP wird nicht
zum synchronen Application-Bootvertrag.

Issue #124 besitzt keinen produktiven NTP-/SNTP- oder WLAN-Onboardingpfad.
Issue #89 fuer WLAN-Onboarding/Provisionierung bleibt separat offen. #124
besitzt ausschliesslich die ITimeSource-Injektion, die
`nullopt`-/trusted-UTC-Semantik und die nichtblockierende Re-Evaluation, wenn
ein spaeterer Producer ueber denselben Port trusted UTC liefert. Es implementiert
keinen WLAN-Lifecycle, Credential-Speicher, Connection Manager oder
Connectivity-UI. Der Status ist daher getrennt zu fuehren:

```text
RECOVERY_CONTRACT_IMPLEMENTED=PLANNED
REAL_NETWORK_TIME_PRODUCER_AVAILABLE=NOT_PROVEN_IN_CURRENT_REPOSITORY
```

Die Recoveryimplementierung darf ohne aktuell verfuegbaren realen
Netzwerkzeit-Producer native und fachlich abgeschlossen werden. Eine spaetere
Connectivity-Integration liefert lediglich Werte ueber den bestehenden
`ITimeSource`-Vertrag.

Die `ProcessStateMachine` bleibt rein deterministisch. Sie bekommt keine
Persistenz-, NTP-, UTC- oder Hardwareabhaengigkeit. Die Recoverybewertung und
der Zeitstatus bleiben an der bestehenden Application-/Persistenzgrenze.
`TemperatureControlApplicationOrchestrator` bleibt der Handoff fuer RAM-
Reset und Aktor-/Controller-Grenzen, nicht der Owner der Persistenz-
Recoveryentscheidung.

## 6. FERMENTING- und Recovery-Ablauf

### 6.1 Zeit fehlt beim Boot

```text
Current FERMENTING + alle Integritaetspruefungen PASS
  + Checkpoint-UTC vorhanden
  + aktuelle trusted UTC fehlt
  -> RecoveryEvaluation / TimePending
  -> ACTORS_OFF
  -> keine Restzeit, kein Resume, kein NoActiveRun-Tombstone
  -> asynchroner naechster Bewertungsversuch
```

Ein erneuter Versuch ist nur eine neue Bewertung derselben validierten
Evidenz. Er darf keine pauschalen Persistenzschreibvorgaenge pro Update oder
Sensorzyklus erzeugen.

### 6.2 Zeit ist vertrauenswuerdig verfuegbar

```text
Current FERMENTING + dieselbe Persistenzevidenz + trusted checkpoint UTC
  + trusted current UTC
  -> einfache Wall-Clock-Rechnung
  -> recovered elapsed < nominal: automatisch logisch FERMENTING
  -> recovered elapsed >= nominal: normale FermentationCompleted-Semantik
  -> keine automatische Aktorfreigabe
  -> bestehender FSM-/Command-/Persistenz-/Safety-Handoff
```

Die Umsetzung muss Zeitruecksprung, negative Differenz, Additionsoverflow,
unplausible Werte und konkurrierende neue Persistenzrevisionen fail-closed
behandeln. Eine aktuelle Zeit, die nur technisch lesbar aber nicht
vertrauenswuerdig ist, reicht nicht.

### 6.3 Nominale Dauer waehrend des Ausfalls erreicht

Die ermittelte Gesamtzeit verwendet die normale FSM-Semantik von
`FermentationCompleted`:

```text
CompletionMode::FinishWithoutCooling
    -> Completed

CompletionMode::CoolThenFinish
CompletionMode::CoolAndHoldForDuration
CompletionMode::CoolAndHoldUntilManualStop
    -> Cooling
```

Es gibt kein generisches `CompletionPending` nur wegen Power Loss, kein
automatisches Ueberspringen von `Cooling` und kein behauptetes
`CoolingTargetReached` waehrend des Ausfalls. `CoolingTargetReached` ist
sensor-/signalabhaengig. Ohne Ausfallmesswerte darf die Software weder den
Zeitpunkt des Kuehlzieles rekonstruieren noch Stromausfallzeit auf einen noch
nicht begonnenen `CoolHolding`-Timer anrechnen.

Nach dem Boot darf der logisch abgelaufene FERMENTING-Run gemaess dieser
normalen Semantik in `Completed` oder `Cooling` stehen. Jeder weitere
Kuehl-/Hold-Uebergang braucht die normale frische Evidenz. Die bestehende
Benutzerquittierung von `Completed` bleibt unveraendert und wird nicht durch
eine neue Power-Loss-Bestaetigung ersetzt.

### 6.4 Nachgelagerte Aktorfreigabe

Auch nach einer positiven Zeit-Recovery muessen Configuration Runtime, die
aktuelle Sensorqualitaet und Regelsensorauswahl, Hardware-/Commissioning-
Nachweis und Safety-/Planner-Gate frisch erfolgreich sein. Eine Recovery-
Disposition darf weder den #23/#24-Watchdog umgehen noch einen letzten
elektrischen Zustand anwenden.

## 7. Die sechs isolierten Edge Cases

### 7.1 Berechnete Zeit ueberschreitet die nominelle Dauer

```text
CURRENT_STATE=Die normale FSM besitzt FermentationCompleted und die CompletionModes FinishWithoutCooling, CoolThenFinish, CoolAndHoldForDuration und CoolAndHoldUntilManualStop. Der aktuelle Recoveryvertrag darf die sensorabhaengige CoolingTargetReached-Evidenz waehrend des Ausfalls nicht erfinden.
MINIMAL_R1_OPTION=Recovered elapsed auf die nominale Grenze begrenzen und die bestehende FermentationCompleted-Semantik auswerten: FinishWithoutCooling -> Completed; alle drei Cooling-Modi -> Cooling.
RECOMMENDATION=Normale FSM-Completion-Semantik verwenden. Kein generisches CompletionPending nur wegen Power Loss, kein automatisches Ueberspringen von Cooling und keine Anrechnung auf einen noch nicht beobachteten CoolHolding-Timer.
WHY=Der Stromausfall ist keine Benutzerentscheidung fuer einen vertrauenswuerdigen Current-FERMENTING-Run. Die normale CompletionMode-Auswertung trennt die berechenbare Fermentationszeit von der nicht rekonstruierbaren Kuehlziel-/Hold-Evidenz.
OWNER_DECISION_REQUIRED=NO
```

### 7.2 Direkte naechste Phase oder Entscheidung nach Boot

```text
CURRENT_STATE=Ein vollstaendig validierter Current-FERMENTING-Run ist die kanonische letzte Wahrheit. Die normale FSM definiert die fachliche CompletionMode-Auswertung; CoolingTargetReached bleibt sensor-/signalabhaengig.
MINIMAL_R1_OPTION=Unterhalb der nominalen Dauer den Kandidaten automatisch logisch als FERMENTING mit neuem aktuellem stateEnteredAtMillis fortsetzen. Ab nominaler Dauer die normale FermentationCompleted-Semantik anwenden; anschliessende Cooling-/Hold-Uebergaenge erst mit frischer Evidenz.
RECOMMENDATION=Kein Benutzer-Dialog allein wegen Stromausfall und kein automatisches Ueberspringen sensorabhaengiger Cooling-/Hold-Schritte. Die logische Current-Fortsetzung ist automatisch; Aktorfreigabe bleibt separat und fail-closed.
WHY=Die Ownerentscheidung trennt fachliche Zustandsfortsetzung von elektrischer Freigabe. So wird Power Loss nicht als Prozessende fehlinterpretiert, ohne Kuehlmesswerte zu erfinden oder Safety-Gates zu umgehen.
OWNER_DECISION_REQUIRED=NO
```

### 7.3 Kanonische absolute Zeitquelle und UTC-Anker

```text
CURRENT_STATE=Der app-neutrale ITimeSource-Port existiert; die produktive EspTimerTimeSource liefert aktuell nur monotone Zeit und fuer unixTimeSeconds() immer nullopt. Checkpointzeit und optionaler UTC-Wert existieren bereits in RunCheckpointTime/Envelope.
MINIMAL_R1_OPTION=ITimeSource als einzigen Application-Zeitport verwenden; die UTC von RunPersistenceRawRecord als exakt gebundenen Checkpoint-Anker und unixTimeSeconds() als aktuelle UTC verwenden. Die monotone Checkpointzeit bleibt bootlokale Evidenz und ersetzt keinen absoluten Anker.
RECOMMENDATION=Den bestehenden ITimeSource-/Record-Vertrag verbindlich wiederverwenden. Der Adapter darf nur dann einen UTC-Wert liefern, wenn sein Portvertrag ihn als verlaesslich zusichert; #124 implementiert keinen WLAN-/NTP-Producer.
WHY=Das erfuellt ADR-013 ohne Fermentationsvertrag in device_platform und vermeidet einen Schemaumbau, waehrend der exakte Record-Anker vor spaeteren globalen Zeitbeobachtungen geschuetzt wird.
OWNER_DECISION_REQUIRED=NO
```

### 7.4 Neuer kleiner TimePending-Status oder bestehender Vertrag

```text
CURRENT_STATE=ProcessState::RecoveryEvaluation existiert; RECOVERY_TIME_PENDING ist dokumentarisch als C2-Kontext vorhanden, aber es gibt noch keinen kleinen aktiven R1-Status fuer fehlende trusted UTC.
MINIMAL_R1_OPTION=RecoveryEvaluation als FSM-Zustand behalten und eine eng begrenzte typisierte app-seitige Disposition WaitingForTrustedTime/TimePending ergaenzen; keine neue FSM-Hauptphase, keinen neuen persistierten Prozesszustand und keine Recoveryengine.
RECOMMENDATION=WaitingForTrustedTime verbindlich als RAM-/Application-Disposition unter RecoveryEvaluation einfuehren. Bei spaeter trusted UTC dieselbe geladene revisionsgebundene Evidenz erneut bewerten; bei Revisionstausch fail-closed.
WHY=Die Disposition verhindert, dass fehlende Zeit als NoActiveRun oder als Resume-Behauptung erscheint, ohne die FSM oder device_platform mit Netzwerksemantik zu belasten.
OWNER_DECISION_REQUIRED=NO
```

### 7.5 Schema-3-/#18-Felder

```text
CURRENT_STATE=Die alten Pending-, Episode-, PriorElapsed-, RecoveryAdjustment-, Temperatur- und weightedProgress-Felder sind in Schema 3 lesbar und werden von Codec, Recoverycode und Tests verwendet; sie sind fuer den aktuellen R1-Pfad fachlich nicht kanonisch.
MINIMAL_R1_OPTION=Lesbarkeit, CRC-/Schema-/Referenzvalidierung und Rueckwaertskompatibilitaet behalten; alte Felder aktiv aus der R1-Zeitentscheidung ausschliessen. Nur Checkpoint-UTC, exakter persistierter Fortschritt und nominale Dauer in ihrer jeweiligen Rolle verwenden.
RECOMMENDATION=Keine sofortige Feld-/Schemaentfernung und keine automatische Migration. Alte Werte duerfen eine gueltige Current-Lage nicht allein wegen ihrer Anwesenheit verwerfen, aber Widerspruch oder Integritaetsfehler bleibt fail-closed.
WHY=So wird #18 nicht reaktiviert, der Wire-Vertrag bleibt stabil und die neue R1-Logik bleibt nachvollziehbar klein.
OWNER_DECISION_REQUIRED=NO
```

### 7.6 `OLDER_VALID_CHECKPOINT_RESUME` aus #90

```text
CURRENT_STATE=#90 liefert einen technisch validierten, aelteren Checkpoint als getrennte Fallback-/Recoveryklassifikation; er ist kein automatischer Produkt-Resume und die reale Callback-12-/Hardwarekampagne ist noch ein eigenes Gate.
MINIMAL_R1_OPTION=Fallback technisch getrennt halten. Keine automatische Promotion und keine automatische Wall-Clock-Rechnung auf einem nur als Fallback markierten Datensatz. Erst eine explizite fachliche Auswahl/Akzeptanz eines vollstaendig validierten Fallbacks darf denselben R1-Zeitvertrag anwenden; auch dann bleiben Actors-Off und Safety-Handoffs zwingend.
RECOMMENDATION=Das #90-Oracle unveraendert streng lassen: OLDER_VALID_CHECKPOINT_RESUME bleibt ein nicht aktivierendes Angebot. Eine explizit ausgewaehlte Fallback-Evidenz darf erst danach in denselben Zeitvertrag eintreten; ohne Auswahl oder UTC-Anker bleibt sie fail-closed/TimePending und wird nicht als NoActiveRun verkleidet.
WHY=#124 darf weder die NVS-/Callback-12-Orakel abschwaechen noch einen aelteren technisch validierten Record still zum Current befoerdern. Current-FERMENTING ist automatisch, Fallback ist wegen der ersetzten Current-Wahrheit explizit.
OWNER_DECISION_REQUIRED=NO
```

## 8. Spaetere rendererunabhaengige Projektion fuer #25

Issue #25 wird in diesem Auftrag weder implementiert noch geplant. Der
Recovery-Zielvertrag muss spaeter lediglich folgende semantische Informationen
rendererunabhaengig projizierbar machen:

- `Normal`;
- `WaitingForTrustedTime` unter `RecoveryEvaluation`;
- `CurrentRunRecovered` fuer automatisch logisch fortgesetztes Current-
  `FERMENTING`;
- `FallbackSelectionRequired` fuer ein bewusst auszuwaehlendes, niemals
  automatisch zu promotendes #90-Fallback-Angebot;
- `RecoveryRejectedOrFailClosed` mit strukturiertem Grund;
- `Completed`;
- `Cooling`;
- die fachlich notwendigen Gruende fuer fehlende Zeit, ungueltige Persistenz,
  widerspruechliche Evidenz sowie benoetigte frische Sensor-/Safety-Evidenz;
- keine Roh-GPIO-, Renderer-, LVGL-, Web-, Layout- oder Textentscheidung.

Diese Liste ist nur die spaetere semantische Projektionsgrenze. Nach
der Recoveryimplementation kann #25 gegen diesen freigegebenen Vertrag geplant
werden. Ein normal vertrauenswuerdig wiederhergestellter Current-
`FERMENTING`-Run benoetigt keinen bestaetigenden UI-Dialog allein wegen des
Stromausfalls.

## 9. Umsetzungsslices nach Planfreigabe

Die folgenden Slices bilden eine zusammenhaengende Umsetzung. Die
Ownerentscheidungen sind verbindlich abgeschlossen; ein neuer materieller
Produktentscheid darf nicht still erfunden werden.

### Slice A – Exakte Zeitbasis und Recovery-Disposition

- obige schemafreie Formel mit `KnownTotal`-, Overflow-, Timestamp- und
  Revisionsgates implementierbar machen;
- `WaitingForTrustedTime` als app-seitige typisierte Disposition unter
  `ProcessState::RecoveryEvaluation` festlegen;
- keine Persistence-Mutation nur durch das Warten auf Zeit;
- native Tabellen-/Propertytests fuer genaue Rechnung, Zeitfehler und
  fail-closed Statusabgrenzung.

### Slice B – Current FERMENTING Recovery

- Current-Pfad vollstaendig technisch und fachlich validieren;
- trusted UTC -> `recovered_elapsed_seconds` berechnen;
- unter nominaler Dauer automatisch logisch als `FERMENTING` fortsetzen;
- Write-before-Apply einhalten;
- Aktoren bis zu den frischen Safety-Gates aus lassen;
- Mehrfach-Reboot ohne Doppelzaehlung nachweisen.

### Slice C – Dauergrenze und normale FSM

- bei erreichter Dauer die normale `FermentationCompleted`-Semantik verwenden;
- `FinishWithoutCooling` -> `Completed`;
- `CoolThenFinish`, `CoolAndHoldForDuration` und
  `CoolAndHoldUntilManualStop` -> `Cooling`;
- kein Ueberspringen sensorabhaengiger Cooling-/Hold-Grenzen;
- keine Anrechnung auf einen noch nicht begonnenen `CoolHolding`-Timer.

### Slice D – TimeSource und Composition

- bestehenden `ITimeSource`-Port in den R1-Recoverypfad injizieren;
- ESP-IDF-Adaptergrenze klein halten;
- kein WLAN-/Provisioning-Scope und kein Connectivity Manager;
- `VirtualTimeSource` absent -> trusted transition testen;
- keine blockierende Bootwartephase.

### Slice E – #90- und Legacy-Abgrenzung

- Fallback nicht automatisch promoten oder aktivieren;
- weighted-/C2-Pfad aus dem aktiven R1-Aufrufgraphen ausschliessen;
- keine breite Cleanup-Arbeit und keine Entfernung aller C2-Dateien als Ziel;
- bestehende #90-Orakel unveraendert streng halten;
- keine neue zweite Recovery- oder Persistenzkomponente.

### Slice F – Normative Source-of-Truth-Nachfuehrung

Erst nach erfolgreicher Review der Implementierung werden mindestens
`docs/RECOVERY_AND_INTERRUPTION.md`, `docs/RUN_PERSISTENCE.md`,
`docs/STATE_MACHINE.md` und `docs/SYSTEM_SAFETY_AND_RECOVERY.md` auf denselben
R1-Vertrag gebracht. Betroffene Anforderungen und Akzeptanztests werden nur
gezielt aktualisiert; historische #18/#24-Provenienz bleibt verlinkt. Dieser
Plan-PR fuehrt diese fachliche Dokumentkorrektur nicht vorweg.

## 10. Tests und Nachweise je Slice

Die folgenden Nachweise sind fuer die spaetere Umsetzung vorgesehen, nicht
Bestandteil dieses Plan-PRs:

| Slice | Gezielte Tests/Nachweise |
|---|---|
| A | Native Recovery-Disposition- und Boot-Klassifikationsmatrix; `KnownTotal`-/Timestamp-/CRC-/Referenz-/Revision-/Overflow-Grenzen; `WaitingForTrustedTime` ist nicht `NoActiveRun` und nicht `ResumeOffer` |
| B | `test_run_persistence_coordinator`, Checkpoint- und Snapshot-/Codec-Tests: exakt `observedRunSeconds + live segment`, Record-UTC-Anker, Write-before-Apply und automatische logische `FERMENTING`-Fortsetzung |
| C | `test_process_state_machine`: `FermentationCompleted`-Semantik fuer alle vier CompletionModes; `FinishWithoutCooling -> Completed`, drei Cooling-Modi -> `Cooling`; kein CoolingTargetReached ohne Sensorbeobachtung und kein Hold-Timer-Kredit |
| D | `test_time_source`, VirtualTimeSource absent -> trusted transition, Application-/Composition-/Dependency-Guard; `ITimeSource`-Injektion ohne blockierenden Boot-Wait und ohne WLAN-/Provisioning-Implementierung |
| E | `test_issue90_product_recovery_oracle`; Fallback nie automatisch Current/Aktorfreigabe, explizite Auswahlgrenze; `test_run_recovery_time`/`test_run_progress_weighting` bleiben als Legacy-Negativtests oder werden nur bei realer aktiver Kopplung entfernt |
| B/E | Mehrfach-Reboot-/Mehrfach-Recovery-Szenarien: Checkpoint A, weiterer Live-Abschnitt, Checkpoint B, zweiter Ausfall; keine Doppelzaehlung; alte Weighted-/Temperaturfelder beeinflussen `recovered_elapsed_seconds` nie |
| F | Markdown-/Link-/Source-of-Truth-Checks sowie Review des vollstaendigen aktuellen Diffs; danach gezielte geaenderte Bereichstests |

### 10.1 Konkrete Testfaelle

Mindestens folgende nativen Tests und Property-/Tabellenfaelle sind
vorzusehen:

```text
CURRENT_FERMENTING_EXACT_TIME:
  checkpoint persisted elapsed = 20 min
  outage = 10 min
  -> recovered elapsed = 30 min
  -> logical FERMENTING, sofern nominale Dauer nicht erreicht

MULTI_REBOOT_NO_DOUBLE_COUNTING:
  recovery A -> weiterer Live-Abschnitt -> checkpoint B -> zweiter Ausfall
  -> jeder Abschnitt genau einmal in observedRunSeconds

DURATION_COMPLETION:
  FinishWithoutCooling -> Completed
  CoolThenFinish -> Cooling
  CoolAndHoldForDuration -> Cooling
  CoolAndHoldUntilManualStop -> Cooling

NO_INFERRED_COOLING:
  keine Sensor-/Signal-Evidenz waehrend des Ausfalls
  -> CoolingTargetReached nie behaupten
  -> Ausfallzeit nie auf noch nicht begonnenen CoolHolding-Timer anrechnen

TIME_PENDING:
  trusted checkpoint + keine aktuelle UTC
  -> RecoveryEvaluation / WaitingForTrustedTime
  -> actors off, kein Tombstone, kein Resume-Claim, keine Persistence-Mutation
  spaetere trusted UTC
  -> dieselbe revisionsgebundene Evidenz erneut bewerten

TIME_FAILURES:
  current UTC < checkpoint UTC -> fail closed
  arithmetic overflow -> fail closed
  missing checkpoint UTC -> fail closed
  PartialUnknownHistory -> no exact R1 resume
  neue Persistenzrevision waehrend Pending -> stale evidence / fail closed

TIME_SOURCE:
  ITimeSource::unixTimeSeconds() == nullopt -> nicht trusted/verfuegbar
  Wert vorhanden -> trusted absolute UTC nur gemaess Portvertrag

FALLBACK:
  FallbackRecovered / OLDER_VALID_CHECKPOINT_RESUME
  -> nie automatische Current-Promotion oder Aktorfreigabe
  -> erst explizite Auswahl/Akzeptanz darf Zeitrechnung verwenden

LEGACY:
  weightedProgress oder Temperatur-Evidenz vorhanden
  -> nie R1-Ausfallgutschrift oder recovered elapsed beeinflussen
```

In diesem Plan-Gate werden keine Firmware-, ESP-IDF-, Hardware- oder
vollstaendigen Testlaeufe ausgefuehrt. `NOT_RUN` ist kein bestandenes Ergebnis.
Ein spaeterer physischer Nachweis bleibt strikt getrennt von Software-/Host-
und CI-Nachweisen; historische Boardlogs gelten nicht als aktuelle
Hardwareakzeptanz.

## 11. Akzeptanzkriterien fuer die spaetere Umsetzung

1. Der dokumentierte R1-Vertrag sagt ausdruecklich
   `REPLACES_R1_NO_FERMENTING_RESUME_POLICY=YES`, ohne #18 oder #24 zu
   reaktivieren.
2. Ein vollstaendig validierter `FERMENTING`-Checkpoint mit vertrauenswuerdigem
   UTC-Anker und aktueller UTC verwendet exakt
   `observedRunSeconds + live segment + outage`; Temperatur-/Biologie-/Weighting-
   Felder beeinflussen die Entscheidung nicht.
3. Liegt `recovered_elapsed_seconds` unter der nominalen Dauer, wird der
   Current-Run ohne Benutzerbestaetigung logisch automatisch als `FERMENTING`
   fortgesetzt; `stateEnteredAtMillis` wird auf die aktuelle monotone Zeit
   gesetzt und spaetere Live-Zeit wird nicht doppelt gezaehlt.
4. Bei erreichter nominaler Dauer gilt die normale FSM-Semantik:
   `FinishWithoutCooling -> Completed`, alle drei Cooling-Modi -> `Cooling`.
   CoolingTargetReached und ein noch nicht begonnener CoolHolding-Timer werden
   waehrend des Ausfalls nie inferiert.
5. Fehlt trusted UTC unmittelbar nach Boot, bleibt derselbe valide Run in
   `RecoveryEvaluation/WaitingForTrustedTime`; es gibt keine Persistence-
   Mutation nur wegen des Wartens, keinen Tombstone und keinen Resume-Claim.
6. WLAN/NTP laeuft asynchron; #124 implementiert kein WLAN-Onboarding,
   Credentialmanagement, Connectivity UI oder einen neuen NTP-Service in
   `device_platform`. `ITimeSource` bleibt der einzige Zeitport.
7. Persistenz-, Zeitanker-, Run-, Schema-, CRC-, Epoch-, Referenz-,
   Revisions- und Rechenunsicherheit bleibt `NO_GUESS`,
   `NO_AUTOMATIC_RESUME`, `FAIL_CLOSED` und wird nicht umetikettiert.
8. Nach jedem Boot sowie bei Pending, Reject, Fehler und unbekannter Lage sind
   Aktoren aus; keine elektrischen Zustandsreste werden restauriert.
9. Checkpointgrenzen und unmittelbare Eventpersistenz bleiben unveraendert;
   Sensorzyklen erzeugen keine neuen pauschalen Writes.
10. Der bestehende einzelne `RunPersistenceCoordinator` sowie die #121-
   Composition bleiben Source of Truth; es gibt keinen zweiten Coordinator,
   keinen monolithischen SafetyCore und keinen App-Typ in `device_platform`.
11. Schema-3-/#18-Felder bleiben, soweit erforderlich, kompatibel lesbar,
   erzeugen aber keine temperaturgewichtete R1-Entscheidung.
12. `OLDER_VALID_CHECKPOINT_RESUME` aus #90 bleibt ein getrenntes, nicht
   automatisch aktivierendes Fallback-Angebot; Callback-12- und
   NVS-Orakel werden nicht abgeschwaecht; nur explizite Fallback-Auswahl kann
   in denselben Zeitvertrag eintreten.
13. Mehrfach-Reboot und Mehrfach-Recovery zaehlen keinen bereits angerechneten
   Live-Abschnitt doppelt.
14. Die sechs Edge Cases sind gemaess den verbindlichen Ownerentscheidungen
   umgesetzt und in gezielten nativen Tests nachgewiesen.
15. Die semantischen Recoveryzustaende sind spaeter fuer #25 projizierbar,
   ohne UI-/Renderer-/Layoutentscheidungen in Issue #124 einzufuehren.

## 12. Source-of-Truth-Updates

### In dieser Planrunde

- neues Issue #124 angelegt;
- neuer versionierter Plan unter diesem Pfad;
- `docs/ROADMAP.md` minimal aktualisiert: #124 steht vor #25 und die
  Priorisierungsrichtung verweist darauf;
- Draft-PR und genau ein aktueller `SESSION HANDOVER` werden mit Plan-Commit
  und SHA synchronisiert;
- keine normative Recoverydokumentation, kein Produktionscode, kein Testcode,
  kein Schema.

### Nach Owner-Freigabe und Umsetzung

- normative Recovery-, Persistenz-, FSM- und Safety-Dokumente auf einen
  widerspruchsfreien R1-Stand bringen;
- relevante Anforderungen/Akzeptanztests und ADR-Register nur bei echter
  neuer Architekturentscheidung aktualisieren;
- #25 erst gegen die freigegebene semantische Recoveryoberflaeche planen;
- Roadmap auf den dann live verifizierten Status und die exakten SHAs bringen.

## 13. Risiken und Gegenmassnahmen

| Risiko | Gegenmassnahme/Abbruchsignal |
|---|---|
| Systemzeit ist vorhanden, aber nicht vertrauenswuerdig oder springt zurueck | Qualitaets-/Monotoniepruefung; Pending oder fail-closed, niemals raten |
| Persistierter UTC-Anker ist nicht exakt mit dem Fortschrittswert gekoppelt | Slice A stoppt vor Implementation; keine neue implizite Semantik |
| NTP kommt nie | Dauerhaft all-off/TimePending oder fail-closed; kein Bootblock und kein Resumeversprechen |
| Alte Schema-3-Felder werden indirekt wieder fachlich wirksam | explizite Negativtests und Entfernen der aktiven C2-Aufrufkette |
| Zeit ueberschreitet nominelle Dauer | normale FermentationCompleted-Semantik; FinishWithoutCooling -> Completed, Cooling-Modi -> Cooling; keine stille Phase |
| Fallback wird zum stillen Current promoted | #90-Oracle und explizite Auswahlgrenze unveraendert lassen |
| Zeitport oder NTP-Integration leakt Fermentationslogik in Plattform | Architektur-Guard; sofortiger Abbruch und Owner-Richtung |
| Aktorfreigabe wird aus Recoveryberechnung abgeleitet | All-off-/Safety-Handoff-Test; Diff reviewen und Slice abbrechen |
| Testmigration entfernt historische Orakel ohne Ersatz | erst nach gleicher oder staerkerer Negativabdeckung entfernen |

## 14. Rollback- und Abbruchgrenzen

- Vor Owner-Freigabe wird dieser Plan nicht umgesetzt. Der Draft bleibt
  Draft; kein Merge, kein Ready-for-review, kein Auto-Merge und kein
  Branchloeschen.
- Die spaetere Implementation stoppt, wenn ein vorhandenes Persistenzfeld die
  geforderte Semantik nicht traegt und dafuer ein Schemaumbau erforderlich
  waere, wenn ein neuer materieller Produktentscheid sichtbar wird, wenn ein
  zweiter Coordinator erforderlich erschiene oder wenn Aktor-/Hardwarevertraege
  erweitert werden muessten.
- Bei einem unklaren Write-Ausgang, technischer Indeterminiertheit,
  untrusted Zeit oder nicht reproduzierbarem Build-/Hostverhalten wird nicht
  auf Erfolg umetikettiert. Es gilt der bestehende fail-closed-Abbruch.
- Ein spaeterer Rollback erfolgt nur durch einen Owner freigegebenen
  Commit-/PR-Rollback auf dem aktuellen Branch ohne Force-Push. Alte #18-/#24-
  Historie wird nicht wieder geoeffnet; die normative Dokumentation darf nach
  einem Rollback nicht zwei aktive Wahrheiten enthalten.
- Physische Hardwaretests, NVS-Power-Cuts und reale Aktorfreigaben sind keine
  stillschweigende Folge dieses Plan-PRs und bleiben ihre eigenen Owner-Gates.

## 15. Abgeschlossene Ownerentscheidungen und Abschlussgate

```text
OWNER_DECISIONS_REQUIRED=NONE
POWER_LOSS_ALONE_REQUIRES_USER_CONFIRMATION=NO
TRUSTED_CURRENT_FERMENTING_LOGICAL_RECOVERY=AUTOMATIC
FERMENTATION_DURATION_REACHED_DURING_OUTAGE=USE_NORMAL_FSM_COMPLETION_SEMANTICS
TIME_PENDING=WaitingForTrustedTime under RecoveryEvaluation
OLDER_VALID_CHECKPOINT_AUTO_PROMOTION=NO
OLDER_VALID_CHECKPOINT_AUTO_ACTIVATION=NO
SECOND_RECOVERY_COORDINATOR_PLANNED=NO
SECOND_PERSISTENCE_COORDINATOR_PLANNED=NO
```

Damit sind die vier bisherigen Edge-Case-Entscheidungen geschlossen:
`elapsed >= nominal` nutzt die normale FSM-Completion-Semantik; ein normaler
Current-FERMENTING-Run wird automatisch logisch fortgesetzt; `TimePending`
ist eine typed Application-Disposition unter `RecoveryEvaluation`; und ein
#90-Fallback wird nie automatisch promoted oder aktiviert, sondern benoetigt
explizite Auswahl/Akzeptanz. Die bestehende Wahl des app-neutralen
`ITimeSource`-Ports, die Schema-3-Lesbarkeit und der enge #124-Network-Scope
sind damit ebenfalls festgelegt.

Wenn das erneute Live-Audit einen neuen materiellen Produktentscheid sichtbar
macht, wird dieser nicht still erfunden; der Plan stoppt mit einem klaren
Owner-Gate. Der aktuelle Auftrag erzeugt jedoch keine offene
Ownerentscheidung.

```text
ISSUE25_IMPLEMENTATION=NOT_STARTED
IMPLEMENTATION=NOT_STARTED
MERGE=NO
OWNER_PLAN_REVIEW_REQUIRED=YES
STOP=Owner Review der vollstaendigen korrigierten Planfassung
```
