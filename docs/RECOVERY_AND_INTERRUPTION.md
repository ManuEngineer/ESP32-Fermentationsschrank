# Unterbrechungen und Wiederanlauf

## Status

Dieses Dokument ergaenzt [`STATE_MACHINE.md`](STATE_MACHINE.md) um die in
Phase 3B akzeptierten Sonderfaelle. Exakte Zeitgrenzen, Temperaturmodelle und
Fehlerklassen werden spaeter in `TEMPERATURE_CONTROL.md`,
`SAFETY_AND_FAULTS.md` und `SETTINGS_AND_STORAGE.md` vervollstaendigt.

## Kein Tuerkontakt im ersten Release

Im ersten Release wird kein Tuerkontakt eingebaut.

Folgen:

- Die Software kann eine geoeffnete Tuer nicht direkt erkennen.
- Temperaturabweichungen durch eine geoeffnete Tuer werden wie andere
  Temperaturabweichungen behandelt.
- Es gibt keinen tuergesteuerten Timerstopp und keine tuergesteuerte
  Peltierabschaltung.
- Bedienungsanweisungen duerfen nicht behaupten, die Tuerstellung zu kennen.

Die Softwarearchitektur soll optional ein spaeteres Ereignis `door_open`
beziehungsweise `door_closed` aufnehmen koennen. Solange kein Tuerkontakt in
der bestaetigten Hardwarekonfiguration vorhanden ist, bleibt diese Funktion
deaktiviert und hat keinerlei Einfluss auf den Ablauf.

Falls spaeter ein Tuerkontakt nachgeruestet wird, ist als Standardverhalten
vorgesehen:

- Peltier bei offener Tuer AUS
- Fermentationstimer laeuft weiter
- sichtbare Meldung `Tuer offen`
- nach laengerer Tueröffnung Warnung und akustisches Signal
- nach dem Schliessen normale Temperaturregelung fortsetzen

## Ausfall oder Entfernen des Produktfuehlers

Ein produktgefuehrter Lauf darf nicht still und automatisch auf den
Luftfuehler wechseln.

Bei Ausfall, Entfernen oder ungueltigen Messwerten des Produktfuehlers:

1. Peltier vorlaeufig sicher AUS.
2. Der Lauf wechselt in eine entscheidungspflichtige Warnung.
3. Der Luftfuehler und alle weiteren Sicherheitsbedingungen werden geprueft.
4. Nur zulaessige Aktionen werden angeboten.

Moegliche Aktionen:

- `Mit Luftfuehler fortsetzen`, falls der Luftfuehler gueltig ist
- `Abbrechen und ausschalten`
- `Abbrechen und kuehlen`, falls Kuehlen sicher zulaessig ist

Bei einer Fortsetzung mit Luftfuehler:

- der Sensorwechsel wird deutlich angezeigt
- der Wechsel wird mit Zeit und Messwerten protokolliert
- der Lauf bleibt derselbe Programmschnappschuss, erhaelt aber einen
  dokumentierten Wechsel des Regelmodus
- die spaetere Bewertung der Temperaturfuehrung beruecksichtigt die geringere
  Aussagekraft gegenueber einer direkten Produktmessung

Die genaue Behandlung der Zeit, die bis zur Benutzerentscheidung vergeht, wird
zusammen mit der Unterbrechungskompensation festgelegt.

## Maximale Wartezeit nach dem Vorheizen

Der Zustand `WAITING_FOR_PRODUCT` besitzt eine pro Programm konfigurierbare
maximale Wartezeit.

Vorgesehener Ablauf:

1. Der leere Schrank haelt die vorbereitete Temperatur.
2. Vor Ablauf der Maximalzeit erfolgt eine sichtbare und akustische Warnung.
3. Wird die Maximalzeit erreicht, beginnt die Fermentation nicht automatisch.
4. Die Vorheizphase wird sicher beendet und der Lauf als nicht gestartet
   beziehungsweise abgebrochen protokolliert.
5. Der Benutzer kann den Programmlauf anschliessend neu starten.

Die konkreten Warte- und Vorwarnzeiten bleiben `TBD_COMMISSIONING`.

## Fehlerquittierung nach Fehlerklasse

Die Rueckkehr aus `FAULT` ist von der Fehlerklasse abhaengig.

### Potenziell fortsetzbare Fehler

Beispiele:

- Produktfuehler voruebergehend entfernt und danach wieder gueltig
- kurzzeitiger nichtkritischer Kommunikationsfehler
- voruebergehend ungueltiger Messwert, der sich eindeutig erholt hat

Eine Fortsetzung darf nur angeboten werden, wenn Sensoren, Aktoren und
Sicherheitsbedingungen erneut vollstaendig geprueft wurden.

### Laufbeendende Fehler

Beispiele:

- harte Ueber- oder Untertemperaturgrenze verletzt
- widerspruechliche H-Bruecken-Anforderung
- nicht gewaehrleisteter sicherer Ausgangszustand
- beschaedigte oder widerspruechliche Persistenzdaten
- kritischer interner Softwarefehler

Bei diesen Fehlern fuehrt eine Quittierung nur in einen sicheren Zustand. Der
urspruengliche Lauf wird nicht normal fortgesetzt.

Die verbindliche Fehlerklassifikation folgt in `SAFETY_AND_FAULTS.md`.

## Verhalten in RECOVERY_DECISION

Nach einer langen oder nicht sicher bewertbaren Unterbrechung wird nicht blind
der letzte Aktorzustand wiederhergestellt.

Die waehrend `RECOVERY_DECISION` zulaessige Standardaktion richtet sich nach:

- unterbrochenem Programm
- unterbrochener Prozessphase
- gueltigen Sensoren
- aktueller Temperatur
- Dauer und Unsicherheit der Unterbrechung
- Sicherheits- und Fehlerstatus

Grundsaetze:

- Heizen wird nicht allein deshalb fortgesetzt, weil vor dem Stromausfall
  geheizt wurde.
- Eine bereits laufende Kuehl- oder Kuehlhaltephase darf bei gueltigen Sensoren
  eher wieder aufgenommen werden als eine unsichere Heizphase.
- Ein Programm kann als sichere Alternative `Abbrechen und kuehlen` anbieten.
- Sind Dauer oder Temperaturverlauf nicht verlaesslich bestimmbar, ist eine
  Benutzerentscheidung erforderlich.
- Jede angebotene Aktion wird zuerst durch die Sicherheitslogik freigegeben.

## Biologische Wirkung einer Stromunterbrechung

Eine Fermentation stoppt bei sinkender Temperatur nicht schlagartig. Sie laeuft
in der Regel langsamer weiter. Deshalb sind weder ein vollstaendiges Anrechnen
der Stromausfallzeit noch ein vollstaendiges Pausieren des Timers allgemein
korrekt.

Das Ziel ist eine **temperaturgewichtete Unterbrechungskompensation**:

- Zeit nahe der Solltemperatur zaehlt weitgehend als wirksame
  Fermentationszeit.
- Zeit deutlich unter der Solltemperatur zaehlt nur teilweise.
- Daraus wird eine erforderliche Verlaengerung der verbleibenden Laufzeit
  abgeleitet.

Konzeptionell wird die wirksame Fermentationszeit als temperaturabhaengige
Groesse behandelt:

```text
wirksamer Fortschritt = Summe aus Zeitabschnitten * Aktivitaetsfaktor(Temperatur)
```

Der Aktivitaetsfaktor ist bei der Solltemperatur ungefaehr `1`. Bei tieferen
Temperaturen liegt er darunter. Die konkrete Funktion darf nicht ohne
praktische Grundlage erfunden werden, weil sie von Kultur und Prozess abhaengt.

## Verfuegbare Temperaturgrundlage

### Produktgefuehrter Lauf

Ist nach dem Neustart ein gueltiger Produktfuehler vorhanden, hat dessen
Temperatur fuer die Bewertung Vorrang.

### Luftgefuehrter Lauf

Ohne Produktfuehler wird die Schranklufttemperatur verwendet. Die daraus
berechnete Kompensation besitzt eine geringere Sicherheit, weil die
Produkttemperatur traeger reagieren kann als die Lufttemperatur.

### Raumtemperatur

Eine separate Raumtemperaturmessung ist derzeit nicht vorgesehen. Sie kann
spaeter als zusaetzlicher Eingang fuer ein thermisches Modell ergaenzt werden.
Die Schranklufttemperatur nach dem Neustart ist nicht automatisch mit der
Raumtemperatur gleichzusetzen.

## Fehlende Messwerte waehrend des Stromausfalls

Der ESP32 und seine Temperatursensoren sind waehrend des Stromausfalls ohne
Versorgung. Deshalb existiert fuer die Unterbrechungszeit keine kontinuierliche
Messkurve.

Verfuegbar sind hoechstens:

- letzte gueltige Temperatur vor dem Ausfall
- Zeitstempel des letzten gespeicherten Zustands
- erste gueltige Temperatur nach dem Neustart
- Dauer der Unterbrechung, sofern eine verlaessliche Zeitquelle verfuegbar ist
- spaeter eventuell Raumtemperatur oder ein kalibriertes thermisches Modell

Die Temperaturentwicklung waehrend des Ausfalls muss daher geschaetzt werden.
Eine spaetere Inbetriebnahme kann das Abkuehlverhalten des realen Schrankes mit
Wasser beziehungsweise Testlast vermessen und daraus ein konservatives
thermisches Modell ableiten.

## Vorgesehener Wiederanlauf

### Kurze und sicher bewertbare Unterbrechung

Bei einer kurzen Unterbrechung darf automatisch fortgesetzt werden, wenn:

- Unterbrechungsdauer verlaesslich bekannt ist
- aktueller Sensorstatus gueltig ist
- aktuelle Temperatur innerhalb der zulaessigen Wiederanlaufgrenzen liegt
- kein Sicherheitsfehler vorliegt
- die berechnete Zeitverlaengerung einen festgelegten Maximalwert nicht
  ueberschreitet

Die Restzeit wird dabei um eine temperaturgewichtete Kompensation verlaengert.
Die Verlaengerung und ihre Berechnungsgrundlage werden angezeigt und
protokolliert.

### Lange oder unsichere Unterbrechung

Bei einer langen oder nicht verlaesslich bewertbaren Unterbrechung erfolgt keine
unbemerkte automatische Zeitkorrektur. Die Bedienoberflaeche zeigt mindestens:

- geschaetzte Unterbrechungsdauer
- letzte Temperatur vor dem Ausfall
- aktuelle Produkt- oder Lufttemperatur
- Vertrauensstufe der Schaetzung
- vorgeschlagene Verlaengerung

Moegliche Aktionen:

```text
[Fortsetzen mit vorgeschlagener Verlaengerung]
[Fortsetzen und Verlaengerung anpassen]
[Abbrechen und ausschalten]
[Abbrechen und kuehlen]
```

Nur durch die Sicherheitslogik erlaubte Aktionen werden angezeigt.

## Zeitquelle fuer die Unterbrechungsdauer

Fuer eine verlaessliche Unterscheidung zwischen kurzer und langer Unterbrechung
wird eine belastbare Zeitquelle benoetigt.

Moegliche Varianten:

1. Netzwerkzeit nach dem Neustart und zuvor gespeicherter Zeitstempel
2. batteriegepufferte Echtzeituhr, beispielsweise spaeter ein RTC-Modul
3. Unterbrechungsdauer unbekannt; immer als unsichere Unterbrechung behandeln

Das erste Release darf nicht automatisch fortsetzen, wenn die
Unterbrechungsdauer nicht verlaesslich bestimmt werden kann.

Die Entscheidung zwischen reiner Netzwerkzeit und einer zusaetzlichen RTC ist
noch offen.

## Akzeptierte Entscheidungen

- [x] kein Tuerkontakt im ersten Release
- [x] optionale Tuerereignisse in der Architektur fuer eine spaetere Erweiterung
- [x] bei spaeterem Tuerkontakt Peltier AUS, Timer laeuft weiter
- [x] Produktfuehlerausfall fuehrt nicht zu stillem Wechsel auf Luftregelung
- [x] Fortsetzung mit Luftfuehler ist nach Benutzerentscheidung moeglich
- [x] maximale Wartezeit in `WAITING_FOR_PRODUCT` pro Programm
- [x] Fehlerquittierung und Fortsetzung richten sich nach Fehlerklasse
- [x] sichere Aktion in `RECOVERY_DECISION` richtet sich nach Programm und Phase
- [x] Stromausfallzeit wird weder pauschal voll angerechnet noch voll pausiert
- [x] Ziel ist eine temperaturgewichtete Verlaengerung der Fermentationszeit
- [x] Produktfuehler hat fuer die Kompensation Vorrang, Luftfuehler ist Ersatz mit
      geringerer Sicherheit

## Noch offen

- Zeitquelle fuer verlaessliche Unterbrechungsdauer
- Grenzwert fuer kurze und lange Unterbrechung
- konkrete Temperatur-Aktivitaetsfunktion oder konservative Ersatzlogik
- maximale automatisch zulaessige Zeitverlaengerung
- Behandlung der Entscheidungszeit bei Produktfuehlerausfall
- genaue programmspezifische Aktion in `RECOVERY_DECISION`
- konkrete Fehlerklassen und Fortsetzungsbedingungen
