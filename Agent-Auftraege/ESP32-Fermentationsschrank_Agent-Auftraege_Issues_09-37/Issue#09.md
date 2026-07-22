# Agent-Auftrag fuer Issue #09

## Issue

**[E0.1] PlatformIO-Profile und Projektgrundlage einrichten**  
Aktueller Snapshot-Status: `COMPLETED`  
Epic: #2  
GitHub: https://github.com/ManuEngineer/ESP32-Fermentationsschrank/issues/9

> Der Status und Inhalt auf GitHub sind die aktuelle Wahrheit. Lies das Live-Issue vor jeder Arbeit erneut. Dieser Auftrag ist eine Arbeitsanweisung, kein Ersatz fuer das Issue.

## Fertiger Auftrag zum Kopieren

```text
Arbeite im Repository `ManuEngineer/ESP32-Fermentationsschrank` ausschliesslich an Issue #9:
"[E0.1] PlatformIO-Profile und Projektgrundlage einrichten"

Das Issue ist bereits abgeschlossen. Fuehre nur eine Verifikationsanalyse durch.

1. Sicheren Ausgangszustand herstellen
   - `git checkout main`
   - `git pull --ff-only`
   - `git fetch --prune`
   - `git status --short`
   - Bei lokalen Aenderungen, unklaren ungetrackten Dateien oder einem nicht sauberen Stand: anhalten und berichten. Nichts ungefragt verwerfen.
   - `config/hardware.yaml`, `config/pins.yaml`, Secrets, `.env`-Dateien und andere lokale Konfigurationen niemals einchecken.

2. Vor jeder Aenderung vollstaendig lesen
   - das aktuelle GitHub-Issue #9 inklusive Kommentare
   - `AGENTS.md`
   - alle fuer betroffene Unterverzeichnisse geltenden weiteren `AGENTS.md`
   - `docs/SPECIFICATION_REVIEW.md`, insbesondere die Dokumentationsprioritaet
   - `docs/DECISIONS.md`
   - die unten genannten Spezifikationsquellen
   - den aktuellen Code und die Tests der abgeschlossenen Abhaengigkeiten

3. Abhaengigkeiten und Freigabe pruefen
   - Keine Implementierungsabhaengigkeit; Spezifikations-PR musste gemergt sein.
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
   `feat/issue-9-platformio-profile-und-projektgrundlage-einrichten`

   Keine Aenderungen direkt auf `main`. Keine anderen Issues in diesen Branch aufnehmen.

6. Verbindlicher Scope von Issue #9
   - PlatformIO-Umgebungen `native`, `esp32_bringup` und `esp32_release`
   - gemeinsame, klar getrennte Quell- und Teststruktur
   - Release-1-Ziel: ESP32-32E, 4 MB Flash, keine PSRAM-Abhaengigkeit
   - Web-OTA und andere Zukunftsfunktionen im Releaseprofil deaktivieren
   - sichere Standardkonfiguration ohne reale Aktorfreigabe

7. Akzeptanzkriterien
   - alle drei Profile sind definiert und dokumentiert
   - `native` kompiliert ohne Arduino-Hardware
   - beide ESP32-Profile bauen mit unterschiedlicher Freigabepolitik
   - `esp32_bringup` startet konzeptionell in `HARDWARE_UNVERIFIED`
   - keine Geheimnisse oder lokale Zugangsdaten sind eingecheckt

8. Issue-spezifische Tests und Nachweise
   - Build aller Profile
   - Konfigurationspruefung auf 4 MB und keine vorausgesetzte PSRAM

9. Zwingende Projektregeln
- Dieses Issue ist bereits abgeschlossen. Implementiere es nicht erneut.
- Pruefe nur, ob der aktuelle `main`-Stand das Issue weiterhin abdeckt.
- Erstelle keinen Branch und keinen PR, ausser der Owner fordert ausdruecklich eine Korrektur oder Regressionbehebung an.
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
    - alle in Issue #9 neu eingefuehrten Tests

    Dokumentiere jeden Befehl und das Ergebnis. Nicht ausfuehrbare Hardware- oder Langzeittests als `BLOCKED` ausweisen; niemals als bestanden darstellen.

11. Pull Request
    Erstelle nur bei einer in sich vollstaendigen, geprueften Aenderung einen kleinen PR gegen `main`.
    Der PR enthaelt:
    - klare Zusammenfassung,
    - Scope und bewusst nicht enthaltene Punkte,
    - ausgefuehrte Tests mit Ergebnissen,
    - Ressourcenwirkung,
    - bekannte Einschraenkungen und BLOCKED-Punkte,
    - `Closes #9` nur bei vollstaendig erfuellter Definition of Done; sonst `Refs #9`.

    Den PR nicht selbst mergen. Nach Reviews neue Kommentare einzeln fachlich pruefen, nur berechtigte Punkte einarbeiten und danach die vollstaendige CI erneut abwarten.
```

## Spezifikationsquellen

- docs/IMPLEMENTATION_PLAN.md
- docs/FIRMWARE_UPDATE_AND_ROLLBACK.md
- docs/RESOURCE_BUDGET_AND_MAINTENANCE.md

## Definition of Done

Code, Builds, Tests und Dokumentation abgeschlossen; Hardwarewerte bleiben sichtbar TBD_HARDWARE beziehungsweise TBD_IMPLEMENTATION_BUDGET.

## Vorgeschlagener Branch

`feat/issue-9-platformio-profile-und-projektgrundlage-einrichten`
