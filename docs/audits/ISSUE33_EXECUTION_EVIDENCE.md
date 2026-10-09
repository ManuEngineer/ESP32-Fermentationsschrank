# Issue #33 – Execution-Evidence (laufend)

Plan: `docs/tasks/issue-33-bts7960-hbridge-plan.md`, Revision 2 (Freigabe `d55d2db`).
Dieses Dokument fuehrt nur tatsaechlich ausgefuehrte Nachweise je Schnitt mit dem
getesteten Code-Commit. Die Abschnitte S1–S4 sind historische Schnittnachweise; ihre
`NOT_RUN`-Angaben galten fuer den damaligen Stand. Der folgende Block ist der **aktuelle
verifizierte Gesamtstand** (Stand 2026-10-09).

```text
STATUS (aktuell, verifiziert 2026-10-09)
SOFTWARE=S1–S5 umgesetzt; PR #196 in main gemergt (Merge-Commit f33e8593c79920aca9084528547a220f284b9f42)
BUILDER_SELF_CHECK=PASS           (Abschnitt S5, Implementierungs-HEAD 3a6483a73af57bb9c80dd2289189173c20a054d0)
PR_HEAD=1a525217865f97e318211bba4e2b04e809b19123
PRE_READY_LOCAL (GitHub-Commit-Status am PR_HEAD)=success, Beschreibung "host+esp PASS"
  (Einzelphasen Host/ESP/Stack nicht gesondert in Repository-Evidence belegt; kein Einzel-PASS ausgewiesen)
Hardware, Flash, reale Peltiertests=NOT_RUN
ACTUATOR_RELEASE=NO   REAL_PELTIER_TEST=NOT_RUN
SSOT_CONFORMANCE=PENDING   FUNCTIONAL_HARDWARE_VERIFICATION=PENDING
ADAPTER_SAFETY_VERIFICATION=PENDING
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

## S3 – `EspIdfSharedEnableBridge` (ESP-IDF-Adapter) und Boot-Initialisierung

```text
GETESTETER_CODE_COMMIT=f6117631ff57ab0e513af11473059855c38bc878 (sauberer Arbeitsbaum)
UMGEBUNG=Linux-Target, ESP-IDF v6.1 (CMock-GPIO-Mock, Ruby 3.3.8), PlatformIO 6.1.19 (-e native)
```

| Nachweis | Befehl | Ergebnis |
|---|---|---|
| Adapter-Hosttest (neu) | `idf.py -C test/esp_idf_shared_enable_bridge_host -B <build> build` und `<build>/issue33_shared_enable_bridge_host.elf` | PASS, 13 Tests, 0 Failures |
| Hosttest #32-Ausgabeadapter (Regression) | `idf.py -C test/esp_idf_binary_output_sink_host -B <build> build` und `.elf` | PASS, 12 Tests |
| Native Bruecke/Driver/Mock (Regression) | `pio test -e native -f test_shared_enable_bridge_sink -f test_actuator_plan_sink_driver -f test_sensor_actuator_mocks` | PASS, 47 Faelle |
| Architekturgrenzen | `python3 scripts/check_architecture_boundaries.py` und `--selftest` | PASS |
| ESP-IDF-Profile (Adapter kompiliert in beiden) | `python3 scripts/build_esp_idf_profiles.py all` | PASS (keine Compiler-Warnung im Log) |
| Format | `clang-format --dry-run -Werror`, `git diff --check` | PASS |

Abgedeckt durch `esp_idf_shared_enable_bridge_host` (GPIO-Aufrufprotokoll der gemockten
ESP-IDF-GPIO-Funktionen): Konstruktor und Aufrufe vor `begin()` ohne GPIO-Zugriff;
Initialisierungsreihenfolge exakt Enable (GPIO25) → RPWM (GPIO13) → LPWM (GPIO14), je
inaktiv vorgesetzt, dann Ausgang ohne Pulls, danach die All-off-Pruefung der Bruecke (9
GPIO-Aufrufe, nie ein HIGH); Fehler an **jeder** der drei Stufen, je als Preset- und als
Konfigurationsfehler (alle drei Stufen werden versucht, `Bridge.begin()` laeuft nicht, je
Ausgang genau ein Best-effort-AUS, danach erreicht kein Befehl mehr einen GPIO);
`Unconfirmed`-Polaritaet des Enable → Adapter inert ohne Zugriff auf diesen Pin und ohne
HIGH; EIN-/AUS-Sequenzen beider Richtungen auf GPIO-Ebene (Schenkel zuerst, Enable zuletzt;
Enable zuerst aus); widerspruechliche Befehle → Abschaltung aller drei Pins und
Verriegelung; GPIO-Schreibfehler beim Enable-EIN und beim Schenkel-AUS → Abschaltrunde,
Verriegelung, keine Freigabe der Gegenrichtung; Pin-Invarianten ueber das gesamte Protokoll
(RPWM/LPWM nie gleichzeitig HIGH, ein Schenkel nur bei LOW-Enable HIGH).

Mutationsproben (Original danach wiederhergestellt, Quelle diff-identisch):
`Bridge.begin()` auch bei fehlgeschlagener Stufe → 6 Tests schlagen fehl;
Initialisierungsreihenfolge vertauscht → 1; Best-effort-AUS entfernt → 6.

Abgrenzung: Die Pins im Test sind die SSOT-Werte (13/14/25), aber der Adapter selbst kennt
keine Rollen und bekommt Pins/Polaritaet vom Aufrufer. Die LOW-Pegel der
Initialisierung sind der SSOT-Designzustand, kein Nachweis fuer Modulpolaritaet oder
Boot-/Reset-Hardwareverhalten. Der Adapter ist noch nicht in `main/` komponiert (S4) und
nicht an Driver/Planner angeschlossen. Der Linux-Hosttest ist nicht Teil von
`scripts/run_pre_ready_gates.sh`.

Weiterhin `NOT_RUN`: Builder-Self-Check, Pre-Ready-Lauf, alle Hardwarenachweise.

## S4 – Generator, Header, Composition Root, Doku

```text
GETESTETER_CODE_COMMIT=efcd80e691afbc46cf00cd49d9f826ef631b7a7a (sauberer Arbeitsbaum)
UMGEBUNG=Linux, PlatformIO 6.1.19 (-e native), ESP-IDF v6.1 (CMock, Ruby 3.3.8), clang-format 21.1.3
```

| Nachweis | Befehl | Ergebnis |
|---|---|---|
| SSOT-Header aktuell | `python3 scripts/generate_board_profile_header.py --check` | PASS |
| Generator-Selbsttest (bestehende + #32- + #33-Faelle) | `python3 scripts/generate_board_profile_header.py --selftest` | PASS, 18 Faelle (davon 5 neue Bruecken-Faelle: Rolle fehlt, Rolle doppelt, ungueltiges/fehlendes `active_level`, `TBD_HARDWARE` → `Unconfirmed`; GPIO34/35 nicht erzeugt) |
| Composition-Guard (neu) | `pio test -e native -f test_peltier_composition_guard` | PASS, 2 Faelle |
| Native Suiten gemeinsam (Guard, Reset-Flow-Quelltext-Guard, Bruecke, Driver, Mocks, Planner, Interlock) | `pio test -e native -f <sieben Suiten>` | PASS, 159 Faelle |
| CMock-Hosttests | `idf.py -C test/esp_idf_shared_enable_bridge_host ... build` + `.elf`; `test/esp_idf_binary_output_sink_host` | PASS, 13 bzw. 12 Tests |
| ESP-IDF-Profile (Composition Root kompiliert) | `python3 scripts/build_esp_idf_profiles.py all` | PASS (keine Compiler-Warnung im Log) |
| Architekturgrenzen | `python3 scripts/check_architecture_boundaries.py` | PASS |
| Format | `clang-format --dry-run -Werror`, `git diff --check` | PASS |

Mutationsprobe des Composition-Guards: ein angehaengter Aufruf `peltierBridge.setForward(true)`
in `main/app_main.cpp` laesst den Guard fehlschlagen (Original danach wiederhergestellt).

Header: die bisherigen Konstanten (Display/Touch/OneWire, drei MOSFET-Ausgaenge) sind
unveraendert; neu `kBtsRpwmPin`=13, `kBtsLpwmPin`=14, `kBtsEnablePin`=25 mit je
`ActiveHigh` aus `active_level: high`. Die Boardprofil-YAML ist unveraendert.

Abgrenzung: Die Composition Root erzeugt die Bruecke und ruft `begin()` (GPIO25 → 13 → 14,
LOW/inaktiv); sie ist nicht an Driver/Planner angeschlossen, und `main/` ruft keine
Richtungsbefehle auf. Die LOW-Pegel sind der SSOT-Designzustand, kein Nachweis fuer
Modulpolaritaet oder Boot-/Reset-Hardwareverhalten. Das Bootlog der Composition Root
(`peltier bridge: ...`) wurde **nicht** auf Hardware beobachtet.

Weiterhin `NOT_RUN`: Builder-Self-Check (`run_pre_ready_gates.sh self-check`) inklusive
statischem Stack-Gate (#121) fuer die erweiterte `app_main`, Pre-Ready-Lauf, alle
Hardwarenachweise (HW-33-01..05).

## S5 – Builder-Self-Check und Gesamtstand vor Independent Review

```text
IMPLEMENTIERUNGS_HEAD=3a6483a73af57bb9c80dd2289189173c20a054d0
  (Quellcode identisch mit dem S4-Code-Commit efcd80e691afbc46cf00cd49d9f826ef631b7a7a;
   Differenz = nur diese Evidence-Datei)
BUILDER_SELF_CHECK=PASS   bash scripts/run_pre_ready_gates.sh self-check
  BOARD_PROFILE_SINGLE_SOURCE=PASS  CLANG_FORMAT=PASS  CLANG_TIDY=PASS
  BUILDER_STATIC_ANALYSIS_SELF_CHECK=PASS
```

Die Schnitt-Nachweise S1–S4 (getestete Code-Commits `d751515`, `e850e1a`, `f611763`,
`efcd80e`) stehen unveraendert in den Abschnitten oben. Der Self-Check ist die
statische Analyse (Format, clang-tidy, Board-Header-Einzelquelle); er enthaelt **nicht**
das statische Stack-Gate (#121) der `esp`-Phase und ersetzt weder Fachtests noch den
Pre-Ready-Lauf.

Planabdeckung (`docs/tasks/issue-33-bts7960-hbridge-plan.md`, Revision 2, Freigabe `d55d2db`):

| Plan | Stand |
|---|---|
| S1 Portvertrag `bool` + Driver-Sperre | umgesetzt (`d751515`) |
| S2 `SharedEnableBridgeSink` + native Tests | umgesetzt (`e850e1a`) |
| S3 `EspIdfSharedEnableBridge` + Linux-CMock-Hosttest | umgesetzt (`f611763`) |
| S4 Generator, Header, Composition Root, Guard, Doku | umgesetzt (`efcd80e`) |
| S5 Self-Check, Beweisdokumentation | dieser Abschnitt |
| O1 = A / O2 / O3 / O4 = A | wie beschlossen umgesetzt; kein Re-Arm, keine eigene Zeitbasis, Bruecke ohne Planner-Anbindung |

Abweichungen/Praezisierungen gegenueber dem Planwortlaut (keine Architekturaenderung):
1. `EspIdfSharedEnableBridge::begin()` liefert `bool` (`true` = alle drei Ausgaenge und die
   Bruecke bereit) statt eines Ergebnis-Enums.
2. Die Quelltext-Pruefung „kein `setForward(`/`setReverse(` in `main/`“ ist die eigene
   Suite `test_peltier_composition_guard` (zusaetzlich zum bestehenden Reset-Flow-Guard).
3. Die Pinvalidierung der GPIO-Ausgaenge liegt wie in #32 beim ESP-IDF-Treiber
   (Maskenschutz 0..63 im Adapter).

Weiterhin offen bzw. `NOT_RUN`: Independent Review, statisches Stack-Gate (#121) und
vollstaendiger Pre-Ready-Lauf (Ownerfreigabe), alle Hardwarenachweise (HW-33-01..05),
Laufzeitverhalten der `AllOffBinarySink` (Bring-up-Probe, nativ nicht testbar). Der
Linux-CMock-Hosttest ist nicht Teil des Pre-Ready-Gates. `ACTUATOR_RELEASE=NO`,
`REAL_PELTIER_TEST=NOT_RUN`, `SSOT_CONFORMANCE=PENDING`,
`FUNCTIONAL_HARDWARE_VERIFICATION=PENDING`, `ADAPTER_SAFETY_VERIFICATION=PENDING`;
R_IS/L_IS bleiben deaktiviert und unbeschaltet. Vorbedingung fuer eine reale
Peltierfreigabe ausserhalb von #33: Release-Kette (Aktor-Gate `Allowed`, Planner-/
Regelschleifen-Komposition #35/#90, Meldung eines Luefterfehlers nach oben).
