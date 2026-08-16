# Issue #24 – Safety-Core, Verriegelung, Restart und SAFE_BOOT

Status: **PLANREVISION – NOCH NICHT ZUR UMSETZUNG FREIGEGEBEN**

Issue: #24 – `[E3.5] Fehlerklassen, Verriegelung, SAFE_BOOT und Fehlerinjektion`

Branch: `agent/issue-24-safety-core-clean-restart`

Draft-PR: #108

Live-Basis bei Erstellung dieser vollstaendigen Planfassung:

```text
main @ b8eae5f4da5f2666b5a9bda333d115254c4db5b2
zu ersetzende Plan-SHA:
f3b034bf7d2a28abf5609b41eff749994eb7696e
```

Diese Datei ersetzt **alle frueheren Issue-#24-Planfassungen vollstaendig**.
PR #107 und fruehere Plan-SHAs sind nur historische Lernreferenzen und keine
normative Implementierungsquelle.

---

# 1. Ziel

Issue #24 implementiert den Release-1-Safety-Core als zentrale Autoritaet fuer:

- vier Fehlerklassen P1/O2/S3/Y4;
- stabile maschinenlesbare FaultCodes;
- voneinander unabhaengige gleichzeitig aktive Ursachen;
- unmittelbare sichere Aktorreaktionen;
- automatische Wiederfreigabe nur fuer explizit erlaubte P1/O2-Faelle;
- persistente S3/Y4-Verriegelungen;
- Trennung von Quittierung, Cause-Clear und Reset;
- Primaer-/Follow-up-Beziehungen;
- einmaligen kontrollierten Restart fuer explizit restartfaehige Faults;
- Restart-Episode und `SAFE_BOOT`;
- redundante Safety-Persistenz;
- getrennten minimalen EmergencyMarker;
- reale Integration der vorhandenen #20/#21/#22/#23/#56/#57-Producer;
- Run-Persistenz-/Recovery-Integration gegen #17/#18;
- deterministische Fault-Injektion;
- typisierte SafetyEvents als spaeteren Input fuer #19;
- ein zentrales Aktor-Safety-Gate ohne caller-supplied Freigabe.

Fail-closed ist die Grundregel.

---

# 2. Verbindliche Quellen und vorhandene Vertraege

Vor Umsetzung und Review sind mindestens zu verwenden:

| Bereich | bestehende Quelle |
|---|---|
| Prozesszustand | #14 / `process_state_machine.*`, `STATE_MACHINE.md` |
| Commands | #15 / `run_commands.*`, `RUN_COMMANDS.md` |
| Run-Persistenz | #17 / `RunPersistenceCoordinator`, `RUN_PERSISTENCE.md` |
| Recovery | #18 / `RunRecoveryCoordinator`, `RECOVERY_AND_INTERRUPTION.md` |
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
- bestehende Run-/Config-/Sensor-/Control-/Planner-Vertraege.

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
- eine neue allgemeine Recovery-State-Machine;
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
gemessen.

Keine monotone Zeit wird ueber Reboots hinweg subtrahiert.

## 5.2 Y4-005-Requalifikation

Bei `abnormalRestartCount == 3` bleibt Y4-005 gelatcht.

Nach 30 Minuten stabiler Uptime **auch innerhalb SAFE_BOOT** darf ausschliesslich
dessen Ursache:

```text
causeCleared = true
```

werden.

Der Counter bleibt 3 und der Latch bleibt aktiv.

Erst ein erfolgreicher geschuetzter Reset von Y4-005:

```text
abnormalRestartCount = 0
Fault entfernen
```

Danach bleibt `SAFE_BOOT` bestehen, bis der separate SAFE_BOOT-Exit erfolgreich
ist.

## 5.3 FAULT ist fuer S3/Y4 terminal fuer den Release-1-Lauf

P1/O2 duerfen nur gemaess ihrem Katalogeintrag automatisch rearmen.

Ein S3/Y4-Fault fuehrt den Prozess in `FAULT`.

Nach dem letzten erfolgreichen Faultreset:

```text
FAULT -> STANDBY
```

Der aktive Lauf wird terminal ueber den bestehenden
`clearActiveRunState()`-Vertrag entfernt.

Es gibt **kein**:

```text
FAULT -> RECOVERY_EVALUATION
```

in Issue #24.

Damit wird kein zweiter #18-Recoveryvertrag aufgebaut.

## 5.4 SAFE_BOOT beendet einen noch nicht reaktivierten Lauf

Wenn beim Boot `SAFE_BOOT` erforderlich ist:

- ein geladener aktiver Run wird nicht ueber #18 reaktiviert;
- sobald der Run-Store eindeutig les-/schreibbar ist, wird er ueber einen
  kleinen terminalen #17-Pfad als `NoActiveRun/STANDBY` persistiert;
- ist der Run-Store indeterminiert, bleibt das Geraet in SAFE_BOOT und der
  Exit bleibt gesperrt.

Damit ist `SAFE_BOOT -> STANDBY` eindeutig und kann keinen alten Run
automatisch wieder aufnehmen.

## 5.5 SafetyStorageEpoch

Schema 1 verwendet:

```text
SafetyStorageEpoch = 1
```

unabhaengig von der Configuration-`StorageEpoch`.

Ein Configuration-Werksreset darf Safety-Latches nicht implizit unlesbar
machen oder loeschen.

Eine spaetere explizite Safety-Epoch-Aenderung benoetigt eigenen Scope.

---

# 6. Architektur und Verantwortungen

## 6.1 FaultCore – reine Policy

`FaultCore` ist:

- hardwarefrei;
- persistenzfrei;
- deterministisch;
- nativ testbar;
- ohne dynamische Allokation.

Er besitzt:

- aktiven Faultzustand;
- FaultIdentity;
- InstanceIds;
- `faultRevision`;
- Cause-Clear;
- Primary-/Follow-up-Bezug;
- restartAttempted;
- Hauptfaultauswahl;
- Policyaggregation.

Er schreibt keinen Store und ruft keine Hardware.

## 6.2 SafetyStateStore – nur persistenter Safetyrecord

Verantwortung:

- Keys;
- Envelope;
- Codec;
- Slot-Scan;
- Readback;
- CommitOutcomeUnknown;
- Redundanzpruefung;
- geschuetzte Reparatur.

Keine Faultklassifikation.

## 6.3 SafetyEmergencyMarkerStore

Getrennter kleiner Storepfad fuer:

- normalen SafetyState-Commitfehler;
- unbestimmten Safety-State;
- Counter-/Sequence-Exhaustion.

Keine normale Faultliste.

## 6.4 SafetyFaultService – eine mutable Safety-Autoritaet

`SafetyFaultService` besitzt:

- `FaultCore`;
- geladenen/persistierten SafetyState;
- RestartEpisode;
- RestartIntent;
- `safeBootRequired`;
- Storagequalification;
- Resetbewertung;
- Resetcommit;
- SafetyMutationResults.

Es ist die **einzige** mutable Fault-/Latch-/Reset-Autoritaet.

## 6.5 SafetyProcessCoordinator – nur Cross-Domain-Sequenzierung

Ein kleiner `SafetyProcessCoordinator` koordiniert:

- Bootreihenfolge;
- Run-Persistenzstatus;
- Runtime-`FAULT`-Eintritt;
- terminalen `FAULT -> STANDBY`-Pfad;
- SAFE_BOOT-Eintritt;
- terminales Run-Verwerfen bei SAFE_BOOT;
- SAFE_BOOT-Exit;
- #15-FaultReset-Handoff.

Er besitzt **keinen zweiten Faultzustand**.

Er ruft vorhandene #17-/#18-/StateMachine-Vertraege auf.

## 6.6 Aktorpfad

Der produktive abstrakte Pfad lautet:

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
P1 = Hinweis / Warnung
O2 = behebbarer Betriebsfehler
S3 = verriegelter Sicherheitsfehler
Y4 = schwerer Systemfehler
```

Dominanz:

```text
Y4 > S3 > O2 > P1
```

Die hoechste aktive Klasse bestimmt die Safetywirkung.

Alle aktiven Ursachen bleiben erhalten.

---

# 8. FaultIdentity

Exakt:

```cpp
struct FaultIdentity {
    FaultCode code;
    FaultSource source;
};
```

Nicht Bestandteil:

- RunRevision;
- FaultRevision;
- ControlRequestSequence;
- Sensorsequence;
- Plannersequence;
- Bootzeit;
- Config-StateRevision;
- Persistenzrevision;
- Zeitstempel.

Ein neues Messergebnis derselben Ursache erzeugt **keine** neue Instanz.

---

# 9. FaultSource

Release 1:

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

Bedeutung:

| Code | Bedeutung |
|---|---|
| P1-001 | Prozesswarnung |
| O2-001 | Produktfuehler degradiert / validierter Ersatzbetrieb |
| O2-002 | erforderlicher fixer Sensor voruebergehend STALE |
| S3-001 | erforderlicher fixer Sensor FAILED |
| S3-002 | Sensor-/Rollenplausibilitaet sicherheitsrelevant unaufgeloest |
| S3-003 | thermische Safetygrenze |
| S3-004 | Aktoranforderungs-Watchdog |
| S3-005 | elektrischer/Ausgangsfehler eines Aktors |
| S3-006 | funktionaler Fanfehler |
| Y4-001 | Run nicht sicher rekonstruierbar |
| Y4-002 | Configuration Recovery unavailable/integrity failure |
| Y4-003 | Configuration RuntimeFailure / CommitIndeterminate |
| Y4-004 | kritischer Persistenzfehler |
| Y4-005 | Restartloop eskaliert |
| Y4-006 | sicherheitsrelevante Evidenz unbekannt/unaufgeloest |
| Y4-007 | interne Safety-Invariante nicht beweisbar |

---

# 11. Katalog – erlaubte Identitaeten und Policy

## 11.1 Transient

```text
P1-001 / Process

O2-001 / ProductSensor

O2-002 / AirSensor
O2-002 / CoolingSensor
```

4 Identitaeten.

## 11.2 Persistent

```text
S3-001 / AirSensor
S3-001 / CoolingSensor
S3-002 / SensorSet
S3-003 / TemperatureSafety
S3-004 / ActuatorPlanner

S3-005 / Peltier
S3-005 / OuterFan
S3-005 / InnerFan

S3-006 / OuterFan
S3-006 / InnerFan

Y4-001 / RunPersistence
Y4-002 / ConfigurationRecovery
Y4-003 / ConfigurationRuntime

Y4-004 / RunPersistence
Y4-004 / SafetyPersistence

Y4-005 / RestartSupervisor

Y4-006 / Process
Y4-006 / SensorSet
Y4-006 / Control
Y4-006 / ActuatorPlanner
Y4-006 / RunPersistence
Y4-006 / ConfigurationRuntime
Y4-006 / ConfigurationRecovery
Y4-006 / SafetyPersistence
Y4-006 / RestartSupervisor

Y4-007 / SafetyCore
```

26 persistente Identitaeten.

Gesamt:

```text
30
```

Compile-time-fester Katalog.

Keine Eviction.
Keine Laufzeit-Erweiterung.
Keine Correlation-Slots.

---

# 12. Displayprioritaet innerhalb einer Klasse

Jeder `FaultCatalogEntry` besitzt:

```text
displayPriority
```

kleiner = prominenter.

## Y4

```text
Y4-007  10
Y4-004  20
Y4-005  30
Y4-003  40
Y4-002  50
Y4-001  60
Y4-006  70
```

## S3

```text
S3-005  10
S3-003  20
S3-001  30
S3-002  40
S3-004  50
S3-006  60
```

## O2

```text
O2-002 10
O2-001 20
```

P1 besitzt nur P1-001.

Tiebreak bei gleichem Code:

```text
numerischer FaultSource-Wert
```

Diese Reihenfolge beeinflusst nur Hauptmeldung/Anzeige, nie Safe-State oder
Reset.

---

# 13. Fault-Lifecycle

## 13.1 Runtime FaultRecord

```text
identity
instanceId
causeCleared
firstSeenBootSequence
firstSeenMonotonicMillis
optional primary reference
restartAttempted
```

Kein generisches persistentes `detail`.

Acknowledgement ist nicht Teil des persistenten Safetyrecords.

## 13.2 Neue Ursache

1. Katalog pruefen.
2. Keine aktive identische Instanz vorhanden.
3. `nextInstanceId` checked.
4. `faultRevision` checked.
5. neue Instanz.
6. RAM-Gate sofort sicher.
7. S3/Y4 SafetyState persistieren.
8. bei Commitfehler EmergencyMarker.

## 13.3 Gleiche Ursache erneut aktiv

Wenn bereits:

```text
causeCleared == false
```

bleiben:

- instanceId;
- faultRevision;
- persistierter Record

unveraendert.

Kein Write pro Tick.

## 13.4 Relapse

Wenn dieselbe aktive S3/Y4-Instanz bereits:

```text
causeCleared == true
```

hat und der Producer wieder Active meldet:

```text
causeCleared = false
faultRevision++
persistieren/readback
```

Eine alte ResetEvaluation ist dadurch stale.

## 13.5 Cause-Clear

Nur wenn der **gesamte kanonische Producerzustand dieser FaultIdentity**
eindeutig wieder qualifiziert ist.

Dann:

```text
causeCleared = true
faultRevision++
```

S3/Y4 persistieren.

Ein einzelner behobener Untergrund darf eine noch aktive zweite Unterursache
derselben Identity nicht clearen.

## 13.6 P1/O2 Auto-Rearm

Nur Katalogeintraege mit `autoRearm=true`.

Beim qualifizierten Clear:

- Instanz entfernen;
- `faultRevision++`;
- keine SafetyState-Persistenz, weil P1/O2 nicht rebootgelatcht sind.

## 13.7 Reset

Reset entfernt genau eine S3/Y4-Instanz.

Vor Commit erneut pruefen:

- target instance existiert;
- target gehoert zur erwarteten FaultIdentity;
- erwartete `faultRevision` stimmt;
- `causeCleared == true`;
- codebezogene Safetychecks positiv;
- Authorization positiv;
- keine andere **uncleared** gleich-/hoeherklassige Ursache blockiert;
- SafetyStorage healthy.

Andere bereits `causeCleared` Latches duerfen nacheinander resettiert werden.

Reset:

```text
faultRevision++
target entfernen
```

persistieren/readback.

## 13.8 Acknowledgement

Quittierung:

- ist Message-/UI-Zustand;
- aendert kein CauseClear;
- aendert keinen Latch;
- aendert `faultRevision` nicht;
- ist nicht rebootkritisch;
- darf nach Reboot wieder als nicht quittiert erscheinen.

---

# 14. Primary-/Follow-up

Persistiert:

```text
primaryInstanceId
primaryCode
primarySource
```

`0` bedeutet kein Primary.

Regeln:

- keine Selbstreferenz;
- Primary muss beim Erzeugen existieren;
- Code/Source muessen Katalogwert sein;
- spaeterer Primaryreset loescht Follow-up nicht;
- Follow-up benoetigt eigenes Clear/Reset;
- Decoder darf eine historische PrimaryInstanceId akzeptieren, deren aktive
  Instanz inzwischen nicht mehr im Record liegt, sofern Code/Source gueltig
  und nicht Selbstreferenz sind.

#19 uebernimmt spaeter die Langzeithistorie.

---

# 15. FaultRevision und Counter

## 15.1 Initialwert

```text
faultRevision = 1
nextInstanceId = 1
bootSequence = 1
```

0 ist ungueltig/reserviert.

## 15.2 FaultRevision-Ueberlauf

Kann die naechste resetrelevante Revision nicht gebildet werden:

- RAM `SafetyCoreExhausted`;
- Gate ImmediateStop;
- EmergencyMarker `SafetyCounterExhausted`;
- kein weiterer normaler Safetycommit.

## 15.3 InstanceId-Allokation und Ueberlauf

`nextInstanceId` bedeutet immer **naechste noch nicht verwendete ID**.

Bei neuer Instanz:

1. `nextInstanceId` muss nonzero und kleiner als `UINT32_MAX` sein;
2. dieser Wert wird als `instanceId` verwendet;
3. der persistierte Kandidat setzt `nextInstanceId = alt + 1`.

`UINT32_MAX` wird bewusst nicht mehr als neue ID ausgegeben, weil danach kein
eindeutiger naechster Wert mehr persistierbar waere.

Fehlt Headroom:

- keine neue Instanz;
- RAM fail-closed;
- EmergencyMarker `SafetyCounterExhausted`.

Keine Wiederverwendung alter IDs.

---

# 16. SafetyState-Persistenz

## 16.1 Record

```text
RecordTypeId = 9
Schema = 1
SafetyStorageEpoch = 1

Keys:
sf0
sf1
```

Max Envelope:

```text
1024 Byte
```

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

Basis:

```text
20 Byte
```

## 16.3 Persistenter Faultrecord

| Feld | Wire |
|---|---:|
| instanceId | u32 |
| code | u16 |
| source | u8 |
| flags (`causeCleared`, `restartAttempted`) | u8 |
| firstSeenBootSequence | u32 |
| firstSeenMonotonicMillis | u64 |
| primaryInstanceId | u32 |
| primaryCode | u16 |
| primarySource | u8 |

Pro Fault:

```text
27 Byte
```

Maximal 26 persistente Faults:

```text
payload_max = 20 + 26 * 27
            = 722 Byte

Envelope ohne UTC = 37 Byte

record_max = 759 Byte
```

Unter dem 1024-Byte-Limit.

## 16.4 Kein redundantes Policy-Wireformat

Nicht gespeichert werden:

- FaultClass;
- displayPriority;
- persistence policy;
- autoRearm;
- reset auth policy;
- Fanpolicy;
- Gatewirkung.

Diese Felder folgen ausschliesslich aus dem compile-time Katalog.

---

# 17. SafetyState Initialisierung und Scan

## 17.1 Fabrikneuer Safety-Namespace

Nur wenn:

```text
sf0 == NotFound
sf1 == NotFound
sem0 == NotFound
sem1 == NotFound
```

und alle Reads technisch erfolgreich waren.

Dann SafetyState-Payload:

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

Initialisierung:

1. sf0 Envelope Revision 1 schreiben/readback;
2. sf1 semantisch gleicher Payload, Envelope Revision 2 schreiben/readback;
3. erst danach `storageIntegrityQualified=true`.

Powerloss zwischen 1 und 2 fuehrt beim naechsten Boot in den geschuetzten
Storage-Recoverypfad, nicht zu normalem Allowed.

## 17.2 Normaler Scan

Beide Slots werden technisch geprueft.

### Gesund

- zwei gueltige Records;
- hoechste `versionValue` ist kanonisch;
- gleiche Revision nur erlaubt, wenn Payload bytegleich ist.

### Unsicher

- ReadError;
- CapacityError;
- CRC/Envelopefehler;
- unbekanntes Schema/Epoch;
- eine gueltige und eine fehlende/defekte Seite;
- gleiche Revision mit anderem Payload;
- semantisch ungueltiger Payload.

Unsicher => keine normale Freigabe.

---

# 18. Semantische SafetyState-Validierung

Mindestens:

- `nextInstanceId != 0`;
- `faultRevision != 0`;
- `bootSequence != 0`;
- `abnormalRestartCount <= 3`;
- RestartIntent bekannt;
- `restartIntentState==None` genau mit `restartIntentFaultInstanceId==0`;
- `Prepared` verlangt bekannte persistente target instance;
- `persistentFaultCount <= 26`;
- jeder Record ist S3/Y4;
- bekannte Code-/Source-Kombination;
- eindeutige FaultIdentity;
- eindeutige nonzero instanceId;
- bekannte Flags;
- gueltige Primarymetadaten;
- keine Selbstreferenz;
- keine Restbytes;
- keine unbekannten Enums.

---

# 19. SafetyState Commit

`StorageEnvelope::versionValue` ist die `SafetyStateRecordRevision`.

Verbindlich:

- nach der dualen Initialisierung ist die hoechste Revision 2;
- jede spaetere Mutation verwendet checked `max(validRevision)+1`;
- der Slot mit der aktuell niedrigeren/alten Revision ist das naechste
  Schreibziel;
- `UINT64_MAX` wird nie umgebrochen; Revisionsexhaustion fuehrt in den
  EmergencyMarker-/fail-closed-Pfad.

Normale Mutation:

1. RAM-Kandidat bauen;
2. alle Invarianten;
3. Envelope Revision checked;
4. alternierenden Slot waehlen;
5. kodieren;
6. write;
7. readback;
8. Envelope + Payload exakt verifizieren;
9. erst dann persistent bestaetigt.

`CommitOutcomeUnknown`:

```text
readback exakt neuer Candidate -> committed
readback eindeutig alter Candidate -> nicht committed
sonst -> indeterminate
```

Kein Raten.

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

Max Envelope:

```text
64 Byte
```

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
59 Byte Envelope
```

## 20.3 State

```text
Active
Cleared
```

## 20.4 Reason

```text
SafetyStateWriteFailed
SafetyStateCapacityFailure
SafetyStateCommitIndeterminate
SafetyStateReadbackMismatch
SafetyStateCorruptOrUnreadable
SafetyCounterExhausted
```

## 20.5 MarkerSequence und Slotrotation

`StorageEnvelope::versionValue` ist die `MarkerSequence`.

Verbindlich:

- sind beide Marker-Slots `NotFound`, beginnt der erste `Active`-Marker mit
  Sequence 1 auf `sem0`;
- jeder spaetere Markerzustand verwendet checked `max(validSequence)+1`;
- geschrieben wird bevorzugt der andere Slot als der aktuell hoechste
  gueltige Marker;
- jeder Write wird readback-verifiziert;
- `UINT64_MAX` wird nie auf 0 umgebrochen; bei erschoepfter MarkerSequence ist
  der Marker-Store dauerhaft unqualifiziert und der Boot bleibt fail-closed;
- eine `Cleared`-Sequence darf nur einen aelteren `Active`-Marker ueberstimmen,
  wenn beide Markerrecords technisch/semantisch gueltig sind.

## 20.6 Fehlerpfad

Normaler SafetyState-Commitfehler:

1. RAM SafetyPersistenceUncertain;
2. Gate ImmediateStop;
3. Marker Active auf bevorzugten Slot schreiben/readback;
4. falls nicht bestaetigt: genau ein zweiter Versuch auf dem anderen Slot;
5. danach kein Write-Loop.

Scheitern beide:

- aktueller Boot bleibt fail-closed;
- kein automatischer Restart;
- beim naechsten Boot sind vor jeder Freigabe vollstaendige Producer-
  Requalifikation und erfolgreicher SafetyStorage-Write/Readback Pflicht.

---

# 21. Redundanz-Recovery

Der Recoverypfad darf unbekannte Safetyhistorie nie still als "leer" behandeln.

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
7. zweiten SafetyState-Slot mit naechster Revision auf denselben semantischen
   Zustand bringen/readback;
8. erst jetzt SafetyState-Redundanz als healthy markieren;
9. vorhandene Markerhistorie analog auf zwei technisch gueltige Slots
   reparieren;
10. erst danach einen neuen `Cleared`-Marker schreiben/readback.

Wichtig:

- der neue Y4-004-Latch bleibt **nach** erfolgreicher Redundanzreparatur aktiv;
- Storage-Recovery setzt nur dessen `causeCleared=true`, wenn alle Read/Write-
  und Readbackpruefungen bestanden sind;
- erst ein separater geschuetzter Faultreset darf Y4-004 entfernen;
- SAFE_BOOT bleibt danach zusaetzlich bis zum separaten Exit bestehen.

Damit wird ein moeglicherweise verlorener neuerer Fault nicht still vergessen:
die unbekannte Persistenzhistorie wird durch einen hoeherklassigen
SafetyPersistence-Latch vertreten.

## 21.2 Kein semantisch gueltiger SafetyState vorhanden

Wenn SafetyState-Slots existieren/Fehler melden, aber **kein einziger**
semantisch gueltiger SafetyState rekonstruierbar ist:

- keine neue leere Safetyhistorie erfinden;
- keine InstanceIds/FaultRevisionen auf 1 zuruecksetzen;
- Marker nicht `Cleared` setzen;
- `storageIntegrityQualified=false`;
- SAFE_BOOT bleibt dauerhaft aktiv.

Dieser Zustand benoetigt den bereits vorgesehenen expliziten lokalen
Vollreset-/UART-Recoveryweg; Issue #24 implementiert keinen stillen
Safety-History-Reset.

Nur der in Abschnitt 17.1 bewiesene **vollstaendig fabrikneue Namespace**
(die vier Slots eindeutig `NotFound`, alle Reads erfolgreich) darf neu
initialisiert werden.

## 21.3 Marker-only Unsicherheit

Ist der normale SafetyState vollstaendig gesund, aber der Markerpfad aktiv oder
degradiert:

- normaler SafetyState bleibt Basis;
- Y4-004 / SafetyPersistence wird darin aktiviert, falls noch nicht vorhanden;
- Markerredundanz wird repariert;
- Marker wird `Cleared`;
- Y4-004 bleibt bis CauseClear + geschuetztem Reset;
- SAFE_BOOT bleibt bis separatem Exit.

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

Keine Fermentationsbegriffe im Port.

Testsupport:

```text
SimulatedResetController
```

Kein ESP-IDF-Adapter in #24.

---

# 23. RestartIntent

```text
RestartIntentState::None
RestartIntentState::Prepared
```

## 23.1 Prepare

Nur fuer Katalogeintraege `restartEligible=true`.

Release 1:

```text
S3-004 / ActuatorPlanner
```

Einmal pro `instanceId`.

Vor `requestSoftwareRestart()` persistieren/readback:

```text
fault.restartAttempted=true
restartIntentState=Prepared
restartIntentFaultInstanceId=<instance>
```

## 23.2 Request Rejected

Wenn:

```text
requestSoftwareRestart() == Rejected
```

dann genau ein Commit:

```text
restartIntentState=None
restartIntentFaultInstanceId=0
```

Kein zweiter Restartversuch derselben Instanz.

Commitfehler => EmergencyMarker/fail-closed.

## 23.3 Request Requested

Prepared bleibt bis zum naechsten Boot bestehen.

Es wird kein Reboot behauptet, bevor der naechste Boot tatsaechlich beobachtet
wurde.

---

# 24. BootSequence und RestartEpisode

## 24.1 BootSequence

Nach erfolgreichem SafetyState-Load:

- erster initialisierter Boot: 1;
- jeder weitere Boot checked +1;
- genau einmal pro Boot;
- vor Erzeugung neuer bootlokaler Faulttimestamps.

Overflow => EmergencyMarker/fail-closed.

## 24.2 Resetursache

Abnormal:

```text
Brownout
WatchdogOrPanic
Unknown
SoftwareRestart ohne Prepared
SoftwareRestart mit Prepared
Prepared + unerwartet anderer Resetgrund
```

Nicht abnormal ohne Prepared:

```text
PowerOn
External
```

## 24.3 Prepared konsumieren

Beim Boot:

1. ResetCause lesen;
2. Prepared genau einmal auswerten;
3. abnormalen Counter saturierend aktualisieren;
4. Prepared im selben Safety-Kandidaten auf None setzen;
5. commit/readback.

Kein Boot darf denselben Prepared-Intent zweimal zaehlen.

## 24.4 Counter

```text
0,1,2,3
```

saturierend.

Nie 4.

Bei Uebergang auf 3:

- Y4-005 anlegen oder bestehende Instanz aktiv halten;
- `safeBootRequired=true`;
- ein atomarer SafetyState-Kandidat.

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
-> RunPersistence #17/#18 load (noch keine aktive Recovery)
-> aktuelle Sensorgrundqualifikation
-> Safety-Fault-Mappings
-> SAFE_BOOT-Entscheidung
```

SAFE_BOOT mindestens bei:

- aktiver/unklarer EmergencyMarker;
- unqualifiziertem SafetyStorage;
- bestehendem persistentem S3/Y4 beim Boot;
- abnormalRestartCount == 3;
- Y4-Configurationzustand;
- RunPersistence indeterminiert/nicht rekonstruierbar;
- sicherheitsrelevanter unbekannter Bootevidenz.

Wenn SafetyState technisch schreibbar:

```text
safeBootRequired=true
```

vor `BOOT -> SAFE_BOOT` persistieren/readback.

Wenn SafetyState gerade die Ursache des Problems ist:

- Marker/Corruption selbst haelt SAFE_BOOT persistent/fail-closed;
- Recovery rekonstruiert spaeter zuerst einen State mit
  `safeBootRequired=true`.

---

# 26. Run-Verhalten bei SAFE_BOOT

## 26.1 Kein normaler Recovery-Start

Wenn SAFE_BOOT bereits feststeht:

- `activateLoadedRun()` wird fuer einen normalen aktiven Run **nicht**
  aufgerufen;
- kein PI-/Planner-Resume;
- kein Run wird automatisch weitergefuehrt.

## 26.2 Neuer schmaler #17-Terminalisierungspfad

`RunPersistenceCoordinator` erhaelt genau eine neue Safety-Integration:

```text
terminateLoadedRunForSafeBoot(...)
```

Diese API ist **kein zweiter Persistenzkern**. Sie muss intern den bereits
vorhandenen `writeSnapshotCore()`-/Slot-/Fallback-Vertrag wiederverwenden.

Zulaessige Coordinator-Ausgangszustaende:

```text
LoadedActiveRun
FallbackRecoveryPending
Ready mit bereits restauriertem aktivem Fault-Snapshot
```

Zweck:

- geladenen `LoadedActiveRun` oder `FallbackRecoveryPending` nicht aktivieren;
- daraus einen `NoActiveRun/STANDBY`-Kandidaten bilden;
- vorhandenen `clearActiveRunState()`-Vertrag wiederverwenden;
- `RunPersistenceMutationKind::Recovery` verwenden;
- Fallbackreferenz terminal entfernen;
- den geladenen Snapshot nur soweit in einen lokalen Kandidaten projizieren,
  wie der bestehende Run-Persistence-Vertrag dies bereits tut;
- `clearActiveRunState(candidate)` als einzige terminale Bereinigung;
- Kandidat muss vor Write als gueltiger `NoActiveRun/STANDBY`-Snapshot
  validieren;
- write-before-apply;
- nach Erfolg Coordinator `Ready`.

Wenn der Store nicht eindeutig ist:

- keine Terminalisierung raten;
- SAFE_BOOT bleibt;
- Exit gesperrt.

Ein geladener `ProcessState::Fault` ohne passenden aktiven #24-Latch wird vor
der Terminalisierung als `Y4-001 / RunPersistence` klassifiziert. Damit kann
ein historischer #18-`RecoveryRejected -> Fault` oder ein Crash zwischen
Safetyreset und Run-Terminalisierung nie als bereits bewusst resettiert gelten.

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
   - vorhandenen #17 `persistTransition()` write-before-apply nutzen;
   - aktiven Run als `Fault` erhalten;
7. ohne aktiven Run:
   - ProcessState RAM auf `Fault`;
   - SafetyState ist die rebootpersistente Wahrheit;
8. #17-Fehler beim Fault-Commit:
   - auf Y4-004/Y4-001 mappen;
   - Gate bleibt ohnehin ImmediateStop.

Safety-Abschaltung wartet nie auf Persistenz.

---

# 28. FaultReset / #15

## 28.1 Bestehenden Vertrag beibehalten

`FaultResetEvaluation` bleibt der von #24 qualifizierte #15-Vertrag.

Verbindliche Erweiterung:

```text
targetFaultInstanceId
```

liegt **in `FaultResetEvaluation`**.

Kein alternatives Feld „oder in Decision“.

## 28.2 #15-Verantwortung

`decideFaultReset()` prueft:

- Envelope;
- CommandId;
- expectedStateSequence;
- expectedFaultRevision;
- Bestaetigung;
- `FaultResetEvaluation`;
- targetFaultInstanceId nonzero;
- Evaluation faultRevision == RunCommandState-Projektion.

#15 darf **nicht**:

- FaultCore mutieren;
- faultRevision selbst erhoehen;
- `criticalSafetyEventPending=false` setzen.

Die Decision darf nur RAM-Commandbookkeeping vorbereiten und traegt:

```text
authorizedFaultResetInstanceId
```

als eindeutigen Handoff.

`ResetFault` bleibt gemaess bestehendem #17-Vertrag **nicht** in
`isPersistedRunCommand()`.

---

# 29. FaultReset Commitreihenfolge

```text
SafetyFaultService.evaluateReset(target)
-> #15 decideFaultReset(...)
-> SafetyFaultService.commitReset(
       target,
       expectedFaultRevision)
-> SafetyState persist/readback
-> #15 Commandbookkeeping in RAM anwenden
-> RunCommandState Safetyprojektion synchronisieren
```

Wenn weiterhin blockierende Faults aktiv sind:

```text
ProcessState unveraendert
Gate bleibt sicher
```

Wenn letzter blockierender Fault entfernt wurde:

### Aktueller ProcessState == Fault

- terminalen `FaultResetCompleted`-Uebergang `FAULT -> STANDBY` erzeugen;
- wenn aktiver Run vorhanden:
  - #17 `persistTransition()` erweitern;
  - `clearActiveRunState(candidate)` vor Snapshotbildung;
  - `NoActiveRun/STANDBY` persistieren;
  - erst nach Commit anwenden;
- ohne aktiven Run: RAM-Transition anwenden.

### Aktueller ProcessState == SafeBoot

- **kein Prozessuebergang**;
- SAFE_BOOT bleibt;
- separater Exit erforderlich.

### anderer Zustand

- kein versteckter Prozesswechsel;
- unmoegliche Kombination -> Y4-007 bzw. fail-closed.

---

# 30. FaultReset Crash-/Fehlermatrix

## Vor SafetyState-Commit

Kein Reset wirksam.

## SafetyState-Commit erfolgreich, Crash vor Command-RAM-Bookkeeping

Reboot laedt den dauerhaft resettierten Faultzustand.
Command-ID ist bewusst nicht rebootpersistent.
Kein Sicherheitsproblem.

## SafetyState-Commit erfolgreich, Crash vor FAULT-Terminalisierung

Persistierter Run bleibt `Fault`.

Beim naechsten Boot:

- `Fault` wird nicht automatisch resumt;
- wenn SafetyBoot nicht bereits aus anderem Grund gilt, der persistierte
  Fault-Run ist eine Safety-Reconciliation-Situation;
- der Bootpfad terminalisiert ihn nach `NoActiveRun/STANDBY` oder bleibt
  fail-closed, wenn Persistenz dies nicht erlaubt.

## #17-Terminalisierung schlaegt fehl

- ProcessState bleibt Fault;
- kein Aktor;
- technischer Fehler wird exakt auf Y4-004/Y4-001 gemappt;
- bei SafetyState-Persistenzfehler EmergencyMarker.

---

# 31. Neue ProcessEvents

Nur:

```text
FaultResetCompleted
SafeBootExitCompleted
```

## FaultResetCompleted

```text
Fault -> Standby
```

terminal.

Kein RecoveryEvaluation.

## SafeBootExitCompleted

```text
SafeBoot -> Standby
```

nur nach erfolgreichem SAFE_BOOT-Exit.

`STATE_MACHINE.md` wird entsprechend aktualisiert.

---

# 32. SAFE_BOOT Exit

## 32.1 Voraussetzungen

- aktueller ProcessState `SafeBoot`;
- `safeBootRequired == true`;
- EmergencyMarker eindeutig Cleared/none und Marker-Redundanz healthy;
- SafetyState-Redundanz healthy;
- kein aktiver S3/Y4;
- ConfigurationService `Operational`;
- ConfigurationRecovery aktuell qualifiziert;
- RunPersistence eindeutig und **NoActiveRun**;
- kein RunPersistence-Indeterminate;
- AirSensor VALID;
- CoolingSensor VALID;
- SensorSet nicht unresolved;
- PlannerWatchdog nicht gelatcht;
- kein Y4-Unknown;
- trusted Serviceauthorization true.

ProductSensor ist fuer den Rueckweg nach Standby nicht generell Pflicht.

## 32.2 Reihenfolge

Da der Run bereits beim SAFE_BOOT-Eintritt terminalisiert wurde:

1. alle Exitbedingungen neu pruefen;
2. `safeBootRequired=false` im SafetyState persistieren/readback;
3. erst danach `SafeBootExitCompleted` RAM-Transition;
4. `STANDBY`;
5. Gate kann erst in einem **neuen** normalen Safety-Evaluationsschritt
   `Allowed` werden.

Crash nach SafetyState-Commit vor RAM-Transition:

- Run ist NoActiveRun;
- Reboot sieht `safeBootRequired=false`;
- normale Bootqualifikation fuehrt nach Standby, sofern weiterhin alle Gates
  positiv sind.

---

# 33. SensorQuality #20

## O2-002 STALE

Air/Cooling STALE:

```text
O2-002 / konkrete Source
```

- Gate ImmediateStop fuer Peltier;
- kein ProcessState Fault;
- bestehende #23 Fan-Nachlaufsemantik bleibt aktiv;
- autoRearm erst nach #20-qualifiziertem VALID.

## S3-001 FAILED

Air/Cooling FAILED:

- O2-002 derselben Source entfernen;
- S3-001 raisen;
- Runtime -> Fault;
- nach stabil VALID: `causeCleared=true`;
- Latch bis Reset.

#24 dupliziert keine Filter-/CRC-/Recoverycounter.

---

# 34. SensorSelection #21

## O2-001

ProductSensor degradiert / validierter AirFallback.

`AirFallbackActive + permission Allowed`:

- O2-001 darf sichtbar bleiben;
- Gate darf ansonsten Allowed werden;
- luftgefuehrte Regelung laeuft weiter.

## S3-002

Bei sicherheitsrelevantem:

- `SafeLocked`;
- `CrossRoleEvidenceIndeterminate`;

S3-002 / SensorSet.

## Y4-006 / SensorSet

Nur fuer strukturell ungueltige/unbekannte Safety-Evidenz, nicht fuer normale
PolicyWait/UserAction.

CauseClear erst, wenn der **gesamte aktuelle #21-Safetykontext** wieder
eindeutig qualifiziert ist.

---

# 35. TemperatureControl #22

Keine Faults aus normalen:

```text
NeutralBand
Saturated
AirLimitReduced
AirLimitBlocked
```

Mappings:

```text
InvalidConfiguration      -> Y4-006 / Control
TimeInvalid               -> Y4-006 / Control
RequestIdentityExhausted  -> Y4-006 / Control
```

`SensorUnavailable` / `InvalidSample`:

- Gate unresolved/blocked;
- konkrete Ursache soll aus #20/#21 kommen;
- kein duplizierter Sensorfault.

`NoCommissioning`:

- `Unresolved`;
- kein erfundener Fault;
- keine Aktorfreigabe.

---

# 36. ActuatorPlanner #23

## S3-004 Watchdog

Realer Producer:

```text
ActuatorPlanner::state().latchedWatchdogFault
```

=> S3-004 / ActuatorPlanner.

Einmaliger kontrollierter Restart erlaubt.

CauseClear erst nach neuer aktueller vertrauenswuerdiger Applicationevidenz:

- temperaturgeregelte Phase verlassen;
  oder
- neue strukturell gueltige #22-Evaluation im aktuellen ControlContext;
- Planner/Peltier abstrakt sicher Idle.

Nach erfolgreichem S3-004 Reset:

```text
ActuatorPlanner::applyExternalWatchdogFaultReset(now)
```

genau einmal.

---

# 37. Injection-only Aktordiagnose

## S3-005 – elektrischer/Ausgangsfehler

Zulaessig:

```text
Peltier
OuterFan
InnerFan
```

Noch kein realer Hardwareproducer.

## S3-006 – funktionaler Fanfehler

Zulaessig:

```text
OuterFan
InnerFan
```

Noch kein realer Tachometer-/Stromproducer.

Keine Hardwarefaehigkeit wird behauptet.

---

# 38. Fan-Safety-Directive

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

Aggregation:

```text
ForceOff > ForceOn > PlannerManaged
```

Policy:

- allgemeine Sensor-/Control-/Persistenzfaults:
  - Fan `PlannerManaged`;
  - #23 verwaltet bestehenden Nachlauf;
- S3-005 / OuterFan:
  - outer `ForceOff`;
- S3-005 / InnerFan:
  - inner `ForceOff`;
- S3-006 / OuterFan:
  - outer `ForceOn`;
- S3-006 / InnerFan:
  - inner `ForceOff`;
- Peltierfehler:
  - Peltier Gate ImmediateStop;
  - outer fan `PlannerManaged`.

Diese Aktionen sind **Sollbefehle**, kein physisches Feedback.

---

# 39. RunPersistence #17/#18 – exakte Mappingmatrix

## Boot / Load

| Status | #24 |
|---|---|
| NoPersistedRun | kein Fault |
| Current, normale aktive/Completed-Topologie | kein Fault; normale #18-Bewertung nur wenn kein SAFE_BOOT |
| Current mit `ProcessState::Fault` und bereits passendem aktivem #24-S3/Y4-Latch | kein zusaetzlicher Fault; der Run-Fault ist erwartete Folge |
| Current mit `ProcessState::Fault` ohne passenden #24-Latch | Y4-001 / RunPersistence; niemals normal resumieren |
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

Normale erwartbare:

```text
AlreadyProcessed
AlreadyPersisted
NotEligible
NotAllowedInState
StaleDecision
Busy
NotDue
NoActiveRun
```

sind nicht automatisch persistente SafetyFaults.

Ein `BlockedIndeterminate`-Coordinatorstate ohne bereits gemappte Ursache
liefert Y4-006 / RunPersistence.

---

# 40. Configuration #57

Direkt:

```text
ConfigurationSafetyProducer::ConfigurationUnavailable
ConfigurationSafetyProducer::ConfigurationIntegrityFailure
```

=> Y4-002 / ConfigurationRecovery.

Clear erst wenn aktuelle #57-Recovery eindeutig wieder RuntimeReady bzw.
erfolgreich abgeschlossen ist.

Kein Auto-Reset des Latches.

---

# 41. Configuration #56

`ConfigurationServiceMode`:

```text
RuntimeFailure       -> Y4-003 / ConfigurationRuntime
CommitIndeterminate  -> Y4-003 / ConfigurationRuntime
Operational          -> positive Clear-Qualifikation
```

Zwischenzustaende:

```text
NoRuntime
RecoveryPreparing
CommitInProgress
ResetPreparing
ResetEligibleNoRuntime
EpochResetting
BootstrapFinalizationPending
```

=> Gate `Unresolved`, solange sie fachlich erwartbar sind.

Sie erzeugen nicht pro Tick neue Y4-Faults.

Ein unbekannter/unmoeglicher Modewert:

```text
Y4-006 / ConfigurationRuntime
```

---

# 42. CONFIGURATION_SAFETY_INTEGRATION_GATE

Pflichttests gegen reale Producer:

```text
ConfigurationRuntimeFailure
ConfigurationUnavailable
ConfigurationIntegrityFailure
nicht aufloesbarer CommitOutcomeUnknown / CommitIndeterminate
```

Jeder Fall beweist:

- persistenter Y4-Latch;
- unmittelbare Aktorsperre;
- Reboot loescht Latch nicht;
- Boot -> SAFE_BOOT;
- Ursache-Recovery alleine loescht Latch nicht;
- geschuetzter Faultreset erforderlich;
- SAFE_BOOT bleibt danach bis separatem Exit;
- aktiver Plannerpfad kann Gate nicht umgehen.

---

# 43. Y4-006 – unabhaengige Unknown-Ursachen

Feste Sources:

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

Jede Source ist eigene FaultIdentity.

Beispiel:

```text
Y4-006 / ConfigurationRuntime
Y4-006 / Process
Y4-006 / SensorSet
```

sind drei Instanzen.

Clear von SensorSet beruehrt die beiden anderen nicht.

Innerhalb einer Source wird erst gecleart, wenn **alle** Bedingungen dieser
kanonischen Domain wieder eindeutig sind.

---

# 44. SafetyGate / SafetyDirective

## ImmediateStop

Mindestens:

- ProcessState Fault;
- ProcessState SafeBoot;
- aktiver blockierender S3/Y4;
- EmergencyMarker active/unknown;
- SafetyStorage unqualified;
- SafetyPersistence RAM latch;
- O2-002 Air/Cooling STALE;
- #21 Permission Blocked;
- terminaler Safety-Prozesscommit in Bearbeitung.

## Unresolved

Wenn notwendige aktuelle Evidenz fehlt, aber noch keine stabile FaultIdentity
klassifiziert ist.

## Allowed

Nur bei:

- SafetyStorage healthy;
- Marker resolved/none;
- kein Fault/SafeBoot;
- kein blockierender Fault;
- Configuration Operational;
- Runzustand eindeutig;
- Air VALID;
- Cooling VALID;
- #21 Permission passend;
- ProductEvidence falls aktueller ProductMode sie verlangt;
- ControlContext gueltig;
- PlannerWatchdog nicht gelatcht.

`Allowed` wird nie persistiert.

---

# 45. Kein Caller-supplied Allowed

Der planner-/driver-gebundene
`TemperatureControlApplicationOrchestrator` muss den zentralen
`SafetyFaultService` bzw. einen daraus nicht faelschbaren internen
SafetyDirective-Provider besitzen.

`tickActuatorPlan()` akzeptiert im produktiven gebundenen Pfad **keinen**
freien `ActuatorSafetyGateInput`.

Ablauf:

1. SafetyDirective zentral ableiten;
2. internen Plannerinput bauen;
3. Planner tick;
4. FanDirective auf Plannerresultat anwenden;
5. SinkDriver;
6. PlannerWatchdog-Evidenz an Safety zurueckmelden.

Der actorfreie aktuelle ESP-IDF-Skeletonroot bleibt actorfrei.
#24 baut keine Fake-Hardware auf.

---

# 46. Trust Boundary fuer Authorization

#24 implementiert keine PIN-Pruefung.

Reset-/SAFE_BOOT-Evaluation erhaelt:

```text
authorizationSatisfied
```

nur aus einer vertrauenswuerdigen Application/Auth-Grenze.

UI/Web/Transport duerfen dieses Feld niemals als frei deserialisierbaren
Benutzerwert direkt setzen.

Bis ein produktiver Auth-Producer existiert:

```text
Serviceauthorization im Produktpfad = false
```

Tests duerfen true/false deterministisch injizieren.

Kein Capability-/Token-Framework.

---

# 47. SafetyEvents fuer #19

Typ:

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
SafetyPersistenceFailed
EmergencyMarkerSet
EmergencyMarkerCleared
RunTerminatedByFaultReset
RunTerminatedBySafeBoot
```

`SafetyEvent` enthaelt mindestens:

- kind;
- optional code/source/instanceId;
- optional primary;
- bootSequence;
- monotonicMillis;
- faultRevision.

Jede einzelne Mutation liefert:

```text
SafetyMutationResult
```

mit:

```text
std::array<SafetyEvent, 4>
eventCount <= 4
```

Keine Queue im FaultCore.
Keine Journalpersistenz in #24.
Journalausfall kann Safety nicht blockieren oder freigeben.

---

# 48. Fehlerinjektion

## Sensor

Reale #20/#21-Typen:

- VALID;
- STALE;
- FAILED;
- SafeLocked;
- CrossRole indeterminate;
- Recovery.

## Planner

Realer #23-Watchdog durch echte stale/fehlende Requestfolge.

## Aktor

S3-005/S3-006 ueber typisierte injection-only Evidenz.

## Persistenz

`SimulatedPersistentStateStore`:

- WriteError;
- CapacityError;
- CommitOutcomeUnknown -> neuer Wert;
- CommitOutcomeUnknown -> alter Wert;
- ReadError;
- CRC corruption;
- semantisch ungueltig bei korrektem CRC;
- SafetyState-Slotfehler;
- Marker-Slotfehler;
- Redundanzreparatur.

## Restart

`SimulatedResetController`:

- PowerOn;
- SoftwareRestart;
- WatchdogOrPanic;
- Brownout;
- External;
- Unknown;
- Restart Requested/Rejected.

---

# 49. Testmatrix – FaultCore

- alle 30 erlaubten Identities exakt einmal im Katalog;
- keine ungueltige Code-/Source-Kombination;
- keine Eviction;
- gleiche Identity + 100 wechselnde Mess-/Run-/Plannerrevisionen:
  gleiche instanceId;
- unabhaengige Multi-Faults;
- Klassenprioritaet;
- Intra-Class displayPriority;
- CauseClear;
- Relapse setzt false + neue faultRevision;
- alte ResetEvaluation nach Relapse stale;
- P1/O2 autoRearm nur wenn erlaubt;
- S3/Y4 nie auto reset;
- Primary/Follower;
- Primaryreset loescht Follower nicht;
- faultRevision overflow;
- instanceId overflow.

---

# 50. Testmatrix – Y4-006 Regression

Gleichzeitig:

```text
Y4-006 / ConfigurationRuntime
Y4-006 / Process
Y4-006 / SensorSet
```

SensorSet qualifizieren.

Erwartung:

```text
nur SensorSet causeCleared
andere bleiben aktiv
Gate bleibt ImmediateStop
```

Danach jede Domain einzeln.

Innerhalb ConfigurationRuntime:

- zwei unresolved Unterbedingungen simulieren;
- nur eine beheben;
- Identity darf noch nicht causeCleared werden.

---

# 51. Testmatrix – Wireformat

- initialer Payload;
- 26 persistente Faults;
- flags;
- Primary;
- safeBootRequired;
- RestartIntent;
- RestartCounter 0..3;
- Golden Bytes;
- Roundtrip;
- max 759 Byte;
- unknown Code;
- unknown Source;
- duplicate Identity;
- duplicate instanceId;
- invalid Primary;
- invalid Count;
- rest bytes;
- truncate;
- falsche Epoch;
- falsches Schema;
- falscher RecordType;
- CRC.

---

# 52. Testmatrix – SafetyState Store

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

# 53. Testmatrix – EmergencyMarker/Recovery

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
- MarkerSequence overflow.

---

# 54. Testmatrix – Restart

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
- gleicher Fault nie zweiter Restart;
- Count 1;
- Count 2;
- Count 3 -> Y4-005 + safeBootRequired;
- Count saturiert 3;
- 29:59 normal stabil -> nicht clear;
- 30:00 normal stabil -> Count 0;
- Y4-005 in SAFE_BOOT: 30:00 -> nur causeCleared;
- Y4-005 Reset -> Count 0, SafeBoot bleibt.

---

# 55. Testmatrix – FAULT

- S3/Y4 in aktiver Phase -> immediate Peltier stop;
- SafetyState committed;
- `CriticalFault` persisted;
- aktiver Run bleibt waehrend Fault als Fault-Snapshot;
- zweiter Fault erzeugt keinen zweiten ProcessTransition;
- Reset eines von mehreren Faults -> Fault bleibt;
- letzter Faultreset -> FaultResetCompleted;
- aktiver Run -> terminal `NoActiveRun/STANDBY`;
- `clearActiveRunState()` einzige terminale Runbereinigung;
- kein `RecoveryEvaluation`;
- #17-Commitfehler -> Fault bleibt + Y4 mapping;
- Reboot vor Terminalisierung -> kein automatischer Resume.

---

# 56. Testmatrix – SAFE_BOOT

- persistenter S3/Y4 beim Boot -> safeBootRequired true;
- RestartCount 3 -> safeBootRequired true;
- config Y4 -> safeBootRequired true;
- run Y4 -> safeBootRequired true;
- Markerproblem -> SAFE_BOOT;
- aktiver geladenener Run wird nicht aktiviert;
- terminale Runbereinigung fuer SAFE_BOOT;
- RunStore indeterminate -> kein Exit;
- Reboot verlaesst SafeBoot nicht;
- Faultreset in SafeBoot verlaesst SafeBoot nicht;
- MarkerRecovery verlaesst SafeBoot nicht;
- Exit ohne Auth -> reject;
- Exit mit Air STALE -> reject;
- Exit mit Cooling STALE -> reject;
- Exit mit Config nicht Operational -> reject;
- Exit mit Run nicht NoActiveRun -> reject;
- alle Bedingungen -> safety flag clear -> SafeBootExitCompleted -> Standby;
- kein direkter Aktortest aus SafeBoot.

---

# 57. Testmatrix – #15 FaultReset

- targetFaultInstanceId 0;
- stale faultRevision;
- cause active;
- auth false;
- safetychecks false;
- andere uncleared gleich-/hoeherklassige Ursache;
- andere cause-cleared Latches duerfen Zielreset nicht blockieren;
- #15 mutiert FaultCore nicht;
- #15 setzt criticalSafetyEventPending nicht selbst false;
- ResetFault bleibt non-persisted RunCommand;
- SafetyState ist Reset-Linearisierung;
- Command-RAM-Bookkeeping erst nach Safetycommit;
- Projektion danach synchronisiert.

---

# 58. Testmatrix – Fanpolicy

- Sensorfault waehrend aktivem Peltier -> Peltier sofort Idle, #23 RunOn;
- S3-005 OuterFan -> ForceOff dominiert;
- S3-006 OuterFan -> ForceOn;
- S3-006 InnerFan -> Inner ForceOff;
- gleichzeitig OuterFan ForceOn + elektrischem OuterFan Fault ->
  ForceOff gewinnt;
- kein Fault kann Peltier trotz ImmediateStop aktivieren;
- keine Fanpolicy behauptet Feedback.

---

# 59. Testmatrix – Configuration Gate

Reale Producer:

- ConfigurationRuntimeFailure;
- ConfigurationUnavailable;
- ConfigurationIntegrityFailure;
- CommitIndeterminate.

Jeweils:

- Y4 Identity;
- persistiert;
- gate ImmediateStop;
- reboot retain;
- SAFE_BOOT;
- Recovery -> nur causeClear;
- Reset notwendig;
- SafeBootExit separat;
- Plannerpfad kein Bypass.

---

# 60. Dokumentation

Dauerhaft aktualisieren:

- `docs/SAFETY_AND_FAULTS.md`
- `docs/SAFETY_COMPONENT_FAULTS.md`
- `docs/SYSTEM_SAFETY_AND_RECOVERY.md`
- `docs/ACCEPTANCE_TESTS.md`
- `docs/RUN_COMMANDS.md`
- `docs/STATE_MACHINE.md`
- `docs/RUN_PERSISTENCE.md`
- `docs/RECOVERY_AND_INTERRUPTION.md`
- `docs/ARCHITECTURE.md`
- `docs/ROADMAP.md`

Wesentliche neue dauerhafte Aussagen:

- finaler Faultcodekatalog;
- terminale R1-S3/Y4-Faultpolicy;
- `FAULT -> STANDBY`;
- SAFE_BOOT terminiert geladenen Run statt ihn zu reaktivieren;
- Restart 3 / 30min;
- SafetyState/EmergencyMarker;
- MarkerRecovery != SafeBootExit;
- #15/#24/#17-Autoritaetsgrenze;
- Fan-Safety-Directive.

Keine PR-Reviewhistorie in kanonischen Fachdocs.

---

# 61. Voraussichtliche Dateien

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

Kleine Klassen/Module nach Verantwortung; kein einzelnes 1800-Zeilen-Servicefile.

## Geaendert

```text
run_commands.hpp/.cpp
process_state_machine.hpp/.cpp
run_persistence_coordinator.hpp/.cpp
temperature_control_orchestrator.hpp/.cpp
actuator_plan_types.hpp
```

Nur tatsaechlich notwendige Aenderungen.

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

# 62. Ressourcenregeln

- FaultCore: feste `std::array`, keine unbounded Container;
- maximal 30 Runtime-Identitaeten;
- maximal 26 persistente Faultrecords;
- SafetyState Envelope <= 1024 Byte;
- Marker Envelope <= 64 Byte;
- keine Writes pro Control-Tick;
- gleiche aktive FaultIdentity ohne Zustandsaenderung erzeugt keinen Write;
- Restart stable clear maximal ein Write beim Grenzerreichen;
- SafetyEvents fixed array;
- keine PSRAM-Annahme;
- finaler Base-/Head-Ressourcenvergleich gemaess Quality-Gates.

Keine exakte C++-Struct-RAMgroesse wird vor Compiler-/Buildmessung behauptet.

---

# 63. Umsetzungsschnitte

Jeder Slice:

- gezielte Tests;
- direkt betroffene Konsumententests;
- `git diff --check`;
- Architekturgrenzen;
- Secretcheck;
- Ownerreview;
- kein Full-Suite-Zwang waehrend Draft.

## Slice 1 – FaultCore + Katalog

- Typen;
- 16 Codes;
- 30 erlaubte Identities;
- Policy;
- DisplayPriority;
- CauseClear/Relapse;
- Primary/Follow-up;
- Fanpolicy rein;
- Events rein.

## Slice 2 – SafetyState Codec/Store

- RecordType 9;
- sf0/sf1;
- duale Init;
- 759-Byte-Max;
- semantic decode;
- CommitOutcomeUnknown;
- Redundanz.

## Slice 3 – EmergencyMarker + Repair

- RecordType 10;
- sem0/sem1;
- bounded two-slot fallback;
- Redundanzrecovery;
- MarkerRecovery;
- kein SafeBootExit.

## Slice 4 – RestartSupervisor

- ResetPort;
- Simulator;
- bootSequence;
- RestartIntent;
- 3 / 30min;
- Y4-005.

## Slice 5 – Process/Run Safety

- Runtime S3/Y4 -> Fault;
- FaultResetCompleted -> Standby;
- clearActiveRunState;
- #17 terminal persistTransition;
- terminateLoadedRunForSafeBoot;
- SafeBootExitCompleted;
- kein Fault->RecoveryEvaluation.

## Slice 6 – Producer #20/#21/#22/#23

- Sensor;
- Selection;
- Control;
- Watchdog;
- injection-only actuator;
- FanDirective.

## Slice 7 – Producer #17/#18/#56/#57 + Boot

- exakte Runmappingmatrix;
- Config Gate;
- Bootreihenfolge;
- safeBootRequired true;
- geladenen Run nicht reaktivieren.

## Slice 8 – #15 FaultReset + Actuator Bypass

- targetFaultInstanceId;
- #15 keine SafetyMutation;
- ResetFault non-persisted bestaetigen;
- SafetyState reset linearisieren;
- Orchestrator SafetyDirective zwingend;
- kein caller Allowed.

## Slice 9 – Dokumentation und Abschluss

- alle kanonischen Quellen;
- Roadmap;
- vollstaendiges Ownerreview.

Erst bei 0 Findings und Owner-Anweisung:

- Full Native;
- Builds;
- Ressourcenvergleich;
- CI.

Hardware bleibt `NOT_RUN`, solange nicht separat freigegeben/verkabelt.

---

# 64. Aufgabenliste

## Plan-Gate

- [x] aktuelle Planfassung vollstaendig ersetzen
- [x] Roadmap gegen Live-Stand pruefen
- [ ] PR-Body neue exakte Plan-SHA
- [x] Implementation NOT_STARTED
- [ ] Plan-/Diff-/Architektur-/Secret-Gates
- [x] Tests/Builds NOT_RUN
- [ ] Handover
- [ ] Ownerfreigabe der exakten neuen Plan-SHA

## Slice 1
- [ ] FaultCore
- [ ] Katalog
- [ ] Prioritaet
- [ ] Relapse
- [ ] Events
- [ ] Ownerreview

## Slice 2
- [ ] SafetyState wire
- [ ] duale Init
- [ ] Store
- [ ] Readback
- [ ] Redundanztests
- [ ] Ownerreview

## Slice 3
- [ ] Marker
- [ ] two-slot fallback
- [ ] Reparatur
- [ ] MarkerRecovery != Exit
- [ ] Ownerreview

## Slice 4
- [ ] ResetPort
- [ ] RestartIntent
- [ ] bootSequence
- [ ] 3 / 30min
- [ ] Ownerreview

## Slice 5
- [ ] Runtime Fault
- [ ] Fault -> Standby
- [ ] terminal Run clear
- [ ] SafeBoot Run termination
- [ ] ProcessEvents
- [ ] Ownerreview

## Slice 6
- [ ] #20
- [ ] #21
- [ ] #22
- [ ] #23
- [ ] FanDirective
- [ ] Ownerreview

## Slice 7
- [ ] #17/#18 mapping
- [ ] #56/#57
- [ ] Config gate
- [ ] Boot
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

# 65. Stop-/Replan-Grenzen

Neue vollstaendige Planrevision und neue Owner-SHA erforderlich bei Aenderung an:

- FaultCode-/Source-Katalog;
- FaultIdentity;
- Fault terminal vs Recovery;
- Safety Wireformat;
- RecordType/Keys/Epoch;
- Restart 3 / 30min;
- EmergencyMarker-Semantik;
- #15/#17/Safety Commitreihenfolge;
- SAFE_BOOT Run-Termination;
- Fan-Safety-Directive;
- Aktor-Gate-Autoritaet;
- neuer Hardware-/ESP-IDF-Abhaengigkeit;
- neu behauptetem realen Producer.

Lokale mechanische Implementierungsdetails innerhalb dieser Vertraege
benoetigen keine Planrevision.

---

# 66. Definition of Done

Issue #24 ist abgeschlossen, wenn:

- vier Klassen implementiert;
- stabile Codes implementiert;
- alle erlaubten Identities compile-time begrenzt;
- unabhaengige Ursachen koexistieren;
- kein last-origin-wins;
- Relapse stale-t alte Resetbewertungen;
- S3/Y4 persistent;
- Quittierung/Clear/Reset getrennt;
- Restart einmal pro Fault;
- 3 abnormale Boots -> SAFE_BOOT;
- 30min-Vertrag getestet;
- SAFE_BOOT rebootfest;
- aktiver Run in SAFE_BOOT nicht reaktiviert;
- S3/Y4 Fault terminal nach Standby;
- EmergencyMarker getrennt;
- Redundanzrepair getestet;
- MarkerRecovery != Exit;
- #17/#18/#20/#21/#22/#23/#56/#57 real konsumiert;
- Configuration-Safety-Gate vollstaendig;
- Fan-Safetyreaktionen modelliert;
- kein caller-supplied Allowed;
- Fehlerinjektionen reproduzierbar;
- SafetyEvents bounded bereitgestellt;
- kanonische Doku konsistent;
- gezielte und final geforderte Tests bestanden;
- nicht ausgefuehrte Hardwaretests ehrlich NOT_RUN/BLOCKED;
- keine Hardwarewerte erfunden.

---

# 67. Plan-Gate

Nach Uebernahme dieser vollstaendigen Fassung in PR #108:

1. nur Plan/Roadmap/PR-Body/Handover aendern;
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
