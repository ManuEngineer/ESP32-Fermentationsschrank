# Agent-Auftrag fuer Issue #10

## Issue

**[E0.2] Native Tests, CI, virtuelle Zeit und Buildberichte**  
Aktueller Snapshot-Status: `READY`  
Epic: #2  
GitHub: https://github.com/ManuEngineer/ESP32-Fermentationsschrank/issues/10

> Der Status und Inhalt auf GitHub sind die aktuelle Wahrheit. Lies das Live-Issue vor jeder Arbeit erneut. Dieser Auftrag ist eine Arbeitsanweisung, kein Ersatz fuer das Issue.

## Fertiger Auftrag zum Kopieren

```text
Arbeite im Repository `ManuEngineer/ESP32-Fermentationsschrank` ausschliesslich an Issue #10:
"[E0.2] Native Tests, CI, virtuelle Zeit und Buildberichte"

Pruefe den aktuellen Live-Status des Issues. Beginne nur, wenn es `READY` ist und alle Abhaengigkeiten abgeschlossen sind.

1. Sicheren Ausgangszustand herstellen
   - `git checkout main`
   - `git pull --ff-only`
   - `git fetch --prune`
   - `git status --short`
   - Bei lokalen Aenderungen, unklaren ungetrackten Dateien oder einem nicht sauberen Stand: anhalten und berichten. Nichts ungefragt verwerfen.
   - `config/hardware.yaml`, `config/pins.yaml`, Secrets, `.env`-Dateien und andere lokale Konfigurationen niemals einchecken.

2. Vor jeder Aenderung vollstaendig lesen
   - das aktuelle GitHub-Issue #10 inklusive Kommentare
   - `AGENTS.md`
   - alle fuer betroffene Unterverzeichnisse geltenden weiteren `AGENTS.md`
   - `docs/SPECIFICATION_REVIEW.md`, insbesondere die Dokumentationsprioritaet
   - `docs/DECISIONS.md`
   - die unten genannten Spezifikationsquellen
   - den aktuellen Code und die Tests der abgeschlossenen Abhaengigkeiten

3. Abhaengigkeiten und Freigabe pruefen
   - #9
   - Nicht auf Annahmen oder veraltete Issue-Snapshots vertrauen.
   - Ist eine Abhaengigkeit offen oder der Issue-Status nicht freigegeben, keinen Implementierungsbranch erstellen. Stattdessen Blocker, benoetigte Entscheidung und naechsten sinnvollen Schritt berichten.

4. Vor Codeaenderungen zuerst einen Plan vorlegen
   Berichte:
   - was im aktuellen Stand bereits vorhanden ist,
   - welche Akzeptanzkriterien noch fehlen,
   - welche Dateien voraussichtlich geaendert oder neu angelegt werden,
   - welche Tests und Nachweise vorgesehen sind,
   - welche Risiken, Architekturentscheidungen oder Spezifikationskonflikte bestehen.

   Bei einer echten Architekturentscheidung, einem Spezifikationswiderspruch oder einer sicherheitsrelevanten Unklarheit anhalten und den Owner fragen. Normale Implementierungsdetails selbststaendig entscheiden.

5. Branch
   Nach Freigabe des Plans einen neuen Branch vom aktuellen `main` erstellen:
   `feat/issue-10-native-tests-ci-virtuelle-zeit-und-buildberichte`

   Keine Aenderungen direkt auf `main`. Keine anderen Issues in diesen Branch aufnehmen.

6. Verbindlicher Scope von Issue #10
   - native Testausfuehrung auf dem Entwicklungsrechner
   - CI fuer native Tests und beide ESP32-Zielbuilds
   - virtuelle monotone und absolute Zeitquelle
   - Pruefung auf Geheimnisse und lokale Konfigurationsdateien
   - Firmware- und Ressourcen-Groessenbericht
   - klare Ergebnisse `PASS`, `FAILED`, `BLOCKED`
   - verbindliche Format-, Compilerwarnungs- und Static-Analysis-Strategie
   - versionierte Werkzeugkonfiguration und dokumentierte Ausnahmen

7. Akzeptanzkriterien
   - Tests sind ohne reale Uhrzeit und Netzwerk reproduzierbar
   - CI blockiert bei fehlerhaftem Kern- oder Sicherheitstest
   - beide ESP32-Profile werden gebaut
   - eingecheckte Geheimnisse werden erkannt
   - Buildbericht weist Firmwaregroesse aus
   - Formatpruefung und statische Analysen laufen reproduzierbar in CI
   - neue Compilerwarnungen werden nicht still akzeptiert

8. Issue-spezifische Tests und Nachweise
   - erfolgreicher und absichtlich fehlerhafter CI-Fall
   - Zeitvorwaertsprung, Neustart und fehlende UTC-Zeit im nativen Test
   - absichtlich falsch formatierte oder statisch auffaellige Testaenderung wird erkannt

9. Zwingende Projektregeln
- Arbeite software-first und halte alle fachlichen Komponenten nativ testbar.
- Keine reale GPIO-, Treiber- oder Aktorlogik in diesem Issue einfuehren.
- `TBD_HARDWARE` und `TBD_COMMISSIONING` bleiben Platzhalter und duerfen nicht als bestaetigte Werte verwendet werden.
- `src/main.cpp` bleibt Composition Root ohne Prozesslogik.
- Wiederverwendbare Geraetedienste gehoeren in `lib/device_platform`; konkrete Fermentationslogik in `lib/fermentation_app`.
   - Bestehende Sicherheitsinvarianten niemals abschwaechen.
   - Keine neue grosse Abhaengigkeit ohne konkrete Begruendung, Ressourcenwirkung und Alternativenvergleich.
   - Keine parallele zweite Architektur oder doppelte Quelle der Wahrheit einfuehren.
   - Dokumentation und Changelog mit der Implementierung synchron halten.
   - Keine Tests deaktivieren, um CI gruen zu machen.

10. Mindestpruefungen vor einem PR
    Fuehre alle fuer den Scope relevanten Pruefungen aus. Soweit anwendbar mindestens:
    - `pio run -e native -e esp32_bringup -e esp32_release`
    - `pio test -e native`
    - `python scripts/check_platformio_config.py`
    - Format-, Compilerwarnungs-, Static-Analysis- und Secret-Pruefungen aus dem aktuellen Repository
    - alle in Issue #10 neu eingefuehrten Tests

    Dokumentiere jeden Befehl und das Ergebnis. Nicht ausfuehrbare Hardware- oder Langzeittests als `BLOCKED` ausweisen; niemals als bestanden darstellen.

11. Pull Request
    Erstelle nur bei einer in sich vollstaendigen, geprueften Aenderung einen kleinen PR gegen `main`.
    Der PR enthaelt:
    - klare Zusammenfassung,
    - Scope und bewusst nicht enthaltene Punkte,
    - ausgefuehrte Tests mit Ergebnissen,
    - Ressourcenwirkung,
    - bekannte Einschraenkungen und BLOCKED-Punkte,
    - `Closes #10` nur bei vollstaendig erfuellter Definition of Done; sonst `Refs #10`.

    Den PR nicht selbst mergen. Nach Reviews neue Kommentare einzeln fachlich pruefen, nur berechtigte Punkte einarbeiten und danach die vollstaendige CI erneut abwarten.
```

## Spezifikationsquellen

- docs/ACCEPTANCE_TESTS.md
- docs/IMPLEMENTATION_PLAN.md

## Definition of Done

CI, Testharness, Zeitquelle, Berichte, Format-/Analysewerkzeuge und Dokumentation sind nutzbar.

## Vorgeschlagener Branch

`feat/issue-10-native-tests-ci-virtuelle-zeit-und-buildberichte`
