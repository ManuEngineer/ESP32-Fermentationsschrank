# AGENTS.md

Diese Datei gilt fuer das gesamte Repository. Untergeordnete `AGENTS.md`
ergaenzen sie nur um Regeln ihres Verzeichnisbaums.

## Projektauftrag

Entwickelt wird eine sichere, lokal bedienbare ESP32-Firmware fuer einen
Fermentationsschrank. Regelung und Safety duerfen Netzwerk, Web oder Anzeige
nicht benoetigen. Bei unbekanntem oder fehlerhaftem Zustand gilt fail-closed.

Auftraege an Agenten werden immer als Markdown-Datei bereitgestellt.

## Aktueller Stand und Quellen

Vor Planung, Umsetzung, Fortsetzung oder Review sind Repository, Branch,
aktueller `HEAD`, Live-Issue/PR, `docs/ROADMAP.md`, freigegebener Plan-Commit
und neuester `SESSION HANDOVER`, soweit vorhanden, zu pruefen.

Bestehender Code, Modelle, Tests, akzeptierte ADRs und kanonische Dokumente
werden vor neuen Modellen oder Vertraegen verwendet. Parallelvertraege und
stille Neuerfindungen sind unzulaessig.

Die Dokumentationsprioritaet steht ausschliesslich in
`docs/SPECIFICATION_REVIEW.md`, das ADR-Register in `docs/DECISIONS.md`.
Widersprueche, fehlende Entscheidungen und materielle Abweichungen werden dem
Owner vorgelegt und nicht still aufgeloest.

## Engineering, Safety und Architektur

`docs/ENGINEERING_PRINCIPLES.md` ist verbindlich: Repository-first,
inkrementelle Kontextaktualisierung, SOLID, DRY, KISS und Espressif-first.
Diese Grundsaetze rechtfertigen weder vorsorgliche Ueberabstraktion noch eine
Vereinfachung von Safety, Security, Recovery, Testbarkeit oder Vertraegen.

Keine Aktorfreigabe darf bei Boot, Reset, Fehler, unbekanntem Zustand,
unbestaetigter Hardware oder offenem Safety-Gate eingefuehrt oder vorausgesetzt
werden. GPIOs, Pegel, Controller, Verdrahtung, Grenzwerte und Testergebnisse
werden nicht geraten. `TBD_HARDWARE`, `TBD_COMMISSIONING` und
`TBD_IMPLEMENTATION_BUDGET` sind nie gueltige produktive Laufzeitwerte.

Die Release-1-Abgrenzung steht in `docs/SPECIFICATION_REVIEW.md`.
Zukunftsfunktionen werden nicht teilweise vorgebaut. OTA und automatischer
Firmwaredownload sind fuer spaeter vorgesehen, aber nicht Release 1; jetzt
werden dafuer keine Bibliotheken, Slots oder Speicherreserven eingebaut.

ADR-013 ist fuer die Modularchitektur verbindlich:

- `device_platform`: portable, anwendungsneutrale Ports und Dienste;
- `device_platform_esp_idf`: konkrete ESP-IDF-Adapter;
- `fermentation_app`: Fachlogik nur gegen abstrakte Plattformports;
- `device_platform_test_support`: Testhilfen, nie Produktionsabhaengigkeit.

Bei Arbeiten in diesen Verzeichnissen gilt zusaetzlich die lokale `AGENTS.md`.

## Branch-, Plan- und Owner-Gates

Es wird nie direkt auf `main` gearbeitet. Grundsaetzlich gilt ein
zusammenhaengender Scope und ein Issue pro Branch und Pull Request.

Nicht triviale Arbeit folgt `docs/AGENT_WORKFLOW.md`:

1. Branch und Draft-PR erstellen.
2. Die Planung bevorzugt im nativen Planungsmodus des verwendeten Agenten
   durchfuehren; bei Codex ist Plan Mode der bevorzugte Planungsmodus.
3. Das Ergebnis als vollstaendigen versionierten Markdown-Plan unter
   `docs/tasks/` committen und anhalten.
4. Erst nach Freigabe des exakten Plan-Commits umsetzen.
5. Bei materieller Abweichung Plan aktualisieren und erneut freigeben lassen.

Der ausführende Agent ist der Builder; der formale Full Review erfolgt
grundsätzlich unabhängig vom Builder. Nach dem Builder-Self-Check hält der
Agent für den Independent Review an. Nach lokal begrenzten Korrekturen gilt
Fix Verification statt eines erneuten Full Review, solange keine materielle
Änderung vorliegt. Details stehen ausschließlich in `docs/AGENT_WORKFLOW.md`.

Ein nativer Planungsmodus ist nur das Arbeitsmittel zur Planerstellung. Er
ersetzt weder den versionierten Markdown-Plan noch die Ownerfreigabe der exakten
Plan-SHA. Ist kein nativer Planungsmodus verfuegbar, gilt derselbe Planvertrag
ohne tool-spezifischen Ersatzprozess.

Der Agent setzt keinen PR selbst auf `Ready for review` oder Draft, mergt nicht,
aktiviert kein Auto-Merge, verwendet keinen Force-Push, loescht keinen Branch,
schliesst kein Issue eigenmaechtig und fuehrt `/clear` nicht selbst aus.

`docs/ROADMAP.md` wird zu Beginn jedes neuen PR, nach jedem Merge und bei
materieller Reihenfolgeaenderung aktualisiert; Anforderungen werden dort nicht
kopiert.

## Tests, CI und Review

In Draft werden nur gezielte lokale Tests des geaenderten Bereichs und bei
gemeinsamen Vertraegen die direkt betroffenen Konsumententests ausgefuehrt.

Ein Review prueft den vollstaendigen aktuellen Diff gegen Plan, Anforderungen,
Architektur, Tests, Dokumentation sowie SOLID, DRY und KISS; es endet nicht bei
den auffaelligsten Befunden.

Die verbindliche Reihenfolge vor einem Ready-Wechsel lautet:

```text
Independent Review abgeschlossen
-> OPEN_BLOCKERS=0
-> Owner autorisiert finalen lokalen Pre-Ready-Lauf
-> PRE_READY_LOCAL_GATES=PASS auf exakt finalem HEAD
-> Owner setzt Ready for review
-> GitHub-CI PASS
-> Merge-Gate
```

Ein vollstaendiger lokaler Pre-Ready-Lauf erfolgt nur nach abgeschlossenem
Independent Full Review mit `OPEN_BLOCKERS=0`, auf dem finalen `HEAD` und nach
ausdruecklicher Owner-Anweisung; `FOLLOW-UP` und `NO-ACTION` blockieren ihn
nicht. `OPEN_BLOCKERS=0` allein erlaubt weder den Lauf noch den Wechsel auf
`Ready for review`.

GitHub-Firmware-CI laeuft nicht im Draft. Erst nach
`PRE_READY_LOCAL_GATES=PASS` setzt der Owner auf `Ready for review`; spaetere
semantische Pushes eines Nicht-Draft-PR starten CI erneut. Markdown-only loest
keine Firmware-CI aus. Bei semantischen Aenderungen gilt die im Workflow
definierte Fix-Verification-/Materialitaetsregel; ein neuer Full Review ist
nur bei materieller Aenderung oder breitem neuem Diff erforderlich.

Nach CI-Fehler legt der Agent Befund und Korrekturplan vor; nur der Owner
entscheidet ueber eine Rueckstufung auf Draft und den erneuten
`Ready for review`-Wechsel.

Zeitpunkt, Voraussetzungen, Profile, Toolvertraege und Ergebnisstatus stehen
in `docs/CI_AND_QUALITY_GATES.md`. Die vollstaendigen ausfuehrbaren
Gatebefehle und die clang-tidy-Dateiliste stehen ausschliesslich im dort
referenzierten `scripts/run_pre_ready_gates.sh`. Nicht ausgefuehrte Tests sind
nicht bestanden.

## Session-Handover

Vor Sessionende, Kontextreset oder Agentenwechsel wird bei offenem PR genau ein
aktueller `SESSION HANDOVER`-Kommentar erstellt und danach angehalten. Er nennt
kompakt `HEAD`, freigegebenen Plan-Commit, erledigte Bereiche, Tests, offene
Befunde und den naechsten Schritt.

Der neueste Handover ersetzt fruehere. Diffs, Plaene und Logs werden
referenziert, nicht kopiert. Ist kein PR-Kommentar moeglich, wird der Text im
Chat als nicht veroeffentlicht ausgegeben.

## Bedingte Lesematrix

| Aufgabe | Zusaetzlich zu lesen |
|---|---|
| Jede Aufgabe | Markdown-Auftrag, Live-Issue/PR, Roadmap, lokale `AGENTS.md` |
| Planung, Umsetzung, Review | Agent-Workflow, Engineering-Grundsaetze; bei Code/Build/Tests CI-Dokument |
| Produkt, Release, Scope | Spezifikationsreview, relevante ADRs |
| Architektur, Module | ADR-Register, relevante ADRs, Modulindex, lokale Regeln |
| Bibliotheken | Engineering-Grundsaetze, Adopt-or-build, Lizenz-/Herstellerquellen |
| Safety, Recovery, Aktoren, Persistenz | betroffene Fachvertraege und Akzeptanztests |
| Hardware, Bring-up | Hardware, Open Points; bei ESP-IDF auch Upgradevertrag |
| UI, Web, Netzwerk, Fachlogik | nur direkt referenzierte Fachdokumente |

Der freigegebene Plan nennt die konkret erforderlichen Fachquellen. Das gesamte
Dokumentationsverzeichnis wird nicht pauschal vollstaendig gelesen.
