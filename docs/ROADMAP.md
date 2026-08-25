# Projekt-Roadmap

Stand: 2026-08-25

Diese Datei ist die einzige aktuelle Status- und Taskuebersicht. Fachliche
Anforderungen, vollstaendige Issue-Inhalte und historische Begruendungen werden
nicht kopiert, sondern verlinkt.

## Aktuelle Arbeit

| Prioritaet | Arbeit | Status | Naechstes Gate |
|---:|---|---|---|
| 1 | Issue #29 – ESP32-Bring-up, Partition, Ressourcen und sichere Ausgangszustaende | `SOFTWARE_IMPLEMENTED_HARDWARE_TESTED_PASS_PENDING_LEVELS`; Plan `docs/tasks/issue-29-implementation-plan.md @ 4f49b44cff47f55bfd425d9e39c5a07256782ed7` freigegeben, Software-/Buildnachweise und zwei reale post-fix 40-s-Smokes liegen in Draft-PR #116 vor. Die Baseline bleibt aktorfrei. Das einzige verbleibende #29-Abnahmegate sind die sicheren unbelasteten MCU-/Gate-/Bootpegel. Die PCB-Revision/Silkscreen ist nach Ownerentscheidung kein Abnahmekriterium. #29 bleibt Prioritaet 1 und bis zur Pegelmessung offen; diese Priorisierung ist keine pauschale Blockade der #90-Software-/Hostarbeit. #90 bleibt actor-free; physische Ausgangspegel sind kein #90-Produktgate. | Owner-Nachweis der sicheren unbelasteten MCU-/Gate-/Bootpegel; danach Review-/Abnahmentscheidung ohne Issue-Schliessung durch den Agenten. Die #90-Software-/Host-/Oraclearbeit folgt ihren eigenen Owner-Gates. |
| 2 | Issue #90 – produktiver ESP-IDF-NVS-Adapter fuer `IStateStore` | `R5_9_SLICES_1_TO_6_IMPLEMENTED_AND_VERIFIED`; `R5_9_SLICE_7_PARTIALLY_EXECUTED_FAILED_NEW_BOOT_PANIC_BEFORE_RECOVERY`. Kanonischer Plan bleibt R5.9 (`docs/tasks/issue-90-clean-restart-plan-r5.9.md @ baf0b2ae04cd42afa75dfa00e21d900116b38bc8`); keine weitere #90-Planrevision. Real erbracht: `SOFTWARE_ORACLE_GATE=PASS`, `NVS_ADAPTER_GATE=PASS`, `CAPACITY_GATE=PASS`, `DEFINED_BACKEND_MATRIX_COMPLETE=true`, `CI_ARTIFACT_SECRET_SCAN_GATE=PASS`, `STATIC_PROJECT_SCRATCH_EVIDENCE=PASS`, `CONFIGURED_RELEASE_MAIN_TASK_STACK=3584`, `BOARD_IDENTITY_GATE=PASS`, `UART_RESET_GATE=PASS` (reales Board, zwei unabhaengige Reboots). #119 hat die Composition hergestellt und den urspruenglichen Interrupt-WDT in `NvsOwningContext::create()` real behoben (`PREVIOUS_PRODUCT_BOOT_WDT_ROOT_CAUSE=NOT_REPRODUCED`); der Produktpfad laeuft seitdem unmittelbar vor der Application-Composition deterministisch in einen neuen, weiterhin ungeklaerten `LoadProhibited`-Panic (Details #119/PR #120). Deshalb weiterhin `REAL_NVS_RECOVERY_GATE=BLOCKED`, `CALLBACK_12_REAL_NVS_PRODUCT_GATE=BLOCKED`, `MANUAL_POWER_CUT_GATE=BLOCKED`, `FULL_RUNTIME_STACK_HEADROOM=BLOCKED`. `device_platform -> fermentation_app`, `device_platform_esp_idf -> fermentation_app` und fermentationsspezifische `NvsStateStore`-Defaults bestaetigt `NONE`. | Ownerreview von PR #118 auf dem eingefrorenen technischen Stand; die produktpfadabhaengigen Restgates warten auf den #121-Simplification-Plan, nicht auf eine #119/#120-Fortsetzung. |
| 3 | Issue #119 – produktive StateStore-/Application-Composition anbinden und real verifizieren | `STEP_2_REVERIFICATION=FAILED_NEW_BOOT_PANIC`; Schritt 1/1.1 gemaess R1.0/R1.1-Plan umgesetzt und ownerreviewt (`PASS`), Boot-Composition-Helper implementiert. Eine reale Schritt-2-Hardware-Reverifikation (PR #120) hat den urspruenglichen Boot-WDT real behoben (`PREVIOUS_PRODUCT_BOOT_WDT_ROOT_CAUSE=NOT_REPRODUCED`), ist aber deterministisch in einen neuen `LoadProhibited`-Panic unmittelbar vor `composeAndBeginApplication(...)` gelaufen. `CURRENT_LOAD_PROHIBITED_ROOT_CAUSE=UNRESOLVED`. Ein unabhaengig beauftragter Architektur-Audit der beteiligten Lifecycle-/Safety-/Persistenz-Vertraege kam parallel zum Ergebnis, dass die aktuelle Verzahnung fuer Release 1 vereinfacht werden sollte (`ARCHITECTURE_AUDIT=COMPLETED`, `ARCHITECTURE_AUDIT_OWNER_REVIEW=PASS_WITH_CORRECTIONS`, `ARCHITECTURE_VERDICT=SIMPLIFY`). PR #120 bleibt OPEN/Draft/eingefroren als fehlgeschlagener Composition-/Diagnosepfad; keine weitere Hardwareverifikation oder Korrektur auf diesem PR ohne neue Ownerfreigabe. | STOP – der naechste Schritt ist der #121-Simplification-Plan, nicht eine #119/#120-Fortsetzung. |
| 4 | Issue #121 – Release-1 Device-/Application-Lifecycle- und Safety-Policy-Vereinfachung | `IMPLEMENTATION_STEP_5=PASS`; Schritt-5-Korrektur gegen den früher freigegebenen R1-Plan `docs/tasks/issue-121-lifecycle-safety-simplification-plan.md @ e249b51cedf6f6a3edbce3a0889c48d77b79e828` abgeschlossen: sechs Resetzeilen entfernt – vier `std::array`-Ganzzuweisungen und zwei Count-Resets. `STEP_5_ORIGINAL_SOURCE_SHA=18f9c85f9221f81e006eb994324f62758ae414fd`, `STEP_5_CORRECTION_SOURCE_SHA=7049380d0e4bd9c0522c4475fa156febcd63ed5d`, `STEP_5_EFFECTIVE_SOURCE_SHA=7049380d0e4bd9c0522c4475fa156febcd63ed5d`, keine Code-/Policy-/Wire-/Schemaänderung. Vor Schritt 6 wurde ein materieller Planfehler korrigiert: `PRIOR_REVIEWED_CORRECTION_SHA=1da7fc41652a8070f8120be5a1d8eefbb2721444`, `PLAN_SHA=9b101295af6468878057758356de33848ec18061`, `PRIOR_APPROVED_PLAN_SHA=e249b51cedf6f6a3edbce3a0889c48d77b79e828`, `PLAN_CORRECTION_REASON=FERMENTATION_APP_MUST_NOT_DEPEND_ON_DEVICE_PLATFORM_ESP_IDF`, `OWNER_PLAN_REVIEW=PENDING`, `ARCHITECTURE_BOUNDARY_CHANGE=NO`, `IMPLEMENTATION=IN_PROGRESS_BLOCKED_BEFORE_STEP_6`, `IMPLEMENTATION_STEPS_1_TO_5=PASS`, `IMPLEMENTATION_STEP_6=NOT_STARTED`, `IMPLEMENTATION_STEP_6_GATE=BLOCKED_PENDING_REVISED_PLAN_OWNER_REVIEW`, `STEP_6_SOURCE_SHA=NOT_CREATED`, `IMPLEMENTATION_STEP_7=NOT_STARTED`, `RESOLVER_LIFETIME_TABLE_CONFLICT=CLOSED`, `CURRENT_IMPLEMENTATION_STATUS_CONFLICT=CLOSED`, `STEP5_STEP6_SCOPE_HISTORY_CONFLICT=CLOSED`, `MAKE_DECODE_RESET_TEXT_CONFLICT=CLOSED`, `STEP5_STACK_EVIDENCE_STATUS_CONFLICT=CLOSED`, `POST_APPROVAL_PLAN_HISTORY=CLOSED`, `PLAN_DEVIATION=NONE`, `SOURCE_OF_TRUTH_CONFLICT=NONE`. Step-5-Einzelframes: `MAKE_SNAPSHOT_INTO_FRAME=48 B`, `DECODE_SNAPSHOT_INTO_FRAME=512 B`, `PERSIST_COMMAND_FRAME_BEFORE=9280 B`, `PERSIST_COMMAND_FRAME_AFTER=688 B`; der finale kumulative Produkt-Stackgate bleibt `PENDING_FINAL_PRODUCT_CALLGRAPH_AFTER_STEP_6`. | STOP – Owner Full Review der korrigierten exakten Plan-SHA `9b101295af6468878057758356de33848ec18061`; keine Implementation beginnen. |
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
