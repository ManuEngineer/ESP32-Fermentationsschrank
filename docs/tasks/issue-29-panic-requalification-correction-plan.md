# Issue #29 – Panic-Requalifikation: Ursachenrahmen und Korrekturplan

Dieser Plan ist eine eigenständige, vollständige Korrekturplanrevision für den
am 2026-08-30 auf der aktuellen R1-Integrationsbaseline real reproduzierten
`esp32_bringup`-Panic. Er ersetzt **nicht** den ursprünglichen
`docs/tasks/issue-29-implementation-plan.md @ 4f49b44cff47f55bfd425d9e39c5a07256782ed7`,
der als Herkunfts- und Architekturvertrag für Ziel, Scope, Isolationsdesign
(Abschnitt 6/7 dort) und die grundsätzliche Hardware-Abnahmemethodik (35-s-Smoke,
Pegelmessung) unverändert gültig bleibt. Dieser Plan ist ausschließlich für den
neuen Requalifikationsbefund zuständig und macht den heute gültigen
Gesamtvertrag für diesen Befund vollständig selbst verständlich, ohne Abschnitte
der alten Revision nur per Verweis fortzuschreiben.

## 1. Status, Basis und Owner-Gate

- Repository: `ManuEngineer/ESP32-Fermentationsschrank`
- Aktuelle Integrationsbasis: `integration/r1-development @ c1f5fbb5f19ab8e7d2c25708fe79777d523217d4`
  (PR #128 gemergt; Issue #90 `CLOSED/COMPLETED`)
- ESP-IDF: `v6.0.2`, Commit `7101770dc6db2667b3c477cc31365dd1acd6db4e`
- Branch dieses Plans: `agent/issue-29-requalification-r1`
- Kanonischer Plan: `docs/tasks/issue-29-panic-requalification-correction-plan.md`
- Ursprünglicher Architekturvertrag: `docs/tasks/issue-29-implementation-plan.md @ 4f49b44cff47f55bfd425d9e39c5a07256782ed7`
- Planfreigabe: Owner muss die exakte Commit-SHA dieses Plans freigeben, bevor
  irgendein in Abschnitt 5 beschriebener Schritt implementiert wird.

### Unbedingtes Stop-Gate

Bis zur ausdrücklichen Ownerfreigabe der exakten Plan-SHA sind ausschließlich
erlaubt:

1. dieser Plan-Commit und die zugehörige Roadmap-/Issue-Synchronisation;
2. Draft-PR-Erstellung und -Pflege;
3. weiterer read-only Nachweis (kein Flashen, keine Codeänderung).

Nicht erlaubt: jede Änderung an `main/issue_29_bringup_probe.cpp`,
`main/issue_29_bringup_probe.hpp`, `main/issue_29_bringup_fault_seam.hpp`,
`main/CMakeLists.txt`, ESP-IDF-Buildflags, oder ein neuer Hardwarelauf.

## 2. Requalifikationsbefund (Zusammenfassung, kein neuer Nachweis in diesem Plan)

Vollständiger Nachweis in `docs/ROADMAP.md` (Zeile Issue #29) und im lokalen,
nicht versionierten Messordner `build/issue29_requalification/`. Kurzfassung:

```text
SOURCE_SHA=c1f5fbb5f19ab8e7d2c25708fe79777d523217d4
PROFILE=esp32_bringup (ohne #90-Harness)
BOOT_1=PANIC_REPRODUCED
BOOT_2=NOT_RUN_STOP_GATE
BOOT_3=NOT_RUN_STOP_GATE

PANIC=Guru Meditation Error (LoadProhibited)
PANIC_INDUCED_RESET=YES (rst:0xc, SW_CPU_RESET, ausgelöst vom Panic-Handler selbst)
UNRELATED_UNEXPECTED_RESET=NO
WATCHDOG=NO
BROWNOUT=NO
ACTOR_RELEASE=false (gültig bis zum Panic)
```

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

## 3. Read-only Stackherleitung auf dem aktuellen Build (kein neuer Hardwarelauf)

Ausgeführt gegen den bereits vorhandenen, nachweislich exakt zu Boot 1
gehörenden Build (ELF enthält `APP_SOURCE_GIT_SHA=c1f5fbb5f19ab8e7d2c25708fe79777d523217d4`;
`.su`/`.ci`-Zeitstempel decken sich mit dem letzten unveränderten Commit
`d34a2852` der betroffenen Quellen, Arbeitsbaum sauber):

```bash
python3 scripts/analyze_issue_29_stack.py build/esp32_bringup
```

Ergebnis:

```text
probeTask: frame=48 bytes qualifier=static cumulative=48
runProbe: frame=65712 bytes qualifier=static cumulative=65760
persistFreshStartCommand: frame=32 bytes qualifier=static cumulative=65792
orchestrator.persistCommand: frame=304 bytes qualifier=static cumulative=66096
coordinator.persistCommand: frame=688 bytes qualifier=static cumulative=66784
coordinator.result: frame=32 bytes qualifier=static cumulative=66816

CURRENT_CUMULATIVE_CALL_PATH_BYTES=66816
CURRENT_SAFETY_BUFFER_BYTES=4096
CURRENT_REQUIRED_CONFIGURED_TASK_STACK_BYTES=71680

COMPILED_kMeasuredCallPathBytes=62928   (main/issue_29_bringup_probe.cpp:64, unverändert seit 3bc5bfe)
COMPILED_kProbeTaskStackBytes=67584     (main/issue_29_bringup_probe.cpp:70-71, unverändert seit 3bc5bfe)

RUNPROBE_FRAME_QUALIFIER=static
ALL_REQUIRED_CHAIN_QUALIFIERS=static
CALLGRAPH_EDGES_COMPLETE=YES (Skript hätte sonst mit SystemExit abgebrochen)
```

Alle sechs Frames sind weiterhin `static`, alle geforderten Callgraph-Kanten
sind vorhanden; das Skript ist ohne Fehler durchgelaufen.

### Entscheid A (aktiv)

```text
CURRENT_CUMULATIVE_CALL_PATH_BYTES (66816) > COMPILED_kMeasuredCallPathBytes (62928)  -> WAHR
CURRENT_REQUIRED_CONFIGURED_TASK_STACK_BYTES (71680) > COMPILED_kProbeTaskStackBytes (67584) -> WAHR

PRIMARY_DIAGNOSIS=STALE_ISSUE29_DIAGNOSTIC_STACK_BUDGET
HEAP_WALK_ROOT_CAUSE=NOT_CLAIMED
ROOT_CAUSE=UNRESOLVED
```

Beide Entscheid-A-Bedingungen sind erfüllt. **STOP mit Diagnose.** Dieser Plan
implementiert keinen Fix; er entscheidet nur die fünf in Abschnitt 5
geforderten Punkte für eine spätere, separat freizugebende Umsetzung.

## 4. Was die Messung zusätzlich zeigt (Einordnung, keine Root-Cause-Behauptung)

Zwei Beobachtungen ordnen den Befund präziser ein, ohne eine Ursache
festzulegen:

1. **`runProbe` allein wuchs um 12480 Bytes** (historisch dokumentiert 53232
   Bytes in `docs/ISSUE_29_BUILD_REPORT.md`, Firmwarestand `3bc5bfe`; aktuell
   65712 Bytes). `runProbe` hält `kHeldObjectBytes`
   (`main/issue_29_bringup_probe.cpp:52-58`: `CommandDecision`,
   `RunCommandState`, `ProgramStartRequest`, `RunPersistenceCoordinator`,
   `TemperatureControlApplicationOrchestrator`, `TemperatureController`,
   `ActuatorPlanner`, `TargetQualificationEvaluator`, `ActuationInterlock`)
   als eigene lokale Variablen für die gesamte Funktionsdauer – bereits bevor
   überhaupt einer der beiden Teilpfade (Ressourcenprobe oder
   Fault-Seam-Pfad) läuft. `docs/tasks/issue-121-lifecycle-safety-simplification-plan.md`,
   Abschnitt 4.7, wendet dieselbe Instrumentierungstechnik ausdrücklich nur
   auf den **Produktpfad** an, nicht auf die #29-Diagnose-Probe; ob eine
   #121-Typänderung ursächlich für das Wachstum ist, ist damit plausibel,
   aber **nicht bewiesen** und wird hier nicht behauptet.
2. **Die von `scripts/analyze_issue_29_stack.py` geprüfte Kette deckt den
   real abgestürzten Pfad nicht ab.** Das Skript validiert ausschließlich
   `probeTask -> runProbe -> persistFreshStartCommand -> orchestrator.persistCommand
   -> coordinator.persistCommand -> coordinator.result` (den
   "Candidate-Allocation-Failure"-Pfad). Der real abgestürzte Pfad
   `runProbe -> sampleResources -> heap_caps_get_largest_free_block ->
   heap_caps_get_info -> multi_heap_get_info_impl -> tlsf_walk_pool` ist
   davon **nicht erfasst**: `-fstack-usage`/`-fcallgraph-info=su` sind nur für
   `app_main.cpp` und `issue_29_bringup_probe.cpp` aktiviert
   (`main/CMakeLists.txt`), nicht für die ESP-IDF-Heap-Komponente. Für
   `sampleResources` liegt ein `.su`-Wert vor (32 Bytes, `static`); für
   `heap_caps_get_largest_free_block`, `heap_caps_get_info`,
   `multi_heap_get_info_impl` und `tlsf_walk_pool` liegt **kein** `.su`-Wert
   vor. Bereits `probeTask` (48) + `runProbe` (65712) = 65760 Bytes sind vor
   dem ersten Aufruf von `sampleResources` belegt, bei einer konfigurierten
   Taskgröße von 67584 Bytes – ein Restspielraum von 1824 Bytes für
   `sampleResources` und die gesamte, bisher ungemessene Heap-Walk-Kette.
   Das ist ein **Messlückenbefund**, keine Ursachenbehauptung.

## 5. Geforderte Entscheidungen für die spätere Umsetzung (Planungsarbeit)

Die folgenden fünf Punkte sind **Entscheidungen dieses Plans**, deren
**Implementierung erst nach separater Ownerfreigabe dieser exakten Plan-SHA**
beginnt (`IMPLEMENTATION=NOT_STARTED`).

### 5.1 Aktuelle begründete Diagnose-Task-Stackgröße

Die Umsetzung muss vor jeder neuen Konstante die Messlücke aus Abschnitt 4
Punkt 2 schließen:

1. `-fstack-usage`/`-fcallgraph-info=su` read-only-analytisch **nur für die
   betroffenen ESP-IDF-Heap-Quellen** (`heap_caps.c`, `multi_heap.c`,
   `tlsf.c`) im `esp32_bringup`-Bring-up-Profil ergänzen – dieselbe bereits
   bestehende Technik, keine neue Analyseplattform, keine Änderung an
   Produktlogik.
2. `scripts/analyze_issue_29_stack.py` um eine zweite Kette erweitern:
   `probeTask -> runProbe -> sampleResources -> heap_caps_get_largest_free_block
   -> heap_caps_get_info -> multi_heap_get_info_impl -> tlsf_walk_pool`
   (nicht-inlinierte Kanten), analog zur bestehenden Prüfmethodik (Qualifier
   `static` erzwingen, fehlende Kante bricht ab).
3. `kMeasuredCallPathBytes` in `main/issue_29_bringup_probe.cpp` auf das
   Maximum beider real gemessener Ketten setzen, mit demselben begründeten
   4096-Byte-Sicherheitspuffer, auf 1024 Bytes aufgerundet.
4. Kein geratener Zielwert in diesem Plan: Der exakte neue Zahlenwert wird
   erst nach Schritt 1–2 real gemessen und dann in der Umsetzung eingetragen.

### 5.2 Verhindern, dass historische Stackwerte weiter still kompiliert werden

`scripts/analyze_issue_29_stack.py` ist heute in keinem Build- oder
CI-Schritt verdrahtet (`grep` über `.github/workflows/`, `scripts/build_esp_idf_profiles.py`,
`scripts/run_esp_idf_static_analysis.py`, `scripts/selftest_quality_gates.py`
ergibt keinen Treffer). Genau das erlaubte der stillen Divergenz zwischen
`kMeasuredCallPathBytes` und dem real kompilierten Wert. Umsetzung:

1. Einen neuen Aufruf von `scripts/analyze_issue_29_stack.py` (erweitert um
   5.1) als Pflichtschritt in `scripts/build_esp_idf_profiles.py` nach dem
   `esp32_bringup`-Build ergänzen.
2. Das Skript bricht den Build/CI-Lauf ab, wenn die real gemessene kumulierte
   Summe größer ist als die im Quelltext hartcodierte
   `kMeasuredCallPathBytes`-Konstante (statt nur zu drucken).
3. Damit erzwingt jede künftige Änderung an einem der gehaltenen Typen
   (`CommandDecision`, `RunCommandState`, `ProgramStartRequest`,
   `RunPersistenceCoordinator`, `TemperatureControlApplicationOrchestrator`,
   `TemperatureController`, `ActuatorPlanner`,
   `TargetQualificationEvaluator`, `ActuationInterlock`) oder an der
   Heap-Walk-Kette eine bewusste, im jeweiligen PR sichtbare Aktualisierung
   der Konstante statt eines stillen Auseinanderlaufens.

### 5.3 Rebuild-/Stackanalyse-Gate vor jeder künftigen #29-Hardwarenutzung

Vor jedem künftigen Flash von `esp32_bringup` für Issue #29 sind verbindlich,
in dieser Reihenfolge, mit `PASS` nachzuweisen:

```bash
python scripts/build_esp_idf_profiles.py all
python scripts/analyze_issue_29_stack.py build/esp32_bringup
```

Ein `FAILED` an dieser Stelle blockiert jeden weiteren Hardwareschritt.

### 5.4 Erneute actor-free Hardware-Requalifikation nach dem Fix

Erst nach 5.1–5.3 mit `PASS`:

1. `esp32_bringup` (ohne #90-Harness) auf demselben unbelasteten Board
   flashen;
2. mindestens drei voneinander unabhängige reale Boots, je mindestens
   35-s-Smoke, aktorfrei, ohne 12-V-Verbraucher;
3. bei erneutem Panic: sofort STOP, Befund dokumentieren, keinen weiteren
   Fixversuch ohne neue Owner-Freigabe.

### 5.5 Pegelmessungen erst nach stabilem 3/3-Smoke

Die in `docs/tasks/issue-29-implementation-plan.md` offenen Pegelmessungen
(`POWER_ON`, `RESET_EN`, `BOOTLOADER_IO0`, `ACTOR_FREE_NORMAL_BOOT`) werden
erst nach `BOOT_REQUALIFICATION=3_OF_3_PASS` gemäß 5.4 abgearbeitet. Sie sind
kein Ersatz für die Panic-Behebung und werden nicht vorgezogen.

## 6. Ausdrückliche Nicht-Ziele dieses Korrekturplans

```text
heap_caps_get_largest_free_block entfernen oder ersetzen
den Heap-Walk pauschal als Fehler erklären
Watchdog-Konfiguration ändern
CONFIG_ESP_MAIN_TASK_STACK_SIZE oder eine andere Produkt-/Main-Taskgröße erhöhen
Produktlogik (fermentation_app, device_platform) ändern
NVS-Verhalten ändern
#124/#126-Vertrag ändern
eine neue generische IResourceMonitor-/Stackanalyse-Plattform einführen
eine pauschale Erhöhung von kProbeTaskStackBytes ohne die Messung aus 5.1
```

Ein "API umgehen und schauen ob es weiterläuft" gilt ausdrücklich nicht als
Root-Cause-Nachweis.

## 7. Akzeptanzkriterien dieses Korrekturplans

Dieser Korrekturplan selbst ist abgeschlossen, wenn:

- die Owner-Freigabe der exakten Plan-SHA vorliegt;
- daraufhin 5.1–5.3 mit realem `PASS` umgesetzt und nachgewiesen sind;
- 5.4 mit `BOOT_REQUALIFICATION=3_OF_3_PASS` real nachgewiesen ist;
- 5.5 entweder mit vollständigem `PASS` oder mit konkret benannten
  `TBD_HARDWARE`-Restpunkten abgeschlossen ist;
- Issue #29 und `docs/ROADMAP.md` den dann erreichten Stand tragen.

## 8. Owner-Gate

Keine Implementierung von Abschnitt 5 vor ausdrücklicher Freigabe der exakten
Commit-SHA dieses Plans. Kein Merge, kein `Ready for review`, kein
Issue-Schluss durch den Agenten.
