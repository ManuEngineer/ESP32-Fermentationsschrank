# Issue #32 – Onboard-MOSFET-Ausgaenge (Innenluefter, Aussenluefter, Summer): Softwareplan (Revision 1)

Gilt nur fuer den Software-/Adapterteil von #32. Die reale Funktions- und
Hardwareabnahme (`SSOT_CONFORMANCE`, `FUNCTIONAL_HARDWARE_VERIFICATION`) bleibt
offen; #32 wird durch den Software-Merge nicht geschlossen. #33 (BTS7960/Peltier)
folgt separat. `ACTUATOR_RELEASE=NO`.

## 1. Ziel und Nicht-Ziele

Ziel: ein kleiner, rollenfreier ESP-IDF-Ausgangsadapter fuer die Onboard-MOSFET-
Kanaele, der den bestehenden Port `device_platform::IBinaryOutputSink`
implementiert, und dessen Anbindung in der Composition Root fuer Innenluefter
(GPIO16), Aussenluefter (GPIO17) und Summer (GPIO26) — **ohne** dass die
unbestaetigte Polaritaet geraten wird.

Nicht-Ziele: kein BTS7960-/Peltier-Code (#33); keine Aktorfreigabe
(`ActuatorSafetyGateStatus::Allowed` wird nicht gesetzt); keine Aenderung von
Boardprofil, GPIO-Matrix, Polaritaeten oder Pull-Beschaltung; GPIO27 (Reserve) wird
nicht belegt; keine neue Service-UI; keine Verdrahtung des Planers/der Regelschleife
(die Regelschleife ist im Produkt noch nicht komponiert, #90); keine Ton-/PWM-Funktion;
keine Hardwaretests oder Flashes; keine neue Bibliothek (`esp_driver_gpio` ist
bereits Abhaengigkeit von `device_platform_esp_idf`).

## 2. Verifizierte Ausgangslage

```text
BASELINE=main df5a3ddf41889b46f9b4d3c64085092a88e25a0a
ISSUE_32=OPEN, BLOCKED_HARDWARE, SSOT_CONFORMANCE=PENDING, FUNCTIONAL_HARDWARE_VERIFICATION=PENDING
```

- Port: `lib/device_platform/src/binary_output_sink.hpp` (`setEnabled(bool)`, rollenfrei).
- Verhalten ist bereits vorhanden und wird nur **konsumiert**: `ActuatorPlanSinkDriver`
  (`fermentation_app`) setzt Aussenluefter und Innenluefter ueber zwei
  `IBinaryOutputSink`; Aussenluefter bei Heizen/Kuehlen unbedingt an, `Unknown`/
  ungueltige Richtung alles aus. Nachlauf-/Dauerbetriebsregeln liegen im Planner
  (`outerFanPostRunMillis`, `innerFanPostRunMillis`) und bleiben unberuehrt.
- Das Produkt komponiert Planner/Driver/Orchestrator noch nicht (`main/app_main.cpp`
  ohne `ActuatorPlanSinkDriver`; nur `issue_29_bringup_probe.cpp` mit `AllOff*`-Sinks).
  Ein produktiver Aufrufer von `setEnabled(true)` existiert nicht und entsteht durch
  #32 nicht.
- SSOT `config/board_profiles/esp32_32e_quad_mosfet_r1.yaml`: GPIO16 `internal_fan`,
  GPIO17 `external_heatsink_fan`, GPIO26 `active_buzzer`, GPIO27 `reserve`; fuer alle
  vier `active_level`, `safe_boot_level` = `TBD_HARDWARE`, `external_bias` =
  `TBD_BOARD_CIRCUIT`, `assignment_status` = `board_fixed_pending_functional_verification`.
- Pinquelle: `scripts/generate_board_profile_header.py` erzeugt
  `main/generated/board_profile_r1.hpp` (Regel aus #130/#31: keine zweite
  handgepflegte Pinliste). Der Generator weist heute Pins mit
  `board_fixed_pending_functional_verification` bewusst ab und kennt nur acht
  Display-/Touch-Pins.
- Adaptervorbild: `EspIdfDisplayTouchAdapter` (Backlight-GPIO mit `gpio_config`/
  `gpio_set_level`); ESP-Adapter werden nicht nativ gebaut (`lib_ignore =
  device_platform_esp_idf`), portable Logik ist nativ testbar.
- Summer: SSOT `active_buzzer`; `docs/HARDWARE.md`: aktiver Summer geplant, aktiver
  Pegel/Gate-Beschaltung/Boot-Wirkung offen; `docs/RUNTIME_BEHAVIOR.md`: konkrete
  Summermuster, Wiederholung, Quittierung offen; kein Dauerton; Stummschaltung.

## 3. Kernentwurf: nicht-aktivierender Zustand ohne Polaritaetsannahme

Software kann ohne bekannte Polaritaet **keinen** Pegel treiben, ohne zu raten. Der
einzige nachweislich nicht-aktivierende Softwarezustand ist: **die GPIOs nicht
anfassen**. Belegt (Quellstand): kein Pfad in `main/`, `lib/` oder `sdkconfig*`
konfiguriert GPIO16/17/26/27 heute (`git grep`; `CONFIG_SPIRAM` ist nicht gesetzt, die
Pins gehen also nicht an PSRAM). Der Hardware-Resetzustand dieser Pads (Eingang,
Pulls) wird **nicht** vorausgesetzt; er ist H2 und wird im TRM/Datenblatt nur als
Zusatzbeleg gelesen.

Ein Adapter `EspIdfBinaryOutputSink(pin, polarity)` (ein Port, eine Klasse, kein
neuer Port) implementiert `IBinaryOutputSink`:

```text
OutputPolarity { Unconfirmed, ActiveHigh, ActiveLow }

Konstruktor: kein Hardwarezugriff
begin():
  Unconfirmed          -> KEIN GPIO-Aufruf, Zustand "nicht gestartet"
  ActiveHigh/ActiveLow -> inaktiven Pegel vorsetzen (gpio_set_level), dann
                          gpio_config als Ausgang, ohne Pull-up/Pull-down;
                          Fehler -> Faulted (EIN wird verworfen)
setEnabled(x):
  nicht gestartet | Unconfirmed | Faulted -> EIN verworfen
  bereit -> gpio_set_level(pegel(x, polarity)); Fehler -> Faulted
setEnabled(false) bereit/Faulted -> best effort inaktiver Pegel
```

`inactiveLevel`: ActiveHigh -> LOW, ActiveLow -> HIGH. Verbote im Adapter: kein
`gpio_reset_pin()` (in der ESP-IDF-6.1-Quelle `components/esp_driver_gpio/src/gpio.c`
aktiviert er fuer ausgangsfaehige Pins den internen Pull-up und wuerde den
`Unconfirmed`-Zustand veraendern), kein Pull, kein Zeit-/Nachlauf-Timer (der zwingende
Aussenluefter-Nachlauf bleibt Planner-/Driver-Vertrag; der Adapter schaltet nur auf
Befehl), keine Rollennamen.

Eine Polaritaet wird nie im Code geraten: sie kommt ausschliesslich aus der SSOT
(`active_level` `high`/`low`); `TBD_HARDWARE` ergibt `Unconfirmed`. Eine SSOT-Aenderung
ist ein eigener Plan-/Owner-Gate-Scope.

Harte Grenze (Hardware-Gate): `Unconfirmed` heisst „Software treibt nichts“, nicht
„Verbraucher ist aus“. Ob ein Verbraucher dann und im Boot-/Resetfenster vor `app_main`
tatsaechlich aus ist, haengt an der unbekannten Platinenbeschaltung (`external_bias`,
`TBD_BOARD_CIRCUIT`) und wird nur im Hardwaretest beurteilt.

**Vorbedingung fuer #33/#90 (nicht in #32 geloest):** `setEnabled` ist `void`; ein
verworfenes EIN (Faulted/Unconfirmed) ist fuer den Driver unsichtbar. Sobald ein echter
H-Bruecken-Sink existiert, koennte der Peltier ohne laufenden Aussenluefter freigegeben
werden. Vor der ersten realen Peltierfreigabe muss der Interlock-/Safety-Pfad einen
fehlgeschlagenen oder unbestaetigten Aussenluefter erkennen (Interlock-Scope, #33/#35/
#90). Der Adapter bietet dafuer bewusst keinen Zustandsabfrage-Vorbau.

## 4. Schnitte und Module (ADR-013)

| Modul | Neu | Inhalt |
|---|---|---|
| `device_platform` | `output_polarity.hpp` | nur `enum class OutputPolarity` (rollen- und anwendungsneutral) |
| `device_platform_esp_idf` | `esp_idf_binary_output_sink.{hpp,cpp}` | Adapter laut Abschnitt 3 |
| `scripts` + `main/generated` | Generator erweitert, Header neu erzeugt | Pins 16/17/26 und je `OutputPolarity` aus der SSOT (nach `application_role`); GPIO27 wird nicht erzeugt; die acht bestehenden Konstanten bleiben byte-identisch |
| `main/app_main.cpp` (Composition Root) | drei Adapterinstanzen | Rollenzuordnung (Innen-/Aussenluefter/Summer) ausschliesslich hier; `begin()` frueh im Boot; keine Anbindung an Planner/Driver |
| `test/esp_idf_binary_output_sink_host` | Linux-Target-Hosttest | Adapter gegen den von ESP-IDF mitgelieferten CMock des GPIO-Treibers (`$IDF_PATH/tools/mocks/driver`) |

`fermentation_app` wird nicht geaendert. Keine neue Bibliothek (`esp_driver_gpio` ist
bereits Abhaengigkeit).

Seam-Befund (Grundlage fuer O2): Es existiert kein portables Testfenster fuer GPIO-
Adapter in PlatformIO nativ (`lib_ignore = device_platform_esp_idf`). Dagegen gibt es
das Muster `test/esp_idf_*_host` (Linux-Target-Projekte, z. B. `esp_idf_http_server_
adapter_host`, `esp_idf_nvs_adapter_host`) und ESP-IDF liefert einen CMock fuer
`driver/gpio.h`. Die vorhandenen Host-Projekte werden per README-Befehl manuell
gebaut und sind **nicht** Teil von `scripts/run_pre_ready_gates.sh`; das gilt dann
auch fuer den neuen Test (siehe O2). Die Machbarkeit (CMock bindet `gpio_config`/
`gpio_set_level` in ein Linux-Target-Projekt) wird als erster Schritt von S1 belegt.

## 5. Summer: Abgrenzung

Innerhalb vorhandener Vertraege wird nur der binaere Kanal (EIN/AUS ueber
`IBinaryOutputSink`) bereitgestellt. Aktiver Summer (SSOT) wird mit konstantem Pegel
betrieben; ein passiver Piezo braeuchte Tonfrequenz/PWM und ist **nicht** Teil dieses
Plans (H5). Es gibt keinen vorhandenen Vertrag fuer Summermuster, Quittierung oder
Prioritaetsabbildung (offen in `RUNTIME_BEHAVIOR.md`); sie werden hier nicht
erfunden. Die Akzeptanz „nichtblockierende Summermuster“ bleibt dadurch als **offener
#32-Punkt** stehen, bis ein Musterspieler entschieden wird (O3).

## 6. Tests

Neu, Linux-Hosttest `esp_idf_binary_output_sink_host` (CMock-GPIO, Aufrufreihenfolge
und -argumente):

- `Unconfirmed`: `begin()` und beliebige `setEnabled`-Folgen erzeugen **null**
  GPIO-Aufrufe (Kerntest „nicht aktivierend“; ein unerwarteter Aufruf scheitert).
- `ActiveHigh`/`ActiveLow`: `begin()` setzt zuerst den inaktiven Pegel, danach
  `gpio_config` (Ausgang, ohne Pulls); EIN/AUS-Pegelabbildung; vor `begin()` wird EIN
  verworfen; Konstruktor ohne GPIO-Aufruf; kein `gpio_reset_pin`.
- Fehler: `gpio_config`-Fehler -> EIN verworfen, kein weiterer Pegelaufruf; `gpio_set_level`-
  Fehler -> EIN verworfen; AUS versucht den inaktiven Pegel.
- Kanaltrennung: drei Instanzen mit verschiedenen Pins beeinflussen sich nicht.

Bestehende Nachweise fuer das Verhalten bleiben unveraendert und werden nur als
Regression ausgefuehrt: `test_actuator_plan_sink_driver` (Mapping Heizen/Kuehlen/Idle/
`Unknown`, Aussenluefter unbedingt an), `test_actuator_planner` (Nachlauf/
Dauerbetrieb), `test_actuation_interlock`, `check_architecture_boundaries.py`,
Board-Header-`--check`/`--selftest` (neuer SSOT-Fall: `TBD_HARDWARE` -> `Unconfirmed`,
`high`/`low` -> `ActiveHigh`/`ActiveLow`, ungueltiger Wert -> Fehler). Die
Kombination Driver -> Adapter ist ueber den Port `IBinaryOutputSink` bereits vertraglich
abgedeckt; ein nativer Test mit dem ESP-Adapter ist nicht moeglich und wird nicht
vorgebaut.

Build beider ESP-IDF-Profile (`bringup`, `release`) ueber
`scripts/build_esp_idf_profiles.py`. Pre-Ready-Gate erst nach Review und Ownerfreigabe.

Alle Hardwaretests bleiben `NOT_RUN`: kein Flash, kein Schalten von Verbrauchern.

## 7. Umsetzungsschnitte (je ein Commit, danach Stopp zur Ownerpruefung)

1. S1: `OutputPolarity`, Adapter, Linux-Hosttest inkl. Machbarkeitsbeleg des CMock.
   Scheitert die Machbarkeit, stoppt der Builder und legt den Befund mit der
   Portvariante (O2 = B) vor.
2. S2: Generator/Header (Pins + Polaritaet aus der SSOT, SIM-/Selftestfaelle),
   Composition Root, Doku (`ARCHITECTURE.md`, `ACCEPTANCE_TESTS.md` mit neuen
   Nachweiskennungen `SIM-32-01..SIM-32-05` fuer Unconfirmed-Null-Aufrufe,
   Pegelabbildung, Fehlerpfad, Kanaltrennung, SSOT-Ableitung; ROADMAP).
3. S3: Builder-Self-Check, Build beider Profile, Beweisdokumentation (nur tatsaechlich
   ausgefuehrte Nachweise mit exaktem HEAD; Hardware `NOT_RUN`).

## 8. Hardware-/Owner-Gates (ausserhalb dieses PR)

- **H1 Polaritaet:** wird im owning Hardwaretest funktional bestimmt (`HARDWARE.md`).
  Ein Verfahren mit sicherer Einzel-/Ersatzlast legt der Owner/Hardwareplan fest; das
  Ergebnis aendert die SSOT in einem eigenen Plan-/Owner-Gate-Scope.
- **H2 Boot/Reset:** Verhalten vor `app_main` sowie bei Brownout/Bootloader haengt an der
  Platinenbeschaltung; nur funktional mit sicherem Einzelverbraucher.
- **H3** Kanal/Verbraucher, **H4** Aussenluefter-Nachlauf real, **H5** Summertyp
  (aktiv/passiv), Lautstaerke, Montageort.

## 9. Offene Ownerentscheidungen

- **O1a** Pins und Polaritaet kommen aus der SSOT ueber den erweiterten Generator
  (Empfehlung A). Alternative B waere eine handgeschriebene Pinliste in `app_main`
  (widerspricht der #130/#31-Regel „keine zweite Pinliste“).
- **O1b** Welche SSOT-Felder muessen bestaetigt sein, bevor der Generator `ActiveHigh/
  ActiveLow` ausgibt? Empfehlung: nur `active_level` = `high`/`low`.
  `assignment_status` bleibt bis zur funktionalen Pruefung `pending` (zirkulaer, wenn
  verlangt); `safe_boot_level`/Beschaltung sind H2. Strengere Bedingungen sind eine
  Ownerentscheidung.
- **O2** Testweg: A) ein Adapter plus Linux-Hosttest mit ESP-IDF-CMock (Empfehlung; nutzt
  das vorhandene `test/esp_idf_*_host`-Muster, kein neuer Port, aber **nicht** im
  Pre-Ready-Gate — Aufnahme ins Gate waere ein eigener Gate-Scope); B) zusaetzlicher
  kleiner Port `IDigitalOutputLine` mit portabler Polaritaetsklasse und Mock, damit die
  Polaritaetslogik in den nativen Gate-Tests laeuft (mehr Struktur, ein weiterer Port).
- **O3** Summermuster: B) kein Musterspieler in #32; „nichtblockierende Summermuster“
  bleibt offen bis Muster/Quittierung spezifiziert sind (Empfehlung, da kein Vertrag
  vorhanden und keine Zukunftsfunktion vorgebaut wird). A) generischer, nichtblockierender
  Musterspieler (Schrittfolge/Limits vom Aufrufer, Zeit injiziert, endet immer inaktiv,
  keine konkreten Muster) als Zusatzschnitt S3b.
- **O4** Der Composition Root erzeugt die drei Ausgaenge und ruft `begin()` am Boot, obwohl
  kein Planner angebunden ist (Empfehlung: ja — `begin()` ist der Boot-Zustandsanker;
  bei `Unconfirmed` ohne Hardwarewirkung).

## 10. Risiken

- `Unconfirmed` kann einen mit externem Pull aktiven Verbraucher nicht verhindern (H2).
- Solange die SSOT `TBD_HARDWARE` fuehrt, ist der Kanal funktionslos; Funktionspruefung
  ist ohne Polaritaetsbestimmung (H1) nicht moeglich.
- Generatorerweiterung beruehrt eine geteilte Quelle; die acht bestehenden Konstanten
  muessen byte-identisch bleiben (Nachweis: `--check`).
- Der Linux-Hosttest ist nicht gate-gefuehrt (O2 A): Regressionen im Adapter werden nur bei
  manuellem Lauf erkannt.
