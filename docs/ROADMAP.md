# Projekt-Roadmap

Stand: 2026-09-01

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

## Aktuelle Arbeit

| Prioritaet | Arbeit | Status | Naechstes Gate |
|---:|---|---|---|
| 1 | Issue #25 – gemeinsame rendererunabhaengige Device-UI-/App-Vertraege | `PLANNED_SPEC_PENDING`; gemeinsame Shell-, App-, View-Model- und Command-Vertraege fuer Touch und Web. Keine Renderer- oder Pluginplattform. Recovery-Projektion erst gegen den stabilen #124-Zielvertrag. | Eigener Plan und native Vertragsnachweise auf der Ressourcenbasis aus #29/#90/#124/#126 |
| 2 | Issue #26 – lokale Touch-Shell und Fermentations-Workspace | `PLANNED_SPEC_PENDING`; baut auf #25 auf und bleibt von realer Displayhardware getrennt, bis #31 folgt. | Eigener Plan, simulierte Bedienpfade und produktionsnahe Shell-/App-Vertraege |
| 3 | Issue #31 – realer Renderer, Display, Touch und Kalibrierung | `BLOCKED_HARDWARE`; folgt #25/#26 und bringt die echte Bedienung am Gerät über dieselben Contracts. | SSOT-/Verdrahtungskonformität, Controller-/SPI-/CS-/Reset-/Backlight-/Touch-/Wake-/Kalibrierungs-/Recovery-/Fehlerisolationsnachweise, Ressourcen-/Lizenznachweis und reale Funktionstests ohne generelles Pegelmessgate |
| 4 | Issue #30 – reale DS18B20-Sensoradapter | `BLOCKED_HARDWARE`; #20/#21 sind abgeschlossen, die produktionsnahen Bedien-/Servicepfade bleiben Grundlage. | Eigener Plan, reale Bus-, ROM-, CRC-, Hot-Plug- und Fehlerprüfungen über die bestehende Produktsoftware |
| 5 | Issue #32 – Lüfter, Summer und Onboard-MOSFET-Ausgaenge | `BLOCKED_HARDWARE`; eigener abschliessbarer Hardware-/Adapterscope nach #23/#24/#29. Begrenzte nichtproduktive Serviceprüfungen sind zulässig; #28/#35/#106 sind keine #32-Abschlussvoraussetzungen. | `ELECTRICAL_LEVEL_MEASUREMENT=NOT_REQUIRED_WAIVED`, SSOT-/Kanal-/Verbraucherzuordnung, funktionales AUS/EIN, Boot-/Reset-Sicherheit, Lüfter/Nachlauf/Summer und produktionsnaher Adapter-/Treiberpfad als `FUNCTIONAL_HARDWARE_VERIFICATION`; kein separates Adapter-Safety-Gate und keine produktive `ActuatorSafetyGateStatus::Allowed`-Freigabe |
| 6 | Issue #33 – BTS7960, R_IS/L_IS und begrenzte Peltierpruefungen | `BLOCKED_HARDWARE`; folgt auf dem abgeschlossenen #32-Hardwarefundament nach #30. R_IS/L_IS sind für R1 deaktiviert und `FUTURE_RELEASE`. | SSOT-/Funktionsnachweis, H-Brücken-Adapter-Safety mit Mutual Exclusion/Break-before-make/fail-closed/Boot-disabled und begrenzte sichere Peltier-/BTS7960-Serviceprüfung über die echte Produktsoftware |
| 7 | Issue #106 strukturell – Per-Run-Producer-/Schema-/Snapshotmechanismus | `PLANNED_SPEC_PENDING`; darf nach #33 strukturell ohne erfundene Produktivwerte vorbereitet werden. | Eigener Plan; #35 bleibt Werte-/Grenzengate, keine TBD-Aktivierung |
| 8 | Issue #34 – Sensorvergleich und thermische Grundvermessung | `TBD_COMMISSIONING`; nach #30/#31/#32/#33 und damit bewusst später als der bedienbare Gerätepfad. | Reale Messreihen, Offsets und auswertbare Messprotokolle; vollständige Lauf-/Diagnose-/Serviceexporte bleiben #28 |
| 9 | Issue #35 – PI-, Luft-, Aktor- und Sicherheitsparameter | `TBD_COMMISSIONING`; reale Werte und Grenzen nach #34. | Commissioning-Nachweise und verbindliche produktive Werte-/Safetyfreigabe |
| 10 | Issue #106 produktiv – Per-Run-Bindung und Aktoraktivierung | `PLANNED_SPEC_PENDING`; produktiver Abschluss erst mit den durch #35 gelieferten Werten und Grenzen. | Produktive Snapshot-/Recoverybindung und Aktivierung ohne TBD-Werte |
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
spaetere Hardware-/Netzwerkgates. #18 und #24 bleiben geschlossen, die
#121-Architektur bleibt unveraendert und #25 wird in diesem Schritt weder
implementiert noch geplant.

Die abgeschlossene Basis und die nächste fachliche Phase sind getrennt:

```text
abgeschlossene Basis: #29 -> #90 -> #121 -> #124 -> #126
nächste fachliche Phase: #25 -> #26 -> #31 -> #30 -> #32 -> #33
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
