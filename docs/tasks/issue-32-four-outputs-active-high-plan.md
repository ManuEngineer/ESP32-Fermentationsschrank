# Issue #32 – vier Onboard-MOSFET-Ausgaenge einheitlich ActiveHigh (Plan, Revision 1)

Status: **Freigabe dieser Plan-Commit-SHA durch den Owner ausstehend.** Es wird
nichts umgesetzt, bevor die exakte SHA freigegeben ist.

Gilt nur fuer Teil A des Ownerauftrags (Issue #32, Software-/Boardprofilschnitt
plus minimaler Luefter-Betriebscheck). Teil B (#190, DS18B20) und Teil C
(Produktintegration, #34-#37) sind eigene Issues/PRs mit eigenem Plan und
beginnen erst nach Merge dieses PR. `ACTUATOR_RELEASE=NO` bleibt unveraendert.

## 1. Ziel und Nicht-Ziele

Ziel: Die vier baugleichen MOSFET-Kanaele des ESP32-MOS-X4 (GPIO16/17/26/27)
werden in der SSOT einheitlich `active_level: high` gefuehrt; die Firmware
initialisiert alle vier im normalen App-Start auf LOW/AUS; beide angeschlossenen
Luefter werden nach der Firmwareinitialisierung einmal AUS/EIN/AUS geprueft.

Nicht-Ziele: kein Peltier-/BTS7960-Betrieb, keine Planneranbindung, keine
`Allowed`-Freigabe, kein neuer Treiber, kein generisches Ausgangsframework,
keine zweite Pinliste, keine Einzelpruefserie je Kanal, kein Test von Summer
(GPIO26) und Reserve (GPIO17), keine Aussage zu elektrischer Konformitaet oder
Ausfallsicherheit bei MCU-Haenger.

## 2. Ausgangslage (verifiziert gegen `origin/main` `09b704e`)

- SSOT `config/board_profiles/esp32_32e_quad_mosfet_r1.yaml`: GPIO16
  `internal_fan` bereits `active_level: high` (PR #212); GPIO17
  `external_heatsink_fan`, GPIO26 `active_buzzer`, GPIO27 `reserve` stehen auf
  `TBD_HARDWARE` (`Unconfirmed` -> der Adapter fuehrt keine GPIO-Operation aus).
- `scripts/generate_board_profile_header.py`: `OUTPUT_ROLES` kennt drei Rollen;
  die Reserve wird bewusst nicht erzeugt (Docstring Zeile ~49, Header-Kommentar,
  Selftest nutzt `reserve` als Fixture-Rolle, Zeilen ~394/589).
- `main/app_main.cpp` (~Zeile 600): drei `EspIdfBinaryOutputSink` fuer
  inner/outer/buzzer, `begin()` mit Log; keine Reserve-Instanz.
- Physisch angeschlossen: Innenluefter an OUT1/GPIO16, **zweiter Luefter an
  OUT4/GPIO27**; OUT2 und OUT3 unbestueckt (Evidence
  `docs/audits/ISSUE32_BOOT_DIAGNOSIS_EVIDENCE.md`). LOW=AUS ist fuer
  OUT1/GPIO16 und OUT4/GPIO27 real beobachtet, `POST_BOOT_OUT1_OFF=PASS`.
- Planner/Driver sind im Produkt nicht komponiert; es gibt keinen produktiven
  Aufrufer von `setEnabled(true)`. Vorhanden fuer Bring-up: `main/issue_29_bringup_probe.cpp`
  (`AllOffBinarySink`), `issue_30_sensor_commissioning_harness.cpp` (UART).

## 3. Owner-Scopeentscheidung (hervorgehoben, nicht still geaendert)

> **NEUE OWNER-SCOPEENTSCHEIDUNG (Auftrag vom 2026-11-18):** Die Rollenzuordnung
> im Boardprofil wird geaendert. `external_heatsink_fan` wandert von GPIO17
> (OUT2) auf **GPIO27 (OUT4)**, `reserve` von GPIO27 auf **GPIO17 (OUT2)**.
> Begruendung: der zweite Luefter haengt physisch an OUT4; statt Umverdrahtung
> wird die SSOT angepasst. OUT3/GPIO26 bleibt `active_buzzer` (unbestueckt).
> `config/board_profiles/…r1.yaml` und die Zuordnungsvermerke in
> `docs/HARDWARE.md`/`docs/ROADMAP.md` werden im selben Commit angepasst; die
> historischen Plaene (#130, #132) bleiben als Audit unveraendert.

Ownerakzeptierte Uebertragung: der LOW=AUS-Nachweis von OUT1/OUT4 gilt fuer die
baugleichen OUT2/OUT3 mit; `ActiveHigh` fuer alle vier. Restrisiko (Luefter
laeuft bei Boot/gehaltenem EN-Reset; MCU-Haenger auf LOW haelt AUS, auf HIGH
nicht ausgeschlossen) ist fuer R1 akzeptiert und wird nicht als Konformitaet
dargestellt.

## 4. Umsetzungsschnitte

**C1 – SSOT + Generator + Header (ein Commit).**
- YAML: GPIO16/17/26/27 `active_level: high`; Rollen tauschen wie oben.
  `safe_boot_level`/`external_bias` bleiben `TBD_*` (nicht gemessen, nicht
  geraten); `assignment_status` bleibt `board_fixed_pending_functional_verification`.
- Generator: `OUTPUT_ROLES` um `reserve` -> `kReservePin`/`kReservePolarity`
  erweitern (eine Zeile plus Header-Kommentar/Docstring); Selftest anpassen
  (neue Pin-Erwartungen 27/17, Reserve nun erzeugt; die Fixture, die `reserve`
  als Ersatzrolle benutzt, wird umgeschrieben statt ausgenommen).
  `main/generated/board_profile_r1.hpp` neu erzeugen (`--check` gruen).
- Test: bestehender Generator-Selftest; kein neuer Test ausser Erwartungen der
  vier Pins/Polaritaeten.

**C2 – Composition Root (ein Commit).**
- `app_main.cpp`: vierte `EspIdfBinaryOutputSink reserveOutput(kReservePin,
  kReservePolarity)` neben den drei bestehenden; `begin()` fuer alle vier,
  gemeinsames Log. Die Reserve bleibt unverbunden (kein Konsument). Kein neuer
  Treiber/Port. Alle vier im normalen Start auf LOW (inaktiv).
- Test: bestehender `esp_idf_binary_output_sink_host` (ActiveHigh-Pfad ist dort
  abgedeckt) und Build des Produktimages; ggf. ein kleiner Smoke-Fall mit
  GPIO27 `ActiveHigh`. Keine vierfache Wiederholung.

**C3 – Betriebscheck der beiden Luefter (Hardware, ohne Produktcode-Erweiterung).**
Vorschlag (Owner-Entscheidung D1): im **bereits vorhandenen Bring-up-Profil**
(`esp32_bringup`, UART-Harness-Muster) ein kurzer, bring-up-only Pfad, der nach
`begin()` je Luefter einmal `setEnabled(true)` fuer wenige Sekunden und danach
`setEnabled(false)` ausfuehrt; der Pfad ist per Kconfig/Profil nicht im
Release-Image (wie der #30-Harness). Ablauf: Flash (gesondertes Ownergate,
Restore wie in der #212-Evidence), Normalstart -> beide AUS (Owner-Beobachtung
+ UART), dann pro Luefter EIN -> AUS. Ergebnis wird als `HIGH_EIN` fuer
OUT1/OUT4 bzw. NOT_RUN dokumentiert; OUT2/OUT3 `NOT_RUN` (ownerakzeptierte
Uebertragung). Evidence in `docs/audits/ISSUE32_BOOT_DIAGNOSIS_EVIDENCE.md`.

**C4 – Doku.** ROADMAP-Zeile #32, ACCEPTANCE_TESTS (SIM-32-05 anpassen:
GPIO16/17/26/27 + Reserve), HARDWARE.md Kanalzuordnung. Statuswerte:
`HIGH_EIN` nur nach Betriebscheck PASS; `SSOT_CONFORMANCE`/
`FUNCTIONAL_HARDWARE_VERIFICATION` werden **nicht** pauschal auf PASS gesetzt,
sondern auf den real erreichten Umfang (beide Luefter) beschraenkt benannt.

## 5. Tests und Nachweise

Lokal, gezielt: `python3 scripts/generate_board_profile_header.py --selftest`
und `--check`; Host-Test `esp_idf_binary_output_sink_host`; `pio run`/idf-Build
des Release- und Bring-up-Profils; direkt betroffene Konsumenten
(`check_architecture_boundaries.py`). Hardware nur C3. Nicht ausgefuehrte
Tests sind NOT_RUN. Voller Pre-Ready-Lauf erst nach Independent Review auf
Owner-Anweisung.

## 6. Risiken

- HIGH=EIN-Wirkung ist unbelegt; faellt C3 negativ aus, ist die Annahme
  "alle vier ActiveHigh" widerlegt -> Plan zurueck an den Owner, nichts raten.
- Rollenwechsel: alle Verweise auf GPIO17/27 gezielt per `git grep` abgleichen
  (Docs, Tests, Selftest); Audit-Plaene bleiben unveraendert.
- Boot/EN-Reset: Luefter kann laufen (akzeptiert); Peltier bleibt davon
  unberuehrt AUS und gesperrt.

## 7. Offene Owner-Entscheidungen

- **D1 (Empfehlung: wie C3):** Bring-up-only Kurz-EIN/AUS-Pfad im bestehenden
  Bring-up-Profil. Alternative: bestehende UART-Harness erweitern. Beides
  bleibt aus dem Release-Image.
- **D2:** Freigabe dieser Plan-SHA und des Rollentauschs (Abschnitt 3).
- Flash/Hardwarelauf C3 bekommt ein gesondertes Ownergate unmittelbar vor
  Ausfuehrung.
