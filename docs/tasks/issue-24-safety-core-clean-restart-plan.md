# Issue #24 – eigenständiger Plan für Safety-Core, Verriegelung und SAFE_BOOT

Status dieser Fassung: **PLANRunde, noch nicht zur Umsetzung freigegeben**
Issue: #24 – `[E3.5] Fehlerklassen, Verriegelung, SAFE_BOOT und Fehlerinjektion`
Vorgesehener Branch: `agent/issue-24-safety-core-clean-restart`
Planpfad: `docs/tasks/issue-24-safety-core-clean-restart-plan.md`

## 1. Verbindliche Arbeitsgrenze

Diese Fassung ist ein vollständiger, eigenständiger Plan für Issue #24. Sie
setzt keine frühere Planrevision zusammen und verwendet PR #107 nicht als
Vertrags- oder Implementierungsbasis.

In dieser Runde wurden ausschließlich Bestandsaufnahme, Plan- und
Roadmap-Dokumentation sowie reine Dokumentationsprüfungen durchgeführt.

Die Umsetzung bleibt bis zur ausdrücklichen Owner-Freigabe des exakten
Plan-Commits gesperrt. Insbesondere werden bis dahin keine Produktions- oder
Testdateien, Adapter, Verträge, Builds, Hardwaretests, Ready-for-review-
Wechsel, Merges oder Issue-Schließungen vorgenommen.

### Kontextbaseline

```text
CONTEXT_BASELINE_BRANCH: agent/issue-24-safety-core-clean-restart
CONTEXT_BASELINE_SHA: b8eae5f4da5f2666b5a9bda333d115254c4db5b2
CONTEXT_HEAD_SHA: b8eae5f4da5f2666b5a9bda333d115254c4db5b2
CONTEXT_PLAN_SHA: NONE (wird nach dem Plan-Commit im Draft-PR eingetragen)
CONTEXT_REFRESH_MODE: FULL
CONTEXT_DELTA: neuer Branch direkt von live origin/main; keine Quellcodeänderung
SOURCE_OF_TRUTH_CONFLICT: NONE; offene Fachentscheidungen sind als Owner-Gates ausgewiesen
```

Der Branch wurde nach einer erneuten Live-Prüfung von `origin/main` direkt von
`b8eae5f4da5f2666b5a9bda333d115254c4db5b2` erstellt. Der Plan-Commit muss
diesen Startpunkt nachvollziehbar erhalten.

## 2. Ziel und Abgrenzung

Issue #24 erhält genau eine zentrale, deterministische Safety-/Fault-Autorität
für Fehleridentität, Lifecycle, Verriegelung, Reset, Restart-Evidence und
`SAFE_BOOT`. Diese Autorität konsumiert die bereits gemergten typisierten
Producerverträge und gibt eine schmale, immutable Safety-Gate-Projektion an den
bestehenden #23-Aktorpfad.

Das Ergebnis muss mindestens die Live-Anforderungen aus Issue #24 und den
kanonischen Verträgen erfüllen:

- vier Fehlerklassen mit stabiler Identität und höchster-Klasse-Dominanz;
- unabhängige gleichzeitige Fehler ohne Überschreiben durch eine
  `last-origin-wins`-Semantik;
- sofortige sichere Reaktion, getrennte Quittierung und autorisierter Reset;
- persistente Verriegelung über Neustarts;
- begrenzte, deterministische Restart-Evidence und explizites `SAFE_BOOT`;
- reproduzierbare Fehlerinjektion an den realen abstrakten Verträgen;
- Integration von #15, #17, #18, #20, #21, #22, #23 sowie dem #56/#57-
  Configuration-Safety-Integration-Gate;
- kein normaler produktiver Startpfad ohne den Safety-Core.

Nicht Bestandteil dieses Plans sind ESP-IDF-Resetadapter, `esp_restart()`,
GPIO- oder reale Aktor-/Sensoradapter, neue Hardwarediagnose,
Commissioninggrenzen, thermische Recoveryparameter, Service-PIN-Verifikation,
OTA, ein NVS-Adapter aus #90 oder Änderungen an anderen Issues nur zur
Bequemlichkeit von #24. Wo ein solcher Baustein für einen realen Nachweis
fehlt, bleibt der abstrakte Vertrag beziehungsweise ein Owner-Gate bestehen;
es wird kein fehlender Produktionswert erfunden.

## 3. Live-Bestandsaufnahme und Quellen

### 3.1 Live-Status

| Gegenstand | Live-Befund zum Planstart | Konsequenz |
|---|---|---|
| `origin/main` | `b8eae5f4da5f2666b5a9bda333d115254c4db5b2` | verbindliche Branchbasis |
| Issue #24 | offen, Status im Body `PLANNED_SPEC_PENDING`; Abhängigkeiten #14, #15, #17, #20, #21, #23; #56/#57 als Configuration-Safety-Integration-Gate | Issue-Vertrag und Gate-Matrix sind maßgeblich |
| PR #107 | offen, Draft, Branch `agent/issue-24-fehlerklassen-safe-boot-plan`, Head `9ba857545094dafe132f43d95e975b43bab31c38`, Base `main @ b8eae5f4...` | unangetastete, nicht normative Fehler- und Lernreferenz |
| neuer Branch | `agent/issue-24-safety-core-clean-restart` direkt von `origin/main` | einzige Implementierungsbasis für einen später freigegebenen Plan |
| `docs/ROADMAP.md` | Stand 2026-08-14; #23 aktuelle Arbeit, #24 nächste fachliche Arbeit | wird für die neue Planrunde auf den 2026-08-15-Stand synchronisiert |

PR #107 wird nicht rebased, weiterentwickelt, cherry-picked, geschlossen oder
als normative Quelle verwendet. Jede dort beobachtete Idee wurde gegen die
Live-Issue, `main`, ADRs und kanonischen Verträge neu bewertet.

### 3.2 Verbindliche Repository- und Fachquellen

| Quelle | Für diesen Plan verbindlich |
|---|---|
| `AGENTS.md`, lokale `AGENTS.md` | Branch-, Owner-, Safety-, Modul- und Stop-Regeln |
| `docs/AGENT_WORKFLOW.md` | vollständiger eigenständiger Plan, Commit-Gate, Handover |
| `docs/ENGINEERING_PRINCIPLES.md` | Repository-first, SOLID, DRY, KISS, keine Parallelverträge |
| `docs/CI_AND_QUALITY_GATES.md` | reine Planphase: keine Builds und keine vollständigen Testläufe; PASS/NOT_RUN ehrlich |
| `docs/SPECIFICATION_REVIEW.md` | Release-1-Scope, fail-closed, Persistenz-, Reboot- und SAFE_BOOT-Rahmen |
| `docs/DECISIONS.md` ADR-013, ADR-014, ADR-018 | Composition Root/Modulgrenzen, Zustandsentscheidung, Configuration-Variante B |
| `docs/SAFETY_AND_FAULTS.md` | vier Klassen, unabhängige Fehler, Lifecycle, Reset und Dominanz |
| `docs/SAFETY_COMPONENT_FAULTS.md` | Sensor-, Aktor- und thermische Reaktionsmatrix; offene Commissioningwerte bleiben offen |
| `docs/SYSTEM_SAFETY_AND_RECOVERY.md` | Bootreihenfolge, kritische Persistenz, Restartbegrenzung, SAFE_BOOT, Resetnachweis |
| `docs/ACCEPTANCE_TESTS.md` | dauerhafte Testorakel; keine PR-Historie wird dort ergänzt |
| `docs/CONFIGURATION_PERSISTENCE.md` | `IStateStore`, Envelope, `CommitOutcomeUnknown`, #56/#57-Verträge |
| `docs/IMPLEMENTATION_ISSUES.md` | E3-Abhängigkeitsgraph und #56/#57-Gate ohne künstliche harte Issue-Abhängigkeit |
| bestehender Code auf `main` | #15 Commandpfad, #17/#18 Run-Recovery, #20/#21 Sensorverträge, #22 Resultat, #23 Planner/Sink, Composition Roots |
| Live-Issue #24, Abhängigkeiten #14/#15/#17/#18/#20/#21/#22/#23/#56/#57 | aktuelle fachliche Inputs und Integrationsgrenzen |

Die konkreten stabilen Issue-#24-Codes sowie das exakte Restart-Fenster sind in
den kanonischen Quellen noch nicht vollständig festgelegt. Der Plan macht
deshalb einen vollständigen, prüfbaren Vorschlag und kennzeichnet die
Freigabe vor Umsetzung als `OG-24-CODES` beziehungsweise `OG-24-RESTART`.
Die vorgeschlagenen Werte sind bis dahin keine Laufzeitkonstanten.

## 4. PR #107 als nicht normative Fehleranalyse

Die Live-Auswertung von PR #107 hat mindestens folgende für den Neustart
relevante Fehlentwicklungen bestätigt:

| Befund | Nachweis in PR #107 | Auflösung in diesem Plan |
|---|---|---|
| A: globales `diagnosticOrigin` | eine globale `Y4-008`-Instanz speichert nur den zuletzt gemeldeten Origin | unabhängige Identitäten aus Producerdomäne, Quelle, Korrelation und Instance-ID; jede Ursache erhält einen eigenen begrenzten Slot; Clear matcht genau diese Identität |
| B: Marker-Recovery vermischt sich mit `SAFE_BOOT` | `recoverSafetyStateMarker()` liegt auf demselben Servicepfad wie die Exit-Logik | Storage-/Integrity-Marker werden separat qualifiziert; `SAFE_BOOT`-Latch bleibt unverändert; Exit nur über explizite autorisierte Exit-Transition |
| C: zweiter Safetyzustand im Commandmodell | PR #107 ergänzt ein Fault-Snapshot im `RunCommandState` und mutiert Safetyprojektionen im Commandpfad | eine `SafetyFaultService`-Instanz besitzt alle Fault-/Latch-/Resetzustände; #15 erhält nur Revision, Pending- und typisierte Resetprojektion |
| D: unvollständiges Wireformat | semantisch notwendige Fault-, Restart- und Markerzustände waren nicht vollständig als Roundtrip-Schema abgesichert | vollständige Feld-/Typ-/Byte-/Validierungs-/Migrations-/Overflow-Tabelle vor Implementierung; feste Records und vollständige Codec-Orakel |
| E: Recordgrößen- statt Semantiktests | ein Bytebudget ersetzt keinen Werte- und Corruption-Roundtrip | Matrix mit mehreren Enum-/Origin-/State-Werten, Beziehungen, IDs, Revisionen, Corruption, Mismatch und `CommitOutcomeUnknown` |
| F: alter Application-Startpfad | der alte `begin(platform)` kann den Safety-Service umgehen; Safety-Dependencies sind nur im neuen Overload vorhanden | ein einziger verpflichtender Composition-Vertrag; fehlende Safety-Dependencies sind ein fail-closed Startup-Ergebnis; kein produktiver safety-freier Overload |
| G: `begin() == false` als SAFE_BOOT | gewöhnlicher Startup-Fehler und kritischer Safety-Persistenzfehler werden nicht sauber unterschieden | typisierte Startup-/Boot-Ergebnisse und definierter aktorfreier Bootzustand; kritische Recordfehler führen nicht zu normaler Freigabe |
| H: kanonische Dokumente als Verlaufslog | PR #107 ergänzt Reviewstände, alte Heads und temporäre Teststatus in dauerhafte Fachdokumente | kanonische Dokumente enthalten nur dauerhafte Verträge; Plan-/Review-/Testverlauf bleibt PR-Body, Kommentar oder Handover |

Die PR-107-Änderungen werden nicht übernommen. Auch die dort beobachteten
Slotzahlen, Schemawerte, Restartwerte und Adapter sind keine Vorgabe für die
Umsetzung.

## 5. Owner-Gates vor jeder Umsetzung

Diese Gates sind absichtlich sichtbar. Sie werden nicht durch Annahmen,
`TBD_*`-Werte oder Test-Only-Verträge ersetzt.

| Gate | Entscheidung/Nachweis | Umsetzungssperre bei fehlender Freigabe |
|---|---|---|
| `OG-24-CODES` | Freigabe der stabilen 21 Codeanker und ihrer Ursache-/Correlation-Schlüssel gemäß Abschnitt 6; mindestens die darin genannten Y4-006- und Y4-008-Semantiken müssen erhalten bleiben | keine Code-Enums, Persistenzwerte oder Resetmatrix implementieren |
| `OG-24-RESTART` | exakte Anzahl abnormaler Restart-Evidenzen und Zeitfenster; die kanonische „starting point 3“-Aussage ist noch kein finaler Laufzeitwert | keine Restart-Eskalation oder SAFE_BOOT-Schwelle implementieren |
| `OG-24-RECOVERY` | codebezogene Resetberechtigung, aktuelle Safetychecks, erlaubte automatische Wiederfreigabe sowie Commissioning-/Thermalschwellen | keine implizite Auto-Rearm- oder Resetpolicy implementieren |
| `OG-24-AUTH` | Form des autorisierten SAFE_BOOT-Exit- und Service-Reset-Nachweises; #24 konsumiert nur typisierte Evidenz | keine PIN-, Token- oder Identitätsprüfung in #24 erfinden |
| `OG-24-ROOT` | Freigabe des verpflichtenden Composition-Root-Vertrags mit fail-closed Verhalten, obwohl reale ESP-IDF-/NVS-/Resetadapter außerhalb dieses Scopes bleiben | keinen alten `begin()`-Pfad oder normalen Start ohne Safety-Core zulassen |
| `OG-24-PRODUCERS` | Zuordnung der tatsächlich auf `main` vorhandenen Producer zu realen bzw. injection-only Quellen; fehlende Hardwarefeedbacks bleiben Contract-/Injection-only | keine GPIO-, Tachometer-, Strom- oder thermischen Hardwarebehauptung implementieren |
| `OG-24-BUDGET` | Bestätigung, dass die aus der Producer-Tabelle berechnete Recordgröße in den später gemessenen Ressourcenrahmen passt | keine physische Flash-/Heap-Garantie aus der Planrechnung ableiten |

## 6. Fault-, Producer- und Capacity-Modell

### 6.1 Identitätsregel

Jede aktive Ursache ist ein eigener `FaultInstance`. Die Identität ist der
Tupel:

```text
(stableCode, producerDomain, producerSource, correlationKey)
```

`instanceId`, `faultRevision` und monotone `diagnosticSequence` sind davon
separate, persistierte Lebenszyklusdaten. Ein neuer Vorgang derselben
Identität darf nur gemäß dem Codevertrag dedupliziert werden; eine andere
Correlation oder Producerquelle erzeugt eine neue Instanz.

Für `Y4-008` ist `producerDomain` selbst Teil der Identität. Die sechs im
aktuellen Vertrag realistisch möglichen unbekannten Domänen sind `Process`,
`Configuration`, `Boot`, `Sensor`, `Actuator` und `Persistence`. Damit kann
eine ungeklärte Sensorursache eine ungeklärte Process-, Configuration-, Boot-,
Actuator- oder Persistenceursache nicht überschreiben. Ein Clear darf nur die
gematchte Identität auflösen.

Die Tabelle verwendet vorläufige stabile Codeanker. Die Labels sind
fachlich vollständig, aber bis `OG-24-CODES` nicht als bereits akzeptierte
öffentliche Codes zu behandeln.

### 6.2 Vollständige Producer-/Fault-Tabelle

`B` ist die maximal gleichzeitig aktive Anzahl eigenständiger Instanzen dieser
Zeile. `P` bedeutet persistiert; `R` bedeutet beim Boot aus dem Producer neu
bewertet; `I` bedeutet derzeit nur Contract-/Injection-only. Die Tabelle ist
die Herleitung der Capacity, nicht eine nachträgliche Anpassung an eine
vorgewählte Slotzahl.

| Vorschlag | Producer und heutiger Status | Identität/Correlation | Klasse | B | Latch | Sofortreaktion | Auto-Rearm | Cause-Clear/Reset | Reboot/SAFE_BOOT | Persistenz/Primary-Follow-up | Injection-Orakel |
|---|---|---|---:|---:|---|---|---|---|---|---|---|
| `P1-001` Process warning | #14/#22 Prozess- bzw. Control-Resultat, real typisiert | Prozessphase + Control-Revision | P1 | 1 | nein | keine zusätzliche Aktorfreigabe aus diesem Code | ja, nur bei explizit freigegebenem Codevertrag | Resultat aktuell nicht warnend; kein Safety-Reset | `R`, kein Latch | bootlokal; Follow-up auf O2/S3 möglich | mehrere Control-Reasons, keine falsche Safetyfreigabe |
| `O2-001` Product sensor fallback | #20/#21 real; Ersatzstrategie über #21 | Sensorrolle + Auswahlrevision | O2 | 1 | nein | Peltier aus bis gültige Ersatz-/Requalifikation | nur nach #21-Nachweis | stabile gültige Auswahl; Resetprüfung des zentralen Cores | `R` | bootlokal; kein Primär-Latch | VALID/STALE/FAILED und Auswahlwechsel |
| `O2-002` Thermal response observation | thermische Reaktion aus #35/Commissioning bisher nicht real; Injection-only | Richtung + Control-Revision + Beobachtungsfenster | O2 | 1 | nein | Peltier aus, Diagnose | nur explizit nach `OG-24-RECOVERY` | neue Sensor-/Aktor-Evidenz | `R`, keine automatische Freigabe | bootlokal; kann zu `S3-009` folgen | langsame Produktreaktion darf nicht allein genügen |
| `O2-003` Sensor requalification | #20/#21 realer Stale-/Requalifikationspfad | Sensorrolle + Requalifikationslauf | O2 | 1 | nein | betroffene Peltierfreigabe aus | nur nach #20/#21 | stabile Mehrfachmessung | `R` | bootlokal; Folgebeziehung zu S3 bleibt erhalten | gültige, alte, ungültige und wechselnde Snapshots |
| `S3-001` Air sensor failed | #20 real, Produktintegration über #21 | Sensor-ID + Bus-/Messfolge | S3 | 1 | ja | beide Peltier-Richtungen aus, sicherer Lüfternachlauf | nein | Ursache behoben, aktuelle Sensor-/Storage-/Runchecks, autorisierter Reset | `P` | eigene Instanz; kein Clear anderer Sensoren | FAILED, Recovery ohne direkten Aktorzugriff |
| `S3-002` Cooling sensor failed | #20 real | Sensor-ID + Bus-/Messfolge | S3 | 1 | ja | beide Peltier-Richtungen aus, Lüfter gemäß #23 | nein | wie S3-001 | `P` | eigene Instanz | FAILED und unabhängige Air-Ursache |
| `S3-003` Sensor compatibility unresolved | #20/#21 real; inkompatible Rollen-/Plausibilitätsdaten | Rollenpaar + Auswahl-/Plausibilitätsrevision; maximal 3 Rollenpaare | S3 | 3 | ja | Peltier aus, keine stille Umschaltung | nein | eindeutige neue Plausibilität plus autorisierter Reset | `P` | je Rollenpaar; Primary/Follow-up bleibt separat | mehrere gleichzeitig unklare Rollenpaare |
| `S3-004` Safety intervention limit | Grenzlogik auf Basis #20/#21/#22; Schwellen noch Commissioning | Richtung + Control-Revision | S3 | 1 | ja | auslösende Richtung aus, keine automatische Gegenrichtung ohne Vertrag | nein | neue qualifizierte Werte, Resetchecks | `P` | kann Folge von O2-002 sein, wird nicht von ihm gelöscht | Limit, Recoveryversuch und Wiederholung |
| `S3-005` Hard safety limit | #20/#21/#22; Schwellen noch Commissioning | Sensorrolle + Richtung + Messfolge | S3 | 1 | ja | beide Richtungen aus | nein | aktuelle sichere Messung, Autorisierung, Reset | `P` | eigener Latch | beide Richtungen und Grenzwertcorruption |
| `S3-006` Outer fan fault | #23 Planner/Sink-Vertrag; reale Rückmeldung aktuell nicht vorhanden | Fan-Sink + Planrevision | S3 | 1 | ja | Peltier aus; sicherer Nachlauf nur soweit Vertrag beweist | nein | nur typisierte sichere Sink-/Watchdog-Evidenz | `P` | eigener Latch | Widerspruch/fehlende Evidenz, kein Tachometer behauptet |
| `S3-007` Inner fan fault | #23 Planner/Sink-Vertrag; reale Rückmeldung aktuell nicht vorhanden | Fan-Sink + Planrevision | S3 | 1 | ja | Peltier aus | nein | wie S3-006 | `P` | eigener Latch | unabhängiges Innen-/Außen-Fehlerpaar |
| `S3-008` Planner/sink/watchdog contradiction | #23 realer Planner-/Watchdog-Vertrag; physischer Sink nur abstrakt | Quelle `Planner`, `Sink` oder `Watchdog` + Plan-/Watchdog-Revision | S3 | 2 | ja | zentraler Gate-Stop; kein direkter Sink-Bypass | nein | widerspruchsfreie aktuelle Plan-/Sink-Evidenz, autorisierter Reset | `P` | Planner und Sink bleiben getrennt; Primary/Follow-up zulässig | stale Gate, falscher Plan, Watchdog-Ereignis |
| `S3-009` Thermal response escalation | heute nur #22/#23 Vertrags-/Injection-Pfad; keine reale Temperaturbehauptung | Richtung + Beobachtungsrevision | S3 | 1 | ja | Peltier aus | nein | neue qualifizierte Kurzzeit-/Langzeit-Evidenz nach `OG-24-RECOVERY` | `P` | Folge von O2-002 möglich, niemals dessen Clear | Wiederholung eskaliert, Produktträgheit wird berücksichtigt |
| `Y4-001` Run not reconstructible | #17/#18 real; `loadAndInitialize()`/Recovery-Resultate | Run-ID + Persistenzrevision | Y4 | 1 | ja | alle Aktoren aus | nein | vollständige Run-Recovery und autorisierter Reset | `P`, bei ungeklärtem Boot `SAFE_BOOT` | eigener System-Latch | gültiger, fehlender und unvollständiger Checkpoint |
| `Y4-002` Safety persistence failure | `IStateStore`/Safetyrecord; Fehler injizierbar, kein neuer NVS-Adapter | Record-Key + Recordrevision + Store-Epoch | Y4 | 1 | ja | RAM-Latch, Aktoren aus; unbekannter Write bleibt ungeklärt | nein | vollständiger Read/Write/Readback-Nachweis | `P`, unklarer Boot -> `SAFE_BOOT` | Marker-/Capacitydaten außerhalb der Faultslots | WriteError, CapacityError, readback mismatch, `CommitOutcomeUnknown` |
| `Y4-003` Internal boot invariant failure | Composition-/Bootvertrag, derzeit Contract-/Injection-only | Bootphase + Bootrevision | Y4 | 1 | ja | aktorfreier Bootzustand | nein | neue vollständige Bootnachweise und Autorisierung | `P`/`SAFE_BOOT` | eigener Latch | fehlende Safety-Dependency, falsche Bootreihenfolge |
| `Y4-004` Controlled restart failure | neutraler Restart-/Observation-Port, ESP-IDF-Adapter außerhalb Scope | Restart-Evidence-ID + Resetobservation | Y4 | 1 | ja | aktueller Lauf bleibt sicher aus | nein | nur neue eindeutige Evidence und Recovery | `P`, wiederholt -> SAFE_BOOT nach Gate | Restart-Evidence-Beziehung | exactly-once, duplicate, missing und stale observation |
| `Y4-005` Configuration safety gate | #56/#57 real typisiert; Varianten `ConfigurationRuntimeFailure`, `ConfigurationUnavailable`, `ConfigurationIntegrityFailure`, `CommitOutcomeUnknown` | Config-Quelle (#56/#57) + Root-/Storage-Epoch + Statusrevision; max. 2 unabhängige Quellen | Y4 | 2 | ja | keine normale Freigabe; Aktoren aus | nein | jeweils nur der zugehörige Gate-Status wird neu qualifiziert | `P`, unbekannt -> SAFE_BOOT | #56 und #57 unabhängig; keine Doppelentscheidung in #24 | alle vier Statusvarianten, einschließlich unresolved outcome |
| `Y4-006` Safety marker/capacity recovery | Safetyrecord-Marker, Capacity- oder Integrity-Status; zentrale Autorität | Markerart + Recordrevision + betroffene Instance-/Correlation-Referenz | Y4 | 1 | ja/Marker | fail-closed; niemals eigener Gate-Allowed-Wert | nein | Marker nur nach vollständigem Read/Write/Readback | `P`; Recovery beendet SAFE_BOOT nicht | **kein aktiver Faultslot**, eigener persistenter Marker | marker valid/invalid, capacity full, dangling reference |
| `Y4-007` Abnormal restart / SAFE_BOOT latch | Restart-Policy aus #18/#24; exakte Schwelle noch offen | Restart-Episode-ID + Evidence-ID + Zeitfenster | Y4 | 1 | ja | alle Aktoren aus, keine Auto-Run-/Aktor-Tests | nein | expliziter SAFE_BOOT-Exit mit aktueller Evidenz und Autorisierung | `P`, normaler Reboot löscht nicht | eigener persistent state | einmaliger und wiederholter abnormaler Restart |
| `Y4-008` Unknown/unresolved cause | mehrere reale oder injection-only Producerdomänen wie in 6.1 | **Domäne + Quelle + Correlation**, niemals nur letzter Origin; maximal 6 Domänen | Y4 | 6 | ja | Unresolved ist nie `Allowed`; höchste Klasse bleibt wirksam | nein | nur exakt gematchte Ursache und aktuelle Checks | `P`; ungeklärt -> SAFE_BOOT | jede Domäne eigener Slot; keine Eviction | sechs gleichzeitige unbekannte Ursachen und unabhängige Clears |

### 6.3 Kapazitätsableitung

Die maximale aktive Faultkapazität wird aus der Tabelle abgeleitet:

```text
K_active
  = 1(P1-001)
  + 3(O2-001..003)
  + (1+1+3+1+1+1+1+2+1)(S3-001..009)
  + (1+1+1+1+2+1+1+6)(Y4-001..008)
  = 30 Faultinstanzen
```

`Y4-006` benötigt zusätzlich genau einen persistierten Marker außerhalb der
30 Faultslots. Eine volle Tabelle führt nicht zu Eviction: Neue Ursachen
setzen den Marker `CapacityExceeded`, bleiben sicherheitswirksam als
unqualifizierte/ungeklärte Ursache und verhindern `Allowed`, bis eine
autorisierte, ursachengenaue Recovery den Zustand wieder beweist. Ein aktiver
Latch darf nie zur Entwarnung verdrängt werden. `instanceId` und
`faultRevision` sind monotone, begrenzte `uint32`-Werte; vor dem Überlauf wird
fail-closed in `Y4-006`/`Y4-002` gewechselt, nicht gewrappt.

Die Capacity ist nur eine logische Vertragsgrenze. Ob das daraus folgende
Wireformat in Flash-, RAM- und Stackbudget passt, wird erst mit realen Builds
und dem Gate `OG-24-BUDGET` geprüft; `TBD_IMPLEMENTATION_BUDGET` bleibt kein
Laufzeitwert.

## 7. Zentrale Autorität und Datenfluss

### 7.1 Fachliche Architektur

```text
#20 SensorQualitySnapshot + #21 Auswahl/Plausibilität
#14 Prozessstatus + #22 TemperatureControlResult
#17/#18 Run-Recovery- und Checkpointresultate
#23 Planner-/Watchdog-/Sink-Evidenz
#56/#57 Configuration-Safety-Gate
neutraler Reset-/Restart-Observation-Port und Injection-Ports
        |
        v
eine SafetyFaultService / FaultCore-Instanz (#24)
  - FaultIdentity-/Lifecycle-Tabelle
  - einzige Latch-, Reset-, Revision- und SAFE_BOOT-Autorität
  - Capacity-/Marker-/Restart-Evidence-Verwaltung
        |
        +--> immutable CommandSafetyProjection -> bestehender #15 Commandpfad
        |
        +--> immutable SafetyGateEvidence -> ActuatorSafetyGateInput
        |                                      |
        |                                      v
        |                            bestehender #23 ActuatorPlanner
        |                                      |
        |                                      v
        |                            bestehender ActuatorPlanSinkDriver
        |
        +--> SafetyStateRecord -> bestehender IStateStore/StorageEnvelope
        |
        +--> begrenztes Journal-/Ereignis-Interface (kein #19-Archiv)
```

Der Planner darf nur eine vom zentralen Service erzeugte, revisionsgebundene
Gate-Evidence konsumieren. Ein caller-supplied `Allowed`, ein veraltetes
Evidence-Objekt, fehlende Herkunft oder eine unbekannte Revision ergibt
`Unresolved`/Stop. Der Sink erhält weiterhin nur den bestehenden #23-Plan und
kann keine Safetyfreigabe erzeugen.

### 7.2 Genau eine Fault-/Reset-Autorität

`SafetyFaultService` besitzt als einzige mutable Daten:

- Faultinstanzen, Lifecycle, Ursache-Clear und Primary-/Follow-up-Referenzen;
- `faultRevision`, `instanceId` und Capacityzustand;
- persistente Safetymarker, Restart-Episode und `SAFE_BOOT`;
- Reset- und SAFE_BOOT-Exit-Transitions.

`RunCommandState` bleibt der #15-Commandzustand. Seine vorhandenen Felder
`faultRevision` und `criticalSafetyEventPending` werden nur als schmale,
immutable beziehungsweise revisionsgeprüfte Projektion behandelt. Ein
Fault-Snapshot, Fault-Latch, eigener Clear- oder Reset-State kommt nicht in das
Commandmodell. `FaultResetEvaluation` wird nicht als fremde Safetyentscheidung
akzeptiert: Der Service erzeugt eine typisierte, revisionsgebundene
Reset-Capability; #15 transportiert und validiert sie mit seinem vorhandenen
Command-/Persist-then-apply-Vertrag. Der Commandpfad setzt weder einen Latch
noch das Actuator-Gate zurück.

Damit gilt:

- Single Responsibility: Faultpolicy und Reset liegen nur im Service;
- Dependency Inversion: Commands, Planner und Sink konsumieren Ports/
  Projektionen;
- DRY: #15 entscheidet Commandatomarität, #24 entscheidet Safety;
- KISS: kein zweiter FaultCore und keine vollständige Faultkopie im Commandstate.

### 7.3 Composition Root ohne Bypass

Die bestehenden `src/main.cpp`- und `main/app_main.cpp`-Roots rufen heute
`platform.begin(startupContext) && application.begin(platform)` auf. Der Plan
ändert diesen produktiven Vertrag so, dass es nur noch einen verpflichtenden
Application-Start mit `SafetyComposition` gibt. Dieser enthält ausschließlich
abstrakte Ports für StateStore, Zeit, Reset-/Restartbeobachtung, Journal und
die #56/#57-Gate-Projektion.

Ein Root, der eine erforderliche Safety-Dependency nicht liefern kann, erhält
ein typisiertes Ergebnis wie `SafetyDependenciesUnavailable` und bleibt
aktor- und lauf-inaktiv. Das ist ein normaler Application-Startupfehler und
nicht automatisch `SAFE_BOOT`; ein geladener, beschädigter kritischer
Safetyrecord ergibt dagegen einen expliziten aktorfreien `SafetyBootRequired`-
Zustand. Keiner dieser Fälle darf `Allowed` herstellen.

Der bisherige `begin(IPlatformServices&)` ohne SafetyComposition wird entfernt
oder in einen nicht produktiv erreichbaren, fail-closed-only Testvertrag
überführt. Ein alternativer öffentlicher `begin()`-Pfad ist nicht zulässig.
Damit gibt es keinen safety-freien normalen Start und keine künstliche
Abhängigkeit auf #29/#90: reale ESP-IDF-/NVS-/Resetadapter bleiben außerhalb
des Scopes, ein fehlender Adapter führt jedoch nicht zu einer Freigabe.

## 8. Boot-, Recovery- und SAFE_BOOT-Vertrag

### 8.1 Bootreihenfolge

```text
Outputs sicher AUS
  -> Safetyrecord laden und Envelope/Schema/CRC/Referenzen vollständig prüfen
  -> ungültig/unklar: fail-closed SafetyBootRequired + Diagnosezugang ohne Aktoren
  -> Restart-Cause beobachten und Restart-Evidence genau einmal konsumieren
  -> #56/#57 Configuration-Safety-Status qualifizieren
  -> #17/#18 Run-Recovery laden und Reconstructibility prüfen
  -> #20/#21 Sensorqualität, Auswahl und Plausibilität qualifizieren
  -> #22 Resultat und #23 Planner-/Sink-Vertrag qualifizieren
  -> zentrale Fault-/Dominanz-/SAFE_BOOT-Entscheidung
  -> neue Boot-/Recovery-Evidenz vor jeder Anwendung persistieren und readbacken
  -> nur bei expliziter autorisierter Exit-Transition Gate-Evidence erzeugen
  -> #23 Planner und Sink erhalten die Evidence; sonst bleibt alles sicher AUS
```

Normale Application-Startupfehler, Safetyrecord-/Integrityfehler und
`SAFE_BOOT` sind unterschiedliche typisierte Zustände. Diagnose, lokales
Auslesen und die nach dem kanonischen Vertrag erlaubte aktorfreie
Recoveryfunktion bleiben in `SAFE_BOOT` verfügbar. Kein Diagnosepfad enthält
einen Aktortest oder eine normale Aktorfreigabe.

### 8.2 Marker-Recovery ist kein SAFE_BOOT-Exit

`Y4-006` qualifiziert ausschließlich die Eigenschaft, dass der Safetyrecord,
Capacitymarker oder Integritätsnachweis wieder lesbar, schreibbar und
readback-identisch ist. Diese Transition darf:

- den Marker auflösen, wenn genau dessen Referenz und Revision geprüft sind;
- eine neue Recordrevision persistieren;
- die `StorageIntegrityQualified`-Eigenschaft aktualisieren.

Sie darf **nicht**:

- `safeBootRequired` oder die SAFE_BOOT-Episode löschen;
- Restart-Evidence als konsumiert oder erfolgreich fingieren;
- einen Fault-Latch anderer Domäne löschen;
- `ActuatorSafetyGateStatus::Allowed` herstellen.

Der einzige SAFE_BOOT-Exit ist `requestAuthorizedSafeBootExit`, ausgeführt von
der zentralen Autorität. Er verlangt eine aktuelle, codebezogene,
autorisierte Exit-Evidence, einen gültigen Record mit readback, qualifizierte
#56/#57- und #17/#18-Nachweise, gültige #20/#21/#22-Evidenz, keine aktive
dominante Faultinstanz sowie die vom Codevertrag geforderten Resetchecks. Erst
danach darf die nächste zentrale Auswertung ein Gate-Evidence erzeugen. Ein
normaler Reboot allein beendet SAFE_BOOT nie.

### 8.3 Restart-Evidence exactly once

Ein kontrollierter Neustart wird als persistente Sequenz behandelt:

1. zentrale Autorität erstellt `RestartIntent` mit neuer `evidenceId`,
   Ziel-Faultrevision und Episode-ID;
2. Intent wird write-before-apply mit readback persistiert;
3. erst danach darf der neutrale Restart-Port die Aktion anfordern;
4. beim nächsten Boot wird die externe Resetbeobachtung mit der Evidence-ID
   korreliert;
5. eine passende Evidence wird genau einmal konsumiert und als konsumiert
   persistiert; Duplikate sind kein neuer Restart;
6. fehlende, stale, widersprüchliche oder `CommitOutcomeUnknown`-Evidence
   bleibt fail-closed und erzeugt keine Erfolgsmeldung.

Die Eskalation wiederholter abnormaler Neustarts verwendet erst nach
`OG-24-RESTART` die festgelegte Schwelle und das festgelegte Fenster. Die
kanonische Ausgangsaussage „starting point 3“ wird nicht still als
Laufzeitwert gesetzt.

## 9. Persistenz- und Wire-Schema vor Implementierung

### 9.1 Allgemeine Regeln

Der Safetyrecord verwendet den bestehenden `IStateStore` und das bestehende
`StorageEnvelope`-Format auf `main`; er erfindet keinen zweiten Storageport.
Der Envelope trägt Recordtyp, bestehende Envelope-Schema-Version, StorageEpoch
und Version/Revision. UTC wird nicht zur Safetyidentität verwendet.

Der Payload ist ein fester, little-endian-unabhängig definierter Big-Endian-
Record. Keine optionalen oder variablen Faultslots werden in der ersten
Version zugelassen. Unbekannte Enumwerte, falsche Version, falsche Länge,
CRC-/Epoch-/Referenzfehler und abgeschnittene Daten ergeben `SafetyBootRequired`
beziehungsweise den passenden fail-closed RAM-Latch. Eine alte Version wird
nicht still als aktuelle Version interpretiert.

### 9.2 Payloadgröße und Feldschema

Der Payload besteht aus einer 128-Byte-Basis und 30 festen Slots zu je 64 Byte.
Zusätzlich enthält der bestehende Envelope ohne UTC die auf `main` definierte
37-Byte-Hülle. Die Planrechnung ergibt daher:

```text
payload = 128 + 30 * 64 = 2048 Byte
record  = 37 + 2048 = 2085 Byte
```

Das ist eine abgeleitete logische Obergrenze, keine gemessene Flashgarantie;
`OG-24-BUDGET` bleibt vor Umsetzung offen.

#### Basis, 128 Byte

| Feld | Typ/Bytes | Persistenz | Validierung und Unknown-Verhalten |
|---|---:|---|---|
| Payload-Schema | `u32 / 4` | ja | exakt bekannte Version, sonst SafetyBoot |
| Recordrevision | `u32 / 4` | ja | monotone, nicht null wrapende Revision |
| Faultrevision | `u32 / 4` | ja | monotone; Overflow fail-closed |
| Next-Instance-ID | `u32 / 4` | ja | nicht null; Overflow fail-closed |
| StorageEpoch-Spiegel | `u64 / 8` | ja | muss Envelope-Epoch entsprechen |
| SAFE_BOOT-State | `enum u8 / 1` | ja | nur `Cleared`, `Required`, `ExitPending`; andere Werte SafetyBoot |
| SAFE_BOOT-Grund | `code u16 / 2` | ja | bekannte Y4-/Integritygründe |
| SAFE_BOOT-Faultinstance | `u32 / 4` | ja | null oder gültige Slot-ID |
| SAFE_BOOT-Revision | `u32 / 4` | ja | passt zur referenzierten Faultrevision |
| Exit-Authorization-ID | `u64 / 8` | ja | null nur ohne ExitPending |
| Exit-Evidence-Revision | `u32 / 4` | ja | aktuelle qualifizierte Evidence |
| Marker-State | `enum u8 / 1` | ja | `None`, `Pending`, `Capacity`, `Integrity`; unbekannt fail-closed |
| Marker-Code | `u16 / 2` | ja | bekannter Code oder Null bei None |
| Marker-Domain | `enum u8 / 1` | ja | gültige Producerdomäne |
| Marker-Source | `u32 / 4` | ja | stabile Quelle |
| Marker-Correlation | `u32 / 4` | ja | darf nicht durch andere Ursache ersetzt werden |
| Marker-Faultrevision | `u32 / 4` | ja | gültige Revision |
| Marker-Recordrevision | `u32 / 4` | ja | readback-/Clear-Bezug |
| Active-Slot-Count | `u8 / 1` | ja | `0..30`, exakt gegen Slots |
| Capacity-Cause | `u16 / 2` | ja | nur bei Capacity-Marker sonst Null |
| Capacity-Domain | `u8 / 1` | ja | gültige Domain oder Null |
| Capacity-Source | `u32 / 4` | ja | gültig bei Capacity |
| Capacity-Correlation | `u32 / 4` | ja | gültig bei Capacity |
| Capacity-Revision | `u32 / 4` | ja | gültig bei Capacity |
| Restart-Episode-ID | `u32 / 4` | ja | nicht null bei Episode |
| Abnormal-Restart-Count | `u32 / 4` | ja | begrenzt, Overflow fail-closed |
| Restart-Window-Start | `u64 / 8` | ja | monotone Zeit, nur nach Owner-Fenster |
| Last-Evidence-ID | `u32 / 4` | ja | genau-einmal-Korrelation |
| Next-Evidence-ID | `u32 / 4` | ja | nicht null, Overflow fail-closed |
| Restart-Episode-State | `enum u8 / 1` | ja | `None`, `Open`, `SafeBoot`; unbekannt fail-closed |
| Restart-Intent-State | `enum u8 / 1` | ja | `None`, `Prepared`, `Observed`, `Consumed`; Reihenfolge prüfen |
| Restart-Cause | `enum u8 / 1` | ja | bekannte Resetursache oder Unknown; Unknown ist nicht Erfolg |
| Restart-Observation-ID | `u64 / 8` | ja | eindeutige Beobachtung, bei fehlend fail-closed |
| Config-Gate-Qualified | `bool u8 / 1` | ja | nur #56/#57-Projektion darf true liefern |
| Integrity-Qualified | `bool u8 / 1` | ja | nur nach Read/Write/Readback |
| Reserved/CRC-contract bytes | `u8 / 8` | ja | müssen Null sein; sonst Corruption |

Die obige Tabelle ist in dieser Reihenfolge zu packen; die Summe der
aufgeführten Bytebedarfe ergibt 128 Byte. Der Payload-CRC bleibt Bestandteil
des bestehenden Envelopes und wird nicht als neue parallele Integritätslogik
implementiert.

#### Jeder Faultslot, 64 Byte

| Feld | Typ/Bytes | Persistenz | Validierung und Unknown-Verhalten |
|---|---:|---|---|
| Instance-ID | `u32 / 4` | ja | eindeutig, nicht null |
| Stable-Code | `u16 / 2` | ja | `OG-24-CODES`-Tabelle, sonst fail-closed |
| Klasse | `enum u8 / 1` | ja | P1/O2/S3/Y4, unbekannt fail-closed |
| Producer-Domain | `enum u8 / 1` | ja | bekannte Domain, insbesondere Y4-008 |
| Producer-Source | `u32 / 4` | ja | stabile Quelle |
| Correlation-Key | `u32 / 4` | ja | Teil der Identität |
| Diagnostic-Sequence | `u32 / 4` | ja | monotone Sequenz |
| Created-Monotonic | `u64 / 8` | ja | nur deterministische Zeitbasis |
| Lifecycle-State | `enum u8 / 1` | ja | ACTIVE_UNACKNOWLEDGED, ACTIVE_ACKNOWLEDGED, CAUSE_CLEARED_LOCKED, CLEARED |
| Cause-State | `enum u8 / 1` | ja | `Active`, `Cleared`, `Unknown`; Unknown hält sicher |
| Latch-State | `enum u8 / 1` | ja | `None`, `Latched`; Y4/S3-Regel prüfen |
| Auto-Rearm-State | `enum u8 / 1` | ja | `NotAllowed`, `Eligible`, `Used`; unbekannt nicht freigeben |
| Fault-Revision | `u32 / 4` | ja | zum Servicezustand passend |
| Primary-Present | `bool u8 / 1` | ja | exakt mit Primary-ID konsistent |
| Primary-Instance-ID | `u32 / 4` | ja | gültig, nicht self, keine dangling reference |
| Clear-Evidence-Revision | `u32 / 4` | ja | nur nach passendem Cause-Clear |
| Reset-Authorization-ID | `u64 / 8` | ja | null bis autorisierter Resetversuch |
| Last-Reset-Result | `enum u8 / 1` | ja | bekannte Resultate, Unknown fail-closed |
| Slot-Flags/Reserved | `u8 / 10` | ja | nur definierte Bits; sonst Corruption |

Auch diese Tabelle summiert exakt 64 Byte. Nicht in der Tabelle stehende
Semantik darf nicht heimlich im RAM verbleiben: Entweder wird ein zusätzliches
Feld nach einer neuen Plan-/Owner-Freigabe ergänzt oder sie wird ausdrücklich
als bootlokal markiert und beim Boot fail-closed neu qualifiziert.

### 9.3 Migration, Overflow und Commit-Ergebnis

- Nur Payload-Schema 1 wird in diesem Plan implementiert. Eine andere Version
  wird als unbekannt geladen, bleibt fail-closed und erhält keine automatische
  Migration.
- Migrationen für spätere Versionen benötigen eine neue vollständige
  Planrevision mit Feldmapping und Roundtrip-Orakel.
- `WriteError` und `CapacityError` sind kein Commit.
- `CommitOutcomeUnknown` ist weder Erfolg noch Misserfolg, sondern ein
  persistenter ungeklärter Safetyzustand. Die Anwendung darf nicht raten, ob
  alt oder neu geschrieben wurde. Nur ein vollständiger, eindeutig validierter
  Readback darf die Unsicherheit auflösen.
- Readback-Mismatch, falscher Epoch, abgeschnittener Record, falsche Länge,
  ungültiger Enumwert oder ungültige Referenz hält den RAM-Latch und führt beim
  nächsten Boot zu `SAFE_BOOT`.
- Recordcapacity ist eine harte semantische Grenze. Keine Slot-Eviction,
  kein stiller Truncate und keine Reduktion der Faultsemantik bei voller
  Tabelle.

## 10. Invarianten

| Invariante | Erwartung und Nachweis |
|---|---|
| Unknown/Unresolved | nie `Allowed`; `ActuatorSafetyGateInput` bleibt `Unresolved` oder Stop |
| mehrere unabhängige Ursachen | getrennte Identität, Slots und Clear-Evidence; keine gegenseitige Überschreibung |
| höchste aktive Fehlerklasse | zentrale Dominanzfunktion bestimmt den sicheren Ausgang; sie liest alle aktiven Instanzen |
| Reboot | löscht keinen notwendigen S3/Y4-Latch, Marker oder SAFE_BOOT-Zustand |
| Marker-Recovery | qualifiziert nur Storage/Integrity; keine SAFE_BOOT-Exit-Transition |
| SAFE_BOOT-Exit | nur explizit, autorisiert, codebezogen und nach allen aktuellen Nachweisen |
| Faultreset | nur `SafetyFaultService`; #15 transportiert nur die Capability |
| Commandpfad | kann weder Faultzustand noch `Allowed` erfinden |
| Planner/Sink | konsumieren Gate-/Plan-Evidence; kein direkter Bypass |
| Persistenz | alle rebootrelevanten Felder im vollständigen Record oder ausdrücklich bootlokal fail-closed |
| Persistenzfehler | kein normaler Runtime-/Aktorpfad bei unbekanntem oder beschädigtem Record |
| `CommitOutcomeUnknown` | niemals als Commit-Erfolg raten |
| Primary/Follow-up | Referenzen sind gültig, nicht self-referenziell und nicht dangling |
| Capacity | aktive Latches werden nicht evicted; Capacitymarker hält sicher |
| Restart-Evidence | write-before-apply und exactly-once über Evidence-ID |
| wiederholter abnormaler Restart | nach `OG-24-RESTART` gemäß freigegebener Policy `SAFE_BOOT` |
| Application-Root | kein safety-freier produktiver Startpfad |
| Tests | prüfen Vertragssemantik und Negativfälle, nicht bloß Implementierungsdetails |

## 11. Zieltests – Orakel vor Implementation

Die folgenden Orakel werden vor den jeweiligen Implementierungsschnitten als
Testfälle beschrieben. In dieser Planrunde werden sie nicht ausgeführt.

### 11.1 FaultCore-/Policy-Orakel

- alle 21 Codeanker und jede Producerdomäne werden über den zentralen Service
  injiziert; zwei zulässige Correlations derselben Klasse deduplizieren nur
  gemäß Tabelle;
- sechs unterschiedliche `Y4-008`-Domänen bleiben gleichzeitig aktiv und
  werden einzeln gecleart; das Clear einer Sensorursache lässt Process-,
  Configuration-, Boot-, Actuator- und Persistence-Ursachen aktiv;
- Dominanz, Immediate-Stop, Latch, Quittierung, Cause-Clear, autorisierter
  Reset und verbotener Auto-Rearm werden je Codeklasse geprüft;
- Primary-/Follow-up-Anlage, Clear-Reihenfolge, ungültige Selbst-/Fremdrefenz
  und Referenzrevision werden geprüft;
- `K_active=30`, Vollzustand, Capacitymarker, fehlende Slot-Eviction und
  `instanceId`-/`faultRevision`-Grenze werden geprüft;
- gleiche Event-ID, neue Correlation, stale Revision und widersprüchliche
  Producer-Evidence haben jeweils ein festgelegtes Ergebnis.

### 11.2 Persistenz- und Reboot-Orakel

- vollständiger Encode-/Decode-Roundtrip des gesamten Basisrecords und aller
  30 Slots;
- mindestens zwei Werte für jede relevante Enum-/Origin-/State-Dimension,
  darunter alle vier Klassen, mehrere `Y4-008`-Domains, Lifecyclezustände,
  Restartzustände, SAFE_BOOT-Zustände und Config-Gate-Statusvarianten;
- Primary-/Follow-up-Beziehungen, Faultrevision, Instance-ID,
  Diagnostic-Sequence, Restart-Evidence-ID, Episode-ID, SAFE_BOOT-Grund,
  Capacitymarker und Integritystatus bleiben bytegenau semantisch erhalten;
- Neustart mit aktivem Latch, mehreren Latches, SAFE_BOOT und ungeklärter
  Ursache erhält die Safetysemantik;
- CRC-/Payload-Corruption, ungültige Enumwerte, ungültige/dangling/self-
  Referenzen, falsche Version, falsche Länge, abgeschnittene Daten,
  Epoch-Mismatch und Readback-Mismatch führen fail-closed;
- `WriteError`, `CapacityError` und `CommitOutcomeUnknown` werden getrennt
  geprüft; kein Fall wird als erfolgreicher Commit behandelt;
- Restart-Evidence wird write-before-apply erzeugt und über einen simulierten
  Reboot genau einmal konsumiert; Duplicate/Replay/fehlende Evidence bleibt
  sicher;
- Marker-Recovery aktualisiert nur Storage-/Integrity-Qualifikation;
  SAFE_BOOT bleibt bestehen, bis der explizite Exit-Contract erfüllt ist.

### 11.3 Reale Integrationspfade

- #20/#21 liefern echte `SensorQualitySnapshot`-, Auswahl- und
  Plausibilitätsverträge; kein zweiter Sensorbewertungsalgorithmus wird im
  FaultCore nachgebaut;
- #22 liefert jedes `TemperatureControlResult` einschließlich neutraler,
  begrenzter, blockierter und ungültiger Zustände; ein Resultat wird nicht
  pauschal als Fault erfunden;
- #23 Planner und vorhandener Sink erhalten die zentrale Gate-Evidence und
  zeigen für `Unresolved`, stale, dominante Faults und SAFE_BOOT sichere
  Ausgänge;
- #17/#18 werden über ihre echten Load-/Recovery-Resultate angebunden;
  Run-Reconstructibility wird nicht aus einem Test-Only-Snapshot erfunden;
- #56/#57 liefern die vier Configuration-Statusvarianten als unabhängige
  typisierte Inputs; `CommitOutcomeUnknown` bleibt ungeklärt und blockierend;
- #15 verwendet den bestehenden Command-/Revision-/Persist-then-apply-Pfad;
  kein zweiter Reset- oder Faultdecision-Pfad entsteht.

### 11.4 Bypass- und Negativorakel

- kein caller-supplied `Allowed` kann einen zentralen `Unresolved`- oder
  stale-Zustand überschreiben;
- kein zweiter FaultCore, kein FaultSnapshot und kein mutierbarer Faultstate im
  `RunCommandState`;
- kein direkter Planner-/Sink-Aufruf, alter Application-Startpfad oder
  fehlende Safety-Dependency erzeugt normale Aktorfreigabe;
- fehlende, korrupte oder nicht aktuelle Evidence bleibt fail-closed;
- Ursache A clear löscht Ursache B nicht;
- Marker-Recovery erzeugt keine SAFE_BOOT-Exit-Transition und keine Allowed-
  Evidence;
- beschädigte Safety-Persistenz erzeugt keinen normalen Startup-/Aktorpfad;
- in SAFE_BOOT sind Runstart, Auto-Rearm und Aktortests unerreichbar.

## 12. Geplante Umsetzungsschnitte nach Owner-Freigabe

Die folgenden Schnitte sind Navigations- und Reviewgrenzen, keine
Freigabe zur Umsetzung:

1. **Typisierte Faultidentität und Policy:** `fault_types`, zentrale
   `SafetyFaultService`-/Policy-Ports, Code-/Producermatrix, Dominanz,
   Lifecycle, Primary/Follow-up, Capacity und FaultCore-Orakel.
2. **Safetyrecord und Codec:** bestehender `IStateStore`-/Envelope-Pfad,
   Schema 1, vollständige Validierung, Migration-/Overflowregeln,
   Readback-/Unknown-Behandlung und semantische Persistenztests.
3. **Boot-/Restart-/SAFE_BOOT-Orchestrierung:** Bootzustände,
   Restart-Evidence exactly once, getrennte Marker-Recovery, neutraler
   Reset-/Observation-Port und SAFE_BOOT-Orakel. Kein ESP-IDF-Adapter.
4. **Producerintegration:** reale #20/#21/#22/#23/#17/#18/#56/#57-Verträge
   über schmale Adapter; keine zweite Producerlogik oder Persistenzentscheidung.
5. **Command- und Composition-Integration:** #15-Projektion,
   revisionsgebundene Reset-Capability, verpflichtende Roots in
   `src/main.cpp` und `main/app_main.cpp`, Entfernung des safety-freien
   `begin()`-Pfads, Bypass-Orakel.
6. **Normative Dokumentation:** nur dauerhafte Verträge und Testorakel in den
   dafür kanonischen Dokumenten aktualisieren. Reviewstände, Testläufe,
   alte Heads und Ownerentscheidungen bleiben ausschließlich PR-Body,
   Reviewkommentar oder SESSION HANDOVER.
7. **Gezielte Plan-/Implementierungs-Gates:** Diff-/Format-/Architektur-/
   Secretscan und die betroffenen nativen Tests nach `CI_AND_QUALITY_GATES`;
   vollständige Builds, Full Native, ESP-IDF, Hardware und CI erst nach den
   dafür geltenden Owner-Gates.

### Voraussichtlich betroffene Bereiche

Die konkrete Dateiliste wird vor jedem Schnitt gegen den freigegebenen Plan
und den dann aktuellen `main`-Stand erneut geprüft. Voraussichtlich betroffen
sind nur:

- `lib/fermentation_app`: zentrale Fault-/Safety-Service- und Recordtypen,
  Boot-/Restart-Orchestrierung, Application-Komposition sowie die schmale
  #15-/#23-Projektion;
- `src/main.cpp` und `main/app_main.cpp`: verpflichtender SafetyComposition-
  Aufruf und fail-closed Startup-Ergebnis;
- `device_platform`: nur neutrale, anwendungsneutrale Reset-/Observation-
  Ports, falls der bestehende Vertrag sie nicht abdeckt;
- `device_platform_test_support`: deterministische Store-/Zeit-/Restart-
  Simulation, nie Produktionsabhängigkeit;
- betroffene `test/`-Bereiche für die in Abschnitt 11 definierten Orakel;
- kanonische Safety-/Acceptance-Dokumente nur bei dauerhaftem Vertragsbedarf.

Nicht vorgesehen sind Änderungen in `device_platform_esp_idf`, GPIO-
Treibern, NVS-/Flashadaptern, ESP-IDF-Resetcode oder fremden Issues ohne ein
neues, explizit freigegebenes Gate.

## 13. Dokumentation, Roadmap und Nachweise

In dieser Planrunde wird `docs/ROADMAP.md` auf die neue Issue-#24-Planrunde
und den Branch synchronisiert. Diese Plan-Datei ist die vollständige
Handlungsgrundlage; die Roadmap wiederholt weder Faultmatrix noch
Akzeptanzkriterien.

Der Draft-PR-Body muss nach dem Commit exakt nennen:

- Branch `agent/issue-24-safety-core-clean-restart`;
- Base `main` und `b8eae5f4da5f2666b5a9bda333d115254c4db5b2`;
- Planpfad dieser Datei;
- exakte Plan-Commit-SHA;
- die nicht normative Verwendung von PR #107 als Fehler-/Lernreferenz;
- offene Owner-Gates;
- `Implementation: NOT_STARTED` und `Tests/Builds: NOT_RUN`.

Der einzige aktuelle SESSION HANDOVER gehört in den neuen Draft-PR und nennt
dieselben Nachweise. Kanonische Dokumente erhalten keine Reviewverlaufsliste.

## 14. Planrunden-Abschluss und Stop-Kriterium

Vor dem Stop werden noch einmal lokal und live verifiziert:

1. Branch, lokaler `HEAD`, Remote-Branch-HEAD und `origin/main`;
2. PR-Base, Draft-Status, PR-Head und Planpfad;
3. exakte Plan-Commit-SHA im PR-Body und Handover;
4. Roadmap und Plan-Datei;
5. `git diff --check`, zulässige Dokumentations-/Architektur-/Secret-Gates;
6. keine Produktions- oder Testcodeänderung und keine ausgeführten Builds/
   Tests.

Danach lautet der Status:

```text
Implementation: NOT_STARTED
Tests: NOT_RUN
Builds: NOT_RUN
Hardware: NOT_RUN
Owner gate: WAITING_FOR_EXACT_PLAN_SHA_APPROVAL
```

Die Arbeit wird an diesem Punkt angehalten. Eine allgemeine Zustimmung oder
die Freigabe eines anderen PR-/Plan-Commits berechtigt nicht zur Umsetzung.
