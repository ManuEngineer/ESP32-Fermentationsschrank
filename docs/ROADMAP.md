# Projekt-Roadmap

Stand: 2026-08-06

Diese Datei ist die einzige aktuelle Status- und Taskuebersicht. Fachliche
Anforderungen, vollstaendige Issue-Inhalte und historische Begruendungen werden
nicht kopiert, sondern verlinkt.

## Aktuelle Arbeit

| Prioritaet | Arbeit | Status | Naechstes Gate |
|---:|---|---|---|
| 1 | PR #98 – Agentenregeln und Statusquellen konsolidieren | Draft; Umsetzung und Main-Synchronisierung abgeschlossen; drei Befunde aus dem ersten vollständigen Review korrigiert; zweites vollständiges Review ohne neuen Befund; Owner bestätigt Einzelentwickler-Workflow | Owner entfernt die unpassende GitHub-Regel `1 approving review`; danach Ownerwechsel auf `Ready for review` und vollständige GitHub-CI |

## Naechste fachliche Arbeit

Issue #21 – Regelsensorauswahl, Ersatzbetrieb und Rueckkehrlogik – ist die
naechste fachliche Arbeit. Sie beginnt erst nach Abschluss von PR #98 und einem
erneuten Live-Abgleich. Vor jeder Umsetzung ist ein eigener Plan-first-Draft-PR
erforderlich.

## Zulaessige Parallelitaet

- Keine weitere Produktionsimplementierung soll PR #98 und die danach geltenden
  konsolidierten Arbeitsregeln ueberholen.
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
- OTA ist fuer ein spaeteres Release vorgesehen, aber kein Release-1-Scope.

## Zuletzt abgeschlossene groessere Grundlagen

- PR #79: ESP-IDF 6.0.2 als einziger ESP32-Produktionspfad;
- PR #84: Laufpersistenz und Kontrollpunkte;
- PR #88: Espressif-first-Synchronisierung des Adopt-or-build-Audits;
- PR #92: Markdown-only-Aenderungen ohne vollstaendige Firmware-CI;
- PR #95 / Issue #20: Sensorqualitaet, Filterung und Plausibilitaet;
- PR #96: kompakter Session-Handover-Vertrag.

## Pflege

Aktualisierung ist erforderlich:

- zu Beginn jedes neuen Pull Requests;
- nach jedem Merge;
- bei materieller Reihenfolgeaenderung;
- bei neuem Blocker oder Ownerentscheid.

Details bleiben in Live-Issue, Pull Request, freigegebenem Plan, ADR oder
Fachvertrag.
