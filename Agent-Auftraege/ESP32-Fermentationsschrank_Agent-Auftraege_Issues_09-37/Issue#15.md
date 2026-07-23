# Agent-Auftrag fuer Issue #15

## Issue

**[E1.4] Laufkommandos, Meldungen und Bedienaktionen implementieren**  
Aktueller Snapshot-Status: `READY`  
Epic: #3  
GitHub: https://github.com/ManuEngineer/ESP32-Fermentationsschrank/issues/15

> Der Status und Inhalt auf GitHub sind die aktuelle Wahrheit. Lies das Live-Issue
> vor jeder Arbeit erneut. Dieser Auftrag ist eine Arbeitsanweisung, kein Ersatz
> fuer das Issue.

## Fertiger Auftrag zum Kopieren

```text
Arbeite im Repository `ManuEngineer/ESP32-Fermentationsschrank` ausschliesslich
an Issue #15:
"[E1.4] Laufkommandos, Meldungen und Bedienaktionen implementieren"

Pruefe den aktuellen Live-Status des Issues. Beginne nur, wenn es `READY` ist,
alle Abhaengigkeiten abgeschlossen sind und kein offener Pull Request oder
Remote-Branch dasselbe Issue bearbeitet.

1. Sicheren Ausgangszustand herstellen
   - `git checkout main`
   - `git pull --ff-only`
   - `git fetch --prune`
   - `git status --short`
   - Bei lokalen Aenderungen, unklaren ungetrackten Dateien oder einem nicht
     sauberen Stand: anhalten und berichten. Nichts ungefragt verwerfen.
   - `config/hardware.yaml`, `config/pins.yaml`, Secrets, `.env`-Dateien und
     andere lokale Konfigurationen niemals einchecken.

2. Vor jeder Aenderung vollstaendig lesen
   - das aktuelle GitHub-Issue #15 inklusive Kommentare
   - `AGENTS.md`
   - alle fuer betroffene Unterverzeichnisse geltenden weiteren `AGENTS.md`
   - `docs/SPECIFICATION_REVIEW.md`, insbesondere die
     Dokumentationsprioritaet
   - `docs/DECISIONS.md`
   - `docs/RUN_COMMANDS.md`
   - `docs/STATE_MACHINE.md`
   - `docs/PROGRAMS.md`
   - `docs/RUNTIME_BEHAVIOR.md`
   - `docs/LOCAL_RUNTIME_UI.md`
   - `docs/TEMPERATURE_CONTROL.md`
   - `docs/SAFETY_AND_FAULTS.md`
   - den aktuellen Code und die Tests der abgeschlossenen Abhaengigkeiten

3. Abhaengigkeiten und Freigabe pruefen
   - #13 muss abgeschlossen sein.
   - #14 muss abgeschlossen sein.
   - Nicht auf Annahmen oder veraltete Issue-Snapshots vertrauen.
   - Ist eine Abhaengigkeit offen oder der Issue-Status nicht freigegeben,
     keinen Implementierungsbranch erstellen. Stattdessen Blocker, benoetigte
     Entscheidung und naechsten sinnvollen Schritt berichten.

4. Vor Codeaenderungen zuerst einen Plan vorlegen
   Berichte:
   - was im aktuellen Stand bereits vorhanden ist,
   - welche Akzeptanzkriterien noch fehlen,
   - welche Dateien voraussichtlich geaendert oder neu angelegt werden,
   - welche Tests und Nachweise vorgesehen sind,
   - welche Risiken, Architekturentscheidungen oder Spezifikationskonflikte
     bestehen.

   Bei einer echten neuen Architekturentscheidung, einem
   Spezifikationswiderspruch oder einer sicherheitsrelevanten Unklarheit
   anhalten und den Owner fragen. Normale Implementierungsdetails
   selbststaendig entscheiden.

5. Branch
   Nach Freigabe des Plans einen neuen Branch vom aktuellen `main` erstellen:
   `feat/issue-15-laufkommandos-meldungen-und-bedienaktionen-implement`

   Keine Aenderungen direkt auf `main`. Keine anderen Issues in diesen Branch
   aufnehmen.

6. Verbindliche Architektur
   - Die Kommandoschicht gehoert in `lib/fermentation_app/`.
   - Kein allgemeines Command-Framework in `device_platform` einfuehren.
   - Display und Web sind nur spaetere Adapter derselben fachlichen
     Kommandoschicht.
   - Keine direkte Abhaengigkeit von Arduino, GPIO, WLAN, Display, Web,
     Dateisystem, realer Systemzeit oder produktiver Persistenz.
   - Alle fachlichen Komponenten im Profil `native` deterministisch testbar
     halten.

7. Zweistufiges Entscheidungsmodell
   Alle lauf-, meldungs- und verriegelungswirksamen Kommandos verwenden:

   `Kommando validieren -> noch nicht angewendete CommandDecision erzeugen
    -> spaeter in #17 atomar persistieren -> bewusst anwenden`

   - Die Entscheidungsfunktion mutiert keinen Lauf, Zustand, Meldungsstatus und
     keine Verriegelung.
   - Eine Entscheidung ist bis zur Anwendung verwerfbar.
   - Bei spaeterem Persistenzfehler bleibt der bisherige Zustand wirksam.
   - Issue #15 implementiert keine produktive Persistenz.
   - Native Tests duerfen Entscheidungen ueber einen einfachen In-Memory-
     Testtreiber verwerfen oder anwenden.
   - Die bestehende direkte Laufanpassung aus #13 ist auf ein entsprechendes
     Entscheiden-und-Anwenden-Modell umzustellen, ohne die append-only
     Revisionssemantik zu verlieren.

8. Kommando-Umschlag, Konflikte und Idempotenz
   Ein Kommando besitzt mindestens:
   - eindeutige Kommando-ID,
   - Quelle,
   - monotonen Zeitbezug,
   - erwartete Zustandssequenz,
   - soweit relevant erwartete Lauf-, Meldungs- oder Fehlerrevision,
   - fachliche Eingaben,
   - bei kritischen Aktionen eine ausdrueckliche Bestaetigung.

   Verbindliche Konfliktregeln:
   - Display und Web sind gleichberechtigt; keine pauschale Quellenprioritaet.
   - Das erste gueltig angewendete Kommando gewinnt.
   - Ein Kommando auf veraltetem Stand wird eindeutig als Konflikt oder
     `StaleState` abgelehnt.
   - Abgelehnte Kommandos veraendern keine Daten.
   - Dieselbe Kommando-ID wird nicht doppelt ausgefuehrt.
   - Sicherheitsereignisse haben Vorrang vor Komfortkommandos.
   - Webtransport, Anmeldung und konkrete Wiederholungsmechanik bleiben #27.

9. Verbindlicher fachlicher Scope
   - Startzusammenfassung und bestaetigter Start
   - Erzeugung eines unveraenderlichen manuellen Laufplans
   - Stopoptionen: Abbruch/Aus, Abbruch/Kuehlen, Zurueck
   - Abschluss und Quittierung
   - `Quittieren`, `Stummschalten` und `Fehler zuruecksetzen` als getrennte
     Kommandos
   - Meldungsprioritaeten und akustische Absichten als fachliche Daten
   - Zieltemperatur- und Restdaueranpassung mit Vorschau/Bestaetigung
   - gleichzeitige Bedienaktionen konfliktfrei und idempotent behandeln

10. Manueller Laufplan
    Manuelles Temperaturhalten und `Abbrechen und kuehlen` verwenden denselben
    unveraenderlichen manuellen Laufplan mit mindestens:
    - Lauf-ID,
    - Zieltemperatur,
    - Sensorbetrieb,
    - Vorheizen EIN/AUS,
    - maximaler Produktwartezeit nur bei Vorheizen,
    - Zielband und Qualifikationsdauer,
    - Laufart `ManualHolding`,
    - Quelle und Erstellungszeit.

    Er enthaelt keine Fermentationsdauer. Der kanonische Zustandsweg aus
    `STATE_MACHINE.md` bleibt unveraendert. Kein eigener Zustand
    `MANUAL_COOLING`.

11. Stopoptionen
    - `Zurueck` verwirft nur den Dialog und veraendert nichts.
    - `Abbrechen und ausschalten` schlaegt atomar Abbruch, Ereignis und
      Uebergang nach `STANDBY` vor. Konkrete Aktor- und Luefternachlaufsteuerung
      bleibt ausserhalb von #15.
    - `Abbrechen und kuehlen` ist eine unteilbare CommandDecision aus:
      1. bisherigen Lauf abbrechen,
      2. Abbruch protokollierbar machen,
      3. neuen validierten manuellen Laufplan mit eigenem Schnappschuss
         erzeugen,
      4. neuen manuellen Lauf starten.
    - Der alte Lauf darf nicht bereits beendet werden, wenn der neue Laufplan
      ungueltig ist oder nicht erzeugt werden kann.

12. Laufanpassungen
    - Quellprogramm und Programmschnappschuss bleiben unveraendert.
    - Vorschau zeigt alten/neuen Wert, Phase, erkennbare Wirkung und notwendige
      Hinweise.
    - Bestaetigte Aenderungen erzeugen eine protokollierbare append-only
      Laufrevision.
    - Zielaenderung in `PREHEATING`, `REACHING_TARGET` oder
      `QUALIFYING_TARGET` bewertet Zielerreichung und Zielqualifikation neu.
    - Zielaenderung waehrend `FERMENTING`:
      - Zustand bleibt `FERMENTING`,
      - Restdauer laeuft weiter,
      - keine blockierende Neuqualifikation,
      - keine automatische biologische Zeitkorrektur,
      - Vorschau weist darauf ausdruecklich hin.
    - Reine Restdaueranpassung loest keine Neuqualifikation aus.
    - Restdauer null bleibt gemaess bestehender Laufsemantik zulaessig.
    - Fermentationsziel und Fermentationsrestzeit sind in unpassenden Phasen
      abzulehnen, insbesondere waehrend Kuehlen, Halten, Abschluss, Fehler,
      Recovery und Service.
    - Eine Zielwertanpassung eines bereits laufenden manuellen Haltebetriebs ist
      nicht Scope von #15. Der Lauf wird beendet und mit neuem bestaetigten
      Laufplan gestartet.

13. Meldungen
    Meldungen bleiben fachliche, hardwarefreie Daten mit mindestens:
    - stabilem Meldungscode,
    - Klasse und Prioritaet,
    - Quelle beziehungsweise Ausloeser,
    - monotonem Zeitbezug,
    - Aktiv-, quittiert- und erledigt-Status,
    - Entscheidungsanforderung,
    - abstrakter akustischer Absicht,
    - soweit relevant Lauf-, Zustands- oder Fehlerreferenz.

    Keine konkreten Displaytexte, Summerfrequenzen oder GPIO-Signale in #15.

14. Quittieren, Stummschalten und Fehlerreset
    - `Quittieren` bestaetigt nur die Wahrnehmung und loest keine Verriegelung.
    - `Stummschalten` beeinflusst nur die akustische Wiederholung.
    - Eine Quittierung der Hauptmeldung quittiert nicht automatisch andere
      Meldungen.
    - `Fehler zuruecksetzen` verwendet eine aktuelle, bereits qualifizierte
      Resetfreigabebewertung der spaeteren Sicherheitslogik.
    - Ohne positive und aktuelle Freigabe wird der Reset abgelehnt.
    - #15 liest dafuer keine realen Sensoren, kennt keine konkrete Service-PIN
      und erfindet keine Fehlercode-Matrix.
    - Die konkrete Resetpolitik und Erzeugung der Bewertung bleiben #24.

15. Akzeptanzkriterien
    - Kommandos sind atomar und zustandsabhaengig validiert.
    - Entscheidungen bleiben bis zur Anwendung reversibel.
    - Konfliktierende und wiederholte Kommandos liefern eindeutige Ergebnisse.
    - Quittierung und Stummschaltung loesen keine Verriegelung.
    - Stoppen fuehrt nie zu verbotener Aktorwiederherstellung.
    - `Abbrechen und kuehlen` kann nicht als halbe fachliche Aktion angewendet
      werden.
    - Laufanpassungen erzeugen nachvollziehbare Revisionen.
    - Zielaenderung waehrend `FERMENTING` unterbricht den Timer nicht und
      erfindet keine Zeitkorrektur.
    - Die Kommandoschicht bleibt hardware-, transport- und persistenzfrei.

16. Issue-spezifische Tests
    Mindestens:
    - bestaetigter und nicht bestaetigter Start,
    - ungueltige und veraltete Startdaten,
    - alle Stopoptionen,
    - manueller Laufplan mit und ohne Vorheizen,
    - Berechnen, Verwerfen und Anwenden von Entscheidungen,
    - Display-/Web-Konflikt ohne Quellenprioritaet,
    - wiederholte Kommando-ID,
    - veraltete Zustands-, Lauf-, Meldungs- und Fehlerrevisionen,
    - Laufanpassung in mehreren Phasen,
    - weiterlaufender Timer bei Zielaenderung in `FERMENTING`,
    - Quittierung, Stummschaltung und Reset als getrennte Aktionen,
    - erlaubte und abgelehnte deterministische Resetfreigabebewertungen,
    - keine direkte Persistenz, Hardwarewirkung oder Display-/Web-Abhaengigkeit.

17. Ausdruecklicher Nicht-Scope
    - produktive atomare Persistenz und Kontrollpunkte (#17)
    - temperaturgewichteter Fortschritt und Wiederanlaufkorrekturen (#18)
    - konkrete Fehlercode-, Verriegelungs- und Resetpolitik (#24)
    - gemeinsame UI-Modelle und reale Bedienoberflaechen (#25/#26)
    - Webtransport, Anmeldung und konkrete API (#27)
    - GPIO-, Treiber-, Aktor- oder Summerimplementierung
    - Hardware- und Inbetriebnahmewerte

18. Zwingende Projektregeln
    - Arbeite software-first und halte alle fachlichen Komponenten nativ
      testbar.
    - `TBD_HARDWARE` und `TBD_COMMISSIONING` bleiben Platzhalter und duerfen
      nicht als bestaetigte Werte verwendet werden.
    - `src/main.cpp` bleibt Composition Root ohne Prozesslogik.
    - Bestehende Sicherheitsinvarianten niemals abschwaechen.
    - Keine neue grosse Abhaengigkeit ohne konkrete Begruendung,
      Ressourcenwirkung und Alternativenvergleich.
    - Keine parallele zweite Architektur oder doppelte Quelle der Wahrheit.
    - Dokumentation und Changelog mit der Implementierung synchron halten.
    - Keine Tests deaktivieren, um CI gruen zu machen.

19. Mindestpruefungen vor einem PR
    Fuehre alle fuer den Scope relevanten Pruefungen aus, soweit anwendbar
    mindestens:
    - `pio run -e native -e esp32_bringup -e esp32_release`
    - `pio test -e native`
    - `python scripts/check_platformio_config.py`
    - Format-, Compilerwarnungs-, Static-Analysis-, Architektur- und
      Secret-Pruefungen aus dem aktuellen Repository
    - alle in Issue #15 neu eingefuehrten Tests

    Dokumentiere jeden Befehl und das Ergebnis. Nicht ausfuehrbare Hardware-
    oder Langzeittests als nicht ausgefuehrt beziehungsweise `BLOCKED`
    ausweisen; niemals als bestanden darstellen.

20. Pull Request
    Erstelle nur bei einer in sich vollstaendigen, geprueften Aenderung einen
    kleinen PR gegen `main`.

    Der PR enthaelt:
    - klare Zusammenfassung,
    - Scope und bewusst nicht enthaltene Punkte,
    - ausgefuehrte Tests mit Ergebnissen,
    - Ressourcenwirkung,
    - bekannte Einschraenkungen und BLOCKED-Punkte,
    - relevante Sicherheits- und Architekturhinweise,
    - `Closes #15`.

    Den PR nicht selbst mergen. Nach Reviews neue Kommentare einzeln fachlich
    pruefen, nur berechtigte Punkte einarbeiten und danach die vollstaendige CI
    erneut abwarten.

21. Nach dem PR anhalten
    - nicht mergen und kein Auto-Merge aktivieren,
    - Branch nicht loeschen,
    - Issue nicht manuell schliessen,
    - nicht mit #16 oder einem anderen Issue beginnen,
    - keine weiteren Repository-Aenderungen durchfuehren.
```

## Spezifikationsquellen

- `docs/RUN_COMMANDS.md`
- `docs/STATE_MACHINE.md`
- `docs/PROGRAMS.md`
- `docs/RUNTIME_BEHAVIOR.md`
- `docs/LOCAL_RUNTIME_UI.md`
- `docs/TEMPERATURE_CONTROL.md`
- `docs/SAFETY_AND_FAULTS.md`

## Definition of Done

Kommandos, Ergebnisse, Konflikt- und Idempotenzregeln, Meldungsmodell, Tests und
Dokumentation abgeschlossen.

## Vorgeschlagener Branch

`feat/issue-15-laufkommandos-meldungen-und-bedienaktionen-implement`
