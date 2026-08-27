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

Ein `FERMENTING`-Checkpoint darf nur dann als Zeit-Recovery bewertet werden,
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

Dann wird die reale Wall-Clock-Zeit als reine Zeitrechnung angerechnet:

```text
elapsed_after_boot
  = persisted_elapsed_at_checkpoint
  + trusted_current_time
  - trusted_checkpoint_absolute_time
```

Eine fachlich aequivalente Darstellung ist zulaessig, wenn sie die
kanonischen vorhandenen Felder korrekt nutzt. Die Formel ist keine
Temperatur-, Biologie- oder Zustandsrekonstruktion.

### 3.3 Kein biologisches Modell

R1 verwendet keine temperaturgewichtete Ausfallrekonstruktion, keine
geschaetzte Temperaturkurve, keine biologische Gewichtungsfunktion, keine
modellbasierte Gutschrift und keine Approximation aus Vor-/Nach-
Ausfalltemperaturen. `weightedProgress`, `nominalRecoveryAdjustment`,
`PendingRecoveryAnchor`, Recovery-Episode- und Temperatur-Evidenz erzeugen
keine R1-Entscheidung. Sie duerfen nur aus Kompatibilitaetsgruenden lesbar
bleiben, sofern ihre Integritaetsvalidierung das zulaesst.

### 3.4 Boot, WLAN/NTP und TimePending

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
   kein Resume behauptet. WLAN/NTP darf parallel arbeiten; Boot wartet weder
   synchron noch in einer Endlosschleife.
6. Sobald dieselbe app-neutrale Zeitquelle vertrauenswuerdige absolute Zeit
   liefert, wird genau dieselbe validierte Persistenzevidenz erneut bewertet.
   Die Bewertung darf nicht durch eine inzwischen neu erfundene oder
   unvollstaendig validierte Momentaufnahme ersetzt werden.
7. Eine gueltige Zeitrechnung fuehrt zunaechst nur zu einer Recovery-
   Disposition. Runtime/FSM, frische Config-/Sensor-/Hardware-/Safety-
   Evidenz und der bestehende Command-/Transition-Write-before-Apply-Pfad
   bleiben vor jeder eventuellen Freigabe.

`TimePending` ist dabei kein neuer allgemeiner Recovery-Engine-Zustand und
kein synchroner Netzwerkvertrag. Falls ein bestehender
`RecoveryEvaluation`-/Disposition-Vertrag die Unterscheidung ohne
semantische Ueberladung tragen kann, ist dieser zu erweitern statt eine neue
FSM-Hauptphase einzufuehren.

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

## 4. Persistierte Felder, Wiederverwendung und Legacygrenze

### 4.1 Kein Schema- oder Wire-Break

```text
PERSISTENCE_SCHEMA_CHANGE_PLANNED=NO
```

Die Umsetzung darf keine neue Schema-Version, keinen neuen Pflicht-Key und
keinen unnoetigen Migrationsschnitt einfuehren. Vor dem ersten Codecommit
der Umsetzung ist durch Tests und Codepfadnachweis zu bestaetigen, welches
bestehende Feld exakt `persisted_elapsed_at_checkpoint` repraesentiert.

Als Kandidaten sind nur bereits kanonische Felder zulaessig:

- `RunCheckpointTime::utcUnixSeconds` beziehungsweise der bestehende
  persistierte UTC-Wert des Checkpoint-Envelopes als absoluter Anker;
- `RunPersistenceSnapshot::checkpointMonotonicMillis` nur als bestehender
  Checkpoint-Zeitanteil innerhalb eines Boots, nicht als bootuebergreifende
  Ausfallzeit;
- die bereits persistierte Run-Fortschritts-/Beobachtungszeit, insbesondere
  `runProgress.observedRunSeconds`, sofern der aktuelle Producer nachweislich
  den fachlich erforderlichen Checkpointwert schreibt;
- die bestehende `nominalDurationSeconds` nur als Zielgrenze, nie als
  Ausfallzeit.

Falls kein vorhandenes Feld den exakten Checkpoint-Fortschritt traegt, stoppt
die Umsetzung vor einer Schemaerfindung und legt eine neue Ownerentscheidung
vor. Ein neues Feld ist in diesem Plan nicht freigegeben.

### 4.2 Alte Schema-3-/#18-Felder

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
            -> TimePending, solange trusted UTC fehlt
            -> Wall-Clock-Kandidat bei trusted UTC
       -> bestehende Process-FSM fuer explizite Runtime-Entscheidung
       -> frische Sensor-/Hardware-/Safety-Evidenz
       -> bestehende Aktor-/Planner-Grenze; Boot bleibt all-off
```

Die Fachlogik verwendet nur `device_platform::ITimeSource`. Ein
ESP-IDF-Adapter darf die absolute Zeit aus dem bereits vorgesehenen
System-/NTP-Zustand liefern, darf aber keine Fermentationsbegriffe in
`device_platform` einfuehren. `main/app_main.cpp` bleibt fuer Erstellung,
Verdrahtung und Lebensdauer der Quelle verantwortlich. WLAN/NTP wird nicht
zum synchronen Application-Bootvertrag.

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
  -> Recovery-Disposition mit berechnetem Fortschritt
  -> keine automatische Aktorfreigabe
  -> explizite fachliche Entscheidung ueber Resume/Completion
  -> bestehender FSM-/Command-/Persistenz-/Safety-Handoff
```

Die Umsetzung muss Zeitruecksprung, negative Differenz, Additionsoverflow,
unplausible Werte und konkurrierende neue Persistenzrevisionen fail-closed
behandeln. Eine aktuelle Zeit, die nur technisch lesbar aber nicht
vertrauenswuerdig ist, reicht nicht.

### 6.3 Nachgelagerte Aktorfreigabe

Auch nach einer positiven Zeit-Recovery muessen Configuration Runtime, die
aktuelle Sensorqualitaet und Regelsensorauswahl, Hardware-/Commissioning-
Nachweis und Safety-/Planner-Gate frisch erfolgreich sein. Eine Recovery-
Disposition darf weder den #23/#24-Watchdog umgehen noch einen letzten
elektrischen Zustand anwenden.

## 7. Die sechs isolierten Edge Cases

### 7.1 Berechnete Zeit ueberschreitet die nominelle Dauer

```text
CURRENT_STATE=Die vorhandene Zeitrechnung kann elapsed_after_boot groesser oder gleich nominalDurationSeconds ergeben; ein verbindlicher R1-Uebergang aus FERMENTING ist aktuell nicht definiert.
MINIMAL_R1_OPTION=Berechnung saturiert beziehungsweise markiert die nominale Grenze und erzeugt eine explizite Completion-Pending-/Resume-Disposition ohne Aktorfreigabe.
RECOMMENDATION=Nicht still weiterrechnen und nicht automatisch in die naechste Phase wechseln; den Zustand als fachlich berechnetes Ende mit ausstehender expliziter Entscheidung exponieren.
WHY=Die Wall-Clock-Rechnung ist vertrauenswuerdig, beantwortet aber nicht die fachliche und Safety-Frage, ob ein Prozessabschluss, ein kontrollierter naechster Phasenuebergang oder eine Benutzeraktion erforderlich ist. Negative/ueberlaufende Darstellungen muessen ausgeschlossen werden.
OWNER_DECISION_REQUIRED=YES
```

### 7.2 Direkte naechste Phase oder Entscheidung nach Boot

```text
CURRENT_STATE=Die FSM kennt RecoveryEvaluation und explizite Recovery-Events, aber der aktuelle R1-Vertrag erlaubt keinen automatischen FERMENTING-Resume und definiert keinen automatischen Abschluss nach Wall-Clock-Ueberschreitung.
MINIMAL_R1_OPTION=RecoveryEvaluation beibehalten; Resume beziehungsweise Completion nur als explizite, bestaetigbare fachliche Entscheidung durch den bestehenden Command-/Transition-Pfad anbieten.
RECOMMENDATION=Kein direkter automatischer Phasenwechsel. Erst nach expliziter Entscheidung, vollstaendiger Runtime-/FSM-Validierung und frischer Safety-Evidenz darf ein fachlicher Uebergang committed werden.
WHY=Power-Loss-Recovery darf die Aktor- und Safety-Grenze nicht implizit ueberfahren. So bleiben Berechnung, Disposition und physische Freigabe getrennt.
OWNER_DECISION_REQUIRED=YES
```

### 7.3 Kanonische absolute Zeitquelle und UTC-Anker

```text
CURRENT_STATE=Der app-neutrale ITimeSource-Port existiert; die produktive EspTimerTimeSource liefert aktuell nur monotone Zeit und fuer unixTimeSeconds() immer nullopt. Checkpointzeit und optionaler UTC-Wert existieren bereits in RunCheckpointTime/Envelope.
MINIMAL_R1_OPTION=ITimeSource als einziger Application-Zeitport verwenden; den vorhandenen persistierten Checkpoint-UTC-Wert als Anker und die bestehende Runtime-/NTP-UTC als aktuelle Zeit verwenden. Die monotone Checkpointzeit bleibt bootlokale Evidenz und ersetzt keinen absoluten Anker.
RECOMMENDATION=Diese bestehenden Felder und den bestehenden Port als kanonisch festlegen; vor Umsetzung durch Codec-/Producer-Tests beweisen, dass der UTC-Wert mit genau dem Checkpoint und dem persistierten Fortschritt verknuepft ist. NTP liefert nur die Quelle, nicht die Fachentscheidung.
WHY=Das erfuellt ADR-013 ohne Fermentationsvertrag in device_platform und vermeidet einen Schemaumbau. Ein neuer Zeitanker oder eine zweite Zeitabstraktion wuerde die Vertrauensgrenze unnoetig vervielfachen.
OWNER_DECISION_REQUIRED=NO, sofern der Feldnachweis den bestehenden UTC-/Fortschrittsvertrag bestaetigt; andernfalls vor Implementierung YES.
```

### 7.4 Neuer kleiner TimePending-Status oder bestehender Vertrag

```text
CURRENT_STATE=ProcessState::RecoveryEvaluation existiert; RECOVERY_TIME_PENDING ist dokumentarisch als C2-Kontext vorhanden, aber es gibt noch keinen kleinen aktiven R1-Status fuer fehlende trusted UTC.
MINIMAL_R1_OPTION=RecoveryEvaluation als FSM-Zustand behalten und eine eng begrenzte typisierte Recovery-Disposition beziehungsweise Statusinformation WaitingForTrustedTime/TimePending ergaenzen; keine neue FSM-Hauptphase und keine Recoveryengine.
RECOMMENDATION=Ein expliziter kleiner Status ist noetig, wenn der bestehende RecoveryEvaluation-Vertrag die Unterscheidung nicht verlustfrei tragen kann. Er muss app-seitig bleiben und darf weder NoActiveRun noch ResumeOffer semantisch ueberladen.
WHY=Ohne unterscheidbare Pending-Semantik wuerde fehlende Zeit erneut falsch als NoActiveRun oder als behauptetes Resume erscheinen. Ein voller Coordinator oder Netzwerkstatusautomat ist dafuer nicht erforderlich.
OWNER_DECISION_REQUIRED=YES
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
MINIMAL_R1_OPTION=Fallback technisch getrennt halten. Keine automatische Promotion und keine automatische Wall-Clock-Rechnung auf einem nur als Fallback markierten Datensatz. Erst eine explizite fachliche Auswahl eines vollstaendig validierten Fallbacks darf denselben R1-Zeitvertrag anwenden; auch dann bleiben Actors-Off und Safety-Handoffs zwingend.
RECOMMENDATION=Das #90-Oracle unveraendert streng lassen und OLDER_VALID_CHECKPOINT_RESUME als nicht aktivierendes Angebot behandeln. Ohne explizite Auswahl oder ohne vertrauenswuerdigen UTC-Anker bleibt die Lage fail-closed/TimePending und wird nicht als NoActiveRun verkleidet.
WHY=#124 darf weder die NVS-/Callback-12-Orakel abschwaechen noch einen aelteren technisch validierten Record still zum aktuellen Run befoerdern. Die einfache Zeitrechnung ist nur fuer eine eindeutig gewaehlte und voll validierte Evidenz zulaessig.
OWNER_DECISION_REQUIRED=YES fuer die ausdrueckliche Zulassung eines explizit gewaehlten Fallbacks als Zeitrechnungsbasis; die Nicht-Promotion ohne diese Auswahl ist verbindlich.
```

## 8. Spaetere rendererunabhaengige Projektion fuer #25

Issue #25 wird in diesem Auftrag weder implementiert noch geplant. Der
Recovery-Zielvertrag muss spaeter lediglich folgende semantische Informationen
rendererunabhaengig projizierbar machen:

- normaler Prozess-/Recoverystatus;
- `RecoveryEvaluation` als fachliche Bewertung;
- `WaitingForTrustedTime`/`TimePending` mit unverfaenglichem Statusgrund;
- berechnete Recovery-/Resume-Verfuegbarkeit ohne Aktorfreigabe;
- Completion-Pending, falls die Ownerentscheidung diesen Fall bestaetigt;
- explizit abgelehntes oder fail-closed Recovery-Ergebnis;
- strukturierter Grund/Status fuer fehlende Zeit, ungueltige Persistenz,
  widerspruechliche Evidenz oder erforderliche Benutzer-/Safety-Entscheidung;
- keine Roh-GPIO-, Renderer-, LVGL-, Web-, Layout- oder Textentscheidung.

Diese Liste ist nur die spaetere semantische Projektionsgrenze. Nach
Ownerfreigabe wird entschieden, ob zuerst die Recoveryimplementation folgt
oder #25 gegen den freigegebenen Vertrag geplant werden kann.

## 9. Umsetzungsslices nach Planfreigabe

Die folgenden Slices bilden eine zusammenhaengende Umsetzung. Kein Slice darf
die Ownerentscheidungen aus Abschnitt 7 still ersetzen.

### Slice A – Kanonische Felder und Disposition festschreiben

- aktuellen Producer-/Codec-Pfad fuer Checkpoint-UTC und den exakten
  persistierten Fortschritt nachweisen;
- kleine app-seitige Recovery-Disposition fuer `TimePending`, trusted
  Wall-Clock-Ergebnis und fail-closed Gruende festlegen;
- `ProcessState::RecoveryEvaluation` nicht durch eine neue Hauptphase ersetzen;
- keine Schema-/Wire-Aenderung;
- Tests fuer fehlende/ungueltige/zeitlich widerspruechliche Evidenz sowie
  Statusabgrenzung zu `NoActiveRun`, `ResumeOffer` und `SAFE_BOOT`.

### Slice B – Bestehenden Zeitport bis zur Composition Root verdrahten

- `ITimeSource` nur dort injizieren, wo die Application Recovery bewertet;
- `main/app_main.cpp` als einziger Composition Root fuer die konkrete Quelle;
- ESP-IDF-Adapter so anbinden, dass `unixTimeSeconds()` nur bei
  vertrauenswuerdiger aktueller UTC einen Wert liefert;
- keine synchrone WLAN-/NTP-Wartephase und kein NTP-Vertrag in
  `device_platform`;
- native Tests mit `VirtualTimeSource` fuer absent/present/regressed UTC und
  monotone Zeit;
- konkrete ESP-IDF-/NTP-Verifikation als eigener Build-/Host-/Hardware-
  Nachweis gemaess `docs/CI_AND_QUALITY_GATES.md`, nicht als simulierte
  Hardwareabnahme ausgeben.

### Slice C – Einfache R1-Bewertung im bestehenden Persistenzpfad

- `Current FERMENTING` nur nach kompletter bestehender Integritaets- und
  Referenzvalidierung bewerten;
- ohne trusted UTC in `RecoveryEvaluation/TimePending` halten;
- mit trusted UTC die einfache Wall-Clock-Rechnung ausfuehren;
- negative Differenzen, Overflow, fehlende Anker, neue Revisionen und
  widerspruechliche Evidenz fail-closed behandeln;
- Checkpointplan und unmittelbare Event-Writes unveraendert lassen;
- keinen C2-Weighted-/Temperature-/Episode-Pfad aufrufen;
- `RunPersistenceCoordinator` bleibt einziger Persistenzowner.

### Slice D – FSM-, Runtime- und Actuation-Handoff

- Recovery-Disposition ueber den bestehenden expliziten FSM-/Command-Pfad
  weiterfuehren;
- die Ownerentscheidung fuer nominale Ueberschreitung implementieren, erst
  nach deren Freigabe;
- `Applied` und bestehende Write-before-Apply-Semantik als alleinige RAM-/FSM-
  Handoff-Grenze beibehalten;
- frische Config-, Sensor-, Hardware- und Safety-Evidenz erzwingen;
- `ACTORS_OFF` bei Boot, Pending, Reject, Fehler und unbekannter Lage
  nachweisen; niemals GPIO-Zustaende restaurieren.

### Slice E – Legacy- und #90-Abgrenzung

- historische `RunRecoveryCoordinator`-/Weighting-Aufrufe aus dem aktiven
  Pfad und dem Orchestrator entfernen oder stilllegen;
- Codec-/Kompatibilitaetstests fuer lesbare Schema-3-Felder behalten;
- alte Felder aktiv als nicht R1-kanonisch testen;
- `OLDER_VALID_CHECKPOINT_RESUME` und Callback-12-Orakel unveraendert streng
  halten; keine automatische Fallback-Promotion;
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
| A | Native Recovery-Disposition- und Boot-Klassifikationsmatrix; Current/Schema/CRC/Referenz- und Unknown-Grenzen; `TimePending` ist nicht `NoActiveRun` und nicht `ResumeOffer` |
| B | `test_time_source`; native Virtual-Time-Tests; Composition-/Dependency-Guard; ESP-IDF-Adapter-/NTP-Hostnachweis nur mit verfuegbarer Quelle, kein synchroner Boot-Wait |
| C | `test_run_persistence_coordinator`, Checkpoint- und Snapshot-/Codec-Tests: Formel, exakter UTC-Anker, Feldwiederverwendung, alte Felder ohne Gewichtung, kein Write je Sensorzyklus, 1/5/60-Minuten-Grenzen, unknown write outcome |
| D | `test_process_state_machine`, bestehende Orchestrator-/Actuation-Tests: RecoveryEvaluation, Pending, explizite Entscheidung, nominale Ueberschreitung gemaess Ownerentscheidung, all-off und kein GPIO-Restore |
| E | `test_run_recovery_time`/`test_run_progress_weighting` als Legacy-Negativtests oder gezielte Entfernung ihrer aktiven Produktionskopplung; `test_issue90_product_recovery_oracle`; Fallback bleibt nicht aktivierend |
| F | Markdown-/Link-/Source-of-Truth-Checks sowie Review des vollstaendigen aktuellen Diffs; danach gezielte geaenderte Bereichstests |

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
   UTC-Anker und aktueller UTC rechnet ausschliesslich reale Wall-Clock-Zeit
   an; Temperatur-/Biologie-/Weighting-Felder beeinflussen die Entscheidung
   nicht.
3. Fehlt trusted UTC unmittelbar nach Boot, bleibt derselbe valide Run in
   `RecoveryEvaluation/TimePending`; er wird weder als `NoActiveRun`
   verworfen noch als Resume behauptet.
4. WLAN/NTP laeuft asynchron; es gibt keinen blockierenden Bootloop und keine
   fachliche Recovery-Logik in `device_platform`.
5. Persistenz-, Zeitanker-, Run-, Schema-, CRC-, Epoch- und Referenzunsicherheit
   bleibt `NO_GUESS`, `NO_AUTOMATIC_RESUME`, `FAIL_CLOSED` und wird nicht
   umetikettiert.
6. Nach jedem Boot sowie bei Pending, Reject, Fehler und unbekannter Lage sind
   Aktoren aus; keine elektrischen Zustandsreste werden restauriert.
7. Checkpointgrenzen und unmittelbare Eventpersistenz bleiben unveraendert;
   Sensorzyklen erzeugen keine neuen pauschalen Writes.
8. Der bestehende einzelne `RunPersistenceCoordinator` sowie die #121-
   Composition bleiben Source of Truth; es gibt keinen zweiten Coordinator,
   keinen monolithischen SafetyCore und keinen App-Typ in `device_platform`.
9. Schema-3-/#18-Felder bleiben, soweit erforderlich, kompatibel lesbar,
   erzeugen aber keine temperaturgewichtete R1-Entscheidung.
10. `OLDER_VALID_CHECKPOINT_RESUME` aus #90 bleibt ein getrenntes, nicht
    automatisch aktivierendes Fallback-Angebot; Callback-12- und
    NVS-Orakel werden nicht abgeschwaecht.
11. Die sechs Edge Cases sind mit der jeweiligen Ownerentscheidung umgesetzt
    und in gezielten nativen Tests nachgewiesen.
12. Die semantischen Recoveryzustaende sind spaeter fuer #25 projizierbar,
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
| Zeit ueberschreitet nominelle Dauer | Ownerentscheidung zu Completion-Pending versus explizitem naechstem Schritt; keine stille Phase |
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
  waere, wenn eine Ownerentscheidung aus Abschnitt 7 fehlt, wenn ein zweiter
  Coordinator erforderlich erschiene oder wenn Aktor-/Hardwarevertraege
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

## 15. Offene Ownerentscheidungen und Abschlussgate

```text
OWNER_DECISION_REQUIRED=YES
OPEN_OWNER_DECISIONS=
1. Verhalten bei elapsed_after_boot >= nominalDurationSeconds.
2. Kein automatischer naechster Phasenwechsel versus explizite Completion-/Resume-Entscheidung.
3. Explizite Zulassung eines vollstaendig validierten, bewusst ausgewaehlten #90-Fallbacks als Zeitrechnungsbasis.
4. Exakte Form der kleinen app-seitigen TimePending-Disposition, sofern RecoveryEvaluation sie nicht ohne semantische Ueberladung traegt.
```

Die bestehende Wahl des app-neutralen `ITimeSource`-Ports sowie das Beibehalten
der Schema-3-Lesbarkeit sind Empfehlungen ohne offene Ownerentscheidung,
solange der in Slice A geforderte Feldnachweis erfolgreich ist. Eine
Abweichung davon erzeugt vor Umsetzung ein neues Owner-Gate.

```text
ISSUE25_IMPLEMENTATION=NOT_STARTED
IMPLEMENTATION=NOT_STARTED
MERGE=NO
OWNER_PLAN_REVIEW_REQUIRED=YES
STOP=Owner Review des neuen R1-Recoveryplans
```
