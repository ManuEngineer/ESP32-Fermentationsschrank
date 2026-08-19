# Issue #29 Messprotokoll und Abnahmestatus

Status dieses Protokolls: Software-/Buildnachweise `PASS`. Reale Board-/UART-/
Flash-/PSRAM-Nachweise sind jetzt `PASS`. `esp32_release` besteht den
35-Sekunden-Smoke real auf dem Board. `esp32_bringup` scheitert dagegen
reproduzierbar real auf dem Board: die Bring-up-Probe erreicht kein
`PASS`, wodurch `app_main` vor dem Heartbeat-Smoke sicher stoppt (siehe
Abschnitt "Ressourcenprobe und Fehlervertragsprobe"). Die Issue-DoD ist damit
weiterhin nicht abgenommen — nicht mehr wegen fehlender Hardware, sondern
wegen eines konkreten, reproduzierbaren `esp32_bringup`-Befunds.

Implementierungsstatus: `SOFTWARE_IMPLEMENTED_HARDWARE_TESTED_BRINGUP_FAILED`;
die reale Hardware ist jetzt erreichbar und beide Profile wurden real
geflasht und beobachtet. Die Issue-Abnahme bleibt offen, bis der
`esp32_bringup`-Befund durch den Owner bewertet und (nach Plan-Freigabe einer
Korrektur) erneut real nachgewiesen ist.

## Identität und Scope

- Issue: #29
- PR: #116, Draft
- Branch: `agent/issue-29-esp32-bringup-plan`
- Basis: `main @ 87dd593fcdc8d26831873a4163b174340b4347c0`
- freigegebener Plan:
  `docs/tasks/issue-29-implementation-plan.md @ 4f49b44cff47f55bfd425d9e39c5a07256782ed7`
- Implementierungs-HEAD (Firmwareinhalt): `5950814fc21be557e565dad3aa6acf3dbe3c0b64`
- real geflashter PR-HEAD: `c4c8b33f4dbaef727200ea410d887ec5417aa1b0`
  (`App version: c4c8b33` in jedem Bootlog). `git diff --stat
  5950814fc21be557e565dad3aa6acf3dbe3c0b64
  c4c8b33f4dbaef727200ea410d887ec5417aa1b0` zeigt ausschließlich
  Dokumentänderungen (`docs/ISSUE_29_BUILD_REPORT.md`,
  `docs/ISSUE_29_MEASUREMENTS.md`, `docs/ROADMAP.md`); der Firmwareinhalt von
  `c4c8b33` ist damit identisch zum Implementierungs-HEAD `5950814`.
- ESP-IDF: `v6.0.2 @ 7101770dc6db2667b3c477cc31365dd1acd6db4e`
- Ziel: ESP32 ohne PSRAM, aktorfrei und unbelastet
- Werkzeuge: `esptool v5.3.1`, `idf.py`/ESP-IDF `v6.0.2`, Python
  `3.13.5` (`idf6.0_py3.13_env`)
- Port: `/dev/ttyUSB0` (FTDI FT232R USB UART, Seriennummer `A5069RR4`),
  automatischer DTR/RTS-Reset über IO0/EN (kein manueller BOOT/EN-Taster
  erforderlich)

Die Messung behauptet keine spätere UI-, NVS-, Web-, Display- oder
Parallelbelastung. Diese Konsumenten müssen ihre eigenen Task-/Heapwirkungen
erneut messen; das spätere Lastgate, insbesondere #37, bleibt offen.

## Software- und Buildnachweise

| Nachweis | Ergebnis | Konkrete Evidenz |
|---|---|---|
| Native gezielte Tests | `PASS` | `test_run_commands` und `test_run_persistence_coordinator`; 162/162 Testfälle erfolgreich |
| Native vollständige Suite | `PASS` | 42 Testgruppen; 965/965 Testfälle erfolgreich |
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
| CI-Artefakt-Scanabdeckung | `PASS` | `python3 scripts/check_ci_artifact_scan_coverage.py` |
| Build-/Ressourcenreport | `PASS` | [`ISSUE_29_BUILD_REPORT.md`](ISSUE_29_BUILD_REPORT.md), Source-SHA exakt zugeordnet |

Der versionierte Build-/Ressourcenbericht enthält die von ESP-IDF erzeugten
Größen- und Partitionstabellenartefakte. Rohartefakte aus `build/` bleiben
lokale Buildartefakte; ihre kanonischen Werte sind im Bericht festgehalten.

## Compile-time-Isolation des Fault-Seams

| Profil | `APP_ISSUE_29_BRINGUP_PROBE` in Compile-Commands | Diagnose-Taskquelle |
|---|---:|---:|
| `esp32_bringup` | 42 Vorkommen im generierten Compile-Command-Artefakt | 4 Vorkommen |
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

Die Diagnose-Taskgröße wird im Code aus den tatsächlich kompilierten Xtensa-
Objektgrößen und der `-fstack-usage`-/`-fcallgraph-info=su`-Evidenz des
Bring-up-Aufrufpfads gebildet. Der Build dieses Heads meldet
`sizeof(CommandDecision)=9872`,
`sizeof(RunCommandState)=4784`, `sizeof(ProgramStartRequest)=304`,
`sizeof(RunPersistenceCoordinator)=8336`,
`sizeof(TemperatureControlApplicationOrchestrator)=152`,
`sizeof(TemperatureController)=280`, `sizeof(ActuatorPlanner)=424`,
`sizeof(TargetQualificationEvaluator)=128` und `sizeof(SafetyCore)=16`; die
Summe der gehaltenen statischen Objekte beträgt 24296 Bytes. Einzelne `.su`-
Werte werden nicht additiv als Call-Path-Obergrenze missverstanden. Der
relevante, tatsächlich im `.ci`-Callgraph belegte Fehlerpfad ist:

| gleichzeitig lebende Funktion | `.su`-Frame | Qualifier | kumulativ |
|---|---:|---|---:|
| `probeTask(void*)` | 48 B | `static` | 48 B |
| `runProbe(ProbeContext&)` | 53232 B | `static` | 53280 B |
| `persistFreshStartCommand(...)` | 32 B | `static` | 53312 B |
| `TemperatureControlApplicationOrchestrator::persistCommand(...)` | 304 B | `static` | 53616 B |
| `RunPersistenceCoordinator::persistCommand(...)` | 9280 B | `static` | 62896 B |
| `RunPersistenceCoordinator::result(...)` | 32 B | `static` | 62928 B |

`scripts/analyze_issue_29_stack.py build/esp32_bringup` prüft diese sechs
Frames, ihre `static`-Qualifier und die tatsächlichen `.ci`-Kanten. Die
kompilierte kumulative Obergrenze beträgt 62928 Bytes. Der begründete,
begrenzte Diagnosepuffer von 4096 Bytes deckt Task-Einstieg/RTOS-Rahmen und
kleine Compiler-/Instrumentierungsvariation ab; anschließend wird
reproduzierbar auf 1024 Bytes ausgerichtet. Daraus folgt die initiale
Diagnose-Taskgröße von 67584 Bytes. Ein `dynamic`-/`unbounded`-Qualifier,
fehlende Kante oder nicht reproduzierbare Kette blockiert den Hardwarelauf;
es wird keine Stackgröße geraten. Diese Zahlen sind kein Produktiv-Stackwert
und verschieben `CommandDecision` nicht auf den Heap;
`CONFIG_ESP_MAIN_TASK_STACK_SIZE` bleibt unverändert. Der On-Target-HWM muss
die Herleitung anschließend bestätigen; Stackoverflow, Panic, Watchdog oder
fehlender HWM sind `FAILED`/`BLOCKED`, nie `PASS`.

Die vorgesehenen Messpunkte bleiben getrennt:

- `B0`: vor `xTaskCreate`, ohne Diagnose-Task;
- `B1`: nach Erzeugung des zunächst blockierten Diagnose-Tasks;
- innerhalb des Tasks: vor Decision, vollständige Decision gehalten,
  vollständig erfolgreich angewendeter lokaler Kandidat gehalten, nach
  Decision-Freigabe;
- `B2`: erst nach bestätigtem Selbstlöschen und ausreichender Idle-Zeit für die
  Freigabe von TCB/Stack;
- je Punkt: freier Heap, Minimum-Free-Heap, größter 8-Bit-Block;
- innerhalb des Tasks zusätzlich Ready-/Completion-Task-High-Water-Mark und
  Main-Task-High-Water-Mark.

Reale On-Target-Ausführung auf `esp32_bringup` (Board `20:50:0d:1b:2f:34`,
Reset via kontrolliertem RTS-Puls, zwei unabhängige Läufe mit identischen
Werten und Zeitstempeln):

```text
E (3319) issue29_probe: BLOCKED: deferred diagnostic-task cleanup was not proven against B0
I (3319) issue29_probe: stack_formula_bytes=67584 command_decision_bytes=9872 run_command_state_bytes=4784 persistence_coordinator_bytes=8336 held_object_bytes=24296 call_path_bytes=62928 call_path_safety_buffer_bytes=4096 configured_task_stack_bytes=67584
I (3329) issue29_probe: before_decision free_heap_bytes=232888 minimum_free_heap_bytes=232888 largest_free_block_bytes=110592 task_stack_hwm_bytes=13228 main_stack_hwm_bytes=3064
I (3349) issue29_probe: decision_held free_heap_bytes=231392 minimum_free_heap_bytes=231392 largest_free_block_bytes=110592 task_stack_hwm_bytes=7628 main_stack_hwm_bytes=3064
I (3369) issue29_probe: local_apply_held free_heap_bytes=230056 minimum_free_heap_bytes=230056 largest_free_block_bytes=110592 task_stack_hwm_bytes=7628 main_stack_hwm_bytes=3064
I (3379) issue29_probe: after_decision_release free_heap_bytes=232888 minimum_free_heap_bytes=230056 largest_free_block_bytes=110592 task_stack_hwm_bytes=7628 main_stack_hwm_bytes=3064
I (3399) issue29_probe: before_task_create free_heap_bytes=303964 minimum_free_heap_bytes=303964 largest_free_block_bytes=172032 task_stack_hwm_bytes=0 main_stack_hwm_bytes=3064
I (3409) issue29_probe: after_task_create_blocked free_heap_bytes=234224 minimum_free_heap_bytes=234224 largest_free_block_bytes=110592 task_stack_hwm_bytes=67084 main_stack_hwm_bytes=3064
E (3429) issue29_probe: after_task_cleanup status=BLOCKED
I (3429) issue29_probe: task_ready_hwm_bytes=67132
I (3439) issue29_probe: task_completion_hwm_bytes=5532
I (3439) issue29_probe: fault_status=18 fault_step=1 fault_technical_reason=7 fault_durability=0 fault_pass=PASS writes=0 reads=3 state_unchanged=true persistence_unchanged=true actor_release=false safety_fail_closed=true
I (3459) issue29_probe: internal_resource_hwm_valid=true local_apply_measured=true cleanup_handoff_received=true task_cleanup_proven=false after_task_cleanup_measured=false
I (3479) issue29_probe: result=FAILED
E (3479) app_main: Issue 29 bring-up probe failed; stopping before the heartbeat smoke
```

| Nachweis | Ergebnis | Grund / Nachweisgrenze |
|---|---|---|
| reale 48/96/1024-`CommandDecision`-Ressourcenprobe erzeugt und gehalten | `PASS` | `before_decision`/`decision_held`/`local_apply_held`/`after_decision_release` real gemessen |
| `B0` (`before_task_create`) und `B1` (`after_task_create_blocked`) | `PASS` | `free_heap_bytes=303964`/`largest_free_block_bytes=172032` (B0) bzw. `234224`/`110592` (B1); Taskstack-Allokation (61440 B) exakt sichtbar |
| `B2` (`after_task_cleanup`) | `FAILED` | `largest_free_block_bytes` erreicht innerhalb der 3000-ms-Bindung (`kCleanupWaitTicks`) nicht wieder `>= 172032`; bleibt bei `110592`; zwei Läufe reproduzierbar identisch |
| interne vier Probezeitpunkte | `PASS` | alle vier Punkte real geloggt (siehe Log oben) |
| Task-Stack-High-Water-Mark | `PASS` | `task_ready_hwm_bytes=67132`, `task_completion_hwm_bytes=5532`; `uxTaskGetStackHighWaterMark(nullptr)` liefert Byteeinheit auf ESP32 wie erwartet |
| On-Target-Allokationsfehlervertrag | `PASS` | `fault_status=18 fault_pass=PASS writes=0 reads=3` — der Allokationsfehler selbst wurde sauber fail-closed behandelt |
| fachlicher Zustand unverändert | `PASS` | `state_unchanged=true` |
| kein Persistenz-Write/-Commit | `PASS` | `persistence_unchanged=true`, `writes=0` |
| keine neue Aktorfreigabe, Safety fail-closed | `PASS` | `actor_release=false`, `safety_fail_closed=true` |
| Gesamtergebnis `esp32_bringup`-Probe (`result=`) | `FAILED` | Gate `pass = completedSafely && stackObserved && taskCleanupProven && afterTaskCleanupMeasured && resourcePass && faultPass` scheitert ausschließlich an `taskCleanupProven`/`afterTaskCleanupMeasured`; alle anderen Teilkriterien sind einzeln `PASS` |

Die lokale Fehlervertragsprobe verwendet nur den bestehenden
Command-/Apply-/Persistenzweg, einen lokalen `IStateStore`-Double und
All-off-Sinks. Eine allgemeine NVS-/Commit-/Power-Cut-Matrix aus #90 wurde
nicht vorgezogen.

Weil `main/app_main.cpp` (Abschnitt "Fehlervertragsprobe und
On-Target-Fault-Seam" im Plan) den Heartbeat-Smoke erst nach `PASS` der
Bring-up-Probe startet, kehrt `app_main()` bei diesem `FAILED`-Ergebnis
kontrolliert zurück, bevor die Laufzeitschleife beginnt (siehe
`app_main: Issue 29 bring-up probe failed; stopping before the heartbeat
smoke` und danach `main_task: Returned from app_main()`). Das ist der im
Plan vorgesehene sichere Fehlerpfad (kein Busy-Loop, kein automatischer
Reboot, keine Aktoren) — es bedeutet aber auch, dass der
`esp32_bringup`-35-Sekunden-Heartbeat-Smoke real nicht erreicht wird, solange
dieser Befund besteht.

## Hardwarebaseline und Smoke

Das Board ist über `/dev/ttyUSB0` (FTDI FT232R, Serie `A5069RR4`) mit
verdrahtetem IO0/EN erreichbar. Der automatische DTR/RTS-Reset funktioniert
reproduzierbar sowohl für `esptool` (Flash, ROM-Bootloader-Sync) als auch für
einen kontrollierten Reset-zu-Run über einen expliziten RTS-Puls (siehe
Befehle unten); ein manueller BOOT/EN-Taster war nicht erforderlich.

Read-only Chip-/Flash-Erfassung (`esptool.py --port /dev/ttyUSB0 --baud
115200 flash_id`):

```text
Chip type:          ESP32-D0WD-V3 (revision v3.1)
Features:           Wi-Fi, BT, Dual Core + LP Core, 240MHz, Vref calibration in eFuse, Coding Scheme None
MAC:                20:50:0d:1b:2f:34
Manufacturer: b3
Device: 4016
Detected flash size: 4MB
```

Kein PSRAM in den gemeldeten Features und keine PSRAM-Region im
firmwareseitigen `heap_init`-Log (nur die fünf erwarteten
DRAM/D-IRAM/IRAM-Regionen) — konsistent mit der Planannahme "ESP32 ohne
PSRAM".

Reset-/Flash-Befehle (jeweils nach `. "$IDF_PATH/export.sh"`):

```bash
python3 scripts/build_esp_idf_profiles.py all
idf.py -B build/esp32_bringup -p /dev/ttyUSB0 flash
idf.py -B build/esp32_release -p /dev/ttyUSB0 flash
```

Die UART-Erfassung selbst nutzt einen kleinen `pyserial`-Helfer statt
`idf.py monitor`, weil Letzteres ein TTY an `stdin` voraussetzt. Der Helfer
öffnet den Port mit `dtr=False`/`rts=False` (IO0/EN released) und führt
anschließend exakt die von `esptool.reset.HardReset` (non-flow-control-Pfad)
verwendete Sequenz aus: `RTS=True` (EN→LOW) für 100 ms, dann `RTS=False`
(EN→HIGH) — ein regulärer Reset in den Run-Modus, kein Bootloader-Eintritt.

`esp32_release`-Smoke-Auszug (Anfang/Ende der 40-s-Erfassung, Reset →
POWERON, gekürzt in der Mitte):

```text
rst:0x1 (POWERON_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
...
I (244) app_main: profile: esp32_release
I (254) app_main: hardware state: HARDWARE_UNVERIFIED
I (254) app_main: actuator policy: REQUIRE_VERIFIED_HARDWARE
I (264) app_main: real actuators: disabled
I (264) app_main: application: ready
I (274) app_main: resources: free_heap_bytes=304668 stack_hwm_bytes=3072
I (1284) app_main: heartbeat: safe test mode, uptime_ms=1003
...
I (30284) app_main: resources: free_heap_bytes=304668 stack_hwm_bytes=3072
...
I (39284) app_main: heartbeat: safe test mode, uptime_ms=39003
```

| Kriterium | Ergebnis | Feststellung |
|---|---|---|
| Board/Revision | `PASS` | ESP32-D0WD-V3, Chiprevision v3.1 (`esptool` und übereinstimmend `efuse_init: Chip rev: v3.1` in beiden Bootlogs) |
| reale Flash-ID und Flashgröße | `PASS` | Manufacturer `b3`, Device `4016`, 4 MB (`esptool flash_id`); Bootlog bestätigt `SPI Flash Size: 4MB` |
| realer PSRAM-Status | `PASS` | kein PSRAM (weder `esptool`-Feature noch zusätzliche Heap-Region); Board entspricht der Planannahme "ohne PSRAM" |
| UART/FT232RL und ROM-Bootloader-Recovery | `PASS` | `esptool`-Sync, Flash-Schreib-/Verify-Zyklus und kontrollierter Run-Reset über DTR/RTS reproduzierbar erfolgreich |
| generierte Partitionstabelle | `PASS` als Buildbaseline | siehe [`ISSUE_29_BUILD_REPORT.md`](ISSUE_29_BUILD_REPORT.md); Bootlog bestätigt dieselbe Tabelle (`nvs`/`phy_init`/`factory`); nicht als finale Produktionstabelle behauptet |
| `esp32_bringup` mindestens 35 s | `FAILED` | Bring-up-Probe scheitert real bei `result=FAILED` (siehe vorheriger Abschnitt); `app_main` stoppt sicher nach ca. 3,5 s Uptime, bevor die Heartbeat-Schleife beginnt — die 35-s-Anforderung wird dadurch nicht erreicht |
| `esp32_release` mindestens 35 s auf demselben Board | `PASS` | reale Erfassung über 39+ s, monotone Uptime `1003`…`39003` ms, kein Abbruch |
| exakt zwei reguläre Smoke-Ressourcenpunkte | `PASS` (nur `esp32_release`) | `resources: free_heap_bytes=304668 stack_hwm_bytes=3072` bei `t=274 ms` und erneut identisch bei `t=30284 ms`; für `esp32_bringup` `NOT_RUN`, weil die Laufzeitschleife wegen des Probe-`FAILED` nicht erreicht wird |
| Resetursache, Panic, Watchdog, Brownout | `PASS` (nur `esp32_release`) | `rst:0x1 (POWERON_RESET)` in beiden Bootlogs, kein Panic/Watchdog/Brownout/unerwarteter Reset während der 39-s-`esp32_release`-Erfassung; für `esp32_bringup` gilt derselbe saubere Boot, aber kein Laufzeitfenster zur Beobachtung nach dem Probe-Abbruch |
| sichere unbelastete MCU-/Gate-/Bootpegel | `NOT_RUN` | kein Multimeter/Messaufbau in dieser Ausführungsumgebung verfügbar; erfordert physische Messung durch den Owner |
| belastete MOSFET-/Verbraucherwirkung | `NOT_RUN` | ausdrücklich nicht Bestandteil von #29 |

Es wurden keine 12-V-Verbraucher, Aktoren, Sensoren, Displays, Lüfter,
BTS7960- oder Peltierkomponenten integriert oder aktiviert. `HARDWARE_UNVERIFIED`,
`TBD_HARDWARE`, `TBD_COMMISSIONING` und `TBD_IMPLEMENTATION_BUDGET` bleiben
deshalb bestehen. Die physische PCB-/Boardrevision (Silkscreen-Aufdruck) und
die sicheren unbelasteten Pegelmesspunkte bleiben ebenfalls offen, da sie
eine physische Ablesung beziehungsweise ein Messgerät vor Ort erfordern.

## Dokumentationsrückführung

Trotz jetzt vorhandener `confirmed_test`-Evidenz für Board, Flashgröße und
PSRAM-Status wurden `docs/HARDWARE.md` und `docs/OPEN_POINTS.md` in diesem
Schritt bewusst **nicht** aktualisiert. Gründe:

- der `esp32_bringup`-Hardwarelauf ist real `FAILED` (siehe oben); der Plan
  sieht die kanonische Rückführung erst nach abgeschlossenem, bewertetem
  Schnitt 4 vor, nicht während eines offenen Befunds;
- die sicheren unbelasteten MCU-/Gate-/Bootpegel bleiben `NOT_RUN` (kein
  Messgerät verfügbar) — `docs/HARDWARE.md` verlangt genau diese Pegelfakten
  zusätzlich zu Board/Flash/PSRAM;
- eine Rückführung soll dem Owner als bewusste, einzelne Entscheidung
  vorgelegt werden, nicht im selben Lauf wie die Hardwaremessung erfolgen.

`docs/RESOURCE_BUDGET_AND_MAINTENANCE.md` bleibt unverändert; der
aktualisierte [`ISSUE_29_BUILD_REPORT.md`](ISSUE_29_BUILD_REPORT.md) und die
compilerbasierte Stack-Usage-Herleitung sind Build-/Ressourcennachweise, aber
kein Beleg für ein kanonisches Produktions- oder Parallelbudget.

Nächster Schritt: Owner-Bewertung des `esp32_bringup`-Befunds
(`after_task_cleanup`/`taskCleanupProven` in
`main/issue_29_bringup_probe.cpp`) und Entscheidung über eine
Plankorrektur (z. B. Zeitfenster, Ursachenanalyse der Heap-Fragmentierung,
oder ein anderer Cleanup-Nachweis). Erst nach real erfolgreichem
`esp32_bringup`-Smoke auf demselben Board und nach einer physischen
Pegelmessung dürfen bestätigte Fakten in die kanonischen Hardware- und
Open-Points-Dokumente zurückgeführt werden.
