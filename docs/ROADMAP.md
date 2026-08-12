# Projekt-Roadmap

Stand: 2026-08-12

Diese Datei ist die einzige aktuelle Status- und Taskuebersicht. Fachliche
Anforderungen, vollstaendige Issue-Inhalte und historische Begruendungen werden
nicht kopiert, sondern verlinkt.

## Aktuelle Arbeit

| Prioritaet | Arbeit | Status | Naechstes Gate |
|---:|---|---|---|
| 1 | Draft-PR #102 / Issue #18 – Wiederanlauf und temperaturgewichteter Fortschritt | Reaktivierung und Korrekturstand 7B-7D sowie C9/C10/C11 sind in PR #102 umgesetzt; PR ist noch nicht nach `main` gemergt; erneutes vollstaendiges Owner-Abschlussreview ist offen | Owner-Abschlussreview, danach Ownerentscheidung zu `Ready for review` |
| 2 | Epic-E1-Abschlussnachfuehrung – `CommandDecision`-Ressourcengate aus PR #53 | PR #103 ist gemergt (Live-Issue #29 als reale ESP32-Nachverfolgung ergaenzt, `OPEN_POINTS.md` kanonisch synchronisiert); das reale Ressourcen-Gate bleibt ueber #29/`OPEN_POINTS.md` offen sichtbar, bis reale Hardware-Messung vorliegt | Owner entscheidet ueber Abschluss von Epic #3 als `completed` |

## Naechste fachliche Arbeit

Issue #21 – Regelsensorauswahl, Ersatzbetrieb und Rueckkehrlogik – ist ueber
PR #99 nach `main` gemergt (`082fb3f`). Als naechste fachliche Arbeit hat der
Owner Issue #18 – Wiederanlauf und temperaturgewichteter Fortschritt
beauftragt; das ersetzt die zuvor genannte Reihenfolge "#22 nach #21".
Issue #22 (Epic E3, zeitproportionale PI-Regelung) bleibt die naechste
fachliche Arbeit innerhalb Epic E3 und ist von dieser Umstellung nicht
inhaltlich betroffen, wird aber nicht vor #18 begonnen.

Issue #18 baut auf der in `docs/RUN_PERSISTENCE.md`, Abschnitt
"Recovery-API und Regelsensorauswahl bei Reaktivierung", dokumentierten
Grundlage aus #21 auf. Die tatsaechliche Reaktivierung eines geladenen aktiven
Laufs (inklusive Freigabe der Regelung) ist in PR #102 umgesetzt, aber noch
nicht nach `main` gemergt. Details und Abhaengigkeitsstand stehen im Plan
unter `docs/tasks/`; die Korrekturschnitte 6-8A, der in Revision 14 definierte
Fault-Restore-/Fail-Closed-Fallback-/Schema-Korrekturblock 7B-7D, die
geplanten Commits 9-12 sowie die vom Abschlussreview verlangten
C9/C10/C11-Korrekturen sind umgesetzt. Das naechste Gate ist das erneute
vollstaendige Owner-Abschlussreview; danach entscheidet der Owner ueber
`Ready for review`.

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
