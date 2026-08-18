# Projekt-Roadmap

Stand: 2026-08-18

Diese Datei ist die einzige aktuelle Status- und Taskuebersicht. Fachliche
Anforderungen, vollstaendige Issue-Inhalte und historische Begruendungen werden
nicht kopiert, sondern verlinkt.

## Aktuelle Arbeit

| Prioritaet | Arbeit | Status | Naechstes Gate |
|---:|---|---|---|
| 1 | Issue #29 – ESP32-Bring-up, Partition, Ressourcen und sichere Ausgangszustaende | `READY`; erster technischer Schritt zur realen ESP32-/Flash-/Heap-/Stack-/UART-Basis. Die Baseline bleibt aktorfrei und behauptet keine produktive Aktorfreigabe. | Eigener Plan-first-Draft-PR und danach reale Board-, Flash-, Boot-, UART- und Ressourcenmessungen |
| 2 | Issue #90 – produktiver ESP-IDF-NVS-Adapter fuer `IStateStore` | `PLANNED_SPEC_PENDING`; harte Grundlage #29 sowie der abgeschlossene generische Store-/Wirevertrag #54 sind zu verwenden. Keine zweite Persistenzarchitektur. | #29-Baseline und eigener freigegebener #90-Plan; danach NVS-/Partitions-/Power-Cut-/Readback-Nachweise |
| 3 | Issue #30 – reale DS18B20-Sensoradapter | `BLOCKED_HARDWARE`; #20/#21 sind abgeschlossen, #29 bleibt die reale Board-/Ressourcenbasis. | #29-Nachweis, eigener Plan und reale Bus-, ROM-, CRC-, Hot-Plug- und Fehlerprüfungen |
| 4 | Issue #32 – Lüfter, Summer und Onboard-MOSFET-Ausgaenge | `BLOCKED_HARDWARE`; #23/#24 sind abgeschlossen, #29 bleibt die offene Hardwarebasis, und die bestehende Abhängigkeit auf #28 bleibt unverändert sichtbar. | #29-Hardwarebasis plus Auflösung der bestehenden #28-Abhängigkeit; keine kosmetische Dependency-Streichung |
| 5 | Issue #33 – BTS7960, R_IS/L_IS und begrenzte Peltierpruefungen | `BLOCKED_HARDWARE`; folgt erst nach #29/#30/#32 und den unveränderten Safety-/Aktorverträgen. | sichere Lüfter-/MOSFET-Basis, Sensorbasis, eigener Plan und begrenzte aktorfreie bzw. Service-Hardwaretests |
| 6 | Issue #19 – Journale, Aufbewahrung, Bereinigung, Backup und Import | Offen; Status des vorhandenen Entwurfs: `REVIEW_DRAFT – PRESERVE, NOT APPROVED, NOT CANONICAL, IMPLEMENTATION NOT_STARTED`. Der Entwurf bleibt Reviewgrundlage und wird nicht als Plan- oder Implementierungsfreigabe behandelt. | Nach der Hardwarepriorität neue vollständige Planrevision auf aktuellem `main`; reale Budgets, NVS-/Partitionspfad, R1-Importbedarf und `RunImportAdmission` erneut entscheiden |
| 7 | Issue #28 / #36 / #35 / #106 – Diagnose-, Abnahme-, Commissioning- und Aktor-Integrationsgates | Weiterhin offen mit bestehenden Abhängigkeiten. #28 verweist auf #19; #36 bündelt #28–#35; #35 liefert reale PI-/Safety-Grenzen; #106 bindet den per-Run-Aktorparametersnapshot an diese Werte. | Nach Auflösung der jeweiligen Live-Abhängigkeiten erste reale geschlossene Regelkette und anschließende Hardware-/Commissioning-Nachweise |

## Naechste fachliche Arbeit

PR #110 / Issue #24 und PR #113 / Issue #111 sind auf dem aktuellen `main`
abgeschlossen. Der Release-1-Safety-Core bleibt beim bestätigten KISS- und
fail-closed-Vertrag; Hardware-, reale Aktor-, NVS- und
Inbetriebnahmenachweise sind dadurch nicht vorweggenommen.

Die kürzeste sinnvolle technische Reihenfolge führt zunächst zur realen
Hardwarebasis:

1. #29: aktorfreier ESP32-Bring-up sowie Flash-, Heap-, Stack-, UART- und
   sichere Boot-/Resetbaseline;
2. #90: produktiver ESP-IDF-NVS-Adapter über den bestehenden `IStateStore`;
3. #30: reale Temperatursensoren;
4. #32: Lüfter, Summer und MOSFET-Ausgänge;
5. #33: BTS7960/Peltier mit begrenzten Service-/Bring-up-Prüfungen.

Danach werden #35/#36 und #106 anhand ihrer bestehenden Werte-, Hardware- und
Abnahmeabhängigkeiten für die erste reale geschlossene Regelkette eingeordnet.
Issue #19 wird zeitlich nach hinten verschoben, bleibt offen und fachlich
erhalten. Die im Issue-#32-Text bestehende Abhängigkeit auf #28 und die
nachgelagerte #28-Abhängigkeit auf #19 werden nicht still verändert; falls die
Hardwarekette diese Grenze früher benötigt, ist dafür eine ausdrückliche
Ownerentscheidung zur Abhängigkeit erforderlich.

Issue #16 bleibt Trackingcontainer: #54–#57 und das
`CONFIGURATION_SAFETY_INTEGRATION_GATE` sind abgeschlossen, reale
NVS-/Partitions-/Flash-/Hardwareabnahmen bleiben insbesondere über #90 offen.

## Zulaessige Parallelitaet

- PR #113 / Issue #111 sind als Markdown-only-Governancearbeit abgeschlossen;
  Firmware- und Safety-Semantik bleiben davon unberührt.
- #29, #90, #30, #32 und #33 beginnen jeweils nur über ihr eigenes
  Live-Issue, den geltenden Plan-first-Workflow und die jeweiligen Hardware-/
  Owner-Gates.
- #19 erhält vor einer späteren Umsetzung eine neue vollständige Planrevision
  auf dann aktuellem `main`. Der bestehende Review-Draft ist weder
  freigegeben noch kanonisch.
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
