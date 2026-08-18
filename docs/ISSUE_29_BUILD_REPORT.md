# Issue #29 Build- und Ressourcenbericht

Dieser Bericht gehört zu Issue #29 und ist dem Implementierungs-HEAD
`6be497af855af0c40084999397da055fcae9e015` zugeordnet. Die ESP-IDF-Profile
wurden mit ESP-IDF `v6.0.2` / Commit
`7101770dc6db2667b3c477cc31365dd1acd6db4e` gebaut.

## `esp32_bringup`

- `size.json total_size`: 194191 Bytes
- DRAM: 13202 / 180736 Bytes
- IRAM: 42023 / 131072 Bytes
- App-BIN: 194304 Bytes
- ELF: 6969396 Bytes
- Mapfile: 5023334 Bytes
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

## Xtensa-Stack-Usage-Herleitung

Der Bring-up-Build dieses Heads aktiviert `-fstack-usage` für die Diagnose-
Probe sowie `run_commands.cpp`, `run_persistence_coordinator.cpp`,
`temperature_control_orchestrator.cpp`, `process_state_machine.cpp` und
`program_model.cpp`. Die relevante Evidenz lautet:

- `runProbe()`: 53248 Bytes maximaler Frame;
- `decideProgramStart()`: 3072 Bytes;
- `RunPersistenceCoordinator::loadAndInitialize()`: 8192 Bytes;
- `RunPersistenceCoordinator::persistCommand()`: 9280 Bytes;
- `TemperatureControlApplicationOrchestrator::persistCommand()`: 304 Bytes;
- gehaltene Xtensa-Objektsumme: 24296 Bytes;
- konservativer, begründeter Sicherheitspuffer: 4096 Bytes;
- konfigurierte Diagnose-Taskgröße: 57344 Bytes.

Die `uxTaskGetStackHighWaterMark()`-Einheit ist für ESP32 in ESP-IDF 6.0.2
als Bytes verifiziert. Die Werte sind Compiler-/Build-Evidenz, keine
On-Target-HWM-Messung und kein Produktivbudget. `CONFIG_ESP_MAIN_TASK_STACK_SIZE`
wurde nicht verändert.
