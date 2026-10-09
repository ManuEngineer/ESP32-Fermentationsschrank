# Issue #33 – Execution-Evidence (laufend)

Plan: `docs/tasks/issue-33-bts7960-hbridge-plan.md`, Revision 2 (Freigabe `d55d2db`).
Dieses Dokument fuehrt nur tatsaechlich ausgefuehrte Nachweise je Schnitt mit dem
getesteten Code-Commit. Stand: **S1 und S2 umgesetzt**; S3–S5 nicht begonnen.

```text
ACTUATOR_RELEASE=NO   REAL_PELTIER_TEST=NOT_RUN
SSOT_CONFORMANCE=PENDING   FUNCTIONAL_HARDWARE_VERIFICATION=PENDING
ADAPTER_SAFETY_VERIFICATION=PENDING
Hardware, Flash, Pre-Ready-Lauf, Self-Check = NOT_RUN
```

## S1 – Portvertrag `setEnabled -> bool` und Aussenluefter-Sperre im Driver

```text
GETESTETER_CODE_COMMIT=d7515153c03b03c46763a56b8074f0722bdd64c4 (sauberer Arbeitsbaum, git status leer)
UMGEBUNG=Linux, PlatformIO 6.1.19 (-e native), ESP-IDF v6.1 (fff9895c82d744c7237be8847347bdd1b07c6643), Ruby 3.3.8 (CMock), clang-format 21.1.3
```

| Nachweis | Befehl | Ergebnis |
|---|---|---|
| Driver inkl. neuer Aussenluefter-Sperre | `pio test -e native -f test_actuator_plan_sink_driver` | PASS, 9 Faelle (vier neu: Sperre Heizen, Sperre Kuehlen, laufender Peltier wird bei abgelehntem Luefter abgeschaltet, Idle/Unknown/ungueltig bleiben AUS bei ablehnenden Sinks) |
| Mock (Ablehnungsschalter) | `pio test -e native -f test_sensor_actuator_mocks` | PASS, 16 Faelle (ein neuer: abgelehnte Befehle aendern Zustand/Journal nicht, `false`, Zaehler) |
| Konsumenten Driver+Mock | `pio test -e native -f test_configuration_service` | PASS, 47 |
| | `pio test -e native -f test_run_persistence_coordinator` | PASS, 163 |
| | `pio test -e native -f test_factory_reset_flow` (Quelltext-Guard `setEnabled(`) | PASS, 23 |
| Planner/Interlock/Regelung (Regression) | `pio test -e native -f test_actuator_planner` / `-f test_actuation_interlock` / `-f test_temperature_control` | PASS, 46 / 41 / 40 |
| Alle acht Suiten zusammen | `pio test -e native -f <acht Suiten>` | PASS, 385 Faelle, 0 Fehler |
| ESP-IDF-Linux-CMock-Hosttest, `bool`-Pfade | `idf.py -C test/esp_idf_binary_output_sink_host -B <build> build` und `<build>/issue32_binary_output_sink_host.elf` | PASS, 12 Tests, 0 Failures (Rueckgabewert in jedem Pfad geprueft: Unconfirmed/NotStarted/Faulted/ungueltiger Pin = `false`, bereit = `true`, Treiberfehler = `false`, Faulted-AUS = `false` ohne Entriegelung) |
| ESP-IDF-Profile, Bring-up-Probe kompiliert | `python3 scripts/build_esp_idf_profiles.py all` | PASS (`esp32_bringup` und `esp32_release`, keine Compiler-Warnung im Log; `issue_29_bringup_probe.cpp` ist im Standardprofil `esp32_bringup` Teil des Builds) |
| Architekturgrenzen | `python3 scripts/check_architecture_boundaries.py` | PASS |
| Format | `clang-format --dry-run -Werror` (geaenderte C++-Dateien), `git diff --check` | PASS |

Mutationsproben (jeweils Original danach wiederhergestellt): Driver-Sperre entfernt →
drei Driver-Tests schlagen fehl; Best-effort-AUS im `Faulted`-Zustand liefert `true` →
drei Hosttests schlagen fehl.

`AllOffBinarySink` (Bring-up-Probe, `main/issue_29_bringup_probe.cpp`, ESP-only):
- Code: `setEnabled(true)` liefert `false` (EIN nie als angewendet), `setEnabled(false)`
  liefert `true`; `releaseObserved` bleibt wie bisher.
- Build: PASS (siehe Profilbuild). Laufzeitverhalten: **NOT_RUN** — die Klasse liegt in
  `main/` hinter ESP-Includes und ist nativ nicht testbar; ihre Ausfuehrung waere ein
  Hardwarelauf der Probe und ist nicht Teil dieses Schnitts. Abgedeckt ist sie damit nur
  durch Compile und Code-Review.

Nicht ausgefuehrt (`NOT_RUN`): Builder-Self-Check (`run_pre_ready_gates.sh self-check`)
und vollstaendiger Pre-Ready-Lauf (S5 bzw. Ownerfreigabe), alle Hardwarenachweise.
Unveraendert offen laut Plan: Meldung eines Luefterfehlers nach oben, Klassifikation und
Wiederanlauf (#35/#90).

## S2 – `SharedEnableBridgeSink` (portabel, `device_platform`)

```text
GETESTETER_CODE_COMMIT=e850e1acfb738409e667d41da9706cca00f33446 (sauberer Arbeitsbaum)
UMGEBUNG=Linux, PlatformIO 6.1.19 (-e native), ESP-IDF v6.1, clang-format 21.1.3
```

| Nachweis | Befehl | Ergebnis |
|---|---|---|
| Bruecken-Logik (neu) | `pio test -e native -f test_shared_enable_bridge_sink` | PASS, 22 Faelle |
| Regression Driver/Mock/Planner/Interlock | `pio test -e native -f test_actuator_plan_sink_driver -f test_sensor_actuator_mocks -f test_actuator_planner -f test_actuation_interlock` | PASS, 112 Faelle |
| Architekturgrenzen (Bruecke rollen- und ESP-frei) | `python3 scripts/check_architecture_boundaries.py` und `--selftest` | PASS |
| ESP-IDF-Profile | `python3 scripts/build_esp_idf_profiles.py all` | PASS (keine Compiler-Warnung im Log) |
| Format | `clang-format --dry-run -Werror`, `git diff --check` | PASS |

Abgedeckt durch `test_shared_enable_bridge_sink` (Befehlsfolgen ueber ein gemeinsames
Ablaufprotokoll mit Ablehnungsschalter je Ausgang): `begin()` mit drei bereiten Ausgaengen
(AUS-Folge Enable, Vorwaerts, Rueckwaerts) und mit je einem nicht initialisierten
Ausgang (Enable, Vorwaerts, Rueckwaerts einzeln → nicht Ready, zwei Versuchsrunden,
keine Aktivierung danach; ein `setEnabled(false)` auf nicht initialisiertem Ausgang zaehlt
nicht als All-off-Initialisierung); Aufrufe vor `begin()`; EIN-Folge (Schenkel zuerst,
Enable zuletzt) und AUS-Folge (Enable zuerst) fuer beide Richtungen; Idempotenz;
Mutual Exclusion (beide Reihenfolgen) mit Abschaltung und Verriegelung;
Break-before-make (Richtungswechsel nur ueber All-off, auch bei unmittelbar folgendem
Gegenrichtungsbefehl; fehlgeschlagenes AUS blockiert die neue Richtung); Fehler an jedem
Einzelbefehl bei EIN und bei AUS (Schenkel, Enable, beide Richtungen) → Abschaltrunde,
Verriegelung, keine Reaktivierung auch nach Erholung des Ausgangs; Best-effort-AUS im
`Faulted`-Zustand ohne Entriegelung; Abschaltung versucht jeden Ausgang einmal auch bei
ablehnenden Ausgaengen; Sitzungs-Invarianten ueber die gesamte Befehlsfolge (nie beide
Schenkel EIN, Schenkel nur bei ausgeschaltetem Enable EIN).

Mutationsproben (Original danach wiederhergestellt, Quelle diff-identisch): Konfliktpruefung
entfernt → 4 Tests schlagen fehl; `begin()` ignoriert Ergebnisse → 3; Enable vor Schenkel
einschalten → 4; Schenkel vor Enable ausschalten → 7.

Abgrenzung: Die Bruecke erzwingt Reihenfolge und Konflikte, keine Dauer (kein Zeitgeber);
Mindest-Auszeit und Polaritaetswechsel-Totzeit bleiben im Planner. „AUS“ ist ein
softwareseitiger Befehl, keine garantierte physische Abschaltung. Die Bruecke ist noch
nicht an einen realen GPIO-Ausgang gebunden (S3) und nicht in `main/` komponiert (S4).

Weiterhin `NOT_RUN`: Builder-Self-Check, Pre-Ready-Lauf, alle Hardwarenachweise.
