# Issue #24 – Safety-Core, Verriegelung, Restart, S3-Recovery und Y4-Terminal

Status: **PLANREVISION – NOCH NICHT ZUR UMSETZUNG FREIGEGEBEN**

Issue: #24 – `[E3.5] Fehlerklassen, Verriegelung, SAFE_BOOT und Fehlerinjektion`

Branch: `agent/issue-24-safety-core-clean-restart`

Draft-PR: #108

Live-Basis bei Erstellung dieser vollstaendigen Planfassung:

```text
main @ b8eae5f4da5f2666b5a9bda333d115254c4db5b2
zu ersetzende Plan-SHA:
33f7394678ac73abd083aa38b95f743ebfa13f94
```

Diese Datei ersetzt **alle frueheren Issue-#24-Planfassungen vollstaendig**.
PR #107 und fruehere Plan-SHAs sind nur historische Lernreferenzen und keine
normative Implementierungsquelle.

Diese Fassung setzt eine verbindliche, nicht mehr offene Ownerentscheidung um
(Abschnitt 5.3): **S3 ist `RecoverIfProvable`, Y4 ist `Terminal`.** Die
vorherige Fassung (`33f7394`) hatte jeden S3/Y4-Fault pauschal terminal
gemacht; das war zu grob und wird hiermit ersetzt.

---

# 1. Ziel

Issue #24 implementiert den Release-1-Safety-Core als zentrale Autoritaet fuer:

- vier Fehlerklassen P1/O2/S3/Y4;
- stabile maschinenlesbare FaultCodes;
- voneinander unabhaengige gleichzeitig aktive Ursachen;
- unmittelbare sichere Aktorreaktionen;
- automatische Wiederfreigabe nur fuer explizit erlaubte P1/O2-Faelle;
- persistente S3/Y4-Verriegelungen mit vollstaendig von P1/O2 getrennten
  Identitaets- und Revisionsraeumen;
- Trennung von Quittierung, Cause-Clear und Reset;
- Primaer-/Follow-up-Beziehungen;
- **S3: `RecoverIfProvable`** – nach geschuetztem Reset wird ein vorher
  laufender Fermentationslauf nur dann fortgesetzt, wenn die bestehende
  #18-Recovery ueber einen kontrollierten Restart beweisen kann, dass er
  sicher rekonstruierbar ist; sonst terminal;
- **Y4: `Terminal`** – der vorherige Lauf wird nach Y4 in Release 1 nie wieder
  aufgenommen;
- einen autorisierten Run-Abandon-/Terminalisierungspfad fuer Runs, die beim
  Laden nicht sicher rekonstruierbar waren, aber deren Speicher spaeter
  wieder eindeutig les-/schreibbar ist;
- redundante Safety-Persistenz mit vollstaendig normiertem Wireformat;
- getrennten minimalen EmergencyMarker;
- reale Integration der vorhandenen #20/#21/#22/#23/#56/#57-Producer;
- deterministische Fault-Injektion;
- typisierte, in ihrer Anzahl pro Mutation bewiesen begrenzte SafetyEvents
  als spaeteren Input fuer #19;
- ein zentrales Aktor-Safety-Gate ohne caller-supplied Freigabe.

Fail-closed ist die Grundregel. Kein zweiter Recoveryalgorithmus, kein
zweiter Persistenzkern.

---

# 2. Verbindliche Quellen und vorhandene Vertraege

Vor Umsetzung und Review sind mindestens zu verwenden:

| Bereich | bestehende Quelle |
|---|---|
| Prozesszustand | #14 / `process_state_machine.*`, `STATE_MACHINE.md` |
| Commands | #15 / `run_commands.*`, `RUN_COMMANDS.md` |
| Run-Persistenz | #17 / `RunPersistenceCoordinator`, `RUN_PERSISTENCE.md` |
| Recovery | #18 / `RunRecoveryCoordinator` (duenner Adapter um `RunPersistenceCoordinator`), `RECOVERY_AND_INTERRUPTION.md` |
| Recovery-Schreibpfad | `RunPersistenceCoordinator::persistRecoveryCandidate()` – bereits vorhandener, fuer genau diesen Zweck dokumentierter write-before-apply Recovery-Schreibpfad (`RunPersistenceMutationKind::Recovery`) |
| Sensorqualitaet | #20 / `SensorQualitySnapshot` |
| Sensorselektion | #21 / `SensorSelectionRuntimeState` |
| PI / Control | #22 / `TemperatureControlResult` |
| Planner / Watchdog / Fanlogik | #23 / `ActuatorPlanner` |
| Configuration Runtime | #56 / `ConfigurationService` |
| Configuration Boot/Recovery | #57 / `ConfigurationRecoveryService` |
| Persistenzport | `device_platform::IStateStore` |
| Wireformat | `StorageEnvelope`, Big-Endian-Codecs, CRC |
| Slotpruefung | `scanTechnicalSlotCandidates()`, `loadSlotPayload()` |
| Zeit | `device_platform::ITimeSource` |
| spaeteres Journal | #19 |

Repositoryregeln aus `AGENTS.md`, den lokalen `AGENTS.md`,
`AGENT_WORKFLOW.md`, `ENGINEERING_PRINCIPLES.md`, `SPECIFICATION_REVIEW.md`
und ADR-013 bleiben bindend.

## 2.1 Live-Codefakten, die diese Planfassung tragen

Vor Erstellung dieser Fassung wurden folgende Codefakten verifiziert und sind
fuer die Architektur in Abschnitt 5.3/29 tragend:

- `RunPersistenceCoordinator::activateLoadedRun()` bricht sofort ab, wenn der
  geladene Snapshot `processState.state == ProcessState::Fault` ist
  (`run_persistence_coordinator.cpp:515-526`) und geht **nicht** in die
  Hop-1/Hop-2-Recoverylogik. Dieser Guard ist laut
  `docs/RUN_PERSISTENCE.md:412-413` bewusst so vorgesehen ("ausserhalb des
  bewusst abgelehnten aktiven Fault-Falls"). Es gibt **keinen** In-Prozess-Weg
  von `Fault` in eine normale Recovery-Auswertung.
- `PendingRecoveryAnchor`/`recoveryBootAnchorMonotonicMillis` werden
  ausschliesslich innerhalb von `activateLoadedRun()`/
  `activateFallbackRecoveredRun()` gesetzt, die wiederum ausschliesslich ueber
  `loadAndInitialize()` erreichbar sind (`run_persistence_coordinator.cpp:257,
  443, 483`). Recovery ist strukturell **an den Boot-Pfad gebunden**, nicht
  nur "noch nicht verdrahtet".
- `loadAndInitialize()` hat aktuell **keinen** Aufrufer in Produktionscode
  (nur in `test/test_run_persistence_coordinator/`). Der komplette
  Boot-Orchestrator (der `#17`/`#18` tatsaechlich verdrahtet) existiert noch
  nicht — das zu bauen ist explizit Scope von #24 (`SafetyProcessCoordinator`,
  Abschnitt 6.5).
- `RunPersistenceCoordinator::persistRecoveryCandidate()`
  (`run_persistence_coordinator.hpp:225-233`, Implementierung
  `run_persistence_coordinator.cpp:1823-1858`) ist bereits ein generischer,
  fuer genau diesen Zweck dokumentierter Schreibpfad: der Aufrufer liefert
  einen vollstaendigen Kandidaten mit `runRevision = current.runRevision + 1`,
  die Funktion validiert, baut den Snapshot und schreibt ueber den
  bestehenden `writeSnapshotCore()` mit `RunPersistenceMutationKind::Recovery`
  — laut eigenem Kommentar entsteht dabei **kein zweiter Schreibkern**.
- „Readback" im Sinn eines synchronen Read-after-Write existiert im
  bestehenden `#17`-Vertrag **nicht** (`grep -ni readback` in
  `run_persistence_coordinator.cpp`/`run_persistence_store.cpp` ist leer).
  Die vorhandene Integritaetsgarantie ist Compare-and-Swap beim Schreiben
  (`writeSlotExact`/`writeHeadExact` gegen den zuvor gelesenen Altwert) plus
  physische Envelope-/CRC-Revalidierung beim naechsten Boot. Diese Planfassung
  uebernimmt fuer alle `#17`-Schreibpfade (inklusive Run-Abandon, Abschnitt
  26) **exakt dieses bestehende Modell** und erfindet keine zweite
  Verifikationsebene nur fuer #24. Nur die **neuen**, in Issue #24 selbst
  eingefuehrten Stores (`SafetyStateStore`, `SafetyEmergencyMarkerStore`,
  Abschnitte 16-20) verwenden synchronen Readback — das ist eine bewusste
  Abweichung zwischen einem neuen und einem wiederverwendeten Store, kein
  Widerspruch.
- `ProcessRuntimeState` (`process_state_machine.hpp:72-79`) enthaelt `state`,
  `stateEnteredAtMillis`, `targetReachStartedAtMillis`,
  `qualificationValidSinceMillis`, `targetReachWarningIssued`,
  `transitionSequence`. Dieser vollstaendige Wert (nicht nur das
  `ProcessState`-Enum) muss beim Uebergang in `Fault` erhalten bleiben, damit
  eine spaetere Korrektur ihn 1:1 wiederherstellen kann (Abschnitt 29.2).

---

# 3. Adopt-or-build / Bibliotheken

Es wird **kein neues Fault-, FSM-, Event-Bus- oder Persistence-Framework**
eingefuehrt.

Vorhandene technische Primitive werden wiederverwendet:

- `IStateStore`;
- `StateStoreKey`;
- `StorageEnvelope`;
- Slot-Scan und Payload-Load;
- Big-Endian-Codecs;
- CRC;
- `ITimeSource`;
- bestehende Run-/Config-/Sensor-/Control-/Planner-Vertraege;
- `RunPersistenceCoordinator::persistRecoveryCandidate()` als bestehender
  Recovery-Schreibpfad fuer die S3-Korrekturbuchung (Abschnitt 29.2).

Die projektspezifische Fault-/Reset-/SAFE_BOOT-Policy bleibt im
`fermentation_app`-Fachkern.

Fuer einen spaeteren realen ESP-IDF-Resetadapter wird die native ESP-IDF-API
verwendet. Dieser Adapter ist **nicht** Scope von Issue #24.

Neue Drittanbieter-Lizenzabhaengigkeiten entstehen nicht.

---

# 4. Nicht-Scope

Issue #24 implementiert nicht:

- ESP-IDF-Resetadapter;
- `esp_restart()`-Adapter;
- NVS-Adapter (#90);
- reale GPIO-/BTS7960-/Fan-/Sensoradapter;
- Fan-Tachometer;
- Strommessung;
- thermische Commissioningwerte aus #35;
- produktive Service-PIN-/Loginlogik;
- Webtransport;
- OTA;
- Journalpersistenz/Retention/Export/Import aus #19;
- allgemeine Event-Busse;
- eine neue allgemeine Recovery-State-Machine (die bestehende `#18`-Logik
  wird fuer S3-Recovery unveraendert wiederverwendet, siehe Abschnitt 29);
- ein Schema-4-Fault-Recoverymodell.

Physische Fan-/Aktorfehler ohne realen Producer bleiben ehrlich
**injection-only**.

---

# 5. Release-1-Entscheidungen

## 5.1 Restart-Episode

```text
3 abnormale Boots innerhalb derselben Restart-Episode
=> Y4-005 + SAFE_BOOT
```

Vor Erreichen von 3 endet eine Restart-Episode nach:

```text
30 Minuten ununterbrochener stabiler Uptime
```

mit:

- keinem neuen abnormalen Reset;
- keinem aktiven S3/Y4;
- keinem offenen RestartIntent;
- gesunder Safety-Persistenz.

Die Zeit wird nur innerhalb des aktuellen Boots mit `monotonicMillis()`
gemessen. Keine monotone Zeit wird ueber Reboots hinweg subtrahiert.

PowerOn/External-Reset ohne offenen RestartIntent erhoeht den abnormalen
Zaehler nicht, loescht ihn aber auch nicht automatisch. Brownout,
Watchdog/Panic, unbekannte Resetursache und unerwarteter Software-Reset sind
abnormal.

Ein von #24 vorbereiteter kontrollierter Software-Restart (sowohl fuer
S3-Recovery aus Abschnitt 5.3 als auch fuer andere restart-eligible Faelle)
zaehlt als Teil der abnormalen Restart-Episode. Er darf pro ausloesender
Faultinstanz genau einmal automatisch angefordert werden.

## 5.2 Y4-005-Requalifikation

Bei `abnormalRestartCount == 3` bleibt Y4-005 gelatcht.

Nach 30 Minuten stabiler Uptime **auch innerhalb SAFE_BOOT** darf
ausschliesslich dessen Ursache:

```text
causeCleared = true
```

werden. Der Counter bleibt 3 und der Latch bleibt aktiv.

Erst ein erfolgreicher geschuetzter Reset von Y4-005:

```text
abnormalRestartCount = 0
Fault entfernen
```

Danach bleibt `SAFE_BOOT` bestehen, bis der separate SAFE_BOOT-Exit
erfolgreich ist. (Y4-005 ist wie jedes Y4 `Terminal` — kein Run-Resume.)

## 5.3 S3 = `RecoverIfProvable`, Y4 = `Terminal` (verbindliche Ownerentscheidung)

Diese Entscheidung ist getroffen und **kein offenes Owner-Gate**.

### S3

```text
S3 -> FAULT
CauseClear
-> geschuetzter Reset
-> Reset committet (Latch entfernt)
-> war ein aktiver Run vorhanden?
     nein: sofort FAULT -> STANDBY (wie Y4, siehe unten)
     ja:   SafetyProcessCoordinator korrigiert den persistierten
           #17-Run-Snapshot zurueck auf die vor dem Fault erhaltene
           ProcessRuntimeState (Abschnitt 29.2), fordert danach GENAU
           EINEN kontrollierten Restart an
     -> naechster Boot: normaler, unveraenderter #18-Bootpfad
        (loadAndInitialize -> activateLoadedRun/activateFallbackRecoveredRun)
        wertet den Run wie jede gewoehnliche unkontrollierte Unterbrechung
        aus
          -> #18 kann sicher rekonstruieren: Hop-1/Hop-2 Resume
          -> #18 kann nicht sicher rekonstruieren: #18s eigene bestehende
             Terminalisierung (z. B. `clearActiveRunState()` beim
             WaitingForProduct-Expiry-Pfad) bzw. der Run-Abandon-Pfad aus
             Abschnitt 26, falls #18 den Snapshot als nicht rekonstruierbar
             einstuft
```

Es gibt **keinen zweiten Recoveryalgorithmus**. #24 ruft #18s bestehende,
unveraenderte Logik nur ueber einen echten, kontrollierten Neustart auf —
das ist der einzige mit dem bestehenden Guard in `activateLoadedRun()`
kompatible Weg (Abschnitt 2.1). Es gibt keine automatische S3-Wiederfreigabe
ohne diesen Reboot und keine erfundene Recovery.

### Y4

```text
Y4 -> FAULT / SAFE_BOOT
Ursache beheben
-> causeCleared
-> geschuetzter Reset
-> alter Lauf terminal ueber clearActiveRunState()
-> FAULT -> STANDBY (bzw. SAFE_BOOT bleibt bis separatem Exit)
```

Kein `RecoverIfProvable` fuer Y4. Kein Restart-fuer-Recovery-Zweck.

### Kein zweiter Recovery-Vertrag

Die bestehende #18-Recovery bleibt die einzige fachliche Recoveryautoritaet.
Ein blosses `FAULT -> RECOVERY_EVALUATION` ohne die von #18 verlangten Anker
und Evidenzen (`pendingRecoveryAnchor`, `recoveryBootAnchorMonotonicMillis`)
bleibt verboten — genau das war der CRITICAL-2-Fehler der vorherigen
Planfassung `f3b034b`. Diese Fassung loest S3-Recovery deshalb ausschliesslich
ueber einen echten Reboot und die dadurch unveraendert erreichbare
Standard-#18-Logik.

## 5.4 SAFE_BOOT beendet einen noch nicht reaktivierten Lauf

Wenn beim Boot `SAFE_BOOT` erforderlich ist:

- ein geladener aktiver Run wird nicht ueber #18 reaktiviert;
- sobald der Run-Store eindeutig les-/schreibbar ist, wird er ueber den
  Run-Abandon-Pfad (Abschnitt 26) als `NoActiveRun/STANDBY` terminalisiert;
- ist der Run-Store indeterminiert, bleibt das Geraet in SAFE_BOOT und der
  Exit bleibt gesperrt.

Damit ist `SAFE_BOOT -> STANDBY` eindeutig und kann keinen alten Run
automatisch wieder aufnehmen.

## 5.5 SafetyStorageEpoch

Schema 1 verwendet:

```text
SafetyStorageEpoch = 1
```

unabhaengig von der Configuration-`StorageEpoch`. Ein Configuration-Werksreset
darf Safety-Latches nicht implizit unlesbar machen oder loeschen. Eine
spaetere explizite Safety-Epoch-Aenderung benoetigt eigenen Scope.

## 5.6 Persistente S3/Y4- und transiente P1/O2-Identitaet sind vollstaendig getrennt

Persistente Identitaet (`nextInstanceId`, `faultRevision`) gilt
**ausschliesslich** fuer rebootrelevante S3/Y4-Latches. P1/O2 erhalten einen
eigenen, rein RAM-lokalen Identitaets-/Revisionsraum, der nie persistiert
wird und nie denselben Zaehler wie S3/Y4 konsumiert.

**Begruendung (Sicherheitsrelevanz, nicht nur Kosmetik):** `faultRevision`
ist der Staleness-Anker fuer `FaultResetEvaluation`/`decideFaultReset()`
(`CommandEnvelope.expectedFaultRevision`). Wuerde ein gemeinsamer Zaehler
durch haeufige P1/O2-Ereignisse (z. B. flatterndes Sensor-STALE) waehrend der
Laufzeit erhoeht, aber nach einem Reboot nicht mitpersistiert, koennte
`faultRevision` nach dem Reboot **zurueckspringen**. Eine alte, laengst
ueberholte Command-Entscheidung eines Clients koennte dann post-reboot
zufaellig wieder exakt matchen, obwohl der tatsaechliche Faultzustand ein
voellig anderer ist — ein Staleness-Check-Bypass. Details in Abschnitt 15.

---

# 6. Architektur und Verantwortungen

## 6.1 FaultCore – reine Policy

`FaultCore` ist hardwarefrei, persistenzfrei, deterministisch, nativ testbar,
ohne dynamische Allokation.

Er besitzt:

- **zwei getrennte** Faultzustaende: `PersistentFaultState` (S3/Y4, mit
  `nextInstanceId`/`faultRevision`) und `TransientFaultState` (P1/O2, mit
  eigenem, nie persistiertem `transientNextInstanceId`/
  `transientFaultRevision`);
- FaultIdentity je Instanz;
- Cause-Clear;
- Primary-/Follow-up-Bezug (nur innerhalb derselben Klasse: S3/Y4 referenzieren
  S3/Y4, P1/O2 referenzieren P1/O2 — kein klassenuebergreifender Primary);
- `restartAttempted` (nur S3/Y4);
- Hauptfaultauswahl (klassenuebergreifend: Y4 > S3 > O2 > P1);
- Policyaggregation aus dem compile-time Katalog (Abschnitt 11).

Er schreibt keinen Store und ruft keine Hardware.

## 6.2 SafetyStateStore – nur persistenter Safetyrecord (S3/Y4)

Verantwortung: Keys, Envelope, Codec, Slot-Scan, Readback,
CommitOutcomeUnknown, Redundanzpruefung, geschuetzte Reparatur. Keine
Faultklassifikation. Kennt nur `PersistentFaultState`.

## 6.3 SafetyEmergencyMarkerStore

Getrennter kleiner Storepfad fuer normalen SafetyState-Commitfehler,
unbestimmten Safety-State, Counter-/Sequence-Exhaustion. Keine normale
Faultliste.

## 6.4 SafetyFaultService – eine mutable Safety-Autoritaet

`SafetyFaultService` besitzt: `FaultCore` (beide Teilzustaende),
geladenen/persistierten SafetyState, RestartEpisode, RestartIntent,
`safeBootRequired`, Storagequalification, Resetbewertung, Resetcommit,
SafetyMutationResults. Es ist die **einzige** mutable Fault-/Latch-/Reset-
Autoritaet.

## 6.5 SafetyProcessCoordinator – Cross-Domain-Sequenzierung, S3-Recovery-Handoff, Run-Abandon

Ein kleiner `SafetyProcessCoordinator` koordiniert:

- Bootreihenfolge;
- Run-Persistenzstatus;
- Runtime-`FAULT`-Eintritt (inkl. Erhalt der `ProcessRuntimeState` vor dem
  Fault fuer eine spaetere S3-Korrekturbuchung, Abschnitt 29.2);
- **S3-Recovery-Handoff**: nach erfolgreichem S3-Reset mit aktivem Run die
  `#17`-Korrekturbuchung ueber `persistRecoveryCandidate()` sowie die
  Restart-Anforderung (Abschnitt 29.2);
- terminalen `FAULT -> STANDBY`-Pfad (Y4 sowie S3 ohne aktiven Run);
- SAFE_BOOT-Eintritt;
- **Run-Abandon** (Abschnitt 26): terminales Beenden eines geladenen, nicht
  sicher rekonstruierbaren Runs, sowohl bei SAFE_BOOT-Eintritt als auch beim
  spaeter wieder les-/schreibbaren Run-Store;
- SAFE_BOOT-Exit;
- #15-FaultReset-Handoff.

Er besitzt **keinen zweiten Faultzustand** und **keinen zweiten
Recoveryalgorithmus**. Er ruft ausschliesslich vorhandene #17-/#18-/
StateMachine-Vertraege auf (`persistTransition()`, `persistRecoveryCandidate()`,
`clearActiveRunState()`, `loadAndInitialize()`/`activateLoadedRun()` via dem
neuen Boot-Orchestrator).

## 6.6 Aktorpfad

```text
bestehende Producer
  -> SafetyFaultService
  -> ActuatorSafetyDirective
  -> TemperatureControlApplicationOrchestrator
  -> ActuatorPlanner
  -> ActuatorPlanSinkDriver
```

Der Caller kann kein `Allowed` injizieren.

---

# 7. Faultklassen

```text
P1 = Hinweis / Warnung           (transient)
O2 = behebbarer Betriebsfehler   (transient)
S3 = verriegelter Sicherheitsfehler   (persistent, RecoverIfProvable)
Y4 = schwerer Systemfehler            (persistent, Terminal)
```

Dominanz: `Y4 > S3 > O2 > P1`. Die hoechste aktive Klasse bestimmt die
Safetywirkung. Alle aktiven Ursachen bleiben erhalten.

---

# 8. FaultIdentity

Exakt:

```cpp
struct FaultIdentity {
    FaultCode code;
    FaultSource source;
};
```

Nicht Bestandteil: RunRevision, FaultRevision, ControlRequestSequence,
Sensorsequence, Plannersequence, Bootzeit, Config-StateRevision,
Persistenzrevision, Zeitstempel.

Ein neues Messergebnis derselben Ursache erzeugt **keine** neue Instanz.

Eine `FaultIdentity` ist entweder ausschliesslich persistent (S3/Y4) oder
ausschliesslich transient (P1/O2) — bestimmt einzig durch `FaultClass` im
compile-time Katalog (Abschnitt 11), niemals laufzeitabhaengig.

---

# 9. FaultSource

## 9.1 Release-1-Sources

```text
Process
ProductSensor
AirSensor
CoolingSensor
SensorSet
TemperatureSafety
Control
ActuatorPlanner
Peltier
OuterFan
InnerFan
RunPersistence
ConfigurationRuntime
ConfigurationRecovery
SafetyPersistence
RestartSupervisor
SafetyCore
```

Unbekannte Werte werden abgelehnt.

## 9.2 Stabile Wirewerte (F4)

Keine unbeabsichtigte C++-Enum-Reihenfolge als Wirevertrag. Der Codec bildet
explizit auf feste `u8`-Werte ab:

```text
0  = reserviert/ungueltig
1  = Process
2  = ProductSensor
3  = AirSensor
4  = CoolingSensor
5  = SensorSet
6  = TemperatureSafety
7  = Control
8  = ActuatorPlanner
9  = Peltier
10 = OuterFan
11 = InnerFan
12 = RunPersistence
13 = ConfigurationRuntime
14 = ConfigurationRecovery
15 = SafetyPersistence
16 = RestartSupervisor
17 = SafetyCore
```

Ein Decoder, der einen Wert ausserhalb `1..17` liest, lehnt den Record ab
(fail-closed, kein Default).

---

# 10. Stabile FaultCodes

```text
P1-001 = 0x1001

O2-001 = 0x2001
O2-002 = 0x2002

S3-001 = 0x3001
S3-002 = 0x3002
S3-003 = 0x3003
S3-004 = 0x3004
S3-005 = 0x3005
S3-006 = 0x3006

Y4-001 = 0x4001
Y4-002 = 0x4002
Y4-003 = 0x4003
Y4-004 = 0x4004
Y4-005 = 0x4005
Y4-006 = 0x4006
Y4-007 = 0x4007
```

Sichtbarer technischer Code ist entsprechend `P1-001` usw.; Texte werden
spaeter ueber den Code lokalisiert.

---

# 11. Vollstaendige FaultCatalog-Matrix (F5)

Diese Matrix ist fuer **jede** erlaubte `(FaultCode, FaultSource)`-Identity
vollstaendig und normativ. Keine Zelle ist `TBD` oder „waehrend der
Implementierung festzulegen". `PostResetRunPolicy` ist klassenabgeleitet
(`S3 -> RecoverIfProvable`, `Y4 -> Terminal`, `P1`/`O2 -> AutoRearm`) und wird
trotzdem je Zeile explizit ausgewiesen, wie gefordert.

Spaltenkuerzel: **Kl.**=FaultClass, **P/T**=persistent/transient,
**AR**=autoRearm, **DP**=displayPriority, **Gate**=Gatewirkung
(`IS`=ImmediateStop, `U`=Unresolved-abhaengig, `—`=kein direkter Gate-Effekt,
`durch #21`=durch #21-Permission bestimmt), **OF/IF**=OuterFan/InnerFan
SafetyAction (`PM`=PlannerManaged, `FOn`=ForceOn, `FOff`=ForceOff),
**RE**=RestartEligible, **Auth**=ResetAuthorizationPolicy, **PRRP**=
PostResetRunPolicy, **Producer**=realer Producer oder `injection-only`.

| Code | Source | Kl. | P/T | AR | DP | Gate | OF | IF | RE | Auth | CauseClear-Oracle | PRRP | Producer |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| P1-001 | Process | P1 | T | ja | 10 (nur P1) | — | PM | PM | nein | keine (Auto) | vorhandene typisierte Prozesswarnung requalifiziert | AutoRearm | vorhandene Prozesswarnung, soweit #24 sie konsumiert |
| O2-001 | ProductSensor | O2 | T | ja | 20 | durch #21 | PM | PM | nein | keine (Auto) | #21-Permission wieder `Allowed`/AirFallback gueltig | AutoRearm | #20/#21 |
| O2-002 | AirSensor | O2 | T | ja | 10 | IS | PM | PM | nein | keine (Auto) | #20 `VALID` fuer AirSensor | AutoRearm | #20 |
| O2-002 | CoolingSensor | O2 | T | ja | 10 | IS | PM | PM | nein | keine (Auto) | #20 `VALID` fuer CoolingSensor | AutoRearm | #20 |
| S3-001 | AirSensor | S3 | P | nein | 30 | IS | PM | PM | ja | Operator | #20 `VALID` stabil (AirSensor) | RecoverIfProvable | #20 |
| S3-001 | CoolingSensor | S3 | P | nein | 30 | IS | PM | PM | ja | Operator | #20 `VALID` stabil (CoolingSensor) | RecoverIfProvable | #20 |
| S3-002 | SensorSet | S3 | P | nein | 40 | IS | PM | PM | ja | Operator | #21 CrossRole/SafeLocked eindeutig aufgeloest | RecoverIfProvable | #21 |
| S3-003 | TemperatureSafety | S3 | P | nein | 20 | IS | PM | PM | ja | Service | Commissioning-Producer (#35) qualifiziert erneut | RecoverIfProvable | injection-only bis #35-Producer real |
| S3-004 | ActuatorPlanner | S3 | P | nein | 50 | IS | PM | PM | ja | Operator | #23 `latchedWatchdogFault` requalifiziert (Abschnitt 36) | RecoverIfProvable | #23 |
| S3-005 | Peltier | S3 | P | nein | 10 | IS | PM | PM | ja | Service | typisierter ActuatorDiagnostic wieder `Healthy` | RecoverIfProvable | injection-only |
| S3-005 | OuterFan | S3 | P | nein | 10 | IS | FOff | PM | ja | Service | typisierter ActuatorDiagnostic wieder `Healthy` | RecoverIfProvable | injection-only |
| S3-005 | InnerFan | S3 | P | nein | 10 | IS | PM | FOff | ja | Service | typisierter ActuatorDiagnostic wieder `Healthy` | RecoverIfProvable | injection-only |
| S3-006 | OuterFan | S3 | P | nein | 60 | IS | FOn | PM | ja | Service | typisierter ActuatorDiagnostic wieder `Healthy` | RecoverIfProvable | injection-only |
| S3-006 | InnerFan | S3 | P | nein | 60 | IS | PM | FOff | ja | Service | typisierter ActuatorDiagnostic wieder `Healthy` | RecoverIfProvable | injection-only |
| Y4-001 | RunPersistence | Y4 | P | nein | 60 | IS | FOff | FOff | nein | Service | Run-Abandon-Pfad erfolgreich abgeschlossen (Abschnitt 26) | Terminal | #17/#18 |
| Y4-002 | ConfigurationRecovery | Y4 | P | nein | 50 | IS | FOff | FOff | nein | Service | #57 `RuntimeReady`/`FactoryInitializationCompleted`/`FactoryResetCompleted` | Terminal | #57 |
| Y4-003 | ConfigurationRuntime | Y4 | P | nein | 40 | IS | FOff | FOff | nein | Service | #56 `Operational` | Terminal | #56 |
| Y4-004 | RunPersistence | Y4 | P | nein | 20 | IS | FOff | FOff | nein | Service | erfolgreiche Redundanzreparatur/Run-Abandon (Abschnitt 21/26) | Terminal | #17 |
| Y4-004 | SafetyPersistence | Y4 | P | nein | 20 | IS | FOff | FOff | nein | Service | erfolgreiche SafetyState-/Marker-Redundanzreparatur | Terminal | #24-Store |
| Y4-005 | RestartSupervisor | Y4 | P | nein | 30 | IS | FOff | FOff | nein | Service | 30 min stabile Uptime (Abschnitt 5.2) | Terminal | #24 + ResetCause |
| Y4-006 | Process | Y4 | P | nein | 70 | IS | FOff | FOff | nein | Service | gesamte Process-Domain wieder eindeutig | Terminal | #14 |
| Y4-006 | SensorSet | Y4 | P | nein | 70 | IS | FOff | FOff | nein | Service | gesamte #21-Safety-Domain wieder eindeutig | Terminal | #21 |
| Y4-006 | Control | Y4 | P | nein | 70 | IS | FOff | FOff | nein | Service | #22-ControlContext wieder strukturell gueltig | Terminal | #22 |
| Y4-006 | ActuatorPlanner | Y4 | P | nein | 70 | IS | FOff | FOff | nein | Service | #23-Planner-Domain wieder eindeutig | Terminal | #23 |
| Y4-006 | RunPersistence | Y4 | P | nein | 70 | IS | FOff | FOff | nein | Service | #17-Domain wieder eindeutig | Terminal | #17 |
| Y4-006 | ConfigurationRuntime | Y4 | P | nein | 70 | IS | FOff | FOff | nein | Service | #56-Domain wieder eindeutig | Terminal | #56 |
| Y4-006 | ConfigurationRecovery | Y4 | P | nein | 70 | IS | FOff | FOff | nein | Service | #57-Domain wieder eindeutig | Terminal | #57 |
| Y4-006 | SafetyPersistence | Y4 | P | nein | 70 | IS | FOff | FOff | nein | Service | #24-Store-Domain wieder eindeutig | Terminal | #24-Store |
| Y4-006 | RestartSupervisor | Y4 | P | nein | 70 | IS | FOff | FOff | nein | Service | RestartSupervisor-Domain wieder eindeutig | Terminal | #24 |
| Y4-007 | SafetyCore | Y4 | P | nein | 10 | IS | FOff | FOff | nein | Service | interne Invariante erneut bewiesen (nur nach Codeaenderung) | Terminal | #24 selbst |

Zeilenzahl: 4 transient + 26 persistent = **30**, exakt der Katalogumfang aus
Abschnitt 12.

Fanpolicy-Aggregation bei mehreren gleichzeitigen Ursachen bleibt
`ForceOff > ForceOn > PlannerManaged` je Fan (Abschnitt 43).

`RestartEligible=ja` bedeutet nur, dass ein kontrollierter Restart **falls
ein aktiver Run vorhanden ist** angefordert wird (Abschnitt 29.2). Ohne
aktiven Run erfolgt bei jedem S3-Reset sofortiges `FAULT -> STANDBY` ohne
Restart.

`Auth=Service` gilt fuer alle Y4 (schwere Systemfehler) sowie fuer die
hardwarenahen/injection-only S3-Faelle (S3-003, S3-005, S3-006). `Auth
=Operator` gilt fuer die haeufigeren, klar rueckqualifizierbaren S3-Faelle
(S3-001, S3-002, S3-004). Diese Zuordnung ist Release-1-Entscheidung und kein
TBD.

---

# 12. Compile-time Capacity

```text
4 transiente P1/O2-Identitaeten
26 persistente S3/Y4-Identitaeten
30 Identitaeten insgesamt
```

Compile-time-fester Katalog. Keine Eviction. Keine Laufzeit-Erweiterung.
Keine Correlation-Slots.

---

# 13. Fault-Lifecycle

## 13.1 Runtime FaultRecord

Persistente (S3/Y4) Instanz:

```text
identity
instanceId            (aus dem PERSISTENTEN Zaehler)
causeCleared
firstSeenBootSequence
firstSeenMonotonicMillis
optional primary reference (nur S3/Y4)
restartAttempted
```

Transiente (P1/O2) Instanz:

```text
identity
instanceId            (aus dem TRANSIENTEN, rein RAM-lokalen Zaehler)
causeCleared
firstSeenMonotonicMillis (bootlokal, keine firstSeenBootSequence noetig)
optional primary reference (nur P1/O2)
```

Kein generisches persistentes `detail`. Acknowledgement ist nicht Teil des
persistenten Safetyrecords (Abschnitt 13.8).

## 13.2 Neue Ursache

1. Katalog pruefen (Klasse bestimmt, welcher Zaehler verwendet wird).
2. Keine aktive identische Instanz vorhanden.
3. `nextInstanceId` (persistent) bzw. `transientNextInstanceId` (transient)
   checked erhoehen.
4. `faultRevision` (persistent) bzw. `transientFaultRevision` (transient)
   checked erhoehen.
5. neue Instanz.
6. RAM-Gate sofort sicher.
7. Nur S3/Y4: SafetyState persistieren.
8. Bei Commitfehler (nur S3/Y4): EmergencyMarker.

P1/O2 haben **keinen** Schritt 7/8 — sie sind nie persistiert.

## 13.3 Gleiche Ursache erneut aktiv

Wenn bereits `causeCleared == false`, bleiben instanceId, faultRevision und
(bei S3/Y4) persistierter Record unveraendert. Kein Write pro Tick.

## 13.4 Relapse

Wenn dieselbe aktive Instanz bereits `causeCleared == true` hat und der
Producer wieder Active meldet:

```text
causeCleared = false
(persistente oder transiente) faultRevision++
bei S3/Y4: persistieren/readback
```

Eine alte ResetEvaluation ist dadurch stale.

## 13.5 Cause-Clear

Nur wenn der gesamte kanonische Producerzustand dieser FaultIdentity
eindeutig wieder qualifiziert ist (CauseClear-Oracle-Spalte in Abschnitt 11).

```text
causeCleared = true
faultRevision++ (im jeweils zustaendigen Zaehler)
```

S3/Y4 persistieren. Ein einzelner behobener Untergrund darf eine noch aktive
zweite Unterursache derselben Identity nicht clearen.

## 13.6 P1/O2 Auto-Rearm

Alle P1/O2-Katalogeintraege haben `autoRearm=true` (Abschnitt 11). Beim
qualifizierten Clear: Instanz entfernen, transiente `faultRevision++`, **keine
SafetyState-Persistenz**.

## 13.7 Reset (nur S3/Y4)

Reset entfernt genau eine persistente S3/Y4-Instanz. Vor Commit erneut
pruefen: target instance existiert, target gehoert zur erwarteten
FaultIdentity, erwartete `faultRevision` stimmt, `causeCleared == true`,
codebezogene Safetychecks positiv, Authorization gemaess Abschnitt-11-Spalte
`Auth` positiv, keine andere uncleared gleich-/hoeherklassige Ursache
blockiert, SafetyStorage healthy.

Andere bereits `causeCleared` Latches duerfen nacheinander resettiert werden.

```text
faultRevision++
target entfernen
```

persistieren/readback. Was danach mit einem eventuell aktiven Run passiert,
regelt Abschnitt 29 (S3 vs. Y4 unterschiedlich).

P1/O2 haben keinen Reset-Befehl — sie clearen ausschliesslich automatisch
(13.6).

## 13.8 Acknowledgement

Quittierung: ist Message-/UI-Zustand, aendert kein CauseClear, aendert
keinen Latch, aendert `faultRevision` nicht, ist nicht rebootkritisch, darf
nach Reboot wieder als nicht quittiert erscheinen.

---

# 14. Primary-/Follow-up

Nur innerhalb derselben Persistenzklasse (S3/Y4 <-> S3/Y4, P1/O2 <-> P1/O2).

Persistiert (nur S3/Y4):

```text
primaryInstanceId
primaryCode
primarySource
```

`0` in allen drei Feldern gleichzeitig bedeutet kein Primary. Jede andere
Kombination, bei der nicht alle drei `0` sind, aber mindestens eines davon
`0` ist, ist eine ungueltige halbe Kombination und wird abgelehnt (F4).

Regeln: keine Selbstreferenz; Primary muss beim Erzeugen existieren;
Code/Source muessen ein Katalogwert sein; spaeterer Primaryreset loescht
Follow-up nicht; Follow-up benoetigt eigenes Clear/Reset; Decoder darf eine
historische PrimaryInstanceId akzeptieren, deren aktive Instanz inzwischen
nicht mehr im Record liegt, sofern Code/Source gueltig und nicht
Selbstreferenz sind.

#19 uebernimmt spaeter die Langzeithistorie.

---

# 15. Persistente vs. transiente Zaehler und Ueberlauf (F2)

## 15.1 Persistent (S3/Y4)

```text
faultRevision = 1        (initial)
nextInstanceId = 1        (initial)
0 ist ungueltig/reserviert
```

Rebootstabil, keine Wiederverwendung, checked increment, High-Watermark wird
persistent gesichert. `FaultResetEvaluation`s `expectedFaultRevision`
referenziert ausschliesslich diesen Zaehler.

Kann die naechste resetrelevante persistente Revision nicht gebildet werden:
RAM `SafetyCoreExhausted`, Gate ImmediateStop, EmergencyMarker
`SafetyCounterExhausted`, kein weiterer normaler Safetycommit.

`nextInstanceId` bedeutet immer die naechste noch nicht verwendete
persistente ID. `UINT32_MAX` wird bewusst nicht mehr als neue ID ausgegeben.
Fehlt Headroom: keine neue Instanz, RAM fail-closed, EmergencyMarker
`SafetyCounterExhausted`. Keine Wiederverwendung alter IDs.

## 15.2 Transient (P1/O2)

```text
transientFaultRevision = 1   (bei jedem Boot neu)
transientNextInstanceId = 1  (bei jedem Boot neu)
```

**Niemals persistiert.** Konsumiert **nie** den persistenten
High-Watermark. Dient ausschliesslich der Anzeige-/UI-Eindeutigkeit innerhalb
eines Boots. Ein Ueberlauf hier ist unkritisch fuer die Persistenzintegritaet
(sattes Verhalten: neue transiente Instanzen werden bei erschoepftem
transienten Zaehler abgelehnt und als Y4-006/Process gemeldet — ein
erschoepfter Anzeige-Zaehler ist selbst eine sicherheitsrelevante Anomalie).

## 15.3 Pflichttest

```text
mehrere P1/O2
-> transient clear
-> Reboot
-> neuer S3/Y4
=> keine bereits persistente ID wiederverwendet
=> persistente Resetrevision nicht zurueckgedreht
=> ein pre-reboot capturedes expectedFaultRevision einer alten
   FaultResetEvaluation bleibt nach dem Reboot exakt so stale, wie es vor
   dem Reboot war (kein zufaelliges Wieder-Matchen durch P1/O2-Aktivitaet)
```

---

# 16. SafetyState-Persistenz (nur S3/Y4)

## 16.1 Record

```text
RecordTypeId = 9
Schema = 1
SafetyStorageEpoch = 1

Keys:
sf0
sf1
```

Max Envelope: `1024 Byte`.

## 16.2 Basis-Payload

| Feld | Wire |
|---|---:|
| nextInstanceId | u32 |
| faultRevision | u32 |
| bootSequence | u32 |
| safeBootRequired | u8 bool |
| abnormalRestartCount | u8 |
| restartIntentState | u8 |
| restartIntentFaultInstanceId | u32 |
| persistentFaultCount | u8 |

Basis: **20 Byte**.

## 16.3 Persistenter Faultrecord

| Feld | Wire |
|---|---:|
| instanceId | u32 |
| code | u16 |
| source | u8 |
| flags (Bitfeld, siehe 16.5) | u8 |
| firstSeenBootSequence | u32 |
| firstSeenMonotonicMillis | u64 |
| primaryInstanceId | u32 |
| primaryCode | u16 |
| primarySource | u8 |

Pro Fault: **27 Byte**.

Maximal 26 persistente Faults (Abschnitt 12 — unveraendert durch F1-F3, da
`preFaultProcessState` bewusst NICHT hier persistiert wird, siehe Abschnitt
29.2):

```text
payload_max = 20 + 26 * 27
            = 722 Byte

Envelope ohne UTC = 37 Byte

record_max = 759 Byte
```

Unter dem 1024-Byte-Limit.

## 16.4 Kein redundantes Policy-Wireformat

Nicht gespeichert werden: FaultClass, persistent/transient, autoRearm,
displayPriority, Gatewirkung, Fan-Actions, RestartEligible,
ResetAuthorizationPolicy, PostResetRunPolicy. Diese Felder folgen
ausschliesslich aus dem compile-time Katalog (Abschnitt 11).

## 16.5 Exakte Flag-Bitbelegung (F4)

```text
bit 0 = causeCleared
bit 1 = restartAttempted
bits 2..7 = reserved, muessen 0 sein
```

Ein Decoder, der ein reserviertes Bit gesetzt liest, lehnt den Record ab
(fail-closed, keine stille Ignorierung unbekannter Bits).

## 16.6 Kanonische Persistenzreihenfolge (F4)

Persistente Faultrecords werden **aufsteigend nach `instanceId`** kodiert.
Encoder, Decoder und Golden-Byte-Tests muessen dieselbe Ordnung beweisen; ein
Decoder, der Records in absteigender oder unsortierter Reihenfolge liest,
lehnt den Payload ab (Ordnungsverletzung ist ein semantischer Fehler, kein
Toleranzfall).

## 16.7 UTC im Envelope (F4)

`StorageEnvelope::utcUnixSeconds = std::nullopt` fuer **jeden**
`SafetyStateRecord`- und `EmergencyMarker`-Envelope, ausnahmslos.

**Begruendung:** Safety-Zeit ist gemaess Abschnitt 5.1 grundsaetzlich
bootlokal-monoton; UTC wird durch die Configuration-/NTP-Schicht (#56)
bereitgestellt, die im Bootreihenfolge-Vertrag (Abschnitt 25) **nach** dem
Laden von SafetyState/EmergencyMarker qualifiziert wird. Ein UTC-Pflichtfeld
wuerde eine Bootstrapping-Abhaengigkeit erzeugen, die es fuer die
Safety-Schicht nicht geben darf. Ohne UTC betraegt der Envelope-Overhead
exakt `37 Byte` (33 Byte Header + 4 Byte CRC); das ist die einzige Basis, auf
der die 1024-/64-Byte-Grenzen in diesem Plan gelten. Mit UTC waeren es
`45 Byte` — dieser Fall ist normativ ausgeschlossen, nicht nur unbenutzt.

## 16.8 Golden-Byte-Tests (F4, Pflicht)

Mindestens:

- leerer initialer SafetyState (beide Slots frisch initialisiert);
- genau ein S3;
- genau ein Y4;
- Primary/Follow-up-Paar;
- RestartIntent `Prepared`;
- `safeBootRequired=true`;
- maximaler persistenter Faultrecord (26 Eintraege, aufsteigend sortiert);
- Active Marker;
- Cleared Marker.

Jeder Test prueft die exakten Bytes, nicht nur Rundtrip-Gleichheit.

---

# 17. SafetyState Initialisierung und Scan

## 17.1 Fabrikneuer Safety-Namespace

Nur wenn `sf0 == NotFound`, `sf1 == NotFound`, `sem0 == NotFound`,
`sem1 == NotFound` und alle Reads technisch erfolgreich waren.

```text
nextInstanceId=1
faultRevision=1
bootSequence=1
safeBootRequired=false
abnormalRestartCount=0
restartIntentState=None
restartIntentFaultInstanceId=0
persistentFaultCount=0
```

Initialisierung: sf0 Envelope Revision 1 schreiben/readback; sf1 semantisch
gleicher Payload, Envelope Revision 2 schreiben/readback; erst danach
`storageIntegrityQualified=true`. Powerloss zwischen 1 und 2 fuehrt beim
naechsten Boot in den geschuetzten Storage-Recoverypfad, nicht zu normalem
Allowed.

## 17.2 Normaler Scan

### Gesund

Zwei gueltige Records; hoechste `versionValue` ist kanonisch; gleiche
Revision nur erlaubt, wenn Payload bytegleich ist.

### Unsicher

ReadError, CapacityError, CRC-/Envelopefehler, unbekanntes Schema/Epoch,
eine gueltige und eine fehlende/defekte Seite, gleiche Revision mit anderem
Payload, semantisch ungueltiger Payload. Unsicher => keine normale
Freigabe.

---

# 18. Semantische SafetyState-Validierung

Mindestens: `nextInstanceId != 0`; `faultRevision != 0`; `bootSequence != 0`;
`abnormalRestartCount <= 3`; RestartIntent bekannt;
`restartIntentState==None` genau mit `restartIntentFaultInstanceId==0`;
`Prepared` verlangt bekannte persistente target instance;
`persistentFaultCount <= 26`; jeder Record ist S3/Y4; bekannte
Code-/Source-Kombination (aus der Matrix in Abschnitt 11); eindeutige
FaultIdentity; eindeutige nonzero instanceId; nur bekannte Flag-Bits (16.5);
gueltige Primarymetadaten; keine Selbstreferenz; kanonische Reihenfolge
(16.6); keine Restbytes; keine unbekannten Enums.

---

# 19. SafetyState Commit

`StorageEnvelope::versionValue` ist die `SafetyStateRecordRevision`.

Verbindlich: nach der dualen Initialisierung ist die hoechste Revision 2;
jede spaetere Mutation verwendet checked `max(validRevision)+1`; der Slot mit
der aktuell niedrigeren/alten Revision ist das naechste Schreibziel;
`UINT64_MAX` wird nie umgebrochen — Revisionsexhaustion fuehrt in den
EmergencyMarker-/fail-closed-Pfad.

Normale Mutation: RAM-Kandidat bauen; alle Invarianten pruefen; Envelope
Revision checked bestimmen; alternierenden Slot waehlen; kodieren; write;
readback; Envelope + Payload exakt verifizieren; erst dann persistent
bestaetigt.

`CommitOutcomeUnknown`: readback exakt neuer Candidate -> committed; readback
eindeutig alter Candidate -> nicht committed; sonst -> indeterminate. Kein
Raten.

---

# 20. EmergencyMarker

## 20.1 Record

```text
RecordTypeId = 10
Schema = 1
SafetyStorageEpoch = 1

Keys:
sem0
sem1
```

Max Envelope: `64 Byte`.

## 20.2 Payload

| Feld | Wire |
|---|---:|
| markerState | u8 |
| markerReason | u8 |
| bootSequence | u32 |
| monotonicMillis | u64 |
| attemptedSafetyRecordRevision | u64 |

```text
22 Byte Payload
59 Byte Envelope (ohne UTC, siehe 16.7)
```

Passt unter das 64-Byte-Limit.

## 20.3 Exakte Wirewerte (F4)

```text
MarkerState:  0 = reserviert/ungueltig, 1 = Active, 2 = Cleared
MarkerReason: 0 = reserviert/ungueltig
              1 = SafetyStateWriteFailed
              2 = SafetyStateCapacityFailure
              3 = SafetyStateCommitIndeterminate
              4 = SafetyStateReadbackMismatch
              5 = SafetyStateCorruptOrUnreadable
              6 = SafetyCounterExhausted
```

Unbekannter Wert in beiden Feldern ist fail-closed (SAFE_BOOT), kein
Default.

## 20.4 MarkerSequence und Slotrotation

`StorageEnvelope::versionValue` ist die `MarkerSequence`. Sind beide
Marker-Slots `NotFound`, beginnt der erste `Active`-Marker mit Sequence 1 auf
`sem0`. Jeder spaetere Markerzustand verwendet checked `max(validSequence)+1`.
Geschrieben wird bevorzugt der andere Slot als der aktuell hoechste gueltige
Marker. Jeder Write wird readback-verifiziert. `UINT64_MAX` wird nie auf 0
umgebrochen; bei erschoepfter MarkerSequence ist der Marker-Store dauerhaft
unqualifiziert und der Boot bleibt fail-closed. Eine `Cleared`-Sequence darf
nur einen aelteren `Active`-Marker ueberstimmen, wenn beide Markerrecords
technisch/semantisch gueltig sind.

## 20.5 Fehlerpfad

Normaler SafetyState-Commitfehler: RAM `SafetyPersistenceUncertain` sofort;
Gate `ImmediateStop`; Marker `Active` auf bevorzugten Slot schreiben/readback;
falls nicht bestaetigt, genau ein zweiter Versuch auf dem anderen Slot;
danach kein Write-Loop. Scheitern beide: aktueller Boot bleibt fail-closed,
kein automatischer Restart, beim naechsten Boot sind vor jeder Freigabe
vollstaendige Producer-Requalifikation und erfolgreicher
SafetyStorage-Write/Readback Pflicht.

---

# 21. Redundanz-Recovery

Der Recoverypfad darf unbekannte Safetyhistorie nie still als "leer"
behandeln.

## 21.1 Mindestens ein semantisch gueltiger SafetyState vorhanden

Wenn genau ein SafetyState-Kandidat vollstaendig gueltig ist und die andere
Seite fehlt/defekt/unlesbar ist:

1. Gate bleibt ImmediateStop;
2. gueltigen Kandidaten als konservative bekannte Basis laden;
3. `safeBootRequired=true` setzen;
4. falls noch nicht aktiv, `Y4-004 / SafetyPersistence` als eigenen
   persistenten Latch anlegen;
5. Fault-/Instance-Counter checked fortschreiben;
6. defekten/fehlenden SafetyState-Slot mit dieser konservativen Basis
   ueberschreiben/readback;
7. zweiten SafetyState-Slot mit naechster Revision auf denselben
   semantischen Zustand bringen/readback;
8. erst jetzt SafetyState-Redundanz als healthy markieren;
9. vorhandene Markerhistorie analog auf zwei technisch gueltige Slots
   reparieren;
10. erst danach einen neuen `Cleared`-Marker schreiben/readback.

Der neue Y4-004-Latch bleibt **nach** erfolgreicher Redundanzreparatur aktiv;
Storage-Recovery setzt nur dessen `causeCleared=true`, wenn alle Read-/Write-
und Readbackpruefungen bestanden sind; erst ein separater geschuetzter
Faultreset darf Y4-004 entfernen; SAFE_BOOT bleibt danach zusaetzlich bis zum
separaten Exit bestehen.

## 21.2 Kein semantisch gueltiger SafetyState vorhanden

Wenn SafetyState-Slots existieren/Fehler melden, aber kein einziger
semantisch gueltiger SafetyState rekonstruierbar ist: keine neue leere
Safetyhistorie erfinden; keine InstanceIds/FaultRevisionen auf 1
zuruecksetzen; Marker nicht `Cleared` setzen; `storageIntegrityQualified=false`;
SAFE_BOOT bleibt dauerhaft aktiv. Dieser Zustand benoetigt den bereits
vorgesehenen expliziten lokalen Vollreset-/UART-Recoveryweg; Issue #24
implementiert keinen stillen Safety-History-Reset. Nur der in Abschnitt 17.1
bewiesene vollstaendig fabrikneue Namespace darf neu initialisiert werden.

## 21.3 Marker-only Unsicherheit

Ist der normale SafetyState vollstaendig gesund, aber der Markerpfad aktiv
oder degradiert: normaler SafetyState bleibt Basis; Y4-004 /
SafetyPersistence wird darin aktiviert, falls noch nicht vorhanden;
Markerredundanz wird repariert; Marker wird `Cleared`; Y4-004 bleibt bis
CauseClear + geschuetztem Reset; SAFE_BOOT bleibt bis separatem Exit.
Marker-Recovery ist niemals Faultreset oder SAFE_BOOT-Exit.

---

# 22. Reset-Port

Verbindlicher neutraler Port in `device_platform`:

```cpp
enum class ResetCause : std::uint8_t {
    PowerOn,
    SoftwareRestart,
    WatchdogOrPanic,
    Brownout,
    External,
    Unknown,
};

enum class RestartRequestStatus : std::uint8_t {
    Requested,
    Rejected,
};

class IResetController {
public:
    IResetController() = default;
    virtual ~IResetController() = default;

    IResetController(const IResetController&) = delete;
    IResetController& operator=(const IResetController&) = delete;
    IResetController(IResetController&&) = delete;
    IResetController& operator=(IResetController&&) = delete;

    [[nodiscard]] virtual ResetCause resetCause() const = 0;
    [[nodiscard]] virtual RestartRequestStatus requestSoftwareRestart() = 0;
};
```

Keine Fermentationsbegriffe im Port. Testsupport: `SimulatedResetController`.
Kein ESP-IDF-Adapter in #24.

---

# 23. RestartIntent

```text
RestartIntentState: 0 = reserviert/ungueltig, 1 = None, 2 = Prepared
```

(Wire-Werte gemaess F4; `None` ist nicht `0`, weil `0` global als
"ungueltig/unbekannt" reserviert bleibt und ein Decoder ein rohes `0`
niemals still als `None` interpretieren darf.)

## 23.1 Prepare

Ausgeloest fuer jeden `RestartEligible=ja`-Katalogeintrag (Abschnitt 11:
alle S3), **aber nur wenn beim erfolgreichen Reset ein aktiver Run
vorhanden war** (Abschnitt 29.2). Einmal pro `instanceId`.

Vor `requestSoftwareRestart()` persistieren/readback:

```text
fault.restartAttempted=true       (auf der bereits committeten Reset-Revision
                                    dokumentarisch, die Instanz selbst existiert
                                    zu diesem Zeitpunkt bereits nicht mehr)
restartIntentState=Prepared
restartIntentFaultInstanceId=<ehemalige instanceId, nur zu Diagnosezwecken>
```

## 23.2 Request Rejected

Wenn `requestSoftwareRestart() == Rejected`: genau ein Commit
`restartIntentState=None`, `restartIntentFaultInstanceId=0`. Kein zweiter
Restartversuch derselben Instanz. Commitfehler => EmergencyMarker/fail-closed.
Der Run bleibt in diesem Fall in seinem korrigierten (Abschnitt 29.2), nicht
neu gebooteten Zustand liegen — der naechste **beliebige** Boot (auch
unkontrolliert) wertet ihn danach ganz normal ueber die unveraenderte
#18-Logik aus, da der Snapshot bereits korrekt ist.

## 23.3 Request Requested

`Prepared` bleibt bis zum naechsten Boot bestehen. Es wird kein Reboot
behauptet, bevor der naechste Boot tatsaechlich beobachtet wurde.

---

# 24. BootSequence und RestartEpisode

## 24.1 BootSequence

Nach erfolgreichem SafetyState-Load: erster initialisierter Boot ist 1;
jeder weitere Boot checked +1; genau einmal pro Boot; vor Erzeugung neuer
bootlokaler Faulttimestamps. Overflow => EmergencyMarker/fail-closed.

## 24.2 Resetursache

Abnormal: `Brownout`, `WatchdogOrPanic`, `Unknown`, `SoftwareRestart` ohne
Prepared, `SoftwareRestart` mit Prepared, Prepared + unerwartet anderer
Resetgrund. Nicht abnormal ohne Prepared: `PowerOn`, `External`.

## 24.3 Prepared konsumieren

Beim Boot: ResetCause lesen; Prepared genau einmal auswerten; abnormalen
Counter saturierend aktualisieren; Prepared im selben Safety-Kandidaten auf
`None` setzen; commit/readback. Kein Boot darf denselben Prepared-Intent
zweimal zaehlen.

## 24.4 Counter

`0,1,2,3`, saturierend, nie 4. Bei Uebergang auf 3: Y4-005 anlegen oder
bestehende Instanz aktiv halten; `safeBootRequired=true`; ein atomarer
SafetyState-Kandidat (Events dazu: Abschnitt 47/48).

---

# 25. SAFE_BOOT Eintritt

Bootreihenfolge:

```text
alle Outputs physisch/abstrakt AUS
-> EmergencyMarker scan
-> SafetyState scan/init
-> bootSequence / ResetCause / RestartIntent
-> Configuration #57
-> Configuration #56
-> RunPersistence #17/#18 load (noch keine aktive Recovery-Aktivierung)
-> aktuelle Sensorgrundqualifikation
-> Safety-Fault-Mappings
-> SAFE_BOOT-Entscheidung
```

SAFE_BOOT mindestens bei: aktiver/unklarer EmergencyMarker, unqualifiziertem
SafetyStorage, bestehendem persistentem S3/Y4-Latch beim Boot,
`abnormalRestartCount == 3`, Y4-Configurationzustand, RunPersistence
indeterminiert/nicht rekonstruierbar, sicherheitsrelevanter unbekannter
Bootevidenz.

Wenn SafetyState technisch schreibbar: `safeBootRequired=true` vor
`BOOT -> SAFE_BOOT` persistieren/readback. Wenn SafetyState gerade die
Ursache des Problems ist: Marker/Corruption selbst haelt SAFE_BOOT
persistent/fail-closed; Recovery rekonstruiert spaeter zuerst einen State mit
`safeBootRequired=true`.

**Wichtig fuer S3-Recovery (Abschnitt 5.3/29):** Solange ein S3-Latch aktiv
ist, erzwingt dieser Schritt SAFE_BOOT unabhaengig davon, was der #17-Run-
Snapshot als `processState` traegt. Die Sicherheit des S3-Recovery-Pfads
haengt deshalb **nicht** davon ab, ob der Run-Snapshot rechtzeitig korrigiert
wurde — ein unkontrollierter Reboot waehrend eines noch ungeklaerten S3
fuehrt immer in SAFE_BOOT, egal was `#17` zeigt.

---

# 26. Run-Abandon-Pfad (F3, verallgemeinert)

## 26.1 Zweck und Geltungsbereich

Ein autorisierter Terminalisierungspfad fuer einen geladenen Run, der nicht
sicher rekonstruierbar ist oder werden darf, aber dessen physischer
Run-Persistenzspeicher wieder eindeutig les-/schreibbar ist. Gilt fuer:

- SAFE_BOOT-Eintritt mit geladenem, noch nicht reaktiviertem Run
  (Abschnitt 5.4);
- Y4-001-Orphan-Fall: ein geladener `Current`-Run mit
  `processState.state == Fault`, aber **ohne** einen dazu passenden aktiven
  S3/Y4-Latch (kann durch einen Crash zwischen der S3-Korrekturbuchung und
  ihrer Bestaetigung entstehen, Abschnitt 29.3);
- die Ladefehlerfaelle aus Abschnitt 39 (`NotReconstructible`,
  `NotReconstructibleOrphanedState`, `UnsupportedSchema`, `ForeignEpoch`,
  `PreparedInterrupted`, `ReadFailed`, `CapacityExceeded`), sobald der Store
  wieder technisch gesund ist.

## 26.2 API und Wiederverwendung

Genau eine neue, schmale `RunPersistenceCoordinator`-Methode:

```text
abandonUnrecoverableRun(...)
```

Sie ist **kein zweiter Persistenzkern**. Sie baut intern einen vollstaendigen
`NoActiveRun/STANDBY`-Kandidaten (`clearActiveRunState()`-Vertrag,
`run_commands.cpp:577-597`) und schreibt ihn ueber denselben Mechanismus wie
`persistRecoveryCandidate()` — `writeSnapshotCore()` mit
`RunPersistenceMutationKind::Recovery`.

Zulaessige Coordinator-Ausgangszustaende: `LoadedActiveRun`,
`FallbackRecoveryPending`, `Ready` mit einem bereits geladenen, als nicht
rekonstruierbar erkannten Snapshot.

Ablauf:

1. geladenen Run **niemals** raten oder rekonstruieren;
2. **keinen** Factory Reset ausloesen — Programme und Configuration bleiben
   unangetastet;
3. `candidate = clearActiveRunState(current)` bilden;
4. `candidate` muss vor dem Schreiben als gueltiger `NoActiveRun/STANDBY`-
   Snapshot validieren (`noActiveRunHasNoRecoveryFields()` und die
   bestehende `validateRunPersistenceSnapshot()`-Pruefung);
5. write-before-apply ueber `writeSnapshotCore(..., RunPersistenceMutationKind::Recovery)`
   — **identische Integritaetsgarantie wie jeder andere #17-Schreibpfad**:
   Compare-and-Swap gegen den zuvor gelesenen Altwert plus physische
   Envelope-/CRC-Revalidierung beim naechsten Boot. Das ist eine bewusste
   Abweichung von der Auftragsformulierung „Write und Readback
   verifizieren" — ein synchroner Read-after-Write existiert im
   `#17`-Vertrag nirgends und wird hier nicht neu erfunden, um keinen
   zweiten, inkonsistenten Verifikationsmechanismus neben dem bestehenden
   `#17`-Modell zu etablieren (siehe Abschnitt 2.1);
6. erst nach `Applied` gilt die technische Ursache als beseitigt qualifiziert;
7. der zugehoerige Y4-Latch (z. B. Y4-001) bleibt bis zum geschuetzten Reset
   erhalten — Run-Abandon beseitigt die Ursache, nicht den Latch;
8. SAFE_BOOT wird separat behandelt (Abschnitt 25/32) — Run-Abandon allein
   verlaesst SAFE_BOOT nicht;
9. bei erneutem Persistenzfehler waehrend Schritt 5 bleibt das System
   fail-closed (Mapping auf Y4-004 gemaess Abschnitt 39, identisch zu jedem
   anderen `#17`-Schreibfehler).

## 26.3 Kein normaler Recovery-Start waehrend SAFE_BOOT

Wenn SAFE_BOOT bereits feststeht: `activateLoadedRun()` wird fuer einen
normalen aktiven Run **nicht** aufgerufen; kein PI-/Planner-Resume; kein Run
wird automatisch weitergefuehrt. Stattdessen greift 26.1/26.2.

Kein neues Wire-Schema.

---

# 27. Runtime S3/Y4 -> FAULT

Bei neuem blockierendem S3/Y4:

1. FaultCore RAM sofort aktiv;
2. Gate ImmediateStop;
3. SafetyState commit/readback;
4. wenn ProcessState bereits Fault/SafeBoot: kein zweiter Prozesswechsel;
5. sonst `CriticalFault` gegen bestehende StateMachine;
6. bei aktivem Run:
   - **vor** dem Transitions-Commit die vollstaendige aktuelle
     `ProcessRuntimeState` (Abschnitt 2.1) RAM-seitig im
     `SafetyProcessCoordinator` sichern — dies ist die spaetere Grundlage
     fuer eine S3-Korrekturbuchung (Abschnitt 29.2) und wird **nicht**
     persistiert (rein boot-session-lokal; ein unkontrollierter Reboot vor
     einem erfolgreichen, autorisierten Reset macht diese Kopie ohnehin
     gegenstandslos, weil dann SAFE_BOOT ueber den weiterhin aktiven Latch
     greift, Abschnitt 25);
   - vorhandenen #17 `persistTransition()` write-before-apply nutzen;
   - aktiven Run als `Fault` erhalten (der persistierte Snapshot zeigt
     wahrheitsgemaess `Fault`, exakt wie es der bestehende Schema-3-Vertrag
     vorsieht);
7. ohne aktiven Run: ProcessState RAM auf `Fault`; SafetyState ist die
   rebootpersistente Wahrheit;
8. #17-Fehler beim Fault-Commit: auf Y4-004/Y4-001 mappen; Gate bleibt
   ohnehin ImmediateStop.

Safety-Abschaltung wartet nie auf Persistenz.

---

# 28. FaultReset / #15

## 28.1 Bestehenden Vertrag beibehalten

`FaultResetEvaluation` bleibt der von #24 qualifizierte #15-Vertrag.
Verbindliche Erweiterung: `targetFaultInstanceId` liegt **in**
`FaultResetEvaluation`. Kein alternatives Feld „oder in Decision".

## 28.2 #15-Verantwortung

`decideFaultReset()` prueft: Envelope; CommandId; expectedStateSequence;
expectedFaultRevision (persistenter Zaehler, Abschnitt 15.1); Bestaetigung;
`FaultResetEvaluation`; targetFaultInstanceId nonzero; Evaluation
faultRevision == RunCommandState-Projektion.

#15 darf **nicht**: FaultCore mutieren; faultRevision selbst erhoehen;
`criticalSafetyEventPending=false` setzen.

Die Decision darf nur RAM-Commandbookkeeping vorbereiten und traegt
`authorizedFaultResetInstanceId` als eindeutigen Handoff.

`ResetFault` bleibt gemaess bestehendem #17-Vertrag **nicht** in
`isPersistedRunCommand()` (verifiziert: `run_persistence_contract.cpp:283-302`
gibt fuer `CommandKind::ResetFault` explizit `false` zurueck).

---

# 29. FaultReset Commitreihenfolge — S3 vs. Y4

## 29.1 Gemeinsamer Anfang (beide Klassen)

```text
SafetyFaultService.evaluateReset(target)
-> #15 decideFaultReset(...)
-> SafetyFaultService.commitReset(target, expectedFaultRevision)
   -> SafetyState persist/readback (Latch entfernt, faultRevision++)
-> #15 Commandbookkeeping in RAM anwenden
-> RunCommandState Safetyprojektion synchronisieren
```

Wenn nach diesem Reset weiterhin blockierende Faults aktiv sind:
`ProcessState unveraendert`, Gate bleibt sicher — kein weiterer Schritt.

Wurde der letzte blockierende Fault entfernt, verzweigt die Behandlung nach
Klasse und ob ein aktiver Run vorhanden ist:

## 29.2 Zweig A: Y4, oder S3 ohne aktiven Run — sofort terminal

```text
Fault -> Standby, terminal (ProcessEvent::FaultResetCompleted)
```

Wenn aktiver Run vorhanden (nur bei Y4 moeglich, da S3-ohne-Run per
Definition keinen Run hat): `clearActiveRunState(candidate)` vor
Snapshotbildung, `NoActiveRun/STANDBY` ueber `persistTransition()`/den
bestehenden `#17`-Terminalisierungspfad persistieren, erst nach Commit
anwenden. Ohne aktiven Run: RAM-Transition sofort anwenden.

Wenn `ProcessState == SafeBoot`: **kein Prozessuebergang**, SAFE_BOOT bleibt,
separater Exit erforderlich (Abschnitt 32).

## 29.3 Zweig B: S3 mit aktivem Run — Restart-Handoff an #18

Voraussetzung: die in Abschnitt 27 Schritt 6 RAM-gesicherte
`ProcessRuntimeState` (der Zustand unmittelbar vor Fault-Eintritt) ist noch
vorhanden (dieselbe Boot-Session, kein Reboot dazwischen — sonst greift
SAFE_BOOT laut Abschnitt 25 und dieser Zweig wird nicht erreicht).

```text
1. SafetyProcessCoordinator baut candidate = aktueller RunCommandState
   mit candidate.processState = gesicherte Pre-Fault-ProcessRuntimeState,
   candidate.runRevision = current.runRevision + 1
2. RunPersistenceCoordinator::persistRecoveryCandidate(current, candidate,
   time) — write-before-apply, RunPersistenceMutationKind::Recovery,
   bestehender #17-Schreibkern, kein neues Wire-Schema
3. bei Applied:
     Event RunRecoveryHandoffPrepared (Abschnitt 47)
     RestartIntent Prepared fuer die (bereits entfernte) Faultinstanz
     schreiben (Abschnitt 23.1), readback
     requestSoftwareRestart()
   bei jedem anderen Ergebnis (WriteFailed / CapacityExceeded /
     PersistenceIndeterminate / PersistenceCommittedApplyFailed):
     kein Restart anfordern; RAM bleibt in Fault; technischer Fehler wird
     exakt auf Y4-004/Y4-001 gemappt (Abschnitt 39 — dieselben, bereits
     bestehenden Status-Werte); Gate bleibt ohnehin ImmediateStop
4. naechster Boot (kontrolliert oder unkontrolliert, macht keinen
   Unterschied mehr): SAFE_BOOT-Pruefung findet keinen aktiven S3-Latch mehr
   (er wurde bereits in 29.1 entfernt) -> normaler Bootpfad ->
   loadAndInitialize() laedt einen Snapshot mit `processState` = der
   wiederhergestellten Vor-Fault-Phase (nicht `Fault`) -> vollstaendig
   unveraenderte #18-Logik (activateLoadedRun()/Hop-1/Hop-2) entscheidet
   selbststaendig Resume oder eigene Terminalisierung — #24 greift ab hier
   nicht mehr ein
```

Kein zweiter Recoveryalgorithmus: Schritt 4 ruft ausschliesslich
bestehenden, unveraenderten `#18`-Code auf.

## 29.4 Warum diese Reihenfolge crash-sicher ist, ohne neue Koordinationsfelder

Der Auftrag verlangt eine geschlossene Crash-/Fehlermatrix fuer: vor
Fault-Persistenz, nach Fault-Persistenz, nach FaultReset, vor
Recovery-Handoff, waehrend Recovery-Persistenz. Diese Fassung deckt sie
**durch Komposition bereits bestehender Regeln** ab, ohne ein neues
persistentes Koordinationsfeld einzufuehren:

| Zeitpunkt | Folge |
|---|---|
| Crash **vor** SafetyState-Reset-Commit (29.1) | Latch bleibt vollstaendig aktiv, kein Reset wirksam, naechster Boot: SAFE_BOOT (Abschnitt 25) |
| Crash **nach** SafetyState-Reset-Commit, **vor** Schritt 2 (29.3) | SafetyState zeigt keinen Latch mehr; `#17`-Run-Snapshot zeigt noch `Fault`. Naechster Boot: kein SAFE_BOOT durch Latch, aber `loadAndInitialize()` laedt `Current`+`Fault` ohne passenden Latch — exakt der bereits definierte Y4-001-Orphan-Fall (Abschnitt 39). Der Run wird als Y4-001 klassifiziert und ueber den Run-Abandon-Pfad (Abschnitt 26) terminalisiert. Kein unsicherer Resume moeglich. |
| Schritt 2 selbst schlaegt fehl (`PersistenceIndeterminate` o.ae.) | Bereits bestehende `#17`-Fehlerbehandlung greift (Abschnitt 39: `PersistenceIndeterminate -> Y4-004`), kein Restart wird angefordert, RAM bleibt Fault. Identisch zu jedem anderen fehlgeschlagenen `#17`-Schreibversuch — keine Sonderbehandlung noetig. |
| Crash **nach** Schritt 2 erfolgreich, **vor** Schritt 3 (RestartIntent) | `#17`-Snapshot ist bereits korrigiert (nicht mehr `Fault`). Egal ob der naechste Boot kontrolliert oder unkontrolliert ist: `loadAndInitialize()` sieht einen normalen aktiven Snapshot und startet ganz normale, unveraenderte Hop-1/Hop-2-Auswertung. Kein #24-Zutun mehr noetig. |
| Crash **waehrend** Schritt 3 (RestartIntent-Commit) | Bestehende Abschnitt-23-Logik: Commitfehler => EmergencyMarker/fail-closed; erfolgreicher Commit vor Crash => naechster Boot konsumiert `Prepared` gemaess Abschnitt 24.3 wie jeder andere Restart-Intent. |
| Crash **nach** `requestSoftwareRestart()`, vor dem tatsaechlichen Reboot | Rueckgabe `Requested` bedeutet nur „Anforderung akzeptiert", kein bewiesener Reboot (Abschnitt 23.3, unveraendert). Der naechste tatsaechliche Boot ist in jedem Fall bereits durch die Snapshot-Korrektur aus Schritt 2 sicher. |

Damit ist jede Zwischenstelle entweder durch eine bereits bestehende
`#17`-Fehlerregel oder durch die bereits geplante Y4-001-Orphan-Klassifikation
abgedeckt. Es gibt keinen Zustand, in dem ein Run ohne vollstaendige neue
#18-Auswertung wieder Aktoren freigeben kann.

---

# 30. Neue ProcessEvents

Nur:

```text
FaultResetCompleted
SafeBootExitCompleted
```

## FaultResetCompleted

`Fault -> Standby`, terminal. Ausgeloest fuer Y4 und fuer S3 ohne aktiven
Run (Abschnitt 29.2). **Nicht** ausgeloest fuer S3 mit aktivem Run
(Abschnitt 29.3) — dort bleibt RAM in `Fault`, bis der tatsaechliche
kontrollierte Reboot stattfindet. Kein `RecoveryEvaluation`.

## SafeBootExitCompleted

`SafeBoot -> Standby`, nur nach erfolgreichem SAFE_BOOT-Exit (Abschnitt 32).

`STATE_MACHINE.md` wird entsprechend aktualisiert.

---

# 31. SAFE_BOOT Exit

## 31.1 Voraussetzungen

Aktueller ProcessState `SafeBoot`; `safeBootRequired == true`; EmergencyMarker
eindeutig Cleared/none und Marker-Redundanz healthy; SafetyState-Redundanz
healthy; kein aktiver S3/Y4; ConfigurationService `Operational`;
ConfigurationRecovery aktuell qualifiziert; RunPersistence eindeutig und
**NoActiveRun**; kein RunPersistence-Indeterminate; AirSensor VALID;
CoolingSensor VALID; SensorSet nicht unresolved; PlannerWatchdog nicht
gelatcht; kein Y4-Unknown; trusted Serviceauthorization true.

ProductSensor ist fuer den Rueckweg nach Standby nicht generell Pflicht.

## 31.2 Reihenfolge

Da ein evtl. geladener Run bereits beim SAFE_BOOT-Eintritt ueber den
Run-Abandon-Pfad terminalisiert wurde (Abschnitt 26):

1. alle Exitbedingungen neu pruefen;
2. `safeBootRequired=false` im SafetyState persistieren/readback;
3. erst danach `SafeBootExitCompleted` RAM-Transition;
4. `STANDBY`;
5. Gate kann erst in einem **neuen** normalen Safety-Evaluationsschritt
   `Allowed` werden.

Crash nach SafetyState-Commit vor RAM-Transition: Run ist bereits
`NoActiveRun`; Reboot sieht `safeBootRequired=false`; normale
Bootqualifikation fuehrt nach Standby, sofern weiterhin alle Gates positiv
sind.

---

# 32. SensorQuality #20

## O2-002 STALE

Air/Cooling STALE: `O2-002 / konkrete Source`; Gate ImmediateStop fuer
Peltier; kein ProcessState Fault; bestehende #23 Fan-Nachlaufsemantik bleibt
aktiv; autoRearm erst nach #20-qualifiziertem VALID.

## S3-001 FAILED

Air/Cooling FAILED: O2-002 derselben Source entfernen; S3-001 raisen;
Runtime -> Fault; nach stabil VALID: `causeCleared=true`; Latch bis Reset
(dann `RecoverIfProvable`, Abschnitt 29.3, falls Run aktiv war).

#24 dupliziert keine Filter-/CRC-/Recoverycounter.

---

# 33. SensorSelection #21

## O2-001

ProductSensor degradiert / validierter AirFallback. `AirFallbackActive +
permission Allowed`: O2-001 darf sichtbar bleiben; Gate darf ansonsten
Allowed werden; luftgefuehrte Regelung laeuft weiter.

## S3-002

Bei sicherheitsrelevantem `SafeLocked`/`CrossRoleEvidenceIndeterminate`:
S3-002 / SensorSet.

## Y4-006 / SensorSet

Nur fuer strukturell ungueltige/unbekannte Safety-Evidenz, nicht fuer normale
PolicyWait/UserAction. CauseClear erst, wenn der gesamte aktuelle
#21-Safetykontext wieder eindeutig qualifiziert ist.

---

# 34. TemperatureControl #22

Keine Faults aus normalen: `NeutralBand`, `Saturated`, `AirLimitReduced`,
`AirLimitBlocked`.

Mappings: `InvalidConfiguration -> Y4-006/Control`; `TimeInvalid ->
Y4-006/Control`; `RequestIdentityExhausted -> Y4-006/Control`.
`SensorUnavailable`/`InvalidSample`: Gate unresolved/blocked, konkrete
Ursache soll aus #20/#21 kommen, kein duplizierter Sensorfault.
`NoCommissioning`: `Unresolved`, kein erfundener Fault, keine
Aktorfreigabe.

---

# 35. ActuatorPlanner #23

## S3-004 Watchdog

Realer Producer: `ActuatorPlanner::state().latchedWatchdogFault` =>
S3-004 / ActuatorPlanner. `RecoverIfProvable` wie jedes S3 (Abschnitt 29.3),
nicht mehr als S3-004-spezifischer Sonderfall — die Restart-Mechanik ist ab
dieser Fassung fuer alle S3 einheitlich.

CauseClear erst nach neuer aktueller vertrauenswuerdiger
Applicationevidenz: temperaturgeregelte Phase verlassen, oder neue
strukturell gueltige #22-Evaluation im aktuellen ControlContext,
Planner/Peltier abstrakt sicher Idle. Es wird kein physisches
Sink-Acknowledgement behauptet.

Nach erfolgreichem systemweiten Reset ruft die Application einmal
`ActuatorPlanner::applyExternalWatchdogFaultReset(now)` auf — dies geschieht
**vor** dem Restart-Handoff aus Abschnitt 29.3, damit der Planner beim
naechsten Boot nicht erneut sofort denselben Watchdog ausloest.

---

# 36. Injection-only Aktordiagnose

## S3-005 – elektrischer/Ausgangsfehler

Zulaessig: Peltier, OuterFan, InnerFan. Noch kein realer Hardwareproducer.

## S3-006 – funktionaler Fanfehler

Zulaessig: OuterFan, InnerFan. Noch kein realer Tachometer-/Stromproducer.

Keine Hardwarefaehigkeit wird behauptet.

---

# 37. Fan-Safety-Directive

```cpp
enum class FanSafetyAction : std::uint8_t {
    PlannerManaged,
    ForceOn,
    ForceOff,
};

struct ActuatorSafetyDirective {
    ActuatorSafetyGateStatus gate;
    FanSafetyAction outerFanAction;
    FanSafetyAction innerFanAction;
};
```

Aggregation bei mehreren gleichzeitig aktiven Ursachen je Fan:
`ForceOff > ForceOn > PlannerManaged`. Die vollstaendige Zuordnung je Identity
steht in der Matrix in Abschnitt 11 (Spalten OF/IF). Diese Aktionen sind
Sollbefehle, kein physisches Feedback. Die bestehenden #23-Nachlaufregeln
bleiben fuer `PlannerManaged` die einzige Quelle der Wahrheit.

---

# 38. RunPersistence #17/#18 – exakte Mappingmatrix

## Boot / Load

| Status | #24 |
|---|---|
| NoPersistedRun | kein Fault |
| Current, normale aktive/Completed-Topologie | kein Fault; normale #18-Bewertung nur wenn kein SAFE_BOOT |
| Current mit `ProcessState::Fault` und bereits passendem aktivem #24-S3/Y4-Latch | kein zusaetzlicher Fault; der Run-Fault ist erwartete Folge |
| Current mit `ProcessState::Fault` **ohne** passenden #24-Latch | Y4-001 / RunPersistence; niemals normal resumieren; Run-Abandon-Pfad (Abschnitt 26) — dies ist der Crash-Kompositionsfall aus Abschnitt 29.4 |
| NoActiveRun | kein Fault |
| FallbackRecovered | kein Y4; Diagnoseevent |
| PreparedInterrupted | Y4-004 / RunPersistence |
| NotReconstructible | Y4-001 / RunPersistence |
| NotReconstructibleOrphanedState | Y4-001 / RunPersistence |
| ReadFailed | Y4-004 / RunPersistence |
| CapacityExceeded | Y4-004 / RunPersistence |
| UnsupportedSchema | Y4-001 / RunPersistence |
| ForeignEpoch | Y4-001 / RunPersistence |
| AlreadyInitialized an unmoeglicher Bootstelle | Y4-007 / SafetyCore |

## Runtime

```text
WriteFailed
CapacityExceeded
PersistenceIndeterminate
CounterOverflow
    -> Y4-004 / RunPersistence

PersistenceCommittedApplyFailed
    -> Y4-001 / RunPersistence
```

Normale erwartbare: `AlreadyProcessed`, `AlreadyPersisted`, `NotEligible`,
`NotAllowedInState`, `StaleDecision`, `Busy`, `NotDue`, `NoActiveRun` sind
nicht automatisch persistente SafetyFaults.

Ein `BlockedIndeterminate`-Coordinatorstate ohne bereits gemappte Ursache
liefert Y4-006 / RunPersistence.

Alle in dieser Tabelle referenzierten Y4-Faelle: sobald der Run-Store wieder
eindeutig les-/schreibbar ist, terminalisiert der Run-Abandon-Pfad
(Abschnitt 26) den betroffenen Run; der Y4-Latch selbst bleibt bis zum
geschuetzten Reset bestehen.

---

# 39. Configuration #57

Direkt: `ConfigurationSafetyProducer::ConfigurationUnavailable`,
`ConfigurationSafetyProducer::ConfigurationIntegrityFailure` =>
Y4-002 / ConfigurationRecovery. Clear erst wenn aktuelle #57-Recovery
eindeutig wieder `RuntimeReady` bzw. erfolgreich abgeschlossen ist. Kein
Auto-Reset des Latches.

---

# 40. Configuration #56

`ConfigurationServiceMode`: `RuntimeFailure -> Y4-003/ConfigurationRuntime`;
`CommitIndeterminate -> Y4-003/ConfigurationRuntime`; `Operational ->`
positive Clear-Qualifikation.

Zwischenzustaende (`NoRuntime`, `RecoveryPreparing`, `CommitInProgress`,
`ResetPreparing`, `ResetEligibleNoRuntime`, `EpochResetting`,
`BootstrapFinalizationPending`) => Gate `Unresolved`, solange sie fachlich
erwartbar sind; sie erzeugen nicht pro Tick neue Y4-Faults. Ein
unbekannter/unmoeglicher Modewert: `Y4-006 / ConfigurationRuntime`.

---

# 41. CONFIGURATION_SAFETY_INTEGRATION_GATE

Pflichttests gegen reale Producer: `ConfigurationRuntimeFailure`,
`ConfigurationUnavailable`, `ConfigurationIntegrityFailure`, nicht
aufloesbarer `CommitOutcomeUnknown`/`CommitIndeterminate`. Jeder Fall
beweist: persistenter Y4-Latch; unmittelbare Aktorsperre; Reboot loescht
Latch nicht; Boot -> SAFE_BOOT; Ursache-Recovery alleine loescht Latch nicht;
geschuetzter Faultreset erforderlich; SAFE_BOOT bleibt danach bis separatem
Exit; aktiver Plannerpfad kann Gate nicht umgehen.

---

# 42. Y4-006 – unabhaengige Unknown-Ursachen

Feste Sources: `Process`, `SensorSet`, `Control`, `ActuatorPlanner`,
`RunPersistence`, `ConfigurationRuntime`, `ConfigurationRecovery`,
`SafetyPersistence`, `RestartSupervisor`. Jede Source ist eigene
FaultIdentity. Beispiel: `Y4-006/ConfigurationRuntime`, `Y4-006/Process`,
`Y4-006/SensorSet` sind drei Instanzen. Clear von SensorSet beruehrt die
beiden anderen nicht. Innerhalb einer Source wird erst gecleart, wenn alle
Bedingungen dieser kanonischen Domain wieder eindeutig sind. Keine
`last-origin-wins`-Semantik.

---

# 43. SafetyGate / SafetyDirective

## ImmediateStop

Mindestens: ProcessState Fault; ProcessState SafeBoot; aktiver blockierender
S3/Y4; EmergencyMarker active/unknown; SafetyStorage unqualified;
SafetyPersistence RAM latch; O2-002 Air/Cooling STALE; #21 Permission
Blocked; terminaler Safety-Prozesscommit in Bearbeitung.

## Unresolved

Wenn notwendige aktuelle Evidenz fehlt, aber noch keine stabile
FaultIdentity klassifiziert ist.

## Allowed

Nur bei: SafetyStorage healthy; Marker resolved/none; kein Fault/SafeBoot;
kein blockierender Fault; Configuration Operational; Runzustand eindeutig;
Air VALID; Cooling VALID; #21 Permission passend; ProductEvidence falls
aktueller ProductMode sie verlangt; ControlContext gueltig; PlannerWatchdog
nicht gelatcht. `Allowed` wird nie persistiert.

---

# 44. Kein Caller-supplied Allowed

Der planner-/driver-gebundene `TemperatureControlApplicationOrchestrator`
muss den zentralen `SafetyFaultService` bzw. einen daraus nicht faelschbaren
internen SafetyDirective-Provider besitzen. `tickActuatorPlan()` akzeptiert
im produktiven gebundenen Pfad **keinen** freien `ActuatorSafetyGateInput`.

Ablauf: SafetyDirective zentral ableiten; internen Plannerinput bauen;
Planner tick; FanDirective auf Plannerresultat anwenden; SinkDriver;
PlannerWatchdog-Evidenz an Safety zurueckmelden.

Der actorfreie aktuelle ESP-IDF-Skeletonroot bleibt actorfrei. #24 baut keine
Fake-Hardware auf.

---

# 45. Trust Boundary fuer Authorization

#24 implementiert keine PIN-Pruefung. Reset-/SAFE_BOOT-Evaluation erhaelt
`authorizationSatisfied` nur aus einer vertrauenswuerdigen Application-/Auth-
Grenze; passend zur Spalte `Auth` in Abschnitt 11 (`Operator`/`Service`).
UI/Web/Transport duerfen dieses Feld niemals als frei deserialisierbaren
Benutzerwert direkt setzen. Bis ein produktiver Auth-Producer existiert:
Serviceautorisierung im Produktpfad = `false`. Tests duerfen beide Zustaende
deterministisch injizieren. Kein Capability-/Token-Framework.

---

# 46. SafetyEvents fuer #19 (F6, mit bewiesenem Bound)

## 46.1 Typen

```text
FaultRaised
FaultCauseCleared
FaultRelapsed
FaultReset
SafeBootEntered
SafeBootExited
RestartPrepared
RestartObserved
RestartRejected
RunRecoveryHandoffPrepared      (neu, Abschnitt 29.3)
SafetyPersistenceFailed
EmergencyMarkerSet
EmergencyMarkerCleared
RunTerminatedByFaultReset
RunTerminatedBySafeBoot
```

`SafetyEvent` enthaelt mindestens: kind; optional code/source/instanceId;
optional primary; bootSequence; monotonicMillis; faultRevision (aus dem
jeweils zustaendigen Zaehler, Abschnitt 15).

## 46.2 Bound-Beweis

Jede atomare `SafetyMutation` entspricht **genau einem** Aufruf von `raise()`,
`clear()`, `resetFault()`, dem Boot-SAFE_BOOT-Uebergangskandidaten aus
Abschnitt 24.4, oder einer einzelnen Marker-/Redundanzoperation. **Normativ:**
Boot-Evaluation ruft `raise()` fuer jeden Producer sequenziell einzeln auf —
niemals wird eine einzelne Mutation genutzt, um mehrere unabhaengige
Identities gleichzeitig zu raisen. Diese Regel ist Teil des Vertrags, nicht
nur eine Implementierungsempfehlung; eine Implementierung, die batched, ist
ein Vertragsbruch.

Vollstaendige Enumeration der pro Mutation moeglichen Ereigniskombinationen:

| Mutation | Ereignisse | Anzahl |
|---|---|---|
| `raise()` neu | `FaultRaised` | 1 |
| `raise()` Relapse | `FaultRelapsed` | 1 |
| `clear()` | `FaultCauseCleared` | 1 |
| `resetFault()` ohne Run | `FaultReset` | 1 |
| `resetFault()` mit Run, Y4/Terminal | `FaultReset` + `RunTerminatedByFaultReset` | 2 |
| `resetFault()` mit Run, S3/RecoverIfProvable | `FaultReset` (Handoff ist eigene Mutation) | 1 |
| Recovery-Handoff (Abschnitt 29.3) | `RunRecoveryHandoffPrepared` | 1 |
| RestartIntent Prepare | `RestartPrepared` | 1 |
| RestartIntent Rejected | `RestartRejected` | 1 |
| Boot: RestartIntent konsumiert, kein Episodeende | `RestartObserved` | 1 |
| **Boot: RestartIntent konsumiert + Uebergang auf 3 (Abschnitt 24.4)** | `RestartObserved` + `FaultRaised`(Y4-005) + `SafeBootEntered` | **3** |
| SAFE_BOOT-Eintritt ohne Restart-Eskalation | `SafeBootEntered` | 1 |
| SAFE_BOOT-Exit | `SafeBootExited` | 1 |
| Run-Abandon (Abschnitt 26) | `RunTerminatedByFaultReset` oder `RunTerminatedBySafeBoot` | 1 |
| Normaler SafetyState-Commitfehler -> Marker | `SafetyPersistenceFailed` + `EmergencyMarkerSet` | 2 |
| Marker-Recovery | `EmergencyMarkerCleared` | 1 |

**Bewiesenes Maximum: 3** (Boot-Restart-Eskalation). Gewaehlter Bound:

```cpp
std::array<SafetyEvent, 4>
eventCount <= 4
```

4 statt des bewiesenen Maximums 3 als bewusste Sicherheitsmarge. Eine
Bound-Verletzung (mehr als 4 Events in einer Mutation) ist ein interner
Vertragsfehler und wird fail-closed behandelt (`Y4-007/SafetyCore`), nicht
still trunkiert. Keine dynamische Queue.

---

# 47. Fehlerinjektion

## Sensor

Reale #20/#21-Typen: VALID, STALE, FAILED, SafeLocked, CrossRole
indeterminate, Recovery.

## Planner

Realer #23-Watchdog durch echte stale/fehlende Requestfolge.

## Aktor

S3-005/S3-006 ueber typisierte injection-only Evidenz.

## Persistenz

`SimulatedPersistentStateStore`: WriteError, CapacityError,
`CommitOutcomeUnknown` -> neuer Wert, `CommitOutcomeUnknown` -> alter Wert,
ReadError, CRC corruption, semantisch ungueltig bei korrektem CRC,
SafetyState-Slotfehler, Marker-Slotfehler, Redundanzreparatur.

## Restart

`SimulatedResetController`: PowerOn, SoftwareRestart, WatchdogOrPanic,
Brownout, External, Unknown, Restart Requested/Rejected.

## Recovery-Handoff (neu)

Deterministisches Injizieren jedes `RunPersistenceResultStatus`-Werts aus
Schritt 2 in Abschnitt 29.3 (`Applied`, `WriteFailed`, `CapacityExceeded`,
`PersistenceIndeterminate`, `PersistenceCommittedApplyFailed`), um die
Crashmatrix aus Abschnitt 29.4 vollstaendig abzudecken.

---

# 48. Testmatrix – FaultCore

- alle 30 erlaubten Identities exakt einmal im Katalog;
- keine ungueltige Code-/Source-Kombination;
- keine Eviction;
- gleiche Identity + 100 wechselnde Mess-/Run-/Plannerrevisionen: gleiche
  instanceId;
- unabhaengige Multi-Faults;
- Klassenprioritaet;
- Intra-Class displayPriority;
- CauseClear;
- Relapse setzt false + neue faultRevision (im richtigen — persistenten oder
  transienten — Zaehler);
- alte ResetEvaluation nach Relapse stale;
- P1/O2 autoRearm nur wenn erlaubt (alle P1/O2 laut Matrix, Abschnitt 11);
- S3/Y4 nie auto reset;
- Primary/Follower (nur innerhalb derselben Klasse);
- Primaryreset loescht Follower nicht;
- persistente faultRevision overflow;
- persistente instanceId overflow;
- **transiente** faultRevision/instanceId Ueberlauf beeinflusst den
  persistenten Zaehler nicht (F2).

---

# 49. Testmatrix – Y4-006 Regression

Gleichzeitig `Y4-006/ConfigurationRuntime`, `Y4-006/Process`,
`Y4-006/SensorSet`. SensorSet qualifizieren. Erwartung: nur SensorSet
causeCleared, andere bleiben aktiv, Gate bleibt ImmediateStop. Danach jede
Domain einzeln. Innerhalb ConfigurationRuntime: zwei unresolved
Unterbedingungen simulieren, nur eine beheben, Identity darf noch nicht
causeCleared werden.

---

# 50. Testmatrix – Wireformat (F4)

- initialer Payload;
- 26 persistente Faults, aufsteigend nach instanceId;
- flags inkl. Ablehnung gesetzter reserved bits;
- Primary inkl. Ablehnung halb-gueltiger Kombinationen;
- safeBootRequired;
- RestartIntent (`None`=1, `Prepared`=2, `0` abgelehnt);
- RestartCounter 0..3;
- Golden Bytes (Liste aus Abschnitt 16.8);
- Roundtrip;
- max 759 Byte;
- unknown Code;
- unknown Source (Wert ausserhalb 1..17);
- duplicate Identity;
- duplicate instanceId;
- invalid Primary;
- invalid Count;
- falsche Persistenzreihenfolge (absteigend/unsortiert) wird abgelehnt;
- rest bytes;
- truncate;
- falsche Epoch;
- falsches Schema;
- falscher RecordType;
- CRC;
- **UTC-Feld gesetzt wird abgelehnt** (Abschnitt 16.7 ist keine
  Kann-Bestimmung).

---

# 51. Testmatrix – SafetyState Store

- beide NotFound -> duale Initialisierung;
- Powerloss nach sf0 vor sf1 -> kein normaler Allowed;
- zwei valide -> hoechste Revision;
- gleiche Revision/gleicher Payload;
- gleiche Revision/anderer Payload -> fail closed;
- eine Seite NotFound -> Recovery erforderlich;
- eine Seite CRC-defekt -> Recovery erforderlich;
- eine Seite ReadError -> Recovery erforderlich;
- Commit Success + readback;
- CommitOutcomeUnknown -> neu;
- CommitOutcomeUnknown -> alt;
- Readback mismatch;
- RecordRevision overflow.

---

# 52. Testmatrix – EmergencyMarker/Recovery

- beide NotFound -> kein Marker;
- Active + NotFound -> SAFE_BOOT;
- Active + alter Cleared -> Active gewinnt nach Sequence;
- neuer Cleared + alter Active -> Cleared nur bei healthy Markerhistorie;
- corrupt slot -> SAFE_BOOT;
- Markerwrite erster Slot fail -> zweiter Versuch;
- beide fail -> RAM fail-closed;
- MarkerRecovery repariert SafetyState-Redundanz;
- MarkerRecovery repariert Marker-Redundanz;
- MarkerRecovery schreibt safeBootRequired true;
- MarkerRecovery verlaesst SAFE_BOOT nicht;
- MarkerSequence overflow;
- unbekannter MarkerState/MarkerReason-Wert -> fail-closed (F4).

---

# 53. Testmatrix – Restart

- erster Boot;
- bootSequence increment einmal;
- bootSequence overflow;
- PowerOn ohne Intent;
- External ohne Intent;
- Brownout;
- Watchdog/Panic;
- Unknown;
- SoftwareRestart ohne Intent;
- Prepared + SoftwareRestart;
- Prepared + PowerOn/External;
- Prepared wird genau einmal konsumiert;
- Request Rejected -> Intent clear, kein Retry;
- Request Requested -> Intent bleibt bis Boot;
- gleicher (bereits entfernter) Fault-Instanzverweis nie zweiter Restart;
- Count 1;
- Count 2;
- Count 3 -> Y4-005 + safeBootRequired;
- Count saturiert 3;
- 29:59 normal stabil -> nicht clear;
- 30:00 normal stabil -> Count 0;
- Y4-005 in SAFE_BOOT: 30:00 -> nur causeCleared;
- Y4-005 Reset -> Count 0, SafeBoot bleibt (Y4 ist Terminal).

---

# 54. Verpflichtende S3-Testmatrix (Auftrag §11)

## S3, Recovery beweisbar

```text
FERMENTING
-> S3
-> FAULT
-> Ursache stabil wieder qualifiziert
-> causeCleared
-> geschuetzter Reset (Latch entfernt)
-> #17-Korrekturbuchung (persistRecoveryCandidate) Applied
-> RestartIntent Prepared, kontrollierter Restart
-> naechster Boot: loadAndInitialize() sieht Fermenting (nicht Fault)
-> Hop-1/Hop-2 (unveraendert): Recovery beweisbar
-> Lauf korrekt reaktiviert
```

Beweisen: kein Aktor vor vollstaendiger #18-Recovery; keine alte
Planner-/PI-Anforderung direkt wiederverwendet; keine alte Zielqualifikation
blind uebernommen; Fortschritt nur gemaess #18; aktuelle
Sensor-/Safety-/Control-Evidenz; write-before-apply in jedem Schritt.

## S3, Recovery nicht beweisbar

```text
FAULT
-> Reset zulaessig (wie oben, Latch entfernt, Korrekturbuchung, Restart)
-> naechster Boot: #18 kann sicheren Lauf nicht beweisen
   (unveraenderte, bestehende #18-Regeln, z. B. WaitingForProduct-Expiry
   oder Evidenz indeterminiert)
-> #18s eigene bestehende Terminalisierung bzw. Run-Abandon-Pfad
-> STANDBY
```

Kein Raten; #24 fuehrt hier keine eigene Logik aus.

## Weitere S3-Faelle

- S3 in `QUALIFYING_TARGET`;
- S3 in `FERMENTING`;
- S3 in `COOLING`;
- S3 in `COOL_HOLDING`;
- S3 in `MANUAL_HOLDING`;
- S3 ohne aktiven Run (Standby) -> sofortiges `FAULT -> STANDBY`, kein
  Restart;
- Reboot **waehrend** S3-FAULT, bevor ein Reset versucht wurde -> SAFE_BOOT
  (Latch noch aktiv), kein Recovery-Handoff;
- Crash nach S3-Reset, vor Korrekturbuchung -> Y4-001-Orphan-Klassifikation
  beim naechsten Boot (Abschnitt 29.4);
- Crash nach Korrekturbuchung, vor Restart -> naechster (auch
  unkontrollierter) Boot rekonstruiert normal ueber #18;
- mehrere gleichzeitige S3 -> erst nach Reset des letzten blockierenden S3
  greift 29.2/29.3;
- S3 + Y4 gleichzeitig: Y4-Terminalitaet dominiert (kein Recovery-Handoff,
  solange ein Y4 aktiv ist).

---

# 55. Verpflichtende Y4-Testmatrix (Auftrag §12)

1. Y4 bei aktivem Lauf;
2. sofortige sichere Reaktion;
3. kein automatisches Resume;
4. Ursache behoben;
5. `causeCleared`;
6. geschuetzter Reset;
7. alter Run terminalisiert (`clearActiveRunState()`, Abschnitt 29.2);
8. `NoActiveRun/STANDBY` oder SAFE_BOOT bleibt bis separatem Exit;
9. kein #18-Resume, kein Recovery-Handoff (Abschnitt 29.3 gilt nicht fuer
   Y4);
10. Reboot an jeder kritischen Zwischenstelle fail-closed.

---

# 56. Nicht rekonstruierbare Run-Daten – Pflichtfaelle (Auftrag §13)

Mindestens: `NotReconstructible`, `NotReconstructibleOrphanedState`,
`UnsupportedSchema`, `ForeignEpoch`, `PreparedInterrupted`, `ReadFailed`,
`CapacityExceeded`; Store wird spaeter wieder technisch gesund; autorisierte
Run-Terminalisierung ueber `abandonUnrecoverableRun()` (Abschnitt 26); neuer
`NoActiveRun`-Commit; Bestaetigung ueber das bestehende
CAS-/Next-Boot-Revalidierungsmodell (kein synchrones Readback, Abschnitt
26.2 Schritt 5); kein Factory Reset; Programme/Configuration unveraendert;
zugehoeriger Y4-Latch bleibt bis Reset; SAFE_BOOT separat; Crash vor/nach
Terminalisierungscommit (identisch zu jedem anderen `#17`-Schreibfehler,
Abschnitt 39).

---

# 57. Testmatrix – FaultReset / #15

- targetFaultInstanceId 0;
- stale (persistente) faultRevision;
- cause active;
- auth false (gemaess Abschnitt-11-Spalte `Auth`);
- safetychecks false;
- andere uncleared gleich-/hoeherklassige Ursache;
- andere cause-cleared Latches duerfen Zielreset nicht blockieren;
- #15 mutiert FaultCore nicht;
- #15 setzt criticalSafetyEventPending nicht selbst false;
- ResetFault bleibt non-persisted RunCommand;
- SafetyState ist Reset-Linearisierung;
- Command-RAM-Bookkeeping erst nach Safetycommit;
- Projektion danach synchronisiert;
- Y4/S3-ohne-Run -> sofortiges FaultResetCompleted;
- S3-mit-Run -> Recovery-Handoff statt FaultResetCompleted (Abschnitt 29.3).

---

# 58. Testmatrix – Fanpolicy

- Sensorfault waehrend aktivem Peltier -> Peltier sofort Idle, #23 RunOn;
- S3-005 OuterFan -> ForceOff dominiert;
- S3-006 OuterFan -> ForceOn;
- S3-006 InnerFan -> Inner ForceOff;
- gleichzeitig OuterFan ForceOn + elektrischem OuterFan Fault -> ForceOff
  gewinnt;
- kein Fault kann Peltier trotz ImmediateStop aktivieren;
- keine Fanpolicy behauptet Feedback.

---

# 59. Testmatrix – Configuration Gate

Reale Producer: `ConfigurationRuntimeFailure`, `ConfigurationUnavailable`,
`ConfigurationIntegrityFailure`, `CommitIndeterminate`. Jeweils: Y4 Identity;
persistiert; gate ImmediateStop; reboot retain; SAFE_BOOT; Recovery -> nur
causeClear; Reset notwendig; SafeBootExit separat; Plannerpfad kein Bypass.

---

# 60. Testmatrix – SAFE_BOOT

- persistenter S3/Y4 beim Boot -> safeBootRequired true;
- RestartCount 3 -> safeBootRequired true;
- config Y4 -> safeBootRequired true;
- run Y4 -> safeBootRequired true;
- Markerproblem -> SAFE_BOOT;
- aktiver geladener Run wird nicht aktiviert;
- Run-Abandon fuer SAFE_BOOT (Abschnitt 26);
- RunStore indeterminate -> kein Exit;
- Reboot verlaesst SafeBoot nicht;
- Faultreset in SafeBoot verlaesst SafeBoot nicht (weder Y4- noch
  S3-Reset — SAFE_BOOT-Exit ist immer separat);
- MarkerRecovery verlaesst SafeBoot nicht;
- Exit ohne Auth -> reject;
- Exit mit Air STALE -> reject;
- Exit mit Cooling STALE -> reject;
- Exit mit Config nicht Operational -> reject;
- Exit mit Run nicht NoActiveRun -> reject;
- alle Bedingungen -> safety flag clear -> SafeBootExitCompleted -> Standby;
- kein direkter Aktortest aus SafeBoot.

---

# 61. Dokumentation

Dauerhaft aktualisieren: `docs/SAFETY_AND_FAULTS.md`,
`docs/SAFETY_COMPONENT_FAULTS.md`, `docs/SYSTEM_SAFETY_AND_RECOVERY.md`,
`docs/ACCEPTANCE_TESTS.md`, `docs/RUN_COMMANDS.md`, `docs/STATE_MACHINE.md`,
`docs/RUN_PERSISTENCE.md`, `docs/RECOVERY_AND_INTERRUPTION.md`,
`docs/ARCHITECTURE.md`, `docs/ROADMAP.md`.

Wesentliche neue dauerhafte Aussagen: finaler Faultcodekatalog;
**S3 = `RecoverIfProvable` ueber kontrollierten Restart in die bestehende
#18-Logik, Y4 = `Terminal`**; `FAULT -> STANDBY` fuer Y4/S3-ohne-Run;
`Run-Abandon-Pfad` (`abandonUnrecoverableRun`); Restart 3/30min;
SafetyState/EmergencyMarker inkl. exaktem Wireformat; MarkerRecovery !=
SafeBootExit; #15/#24/#17-Autoritaetsgrenze; Fan-Safety-Directive; getrennte
persistente/transiente Faultzaehler.

Keine PR-Reviewhistorie in kanonischen Fachdocs.

---

# 62. Voraussichtliche Dateien

## Neu `fermentation_app`

```text
fault_types.hpp
fault_catalog.hpp/.cpp
fault_core.hpp/.cpp
safety_events.hpp
safety_state_codec.hpp/.cpp
safety_state_store.hpp/.cpp
safety_emergency_marker_store.hpp/.cpp
safety_storage_contract.hpp
restart_supervisor.hpp/.cpp
safety_fault_service.hpp/.cpp
safety_process_coordinator.hpp/.cpp
actuator_diagnostic_types.hpp
```

Kleine Klassen/Module nach Verantwortung; kein einzelnes
1800-Zeilen-Servicefile.

## Geaendert

```text
run_commands.hpp/.cpp
process_state_machine.hpp/.cpp
run_persistence_coordinator.hpp/.cpp   (neu: abandonUnrecoverableRun())
temperature_control_orchestrator.hpp/.cpp
actuator_plan_types.hpp
```

Nur tatsaechlich notwendige Aenderungen. `persistRecoveryCandidate()` und
`clearActiveRunState()` werden **wiederverwendet**, nicht geaendert.

## device_platform

```text
reset_controller.hpp
```

## test support

```text
simulated_reset_controller.hpp/.cpp
```

Bestehenden PersistentStateStore-Simulator erweitern statt zweitem Store.

---

# 63. Ressourcenregeln

- FaultCore: feste `std::array`, keine unbounded Container;
- maximal 30 Runtime-Identitaeten (4 transient + 26 persistent);
- maximal 26 persistente Faultrecords;
- SafetyState Envelope <= 1024 Byte;
- Marker Envelope <= 64 Byte;
- keine Writes pro Control-Tick;
- gleiche aktive FaultIdentity ohne Zustandsaenderung erzeugt keinen Write;
- Restart stable clear maximal ein Write beim Grenzerreichen;
- SafetyEvents fixed array (Bound 4, bewiesenes Maximum 3);
- keine PSRAM-Annahme;
- finaler Base-/Head-Ressourcenvergleich gemaess Quality-Gates.

Keine exakte C++-Struct-RAMgroesse wird vor Compiler-/Buildmessung
behauptet.

---

# 64. Architekturdiagramme

## 64.1 Runtime

```text
#20/#21/#22/#23/#56/#57/#17 Producer
        |
        v
SafetyFaultService / FaultCore
  (getrennt: PersistentFaultState S3/Y4 | TransientFaultState P1/O2)
        |
        +--> SafetyStateStore / EmergencyMarkerStore
        |
        +--> ActuatorSafetyDirective
        |       |
        |       v
        |   bestehender #23 Planner
        |       |
        |       v
        |      Sink
        |
        +--> SafetyProcessCoordinator
                |
                +--> #17 Persistence (persistTransition,
                |         persistRecoveryCandidate, abandonUnrecoverableRun)
                +--> #18 Recovery (unveraendert: loadAndInitialize,
                |         activateLoadedRun/activateFallbackRecoveredRun)
                +--> bestehende ProcessStateMachine
```

Nur eine Faultautoritaet. Nur eine Recoveryautoritaet. Nur ein
Run-Persistenzkern.

## 64.2 S3 vs. Y4

```text
S3 (RecoverIfProvable):
FAULT
 -> CauseClear
 -> geschuetzter Reset (Latch entfernt)
 -> aktiver Run vorhanden?
      nein -> FAULT -> STANDBY (wie Y4)
      ja   -> #17-Korrekturbuchung (persistRecoveryCandidate)
              -> kontrollierter Restart
              -> naechster Boot: UNVERAENDERTE #18-Logik
                   -> Resume
                   oder
                   -> #18s eigene Terminalisierung / Run-Abandon -> STANDBY

Y4 (Terminal):
FAULT / SAFE_BOOT
 -> Ursache beheben
 -> geschuetzter Reset
 -> alter Run terminal (clearActiveRunState)
 -> STANDBY bzw. separater SAFE_BOOT-Exit
 -> KEIN Recovery-Handoff, KEIN Restart-fuer-Recovery-Zweck
```

---

# 65. Umsetzungsslices

Jeder Slice: gezielte Tests; direkt betroffene Konsumententests;
`git diff --check`; Architekturgrenzen; Secretcheck; Ownerreview; kein
Full-Suite-Zwang waehrend Draft.

## Slice 1 – FaultCore + vollstaendiger Catalog

Typen; 16 Codes; 30 erlaubte Identities (Matrix aus Abschnitt 11 vollstaendig
als compile-time Katalog); **getrennte persistente/transiente Zaehler
(F2)**; Policy; DisplayPriority; CauseClear/Relapse; Primary/Follow-up
(klassenintern); Fanpolicy rein; Events rein.

## Slice 2 – SafetyState Wire/Store

RecordType 9; sf0/sf1; duale Init; exaktes Wireformat inkl. Flag-Bits,
Persistenzreihenfolge, No-UTC-Regel (F4); 759-Byte-Max; semantic decode;
CommitOutcomeUnknown; Redundanz; Golden-Byte-Tests (16.8).

## Slice 3 – EmergencyMarker + Repair

RecordType 10; sem0/sem1; exakte MarkerState-/MarkerReason-Werte (F4);
bounded two-slot fallback; Redundanzrecovery; MarkerRecovery; kein
SafeBootExit.

## Slice 4 – RestartSupervisor

ResetPort; Simulator; bootSequence; RestartIntent (vereinheitlicht fuer alle
S3, nicht mehr S3-004-spezifisch); 3/30min; Y4-005.

## Slice 5 – Process-/Run-Safety inklusive S3-Recovery vs. Y4-Terminal

Runtime S3/Y4 -> Fault inkl. RAM-Erhalt der Pre-Fault-`ProcessRuntimeState`;
`FaultResetCompleted -> Standby` (Y4/S3-ohne-Run); `clearActiveRunState`;
**`abandonUnrecoverableRun()`** (verallgemeinerter Run-Abandon-Pfad,
Abschnitt 26); **S3-Recovery-Handoff ueber `persistRecoveryCandidate()` +
kontrollierten Restart** (Abschnitt 29.3); `SafeBootExitCompleted`; die
vollstaendige Crashmatrix aus Abschnitt 29.4.

## Slice 6 – reale Producer #20/#21/#22/#23

Sensor; Selection; Control; Watchdog; injection-only actuator; FanDirective.

## Slice 7 – reale Producer #17/#18/#56/#57 + Boot

Exakte Runmappingmatrix (Abschnitt 38); Config Gate; Bootreihenfolge;
safeBootRequired true; geladenen Run nicht reaktivieren; Boot-Orchestrator,
der `loadAndInitialize()` erstmals produktiv verdrahtet.

## Slice 8 – #15 FaultReset + Actuator Bypass

targetFaultInstanceId; #15 keine SafetyMutation; ResetFault non-persisted
bestaetigen; SafetyState reset linearisieren; Orchestrator SafetyDirective
zwingend; kein caller Allowed.

## Slice 9 – Dokumentation und Abschluss

Alle kanonischen Quellen (Abschnitt 61); Roadmap; vollstaendiges
Ownerreview. Erst bei 0 Findings und Owner-Anweisung: Full Native; Builds;
Ressourcenvergleich; CI. Hardware bleibt `NOT_RUN`, solange nicht separat
freigegeben/verkabelt.

---

# 66. Aufgabenliste

## Plan-Gate

- [x] aktuelle Planfassung vollstaendig ersetzt
- [x] S3=`RecoverIfProvable`/Y4=`Terminal` vollstaendig umgesetzt (F1)
- [x] persistente/transiente Fault-IDs getrennt (F2)
- [x] Run-Abandon-Pfad verallgemeinert spezifiziert (F3)
- [x] alle Wirewerte, Flagbits, Reihenfolge, UTC-Vertrag fest (F4)
- [x] vollstaendige FaultCatalog-Matrix (F5)
- [x] SafetyEvent-Bound bewiesen (F6)
- [x] Roadmap gegen Live-Stand geprueft
- [ ] PR-Body neue exakte Plan-SHA
- [x] Implementation `NOT_STARTED`
- [ ] `git diff --check`/Architektur-/Secret-Gates
- [x] Tests/Builds `NOT_RUN`
- [ ] SESSION HANDOVER mit neuer exakter Plan-SHA
- [ ] Ownerfreigabe der exakten neuen Plan-SHA

## Slice 1

- [ ] FaultCore mit getrennten Zaehlern
- [ ] vollstaendiger Katalog (30 Identities)
- [ ] Prioritaet
- [ ] Relapse
- [ ] Events
- [ ] Ownerreview

## Slice 2

- [ ] SafetyState wire (Flags, Reihenfolge, No-UTC)
- [ ] duale Init
- [ ] Store
- [ ] Golden Bytes
- [ ] Redundanztests
- [ ] Ownerreview

## Slice 3

- [ ] Marker (exakte Wirewerte)
- [ ] two-slot fallback
- [ ] Reparatur
- [ ] MarkerRecovery != Exit
- [ ] Ownerreview

## Slice 4

- [ ] ResetPort
- [ ] RestartIntent (vereinheitlicht)
- [ ] bootSequence
- [ ] 3/30min
- [ ] Ownerreview

## Slice 5

- [ ] Runtime Fault + Pre-Fault-State-Erhalt
- [ ] Fault -> Standby (Y4/S3-ohne-Run)
- [ ] `abandonUnrecoverableRun()`
- [ ] S3-Recovery-Handoff (`persistRecoveryCandidate` + Restart)
- [ ] Crashmatrix (Abschnitt 29.4) vollstaendig getestet
- [ ] ProcessEvents
- [ ] Ownerreview

## Slice 6

- [ ] #20
- [ ] #21
- [ ] #22
- [ ] #23 (vereinheitlichter Restart)
- [ ] FanDirective
- [ ] Ownerreview

## Slice 7

- [ ] #17/#18 mapping
- [ ] #56/#57
- [ ] Config gate
- [ ] Boot-Orchestrator
- [ ] Ownerreview

## Slice 8

- [ ] #15 Reset
- [ ] non-persisted ResetFault
- [ ] no Safety duplicate
- [ ] no caller Allowed
- [ ] Planner/Sink integration
- [ ] Ownerreview

## Slice 9

- [ ] kanonische Doku
- [ ] Roadmap
- [ ] komplettes PR-Review
- [ ] 0 Findings
- [ ] Owner-Gesamtlauf

---

# 67. Stop-/Replan-Grenzen

Neue vollstaendige Planrevision und neue Owner-SHA erforderlich bei
Aenderung an: FaultCode-/Source-Katalog; FaultIdentity; S3-Recovery- vs.
Y4-Terminal-Entscheidung; persistente/transiente Trennung; Safety
Wireformat; RecordType/Keys/Epoch; Restart 3/30min; EmergencyMarker-
Semantik; #15/#17/Safety-Commitreihenfolge; Run-Abandon-Vertrag;
Fan-Safety-Directive; Aktor-Gate-Autoritaet; neuer Hardware-/ESP-IDF-
Abhaengigkeit; neu behauptetem realen Producer.

Lokale mechanische Implementierungsdetails innerhalb dieser Vertraege
benoetigen keine Planrevision.

---

# 68. Definition of Done

Issue #24 ist abgeschlossen, wenn:

- vier Klassen implementiert;
- stabile Codes implementiert;
- alle 30 erlaubten Identities compile-time begrenzt;
- persistente/transiente Fault-Zaehler vollstaendig getrennt;
- unabhaengige Ursachen koexistieren;
- kein last-origin-wins;
- Relapse stale-t alte Resetbewertungen;
- S3/Y4 persistent;
- Quittierung/Clear/Reset getrennt;
- **S3-Reset mit aktivem Run fuehrt ueber einen kontrollierten Restart in die
  unveraenderte #18-Recovery und resultiert je nach deren Ergebnis in Resume
  oder Terminal**;
- **Y4-Reset terminalisiert den Run immer, ohne Restart-fuer-Recovery-
  Zweck**;
- 3 abnormale Boots -> SAFE_BOOT;
- 30-min-Vertrag getestet;
- SAFE_BOOT rebootfest;
- aktiver Run in SAFE_BOOT nicht reaktiviert, sondern ueber
  `abandonUnrecoverableRun()` terminalisiert;
- EmergencyMarker getrennt;
- Redundanzrepair getestet;
- MarkerRecovery != Exit;
- #17/#18/#20/#21/#22/#23/#56/#57 real konsumiert, ohne #18 zu veraendern;
- Configuration-Safety-Gate vollstaendig;
- Fan-Safetyreaktionen modelliert;
- kein caller-supplied Allowed;
- Fehlerinjektionen reproduzierbar, inklusive Recovery-Handoff-Fehlerpfade;
- SafetyEvents bounded und mit bewiesenem Maximum bereitgestellt;
- kanonische Doku konsistent;
- gezielte und final geforderte Tests bestanden (inklusive S3-/Y4-
  Testmatrizen aus Abschnitt 54/55/56);
- nicht ausgefuehrte Hardwaretests ehrlich `NOT_RUN`/`BLOCKED`;
- keine Hardwarewerte erfunden.

---

# 69. Plan-Gate

Nach Uebernahme dieser vollstaendigen Fassung in PR #108:

1. nur Plan/PR-Body/Handover aendern (Roadmap nur bei realem Bedarf,
   Abschnitt 70);
2. keine Produktions- oder Testimplementation;
3. neue exakte Plan-SHA committen/pushen;
4. `git diff --check`;
5. Architekturgrenzen;
6. Secretscan;
7. Tests/Builds `NOT_RUN`;
8. Handover;
9. anhalten.

**Keine Implementierung vor ausdruecklicher Ownerfreigabe exakt dieser neuen
Plan-Commit-SHA.**

---

# 70. Live-Statusabgleich dieser Planrunde

`origin/main` unveraendert bei `b8eae5f4da5f2666b5a9bda333d115254c4db5b2`
seit der vorherigen Planrunde (`33f7394`). PR #108 Remote-HEAD war vor
Beginn dieser Runde exakt `33f7394678ac73abd083aa38b95f743ebfa13f94`. Kein
Drift. `docs/ROADMAP.md` wurde erneut gegen den Live-Stand geprueft und ist
weiterhin akkurat (kein hartkodierter Plan-SHA-Verweis, #22/#23 korrekt als
abgeschlossen/gemergt markiert, #24/PR #108 als aktuelle Arbeit, #19 als
naechste fachliche Arbeit) — keine Aenderung in dieser Runde.
