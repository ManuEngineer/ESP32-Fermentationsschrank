# Produktvision und Nutzung

## Ziel des Produkts

Der ESP32-Fermentationsschrank soll Fermentationsprozesse automatisch und
reproduzierbar auf einer vorgegebenen Produkttemperatur fuehren. Das Geraet
kann die Zieltemperatur je nach Ausgangslage durch **Heizen oder Kuehlen**
erreichen, die Fermentationszeit kontrolliert ablaufen lassen und das Produkt
nach Programmende optional aktiv herunterkuehlen oder gekuehlt halten.

Die Bedienung richtet sich an Manuel und seine Partnerin sowie grundsaetzlich
an Personen ohne Programmier- oder Elektronikkenntnisse. Technische Details
duerfen in einem Diagnose- oder Servicemodus sichtbar sein, duerfen aber fuer
die normale Nutzung nicht erforderlich sein.

## Produktgrundsaetze

1. Das Geraet funktioniert im Normalbetrieb vollstaendig ohne Cloud und ohne
   aktive Internetverbindung.
2. Programme koennen lokal am Touchdisplay ausgewaehlt, gestartet, ueberwacht
   und gestoppt werden.
3. Eine lokale Weboberflaeche bietet zusaetzlich die vollstaendige Bedienung,
   Konfiguration und Ueberwachung.
4. Sicherheitsfunktionen und laufende Temperaturregelung bleiben auch bei
   Ausfall von Display, Touch, Weboberflaeche, Internet oder Heimserver
   funktionsfaehig.
5. Hardware- und Softwarefehler duerfen keine unbeabsichtigte Freigabe von
   Heizung, Kuehlung oder anderen Aktoren bewirken.
6. Programme und Bedienoberflaechen werden so gestaltet, dass spaetere
   Erweiterungen moeglich sind, ohne bestehende Programme unbrauchbar zu
   machen.

## Zielbenutzer

### Normaler Benutzer

Der normale Benutzer muss ohne technische Kenntnisse folgende Aufgaben
ausfuehren koennen:

- gespeichertes Programm auswaehlen
- Programmparameter vor dem Start pruefen
- Programm starten und stoppen
- aktuelle Luft- und Produkttemperatur sehen
- aktuelle Prozessphase und Restzeit sehen
- Meldungen und klare Handlungsanweisungen verstehen
- nach einer Unterbrechung zwischen angebotenen sicheren Aktionen waehlen

Der normale Benutzer muss weder GPIOs, Sensoradressen, Reglerparameter noch
Netzwerkprotokolle kennen.

### Servicemodus

Ein getrennt geschuetzter Servicemodus ist vorgesehen fuer:

- Anzeige technischer Messwerte und Diagnosen
- Sensorzuordnung und Sensortest
- Display- und Touchkalibrierung
- kontrollierte Einzeltests von Lueftern und Peltier-Richtung
- Pruefung von Ausgaengen und Sicherheitsverriegelungen

Direkte Aktorsteuerung darf nur im Servicemodus und unter zusaetzlichen
Sicherheitsbedingungen moeglich sein. Sie ist kein Bestandteil des normalen
manuellen Betriebs.

## Bedienung ohne Netzwerk

Das Geraet muss ohne WLAN oder Internet vollstaendig bedienbar bleiben:

- Programm auswaehlen
- Programm starten und stoppen
- Status und Temperaturen anzeigen
- Meldungen bestaetigen
- nach Stromausfall lokal entscheiden
- einfachen manuellen Betrieb verwenden

Netzwerkfunktionen sind Komfort- und Fernbedienfunktionen, aber keine
Voraussetzung fuer die eigentliche Fermentation.

## Programme

### Standardprogramme

Die erste nutzbare Ausbaustufe enthaelt vier vorbereitete Programme:

1. Joghurt mild
2. Joghurt stichfest
3. Milchkefir
4. Wasserkefir

Die genauen Temperaturen, Zeiten und Kuehloptionen werden in der Phase
`Programme und Prozessablauf` festgelegt.

### Erweiterbarkeit

Die Anzahl der Programme darf nicht fest auf vier oder fuenf begrenzt sein.
Der Benutzer soll spaeter weitere Programme anlegen koennen, beispielsweise:

- Kombucha
- weitere Joghurtkulturen
- eigene Fermentationsversuche
- Programme mit angepassten Zeiten und Temperaturen

Die Oberflaeche muss deshalb eine dynamische Programmliste unterstuetzen. Eine
konkrete technische Obergrenze darf nur aus nachvollziehbaren Speicher- oder
Bediengrenzen entstehen und wird spaeter dokumentiert.

## Allgemeiner Prozessablauf

Die erste Ausbaustufe verwendet grundsaetzlich folgenden Ablauf:

```text
Programm auswaehlen
  -> Startparameter bestaetigen
  -> Zieltemperatur erreichen
       -> je nach Ausgangstemperatur heizen oder kuehlen
  -> Temperatur stabilisieren
  -> Fermentationszeit
  -> optional aktiv kuehlen
  -> optional gekuehlt halten
  -> beendet
```

Ein Programm verwendet in der ersten Ausbaustufe eine Fermentationstemperatur.
Das Datenmodell und die Modulgrenzen sollen spaetere Programme mit mehreren
Temperaturstufen nicht unnoetig verhindern. Die genaue Modellierung wird in
`PROGRAMS.md` festgelegt.

## Verhalten nach Programmende

Das Verhalten nach Programmende ist pro Programm konfigurierbar. Vorgesehene
Varianten sind:

- ohne aktive Kuehlung beenden
- bis zu einer Zieltemperatur herunterkuehlen und danach beenden
- bis zu einer Zieltemperatur herunterkuehlen und fuer eine festgelegte Zeit
  halten
- bis zu einer Zieltemperatur herunterkuehlen und bis zum manuellen Beenden
  halten

Die Anzeige muss klar unterscheiden zwischen abgeschlossener Fermentation,
aktivem Herunterkuehlen und anschliessendem Kuehlhalten.

## Manueller Betrieb

Zusaetzlich zu gespeicherten Programmen ist ein einfacher manueller Modus
vorgesehen. Der Benutzer gibt mindestens an:

- Zieltemperatur
- Dauer
- Verhalten nach Ablauf

Der manuelle Modus nutzt dieselben Sicherheits-, Sensor- und
Regelmechanismen wie gespeicherte Programme. Er ist keine direkte
Aktorsteuerung.

## Weboberflaeche und Fernbedienung

Die Weboberflaeche soll im lokalen Netzwerk die vollstaendige Bedienung
erlauben:

- Status und Temperaturen anzeigen
- Programme anzeigen und bearbeiten
- Programme starten und stoppen
- Einstellungen verwalten
- Meldungen und Unterbrechungen behandeln

Fernzugriff von ausserhalb des Heimnetzes ist nicht Bestandteil des ersten
Releases. Der ESP32 darf nicht direkt offen ins Internet gestellt werden.
Authentisierung, Autorisierung und eine spaetere Fernzugriffsarchitektur werden
in `WEB_UI.md` und `NETWORK.md` festgelegt.

## Stromausfall und Wiederanlauf

### Infrastruktur waehrend eines Stromausfalls

Der Fermentationsschrank selbst verliert bei einem Stromausfall seine
Versorgung. Folgende Infrastruktur laeuft ueber eine USV noch ungefaehr
30 Minuten weiter:

```text
USV
├── Router
└── Heimserver
```

Dadurch kann der Heimserver in einem spaeteren Release das Verschwinden des
ESP32 anhand eines fehlenden Heartbeats erkennen. Eine solche Benachrichtigung
ist jedoch ausdruecklich **nicht Bestandteil des ersten Releases**.

### Kurzer Stromausfall

Nach einem kurzen Stromausfall darf ein zuvor laufendes Programm automatisch
fortgesetzt werden, sofern alle folgenden Bedingungen erfuellt sind:

- ein gueltiger, persistierter Programmschnappschuss ist vorhanden
- die Unterbrechungsdauer liegt unter einem noch festzulegenden Grenzwert
- alle erforderlichen Sensoren liefern nach dem Neustart gueltige Werte
- die Temperaturen liegen innerhalb eines noch festzulegenden
  Wiederanlaufbereichs
- kein Sicherheits- oder Hardwarefehler ist aktiv

Die genaue Zeitgrenze und die zulaessigen Temperaturabweichungen werden spaeter
pro Prozessart oder global festgelegt.

### Langer Stromausfall

Nach einer langen oder nicht sicher bewertbaren Unterbrechung darf das Programm
nicht blind automatisch weiterlaufen. Das Geraet wechselt in einen sicheren
Unterbrechungszustand und verlangt eine Entscheidung:

- Programm fortsetzen, sofern die Software dies noch als zulaessig bewertet
- Programm abbrechen
- eine spaeter festzulegende sichere Abschlussaktion ausfuehren, zum Beispiel
  Kuehlen, falls dies fuer das konkrete Programm sinnvoll ist

Im ersten Release erfolgt diese Entscheidung lokal am Display oder ueber die
lokale Weboberflaeche, sobald der Benutzer wieder Zugriff auf das Heimnetz hat.

## Benachrichtigungen und spaetere Releases

Telegram, Push-Benachrichtigungen und serververmittelte Fernaktionen werden auf
ein spaeteres Release verschoben.

Eine spaetere Architektur kann wie folgt aussehen:

```text
ESP32 oder Server-Heartbeat-Ueberwachung
  -> Heimserver / Notification Gateway
  -> Telegram oder anderer Benachrichtigungskanal
```

Moegliche spaetere Ereignisse:

- ESP32 waehrend eines Stromausfalls nicht mehr erreichbar
- Versorgung wiederhergestellt
- Entscheidung zum Fortsetzen oder Abbrechen erforderlich
- Sensor- oder Aktorfehler
- Programm beendet
- Kuehlziel erreicht

Der Heimserver kann waehrend der ungefaehr 30-minuetigen USV-Laufzeit bereits
das Ausbleiben des ESP32 erkennen. Nach Ablauf der USV ist auch diese
Meldekette nicht mehr garantiert.

Unabhaengig von spaeteren Benachrichtigungen muss der ESP32 eigenstaendig und
sicher arbeiten. Fernaktionen duerfen die lokale Sicherheitslogik niemals
umgehen.

## Verhalten ohne erreichbaren Benutzer

Eine ausstehende Benutzerentscheidung darf das Geraet nicht in einen
undefinierten Zustand bringen. Fuer jeden Unterbrechungsfall muss spaeter ein
sicherer Standardzustand mit einer maximalen Wartezeit definiert werden.

Dabei wird zwischen zwei Fragen unterschieden:

1. **Technische Sicherheit:** Welche Aktoren duerfen laufen?
2. **Prozessqualitaet:** Ist die Fermentation nach der Unterbrechung noch
   sinnvoll fortsetzbar?

Die Software darf nur Aktionen anbieten, die anhand von Zustand,
Unterbrechungsdauer und Sensorwerten als zulaessig bewertet wurden.

## Ausdrueckliche Nicht-Ziele des ersten Releases

- konkrete GPIO-Belegung vor Hardwareverifikation
- Telegram-Bot oder sonstige Push-Benachrichtigung
- Fernzugriff aus dem Internet
- Cloud-Abhaengigkeit
- mehrstufige Temperaturprogramme
- direkte Aktorsteuerung ausserhalb des Servicemodus

## Akzeptierte Produktentscheidungen

- [x] Bedienung fuer nichttechnische Benutzer ohne Programmierung
- [x] vollstaendige lokale Bedienung ohne WLAN
- [x] vier vorbereitete Standardprogramme im ersten Release
- [x] dynamisch erweiterbare Programmliste
- [x] Zieltemperatur kann durch Heizen oder Kuehlen erreicht werden
- [x] erster Release mit einer Fermentationstemperatur pro Programm
- [x] Architektur soll spaetere mehrstufige Programme nicht verhindern
- [x] Verhalten nach Programmende pro Programm konfigurierbar
- [x] vollstaendige Bedienung ueber die lokale Weboberflaeche
- [x] kurzer Stromausfall kann unter validierten Bedingungen automatisch
      fortgesetzt werden
- [x] langer Stromausfall verlangt eine Benutzerentscheidung
- [x] einfacher manueller Zeit-/Temperaturbetrieb
- [x] geschuetzter Servicemodus fuer technische Tests
- [x] Router und Heimserver laufen bei Stromausfall rund 30 Minuten ueber USV
- [x] Telegram und Push-Benachrichtigungen werden auf ein spaeteres Release
      verschoben

## Offene Punkte fuer spaetere Phasen

- Grenzwert fuer kurzen und langen Stromausfall
- Bewertung der Prozessqualitaet nach Temperaturabweichung
- sicherer Zustand waehrend einer ausstehenden Entscheidung
- erlaubte Aktionen nach einer Unterbrechung
- Datenmodell fuer dynamische und spaeter mehrstufige Programme
- spaetere Heartbeat-Ueberwachung durch den Heimserver
- spaetere Benachrichtigungs- und Fernzugriffsarchitektur
