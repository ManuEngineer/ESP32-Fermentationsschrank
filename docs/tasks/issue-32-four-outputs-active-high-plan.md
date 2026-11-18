# Issue #32 – vier Onboard-MOSFET-Ausgaenge einheitlich ActiveHigh (Plan, Revision 2)

Status: **Freigabe dieser Plan-Commit-SHA durch den Owner ausstehend.** Es wird
nichts umgesetzt, bevor die exakte SHA freigegeben ist. Revision 2 ersetzt
Revision 1 (`a5495dc`) vollstaendig; deren Rollentausch GPIO17/GPIO27 ist
zurueckgenommen und wird nicht zur Freigabe gestellt.

Gilt nur fuer Teil A des Ownerauftrags (Issue #32). Teil B (#190, DS18B20) und
Teil C (Produktintegration, #34-#37) sind eigene Issues/PRs mit eigenem Plan.
`ACTUATOR_RELEASE=NO` bleibt unveraendert.

## 1. Ziel und Nicht-Ziele

Ziel (**jetzt, #214**): korrekte produktive Pin-/Rollen- und Polaritaetszuordnung
(`ActiveHigh` fuer alle vier PCB-Kanaele), GPIO-AUS-Initialisierung aller vier
Kanaele im normalen App-Start und ein manuell ausloesbarer, kurzer
HIGH=EIN-/LOW-Test, ohne pauschale Aktorfreigabe.

Nicht-Ziele: **keine geregelte Luefterfunktion** (siehe §6); kein Rollen-, GPIO-,
Kabel- oder Architekturwechsel; kein Peltier-/BTS7960-Betrieb; keine
`Allowed`-Freigabe; kein neuer Treiber, keine neue GPIO-Abstraktion, kein
generisches Ausgangsframework, keine zweite Pinliste; keine vierfache
Einzeltestkampagne; kein Test des Summers (OUT3/GPIO26); keine Aussage zu
elektrischer Konformitaet oder Ausfallsicherheit bei MCU-Haenger.

## 2. Verbindliche Belegung (unveraendert) und Testaufbau

| PCB-Ausgang | GPIO | `application_role` |
|---|---:|---|
| OUT1 | 16 | `internal_fan` |
| OUT2 | 17 | `external_heatsink_fan` |
| OUT3 | 26 | `active_buzzer` |
| OUT4 | 27 | `reserve` (NC im Endaufbau) |

Der aktuelle Testaufbau (Luefter an OUT1 und OUT4) besteht aus **temporaeren
Funktionspruefasten**, keiner verbindlichen Produktverdrahtung; daraus folgt
keine Aenderung der Tabelle. Der OUT4-Test ist ein **Reservekanal-Hardwaretest**
und beweist keine produktive Aussenluefterfunktion von OUT2.

## 3. Verifizierte Ausgangslage (`origin/main` `09b704e`)

- SSOT `config/board_profiles/esp32_32e_quad_mosfet_r1.yaml`: GPIO16 bereits
  `active_level: high` (PR #212); GPIO17/26/27 `TBD_HARDWARE` (`Unconfirmed` ->
  der Adapter fuehrt keine GPIO-Operation aus, dieser Zustand wird beendet).
- `scripts/generate_board_profile_header.py`: `OUTPUT_ROLES` kennt drei Rollen;
  die Reserve wird bewusst nicht erzeugt (Docstring, Header-Kommentar; Selftest
  nutzt `reserve` als Fixture-Rolle).
- `main/app_main.cpp` (~Z. 600): drei `EspIdfBinaryOutputSink` fuer
  inner/outer/buzzer; keine Reserve-Instanz; keine Verbindung zu
  `ActuatorPlanSinkDriver`/`ActuatorPlanner`.
- Real beobachtet: LOW=AUS an OUT1/GPIO16 und OUT4/GPIO27,
  `POST_BOOT_OUT1_OFF=PASS`; Evidence `docs/audits/ISSUE32_BOOT_DIAGNOSIS_EVIDENCE.md`.
  OUT2/OUT3 uebernehmen die Polaritaet per Ownerentscheidung als begruendete
  Gleichheitsannahme, **nicht** als Einzelmess-PASS.
- Bring-up-Muster vorhanden: `issue_29_bringup_probe.cpp`,
  `issue_30_sensor_commissioning_harness.cpp` (UART, nur `esp32_bringup`).

## 4. Umsetzungsschnitte

**C1 – SSOT, Generator, Header (ein Commit).**
- YAML: GPIO17/26/27 `active_level: high` (GPIO16 bleibt). `safe_boot_level`,
  `external_bias` und `assignment_status` bleiben unveraendert (`TBD_*` bzw.
  `board_fixed_pending_functional_verification`); nichts davon wird als
  bestaetigt ausgegeben. Rollen und GPIO-Zuordnung bleiben unveraendert.
- Generator: `OUTPUT_ROLES` um `reserve` -> `kReservePin`/`kReservePolarity`
  (**eine** Konstante) erweitern; Docstring/Header-Kommentar anpassen; Selftest
  anpassen (alle vier `ActiveHigh`, Reserve nun erzeugt; die Fixture, die
  `reserve` als Ersatzrolle benutzt, wird umgeschrieben statt ausgenommen).
  Header neu erzeugen, `--check` gruen.

**C2 – Composition Root (ein Commit).**
- `app_main.cpp`: **eine** unverbundene `EspIdfBinaryOutputSink reserveOutput`
  mit `kReservePin`/`kReservePolarity`; `begin()` fuer alle vier, gemeinsames Log.
  Inner/Outer/Buzzer bleiben an 16/17/26. Die Reserve wird nicht als
  Aussenluefter oder anderer Verbraucher komponiert. Alle vier starten auf
  LOW/AUS. Booten/gehaltener EN-Reset: Luefter duerfen laufen (Ownerakzeptanz).

**C3 – manueller Bring-up-only-HIGH=EIN-Test (Code ein Commit, Lauf gesondert).**
- Nur im Profil `esp32_bringup` (wie #30-Harness, nicht im Release-Image), per
  eindeutigem UART-Befehl **manuell** ausgeloest; **kein** automatischer
  EIN-Test beim Boot.
- Pro tatsaechlich benutzter Pruefast (OUT1 und OUT4) hoechstens **ein** kurzer
  EIN/AUS-Zyklus; danach LOW. Abbruch-/Fehlerverhalten: schlaegt `setEnabled`
  fehl oder bricht die UART/der Befehl ab, wird sofort LOW getrieben und
  `FAIL` gemeldet; ein Timeout (wenige Sekunden) erzwingt die Rueckkehr auf LOW;
  ein unbekannter Befehl aendert nichts. OUT2/OUT3: keine Einzeltestkampagne.
- Physischer Flash/Schaltversuch nur nach gesondertem Owner-Gate unmittelbar
  vor Ausfuehrung (Backup/Restore wie in der #212-Evidence). Ergebnis in die
  Evidence-Datei; `HIGH_EIN` nur fuer tatsaechlich geschaltete Kanaele PASS.
  Faellt der Test negativ aus, ist die Gleichheitsannahme widerlegt -> Plan
  zurueck an den Owner, keine geratene Korrektur.

**C4 – Doku.** ROADMAP-Zeile #32, ACCEPTANCE_TESTS (SIM-32-05: vier Pins, vier
`ActiveHigh`, Reserve erzeugt), HARDWARE.md. `SSOT_CONFORMANCE`/
`FUNCTIONAL_HARDWARE_VERIFICATION` werden nicht pauschal PASS, sondern auf den
real erreichten Umfang beschraenkt. Der offene Produktpfad (§6) wird in Roadmap
und Handover sichtbar festgehalten.

## 5. Tests und Nachweise

Lokal, gezielt: `generate_board_profile_header.py --selftest` und `--check`;
Host-Test `esp_idf_binary_output_sink_host` (ActiveHigh-Pfad; ggf. ein
Smoke-Fall mit GPIO27); Build Release- und Bring-up-Profil;
`check_architecture_boundaries.py`. Hardware nur C3. Nicht ausgefuehrte Tests
sind NOT_RUN. Voller Pre-Ready-Lauf erst nach Independent Review auf
Owner-Anweisung.

## 6. Abgrenzung: Bootinitialisierung ist keine Luefterregelung

Nach #214 sind Sink-Objekte erzeugt und AUS initialisiert, aber **nicht** mit
`ActuatorPlanSinkDriver`/`ActuatorPlanner` verbunden. Eine geregelte
Luefterfunktion entsteht dadurch **nicht**.

- **Jetzt (#214):** Pin-/Rollen-/Polaritaetszuordnung, GPIO-AUS-Init, manuell
  testbare HIGH/LOW-Funktion, keine pauschale Aktorfreigabe.
- **Offener Produktpfad nach der Sensorinbetriebnahme (#190):** vorhandene
  Fan-Sinks an den bestehenden Driver/Planner und die echte Sensor-/Safety-
  Evidenz anbinden; keine neue Treiberschicht, keine Bypass-Freigabe, kein
  Peltierbetrieb ohne dessen eigene Sicherheitsvoraussetzungen (#33).
  #214 wird nicht als fertige Luefterregelung deklariert.

## 7. Risiken und offene Entscheidungen

- HIGH=EIN fuer OUT1/OUT4 unbelegt bis C3; OUT2/OUT3 nur Gleichheitsannahme.
- Boot/EN-Reset: Luefter laufen moeglich (akzeptiert); Peltier bleibt AUS und
  gesperrt, Safetygrenze unberuehrt.
- Offene Ownerentscheidung: **nur** Freigabe dieser Plan-SHA (Rollentausch
  entfaellt). Das Hardware-Gate fuer C3 folgt separat vor dem Flash.
