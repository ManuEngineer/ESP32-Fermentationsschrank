# Issue #90 Build- und Ressourcenbericht

- Status: gezielte ESP-IDF-Buildprofile `PASS`; vollständiger Host-
  `exhaustive`-Lauf `FAIL` bei Callback 12. Kein vollständiger projektweiter
  Gesamtlauf ausgeführt.
- Provenienz: `scripts/build_esp_idf_profiles.py all`,
  `scripts/run_esp_idf_static_analysis.py all` und
  `scripts/build_report.py --esp-idf-profiles bringup release` mit
  ESP-IDF `v6.0.2 @ 7101770dc6db2667b3c477cc31365dd1acd6db4e`.
- Build-relevanter PR-#117-Source-Commit:
  `17100cc87427d7bcbeb25a8f53920abe686869d6`.
  Der ausgecheckte Berichts-Commit
  `165a9691bd47233dc536715aa1e1926e35dcc13d` enthält danach nur die
  R4-Planrevision und ändert den Firmware-Source nicht.
- Host-BDL: gezielte `ci-regression`-Gruppen `PASS`; `exhaustive` bricht bei
  `old-or-new-bdl-cut: old-or-new read status=1 callback=12` ab. Status `1`
  ist im bestehenden Vertrag `NotFound` und kein zulässiger Abschluss.
- Bring-up-Harness-Build: `PASS`; der statische Scratch-/Stacknachweis
  meldet `gHarnessScratch=16240` B und maximal `400` B Funktionsstack.
- On-target Heap-, Largest-Block- und Stack-HWM: `NOT_RUN`, weil kein
  ESP32-Testlauf vorliegt.
- Ein vollständiger lokaler projektweiter Gesamtlauf bleibt gemäß
  `AGENTS.md` bis Review und Owner-Anweisung ausstehend.

## Host-Nachweise auf dem synchronisierten Source

- `ISSUE90_HOST_MODE=ci-regression`: `PASS` für `binary-empty`,
  `invalid-configuration`, `bounded-read`, `blob-boundaries`,
  `maximum-record`, `error-mapping`, `old-or-new-bdl-cut` und
  `prefilled-gc-erase`.
- `ISSUE90_HOST_MODE=exhaustive`: `FAIL` reproduzierbar bei
  `old-or-new-bdl-cut`, Callback 12, `status=1/NotFound`.
- `esp32_bringup`: Build `PASS`, ESP-Clang-Static-Analysis `PASS`.
- `esp32_release`: Build `PASS`, ESP-Clang-Static-Analysis `PASS`.

## ESP-IDF esp32_bringup (JSON2-basierte IDF-Messung)

- Profil: esp32_bringup
- ESP-IDF-Tag: v6.0.2
- ESP-IDF-Commit: 7101770dc6db2667b3c477cc31365dd1acd6db4e
- Build-Commit: 165a9691bd47233dc536715aa1e1926e35dcc13d
- Source-Git-SHA: 17100cc87427d7bcbeb25a8f53920abe686869d6
- Gesamter Flashverbrauch (size.json total_size): 195151 Bytes
- DRAM: 13202 / 180736 Bytes
- IRAM: 42023 / 131072 Bytes
- App-BIN (esp32_fermentationsschrank.bin): 195264 Bytes
- ELF: 6972672 Bytes
- Mapfile: 5221053 Bytes
- Bootloader-BIN: 26096 Bytes
- Partitionstabellen-BIN: 3072 Bytes
- sdkconfig SHA-256: 0343c16e9a3efcaa54dff972050d5ed522d092c5e483e01596e06308d6a3785e

## ESP-IDF esp32_release (JSON2-basierte IDF-Messung)

- Profil: esp32_release
- ESP-IDF-Tag: v6.0.2
- ESP-IDF-Commit: 7101770dc6db2667b3c477cc31365dd1acd6db4e
- Build-Commit: 165a9691bd47233dc536715aa1e1926e35dcc13d
- Source-Git-SHA: 17100cc87427d7bcbeb25a8f53920abe686869d6
- Gesamter Flashverbrauch (size.json total_size): 127635 Bytes
- DRAM: 12650 / 180736 Bytes
- IRAM: 42023 / 131072 Bytes
- App-BIN (esp32_fermentationsschrank.bin): 127760 Bytes
- ELF: 3156540 Bytes
- Mapfile: 3048203 Bytes
- Bootloader-BIN: 26096 Bytes
- Partitionstabellen-BIN: 3072 Bytes
- sdkconfig SHA-256: ac524e9219d24b46b14b0e2e6c6375c341a429fda7b8ec2c18104a865363eb9d
