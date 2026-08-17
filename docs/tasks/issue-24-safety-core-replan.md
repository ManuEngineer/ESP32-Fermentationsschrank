# Issue #24 – Release-1 Safety Core Plan

## 1. Status, Scope und Owner-Gate

Dieses Dokument ist die alleinige normative Planfassung für PR #110. Es ist
vollständig und eigenständig ausführbar; keine externe Planchronik ist für die
Umsetzung erforderlich.

```text
Repository: ManuEngineer/ESP32-Fermentationsschrank
Issue: #24 – [E3.5] Fehlerklassen, Verriegelung, SAFE_BOOT und Fehlerinjektion
PR: #110 – [E3.5] Issue #24 safety core replan from main
Branch: agent/issue-24-safety-core-replan-v2
Base: main
Verifizierte origin/main-SHA: b8eae5f4da5f2666b5a9bda333d115254c4db5b2
HEAD vor dieser Planfassung: f686e3e371336041dba883013b3878a20eba5493
Planpfad: docs/tasks/issue-24-safety-core-replan.md
PR-Status: OPEN / DRAFT
Implementation: NOT_STARTED
Planfreigabe: ausstehend für den exakten Commit dieser Datei
```

Issue #24 bleibt offen. PR #110 bleibt Draft. Es gibt in diesem Auftrag keine
Produktions-, Test-, Adapter- oder Persistenzimplementation, keinen neuen
Branch oder PR, kein Ready-for-review, keinen Merge und keine Issue-Schließung.

### Release-1-Ziel

Der Safety Core macht Boot, Laufklassifikation, Safety-Gate und Fehlerreaktion
fail-closed und reproduzierbar. Ein alter Lauf wird nur nach einer einfachen,
frischen und eindeutigen R1-Prüfung als Resume-Angebot angezeigt. Boot oder
Restore geben niemals selbst Aktoren frei.

Nicht Teil von R1 sind automatische oder verlustfreie Charge-Recovery,
Restart-Akkumulatoren, Resetzeitfenster, ein neuer allgemeiner Safety-Record,
Recovery-Evidence/Lineage/Capability, Fallback-Promotion, Rollback-Resume und
eine neue Persistenz- oder Recovery-Engine. Schema 3 bleibt lesbar; neutrale
Schema-3-Felder sind kein pauschaler Ablehnungsgrund.

## 2. Verbindliche Grenzen und Verantwortlichkeiten

Die Umsetzung verwendet die vorhandenen Verträge von `origin/main` und die
kanonischen Quellen aus Abschnitt 10.

| Bereich | Besitzer und R1-Grenze |
|---|---|
| Konfiguration | #56/#57 und ihre vorhandenen Producer bleiben die einzige Konfigurationswahrheit. Unavailable, IntegrityFailure, RuntimeFailure und CommitIndeterminate bleiben fail-closed. |
| Sensorqualität/-auswahl | #20/#21 liefern Qualität, Auswahl und Fallbackprojektion. #24 kopiert weder Sensor-FSM noch Sensorzustand. |
| Regelung | #22 bleibt Besitzer von PI- und Control-Logik. #24 bewertet nur die Safety-Grenze der Anfrage. |
| Aktorplanung | #23 bleibt Besitzer von Timing, Totzeit, Sink und Request-Watchdog. #24 liefert die einzige fachliche Safety-Direktive; ein caller-supplied `Allowed` ist nie Safety-Wahrheit. |
| Plattform | `device_platform` enthält nur portable Ports/Dienste, `device_platform_esp_idf` nur ESP-IDF-Adapter, `fermentation_app` die Fachentscheidung und `device_platform_test_support` nur Testhilfen. ADR-013 bleibt vollständig gültig. |
| Aktorausgänge | Der Gate-Default ist `ActuatorSafetyGateStatus::Unresolved`; vor vollständiger Validierung entsteht nur abstrakter Idle/Stop. Physische GPIO-, Pegel-, BTS7960-, MOSFET- und Lüfterbeweise bleiben E5/#29/#32/#33. |
| #106 | #24 darf native Allow-/Deny- und abstrakte Planner-/Sink-Tests liefern, aber keine produktive reale `Allowed`-Verdrahtung am Pro-Lauf-Parameter-Gate vorbei herstellen. |
| Quittierung | Ack, Anzeige und Journal sind Seiteneffekte. Ack ändert weder Fault-Clear noch Gate und ist niemals Wiederfreigabe. |

SafetyCore ist die einzige mutable Safety-Autorität. Process-FSM,
RunPersistenceCoordinator, ConfigurationService, Sensorquellen, Planner und
Sink behalten ihre fachlichen Zustände. Es entsteht keine zweite Sensor-,
Konfigurations-, Persistenz-, Recovery- oder Safety-Wahrheit.

## 3. SafetyCore und minimaler R1-Fault-Code-Vertrag

SafetyCore aggregiert kanonische Producer-Evidenz, führt den begrenzten aktiven
Safetyzustand und erzeugt die bestehende Gate-Projektion:

```text
Producer-Evidenz
  -> SafetyCore / FaultCode-Lifecycle
  -> ActuatorSafetyGateInput (Default Unresolved)
  -> ActuatorPlanner
  -> ActuatorPlanSinkDriver
  -> abstrakte Aktorports
```

Die drei R1-Dispositionen sind `Informational`, `OperationalBlocked` und
`LatchedOrSafeBoot`. `LatchedOrSafeBoot` ist nur ein Gruppierungsname; die
folgende Matrix legt für jeden Code die tatsächliche Gate-Reaktion eindeutig
fest. Es gibt keine vierstufige Fehlerklassenhierarchie.

Die FaultCode-Menge ist ein endlicher, compile-time bekannter, typisierter
`uint16_t`-Code. Die Werte sind stabil für Journal-/Diagnose-/Schnittstellen-
transport; sie führen keinen neuen persistenten Safety-Key ein. Technische
Detailursachen bleiben beim Producer und werden nur referenziert.

| Stabiler R1-Code / Wirewert | Kanonischer Producer und Detail | Disposition / Gate-Reaktion | Auto-Clear und Bedingung | Ack / manueller Reset / Neustart |
|---|---|---|---|---|
| `ConfigurationRuntimeFailure` / `0x0101` | `ConfigurationService` – vorhandener RuntimeFailure-Cause | `OperationalBlocked` / `ImmediateStop` | Ja, nur bei frischer gültiger Runtime-Projektion desselben Producers | Ack nur Anzeige; kein manueller Reset; Neustart gibt nicht frei |
| `ConfigurationUnavailable` / `0x0102` | `ConfigurationRecoveryService::ConfigurationSafetyProducer` | `LatchedOrSafeBoot` / `SAFE_BOOT` | Nein; erst frische verfügbare und integritätsgeprüfte Konfiguration | Ack nur Anzeige; kein unabhängiges Clear; Neustart gibt nicht frei |
| `ConfigurationIntegrityFailure` / `0x0103` | `ConfigurationRecoveryService::ConfigurationSafetyProducer` | `LatchedOrSafeBoot` / `SAFE_BOOT` | Nein; erst bestehender Producervertrag mit frischer Integritätsevidenz | Ack nur Anzeige; kein unabhängiges Clear; Neustart gibt nicht frei |
| `ConfigurationCommitIndeterminate` / `0x0104` | `ConfigurationService` – `CommitIndeterminate` bzw. vorhandener Commit-Status/Cause | `LatchedOrSafeBoot` / `SAFE_BOOT` | Nein; bestehende Commit-Auflösung muss eindeutig gültig sein | Ack nur Anzeige; kein neuer Reset-/Persistenzpfad; Neustart gibt nicht frei |
| `RunNotReconstructible` / `0x0201` | `RunPersistenceCoordinator` bei technisch vertrauenswürdigem, aber R1-semantisch nicht resumefähigem Current | `OperationalBlocked` / `ImmediateStop` für einen aktiven Alt-Lauf, danach `NoActiveRun` | Nein; nach erfolgreichem kanonischem Abschluss ist ein neuer Lauf möglich | Ack nur Anzeige; kein manueller Safety-Reset; Neustart gibt nicht frei |
| `RunPersistenceIndeterminate` / `0x0202` | Coordinator bei unbestimmter Gesamttransaktion oder nicht vertrauenswürdigem Load | `LatchedOrSafeBoot` / `SAFE_BOOT` | Nein; nur bestehende Store-/Coordinator-Neubewertung mit vertrauenswürdigem Ergebnis | Ack nur Anzeige; kein neuer Record; Neustart gibt nicht frei |
| `SafetySensorUnavailable` / `0x0301` | Projektion der vorhandenen #20/#21-Qualitäts-/Auswahlzustände | `OperationalBlocked` / `ImmediateStop` | Ja, nur frische gültige Sicherheitsrollen- und Auswahlprojektion | Ack nur Anzeige; kein manueller Reset; Neustart gibt nicht frei |
| `ActuatorRequestWatchdog` / `0x0401` | #23 vorhandene Request-Watchdog-Evidenz und bestehender Latched-Zustand | `LatchedOrSafeBoot` / `ImmediateStop` | Nein; nur bestehender #23-Clear mit frischer Safety-/Sink-Evidenz | Ack nur Anzeige; fachlicher Reset nur über bestehenden #23-Vertrag; Neustart gibt nicht frei |
| `ThermalSafetyIntervention` / `0x0501` | Bereits definierter R1-Temperatur-/Safety-Producer; keine neuen #35-Grenzwerte | `OperationalBlocked` / `ImmediateStop` | Ja, nur frische kanonische sichere Evidenz des Producers | Ack nur Anzeige; kein manueller Safety-Reset; Neustart gibt nicht frei |
| `ThermalHardLimit` / `0x0502` | Bereits definierter R1-Hard-Limit-Producer; Grenzwerte bleiben #35/E5 | `LatchedOrSafeBoot` / `SAFE_BOOT` | Nein; bestehende Ursachen-, Hardware- und Integritätsprüfung erforderlich | Ack nur Anzeige; manueller Reset nur falls der bestehende Producer ihn vorsieht; Neustart gibt nicht frei |
| `SystemSafetyUnknown` / `0x0601` | SafetyCore für unknown/unmapped Producer-, Boot-, System- oder Integritätszustand | `LatchedOrSafeBoot` / `SAFE_BOOT` | Nein; nur vollständige frische Vertrauensevidenz, keine Ack-Abkürzung | Ack nur Anzeige; kein unabhängiges Clear; Neustart gibt nicht frei |

`ThermalSafetyIntervention` und `ThermalHardLimit` werden nur emittiert, wenn
ein bestehender R1-Producer dafür eine fachlich definierte Eingabe besitzt.
`TBD_HARDWARE`, `TBD_COMMISSIONING` und #35-Grenzwerte werden in #24 nicht
erfunden. Ein unbekannter oder nicht gemappter Producerzustand wird immer zu
`SystemSafetyUnknown` und niemals zu `Allowed`.

Reine Information ohne Safetywirkung bleibt Journal-/Notification-Ereignis
und wird nicht künstlich zum FaultCode. SafetyCore besitzt keine Historie und
keine dynamischen Faulttexte: aktive und acknowledged Zustände sind fest bzw.
enum-indexiert begrenzt, `IEventJournal` besitzt die Historie, Formatierung und
Notification liegen außerhalb des zeitkritischen Safety-Entscheidungskerns.
Es gibt kein Heap-Wachstum pro Fault-Ereignis und keine PSRAM-Annahme.

### Producer-Projektion

- Konfiguration konsumiert direkt `ConfigurationSafetyProducer` sowie die
  vorhandenen `ConfigurationServiceMode`-/`ConfigurationCommitStatus`-Werte
  inklusive typisierter Causes über genau eine schmale Safety-Eingangsprojektion.
  Es entstehen keine parallelen Safety-Enums für dieselben Zustände.
- Sensorik konsumiert #20 `SensorQuality` und #21 Auswahl-/Fallbackresultate.
- Planner/Sink konsumiert die vorhandene #23 Watchdog-/Gate-Evidenz. Ein
  technischer Zustand ohne kanonischen Mappingeintrag bleibt unbekannt-safe.
- Run-Persistenz konsumiert Load-/Coordinator-Status, Transaktionsschritt,
  technische Ursache und Durability. Ein semantisch nicht resumefähiger,
  technisch integer geladener Run ist kein technischer Systemfault.

Die Reihenfolge ist zwingend:

```text
Producer-Evidenz -> Safetyzustand/Gate bestimmen -> Stop/Unresolved wirksam
                 -> danach Journal/Notification
```

`IEventJournal::record(...) == false` und ein Fehler von
`IUserNotificationSink::notify(...)` dürfen Stop, Unresolved, SAFE_BOOT oder
eine vorhandene Sperre nicht verhindern, zurückrollen oder eine zweite
Safetyreaktion erzeugen. Ack hängt nicht von einem Journalerfolg ab, sofern ein
bereits bestehender Fachvertrag nicht ausdrücklich etwas anderes verlangt.

## 4. Run-Persistenz: Einzel-Write und Gesamttransaktion

### 4.1 Einzelner Key-Write

Der vorhandene `IStateStore`-/`RunPersistenceStore::writeExact()`-Vertrag wird
unverändert verwendet:

| `IStateStore::write()` | `writeExact()` | Readback |
|---|---|---|
| `Success` | `Written` | keiner; der neue Wert ist vollständig und dauerhaft gespeichert |
| `WriteError` | `WriteError` / `NotWritten` | keiner; dieser Key-Write ist sicher nicht wirksam |
| `CapacityError` | `CapacityError` | keiner; dieser Key-Write ist sicher nicht wirksam |
| `CommitOutcomeUnknown` | `Written`, `NotWritten` oder `Indeterminate` | genau ein vorhandener `writeExact()`-Readback: neuer Wert, alter Wert oder fremd/Fehler |

Ein zweiter Readback nach normalem `Success` ist verboten. Die Aussage über
`WriteError`, `CapacityError` und `NotWritten` betrifft nur diesen einzelnen
Key-Write und nicht automatisch eine bereits mehrstufig veränderte
Gesamttransaktion.

### 4.2 Nichtperiodischer Coordinator-Pfad

Für Resume-Entscheidung und `NoActiveRun` wird ausschließlich der bestehende
`RunPersistenceCoordinator::writeSnapshotCore()`-Pfad verwendet:

```text
PreparedHead -> CheckpointSlot -> CommittedHead
```

| Transaktionspunkt | Bestehender Gesamtzustand | R1-Folge |
|---|---|---|
| Fehler vor bzw. beim `PreparedHead`, sicher nicht geschrieben | keine dauerhafte Mutation; rollbackfähiger Coordinator-State | kein RAM/FSM-Apply; bestehender Unchanged-/Rollback-Vertrag bleibt gültig |
| `PreparedHead` indeterminate | Gesamtzustand unbestimmt | bestehender `BlockedIndeterminate`-/`PersistenceIndeterminate`-/`MayHaveChanged`-Vertrag; kein Apply |
| `PreparedHead` definitiv geschrieben, danach Slot-Fehler | PreparedHead dauerhaft vorhanden; Teiltransaktion geändert | `BlockedIndeterminate`, Durability `Changed` bzw. bestehender Coordinatorstatus; kein Apply; alten Gesamtzustand nicht als sicher autoritativ behaupten |
| `PreparedHead` und Slot geschrieben, danach `CommittedHead`-Fehler | Teiltransaktion dauerhaft vorhanden | `BlockedIndeterminate`/`PersistenceIndeterminate` gemäß Coordinator; kein Apply; kein Rückschluss auf unveränderten Altzustand |
| kompletter nichtperiodischer Coordinator-Pfad `Applied` | Zielzustand vollständig committed | erst jetzt detached Kandidat in RAM/FSM anwenden |

`WriteError`, `CapacityError` oder `NotWritten` eines Slot- oder CommittedHead-
Writes ändern nach einem definitiv geschriebenen PreparedHead nicht die
Gesamtfolge zurück auf „old authoritative“. Die exakten Status-, Step- und
Durability-Werte werden aus dem bestehenden Coordinatorvertrag wiederverwendet;
#24 baut keine Transaktionsengine.

Die Apply-Invariante lautet:

```text
detached Kandidat
  -> bestehender Coordinator-Write-before-Apply-Pfad
  -> Gesamtstatus Applied?
       ja  -> RAM/FSM anwenden
       nein -> kein normales Standby/Allowed; Coordinator-/Safety-Status erhalten
```

Bei `RecoveryEvaluation` bleibt `RecoveryResume` ein expliziter Befehl ohne
`Allowed` und ohne Aktorcommand. Nach aktueller Config-/Sensor-/Safety-Prüfung
wird die Entscheidung über den bestehenden Persistenzpfad committed; erst
`Applied` aktiviert die FSM.

`RecoveryReject` bei Ablehnung, Timeout oder nicht resumefähigem Current wird
wiederverwendet, aber topologisch neu festgelegt: Aktoren bleiben AUS, zuerst
wird der kanonische `NoActiveRun`-Snapshot über denselben Gesamtpfad geschrieben,
und erst ein Gesamtstatus `Applied` führt `RecoveryEvaluation` mit
`TransitionReason::RecoveryRejected` nach `Standby`. Bei Writefehler,
`PersistenceIndeterminate`, `BlockedIndeterminate` oder unbekannter Durability
behauptet RAM nicht normales Standby; der bestehende unknown-safe-/
`SAFE_BOOT`-Vertrag bleibt aktiv. Eine neue FSM, ein neuer Event oder ein neuer
Persistenzschlüssel ist dafür nicht vorgesehen.

## 5. Schema-3-Rekonstruktion, Boot-/Load-Matrix und Fallback

### 5.1 Exaktes R1-Prädikat

`schemaVersion == 3` und die bloße Existenz regulärer Schema-3-Felder lehnen
einen Resume nicht ab. Ein Current ist R1-eindeutig nur, wenn Head, referenzierter
Slot, Codec-/Cross-Field-Validierung, Run-/Process-Projektion,
Konfigurationssnapshot, Sensorselection und Runrevision gültig sind, der
Prozesszustand der Phasenmatrix entspricht und keine offene Zeit-/Recovery-
Entscheidung benötigt.

| Schema-3-Feld | Neutral und ignorierbar | Für einfache Rekonstruktion | Blockiert R1-Resume bei aktiver Semantik | Legacy-/Diagnoseinformation |
|---|---|---|---|---|
| `RecoveryTemperatureEvidence` | Default ohne gültige gefilterte Rollewerte | nicht erforderlich | nie allein; keine Zeitgutschrift | vorhandene Last-known-Werte nur diagnostisch |
| `RunProgressState` | `KnownTotal`, kein `weightedProgress`; `observedRunSeconds` wird nicht gutgeschrieben | kein Zeitbeweis | `PartialUnknownHistory` oder gesetztes `weightedProgress` | beobachtete Werte ohne Freigabewirkung |
| `PendingRecoveryAnchor` | nicht vorhanden | nicht erforderlich | vorhanden | Ankerdaten werden nicht neu interpretiert |
| `recoveryBootAnchorMonotonicMillis` | nicht vorhanden | nicht erforderlich | vorhanden zusammen mit offener Recovery | technische Diagnose |
| `TaggedPriorBootPhaseElapsed` | nicht vorhanden | nicht erforderlich | vorhanden | niemals als R1-Restzeit verwenden |
| `NominalRecoveryAdjustmentState` | nicht vorhanden | nicht erforderlich | vorhanden | keine gewichtete Gutschrift |
| `lastRecoveryEpisodeEvidence` | nicht vorhanden | nicht erforderlich | nur wenn mit offener Recovery-/Progressentscheidung verknüpft | abgeschlossene Vor-/Nach-Ausfall-Diagnose ignorierbar |
| `recoveryEpisodeRevision` | `0` oder vorhandene Revision ohne offene Evidenz | nicht erforderlich | nur zusammen mit ungültiger/offener Recoverysemantik | reine technische Diagnose |

Ungültige Kombinationen, unbekannte Enumwerte, beschädigte Bytes oder fehlende
für den Snapshot erforderliche Kernfelder sind kein semantischer Abbruch,
sondern ein technisch nicht vertrauenswürdiger Load und führen zu `SAFE_BOOT`.
Ein alter Zeit-, UTC-, `PriorBootPhaseElapsed`-, weighted-Progress- oder
Nominalwert wird niemals als automatische R1-Gutschrift verwendet.

### 5.2 Vollständige Load-/Boot-Disposition

Die Implementierung verwendet die vorhandenen
`RunPersistenceLoadStatus`- und Coordinator-Zustände; es entsteht keine zweite
Load-FSM.

| Realer Load-/Coordinator-Fall | R1-Ausgang |
|---|---|
| `NoPersistedRun` | sicherer `Standby`, Gate bleibt zunächst `Unresolved` |
| `NoActiveRun` | sicherer `Standby`, keine Aktorfreigabe |
| gültiger `Current` mit `Completed` | vorhandene `Completed`-Projektion beibehalten; kein Resume, keine Aktoren |
| gültiger `Current` mit aktivem Run und vollständig erfülltem R1-Prädikat | nicht freigebendes `RecoveryEvaluation`-Resume-Angebot; kein `Allowed` |
| gültiger, technisch integerer Current, aber R1-semantisch nicht resumefähig | `RunNotReconstructible`; kanonischen `NoActiveRun`-Abschluss wie Abschnitt 4, nur nach `Applied` `Standby` |
| gültiger Current mit alter/offener komplexer Recovery-Evidenz | kein komplexer Resume; sofern der Store eindeutig vertrauenswürdig ist, derselbe sichere `NoActiveRun`-Abbruch |
| gültiger historischer `Current` in `Fault` | terminale `Fault`-Projektion und Diagnose erhalten; keine RecoveryEvaluation, kein Allowed, kein automatisches Überschreiben; ein späterer expliziter Clear folgt dem bestehenden Producer-/Write-before-Apply-Vertrag |
| `FallbackRecovered` | technische Degradation/Integritätsfall; kein Fallback-Resume, keine Promotion und kein Tombstone zum Verbergen des unklaren Current; `SAFE_BOOT`/unknown-safe |
| `FallbackRecoveryPending` | bestehender Coordinator-Block; `SAFE_BOOT`/unknown-safe, keine Mutation zum Verbergen |
| `PreparedInterrupted` | `SAFE_BOOT`/unknown-safe; keine automatische Mutation |
| `NotReconstructible` | `SAFE_BOOT`/unknown-safe; kein blindes `NoActiveRun` |
| `NotReconstructibleOrphanedState` | `SAFE_BOOT`/unknown-safe; kein blindes `NoActiveRun` |
| `ReadFailed` | `SAFE_BOOT`/unknown-safe |
| `CapacityExceeded` beim Load | `SAFE_BOOT`/unknown-safe |
| `UnsupportedSchema` | `SAFE_BOOT`/unknown-safe |
| `ForeignEpoch` | `SAFE_BOOT`/unknown-safe |
| `AlreadyInitialized` bei erneutem Load | vorhandenen Coordinator-State nicht überschreiben; kein Resume/Allowed; bei nicht sicher feststellbarem State `SAFE_BOOT` |
| `BlockedIndeterminate` oder `PersistenceCommittedApplyFailed` | `SAFE_BOOT`/unknown-safe, kein RAM/FSM-Apply |

`NoActiveRun` ist damit ausschließlich der sichere Abbruch für einen
vertrauenswürdig geladenen, fachlich nicht mehr einfachen R1-Resume-Kandidaten.
Ein technisch nicht vertrauenswürdiger Persistenzzustand wird nicht durch einen
neuen Tombstone „repariert“.

### 5.3 Fallback-Regel

Der vom Committed Head referenzierte gültige Current ist der einzige
R1-Kandidat. Der normale Commit setzt den Fallback bewusst auf den vorherigen
Current-Checkpoint; eine ältere Runrevision, TransitionSequence oder
Prozessprojektion ist daher legitim und kein Gleichheitsfehler.

- Gültiger Current plus älterer legitimer Fallback: Current wird unabhängig
  vom Fallback nach dem R1-Prädikat bewertet.
- Defekter Current plus lesbarer Fallback: `FallbackRecovered`, kein Resume,
  keine Promotion, kein Rollback-Resume.
- Widersprüchlicher oder technisch unklarer Head-/Slotzustand: unknown-safe/
  `SAFE_BOOT`.
- Es gibt keinen Vergleich `Current == Fallback` als Resume-Voraussetzung und
  keine zweite Fallback-Auswahlentscheidung.

## 6. R1-Resume-Phasenmatrix

Die folgende Matrix ist vor Implementation verbindlich. Sie ersetzt einen
offenen generischen Prädikatsplatzhalter.

| Aktiver `ProcessState` | R1-Resume | Ziel nach bestätigtem Resume | Bootlokale Timer / verworfene Werte | Begründung |
|---|---|---|---|---|
| `Preheating` | Ja | `Preheating` | Ziel-/Sensorbewertung und eventuelle Qualifikation ab Boot neu starten; alte Timer-/Qualifikationswerte verwerfen | ziel- und sensorbasierte Fortsetzung mit frischer Evidenz |
| `WaitingForProduct` | Nein | kein Resume; `NoActiveRun` | maximale Wartezeit und alte Dauer nicht fortführen | alte Wartezeit könnte die Frist bereits überschritten haben |
| `ReachingTarget` | Nein | kein Resume; `NoActiveRun` | Target-Reach-Timer und Warnstatus verwerfen | bootlokale Erreichungsdauer ist nicht beweisbar |
| `QualifyingTarget` | Nein | kein Resume; `NoActiveRun` | Qualifikationszeit und monotonic Basis verwerfen | Zielqualifikation startet nicht mit alter Zeitgutschrift |
| `Fermenting` | Nein | kein Resume; `NoActiveRun` | Dauer, `PriorBootPhaseElapsed`, UTC-/Progressrechnung verwerfen | Restzeit ist ohne freigegebenen Zeitbeweis unbekannt |
| `Cooling` | Ja | `Cooling` | aktuelle Ziel-/Sensorbewertung frisch starten; keine alte Dauer gutschreiben | primär sensor-/zielbasiert, keine alte Restzeit erforderlich |
| `CoolHolding` | Nein | kein Resume; `NoActiveRun` | Dauer und `PriorBootPhaseElapsed` verwerfen | dauerbasierte Haltephase ohne sicheren Zeitbeweis |
| `ManualHolding` | Ja, nur als ausdrücklich indefinite manuelle Haltephase | `ManualHolding` | keine alte Dauer; bei einem gespeicherten endlichen Hold-Parameter ist das Prädikat falsch und es gilt `NoActiveRun` | manuell, nicht automatisch dauerbasiert; endliche Variante darf nicht geschätzt werden |

Für alle positiven Zeilen gelten zusätzlich: vollständige frische Config-,
Sensor-, Selection-, Planner- und Safety-Prüfung, kein offener Recovery-/Progress-
Entscheidungszustand, Planner bis zur Benutzerbestätigung `Unresolved`/AUS und
kein Aktorcommand aus Restore. Jede negative Zeile bildet den detached
`NoActiveRun`-Kandidaten; der Apply erfolgt nur über `Applied`.

## 7. Boot, Resetcause und E3/E5

Ein kleiner app-neutraler Read-only-Port in `device_platform` liefert eine
portable Resetcause. Der deterministische Testadapter liegt in
`device_platform_test_support`; ein ESP-IDF-Adapter darf innerhalb des
Umsetzungsschnitts in `device_platform_esp_idf` ergänzt werden. Der Port trifft
keine Safetyentscheidung, zählt nichts, persistiert nichts und konfiguriert
keinen Watchdog.

Der ESP-IDF-Adapter mappt die vollständige Ziel-API von `esp_reset_reason()`
exhaustiv: `UNKNOWN`, `POWERON`, `EXT`, `SW`, `PANIC`, `INT_WDT`, `TASK_WDT`,
`WDT`, `DEEPSLEEP`, `BROWNOUT`, `SDIO`, `USB`, `JTAG`, `EFUSE`, `PWR_GLITCH`
und `CPU_LOCKUP` erhalten portable Diagnosewerte; ein unbekannter künftiger
Wert fällt auf `Unknown/Other` und bleibt fail-safe. Nicht jede Ursache erhält
eine eigene Safetyreaktion. Für alle Ursachen gilt zuerst all-off,
vollständige Revalidierung und kein automatischer Resume. Die Wiederholung
derselben Ursache verändert nichts: kein Zähler, kein Zeitfenster, keine neue
Persistenz.

Insbesondere ändert #24 weder TWDT-Zeitwerte noch
`CONFIG_ESP_TASK_WDT_PANIC` und behauptet nicht, dass ein Task-Watchdog immer
einen Hardware-Reset auslöst. Der Port diagnostiziert nur den tatsächlich
gelieferten Grund.

Die Softwaregrenze vor E5 lautet: Gate `Unresolved`, abstrakter Planner-/Sink-
Idle/Stop, kein normales `Allowed` vor vollständiger Bootvalidierung. Der
Resetcause-Adapter initialisiert keine GPIOs und ist kein Aktoradapter. Die
physische Ausgangsbeweiskette bleibt E5 und umgeht #106 nicht.

## 8. Aktiver Produktpfad und Legacy-Abgrenzung

Für #24 wird nur der aktive Produktpfad vereinfacht:

**C1 – zwingend in #24:** automatische Recovery auslösen, alten Run ohne
explizite Entscheidung aktivieren, alte Zeit/Fortschritt automatisch gutschreiben,
Fallback promoten/rollbacken oder auf Grundlage alter Recoverylogik Aktoren
freigeben. Diese Aufrufe werden deaktiviert/entfernt; negative Architekturtests
belegen, dass sie keinen produktiven Pfad mehr erreichen.

**C2 – zunächst nicht löschen:** bereits gemergte, nach C1 unaufgerufene
`run_recovery_time`, `run_progress_weighting`, Recovery-Modelle, Helfer und
zugehörige historische Tests bleiben als Legacy/Deprecated bestehen. Ihre
Existenz erzeugt keinen aktiven Vertrag, weil keine produktive Aufrufkante
mehr besteht. Ein kleiner separater Follow-up-Cleanup wird nach stabiler
Integration von #24 empfohlen. #24 löscht C2 nur, wenn ein konkreter Baustein
nachweislich nicht abtrennbar ist oder einen Parallelvertrag erzwingt; dann
muss genau dieser Baustein samt Begründung vor Umsetzung im Plan aktualisiert
und erneut freigegeben werden.

## 9. Kanonische Dokumente und Umsetzungsschnitte

Nach Ownerfreigabe werden die betroffenen Dokumente in einem eigenen
Dokumentationsschnitt synchronisiert; diese Planrevision ändert sie noch nicht.

1. `docs/DIAGNOSTICS_AND_MAINTENANCE.md`: passiver Boottest und Diagnoseexport
   nennen Resetcause als Diagnose, keinen generischen Restart-Zähler und keinen
   neuen allgemeinen Safety-Latch; die bisherige generelle Annahme eines
   persistierten Restartzählers bzw. einer persistierten Verriegelung wird
   entfernt; `SAFE_BOOT` folgt aktuellem untrusted System-/Config-/Persistenzzustand.
2. `docs/RUNTIME_BEHAVIOR.md`: Meldungen referenzieren die stabile R1-
   FaultCode-/Disposition-Matrix; es bleibt keine zweite offene Fehlerklassen-
   oder Codeklassifikation.
3. `docs/RESOURCE_BUDGET_AND_MAINTENANCE.md`: als bindende SafetyCore-Quelle
   bestätigen: feste/begrenzte Fehlerzustände, keine Historie im Core, keine
   unkontrollierten dynamischen Faultstrings, 4 MB Flash, keine PSRAM-Annahme.
4. `SAFETY_AND_FAULTS.md`, `SAFETY_COMPONENT_FAULTS.md`,
   `SYSTEM_SAFETY_AND_RECOVERY.md`, `STATE_MACHINE.md`, `RUN_PERSISTENCE.md`,
   `RECOVERY_AND_INTERRUPTION.md` und `ACCEPTANCE_TESTS.md`: neue Load-,
   Resume-, Reject-, Fault- und Journalreihenfolge ohne automatische
   Charge-Recovery.
5. `SPECIFICATION_REVIEW.md`, `REQUIREMENTS.md`, `ARCHITECTURE.md`,
   `CONFIGURATION_PERSISTENCE.md` und `IMPLEMENTATION_ISSUES.md`: R1-Scope,
   #56/#57-Gate, ADR-013 sowie E3/E5/#106-Grenzen synchronisieren und alte
   Restart-Eskalations- bzw. generische Persistenz-Latch-Annahmen entfernen.
6. `DECISIONS.md`/ADR-018 und ADR-013 bleiben vollständig erhalten; nur echte
   Widersprüche werden dokumentarisch bereinigt. Keine Auth-/PIN- oder
   Hardware-Implementierung wird in #24 erfunden.

Vorgesehene Umsetzungsschnitte nach Freigabe:

1. portable Resetcause-Ports, Testadapter und sichere Composition-Root-
   Verdrahtung ohne Aktorhardware;
2. SafetyCore, FaultCode-Matrix, feste Zustandsrepräsentation und Producer-
   Projektion;
3. Load-/Fallback-/Schema-3-Prädikat, Write-before-Apply und vorhandene FSM-
   Eventtopologie;
4. C1-Abtrennung, Planner-/Sink-Gate und #106/E3/E5-Negativgrenzen;
5. kanonische Dokumente und gezielte Test-/Nachweisschnitte.

Jeder Schnitt stoppt bei einer materiellen Abweichung und verlangt eine neue
Planfreigabe. Es werden keine neuen Persistenzschemas, Keys, Recovery-Capabilities
oder Parallel-Engines eingeführt.

## 10. Test- und Nachweisvertrag

Die Tests werden erst nach Ownerfreigabe geplant umgesetzt; in dieser
Planphase gibt es keinen Testcode. Die Orakel sind:

### Persistenz und Apply

- einzelner `Success` ohne zusätzlichen Readback erlaubt Apply;
- `WriteError` und `CapacityError` auf einem Einzelkey ohne Apply;
- `CommitOutcomeUnknown` mit Readback neuer Wert -> `Written`/weiter;
- Readback alter Wert -> `NotWritten`/kein Apply;
- fremder Wert oder Readbackfehler -> `Indeterminate`/kein Apply/
  unknown-safe;
- PreparedHead sicher nicht geschrieben -> rollbackfähiger alter Coordinator-
  State, kein Apply;
- PreparedHead indeterminate -> blocked/indeterminate, kein Apply;
- PreparedHead `Success`, Slot `WriteError`/`CapacityError`/`NotWritten` ->
  PreparedHead dauerhaft, Coordinator blocked/changed, kein „old authoritative“;
- PreparedHead `Success`, Slot `Success`, danach CommittedHead `WriteError`
  (sowie CapacityError/NotWritten) -> Teiltransaktion dauerhaft,
  blocked/indeterminate, kein Apply;
- vollständiger nichtperiodischer Pfad `Applied` -> erst dann RAM/FSM-Apply.

### Boot, Load, Fallback und Resume

- jeder Load-/Coordinator-Fall aus Abschnitt 5 erhält exakt sein Orakel;
- `Completed` bleibt abgeschlossen und gibt nichts frei;
- historischer `Fault` bleibt diagnostisch terminal und aktiviert keine Recovery;
- gültiger Current plus älterer legitimer Fallback erlaubt Current-Prüfung;
- defekter Current plus gültiger Fallback erzeugt kein Fallback-Resume;
- unklarer Head-/Slotzustand bleibt unknown-safe;
- jeder der acht aktiven Prozesszustände erhält den positiven/negativen Nachweis
  aus Abschnitt 6;
- RecoveryEvaluation bleibt nicht freigebend; Resume ist explizit;
- Reject/Timeout/nicht resumefähiger Run schreibt zuerst `NoActiveRun`, und
  nur `Applied` darf nach `Standby` führen.

### Fault-, Ressourcen- und Seiteneffektverhalten

- jeder reale Producerzustand mappt deterministisch genau auf den Matrixcode;
- unknown/unmapped bleibt `SystemSafetyUnknown`/fail-closed;
- Ack verändert weder Code-Lifecycle noch Gate;
- Auto-Clear passiert nur für ausdrücklich markierte Codes mit frischer
  kanonischer Evidenz;
- kritische, unknown- und indeterminate-Codes erzeugen nie `Allowed`;
- Journal- oder Notificationfehler beeinflussen Stop/Unresolved nicht;
- keine neue Safety-Persistenz, kein Fault-History-Container, keine dynamischen
  Faulttexte und kein Heap-Wachstum pro Ereignis;
- #20/#21, Config, Planner und FSM werden nicht im SafetyCore dupliziert.

### Resetcause, E3/E5 und reale Gates

- jeder bekannte ESP-IDF-Resetwert und `Unknown/Other` wird deterministisch
  getestet;
- keine Ursache zählt sich auf, erzeugt ein Zeitfenster oder persistiert eine
  Sperre;
- kein Test behauptet einen zwingenden TWDT-Hardware-Reset;
- abstrakter Sink bleibt bis Bootvalidierung Idle/Stop;
- kein produktiver Hardware-`Allowed`-Bypass und keine #106-Umgehung;
- GPIO-, Brownout-, Watchdog- und reale Aktornachweise bleiben ausdrückliche
  Integrationsgates und werden nicht durch Native-Tests als bestanden behauptet.

Die Befehle und Profile für spätere gezielte Tests stehen ausschließlich in
`docs/CI_AND_QUALITY_GATES.md`. In diesem Draft-Auftrag werden nur leichte
Plan-/Markdown-Gates ausgeführt; Firmware-Builds, vollständige Testläufe,
Hardwaretests und CI bleiben `NOT_RUN`.

## 11. Abschluss-Gate

Nach der Planänderung werden ausschließlich:

1. Markdown-/Planstruktur- und Diff-Gates,
2. `docs/ROADMAP.md`-Synchronisierung,
3. PR-Body-Synchronisierung,
4. genau ein aktueller `SESSION HANDOVER`-Kommentar

ausgeführt. Danach werden Branch, Base, PR-HEAD, Draftstatus, Issue-Status,
Planpfad und exakte Plan-SHA erneut gelesen. Die exakte neue Plan-SHA wartet
auf ausdrückliche Ownerfreigabe. Bis dahin bleibt
`Implementation: NOT_STARTED` und PR #110 Draft.
