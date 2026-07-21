# Betriebszustaende und Zustandsmaschine

## Status

Dieses Dokument beschreibt den akzeptierten Grundaufbau der Betriebszustaende.
Die Detailbedingungen fuer Regelung, Fehlerklassen, Persistenz und Zeitmodelle
werden in den zugehoerigen Spezifikationsdokumenten vervollstaendigt.

Ergaenzende Wiederanlaufregeln stehen in
[`RECOVERY_AND_INTERRUPTION.md`](RECOVERY_AND_INTERRUPTION.md).

## Grundsaetze

- Jeder Zustand hat einen klar definierten Zweck.
- Aktoren duerfen nur in Zustaenden aktiv sein, in denen dies ausdruecklich
  vorgesehen ist.
- Ein Zustandswechsel muss atomar, protokollierbar und nachvollziehbar sein.
- Es gibt keine allgemeine Pausenfunktion fuer laufende Fermentationen.
- Warnungen und Fehler werden getrennt behandelt.
- Ein manueller Lauf verwendet dieselben Sicherheits- und Regelmechanismen wie
  ein gespeichertes Programm.
- Direkte Aktorsteuerung ist ausschliesslich im geschuetzten Servicemodus
  zulaessig.
- Ein Wiederanlauf nach Stromausfall wartet nicht blockierend auf den Benutzer.
- Ein echter Sicherheitsfehler hat immer Vorrang vor automatischem Fortfahren.

## Hauptzustaende

```text
BOOT
  -> STANDBY
  -> optional PREHEATING
  -> optional WAITING_FOR_PRODUCT
  -> REACHING_TARGET
  -> QUALIFYING_TARGET
  -> FERMENTING
  -> optional COOLING
  -> optional COOL_HOLDING
  -> COMPLETED
  -> STANDBY
```

Besondere Zustaende und Kontexte:

```text
RECOVERY_EVALUATION
RECOVERY_TIME_PENDING
WARNING_REQUIRES_ACTION
FAULT
SERVICE_MODE
MANUAL_HOLDING
```

`RECOVERY_TIME_PENDING` ist kein eigener blockierender Betriebszustand, sondern
ein zusaetzlicher Kontext eines bereits wieder laufenden Prozesszustands.

## BOOT

### Zweck

Der ESP32 startet in einen sicheren Zustand und prueft, ob ein normaler
Standby-Start oder die Wiederaufnahme eines unterbrochenen Laufes erforderlich
ist.

### Verhalten

- Peltier AUS
- alle schaltbaren Ausgaenge zunaechst AUS
- gespeicherte Konfiguration laden und validieren
- Sensoren erkennen und Grundplausibilitaet pruefen
- gespeicherten Laufzustand pruefen
- Reset- und Startursache protokollieren, soweit technisch moeglich
- Netzwerkverbindung und Zeitabgleich parallel vorbereiten

### Uebergaenge

```text
kein laufender gespeicherter Prozess
  -> STANDBY

gueltiger unterbrochener Prozess vorhanden
  -> RECOVERY_EVALUATION

kritischer Initialisierungsfehler
  -> FAULT
```

Es gibt keine zusaetzliche allgemeine Startbestaetigung nach jedem Einschalten.
Ein normaler sicherer Start fuehrt automatisch zu `STANDBY`.

## STANDBY

### Zweck

Sicherer Ruhezustand ohne laufenden Prozess.

### Aktoren

- Peltier AUS
- Aussenluefter AUS
- Innenluefter standardmaessig AUS
- Summer AUS

### Verfuegbare Funktionen

- Temperaturen und Sensorstatus anzeigen
- Programme auswaehlen und bearbeiten
- neuen Programmlauf starten
- manuellen Zeit-/Temperaturlauf starten
- manuellen Temperatur-Haltebetrieb starten
- Servicemodus oeffnen
- Einstellungen, Diagnose und Netzwerkfunktionen verwenden

## PREHEATING

### Zweck

Leeren Schrank vor dem Einsetzen des Produkts auf die Programmsolltemperatur
bringen. Je nach Ausgangslage kann dies Heizen oder Kuehlen bedeuten.

### Ende

```text
Ziel des leeren Schrankes erfolgreich qualifiziert
  -> WAITING_FOR_PRODUCT
```

Nach einem Stromausfall wird die Vorheizphase nur wieder aufgenommen, wenn der
gespeicherte Lauf gueltig ist und die maximale Wartezeit noch nicht abgelaufen
ist.

## WAITING_FOR_PRODUCT

### Zweck

Der Schrank haelt die vorbereitete Temperatur und wartet darauf, dass der
Benutzer das Produkt einsetzt und dies bewusst bestaetigt.

### Verhalten

- Anzeige `Produkt einsetzen`
- akustisches, quittierbares Signal
- Temperaturregelung bleibt aktiv, soweit sicher und sinnvoll
- Fermentationstimer laeuft nicht
- pro Programm konfigurierbare maximale Wartezeit

### Uebergaenge

```text
Benutzer drueckt WEITER / START
  -> REACHING_TARGET

maximale Wartezeit abgelaufen
  -> Vorheizen sicher beenden
  -> Lauf als nicht gestartet oder abgebrochen protokollieren
  -> STANDBY
```

Ein automatischer Start ohne die zweite Bestaetigung ist nicht zulaessig. Auch
nach einem Stromausfall darf nicht angenommen werden, dass das Produkt bereits
eingesetzt wurde.

## REACHING_TARGET

### Zweck

Die fuer den Lauf massgebende Temperatur auf den Sollwert bringen.

Moegliche Aktionsrichtung:

- Heizen
- Kuehlen
- Temperatur halten, falls bereits im Zielbereich

### Uebergaenge

```text
Zielbereich erreicht
  -> QUALIFYING_TARGET

maximale Zielerreichungszeit ueberschritten
  -> Warnung; Zustand laeuft standardmaessig weiter

kritischer Sensor- oder Sicherheitsfehler
  -> FAULT
```

## QUALIFYING_TARGET

### Zweck

Pruefen, ob der Sollwert nicht nur kurz durchlaufen, sondern fuer die
festgelegte Zeit ausreichend stabil erreicht wurde.

Die Zielqualifikation ist nicht Teil der Fermentationszeit.

### Uebergaenge

```text
Ziel erfolgreich qualifiziert
  -> FERMENTING

laengere oder deutliche Abweichung
  -> REACHING_TARGET oder Neustart der Qualifikation

kritischer Fehler
  -> FAULT
```

Nach einem Stromausfall beginnt eine zuvor nur teilweise absolvierte
Zielqualifikation neu.

## FERMENTING

### Zweck

Eigentliche temperaturgefuehrte Fermentationsphase mit laufendem Timer.

### Regeln

- Eine allgemeine Pausenfunktion existiert nicht.
- Die biologische Fermentation laeuft auch bei tieferer Temperatur weiter,
  normalerweise langsamer.
- Kurze oder moderate Temperaturabweichungen lassen den Timer standardmaessig
  weiterlaufen.
- Warnungen koennen angezeigt und protokolliert werden, ohne den Lauf
  automatisch zu stoppen.
- Harte Fehler oder Sicherheitsgrenzen fuehren zu `FAULT`.

### Normale Uebergaenge

```text
Fermentationszeit abgelaufen, Abschluss ohne Kuehlung
  -> COMPLETED

Fermentationszeit abgelaufen, aktives Kuehlen vorgesehen
  -> COOLING
```

### Wiederanlauf

Nach einem Stromausfall wird die Temperaturregelung nach gueltigem Sensor- und
Sicherheitscheck automatisch wieder aufgenommen. Der Lauf wartet nicht auf eine
Benutzerbestaetigung.

Die verbleibende Dauer wird spaeter anhand der Unterbrechungsdauer und der
Temperaturschaetzung korrigiert. Solange die Netzwerkzeit noch fehlt, bleibt der
Kontext `RECOVERY_TIME_PENDING` aktiv.

## COOLING

### Zweck

Produkt beziehungsweise Schrank nach der Fermentation aktiv auf die
konfigurierte Kuehlzieltemperatur bringen.

### Uebergaenge

```text
Kuehlziel erreicht, danach beenden
  -> COMPLETED

Kuehlziel erreicht, danach zeitlich begrenzt halten
  -> COOL_HOLDING

Kuehlziel erreicht, bis manuell beendet halten
  -> COOL_HOLDING

kritischer Fehler
  -> FAULT
```

Nach einem Stromausfall wird eine gueltige Kuehlphase automatisch wieder
aufgenommen. Sie wartet nicht auf eine Benutzerentscheidung.

## COOL_HOLDING

### Zweck

Kuehltemperatur nach Erreichen des Kuehlziels halten.

Moegliche Varianten:

- fuer eine festgelegte Dauer
- bis zur manuellen Beendigung

### Uebergaenge

```text
Haltezeit abgelaufen
  -> COMPLETED

Benutzer beendet Halten
  -> COMPLETED

kritischer Fehler
  -> FAULT
```

Nach einem Stromausfall wird das Kuehlhalten bei gueltigen Sensoren automatisch
wieder aufgenommen. Eine zeitlich begrenzte Haltephase wird nach verfuegbarer
Zeitbasis korrigiert.

## MANUAL_HOLDING

### Zweck

Eine manuell gewaehlte Zieltemperatur ohne Timer halten.

Der Zustand verwendet dieselbe Regel- und Sicherheitslogik wie ein
Programmlauf und endet erst durch bewusste Benutzeraktion oder einen Fehler.
Nach einem Stromausfall wird er nach erfolgreicher Wiederanlaufpruefung
automatisch fortgesetzt.

## COMPLETED

### Zweck

Lauf ist fachlich abgeschlossen und wartet auf Benutzerquittierung.

### Verhalten

- Peltier AUS
- definierter Luefternachlauf
- optische Meldung `PROGRAMM BEENDET`
- kurzes akustisches Signalmuster
- Summer laeuft nicht dauerhaft
- abgeschlossener Lauf und Ergebnisdaten bleiben sichtbar

### Uebergang

```text
Benutzer quittiert
  -> STANDBY
```

Das Geraet wechselt nicht automatisch aus `COMPLETED` nach `STANDBY`. Nach
einem Stromausfall wird der bereits abgeschlossene Zustand wieder angezeigt;
es wird keine Regelphase neu gestartet.

## Manuelles Stoppen eines Laufes

Es gibt keine Pause, aber ein laufender Prozess kann bewusst abgebrochen
werden.

Nach `STOP` zeigt die Bedienoberflaeche:

```text
[Abbrechen und ausschalten]
[Abbrechen und kuehlen]
[Zurueck]
```

### Abbrechen und ausschalten

- laufenden Prozess als abgebrochen markieren
- Peltier AUS
- definierter Luefternachlauf
- Abbruch protokollieren
- danach `STANDBY`

### Abbrechen und kuehlen

- urspruenglichen Prozess als abgebrochen markieren
- Benutzer bestaetigt Kuehlziel und Abschlussverhalten
- neuer, expliziter manueller Kuehllauf wird gestartet
- technischer Uebergang in `REACHING_TARGET` beziehungsweise `COOLING`

`Abbrechen und kuehlen` ist keine Fortsetzung des urspruenglichen Programms,
sondern ein neuer Lauf mit eigenem Schnappschuss und eigener Protokollierung.

## Warnungen

Warnungen bedeuten, dass der Prozess grundsaetzlich weiterlaufen darf.

Beispiele:

- erwartete Zielerreichungszeit ueberschritten
- Temperatur zeitweise ausserhalb des normalen Arbeitsbereichs
- Kuehlziel wird langsamer als erwartet erreicht
- Netzwerkzeit nach Wiederanlauf noch nicht verfuegbar
- nichtkritische Netzwerk- oder Oberflaechenfunktion ausgefallen

Standardverhalten:

- Lauf geht weiter
- Warnung wird sichtbar angezeigt
- Ereignis wird protokolliert
- je nach Warnung akustisches Signal

Falls eine Warnung zwingend eine Entscheidung benoetigt, kann der fachliche
Unterzustand `WARNING_REQUIRES_ACTION` verwendet werden. Die Sicherheitslogik
legt fest, welche Aktoren waehrenddessen weiterlaufen duerfen.

Ein fehlender Produktfuehler in einem produktgefuehrten Lauf ist ein Beispiel,
bei dem nicht still auf einen anderen Sensor gewechselt werden darf.

## FAULT

Fehler bedeuten, dass der Prozess nicht normal weiterlaufen darf.

Beispiele:

- primaerer Prozesssensor ausgefallen oder ungueltig
- harte Ueber- oder Untertemperaturgrenze verletzt
- unzulaessige oder widerspruechliche H-Bruecken-Anforderung
- kritischer interner Software- oder Persistenzfehler
- sichere Ausgangszustaende nicht gewaehrleistet

### Verhalten

- Peltier unverzueglich sicher deaktivieren
- Luefter nur gemaess spaeter festgelegter Fehlerstrategie betreiben
- Fehlermeldung und Fehlercode anzeigen
- akustisches Fehlersignal
- Lauf als fehlerhaft markieren und protokollieren
- kein automatisches Zurueckkehren in den Prozess ohne ausdruecklich
  spezifizierte Wiederanlaufregel

Die konkrete Klassifikation und Reaktion wird in `SAFETY_AND_FAULTS.md`
festgelegt.

## RECOVERY_EVALUATION

### Zweck

Nach Boot unverzueglich bestimmen, wie ein unterbrochener Lauf sicher und
fachlich sinnvoll weitergefuehrt wird.

Zu bewerten sind mindestens:

- Persistenzdaten und Programmschnappschuss gueltig
- Art und Phase des unterbrochenen Laufes
- aktuelle Sensorwerte
- letzte gespeicherte Produkt- oder Schranklufttemperatur
- aktuelle Produkt- oder Schranklufttemperatur
- aktive Fehler oder Warnungen
- Verfuegbarkeit einer verlaesslichen Zeitbasis

### Grundsatz

`RECOVERY_EVALUATION` ist kurz und blockiert nicht bis zur Benutzerentscheidung
oder bis zur Netzwerkzeit. Es wird unmittelbar eine sichere phasenbezogene
Aktion ausgewaehlt.

### Uebergaenge

```text
Fortsetzung technisch und sicher zulaessig
  -> geeigneten normalen Prozesszustand automatisch wieder aufnehmen

Netzwerkzeit noch nicht verfuegbar
  -> geeigneten normalen Prozesszustand automatisch wieder aufnehmen
  -> Kontext RECOVERY_TIME_PENDING setzen

Fortsetzung wegen Sicherheitsfehler nicht zulaessig
  -> FAULT

Persistenz unbrauchbar oder kein fachlicher Lauf rekonstruierbar
  -> sicherer Fehler- oder Standby-Zustand gemaess Fehlerklasse
```

## RECOVERY_TIME_PENDING

### Zweck

Kennzeichnen, dass der Prozess bereits sicher weiterlaeuft, die genaue
Unterbrechungsdauer und Restzeitkorrektur aber noch nicht bestimmt sind.

### Verhalten

- Netzwerk und NTP-Zeitabgleich laufen im Hintergrund
- Oberflaeche zeigt `Unterbrechungsdauer wird bestimmt`
- keine ungesicherte exakte Restzeit behaupten
- normale phasenbezogene Regelung fortsetzen

### Ende

```text
Netzwerkzeit verfuegbar
  -> Unterbrechungsdauer bestimmen
  -> Restzeit oder Haltezeit korrigieren
  -> Korrektur anzeigen und protokollieren
  -> Kontext RECOVERY_TIME_PENDING entfernen

Netzwerkzeit bleibt zu lange unbekannt
  -> konservative programmspezifische Ersatzlogik anwenden
  -> niedrige Vertrauensstufe anzeigen
  -> Prozess weiterfuehren, sofern sicher
```

## Zeitquelle

Im ersten Release ist Netzwerkzeit die primaere Zeitquelle. Der ESP32 startet
jedoch schneller als Router und NTP-Verbindung. Deshalb beginnt der
phasenbezogene Wiederanlauf sofort und die Zeitkorrektur erfolgt nachtraeglich.

Die Architektur soll spaeter eine batteriegepufferte Echtzeituhr, zum Beispiel
ein DS3231-Modul, als alternative oder zusaetzliche Quelle ermoeglichen. Ein
RTC-Modul ist fuer das erste Release nicht erforderlich.

## SERVICE_MODE

### Eintritt

Der Servicemodus darf nur aus `STANDBY` betreten werden.

### Regeln

- kein laufendes Programm
- deutlicher Warnhinweis vor Aktortests
- Peltier- und Lueftertests zeitlich begrenzen
- Richtungswechsel und Totzeiten bleiben erzwungen
- Sensor- und Sicherheitspruefungen bleiben aktiv
- Fehler beendet den aktiven Test sofort
- beim Verlassen alle Ausgaenge AUS
- danach Rueckkehr zu `STANDBY`

Der Servicemodus darf waehrend eines laufenden Programms nicht geoeffnet
werden.

## Manuelle Betriebsarten

Es werden zwei getrennte manuelle Betriebsarten vorgesehen.

### Manueller Zeit-/Temperaturlauf

Der Benutzer gibt mindestens an:

- Zieltemperatur
- Dauer
- optional Vorheizen
- Sensorbetrieb
- Abschlussverhalten

Dieser Modus wird technisch wie ein temporaeres Programm behandelt und nutzt
dieselben Zustaende:

```text
optional PREHEATING
  -> optional WAITING_FOR_PRODUCT
  -> REACHING_TARGET
  -> QUALIFYING_TARGET
  -> FERMENTING
  -> gewaehlte Abschlussphase
```

### Manueller Temperatur-Haltebetrieb

Zusaetzlich ist ein Betrieb ohne Timer vorgesehen.

Der Benutzer gibt mindestens an:

- Zieltemperatur
- Sensorbetrieb
- optional Vorheizen

Ablauf:

```text
optional PREHEATING
  -> optional WAITING_FOR_PRODUCT
  -> REACHING_TARGET
  -> QUALIFYING_TARGET
  -> MANUAL_HOLDING
```

Dieser Modus ist keine direkte Aktorsteuerung und verwendet weiterhin alle
normalen Sicherheits-, Sensor- und Regelmechanismen.

## Akzeptierte Entscheidungen

- [x] normaler Boot fuehrt nach Selbsttest automatisch zu `STANDBY`
- [x] laufender gespeicherter Prozess wird bei Boot separat bewertet
- [x] im Standby sind Peltier und Luefter standardmaessig AUS
- [x] keine allgemeine Pausenfunktion
- [x] Stop bietet `Abbrechen und ausschalten` sowie `Abbrechen und kuehlen`
- [x] `COMPLETED` bleibt bis zur Benutzerquittierung aktiv
- [x] Warnungen lassen den Prozess standardmaessig weiterlaufen
- [x] Fehler stoppen den normalen Prozess
- [x] Servicemodus nur aus `STANDBY`
- [x] manueller Zeit-/Temperaturlauf als temporaeres Programm
- [x] zusaetzlicher manueller Temperatur-Haltebetrieb ohne Timer
- [x] kein Tuerkontakt im ersten Release; spaetere Erweiterung vorgesehen
- [x] Produktfuehlerausfall fuehrt nicht zu stillem Sensorwechsel
- [x] Wiederanlauf nach Stromausfall wartet nicht auf den Benutzer
- [x] Fermentations-, Kuehl- und Haltephasen werden phasenbezogen automatisch
      weitergefuehrt
- [x] Netzwerkzeit wird nach dem Wiederanlauf parallel beschafft
- [x] spaetere RTC-Unterstuetzung bleibt moeglich

## Noch offen

- Luefternachlauf je Zustand
- Verhalten bei ausstehender Entscheidung in `WARNING_REQUIRES_ACTION`
- Persistenzzeitpunkte und Wiederherstellung der Restzeit
- Prioritaet gleichzeitiger Warnungen und Fehler
- konkrete konservative Zeitkompensation bei unbekannter Unterbrechungsdauer
- genaue Fehlerklassen und Fortsetzungsbedingungen
