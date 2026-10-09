# Issue #32 – S1/S2 Execution-Evidence (Zwischenstand)

Plan: `docs/tasks/issue-32-onboard-mosfet-outputs-plan.md` (Revision 1, Freigabe `2c1d32b`).
Schnitt S1 (Adapter `EspIdfBinaryOutputSink`, Linux-CMock-Hosttest). S2/S3 nicht begonnen.

```text
GETESTETER_COMMIT=6e74ed571e8abe0e22d5ff6155e3f25e3927ac34 (sauberer Worktree)
UMGEBUNG=ESP-IDF v6.1 (fff9895c82d744c7237be8847347bdd1b07c6643), Ruby 3.3.8 (CMock-Generator), Linux-Target
ACTUATOR_RELEASE=NO   kein Flash, keine Hardware, keine Last

HOSTTEST_BUILD=PASS
  idf.py -C test/esp_idf_binary_output_sink_host -B <build> build
HOSTTEST_LAUF=PASS   12 Tests, 0 Failures, 0 Ignored
  <build>/issue32_binary_output_sink_host.elf
MUTATIONSPROBE=PASS  Unconfirmed mit GPIO-Aufrufen -> 2 Tests schlagen fehl (Commit b4fc0c1);
                     Wegnahme des sofortigen Inaktiv-Versuchs nach Schreibfehler -> 2 Tests schlagen fehl
ESP_IDF_PROFILE_BUILD=PASS  python3 scripts/build_esp_idf_profiles.py all  (esp32_bringup, esp32_release)
ARCHITEKTURGRENZEN=PASS     python3 scripts/check_architecture_boundaries.py
FORMAT=PASS                 clang-format --dry-run -Werror (neue/geaenderte Dateien), git diff --check
PRE_READY_GATE=NOT_RUN  clang-tidy=NOT_RUN  uebrige PlatformIO-Native-Tests=NOT_RUN
HARDWARE (Pegel, Polaritaet, Kanalfunktion, Boot/Reset)=NOT_RUN
SSOT_CONFORMANCE=PENDING  FUNCTIONAL_HARDWARE_VERIFICATION=PENDING
```

Grenzen: `Unconfirmed` bedeutet nur „kein aktiver GPIO-Zugriff“, nicht nachgewiesen
„Verbraucher AUS“. Nach einem fehlgeschlagenen `gpio_set_level` wird `Faulted`
verriegelt und einmalig der inaktive Pegel versucht; das ist ein
softwareseitiger Abschaltversuch ohne Garantie einer physischen Abschaltung bei
Treiber-/Hardwarefehler. Die Pinvalidierung (Ausgangsfaehigkeit) liegt beim
ESP-IDF-Treiber; der Adapter schuetzt nur die Maskenverschiebung (0..63).
Der Hosttest ist nicht Teil von `scripts/run_pre_ready_gates.sh`.

## S2 (Generator, Composition Root, Doku)

```text
GETESTETER_COMMIT_S2=39bad999ff17fa128d412933de2ad45b6e3c1e88 (Quellstand identisch mit dem Arbeitsbaum der Laeufe)
HOSTTEST_LAUF=PASS   12 Tests, 0 Failures (Neubau im leeren Build-Verzeichnis)
GENERATOR_CHECK=PASS python3 scripts/generate_board_profile_header.py --check
GENERATOR_SELFTEST=PASS  13 Faelle (7 bestehende + 6 neue Ausgangsfaelle)
ESP_IDF_PROFILE_BUILD=PASS  python3 scripts/build_esp_idf_profiles.py all  (0 Compiler-Warnungen im Log)
ARCHITEKTURGRENZEN=PASS  check_architecture_boundaries.py und --selftest
REGRESSION_NATIVE=PASS   pio test -e native -f test_actuator_plan_sink_driver -f test_actuator_planner -f test_actuation_interlock
FORMAT=PASS              clang-format --dry-run -Werror (geaenderte C++-Dateien), git diff --check
PRE_READY_GATE=NOT_RUN  clang-tidy=NOT_RUN  statisches Stack-Gate (#121)=NOT_RUN
```

Das Boot-Log der Composition Root (`onboard outputs: inner_fan=... outer_fan=...
buzzer=...`) wurde **nicht** auf Hardware beobachtet (kein Flash). Erwartet bei
`TBD_HARDWARE`: alle drei `polarity_unconfirmed_no_gpio_access`.

## S3 (Builder-Self-Check, Gesamtstand vor Independent Review)

```text
IMPLEMENTIERUNGS_HEAD=750ea2111761be2c2cfee380ca71091f8550979f
  (Quellcode identisch mit 39bad999ff17fa128d412933de2ad45b6e3c1e88; Differenz = nur diese Evidence-Datei)
BUILDER_SELF_CHECK=PASS   bash scripts/run_pre_ready_gates.sh self-check
  BOARD_PROFILE_SINGLE_SOURCE=PASS  CLANG_FORMAT=PASS  CLANG_TIDY=PASS
  BUILDER_STATIC_ANALYSIS_SELF_CHECK=PASS
```

Planabdeckung (`docs/tasks/issue-32-onboard-mosfet-outputs-plan.md`, Rev. 1):

| Plan | Stand |
|---|---|
| S1 Adapter + Linux-Hosttest | umgesetzt, 12/12 PASS |
| S2 Generator/Header, Composition Root, Doku | umgesetzt (Header: bisherige zehn Konstanten unveraendert, nicht acht — redaktioneller Altstand im Plan; GPIO27 nicht erzeugt) |
| S3 Self-Check, Builds, Beweisdokumentation | dieser Abschnitt |
| O1a/O1b/O2/O3/O4 | A / A / A / B / Ja umgesetzt; keine Summermuster-Engine, keine Ton-/PWM-Ansteuerung |

Abweichungen vom Planwortlaut (keine Architekturaenderung):
1. Die Pinvalidierung nutzt nicht `GPIO_IS_VALID_OUTPUT_GPIO` (auf dem Linux-Target nicht
   definiert); der Adapter schuetzt nur die 64-Bit-Maskenverschiebung (0..63), die
   Ausgangsfaehigkeit entscheidet der ESP-IDF-Treiber.
2. `begin()` liefert `BinaryOutputBeginResult` (`PolarityUnconfirmed`/`Ready`/`Failed`)
   fuer das Boot-Log; das ist keine Zustandsabfrage.
3. Nach einem Schreibfehler verriegelt der Adapter `Faulted` und versucht einmalig den
   inaktiven Pegel (Review B1); ein softwareseitiger Versuch ohne Garantie.

Offen bzw. `NOT_RUN`: Independent Review, vollstaendiger Pre-Ready-Lauf, statisches
Stack-Gate (#121, Teil der `esp`-Phase), alle Hardwarenachweise (HW-32-01..03).
`SSOT_CONFORMANCE=PENDING`, `FUNCTIONAL_HARDWARE_VERIFICATION=PENDING`,
`ACTUATOR_RELEASE=NO`. Vorbedingung fuer #33/#90 (nicht in #32 geloest): ein
verworfenes EIN am Aussenluefter ist fuer Driver/Planner unsichtbar; der
Interlock-Pfad muss vor einer realen Peltierfreigabe einen fehlgeschlagenen oder
unbestaetigten Aussenluefter erkennen.
