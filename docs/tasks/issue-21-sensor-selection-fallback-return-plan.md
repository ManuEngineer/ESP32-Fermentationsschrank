# Plan: Issue #21 – Regelsensorauswahl, Ersatzbetrieb und Rueckkehrlogik

## 1. Metadaten und Status

```text
Issue: #21 [E3.2] Regelsensorauswahl, Ersatzbetrieb und Rueckkehrlogik
Epic: #5 (E3)
Branch: plan/issue-21-sensor-selection-fallback-return
Baseline main: ff2e66a8c340d61c8c4517f90fd3fba5a8fc3db2
Vorherige, NICHT freigegebene Plan-Commits:
  c505fce6cbd12a02f9c195cdba7bf0dc37d3c8bd (Revision 1)
  aaeefbdf6997bbbbd9359985ed00f9b75ab6283e (Revision 2)
  2e3a041131996d73cb0ce342f256f06f79f694bd (Revision 3)
  6a85c331cf17673d03ec2a231100e5f3af7b916b (Revision 4)
PLAN_ONLY: YES
IMPLEMENTATION_STARTED: NO
PLAN_STATUS: PLAN_DRAFT_REVIEW_REQUIRED
IMPLEMENTATION_STATUS: IMPLEMENTATION_BLOCKED_PENDING_PLAN_APPROVAL
```

Dies ist Revision 5. Sie behebt die im PR-#99-Review vom 2026-08-06 zu
Revision 4 benannten Luecken bei neu konkretisierten Integrationsstellen:
fehlender Eigentuemer und fehlende kanonische Anwendungsfunktion fuer den
fluechtigen Sensorselektions-Laufzeitzustand, fehlender persistierter
Kommandovertrag fuer manuelle Aktionen, widerspruechliches
Policy-Zeitverhalten (Wartezeit fuer eine Policy ohne Wartezeit),
unbegrenzte Wiederholschleife bei der automatischen Rueckkehrvalidierung,
unvollstaendig angeschlossene Start-Notice, missverstaendliche
Effektbezeichnung, fehlende thermische Evidenz-Invarianten sowie eine an
der #17/#18-Grenze zu optimistische Testbehauptung. Produktionscode,
produktive Tests, Toolchain, Buildkonfiguration, Hardwarekonfiguration und
Abhaengigkeiten werden weiterhin in dieser Planungsphase nicht geaendert.

## 2. Live-Issue- und Abhaengigkeitsabgleich

| Quelle | Live-Stand am 2026-08-06 | Bedeutung fuer diesen Plan |
|---|---|---|
| Issue #21 | OPEN, Body-Status `PLANNED_SPEC_PENDING`, keine Kommentare | eigener Plan-first-Draft-PR, keine Implementierungsfreigabe |
| Issue #14 | CLOSED | kanonische Prozesszustaende und Uebergangstopologie stehen zur Verfuegung |
| Issue #17 | Implementiert (`RunPersistenceCoordinator`) | `LoadedActiveRun`/`RecoveryPending` bestehen bereits; Recoveryaktivierung bleibt #18 |
| Issue #18 | OPEN | zustaendig fuer `LoadedActiveRun -> Ready`; #21 liefert nur Daten und eine reine Entscheidungsfunktion |
| Issue #20 | CLOSED | `SensorQualitySnapshot`/`SensorQualityPipeline` bleiben Qualitaetsquelle |
| Epic #5 | OPEN | Issue bleibt Teil des E3-Sensor-/Regel-/Safety-Kerns |
| PR #99 | OPEN, Draft, dieser Plan ist die fuenfte Revision | Reviewbefunde vom 2026-08-06 zu Revision 4 sind Grundlage dieser Ueberarbeitung |

Issue #21 hat weiterhin keine Kommentare mit zusaetzlichen Anforderungen.
Issue #17 und #18 werden nicht veraendert.

## 3. Verbindliche Quellen und Lesematrix

Bei Widerspruechen gilt die in `docs/SPECIFICATION_REVIEW.md` festgelegte
Reihenfolge (unveraendert seit Revision 1).

Zusaetzlich zu den in Revision 1-4 gelesenen Quellen wurden fuer diese
Ueberarbeitung konkret nachvollzogen:

- `lib/fermentation_app/src/run_commands.hpp`, `struct CommandDecision`
  (Zeile 300-315): traegt bereits `before`/`after: RunCommandState`
  (vollstaendiger Zustand, nicht nur ein schmaler Ausschnitt),
  `std::optional<StartSummary> startSummary` als bestehendes Muster fuer
  einen kommandobezogenen Zusatzwert - ein analoges
  `std::optional<StartSensorSelectionNotice>`-Feld fuegt sich ohne
  Strukturbruch ein;
- `lib/fermentation_app/src/run_commands.cpp`, `applyRunCommand`
  (Zeile 848-874): **generisch fuer jeden `CommandKind`** - kein Switch pro
  Kommandotyp, sondern eine gemeinsame Staleness-Pruefung gegen einzeln
  aufgezaehlte Felder von `current` gegen `decision.before`
  (`commandSequence`, `processState`, `runRevision`, `messageRevision`,
  `faultRevision`, `criticalSafetyEventPending`, `activeRunId`,
  `activeRunSensorMode`), gefolgt von `current = decision.after;` als
  Ganzes. **Weder `sensorSelectionRuntime` noch `sensorSelection` sind in
  dieser Liste** - eine neue manuelle Sensorwechsel-Aktion wuerde ohne
  Erweiterung dieser Liste eine zwischenzeitlich (durch eine parallele
  automatische Bewertung) veraenderte Auswahlphase stillschweigend
  ueberschreiben koennen (siehe 6.14.3);
- `isRunComfortCommand` (Zeile 54-70) und die (nicht explizit gefundene,
  aber aus dem Muster ableitbare) Erwartung eines vollstaendigen
  `CommandKind`-Switches: beide sind **erschoepfend** ueber `CommandKind`
  geschrieben - ein neuer `CommandKind`-Wert erzwingt beim Compilieren eine
  bewusste Entscheidung an jeder Stelle, an der ueber `CommandKind`
  geschaltet wird (derselbe Absicherungsmechanismus wie der
  `static_assert` bei `kRequiredFields` in `program_model.cpp`);
- `CommandStatus::NoChange` (bereits bestehender Enumwert, referenziert an
  mehreren Stellen in `run_commands.cpp`, z. B. Zeile 300, 532, 772, 800):
  eine Entscheidung mit `status = NoChange` ist nicht `.proposed()` und
  wird von der aufrufenden Schicht direkt beantwortet, ohne
  `persistCommand` ueberhaupt aufzurufen - das bestehende, bereits an
  mehreren Stellen genutzte Muster fuer eine bestaetigte, aber wirkungslose
  Anfrage passt exakt auf einen wirkungslosen `RecheckProduct`.

Roadmaps und historische Plaene werden nicht als implementierter Ist-Stand
behandelt.

## 4. Ziel und Nicht-Ziele

### Ziel

Zusaetzlich zu den in Revision 1-4 etablierten Zielen macht diese Revision
explizit:

- ein genau benannter, RAM-only Eigentuemer (`RunCommandState::
  sensorSelectionRuntime`) fuer die flüchtige Auswahlphase, Peltier-
  Permission-Momentaufnahme, Fallback-Wartezeit-Start und den
  Rueckkehrvalidierungs-/Re-Arm-Zustand, angewendet durch **genau eine**
  kanonische Funktion (`applySensorSelectionDecision`, 6.4.11) fuer sowohl
  automatische als auch manuelle Aenderungen;
- manuelle Sensorentscheidungen (mit Luft fortsetzen, zum Produkt
  zurueckkehren, Produkt erneut pruefen) sind echte, persistierte,
  idempotente Kommandos ueber den bestehenden `persistCommand`-Pfad, nicht
  nur eine Randnotiz zu `CommandEnvelope`;
- ein policy-bewusstes, in sich widerspruchsfreies Zeitverhalten: eine
  Policy ohne konfigurierbare Wartezeit (`WaitForUser`) wartet nicht auf
  eine nicht existierende Wartezeit; `StopToSafeState` erzeugt in jedem
  Fall genau eine atomare Entscheidung, nie zwei;
- eine verschleisssichere, terminierende Re-Arm-Regel fuer die automatische
  Rueckkehrvalidierung, die eine Bewertungsschleife pro Messzyklus
  strukturell ausschliesst;
- eine vollstaendig angeschlossene Start-Notice ueber
  `CommandDecision`/`RunPersistenceResult`;
- eine Effektbezeichnung, die nicht als direkte Aktorfreigabe missverstanden
  werden kann;
- ausgewertete, nicht nur deklarierte thermische Evidenz-Invarianten;
- eine Testmatrix, die zwischen dem in #21 tatsaechlich testbaren Teil des
  Zwei-Slot-/Recovery-Vertrags und dem erst in #18 erfuellbaren Teil
  unterscheidet, ohne private Coordinatorzustaende zu umgehen.

### Nicht-Ziele

Unveraendert seit Revision 4, ergaenzt um:

- keine Erweiterung von `applyRunCommand` ueber die in 6.14.3 konkret
  benannten zwei neuen Felder hinaus - keine generelle Ueberarbeitung der
  bestehenden Staleness-Pruefung;
- keine zweite, #21-eigene Altersarithmetik auf
  `ThermalCompatibilityEvidence::evaluatedAtMonotonicMillis` - die
  inhaltliche Alters-/`Stale`-Bewertung bleibt Sache des Produzenten
  (#22/#23), #21 prueft nur auf einen unmoeglichen Zukunftszeitstempel
  (6.10).

## 5. Befund des aktuellen Codes

Unveraendert seit Revision 4, ergaenzt um die in Abschnitt 3 verifizierten
Fundstellen: `applyRunCommand`s unvollstaendige Staleness-Feldliste,
`CommandDecision`s bereits vorhandenes `optional`-Zusatzfeld-Muster
(`startSummary`), `CommandStatus::NoChange` als bereits etabliertes Muster
fuer wirkungslose, aber bestaetigte Anfragen.

## 6. Fachvertraege

### 6.1-6.3

Unveraendert seit Revision 3/4: Eingaben und Rollen (6.1), Rueckkehrstrategie
im Programmmodell (6.2), Ausgabewert und Zustandsseparation (6.3).

### 6.4 Auswahlzustandsautomat

#### 6.4.1 Zustaende

Zustandsliste und Grundtabelle unveraendert seit Revision 4 (`NormalProduct`,
`NormalAir`, `ProductFailureDetected`, `UserDecisionRequired`,
`AirFallbackActive`, `ReturnValidationPending`, `SafeLocked`,
`RestartRevalidationPending`), mit den in 6.4.9-6.4.12 dieser Revision
przisierten Uebergangs-, Zeit- und Persistenzregeln.

#### 6.4.2-6.4.8

Unveraendert seit Revision 4 (Uebergangstabelle als Grundlage, Start der
Fallback-Wartezeit, erneut gueltiger Produktwert, einmalige Verarbeitung
manueller Aktionen, Idempotenz, Abbruch/Wiederaufnahme der
Rueckkehrvalidierung, Revisions-/Kapazitaetsgrenzen), **praezisiert** durch
die folgenden neuen Unterabschnitte. Wo diese Revision einer frueheren
Aussage widerspricht, gilt die neue Aussage.

#### 6.4.9 Grundregel: was wird atomar persistiert, mit Vorrangregel

Unveraendert die vier Bedingungen aus Revision 4:

```text
(a) peltierPermission aendert sich (Allowed<->Blocked)
(b) activeMode aendert sich
(c) der Zustand SafeLocked wird betreten
(d) eine laufende automatische Rueckkehrvalidierung wird abgebrochen
```

**Neu: Vorrangregel fuer mehrdeutige Zyklen (Review-Befund 3/4).** Erfuellt
ein einzelner Bewertungszyklus mehr als eine dieser Bedingungen
gleichzeitig, wird **genau eine** Entscheidung emittiert, nach folgender
Rangfolge (hoechste zuerst):

```text
1. (c) SafeStateEntry
2. (b) ModeChanged
3. (d) ReturnValidationAborted
4. (a) ProductFailureBlock / RecoveryRevalidation
```

`SafeStateEntry` gewinnt strukturell immer. Damit sind beide im Review
benannten Ueberschneidungsfaelle eindeutig geloest, ohne sie einzeln
kodieren zu muessen:

- `StopToSafeState` + Produktausfall: der Zyklus erfuellt sowohl (a)
  (Permission wuerde ohne Policy-Sonderfall Allowed->Blocked) als auch (c);
  nach der Vorrangregel wird **ausschliesslich** `SafeStateEntry` emittiert.
  Es gibt fuer diese Policy nie einen separaten `ProductFailureBlock`-
  Vorlauf (6.4.10) - `(b)` und `(c)` sind aufgrund von 6.4.1s
  "Modus unveraendert beim Eintritt in `SafeLocked`" ohnehin nie
  gleichzeitig erfuellbar, brauchen also keine Rangfolge untereinander.
- gleichzeitiger Air-/Cooling-Ausfall waehrend `ReturnValidationPending`:
  erfuellt sowohl (d) (Rueckkehrvalidierung waere abzubrechen) als auch (c)
  (Air/Cooling ungueltig); nach der Vorrangregel wird ausschliesslich
  `SafeStateEntry` emittiert, der Uebergang erfolgt direkt
  `ReturnValidationPending -> SafeLocked` (bereits so in der
  Uebergangstabelle vorgesehen), nicht ueber einen Zwischenschritt.

Klassifikationstabelle (ersetzt die Tabelle aus Revision 4):

| Bedingung(en) im Zyklus | Emittierte Cause | Persistenzweg (6.14) |
|---|---|---|
| nur (c), oder (c) zusammen mit (a)/(d) | `SafeStateEntry` | `persistSensorSelection` (automatisch) oder `persistCommand` (falls durch eine Benutzeraktion ausgeloest) |
| (b), automatisch (Timeout/automatische Rueckkehr) | `FallbackToAir` / `AutomaticValidatedReturn` | `persistSensorSelection` |
| (b), manuell (Benutzeraktion) | `ManualUserFallback` / `ManualUserReturn` | `persistCommand` (6.14.3) |
| nur (d) | `ReturnValidationAborted` | `persistSensorSelection` |
| nur (a), Allowed -> Blocked, Policy != StopToSafeState | `ProductFailureBlock` | `persistSensorSelection` |
| nur (a), Blocked -> Allowed | `RecoveryRevalidation` | `persistSensorSelection` (automatisch) oder `persistCommand` (falls durch `RecheckProduct` ausgeloest) |
| keine der obigen | `None` | keine Persistenz, hoechstens `MessageCode`-Diagnose |

#### 6.4.10 Policy-Zeitverhalten (Korrektur, Review-Befund 3)

**Widerspruch in Revision 4 behoben:** `WaitForUser` besitzt nach der
Cross-Field-Regel in 6.13 (Regel 5) niemals ein `fallbackDelaySeconds` -
"nach Ablauf der Wartezeit" war fuer diese Policy nie erfuellbar. Korrigierte,
policy-spezifische Ablaeufe:

```text
FallbackToAirAfterTimeout:
  ProductFailureDetected wird betreten
    -> ProductFailureBlock sofort persistiert (Peltier Blocked)
    -> Zustand bleibt ProductFailureDetected fuer die Dauer der
       konfigurierten Wartezeit
    -> automatische Luftfortsetzung (FallbackToAir) nach Ablauf
    -> ODER bestaetigte manuelle Luftfortsetzung (ManualUserFallback)
       jederzeit bereits waehrend der Wartezeit, sofern SensorPreference
       Luft erlaubt und Air/Cooling gueltig sind (vereinheitlichte
       ContinueWithAir-Regel, siehe unten)
  UserDecisionRequired wird fuer diese Policy nie betreten.

WaitForUser:
  ProductFailureDetected wird betreten
    -> ProductFailureBlock sofort persistiert (Peltier Blocked)
    -> im selben Zyklus fluechtiger Uebergang nach UserDecisionRequired
       (keine eigene Revision, siehe 6.4.9-Tabelle "keine der obigen")
    -> bestaetigte Luftfortsetzung (ManualUserFallback) sofort moeglich,
       sofern SensorPreference Luft erlaubt (bei ProductRequired
       strukturell ausgeschlossen, 6.4.13)
    -> Produkt wird waehrenddessen wieder gueltig -> RecoveryRevalidation,
       automatisch, KEINE explizite Benutzerbestaetigung noetig
       (Korrektur: Revision 4 verlangte hier faelschlich zusaetzlich eine
       Benutzeraktion, obwohl derselbe automatische Ruecklauf bereits fuer
       FallbackToAirAfterTimeout in 6.4.4 gilt - beide Policies werden
       hier vereinheitlicht, keine sachliche Begruendung fuer eine
       Ausnahme bei WaitForUser)

StopToSafeState:
  Produktausfall erkannt -> direkt SafeLocked, genau eine atomare
    SafeStateEntry-Entscheidung (6.4.9-Vorrangregel), kein
    ProductFailureBlock-Zwischenschritt.
```

**Vereinheitlichte `ContinueWithAir`-Regel:** die manuelle Aktion "mit Luft
fortsetzen" ist gueltig, sobald `selectionPhase ∈ {ProductFailureDetected,
UserDecisionRequired}` UND `SensorPreference` Luftbetrieb erlaubt (nicht
`ProductRequired`) UND Air/Cooling `VALID` sind - unabhaengig davon, ob eine
Wartezeit existiert oder bereits abgelaufen ist. Das ersetzt zwei
policy-spezifische Regeln (Revision 4 kannte diese Aktion nur ab
`UserDecisionRequired`) durch eine einzige, fuer beide Policies gueltige
Regel.

Fuer produktgefuehrte manuelle Laeufe (6.8) gilt derselbe `WaitForUser`-
Ablauf ohne erfundene Wartezeit - unveraendert in der Substanz, jetzt am
korrigierten Automaten nachvollziehbar.

#### 6.4.11 Kanonischer Laufzeitzustand und Anwendungsfunktion (Review-Befund 1)

**Neuer Abschnitt.** Eigentuemer des fluechtigen Auswahlzustands:

```cpp
enum class SensorSelectionPhase : std::uint8_t {
    NormalProduct, NormalAir, ProductFailureDetected, UserDecisionRequired,
    AirFallbackActive, ReturnValidationPending, SafeLocked,
    RestartRevalidationPending,
};

enum class SensorPeltierPermission : std::uint8_t { Allowed, Blocked };

struct ReturnValidationRuntimeState {
    std::optional<std::uint64_t> enteredAtMonotonicMillis;
    std::optional<std::uint32_t> lastObservedProfileRevision;
    // begrenzte monotone Retry-Sperre, siehe 6.4.12
    std::optional<std::uint64_t> retryNotBeforeMonotonicMillis;
};

struct SensorSelectionRuntimeState {
    SensorSelectionPhase phase{SensorSelectionPhase::RestartRevalidationPending};
    SensorPeltierPermission permission{SensorPeltierPermission::Blocked};
    std::optional<std::uint64_t> fallbackWaitStartedAtMonotonicMillis;
    ReturnValidationRuntimeState returnValidation;
};
```

`RunCommandState` erhaelt `SensorSelectionRuntimeState sensorSelectionRuntime`.
Der Default (`RestartRevalidationPending`/`Blocked`) gilt nur, solange kein
Start- oder Restore-Pfad ihn explizit setzt - `decideProgramStart`/
`decideManualStart` setzen ihn wie `activeRunSensorMode` explizit auf den
tatsaechlichen Startwert (`NormalProduct`/`NormalAir`, `Allowed`).
Bootlokale Timer (`fallbackWaitStartedAtMonotonicMillis`,
`returnValidation.*`) sind **nicht** Teil von `PersistedSensorSelectionState`
(6.12.1) und ausdruecklich ausserhalb des Wireformats - unveraendert seit
Revision 3/4.

**Genau eine kanonische Anwendungsfunktion**, verwendet von *beiden* Wegen:

```cpp
struct SensorSelectionApplyResult {
    RunCommandState state;   // Kandidat mit aktualisiertem Runtime- und
                              // ggf. persistiertem Zustand
    bool persistWorthy;      // true genau dann, wenn 6.4.9 (a)-(d) erfuellt
    // Event-/Notice-Nutzlast gemaess 6.11
};

[[nodiscard]] SensorSelectionApplyResult applySensorSelectionDecision(
    const RunCommandState& current, const SensorSelectionDecision& decision);
```

```text
Weg 1 - automatische periodische Bewertung:
  applySensorSelectionDecision(current, decision)
  -> persistWorthy == true:  Kandidat via persistSensorSelection committen
     (6.14.2), danach current = committed state
  -> persistWorthy == false: current = result.state direkt (RAM-only,
     keine Laufrevision, kein Flashwrite, keine Aktorfreigabeaenderung)

Weg 2 - manuelle Aktion (decideApplySensorSelectionAction, 6.14.3):
  ruft applySensorSelectionDecision(current, decision) auf DERSELBEN
  Kandidatenkopie auf, um CommandDecision::after zu bilden
  -> Kommando durchlaeuft persistCommand wie jedes andere Kommando; bei
     persistWorthy == false liefert die decide-Funktion
     CommandStatus::NoChange und persistCommand wird von der aufrufenden
     Schicht gar nicht erst aufgerufen (bestehendes Muster, siehe 3)
```

Direkte produktive Nebenpfade, die `sensorSelectionRuntime` ausserhalb
dieser Funktion veraendern, sind unzulaessig - dieselbe Ein-Implementierung
verhindert, dass Automat-Regeln an zwei Stellen auseinanderlaufen (loest
den in Blocking 2 des Reviews benannten Zwei-Schreiber-Zustand).

#### 6.4.12 Re-Arm-Regel fuer die automatische Rueckkehrvalidierung (Review-Befund 4)

**Neuer Abschnitt.** Verschleisssicherer, terminierender Ablauf fuer
`ReturnValidationPending`:

```text
Eintritt (Produkt wird waehrend AirFallbackActive valide, ReturnStrategy =
AutomaticValidatedReturnToProduct):
  -> returnValidation.enteredAtMonotonicMillis = jetzt
  -> returnValidation.lastObservedProfileRevision =
     thermalCompatibility.profileRevision
  -> Stabilitaets-/Evidenzfortschritt beginnt bei Null (kein
     Teilfortschritt aus einem fruehreren Versuch)

Waehrend ReturnValidationPending, pro Bewertungszyklus:
  Evidenz Unavailable, unvollstaendig oder (noch) nicht stabil, oder Stale:
    -> Zustand bleibt ReturnValidationPending
    -> KEINE Revision, KEINE Abbruchmeldung (6.4.9: keine der (a)-(d)
       Bedingungen ist erfuellt - reine RAM-Bewertung)
  Evidenz Incompatible ODER Produkt wird waehrenddessen erneut ungueltig:
    -> genau EIN ReturnValidationAborted (6.4.9), Uebergang nach
       AirFallbackActive (oder SafeLocked bei gleichzeitigem Air-/
       Cooling-Ausfall, Vorrangregel 6.4.9)
    -> returnValidation.retryNotBeforeMonotonicMillis = jetzt +
       kReturnValidationRetryIntervalMillis (firmwarefeste, begrenzte
       Sperre; realer Wert TBD_COMMISSIONING, Obergrenze firmwarefest)
  Evidenz Compatible UND alle uebrigen 6.7/6.10-Kriterien erfuellt:
    -> genau EIN AutomaticValidatedReturn (6.4.9), Uebergang nach
       NormalProduct

Re-Arm - ein NEUER Eintritt in ReturnValidationPending nach einem Abbruch
ist erst zulaessig, wenn MINDESTENS eine Bedingung gilt:
  (i)   thermalCompatibility.profileRevision hat sich gegenueber
        returnValidation.lastObservedProfileRevision geaendert;
  (ii)  returnValidation.retryNotBeforeMonotonicMillis ist erreicht oder
        nicht gesetzt;
  (iii) eine explizite RecheckProduct-Benutzeraktion (6.14.3) hat die
        Sperre im selben Kommandodurchlauf aufgehoben (siehe unten);
  (iv)  das Produkt war zwischenzeitlich erneut ungueltig (Ruecksprung
        AirFallbackActive -> ProductFailureDetected oder direkt erneuter
        Ausfall) und ist danach erneut valide geworden - dieser Fall
        gilt als neue, unabhaengige Gelegenheit und hebt die Sperre sofort
        auf, ohne auf (i)/(ii) zu warten.

Ohne erfuellte Re-Arm-Bedingung bleibt der Zustand nach einem Abbruch in
AirFallbackActive, auch wenn Produkt weiterhin valide ist - kein
sofortiger erneuter Eintritt.
```

`kReturnValidationRetryIntervalMillis` ist ein neuer firmwarefester
Grenzwert in `sensor_selection_limits.hpp`, analog zu bestehenden
firmwarefesten Obergrenzen (z. B. `kMaximumFallbackDelaySeconds`); der
reale operative Wert bleibt `TBD_COMMISSIONING`, die Obergrenze selbst ist
kein TBD.

**`RecheckProduct` und die Sperre - Reihenfolge (Review-Blocking 3):** die
`RecheckProduct`-Kommandoverarbeitung (6.14.3) hebt
`retryNotBeforeMonotonicMillis` als Teil ihres eigenen Kandidaten-Apply-
Schritts auf, **bevor** dieselbe Kandidatenkopie erneut bewertet wird - die
durch `RecheckProduct` ausgeloeste Bewertung sieht also bereits die
aufgehobene Sperre innerhalb derselben Entscheidung, nicht erst im
naechsten Zyklus. `RecheckProduct` ist gueltig aus `{ProductFailureDetected,
UserDecisionRequired, AirFallbackActive}` - die ersten beiden fuer eine
sofortige Neubewertung des Produktfuehlers, der dritte spezifisch fuer die
Re-Arm-Aufhebung bei `AirFallbackActive`. Aus jedem anderen Zustand liefert
`RecheckProduct` `CommandStatus::NotAllowedInState`.

#### 6.4.13 `ProductRequired` schliesst jeden Luftfallback strukturell aus

Unveraendert seit Revision 4 (zentrale Validierung + Aktionsbeschraenkung),
jetzt formuliert gegen die vereinheitlichte `ContinueWithAir`-Regel aus
6.4.10: die Aktion bleibt fuer `SensorPreference::ProductRequired`
unabhaengig von Zustand und Policy abgelehnt (`CommandStatus::InvalidInput`).

#### 6.4.14 Vollstaendige Policy-/Aktionsmatrix (Review-Befund 3)

| Policy | Luft erlaubt? | Zustand | Aktion/Ereignis | Air/Cooling | Ergebnis |
|---|---|---|---|---|---|
| FallbackToAirAfterTimeout | ja | `ProductFailureDetected` (vor Timeout) | keine | ja | bleibt `ProductFailureDetected`, Blocked |
| FallbackToAirAfterTimeout | ja | `ProductFailureDetected` (vor Timeout) | `ContinueWithAir` | ja | -> `AirFallbackActive`, `ManualUserFallback` |
| FallbackToAirAfterTimeout | ja | `ProductFailureDetected` (vor Timeout) | `ContinueWithAir` | nein | `CommandStatus::InvalidInput` |
| FallbackToAirAfterTimeout | ja | `ProductFailureDetected` | Timeout erreicht | ja | -> `AirFallbackActive`, `FallbackToAir` |
| FallbackToAirAfterTimeout | ja | `ProductFailureDetected` | Timeout erreicht | nein | -> `SafeLocked`, `SafeStateEntry` (Vorrang) |
| FallbackToAirAfterTimeout | nein (`ProductRequired`) | - | - | - | durch 6.13 Regel 1 bereits validierungsseitig ausgeschlossen |
| WaitForUser | ja | `ProductFailureDetected` | sofort (kein Timeout) | - | -> `UserDecisionRequired`, fluechtig |
| WaitForUser | ja | `UserDecisionRequired` | `ContinueWithAir` | ja | -> `AirFallbackActive`, `ManualUserFallback` |
| WaitForUser | ja | `UserDecisionRequired` | `ContinueWithAir` | nein | `CommandStatus::InvalidInput` |
| WaitForUser | nein (`ProductRequired`) | `UserDecisionRequired` | `ContinueWithAir` | - | `CommandStatus::InvalidInput` (6.4.13) |
| WaitForUser | egal | `ProductFailureDetected`/`UserDecisionRequired` | Produkt wieder gueltig | - | -> `NormalProduct`, `RecoveryRevalidation`, keine Benutzeraktion noetig |
| WaitForUser | egal | `ProductFailureDetected`/`UserDecisionRequired` | `RecheckProduct`, weiterhin ungueltig | - | `CommandStatus::NoChange` |
| StopToSafeState | egal | `NormalProduct` | Produktausfall erkannt | egal | -> `SafeLocked`, genau eine `SafeStateEntry` (kein `ProductFailureBlock`-Vorlauf) |

### 6.5-6.9

Unveraendert seit Revision 3/4: vollstaendige Startmatrix (6.5), Produktfehler
und Ersatzbetrieb als Ueberblick (6.6, jetzt praezisiert durch 6.4.9-6.4.14),
Rueckkehr zum Produktfuehler (6.7), produktgefuehrte manuelle Laeufe (6.8, mit
6.4.10s korrigiertem `WaitForUser`-Ablauf), Phasen/Kuehlen/Halten (6.9).

### 6.10 Rollenuebergreifende Plausibilitaetspruefung

Struktureller Vertrag (`CrossRolePlausibilityContext`,
`ThermalCompatibilityEvidence`) unveraendert seit Revision 4. **Neu:
Invarianten fuer `ThermalCompatibilityEvidence`** (Review-Befund 7):

```text
status ∈ {Compatible, Incompatible, Stale}  => profileRevision != 0
evaluatedAtMonotonicMillis <= evaluationMonotonicMillis
  (des umschliessenden CrossRolePlausibilityContext)
  -> sonst blockReason = InvalidContext, fail-closed wie Unavailable
Unavailable erzeugt nie eine Freigabe
ungueltige Enum-/Revisions-/Zeitkombinationen (z. B. Compatible mit
  profileRevision == 0) blockieren fail-closed mit InvalidContext
```

**Stale-Eigentuemerschaft (kanonisch entschieden, kein Owner-Gate):** der
Produzent (spaeter #22/#23) setzt `status = Stale` verbindlich selbst. #21
fuehrt **keine eigene Altersarithmetik** auf
`evaluatedAtMonotonicMillis` durch - das Feld ist reine Diagnose- und
Zukunftsvertrauens-Referenz (siehe die `<=`-Pruefung oben), nicht
Grundlage einer zweiten, #21-eigenen `Stale`-Bewertung. Ein
implementierender Agent darf hierauf **keine** eigene Altersschwelle
aufbauen - das wuerde denselben Doppel-Eigentuemer-Fehler reproduzieren,
den diese Regel gerade ausschliesst.

Die bereits in Revision 4 dokumentierte Abhaengigkeitsaussage (P21-M4, kein
Owner-Gate) bleibt unveraendert.

### 6.11 Ereignis-, Meldungs- und Revisionsvertrag

**Erweiterung gegenueber Revision 4:** neue Ursachen fuer manuelle
Aktionen, korrigierte Kardinalitaetsaussage, vollstaendiger Anschluss der
Start-Notice, umbenannte Effekte.

```cpp
enum class SensorSelectionDecisionCause : std::uint8_t {
    None,
    StartSelection,            // ueber persistCommand (Start)
    ProductFailureBlock,       // ueber persistSensorSelection
    FallbackToAir,             // ueber persistSensorSelection (automatisch)
    ManualUserFallback,        // ueber persistCommand (ContinueWithAir)
    AutomaticValidatedReturn,  // ueber persistSensorSelection (automatisch)
    ManualUserReturn,          // ueber persistCommand (ReturnToProduct)
    RecoveryRevalidation,      // ueber persistSensorSelection ODER
                                // persistCommand (RecheckProduct-Erfolg)
    SafeStateEntry,            // ueber persistSensorSelection ODER
                                // persistCommand
    ReturnValidationAborted,   // ueber persistSensorSelection
};
```

**Korrigierte Zaehlung (Review-Befund 9):** acht Nicht-Start-Ursachen
insgesamt (`ProductFailureBlock`, `FallbackToAir`, `ManualUserFallback`,
`AutomaticValidatedReturn`, `ManualUserReturn`, `RecoveryRevalidation`,
`SafeStateEntry`, `ReturnValidationAborted`) - **nicht** sechs, wie in
Revision 4 an mehreren Stellen faelschlich behauptet. Davon werden **sechs**
ausschliesslich oder primaer ueber `persistSensorSelection` transportiert
(automatische Ursachen) und **zwei** (`ManualUserFallback`,
`ManualUserReturn`) ausschliesslich ueber `persistCommand` (manuelle
Moduswechsel, 6.14.3); `RecoveryRevalidation` und `SafeStateEntry` koennen
je nach Ausloeser (automatischer Zyklus vs. `RecheckProduct`-Kommando bzw.
eine waehrend einer manuellen Aktion erkannte Sicherheitslage) ueber beide
Pfade entstehen - beide Pfade rufen dafuer dieselbe
`applySensorSelectionDecision`-Funktion auf (6.4.11), sodass keine zweite
Regelimplementierung entsteht.

```cpp
struct SensorSelectionEvent {
    RunSensorMode beforeMode;
    RunSensorMode afterMode;
    SensorSelectionDecisionCause cause;
    std::uint32_t runRevision;
    std::uint64_t monotonicMillis;
    std::optional<std::int64_t> utcUnixSeconds;
};

struct SensorSelectionNotice {
    SensorSelectionDecisionCause cause;
    std::uint64_t monotonicMillis;
    std::uint32_t runRevision;
    RunSensorMode activeMode;
    SensorSelectionBlockReason blockReason;
};

struct StartSensorSelectionNotice {
    RunSensorMode requestedMode;
    RunSensorMode effectiveMode;
    std::uint32_t runRevision;
};

enum class SensorSelectionUserAction : std::uint8_t {
    ContinueWithAir,
    ReturnToProduct,
    RecheckProduct,
};

struct SensorSelectionCommandRequest {
    CommandEnvelope envelope;
    SensorSelectionUserAction action;
    bool safetyAllowsChange{false};  // analog zu RunAdjustmentCommandRequest
};
```

**Korrigierte Kardinalitaetsaussage (Review-Befund 5):** *nicht* "genau eine
der drei Nutzlasten pro `persistSensorSelection`-Aufruf" (das war falsch,
weil die Start-Notice nie ueber `persistSensorSelection` entsteht) -
richtig: **genau eine Nutzlast pro tatsaechlicher Sensorentscheidung**,
transportiert entweder ueber `persistSensorSelection` (automatische
Ursachen) oder ueber `persistCommand` (Start, manuelle Moduswechsel,
`RecheckProduct`-Erfolg).

**Anschluss der Start-Notice (Review-Befund 5):**

```cpp
struct CommandDecision {
    // ... bestehende Felder unveraendert ...
    std::optional<StartSensorSelectionNotice> startSensorSelectionNotice;
    std::optional<SensorSelectionEvent> sensorSelectionEvent;
    std::optional<SensorSelectionNotice> sensorSelectionNotice;
};

struct RunPersistenceResult {
    // ... bestehende Felder unveraendert ...
    std::optional<StartSensorSelectionNotice> startSensorSelectionNotice;
    std::optional<SensorSelectionEvent> sensorSelectionEvent;
    std::optional<SensorSelectionNotice> sensorSelectionNotice;
};
```

`decideProgramStart`/`decideManualStart` fuellen
`CommandDecision::startSensorSelectionNotice` analog zu `startSummary` -
bereits vor der Bestaetigungspruefung fuer die Vorschau (angeforderter UND
effektiver Modus sichtbar), aber `persistCommand` kopiert das Feld nach
`RunPersistenceResult` **erst nach erfolgreichem Commit**, exakt wie es
bereits heute `result.effects = decision.effects` nach dem RAM-Apply-
Schritt tut - keine neue Reihenfolge, dieselbe bestehende. Bei
Schreibfehler bleibt `RunPersistenceResult::startSensorSelectionNotice`
`std::nullopt`; die Vorschau (`CommandDecision`) bleibt davon unberuehrt,
weil sie unabhaengig von der Persistenz erzeugt wird. Direkter Start ohne
Ersatz (Zeile 1/3/4/6/7/9/11 der Startmatrix) erzeugt keine
`StartSensorSelectionNotice` (nur bei tatsaechlichem `requestedMode !=
effectiveMode`).

Die drei Nutzlasttypen werden weiterhin ueber die bestehende `runRevision`-
Zahl an den Lauf gebunden, nicht ueber `RunRevision`/`RunChangeReason`.

### 6.12 Persistierter Sensorselektionszustand und die #17/#18-Recoverygrenze

Unveraendert seit Revision 4 (Provenienzmodell 6.12.1, Schema-Bump und
Zwei-Slot-Migration 6.12.2, #17/#18-Abgrenzung/Variante B 6.12.3). **Neuer
Querverweis:** `restoreRunPersistenceSnapshot` setzt
`sensorSelectionRuntime` (6.4.11) beim Restore auf den Default
(`RestartRevalidationPending`/`Blocked`) - dieselbe RAM-Default-
Initialisierung, die bereits in Revision 3/4 fuer den Restart-Vertrag
vorausgesetzt wurde, jetzt am konkret benannten Feld.

### 6.13 Zentrale Cross-Field-Validierung

Unveraendert seit Revision 4 (sechs Regeln inklusive der generellen
`fallbackDelaySeconds`-Regel und der separaten `AirOnly`-Regel).

### 6.14 Atomarer Persistenzpfad fuer laufrelevante Sensorentscheidungen

#### 6.14.1 Neue Typen

Unveraendert seit Revision 3/4: `RunPersistenceMutationKind::SensorSelection`,
`RunCheckpointTrigger::SensorSelection`.

**Neu, fuer den manuellen Pfad (Review-Befund 2):**

```cpp
enum class CommandKind : std::uint8_t {
    // ... bestehende Werte unveraendert ...
    ApplySensorSelectionAction,
};
```

`CommandKind` wird an mehreren Stellen erschoepfend geschaltet
(`isRunComfortCommand`, siehe 3); der neue Wert erzwingt dort eine bewusste
Entscheidung. `isRunComfortCommand(CommandKind::ApplySensorSelectionAction)
= false`: eine Blockierung waehrend `criticalSafetyEventPending` wuerde
einen Lauf ohne jeden Ausweg in `UserDecisionRequired` einfrieren, obwohl
gerade diese Aktion der vorgesehene Weg aus dieser Lage ist - dieselbe
Begruendung, aus der `ResetFault`/`AcknowledgeMessage`/`MuteMessage` bereits
`false` sind.

`isPersistedRunCommand(CommandKind::ApplySensorSelectionAction) = true`
(analog zu allen anderen laufwirksamen Kommandos).

#### 6.14.2 `writeSnapshot`-Korrektur und `applyRunCommand`-Erweiterung

`writeSnapshot`-Korrektur unveraendert seit Revision 3/4 (expliziter
`RunPersistenceMutationKind`-Parameter).

**Neu (Review-Blocking 1, notwendige Korrektur einer bestehenden, gemeinsam
genutzten Funktion):** `applyRunCommand` (`run_commands.cpp` Zeile 848-874)
erhaelt zwei zusaetzliche Felder in seiner Staleness-Vergleichsliste:

```cpp
current.sensorSelectionRuntime != decision.before.sensorSelectionRuntime ||
current.sensorSelection != decision.before.sensorSelection ||
```

**Begruendung und Tragweite:** ohne diese Ergaenzung wuerde ein zwischen
Entscheidung und Anwendung durch eine parallele automatische Bewertung
veraenderter Auswahlzustand (z. B. ein inzwischen eingetretener
`SafeLocked`-Zustand) von einer noch auf dem alten Zustand basierenden
manuellen Kommandoentscheidung stillschweigend ueberschrieben - ein
Sicherheits-Lock koennte verloren gehen. Diese Aenderung betrifft **jeden**
bestehenden Kommandopfad, nicht nur den neuen, weil `applyRunCommand`
generisch fuer alle `CommandKind`-Werte verwendet wird; sie wird deshalb im
Dateischnitt (8) explizit als eigener, isoliert ueberpruefbarer Diff
gefuehrt, nicht als Nebeneffekt des neuen Kommandos.

#### 6.14.3 `persistSensorSelection` (automatisch) und `decideApplySensorSelectionAction` (manuell)

**`persistSensorSelection`** unveraendert im Aufbau seit Revision 4, jetzt
mit korrigiertem Geltungsbereich: transportiert genau die **sechs**
automatischen Ursachen (`ProductFailureBlock`, `FallbackToAir`,
`AutomaticValidatedReturn`, `RecoveryRevalidation`, `SafeStateEntry`,
`ReturnValidationAborted`), nicht die beiden manuellen
(`ManualUserFallback`, `ManualUserReturn`). Nur aus `Ready`/`ReadyEmpty`
aufrufbar.

**Neu: `decideApplySensorSelectionAction`** (Review-Befund 2, empfohlene
Loesung):

```cpp
[[nodiscard]] CommandDecision decideApplySensorSelectionAction(
    const RunCommandState& current,
    const SensorSelectionCommandRequest& request);
```

```text
decide command
-> ruft applySensorSelectionDecision(current, decision) auf derselben
   Kandidatenkopie auf wie der automatische Pfad (6.4.11) - keine zweite
   Regelimplementierung
-> bei persistWorthy == false: CommandStatus::NoChange, kein persistCommand-
   Aufruf durch die aufrufende Schicht (bestehendes Muster, siehe 3)
-> bei persistWorthy == true: CommandStatus::Proposed, `after` traegt den
   aktualisierten Kandidaten inklusive sensorSelectionRuntime/
   sensorSelection
-> persistCommand mit CommandId (bestehender Pfad, RunPersistenceMutationKind::
   Command, KEIN neuer Mutationskind)
-> RAM apply ueber applyRunCommand (6.14.2, mit den neuen Vergleichsfeldern)
-> Event/Notice/Permission-Effekt erst nach Commit (unveraendertes Muster)
```

Damit landet die `CommandId` regulaer im bestehenden, begrenzten
persistierten Kommando-ID-Fenster (`persistedRunCommandIds`), ein
Wiederholungsversuch liefert `CommandStatus::AlreadyProcessed`
(`applyRunCommand`, `containsProcessedCommand`) - keine zweite
Idempotenzimplementierung.

#### 6.14.4 Ergebnis-, Ereignis- und Aktorwirkungsanschluss

**Umbenennung (Review-Befund 6):** `CommandEffect::SensorSelectionPeltierBlocked`/
`SensorSelectionPeltierReleased` aus Revision 4 werden umbenannt in:

```cpp
enum class CommandEffect : std::uint8_t {
    // ... bestehende Werte unveraendert ...
    SensorSelectionPermissionBlocked,
    SensorSelectionPermissionRestored,
};
```

**`SensorSelectionPermissionRestored != Peltier einschalten`** - der Effekt
transportiert ausschliesslich, dass #21s fachliche Voraussetzungen wieder
erfuellt sind (`peltierPermission = Allowed`). #23/#24 pruefen weiterhin
alle uebrigen Freigaben (Regeltotzeit, Aktorsicherheitslogik,
Verriegelungen), bevor ein Aktor tatsaechlich schaltet. Diese Klarstellung
wird an jeder Stelle uebernommen, an der Revision 4 die alten Namen
verwendete (dieser Abschnitt, Testmatrix 9, PR-Beschreibung).

Ansonsten unveraendert seit Revision 4: Effekte nur nach erfolgreichem
Commit, keine Teilwirkung bei Fehlern, Einzelwerte statt Arrays fuer Event/
Notice.

#### 6.14.5 Datei- und Testschnitt

- `run_persistence_contract.hpp/.cpp`, `run_commands.hpp`,
  `run_persistence_codec.hpp/.cpp`, `run_persistence_coordinator.hpp/.cpp`:
  unveraendert seit Revision 4 benannte Dateien, jetzt zusaetzlich mit den
  umbenannten `CommandEffect`-Werten und den neuen
  `RunPersistenceResult`-Feldern.
- **neu:** `run_commands.hpp/.cpp`: `CommandKind::ApplySensorSelectionAction`,
  `SensorSelectionUserAction`, `SensorSelectionCommandRequest`,
  `decideApplySensorSelectionAction`, `SensorSelectionRuntimeState`,
  `RunCommandState::sensorSelectionRuntime`, `applyRunCommand`-Erweiterung
  (isoliert ueberpruefbar, siehe 8).
- `sensor_selection.hpp/.cpp`: `applySensorSelectionDecision` als die eine
  kanonische Anwendungsfunktion (6.4.11), `computeRestartSensorSelection`
  (6.12.3) unveraendert.
- direkt betroffene Tests: `test/test_run_commands/`,
  `test/test_run_persistence_coordinator/`, `test/test_run_checkpoint_codec/`,
  `test/test_sensor_selection/`.

## 7. Modul- und Abhaengigkeitsgrenzen

Unveraendert seit Revision 3/4. `applySensorSelectionDecision` lebt im
`sensor_selection`-Kern und wird von `run_commands.cpp`
(`decideApplySensorSelectionAction`) sowie vom automatischen
Bewertungsaufrufer aufgerufen - keine neue Abhaengigkeitsrichtung.

## 8. Voraussichtlicher Datei- und Commit-Schnitt

Gegenueber Revision 4 wird der bisherige Commit 4 in zwei unabhaengig
ueberpruefbare Commits gesplittet, weil er sonst Startmatrix, manuellen
Laufvertrag, `ProductRequired`-Aktionsgate, den neuen Kommandovertrag und
die `applyRunCommand`-Korrektur gleichzeitig getragen haette.

### Commit 1 - Programmschema (6.2, 6.13)

Unveraendert seit Revision 3/4.

### Commit 2 - Auswahlkern (6.1, 6.3, 6.4, 6.7, 6.10)

- `sensor_selection.hpp/.cpp`: Werttypen, `SensorSelectionRuntimeState`-
  Definition (Typ; die tatsaechliche RAM-Feldhaltung liegt in
  `RunCommandState`, siehe Commit 4), `applySensorSelectionDecision` als
  reine Funktion, vollstaendiger Zustandsautomat inklusive 6.4.9-6.4.14,
  Re-Arm-Regel (6.4.12), `ThermalCompatibility`-Invarianten (6.10),
  `computeRestartSensorSelection`;
- `test/test_sensor_selection/test_sensor_selection.cpp`.

### Commit 3 - Persistenzmechanik: Schema, Migration, automatischer Sensorpfad (6.12, 6.14.1-6.14.2 Codec-Teil, 6.14.4)

Unveraendert seit Revision 4 im Kern (`PersistedSensorSelectionState`,
Schema-Bump, `persistSensorSelection`, `RunCheckpointTrigger`/
`RunPersistenceMutationKind::SensorSelection`), jetzt mit korrigiertem
Geltungsbereich (sechs statt acht Ursachen) und den umbenannten
`CommandEffect`-Werten sowie den neuen `RunPersistenceResult`-Feldern.
Weiterhin **kein** `LoadedActiveRun`-Mutationspfad.

### Commit 4 - Kommandovertrag fuer manuelle Sensorentscheidungen (6.4.11, 6.14.2 `applyRunCommand`-Teil, 6.14.3)

**Neuer, isolierter Commit (Review-Scope-Hinweis).**

- `run_commands.hpp/.cpp`: `RunCommandState::sensorSelectionRuntime`,
  `CommandKind::ApplySensorSelectionAction`, `SensorSelectionCommandRequest`,
  `decideApplySensorSelectionAction`, `isRunComfortCommand`/
  `isPersistedRunCommand`-Erweiterung, **`applyRunCommand`-Staleness-Fix**
  (eigener, kompakter Diff mit eigenem Test gegen jeden bestehenden
  Kommandopfad, nicht nur den neuen);
- `test/test_run_commands/test_run_commands.cpp`.

### Commit 5 - Startvertragsanschluss (6.5, 6.8, 6.9, 6.11 Start-Notice)

- `run_commands.hpp/.cpp`: Startmatrix-Pruefung in `decideProgramStart`,
  fester manueller Produktlaufvertrag (6.8), `StartSensorSelectionNotice`-
  Anschluss an `CommandDecision`/`RunPersistenceResult`;
- direkt betroffene Tests unter `test/test_run_commands/`.

### Commit 6 - fachliche Dokumentation und Abschlussnachweise

Unveraendert seit Revision 3/4, ergaenzt um die explizite #18-Handover-Notiz
(siehe 9.3) in `docs/RUN_PERSISTENCE.md`/`docs/RECOVERY_AND_INTERRUPTION.md`.

## 9. Teststrategie und Testmatrix

### 9.1 Programmschema, verkettete Migration, Codec (Commit 1)

Unveraendert seit Revision 3/4.

### 9.2 Unit-Tests des Auswahlkerns (Commit 2)

Unveraendert seit Revision 4, ergaenzt um:

- **vollstaendige Policy-/Aktionsmatrix** (6.4.14) als Einzeltestfaelle;
- `applySensorSelectionDecision` ist die einzige Stelle, die
  `sensorSelectionRuntime` veraendert - Testfall belegt, dass eine direkt
  konstruierte abweichende Runtime-Kombination von der Funktion selbst nie
  erzeugt wird (z. B. `phase = SafeLocked` mit `permission = Allowed` ist
  unerreichbar);
- **Vorrangregel** (6.4.9): `StopToSafeState` + Produktausfall erzeugt
  genau eine `SafeStateEntry`, kein `ProductFailureBlock`-Vorlauf;
  gleichzeitiger Air-/Cooling-Ausfall waehrend `ReturnValidationPending`
  erzeugt genau eine `SafeStateEntry`, kein separates
  `ReturnValidationAborted`;
- **Re-Arm-Pflichttests** (6.4.12, wörtlich aus dem Review uebernommen):
  10.000 identische Bewertungen mit `Unavailable` erzeugen keinen
  Flashwrite; wiederholtes `Incompatible` erzeugt hoechstens einen Abort
  pro Versuch; eine neue Evidenzgeneration (`profileRevision`-Aenderung)
  startet genau einen neuen Versuch; `Compatible` nach vollstaendiger
  Stabilitaet erzeugt genau eine Rueckkehr; Produkt faellt waehrend
  `ReturnValidationPending` aus und rearmt erst nach einer neuen gueltigen
  Wiedererkennung;
- `RecheckProduct` hebt die Retry-Sperre innerhalb derselben Entscheidung
  auf, bevor die ausgeloeste Neubewertung erfolgt (Reihenfolge-Testfall);
- `RecheckProduct` aus einem nicht zulaessigen Zustand liefert
  `CommandStatus::NotAllowedInState`;
- `ThermalCompatibilityEvidence`-Invarianten (6.10): `profileRevision == 0`
  bei `Compatible`/`Incompatible`/`Stale` -> `InvalidContext`;
  `evaluatedAtMonotonicMillis` in der Zukunft -> `InvalidContext`; #21
  fuehrt nachweislich keine eigene Altersschwelle auf
  `evaluatedAtMonotonicMillis` aus (Regressionstest gegen eine versehentlich
  hinzugefuegte zweite `Stale`-Bewertung).

### 9.3 Persistenzmechanik: automatischer Pfad (Commit 3)

Unveraendert seit Revision 4, mit korrigierter Ursachenzahl (sechs
automatische statt acht) und den umbenannten Effektwerten.

**Praezisierung der #17/#18-Testgrenze (Review-Befund 8, ersetzt eine zu
optimistische Aussage in Revision 4):**

In #21 testbar (ausschliesslich ueber die bestehende oeffentliche API,
kein Zugriff auf private Coordinatorzustaende):

```text
Codec-/Contract-Test mit gemischtem Schema-2-Current und Schema-1-Fallback
Head-/Referenzvalidierung beider bekannten Versionen ({1U, 2U})
Schema-1 laden endet in LoadedActiveRun / RecoveryPending
persistSensorSelection aus LoadedActiveRun bleibt RecoveryPending
  (Regressionstest, siehe Revision 4)
Schema-2-Schreiben aus einem regulaeren Ready-Fixture (nicht aus einem
  ueber loadAndInitialize() geladenen LoadedActiveRun-Zustand)
```

Nicht in #21 testbar, sondern erst in #18 zu erfuellen (in diesem Plan
**nicht** als #21-Testzeile behauptet):

```text
tatsaechliche Recoveryaktivierung eines geladenen Schema-1-Laufs
erster Schema-2-Commit nach dieser Aktivierung
anschliessender Neustart mit Schema-2-Current und Schema-1-Fallback
Aktorfreigabe erst nach persistierter Recoveryentscheidung
```

Diese Uebergabe wird zusaetzlich zu diesem Plan als benannter Abschnitt in
`docs/RUN_PERSISTENCE.md` oder `docs/RECOVERY_AND_INTERRUPTION.md`
dokumentiert (Commit 6), damit sie bei der #18-Planung nicht erneut
recherchiert werden muss.

### 9.4 Kommandovertrag fuer manuelle Sensorentscheidungen (Commit 4)

**Neuer Abschnitt.**

- `ContinueWithAir`/`ReturnToProduct`/`RecheckProduct` je aus jedem
  zulaessigen und mindestens einem unzulaessigen Ausgangszustand;
- Wiederholungsversuch derselben `CommandId` liefert `AlreadyProcessed`,
  keine zweite Wirkung;
- **`applyRunCommand`-Staleness-Regressionstest** (Review-Blocking 1):
  `ContinueWithAir` wird gegen `UserDecisionRequired` entschieden; eine
  zwischenzeitliche automatische Bewertung fuehrt zu `SafeLocked`; die
  Anwendung des ersten Kommandos liefert `CommandStatus::StaleState`, der
  `SafeLocked`-Zustand bleibt erhalten;
- derselbe Regressionstest wird gegen mindestens einen weiteren,
  bestehenden `CommandKind` (z. B. `AdjustRun`) wiederholt, um zu
  bestaetigen, dass die erweiterte Vergleichsliste bestehende Pfade nicht
  bricht;
- `RunPersistenceResult`-Anschluss: `sensorSelectionEvent`/
  `sensorSelectionNotice` nur nach erfolgreichem Commit gesetzt;
  `CommandEffect::SensorSelectionPermissionBlocked`/`-Restored` korrekt
  gesetzt, keine Verwechslung mit einer direkten Aktorfreigabe (Testname
  spiegelt das wider).

### 9.5 Startvertragsanschluss (Commit 5)

Unveraendert seit Revision 4, ergaenzt um: `StartSensorSelectionNotice`
erscheint in `CommandDecision` bereits vor Bestaetigung, in
`RunPersistenceResult` erst nach Commit; Schreibfehler beim Start erzeugt
keine scheinbar ausgefuehrte Start-Notice; direkter Start ohne Ersatz
erzeugt keine Notice.

### 9.6 Gezielte Ausfuehrung nach Freigabe

```bash
pio test -e native --filter test_program_models
pio test -e native --filter test_configuration_codecs
pio test -e native --filter test_configuration_migration
pio test -e native --filter test_sensor_selection
pio test -e native --filter test_run_checkpoint_codec
pio test -e native --filter test_run_persistence_coordinator
pio test -e native --filter test_run_commands
python scripts/check_architecture_boundaries.py
python scripts/check_secrets.py
git diff --check
```

## 10. Safety-, Security-, Recovery- und Hardwaregrenzen

Unveraendert seit Revision 4, ergaenzt um:

- die Vorrangregel (6.4.9) garantiert, dass ein sicherheitsrelevanter
  Zustand (`SafeLocked`) nie durch eine gleichzeitig moegliche schwaechere
  Klassifikation verdraengt wird;
- die Re-Arm-Sperre (6.4.12) verhindert eine unbegrenzte automatische
  Wiederholung derselben abgelehnten Rueckkehr und begrenzt damit auch die
  Flash-Schreibfrequenz;
- `SensorSelectionPermissionRestored` ist ausdruecklich **keine**
  Aktorfreigabe - nachgelagerte Safety-/Regel-/Aktorlogik (#23/#24) prueft
  weiterhin alle uebrigen Bedingungen, bevor ein Aktor schaltet;
- die `applyRunCommand`-Erweiterung (6.14.2) verhindert, dass eine
  zwischenzeitlich eingetretene Sicherheitssperre durch eine veraltete
  manuelle Kommandoentscheidung stillschweigend rueckgaengig gemacht wird.

## 11. Ressourcen- und Betriebsbudget

Unveraendert seit Revision 4, ergaenzt um:

- `SensorSelectionRuntimeState` ist RAM-only und konstant gross (ein Phase-
  Enum, ein Permission-Enum, zwei `optional<uint64_t>`, ein `optional<uint32_t>`)
  - keine Wireformat-Wirkung, keine Aenderung am 8-KB-Checkpoint-Budget;
  - `kReturnValidationRetryIntervalMillis` ist ein einzelner firmwarefester
  Grenzwert, keine neue Datenstruktur.

## 12. SOLID-, DRY- und KISS-Bewertung des geplanten Diffs

Unveraendert seit Revision 4, ergaenzt um:

- **Single Responsibility:** `applySensorSelectionDecision` ist die einzige
  Stelle, die Automat-Regeln kennt; `persistSensorSelection` und
  `decideApplySensorSelectionAction` sind duenne Transport-/
  Persistenzwrapper darum (Korrektur ggue. Revision 4, die implizit zwei
  Schreiber zuliess).
- **DRY:** die Vorrangregel (6.4.9) ersetzt mehrere Einzelfall-
  Entscheidungen (`StopToSafeState`, gleichzeitiger Air-/Cooling-Ausfall)
  durch eine einzige Regel; `RecoveryRevalidation`/`SafeStateEntry` teilen
  sich dieselbe Anwendungsfunktion unabhaengig vom Ausloeser.
- **KISS:** die Re-Arm-Regel nutzt ausschliesslich bereits vorhandene
  Bausteine (Evidenzgeneration, monotone Zeit, bestehendes
  Kommandovokabular) statt einer neuen Zustandsmaschine fuer sich genommen.

## 13. Offene Ownerentscheidungen und Gates

Unveraendert seit Revision 4 (P21-01, P21-M1 bis P21-M3 als echte Gates,
P21-M4 als Abhaengigkeitsaussage). Diese Revision trifft zusaetzlich
folgende, dem Owner zur Bestaetigung (nicht zur offenen Auswahl)
vorgelegte kanonische Entscheidungen innerhalb von P21-01 (Aktions-/
Zeitverhalten) und P21-M2 (Laufzeitzustand/Persistenzweg):

- Vorrangregel `SafeStateEntry` schlaegt alle anderen Klassifikationen
  (6.4.9);
- vereinheitlichte `ContinueWithAir`-Regel unabhaengig von Timeout-Status
  (6.4.10);
- manuelle Moduswechsel ueber `persistCommand`/neuen `CommandKind`, nicht
  ueber `persistSensorSelection` (6.14.3);
- Re-Arm-Bedingungen (i)-(iv) fuer die automatische Rueckkehrvalidierung
  (6.4.12);
- Stale-Eigentuemerschaft beim Produzenten, keine #21-eigene
  Altersarithmetik (6.10).

## 14. Dokumentations- und Abschlussnachweise

Unveraendert seit Revision 4, ergaenzt um: die #18-Handover-Notiz (9.3)
wird als eigener, benannter Abschnitt in einem Fachvertrag nachgewiesen,
nicht nur im Plan-PR erwaehnt.

## 15. Verbindliche `/task`-Taskliste fuer die Umsetzung

```text
/task
[ ] exakten freigegebenen Plan-Commit und Ownerkommentar `PLAN APPROVED` verifizieren
[ ] aktuellen Branch, HEAD, Live-Issue #21, Abhaengigkeiten (inkl. #17/#18-Stand) und Roadmap erneut pruefen
[ ] seit der Planfreigabe geaenderte Quellen, ADRs, Vertraege und lokale Regeln inkrementell lesen
[ ] P21-01, P21-M1 bis P21-M3 aufgeloeste Ownerentscheidungen gegen den Plan abgleichen
[ ] ReturnStrategy-Enum, Feldmaske, Schema 6, verkettete Migration implementieren
[ ] Codec-/Katalog-/Beispielkonfigurationsaenderungen implementieren
[ ] SensorSelectionRuntimeState, applySensorSelectionDecision und vollstaendigen Zustandsautomaten (6.4.9-6.4.14) implementieren
[ ] Vorrangregel und Re-Arm-Regel implementieren und mit den Pflichttests belegen
[ ] ThermalCompatibilityEvidence-Invarianten implementieren, keine eigene Altersarithmetik ergaenzen
[ ] PersistedSensorSelectionState, kRunPersistenceSchema-Bump, persistSensorSelection fuer die sechs automatischen Ursachen implementieren
[ ] CommandKind::ApplySensorSelectionAction, SensorSelectionCommandRequest, decideApplySensorSelectionAction implementieren
[ ] applyRunCommand um sensorSelectionRuntime/sensorSelection erweitern und gegen bestehende Kommandopfade regressionstesten
[ ] StartSensorSelectionNotice an CommandDecision/RunPersistenceResult anschliessen
[ ] CommandEffect auf SensorSelectionPermissionBlocked/-Restored umbenennen und dokumentieren, dass dies keine Aktorfreigabe ist
[ ] Startmatrix (6.5) in decideProgramStart durchsetzen, ProductRequired-Aktionsausschluss (6.4.13) implementieren
[ ] festen Vertrag fuer produktgefuehrte manuelle Laeufe (6.8) implementieren
[ ] verifizieren, dass LoadedActiveRun unveraendert RecoveryPending bleibt
[ ] computeRestartSensorSelection als reine Funktion fuer die spaetere #18-Integration implementieren
[ ] direkte, gezielte Unit-Tests fuer alle in Abschnitt 9 gelisteten Faelle ausfuehren
[ ] gezielte Architektur-, Secret-, Format- und git diff --check-Pruefungen ausfuehren
[ ] betroffene Fachvertraege inklusive #18-Handover-Notiz und docs/ACCEPTANCE_TESTS.md aktualisieren
[ ] docs/ROADMAP.md nur bei tatsaechlicher Status- oder Gatewirkung synchronisieren
[ ] Review des vollstaendigen aktuellen Diffs gegen Issue, Plan, ADRs und Fachvertraege durchfuehren
[ ] SOLID-, DRY- und KISS-Bewertung gegen den tatsaechlichen Diff durchfuehren
[ ] P21-M4-Abhaengigkeitsaussage im PR sichtbar dokumentieren
[ ] alle Reviewbefunde fachlich bewerten; Threads nur nach ausdruecklicher Autorisierung bearbeiten
[ ] PR-Beschreibung mit Plan-SHA, aktuellem HEAD, Tests, Abweichungen und Restgates aktualisieren
[ ] Owner setzt Draft erst nach befundleerem Review auf Ready for review
[ ] genau eine vollstaendige Remote-CI fuer den reviewten HEAD abwarten und Ergebnis dokumentieren
[ ] bei CI-Fehler PR-Draft-/Korrektur-/Reviewzyklus gemaess Workflow durchfuehren
[ ] Abschlussnachweise, geaenderte Dateien und offene Gates vollstaendig dokumentieren
[ ] HALTED_FOR_OWNER_REVIEW beziehungsweise Owner-Entscheidung dokumentieren
```

## 16. Stopbedingung

Nach Commit und Push dieser Planrevision sowie der notwendigen
Roadmap-/PR-/Handover-Aktualisierung wird im Draft-PR der exakte neue
Plan-Commit, der aktuelle HEAD und

```text
IMPLEMENTATION_BLOCKED_PENDING_PLAN_APPROVAL
```

eingetragen. Danach haelt der Agent an. Implementierung beginnt ausschliesslich
nach einem Ownerkommentar der Form:

```text
PLAN APPROVED
Approved plan commit: <exakte Plan-Commit-SHA>
```

Die Freigabe gilt nur fuer genau diese Planversion. Der PR bleibt Draft; der
Agent setzt ihn nicht auf `Ready for review`, startet keine vollstaendige
Remote-CI, merged nicht und loescht den Branch nicht.

## 17. Planungs-Taskliste

```text
/task
[x] Live-main, Branch, Arbeitsbaum und PR #98 verifizieren
[x] Live-Issue #21 und Kommentare lesen
[x] Abhaengigkeiten #14, #17, #18, #20 live verifizieren
[x] Root- und lokale AGENTS-Regeln lesen
[x] Dokumentationsprioritaet und relevante Fachvertraege lesen
[x] bestehenden #20-Sensorqualitaetskern und Lauf-/Persistenzmodelle inventarisieren
[x] fehlende Auswahl-, Fallback-, Rueckkehr- und Ereignisvertraege abgrenzen
[x] offene Ownerentscheidungen und materielle Planabweichungen dokumentieren
[x] SOLID-, DRY- und KISS-Bewertung des geplanten Diffs erstellen
[x] vollstaendige Umsetzung-, Test-, Dokumentations-, Review- und Abschluss-Taskliste erstellen
[x] Plan Revision 1 committen und pushen; Draft-PR aktualisieren (Plan-Commit c505fce6...)
[x] Plan Revision 2 committen und pushen; Draft-PR aktualisieren (Plan-Commit aaeefbdf...)
[x] Plan Revision 3 committen und pushen; Draft-PR und SESSION HANDOVER aktualisieren (Plan-Commit 2e3a0411...)
[x] Plan Revision 4 committen und pushen; Draft-PR und SESSION HANDOVER aktualisieren (Plan-Commit 6a85c331...)
[x] PR-#99-Reviewbefunde zu Revision 4 gegen Code verifiziert: CommandDecision-Struktur, applyRunCommand-Generizitaet und Staleness-Luecke, CommandStatus::NoChange-Muster
[x] Eigentuemer und kanonische Anwendungsfunktion fuer den fluechtigen Sensorselektions-Laufzeitzustand geplant (SensorSelectionRuntimeState, applySensorSelectionDecision)
[x] manuelle Sensorentscheidungen als echte persistierte Kommandos modelliert (CommandKind::ApplySensorSelectionAction, decideApplySensorSelectionAction)
[x] applyRunCommand-Staleness-Fix als eigenen, isoliert testbaren Diff geplant
[x] Policy-Zeitverhalten korrigiert (WaitForUser ohne Wartezeit, vereinheitlichte ContinueWithAir-Regel, StopToSafeState als eine atomare Entscheidung ueber Vorrangregel)
[x] Re-Arm-Regel gegen Bewertungsschleife der automatischen Rueckkehrvalidierung geplant, inklusive Pflichttests
[x] Start-Notice vollstaendig an CommandDecision/RunPersistenceResult angeschlossen, falsche Kardinalitaetsaussage korrigiert
[x] CommandEffect auf SensorSelectionPermissionBlocked/-Restored umbenannt
[x] ThermalCompatibilityEvidence-Invarianten und Stale-Eigentuemerschaft festgelegt
[x] Testmatrix an die #17/#18-Grenze angepasst (kein End-to-End-Testanspruch in #21)
[x] Ursachenzahl korrigiert (acht Nicht-Start-Ursachen, sechs automatisch/zwei manuell)
[x] Planungs-Taskliste bereinigt (Commit/Push/Handover/PR-Aktualisierung fuer Revision 1-4 als erledigt markiert)
[x] ausschliesslich Plan und notwendige Roadmap-/PR-/Handover-Aktualisierung geaendert
[ ] Plan committen und pushen
[ ] SESSION-HANDOVER-Kommentar auf neuen HEAD aktualisieren (ersetzt den bestehenden Kommentar, keinen zweiten anlegen)
[ ] Draft-PR mit exakter neuer Plan-SHA, aktuellem HEAD und aufgeloesten/verbleibenden Ownerentscheidungen aktualisieren
[ ] HALTED_FOR_OWNER_REVIEW
```
