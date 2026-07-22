# Agent-Auftrag fuer Issue #24

## Issue

**[E3.5] Fehlerklassen, Verriegelung, SAFE_BOOT und Fehlerinjektion**  
Aktueller Snapshot-Status: `PLANNED_SPEC_PENDING`  
Epic: #5  
GitHub: https://github.com/ManuEngineer/ESP32-Fermentationsschrank/issues/24

> Der Status und Inhalt auf GitHub sind die aktuelle Wahrheit. Lies das Live-Issue vor jeder Arbeit erneut. Dieser Auftrag ist eine Arbeitsanweisung, kein Ersatz fuer das Issue.

## Fertiger Auftrag zum Kopieren

```text
Arbeite im Repository `ManuEngineer/ESP32-Fermentationsschrank` ausschliesslich an Issue #24:
"[E3.5] Fehlerklassen, Verriegelung, SAFE_BOOT und Fehlerinjektion"

Pruefe den aktuellen Live-Status des Issues. Beginne nur, wenn es `READY` ist und alle Abhaengigkeiten abgeschlossen sind.

1. Sicheren Ausgangszustand herstellen
   - `git checkout main`
   - `git pull --ff-only`
   - `git fetch --prune`
   - `git status --short`
   - Bei lokalen Aenderungen, unklaren ungetrackten Dateien oder einem nicht sauberen Stand: anhalten und berichten. Nichts ungefragt verwerfen.
   - `config/hardware.yaml`, `config/pins.yaml`, Secrets, `.env`-Dateien und andere lokale Konfigurationen niemals einchecken.

2. Vor jeder Aenderung vollstaendig lesen
   - das aktuelle GitHub-Issue #24 inklusive Kommentare
   - `AGENTS.md`
   - alle fuer betroffene Unterverzeichnisse geltenden weiteren `AGENTS.md`
   - `docs/SPECIFICATION_REVIEW.md`, insbesondere die Dokumentationsprioritaet
   - `docs/DECISIONS.md`
   - die unten genannten Spezifikationsquellen
   - den aktuellen Code und die Tests der abgeschlossenen Abhaengigkeiten

3. Abhaengigkeiten und Freigabe pruefen
   - #14
   - #15
   - #17
   - #20
   - #21
   - #23
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
   `feat/issue-24-fehlerklassen-verriegelung-safe-boot-und-fehlerinjek`

   Keine Aenderungen direkt auf `main`. Keine anderen Issues in diesen Branch aufnehmen.

6. Verbindlicher Scope von Issue #24
   - vier Fehlerklassen und stabile Fehlercodes
   - unmittelbare sichere Reaktionen
   - Quittierung getrennt von Fehlerreset
   - automatische Wiederfreigabe nur fuer explizit erlaubte Betriebsfehler
   - persistente Sicherheits- und Systemverriegelungen
   - Watchdog, einmaliger kontrollierter Neustart und `SAFE_BOOT`
   - Primaer-/Folgefehlerbeziehung
   - simulierte Fehlerinjektionen fuer Sensoren, Aktoren, Persistenz, Brownout und Neustart

7. Akzeptanzkriterien
   - hoechste aktive Klasse bestimmt den sicheren Ausgangszustand
   - Neustart loescht keine Verriegelung
   - wiederholte abnormale Neustarts fuehren zu `SAFE_BOOT`
   - keine unbekannte Fehlerursache fuehrt zu einer Aktorfreigabe
   - Fehlerinjektionen sind reproduzierbar und automatisiert

8. Issue-spezifische Tests und Nachweise
   - alle verpflichtenden softwareseitigen Fehlerinjektionen aus `ACCEPTANCE_TESTS.md`

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
    - alle in Issue #24 neu eingefuehrten Tests

    Dokumentiere jeden Befehl und das Ergebnis. Nicht ausfuehrbare Hardware- oder Langzeittests als `BLOCKED` ausweisen; niemals als bestanden darstellen.

11. Pull Request
    Erstelle nur bei einer in sich vollstaendigen, geprueften Aenderung einen kleinen PR gegen `main`.
    Der PR enthaelt:
    - klare Zusammenfassung,
    - Scope und bewusst nicht enthaltene Punkte,
    - ausgefuehrte Tests mit Ergebnissen,
    - Ressourcenwirkung,
    - bekannte Einschraenkungen und BLOCKED-Punkte,
    - `Closes #24` nur bei vollstaendig erfuellter Definition of Done; sonst `Refs #24`.

    Den PR nicht selbst mergen. Nach Reviews neue Kommentare einzeln fachlich pruefen, nur berechtigte Punkte einarbeiten und danach die vollstaendige CI erneut abwarten.
```

## Spezifikationsquellen

- docs/SAFETY_AND_FAULTS.md
- docs/SAFETY_COMPONENT_FAULTS.md
- docs/SYSTEM_SAFETY_AND_RECOVERY.md
- docs/ACCEPTANCE_TESTS.md

## Definition of Done

Fehlerkern, Simulationstests, Journalereignisse und Dokumentation abgeschlossen.

## Vorgeschlagener Branch

`feat/issue-24-fehlerklassen-verriegelung-safe-boot-und-fehlerinjek`
