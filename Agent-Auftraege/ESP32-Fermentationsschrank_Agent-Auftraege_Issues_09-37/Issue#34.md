# Agent-Auftrag fuer Issue #34

## Issue

**[E6.1] Sensorvergleich, Offsets und thermische Grundvermessung**  
Aktueller Snapshot-Status: `TBD_COMMISSIONING`  
Epic: #8  
GitHub: https://github.com/ManuEngineer/ESP32-Fermentationsschrank/issues/34

> Der Status und Inhalt auf GitHub sind die aktuelle Wahrheit. Lies das Live-Issue vor jeder Arbeit erneut. Dieser Auftrag ist eine Arbeitsanweisung, kein Ersatz fuer das Issue.

## Fertiger Auftrag zum Kopieren

```text
Arbeite im Repository `ManuEngineer/ESP32-Fermentationsschrank` ausschliesslich an Issue #34:
"[E6.1] Sensorvergleich, Offsets und thermische Grundvermessung"

Pruefe den aktuellen Live-Status des Issues. Beginne nur, wenn es `READY` ist und alle Abhaengigkeiten abgeschlossen sind.

1. Sicheren Ausgangszustand herstellen
   - `git checkout main`
   - `git pull --ff-only`
   - `git fetch --prune`
   - `git status --short`
   - Bei lokalen Aenderungen, unklaren ungetrackten Dateien oder einem nicht sauberen Stand: anhalten und berichten. Nichts ungefragt verwerfen.
   - `config/hardware.yaml`, `config/pins.yaml`, Secrets, `.env`-Dateien und andere lokale Konfigurationen niemals einchecken.

2. Vor jeder Aenderung vollstaendig lesen
   - das aktuelle GitHub-Issue #34 inklusive Kommentare
   - `AGENTS.md`
   - alle fuer betroffene Unterverzeichnisse geltenden weiteren `AGENTS.md`
   - `docs/SPECIFICATION_REVIEW.md`, insbesondere die Dokumentationsprioritaet
   - `docs/DECISIONS.md`
   - die unten genannten Spezifikationsquellen
   - den aktuellen Code und die Tests der abgeschlossenen Abhaengigkeiten

3. Abhaengigkeiten und Freigabe pruefen
   - #29
   - #30
   - #31
   - #32
   - #33
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
   `commissioning/issue-34-sensorvergleich-offsets-und-thermische-grundvermessu`

   Keine Aenderungen direkt auf `main`. Keine anderen Issues in diesen Branch aufnehmen.

6. Verbindlicher Scope von Issue #34
   - externe Referenzmessgeraete dokumentieren
   - drei DS18B20 vergleichen und individuelle Offsets festlegen
   - leeren Schrank, kleine und grosse Referenzmasse vermessen
   - Aufheizen, Abkuehlen, Halten und Temperaturverteilung erfassen
   - kritischste Stellen und Ort fuer Kuehlkoerpersensor/Temperatursicherung bestimmen
   - Rohdaten und Serviceberichte exportieren

7. Akzeptanzkriterien
   - Referenzgeraete, Aufbau, Sensorpositionen und Messbedingungen sind dokumentiert
   - Offsets sind je ROM-Adresse nachvollziehbar
   - thermische Reaktion fuer Heizen und Kuehlen liegt als Messreihe vor
   - Produkttraegheit und Luftverteilung sind mit mindestens zwei Massen erfasst
   - keine Grenzwerte werden ohne Messgrundlage final gesetzt

8. Issue-spezifische Tests und Nachweise
   - leerer Schrank
   - kleine/grosse Masse
   - mehrere Sollbereiche
   - Stoerung durch Oeffnen
   - Richtungswechsel

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
    - alle in Issue #34 neu eingefuehrten Tests

    Dokumentiere jeden Befehl und das Ergebnis. Nicht ausfuehrbare Hardware- oder Langzeittests als `BLOCKED` ausweisen; niemals als bestanden darstellen.

11. Pull Request
    Erstelle nur bei einer in sich vollstaendigen, geprueften Aenderung einen kleinen PR gegen `main`.
    Der PR enthaelt:
    - klare Zusammenfassung,
    - Scope und bewusst nicht enthaltene Punkte,
    - ausgefuehrte Tests mit Ergebnissen,
    - Ressourcenwirkung,
    - bekannte Einschraenkungen und BLOCKED-Punkte,
    - `Closes #34` nur bei vollstaendig erfuellter Definition of Done; sonst `Refs #34`.

    Den PR nicht selbst mergen. Nach Reviews neue Kommentare einzeln fachlich pruefen, nur berechtigte Punkte einarbeiten und danach die vollstaendige CI erneut abwarten.
```

## Spezifikationsquellen

- docs/SENSOR_TUNING_COMMISSIONING.md
- docs/ACCEPTANCE_TESTS.md
- docs/SAFETY_COMPONENT_FAULTS.md

## Definition of Done

Messprotokolle, Offsets, Exporte und dokumentierte Erkenntnisse abgeschlossen.

## Vorgeschlagener Branch

`commissioning/issue-34-sensorvergleich-offsets-und-thermische-grundvermessu`
