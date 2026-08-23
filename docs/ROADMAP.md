# Projekt-Roadmap

Stand: 2026-08-23

Diese Datei ist die einzige aktuelle Status- und Taskuebersicht. Fachliche
Anforderungen, vollstaendige Issue-Inhalte und historische Begruendungen werden
nicht kopiert, sondern verlinkt.

## Aktuelle Arbeit

| Prioritaet | Arbeit | Status | Naechstes Gate |
|---:|---|---|---|
| 1 | Issue #29 – ESP32-Bring-up, Partition, Ressourcen und sichere Ausgangszustaende | `SOFTWARE_IMPLEMENTED_HARDWARE_TESTED_PASS_PENDING_LEVELS`; Plan `docs/tasks/issue-29-implementation-plan.md @ 4f49b44cff47f55bfd425d9e39c5a07256782ed7` freigegeben, Software-/Buildnachweise und zwei reale post-fix 40-s-Smokes liegen in Draft-PR #116 vor. Die Baseline bleibt aktorfrei. Das einzige verbleibende #29-Abnahmegate sind die sicheren unbelasteten MCU-/Gate-/Bootpegel. Die PCB-Revision/Silkscreen ist nach Ownerentscheidung kein Abnahmekriterium. #29 bleibt Prioritaet 1 und bis zur Pegelmessung offen; diese Priorisierung ist keine pauschale Blockade der #90-Software-/Hostarbeit. #90 bleibt actor-free; physische Ausgangspegel sind kein #90-Produktgate. | Owner-Nachweis der sicheren unbelasteten MCU-/Gate-/Bootpegel; danach Review-/Abnahmentscheidung ohne Issue-Schliessung durch den Agenten. Die #90-Software-/Host-/Oraclearbeit folgt ihren eigenen Owner-Gates. |
| 2 | Issue #90 – produktiver ESP-IDF-NVS-Adapter fuer `IStateStore` | `R5_9_SLICE_6_BLOCKED_PENDING_OWNER_REVIEW`; PR #118 bleibt OPEN/Draft. Slice 5 fuehrt den einzigen generischen `NvsStateStore`-Adapter, den v6.0.2-Read-/Write-Vertrag, den actor-free Owning-Context-Lifecycle fuer `state_store`/`fermentation`, getrennte 4-MB-Produkt-/Testlayouts, einen transitiv pruefbaren ESP-IDF-Consumer und einen projektspezifischen Host-Harness ueber die offizielle v6.0.2-BDL-API ein. Das korrigierte NVS-Modell weist `4784` vollstaendige Slot-Keyspace-Entries, `1036` tatsaechliche Same-Key-Update-Transient-Entries, `0` weitere bewiesene Transient-Entries, `5820` technische Minimum-Entries/`47` Seiten/`0x2F000` Bytes sowie `4` GC-/Fragmentierungsreserve- und `64` Wear-Planungsreserve-Seiten aus; geplant sind `115` Seiten/`0x73000` und damit `141` Seiten Headroom in der unveraenderten `0x100000`-Partition. Der offizielle `nvs_get_stats()`-Crosscheck meldet `before_used=1`, `full_used=4784` und `updated_used=4784`. Clean-Linux-BDL und der separate oeffentliche Consumer-Build sind ohne Shim PASS (`14/14` Hosttests). Die vollstaendige BDL-Matrix ist `COMPLETE`, `exhaustive=true`; die reale Matrix prueft `3999/4000/4001` Byte nach Restart bytegenau, beobachtet `14072` Writes und `503` Erases und klassifiziert GC-/Erase-Cuts nur bei exaktem OLD-/NEW-Readback als Backend-PASS. Echte `rh0`-Write-Cuts vor/nach dem Zielereignis mit Frozen-Image-/Fresh-Process-Recovery sind PASS. Die beiden BDL-I/O-Failure-Faelle verwenden den kanonischen Slice-2-Fall `run_rc0_new_valid_resume`, bewerten den tatsaechlich sichtbaren OLD-Head und sind PASS; der committed-NEW-Head ist ebenfalls PASS. Vier negative Gate-Selbsttests und die kanonische Oracle-ID-Sperre verhindern false-green bzw. erfundene Oracle-IDs. Die Product-Bridge- und Cut-Artefakte tragen die exakte Source-SHA und sind validiert. Native: `987/987 PASS`; ESP-IDF `esp32_bringup`/`esp32_release`, Consumer und offizieller esp-clang-Static-Check: PASS. Die Slice-6-Wiederholung bestaetigt `45/45` Production-vs-Oracle, die ESP-IDF-Profile, Builds, Static Checks, Layout-/Capacity- und Qualitaetspruefungen. Der workflow-genaue neutrale Artefakt-Secret-/Private-Path-Scan ist mit 15 Artefakten, `0` Secrets und `0` privaten absoluten Benutzerpfaden PASS. Sie bleibt jedoch wegen des noch nicht belastbar geschlossenen produktiven Release-Stack-/Scratch-Callgraphnachweises blockiert. Callback 12 bleibt `FAIL_CALLBACK_12_NOT_FOUND`, `KNOWN_BACKEND_LIMITATION`, `REAL_NVS_ONLY`, `product_recovery_gate=NOT_RUN`. `SOURCE_OF_TRUTH_CONFLICT: NONE`; Oracle-Luecken: `NONE`; `PRODUCT_RECOVERY_MISMATCH: NONE`. | Ownerreview des verbleibenden Release-Stack-/Scratch-Blockers; danach keine Slice-7-Arbeit ohne separate Freigabe |
| 3 | Issue #25 – gemeinsame rendererunabhaengige Device-UI-/App-Vertraege | `PLANNED_SPEC_PENDING`; gemeinsame Shell-, App-, View-Model- und Command-Vertraege fuer Touch und Web. Keine Renderer- oder Pluginplattform. | Eigener Plan und native Vertragsnachweise auf der Ressourcenbasis aus #29/#90 |
| 4 | Issue #26 – lokale Touch-Shell und Fermentations-Workspace | `PLANNED_SPEC_PENDING`; baut auf #25 auf und bleibt von realer Displayhardware getrennt, bis #31 folgt. | Eigener Plan, simulierte Bedienpfade und produktionsnahe Shell-/App-Vertraege |
| 5 | Issue #31 – realer Renderer, Display, Touch und Kalibrierung | `BLOCKED_HARDWARE`; folgt #25/#26/#29 und bringt die echte Bedienung am Gerät über dieselben Contracts. | Hardware-/Pin-/Controllerbeweis, Ressourcen-/Lizenznachweis, reale Bedienungs- und Kalibrierungstests |
| 6 | Issue #30 – reale DS18B20-Sensoradapter | `BLOCKED_HARDWARE`; #20/#21 sind abgeschlossen, #29 sowie die produktionsnahen Bedien-/Servicepfade bleiben Grundlage. | Eigener Plan, reale Bus-, ROM-, CRC-, Hot-Plug- und Fehlerprüfungen über die bestehende Produktsoftware |
| 7 | Issue #32 – Lüfter, Summer und Onboard-MOSFET-Ausgaenge | `BLOCKED_HARDWARE`; eigener abschliessbarer Hardware-/Adapterscope nach #23/#24/#29. Begrenzte nichtproduktive Serviceprüfungen sind zulässig; #28/#35/#106 sind keine #32-Abschlussvoraussetzungen. | Reale Zuordnung, Pegel, Boot-/Reset-, Verbraucher-, Strom-/Anlauf- und Adapter-/Testnachweise ohne produktive `ActuatorSafetyGateStatus::Allowed`-Freigabe |
| 8 | Issue #33 – BTS7960, R_IS/L_IS und begrenzte Peltierpruefungen | `BLOCKED_HARDWARE`; folgt auf dem abgeschlossenen #32-Hardwarefundament nach #30. | Begrenzte sichere Peltier-/BTS7960-Serviceprüfung über die echte Produktsoftware |
| 9 | Issue #106 strukturell – Per-Run-Producer-/Schema-/Snapshotmechanismus | `PLANNED_SPEC_PENDING`; darf nach #33 strukturell ohne erfundene Produktivwerte vorbereitet werden. | Eigener Plan; #35 bleibt Werte-/Grenzengate, keine TBD-Aktivierung |
| 10 | Issue #34 – Sensorvergleich und thermische Grundvermessung | `TBD_COMMISSIONING`; nach #29/#30/#31/#32/#33 und damit bewusst später als der bedienbare Gerätepfad. | Reale Messreihen, Offsets und auswertbare Messprotokolle; vollständige Lauf-/Diagnose-/Serviceexporte bleiben #28 |
| 11 | Issue #35 – PI-, Luft-, Aktor- und Sicherheitsparameter | `TBD_COMMISSIONING`; reale Werte und Grenzen nach #34. | Commissioning-Nachweise und verbindliche produktive Werte-/Safetyfreigabe |
| 12 | Issue #106 produktiv – Per-Run-Bindung und Aktoraktivierung | `PLANNED_SPEC_PENDING`; produktiver Abschluss erst mit den durch #35 gelieferten Werten und Grenzen. | Produktive Snapshot-/Recoverybindung und Aktivierung ohne TBD-Werte |
| 13 | Issue #19 / #28 / #36 / #37 – zurückgestellte Journale-, Diagnose-, Abnahme- und Releasegates | #19 bleibt `REVIEW_DRAFT – PRESERVE, NOT APPROVED, NOT CANONICAL, IMPLEMENTATION NOT_STARTED`; #28 bleibt späteres Diagnose-/Service-/Exportgate mit seiner #19-Abhängigkeit. | Neue vollständige #19-Planrevision auf aktuellem `main`; danach spätere vollständige Diagnose-/Abnahme-/Releasegates |

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
