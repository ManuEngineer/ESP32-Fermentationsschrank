# Agent-Auftrag fuer Issue #32

## Issue

**[E5.4] Luefter, Summer und Onboard-MOSFET-Ausgaenge integrieren**  
Aktueller Snapshot-Status: `BLOCKED_HARDWARE`  
Epic: #7  
GitHub: https://github.com/ManuEngineer/ESP32-Fermentationsschrank/issues/32

> Der Status und Inhalt auf GitHub sind die aktuelle Wahrheit. Lies das Live-Issue vor jeder Arbeit erneut. Dieser Auftrag ist eine Arbeitsanweisung, kein Ersatz fuer das Issue.

## Fertiger Auftrag zum Kopieren

```text
Arbeite im Repository `ManuEngineer/ESP32-Fermentationsschrank` ausschliesslich an Issue #32:
"[E5.4] Luefter, Summer und Onboard-MOSFET-Ausgaenge integrieren"

Pruefe den aktuellen Live-Status des Issues. Beginne nur, wenn es `READY` ist und alle Abhaengigkeiten abgeschlossen sind.

1. Sicheren Ausgangszustand herstellen
   - `git checkout main`
   - `git pull --ff-only`
   - `git fetch --prune`
   - `git status --short`
   - Bei lokalen Aenderungen, unklaren ungetrackten Dateien oder einem nicht sauberen Stand: anhalten und berichten. Nichts ungefragt verwerfen.
   - `config/hardware.yaml`, `config/pins.yaml`, Secrets, `.env`-Dateien und andere lokale Konfigurationen niemals einchecken.

2. Vor jeder Aenderung vollstaendig lesen
   - das aktuelle GitHub-Issue #32 inklusive Kommentare
   - `AGENTS.md`
   - alle fuer betroffene Unterverzeichnisse geltenden weiteren `AGENTS.md`
   - `docs/SPECIFICATION_REVIEW.md`, insbesondere die Dokumentationsprioritaet
   - `docs/DECISIONS.md`
   - die unten genannten Spezifikationsquellen
   - den aktuellen Code und die Tests der abgeschlossenen Abhaengigkeiten

3. Abhaengigkeiten und Freigabe pruefen
   - #23
   - #24
   - #28
   - #29
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
   `hardware/issue-32-luefter-summer-und-onboard-mosfet-ausgaenge-integrie`

   Keine Aenderungen direkt auf `main`. Keine anderen Issues in diesen Branch aufnehmen.

6. Verbindlicher Scope von Issue #32
   - Onboard-MOSFET-Pegel und Kanalzuordnung praktisch messen
   - Innenluefter, Aussenluefter und aktiven Summer einzeln integrieren
   - Ausgaenge zuerst unbelastet, danach mit einzelnem Verbraucher pruefen
   - Stromaufnahme und Anlaufverhalten erfassen
   - Innenluefter-Dauerbetrieb in geregelten Phasen
   - Aussenluefter ohne absichtlichen Vorlauf und mit zwingendem Nachlauf
   - sichere Boot- und Resetreaktion mit angeschlossenen Verbrauchern

7. Akzeptanzkriterien
   - jeder aktive Pegel und Kanal ist bestaetigt
   - kein Verbraucher startet unkontrolliert beim Boot oder Reset
   - Aussenluefternachlauf bleibt bei Sicherheitsabschaltung erhalten
   - Summermuster blockieren keine Sicherheitsaufgabe
   - Hardwaretestbericht ist gespeichert

8. Issue-spezifische Tests und Nachweise
   - unbelastete Pegel
   - Einzelverbraucher
   - Start/Stop/Nachlauf
   - Reset
   - simulierte Fehlerreaktionen

9. Zwingende Projektregeln
- Dieses Issue benoetigt reale Hardware und Messnachweise. Erfinde keine Ergebnisse und leite keine Pinbelegung aus aehnlichen Boards ab.
- Ist der Status weiterhin `BLOCKED_HARDWARE`, starte keine reale Umsetzung und erstelle keinen Implementierungs-PR. Liefere stattdessen eine genaue Blocker- und Vorbereitungsanalyse.
- Vor jeder Last zuerst unbelastet messen. Verbraucher einzeln anschliessen. Peltier niemals vor abgeschlossener Sicherungs-, Luefter- und Sensorpruefung betreiben.
- Jede bestaetigte Pin-, Pegel-, Strom- oder Controllerangabe muss auf einem dokumentierten realen Test beruhen.
- Ein PR darf `Closes` nur verwenden, wenn die reale Hardwarepruefung vollstaendig nachgewiesen ist; sonst `Refs` und Status `BLOCKED`.
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
    - alle in Issue #32 neu eingefuehrten Tests

    Dokumentiere jeden Befehl und das Ergebnis. Nicht ausfuehrbare Hardware- oder Langzeittests als `BLOCKED` ausweisen; niemals als bestanden darstellen.

11. Pull Request
    Erstelle nur bei einer in sich vollstaendigen, geprueften Aenderung einen kleinen PR gegen `main`.
    Der PR enthaelt:
    - klare Zusammenfassung,
    - Scope und bewusst nicht enthaltene Punkte,
    - ausgefuehrte Tests mit Ergebnissen,
    - Ressourcenwirkung,
    - bekannte Einschraenkungen und BLOCKED-Punkte,
    - `Closes #32` nur bei vollstaendig erfuellter Definition of Done; sonst `Refs #32`.

    Den PR nicht selbst mergen. Nach Reviews neue Kommentare einzeln fachlich pruefen, nur berechtigte Punkte einarbeiten und danach die vollstaendige CI erneut abwarten.
```

## Spezifikationsquellen

- docs/HARDWARE.md
- docs/ACTUATOR_TIMING.md
- docs/RUNTIME_BEHAVIOR.md
- docs/ACCEPTANCE_TESTS.md

## Definition of Done

Treiber, Pegelmessungen, Stromwerte, Tests und Dokumentation abgeschlossen.

## Vorgeschlagener Branch

`hardware/issue-32-luefter-summer-und-onboard-mosfet-ausgaenge-integrie`
