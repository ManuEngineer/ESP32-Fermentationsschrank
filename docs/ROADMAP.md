# Projekt-Roadmap

Stand: 2026-09-05

Diese Datei ist die einzige aktuelle Status- und Taskuebersicht. Fachliche
Anforderungen, vollstaendige Issue-Inhalte und historische Begruendungen werden
nicht kopiert, sondern verlinkt.

## Abgeschlossen / synchronisiert

| Arbeit | Status |
|---|---|
| Issue #29 / PR #129 | `ISSUE29=CLOSED_COMPLETED`; `PR129=MERGED`; `PR129_MERGE_COMMIT=1fc22d693bae8572144bf61d242a2fe6d0b093bc`; `OWNER_FINAL_REVIEW=PASS`; `CI_RUN=996`; `CI_RESULT=PASS`; `ELECTRICAL_LEVEL_MEASUREMENT=NOT_REQUIRED_WAIVED`; `ACTUATOR_RELEASE=NO` |
| Issue #90 / PR #128 | `ISSUE90=CLOSED/COMPLETED`; `PR128=MERGED @ c1f5fbb5f19ab8e7d2c25708fe79777d523217d4`; `OWNER_FINAL_REVIEW=PASS`; `REAL_POWER_CUTS=6_OF_6_PASS`; `PRODUCTION_RESTORE=PASS`; `POST_RESTORE_PRODUCT_BOOT=PASS` |
| Issue #119 / PR #120 | `ISSUE119=CLOSED/SUPERSEDED`; `PR120=CLOSED_UNMERGED`; `DISPOSITION=FAILED_SUPERSEDED_INTERMEDIATE_APPROACH` |
| Issue #121 | `ISSUE121=CLOSED/COMPLETED`; `OWNER_FINAL_REVIEW=PASS`; `ISSUE121_IMPLEMENTATION=PASS`; `STEP8_HARDWARE_REQUALIFICATION=PASS`; `INTEGRATION_BASELINE_SHA=e62e35800ad46fe11ec72f9e0b4715ee561c577b0` |
| Issue #124 / PR #125 | `ISSUE124=CLOSED`; `PR125=MERGED`; `PR125_MERGE=5b8b86b99347bb0bb104dd1c2968040656119440`; `HARDWARE_RUN=NOT_RUN`; `OWNER_DECISIONS_REQUIRED=NONE` |
| Issue #126 / PR #127 | `ISSUE126=CLOSED`; `PR127=MERGED @ 18fb96b79608914568b98d2ec06694d75ed0402e`; `OWNER_FINAL_IMPLEMENTATION_REVIEW=PASS`; `GITHUB_CI=PASS`; `RTC_HARDWARE=BLOCKED_OWNER_HARDWARE_PENDING`; `NTP_REAL_NETWORK_RUN=NOT_RUN` |
| Issue #130 / PR #131 | `ISSUE130=CLOSED/COMPLETED`; `PR131=MERGED`; `PR131_MERGE_SHA=1fd8f6af53d1b3c23f3aa46c73c4fc3da7513d6d`; `GPIO_SSOT=MERGED`; `GPIO_MATRIX=PLANNED_NOT_CONFIRMED`; `ELECTRICAL_LEVEL_MEASUREMENT=NOT_REQUIRED_WAIVED`; `SSOT_CONFORMANCE=PENDING`; `FUNCTIONAL_HARDWARE_VERIFICATION=PENDING`; `ACTUATOR_RELEASE=NO`; `BOARD_FAMILY_REFERENCE_MATCH=CONFIRMED_BY_OWNER` |
| Issue #136 / PR #137 | Technischer Korrekturscope abgeschlossen; `PR137=MERGED`; `PR137_HEAD=4213ce2b16023fe3074703c97942191a3b60e5f1`; `PR137_MERGE_COMMIT=c347875761c6e357b00ca0c2ed0d185766c17846`; `PR137_REVIEW=PASS`; `PR137_LOCAL_VERIFICATION=PASS`; `PR137_GITHUB_CI=PASS` |
| Issue #134 / PR #135 | `PR135=MERGED`; `PR135_SOURCE_HEAD=5fc476dcf912a4d95cf837a42b8f69d0e183dd17`; `PR135_MERGE_COMMIT=86e55499d9f0dd4dbd2d9fbc95d04549df4d429c`; `CUMULATIVE_OWNER_REVIEW=PASS`; `GITHUB_CI_RUN=1007`; `GITHUB_CI=PASS`; `MERGE_TREE_EQUALS_REVIEWED_SOURCE_TREE=YES`; `ACTUATOR_RELEASE=NO` |
| Issue #25 / PR #142 | `ISSUE25_STATUS=CLOSED_COMPLETED`; `PR142=MERGED`; `PR142_SOURCE_HEAD=6ff0176651cf5f5dfe8b04d424377efa99ce551f`; `PR142_MERGE_COMMIT=87bd668e45ab71a20ceb24ce65fcb5d1440725a8`; `OWNER_FULL_REVIEW=PASS`; `GITHUB_CI_RUN=1015`; `GITHUB_CI=PASS`; `ACTUATOR_RELEASE=NO` |
| Issue #145 / PR #146 | `ISSUE145_STATUS=CLOSED_COMPLETED`; `PR146=MERGED`; `PR146_SOURCE_HEAD=790be691150ddceeeedec8394e1bc66bcad90c57`; `PR146_MERGE_COMMIT=f5aca945c3009408c091a8f03b000e8309af6bcf`; `FIX_VERIFICATION=PASS`; `OPEN_BLOCKERS=0`; `PRODUCTION_CODE_CHANGED=NO` |
| Issue #148 / PR #149 | `ISSUE148_STATUS=CLOSED_COMPLETED`; `PR149=MERGED`; `PR149_SOURCE_HEAD=f5aca945c3009408c091a8f03b000e8309af6bcf`; `PR149_MERGE_SHA=e84dfa8abf220220a33e6e21b95dbd0d7bd9ac90`; `MAIN_RESTORED_AS_NORMAL_DEVELOPMENT_BASE=YES` |
| Issue #144 / PR #147 | `ISSUE144_STATUS=CLOSED_COMPLETED`; `PR147=MERGED`; `PR147_SOURCE_HEAD=81bb985146d2ad926dfc156ab1136f8fefe2b3cb`; `PR147_MERGE_COMMIT=0b8b4cc1673f40296a510fdc0d79440c616ffeb8`; `RUN_IDENTITY_PROVENANCE=MERGED`; `ACTUATOR_RELEASE=NO` |

## Aktuelle Arbeit

| Prioritaet | Arbeit | Status | Naechstes Gate |
|---:|---|---|---|
| 0 | Issue #152 – owning Vertrag manueller Zeit-/Temperaturlauf | `ISSUE152_STATUS=PLANNING`; `PR153=OPEN/DRAFT`; `PR153_HEAD=SEE_CURRENT_SESSION_HANDOVER`; `PLAN_COMMIT=292a8096b981c2137545bb88961f92b9b7a52139`; `PLAN_FIRST=YES`; `BASE_SHA=0b8b4cc1673f40296a510fdc0d79440c616ffeb8`; `IMPLEMENTATION=NOT_STARTED`; `DOWNSTREAM_ISSUE=26`; `ACTUATOR_RELEASE=NO` | Enge Plan-Fix-Verification des korrigierten #152-Plans und Ownerfreigabe des exakten Plan-Commits |
| 1 | Issue #26 – lokale Touch-Shell und Fermentations-Workspace | `ISSUE26_STATUS=PLANNING`; `ISSUE26_STARTED=YES`; `BLOCKED_BY_ISSUE144=NO`; `BLOCKED_BY_ISSUE152=YES_FOR_MANUAL_TIMED_SCOPE`; `IMPLEMENTATION=NOT_STARTED`; `ACTUATOR_RELEASE=NO`; baut auf dem gemergten #25- und #144-Vertrag auf und bleibt von realer Displayhardware getrennt, bis #31 folgt. | #152 owning Vertrag planen, reviewen, ownerfreigeben, umsetzen und mergen; danach #26-Plan/Provenienz auf den exakten Merge-HEAD synchronisieren |
| 2 | Issue #31 – realer Renderer, Display, Touch und Kalibrierung | `BLOCKED_HARDWARE`; folgt #26 und bringt die echte Bedienung am Gerät über dieselben Contracts. | SSOT-/Verdrahtungskonformität, Controller-/SPI-/CS-/Reset-/Backlight-/Touch-/Wake-/Kalibrierungs-/Recovery-/Fehlerisolationsnachweise, Ressourcen-/Lizenznachweis und reale Funktionstests ohne generelles Pegelmessgate |
| 3 | Issue #30 – reale DS18B20-Sensoradapter | `BLOCKED_HARDWARE`; #20/#21 sind abgeschlossen, die produktionsnahen Bedien-/Servicepfade bleiben Grundlage. | Eigener Plan, reale Bus-, ROM-, CRC-, Hot-Plug- und Fehlerprüfungen über die bestehende Produktsoftware |
| 4 | Issue #32 – Lüfter, Summer und Onboard-MOSFET-Ausgaenge | `BLOCKED_HARDWARE`; eigener abschliessbarer Hardware-/Adapterscope nach #23/#24/#29. Begrenzte nichtproduktive Serviceprüfungen sind zulässig; #28/#35/#106 sind keine #32-Abschlussvoraussetzungen. | `ELECTRICAL_LEVEL_MEASUREMENT=NOT_REQUIRED_WAIVED`, SSOT-/Kanal-/Verbraucherzuordnung, funktionales AUS/EIN, Boot-/Reset-Sicherheit, Lüfter/Nachlauf/Summer und produktionsnaher Adapter-/Treiberpfad als `FUNCTIONAL_HARDWARE_VERIFICATION`; kein separates Adapter-Safety-Gate und keine produktive `ActuatorSafetyGateStatus::Allowed`-Freigabe |
| 5 | Issue #33 – BTS7960, R_IS/L_IS und begrenzte Peltierpruefungen | `BLOCKED_HARDWARE`; folgt auf dem abgeschlossenen #32-Hardwarefundament nach #30. R_IS/L_IS sind fuer R1 bewusst unbeschaltet und deaktiviert, als ADC1-Reserve fuer eine moegliche spaetere Integration `FUTURE_RELEASE` reserviert und nicht verworfen; `R1_BLOCKED_BY_R_IS_L_IS=NO`. | SSOT-/Funktionsnachweis, H-Brücken-Adapter-Safety mit Mutual Exclusion/Break-before-make/fail-closed/Boot-disabled und begrenzte sichere Peltier-/BTS7960-Serviceprüfung über die echte Produktsoftware |
| 6 | Issue #106 strukturell – Per-Run-Producer-/Schema-/Snapshotmechanismus | `PLANNED_SPEC_PENDING`; darf nach #33 strukturell ohne erfundene Produktivwerte vorbereitet werden. | Eigener Plan; #35 bleibt Werte-/Grenzengate, keine TBD-Aktivierung |
| 7 | Issue #34 – Sensorvergleich und thermische Grundvermessung | `TBD_COMMISSIONING`; nach #30/#31/#32/#33 und damit bewusst später als der bedienbare Gerätepfad. | Reale Messreihen, Offsets und auswertbare Messprotokolle; vollständige Lauf-/Diagnose-/Serviceexporte bleiben #28 |
| 8 | Issue #35 – PI-, Luft-, Aktor- und Sicherheitsparameter | `TBD_COMMISSIONING`; reale Werte und Grenzen nach #34. | Commissioning-Nachweise und verbindliche produktive Werte-/Safetyfreigabe |
| 9 | Issue #106 produktiv – Per-Run-Bindung und Aktoraktivierung | `PLANNED_SPEC_PENDING`; produktiver Abschluss erst mit den durch #35 gelieferten Werten und Grenzen. | Produktive Snapshot-/Recoverybindung und Aktivierung ohne TBD-Werte |
| 10 | Issue #19 / #28 / #36 / #37 – zurückgestellte Journale-, Diagnose-, Abnahme- und Releasegates | #19 bleibt `REVIEW_DRAFT – PRESERVE, NOT APPROVED, NOT CANONICAL, IMPLEMENTATION NOT_STARTED`; #28 bleibt späteres Diagnose-/Service-/Exportgate mit seiner #19-Abhängigkeit. | Neue vollständige #19-Planrevision auf aktuellem `main`; danach spätere vollständige Diagnose-/Abnahme-/Releasegates |

## Parallele Governance-Arbeit

- Issue #145 / PR #146 – Builder-/Reviewer-, Convergence- und Compute-Governance abgeschlossen: `ISSUE145_STATUS=CLOSED_COMPLETED`; `PR146=MERGED`; `FIX_VERIFICATION=PASS`; `OPEN_BLOCKERS=0`; `PRODUCTION_CODE_CHANGED=NO`.
- Issue #150 / PR #151 – Pre-Ready-CI-Parity-Gate vor `Ready for review` abgeschlossen: `ISSUE150_STATUS=CLOSED_COMPLETED`; `PR151=MERGED`; `PR151_MERGE_COMMIT=913f4c90084b77684ba37674e9070d288b22f5c1`; `IMPLEMENTATION=COMPLETE`; `ACTUATOR_RELEASE=NO`; `SEPARATE_FROM_ISSUE147=YES`.

## Naechste fachliche Arbeit

Der kumulative Integrationscheckpoint Issue #134 / PR #135 ist erfolgreich nach
`main` promoted. PR #149 / Issue #148 hat `main` als normale
Entwicklungsbasis wiederhergestellt; `integration/r1-development` wird nicht
mehr als regulaere Entwicklungsbasis verwendet. Die aktuelle fachliche Arbeit
ist nach dem Merge von PR #147 zunaechst Issue #152 fuer den owning Vertrag
des weiterhin verbindlichen manuellen Zeit-/Temperaturlaufs. Issue #26 ist vom
abgeschlossenen #144-Vertrag nicht mehr blockiert, bleibt aber fuer diesen
konkreten R1-Bedienpfad vom #152-Vertrag abhaengig; seine nachgelagerte
Implementation ist ein spaeterer Schritt nach Planfreigabe.
`ISSUE144_STATUS=CLOSED_COMPLETED`, `PR147=MERGED`,
`ISSUE152_STATUS=PLANNING`, `IMPLEMENTATION=NOT_STARTED`,
`BLOCKED_BY_ISSUE152=YES_FOR_MANUAL_TIMED_SCOPE` und `ACTUATOR_RELEASE=NO`
gelten ab diesem Roadmap-Commit.

PR #110 / Issue #24 und PR #113 / Issue #111 sind auf dem aktuellen `main`
abgeschlossen. Der Release-1-KISS-/fail-closed-Vertrag ist im stateless
`ActuationInterlock` und den zugehoerigen aktuellen Fachvertraegen umgesetzt;
Hardware-, reale Aktor-, NVS- und Inbetriebnahmenachweise sind dadurch nicht
vorweggenommen.

Issue #119 / PR #120 sind geschlossen und bleiben als fehlgeschlagener
Composition-/Diagnosepfad historische Evidenz (neuer `LoadProhibited`-Panic).
Die daraus hervorgegangene Release-1-Vereinfachung der
Lifecycle-/Safety-/Persistenz-Verträge ist in Issue #121 umgesetzt,
ownerreviewt und in der R1-Integrationsbaseline enthalten. Issue #90 ist nach
PR-#128-Merge und Owner Final Review geschlossen. Der separate #29-Panic
(`LoadProhibited` im Bring-up-Probe) ist auf dem normalen `esp32_bringup`-
Profil real reproduziert und als abgeschlossene #29-Evidenz historisch dokumentiert. Der KISS-V2-Plan
`b7d80de7d6e23fd792c2bd48eaa27052a8c61201` ist ownerfreigegeben; der
Current-base-Kontrollpanic ist bestaetigt. Der frühere 96-KiB-Run ist wegen
zusätzlicher Runtime-Log-Änderungen und einer Vorab-Resetsequenz historische,
nicht-qualifizierende Evidenz (`0` gültige Boots). Das korrigierte 96-KiB-
Artefakt besteht dagegen die actor-freie Requalifikation `3_OF_3_PASS`; die
Completion-HWM von 25840 B ergibt 72464 B beobachtete Peak-Nutzung und
überschreitet das alte 67584-B-Budget. Der Owner hat
`ROOT_CAUSE=CONFIRMED_STALE_DIAGNOSTIC_TASK_STACK_BUDGET` bestätigt. Die
#29-Pegelmessung ist bewusst `NOT_RUN_WAIVED_BY_OWNER` und blockiert den
#29-Abschluss nicht; kanonisch gilt `ELECTRICAL_LEVEL_MEASUREMENT=
NOT_REQUIRED_WAIVED`. Ein elektrischer Mess-PASS oder eine Aktorfreigabe
folgt daraus nicht.

Issue #124 ist die vor #25 eingeschobene, eigenstaendige R1-Recovery-
Planung. PR #125 ist in die aktuelle R1-Integrationsbaseline gemergt; die
fachliche #124-Policy bleibt unveraendert. Issue #126 liefert davor die
app-neutrale trusted UTC ueber RTC/NTP und ist mit PR #127 gemergt und
geschlossen; reale RTC-Hardware-/NTP-Netzwerknachweise bleiben separate,
spaetere Hardware-/Netzwerkgates. #18 und #24 bleiben geschlossen.
Die #121-Architektur bleibt unveraendert; Issue #25 ist mit PR #142
abgeschlossen und bildet den gemergten Vertrag für #26.

Die abgeschlossene Basis und die nächste fachliche Phase sind getrennt:

```text
abgeschlossene Basis: #29 -> #90 -> #121 -> #124 -> #126 -> #25 -> #144
nächste fachliche Phase: #152 -> #26 -> #31 -> #30 -> #32 -> #33
  -> erste real bedienbare Fermenter-Hardwareintegration
  -> #106 strukturell -> #34 -> #35 -> #106 produktiv
  -> spätere vollständige Diagnose-/Abnahme-/Releasegates
```

#29 und #90 liefern zuerst reale Plattform-, Ressourcen- und Persistenzbasis.
#126 vervollstaendigt davor den app-neutralen Zeitvertrag, ohne #89-Connectivity
zu duplizieren oder #124 fachlich zu aendern. Der gemergte #25-Vertrag und
#26 bilden darauf die wiederverwendbare Device Shell und den
Fermentations-Workspace; #144 stellt den neutralen Run-Identity-/Provenienz-
vertrag bereit, #152 ergänzt davor den owning Vertrag für den manuellen
Zeit-/Temperaturlauf. #31 bringt danach dieselben
rendererunabhängigen Contracts auf reales Display und Touch. #30, #32 und #33
werden danach über die bis dahin vorhandenen produktionsnahen Bedien-, Service-
und Diagnosepfade integriert.

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
`CONFIGURATION_SAFETY_INTEGRATION_GATE` sind abgeschlossen. Verbleibende
reale Hardware-, Ressourcen- und Releasegates stehen mit ihrer aktuellen
Ownership in `OPEN_POINTS.md` und den offenen Hardware-/Release-Issues; #90
ist geschlossene historische Persistenzprovenienz.

## Zulaessige Parallelitaet

- PR #113 / Issue #111 sind als Markdown-only-Governancearbeit abgeschlossen;
  Firmware- und Safety-Semantik bleiben davon unberührt.
- PR #127 / Issue #126 sind gemergt und geschlossen. Die digitale
  RTC-/NTP-Implementierung dupliziert #89-Connectivity nicht und ändert den
  fachlichen #124-Vertrag nicht. Reale RTC-/Netzwerk- und
  Power-Cycle-Nachweise bleiben separate Hardware-/Netzwerk-Gates.
- #29 und #90 bilden die erste reale Plattformbasis; #144 ist mit PR #147
  gemergt. Danach folgen #152, #26 und #31 für die echte Device Shell, App
  und Bedienung auf dem gemergten #25-Vertrag.
- #152 ist der verpflichtende owning Scope für den weiterhin verbindlichen
  manuellen Zeit-/Temperaturlauf. #26 konsumiert ihn später nur; bis zum
  Merge von #152 bleibt genau dieser #26-Teilpfad blockiert und darf nicht als
  R1-erledigt behauptet werden.
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

- Issue #144 / PR #147 ist abgeschlossen und Bestandteil des aktuellen `main`.
- Issue #152 ist der verpflichtende owning Scope für den manuellen
  Zeit-/Temperaturlauf. #26 bleibt für diesen Teilpfad bis zum Merge von #152
  blockiert und konsumiert den Vertrag danach nur.
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

Die folgenden Einträge sind historische Abschlussprovenienz, keine aktuelle
Architektur- oder Status-SSOT. Ihre damalige Terminologie wird nicht
rückwirkend umgeschrieben.

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
- PR #110 / Issue #24: Release-1 FaultCodes, Disposition, `SAFE_BOOT` und
  Fehlerinjektion; der aktuelle fail-closed Vertrag liegt im stateless
  `ActuationInterlock` und den Producer-eigenen Latches.
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
