# Projekt-Roadmap

Stand: 2026-08-15

Diese Datei ist die einzige aktuelle Status- und Taskuebersicht. Fachliche
Anforderungen, vollstaendige Issue-Inhalte und historische Begruendungen werden
nicht kopiert, sondern verlinkt.

## Aktuelle Arbeit

| Prioritaet | Arbeit | Status | Naechstes Gate |
|---:|---|---|---|
| 1 | Issue #24 – Fehlerklassen, Verriegelung, SAFE_BOOT und Fehlerinjektion (Branch `agent/issue-24-fehlerklassen-safe-boot-plan`) | R3-Implementierung und Ownerreview-Korrekturrunde 4 (getrennte SAFE_BOOT-Pflichtrollen sowie bounded S3-008-/Y4-008-Identitaeten) umgesetzt; PR #107 bleibt Draft | Ownerreview der neuen SHA gegen Plan `48f343c` |
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

Der Branch enthaelt die gegen die freigegebene R3 reconcilierten
Safety-/Recovery-Vertraege. Der eingefrorene R2-Stand war keine normative
Quelle; insbesondere wurden der fachliche Resetport, die R2-Capability und
der alte Safetyrecord entfernt beziehungsweise auf den R3-Vertrag migriert.
Die gezielten nativen Konsumententests sind fuer den korrigierten
Implementierungsstand PASS: Issue #24 31/31, #15 43/43, #23 44/44,
Application/Persistenz 113/113 sowie #56/#57 37/37 und 40/40. Der PlatformIO-
Treiber blieb wegen `Platform Manager: Installing native` BLOCKED; der
vollstaendige native Lauf, Firmwarebuilds, Remote-CI und Hardware bleiben
Draft-/Owner-Gates. Die neue R4-Korrekturrunde bindet Y4-006-Recovery,
Restart-Evidence, O2-002-Rollen, #56/#57-R3-Domains und typed Resetchecks
gegen aktuelle Fault-/Recordrevisionen. Die positive Reset-Safetyentscheidung
wird nicht mehr als frei konstruierbares Produktionsobjekt uebergeben; der
positive SAFE_BOOT-Aktornachweis kommt ueber den privaten Planner-/Sink-Handoff.
Y4-006 persistiert seine bounded Fehlerart und journalisiert
Marker-Recoveryentscheidungen. SAFE_BOOT aggregiert die getrennten Pflichtrollen
CabinetAir und Cooling; S3-008 und Y4-008 verwenden ihre stabilen bounded
Producerdomänen trotz wechselnder Diagnosebeobachtungen.

Die native #56/#57-Bridge ist in der korrigierten R3 als realer
`FermentationApplication`-Application-/Orchestrierungspfad mit einer zentralen
Safetyinstanz und dem bestehenden #23-Planner-/Sinkgate festgelegt. Der
aktuelle Root bleibt ein Skeleton; noch nicht vorhandene ESP-IDF-/NVS-/Hardware-
Adapter werden nicht vorgezogen. #29/#32/#33, #35, #106 und #19 werden nicht
vorgezogen. #35 bleibt das spaetere Gate fuer aktive thermische S3-004-
Recovery.

Die kanonische fachliche Reihenfolge bleibt: Issue #23, danach Issue #24
(Fehlerklassen und SAFE_BOOT) und anschliessend Issue #19 (Journale,
Aufbewahrung, Bereinigung, Backup und Import). Issue #24 befindet sich jetzt
mit der implementierten R3 im eigenen Owner-Gate. Issue #106
(Aktorplaner Per-Run-
Parameter-Snapshot und Recovery-Bindung) bleibt als separates spaeteres
Integrationsgate offen und ist nicht die naechste Arbeit; seine vollstaendige
Abnahme bleibt unter anderem von #35 abhaengig. Issue #35 bleibt
`TBD_COMMISSIONING` und Eigentum der produktiven Werte und Grenzen. Fuer #24
ist nur noch die Ownerreview der exakten neuen R3-SHA offen; der freigegebene
Plan bleibt `48f343ceb49d5a80239702241ae1fbf7d4ebfcd2`.

## Zulaessige Parallelitaet

- Der neue #24-PR bleibt Draft, bis der Owner ihn selbst auf `Ready for review`
  setzt; bis dahin startet keine Firmware-CI.
- Issue #106 wird in diesem PR weder bearbeitet noch geschlossen und definiert
  keine neue Safety-/Fehlerklassifikation.
- Issue #29 und #90 werden in diesem PR weder implementiert noch als pauschale
  Abhaengigkeit vorweggenommen; konkrete ESP-IDF-/Hardwareadapter bleiben ihre
  jeweiligen Gates.
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
