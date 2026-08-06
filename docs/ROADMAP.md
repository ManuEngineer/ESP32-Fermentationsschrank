# Projekt-Roadmap

Stand: 2026-08-06

Diese Datei ist die einzige aktuelle Status- und Taskuebersicht. Fachliche
Anforderungen, vollstaendige Issue-Inhalte und historische Begruendungen werden
nicht kopiert, sondern verlinkt.

## Aktuelle Arbeit

| Prioritaet | Arbeit | Status | Naechstes Gate |
|---:|---|---|---|
| 1 | PR #95 / Issue #20 – Sensorqualitaet, Filterung und Plausibilitaet | Draft, Umsetzung und gezielte Tests abgeschlossen | vollstaendiges Ownerreview; danach Ownerentscheid zu `Ready for review` und CI |
| 2 | PR #98 – Agentenregeln und Statusquellen konsolidieren | Draft, A9.1/A9.2 abgeschlossen; A9.3 in Umsetzung | Spezifikations-/Statusquellen konsolidieren, kompletter Diffreview |

## Naechste fachliche Arbeit

Issue #21 – Regelsensorauswahl, Ersatzbetrieb und Rueckkehrlogik – folgt erst,
wenn PR #95 beziehungsweise Issue #20 abgeschlossen und der Live-Stand erneut
geprueft ist. Vor jeder Umsetzung ist ein eigener Plan-first-Draft-PR
erforderlich.

## Zulaessige Parallelitaet

- PR #98 darf als reine Governance-/Dokumentationsarbeit parallel zu PR #95
  fortgesetzt werden.
- Keine weitere Produktionsimplementierung darf den Scope von #20 oder die
  darauf aufbauende #21-Planung parallel vorwegnehmen.
- Hardware-, Bibliotheks- und Adapterarbeit beginnt nur ueber das zugehoerige
  Live-Issue und einen freigegebenen Plan.

## Blocker und spaetere Gates

- Reale Hardware-, GPIO-, Display-/Touch-, Sensor-, Aktor- und
  Inbetriebnahmenachweise stehen in `OPEN_POINTS.md`.
- Thermische Parameter und Releaseabnahme bleiben bis zu den realen Messungen
  und Belastungstests blockiert.
- Issue #89 (WLAN-Onboarding-Evaluation) und Issue #90
  (ESP-IDF-NVS-Adapter) benoetigen vor Beginn einen eigenen Live-Abgleich und
  freigegebenen Plan.
- OTA ist fuer ein spaeteres Release vorgesehen, aber kein Release-1-Scope.

## Zuletzt abgeschlossene groessere Grundlagen

- PR #79: ESP-IDF 6.0.2 als einziger ESP32-Produktionspfad;
- PR #84: Laufpersistenz und Kontrollpunkte;
- PR #88: Espressif-first-Synchronisierung des Adopt-or-build-Audits;
- PR #92: Markdown-only-Aenderungen ohne vollstaendige Firmware-CI;
- PR #96: kompakter Session-Handover-Vertrag.

## Pflege

Aktualisierung ist erforderlich:

- zu Beginn jedes neuen Pull Requests;
- nach jedem Merge;
- bei materieller Reihenfolgeaenderung;
- bei neuem Blocker oder Ownerentscheid.

Details bleiben in Live-Issue, Pull Request, freigegebenem Plan, ADR oder
Fachvertrag.
