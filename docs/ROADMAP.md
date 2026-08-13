# Projekt-Roadmap

Stand: 2026-08-13

Diese Datei ist die einzige aktuelle Status- und Taskuebersicht. Fachliche
Anforderungen, vollstaendige Issue-Inhalte und historische Begruendungen werden
nicht kopiert, sondern verlinkt.

## Aktuelle Arbeit

| Prioritaet | Arbeit | Status | Naechstes Gate |
|---:|---|---|---|
| 1 | Draft-PR #104 / Issue #22 – Zeitproportionale PI-Regelung und Luftbegrenzung | Planrevision 6 (`6b801d30bfb34e48350ea3c29caa22dfea5f7320`) freigegeben; FR1–FR14 in Slice 1–4 sowie die Owner-Finalreview-Restkorrekturen F1–F5 umgesetzt, einschliesslich eigener PI-Saettigungssperre vor `deltaI`, direkter Fault-/Cooling- und Grace-Warnorakel sowie synchronisierter Fachdokumentation; zehn gezielte Pflichtfilter mit 367/367 nativen Testfaellen PASS; clang-format, native Compile-Database, workflow-genauer clang-tidy, Architektur-, Secret-, Quality-, Artefaktabdeckungs-Gates und `git diff --check` PASS; Base `main @ 10ff98eca4d6f64cc453571d66d4c3b18729b18e` | Vollstaendiger Owner-Review des gesamten PR-Diffs von `main` bis zum finalen HEAD; PR bleibt Draft |
| 2 | Epic-E1-Abschlussnachfuehrung – `CommandDecision`-Ressourcengate aus PR #53 | PR #103 ist gemergt (Live-Issue #29 als reale ESP32-Nachverfolgung ergaenzt, `OPEN_POINTS.md` kanonisch synchronisiert); das reale Ressourcen-Gate bleibt ueber #29/`OPEN_POINTS.md` offen sichtbar, bis reale Hardware-Messung vorliegt | Owner entscheidet ueber Abschluss von Epic #3 als `completed` |

## Naechste fachliche Arbeit

Issue #18 – Wiederanlauf und temperaturgewichteter Fortschritt – ist mit PR
#102 ueber Merge-Commit `10ff98e` nach `main` integriert und wurde am
2026-08-12 als `completed` geschlossen. Die Reaktivierungs-, Persistenz-,
Fail-Closed- und gewichteten Fortschrittsvertraege sowie die dokumentierten
Nachweise stehen im gemergten Plan und den verlinkten Fachvertraegen.

Die vorgesehene fachliche Reihenfolge nach dem Abschluss von #18 ist:
Issue #22 (zeitproportionale PI-Regelung und Luftbegrenzung), danach Issue #23
(Aktorplaner), Issue #24 (Fehlerklassen und SAFE_BOOT) und anschliessend Issue
#19 (Journale, Aufbewahrung, Bereinigung, Backup und Import). Die jeweiligen
Arbeiten beginnen erst nach ihrem eigenen Plan-/Owner-Gate; #22 ist jetzt die
aktuelle Umsetzungsarbeit. Die vollstaendige eigenstaendige Planrevision 6
liegt im Draft-PR #104 unter `docs/tasks/issue-22-pi-control-air-limits-plan.md`
und wurde mit `6b801d30bfb34e48350ea3c29caa22dfea5f7320` freigegeben.

## Zulaessige Parallelitaet

- Der #22-PR bleibt Draft, bis der Owner ihn selbst auf `Ready for review` setzt;
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
- PR #102 / Issue #18: Wiederanlauf und temperaturgewichteter Fortschritt;

## Pflege

Aktualisierung ist erforderlich:

- zu Beginn jedes neuen Pull Requests;
- nach jedem Merge;
- bei materieller Reihenfolgeaenderung;
- bei neuem Blocker oder Ownerentscheid.

Details bleiben in Live-Issue, Pull Request, freigegebenem Plan, ADR oder
Fachvertrag.
