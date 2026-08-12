# Plan: Issue #22 – Zeitproportionale PI-Regelung und Luftbegrenzung

## 1. Status, Scope und Owner-Gate

- Revision: **2**.
- Live-Issue: #22, offen, Status `PLANNED_SPEC_PENDING`.
- Draft-PR: #104, Branch `agent/issue-22-pi-regelung-plan` -> `main`.
- Planpfad: `docs/tasks/issue-22-pi-control-air-limits-plan.md`.
- Die Revision ist ein vollständiger, eigenständig gültiger Plan für den
  gesamten #22-Scope. Sie setzt keine alten Planstände voraus.
- Planbasis der Revision: `main` @
  `10ff98eca4d6f64cc453571d66d4c3b18729b18e`; aktueller Ausgangs-HEAD vor
  dieser Revision: `bf8e4084af7453669f3a74afe9f7d658e`.
- Der exakte Commit, der diese Revision enthält, wird nach dem Commit mit
  voller SHA in der PR-Beschreibung ausgewiesen.
- Umsetzung: **gesperrt**, bis der Owner den exakten neuen Plan-Commit
  ausdrücklich mit `PLAN APPROVED` beziehungsweise
  `Approved plan commit: <SHA>` freigibt.
- Diese Revision ändert jetzt ausschließlich Plan-/Roadmap-Dokumentation und
  PR-Metadaten. Sie implementiert keine Produktionslogik, produktiven Tests,
  Hardware-, GPIO-, Toolchain- oder CI-Änderung.
- PR #104 bleibt Draft. Es gibt kein `Ready for review`, keinen Merge, kein
  Auto-Merge und kein Branch-Löschen.

```text
CONTEXT_BASELINE_BRANCH: agent/issue-22-pi-regelung-plan
CONTEXT_BASELINE_SHA: 10ff98eca4d6f64cc453571d66d4c3b18729b18e
CONTEXT_HEAD_BEFORE_REVISION: bf8e4084af7453669f3a74afe9f7d658e966069d
CONTEXT_PLAN_SHA: NONE (neuer Revision-2-Commit wird nach Commit ausgewiesen)
CONTEXT_REFRESH_MODE: FULL
CONTEXT_DELTA: vollständiger Ownerreview; Live-Issue-22-Abgleich; PI-/Sensor-/Prozessverträge; #21-Sensorselektion; #18-Persistenz-/Recovery-Vertrag; Revision 2
SOURCE_OF_TRUTH_CONFLICT: Gnadenzeit-Eigentümerschaft bleibt gemäß kanonischem Programmvertrag offen und ist kein #22-Entscheid
```

## 2. Live-Ausgangslage und fachliche Reihenfolge

PR #102 / Issue #18 ist mit Merge-Commit
`10ff98eca4d6f64cc453571d66d4c3b18729b18e` nach `main` integriert. Die
Roadmap führt deshalb die Reihenfolge:

```text
#22 -> #23 -> #24 -> #19
```

Issue #22 baut auf den gemergten Sensorqualitätsverträgen aus #20 und der
Regelsensorauswahl aus #21 auf. #23 bleibt der nachgelagerte Aktorplaner und
prüft Aktorfreigabe, Luft-/Kühlkörpersensor, Mindestzeiten, Totzeit und
Richtungswechsel. #24 bleibt der systemweite Safety-/Fehlerkern und besitzt die
Safety-/Fehlerentscheidung. Kein #22-Schnitt ersetzt deren eigene
Plan-/Owner-Gates.

Der aktuelle Code besitzt noch keinen PI-Reglerkern und keine produktive
Regelungsschleife. Wiederzuverwenden sind insbesondere:

- `device_platform::SensorQualitySnapshot` und dessen `Valid`/`Stale`/
  `Failed`-Semantik;
- `RunSensorMode`, die Sensorrollen und die RAM-/Persistenzverträge aus #21;
- `AbstractControlDirection` und `CrossRolePlausibilityContext` aus #21;
- der hardware- und persistenzfreie `process_state_machine`-Vertrag;
- der bestehende `ProcessRuntimeState` einschließlich
  `qualificationValidSinceMillis`;
- abstrakte Plattformports, ohne sie für #22 um Hardware- oder Safetylogik zu
  erweitern.

Die #21-Sensorselektionsfreigabe wird, wo die spätere Integrationsschicht sie
benötigt, als kanonische Sensorrollen-/Regelberechtigung konsumiert. #22
implementiert keine neue Sensorwahl, keinen Fallback und keine parallele
Freigabe. Insbesondere wird die #21-Freigabe nicht in einen neuen allgemeinen
Safety-Vertrag für den PI-Kern umbenannt.

## 3. Ziel und Nicht-Ziele

### 3.1 Ziel

Issue #22 liefert einen deterministischen, hardwarefreien und nativ testbaren
Fachkern für Release 1:

- zeitproportionale PI-Regelung mit Proportional- und Integralanteil;
- einseitig richtungsabhängiges Neutralband und begrenzte Zeitquote;
- vier getrennte Maschinenparametersätze: Luft/Heizen, Luft/Kühlen,
  Produkt/Heizen, Produkt/Kühlen;
- produktgeführte Regelung mit früher oberer/unterer Luftbegrenzung;
- luftgeführte Regelung als eigener normaler Modus ohne behauptete
  Produkttemperatur;
- kontrolliertes Anti-Windup und deterministische Zeit-/Resetregeln;
- Zielqualifikation mit vollständigem Statusautomaten und effektiv aufgelöster,
  validierter Gnadenzeit;
- abstrakte Diagnose- und Ergebniswerte für die spätere Aktorplanung,
  Anzeige und Protokollierung.

### 3.2 Nicht-Ziele und harte Grenzen

- Keine GPIOs, BTS7960-Pegel, H-Brücken-, Lüfter- oder sonstige
  Aktoransteuerung.
- Kein `ControlSafetyPermission` oder anderer neuer allgemeiner #22-Safety-
  Vertrag vor #24.
- Der Kühlkörpersensor ist **kein Eingang der PI-Mathematik** und keine
  Voraussetzung für die Berechnung der abstrakten Regelanforderung. Seine
  Sicherheits-/Aktorfreigabe bleibt #23/#24.
- Keine Mindest-Ein-/Auszeit, kein gemeinsames Schaltfenster, keine Totzeit,
  keine Richtungswechsel-Hysterese, kein Impulsakkumulator und kein
  Aktor-Watchdog; diese Fachlogik gehört zu #23.
- Keine vollständige Sicherheitsklassifikation, persistente Verriegelung,
  `SAFE_BOOT`-Logik oder systemweite Fehlerentscheidung; diese gehören zu #24.
- Keine Sensorbus-/DS18B20-Implementierung, keine neue Sensorwahl und kein
  stiller Wechsel vom Produkt- in den Luftbetrieb.
- Keine aktive Kaskadenregelung, kein D-Anteil und kein Autotuning.
- Keine numerischen Produktionsparameter. PI-Werte, Luftgrenzen, zulässige
  Zeitlücken, Gnadenzeit und sonstige thermische Inbetriebnahmewerte bleiben
  `TBD_COMMISSIONING` und werden nicht erfunden.
- Keine neue Programmschema-Version und kein neues persistentes Grace-Feld in
  #22. Die kanonische Programmdokumentation lässt offen, ob Zielband,
  Qualifikationsdauer und Gnadenzeit je Programm oder als validierte
  Standardwerte aufgelöst werden. #22 konsumiert nur eine effektiv aufgelöste,
  validierte Gnadenzeit.
- Keine Persistenz des PI-Integrators, der vorherigen Demand-Rückmeldung oder
  des flüchtigen Qualifikationsevaluators.
- Keine Wiederherstellung eines alten elektrischen Aktorzustands und kein
  erfundener Qualifikationsfortschritt nach Neustart.

## 4. Verbindliche Quellen und Entscheidungen

### 4.1 Normative Quellen

- Live-Issue #22: Scope, Akzeptanzkriterien, Tests und Definition of Done;
- `docs/SPECIFICATION_REVIEW.md`: Release-1-Grenzen und TBD-Kategorien;
- `docs/TEMPERATURE_CONTROL.md`: PI-Grundsätze, Sensorrollen,
  Produkt-/Luftmodus, frühe Luftbegrenzung und Zielqualifikation;
- `docs/SENSOR_TUNING_COMMISSIONING.md`: vier PI-Parametersätze,
  Integratorregeln und Commissioning-Grenzen;
- `docs/PROGRAMS.md`: unveränderlicher Programmschnappschuss,
  Betriebsmodi, Vorheizen und Zielqualifikation;
- `docs/STATE_MACHINE.md`: kanonische Prozessphasen und verarbeitete
  Prozesssignale;
- `docs/RUN_PERSISTENCE.md`: `ProcessRuntimeState`, Recovery und bestehende
  Persistenz-/Wireformatgrenzen;
- `docs/ARCHITECTURE.md` und `docs/REQUIREMENTS.md`: Modul- und
  Abhängigkeitsrichtung;
- `docs/CI_AND_QUALITY_GATES.md`: gezielte native Tests, Format-, Architektur-
  und Secret-Nachweise;
- `docs/AGENT_WORKFLOW.md`, `docs/ENGINEERING_PRINCIPLES.md`,
  `docs/DECISIONS.md` und `docs/ADR-013_REUSABLE_DEVICE_PLATFORM.md`;
- `docs/THIRD_PARTY_COMPONENTS.md`: Arduino PID und QuickPID bleiben
  `NOT_SELECTED`; die R1-PI-/Safetylogik bleibt projektspezifisch.

Der dauerhafte Komponentenregisterpfad ist genau
`docs/THIRD_PARTY_COMPONENTS.md`; der historische Audit-Unterpfad wird nicht
als kanonische Quelle verwendet.

### 4.2 Wiederverwendete Entscheidungen

- Produktfühler ist im produktgeführten Lauf der primäre Regelsensor; Luft ist
  die frühe Begrenzungs- und nachgelagerte Sicherheitsbeobachtung.
- Im luftgeführten Lauf ist Luft der primäre Regelsensor; der Modus ist normal
  und behauptet keine gemessene Produkttemperatur.
- Vorheizen qualifiziert den leeren Schrank anhand des Luftfühlers.
- Zielqualifikation und Fermentationszeit bleiben getrennt.
- Die Fachlogik bleibt in `fermentation_app` und hardwarefrei; konkrete
  Plattformadapter bleiben außerhalb.
- Der Zustandsautomat konsumiert bereits verarbeitete Prozesssignale und
  entscheidet nicht selbst aus Rohsensoren oder Filtern.

## 5. Verantwortungsgrenzen

### 5.1 #22: PI und Prozesssignal

#22 berechnet aus dem ausgewählten Regelsensor, der aktuellen Luftbeobachtung
für die Produktbegrenzung und dem validierten PI-Profil ausschließlich:

```text
HEAT + Zeitquote
OFF
COOL + Zeitquote
```

Die Ausgabe ist eine unverbindliche abstrakte Demand-Anforderung. Der Kern
entscheidet nicht, ob ein GPIO, eine H-Brücke, ein Peltier oder ein Lüfter
elektrisch freigegeben wird.

Die frühe Luftbegrenzung ist ein regelungsinterner Ausgangsbegrenzer im
Produktmodus. Sie ist keine vollständige Safetyentscheidung. #23 kann und muss
die für einen Aktorzyklus relevante Luft-/Kühlkörpersensorlage und seine
Aktorbedingungen danach erneut prüfen; #24 kann die Gesamtanforderung jederzeit
sperren.

### 5.2 #23/#24: spätere Rückkopplung ohne vorgezogene Fachlogik

Für die spätere Integration erhält #22 einen schmalen, ursachenunabhängigen
`PreviousDemandFeedback`-Wert. Er enthält nur Daten des unmittelbar vorherigen
Aktorzyklus:

```text
PreviousDemandFeedback
  cycleTimestampMonotonicMillis
  requestedDirection: Heating | Cooling | Idle
  requestedQuote: [0, 1]
  appliedQuote: [0, requestedQuote]
```

`appliedQuote == 0` kann dabei aus jeder nachgelagerten Begrenzung entstehen;
#22 kennt und bewertet die Ursache nicht. Der Wert ist keine
  Safetyentscheidung, kein Aktorbefehl und kein zweiter Demand. Er darf nur die
  Fortschreibung des PI-Integrators beeinflussen.

Die Reihenfolge ist nicht zyklisch:

1. Evaluation *n* berechnet aus dem aktuellen Sample eine abstrakte Demand-
   Anforderung.
2. #23/#24 verarbeiten diese Anforderung außerhalb von #22 und liefern danach
   die tatsächlich akzeptierte Quote als Rückmeldung für Zyklus *n*.
3. Evaluation *n+1* validiert diese Rückmeldung gegen die zuletzt von #22
   ausgegebene Demand-Anforderung und wendet sie **vor** der Integrator-
   fortschreibung für *n+1* an.
4. Die Demand-Anforderung von *n+1* wird davon nicht in eine neue
   Aktor-/Safetyentscheidung umgedeutet. Fehlt die Rückmeldung, wird für diesen
   Schritt konservativ nicht positiv aufintegriert.

Die Rückmeldung muss `cycleTimestampMonotonicMillis` des letzten akzeptierten
Samples tragen und mit der zuletzt ausgegebenen Richtung/Quote übereinstimmen.
Eine Rückmeldung ohne vorherigen #22-Zyklus oder mit widersprüchlicher Form ist
`InvalidInput`; der Integrator wird gelöscht und der aktuelle Demand bleibt
fail-closed. Ein Neustart verwirft die Rückmeldung.

Die einzige Wirkung der Rückmeldung ist:

- bei voller Anwendung darf die normale, sonst zulässige Integration erfolgen;
- bei `appliedQuote < requestedQuote` wird für dieselbe Richtung nicht weiter
  positiv aufgeladen und die Integratorkomponente höchstens um die
  Quotendifferenz kontrolliert abgebaut;
- bei Richtungswechsel, neutralem Fehler, ungültigem Eingang oder Modus-/Rollen-
  wechsel wird der Integrator zurückgesetzt.

Damit erfüllt #22 die Anti-Windup-Anforderung aus
`SENSOR_TUNING_COMMISSIONING.md`, ohne Mindestzeiten, Totzeit, Aktorfreigabe
oder Safetyursache von #23/#24 vorwegzunehmen.

## 6. Eigentum der Werttypen und Prozesssignale

### 6.1 Schmale Werttypgrenzen

Der bestehende #21-Typ `AbstractControlDirection` wird ohne Wert- oder
Semantikänderung aus `sensor_selection.hpp` in einen schmalen gemeinsamen
Werttyp-Header verschoben. Es gibt danach genau eine Definition:

```text
AbstractControlDirection = Unknown | Heating | Cooling | Idle
```

`CrossRolePlausibilityContext`, die bestehenden #21-Sensorselektionsfelder und
deren Werte bleiben unverändert. Der direkte #21-Konsumententest wird deshalb
mitgeführt.

`QualificationProgress` gehört nicht in den breiten Regler-/Diagnoseheader.
Dafür wird ein eigener schmaler Prozesssignaltyp eingeführt:

```text
QualificationProgress = Unavailable | Invalid | OutsideBand |
                        Grace | InBand | Complete
```

Dieser Enum wird ausschließlich in `process_signal_types.hpp` definiert. Der
`TargetQualificationEvaluator` produziert ihn; `ProcessSignals` konsumiert
ihn; eine zweite Enumdefinition ist unzulässig. `temperature_control_types.hpp`
enthält dagegen nur Reglerstatus, Reglergründe, Demand- und Luftbegrenzerwerte
und wird nicht zum impliziten Eigentümer des Prozesssignals.

Die Reglerwerte sind vollständig und genau einmal definiert:

```text
TemperatureControlStatus = Demand | Off | Unavailable | InvalidInput
TemperatureControlReason = None | NeutralBand | Saturated |
                            AirLimitReduced | AirLimitBlocked |
                            NoCommissioning | SensorUnavailable |
                            InvalidConfiguration | InvalidSample | TimeInvalid
AirLimitState = NotApplied | Unrestricted | Reduced | Blocked | Unavailable
```

Der Evaluation-Vertrag enthält mindestens:

```text
TemperatureControlEvaluation
  status
  reason
  demand: { direction, timeQuote }
  unboundedQuote
  maximumLimitedQuote
  limitedQuote
  maximumQuoteApplied: bool
  airLimitState
```

`QualificationProgress` wird nicht in diese Struktur eingebettet. Der
Qualifikationsevaluator liefert ein separates Prozesssignalobjekt, das der
Prozesssignalvertrag an den Zustandsautomaten weiterreicht.

### 6.2 PI-Typen

Der reine PI-Kern liest keine Ports und keine globale Uhr. Seine fachlichen
Eingaben sind:

```text
TemperatureControlInput
  sampleTimestampMonotonicMillis: uint64_t
  targetCelsius: finite double
  sensorMode: Product | Air       // von #21 ausgewählte Laufrolle
  air: SensorQualitySnapshot
  product: SensorQualitySnapshot
  previousDemandFeedback: optional<PreviousDemandFeedback>
```

Es gibt **kein** `cooling`-Feld und kein Safety-Permission-Feld im
PI-Eingang. Der Kühlkörpersensor wird ausschließlich in den nachgelagerten
#23/#24-Freigaben betrachtet.

Der Kern verbraucht den von #21 gelieferten `sensorMode` beziehungsweise die
zugehörige Rollen-/Regelberechtigung, implementiert aber deren Auswahl- und
Fallbackautomat nicht neu. Die #21-`SensorPeltierPermission` wird unverändert
an die spätere Integrations-/Aktorfreigabe weitergegeben; sie wird nicht als
neuer allgemeiner #22-Safetyvertrag in die PI-Mathematik eingebaut.

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

TemperatureControlState
  integralContributionQuote: finite, >= 0
  lastSampleTimestampMonotonicMillis: optional<uint64_t>
  lastDirection: optional<AbstractControlDirection>
  lastSensorMode: optional<RunSensorMode>
  lastDemand: optional<direction + requestedQuote + cycleTimestamp>
```

Alle strukturellen Profilwerte werden vor der Bewertung geprüft. Ein fehlendes
Profil ist `Unavailable / NoCommissioning`; ein vorhandenes, aber strukturell
ungültiges Profil ist `InvalidInput / InvalidConfiguration`. Ein Profil mit
nullbarer PI-Verstärkung muss mindestens eine positive Verstärkung besitzen;
Nullwerte werden nicht als stiller Produktdefault eingesetzt.

## 7. PI-Mathematik, Neutralband, Luftbegrenzung und Status

### 7.1 Richtung und Neutralband

Der signierte Fehler ist:

```text
error = targetCelsius - measuredCelsius
```

Die Richtung wird mit dem Parametersatz derselben Richtung gewählt:

- `error > productHeating.neutralBandWidthCelsius` beziehungsweise der
  entsprechenden Luft-Heating-Breite -> `Heating`;
- `error < -productCooling.neutralBandWidthCelsius` beziehungsweise der
  entsprechenden Luft-Cooling-Breite -> `Cooling`;
- innerhalb oder exakt auf einer dieser einseitigen Schwellen -> `Idle`.

Die `neutralBandWidthCelsius` ist eine **einseitige Fehlerschwelle**, nicht die
Gesamtbreite des Bandes. Bei unterschiedlichen Heiz-/Kühlwerten ist das
Neutralband daher exakt
`[-coolingThreshold, +heatingThreshold]`. An beiden Grenzen gilt `Idle` und
Quote `0`.

Diese Neutralbandentscheidung ist keine Gegenrichtungsbestätigung und ersetzt
nicht die Hysterese, Abschaltung, Totzeit oder Richtungswechselprüfung aus
#23.

### 7.2 PI-Ausgang und Integrator

Für die gewählte Richtung wird aus dem Betrag des Fehlers, der proportionalen
Komponente und der Integratorkomponente zunächst die unbeschränkte Quote
gebildet. Danach gilt genau diese Reihenfolge:

```text
maximumLimitedQuote = min(unboundedQuote, maximumQuote)
limitedQuote = maximumLimitedQuote * airLimitFactor
```

Alle Quoten werden checked berechnet und liegen endlich in `[0, 1]`. Die
Integratorkomponente wird nie persistiert. Sie wird zurückgesetzt bei:

- erstem Sample nach Neustart;
- `Idle`/Neutralband;
- Modus- oder effektivem Regelsensorwechsel;
- Richtungswechsel;
- ungültigem Sample, ungültiger Zeit oder Profilfehler;
- nachgelagerter Teil-/Nichtanwendung gemäß vorheriger Rückmeldung.

Bei eigener Quote-/Luftsättigung wird nicht weiter in die bereits begrenzte
Richtung aufgeladen. Eine Rückkehr aus der Begrenzung ermöglicht nur die exakt
durch den checked Vertrag erlaubte Fortschreibung; #22 führt keine
Aktor-Hysterese oder Mindestzeit ein.

### 7.3 Exakte Zeitsemantik des PI-Kerns

`sampleTimestampMonotonicMillis` ist `uint64_t`; `NaN` und Unendlich sind für
diesen Typ nicht darstellbar und werden nicht als mögliche Timestampwerte
behandelt. Ungültig sind stattdessen Struktur- und Reihenfolgefehler.

Für jeden Aufruf gilt:

| Situation | Demand-Ausgabe | Integrator | Zeitbasis |
|---|---|---|---|
| Erster Sample, kein letzter Timestamp | aktueller P-/begrenzter Demand möglich | bleibt exakt unverändert/0; keine Integration | Timestamp wird als erster akzeptierter Anker gespeichert |
| Gleicher Timestamp | aktueller Demand möglich | unverändert; `dt = 0` | Timestamp bleibt akzeptiert |
| Rückwärts laufender Timestamp | `InvalidInput`, Grund `TimeInvalid`, Richtung `Idle`, Quote `0` | wird auf 0 gelöscht | rückwärts laufender Wert wird nicht als neuer Anker übernommen |
| Vorwärts laufender Timestamp mit `delta <= maximumIntegrationStepMillis` | aktueller Demand möglich | nur nach Anti-Windup-Regeln mit `dt = delta` fortgeschrieben | checked Subtraktion; kein Wraparound |
| Vorwärts laufender Timestamp mit zu großer Lücke | `InvalidInput`, Grund `TimeInvalid`, Richtung `Idle`, Quote `0` | wird auf 0 gelöscht | der aktuelle Timestamp wird als neuer Null-/Recoveryanker gespeichert |
| Checked-Differenz-, Konversions- oder Gleitkommaoverflow | `InvalidInput`, Grund `TimeInvalid`, Richtung `Idle`, Quote `0` | wird auf 0 gelöscht | kein unsicherer Zeitwert wird übernommen |

Bei einem rückwärts laufenden Sample bleibt der letzte gültige Timestamp
erhalten. Nach einer zu großen Lücke wird der aktuelle Timestamp als neuer
Anker akzeptiert, aber keine Zeit gutgeschrieben; ein danach normaler Sample
kann den PI-Pfad wieder sichtbar freigeben. Tests müssen jeweils Output und
Integralzustand prüfen.

### 7.4 Frühe Luftbegrenzung

Die Luftbegrenzung gilt nur im Produktmodus, nur in der betroffenen Richtung
und ausschließlich mit einem `Valid`-Luftsnapshot samt endlichem
`filteredCelsius`:

```text
HEAT: air <= upperSoft
        factor = 1, AirLimitState = Unrestricted
      upperSoft < air < upperHard
        factor = (upperHard - air) / (upperHard - upperSoft), Reduced
      air >= upperHard
        factor = 0, Blocked

COOL: air >= lowerSoft
        factor = 1, Unrestricted
      lowerHard < air < lowerSoft
        factor = (air - lowerHard) / (lowerSoft - lowerHard), Reduced
      air <= lowerHard
        factor = 0, Blocked
```

An der weichen Grenze ist die Quote noch unbeschränkt; exakt an der harten
Grenze ist die betroffene Richtung blockiert. Die Gegenrichtung wird durch
diese frühe Begrenzung nicht automatisch gesperrt. Die Grenzwerte sind
Commissioningwerte und nicht die nachgelagerte Safetygrenze.

Im Luftmodus wird Luft nicht nochmals als Produktgrenze auf sich selbst
angewandt: Luft ist dort der primäre Regelsensor und
`AirLimitState = NotApplied`. Der Produktwert wird in diesem Modus nicht als
notwendiger Regelwert verwendet.

### 7.5 Status-/Reason-Invarianten

`TemperatureControlEvaluation` hat genau einen primären `status` und einen
primären `reason`; Rohquote, maximal begrenzte Quote und Luftbegrenzungsstatus
bleiben separate Diagnosefelder. Damit sind folgende Kombinationen verbindlich:

| Status | zulässige Reasons | Demand/Richtung/Quote |
|---|---|---|
| `Demand` | `None`, `Saturated`, `AirLimitReduced` | Richtung zwingend `Heating` oder `Cooling`; Quote `> 0` und `<= 1` |
| `Off` | `NeutralBand`, `AirLimitBlocked` | Richtung zwingend `Idle`; Quote `0` |
| `Unavailable` | `NoCommissioning`, `SensorUnavailable` | Richtung zwingend `Idle`; Quote `0` |
| `InvalidInput` | `InvalidConfiguration`, `InvalidSample`, `TimeInvalid` | Richtung zwingend `Idle`; Quote `0` |

Zusatzregeln:

- `AirLimitReduced` tritt nur bei `Demand`, `AirLimitState = Reduced` und
  `limitedQuote < maximumLimitedQuote` auf.
- `AirLimitBlocked` tritt nur bei `Off`, `AirLimitState = Blocked` und Quote
  `0` auf.
- `Saturated` tritt nur bei `Demand`, wenn die unbeschränkte Quote die
  `maximumQuote` überschreitet und keine Luftreduktion die primäre Diagnose
  ist. Die checked Diagnosefelder zeigen trotzdem jede angewandte Begrenzung.
- `NoCommissioning` ist kein `Off` mit Nullverstärkung, sondern
  `Unavailable`.
- Kein `Demand` trägt eine `Idle`-Richtung, keine Sperr-/Fehlerdiagnose trägt
  eine positive Quote, und Safety-/Aktorgründe werden in #22 nicht als
  #22-Reasons erfunden.

## 8. Zielqualifikation und Gnadenzeit

### 8.1 Eigentum und effektive Eingabe

Der `TargetQualificationEvaluator` erhält eine reine, bereits aufgelöste
Momentaufnahme. Er besitzt keine Programmpersistenz und schreibt kein Schema:

```text
TargetQualificationInput
  phase: Preheating | Target
  sampleTimestampMonotonicMillis: uint64_t
  targetCelsius: finite double
  targetBandHalfWidthCelsius: finite, >= 0 double
  qualificationDurationMillis: validated uint64_t, > 0
  effectiveGraceMillis: optional<validated uint64_t>
  maximumAcceptedSampleGapMillis: optional<validated uint64_t>
  selectedRunMode: Product | Air
  air: SensorQualitySnapshot
  product: SensorQualitySnapshot
```

Quelle und Eigentum der Werte:

- Zieltemperatur, Zielband und Qualifikationsdauer kommen aus dem
  unveränderlichen Programmschnappschuss des aktuellen Laufs. Die
  Qualifikationsdauer wird nur checked in Millisekunden abgeleitet.
- `effectiveGraceMillis` kommt aus einem vorgelagerten, validierten Resolver.
  Dieser Resolver darf gemäß `docs/PROGRAMS.md` später einen Programmwert oder
  einen validierten Standardwert auflösen. #22 entscheidet weder die
  persistente Eigentümerschaft noch führt es ein Programmschemafeld ein.
- `maximumAcceptedSampleGapMillis` ist eine separat validierte
  Evaluator-/Commissioningeingabe, kein neues Programmschemafeld. Fehlt sie,
  ist die Qualifikationsbewertung `Unavailable`; ein ungültiger Wert ist
  `Invalid`.
- Reale Werte und Commissioning gehören zu #35. Muss die persistente
  Eigentümerschaft der Gnadenzeit vor #35 entschieden werden, ist das ein
  ausdrückliches Owner-Gate. #22 implementiert bis dahin weder Persistenz noch
  Migration dafür.

Fehlende effektive Gnadenzeit führt zu `QualificationProgress::Unavailable`,
nicht zu einem erfundenen Nullwert. Das blockiert die Qualifikation und den
  Übergang aus der Qualifikationsphase, ist aber **keine zusätzliche
  Safetyentscheidung des PI-Kerns**; eine separat gültige PI-Anforderung darf
  den Zielwert weiterhin anfahren.

### 8.2 Sensorrolle je Prozessphase

Die effektive Rolle ist explizit:

| Phase | verwendete Rolle | spätere Laufart |
|---|---|---|
| `PREHEATING` | **immer Luft**, weil der Schrank leer qualifiziert wird | unabhängig davon, ob der Lauf danach produkt- oder luftgeführt ist |
| `REACHING_TARGET` | Produkt bei produktgeführtem Lauf, sonst Luft | gemäß unveränderlichem #21-Modus |
| `QUALIFYING_TARGET` | Produkt bei produktgeführtem Lauf, sonst Luft | gemäß unveränderlichem #21-Modus |

Ein Produkt-/Luftwechsel ändert die effektive Rolle nur über den kanonischen
#21-Vertrag. Der Evaluator wird bei einer tatsächlichen Rollenänderung
zurückgesetzt; es gibt keinen stillen Fallback.

### 8.3 Exakte Bedeutung aller Statuswerte

- `Unavailable`: Die Bewertung ist aktuell nicht möglich, weil eine
  erforderliche, aber nicht fehlerhaft strukturierte Voraussetzung fehlt oder
  nicht verfügbar ist: fehlende effektive Gnadenzeit/
  Gap-Konfiguration, fehlende gewählte Rolle, `Stale`/`Failed`-Sensor oder
  fehlender aktueller Sensorwert. Der Zustand wird gelöscht; es wird keine Zeit
  gutgeschrieben.
- `Invalid`: Die Eingabe verletzt den strukturellen Vertrag: nichtfinite
  Temperatur-/Bandwerte, negative/überlaufende Ableitung, unbekannter Enum,
  `Valid` ohne endlichen Filterwert, rückwärts laufender Timestamp,
  zu große Zeitlücke oder checked Overflow. Der Zustand wird gelöscht; der
  Sample wird nicht als neuer Fortschrittsbeleg übernommen.
- `OutsideBand`: Der aktuelle gültige Wert liegt außerhalb des inklusiven
  Bandes und es gibt keinen noch laufenden zulässigen Grace-Abschnitt. Die
  gutgeschriebene Zeit wird auf 0 gesetzt.
- `Grace`: Der aktuelle gültige Wert liegt außerhalb des Bandes, aber ein
  bereits begonnener Qualifikationsfortschritt befindet sich vor Ablauf der
  effektiven Gnadenzeit. Die Ausserhalbzeit wird sichtbar gehalten, aber nicht
  gutgeschrieben.
- `InBand`: Der aktuelle effektive Sensorwert liegt inklusiv im Zielband; die
  gutgeschriebene In-Band-Zeit ist noch kleiner als die erforderliche Dauer.
  Nur Zeit zwischen aufeinanderfolgenden gültigen `InBand`-Samples wird
  gutgeschrieben.
- `Complete`: Der aktuelle Sample ist gültig und im Band, und die exakt
  checked aufsummierte In-Band-Zeit ist mindestens die validierte
  Qualifikationsdauer. Grace-Zeit und Ausserhalbzeit zählen nie dazu.

Ein erster gültiger In-Band-Sample liefert `InBand`, aber noch keine Zeit. Ein
erster gültiger Outside-Sample liefert `OutsideBand`, nicht `Grace`. Grace
beginnt erst, wenn bereits mindestens ein positiver In-Band-Zeitabschnitt
gutgeschrieben wurde. Bei `effectiveGraceMillis == 0` ist der erste Outside-
Sample nach vorhandenem Fortschritt sofort `OutsideBand`. Grace gilt exakt so
lange, wie `outsideElapsed < effectiveGraceMillis`; bei Gleichheit endet sie
und der Fortschritt wird `OutsideBand`/0.

### 8.4 Evaluatorzustand und Auswertungsreihenfolge

Der flüchtige Zustand ist ausschließlich:

```text
creditedInBandMillis
lastSampleTimestampMonotonicMillis?
lastCondition: Unset | InBand | Grace | OutsideBand
graceStartedAtMillis?
effectiveSensorRole?
```

Für einen Sample gilt:

1. Struktur, Phase, effektive Konfiguration, Sensorrolle, Qualität und
   endlichen Filterwert prüfen.
2. Checked Timestampdifferenz prüfen. Erster Sample hat kein `dt`; gleicher
   Timestamp hat `dt = 0`. Rückwärts, zu große Lücke und Overflow liefern
   `Invalid`, löschen den Fortschritt und schreiben keinen Fortschritt gut.
3. Die effektive Rolle mit der letzten Rolle vergleichen; ein Wechsel löscht
   den Evaluator vor jeder Zeitgutschrift.
4. Bandzugehörigkeit inklusiv bestimmen.
5. Bei `InBand` nur dann `dt` addieren, wenn der vorige Zustand ebenfalls
   `InBand` war. Bei Rückkehr aus `Grace` wird die Ausserhalbzeit nicht addiert;
   der Rückkehrsample startet den nächsten In-Band-Abschnitt.
6. Bei Outside nach positivem Fortschritt Grace starten oder bis zur exakten
   Grenze fortführen. Bei Graceablauf alles auf 0 setzen.
7. Erst nach der Gutschrift und nur auf einem aktuellen In-Band-Sample
   `Complete` liefern.

Ein großer Zeitabstand ist damit nie stillschweigend qualifizierende Zeit.
Nach einem `Invalid` durch eine zu große Lücke wird der aktuelle Timestamp als
neuer leerer Anker gespeichert; der Sample selbst bleibt `Invalid`. Ein späterer
normaler Sample kann dadurch wieder `InBand` und anschließend `Complete`
erreichen. Ein rückwärts laufender Timestamp bleibt dagegen verworfen und
verändert den letzten gültigen Anker nicht.

### 8.5 Wirkung im Zustandsautomaten

`ProcessSignals` erhält genau diesen RAM-Vertrag:

```text
ProcessSignals
  qualificationProgress: QualificationProgress = Unavailable
  criticalFault: bool = false
```

Damit ersetzt `QualificationProgress qualificationProgress` den parallelen
booleschen Qualifikationsvertrag. Das bestehende `criticalFault`-Signal bleibt
ein separater, bereits vorhandener Eingang; #22 definiert dessen
Safetybedeutung nicht neu.

Die vollständige Matrix ist:

| Prozesszustand | `Unavailable` | `Invalid` | `OutsideBand` | `Grace` | `InBand` | `Complete` |
|---|---|---|---|---|---|---|
| `PREHEATING` | bleibt `PREHEATING`, Marker löschen | bleibt `PREHEATING`, Marker löschen | bleibt `PREHEATING`, Fortschritt löschen | bleibt `PREHEATING`, Marker halten/setzen | bleibt `PREHEATING`, Marker setzen/halten | `WAITING_FOR_PRODUCT` |
| `REACHING_TARGET` | bleibt `REACHING_TARGET` | bleibt `REACHING_TARGET` | bleibt `REACHING_TARGET` | `QUALIFYING_TARGET` | `QUALIFYING_TARGET` | direkt `FERMENTING`/`MANUAL_HOLDING` gemäß Snapshot |
| `QUALIFYING_TARGET` | zurück `REACHING_TARGET`, Marker löschen | zurück `REACHING_TARGET`, Marker löschen | zurück `REACHING_TARGET`, Marker löschen | bleibt `QUALIFYING_TARGET` | bleibt `QUALIFYING_TARGET` | `FERMENTING`/`MANUAL_HOLDING` gemäß Snapshot |

`PREHEATING` verwendet dabei ausdrücklich immer Luft, auch bei späterem
Produktbetrieb. `Grace` hält im `QUALIFYING_TARGET` die Phase offen, startet
aber keine Fermentationszeit. Nur `Complete` darf den Fermentationstimer oder
`MANUAL_HOLDING` starten. Kritische Fehler werden weiterhin durch den
separaten Prozess-/Safetyvertrag vorrangig behandelt.

### 8.6 Reset, Recovery und `qualificationValidSinceMillis`

Der Evaluator wird vollständig gelöscht bei:

- neuem Lauf, `TargetChanged` oder Zielband-/Daueränderung;
- tatsächlichem Wechsel der effektiven Sensorrolle;
- `Unavailable`, `Invalid`, abgelaufener Gracezeit oder Neustart/Recovery.

`ProcessRuntimeState::qualificationValidSinceMillis` bleibt als bestehendes
Persistenz-/Recoveryfeld erhalten, hat aber nach dieser Revision nur noch diese
enge Bedeutung: Zeitpunkt, ab dem der aktuelle Prozessphasen-Marker erstmals
eine gültige Qualifikationsbeobachtung (`InBand`/zulässiges `Grace`) gesehen
hat. Es speichert weder `creditedInBandMillis` noch Gracezeit und entscheidet
nie allein über `Complete`.

Das Feld wird:

- in `PREHEATING` und `QUALIFYING_TARGET` beim ersten akzeptierten
  `InBand`-/zulässigen `Grace`-Marker gesetzt und solange die Episode gültig
  bleibt erhalten;
- bei `Unavailable`, `Invalid`, `OutsideBand`, Ziel-/Rollenwechsel und jedem
  Übergang aus der Qualifikationsphase gelöscht;
- bei Recovery vor der erneuten Bewertung gelöscht. Ein alter Marker darf
  niemals eine neue Qualifikation abkürzen.

Die alleinige Wahrheit für gutgeschriebene Zeit und `Complete` ist der
flüchtige `TargetQualificationEvaluator`. Der Zustandsautomat nutzt das Feld
nicht für eine Dauerberechnung; dadurch entsteht keine zweite
Qualifikationswahrheit.

Das bestehende Wireformat bleibt ausdrücklich unverändert: kein neues Feld,
keine Feldumnummerierung, kein Schema-Bump und keine neue Persistenz-
Eigentümerschaft. Der bereits vorhandene optionale
`qualificationValidSinceMillis`-Wert wird weiterhin strukturell kodiert, aber
bei Recovery als nicht vertrauenswürdiger Phasenmarker behandelt und vor der
neuen Evaluatorinstanz gelöscht. Die Codec-/Recovery-Tests müssen genau diesen
Fall nachweisen. Die semantische Änderung und ihre Grenzen werden in
`docs/RUN_PERSISTENCE.md` und der Umsetzungsdokumentation ausdrücklich
vermerkt; sie wird nicht stillschweigend eingeführt.

## 9. Modul-, Datei- und Abhängigkeitsplan

### 9.1 Neue Produktionsdateien

- `lib/fermentation_app/src/control_value_types.hpp`
  - genau ein `AbstractControlDirection`-Werttyp;
  - keine Plattform-, Persistenz- oder Safetyabhängigkeit.
- `lib/fermentation_app/src/process_signal_types.hpp`
  - genau ein `QualificationProgress`-Enum;
  - schmaler Prozesssignalvertrag, keine PI-Diagnosewerte.
- `lib/fermentation_app/src/temperature_control_types.hpp`
  - `TemperatureControlStatus`, `TemperatureControlReason`, `AirLimitState`,
    Demand- und Evaluation-Werte;
  - enthält **nicht** `QualificationProgress`.
- `lib/fermentation_app/src/temperature_control.hpp/.cpp`
  - Profilvalidierung, PI-Mathematik, checked Zeit, Anti-Windup,
    Produkt-/Luftsensorrolle, frühe Luftbegrenzung und
    `TargetQualificationEvaluator`;
  - keine `ControlSafetyPermission`, kein Kühlkörpersensor, keine GPIOs.

### 9.2 Bestehende Produktionsdateien und Konsumenten

- `sensor_selection.hpp/.cpp`: neuen schmalen Richtungswerttyp einbinden,
  #21-Semantik und `CrossRolePlausibilityContext` wertgleich halten.
- `process_state_machine.hpp/.cpp`: `ProcessSignals` auf
  `QualificationProgress` umstellen, die Matrix aus Abschnitt 8.5 umsetzen
  und `qualificationValidSinceMillis` nur als Marker behandeln.
- direkte `ProcessSignals`-Konsumenten, insbesondere
  `run_commands`-/`run_persistence_coordinator`-Pfade, gezielt an den
  RAM-Vertrag anpassen. Es werden keine neuen Persistenz- oder Wirefelder
  eingeführt.
- `docs/RUN_PERSISTENCE.md`: bestehende Feldsemantik, Recovery-Löschung und
  unverändertes Wireformat dokumentieren.

### 9.3 Tests

- `test/test_temperature_control/test_temperature_control.cpp`: reine PI-,
  Rückkopplungs-, Zeit-, Luftbegrenzungs- und Qualifikationstests mit
  expliziten Testparametern; keine Produktiv-Commissioningwerte.
- `test/test_process_state_machine/test_process_state_machine.cpp`: vollständige
  `QualificationProgress`-Matrix, Markersemantik, Timerstart nur bei
  `Complete`, TargetChanged und Recovery.
- direkte #21-Regression in `test/test_sensor_selection`: Wertgleichheit von
  `AbstractControlDirection` und `CrossRolePlausibilityContext`.
- direkte Konsumententests für `ProcessSignals`, insbesondere
  `test_run_commands` und `test_run_persistence_coordinator`.
- Codec-/Recoverytests für unverändertes Wireformat und das Verwerfen alter
  `qualificationValidSinceMillis`-Marker.

## 10. Kleine Umsetzungs- und Commit-Schnitte

### Commit 1 – Schmale Werttypen und PI-Kern

- `AbstractControlDirection` ohne Semantikänderung in den schmalen Werttyp
  verschieben;
- `temperature_control_types`, PI-Parameter-, Input-, State-, Demand- und
  Rückkopplungsvertrag einführen;
- Profilprüfung, Richtungs-/Neutralbandsemantik, PI-Quote und checked Zeit
  umsetzen;
- erste/gleiche/rückwärts/zu große Zeitlücke und Overflow exakt testen;
- `PreviousDemandFeedback` nur als vorherigen Zyklus validieren und ausschließlich
  für Integratorfortschreibung verwenden;
- im Commit und im finalen gezielten Nachweis mindestens ausführen:

  ```bash
  pio test -e native --filter test_sensor_selection
  pio test -e native --filter test_temperature_control
  ```

### Commit 2 – Sensorrollen und frühe Luftbegrenzung

- den von #21 gelieferten Modus konsumieren, ohne Auswahl-/Fallbacklogik zu
  duplizieren;
- Produkt- und Luftsensorrolle exakt unterscheiden;
- obere/untere Soft-/Hard-Limits mit Grenzwertsemantik, Status-/Reason-
  Invarianten und monotoner Recovery umsetzen;
- keinen Kühlkörpersensor und keinen Safetyvertrag in den PI-Kern einführen;
- beide Richtungen, beide Modi und Luftsensorqualität gezielt testen.

### Commit 3 – Qualifikation und Prozesssignalvertrag

- `process_signal_types.hpp` mit der einen `QualificationProgress`-Definition
  einführen;
- `TargetQualificationEvaluator` mit vollständiger Status-, Rollen-, Grace-,
  Zeitlücken-, Overflow-, Reset- und Recoverysemantik umsetzen;
- `PREHEATING` ausdrücklich luftgeführt qualifizieren;
- `ProcessSignals` und Zustandsautomat nach Abschnitt 8.5 umstellen;
- `qualificationValidSinceMillis` nur als Marker führen, Recovery löschen und
  keine zweite Zeitwahrheit einbauen;
- `test_process_state_machine`, `test_run_commands`,
  `test_run_persistence_coordinator` und Codec-/Recoverytests gezielt
  aktualisieren.

### Commit 4 – Dokumentation, Guards und Abschlussnachweis

- `TEMPERATURE_CONTROL.md`, `SENSOR_TUNING_COMMISSIONING.md`,
  `PROGRAMS.md`, `ARCHITECTURE.md` und `RUN_PERSISTENCE.md` auf den echten
  Vertrag synchronisieren;
- #22/#23/#24-Grenzen, effektive Grace-Eingabe und offenes #35-Owner-Gate
  dokumentieren;
- Architektur-, Secret-, Format- und gezielte Testnachweise durchführen;
- vollständigen aktuellen Diff gegen Issue, Plan, ADRs, Fachverträge,
  Persistenz-/Recoverygrenzen und Nicht-Ziele prüfen;
- danach anhalten. Kein #23- oder #24-Scope wird nachträglich ergänzt.

Bei jeder materiellen Abweichung an API, Modulgrenze, Sensorrolle,
Integratorfeedback, Persistenz/Wireformat, Gnadenzeit-Eigentümerschaft,
Safetygrenze oder Teststrategie wird der jeweilige Schnitt angehalten, der Plan
aktualisiert und eine neue exakte Plan-SHA zur Ownerfreigabe vorgelegt.

## 11. Discriminating Test-Oracles

Die Tests müssen den Kontrollpfad und die spätere Erholung beweisen; ein
isolierter Fehlerstatus genügt nicht.

### 11.1 PI, Zeit und Anti-Windup

- positiver/negativer Fehler wählt ausschließlich Heating/Cooling;
- positive Fehlergrenze und negative Fehlergrenze sind jeweils inklusive
  `Idle`/Quote 0;
- getrennte Heiz-/Kühlschwellen werden jeweils aus dem passenden Parametersatz
  genommen; die Neutralbandbreite ist nicht doppeldeutig;
- erster Sample erzeugt gegebenenfalls P-Demand ohne Integration;
- gleicher Timestamp erzeugt gegebenenfalls Demand mit unverändertem Integral;
- rückwärts laufender Timestamp und zu große Lücke liefern exakt
  `InvalidInput/TimeInvalid/Idle/0` und löschen den Integralzustand;
- checked Overflow liefert denselben fail-closed Zeitpfad; `uint64_t` wird
  nicht auf NaN/Unendlich geprüft;
- ein vollständiger vorheriger Zyklus erlaubt Integration, eine reduzierte oder
  nicht angewandte vorherige Quote verhindert weitere positive Aufladung und
  baut kontrolliert ab;
- eine Rückmeldung des falschen Zyklus oder mit widersprüchlicher Quote wird
  `InvalidInput` und erzeugt keine positive Demand-Ausgabe;
- Neustart, Neutralband, Modus-/Rollen- und Richtungswechsel löschen den
  Integrator.

### 11.2 Sensorrollen und Luftlimits

- Luftmodus regelt ausschließlich mit Luft und ignoriert einen optionalen
  Produktwert als Regelvoraussetzung;
- Produktmodus verlangt Produkt als Regelsensor und Luft als frühe
  Begrenzungsbeobachtung, aber keinen Kühlkörpersensor im #22-Eingang;
- Produkt/Heat: exakt `upperSoft` noch unbeschränkt, zwischen den Grenzen
  monoton reduziert, exakt `upperHard` blockiert;
- Produkt/Cool: spiegelbildlich mit `lowerSoft`/`lowerHard`;
- Air-Limiter-Reduktion wird bei gültiger Rückkehr aufgehoben;
- `Stale`/`Failed`/fehlender Filterwert des gewählten Regel- oder benötigten
  Luftsensors liefert `Unavailable`, Idle und Quote 0;
- eine spätere gültige Probe mit gültigem Profil kann wieder Demand erzeugen;
- `CrossRolePlausibilityContext` und #21-Regel-/Rollenwerte bleiben
  wertgleich; der direkte Nachweis lautet:

  ```bash
  pio test -e native --filter test_sensor_selection
  ```

### 11.3 Vollständige Qualifikation

- erster In-Band-Sample -> `InBand`, aber 0 gutgeschriebene Zeit;
- erster Outside-Sample -> `OutsideBand`, nicht `Grace`;
- `InBand`-Zeit wird nur zwischen zwei gültigen In-Band-Samples addiert und
  führt exakt ab vollständiger Dauer zu `Complete`;
- kurze Outside-Phase nach positivem Fortschritt -> `Grace`, ohne
  Zeitgutschrift; Rückkehr -> `InBand` und Fortsetzung;
- Outside-Zeit exakt an der Grace-Grenze -> `OutsideBand`, Fortschritt 0;
- `effectiveGraceMillis == 0` lässt keinen Grace-Sample zu;
- `Unavailable` für fehlende/stale/failed Voraussetzungen und `Invalid` für
  strukturelle/temporale Vertragsverletzungen sind getrennt und löschen beide
  den Fortschritt;
- gleicher Timestamp addiert 0; rückwärts und zu große Lücke liefern
  `Invalid`/0 und keine Zeitgutschrift; ein späterer normaler Sample startet
  wieder sichtbar bei `InBand`;
- Zieländerung, effektiver Sensorwechsel und Recovery löschen den Evaluator;
- `PREHEATING` qualifiziert Luft unabhängig vom späteren Laufmodus;
- in `REACHING_TARGET` führen nur `InBand`/`Grace` in die Qualifikationsphase,
  `Complete` darf direkt das Ziel erreichen;
- in `QUALIFYING_TARGET` bleibt `Grace` aktiv, aber nur `Complete` führt zu
  `FERMENTING`/`MANUAL_HOLDING`; `Unavailable`, `Invalid` und `OutsideBand`
  führen zurück nach `REACHING_TARGET`;
- ein alter persistierter `qualificationValidSinceMillis`-Marker verkürzt
  nach Recovery keine neue Qualifikation.

## 12. Gezielte Validierung und Nachweise

Während der Planungsphase werden keine Firmwaretests, Builds oder Hardware-
Nachweise ausgeführt. Nach Ownerfreigabe sind mindestens diese sequenziellen,
gezielten Nachweise vorgesehen:

```bash
pio test -e native --filter test_sensor_selection
pio test -e native --filter test_temperature_control
pio test -e native --filter test_process_state_machine
pio test -e native --filter test_run_commands
pio test -e native --filter test_run_persistence_coordinator
pio test -e native --filter test_run_checkpoint_codec
clang-format --dry-run --Werror <alle geänderten C++- und Header-Dateien>
python3 scripts/check_architecture_boundaries.py
python3 scripts/check_secrets.py
git diff --check
```

Die nativen Filter werden wegen des gemeinsam genutzten `.pio/build/native`
nicht parallel ausgeführt. Nach einer gemeinsamen `ProcessSignals`-Änderung
werden die direkt betroffenen Konsumenten gezielt nachgezogen. Ein
vollständiger nativer Lauf, ESP-IDF-Profile, vollständiger clang-tidy-Lauf,
Hardware-Smoke und Remote-CI bleiben bis zum vollständigen Review und
ausdrücklicher Owner-Anweisung aus.

Ein nicht ausgeführter Nachweis ist `NOT_RUN` beziehungsweise `BLOCKED` mit
konkreter Ursache, niemals `PASS`.

## 13. Safety-, Recovery-, Security- und Hardwaregrenzen

- #22 erzeugt keine GPIO-, H-Brücken- oder direkten Lüfterbefehle.
- Es gibt im #22-PI-Eingang keinen allgemeinen Safety-Permission-Typ und keinen
  Kühlkörpersensor. #23 prüft nachgelagert Aktorfreigabe, Luft-/Kühlkörper,
  Mindestzeiten, Totzeit und Richtungswechsel; #24 entscheidet systemweit
  Safety und Fehler.
- #22 überschreibt keine Safetyabschaltung. Eine Demand-Ausgabe bedeutet nicht,
  dass der Aktor laufen darf.
- `Stale`, `Failed`, fehlende Filterwerte und unbestätigte Sensorrollen werden
  nicht als aktuelle Temperatur verwendet.
- Unbekannte, fehlende oder ungültige Werte werden nicht durch Nullwerte,
  Sentinels oder erfundene Commissioningwerte geheilt.
- Der PI-Integrator, die vorherige Aktor-Rückmeldung und der Evaluatorzustand
  werden nicht persistiert.
- Bestehende Recovery bleibt fail-closed; ein alter
  `qualificationValidSinceMillis`-Marker ist keine Qualifikationsfreigabe.
- Das unveränderte Wireformat wird durch Codec-/Recoverytests belegt; ein
  tatsächlicher Schema- oder Eigentümerbedarf für Grace ist ein Owner-Gate.
- Die Logik bleibt ohne Netzwerk, NTP, Anzeige, Web und externe Regelbibliothek
  nutzbar.
- Keine GPIOs, Pegel, Controller, Verdrahtung, thermischen Grenzwerte oder
  Hardwaretestergebnisse werden geraten.

## 14. Dokumentations-, Roadmap- und Abschluss-Gate

Nach dem Plan-Commit wird PR #104 aktualisiert mit:

- `Revision 2` und vollständigem Planpfad;
- der exakten neuen Plan-Commit-SHA;
- aktuellem Branch-/HEAD-Abgleich;
- unverändertem Draft-Status;
- `IMPLEMENTATION_BLOCKED_PENDING_PLAN_APPROVAL`;
- der offenen Gnadenzeit-Eigentümerschaft bei #35;
- den klaren #22/#23/#24-Grenzen und dem Hinweis, dass nichts implementiert
  wurde.

Die Roadmap bleibt ausschließlich Statusübersicht und wird nur insoweit
aktualisiert, dass sie auf Revision 2 und das neue Owner-Gate zeigt. Nach dem
PR-Update wird angehalten. Erst eine eindeutige Ownerfreigabe der exakten neuen
Plan-SHA erlaubt Commit 1; eine allgemeine Zustimmung, ein Testpass oder der
Draft-Status ersetzt dieses Gate nicht.
