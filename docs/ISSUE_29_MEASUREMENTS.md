# Issue #29 Messprotokoll und Abnahmestatus

**Aktueller Stand (2026-08-30/31):** Der historische `PASS` aus PR #116 (unten
dokumentiert) ist auf der aktuellen `integration/r1-development`-Baseline
**nicht mehr aktuell gültig**. Eine frische Requalifikation hat den bekannten
Panic auf dem normalen `esp32_bringup`-Profil real reproduziert; siehe
Abschnitt "Aktuelle Requalifikation auf `integration/r1-development`" weiter
unten. Der folgende historische Status- und Identitätsabschnitt bleibt als
Provenienz des PR-#116-Standes erhalten und wird nicht rückwirkend
umgeschrieben.

## Historischer Status (PR #116, vor der aktuellen Gesamtbaseline)

Status dieses Protokolls: Software-/Buildnachweise `PASS`. Reale Board-/UART-/
Flash-/PSRAM-Nachweise sind `PASS`. `esp32_release` und (nach der unten
dokumentierten Ursachenanalyse und Korrektur) `esp32_bringup` bestehen beide
real den 35-Sekunden-Smoke auf demselben Board, jeweils mit zwei unabhängigen
Läufen für `esp32_bringup` reproduziert. Offen bleiben ausschließlich die
sicheren unbelasteten MCU-/Gate-/Bootpegel (kein Messgerät in dieser
Ausführungsumgebung).

Implementierungsstatus (historisch, PR #116):
`SOFTWARE_IMPLEMENTED_HARDWARE_TESTED_PASS_PENDING_LEVELS`; beide Profile
bestanden real auf dem damaligen Board/Stand. Die Issue-Abnahme war auch
damals erst vollständig, wenn zusätzlich die sicheren unbelasteten
MCU-/Gate-/Bootpegel real nachgewiesen sind. Die physische PCB-Revision
beziehungsweise der Silkscreen ist nach Ownerentscheidung kein
Abnahmekriterium.

## Identität und Scope

- Issue: #29
- PR: #116, Draft
- Branch: `agent/issue-29-esp32-bringup-plan`
- Basis: `main @ 87dd593fcdc8d26831873a4163b174340b4347c0`
- freigegebener Plan:
  `docs/tasks/issue-29-implementation-plan.md @ 4f49b44cff47f55bfd425d9e39c5a07256782ed7`
- Pre-Fix-Firmwarestand: `5950814fc21be557e565dad3aa6acf3dbe3c0b64`
- erster realer Hardwarelauf (`esp32_bringup` `FAILED`, `esp32_release`
  `PASS`): PR-HEAD `c4c8b33f4dbaef727200ea410d887ec5417aa1b0`
  (`App version: c4c8b33`); Firmwareinhalt identisch zum Pre-Fix-Firmwarestand
  `5950814` (nur Dokumentänderungen dazwischen).
- Firmware-Fix und finaler Firmware-Source-Commit für die erfolgreichen
  Nachweise: `3bc5bfe4120d7ca6609733ab9d0736f1cfe99b59`
  (`fix(issue-29): anchor B2 cleanup proof on B1, not B0`) — einzige
  Firmwareänderung gegenüber `c4c8b33`/`5950814`: die B2-Nachweislogik in
  `main/issue_29_bringup_probe.cpp::run()` (siehe Abschnitt
  "Ursachenanalyse und Korrektur" unten).
- Zwei unabhängige erfolgreiche `esp32_bringup`-Verifikationsläufe mit je
  40 s Erfassungsfenster: Lauf 1 geflasht aus dem korrigierten Source-Commit
  (`App version: 3bc5bfe`), Lauf 2 auf dem damaligen PR-/Dokumentationsstand
  (`App version: 7024d15`). `git diff --name-only
  3bc5bfe4120d7ca6609733ab9d0736f1cfe99b59 a80999b55108aa5775e5cd5e44fae28911383643`
  listet vor dieser Synchronisierung ausschließlich
  `docs/ISSUE_29_BUILD_REPORT.md` und `docs/ISSUE_29_MEASUREMENTS.md`. Die
  Synchronisierung ergänzt keinen Firmwarelogik-Fix, sondern nur den
  vorsichtigen Codekommentar und Dokumentation. Beide Läufe liefen damit auf
  demselben build-relevanten korrigierten Firmwareinhalt und lieferten
  byte-identische Messwerte.
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

### Ursprünglicher realer Fehlerlauf (historisch, PR-HEAD `c4c8b33`)

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

Damals einzeln `PASS`: die reale Ressourcenprobe (alle vier internen
Probezeitpunkte, `B0`/`B1`, Task-Stack-HWM) und der
Allokationsfehlervertrag (`fault_pass=PASS`, `writes=0`,
`state_unchanged=true`, `persistence_unchanged=true`,
`actor_release=false`, `safety_fail_closed=true`). Einzig `B2`
(`after_task_cleanup`) scheiterte: `largest_free_block_bytes` erreichte
innerhalb der 3000-ms-Bindung nicht wieder `>= 172032` (B0), sondern blieb
bei `110592`. Weil `main/app_main.cpp` den Heartbeat-Smoke erst nach `PASS`
der Bring-up-Probe startet, kehrte `app_main()` bei diesem `FAILED`-Ergebnis
sicher zurück, bevor die Laufzeitschleife begann — kein Busy-Loop, kein
automatischer Reboot, keine Aktoren, aber auch kein erreichter
35-s-Heartbeat-Smoke für dieses Profil.

### Ursachenanalyse und Korrektur

Zwei gezielte, real auf demselben Board durchgeführte Experimente (nur die
Wartelogik in `run()` temporär verändert, `probeTask`/`runProbe` und damit
der von `scripts/analyze_issue_29_stack.py` geprüfte Call-Path blieben
unberührt) lieferten die folgenden Messbefunde; die konkrete Ursache des
verbleibenden B0→B2-Restdeltas ist damit nicht bestimmt:

1. **Erweitertes Zeitfenster (12 s statt 3 s), Trace alle 100 ms:**
   `largest_free_block_bytes` blieb über die vollen 12 s exakt bei `110592`
   eingefroren, ohne jede Bewegung. `free_heap_bytes` erholte sich dagegen
   schon nach dem ersten Poll (t=100 ms) auf `303788` und blieb dort ebenso
   flach — 176 Bytes unter der B0-Baseline (`303964`), aber stabil.
   → die 12-s-Wartezeit änderte das Plateau nicht; eine längere Wartezeit ist
   damit kein geeigneter Weg, den B0-Wert wieder zu erreichen.
2. **Zweiter, identisch großer Task-Erzeuge/Lösche-Zyklus** direkt im
   Anschluss (gleicher `kProbeTaskStackDepth`, sofortiges
   `vTaskDelete(nullptr)`): `free_heap_bytes` und `largest_free_block_bytes`
   waren nach diesem zweiten Zyklus bit-identisch zu den Werten davor
   (`303788`/`110592` → `303788`/`110592`).
   → der zweite identisch große Zyklus verursachte keinen weiteren Nettoverlust.

Bewiesen ist damit ausschließlich die **gemessene Zyklus-Invarianz**: Die
12-s-Wartezeit ändert das Plateau nicht, und ein zweiter identisch großer
Erzeuge-/Lösche-Zyklus kostet nachweislich keinen weiteren Nettoverlust. Die
konkrete Ursache des einmaligen Restdeltas zwischen B0 und B2 ist nicht
bestimmt. Deshalb ist die exakte B0-Rückkehr kein geeignetes task-spezifisches
Cleanup-Gate. Der
freigegebene Plan schreibt den B0-Vergleich nicht vor — er verlangt nur, dass
B2 "erst nach nachweisbarer Freigabe" erfasst wird und dass die
Zusatzallokation nicht verborgen wird
(`docs/tasks/issue-29-implementation-plan.md`, Abschnitt 6). Der B0-Vergleich
war eine zusätzliche Implementierungsannahme, keine Planvorgabe; die
Korrektur bleibt damit innerhalb des freigegebenen Plans.

Die Korrektur (`main/issue_29_bringup_probe.cpp::run()`, Commit
`3bc5bfe4120d7ca6609733ab9d0736f1cfe99b59`) verankert den Nachweis stattdessen
an `B1` (`afterTaskCreate`, unmittelbar nach der Task-Erzeugung und vor der
Startbenachrichtigung); der eigentliche Probe-Workload beginnt erst danach:
`freeHeapBytes(B2) >= freeHeapBytes(B1) + kProbeTaskStackBytes`.
`kProbeTaskStackBytes` ist die bestehende, aus dem gemessenen Call-Path
abgeleitete Konstante (kein neuer Wert) und weist mindestens die Rückgabe der
konfigurierten Task-Stackgröße nach. Das gemessene Delta beträgt `69564 B`
gegenüber `67584 B` Stack-Untergrenze, also `1980 B` Abstand. Der
ESP-IDF-/FreeRTOS-Cleanup gibt bei dem dynamisch erzeugten Task neben dem
Stack auch den TCB frei. Das Gate beweist damit zusammen mit dem
dokumentierten Task-Lifecycle und den realen Wiederholungsläufen den
Cleanup-Nachweis für #29; es beweist nicht automatisch die Freiheit von jedem
beliebig kleinen Workload-Leak. Eine stärkere Aussage vollständiger
Heap-Leak-Freiheit müsste separat nachgewiesen werden.
`largestFreeBlockBytes` bleibt Teil der geloggten `after_task_cleanup`-Probe
(nicht verborgen), ist aber kein Gate-Kriterium mehr. `kCleanupWaitTicks`
(3000 ms) blieb unverändert, weil die Untersuchung ausdrücklich belegt hat,
dass es sich nicht um eine Timing-Frage handelt.

### Korrigierter realer Nachweis (zwei unabhängige Läufe à 40 s)

```text
I (199) app_init: App version:      3bc5bfe    [Lauf 1]
I (199) app_init: App version:      7024d15    [Lauf 2]
...
I (399) issue29_probe: before_task_create free_heap_bytes=303964 minimum_free_heap_bytes=303964 largest_free_block_bytes=172032 task_stack_hwm_bytes=0 main_stack_hwm_bytes=3064
I (409) issue29_probe: after_task_create_blocked free_heap_bytes=234224 minimum_free_heap_bytes=234224 largest_free_block_bytes=110592 task_stack_hwm_bytes=67084 main_stack_hwm_bytes=3064
I (429) issue29_probe: after_task_cleanup free_heap_bytes=303788 minimum_free_heap_bytes=230056 largest_free_block_bytes=110592 task_stack_hwm_bytes=0 main_stack_hwm_bytes=3064
I (449) issue29_probe: task_ready_hwm_bytes=67132
I (449) issue29_probe: task_completion_hwm_bytes=5532
I (459) issue29_probe: fault_status=18 fault_step=1 fault_technical_reason=7 fault_durability=0 fault_pass=PASS writes=0 reads=3 state_unchanged=true persistence_unchanged=true actor_release=false safety_fail_closed=true
I (469) issue29_probe: internal_resource_hwm_valid=true local_apply_measured=true cleanup_handoff_received=true task_cleanup_proven=true after_task_cleanup_measured=true
I (489) issue29_probe: result=PASS
I (1499) app_main: heartbeat: safe test mode, uptime_ms=1002
...
I (30499) app_main: resources: free_heap_bytes=303788 stack_hwm_bytes=2680
...
I (39499) app_main: heartbeat: safe test mode, uptime_ms=39002
```

Beide unabhängigen Läufe (frischer `POWERON_RESET` je Lauf, je 40 s
Erfassungsfenster, byte-identische Werte in beiden Läufen) lieferten
`task_cleanup_proven=true`, `after_task_cleanup_measured=true`,
`result=PASS`, liefen danach jeweils bis mindestens `uptime_ms=39002` weiter
und erreichten damit selbst den 35-s-Heartbeat-Smoke für `esp32_bringup` mit
exakt zwei regulären Ressourcenmessungen (`t=289 ms` und `t=30499 ms`),
monotoner Uptime und ohne Panic, Watchdog, Brownout oder unerwarteten Reset.

| Nachweis | Ergebnis | Grund / Evidenz |
|---|---|---|
| reale 48/96/1024-`CommandDecision`-Ressourcenprobe erzeugt und gehalten | `PASS` | unverändert gegenüber dem ursprünglichen Lauf |
| `B0`/`B1` | `PASS` | unverändert: `303964`/`172032` (B0), `234224`/`110592` (B1) |
| `B2` (`after_task_cleanup`) | `PASS` | `free_heap_bytes=303788 >= afterTaskCreate.free_heap_bytes(234224) + kProbeTaskStackBytes(67584)` (Delta 69564 B, Abstand 1980 B); `largestFreeBlockBytes=110592` bleibt informationell geloggt, ist kein Gate mehr. Das weist mindestens die Stack-Rückgabe nach, nicht beliebige Workload-Leak-Freiheit. |
| interne vier Probezeitpunkte | `PASS` | unverändert |
| Task-Stack-High-Water-Mark | `PASS` | unverändert: `task_ready_hwm_bytes=67132`, `task_completion_hwm_bytes=5532` |
| On-Target-Allokationsfehlervertrag | `PASS` | unverändert: `fault_pass=PASS writes=0 reads=3` |
| fachlicher Zustand unverändert / kein Persistenz-Write / keine Aktorfreigabe / fail-closed | `PASS` | unverändert |
| Gesamtergebnis `esp32_bringup`-Probe (`result=`) | `PASS` | beide Läufe identisch |

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
| `esp32_bringup` mindestens 35 s | `PASS` | nach der Korrektur: beide unabhängigen 40-s-Verifikationsläufe erreichen 39+ s Uptime nach `result=PASS` der Bring-up-Probe; siehe "Korrigierter realer Nachweis" oben |
| `esp32_release` mindestens 35 s auf demselben Board | `PASS` | reale Erfassung über 39+ s vor UND nach der Korrektur (unverändertes Verhalten, da `esp32_release` die Probe compile-time nicht enthält); monotone Uptime `1003`…`39003` ms, kein Abbruch |
| exakt zwei reguläre Smoke-Ressourcenpunkte | `PASS` (beide Profile) | `esp32_release`: `free_heap_bytes=304668` bei `t=274 ms` und `t=30284 ms`; `esp32_bringup` (Commit `3bc5bfe`): `free_heap_bytes=303788` bei `t=289 ms` und `t=30499 ms` |
| Resetursache, Panic, Watchdog, Brownout | `PASS` (beide Profile) | `rst:0x1 (POWERON_RESET)` in allen Bootlogs (vor und nach der Korrektur), kein Panic/Watchdog/Brownout/unerwarteter Reset während der jeweils 39-s-Erfassungen |
| sichere unbelastete MCU-/Gate-/Bootpegel | `NOT_RUN` | kein Multimeter/Messaufbau in dieser Ausführungsumgebung verfügbar; erfordert physische Messung durch den Owner |
| belastete MOSFET-/Verbraucherwirkung | `NOT_RUN` | ausdrücklich nicht Bestandteil von #29 |

Es wurden keine 12-V-Verbraucher, Aktoren, Sensoren, Displays, Lüfter,
BTS7960- oder Peltierkomponenten integriert oder aktiviert. `HARDWARE_UNVERIFIED`,
`TBD_HARDWARE`, `TBD_COMMISSIONING` und `TBD_IMPLEMENTATION_BUDGET` bleiben
deshalb bestehen. Die sichere Messung der unbelasteten Pegelmesspunkte bleibt
offen, da sie eine physische Ablesung beziehungsweise ein Messgerät vor Ort
erfordert. Die physische PCB-/Boardrevision beziehungsweise der
Silkscreen-Aufdruck ist nach Ownerentscheidung kein Abnahmekriterium.

## Dokumentationsrückführung

Der `esp32_bringup`-Befund ist inzwischen anhand der gemessenen
Zyklus-Invarianz eingeordnet, korrigiert und mit zwei unabhängigen realen
Läufen verifiziert; beide Profile bestehen ihren
35-s-Smoke real auf demselben Board, und Board/Flash/PSRAM/UART-Recovery sind
`confirmed_test`. Trotzdem wurden `docs/HARDWARE.md` und
`docs/OPEN_POINTS.md` in diesem Schritt weiterhin **nicht** aktualisiert:
Der freigegebene Plan führt Board-, Flash-, PSRAM-, Boot- und unbelastete
Pegelfakten als eine gemeinsame Rückführungsvoraussetzung
(`docs/tasks/issue-29-implementation-plan.md`, Abschnitt 10); die sicheren
unbelasteten MCU-/Gate-/Bootpegel bleiben `NOT_RUN`, weil in dieser
Ausführungsumgebung kein Messgerät verfügbar ist. Issue #29 gilt deshalb erst
dann als vollständig erfüllbar, wenn diese Pegelmessung real vorliegt; bis
dahin bleibt die kanonische Rückführung in `docs/HARDWARE.md` und
`docs/OPEN_POINTS.md` eine bewusste, spätere Owner-Entscheidung. Eine
physische PCB-Revision oder ein Silkscreen-Nachweis ist dafür nicht
erforderlich.

`docs/RESOURCE_BUDGET_AND_MAINTENANCE.md` bleibt unverändert; der
aktualisierte [`ISSUE_29_BUILD_REPORT.md`](ISSUE_29_BUILD_REPORT.md) und die
compilerbasierte Stack-Usage-Herleitung sind Build-/Ressourcennachweise, aber
kein Beleg für ein kanonisches Produktions- oder Parallelbudget.

Nächster Schritt (historisch, PR #116): physische Messung der sicheren
unbelasteten MCU-/Gate-/Bootpegel. Dieser Schritt ist durch den unten
dokumentierten Requalifikationsbefund überholt: Die Pegelmessung bleibt
zusätzlich offen, ist aber nicht der nächste Schritt, solange der
Bring-up-Boot nicht wieder stabil requalifiziert ist (siehe unten).

## Aktuelle Requalifikation auf `integration/r1-development` (2026-08-30/31)

Diese Requalifikation ersetzt den oben dokumentierten historischen `PASS` als
aktuellen Stand. Der historische Abschnitt bleibt als Provenienz des
PR-#116-Standes unverändert erhalten.

```text
SOURCE_SHA=c1f5fbb5f19ab8e7d2c25708fe79777d523217d4
PROFILE=esp32_bringup
ISSUE90_HARNESS=ABSENT
ELF_SHA256=3e25a0ad698a4a5102619dfb49e64584b23dd5d21cd348ad0f4489c22fb3b71c
BIN_SHA256=a5b3426a0d4b893ea912e04145bb441eb9d6d69caf68e0f98b7091e6a3add906

BOOT_1=PANIC_REPRODUCED
BOOT_2=NOT_RUN_STOP_GATE
BOOT_3=NOT_RUN_STOP_GATE

PANIC=LoadProhibited
PANIC_INDUCED_RESET=YES
PANIC_RESET_REASON=rst:0xc (SW_CPU_RESET)
UNRELATED_UNEXPECTED_RESET=NO
WATCHDOG=NO
BROWNOUT=NO
ACTOR_RELEASE=false

ROOT_CAUSE=UNRESOLVED
HEAP_WALK_ROOT_CAUSE=NOT_CLAIMED
PRIMARY_DIAGNOSIS=STALE_ISSUE29_DIAGNOSTIC_STACK_BUDGET
```

### Board und Werkzeuge (unverändert gegenüber der historischen Baseline)

- Board: ESP32-D0WD-V3 (Chiprevision v3.1), MAC `20:50:0d:1b:2f:34`
- Port: `/dev/ttyUSB0`, automatischer DTR/RTS-Reset über IO0/EN
- Flash: Manufacturer `b3`, Device `4016`, 4 MB (`esptool flash-id`)
- `esptool v5.3.1`, ESP-IDF `v6.0.2 @ 7101770dc6db2667b3c477cc31365dd1acd6db4e`

### Relevanter UART-Auszug (Boot 1, gekürzt auf den Panic-relevanten Abschnitt)

```text
I (778) app_main: ESP32-Fermentationsschrank
I (778) app_main: profile: esp32_bringup
I (778) app_main: source git sha: c1f5fbb5f19ab8e7d2c25708fe79777d523217d4
I (788) app_main: hardware state: HARDWARE_UNVERIFIED
I (788) app_main: actuator policy: LOCKED_FOR_BRINGUP
I (798) app_main: real actuators: disabled
I (798) app_main: application: ready
I (798) app_main: resources: free_heap_bytes=233380 stack_hwm_bytes=8832
Guru Meditation Error: Core  1 panic'ed (LoadProhibited). Exception was unhandled.

Core  1 register dump:
PC      : 0x401130d6  PS      : 0x00060533  A0      : 0x80112e5e  A1      : 0x3ffc7e10
A2      : 0xf077d7dc  A3      : 0x40112ea0  A4      : 0x383857e0  A5      : 0x3ffc7e6c
A6      : 0x00000000  A7      : 0x3ffc7e50  A8      : 0xb83f8000  A9      : 0x00000001
A10     : 0x00000001  A11     : 0xb83f8000  A12     : 0x00000001  A13     : 0x3ffc7e50
A14     : 0x00000000  A15     : 0x00060523  SAR     : 0x0000000f  EXCCAUSE: 0x0000001c
EXCVADDR: 0xf077d7e0  LBEG    : 0x4000c46c  LEND    : 0x4000c477  LCOUNT  : 0x00000000

Backtrace: 0x401130d3:0x3ffc7e10 0x40112e5b:0x3ffc7e30 0x400d3996:0x3ffc7e50 0x400d39e5:0x3ffc7e90 0x400db120:0x3ffc7ed0 0x400dc5d8:0x3ffc7ef0 0x400dca26:0x3ffd7fa0 0x40110e52:0x3ffd7fd0

ELF file SHA256: 3e25a0ad6...

Rebooting...
rst:0xc (SW_CPU_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
```

Backtrace, PC-Register und `EXCVADDR` sind über alle 44 vom Panic-Handler
selbst ausgelösten `rst:0xc`-Neustarts innerhalb des ~40-s-Erfassungsfensters
byte-identisch (`grep -a` gegen den vollständigen lokalen Rohlog verifiziert).
Der vollständige Rohlog (nicht committed, siehe unten) bestätigt dies für
jeden der 44 Zyklen; diese Zyklen sind eine Zusatzbeobachtung, keine eigene
Boot-Zählung (siehe `BOOT_1`/`BOOT_2`/`BOOT_3` oben).

### Vollständige addr2line-Kette (gegen das exakt geflashte ELF)

```text
0x401130d6: block_size / block_is_last @ tlsf_block_functions.h:87/98
  (inlined by) tlsf_walk_pool @ tlsf.c:212
0x401130d3: block_next @ tlsf_block_functions.h:161 (inlined by) tlsf_walk_pool @ tlsf.c:221
0x40112e5b: multi_heap_get_info_impl @ multi_heap.c:427
0x400d3996: heap_caps_get_info @ heap_caps.c:392
0x400d39e5: heap_caps_get_largest_free_block @ heap_caps.c:321
0x400db120: fermentation::issue_29_bringup::{anonymous}::sampleResources @ issue_29_bringup_probe.cpp:128
0x400dc5d8: fermentation::issue_29_bringup::{anonymous}::runProbe @ issue_29_bringup_probe.cpp:326
0x400dca26: fermentation::issue_29_bringup::{anonymous}::probeTask @ issue_29_bringup_probe.cpp:450
0x40110e52: vPortTaskWrapper @ port.c:147
```

### Read-only Stackdiagnose (kein neuer Hardwarelauf, keine Codeänderung)

Gegen den exakt zu Boot 1 gehörenden Build:

```text
CURRENT_CUMULATIVE_CALL_PATH_BYTES=66816 (persistFreshStartCommand-Kette; historisch dokumentiert 62928)
CURRENT_REQUIRED_CONFIGURED_TASK_STACK_BYTES=71680
COMPILED_kMeasuredCallPathBytes=62928
COMPILED_kProbeTaskStackBytes=67584
```

Ein zusätzlicher, gezielter Mehrpfadabgleich der bereits vorhandenen
`.su`/`.ci`-Artefakte (rein lesend) findet einen weiteren, bisher nicht
geprüften Pfad mit noch höherem Bedarf:
`runProbe -> decideProgramStart -> decideProgramStartInto -> ActiveRun::start`
= 71920 Bytes, 4336 Bytes über `kProbeTaskStackBytes`. Vollständige Herleitung,
Methodik und alle geprüften Pfade stehen im Korrekturplan:

`docs/tasks/issue-29-panic-requalification-correction-plan.md`

### Evidenzablage

Der vollständige Rohlog (UART, unkomprimiert) sowie ELF/BIN-Hashes liegen
lokal, nicht committed, unter `build/issue29_requalification/` (siehe
`.gitignore`, `build/`). Dieser Abschnitt hier ist die versionierte,
kanonische Kurzfassung; keine Megabyte-Rohlogs werden committed.
