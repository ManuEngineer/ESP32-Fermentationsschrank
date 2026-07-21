# Produktvision und Nutzung

## Ziel des Produkts

Der ESP32-Fermentationsschrank soll Fermentationsprozesse automatisch und
reproduzierbar auf einer vorgegebenen Temperatur fuehren. Das Geraet kann die
Zieltemperatur je nach Ausgangslage durch **Heizen oder Kuehlen** erreichen, die
Fermentationszeit kontrolliert ablaufen lassen und das Produkt nach
Programmende optional aktiv herunterkuehlen oder gekuehlt halten.

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
7. Ein Wiederanlauf nach Stromausfall darf nicht unbegrenzt auf den Benutzer
   warten, sondern muss den Prozess automatisch und phasenbezogen sinnvoll
   weiterfuehren.

## Zielbenutzer

### Normaler Benutzer

Der normale Benutzer muss ohne technische Kenntnisse folgende Aufgaben
ausfuehren koennen:

- gespeichertes Programm auswaehlen
- Programmparameter vor dem Start pruefen
- Programm starten und stoppen
- aktuelle Schrankluft- und Produkttemperatur sehen
- aktuelle Prozessphase und Restzeit sehen
- Meldungen und klare Handlungsanweisungen verstehen
- automatisch getroffene Wiederanlaufentscheidungen nachtraeglich pruefen
- einen Lauf innerhalb der erlaubten Optionen anpassen oder beenden

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
- automatischen Wiederanlauf lokal pruefen
- einfachen manuellen Betrieb verwenden

Netzwerkfunktionen sind Komfort- und Fernbedienfunktionen, aber keine
Voraussetzung fuer die eigentliche Fermentation. Netzwerkzeit ist fuer eine
praezisere Wiederanlaufkorrektur vorgesehen; bis sie verfuegbar ist, arbeitet
das Geraet mit einer sicheren vorlaeufigen Strategie weiter.

## Programme

### Standardprogramme

Die erste nutzbare Ausbaustufe enthaelt vier allgemeine Programme:

1. Joghurt mild
2. Joghurt stichfest
3. Milchkefir
4. Wasserkefir

Sie sind nicht an eine bestimmte Kultur oder Marke gebunden. Die genauen
Temperaturen, Zeiten und Kuehlwerte werden nach Inbetriebnahme und praktischen
Tests festgelegt.

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

```text
Programm auswaehlen
  -> Startparameter bestaetigen
  -> optional vorheizen oder vorkuehlen
  -> optional Produkt einsetzen und bestaetigen
  -> Zieltemperatur erreichen
  -> Zieltemperatur qualifizieren
  -> Fermentationszeit
  -> optional aktiv kuehlen
  -> optional gekuehlt halten
  -> beendet
```

`Zieltemperatur erreichen` kann Heizen oder Kuehlen bedeuten. Die
Zielqualifikation ist eine kurze Pruefung vor dem Timer und nicht Teil der
Fermentationszeit.

Ein Programm verwendet im ersten Release eine Fermentationstemperatur. Das
Datenmodell und die Modulgrenzen sollen spaetere Programme mit mehreren
Temperaturstufen nicht unnoetig verhindern.

## Verhalten nach Programmende

Das Verhalten nach Programmende ist pro Programm konfigurierbar:

- ohne aktive Kuehlung beenden
- bis zu einer Zieltemperatur herunterkuehlen und danach beenden
- bis zu einer Zieltemperatur herunterkuehlen und fuer eine festgelegte Zeit
  halten
- bis zu einer Zieltemperatur herunterkuehlen und bis zum manuellen Beenden
  halten

Die Anzeige muss klar unterscheiden zwischen abgeschlossener Fermentation,
aktivem Herunterkuehlen und anschliessendem Kuehlhalten.

## Manueller Betrieb

Es sind zwei normale manuelle Betriebsarten vorgesehen:

1. Zeit-/Temperaturlauf mit Zieltemperatur, Dauer und Abschlussverhalten
2. Temperatur-Haltebetrieb ohne Timer bis zur manuellen Beendigung

Beide nutzen dieselben Sicherheits-, Sensor- und Regelmechanismen wie
Programme. Direkte Aktorsteuerung gehoert ausschliesslich in den Servicemodus.

## Weboberflaeche und Fernbedienung

Die Weboberflaeche soll im lokalen Netzwerk die vollstaendige Bedienung
erlauben:

- Status und Temperaturen anzeigen
- Programme anzeigen und bearbeiten
- Programme starten und stoppen
- Einstellungen verwalten
- Meldungen und Wiederanlaufdetails anzeigen

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
ist nicht Bestandteil des ersten Releases.

### Automatisches phasenbezogenes Fortfahren

Nach dem Neustart wartet das Geraet nicht darauf, dass der Benutzer vor dem
Display steht. Nach erfolgreicher Pruefung von Persistenz, Sensoren und
Sicherheitsbedingungen wird automatisch eine zur unterbrochenen Phase passende
Aktion ausgefuehrt:

- Vorheizen oder Zieltemperatur erneut anfahren
- Zielqualifikation neu beginnen
- Fermentationsregelung wieder aufnehmen
- Kuehlung oder Kuehlhalten fortsetzen
- manuellen Haltebetrieb fortsetzen
- einen bereits abgeschlossenen Zustand nur wieder anzeigen

Ein Sicherheitsfehler kann das automatische Fortfahren verhindern und hat
immer Vorrang.

### Wirkung auf die Fermentationszeit

Die Fermentation stoppt bei sinkender Temperatur nicht vollstaendig, sondern
verlangsamt sich. Deshalb wird die Stromausfallzeit weder pauschal voll
angerechnet noch pauschal pausiert.

Vorgesehen ist eine temperaturgewichtete Schaetzung. Bei einem
produktgefuehrten Lauf hat der Produktfuehler Vorrang. Ohne Produktfuehler wird
die Schranklufttemperatur des fest eingebauten Luftfuehlers verwendet.

Die daraus abgeleitete Verlaengerung wird automatisch angewendet, angezeigt
und protokolliert. Der Benutzer kann sie spaeter innerhalb sicherer Grenzen
pruefen und anpassen.

### Netzwerkzeit und spaetere RTC-Option

Im ersten Release ist Netzwerkzeit die primaere Zeitquelle. Weil der ESP32 nach
einem Stromausfall schneller startet als Router und NTP-Verbindung, beginnt der
sichere Wiederanlauf sofort. Sobald Netzwerkzeit verfuegbar ist, werden
Unterbrechungsdauer und Zeitkorrektur nachtraeglich praezisiert.

Die Architektur soll eine spaetere batteriegepufferte Echtzeituhr, zum Beispiel
ein DS3231-Modul, ermoeglichen. Eine RTC ist fuer das erste Release nicht
erforderlich.

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
- automatische Wiederanlaufentscheidung und Zeitkorrektur
- Sensor- oder Aktorfehler
- Programm beendet
- Kuehlziel erreicht

Unabhaengig von spaeteren Benachrichtigungen muss der ESP32 eigenstaendig und
sicher arbeiten. Fernaktionen duerfen die lokale Sicherheitslogik niemals
umgehen.

## Ausdrueckliche Nicht-Ziele des ersten Releases

- konkrete GPIO-Belegung vor Hardwareverifikation
- Telegram-Bot oder sonstige Push-Benachrichtigung
- Fernzugriff aus dem Internet
- Cloud-Abhaengigkeit
- mehrstufige Temperaturprogramme
- direkte Aktorsteuerung ausserhalb des Servicemodus
- verpflichtendes RTC-Modul

## Akzeptierte Produktentscheidungen

- [x] Bedienung fuer nichttechnische Benutzer ohne Programmierung
- [x] vollstaendige lokale Bedienung ohne WLAN
- [x] vier allgemeine Standardprogramme im ersten Release
- [x] dynamisch erweiterbare Programmliste
- [x] Zieltemperatur kann durch Heizen oder Kuehlen erreicht werden
- [x] erster Release mit einer Fermentationstemperatur pro Programm
- [x] Architektur soll spaetere mehrstufige Programme nicht verhindern
- [x] Verhalten nach Programmende pro Programm konfigurierbar
- [x] vollstaendige Bedienung ueber die lokale Weboberflaeche
- [x] automatischer phasenbezogener Wiederanlauf ohne blockierendes Warten auf
      den Benutzer
- [x] temperaturgewichtete Verlaengerung nach Stromunterbrechung
- [x] Netzwerkzeit als primaere Zeitquelle
- [x] spaetere RTC-Unterstuetzung bleibt moeglich
- [x] einfacher manueller Zeit-/Temperaturbetrieb
- [x] manueller Temperatur-Haltebetrieb ohne Timer
- [x] geschuetzter Servicemodus fuer technische Tests
- [x] Router und Heimserver laufen bei Stromausfall rund 30 Minuten ueber USV
- [x] Telegram und Push-Benachrichtigungen werden auf ein spaeteres Release
      verschoben

## Offene Punkte fuer spaetere Phasen

- Grenzwert fuer kurze und lange Stromunterbrechung
- konkrete konservative Temperatur- und Zeitkompensation
- Persistenzintervall fuer Laufzustand und Zeitstempel
- genaue Fehlerklassen und Fortsetzungsbedingungen
- Datenmodell fuer dynamische und spaeter mehrstufige Programme
- spaetere Heartbeat-Ueberwachung durch den Heimserver
- spaetere Benachrichtigungs- und Fernzugriffsarchitektur
