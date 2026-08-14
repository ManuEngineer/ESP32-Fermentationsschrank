# Projekt-Roadmap

Stand: 2026-08-14

Diese Datei ist die einzige aktuelle Status- und Taskuebersicht. Fachliche
Anforderungen, vollstaendige Issue-Inhalte und historische Begruendungen werden
nicht kopiert, sondern verlinkt.

## Aktuelle Arbeit

| Prioritaet | Arbeit | Status | Naechstes Gate |
|---:|---|---|---|
| 1 | Issue #24 – Fehlerklassen, Verriegelung, SAFE_BOOT und Fehlerinjektion (Branch `agent/issue-24-fehlerklassen-safe-boot-plan`) | Implementation nach Freigabe des exakten Plan-Commits `3b2befaf7595066cb8fcc0521b32e93212360ba5` in Arbeit; PR #107 bleibt Draft | Vollstaendiger Diff-Review gegen Plan und Ownerentscheidung; danach entscheidet der Owner ueber `Ready for review` |
| 2 | Epic-E1-Abschlussnachfuehrung – `CommandDecision`-Ressourcengate aus PR #53 | PR #103 ist gemergt (Live-Issue #29 als reale ESP32-Nachverfolgung ergaenzt, `OPEN_POINTS.md` kanonisch synchronisiert); das reale Ressourcen-Gate bleibt ueber #29/`OPEN_POINTS.md` offen sichtbar, bis reale Hardware-Messung vorliegt | Owner entscheidet ueber Abschluss von Epic #3 als `completed` |

## Naechste fachliche Arbeit

Issue #22 – Zeitproportionale PI-Regelung und Luftbegrenzung – ist mit PR
#104 ueber Merge-Commit `2986dca` nach `main` integriert und wurde als
`completed` geschlossen. Issue #23 – Aktorplaner, Mindestzeiten, Totzeit und
Luefterlogik – ist mit PR #105 ueber Merge-Commit
`b8eae5f4da5f2666b5a9bda333d115254c4db5b2` integriert und als `completed`
geschlossen. Die bestehenden #22/#23-Vertraege sind kanonisch und werden von
#24 wiederverwendet.

## Issue #24 Implementierungsstatus

Der aktuelle Code-Schnitt enthaelt den zentralen bounded Faultkern, den
persistenten `SafetyStateRecord`, Reset-/Restart-Port und die SAFE_BOOT-
beziehungsweise Recovery-Orchestrierung. Die Projektion in den bestehenden
#15-Commandzustand und das #23-Aktor-Gate bleibt fail-closed; ein normaler
`Allowed`-Pfad kann keinen Safety-Latch ueberschreiben.

Gezielte native Nachweise: `test_issue24_safety` PASS (10/10),
`test_actuator_planner` PASS (44/44), `test_run_commands` PASS (43/43) und
`test_process_state_machine` PASS (35/35). Der vollstaendige native Lauf,
ESP-IDF-Builds, GitHub-CI und Hardware-/Bring-up-Nachweise sind in diesem
Draft NOT_RUN. Die reale Application-Komposition der #56/#57-Producer sowie
der ESP-IDF-Resetadapter bleiben Integrations-/Bring-up-Gates und sind nicht
als abgeschlossen bewiesen.

Die kanonische fachliche Reihenfolge bleibt: Issue #23, danach Issue #24
(Fehlerklassen und SAFE_BOOT) und anschliessend Issue #19 (Journale,
Aufbewahrung, Bereinigung, Backup und Import). Issue #24 beginnt jetzt mit
seinem eigenen Plan-/Owner-Gate. Issue #106 (Aktorplaner Per-Run-
Parameter-Snapshot und Recovery-Bindung) bleibt als separates spaeteres
Integrationsgate offen und ist nicht die naechste Arbeit; seine vollstaendige
Abnahme bleibt unter anderem von #35 abhaengig. Issue #35 bleibt
`TBD_COMMISSIONING` und Eigentum der produktiven Werte und Grenzen.

## Zulaessige Parallelitaet

- Der neue #24-PR bleibt Draft, bis der Owner ihn selbst auf `Ready for review`
  setzt; bis dahin startet keine Firmware-CI.
- Issue #106 wird in diesem PR weder bearbeitet noch geschlossen und definiert
  keine neue Safety-/Fehlerklassifikation.
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
