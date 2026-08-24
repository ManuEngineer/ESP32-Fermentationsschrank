# Projekt-Roadmap

Stand: 2026-08-24

Diese Datei ist die einzige aktuelle Status- und Taskuebersicht. Fachliche
Anforderungen, vollstaendige Issue-Inhalte und historische Begruendungen werden
nicht kopiert, sondern verlinkt.

## Aktuelle Arbeit

| Prioritaet | Arbeit | Status | Naechstes Gate |
|---:|---|---|---|
| 1 | Issue #29 – ESP32-Bring-up, Partition, Ressourcen und sichere Ausgangszustaende | `SOFTWARE_IMPLEMENTED_HARDWARE_TESTED_PASS_PENDING_LEVELS`; Plan `docs/tasks/issue-29-implementation-plan.md @ 4f49b44cff47f55bfd425d9e39c5a07256782ed7` freigegeben, Software-/Buildnachweise und zwei reale post-fix 40-s-Smokes liegen in Draft-PR #116 vor. Die Baseline bleibt aktorfrei. Das einzige verbleibende #29-Abnahmegate sind die sicheren unbelasteten MCU-/Gate-/Bootpegel. Die PCB-Revision/Silkscreen ist nach Ownerentscheidung kein Abnahmekriterium. #29 bleibt Prioritaet 1 und bis zur Pegelmessung offen; diese Priorisierung ist keine pauschale Blockade der #90-Software-/Hostarbeit. #90 bleibt actor-free; physische Ausgangspegel sind kein #90-Produktgate. | Owner-Nachweis der sicheren unbelasteten MCU-/Gate-/Bootpegel; danach Review-/Abnahmentscheidung ohne Issue-Schliessung durch den Agenten. Die #90-Software-/Host-/Oraclearbeit folgt ihren eigenen Owner-Gates. |
| 2 | Issue #90 – produktiver ESP-IDF-NVS-Adapter fuer `IStateStore` | `R5_9_SLICES_1_TO_6_IMPLEMENTED_AND_VERIFIED`; `R5_9_SLICE_7_PARTIALLY_EXECUTED_FAILED_PRODUCT_BOOT_WDT_BEFORE_RECOVERY`. Kanonischer Plan bleibt R5.9 (`docs/tasks/issue-90-clean-restart-plan-r5.9.md @ baf0b2ae04cd42afa75dfa00e21d900116b38bc8`); keine weitere #90-Planrevision. Real erbracht: `SOFTWARE_ORACLE_GATE=PASS`, `NVS_ADAPTER_GATE=PASS`, `CAPACITY_GATE=PASS`, `DEFINED_BACKEND_MATRIX_COMPLETE=true`, `CI_ARTIFACT_SECRET_SCAN_GATE=PASS`, `STATIC_PROJECT_SCRATCH_EVIDENCE=PASS`, `CONFIGURED_RELEASE_MAIN_TASK_STACK=3584`, `BOARD_IDENTITY_GATE=PASS`, `UART_RESET_GATE=PASS`. #119 hat die Composition inzwischen hergestellt; der reale Release-Produktpfad erreicht aber in `NvsOwningContext::create()` bei `nvs_flash_init_partition("state_store")` den Interrupt-WDT, bevor Recovery, Safety-Projektion oder `application: ready` erfolgen. Daher `REAL_NVS_RECOVERY_GATE=FAIL`, `CALLBACK_12_REAL_NVS_PRODUCT_GATE=BLOCKED`, `MANUAL_POWER_CUT_GATE=BLOCKED`, `FULL_RUNTIME_STACK_HEADROOM=BLOCKED` (reale Recovery-Workload-HWM nie erreicht), `PRODUCT_RECOVERY_MISMATCH=NOT_EVALUATED_BOOT_WDT_BEFORE_RECOVERY`, `PRODUCT_BOOT_WDT=FAIL`. `device_platform -> fermentation_app`, `device_platform_esp_idf -> fermentation_app` und fermentationsspezifische `NvsStateStore`-Defaults bestaetigt `NONE`; Backend-Callback-12 bleibt `KNOWN_LIMITATION`. | Ownerentscheidung zum Produkt-Boot-WDT-Befund; keine Backend-/Architekturkorrektur durch #119. |
| 3 | Issue #119 – produktive StateStore-/Application-Composition anbinden und real verifizieren | `STEP_2_VERIFICATION_FAILED_BOOT_WDT_BEFORE_RECOVERY`; Schritt 1 gemaess R1.0-Plan umgesetzt und ownerreviewt (`PASS`). Ursachenanalyse (PR #120) real durchgefuehrt und Ownerentscheidung getroffen: `CAUSE_CLASS=#120_BINARY_OR_INTEGRATION_DEPENDENT`, `ROOT_CAUSE_DIRECTION=NOT_CONTENT_ONLY`, `BOOT_STACK_ROOT_CAUSE=OVERSIZED_APP_MAIN_ENTRY_FRAME` (`app_main()`-Stackframe waechst von 112 auf 16.432 Byte zwischen #118/#120 bei unveraendertem `CONFIG_ESP_MAIN_TASK_STACK_SIZE=3584`; realer Panic-SP zeigt 17.616 Byte Stackverbrauch, faellt bis in den `.dram0.data`-Bereich). `BOOT_STACK_FIX_DIRECTION=RESTRUCTURE_COMPOSITION_ROOT`, `MAIN_TASK_STACK_BLANKET_INCREASE=REJECTED_AS_PRIMARY_FIX`, `MATERIAL_ARCHITECTURE_DECISION_OPEN=NO`. Planrevision R1.1 (`docs/tasks/issue-119-platform-application-composition-plan.md @ dc4ee6fcbd69ec9490e4b4b63016ad5545db4b75`) legt die konsolidierte Zielarchitektur fest und ist ownerfreigegeben (`OWNER_R1_1_PLAN_APPROVAL=GRANTED`, `STEP_1_1_IMPLEMENTATION_APPROVAL=GRANTED`). Schritt 1.1 ist umgesetzt (`R1_1_IMPLEMENTATION=COMPLETED`, `HEAD=fc84cb0a5eecc78cf155dcda0da4617b1fb36be0`): der verbindliche, private `composeAndBeginApplication(...)`-Boot-Helper in `main/app_main.cpp` konstruiert alle boot-only Fachobjekte; `RunPersistenceCoordinator`/`RunPersistenceLoadResult` sind auf heapbesitzenden `std::unique_ptr` mit Boot-only-Lebenszeit (Helper-Scope) und explizitem fail-closed Allocation-Failure-Vertrag (`BOOT_COMPOSITION_ALLOCATION_FAILURE` bei `nullptr` aus `new (std::nothrow)`) umgestellt. Real am ESP32-Release-ELF gemessen: `APP_MAIN_ENTRY_FRAME_AFTER=112` (vorher 16.432) `< CONFIGURED_MAIN_TASK_STACK=3584`, Gate `PASS`; `BOOT_COMPOSITION_HELPER_ENTRY_FRAME=448` (kein grosser Puffer in den Helper verschoben). `R1_1_OWNER_REVIEW=PENDING`, `STEP_2_REVERIFICATION=NOT_STARTED` (Hardware-Retest in dieser Runde nicht autorisiert; reale Runtime-Stack-HWM weiterhin nicht gemessen). R1.0 bleibt architektonisch gueltig (Abschnitt 1-12 der Plandatei unveraendert). | STOP – Owner Full Review der R1.1-Schritt-1.1-Implementation; keine Schritt-2-Wiederholung ohne neue Ownerfreigabe. |
| 4 | Issue #25 – gemeinsame rendererunabhaengige Device-UI-/App-Vertraege | `PLANNED_SPEC_PENDING`; gemeinsame Shell-, App-, View-Model- und Command-Vertraege fuer Touch und Web. Keine Renderer- oder Pluginplattform. | Eigener Plan und native Vertragsnachweise auf der Ressourcenbasis aus #29/#90 |
| 5 | Issue #26 – lokale Touch-Shell und Fermentations-Workspace | `PLANNED_SPEC_PENDING`; baut auf #25 auf und bleibt von realer Displayhardware getrennt, bis #31 folgt. | Eigener Plan, simulierte Bedienpfade und produktionsnahe Shell-/App-Vertraege |
| 6 | Issue #31 – realer Renderer, Display, Touch und Kalibrierung | `BLOCKED_HARDWARE`; folgt #25/#26/#29 und bringt die echte Bedienung am Gerät über dieselben Contracts. | Hardware-/Pin-/Controllerbeweis, Ressourcen-/Lizenznachweis, reale Bedienungs- und Kalibrierungstests |
| 7 | Issue #30 – reale DS18B20-Sensoradapter | `BLOCKED_HARDWARE`; #20/#21 sind abgeschlossen, #29 sowie die produktionsnahen Bedien-/Servicepfade bleiben Grundlage. | Eigener Plan, reale Bus-, ROM-, CRC-, Hot-Plug- und Fehlerprüfungen über die bestehende Produktsoftware |
| 8 | Issue #32 – Lüfter, Summer und Onboard-MOSFET-Ausgaenge | `BLOCKED_HARDWARE`; eigener abschliessbarer Hardware-/Adapterscope nach #23/#24/#29. Begrenzte nichtproduktive Serviceprüfungen sind zulässig; #28/#35/#106 sind keine #32-Abschlussvoraussetzungen. | Reale Zuordnung, Pegel, Boot-/Reset-, Verbraucher-, Strom-/Anlauf- und Adapter-/Testnachweise ohne produktive `ActuatorSafetyGateStatus::Allowed`-Freigabe |
| 9 | Issue #33 – BTS7960, R_IS/L_IS und begrenzte Peltierpruefungen | `BLOCKED_HARDWARE`; folgt auf dem abgeschlossenen #32-Hardwarefundament nach #30. | Begrenzte sichere Peltier-/BTS7960-Serviceprüfung über die echte Produktsoftware |
| 10 | Issue #106 strukturell – Per-Run-Producer-/Schema-/Snapshotmechanismus | `PLANNED_SPEC_PENDING`; darf nach #33 strukturell ohne erfundene Produktivwerte vorbereitet werden. | Eigener Plan; #35 bleibt Werte-/Grenzengate, keine TBD-Aktivierung |
| 11 | Issue #34 – Sensorvergleich und thermische Grundvermessung | `TBD_COMMISSIONING`; nach #29/#30/#31/#32/#33 und damit bewusst später als der bedienbare Gerätepfad. | Reale Messreihen, Offsets und auswertbare Messprotokolle; vollständige Lauf-/Diagnose-/Serviceexporte bleiben #28 |
| 12 | Issue #35 – PI-, Luft-, Aktor- und Sicherheitsparameter | `TBD_COMMISSIONING`; reale Werte und Grenzen nach #34. | Commissioning-Nachweise und verbindliche produktive Werte-/Safetyfreigabe |
| 13 | Issue #106 produktiv – Per-Run-Bindung und Aktoraktivierung | `PLANNED_SPEC_PENDING`; produktiver Abschluss erst mit den durch #35 gelieferten Werten und Grenzen. | Produktive Snapshot-/Recoverybindung und Aktivierung ohne TBD-Werte |
| 14 | Issue #19 / #28 / #36 / #37 – zurückgestellte Journale-, Diagnose-, Abnahme- und Releasegates | #19 bleibt `REVIEW_DRAFT – PRESERVE, NOT APPROVED, NOT CANONICAL, IMPLEMENTATION NOT_STARTED`; #28 bleibt späteres Diagnose-/Service-/Exportgate mit seiner #19-Abhängigkeit. | Neue vollständige #19-Planrevision auf aktuellem `main`; danach spätere vollständige Diagnose-/Abnahme-/Releasegates |

## Naechste fachliche Arbeit

PR #110 / Issue #24 und PR #113 / Issue #111 sind auf dem aktuellen `main`
abgeschlossen. Der Release-1-Safety-Core bleibt beim bestätigten KISS- und
fail-closed-Vertrag; Hardware-, reale Aktor-, NVS- und
Inbetriebnahmenachweise sind dadurch nicht vorweggenommen.

Die endgültige Priorisierungsrichtung ist:

```text
#29 -> #90 -> #25 -> #26 -> #31 -> #30 -> #32 -> #33
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
