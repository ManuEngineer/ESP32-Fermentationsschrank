## ESP-IDF esp32_bringup (JSON2-basierte IDF-Messung)

- Profil: esp32_bringup
- ESP-IDF-Tag: v6.0.2
- ESP-IDF-Commit: 7101770dc6db2667b3c477cc31365dd1acd6db4e
- Build-Commit: c506ae616c528d78b2349209b99997dbb738f0a1
- Source-Git-SHA: c506ae616c528d78b2349209b99997dbb738f0a1
- Gesamter Flashverbrauch (size.json total_size): 195115 Bytes
- DRAM: 13202 / 180736 Bytes
- IRAM: 42023 / 131072 Bytes
- App-BIN (esp32_fermentationsschrank.bin): 195232 Bytes
- ELF: 6972760 Bytes
- Mapfile: 5221053 Bytes
- Bootloader-BIN: 26096 Bytes
- Partitionstabellen-BIN: 3072 Bytes
- sdkconfig SHA-256: 0343c16e9a3efcaa54dff972050d5ed522d092c5e483e01596e06308d6a3785e

## ESP-IDF esp32_release (JSON2-basierte IDF-Messung)

- Profil: esp32_release
- ESP-IDF-Tag: v6.0.2
- ESP-IDF-Commit: 7101770dc6db2667b3c477cc31365dd1acd6db4e
- Build-Commit: c506ae616c528d78b2349209b99997dbb738f0a1
- Source-Git-SHA: c506ae616c528d78b2349209b99997dbb738f0a1
- Gesamter Flashverbrauch (size.json total_size): 127635 Bytes
- DRAM: 12650 / 180736 Bytes
- IRAM: 42023 / 131072 Bytes
- App-BIN (esp32_fermentationsschrank.bin): 127760 Bytes
- ELF: 3156564 Bytes
- Mapfile: 3048203 Bytes
- Bootloader-BIN: 26096 Bytes
- Partitionstabellen-BIN: 3072 Bytes
- sdkconfig SHA-256: ac524e9219d24b46b14b0e2e6c6375c341a429fda7b8ec2c18104a865363eb9d
# Issue #90 Build- und Ressourcenbericht

- Status: `PASS` für die gezielten ESP-IDF-Buildprofile; kein vollständiger
  projektweiter Lauf ausgeführt.
- Provenienz: `scripts/build_esp_idf_profiles.py all` und
  `scripts/build_report.py --append --esp-idf-profiles bringup release` mit
  ESP-IDF `v6.0.2 @ 7101770dc6db2667b3c477cc31365dd1acd6db4e`.
- Bring-up-Harness-Build: `PASS`; der statische Scratch-/Stacknachweis
  meldet `gHarnessScratch=16240` B und maximal `400` B Funktionsstack.
- On-target Heap-, Largest-Block- und Stack-HWM: `BLOCKED/NOT_RUN`, weil kein
  ESP32-Testlauf vorliegt.
- Ein vollständiger lokaler projektweiter Gesamtlauf bleibt gemäß
  `AGENTS.md` bis Review und Owner-Anweisung ausstehend.
