# Plan: Issue #22 – Zeitproportionale PI-Regelung und Luftbegrenzung

## 1. Status, Scope und Owner-Gate

- Revision: **4**.
- Live-Issue: #22, offen, Status `PLANNED_SPEC_PENDING`.
- Draft-PR: #104, Branch `agent/issue-22-pi-regelung-plan` -> `main`.
- Planpfad: `docs/tasks/issue-22-pi-control-air-limits-plan.md`.
- Diese Revision ist ein vollständiger, eigenständig gültiger Plan. Sie setzt
  keine frühere Planrevision als fachliche oder normative Quelle voraus.
- Planbasis: `main` @
  `10ff98eca4d6f64cc453571d66d4c3b18729b18e`.
- Ausgangs-HEAD vor dieser Revision:
  `7ac7fc6dfa099df370281757120d3636805de37a`.
- Der exakte Commit dieser Revision wird nach dem Commit mit voller SHA in
  PR-Beschreibung und aktuellem SESSION-HANDOVER ausgewiesen.
- Die Umsetzung bleibt gesperrt, bis der Owner exakt diesen neuen Plan-Commit
  mit `PLAN APPROVED` beziehungsweise `Approved plan commit: <SHA>` freigibt.
- Diese Revision ändert ausschließlich Plan-/Roadmap-Dokumentation und
  zugehörige PR-Metadaten. Sie implementiert keine Produktionslogik,
  produktiven Tests, Hardware-, GPIO-, Toolchain- oder CI-Änderung.
- PR #104 bleibt Draft. Es gibt kein `Ready for review`, keinen Merge, kein
  Auto-Merge und kein Branch-Löschen.

```text
CONTEXT_BASELINE_BRANCH: agent/issue-22-pi-regelung-plan
CONTEXT_BASELINE_SHA: 10ff98eca4d6f64cc453571d66d4c3b18729b18e
CONTEXT_HEAD_BEFORE_REVISION: 7ac7fc6dfa099df370281757120d3636805de37a
CONTEXT_PLAN_SHA: NONE (wird nach dem Commit dieser Revision eingetragen)
CONTEXT_REFRESH_MODE: FULL
SOURCE_OF_TRUTH_CONFLICT: NONE festgestellt; die R1-PI-Gleichung ist in den
  kanonischen Quellen nicht festgelegt und wird hier als explizite
  Ownerentscheidung formuliert. bandCelsius bleibt einseitige Toleranz.
```

## 2. Ziel, Reihenfolge und Nicht-Ziele

Issue #22 liefert einen deterministischen, hardwarefreien und nativ testbaren
Fachkern für Release 1:

- zeitproportionale PI-Regelung mit exakt definierter Quote-Mathematik;
- vier getrennte Maschinenparametersätze: Luft/Heizen, Luft/Kühlen,
  Produkt/Heizen und Produkt/Kühlen;
- richtungsabhängige einseitige Neutralbandschwellen und begrenzte Zeitquote;
- Produktregelung mit früher Luftbegrenzung;
- Luftregelung als eigener normaler Modus;
- vollständige Zielqualifikation für leeres Vorheizen und spätere Zielphase;
- eine eindeutige, sequenzierte ControlRequest für HEAT, OFF und COOL;
- ein schmaler, dispositionsbasierter Anti-Windup-Vertrag für die spätere
  #23/#24-Integration.

Nicht Bestandteil von #22 sind GPIOs, BTS7960-, Lüfter- oder sonstige
Aktorsignale, Aktorfreigabe, Mindestzeiten, Totzeit, Richtungswechsel-
Hysterese, Impulsakkumulator, Kühlkörpersensor-Auswertung, systemweite
Safety-/Fehlerentscheidungen, aktive Kaskadenregelung, D-Anteil, Autotuning,
eine externe Regelbibliothek oder Persistenz von PI-/Qualifier-RAM-Zustand,
ControlRequest-Identitäten oder Feedbackfenstern.

Die fachliche Reihenfolge bleibt:

```text
#21 persistierter Laufmodus und Sensorselektion
  -> #22 effektive Prozessrolle, PI und abstrakte ControlRequest
  -> #23 Aktorplanung
  -> #24 Safety-/Fehlerkern
```

#22 erzeugt nur die abstrakte Anforderung `HEAT`, `OFF` oder `COOL` mit
Zeitquote. #23 bleibt Aktorplaner; #24 bleibt Safety-/Fehlerkern. Die
nachgelagerte Kette darf eine gültige Anforderung ablehnen, begrenzen oder
aufschieben, ohne dass #22 daraus eine Safetyentscheidung macht.

## 3. Verbindliche Quellen und Wiederverwendung

Vor der Umsetzung sind mindestens diese Quellen erneut gegen ihren dann
aktuellen Stand zu prüfen:

- `docs/SPECIFICATION_REVIEW.md` als Dokumentationspriorität;
- `docs/DECISIONS.md`, `docs/ADR-013_REUSABLE_DEVICE_PLATFORM.md` und der
  Modulindex;
- `docs/AGENT_WORKFLOW.md` und `docs/ENGINEERING_PRINCIPLES.md`;
- `docs/STATE_MACHINE.md` und der bestehende
  `process_state_machine.hpp/.cpp`-Vertrag;
- `docs/TEMPERATURE_CONTROL.md` für Regelstrategie, Sensorrollen und
  Vorheizvertrag;
- `docs/PROGRAMS.md`, `run_commands.hpp/.cpp`, `ActiveRun::effectiveValues()`
  und `ManualRunPlan`;
- `docs/ACTUATOR_TIMING.md` für die Identität jeder gültigen
  Regelanforderung, Impulsakkumulator, Mindestzeiten, Totzeit und
  Integratorverhalten;
- `docs/SENSOR_TUNING_COMMISSIONING.md` für Anti-Windup,
  Sensorrollenwechsel und Commissioning-Eigentümerschaft;
- die #20-Sensorqualitätsverträge und der #21-
  `SensorSelection`-/`CrossRolePlausibilityContext`-Vertrag;
- `docs/RUN_PERSISTENCE.md`, `run_persistence_codec` und
  `run_persistence_coordinator` für Write-before-Apply und Recovery;
- `docs/THIRD_PARTY_COMPONENTS.md`. Arduino PID und QuickPID bleiben
  `NOT_SELECTED`; die R1-PI-/Safetylogik bleibt projektspezifisch.

Der kanonische Komponentenregisterpfad ist exakt
`docs/THIRD_PARTY_COMPONENTS.md`.

Wiederzuverwenden sind insbesondere `RunSensorMode`, seine #21-Semantik und
die aktive Sensorrollen-/Regelberechtigung, `AbstractControlDirection`,
`CrossRolePlausibilityContext`, `SensorQualitySnapshot`, die bestehende
Prozesszustands-Topologie, `ProcessRuntimeState` einschließlich
`qualificationValidSinceMillis`, `ActiveRun`-Laufanpassungen,
`ManualRunPlan`, die CompletionMode-Verträge und die abstrakten
Plattformports. Kein paralleler Sensor-, Safety-, Prozess- oder
Persistenzvertrag wird still erfunden.

## 4. Zuständigkeiten und schmale Werttypen

### 4.1 #22, #23 und #24

#22 nimmt einen bereits aufgelösten Control-Kontext, qualitätsgeprüfte
Sensorbeobachtungen, das effektive Regelziel, ein validiertes PI-Profil und
den Vorzyklus-Feedbackvertrag entgegen. Der Kern kennt weder Prozessphase
noch #21-Fallbacklogik und erzeugt keine Aktorfreigabe.

#23 entscheidet nach der abstrakten ControlRequest über Aktorplanung
einschließlich Mindest-Auszeit, Mindest-Einschaltzeit, Polaritätstotzeit,
Impulsakkumulator, Gegenrichtungsbestätigung, Richtungswechsel und
Luft-/Kühlkörpersensor. #24 besitzt die systemweite Safety-/Fehlerentscheidung.

Die #21-Sensorselektionsfreigabe darf als kanonische Regelberechtigung
konsumiert werden. #22 implementiert keine neue Sensorwahl, keinen Fallback
und kein allgemeines `ControlSafetyPermission`.

### 4.2 Gemeinsame und schmale Prozesswerttypen

`AbstractControlDirection` wird, falls für die gemeinsame ControlRequest-
Verwendung erforderlich, aus `sensor_selection.hpp` in einen schmalen
gemeinsamen Werttyp-Header verschoben. Es gibt genau eine Definition mit
unverändert gleicher #21-Semantik:

```text
AbstractControlDirection = Unknown | Heating | Cooling | Idle
```

`CrossRolePlausibilityContext` und alle bestehenden #21-Felder bleiben
wertgleich. `test_sensor_selection` ist in Commit 1 und im finalen gezielten
Nachweis Pflicht.

Der tatsächliche PI-Regelsensor ist ein eigener, schmaler Werttyp:

```text
ControlSensorRole = Air | Product
```

`RunSensorMode = Product | Air` bleibt der persistierte/kanonische #21-
Laufmodus. `ControlSensorRole` ist dagegen die für den aktuellen
Prozesszustand tatsächlich verwendete PI-Rolle. Die beiden Werte werden nicht
umbenannt oder gleichgesetzt.

`QualificationProgress` gehört nicht in den breiten Regler-/Diagnoseheader.
Es wird genau einmal in einem schmalen `process_signal_types.hpp` oder dem
gleichwertigen Prozesssignal-/State-Machine-Vertrag definiert:

```text
QualificationProgress = Unavailable | Invalid | OutsideBand |
                        Grace | InBand | Complete
```

Der `TargetQualificationEvaluator` produziert diesen Prozesssignaltyp; der
Zustandsautomat konsumiert ihn. Eine zweite Enumdefinition und eine implizite
Kopplung von Prozessstatus an `temperature_control_types.hpp` sind
unzulässig.

Der Prozesssignalvertrag lautet mindestens:

```text
ProcessSignals
  qualificationProgress: QualificationProgress = Unavailable
  coolingTargetConditionValid: bool = false
  criticalFault: bool = false
```

`qualificationProgress` wird ausschließlich in `PREHEATING`,
`REACHING_TARGET` und `QUALIFYING_TARGET` ausgewertet.
`coolingTargetConditionValid` wird ausschließlich in `COOLING` ausgewertet.
Die beiden Bedeutungen werden nicht über einen gemeinsamen Bool, Gnadenzeit
oder Qualifikationsdauer für COOLING zusammengelegt.

## 5. Effektiver Prozess-Control-Kontext

Der Orchestrator löst vor jeder PI-Evaluation einen vollständigen, bereits
validierten Kontext auf. Der PI-Kern erhält daraus nur `ControlSensorRole` und
`targetCelsius`; er interpretiert weder Prozessphase noch `RunSensorMode`.

### 5.1 Effektive Regelsensorrolle

Die Auflösung ist verbindlich:

| Prozessphase | effektive PI-Regelsensorrolle |
|---|---|
| `PREHEATING` | immer `Air` |
| `WAITING_FOR_PRODUCT` | immer `Air` |
| `REACHING_TARGET` | der aktive #21-Laufmodus |
| `QUALIFYING_TARGET` | der aktive #21-Laufmodus |
| `FERMENTING` | der aktive #21-Laufmodus |
| `COOLING` | der aktive #21-Laufmodus |
| `COOL_HOLDING` | der aktive #21-Laufmodus |
| `MANUAL_HOLDING` | der aktive #21-Laufmodus des wirksamen ManualRunPlan |

Im Produktlauf bleibt `activeRunSensorMode == Product` unverändert, obwohl
die effektive Rolle in `PREHEATING` und `WAITING_FOR_PRODUCT` `Air` ist. Diese
Luftrolle ist ein expliziter leerer-Schrank-Prozessvertrag und kein stiller
#21-Fallback. Im Luftlauf bleibt die Rolle in allen temperaturgeregelten
Phasen `Air`.

Bei `ProductInserted` kann ein Produktlauf erstmals von effektiver `Air`-
Rolle auf `Product` wechseln. Dieser tatsächliche Rollenwechsel wird nach
erfolgreichem Prozess-Commit an die Integrator-Transition-Grenze aus
Abschnitt 10 übergeben. Ein Rollenwechsel wird nicht schon durch den
persistierten Modus beim Laufstart vorweggenommen.

Wenn #21 während eines laufenden Laufs seine aktive Regelrolle nach seinen
eigenen Auswahl-/Fehlerregeln ändert, wird nur die wirksame Rollenänderung
weitergereicht. #22 implementiert diese Auswahl nicht neu.

### 5.2 Effektives Regelziel

Der Resolver liefert einen kleinen Vertrag, beispielsweise:

```text
EffectiveControlTarget
  targetCelsius: finite double
  targetKind: FermentationRun | CoolingCompletion | ManualRun
  sourceRevision: checked RAM/context identity
  valid: bool
```

Die Quelle ist vollständig phasenbezogen:

| Prozessphase | PI-Ziel |
|---|---|
| `PREHEATING` | aktuell effektives Fermentations-/Laufziel |
| `WAITING_FOR_PRODUCT` | aktuell effektives Fermentations-/Laufziel |
| `REACHING_TARGET` | aktuell effektives Fermentations-/Laufziel |
| `QUALIFYING_TARGET` | aktuell effektives Fermentations-/Laufziel |
| `FERMENTING` | aktuell effektives Fermentations-/Laufziel |
| `COOLING` | bestehendes `completion.coolingTargetCelsius` |
| `COOL_HOLDING` | dasselbe bestehende Kühlziel |
| `MANUAL_HOLDING` | wirksames Ziel des `ManualRunPlan` |

Für einen Programmlauf kommt das Fermentationsziel nach allen bestätigten
`RunAdjustment`-Revisionen aus
`ActiveRun::effectiveValues().targetTemperatureCelsius`. Der unveränderte
Quell-Programmschnappschuss bleibt davon getrennt. Eine Zielanpassung des
Fermentationsziels deutet den Kühlzielwert nicht um.

Für einen manuellen Lauf kommt das Ziel aus dem wirksamen
`ManualRunPlan`. Für `COOLING` und `COOL_HOLDING` ist ausschließlich das
bestehende Completion-Objekt einschließlich `coolingTargetCelsius` maßgeblich;
das Fermentationsziel darf dort nicht als Ersatz dienen.

Wenn die Phase keinen temperaturgeregelten Laufvertrag besitzt – insbesondere
`BOOT`, `SAFE_BOOT`, `STANDBY`, `COMPLETED`, `FAULT`, `SERVICE_MODE` und
`RECOVERY_EVALUATION` – erzeugt #22 keine gültige ControlRequest. Ein fehlender
oder strukturell unzulässiger Zielwert erzeugt ebenfalls keine gültige
Anforderung.

`FinishWithoutCooling` darf strukturell nicht in `COOLING` eintreten. Ein
fehlender Kühlzielwert für einen kühlenden CompletionMode ist kein stiller
Fallback auf das Fermentationsziel, sondern ein ungültiger Lauf-/Control-
Kontext.

## 6. PI-Datenmodell und exakte R1-Mathematik

### 6.1 PI-Eingabe und Maschinenprofile

Der reine PI-Kern liest keine Ports, keine globale Uhr und keine
Prozessphase:

```text
TemperatureControlInput
  sampleTimestampMonotonicMillis: uint64_t
  targetCelsius: finite double
  controlSensorRole: ControlSensorRole
  air: SensorQualitySnapshot
  product: SensorQualitySnapshot
  previousControlRequestFeedback: optional<PreviousControlRequestFeedback>
```

Der Snapshot der `controlSensorRole` ist der PI-Regelsensor. Die Luft bleibt
zusätzlich im Produktmodus für die frühe #22-Luftbegrenzung erforderlich. Im
Luftmodus ist Luft der Regelsensor; der Produktwert ist nicht notwendig.
Ein fehlender, stale oder failed Regelsensor ist `Unavailable`, kein stiller
Rollenwechsel.

```text
PiDirectionParameters
  proportionalGainQuotePerCelsius: finite, > 0
  integralGainQuotePerCelsiusSecond: finite, > 0
  neutralBandWidthCelsius: finite, > 0
  maximumQuote: finite, (0, 1]
  maximumIntegrationStepMillis: uint64_t, > 0

TemperatureControlParameters
  airHeating
  airCooling
  productHeating
  productCooling
  airLimitLowerBlockCelsius
  airLimitLowerReduceStartCelsius
  airLimitUpperReduceStartCelsius
  airLimitUpperBlockCelsius
```

Alle vier PI-Parametersätze sind getrennt. Für die Release-1-
Produktionssemantik sind `Kp > 0` und `Ki > 0` verbindlich; ein gültiger
Parametersatz darf nicht beide Anteile still auf null setzen. Reale Werte
bleiben `TBD_COMMISSIONING`. Testprofile isolieren P- und I-Wirkung über
Fehler, Zeitabstand und Zustand, nicht über einen ungültigen Null-
Produktionsparametersatz.

Der Integralzustand ist eine endliche, nichtnegative Quote mit der
Richtungsobergrenze `maximumQuote`. Ein separater Integral-Limit-Parameter
wird in R1 nicht eingeführt. Damit gilt für jede Richtung:

```text
0 <= integralContributionQuote <= maximumQuote <= 1
```

### 6.2 Richtung und Neutralband

```text
rawErrorCelsius = targetCelsius - measuredCelsius
```

Die Richtung wird ausschließlich aus dem aktuellen Fehler und der
richtungsabhängigen Schwelle derselben ControlSensorRole bestimmt:

```text
rawErrorCelsius > +heatingNeutralBandWidth -> Heating
rawErrorCelsius < -coolingNeutralBandWidth -> Cooling
sonst                                      -> Idle
```

Die positive und negative Schwelle sind jeweils einseitige
`neutralBandWidthCelsius`-Fehlerschwellen des Heating-/Cooling-
Parametersatzes; sie beschreiben nicht die gesamte Bandbreite. An beiden
Grenzen gilt inklusiv `Idle` und Quote `0`.

Für eine aktive Richtung gilt die Planentscheidung:

```text
directionalThreshold =
  Heating: heatingNeutralBandWidthCelsius
  Cooling: coolingNeutralBandWidthCelsius

activeErrorCelsius = abs(rawErrorCelsius) - directionalThreshold
```

Eine aktive Richtung existiert nur bei `activeErrorCelsius > 0`. Das
Neutralband ersetzt nicht die Gegenrichtungsbestätigung oder
Richtungswechsel-Hysterese aus #23.

### 6.3 PI-Gleichung und Einheiten

Die R1-Gleichung ist in Quote-Einheiten vollständig festgelegt:

```text
P = proportionalGainQuotePerCelsius * activeErrorCelsius

dtSeconds = checked(sampleTimestamp - lastSampleTimestamp) / 1000.0

deltaI = integralGainQuotePerCelsiusSecond
         * activeErrorCelsius
         * dtSeconds

unboundedQuote = P + integralContributionQuote
```

`proportionalGainQuotePerCelsius` hat die Einheit Quote/°C,
`integralGainQuotePerCelsiusSecond` Quote/(°C·s), `P`, `I` und Quote sind
dimensionslos in `[0, 1]`. Der Fehler wird nach Abzug der jeweiligen
Neutralbandschwelle integriert; ein Fehler innerhalb des Neutralbands
integriert nicht.

Der Integral-Kandidat ist checked:

```text
candidateI = min(max(oldI + deltaI, 0), maximumQuote)
```

`oldI + deltaI`, Multiplikationen und die Millisekunden-/Sekundenumrechnung
müssen auf Endlichkeit und Überlauf geprüft werden. Bei einem Rechenfehler
gibt es keine gültige ControlRequest und keinen unsicheren Integralwert.

### 6.4 Verbindliche Auswertungsreihenfolge

Jede Evaluation folgt exakt dieser Reihenfolge:

1. Struktur-, Rollen-, Ziel-, Profil-, Sensor-, Timestamp- und
   Feedbackvalidierung;
2. bereits erfolgreich commitierte Control-Context-Transition anwenden;
3. effektive Rolle, Ziel und aktuelle Richtung bestimmen;
4. `activeErrorCelsius`, P und den aktuellen Luftbegrenzungszustand
   bestimmen;
5. nur wenn kein nachgelagertes Feedback `DeferredOrLimited`/`Rejected` oder
   fehlt und keine aktuelle #22-eigene Begrenzung/Sättigung dagegen spricht,
   `deltaI` berechnen;
6. Integral checked und auf `maximumQuote` begrenzt fortschreiben;
7. `unboundedQuote = P + I` bilden;
8. `maximumLimitedQuote = min(unboundedQuote, maximumQuote)` bilden;
9. die frühe Luftbegrenzung anwenden;
10. Status/Reason setzen und die gültige HEAT/OFF/COOL-ControlRequest mit
    Sequenz und Erzeugungszeitpunkt bilden.

Im Neutralband wird eine gültige `OFF`-ControlRequest erzeugt, keine positive
Integration durchgeführt und der Integralwert nur gemäß der bereits
angewandten Context-/Richtungs-Transition kontrolliert gehalten. Er wird
nicht still durch Zeit oder weitere Off-Samples aufgeladen.

Die #22-eigene Sättigungsprüfung vor Schritt 5 ist ebenfalls eindeutig:
Wenn `P + oldI >= maximumQuote`, wird keine positive `deltaI` berechnet. Ist
`P + oldI < maximumQuote`, darf ein zulässiger Integrationsschritt den
Kandidaten bis genau `maximumQuote` auffüllen; der nächste Sample integriert
dort nicht weiter. Ein checked Kandidat, der durch Addition darüber liegen
würde, wird auf `maximumQuote` begrenzt und löst keinen Overflow aus.

Bei eigener Maximalsättigung, `AirLimitReduced` und `AirLimitBlocked` wird in
der begrenzten Richtung keine weitere positive Integration zugelassen. Ein
bereits zulässig begrenzter Integralwert wird nicht über die Richtungs-
obergrenze hinaus vergrößert.

### 6.5 Frühe Luftbegrenzung ohne Safety-Namen

Die normale #22-Luftbegrenzung verwendet keine Begriffe `Hard` oder `Soft`,
die mit Safety-/Notgrenzen verwechselt werden können:

```text
HEAT: air <= airLimitUpperReduceStartCelsius
        factor = 1, Unrestricted
      reduceStart < air < airLimitUpperBlockCelsius
        factor = (upperBlock - air) / (upperBlock - reduceStart), Reduced
      air >= airLimitUpperBlockCelsius
        factor = 0, Blocked

COOL: air >= airLimitLowerReduceStartCelsius
        factor = 1, Unrestricted
      lowerBlock < air < reduceStart
        factor = (air - lowerBlock) / (reduceStart - lowerBlock), Reduced
      air <= airLimitLowerBlockCelsius
        factor = 0, Blocked
```

An der Reduktionsstartgrenze ist die Quote noch unbeschränkt; an der
Blockgrenze ist die betroffene normale Richtung blockiert. Die Werte müssen
checked validiert werden (`lowerBlock < lowerReduceStart` und
`upperReduceStart < upperBlock`).

Diese Grenzen reduzieren oder blockieren ausschließlich die normale abstrakte
#22-Anforderung. Sie sind keine Safety-Hard-Limits und keine Notgrenzen.
Safety-Eingriffs- und Notgrenzen bleiben #24/#35. Die Luftbegrenzung gilt nur
im Produktregelbetrieb. Im Luftbetrieb ist die Luft der normale Regelsensor
und `AirLimitState = NotApplied`.

## 7. ControlRequest, Status und Invarianten

### 7.1 Gültige ControlRequest

Der Vertrag aus `ACTUATOR_TIMING.md` wird für alle drei gültigen normalen
Anforderungen konkretisiert:

```text
ControlRequestIdentity
  sequence: uint64_t
  createdAtMonotonicMillis: uint64_t

ControlRequest
  identity: ControlRequestIdentity
  direction: Heating | Cooling | Idle
  timeQuote: [0, 1]
```

Die Zuordnung lautet:

```text
HEAT + Quote -> gültige ControlRequest, direction = Heating
OFF          -> gültige ControlRequest, direction = Idle, quote = 0
COOL + Quote -> gültige ControlRequest, direction = Cooling
Unavailable  -> keine gültige ControlRequest
InvalidInput -> keine gültige ControlRequest
```

Jede neu erzeugte gültige HEAT/OFF/COOL-ControlRequest erhält die nächste
RAM-only `uint64_t`-Sequenz und den monotonen Erzeugungszeitpunkt des
aktuellen Samples. Eine neue gültige OFF-Anforderung erhält daher ebenso eine
Identität wie eine aktive Anforderung. Derselbe Timestamp ist zulässig, aber
bei einer neuen gültigen ControlRequest kein Identitätsersatz; er erhält eine
neue Sequenz.

Beim Erreichen `UINT64_MAX` wird nicht gewrappt. Die nächste gültige
Anforderung wird checked als `InvalidInput` ohne ControlRequest abgewiesen;
es gibt keine stillschweigende Wiederverwendung einer alten Identität.
Sequenz, Timestamp, letztes Feedbackfenster und PI-Zustand werden nicht
persistiert und nach Neustart verworfen.

### 7.2 Status-/Reason-Matrix

```text
TemperatureControlStatus = Demand | Off | Unavailable | InvalidInput
TemperatureControlReason = None | NeutralBand | Saturated |
                            AirLimitReduced | AirLimitBlocked |
                            NoCommissioning | SensorUnavailable |
                            InvalidConfiguration | InvalidSample |
                            TimeInvalid | RequestIdentityExhausted
AirLimitState = NotApplied | Unrestricted | Reduced | Blocked | Unavailable
```

| Status | zulässige Reasons | ControlRequest | Richtung/Quote |
|---|---|---|---|
| `Demand` | `None`, `Saturated`, `AirLimitReduced` | zwingend vorhanden | `Heating`/`Cooling`, Quote `>0` und `<=1` |
| `Off` | `NeutralBand`, `AirLimitBlocked` | zwingend vorhanden | `Idle`, Quote `0` |
| `Unavailable` | `NoCommissioning`, `SensorUnavailable` | zwingend nicht vorhanden | `Idle`, Quote `0` nur als Diagnosewert |
| `InvalidInput` | `InvalidConfiguration`, `InvalidSample`, `TimeInvalid`, `RequestIdentityExhausted` | zwingend nicht vorhanden | `Idle`, Quote `0` nur als Diagnosewert |

`AirLimitReduced` tritt nur mit `Demand`, `Reduced` und einer tatsächlich
reduzierten Quote auf. `AirLimitBlocked` tritt nur mit `Off`, `Blocked`,
Quote `0` und einer gültigen OFF-ControlRequest auf. `Saturated` ist eine
Demand-Diagnose für die PI-/Maschinenquotenbegrenzung, sofern nicht die
Luftreduktion die primäre Diagnose ist.

Kein `Demand` trägt `Idle`, kein `Unavailable`/`InvalidInput` trägt eine
ControlRequest, und keine Sperrdiagnose trägt eine positive Quote. Safety-,
Aktor- und Kühlkörpersensorgründe werden nicht als #22-Reasons erfunden.

## 8. Anti-Windup-Feedback und Replay-Schutz

### 8.1 Ursachenschmaler Feedbackvertrag

Eine einzelne aktuelle PI-Quote ist keine physische Aktorleistung. Der #23-
Impulsakkumulator kann kleine Anforderungen über mehrere Fenster sammeln und
später gemeinsam ausführen. #22 behauptet deshalb keine `appliedQuote`.

```text
PreviousControlRequestFeedback
  controlRequestSequence: uint64_t
  disposition: NoIntegratorConstraint |
               DeferredOrLimited |
               Rejected
```

`NoIntegratorConstraint` darf #23/#24 nur für genau diese
ControlRequest melden, wenn für diese konkrete Anforderung kein
nachgelagerter Aufschub und keine Begrenzung vorliegt, die eine positive
Integratorfortschreibung unvertretbar machen würde. Insbesondere ist
`DeferredOrLimited` zu melden bei:

- aktiver Mindest-Auszeit;
- Polaritätstotzeit;
- noch nicht ausführbarem Impulsakkumulator oder Mindestimpuls;
- nachgelagerter Leistungsbegrenzung;
- sonstigem temporärem Aufschub.

`Rejected` bedeutet vollständige Nichtannahme. Die Disposition enthält keinen
Grund, keine Safetyklasse und keine elektrische Momentanquote. #22 kennt
weiterhin keine #23-Zeit, keinen Akkumulator und keine #24-Ursache.

Die R1-Wirkung ist vollständig und konservativ:

- `NoIntegratorConstraint`: normale Integration darf stattfinden, sofern die
  übrigen #22-eigenen Prüfungen, Zeitregeln und Grenzen dies erlauben;
- `DeferredOrLimited`: Integral einfrieren, keine positive Aufladung;
- `Rejected`: Integral einfrieren, keine positive Aufladung;
- fehlendes Feedback für eine vorherige aktive Heating-/Cooling-Anforderung:
  ebenfalls Integral einfrieren, keine positive Aufladung;
- kein Back-Calculation-Gain und kein kontrollierter Abbau aus einer
  behaupteten Quotendifferenz.

Die Auswertung ist nicht zyklisch:

1. Evaluation `n` erzeugt eine gültige ControlRequest oder keinen Request.
2. #23/#24 verarbeiten Request `n` außerhalb von #22.
3. Vor der Integratorfortschreibung von Evaluation `n+1` wird ausschließlich
   Feedback für den letzten aktiven Request `n` geprüft.
4. Danach erzeugt #22 Request `n+1`; das Feedback wird nicht als zweite
   Aktor-/Safetyentscheidung interpretiert.

### 8.2 Feedbackfenster

Nur eine unmittelbar vorherige aktive `Heating`-/`Cooling`-ControlRequest
öffnet ein Feedbackfenster. Für sie gilt:

- fehlendes Feedback im unmittelbaren Folgeaufruf wird als konservatives
  Einfrieren behandelt und das Fenster geschlossen;
- vorhandenes Feedback muss exakt die letzte Request-Sequence referenzieren
  und darf nicht bereits konsumiert sein;
- unbekannte, alte, doppelt konsumierte oder fremde Sequenzen erzeugen
  `InvalidInput / InvalidSample`, keine neue ControlRequest und keinen
  Integratorfortschritt;
- Feedback für eine vorherige gültige OFF-ControlRequest ist unzulässig und
  wird als fremdes/inkompatibles Feedback verworfen; OFF benötigt kein
  Anti-Windup-Feedback;
- ein neuer gültiger OFF-Request schließt das vorherige aktive Fenster;
- ein Unavailable-/InvalidInput-Ergebnis erzeugt keinen neuen Request und
  beendet den normalen Feedbackpfad fail-closed;
- nach Neustart gibt es kein offenes Feedbackfenster.

Der Zeitstempel ist kein Identitätsersatz. Gleiche Timestamps mit neuen
gültigen HEAT/OFF/COOL-Anforderungen erhalten jeweils neue Sequences.

## 9. Integrator-Transition-Policy und Commit-Grenze

### 9.1 Kleiner nicht redundanter Policy-Vertrag

`ACTUATOR_TIMING.md` bleibt maßgeblich: Ein Richtungswechsel setzt den
Integralanteil nicht automatisch pauschal auf null; konkrete Anpassung oder
Teilrücksetzung ist Commissioning.

Der R1-Vertrag verwendet nur zwei tatsächlich unterschiedliche Aktionen:

```text
IntegratorTransitionAction = Reset | BoundedCarry

IntegratorTransitionPolicy
  directionChange: Reset | BoundedCarry
  sensorRoleChange: Reset | BoundedCarry
  targetContextChange: Reset | BoundedCarry
  transitionMaximumCarryQuote: finite double in [0, 1]
```

`Reset` setzt `I = 0`. `BoundedCarry` setzt:

```text
I = min(oldI,
        transitionMaximumCarryQuote,
        newDirectionIntegralLimit)
```

`newDirectionIntegralLimit` ist in R1 `maximumQuote` des neuen
Richtungs-/Rollenparametersatzes. Im Transition-Sample ist unabhängig von
der Aktion keine positive Integration erlaubt. Die strengste neue Grenze
gewinnt. Es gibt keinen zweiten Namen mit derselben Wirkung und keine
Strategiebibliothek.

Die Policy ist vollständig zu validieren und muss explizit injiziert werden;
es gibt keinen Produktionsdefault. Testprofile dürfen konkrete Aktionen und
eine konkrete Carry-Grenze verwenden. Die endgültige Produktionswahl bleibt
`TBD_COMMISSIONING` / #35.

### 9.2 Committe Aufrufgrenze

Der PI-Kern übernimmt persistierbare Kontextänderungen nicht durch bloßen
Vergleich vor ihrem Commit. Der schmale Aufruf lautet sinngemäß:

```text
applyCommittedControlContextTransition(
    state,
    committedTransition,
    validatedIntegratorTransitionPolicy,
    newDirectionIntegralLimit)
```

`committedTransition` benennt ausschließlich konkrete Änderungen:

- angewendete Run-Target-Änderung oder Änderung des effektiven TargetKind;
- angewendeter #21-Sensorrollenwechsel;
- `ProductInserted` bei einem Produktlauf, wenn die effektive PI-Rolle von
  Luft auf Produkt wechselt;
- Eintritt in den Cooling-ControlTarget oder Rückkehr daraus, weil sich der
  tatsächlich verwendete Regelzielkontext von Fermentation zu Completion-
  Cooling bzw. zurück ändert.

Ein generischer undefinierter `relevantPhaseChange`-Eintrag existiert nicht.
Ein reiner Prozessphasenwechsel ohne neuen Ziel- oder Sensorrollen-Kontext
ruft diese Policy nicht auf; neue Läufe, Neustart/Recovery und echtes
Verlassen der Temperaturregelung setzen den PI-Zustand ohnehin sicher zurück.

Der Aufruf erfolgt erst nach dem bestehenden erfolgreichen Persistenz-/Apply-
Pfad der jeweiligen Änderung:

- bei einem vor `FERMENTING` wirksamen `TargetChanged` nach erfolgreicher
  Lauf-/Prozesspersistenz und `applyProcessTransition()`;
- bei einem `TargetChanged` während `FERMENTING` nach erfolgreichem
  `persistCommand`, auch wenn der Prozesszustand `FERMENTING` bleibt;
- bei #21-Rollenwechsel nach erfolgreichem Sensor-/Prozess-Apply;
- bei `ProductInserted` nach erfolgreichem Prozess-Apply.

Eine kombinierte Ziel- und Rollenänderung wird als ein checked Contextwechsel
übergeben. `Reset` hat Vorrang, sonst werden Carry-Grenzen aller betroffenen
Kontextänderungen gemeinsam auf das Minimum begrenzt. Der Kandidat erhält ein
`transitionSamplePending`-Merkmal; der nächste Evaluation-Sample integriert
positiv erst nach der Übergangsbehandlung.

### 9.3 Richtungswechsel innerhalb der PI-Evaluation

Die Richtung wird aus dem aktuellen Fehler bestimmt. Weicht sie von der
letzten gültigen aktiven/neutralen Richtung ab, wird die validierte
`directionChange`-Aktion in derselben Evaluation vor P und I angewandt. Das
ist keine persistierbare Prozessentscheidung und benötigt keinen vorgelagerten
Prozess-Commit. Der Transition-Sample erzeugt bei gültigem Ergebnis trotzdem
eine normale ControlRequest, aber keine positive `deltaI`.

Unavailable/Invalid, echte Fehler, neue Läufe, Neustart und Recovery löschen
den Integrator und den Richtungsanker fail-closed. Ein alter großer
Integralimpuls wird niemals ungebunden in einen neuen Rollen-, Ziel- oder
Richtungskontext übertragen.

## 10. Zeitvertrag des PI-Kerns

`sampleTimestampMonotonicMillis` ist `uint64_t`; `NaN` und Unendlich sind
keine möglichen Timestampwerte. Struktur-, Reihenfolge- und Rechenfehler
werden checked behandelt:

| Situation | ControlRequest | Integralzustand | Zeitwirkung |
|---|---|---|---|
| erster gültiger Sample | aktueller HEAT/OFF/COOL-Request möglich | exakt `0`; keine Integration | als erster Anker speichern |
| gleicher Timestamp | neue gültige Request möglich, neue Sequence | `dt=0`, unverändert außer Policy-Transition | Timestamp bleibt Anker |
| rückwärts laufender Timestamp | `InvalidInput / TimeInvalid`, kein Request | auf `0` löschen; Richtungsanker verwerfen | alter Timestamp wird nicht ersetzt |
| vorwärts, `delta <= maximumIntegrationStepMillis` | aktueller Request möglich | `delta` nur gemäß Feedback-/Limitregeln integrieren | checked übernehmen |
| vorwärts, zu große Lücke | `InvalidInput / TimeInvalid`, kein Request | auf `0` löschen | aktueller Timestamp wird neuer Nullanker, keine Gutschrift |
| checked Differenz-, Konversions-, Float- oder PI-Overflow | `InvalidInput`, kein Request | auf `0` löschen | kein unsicherer Zeitwert wird übernommen |

Bei einer zu großen Lücke wird der aktuelle Timestamp als checked
Recoveryanker gespeichert, aber bis zu einem späteren normalen Sample keine
Zeit gutgeschrieben. Bei Rückwärtszeit bleibt der alte Anker erhalten; kein
späterer Sample darf die rückwärts liegende Lücke überbrücken.

## 11. Zielqualifikation und effektive Eingabe

### 11.1 Eigentum der Werte

Der `TargetQualificationEvaluator` erhält eine reine, bereits aufgelöste
Momentaufnahme:

```text
TargetQualificationInput
  phase: Preheating | Target
  sampleTimestampMonotonicMillis: uint64_t
  targetCelsius: finite double
  bandCelsius: finite double
  qualificationDurationMillis: uint64_t, > 0
  effectiveGraceMillis: optional<checked uint64_t>
  maximumAcceptedSampleGapMillis: optional<checked uint64_t>
  controlSensorRole: ControlSensorRole
  air: SensorQualitySnapshot
  product: SensorQualitySnapshot
```

`targetCelsius` kommt aus dem effektiven Regelzielvertrag in Abschnitt 5.
Zielband und Qualifikationsdauer kommen aus dem bestehenden unveränderlichen
Lauf-/Qualifikationsvertrag. Für `PREHEATING` ist die ControlSensorRole immer
`Air`; `WAITING_FOR_PRODUCT` besitzt keinen aktiven Evaluator.

`effectiveGraceMillis` ist ein vorgelagert effektiv aufgelöster und validierter
Wert. Die kanonische Programmdokumentation lässt Programmwert oder validierten
Standardwert offen. #22 entscheidet weder seine persistente
Eigentümerschaft noch führt es dafür ein Programmschemafeld ein. Reale Werte
und Commissioning liegen bei #35. Muss die persistente Eigentümerschaft vorher
entschieden werden, ist das ein ausdrückliches Owner-Gate.

`maximumAcceptedSampleGapMillis` ist ebenfalls eine effektiv validierte
Eingabe aus dem Regel-/Sampling-/Commissioningvertrag, nicht aus einem neuen
#22-Programmschemafeld. Produktionswert und Eigentümerschaft bleiben
`TBD_COMMISSIONING` / #35 beziehungsweise der später validierte
Samplingvertrag.

- fehlender Grace-Wert oder fehlender Sample-Gap-Wert -> `Unavailable`;
- ungültiger Grace-Wert, insbesondere checked nicht darstellbar, oder
  ungültiger Sample-Gap-Wert, insbesondere `0`/Overflow -> `Invalid`;
- ein Grace-Wert `0` ist als effektiv validierter Wert zulässig und lässt
  keine fortsetzbare Grace-Episode entstehen.

### 11.2 bandCelsius

`bandCelsius` ist die bestehende persistente Feldbezeichnung und das externe
Feld bleibt `target_qualification_band_c`. Die ausdrückliche R1-
Ownerentscheidung lautet:

```text
bandCelsius = einseitige Toleranz / Halbbreite
InBand genau dann, wenn abs(measuredCelsius - targetCelsius) <= bandCelsius
```

Die Grenze ist inklusiv. Der bestehende Wertebereich wird nicht erweitert:
`program_limits::kMinimumQualificationBandCelsius` bis
`program_limits::kMaximumQualificationBandCelsius`; `0` ist kein gültiger
effektiver Laufwert. Die Freigabe dieser Plan-SHA umfasst diese bisher offene
Feldsemantik. Wäre bei einem erneuten Quellenabgleich eine andere kanonische
Bedeutung belegt, würde die Umsetzung mit `SOURCE_OF_TRUTH_CONFLICT`
angehalten und diese Entscheidung nicht überschrieben.

### 11.3 Sensorrolle und Unavailable/Invalid

In `PREHEATING` wird ausschließlich der Luftsnapshot bewertet, unabhängig
davon, ob `activeRunSensorMode` später `Product` oder `Air` ist. In
`REACHING_TARGET` und `QUALIFYING_TARGET` wird ausschließlich die wirksame
ControlSensorRole aus Abschnitt 5 bewertet. Ein fehlender/stale/failed
Snapshot der zuständigen Rolle ist `Unavailable` und wird nicht durch die
andere Rolle ersetzt.

`Invalid` ist davon getrennt: nicht-finite Messung, strukturell ungültige
Parameter, rückwärts laufender Evaluator-Timestamp, zu große Lücke,
checked Overflow oder unzulässiger Band-/Dauerwert sind `Invalid`. Ein
vorhandener, aber nicht verfügbarer #20-Sensorwert ist `Unavailable`.

## 12. Qualifier-Decide-/Apply-Vertrag und Episoden

### 12.1 Keine zweite Prozesszustandsmaschine

Der Evaluator entscheidet ausschließlich Qualifikation:

```text
evaluateQualification(currentEvaluatorState, input)
  -> QualificationDecision {
       progress
       expectedEvaluatorState
       candidateEvaluatorState
       expectedQualificationContext
     }

applyQualificationDecision(decision, commitContext)
```

`expectedQualificationContext` bindet nur Phase, Ziel-/Rollen-/Laufrevision,
Episodeart und die für Stale-Prüfung erforderlichen Context-Identitäten. Es
entscheidet keinen Prozessübergang und enthält kein `processEffect`,
`expectedProcessState` oder parallele Topologie.

Die verbindliche Reihenfolge ist:

1. Der Evaluator berechnet ohne Live-Mutation einen
   `QualificationDecision`-Kandidaten.
2. Der Orchestrator nimmt nur `decision.progress` und bildet daraus den
   kanonischen `ProcessSignals`-Vertrag.
3. `decideProcessTransition()` des bestehenden Zustandsautomaten ist alleinige
   Quelle des Prozessübergangs und der `qualificationValidSinceMillis`-
   Markeränderung.
4. Bei einer persistierbaren Prozess- oder Markeränderung werden Prozess-
   und Evaluatorkandidat gemeinsam vorbereitet, der bestehende
   Write-before-Apply-Pfad schreibt zuerst, danach wird
   `applyProcessTransition()` ausgeführt.
5. Erst nach erfolgreicher Persistenz und erfolgreichem Process-Apply wird
   der Evaluatorkandidat übernommen.
6. Schlägt Persistenz oder Apply fehl, bleibt der vorherige Evaluatorzustand
   wirksam, der Kandidat wird verworfen und der bestehende fail-closed-
   Persistenzfehlerpfad blockiert die normale weitere Qualifikation.

Bei einem Zyklus ohne persistierbare Prozess-/Markeränderung darf der
Evaluator-Kandidat nach erfolgreicher Evaluation als RAM-only-Zustand
übernommen werden. Es wird keine Evaluatorpersistenz eingeführt. Ein
erfolgreicher Retry berechnet aus dem weiterhin gültigen Live-Zustand neu und
schreibt keine Qualifikationszeit doppelt gut.

### 12.2 Exakte Episodenlebensdauer

Der Evaluatorzustand ist in zwei Episodearten getrennt:

```text
QualificationEpisode = None | Preheating | Target
```

Verbindliche Matrix:

| Ereignis/Phase | Evaluatorzustand nach erfolgreichem Commit/Apply |
|---|---|
| neuer Lauf | leer (`None`) |
| `PREHEATING` beginnt | neue `Preheating`-Episode, Kredit `0` |
| `PREHEATING + Complete -> WAITING_FOR_PRODUCT` | Preheating-Episode vollständig verwerfen |
| `WAITING_FOR_PRODUCT` | kein aktiver QualificationEvaluator |
| `ProductInserted -> REACHING_TARGET` | neue `Target`-Episode, Kredit `0` |
| `REACHING_TARGET -> QUALIFYING_TARGET` | dieselbe Target-Episode fortführen |
| `QUALIFYING_TARGET -> REACHING_TARGET` wegen Verlust | Target-Episode vollständig löschen |
| bestätigtes `TargetChanged` | neue Target-Episode erst nach erfolgreichem Commit |
| bestätigter Rollenwechsel | neue Target-Episode erst nach erfolgreichem Commit |
| Recovery | neue Episode, kein alter Kredit |
| Verlassen der Qualifikationsdomäne nach Abschluss | Evaluatorzustand löschen |

Ein vollständig gutgeschriebenes Preheating wird damit niemals auf die
Qualifikationsdauer nach `ProductInserted` übertragen. Der Zustand
`WAITING_FOR_PRODUCT` ist bewusst ohne aktiven Evaluator, obwohl PI und
Innenlüfter dort weiterhin mit Luftrolle und effektivem Laufziel regeln.

### 12.3 Zeit, Progress und Unterbrechung

- erster verwertbarer InBand-Sample: neuer Zeitanker, Kredit `0`;
- gleicher Timestamp: `dt=0`, keine doppelte Gutschrift;
- aufeinanderfolgende verwertbare InBand-Samples: checked `dt` erhöht den
  Kredit bis zur checked Qualifikationsdauer;
- bei erreichter Dauer: `Complete`;
- jeder `Unavailable`-Sample unterbricht die laufende Episode und verwirft
  Kredit, Grace-Anker und letzten verwertbaren Zeitanker;
- jeder `Invalid`-Sample unterbricht die laufende Episode und verwirft Kredit,
  Grace-Anker und letzten verwertbaren Zeitanker;
- `OutsideBand` außerhalb einer aktiven, nicht abgelaufenen Grace-Episode
  unterbricht die Episode und setzt Kredit auf `0`;
- rückwärts laufender Timestamp und zu große Lücke erzeugen `Invalid` und
  dürfen keinen alten Zeitanker für einen späteren Sample erhalten;
- checked Zeitaddition, Differenz, Millisekundenumrechnung und Kreditgrenze
  dürfen nicht wrapen.

Ein später gültiger InBand-Sample startet nach jedem solchen Unterbruch eine
neue Episode mit Kredit `0`. Er darf keine Zeit über `Unavailable`, `Invalid`,
retrograde Zeit, zu große Lücke oder Outside-Unterbruch hinweg anrechnen.

### 12.4 Grace einschließlich direkter InBand-Rückkehr

Vor jeder Rückkehr aus `Grace` wird bei verwertbarem Timestamp checked

```text
outsideElapsed = currentTimestamp - graceStartedAtMillis
```

ausgewertet, unabhängig davon, ob der aktuelle Sample Outside oder InBand ist.
Die Gleichheit `outsideElapsed == effectiveGraceMillis` gehört zur
abgelaufenen Seite.

- bei `< effectiveGraceMillis` bleibt alte Gutschrift erhalten; ein aktueller
  InBand-Sample liefert `InBand` und `0` neue Millisekunden für die Rückkehr;
- bei `>= effectiveGraceMillis` wird alte Gutschrift zuerst vollständig
  verworfen und die Grace-Episode beendet; aktueller InBand-Sample startet
  eine neue Episode mit `InBand` und Kredit `0`, Outside liefert
  `OutsideBand`;
- bei `effectiveGraceMillis == 0` wird keine fortsetzbare Grace-Episode
  eröffnet.

Direkte InBand-Rückkehrtests decken Grace-Zeit `<`, `==` und `>` ab. Ein
weiterer Outside-Sample am Grace-Ende ist nicht erforderlich, damit der alte
Kredit verworfen wird.

### 12.5 Vollständige Bedeutung aller Progress-Werte

- `Unavailable`: erforderliche Evidenz oder effektiv validierte Eingabe fehlt
  oder ist momentan nicht verfügbar; keine Zeitgutschrift, laufende Episode
  wird unterbrochen.
- `Invalid`: vorhandener Sample/Parameter/Zeit-/Kontextwert ist strukturell
  oder semantisch ungültig; keine Zeitgutschrift, laufende Episode wird
  unterbrochen.
- `OutsideBand`: verwertbarer Sample liegt außerhalb des Bandes und eröffnet
  keine fortsetzbare Grace; Kredit ist `0`.
- `Grace`: verwertbarer Outside-Sample bei vorhandener Episode und noch nicht
  abgelaufener Gnadenzeit; Kredit bleibt unverändert.
- `InBand`: verwertbarer Sample im inklusiven Band, Episode nicht vollständig;
  nur checked Folgezeit erhöht den Kredit.
- `Complete`: checked Kredit erreicht Qualifikationsdauer; positive
  Qualifikationsevidenz, aber kein direkter Prozessübergang aus
  `REACHING_TARGET`.

## 13. ProcessSignals und Zustandsautomat

### 13.1 Phasenwirkung

`criticalFault` wird vor jedem normalen Qualifikations- oder Cooling-
Fortschritt ausgewertet und behält die bestehende Priorität.

| Phase / Progress | `Unavailable` | `Invalid` | `OutsideBand` | `Grace` | `InBand` | `Complete` |
|---|---|---|---|---|---|---|
| `PREHEATING` | bleibt Preheating, Episode/Marker resetten | bleibt Preheating, Episode/Marker resetten | bleibt Preheating, Episode resetten | bleibt Preheating | bleibt Preheating | `WAITING_FOR_PRODUCT` |
| `REACHING_TARGET` | bleibt Reaching, Episode leer | bleibt Reaching, Episode leer | bleibt Reaching, Episode leer | `QUALIFYING_TARGET` | `QUALIFYING_TARGET` | `QUALIFYING_TARGET` |
| `QUALIFYING_TARGET` | `REACHING_TARGET` | `REACHING_TARGET` | `REACHING_TARGET` | bleibt Qualifying | bleibt Qualifying | `FERMENTING`/`MANUAL_HOLDING` |
| `COOLING` | ignoriert | ignoriert | ignoriert | ignoriert | ignoriert | ignoriert |

Aus `REACHING_TARGET` führt jeder positive Qualifikationswert, einschließlich
`Complete`, zuerst nach `QUALIFYING_TARGET`. Nur
`QUALIFYING_TARGET + Complete` führt nach `FERMENTING` oder
`MANUAL_HOLDING`. Ein direkter `REACHING_TARGET -> FERMENTING`/
`MANUAL_HOLDING`-Vorschlag ist unzulässig.

`targetReachStartedAtMillis` und `TargetReachTimeExceeded` funktionieren in
`REACHING_TARGET` und `QUALIFYING_TARGET` weiterhin, auch während `Grace`.
Eine Warnung ergänzt die bestehende Qualifikationsentscheidung und ersetzt
keine Topologie.

### 13.2 Cooling und CompletionMode

`decideCooling()` liest ausschließlich `coolingTargetConditionValid`. Der
Qualifikationsprogress, Grace und Qualifikationsdauer sind dort bedeutungslos.
Alle kühlenden Completion-Modi bleiben explizit:

```text
CoolThenFinish
  COOLING -> COMPLETED

CoolAndHoldForDuration
  COOLING -> COOL_HOLDING

CoolAndHoldUntilManualStop
  COOLING -> COOL_HOLDING
```

`COOL_HOLDING` nutzt für PI weiter dasselbe bestehende Kühlziel. Nur
`CoolAndHoldForDuration` darf danach automatisch nach Ablauf der Haltedauer
abschließen; `CoolAndHoldUntilManualStop` bleibt bis zum bestehenden
manuellen Abschlussereignis. `FinishWithoutCooling` darf strukturell nicht in
`COOLING` eintreten und erhält einen expliziten Negativtest.

`CoolingTargetReached` bleibt für alle drei kühlenden Modi unverändert. Es
gibt keine Gnaden- oder Qualifikationsdauer für COOLING.

### 13.3 Bestehende Ereignisse

Die bestehende `TargetChanged`-Topologie bleibt erhalten:

- in `PREHEATING` wird die Phase neu bewertet und die Preheating-Episode
  zurückgesetzt;
- in `REACHING_TARGET` oder `QUALIFYING_TARGET` führt TargetChanged nach
  erfolgreichem Commit zurück nach `REACHING_TARGET`;
- in `FERMENTING` bleibt der Zustand bei einer erlaubten Zielanpassung
  `FERMENTING`; es gibt keine Requalifikation, aber die PI-
  `targetContextChange`-Policy wird nach erfolgreichem Command-Commit
  angewandt;
- in nicht erlaubten Phasen entscheidet der bestehende Laufanpassungs-
  vertrag fail-closed.

`ProductInserted` führt aus `WAITING_FOR_PRODUCT` nach
`REACHING_TARGET`, erzeugt eine neue Target-Episode und wendet bei einem
Produktlauf die effektive Rollenänderung erst nach Process-Apply an.

Recovery aus einer alten `QUALIFYING_TARGET`-Phase beginnt weiterhin neu über
`REACHING_TARGET`; Marker und Evaluatorkredit werden nicht übernommen.

## 14. `qualificationValidSinceMillis` und Persistenz

`ProcessRuntimeState::qualificationValidSinceMillis` bleibt als bestehendes
optionales uint64-Wire-/Persistenzfeld strukturell erhalten. Es wird weder
umbenannt noch entfernt; es gibt keinen Schema-Bump und kein neues Wirefeld.

Das Feld ist ausschließlich Prozessphasen-/Diagnosemarker. Die alleinige
Wahrheit für `creditedInBandMillis`, Grace und verwertbare Qualifikationszeit
ist der flüchtige Evaluator. Der Marker ist nie Zeitquelle.

Verbindliche Lebenszyklusregeln:

- neuer Lauf, Recovery-Rebase, `WAITING_FOR_PRODUCT`, `ProductInserted`,
  TargetChanged, Rollenwechsel, Qualifikationsverlust, Abschluss und Cooling
  löschen den Marker im jeweiligen Kandidaten;
- bei Beginn einer aktuellen Qualifikationsphase darf die alleinige
  Zustandsmaschine den Marker mit dem aktuellen Sample-Zeitpunkt setzen;
- während `QUALIFYING_TARGET` bleibt er als Marker erhalten, ohne Zeit zu
  berechnen;
- `Unavailable`/`Invalid`/Outside in `PREHEATING` bereiten eine
  `QualificationReset`-Phasendatenänderung vor, wenn ein Marker/eine Episode
  besteht;
- der Marker wird erst nach erfolgreicher Persistenz und erfolgreichem Apply
  des zugehörigen Prozesskandidaten live geändert;
- ein alter Marker erzeugt keinen Kredit und ersetzt keinen aktuellen
  Zielwert, Bandwert, Rollenwert oder Zeitanker.

Für `ControlRequestIdentity`, Feedbackfenster, PI-Integrator und
Evaluatorzustand gibt es keine Persistenz. Nach Recovery beginnt der
Evaluator leer; das bestehende Recovery-Verhalten aus alter
`QUALIFYING_TARGET`-Phase bleibt `REACHING_TARGET`.

## 15. TargetChanged- und Rollenwechsel-Commitgrenzen

### 15.1 Zieländerung vor `FERMENTING`

Eine zulässige RunAdjustment-Zieländerung wird als bestehender
Lauf-/Prozesskandidat berechnet. Erst nach erfolgreicher Persistenz und
erfolgreichem `applyProcessTransition()` gilt sie als angewendet. Erst dann
werden:

- der neue `ActiveRun::effectiveValues()`-Zielwert für den nächsten
  Resolver verwendet;
- die alte TargetQualification-Episode verworfen und die neue Target-Episode
  vorbereitet;
- `applyCommittedControlContextTransition()` mit
  `targetContextChange` aufgerufen;
- die nächste PI-Evaluation mit dem neuen Ziel berechnet.

Scheitert Persistenz oder Apply, bleibt der alte effektive Zielwert,
Evaluatorzustand und Integratorkontext wirksam; der bestehende fail-closed-
Fehlerpfad verhindert eine normale Weiterqualifikation aus einem halb
angewendeten Kandidaten.

### 15.2 Zieländerung während `FERMENTING`

Der bestehende Laufvertrag erlaubt eine wirksame Zieltemperaturänderung
während `FERMENTING`, ohne den unveränderten Quell-Programmschnappschuss zu
ändern. Nach erfolgreichem `persistCommand`:

- bleibt `ProcessState::Fermenting` unverändert;
- es gibt keine Target-Requalifikation und keine neue Qualifier-Episode;
- der nächste PI-Resolver verwendet zwingend den neuen effektiven
  `ActiveRun::effectiveValues()`-Zielwert;
- `applyCommittedControlContextTransition()` wendet die
  `targetContextChange`-Policy an.

Der alte Zielwert darf danach keine PI-Ausgabe mehr auslösen. Ein fehlender,
staler oder fehlgeschlagener Command-Commit darf den neuen Wert dagegen nicht
sichtbar machen.

### 15.3 Sensorrollenwechsel

Ein #21-Rollenwechsel wird erst nach dem bestehenden erfolgreichen
Sensor-/Prozess-Apply an #22 weitergereicht. Bei `ProductInserted` geschieht
dies nach dem erfolgreichen Prozessübergang. Die neue ControlSensorRole,
neue TargetQualification-Episode und Integrator-Policy werden nicht vor
dieser Grenze live gesetzt. `activeRunSensorMode` bleibt das unveränderte
kanonische #21-Feld.

## 16. Commit-/Apply-Schnitte

Die Umsetzung bleibt in kleinen, jeweils kompilierbaren Schnitten:

### Commit 1 – Control-Werttypen und Context-Resolver

- `AbstractControlDirection` wertgleich und genau einmal verschieben;
- `ControlSensorRole`, `ControlRequestIdentity` und `ControlRequest`
  definieren;
- `RunSensorMode` nicht verändern;
- effektive Rollen-/Zielmatrix für alle temperaturgeregelten Phasen
  herstellen;
- `ProcessSignals` mit getrenntem COOLING-Signal einführen;
- keine PI-Mathematik und keine neue Sensor-/Safety-Auswahl vorziehen.

Gezielter Nachweis:

```text
pio test -e native --filter test_sensor_selection
pio test -e native --filter test_process_state_machine
```

Zusätzliche Orakel: Produkt- und Luftlauf in `PREHEATING` und
`WAITING_FOR_PRODUCT`, Produktlauf erst nach `ProductInserted` mit
ControlSensorRole `Product`, Air-Lauf durchgehend `Air`, ohne Änderung von
`activeRunSensorMode`.

### Commit 2 – PI-Gleichung, Request-Identität und Status

- vier Parametersätze mit Einheiten und `Kp > 0`, `Ki > 0`;
- Neutralband, `activeErrorCelsius`, P/I-Gleichung und Integralgrenze;
- checked Zeitvertrag;
- frühe Luftbegrenzung mit `airLimit...`-Namen;
- Status-/Reason-Invarianten;
- gültige OFF-ControlRequest mit Sequence/Timestamp.

Gezielter Nachweis mindestens:

```text
pio test -e native --filter test_temperature_control
```

Der neue Filter darf als produktiver Test-Schnitt erst nach Planfreigabe
angelegt werden; in dieser Planrevision wird er nicht implementiert.

### Commit 3 – Feedback und Integrator-Transition

- `PreviousControlRequestFeedback` mit
  `NoIntegratorConstraint`/`DeferredOrLimited`/`Rejected`;
- Feedbackfenster nur für vorherige aktive Requests;
- Replay-/Stale-/Duplicate-/Sequence-Wrap-Schutz;
- `applyCommittedControlContextTransition()`;
- `Reset`/`BoundedCarry`, checked Carry-Grenze und keine positive Integration
  im Transition-Sample;
- Zieländerung während `FERMENTING` ohne Requalifikation.

Gezielte Orakel: Impulsakkumulator-Aufschub, Mindest-Auszeit, Totzeit,
fehlendes Feedback, alle drei Dispositionen, OFF ohne Feedback,
Rollenwechsel nach `ProductInserted`, Zieländerung vor und während
`FERMENTING`, sowie kein unbounded Integraltransfer.

### Commit 4 – Qualifier und Prozessintegration

- vollständiger Decide-/Apply-Evaluator ohne `processEffect`;
- Sensorrolle `Air` für Preheating und kein Evaluator in Waiting;
- getrennte Preheating-/Target-Episoden;
- vollständige Progress-, Unavailable-/Invalid-, Zeit- und Grace-Semantik;
- Orchestrator bildet ProcessSignals und lässt allein
  `decideProcessTransition()` die Topologie entscheiden.

Cut-Point-Tests:

- `REACHING_TARGET + Complete` schlägt nur `QUALIFYING_TARGET` vor;
- `QUALIFYING_TARGET + Complete` führt erst nach erfolgreichem Apply zu
  `FERMENTING`/`MANUAL_HOLDING`;
- Persistenz vor Qualifier-Commit schlägt fehl;
- Persistenz gelingt, Process-Apply schlägt fehl;
- erfolgreicher Retry ohne Doppelgutschrift;
- TargetChanged mit fehlgeschlagenem Persistenz-/Apply-Pfad;
- InBand -> Unavailable -> InBand;
- InBand -> Invalid -> InBand;
- InBand -> retrograde Zeit -> InBand;
- InBand -> zu große Lücke -> InBand;
- Preheating Complete -> Waiting -> ProductInserted ohne Kreditübertragung;
- Grace-Direktrückkehr `<`, `==`, `>`.

### Commit 5 – Persistenz-/Recovery- und Zustandsregressionen

- nur notwendige Anpassungen am bestehenden
  `qualificationValidSinceMillis`-Markerpfad;
- bestehender Write-before-Apply-Vertrag;
- kein neues Wirefeld und kein Schema-Bump;
- alle CompletionModes und Recoverypfade gezielt absichern.

Gezielter Nachweis mindestens:

```text
pio test -e native --filter test_sensor_selection
pio test -e native --filter test_process_state_machine
pio test -e native --filter test_program_models
pio test -e native --filter test_run_commands
pio test -e native --filter test_run_checkpoint_codec
pio test -e native --filter test_run_persistence_coordinator
```

Filter werden nicht parallel gegen dasselbe native Buildverzeichnis
gestartet. Ein vollständiger lokaler Lauf erfolgt erst nach Review ohne
offene Befunde und ausdrücklicher Owner-Anweisung.

## 17. Vollständiger direkter Testkatalog

Der gezielte Nachweis muss mindestens diese Orakel enthalten:

- effektive PI-Regelsensorrolle je Phase, insbesondere Produktlauf in
  `PREHEATING`/`WAITING_FOR_PRODUCT` und Rollenwechsel bei ProductInserted;
- effektive PI-Zielquelle für Preheating/Waiting, Reaching/Qualifying/
  Fermenting, Cooling, CoolHolding und ManualHolding;
- Programmlauf mit `ActiveRun::effectiveValues()` nach TargetChanged;
- manueller Lauf mit wirksamem `ManualRunPlan`;
- Zieländerung in `PREHEATING`, `REACHING_TARGET` und
  `QUALIFYING_TARGET` mit erfolgreichem beziehungsweise fehlgeschlagenem
  Commit;
- Fermenting-Zieländerung: Zustand bleibt Fermenting, neues Ziel wirkt,
  keine Requalifikation, Policy erst nach Command-Commit;
- alle drei kühlenden CompletionModes positiv;
- `FinishWithoutCooling` als strukturell unzulässiger Cooling-Eintritt;
- `COOLING -> COMPLETED`, `COOLING -> COOL_HOLDING` für Dauer und manuellen
  Halt, sowie unveränderte `CoolingTargetReached`-Semantik;
- PI-Gleichung mit Einheiten, P-only-/I-Wirkung über gültige Testprofile,
  Richtungs-Schwellen und begrenztem Integral;
- erster, gleicher, rückwärts laufender, zu großer und overflow-gefährdeter
  Timestamp mit exakt erwarteter Request-/Integralwirkung;
- gleiche Timestampwerte mit neuen Request-Sequenzen;
- gültige OFF-ControlRequest mit Sequence und Timestamp;
- Unavailable/Invalid ohne gültige ControlRequest;
- fehlendes, altes, doppeltes, fremdes und überlaufendes Feedback;
- `NoIntegratorConstraint`, `DeferredOrLimited`, `Rejected` einschließlich
  Impulsakkumulator, Mindest-Auszeit und Totzeit;
- keine positive Integration bei aktueller PI-Sättigung,
  AirLimitReduced/AirLimitBlocked oder fehlendem Feedback;
- `Reset`/`BoundedCarry` an Target-/Rollen-/Richtungswechsel und keine
  positive Integration im Transition-Sample;
- alle sechs `QualificationProgress`-Werte und ihre Wirkung in
  Preheating/Reaching/Qualifying;
- `Unavailable` und `Invalid` mit unterschiedlicher Ursache und Wirkung;
- InBand-Unterbrechung durch Unavailable, Invalid, retrograde Zeit, große
  Lücke und Outside ohne Zeitübertragung;
- Grace-Direktrückkehr mit `<`, `==`, `>` und Gleichheit als Ablauf;
- Preheating immer über Luft, Waiting ohne Evaluator, Target-Episode erst
  nach ProductInserted;
- vollständiger Preheating-Kredit wird nie Target-Qualifikationskredit;
- Qualifier liefert keine Prozesswirkung außerhalb von Progress;
- `qualificationValidSinceMillis` als Marker, nie zweite Zeitwahrheit;
- `criticalFault` vor Qualifikations- und Cooling-Fortschritt;
- `targetReachStartedAtMillis` und `TargetReachTimeExceeded` in Reaching und
  Qualifying, auch während Grace;
- bestehende TargetChanged-Topologie, Recovery aus alter Qualifying-Phase
  über Reaching und Write-before-Apply-Fehlerpfade;
- keine neuen Wire-/Persistenzfelder und kein Schema-Bump;
- `test_sensor_selection` mit wertgleichem `CrossRolePlausibilityContext`.

## 18. Notwendige Dokumentationsnachführung und offene Eigentümerschaft

Während der späteren Umsetzung werden nur die fachlich notwendigen Stellen
aktualisiert: Prozesssignal-/State-Machine-Vertrag,
`docs/TEMPERATURE_CONTROL.md`, `docs/ACTUATOR_TIMING.md`,
`docs/SENSOR_TUNING_COMMISSIONING.md`, `docs/PROGRAMS.md`, relevante
Lauf-/RunCommand-/Persistenzverträge und direkte Testdokumentation. Der
Komponentenregisterverweis bleibt `docs/THIRD_PARTY_COMPONENTS.md`.

Keine Produktionszahlen, GPIOs, Sensoranschlüsse, Aktormodelle,
Safetygrenzen, Sample-Gap-Werte, Grace-Werte, Integrator-Policywerte oder
Hardwareergebnisse werden erfunden. Grace-Eigentümerschaft,
Sample-Gap-Eigentümerschaft und Produktionswahl der Integrator-Transition-
Policy bleiben ausdrücklich validierte Commissioning-/#35-Eingaben. #22
führt dafür keinen Programmschema-Bump, keine Persistenzmigration und keine
neue Komponentenentscheidung ein.

## 19. Abnahmekriterien und Übergabe

Der Plan ist erst umsetzungsbereit, wenn der Owner den exakten neuen
Plan-Commit freigibt. Die Freigabe muss insbesondere umfassen:

- Trennung von persistiertem `RunSensorMode` und effektiver
  `ControlSensorRole`, einschließlich Luftrolle vor ProductInserted;
- vollständige phase-by-phase `targetCelsius`-Quelle einschließlich
  Completion-Kühlziel und ManualRunPlan;
- alle drei kühlenden CompletionModes und Negativpfad für
  `FinishWithoutCooling`;
- gültige OFF-ControlRequest mit derselben checked Identitätssystematik;
- wirkungssichere Anti-Windup-Disposition ohne physische `appliedQuote`;
- exakte PI-Gleichung, Einheiten, Integralgrenze, Reihenfolge und
  Kp/Ki-Validierung;
- vereinfachte `Reset`/`BoundedCarry`-Policy und Commit-Grenze;
- Unterbrechung von Qualifikationszeit bei Unavailable/Invalid und
  vollständige Preheating-/Target-Episodentrennung;
- Qualifier ohne zweite Prozesszustandsmaschine;
- benannte normale Luftbegrenzung ohne Safety-Hard-Limit-Semantik;
- offene Sample-Gap-, Grace- und Commissioning-Eigentümerschaft;
- `bandCelsius` als inklusive einseitige Toleranz/Halbbreite bei unverändertem
  Feld und Wertebereich;
- Marker-/Recovery-/Timer-/#21-/Cooling-/Persistenzregressionen.

Nach erfolgreicher Planfreigabe werden die beschriebenen Commit-Schnitte
umgesetzt. Bis dahin bleibt der Draft in Plan-only-Zustand; kein
Firmwaretest, kein Produktionscode und keine PR-Statusänderung wird aus
dieser Revision abgeleitet.
