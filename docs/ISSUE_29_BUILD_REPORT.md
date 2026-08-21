# Issue #29 Build- und Ressourcenbericht

Dieser Bericht gehört zu Issue #29 und ist dem finalen Firmware-Source-Commit
`3bc5bfe4120d7ca6609733ab9d0736f1cfe99b59` (B2-Fix) zugeordnet. Der frühere
Firmwarestand `5950814fc21be557e565dad3aa6acf3dbe3c0b64` ist nur die
Pre-Fix-Provenienz. Die ESP-IDF-Profile wurden mit ESP-IDF `v6.0.2` / Commit
`7101770dc6db2667b3c477cc31365dd1acd6db4e` gebaut.

## `esp32_bringup`

- `size.json total_size`: 194215 Bytes
- DRAM: 13202 / 180736 Bytes
- IRAM: 42023 / 131072 Bytes
- App-BIN: 194336 Bytes
- ELF: 6971244 Bytes
- Mapfile: 5023889 Bytes
- Bootloader-BIN: 26096 Bytes
- Partitionstabellen-BIN: 3072 Bytes
- `sdkconfig` SHA-256: `2b1de6d6a368794932df27e4bdc9e7e4d3d0b709c788db2bf67bb2c487d07961`

## `esp32_release`

- `size.json total_size`: 126475 Bytes
- DRAM: 12618 / 180736 Bytes
- IRAM: 42023 / 131072 Bytes
- App-BIN: 126592 Bytes
- ELF: 3132728 Bytes
- Mapfile: 2722263 Bytes
- Bootloader-BIN: 26096 Bytes
- Partitionstabellen-BIN: 3072 Bytes
- `sdkconfig` SHA-256: `788bf5fda2065bdf0bcff4775021498a264d05f768ca9a08c23aef0bb53dfa78`

## Geltungsgrenze

Die Werte sind Build-/Artefaktwerte, keine Messung eines angeschlossenen
Boards. Beide Profile verwenden im generierten Build die aktuelle
ESP-IDF-Single-App-Tabelle:

```text
nvs,data,nvs,0x9000,24K,
phy_init,data,phy,0xf000,4K,
factory,app,factory,0x10000,1M,
```

Das ist eine erfasste aktuelle Buildbaseline und keine Festlegung der
finalen Produktionspartitionierung. Reale Flashgröße, Boardrevision und
PSRAM-Status bleiben bis zur Hardwaremessung offen.

## Vollständige lokale Verifikation

Der kombinierte Ressourcenbericht wurde nach dem B2-Fix erneut mit
`Source-Git-SHA`=`3bc5bfe4120d7ca6609733ab9d0736f1cfe99b59` erzeugt und
verifiziert; der tatsächliche Build-Commit dieses Laufs war
`a80999b55108aa5775e5cd5e44fae28911383643`. Die nachfolgenden
Software-/Buildnachweise wurden diesem korrigierten Firmwareinhalt zugeordnet.
`pio run -e native`, die vollständige native Suite mit
965/965 erfolgreichen Testfällen, beide ESP-IDF-Profile, beide
ESP-IDF-Static-Analysis-Profile, Clang-Tidy, Format-, Architektur-, Secret-,
Quality-Gate- und Artefakt-Scanprüfungen sowie die kumulative
Xtensa-Stackherleitung waren `PASS`. Diese Software-/Buildnachweise sind kein
Board-, UART-, Flash-, Pegel- oder Smoke-Nachweis.

Der ursprüngliche reale Hardwarelauf flashte beide Profile vom PR-HEAD
`c4c8b33f4dbaef727200ea410d887ec5417aa1b0` (`App version: c4c8b33` im
Bootlog); der Firmwareinhalt entsprach dem Pre-Fix-Stand `5950814`. Dieser
Lauf zeigte `esp32_bringup` real `FAILED`, während `esp32_release` `PASS` war.
Commit `3bc5bfe4120d7ca6609733ab9d0736f1cfe99b59` korrigiert die
B2-Nachweislogik in `main/issue_29_bringup_probe.cpp::run()` (die einzige
Firmwareänderung gegenüber `c4c8b33`/`5950814`). Der erste erfolgreiche
post-fix-Lauf wurde mit `App version: 3bc5bfe` 40 s erfasst; der zweite mit
`App version: 7024d15` ebenfalls 40 s. Der damalige Laufstand `7024d15` und
der PR-Head vor dieser Synchronisierung `a80999b55108aa5775e5cd5e44fae28911383643`
enthielten gegenüber `3bc5bfe` ausschließlich Dokumentänderungen in den
beiden Issue-29-Berichten. Diese Synchronisierung ergänzt keinen
Firmwarelogik-Fix, sondern nur den vorsichtigen Codekommentar und weitere
Dokumentation; die build-relevante Firmwareprovenienz der beiden erfolgreichen
Läufe bleibt der korrigierte Source-Stand `3bc5bfe`. Die vollständigen realen Board-, Flash-, PSRAM-, Smoke-,
Zyklus-Invarianz- und Probe-Ergebnisse sind in
[`ISSUE_29_MEASUREMENTS.md`](ISSUE_29_MEASUREMENTS.md) dokumentiert und
werden hier nicht dupliziert.

## Xtensa-Stack-Usage-Herleitung

Der Bring-up-Build dieses Heads aktiviert `-fstack-usage` und
`-fcallgraph-info=su` für die Diagnose-Probe sowie die tatsächlich
betroffenen `fermentation_app`-Quellen. Einzelne `.su`-Frames sind keine
kumulative Call-Path-Obergrenze. `python3 scripts/analyze_issue_29_stack.py
build/esp32_bringup` prüft die nicht-inlinierten `.ci`-Kanten und ergibt für
den deterministischen Candidate-Allocation-Failure-Pfad:

| gleichzeitig lebende Funktion | `.su`-Frame | Qualifier | kumulativ |
|---|---:|---|---:|
| `probeTask(void*)` | 48 B | `static` | 48 B |
| `runProbe(ProbeContext&)` | 53232 B | `static` | 53280 B |
| `persistFreshStartCommand(...)` | 32 B | `static` | 53312 B |
| `TemperatureControlApplicationOrchestrator::persistCommand(...)` | 304 B | `static` | 53616 B |
| `RunPersistenceCoordinator::persistCommand(...)` | 9280 B | `static` | 62896 B |
| `RunPersistenceCoordinator::result(...)` | 32 B | `static` | 62928 B |

Die gehaltene Xtensa-Objektsumme beträgt 24296 Bytes; sie ersetzt die
Call-Path-Summe nicht. Der begründete, begrenzte Diagnosepuffer beträgt
4096 Bytes für Task-Einstieg/RTOS-Rahmen und kleine
Compiler-/Instrumentierungsvariation. Auf 1024 Bytes ausgerichtet ergibt
das 67584 Bytes Diagnose-Taskgröße. Alle sechs Qualifier sind `static`; ein
`dynamic`-/`unbounded`-Qualifier oder fehlende Callgraph-Kante blockiert den
Hardwarelauf. Die Werte sind Compiler-/Build-Evidenz, keine On-Target-HWM-
Messung und kein Produktivbudget. `CONFIG_ESP_MAIN_TASK_STACK_SIZE` wurde
nicht verändert. Die `uxTaskGetStackHighWaterMark()`-Einheit ist für ESP32 in
ESP-IDF 6.0.2 als Bytes verifiziert.
