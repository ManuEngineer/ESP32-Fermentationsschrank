# Issue #29 – Panic-Requalifikation: Ursachenrahmen und Korrekturplan

Diese Planrevision ist die **einzige kanonische Planquelle** für die am
2026-08-30 auf der aktuellen R1-Integrationsbaseline real reproduzierte
Panic-Regression im `esp32_bringup`-Bring-up-Probe von Issue #29. Sie enthält
selbst alle für ihre Umsetzung notwendigen Regeln (Bring-up-only-Diagnose,
Isolation, Verbote, Abnahme) und lässt keinen anderen Plan normativ
weitergelten.

Der historische
`docs/tasks/issue-29-implementation-plan.md @ 4f49b44cff47f55bfd425d9e39c5a07256782ed7`
bleibt ausschließlich als **Provenienz** stehen (ursprüngliche Herkunft der
Probe, historische B0/B1/B2-Herleitung, historische Hardwareabnahme aus PR
#116). Für alles, was diese Korrekturrunde betrifft, gilt ab jetzt
ausschließlich dieser Plan.

## 1. Status, Basis und Owner-Gate

- Repository: `ManuEngineer/ESP32-Fermentationsschrank`
- Aktuelle Integrationsbasis: `integration/r1-development @ c1f5fbb5f19ab8e7d2c25708fe79777d523217d4`
  (PR #128 gemergt; Issue #90 `CLOSED/COMPLETED`)
- ESP-IDF: `v6.0.2`, Commit `7101770dc6db2667b3c477cc31365dd1acd6db4e`
- Branch dieses Plans: `agent/issue-29-requalification-r1`
- PR: #129 (Draft, Basis `integration/r1-development`)
- Kanonischer Plan: `docs/tasks/issue-29-panic-requalification-correction-plan.md`
- Historische Provenienz (nicht normativ für diese Korrektur):
  `docs/tasks/issue-29-implementation-plan.md @ 4f49b44cff47f55bfd425d9e39c5a07256782ed7`
- Planfreigabe: Owner muss die exakte Commit-SHA dieser Revision freigeben,
  bevor irgendein Schritt aus Abschnitt 6 implementiert wird.

### Verbindliche Regeln dieser Korrekturrunde (selbst enthalten, kein Verweis)

- **Bring-up-only-Diagnose:** Jede neue oder geänderte Instrumentierung bleibt
  ausschließlich im `esp32_bringup`-Profil aktiv, hinter
  `APP_ISSUE_29_BRINGUP_PROBE`/einem gleichwertigen Compile-Time-Gate.
  `esp32_release` und `native` enthalten sie nicht.
- **Actor-free:** Jede künftige Hardwarenutzung dieses Plans erfolgt ohne
  12-V-Verbraucher, ohne Peltier, ohne Lüfter, ohne reale
  MOSFET-/BTS7960-Ansteuerung.
- **Keine Produkt-/Main-Task-Stackerhöhung:** `CONFIG_ESP_MAIN_TASK_STACK_SIZE`
  und jede andere produktive Taskgröße bleiben unverändert. Nur die
  diagnoseeigene `kProbeTaskStackBytes`/Nachfolgekonstante darf sich ändern,
  und nur begründet aus Abschnitt 6.1.
- **Keine Produktlogik-/Persistenz-/Safety-Änderung:** `fermentation_app`,
  `device_platform`, `device_platform_esp_idf` bleiben fachlich unverändert.
  Diese Korrektur ist ausschließlich Diagnose-Infrastruktur.
- **Compile-Time-Isolation aus Release bleibt erhalten** und wird nach jeder
  Änderung erneut nachgewiesen (Compile-Command-/Symbolprüfung wie in
  `docs/ISSUE_29_MEASUREMENTS.md`, Abschnitt "Compile-time-Isolation").
- **Fault-Seam unverändert**, sofern kein aus Abschnitt 3/4 tatsächlich
  nachgewiesener Befund ihn betrifft. `main/issue_29_bringup_fault_seam.hpp`
  (`lib/fermentation_app/private/issue_29_bringup_fault_seam.hpp`) wird nicht
  vorsorglich angefasst.
- **Reihenfolge:** Erst 3 unabhängige reale Boots à mindestens 35 s, nachdem
  das digitale Stack-Gate aus Abschnitt 6.2/6.3 `PASS` ist. Pegelmessungen
  erst danach (Abschnitt 6.5).
- **#25 bleibt blockiert**, bis Issue #29 Owner-final entschieden ist.

### Unbedingtes Stop-Gate

Bis zur ausdrücklichen Ownerfreigabe der exakten Plan-SHA sind ausschließlich
erlaubt:

1. dieser Plan-Commit und die zugehörige Roadmap-/Issue-/Messprotokoll-
   Synchronisation;
2. Draft-PR-Pflege (#129);
3. weiterer read-only Nachweis gegen bereits vorhandene Buildartefakte (kein
   neuer Build, kein Flashen, keine Codeänderung).

Nicht erlaubt: jede Änderung an `main/**`, `lib/**`, `scripts/**`,
`sdkconfig*`, `CMakeLists.txt`, `.github/**`.

## 2. Requalifikationsbefund (Kurzfassung)

Vollständiger Nachweis in `docs/ISSUE_29_MEASUREMENTS.md`, Abschnitt "Aktuelle
Requalifikation auf `integration/r1-development`". Kurzfassung:

```text
SOURCE_SHA=c1f5fbb5f19ab8e7d2c25708fe79777d523217d4
PROFILE=esp32_bringup (ohne #90-Harness)

BOOT_1=PANIC_REPRODUCED
BOOT_2=NOT_RUN_STOP_GATE
BOOT_3=NOT_RUN_STOP_GATE

PANIC=Guru Meditation Error (LoadProhibited)
PANIC_INDUCED_RESET=YES
PANIC_RESET_REASON=rst:0xc (SW_CPU_RESET)
UNRELATED_UNEXPECTED_RESET=NO
WATCHDOG=NO
BROWNOUT=NO
ACTOR_RELEASE=false (gültig bis zum Panic)
```

Die vom Panic-Handler selbst ausgelösten automatischen `rst:0xc`-Neustarts
(44 Zyklen innerhalb des ~40-s-Erfassungsfensters, byte-identischer
Backtrace/PC/`EXCVADDR`) sind eine **zusätzliche Beobachtung**, keine
eigene Boot-Zählung. Die kanonische Testzählung bleibt `BOOT_1=
PANIC_REPRODUCED`, `BOOT_2`/`BOOT_3=NOT_RUN_STOP_GATE`.

`PANIC_ROOT_CAUSE` wird **nicht** als `heap_caps_get_largest_free_block`
behauptet. Der reale Backtrace beweist ausschließlich, dass der Absturz dort
**beobachtet** wird:

```text
probeTask -> runProbe -> sampleResources
  -> heap_caps_get_largest_free_block -> heap_caps_get_info
  -> multi_heap_get_info_impl -> tlsf_walk_pool/block_is_last
```

Ein Heap-Walk kann der Ort sein, an dem eine bereits vorher entstandene
Speicher-/Stackkorruption erst sichtbar wird. `ROOT_CAUSE=UNRESOLVED` bis zu
einem tatsächlichen Nachweis; `HEAP_WALK_ROOT_CAUSE=NOT_CLAIMED`.

## 3. Warum "Maximum aus zwei bekannten Pfaden" nicht ausreicht

Die vorherige Planrevision wollte den korrigierten Stackwert aus dem Maximum
von genau zwei Pfaden bilden: der bereits von
`scripts/analyze_issue_29_stack.py` geprüften
"Candidate-Allocation-Failure"-Kette und der real abgestürzten
`sampleResources`/Heap-Walk-Kette. Das ist nachweislich unvollständig.

`runProbe()` (`main/issue_29_bringup_probe.cpp:303-393`) enthält mehrere
weitere, voneinander unabhängige verschachtelte Aufrufphasen, bevor und
zusätzlich zu diesen beiden Ketten. Ein gezielter, rein lesender Abgleich der
bereits vorhandenen `.su`/`.ci`-Artefakte aus dem exakt zu Boot 1 gehörenden
Build (kein neuer Build, keine Codeänderung) gegen die tatsächlichen
Aufrufphasen von `probeTask()`/`runProbe()` ergibt:

| Phase | Pfad (aus dem exakt zugehörigen aktuellen Build statisch hergeleitet) | Kumulierte Bytes | vs. `kProbeTaskStackBytes` (67584) |
|---|---|---:|---|
| P0 | `probeTask` Eintritt/Control (`probeTask` 48 + `waitForTaskControl` 32) | 80 | unauffällig |
| P1 | `runProbe`→`maximalStartRequest`(256)→`maximalProgram`(320)→`repeatedUtf8`(32) | 66368 | −1216 |
| P2 | `runProbe`→`sampleResources`(32) — **der real abgestürzte Aufruf** | 65792 | −1792 (Rest für `heap_caps_get_largest_free_block`/`heap_caps_get_info`/`multi_heap_get_info_impl`/`tlsf_walk_pool` **unvermessen**, siehe Abschnitt 4) |
| P3 | `runProbe`→`decideProgramStart`(32)→`decideProgramStartInto`(3072)→`ActiveRun::start`(3056) | **71920** | **+4336** |
| P4 | `runProbe`→`applyCandidateForResourceProbe`(32)→`applyRunCommand`(32) | 65824 | −1760 (weitere Verschachtelung unter `applyRunCommand` noch nicht abschließend geprüft) |
| P5 | `runProbe`→`persistFreshStartCommand`(32)→`orchestrator.persistCommand`(304)→`coordinator.persistCommand`(688)→`coordinator.result`(32) — die bisher einzig geprüfte Kette | 66816 | −768 |
| P6 | `runProbe`→`unchangedStandbyState`(4816) | **70576** | **+2992** |

(Jede Zeile: `probeTask`(48) + `runProbe`(65712) + die genannte Kette; alle
Frames `static`, alle zitierten Kanten im `.ci`-Callgraph vorhanden.)

**P3 ist der aktuell bekannte schlechteste Pfad** — 4336 Bytes über der
kompilierten Diagnose-Taskgröße, ohne dass dafür überhaupt der Heap-Walk
erreicht werden muss. P6 liegt ebenfalls über der konfigurierten Größe. Diese
Tabelle ist ausdrücklich **vorläufig und nicht abschließend**: P2 (unvermessener
Heap-Walk-Anteil) und P4 (nicht vollständig verschachtelt geprüft) können den
tatsächlichen globalen Maximalpfad noch verschieben. Sie beweist aber bereits
empirisch, dass ein Zwei-Pfad-Vergleich nicht genügt, und bestätigt
`PRIMARY_DIAGNOSIS=STALE_ISSUE29_DIAGNOSTIC_STACK_BUDGET` zusätzlich zum
bereits bekannten P5-Befund.

```text
ROOT_CAUSE=UNRESOLVED
HEAP_WALK_ROOT_CAUSE=NOT_CLAIMED
```

Kein Pfad allein wird als Ursache des real beobachteten Boot-1-Absturzes
behauptet; P2 ist der Pfad, auf dem der reale Absturz beobachtet wurde, P3 und
P6 sind unabhängig davon bereits jetzt statisch nachweisbare
Budgetüberschreitungen auf demselben Task.

## 4. Geforderte Entscheidungen für die spätere Umsetzung (Planungsarbeit)

Die folgenden Punkte sind **Entscheidungen dieses Plans**. Ihre
**Implementierung beginnt erst nach separater Ownerfreigabe dieser exakten
Plan-SHA** (`IMPLEMENTATION=NOT_STARTED`).

### 4.1 `ISSUE29_DIAGNOSTIC_TASK_STATIC_STACK_GATE`: vollständige Mehrpfadprüfung

```text
ISSUE29_DIAGNOSTIC_TASK_STATIC_STACK_GATE
=
maximum of all relevant statically reachable probe-task call paths
```

`scripts/analyze_issue_29_stack.py` wird gezielt erweitert (KISS: derselbe
bestehende Mechanismus, keine neue generische Callgraph-Plattform). Die
Erweiterung muss mindestens die folgenden Ablaufphasen von
`probeTask()`/`runProbe()` vollständig abdecken (Leitlinie, keine Umbenennung
der Funktionen verlangt):

```text
P0 task entry/control        (probeTask, waitForTaskControl, deleteProbeTask)
P1 request/state construction (standbyState, maximalStartRequest, maximalProgram, repeatedUtf8)
P2 resource sampling/heap walk (sampleResources -> heap_caps_get_largest_free_block
                                 -> heap_caps_get_info -> multi_heap_get_info_impl
                                 -> tlsf_walk_pool, siehe 4.2 für die fehlende Instrumentierung)
P3 decision creation          (decideProgramStart -> decideProgramStartInto -> ActiveRun::start
                                 und alle weiteren .ci-Kanten von decideProgramStartInto)
P4 local candidate apply      (applyCandidateForResourceProbe -> applyRunCommand
                                 und dessen vollständige .ci-Kanten)
P5 fault-seam/persistence     (persistFreshStartCommand -> orchestrator.persistCommand
                                 -> coordinator.persistCommand -> coordinator.result)
P6 completion/cleanup-relevant task path (unchangedStandbyState und die spätere,
                                 innerhalb desselben Tasks laufende deleteProbeTask-Kette)
```

Für jeden Pfad, den das Skript prüft:

```text
ALL_FRAMES_QUALIFIER=STATIC
ALL_REQUIRED_COMPILED_EDGES=PRESENT
CUMULATIVE_BYTES=<measured>
```

Eine fehlende Kante, ein `dynamic`-/`unbounded`-Qualifier:

```text
STACK_GATE=BLOCKED
STOP
```

Das Skript ermittelt automatisch (nicht manuell wie in Abschnitt 3) **alle**
Pfade von `probeTask` bis zu jedem Blatt im `.ci`-Callgraph, die innerhalb des
Diagnose-Tasks erreichbar sind, und gibt am Ende aus:

```text
CURRENT_MAX_PROBE_TASK_PATH=<exact witness>
CURRENT_MAX_PROBE_TASK_CUMULATIVE_BYTES=<bytes>
```

Der manuelle Befund aus Abschnitt 3 (P3 = 71920 Bytes) ist die Untergrenze für
diesen künftigen automatischen Witness, kein Ersatz dafür.

### 4.2 ESP-IDF-Heap-Instrumentierung: project-local, kein Vendor-Patch

```text
ESP_IDF_VENDOR_SOURCE_MODIFICATION=NO
ESP_IDF_INSTALLATION_PATCH=NO
```

`heap_caps.c`, `multi_heap.c`, `tlsf.c` (Component `heap`) sind heute ohne
`-fstack-usage`/`-fcallgraph-info=su` gebaut. Umsetzung ausschließlich über
das bestehende ESP-IDF/CMake-Component-Target, ohne eine Datei unter
`$IDF_PATH` zu ändern:

1. Bevorzugt: `idf_component_get_property(heap_lib heap COMPONENT_LIB)` im
   Projekt-Build (analog zum bestehenden Muster in `main/CMakeLists.txt`, das
   bereits punktuell `target_compile_options`/`set_source_files_properties`
   auf einzelne Quellen anwendet) und darüber gezielt nur
   `heap_caps.c`/`multi_heap.c`/`tlsf.c` instrumentieren.
2. Falls ESP-IDF 6.0.2 keinen sauberen datei-genauen Zugriff auf die Quellen
   eines fremden Components erlaubt: den gesamten `heap`-Component-Target
   vollständig instrumentieren (component-weit), statt fragile
   Per-File-Hacks zu bauen.
3. Kein Kopieren einer privaten Heap-Implementierung ins Repository, kein
   Patch unter `$IDF_PATH`, keine geänderte Installationsanleitung.

Falls auch das technisch nicht sauber erreichbar ist:

```text
INSTRUMENTATION_PATH=BLOCKED
STOP_OWNER_REVIEW
```

Ohne diese Instrumentierung bleibt P2 (Abschnitt 3) strukturell unvollständig
und `4.1`s globaler Witness kann nicht als vollständig gelten.

### 4.3 Staleness-Gate: beide Konstanten und ihre Ableitung, nicht nur eine Ungleichung

Nicht nur `measured cumulative <= kMeasuredCallPathBytes` prüfen. Zusätzlich:

```text
kMeasuredCallPathBytes == CURRENT_MAX_PROBE_TASK_CUMULATIVE_BYTES
```

oder eine explizit im Code begründete konservative Obergrenze darüber, und:

```text
EXPECTED_TASK_STACK_BYTES =
    align_up(kMeasuredCallPathBytes + kMeasuredCallPathSafetyBufferBytes, 1024)

kProbeTaskStackBytes == EXPECTED_TASK_STACK_BYTES
```

Beide Prüfungen laufen im selben, bereits in 4.4 verdrahteten Build-/CI-Schritt
gegen `main/issue_29_bringup_probe.cpp`'s tatsächlich kompilierte Konstanten
(kein zweiter, unabhängig gepflegter Zahlensatz in einem separaten Skript).
Damit können Callpath-Konstante, Safety-Buffer, Rundung und konfigurierte
Taskgröße nicht mehr unabhängig voneinander auseinanderlaufen — genau das ist
in dieser Requalifikation passiert.

### 4.4 Verhindern, dass historische Stackwerte weiter still kompiliert werden

`scripts/analyze_issue_29_stack.py` ist heute in keinem Build- oder
CI-Schritt verdrahtet. Umsetzung:

1. Aufruf des erweiterten Skripts (4.1–4.3) als Pflichtschritt in
   `scripts/build_esp_idf_profiles.py` nach dem `esp32_bringup`-Build.
2. Abbruch des Build-/CI-Laufs bei `STACK_GATE=BLOCKED` oder einer
   Ungleichheit aus 4.3.
3. Damit erzwingt jede künftige Änderung an einem der gehaltenen Typen
   (`CommandDecision`, `RunCommandState`, `ProgramStartRequest`,
   `RunPersistenceCoordinator`, `TemperatureControlApplicationOrchestrator`,
   `TemperatureController`, `ActuatorPlanner`,
   `TargetQualificationEvaluator`, `ActuationInterlock`), an `ActiveRun`
   (P3) oder an der Heap-Walk-Kette (P2) eine bewusste, im jeweiligen PR
   sichtbare Aktualisierung der Konstanten statt eines stillen
   Auseinanderlaufens.

### 4.5 Rebuild-/Stackanalyse-Gate vor jeder künftigen #29-Hardwarenutzung

Vor jedem künftigen Flash von `esp32_bringup` für Issue #29 sind verbindlich,
in dieser Reihenfolge, mit `PASS` nachzuweisen:

```bash
python scripts/build_esp_idf_profiles.py all
python scripts/analyze_issue_29_stack.py build/esp32_bringup
```

Ein `FAILED`/`BLOCKED` an dieser Stelle blockiert jeden weiteren
Hardwareschritt.

### 4.6 Erneute actor-free Hardware-Requalifikation nach dem Fix

Erst nach 4.1–4.5 mit `PASS`:

1. `esp32_bringup` (ohne #90-Harness) auf demselben unbelasteten Board
   flashen;
2. mindestens drei voneinander unabhängige reale Boots, je mindestens
   35-s-Smoke, aktorfrei, ohne 12-V-Verbraucher;
3. für **jeden** der drei erfolgreichen Boots mindestens dokumentieren:

```text
CONFIGURED_PROBE_TASK_STACK_BYTES=
STATIC_MAX_PROBE_TASK_PATH_BYTES=
STATIC_SAFETY_BUFFER_BYTES=

READY_TASK_STACK_HWM_BYTES=
MIN_OBSERVED_PROBE_TASK_HWM_BYTES=

PANIC=NO
STACK_OVERFLOW=NO
WATCHDOG=NO
BROWNOUT=NO
UNRELATED_UNEXPECTED_RESET=NO
ACTOR_RELEASE=false
```

Der reale Task-HWM (`uxTaskGetStackHighWaterMark`, bereits heute vom Probe
geloggt als `task_ready_hwm_bytes`/`task_completion_hwm_bytes`) ersetzt die
statische Herleitung aus 4.1 nicht, und umgekehrt — beide werden dokumentiert.
Falls die reale Reserve praktisch vollständig aufgebraucht wird:

```text
RUNTIME_STACK_MARGIN=OWNER_REVIEW_REQUIRED
```

Keine pauschale weitere Stackvergrößerung ohne diesen neuen, konkret
benannten Befund.

4. bei erneutem Panic: sofort STOP, Befund dokumentieren, keinen weiteren
   Fixversuch ohne neue Owner-Freigabe.

### 4.7 Pegelmessungen erst nach stabilem 3/3-Smoke

Die in `docs/tasks/issue-29-implementation-plan.md` offenen Pegelmessungen
(`POWER_ON`, `RESET_EN`, `BOOTLOADER_IO0`, `ACTOR_FREE_NORMAL_BOOT`) werden
erst nach `BOOT_REQUALIFICATION=3_OF_3_PASS` gemäß 4.6 abgearbeitet. Sie sind
kein Ersatz für die Panic-Behebung und werden nicht vorgezogen.

## 5. Ausdrückliche Nicht-Ziele dieses Korrekturplans

```text
heap_caps_get_largest_free_block entfernen oder ersetzen
den Heap-Walk pauschal als Fehler erklären
Watchdog-Konfiguration ändern
CONFIG_ESP_MAIN_TASK_STACK_SIZE oder eine andere Produkt-/Main-Taskgröße erhöhen
Produktlogik (fermentation_app, device_platform) ändern
NVS-Verhalten ändern
#124/#126-Vertrag ändern
eine neue generische IResourceMonitor-/Stackanalyse-Plattform einführen
eine pauschale Erhöhung von kProbeTaskStackBytes ohne die Messung aus 4.1
Dateien unter $IDF_PATH ändern, lokale Espressif-Quellen patchen, eine
  kopierte private ESP-IDF-Heapimplementierung ins Repo aufnehmen
```

Ein "API umgehen und schauen ob es weiterläuft" gilt ausdrücklich nicht als
Root-Cause-Nachweis.

## 6. Akzeptanzkriterien

### 6.1 Plan Acceptance (diese Revision)

- Root Cause nicht überbehauptet (`ROOT_CAUSE=UNRESOLVED`,
  `HEAP_WALK_ROOT_CAUSE=NOT_CLAIMED` bleiben bestehen);
- vollständige Stackanalysegrenze festgelegt (Abschnitt 4.1, P0–P6, kein
  Zwei-Pfad-Vergleich mehr);
- Instrumentierungspfad für den unvermessenen Heap-Walk-Anteil festgelegt
  (Abschnitt 4.2, project-local, kein Vendor-Patch);
- Staleness-Gate vollständig (Abschnitt 4.3: beide Konstanten und ihre
  Ableitung, nicht nur eine Ungleichung);
- Hardware-Requalifikation eindeutig (Abschnitt 4.6, inklusive Runtime-HWM);
- Pegelgate eindeutig (Abschnitt 4.7);
- keine materielle technische Entscheidung offen.

### 6.2 Implementation / Issue Acceptance (erst nach Ownerfreigabe)

- digitale Stack-/Build-Gates (4.1–4.5) `PASS`;
- korrigierter Diagnose-Task (`kMeasuredCallPathBytes`/`kProbeTaskStackBytes`
  aus dem realen, vollständigen `CURRENT_MAX_PROBE_TASK_CUMULATIVE_BYTES`);
- 3/3 realer Smoke `PASS` (4.6);
- Runtime-HWM für alle drei Boots dokumentiert;
- danach Pegelgate (4.7);
- Owner Final Review.

## 7. Owner-Gate

Keine Implementierung von Abschnitt 4 vor ausdrücklicher Freigabe der exakten
Commit-SHA dieses Plans. Kein Merge, kein `Ready for review`, kein
Issue-Schluss durch den Agenten.
