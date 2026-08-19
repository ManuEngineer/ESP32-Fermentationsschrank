# Issue #29 Build- und Ressourcenbericht

Dieser Bericht gehört zu Issue #29 und ist dem Implementierungs-HEAD
`5950814fc21be557e565dad3aa6acf3dbe3c0b64` zugeordnet. Die ESP-IDF-Profile
wurden mit ESP-IDF `v6.0.2` / Commit
`7101770dc6db2667b3c477cc31365dd1acd6db4e` gebaut.

## `esp32_bringup`

- `size.json total_size`: 194179 Bytes
- DRAM: 13202 / 180736 Bytes
- IRAM: 42023 / 131072 Bytes
- App-BIN: 194304 Bytes
- ELF: 6969868 Bytes
- Mapfile: 5023889 Bytes
- Bootloader-BIN: 26096 Bytes
- Partitionstabellen-BIN: 3072 Bytes
- `sdkconfig` SHA-256: `2b1de6d6a368794932df27e4bdc9e7e4d3d0b709c788db2bf67bb2c487d07961`

## `esp32_release`

- `size.json total_size`: 126475 Bytes
- DRAM: 12618 / 180736 Bytes
- IRAM: 42023 / 131072 Bytes
- App-BIN: 126592 Bytes
- ELF: 3132344 Bytes
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

Der kombinierte Ressourcenbericht wurde nach dem vollständigen lokalen Lauf
erneut mit `Source-Git-SHA`=
`5950814fc21be557e565dad3aa6acf3dbe3c0b64` erzeugt und verifiziert. Der
Build-Commit der erneuten Artefakterzeugung war der nachgelagerte
Dokumentations-HEAD `c1fc97c4149ea02b855ef9a01c662508d373e07f`; zwischen dem
Implementierungs-HEAD und diesem Dokumentationscommit liegt keine
Firmwareänderung. `pio run -e native`, die vollständige native Suite mit
965/965 erfolgreichen Testfällen, beide ESP-IDF-Profile, beide
ESP-IDF-Static-Analysis-Profile, Clang-Tidy, Format-, Architektur-, Secret-,
Quality-Gate- und Artefakt-Scanprüfungen sowie die kumulative
Xtensa-Stackherleitung waren `PASS`. Diese Software-/Buildnachweise sind kein
Board-, UART-, Flash-, Pegel- oder Smoke-Nachweis.

Der reale Hardwarelauf flashte beide Profile vom PR-HEAD
`c4c8b33f4dbaef727200ea410d887ec5417aa1b0` (`App version: c4c8b33` im
Bootlog); der Firmwareinhalt ist gegenüber `5950814` unverändert (siehe
[`ISSUE_29_MEASUREMENTS.md`](ISSUE_29_MEASUREMENTS.md), Abschnitt "Identität
und Scope"). Die realen Board-, Flash-, PSRAM-, Smoke- und Probe-Ergebnisse
sind dort dokumentiert und werden hier nicht dupliziert.

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
