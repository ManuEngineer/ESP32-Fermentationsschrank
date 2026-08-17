# Projekt-Roadmap

Stand: 2026-08-17

Diese Datei ist die einzige aktuelle Status- und Taskuebersicht. Fachliche
Anforderungen, vollstaendige Issue-Inhalte und historische Begruendungen werden
nicht kopiert, sondern verlinkt.

## Aktuelle Arbeit

| Prioritaet | Arbeit | Status | Naechstes Gate |
|---:|---|---|---|
| 1 | Issue #23 – Aktorplaner, Mindestzeiten, Totzeit und Luefterlogik | PR #105 ist nach `main` gemergt; #23 ist auf dem aktuellen Base abgeschlossen. Das separate produktive Integrationsgate #106 bleibt als eigener Live-Status unveraendert sichtbar. | Issue #24 erhält einen eigenen Plan-/Owner-Gate-PR |
| 2 | Issue #24 – Fehlerklassen, Verriegelung, SAFE_BOOT und Fehlerinjektion | PR #110 ist Draft; die Owner-freigegebene Release-1-KISS-Planrevision `docs/tasks/issue-24-safety-core-replan.md` ist `97be3ddf53297768a277a76c02aa0251b7cd9943`; die Implementierung des fail-closed SafetyCore-, ResetCause-, `NoActiveRun`- und zentralen Planner-Gate-Pfads laeuft mit gezielten Native-Gates | Vollstaendiges Owner-Review des Implementierungs-HEADs; danach entscheidet der Owner ueber `Ready for review` |
| 3 | Epic-E1-Abschlussnachfuehrung – `CommandDecision`-Ressourcengate aus PR #53 | PR #103 ist gemergt (Live-Issue #29 als reale ESP32-Nachverfolgung ergaenzt, `OPEN_POINTS.md` kanonisch synchronisiert); das reale Ressourcen-Gate bleibt ueber #29/`OPEN_POINTS.md` offen sichtbar, bis reale Hardware-Messung vorliegt | Owner entscheidet ueber Abschluss von Epic #3 als `completed` |

## Naechste fachliche Arbeit

Issue #22 – Zeitproportionale PI-Regelung und Luftbegrenzung – ist mit PR
#104 ueber Merge-Commit `2986dca` nach `main` integriert und wurde als
`completed` geschlossen. Der implementierte #22-Fachkern (`ControlRequest`,
`ControlRequestContext`, `ControlSensorRole`, PI-/Luftbegrenzungslogik) ist
kanonisch und wird von Issue #23 ausschliesslich wiederverwendet.

Die vorgesehene fachliche Reihenfolge nach dem Abschluss von #22 ist:
Issue #23 (Aktorplaner, Mindestzeiten, Totzeit und Luefterlogik), danach
Issue #24 (Fehlerklassen und SAFE_BOOT) und anschliessend Issue #19 (Journale,
Aufbewahrung, Bereinigung, Backup und Import). #23 ist mit PR #105 auf dem
aktuellen `main` abgeschlossen; die neue #24-Arbeit beginnt ausschließlich
nach ihrem eigenen Plan-/Owner-Gate. Issue #106 bleibt als separates
produktives Integrationsgate unverändert offen.

## Zulaessige Parallelitaet

- PR #110 bleibt Draft auch nach der Planfreigabe; der Implementierungs-HEAD
  benoetigt ein vollstaendiges Owner-Review, bevor der Owner ueber `Ready for
  review` und Remote-CI entscheidet.
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
