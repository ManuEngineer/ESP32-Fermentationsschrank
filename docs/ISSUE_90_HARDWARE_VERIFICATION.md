# Issue #90 Hardware-Verifikation

## Status

- Gesamtstatus: `NOT_RUN`.
- #29-Software/Bring-up/Board/UART/Flash/Recovery/Smokes sind auf dem
  tatsächlich getesteten realen Board nachgewiesen. Nur die sicheren
  unbelasteten MCU-/Gate-/Bootpegel bleiben dort `NOT_RUN`; die physische
  PCB-Revision/Silkscreen ist nach Ownerentscheidung kein Abnahmekriterium.
  Dieses Pegel-Restgate blockiert #90 nicht.
- Issue-90-NVS-Hardwaretests wurden noch nicht real ausgeführt. Der
  vollständige Hostvertrag bleibt wegen Callback 12 offen; daher darf aus dem
  Host-Harness oder dem #29-Nachweis kein NVS-Hardware-PASS abgeleitet werden.
- Die R5.7-Boardgrenze ist actor-free: bewiesen werden Boot, NVS-/Persistenz-
  und Recoveryklassifikation sowie ein logisch geschlossener Produktiv-/
  Aktorgate über Harness, UART oder Logs. Es werden keine physischen
  Gate-/MOSFET-/Lüfter-/BTS7960-/Peltier- oder UI-/Display-/Touch-Eigenschaften
  abgenommen.
- Der Bring-up-Harness-Build ist ein statischer Buildnachweis, kein
  Hardware-PASS. Reale Partition, UART, Power-Cut, Readback, GC/Erase und
  Heap-/Largest-Block-/Stack-HWM-Messungen bleiben `NOT_RUN`.
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
  -DAPP_ISSUE_90_PLAN_SHA=4b4053bc229f279f5b30fc16ed60c765d1ee151e \
  build
idf.py -B build/issue_90_hardware flash
```

Der Harness-Build ist ausdrücklich ein Bring-up-Testpfad. Das Releaseprofil
wird separat ohne `APP_ISSUE_90_NVS_HARDWARE_TEST` gebaut; eine
Release-Isolationsprüfung stellt sicher, dass weder Harnessquelle noch
`ISSUE90`-Marker oder diese reinen Testabhängigkeiten in den Releasepfad
gelangen.

## Ausführbarer Runner-Vertrag

Der R5.7-Produktnachweis benoetigt keinen externen Power-Cut-Hook und keinen
Power-Controller. Die reale Verifikation bleibt actor-free und bewertet nach
manueller Unterbrechung waehrend aktiver repräsentativer Schreiblast nur
Boot, NVS-Initialisierung, Persistenzzustand, Recoveryklassifikation und den
logisch geschlossenen Gate. Ein manueller Power-Off behauptet keinen exakten
internen Callback-, BLOB-, Index- oder GC-Cut; die praezise interne
Fault-Injection bleibt Hostscope.

Ein `--power-cut-hook` mit `ARM`/`TRIP`/`RESTORE` und den Tokens `ARMED`,
`TRIPPED`, `RESTORED` darf als zusaetzliche Backendcharakterisierung bestehen,
ist aber kein R5.7-Release-1-Abnahmegate.

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

Eine optionale Backendcharakterisierung kann danach diese kalibrierte,
versionierte Konfiguration verwenden. Sie ist getrennt vom Produktgate und
führt keine verpflichtende Release-1-Hardwarevollmatrix ein. Für das R5.7-
Produktgate genügen ein sauberer Kontrolllauf und mindestens drei manuelle
Wiederholungen je ausgewähltem realem Szenario während aktiver repräsentativer
Schreiblast:

```bash
python3 scripts/issue_90_nvs_hardware_verification.py \
  --port <serial-port> \
  --power-cut-hook scripts/issue_90_power_cut_hook.py \
  --artifact-dir build/issue_90_hardware_artifacts \
  --calibration build/issue_90_hardware_artifacts/calibration.json \
  --repetitions 3 \
  --windows blob_data,blob_index,old_removal,gc_erase
```

Der Runner sendet ausschließlich die implementierten UART-Kommandos:
`RESET_PARTITION` (testseitig), `PREFILL seed=0`, `PAGE_EVIDENCE`,
`CUT_ARM token=...`, `ROTATE max_writes=N target_rotation=M`, `READBACK_ALL`,
`REBOOT`, `STOP`. Die Firmware meldet den versionierten Vertrag mit
`READY`, `ROTATE_BEGIN`, `ROTATE_RESULT`, `READBACK` und
`GC_ERASE_DETECTED`; der Runner prüft Felder, SHA-256, Länge, Status,
Provenienz und die reale Partition strikt.

Ein sauberer Kontrolllauf und die manuellen Wiederholungen werden gegen eine
vollstaendig validierte Ausgangssicht geprueft. Der Test darf keinen exakten
internen Cutpunkt behaupten. Reset/Erase bleibt ausschließlich im
Bring-up-Harness und gelangt nicht in `NvsStateStore`; ein unklarer Run wird
nicht still geloescht oder als `NoActiveRun` persistiert.

## GC-/Erase-Orakel

Auf dem Host-BDL ist jeder tatsächliche `Erase`-Callback ein verpflichtender
Exhaustive-Cut-Punkt. Der reale ESP32-Standard-Flashpfad besitzt dagegen
keinen entsprechenden testseitigen Callback und erhält keinen internen oder
produktiven Erase-Hook.

`gc_erase` ist fuer die optionale Backendcharakterisierung real nur dann
akzeptiert, wenn die On-Target-Evidenz
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
`NOT_RUN`.

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
| reale `state_store`-Partition, Flashgröße und Init | `NOT_RUN` – kein Issue-90-Lauf ausgeführt; Hostvertrag Callback 12 offen |
| 22-Schlüssel-Prefill und vollständiger Readback | `NOT_RUN` – kein Issue-90-Lauf ausgeführt |
| drei saubere Neustarts gegen Prefill-Baseline | `NOT_RUN` – kein Issue-90-Lauf ausgeführt |
| ein sauberer Kontrolllauf plus mindestens drei manuelle Wiederholungen je ausgewähltem Szenario | `NOT_RUN` – kein Issue-90-Lauf ausgeführt |
| GC-/Erase-/Raw-Page-Orakel auf realem Flash | `NOT_RUN` – kein Issue-90-Lauf ausgeführt |
| reale Heap-/Largest-Block-/Stack-HWM-Messung | `NOT_RUN` – kein Issue-90-Lauf ausgeführt |
