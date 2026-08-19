# Issue #90 Hardware-Verifikation

## Status

- Gesamtstatus: `BLOCKED/NOT_RUN`.
- Grund: Das #29-Hardwaregate ist offen; deshalb wurden kein verifizierter
  ESP32, kein realer UART-/Power-Cut-Controller und kein Standard-Flashlauf
  ausgeführt.
- Der Bring-up-Harness-Build ist ein statischer Buildnachweis, kein
  Hardware-PASS. Reale Partition, UART, Power-Cut, Readback, GC/Erase und
  Heap-/Largest-Block-/Stack-HWM-Messungen bleiben `BLOCKED/NOT_RUN`.
- Die Firmware führt `state_store` / `fermentation` als explizite R1-
  Konfiguration an den anwendungsneutralen `NvsStateStore` heran. Der Adapter
  selbst besitzt keinen Init-/Deinit-Lifecycle.

## Reproduzierbare Build-/Flash-Reihenfolge

Alle Befehle werden aus einem sauberen, committed Checkout ausgeführt.

```bash
. "$IDF_PATH/export.sh"
python3 scripts/build_esp_idf_profiles.py bringup
export APP_ISSUE_90_NVS_HARDWARE_TEST=1
idf.py -B build/issue_90_hardware \
  -DSDKCONFIG=build/issue_90_hardware/sdkconfig \
  -DSDKCONFIG_DEFAULTS='sdkconfig.defaults;sdkconfig.defaults.bringup' \
  -DAPP_ISSUE_90_NVS_HARDWARE_TEST=1 \
  -DAPP_ISSUE_90_SOURCE_GIT_SHA="$(git rev-parse HEAD)" \
  -DAPP_ISSUE_90_PLAN_SHA=da693e8a24735ff2cc09f019b119083f3792882e \
  build
idf.py -B build/issue_90_hardware flash
```

Der Harness-Build ist ausdrücklich ein Bring-up-Testpfad. Das Releaseprofil
wird separat ohne `APP_ISSUE_90_NVS_HARDWARE_TEST` gebaut; eine
Release-Isolationsprüfung stellt sicher, dass weder Harnessquelle noch
`ISSUE90`-Marker oder diese reinen Testabhängigkeiten in den Releasepfad
gelangen.

## Ausführbarer Runner-Vertrag

`--power-cut-hook` ist ein Repository-relativer Skriptpfad, kein Shell-
Kommando-String. Der Hook muss für `ARM`, `TRIP` und `RESTORE` exakt die
Tokens `ARMED`, `TRIPPED` und `RESTORED` auf stdout liefern.

Zuerst wird ein Kalibrierlauf ausgeführt. Er archiviert für jedes Fenster den
reproduzierbaren Delay, die Rotation, `max_writes`, den Zielpunkt, Vorher- /
Nachher-Page-Evidenz, Readback und die Provenienz:

```bash
python3 scripts/issue_90_nvs_hardware_verification.py \
  --build --port <serial-port> \
  --power-cut-hook scripts/issue_90_power_cut_hook.py \
  --artifact-dir build/issue_90_hardware_artifacts \
  --calibration build/issue_90_hardware_artifacts/calibration.json \
  --calibrate --repetitions 10 \
  --windows blob_data,blob_index,old_removal,gc_erase
```

Danach verwendet der Matrixlauf ausschließlich diese kalibrierte, versionierte
Konfiguration und führt mindestens zehn Wiederholungen je Fenster aus:

```bash
python3 scripts/issue_90_nvs_hardware_verification.py \
  --port <serial-port> \
  --power-cut-hook scripts/issue_90_power_cut_hook.py \
  --artifact-dir build/issue_90_hardware_artifacts \
  --calibration build/issue_90_hardware_artifacts/calibration.json \
  --repetitions 10 \
  --windows blob_data,blob_index,old_removal,gc_erase
```

Der Runner sendet ausschließlich die implementierten UART-Kommandos:
`RESET_PARTITION` (testseitig), `PREFILL seed=0`, `PAGE_EVIDENCE`,
`CUT_ARM token=...`, `ROTATE max_writes=N target_rotation=M`, `READBACK_ALL`,
`REBOOT`, `STOP`. Die Firmware meldet den versionierten Vertrag mit
`READY`, `ROTATE_BEGIN`, `ROTATE_RESULT`, `READBACK` und
`GC_ERASE_DETECTED`; der Runner prüft Felder, SHA-256, Länge, Status,
Provenienz und die reale Partition strikt.

Die drei sauberen Reboots werden gegen die Hash-/Längenbaseline des ersten
deterministischen `PREFILL seed=0` geprüft. Jede Power-Cut-Wiederholung beginnt
mit einem testseitigen Partition-Reset und anschließendem deterministischem
Prefill. Reset/Erase bleibt ausschließlich im Bring-up-Harness und gelangt
nicht in `NvsStateStore`.

## GC-/Erase-Orakel

Auf dem Host-BDL ist jeder tatsächliche `Erase`-Callback ein verpflichtender
Exhaustive-Cut-Punkt. Der reale ESP32-Standard-Flashpfad besitzt dagegen
keinen entsprechenden testseitigen Callback und erhält keinen internen oder
produktiven Erase-Hook.

`gc_erase` wird real ausschließlich akzeptiert, wenn die On-Target-Evidenz
belegt: eine vorher gültige, nichtleere Seite wurde gelöscht, die Nachherseite
ist vollständig `0xff`, eine konsistente neuere Sequenz-/Belegungsstruktur
enthält die nachvollziehbaren lebenden Records, NVS-Stats und kompletter
Readback stimmen überein und Vorher-/Nachher-Page-Hashes sind verschieden.
Erased/alte Header oder ein zufällig neueres Blatt mit gleichem Key reichen
nicht. Die Kalibrierung akzeptiert `gc_erase` nur bei diesem Nachweis.

Die Raw-Page-Evidenz verwendet ein festes, überprüftes No-PSRAM-Scratch-
Budget. Sie liegt nicht als mehrere KiB automatische lokale Arrays auf dem
unveränderten Main-Task-Stack. Statischer Compile-/ELF-Nachweis und reale
Ressourcenmessungen sind getrennt; Letztere bleiben bis zur Hardwareausführung
`BLOCKED/NOT_RUN`.

## Artefakte und offene Nachweise

`--artifact-dir` enthält pro Lauf eine Run-ID und Zeitstempel sowie Manifest,
Ergebnisliste, UART-Log und Hook-/Controller-Log. Außerdem werden Source- und
Firmware-SHA, freigegebene Plan-SHA, IDF-SHA, Profil, reale
Partitionsevidenz, Fenster-/Kalibrierparameter, Tokens, Readback-Hashes,
GC-/Erase-Evidenz, Ressourcenlogs und der Abschlussstatus jeder Wiederholung
gesichert. Repository-relative Pfade bleiben Voraussetzung; die erzeugten
Textartefakte unterliegen dem Secret-/Pfadscan.

| Nachweis | Status |
|---|---|
| reale `state_store`-Partition, Flashgröße und Init | `BLOCKED/NOT_RUN` |
| 22-Schlüssel-Prefill und vollständiger Readback | `BLOCKED/NOT_RUN` |
| drei saubere Neustarts gegen Prefill-Baseline | `BLOCKED/NOT_RUN` |
| zehn Power-Cut-Wiederholungen je vier Fenster | `BLOCKED/NOT_RUN` |
| GC-/Erase-/Raw-Page-Orakel auf realem Flash | `BLOCKED/NOT_RUN` |
| reale Heap-/Largest-Block-/Stack-HWM-Messung | `BLOCKED/NOT_RUN` |
