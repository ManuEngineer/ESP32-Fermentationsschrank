# Issue #132 – R1-Hardwaregate-Semantik vor Integration bereinigen

```text
PLAN_REVISION=FULL_REPLACEMENT
SUPERSEDES_PLAN_SHA=34b37cb400ca5640b364103fcfd6a4ff128b8514
APPROVED_PLAN_SHA=PENDING
OWNER_PLAN_REVIEW_REQUIRED=YES
IMPLEMENTATION_ALLOWED=NO
```

Diese Revision ist vollständig und eigenständig ausführbar. Sie ersetzt die
vorherige Planfassung vollständig; sie ist keine Delta- oder Amendmentfassung.

## 1. Ziel und verbindliche Ownerentscheidungen

Vor einer späteren Promotion von `integration/r1-development` nach `main` wird
die aktuelle R1-Hardwaregate-Semantik kanonisch und widerspruchsfrei
aufgeräumt. Nicht ausgeführte elektrische Pegel- oder Bootmessungen werden
nicht als PASS behauptet. Die Nachweisart wird je Scope getrennt geführt.

Verbindlicher Ownervertrag:

```text
MULTIMETER_REQUIRED_FOR_R1_ACCEPTANCE=NO
BOOT_LEVEL_MEASUREMENT_REQUIRED=NO
GPIO_VOLTAGE_MEASUREMENT_REQUIRED=NO
ELECTRICAL_LEVEL_MEASUREMENT_REQUIRED=NO
R_IS_L_IS_R1=DISABLED
R_IS_L_IS_LEVEL_MEASUREMENT_REQUIRED=NO
R_IS_L_IS=FUTURE_RELEASE
```

Der einzige kanonische Namenssatz für Hardware-Nachweisarten lautet:

```text
ELECTRICAL_LEVEL_MEASUREMENT=NOT_REQUIRED_WAIVED
SSOT_CONFORMANCE=<PASS/PENDING/NOT_APPLICABLE>
FUNCTIONAL_HARDWARE_VERIFICATION=<PASS/PENDING/NOT_APPLICABLE>
ADAPTER_SAFETY_VERIFICATION=<PASS/PENDING/NOT_APPLICABLE>
THERMAL_COMMISSIONING=<PASS/PENDING/NOT_APPLICABLE>
```

Nur für den jeweiligen Scope sinnvolle Felder werden ausgegeben. Ein
Statusfeld wird nicht durch ein zweites Synonym oder einen alten Sammelstatus
parallel weitergeführt. Die kanonischen Werte sind in YAML als
`not_required_waived`, `pass`, `pending` beziehungsweise `not_applicable`
auszuführen; Live-Issue- und Roadmap-Statusblöcke verwenden die oben
angegebenen Großschreibungen.

Die R1-Verifikation verwendet, wo erforderlich, die Board-/Wiring-SSOT,
funktionale Hardwaretests, Software-/Adapter-Safety-Nachweise sowie spätere
thermische Commissioningtests. Ein funktionaler Test bestimmt eine beobachtete
Funktion; er erfindet keinen nicht gemessenen elektrischen Pegel.

## 2. Verifizierte Live-Baseline

Die Planung basiert auf dem am 2026-09-01 erneut live verifizierten Stand:

```text
REPOSITORY=ManuEngineer/ESP32-Fermentationsschrank
BASE_BRANCH=integration/r1-development
BASE_SHA=1fc22d693bae8572144bf61d242a2fe6d0b093bc
MAIN_SHA=87dd593fcdc8d26831873a4163b174340b4347c0
INTEGRATION_AHEAD_OF_MAIN=257
INTEGRATION_BEHIND_MAIN=0
ISSUE29=CLOSED
PR129=MERGED
PR129_MERGE_COMMIT=1fc22d693bae8572144bf61d242a2fe6d0b093bc
ISSUE130=CLOSED
PR131=MERGED
PR131_MERGE_COMMIT=1fd8f6af53d1b3c23f3aa46c73c4fc3da7513d6d
ISSUE132=OPEN
PR133=OPEN_DRAFT
PR133_HEAD=34b37cb400ca5640b364103fcfd6a4ff128b8514
```

Der Arbeitszweig ist
`agent/issue-132-r1-hardware-gate-cleanup`, abgeleitet von
`origin/integration/r1-development`. Die vorherige Plan-SHA
`34b37cb400ca5640b364103fcfd6a4ff128b8514` ist nicht freigegeben und wird
durch diese Revision superseded.

Live-Quellen, die nach Planfreigabe synchronisiert werden:

- [Issue #29](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/issues/29),
  [Issue #31](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/issues/31),
  [Issue #32](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/issues/32),
  [Issue #33](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/issues/33)
  und [Issue #130](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/issues/130);
- [PR #133](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/pull/133),
  der bis zum Owner-Gate Draft bleibt.

## 3. Scope und Nicht-Ziele

### Im Scope

- `docs/ROADMAP.md` nach Issue #29 / PR #129 und Issue #130 / PR #131
  synchronisieren. Issue #29 darf nicht mehr als aktive Priorität oder
  nächstes Gate erscheinen. Die nächste fachliche Phase bleibt exakt:

  ```text
  #25 -> #26 -> #31 -> #30 -> #32 -> #33
  ```

- Aktuelle normative Verwendungen des generischen Sammelstatus und der
  elektrischen Pegelmesspflicht in Repositorydateien und Live-Issues in die
  unten definierte Statusmatrix überführen.
- Das einzige handgepflegte Boardprofil
  `config/board_profiles/esp32_32e_quad_mosfet_r1.yaml` nur in
  Statusmetadaten ändern. GPIO-Zuordnung, Pull-ups/Pull-downs, Netzzuordnung,
  Widerstände, `active_level`, `safe_boot_level` und alle Designwerte bleiben
  byte- beziehungsweise wertgleich.
- `config/hardware.example.yaml` in seiner Hardware-Statusdarstellung
  deterministisch auf die getrennten Felder umstellen.
- `docs/HARDWARE.md`, `docs/DECISIONS.md`, `docs/ACCEPTANCE_TESTS.md`,
  `docs/OPEN_POINTS.md`, die direkt betroffenen Audit-/Anforderungsstellen
  sowie die aktuellen Issue-Bodies auf denselben Vertrag bringen.
- R_IS/L_IS im R1-Status ausdrücklich deaktiviert lassen und eine spätere
  Verwendung ausschließlich als `FUTURE_RELEASE` mit eigenem
  Issue-/Plan-/Owner-Gate-Scope beschreiben.
- Nur die durch diese Statusmetadaten tatsächlich betroffenen Syntax-,
  Konsumenten- und Dokumentationsprüfungen ausführen; keine Firmware- oder
  Hardwaretests.

### Nicht-Ziele

Keine #25- oder #26-Arbeit, keine Produktlogik, keine Produktionscode- oder
Testcodekorrektur, keine neue API, kein Wire- oder Persistenzformat, keine
neue Safetyfunktion, keine Adapter, keine Display-/Touch-/Sensor-/Lüfter- /
Summer-/BTS7960-/Peltierintegration, keine GPIO-Neuzuordnung, keine neue
Hardwaremessung, keine Aktorfreigabe und kein Integrations-PR nach `main`.

## 4. Kanonisches Statusmodell je Scope

Die folgenden Statuszeilen sind der vollständige Zielvertrag für die jeweils
genannten aktuellen Live-Statusblöcke. Nicht genannte Nachweisarten werden im
betreffenden Scope nicht künstlich als `NOT_APPLICABLE` ergänzt.

| Scope | `ELECTRICAL_LEVEL_MEASUREMENT` | `SSOT_CONFORMANCE` | `FUNCTIONAL_HARDWARE_VERIFICATION` | `ADAPTER_SAFETY_VERIFICATION` | `THERMAL_COMMISSIONING` |
|---|---|---|---|---|---|
| Boardprofil-Statusmetadaten | `NOT_REQUIRED_WAIVED` | `PENDING` | `PENDING` | nicht verwenden | nicht verwenden |
| Issue #31 Display/Touch | `NOT_REQUIRED_WAIVED` | `PENDING` | `PENDING` | `NOT_APPLICABLE` | nicht verwenden |
| Issue #32 MOSFET/Fan/Buzzer | `NOT_REQUIRED_WAIVED` | `PENDING` | `PENDING` | `PENDING` | nicht verwenden |
| Issue #33 BTS7960/Peltier-Adapter | `NOT_REQUIRED_WAIVED` | `PENDING` | `PENDING` | `PENDING` | nicht verwenden |

Issue #29 erhält keinen neuen pauschalen Hardware-Freigabestatus. Seine
abgeschlossene technische Evidenz bleibt mit den bestehenden
`BOOT_REQUALIFICATION=3_OF_3_PASS`, `OWNER_FINAL_REVIEW=PASS`,
`CI_RUN=996`, `CI_RESULT=PASS` und `ACTUATOR_RELEASE=NO` getrennt erhalten.
Für #29 wird ausschließlich die Owner-Waiversemantik
`ELECTRICAL_LEVEL_MEASUREMENT=NOT_REQUIRED_WAIVED` verwendet; daraus folgt
weder ein elektrischer Mess-PASS noch eine Aktorfreigabe.

Roadmap-Zielstatus für die abgeschlossene #29-Grundlage:

```text
ISSUE29=CLOSED_COMPLETED
PR129=MERGED
PR129_MERGE_COMMIT=1fc22d693bae8572144bf61d242a2fe6d0b093bc
OWNER_FINAL_REVIEW=PASS
CI_RUN=996
CI_RESULT=PASS
```

## 5. Exakte Statusfeld-Migrationsmatrix: Boardprofil

Die Matrix gilt für den aktuellen Stand von
`config/board_profiles/esp32_32e_quad_mosfet_r1.yaml`. `REMOVE` bedeutet,
dass kein Ersatzsynonym am alten Ort angelegt wird; der in der Zielspalte
genannte kanonische Status ist die einzige neue Quelle.

### 5.1 Profil- und Verifikationsfelder

| Feld | aktueller Wert | Entscheidung | exakter Zielzustand / Begründung |
|---|---|---|---|
| `profile.verification_state` | `planned_not_confirmed` | `REMOVE` | Kein allgemeiner Sammelstatus; maßgeblich sind ausschließlich die expliziten Felder unter `verification`. |
| `profile.electrical_verification` | `pending` | `REMOVE` | Ersetzt durch `verification.electrical_level_measurement: not_required_waived`; keine generelle elektrische Verifikation als R1-Gate. |
| `profile.confirmed_test` | `false` | `REMOVE` | Keine zweite aggregierte PASS-Quelle; der aktuelle Funktionsnachweis ist `verification.functional_hardware_verification: pending`. |
| `profile.actuator_release` | `false` | `KEEP` | Einzige Boardprofil-Freigabesperre; bleibt unverändert `false` beziehungsweise `NO`. |
| `verification.active_levels_confirmed` | `false` | `REMOVE` | Darf nicht als notwendiger Spannungs-/Pegel-PASS fortbestehen; funktionale Bestimmung gehört in den owning Hardwaretest. |
| `verification.boot_levels_confirmed` | `false` | `REMOVE` | Keine Boot-Pegelmesspflicht; funktionales fail-closed Boot-/Resetverhalten bleibt als Teil des Funktionsgates. |
| `verification.mosfet_electrical_behavior_confirmed` | `false` | `REMOVE` | Ersetzt durch `verification.functional_hardware_verification: pending` und Issue #32; kein Mess-PASS. |
| `verification.bts7960_logic_confirmed` | `false` | `REMOVE` | Ersetzt durch Issue #33 mit `FUNCTIONAL_HARDWARE_VERIFICATION=PENDING` und `ADAPTER_SAFETY_VERIFICATION=PENDING`. |
| `verification.display_touch_electrical_function_confirmed` | `false` | `REMOVE` | Ersetzt durch Issue #31 mit SSOT-/Controller-/Funktionsnachweisen. |
| `verification.gpio_functional_test` | `false` | `REMOVE` | Keine zweite Statusquelle; die funktionale Hardwareprüfung bleibt im owning Issue. |
| `verification.confirmed_test` | `false` | `REMOVE` | Kein allgemeines Bool-Gate neben dem kanonischen Funktionsstatus. |
| `verification.actuator_release` | `false` | `REMOVE` | Duplikat entfernen; `profile.actuator_release: false` ist die einzige Boardprofil-Freigabesperre. |
| `verification.electrical_verification` | `pending` | `RENAME_TO=verification.electrical_level_measurement` | Exakter Wert `not_required_waived`; kein Wert `electrical_verification` bleibt aktiv. |
| `verification.board_family_identity` | `confirmed_by_owner_reference_match` | `KEEP` | Beschreibender Owner-Referenzabgleich, kein elektrischer oder funktionaler PASS. |
| `verification.direct_reset_checks` | bestehende Designanforderungen | `KEEP` | Gemeinsame Masse, hochohmiger Reset-Eingang, kein Gegentreiben und kompatible Logikdomäne bleiben Design-/Funktionsanforderungen, keine automatische Multimetermesspflicht. |

Nach der Migration enthält der Statusbereich des Boardprofils mindestens:

```yaml
verification:
  board_family_identity: confirmed_by_owner_reference_match
  electrical_level_measurement: not_required_waived
  ssot_conformance: pending
  functional_hardware_verification: pending
```

`profile.actuator_release: false` bleibt außerhalb dieses Blocks als einzige
Boardprofil-Freigabesperre erhalten. Das Boardprofil führt keine
`ADAPTER_SAFETY_VERIFICATION` oder `THERMAL_COMMISSIONING`, weil diese nicht
die Verantwortung dieses Design-/Wiring-SSOT sind.

### 5.2 PCB-feste MOSFET-Kanalstatus

| Feld | aktueller Wert | Entscheidung | exakter Zielwert |
|---|---|---|---|
| `pins.gpio16.assignment_status` | `board_fixed_pending_electrical_verification` | `RENAME_TO` | `board_fixed_pending_functional_verification` |
| `pins.gpio17.assignment_status` | `board_fixed_pending_electrical_verification` | `RENAME_TO` | `board_fixed_pending_functional_verification` |
| `pins.gpio26.assignment_status` | `board_fixed_pending_electrical_verification` | `RENAME_TO` | `board_fixed_pending_functional_verification` |
| `pins.gpio27.assignment_status` | `board_fixed_pending_electrical_verification` | `RENAME_TO` | `board_fixed_pending_functional_verification` |

`board_fixed_pending_functional_verification` ist der einzige aktive
PCB-feste Pending-Wert. `board_fixed_pending_electrical_verification` darf im
aktiven Boardprofil und in aktuellen normativen Statusdefinitionen nicht als
Synonym verbleiben.

### 5.3 Beschreibende TBD-Felder

Die folgenden bestehenden Felder werden ausdrücklich `KEEP` und nicht in
Messwerte umgewandelt:

```text
active_level: TBD_HARDWARE
safe_boot_level: TBD_HARDWARE
external_bias: TBD_BOARD_CIRCUIT
```

Sie bleiben beschreibende Design-/Unbekanntwerte und sind kein aktueller
R1-Gate-PASS. Sie dürfen weder durch geratene Pegel ersetzt noch als
Laufzeitwert verwendet werden. Ein späterer funktionaler EIN/AUS- oder
Boot-/Resettest darf die beobachtete Funktion bestätigen, ohne diese
unmessbaren Designwerte rückwirkend zu erfinden. Alle übrigen GPIO-, Bus-,
Netz-, Pull- und Widerstandsfelder bleiben unverändert.

## 6. Exakte Statusfeld-Migrationsmatrix: `config/hardware.example.yaml`

Die Beispielkonfiguration ist kein zweites GPIO-SSOT. Ihre Hardwarestatuswerte
werden dennoch so benannt, dass kein alter Sammelstatus eine zweite Wahrheit
erzeugt.

| Feld | aktueller Wert | Entscheidung | exakter Zielzustand |
|---|---|---|---|
| `peltier.ris_lis_usage` | `verify_on_real_module` | `REPLACE_WITH` | `FUTURE_RELEASE`; R1 bleibt `R_IS_L_IS_R1=DISABLED`, ohne Anschluss, Messung oder Implementierung. |
| `hardware_release.state` | `ELECTRICAL_VERIFICATION_PENDING` | `REMOVE` | Vollständig in die getrennten Felder `electrical_level_measurement`, `ssot_conformance`, `functional_hardware_verification` und `adapter_safety_verification` zerlegen. |
| `hardware_release.real_hardware_present` | `true` | `KEEP` | Beschreibende Hardwarepräsenz, kein Funktions- oder Mess-PASS. |
| `hardware_release.board_family_reference_match` | `confirmed_by_owner` | `KEEP` | Owner-Referenzabgleich, kein elektrischer Funktionsnachweis. |
| `hardware_release.board_family` | `esp32_32e_quad_mosfet` | `KEEP` | Design-/Identitätsbeschreibung. |
| `hardware_release.board_profile` | `esp32_32e_quad_mosfet_r1` | `KEEP` | Verweis auf die einzige GPIO-SSOT. |
| `hardware_release.board_revision` | `TBD_HARDWARE` | `KEEP` | Unbekannte reale Revision; kein erfundener Wert. |
| `hardware_release.pins_confirmed` | `false` | `REMOVE` | Nicht als allgemeines Boolean-Gate weiterführen; Ziel ist `ssot_conformance: PENDING`. |
| `hardware_release.active_levels_confirmed` | `false` | `REMOVE` | Nicht als Pegelmess-Gate weiterführen; Ziel ist `functional_hardware_verification: PENDING`. |
| `hardware_release.boot_levels_confirmed` | `false` | `REMOVE` | Keine Bootpegelmesspflicht; funktionales Boot-/Resetverhalten bleibt im Funktionsscope. |
| `hardware_release.peltier_pulse_test_passed` | `false` | `REPLACE_WITH` | `real_peltier_test: NOT_RUN`; kein elektrischer oder Aktorfreigabe-PASS. |

Der resultierende `hardware_release`-Block lautet sinngemäß und ohne
zusätzlichen `state`-Sammelstatus:

```yaml
hardware_release:
  electrical_level_measurement: NOT_REQUIRED_WAIVED
  ssot_conformance: PENDING
  functional_hardware_verification: PENDING
  adapter_safety_verification: PENDING
  actuator_release: NO
  real_peltier_test: NOT_RUN
```

`THERMAL_COMMISSIONING` wird im Beispielblock nicht ergänzt, weil thermische
Inbetriebnahme ein späteres Commissioning-Gate und kein Hardware-/Adapterstatus
dieser Beispielsektion ist. `ELECTRICAL_MEASUREMENT_PASS` wird weder hier noch
an einer anderen aktuellen Stelle eingeführt.

## 7. R_IS/L_IS-R1-Entscheidung und Issue #33

### 7.1 R1-Vertrag

R_IS/L_IS werden für R1 vollständig aus dem Hardware-/Adapter- und
Akzeptanzscope herausgenommen:

```text
R_IS_L_IS_R1=DISABLED
R_IS_L_IS_LEVEL_MEASUREMENT_REQUIRED=NO
R_IS_L_IS=FUTURE_RELEASE
R_IS_L_IS_CONNECTED=NO
R_IS_L_IS_IMPLEMENTED=NO
```

Für R1 gilt ausdrücklich:

- nicht anschließen;
- nicht messen;
- nicht implementieren;
- kein Akzeptanzkriterium;
- kein Required Test;
- kein Definition-of-Done-Gate;
- keine Stromdiagnose und keine Freigabelogik daraus ableiten.

Eine spätere Verwendung benötigt ein eigenes Issue, einen eigenen vollständigen
Plan und ein eigenes Owner-Gate. Der spätere Scope ist ausschließlich
`FUTURE_RELEASE`; die vorliegende R1-Planrevision autorisiert keine spätere
Evaluation vorab.

### 7.2 Unveränderte Boardprofil-Safetywerte

Die folgenden vier Werte müssen byte-/wertgleich erhalten bleiben:

```text
gpio34.ris_lis_enabled=false
gpio34.raw_connection_allowed=false
gpio34.assignment_status=reserved_disabled

gpio35.ris_lis_enabled=false
gpio35.raw_connection_allowed=false
gpio35.assignment_status=reserved_disabled
```

Kein Statuscleanup darf diese Reserve-, Input-only- oder
`raw_connection_allowed=false`-Grenze lockern.

### 7.3 Exakter Issue-#33-Zielzustand

Der Titel von Issue #33 bleibt bestehen. Sein Statusblock wird auf folgenden
Zielzustand synchronisiert:

```text
ISSUE33=BLOCKED_HARDWARE
REAL_ESP_IDF_BTS7960_ADAPTER_EXISTS=NO
ELECTRICAL_LEVEL_MEASUREMENT=NOT_REQUIRED_WAIVED
SSOT_CONFORMANCE=PENDING
FUNCTIONAL_HARDWARE_VERIFICATION=PENDING
ADAPTER_SAFETY_VERIFICATION=PENDING
REAL_PELTIER_TEST=NOT_RUN
ACTUATOR_RELEASE=NO

HBRIDGE_MUTUAL_EXCLUSION_REQUIRED=YES
HBRIDGE_BREAK_BEFORE_MAKE_REQUIRED=YES
HBRIDGE_HARDWARE_ADAPTER_FAIL_CLOSED_REQUIRED=YES
HBRIDGE_BOOT_DEFAULT_DISABLED_REQUIRED=YES
MULTIMETER_REQUIRED_FOR_R1_ACCEPTANCE=NO

R_IS_L_IS_R1=DISABLED
R_IS_L_IS_LEVEL_MEASUREMENT_REQUIRED=NO
R_IS_L_IS=FUTURE_RELEASE
```

Aus dem aktuellen Body werden die reale R_IS/L_IS-Signalpegelbewertung, die
Aktivierung nach Bewertung, das entsprechende Akzeptanzkriterium, der
Required-Test und das implizite R1-DoD-Gate entfernt. Der Body beschreibt
stattdessen, dass R_IS/L_IS bis zu einem späteren eigenen Issue/Plan/Owner-Gate
nicht verbunden, nicht gemessen und nicht implementiert werden.

Die übrigen #33-Anforderungen bleiben vollständig erhalten: echter
BTS7960-Adapter, Mutual Exclusion, Break-before-make, fail-closed
Initialisierung/Fehlerpfad, Boot default disabled, abgesicherte reale
Richtungspulse, Sicherung, Lüfter, Sicherheitssensoren und begrenzte
Peltierprüfung. Die Multimeter-Waiverentscheidung entfernt nur das frühere
up-front-Spannungs-/Polaritätsmessgate.

## 8. Bekannte aktuelle normative Korrekturziele

Die folgenden Stellen werden nach Planfreigabe konkret geprüft und, wenn sie
aktuellen R1-Status oder aktuelle Akzeptanz formulieren, im selben PR
synchronisiert. Historische Abschnitte bleiben als solche erhalten.

### 8.1 `docs/HARDWARE.md`

1. Die Statusdefinition
   `board_fixed_pending_electrical_verification` wird durch die eindeutige
   funktionale Bezeichnung
   `board_fixed_pending_functional_verification` ersetzt. Die Definition darf
   keinen elektrischen Mess-PASS oder Aktorfreigabe nahelegen.
2. `active_levels_confirmed`, `boot_levels_confirmed` und gleichartige
   generische Booleans werden aus aktueller Gate-/Statussprache entfernt oder
   als historische/superseded Evidenz gekennzeichnet. Sie bleiben kein
   notwendiger elektrischer Pegelnachweis.
3. Die R_IS/L_IS-Passage wird für R1 zu
   `disabled / not connected / FUTURE_RELEASE` geändert; keine Messung,
   Implementierung, Akzeptanz oder DoD-Pflicht in R1.
4. Die OneWire-Passage darf keine Spannungs- oder Pegelmesspflicht erzeugen.
   Sie fordert stattdessen funktionale Buskommunikation, ROM-Erkennung, CRC,
   Hot-Plug und Fehlerreaktion sowie die SSOT-/Verdrahtungskonformität der
   Pull-up-Anordnung. Thermische beziehungsweise Ressourcenmessungen bleiben
   davon getrennt.
5. Die Display-/Touch-Passage wird auf
   SSOT-/Verdrahtungskonformität, Controlleridentität, SPI-Funktion,
   CS-/Reset-/Backlight-Funktion, Touch/IRQ, Wake, Kalibrierung, Recovery,
   Fehlerisolation und Ressourcen-/Lizenznachweis ausgerichtet. Es gibt kein
   generelles Pegelmessgate.
6. Gemeinsame Masse, kein unerlaubtes Gegentreiben und zulässige
   Logic-Domain-Kompatibilität bleiben Design-/Modul-/Funktionsanforderungen.
   Sie werden nicht automatisch in ein Multimeter- oder GPIO-Pegelmessgate
   umgedeutet.
7. Aktuelle Vorkommen von `ELECTRICAL_VERIFICATION_PENDING` oder einer
   generischen offenen elektrischen Abnahme werden in die getrennte Matrix
   überführt. Historische Protokolle bleiben als historische Evidenz markiert.

### 8.2 Issue #31

Die aktuellen Legacyformulierungen
`reale elektrische und funktionale Vereinbarkeit`, `reale elektrische
Bestätigung` und `keine unbestätigten ... Pegel` werden nicht pauschal durch
einen Mess-PASS ersetzt. Sie werden durch die konkreten Nachweise des #31-
Scopes ersetzt:

```text
SSOT-/VERDRAHTUNGSKONFORMITÄT
TATSÄCHLICHER DISPLAYCONTROLLER
TATSÄCHLICHER TOUCHCONTROLLER
SPI-FUNKTION
CS-/RESET-/BACKLIGHT-FUNKTION
ROTATION
TOUCH/IRQ
WAKE
KALIBRIERUNG
RECOVERY
FEHLERISOLATION
RESSOURCEN-/LIZENZNACHWEIS
```

`MULTIMETER_REQUIRED=NO`, `BOOT_LEVEL_MEASUREMENT_REQUIRED=NO` und
`GPIO_VOLTAGE_MEASUREMENT_REQUIRED=NO` werden als #31-Rahmen ausdrücklich
beibehalten. Die Aussage, dass unbestätigte Controller-/Verdrahtungsannahmen
nicht als bestätigt gelten, bleibt inhaltlich erhalten, wird aber nicht als
generelle Spannungsmesspflicht formuliert.

### 8.3 Weitere aktuelle Quellen

Die aktive Hardware-/Akzeptanzsprache in `docs/ACCEPTANCE_TESTS.md`,
`docs/OPEN_POINTS.md`, `docs/REQUIREMENTS.md`,
`docs/DIAGNOSTICS_AND_MAINTENANCE.md`, `docs/SAFETY_COMPONENT_FAULTS.md` und
den relevanten Auditdateien wird gegen die Suchmatrix aus Abschnitt 11
bewertet. Insbesondere werden aktuelle R_IS/L_IS-Evaluations- oder
Testpflichten auf `FUTURE_RELEASE` außerhalb von R1 umgestellt. Reale
thermische, Ressourcen-, Sensor- und Kalibriermessungen bleiben zulässige
nicht-GPIO-Pegelnachweise und werden nicht entfernt.

## 9. Roadmap- und Live-Issue-Synchronisierung

### 9.1 Roadmap

In `docs/ROADMAP.md` werden die aktuellen #29- und #130-Zeilen nicht mehr als
aktive nächste Gates geführt. Die abgeschlossene #29-Grundlage erhält kompakt
exakt:

```text
ISSUE29=CLOSED_COMPLETED
PR129=MERGED
PR129_MERGE_COMMIT=1fc22d693bae8572144bf61d242a2fe6d0b093bc
OWNER_FINAL_REVIEW=PASS
CI_RUN=996
CI_RESULT=PASS
```

Die #130-Provenienz bleibt als abgeschlossenes SSOT-Ergebnis mit
`PR131=MERGED`, Merge-SHA `1fd8f6af53d1b3c23f3aa46c73c4fc3da7513d6d`,
`GPIO_MATRIX=PLANNED_NOT_CONFIRMED` und `ACTUATOR_RELEASE=NO` erhalten. Der
alte aktuelle Wert `ELECTRICAL_VERIFICATION=PENDING` wird durch
`ELECTRICAL_LEVEL_MEASUREMENT=NOT_REQUIRED_WAIVED` und, soweit der
SSOT-/Funktionsstatus für diese Referenz geführt wird, die getrennten
`SSOT_CONFORMANCE`-/`FUNCTIONAL_HARDWARE_VERIFICATION`-Felder ersetzt.

Die erste aktive fachliche Priorität ist #25 und die anschließende Richtung
bleibt exakt `#25 -> #26 -> #31 -> #30 -> #32 -> #33`. Keine neue
Priorisierung, kein Rückkopieren vollständiger Issue-Anforderungen und keine
Roadmap-Aussage einer elektrischen R1-Messpflicht.

### 9.2 Live-Issue-Bodies

- **#29:** `ISSUE29=CLOSED_COMPLETED`, `PR129=MERGED`, Merge-SHA, Owner-
  Review- und CI-Nachweise aktualisieren; `ELECTRICAL_LEVEL_MEASUREMENT=
  NOT_REQUIRED_WAIVED` verwenden; `ELECTRICAL_VERIFICATION=PENDING` und
  `ELECTRICAL_MEASUREMENT_PASS=NOT_CLAIMED` nicht als aktuelle Statusfelder
  weiterführen. Historische `LEVEL_MEASUREMENTS=NOT_RUN...`-Evidenz bleibt
  ausdrücklich historisch beziehungsweise waived.
- **#31:** Status- und Prosatext sowie die oben definierten SSOT-/Funktions-
  nachweise synchronisieren; keine vorgeschriebene elektrische Messung.
- **#32:** exakt `ELECTRICAL_LEVEL_MEASUREMENT=NOT_REQUIRED_WAIVED`,
  `SSOT_CONFORMANCE=PENDING`, `FUNCTIONAL_HARDWARE_VERIFICATION=PENDING`,
  `ACTUATOR_RELEASE=NO`; die funktionalen Kanal-, Boot-/Reset-, Nachlauf- und
  Adapteranforderungen bleiben vollständig erhalten.
- **#33:** den vollständigen Statusblock aus Abschnitt 7.3 einsetzen und alle
  R_IS/L_IS-R1-Akzeptanz-/Test-/DoD-Aussagen entfernen; alle H-Bridge-
  Sicherheitsanforderungen bleiben bestehen.
- **#130:** Live-Zustand `CLOSED`, PR-#131-Provenienz, SSOT-Pfad, geplante
  statt bestätigter Hardwareverifikation und `ACTUATOR_RELEASE=NO`
  synchronisieren; kein aktueller generischer elektrischer Sammelstatus.

Technische Abschlussnachweise werden nicht neu interpretiert. Nur ihre
aktuelle Statusdarstellung wird gegen den neuen Vertrag synchronisiert.

## 10. Konsumenten-, Schema- und Kompatibilitätsgrenze

Die folgenden Grenzen sind ausdrücklich getrennt:

```text
PRODUCTION_RUNTIME_SCHEMA_CHANGE=NO
WIRE_FORMAT_CHANGE=NO
PERSISTENCE_SCHEMA_CHANGE=NO
HARDWARE_STATUS_METADATA_SHAPE_CLEANUP=ALLOWED
```

`HARDWARE_STATUS_METADATA_SHAPE_CLEANUP=ALLOWED` gilt nur, wenn die
Konsumentenprüfung bestätigt, dass die betroffenen YAML-/Dokumentations-
Statusmetadaten nicht von Produktionscode, Tests oder einem verbindlichen
Generatorvertrag maschinell konsumiert werden.

Vor jeder Statusänderung wird gesucht nach:

```text
assignment_status
verification_state
electrical_verification
electrical_level_measurement
functional_hardware_verification
ssot_conformance
adapter_safety_verification
thermal_commissioning
hardware_release.state
hardware_release.pins_confirmed
```

Wird ein echter maschineller Konsument, ein Schema-Validator, ein Generator-
Vertrag, eine Testoracle oder eine Produktionsreferenz gefunden, gilt sofort:

```text
STOP
PLAN_REVISION_REQUIRED
IMPLEMENTATION_NOT_ALLOWED
```

Es erfolgt keine stille Kompatibilitätsänderung und keine Produktlogik-
Anpassung innerhalb dieses PRs. Die bisherige Baseline-Suche hat keine
Produktionscode-, Test- oder Generatorauswertung dieser Statusfelder gezeigt;
dieser Befund wird nach Planfreigabe mit der finalen Such- und Konsumenten-
prüfung erneut verifiziert.

## 11. Deterministische repositoryweite Such- und Abschlussmatrix

Nach der späteren Umsetzung wird mindestens mit `rg -n -I` im gesamten
Repository gesucht, ausgenommen nur generierte/veraltete Buildartefakte, die
bereits durch die Repositoryregeln ausgeschlossen sind. Die Suchliste ist
vollständig:

```text
ELECTRICAL_VERIFICATION
ELECTRICAL_VERIFICATION_PENDING
electrical_verification
pending_electrical_verification
board_fixed_pending_electrical_verification
active_levels_confirmed
boot_levels_confirmed
mosfet_electrical_behavior_confirmed
display_touch_electrical_function_confirmed
elektrische Bestätigung
elektrische Verifikation
Bootpegel
Boot-/Resetpegel
Pegelmessung
Multimeter
ris_lis_usage
R_IS
L_IS
```

Zusätzlich wird nach `ELECTRICAL_MEASUREMENT_PASS`,
`DESIGN_SSOT_VERIFICATION`, `SOFTWARE_ADAPTER_SAFETY_VERIFICATION` und
`peltier_pulse_test_passed` gesucht, damit keine Parallelbenennung oder
scheinbarer Mess-PASS übersehen wird.

Jeder Treffer erhält im PR-Nachweis genau eine Klassifikation:

```text
CURRENT_NORMATIVE_MUST_FIX
HISTORICAL_SUPERSEDED_OK
OPTIONAL_NON_GATING_OK
UNRELATED_MEASUREMENT_OK
```

Bewertungsregeln:

- `CURRENT_NORMATIVE_MUST_FIX`: aktiver Status, aktuelle Abnahme, aktuelle
  Roadmap, Issue-Body, Boardprofil oder Hardware-/Safetyvertrag; muss im PR
  geändert werden.
- `HISTORICAL_SUPERSEDED_OK`: abgeschlossene Evidenz mit eindeutigem
  historischem, superseded oder legacy Kontext; darf erhalten bleiben und
  wird nicht rückwirkend umgedeutet.
- `OPTIONAL_NON_GATING_OK`: optionale technische Messung oder zukünftige
  Analyse ohne R1-Gate; der Text muss ausdrücklich nicht-gating beziehungsweise
  `FUTURE_RELEASE` sagen.
- `UNRELATED_MEASUREMENT_OK`: thermische Messung, Ressourcenmessung,
  Sensor-/Touchkalibrierung oder andere fachlich notwendige Messung, die keine
  generelle GPIO-/Bootpegelpflicht behauptet.

Der Abschlussstatus `REPOSITORY_WIDE_STALE_GATE_SEARCH=PASS` ist nur zulässig,
wenn alle Treffer klassifiziert sind, kein ungeklärter Treffer verbleibt,
kein aktueller normativer Legacy-Treffer mehr vorhanden ist und keine
parallele Namenswahrheit erzeugt wurde. Andernfalls lautet der Status
`FAILED` oder – bei einem neuen maschinellen Konsumenten beziehungsweise
materieller Abweichung – `BLOCKED` mit Planrevision; ein pauschales Search
PASS ist unzulässig.

## 12. Umsetzungsschnitte nach Owner-Planfreigabe

1. Voränderungsinventar der Statusfelder, aller maschinellen Konsumenten und
   der Boardprofil-GPIO-/Pull-/Netz-/Designwerte erstellen. Bei Konsumenten-
   fund STOP nach Abschnitt 10.
2. Roadmap und aktuelle kanonische Dokumente gemäß Abschnitten 8 und 9
   synchronisieren. Historische #29-Berichte nur an aktuellen Statusblöcken
   korrigieren; historische Nachweise nicht verfälschen.
3. Boardprofil exakt gemäß Abschnitt 5 und Beispielkonfiguration exakt gemäß
   Abschnitt 6 ändern. Nur die dort genannten Statusfelder dürfen sich ändern.
4. Issue #29, #31, #32, #33 und #130 gemäß Abschnitt 9 synchronisieren;
   Issue #33 zusätzlich vollständig vom R_IS/L_IS-R1-Scope bereinigen.
5. Strukturierte Vorher-/Nachher-Prüfung des Boardprofils ausführen. Jede
   Abweichung außerhalb der erlaubten Statusfelder ist ein Befund und stoppt
   die Umsetzung.
6. Die Suchmatrix aus Abschnitt 11 vollständig ausführen und alle Treffer im
   PR-Nachweis klassifizieren. Ungeklärte aktuelle normative Treffer stoppen
   den PR.
7. Gezielte Syntax-/Dokumentationsprüfungen ausführen, den vollständigen
   aktuellen Diff gegen diese Planrevision reviewen und den PR-Body mit
   Implementations-HEAD und Nachweisen aktualisieren.

## 13. Gezielte Nachweise und Ergebnisstatus

Zulässige Draft-Umsetzungsnachweise:

```text
git diff --check
repository-wide status search and classification
YAML/configuration syntax parse for changed YAML files
directly affected schema/generator/documentation checks, if a consumer exists
structured board-profile comparison: GPIO/PULL/NET/DESIGN_VALUES_UNCHANGED
```

Die konkrete Ausführung wird mit Befehl, Umfang und Ergebnis im PR
dokumentiert. `PASS` bedeutet ausgeführt und erfolgreich; `FAILED` bedeutet
ausgeführt und fehlgeschlagen; `BLOCKED` bedeutet wegen einer konkret
benannten Voraussetzung nicht ausführbar; `NOT_RUN` bleibt nicht ausgeführt.

Ausdrücklich nicht auszuführen:

```text
FIRMWARE_BUILD=NOT_RUN
ESP_IDF_TESTS=NOT_RUN
HARDWARE_TEST=NOT_RUN
NEW_HARDWARE_MEASUREMENT=NOT_RUN
```

Thermische Commissioningtests, Ressourcenmessungen, Sensor-/Touchkalibrierung
und sonstige fachliche Messungen bleiben außerhalb des Waivers und außerhalb
dieses PRs; sie werden weder als ausgeführt behauptet noch entfernt.

## 14. Verbindlicher finaler Implementation-Handover

Nach späterer Planfreigabe, Umsetzung und lokalem Review wird genau ein
aktueller `SESSION HANDOVER`-Kommentar im offenen PR #133 erstellt. Die
Platzhalter werden mit dem tatsächlichen Implementations-HEAD ersetzt; der
Handover muss mindestens exakt diese Semantik enthalten:

```text
CLEANUP_PR=133
CLEANUP_PR_STATE=OPEN_DRAFT
BASE=integration/r1-development
HEAD=<implementation-head>

ISSUE29_ROADMAP_SYNC=PASS
ISSUE31_MEASUREMENT_SEMANTICS=PASS
ISSUE32_STATUS_MODEL=PASS
ISSUE33_STATUS_MODEL=PASS
R_IS_L_IS_R1=DISABLED
BOARD_PROFILE_STATUS_MODEL=PASS
GPIO_ASSIGNMENT_CHANGED=NO
PULL_CONFIGURATION_CHANGED=NO

ELECTRICAL_LEVEL_MEASUREMENT=NOT_REQUIRED_WAIVED
REPOSITORY_WIDE_STALE_GATE_SEARCH=PASS

FEATURE_IMPLEMENTATION=NO
HARDWARE_TEST=NOT_RUN
ACTUATOR_RELEASE=NO
ISSUE25_STARTED=NO

INTEGRATION_TO_MAIN_PR_CREATED=NO
MERGE=NO
OWNER_FINAL_REVIEW_REQUIRED=YES
```

Der Handover nennt zusätzlich Plan-SHA, Basis-SHA, geänderte Dateien,
ausgeführte gezielte Prüfungen, offene Befunde und den nächsten konkreten
Schritt. Danach STOP. Der Agent setzt den PR nicht auf Ready, mergt nicht,
schließt kein Issue und erstellt keinen Integrations-PR nach `main`.

## 15. Exaktes Owner-Plan-Gate

Diese Planrevision wird nach Commit und Push im bestehenden Draft-PR #133
ausgewiesen:

```text
APPROVED_PLAN_SHA=PENDING
OWNER_PLAN_REVIEW_REQUIRED=YES
IMPLEMENTATION_ALLOWED=NO
PR133_STATE=OPEN_DRAFT
```

Keine Implementation, keine Live-Issue-Umsetzung, keine Hardwaretests, kein
Ready-for-review, kein Merge und kein Integrations-PR nach `main`, bevor der
Owner genau die neue Plan-Commit-SHA ausdrücklich freigegeben hat.
