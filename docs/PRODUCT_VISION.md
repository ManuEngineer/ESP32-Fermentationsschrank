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
   Ausfall von Display, Touch, Weboberflaeche, Internet oder externem Server
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
ermoeglichen:

- Status und Temperaturen anzeigen
- Programme anzeigen und bearbeiten
- Programme starten und stoppen
- Einstellungen verwalten
- Meldungen und Unterbrechungen behandeln

Fernzugriff von ausserhalb des Heimnetzes darf den ESP32 nicht direkt offen ins
Internet stellen. Authentisierung, Autorisierung und konkrete
Fernzugriffsarchitektur werden in der Phase `Weboberflaeche und Netzwerk`
festgelegt.

## Stromausfall und Wiederanlauf

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

Die Entscheidung muss lokal am Display und bei vorhandener Verbindung auch aus
der Ferne moeglich sein.

## Benachrichtigung bei Abwesenheit

Das Geraet soll wichtige Ereignisse an den Benutzer melden koennen, besonders:

- langer Stromausfall und wiederhergestellte Versorgung
- Entscheidung zum Fortsetzen oder Abbrechen erforderlich
- Sensor- oder Aktorfehler
- Programm beendet
- Kuehlziel erreicht

### Bevorzugte Richtung

Fuer die erste Fernbenachrichtigung wird eine serververmittelte Loesung mit
Telegram bevorzugt:

```text
ESP32
  -> ausgehende, authentisierte Verbindung
  -> eigener Heimserver / Notification Gateway
  -> Telegram Bot
  -> Nachricht mit sicheren Aktionsschaltflaechen
```

Gruende:

- Der ESP32 benoetigt keinen direkt erreichbaren Internet-Port.
- Das Telegram-Bot-Token bleibt auf dem Server und nicht in der Firmware.
- Die Benachrichtigungslogik kann spaeter ausgetauscht oder erweitert werden.
- Telegram unterstuetzt Nachrichten mit Inline-Schaltflaechen und Rueckmeldungen.
- Der vorhandene Heimserver kann Entscheidungen zwischenspeichern, bis der
  ESP32 wieder erreichbar ist.

Der ESP32 muss trotzdem eigenstaendig und sicher weiterarbeiten, wenn der
Server, Telegram oder das Internet nicht erreichbar ist. Die konkrete
Kommunikation zwischen ESP32 und Server, beispielsweise HTTPS oder MQTT, wird
in `NETWORK.md` entschieden.

Eine direkte Telegram-Integration auf dem ESP32 bleibt technisch moeglich, ist
aber nicht die bevorzugte Architektur, weil sie Bot-Zugangsdaten und
Telegram-spezifische Kommunikationslogik in die Firmware verlagern wuerde.

Offizielle technische Referenzen:

- Telegram Bot API: https://core.telegram.org/bots/api
- Empfang von Bot-Updates per Long Polling oder Webhook:
  https://core.telegram.org/bots/webhooks

## Verhalten ohne erreichbaren Benutzer

Eine ausstehende Fernentscheidung darf das Geraet nicht in einen undefinierten
Zustand bringen. Fuer jeden Unterbrechungsfall muss spaeter ein sicherer
Standardzustand mit einer maximalen Wartezeit definiert werden.

Dabei wird zwischen zwei Fragen unterschieden:

1. **Technische Sicherheit:** Welche Aktoren duerfen laufen?
2. **Prozessqualitaet:** Ist die Fermentation nach der Unterbrechung noch
   sinnvoll fortsetzbar?

Die Software darf eine Fernaktion nur anbieten, wenn sie anhand von Zustand,
Unterbrechungsdauer und Sensorwerten als zulaessig bewertet wurde. Eine
Telegram-Schaltflaeche darf die Sicherheitslogik niemals umgehen.

## Ausdrueckliche Nicht-Ziele dieser Phase

In dieser Spezifikationsphase wird noch nicht festgelegt oder implementiert:

- konkrete GPIO-Belegung
- konkrete Regelparameter
- genaue Programtemperaturen und Zeiten
- konkrete Netzwerkprotokolle
- Telegram-Bot-Code
- Weboberflaechenlayout
- Firmware fuer die Fermentationssteuerung

Diese Punkte werden in den folgenden Spezifikationsphasen schrittweise
entschieden.

## Akzeptierte Produktentscheidungen

- [x] Bedienung fuer nichttechnische Benutzer ohne Programmierung
- [x] vollstaendige lokale Bedienung ohne WLAN
- [x] vier vorbereitete Standardprogramme in der ersten Ausbaustufe
- [x] dynamisch erweiterbare Programmliste
- [x] Zieltemperatur kann durch Heizen oder Kuehlen erreicht werden
- [x] erste Ausbaustufe mit einer Fermentationstemperatur
- [x] Architektur soll spaetere mehrstufige Programme nicht verhindern
- [x] Verhalten nach Programmende pro Programm konfigurierbar
- [x] vollstaendige Bedienung ueber die Weboberflaeche
- [x] kurzer Stromausfall kann unter validierten Bedingungen automatisch
      fortgesetzt werden
- [x] langer Stromausfall verlangt eine lokale oder entfernte Entscheidung
- [x] einfacher manueller Zeit-/Temperaturbetrieb
- [x] geschuetzter Servicemodus fuer technische Tests
- [x] optionale Fernbenachrichtigung ohne Cloud-Abhaengigkeit der Kernfunktion

## Offene Punkte fuer spaetere Phasen

- Grenzwert fuer kurzen und langen Stromausfall
- Bewertung der Prozessqualitaet nach Temperaturabweichung
- sicherer Zustand waehrend einer ausstehenden Entscheidung
- erlaubte Fernaktionen je Zustand
- HTTPS oder MQTT zwischen ESP32 und Heimserver
- Telegram als erster verbindlicher Benachrichtigungskanal oder generische
  Provider-Schnittstelle
- Authentisierung und Schutz von Fernzugriff und Befehlen
