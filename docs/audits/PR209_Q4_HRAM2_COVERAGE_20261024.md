# PR #209 / H-RAM-2 – Coverage-Matrix: Quellen (Analyse, keine Messung)

Ergänzt [Plan §15.6.1](../tasks/issue-27-r1-web-remaining-scope-plan.md). HEAD `1c56b4de623ab22ae9ae31a1b805ba5cea4e6c4a`,
Produktstand `80bfb28`. Alle Zeilen sind Code-/Dokumentbefunde; **keine Hardwaremessung**.

| Matrixzeile | Beleg |
|---|---|
| Release-Image `real actuators: disabled`, Ausgänge nicht am Planner | `main/app_main.cpp` `logBootSummary` (Z. 226); Kommentar zu Onboard-/BTS7960-Ausgängen ebd. („deliberately not connected to the actuator planner“); Roadmap #32/#33 |
| Sensorevidenz erreicht die Anwendung nicht | `publishOwningRuntimeEvidence` definiert in `fermentation_application.cpp:1470`, außerhalb von Tests ohne Aufrufer (`grep` über `main/`, `src/`, `lib/`); `owningRuntimeEvidence_` nur dort gesetzt (Z. 718/766/1386/1473/2150) |
| DS18B20-Sampler budget-gesperrt, „nothing here reaches control“ | `main/app_main.cpp` Z. 778–798; Roadmap #30: `kApprovedDs18b20TaskBudget` leer |
| Start verlangt Luft- und Kühlsensor-Gültigkeit | `run_commands.cpp:826`, `:971` (`!request.airSensorValid \|\| !request.coolingSensorValid` → Ablehnung); `resolveRuntimeEvidence` (`fermentation_application.cpp:1384`) |
| Werksprogramme nicht lauffähig | `program_model.cpp` `MissingCommissioningValue` bei `ValidationPurpose::Runnable`; Golden-Test M-H1 (`factory*` → `InvalidDocument`) |
| Ack/Mute nur mit vorhandener Meldung | Tests installieren Meldungen per Testzugriff (`installMessage`); im Standby ohne Lauf keine Meldungsquelle |
| Geräte-Namenscommit | Intent `set-device-name` (`web_json_codec.cpp:797`), Settings-Pfad §13 Q3 |
| Slice-7-Harness: Run-Control-Writes, Testpartition, UART-Befehle | `main/issue_90_slice7_harness.cpp` Befehle (`RUN_CONTROL_WRITE`, `ARM_RUN_WRITE_LOAD`, `SET_TRUSTED_UTC` …), `decideManualStart` mit `airSensorValid = true` (Z. 543/583); kein `uxTaskGetStackHighWaterMark`/`heap_caps_*` im Harness |
| Harness-Partitionslayout destruktiv | `partitions/issue_90_state_store_test.csv`: `state_store_test` bei `0x300000`/`0x100000`, dieselbe Region wie `state_store` in `issue_90_state_store.csv` |
| Lokaler Editor kann Notizen | `fermentation_touch_workspace.hpp` Feld `ProgramNotes`; Limits `kMaximumProgramCount=16`, `kMaximumNotesScalars=512` |
| NVS-GC/Fehlerinjektion im Produktimage | keine Injektionsnaht im Release-Pfad; Fehlerinjektion nur in Host-Tests (`SequencedWriteStore`) und Harness-Power-Cuts |
| Vorhandene Heap-/Stackwerte | [H-RAM-1](PR206_H_RAM_1_EVIDENCE_20261019.md) (`a9a1156`/Overlay M), [R1_RAM_LVGL48_HW_EVIDENCE](R1_RAM_LVGL48_HW_EVIDENCE.md) (`6c1e159`: `after_ui_init` frei 37 176 B, größter Block 31 744 B, Main-HWM 12 944 B); beide andere Commit-Identität, untereinander nicht abgeglichen – ein Lauf auf einheitlicher Identität ist Teil des Plans |
| Image-/Partitionsbelegung | `esp32_release` `.bin` 1 774 368 B (HEAD-Build), App-Partition `0x2F0000` = 3 080 192 B (`partitions/issue_90_state_store.csv`) |

**Beobachtung (Indikation, keine Messung):** Die PR-#174-Basis meldet einen größten Block von 31 744 B nach
UI-Init; die vor M-H1 pauschale Einzelprogramm-Reservierung (32 769 B) hätte diesen Block überschritten.
M-H1 beseitigt diese Anforderung (nun Dokumentgröße + 1); eine Messung auf dem aktuellen Stand fehlt.
