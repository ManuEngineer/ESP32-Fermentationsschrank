# Agent-Auftrag fuer Issue #14

## Issue

**[E1.3] Zustandsmaschine und Prozessablaeufe implementieren**  
Aktueller Snapshot-Status: `READY`
Epic: #3  
GitHub: https://github.com/ManuEngineer/ESP32-Fermentationsschrank/issues/14

> Der Status und Inhalt auf GitHub sind die aktuelle Wahrheit. Lies das Live-Issue vor jeder Arbeit erneut. Dieser Auftrag ist eine Arbeitsanweisung, kein Ersatz fuer das Issue.

## Fertiger Auftrag zum Kopieren

```text
Arbeite im Repository `ManuEngineer/ESP32-Fermentationsschrank` ausschliesslich an Issue #14:
"[E1.3] Zustandsmaschine und Prozessablaeufe implementieren"

Pruefe den aktuellen Live-Status des Issues. Beginne nur, wenn es `READY` ist und alle Abhaengigkeiten abgeschlossen sind.

1. Sicheren Ausgangszustand herstellen
   - `git checkout main`
   - `git pull --ff-only`
   - `git fetch --prune`
   - `git status --short`
   - Bei lokalen Aenderungen, unklaren ungetrackten Dateien oder einem nicht sauberen Stand: anhalten und berichten. Nichts ungefragt verwerfen.
   - `config/hardware.yaml`, `config/pins.yaml`, Secrets, `.env`-Dateien und andere lokale Konfigurationen niemals einchecken.

2. Vor jeder Aenderung vollstaendig lesen
   - das aktuelle GitHub-Issue #14 inklusive Kommentare
   - `AGENTS.md`
   - alle fuer betroffene Unterverzeichnisse geltenden weiteren `AGENTS.md`
   - `docs/SPECIFICATION_REVIEW.md`, insbesondere die Dokumentationsprioritaet
   - `docs/DECISIONS.md`
   - die unten genannten Spezifikationsquellen
   - den aktuellen Code und die Tests der abgeschlossenen Abhaengigkeiten

3. Abhaengigkeiten und Freigabe pruefen
   - #10
   - #11
   - #12
   - #13
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
   `feat/issue-14-zustandsmaschine-und-prozessablaeufe-implementieren`

   Keine Aenderungen direkt auf `main`. Keine anderen Issues in diesen Branch aufnehmen.

6. Verbindlicher Scope von Issue #14
   - alle kanonischen Zustandsnamen und die grundsaetzlich erlaubte
     Uebergangstopologie
   - Standby, Vorheizen, Warten auf Produkt, Zielerreichung, Zielqualifikation, Fermentation, Kuehlen, Halten, Completed und Fehlerzustaende
   - phasenbezogene Uebergaenge und Zeitlimits
   - produkt- und luftgefuehrte Ablaeufe
   - optionale Vorheizung und Zielqualifikation
   - programmspezifische Produktwartezeit samt Schema-5-Migration
   - vollstaendiger Uebergangsweg fuer `MANUAL_HOLDING`; Laufplan und
     Startkommando folgen in #15
   - deterministische, noch nicht angewendete Uebergangsentscheidungen
   - nur abstrahierte, bereits qualitaetsgepruefte Prozesssignale und monotone
     virtuelle Zeit
   - detaillierte Boot-, Recovery-, Service-, Fehlerreset-, Sensorqualitaets- und
     Persistenzpolitik bleiben in #17, #18, #20 und #24
   - kein direkter Hardwarezugriff
   - keine direkte Nutzung von `IStateStore` oder `IEventJournal` und keine reale
     Aktorfreigabe

7. Akzeptanzkriterien
   - jeder erlaubte Uebergang ist explizit
   - unzulaessige Uebergaenge werden abgelehnt
   - Zielqualifikation startet die Fermentationszeit korrekt
   - Abschlussmodi fuehren in den vorgesehenen Zustand
   - ein kompletter Ablauf ist mit virtueller Zeit reproduzierbar

8. Issue-spezifische Tests und Nachweise
   - alle vier Standardablaeufe
   - Vorheizen ein/aus
   - Produktfuehler vorhanden/nicht vorhanden
   - Phasenzeitlimits und Abbruch

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
    - alle in Issue #14 neu eingefuehrten Tests

    Dokumentiere jeden Befehl und das Ergebnis. Nicht ausfuehrbare Hardware- oder Langzeittests als `BLOCKED` ausweisen; niemals als bestanden darstellen.

11. Pull Request
    Erstelle nur bei einer in sich vollstaendigen, geprueften Aenderung einen kleinen PR gegen `main`.
    Der PR enthaelt:
    - klare Zusammenfassung,
    - Scope und bewusst nicht enthaltene Punkte,
    - ausgefuehrte Tests mit Ergebnissen,
    - Ressourcenwirkung,
    - bekannte Einschraenkungen und BLOCKED-Punkte,
    - `Closes #14` nur bei vollstaendig erfuellter Definition of Done; sonst `Refs #14`.

    Den PR nicht selbst mergen. Nach Reviews neue Kommentare einzeln fachlich pruefen, nur berechtigte Punkte einarbeiten und danach die vollstaendige CI erneut abwarten.
```

## Spezifikationsquellen

- docs/STATE_MACHINE.md
- docs/PROGRAMS.md
- docs/STANDARD_PROGRAMS.md

## Definition of Done

Zustandsmaschine, Uebergangstests und Dokumentation abgeschlossen.

## Freigabeentscheidungen

- Schema 5 ergaenzt `maximumProductWaitMinutes`.
- Gueltiger Bereich: 1 bis 1.440 Minuten.
- Der Wert ist fuer ausfuehrbare Vorheizprogramme verpflichtend, ohne Vorheizen
  unzulaessig und in Katalogvorlagen optional.
- Schema 4 wird ohne erfundenen Wert migriert; migrierte Vorheizprogramme sind
  bis zur Konfiguration nicht ausfuehrbar.
- Alle entwicklerseitigen Programmmodell-Wertebereiche liegen ausschliesslich in
  `lib/fermentation_app/src/program_limits.hpp`.
- Die Eintrittsmeldung von `WAITING_FOR_PRODUCT` ist die Warnung vor Ablauf; es
  gibt keine zweite Warnschwelle.
- `MANUAL_HOLDING` wird erst nach optionalem Vorheizen, Zielerreichung und
  Zielqualifikation erreicht. #15 erstellt den manuellen Laufplan und
  Startbefehl.
- Der Automat ist deterministisch, hardware- und persistenzfrei. Er berechnet
  eine bestaetigungsbeduerftige Uebergangsentscheidung; erst der aufrufende
  Anwendungsteil darf sie nach erfolgreicher Persistenz anwenden.
- #14 definiert alle kanonischen Zustandsnamen und ihre grundsaetzliche
  Topologie. Detailpolitik der Folge-Issues wird nicht vorweggenommen.

## Vorgeschlagener Branch

`feat/issue-14-zustandsmaschine-und-prozessablaeufe-implementieren`
