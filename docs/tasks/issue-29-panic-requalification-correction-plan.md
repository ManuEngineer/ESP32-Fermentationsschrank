# Issue #29 – Panic-Requalifikation: KISS-Korrekturplan

Diese Revision ersetzt den bisherigen vollständigen statischen
Callgraph-Ansatz als aktuelle kanonische Handlungsgrundlage für PR #129. Der
Versuch, aus Projektcode, ESP-IDF, FreeRTOS, libc, libstdc++ und
vorkompilierten Bibliotheken einen globalen Upper Bound zu schließen, wird
bewusst verworfen. Die dokumentierte Analyse bleibt als historische
Diagnoseevidenz erhalten, ist aber kein globaler Freigabebeweis.

~~~text
ISSUE29=OPEN
PR129=OPEN_DRAFT
CURRENT_DEVELOPMENT_BASE=integration/r1-development @ c1f5fbb5f19ab8e7d2c25708fe79777d523217d4
BRANCH=agent/issue-29-requalification-r1
IMPLEMENTATION=NOT_STARTED_KISS_REVISION
ROOT_CAUSE=UNRESOLVED
HARDWARE_RUN=NO
MERGE=NO

EXHAUSTIVE_PROBE_TASK_ALL_PATH_STATIC_GATE=SUPERSEDED_KISS
FULL_TRANSITIVE_STATIC_CALLGRAPH_CLOSURE=NOT_REQUIRED
GENERIC_BINARY_CALLGRAPH_ANALYZER=NO
TRANSITIVE_LIBSTDCXX_BINARY_STACK_PLATFORM=NO
MORE_SDK_COMPONENT_INSTRUMENTATION=NO
STATIC_ANALYSIS_ROLE=DIAGNOSTIC_EVIDENCE_NOT_GLOBAL_UPPER_BOUND
HARDWARE_HWM_ROLE=PRIMARY_EMPIRICAL_STACK_EVIDENCE
~~~

## 1. Provenienz und Owner-Gate

Repository und Basis:

- Repository: "ManuEngineer/ESP32-Fermentationsschrank"
- aktuelle Integrationsbasis: "integration/r1-development @ c1f5fbb5f19ab8e7d2c25708fe79777d523217d4"
- PR: #129, Draft, Branch "agent/issue-29-requalification-r1"
- ESP-IDF: "v6.0.2", Commit
  "7101770dc6db2667b3c477cc31365dd1acd6db4e"
- aktuelle kanonische Plan-Datei:
  "docs/tasks/issue-29-panic-requalification-correction-plan.md"

Historische Plan- und Implementierungs-SHAs bleiben ausschließlich
Provenienz:

- ursprünglicher Issue-29-Plan:
  "docs/tasks/issue-29-implementation-plan.md @ 4f49b44cff47f55bfd425d9e39c5a07256782ed7";
- zuvor freigegebene, nun durch diesen KISS-Pivot ersetzte Planrevision:
  "@ 4a34967ac202196b7afceaebfe2b2429338d6d93";
- Vor-Implementierungsstand der drei zurückzunehmenden Dateien:
  "3fbaf32";
- exhaustive Implementierungsversuche:
  "a19cd1605fb1e74e6dd31f8b457741e9cf92b2d9" und
  "92f6cf6c3ee16868b95b7ee6a4e9b233d9ffb0c6".

Die exakte Commit-SHA dieser neuen vollständigen Planrevision benötigt vor
jeder späteren Umsetzung eine neue ausdrückliche Ownerfreigabe. Die alte
Freigabe von "4a34967" autorisiert keinen Stackfix und keine weitere
exhaustive Analyse.

Bis zur neuen Freigabe gilt:

~~~text
OWNER_PLAN_REVIEW=KISS_REVISION_PENDING
IMPLEMENTATION=NOT_STARTED_KISS_REVISION
HARDWARE_RUN=NO
LEVEL_MEASUREMENTS=NOT_RUN
ISSUE25_STARTED=NO
MERGE=NO
~~~

## 2. Requalifikationsbefund und bekannte statische Evidenz

Der reale Boot-1-Befund auf der aktuellen Integrationsbaseline bleibt
unverändert:

~~~text
SOURCE_SHA=c1f5fbb5f19ab8e7d2c25708fe79777d523217d4
PROFILE=esp32_bringup
ISSUE90_HARNESS=ABSENT

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
~~~

Der Backtrace zeigt den Ort der Beobachtung:

~~~text
probeTask -> runProbe -> sampleResources
  -> heap_caps_get_largest_free_block -> heap_caps_get_info
  -> multi_heap_get_info_impl -> tlsf_walk_pool/block_is_last
~~~

Er beweist nicht, dass der Heap-Walk die ursprüngliche Ursache ist. Ein
Heap-Walk kann eine zuvor entstandene Speicher- oder Stackkorruption zuerst
sichtbar machen.

Die aus der bisherigen Implementierungsrunde erhaltene statische
Build-Evidenz wird kanonisch so eingeordnet:

~~~text
OLD_PROBE_TASK_STACK_BYTES=67584
COMPILED_runProbe_FRAME_BYTES=65712

CURRENT_MAX_KNOWN_STATIC_PATH_BYTES=72224
CURRENT_MAX_KNOWN_STATIC_PATH_IS_COMPLETE_UPPER_BOUND=NO
CURRENT_MAX_KNOWN_STATIC_PATH_IS_UPPER_BOUND=NO

OLD_PROBE_TASK_STACK_IS_BELOW_KNOWN_STATIC_PATH=YES
ROOT_CAUSE=UNRESOLVED
~~~

Damit ist bereits bewiesen:

~~~text
67584 < 72224
~~~

Die 72224 B sind der größte heute bekannte, aus den vorhandenen Buildartefakten
hergeleitete Pfad. Sie sind kein vollständiger Upper Bound und werden nicht
als solcher in CI, im Plan oder in einer Hardwarefreigabe verwendet.

## 3. KISS-Entscheidung und Rücknahme

Der vollständige transitive statische Gate-Ansatz ist
"EXHAUSTIVE_PROBE_TASK_ALL_PATH_STATIC_GATE=SUPERSEDED_KISS".

Der Analyzer aus der Implementierungsrunde darf nicht als korrektes globales
Gate verwendet werden. Insbesondere ist der in F1 festgehaltene
Fail-Closed-Fehler nicht durch eine weitere Analysearchitektur zu reparieren:
ein gespeicherter Stack-Qualifier muss bei der Traversierung wirksam werden,
doch der exhaustive Ansatz ist insgesamt superseded. Es gibt keine neue
Reparaturrunde für diese Architektur.

~~~text
ANALYZER_QUALIFIER_FAIL_CLOSED_BUG=SUPERSEDED_NO_REPAIR
~~~

Die virtuelle Whitelist bleibt ebenfalls historische Diagnoseevidenz, kein
belastbares Zukunfts-CI-Gate: eine "dict[str, set[str]]"-Kante reduziert
mehrere indirekte Aufrufstellen desselben Callers auf denselben
"__indirect_call"-Namen; ein Caller-Substring kann danach eine unvollständige
Override-Menge einsetzen. Diese Konstruktion wird nicht weiter ausgebaut.

Die drei während der Implementierungsrunde geänderten Dateien werden exakt
auf den Vor-Implementierungsstand "3fbaf32" zurückgeführt:

~~~text
scripts/analyze_issue_29_stack.py=REVERTED_TO_3FBAF32
main/CMakeLists.txt=REVERTED_TO_3FBAF32
lib/device_platform/CMakeLists.txt=REVERTED_TO_3FBAF32
~~~

Dabei werden nur die neuen #29-Blöcke zurückgenommen. Die bereits vor PR #129
vorhandene #121-Instrumentierung bleibt bestehen. Es werden keine weiteren
ESP-IDF-Components instrumentiert.

Nach der Rücknahme gilt für die vorhandenen Buildprofile weiterhin die
bestehende Compile-Time-Isolation:

~~~text
BRINGUP_HAS_ISSUE29_PROBE=YES
RELEASE_HAS_ISSUE29_PROBE=NO
NATIVE_HAS_ISSUE29_PROBE=NO
~~~

Der bestehende Issue-90-Harness-Isolationscheck wird in dieser Korrekturrunde
nicht durch eine Annahme ersetzt. Vor späterer Hardware ist nach der
Rücknahme ein frischer Nachweis erforderlich:

~~~text
ISSUE90_HARNESS_HAS_ISSUE29_PROBE=NO
~~~

Der Check wird einmal real gegen den späteren exakten Harness-Build
ausgeführt; "NOT_RE_RUN" ist kein "PASS".

## 4. Minimaler KISS-Stackfix als kontrolliertes Experiment

Nach Freigabe dieser neuen Plan-SHA wird genau ein minimaler
Diagnose-Runtimefix umgesetzt:

~~~text
DIAGNOSTIC_PROBE_TASK_STACK_BYTES=98304
DIAGNOSTIC_PROBE_TASK_STACK_KIB=96
~~~

Die 96 KiB sind:

- keine Produkt-Taskgröße;
- kein globaler statischer Upper Bound;
- keine neue allgemeine Ressourcen- oder Stackplattform;
- eine bewusst großzügige Diagnosegröße für den kontrollierten
  #29-Kausaltest;
- innerhalb des historisch beobachteten internen RAM-Rahmens plausibel.

Die bekannte Rechnung wird als Ausgangspunkt dokumentiert:

~~~text
KNOWN_STATIC_PATH_BYTES=72224
EXTRA_HEADROOM_ABOVE_KNOWN_PATH=26080

HISTORICAL_PRE_TASK_LARGEST_FREE_BLOCK_BYTES=172032
RESIDUAL_VS_HISTORICAL_LARGEST_BLOCK=73728
~~~

Vor dem Hardwarelauf wird "before_task_create" auf dem neuen exakten Build
erneut gemessen. Wenn "xTaskCreate" nicht sauber gelingt oder die
Resource-Probe bereits vor dem eigentlichen Test fail-closed blockiert:

~~~text
STOP
~~~

Es erfolgt keine weitere Stackvergrößerung auf Verdacht.

### 4.1 Begrenzung der Runtime-Differenz

Für den ersten Hardware-Kausaltest darf die einzige
fachlich/runtime-relevante Änderung am #29-Probe die private Diagnosegröße
"kProbeTaskStackBytes -> 98304" sein.

Die drei historischen Ableitungsvariablen werden beibehalten, aber nicht mehr
als vollständige Herleitung oder CI-Gate dargestellt:

~~~text
kMeasuredCallPathBytes=historische/diagnostische Untergrenze
kMeasuredCallPathSafetyBufferBytes=historischer/diagnostischer Pufferwert
kUnroundedProbeTaskStackBytes=historische/diagnostische Rechengröße
~~~

Sie dürfen zur Provenienz und zur Vergleichbarkeit weiter geloggt werden,
haben aber keine Autorität über die neue 96-KiB-Diagnosegröße. Kommentare und
Dokumentation müssen diese niedrigere diagnostische Bedeutung klar benennen.
Die Implementierung ändert keine Probe-Fachlogik, keine Heap-API, keine
Fault-Seam, keine Produkt-/Main-Taskgröße und keine Persistenz.

Damit bleibt der geplante Hardwarevergleich kausal eng:

~~~text
OLD_FAILING_RUNTIME
vs.
NEW_RUNTIME_WITH_LARGER_PRIVATE_DIAGNOSTIC_STACK
~~~

Nicht geändert werden:

~~~text
PRODUCT_TASK_STACK_CHANGED=NO
FAULT_SEAM_CHANGED=NO
PRODUCT_LOGIC_CHANGED=NO
PERSISTENCE_CHANGED=NO
~~~

## 5. Digitale Gates vor Hardware

Nach Ownerfreigabe der neuen Plan-SHA und vollständiger Umsetzung des
Minimalfixes müssen vor jedem Flash alle folgenden Ergebnisse mit dem neuen
exakten Source- und Build-Head vorliegen:

~~~text
SOURCE_TREE_CLEAN=YES

ESP_IDF_BRINGUP_BUILD=PASS
ESP_IDF_RELEASE_BUILD=PASS
FULL_NATIVE_BUILD=PASS
FULL_NATIVE_TESTS=PASS
ESP_CLANG_BRINGUP=PASS
ESP_CLANG_RELEASE=PASS
ARCHITECTURE_GATES=PASS
SECRET_SCAN=PASS
QUALITY_GATES=PASS
GIT_DIFF_CHECK=PASS

BRINGUP_HAS_ISSUE29_PROBE=YES
RELEASE_HAS_ISSUE29_PROBE=NO
NATIVE_HAS_ISSUE29_PROBE=NO
ISSUE90_HARNESS_HAS_ISSUE29_PROBE=NO

PRODUCT_TASK_STACK_CHANGED=NO
FAULT_SEAM_CHANGED=NO
PRODUCT_LOGIC_CHANGED=NO
PERSISTENCE_CHANGED=NO
~~~

Build-, Test- und Qualitätsbefehle stammen ausschließlich aus
"docs/CI_AND_QUALITY_GATES.md". Der vollständige lokale Lauf erfolgt nur
nach abgeschlossenem Review, auf dem finalen HEAD und nach ausdrücklicher
Owner-Anweisung.

Der Analyzer wird nicht in "scripts/build_esp_idf_profiles.py" oder einen
anderen CI-Schritt als globales #29-Stackgate verdrahtet. Eine spätere
diagnostische Ausführung darf nur als Evidenzbericht erscheinen und darf
"72224" nicht in einen globalen Upper Bound umdeuten.

Vor Hardware muss die Provenienz vollständig sein:

~~~text
IMPLEMENTATION_SOURCE_SHA=<exact>
ESP32_BRINGUP_BUILD_SOURCE_SHA=<same exact>
APP_EMBEDDED_SOURCE_SHA=<same exact>
ELF_SHA256=<exact>
~~~

Danach STOP für Owner Review vor Flash.

## 6. Hardware-Kausaltest nach späterer Freigabe

Hardware, Flash und Pegelmessungen sind in dieser Plan-/Korrekturrunde nicht
auszuführen.

### Boot 1

Ein realer, aktorfreier "esp32_bringup"-Boot mit 96-KiB-Diagnose-Task wird
mindestens 35 s beobachtet. Wenn Panic, Watchdog, Brownout oder ein
unerwarteter Reset auftritt:

~~~text
BOOT_1=FAIL
ROOT_CAUSE=UNRESOLVED
STACK_BUDGET_CORRECTION=INSUFFICIENT
STOP
~~~

Boot 2 und Boot 3 werden dann nicht ausgeführt.

Vor dem eigentlichen Test müssen insbesondere folgende Werte aus demselben
Build/Boot vorliegen:

~~~text
before_task_create.free_heap_bytes=
before_task_create.largest_free_block_bytes=
CONFIGURED_PROBE_TASK_STACK_BYTES=98304
~~~

### Boot 2 und Boot 3

Nur wenn Boot 1 vollständig bestanden ist, werden zwei weitere unabhängige
reale Boots durchgeführt. Erforderlich ist:

~~~text
BOOT_1=PASS
BOOT_2=PASS
BOOT_3=PASS
BOOT_REQUALIFICATION=3_OF_3_PASS

PANIC=NO
STACK_OVERFLOW=NO
WATCHDOG=NO
BROWNOUT=NO
UNRELATED_UNEXPECTED_RESET=NO
ACTOR_RELEASE=false
~~~

Pro Boot:

~~~text
READY_TASK_STACK_HWM_BYTES=
COMPLETION_TASK_STACK_HWM_BYTES=
~~~

Gesamt:

~~~text
MIN_OBSERVED_PROBE_TASK_HWM_BYTES=
PEAK_OBSERVED_PROBE_TASK_STACK_USED_BYTES=
    98304 - MIN_OBSERVED_PROBE_TASK_HWM_BYTES
~~~

"MIN_OBSERVED_PROBE_TASK_HWM_BYTES" ist das Minimum über die beobachteten
Ready- und Completion-HWM-Werte aller drei Boots. HWM ist empirische
Stackevidenz und ersetzt keine globale statische Analyse; der hier
supersedete globale Gate-Ansatz ist aber keine Voraussetzung mehr.

## 7. Empirische Root-Cause-Disposition

Die historische alte Taskkapazität bildet die Vergleichsschwelle:

~~~text
OLD_PROBE_TASK_STACK_BYTES=67584
98304 - 67584 = 30720
~~~

### Fall A: 3/3 PASS und Überschreitung der alten Kapazität

Wenn:

~~~text
BOOT_REQUALIFICATION=3_OF_3_PASS
MIN_OBSERVED_PROBE_TASK_HWM_BYTES < 30720
~~~

dann gilt:

~~~text
PEAK_OBSERVED_PROBE_TASK_STACK_USED_BYTES > 67584
~~~

Zusammen mit:

~~~text
OLD_BUILD=REPRODUCIBLE_PANIC
NEW_BUILD=3_OF_3_PASS
ONLY_RUNTIME_RELEVANT_CHANGE=PRIVATE_DIAGNOSTIC_STACK_SIZE
~~~

darf für das Owner Final Review vorgeschlagen werden:

~~~text
ROOT_CAUSE=CONFIRMED_STALE_DIAGNOSTIC_TASK_STACK_BUDGET
~~~

Das ist ein Vorschlag für die Owner-Disposition, keine automatische
Issue-Schließung.

### Fall B: 3/3 PASS innerhalb der alten Kapazität

Wenn:

~~~text
BOOT_REQUALIFICATION=3_OF_3_PASS
MIN_OBSERVED_PROBE_TASK_HWM_BYTES >= 30720
~~~

dann gilt:

~~~text
ROOT_CAUSE=NOT_FULLY_PROVEN
STACK_INCREASE_MITIGATION=PASS
OWNER_DECISION_REQUIRED=YES
~~~

Der Stack wird nicht automatisch als bewiesene Root Cause abgeschlossen.

### Fall C: Panic bleibt bestehen

~~~text
ROOT_CAUSE=UNRESOLVED
STACK_BUDGET_CORRECTION=INSUFFICIENT
STOP
~~~

Keine weitere Stackvergrößerung und kein neuer Fixversuch ohne separat
freigegebenen Owner-Auftrag.

## 8. Resource-Probe-Einordnung

Die 96-KiB-Diagnose-Task reserviert mehr internen RAM. Heap- und
Largest-Block-Werte des Probes sind damit konservativer als bei der alten
Diagnosegröße.

Wenn die vollständige Resource-/Fault-Probe unter 96 KiB Taskstack besteht:

~~~text
RESOURCE_GATE=PASS_CONSERVATIVE
~~~

Wenn sie wegen fehlendem Heap oder fehlgeschlagener Allocation blockiert:

~~~text
PRODUCT_RESOURCE_FAILURE=NOT_AUTOMATICALLY_CLAIMED
DIAGNOSTIC_OVERHEAD_CONFOUNDING=YES
STOP_OWNER_REVIEW
~~~

Dann wird der Stack nicht einfach weiter erhöht und kein Produktproblem
vorschnell behauptet.

## 9. Pegelmessungen

Erst nach:

~~~text
BOOT_REQUALIFICATION=3_OF_3_PASS
~~~

und nach Owner-Review der Root-Cause-Disposition dürfen die ausdrücklich
offenen unbelasteten Pegel gemessen werden:

~~~text
POWER_ON
RESET_EN
BOOTLOADER_IO0
ACTOR_FREE_NORMAL_BOOT
~~~

Keine Pegelmessung in dieser Planrunde. Kein Hardwarelauf ist ein Ersatz für
die fehlende Requalifikation.

## 10. Scope, Verbote und Akzeptanz

In dieser Korrekturrunde sind ausschließlich erlaubt:

~~~text
scripts/analyze_issue_29_stack.py
main/CMakeLists.txt
lib/device_platform/CMakeLists.txt
docs/tasks/issue-29-panic-requalification-correction-plan.md
docs/ISSUE_29_BUILD_REPORT.md
docs/ISSUE_29_MEASUREMENTS.md
docs/ROADMAP.md
PR #129 Body
Issue #29 Status/Kommentar
~~~

Die drei Code-/Builddateien werden nur auf "3fbaf32" zurückgeführt. Die
historische Evidenz aus "a19cd160" und "92f6cf6" bleibt in den Dokumenten,
aber mit der KISS-Einordnung. Nicht erlaubt sind:

~~~text
neuer Stackfix implementieren
Flash
Hardwareboot
Pegelmessung
#25-Arbeit
Merge
Ready-for-review-Wechsel
Auto-Merge
Force-Push
Branch-/Issue-Löschung oder Issue-Schluss
~~~

Für die jetzige Planrevision gelten als abgeschlossen nachzuweisen:

~~~text
EXHAUSTIVE_STATIC_GATE_ATTEMPT=SUPERSEDED_KISS
FULL_TRANSITIVE_STATIC_CALLGRAPH_CLOSURE=NOT_REQUIRED
STATIC_ANALYSIS_ROLE=DIAGNOSTIC_EVIDENCE_NOT_GLOBAL_UPPER_BOUND
HARDWARE_HWM_ROLE=PRIMARY_EMPIRICAL_STACK_EVIDENCE
CURRENT_MAX_KNOWN_STATIC_PATH_BYTES=72224
CURRENT_MAX_KNOWN_STATIC_PATH_IS_COMPLETE_UPPER_BOUND=NO
CURRENT_MAX_KNOWN_STATIC_PATH_IS_UPPER_BOUND=NO
OLD_PROBE_TASK_STACK_IS_BELOW_KNOWN_STATIC_PATH=YES
ROOT_CAUSE=UNRESOLVED
IMPLEMENTATION=NOT_STARTED_KISS_REVISION
HARDWARE_RUN=NO
LEVEL_MEASUREMENTS=NOT_RUN
ISSUE25_STARTED=NO
MERGE=NO
~~~

Der nächste zulässige Schritt nach Commit und Ownerfreigabe der exakten neuen
Plan-SHA ist ausschließlich die Umsetzung des minimalen privaten
96-KiB-Diagnose-Stacktests. Danach gelten die digitalen Gates aus Abschnitt 5,
der frische Harness-Isolationsnachweis und der STOP vor Flash.
