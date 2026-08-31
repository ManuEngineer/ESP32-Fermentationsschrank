# Issue #130 – R1-GPIO-Zielmatrix und Verdrahtung kanonisieren

## Status und Owner-Gate

Diese Datei ist der vollständige, eigenständige Plan für Issue #130. Sie
beschreibt die vom Owner vorgegebene R1-Zielentscheidung und die spätere
Dokumentationsumsetzung. Sie ist keine Bestätigung einer real gemessenen
Verdrahtung.

Bis zur ausdrücklichen Freigabe der exakten Commit-SHA dieser Datei gilt:

```text
PLAN_STATUS=PLAN_ONLY
IMPLEMENTATION=NOT_STARTED
GPIO_MATRIX=PLANNED_NOT_CONFIRMED
HARDWARE_RUN=NOT_RUN
OWNER_PLAN_REVIEW_REQUIRED=YES
MERGE=NO
```

Die Plan-SHA ist die Commit-SHA, die diese vollständige Datei zusammen mit der
minimalen Roadmap-Synchronisierung enthält. Sie wird nach dem Commit im
Draft-PR #131 und im aktuellen `SESSION HANDOVER` eingetragen. Eine allgemeine
Zustimmung zu Issue, PR oder Chattext ersetzt die Freigabe dieser exakten SHA
nicht.

## 1. Verifizierte Live-Basis

Die Live-Prüfung vor Erstellung dieses Plans ergab:

| Quelle | Live-Stand | Bedeutung für Issue #130 |
|---|---|---|
| Repository | `ManuEngineer/ESP32-Fermentationsschrank` | Zielrepository dieses PR |
| Basisbranch | `integration/r1-development @ c1f5fbb5f19ab8e7d2c25708fe79777d523217d4` | verifizierte Branchbasis |
| Arbeitsbranch | `agent/issue-130-r1-gpio-matrix` | eigener Branch für Issue #130 |
| Issue #130 | `OPEN`, `[E5.0] R1-GPIO-Zielmatrix und Verdrahtungsbasis kanonisieren` | eigener Hardware-/Dokumentationsscope |
| PR #131 | `OPEN`, `DRAFT`, gegen `integration/r1-development` | Owner-Gate für diesen Plan |
| PR #129 | `OPEN`, `DRAFT`, Head `92f6cf6c3ee16868b95b7ee6a4e9b233d9ffb0c6` | separater Issue-#29-Panic-/Stack-Scope, nicht Bestandteil dieses Plans |
| Aktueller Branch-Head vor Plan | `85edbb06de298ab4e7ecc307e39de36be277f2c3` (`chore(issue130): initialize draft branch`) | leerer Initialisierungscommit zum Eröffnen des Draft-PR; noch keine Matrixänderung |
| Roadmap | Stand `2026-08-31`, Integration ebenfalls auf `c1f5fbb5…` | wird nur um den neuen Plan-/PR-Status ergänzt |
| Freigegebener Plan dieses PR | `NONE` vor diesem Commit | Ownerfreigabe bleibt offen |

```text
CONTEXT_BASELINE_BRANCH: integration/r1-development
CONTEXT_BASELINE_SHA: c1f5fbb5f19ab8e7d2c25708fe79777d523217d4
CONTEXT_HEAD_SHA: 85edbb06de298ab4e7ecc307e39de36be277f2c3
CONTEXT_PLAN_SHA: assigned by the commit containing this file
CONTEXT_REFRESH_MODE: FULL
CONTEXT_DELTA: Live-Issue/PR/branch/base check; Roadmap; root rules; hardware,
  configuration, open-point, safety, actuator, acceptance, ADR and CI sources;
  local archived component references
SOURCE_OF_TRUTH_CONFLICT: EXISTING_HARDWARE_DOCUMENTS_NOT_YET_ALIGNED_WITH_ISSUE130_TARGET;
  existing TBD/preference text is not treated as confirmation or as a second
  accepted design
```

PR #129 bleibt in allen Fällen getrennt. Wenn #129 vor der Umsetzung dieses
Plans in `integration/r1-development` gemergt wird, muss der aktuelle
Integration-Stand ohne Rebase oder Force-Push in diesen Branch übernommen, die
Roadmap neu gegen den Live-Stand geprüft und historische #129-Statuszeilen
erhalten werden. Dieser Plan übernimmt keine #129-Root-Cause- oder
Stackentscheidung.

## 2. Ziel und Nicht-Ziele

### Ziel

Die akzeptierte Release-1-GPIO- und Verdrahtungs-Zielmatrix wird in den bereits
kanonischen Quellen abgebildet:

- `config/pins.example.yaml` als strukturierte Pin-/Buszuordnung;
- `config/hardware.example.yaml` als komponentenbezogene R1-Beispielkonfiguration;
- `docs/HARDWARE.md` als lesbare Hardware- und Verifikationsquelle;
- `docs/ROADMAP.md` nur mit einer knappen Status-/PR-Synchronisierung;
- `references/LINKS.md` nur, falls für tatsächlich verwendete fehlende
  Herstellerquellen erforderlich.

Die Matrix beschreibt eine akzeptierte Designzuordnung. Jeder Eintrag bleibt
explizit `planned`, `reserved`, `forbidden`, `free` oder
`board-fixed / pending electrical verification`; keiner wird durch diesen PR
zu `confirmed_test`.

### Nicht-Ziele

- keine Änderung an Produktionscode, Testcode, Ports, Adaptern, Bibliotheken,
  Toolchain, Buildprofilen, Wireformaten oder Persistenz;
- keine Aktorfreigabe, kein Firmwareflash, kein UART-/GPIO-/Pegeltest und kein
  sonstiger Hardwarelauf;
- keine Aufhebung von `HARDWARE_UNVERIFIED`, `real_actuators_enabled: false`
  oder der bestehenden Release-/Bring-up-Gates;
- keine Schließung oder fälschliche Bestätigung der Gates #29, #30, #31, #32
  oder #33;
- keine Änderung des gemergten DS3231SN-Softwarevertrags aus #126;
- keine Auswahl eines DS3231M als produktiv unterstützte Softwarevariante;
- kein zweites `GPIO_MATRIX_R1.md` und keine neue parallele Hardwarequelle;
- keine Änderung, Übernahme oder Vereinigung von PR #129 / Issue #29;
- kein `Ready for review`, kein Merge, kein Auto-Merge und kein Issue-Schluss
  durch den Agenten.

## 3. Verbindliche Zielmatrix

Die folgende Tabelle ist die Designentscheidung dieses Auftrags. Sie wird in
der späteren Umsetzung semantisch unverändert in `pins.example.yaml` und
`docs/HARDWARE.md` abgebildet. `planned` bedeutet eine akzeptierte
Zielzuordnung ohne vollständige reale Messung. `board-fixed` bedeutet nur, dass
die Zuordnung aus der PCB-Struktur stammt; Pegel, Bootwirkung und elektrische
Sicherheit bleiben offen.

| Pin | R1-Rolle | Verdrahtung / Regel | Zielstatus |
|---|---|---|---|
| `EN / CHIP_PU` | ESP32-Hardware-Reset + Quelle für TFT-Reset | TFT `RESET` folgt `EN` nur über einseitige, low-aktive Entkopplung; kein harter bidirektionaler Netztie | `planned` |
| `GPIO0` | BOOT / ROM-Download | keine R1-Peripherie anschließen | `reserved` |
| `GPIO1` | UART0 TX | ESP32 TX -> FT232RL RX | `planned` |
| `GPIO2` | TFT D/C | MSP2807 `DC/RS`; keine externe Beschaltung, die GPIO2 beim Reset HIGH erzwingt; schwacher Pulldown zulässig | `planned` |
| `GPIO3` | UART0 RX | FT232RL TX -> ESP32 RX; 3,3-V-I/O | `planned` |
| `GPIO4` | TFT-Backlight PWM | MSP2807 `LED`, active-high; externer Pulldown für Boot/Reset-AUS; ermöglicht die verbindliche R1-Helligkeits-/Dimmfunktion | `planned` |
| `GPIO5` | TFT CS | MSP2807 `CS`; externer ca. 10-kΩ Pull-up nach 3,3 V | `planned` |
| `GPIO6..11` | SPI-Flash | nicht verwenden | `forbidden` |
| `GPIO12` | VDD_SDIO-Strap | keine R1-Funktion | `forbidden` |
| `GPIO13` | BTS7960 RPWM | externer ca. 10-kΩ Pulldown | `planned` |
| `GPIO14` | BTS7960 LPWM | externer ca. 10-kΩ Pulldown | `planned` |
| `GPIO15` | Touch CS | XPT2046/MSP2807 `T_CS`; externer ca. 10-kΩ Pull-up nach 3,3 V | `planned` |
| `GPIO16` | Onboard-MOSFET 1 | Innenlüfter; PCB-seitig fest verdrahteter MOSFET-Kanal | `board-fixed / pending electrical verification` |
| `GPIO17` | Onboard-MOSFET 2 | Außen-/Kühlkörperlüfter; PCB-seitig fest verdrahteter MOSFET-Kanal | `board-fixed / pending electrical verification` |
| `GPIO18` | SPI SCK | gemeinsam TFT `SCK` + Touch `T_CLK` | `planned` |
| `GPIO19` | SPI MISO | gemeinsam TFT `SDO/MISO` + Touch `T_DO` | `planned` |
| `GPIO21` | I2C SDA | DS3231-Familie SDA | `planned` |
| `GPIO22` | I2C SCL | DS3231-Familie SCL | `planned` |
| `GPIO23` | SPI MOSI | gemeinsam TFT `SDI/MOSI` + Touch `T_DIN` | `planned` |
| `GPIO25` | H-Brücken Enable | `R_EN` + `L_EN` gemeinsam; externer ca. 10-kΩ Pulldown; zentrale fail-low Freigabe | `planned` |
| `GPIO26` | Onboard-MOSFET 3 | aktiver Summer; PCB-seitig fest verdrahteter MOSFET-Kanal | `board-fixed / pending electrical verification` |
| `GPIO27` | Onboard-MOSFET 4 | Reserve; PCB-seitig fest verdrahteter MOSFET-Kanal | `board-fixed / pending electrical verification` |
| `GPIO32` | interner 1-Wire-Bus | DS18B20 Schrankluft **und** DS18B20 Kühlkörper gemeinsam; 4,7-kΩ Pull-up nach 3,3 V; 3-Leiter-Betrieb; Rollen über ROM-ID | `planned` |
| `GPIO33` | externer 1-Wire-Bus | abnehmbarer DS18B20 Produktfühler; eigener 4,7-kΩ Pull-up nach 3,3 V; 3-Leiter-Betrieb | `planned` |
| `GPIO34` | BTS7960 R_IS Reserve | ADC1_CH6, input-only; R_IS **nicht roh anschließen**; nur nach realer Pegelmessung und geeigneter Schutz-/Teilerbeschaltung | `reserved / disabled` |
| `GPIO35` | BTS7960 L_IS Reserve | ADC1_CH7, input-only; L_IS **nicht roh anschließen**; nur nach realer Pegelmessung und geeigneter Schutz-/Teilerbeschaltung | `reserved / disabled` |
| `GPIO36` | frei / input-only | keine R1-Zuordnung; DS3231 `INT/SQW` bleibt R1 unbenutzt | `free` |
| `GPIO39` | Touch IRQ | MSP2807/XPT2046 `T_IRQ`; input-only; standardmäßig als Pegel pollen, realen Modul-Pull-up verifizieren | `planned` |

GPIO20, GPIO24, GPIO28..31 und GPIO37..38 werden in der Umsetzung im
vollständigen ESP32-WROOM-32E-/Board-Inventar mitgeprüft. Für nicht bonded oder
nicht herausgeführte Pins wird keine R1-Rolle erfunden. Die Prüfliste darf die
Abwesenheit dieser nicht gelisteten Nummern nicht als frei verfügbare
Hardwarekapazität umdeuten.

## 4. Status- und Sicherheitsmodell

| Status | Bedeutung in diesem PR |
|---|---|
| `planned` | Designzuordnung akzeptiert, am realen Aufbau noch nicht vollständig gemessen |
| `board-fixed / pending electrical verification` | PCB-seitig fest verdrahtete Zuordnung; elektrische Pegel, Bootwirkung und Verbraucherwirkung noch nicht verifiziert |
| `confirmed_test` | nur nach realer Kontinuität, Pegel-, Boot-/Reset- und Funktionsmessung am konkreten Aufbau zulässig; wird durch diesen PR nirgendwo gesetzt |

`reserved`, `forbidden`, `free` und `reserved / disabled` sind
Ressourcen-/Nutzungsstatus, kein Ersatz für Messnachweis. Ein reservierter
oder freier Pin darf nicht als implizite zukünftige Aktorfreigabe erscheinen.

Unverändert bleiben `HARDWARE_UNVERIFIED`,
`pins_confirmed: false`, `active_levels_confirmed: false`,
`boot_levels_confirmed: false`, `real_actuators_enabled: false`,
`requires_verified_hardware_profile: true` und
`peltier_pulse_test_passed: false`. Die Dokumentation erzeugt keine
`ActuatorSafetyGateStatus::Allowed`-Bedingung. Die bestehenden SafetyCore-,
Planner- und Sink-Verträge bleiben bei Boot, Reset, Fehler, unbekanntem Zustand,
unbestätigter Hardware und offenem Safety-Gate all-off beziehungsweise
fail-closed.

## 5. Quellen und fachliche Grenzen

Der Plan verwendet in dieser Reihenfolge:

- `AGENTS.md`, `docs/AGENT_WORKFLOW.md` und `docs/ENGINEERING_PRINCIPLES.md`;
- `docs/SPECIFICATION_REVIEW.md`, `docs/DECISIONS.md`, insbesondere
  ADR-002, ADR-008, ADR-011, ADR-012 und ADR-013;
- `docs/HARDWARE.md`, `docs/HARDWARE_REVISIONS.md`, `docs/OPEN_POINTS.md`,
  `docs/ACTUATOR_TIMING.md`, `docs/SAFETY_COMPONENT_FAULTS.md`,
  `docs/ACCEPTANCE_TESTS.md` und `docs/RUNTIME_BEHAVIOR.md`;
- Live-Issues #29, #30, #31, #32 und #33, Issue #126/PR #127 sowie
  `docs/CI_AND_QUALITY_GATES.md`;
- lokale Archive `references/datasheets/Display/MSP2807-2.8-SPI.pdf`,
  `2.8inch_SPI_Module_MSP2807_User_Manual_EN.pdf`, `ILI9341 Datasheet.pdf`,
  `esp32-32e-quad-mosfet-board-supplier.pdf`, `bts7960-infineon.pdf`,
  `ds18b20.pdf` und `ft232rl-adapter-supplier.pdf`.

Die bestehenden Hardwaredokumente enthalten noch `TBD_HARDWARE`, die frühere
Präferenz „ein GPIO je Sensor“ und offene Display-/MOSFET-/BTS7960-Pegel. Das
ist die auszugleichende Dokumentationsausgangslage, keine reale Bestätigung und
keine zweite akzeptierte Matrix. Hersteller-URLs werden nur ergänzt, wenn ein
verwendeter Nachweis in `references/LINKS.md` fehlt.

Der #126-Softwarevertrag bleibt exakt getrennt:

```text
physical_family=DS3231
delivered_variant=TBD_DELIVERY
software_supported_variant=DS3231SN
```

Die Matrix ist für die Familie elektrisch variantenneutral:

```text
3.3 V -> VCC/+
GND   -> GND/-
GPIO21 <-> SDA/D
GPIO22  -> SCL/C
INT/SQW -> R1 unbenutzt
```

Eine gelieferte DS3231M-Variante löst einen eigenen Register-/Software-
Kompatibilitätsscope aus; dieser PR ändert weder `DS3231SN`, `ITimeSource`,
OSF-/EOSC-Vertrauen noch den #124-Recoveryvertrag.

## 6. Spätere Dokumentationsumsetzung nach Planfreigabe

### 6.1 Pin- und Hardware-YAML

`config/pins.example.yaml` bildet die vollständige Tabelle als eindeutige
R1-Zielbeschreibung ab. Die beiden festen Sensorrollen referenzieren denselben
Bus `fixed_internal` auf GPIO32 mit genau einem 4,7-kΩ-Pull-up; der
Produktbus `product_external` auf GPIO33 erhält einen eigenen Pull-up. Die
BTS7960-Rollen zeigen GPIO13/14/25, wobei `R_EN` und `L_EN` bewusst gemeinsam
auf GPIO25 liegen. GPIO34/35 werden nicht aktiviert.

Display/Touch referenzieren den gemeinsamen SPI-Bus GPIO18/19/23, CS/DC
GPIO5/2, Touch-CS/IRQ GPIO15/39 und Backlight GPIO4. Pull-ups, Pulldowns,
GPIO2-Strapregel und ein benannter Resetpfad von `EN / CHIP_PU` werden als
Verdrahtungsregeln festgehalten. TFT-RESET erhält keinen frei programmierbaren
GPIO.

Die vier PCB-festen MOSFET-Kanäle werden GPIO16/17/26/27 zugeordnet. Aktive
Pegel, Verbraucherwerte und Bootwirkung bleiben elektrische
Verifikationsfelder. Optionale Zukunftsressourcen wie 12-V-ADC, Türkontakt und
Lüfter-Tacho werden nicht vorgezogen.

`config/hardware.example.yaml` wird dazu ohne Sicherheitslockerung
abgeglichen: `fixed_internal`/`product_external`, UI-Signale, BTS7960,
MOSFET-Rollen, UART/FT232RL und die getrennten DS3231-Felder werden konsistent
referenziert. `preferred_bus_topology` darf nicht als konkurrierende R1-
Zielentscheidung stehen bleiben. Profile, Hardwarezustände und
`real_actuators_enabled: false` bleiben unverändert.

### 6.2 `docs/HARDWARE.md`

Die Datei erhält die vollständige Matrix, das Statusmodell, die Begründung für
GPIO4 als Backlight-PWM und die konkrete Zwei-Bus-Topologie. Sie dokumentiert
gemeinsame SPI-Signale, Boot-/Strapbedingungen, DS3231-Familienneutralität,
BTS7960-Safe-Off, FT232-Richtung und die Verifikationsreihenfolge. Kein
Abschnitt bezeichnet einen Zielstatus als `confirmed_test`.

### 6.3 TFT-RESET-Gate

Die Zielschaltung ist kein harter Netztie:

```text
ESP32 EN / CHIP_PU
        |
        +--> einseitige, low-aktive Entkopplung --> MSP2807 TFT_RESET
                                                     |
                                                     +--> definierte 3,3-V-HIGH-Vorspannung
```

Die konkrete Bauteilwahl wird nur festgeschrieben, wenn lokale
MSP2807-/ILI9341-Unterlagen und Espressif-EN-/Reset-Unterlagen eindeutig
belegen, dass `TFT_RESET` ein Displayeingang ist, ein Low auf EN zuverlässig
Low am Display erzeugt, EN bei Freigabe High bleibt, die Displayseite EN nicht
zurücktreibt und Pegel sowie Resetzeiten passen. Eine nichtinvertierende
Open-Drain-/Open-Collector- oder gleichwertig einseitige low-aktive Lösung ist
zulässig. Ohne eindeutigen Nachweis bleiben Bauteilwahl und konkrete
Schaltungsdimensionierung `pending_hardware_verification`; es wird weder
geraten noch `EN == TFT_RESET` eingetragen.

### 6.4 Boot, Input-only, BTS7960 und FT232

Die Umsetzungsprüfung gleicht alle GPIOs 0..39 gegen ESP32-WROOM-32E-
Pinbeschreibung und Boardunterlage ab: GPIO0 bleibt Download-Strap, GPIO2
wird nicht gegen HIGH erzwungen, GPIO5/15 bleiben deselected, GPIO12 bleibt
ohne R1-Funktion, GPIO34/35/36/39 werden nur input-only-konform verwendet und
GPIO6..11 bleiben Flash-reserviert. GPIO25, GPIO32 und der SPI-Bus werden als
absichtliche gemeinsame Netze geprüft. Nicht bonded/nicht herausgeführte
Nummern erhalten keine neue Rolle.

Für BTS7960 bleiben Pulldowns an 13/14/25, LOW bei Boot/Reset/Fehler,
break-before-make und der unzulässige Zustand
`RPWM=HIGH && LPWM=HIGH` sichtbar. R_IS/L_IS werden nicht roh an 34/35
angeschlossen. Der konkrete `74HC244D` beziehungsweise Buffer und seine
Versorgung werden geprüft; 3,3-V-HIGH an 5-V-HC-Logik wird nicht ohne Nachweis
behauptet. FT232RL bleibt auf GPIO1/3 mit 3,3-V-Kompatibilitäts- und DTR/RTS-
Auto-Reset-Gate sowie ohne angenommenen Fermenter-Versorgungsweg.

## 7. Commit-Schnitte nach Ownerfreigabe

Vor der Planfreigabe wird ausschließlich dieser Plan und die minimale
Roadmap-Synchronisierung committed. Danach ist ein kleiner
Dokumentationsschnitt vorgesehen:

1. `config/pins.example.yaml`, `config/hardware.example.yaml` und
   `docs/HARDWARE.md` gemeinsam aktualisieren;
2. `references/LINKS.md` nur bei real benötigten fehlenden Herstellerquellen
   minimal ergänzen;
3. YAML-, Link-, Konsistenz- und Diff-Nachweise ausführen;
4. PR-Body und Roadmap nur mit realen Status- und Nachweisdaten aktualisieren.

Bei einer materiellen Abweichung an Schaltung, Statusmodell, Hardwarerolle,
Softwarevariantenvertrag oder Teststrategie wird angehalten, der vollständige
Plan revidiert committed und eine neue exakte Ownerfreigabe eingeholt.

## 8. Nachweise und Abschluss

### Planphase

Zulässige Prüfungen sind:

```bash
git diff --check
python3 scripts/check_secrets.py
python3 scripts/check_architecture_boundaries.py
```

Geänderte Markdown-Links, lokale Referenzpfade, Planstruktur, PR-Basis und
Arbeitsbaum werden zusätzlich manuell geprüft. Ein projektspezifischer
Markdown-/Link-Validator ist in der Baseline nicht vorhanden; fehlende
Automatisierung wird nicht als PASS ausgegeben.

Nicht ausgeführt und daher nicht bestätigt:

```text
FIRMWARE_BUILD=NOT_RUN
NATIVE_TESTS=NOT_RUN
ESP_IDF_TESTS=NOT_RUN
HARDWARE_RUN=NOT_RUN
FLASH_UART_RESET=NOT_RUN
GPIO_LEVEL_MEASUREMENTS=NOT_RUN
DISPLAY_TOUCH_FUNCTION=NOT_RUN
ACTUATOR_OR_PELTIER_TEST=NOT_RUN
```

### Implementierungsphase

Nach Ownerfreigabe folgen YAML-Syntaxvalidierung, Link-/Pfadprüfung,
Konsistenzprüfung zwischen beiden YAML-Dateien, `docs/HARDWARE.md`, Roadmap
und #29/#30/#31/#32/#33 sowie die systematische GPIO-Inventur 0..39 auf
Doppelbelegung, absichtliche gemeinsame Busse, Strap-Konflikte, Input-only-
Verwendung und Flash-Pins. Kein Beispielwert darf eine Produktivfreigabe
erzeugen.

Reale Kontinuität, Widerstände, Pegel, Boot-/Resetverhalten, Modulidentität,
Verbraucherreaktion, SPI-/Touchfunktion, ROM-IDs, Hot-Plug-Isolation und
begrenzte BTS7960-/Aktorprüfungen bleiben den owning Hardware-Issues
vorbehalten.

Nach Plan-Commit, Push und PR-Update gelten exakt:

```text
IMPLEMENTATION=NOT_STARTED
GPIO_MATRIX=PLANNED_NOT_CONFIRMED
HARDWARE_RUN=NOT_RUN
OWNER_PLAN_REVIEW_REQUIRED=YES
MERGE=NO
```

Der nächste Schritt ist ausschließlich die Ownerprüfung der exakten Plan-SHA.
Ohne diese Freigabe werden keine kanonischen Hardware-/YAML-Dateien geändert,
keine Firmware- oder Hardwaretests gestartet und keine weitere PR-Reife
behauptet.
