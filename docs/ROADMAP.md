# Projekt-Roadmap

Stand: 2026-08-16

Diese Datei ist die einzige aktuelle Status- und Taskuebersicht. Fachliche
Anforderungen, vollstaendige Issue-Inhalte und historische Begruendungen werden
nicht kopiert, sondern verlinkt.

## Aktuelle Arbeit

| Prioritaet | Arbeit | Status | Naechstes Gate |
|---:|---|---|---|
| 1 | Draft-PR #109 (Branch `agent/issue-24-safety-core-replan`) / Issue #24 – Safety-Core-Planrevision von aktuellem `origin/main` | vollständige End-to-End-Planrevision, ausschließlich Plan-/Roadmap-/Metadatenarbeit, Implementation `NOT_STARTED`; Basis `main @ b8eae5f4da5f2666b5a9bda333d115254c4db5b2` | Owner-Review und Freigabe des exakten neuen Plan-SHA |
| 2 | Issue #19 – Journale, Aufbewahrung, Bereinigung, Backup und Import | folgt nach der bestehenden kanonischen Reihenfolge auf Issue #24; nicht Bestandteil von PR #109 | eigener Live-Abgleich, Plan und Owner-Gate |
| 3 | Epic-E1-Abschlussnachfuehrung – `CommandDecision`-Ressourcengate aus PR #53 | PR #103 ist gemergt; das reale Ressourcen-Gate bleibt ueber #29/`OPEN_POINTS.md` offen sichtbar, bis reale Hardware-Messung vorliegt | Owner entscheidet ueber Abschluss von Epic #3 als `completed` |

## Naechste fachliche Arbeit

Issue #22 – Zeitproportionale PI-Regelung und Luftbegrenzung – ist mit PR
#104 ueber Merge-Commit `2986dca` nach `main` integriert und wurde als
`completed` geschlossen. Der implementierte #22-Fachkern (`ControlRequest`,
`ControlRequestContext`, `ControlSensorRole`, PI-/Luftbegrenzungslogik) ist
kanonisch und wird von Issue #23 ausschliesslich wiederverwendet.

Die vorgesehene fachliche Reihenfolge nach dem Abschluss von #22 ist:
Issue #23 (Aktorplaner, Mindestzeiten, Totzeit und Luefterlogik), danach
Issue #24 (Fehlerklassen und SAFE_BOOT) und anschliessend Issue #19 (Journale,
Aufbewahrung, Bereinigung, Backup und Import). #23 ist mit PR #105 über
Merge-Commit `b8eae5f4da5f2666b5a9bda333d115254c4db5b2` in `main` integriert und
damit fachlich abgeschlossen; Issue #106 bleibt als separates produktives
Integrationsgate sichtbar. Issue #24 / Draft-PR #109 ist jetzt die aktuelle
plan-only Owner-Arbeit. Die Arbeiten beginnen jeweils erst nach ihrem eigenen
Plan-/Owner-Gate.

## Zulaessige Parallelitaet

- PR #109 bleibt Draft, bis der Owner ihn selbst auf `Ready for review` setzt;
  bis dahin startet keine Firmware-CI und keine Implementation.
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
- PR #102 / Issue #18: Wiederanlauf und temperaturgewichteter Fortschritt;
- PR #104 / Issue #22: Zeitproportionale PI-Regelung und Luftbegrenzung;
- PR #105 / Issue #23: Aktorplaner, Mindestzeiten, Totzeit und Luefterlogik;
  in `main` integriert; Issue #106 bleibt als separates Integrationsgate.

## Pflege

Aktualisierung ist erforderlich:

- zu Beginn jedes neuen Pull Requests;
- nach jedem Merge;
- bei materieller Reihenfolgeaenderung;
- bei neuem Blocker oder Ownerentscheid.

Details bleiben in Live-Issue, Pull Request, freigegebenem Plan, ADR oder
Fachvertrag.
