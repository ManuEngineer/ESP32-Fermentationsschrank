# Issue #29 Messprotokoll und Abnahmestatus

Status dieses Protokolls: Software-/Buildnachweise `PASS`; reale
Board-/UART-/Flash-/Ressourcen-/Smoke-Nachweise `BLOCKED` beziehungsweise
`NOT_RUN`.

## Identität und Scope

- Issue: #29
- PR: #116, Draft
- Branch: `agent/issue-29-esp32-bringup-plan`
- Basis: `main @ 87dd593fcdc8d26831873a4163b174340b4347c0`
- freigegebener Plan:
  `docs/tasks/issue-29-implementation-plan.md @ 4f49b44cff47f55bfd425d9e39c5a07256782ed7`
- Implementierungs-HEAD: `764759f8b307be75ce0acc8042b8fdef79b19ea7`
- ESP-IDF: `v6.0.2 @ 7101770dc6db2667b3c477cc31365dd1acd6db4e`
- Ziel: ESP32 ohne PSRAM, aktorfrei und unbelastet

Die Messung behauptet keine spätere UI-, NVS-, Web-, Display- oder
Parallelbelastung. Diese Konsumenten müssen ihre eigenen Task-/Heapwirkungen
erneut messen; das spätere Lastgate, insbesondere #37, bleibt offen.

## Software- und Buildnachweise

| Nachweis | Ergebnis | Konkrete Evidenz |
|---|---|---|
| Native gezielte Tests | `PASS` | `test_run_commands` und `test_run_persistence_coordinator`; 162/162 Testfälle erfolgreich |
| Native Produktionsbuild | `PASS` | `pio run -e native` |
| ESP-IDF `esp32_bringup` | `PASS` | `python3 scripts/build_esp_idf_profiles.py all` |
| ESP-IDF `esp32_release` | `PASS` | derselbe Profil-Lauf; Release enthält keinen Issue-29-Probequellpfad |
| ESP-Clang beide Profile | `PASS` | `python3 scripts/run_esp_idf_static_analysis.py all` mit verifiziertem `IDF_TOOLS_PATH` |
| Native Clang-Tidy | `PASS` | kanonische Dateiliste aus `.github/workflows/build.yml` |
| Format | `PASS` | `clang-format-18 --dry-run --Werror` für C/C++ |
| Diff-Whitespace | `PASS` | `git diff --check` |
| Architekturgrenzen | `PASS` | `python3 scripts/check_architecture_boundaries.py` |
| Secret-Scan | `PASS` | `python3 scripts/check_secrets.py` |
| Quality-Gate-Selbsttests | `PASS` | `python3 scripts/selftest_quality_gates.py` |
| Build-/Ressourcenreport | `PASS` | [`ISSUE_29_BUILD_REPORT.md`](ISSUE_29_BUILD_REPORT.md), Source-SHA exakt zugeordnet |

Der versionierte Build-/Ressourcenbericht enthält die von ESP-IDF erzeugten
Größen- und Partitionstabellenartefakte. Rohartefakte aus `build/` bleiben
lokale Buildartefakte; ihre kanonischen Werte sind im Bericht festgehalten.

## Compile-time-Isolation des Fault-Seams

| Profil | `APP_ISSUE_29_BRINGUP_PROBE` in Compile-Commands | Diagnose-Taskquelle |
|---|---:|---:|
| `esp32_bringup` | 42 Vorkommen im generierten Compile-Command-Artefakt | 3 Vorkommen |
| `esp32_release` | 0 | 0 |
| native | 0 im generierten Buildartefakt | 0 |

Die Release-/Native-Prüfung ist damit ein Compile-time-Nachweis: kein
öffentlicher Produktions-Allocator, kein globales `operator new` und kein
nutzbarer Fault-Seam werden in diesen Profilen registriert.

## Ressourcenprobe und Fehlervertragsprobe

Die Implementierung verwendet einen transienten, ausschließlich
`esp32_bringup`-seitigen Diagnose-Task. Der normale `app_main`-Stack wird für
den großen `CommandDecision`-/`RunCommandState`-/Persistenzpfad nicht als
ausreichend vorausgesetzt; `CONFIG_ESP_MAIN_TASK_STACK_SIZE` wurde nicht
geändert.

Die Diagnose-Taskgröße wird im Code aus den tatsächlich kompilierten Größen
von `CommandDecision`, `RunCommandState`, den gehaltenen Probeobjekten und
einem separat benannten 16-KiB-Compiler-/Aufrufpfadpuffer gebildet. Dieser
Wert ist kein Produktiv-Stackwert und verschiebt `CommandDecision` nicht auf
den Heap. Der tatsächliche Task-Stackwert, die statischen Größen, die
Call-Path-/Map-Auswertung und die High-Water-Mark gehören zur
On-Target-Messung.

Die vorgesehenen Messpunkte bleiben getrennt:

- `B0`: vor `xTaskCreate`, ohne Diagnose-Task;
- `B1`: nach Erzeugung des zunächst blockierten Diagnose-Tasks;
- innerhalb des Tasks: vor Decision, vollständige Decision gehalten, lokaler
  Apply, nach Decision-Freigabe;
- `B2`: nach Taskabschluss und Freigabe;
- je Punkt: freier Heap, Minimum-Free-Heap, größter 8-Bit-Block;
- innerhalb des Tasks zusätzlich Ready-/Completion-Task-High-Water-Mark und
  Main-Task-High-Water-Mark.

| Nachweis | Ergebnis | Grund / Nachweisgrenze |
|---|---|---|
| reale 48/96/1024-`CommandDecision`-Ressourcenprobe | `BLOCKED` | kein verifiziert angeschlossenes ESP32-Board/UART-Gerät in dieser Ausführung |
| `B0`/`B1`/`B2` Heap- und Blockwerte | `NOT_RUN` | abhängig vom fehlenden On-Target-Lauf |
| interne vier Probezeitpunkte | `NOT_RUN` | abhängig vom fehlenden On-Target-Lauf |
| Task-Stack-High-Water-Mark | `NOT_RUN` | keine Hardwareausführung; kein Wert wird erfunden |
| On-Target-Allokationsfehlervertrag | `BLOCKED` | kein verifiziertes Ziel für den `esp32_bringup`-only Fault-Seam |
| fachlicher Zustand unverändert | `NOT_RUN` | On-Target-Beobachtung nicht ausgeführt |
| kein Persistenz-Write/-Commit | `NOT_RUN` | lokaler Storedouble ist implementiert, On-Target-Ausführung fehlt |
| keine neue Aktorfreigabe, Safety fail-closed | `NOT_RUN` | All-off-Sinks sind implementiert, On-Target-Ausführung fehlt |

Die lokale Fehlervertragsprobe verwendet nur den bestehenden
Command-/Apply-/Persistenzweg, einen lokalen `IStateStore`-Double und
All-off-Sinks. Eine allgemeine NVS-/Commit-/Power-Cut-Matrix aus #90 wurde
nicht vorgezogen.

## Hardwarebaseline und Smoke

In der Ausführungsumgebung wurde kein verifiziertes `/dev/ttyUSB*`-,
`/dev/ttyACM*`- oder `/dev/serial/by-id`-Gerät gefunden. Deshalb wurden weder
Flash noch Board gelesen oder beschrieben.

| Kriterium | Ergebnis | Feststellung |
|---|---|---|
| Board/Revision | `BLOCKED` | kein angeschlossenes/verifiziertes Board verfügbar |
| reale Flash-ID und Flashgröße | `NOT_RUN` | kein Flashzugriff |
| realer PSRAM-Status | `NOT_RUN` | kein Targetzugriff |
| UART/FT232RL und ROM-Bootloader-Recovery | `BLOCKED` | kein serielles Zielgerät verfügbar |
| generierte Partitionstabelle | `PASS` als Buildbaseline | siehe [`ISSUE_29_BUILD_REPORT.md`](ISSUE_29_BUILD_REPORT.md); nicht als finale Produktionstabelle behauptet |
| `esp32_bringup` mindestens 35 s | `NOT_RUN` | kein Targetzugriff |
| `esp32_release` mindestens 35 s auf demselben Board | `NOT_RUN` | kein Targetzugriff |
| exakt zwei reguläre Smoke-Ressourcenpunkte | `NOT_RUN` | kein Targetzugriff |
| Resetursache, Panic, Watchdog, Brownout | `NOT_RUN` | kein Targetzugriff; keine künstliche Unterspannungsinjektion |
| sichere unbelastete MCU-/Gate-/Bootpegel | `NOT_RUN` | kein sicher zugänglicher Messaufbau verfügbar |
| belastete MOSFET-/Verbraucherwirkung | `NOT_RUN` | ausdrücklich nicht Bestandteil von #29 |

Es wurden keine 12-V-Verbraucher, Aktoren, Sensoren, Displays, Lüfter,
BTS7960- oder Peltierkomponenten integriert oder aktiviert. `HARDWARE_UNVERIFIED`,
`TBD_HARDWARE`, `TBD_COMMISSIONING` und `TBD_IMPLEMENTATION_BUDGET` bleiben
deshalb bestehen.

## Dokumentationsrückführung

Mangels realer Hardwareevidenz wurden `docs/HARDWARE.md` und
`docs/OPEN_POINTS.md` nicht abgehakt oder mit unbestätigten Boardfakten
aktualisiert. `docs/RESOURCE_BUDGET_AND_MAINTENANCE.md` bleibt unverändert;
der Buildbericht ist kein Beleg für ein kanonisches Produktions- oder
Parallelbudget.

Vor einem späteren Hardwaregate müssen die fehlenden Werte auf dem exakt
gleichen Implementierungs-HEAD ergänzt und mit UART-/Messartefakten verlinkt
werden. Erst dann dürfen bestätigte Fakten in die kanonischen Hardware- und
Open-Points-Dokumente zurückgeführt werden.
