# Agent-Auftrag fuer Issue #35

## Issue

**[E6.2] PI-Parameter, Luftbegrenzungen und Sicherheitsgrenzen festlegen**  
Aktueller Snapshot-Status: `TBD_COMMISSIONING`  
Epic: #8  
GitHub: https://github.com/ManuEngineer/ESP32-Fermentationsschrank/issues/35

> Der Status und Inhalt auf GitHub sind die aktuelle Wahrheit. Lies das Live-Issue vor jeder Arbeit erneut. Dieser Auftrag ist eine Arbeitsanweisung, kein Ersatz fuer das Issue.

## Fertiger Auftrag zum Kopieren

```text
Arbeite im Repository `ManuEngineer/ESP32-Fermentationsschrank` ausschliesslich an Issue #35:
"[E6.2] PI-Parameter, Luftbegrenzungen und Sicherheitsgrenzen festlegen"

Pruefe den aktuellen Live-Status des Issues. Beginne nur, wenn es `READY` ist und alle Abhaengigkeiten abgeschlossen sind.

1. Sicheren Ausgangszustand herstellen
   - `git checkout main`
   - `git pull --ff-only`
   - `git fetch --prune`
   - `git status --short`
   - Bei lokalen Aenderungen, unklaren ungetrackten Dateien oder einem nicht sauberen Stand: anhalten und berichten. Nichts ungefragt verwerfen.
   - `config/hardware.yaml`, `config/pins.yaml`, Secrets, `.env`-Dateien und andere lokale Konfigurationen niemals einchecken.

2. Vor jeder Aenderung vollstaendig lesen
   - das aktuelle GitHub-Issue #35 inklusive Kommentare
   - `AGENTS.md`
   - alle fuer betroffene Unterverzeichnisse geltenden weiteren `AGENTS.md`
   - `docs/SPECIFICATION_REVIEW.md`, insbesondere die Dokumentationsprioritaet
   - `docs/DECISIONS.md`
   - die unten genannten Spezifikationsquellen
   - den aktuellen Code und die Tests der abgeschlossenen Abhaengigkeiten

3. Abhaengigkeiten und Freigabe pruefen
   - #22
   - #23
   - #24
   - #33
   - #34
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
   `commissioning/issue-35-pi-parameter-luftbegrenzungen-und-sicherheitsgrenzen`

   Keine Aenderungen direkt auf `main`. Keine anderen Issues in diesen Branch aufnehmen.

6. Verbindlicher Scope von Issue #35
   - vier PI-Parametersaetze fuer Luft/Produkt und Heizen/Kuehlen bestimmen
   - Schaltfenster, Mindest-Ein-/Auszeit, Totzeit und Nachlauf validieren
   - fruehe Luftbegrenzungen, Prozesswarnungen, Sicherheits-Eingriffs- und harte Notgrenzen festlegen
   - begrenzte Gegenrichtungsversuche validieren
   - Filterzeiten, Zielband, Gnadenzeit und Sensor-Rueckkehrstabilitaet bestimmen
   - einmalige Temperatursicherung auswaehlen, montieren und dokumentieren

7. Akzeptanzkriterien
   - Parameter beruhen auf dokumentierten Messreihen
   - Heizen und Kuehlen sind bei mehreren Massen stabil
   - keine kritischen Ueberschwinger oder unkontrollierten Richtungswechsel
   - Sicherheits-Eingriffsgrenze und harte Notgrenze sind getrennt nachgewiesen
   - Temperatursicherung, Montageort und Rating sind dokumentiert
   - alle TBD_COMMISSIONING-Werte dieses Scopes werden aktualisiert

8. Issue-spezifische Tests und Nachweise
   - Sprungantworten
   - Sollwerthalten
   - Tag-/Nacht-Richtungswechsel
   - Ueberschwingen
   - Gegenrichtung
   - Notgrenze

9. Zwingende Projektregeln
- Dieses Issue ist eine reale Inbetriebnahme-/Abnahmeaufgabe. Messwerte, Parameter und Grenzwerte duerfen nur aus dokumentierten realen Versuchen stammen.
- Ist der Status weiterhin `TBD_COMMISSIONING`, beginne nur nach ausdruecklicher Freigabe und wenn alle Abhaengigkeiten abgeschlossen sind.
- Vor dem Versuch einen Mess- und Sicherheitsplan mit Abbruchkriterien erstellen und vom Owner bestaetigen lassen.
- Rohdaten, Aufbau, Geraete, Firmwarestand, Datum, Umgebungsbedingungen und Abweichungen versioniert dokumentieren.
- Keine bestandene Abnahme behaupten, solange der reale Nachweis nicht vollstaendig vorliegt.
- Ein PR darf `Closes` nur verwenden, wenn alle realen Nachweise und Release-Gates erfuellt sind; sonst `Refs` und klarer Status `BLOCKED`.
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
    - alle in Issue #35 neu eingefuehrten Tests

    Dokumentiere jeden Befehl und das Ergebnis. Nicht ausfuehrbare Hardware- oder Langzeittests als `BLOCKED` ausweisen; niemals als bestanden darstellen.

11. Pull Request
    Erstelle nur bei einer in sich vollstaendigen, geprueften Aenderung einen kleinen PR gegen `main`.
    Der PR enthaelt:
    - klare Zusammenfassung,
    - Scope und bewusst nicht enthaltene Punkte,
    - ausgefuehrte Tests mit Ergebnissen,
    - Ressourcenwirkung,
    - bekannte Einschraenkungen und BLOCKED-Punkte,
    - `Closes #35` nur bei vollstaendig erfuellter Definition of Done; sonst `Refs #35`.

    Den PR nicht selbst mergen. Nach Reviews neue Kommentare einzeln fachlich pruefen, nur berechtigte Punkte einarbeiten und danach die vollstaendige CI erneut abwarten.
```

## Spezifikationsquellen

- docs/TEMPERATURE_CONTROL.md
- docs/ACTUATOR_TIMING.md
- docs/SENSOR_TUNING_COMMISSIONING.md
- docs/SAFETY_COMPONENT_FAULTS.md

## Definition of Done

Parameterrevision, Messnachweise, Sicherheitsfreigabe und Dokumentation abgeschlossen.

## Vorgeschlagener Branch

`commissioning/issue-35-pi-parameter-luftbegrenzungen-und-sicherheitsgrenzen`
