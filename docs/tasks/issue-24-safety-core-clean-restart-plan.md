# Issue #24 – Safety-Core, Verriegelung, Restart, S3-Recovery und Y4-Terminal

Status: **PLANREVISION – NOCH NICHT ZUR UMSETZUNG FREIGEGEBEN**

Issue: #24 – `[E3.5] Fehlerklassen, Verriegelung, SAFE_BOOT und Fehlerinjektion`

Branch: `agent/issue-24-safety-core-clean-restart`

Draft-PR: #108

Live-Basis bei Erstellung dieser vollstaendigen Planfassung:

```text
main @ b8eae5f4da5f2666b5a9bda333d115254c4db5b2
zu ersetzende Plan-SHA:
f944193472bd55d40acd051c3dbe51cffe75fdf7
```

Diese Datei ersetzt **alle frueheren Issue-#24-Planfassungen vollstaendig**.
PR #107 und fruehere Plan-SHAs sind nur historische Lernreferenzen und keine
normative Implementierungsquelle.

Diese Fassung schliesst drei CRITICAL-, vier MAJOR- und einen MODERATE-
Reviewbefund sowie einen Prozessbefund gegen die vorherige Fassung
(`f944193`):

- **C1**: der bisherige Mechanismus „Head-only Fallback-Promotion" (Zeigen
  von `current` auf die bestehende `fallback`-Referenz, kein neuer Payload)
  ist im bestehenden `#17`-Prepared-Head-Wireformat **strukturell nicht
  darstellbar**, weil `validPreparedHead()`/`encodeRunPersistenceHead()`/
  `decodeRunPersistenceHead()` `newRunRevision >= oldRunRevision` und
  `newTransitionSequence >= oldTransitionSequence` sowohl beim Kodieren als
  auch beim Dekodieren hart erzwingen (direkt am Code verifiziert,
  `run_persistence_codec.cpp:1360-1459` und die Decoder-Gegenstelle). Fix:
  eine eng gefasste, explizit benannte und im Dateiscope deklarierte
  Erweiterung von `validPreparedHead()` samt einem neuen, schmalen
  `RunPersistenceHead`-Feld (Abschnitt 29.3).
- **C2**: der Run-Abandon-„Pfad B" aus `BlockedIndeterminate` schrieb in der
  vorherigen Fassung nur einen Head-Verweis, keinen physischen
  `NoActiveRun`-Tombstone-Record. Da `RunCheckpointReference` Laenge/CRC des
  referenzierten Slots traegt, waere ein reiner Head-Write beim naechsten
  Laden erneut als `NotReconstructible` gescheitert. Fix: Pfad B baut,
  schreibt und referenziert jetzt einen echten physischen
  `NoActiveRun/STANDBY`-Tombstone-Record (Abschnitt 26.2).
- **C3**: `PreparedInterrupted` blieb dauerhaft unaufloesbar, weil ein
  gesunder Store einen bereits persistierten `Prepared`-Head nicht von
  selbst aendert. Fix: ein autorisierter Terminalisierungspfad fuer einen
  technisch lesbaren, aber unterbrochenen `Prepared`-Head, der die alte
  Transaktion nie blind committet und nie den alten Run resumiert, sondern
  ehrlich vom letzten bekannten Stand aus einen neuen Tombstone schreibt
  (Abschnitt 26.2).
- **M1**: nach erfolgreicher Fallback-Promotion konnten normale
  `#17`-Schreibpfade (`checkpointPeriodic()`, Command-/Transition-/
  SensorSelection-Writes) den soeben promovierten Head bei verzoegertem oder
  abgelehntem Restart theoretisch ueberschreiben. Fix: neuer RAM-Zustand
  `RunPersistenceCoordinatorState::SafetyRecoveryRestartPending`, der von
  keinem normalen Schreibpfad akzeptiert wird (Abschnitt 5.7, 29.3).
- **M2**: ein einzelner Y4 in der Fault-Historie sollte die Run-Terminalitaet
  fuer den Rest der Episode auch dann noch erzwingen, wenn dieser Y4 selbst
  laengst zurueckgesetzt wurde und nur noch S3-Latches aktiv sind — die
  vorherige Fassung modellierte das nicht, sondern verliess sich auf eine
  reine Prosaerklaerung. Fix: ein erfolgreicher Y4-Reset terminalisiert den
  aktiven Run sofort, unabhaengig von noch aktiven anderen Latches
  (Abschnitt 29.1).
- **M3**: `Y4-006/ActuatorPlanner` blieb `PlannerManaged` fuer die
  Fan-Reaktion, obwohl dieser Fault genau bedeutet, dass die Planner-Domain
  selbst nicht mehr vertrauenswuerdig ist. Fix: deterministische,
  Planner-unabhaengige Fan-Aktion fuer `Y4-006/ActuatorPlanner`; `Y4-007/
  SafetyCore` wurde geprueft und bleibt begruendet `PlannerManaged`
  (Abschnitt 38).
- **M4**: die Fallback-Promotion vermeidet zwar, die Fault-Dauer als Laufzeit
  zu buchen, verschiebt aber die Unsicherheit auf das Intervall zwischen dem
  Fallback-Checkpoint und dem tatsaechlichen Fault-Eintritt. Fix: dies wird
  als bewusste, dokumentierte Entscheidung („Variante A", konservativer
  Rueckfall) festgehalten, begrenzt durch das Checkpoint-Intervall
  (Abschnitt 5.8).
- **D1**: historische `primaryInstanceId`-Werte erhalten eine explizite
  Obergrenze (`< nextInstanceId`, Abschnitt 14).
- **Prozessbefund**: die Aufgabenliste (Abschnitt 69) trennt jetzt
  plan-interne, in dieser SHA tatsaechlich abgeschlossene Punkte von
  Post-Commit-/PR-Metadaten-Schritten, deren Wahrheit im PR-Body/Handover
  liegt, nicht in dieser Datei.

Die Ownerentscheidung `S3 = RecoverIfProvable, Y4 = Terminal` bleibt
unveraendert verbindlich; nur der Mechanismus wird in dieser Runde erneut
korrigiert.

---

# 1. Ziel

Issue #24 implementiert den Release-1-Safety-Core als zentrale Autoritaet
fuer:

- vier Fehlerklassen P1/O2/S3/Y4;
- stabile maschinenlesbare FaultCodes;
- voneinander unabhaengige gleichzeitig aktive Ursachen;
- unmittelbare sichere Aktorreaktionen;
- automatische Wiederfreigabe nur fuer explizit erlaubte P1/O2-Faelle;
- persistente S3/Y4-Verriegelungen mit vollstaendig von P1/O2 getrennten
  Identitaets- und Revisionsraeumen;
- Trennung von Quittierung, Cause-Clear und Reset;
- Primaer-/Follow-up-Beziehungen mit expliziter Obergrenze fuer historische
  Primary-IDs;
- **S3: `RecoverIfProvable`** – nach geschuetztem Reset wird ein vorher
  laufender Fermentationslauf nur dann fortgesetzt, wenn die bestehende
  #18-Recovery ueber einen kontrollierten Restart beweisen kann, dass er
  sicher rekonstruierbar ist; die Fault-Dauer selbst wird dabei niemals als
  normale beobachtete Laufzeit gutgeschrieben; das Intervall zwischen dem
  verwendeten Fallback-Checkpoint und dem tatsaechlichen Fault-Eintritt wird
  als begrenzte, dokumentierte Recovery-Unsicherheit behandelt (Abschnitt
  5.8); sonst terminal;
- **Y4: `Terminal`** – der vorherige Lauf wird nach Y4 in Release 1 nie
  wieder aufgenommen; ein erfolgreicher Y4-Reset terminalisiert den Run
  sofort, unabhaengig davon, ob danach noch weitere Latches aktiv sind;
- einen autorisierten Run-Abandon-/Terminalisierungspfad mit drei
  Eintrittsmoeglichkeiten (aus `LoadedActiveRun`/`FallbackRecoveryPending`/
  `Ready`, aus `BlockedIndeterminate` inklusive `PreparedInterrupted`, und
  aus `SafetyRecoveryRestartPending`), der in jedem Fall einen echten
  physischen `NoActiveRun`-Tombstone schreibt;
- redundante Safety-Persistenz mit vollstaendig normiertem Wireformat;
- getrennten minimalen EmergencyMarker;
- reale Integration der vorhandenen #20/#21/#22/#23/#56/#57-Producer;
- deterministische Fault-Injektion;
- typisierte, in ihrer Anzahl pro Mutation bewiesen begrenzte SafetyEvents
  als spaeteren Input fuer #19;
- ein zentrales Aktor-Safety-Gate ohne caller-supplied Freigabe, das
  waehrend eines S3-Recovery-Handoffs ohne Luecke geschlossen bleibt, und
  das nach erfolgreicher Fallback-Promotion keine normalen Run-Schreibpfade
  mehr zulaesst, bis entweder der tatsaechliche Reboot erfolgt oder ein
  autorisierter Run-Abandon stattfindet.

Fail-closed ist die Grundregel. Kein zweiter Recoveryalgorithmus, kein
zweiter Persistenzkern, keine zweite Recoveryautoritaet.

---

# 2. Verbindliche Quellen und vorhandene Vertraege

Vor Umsetzung und Review sind mindestens zu verwenden:

| Bereich | bestehende Quelle |
|---|---|
| Prozesszustand | #14 / `process_state_machine.*`, `STATE_MACHINE.md` |
| Commands | #15 / `run_commands.*`, `RUN_COMMANDS.md` |
| Run-Persistenz | #17 / `RunPersistenceCoordinator`, `run_persistence_codec.cpp`, `run_persistence_contract.hpp`, `RUN_PERSISTENCE.md` |
| Recovery | #18 / `RunRecoveryCoordinator` (duenner Adapter um `RunPersistenceCoordinator`), `RECOVERY_AND_INTERRUPTION.md` |
| Recovery-Schreibpfad (Zeit-/Gewichtungskandidaten) | `RunPersistenceCoordinator::persistRecoveryCandidate()` |
| Standard-Fallback-Rotation | `writeSnapshotCore()` mit `RunPersistenceFallbackMode::UseStandardFallback` — bewahrt bei jedem neuen aktiven Snapshot den bisherigen `current` als `fallback` |
| Zweiphasiger Head-/Slot-Write | `writeSnapshotCore()`s Prepared-Head-Write -> Slot-Write -> Committed-Head-Write-Sequenz, CAS gegen zuvor gelesene Rohbytes (`run_persistence_coordinator.cpp:1617-1676`) |
| Sensorqualitaet | #20 / `SensorQualitySnapshot` |
| Sensorselektion | #21 / `SensorSelectionRuntimeState` |
| PI / Control | #22 / `TemperatureControlResult` |
| Planner / Watchdog / Fanlogik | #23 / `ActuatorPlanner`, `ACTUATOR_TIMING_AND_FANS.md` |
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

Vor Erstellung dieser Fassung wurden folgende Codefakten direkt am Quellcode
(nicht aus einer Zusammenfassung) verifiziert:

- `RunPersistenceCoordinator::activateLoadedRun()`
  (`run_persistence_coordinator.cpp:499-526`) enthaelt einen **unbedingten**
  Guard:

  ```cpp
  if (current.processState.state == ProcessState::Fault) {
      state_ = RunPersistenceCoordinatorState::Ready;
      auto restored = result(Applied, RamApply, None, Unchanged);
      restored.coordinatorState = state_;
      return {restored, current};
  }
  ```

  `current` ist ein **Parameter**, den der Aufrufer aus dem geladenen
  Snapshot baut — der Guard greift also fuer jeden Run, dessen *persistierte*
  `processState` beim Laden `Fault` ist, unabhaengig von der Historie. Es
  gibt **keinen** In-Prozess-Weg von `Fault` in eine normale
  Recovery-Auswertung.
- `writeSnapshotCore()` setzt bei
  `RunPersistenceFallbackMode::UseStandardFallback` (dem Default von
  `RunPersistenceFallbackDirective{}`) fuer jeden neuen aktiven Snapshot
  `committed.fallback = currentHead_->current;` — die vorherige
  `current`-Referenz wird unveraendert zur neuen `fallback`-Referenz. Wird
  die `CriticalFault`-Transition mit dem Default-Directive persistiert (kein
  explizites Directive noetig), bleibt der **letzte gueltige Pre-Fault-
  Checkpoint** dadurch automatisch, physisch unveraendert und mit seinem
  **echten, eingefrorenen** `checkpointMonotonicMillis`/`ProcessRuntimeState`
  als `fallback` erreichbar.
- **C1 — die zentrale Korrektur dieser Runde:** `RunPersistenceHead` traegt
  `oldRunRevision`/`newRunRevision`/`oldTransitionSequence`/
  `newTransitionSequence` (`run_persistence_contract.hpp:68-84`).
  `validPreparedHead()` und `encodeRunPersistenceHead()`
  (`run_persistence_codec.cpp:1360-1459`) lehnen jeden Prepared-Head-
  Kandidaten mit `newRunRevision < oldRunRevision` oder
  `newTransitionSequence < oldTransitionSequence` **hart ab** — dieselbe
  Bedingung wird beim Dekodieren erneut geprueft. Eine `CriticalFault`-
  Transition erhoeht `transitionSequence`; der Pre-Fault-Fallback-Checkpoint
  hat deshalb **strukturell** eine niedrigere `transitionSequence` als der
  aktuelle Fault-Checkpoint. Ein Head-Kandidat, dessen `current` unveraendert
  auf die `fallback`-Referenz zeigt (vorherige Fassung, C2-Mechanismus),
  kann deshalb mit den echten, ehrlichen Sequenzwerten beider Checkpoints
  **nicht kodiert werden** — dies ist kein Implementierungsdetail, sondern
  eine harte Wireformat-Grenze. Die Korrektur (Abschnitt 29.3) fuehrt eine
  eng gefasste, benannte Ausnahme in `validPreparedHead()` ein.
- Bereits bestehender Präzedenzfall fuer eine Recovery-spezifische Ausnahme
  in `validPreparedHead()`: `validSameSlotRecovery` erlaubt fuer
  `mutationKind == Recovery` mit gueltigem `preparedFallback` auf einem
  anderen Slot als `target`, dass `preparedCurrent->slot == target.slot`
  ist. Die neue C1-Ausnahme (Abschnitt 29.3) ist ein **eigenstaendiger,
  disjunkter** Zweig — sie verlangt `!preparedFallback.has_value()` und
  kollidiert dadurch nicht mit `validSameSlotRecovery`s Bedeutung dieses
  Felds.
- `RunPersistenceMutationKind` (`run_persistence_contract.hpp:61-66`) hat
  exakt vier Werte: `Command=1`, `Transition=2`, `SensorSelection=3`,
  `Recovery=4`. `RunCheckpointVariant`
  (`run_persistence_contract.hpp:33-37`): `ProgramRun=1`, `ManualRun=2`,
  `NoActiveRun=3`. Aktuelles Schema: `kCurrentRunPersistenceSchema = 3U`
  (`run_persistence_contract.hpp:30`); `kRecoveryFieldsIntroducedInSchema =
  3U` (`run_persistence_codec.cpp:25`).
- `RunCheckpointReference` (`run_persistence_contract.hpp:46-54`) ist ein
  reiner Slot-Deskriptor (`slot`, `schemaVersion`, `storageEpoch`,
  `checkpointRevision`, `payloadLength`, `payloadCrc`, `variant`) — kein
  Payload. **C2 — die zentrale Korrektur dieser Runde:** genau weil
  `payloadLength`/`payloadCrc` Teil der Referenz sind, reicht ein reiner
  Head-Write, der auf einen Slot ohne passenden physischen Record zeigt,
  nicht aus — `runCheckpointReferenceMatches()`-artige Ladepruefungen
  schlagen beim naechsten Boot fehl. Jeder terminale `NoActiveRun`-Head-
  Commit braucht einen echten physischen Slot-Write.
- Der bestehende zweiphasige Write (`run_persistence_coordinator.cpp:1617-
  1676`, nicht-periodischer Zweig) schreibt in dieser Reihenfolge: (1)
  Prepared-Head via `store_.writeHeadExact(preparedBytes, oldHead, ...)`,
  (2) Slot via `store_.writeSlotExact(target, targetBytes, oldSlot, ...)`,
  (3) Committed-Head via `store_.writeHeadExact(committedBytes,
  preparedBytes, ...)`. `writeHeadExact()`s CAS-Praeimage ist ein
  **rohes Byte-`std::optional<std::string>`**, unabhaengig davon, ob die
  referenzierten Altbytes einen `Prepared`- oder `Committed`-Head
  dekodieren — direkt am Code verifiziert. **C3 — die zentrale Erkenntnis
  dieser Runde:** eine Terminalisierung eines stecken gebliebenen
  `Prepared`-Heads braucht deshalb **keine** Codec-Aenderung — sie kann
  denselben Primitiv verwenden, mit dem gelesenen `Prepared`-Head als
  CAS-Praeimage fuer eine neue, gewoehnlich-monotone Transaktion. Dies steht
  im bewussten Kontrast zu C1 (dort *ist* eine Codec-Aenderung noetig, weil
  dort echte Rueckwaertsbewegung in den Sequenzwerten verlangt wird, nicht
  nur ein neues CAS-Praeimage).
- `loadAndInitialize()` (`run_persistence_coordinator.cpp:257-284`)
  konsultiert `fallback` **ausschliesslich**, wenn das Laden von `current`
  technisch fehlschlaegt (`loadReference()` liefert keinen Wert). Ein
  technisch valider, aber semantisch `Fault`-Snapshot in `current` fuehrt
  **nicht** automatisch zur Fallback-Nutzung — das Umschalten muss aktiv
  (durch einen eigenen Head-Commit) erfolgen.
- `enterBlockedIndeterminate()` (`run_persistence_coordinator.cpp:238-240`)
  ist eine reine RAM-Zustandsaenderung (`state_ = BlockedIndeterminate;`)
  ohne Persistenzseiteneffekt — der physische Store bleibt unangetastet.
  `persistRecoveryCandidate()` akzeptiert aktuell nur `Ready` oder
  `LoadedActiveRun` (`run_persistence_coordinator.cpp:1827-1830`) und lehnt
  `BlockedIndeterminate` ab. Dieselbe Guard-Struktur (Ablehnung nicht
  explizit erlaubter Zustaende) ist die Grundlage fuer den neuen
  `SafetyRecoveryRestartPending`-Zustand (M1, Abschnitt 5.7).
- `loadAndInitialize()` ist ein Einmalvertrag (`state_ != Uninitialized` ->
  `AlreadyInitialized`, Zeile 258-260) und darf nach `BlockedIndeterminate`
  nicht erneut aufgerufen werden, um eine Requalifikation zu versuchen.
- `docs/SAFETY_AND_FAULTS.md:175-178`: „Der Aussenluefter wird nicht
  pauschal zusammen mit dem Peltier abgeschaltet, solange Restwaerme
  abgefuehrt werden muss. Der Innenluefter kann je nach Ursache weiterlaufen,
  nachlaufen oder abgeschaltet werden."; Zeile 160-161: „Aussenluefter-
  Nachlauf beziehungsweise erforderliche Dauerbelueftung starten ->
  Innenluefter gemaess Fehlerursache behandeln" als **generische** Reaktion
  bei jedem sicherheitsrelevanten Fehler, nicht nur bei fanbezogenen.
- `docs/SAFETY_COMPONENT_FAULTS.md:374-375`: Aussenluefterausgang bleibt zur
  Restwaermeabfuhr eingeschaltet, „sofern kein elektrischer Ausgangsfehler
  dagegen spricht"; Zeile 461: „Aussenluefterfehler | Peltier AUS,
  Luefterausgang soweit sicher EIN".
- `docs/SAFETY_AND_FAULTS.md:211-213`: „einfache behebbare Betriebsfehler
  duerfen durch einen normalen Bediener zurueckgesetzt werden ...
  sicherheitskritische oder technische Fehler verlangen die Service-PIN".
  Keine der kanonischen Quellen (`SAFETY_AND_FAULTS.md`,
  `SAFETY_COMPONENT_FAULTS.md`, `SYSTEM_SAFETY_AND_RECOVERY.md`) nennt eine
  Operator-Ausnahme fuer einen einzelnen S3-Code — alle dort genannten
  Klasse-3-Beispiele (Zeile 100-103: Ueber-/Untertemperatur,
  H-Bruecken-Fehlansteuerung, ausgefallener Schrankluftfuehler,
  Aussenluefterfehler) sind ausnahmslos Service-PIN-pflichtig.

---

# 3. Adopt-or-build / Bibliotheken

Es wird **kein neues Fault-, FSM-, Event-Bus- oder Persistence-Framework**
eingefuehrt.

Vorhandene technische Primitive werden wiederverwendet: `IStateStore`,
`StateStoreKey`, `StorageEnvelope`, Slot-Scan und Payload-Load,
Big-Endian-Codecs, CRC, `ITimeSource`, bestehende Run-/Config-/Sensor-/
Control-/Planner-Vertraege, die bestehende Standard-Fallback-Rotation in
`writeSnapshotCore()`, die bestehende zweiphasige Prepared-/Slot-/Committed-
Write-Sequenz und ihre CAS-Primitiven. Die einzige neue Wireformat-
Erweiterung dieser Runde ist eng gefasst: ein zusaetzliches, schema-
gegatetes `bool`-Feld auf `RunPersistenceHead` und eine eng benannte
Ausnahme in `validPreparedHead()` fuer die verifizierte Safety-Fallback-
Promotion (Abschnitt 29.3, C1). Kein zweiter Codec, kein zweites Wireformat.

Die projektspezifische Fault-/Reset-/SAFE_BOOT-Policy bleibt im
`fermentation_app`-Fachkern. Fuer einen spaeteren realen ESP-IDF-
Resetadapter wird die native ESP-IDF-API verwendet; dieser Adapter ist
**nicht** Scope von Issue #24. Neue Drittanbieter-Lizenzabhaengigkeiten
entstehen nicht.

---

# 4. Nicht-Scope

Issue #24 implementiert nicht: ESP-IDF-Resetadapter; `esp_restart()`-
Adapter; NVS-Adapter (#90); reale GPIO-/BTS7960-/Fan-/Sensoradapter;
Fan-Tachometer; Strommessung; thermische Commissioningwerte aus #35;
produktive Service-PIN-/Loginlogik; Webtransport; OTA; Journalpersistenz/
Retention/Export/Import aus #19; allgemeine Event-Busse; eine neue
allgemeine Recovery-State-Machine (die bestehende `#18`-Logik wird fuer
S3-Recovery unveraendert wiederverwendet); ein Schema-4-Fault-Recoverymodell
im Sinne eines neuen Faultwireformats (die Schema-Erweiterung dieser Runde
betrifft ausschliesslich `RunPersistenceHead`, nicht `SafetyStateRecord`).

Physische Fan-/Aktorfehler ohne realen Producer bleiben ehrlich
**injection-only**.

Produktionsgrenze: die reale ESP32-Resetursache/-Resetanforderung und die
Hardwarebaseline laufen ueber die bestehenden Hardware-/Bring-up-Gates
(u. a. #29). Keine produktive Aktorfreigabe darf #24 umgehen.

---

# 5. Release-1-Entscheidungen

## 5.1 Restart-Episode

```text
3 abnormale Boots innerhalb derselben Restart-Episode
=> Y4-005 + SAFE_BOOT
```

Vor Erreichen von 3 endet eine Restart-Episode nach 30 Minuten
ununterbrochener stabiler Uptime ohne neuen abnormalen Reset, ohne aktiven
S3/Y4, ohne offenen RestartIntent, mit gesunder Safety-Persistenz. Die Zeit
wird nur innerhalb des aktuellen Boots mit `monotonicMillis()` gemessen;
keine monotone Zeit wird ueber Reboots hinweg subtrahiert. PowerOn/
External-Reset ohne offenen RestartIntent erhoeht den abnormalen Zaehler
nicht, loescht ihn aber auch nicht automatisch. Brownout, Watchdog/Panic,
unbekannte Resetursache und unerwarteter Software-Reset sind abnormal.

Ein von #24 vorbereiteter kontrollierter Software-Restart (S3-Recovery-
Handoff, Abschnitt 29.3) zaehlt als Teil der abnormalen Restart-Episode. Er
darf pro auslosender persistenter Faultinstanz genau einmal automatisch
angefordert werden — nicht durch erneute Bindung an eine (bereits entfernte)
Instanz, sondern durch das in Abschnitt 23 definierte historische
`RestartIntent`, das nach Verbrauch nie erneut fuer dieselbe
`sourceInstanceId` gesetzt wird.

## 5.2 Y4-005-Requalifikation

Bei `abnormalRestartCount == 3` bleibt Y4-005 gelatcht. Nach 30 Minuten
stabiler Uptime **auch innerhalb SAFE_BOOT** darf ausschliesslich dessen
Ursache `causeCleared = true` werden; der Counter bleibt 3 und der Latch
bleibt aktiv. Erst ein erfolgreicher geschuetzter Reset von Y4-005 setzt
`abnormalRestartCount = 0` und entfernt den Fault. Danach bleibt `SAFE_BOOT`
bestehen, bis der separate SAFE_BOOT-Exit erfolgreich ist (Y4-005 ist wie
jedes Y4 `Terminal` — kein Run-Resume).

## 5.3 S3 = `RecoverIfProvable`, Y4 = `Terminal` (verbindliche Ownerentscheidung)

Diese Entscheidung ist getroffen und **kein offenes Owner-Gate**. Nur der
Mechanismus wurde in dieser Runde erneut korrigiert (C1-C3, M1, M2).

### S3

```text
S3 -> FAULT
  #17 persistiert die Fault-Transition mit Standard-Fallback-Rotation:
  der letzte gueltige Pre-Fault-Checkpoint bleibt unveraendert als
  `fallback` erreichbar (Abschnitt 27)
CauseClear
-> geschuetzter Reset (SafetyState-Commit: Latch entfernt)
-> war dieser Reset ein Y4 UND ist ein aktiver Run vorhanden?
     ja: Run sofort terminalisiert (M2, Abschnitt 29.1) -- dieser Zweig
         betrifft nur Y4, siehe Y4-Abschnitt unten
-> nach diesem Reset: aktiver (noch nicht terminalisierter) Run vorhanden
   UND kein weiterer blockierender Fault mehr aktiv?
     nein (Run bereits terminalisiert, oder weitere Faults aktiv): siehe
       Abschnitt 29.1/29.2
     ja: S3RecoveryHandoffPending (RAM bleibt durchgehend in FAULT,
         Gate bleibt ImmediateStop -- kein Gate-Fenster, schliesst C3-alt)
         -> Fallback technisch/semantisch verifizieren
         -> gueltig: verifizierte Safety-Fallback-Promotion ueber die
            neue, eng gefasste `validPreparedHead()`-Ausnahme (C1,
            Abschnitt 29.3): current := bisherige fallback-Referenz,
            UNVERAENDERTE Payload-Bytes -- schliesst C2-alt (keine
            Fault-Zeit wird als Laufzeit gebucht)
            -> Coordinatorzustand SafetyRecoveryRestartPending (M1):
               kein normaler #17-Schreibpfad mehr moeglich, bis Reboot
               oder autorisierter Run-Abandon
            -> RestartIntent(kind=S3Recovery,
               sourceInstanceId=<entfernte Faultinstanz, nur historisch,
               schliesst C1-RestartIntent>)
            -> genau ein kontrollierter Restart
         -> ungueltig: RecoverIfProvable=false -> Run-Abandon -> STANDBY
-> naechster Boot: vollstaendig unveraenderte #18-Logik
     (loadAndInitialize -> current ist jetzt der promovierte, zeitlich
     unverfaelschte Pre-Fault-Checkpoint -> activateLoadedRun/Hop-1/Hop-2)
     -> sicher beweisbar: Resume
     -> nicht sicher beweisbar: #18s eigene Terminalisierung bzw.
        Run-Abandon-Pfad (Abschnitt 26) -> STANDBY
```

Es gibt keine automatische S3-Wiederfreigabe und keine erfundene Recovery.
Kein zweiter Recoveryalgorithmus: der einzige neue #24-Beitrag ist die eng
gefasste Fallback-Promotion (ein schmaler, referenzieller #17-Write unter
einer neuen, eng benannten Codec-Ausnahme) und das Anfordern eines Reboots —
die gesamte inhaltliche Recovery-Entscheidung bleibt bei der bestehenden,
unveraenderten `#18`-Logik.

### Y4

```text
Y4 -> FAULT / SAFE_BOOT
Ursache beheben
-> causeCleared
-> geschuetzter Reset
-> aktiver Run vorhanden?
     ja: SOFORTIGE Run-Terminalisierung ueber clearActiveRunState()
         (M2, Abschnitt 29.1) -- unabhaengig davon, ob nach diesem Reset
         noch weitere Latches (S3 oder Y4) aktiv sind; ProcessState
         bleibt Fault, falls weitere Latches aktiv sind, aber der Run
         selbst ist ab diesem Moment bereits NoActiveRun
     nein: nichts zu terminalisieren
-> kein weiterer blockierender Fault mehr aktiv?
     ja: FAULT -> STANDBY (bzw. SAFE_BOOT bleibt bis separatem Exit)
     nein: ProcessState bleibt Fault, kein weiterer Schritt
```

Kein `RecoverIfProvable` fuer Y4. Kein Restart-fuer-Recovery-Zweck. Die
Terminalitaet eines Runs, der irgendwann waehrend seiner Fault-Episode einen
aktiven Y4 hatte, ist ab dieser Fassung **kein separat zu pruefendes
Historienmerkmal mehr, sondern eine strukturelle Konsequenz**: der Run wird
bereits im Moment des Y4-Resets terminalisiert (M2) — ein spaeterer Reset
eines verbleibenden S3 trifft deshalb nie mehr auf einen aktiven Run, den er
recovern koennte (Abschnitt 30, Fall 17, jetzt mechanisch statt nur
deklarativ geschlossen).

### Kein zweiter Recovery-Vertrag

Die bestehende #18-Recovery bleibt die einzige fachliche Recoveryautoritaet.
Ein blosses `FAULT -> RECOVERY_EVALUATION` ohne die von #18 verlangten Anker
und Evidenzen bleibt verboten.

## 5.4 SAFE_BOOT beendet einen noch nicht reaktivierten Lauf

Wenn beim Boot `SAFE_BOOT` erforderlich ist: ein geladener aktiver Run wird
nicht ueber #18 reaktiviert; sobald der Run-Store eindeutig les-/schreibbar
ist, wird er ueber den Run-Abandon-Pfad (Abschnitt 26) als
`NoActiveRun/STANDBY` terminalisiert; ist der Run-Store indeterminiert,
bleibt das Geraet in SAFE_BOOT und der Exit bleibt gesperrt.

## 5.5 SafetyStorageEpoch

Schema 1 verwendet `SafetyStorageEpoch = 1` unabhaengig von der
Configuration-`StorageEpoch`. Ein Configuration-Werksreset darf
Safety-Latches nicht implizit unlesbar machen oder loeschen. Eine spaetere
explizite Safety-Epoch-Aenderung benoetigt eigenen Scope.

## 5.6 Persistente S3/Y4- und transiente P1/O2-Identitaet sind vollstaendig getrennt

Persistente Identitaet (`nextInstanceId`, `faultRevision`) gilt
ausschliesslich fuer rebootrelevante S3/Y4-Latches. P1/O2 erhalten einen
eigenen, rein RAM-lokalen Identitaets-/Revisionsraum, der nie persistiert
wird und nie denselben Zaehler wie S3/Y4 konsumiert.

**Begruendung:** `faultRevision` ist der Staleness-Anker fuer
`FaultResetEvaluation`. Ein gemeinsamer Zaehler koennte durch haeufige
P1/O2-Ereignisse in RAM erhoeht werden, ohne mitpersistiert zu werden — nach
einem Reboot koennte `faultRevision` dadurch zurueckspringen und eine alte,
laengst ueberholte Command-Entscheidung wieder faelschlich matchen: ein
Staleness-Check-Bypass. Details in Abschnitt 15.

## 5.7 Zwei RAM-lokale Orchestrierungszustaende: `S3RecoveryHandoffPending` und `SafetyRecoveryRestartPending`

`S3RecoveryHandoffPending` (Abschnitt 29.3) ist ein benannter, RAM-lokaler
Orchestrierungszustand des `SafetyProcessCoordinator` fuer genau diese
Boot-Session, solange die Fallback-Verifikation/-Promotion selbst laeuft.
Sein Namensraum dient Diagnose/Testbarkeit; seine Gate-Wirkung ist
**vollstaendig redundant** mit der bereits bestehenden, unbedingten
`ProcessState::Fault`-Regel in der `ImmediateStop`-Matrix (Abschnitt 43) —
RAM verlaesst `Fault` in diesem gesamten Ablauf nie vor dem tatsaechlichen
Reboot (Abschnitt 27, 29.3). Es entsteht dadurch **keine** zweite,
unabhaengige Sicherheitsquelle.

`SafetyRecoveryRestartPending` (M1, neu in dieser Runde) ist dagegen ein
echter, neuer Wert des bestehenden `RunPersistenceCoordinatorState`-Enums,
kein reiner Diagnosetag. Der `RunPersistenceCoordinator` wechselt genau dann
in diesen Zustand, wenn die Fallback-Promotion (Abschnitt 29.3) `Applied`
zurueckgibt — **statt** in den sonst ueblichen `Ready`-Zustand. Da
`checkpointPeriodic()`, `persistCommand()`, `persistTransition()`,
`persistSensorSelection()` und `persistRecoveryCandidate()` bereits heute
jeweils eine explizite Liste akzeptierter Zustaende pruefen und jeden nicht
gelisteten Zustand ablehnen (verifiziert am bestehenden `Ready`/
`LoadedActiveRun`-Guard von `persistRecoveryCandidate()`, Abschnitt 2.1),
reicht es, `SafetyRecoveryRestartPending` in **keiner** dieser Listen
aufzunehmen — alle fuenf Schreibpfade lehnen dadurch automatisch mit dem
bereits bestehenden, bereits als unkritisch klassifizierten
`NotAllowedInState`-Ergebnis ab (Abschnitt 39). Es wird kein neuer,
paralleler Blockierungsmechanismus erfunden, sondern derselbe bestehende
Zustands-Guard um einen weiteren Wert erweitert. Einzige zulaessige Wege aus
`SafetyRecoveryRestartPending` heraus: ein tatsaechlicher Reboot (der
Zustand ist reine RAM-Information und wird beim naechsten
`loadAndInitialize()` nicht rekonstruiert, sondern faellt weg), oder ein
autorisierter Run-Abandon, der `SafetyRecoveryRestartPending` als dritten
Eintrittszustand neben `BlockedIndeterminate` und
`LoadedActiveRun`/`FallbackRecoveryPending`/`Ready` akzeptiert (Abschnitt
26.2).

## 5.8 M4: zeitliche Grenzen der Fallback-Promotion (Variante A)

Die Fallback-Promotion verhindert, dass die Fault-Dauer selbst als sicher
beobachtete Laufzeit gebucht wird (C2). Sie verschiebt die Wahrheitsbasis
aber auf den **letzten gueltigen Pre-Fault-Checkpoint**, nicht auf den
exakten Moment des Fault-Eintritts selbst — zwischen diesem Checkpoint und
dem tatsaechlichen Fault-Eintritt kann bis zu einem Checkpoint-Intervall
(`intervalMinutes`, oder weniger, falls ein Transition-/Command-Checkpoint
kurz vor dem Fault lag) ungeklaerte Zeit liegen. Diese Runde trifft dazu
explizit **Variante A** (konservativer Rueckfall), wie im Auftrag als
bevorzugt benannt:

1. Recovery faellt auf den letzten gueltigen Checkpoint zurueck — dies ist
   bereits durch die Standard-Fallback-Rotation (Abschnitt 27) so
   festgelegt, keine neue Mechanik.
2. Die Zeit zwischen diesem Checkpoint (`t0`) und dem tatsaechlichen
   Fault-Eintritt (`t1`) wird **nicht separat modelliert**, sondern faellt
   vollstaendig in dieselbe Boot-zu-Boot-Luecke, die #18 fuer JEDE
   Unterbrechung ohnehin bewertet (`t0` bis zum Recovery-Boot `t2`) — sie
   wird an keiner Stelle als sicher beobachteter Fortschritt behandelt.
3. Es wird an keiner Stelle blind normale Zeit gutgeschrieben — weder fuer
   `t0..t1` noch fuer `t1..t2`.
4. #18 allein entscheidet ueber UTC-/Temperatur-/Evidenzbasierte
   Vertrauenswuerdigkeit dieser gesamten Luecke (`t0..t2`), unveraendert
   nach seinen bestehenden Regeln (Hop-1/Hop-2, `RecoveryConfidence`).
   #24 fuehrt hier keine eigene Zeit-/Fortschrittslogik ein.
5. Die zusaetzliche Unsicherheit gegenueber einem hypothetischen exakten
   Fault-Zeitstempel ist durch das Checkpoint-Intervall nach oben begrenzt
   — sie waechst nicht unbegrenzt und ist nicht Teil eines neuen Wirefelds.

Es wird kein „Pre-Fault-Boundary-Anchor" (Variante B) und kein neues
Wirefeld fuer diesen Zweck eingefuehrt — dies wuerde eine zweite,
ueberlappende Unsicherheitsquelle neben der bereits bestehenden
#18-Boot-Luecken-Bewertung schaffen, ohne fachlich zwingenden Bedarf.

---

# 6. Architektur und Verantwortungen

## 6.1 FaultCore – reine Policy

`FaultCore` ist hardwarefrei, persistenzfrei, deterministisch, nativ
testbar, ohne dynamische Allokation. Er besitzt zwei getrennte
Faultzustaende: `PersistentFaultState` (S3/Y4, mit
`nextInstanceId`/`faultRevision`) und `TransientFaultState` (P1/O2, mit
eigenem, nie persistiertem `transientNextInstanceId`/
`transientFaultRevision`); FaultIdentity je Instanz; Cause-Clear; Primary-/
Follow-up-Bezug (nur innerhalb derselben Klasse); Hauptfaultauswahl
(klassenuebergreifend: Y4 > S3 > O2 > P1); Policyaggregation aus dem
compile-time Katalog (Abschnitt 11). Er schreibt keinen Store und ruft keine
Hardware.

## 6.2 SafetyStateStore – nur persistenter Safetyrecord (S3/Y4)

Verantwortung: Keys, Envelope, Codec, Slot-Scan, Readback,
CommitOutcomeUnknown, Redundanzpruefung, geschuetzte Reparatur. Keine
Faultklassifikation. Kennt nur `PersistentFaultState`.

## 6.3 SafetyEmergencyMarkerStore

Getrennter kleiner Storepfad fuer normalen SafetyState-Commitfehler,
unbestimmten Safety-State, Counter-/Sequence-Exhaustion. Keine normale
Faultliste.

## 6.4 SafetyFaultService – eine mutable Safety-Autoritaet

Besitzt: `FaultCore` (beide Teilzustaende), geladenen/persistierten
SafetyState, RestartEpisode, RestartIntent (Abschnitt 23, neues Format),
`safeBootRequired`, Storagequalification, Resetbewertung, Resetcommit,
SafetyMutationResults. Es ist die einzige mutable Fault-/Latch-/Reset-
Autoritaet.

## 6.5 SafetyProcessCoordinator – Cross-Domain-Sequenzierung, S3-Recovery-Handoff, Run-Abandon

Ein kleiner `SafetyProcessCoordinator` koordiniert: Bootreihenfolge;
Run-Persistenzstatus; Runtime-`FAULT`-Eintritt; sofortige Y4-Run-
Terminalisierung (M2, Abschnitt 29.1); **S3-Recovery-Handoff**
(Fallback-Verifikation, verifizierte Safety-Fallback-Promotion,
RestartIntent, Restartanforderung — Abschnitt 29.3) unter der
Diagnosekennzeichnung `S3RecoveryHandoffPending` und dem anschliessenden
Coordinatorzustand `SafetyRecoveryRestartPending` (M1); terminalen
`FAULT -> STANDBY`-Pfad (Y4 sowie S3 ohne aktiven Run); SAFE_BOOT-Eintritt;
**Run-Abandon** mit drei Eintrittspfaden (Abschnitt 26, inklusive
Requalifikationspfad aus `BlockedIndeterminate`, autorisierter
`PreparedInterrupted`-Terminalisierung, und Abandon aus
`SafetyRecoveryRestartPending`); SAFE_BOOT-Exit; #15-FaultReset-Handoff.

Er besitzt **keinen zweiten Faultzustand** und **keinen zweiten
Recoveryalgorithmus**. Er ruft ausschliesslich vorhandene #17-/#18-/
StateMachine-Vertraege auf und die schmalen neuen #17-Erweiterungen
(verifizierte Safety-Fallback-Promotion, Run-Abandon mit physischem
Tombstone-Write, `PreparedInterrupted`-Terminalisierung).

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

```cpp
struct FaultIdentity {
    FaultCode code;
    FaultSource source;
};
```

Nicht Bestandteil: RunRevision, FaultRevision, ControlRequestSequence,
Sensorsequence, Plannersequence, Bootzeit, Config-StateRevision,
Persistenzrevision, Zeitstempel. Ein neues Messergebnis derselben Ursache
erzeugt keine neue Instanz. Eine `FaultIdentity` ist entweder ausschliesslich
persistent (S3/Y4) oder ausschliesslich transient (P1/O2) — bestimmt einzig
durch `FaultClass` im compile-time Katalog, niemals laufzeitabhaengig.

---

# 9. FaultSource

## 9.1 Release-1-Sources

```text
Process, ProductSensor, AirSensor, CoolingSensor, SensorSet,
TemperatureSafety, Control, ActuatorPlanner, Peltier, OuterFan, InnerFan,
RunPersistence, ConfigurationRuntime, ConfigurationRecovery,
SafetyPersistence, RestartSupervisor, SafetyCore
```

Unbekannte Werte werden abgelehnt.

## 9.2 Stabile Wirewerte (F4, unveraendert)

```text
0 = reserviert/ungueltig
1=Process 2=ProductSensor 3=AirSensor 4=CoolingSensor 5=SensorSet
6=TemperatureSafety 7=Control 8=ActuatorPlanner 9=Peltier 10=OuterFan
11=InnerFan 12=RunPersistence 13=ConfigurationRuntime
14=ConfigurationRecovery 15=SafetyPersistence 16=RestartSupervisor
17=SafetyCore
```

Ein Decoder, der einen Wert ausserhalb `1..17` liest, lehnt den Record ab
(fail-closed, kein Default).

---

# 10. Stabile FaultCodes

```text
P1-001=0x1001
O2-001=0x2001 O2-002=0x2002
S3-001=0x3001 S3-002=0x3002 S3-003=0x3003 S3-004=0x3004 S3-005=0x3005 S3-006=0x3006
Y4-001=0x4001 Y4-002=0x4002 Y4-003=0x4003 Y4-004=0x4004 Y4-005=0x4005 Y4-006=0x4006 Y4-007=0x4007
```

Sichtbarer technischer Code ist entsprechend `P1-001` usw.; Texte werden
spaeter ueber den Code lokalisiert.

---

# 11. Vollstaendige FaultCatalog-Matrix (F5, M3 in dieser Runde erneut korrigiert)

Diese Matrix ist fuer **jede** erlaubte `(FaultCode, FaultSource)`-Identity
vollstaendig und normativ. Keine Zelle ist `TBD`.

Spaltenkuerzel: **Kl.**=FaultClass, **P/T**=persistent/transient,
**AR**=autoRearm, **DP**=displayPriority, **Gate**=Gatewirkung
(`IS`=ImmediateStop, `durch #21`=durch #21-Permission bestimmt),
**OF/IF**=OuterFan/InnerFan SafetyAction (`PM`=PlannerManaged,
`FOn`=ForceOn, `FOff`=ForceOff), **RE**=RestartEligible,
**Auth**=ResetAuthorizationPolicy, **PRRP**=PostResetRunPolicy,
**Producer**=realer Producer oder `injection-only`.

**Default (unveraendert seit M3, Vorrunde):** fuer jeden Code, der nicht
selbst ein Fanfehler oder eine untrusted-Planner-Domain-Ursache ist, gilt
`OF=PM, IF=PM` — der Aussenluefter faehrt seinen bestehenden #23-Nachlauf
normal weiter (Restwaerme), der Innenluefter bleibt unter #23-Kontrolle.

**M3-Korrektur DIESER Runde:** `Y4-006/ActuatorPlanner` ist eine Ursache,
die bedeutet, dass die Planner-Domain selbst strukturell nicht mehr
eindeutig ist — `PlannerManaged` wuerde in diesem einen Fall bedeuten, die
Fan-Sicherheitsreaktion an genau die Domain zu delegieren, deren
Vertrauenswuerdigkeit der Fault selbst infrage stellt. `Y4-006/
ActuatorPlanner` erhaelt deshalb eine deterministische, Planner-unabhaengige
Fan-Aktion: `OF=ForceOn` (Restwaermeabfuhr darf nicht von der als
strukturell unklar eingestuften Planner-Domain abhaengen —
`SAFETY_AND_FAULTS.md:175-178`), `IF=ForceOff` (keine Textgrundlage, den
Innenluefter blind ueber eine als nicht eindeutig eingestufte Domain laufen
zu lassen; da die Domain, die ihn normalerweise steuert, selbst betroffen
ist, wird er stattdessen deterministisch stillgelegt, statt ihrer Kontrolle
zu unterliegen). `Y4-007/SafetyCore` wurde in dieser Runde auf dasselbe
Muster explizit geprueft (Abschnitt 38.3.1) und bewusst **nicht** geaendert
— Begruendung dort.

**M2-Korrektur (Authorization, aus Vorrunde, unveraendert):** Alle S3 und
alle Y4 verlangen `Service`. Keine kanonische Quelle nennt eine
Operator-Ausnahme fuer einen einzelnen S3-Code
(`SAFETY_AND_FAULTS.md:98-103,211-213`).

| Code | Source | Kl. | P/T | AR | DP | Gate | OF | IF | RE | Auth | CauseClear-Oracle | PRRP | Producer |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| P1-001 | Process | P1 | T | ja | 10 | — | PM | PM | nein | keine (Auto) | vorhandene typisierte Prozesswarnung requalifiziert | AutoRearm | vorhandene Prozesswarnung, soweit #24 sie konsumiert |
| O2-001 | ProductSensor | O2 | T | ja | 20 | durch #21 | PM | PM | nein | keine (Auto) | #21-Permission wieder `Allowed`/AirFallback gueltig | AutoRearm | #20/#21 |
| O2-002 | AirSensor | O2 | T | ja | 10 | IS | PM | PM | nein | keine (Auto) | #20 `VALID` fuer AirSensor | AutoRearm | #20 |
| O2-002 | CoolingSensor | O2 | T | ja | 10 | IS | PM | PM | nein | keine (Auto) | #20 `VALID` fuer CoolingSensor | AutoRearm | #20 |
| S3-001 | AirSensor | S3 | P | nein | 30 | IS | PM | PM | ja | Service | #20 `VALID` stabil (AirSensor) | RecoverIfProvable | #20 |
| S3-001 | CoolingSensor | S3 | P | nein | 30 | IS | PM | PM | ja | Service | #20 `VALID` stabil (CoolingSensor) | RecoverIfProvable | #20 |
| S3-002 | SensorSet | S3 | P | nein | 40 | IS | PM | PM | ja | Service | #21 CrossRole/SafeLocked eindeutig aufgeloest | RecoverIfProvable | #21 |
| S3-003 | TemperatureSafety | S3 | P | nein | 20 | IS | PM | PM | ja | Service | Commissioning-Producer (#35) qualifiziert erneut | RecoverIfProvable | injection-only bis #35-Producer real |
| S3-004 | ActuatorPlanner | S3 | P | nein | 50 | IS | PM | PM | ja | Service | #23 `latchedWatchdogFault` requalifiziert (Abschnitt 36) | RecoverIfProvable | #23 |
| S3-005 | Peltier | S3 | P | nein | 10 | IS | PM | PM | ja | Service | typisierter ActuatorDiagnostic wieder `Healthy` | RecoverIfProvable | injection-only |
| S3-005 | OuterFan | S3 | P | nein | 10 | IS | PM | PM | ja | Service | typisierter ActuatorDiagnostic wieder `Healthy` | RecoverIfProvable | injection-only |
| S3-005 | InnerFan | S3 | P | nein | 10 | IS | PM | FOff | ja | Service | typisierter ActuatorDiagnostic wieder `Healthy` | RecoverIfProvable | injection-only |
| S3-006 | OuterFan | S3 | P | nein | 60 | IS | FOn | PM | ja | Service | typisierter ActuatorDiagnostic wieder `Healthy` | RecoverIfProvable | injection-only |
| S3-006 | InnerFan | S3 | P | nein | 60 | IS | PM | PM | ja | Service | typisierter ActuatorDiagnostic wieder `Healthy` | RecoverIfProvable | injection-only |
| Y4-001 | RunPersistence | Y4 | P | nein | 60 | IS | PM | PM | nein | Service | Run-Abandon-Pfad erfolgreich abgeschlossen (Abschnitt 26) | Terminal | #17/#18 |
| Y4-002 | ConfigurationRecovery | Y4 | P | nein | 50 | IS | PM | PM | nein | Service | #57 `RuntimeReady`/`FactoryInitializationCompleted`/`FactoryResetCompleted` | Terminal | #57 |
| Y4-003 | ConfigurationRuntime | Y4 | P | nein | 40 | IS | PM | PM | nein | Service | #56 `Operational` | Terminal | #56 |
| Y4-004 | RunPersistence | Y4 | P | nein | 20 | IS | PM | PM | nein | Service | erfolgreiche Redundanzreparatur/Run-Abandon (Abschnitt 21/26) | Terminal | #17 |
| Y4-004 | SafetyPersistence | Y4 | P | nein | 20 | IS | PM | PM | nein | Service | erfolgreiche SafetyState-/Marker-Redundanzreparatur | Terminal | #24-Store |
| Y4-005 | RestartSupervisor | Y4 | P | nein | 30 | IS | PM | PM | nein | Service | 30 min stabile Uptime (Abschnitt 5.2) | Terminal | #24 + ResetCause |
| Y4-006 | Process | Y4 | P | nein | 70 | IS | PM | PM | nein | Service | gesamte Process-Domain wieder eindeutig | Terminal | #14 |
| Y4-006 | SensorSet | Y4 | P | nein | 70 | IS | PM | PM | nein | Service | gesamte #21-Safety-Domain wieder eindeutig | Terminal | #21 |
| Y4-006 | Control | Y4 | P | nein | 70 | IS | PM | PM | nein | Service | #22-ControlContext wieder strukturell gueltig | Terminal | #22 |
| Y4-006 | ActuatorPlanner | Y4 | P | nein | 70 | IS | **FOn** | **FOff** | nein | Service | #23-Planner-Domain wieder eindeutig | Terminal | #23 |
| Y4-006 | RunPersistence | Y4 | P | nein | 70 | IS | PM | PM | nein | Service | #17-Domain wieder eindeutig | Terminal | #17 |
| Y4-006 | ConfigurationRuntime | Y4 | P | nein | 70 | IS | PM | PM | nein | Service | #56-Domain wieder eindeutig | Terminal | #56 |
| Y4-006 | ConfigurationRecovery | Y4 | P | nein | 70 | IS | PM | PM | nein | Service | #57-Domain wieder eindeutig | Terminal | #57 |
| Y4-006 | SafetyPersistence | Y4 | P | nein | 70 | IS | PM | PM | nein | Service | #24-Store-Domain wieder eindeutig | Terminal | #24-Store |
| Y4-006 | RestartSupervisor | Y4 | P | nein | 70 | IS | PM | PM | nein | Service | RestartSupervisor-Domain wieder eindeutig | Terminal | #24 |
| Y4-007 | SafetyCore | Y4 | P | nein | 10 | IS | PM | PM | nein | Service | interne Invariante erneut bewiesen (nur nach Codeaenderung) | Terminal | #24 selbst |

Zeilenzahl: 4 transient + 26 persistent = **30**, exakt der Katalogumfang aus
Abschnitt 12.

Fanpolicy-Aggregation bei mehreren gleichzeitig aktiven Ursachen je Fan:
`ForceOff > ForceOn > PlannerManaged` (Abschnitt 38.4).

`RestartEligible=ja` bedeutet nur, dass ein kontrollierter Restart **falls
ein aktiver Run vorhanden ist** angefordert wird (Abschnitt 29.2/29.3). Ohne
aktiven Run erfolgt bei jedem S3-Reset sofortiges `FAULT -> STANDBY` ohne
Restart.

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
```

**Kein `restartAttempted` mehr auf der Faultinstanz:** die alte Idee, einen
zweiten Restart derselben Ursache ueber ein Flag auf der (nach
erfolgreichem Reset ohnehin entfernten) Instanz zu verhindern, ist
gegenstandslos — eine entfernte Instanz kann per Definition nicht erneut
resettet werden, und ein neues Auftreten derselben `(code, source)`-Identity
nach einem Reset ist eine **neue** Instanz mit eigenem `instanceId`, die
ihren eigenen, unabhaengigen Restart-Anspruch hat. Die
„kein zweiter Restart"-Garantie lebt ausschliesslich in
`restartIntentSourceInstanceId` (Abschnitt 15.1, 23): dieser Wert wird pro
persistenter `instanceId` hoechstens einmal als RestartIntent-Quelle
verwendet, weil `instanceId`-Werte nie wiederverwendet werden.

Transiente (P1/O2) Instanz:

```text
identity
instanceId            (aus dem TRANSIENTEN, rein RAM-lokalen Zaehler)
causeCleared
firstSeenMonotonicMillis (bootlokal)
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

P1/O2 haben keinen Schritt 7/8 — sie sind nie persistiert.

## 13.3 Gleiche Ursache erneut aktiv

Wenn bereits `causeCleared == false`, bleiben instanceId, faultRevision und
(bei S3/Y4) persistierter Record unveraendert. Kein Write pro Tick.

## 13.4 Relapse

Wenn dieselbe aktive Instanz bereits `causeCleared == true` hat und der
Producer wieder Active meldet: `causeCleared = false`, `faultRevision++` (im
zustaendigen Zaehler), bei S3/Y4 persistieren/readback. Eine alte
ResetEvaluation ist dadurch stale.

## 13.5 Cause-Clear

Nur wenn der gesamte kanonische Producerzustand dieser FaultIdentity
eindeutig wieder qualifiziert ist (CauseClear-Oracle-Spalte in Abschnitt 11):
`causeCleared = true`, `faultRevision++`. S3/Y4 persistieren. Ein einzelner
behobener Untergrund darf eine noch aktive zweite Unterursache derselben
Identity nicht clearen.

## 13.6 P1/O2 Auto-Rearm

Alle P1/O2-Katalogeintraege haben `autoRearm=true` (Abschnitt 11). Beim
qualifizierten Clear: Instanz entfernen, transiente `faultRevision++`, keine
SafetyState-Persistenz.

## 13.7 Reset (nur S3/Y4)

Reset entfernt genau eine persistente S3/Y4-Instanz. Vor Commit erneut
pruefen: target instance existiert, target gehoert zur erwarteten
FaultIdentity, erwartete `faultRevision` stimmt, `causeCleared == true`,
codebezogene Safetychecks positiv, Authorization gemaess Abschnitt-11-Spalte
`Auth` positiv (ausnahmslos `Service` fuer S3/Y4), keine andere uncleared
gleich-/hoeherklassige Ursache blockiert, SafetyStorage healthy. Andere
bereits `causeCleared` Latches duerfen nacheinander resettiert werden.

```text
faultRevision++
target entfernen
```

persistieren/readback. Was danach mit einem eventuell aktiven Run passiert,
regelt Abschnitt 29 (S3 vs. Y4 unterschiedlich, inklusive der sofortigen
Y4-Run-Terminalisierung, M2).

P1/O2 haben keinen Reset-Befehl — sie clearen ausschliesslich automatisch.

## 13.8 Acknowledgement

Quittierung: ist Message-/UI-Zustand, aendert kein CauseClear, aendert
keinen Latch, aendert `faultRevision` nicht, ist nicht rebootkritisch, darf
nach Reboot wieder als nicht quittiert erscheinen.

---

# 14. Primary-/Follow-up (D1 in dieser Runde ergaenzt)

Nur innerhalb derselben Persistenzklasse (S3/Y4 <-> S3/Y4, P1/O2 <-> P1/O2).

Persistiert (nur S3/Y4): `primaryInstanceId`, `primaryCode`, `primarySource`.
`0` in allen drei Feldern gleichzeitig bedeutet kein Primary. Jede andere
Kombination, bei der nicht alle drei `0` sind, aber mindestens eines davon
`0` ist, ist eine ungueltige halbe Kombination und wird abgelehnt.

Regeln: keine Selbstreferenz; Primary muss beim Erzeugen existieren;
Code/Source muessen ein Katalogwert sein; spaeterer Primaryreset loescht
Follow-up nicht; Follow-up benoetigt eigenes Clear/Reset; Decoder darf eine
historische PrimaryInstanceId akzeptieren, deren aktive Instanz inzwischen
nicht mehr im Record liegt, sofern Code/Source gueltig und nicht
Selbstreferenz sind.

**D1-Ergaenzung dieser Runde:** eine historische `primaryInstanceId` muss
zusaetzlich `< nextInstanceId` (persistenter Zaehler, Abschnitt 15.1) sein —
analog zur bereits bestehenden Regel fuer `restartIntentSourceInstanceId`
(Abschnitt 23.2). Ein Primary muss nicht mehr aktiv sein, aber sein
`instanceId`-Wert muss plausibel jemals vom persistenten Zaehler ausgegeben
worden sein; ein Wert `>= nextInstanceId` kann nie ausgegeben worden sein
und wird als strukturell ungueltig abgelehnt. Fuer P1/O2-Primaries gilt die
analoge Regel gegen den transienten Zaehler
(`< transientNextInstanceId`, bootlokal). #19 uebernimmt spaeter die
Langzeithistorie.

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
`SafetyCounterExhausted`. Keine Wiederverwendung alter IDs — dies ist auch
die Grundlage der RestartIntent-Korrektur (Abschnitt 23):
`restartIntentSourceInstanceId` bleibt eindeutig und unverwechselbar, obwohl
die referenzierte Instanz selbst nach einem erfolgreichen Reset nicht mehr
existiert.

## 15.2 Transient (P1/O2)

```text
transientFaultRevision = 1   (bei jedem Boot neu)
transientNextInstanceId = 1  (bei jedem Boot neu)
```

Niemals persistiert. Konsumiert nie den persistenten High-Watermark. Dient
ausschliesslich der Anzeige-/UI-Eindeutigkeit innerhalb eines Boots. Ein
Ueberlauf hier ist unkritisch fuer die Persistenzintegritaet (sattes
Verhalten: neue transiente Instanzen werden bei erschoepftem transienten
Zaehler abgelehnt und als Y4-006/Process gemeldet).

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
   dem Reboot war
```

---

# 16. SafetyState-Persistenz (nur S3/Y4)

## 16.1 Record

```text
RecordTypeId = 9
Schema = 1
SafetyStorageEpoch = 1
Keys: sf0, sf1
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
| restartIntentKind | u8 |
| restartIntentSourceInstanceId | u32 |
| persistentFaultCount | u8 |

Basis: **21 Byte** (unveraendert gegenueber der Vorrunde — diese Runde
aendert kein Feld der SafetyState-Persistenz; alle C1-C3/M1/M2-Korrekturen
dieser Runde betreffen ausschliesslich den `#17`-Run-Persistence-Vertrag und
RAM-lokale Coordinatorzustaende, nicht die SafetyState-/EmergencyMarker-
Wireformate).

## 16.3 Persistenter Faultrecord

| Feld | Wire |
|---|---:|
| instanceId | u32 |
| code | u16 |
| source | u8 |
| flags (nur `causeCleared`, siehe 16.5) | u8 |
| firstSeenBootSequence | u32 |
| firstSeenMonotonicMillis | u64 |
| primaryInstanceId | u32 |
| primaryCode | u16 |
| primarySource | u8 |

Pro Fault: **27 Byte** (unveraendert).

Maximal 26 persistente Faults:

```text
payload_max = 21 + 26 * 27
            = 723 Byte

Envelope ohne UTC = 37 Byte

record_max = 760 Byte
```

Unter dem 1024-Byte-Limit (unveraendert gegenueber der Vorrunde).

## 16.4 Kein redundantes Policy-Wireformat

Nicht gespeichert werden: FaultClass, persistent/transient, autoRearm,
displayPriority, Gatewirkung, Fan-Actions, RestartEligible,
ResetAuthorizationPolicy, PostResetRunPolicy. Diese Felder folgen
ausschliesslich aus dem compile-time Katalog (Abschnitt 11).

## 16.5 Exakte Flag-Bitbelegung (F4)

```text
bit 0 = causeCleared
bits 1..7 = reserved, muessen 0 sein
```

Ein Decoder, der ein reserviertes Bit gesetzt liest, lehnt den Record ab.

## 16.6 Kanonische Persistenzreihenfolge (F4)

Persistente Faultrecords werden aufsteigend nach `instanceId` kodiert.
Encoder, Decoder und Golden-Byte-Tests muessen dieselbe Ordnung beweisen;
ein Decoder, der Records in absteigender oder unsortierter Reihenfolge
liest, lehnt den Payload ab.

## 16.7 UTC im Envelope (F4)

`StorageEnvelope::utcUnixSeconds = std::nullopt` fuer jeden
`SafetyStateRecord`- und `EmergencyMarker`-Envelope, ausnahmslos.
Begruendung: Safety-Zeit ist bootlokal-monoton; UTC wird durch #56
bereitgestellt, das im Bootreihenfolge-Vertrag (Abschnitt 25) **nach** dem
Laden von SafetyState/EmergencyMarker qualifiziert wird. Ohne UTC betraegt
der Envelope-Overhead exakt 37 Byte; mit UTC waeren es 45 Byte — dieser Fall
ist normativ ausgeschlossen.

## 16.8 Golden-Byte-Tests (F4, Pflicht)

Mindestens: leerer initialer SafetyState; genau ein S3; genau ein Y4;
Primary/Follow-up-Paar; RestartIntent `Prepared` mit `kind=S3Recovery` und
historischer `sourceInstanceId`; `safeBootRequired=true`; maximaler
persistenter Faultrecord (26 Eintraege, aufsteigend sortiert); Active
Marker; Cleared Marker. Jeder Test prueft die exakten Bytes, nicht nur
Rundtrip-Gleichheit.

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
restartIntentKind=None
restartIntentSourceInstanceId=0
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
`restartIntentState==None` genau mit `restartIntentKind==None` und
`restartIntentSourceInstanceId==0`; `Prepared` verlangt bekanntes
`restartIntentKind` (aktuell nur `S3Recovery`) und
`restartIntentSourceInstanceId != 0` **aus dem persistenten
`instanceId`-Wertebereich kleiner `nextInstanceId`** — eine aktive
Faultinstanz ist dafuer explizit **nicht** erforderlich; `Prepared` mit
`sourceInstanceId >= nextInstanceId` wird abgelehnt;
`persistentFaultCount <= 26`; jeder Record ist S3/Y4; bekannte
Code-/Source-Kombination (aus der Matrix in Abschnitt 11); eindeutige
FaultIdentity; eindeutige nonzero instanceId; nur bekannte Flag-Bits (16.5);
gueltige Primarymetadaten (inklusive der D1-Obergrenze aus Abschnitt 14);
keine Selbstreferenz; kanonische Reihenfolge (16.6); keine Restbytes; keine
unbekannten Enums.

---

# 19. SafetyState Commit

`StorageEnvelope::versionValue` ist die `SafetyStateRecordRevision`.

Verbindlich: nach der dualen Initialisierung ist die hoechste Revision 2;
jede spaetere Mutation verwendet checked `max(validRevision)+1`; der Slot
mit der aktuell niedrigeren/alten Revision ist das naechste Schreibziel;
`UINT64_MAX` wird nie umgebrochen — Revisionsexhaustion fuehrt in den
EmergencyMarker-/fail-closed-Pfad.

Normale Mutation: RAM-Kandidat bauen; alle Invarianten pruefen; Envelope
Revision checked bestimmen; alternierenden Slot waehlen; kodieren; write;
readback; Envelope + Payload exakt verifizieren; erst dann persistent
bestaetigt.

`CommitOutcomeUnknown`: readback exakt neuer Candidate -> committed;
readback eindeutig alter Candidate -> nicht committed; sonst ->
indeterminate. Kein Raten.

---

# 20. EmergencyMarker

## 20.1 Record

```text
RecordTypeId = 10
Schema = 1
SafetyStorageEpoch = 1
Keys: sem0, sem1
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
MarkerState:  0=reserviert/ungueltig, 1=Active, 2=Cleared
MarkerReason: 0=reserviert/ungueltig
              1=SafetyStateWriteFailed
              2=SafetyStateCapacityFailure
              3=SafetyStateCommitIndeterminate
              4=SafetyStateReadbackMismatch
              5=SafetyStateCorruptOrUnreadable
              6=SafetyCounterExhausted
```

Unbekannter Wert in beiden Feldern ist fail-closed (SAFE_BOOT), kein
Default.

## 20.4 MarkerSequence und Slotrotation

`StorageEnvelope::versionValue` ist die `MarkerSequence`. Sind beide
Marker-Slots `NotFound`, beginnt der erste `Active`-Marker mit Sequence 1
auf `sem0`. Jeder spaetere Markerzustand verwendet checked
`max(validSequence)+1`. Geschrieben wird bevorzugt der andere Slot als der
aktuell hoechste gueltige Marker. Jeder Write wird readback-verifiziert.
`UINT64_MAX` wird nie auf 0 umgebrochen; bei erschoepfter MarkerSequence ist
der Marker-Store dauerhaft unqualifiziert und der Boot bleibt fail-closed.
Eine `Cleared`-Sequence darf nur einen aelteren `Active`-Marker ueberstimmen,
wenn beide Markerrecords technisch/semantisch gueltig sind.

## 20.5 Fehlerpfad

Normaler SafetyState-Commitfehler: RAM `SafetyPersistenceUncertain` sofort;
Gate `ImmediateStop`; Marker `Active` auf bevorzugten Slot schreiben/
readback; falls nicht bestaetigt, genau ein zweiter Versuch auf dem anderen
Slot; danach kein Write-Loop. Scheitern beide: aktueller Boot bleibt
fail-closed, kein automatischer Restart, beim naechsten Boot sind vor jeder
Freigabe vollstaendige Producer-Requalifikation und erfolgreicher
SafetyStorage-Write/Readback Pflicht.

---

# 21. Redundanz-Recovery

Der Recoverypfad darf unbekannte Safetyhistorie nie still als "leer"
behandeln.

## 21.1 Mindestens ein semantisch gueltiger SafetyState vorhanden

Wenn genau ein SafetyState-Kandidat vollstaendig gueltig ist und die andere
Seite fehlt/defekt/unlesbar ist: Gate bleibt ImmediateStop; gueltigen
Kandidaten als konservative bekannte Basis laden; `safeBootRequired=true`
setzen; falls noch nicht aktiv, `Y4-004/SafetyPersistence` als eigenen
persistenten Latch anlegen; Fault-/Instance-Counter checked fortschreiben;
defekten/fehlenden Slot mit dieser konservativen Basis ueberschreiben/
readback; zweiten Slot mit naechster Revision auf denselben semantischen
Zustand bringen/readback; erst jetzt Redundanz als healthy markieren;
vorhandene Markerhistorie analog auf zwei technisch gueltige Slots
reparieren; erst danach einen neuen `Cleared`-Marker schreiben/readback.

Der neue Y4-004-Latch bleibt nach erfolgreicher Redundanzreparatur aktiv;
Storage-Recovery setzt nur dessen `causeCleared=true`, wenn alle Read-/
Write- und Readbackpruefungen bestanden sind; erst ein separater
geschuetzter Faultreset darf Y4-004 entfernen; SAFE_BOOT bleibt danach
zusaetzlich bis zum separaten Exit bestehen.

## 21.2 Kein semantisch gueltiger SafetyState vorhanden

Wenn SafetyState-Slots existieren/Fehler melden, aber kein einziger
semantisch gueltiger SafetyState rekonstruierbar ist: keine neue leere
Safetyhistorie erfinden; keine InstanceIds/FaultRevisionen auf 1
zuruecksetzen; Marker nicht `Cleared` setzen; `storageIntegrityQualified=
false`; SAFE_BOOT bleibt dauerhaft aktiv. Dieser Zustand benoetigt den
bereits vorgesehenen expliziten lokalen Vollreset-/UART-Recoveryweg; Issue
#24 implementiert keinen stillen Safety-History-Reset. Nur der in
Abschnitt 17.1 bewiesene vollstaendig fabrikneue Namespace darf neu
initialisiert werden.

## 21.3 Marker-only Unsicherheit

Ist der normale SafetyState vollstaendig gesund, aber der Markerpfad aktiv
oder degradiert: normaler SafetyState bleibt Basis; Y4-004/
SafetyPersistence wird darin aktiviert, falls noch nicht vorhanden;
Markerredundanz wird repariert; Marker wird `Cleared`; Y4-004 bleibt bis
CauseClear + geschuetztem Reset; SAFE_BOOT bleibt bis separatem Exit.
Marker-Recovery ist niemals Faultreset oder SAFE_BOOT-Exit.

---

# 22. Reset-Port

```cpp
enum class ResetCause : std::uint8_t {
    PowerOn, SoftwareRestart, WatchdogOrPanic, Brownout, External, Unknown,
};

enum class RestartRequestStatus : std::uint8_t { Requested, Rejected };

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

Keine Fermentationsbegriffe im Port. Testsupport:
`SimulatedResetController`. Kein ESP-IDF-Adapter in #24.

---

# 23. RestartIntent (vollstaendig vom Fault-Lifecycle entkoppelt)

## 23.1 Grundproblem einer frueheren Fassung

Eine fruehere Fassung liess `RestartIntent.Prepared` auf
`restartIntentFaultInstanceId` verweisen — eine Faultinstanz, die im
S3-Handoff (Abschnitt 29) zu diesem Zeitpunkt bereits erfolgreich resettiert
und entfernt worden war. Der eigene Validator (Abschnitt 18) haette einen
Verweis auf eine nicht mehr existierende Instanz ablehnen muessen. Diese
Fassung trennt RestartIntent und Fault-Lifecycle vollstaendig.

## 23.2 Wire- und Semantikkorrektur

```text
RestartIntentState:  0=reserviert, 1=None, 2=Prepared
RestartIntentKind:    0=reserviert/None, 1=S3Recovery
restartIntentSourceInstanceId: u32
```

`sourceInstanceId` ist eine **historische** persistente `instanceId` aus dem
S3/Y4-ID-Raum (Abschnitt 15.1) — sie dient ausschliesslich der Diagnose
("welcher Fault hat diesen Restart ausgeloest") und ist **keine**
Voraussetzung fuer eine aktive Faultinstanz. Mindestens:

```text
None:
  sourceInstanceId == 0
  kind == None

Prepared:
  sourceInstanceId != 0
  sourceInstanceId < nextInstanceId  (stammt aus dem persistenten
    ID-Raum, muss aber NICHT mehr aktiv sein)
  kind == S3Recovery                (aktuell einziger definierter Wert)
  eine aktive Faultinstanz mit dieser ID ist NICHT erforderlich
```

Ein unbekannter `RestartIntentKind`-Wert ist fail-closed (Abschnitt 18).

## 23.3 Prepare (Teil des S3-Recovery-Handoffs, Abschnitt 29.3)

Ausgeloest ausschliesslich nach erfolgreicher verifizierter Safety-
Fallback-Promotion (Abschnitt 29.3, Schritt 3) — also **nachdem** der
Run-Snapshot bereits sicher auf den unveraenderten Pre-Fault-Checkpoint
zeigt und der Coordinator bereits `SafetyRecoveryRestartPending` ist. Vor
`requestSoftwareRestart()` persistieren/readback:

```text
restartIntentState=Prepared
restartIntentKind=S3Recovery
restartIntentSourceInstanceId=<instanceId des soeben entfernten S3-Faults>
```

Kein Feld auf der (nicht mehr existierenden) Faultinstanz wird dabei
beruehrt.

## 23.4 Request Rejected

Wenn `requestSoftwareRestart() == Rejected`: genau ein Commit
`restartIntentState=None`, `restartIntentKind=None`,
`restartIntentSourceInstanceId=0`. Kein zweiter Restartversuch. Commitfehler
=> EmergencyMarker/fail-closed. Die bereits erfolgte Fallback-Promotion
(Abschnitt 29.3, Schritt 3) bleibt davon **unberuehrt** — sie ist zu diesem
Zeitpunkt bereits durable committet, und der Coordinator bleibt
`SafetyRecoveryRestartPending` (M1). Der Run wartet damit in korrekt
korrigiertem, zeitlich unverfaelschtem Zustand auf einen kuenftigen Boot
(kontrolliert nachgeholt oder unkontrolliert), oder ein Service/Benutzer
terminalisiert ihn stattdessen explizit ueber den Run-Abandon-Pfad aus
`SafetyRecoveryRestartPending` (Abschnitt 26.2). Kein automatischer Retry.

## 23.5 Request Requested

`Prepared` bleibt bis zum naechsten Boot bestehen. Es wird kein Reboot
behauptet, bevor der naechste Boot tatsaechlich beobachtet wurde
(`RestartRequestStatus::Requested` bedeutet nur „Anforderung akzeptiert").

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
`None`/`None`/`0` setzen; commit/readback. Kein Boot darf denselben
Prepared-Intent zweimal zaehlen.

## 24.4 Counter

`0,1,2,3`, saturierend, nie 4. Bei Uebergang auf 3: Y4-005 anlegen oder
bestehende Instanz aktiv halten; `safeBootRequired=true`; ein atomarer
SafetyState-Kandidat.

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
persistent/fail-closed.

**Wichtig fuer S3-Recovery:** Solange ein S3-Latch aktiv ist, erzwingt
dieser Schritt SAFE_BOOT unabhaengig davon, was der #17-Run-Snapshot
traegt. Die Sicherheit des S3-Recovery-Pfads haengt deshalb nicht davon ab,
ob die Fallback-Promotion (Abschnitt 29.3) rechtzeitig erfolgte — ein
unkontrollierter Reboot waehrend eines noch ungeloesten S3 fuehrt immer in
SAFE_BOOT.

---

# 26. Run-Abandon-Pfad (F3, C2/C3/M1 in dieser Runde vollstaendig korrigiert)

## 26.1 Zweck und Geltungsbereich

Ein autorisierter Terminalisierungspfad fuer einen Run, der nicht sicher
rekonstruierbar ist oder werden darf, aber dessen physischer
Run-Persistenzspeicher wieder eindeutig les-/schreibbar ist. Gilt fuer:

- SAFE_BOOT-Eintritt mit geladenem, noch nicht reaktiviertem Run
  (Abschnitt 5.4);
- Y4-001-Orphan-Fall: ein geladener `Current`-Run mit
  `processState.state == Fault`, aber ohne passenden aktiven S3/Y4-Latch
  (kann durch einen Crash zwischen SafetyState-Reset und vollstaendiger
  Fallback-Promotion entstehen, Abschnitt 30, Fall 7);
- **die haeufigsten realen Ladefehler** (`NotReconstructible`,
  `NotReconstructibleOrphanedState`, `UnsupportedSchema`, `ForeignEpoch`,
  `PreparedInterrupted`, `ReadFailed`, `CapacityExceeded`) — diese versetzen
  `loadAndInitialize()` **nachweislich** in `BlockedIndeterminate`
  (Abschnitt 2.1), nicht in `LoadedActiveRun`/`FallbackRecoveryPending`;
- **den Zustand `SafetyRecoveryRestartPending`** (M1, Abschnitt 5.7): ein
  Service/Benutzer kann den soeben promovierten Run auch terminalisieren,
  statt auf einen kuenftigen Reboot zu warten.

## 26.2 Drei Eintrittspfade, eine gemeinsame Kernoperation

Alle drei Pfade schreiben denselben terminalen `NoActiveRun/STANDBY`-
Kandidaten als **echten physischen Checkpoint-Record** (nicht nur als
Head-Referenz) ueber denselben bestehenden zweiphasigen #17-Write-
Mechanismus (Prepared-Head -> Slot -> Committed-Head, CAS gegen die zuletzt
gelesenen Rohbytes — Abschnitt 2.1). Es entsteht **keine zweite
Persistenzengine**.

### Pfad A — aus `LoadedActiveRun`/`FallbackRecoveryPending`/`Ready`

Der Coordinator hat den Run bereits erfolgreich (technisch) geladen; die
Entscheidung "nicht rekonstruierbar" kommt von #18 selbst oder von
SAFE_BOOT. Ablauf:

1. geladenen Run niemals raten oder rekonstruieren;
2. `candidate = clearActiveRunState(current)` bilden — ein vollstaendiger
   `NoActiveRun/STANDBY`-Snapshot, keine Teilaenderung;
3. `candidate` gegen `noActiveRunHasNoRecoveryFields()`/
   `validateRunPersistenceSnapshot()` validieren;
4. write-before-apply ueber denselben zweiphasigen Head-/Slot-Commit-
   Mechanismus (`RunPersistenceMutationKind::Recovery`) wie jeder andere
   #17-Schreibpfad — CAS gegen den zuvor gelesenen Altwert, physische
   Envelope-/CRC-Revalidierung beim naechsten Boot; **kein synchrones
   Readback** (siehe Abschnitt 2.1 — das ist eine bewusste Abweichung von
   der wortgetreuen Formulierung „Write und Readback verifizieren": ein
   synchroner Read-after-Write existiert im bestehenden `#17`-Vertrag
   nirgends und wird hier nicht als zweites, inkonsistentes
   Verifikationsmodell neu erfunden; nur die neuen #24-eigenen Stores —
   SafetyState/EmergencyMarker — verwenden synchronen Readback);
5. erst nach `Applied` gilt die technische Ursache als beseitigt
   qualifiziert.

### Pfad B — aus `BlockedIndeterminate` (der haeufigste reale Fall)

`loadAndInitialize()` ist ein Einmalvertrag und darf nicht erneut aufgerufen
werden (Abschnitt 2.1). Der Run-Abandon-Pfad fuehrt stattdessen eine eigene,
schmale **technische Requalifikation** direkt gegen den Store durch,
unabhaengig vom (moeglicherweise fehlenden oder nicht vertrauenswuerdigen)
in-memory `currentHead_`:

1. Head-Record erneut lesen und dekodieren (Envelope, Schema, Epoch, CRC) —
   dieselbe technische Pruefung, die `loadAndInitialize()` bereits beim
   ersten Versuch durchgefuehrt hat, hier aber als eigenstaendige,
   read-only Vorabpruefung. Diese Vorabpruefung selbst hat noch keinen
   physischen Seiteneffekt — ein Crash an dieser Stelle aendert nichts;
   ein erneuter Aufruf beginnt hier unveraendert neu.
2. Ist der Head weiterhin nicht dekodierbar (Envelope-/CRC-/Schema-/
   Epoch-Fehler): **fail-closed bleiben, kein Schreibversuch** (kein
   „Fake-Retry" von `loadAndInitialize()`).
3. Ist der Head technisch dekodierbar, aber weiterhin `Prepared`
   (`PreparedInterrupted`, C3 — dies ist der zuvor dauerhaft unaufloesbare
   Fall): eine autorisierte Terminalisierung genau dieser einen stecken
   gebliebenen Transaktion, **ohne** sie jemals blind zu committen und
   **ohne** jemals den davon betroffenen alten Run zu resumieren:
   1. den gelesenen `Prepared`-Head vollstaendig dekodieren und gegen
      `validPreparedHead()` strukturell validieren — ein Head, der nicht
      einmal diese eigene strukturelle Pruefung besteht, bleibt endgueltig
      fail-closed, kein Terminalisierungsversuch (Ambiguitaet gewinnt
      immer gegen einen Schreibversuch);
   2. die vom Head referenzierten physischen Slots (`target`,
      `preparedCurrent`, `preparedFallback` falls vorhanden) werden dabei
      **nicht gelesen**, um den alten Run zu rekonstruieren oder die alte
      Transaktion zu vervollstaendigen;
   3. `oldRunRevision`/`oldTransitionSequence` des stecken gebliebenen
      Prepared-Heads werden als ehrliche letzte-bekannt-gute Baseline fuer
      eine **brandneue** terminale Transaktion uebernommen:
      `newRunRevision = oldRunRevision + 1`,
      `newTransitionSequence = oldTransitionSequence + 1` — dies ist eine
      gewoehnliche, vollstaendig monotone Recovery-Transaktion, **keine**
      Ausnahme und **kein** Codec-Change (bewusster Kontrast zu C1 in
      Abschnitt 29.3: hier wird ehrlich vom letzten bekannten Stand aus
      vorwaerts geschrieben, nichts wird rueckwaerts bewegt);
   4. derselbe zweiphasige Mechanismus wie Schritt 5 unten (Prepared-Head-
      Write -> Slot-Write -> Committed-Head-Write, in dieser Reihenfolge,
      identisch zum bestehenden `writeSnapshotCore()`-Muster) baut einen
      vollstaendigen physischen `NoActiveRun/STANDBY`-Tombstone-Record auf,
      CAS gegen die in Schritt 1 gelesenen rohen Bytes des stecken
      gebliebenen Prepared-Heads als Praeimage;
   5. gelingt dies (`Applied`): weiter mit Schritt 6;
   6. scheitert dies (`WriteFailed`/`CapacityExceeded`/
      `PersistenceIndeterminate`): Coordinator bleibt `BlockedIndeterminate`,
      kein Retry-Loop;
   7. **Idempotenz bei Crash:** ein Crash waehrend dieser Terminalisierung
      selbst (nach dem Prepared-Head-Write, vor Slot- oder Committed-Head-
      Write) hinterlaesst erneut einen `Prepared`-Head — ein erneuter
      Aufruf des Run-Abandon-Pfads liest ihn in Schritt 1 erneut und
      wendet exakt denselben Mechanismus erneut an (der neue stecken
      gebliebene Head ist selbst bereits ein Terminalisierungsversuch und
      wird identisch behandelt) — kein Sonderfall noetig.
4. Ist der Head jetzt technisch dekodierbar und `Committed`: dessen
   `current`-Referenzfeld als CAS-Praeimage fuer den unten beschriebenen
   physischen Tombstone-Write (Schritt 5) verwenden.
5. **Physischer Tombstone-Write (C2 — ersetzt den vorherigen, unzureichenden
   Nur-Head-Write):**
   1. einen vollstaendigen `NoActiveRun/STANDBY`-`RunPersistenceSnapshot`
      bauen (dieselbe Konstruktion, die `clearActiveRunState()` bereits
      verwendet — Programme/Configuration bleiben unangetastet, nur die
      Run-Domain-Felder werden geleert);
   2. einen physischen Zielslot nach derselben Slot-Auswahlregel bestimmen,
      die der bestehende nicht-periodische Zweig von `writeSnapshotCore()`
      bereits verwendet (der jeweils nicht-aktuell-referenzierte Slot);
   3. Prepared-Head schreiben (`store_.writeHeadExact(preparedBytes,
      oldHeadBytes, ...)`, CAS gegen die in Schritt 1/4 gelesenen rohen
      Altbytes);
   4. den kodierten Snapshot physisch in den Zielslot schreiben
      (`store_.writeSlotExact(target, targetBytes, oldSlotBytes, ...)`);
   5. Committed-Head schreiben (`store_.writeHeadExact(committedBytes,
      preparedBytes, ...)`, `current` referenziert jetzt den soeben
      physisch geschriebenen Tombstone-Record, `fallback` geleert);
   6. es wird **niemals** versucht, den Inhalt des alten Runs zu lesen oder
      zu rekonstruieren — nur ein neuer, vollstaendiger `NoActiveRun`-
      Record wird geschrieben.
6. Nach `Applied`: Coordinatorzustand wechselt von `BlockedIndeterminate`
   (bzw. `SafetyRecoveryRestartPending`, siehe Pfad C) auf `Ready`/
   `NoActiveRun`; die technische Ursache gilt erst jetzt als beseitigt
   qualifiziert.
7. Scheitert der Tombstone-Write (`WriteFailed`/`CapacityExceeded`/
   `PersistenceIndeterminate`): Coordinator bleibt im Ausgangszustand, kein
   Retry-Loop, kein Write-Storm. Ein Crash zwischen den drei Teilwrites von
   Schritt 5 hinterlaesst denselben `PreparedInterrupted`-Zustand wie in
   Schritt 3 behandelt — ein erneuter Run-Abandon-Aufruf liest ihn erneut
   und terminalisiert ihn erneut ueber Schritt 3, idempotent.

### Pfad C — aus `SafetyRecoveryRestartPending` (M1, neu in dieser Runde)

Nach erfolgreicher Fallback-Promotion (Abschnitt 29.3) kann ein Service/
Benutzer den soeben promovierten Run auch terminalisieren, statt den Reboot
abzuwarten. Der Ablauf ist identisch zu Pfad A (der promovierte Head ist
technisch ein gewoehnlicher `Committed`-Head mit `current` auf dem
promovierten Checkpoint) — CAS-Praeimage ist der zuletzt bekannte,
promovierte `Committed`-Head. Nach `Applied` verlaesst der Coordinator
`SafetyRecoveryRestartPending` in Richtung `Ready`/`NoActiveRun`; ein
bereits vorbereiteter, aber noch nicht konsumierter `RestartIntent` bleibt
technisch bestehen, wird aber beim naechsten Boot auf einen `NoActiveRun`-
Zustand treffen und hat dann keine fachliche Wirkung mehr (Abschnitt 23.3
verlangt keine aktive Faultinstanz und keinen aktiven Run als
Voraussetzung fuer den Boot-Konsum von `Prepared`).

## 26.3 Gemeinsame Invarianten (alle drei Pfade)

Kein Factory Reset; Programme und Configuration bleiben unangetastet; der
zugehoerige Y4-Latch (z. B. Y4-001/Y4-004) bleibt bis zum geschuetzten Reset
erhalten — Run-Abandon beseitigt die technische Ursache, nicht den Latch;
SAFE_BOOT wird separat behandelt (Abschnitt 25/32) — Run-Abandon allein
verlaesst SAFE_BOOT nicht; bei erneutem Persistenzfehler bleibt das System
fail-closed (Mapping auf Y4-004/Y4-001 gemaess Abschnitt 39, identisch zu
jedem anderen #17-Schreibfehler); jeder terminale Record ist ein echter
physischer Checkpoint (C2), niemals eine reine Head-Referenz ohne
passenden Slot-Inhalt.

## 26.4 Kein normaler Recovery-Start waehrend SAFE_BOOT

Wenn SAFE_BOOT bereits feststeht: `activateLoadedRun()` wird fuer einen
normalen aktiven Run nicht aufgerufen; kein PI-/Planner-Resume; kein Run
wird automatisch weitergefuehrt. Stattdessen greift 26.1-26.3.

Kein neues Wire-Schema fuer die SafetyState-Persistenz; die einzige
Wireformat-Aenderung dieser Runde betrifft `RunPersistenceHead` (Abschnitt
29.3, C1).

---

# 27. Runtime S3/Y4 -> FAULT

Bei neuem blockierendem S3/Y4:

1. FaultCore RAM sofort aktiv;
2. Gate ImmediateStop;
3. SafetyState commit/readback;
4. wenn ProcessState bereits Fault/SafeBoot: kein zweiter Prozesswechsel;
5. sonst `CriticalFault` gegen bestehende StateMachine;
6. bei aktivem Run: `persistTransition()` write-before-apply nutzen, **mit
   dem Default-`RunPersistenceFallbackDirective{}`
   (`UseStandardFallback`)** — dies ist keine beilaeufige Wahl, sondern die
   tragende Grundlage des S3-Recovery-Handoffs (Abschnitt 29.3): die
   Standard-Fallback-Rotation bewahrt den zuletzt gueltigen Pre-Fault-
   Checkpoint unveraendert, mit seinem echten `checkpointMonotonicMillis`,
   als `fallback`-Referenz. Der aktive Run wird als `Fault` erhalten (der
   persistierte Snapshot zeigt wahrheitsgemaess `Fault`, exakt wie es der
   bestehende Schema-3-Vertrag vorsieht) — **es wird keine
   `ProcessRuntimeState` gesondert in RAM zwischengespeichert**, weil der
   physische Fallback-Slot diese Aufgabe bereits vollstaendig und
   zeitlich unverfaelscht uebernimmt;
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

`ResetFault` bleibt gemaess bestehendem #17-Vertrag nicht in
`isPersistedRunCommand()` (verifiziert: `run_persistence_contract.cpp:283-302`
gibt fuer `CommandKind::ResetFault` explizit `false` zurueck).

---

# 29. FaultReset Commitreihenfolge — S3 vs. Y4 (C1/C2/C3/M1/M2 in dieser Runde vollstaendig korrigiert)

## 29.1 Gemeinsamer Anfang und sofortige Y4-Run-Terminalisierung (M2)

```text
SafetyFaultService.evaluateReset(target)
-> #15 decideFaultReset(...)
-> SafetyFaultService.commitReset(target, expectedFaultRevision)
   -> SafetyState persist/readback (Latch entfernt, faultRevision++)
-> #15 Commandbookkeeping in RAM anwenden
-> RunCommandState Safetyprojektion synchronisieren
```

Dieser Commit enthaelt **ausschliesslich** die Faultentfernung — keine
Run-Snapshot-Aenderung.

**M2 — neu in dieser Runde, ersetzt die vorherige rein deklarative
„Y4-in-der-Historie-dominiert"-Regel durch einen echten Mechanismus:**
unmittelbar danach wird geprueft, ob `target` ein Y4 war **und** ein
aktiver Run vorhanden ist — **unabhaengig davon**, ob nach diesem Reset noch
weitere Latches (S3 oder ein anderes Y4) aktiv sind:

```text
war target ein Y4 UND aktiver Run vorhanden?
  ja: sofortige Run-Terminalisierung ueber clearActiveRunState() und
      denselben zweiphasigen Head-/Slot-Commit-Mechanismus wie jeder
      andere #17-Schreibpfad (RunPersistenceMutationKind::Recovery) —
      DIES GESCHIEHT SOFORT, NICHT ERST WENN target der letzte
      blockierende Fault war. ProcessState selbst bleibt Fault, wenn
      danach noch weitere Latches aktiv sind; nur der Run-Snapshot
      wird bereits jetzt zu NoActiveRun/STANDBY terminalisiert.
  nein: keine Sofortmassnahme hier
```

Erst danach wird geprueft, ob nach diesem Reset weiterhin blockierende
Faults aktiv sind:

```text
weiterhin blockierende Faults aktiv?
  ja: ProcessState unveraendert, Gate bleibt sicher, kein weiterer
      Schritt in diesem Commit
  nein (letzter blockierender Fault gerade entfernt): Zweig A oder B:
    Run ist bereits terminalisiert (durch dieses Y4 gerade eben, oder
      durch ein fruehreres Y4 derselben Episode) ODER war nie aktiv
      -> Zweig A (29.2)
    Run ist noch aktiv (kein Y4 war jemals in dieser Episode aktiv, der
      soeben entfernte letzte Fault war also zwingend ein S3, da jedes
      Y4 den Run bereits im eigenen Reset-Moment terminalisiert)
      -> Zweig B (29.3)
```

**Warum dies Fall 17 (Abschnitt 30) mechanisch statt nur deklarativ
schliesst:** sobald irgendein Y4 waehrend einer Fault-Episode zurueckgesetzt
wird, existiert der aktive Run ab diesem Moment schlicht nicht mehr — ein
spaeterer Reset eines verbliebenen S3 kann deshalb strukturell nie mehr auf
Zweig B (Fallback-Promotion) treffen, weil dessen Vorbedingung „aktiver Run
vorhanden" bereits durch die Y4-Terminalisierung falsch geworden ist. Es ist
keine zusaetzliche Sonderregel noetig, die „ein Y4 in der Historie" separat
nachverfolgt.

## 29.2 Zweig A: kein aktiver Run mehr zu recovern

```text
Fault -> Standby, terminal (ProcessEvent::FaultResetCompleted)
```

Der Run ist zu diesem Zeitpunkt entweder bereits terminalisiert (durch die
Y4-Sofortterminalisierung aus 29.1, zu diesem oder einem frueheren
Reset-Zeitpunkt der Episode) oder war fuer diesen Reset nie aktiv
(S3-ohne-Run). Dieser Zweig fuehrt selbst **keine** Run-Snapshot-Aenderung
mehr durch — das ist bereits in 29.1 geschehen bzw. war nicht noetig.

Wenn `ProcessState == SafeBoot`: kein Prozessuebergang, SAFE_BOOT bleibt,
separater Exit erforderlich (Abschnitt 32).

## 29.3 Zweig B: letzter Fault war S3, Run noch aktiv — verifizierte Safety-Fallback-Promotion (C1)

Vorbedingung laut 29.1: kein Y4 war in dieser Episode jemals aktiv, der
gerade entfernte letzte blockierende Fault war ein S3, der Run ist noch
aktiv.

### 29.3.1 Voraussetzung: gueltige Fallback-Referenz

Es existiert eine `fallback`-Referenz im aktuellen `RunPersistenceHead`,
gesetzt durch die Standard-Fallback-Rotation beim `CriticalFault`-Commit
(Abschnitt 27, Schritt 6). Diese Voraussetzung ist **strukturell fast immer
erfuellt**: ein aktiver Run existiert nur, wenn er zuvor gestartet wurde,
und bereits dessen erster Checkpoint (`RunStarted`) war ein aktiver
`current`-Write — die `CriticalFault`-Transition rotiert diesen (oder jeden
spaeteren periodischen/Transitions-Checkpoint) danach automatisch zu
`fallback`. Trotzdem wird defensiv geprueft, nie vorausgesetzt.

### 29.3.2 C1: die verifizierte Safety-Fallback-Promotion als eng gefasste `#17`-Wireformat-Erweiterung

Der in der Vorrunde beschriebene Mechanismus („current zeigt unveraendert
auf die bisherige fallback-Referenz, kein neuer Payload-Write") ist —
direkt am Code verifiziert (Abschnitt 2.1) — mit dem bestehenden
Prepared-Head-Wireformat **nicht darstellbar**, weil
`validPreparedHead()`/`encodeRunPersistenceHead()`/
`decodeRunPersistenceHead()` `newRunRevision >= oldRunRevision` und
`newTransitionSequence >= oldTransitionSequence` unbedingt erzwingen und
der Fallback-Checkpoint strukturell eine niedrigere `transitionSequence`
als der aktuelle Fault-Checkpoint hat. Diese Runde loest das durch eine eng
gefasste, explizit benannte Wireformat-Erweiterung, keinen Workaround mit
falschen Metadaten:

**Neues Feld** auf `RunPersistenceHead`:

```text
bool safetyFallbackPromotion  (default false)
```

**Wire-Kodierung:** ein zusaetzliches u8-Bool im Prepared-Head-Payload,
unmittelbar nach `newTransitionSequence`, geschrieben genau dann, wenn
`mutationKind == Recovery` **und** `schemaVersion >= 4`
(`kSafetyFallbackPromotionIntroducedInSchema = 4U`,
`kCurrentRunPersistenceSchema` wird in dieser Runde von 3 auf 4 angehoben).
Fuer jeden anderen `mutationKind` oder `schemaVersion < 4` wird das Byte
nicht geschrieben; das Feld dekodiert dann als `false`. `RUN_PERSISTENCE.md`
wird entsprechend aktualisiert (Abschnitt 64).

**Neue Praedikatslogik in `validPreparedHead()`** (dieser Runde,
`run_persistence_codec.cpp`; die bestehende harte Monotonie-Ablehnung wird
dafuer aus dem unbedingten Kopfblock der Funktion herausgeloest und nur noch
im `else`-Zweig unten angewendet):

```text
isSafetyFallbackPromotion :=
    head.mutationKind == Recovery AND head.safetyFallbackPromotion

wenn isSafetyFallbackPromotion:
    ablehnen, falls
      - head.commandId gesetzt ist, ODER
      - head.preparedCurrent NICHT gesetzt ist, ODER
      - head.preparedCurrent keine gueltige Referenz ist (validReference),
        ODER
      - head.preparedCurrent->variant == NoActiveRun ist (der zu
        superseedende Checkpoint muss ein aktiver Run sein, kein bereits
        terminaler Tombstone), ODER
      - head.preparedFallback GESETZT ist (disjunkt von
        validSameSlotRecovery, das preparedFallback fuer eine andere
        Bedeutung reserviert), ODER
      - head.preparedCurrent->slot == head.target.slot ist (Quelle und Ziel
        muessen verschiedene physische Slots sein), ODER
      - head.target keine gueltige Referenz ist, ODER
      - head.target->variant == NoActiveRun ist (Promotion zielt immer auf
        einen aktiven Run-Snapshot, nie auf einen Tombstone)
    -- die Monotonie-Bedingung (newRunRevision >= oldRunRevision,
    -- newTransitionSequence >= oldTransitionSequence) wird in diesem
    -- Zweig BEWUSST NICHT geprueft: old*/new* tragen die ehrlichen,
    -- caller-seitig aus den echten physischen Checkpoints gelesenen
    -- Sequenzwerte des Fault-Checkpoints (alt) und des promovierten
    -- Fallback-Checkpoints (neu) -- diese sind per Konstruktion
    -- nicht-monoton, weil der promovierte Checkpoint chronologisch
    -- AELTER ist. Es werden keine erfundenen Werte verwendet, nur die
    -- Verpflichtung zur Monotonie entfaellt fuer diesen einen, eng
    -- gepruesten Fall.
sonst (bestehendes Verhalten, unveraendert):
    ablehnen, falls newRunRevision < oldRunRevision ODER
      newTransitionSequence < oldTransitionSequence
```

`validCommittedHead()` benoetigt **keine** Aenderung: der aus der Promotion
resultierende `Committed`-Head unterscheidet sich strukturell nicht von
jedem anderen gueltigen `Committed`-Head (er hat `oldRunRevision ==
newRunRevision == oldTransitionSequence == newTransitionSequence == 0`, wie
jeder `Committed`-Head — die Vorgeschichte ist dort bereits geloescht).

**Betroffene Dateien (explizit im Dateiscope, Abschnitt 65):**
`run_persistence_contract.hpp` (neues Feld, neue Schema-Konstante),
`run_persistence_codec.cpp` (`validPreparedHead()`-Erweiterung, Encoder/
Decoder-Erweiterung fuer das neue Feld). Kein anderer Teil des `#17`-
Vertrags aendert sich.

### 29.3.3 Ablauf

```text
1. S3RecoveryHandoffPending markieren (Abschnitt 5.7 -- reine Diagnose,
   RAM-ProcessState bleibt ab hier bis zum tatsaechlichen Reboot
   durchgehend Fault; kein Zwischenschritt schreibt ihn um)

2. currentHead_.fallback vorhanden und dessen physischer Slot technisch/
   semantisch gueltig (dieselben Pruefungen wie loadAndInitialize()'s
   Fallback-Ladepfad: Envelope/Schema/Epoch/CRC/Referenzabgleich), sein
   `variant != NoActiveRun` (kein Tombstone), und seine `processState`
   entspricht einer `stateUsesRunSnapshot()`-Phase, und er gehoert zum
   selben Run/Programmsnapshot wie der aktuelle Fault-Checkpoint?
     nein: RecoverIfProvable=false -> Zweig A (29.2), kein Restart
     ja:   weiter mit Schritt 3

3. RunPersistenceCoordinator::promoteFallbackForSafetyRecovery()
   (schmale neue #17-API):
   - neuer RunPersistenceHead-Kandidat, Prepared:
       preparedCurrent := <aktuelle current-Referenz, der Fault-Checkpoint>
       target           := <bisherige fallback-Referenz, UNVERAENDERT>
       preparedFallback := nicht gesetzt
       mutationKind     := Recovery
       safetyFallbackPromotion := true   (C1)
       oldRunRevision/oldTransitionSequence := die ECHTEN, aus dem
         Fault-Checkpoint gelesenen Werte
       newRunRevision/newTransitionSequence := die ECHTEN, aus dem
         promovierten Fallback-Checkpoint gelesenen Werte
   - Committed-Head danach: current := target (der promovierte
     Checkpoint), fallback := ClearFallback
   - es wird KEIN neuer Checkpoint-Slot-Payload beschrieben, nur der Head
     zeigt um -- die Payload-Bytes des promovierten Checkpoints
     (inklusive seines echten, eingefrorenen checkpointMonotonicMillis
     und der vollstaendigen ProcessRuntimeState) bleiben exakt so, wie
     sie vor dem Fault geschrieben wurden -- schliesst C2-alt: die
     Fault-Dauer wird an keiner Stelle als normale Laufzeit gebucht, weil
     niemals ein neuer Zeitstempel auf die alte Phase geschrieben wird;
   - write-before-apply ueber denselben Prepared/Committed-Head-Mechanismus
     und dieselben CAS-Primitiven wie jeder andere #17-Head-Commit, jetzt
     unter der in 29.3.2 beschriebenen eng gefassten Codec-Ausnahme; kein
     zweiter Persistenzkern.

4. bei Applied:
     Coordinatorzustand wechselt auf SafetyRecoveryRestartPending (M1,
       Abschnitt 5.7) -- kein normaler #17-Schreibpfad mehr moeglich, bis
       Reboot oder autorisierter Run-Abandon (Pfad C, Abschnitt 26.2)
     Event RunRecoveryHandoffPrepared
     RestartIntent (kind=S3Recovery, sourceInstanceId=<soeben entfernte
       Faultinstanz, rein historisch, Abschnitt 23>) schreiben/readback
     requestSoftwareRestart()
   bei jedem anderen Ergebnis (WriteFailed / CapacityExceeded /
     PersistenceIndeterminate / PersistenceCommittedApplyFailed):
     kein Restart; RAM bleibt Fault; Coordinator bleibt NICHT
     SafetyRecoveryRestartPending; technischer Fehler wird exakt auf
     Y4-004/Y4-001 gemappt (Abschnitt 39 -- dieselben bestehenden
     Status-Werte); Gate bleibt ohnehin ImmediateStop

5. naechster Boot (kontrolliert oder unkontrolliert, macht ab Schritt 3
   Applied keinen Unterschied mehr): SAFE_BOOT-Pruefung findet keinen
   aktiven S3-Latch mehr (bereits in 29.1 entfernt) -> normaler Bootpfad ->
   loadAndInitialize() laedt current (jetzt der promovierte, zeitlich
   unveraenderte Pre-Fault-Checkpoint) technisch erfolgreich ->
   LoadedActiveRun, Status Current, mit dem ORIGINALEN
   checkpointMonotonicMillis -> vollstaendig unveraenderte #18-Hop-1-Logik
   (activateLoadedRun()) wertet den nun real verstrichenen Zeitraum
   (schliesst automatisch die gesamte Fault-Dauer PLUS das begrenzte
   Intervall zwischen Fallback-Checkpoint und Fault-Eintritt als Teil der
   Boot-zu-Boot-Luecke ein, Abschnitt 5.8/M4) mit ihren bestehenden
   Unsicherheitsregeln aus -- #24 greift ab hier nicht mehr ein
     -> sicher beweisbar: Resume
     -> nicht sicher beweisbar: #18s eigene Terminalisierung bzw.
        Run-Abandon-Pfad (Abschnitt 26)
```

Kein zweiter Recoveryalgorithmus: Schritt 5 ruft ausschliesslich
bestehenden, unveraenderten `#18`-Code auf. Der einzige neue #24-Beitrag ist
eine eng gefasste, verifizierte Referenzumbiegung unter einer eng benannten
Codec-Ausnahme (Schritt 3), das Blockieren normaler Schreibpfade
(`SafetyRecoveryRestartPending`) und das Anfordern eines Reboots — die
gesamte inhaltliche Recovery-Entscheidung bleibt bei der bestehenden,
unveraenderten `#18`-Logik.

## 29.4 Restart `Rejected` oder verzoegert

Siehe Abschnitt 23.4/23.5: die Fallback-Promotion aus Schritt 3 bleibt in
beiden Faellen bereits durable bestehen, und der Coordinator bleibt
`SafetyRecoveryRestartPending`. Ein spaeterer, auch manuell ausgeloester
Boot findet den korrigierten Run-Zustand unveraendert vor; alternativ kann
ein Service/Benutzer den Run stattdessen ueber Pfad C (Abschnitt 26.2)
terminalisieren.

---

# 30. Crash-/Powerlossmatrix (in dieser Runde auf 28 Faelle erweitert: 20 S3/allgemein + 8 Run-Abandon)

Kein Fall darf normalen Aktorbetrieb ohne bewiesene #18-Recovery erlauben.

## 30.1 Faelle 1-20 (S3/Y4/allgemein, gegenueber der Vorrunde aktualisiert)

1. **Crash vor SafetyState-Faultcommit** (Abschnitt 27, S3 raised, RAM-Gate
   bereits sofort aktiv, Schritt 3 noch nicht committet): kein Fault
   persistiert; naechster Boot sieht keinen Latch; Ursache wird beim
   naechsten Tick, falls weiterhin vorhanden, erneut erkannt und raised.
   Kein Sicherheitsproblem, da das RAM-Gate sofort wirkte.
2. **Crash nach S3-Latchcommit, vor Run-Faultcommit** (SafetyState zeigt
   S3-Latch, `persistTransition()` fuer den Run noch nicht committet):
   naechster Boot sieht Latch aktiv -> SAFE_BOOT unabhaengig vom
   #17-Snapshot-Zustand.
3. **Crash nach Run-Faultcommit** (Fault-Transition inkl. Standard-
   Fallback-Rotation persistiert): naechster Boot sieht Latch aktiv ->
   SAFE_BOOT; `#17` zeigt `current=Fault`, `fallback=`Pre-Fault-Checkpoint
   (unveraendert korrekt); das Intervall zwischen diesem Fallback-
   Checkpoint und dem tatsaechlichen Fault-Eintritt ist bereits Teil der
   spaeter von #18 bewerteten Unsicherheit (Abschnitt 5.8).
4. **Reboot waehrend ungeloestem S3** (beliebiger spaeterer Zeitpunkt):
   SAFE_BOOT durch aktiven Latch erzwungen, unabhaengig vom `#17`-Zustand.
5. **Crash nach CauseClear**, vor Reset: Latch bleibt aktiv (CauseClear
   allein setzt keinen Reset um) -> SAFE_BOOT bei Reboot wie zuvor.
6. **Crash waehrend FaultReset** (SafetyState-Reset-Commit, 29.1, in
   Bearbeitung/`CommitOutcomeUnknown`): bestehende `CommitOutcomeUnknown`-
   Behandlung (Abschnitt 19) greift — eindeutig committet, eindeutig nicht,
   oder EmergencyMarker/fail-closed bei Indeterminate. Die
   Y4-Sofortterminalisierung (M2) und jeder weitere Schritt aus 29.1
   beginnen erst, nachdem dieser Commit eindeutig bestaetigt ist.
7. **Crash nach FaultReset, vor vollstaendigem Handoff** (SafetyState zeigt
   keinen S3-Latch mehr; `#17`-`current` zeigt noch `Fault`, die
   Fallback-Promotion — 29.3 Schritt 3 — noch nicht committet): naechster
   Boot -- SAFE_BOOT wird durch den (bereits entfernten) Latch nicht mehr
   erzwungen; `loadAndInitialize()` laedt `current` (`Fault`) technisch
   erfolgreich -> `LoadedActiveRun`; `activateLoadedRun()`'s Fault-Terminal-
   Guard greift -> Run bleibt liegen; wird ueber die Y4-001-Orphan-
   Klassifikation (Abschnitt 39, "Current mit Fault ohne passenden Latch")
   erkannt und ueber den Run-Abandon-Pfad A (Abschnitt 26.2) terminalisiert.
   Kein unsicherer Resume moeglich.
8. **Crash waehrend der Fallback-Promotion selbst** (29.3 Schritt 3, in
   Bearbeitung — Prepared-Head-Write erfolgt, aber Slot- oder Committed-
   Head-Write der Promotion noch nicht): bestehende #17-Head-Fehler-
   behandlung greift (`WriteFailed`/`CapacityExceeded`/
   `PersistenceIndeterminate`/`PersistenceCommittedApplyFailed` ->
   Y4-004/Y4-001, Abschnitt 39); kein Restart wird angefordert; RAM bleibt
   `Fault`; Coordinator wird NICHT `SafetyRecoveryRestartPending`. Ein
   dabei physisch stecken gebliebener `Prepared`-Head wird beim naechsten
   Boot als `PreparedInterrupted` erkannt und ueber die C3-Terminalisierung
   (Abschnitt 26.2, Pfad B Schritt 3) abgeschlossen — niemals durch
   blindes Fortsetzen der Promotion selbst.
9. **Crash nach Handoff, vor Restartrequest** (Fallback-Promotion bereits
   `Applied`, `current` zeigt korrekt auf den unveraenderten Pre-Fault-
   Checkpoint, Coordinator bereits `SafetyRecoveryRestartPending`,
   RestartIntent noch nicht geschrieben): jeder folgende Boot laedt
   `current` erfolgreich mit dem korrekten Zeitstempel -> normale
   unveraenderte #18-Hop-1-Logik. `SafetyRecoveryRestartPending` ist reine
   RAM-Information und wird beim naechsten `loadAndInitialize()` nicht
   rekonstruiert — kein #24-Zutun mehr noetig.
10. **Restart `Rejected`**: siehe Abschnitt 23.4/29.4 — Fallback-Promotion
    bleibt bestehen; Coordinator bleibt `SafetyRecoveryRestartPending`; kein
    automatischer Retry; RAM bleibt bis zu einem kuenftigen Boot `Fault`;
    ein autorisierter Abandon aus `SafetyRecoveryRestartPending` (Pfad C)
    bleibt jederzeit moeglich.
11. **Restart `Requested`, Reset tritt nicht sofort ein**: `Requested`
    bedeutet nur „Anforderung akzeptiert" (Abschnitt 23.5); RAM bleibt
    `Fault` bis der Boot tatsaechlich beobachtet wird; Gate bleibt
    `ImmediateStop`; Coordinator bleibt `SafetyRecoveryRestartPending`.
12. **Crash waehrend RestartIntent-Commit**: bestehende Abschnitt-23-Logik
    — Commitfehler -> EmergencyMarker/fail-closed; erfolgreicher Commit vor
    Crash -> naechster Boot konsumiert `Prepared` gemaess Abschnitt 24.3.
13. **Boot mit Prepared S3Recovery Intent**: ResetCause lesen, `Prepared`
    genau einmal konsumieren (`kind`/`sourceInstanceId` statt
    Faultinstanz-Verweis), `abnormalRestartCount` aktualisieren; `current`
    ist dank der Fallback-Promotion bereits die korrekte Pre-Fault-Phase ->
    normale #18-Auswertung.
14. **Boot mit Intent, Fallback fehlt/ist korrupt** (kann nur eintreten,
    wenn zwischen Promotion-Commit und diesem Boot eine physische
    Beschaedigung des promovierten Slots eintrat, da die Promotion selbst
    vorab technisch validiert hatte): `loadAndInitialize()` erkennt den
    Fehler beim Laden von `current` (jetzt der ehemalige Fallback-Slot)
    ueber die bestehende Slot-Ladevalidierung -> `NotReconstructible`/
    `ReadFailed`/etc. -> `BlockedIndeterminate` -> Run-Abandon-Pfad B
    (Abschnitt 26.2), inklusive echtem physischem Tombstone-Write (C2).
15. **#18 Recovery beweisbar**: normaler, unveraenderter Hop-1-Resume.
16. **#18 Recovery nicht beweisbar**: #18s eigene bestehende
    Terminalisierung bzw. Run-Abandon-Pfad -> STANDBY.
17. **S3 + Y4 gleichzeitig, oder S3 nach vorherigem Y4 in derselben
    Episode** (M2, mechanisch statt nur deklarativ geschlossen): solange
    ein Y4 aktiv ist, verhindert Y4-Terminalitaet jeden Recovery-Handoff
    (29.1: „weiterhin blockierende Faults aktiv" -> kein weiterer Schritt).
    Sobald ein Y4 zurueckgesetzt wird und ein aktiver Run vorhanden ist,
    terminalisiert 29.1 diesen Run **sofort**, unabhaengig von noch
    aktiven anderen Latches. Ein spaeterer Reset eines verbleibenden S3
    trifft dadurch strukturell nie mehr auf einen aktiven Run — Zweig B
    (29.3) kann fuer diese Episode nicht mehr erreicht werden, weil seine
    Vorbedingung „aktiver Run vorhanden" bereits falsch ist.
18. **Mehrere S3 gleichzeitig**: Reset jedes einzelnen S3 entfernt genau
    dessen Latch (13.7); Fallback-Promotion (29.3) wird erst versucht, wenn
    nach dem letzten Reset kein blockierender S3/Y4 mehr aktiv ist und kein
    Y4 in dieser Episode den Run bereits terminalisiert hat.
19. **SafetyStore-Fehler waehrend Handoff** (SafetyState-Store selbst
    waehrend des Reset-Commits, 29.1, fehlerhaft): EmergencyMarker-Pfad
    (Abschnitt 20) greift wie bei jedem anderen SafetyState-Commitfehler;
    kein Handoff wird versucht, solange der Reset selbst nicht bestaetigt
    committet ist.
20. **RunStore `BlockedIndeterminate` waehrend des Handoffs** (die
    technische Vorabpruefung der Fallback-Referenz in 29.3 Schritt 2
    schlaegt mit einem Store-Fehler statt einer semantischen Ablehnung
    fehl): `RecoverIfProvable=false`, Run wird ueber den Run-Abandon-Pfad B
    terminalisiert, sobald der Store wieder technisch gesund ist; bis
    dahin bleibt der zugehoerige Y4-Latch aktiv und das Gate
    `ImmediateStop`.

## 30.2 Faelle 21-28 (Run-Abandon, neu in dieser Runde: C2/C3/M1)

21. **Crash waehrend der rein lesenden Vorabpruefung** (Pfad B, Schritt 1 —
    Head erneut lesen/dekodieren, noch kein Schreibversuch begonnen): kein
    physischer Seiteneffekt; ein erneuter Aufruf des Run-Abandon-Pfads
    beginnt Schritt 1 unveraendert erneut.
22. **Head bei der Requalifikation weiterhin nicht dekodierbar**
    (Envelope-/CRC-/Schema-/Epoch-Fehler): fail-closed, kein
    Schreibversuch (Abschnitt 26.2, Pfad B Schritt 2).
23. **`PreparedInterrupted`-Terminalisierung (C3): Crash nach dem Prepared-
    Head-Write der Terminalisierung, vor Slot- oder Committed-Head-Write**:
    naechster Boot bzw. naechster Run-Abandon-Aufruf sieht erneut einen
    (neuen) `Prepared`-Head -> derselbe C3-Mechanismus (Abschnitt 26.2,
    Pfad B Schritt 3) greift erneut, idempotent.
24. **`PreparedInterrupted`-Terminalisierung: Crash nach dem Slot-Write,
    vor dem Committed-Head-Write**: identisch zu Fall 23 — Head bleibt
    `Prepared`, naechster Versuch idempotent; der Slot wird beim erneuten
    Versuch per CAS gegen den bereits physisch geschriebenen Wert entweder
    unveraendert uebernommen oder erneut ueberschrieben.
25. **`PreparedInterrupted`-Terminalisierung vollstaendig erfolgreich**
    (Committed): Coordinator wechselt auf `Ready`/`NoActiveRun`; die
    technische Ursache gilt als beseitigt.
26. **Pfad-B-Requalifikation bei bereits `Committed`-Head (Normalfall):
    Crash waehrend des eigentlichen Tombstone-Writes** (Prepared-Head-
    Write, Slot-Write, oder Committed-Head-Write, Abschnitt 26.2 Pfad B
    Schritt 5): identisch zu den Faellen 23/24 — der naechste Versuch
    behandelt den dadurch entstandenen neuen `Prepared`-Head ueber
    denselben C3-Mechanismus, idempotent.
27. **Requalifikations- bzw. Tombstone-Write scheitert definitiv** (kein
    Crash, sondern `WriteFailed`/`CapacityExceeded`/
    `PersistenceIndeterminate` als echtes Ergebnis): Coordinator bleibt im
    Ausgangszustand (`BlockedIndeterminate` bzw.
    `SafetyRecoveryRestartPending`), kein Retry-Loop, kein Write-Storm.
28. **Run-Abandon aus `SafetyRecoveryRestartPending`** (Pfad C, M1, neuer
    dritter Eintrittspfad): nach erfolgreicher Fallback-Promotion (29.3)
    terminalisiert ein Service/Benutzer den soeben promovierten Run
    stattdessen sofort, statt den Reboot abzuwarten — derselbe
    Tombstone-Mechanismus wie Pfad A/B, CAS gegen den zuletzt bekannten,
    promovierten `Committed`-Head. Ein bereits geschriebener, aber noch
    nicht konsumierter `RestartIntent` bleibt technisch bestehen und hat
    beim naechsten Boot auf einen `NoActiveRun`-Zustand keine fachliche
    Wirkung mehr (Abschnitt 26.2, Pfad C).

---

# 31. Neue ProcessEvents

Nur: `FaultResetCompleted`, `SafeBootExitCompleted`.

## FaultResetCompleted

`Fault -> Standby`, terminal. Ausgeloest fuer Y4 und fuer S3 ohne aktiven
Run (Abschnitt 29.2). **Nicht** ausgeloest fuer S3 mit aktivem Run
(Abschnitt 29.3) — dort bleibt RAM in `Fault`, bis der tatsaechliche
kontrollierte Reboot stattfindet. Kein `RecoveryEvaluation`.

## SafeBootExitCompleted

`SafeBoot -> Standby`, nur nach erfolgreichem SAFE_BOOT-Exit (Abschnitt 32).

`STATE_MACHINE.md` wird entsprechend aktualisiert.

---

# 32. SAFE_BOOT Exit

## 32.1 Voraussetzungen

Aktueller ProcessState `SafeBoot`; `safeBootRequired == true`;
EmergencyMarker eindeutig Cleared/none und Marker-Redundanz healthy;
SafetyState-Redundanz healthy; kein aktiver S3/Y4; ConfigurationService
`Operational`; ConfigurationRecovery aktuell qualifiziert; RunPersistence
eindeutig und **NoActiveRun**; kein RunPersistence-Indeterminate; AirSensor
VALID; CoolingSensor VALID; SensorSet nicht unresolved; PlannerWatchdog
nicht gelatcht; kein Y4-Unknown; trusted Serviceauthorization true.

ProductSensor ist fuer den Rueckweg nach Standby nicht generell Pflicht.

## 32.2 Reihenfolge

Da ein evtl. geladener Run bereits beim SAFE_BOOT-Eintritt ueber den
Run-Abandon-Pfad terminalisiert wurde (Abschnitt 26):

1. alle Exitbedingungen neu pruefen;
2. `safeBootRequired=false` im SafetyState persistieren/readback;
3. erst danach `SafeBootExitCompleted` RAM-Transition;
4. `STANDBY`;
5. Gate kann erst in einem neuen normalen Safety-Evaluationsschritt
   `Allowed` werden.

Crash nach SafetyState-Commit vor RAM-Transition: Run ist bereits
`NoActiveRun`; Reboot sieht `safeBootRequired=false`; normale
Bootqualifikation fuehrt nach Standby, sofern weiterhin alle Gates positiv
sind.

---

# 33. SensorQuality #20

## O2-002 STALE

Air/Cooling STALE: `O2-002 / konkrete Source`; Gate ImmediateStop fuer
Peltier; kein ProcessState Fault; bestehende #23 Fan-Nachlaufsemantik
bleibt aktiv; autoRearm erst nach #20-qualifiziertem VALID.

## S3-001 FAILED

Air/Cooling FAILED: O2-002 derselben Source entfernen; S3-001 raisen;
Runtime -> Fault; nach stabil VALID: `causeCleared=true`; Latch bis Reset
(dann `RecoverIfProvable`, Abschnitt 29.3, falls Run aktiv war).

#24 dupliziert keine Filter-/CRC-/Recoverycounter.

---

# 34. SensorSelection #21

## O2-001

ProductSensor degradiert / validierter AirFallback. `AirFallbackActive +
permission Allowed`: O2-001 darf sichtbar bleiben; Gate darf ansonsten
Allowed werden; luftgefuehrte Regelung laeuft weiter.

## S3-002

Bei sicherheitsrelevantem `SafeLocked`/`CrossRoleEvidenceIndeterminate`:
S3-002 / SensorSet.

## Y4-006 / SensorSet

Nur fuer strukturell ungueltige/unbekannte Safety-Evidenz, nicht fuer
normale PolicyWait/UserAction. CauseClear erst, wenn der gesamte aktuelle
#21-Safetykontext wieder eindeutig qualifiziert ist.

---

# 35. TemperatureControl #22

Keine Faults aus normalen: `NeutralBand`, `Saturated`, `AirLimitReduced`,
`AirLimitBlocked`.

Mappings: `InvalidConfiguration -> Y4-006/Control`; `TimeInvalid ->
Y4-006/Control`; `RequestIdentityExhausted -> Y4-006/Control`.
`SensorUnavailable`/`InvalidSample`: Gate unresolved/blocked, konkrete
Ursache soll aus #20/#21 kommen, kein duplizierter Sensorfault.
`NoCommissioning`: `Unresolved`, kein erfundener Fault, keine
Aktorfreigabe.

---

# 36. ActuatorPlanner #23

## S3-004 Watchdog

Realer Producer: `ActuatorPlanner::state().latchedWatchdogFault` =>
S3-004 / ActuatorPlanner. `RecoverIfProvable` wie jedes S3 (Abschnitt
29.3) — die Restart-Mechanik ist ab dieser Fassung fuer alle S3 einheitlich
(verifizierte Safety-Fallback-Promotion), nicht mehr S3-004-spezifisch.

CauseClear erst nach neuer aktueller vertrauenswuerdiger
Applicationevidenz: temperaturgeregelte Phase verlassen, oder neue
strukturell gueltige #22-Evaluation im aktuellen ControlContext,
Planner/Peltier abstrakt sicher Idle. Es wird kein physisches
Sink-Acknowledgement behauptet.

Nach erfolgreichem systemweiten Reset ruft die Application einmal
`ActuatorPlanner::applyExternalWatchdogFaultReset(now)` auf — dies geschieht
**vor** der Fallback-Promotion aus Abschnitt 29.3, damit der Planner beim
naechsten Boot nicht erneut sofort denselben Watchdog ausloest.

---

# 37. Injection-only Aktordiagnose

## S3-005 – elektrischer/Ausgangsfehler

Zulaessig: Peltier, OuterFan, InnerFan. Noch kein realer Hardwareproducer.

## S3-006 – funktionaler Fanfehler

Zulaessig: OuterFan, InnerFan. Noch kein realer Tachometer-/Stromproducer.

Keine Hardwarefaehigkeit wird behauptet.

---

# 38. Fan-Safety-Directive (M3, diese Runde erneut korrigiert)

```cpp
enum class FanSafetyAction : std::uint8_t {
    PlannerManaged, ForceOn, ForceOff,
};

struct ActuatorSafetyDirective {
    ActuatorSafetyGateStatus gate;
    FanSafetyAction outerFanAction;
    FanSafetyAction innerFanAction;
};
```

## 38.1 Kanonische Grundregel

`docs/SAFETY_AND_FAULTS.md:160-161,175-178` beschreibt die generische
Reaktion bei **jedem** sicherheitsrelevanten Fehler:

> „Aussenluefter-Nachlauf beziehungsweise erforderliche Dauerbelueftung
> starten -> Innenluefter gemaess Fehlerursache behandeln" ... „Der
> Aussenluefter wird nicht pauschal zusammen mit dem Peltier abgeschaltet,
> solange Restwaerme abgefuehrt werden muss. Der Innenluefter kann je nach
> Ursache weiterlaufen, nachlaufen oder abgeschaltet werden."

Das bedeutet: **Peltier/H-Bruecke werden fail-closed sofort AUS geschaltet
(ImmediateStop), aber Fans sind ursachenspezifisch — niemals pauschal
mitabgeschaltet.**

## 38.2 Default (jeder Code, der nicht selbst ein Fanfehler oder eine untrusted-Planner-Ursache ist)

```text
OuterFan = PlannerManaged
InnerFan = PlannerManaged
```

Die bestehende #23-Nachlauflogik entscheidet normal weiter — Peltier ist
bereits ueber das Gate separat gesperrt.

## 38.3 Fanfehler-spezifische Abweichungen

| Fehler | OuterFan | InnerFan | Quelle |
|---|---|---|---|
| S3-005/Peltier | PM | PM | kein Fanbezug |
| S3-005/OuterFan (elektrisch) | **ForceOff** | PM | `SAFETY_COMPONENT_FAULTS.md:374-375`: eingeschaltet lassen, „sofern kein elektrischer Ausgangsfehler dagegen spricht" — hier spricht er dagegen |
| S3-005/InnerFan (elektrisch) | PM | **ForceOff** | dieselbe elektrische Sicherheitslogik, spiegelbildlich fuer den Innenluefter |
| S3-006/OuterFan (funktional) | **ForceOn** | PM | `SAFETY_COMPONENT_FAULTS.md:461`: „Aussenluefterfehler \| Peltier AUS, Luefterausgang soweit sicher EIN" |
| S3-006/InnerFan (funktional) | PM | PM | keine Textgrundlage fuer Forcieren in eine Richtung; „Fan-/Ausgangsfehler ... nicht blind aktivieren" spricht gegen ForceOn, keine Quelle verlangt ForceOff fuer einen rein funktionalen (nicht elektrisch gefaehrlichen) Innenluefterbefund |
| **Y4-006/ActuatorPlanner (neu in dieser Runde, M3)** | **ForceOn** | **ForceOff** | siehe 38.3.1 |

### 38.3.1 M3: Y4-006/ActuatorPlanner und die Pruefung von Y4-007/SafetyCore

`Y4-006/ActuatorPlanner` bedeutet laut Catalog-Oracle (Abschnitt 11): „die
gesamte Planner-Domain ist nicht mehr eindeutig". `PlannerManaged` heisst
technisch (Abschnitt 6.6/45): der Aussen-/Innenluefter-Ausgang folgt
unveraendert dem Tick-Ergebnis des `ActuatorPlanner` selbst. Genau diese
Domain als Quelle der Fan-Sicherheitsreaktion zu verwenden, waere zirkulaer
— der Fault besagt ausdruecklich, dass ihrem Tick-Ergebnis (auch fuer Fans)
nicht mehr vertraut werden darf. Deshalb erhaelt `Y4-006/ActuatorPlanner`
eine deterministische, vom Planner-Tick unabhaengige Fan-Aktion:

- `OuterFan = ForceOn`: die Restwaermeabfuhr (`SAFETY_AND_FAULTS.md:
  175-178`) darf nicht von einer als strukturell unklar eingestuften Domain
  abhaengen — ein fest verdrahtetes `ForceOn` unter dem bestehenden
  Aggregationsschema (Abschnitt 38.4) sichert dies unabhaengig vom Planner.
- `InnerFan = ForceOff`: es gibt keine Textgrundlage, den Innenluefter
  blind laufen zu lassen, und die Domain, die ihn normalerweise steuert,
  ist selbst betroffen — statt ihn dem Planner zu ueberlassen, wird er
  deterministisch stillgelegt (analog zur elektrischen S3-005-Logik: ein
  nicht mehr vertrauenswuerdiger Steuerpfad fuehrt zu `ForceOff`, nicht zu
  unkontrolliertem Weiterlaufen).

`Y4-007/SafetyCore` wurde auf dasselbe Muster explizit geprueft und bleibt
**bewusst** `PlannerManaged`: Y4-007 bedeutet eine Verletzung einer internen
`#24`-eigenen Invariante (z. B. SafetyEvent-Bound-Ueberschreitung,
`AlreadyInitialized` an unmoeglicher Bootstelle — Abschnitt 39/47.2), nicht
eine Aussage ueber die Korrektheit der `ActuatorPlanner`-Domain. Die
Fan-Ausgabe des Planners wird dadurch nicht unmittelbar unglaubwuerdig; die
Direktivenableitung selbst beruht auf dem unveraenderten, statischen
compile-time-Katalog (Abschnitt 11), nicht auf der verletzten Laufzeit-
Invariante. Es besteht deshalb kein struktureller Zirkelschluss wie bei
Y4-006/ActuatorPlanner, und `PM/PM` bleibt korrekt.

## 38.4 Aggregation

Bei mehreren gleichzeitig aktiven Ursachen je Fan: `ForceOff > ForceOn >
PlannerManaged` (z. B. gleichzeitig S3-006/OuterFan=ForceOn und
S3-005/OuterFan=ForceOff -> ForceOff gewinnt, da ein bestaetigter
elektrischer Befund immer Vorrang vor einer reinen Nachlaufpraeferenz hat;
ebenso gewinnt bei gleichzeitigem Y4-006/ActuatorPlanner=ForceOn und einem
hypothetischen ForceOff einer anderen aktiven Ursache am selben Fan stets
ForceOff).

Diese Aktionen sind Sollbefehle, kein physisches Feedback. Die
vollstaendige Zuordnung je Identity steht in der Matrix in Abschnitt 11
(Spalten OF/IF).

---

# 39. RunPersistence #17/#18 – exakte Mappingmatrix

## Boot / Load

| Status | #24 |
|---|---|
| NoPersistedRun | kein Fault |
| Current, normale aktive/Completed-Topologie | kein Fault; normale #18-Bewertung nur wenn kein SAFE_BOOT |
| Current mit `ProcessState::Fault` und bereits passendem aktivem #24-S3/Y4-Latch | kein zusaetzlicher Fault; der Run-Fault ist erwartete Folge |
| Current mit `ProcessState::Fault` **ohne** passenden #24-Latch | Y4-001 / RunPersistence; niemals normal resumieren; Run-Abandon-Pfad (Abschnitt 26) — dies ist der Crash-Kompositionsfall aus Abschnitt 30, Fall 7 |
| NoActiveRun | kein Fault |
| FallbackRecovered | kein Y4; Diagnoseevent |
| PreparedInterrupted | Y4-004 / RunPersistence; Run-Abandon-Pfad B mit C3-Terminalisierung (Abschnitt 26.2) loest den Zustand auf |
| NotReconstructible | Y4-001 / RunPersistence |
| NotReconstructibleOrphanedState | Y4-001 / RunPersistence |
| ReadFailed | Y4-004 / RunPersistence |
| CapacityExceeded | Y4-004 / RunPersistence |
| UnsupportedSchema | Y4-001 / RunPersistence |
| ForeignEpoch | Y4-001 / RunPersistence |
| AlreadyInitialized an unmoeglicher Bootstelle | Y4-007 / SafetyCore |

Alle diese Ladefehler (ausser dem letzten) versetzen `loadAndInitialize()`
nachweislich in `BlockedIndeterminate` (Abschnitt 2.1) — der Run-Abandon-
Pfad B (Abschnitt 26.2) ist genau fuer diesen Coordinatorzustand konzipiert,
inklusive der autorisierten `PreparedInterrupted`-Terminalisierung (C3).

## Runtime

```text
WriteFailed, CapacityExceeded, PersistenceIndeterminate, CounterOverflow
    -> Y4-004 / RunPersistence
PersistenceCommittedApplyFailed
    -> Y4-001 / RunPersistence
```

Normale erwartbare: `AlreadyProcessed`, `AlreadyPersisted`, `NotEligible`,
`NotAllowedInState`, `StaleDecision`, `Busy`, `NotDue`, `NoActiveRun` sind
nicht automatisch persistente SafetyFaults. **`NotAllowedInState` ist auch
das erwartbare Ergebnis jedes normalen Schreibversuchs
(`checkpointPeriodic()`, `persistCommand()`, `persistTransition()`,
`persistSensorSelection()`, `persistRecoveryCandidate()`), waehrend der
Coordinator `SafetyRecoveryRestartPending` ist (M1, Abschnitt 5.7) — dies
ist die beabsichtigte, bereits bestehende Guard-Wirkung, kein neuer
Fehlerpfad.**

Ein `BlockedIndeterminate`-Coordinatorstate ohne bereits gemappte Ursache
liefert Y4-006 / RunPersistence.

Alle in dieser Tabelle referenzierten Y4-Faelle: sobald der Run-Store
wieder eindeutig les-/schreibbar ist, terminalisiert der Run-Abandon-Pfad
(Abschnitt 26) den betroffenen Run; der Y4-Latch selbst bleibt bis zum
geschuetzten Reset bestehen.

---

# 40. Configuration #57

Direkt: `ConfigurationSafetyProducer::ConfigurationUnavailable`,
`ConfigurationSafetyProducer::ConfigurationIntegrityFailure` =>
Y4-002 / ConfigurationRecovery. Clear erst wenn aktuelle #57-Recovery
eindeutig wieder `RuntimeReady` bzw. erfolgreich abgeschlossen ist. Kein
Auto-Reset des Latches.

---

# 41. Configuration #56

`ConfigurationServiceMode`: `RuntimeFailure -> Y4-003/ConfigurationRuntime`;
`CommitIndeterminate -> Y4-003/ConfigurationRuntime`; `Operational ->`
positive Clear-Qualifikation.

Zwischenzustaende (`NoRuntime`, `RecoveryPreparing`, `CommitInProgress`,
`ResetPreparing`, `ResetEligibleNoRuntime`, `EpochResetting`,
`BootstrapFinalizationPending`) => Gate `Unresolved`, solange sie fachlich
erwartbar sind; sie erzeugen nicht pro Tick neue Y4-Faults. Ein
unbekannter/unmoeglicher Modewert: `Y4-006 / ConfigurationRuntime`.

---

# 42. CONFIGURATION_SAFETY_INTEGRATION_GATE

Pflichttests gegen reale Producer: `ConfigurationRuntimeFailure`,
`ConfigurationUnavailable`, `ConfigurationIntegrityFailure`, nicht
aufloesbarer `CommitOutcomeUnknown`/`CommitIndeterminate`. Jeder Fall
beweist: persistenter Y4-Latch; unmittelbare Aktorsperre; Reboot loescht
Latch nicht; Boot -> SAFE_BOOT; Ursache-Recovery alleine loescht Latch
nicht; geschuetzter Faultreset erforderlich; SAFE_BOOT bleibt danach bis
separatem Exit; aktiver Plannerpfad kann Gate nicht umgehen.

---

# 43. Y4-006 – unabhaengige Unknown-Ursachen

Feste Sources: `Process`, `SensorSet`, `Control`, `ActuatorPlanner`,
`RunPersistence`, `ConfigurationRuntime`, `ConfigurationRecovery`,
`SafetyPersistence`, `RestartSupervisor`. Jede Source ist eigene
FaultIdentity. Beispiel: `Y4-006/ConfigurationRuntime`, `Y4-006/Process`,
`Y4-006/SensorSet` sind drei Instanzen. Clear von SensorSet beruehrt die
beiden anderen nicht. Innerhalb einer Source wird erst gecleart, wenn alle
Bedingungen dieser kanonischen Domain wieder eindeutig sind. Keine
`last-origin-wins`-Semantik.

---

# 44. SafetyGate / SafetyDirective

## ImmediateStop

Mindestens: ProcessState Fault; ProcessState SafeBoot; aktiver
blockierender S3/Y4; EmergencyMarker active/unknown; SafetyStorage
unqualified; SafetyPersistence RAM latch; O2-002 Air/Cooling STALE; #21
Permission Blocked; terminaler Safety-Prozesscommit in Bearbeitung.
`S3RecoveryHandoffPending` traegt hierzu keine eigene Regel bei — er ist in
jeder seiner Auftretensphasen bereits durch `ProcessState Fault` erfasst
(Abschnitt 5.7). `SafetyRecoveryRestartPending` traegt ebenfalls keine
eigene `ImmediateStop`-Regel bei — auch dieser Zustand tritt ausschliesslich
ein, waehrend `ProcessState` noch `Fault` ist.

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

# 45. Kein Caller-supplied Allowed

Der planner-/driver-gebundene `TemperatureControlApplicationOrchestrator`
muss den zentralen `SafetyFaultService` bzw. einen daraus nicht
faelschbaren internen SafetyDirective-Provider besitzen. `tickActuatorPlan()`
akzeptiert im produktiven gebundenen Pfad **keinen** freien
`ActuatorSafetyGateInput`.

Ablauf: SafetyDirective zentral ableiten; internen Plannerinput bauen;
Planner tick; FanDirective auf Plannerresultat anwenden; SinkDriver;
PlannerWatchdog-Evidenz an Safety zurueckmelden.

Der actorfreie aktuelle ESP-IDF-Skeletonroot bleibt actorfrei. #24 baut
keine Fake-Hardware auf.

---

# 46. Trust Boundary fuer Authorization (M2 aus Vorrunde, unveraendert)

#24 implementiert keine PIN-Pruefung. Reset-/SAFE_BOOT-Evaluation erhaelt
`authorizationSatisfied` nur aus einer vertrauenswuerdigen Application-/
Auth-Grenze; passend zur Spalte `Auth` in Abschnitt 11. Alle S3- und
Y4-Codes verlangen `Service`; es existiert in Release 1 keine
Operator-Reset-Stufe fuer verriegelte Sicherheitsfehler oder schwere
Systemfehler (`SAFETY_AND_FAULTS.md:211-213`, Abschnitt 2.1). UI/Web/
Transport duerfen dieses Feld niemals als frei deserialisierbaren
Benutzerwert direkt setzen. Bis ein produktiver Auth-Producer existiert:
Serviceautorisierung im Produktpfad = `false`. Tests duerfen beide Zustaende
deterministisch injizieren. Kein Capability-/Token-Framework.

---

# 47. SafetyEvents fuer #19 (F6, Bound in dieser Runde erneut geprueft und bestaetigt)

## 47.1 Typen

```text
FaultRaised, FaultCauseCleared, FaultRelapsed, FaultReset,
SafeBootEntered, SafeBootExited,
RestartPrepared, RestartObserved, RestartRejected,
RunRecoveryHandoffPrepared,
SafetyPersistenceFailed, EmergencyMarkerSet, EmergencyMarkerCleared,
RunTerminatedByFaultReset, RunTerminatedBySafeBoot
```

`SafetyEvent` enthaelt mindestens: kind; optional code/source/instanceId;
optional primary; bootSequence; monotonicMillis; faultRevision (aus dem
jeweils zustaendigen Zaehler, Abschnitt 15). Keine neuen Eventtypen in
dieser Runde: die Y4-Sofortterminalisierung (M2) verwendet den bestehenden
`RunTerminatedByFaultReset`; die `PreparedInterrupted`-Terminalisierung und
der Tombstone-Write in Pfad B/C (C2/C3/M1) verwenden ebenfalls
`RunTerminatedByFaultReset` bzw. `RunTerminatedBySafeBoot`, je nach
ausloesendem Kontext, identisch zu jedem anderen Run-Abandon.

## 47.2 Bound-Beweis

Jede atomare `SafetyMutation` entspricht genau einem Aufruf von `raise()`,
`clear()`, `resetFault()` (nur die SafetyState-Faultentfernung, Abschnitt
29.1 — **nicht** gebuendelt mit der Fallback-Promotion oder der
Y4-Sofortterminalisierung, die jeweils eigenstaendige #17-Commits sind),
der Fallback-Promotion selbst (29.3 Schritt 3), der Y4-Sofort-
terminalisierung selbst (29.1, M2), dem Boot-SAFE_BOOT-Uebergangskandidaten
(24.4), oder einer einzelnen Marker-/Redundanzoperation. **Normativ:**
Boot-Evaluation ruft `raise()` fuer jeden Producer sequenziell einzeln
auf — niemals batched.

| Mutation | Ereignisse | Anzahl |
|---|---|---|
| `raise()` neu | `FaultRaised` | 1 |
| `raise()` Relapse | `FaultRelapsed` | 1 |
| `clear()` | `FaultCauseCleared` | 1 |
| `resetFault()` (SafetyState-Commit, jede Klasse) | `FaultReset` | 1 |
| Y4-Sofortterminalisierung (29.1, M2, eigenstaendiger Folgeschritt desselben Reset-Commits) | `RunTerminatedByFaultReset` | 1 |
| Terminal-Run-Cleanup (29.2, ohne bereits erfolgte M2-Terminalisierung) | `RunTerminatedByFaultReset` | 1 |
| Fallback-Promotion (29.3 Schritt 3) | `RunRecoveryHandoffPrepared` | 1 |
| RestartIntent Prepare | `RestartPrepared` | 1 |
| RestartIntent Rejected | `RestartRejected` | 1 |
| Boot: RestartIntent konsumiert, kein Episodeende | `RestartObserved` | 1 |
| **Boot: RestartIntent konsumiert + Uebergang auf 3 (24.4)** | `RestartObserved` + `FaultRaised`(Y4-005) + `SafeBootEntered` | **3** |
| SAFE_BOOT-Eintritt ohne Restart-Eskalation | `SafeBootEntered` | 1 |
| SAFE_BOOT-Exit | `SafeBootExited` | 1 |
| Run-Abandon (Abschnitt 26, Pfad A/B/C) | `RunTerminatedByFaultReset` oder `RunTerminatedBySafeBoot` | 1 |
| Normaler SafetyState-Commitfehler -> Marker | `SafetyPersistenceFailed` + `EmergencyMarkerSet` | 2 |
| Marker-Recovery | `EmergencyMarkerCleared` | 1 |

**Bewiesenes Maximum weiterhin: 3** (Boot-Restart-Eskalation). Die
Y4-Sofortterminalisierung (M2) ist ein eigener Reset-Commit-Folgeschritt und
komponiert nicht mit der Boot-Eskalationszeile — beide koennen nicht
innerhalb derselben atomaren Mutation auftreten (die eine ist ein
Reset-Ergebnis zur Laufzeit, die andere ein Boot-Ergebnis). Gewaehlter
Bound bleibt unveraendert:

```cpp
std::array<SafetyEvent, 4>
eventCount <= 4
```

4 statt des bewiesenen Maximums 3 als bewusste Sicherheitsmarge. Eine
Bound-Verletzung ist ein interner Vertragsfehler und wird fail-closed
behandelt (`Y4-007/SafetyCore`), nicht still trunkiert. Keine dynamische
Queue.

---

# 48. Fehlerinjektion

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

## Recovery-Handoff (erweitert)

Deterministisches Injizieren jedes `RunPersistenceResultStatus`-Werts aus
Abschnitt 29.3 Schritt 3 (`Applied`, `WriteFailed`, `CapacityExceeded`,
`PersistenceIndeterminate`, `PersistenceCommittedApplyFailed`), um die
Crashmatrix aus Abschnitt 30.1 vollstaendig abzudecken; zusaetzlich:
`fallback`-Referenz fehlt, `fallback` zeigt auf `NoActiveRun`-Tombstone,
`fallback`-Slot physisch korrupt, `fallback` gehoert zu einem anderen Run/
Programmsnapshot (jeweils in Schritt 2 der Vorabpruefung); ein Prepared-
Head-Kandidat mit `safetyFallbackPromotion=true`, aber verletzter
Vorbedingung (Abschnitt 29.3.2) wird deterministisch von `validPreparedHead()`
abgelehnt.

## Run-Abandon (erweitert: C2/C3/M1)

Deterministisches Injizieren: Head bei der Requalifikation (Abschnitt
26.2, Pfad B, Schritt 1) weiterhin nicht dekodierbar; Head weiterhin
`Prepared` (`PreparedInterrupted`-Terminalisierung wird ausgeloest); Head
jetzt `Committed` und Requalifikations-/Tombstone-Write gelingt;
Requalifikations-/Tombstone-Write scheitert
(`WriteFailed`/`CapacityExceeded`/`PersistenceIndeterminate`) an jedem der
drei Teilschritte (Prepared-Head, Slot, Committed-Head) einzeln; Crash
zwischen den drei Teilschritten (deterministisch simuliert durch
Abbruch nach jedem einzelnen Teilwrite); Run-Abandon aus
`SafetyRecoveryRestartPending` (Pfad C) mit Applied/WriteFailed/
CapacityExceeded/PersistenceIndeterminate.

---

# 49. Testmatrix – FaultCore

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
- transiente faultRevision/instanceId Ueberlauf beeinflusst den
  persistenten Zaehler nicht (F2);
- eine entfernte (resettete) persistente instanceId wird nie
  wiederverwendet, auch nicht als `restartIntentSourceInstanceId` oder als
  `primaryInstanceId` einer spaeteren, unabhaengigen Instanz;
- **(D1, neu)** ein historischer `primaryInstanceId`-Wert `>=
  nextInstanceId` wird abgelehnt, auch wenn Code/Source sonst gueltig und
  keine Selbstreferenz vorliegt.

---

# 50. Testmatrix – Y4-006 Regression

Gleichzeitig `Y4-006/ConfigurationRuntime`, `Y4-006/Process`,
`Y4-006/SensorSet`. SensorSet qualifizieren. Erwartung: nur SensorSet
causeCleared, andere bleiben aktiv, Gate bleibt ImmediateStop. Danach jede
Domain einzeln. Innerhalb ConfigurationRuntime: zwei unresolved
Unterbedingungen simulieren, nur eine beheben, Identity darf noch nicht
causeCleared werden.

---

# 51. Testmatrix – Wireformat

## 51.1 SafetyState/EmergencyMarker (unveraendert gegenueber der Vorrunde)

- initialer Payload (21 Byte Basis);
- 26 persistente Faults, aufsteigend nach instanceId;
- flags inkl. Ablehnung gesetzter reserved bits (nur bit 0 bedeutungsvoll);
- Primary inkl. Ablehnung halb-gueltiger Kombinationen und Ablehnung eines
  Primary mit `primaryInstanceId >= nextInstanceId` (D1);
- safeBootRequired;
- RestartIntent (`None`=1/`None`=0/`0`, `Prepared`=2/`S3Recovery`=1/
  historische instanceId);
- RestartIntent mit `sourceInstanceId >= nextInstanceId` wird abgelehnt;
- RestartIntent mit `sourceInstanceId` einer bereits entfernten (nicht mehr
  aktiven) Instanz wird **akzeptiert** (das ist der Normalfall);
- RestartCounter 0..3;
- Golden Bytes (Liste aus Abschnitt 16.8);
- Roundtrip;
- max 760 Byte;
- unknown Code;
- unknown Source (Wert ausserhalb 1..17);
- unknown RestartIntentKind -> fail-closed;
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
- UTC-Feld gesetzt wird abgelehnt (Abschnitt 16.7 ist keine
  Kann-Bestimmung).

## 51.2 `RunPersistenceHead` `safetyFallbackPromotion` (C1, neu in dieser Runde)

- `safetyFallbackPromotion=false`, `mutationKind` beliebig: bestehendes,
  vollstaendig unveraendertes Monotonieverhalten (Regressionstest gegen
  jeden bestehenden `#17`-Head-Test);
- `safetyFallbackPromotion=true`, `mutationKind != Recovery`: abgelehnt;
- `safetyFallbackPromotion=true`, `mutationKind == Recovery`,
  `commandId` gesetzt: abgelehnt;
- `safetyFallbackPromotion=true`, `preparedCurrent` fehlt: abgelehnt;
- `safetyFallbackPromotion=true`, `preparedCurrent->variant ==
  NoActiveRun`: abgelehnt;
- `safetyFallbackPromotion=true`, `preparedFallback` gesetzt: abgelehnt
  (Disjunktheit zu `validSameSlotRecovery`);
- `safetyFallbackPromotion=true`, `preparedCurrent->slot ==
  target.slot`: abgelehnt;
- `safetyFallbackPromotion=true`, `target->variant == NoActiveRun`:
  abgelehnt;
- `safetyFallbackPromotion=true`, alle Vorbedingungen erfuellt,
  `newRunRevision < oldRunRevision` **und** `newTransitionSequence <
  oldTransitionSequence`: **akzeptiert** (der zentrale, beabsichtigte
  Unterschied zum Normalfall);
- `safetyFallbackPromotion=true`, alle Vorbedingungen erfuellt: Committed-
  Head danach identisch strukturell gueltig wie jeder andere Committed-Head
  (`validCommittedHead()` unveraendert);
- Encoder/Decoder-Rundtrip fuer `schemaVersion < 4`: Feld wird nicht
  geschrieben, dekodiert als `false`;
- Golden Bytes fuer einen Prepared-Head mit `safetyFallbackPromotion=true`.

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
- MarkerSequence overflow;
- unbekannter MarkerState/MarkerReason-Wert -> fail-closed (F4).

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
- Request Rejected -> Intent clear, kein Retry, Fallback-Promotion bleibt
  bestehen, Coordinator bleibt `SafetyRecoveryRestartPending` (23.4);
- Request Requested -> Intent bleibt bis Boot;
- dieselbe (historische) `sourceInstanceId` triggert nie einen zweiten
  automatischen Restart, weil sie nach Verbrauch auf `None`/`0` gesetzt
  wird und eine neue Instanz ohnehin eine neue `instanceId` erhaelt;
- Count 1;
- Count 2;
- Count 3 -> Y4-005 + safeBootRequired;
- Count saturiert 3;
- 29:59 normal stabil -> nicht clear;
- 30:00 normal stabil -> Count 0;
- Y4-005 in SAFE_BOOT: 30:00 -> nur causeCleared;
- Y4-005 Reset -> Count 0, SafeBoot bleibt (Y4 ist Terminal).

---

# 55. Verpflichtende S3-Testmatrix

## S3, Recovery beweisbar

```text
FERMENTING
-> S3
-> FAULT (Standard-Fallback-Rotation bewahrt den Pre-Fault-Checkpoint)
-> Ursache stabil wieder qualifiziert
-> causeCleared
-> geschuetzter Reset (SafetyState-Commit: Latch entfernt)
-> kein Y4 in dieser Episode aktiv/aktiv gewesen -> Run weiterhin aktiv
-> S3RecoveryHandoffPending, RAM bleibt Fault
-> Fallback verifiziert gueltig
-> verifizierte Safety-Fallback-Promotion Applied (current zeigt jetzt auf
   den unveraenderten Pre-Fault-Checkpoint, ueber die neue eng gefasste
   validPreparedHead()-Ausnahme, Abschnitt 29.3.2)
-> Coordinator SafetyRecoveryRestartPending; normale #17-Schreibpfade
   werden abgelehnt (M1) -- explizit getestet: checkpointPeriodic(),
   persistCommand(), persistTransition(), persistSensorSelection(),
   persistRecoveryCandidate() liefern alle NotAllowedInState
-> RestartIntent(S3Recovery, historische instanceId), kontrollierter Restart
-> naechster Boot: loadAndInitialize() sieht Fermenting mit dem
   ORIGINALEN checkpointMonotonicMillis (nicht Fault, kein neuer
   Zeitstempel)
-> Hop-1/Hop-2 (unveraendert): Recovery beweisbar
-> Lauf korrekt reaktiviert
```

Beweisen: kein Aktor vor vollstaendiger #18-Recovery; keine alte
Planner-/PI-Anforderung direkt wiederverwendet; keine alte Zielqualifikation
blind uebernommen; Fortschritt nur gemaess #18; aktuelle Sensor-/Safety-/
Control-Evidenz; write-before-apply in jedem Schritt; **die Fault-Dauer
selbst sowie das Intervall zwischen Fallback-Checkpoint und Fault-Eintritt
sind Teil der von #18 als Unterbrechung bewerteten Zeitluecke, nicht Teil
des als sicher beobachtet geltenden Fortschritts (Abschnitt 5.8, M4)**.

## S3, Recovery nicht beweisbar

```text
FAULT
-> Reset zulaessig (wie oben, Latch entfernt, Fallback-Promotion, Restart)
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
  Restart, keine Fallback-Pruefung;
- Reboot **waehrend** S3-FAULT, bevor ein Reset versucht wurde -> SAFE_BOOT
  (Latch noch aktiv), kein Recovery-Handoff;
- Crash nach S3-Reset, vor Fallback-Promotion -> Y4-001-Orphan-
  Klassifikation beim naechsten Boot, Run-Abandon Pfad A (Abschnitt 30,
  Fall 7);
- Crash waehrend Fallback-Promotion -> bestehende #17-Fehlerbehandlung,
  kein Restart, kein `SafetyRecoveryRestartPending` (Abschnitt 30, Fall 8);
- Crash nach Fallback-Promotion, vor Restart -> naechster (auch
  unkontrollierter) Boot rekonstruiert normal ueber #18 (Abschnitt 30, Fall
  9);
- Run-Abandon aus `SafetyRecoveryRestartPending` statt Reboot abzuwarten
  (Abschnitt 30, Fall 28);
- mehrere gleichzeitige S3 -> erst nach Reset des letzten blockierenden S3
  greift 29.2/29.3;
- S3 + Y4 gleichzeitig oder S3 nach vorherigem Y4 in derselben Episode:
  Y4-Sofortterminalisierung (M2, Abschnitt 30, Fall 17) dominiert, kein
  Recovery-Handoff mehr erreichbar, weil kein aktiver Run mehr existiert.

---

# 56. Verpflichtende Y4-Testmatrix

1. Y4 bei aktivem Lauf;
2. sofortige sichere Reaktion;
3. kein automatisches Resume;
4. Ursache behoben;
5. `causeCleared`;
6. geschuetzter Reset;
7. **(M2, neu)** Run-Terminalisierung erfolgt SOFORT bei diesem Reset, auch
   wenn danach noch weitere Latches (ein anderes Y4 oder ein S3) aktiv
   bleiben — nicht erst, wenn dieser Y4 der letzte blockierende Fault war;
8. `NoActiveRun/STANDBY` (bereits durch Schritt 7 erreicht) oder SAFE_BOOT
   bleibt bis separatem Exit, falls weitere Latches aktiv bleiben;
9. kein #18-Resume, kein Recovery-Handoff (Abschnitt 29.3 gilt nicht fuer
   Y4);
10. **(M2, neu)** ein spaeterer Reset eines nach diesem Y4 noch aktiv
    gebliebenen S3 trifft auf keinen aktiven Run mehr und fuehrt direkt in
    Zweig A (29.2), niemals in Zweig B (29.3);
11. Reboot an jeder kritischen Zwischenstelle fail-closed.

---

# 57. Zeit-/Fortschrittstests (M4-Semantik in dieser Runde praezisiert)

## Fermenting

```text
10 min normal FERMENTING (t0 = Ende dieser Phase, letzter Checkpoint)
5 min FAULT mit Peltier AUS (t1 = tatsaechlicher Fault-Eintritt)
S3 Reset -> Fallback-Promotion -> Restart -> Boot (t2) -> Recovery
```

Beweisen (Abschnitt 5.8, Variante A):

- die 5 min FAULT (`t1..t2`, inklusive Restartlatenz) werden **nicht** als
  sicher beobachteter normaler Fortschritt addiert;
- das Intervall zwischen `t0` (letzter Checkpoint vor dem Fault) und `t1`
  (tatsaechlicher Fault-Eintritt) wird **ebenfalls nicht** als sicher
  beobachteter Fortschritt behandelt — es ist Teil derselben, von #18
  bewerteten Unsicherheit wie `t1..t2`;
- das promovierte `current` traegt exakt den `checkpointMonotonicMillis`-
  Wert von `t0`, nicht von `t1` oder `t2`;
- die gesamte Luecke `t0..t2` erscheint fuer #18 als Teil der ganz normalen
  Boot-zu-Boot-Zeitluecke (identisch zu jeder anderen Unterbrechung);
- die zusaetzliche Unsicherheit gegenueber einem hypothetischen exakten
  `t1`-Zeitstempel ist durch das Checkpoint-Intervall nach oben begrenzt;
- Behandlung ausschliesslich ueber bestehende #18-Zeit-/Temperatur-/
  Unsicherheitsregeln; ohne Evidenz keine erfundene Gutschrift; #24 fuehrt
  keine eigene Zeit-/Fortschrittslogik ein.

## Weitere Phasen

- `QUALIFYING_TARGET`: alte Qualifikation nicht wiederherstellen (der
  promovierte Checkpoint traegt exakt die Vor-Fault-Qualifikation von `t0`,
  keine neue, keine Ruecksicht auf `t0..t1`);
- `WAITING_FOR_PRODUCT`: Faultzeit korrekt in #18-Wartezeitbewertung, da der
  eingefrorene Checkpoint-Zeitstempel unveraendert bleibt;
- `COOLING`, `COOL_HOLDING`, `MANUAL_HOLDING`: bestehende #18-Regeln;
- keine eigene #24-Zeitlogik in irgendeinem Fall.

---

# 58. RestartIntent-Tests

- `None` + `sourceInstanceId=0` gueltig;
- `None` + `sourceInstanceId != 0` ungueltig;
- `Prepared` + historische gueltige `sourceInstanceId` (bereits entfernte
  Instanz) — **gueltig, das ist der Normalfall**;
- `Prepared` + `sourceInstanceId=0` ungueltig;
- `Prepared` + `sourceInstanceId >= nextInstanceId` ungueltig;
- eine aktive Faultinstanz ist fuer `Prepared` **nicht** erforderlich;
- Intent genau einmal konsumiert;
- Rejected -> kein Auto-Retry;
- Rejected -> Gate bleibt geschlossen (RAM bleibt Fault), Coordinator
  bleibt `SafetyRecoveryRestartPending`;
- Requested -> Gate bleibt bis Boot geschlossen;
- unknown `RestartIntentKind` -> fail-closed;
- Counter-/Sequenceoverflow;
- ein Boot mit `Prepared`, aber der Run bereits vorher ueber
  `SafetyRecoveryRestartPending`/Pfad C terminalisiert (Abschnitt 26.2):
  Intent wird trotzdem konsumiert, trifft aber auf `NoActiveRun` und hat
  keine fachliche Wirkung mehr.

---

# 59. Run-Abandon-Tests (C2/C3/M1 in dieser Runde vollstaendig erweitert)

## 59.1 Grundlegend (aus Vorrunde, weiterhin gueltig)

- jeder relevante Ladefehler (`NotReconstructible`,
  `NotReconstructibleOrphanedState`, `UnsupportedSchema`, `ForeignEpoch`,
  `PreparedInterrupted`, `ReadFailed`, `CapacityExceeded`) fuehrt real in
  `BlockedIndeterminate` (verifiziert gegen den tatsaechlichen
  `loadAndInitialize()`-Code, nicht angenommen);
- Store spaeter technisch gesund;
- autorisierter Abandon **aus `BlockedIndeterminate` selbst** erreichbar
  (Pfad B, Abschnitt 26.2);
- autorisierter Abandon aus `LoadedActiveRun`/`FallbackRecoveryPending`/
  `Ready` erreichbar (Pfad A, Y4-001-Orphan-Fall);
- **autorisierter Abandon aus `SafetyRecoveryRestartPending` erreichbar**
  (Pfad C, M1, neu);
- kein Fake-Retry von normalem `loadAndInitialize()` (Requalifikation in
  Pfad B liest den Store direkt, ruft `loadAndInitialize()` nicht erneut);
- Programme/Configuration unveraendert;
- CAS/Store-indeterminate erneut fail-closed, kein Retry-Loop;
- Crash nach committed Abandon;
- Y4 bleibt bis Reset;
- SAFE_BOOT bleibt bis separatem Exit, falls aktiv.

## 59.2 Physischer Tombstone (C2, neu)

- der terminale `NoActiveRun`-Head referenziert nach jedem der drei Pfade
  einen physisch tatsaechlich geschriebenen Checkpoint-Record (Slot-Bytes
  vorhanden, `payloadLength`/`payloadCrc` der Referenz stimmen exakt mit
  dem physischen Record ueberein);
- ein zweiter, unabhaengiger `loadAndInitialize()`-Aufruf (in einem
  frischen Testkontext, nicht im selben Coordinator-Einmalvertrag) laedt
  den geschriebenen Tombstone erfolgreich als `NoActiveRun`, ohne erneut
  `NotReconstructible` zu liefern — dies ist der zentrale Regressionstest
  gegen den C2-Defekt der Vorrunde (reiner Head-Write ohne Slot-Inhalt);
- Reihenfolge des Writes ist Prepared-Head -> Slot -> Committed-Head,
  identisch zum bestehenden `writeSnapshotCore()`-Muster.

## 59.3 `PreparedInterrupted`-Terminalisierung (C3, neu)

- `PreparedInterrupted` aus einer normalen `#17`-Transaktion (Command/
  Transition/SensorSelection/Recovery) fuehrt zur Terminalisierung ueber
  denselben Mechanismus wie `PreparedInterrupted` aus einer unterbrochenen
  Safety-Fallback-Promotion (29.3) selbst — kein Sonderfall je nach
  Ursache der urspruenglichen Transaktion;
- ein `Prepared`-Head, der die eigene strukturelle `validPreparedHead()`-
  Pruefung nicht besteht, bleibt endgueltig fail-closed, kein
  Terminalisierungsversuch;
- die alte Transaktion wird niemals blind committet;
- der alte Run wird niemals resumiert, auch nicht teilweise;
- `newRunRevision`/`newTransitionSequence` der Terminalisierung sind exakt
  `oldRunRevision + 1`/`oldTransitionSequence + 1` des stecken gebliebenen
  Heads — kein Codec-Change, gewoehnliche Monotonie;
- CAS-Race: ein anderer Prozess veraendert die Rohbytes zwischen Lesen und
  Schreiben -> `PersistenceIndeterminate`/Ablehnung, kein blindes
  Ueberschreiben;
- Crash nach dem Prepared-Head-Write der Terminalisierung, vor Slot-Write
  -> naechster Boot bzw. naechster Aufruf sieht erneut `PreparedInterrupted`
  -> derselbe Mechanismus greift erneut (idempotent);
- Crash nach dem Slot-Write, vor dem Committed-Head-Write -> identisch
  idempotent;
- naechster Boot nach erfolgreicher Terminalisierung ist exakt
  `NoActiveRun`, niemals ein Resume des alten Runs.

## 59.4 `SafetyRecoveryRestartPending`-Blockade (M1, neu)

- `checkpointPeriodic()` liefert `NotAllowedInState`, wenn der Coordinator
  `SafetyRecoveryRestartPending` ist;
- `persistCommand()` liefert `NotAllowedInState` im selben Zustand;
- `persistTransition()` liefert `NotAllowedInState` im selben Zustand;
- `persistSensorSelection()` liefert `NotAllowedInState` im selben Zustand;
- `persistRecoveryCandidate()` liefert `NotAllowedInState` im selben
  Zustand;
- Restart `Rejected` bewahrt den promovierten Zustand exakt (keiner der
  obigen fuenf Schreibpfade hat ihn in der Zwischenzeit veraendert);
- ein spaeterer externer Boot (kontrolliert oder unkontrolliert) sieht den
  promovierten Fallback unveraendert;
- terminaler Abandon aus `SafetyRecoveryRestartPending` bleibt jederzeit
  moeglich (Pfad C, 59.1).

---

# 60. Testmatrix – FaultReset / #15

- targetFaultInstanceId 0;
- stale (persistente) faultRevision;
- cause active;
- auth false (gemaess Abschnitt-11-Spalte `Auth`, ausnahmslos `Service`
  fuer S3/Y4);
- safetychecks false;
- andere uncleared gleich-/hoeherklassige Ursache;
- andere cause-cleared Latches duerfen Zielreset nicht blockieren;
- #15 mutiert FaultCore nicht;
- #15 setzt criticalSafetyEventPending nicht selbst false;
- ResetFault bleibt non-persisted RunCommand;
- SafetyState ist Reset-Linearisierung (nur die Faultentfernung, keine
  Run-Aenderung, Abschnitt 29.1);
- Command-RAM-Bookkeeping erst nach Safetycommit;
- Projektion danach synchronisiert;
- Y4-mit-Run -> sofortige Run-Terminalisierung (M2), unabhaengig von
  verbleibenden Latches;
- Y4/S3-ohne-Run(mehr) -> sofortiges FaultResetCompleted;
- S3-mit-verbleibendem-aktivem-Run -> Fallback-Promotion-Handoff statt
  FaultResetCompleted (Abschnitt 29.3), RAM bleibt Fault.

---

# 61. Testmatrix – Fanpolicy (M3 in dieser Runde erneut korrigiert)

- Default fuer nicht-fanbezogene, nicht-Planner-untrusted Faults (inkl.
  der meisten Y4): OuterFan=PM, InnerFan=PM;
- Sensorfault (S3-001/O2-002) waehrend aktivem Peltier -> Peltier sofort
  Idle, #23-Nachlauf laeuft normal weiter (PM);
- S3-005/OuterFan (elektrisch) -> ForceOff dominiert;
- S3-005/InnerFan (elektrisch) -> ForceOff dominiert;
- S3-006/OuterFan (funktional) -> ForceOn;
- S3-006/InnerFan (funktional) -> PlannerManaged, kein Forcieren;
- **(M3, neu)** Y4-006/ActuatorPlanner -> OuterFan=ForceOn,
  InnerFan=ForceOff, unabhaengig vom aktuellen Plannertick-Ergebnis;
- **(M3, neu)** Y4-007/SafetyCore -> bleibt OuterFan=PM, InnerFan=PM
  (explizit gegengetestet: die Direktivenableitung selbst haengt nicht von
  der verletzten SafetyCore-Invariante ab);
- gleichzeitig S3-006/OuterFan (ForceOn) + S3-005/OuterFan (ForceOff) ->
  ForceOff gewinnt (Aggregation, Abschnitt 38.4);
- gleichzeitig Y4-006/ActuatorPlanner (OuterFan=ForceOn) + eine
  hypothetische gleichzeitige ForceOff-Ursache am selben Fan -> ForceOff
  gewinnt;
- kein Fault kann Peltier trotz ImmediateStop aktivieren;
- keine Fanpolicy behauptet physisches Feedback;
- Y4-002/Y4-003/Y4-004/Y4-005/Y4-007 lassen den Aussenluefter-Nachlauf
  ungestoert laufen (Restwaermeabfuhr, `SAFETY_AND_FAULTS.md:175-178`).

---

# 62. Testmatrix – Configuration Gate

Reale Producer: `ConfigurationRuntimeFailure`, `ConfigurationUnavailable`,
`ConfigurationIntegrityFailure`, `CommitIndeterminate`. Jeweils: Y4 Identity;
persistiert; gate ImmediateStop; reboot retain; SAFE_BOOT; Recovery -> nur
causeClear; Reset notwendig; SafeBootExit separat; Plannerpfad kein Bypass.

---

# 63. Testmatrix – SAFE_BOOT

- persistenter S3/Y4 beim Boot -> safeBootRequired true;
- RestartCount 3 -> safeBootRequired true;
- config Y4 -> safeBootRequired true;
- run Y4 -> safeBootRequired true;
- Markerproblem -> SAFE_BOOT;
- aktiver geladener Run wird nicht aktiviert;
- Run-Abandon fuer SAFE_BOOT (Abschnitt 26, Pfad A);
- RunStore indeterminate -> kein Exit, Run-Abandon Pfad B sobald Store
  gesund, inklusive `PreparedInterrupted`-Terminalisierung;
- Reboot verlaesst SafeBoot nicht;
- Faultreset in SafeBoot verlaesst SafeBoot nicht (weder Y4- noch
  S3-Reset — SAFE_BOOT-Exit ist immer separat);
- MarkerRecovery verlaesst SafeBoot nicht;
- Exit ohne Auth -> reject;
- Exit mit Air STALE -> reject;
- Exit mit Cooling STALE -> reject;
- Exit mit Config nicht Operational -> reject;
- Exit mit Run nicht NoActiveRun -> reject;
- alle Bedingungen -> safety flag clear -> SafeBootExitCompleted ->
  Standby;
- kein direkter Aktortest aus SafeBoot.

---

# 64. Dokumentation

Dauerhaft aktualisieren: `docs/SAFETY_AND_FAULTS.md`,
`docs/SAFETY_COMPONENT_FAULTS.md`, `docs/SYSTEM_SAFETY_AND_RECOVERY.md`,
`docs/ACCEPTANCE_TESTS.md`, `docs/RUN_COMMANDS.md`, `docs/STATE_MACHINE.md`,
`docs/RUN_PERSISTENCE.md` (neu: `safetyFallbackPromotion`-Feld, Schema-4-
Anhebung, eng gefasste `validPreparedHead()`-Ausnahme),
`docs/RECOVERY_AND_INTERRUPTION.md`, `docs/ACTUATOR_TIMING_AND_FANS.md`,
`docs/ARCHITECTURE.md`, `docs/ROADMAP.md`.

Wesentliche neue dauerhafte Aussagen: finaler Faultcodekatalog; **S3 =
`RecoverIfProvable` ueber eine verifizierte, eng gefasste Safety-Fallback-
Promotion + kontrollierten Restart in die unveraenderte #18-Logik; die
Fault-Dauer sowie das Intervall zwischen Fallback-Checkpoint und
Fault-Eintritt werden nie als beobachtete Laufzeit gebucht (Variante A);
Y4 = `Terminal`, mit sofortiger Run-Terminalisierung bei jedem Y4-Reset**;
`FAULT -> STANDBY` fuer Y4/S3-ohne-Run; `Run-Abandon-Pfad` (drei
Eintrittspfade, jeder mit echtem physischem Tombstone-Write); autorisierte
`PreparedInterrupted`-Terminalisierung; `SafetyRecoveryRestartPending`
blockiert normale Run-Schreibpfade nach erfolgreicher Promotion;
RestartIntent getrennt vom Fault-Lifecycle (historische `sourceInstanceId`);
Restart 3/30min; SafetyState/EmergencyMarker inkl. exaktem Wireformat;
MarkerRecovery != SafeBootExit; #15/#24/#17-Autoritaetsgrenze;
Fan-Safety-Directive (ursachenspezifisch, inkl. Planner-unabhaengiger
Reaktion fuer Y4-006/ActuatorPlanner); alle S3/Y4 verlangen Service-PIN;
getrennte persistente/transiente Faultzaehler; D1-Obergrenze fuer
historische Primary-IDs.

Keine PR-Reviewhistorie in kanonischen Fachdocs.

---

# 65. Voraussichtliche Dateien

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
run_persistence_coordinator.hpp/.cpp   (neu: promoteFallbackForSafetyRecovery(),
                                         abandonUnrecoverableRun() inkl.
                                         BlockedIndeterminate- und
                                         SafetyRecoveryRestartPending-
                                         Requalifikation,
                                         PreparedInterrupted-Terminalisierung;
                                         neuer Coordinatorzustand
                                         SafetyRecoveryRestartPending)
run_persistence_contract.hpp           (C1: neues Feld
                                         RunPersistenceHead::safetyFallbackPromotion,
                                         neue Schemakonstante
                                         kSafetyFallbackPromotionIntroducedInSchema=4,
                                         kCurrentRunPersistenceSchema 3 -> 4)
run_persistence_codec.cpp              (C1: validPreparedHead()-Erweiterung,
                                         Encoder-/Decoder-Erweiterung fuer
                                         das neue Feld)
temperature_control_orchestrator.hpp/.cpp
actuator_plan_types.hpp
```

Nur tatsaechlich notwendige Aenderungen. `persistRecoveryCandidate()`,
`clearActiveRunState()` und die bestehende Standard-Fallback-Rotation in
`writeSnapshotCore()` werden **wiederverwendet**, nicht in ihrer
bestehenden Semantik veraendert; `validCommittedHead()` bleibt
unveraendert (Abschnitt 29.3.2).

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

# 66. Ressourcenregeln

- FaultCore: feste `std::array`, keine unbounded Container;
- maximal 30 Runtime-Identitaeten (4 transient + 26 persistent);
- maximal 26 persistente Faultrecords;
- SafetyState Envelope <= 1024 Byte (760 Byte bei maximaler Belegung,
  unveraendert);
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

# 67. Architekturdiagramme

## 67.1 Runtime

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
                +--> #17 Persistence (persistTransition mit
                |         Standard-Fallback-Rotation,
                |         promoteFallbackForSafetyRecovery()
                |         [neue eng gefasste Codec-Ausnahme],
                |         abandonUnrecoverableRun()
                |         [physischer Tombstone-Write, drei
                |         Eintrittspfade, PreparedInterrupted-
                |         Terminalisierung])
                +--> #18 Recovery (unveraendert: loadAndInitialize,
                |         activateLoadedRun/activateFallbackRecoveredRun)
                +--> bestehende ProcessStateMachine
```

Nur eine Faultautoritaet. Nur eine Recoveryautoritaet. Nur ein
Run-Persistenzkern.

## 67.2 S3 vs. Y4 (in dieser Runde aktualisiert)

```text
S3 (RecoverIfProvable):
FAULT (Standard-Fallback-Rotation bewahrt Pre-Fault-Checkpoint unveraendert)
 -> CauseClear
 -> geschuetzter Reset (Latch entfernt)
 -> kein Y4 in dieser Episode aktiv gewesen, Run weiterhin aktiv,
    kein weiterer blockierender Fault mehr aktiv?
      nein -> siehe 29.1/29.2 (Run ggf. bereits terminalisiert oder noch
              weitere Latches aktiv)
      ja   -> S3RecoveryHandoffPending (RAM bleibt FAULT, Gate bleibt
              ImmediateStop, kein Gate-Fenster)
              -> Fallback verifizieren
              -> verifizierte Safety-Fallback-Promotion (current :=
                 fallback-Referenz, UNVERAENDERTE Payload/Zeitstempel,
                 ueber neue eng gefasste validPreparedHead()-Ausnahme)
              -> Coordinator SafetyRecoveryRestartPending (normale
                 #17-Schreibpfade blockiert)
              -> RestartIntent(S3Recovery, historische instanceId)
              -> kontrollierter Restart (oder autorisierter Abandon
                 aus SafetyRecoveryRestartPending statt Reboot)
              -> naechster Boot: UNVERAENDERTE #18-Logik, Fault-Dauer UND
                 Fallback-zu-Fault-Intervall sind Teil der normalen
                 Boot-Luecke, nicht der Laufzeit (Variante A)
                   -> Resume
                   oder
                   -> #18s eigene Terminalisierung / Run-Abandon -> STANDBY

Y4 (Terminal):
FAULT / SAFE_BOOT
 -> Ursache beheben
 -> geschuetzter Reset
 -> SOFORTIGE Run-Terminalisierung (clearActiveRunState), unabhaengig von
    noch aktiven weiteren Latches
 -> STANDBY (falls kein weiterer Latch mehr aktiv) bzw. ProcessState
    bleibt FAULT (falls noch weitere Latches aktiv, Run ist trotzdem
    bereits terminalisiert) bzw. separater SAFE_BOOT-Exit
 -> KEIN Recovery-Handoff, KEIN Restart-fuer-Recovery-Zweck
 -> ein spaeterer Reset eines verbleibenden S3 derselben Episode trifft
    strukturell auf keinen aktiven Run mehr -> immer Zweig A (29.2)
```

---

# 68. Umsetzungsslices

Jeder Slice: gezielte Tests; direkt betroffene Konsumententests;
`git diff --check`; Architekturgrenzen; Secretcheck; Ownerreview; kein
Full-Suite-Zwang waehrend Draft.

## Slice 1 – FaultCore + vollstaendiger Catalog

Typen; 16 Codes; 30 erlaubte Identities (Matrix aus Abschnitt 11
vollstaendig als compile-time Katalog, inkl. korrigierter Auth-/Fan-Spalten,
M3-Zeile Y4-006/ActuatorPlanner); getrennte persistente/transiente Zaehler
(F2); kein `restartAttempted` mehr auf der Faultinstanz; D1-Obergrenze fuer
historische Primary-IDs; Policy; DisplayPriority; CauseClear/Relapse;
Primary/Follow-up (klassenintern); Fanpolicy rein; Events rein.

## Slice 2 – SafetyState Wire/Store

RecordType 9; sf0/sf1; duale Init; exaktes Wireformat (unveraendert
gegenueber der Vorrunde: 21 Byte Basis, 760 Byte max), Flag-Bits,
Persistenzreihenfolge, No-UTC-Regel; semantic decode; CommitOutcomeUnknown;
Redundanz; Golden-Byte-Tests (16.8).

## Slice 3 – EmergencyMarker + Repair

RecordType 10; sem0/sem1; exakte MarkerState-/MarkerReason-Werte; bounded
two-slot fallback; Redundanzrecovery; MarkerRecovery; kein SafeBootExit.

## Slice 4 – RestartSupervisor

ResetPort; Simulator; bootSequence; RestartIntent (entkoppelt vom
Fault-Lifecycle, `kind`/`sourceInstanceId`); 3/30min; Y4-005.

## Slice 5 – Process-/Run-Safety inklusive S3-Recovery vs. Y4-Terminal (C1-C3, M1, M2 dieser Runde)

Runtime S3/Y4 -> Fault mit Standard-Fallback-Rotation; `FaultResetCompleted
-> Standby`; sofortige Y4-Run-Terminalisierung (M2, Abschnitt 29.1),
unabhaengig von verbleibenden Latches; `clearActiveRunState`;
**`promoteFallbackForSafetyRecovery()`** inklusive der neuen eng gefassten
`validPreparedHead()`-Ausnahme und dem neuen `safetyFallbackPromotion`-Feld
(C1, `run_persistence_contract.hpp`/`run_persistence_codec.cpp`);
**`abandonUnrecoverableRun()`** mit drei Eintrittspfaden (aus
`LoadedActiveRun`/`FallbackRecoveryPending`/`Ready`, aus
`BlockedIndeterminate` inklusive autorisierter `PreparedInterrupted`-
Terminalisierung, aus `SafetyRecoveryRestartPending`) und echtem
physischem Tombstone-Write (C2/C3); neuer Coordinatorzustand
`SafetyRecoveryRestartPending` (M1) mit vollstaendiger Blockade der
fuenf normalen `#17`-Schreibpfade; `S3RecoveryHandoffPending`-Diagnose;
`SafeBootExitCompleted`; die vollstaendige 28-Fall-Crashmatrix aus
Abschnitt 30.

## Slice 6 – reale Producer #20/#21/#22/#23

Sensor; Selection; Control; Watchdog; injection-only actuator;
FanDirective mit korrigierter, ursachenspezifischer Matrix inklusive
Y4-006/ActuatorPlanner (M3).

## Slice 7 – reale Producer #17/#18/#56/#57 + Boot

Exakte Runmappingmatrix (Abschnitt 39); Config Gate; Bootreihenfolge;
safeBootRequired true; geladenen Run nicht reaktivieren; Boot-Orchestrator,
der `loadAndInitialize()` erstmals produktiv verdrahtet.

## Slice 8 – #15 FaultReset + Actuator Bypass

targetFaultInstanceId; #15 keine SafetyMutation; ResetFault non-persisted
bestaetigen; SafetyState reset linearisieren; Orchestrator SafetyDirective
zwingend; kein caller Allowed; Service-PIN-Authorization fuer alle S3/Y4.

## Slice 9 – Dokumentation und Abschluss

Alle kanonischen Quellen (Abschnitt 64, inkl. `ACTUATOR_TIMING_AND_FANS.md`
und `RUN_PERSISTENCE.md`); Roadmap; vollstaendiges Ownerreview. Erst bei 0
Findings und Owner-Anweisung: Full Native; Builds; Ressourcenvergleich; CI.
Hardware bleibt `NOT_RUN`, solange nicht separat freigegeben/verkabelt.

---

# 69. Aufgabenliste (D2: plan-interne Punkte von Post-Commit-Metadaten getrennt)

## 69.1 Plan-interne Punkte — Wahrheit liegt in dieser committeten Datei

- [x] aktuelle Planfassung vollstaendig ersetzt
- [x] C1 (verifizierte Safety-Fallback-Promotion als eng gefasste,
      explizit benannte `#17`-Wireformat-Erweiterung statt strukturell
      unmoeglichem Head-only-Repoint) geschlossen
- [x] C2 (Run-Abandon Pfad B schreibt echten physischen
      `NoActiveRun`-Tombstone, nicht nur eine Head-Referenz) geschlossen
- [x] C3 (autorisierte `PreparedInterrupted`-Terminalisierung ohne
      Codec-Change, CAS gegen den stecken gebliebenen Prepared-Head)
      geschlossen
- [x] M1 (`SafetyRecoveryRestartPending` blockiert alle fuenf normalen
      `#17`-Schreibpfade nach erfolgreicher Promotion) geschlossen
- [x] M2 (sofortige Y4-Run-Terminalisierung bei jedem Y4-Reset, unabhaengig
      von verbleibenden Latches, ersetzt die rein deklarative
      Fall-17-Regel) geschlossen
- [x] M3 (Y4-006/ActuatorPlanner: deterministische, Planner-unabhaengige
      Fan-Aktion; Y4-007/SafetyCore geprueft und bewusst PM/PM belassen)
      geschlossen
- [x] M4 (Variante A: konservativer Rueckfall inklusive
      Fallback-zu-Fault-Intervall explizit dokumentiert, keine neuen
      Wirefelder) geschlossen
- [x] D1 (historische `primaryInstanceId` erhaelt Obergrenze
      `< nextInstanceId`) geschlossen
- [x] Ownerentscheidung S3=`RecoverIfProvable`/Y4=`Terminal` weiterhin
      vollstaendig umgesetzt
- [x] vollstaendige FaultCatalog-Matrix (F5) mit M3-Korrektur neu
      ausgegeben
- [x] SafetyEvent-Bound neu geprueft und bestaetigt (F6, weiterhin 3/4)
- [x] Crash-/Powerlossmatrix auf 28 Faelle erweitert (20 S3/allgemein + 8
      Run-Abandon)
- [x] Roadmap gegen Live-Stand geprueft
- [x] Dateiscope aktualisiert: `run_persistence_contract.hpp`/
      `run_persistence_codec.cpp` explizit als geaenderte Dateien benannt
- [x] Implementation `NOT_STARTED`
- [x] Tests/Builds `NOT_RUN`

## 69.2 Post-Commit-/PR-Metadaten — Wahrheit liegt im PR-Body/Handover, nicht in dieser Datei

Diese Schritte koennen per Definition erst **nach** dem Commit dieser
Datei ausgefuehrt werden; ihr Status wird deshalb nicht als Checkbox in der
committeten Planfassung behauptet, sondern in der PR-#108-Beschreibung und
im SESSION-HANDOVER-Kommentar dokumentiert:

1. `git diff --check`/Architektur-/Secret-Gates gegen den neuen Commit
   ausfuehren;
2. neue exakte Plan-SHA committen/pushen;
3. PR-Body mit der neuen exakten Plan-SHA aktualisieren;
4. SESSION HANDOVER mit der neuen exakten Plan-SHA veroeffentlichen;
5. auf Ownerfreigabe der exakten neuen Plan-SHA warten.

## Slice 1-9

Detailliert in Abschnitt 68; je Slice: Implementierung, gezielte Tests,
Ownerreview — noch nicht begonnen (`NOT_STARTED`).

---

# 70. Stop-/Replan-Grenzen

Neue vollstaendige Planrevision und neue Owner-SHA erforderlich bei
Aenderung an: FaultCode-/Source-Katalog; FaultIdentity; S3-Recovery- vs.
Y4-Terminal-Entscheidung; Mechanismus der Safety-Fallback-Promotion
(inklusive der `validPreparedHead()`-Ausnahme und des
`safetyFallbackPromotion`-Felds); Run-Abandon-Vertrag (alle drei Pfade,
inklusive `PreparedInterrupted`-Terminalisierung); `SafetyRecoveryRestartPending`-
Semantik; Zeitpunkt/Bedingung der Y4-Run-Terminalisierung; persistente/
transiente Trennung; RestartIntent-Semantik; Safety-Wireformat;
`RunPersistenceHead`-Wireformat/Schema; RecordType/Keys/Epoch; Restart
3/30min; EmergencyMarker-Semantik; #15/#17/Safety-Commitreihenfolge;
Fan-Safety-Directive; Reset-Authorization-Policy; Aktor-Gate-Autoritaet;
neuer Hardware-/ESP-IDF-Abhaengigkeit; neu behauptetem realen Producer.

Lokale mechanische Implementierungsdetails innerhalb dieser Vertraege
benoetigen keine Planrevision.

---

# 71. Definition of Done

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
- **S3-Reset mit aktivem Run fuehrt ueber eine verifizierte, eng gefasste
  Safety-Fallback-Promotion und einen kontrollierten Restart in die
  unveraenderte #18-Recovery, ohne die Fault-Dauer oder das
  Fallback-zu-Fault-Intervall als Laufzeit zu buchen und ohne ein
  Gate-Fenster, und resultiert je nach deren Ergebnis in Resume oder
  Terminal**;
- **normale `#17`-Schreibpfade sind waehrend `SafetyRecoveryRestartPending`
  vollstaendig blockiert**;
- **jeder Y4-Reset terminalisiert einen aktiven Run sofort, unabhaengig von
  verbleibenden Latches, ohne Restart-fuer-Recovery-Zweck**;
- 3 abnormale Boots -> SAFE_BOOT;
- 30-min-Vertrag getestet;
- SAFE_BOOT rebootfest;
- aktiver Run in SAFE_BOOT nicht reaktiviert, sondern ueber
  `abandonUnrecoverableRun()` terminalisiert;
- **Run-Abandon aus allen drei Eintrittspfaden
  (`BlockedIndeterminate`/`PreparedInterrupted`,
  `LoadedActiveRun`/`FallbackRecoveryPending`/`Ready`,
  `SafetyRecoveryRestartPending`) erreichbar, jeweils mit echtem
  physischem Tombstone-Write**;
- EmergencyMarker getrennt;
- Redundanzrepair getestet;
- MarkerRecovery != Exit;
- #17/#18/#20/#21/#22/#23/#56/#57 real konsumiert, ohne #18 zu veraendern;
- Configuration-Safety-Gate vollstaendig;
- **Fan-Safetyreaktionen ursachenspezifisch modelliert, inklusive
  Planner-unabhaengiger Reaktion fuer Y4-006/ActuatorPlanner**;
- **alle S3/Y4 verlangen Service-PIN-Autorisierung**;
- **historische Primary-IDs sind gegen den jeweiligen `nextInstanceId`
  begrenzt**;
- kein caller-supplied Allowed;
- Fehlerinjektionen reproduzierbar, inklusive Recovery-Handoff-, Run-
  Abandon- und `PreparedInterrupted`-Terminalisierungs-Fehlerpfaden;
- SafetyEvents bounded und mit bewiesenem Maximum bereitgestellt;
- kanonische Doku konsistent (inklusive `ACTUATOR_TIMING_AND_FANS.md` und
  `RUN_PERSISTENCE.md`);
- gezielte und final geforderte Tests bestanden (inklusive S3-/Y4-
  Testmatrizen, RestartIntent-Tests, Run-Abandon-Tests inklusive
  physischem Tombstone und `PreparedInterrupted`-Terminalisierung,
  Zeit-/Fortschrittstests aus Abschnitt 55-59);
- nicht ausgefuehrte Hardwaretests ehrlich `NOT_RUN`/`BLOCKED`;
- keine Hardwarewerte erfunden.

---

# 72. Plan-Gate

Nach Uebernahme dieser vollstaendigen Fassung in PR #108:

1. nur Plan/PR-Body/Handover aendern (Roadmap nur bei realem Bedarf);
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

# 73. Live-Statusabgleich dieser Planrunde

`origin/main` unveraendert bei `b8eae5f4da5f2666b5a9bda333d115254c4db5b2`
seit der vorherigen Planrunde (`f944193`). PR #108 Remote-HEAD war vor
Beginn dieser Runde exakt `f944193472bd55d40acd051c3dbe51cffe75fdf7`. Kein
Drift.

Direkt am aktuellen Quellcode (nicht aus einer Zusammenfassung) verifiziert
fuer diese Runde: `run_persistence_codec.cpp:1360-1459`
(`validPreparedHead()`, `validCommittedHead()`, die Monotonie-Erzwingung in
`encodeRunPersistenceHead()`); der bestehende `validSameSlotRecovery`-
Praezedenzfall; `run_persistence_contract.hpp:33-84`
(`RunCheckpointVariant`, `RunPersistenceMutationKind`, `RunPersistenceHead`);
`run_persistence_codec.cpp:23-25` (Schema-Konstanten); der zweiphasige
Write-Mechanismus in `run_persistence_coordinator.cpp:1617-1676`
(Reihenfolge Prepared-Head -> Slot -> Committed-Head; `writeHeadExact()`s
CAS-Praeimage ist rohbytebasiert und zustandsunabhaengig, die entscheidende
Grundlage fuer die codec-freie C3-Loesung); `run_persistence_contract.cpp`s
`makeRunPersistenceSnapshot()` (bestaetigt: `checkpointMonotonicMillis`
wird immer vom Aufrufer-Zeitparameter gesetzt, kein impliziter
Monotoniecheck in dieser Funktion selbst — relevant fuer die Verwerfung der
zwischenzeitlich erwogenen Alternative „neuer Checkpoint mit kopiertem
Alt-Zeitstempel" zugunsten der gewaehlten Codec-Ausnahme).

`docs/ROADMAP.md` wurde erneut gegen den Live-Stand geprueft und ist
weiterhin akkurat (kein hartkodierter Plan-SHA-Verweis, #22/#23 korrekt als
abgeschlossen/gemergt markiert, #24/PR #108 als aktuelle Arbeit, #19 als
naechste fachliche Arbeit) — keine Aenderung in dieser Runde.
