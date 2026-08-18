# Projekt-Roadmap

Stand: 2026-08-18

Diese Datei ist die einzige aktuelle Status- und Taskuebersicht. Fachliche
Anforderungen, vollstaendige Issue-Inhalte und historische Begruendungen werden
nicht kopiert, sondern verlinkt.

## Aktuelle Arbeit

| Prioritaet | Arbeit | Status | Naechstes Gate |
|---:|---|---|---|
| 1 | Draft-PR #113 / Issue #111 – Codex Plan Mode in Plan-first-Workflow integrieren | Nach Merge von PR #110 auf `main` retargetet; Markdown-/Governance-only mit genau `AGENTS.md`, `docs/AGENT_WORKFLOW.md` und dieser Roadmap. Der Stacking-Konflikt wird auf dem neuen `main`-Stand aufgeloest; Firmwarecode und Safety-Vertraege bleiben unberuehrt. | Vollstaendiges Owner-Review des bereinigten 3-Dateien-Diffs; danach Ownerentscheidung ueber Ready/Merge gemaess Markdown-only-Workflow |
| 2 | Issue #19 – Journale, Aufbewahrung, Bereinigung, Backup und Import | Naechste fachliche Arbeit nach dem Abschluss von #24. Issue #16 bleibt als Trackingcontainer offen; sein direkter Konfigurationskern #54–#57 und das #24-Safety-Integrationsgate sind abgeschlossen, reale NVS-/Partitions-/Hardwaregates bleiben separat sichtbar. | Vor Planbeginn Live-Abgleich von #19 und #16; danach eigener Plan-Mode-/Plan-SHA-/Owner-Gate-PR fuer #19 |
| 3 | Epic-E1-Abschlussnachfuehrung – `CommandDecision`-Ressourcengate aus PR #53 | PR #103 ist gemergt; Live-Issue #29 und `OPEN_POINTS.md` halten das reale ESP32-Ressourcengate sichtbar, bis reale Hardware-Messung vorliegt. | Owner entscheidet ueber Abschluss von Epic #3 erst nach dem realen Ressourcennachweis |

## Naechste fachliche Arbeit

Issue #24 – Release-1 Safety Core – ist mit PR #110 ueber Merge-Commit
`a802b1f54435258e1e96cf92a9af16e72040a00d` nach `main` integriert. Der
vollstaendige GitHub-CI-Lauf am finalen Implementierungs-HEAD `39f0898a` hat
Native Build/Tests, clang-tidy, Architektur-/Quality-/Secret-Gates, beide
ESP-IDF-6.0.2-Profile und ESP-IDF-Static-Analysis bestanden. Hardware- und
Inbetriebnahmenachweise bleiben wie vorgesehen spaetere E5-/Open-Point-Gates.

Die fachliche Reihenfolge ist damit nach #23 und #24 bei Issue #19 angekommen:
Journale, Aufbewahrung, Bereinigung, Backup und Import. #19 beginnt erst nach
seinem eigenen Live-Abgleich und Plan-/Owner-Gate. Die offene #16-Abhaengigkeit
wird dabei nicht pauschal als erledigt oder blockierend interpretiert: #16 ist
ein Trackingcontainer mit abgeschlossenem direktem Konfigurationskern und noch
offenen realen NVS-/Hardware-Abnahmen. Issue #106 bleibt als separates
produktives Aktor-Integrationsgate unveraendert offen und wird durch #19 nicht
umgangen.

## Zulaessige Parallelitaet

- PR #113 ist ein Markdown-only Governance-PR und wird nach seinem eigenen
  vollstaendigen Owner-Review separat abgeschlossen; er veraendert keine
  Firmware- oder Safety-Semantik.
- Neue fachliche Implementierung fuer #19 beginnt erst nach eigenem Branch,
  Draft-PR, kanonischem Markdown-Plan und Freigabe der exakten Plan-SHA.
- Hardware-, Bibliotheks- und Adapterarbeit beginnt nur ueber das zugehoerige
  Live-Issue und einen freigegebenen Plan.
- Unabhaengige Recherche darf keine Umsetzung, Produktauswahl oder
  Hardwarefreigabe vorwegnehmen.

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

## Pflege

Aktualisierung ist erforderlich:

- zu Beginn jedes neuen Pull Requests;
- nach jedem Merge;
- bei materieller Reihenfolgeaenderung;
- bei neuem Blocker oder Ownerentscheid.

Details bleiben in Live-Issue, Pull Request, freigegebenem Plan, ADR oder
Fachvertrag.
