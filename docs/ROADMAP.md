# Projekt-Roadmap

Stand: 2026-08-07

Diese Datei ist die einzige aktuelle Status- und Taskuebersicht. Fachliche
Anforderungen, vollstaendige Issue-Inhalte und historische Begruendungen werden
nicht kopiert, sondern verlinkt.

## Aktuelle Arbeit

| Prioritaet | Arbeit | Status | Naechstes Gate |
|---:|---|---|---|
| 1 | Draft-PR #99 / Issue #21 – Regelsensorauswahl, Ersatzbetrieb und Rueckkehrlogik | Plan freigegeben (Revision 7); Commits 1-6 sowie drei separat beauftragte Korrekturrunden nach Ownerreview (Implementierungsreview, Abschlussreview, letzter Abschlussblocker) umgesetzt; kein offener fachlicher Blocker mehr bekannt | Owner prueft finalen `HEAD` und entscheidet ueber `Ready for review` |

## Naechste fachliche Arbeit

Issue #21 – Regelsensorauswahl, Ersatzbetrieb und Rueckkehrlogik – ist im
Draft-PR #99 auf dem Branch `plan/issue-21-sensor-selection-fallback-return`
umgesetzt: der sechsteilige Commit-Schnitt aus dem freigegebenen Plan
(Commit 6 abgeschlossen) sowie drei zusaetzliche, vom Owner separat
beauftragte Korrekturrunden (sieben Reviewbefunde nach
Implementierungsreview; Architekturguard-, Transportvertrags- und
Dokumentationskorrekturen nach Abschlussreview; Korrektur des dabei
aufgedeckten letzten fachlichen Blockers im manuellen `AppliedRamOnly`-Pfad)
sind abgeschlossen. Der Draft-PR hat damit keinen bekannten offenen
#21-Blocker mehr. Naechstes Gate ist die finale Owner-Pruefung des
`HEAD`; ausschliesslich der Owner entscheidet ueber `Ready for review` und
die vollstaendige Remote-CI. Als naechste fachliche Planungsarbeit nach #21
ist Issue #22 zu nennen; P21-M4 bleibt darin nur die zu #22/#23 gehoerende
Abhaengigkeitsaussage (kein eigenes Gate).

## Zulaessige Parallelitaet

- Der PR bleibt Draft, bis der Owner ihn selbst auf `Ready for review` setzt;
  bis dahin beginnt keine vollstaendige Remote-CI.
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
