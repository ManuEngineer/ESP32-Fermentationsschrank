# AGENTS.md

Diese Datei gilt fuer das gesamte Repository. Untergeordnete `AGENTS.md`
ergaenzen sie nur um Regeln des jeweiligen Verzeichnisbaums.

## Projektauftrag

Entwickelt wird eine sichere, lokal bedienbare ESP32-Firmware fuer einen
Fermentationsschrank. Netzwerk, Web und Anzeige duerfen fuer Regelung und
Sicherheit nicht erforderlich sein. Bei unbekanntem oder fehlerhaftem Zustand
gilt fail-closed.

Auftraege an Agenten werden immer als Markdown-Datei bereitgestellt.

## Aktueller Stand und Quellen

Vor Planung, Umsetzung, Fortsetzung oder Review sind mindestens Repository,
Branch, aktueller `HEAD`, Live-Issue, vorhandener Pull Request,
`docs/ROADMAP.md` sowie freigegebener Plan-Commit und neuester
`SESSION HANDOVER`, soweit vorhanden, zu pruefen.

Bestehender Code, Modelle, Tests, akzeptierte ADRs und kanonische Dokumente
werden vor neuen Modellen oder Vertraegen verwendet. Parallelvertraege und
stille Neuerfindungen sind unzulaessig.

Die vollstaendige Dokumentationsprioritaet steht ausschliesslich in
`docs/SPECIFICATION_REVIEW.md`. Das zentrale ADR-Register ist
`docs/DECISIONS.md`. Widersprueche, fehlende Entscheidungen und materielle
Abweichungen werden dem Owner vorgelegt und nicht still aufgeloest.

## Engineering-Grundsaetze

`docs/ENGINEERING_PRINCIPLES.md` ist verbindlich. Dies umfasst Repository-first,
inkrementelle Kontextaktualisierung, SOLID, DRY, KISS und Espressif-first.

Die Grundsaetze werden verhaeltnismaessig angewendet. Sie rechtfertigen weder
vorsorgliche Ueberabstraktion noch eine Vereinfachung von Safety, Security,
Recovery, Testbarkeit oder dokumentierten Vertraegen.

## Safety, Hardware und Release-Scope

Kein Agent darf eine Aktorfreigabe bei Boot, Reset, Fehler, unbekanntem Zustand,
unbestaetigter Hardware oder offenem Safety-Gate einfuehren oder voraussetzen.

GPIOs, Pegel, Controller, Verdrahtung, Grenzwerte und Testergebnisse duerfen
nicht geraten oder aus aehnlicher Hardware als bestaetigt uebernommen werden.
`TBD_HARDWARE`, `TBD_COMMISSIONING` und `TBD_IMPLEMENTATION_BUDGET` sind niemals
gueltige produktive Laufzeitwerte.

Die verbindliche Release-1-Abgrenzung steht in
`docs/SPECIFICATION_REVIEW.md`. Zukunftsfunktionen werden nicht teilweise oder
vorsorglich implementiert. OTA und automatischer Firmwaredownload sind fuer ein
spaeteres Release vorgesehen, aber nicht Bestandteil von Release 1; dafuer
werden jetzt keine Bibliotheken, Slots oder Speicherreserven eingebaut.

## Modularchitektur

`docs/ADR-013_REUSABLE_DEVICE_PLATFORM.md` ist verbindlich.

- `device_platform` enthaelt portable, anwendungsneutrale Ports und Dienste.
- `device_platform_esp_idf` implementiert diese Ports mit ESP-IDF.
- `fermentation_app` kennt nur abstrakte Plattformports.
- `device_platform_test_support` enthaelt nur Testhilfen und darf nicht Teil
  eines Produktionsbuilds sein.

Bei Arbeiten in diesen Verzeichnissen gilt zusaetzlich die lokale `AGENTS.md`.

## Branch-, Plan- und Owner-Gates

Es wird niemals direkt auf `main` gearbeitet. Grundsaetzlich gilt ein
zusammenhaengender Scope und ein Issue pro Branch und Pull Request.

Nicht triviale Implementierungs-, Architektur-, Persistenz-, Security-, Safety-,
Hardware-, Bibliotheks- oder moduluebergreifende Arbeit verwendet den
Plan-first-Workflow aus `docs/AGENT_WORKFLOW.md`:

1. Branch und Draft-PR erstellen.
2. Versionierten Plan unter `docs/tasks/` erstellen.
3. Nach dem Plan-Commit anhalten.
4. Erst nach Freigabe des exakten Plan-Commits umsetzen.
5. Bei materieller Abweichung Plan aktualisieren und erneut freigeben lassen.

Der Agent setzt einen PR nicht selbst auf `Ready for review`, mergt nicht,
aktiviert kein Auto-Merge, verwendet keinen Force-Push, loescht keinen Branch,
schliesst kein Issue eigenmaechtig und fuehrt `/clear` nicht selbst aus.

`docs/ROADMAP.md` wird zu Beginn jedes neuen PR, nach jedem Merge und bei einer
materiellen Reihenfolgeaenderung aktualisiert. Anforderungen und Issue-Inhalte
werden dort nicht kopiert.

## Tests, CI und Review

Waehrend der Draft-Umsetzung werden nur gezielte lokale Tests fuer den
tatsaechlich geaenderten Bereich ausgefuehrt.

Ein Review prueft den vollstaendigen aktuellen Diff gegen Plan, Anforderungen,
Architektur, Tests, Dokumentation sowie SOLID, DRY und KISS. Es darf sich nicht
auf die auffaelligsten Befunde beschraenken.

Ein vollstaendiger lokaler Testlauf erfolgt nur nach abgeschlossenem Review ohne
offene Befunde, auf dem finalen `HEAD` und nach ausdruecklicher Owner-Anweisung.

GitHub-CI laeuft nicht waehrend der Draft-Phase. Der Owner setzt den PR nach dem
Review auf `Ready for review`; dadurch startet der vollstaendige CI-Lauf.
Spaetere Pushes auf einen nicht als Draft markierten PR starten CI erneut.
Reine Markdown-Aenderungen loesen keine vollstaendige Firmware-CI aus.

Testbefehle, Buildprofile, Werkzeuge und Ergebnisstatus stehen ausschliesslich
in `docs/CI_AND_QUALITY_GATES.md`. Nicht ausgefuehrte Tests duerfen nicht als
bestanden bezeichnet werden.

## Session-Handover

Vor Sessionende, Kontextreset oder Agentenwechsel muss bei einem offenen PR
genau ein aktueller `SESSION HANDOVER`-Kommentar erstellt und danach angehalten
werden. Er nennt kompakt aktuellen `HEAD`, freigegebenen Plan-Commit, erledigte
Bereiche, ausgefuehrte Tests, offene Befunde und den naechsten Schritt.

Der neueste Handover ersetzt fruehere Handovers. Diffs, Plaene und Logs werden
referenziert, nicht kopiert. Kann kein PR-Kommentar erstellt werden, wird der
fertige Text im Chat als nicht veroeffentlicht ausgegeben.

## Bedingte Lesematrix

| Aufgabe | Zusaetzlich zu lesen |
|---|---|
| Jede Aufgabe | Markdown-Auftrag, Live-Issue/PR, `docs/ROADMAP.md`, lokale `AGENTS.md` |
| Planung, Umsetzung oder Review | `docs/AGENT_WORKFLOW.md`, `docs/ENGINEERING_PRINCIPLES.md`; bei Code, Build oder Tests auch `docs/CI_AND_QUALITY_GATES.md` |
| Produkt-, Release- oder Scopeentscheidung | `docs/SPECIFICATION_REVIEW.md`, relevante akzeptierte ADRs |
| Architektur, Module oder Abhaengigkeiten | `docs/DECISIONS.md`, relevante ADRs, `lib/README.md`, lokale `AGENTS.md` |
| Bibliotheks- oder Komponentenauswahl | `docs/ENGINEERING_PRINCIPLES.md`, `docs/ADOPT_OR_BUILD.md`, Lizenz- und Herstellerquellen |
| Safety, Fehler, Recovery, Aktoren oder kritische Persistenz | betroffene Fachvertraege und Akzeptanztests |
| Hardware, Bring-up oder Inbetriebnahme | `docs/HARDWARE.md`, `docs/OPEN_POINTS.md`; bei ESP-IDF auch `docs/ESP_IDF_UPGRADE_CONTRACT.md` |
| UI, Web, Netzwerk, Konfiguration oder Fachlogik | nur vom Issue, Plan, ADR oder Code direkt referenzierte Fachdokumente |

Der freigegebene Plan benennt die konkret erforderlichen Fachquellen. Das
gesamte Dokumentationsverzeichnis wird nicht pauschal vollstaendig gelesen.
