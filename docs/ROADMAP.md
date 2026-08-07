# Projekt-Roadmap

Stand: 2026-08-07

Diese Datei ist die einzige aktuelle Status- und Taskuebersicht. Fachliche
Anforderungen, vollstaendige Issue-Inhalte und historische Begruendungen werden
nicht kopiert, sondern verlinkt.

## Aktuelle Arbeit

| Prioritaet | Arbeit | Status | Naechstes Gate |
|---:|---|---|---|
| 1 | Draft-PR #99 / Issue #21 – Regelsensorauswahl, Ersatzbetrieb und Rueckkehrlogik | Plan freigegeben (Revision 7); Commits 1-6 sowie vier separat beauftragte Korrekturrunden nach Ownerreview (Implementierungsreview, Abschlussreview, letzter Abschlussblocker, CI-Korrektur nach fehlgeschlagenem `clang-tidy` in CI-Run #716) umgesetzt; kein offener fachlicher Blocker mehr bekannt | Owner prueft finalen `HEAD` (erneutes Abschlussreview) und entscheidet ueber erneutes `Ready for review` |
| 2 | Epic-E1-Abschlussnachfuehrung – offenes `CommandDecision`-Ressourcengate aus PR #53 | triviale Dokumentationsnachfuehrung; Live-Issue #29 ist als reale ESP32-Nachverfolgung ergaenzt, `OPEN_POINTS.md` wird kanonisch synchronisiert; keine Produktionslogik oder Messung | Owner reviewt und mergt den separaten Markdown-only-PR; danach kann Epic #3 als `completed` geschlossen werden |

## Naechste fachliche Arbeit

Issue #21 – Regelsensorauswahl, Ersatzbetrieb und Rueckkehrlogik – ist im
Draft-PR #99 auf dem Branch `plan/issue-21-sensor-selection-fallback-return`
umgesetzt: der sechsteilige Commit-Schnitt aus dem freigegebenen Plan
(Commit 6 abgeschlossen) sowie vier zusaetzliche, vom Owner separat
beauftragte Korrekturrunden (sieben Reviewbefunde nach
Implementierungsreview; Architekturguard-, Transportvertrags- und
Dokumentationskorrekturen nach Abschlussreview; Korrektur des dabei
aufgedeckten letzten fachlichen Blockers im manuellen `AppliedRamOnly`-Pfad;
CI-Korrektur zweier `clang-tidy`-Befunde, nachdem CI-Run #716 nach dem
ersten `Ready for review` an diesem Schritt gescheitert war) sind
abgeschlossen. Der Draft-PR hat damit keinen bekannten offenen
#21-Blocker mehr. Naechstes Gate ist ein erneutes Abschlussreview durch den
Owner auf dem finalen `HEAD`; ausschliesslich der Owner entscheidet ueber
ein erneutes `Ready for review` und die dadurch neu ausgeloeste vollstaendige
Remote-CI. Als naechste fachliche Planungsarbeit nach #21 ist Issue #22 zu
nennen; P21-M4 bleibt darin nur die zu #22/#23 gehoerende
Abhaengigkeitsaussage (kein eigenes Gate).

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
- PR #96: kompakter Session-Handover-Vertrag.

## Pflege

Aktualisierung ist erforderlich:

- zu Beginn jedes neuen Pull Requests;
- nach jedem Merge;
- bei materieller Reihenfolgeaenderung;
- bei neuem Blocker oder Ownerentscheid.

Details bleiben in Live-Issue, Pull Request, freigegebenem Plan, ADR oder
Fachvertrag.
