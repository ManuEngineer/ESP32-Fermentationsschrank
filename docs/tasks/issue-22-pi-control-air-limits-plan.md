# Plan: Issue #22 – Zeitproportionale PI-Regelung und Luftbegrenzung

## 1. Status, Scope und Owner-Gate

- Revision: **3**.
- Live-Issue: #22, offen.
- Draft-PR: #104, Branch `agent/issue-22-pi-regelung-plan` -> `main`.
- Planpfad: `docs/tasks/issue-22-pi-control-air-limits-plan.md`.
- Diese Revision ist ein vollständiger, eigenständig gültiger Plan. Sie setzt
  keine frühere Planrevision als fachliche oder normative Quelle voraus.
- Planbasis: `main` @
  `10ff98eca4d6f64cc453571d66d4c3b18729b18e`.
- Ausgangs-HEAD vor dieser Revision:
  `2a46ee7a62700093e26274f48d488c380b937831`.
- Der exakte Commit dieser Revision wird nach dem Commit mit voller SHA in
  der PR-Beschreibung und im aktuellen SESSION-HANDOVER ausgewiesen.
- Umsetzung bleibt gesperrt, bis der Owner exakt diesen neuen Plan-Commit mit
  `PLAN APPROVED` beziehungsweise `Approved plan commit: <SHA>` freigibt.
- Diese Revision ändert ausschließlich Plan-/Roadmap-Dokumentation und
  zugehörige PR-Metadaten. Sie implementiert keine Produktionslogik,
  produktiven Tests, Hardware-, GPIO-, Toolchain- oder CI-Änderung.
- PR #104 bleibt Draft. Es gibt kein `Ready for review`, keinen Merge, kein
  Auto-Merge und kein Branch-Löschen.

```text
CONTEXT_BASELINE_BRANCH: agent/issue-22-pi-regelung-plan
CONTEXT_BASELINE_SHA: 10ff98eca4d6f64cc453571d66d4c3b18729b18e
CONTEXT_HEAD_BEFORE_REVISION: 2a46ee7a62700093e26274f48d488c380b937831
CONTEXT_PLAN_SHA: NONE (wird nach dem Commit dieser Revision eingetragen)
CONTEXT_REFRESH_MODE: FULL
SOURCE_OF_TRUTH_CONFLICT: NONE festgestellt; bandCelsius wird durch diese
  Ownerfreigabe als einseitige Toleranz/Halbbreite festgelegt.
```

## 2. Ziel, Reihenfolge und Nicht-Ziele

Issue #22 liefert einen deterministischen, hardwarefreien und nativ testbaren
Fachkern für Release 1:

- zeitproportionale PI-Regelung mit Proportional- und Integralanteil;
- vier getrennte Maschinenparametersätze: Luft/Heizen, Luft/Kühlen,
  Produkt/Heizen und Produkt/Kühlen;
- richtungsabhängige einseitige Neutralbandschwellen und begrenzte Zeitquote;
- Produktregelung mit früher Luftbegrenzung;
- Luftregelung als eigener normaler Modus;
- vollständige Zielqualifikation für Vorheizen und Zielphase;
- ein schmaler Anti-Windup- und Demand-Identitätsvertrag für die spätere
  Integration.

Nicht Bestandteil von #22 sind GPIOs, BTS7960-, Lüfter- oder sonstige
Aktorsignale, Aktorfreigabe, Mindestzeiten, Totzeit, Richtungswechsel-
Hysterese, Impulsakkumulator, Kühlkörpersensor-Auswertung, systemweite
Safety-/Fehlerentscheidungen, Persistenz des PI- oder Evaluator-RAM-Zustands,
aktive Kaskadenregelung, D-Anteil, Autotuning und eine externe Regelbibliothek.

Die fachliche Reihenfolge bleibt:

```text
#21 Sensorrollen/-freigabe -> #22 abstrakte Regelanforderung
                                  -> #23 Aktorplanung
                                  -> #24 Safety-/Fehlerkern
```

Vor der Umsetzung wird der vollständige aktuelle Diff gegen diesen Plan,
Issue #22, die kanonischen Verträge und die direkten Konsumententests geprüft.
Ein vollständiger Firmwarelauf bleibt bis zum Owner-Gate und der späteren
Review-/CI-Anweisung ausgeschlossen.

## 3. Verbindliche Quellen und Wiederverwendung

Vor der Umsetzung sind mindestens diese Quellen erneut gegen ihren dann
aktuellen Stand zu prüfen:

- `docs/SPECIFICATION_REVIEW.md` als Dokumentationspriorität;
- `docs/DECISIONS.md`, `docs/ADR-013_REUSABLE_DEVICE_PLATFORM.md` und der
  Modulindex;
- `docs/AGENT_WORKFLOW.md` und `docs/ENGINEERING_PRINCIPLES.md`;
- `docs/STATE_MACHINE.md` und der bestehende
  `process_state_machine.hpp/.cpp`-Vertrag;
- `docs/PROGRAMS.md`, `run_commands.hpp/.cpp`, `ActiveRun::effectiveValues()`
  und `ManualRunPlan`;
- `docs/ACTUATOR_TIMING.md` für PI-/Aktorgrenzen, Impulsakkumulator,
  Richtungswechsel und Regelanforderungs-Identität;
- `docs/SENSOR_TUNING_COMMISSIONING.md` für die Anti-Windup-Anforderung;
- die #20-Sensorqualitätsverträge und der #21-
  `SensorSelection`-/`CrossRolePlausibilityContext`-Vertrag;
- `docs/RUN_PERSISTENCE.md`, `run_persistence_codec` und
  `run_persistence_coordinator` für Write-before-Apply und Recovery;
- `docs/THIRD_PARTY_COMPONENTS.md`. Arduino PID und QuickPID bleiben
  `NOT_SELECTED`; die R1-PI-/Safetylogik bleibt projektspezifisch.

Der kanonische Komponentenregisterpfad ist exakt
`docs/THIRD_PARTY_COMPONENTS.md`.

Wiederzuverwenden sind insbesondere die #21-Sensorrollen und ihre
Regelberechtigung, `AbstractControlDirection`,
`CrossRolePlausibilityContext`, `SensorQualitySnapshot`, die bestehende
Prozesszustands-Topologie, `ProcessRuntimeState` einschließlich
`qualificationValidSinceMillis`, die bestehenden Laufanpassungen sowie die
abstrakten Plattformports. Kein paralleler Sensor-, Safety-, Prozess- oder
Persistenzvertrag wird still erfunden.

## 4. Architektur- und Verantwortungsgrenzen

### 4.1 #22

#22 nimmt einen bereits ausgewählten Regelsensor, die aktuelle
Luftbeobachtung, einen validierten PI-Parametersatz und den schmalen
Vorzyklus-Feedbackvertrag entgegen. Der Kern erzeugt ausschließlich:

```text
HEAT + Zeitquote
OFF
COOL + Zeitquote
```

Das ist eine abstrakte Soll-Anforderung, kein Aktorbefehl und keine
Safetyfreigabe. #22 prüft weder Kühlkörpersensor noch Aktorfreigabe und führt
keinen allgemeinen `ControlSafetyPermission`-Vertrag ein.

Produkt-/Luft-Regelsensor und die frühe Luftbegrenzung bleiben in #22. Die
kanonische #21-Sensorselektionsfreigabe darf als Sensorrollen- und
Regelberechtigung konsumiert werden; #22 implementiert keine neue Auswahl,
keinen Fallback und keine parallele Berechtigung.

### 4.2 #23 und #24

#23 entscheidet nach der abstrakten Anforderung über Aktorplanung einschließlich
Mindestzeiten, Totzeit, Impulsakkumulator, Gegenrichtungsbestätigung,
Richtungswechsel und Luft-/Kühlkörpersensor. #24 besitzt die systemweite
Safety-/Fehlerentscheidung. Die Rückkopplung aus diesen Schichten bleibt in
#22 ursachenunabhängig und darf deren Fachlogik nicht vorwegnehmen.

## 5. Schmale Werttypen und Prozesssignale

`AbstractControlDirection` wird, falls für die neue gemeinsame Verwendung
erforderlich, aus `sensor_selection.hpp` in einen schmalen gemeinsamen
Werttyp-Header verschoben. Es gibt danach genau eine Definition mit exakt
gleicher #21-Semantik und gleichen Werten:

```text
AbstractControlDirection = Unknown | Heating | Cooling | Idle
```

`CrossRolePlausibilityContext` und alle bestehenden #21-Felder bleiben
wertgleich. `test_sensor_selection` ist in Commit 1 und im finalen gezielten
Nachweis ausdrücklich enthalten.

`QualificationProgress` gehört nicht in den breiten
Regler-/Diagnoseheader. Es wird genau einmal in einem schmalen
`process_signal_types.hpp` oder dem gleichwertigen Prozesssignal-/State-
Machine-Vertrag definiert:

```text
QualificationProgress = Unavailable | Invalid | OutsideBand |
                        Grace | InBand | Complete
```

Der `TargetQualificationEvaluator` produziert diesen schmalen
Prozesssignaltyp; der Zustandsautomat konsumiert ihn. Eine zweite Enum-
Definition und eine implizite Kopplung von Prozessstatus an
`temperature_control_types.hpp` sind unzulässig.

Der neue Prozesssignalvertrag lautet mindestens:

```text
ProcessSignals
  qualificationProgress: QualificationProgress = Unavailable
  coolingTargetConditionValid: bool = false
  criticalFault: bool = false
```

`qualificationProgress` wird ausschließlich in `PREHEATING`,
`REACHING_TARGET` und `QUALIFYING_TARGET` ausgewertet.
`coolingTargetConditionValid` wird ausschließlich in `COOLING` ausgewertet.
Die beiden Bedeutungen werden nicht über einen gemeinsamen Bool oder über
Gnadenzeit/Qualifikationsdauer für COOLING zusammengelegt.

## 6. PI-Datenmodell und Mathematik

### 6.1 Eingabe und Parametersätze

Der reine PI-Kern liest keine Ports und keine globale Uhr:

```text
TemperatureControlInput
  sampleTimestampMonotonicMillis: uint64_t
  targetCelsius: finite double
  sensorMode: Product | Air
  air: SensorQualitySnapshot
  product: SensorQualitySnapshot
  previousDemandFeedback: optional<PreviousDemandFeedback>
```

`sensorMode` und die dazugehörige Regelberechtigung stammen aus #21. Es gibt
kein Kühlkörpersensorfeld und kein allgemeines Safety-Permission-Feld im
PI-Eingang.

```text
PiDirectionParameters
  proportionalGain: finite, >= 0
  integralGainPerSecond: finite, >= 0
  neutralBandWidthCelsius: finite, > 0
  maximumQuote: finite, (0, 1]
  maximumIntegrationStepMillis: uint64_t, > 0

TemperatureControlParameters
  airHeating
  airCooling
  productHeating
  productCooling
  lowerHard < lowerSoft < upperSoft < upperHard
```

Die vier Parametersätze sind getrennt; Produktionszahlen bleiben
`TBD_COMMISSIONING`. Ein fehlendes Profil ist
`Unavailable / NoCommissioning`, ein vorhandenes strukturell ungültiges
Profil `InvalidInput / InvalidConfiguration`. Es gibt keinen Produktions-
default aus Null- oder geratenen Werten.

### 6.2 Richtung und Neutralband

```text
error = targetCelsius - measuredCelsius
```

Die Schwelle für positiven Fehler ist ausschließlich
`neutralBandWidthCelsius` des Heating-Parametersatzes derselben Sensorrolle.
Die Schwelle für negativen Fehler ist ausschließlich die entsprechende
Cooling-Breite. Der Wert ist eine **einseitige Fehlerschwelle**, nicht die
gesamte Bandbreite:

```text
error > +heatingNeutralBandWidth  -> Heating
error < -coolingNeutralBandWidth  -> Cooling
-coolingNeutralBandWidth <= error
  <= heatingNeutralBandWidth      -> Idle
```

An beiden Grenzen gilt `Idle` und Quote `0`; die Grenzen sind inklusiv im
Neutralband. Diese Entscheidung ersetzt nicht die in #23 separat
festgelegte Gegenrichtungsbestätigung oder Richtungswechsel-Hysterese.

### 6.3 Quote, Luftbegrenzung und Integrator

Für die gewählte Richtung wird zunächst die unbeschränkte PI-Quote berechnet,
danach die Maschinenquote und im Produktmodus die frühe Luftbegrenzung:

```text
maximumLimitedQuote = min(unboundedQuote, maximumQuote)
limitedQuote = maximumLimitedQuote * airLimitFactor
```

Alle Berechnungen sind checked und endlich in `[0, 1]`.

Die Luftbegrenzung gilt ausschließlich im Produktmodus und mit einem gültigen,
endlichen Luftsnapshot:

```text
HEAT: air <= upperSoft                         factor 1, Unrestricted
      upperSoft < air < upperHard              linear, Reduced
      air >= upperHard                         factor 0, Blocked

COOL: air >= lowerSoft                         factor 1, Unrestricted
      lowerHard < air < lowerSoft              linear, Reduced
      air <= lowerHard                         factor 0, Blocked
```

An der weichen Grenze ist die Quote noch unbeschränkt, an der harten Grenze
ist die betroffene Richtung blockiert. Im Luftmodus ist Luft der normale
Regelsensor und die Produkt-Luftbegrenzung `NotApplied`; ein Produktwert ist
dort nicht erforderlich.

Der PI-Integrator wird nicht persistiert. Die genaue
Kontextwechsel-/Resetentscheidung steht in Abschnitt 9. Unabhängig davon
gilt als sichere R1-Regel: bei eigener Begrenzung, fehlender/ungültiger
Rückmeldung oder nachgelagerter Nicht-/Teilannahme keine weitere positive
Aufladung in die bereits blockierte Richtung.

### 6.4 Zeitvertrag des PI-Kerns

`sampleTimestampMonotonicMillis` ist `uint64_t`; `NaN` und Unendlich sind
keine möglichen Timestampwerte. Struktur-, Reihenfolge- und Rechenfehler
werden checked behandelt:

| Situation | Demand | Integralzustand | Timestampwirkung |
|---|---|---|---|
| erster Sample | aktueller P-/begrenzter Demand möglich | exakt 0/unverändert; keine Integration | als erster Anker speichern |
| gleicher Timestamp | aktueller Demand möglich | unverändert; `dt = 0` | Anker bleibt gültig |
| rückwärts | `InvalidInput / TimeInvalid`, `Idle`, `0` | auf 0 löschen | alter gültiger Anker bleibt |
| vorwärts, Lücke innerhalb des checked Maximums | aktueller Demand möglich | mit `dt` fortschreiben, sofern Anti-Windup erlaubt | checked übernehmen |
| vorwärts, zu große Lücke | `InvalidInput / TimeInvalid`, `Idle`, `0` | auf 0 löschen | aktueller Timestamp wird neuer Anker, ohne Zeitgutschrift |
| checked Differenz-, Konversions- oder Float-Overflow | `InvalidInput / TimeInvalid`, `Idle`, `0` | auf 0 löschen | kein unsicherer Zeitwert wird übernommen |

Eine rückwärts laufende Zeit ersetzt den letzten Anker nicht. Nach einer zu
großen Lücke ist der neue Anker akzeptiert, aber erst spätere normale Samples
dürfen wieder integrieren. Diese sechs Fälle erhalten je einen Test für
Demand und Integralzustand.

## 7. Status-/Reason-Vertrag

Der Evaluationswert enthält genau einen Status und Reason sowie getrennte
Diagnosewerte für unbeschränkte, maximal begrenzte und luftbegrenzte Quote:

```text
TemperatureControlStatus = Demand | Off | Unavailable | InvalidInput
TemperatureControlReason = None | NeutralBand | Saturated |
                            AirLimitReduced | AirLimitBlocked |
                            NoCommissioning | SensorUnavailable |
                            InvalidConfiguration | InvalidSample | TimeInvalid
AirLimitState = NotApplied | Unrestricted | Reduced | Blocked | Unavailable
```

| Status | zulässige Reason | Richtung | Quote |
|---|---|---|---:|
| `Demand` | `None`, `Saturated`, `AirLimitReduced` | `Heating` oder `Cooling` | `> 0` und `<= 1` |
| `Off` | `NeutralBand`, `AirLimitBlocked` | zwingend `Idle` | `0` |
| `Unavailable` | `NoCommissioning`, `SensorUnavailable` | zwingend `Idle` | `0` |
| `InvalidInput` | `InvalidConfiguration`, `InvalidSample`, `TimeInvalid` | zwingend `Idle` | `0` |

`AirLimitReduced` ist nur mit `Demand`, `Reduced` und einer tatsächlich
reduzierten Quote erlaubt. `AirLimitBlocked` ist nur mit `Off`, `Blocked` und
Quote `0` erlaubt. `Saturated` ist nur eine `Demand`-Diagnose für die
PI-/Maschinenquotenbegrenzung, sofern nicht die Luftreduktion die primäre
Diagnose ist. Safety-, Aktor- und Kühlkörpersensorgründe werden nicht als
#22-Reasons erfunden. Widersprüchliche Kombinationen sind Testfehler.

## 8. Anti-Windup-Feedback und Demand-Identität

### 8.1 Schmaler ursachenunabhängiger Feedbackvertrag

Der #22-Vertrag behauptet keine physisch ausgeführte Quote pro PI-Zyklus.
Der bereits akzeptierte #23-Impulsakkumulator kann aufgeschobene kleine
Anforderungen später gemeinsam ausführen; eine spätere elektrische Leistung
kann deshalb größer sein als die einzelne aktuelle Quote.

```text
PreviousDemandFeedback
  demandSequence: uint64_t
  disposition: FullyAccepted | DeferredOrLimited | Rejected
```

Bedeutung:

- `FullyAccepted` bedeutet nur, dass die nachgelagerte Kette die abstrakte
  Anforderung für ihre weitere Planung angenommen hat. Es behauptet keine
  physische Momentanleistung und keine sofortige Pulsabgabe.
- `DeferredOrLimited` bedeutet, dass die nachgelagerte Kette die Anforderung
  wegen einer beliebigen nachgelagerten Begrenzung nicht normal weiterführen
  konnte, ohne die Ursache in #22 zu benennen.
- `Rejected` bedeutet, dass sie die Anforderung nicht angenommen hat.

#22 kennt keine Mindestzeit, Totzeit, Impulsakkumulation, Aktorfreigabe oder
Safetyursache. In R1 ist die Wirkung konservativ und vollständig definiert:
`DeferredOrLimited` und `Rejected` unterbinden mindestens jede weitere
positive Integratoraufladung für die betroffene Richtung; es gibt kein
Back-Calculation und keinen erfundenen Gain. Bei fehlender Rückmeldung wird
ebenfalls nicht positiv integriert. Eine spätere #23-Planrevision darf die
Disposition fachlich präzisieren, darf aber weder die Demand-Identität noch
die Aussage ändern, dass #22 keine physische Quote behauptet.

Die Reihenfolge ist nicht zyklisch:

1. Evaluation `n` erzeugt eine neue abstrakte Demand mit einer neuen
   `demandSequence`.
2. #23/#24 verarbeiten diese Demand außerhalb von #22.
3. Vor der Integratorfortschreibung der nächsten Evaluation `n+1` wird das
   Feedback für genau die Demand `n` geprüft.
4. Erst danach erzeugt #22 gegebenenfalls Demand `n+1`; das Feedback wird
   nicht in eine zweite Aktor- oder Safetyentscheidung umgedeutet.

### 8.2 Checked Demand-Identität

Die Demand-Identität ist RAM-only und nicht zeitbasiert:

- jede tatsächlich neu erzeugte gültige Demand-Evaluation erhält genau die
  nächste `uint64_t`-Sequenz, beginnend bei `1`;
- auch eine gültige Neubewertung mit gleichem Timestamp erhält eine neue
  Sequenz; `dt = 0` ist kein Identitätsersatz;
- `lastDemandSequence` und der Status, ob das Feedbackfenster offen ist,
  bleiben flüchtig;
- Feedback ist nur gültig, wenn es die unmittelbar vorherige Demand-Sequence
  referenziert und noch nicht konsumiert wurde;
- fehlendes Feedback im unmittelbaren Folgeaufruf wird als konservatives
  `DeferredOrLimited` behandelt und das Fenster geschlossen; später
  eintreffendes Feedback ist stale und wird nicht nachträglich angewandt;
- unbekannte, alte, doppelt konsumierte oder nicht zur letzten Demand passende
  Sequenzen werden verworfen und erzeugen `InvalidInput / InvalidSample`,
  Richtung `Idle`, Quote `0` und keinen Integratorfortschritt;
- beim Erreichen `UINT64_MAX` wird nicht gewrappt. Eine weitere neue Demand
  wird checked als `InvalidInput` abgewiesen; es gibt keinen stillen
  Identitätswechsel und kein Persistenzschema dafür.

Eine nicht erzeugte `Off`-/`Unavailable`-/`InvalidInput`-Bewertung eröffnet
keine neue Demand-Identität und invalidiert ein noch offenes Feedbackfenster.
Ein Neustart verwirft das flüchtige Fenster. Tests decken gleiche Timestamps,
fehlendes, altes, doppeltes, fremdes und überlaufendes Feedback ab.

## 9. PI-Integrator bei Kontextwechseln

`ACTUATOR_TIMING.md` bleibt maßgeblich: ein Richtungswechsel setzt den
Integralanteil nicht automatisch und pauschal auf null. Sollwertsprung,
Sensorrollenwechsel, Moduswechsel und relevante Phasenwechsel können eine
kontrollierte Anpassung oder Teilrücksetzung verlangen; ihre endgültige
Produktionswahl bleibt Commissioning.

Dafür erhält der PI-Kern einen kleinen, validierten und injizierten Vertrag,
keinen Produktionsdefault und keine Strategiebibliothek:

```text
IntegratorTransitionAction = Reset | Freeze | BoundedCarry

IntegratorTransitionPolicy
  directionChange
  sensorRoleChange
  targetChanged
  relevantPhaseChange
  maximumCarryQuote: finite, >= 0
```

Der Vertrag ist vollständig zu validieren und muss vom Aufrufer explizit
geliefert werden. `Reset` setzt den Integralanteil auf `0` und integriert im
Übergangssample nicht. `Freeze` erhält höchstens den checked Wert
`min(oldIntegral, maximumCarryQuote)` und integriert im Übergangssample nicht.
`BoundedCarry` überträgt ebenfalls höchstens diesen checked Grenzwert und
verhindert im Übergangssample jede positive Aufladung. Damit kann kein großer
alter Integralimpuls unbegrenzt in einen anderen Sensor-, Richtungs- oder
Sollwertkontext gelangen.

Für neue Läufe, Neustart/Recovery und echte ungültige Zustände gilt unabhängig
von der injizierten Policy immer `Reset`. Für bestätigte
Richtungswechsel, Sensorrollenwechsel, Sollwertsprünge, Moduswechsel und
relevante Phasenwechsel wird die jeweilige Policy-Aktion ausgewertet; #22
entscheidet keinen Produktionswert. Testprofile dürfen `Reset` oder
`BoundedCarry` mit konkreten Testgrenzen wählen. Die reale Auswahl bleibt
`TBD_COMMISSIONING` / #35.

Ein bestätigtes `TargetChanged` löst nach erfolgreicher Run-/Prozessänderung
die Policy-Aktion `targetChanged` aus. Der alte Integrator wird nicht still
weitergeführt; die gewählte Test-/Commissioning-Policy ist in der
Evaluation nachvollziehbar. Die Entscheidung ist von der
TargetQualification-Resetentscheidung getrennt, wird aber an derselben
erfolgreichen Commit-/Apply-Grenze synchronisiert.

## 10. Zieltemperatur, Zielband und Laufanpassungen

### 10.1 Effektive Zielquelle

`targetCelsius` ist immer der **aktuell wirksame Zielwert des laufenden
Prozesses**, nicht der unveränderte Zielwert des Quellprogramms:

- Programmlauf: `ActiveRun::effectiveValues().targetTemperatureCelsius` nach
  allen erfolgreich angewendeten `RunAdjustment`-Revisionen;
- manueller Lauf: der wirksame Wert des `ManualRunPlan`;
- Zielband und Qualifikationsdauer: der bestehende unveränderliche
  Lauf-/Qualifikationsvertrag dieses Laufs. Eine Zielanpassung ersetzt nicht
  stillschweigend Band oder Dauer.

Die Evaluator-Eingabe wird pro Bewertung aus diesen effektiven Werten neu
aufgebaut. Ein alter Quell-Programmsollwert darf nach einer bestätigten
Laufanpassung keine Qualifikation mehr auslösen. `COOLING` verwendet weiterhin
den bestehenden Kühlzielvertrag und niemals den
`TargetQualificationEvaluator`.

### 10.2 TargetChanged und manuelle Läufe

Eine Zieländerung wird zunächst als bestehende Lauf-/Prozesskandidaten-
änderung berechnet. Erst nach erfolgreicher Persistenz und erfolgreichem
`applyProcessTransition()` gilt sie als angewendet. Danach:

- wird der Evaluator für die neue effektive Zieltemperatur zurückgesetzt;
- wird die PI-Integrator-Policy `targetChanged` angewandt;
- verwendet die nächste Bewertung zwingend den neuen effektiven Zielwert;
- beginnt die Qualifikation gemäß der bestehenden Prozess-Topologie neu.

Scheitert Persistenz oder Apply, bleiben alter effektiver Laufwert,
Evaluatorzustand und Integratorkontext gemäß dem bestehenden fail-closed-
Fehlerpfad wirksam; es wird kein halb angewendeter TargetChanged-Zustand
veröffentlicht.

Gezielte Tests decken Zieländerung in `PREHEATING`, `REACHING_TARGET` und
`QUALIFYING_TARGET` sowie einen manuellen Lauf ab. Der manuelle Lauf testet
die effektive `ManualRunPlan`-Quelle und beweist, dass kein Programmsnapshot
als Ersatz gelesen wird.

## 11. TargetQualificationEvaluator

### 11.1 Eingabe und Sensorrolle

Der Evaluator erhält genau:

```text
TargetQualificationInput
  phase: Preheating | Target
  sampleTimestampMonotonicMillis: uint64_t
  targetCelsius: finite double
  bandCelsius: finite double
  qualificationDurationMillis: uint64_t, > 0
  effectiveGraceMillis: optional<checked uint64_t>
  maximumAcceptedSampleGapMillis: optional<checked uint64_t>
  selectedRunMode: Product | Air
  air: SensorQualitySnapshot
  product: SensorQualitySnapshot
```

`bandCelsius` ist die bestehende persistente Feldbezeichnung. Seine durch die
Freigabe dieser Plan-SHA ausdrücklich zu treffende Ownerentscheidung lautet:

```text
bandCelsius = einseitige Toleranz / Halbbreite
InBand genau dann, wenn abs(measuredCelsius - targetCelsius) <= bandCelsius
```

Die Grenze ist inklusiv. Der bestehende Wertebereich wird nicht erweitert:
`program_limits::kMinimumQualificationBandCelsius` bis
`program_limits::kMaximumQualificationBandCelsius`; `0` ist kein gültiger
effektiver Laufwert. Der persistente Feldname `bandCelsius` und das externe
Feld `target_qualification_band_c` bleiben unverändert. Wäre bei einem
erneuten Quellenabgleich eine andere kanonische Bedeutung belegt, würde die
Umsetzung mit `SOURCE_OF_TRUTH_CONFLICT` angehalten und diese Entscheidung
nicht überschrieben.

In `PREHEATING` wird immer der Luftsnapshot verwendet, unabhängig vom
späteren Produkt-/Luftmodus. In `REACHING_TARGET` und
`QUALIFYING_TARGET` wird der von #21 ausgewählte Produkt- beziehungsweise
Luftsnapshot verwendet. Ein ungültiger/fehlender Produktwert wird im
Produktmodus nicht durch Luft ersetzt.

### 11.2 Zustandsbehafteter, aber entscheidungsreiner Vertrag

Der Evaluator besitzt flüchtigen Zustand, aber die Berechnung mutiert ihn
nicht irreversibel:

```text
evaluateQualification(currentEvaluatorState, input)
  -> QualificationDecision {
       progress
       expectedEvaluatorState
       candidateEvaluatorState
       expectedProcessState
       processEffect
     }

applyQualificationDecision(decision, commitContext)
```

`expectedEvaluatorState` und die Ziel-/Rollen-/Phasenrevision verhindern, dass
eine veraltete Entscheidung angewendet wird. `candidateEvaluatorState`
enthält insbesondere `creditedInBandMillis`, letzten gültigen Sample-
Timestamp, letzten Qualifikationszustand und `graceStartedAtMillis`.

Die verbindliche Commit-/Apply-Reihenfolge lautet:

1. Aus dem aktuellen Live-Evaluatorzustand und der aktuellen Eingabe wird
   eine Kandidatenentscheidung berechnet; Live-Zustand und
   `ProcessRuntimeState` werden dabei nicht mutiert.
2. Führt die Entscheidung zu einer persistierbaren Prozessänderung, werden
   Prozesskandidat und Evaluatorkandidat gemeinsam vorbereitet.
3. Der bestehende Write-before-Apply-Pfad persistiert den Prozesskandidaten.
4. Nur nach erfolgreicher Persistenz und erfolgreichem
   `applyProcessTransition()` wird der Evaluatorkandidat live übernommen.
5. Schlägt Persistenz oder Apply fehl, bleibt der vorherige Evaluatorzustand
   wirksam; der Kandidat wird verworfen und der bestehende fail-closed-
   Persistenzfehlerpfad bleibt maßgeblich.

In einem Zyklus ohne persistierbare Prozessänderung darf der Kandidat nach
der normalen erfolgreichen Evaluation als RAM-only-Zustand übernommen
werden. Es wird keine Persistenz des Evaluatorzustands eingeführt.

TargetChanged, Rollenwechsel, Recovery und Phasenwechsel benutzen dieselbe
Grenze. Ein erfolgreicher Retry berechnet aus dem alten Live-Zustand erneut
und schreibt keine Zeit doppelt gut. Ein `Process-Apply`-Fehler nach
erfolgreicher Persistenz übernimmt den Evaluator ebenfalls nicht; die
bestehenden Recovery-/Fault-Regeln verhindern eine stille Fortsetzung.

### 11.3 Zeit- und Resetsemantik

- Erster Sample: Zustand wird als neuer Zeitanker vorbereitet; es gibt keine
  Zeitgutschrift.
- Gleicher Timestamp: `dt = 0`; keine doppelte Gutschrift, die
  Bandklassifikation darf sich dennoch ändern.
- Rückwärts laufender Timestamp, zu große Lücke, checked Overflow oder
  ungültige Konfiguration: `Invalid`, kein Kandidat mit gutgeschriebener
  Zeit; der alte gültige Zustand bleibt bis zur expliziten Recovery-/Reset-
  Grenze erhalten.
- Neue Läufe und Recovery löschen Episode, Grace und Kredit.
- `TargetChanged` löscht Episode, Grace und Kredit erst nach erfolgreichem
  Lauf-/Prozess-Commit.
- Ein bestätigter Sensorrollenwechsel löscht Episode, Grace und Kredit an
  derselben Rollenwechsel-Commit-Grenze; es gibt keinen Rollenfallback.
- Ein Wechsel aus `QUALIFYING_TARGET` zurück nach `REACHING_TARGET` löscht
  die Qualifikation; die alte Episode wird nicht bei Rückkehr fortgesetzt.

### 11.4 Vollständige Bedeutung der Progress-Werte

- `Unavailable`: Es fehlt verwertbare Evidenz oder eine separat erforderliche
  Voraussetzung ist noch nicht verfügbar, zum Beispiel kein erlaubter
  Snapshot der zuständigen Rolle. Es liegt nicht zwingend ein fehlerhafter
  Wert vor; der Kandidat schreibt keine Zeit gut.
- `Invalid`: Ein vorhandener Sample, Timestamp oder Qualifikationsparameter
  ist strukturell/semantisch ungültig, etwa nicht-finite Messung,
  Rückwärtszeit, zu große Lücke, checked Overflow oder `bandCelsius == 0`.
  Der Kandidat schreibt keine Zeit gut. `Invalid` ist nicht dasselbe wie
  `Unavailable`.
- `OutsideBand`: Verwertbarer Sample liegt außerhalb des inklusiven Bandes
  und es gibt keine fortsetzbare aktive Grace-Episode. Kredit wird nicht
  erhöht.
- `Grace`: Verwertbarer Outside-Sample bei vorhandener Gutschrift und noch
  nicht abgelaufener Gnadenzeit. Kredit bleibt unverändert; außerhalb wird
  keine Zeit gutgeschrieben.
- `InBand`: Verwertbarer Sample liegt im Band und die aktuelle Episode ist
  noch nicht vollständig. Nur aufeinanderfolgende verwertbare InBand-Samples
  mit checked Zeitabstand erhöhen den Kredit; ein Episode-Neustart liefert
  zunächst `InBand` mit Kredit `0`.
- `Complete`: Die checked gutgeschriebene InBand-Zeit erreicht die
  Qualifikationsdauer. Der Wert ist eine positive Qualifikationsevidenz, aber
  kein direkter Zustandsübergang aus `REACHING_TARGET`.

### 11.5 Grace-Ablauf und direkte Rückkehr ins Band

Vor jeder Rückkehr aus `Grace` wird bei verwertbarem aktuellem Timestamp die
checked Zeit
`outsideElapsed = currentTimestamp - graceStartedAtMillis` ausgewertet,
unabhängig davon, ob der aktuelle Sample Outside oder InBand ist. Die
Gleichheit `outsideElapsed == effectiveGraceMillis` gehört zur abgelaufenen
Seite.

- Bei `< effectiveGraceMillis` bleibt die alte Gutschrift erhalten. Ein
  aktueller InBand-Sample kehrt mit `InBand` zurück, schreibt für die
  Rückkehr selbst `0` neue Millisekunden gut und setzt die Folgeanker für
  weitere InBand-Samples.
- Bei `>= effectiveGraceMillis` wird die alte Gutschrift **zuerst vollständig
  verworfen** und die Grace-Episode beendet. Ist der aktuelle Sample dann
  InBand, startet er eine neue Episode und liefert `InBand` mit `0` Kredit;
  ist er Outside, liefert er `OutsideBand` ohne alte Gutschrift.
- Bei `effectiveGraceMillis == 0` wird keine fortsetzbare Grace-Episode
  eröffnet; Outside verwirft eine vorhandene Episode sofort.

Tests decken direkte InBand-Rückkehr mit Grace-Zeit `<`, `==` und `>` sowie
Outside-Samples am und nach dem Ablauf ab. Ein Sample genau am Grace-Ende ist
kein weiterer Outside-Sample erforderlich, damit der alte Kredit verworfen
wird.

### 11.6 Prozessphase-Wirkung jedes Progress-Wertes

| Phase / Progress | `Unavailable` | `Invalid` | `OutsideBand` | `Grace` | `InBand` | `Complete` |
|---|---|---|---|---|---|---|
| `PREHEATING` | bleibt Preheating, keine Freigabe | bleibt Preheating, Fehlerdiagnose | bleibt Preheating, Episode ggf. löschen | bleibt Preheating, Kredit nicht erhöhen | bleibt Preheating, Episode führen | `WaitingForProduct`, `PreheatQualified` |
| `REACHING_TARGET` | bleibt Reaching | bleibt Reaching | bleibt Reaching | **`QUALIFYING_TARGET`** | **`QUALIFYING_TARGET`** | **`QUALIFYING_TARGET`**, niemals direkt Fermenting/Manual |
| `QUALIFYING_TARGET` | `ReachingTarget`, alte Qualifikation löschen | `ReachingTarget`, alte Qualifikation löschen | `ReachingTarget`, alte Qualifikation löschen | bleibt Qualifying | bleibt Qualifying | `FERMENTING` oder `MANUAL_HOLDING` |
| `COOLING` | wird ignoriert | wird ignoriert | wird ignoriert | wird ignoriert | wird ignoriert | wird ignoriert |

Bei `REACHING_TARGET + Complete` wird also zuerst die bestehende
`REACHING_TARGET -> QUALIFYING_TARGET`-Topologie persistiert. Der nächste
Zyklus darf mit derselben aktuellen Evidenz
`QUALIFYING_TARGET + Complete` und damit den normalen zweiten Übergang
vorlegen. Ein direkter Übergang von `REACHING_TARGET` nach
`FERMENTING`/`MANUAL_HOLDING` ist unzulässig und erhält einen expliziten
Negativtest.

## 12. ProcessSignals und bestehende Zustandsautomat-Topologie

### 12.1 Priorität und Übergänge

`criticalFault` wird vor jedem normalen Qualifikations- oder Cooling-
Fortschritt ausgewertet und behält seine bestehende Priorität. Die
Qualifikationsauswertung benutzt nur `qualificationProgress`:

- `PREHEATING`: nur `Complete` führt wie bisher zu `WAITING_FOR_PRODUCT`;
  alle anderen Werte halten die Phase beziehungsweise aktualisieren nur
  ihren Kandidatenmarker.
- `REACHING_TARGET`: jeder positive Qualifikationswert, einschließlich
  `Grace`, `InBand` und bereits berechnetem `Complete`, führt zunächst nach
  `QUALIFYING_TARGET`.
- `QUALIFYING_TARGET`: nur `Complete` führt nach `FERMENTING` oder
  `MANUAL_HOLDING`; `Unavailable`, `Invalid` und `OutsideBand` führen wie
  bisher zurück nach `REACHING_TARGET`, `Grace` und `InBand` halten die Phase.
- `COOLING`: ausschließlich `coolingTargetConditionValid` darf den
  bestehenden `CoolingTargetReached`-Übergang auslösen. Der Progresswert ist
  dort vollständig bedeutungslos.

`targetReachStartedAtMillis` und `TargetReachTimeExceeded` bleiben in
`REACHING_TARGET` und `QUALIFYING_TARGET` aktiv, auch während `Grace`. Ein
Warnereignis darf die bestehende Qualifikations-/Phasenentscheidung nur
ergänzen, nicht ihre Topologie ersetzen.

### 12.2 Cooling ohne Qualifikationslogik

`decideCooling()` liest ausschließlich `coolingTargetConditionValid`:

- bei `CompletionMode::CoolThenFinish` führt `true` zu
  `COMPLETED` mit `CoolingTargetReached` und der bestehenden
  Completion-Nachricht;
- bei `CompletionMode::CoolAndHoldForDuration` führt `true` zu
  `COOL_HOLDING` mit `CoolingTargetReached`;
- bei `false` bleibt `COOLING`;
- `qualificationProgress::Complete`, Gnadenzeit und Qualifikationsdauer
  dürfen diesen Pfad nicht beeinflussen.

Gezielte Regressionstests beweisen `COOLING -> COMPLETED` und
`COOLING -> COOL_HOLDING` sowie beide CompletionModes und die Unabhängigkeit
vom Qualifikationsprogress.

### 12.3 Bestehende Ereignisse und Recovery

Die vorhandene `TargetChanged`-Topologie bleibt erhalten; nur die effektive
Zielquelle und die Commit-Grenze aus Abschnitt 10 werden ergänzt. Recovery
aus einer alten `QUALIFYING_TARGET`-Phase beginnt weiterhin neu über
`REACHING_TARGET`; der alte Evaluatorkredit und
`qualificationValidSinceMillis` werden nicht als Qualifikation fortgesetzt.

`CoolingTargetReached` bleibt für alle CompletionModes unverändert. Es gibt
kein neues Prozessereignis, kein neues Persistenzfeld und keinen Schema-Bump.

## 13. `qualificationValidSinceMillis` und Persistenz

`ProcessRuntimeState::qualificationValidSinceMillis` bleibt als bestehendes
Wire-/Persistenzfeld strukturell erhalten. Es wird nicht in ein neues Feld
umbenannt, nicht entfernt und nicht als neuer Wirevertrag erweitert.

Seine verbleibende Bedeutung ist ausschließlich ein persistierter
Prozessmarker der aktuellen Qualifikationsphase/-episode für Diagnose,
Recovery-Topologie und bestehende Vertragskompatibilität. Es ist **nicht** die
Quelle für gutgeschriebene Qualifikationszeit. Der flüchtige Evaluator ist die
alleinige Wahrheit für `creditedInBandMillis`.

Verbindliche Lebenszyklusregeln:

- `nullopt` bei neuem Lauf, Reset, Recovery-Rebase und vor Beginn einer
  akzeptierten Qualifikation;
- beim erfolgreichen Eintritt in eine aktuelle Qualifikationsphase wird der
  Marker mit dem dafür verwendeten Sample-Zeitpunkt gesetzt, auch wenn der
  Kandidat bereits `Complete` meldet;
- während die aktuelle Qualifying-Phase durch `Grace` oder `InBand` erhalten
  bleibt, bleibt er als Phasenmarker erhalten; daraus wird keine Zeit
  berechnet. `Unavailable` und `Invalid` führen gemäß Abschnitt 11.6 aus
  `QUALIFYING_TARGET` heraus und löschen ihn mit diesem Kandidaten;
- bei bestätigtem Qualifikationsverlust, Rückkehr nach `REACHING_TARGET`,
  `TargetChanged`, Rollenwechsel, Recovery aus alter Qualifying-Phase,
  Abschluss oder Cooling wird er gelöscht;
- beim direkten Grace-Ablauf mit InBand wird er erst im neuen Kandidaten und
  nur an der zugehörigen Commit-/Apply-Grenze auf den neuen Episodezeitpunkt
  gesetzt;
- ein alter persistierter Marker darf weder Kredit erzeugen noch einen
  aktuellen Zielwert, Bandwert oder Zeitstempel ersetzen.

Persistenz- und Recovery-Code bleibt für das vorhandene optionale uint64-
Feld wire-kompatibel. Es werden keine Evaluatorfelder, Demand-Sequenzen,
Feedbackfenster, PI-Integrale oder alten Aktorzustände persistiert. Ein
Schema-Bump und ein stiller Wireformat-Vertrag sind ausgeschlossen.

## 14. Commit-/Apply-Cut-Points und Fehlerfälle

Die Umsetzung bleibt in kleinen, jeweils kompilierbaren Schnitten:

### Commit 1 – schmale Wert- und Prozesssignalgrenzen

- `AbstractControlDirection` wertgleich in den schmalen gemeinsamen
  Werttyp verschieben;
- genau ein `QualificationProgress` und den erweiterten `ProcessSignals`-
  Vertrag einführen;
- bestehende #21-Konsumenten aktualisieren;
- keine PI- oder Prozesslogik vorziehen.

Nachweis mindestens:

```text
pio test -e native --filter test_sensor_selection
pio test -e native --filter test_process_state_machine
```

### Commit 2 – reine PI-Auswertung

- vier Parametersätze, Neutralband, Zeitvertrag, Luftbegrenzung und
  Status-/Reason-Invarianten implementieren;
- Demand-Sequenz und schmalen PreviousDemandFeedback-Vertrag einführen;
- keine Aktor-/Safety- oder Hardwareabhängigkeit.

Nachweis mindestens:

```text
pio test -e native --filter test_temperature_control
```

### Commit 3 – Qualifikation und Zustandsautomat

- reine Decide-/Apply-Evaluatorgrenze implementieren;
- vollständige Progress-, Sensorrollen-, Zeit-, Grace- und Resetsemantik;
- ProcessSignals-Topologie einschließlich getrenntem Cooling-Signal;
- effektive Zielquelle und TargetChanged-Kopplung.

Cut-Point-Tests sind verpflichtend:

- `REACHING_TARGET -> QUALIFYING_TARGET`, Persistenz vor Commit schlägt fehl;
- Persistenz gelingt, `applyProcessTransition()` schlägt fehl;
- erfolgreicher Retry ohne doppelte Zeitgutschrift;
- `TargetChanged` mit fehlgeschlagenem Persistenz- und Apply-Pfad;
- `REACHING_TARGET` schlägt nie direkt `FERMENTING`/`MANUAL_HOLDING` vor;
- Grace-Rückkehr `<`, `==`, `>` mit direktem InBand-Sample;
- Zieländerung in Preheating, Reaching, Qualifying und manuellem Lauf;
- `COOLING -> COMPLETED` und `COOLING -> COOL_HOLDING`.

Nachweis mindestens:

```text
pio test -e native --filter test_process_state_machine
pio test -e native --filter test_temperature_control
pio test -e native --filter test_run_commands
pio test -e native --filter test_program_models
```

### Commit 4 – Persistenz-/Recovery- und direkte Konsumentenregressionen

- ausschließlich notwendige Vertragsanpassungen an bestehendem
  `qualificationValidSinceMillis`-Codec und Coordinator-
  Write-before-Apply-Vertrag;
- keine neuen Wirefelder und kein Schema-Bump;
- #21-Regression, bestehende Zustandsautomatpfade und Laufanpassungen
  abschließend gezielt testen.

Nachweis mindestens:

```text
pio test -e native --filter test_sensor_selection
pio test -e native --filter test_run_checkpoint_codec
pio test -e native --filter test_run_persistence_coordinator
pio test -e native --filter test_process_state_machine
pio test -e native --filter test_run_commands
```

Die Filter werden nicht parallel gegen dasselbe native Buildverzeichnis
gestartet. Ausgelassene Tests gelten nicht als bestanden.

## 15. Direkter Testkatalog

Zusätzlich zu den Commit-Gates muss der gezielte Nachweis folgende Orakel
enthalten:

- PI: erster, gleicher, rückwärts laufender, zu großer und overflow-
  gefährdeter Timestamp mit exakt erwarteter Demand-/Integralwirkung;
- Neutralband: unterschiedliche Heating-/Cooling-Schwellen, beide exakten
  Grenzen und Nachweis, dass dies #23-Hysterese nicht ersetzt;
- Luftmodus und Produktmodus einschließlich früher Luftreduktion und Block;
- Status-/Reason-Invarianten und widersprüchliche Kombinationen;
- Demand-Sequenzen bei gleichem Timestamp sowie fehlendes, altes, doppeltes,
  fremdes und überlaufendes Feedback;
- `FullyAccepted`, `DeferredOrLimited` und `Rejected` mit konservativem
  Integrator-Freeze ohne physische `appliedQuote`;
- alle sechs `QualificationProgress`-Werte und ihre Wirkungen in allen drei
  Qualifikationsphasen;
- `Unavailable` ohne verwertbare Evidenz versus `Invalid` bei fehlerhaftem
  Sample/Vertrag;
- Luftsensor in Preheating für Produkt- und Luftlauf; Produkt-/Luftrolle in
  Reaching und Qualifying ohne stillen Fallback;
- Zielband inklusive Grenze, Halbbreiten-Semantik, bestehende Min-/Max-
  Limits und `0` als ungültiger Wert;
- Qualifikationsdauer ohne Overflow, gleiche Zeit, Rückwärtszeit, große Lücke
  und checked Addition;
- Grace-Direktrückkehr und Gleichheit als Ablauf;
- `qualificationValidSinceMillis` nur als Marker, kein zweiter Kreditpfad;
- Decide ohne Mutation, Apply erst nach Persistenz und Process-Apply,
  Verwerfen bei jedem Fehler und Retry ohne Doppelgutschrift;
- TargetChanged-Commitgrenze und effektive Zielquelle für Programm und
  manuelle Läufe;
- neue Läufe, Neustart/Recovery, Rollenwechsel, Richtungswechsel,
  Sollwertsprung und Phasenwechsel gemäß injiziertem Testprofil ohne
  unbegrenzten Integraltransfer;
- `criticalFault` vor Qualifikations- und Cooling-Fortschritt;
- `targetReachStartedAtMillis` und `TargetReachTimeExceeded` in Reaching und
  Qualifying, auch während Grace;
- unveränderte `TargetChanged`-Topologie, `CoolingTargetReached` für alle
  CompletionModes und Recovery aus alter Qualifying-Phase über Reaching;
- kein neues Persistenz-/Wirefeld und kein Schema-Bump.

## 16. Notwendige Dokumentations- und Implementierungsnachführung

Während der späteren Umsetzung werden nur die fachlich notwendigen Stellen
aktualisiert: Prozesssignal-/State-Machine-Vertrag, Temperaturregelungs-
vertrag, `docs/ACTUATOR_TIMING.md`, `docs/SENSOR_TUNING_COMMISSIONING.md`,
`docs/PROGRAMS.md`, relevante Lauf-/Persistenzverträge und die betroffenen
Testdokumentationen. Der Komponentenregisterverweis bleibt
`docs/THIRD_PARTY_COMPONENTS.md`.

Es werden keine Produktionszahlen, GPIOs, Sensoranschlüsse, Aktormodelle,
Safetygrenzen oder Hardwareergebnisse erfunden. Der effektive Grace-Wert und
die Integrator-Transition-Policy bleiben bis zur Commissioningentscheidung
validierte Eingaben ohne #22-Programmschema-Bump. Eine Änderung dieser
offenen Eigentümerschaft oder der Produktionspolicy ist ein separates
Owner-Gate.

## 17. Abnahmekriterien und Übergabe

Der Plan ist erst umsetzungsbereit, wenn der Owner den exakten neuen
Plan-Commit freigibt. Die Freigabe muss insbesondere umfassen:

- die getrennten `qualificationProgress`-/`coolingTargetConditionValid`-
  Bedeutungen;
- die erhaltene `REACHING_TARGET -> QUALIFYING_TARGET ->
  FERMENTING/MANUAL_HOLDING`-Topologie;
- die effektive Zieltemperaturquelle und TargetChanged-Grenze;
- die Evaluator-Decide-/Apply-Semantik;
- den dispositionsbasierten, sequenzidentifizierten Freeze-Feedback;
- die offene, injizierte Integrator-Transition-Policy;
- die vollständige Progress-/Grace-/Zeitmatrix;
- die Entscheidung `bandCelsius = einseitige Toleranz/Halbbreite` bei
  unverändertem persistentem Feld und Wertebereich;
- `qualificationValidSinceMillis` als Marker ohne zweite Zeitwahrheit;
- die expliziten COOLING-, #21-, Recovery-, Timer- und Persistenzregressionen.

Nach erfolgreicher Planfreigabe werden die beschriebenen Commit-Schnitte
umgesetzt. Bis dahin bleibt der Draft unverändert in Plan-only-Zustand; kein
Firmwaretest, kein Produktionscode und keine PR-Statusänderung wird aus
dieser Revision abgeleitet.
