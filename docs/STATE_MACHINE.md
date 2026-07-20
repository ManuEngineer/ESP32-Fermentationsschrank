# Betriebszustaende und Zustandsmaschine

## Status

Dieses Dokument beschreibt den akzeptierten Grundaufbau der Betriebszustaende.
Die Detailbedingungen fuer einzelne Uebergaenge, Warnungen, Fehler und den
Wiederanlauf werden in den naechsten Spezifikationsschritten ergaenzt.

## Grundsaetze

- Jeder Zustand hat einen klar definierten Zweck.
- Aktoren duerfen nur in Zustaenden aktiv sein, in denen dies ausdruecklich
  vorgesehen ist.
- Ein Zustandswechsel muss atomar und nachvollziehbar sein.
- Es gibt keine allgemeine Pausenfunktion fuer laufende Fermentationen.
- Warnungen und Fehler werden getrennt behandelt.
- Ein manueller Lauf verwendet dieselben Sicherheits- und Regelmechanismen wie
  ein gespeichertes Programm.
- Direkte Aktorsteuerung ist ausschliesslich im geschuetzten Servicemodus
  zulaessig.

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

Besondere Zustaende:

```text
RECOVERY_EVALUATION
RECOVERY_DECISION
WARNING_REQUIRES_ACTION
FAULT
SERVICE_MODE
MANUAL_HOLDING
```

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

Nach erfolgreicher Zielqualifikation des leeren Schrankes:

```text
PREHEATING
  -> WAITING_FOR_PRODUCT
```

## WAITING_FOR_PRODUCT

### Zweck

Der Schrank haelt die vorbereitete Temperatur und wartet darauf, dass der
Benutzer das Produkt einsetzt und dies bewusst bestaetigt.

### Verhalten

- Anzeige `Produkt einsetzen`
- akustisches, quittierbares Signal
- Temperaturregelung des leeren beziehungsweise geoeffneten Schrankes bleibt
  aktiv, soweit sicher und sinnvoll
- Fermentationstimer laeuft nicht

### Uebergang

```text
Benutzer drueckt WEITER / START
  -> REACHING_TARGET
```

Ein automatischer Start ohne diese zweite Bestaetigung ist nicht zulaessig.

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

## FERMENTING

### Zweck

Eigentliche temperaturgefuehrte Fermentationsphase mit laufendem Timer.

### Regeln

- Eine allgemeine Pausenfunktion existiert nicht.
- Die biologische Fermentation laeuft auch bei pausierter Software weiter;
  deshalb soll die Bedienoberflaeche keinen irrefuehrenden Pause-Befehl bieten.
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

## COMPLETED

### Zweck

Lauf ist fachlich abgeschlossen und wartet auf Benutzerquittierung.

### Verhalten

- Peltier AUS, sofern nicht bereits ein beabsichtigter Kuehlhaltezustand zuvor
  beendet wurde
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

Das Geraet wechselt nicht automatisch aus `COMPLETED` nach `STANDBY`.

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
- nichtkritische Netzwerk- oder Oberflaechenfunktion ausgefallen

Standardverhalten:

- Lauf geht weiter
- Warnung wird sichtbar angezeigt
- Ereignis wird protokolliert
- je nach Warnung akustisches Signal
- einzelne Warnklassen duerfen spaeter eine Benutzerentscheidung verlangen

Falls eine Warnung zwingend eine Entscheidung benoetigt, kann der fachliche
Unterzustand `WARNING_REQUIRES_ACTION` verwendet werden. Die Sicherheitslogik
bleibt waehrenddessen aktiv und legt fest, welche Aktoren weiterlaufen duerfen.

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

Nach Boot pruefen, ob ein unterbrochener Lauf automatisch fortgesetzt werden
darf.

Zu bewerten sind mindestens:

- Persistenzdaten und Programmschnappschuss gueltig
- Art und Phase des unterbrochenen Laufes
- geschaetzte Unterbrechungsdauer
- aktuelle Sensorwerte
- Temperaturabweichung
- aktive Fehler oder Warnungen

### Uebergaenge

```text
automatische Fortsetzung zulaessig
  -> zuletzt fachlich gueltiger Prozesszustand

Benutzerentscheidung erforderlich
  -> RECOVERY_DECISION

Fortsetzung technisch unzulaessig
  -> RECOVERY_DECISION mit eingeschraenkten Aktionen oder FAULT
```

## RECOVERY_DECISION

### Zweck

Nach einer langen oder nicht sicher bewertbaren Unterbrechung auf eine
Benutzerentscheidung warten.

Moegliche, von der Sicherheitslogik freigegebene Aktionen:

- fortsetzen
- abbrechen und ausschalten
- abbrechen und kuehlen

Das erste Release bietet diese Entscheidung lokal am Display und in der lokalen
Weboberflaeche an. Push- oder Telegram-Benachrichtigungen sind nicht Bestandteil
des ersten Releases.

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

`MANUAL_HOLDING` haelt die Zieltemperatur bis zur manuellen Beendigung. Dieser
Modus ist keine direkte Aktorsteuerung und verwendet weiterhin alle normalen
Sicherheits-, Sensor- und Regelmechanismen.

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

## Noch offen fuer Phase 3B

- genaue Unterzustaende und Uebergaenge bei Tueröffnung
- Verhalten beim Wechsel oder Ausfall des Produktfuehlers
- erlaubte Aktoren in `RECOVERY_DECISION`
- Rueckkehr aus `FAULT` und Bedingungen fuer Fehlerquittierung
- Luefternachlauf je Zustand
- Verhalten bei langer Benutzerabwesenheit in `WAITING_FOR_PRODUCT`
- Verhalten bei ausstehender Entscheidung in `WARNING_REQUIRES_ACTION`
- Persistenzzeitpunkte und Wiederherstellung der Restzeit
- Prioritaet gleichzeitiger Warnungen und Fehler
