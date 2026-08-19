# Issue #90 Build- und Ressourcenbericht

- Status: `PASS` fuer die gezielten ESP-IDF-Buildprofile; kein vollstaendiger
  projektweiter Lauf ausgefuehrt.
- Provenienz: kanonischer `scripts/build_esp_idf_profiles.py all` und
  `scripts/build_report.py --append --esp-idf-profiles bringup release` mit
  ESP-IDF `v6.0.2 @ 7101770dc6db2667b3c477cc31365dd1acd6db4e`.
- Bring-up-Harness-Build: `PASS` in einem getrennten
  `build/issue_90_hw_verify_corrected/`-Pfad mit
  `APP_ISSUE_90_NVS_HARDWARE_TEST=1`; das Releaseprofil enthaelt ihn nicht
  (Compile-Database und ELF-Marker geprueft).
- On-target Heap-, groesster zusammenhaengender Block- und Stack-HWM:
  `BLOCKED/NOT_RUN`, weil kein ESP32-Testlauf vorliegt. Die Firmware misst
  diese Werte im Harness; die Hardwarewerte werden erst mit dem
  `READY`-/Readback-Protokoll als reale Nachweise erfasst.
- Ein vollstaendiger lokaler projektweiter Gesamtlauf bleibt gemaess
  `AGENTS.md` bis Review und Owner-Anweisung ausstehend.

## ESP-IDF esp32_bringup (JSON2-basierte IDF-Messung)

- Profil: esp32_bringup
- ESP-IDF-Tag: v6.0.2
- ESP-IDF-Commit: 7101770dc6db2667b3c477cc31365dd1acd6db4e
- Build-Commit: 87bf05f51081db59c691afe43d22181253c94e9d
- Source-Git-SHA: 0573096dcc77bd98013b7a3f5be6dc123d90436b
- Gesamter Flashverbrauch (size.json total_size): 195115 Bytes
- DRAM: 13202 / 180736 Bytes
- IRAM: 42023 / 131072 Bytes
- App-BIN (esp32_fermentationsschrank.bin): 195232 Bytes
- ELF: 6973160 Bytes
- Mapfile: 5225435 Bytes
- Bootloader-BIN: 26096 Bytes
- Partitionstabellen-BIN: 3072 Bytes
- sdkconfig SHA-256: 293b09c98ff0eb54c9d98339094155124805cc2b620a5a15f17727446c7e553e

## ESP-IDF esp32_release (JSON2-basierte IDF-Messung)

- Profil: esp32_release
- ESP-IDF-Tag: v6.0.2
- ESP-IDF-Commit: 7101770dc6db2667b3c477cc31365dd1acd6db4e
- Build-Commit: 87bf05f51081db59c691afe43d22181253c94e9d
- Source-Git-SHA: 0573096dcc77bd98013b7a3f5be6dc123d90436b
- Gesamter Flashverbrauch (size.json total_size): 127635 Bytes
- DRAM: 12650 / 180736 Bytes
- IRAM: 42023 / 131072 Bytes
- App-BIN (esp32_fermentationsschrank.bin): 127760 Bytes
- ELF: 3156676 Bytes
- Mapfile: 3052585 Bytes
- Bootloader-BIN: 26096 Bytes
- Partitionstabellen-BIN: 3072 Bytes
- sdkconfig SHA-256: 3891bbe2a886f52e9f37623699b378f103fdc8d799dd83b70ee804b8a21ff1b3
