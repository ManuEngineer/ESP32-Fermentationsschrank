# Plan: Issue #22 – Zeitproportionale PI-Regelung und Luftbegrenzung

## 1. Status und Owner-Gate

- Revision: **1**.
- Live-Issue: #22, offen, Status `PLANNED_SPEC_PENDING`.
- Draft-PR: #104, Branch `agent/issue-22-pi-regelung-plan` -> `main`.
- Planpfad: `docs/tasks/issue-22-pi-control-air-limits-plan.md`.
- Plan-Commit: wird nach diesem Plan-Commit mit voller SHA im PR eingetragen.
- Umsetzung: **gesperrt**, bis der Owner den exakten Plan-Commit ausdruecklich
  mit `PLAN APPROVED` beziehungsweise `Approved plan commit: <SHA>` freigibt.
- Der Plan implementiert noch nichts. Insbesondere werden in dieser Revision
  keine Produktionsdateien, produktiven Tests, Hardware-, GPIO-, Toolchain-
  oder CI-Einstellungen geaendert.

```text
CONTEXT_BASELINE_BRANCH: agent/issue-22-pi-regelung-plan
CONTEXT_BASELINE_SHA: 10ff98eca4d6f64cc453571d66d4c3b18729b18e
CONTEXT_HEAD_SHA: e52e2811d86e989cd888ca08618eae0a20b8491e
CONTEXT_PLAN_SHA: NONE
CONTEXT_REFRESH_MODE: FULL
CONTEXT_DELTA: PR-102-Merge auf main; Issue-18-Abschluss; Roadmap-Synchronisierung; Live-Issue-22-Abgleich; Sensor-, Zustandsautomaten-, Plattform- und Temperaturvertraege
SOURCE_OF_TRUTH_CONFLICT: NONE
```

## 2. Live-Ausgangslage und Reihenfolge

PR #102 / Issue #18 ist mit Merge-Commit `10ff98eca4d6f64cc453571d66d4c3b18729b18e`
nach `main` integriert. Issue #18 wurde am 2026-08-12 mit State-Reason
`completed` geschlossen. Die Roadmap nennt deshalb jetzt die fachliche
Reihenfolge:

```text
#22 -> #23 -> #24 -> #19
```

Issue #22 haengt live von #20 (Sensorqualitaet) und #21
(Regelsensorauswahl/Ersatzbetrieb) ab. Beide Grundlagen sind geschlossen und
auf `main` enthalten. #23 bleibt der nachgelagerte Aktorplaner; #24 bleibt der
nachgelagerte Safety-/Fehlerkern; #19 folgt erst danach. Es wird keine
Abhaengigkeit zu #23 oder #24 vorgezogen, die deren eigenen Plan- und
Owner-Gates ersetzt.

Der aktuelle Code besitzt noch keinen PI-Reglerkern und keine produktive
Regelungsschleife. Vorhanden und wiederzuverwenden sind insbesondere:

- `device_platform::SensorQualitySnapshot` mit `Valid`, `Stale`, `Failed`,
  gefiltertem Wert, Messalter und Fehlerursache;
- `RunSensorMode` sowie der in #21 eingefuehrte
  `AbstractControlDirection`-Vertrag;
- `CrossRolePlausibilityContext` und die Sensorselektionsfreigabe aus #21;
- der hardware- und persistenzfreie `process_state_machine`-Vertrag mit
  abstrahierten `ProcessSignals`;
- `ITemperatureSource` und `IBidirectionalActuatorSink` als Plattformports.

`FermentationApplication::update()` ist derzeit absichtlich leer. #22 baut
deshalb den reinen, nativ testbaren Fachkern und seinen Prozesssignalvertrag;
eine neue laufende Hardware- oder Composition-Root-Verdrahtung ist nicht Teil
dieses Plans.

## 3. Ziel und Nicht-Ziele

### 3.1 Ziel

Issue #22 liefert einen deterministischen, hardwarefreien Regelkern fuer
Release 1:

- PI-Regelung mit Proportional- und Integralanteil;
- neutraler Bereich und begrenzte Zeitquote;
- vier getrennte Parametersaetze: Luft/Heizen, Luft/Kuehlen,
  Produkt/Heizen, Produkt/Kuehlen;
- produktgefuehrte Regelung mit frueher oberer/unterer Luftbegrenzung;
- luftgefuehrte Regelung als eigener Modus ohne behauptete Produkttemperatur;
- kontrolliertes Anti-Windup und Reset/Freigabegrenzen;
- qualitaetsgebundene Zielqualifikation mit einer Gnadenzeit, die kurze
  Ausreisser toleriert, aber die Ausserhalb-Zeit nicht als Qualifikationszeit
  gutschreibt;
- abstrakte Diagnose- und Ergebnisgruende fuer spaetere Anzeige, Journal und
  Aktorplanung.

### 3.2 Nicht-Ziele

- keine GPIOs, BTS7960-Pegel, H-Bruecken- oder Luefteransteuerung;
- kein gemeinsames Schaltfenster, keine Mindest-Ein-/Auszeit, kein
  Impulsakkumulator, keine Totzeit und kein Aktor-Watchdog; das ist #23;
- keine vollstaendige Sicherheitsklassifikation, persistente Verriegelung,
  `SAFE_BOOT`-Logik oder Fehlerinjektion; das ist #24;
- keine Sensorbus- oder DS18B20-Implementierung; das ist ein spaeterer
  Adapter-/Hardware-Scope;
- keine vollstaendige Produkt-Luft-Kaskadenregelung;
- kein PID-D-Anteil und kein Autotuning;
- keine numerischen Produktionsparameter. PI-Verstaerkungen, Bandbreiten,
  Luftgrenzen, Gnadenzeit und sonstige thermische Inbetriebnahmewerte bleiben
  `TBD_COMMISSIONING` und duerfen nicht als Laufzeitdefault erfunden werden;
- keine neue Programmschema-Version nur fuer die Gnadenzeit. Die bestehende
  Programmschnittstelle liefert Zielband und Qualifikationsdauer; die
  Gnadenzeit kommt aus einem separaten, explizit validierten
  Commissioning-Profil. Fehlt dieses Profil, ist die Regel-/Qualifikations-
  bewertung `Unavailable` und nicht stillschweigend freigegeben;
- keine Persistenz der fluechtigen PI- oder Qualifikationsintegratoren. Ein
  Neustart erzeugt keinen wiederverwendbaren alten Aktorzustand; die
  bestehende #18-Recovery startet eine nur teilweise Qualifikation erneut.

## 4. Verbindliche Quellen und Entscheidungen

### 4.1 Normative Quellen

- Live-Issue #22: Scope, Akzeptanzkriterien und Definition of Done;
- `docs/TEMPERATURE_CONTROL.md`: PI-Grundsaetze, vier Parametersaetze,
  Produkt-/Luftmodus, Luftbegrenzung, Zielqualifikation und Zukunftsgrenzen;
- `docs/SENSOR_TUNING_COMMISSIONING.md`: Sensorqualitaet, gefilterte Werte,
  vier PI-Parametersaetze, Integratorregeln und `TBD_COMMISSIONING`;
- `docs/PROGRAMS.md`: produkt-/luftgefuehrter Betrieb und die Trennung von
  Sensoraufbereitung/Gnadenzeit und Zustandsautomat;
- `docs/STATE_MACHINE.md`: Zielqualifikation vor dem Timer und abstrakte,
  qualitaetsgepruefte Prozesssignale;
- `docs/ARCHITECTURE.md` und `docs/REQUIREMENTS.md`: Modulgrenzen,
  abstrakte Anforderungen und Release-1-Ausschluesse;
- `docs/CI_AND_QUALITY_GATES.md`: gezielte native Tests, Format-, Architektur-
  und Secret-Pruefungen;
- `docs/AGENT_WORKFLOW.md`, `docs/ENGINEERING_PRINCIPLES.md`,
  `docs/DECISIONS.md` und ADR-013/014: Plan-, Owner-, Architektur- und
  Determinismus-Gates.

### 4.2 Wiederverwendete Entscheidungen

- ADR-004: Produktfuehler ist bei produktgefuehrtem Lauf primaer, Luft ist
  Begrenzungs-/Sicherheitssensor; ohne Produktfuehler ist Luft primaer.
- ADR-005: Zielqualifikation ist von der Fermentationszeit getrennt.
- ADR-012/013: Fachkern bleibt nativ testbar, hardwarefrei und in
  `fermentation_app`; Plattformports werden nicht mit Fachlogik erweitert.
- ADR-014: Der Zustandsautomat berechnet aus abstrakten Signalen eine
  unverbindliche Entscheidung; Persistenz und Aktorfreigabe liegen ausserhalb.
- ADR-015: Programmspezifische Wartezeit bleibt getrennt von der
  Zielqualifikation.
- `docs/audits/THIRD_PARTY_COMPONENTS.md`: Arduino-PID/QuickPID sind nicht
  ausgewaehlt. #22 verwendet eine kleine eigene, deterministische PI-Logik und
  fuehrt keine Bibliotheksabhaengigkeit ein.

### 4.3 Owner-Gates und offene materielle Entscheidungen

Der Plan legt folgende fuer die Umsetzung erforderlichen Grenzen fest:

1. **Commissioning-Gate:** Ein fehlendes, ungueltiges oder unvollstaendiges
   Profil erzeugt keine Heat-/Cool-Anforderung. Testprofile duerfen nur in
   nativen Tests mit expliziten Zahlen verwendet werden; sie sind keine
   Produktdefaults.
2. **Safety-Gate:** Der Regelkern akzeptiert eine dreistufige abstrakte
   Freigabe `Allowed`, `Blocked`, `Unknown`; nur `Allowed` kann Heat/Cool
   erzeugen. #24 liefert spaeter die reale Sicherheitsentscheidung. `Unknown`
   ist fail-closed und keine zusaetzliche Sicherheitsquelle.
3. **Aktor-Gate:** Ein `TemperatureControlEvaluation` ist nur eine fachliche
   Anforderung. #23 entscheidet spaeter Mindestzeiten, Totzeit, Richtung und
   abstrakten Aktorbefehl.
4. **Gnadenzeit-Eigentum:** Die Gnadenzeit wird als Maschinen-/Commissioning-
   wert injiziert, nicht als stiller Programmschemawert. Eine spaetere
   Umstellung auf pro-Programm-Persistenz ist eine materielle Schemaabweichung
   und benoetigt einen neuen Plan-/Owner-Gate.

## 5. Fachlicher Vertrag

### 5.1 Werttyp-Eigentum ohne Parallelvertrag

Der bestehende `AbstractControlDirection`-Enum liegt aktuell in
`sensor_selection.hpp`, obwohl #21 ihn ausdruecklich fuer #22/#23 als
abstrakten Folge-Vertrag vorbereitet. Im ersten Umsetzungsschnitt wird dieser
bestehende Typ ohne semantische Aenderung nach
`lib/fermentation_app/src/temperature_control_types.hpp` verschoben.
`sensor_selection.hpp` bindet den neuen Werttyp ein; es wird kein zweiter
Richtungs-Enum eingefuehrt. Die bestehenden `CrossRolePlausibilityContext`-
Felder und #21-Tests bleiben wertgleich.

Der neue Werttyp-Header enthaelt ausserdem nur die kleinen, fachlich
gemeinsamen Enums:

```text
AbstractControlDirection = Unknown | Heating | Cooling | Idle
ControlSafetyPermission  = Allowed | Blocked | Unknown
TemperatureControlStatus = Demand | Off | Unavailable | InvalidInput
TemperatureControlReason = None | NoCommissioning | SafetyBlocked |
                           SafetyUnknown | SensorUnavailable |
                           SensorPermissionBlocked | InvalidSample |
                           NeutralBand | AirLimitReduced |
                           AirLimitBlocked | Saturated | TimeInvalid
AirLimitState           = NotApplied | Unrestricted | Reduced | Blocked |
                           Unavailable
QualificationProgress    = Unavailable | Invalid | OutsideBand |
                           Grace | InBand | Complete
```

Unbekannte Enumwerte werden bei Validierung abgelehnt. `Unknown`,
`Unavailable` und `Invalid` sind echte beobachtbare Zustandswerte und werden
nicht als `Idle` oder `InBand` umgedeutet.

### 5.2 PI-Eingang und Ergebnis

Der reine Regelkern erhaelt eine vollstaendige Momentaufnahme. Er liest keine
Ports und keine globale Uhr:

```text
TemperatureControlInput
  sampleTimestampMonotonicMillis
  targetCelsius
  sensorMode: Product | Air
  air: SensorQualitySnapshot
  product: SensorQualitySnapshot
  cooling: SensorQualitySnapshot
  sensorPermission: SensorPeltierPermission
  safetyPermission: ControlSafetyPermission

TemperatureControlState
  integralContributionQuote
  lastSampleTimestampMonotonicMillis?
  lastSensorMode?
  lastDirection?

TemperatureControlEvaluation
  status
  reason
  demand: direction + timeQuote
  unboundedQuote
  limitedQuote
  selectedSensorMode
  airLimitState
```

`timeQuote` ist endlich und liegt immer in `[0, 1]`. `Heat` und `Cool` sind
gegenseitig exklusiv, weil das Ergebnis genau eine Richtung oder `Idle`
enthaelt. Bei `Off`, `Unavailable` oder `InvalidInput` ist die Quote `0` und
die Richtung `Idle`; die Diagnose unterscheidet diese Ursachen.

Der Regelwert ist immer `filteredCelsius` einer `Valid`-Snapshot. Ein
vorhandener veralteter Filterwert in `Stale` reicht nicht fuer Heat/Cool. In
beiden Modi muessen Luft- und Kuehlkoerpersensor `Valid` mit gefiltertem Wert
sein; im Produktmodus gilt dies zusaetzlich fuer den Produktsensor. Im
Luftmodus wird die Lufttemperatur als primaerer Regelwert verwendet, ohne eine
Produkttemperatur zu behaupten.

`sensorPermission == Allowed`, `safetyPermission == Allowed` und ein
vollstaendiges Commissioning-Profil sind notwendige, aber keine hinreichenden
Bedingungen fuer eine Nachfrage. Jede andere Kombination bleibt fail-closed.

### 5.3 Parametersaetze und Validierung

Das Profil besitzt vier benannte Parametersaetze, nicht eine untypisierte
Liste:

```text
airHeating
airCooling
productHeating
productCooling
```

Jeder Satz enthaelt mindestens:

- endlichen, nichtnegativen Proportionalfaktor;
- endlichen, nichtnegativen Integrationsfaktor pro Sekunde;
- positive Neutralbandbreite in Grad Celsius;
- `maximumQuote` in `[0, 1]`;
- eine positive maximale Integrationszeit pro Auswertung.

Das Profil enthaelt ausserdem vier geordnete Luftgrenzen
`lowerHard < lowerSoft < upperSoft < upperHard` sowie die positive
Gnadenzeit. Alle numerischen Werte werden vor der ersten Bewertung strukturell
validiert: endliche Fliesskommazahl, Wertebereich, sinnvolle Ordnung und keine
verdeckte `NaN`-/Unendlichkeitsbehandlung. Ein unvollstaendiges Profil ist
`NoCommissioning`, nicht ein Profil mit Nullverstaerkung.

Die Anti-Windup-Regel ist kein frei veraenderbarer Produktionsschalter:

- innerhalb des Neutralbands wird die Quote `0` und der alte Integralanteil
  geloescht;
- bei Mode-/Richtungs-/Sensorwechsel, ungueltigem Sensor, Gate-Sperre oder
  ungueltigem Zeitstempel wird der Integralanteil geloescht;
- wird die Nachfrage durch Quote- oder Luftgrenze in derselben Fehlerrichtung
  gesaettigt, wird nicht weiter integriert;
- zeigt der Fehler gegen die Saettigung, darf der Integralanteil kontrolliert
  abbauen;
- waehrend `AirLimitReduced` wird keine weitere Aufladung gegen die
  Begrenzung zugelassen;
- nach Fehler oder Neustart wird kein alter Integralanteil rekonstruiert.

### 5.4 PI-Mathematik und Zeit

Der Fehler ist `target - measured`:

- positiver Fehler ausserhalb des Neutralbands -> `Heating`;
- negativer Fehler ausserhalb des Neutralbands -> `Cooling`;
- Fehler innerhalb des Neutralbands -> `Idle`.

Der unbeschraenkte Betrag wird aus proportionalem Anteil und kontrolliertem
Integralanteil gebildet. Danach wird zuerst auf `maximumQuote` und danach auf
die richtungsabhaengige Luftbegrenzung angewandt. Die Ausgabe bleibt
zeitproportional; sie ist kein hochfrequenter PWM-Wert und kein Aktorbefehl.

Die Integrationsdauer wird ausschliesslich aus dem monotonen Sample-Zeitstempel
des Eingangs und dem zuletzt akzeptierten Zeitstempel des Reglers gebildet.

- erster Sample, gleicher Zeitstempel oder rueckwaerts laufende Zeit -> keine
  Integration;
- nicht endliche oder strukturell ungueltige Zeit -> `TimeInvalid`, Quote `0`;
- eine unzulaessig grosse Luecke wird nicht als unbewiesene Integrationszeit
  gutgeschrieben, sondern setzt den Integralanteil fail-closed zurueck;
- UTC, Netzwerkzeit und reale Zeitquellen sind unbekannt und nicht Teil des
  Regelkerns.

### 5.5 Luftbegrenzung

Die fruehe Luftbegrenzung gilt nur im Produktmodus und nur fuer die betroffene
Richtung:

```text
HEAT: air <= upperSoft      volle PI-Quote
      upperSoft < air < upperHard  Quote linear bis 0 reduzieren
      air >= upperHard      HEAT blockieren

COOL: air >= lowerSoft      volle PI-Quote
      lowerHard < air < lowerSoft  Quote linear bis 0 reduzieren
      air <= lowerHard       COOL blockieren
```

Die Grenzwerte stammen ausschliesslich aus dem validierten Commissioning-
Profil. An der weichen Grenze beginnt die Reduktion, an der harten Grenze ist
die Richtung gesperrt. Eine Gegenrichtung wird durch eine Luftbegrenzung nicht
gesperrt, sofern ihre eigene Eingangs-, Sensor- und Safety-Pruefung besteht.

Im Luftmodus wird die Luft nicht nochmals als Produktbegrenzung auf sich selbst
angewandt. Sie bleibt primaerer Messwert und Pflichtsensor; die absolute
Sicherheitsentscheidung bleibt ein separates Gate.

### 5.6 Zielqualifikation und Gnadenzeit

Die Sensoraufbereitung und Gnadenzeit bleiben ausserhalb des reinen
Zustandsautomaten. Ein neuer `TargetQualificationEvaluator` arbeitet auf
gefilterten, qualitaetsgeprueften Samples und liefert
`QualificationProgress`.

Der Evaluator fuehrt nur fluechtigen Laufzustand:

```text
creditedInBandMillis
pendingGraceStartedAtMillis?
lastSampleTimestampMonotonicMillis?
lastCondition
```

Semantik:

1. Fehlendes Profil, ungueltiges Ziel/Band, `Stale`/`Failed`, fehlender
   Filterwert, ungueltiger Zeitstempel oder Sensor-/Moduswechsel setzen den
   Fortschritt zurueck und liefern `Unavailable` beziehungsweise `Invalid`.
2. Ein `Valid`-Sample im Band liefert `InBand`; nur Zeit zwischen gueltigen
   In-Band-Samples wird zu `creditedInBandMillis` addiert.
3. Nach bereits gutgeschriebener In-Band-Zeit beginnt ein Ausserhalb-Sample
   eine `Grace`-Phase. Diese Zeit bleibt sichtbar, wird aber **nicht** als
   Qualifikationszeit addiert.
4. Kehrt der Wert vor Ablauf der Gnadenzeit in das Band zurueck, wird die
   gutgeschriebene Zeit fortgesetzt. Uebersteigt die Ausserhalb-Zeit die
   Gnadenzeit, wird der Fortschritt auf null gesetzt und `OutsideBand`
   geliefert.
5. Eine ungueltige Messung ist kein tolerierter Ausreisser: Sie beendet die
   aktuelle Qualifikation sofort fail-closed.
6. `Complete` wird nur bei einem aktuellen gueltigen In-Band-Sample und
   mindestens der vollstaendig gutgeschriebenen Qualifikationsdauer geliefert.
   Ein Ende waehrend `Grace` qualifiziert den Lauf nicht.

Damit koennen kurze Ausreisser toleriert werden, ohne dass wiederholte
Ausserhalb-Intervalle den Lauf durch blosses Verstreichen der Gnadenzeit
qualifizieren. Der Evaluator wird bei neuem Lauf, neuer Zielqualifikation,
Sensorrollenwechsel, Zielaenderung und Recovery-Revalidation zurueckgesetzt.

Der bestehende `process_state_machine`-Vertrag wird dafuer von einem
zusammenfassenden Bool auf einen verarbeiteten `QualificationProgress`-
Zustand erweitert:

- `Preheating` wechselt erst bei `Complete` nach `WaitingForProduct`;
- `ReachingTarget` betritt `QualifyingTarget` bei `InBand` oder `Grace`;
- `QualifyingTarget` bleibt bei `InBand`/`Grace`, geht bei `OutsideBand` oder
  `Invalid` zurueck nach `ReachingTarget` und wechselt bei `Complete` in
  `Fermenting` beziehungsweise `ManualHolding`;
- Rohsensoren, Filter und Gnadenzeit werden nicht in den Zustandsautomaten
  verschoben;
- `qualificationValidSinceMillis` bleibt als Phasen-/Diagnosezeitpunkt
  erhalten, wird aber nicht als zweite Qualifikationswahrheit neben dem
  Evaluator verwendet.

Die bestehende #18-Recovery-Semantik bleibt erhalten: eine nur teilweise
`QualifyingTarget`-Phase wird nach Neustart erneut bewertet, der fluechtige
Evaluator wird nicht aus dem alten Integral- oder Filterzustand rekonstruiert.
Es gibt deshalb in #22 keinen neuen Persistenz- oder Wireformat-Schnitt.

## 6. Modul- und Dateiplan

### 6.1 Neue Produktionsdateien

- `lib/fermentation_app/src/temperature_control_types.hpp`
  - gemeinsamer Richtungswerttyp und Ergebnis-/Gate-Enums;
  - keine Plattform- oder Hardwareabhaengigkeit.
- `lib/fermentation_app/src/temperature_control.hpp`
  - `TemperatureControlParameters`, `TemperatureControlInput`,
    `TemperatureControlState`, `TemperatureControlEvaluation`;
  - reine Validierung, PI-/Luftbegrenzungs- und
    `TargetQualificationEvaluator`-Schnittstellen.
- `lib/fermentation_app/src/temperature_control.cpp`
  - checked PI-Berechnung, Anti-Windup, Zeitstempelverarbeitung,
    Luftgrenzwertprojektion und Qualifikationsfortschritt.

### 6.2 Bestehende Produktionsdateien

- `lib/fermentation_app/src/sensor_selection.hpp`
  - bestehenden `AbstractControlDirection`-Typ aus dem neuen Werttyp-Header
    beziehen; keine semantische Paralleldefinition.
- `lib/fermentation_app/src/process_state_machine.hpp/.cpp`
  - `ProcessSignals` auf `QualificationProgress` umstellen;
  - deterministische Uebergaenge fuer `InBand`, `Grace`, `OutsideBand`,
    `Invalid`, `Complete` festlegen;
  - Recovery-, Critical-Fault-, Snapshot- und Persistenzgrenzen unveraendert
    erhalten.
- `test/test_process_state_machine/test_process_state_machine.cpp`
  - alle bestehenden bool-Signale auf den neuen verarbeiteten Vertrag
    aktualisieren und neue Grenzfaelle ergaenzen.
- direkte Konsumenten von `ProcessSignals`, insbesondere
  `test/test_run_persistence_coordinator/test_run_persistence_coordinator.cpp`
  und `test/test_run_commands/test_run_commands.cpp`, gezielt an den neuen
  Vertrag anpassen. Es werden keine Recovery-/Command-Vertraege erweitert.

### 6.3 Neue Testdatei

- `test/test_temperature_control/test_temperature_control.cpp`
  - reine PI-, Gate-, Luftbegrenzungs-, Zeit- und Qualifikationstests mit
    expliziten Testparametern;
  - kein Platform- oder Hardwaretest und keine Produktionskommissionierung.

### 6.4 Dokumentationswirkung der spaeteren Umsetzung

Nach Umsetzung des freigegebenen Plans werden die normative Temperatur- und
Sensor-Dokumentation sowie die Architektur aktualisiert, mindestens:

- `docs/TEMPERATURE_CONTROL.md`: konkrete digitale Vertragssemantik,
  Anti-Windup, Air-Limiter und Unavailable-Gate;
- `docs/SENSOR_TUNING_COMMISSIONING.md`: vier Profilfelder,
  Zeitstempel-/Integratorregeln und verbleibende Commissioning-Werte;
- `docs/PROGRAMS.md`: verarbeiteter Qualifikationsstatus und die unveraenderte
  Trennung vom Zustandsautomaten;
- `docs/ARCHITECTURE.md`: Besitz des Regelkerns und die Grenze zu #23/#24.

Diese Dokumente erhalten keine erfundenen thermischen Zahlenwerte. Die
Roadmap wird erst bei einem tatsaechlichen Implementierungsstatus aktualisiert.

## 7. Kleine Umsetzungs- und Commit-Schnitte

### Commit 1 – Werttypen, PI-Kern und Commissioning-Gate

- `temperature_control_types.hpp` einfuehren und den bestehenden Richtungs-
  typ ohne Semantikaenderung verschieben;
- Parametersaetze, Validierung, Input-/Output- und State-Vertrag definieren;
- PI-Richtung, Neutralband, Quote, Zeitdelta und Anti-Windup rein berechnen;
- fehlendes Profil, nicht erlaubtes Gate, ungueltige Sensorqualitaet und
  ungueltige Zeit fail-closed behandeln;
- isolierte native Tests fuer Vorzeichen, Neutralband, Quote, Saettigung,
  Integratorabbau, Reset und alle Unavailable-Gruende.

### Commit 2 – Sensorrolle, Luftbegrenzung und Diagnose

- Product-/Air-Modus mit vier Parametersaetzen auswaehlen;
- Luft-Sollgrenzen fuer Heat/Cool wie in Abschnitt 5.5 anwenden;
- `AirLimitReduced`/`AirLimitBlocked`, Rohquote und angewandte Quote
  diagnostisch unterscheiden;
- Kuehlkoerper-, Luft- und primaeren Regelsensor strikt pruefen;
- die #21-Sensorfreigabe konsumieren, aber keine neue Sensorwahl
  implementieren;
- Testmatrix fuer beide Modi, Richtungen, Soft-/Hard-Grenzen und
  Sensorwechsel/Permission erstellen.

### Commit 3 – Qualifikation und Prozesssignalvertrag

- `TargetQualificationEvaluator` mit credited-time-/Grace-Semantik
  ergaenzen;
- `ProcessSignals` und Zustandsautomat auf den verarbeiteten
  `QualificationProgress`-Vertrag umstellen;
- Zielqualifikation vor Fermentationsstart, Ruecksprung bei Ablauf/Invalid
  und `ManualHolding` gleich behandeln;
- bestehende Recovery-/Command-Konsumenten und Tests ohne neue Persistenz-
  oder Wireformat-Felder anpassen;
- Reboot-/Recovery-Tests belegen, dass partielle Qualifikation nicht aus
  altem fluechtigem Zustand rekonstruiert wird.

### Commit 4 – Dokumentation, Guards und Abschlussnachweis

- normative Temperatur-, Sensor-, Programm- und Architekturtexte auf den
  tatsaechlichen Vertrag synchronisieren;
- Architekturgrenzen, Secret-Check, Format- und gezielte Testnachweise
  ausfuehren;
- vollstaendigen aktuellen Diff gegen Issue, Plan, ADRs, Safety-Grenzen,
  Tests und Nicht-Ziele reviewen;
- nach diesem Commit anhalten. #23/#24-Scope wird nicht vorgezogen.

Bei einer materiellen Abweichung an API, Sensor-/Safety-Gate, Persistenz,
Parameter-Eigentum, Modulgrenze oder Teststrategie wird der jeweilige Commit
angehalten, der Plan aktualisiert und eine neue exakte Plan-SHA zur
Ownerfreigabe vorgelegt.

## 8. Discriminating Test-Oracles

Die spaeteren Tests muessen einen sichtbaren Kontrollpfad und die Erholung in
den spaeteren gueltigen Zustand pruefen. Nur einen unmittelbaren Fehlerstatus
zu behaupten reicht nicht.

### PI-Kern

- positiver/negativer Fehler erzeugt ausschliesslich Heat/Cool in der korrekten
  Richtung;
- exakt an beiden Neutralbandgrenzen ist die Ausgabe `Off`;
- hoher Fehler bleibt an `maximumQuote` begrenzt;
- Integralanteil laedt sich bei gesperrtem Gate nicht auf;
- ein gesaettigter Ausgang kann bei umgekehrtem Fehler kontrolliert abbauen;
- neutraler Bereich, Moduswechsel, Richtungswechsel, Sensorwechsel, Neustart
  und ungueltiger Timestamp loeschen den Integralanteil;
- erste Probe integriert nicht rueckwirkend und rueckwaerts laufende Zeit
  erzeugt keine positive Quote;
- `NaN`, Unendlich, fehlendes Profil und fehlender Filterwert erzeugen keine
  normale Demand-Ausgabe.

### Luftgrenzen und Sensoren

- Produkt/Heat wird an `upperSoft` reduziert und an `upperHard` gesperrt;
- Produkt/Cool wird spiegelbildlich an `lowerSoft` reduziert und an
  `lowerHard` gesperrt;
- Reduktion bleibt monoton und wird bei gueltigem Rueckgang wieder aufgehoben;
- Luftmodus verwendet Luft als Regelsensor und wendet keine Produktgrenze auf
  sich selbst an;
- Product `Stale`/`Failed`, Air `Stale`/`Failed`, Cooling `Stale`/`Failed`,
  blockierte Sensorfreigabe und unbekanntes Safety-Gate bleiben `Off`/
  `Unavailable`;
- eine spaetere gueltige Probe mit offenem Gate kann wieder eine Demand-
  Ausgabe erzeugen und beweist die Recovery statt nur die Sperre.

### Zielqualifikation und Zustandsautomat

- erstes In-Band-Sample startet Fortschritt, qualifiziert aber nicht;
- In-Band-Zeit wird gutgeschrieben und fuehrt exakt bei vollstaendiger Dauer
  zu `Complete`;
- ein kurzer Ausreisser liefert `Grace`, pausiert die Gutschrift und setzt
  sich nach Rueckkehr fort;
- ein Ausreisser ueber die Gnadenzeit setzt auf null und geht erst nach neuer
  In-Band-Dauer wieder zu `Complete`;
- `Invalid`/`Stale`/`Failed` loescht sofort; ein spaeter gueltiger Sample baut
  den Fortschritt von vorn auf;
- `Complete` startet erst danach `Fermenting`/`ManualHolding`; ein blosser
  `InBand`- oder `Grace`-Status startet den Timer nicht;
- `TargetChanged`, Sensorrollenwechsel und Recovery setzen den Evaluator
  zurueck;
- beim Recovery bleibt die bestehende #18-Regel erhalten: kein alter PI- oder
  Qualifikationsintegrator wird als Aktorzustand wiederhergestellt.

## 9. Validierung und Nachweise

In der Planungsphase werden keine Firmwaretests oder Builds ausgefuehrt. Nach
Ownerfreigabe sind mindestens diese gezielten Nachweise vorgesehen:

```bash
pio test -e native --filter test_temperature_control
pio test -e native --filter test_process_state_machine
pio test -e native --filter test_run_commands
pio test -e native --filter test_run_persistence_coordinator
clang-format --dry-run --Werror <alle-geaenderten-C++-und-Header-Dateien>
python3 scripts/check_architecture_boundaries.py
python3 scripts/check_secrets.py
git diff --check
```

Die Filter werden wegen des gemeinsam genutzten nativen Build-Verzeichnisses
nicht parallel ausgefuehrt. Bei einer materiellen Aenderung des gemeinsamen
`ProcessSignals`-Vertrags werden nur die direkt betroffenen Konsumenten
gezielt nachgezogen. Ein vollstaendiger nativer Lauf, ESP-IDF-Profile,
clang-tidy-Produktionslauf, Hardware-Smoke und Remote-CI bleiben bis zum
vollstaendigen Review und ausdruecklicher Owner-Anweisung aus.

Ein nicht ausgefuehrter Nachweis wird als `NOT_RUN` beziehungsweise `BLOCKED`
mit konkreter Ursache dokumentiert, niemals als `PASS`.

## 10. Safety-, Recovery-, Security- und Hardwaregrenzen

- Die fachliche Regelung erzeugt keine GPIO-, H-Bruecken- oder direkten
  Luefterbefehle.
- `ControlSafetyPermission::Unknown` und `Blocked` bleiben ohne Heat/Cool.
- `Stale`, `Failed`, fehlende Filterwerte und unbestaetigte Sensorrollen
  werden nicht als aktuelle Temperatur verwendet.
- Kein unbekannter Zustand wird durch einen Nullwert, Sentinel oder erfundenen
  Commissioning-Wert geheilt.
- Der Regler darf die Sicherheitsabschaltung nicht ueberstimmen; #23 darf die
  abstrakte Demand-Anforderung nur nach seinen eigenen Gates in einen
  Aktorbefehl ueberfuehren; #24 kann jederzeit sperren.
- Der letzte elektrische Zustand wird nie persistiert oder wiederhergestellt.
- Der fluechtige Integral- und Qualifikationszustand wird nach Reset/Reboot
  verworfen. Die #18-Recovery bleibt fail-closed und fuehrt eine neue
  Bewertung durch.
- Es werden keine GPIOs, Pegel, Controller, Verdrahtung, Grenzwerte oder
  Hardwaretestergebnisse geraten.
- Die Logik bleibt ohne Netzwerk, NTP, Anzeige, Web und externe Bibliothek
  nutzbar.

## 11. Dokumentations- und Abschluss-Gate

Nach dem Plan-Commit wird der Draft-PR #104 aktualisiert mit:

- Planpfad `docs/tasks/issue-22-pi-control-air-limits-plan.md`;
- voller Plan-Commit-SHA;
- aktuellem Branch-/HEAD-Abgleich;
- unveraendertem Draft-Status;
- dem Hinweis `IMPLEMENTATION_BLOCKED_PENDING_PLAN_APPROVAL`;
- den offenen Commissioning-, #23- und #24-Grenzen.

Danach wird angehalten. Erst eine eindeutige Ownerfreigabe der exakten
Plan-SHA erlaubt die Umsetzung von Commit 1. Eine allgemeine Zustimmung,
Testpass oder Draft-PR-Erstellung ersetzt dieses Gate nicht.
