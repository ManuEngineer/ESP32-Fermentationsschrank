# Projekt-Roadmap

Stand: 2026-08-27

Diese Datei ist die einzige aktuelle Status- und Taskuebersicht. Fachliche
Anforderungen, vollstaendige Issue-Inhalte und historische Begruendungen werden
nicht kopiert, sondern verlinkt.

## Aktuelle Arbeit

| Prioritaet | Arbeit | Status | Naechstes Gate |
|---:|---|---|---|
| 1 | Issue #29 – ESP32-Bring-up, Partition, Ressourcen und sichere Ausgangszustaende | `SOFTWARE_IMPLEMENTED_HARDWARE_TESTED_PASS_PENDING_LEVELS`; Plan `docs/tasks/issue-29-implementation-plan.md @ 4f49b44cff47f55bfd425d9e39c5a07256782ed7` freigegeben, Software-/Buildnachweise und zwei reale post-fix 40-s-Smokes liegen in Draft-PR #116 vor. Die Baseline bleibt aktorfrei. Das einzige verbleibende #29-Abnahmegate sind die sicheren unbelasteten MCU-/Gate-/Bootpegel. Die PCB-Revision/Silkscreen ist nach Ownerentscheidung kein Abnahmekriterium. #29 bleibt Prioritaet 1 und bis zur Pegelmessung offen; diese Priorisierung ist keine pauschale Blockade der #90-Software-/Hostarbeit. #90 bleibt actor-free; physische Ausgangspegel sind kein #90-Produktgate. | Owner-Nachweis der sicheren unbelasteten MCU-/Gate-/Bootpegel; danach Review-/Abnahmentscheidung ohne Issue-Schliessung durch den Agenten. Die #90-Software-/Host-/Oraclearbeit folgt ihren eigenen Owner-Gates. |
| 2 | Issue #90 – produktiver ESP-IDF-NVS-Adapter fuer `IStateStore` | `SLICE_7_DIGITAL_INFRASTRUCTURE_COMPLETE_PHYSICAL_OWNER_ACTION_PENDING`; kanonischer R5.9-Plan bleibt `docs/tasks/issue-90-clean-restart-plan-r5.9.md @ baf0b2ae04cd42afa75dfa00e21d900116b38bc8`. Die neue R1-Entwicklungsbasis `integration/r1-development @ 086db53c90819d90e2521123c0c735f471cc6394` bleibt unveraendert; Draft-Folge-PR #123 (`agent/issue-90-slice7-product-runner` gegen diese Basis) enthaelt den privaten, bring-up-only `APP_ISSUE_90_SLICE7_HARNESS`. Er nutzt fuer repräsentative Configuration-/Run-Writes die aktuellen Produktservices und den heutigen `NvsStateStore`, stellt bounded actor-free Lastfenster, UART-Protokoll, sichere `state_store_test`-Backup-/Restore-Pruefung und Release-Symbolisolation bereit. Native 1.019/1.019 Tests, ESP-IDF-Profile, esp-clang, NVS-Hostadapter, Partitions-/Kapazitaets-, Architektur-, Secret-, Format- und Diff-Gates sind PASS. `CALLBACK_12_REAL_TRIGGER=NOT_REPRODUCIBLE_DIGITALLY`; keine physische Power-Cut-Kampagne wurde ausgefuehrt. #29 bleibt als separates `BLOCKED_OWNER_MEASUREMENT_PENDING` offen. | Owner-Aktion: dedizierten Bring-up-Harness mit verifiziertem Backup verwenden, je ausgewaehltem Config-/Run-Szenario drei echte physische Stromunterbrechungen durchfuehren, Zustand wiederherstellen/verifizieren und anschliessend den Draft-PR reviewen. |
| 3 | Issue #119 – produktive StateStore-/Application-Composition anbinden und real verifizieren | `STEP_2_REVERIFICATION=FAILED_NEW_BOOT_PANIC`; Schritt 1/1.1 gemaess R1.0/R1.1-Plan umgesetzt und ownerreviewt (`PASS`), Boot-Composition-Helper implementiert. Eine reale Schritt-2-Hardware-Reverifikation (PR #120) hat den urspruenglichen Boot-WDT real behoben (`PREVIOUS_PRODUCT_BOOT_WDT_ROOT_CAUSE=NOT_REPRODUCED`), ist aber deterministisch in einen neuen `LoadProhibited`-Panic unmittelbar vor `composeAndBeginApplication(...)` gelaufen. `CURRENT_LOAD_PROHIBITED_ROOT_CAUSE=UNRESOLVED`. Ein unabhaengig beauftragter Architektur-Audit der beteiligten Lifecycle-/Safety-/Persistenz-Vertraege kam parallel zum Ergebnis, dass die aktuelle Verzahnung fuer Release 1 vereinfacht werden sollte (`ARCHITECTURE_AUDIT=COMPLETED`, `ARCHITECTURE_AUDIT_OWNER_REVIEW=PASS_WITH_CORRECTIONS`, `ARCHITECTURE_VERDICT=SIMPLIFY`). PR #120 bleibt OPEN/Draft/eingefroren als fehlgeschlagener Composition-/Diagnosepfad; keine weitere Hardwareverifikation oder Korrektur auf diesem PR ohne neue Ownerfreigabe. | STOP – der naechste Schritt ist der #121-Simplification-Plan, nicht eine #119/#120-Fortsetzung. |
| 4 | Issue #121 – Release-1 Device-/Application-Lifecycle- und Safety-Policy-Vereinfachung | `STEP_8_HARDWARE_REQUALIFICATION=PASS_PENDING_OWNER_FINAL_REVIEW`; kanonischer Plan `docs/tasks/issue-121-lifecycle-safety-simplification-plan.md @ 3fb4d17418d449818b4f941f99e261525e25a54d`; `OWNER_PLAN_REVIEW=PASS`, `SOURCE_OF_TRUTH_CONFLICT=NONE`, Schritte 1–7 sind PASS und ownerreviewt. `STEP_8_PRE_CORRECTION_SOURCE_SHA=2cedf885e0916df784764ff050fa54b96e1aec1c`, `STEP_8_PRE_CORRECTION_CHANGES=RETAINED_AND_ACCEPTED`; der Post-Korrekturteil und die Hardware-Requalifikation sind mit `IMPLEMENTATION_STEP_8=PASS_PENDING_OWNER_FINAL_REVIEW`, `STEP_8_POST_PLAN_CORRECTION_IMPLEMENTATION=PRE_HARDWARE_PASS`, `STEP_8_PRE_HARDWARE=PASS` und `STEP_8_HARDWARE_REQUALIFICATION=PASS` abgeschlossen. Einzige Konfigurationsänderung: `MAIN_TASK_STACK_CONFIG_SOURCE=sdkconfig.defaults`, `CONFIG_ESP_MAIN_TASK_STACK_SIZE=16384`, `BRINGUP_RELEASE_DUPLICATE_STACK_SETTING=NO`, `STACK_MICRO_OPTIMIZATION_TO_3584_REQUIRED=NO`, Quelle/Commit `STEP_8_STACK_CONFIG_SOURCE_SHA=ffada481d0693abb82ae2106551b19e40bfc3171`. Frische effektive Kconfig: Bring-up/Release jeweils `16384 B`; frischer statischer Produkt-Bootpfad: Bring-up/Release jeweils `10576 B`, `STATIC_HEADROOM=5808 B`, `STATIC_HEADROOM >= 4096 B`, `PRODUCT_BOOT_WITNESS_ALL_FRAMES_QUALIFIER=STATIC`, `PRODUCT_BOOT_WITNESS_COMPILED_EDGES=PRESENT`, `NO_PRODUCT_REACHABLE_SINGLE_FRAME_EXCEEDS_CONFIGURED_TASK_STACK=PASS`, `PRODUCT_REACHABLE_STATIC_STACK_GATE=PASS`. Ressourcen: `SIZEOF_FERMENTATION_APPLICATION=44 B`, `SIZEOF_RUN_PERSISTENCE_COORDINATOR=20752 B`, `SIZEOF_RUN_PERSISTENCE_WORKING_SET=12416 B`, `APP_MAIN_ENTRY_FRAME=144 B`, `STATIC_DRAM=13450 B`/`13442 B` (Bring-up/Release), `STATIC_DRAM_DELTA=0 B` gegen frische #122-Vorkonfigurationsbaseline, `FLASH_APP_SIZE=272528 B`/`251136 B` App-BIN (Bring-up/Release). `FULL_NATIVE_SUITE=PASS`, beide ESP-IDF-Builds und beide ESP-Clang-Gates sowie NVS-Host-, Architektur-, Secret-, Format- und Diff-Gates sind PASS; Draft-CI ist real `skipping`. Actor-free Release-Hardwarerequalifikation mit unveränderter Firmware: Diagnose-Lauf `55.078 s`, Produktionszyklen `45.060 s`, `45.062 s`, `45.062 s`; `DIAG_FREE_HEAP_BEFORE_APPLICATION_COMPOSITION=266836 B`, `DIAG_MINIMUM_FREE_HEAP_BEFORE_APPLICATION_COMPOSITION=266476 B`, `DIAG_LARGEST_BLOCK_BEFORE_APPLICATION_COMPOSITION=131072 B`, `DIAG_MAIN_TASK_HWM_BEFORE_APPLICATION_COMPOSITION=15224 B`, nach Application-Composition `237740 B`/`233896 B`/`110592 B`/`8904 B`, nach 30 s identisch. Über drei Produktionszyklen: `REAL_MAIN_TASK_STACK_HWM_REMAINING_MIN=8904 B`, `MIN_PRODUCTION_FREE_HEAP_AFTER_BEGIN=237740 B`, `MIN_PRODUCTION_FREE_HEAP_AFTER_30S=237740 B`; `MAIN_TASK_STACK_HIGH_WATERMARK=PASS`, `COORDINATOR_HEAP_ALLOCATION_LARGEST_BLOCK_GATE=PASS`, `NO_PANIC_WATCHDOG_BROWNOUT_UNEXPECTED_RESET=PASS`, `RESOURCE_DEGRADATION=NONE_OBSERVED`, `REAL_ACTUATORS_ENABLED=NO`, `ACTOR_FREE=YES`. `FACTORY_INIT_POST_CONFIG=NOT_RUN_SAFE_PRESERVATION_UNAVAILABLE`; kein physischer Power-Cycle wurde behauptet. `OWNER_HARDWARE_RUN_AUTHORIZATION=YES`, `OWNER_FINAL_REVIEW=REQUIRED`, `PHYSICAL_BOOT_OUTPUT_SAFETY=PENDING_ISSUE29`, `HARDWARE_RUN=PASS`, `MERGE=NO`; WLAN, LVGL/Display und Web bleiben `NOT_RUN`. Produktions-/Testcode, SDKConfig-Overlays, Resume-/UTC-/NTP-/Checkpoint-/Recovery-Verträge und Hardwarezustand bleiben unverändert; WIP-Ref bleibt `NOT_FOUND`, WIP-Cherry-Pick `NO`. | STOP – Owner Final Review der Step-8-Hardware-Requalifikation; Issue #29-Pegelgate und WLAN/LVGL/Web-Ressourcenfreigaben bleiben offen, kein Ready-for-review und kein Merge durch den Agenten. |
| 5 | Issue #25 – gemeinsame rendererunabhaengige Device-UI-/App-Vertraege | `PLANNED_SPEC_PENDING`; gemeinsame Shell-, App-, View-Model- und Command-Vertraege fuer Touch und Web. Keine Renderer- oder Pluginplattform. | Eigener Plan und native Vertragsnachweise auf der Ressourcenbasis aus #29/#90 |
| 6 | Issue #26 – lokale Touch-Shell und Fermentations-Workspace | `PLANNED_SPEC_PENDING`; baut auf #25 auf und bleibt von realer Displayhardware getrennt, bis #31 folgt. | Eigener Plan, simulierte Bedienpfade und produktionsnahe Shell-/App-Vertraege |
| 7 | Issue #31 – realer Renderer, Display, Touch und Kalibrierung | `BLOCKED_HARDWARE`; folgt #25/#26/#29 und bringt die echte Bedienung am Gerät über dieselben Contracts. | Hardware-/Pin-/Controllerbeweis, Ressourcen-/Lizenznachweis, reale Bedienungs- und Kalibrierungstests |
| 8 | Issue #30 – reale DS18B20-Sensoradapter | `BLOCKED_HARDWARE`; #20/#21 sind abgeschlossen, #29 sowie die produktionsnahen Bedien-/Servicepfade bleiben Grundlage. | Eigener Plan, reale Bus-, ROM-, CRC-, Hot-Plug- und Fehlerprüfungen über die bestehende Produktsoftware |
| 9 | Issue #32 – Lüfter, Summer und Onboard-MOSFET-Ausgaenge | `BLOCKED_HARDWARE`; eigener abschliessbarer Hardware-/Adapterscope nach #23/#24/#29. Begrenzte nichtproduktive Serviceprüfungen sind zulässig; #28/#35/#106 sind keine #32-Abschlussvoraussetzungen. | Reale Zuordnung, Pegel, Boot-/Reset-, Verbraucher-, Strom-/Anlauf- und Adapter-/Testnachweise ohne produktive `ActuatorSafetyGateStatus::Allowed`-Freigabe |
| 10 | Issue #33 – BTS7960, R_IS/L_IS und begrenzte Peltierpruefungen | `BLOCKED_HARDWARE`; folgt auf dem abgeschlossenen #32-Hardwarefundament nach #30. | Begrenzte sichere Peltier-/BTS7960-Serviceprüfung über die echte Produktsoftware |
| 11 | Issue #106 strukturell – Per-Run-Producer-/Schema-/Snapshotmechanismus | `PLANNED_SPEC_PENDING`; darf nach #33 strukturell ohne erfundene Produktivwerte vorbereitet werden. | Eigener Plan; #35 bleibt Werte-/Grenzengate, keine TBD-Aktivierung |
| 12 | Issue #34 – Sensorvergleich und thermische Grundvermessung | `TBD_COMMISSIONING`; nach #29/#30/#31/#32/#33 und damit bewusst später als der bedienbare Gerätepfad. | Reale Messreihen, Offsets und auswertbare Messprotokolle; vollständige Lauf-/Diagnose-/Serviceexporte bleiben #28 |
| 13 | Issue #35 – PI-, Luft-, Aktor- und Sicherheitsparameter | `TBD_COMMISSIONING`; reale Werte und Grenzen nach #34. | Commissioning-Nachweise und verbindliche produktive Werte-/Safetyfreigabe |
| 14 | Issue #106 produktiv – Per-Run-Bindung und Aktoraktivierung | `PLANNED_SPEC_PENDING`; produktiver Abschluss erst mit den durch #35 gelieferten Werten und Grenzen. | Produktive Snapshot-/Recoverybindung und Aktivierung ohne TBD-Werte |
| 15 | Issue #19 / #28 / #36 / #37 – zurückgestellte Journale-, Diagnose-, Abnahme- und Releasegates | #19 bleibt `REVIEW_DRAFT – PRESERVE, NOT APPROVED, NOT CANONICAL, IMPLEMENTATION NOT_STARTED`; #28 bleibt späteres Diagnose-/Service-/Exportgate mit seiner #19-Abhängigkeit. | Neue vollständige #19-Planrevision auf aktuellem `main`; danach spätere vollständige Diagnose-/Abnahme-/Releasegates |

## Naechste fachliche Arbeit

PR #110 / Issue #24 und PR #113 / Issue #111 sind auf dem aktuellen `main`
abgeschlossen. Der Release-1-Safety-Core bleibt beim bestätigten KISS- und
fail-closed-Vertrag; Hardware-, reale Aktor-, NVS- und
Inbetriebnahmenachweise sind dadurch nicht vorweggenommen.

Issue #119 / PR #120 bleibt ein eingefrorener, fehlgeschlagener
Composition-/Diagnosepfad (neuer `LoadProhibited`-Panic, Ursache weiterhin
ungeklärt). Ein unabhängiger Architektur-Audit empfiehlt eine gezielte
Release-1-Vereinfachung der Lifecycle-/Safety-/Persistenz-Verträge; der Owner
hat die Hauptrichtung mit verbindlichen Korrekturen angenommen. Der neue,
eigenständige Plan-Scope dazu steht in Issue #121 vor jeder Implementation.

Die endgültige Priorisierungsrichtung ist:

```text
#29 -> #90 -> #121 -> #25 -> #26 -> #31 -> #30 -> #32 -> #33
  -> erste real bedienbare Fermenter-Hardwareintegration
  -> #106 strukturell -> #34 -> #35 -> #106 produktiv
  -> spätere vollständige Diagnose-/Abnahme-/Releasegates
```

#29 und #90 liefern zuerst reale Plattform-, Ressourcen- und Persistenzbasis.
#25/#26 bilden darauf die wiederverwendbare Device Shell und den
Fermentations-Workspace; #31 bringt dieselben rendererunabhängigen Contracts
auf reales Display und Touch. #30, #32 und #33 werden danach über die bis dahin
vorhandenen produktionsnahen Bedien-, Service- und Diagnosepfade integriert.

Es entsteht keine separate Wegwerf-Testsoftware und kein zweiter temporärer
Bedien- oder Diagnosevertrag. Schmale Low-Level-Nachweise bleiben zulässig,
wenn sie für Treiber, Pegel, Boot-/Reset-Sicherheit oder Fehlersuche notwendig
sind; sie ersetzen aber nicht die spätere Produktsoftware.

Issue #32 besitzt einen eigenen abschliessbaren Hardware-/Adapterscope nach
#23/#24/#29. #28, #35 und #106 sind keine Abschlussvoraussetzungen von #32:
#28 bleibt das spätere Diagnose-/Service-/Exportgate, #35 das Werte-/
Safety-Commissioninggate und #106 das produktive Per-Run-/Aktivierungsgate.
Das Schliessen von #32 behauptet keine produktive
`ActuatorSafetyGateStatus::Allowed`-Freigabe.

#34 und #35 bleiben wichtige Commissioning- und Releasegates, sind aber nicht
Voraussetzung für die frühere bedienbare Firmware oder die schrittweise reale
Hardwareintegration. #106 darf strukturell ohne erfundene Produktivwerte
vorbereitet werden; produktive Werte und Aktivierung bleiben an #35 gebunden.

Issue #19 bleibt zeitlich zurückgestellt, offen und fachlich erhalten. Sein
externer Entwurf bleibt nicht kanonisch; vor einer späteren Umsetzung ist auf
aktuellem `main` eine neue vollständige Planrevision erforderlich. #28 wird
nicht als vorgezogener Parallelvertrag umgesetzt und behält seine Abhängigkeit
auf #19.

Issue #16 bleibt Trackingcontainer: #54–#57 und das
`CONFIGURATION_SAFETY_INTEGRATION_GATE` sind abgeschlossen, reale
NVS-/Partitions-/Flash-/Hardwareabnahmen bleiben insbesondere über #90 offen.

## Zulaessige Parallelitaet

- PR #113 / Issue #111 sind als Markdown-only-Governancearbeit abgeschlossen;
  Firmware- und Safety-Semantik bleiben davon unberührt.
- #29 und #90 bilden die erste reale Plattformbasis; danach folgen #25, #26
  und #31 für die echte Device Shell, App und Bedienung.
- #30, #32 und #33 werden über die produktionsnahen UI-/Service-/Diagnosepfade
  integriert. Low-Level-Hardwaretests bleiben schmal und erzeugen keine
  separate Wegwerf-Testanwendung.
- #106 strukturell, #34, #35 und #106 produktiv folgen erst nach der ersten
  real bedienbaren Fermenter-Hardwareintegration.
- #19 erhält vor einer späteren Umsetzung eine neue vollständige Planrevision
  auf dann aktuellem `main`. Der bestehende Review-Draft ist weder
  freigegeben noch kanonisch.
- #28 bleibt ein späteres Diagnose-/Service-/Exportgate und verwendet, wo
  Bedienung erforderlich ist, die #25/#26/#31-Contracts statt eines zweiten
  temporären Vertrags. Web/WLAN blockieren den lokalen Gerätebetrieb nicht.
- Hardware-, Bibliotheks- und Adapterarbeit beginnt nur ueber das zugehoerige
  Live-Issue und einen freigegebenen Plan.
- Unabhaengige Recherche darf keine Umsetzung, Produktauswahl oder
  Hardwarefreigabe vorwegnehmen.
- Die spätere Device-UI bleibt eine wiederverwendbare Shell/App-Architektur
  mit gemeinsamen rendererunabhängigen Contracts für Touch und Web. #25/#26
  werden nicht zu einer nur-fermenterspezifischen oder universellen
  Plugin-/Widget-/Runtime-Discovery-Plattform umgedeutet.

## Blocker und spaetere Gates

- Reale Hardware-, GPIO-, Display-/Touch-, Sensor-, Aktor- und
  Inbetriebnahmenachweise stehen in `OPEN_POINTS.md`.
- Thermische Parameter und Releaseabnahme bleiben bis zu den realen Messungen
  und Belastungstests blockiert.
- Issue #89 (WLAN-Onboarding-Evaluation) und Issue #90
  (ESP-IDF-NVS-Adapter) benoetigen vor Beginn einen eigenen Live-Abgleich und
  freigegebenen Plan.
- Issue #114 bewahrt den frueheren komplexen Advanced-Safety-/Recovery-Entwurf
  als `FUTURE_SCOPE_REFERENCE_NON_NORMATIVE`. Er ist kein Release-1-Gate und
  wird vor einer spaeteren Umsetzung vollstaendig gegen den dann aktuellen
  Stand neu geplant und ownerfreigegeben.
- Issue #19 bleibt offen. Der vorhandene Planentwurf wird als
  `REVIEW_DRAFT – PRESERVE, NOT APPROVED, NOT CANONICAL,
  IMPLEMENTATION NOT_STARTED` behandelt. Es gibt keine freigegebene
  Plan-SHA und keine #19-Implementation.
- OTA ist fuer ein spaeteres Release vorgesehen, aber kein Release-1-Scope.

## Zuletzt abgeschlossene groessere Grundlagen

- PR #98: Agentenregeln und Statusquellen konsolidiert und nach `main` gemergt;
- PR #79: ESP-IDF 6.0.2 als einziger ESP32-Produktionspfad;
- PR #84: Laufpersistenz und Kontrollpunkte;
- PR #88: Espressif-first-Synchronisierung des Adopt-or-build-Audits;
- PR #92: Markdown-only-Aenderungen ohne vollstaendige Firmware-CI;
- PR #95 / Issue #20: Sensorqualitaet, Filterung und Plausibilitaet;
- PR #96: kompakter Session-Handover-Vertrag;
- PR #99 / Issue #21: Regelsensorauswahl, Ersatzbetrieb und Rueckkehrlogik;
- PR #102 / Issue #18: Wiederanlauf und temperaturgewichteter Fortschritt als erhaltener C2-Legacy-/Future-Ausbaupfad;
- PR #104 / Issue #22: Zeitproportionale PI-Regelung und Luftbegrenzung;
- PR #105 / Issue #23: Aktorplaner, Mindestzeiten, Totzeit und Luefterlogik;
- PR #110 / Issue #24: Release-1 FaultCodes, Disposition, `SAFE_BOOT`, Fehlerinjektion und zentrale fail-closed SafetyCore-Grenze.
- PR #113 / Issue #111: Codex Plan Mode als Planungsmodus ohne Änderung des
  Owner-Gates.

## Pflege

Aktualisierung ist erforderlich:

- zu Beginn jedes neuen Pull Requests;
- nach jedem Merge;
- bei materieller Reihenfolgeaenderung;
- bei neuem Blocker oder Ownerentscheid.

Details bleiben in Live-Issue, Pull Request, freigegebenem Plan, ADR oder
Fachvertrag.
