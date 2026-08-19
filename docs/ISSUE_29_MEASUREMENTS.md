# Issue #29 Messprotokoll und Abnahmestatus

Status dieses Protokolls: Software-/Buildnachweise `PASS`. Reale Board-/UART-/
Flash-/PSRAM-Nachweise sind `PASS`. `esp32_release` und (nach der unten
dokumentierten Ursachenanalyse und Korrektur) `esp32_bringup` bestehen beide
real den 35-Sekunden-Smoke auf demselben Board, jeweils mit zwei unabhängigen
Läufen für `esp32_bringup` reproduziert. Offen bleiben ausschließlich die
sicheren unbelasteten MCU-/Gate-/Bootpegel (kein Messgerät in dieser
Ausführungsumgebung) sowie die physische PCB-Revision (Silkscreen).

Implementierungsstatus: `SOFTWARE_IMPLEMENTED_HARDWARE_TESTED_PASS_PENDING_LEVELS`;
beide Profile bestehen real auf dem Board; die Issue-Abnahme ist erst
vollständig, wenn zusätzlich die unbelasteten Pegelmesspunkte real
nachgewiesen sind.

## Identität und Scope

- Issue: #29
- PR: #116, Draft
- Branch: `agent/issue-29-esp32-bringup-plan`
- Basis: `main @ 87dd593fcdc8d26831873a4163b174340b4347c0`
- freigegebener Plan:
  `docs/tasks/issue-29-implementation-plan.md @ 4f49b44cff47f55bfd425d9e39c5a07256782ed7`
- Implementierungs-HEAD (Firmwareinhalt): `5950814fc21be557e565dad3aa6acf3dbe3c0b64`
- erster realer Hardwarelauf (`esp32_bringup` `FAILED`, `esp32_release`
  `PASS`): PR-HEAD `c4c8b33f4dbaef727200ea410d887ec5417aa1b0`
  (`App version: c4c8b33`); Firmwareinhalt identisch zum
  Implementierungs-HEAD `5950814` (nur Dokumentänderungen dazwischen).
- korrigierter, real verifizierter Hardwarelauf (beide Profile `PASS`):
  Commit `3bc5bfe4120d7ca6609733ab9d0736f1cfe99b59` (`fix(issue-29): anchor B2 cleanup proof on B1, not
  B0`) — einzige Firmwareänderung gegenüber `c4c8b33`/`5950814`: die
  B2-Nachweislogik in `main/issue_29_bringup_probe.cpp::run()` (siehe
  Abschnitt "Root Cause und Korrektur" unten). Zwei unabhängige
  `esp32_bringup`-Verifikationsläufe mit je 40 s Erfassungsfenster: der
  erste vor dem folgenden Dokumentationscommit (`App version: 3bc5bfe`), der
  zweite danach (`App version: 7024d15`, aktueller HEAD). `git diff --stat
  3bc5bfe4120d7ca6609733ab9d0736f1cfe99b59
  7024d15df6bd9c647e91416f5ab3b66d13035f62` zeigt ausschließlich
  Dokumentänderungen (`docs/ISSUE_29_BUILD_REPORT.md`,
  `docs/ISSUE_29_MEASUREMENTS.md`); beide Läufe sind damit derselbe
  Firmwareinhalt und lieferten byte-identische Messwerte.
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

### Root Cause und Korrektur

Zwei gezielte, real auf demselben Board durchgeführte Experimente (nur die
Wartelogik in `run()` temporär verändert, `probeTask`/`runProbe` und damit
der von `scripts/analyze_issue_29_stack.py` geprüfte Call-Path blieben
unberührt) klärten die Ursache:

1. **Erweitertes Zeitfenster (12 s statt 3 s), Trace alle 100 ms:**
   `largest_free_block_bytes` blieb über die vollen 12 s exakt bei `110592`
   eingefroren, ohne jede Bewegung. `free_heap_bytes` erholte sich dagegen
   schon nach dem ersten Poll (t=100 ms) auf `303788` und blieb dort ebenso
   flach — 176 Bytes unter der B0-Baseline (`303964`), aber stabil.
   → schließt eine reine Timing-Ursache aus (Kategorie 2): ein längeres
   Zeitfenster hätte den Wert nie näher an B0 gebracht.
2. **Zweiter, identisch großer Task-Erzeuge/Lösche-Zyklus** direkt im
   Anschluss (gleicher `kProbeTaskStackDepth`, sofortiges
   `vTaskDelete(nullptr)`): `free_heap_bytes` und `largest_free_block_bytes`
   waren nach diesem zweiten Zyklus bit-identisch zu den Werten davor
   (`303788`/`110592` → `303788`/`110592`).
   → schließt ein echtes Leck pro Task-Zyklus aus (Kategorie 1): ein Leck
   hätte beim zweiten Zyklus weiteren Speicher gekostet.

Damit bleibt nur Kategorie 3: Die Freigabe des Diagnose-Tasks funktioniert
korrekt und ist bereits nach dem ersten Poll abgeschlossen; das bisherige
Kriterium `largestFreeBlockBytes(B2) >= largestFreeBlockBytes(B0)` **und**
`freeHeapBytes(B2) >= freeHeapBytes(B0)` vermischt diese echte
Task-Reclamation mit einer Heap-Layout-Wirkung, die bereits bei `B1` vorliegt.
Bewiesen ist dabei ausschließlich die **Zyklus-Invarianz**: Experiment 2 zeigt
messtechnisch eindeutig, dass diese Wirkung nicht mit dem Diagnose-Task-
Lifecycle skaliert — ein zweiter, identisch großer Erzeuge-/Lösche-Zyklus
kostet nachweislich null zusätzliche Bytes. Die naheliegende Erklärung, dass
es sich um eine einmalige Lazy-Initialisierung beim allerersten `xTaskCreate`
dieses Profils handelt, ist dagegen ausdrücklich **nicht** untersucht oder
belegt und wird hier nicht als Tatsache behauptet — für die Korrektur ist nur
die bewiesene Zyklus-Invarianz relevant, nicht deren Ursache. Der
freigegebene Plan schreibt den B0-Vergleich nicht vor — er verlangt nur, dass
B2 "erst nach nachweisbarer Freigabe" erfasst wird und dass die
Zusatzallokation nicht verborgen wird
(`docs/tasks/issue-29-implementation-plan.md`, Abschnitt 6). Der B0-Vergleich
war eine zusätzliche Implementierungsannahme, keine Planvorgabe; die
Korrektur bleibt damit innerhalb des freigegebenen Plans.

Die Korrektur (`main/issue_29_bringup_probe.cpp::run()`, Commit
`3bc5bfe4120d7ca6609733ab9d0736f1cfe99b59`) verankert den Nachweis stattdessen
an `B1` (`afterTaskCreate`, unmittelbar nach der Task-Erzeugung, vor jedem
Probe-Workload): `freeHeapBytes(B2) >= freeHeapBytes(B1) + kProbeTaskStackBytes`.
`kProbeTaskStackBytes` ist die bereits vorhandene, aus dem gemessenen
Call-Path abgeleitete Konstante (kein neuer Wert) und damit eine konservative,
kausal dem Diagnose-Task zuordenbare Untergrenze — ein echtes Ausbleiben der
Freigabe hätte diesen Schwellenwert weiterhin nicht erreicht. Die
B1-Verankerung kann außerdem keinen Heapverlust aus dem eigentlichen
Probe-Workload (Decision-/Kandidat-/String-Allokationen vor `B1`) verdecken:
ein solcher Verlust würde das Delta zwischen `B1` und `B2` verkleinern und
den Schwellenwert damit eher verfehlen, nie fälschlich erfüllen.
`largestFreeBlockBytes` bleibt Teil der geloggten `after_task_cleanup`-Probe
(nicht verborgen), ist aber kein Gate-Kriterium mehr. `kCleanupWaitTicks`
(3000 ms) blieb unverändert, weil die Untersuchung ausdrücklich belegt hat,
dass es sich nicht um eine Timing-Frage handelt.

### Korrigierter realer Nachweis (zwei unabhängige Läufe à 40 s)

```text
I (199) app_init: App version:      3bc5bfe    [Lauf 1] / 7024d15 [Lauf 2]
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
| `B2` (`after_task_cleanup`) | `PASS` | `free_heap_bytes=303788 >= afterTaskCreate.free_heap_bytes(234224) + kProbeTaskStackBytes(67584)` (Delta 69564 B); `largestFreeBlockBytes=110592` bleibt informationell geloggt, ist kein Gate mehr |
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
deshalb bestehen. Die physische PCB-/Boardrevision (Silkscreen-Aufdruck) und
die sicheren unbelasteten Pegelmesspunkte bleiben ebenfalls offen, da sie
eine physische Ablesung beziehungsweise ein Messgerät vor Ort erfordern.

## Dokumentationsrückführung

Der `esp32_bringup`-Befund ist inzwischen ursachenanalysiert, korrigiert und
mit zwei unabhängigen realen Läufen verifiziert; beide Profile bestehen ihren
35-s-Smoke real auf demselben Board, und Board/Flash/PSRAM/UART-Recovery sind
`confirmed_test`. Trotzdem wurden `docs/HARDWARE.md` und
`docs/OPEN_POINTS.md` in diesem Schritt weiterhin **nicht** aktualisiert:
Der freigegebene Plan führt Board-, Flash-, PSRAM-, Boot- und unbelastete
Pegelfakten als eine gemeinsame Rückführungsvoraussetzung
(`docs/tasks/issue-29-implementation-plan.md`, Abschnitt 10); die sicheren
unbelasteten MCU-/Gate-/Bootpegel bleiben `NOT_RUN`, weil in dieser
Ausführungsumgebung kein Messgerät verfügbar ist. Issue #29 gilt deshalb erst
dann als vollständig erfüllbar, wenn auch diese Pegelmessung real vorliegt;
bis dahin bleibt die kanonische Rückführung in `docs/HARDWARE.md` und
`docs/OPEN_POINTS.md` eine bewusste, spätere Owner-Entscheidung.

`docs/RESOURCE_BUDGET_AND_MAINTENANCE.md` bleibt unverändert; der
aktualisierte [`ISSUE_29_BUILD_REPORT.md`](ISSUE_29_BUILD_REPORT.md) und die
compilerbasierte Stack-Usage-Herleitung sind Build-/Ressourcennachweise, aber
kein Beleg für ein kanonisches Produktions- oder Parallelbudget.

Nächster Schritt: physische Messung der sicheren unbelasteten
MCU-/Gate-/Bootpegel (Multimeter/Messaufbau erforderlich) sowie Ablesen der
physischen PCB-Revision (Silkscreen). Erst danach sind alle in Plan-Abschnitt
5 geforderten #29-Hardwarekriterien belegt und die Rückführung in
`docs/HARDWARE.md`/`docs/OPEN_POINTS.md` sowie eine vollständige
Issue-Abnahme möglich.
