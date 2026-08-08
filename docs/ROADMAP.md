# Projekt-Roadmap

Stand: 2026-08-07

Diese Datei ist die einzige aktuelle Status- und Taskuebersicht. Fachliche
Anforderungen, vollstaendige Issue-Inhalte und historische Begruendungen werden
nicht kopiert, sondern verlinkt.

## Aktuelle Arbeit

| Prioritaet | Arbeit | Status | Naechstes Gate |
|---:|---|---|---|
| 1 | Draft-PR #102 / Issue #18 – Wiederanlauf und temperaturgewichteter Fortschritt | Planung laeuft auf Basis von `main` (`17ab3f5`, inkl. PR #103); Revision 4 in Arbeit, Freigabe steht aus | Owner prueft und gibt exakten Plan-Commit frei |
| 2 | Epic-E1-Abschlussnachfuehrung – `CommandDecision`-Ressourcengate aus PR #53 | PR #103 ist gemergt (Live-Issue #29 als reale ESP32-Nachverfolgung ergaenzt, `OPEN_POINTS.md` kanonisch synchronisiert); das reale Ressourcen-Gate bleibt ueber #29/`OPEN_POINTS.md` offen sichtbar, bis reale Hardware-Messung vorliegt | Owner entscheidet ueber Abschluss von Epic #3 als `completed` |

## Naechste fachliche Arbeit

Issue #21 – Regelsensorauswahl, Ersatzbetrieb und Rueckkehrlogik – ist ueber
PR #99 nach `main` gemergt (`082fb3f`). Als naechste fachliche Arbeit hat der
Owner Issue #18 – Wiederanlauf und temperaturgewichteter Fortschritt
beauftragt; das ersetzt die zuvor genannte Reihenfolge "#22 nach #21".
Issue #22 (Epic E3, zeitproportionale PI-Regelung) bleibt die naechste
fachliche Arbeit innerhalb Epic E3 und ist von dieser Umstellung nicht
inhaltlich betroffen, wird aber nicht vor #18 begonnen.

Issue #18 baut auf der in `docs/RUN_PERSISTENCE.md`, Abschnitt "Uebergabe an
ein spaeteres Vorhaben: Regelsensorauswahl bei Reaktivierung", dokumentierten
Grundlage aus #21 auf: der persistierte und laufzeitseitige
Sensorselektionszustand sowie eine reine Empfehlungsfunktion sind vorhanden,
die tatsaechliche Reaktivierung eines geladenen aktiven Laufs (inklusive
Freigabe der Regelung) ist noch offen. Details, Abhaengigkeitsstand und
offene Ownerentscheidungen stehen im Plan unter `docs/tasks/`.

## Zulaessige Parallelitaet

- Der PR bleibt Draft, bis der Owner ihn selbst auf `Ready for review` setzt;
  bis dahin startet keine erneute vollstaendige Remote-CI.
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

- PR #98: Agentenregeln und Statusquellen konsolidiert und nach `main` gemergt;
- PR #79: ESP-IDF 6.0.2 als einziger ESP32-Produktionspfad;
- PR #84: Laufpersistenz und Kontrollpunkte;
- PR #88: Espressif-first-Synchronisierung des Adopt-or-build-Audits;
- PR #92: Markdown-only-Aenderungen ohne vollstaendige Firmware-CI;
- PR #95 / Issue #20: Sensorqualitaet, Filterung und Plausibilitaet;
- PR #96: kompakter Session-Handover-Vertrag;
- PR #99 / Issue #21: Regelsensorauswahl, Ersatzbetrieb und Rueckkehrlogik.

## Pflege

Aktualisierung ist erforderlich:

- zu Beginn jedes neuen Pull Requests;
- nach jedem Merge;
- bei materieller Reihenfolgeaenderung;
- bei neuem Blocker oder Ownerentscheid.

Details bleiben in Live-Issue, Pull Request, freigegebenem Plan, ADR oder
Fachvertrag.
