# Projekt-Roadmap

Stand: 2026-08-31

Diese Datei ist die einzige aktuelle Status- und Taskuebersicht. Fachliche
Anforderungen, vollstaendige Issue-Inhalte und historische Begruendungen werden
nicht kopiert, sondern verlinkt.

## Aktuelle Arbeit

| Prioritaet | Arbeit | Status | Naechstes Gate |
|---:|---|---|---|
| 1 | Issue #29 – ESP32-Bring-up, Partition, Ressourcen und sichere Ausgangszustaende | `ISSUE29=OPEN`; `PR129=OPEN_DRAFT`; `CURRENT_DEVELOPMENT_BASE=integration/r1-development @ c1f5fbb5f19ab8e7d2c25708fe79777d523217d4`; `EXHAUSTIVE_STATIC_GATE_ATTEMPT=SUPERSEDED_KISS`; `FULL_TRANSITIVE_STATIC_CALLGRAPH_CLOSURE=NOT_REQUIRED`; `STATIC_ANALYSIS_ROLE=DIAGNOSTIC_EVIDENCE_NOT_GLOBAL_UPPER_BOUND`; `HARDWARE_HWM_ROLE=PRIMARY_EMPIRICAL_STACK_EVIDENCE`; `ANALYZER_REVERTED=YES`; `MAIN_CMAKE_ISSUE29_HEAP_INSTRUMENTATION_REVERTED=YES`; `DEVICE_PLATFORM_ISSUE29_INSTRUMENTATION_REVERTED=YES`; `CURRENT_MAX_KNOWN_STATIC_PATH_BYTES=72224`; `CURRENT_MAX_KNOWN_STATIC_PATH_IS_UPPER_BOUND=NO`; `OLD_PROBE_TASK_STACK_IS_BELOW_KNOWN_STATIC_PATH=YES`; `DIAGNOSTIC_PROBE_TASK_STACK_BYTES=98304`; `ROOT_CAUSE=UNRESOLVED`; `IMPLEMENTATION=NOT_STARTED_KISS_REVISION`; `HARDWARE_RUN=NO`; `LEVEL_MEASUREMENTS=NOT_RUN`; `ISSUE25_STARTED=NO`; `MERGE=NO`; `OWNER_PLAN_REVIEW=KISS_REVISION_PENDING`; `NEW_PLAN_SHA=PENDING_OWNER_REVIEW`. Details: Issue #29, `docs/ISSUE_29_MEASUREMENTS.md`, `docs/ISSUE_29_BUILD_REPORT.md`, `docs/tasks/issue-29-panic-requalification-correction-plan.md`; historische Freigabe `4a34967ac202196b7afceaebfe2b2429338d6d93` bleibt Provenienz. | Exakte neue Plan-SHA durch Owner vollständig freigeben; danach ausschließlich den privaten 96-KiB-Diagnose-Stacktest umsetzen, digitale Gates und den frischen #90-Harness-Isolationsnachweis erbringen, dann STOP vor Flash. Pegelmessung erst nach 3/3 PASS und Owner-Review. #29 bleibt von der #90-Power-Cut-Kampagne getrennt; #25 bleibt blockiert. |
| 2 | Issue #90 – produktiver ESP-IDF-NVS-Adapter fuer `IStateStore` | `ISSUE90=CLOSED/COMPLETED`; `PR128=MERGED @ c1f5fbb5f19ab8e7d2c25708fe79777d523217d4` (in `integration/r1-development`); `OWNER_FINAL_REVIEW=PASS`; `REAL_POWER_CUTS=6_OF_6_PASS`; `PRODUCTION_RESTORE=PASS`; `POST_RESTORE_PRODUCT_BOOT=PASS`. Details: Issue #90, PR #128. | Keine offene #90-Arbeit; Issue geschlossen. Der separate #29-Panic-Befund bleibt unabhaengig offen und wurde durch #90 weder geloest noch geschlossen. |
| 3 | Issue #119 – produktive StateStore-/Application-Composition anbinden und real verifizieren | `ISSUE119=CLOSED/SUPERSEDED`; `PR120=CLOSED_UNMERGED`; `DISPOSITION=FAILED_SUPERSEDED_INTERMEDIATE_APPROACH`; `INCLUDED_IN_CURRENT_INTEGRATION_BASELINE=NO`; der historische Fehlpfad bleibt dokumentiert. | Keine weitere Arbeit auf #119/#120; die aktuelle Implementierung ist in der R1-Integrationsbaseline erhalten. |
| 4 | Issue #121 – Release-1 Device-/Application-Lifecycle- und Safety-Policy-Vereinfachung | `ISSUE121=CLOSED/COMPLETED`; `OWNER_FINAL_REVIEW=PASS`; `ISSUE121_IMPLEMENTATION=PASS`; `STEP8_HARDWARE_REQUALIFICATION=PASS`; `INTEGRATION_BASELINE_SHA=e62e35800ad46fe11ec72f9e0b4715ee561c577b0`; kanonischer Plan: `docs/tasks/issue-121-lifecycle-safety-simplification-plan.md @ 3fb4d17418d449818b4f941f99e261525e25a54d`. | Keine offenen #121-Kriterien; #29-Level bleibt separates offenes Hardwaregate. #90 ist geschlossen. |
| 5 | Issue #124 – R1-Stromausfall-Recovery auf einfachen Zeitvertrag konsolidieren | `ISSUE124=CLOSED`; `PR125=MERGED`; `PR125_MERGE=5b8b86b99347bb0bb104dd1c2968040656119440`; `HARDWARE_RUN=NOT_RUN`; `OWNER_DECISIONS_REQUIRED=NONE`; `R1_PHASE_TIMER_CONTINUITY_FIELD=priorBootPhaseElapsed`; `OUTAGE_TIME_IN_OBSERVED_RUN_SECONDS=NO`; `TEMPERATURE_WEIGHTED_RECOVERY_R1=NO`; kanonischer Plan: `docs/tasks/issue-124-r1-power-loss-recovery-plan.md @ 6f4e1a54d521ba60de185f350d571cbefaa23d71`; #18/#24 bleiben historische Provenienz. | Keine weitere #124-Implementierung; PR #125 ist gemergt, die fachliche Recoverysemantik bleibt unverändert. |
| 6 | Issue #126 – R1-Absolute-Zeitplattform mit DS3231SN und ESP-IDF-SNTP | `ISSUE126=CLOSED`; `PR127=MERGED @ 18fb96b79608914568b98d2ec06694d75ed0402e` (in `integration/r1-development`); `OWNER_FINAL_IMPLEMENTATION_REVIEW=PASS`; `GITHUB_CI=PASS`; `RTC_DEVICE=DS3231SN`; `RTC_HARDWARE_OPTIONAL=YES`; `NTP_ONLY_MODE_SUPPORTED=YES`; `RTC_HARDWARE=BLOCKED_OWNER_HARDWARE_PENDING`; `NTP_REAL_NETWORK_RUN=NOT_RUN`; kanonischer Plan: `docs/tasks/issue-126-absolute-time-rtc-ntp-plan.md @ 52bd69f37e7baac782ebd2fb927f3fa57003f1c7`; #89 bleibt Connectivity-Eigentuemer, #124 bleibt fachlich unveraendert. | Keine offene #126-Implementierungsarbeit; Issue geschlossen, PR gemergt. Reale RTC-Hardware-/NTP-Netzwerknachweise bleiben separate, spaetere Hardware-/Netzwerkgates. |
| 7 | Issue #25 – gemeinsame rendererunabhaengige Device-UI-/App-Vertraege | `PLANNED_SPEC_PENDING`; gemeinsame Shell-, App-, View-Model- und Command-Vertraege fuer Touch und Web. Keine Renderer- oder Pluginplattform. Recovery-Projektion erst gegen den stabilen #124-Zielvertrag. | Eigener Plan und native Vertragsnachweise auf der Ressourcenbasis aus #29/#90/#124/#126 |
| 8 | Issue #26 – lokale Touch-Shell und Fermentations-Workspace | `PLANNED_SPEC_PENDING`; baut auf #25 auf und bleibt von realer Displayhardware getrennt, bis #31 folgt. | Eigener Plan, simulierte Bedienpfade und produktionsnahe Shell-/App-Vertraege |
| 9 | Issue #31 – realer Renderer, Display, Touch und Kalibrierung | `BLOCKED_HARDWARE`; folgt #25/#26/#29 und bringt die echte Bedienung am Gerät über dieselben Contracts. | Hardware-/Pin-/Controllerbeweis, Ressourcen-/Lizenznachweis, reale Bedienungs- und Kalibrierungstests |
| 10 | Issue #30 – reale DS18B20-Sensoradapter | `BLOCKED_HARDWARE`; #20/#21 sind abgeschlossen, #29 sowie die produktionsnahen Bedien-/Servicepfade bleiben Grundlage. | Eigener Plan, reale Bus-, ROM-, CRC-, Hot-Plug- und Fehlerprüfungen über die bestehende Produktsoftware |
| 11 | Issue #32 – Lüfter, Summer und Onboard-MOSFET-Ausgaenge | `BLOCKED_HARDWARE`; eigener abschliessbarer Hardware-/Adapterscope nach #23/#24/#29. Begrenzte nichtproduktive Serviceprüfungen sind zulässig; #28/#35/#106 sind keine #32-Abschlussvoraussetzungen. | Reale Zuordnung, Pegel, Boot-/Reset-, Verbraucher-, Strom-/Anlauf- und Adapter-/Testnachweise ohne produktive `ActuatorSafetyGateStatus::Allowed`-Freigabe |
| 12 | Issue #33 – BTS7960, R_IS/L_IS und begrenzte Peltierpruefungen | `BLOCKED_HARDWARE`; folgt auf dem abgeschlossenen #32-Hardwarefundament nach #30. | Begrenzte sichere Peltier-/BTS7960-Serviceprüfung über die echte Produktsoftware |
| 13 | Issue #106 strukturell – Per-Run-Producer-/Schema-/Snapshotmechanismus | `PLANNED_SPEC_PENDING`; darf nach #33 strukturell ohne erfundene Produktivwerte vorbereitet werden. | Eigener Plan; #35 bleibt Werte-/Grenzengate, keine TBD-Aktivierung |
| 14 | Issue #34 – Sensorvergleich und thermische Grundvermessung | `TBD_COMMISSIONING`; nach #29/#30/#31/#32/#33 und damit bewusst später als der bedienbare Gerätepfad. | Reale Messreihen, Offsets und auswertbare Messprotokolle; vollständige Lauf-/Diagnose-/Serviceexporte bleiben #28 |
| 15 | Issue #35 – PI-, Luft-, Aktor- und Sicherheitsparameter | `TBD_COMMISSIONING`; reale Werte und Grenzen nach #34. | Commissioning-Nachweise und verbindliche produktive Werte-/Safetyfreigabe |
| 16 | Issue #106 produktiv – Per-Run-Bindung und Aktoraktivierung | `PLANNED_SPEC_PENDING`; produktiver Abschluss erst mit den durch #35 gelieferten Werten und Grenzen. | Produktive Snapshot-/Recoverybindung und Aktivierung ohne TBD-Werte |
| 17 | Issue #19 / #28 / #36 / #37 – zurückgestellte Journale-, Diagnose-, Abnahme- und Releasegates | #19 bleibt `REVIEW_DRAFT – PRESERVE, NOT APPROVED, NOT CANONICAL, IMPLEMENTATION NOT_STARTED`; #28 bleibt späteres Diagnose-/Service-/Exportgate mit seiner #19-Abhängigkeit. | Neue vollständige #19-Planrevision auf aktuellem `main`; danach spätere vollständige Diagnose-/Abnahme-/Releasegates |

## Naechste fachliche Arbeit

PR #110 / Issue #24 und PR #113 / Issue #111 sind auf dem aktuellen `main`
abgeschlossen. Der Release-1-Safety-Core bleibt beim bestätigten KISS- und
fail-closed-Vertrag; Hardware-, reale Aktor-, NVS- und
Inbetriebnahmenachweise sind dadurch nicht vorweggenommen.

Issue #119 / PR #120 sind geschlossen und bleiben als fehlgeschlagener
Composition-/Diagnosepfad historische Evidenz (neuer `LoadProhibited`-Panic).
Die daraus hervorgegangene Release-1-Vereinfachung der
Lifecycle-/Safety-/Persistenz-Verträge ist in Issue #121 umgesetzt,
ownerreviewt und in der R1-Integrationsbaseline enthalten. Issue #90 ist nach
PR-#128-Merge und Owner Final Review geschlossen. Der separate #29-Panic
(`LoadProhibited` im Bring-up-Probe) ist jetzt auch auf dem normalen
`esp32_bringup`-Profil real reproduziert und bleibt Prioritaet 1, bis ein
ownerfreigegebener Korrekturplan vorliegt.

Issue #124 ist die vor #25 eingeschobene, eigenstaendige R1-Recovery-
Planung. PR #125 ist in die aktuelle R1-Integrationsbaseline gemergt; die
fachliche #124-Policy bleibt unveraendert. Issue #126 liefert davor die
app-neutrale trusted UTC ueber RTC/NTP und ist mit PR #127 gemergt und
geschlossen; reale RTC-Hardware-/NTP-Netzwerknachweise bleiben separate,
spaetere Hardware-/Netzwerkgates. #18 und #24 bleiben geschlossen, die
#121-Architektur bleibt unveraendert und #25 wird in diesem Schritt weder
implementiert noch geplant.

Die endgültige Priorisierungsrichtung ist:

```text
#29 -> #90 -> #121 -> #124 -> #126 -> #25 -> #26 -> #31 -> #30 -> #32 -> #33
  -> erste real bedienbare Fermenter-Hardwareintegration
  -> #106 strukturell -> #34 -> #35 -> #106 produktiv
  -> spätere vollständige Diagnose-/Abnahme-/Releasegates
```

#29 und #90 liefern zuerst reale Plattform-, Ressourcen- und Persistenzbasis.
#126 vervollstaendigt davor den app-neutralen Zeitvertrag, ohne #89-Connectivity
zu duplizieren oder #124 fachlich zu aendern. #25/#26 bilden darauf die
wiederverwendbare Device Shell und den
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
- PR #127 / Issue #126 sind gemergt und geschlossen. Die digitale
  RTC-/NTP-Implementierung dupliziert #89-Connectivity nicht und ändert den
  fachlichen #124-Vertrag nicht. Reale RTC-/Netzwerk- und
  Power-Cycle-Nachweise bleiben separate Hardware-/Netzwerk-Gates.
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
- Issue #89 (WLAN-Onboarding-Evaluation) benoetigt vor Beginn einen eigenen
  Live-Abgleich und freigegebenen Plan. Issue #90 ist geschlossen.
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
