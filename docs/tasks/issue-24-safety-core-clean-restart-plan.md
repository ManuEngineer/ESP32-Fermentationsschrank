# Issue #24 – verbindlicher Ersatzplan: Safety-Core, Verriegelung, Restart und SAFE_BOOT

Status: **PLAN – noch nicht zur Umsetzung freigegeben**

Issue: #24 – `[E3.5] Fehlerklassen, Verriegelung, SAFE_BOOT und Fehlerinjektion`

Branch: `agent/issue-24-safety-core-clean-restart`

Draft-PR: #108

Basis bei Erstellung dieses Plans:

```text
main @ b8eae5f4da5f2666b5a9bda333d115254c4db5b2
```

Diese Datei ist eine **vollstaendige eigenstaendige Planfassung**. Sie setzt weder PR #107 noch die vorherige Planfassung in PR #108 als Vertragsquelle voraus.

---

# 1. Ziel

Issue #24 implementiert den Release-1-Safety-Core als **eine einzige zentrale Fault-, Latch-, Reset-, Restart- und SAFE_BOOT-Autoritaet** innerhalb von `fermentation_app`.

Der Core:

- klassifiziert Fehler in P1/O2/S3/Y4;
- fuehrt stabile maschinenlesbare Fehlercodes;
- haelt gleichzeitig aktive unabhaengige Ursachen getrennt;
- sperrt Aktoren fail-closed;
- verriegelt S3/Y4 persistent;
- trennt Quittierung, Cause-Clear und Reset;
- konsumiert reale Producer aus den bereits gemergten Issues;
- verwaltet genau einen kontrollierten Restartversuch pro dafuer vorgesehenem Faultvorgang;
- eskaliert wiederholte abnormale Restarts nach `SAFE_BOOT`;
- speichert kritische Safetydaten redundant und atomar;
- besitzt einen getrennten minimalen Emergency-Marker fuer Safety-Persistenzfehler;
- liefert reproduzierbare native Fehlerinjektionen;
- erzeugt typisierte Fault-/Reset-/SAFE_BOOT-Ereignisse fuer das spaetere Journal aus #19;
- laesst keinen produktiven Planner-/Sinkpfad an der zentralen Safetybewertung vorbei.

Der Plan ist absichtlich fuer ESP32-WROOM-32E mit 4 MB Flash und ohne PSRAM begrenzt.

---

# 2. Verbindliche Quellen und Wiederverwendung

Vor jedem Umsetzungsschnitt werden mindestens die folgenden bereits gemergten Vertraege wiederverwendet und nicht nachgebaut:

| Bereich | bestehende Quelle |
|---|---|
| Prozesszustand | #14 / `process_state_machine.*` |
| Commands / Staleness / Bestaetigung | #15 / `run_commands.*`, `RUN_COMMANDS.md` |
| Run-Persistenz | #17 / `RunPersistenceCoordinator` |
| Recovery | #18 |
| Sensorqualitaet | #20 / `SensorQualitySnapshot` |
| Sensorselektion | #21 / `SensorSelectionRuntimeState`, BlockReason |
| PI / ControlResult | #22 / `TemperatureControlResult` |
| Planner / Watchdog / Sink | #23 / `ActuatorPlanner`, `ActuatorWatchdogFaultEvidence`, `ActuatorPlanSinkDriver` |
| Configuration Runtime | #56 / `ConfigurationServiceMode`, RuntimeFailure, CommitIndeterminate |
| Configuration Recovery | #57 / `ConfigurationRecoveryResult`, `ConfigurationSafetyProducer` |
| Persistenzport | `device_platform::IStateStore` |
| Wire-Envelopes | `StorageEnvelope`, Big-Endian-Codec, CRC |
| technischer Slot-Scan | `scanTechnicalSlotCandidates()` / `loadSlotPayload()` |
| Zeit | `ITimeSource` |
| spaeteres Journal | #19 – **nicht** in #24 implementieren |

Repositoryregeln aus `AGENTS.md`, den lokalen `AGENTS.md`, `AGENT_WORKFLOW.md`, `ENGINEERING_PRINCIPLES.md` und ADR-013 bleiben verbindlich.

## 2.1 Adopt-or-build / Bibliotheken

Vor der Umsetzung ist kein neuer Drittanbieter-Fault-Framework-Baustein vorgesehen.

Begruendung:

- die fachliche Fault-/Reset-/SAFE_BOOT-Semantik ist projektspezifisch;
- die benoetigten technischen Primitive existieren bereits im Repository:
  `IStateStore`, `StorageEnvelope`, Slot-Scan, Big-Endian-Codec, CRC,
  `ITimeSource`, Run-Persistenz, Sensor-/Control-/Planner-Vertraege;
- eine zusaetzliche allgemeine Fault-FSM- oder Persistence-Bibliothek wuerde
  dieselben Domaenenregeln nur hinter einer weiteren Abstraktionsschicht
  verstecken.

Damit wird das Rad bei den technischen Primitive **nicht** neu erfunden, waehrend
die projektspezifische Safety-Policy bewusst im eigenen Fachkern bleibt.

Fuer einen spaeteren ESP-IDF-Resetadapter ist die native ESP-IDF-Resetursachen-
und Restart-API zu verwenden. Issue #24 implementiert diesen Adapter nicht.
Es entsteht in diesem Plan keine neue Drittanbieter-Lizenzabhaengigkeit.

---

# 3. Nicht-Scope

Issue #24 implementiert **nicht**:

- ESP-IDF-Resetadapter;
- `esp_restart()`-Adapter;
- NVS-Adapter oder Issue #90;
- GPIO-, BTS7960-, Fan- oder Sensor-Hardwareadapter;
- Fan-Tachometer;
- neue Strommessung;
- reale thermische Commissioning-Grenzwerte;
- neue PIN-/Login-/Sessionlogik;
- OTA;
- Journalpersistenz, Retention, Export oder Import aus #19;
- physische Flashredundanz ausserhalb des vorhandenen Flash;
- kuenftige Hardwarediagnose als angeblich realen Producer;
- ein allgemeines Event-Bus-, Plugin-, Provider- oder Safety-Framework.

Der aktuelle ESP-IDF-Root bleibt ein actorfreier Bring-up-/Skeletonpfad, solange er laut aktuellem Vertrag `real actuators: disabled` ist. Issue #24 baut dort keine kuenstlichen Hardware-/NVS-Abhaengigkeiten vor.

---

# 4. Festgelegte Release-1-Entscheidungen

Die folgenden technischen Entscheidungen sind Bestandteil dieses Plans. Mit der Freigabe der exakten Plan-SHA sind sie fuer Issue #24 freigegeben.

## 4.1 Restartgrenze

```text
3 abnormale Restart-Boots innerhalb derselben Restart-Episode
=> SAFE_BOOT
```

Eine Restart-Episode endet erst nach:

```text
30 Minuten ununterbrochener stabiler Laufzeit
```

ohne:

- neuen abnormalen Reset;
- aktiven S3/Y4-Fault;
- offenen RestartIntent;
- SAFE_BOOT.

Die 30 Minuten werden ausschliesslich mit der aktuellen bootlokalen monotonen Zeit gemessen. Es wird **keine Zeit ueber Reboots hinweg subtrahiert oder geraten**.

PowerOn/External-Reset ohne offenen RestartIntent erhoeht den abnormalen Zaehler nicht, loescht ihn aber auch nicht automatisch. Brownout, Watchdog/Panic, unbekannte Resetursache und unerwarteter Software-Reset sind abnormal.

Ein von #24 vorbereiteter kontrollierter Software-Restart zaehlt als Teil der abnormalen Restart-Episode. Er darf pro ausloesender Faultinstanz genau einmal automatisch angefordert werden.

## 4.2 Safety-Storage-Epoch

Safetyrecords verwenden in Schema 1 eine eigene, vom Configuration-Werksreset unabhaengige:

```text
SafetyStorageEpoch = 1
```

Sie wird **nicht** mit der Configuration-`StorageEpoch` fortgeschrieben.

Begruendung:

- #57 macht alte Configuration-Epochen bei Werksreset absichtlich unerreichbar;
- #57 delegiert Safety-Latches und Safetyfreigabe ausdruecklich an #24;
- ein Configuration-Werksreset darf keinen Safety-Latch nebenbei loeschen.

Eine spaetere explizite Safety-Reset-/Migrationsepoch benoetigt einen eigenen freigegebenen Vertrag.

## 4.3 Safety-Recordtypen und Keys

Nach den bereits belegten RecordTypeIds 1..8 werden fuer #24 fest reserviert:

```text
RecordTypeId 9  = SafetyStateRecord
RecordTypeId 10 = SafetyEmergencyMarker
```

Keys:

```text
sf0
sf1
sem0
sem1
```

Alle Keys liegen innerhalb des bestehenden 15-Zeichen-Vertrags.

## 4.4 Safety-Recordlimits

```text
SafetyStateRecord max envelope bytes: 1024
SafetyEmergencyMarker max envelope bytes: 64
```

Das sind harte Softwarelimits fuer diesen Vertrag, keine Behauptung ueber reale NVS-/Flashlebensdauer.

---

# 5. Eine einzige Safety-Autoritaet

## 5.1 Eigentuemerschaft

`SafetyFaultService` besitzt als einzige mutable Autoritaet:

- aktive Faultinstanzen;
- `faultRevision`;
- Faultinstanz-IDs;
- Cause-Clear;
- Acknowledgementstatus;
- Primary-/Follow-up-Bezug;
- persistente S3-/Y4-Latches;
- RestartIntent;
- RestartEpisode;
- `safeBootRequired`;
- Safety-Persistenzqualifikation;
- Resetentscheidung;
- SAFE_BOOT-Exit.

Andere Komponenten duerfen Safety nur:

- beobachten;
- typisierte Producerdaten melden;
- immutable Snapshots/Entscheidungen konsumieren.

Keine andere Komponente fuehrt einen zweiten FaultCore.

## 5.2 Nicht autoritative Projektion in `RunCommandState`

Die bereits vorhandenen Felder:

```cpp
std::uint32_t faultRevision;
bool criticalSafetyEventPending;
```

bleiben erhalten, sind aber nur eine **RAM-Projektion** des `SafetyFaultService`.

Sie sind nicht Quelle der Wahrheit.

Nach Boot und nach jeder Safety-Mutation werden sie aus dem zentralen Service synchronisiert.

Sie gehoeren weiterhin nicht in den #17-Run-Snapshot.

---

# 6. Stabiler Fault-Katalog

## 6.1 Grundregel fuer Identitaet

Eine Faultidentitaet ist exakt:

```cpp
struct FaultIdentity {
    FaultCode code;
    FaultSource source;
};
```

**Nicht Teil der Identitaet** sind:

- Zeit;
- ControlRequestSequence;
- RunRevision;
- FaultRevision;
- Sensor-Messfolge;
- Plannerrevision;
- Persistenzrevision;
- Bootrevision;
- Restart-Evidence-ID;
- Config-StateRevision.

Solche Werte sind nur Evidenz oder Diagnosemetadaten.

Ein erneutes Beobachten derselben `(code, source)`-Ursache aktualisiert dieselbe aktive Instanz. Es erzeugt keinen neuen Slot.

## 6.2 Fehlerklassen

```text
P1 = Hinweis / Prozesswarnung
O2 = behebbarer Betriebsfehler
S3 = verriegelter Sicherheitsfehler
Y4 = schwerer Systemfehler
```

Klasse, Persistenz, Latchpolicy, Gatewirkung und Auto-Rearm werden zentral aus `FaultCode` und `FaultSource` abgeleitet.

Diese Policy wird **nicht redundant im Wireformat gespeichert**.

## 6.3 FaultSource

Release 1 verwendet einen festen kleinen Source-Typ:

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

Unbekannte Enumwerte werden nicht auf einen Default gemappt.

## 6.4 Stabile Codes

Numerische C++-Werte:

```text
P1-001 = 0x1001
O2-001 = 0x2001
O2-002 = 0x2002
S3-001 = 0x3001
S3-002 = 0x3002
S3-003 = 0x3003
S3-004 = 0x3004
S3-005 = 0x3005
Y4-001 = 0x4001
Y4-002 = 0x4002
Y4-003 = 0x4003
Y4-004 = 0x4004
Y4-005 = 0x4005
Y4-006 = 0x4006
Y4-007 = 0x4007
```

Sichtbarer technischer Code ist entsprechend `P1-001` usw.; Texte werden spaeter ueber den Code lokalisiert.

## 6.5 Katalog und reale Producer

| Code | Bedeutung | zulaessige Source(s) | aktuell realer Producer | persistent | Gate |
|---|---|---|---|---|---|
| P1-001 | Prozesswarnung | Process | vorhandene typisierte Prozesswarnung, soweit #24 sie konsumiert | nein | keine direkte Aktorsperre |
| O2-001 | Produktfuehler degradiert / Ersatzbetrieb | ProductSensor | #20/#21 | nein | durch #21-Permission bestimmt |
| O2-002 | erforderlicher fixer Sensor voruebergehend STALE | AirSensor, CoolingSensor | #20 | nein | ImmediateStop bis wieder VALID |
| S3-001 | erforderlicher fixer Sensor FAILED | AirSensor, CoolingSensor | #20 | ja | ImmediateStop |
| S3-002 | Sensor-/Rollenplausibilitaet sicherheitsrelevant unaufgeloest | SensorSet | #21 CrossRole/SafeLocked | ja | ImmediateStop |
| S3-003 | Safety-Temperaturgrenze | TemperatureSafety | **injection-only bis Commissioning-Producer real ist** | ja | ImmediateStop |
| S3-004 | Aktoranforderungs-Watchdog | ActuatorPlanner | #23 `latchedWatchdogFault` | ja | ImmediateStop, einmaliger kontrollierter Restart erlaubt |
| S3-005 | physische Aktordiagnose | Peltier, OuterFan, InnerFan | **injection-only; kein Tachometer/Stromsignal behaupten** | ja | ImmediateStop |
| Y4-001 | aktiver Lauf nicht sicher rekonstruierbar | RunPersistence | #17/#18 Load/Recovery | ja | ImmediateStop |
| Y4-002 | Configuration unavailable/integrity failure | ConfigurationRecovery | #57 | ja | ImmediateStop / SAFE_BOOT bei Boot |
| Y4-003 | Configuration runtime/commit indeterminate | ConfigurationRuntime | #56 | ja | ImmediateStop |
| Y4-004 | kritischer Persistenzfehler | RunPersistence, SafetyPersistence | #17 und #24-Store | ja | ImmediateStop |
| Y4-005 | Restartloop / Restart-Episode eskaliert | RestartSupervisor | #24 + ResetCause | ja | SAFE_BOOT |
| Y4-006 | sicherheitsrelevante Evidenz unbekannt/unaufgeloest | Process, SensorSet, Control, ActuatorPlanner, RunPersistence, ConfigurationRuntime, ConfigurationRecovery, SafetyPersistence, RestartSupervisor | reale Mappings bei unbekanntem/inkonsistentem Zustand | ja | ImmediateStop |
| Y4-007 | interne Safety-Invariante nicht beweisbar | SafetyCore | #24 selbst | ja | ImmediateStop / SAFE_BOOT |

## 6.6 Compile-time Capacity

Zulaessige `(FaultCode, FaultSource)`-Paare stehen in einem einzigen `constexpr`-Katalog.

Daraus werden compile-time abgeleitet:

```text
4 transiente P1/O2-Identitaeten
24 persistente S3/Y4-Identitaeten
28 Identitaeten insgesamt
```

Es gibt **keine dynamische freie Fault-Capacity**, die durch neue Correlations gefuellt werden kann.

Invarianten:

- jede aktive Identitaet existiert hoechstens einmal;
- ein nicht im Katalog vorhandenes Paar ist ein interner Vertragsfehler;
- keine Eviction aktiver Faults;
- kein Ringpuffer fuer aktive Faults;
- keine unbounded Liste.

---

# 7. Fault-Lifecycle

## 7.1 RAM-Zustand einer Faultinstanz

Mindestens:

```text
identity
instanceId
active
acknowledged
causeCleared
firstSeenBootSequence
firstSeenMonotonicMillis
detail
optional primary reference
restartAttempted
```

## 7.2 Raise

`raise`:

1. validiert `(code, source)` gegen den Katalog;
2. findet eine bereits aktive identische Ursache;
3. bei bestehender Ursache:
   - keine neue Instanz;
   - Diagnose-Detail darf aktualisiert werden;
   - kein redundanter Persistenzwrite, wenn sich kein persistenter Zustand aendert;
4. bei neuer Ursache:
   - neue `instanceId`;
   - `faultRevision` + 1 checked;
   - ImmediateStop im RAM sofort wirksam;
   - bei S3/Y4 persistieren;
   - erst nach bestaetigtem Persistenzcommit gilt der neue persistente Record als dauerhaft.

Kann eine neue persistente Ursache nicht gespeichert werden, wird **nicht** zur alten Freigabe zurueckgekehrt: RAM bleibt gesperrt und der Emergency-Marker-Pfad startet.

## 7.3 Acknowledge

Acknowledgement:

- aendert niemals `causeCleared`;
- aendert niemals Gatewirkung;
- aendert niemals Latch;
- bei S3/Y4 darf der Ack-Zustand persistent mitgefuehrt werden;
- erzeugt ein typisiertes `FaultAcknowledged`-Ereignis.

Nach Boot darf ein nicht sicher rekonstruierbarer Ack-Zustand konservativ als nicht quittiert gelten; Safetyveraenderung entsteht dadurch nicht.

## 7.4 Cause-Clear

Cause-Clear bedeutet nur:

```text
die konkrete Ursache wurde durch ihren realen Producer neu qualifiziert
```

Bei S3/Y4:

```text
active latch + causeCleared = true
```

Der Latch bleibt aktiv und das Gate bleibt gesperrt.

Bei explizit auto-rearm-faehigen P1/O2 darf die Instanz nach ihrer codebezogenen Requalifikation automatisch verschwinden.

## 7.5 Reset

Ein S3/Y4-Faultreset darf nur erfolgreich sein, wenn:

- Zielinstanz existiert und ist aktuell;
- `causeCleared == true`;
- aktuelle codebezogene Safetychecks bestanden sind;
- notwendige Autorisierung ist bestaetigt;
- keine andere gleich-/hoeherklassige **noch ungeklärte aktive Ursache** die Bewertung verhindert;
- Runzustand ist fuer den naechsten sicheren Schritt rekonstruierbar;
- Safety-Persistenz ist aktuell qualifiziert.

Wichtig gegen Reset-Deadlock:

Eine andere Instanz, deren Ursache bereits `causeCleared == true` ist, verhindert den Reset der Zielinstanz **nicht**. Sie bleibt aber selbst gelatcht und haelt das Aktorgate weiterhin geschlossen.

So koennen mehrere beseitigte Latches nacheinander sauber zurueckgesetzt werden, ohne dass der erste Reset bereits Aktoren freigibt.

---

# 8. Primary-/Follow-up-Vertrag

Eine neue Faultinstanz darf optional auf eine Primaerinstanz verweisen.

Persistiert werden:

```text
primaryInstanceId
primaryCode
primarySource
```

Regeln:

- keine Selbstreferenz;
- Primary-Code/Source muss einem bekannten Katalogeintrag entsprechen;
- die Primaerinstanz muss beim Erzeugen der Beziehung existiert haben;
- die Primaerinstanz darf spaeter bereits resettiert sein, ohne dass die Follow-up-Referenz ungueltig wird;
- Reset/Clear des Primary loescht den Follow-up niemals automatisch;
- Follow-up wird nur nach eigener Ursache-Clear-/Resetlogik geloescht.

Damit gibt es keine dangling aktive Pointerbeziehung und trotzdem bleibt die Ursache nachvollziehbar.

Historische Langzeitaufbewahrung der Beziehung gehoert spaeter in #19.

---

# 9. Persistenzarchitektur

## 9.1 Normaler SafetyStateRecord

Zwei Slots:

```text
sf0
sf1
```

RecordType:

```text
9
```

Schema:

```text
1
```

SafetyStorageEpoch:

```text
1
```

Envelope-`versionValue` ist die `SafetyStateRecordRevision`.

### Bootscan

`scanTechnicalSlotCandidates()` wird wiederverwendet.

Regeln:

- beide `NotFound` und kein Emergency Marker:
  - Safety-Namespace ist noch nicht initialisiert;
  - blanker Safetyrecord Revision 1 darf erzeugt werden;
- `ReadError`, `CapacityError`, CRC-, Envelope-, Schema- oder Identitaetsfehler:
  - **nicht** als `NotFound` behandeln;
  - SAFE_BOOT;
- technisch gueltige Kandidaten:
  - absteigend nach `versionValue`;
  - gewaehlten Payload erneut ueber `loadSlotPayload()` laden;
  - vollstaendig semantisch validieren;
- zwei technisch gueltige Kandidaten mit gleicher Revision aber unterschiedlichem Payload:
  - widerspruechlich;
  - SAFE_BOOT.

Ein korruptes zweites Safety-Slot wird nicht still ignoriert, weil es einen unbekannten neueren Latch enthalten koennte.

## 9.2 SafetyState-Payload

Big-Endian, expliziter Codec, kein `memcpy` eines C++-Structs.

Basisfelder:

| Feld | Wire |
|---|---:|
| `nextInstanceId` | u32 |
| `faultRevision` | u32 |
| `bootSequence` | u32 |
| `safeBootRequired` | u8 bool |
| `safeBootReason` | u8 enum |
| `abnormalRestartCount` | u8 |
| `restartIntentState` | u8 enum |
| `restartIntentFaultInstanceId` | u32 |
| `persistentFaultCount` | u8 |

Basisgroesse: **21 Byte**.

Pro persistentem aktiven Faultrecord:

| Feld | Wire |
|---|---:|
| `instanceId` | u32 |
| `code` | u16 |
| `source` | u8 |
| `flags` (`acknowledged`, `causeCleared`, `restartAttempted`) | u8 |
| `firstSeenBootSequence` | u32 |
| `firstSeenMonotonicMillis` | u64 |
| `primaryInstanceId` (`0` = keiner) | u32 |
| `primaryCode` (`0` ohne Primary) | u16 |
| `primarySource` (`0` ohne Primary) | u8 |
| `detail` | u16 |

Faultrecordgroesse: **29 Byte**.

Maximal 24 persistente Identitaeten:

```text
payload_max = 21 + 24 * 29
            = 717 Byte

envelope_no_utc = 37 Byte

record_max = 754 Byte
```

Das harte 1024-Byte-Limit besitzt damit Reserve fuer den Envelopevertrag, ohne eine zweite Payloadkopie oder 2-KiB-Safetystruktur zu verlangen.

Die Policy-Felder:

- Fehlerklasse;
- persistent ja/nein;
- Latch ja/nein;
- Auto-Rearm;
- Gatewirkung;
- erforderliche Resetstufe;

werden **nicht** gespeichert, weil sie deterministisch aus dem stabilen Katalog folgen.

## 9.3 Semantische Decode-Validierung

Der Decoder akzeptiert nur:

- `persistentFaultCount <= 24`;
- bekannte Code-/Source-Kombination;
- nur S3/Y4 in diesem Record;
- eindeutige `instanceId != 0`;
- keine doppelte FaultIdentity;
- nur bekannte Flags;
- gueltige Primary-Metadaten;
- keine Selbstreferenz;
- `faultRevision != 0`;
- `nextInstanceId != 0`;
- `bootSequence != 0`;
- bekannte SAFE_BOOT-/Restart-Enums;
- `abnormalRestartCount <= 3`.

Unbekannte Enumwerte, falsche Laenge, Restbytes oder semantischer Widerspruch sind kein Fallback auf Defaults.

## 9.4 Commit des normalen Safetyrecords

Mutation:

1. vollstaendigen Kandidaten in RAM bauen;
2. alle Invarianten pruefen;
3. neue `SafetyStateRecordRevision` checked bestimmen;
4. anderen/geeigneten Slot waehlen;
5. Envelope kodieren;
6. schreiben;
7. **immer** zuruecklesen;
8. Envelope + Payload + exakte Kandidatensemantik verifizieren;
9. erst danach neuen Safetyzustand als persistent bestaetigt uebernehmen.

`CommitOutcomeUnknown`:

- exakter readback des neuen Kandidaten -> Commit bestaetigt;
- alter/anderer eindeutiger Wert -> Mutation nicht bestaetigt;
- Readfehler/uneindeutiger Zustand -> kritisch indeterminiert.

Jeder fehlgeschlagene Safety-State-Commit fuehrt fail-closed in den Emergency-Marker-Pfad.

---

# 10. Separater SafetyEmergencyMarker

## 10.1 Zweck

Der Marker ist ausschliesslich der minimale Fallback, wenn der normale Safetyrecord nicht mehr verlaesslich fortgeschrieben werden kann.

Slots:

```text
sem0
sem1
```

RecordType:

```text
10
```

Schema / SafetyStorageEpoch:

```text
1 / 1
```

Envelope-`versionValue` ist die MarkerSequence.

## 10.2 Payload

| Feld | Wire |
|---|---:|
| MarkerState (`Active`/`Cleared`) | u8 |
| MarkerReason | u8 |
| bootSequence | u32 |
| monotonicMillis | u64 |
| attemptedSafetyRecordRevision | u64 |

Payload: **22 Byte**.

Envelope ohne UTC:

```text
22 + 37 = 59 Byte
```

Damit passt der gesamte Marker in das 64-Byte-Limit.

## 10.3 MarkerReason

Mindestens:

```text
SafetyStateWriteFailed
SafetyStateCapacityFailure
SafetyStateCommitIndeterminate
SafetyStateReadbackMismatch
SafetyStateCorruptOrUnreadable
SafetyCounterExhausted
```

## 10.4 Verhalten

Wenn ein kritischer Safety-State-Commit scheitert:

1. RAM-seitiger `SafetyPersistenceUncertain`-Latch sofort setzen;
2. Aktorgate `ImmediateStop`;
3. genau einen Marker-Commitversuch ueber den redundanten Markerpfad durchfuehren;
4. Marker readback-verifizieren;
5. scheitert auch dies:
   - keine weiteren Schreibloops;
   - RAM-Latch bleibt bis Reset;
   - kein normaler Betrieb.

## 10.5 Bootprioritaet

Emergency-Marker werden **vor** normaler Aktorfreigabe ausgewertet.

- aktiver gueltiger Marker -> SAFE_BOOT;
- Marker-Slot ReadError/Capacity/Corruption -> SAFE_BOOT;
- beide Marker `NotFound` -> kein Marker;
- hoechste gueltige MarkerSequence `Cleared` und keine Marker-Slot-Probleme -> Marker ist geloest.

Ein `Cleared`-Marker ist ein persistierter Zustand, kein Delete.

---

# 11. Marker-Recovery ist nicht SAFE_BOOT-Exit

Geschuetzte Marker-Recovery:

1. Safety-State-Slots technisch lesen;
2. semantisch gueltigen Safetyzustand herstellen;
3. Write/Readback-Integritaet erfolgreich pruefen;
4. neuen kohärenten SafetyStateRecord persistieren;
5. Emergency Marker als `Cleared` persistieren und readbacken;
6. `storageIntegrityQualified = true` in der aktuellen RAM-Sitzung setzen.

Sie darf **nicht**:

- `safeBootRequired` loeschen;
- S3/Y4-Faults resetten;
- RestartEpisode loeschen;
- Aktorgate auf `Allowed` setzen.

SAFE_BOOT bleibt bestehen, bis der separate Exitvertrag erfuellt ist.

---

# 12. Reset-/Restart-Port

## 12.1 Neuer neutraler device_platform-Port

Nur weil auf `main` kein entsprechender Port existiert, wird ein kleiner anwendungsneutraler Port eingefuehrt.

Beispielvertrag:

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
    virtual ~IResetController() = default;
    [[nodiscard]] virtual ResetCause resetCause() const = 0;
    [[nodiscard]] virtual RestartRequestStatus requestSoftwareRestart() = 0;
};
```

Keine Faultcodes, SAFE_BOOT-Begriffe oder Fermentationsbegriffe in `device_platform`.

`device_platform_test_support` erhaelt einen deterministischen Simulator.

Kein ESP-IDF-Adapter in Issue #24.

## 12.2 Write-before-apply

Ein kontrollierter Restart darf nur angefordert werden, wenn vorher im normalen Safetyrecord atomar persistiert und readbackbestaetigt wurden:

- `restartAttempted=true` an der ausloesenden Faultinstanz;
- `restartIntentState=Prepared`;
- `restartIntentFaultInstanceId`.

Erst danach:

```text
requestSoftwareRestart()
```

Rueckkehr von `Requested` bedeutet nur „Anforderung akzeptiert“, nicht „Reboot bewiesen“.

Der naechste Boot und dessen `ResetCause` sind der einzige Nachweis fuer die tatsaechliche Restartfolge.

## 12.3 Keine Restartschleife

Dasselbe `instanceId` darf nie einen zweiten automatischen Restart ausloesen.

Ein abgelehnter oder nicht erfolgter Restart fuehrt nicht zu wiederholten Tick-basierten Restartversuchen.

---

# 13. RestartEpisode und SAFE_BOOT

## 13.1 Abnormale Ursachen

Abnormal zaehlen:

- `Brownout`;
- `WatchdogOrPanic`;
- `Unknown`;
- unerwarteter `SoftwareRestart`;
- erwarteter kontrollierter Software-Restart nach SafetyFault.

Nicht abnormal:

- `PowerOn` ohne offenen RestartIntent;
- `External` ohne offenen RestartIntent.

Ein normaler PowerCycle setzt einen vorhandenen Episodezaehler nicht zurueck.

## 13.2 Bootauswertung

Bootreihenfolge:

```text
physische/simulierte Outputs bleiben AUS
-> Emergency Marker laden
-> SafetyStateRecord laden und validieren
-> ResetCause erfassen
-> offenen RestartIntent genau einmal auswerten
-> abnormalRestartCount aktualisieren
-> persistierte S3/Y4-Faults pruefen
-> Configuration #57/#56 qualifizieren
-> Run #17/#18 qualifizieren
-> Sensoren #20/#21 qualifizieren
-> SAFE_BOOT-Entscheidung
-> Boottransition der bestehenden ProcessStateMachine
```

SAFE_BOOT wird verlangt, wenn mindestens:

- Emergency Marker aktiv/unklar;
- SafetyStateRecord korrupt/unlesbar/semantisch ungueltig;
- persistierter S3/Y4-Latch beim Boot vorhanden;
- `abnormalRestartCount >= 3`;
- Y4-Configurationzustand;
- aktiver Lauf nicht sicher rekonstruierbar;
- sicherheitsrelevante Bootevidenz unaufgeloest.

## 13.3 Stable-Window-Clear

Nur in normalem, nicht SAFE_BOOT Betrieb:

```text
30 min aktuelle monotone stabile Laufzeit
+ kein aktiver S3/Y4
+ kein RestartIntent
+ Safety-Persistenz healthy
=> abnormalRestartCount auf 0 persistieren
```

Das geschieht genau einmal beim Erreichen der Grenze, nicht pro Tick.

---

# 14. SAFE_BOOT-Exit

SAFE_BOOT verlaesst sich niemals durch:

- Reboot;
- Marker-Recovery allein;
- Quittierung;
- einen einzelnen erfolgreichen Write;
- blosses Verschwinden einer Messwertwarnung.

## 14.1 Voraussetzungen

`requestSafeBootExit()` darf nur positiv bewertet werden, wenn:

1. `safeBootRequired == true`;
2. Emergency Marker eindeutig `Cleared`;
3. Safety-State Write/Readback in diesem Boot qualifiziert;
4. kein persistenter S3/Y4-Latch mehr aktiv;
5. Configuration #56/#57 aktuell operational/qualified;
6. Run-Persistenz nicht indeterminiert;
7. AirSensor aktuell `VALID`;
8. CoolingSensor aktuell `VALID`;
9. Sensor-/CrossRole-Evidenz nicht unresolved;
10. ActuatorPlanner besitzt keinen aktiven Watchdog-Latch;
11. kein aktueller Y4-Unknown-Zustand;
12. notwendige Serviceautorisierung wurde von der aufrufenden Schicht bestaetigt.

ProductSensor ist fuer den Exit nach `Standby` nicht generell Pflicht.

## 14.2 Commitreihenfolge

1. Exitbedingungen rein pruefen;
2. SafetyState-Kandidat mit `safeBootRequired=false` persistieren/readbacken;
3. erst nach bestaetigtem Commit:
   - `ProcessEvent::SafeBootExitCompleted` anwenden;
4. ProcessState muss `SafeBoot -> Standby` werden;
5. neue Aktorfreigabe entsteht erst aus der naechsten normalen zentralen Gateauswertung.

Wenn Prozesspersistenz/Transition fehlschlaegt, bleibt das System fail-closed; es entsteht kein aktiver Lauf.

---

# 15. Erweiterung der bestehenden ProcessStateMachine

Issue #24 fuegt nur die zwei fehlenden Safety-Rueckwege hinzu:

```text
ProcessEvent::FaultResetCompleted
ProcessEvent::SafeBootExitCompleted
```

sowie passende `TransitionReason`s.

## 15.1 FaultResetCompleted

Aus:

```text
Fault
```

nach:

```text
RecoveryEvaluation
```

wenn ein aktiver Laufkontext vorhanden und rekonstruierbar ist.

Sonst nach:

```text
Standby
```

Nie direkt:

```text
Fault -> Heating/Cooling/Fermenting/...
```

Die Wiederaufnahme eines Laufes bleibt ausschliesslich der vorhandene #18-Recoverypfad.

## 15.2 SafeBootExitCompleted

Nur:

```text
SafeBoot -> Standby
```

Kein direkter Restore eines alten aktiven Laufes aus SAFE_BOOT.

---

# 16. Reale Producer-Mappings

Mappings bleiben klein, one-way und verwenden vorhandene Typen.

## 16.1 SensorQuality #20

### Air/Cooling STALE

```text
SensorQuality::Stale
=> O2-002 fuer die konkrete Source
=> ImmediateStop
```

Bei stabiler Rueckkehr zu `VALID`:

```text
O2-002 auto clear
```

### Air/Cooling FAILED

```text
SensorQuality::Failed
=> O2-002 beenden
=> S3-001 raisen
```

Nach durch #20 bereits validierter stabiler Rueckkehr zu `VALID`:

```text
S3-001 causeCleared=true
```

Latch bleibt bis Reset.

#24 implementiert keine zweite CRC-, Filter-, Alter- oder Recoveryzaehlung.

## 16.2 ProductSensor / #21

`O2-001` bildet den produktfuehlerbezogenen Ersatzbetriebsfall ab.

Reale Inputs sind die bestehenden #21-Phasen und Permission.

Wichtig:

- `AirFallbackActive + permission Allowed` darf trotz aktivem O2-001 normal luftgefuehrt regeln, sofern Air/Cooling und alle anderen Gates gueltig sind;
- O2-001 selbst ist deshalb kein pauschaler ImmediateStop;
- `ProductFailureDetected`, `UserDecisionRequired`, `ReturnValidationPending` mit Blocked-Permission verhindern Freigabe ueber die bestehende #21-Permission;
- `CrossRoleEvidenceIndeterminate` bzw. sicherheitsrelevantes `SafeLocked` wird S3-002;
- `InvalidContext`/unbekannte sicherheitsrelevante Evidenz wird Y4-006/SensorSet.

Kein zweiter Sensorselektionsautomat.

## 16.3 TemperatureControl #22

Normale Zustaende:

```text
NeutralBand
Saturated
AirLimitReduced
AirLimitBlocked
```

erzeugen fuer sich keinen SafetyFault.

Fail-closed Mappings:

- `InvalidConfiguration` -> Y4-006 / Control;
- `TimeInvalid` -> Y4-006 / Control;
- `RequestIdentityExhausted` -> Y4-006 / Control;
- `SensorUnavailable` / `InvalidSample` -> Aktorfreigabe unresolved; eigentliche Sensorursache kommt aus #20/#21;
- `NoCommissioning` -> unresolved, aber kein erfundener Fault.

## 16.4 ActuatorPlanner #23

`ActuatorPlanner::state().latchedWatchdogFault` bzw. reales Watchdog-Ergebnis:

```text
=> S3-004 / ActuatorPlanner
```

Der Planner hat die konkrete Tick-Reaktion bereits fail-closed ausgefuehrt.

#24 persistiert den systemweiten Latch.

### Cause-Clear fuer S3-004

Cause-Clear wird erst akzeptiert, wenn nach dem Watchdogereignis neue, aktuelle Anwendungsevidenz vorliegt:

- entweder die aktuelle Phase benoetigt keine Temperaturregelung mehr;
- oder eine neue strukturell gueltige #22-Auswertung fuer den aktuellen ControlContext wurde erzeugt;
- Planneroutput ist physisch abstrakt `Idle`/sicher gestoppt.

Es wird **kein** physisches Sink-Acknowledgement behauptet.

Nach erfolgreichem systemweiten Faultreset ruft die Application einmal:

```text
ActuatorPlanner::applyExternalWatchdogFaultReset(now)
```

auf.

## 16.5 Physische Aktordiagnose

S3-005 besitzt einen kleinen typisierten Vertrag fuer:

```text
Peltier
OuterFan
InnerFan
```

Zustaende mindestens:

```text
Healthy
Fault
Unknown
```

In Issue #24 gibt es dafuer nur deterministische Injection-/Contracttests.

Kein Produktionscode behauptet, dass das aktuelle Board diese Information bereits messen kann.

## 16.6 RunPersistence #17/#18

Bootmapping von `RunPersistenceLoadStatus`:

Normal/kein Y4:

```text
NoPersistedRun
Current
NoActiveRun
FallbackRecovered
PreparedInterrupted
```

soweit der bestehende #17/#18-Vertrag daraus einen sicheren Recoverypfad bereitstellt.

Y4-001:

```text
NotReconstructible
NotReconstructibleOrphanedState
```

Y4-004 oder Y4-006 je technischer Ursache:

```text
ReadFailed
CapacityExceeded
UnsupportedSchema
ForeignEpoch
```

Die genaue Zuordnung folgt der bereits vorhandenen typisierten Bedeutung; #24 baut keine zweite Run-Recoverylogik.

Laufzeit-`RunPersistenceResult`:

```text
WriteFailed
CapacityExceeded
PersistenceIndeterminate
PersistenceCommittedApplyFailed
```

wird als kritische systemweite Persistenz-/Rekonstruktionsgefahr auf den zentralen Safety-Core abgebildet.

## 16.7 ConfigurationRecovery #57

Direkt konsumieren:

```text
ConfigurationSafetyProducer::ConfigurationUnavailable
ConfigurationSafetyProducer::ConfigurationIntegrityFailure
```

=> Y4-002 / ConfigurationRecovery.

`RuntimeReady`, `FactoryInitializationCompleted`, `FactoryResetCompleted` qualifizieren die Recovery-Seite neu, loeschen einen bestehenden Y4-Latch aber nicht automatisch. Sie setzen nur dessen Ursache `causeCleared`, sofern der aktuelle Graph wieder eindeutig gueltig ist.

## 16.8 ConfigurationService #56

Mindestens:

```text
RuntimeFailure
CommitIndeterminate
```

=> Y4-003 / ConfigurationRuntime.

`Operational` ist positive aktuelle Qualifikation.

Zwischenzustaende wie `CommitInProgress`, Reset/Recovery-Aufbau oder `NoRuntime` liefern keine normale Aktorfreigabe. Sie muessen nicht fuer jeden kurzen Zustand einen neuen persistenten Y4-Fault erzeugen; das Gate bleibt `Unresolved`, bis ein eindeutiger Endzustand vorliegt.

---

# 17. Unresolved-Semantik Y4-006

Y4-006 ist **nicht eine globale Instanz**.

Zulaessige feste Sources:

```text
Process
SensorSet
Control
ActuatorPlanner
RunPersistence
ConfigurationRuntime
ConfigurationRecovery
SafetyPersistence
RestartSupervisor
```

Damit koennen bis zu neun unabhaengige unresolved Ursachen gleichzeitig aktiv sein.

Beispiel:

```text
Y4-006 / ConfigurationRuntime
Y4-006 / SensorSet
Y4-006 / Process
```

sind drei getrennte Faultinstanzen.

Wenn spaeter die Sensorursache aufgeloest wird:

```text
nur Y4-006 / SensorSet -> causeCleared
```

ConfigurationRuntime und Process bleiben unangetastet.

Keine `last-origin-wins`-Semantik.

---

# 18. Aktor-Safety-Gate

## 18.1 Keine persistierte Freigabe

`Allowed` wird niemals persistiert.

Nach jedem Boot beginnt die aktuelle Releasequalifikation als unbekannt.

## 18.2 Ergebnis

`SafetyFaultService` erzeugt ausschliesslich:

```text
ActuatorSafetyGateStatus::ImmediateStop
ActuatorSafetyGateStatus::Unresolved
ActuatorSafetyGateStatus::Allowed
```

### ImmediateStop

Mindestens bei:

- aktivem/gelatchtem blockierenden S3/Y4;
- aktivem Emergency Marker;
- SAFE_BOOT;
- kritischem Safety-Persistenz-RAM-Latch;
- O2-002 Air/Cooling STALE;
- einer aktuell sicherheitsrelevant blockierenden #21-Permission.

### Unresolved

Mindestens wenn eine fuer den aktuellen Kontext erforderliche Evidenz fehlt oder unbekannt ist, aber noch kein stabil klassifizierter Fault gesetzt wurde.

### Allowed

Nur wenn alle fuer den aktuellen Kontext erforderlichen aktuellen Nachweise positiv sind.

Mindestens:

- Safetyrecord geladen und healthy;
- kein Emergency Marker;
- kein SAFE_BOOT;
- kein blockierender Fault;
- Configuration operational;
- Runzustand nicht indeterminiert;
- AirSensor VALID;
- CoolingSensor VALID;
- #21-Permission fuer den aktuellen Lauf erlaubt;
- bei Product-Mode die von #21 verlangte ProductSensor-Evidenz;
- ControlContext strukturell gueltig;
- Planner-Watchdog nicht gelatcht.

---

# 19. Kein Planner-/Sink-Bypass

Der aktuelle aktive Aktorpfad ist:

```text
TemperatureControlApplicationOrchestrator
  -> ActuatorPlanner
  -> ActuatorPlanSinkDriver
```

Der heute vorhandene Aufruf:

```cpp
tickActuatorPlan(..., ActuatorSafetyGateInput safetyGate)
```

wird fuer den planner-/driver-gebundenen Produktionspfad geaendert.

## 19.1 Verbindliche Form

Der Konstruktor, der `ActuatorPlanner` und `ActuatorPlanSinkDriver` bindet, muss zusaetzlich den zentralen `SafetyFaultService` binden.

`tickActuatorPlan()` nimmt **keinen frei vom Caller gesetzten SafetyGate-Wert** mehr entgegen.

Der Orchestrator:

1. fragt den zentralen Safety-Service;
2. baut daraus intern `ActuatorSafetyGateInput`;
3. ruft den Planner;
4. wendet den Planneroutput am bestehenden SinkDriver an;
5. meldet Planner-Watchdogereignisse wieder an den Safety-Service.

Damit kann ein produktiver Caller nicht `Allowed` erfinden.

## 19.2 Bestehender actorfreier Root

`main/app_main.cpp` und `src/main.cpp` treiben auf aktuellem `main` keinen realen ActuatorPlanner/Sink und der ESP-IDF-Root deklariert reale Aktoren als disabled.

Issue #24 baut dort deshalb **keinen Fake-StateStore oder Fake-Hardwareadapter** ein.

Ein statischer/Integrationstest muss aber beweisen:

> Sobald der aktive planner-/driver-gebundene Orchestrator verwendet wird, ist `SafetyFaultService` zwingende Konstruktorabhaengigkeit.

---

# 20. FaultReset und #15/#17 ohne zweite Safety-Autoritaet

## 20.1 Bestehenden Vertrag wiederverwenden

`FaultResetEvaluation` bleibt der #15-Vertrag.

Minimal notwendige Erweiterung:

```text
targetFaultInstanceId
```

in:

- `FaultResetRequest`;
- `FaultResetEvaluation` oder der zugehoerigen Decisionprojektion.

`CommandEnvelope.expectedFaultRevision` bleibt der globale Stalenessanker.

## 20.2 Aenderung in `decideFaultReset()`

#15 darf nach positiver Evaluation **nicht mehr**:

```text
faultRevision selbst erhoehen
criticalSafetyEventPending selbst loeschen
```

Stattdessen:

- Envelope/Staleness/Bestaetigung pruefen;
- #24-Evaluation pruefen;
- normalen CommandDecision-Kandidaten erzeugen;
- `FaultResetAuthorized` + Zielinstance transportieren;
- keine Safety-Mutation ausfuehren.

## 20.3 Commitreihenfolge

```text
SafetyFaultService.evaluateReset(target)
-> #15 decideFaultReset(...)
-> #17 persistCommand(...)
-> nur bei bestaetigtem #17-Erfolg:
     SafetyFaultService.resetFault(target)
-> SafetyStateRecord persist/readback
-> RunCommandState Safetyprojektion synchronisieren
-> falls S3-004:
     planner.applyExternalWatchdogFaultReset()
-> wenn keine blockierenden Latches mehr:
     FaultResetCompleted-ProcessTransition anfordern
```

## 20.4 Crashmatrix

### Crash vor #17-Commit

Kein Safetyreset. Latch bleibt.

### Crash nach #17-Commit, vor SafetyState-Commit

Latch bleibt persistent. Reboot bleibt safe. Neue Command-ID kann spaeter erneut versuchen.

### SafetyState-Commit schlaegt fehl

Emergency Marker / ImmediateStop. Latch wird nicht als erfolgreich resettiert behauptet.

### Crash nach SafetyState-Commit, vor ProcessTransition

Safetyfault ist resettiert, aber ProcessState bleibt `Fault`. Dadurch keine Aktorfreigabe. Nach Recovery kann die Transition erneut ausgefuehrt werden.

Diese Reihenfolge benoetigt keine verteilte Zwei-Phasen-Transaktion ueber Run- und Safetyrecord.

---

# 21. Fault-Acknowledgement

Quittierung bleibt getrennt vom Reset.

Bestehende #15-Meldungsquittierung bleibt der Benutzercommand.

Die Safetyintegration darf nach erfolgreicher Quittierung einer faultgebundenen RuntimeMessage den betreffenden Fault `acknowledged=true` setzen.

Dafuer wird die Meldungsprojektion bei Bedarf minimal um die konkrete `faultInstanceId` ergaenzt; `faultRevision` allein reicht bei mehreren gleichzeitigen Faults nicht.

Regeln:

- Ack kann keine Ursache clearen;
- Ack kann keinen Latch resetten;
- Ack kann `Allowed` nicht herstellen;
- Ack-Ereignis wird fuer #19 als typisiertes Event ausgegeben.

---

# 22. Authorization-Grenze

Issue #24 implementiert keine PIN-Pruefung.

Der Safety-Core definiert pro Code nur die benoetigte Stufe, z. B.:

```text
Operator
Service
```

Die aufrufende Schicht liefert fuer eine konkrete Reset-/SAFE_BOOT-Anfrage nur das Ergebnis der spaeteren vertrauenswuerdigen Authentisierung:

```text
authorizationSatisfied = true/false
```

Bis ein produktiver Service-PIN-/Auth-Producer existiert, darf der reale Produktpfad keine Servicefreigabe herstellen.

Tests duerfen beide Zustaende deterministisch injizieren.

Es werden keine langlebigen Capability-Tokens, Pointer-Tokens oder versteckten Authframeworks gebaut.

---

# 23. Fault-/Safety-Ereignisse und Grenze zu Issue #19

Issue #24 erzeugt kleine typisierte Ereignisse, z. B.:

```text
FaultRaised
FaultAcknowledged
FaultCauseCleared
FaultReset
SafeBootEntered
SafeBootExited
RestartPrepared
RestartObserved
SafetyPersistenceFailed
EmergencyMarkerSet
EmergencyMarkerCleared
```

Mindestens enthalten:

- EventKind;
- FaultCode/Source falls zutreffend;
- instanceId;
- optional Primary;
- aktuelle bootlokale monotone Zeit;
- faultRevision.

Issue #24 speichert daraus **kein Langzeitjournal**.

Die konkrete append-only-/Ringpufferpersistenz, Retention, Export und Bereinigung bleibt #19.

Tests pruefen, dass die richtigen Ereignisse entstehen und keine Safetyentscheidung von einem funktionierenden Journal abhaengt.

---

# 24. Fehlerinjektion

Keine zweite Test-only-Safetylogik.

## 24.1 Sensor

Ueber reale #20/#21-Typen:

- VALID;
- STALE;
- FAILED;
- CrossRole indeterminate;
- Recovery.

## 24.2 Aktor

- realer #23-Watchdog wird durch fehlende/stale Requestfolge ausgeloest;
- S3-005 wird ueber den kleinen typisierten ActuatorDiagnostic-Vertrag mit Peltier/OuterFan/InnerFan injiziert;
- keine physische Rueckmeldung wird behauptet.

## 24.3 Persistenz

`SimulatedPersistentStateStore` injiziert:

- WriteError;
- CapacityError;
- CommitOutcomeUnknown -> neuer Record;
- CommitOutcomeUnknown -> alter Record;
- ReadError beim Readback;
- CRC-korrupte Bytes;
- semantisch ungueltigen, CRC-korrekten Payload;
- Fehler auch im Emergency-Marker-Pfad.

## 24.4 Brownout / Restart

`SimulatedResetController` liefert:

- PowerOn;
- SoftwareRestart;
- WatchdogOrPanic;
- Brownout;
- External;
- Unknown.

Tests pruefen RestartEpisode und SAFE_BOOT deterministisch.

---

# 25. Testorakel

## 25.1 FaultCore

- jede Katalogidentitaet ist eindeutig;
- unbekannte Code-/Source-Kombination wird nie still akzeptiert;
- wiederholte Evidenz derselben Identitaet erzeugt keine neue Instanz;
- mehrere unterschiedliche Faults koexistieren;
- P1/O2/S3/Y4-Dominanz;
- Quittierung != Cause-Clear != Reset;
- Primary/Follow-up bleibt nachvollziehbar;
- Primaryreset loescht Follow-up nicht;
- Instance-/FaultRevision-Overflow fail-closed.

## 25.2 Spezieller Regressionstest gegen PR #107/#108

Gleichzeitig:

```text
Y4-006 / ConfigurationRuntime
Y4-006 / Process
Y4-006 / SensorSet
```

Dann Sensorursache aufloesen.

Erwartung:

```text
nur SensorSet causeCleared
ConfigurationRuntime aktiv
Process aktiv
Gate weiterhin ImmediateStop
```

Anschliessend jede Ursache separat.

## 25.3 Volatile Evidenz darf keine neue Faultinstanz erzeugen

Dieselbe aktive Ursache wird mit:

- 100 verschiedenen Sensorzeitpunkten;
- 100 ControlRequestSequences;
- 100 Planner-/RunRevisionen

erneut beobachtet.

Erwartung:

```text
immer dieselbe instanceId
aktive Anzahl unveraendert
```

## 25.4 Persistenzcodec

Golden-/Roundtriptests:

- leere SafetyState;
- alle 24 persistenten Katalogidentitaeten;
- Ack/CauseClear/RestartAttempted Flags;
- Primarymetadaten;
- safeBoot;
- RestartIntent;
- abnormalRestartCount;
- maximale Payload;
- unknown enum;
- unknown code/source;
- duplicate identity;
- duplicate instanceId;
- invalid Primary;
- falsche Anzahl;
- Restbytes;
- truncated;
- Envelope-CRC;
- falscher RecordType;
- falsches Schema;
- falsche SafetyEpoch.

Wiregroessen werden aus den Codecfunktionen getestet und nicht nur aus Kommentaren behauptet.

## 25.5 SafetyStateStore

- beide Slots NotFound -> initialer Blankrecord;
- ein gueltiger Slot;
- zwei gueltige Revisionen -> hoechste;
- gleiche Revision / anderer Payload -> SAFE_BOOT;
- ReadError nie NotFound;
- CRC-Fehler nie ignoriert;
- Success + exakter Readback;
- CommitOutcomeUnknown -> exakt neu;
- CommitOutcomeUnknown -> alt;
- ReadbackFailure;
- SafetyState-Fehler -> EmergencyMarker;
- EmergencyMarker-Fehler -> RAM-only Lock.

## 25.6 Emergency Marker

- Active ueberlebt Reboot;
- Cleared erst nach erfolgreicher Marker-Recovery;
- Marker-Recovery loescht SAFE_BOOT nicht;
- korruptes Marker-Slot -> SAFE_BOOT;
- ein einzelner erfolgreicher normaler Write loescht Marker nicht.

## 25.7 Restart

- PowerOn ohne Episode;
- einzelner Brownout;
- Watchdog;
- unerwarteter SoftwareRestart;
- kontrollierter Restart write-before-apply;
- derselbe Fault bekommt keinen zweiten Restart;
- 3 abnormale Boots -> SAFE_BOOT;
- 29:59 stabil -> Counter bleibt;
- 30:00 stabil -> Counter wird einmal persistiert auf 0;
- Reboot vor 30 Minuten -> Counter bleibt;
- persistierter S3/Y4 beim Boot -> SAFE_BOOT unabhaengig vom Counter.

## 25.8 SAFE_BOOT

- normaler Reboot verlaesst SAFE_BOOT nicht;
- Marker-Recovery allein verlaesst SAFE_BOOT nicht;
- fehlende Configqualifikation -> Exit abgelehnt;
- Air STALE/FAILED -> Exit abgelehnt;
- Cooling STALE/FAILED -> Exit abgelehnt;
- Watchdog-Latch -> Exit abgelehnt;
- Serviceautorisierung fehlt -> Exit abgelehnt;
- alle Bedingungen positiv -> SafetyState persistiert, danach `SafeBoot -> Standby`;
- kein direkter Aktortest aus SAFE_BOOT.

## 25.9 Reset / #15 / #17

- stale faultRevision;
- falsche targetInstance;
- cause active;
- Safetycheck fehlt;
- Authorization fehlt;
- anderer uncleared gleich-/hoeherer Fault;
- anderer cause-cleared Latch verhindert Zielreset nicht, aber Gate bleibt blockiert;
- #15 mutiert FaultCore nicht;
- #17 Commitfehler -> Safetyreset nicht ausgefuehrt;
- Crash nach #17 vor SafetyState -> Latch bleibt;
- SafetyState-Commitfehler -> EmergencyMarker;
- letzter Fault reset -> erst dann Process-Fault-Exit;
- aktiver Lauf -> `Fault -> RecoveryEvaluation`;
- kein aktiver Lauf -> `Fault -> Standby`.

## 25.10 Reale Integrationspfade

- #20 Air/Cooling STALE/FAILED/Recovery;
- #21 ProductFallback und SafeLocked;
- #22 InvalidConfiguration/TimeInvalid sowie normale AirLimitReduced/Blocked ohne falschen Fault;
- #23 realer Watchdog;
- #17/#18 NotReconstructible;
- #56 RuntimeFailure/CommitIndeterminate;
- #57 ConfigurationUnavailable/IntegrityFailure;
- alle Configuration-Gate-Faelle blockieren normale Aktorfreigabe.

## 25.11 Bypass

- planner-/driver-gebundener Orchestrator laesst sich nicht ohne SafetyFaultService konstruieren;
- `tickActuatorPlan()` akzeptiert keinen caller-supplied `Allowed`;
- Unresolved -> Planner Idle;
- ImmediateStop -> Planner stoppt sofort trotz Mindestzeiten;
- Sink kann keine Freigabe erzeugen;
- kein Testhelper ist Produktionsabhaengigkeit.

---

# 26. Dokumentationsaenderungen

Nur dauerhafte Verträge aktualisieren:

- `docs/SAFETY_AND_FAULTS.md`
  - finaler 15-Code-Katalog;
  - stabile Source-/Identity-Regel;
  - Resetfolge;
- `docs/SYSTEM_SAFETY_AND_RECOVERY.md`
  - 3 Restarts / 30 Minuten;
  - separater Emergency Marker;
  - Marker-Recovery != SAFE_BOOT-Exit;
- `docs/SAFETY_COMPONENT_FAULTS.md`
  - reale vs injection-only Actuatordiagnose klar kennzeichnen;
- `docs/ACCEPTANCE_TESTS.md`
  - dauerhafte Orakel aus diesem Plan, **keine PR-Historie**;
- `docs/RUN_COMMANDS.md`
  - targetFaultInstanceId und #15/#24-Autoritaetsgrenze;
- `docs/ROADMAP.md`
  - #23 als abgeschlossen;
  - #24 / PR #108 als aktuelle fachliche Arbeit;
  - #19 danach als naechste fachliche Arbeit.

PR-Runden, alte Heads und Testlaufhistorie bleiben im PR/Handover.

---

# 27. Voraussichtlich betroffene Produktionsdateien

Die genaue Dateiliste wird pro Slice gegen Live-HEAD geprueft. Erwartet:

## `lib/fermentation_app/src/`

Neu bzw. zentral:

```text
fault_types.hpp
fault_catalog.hpp/.cpp
safety_fault_service.hpp/.cpp
safety_state_codec.hpp/.cpp
safety_state_store.hpp/.cpp
safety_storage_contract.hpp
safety_events.hpp
restart_supervisor.hpp/.cpp
actuator_diagnostic_types.hpp
```

Gezielt angepasst:

```text
run_commands.hpp/.cpp
process_state_machine.hpp/.cpp
temperature_control_orchestrator.hpp/.cpp
```

## `lib/device_platform/src/`

Nur:

```text
reset_controller.hpp
```

falls kein gleichwertiger Port am jeweiligen Live-HEAD vorhanden ist.

## `lib/device_platform_test_support/src/`

Nur deterministische Simulatoren, insbesondere:

```text
simulated_reset_controller.hpp/.cpp
```

Bestehenden `SimulatedPersistentStateStore` erweitern statt zweiten Store bauen, falls seine vorhandenen Cut-Points ausreichen.

## Tests

Neue kleine Testbereiche entlang der Module, statt eines einzigen riesigen Issue-24-Testfiles.

---

# 28. Umsetzungsschnitte

Jeder Slice endet mit:

- gezielten Tests;
- `git diff --check`;
- Architekturgrenzen;
- Secretscan;
- kurzem Handover;
- Ownerreview.

Keine Full Native Suite in jedem Draft-Slice.

## Slice 1 – Faultmodell und pure Policy

Umfang:

- `FaultCode`, `FaultSource`, `FaultDetail`;
- compile-time Katalog;
- FaultIdentity;
- reine FaultCore-Lifecyclelogik;
- Primary/Follow-up;
- P1/O2/S3/Y4 Policy;
- keine Persistenz;
- keine Producerintegration.

Tests:

- Katalog;
- Identitaet;
- Dominanz;
- Multi-Fault;
- same identity / wechselnde Evidenz;
- Reset-/Ack-/CauseClear-Pure-Policy;
- Overflow.

**Stop zum Ownerreview.**

## Slice 2 – Codec + normaler SafetyStateStore

Umfang:

- Safety Storage Contract;
- RecordType 9;
- sf0/sf1;
- exaktes Wireformat;
- Slotscan;
- semantic decode;
- write/readback/CommitOutcomeUnknown.

Noch kein Emergency Marker.

Tests:

- Golden Bytes;
- Roundtrip;
- Corruption;
- Slotselection;
- Commitmatrix.

**Stop zum Ownerreview.**

## Slice 3 – Emergency Marker

Umfang:

- RecordType 10;
- sem0/sem1;
- 22-Byte-Payload;
- RAM fallback;
- Marker-Recovery;
- Marker darf SAFE_BOOT nicht clearen.

Tests komplette Cut-Point-Matrix.

**Stop zum Ownerreview.**

## Slice 4 – ResetSupervisor / SAFE_BOOT

Umfang:

- neutraler `IResetController`;
- Simulator;
- RestartIntent;
- 3-Restart-/30-Minuten-Episode;
- Bootauswertung;
- ProcessStateMachine:
  - `SafeBootExitCompleted`;
  - `FaultResetCompleted`;
- SAFE_BOOT-Exitcontract.

Kein ESP-IDF-Adapter.

**Stop zum Ownerreview.**

## Slice 5 – reale Producer #20/#21/#22/#23

Umfang:

- SensorQuality-Mapping;
- SensorSelection-Mapping;
- TemperatureControl-Mapping;
- Planner-Watchdog-Mapping;
- injection-only ActuatorDiagnostic;
- keine duplizierte Fachlogik.

Tests gegen echte existierende Typen.

**Stop zum Ownerreview.**

## Slice 6 – reale Producer #17/#18/#56/#57

Umfang:

- RunPersistence Load/Runtime;
- ConfigurationRuntime;
- ConfigurationRecovery;
- Configuration-Safety-Integration-Gate vollstaendig;
- keine zweite Persistenz-/Recoverymaschine.

Tests:

- alle Issue-#24-Configuration-Akzeptanzfaelle;
- Run not reconstructible;
- indeterminate writes.

**Stop zum Ownerreview.**

## Slice 7 – Command-/Resetintegration

Umfang:

- targetFaultInstanceId;
- bestehende FaultResetEvaluation wiederverwenden;
- #15 nicht mehr Safety mutieren lassen;
- #17-persist-before-Safety-reset;
- Ack-Verknuepfung;
- Fault -> RecoveryEvaluation/Standby.

Tests inklusive Crashmatrix.

**Stop zum Ownerreview.**

## Slice 8 – Aktorpfad / Bypass-Haertung

Umfang:

- aktiver Orchestrator bindet SafetyFaultService zwingend;
- caller kann kein `Allowed` mehr liefern;
- Planner/Sink-Handoff;
- S3-004 Reset-Handoff.

Tests:

- Unresolved;
- ImmediateStop;
- watchdog;
- Bypass;
- Timingregeln duerfen SafetyStop nicht verhindern.

**Stop zum Ownerreview.**

## Slice 9 – kanonische Dokumentation und Issue-DoD

Umfang:

- dauerhafte Safetydocs;
- ACCEPTANCE_TESTS;
- RUN_COMMANDS;
- ROADMAP;
- keine PR-Historie.

Danach vollstaendiges Ownerreview des gesamten PR-Diffs.

Erst wenn 0 offene Findings:

- Owner fordert den vollstaendigen Test-/Build-Gesamtlauf an;
- Native Full Suite;
- relevante Buildprofile;
- Ressourcenvergleich;
- Remote CI gemaess Repositoryregeln.

Hardwaretests bleiben `NOT_RUN`, sofern nicht separat freigegeben und verkabelt.

---

# 29. Aufgabenliste

Die Liste ist Navigationshilfe, keine starre Mikro-Commit-Vorschrift.

## Planung / Freigabe

- [x] Ersatzplan als vollstaendige neue Planfassung in PR #108 uebernommen
- [x] alte Plan-SHA nicht mehr als freigegeben bezeichnet
- [x] Roadmap auf realen #23/#24-Stand korrigiert
- [ ] exakte neue Plan-SHA in PR-Body und SESSION HANDOVER eingetragen
- [x] Implementation weiterhin `NOT_STARTED`
- [ ] Ownerfreigabe exakt dieser neuen Plan-SHA

## Slice 1

- [ ] stabiler FaultCode-/Source-Katalog
- [ ] pure FaultCore-Policy
- [ ] Multi-Ursachen-Semantik
- [ ] Primary/Follow-up
- [ ] Lifecycle-/Overflowtests
- [ ] Ownerreview

## Slice 2

- [ ] SafetyState Wireformat
- [ ] RecordType 9 / sf0/sf1
- [ ] semantic Decoder
- [ ] readback / CommitOutcomeUnknown
- [ ] Golden-/Corruptiontests
- [ ] Ownerreview

## Slice 3

- [ ] RecordType 10 / sem0/sem1
- [ ] Emergency Marker
- [ ] RAM-fail-closed
- [ ] Marker-Recovery != SAFE_BOOT-Exit
- [ ] Cut-Point-Tests
- [ ] Ownerreview

## Slice 4

- [ ] neutraler Reset-Port
- [ ] SimulatedResetController
- [ ] RestartIntent write-before-apply
- [ ] 3-Restart-/30-Minuten-Episode
- [ ] SAFE_BOOT Boot-/Exitlogik
- [ ] fehlende ProcessTransitions
- [ ] Ownerreview

## Slice 5

- [ ] #20 SensorQuality
- [ ] #21 SensorSelection
- [ ] #22 ControlResult
- [ ] #23 Watchdog
- [ ] ActuatorDiagnostic injection-only
- [ ] Ownerreview

## Slice 6

- [ ] #17/#18 RunPersistence/Recovery
- [ ] #56 ConfigurationRuntime
- [ ] #57 ConfigurationRecovery
- [ ] reales CONFIGURATION_SAFETY_INTEGRATION_GATE
- [ ] Ownerreview

## Slice 7

- [ ] FaultReset target instance
- [ ] #15 nur Commandautoritaet
- [ ] #17 persist-before-reset
- [ ] Crashmatrix
- [ ] Fault-Exit in RecoveryEvaluation/Standby
- [ ] Ownerreview

## Slice 8

- [ ] SafetyService zwingend im aktiven Orchestrator
- [ ] kein caller-supplied Allowed
- [ ] Planner-/Sink-Bypass negativ getestet
- [ ] Watchdog-Reset-Handoff
- [ ] Ownerreview

## Slice 9 / Abschluss

- [ ] kanonische Dokumentation
- [ ] Roadmap
- [ ] keine PR-Historie in Fachdocs
- [ ] vollstaendiges PR-Review
- [ ] 0 offene Findings
- [ ] Owner fordert Gesamtlauf an
- [ ] Gesamtlauf/Builds/CI dokumentiert
- [ ] Owner entscheidet Ready/Merge

---

# 30. Stop-Regeln waehrend der Umsetzung

Neue vollstaendige Planrevision und neue Owner-SHA-Freigabe ist erforderlich, wenn waehrend der Umsetzung eine materielle Aenderung notwendig wird an:

- FaultCode-/Source-Katalog;
- Persistenzschema;
- RecordType/Keys;
- Restartgrenze 3 / Stable Window 30 min;
- Reset-/SAFE_BOOT-Transitions;
- #15/#17 Commitreihenfolge;
- Aktor-Gate-Autoritaet;
- neuen Hardware-/ESP-IDF-Abhaengigkeiten;
- neuen realen Producerbehauptungen.

Keine neue Planrevision ist fuer rein lokale Implementierungsdetails notwendig, wenn sie den freigegebenen Vertrag nicht veraendern.

---

# 31. Definition of Done fuer Issue #24

Issue #24 ist erst abgeschlossen, wenn:

- vier Klassen und stabile Codes implementiert sind;
- gleichzeitig aktive unabhaengige Ursachen korrekt koexistieren;
- kein `last-origin-wins`;
- S3/Y4 rebootfest verriegelt sind;
- Quittierung, Cause-Clear und Reset getrennt sind;
- Reset zentral und fail-closed ist;
- Restart write-before-apply und einmal pro Faultinstanz arbeitet;
- 3 abnormale Restart-Boots vor 30 Minuten stabiler Laufzeit SAFE_BOOT ausloesen;
- SAFE_BOOT rebootfest und actorfrei ist;
- Emergency Marker getrennt und getestet ist;
- Marker-Recovery SAFE_BOOT nicht umgeht;
- #17/#18/#20/#21/#22/#23/#56/#57 real konsumiert werden;
- alle vier Configuration-Safety-Gate-Faelle aus Issue #24 getestet sind;
- Planner-/Sinkpfad keinen caller-supplied Safety-Bypass besitzt;
- Sensor-, Aktor-, Persistenz- und Brownout/Restart-Injektion reproduzierbar ist;
- Fault-/Reset-/SAFE_BOOT-Events fuer #19 typisiert bereitstehen;
- kanonische Dokumentation konsistent ist;
- alle gezielten und final geforderten Tests/Gates bestanden sind;
- offene Hardware-/Commissioningtests ehrlich `NOT_RUN/BLOCKED` bleiben;
- kein ESP-IDF-/Hardwarewert erfunden wurde.

---

# 32. Plan-Gate

Nach Uebernahme dieser vollstaendigen Planfassung in PR #108:

1. nur Plan/Roadmap/PR-Body/Handover committen;
2. keine Produktions- oder Testimplementation im selben Commit;
3. exakte neue Plan-SHA veroeffentlichen;
4. `git diff --check`, Architekturgrenzen und Secretscan ausfuehren;
5. Tests/Builds fuer die reine Planrunde als `NOT_RUN` dokumentieren;
6. anhalten.

**Keine Implementierung, bis der Owner exakt die neue Plan-Commit-SHA freigegeben hat.**
