# Issue #24 – Fehlerklassen, Verriegelung, SAFE_BOOT und Fehlerinjektion

## Planrevision 3 – PLAN_R3_PENDING_OWNER_APPROVAL

| Feld | Verbindlicher Stand |
|---|---|
| Issue | #24 – `[E3.5] Fehlerklassen, Verriegelung, SAFE_BOOT und Fehlerinjektion` |
| Draft-PR | #107 |
| Branch | `agent/issue-24-fehlerklassen-safe-boot-plan` |
| Owner-reviewter Ausgangsstand | `6108ec649fb6d969b013e4eecf3ed25889127ee0` |
| Base/main | `b8eae5f4da5f2666b5a9bda333d115254c4db5b2` |
| R2 | suspendiert; weder normative R3-Quelle noch umzuschreibender Code |
| Status | `PLAN_R3_PENDING_OWNER_APPROVAL` |
| Exakte Freigabe-SHA | SHA des vollständigen Dokumentationscommits dieser Revision |
| Produktionscode, Tests, Adapter, Build/Toolchain, Hardware | in dieser Planungsrunde unverändert |

Diese Datei ist die vollständige und eigenständige Implementierungsgrundlage
für R3. Alte R2-/R3-Texte sind nicht implizit Bestandteil dieser Fassung. Die
Ownerentscheidungen OD-24-01 bis OD-24-10 sind technisch konsolidiert; es
bleibt kein Owner-Gate zum Erfinden von Codes, einer Kapazitätszahl oder einer
Resetarchitektur. Der nächste Schritt nach diesem Commit ist ausschließlich
Ownerreview und Freigabe der exakten SHA.

## 1. Live-Baseline und Planungs-Gate

Vor dieser Korrektur wurden Repository, Branch, Arbeitsbaum, `origin/main`,
Live-Issue #24, Draft-PR #107, der aktuelle R3-Plan, der neueste SESSION
HANDOVER, Root- und lokale `AGENTS.md`, `docs/AGENT_WORKFLOW.md`,
`docs/ENGINEERING_PRINCIPLES.md`, `docs/SPECIFICATION_REVIEW.md`,
`docs/DECISIONS.md`, ADR-013/014/018 sowie die betroffenen Safety-, Recovery-,
Persistenz-, Command- und Acceptance-Quellen geprüft.

Der Branch und PR-HEAD standen auf `6108ec649fb6d969b013e4eecf3ed25889127ee0`; der Arbeitsbaum war sauber,
`origin/main` stand auf `b8eae5f4…`, Issue #24 war offen und PR #107 war offen
und Draft. Der neueste Handover bestätigte `PLAN_R3_PENDING_OWNER_APPROVAL`.
Es wurde keine materielle Live-Abweichung gefunden, die eine der bestätigten
Ownerentscheidungen neu öffnen würde. Der eingefrorene R2-Produktionsstand wird
nicht zurückgebaut oder korrigiert.

Die maßgeblichen Fachquellen bleiben:

- `docs/SAFETY_AND_FAULTS.md` für Klassen, Codes und codebezogene Resetpolicy;
- `docs/SAFETY_COMPONENT_FAULTS.md` für Sensor-, Temperatur- und Aktorursachen;
- `docs/SYSTEM_SAFETY_AND_RECOVERY.md` für Restart-Episode, Boot und Recovery;
- `docs/RUN_COMMANDS.md` für den bestehenden #15-Commandpfad;
- `docs/CONFIGURATION_PERSISTENCE.md` und die öffentlichen #56/#57-Typen für
  Persistenz- und Konfigurationsresultate;
- `docs/ACCEPTANCE_TESTS.md` für historische R2-Nachweise und R3-Zielorakel;
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
Autorisierung sind niemals `Allowed`.

Nicht Teil dieses Plans sind ESP-IDF-Resetmapping und `esp_restart()` (#29/E5),
reale NVS-/Flashadapter, Hardware-/GPIO-/Grenzwertfestlegung, #35-Commissioning,
die erneute Service-PIN-Verifikation, E4, #29/#90-Adapter, OTA oder eine
Capability-/Token-/Pointerarchitektur. Die Root-Skeletons
`src/main.cpp` und `main/app_main.cpp` werden nicht künstlich erweitert. ADR-013
trägt die Modulgrenzen; ADR-014 und ADR-018 tragen die vorhandenen
Determinismus- und #56/#57-Verträge. Ein neuer ADR ist nicht erforderlich.

## 3. Verbindliche R3-Semantik

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
| `O2-003` – #20 unklare Rollen-/Plausibilitätsabweichung; Sensorrollen-/Messkorrelation | Peltier aus, keine Ersatzfreigabe | kein Auto-Rearm; nur eindeutige neue Evidenz | Bedienerreset nach Rollen-, Plausibilitäts- und Safetychecks | kein Reboot / kein Exit | primary; S3-003 kann folgen |
| `S3-001` – #20 Schrankluftsensor `FAILED`; Sensorrolle | Peltier/H-Brücke aus, sicherer Nachlauf | persistenter Latch; kein Auto-Rearm | Serviceautorisierung nach stabiler Sensor- und Safetyprüfung | kein zusätzlicher Reboot / kein Exit durch Reset | primary |
| `S3-002` – #20 Außenwärmetauscher-/Kühlkörpersensor `FAILED`; Sensorrolle | Peltier/Richtungen aus, sichere Wärmeabfuhr | persistenter Latch; kein Auto-Rearm | Serviceautorisierung nach Sensor-/Aktorprüfung | kein zusätzlicher Reboot / kein Exit durch Reset | primary |
| `S3-003` – #24-interne Persistenz eines vorhandenen #21-`CrossRolePlausibilityContext`-/`ThermalCompatibility::Incompatible`-Befunds; Rollen-/Plausibilitätskorrelation | Peltier aus, kein Fallback | persistenter Latch; kein Auto-Rearm | Serviceautorisierung nach Ursachefreiheit und Plausibilitätsnachweis | kein zusätzlicher Reboot / kein Exit durch Reset | primary; O2-003 bleibt nachvollziehbar |
| `S3-004` – stabiler #24-Release-1-Contract-/Injection-Code für Sicherheits-Eingriffsgrenze; späterer qualifizierter #35-Producer; eine obere/untere Grenzkorrelation | Leistung aus, Richtung sperren, Impuls/Integrator verwerfen | persistenter Latch; keine produktive Auto-Recovery; #35 fehlt => `Unresolved` | Serviceautorisierung nach Checks; Reset löscht nicht automatisch und erzeugt keine Recovery | kein zusätzlicher Reboot / kein Exit durch Reset | primary; Recovery bleibt Folge |
| `S3-005` – stabiler #24-Release-1-Contract-/Injection-Code für harte thermische Notgrenze; späterer qualifizierter #35-/Hardware-Producer; eine obere/untere Grenzkorrelation | sofort aus, keine Gegenrichtung, sichere Lüfterstrategie | persistenter Latch; kein Auto-Rearm | technische Serviceautorisierung nach Grenz-/Hardwareprüfung | kein zusätzlicher Reboot / kein Exit durch Reset | primary |
| `S3-006` – stabiler #24-Release-1-Contract-/Injection-Code für Außenlüfterfehler; reale Fan-Diagnose folgt dem zuständigen späteren Hardware-/Commissioning-Gate; Außenlüfterrolle | Peltier aus, Restwärme fail-closed behandeln | persistenter Latch; kein Auto-Rearm | Serviceautorisierung nach Fan-/Ausgangsprüfung | kein zusätzlicher Reboot / kein Exit durch Reset | primary |
| `S3-007` – stabiler #24-Release-1-Contract-/Injection-Code für Innenlüfterfehler; reale Fan-Diagnose folgt dem zuständigen späteren Hardware-/Commissioning-Gate; Innenlüfterrolle | Peltier aus oder richtungsbezogen sperren | persistenter Latch; kein Auto-Rearm | Serviceautorisierung nach Fan-/Ausgangsprüfung | kein zusätzlicher Reboot / kein Exit durch Reset | primary |
| `S3-008` – #23 `ActuatorWatchdogFaultEvidence`; Planner-/Diagnose-Evidenz | Peltier aus, Safety-Gate stop, vollständige 64-bit-Evidenz sichern | persistenter Latch; kein Auto-Rearm | technische Autorisierung nach #23-, Sensor- und Aktorchecks | standardmäßig kein Reboot / kein Exit durch Reset | primary; Folgefehler bleiben |
| `S3-009` – stabiler #24-Release-1-Contract-/Injection-Code für H-Brücke/Strom/Ausgang/Richtung; reale Diagnose folgt dem zuständigen späteren Hardware-/Commissioning-Gate; ein Output-Domain-Schlüssel | beide Richtungen aus, Plan verwerfen | persistenter Latch; kein Auto-Rearm | technische Serviceautorisierung nach Ausgangsprüfung | kein zusätzlicher Reboot / kein Exit durch Reset | primary |
| `Y4-001` – #56 `ConfigurationRuntimeFailure`; eine aktive Konfigurationsrevision | keine neue Konfiguration, Aktoren sicher stoppen | persistenter System-Latch; kein Auto-Rearm | Service/technisch nach Konfigurations-, Persistenz- und Integritätschecks | kein zusätzlicher Reboot / kein Exit durch Reset | primary |
| `Y4-002` – #56/#57 `ConfigurationCommitIndeterminate`/`CommitOutcomeUnknown`; eine aktive Commit-Korrelation | unklare Revision sperren, Aktoren aus | persistenter System-Latch; kein Auto-Rearm | Service/technisch erst nach eindeutigem Status und neuer Revision | kein zusätzlicher Reboot / kein Exit durch Reset | primary |
| `Y4-003` – #57 `ConfigurationUnavailable`; eine Recovery-/Graph-Quelle | keine Teilkonfiguration verwenden, Aktoren aus | persistenter System-Latch; kein Auto-Rearm | Service/technisch nach gültiger Revision | kein zusätzlicher Reboot / kein Exit durch Reset | primary |
| `Y4-004` – #57 `ConfigurationIntegrityFailure`; eine Graph-/Konfigurationsdomäne | Integritätsfehler fail-closed, Aktoren aus | persistenter System-Latch; kein Auto-Rearm | Service/technisch nach Integritätsprüfung | kein zusätzlicher Reboot / kein Exit durch Reset | primary |
| `Y4-005` – #17/#18 kritischer Laufcheckpoint nicht rekonstruierbar; ein aktiver Lauf | Peltier/H-Brücke aus, Lauf sicher beenden | persistenter System-Latch; kein Auto-Rearm | Service/technisch nach neuer gültiger Laufrevision | kein zusätzlicher Reboot / kein Exit durch Reset | primary |
| `Y4-006` – #24 Safety-State-Read/Write/Capacity/Integrity; ein globaler Basisrecord-Marker | RAM-Latch und Aktorsperre sofort, Marker im selben Record versuchen | persistenter System-Latch/Overflowmarker; kein Auto-Rearm, keine Eviction | Service/technisch nach Read-/Write-/Capacity-/Integritätsprüfung | kein zusätzlicher Reboot; Exit nur separate Bootpolicy | primary; aktive Slotliste wird nicht verfälscht |
| `Y4-007` – #24 interner Safety-/Recoveryfehler; eine SafetyService-Evidenz | Aktoren aus, Recoveryevidenz sichern, höchstens den einen zulässigen kontrollierten Restart vorbereiten | persistenter System-Latch; kein Auto-Rearm | technische Autorisierung nach Ursachefreiheit und Integritätsprüfung | genau ein automatischer kontrollierter Restart pro aktiver Recoveryursache/-episode; danach kein zweiter automatischer Restart | primary; S3-008 bleibt separat |
| `Y4-008` – unbekannter Resetgrund, fehlende/doppelte/mismatched App-Evidenz oder unbekannter Safetyinput; eine Bootbeobachtung | `Allowed` verbieten, Aktoren aus, fail-closed persistieren | persistenter System-Latch; kein Auto-Rearm | bis geklärter Ursache/Evidenz verboten | kein Reboot als Lösung / kein Exit ungeklärt | immer primary |
| `Y4-009` – dritter abnormaler Restart / offene SAFE_BOOT-Episode; eine Episode | vor Aktor-/Lauffreigabe `SAFE_BOOT` | persistente Episode-/Systemverriegelung; kein Auto-Rearm durch Reboot | bewusster autorisierter codebezogener Exit nach allen Checks | kein automatischer zusätzlicher Reboot; ein technischer Restart ist nur Policy des zugrunde liegenden Codes | primary; auslösende Latches bleiben |

Die Matrix führt keine historische Lücke und keine Zukunftsfunktion für #19
ein. `Unknown`/`Unresolved` wird nicht in eine harmlose Klasse normalisiert.
`P1-001` wird ausschließlich aus der bestehenden `ProcessMessage`-Erzeugung
des Prozessautomaten projiziert; #24 berechnet weder eine eigene
`maximumTargetReachMinutes`-Zeitlogik noch einen Fault aus dem normalen
`TemperatureControlReason::AirLimitReduced`. Bei O2 wird immer der
ursprüngliche #22-Reason beziehungsweise der vorhandene #20/#21-Sensorbefund
verwendet; #23 `NoValidRequest` ist kein zusätzlicher Faultproducer.

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

## 4. Aktive Producer-Worst-Case-Analyse und Capacity

### 4.1 Aktive Instanzen

Cleared-Historie und reine Journalereignisse zählen nicht. Die Bound wird aus
gleichzeitig aktiven unabhängigen Instanzen abgeleitet:

| Code | Max. aktive Instanzen | Producer/Rolle/Identität | Mehrfachinstanz und Koexistenz |
|---|---:|---|---|
| `S3-001` | 1 | realer #20-`SensorQualitySnapshot`, Schrankluft-Sensorrolle | eine Rolle, ein SourceKey |
| `S3-002` | 1 | realer #20-`SensorQualitySnapshot`, Außenwärmetauscher-/Kühlkörper-Sensorrolle | eine Rolle, ein SourceKey |
| `S3-003` | 1 | #24-interne Latchprojektion aus vorhandenem #21-`CrossRolePlausibilityContext`-/`ThermalCompatibility::Incompatible`-Befund | ein aktueller Korrelationsfall; keine parallelen Konfliktinstanzen im Producer |
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
| `Y4-005` | 1 | #17/#18 aktiver Laufcheckpoint | ein aktiver Lauf |
| `Y4-006` | 1 | globaler #24-Basisrecord-/Overflowmarker | kein variabler Slot; ein Marker aggregiert den globalen Capacity-/Persistenzzustand |
| `Y4-007` | 1 | eine zentrale `SafetyFaultService`-Recoveryevidenz | genau eine zentrale Safetyinstanz |
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

Die Slotzahl `17` ist aus der finalen R3-Code-/Injection-Matrix und den heute
belegten Rollen abgeleitet und nicht aus R2 übernommen: neun S3-Latches plus
acht nicht-markerartige Y4-Latches. Die Contract-only-S3-Codes werden für den
konservativen Injektions-Worst-Case jeweils mit einer unabhängigen Instanz
gezählt. Ein späterer realer Producer übernimmt exakt denselben Code und die
zugehörige Rollen-/Korrelationsidentität; er fügt keinen zusätzlichen Slot
hinzu. Eine später nachgewiesene Mehrfachinstanz derselben Rolle wäre vor
Implementierung eine materielle R3-Abweichung und dürfte nicht still in diese
Bound gedrückt werden.

### 4.2 Neuer R3-Record und Byte-Nachweis

Die 80-Byte-/48-Byte-Werte aus R2 sind kein R3-Vertrag und werden nicht zur
Begründung verwendet. R3 definiert stattdessen einen neuen festen,
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

Damit ergibt sich bewusst für R3:

```text
Payload = 128 + 17 * 64 = 1.216 Byte
Record  = 37 + 1.216   = 1.253 Byte
Application-Limit       = 2.048 Byte
Reserve                 =   795 Byte
```

`2.048` ist kein globales `IStateStore`-Limit. R3 wählt es als
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

Die Root-Skeletons bleiben unverändert. #24 erzeugt keine pauschale #29/#90-
Abhängigkeit. E5 wiederholt denselben Nichtumgehungsvertrag mit realen
Adaptern.

## 6. Konkreter Implementierungsschnitt nach Ownerfreigabe

Erst nach Freigabe genau dieser Plan-SHA und erneuter Liveprüfung von
lokalem/remote/PR-HEAD wird umgesetzt:

1. R3-Code-/Policy-/Faultrecord-Verträge, `17`-Slot-Bound, Basis-Overflowmarker und
   neuer fester Record-Codec;
2. minimale #15-Migration in
   `lib/fermentation_app/src/run_commands.hpp/.cpp` und Konsumenten in
   `safety_fault_service.hpp/.cpp`, ohne positive Caller-Safetyfelder;
3. zentrale Ursachen-, Latch-, Reset-, Restart- und SAFE_BOOT-Evaluation;
4. Application-Grenze in `fermentation_application.*` und reale #56/#57-
   Gateprojektion auf #23;
5. Korrektur von `lib/device_platform/src/reset_port.hpp` und nativer
   Testhilfe auf neutralen Plattformvertrag; kein ESP-IDF-Mapping;
6. S3-004 contract-only, einmalige Recovery-Restart-Evidenz und
   fail-closed Capacity-/Readbackpfad;
7. gezielte native Tests und reale Application-End-to-End-/Negativtests.

Die konkrete PIN-Verifikation, Hardwarediagnose, ESP-IDF-Resetursache und
`esp_restart()` bleiben außerhalb dieses Schnittrahmens. Bei einer materiellen
Abweichung wird die Implementierung gestoppt, diese R3 aktualisiert und erneut
zur Ownerfreigabe vorgelegt.

## 7. Vollständige R3-Zieltestmatrix (nach Freigabe; aktuell NOT_RUN)

Die folgenden Tests sind Zielorakel, keine in dieser Planungsrunde ausgeführten
Ergebnisse. Draft führt später nur gezielte native Tests des geänderten Bereichs
aus; Full-Suite bleibt Owner-Gate.

### Fault/Sensor

- `ProcessMessage::TargetReachTimeExceeded` aus dem bestehenden Prozessautomaten
  mit `ProcessRunSnapshot::maximumTargetReachMinutes`; #24 dupliziert keine
  Zeitlogik und erzeugt keinen P1-Fault aus normalem `AirLimitReduced`;
- Produktfühler O2/Fallback gemäß #21; die ursprüngliche #20/#22-Reason bestimmt
  die Projektion, nicht #23 `NoValidRequest`;
- #22 `NoCommissioning`, `SensorUnavailable`, `InvalidConfiguration`,
  `InvalidSample`, `TimeInvalid` und `RequestIdentityExhausted` werden
  reason-spezifisch geprüft: Sensorursachen bleiben O2/S3, Konfigurations-
  ursachen gehen in den vorhandenen #56/#57-/Y4-Vertrag, nicht sicher
  zuordenbare Ursachen in den fail-closed Unknown-Pfad;
- Schrankluft `FAILED` -> `S3-001`;
- Kühlkörper-/Außenwärmetauscher `FAILED` -> `S3-002`;
- persistenter Sensorwiderspruch -> `S3-003`;
- thermische Eingriffsgrenze -> `S3-004` und bis #35 keine aktive Recovery;
- harte Notgrenze nach bestehendem S3-004-Latch -> S3-004 und S3-005 bleiben
  beide nachvollziehbar und aktiv.

### Aktor

- #23 `ActuatorWatchdogFaultEvidence` -> S3-008;
- S3-006/S3-007 und S3-009 als reproduzierbare Contract-/Injection-Cases;
  kein Test behauptet eine heute vorhandene reale Fan- oder
  H-Brücken-/Stromdiagnose;
- Planner-/Sink-Bypassversuch -> keine Freigabe, auch nicht aus normalem
  `Allowed` oder direkter Sinkanfrage.

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
- `docs/ACCEPTANCE_TESTS.md`: vollständige R3-Zielorakel, historische R2-
  Läufe unverändert `NOT_ACCEPTED_PENDING_R3`;
- `docs/RUN_COMMANDS.md`: neutrale Resetdaten und zentrale Evaluation;
- `docs/ROADMAP.md`: nur Status/Gate.

Es wird kein ADR als `accepted` ergänzt. Ein ADR-Entwurf wäre nur bei einer
später tatsächlich nachgewiesenen neuen langfristigen Architekturentscheidung
außerhalb ADR-013/014/018 zulässig.

Ausgeführt werden ausschließlich:

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
Ownerreview und Freigabe der exakten vollständigen R3-SHA
```
