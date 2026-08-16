# Projekt-Roadmap

Stand: 2026-08-16

Diese Datei ist die einzige aktuelle Status- und Taskuebersicht. Fachliche
Anforderungen, vollstaendige Issue-Inhalte und historische Begruendungen werden
nicht kopiert, sondern verlinkt.

## Aktuelle Arbeit

| Prioritaet | Arbeit | Status | Naechstes Gate |
|---:|---|---|---|
| 1 | Draft-PR #108 (Branch `agent/issue-24-safety-core-clean-restart`) / Issue #24 – Fehlerklassen, Verriegelung, SAFE_BOOT und Fehlerinjektion | Vollstaendiger eigenstaendiger Ersatzplan (`docs/tasks/issue-24-safety-core-clean-restart-plan.md`) uebernommen, gegen den Live-Stand auf `main @ b8eae5f4da5f2666b5a9bda333d115254c4db5b2` verifiziert und committet; die vorherige Planfassung dieses PR ist nicht mehr freigegeben; PR #107 bleibt unangetastete, nicht normative Fehler- und Lernreferenz; Implementation `NOT_STARTED` | Ownerfreigabe der exakten neuen Plan-Commit-SHA (siehe PR-Body/SESSION HANDOVER), danach erneute Liveprüfung vor Umsetzungsbeginn |
| 2 | Epic-E1-Abschlussnachfuehrung – `CommandDecision`-Ressourcengate aus PR #53 | PR #103 ist gemergt (Live-Issue #29 als reale ESP32-Nachverfolgung ergaenzt, `OPEN_POINTS.md` kanonisch synchronisiert); das reale Ressourcen-Gate bleibt ueber #29/`OPEN_POINTS.md` offen sichtbar, bis reale Hardware-Messung vorliegt | Owner entscheidet ueber Abschluss von Epic #3 als `completed` |

## Naechste fachliche Arbeit

Issue #22 (Zeitproportionale PI-Regelung) und Issue #23 (Aktorplaner,
Mindestzeiten, Totzeit und Luefterlogik) sind abgeschlossen und ueber PR #104
bzw. PR #105 nach `main` gemergt. Die vorgesehene fachliche Reihenfolge ist
jetzt: Issue #24 (Fehlerklassen, Verriegelung, SAFE_BOOT und Fehlerinjektion)
als aktuelle fachliche Arbeit in Draft-PR #108, danach Issue #19 (Journale,
Aufbewahrung, Bereinigung, Backup und Import) als naechste fachliche Arbeit.
Issue #24 verwendet einen vollstaendigen eigenstaendigen Ersatzplan unter
`docs/tasks/issue-24-safety-core-clean-restart-plan.md`; die Umsetzung
beginnt erst nach Ownerfreigabe der exakten Plan-Commit-SHA. Issue #106
(Aktorplaner Per-Run-Parameter-Snapshot und Recovery-Bindung) bleibt offen
und ist als benanntes, blockierendes Integrationsgate vor jeder produktiven
#23-Aktorverdrahtung angelegt; seine vollstaendige Abnahme bleibt unter
anderem von #35 abhaengig, das `TBD_COMMISSIONING` und Eigentum der
produktiven Werte und Grenzen bleibt.

## Zulaessige Parallelitaet

- Der #24-PR (#108) bleibt Draft, bis der Owner ihn selbst auf
  `Ready for review` setzt; bis dahin startet keine erneute vollstaendige
  Remote-CI.
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

## Pflege

Aktualisierung ist erforderlich:

- zu Beginn jedes neuen Pull Requests;
- nach jedem Merge;
- bei materieller Reihenfolgeaenderung;
- bei neuem Blocker oder Ownerentscheid.

Details bleiben in Live-Issue, Pull Request, freigegebenem Plan, ADR oder
Fachvertrag.
