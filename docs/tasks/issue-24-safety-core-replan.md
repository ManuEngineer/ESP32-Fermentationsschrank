# Issue #24 – Release-1 Safety Core Plan

## Status und Freigabe

- Repository: `ManuEngineer/ESP32-Fermentationsschrank`
- Issue: #24 – [E3.5] Fehlerklassen, Verriegelung, SAFE_BOOT und Fehlerinjektion
- PR: #110 – [E3.5] Issue #24 safety core replan from main
- Branch: `agent/issue-24-safety-core-replan-v2`
- Verifizierte Basis: `main` @ `b8eae5f4da5f2666b5a9bda333d115254c4db5b2`
- Implementation: `NOT_STARTED`
- PR bleibt offen und Draft.

Diese Fassung ist der vollständige, eigenständig ausführbare Release-1-Plan.
Implementation, Testcode und Adaptercode beginnen erst nach ausdrücklicher
Freigabe genau dieses Plan-Commits.

## 1. Release-1-Ziel und Grenzen

Release 1 schützt vor unsicherer Aktorfreigabe. Der Verlust einer Charge nach
Stromausfall, Brownout, Watchdog, Panic, Crash oder unklarem Runzustand ist
akzeptiert. Boot und Restore sind deshalb fail-closed; eine Charge wird nicht
um jeden Preis gerettet.

Nicht Teil von #24 sind Hochverfügbarkeit, SIL-/Medizinalarchitektur,
Service-PIN, Restart-Zähler oder Resetzeitfenster, allgemeine Safety-
Persistenz, persistente Watchdog-/Sensor-/Thermal-Latches, automatische
`SAFETY_RECOVERY`-Gegenrichtung, Charge-Rettung, neue Thermal-/Hardware-
Producer oder Grenzwerte aus #35/E5 sowie eine universelle Fehlerplattform.

Es werden keine neuen Run-Persistenzschemas, Persistenzschlüssel,
Recovery-Evidence, Lineage, Capability oder zweite Safety-Wahrheit eingeführt.

## 2. Bestehende Verantwortungsgrenzen

Die Umsetzung verwendet die vorhandenen Verträge und ergänzt nur die für #24
notwendige Safety-Projektion:

- `device_platform` bleibt anwendungsneutral; ESP-IDF-Code bleibt in
  `device_platform_esp_idf`; Fachlogik bleibt in `fermentation_app`; Test-
  adapter bleiben in `device_platform_test_support` (ADR-013).
- #17 bleibt Eigentümer von `IStateStore`, `RunPersistenceStore` und
  `RunPersistenceCoordinator`, einschließlich Load- und Transaktionsstatus.
- #20/#21 bleiben Eigentümer von Sensorqualität, Produktsensor-Auswahl,
  Luftfallback und Rückkehrvalidierung. #24 konsumiert deren echte
  fail-closed Projektion und bildet keine zweite Sensor-FSM.
- #22 bleibt Eigentümer von PI-/Control-Logik, #23 von Aktor-Timing,
  Totzeit und Request-Watchdog.
- #56/#57 bleiben Eigentümer der Configuration-Safety-Producer. #24
  projiziert deren reale Zustände, kopiert ihre Zustandsmaschinen nicht.
- `ActuatorSafetyGateStatus::Unresolved` ist der sichere Default. Nur die
  zentrale Kette SafetyCore -> Planner -> Sink darf die fachliche
  Freigabeentscheidung liefern; ein aufruferseitiges `Allowed` ist keine
  Safety-Wahrheit.
- #106 darf nicht umgangen werden. #24 liefert abstrakte Gate-/Planner-/Sink-
  Integration und native Allow-/Deny-Nachweise, aber keine produktive reale
  Aktorfreigabe vor dem bestehenden Pro-Lauf-Parameter-Gate.

## 3. Boot und ResetCause

Der Gate-Zustand startet `Unresolved`. Der abstrakte Aktorpfad erzeugt bis zum
Abschluss der Bootvalidierung ausschließlich Idle/Stop. Kein Boot, Restore oder
Reset erzeugt `Allowed`.

Jeder Bootgrund verwendet denselben Ablauf: ResetCause diagnostisch lesen,
Aktoren abstrakt all-off halten, Configuration, Run-Persistenz und erforderliche
aktuelle Sensor-/Safety-Evidenz vollständig revalidieren und erst nach einem
neuen expliziten Start- oder Resume-Befehl erneut prüfen. Es gibt keinen
automatischen Resume.

Dafür werden folgende kleine, anwendungsneutrale Ports geplant:

1. read-only ResetCause-Port in `device_platform`;
2. deterministischer Testadapter in `device_platform_test_support`;
3. optionaler ESP-IDF-Adapter in `device_platform_esp_idf`, der alle vom
   verwendeten `esp_reset_reason()`-API gelieferten bekannten Werte auf
   portable Diagnosekategorien abbildet und unbekannte/neue Werte als
   `Unknown/Other` liefert.

Der Port trifft keine Safetyentscheidung, schreibt keine Persistenz, zählt
keine Neustarts und konfiguriert keinen Watchdog. Die Umsetzung ändert weder
TWDT-Zeitwerte noch `CONFIG_ESP_TASK_WDT_PANIC`. Der ESP-IDF-Adapter wird nicht
zum Aktoradapter.

## 4. Minimaler Safety- und Fault-Vertrag

R1 besitzt eine endliche, typisierte FaultCode-Menge. UI-Texte sind keine
Safety-Wahrheit. Aktiver und quittierter Zustand bleibt fest bzw. enum-
indexiert im SafetyCore; daraus entsteht keine neue persistente Safety-
Permanenz. SafetyCore besitzt keine Fault-Historie. Historie und Journaling
bleiben beim vorhandenen `IEventJournal`. Stringformatierung und Notification
liegen außerhalb des zeitkritischen Safety-Kerns.

Die drei R1-Reaktionen sind direkt und ohne historische Fehlerklassenhierarchie
definiert:

- `Information`: keine Aktoränderung;
- `Blocked/ImmediateStop`: kein `Allowed`, laufende Freigabe sofort stoppen;
- `SAFE_BOOT`: kein `Allowed`, Boot-/System-/Config-/Persistenzzustand bleibt
  gesperrt.

Die normative minimale Matrix lautet:

| FaultCode / Wire-Wert | Kanonischer Producer und Detail | R1-Reaktion | Auto-Clear / Clear | Ack | Neustart |
|---|---|---|---|---|---|
| `ConfigurationRuntimeFailure` / `0x0101` | `ConfigurationService` RuntimeFailure | `Blocked/ImmediateStop` | nein; nur frische gültige Configuration-Evidenz und neuer expliziter Start | nur Anzeige/Journaling, nie Gate-Clear | keine Freigabe, keine neue Persistenz |
| `ConfigurationUnavailable` / `0x0102` | `ConfigurationRecoveryService` | `SAFE_BOOT` | nein; neuer Boot-/Revalidate-Pfad mit gültiger Config | nur Anzeige/Journaling | all-off, kein Resume |
| `ConfigurationIntegrityFailure` / `0x0103` | `ConfigurationRecoveryService` | `SAFE_BOOT` | nein; gültige Integritätsprüfung im bestehenden Producervertrag | nur Anzeige/Journaling | all-off, kein Resume |
| `ConfigurationCommitIndeterminate` / `0x0104` | `ConfigurationService`/Recovery: nicht aufgelöster Commit | `SAFE_BOOT` | nein; bestehende Commit-/Recovery-Evidenz muss eindeutig gültig sein | nur Anzeige/Journaling | keine neue Safety-Persistenz |
| `RunPersistenceUntrusted` / `0x0201` | #17 Load-/Coordinator-Status ohne vertrauenswürdigen kanonischen Zustand | `SAFE_BOOT` | nein; bestehender #17-Zustand bleibt maßgeblich | nur Anzeige/Journaling | kein Tombstone, kein Resume |
| `SafetySensorUnavailable` / `0x0301` | #20/#21 echte `STALE`/`FAILED`- bzw. gesperrte Safety-Projektion | `Blocked/ImmediateStop` | nur frische #20/#21-Qualitäts- und Auswahlvalidierung | nie Safety-Clear | kein automatisches Resume |
| `ActuatorRequestWatchdog` / `0x0401` | bestehender #23 Request-Watchdog | `Blocked/ImmediateStop` | kein Auto-Clear; nur expliziter Reset im selben Boot über den bestehenden Pfad nach frischer Evidenz | nie Safety-Clear | RAM-Latch darf verloren gehen, Boot bleibt trotzdem all-off |
| `SystemProducerUnknown` / `0x0501` | unbekannter oder nicht abbildbarer Producerzustand | `SAFE_BOOT` | nein; Ursache muss durch aktuellen validierten Pfad aufgelöst werden | nur Anzeige/Journaling | keine implizite Freigabe |

`ConfigurationRuntimeFailure` wird als Laufzeitblock behandelt; die übrigen
technisch untrusted Zustände bleiben im `SAFE_BOOT`-Pfad. Ein technisch
integerer, aber fachlich nicht einfach resumefähiger Run ist dagegen kein
FaultCode: er wird kanonisch als `NoActiveRun` beendet.

Stabile Wire-Werte werden nur für diese begrenzte R1-Menge festgelegt. Weitere
technische Unterursachen bleiben strukturierte Detailursache des kanonischen
Producers. Unbekannte Producerzustände mappen deterministisch auf
`SystemProducerUnknown` und bleiben fail-closed.

Quittierung darf nur Meldung, Akustik oder Anzeigezustand ändern. Sie löscht
keinen Fault, setzt keinen Watchdog zurück und kann niemals `Allowed` erzeugen.
Safetyentscheidung und Gatewirkung erfolgen vor Journal und Notification.
Ein Fehler von `IEventJournal::record()` oder
`IUserNotificationSink::notify()` verhindert keine Abschaltung, rollt keine
Safetyentscheidung zurück und erzeugt keinen zweiten Safetyzustand.

## 5. Sensor- und Watchdog-Verhalten

`SensorQuality::Stale` oder `Failed` in der für den Peltierpfad relevanten
Projektion sperrt die Freigabe und führt bei laufender Freigabe zu
`ImmediateStop`. Eine Rückkehr ist nur über die vorhandene #20/#21-
Qualitäts-, Auswahl- und Rückkehrvalidierung zulässig. Produktfühler-
Fallback bleibt vollständig #21; #24 führt weder Auswahl- noch
Fallbackzustandsmaschine.

Der bestehende #23-Request-Watchdog wird als Current-Boot-RAM-Latch behandelt:

- Trip setzt den vorhandenen Latch und erzwingt `ImmediateStop`.
- Eine neue Request und eine Quittierung löschen ihn nicht.
- Ein expliziter fachlicher Reset nutzt ausschließlich
  `applyExternalWatchdogFaultReset()` und ist nur nach frischer gültiger
  Safety-/Planner-Evidenz im selben Boot zulässig.
- Ein Reboot darf den RAM-Latch verlieren, schreibt dafür aber keinen neuen
  Safety-Key. Der anschließende Boot bleibt all-off und durchläuft die
  vollständige Revalidierung ohne automatisches Resume.

Normales #23-Request-Timing, Deadtime und Fanverhalten bleiben unverändert.
Automatische `SAFETY_RECOVERY`-Gegenrichtung wird nicht eingeführt.

## 6. Run nach Neustart und Resume

### 6.1 Load-/Boot-Matrix

Die Implementierung verwendet die vorhandenen
`RunPersistenceLoadStatus`- und Coordinator-Zustände; es entsteht keine zweite
Load-FSM:

| Realer #17-Fall | R1-Ausgang |
|---|---|
| `NoPersistedRun` oder `NoActiveRun` | sicherer `Standby`, kein `Allowed` |
| gültiger `Current` mit `Completed` | bestehende Completed-Projektion erhalten, kein Resume und keine Aktorfreigabe |
| gültiger aktiver `Current`, vollständiges R1-Prädikat | nicht freigebendes Resume-Angebot (`RecoveryEvaluation`) |
| technisch integerer `Current`, semantisch nicht einfach resumefähig | kanonischer `NoActiveRun`-Abschluss über Write-before-Apply |
| integerer `Current` mit alter/offener komplexer Recovery-Evidenz | kein komplexer Resume; vertrauenswürdig geladen -> `NoActiveRun` |
| historischer `Current` in `Fault` | keine Recovery-Aktivierung; Diagnose erhalten, konservativer terminaler/Abbruchpfad ohne Freigabe |
| `FallbackRecovered` oder `FallbackRecoveryPending` | technischer Degradations-/Integritätsfall; `SAFE_BOOT`, kein Resume und keine Promotion |
| `PreparedInterrupted` | `SAFE_BOOT`/blocked, keine automatische Mutation zum Verbergen des Zustands |
| `NotReconstructible` oder `NotReconstructibleOrphanedState` | `SAFE_BOOT`, kein Tombstone |
| `ReadFailed`, `CapacityExceeded`, `UnsupportedSchema`, `ForeignEpoch` | `SAFE_BOOT`, kein Resume |
| technisch beschädigter Current ohne vertrauenswürdigen kanonischen Zustand | `SAFE_BOOT`, nicht blind als `NoActiveRun` überschreiben |

Ein gültiger Current darf resumefähig sein, auch wenn der bewusst ältere
Fallback eine andere `RunRevision`, `TransitionSequence` oder Projektion
enthält. Current und Fallback werden nicht verglichen. Der Fallback ist kein
alternativer Resume-Kandidat und wird nie promotet.

### 6.2 Exaktes R1-Resume-Prädikat

`schemaVersion == 3` und reguläre neutrale/default Schema-3-Felder sind kein
Ablehnungsgrund. `RecoveryTemperatureEvidence` und `RunProgressState` werden
nur dann als blockierend bewertet, wenn sie semantisch eine offene alte
Recoveryentscheidung oder benötigte Zeit-/Progressgutschrift repräsentieren.
Alte Zeit- und Progresswerte erzeugen keine R1-Gutschrift.

Die zulässige R1-Fortsetzung ist vor Implementation festgelegt:

| `ProcessState` | R1-Resume | Ziel / Zeitbasis |
|---|---|---|
| `Preheating` | ja, falls aktuelle Config/Sensor-/Planner-Evidenz vollständig gültig | `Preheating`; bootlokale Zielprüfung neu starten, keine alte Zeitgutschrift |
| `WaitingForProduct` | nein, wenn die alte maximale Wartezeit für die Entscheidung benötigt würde | `NoActiveRun`; keine alte Wartezeit schätzen |
| `ReachingTarget` | nein, wenn der alte Reach-Timer erforderlich wäre | `NoActiveRun`; Target-Reach-Timer nicht fortschreiben |
| `QualifyingTarget` | nein | `NoActiveRun`; Qualifikationszeit vollständig neu beginnen statt alte monotone Basis zu verwenden |
| `Fermenting` | nein, sobald Dauer oder `PriorBootPhaseElapsed` für die Restzeit benötigt würden | `NoActiveRun`; keine gewichtete Progress-/UTC-Ausfallrechnung |
| `Cooling` | ja, falls Ziel-/Sensorfortsetzung mit frischer Evidenz eindeutig ist | `Cooling`; bootlokale Prüfungen neu beginnen, keine alte Zeitgutschrift |
| `CoolHolding` | nein, wenn die alte Haltedauer benötigt würde | `NoActiveRun`; keine alte Haltedauer gutschreiben |
| `ManualHolding` | ja, sofern die Fortsetzung nicht von alter Dauer abhängt | `ManualHolding`; keine implizite Zeitgutschrift |

Nur die mit „ja“ markierten Zustände bieten ein Resume an. Das Angebot bleibt
`RecoveryEvaluation`, nicht freigebend: kein `Allowed`, kein Aktorcommand und
Planner/Sink bleiben `Unresolved`/Idle. Ein expliziter Resume-Befehl benötigt
erneut aktuelle Config-, Sensor-, Safety- und Planner-Evidenz; erst danach
folgt der persistente Write-before-Apply-Pfad und anschließend die FSM-
Aktivierung.

Benutzerablehnung, Timeout oder ein nicht rekonstruierbarer, aber technisch
vertrauenswürdiger Current verwenden die vorhandenen
`RecoveryReject`/`RecoveryRejected`-Semantiken in der neuen Topologie:

1. Aktoren bleiben aus.
2. Detached-Kandidat `NoActiveRun` über den bestehenden #17-Pfad bilden und
   persistieren.
3. Nur bei definitivem Gesamtstatus `Applied` den RAM-/FSM-Zustand nach
   `Standby` anwenden.
4. Bei `PersistenceIndeterminate`, Blocked oder sonstigem nicht definitivem
   Gesamtstatus keinen normalen Standby als dauerhafte Wahrheit behaupten;
   `SAFE_BOOT`/unknown-safe gemäß #17 beibehalten.

### 6.3 Persistenzsemantik

Einzel-Key-Write und Run-Persistence-Gesamttransaktion sind getrennt.
`IStateStore::Success` bedeutet dauerhaft geschrieben und benötigt keinen
zweiten Readback. `WriteError` und `CapacityError` bedeuten nur für diesen
Key-Write sicher nicht geschrieben. Nur `CommitOutcomeUnknown` wird durch den
bestehenden `RunPersistenceStore::writeExact()`-Readback auf `Written`,
`NotWritten` oder `Indeterminate` aufgelöst.

Der bestehende nichtperiodische #17-Coordinator bleibt die einzige
Gesamttransaktion:

`PreparedHead -> CheckpointSlot -> CommittedHead`

| Transaktionspunkt | Durability | R1-Folge |
|---|---|---|
| Fehler beim PreparedHead, sicher keine Mutation | alter Zustand bzw. rollbackfähiger Coordinator-State gemäß #17 | kein RAM-/FSM-Apply |
| PreparedHead indeterminate | unbestimmt | `BlockedIndeterminate`/`PersistenceIndeterminate`, kein Apply |
| PreparedHead definitiv geschrieben, danach Slot-Fehler | Teiltransaktion dauerhaft vorhanden | kein Apply; alten Gesamtzustand nicht als sicher autoritativ behaupten; bestehendes blocked/indeterminate Verhalten |
| PreparedHead und Slot geschrieben, danach CommittedHead-Fehler | Teiltransaktion dauerhaft vorhanden | kein Apply; bestehendes blocked/indeterminate Verhalten |
| vollständiger Coordinator-Pfad `Applied` | Zielzustand vollständig abgeschlossen | erst jetzt detached Kandidat in RAM/FSM anwenden |

Ein späteres `WriteError`, `CapacityError` oder `NotWritten` darf deshalb eine
bereits geschriebene Prepared-/Slot-Phase nicht wegdefinieren. #24 baut keine
Transaktions- oder Readback-Engine.

## 7. E3/E5- und #106-Grenze

Alle E3-Ausgänge bleiben bis E5 abstrakte Aktorbefehle. #24 beweist an den
vorhandenen Planner-/Sink-Ports die Reihenfolge „Gate `Unresolved` -> Idle/Stop
-> vollständige Validierung -> gegebenenfalls explizite Freigabe“. Es
initialisiert keine realen GPIOs und behauptet keine physischen Pegel.

GPIO-Level, BTS7960 Enable/PWM, MOSFET-Pins, Lüfterausgänge sowie reale
Power-on-/Brownout-Nachweise bleiben E5/#29/#32/#33. Nicht implementierte
Thermal-/Hardwaregrenzen und `TBD_HARDWARE`/`TBD_COMMISSIONING` werden in #24
nicht als Laufzeitwerte oder Tests erfunden.

## 8. Dokumentationsschnitt

Im Umsetzungsscope werden widersprüchliche Release-1-Aussagen gezielt und
kanonisch bereinigt in:

- `docs/SAFETY_AND_FAULTS.md`
- `docs/SAFETY_COMPONENT_FAULTS.md`
- `docs/SYSTEM_SAFETY_AND_RECOVERY.md`
- `docs/STATE_MACHINE.md`
- `docs/DIAGNOSTICS_AND_MAINTENANCE.md`
- `docs/RUNTIME_BEHAVIOR.md`
- `docs/ACTUATOR_TIMING.md`
- `docs/RUN_PERSISTENCE.md`
- `docs/RECOVERY_AND_INTERRUPTION.md`
- `docs/ACCEPTANCE_TESTS.md`
- `docs/REQUIREMENTS.md`
- `docs/ARCHITECTURE.md`
- `docs/CONFIGURATION_PERSISTENCE.md`
- `docs/IMPLEMENTATION_ISSUES.md`

Zu entfernen oder auf den R1-Vertrag zu korrigieren sind dort, soweit sie
#24 betreffen: generischer persistenter Safety-Latch, Restart-Akkumulation und
Resetzeitfenster, Service-PIN-Pflicht, kontrollierter automatischer Restart,
automatische thermische `SAFETY_RECOVERY`, persistenter Watchdog-Latch über
Reboot sowie Charge-Rettungslogik. Die bestehenden allgemeinen #17-,
Device-Platform-, #20/#21- und #23-Verträge bleiben erhalten.

`docs/RESOURCE_BUDGET_AND_MAINTENANCE.md` bleibt verbindliche Quelle für
statische/fest begrenzte Safety-Daten, fehlende Fault-Historie, fehlendes
unkontrolliertes String-/Heap-Wachstum und das 4-MB-/No-PSRAM-Budget.
`docs/DECISIONS.md`/ADR-013 sowie `docs/SPECIFICATION_REVIEW.md` bleiben
Architektur- und Prioritätsquellen.

## 9. R1-Testvertrag

Der Plan umfasst nur gezielte Native-/Port-/Anwendungstests für reale R1-
Pfade; Firmware-Builds und Hardwarebeweise sind dadurch nicht bestanden.
Mindestens nachzuweisen sind:

- Boot all-off, Gate `Unresolved`, abstrakter Sink Idle/Stop und kein
  automatisches `Allowed` für jede ResetCause-Kategorie einschließlich
  `Unknown/Other`;
- `NoPersistedRun`/`NoActiveRun` -> Standby;
- gültiger resumefähiger Current -> `RecoveryEvaluation` ohne Freigabe;
- jede Zeile der Resume-Phasenmatrix, einschließlich Verwerfung zeit-/
  progressabhängiger Phasen;
- integerer nicht resumefähiger Run -> `NoActiveRun`, technisch untrusted
  Load -> `SAFE_BOOT` ohne Tombstone;
- gültiger Current mit älterem legitimen Fallback -> Current wird unabhängig
  vom Fallback bewertet; defekter Current mit Fallback -> kein Fallback-Resume;
- alle realen Load-Status und der historische `Completed`-/`Fault`-Fall;
- Einzel-Write: `Success` ohne zweiten Readback, Write-/Capacity-Fehler,
  Unknown mit neuem/alten/fremdem Wert oder Readfehler;
- Gesamttransaktion: PreparedHead sicher nicht geschrieben, PreparedHead
  indeterminate, Slot-Fehler nach PreparedHead, CommittedHead-Fehler nach
  PreparedHead+Slot und vollständiger `Applied`-Pfad;
- reale Configuration-Producer fail-closed und deterministisch auf die
  Fault-Matrix projiziert; unbekannter Producer -> `SystemProducerUnknown`;
- #20/#21 `STALE`/`FAILED` -> Stop, frische Rückkehr nur über bestehenden
  Qualitäts-/Auswahlvertrag;
- #23 Watchdog-Trip -> Stop, neue Request/Ack löschen ihn nicht, expliziter
  Reset nur mit gültiger Evidenz, Reboot ohne persistente Watchdog-Persistenz;
- Ack, Journalfehler und Notificationfehler verändern nie Gate, Clear oder
  Abschaltung; SafetyCore besitzt keine wachsende Fault-Historie;
- keine neue Safety-Persistenz, kein Restart-Zähler, kein Resetzeitfenster,
  kein Service-PIN und keine Thermal-/Hardwarefault-Tests in #24;
- E3/E5- und #106-Negativgrenzen: kein physischer Aktorbeweis und kein
  produktiver `Allowed`-Bypass.

## 10. Umsetzungsschnitt und Abschlusskriterien

Die Umsetzung erfolgt in dieser Reihenfolge:

1. bestehende Producer-/Port-Projektionen und die begrenzte FaultCode-Menge
   implementieren;
2. Boot-/ResetCause-Revalidierung und zentrale SafetyCore -> Planner -> Sink-
   Kette anschließen;
3. #20/#21-Sensorprojektion und den bestehenden #23-Resetpfad anbinden;
4. #17-Load-Matrix, Resume-Prädikat und Write-before-Apply mit dem vorhandenen
   Coordinator verwenden;
5. die genannten kanonischen Dokumente synchronisieren und die gezielten R1-
   Tests ergänzen.

Fertig ist #24 erst, wenn kein Safety-Pfad bei Boot, Fehler, untrusted
Persistenz, fehlender Sensorqualität oder Watchdog-Latch `Allowed` liefert,
die echte #17-Gesamttransaktion korrekt als Ganzes behandelt wird, die
FaultCode-Matrix deterministisch ist und #106/E3/E5 nicht umgangen werden.

Bis zur Ownerfreigabe bleiben PR #110 Draft und `Implementation: NOT_STARTED`.
