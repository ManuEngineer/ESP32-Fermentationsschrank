# Issue #24 – Fehlerklassen, Verriegelung, SAFE_BOOT und Fehlerinjektion

## Planrevision 4 – PLAN_R4_PENDING_OWNER_APPROVAL

| Feld | Verbindlicher Stand |
|---|---|
| Issue | #24 – `[E3.5] Fehlerklassen, Verriegelung, SAFE_BOOT und Fehlerinjektion` |
| Draft-PR | #107 |
| Branch | `agent/issue-24-fehlerklassen-safe-boot-plan` |
| Owner-reviewter Ausgangsstand | `dc9e7772893d9c7d929085c4ba0a38f72bb5164a` |
| Base/main | `b8eae5f4da5f2666b5a9bda333d115254c4db5b2` |
| Vorheriger freigegebener Plan | `48f343ceb49d5a80239702241ae1fbf7d4ebfcd2` |
| R2/R3 | suspendiert beziehungsweise durch diese vollständige R4 ersetzt; kein historischer Plan ist implizite Quelle |
| Status | `PLAN_R4_PENDING_OWNER_APPROVAL` |
| Exakte Freigabe-SHA | SHA des vollständigen Dokumentationscommits dieser Revision |
| Produktionscode, Tests, Adapter, Build/Toolchain, Hardware | in dieser Planungsrunde unverändert |

Diese Datei ist die vollständige und eigenständige Implementierungsgrundlage
für R4. Alte R2-/R3-Texte sind nicht implizit Bestandteil dieser Fassung. Die
Ownerentscheidungen OD-24-01 bis OD-24-10 sowie die verbindlichen
Ownerentscheidungen C2-Rest, C3, C6 und C7 aus dem aktuellen Auftrag sind
vollständig eingearbeitet. R4 entscheidet weder eine physische
Aktor-Rückmeldung noch einen realen Y4-007-Produktions-Trigger. Der nächste
Schritt nach diesem Commit ist ausschließlich Ownerreview und Freigabe der
exakten SHA.

## 1. Live-Baseline und Planungs-Gate

Vor dieser Revision wurden Repository, Branch, Arbeitsbaum, `origin/main`,
Live-Issue #24, Draft-PR #107, der aktuelle freigegebene Plan, der neueste SESSION
HANDOVER, Root- und lokale `AGENTS.md`, `docs/AGENT_WORKFLOW.md`,
`docs/ENGINEERING_PRINCIPLES.md`, `docs/SPECIFICATION_REVIEW.md`,
`docs/DECISIONS.md`, ADR-013/014/018 sowie die betroffenen Safety-, Recovery-,
Persistenz-, Command- und Acceptance-Quellen geprüft.

Der Branch und PR-HEAD standen auf `dc9e7772893d9c7d929085c4ba0a38f72bb5164a`; der Arbeitsbaum war sauber,
`origin/main` stand auf `b8eae5f4da5f2666b5a9bda333d115254c4db5b2`, Issue #24 war offen und PR #107 war offen
und Draft. Der letzte Handover bestätigte die vier offenen Owner-Gates.
Der aktuelle Auftrag schließt diese Gates fachlich, ändert aber den
freigegebenen R3-Plan materiell: C2 ergänzt einen typisierten Software-
Readiness-Nachweis, C3 bindet den realen #17/#18-Ladepfad, C6 bindet die
reason-by-reason-Projektion des realen #22-Ergebnisses und C7 bestätigt
Contract-/Injection-only ohne Produktions-Trigger. Deshalb ist R4 eine
vollständige neue Planrevision; der eingefrorene R2-Produktionsstand wird nicht
zurückgebaut oder korrigiert.

Die maßgeblichen Fachquellen bleiben:

- `docs/SAFETY_AND_FAULTS.md` für Klassen, Codes und codebezogene Resetpolicy;
- `docs/SAFETY_COMPONENT_FAULTS.md` für Sensor-, Temperatur- und Aktorursachen;
- `docs/SYSTEM_SAFETY_AND_RECOVERY.md` für Restart-Episode, Boot und Recovery;
- `docs/RUN_COMMANDS.md` für den bestehenden #15-Commandpfad;
- `docs/CONFIGURATION_PERSISTENCE.md` und die öffentlichen #56/#57-Typen für
  Persistenz- und Konfigurationsresultate;
- `docs/ACCEPTANCE_TESTS.md` für historische R2-Nachweise und die bisherigen
  Zielorakel;
- `docs/ROADMAP.md` ausschließlich für Status und Gate.

## 2. Ziel, Architektur und Abgrenzung

Issue #24 erhält genau einen zentralen Safety-/Faultpfad. Er klassifiziert die
heute belegten Ursachen, setzt die sichere Aktorreaktion vor jede
Komfortanforderung, hält Latches und Restart-Evidenz über Neustarts,
projiziert auf den bestehenden #23-`ActuatorSafetyGateInput` und konsumiert die
echten öffentlichen #56/#57-Ergebnistypen an der realen
`FermentationApplication`-Grenze.

Der einzige Aktorpfad ist:

```text
#20/#21/#22/#23/#56/#57-Ergebnisse
  -> eine zentrale SafetyFaultService-Instanz
  -> ActuatorSafetyGateInput
  -> ActuatorPlanner
  -> ActuatorPlanSinkDriver
```

`Unknown`, `Unresolved`, fehlende Revision, unklare Persistenz und fehlende
Autorisierung sind niemals `Allowed`. Der reale Run-Ladepfad und das reale
#22-Ergebnis werden an genau dieser Application-Grenze einmalig in den
zentralen Safetypfad projiziert; weder #24 noch die Tests bilden #17/#18- oder
#22-Fachlogik ein zweites Mal nach.

Nicht Teil dieses Plans sind ESP-IDF-Resetmapping und `esp_restart()` (#29/E5),
reale NVS-/Flashadapter, Hardware-/GPIO-/Grenzwertfestlegung, #35-Commissioning,
die erneute Service-PIN-Verifikation, E4, #29/#90-Adapter, OTA oder eine
Capability-/Token-/Pointerarchitektur. Die notwendige native
`FermentationApplication`-/Boot-/Recovery-Komposition wird erweitert; daraus
folgt keine ESP-IDF-Reset- oder Hardwareadapterarbeit in `src/main.cpp` oder
`main/app_main.cpp`. ADR-013 trägt die Modulgrenzen; ADR-014 und ADR-018 tragen
die vorhandenen Determinismus- und #56/#57-Verträge. Ein neuer ADR ist nicht
erforderlich.

## 3. Verbindliche R4-Semantik

### 3.1 Restart-Episode (OD-24-01/02)

Release 1 verwendet eine offene Restart-Streak/Episode:

1. Der dritte abnormale Neustart innerhalb der offenen Episode erzwingt vor
   normaler Aktor-/Lauffreigabe `SAFE_BOOT`.
2. Die Episode schließt erst nach 30 Minuten durchgehend stabilem,
   abnormal-restartfreiem Betrieb.
3. Diese 30 Minuten werden ausschließlich mit einer monotonen Laufzeitquelle
   im laufenden Boot gemessen. NTP, RTC, Netzwerkzeit und Wall Clock sind keine
   Voraussetzung oder Ersatz.
4. Die 30 Minuten sind firmwarefest und nicht servicekonfigurierbar.
5. Ein normaler Neustart vor Episodenschluss schließt die Episode nicht. Ein
   abnormaler Neustart während der Stabilitätsphase verwirft die bisherige
   stabile Laufzeitbewertung; der nächste erfolgreiche Boot beginnt neu.
6. Stromlosigkeit löscht die offene Episode nicht, weil ohne
   stromausfallsichere Zeitbasis keine Power-off-Dauer bewiesen werden kann.
7. Ein kontrollierter Neustart ist nur abnormal, wenn die codebezogene
   Safety-/Software-Recovery-Policy ihn so klassifiziert. Ein autorisierter
   normaler Service-/Recovery-Neustart ist nicht automatisch abnormal.
8. Ein unbekannter Plattform-Resetgrund wird nicht als normal geraten, sondern
   fail-closed als `Y4-008` behandelt.
9. Ein normaler Neustart ist niemals ein `SAFE_BOOT`-Exit. Der Exit ist eine
   bewusste, autorisierte, codebezogene Entscheidung nach Persistenz-,
   Integritäts-, Sensor- und Aktorprüfungen.

Der Episodezustand wird im selben kanonischen Safetyrecord wie Latches und
Restart-Evidenz persistiert. Das ist kein Wall-Clock-Fenster über stromlose
Zustände.

### 3.2 Fehlercodes und Resetpolicy (OD-24-03/10)

Der sprachunabhängige Namensraum ist `P1-*`, `O2-*`, `S3-*`, `Y4-*`. Die
folgende Matrix ist vollständig. Sie unterscheidet reale heutige Producer,
deterministische #24-interne Ursachen und stabile Release-1-Code-/Injection-
Contracts für spätere qualifizierte Producer. Ein Contract-only-Code behauptet
keine heutige Hardwarediagnose; seine Safetyreaktion und Injektion bleiben
trotzdem Bestandteil des #24-Vertrags. Cleared-Historie ist keine aktive
Latchkapazität. Jede Zeile definiert Klasse, Producer/Ursache, Reaktion, Latch,
Auto-Rearm, Resetberechtigung, Reboot-/SAFE_BOOT-Policy sowie
Primary-/Follow-up-Fähigkeit.

`Reset` meint bewussten Faultreset, nicht Quittierung. Ein normaler Reboot ist
bei keinem Code ein Faultreset.

| Code – Producer/Ursache – aktive Identität | Sofortreaktion | Latch / Auto-Rearm | Resetberechtigung | Reboot / SAFE_BOOT-Exit | Primary / Follow-up |
|---|---|---|---|---|---|
| `P1-001` – bestehender Prozessautomat mit `ProcessRuntimeState::targetReachWarningIssued`, `TransitionReason::TargetReachTimeExceeded`, `ProcessMessage::TargetReachTimeExceeded` und `ProcessRunSnapshot::maximumTargetReachMinutes`; Run-Snapshot-/Prozesslauf-Identität | Warnung; nur bei vollständiger Safetyfreigabe fortsetzen | nein; neue gültige Prozessbewertung beendet die Meldung | kein Faultreset; Quittierung ohne Freigabeänderung | kein Reboot / kein Exit | primary; darf Folge einer Störung sein |
| `O2-001` – #20/#21 Produktfühler `STALE`/`FAILED`, gültiger Luftfallback; Produkt-Sensorrolle | Peltier aus; nur validierte #21-Ersatzstrategie | kein persistenter Latch; nur #21-Policy darf rearmen | Bediener-/#21-Reset nach Ursachefreiheit und Checks | kein Reboot / kein Exit | primary; Eskalation bleibt separat |
| `O2-002` – #20/#21 kurzzeitig `STALE` in einer Sicherheitsrolle; Sensorrolle | Peltier aus, Nachlauf, begrenztes Wiedererkennungsfenster | kein Latch; Rückkehr nur nach stabilen Messungen vor `FAILED` | während Wiedererkennung kein Reset | kein Reboot / kein Exit | primary; S3-Eskalation bleibt |
| `S3-001` – #20 Schrankluftsensor `FAILED`; Sensorrolle | Peltier/H-Brücke aus, sicherer Nachlauf | persistenter Latch; kein Auto-Rearm | Serviceautorisierung nach stabiler Sensor- und Safetyprüfung | kein zusätzlicher Reboot / kein Exit durch Reset | primary |
| `S3-002` – #20 Außenwärmetauscher-/Kühlkörpersensor `FAILED`; Sensorrolle | Peltier/Richtungen aus, sichere Wärmeabfuhr | persistenter Latch; kein Auto-Rearm | Serviceautorisierung nach Sensor-/Aktorprüfung | kein zusätzlicher Reboot / kein Exit durch Reset | primary |
| `S3-003` – stabiler #24-Release-1-Contract-/Injection-Code für einen anhaltenden oder sicherheitsrelevanten Sensorwiderspruch; späterer qualifizierter Plausibilitäts-/Commissioning-Producer übernimmt denselben Code; Rollen-/Plausibilitätskorrelation | Peltier aus, kein Fallback | persistenter Latch; kein Auto-Rearm | Serviceautorisierung nach Ursachefreiheit und Plausibilitätsnachweis | kein zusätzlicher Reboot / kein Exit durch Reset | primary |
| `S3-004` – stabiler #24-Release-1-Contract-/Injection-Code für Sicherheits-Eingriffsgrenze; späterer qualifizierter #35-Producer; eine obere/untere Grenzkorrelation | Leistung aus, Richtung sperren, Impuls/Integrator verwerfen | persistenter Latch; keine produktive Auto-Recovery; #35 fehlt => `Unresolved` | Serviceautorisierung nach Checks; Reset löscht nicht automatisch und erzeugt keine Recovery | kein zusätzlicher Reboot / kein Exit durch Reset | primary; Recovery bleibt Folge |
| `S3-005` – stabiler #24-Release-1-Contract-/Injection-Code für harte thermische Notgrenze; späterer qualifizierter #35-/Hardware-Producer; eine obere/untere Grenzkorrelation | sofort aus, keine Gegenrichtung, sichere Lüfterstrategie | persistenter Latch; kein Auto-Rearm | technische Serviceautorisierung nach Grenz-/Hardwareprüfung | kein zusätzlicher Reboot / kein Exit durch Reset | primary |
| `S3-006` – stabiler #24-Release-1-Contract-/Injection-Code für Außenlüfterfehler; reale Fan-Diagnose folgt dem zuständigen späteren Hardware-/Commissioning-Gate; Außenlüfterrolle | Peltier aus, Restwärme fail-closed behandeln | persistenter Latch; kein Auto-Rearm | Serviceautorisierung nach Fan-/Ausgangsprüfung | kein zusätzlicher Reboot / kein Exit durch Reset | primary |
| `S3-007` – stabiler #24-Release-1-Contract-/Injection-Code für Innenlüfterfehler; reale Fan-Diagnose folgt dem zuständigen späteren Hardware-/Commissioning-Gate; Innenlüfterrolle | Peltier aus oder richtungsbezogen sperren | persistenter Latch; kein Auto-Rearm | Serviceautorisierung nach Fan-/Ausgangsprüfung | kein zusätzlicher Reboot / kein Exit durch Reset | primary |
| `S3-008` – #23 `ActuatorWatchdogFaultEvidence`; Planner-/Diagnose-Evidenz | Peltier aus, Safety-Gate stop, vollständige 64-bit-Evidenz sichern | persistenter Latch; kein Auto-Rearm | technische Autorisierung nach Ursachefreiheit, Sensorcheck, typisiertem #23-Software-Aktorcheck und Integritätsprüfung | standardmäßig kein Reboot / kein Exit durch Reset | primary; Folgefehler bleiben |
| `S3-009` – stabiler #24-Release-1-Contract-/Injection-Code für H-Brücke/Strom/Ausgang/Richtung; reale Diagnose folgt dem zuständigen späteren Hardware-/Commissioning-Gate; ein Output-Domain-Schlüssel | beide Richtungen aus, Plan verwerfen | persistenter Latch; kein Auto-Rearm | technische Serviceautorisierung nach Ausgangsprüfung | kein zusätzlicher Reboot / kein Exit durch Reset | primary |
| `Y4-001` – #56 `ConfigurationRuntimeFailure`; eine aktive Konfigurationsrevision | keine neue Konfiguration, Aktoren sicher stoppen | persistenter System-Latch; kein Auto-Rearm | Service/technisch nach Konfigurations-, Persistenz- und Integritätschecks | kein zusätzlicher Reboot / kein Exit durch Reset | primary |
| `Y4-002` – #56/#57 `ConfigurationCommitIndeterminate`/`CommitOutcomeUnknown`; eine aktive Commit-Korrelation | unklare Revision sperren, Aktoren aus | persistenter System-Latch; kein Auto-Rearm | Service/technisch erst nach eindeutigem Status und neuer Revision | kein zusätzlicher Reboot / kein Exit durch Reset | primary |
| `Y4-003` – #57 `ConfigurationUnavailable`; eine Recovery-/Graph-Quelle | keine Teilkonfiguration verwenden, Aktoren aus | persistenter System-Latch; kein Auto-Rearm | Service/technisch nach gültiger Revision | kein zusätzlicher Reboot / kein Exit durch Reset | primary |
| `Y4-004` – #57 `ConfigurationIntegrityFailure`; eine Graph-/Konfigurationsdomäne | Integritätsfehler fail-closed, Aktoren aus | persistenter System-Latch; kein Auto-Rearm | Service/technisch nach Integritätsprüfung | kein zusätzlicher Reboot / kein Exit durch Reset | primary |
| `Y4-005` – realer einmaliger #17/#18-`loadAndInitialize()`-Pfad erkennt einen kritischen aktiven Lauf als nicht rekonstruierbar | Peltier/H-Brücke aus, Lauf sicher beenden | persistenter System-Latch; kein Auto-Rearm | Service/technisch nach neuer gültiger Laufrevision | kein zusätzlicher Reboot / kein Exit durch Reset | primary |
| `Y4-006` – #24 Safety-State-Read/Write/Capacity/Integrity; ein globaler Basisrecord-Marker | RAM-Latch und Aktorsperre sofort, Marker im selben Record versuchen | persistenter System-Latch/Overflowmarker; kein Auto-Rearm, keine Eviction | Service/technisch nach Read-/Write-/Capacity-/Integritätsprüfung | kein zusätzlicher Reboot; Exit nur separate Bootpolicy | primary; aktive Slotliste wird nicht verfälscht |
| `Y4-007` – stabiler #24-Contract-/Injection-Code für einen internen Safety-/Recoveryfehler; eine SafetyService-Evidenz | Aktoren aus, Recoveryevidenz sichern, höchstens den einen zulässigen kontrollierten Restart vorbereiten | persistenter System-Latch; kein Auto-Rearm | technische Autorisierung nach Ursachefreiheit und Integritätsprüfung | genau ein automatischer kontrollierter Restart pro aktiver Recoveryursache/-episode; danach kein zweiter automatischer Restart | primary; S3-008 bleibt separat |
| `Y4-008` – unbekannter Resetgrund, fehlende/doppelte/mismatched App-Evidenz oder unbekannter Safetyinput; eine Bootbeobachtung | `Allowed` verbieten, Aktoren aus, fail-closed persistieren | persistenter System-Latch; kein Auto-Rearm | bis geklärter Ursache/Evidenz verboten | kein Reboot als Lösung / kein Exit ungeklärt | immer primary |
| `Y4-009` – dritter abnormaler Restart / offene SAFE_BOOT-Episode; eine Episode | vor Aktor-/Lauffreigabe `SAFE_BOOT` | persistente Episode-/Systemverriegelung; kein Auto-Rearm durch Reboot | bewusster autorisierter codebezogener Exit nach allen Checks | kein automatischer zusätzlicher Reboot; ein technischer Restart ist nur Policy des zugrunde liegenden Codes | primary; auslösende Latches bleiben |

Die Matrix enthält 21 stabile Codes: 1 P1-, 2 O2-, 9 S3- und 9 Y4-Codes. Sie
führt keine historische Lücke und keine Zukunftsfunktion für #19 ein.
`Unknown`/`Unresolved` wird nicht in eine harmlose Klasse normalisiert.
`P1-001` wird ausschließlich aus der bestehenden `ProcessMessage`-Erzeugung
des Prozessautomaten projiziert; #24 berechnet weder eine eigene
`maximumTargetReachMinutes`-Zeitlogik noch einen Fault aus dem normalen
`TemperatureControlReason::AirLimitReduced` oder `AirLimitBlocked`. Beide sind
normale #22-Regelzustände und allein weder P1 noch O2; ein separater Fault
entsteht nur aus einer eigenen realen Fehlerursache. Bei O2 wird immer der
ursprüngliche #22-Reason beziehungsweise der vorhandene #20/#21-Sensorbefund
verwendet; #23 `NoValidRequest` ist kein zusätzlicher Faultproducer und darf
auch in der #22-Projektion keine zusätzliche Fehlerursache werden.

`ThermalCompatibility::Incompatible` aus einem strukturell gültigen #21-
`CrossRolePlausibilityContext` ist kein direkter `S3-003`-Producer. Der
bestehende #21-Pfad darf damit bei ansonsten gültiger Evidenz die
`ReturnValidationPending`-Rückkehr abbrechen und `AirFallbackActive` beibehalten;
allein daraus entsteht kein S3-Latch. `S3-003` bleibt für die explizite
reproduzierbare Injection eines anhaltenden oder sicherheitsrelevanten
Sensorwiderspruchs reserviert, bis ein späterer qualifizierter Producer diese
stärkere Semantik typisiert liefert.

### 3.3 S3-004 contract-only und firmwarefeste Obergrenze (OD-24-05)

S3-004 setzt sofort `ImmediateStop` und bleibt gelatcht. Ohne qualifizierte
#35-Commissioningrevision ist `SAFETY_RECOVERY` `Unresolved` und damit
deaktiviert/fail-closed. Der bestehende #23-`ActuatorSafetyGateInput`-,
Planner- und Sinkpfad bleibt die einzige Aktorgrenze; ein normales `Allowed`
kann den Zustand nicht umgehen. Eine spätere Recovery löscht den S3-004-Latch
nicht automatisch.

Die kanonische Safetygrenze bleibt erhalten: maximal zwei Recoveryversuche,
Werkseinstellung zunächst ein Versuch, zulässiger Maschinenparameter `0..2`.
#35 darf später einen Wert innerhalb dieser Grenze validieren; #24 aktiviert
keinen Runtimewert und erfindet keine Leistung, Pulsdauer, Trend-, Temperatur-
oder Revisionswerte.

### 3.4 Resetvertrag (OD-24-06/07)

Der bestehende #15-Commandpfad wird minimal migriert:

- `FaultResetRequest` enthält nur `CommandEnvelope`, exaktes Faultziel und die
  erwartete Faultrevision, sofern sie nicht im Envelope eindeutig enthalten ist;
- `FaultResetEvaluation` bleibt fachliche Ergebnisprojektion und wird zentral
  aus Fault-/Latchzustand, Ursachenfreiheit, Sensor-/Aktor-/Persistenz-/Integritäts-
  checks, blockierenden Faults, codebezogener Policy und typisierter vorhandener
  Autorisierungsevidenz berechnet;
- positive Caller-Felder, `authorizationSatisfied=true`, Pointer, Tokens und
  Capabilityobjekte als Bypass entfallen;
- echte Service-PIN-Verifikation wird in #24 nicht implementiert. Eine schmale
  interne Autorisierungsevidenz genügt als Übergabevertrag; fehlend oder nicht
  passend ist fail-closed. E4 ist keine #24-Abhängigkeit.

### 3.5 Neutraler Resetport (OD-24-08)

Der bestehende Branch-Port wird nach Ownerfreigabe korrigiert, weil seine
aktuelle Fassung mit `AuthorizedRestart`, `ControlledSafetyRestart` und
`AuthorizedFaultReset` noch nicht anwendungsneutral ist. Zielsemantik in
`device_platform`:

- `ResetCause`: `PowerOn`, `SoftwareRestart`, `WatchdogOrPanic`, `Brownout`,
  `ExternalOrOther`, `Unknown`;
- bootlokal stabile `ResetCauseSnapshot`-Beobachtung mit `observationId`;
- generisches `requestRestart()` mit kleinstmöglichem neutralem
  Annahme-/Ablehnungs-/Outcome-Unknown-Resultat;
- keine Faultcodes, Fermentationsbegriffe, Service-/Safetyabsichten,
  Restart-Episode oder SAFE_BOOT-Regel im Port;
- native Testbarkeit bleibt Pflicht; ESP-IDF-Mapping und `esp_restart()` bleiben
  #29/E5.

Die fachliche Software-Rebootbedeutung wird ausschließlich in
`fermentation_app` bestimmt. Vor dem Neustart persistiert #24 eine
`RestartIntentEvidence` mit Typ `AutomaticSafetyRecovery`,
`AuthorizedTechnicalRestart` oder `AuthorizedSafeBootExit`, Ziel-Fault/-Revision,
Episode- und Evidenzrevision sowie Zustand. Plattformursache
`SoftwareRestart` plus exakt passende, noch nicht konsumierte Evidenz darf
klassifizieren; fehlende, widersprüchliche oder doppelt konsumierte Evidenz
führt zu `Y4-008`. Die Evidenz wird exactly once konsumiert. Kein ESP-IDF-Adapter
darf die Applicationabsicht aus einer Hardwareursache erfinden.

### 3.6 Einmaliger kontrollierter Restart

Für dieselbe aktive Safety-/Software-Recoveryursache in derselben Episode ist
höchstens ein automatischer kontrollierter Neustart zulässig. Vor dem Aufruf
von `requestRestart()` muss die zugehörige Evidenz im kanonischen Safetyrecord
write-before-apply als dauerhaft bestätigt gespeichert sein. Bei
`OutcomeUnknown`, fehlender Readback-Bestätigung oder erneutem Request wird
nicht automatisch wiederholt; der Zustand bleibt fail-closed und eskaliert
codebezogen.

Ein autorisierter technischer Service-Neustart ist eine getrennte Evidenz und
zählt nicht automatisch als abnormal. Ein Neustart ist niemals selbst ein
Faultreset.

`Y4-007` definiert den einen automatischen Recovery-Restart, nicht einen
zweiten Neustart nach späterem Faultreset. Nach erfolgreichem Restart und
bestandenen Service-/Integritätschecks wird kein weiterer Reboot verlangt.
`Y4-009` verlangt für SAFE_BOOT-Exit selbst keinen Reboot. Nur ein konkreter
zugrunde liegender Fehler darf einen eigenen technischen Neustart als seine
einmalige Policy verlangen; ein globaler `FaultResetBootIntent` wird nicht
eingeführt.

### 3.7 Verbindliche Ownerentscheidungen C2-Rest, C3, C6 und C7

Diese vier Entscheidungen sind Bestandteil dieses vollständigen R4-Vertrags:

**C2 – S3-008 Software-Aktorbereitschaft.** Der S3-008-Faultreset wird nur
über den bestehenden #23-`ActuatorPlanner` und den bestehenden
`ActuatorPlanSinkDriver` vorbereitet. Der Resetpfad erzeugt über die vorhandene
Planner-Stopoperation einen typisierten sicheren AUS-Plan und reicht ihn über
den Sink weiter. Der Nachweis ist ausschließlich Software-Readiness und
enthält keine Strom-, H-Brücken-, Lüfter- oder sonstige physische
Rückmeldung. Er ist nur vollständig, wenn alle folgenden typisierten
Ergebnisse gleichzeitig vorliegen:

1. die aktuelle #23-Watchdog-Episode ist beendet und es wurde im Readiness-
   Schritt keine neue Watchdogursache beobachtet; die persistierte
   `ActuatorWatchdogFaultEvidence` bleibt als historische Fault-Evidenz
   unverändert erhalten;
2. der Planner-Zustand ist nach seiner bestehenden Zustandsrepräsentation
   konsistent: kein aktiver Plan, keine aktive physische Peltier-Richtung,
   kein gebundener Akkumulator-/Gegenrichtungszustand und kein offener
   Watchdog-Heartbeat; unbekannte oder widersprüchliche Felder sind nicht
   bereit;
3. der bestehende Planner erzeugt einen sicheren AUS-Plan mit bekannter
   `Idle`-Richtung und zulässiger Stop-/Nachlaufsemantik;
4. der bestehende Sinkpfad akzeptiert genau diesen sicheren Plan. Eine
   erfolgreiche Ausführung der vorhandenen Sinkoperationen wird als
   typisierte Software-Annahme des Sinkpfads protokolliert, niemals als
   Hardware-Rückmeldung.

Fehlt Planner, Sink, Readinessstatus, Konsistenz, Ursachenfreiheit oder
Übereinstimmung zwischen Plan und Sinkannahme, bleibt der Reset fail-closed.
Die SafetyFaultService bleibt die einzige Resetautorität; es gibt keine zweite
Aktor-Safetylogik und keinen direkten Sink- oder Planner-Bypass. Die
write-before-apply-Reihenfolge bleibt erhalten: erst Safetyrecord-Commit, dann
das Aufheben des #23-Watchdog-Latches.

**C3 – realer #17/#18-Producer.** `FermentationApplication` erhält in seiner
bestehenden Boot-/Recovery-Komposition eine typisierte Referenz auf den
vorhandenen `RunPersistenceCoordinator`. `loadAndInitialize()` wird in diesem
Boot genau einmal aufgerufen. `NoPersistedRun` und `NoActiveRun` bleiben gültige
leere Zustände. `Current` und `FallbackRecovered` werden ausschließlich über
`restoreRunPersistenceSnapshot()` in den vorhandenen Laufzustand übernommen;
eine fehlende oder inkonsistente Wiederherstellung ist nicht rekonstruierbar.
`PreparedInterrupted`, `NotReconstructible`,
`NotReconstructibleOrphanedState`, `ReadFailed`, `CapacityExceeded`,
`UnsupportedSchema` und `ForeignEpoch` werden im kritischen aktiven
Wiederanlaufpfad fail-closed als `Y4-005` an dieselbe zentrale
`SafetyFaultService` projiziert. Ein erfolgreich rekonstruierter aktiver Lauf
erzeugt kein `Y4-005`. Die Application exponiert nur den typisierten
Lade-/Wiederherstellungszustand; sie dupliziert weder #17/#18-Codec,
Recoveryentscheidung noch Faultlogik. Bei `Y4-005` bleiben die Aktoren über
den bestehenden #23-Gatepfad gesperrt.

**C6 – reale #22-Reason-Projektion.** Der echte
`TemperatureControlResult` wird am vorhandenen
`TemperatureControlApplicationOrchestrator`-Handoff genau einmal an die
zentrale `SafetyFaultService` übergeben. Die Projektion verarbeitet die
kanonischen Reasons einzeln und verwendet nur vorhandene typisierte
Begleitevidenz:

| #22-Reason | #24-Projektion |
|---|---|
| `None`, `NeutralBand`, `Saturated` | normaler #22-Regelzustand; kein Fault |
| `AirLimitReduced`, `AirLimitBlocked` | normaler #22-Luftbegrenzungszustand; allein kein Fault und kein P1/O2-Fault |
| `SensorUnavailable`, `InvalidSample` | nur mit der gleichzeitig vorliegenden #20-`SensorQualitySnapshot` und der #21-Regelsensorrolle auf O2/S3; fehlende oder widersprüchliche Sensorbegleitevidenz -> Y4-008 |
| `NoCommissioning`, `InvalidConfiguration` | keine neue #24-Konfigurationslogik; die vorhandene typisierte #56/#57-/Y4-Grenze wird verwendet, und ohne passende Producer-Evidenz bleibt die Projektion fail-closed |
| `TimeInvalid`, `RequestIdentityExhausted` | keine erfundene Sensor- oder Aktorursache; sicherheitsrelevante Unklarheit -> bestehender Y4-008-Unknown-Pfad |

`#23 NoValidRequest` wird ausschließlich als Planner-/Annahmezustand
behandelt und niemals als zusätzliche oder alternative Fehlerursache in diese
Matrix aufgenommen. `AirLimitReduced` und `AirLimitBlocked` erzeugen allein
keinen Fault. Eine zweite Reason-Matrix außerhalb dieser zentralen Projektion
ist unzulässig.

**C7 – Y4-007 Contract-only.** R4 führt keinen künstlichen Produktions-Trigger
für `Y4-007` ein. Der Code bleibt der stabile Contract-/Injection-Code für
einen internen Safety-/Recoveryfehler. Persistenter Latch, fail-closed-
Wirkung, technische Autorisierung nach Ursachefreiheit und Integrität, genau
ein automatischer kontrollierter Restart pro aktiver Ursache/Episode und
deterministische Restart-Evidenz werden unverändert erhalten und gezielt
regressionsgetestet. Ein späterer realer Producer darf erst eingeführt werden,
wenn eine konkrete typisierte interne Recoveryfehler-Ursache existiert; das
erfordert eine eigene fachliche Prüfung und eine neue Plan-/Ownerfreigabe.

## 4. Aktive Producer-Worst-Case-Analyse und Capacity

### 4.1 Aktive Instanzen

Cleared-Historie und reine Journalereignisse zählen nicht. Die Bound wird aus
gleichzeitig aktiven unabhängigen Instanzen abgeleitet:

| Code | Max. aktive Instanzen | Producer/Rolle/Identität | Mehrfachinstanz und Koexistenz |
|---|---:|---|---|
| `S3-001` | 1 | realer #20-`SensorQualitySnapshot`, Schrankluft-Sensorrolle | eine Rolle, ein SourceKey |
| `S3-002` | 1 | realer #20-`SensorQualitySnapshot`, Außenwärmetauscher-/Kühlkörper-Sensorrolle | eine Rolle, ein SourceKey |
| `S3-003` | 1 | stabiler #24-Contract-/Injection-Code für einen anhaltenden oder sicherheitsrelevanten Sensorwiderspruch; späterer qualifizierter Plausibilitäts-/Commissioning-Producer | ein aktueller Korrelationsfall; keine parallelen Konfliktinstanzen im Producer |
| `S3-004` | 1 | stabiler #24-Contract-/Injection-Code; späterer qualifizierter #35-Grenzproducer | obere und untere Grenze sind bei einer einzelnen Prozessgröße gegenseitig exklusiv; der Latch bleibt aber beim späteren S3-005 bestehen |
| `S3-005` | 1 | stabiler #24-Contract-/Injection-Code; späterer qualifizierter #35-/Hardware-Grenzproducer | gleiche physikalische Grenze; kann zusätzlich zum bestehenden S3-004 aktiv werden |
| `S3-006` | 1 | stabiler #24-Contract-/Injection-Code; spätere Fan-Diagnose-/Commissioning-Evidenz | eine Außenlüfterrolle; kein zweiter Fan-Producer |
| `S3-007` | 1 | stabiler #24-Contract-/Injection-Code; spätere Fan-Diagnose-/Commissioning-Evidenz | eine Innenlüfterrolle; kein zweiter Fan-Producer |
| `S3-008` | 1 | #23 `ActuatorWatchdogFaultEvidence` | ein Planner-/Aktor-Watchdog; Wiederholungen derselben Evidenz sind dieselbe Instanz |
| `S3-009` | 1 | stabiler #24-Contract-/Injection-Code; spätere H-Brücken-/Strom-/Ausgangsdiagnose | ein Output-Domain-Schlüssel; keine künstliche Vervielfachung |
| `Y4-001` | 1 | #56 aktive `ConfigurationRuntimeFailure` | eine serialisierte Konfigurationsrevision |
| `Y4-002` | 1 | #56/#57 Commit-Korrelation | Mutation Coordinator liefert keine parallelen aktiven Commitvorgänge |
| `Y4-003` | 1 | #57 `ConfigurationUnavailable` | eine Recovery-/Graph-Quelle |
| `Y4-004` | 1 | #57 `ConfigurationIntegrityFailure` | eine Graph-/Konfigurationsdomäne |
| `Y4-005` | 1 | realer einmaliger #17/#18-`loadAndInitialize()`-Producer für einen nicht rekonstruierbaren kritischen aktiven Lauf | ein aktiver Lauf |
| `Y4-006` | 1 | globaler #24-Basisrecord-/Overflowmarker | kein variabler Slot; ein Marker aggregiert den globalen Capacity-/Persistenzzustand |
| `Y4-007` | 1 | stabile zentrale `SafetyFaultService`-Contract-/Injection-Evidenz; kein Produktions-Producer in R4 | genau eine zentrale Safetyinstanz |
| `Y4-008` | 1 | eine bootlokale Reset-/Evidenzbeobachtung | Dubletten werden nicht neu gezählt |
| `Y4-009` | 1 | eine offene Restart-Episode | globaler Episodenzustand |

S3-004 und S3-005 sind deshalb nicht exklusiv: S3-004 wird bei S3-005 nicht
gelöscht. Die neun S3-IDs ergeben neun aktive S3-Instanzen. Von den neun
Y4-Zuständen sind acht normale Faultinstanzen variable Slot-Latches;
`Y4-006` ist ausschließlich der außerhalb der Slotliste persistierte
Basisrecord-Marker. Die maximale variable Slot-Bound ist daher:

```text
S3 worst case:              9 variable Latches
Y4 worst case:              8 variable Latches + 1 Basisrecord-Marker
aktive Slot-Latches:       17
darstellbare Bedingungen:  18 einschließlich Y4-006-Marker
Cleared-Historie:           nicht enthalten
```

Die Slotzahl `17` ist aus der vollständigen R4-Code-/Injection-Matrix und den heute
belegten Rollen abgeleitet und nicht aus R2 übernommen: neun S3-Latches plus
acht nicht-markerartige Y4-Latches. Die Contract-only-S3-Codes werden für den
konservativen Injektions-Worst-Case jeweils mit einer unabhängigen Instanz
gezählt. Ein späterer realer Producer übernimmt exakt denselben Code und die
zugehörige Rollen-/Korrelationsidentität; er fügt keinen zusätzlichen Slot
hinzu. Eine später nachgewiesene Mehrfachinstanz derselben Rolle wäre vor
Implementierung eine materielle R4-Abweichung und dürfte nicht still in diese
Bound gedrückt werden.

### 4.2 Neuer R4-Record und Byte-Nachweis

Die 80-Byte-/48-Byte-Werte aus R2 sind kein R4-Vertrag und werden nicht zur
Begründung verwendet. R4 definiert stattdessen einen neuen festen,
sprachunabhängigen Safetyrecord:

- Basisrecord: 128 Byte, einschließlich Schema-/Record-/Faultrevision,
  Instanzsequenz, Slotanzahl, `safeBootRequired`, Episodezähler und
  Stabilitätsstatus, letzter neutraler Resetbeobachtung, Restart-Evidence und
  dem Capacity-/Overflowmarker;
- aktiver Slot: 64 Byte, einschließlich Code/Klasse/Status/Disposition,
  Source-/Correlation-/Instance-Identität, monotonem Entstehungsbezug,
  Faultrevision, Primary-Bezug, Diagnose-High-Watermark und
  `automaticRecoveryRestartUsed`;
- keine Strings, keine Wall-Clock-Voraussetzung, keine unbounded Historie und
  keine Cleared-Historie im Safetyrecord;
- kein UTC-Feld im Safetyrecord. Der vorhandene anwendungsneutrale
  `StorageEnvelope` Version 1 aus `lib/device_platform/src/storage_envelope.hpp`
  trägt ohne optionales UTC einen 33-Byte-Header vor CRC plus 4-Byte-CRC, also
  37 Byte. Diese generische Envelopegröße ist unabhängig vom suspendierten
  R2-Anwendungsrecord.

Damit ergibt sich bewusst für R4:

```text
Payload = 128 + 17 * 64 = 1.216 Byte
Record  = 37 + 1.216   = 1.253 Byte
Application-Limit       = 2.048 Byte
Reserve                 =   795 Byte
```

`2.048` ist kein globales `IStateStore`-Limit. R4 wählt es als
application-spezifisches `maxBytes` für genau diesen Safetyrecord: Es hält die
Kodierung fest und bounded, vermeidet dynamische/unbounded Latchdaten, lässt
795 Byte Prüf- und Evolutionsreserve und bleibt im 4-MB-/No-PSRAM-Ziel. Das ist
ein Speicher-/Stack-/Testbarkeitsbudget, kein Hardwarebeweis. Die spätere
Implementierung muss die Felder mit festen Breiten statisch gegen
`37 + 128 + 17*64 <= 2048` prüfen und den tatsächlichen
`TBD_IMPLEMENTATION_BUDGET`-Bericht liefern. `Y4-006` wird bei voller
Slot-Bound zusätzlich im Basisrecord markiert und nicht als 18. Slot
materialisiert.

### 4.3 Capacity-/Persistenz-Fail-closed

Der Capacityzustand benötigt keinen freien Latchslot:

- der Basisrecord enthält immer einen bounded
  `capacityFailureLatched`-/Overflowmarker mit Fehlerart, Markerrevision,
  Source-/Correlation-Identität und `safeBootRequired`;
- bei voller Slotbound wird kein aktiver Latch verdrängt, zusammengelegt oder
  als Cleared markiert. Die neue unabhängige Ursache setzt RAM-Safetywirkung
  sofort und aktualisiert den Basismarker; `Y4-006` ist dessen Projektion, kein
  zusätzlicher Slot;
- der Marker wird zusammen mit dem normalen Safetyrecord versucht zu
  persistieren. Es gibt keinen zweiten Store und keine zweite Faultplattform;
- `WriteError`, `CapacityError`, `ReadError`, Korruption, Mismatch und
  `CommitOutcomeUnknown` bleiben in RAM und beim Boot fail-closed. Ein späterer
  isolierter Write oder ein normaler Reboot deutet den unsicheren Lauf nicht
  nachträglich als sicher;
- bei vorhandenem Overflowmarker ist Boot deterministisch `SAFE_BOOT`, bis ein
  bewusster codebezogener Servicepfad nach fehlenden aktiven Ursachen,
  Capacityfreiheit und vollständiger Readback-/Integritätsprüfung einen Reset
  erlaubt;
- jede Revision, Instance-ID und Slotanzahl wird vor Hochzählen auf Overflow
  geprüft.

## 5. Boot, Restart-Evidence und Application-Grenze

### 5.1 Bootablauf

```text
Boot
  -> alle Aktorausgänge sicher AUS
  -> IResetController genau einmal bootlokal beobachten
  -> neutralen Resetgrund nicht als Applicationabsicht interpretieren
  -> Safetyrecord, Latches, Marker und Restart-Evidence validieren
  -> Application-Evidence exakt einmal gegen Ursache/Revision/Episode abgleichen
  -> Episode fortschreiben; beim dritten abnormalen Restart SAFE_BOOT setzen
  -> reale #56/#57-Ergebnisse über eine Gateinstanz konsumieren
  -> Safetyprojektion auf ActuatorSafetyGateInput erzeugen
  -> nur vollständig geklärtes `Allowed` an Planner/Sink weitergeben
```

Die Plattformbeobachtung wird nicht pro wiederholtem Tick neu gezählt. Eine
`observationId` ist bootlokal stabil; dieselbe Beobachtung ist kein zweiter
Restart. Software-Recovery-Evidenz wird vor dem Restart write-before-apply
gesichert und nach passender Plattformursache exactly once konsumiert.

### 5.2 Reale #56/#57-Integration (OD-24-09)

Der minimale konkrete Integrationspunkt liegt in
`lib/fermentation_app/src/fermentation_application.hpp/.cpp`, direkt unter
`FermentationApplication`, und verwendet die vorhandene
`configuration_safety_integration_gate.hpp/.cpp`-Grenze:

1. `FermentationApplication` besitzt/bindet genau eine
   `SafetyFaultService`-Instanz und genau eine
   `ConfigurationSafetyIntegrationGate`-Instanz.
2. Boot konsumiert den echten `ConfigurationRecoveryResult`; laufende #56-
   `ConfigurationCommitResult`-/Status- und #57-Recoveryresultate werden über
   dieselbe Gateinstanz weitergereicht.
3. Der Safetykern projiziert den Gesamtzustand auf den bestehenden
   `ActuatorSafetyGateInput`.
4. `ActuatorPlanner` und `ActuatorPlanSinkDriver` bleiben der einzige
   Aktorpfad. Kein normales `Allowed` kann die Safetyprojektion umgehen.
5. Native End-to-End-Tests instanziieren diesen echten Applicationpfad mit den
   öffentlichen #56/#57-Typen; ein test-only Ersatzmapper gilt nicht.

Die ESP-IDF-/Hardware-Root-Skeletons bleiben ohne neue Adapter unverändert; die
native `FermentationApplication`-Komposition wird nur um die drei vorhandenen
fachlichen Übergaben für #17/#18, #22 und den bestehenden #23-Planner-/Sinkpfad
ergänzt. #24 erzeugt keine pauschale #29/#90-Abhängigkeit. E5 wiederholt
denselben Nichtumgehungsvertrag mit realen Adaptern.

### 5.3 Einmaliger #17/#18-Wiederanlauf (C3)

`FermentationApplication::begin(..., SafetyDependencies, ...)` bindet den
vorhandenen `RunPersistenceCoordinator` als erforderliche Bootabhängigkeit für
den produktiven Recoverypfad. Vor einer normalen Application-Bereitschaft wird
exakt einmal `loadAndInitialize()` aufgerufen. Der Status wird unverändert
typisiert weitergegeben. Nur `Current`/`FallbackRecovered` mit erfolgreicher
`restoreRunPersistenceSnapshot()`-Projektion dürfen einen aktiven Lauf in der
Application repräsentieren. Alle kritischen, nicht rekonstruierbaren
Loadstatus werden über eine einzige neue, schmale
`SafetyFaultService`-Producerfunktion als `Y4-005` projiziert; der Coordinator
bleibt Eigentümer seiner gesamten Persistenz- und Recoverylogik. Ein leeres
oder erfolgreich als `NoActiveRun` geladenes System erzeugt keinen Y4-005.

Der Fehlerfall setzt keine neue Lauf- oder Persistenzlogik in #24 ein. Die
Application bleibt für Safetyzwecke gesperrt, und der vorhandene
`ActuatorSafetyGateInput` erreicht den Planner weiterhin nur fail-closed.

### 5.4 Reale #22-Projektion (C6)

`TemperatureControlApplicationOrchestrator::evaluateTemperatureControl()`
bleibt die einzige Application-Grenze für das echte #22-Ergebnis. Nach der
Berechnung und vor der späteren Plannerübergabe reicht derselbe
`TemperatureControlResult` gemeinsam mit den bereits verwendeten
#20-Snapshots und der aufgelösten #21-Regelsensorrolle an
`SafetyFaultService::consumeTemperatureControlResult()` weiter. Die Methode
projiziert keine Quote, keine Air-Limit-Grenze und keinen Plannerzustand; sie
ordnet nur die bereits erzeugte typisierte Reason nach der C6-Matrix zu. Ein
fehlender oder widersprüchlicher Begleitzustand führt nicht zu einem
Normalpfad.

### 5.5 C2-Resetübergabe über #23

Für einen S3-008-Reset bindet der bestehende Resetaufruf zusätzlich den
`ActuatorPlanSinkDriver`. `SafetyFaultService::resetFault()` erzeugt dort über
`ActuatorPlanner::forceStop()` den sicheren AUS-Plan, übergibt ihn genau einmal
an den Sink und prüft den typisierten Software-Annahme-/Konsistenznachweis,
bevor der persistierte Faultcore geändert wird. Nur nach erfolgreichem
Safetyrecord-Commit wird `applyExternalWatchdogFaultReset()` aufgerufen. Alle
anderen Faultcodes behalten ihren bisherigen Resetvertrag; fehlende
Planner-/Sinkbindung bei S3-008 ist eine Ablehnung.

## 6. Konkreter Implementierungsschnitt nach Ownerfreigabe

Erst nach Freigabe genau dieser Plan-SHA und erneuter Liveprüfung von
lokalem/remote/PR-HEAD wird umgesetzt:

1. den bestehenden #23-Planner-/Sinkpfad um den typisierten
   Software-Readiness-/Sinkannahmenachweis ergänzen und den S3-008-Reset in
   `safety_fault_service.*` write-before-apply daran binden;
2. `FermentationApplication::SafetyDependencies` um den vorhandenen
   `RunPersistenceCoordinator` ergänzen, `loadAndInitialize()` genau einmal
   ausführen, die bestehende Snapshot-Restaurierung verwenden und
   nicht rekonstruierbare kritische Loadstatus einmalig als Y4-005 an die
   zentrale SafetyFaultService projizieren;
3. `TemperatureControlApplicationOrchestrator` an den vorhandenen echten
   `TemperatureControlResult`-Handoff anbinden und in
   `SafetyFaultService` die vollständige C6-Reason-Matrix mit den vorhandenen
   #20/#21- und #56/#57-Typen implementieren; Air-Limit-Zustände bleiben
   faultfrei, `NoValidRequest` bleibt außerhalb der Matrix;
4. den bestehenden Y4-007-Injection-/Restartvertrag unverändert lassen und
   nur die vollständigen positiven und negativen Contract-Orakel ergänzen;
5. nur die tatsächlich betroffenen kanonischen Dokumentationen und
   `docs/ROADMAP.md` auf R4, C2/C3/C6/C7 und den Ownergate aktualisieren;
6. keine ESP-IDF-/NVS-/Hardwareadapter, keine neuen Grenzwerte, keine
   physische Aktordiagnose, keinen Produktions-Trigger für Y4-007 und keine
   zweite Fault-, Recovery-, Persistenz- oder Aktor-Safetylogik einführen.

Voraussichtlich betroffene Dateien sind ausschließlich die vorhandenen
Grenzen und ihre direkten Orakel: `actuator_plan_types.hpp`,
`actuator_plan_sink_driver.hpp/.cpp`, `safety_fault_service.hpp/.cpp`,
`fermentation_application.hpp/.cpp`,
`temperature_control_orchestrator.hpp/.cpp` sowie
`test/test_issue24_safety/test_issue24_safety.cpp`,
`test/test_actuator_planner/test_actuator_planner.cpp`,
`test/test_run_persistence_coordinator/test_run_persistence_coordinator.cpp`
und der direkt betroffene #22-Testfilter. Neue Module, Ports oder Adapter sind
nicht geplant.

Die konkrete PIN-Verifikation, Hardwarediagnose, ESP-IDF-Resetursache und
`esp_restart()` bleiben außerhalb dieses Schnittrahmens. Bei einer materiellen
Abweichung wird die Implementierung gestoppt, diese R4 aktualisiert und erneut
zur Ownerfreigabe vorgelegt.

## 7. Vollständige R4-Zieltestmatrix (nach Freigabe; aktuell NOT_RUN)

Die folgenden Tests sind Zielorakel, keine in dieser Planungsrunde ausgeführten
Ergebnisse. Draft führt später nur gezielte native Tests des geänderten Bereichs
aus; Full-Suite bleibt Owner-Gate.

### Fault/Sensor

- `ProcessMessage::TargetReachTimeExceeded` aus dem bestehenden Prozessautomaten
  mit `ProcessRunSnapshot::maximumTargetReachMinutes`; #24 dupliziert keine
  Zeitlogik und erzeugt keinen P1-Fault aus normalem `AirLimitReduced` oder
  `AirLimitBlocked`;
- #21-Evidenz mit gültigen Air-, Product- und Cooling-Snapshots sowie gültiger
  Revision und `ThermalCompatibility::Incompatible` -> bestehende
  `ReturnValidationAborted`-/`AirFallbackActive`-Rückkehrlogik; kein S3-003-
  Latch allein aus diesem Enum;
- Produktfühler O2/Fallback gemäß #21; die ursprüngliche #20/#22-Reason bestimmt
  die Projektion, nicht #23 `NoValidRequest`;
- #22 `NoCommissioning`, `SensorUnavailable`, `InvalidConfiguration`,
  `InvalidSample`, `TimeInvalid` und `RequestIdentityExhausted` werden
  reason-spezifisch geprüft: Sensorursachen bleiben O2/S3, Konfigurations-
  ursachen gehen in den vorhandenen #56/#57-/Y4-Vertrag, nicht sicher
  zuordenbare Ursachen in den fail-closed Unknown-Pfad;
- Schrankluft `FAILED` -> `S3-001`;
- Kühlkörper-/Außenwärmetauscher `FAILED` -> `S3-002`;
- explizite simulierte Ursache `persistent/safety-relevant sensor
  contradiction` -> `S3-003`, persistenter Latch, `ImmediateStop`, kein
  Auto-Rearm und Reset erst nach Ursachefreiheit und den vorgesehenen Checks;
- thermische Eingriffsgrenze -> `S3-004` und bis #35 keine aktive Recovery;
- harte Notgrenze nach bestehendem S3-004-Latch -> S3-004 und S3-005 bleiben
  beide nachvollziehbar und aktiv.

### Aktor

- #23 `ActuatorWatchdogFaultEvidence` -> S3-008;
- S3-006/S3-007 und S3-009 als reproduzierbare Contract-/Injection-Cases;
  kein Test behauptet eine heute vorhandene reale Fan- oder
  H-Brücken-/Stromdiagnose;
- vollständige C2-Software-Readiness: auslösende Watchdog-Episode nicht mehr
  aktiv, konsistenter Planner, erzeugter sicherer AUS-Plan und akzeptierter
  bestehender Sinkpfad -> Reset zulässig;
- fehlender Planner, fehlender Sink, unbekannter oder widersprüchlicher
  Readinessstatus, ungültige Plannerparameter sowie nicht passender
  AUS-Plan/Sinkannahme -> Reset fail-closed;
- Planner-/Sink-Bypassversuch -> keine Freigabe, auch nicht aus normalem
  `Allowed` oder direkter Sinkanfrage.

### C3 – realer #17/#18-Wiederanlauf

- `FermentationApplication` ruft `loadAndInitialize()` einmalig auf;
- gültiger aktiver `Current`-Load mit vorhandener
  `restoreRunPersistenceSnapshot()`-Projektion -> aktiver Lauf, kein Y4-005;
- gültiger `FallbackRecovered`-Load mit derselben bestehenden
  Restaurierung -> kein Y4-005;
- nicht rekonstruierbarer aktiver Current-/Fallback-Zustand,
  `PreparedInterrupted` und kritische bestehende Loadfehler -> Y4-005,
  persistenter Latch und `ActuatorSafetyGateInput != Allowed`;
- `NoPersistedRun`/`NoActiveRun` -> kein Y4-005;
- erneuter Boot-/Begin-Aufruf oder fehlender typisierter Producer darf keine
  zweite Persistenz-/Faultentscheidung erzeugen.

### C6 – vollständige #22-Reason-Projektion

- `None`, `NeutralBand`, `Saturated`, `AirLimitReduced` und
  `AirLimitBlocked` erzeugen allein keinen Fault;
- `AirLimitReduced` und `AirLimitBlocked` bleiben auch bei wiederholter
  Projektion faultfrei und werden nicht als `P1-001`, O2 oder S3 umgedeutet;
- `SensorUnavailable` und `InvalidSample` mit gültiger #20/#21-
  Rollen-/Snapshot-Evidenz -> der bestehende O2/S3-Producerpfad;
- fehlende, unbekannte oder widersprüchliche Sensorbegleitevidenz -> Y4-008
  fail-closed;
- `NoCommissioning`/`InvalidConfiguration` werden nur über die vorhandene
  #56/#57-/Y4-Grenze bewertet; ohne passende typisierte Producerantwort kein
  `Allowed`;
- `TimeInvalid` und `RequestIdentityExhausted` werden nicht als Sensor- oder
  `NoValidRequest`-Fault erfunden, sondern bei sicherheitsrelevanter
  Unklarheit fail-closed behandelt;
- kein Orakel ruft die #23-`NoValidRequest`-Klassifikation als alternative
  Faultursache auf.

### C7 – Y4-007 Contract-/Injection-Nachweis

- bestehende Y4-007-Injection erzeugt den persistenten System-Latch;
- fehlende Ursachefreiheit, Integritätsfehler oder fehlende technische
  Autorisierung bleiben fail-closed;
- der erste automatische kontrollierte Restart derselben aktiven
  Ursache/Episode wird write-before-apply vorbereitet und akzeptiert;
- ein zweiter automatischer Restart derselben Ursache/Episode wird ohne
  neuen Trigger verhindert;
- Restart-Evidenz bleibt nach Bootabgleich exakt einmal, nachvollziehbar und
  deterministisch;
- es existiert weiterhin kein produktiver Y4-007-Trigger; ein Test darf keinen
  hypothetischen internen Producer einschleusen.

### Persistenz und Capacity

- `WriteError`, `CapacityError`, `ReadError`, Korruption;
- `CommitOutcomeUnknown`: Readback bestätigt neuen Stand, Readback zeigt alten
  Stand, Readbackfehler oder Mismatch;
- alle 17 aktiven Slots belegt plus unabhängige neue Safetyursache: kein Evict,
  kein Slot für `Y4-006` erforderlich, Basis-Overflowmarker und `SAFE_BOOT`;
- RAM-Latch bleibt bei fehlgeschlagenem Safetywrite;
- ein später isolierter erfolgreicher Write ist keine Entwarnung;
- statische Payload-/Envelope-/2.048-Byte-Bound und Revisions-/ID-Overflow.

### Restart und Brownout

- `PowerOn`, passender `SoftwareRestart` mit Application-Evidence,
  `WatchdogOrPanic`, `Brownout`, `ExternalOrOther`, `Unknown`;
- eine bootlokale Observation wird nicht mehrfach als Restart gezählt;
- derselbe automatische Recoveryfault darf höchstens einen kontrollierten
  Restart auslösen; der zweite Versuch bleibt aus;
- dritter abnormaler Restart -> `SAFE_BOOT`;
- 29:59, 30:00 und >30:00 monotone stabile Laufzeit;
- normaler Reboot und Power-off schließen die Episode nicht;
- abnormaler Restart während Stabilitätsphase startet die Bewertung neu;
- normaler Reboot verlässt `SAFE_BOOT` nicht;
- `Y4-009` erzeugt keinen generischen zweiten Reboot.

### Reale #56/#57-Grenze

- `ConfigurationRuntimeFailure`;
- nicht auflösbarer `CommitOutcomeUnknown` und reale Commitindeterminate-
  Statuskette;
- `ConfigurationUnavailable`;
- `ConfigurationIntegrityFailure`;
- echte `FermentationApplication`-Grenze, genau eine Safetyinstanz, reale
  öffentliche Resultate und #23-Planner-/Sinkpfad;
- `Unknown`/`Unresolved` -> nie `Allowed`.

### Reset, Journal und S3-004

- neutraler `FaultResetRequest` ohne Caller-Safetyentscheidung;
- fehlende oder falsche typisierte Autorisierung -> fail-closed;
- codebezogene Reset-/Reboot-/SAFE_BOOT-Entscheidung je S3/Y4;
- Journalprojektion über das bestehende `IEventJournal` für Fault, Restart,
  Reset und SAFE_BOOT;
- Journalfehler darf Safetycommit oder sichere Reaktion niemals in `Allowed`
  umdeuten;
- S3-004 ohne #35-Qualifikation: keine aktive Recovery, kein PI-/Planner-
  Bypass, Latch bleibt gesetzt, Versuchszahlvertrag `0..2` bleibt erhalten.

## 8. Kanonische Synchronisierung und Prüfgrenzen

Dieser Dokumentationsstand synchronisiert nur die tatsächlich betroffenen
Quellen:

- `docs/SAFETY_AND_FAULTS.md`: Matrix, Producer, Reset-/Rebootpolicy und
  geschlossene versus offene Punkte;
- `docs/SAFETY_COMPONENT_FAULTS.md`: S3-004 contract-only und harte
  firmwarefeste Obergrenze `<=2`;
- `docs/SYSTEM_SAFETY_AND_RECOVERY.md`: Episode, monotone Stabilität,
  Power-off und einmaliger kontrollierter Restart;
- `docs/ACCEPTANCE_TESTS.md`: vollständige R4-Zielorakel, historische R2-
  Läufe unverändert `NOT_ACCEPTED_PENDING_R3`;
- `docs/RUN_COMMANDS.md`: neutrale Resetdaten und zentrale Evaluation;
- `docs/ROADMAP.md`: nur Status/Gate.

Es wird kein ADR als `accepted` ergänzt. Ein ADR-Entwurf wäre nur bei einer
später tatsächlich nachgewiesenen neuen langfristigen Architekturentscheidung
außerhalb ADR-013/014/018 zulässig.

Nach Ownerfreigabe werden ausschließlich die gezielten nativen Filter der
geänderten Bereiche und ihre direkten Konsumenten ausgeführt:

- `test_issue24_safety` für C2, C3, C6 und C7;
- `test_actuator_planner` für die #23-Readiness-/Stop-/Sinkgrenze;
- `test_run_persistence_coordinator` für die unveränderte #17/#18-
  Load-/Restore-Regression;
- der bestehende #22-Temperaturkontrollfilter für die vollständige
  Status-/Reason-Matrix.

Zusätzlich gelten die Repository-Gates `clang-format --dry-run --Werror`,
`git diff --check`, Architektur-/Dependency-Gate und Secret-Scan. Ausgeführt
werden in dieser Planungsrunde ausschließlich:

- `git diff --check`;
- Markdown-/Architektur-Konsistenzprüfung;
- Secret-Scan gemäß Repository-Regel.

`NOT_RUN` bleiben:

- Native-Suite und Firmwaretests;
- ESP-IDF-, PlatformIO- und sonstige Firmwarebuilds;
- Ressourcen-/Hardware-/Bring-up-/thermische Tests;
- Remote-CI.

Der PR bleibt Draft, es gibt keinen Ready-for-review-Wechsel, keinen Merge,
kein Auto-Merge, keinen Issue-Abschluss, keine Branchlöschung und keinen
Force-Push. Nach Commit, normalem Push, PR-Body-/Statusaktualisierung und genau
einem neuen aktuellen SESSION HANDOVER ist der nächste und einzige Schritt:

```text
Ownerreview und Freigabe der exakten vollständigen R4-SHA
```
