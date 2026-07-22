# Agent-Auftrag fuer Issue #33

## Issue

**[E5.5] BTS7960, R_IS/L_IS und begrenzte Peltierpruefungen**  
Aktueller Snapshot-Status: `BLOCKED_HARDWARE`  
Epic: #7  
GitHub: https://github.com/ManuEngineer/ESP32-Fermentationsschrank/issues/33

> Der Status und Inhalt auf GitHub sind die aktuelle Wahrheit. Lies das Live-Issue vor jeder Arbeit erneut. Dieser Auftrag ist eine Arbeitsanweisung, kein Ersatz fuer das Issue.

## Fertiger Auftrag zum Kopieren

```text
Arbeite im Repository `ManuEngineer/ESP32-Fermentationsschrank` ausschliesslich an Issue #33:
"[E5.5] BTS7960, R_IS/L_IS und begrenzte Peltierpruefungen"

Pruefe den aktuellen Live-Status des Issues. Beginne nur, wenn es `READY` ist und alle Abhaengigkeiten abgeschlossen sind.

1. Sicheren Ausgangszustand herstellen
   - `git checkout main`
   - `git pull --ff-only`
   - `git fetch --prune`
   - `git status --short`
   - Bei lokalen Aenderungen, unklaren ungetrackten Dateien oder einem nicht sauberen Stand: anhalten und berichten. Nichts ungefragt verwerfen.
   - `config/hardware.yaml`, `config/pins.yaml`, Secrets, `.env`-Dateien und andere lokale Konfigurationen niemals einchecken.

2. Vor jeder Aenderung vollstaendig lesen
   - das aktuelle GitHub-Issue #33 inklusive Kommentare
   - `AGENTS.md`
   - alle fuer betroffene Unterverzeichnisse geltenden weiteren `AGENTS.md`
   - `docs/SPECIFICATION_REVIEW.md`, insbesondere die Dokumentationsprioritaet
   - `docs/DECISIONS.md`
   - die unten genannten Spezifikationsquellen
   - den aktuellen Code und die Tests der abgeschlossenen Abhaengigkeiten

3. Abhaengigkeiten und Freigabe pruefen
   - #23
   - #24
   - #29
   - #30
   - #32
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
   `hardware/issue-33-bts7960-r-is-l-is-und-begrenzte-peltierpruefungen`

   Keine Aenderungen direkt auf `main`. Keine anderen Issues in diesen Branch aufnehmen.

6. Verbindlicher Scope von Issue #33
   - BTS7960-Pinbelegung, Enable-Verhalten und Richtungen praktisch verifizieren
   - Hardware-Pulldowns oder gleichwertige sichere Freigabestufe nachweisen
   - H-Brueckenausgang und Polaritaet vor Peltieranschluss mit Multimeter pruefen
   - R_IS/L_IS nur nach Pegel- und Nutzbarkeitspruefung anschliessen
   - Peltier erst mit 7,5-A-Sicherung, geprueften Lueftern und gueltigen Sicherheitssensoren
   - begrenzte Heiz-/Kuehlpulse, Abschaltung, Nachlauf, Mindest-Auszeit und Totzeit
   - Testabbruch und Fehlerreaktionen

7. Akzeptanzkriterien
   - beide Richtungen koennen hardware- und softwareseitig nie gleichzeitig aktiv sein
   - Boot/Reset laesst BTS7960 sicher deaktiviert
   - Polaritaet und reale Heiz-/Kuehlrichtung sind dokumentiert
   - R_IS/L_IS werden nur bei brauchbarem Signal aktiviert
   - erste reale Peltierfreigabe erfolgt ausschliesslich im Service-/Bring-up-Modus
   - jeder Puls ist zeitlich und leistungsmassig begrenzt

8. Issue-spezifische Tests und Nachweise
   - unbelastete H-Bruecke
   - Polaritaet
   - Reset
   - Fehlerabbruch
   - Heiz-/Kuehlpulse
   - Totzeit
   - Luefternachlauf
   - Sensorabzug

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
    - alle in Issue #33 neu eingefuehrten Tests

    Dokumentiere jeden Befehl und das Ergebnis. Nicht ausfuehrbare Hardware- oder Langzeittests als `BLOCKED` ausweisen; niemals als bestanden darstellen.

11. Pull Request
    Erstelle nur bei einer in sich vollstaendigen, geprueften Aenderung einen kleinen PR gegen `main`.
    Der PR enthaelt:
    - klare Zusammenfassung,
    - Scope und bewusst nicht enthaltene Punkte,
    - ausgefuehrte Tests mit Ergebnissen,
    - Ressourcenwirkung,
    - bekannte Einschraenkungen und BLOCKED-Punkte,
    - `Closes #33` nur bei vollstaendig erfuellter Definition of Done; sonst `Refs #33`.

    Den PR nicht selbst mergen. Nach Reviews neue Kommentare einzeln fachlich pruefen, nur berechtigte Punkte einarbeiten und danach die vollstaendige CI erneut abwarten.
```

## Spezifikationsquellen

- docs/HARDWARE.md
- docs/ACTUATOR_TIMING.md
- docs/SAFETY_COMPONENT_FAULTS.md
- docs/DIAGNOSTICS_AND_MAINTENANCE.md
- docs/ACCEPTANCE_TESTS.md

## Definition of Done

Verdrahtung, Messprotokoll, Treiber, sichere Servicepruefung und Dokumentation abgeschlossen.

## Vorgeschlagener Branch

`hardware/issue-33-bts7960-r-is-l-is-und-begrenzte-peltierpruefungen`
