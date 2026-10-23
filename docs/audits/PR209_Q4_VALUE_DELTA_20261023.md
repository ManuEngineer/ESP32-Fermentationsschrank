# PR #209 / Q4 – Korrekturdelta Value-Stackpfade: Umsetzung und Messung

Plan: [§14](../tasks/issue-27-r1-web-remaining-scope-plan.md), freigegebene Plan-SHA
`05f8d7482c52ada9f53e70ff7958d5a4e20689f6`. Produktquellen: Commits `eeb23ff` (Domain-Kerne),
`3a1e664` (Bridge/Application/Koordinator); gemessener HEAD `1427dea` (Quellen identisch zu
`3a1e664`, danach nur Tests). Profile: beide echten ESP-IDF-Builds
(`python3 scripts/build_esp_idf_profiles.py all --require-clean-source-tree`, **PASS**,
`SOURCE_TREE_CLEAN=YES`). Compiler xtensa-esp32-elf GCC 15.2.0, `-Og`, `-fno-exceptions`.

## Umgesetzt

- `decideApplyRecoveryTimeCorrectionInto` / `decideApplySensorSelectionActionInto` (je
  `CommandDecision&` und `CommandDecisionView&`) über einen gemeinsamen Kern; die Value-Funktionen
  sind dünne Hüllen. Der lokale `RunCommandState`-Kandidat der Sensorselektion entfällt.
- `FermentationUiCommandBridge::decidePreparedIntoCore` ohne `unsupportedAppDetail` für beide Typen.
- `FermentationApplication::applyConfirmedPreparedValue` samt Abzweig entfernt (einziger Aufrufer).
- `persistCommandInternal`: Identitätscheck `&decision.after == &workingSet_.candidate &&
  commandBorrowed_` für die View vor dem `AppliedRamOnly`-Zweig (Mutationsprobe: ohne Check
  schlägt der Foreign-after-Test fehl).

## Gezielte native Tests (alle PASS)

`test_run_commands` 52, `test_run_persistence_coordinator` 171, `test_fermentation_ui_commands` 14,
`test_web_application_routes` 120, `test_local_touch_ui` 61, `test_renderer_boundary` 85,
`test_run_checkpoint_codec` 38. Neu: Value-vs-View-Matrix für beide Befehle (8 Szenarien je Fall,
vollständige Ergebnisfelder); Koordinator: persistent Sensorselektion Value=Borrow über
Write-Fehler/Power-cut/Unknown, RAM-only ohne Write und ohne `persistedIds_`, Foreign-after,
Recovery Value=Borrow; Web: beide Intents im Standby und bei aktivem Lauf über den Borrow-Pfad,
Replay identisch, Borrow freigegeben.

## Stackbilanz (beide Profile identisch)

Dateien: [SUMMARY](PR209_Q4_VALUE_DELTA_20261023_SUMMARY.tsv),
[PATHS](PR209_Q4_VALUE_DELTA_20261023_PATHS.tsv),
[UNKNOWN](PR209_Q4_VALUE_DELTA_20261023_UNKNOWN.tsv),
[Skript](PR209_Q4_VALUE_DELTA_20261023_PATHS.py.txt) (Methodik wie
`PR209_Q4_IMPLEMENTATION_20261023_PATHS.py.txt`, SDK-`.ci` der 54 instrumentierten Einheiten
unverändert wiederverwendet; Value-Fallback-Filter und Sonderfall entfernt).

| Pfad | Prefix | bekannte Summe | Rest zu 8192 B | unbekannte Knoten |
|---|---|---|---|---|
| Entscheidung Recovery-Zeit | 4576 | 5760 | 2432 | 28 |
| Entscheidung Sensorselektion | 4576 | 6672 | 1520 | 33 |
| größter bisheriger Entscheidungspfad (RunAdjustment) | 4576 | 6176 | 2016 | 42 |
| Application-Wurzel, alle neun Varianten | 3056 | 7216 | 976 | 135 |
| Run-Store / Write-Readback | 5248 / 5952 | 7216 | 976 | 70 / 38 |

- Der `applyConfirmedPreparedValue`-Frame (10 880 B) ist im Graphen nicht mehr vorhanden (Assertion
  im Skript). Der Sensorselektionspfad ist mit 6672 B der größte Entscheidungspfad, bleibt aber
  unter dem bisherigen Maximum 7216 B der Run-Store-Kette; das bekannte Gesamtmaximum ändert sich
  nicht.
- Alle Summen bleiben **NOT_PROVEN**: SDK-/ROM-/libc-/indirekte/Allocator-/Cleanup-Kanten sind
  offen (Spalte „unbekannte Knoten“). 8192 − 7216 = 976 B rechnerischer Rest, nach 512 B
  Reserveboden nur 464 B für Unbestimmtes. Die früheren Sieben-Varianten-Werte sind keine
  Vollketten-PASS.
- Keine Heap-/OOM-/Largest-Free-Aussage; Gate bleibt im bestehenden Ressourcenvertrag.

## Status

`HTTPD_FULL_CHAIN_STACK=NOT_PROVEN` (der zuvor statisch nachgewiesene 15 248-B-FAIL ist beseitigt;
Vollkettenobergrenze nicht bewiesen), `C5_RESOURCE_GATE=HOLD`, `RAM_PASS=NOT_PROVEN`,
`ACTUATOR_RELEASE=NO`; D9/Hardware/Flash/HWM `NOT_RUN`.
