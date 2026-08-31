# Issue #130 – R1-GPIO-SSOT und Synchronisierung der Hardware-Issues

## Planstatus und Owner-Gate

Dieser Plan ist die vollständige Neubewertung von PR #131 / Issue #130. Der
direkte Vorgänger ist
cdac588dbbaa3ac9b3695e3089efd38bddcea058. Die älteren Plan-SHAs
6d165feacfd0a04a04eec2e5ab7785cfe2bf9b26 und
f85df6127708e69484a0f69d50a6bc5d23978071 bleiben als historische
superseded Provenienz erhalten; sie sind keine elektrische Autorität.

Die folgende Statuszeile beschreibt den Zustand nach dem Plan-Commit. Die
exakte neue Plan-SHA wird erst durch diesen Commit festgelegt und danach im
Draft-PR und im aktuellen SESSION-HANDOVER eingetragen.

    PLAN_REVISION=COMPLETE
    DIRECT_PREDECESSOR_PLAN_SHA=cdac588dbbaa3ac9b3695e3089efd38bddcea058
    REAL_HARDWARE_PRESENT=YES
    BOARD_FAMILY_REFERENCE_MATCH=CONFIRMED_BY_OWNER
    BOARD_FAMILY=esp32_32e_quad_mosfet
    MCU_MODULE=ESP32-WROOM-32E
    BOARD_REVISION=TBD_HARDWARE
    ACTIVE_LEVELS_CONFIRMED=NO
    BOOT_LEVELS_CONFIRMED=NO
    MOSFET_ELECTRICAL_BEHAVIOR_CONFIRMED=NO
    BTS7960_LOGIC_CONFIRMED=NO
    DISPLAY_TOUCH_ELECTRICAL_FUNCTION_CONFIRMED=NO
    GPIO_FUNCTIONAL_TEST=NO
    ACTUATOR_RELEASE=NO
    CONFIRMED_TEST=NO
    ADR002_AMENDMENT=PLANNED
    GPIO_MATRIX=UNCHANGED
    GPIO_MATRIX_STATUS=PLANNED_NOT_CONFIRMED
    ELECTRICAL_VERIFICATION=PENDING
    ISSUE130_PLAN_SCOPE_SYNC=PASS
    POST_MERGE_ISSUE_SYNC=PLANNED
    PLAN_APPROVED=NO
    GPIO_SSOT=PLANNED
    ISSUE_SYNC=PLANNED
    OLD_ISSUE_PIN_ASSUMPTIONS=NON_AUTHORITATIVE
    EXTERNAL_RESISTORS=OWNER_APPROVED_AS_SPECIFIED
    IMPLEMENTATION=NOT_STARTED
    SSOT_IMPLEMENTED_IN_PR=NOT_STARTED
    SSOT_MERGED_TO_INTEGRATION=NO
    ISSUES_SYNCHRONIZED_POST_MERGE=NOT_STARTED
    GPIO_MATRIX=PLANNED_NOT_CONFIRMED
    HARDWARE_RUN=NOT_RUN
    OWNER_PLAN_REVIEW_REQUIRED=YES
    MERGE=NO

Es gibt für diesen Scope keine freigegebene Plan-SHA. Umsetzung, Issue-Body-
Synchronisierung und Hardwareabnahme bleiben bis zur ausdrücklichen Freigabe
genau dieser neuen Plan-SHA gesperrt. Die Boardfamilienidentität ist durch
Owner-Referenzabgleich bestätigt; das ist kein elektrischer Funktionsnachweis.

## 1. Live-Basis und Abgrenzung

Vor der Neubewertung wurden Repository, Branch, Live-PRs, Issue #130,
Roadmap, lokale Agentenregeln und die feste Pfadnutzung geprüft.

| Gegenstand | Live-Stand / Bedeutung |
|---|---|
| Zielbasis | origin/integration/r1-development, c1f5fbb5f19ab8e7d2c25708fe79777d523217d4 |
| Arbeitsbranch | agent/issue-130-r1-gpio-matrix |
| PR #131 | OPEN, DRAFT, Base integration/r1-development, Head cdac588dbbaa3ac9b3695e3089efd38bddcea058 |
| Issue #130 | OPEN, Eigentümer des eigenständigen GPIO-/Verdrahtungsscope; beim Start dieser Revision enthielt der Live-Body noch die nun ersetzte Reset-Entkopplungsannahme |
| PR #129 | OPEN, DRAFT, Head f3725e5557b040dc388a9d9ca9077329b0e5c672; vollständig getrennt |
| bisheriger Plan | docs/tasks/issue-130-r1-gpio-matrix-plan.md @ f85df6127708e69484a0f69d50a6bc5d23978071; nicht freigegeben |
| Roadmap | bestehende Issue-130-Zeile; wird nur auf den neuen Planstatus und die SSOT-/Issue-Gate-Semantik synchronisiert |
| feste Pfadnutzung | Keine aktive Script-/CI-/Dokumentationsreferenz auf eine vollständige pins.example.yaml-Matrix gefunden; historische Verweise in einem älteren Plan werden bei der Migration nicht als Laufzeitvertrag behandelt |
| Arbeitszustand | Branch war vor dieser Planrevision sauber; die bestehende Plan-Datei wurde ausschließlich zur vollständigen Ersetzung entfernt und wird in diesem Commit wieder angelegt |

Die reale Controllerplatine ist vorhanden und entspricht laut Owner-Abgleich
der im Repository dokumentierten ESP32-WROOM-32E-Quad-MOSFET-Boardfamilie.
Damit ist BOARD_FAMILY_IDENTITY=CONFIRMED_BY_OWNER_REFERENCE_MATCH. Die
Boardfamilie und das MCU-Modul sind identifiziert; eine exakte
PCB-Revisionskennung ist nicht belegt und bleibt
BOARD_REVISION=TBD_HARDWARE. Daraus folgen keine bestätigten aktiven Pegel,
Boot-/Resetpegel, MOSFET-/BTS-/Display-/Touch-Funktionen, GPIO-Funktionstests
oder Aktorfreigaben. ELECTRICAL_VERIFICATION=PENDING.

PR #129 behandelt ausschließlich Issue #29, Panic-Requalifikation und
Stackbudget. Inhalte, Root-Cause-Entscheidungen und historische Statuszeilen
von PR #129 werden nicht übernommen oder verändert. Falls PR #129 vor der
Umsetzung dieses Plans in die Integrationsbasis gelangt, wird der GPIO-Branch
ohne Rebase oder Force-Push auf den neuen Live-Stand synchronisiert; die
Roadmap wird dann nur gegen diesen Stand aufgelöst.

## 2. Ziel und Nicht-Ziele dieser Runde

Ziel ist ein vollständiger, standalone versionierter Umsetzungsplan für:

1. eine einzige handgepflegte R1-Boardprofilquelle für konkrete GPIO-Zahlen;
2. elektrische Metadaten, Bus-/Netz-Sharing und den vollständigen Inventarbereich
   EN sowie GPIO0 bis GPIO39;
3. die vom Owner festgelegte Widerstandspolitik;
4. die direkte gemeinsame EN-/TFT-RESET-Zielverdrahtung;
5. die spätere Synchronisierung der relevanten Hardware-Issues auf die SSOT;
6. einen späteren Generator-/Validatorpfad ohne handgepflegte Firmware-
   Doppelquelle.

In dieser Runde werden ausschließlich dieser Plan, die minimale Roadmap- und
PR-/Handover-Synchronisierung sowie die ausdrücklich erlaubte Korrektur des
Scope-/Plan-Texts von Issue #130 geändert. Nicht enthalten sind:

- Anlage von config/board_profiles/esp32_32e_quad_mosfet_r1.yaml;
- Migration, Entfernung oder inhaltliche Erweiterung von
  config/pins.example.yaml;
- Änderung von config/hardware.example.yaml;
- Änderung von docs/HARDWARE.md oder references/LINKS.md;
- Änderungen an Issue-Bodies #29, #30, #31, #32, #33 oder anderen Issues;
  Issue #130 ist die ausdrücklich erlaubte Ausnahme und wird nur auf den
  aktuellen Plan-/Ownerentscheid korrigiert, ohne eine ungemergte SSOT als
  bereits autoritativ auszugeben;
- Änderung von docs/DECISIONS.md beziehungsweise Implementation des
  ADR-002-Amendments;
- Firmware, ESP-IDF-Adapter, Composition Root, Generator oder Validator;
- Produktionsfreigabe, Aktorfreigabe, Firmware-Build oder Hardwarelauf;
- eine Behauptung von confirmed_test.

Die im Folgenden beschriebene Matrix ist im Plan die verbindliche
Umsetzungsgrundlage dieses Scopes. Nach Freigabe wird sie in die Boardprofil-
Quelle überführt; sie wird nicht zusätzlich als unabhängige Matrixdatei
angelegt. Die Korrektur des Issue-#130-Texts in dieser Runde ist ein
Plan-/Scope-Sync, kein Merge und kein Nachweis einer bereits vorhandenen
SSOT.

## 3. Owner-Entscheidungen und Revisionsdelta

Alte Issues, alte Planstände und historische Präferenzen sind für konkrete
GPIO-Zahlen, Pegel und Pull-Widerstände nicht autoritativ. Die technische
Autorität wird nach Umsetzung und Merge dieses Plans auf das versionierte
R1-Boardprofil unter
config/board_profiles/esp32_32e_quad_mosfet_r1.yaml verlagert. Bis zum Merge
ist dieser Pfad ein geplanter Zielpfad und keine Live-Quelle auf
integration/r1-development.

Gegenüber dem Vorgängerplan gelten insbesondere diese Änderungen:

- Das Boardprofil wird die einzige handgepflegte Quelle für konkrete R1-GPIO-
  Zahlen.
- Eine vollständige zweite GPIO-Matrix in config/pins.example.yaml ist
  unzulässig; vor der Umsetzung wird der Pfad nochmals auf Vertragsnutzung
  geprüft und anschließend auf einen kleinen, ausdrücklich
  nichtautoritativen Profilverweis reduziert oder entfernt.
- config/hardware.example.yaml verweist nur noch auf
  controller.board_profile: esp32_32e_quad_mosfet_r1 und dupliziert keine
  Pinzahlen.
- Die externen Widerstände aus Abschnitt 7 sind Designbestandteil und keine
  spätere Messoption.
- GPIO32 trägt den gemeinsamen internen Multidrop-Bus für Schrankluft und
  Kühlkörper; GPIO33 bleibt der eigene abnehmbare Produktfühler-Bus.
- GPIO4 wird für Backlight-PWM verwendet; die verbindliche lokale
  Helligkeits-/Dimmfunktion wird nicht durch always-on 3,3 V ersetzt.
- TFT_RESET folgt EN/CHIP_PU über ein direktes gemeinsames active-low
  Reset-Netz. Die im Vorgängerplan vorgesehene einseitige Entkopplungsstufe
  ist damit verworfen. Eine zusätzliche Stufe ist nur zulässig, wenn das
  veröffentlichte MSP2807-Schaltbild und das reale Modul einen elektrischen
  Widerspruch nachweisen.
- Issue #130 wird in dieser Planrevision vom veralteten Reset-/Scope-Text
  bereinigt, ohne die ungemergte Zielquelle als produktiv autoritativ zu
  behaupten.
- #29 bis #33 und weitere tatsächlich betroffene Issues werden erst nach dem
  Merge dieses PR gegen die reale Merge-SHA und das dann existierende
  Boardprofil synchronisiert; vor dem Merge bleibt ISSUE_SYNC=PLANNED.

### Geplanter ADR-002-Amendment

Der aktuelle kanonische ADR-002 in docs/DECISIONS.md ist für den früheren
Stand formuliert und verbietet konkrete GPIO-Zuweisungen pauschal, solange
Hardwarebestätigung fehlt. Er wird nicht gelöscht und in dieser Runde nicht
geändert. Nach Ownerfreigabe wird er minimal amended, weil Board-/Hardware-
identität, Design-/Verdrahtungsentscheidung und elektrische Verifikation
getrennte Gates sind.

Vorgesehener Amendment:

    ADR-002: GPIO-Designzuweisung und reale Hardwarefreigabe sind getrennte
    Gates.

    Status: accepted; amended by Issue #130 / PR #131

    Konkrete GPIO-Zahlen dürfen als versionierter Design-/Sollzustand in
    einer getrackten Board-/Wiring-SSOT festgelegt werden, sobald die
    zugrunde liegende Board-/Modulfamilie hinreichend identifiziert und die
    Zuordnung gegen belastbare Schalt-, Hersteller- und PCB-Unterlagen
    geprüft ist.

    Solche Werte sind planned bzw.
    board_fixed_pending_electrical_verification und stellen noch keinen
    elektrischen Hardware-PASS dar.

    PCB-feste Zuordnungen dürfen als
    board_fixed_pending_electrical_verification dokumentiert werden, wenn
    reale Platine und Referenzunterlage in der Boardfamilie übereinstimmen.

    confirmed_test entsteht ausschließlich durch reale elektrische bzw.
    funktionale Verifikation am konkreten Aufbau.

    Ein geplanter oder board-fixed GPIO darf niemals automatisch
    pins_confirmed, active_levels_confirmed, boot_levels_confirmed,
    actuator_release oder ein anderes Hardware-PASS-Gate setzen.

    Safety-relevante Ausgänge bleiben bis zu ihren owning Hardwaregates
    fail-closed.

    Historische Issues oder alte Pinannahmen sind keine elektrische
    Autorität, wenn sie der gemergten Board-/Wiring-SSOT oder belastbaren
    Primärquellen widersprechen.

    Eine zweite unabhängig handgepflegte Pinmatrix bleibt verboten.

Der Statussatz board-fixed ist hierbei nur Kurzform im Fließtext; als
Assignment-Status wird ausschließlich
board_fixed_pending_electrical_verification verwendet. Der Amendment-Text
wird erst in der Implementierungsphase in docs/DECISIONS.md eingetragen.

## 4. Primärquellen- und Real-Hardware-Prüfung

Vor der Implementierung werden die folgenden Fragen gegen vorhandene lokale
Quellen, konkrete PCB-/Modulunterlagen und Herstellerquellen beantwortet. Die
Planphase behauptet damit noch keine reale elektrische Bestätigung.

| Prüfobjekt | Zu belegende Punkte | Ergebnisstatus in dieser Planphase |
|---|---|---|
| Owner-Referenzabgleich der realen Controllerplatine | Reale Hardware vorhanden; Übereinstimmung mit der im Repository dokumentierten ESP32-WROOM-32E-Quad-MOSFET-Boardfamilie | CONFIRMED_BY_OWNER; keine Aussage über aktive Pegel, Bootwirkung oder Funktion |
| ESP32-WROOM-32E/32UE und Espressif GPIO-/Hardware-Guidelines | EN/CHIP_PU, GPIO0/GPIO2/GPIO12/GPIO15-Straps, GPIO6..11 Flash, Input-only GPIO34..39, nicht herausgeführte Pins, Boot-Pegel und UART0 | geplant, noch kein Hardwaretest |
| MSP2807-Schaltbild und reales Modul | ILI9341 RESET als Eingang, Signalrichtung, LED-Polarität, CS/DC/SPI/Touch-Signale, R4 tatsächlich 10 kOhm nach 3,3 V für T_IRQ | geplant; R4 bleibt bis zum Nachweis nicht als real bestätigt |
| ILI9341-Datenblatt | RESET- und SPI-Pegel sowie gemeinsame Reset-Netzverträglichkeit | geplant |
| DS18B20-Datenblatt und reale Sensorverdrahtung | 3-Leiter-Betrieb, Multidrop/ROM-ID, Pull-up je Bus, CRC/Hot-Plug-Verhalten | geplant |
| gelieferte DS3231-Platine und DS3231-Familienunterlagen | SDA/SCL-Pull-ups stromlos identifizieren, VCC=3,3 V verifizieren, INT/SQW unbenutzt, delivered_variant getrennt von Softwarefreigabe dokumentieren | geplant |
| Infineon BTS7960-/konkrete IBT-2-Unterlagen | RPWM/LPWM/EN-Pegel, R_IS/L_IS-Verhalten, Eingangsbuffer, Versorgung und 3,3-V-HIGH-Kompatibilität | geplant; konkrete Buffer-Variante, z. B. 74HC244D, nicht geraten |
| TI-74HC244-Unterlage, falls auf dem realen Modul vorhanden | Logikpegel und Versorgung des tatsächlichen Eingangsbuffers | geplant |
| FTDI FT232R-Unterlage und Espressif Auto-Reset-Unterlage | 3,3-V-UART-I/O, DTR/RTS nur über die vorgesehene Auto-Reset-Logik, kein zusätzlicher Versorgungsweg | geplant |

Vorrang haben die bereits im Repository vorhandenen Dateien unter
references/datasheets/ und die konkrete lokale PCB-/Moduldokumentation,
insbesondere:

- references/datasheets/Display/MSP2807-2.8-SPI.pdf;
- references/datasheets/Display/2.8inch_SPI_Module_MSP2807_User_Manual_EN.pdf;
- references/datasheets/Display/ILI9341 Datasheet.pdf;
- references/datasheets/esp32-32e-quad-mosfet-board-supplier.pdf;
- references/datasheets/bts7960-infineon.pdf;
- references/datasheets/ds18b20.pdf;
- references/datasheets/ft232rl-adapter-supplier.pdf.

Die bei der Umsetzung tatsächlich vorhandenen Dateinamen werden vor der
Quellenprüfung mit rg/find verifiziert; ein nicht vorhandener Dateiname wird
nicht als Quelle behauptet. Fehlende Herstellerquellen werden nur im
notwendigen Umfang in references/LINKS.md ergänzt. Eine Quelle gilt nicht als
Bestätigung einer realen Platinenbestückung; diese wird bei der
Hardwareabnahme separat protokolliert.

Vorgesehene Herstellerreferenzen:

- Espressif ESP32-WROOM-32E/32UE Datasheet und ESP32 Hardware Design
  Guidelines;
- Analog Devices DS18B20 und DS3231-Familie;
- Infineon BTS7960;
- TI 74HC244, falls der konkrete IBT-2-Buffer dies erfordert;
- LCDWiki MSP2807, lokales MSP2807-Schaltbild und ILI9341-Datenblatt;
- FTDI FT232R.

## 5. Kanonisches Boardprofil nach Freigabe

Die nach Freigabe anzulegende Datei lautet:

    config/board_profiles/esp32_32e_quad_mosfet_r1.yaml

Der Profilkopf und ein repräsentativer elektrischer Pinvertrag haben
sinngemäß diese Form:

    profile:
      id: esp32_32e_quad_mosfet_r1
      board_family: esp32_32e_quad_mosfet
      board_revision: TBD_HARDWARE
      mcu_module: ESP32-WROOM-32E
      release: r1
      verification_state: planned_not_confirmed

    pins:
      gpio13:
        function: peltier_rpwm
        direction: output
        active_level: high
        external_bias:
          type: pulldown
          resistance_ohm: 10000
          placement: receiver_side_or_ibt2_connector_side
        safe_boot_level: low
        assignment_status: planned

      gpio16:
        function: onboard_mosfet_1
        application_role: internal_fan
        direction: output
        active_level: TBD_HARDWARE
        external_bias: TBD_BOARD_CIRCUIT
        assignment_status: board_fixed_pending_electrical_verification

      gpio39:
        function: touch_irq
        direction: input_only
        active_level: low
        external_bias:
          type: module_pullup
          resistance_ohm: 10000
          source: MSP2807_R4
          placement: module
          condition: omit_external_pullup_if_confirmed
        assignment_status: planned

    buses:
      display_touch_spi:
        sck_gpio: 18
        mosi_gpio: 23
        miso_gpio: 19

    nets:
      display_reset:
        source: EN_CHIP_PU
        target: MSP2807_RESET
        topology: direct_active_low_shared_reset

Der Ausschnitt ist nur ein Schema-Beispiel und ersetzt nicht das vollständige
Inventar in diesem Plan. Die tatsächlich anzulegende Datei enthält alle
Pins, Busse, Netze und Widerstände aus den folgenden Abschnitten.

Sie ist die einzige handgepflegte Quelle für konkrete R1-GPIO-Zahlen. Das
Profil enthält mindestens:

- profile.id: esp32_32e_quad_mosfet_r1;
- profile.board_family: esp32_32e_quad_mosfet;
- profile.board_revision: TBD_HARDWARE;
- profile.mcu_module: ESP32-WROOM-32E;
- profile.release: r1;
- profile.verification_state: planned_not_confirmed;
- pro Pin Funktion, optionale Anwendungsrolle, direction, capability,
  elektrische aktive Ebene nur bei sinnvoller Semantik, externe Bias-
  Metadaten einschließlich Zielplatzierung, sicheren Boot-/Reset-Pegel,
  Assignment-Status und Verifikationshinweise;
- buses für absichtliches SPI-, I2C-, UART- und 1-Wire-Sharing;
- nets für gemeinsame Signale wie EN/TFT_RESET und BTS-Enable;
- eine resistor_policy mit fest verbauten und konditional nicht zu
  duplizierenden Widerständen einschließlich der physischen Zielplatzierung;
- eine klare Trennung von Designzustand, Board-Fixierung und realem
  Messzustand.

Für jedes externe Bias-Netz wird unter external_bias mindestens type,
resistance_ohm beziehungsweise eine klare Konditionalregel und placement
geführt. placement beschreibt die vorgesehene physische Lage am
Empfängereingang, Steckverbinder, Modul oder direkt am Bus; es behauptet
keine bereits erfolgte Bestückungsprüfung.

Die Feldnamen und Statuswerte werden deterministisch und ohne YAML-Laufzeit-
interpretation auf dem ESP32 verwendet. Wo eine elektrische aktive Ebene
keinen Sinn ergibt, wird active_level weggelassen; SPI, I2C, UART und
bidirektionale 1-Wire-Daten erhalten stattdessen Busrolle und Richtung.

Vorgesehene Statussemantik:

- planned: Designzuordnung akzeptiert, am realen Aufbau noch nicht vollständig
  gemessen;
- board_fixed_pending_electrical_verification: PCB-seitig fest verdrahtete
  Zuordnung, elektrische Pegel, Bootwirkung und Verbraucherwirkung noch nicht
  bestätigt;
- reserved: für eine definierte Boot-/ROM-/Systemrolle reserviert, nicht als
  R1-Peripherie verwenden;
- reserved_disabled: reserviert bzw. deaktiviert, keine produktive Funktion;
- forbidden: für R1 verboten, insbesondere Flash-/Strap-Konflikt;
- unavailable_not_exposed: nicht verfügbar oder am ESP32-WROOM-32E nicht
  herausgeführt, niemals free;
- free: keiner R1-Funktion zugewiesen und innerhalb der separat
  dokumentierten Pin-Capabilities verfügbar; free bedeutet nicht allgemein
  input-only;
- confirmed_test: erst nach realer Kontinuität, Pegel-, Boot-/Reset- und
  Funktionsmessung am konkreten Aufbau. Diese Stufe wird nicht durch
  Dokumentation erzeugt.

Die vollständige Assignment-Statusmenge ist damit:
planned, board_fixed_pending_electrical_verification, reserved,
reserved_disabled, forbidden, unavailable_not_exposed, free und
confirmed_test. Der Profilzustand planned_not_confirmed ist davon getrennt
und darf nicht als Assignment-Status verwendet werden. Für EN gilt
assignment_status: planned. Capability ist eine davon getrennte Eigenschaft.
Für GPIO36 lautet die Zielstruktur ausdrücklich:

    gpio36:
      capability: input_only
      assignment_status: free

## 6. Vollständiges R1-GPIO-Inventar als Zielinhalt

Die folgende Tabelle ist der vollständige Scope für das Profil. Sie ist ein
Planartefakt und keine zusätzliche Repository-Quelle. docs/HARDWARE.md wird
nach Freigabe keine zweite vollständige handgepflegte Matrix enthalten,
sondern Statusmodell, Safety-/Verdrahtungsregeln und den Verweis auf die SSOT
erklären. Ein vollständiger lesbarer Matrixblock wäre nur zulässig, wenn er
deterministisch aus dem Profil erzeugt und im selben Scope gegen die SSOT
geprüft wird; das ist nicht die bevorzugte KISS-Variante dieses Plans.

| Pin | R1-Zielrolle | Richtung / elektrische Metadaten | Bias / sicherer Zustand | Assignment-Status |
|---|---|---|---|---|
| EN / CHIP_PU | ESP32-Hardware-Reset und Quelle für TFT_RESET | ESP32-Resetnetz, active-low | definierte ESP32-EN-Schaltung; direktes gemeinsames Resetnetz | planned |
| GPIO0 | BOOT / ROM-Download | Strap; keine R1-Peripherie | Bootstrapping unverändert; keine Zusatzlast, die Download verhindert | reserved |
| GPIO1 | UART0 TX | output; ESP32 TX -> FT232RL RXD | UART-Verbindung, keine Aktorrolle | planned |
| GPIO2 | TFT D/C bzw. DC/RS | output; D/C-Signal ohne allgemeine aktive Ebene | keine externe Beschaltung, die beim Reset HIGH erzwingt; schwacher Pulldown zulässig | planned |
| GPIO3 | UART0 RX | input; FT232RL TXD -> ESP32 RX | 3,3-V-I/O-Kompatibilität verifizieren | planned |
| GPIO4 | TFT-Backlight PWM / MSP2807 LED | output, active-high | externer 10-kOhm Pulldown nach GND; safe_boot_level low, Backlight AUS | planned |
| GPIO5 | TFT CS | output, active-low | externer 10-kOhm Pull-up nach 3,3 V; safe_boot_level high/deselected | planned |
| GPIO6 | SPI-Flash | reserved system function | nicht verwenden | forbidden |
| GPIO7 | SPI-Flash | reserved system function | nicht verwenden | forbidden |
| GPIO8 | SPI-Flash | reserved system function | nicht verwenden | forbidden |
| GPIO9 | SPI-Flash | reserved system function | nicht verwenden | forbidden |
| GPIO10 | SPI-Flash | reserved system function | nicht verwenden | forbidden |
| GPIO11 | SPI-Flash | reserved system function | nicht verwenden | forbidden |
| GPIO12 | VDD_SDIO-Strap | reserved strap | keine R1-Funktion; nicht verwenden | forbidden |
| GPIO13 | BTS7960 RPWM | output, active-high | externer 10-kOhm Pulldown nach GND; safe_boot_level low | planned |
| GPIO14 | BTS7960 LPWM | output, active-high | externer 10-kOhm Pulldown nach GND; safe_boot_level low | planned |
| GPIO15 | Touch CS / XPT2046 T_CS | output, active-low | externer 10-kOhm Pull-up nach 3,3 V; safe_boot_level high/deselected | planned |
| GPIO16 | Onboard-MOSFET 1, Innenlüfter | output; active_level TBD_HARDWARE | external_bias TBD_BOARD_CIRCUIT; kein erfundener Zusatz-Pulldown | board_fixed_pending_electrical_verification |
| GPIO17 | Onboard-MOSFET 2, Aussen-/Kühlkörperlüfter | output; active_level TBD_HARDWARE | external_bias TBD_BOARD_CIRCUIT; kein erfundener Zusatz-Pulldown | board_fixed_pending_electrical_verification |
| GPIO18 | SPI SCK, TFT SCK + Touch T_CLK | output; shared bus clock | SPI-Busrolle, keine allgemeine active_level-Angabe | planned |
| GPIO19 | SPI MISO, TFT SDO/MISO + Touch T_DO | input; shared bus data | SPI-Busrolle, keine allgemeine active_level-Angabe | planned |
| GPIO20 | unavailable | nicht verfügbar / nicht verwenden | niemals als free behandeln | unavailable_not_exposed |
| GPIO21 | I2C SDA, DS3231 SDA/D | bidirectional; I2C data | Modul-Pull-up-Entscheid nach stromloser Prüfung, siehe Abschnitt 7 | planned |
| GPIO22 | I2C SCL, DS3231 SCL/C | output/open-drain bus role; I2C clock | Modul-Pull-up-Entscheid nach stromloser Prüfung, siehe Abschnitt 7 | planned |
| GPIO23 | SPI MOSI, TFT SDI/MOSI + Touch T_DIN | output; shared bus data | SPI-Busrolle, keine allgemeine active_level-Angabe | planned |
| GPIO24 | unavailable | nicht verfügbar / nicht verwenden | niemals als free behandeln | unavailable_not_exposed |
| GPIO25 | BTS7960 R_EN + L_EN gemeinsam | output, active-high; central fail-low enable | externer 10-kOhm Pulldown nach GND; safe_boot_level low | planned |
| GPIO26 | Onboard-MOSFET 3, aktiver Summer | output; active_level TBD_HARDWARE | external_bias TBD_BOARD_CIRCUIT; kein erfundener Zusatz-Pulldown | board_fixed_pending_electrical_verification |
| GPIO27 | Onboard-MOSFET 4, Reserve | output; active_level TBD_HARDWARE | external_bias TBD_BOARD_CIRCUIT; kein erfundener Zusatz-Pulldown | board_fixed_pending_electrical_verification |
| GPIO28 | unavailable | nicht verfügbar / nicht verwenden | niemals als free behandeln | unavailable_not_exposed |
| GPIO29 | unavailable | nicht verfügbar / nicht verwenden | niemals als free behandeln | unavailable_not_exposed |
| GPIO30 | unavailable | nicht verfügbar / nicht verwenden | niemals als free behandeln | unavailable_not_exposed |
| GPIO31 | unavailable | nicht verfügbar / nicht verwenden | niemals als free behandeln | unavailable_not_exposed |
| GPIO32 | interner 1-Wire-Bus: Schrankluft + Kühlkörper | bidirectional data; gemeinsamer Multidrop-Bus | 4,7-kOhm Pull-up nach 3,3 V; 3-Leiter-Betrieb; Rollen per ROM-ID | planned |
| GPIO33 | externer 1-Wire-Bus: abnehmbarer Produktfühler | bidirectional data; eigener Hot-Plug-Bus | eigener 4,7-kOhm Pull-up nach 3,3 V; 3-Leiter-Betrieb | planned |
| GPIO34 | BTS7960 R_IS Reserve / disabled | input_only, ADC1_CH6; nicht roh anschließen | ris_lis_enabled=false; keine produktive Nutzung; Schutz-/Teiler-/RC-/Clamp-Schaltung erst nach Pegelmessung | reserved_disabled |
| GPIO35 | BTS7960 L_IS Reserve / disabled | input_only, ADC1_CH7; nicht roh anschließen | ris_lis_enabled=false; keine produktive Nutzung; Schutz-/Teiler-/RC-/Clamp-Schaltung erst nach Pegelmessung | reserved_disabled |
| GPIO36 | frei; DS3231 INT/SQW bleibt unbenutzt | capability: input_only; direction: input_only; keine R1-Zuordnung | nicht als Ausgang verwenden | free |
| GPIO37 | unavailable / nicht auf ESP32-WROOM-32E herausgeführt | nicht verwenden | niemals als free behandeln | unavailable_not_exposed |
| GPIO38 | unavailable / nicht auf ESP32-WROOM-32E herausgeführt | nicht verwenden | niemals als free behandeln | unavailable_not_exposed |
| GPIO39 | Touch IRQ / XPT2046 T_IRQ | input_only, active-low; Pegel pollen | MSP2807 R4 als 10-kOhm Pull-up nach 3,3 V am realen Modul verifizieren; kein zusätzlicher Pull-up, wenn bestätigt | planned |

Die vier fest verdrahteten Onboard-MOSFET-Kanäle erhalten den Status
board_fixed_pending_electrical_verification, aber weder aktive Polarität noch
Gate-/Treiberbias werden aus einer alten Issue-Annahme abgeleitet. GPIO34/35
bleiben bis zu einer separaten
Mess- und Schutzbewertung mit ris_lis_enabled=false deaktiviert. Die
input-only-Eigenschaft von GPIO34 bis GPIO39 wird im späteren Validator
ausdrücklich geprüft.

## 7. Busse, gemeinsame Netze und Widerstandspolitik

### 7.1 Explizite Busdefinitionen

Das Profil erhält mindestens diese Busobjekte:

    buses:
      display_touch_spi:
        sck_gpio: 18
        mosi_gpio: 23
        miso_gpio: 19
        members:
          - MSP2807_SCK
          - MSP2807_SDI_MOSI
          - MSP2807_SDO_MISO
          - XPT2046_T_CLK
          - XPT2046_T_DIN
          - XPT2046_T_DO
      one_wire_internal:
        gpio: 32
        roles:
          - chamber_air
          - heatsink
        pullup_ohm: 4700
        topology: multidrop
        wiring: three_wire
      one_wire_product:
        gpio: 33
        roles:
          - product
        pullup_ohm: 4700
        topology: dedicated_removable_bus
        wiring: three_wire
      ds3231_i2c:
        sda_gpio: 21
        scl_gpio: 22
        int_sqw: unused_r1
      uart0_programming:
        tx_gpio: 1
        rx_gpio: 3

Gemeinsames SPI-Sharing ist absichtlich und keine Doppelbelegung:
GPIO18 -> SCK + T_CLK, GPIO23 -> SDI/MOSI + T_DIN und GPIO19 <-
SDO/MISO + T_DO. TFT_CS und T_CS bleiben getrennte Deselect-Signale.

Die beiden festen DS18B20 teilen sich bewusst GPIO32 über ROM-ID-Rollen. Der
abnehmbare Produktfühler bleibt wegen Hot-Plug-/Fehlerisolation auf GPIO33.
Ein Fehler des gemeinsamen festen Busses muss in der späteren Fachlogik
weiterhin fail-closed für die Peltierfreigabe wirken.

### 7.2 Gemeinsame Netze

Das Profil erhält mindestens:

    nets:
      display_reset:
        source: EN_CHIP_PU
        target: MSP2807_RESET
        topology: direct_active_low_shared_reset
        additional_stage: none_unless_primary_source_conflict
      bts_enable:
        source_gpio: 25
        targets:
          - BTS7960_R_EN
          - BTS7960_L_EN
        topology: shared_fail_low_enable

TFT_RESET ist ein Eingang des Displays und kein Ausgang, der EN zurücktreiben
darf. Die direkte gemeinsame active-low Verbindung wird gegen MSP2807-
Schaltbild, reale Modulbestückung und EN-Schaltung geprüft. Die bestehende
ESP32-EN-/Auto-Reset-Schaltung bleibt unverändert. Kein GPIO0, kein zusätzlicher
GPIO und keine Diode, Inverter-, Buffer- oder Open-Drain-Stufe wird im R1-Ziel
vorgesehen, solange kein realer elektrischer Widerspruch nachgewiesen ist.

Für die direkte Verbindung sind zusätzlich ausdrücklich zu prüfen und zu
dokumentieren:

- ESP32 und reales MSP2807 teilen einen gemeinsamen GND;
- RESET des realen MSP2807 ist tatsächlich ein hochohmiger Eingang zum
  ILI9341 und kein Ausgang oder aktiv treibender Knoten;
- kein unabhängiger Modulzustand kann das gemeinsame EN-Netz aktiv treiben;
- Power- und Logic-Domain des realen Moduls sind mit der direkten Verbindung
  kompatibel;
- jede Abweichung des realen Moduls vom veröffentlichten MSP2807-Schaltbild
  führt zu STOP und einer Boardprofil-/Planrevision.

Eine Entkopplungsstufe wird nicht vorsorglich ergänzt. Der Nachweis muss die
direkte active-low Zieltopologie tragen; ein unklarer oder widersprüchlicher
Befund ist kein confirmed_test und keine stillschweigende Freigabe.

### 7.3 Verbindliche externe Widerstände

Diese Widerstände werden im Profil als Designbestandteil mit Netz, Wert,
Zweck und sicherem Zustand erfasst und nach Planfreigabe im realen Aufbau
verbaut:

| Netz | Widerstand | Zweck |
|---|---:|---|
| GPIO32 / OneWire internal DATA nach 3,3 V | 4,7 kOhm | Pull-up des gemeinsamen festen 1-Wire-Multidrop-Busses |
| GPIO33 / OneWire product DATA nach 3,3 V | 4,7 kOhm | Pull-up des abnehmbaren Produktfühler-Busses |
| GPIO13 / BTS RPWM nach GND | 10 kOhm | Fail-low bei Boot, Reset und Fehler |
| GPIO14 / BTS LPWM nach GND | 10 kOhm | Fail-low bei Boot, Reset und Fehler |
| GPIO25 / gemeinsames BTS R_EN/L_EN nach GND | 10 kOhm | zentrale fail-low Freigabe |
| GPIO5 / TFT_CS nach 3,3 V | 10 kOhm | TFT beim Boot/Reset sicher deselected |
| GPIO15 / Touch_CS nach 3,3 V | 10 kOhm | Touch beim Boot/Reset sicher deselected |
| GPIO4 / TFT_BL nach GND | 10 kOhm | Backlight beim Boot/Reset AUS |

Für die physische Zielplatzierung wird im Profil zusätzlich festgehalten:

- RPWM, LPWM und gemeinsames R_EN/L_EN: placement =
  receiver_side_or_ibt2_connector_side; der Pulldown liegt bevorzugt am
  IBT-2-/Empfängereingang oder am Steckverbinder, damit der Eingang auch bei
  abgezogenem oder hochohmigem Controller LOW bleibt;
- TFT_CS, Touch_CS und TFT_BL: placement =
  receiver_input_or_connector_side, soweit der reale Aufbau dies praktisch
  zulässt;
- GPIO32 und GPIO33: placement = one_wire_bus_data_net_at_receiver_or_bus;
  der jeweilige Pull-up liegt elektrisch eindeutig am zugehörigen Bus und
  wird nicht über den anderen Bus geteilt.

placement beschreibt die Design-/Bestückungsvorgabe und wird erst durch
Kontinuitäts- und Pegelmessung am realen Aufbau zu einem Hardwarebefund.

Die BTS-Steuerung bleibt softwareseitig zusätzlich safe-off:
EN=LOW, RPWM=LOW und LPWM=LOW bei Boot/Reset/Fehler; Richtungswechsel
break-before-make; RPWM=HIGH und LPWM=HIGH ist unzulässig.

### 7.4 Keine unnötige Doppelbeschaltung

- GPIO39/T_IRQ erhält keinen zusätzlichen externen Pull-up, wenn das reale
  MSP2807 R4 tatsächlich 10 kOhm nach 3,3 V bestätigt. Bis dahin ist die
  Modulbestückung ein Hardware-Verifikationsgate, nicht eine bestätigte
  Messung.
- GPIO21/22 erhalten keinen zusätzlichen I2C-Pull-up, wenn die stromlose
  Prüfung des gelieferten DS3231-Moduls je Leitung ungefähr 4,7 kOhm nach
  Modul-VCC und Modul-VCC=3,3 V nachweist. Andernfalls werden SDA und SCL
  jeweils mit 4,7 kOhm nach 3,3 V vorgesehen.
- GPIO16, GPIO17, GPIO26 und GPIO27 erhalten keinen zusätzlichen
  MOSFET-Steuerwiderstand oder Pulldown, bevor die reale Onboard-Schaltung
  einschließlich direkter/invertierter Ansteuerung, Gate-/Treiberbias und
  Verbraucherwirkung bekannt ist.

## 8. Komponenten- und Schnittstellenverträge

### Display und Touch

Der gemeinsame SPI-Bus lautet:

    GPIO18 -> SCK + T_CLK
    GPIO23 -> SDI/MOSI + T_DIN
    GPIO19 <- SDO/MISO + T_DO
    GPIO5  -> TFT_CS
    GPIO2  -> TFT_DC/RS
    GPIO15 -> T_CS
    GPIO39 <- T_IRQ
    GPIO4  -> LED / Backlight PWM

TFT_CS und T_CS müssen durch die externen Pull-ups während Boot/Reset HIGH
und damit deselected sein. GPIO2 darf keinen Boot-Strap-Konflikt durch eine
externe HIGH-Erzwingung erhalten. T_IRQ wird standardmäßig als Pegel gepollt;
die reale Modulbeschaltung und der IRQ-Pegel werden in #31 verifiziert.

### DS18B20

Der interne Bus GPIO32 enthält die Rollen Schrankluft und Kühlkörper. Beide
Sensoren laufen im 3-Leiter-Betrieb an einem 1-Wire-Multidrop-Bus und werden
über ROM-ID unterschieden. Der externe Produktfühler bleibt im 3-Leiter-
Betrieb auf GPIO33 mit eigenem Pull-up und eigener Fehler-/Hot-Plug-Isolation.
Treiber, CRC, ROM-Zuordnung und reale Busfunktion gehören in Issue #30 und
werden dort nicht durch eine neue GPIO-Entscheidung ersetzt.

### DS3231 mini

Die Matrix ist variantenneutral:

    3,3 V -> VCC/+
    GND   -> GND/-
    GPIO21 <-> SDA/D
    GPIO22  -> SCL/C
    INT/SQW -> R1 unbenutzt

Der Board-/Wiring-SSOT-Teil erhält nach Primärquellenbestätigung:

    rtc:
      physical_family: DS3231
      sda_gpio: 21
      scl_gpio: 22
      int_sqw: unused_r1
      wiring_compatible_variants:
        - DS3231SN
        - DS3231M

Die Liste wiring_compatible_variants wird nur eingetragen, wenn die
Primärquellen die elektrische Verdrahtungskompatibilität bestätigen. Das
tatsächlich gelieferte Modul ist kein statisches Board-Wiring-Metadatum:

    delivered_variant: TBD_DELIVERY
    software_supported_variant: DS3231SN

Diese Felder gehören in Produkt-/Hardwarekonfiguration beziehungsweise
Commissioning. Der bereits gemergte Softwarevertrag aus #126 wird nicht
still umgeschrieben. Falls DS3231M geliefert wird, ist
Kompatibilitäts-/Registerarbeit ein eigener Follow-up-Scope und keine
GPIO-Änderung.

### BTS7960 / IBT-2

Die Steuerung lautet:

    GPIO13 -> RPWM
    GPIO14 -> LPWM
    GPIO25 -> R_EN + L_EN

RPWM, LPWM und das gemeinsame EN erhalten die externen 10-kOhm-Pulldowns.
R_IS/L_IS werden nicht roh an GPIO34/35 angeschlossen. Die konkrete
IBT-2-/BTS7960-Variante, der sichtbare 74HC244D oder ein anderer
Eingangsbuffer, dessen Versorgung, Logikpegel, Richtung, Ausgangspolarität
und die Nutzbarkeit der Strommessausgänge werden in #33 real verifiziert.
Eine 3,3-V-ESP32-HIGH-Kompatibilität bei 5-V-HC-Logik wird nicht ohne Nachweis
behauptet.

### FT232RL / Programmierung

    ESP32 GPIO1 / U0TXD -> FT232RL RXD
    ESP32 GPIO3 / U0RXD <- FT232RL TXD
    ESP32 GND            -  FT232RL GND

UART-I/O muss 3,3-V-kompatibel sein. GPIO0 bleibt BOOT/Download-Strap und EN
bleibt Reset/CHIP_PU. DTR/RTS dürfen ausschließlich über die von Espressif
vorgesehene Auto-Reset-Logik auf EN/GPIO0 wirken, nicht als rohe Direkt-
verbindung. Der FT232 ist kein zusätzlicher Versorgungsweg für den
Fermenter, sofern der reale Aufbau dies nicht ausdrücklich verifiziert.

## 9. Geplante Dateien und SSOT-Migration nach Freigabe

Die Implementierung folgt nach Ownerfreigabe in einem vollständigen Diff gegen
diesen Plan:

1. docs/DECISIONS.md durch einen minimalen Amendment von ADR-002 ergänzen:
   Status accepted; amended by Issue #130 / PR #131, mit der in Abschnitt 3
   beschriebenen Trennung von Boardidentität, Designzuweisung und realer
   elektrischer Verifikation. ADR-002 wird nicht gelöscht und es wird kein
   neuer ADR angelegt.
2. config/board_profiles/esp32_32e_quad_mosfet_r1.yaml anlegen und die Matrix,
   Metadaten,
   Busse, Netze und Widerstandspolitik gemäß Abschnitt 5 bis 7 eintragen.
3. Vor der Änderung erneut mit rg prüfen, ob pins.example.yaml inzwischen von
   Script, CI oder Dokumentation als fester Vertragspfad verwendet wird.
4. config/pins.example.yaml entweder entfernen oder auf einen kleinen,
   ausdrücklich nichtautoritativen Verweis reduzieren, zum Beispiel mit
   profile_id: esp32_32e_quad_mosfet_r1. Keine vollständigen GPIO-Zahlen
   duplizieren.
5. config/hardware.example.yaml um
   controller.board_profile: esp32_32e_quad_mosfet_r1 ergänzen und konkrete
   Pinzahlen daraus entfernen. Produkt-/Komponentendaten bleiben dort
   erhalten. physical_family: DS3231 und wiring_compatible_variants kommen
   in das Produkt-/Wiring-Metadatum, sofern die Primärquellenprüfung die
   Variantenneutralität bestätigt. delivered_variant: TBD_DELIVERY und
   software_supported_variant: DS3231SN gehören in Hardwarekonfiguration
   beziehungsweise Commissioning, nicht in das statische Boardprofil.
6. docs/HARDWARE.md mit dem eindeutigen Hinweis aktualisieren:

       Electrical/design SSOT for the R1 pin assignment:
       config/board_profiles/esp32_32e_quad_mosfet_r1.yaml

   Das Dokument erklärt Statusmodell, Safety-/Verdrahtungsregeln,
   Reset-/Boot-Randbedingungen und die Verifikationszustände lesbar. Es
   enthält keine zweite vollständige handgepflegte GPIO-/Widerstandsmatrix.
   Konkrete Zahlen und Widerstandswerte werden über die SSOT referenziert.
   Ein vollständiger Markdown-Block ist nur zulässig, wenn er deterministisch
   aus dem Profil erzeugt und im selben Scope gegen die SSOT geprüft wird;
   dieses Profilreferenzmodell ist die bevorzugte KISS-Variante.

   Die Hardwarestatusdarstellung verwendet dieselbe Dreiteilung:

   - Identität/Referenzabgleich: reale Hardware vorhanden und Boardfamilie
     mit der Repository-Referenz durch den Owner abgeglichen;
   - Designzustand: planned oder
     board_fixed_pending_electrical_verification;
   - reale elektrische Abnahme: confirmed_test.

   Die Designmatrix darf nach dem Merge nicht mehr pauschal als
   alle GPIOs seien TBD_HARDWARE beschrieben werden. TBD_HARDWARE bleibt auf
   tatsächlich offene Eigenschaften begrenzt, insbesondere exakte
   PCB-Revision, aktive MOSFET-Pegel, Gate-/Treiberbias, reale
   IBT-2-Variante/Logikschwelle und andere noch nicht verifizierte
   elektrische Eigenschaften.
7. references/LINKS.md nur für tatsächlich fehlende Hersteller-/Schaltungs-
   quellen ergänzen.
8. docs/ROADMAP.md nur mit Status, Planreferenz und Gate synchronisieren; die
   Anforderungen werden dort nicht kopiert.

Die spätere produktive Nutzung darf keine handgepflegte Ersatzmatrix in
Firmwareheadern einführen.

## 10. Issue-Synchronisierung und sichere Reihenfolge

### 10.1 Ausnahme: Issue #130 in dieser Planrevision

Der Live-Body von Issue #130 enthält beim Start dieser Revision noch die
supersedete Aussage einer einseitigen low-aktiven Entkopplung zwischen
EN/CHIP_PU und Display-RESET. Dieser Scope-/Plantext wird jetzt korrigiert.
Die Korrektur ist GitHub-Metadaten-Synchronisierung und behauptet weder
Hardwareimplementation noch confirmed_test.

Der korrigierte Issue-#130-Text muss mindestens festhalten:

- Die bisherigen elektrischen Detailannahmen des ursprünglichen Issues sind
  durch die neue Primärquellen-/SSOT-Neubewertung superseded.
- Der direkte gemeinsame active-low EN/TFT_RESET-Pfad ist die aktuelle
  Designentscheidung; die alte Entkopplung ist nicht mehr die Zielschaltung.
- Konkrete GPIO-Zahlen und Widerstände werden erst nach Freigabe der neuen
  Plan-SHA und anschließendem Merge der SSOT autoritativ.
- Der geplante Zielpfad ist
  config/board_profiles/esp32_32e_quad_mosfet_r1.yaml; bis zum Merge ist
  dieser Pfad auf integration/r1-development noch nicht vorhanden und
  nicht live autoritativ.
- PLAN_ONLY, PLAN_REVISION=COMPLETE, PLAN_SHA=<new-plan-sha>,
  REAL_HARDWARE_PRESENT=YES, BOARD_FAMILY_REFERENCE_MATCH=CONFIRMED_BY_OWNER,
  BOARD_REVISION=TBD_HARDWARE, GPIO_MATRIX=UNCHANGED,
  GPIO_MATRIX_STATUS=PLANNED_NOT_CONFIRMED, ELECTRICAL_VERIFICATION=PENDING,
  IMPLEMENTATION=NOT_STARTED, HARDWARE_RUN=NOT_RUN und kein confirmed_test.
- PR #129 / Issue #29 bleiben vollständig getrennt.

Issue #130 darf den geplanten SSOT-Pfad also als Ziel- und Planreferenz
nennen, nicht als bereits gemergte oder produktiv autoritative Quelle. Nach
dem Merge wird der Body ein letztes Mal auf die tatsächliche Merge-SHA und
den tatsächlich vorhandenen SSOT-Pfad synchronisiert.

### 10.2 Keine SSOT-Umschaltung in #29 bis #33 vor dem Merge

Issue-Bodies sind GitHub-Metadaten und nicht Teil des mergebaren
PR-Diffs. #29, #30, #31, #32, #33 sowie weitere betroffene Issues werden in
dieser Planrevision und während der PR-Implementierung nicht auf eine
ungemergte Datei als kanonische Wahrheit umgeschaltet. Bis zum Owner-Merge
bleibt:

    ISSUE_SYNC=PLANNED
    SSOT_MERGED_TO_INTEGRATION=NO
    ISSUES_SYNCHRONIZED_POST_MERGE=NOT_STARTED

Sichere Reihenfolge:

1. Owner gibt exakt diese neue Plan-SHA frei.
2. PR #131 implementiert und reviewt Boardprofil, Konfigurationsmigration
   und kanonische Repo-Dokumentation gemäß Plan.
3. Der Owner entscheidet über den Merge von PR #131 in
   integration/r1-development.
4. Nach dem Merge wird die reale Merge-SHA verifiziert.
5. Erst danach werden #29 bis #33 und weitere tatsächlich betroffene Issues
   gegen den existierenden SSOT-Pfad und die Merge-SHA synchronisiert.
6. Die Issues erhalten Scope-/Verifikationsverweise, aber keine erneut
   handgepflegte vollständige GPIO-Matrix.

Die Merge-SHA darf als Provenienz genannt werden. Ein nicht gemergter
PR-Head oder eine bloße Plan-SHA ersetzt sie nicht.

### 10.3 Post-Merge-Semantik der betroffenen Issues

Nach dem Merge wird in einem gezielten Metadaten-Sync nur bei echtem
Widerspruch oder konkreter alter GPIO-Annahme korrigiert. Der gemeinsame
Verweistext lautet dann:

    Konkrete GPIO-Zahlen, Pull-Beschaltungen und gemeinsame Bus-/Netzzuordnungen
    werden nicht in diesem Issue neu festgelegt. Maßgebend ist das gemergte
    R1-Boardprofil unter config/board_profiles/esp32_32e_quad_mosfet_r1.yaml,
    Merge-SHA <actual-merge-sha>. Änderungen am Boardprofil benötigen einen
    eigenen nachvollziehbaren Plan-/Owner-Gate-Scope.

Zielsemantik der einzelnen Issues:

| Issue | Eigentümerschaft nach Synchronisierung |
|---|---|
| #29 | Die gemergte Board-/Wiring-SSOT definiert den Sollzustand. #29 verifiziert Bring-up, Boot-/Strap-/Resetverhalten und die reale elektrische Vereinbarkeit. Ein realer Widerspruch führt zu einem eigenen Boardprofil-Änderungsscope; #29 vergibt keine Pins neu. |
| #30 | Die gemergte R1-Zwei-Bus-Topologie bleibt maßgebend; die historische Präferenz separate_bus_per_sensor darf sie nicht wieder öffnen. #30 verifiziert DS18B20-Treiber, ROM-ID-Rollen, CRC, Hot-Plug und reale Busfunktion. |
| #31 | #31 verifiziert realen Display-/Touch-Controller, Modulbestückung, SPI, CS-Bias, Backlight, IRQ und Resetnetz. Es gibt dort keine neue GPIO-, Reset- oder Backlight-Zuordnung. |
| #32 | #32 verifiziert PCB-feste MOSFET-Kanäle, aktive Pegel, Treiber-/Gatebeschaltung, Boot-/Resetwirkung und Verbraucherwirkung. Es gibt dort keine neue GPIO-Kanalzuordnung. |
| #33 | #33 verifiziert konkrete IBT-2-/BTS7960-Variante, Logikschwellen, Buffer-Versorgung, Polarität, Richtung und R_IS/L_IS. Es gibt dort keine neue RPWM-/LPWM-/EN-Zuordnung. Ein realer Widerspruch erfordert einen eigenen Boardprofil-Änderungsscope. |
| #130 | Nach dem Merge wird Issue #130 abschließend auf den tatsächlichen SSOT-Pfad und die tatsächliche Merge-SHA synchronisiert; die aktuelle Plan-/Scope-Korrektur bleibt von Hardwarebestätigung getrennt. |

Andere offene Issues werden nach konkreten alten GPIO-Annahmen durchsucht.
Nur technisch betroffene Issues werden synchronisiert; unbetroffene Issues
erhalten keinen Standardtext und keine erneute Matrix.

## 11. Späterer Firmware- und Generatorvertrag

Das Boardprofil ist zunächst die kanonische Hardwarebeschreibung. Bevor ein
produktiver Hardwareadapter konkrete GPIO-Zahlen verwendet, müssen diese
Werte aus derselben Quelle abgeleitet werden. Der bevorzugte Pfad ist:

    config/board_profiles/esp32_32e_quad_mosfet_r1.yaml
                 |
                 v
    deterministischer Build-Time-Generator
                 |
                 v
    generierter board_profile.hpp
                 |
                 v
    Composition Root -> generischer ESP-IDF-Adapter

board_profile.hpp ist generiert und nicht handgepflegt. Eine zweite Liste mit
GPIO_NUM_13, GPIO_NUM_14 oder vergleichbaren konkreten Zahlen ist unzulässig.
YAML wird nicht zur Laufzeit auf dem ESP32 interpretiert. fermentation_app
bleibt frei von konkreten GPIO-Zahlen; lib/device_platform/** erhält keine
konkrete R1-GPIO-Matrix oder anwendungsspezifische Boardzuordnung;
device_platform_esp_idf stellt generische Adapter bereit; die konkrete
Zuordnung wird ausschließlich in der ESP-IDF-Composition bereitgestellt.
Andere Geräte oder Boards erhalten eigene Boardprofile und werden nicht in
dieses R1-Profil hineingemischt.

Falls ein Generator bei der ersten Adapterintegration unverhältnismäßig wäre,
wird vor jeder Implementierung ein einfacher gleichwertiger Single-Source-
Mechanismus als eigener Owner-Gate-Scope vorgeschlagen. Zwei unabhängig
gepflegte Pinlisten sind in keinem Fall zulässig. Generator und Adapter sind
nicht Teil dieser Planphase.

## 12. Automatische Konsistenzprüfung als späterer Scope

Spätestens bei produktiver Nutzung des Profils wird ein kleiner deterministi-
scher Repository-Validator oder Profiltest vorgesehen. Er prüft mindestens:

- forbidden und unavailable GPIOs sind nicht als Peripherie belegt;
- input-only GPIOs sind nicht als Output definiert;
- GPIO6..11 und GPIO12 werden nicht produktiv verwendet;
- GPIO0 bleibt BOOT/ROM-Download;
- unbeabsichtigte Doppelbelegung wird abgelehnt;
- beabsichtigtes Bus-/Netz-Sharing ist explizit unter buses oder nets
  deklariert;
- BTS-RPWM, BTS-LPWM und gemeinsames BTS-EN besitzen den externen fail-low
  Bias;
- board_fixed_pending_electrical_verification bleibt von confirmed_test
  getrennt;
- confirmed_test kann nicht durch eine reine Dokumentationsänderung
  entstehen.

Der Validator ist kein Runtime-Safety-Ersatz und wird in dieser Runde nicht
implementiert. Er darf keine Aktorfreigabe erzeugen.

## 13. Architektur-, Safety- und Statusgrenzen

Das Boardprofil und die Beispiele beschreiben den Ziel-/Designzustand. Sie
erzeugen keine produktive Aktorfreigabe. Bei Boot, Reset, Fehler, unbekanntem
Zustand, unbestätigter Hardware oder offenem Safety-Gate bleibt die
Freigabe fail-closed. Die offenen Hardwaregates #29 bis #33 werden durch
diesen PR nicht als bestanden markiert.

Die vier Onboard-MOSFET-Kanäle bleiben bis zur realen elektrischen Prüfung
board_fixed_pending_electrical_verification. GPIO34/35 bleiben
reserved_disabled. R_IS/L_IS werden nicht roh angeschlossen. Die
Software-/Hardwareverifikation des BTS-Enable- und PWM-Safe-Offs wird erst
im jeweiligen Verifikationsscope ausgewiesen.

Statusregeln:

- planned ist ein akzeptierter Zielentwurf ohne vollständige reale Messung;
- board_fixed_pending_electrical_verification bedeutet PCB-seitige Zuordnung
  mit offenen elektrischen Pegel-/Boot-/Verbrauchergates;
- confirmed_test erfordert reale Kontinuität, Pegel, Boot-/Resetverhalten und
  Funktion am konkreten Aufbau;
- Build-, Markdown-, Link- oder Architekturchecks sind kein
  Hardware-Nachweis;
- historische Logs oder frühere Issue-Aussagen sind kein aktueller
  confirmed_test.

## 14. Zustandsmodell, Commit- und Prüfplan

Die vier Phasen werden ausdrücklich getrennt:

| Phase | Bedeutung |
|---|---|
| PLAN_APPROVED | Owner hat exakt die neue Plan-SHA freigegeben; noch keine SSOT- oder Dokumentationsimplementation |
| SSOT_IMPLEMENTED_IN_PR | Boardprofil, Konfigurationsmigration und kanonische Repo-Dokumentation sind im PR implementiert und gezielt geprüft; PR bleibt bis zur Ownerentscheidung Draft bzw. reviewgesperrt |
| SSOT_MERGED_TO_INTEGRATION | Owner hat PR #131 gemergt; die tatsächliche Merge-SHA ist verifiziert und die SSOT existiert auf integration/r1-development |
| ISSUES_SYNCHRONIZED_POST_MERGE | Erst nach dem Merge wurden #29 bis #33 und weitere tatsächlich betroffene Issues gegen Pfad und Merge-SHA synchronisiert; #130 erhält dabei seinen abschließenden Merge-Stand |

Aktueller Zustand dieses Plan-PR:

    PLAN_APPROVED=NO
    SSOT_IMPLEMENTED_IN_PR=NOT_STARTED
    SSOT_MERGED_TO_INTEGRATION=NO
    ISSUE_SYNC=PLANNED
    ISSUES_SYNCHRONIZED_POST_MERGE=NOT_STARTED

Nach Freigabe der exakten Plan-SHA wird die Umsetzung in nachvollziehbaren
PR-Commits durchgeführt:

1. Boardprofil, SSOT-Migration der Konfigurationsbeispiele und
   docs/HARDWARE.md als nichtduplizierende Dokumentation gegen diesen Plan.
2. Nur notwendige Quellenlinks und minimale Roadmap-Synchronisierung.
3. Vollständiger Diff, YAML-/Profilprüfung und direkte Konsistenzprüfung;
   keine Issue-Body-Umschaltung auf die ungemergte SSOT.
4. Owner-Review und Owner-Merge von PR #131.
5. Verifikation der tatsächlichen Merge-SHA.
6. Separater Post-Merge-Sync der tatsächlich betroffenen Issue-Bodies,
   einschließlich #130-Abschluss auf den gemergten Pfad.
7. Ein später separat freizugebender Generator-/Validator-/Adapter-Scope,
   falls für produktive Nutzung erforderlich.

Bei jedem materiellen Widerspruch zwischen Profil, realem PCB/Modul und
Primärquelle wird angehalten. Die Matrix wird dann nicht still umgedeutet;
stattdessen wird der Plan beziehungsweise das Boardprofil mit einem eigenen
Owner-Gate-Scope revidiert. Es gibt keinen Force-Push und keine
Historienrekonstruktion.

Für die aktuelle Planphase sind nur diese gezielten Nachweise vorgesehen:

    git diff --check
    python3 scripts/check_secrets.py
    python3 scripts/check_architecture_boundaries.py

YAML-Validierung, Firmware-/ESP-IDF-Build, Generator-/Validatorlauf,
Firmwaretests und Hardwaretests sind in dieser Runde NOT_RUN, weil noch
keine YAML-, Firmware- oder Hardwareimplementierung erfolgt. Ein
Markdown-/Linkcheck darf nur nach den im Repository verbindlichen
Qualitätsregeln ausgeführt und mit seinem tatsächlichen Status ausgewiesen
werden.

## 15. Stop-Zustand und Übergabe

Nach Commit und Push dieses Plans werden der Draft-PR #131, die neue exakte
Plan-SHA und genau ein aktuelles SESSION HANDOVER synchronisiert. Danach
wartet der Agent auf die ausdrückliche Ownerfreigabe der exakten Plan-SHA.

Die Übergabe muss mindestens ausweisen:

    IMPLEMENTATION=NOT_STARTED
    GPIO_MATRIX=PLANNED_NOT_CONFIRMED
    HARDWARE_RUN=NOT_RUN
    OWNER_PLAN_REVIEW_REQUIRED=YES
    MERGE=NO

Es wird weder auf Ready for review gesetzt noch gemergt, auto-gemergt, ein
Issue geschlossen, ein Branch gelöscht oder ein Hardwarelauf gestartet.
