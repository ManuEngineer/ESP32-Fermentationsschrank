# Issue #32 – S1 Execution-Evidence (Zwischenstand)

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
