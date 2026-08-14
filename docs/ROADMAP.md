# Projekt-Roadmap

Stand: 2026-08-14

Diese Datei ist die einzige aktuelle Status- und Taskuebersicht. Fachliche
Anforderungen, vollstaendige Issue-Inhalte und historische Begruendungen werden
nicht kopiert, sondern verlinkt.

## Aktuelle Arbeit

| Prioritaet | Arbeit | Status | Naechstes Gate |
|---:|---|---|---|
| 1 | Draft-PR #105 (Branch `agent/issue-23-aktorplaner-plan`) / Issue #23 – Aktorplaner, Mindestzeiten, Totzeit und Luefterlogik | Planrevision 8 (`docs/tasks/issue-23-actuator-planner-plan.md`, Commit `3fa28d32ce9a0782edb984f27006467eb8d5f532`) ist ownerfreigegeben; ein zweiter Owner-Review-Zyklus (Befunde R1-R8, Basis HEAD `2002493`) laeuft, Plan-Item 8 bleibt gesperrt, keine Planrevision 9. R1 (BLOCKER, `feedbackEpisodeSubjectSequence` als alleinige Autoritaet in `rejectToIdle()`/`forceStop()`), R2 (BLOCKER, alle 13 Plan-19.2-Faelle mit dedizierten Tests belegt) und R6 (MINOR, direkter Regressionstest fuer die plannerlose `evaluateTemperatureControl()`) sind auf HEAD `9f14b22` behoben; R3 (BLOCKER, planner-gebundene Lifecycle-Boundary-Abdeckung §19.4), R4 (MAJOR, vollstaendige Cross-Sink-Trace §19.3) und R5 (MAJOR, robusterer Anti-Windup-Nachweis) sind offen; `test_actuator_planner` 44/44, Sink-Driver 6/6, Application-/Persistenzpfad 112/112 und `test_temperature_control` 40/40 PASS; Architektur-/Secret-Gates PASS; vollstaendiger Native-Lauf, ESP-IDF, Build-/Hardware-/CI- und Owner-Gesamtlauf NOT_RUN; Issue #106 bleibt unveraendert offen und separates produktives Integrationsgate; Base `main @ 2986dca5736a34171910c9245a3d5f43fa55da06` (Merge von PR #104 / Issue #22) | Owner-Review der R1/R2/R6-Korrekturen fuer Draft-PR #105; danach R3/R4/R5 |
| 2 | Epic-E1-Abschlussnachfuehrung – `CommandDecision`-Ressourcengate aus PR #53 | PR #103 ist gemergt (Live-Issue #29 als reale ESP32-Nachverfolgung ergaenzt, `OPEN_POINTS.md` kanonisch synchronisiert); das reale Ressourcen-Gate bleibt ueber #29/`OPEN_POINTS.md` offen sichtbar, bis reale Hardware-Messung vorliegt | Owner entscheidet ueber Abschluss von Epic #3 als `completed` |

## Naechste fachliche Arbeit

Issue #22 – Zeitproportionale PI-Regelung und Luftbegrenzung – ist mit PR
#104 ueber Merge-Commit `2986dca` nach `main` integriert und wurde als
`completed` geschlossen. Der implementierte #22-Fachkern (`ControlRequest`,
`ControlRequestContext`, `ControlSensorRole`, PI-/Luftbegrenzungslogik) ist
kanonisch und wird von Issue #23 ausschliesslich wiederverwendet.

Die vorgesehene fachliche Reihenfolge nach dem Abschluss von #22 ist:
Issue #23 (Aktorplaner, Mindestzeiten, Totzeit und Luefterlogik), danach
Issue #24 (Fehlerklassen und SAFE_BOOT) und anschliessend Issue #19 (Journale,
Aufbewahrung, Bereinigung, Backup und Import). Die jeweiligen Arbeiten
beginnen erst nach ihrem eigenen Plan-/Owner-Gate; #23 befindet sich jetzt in
Umsetzung. Die vollstaendige eigenstaendige Planrevision 8 liegt im
Draft-PR #105 unter `docs/tasks/issue-23-actuator-planner-plan.md`
(Planrevision 8, Plan-Commit `3fa28d32ce9a0782edb984f27006467eb8d5f532`,
freigegeben). Issue #106 (Aktorplaner Per-Run-Parameter-Snapshot und
Recovery-Bindung) bleibt offen, wurde live praezisiert und ist als
benanntes, blockierendes Integrationsgate vor jeder produktiven
#23-Aktorverdrahtung angelegt.

## Zulaessige Parallelitaet

- Der #23-PR bleibt Draft, bis der Owner ihn selbst auf `Ready for review` setzt;
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
- PR #104 / Issue #22: Zeitproportionale PI-Regelung und Luftbegrenzung;

## Pflege

Aktualisierung ist erforderlich:

- zu Beginn jedes neuen Pull Requests;
- nach jedem Merge;
- bei materieller Reihenfolgeaenderung;
- bei neuem Blocker oder Ownerentscheid.

Details bleiben in Live-Issue, Pull Request, freigegebenem Plan, ADR oder
Fachvertrag.
