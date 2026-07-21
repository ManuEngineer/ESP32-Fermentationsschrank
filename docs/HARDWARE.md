# Hardware

## Statuskennzeichnung

| Kennzeichnung | Bedeutung |
|---|---|
| `confirmed_order` | aus der bestellten Produktbeschreibung uebernommen |
| `confirmed_test` | am realen Aufbau gemessen und dokumentiert |
| `planned` | fuer Release 1 verbindlich vorgesehen, aber noch nicht real bestaetigt |
| `candidate` | moegliche Loesung, noch nicht entschieden |
| `TBD_HARDWARE` | reale Komponente, Pin, Pegel oder Verdrahtung muss geprueft werden |
| `TBD_COMMISSIONING` | thermischer oder regelungstechnischer Wert wird am Schrank bestimmt |
| `FUTURE_RELEASE` | bewusst nicht Bestandteil von Release 1 |

Kein Kandidat und kein `TBD_HARDWARE` darf als bestaetigte Verdrahtung in die
Releasefirmware uebernommen werden.

## Controllerboard

Planungsbasis:

- ESP32-WROOM-32E beziehungsweise bestellte ESP32-32E-Boardvariante
- 4 MB Flash laut bestellter Produktbeschreibung
- keine vorausgesetzte PSRAM
- vier Onboard-MOSFET-Ausgaenge
- 3,3-V-Logik
- Programmierung und Wiederherstellung ueber FT232RL/UART

Noch zu messen:

- exakte Boardrevision und Modulbeschriftung
- tatsaechliche Flashgroesse und Partitionseigenschaften
- PSRAM-Erkennung
- verfuegbare GPIOs
- Zuordnung und aktive Pegel der vier MOSFET-Kanaele
- Boot-, Reset-, Brownout- und Bootloaderpegel aller verwendeten Signale
- Verhalten der MOSFET-Ausgaenge ohne und mit angeschlossenen Verbrauchern

## Peltier und Leistungspfad

Geplant:

- Peltier: etwa 12 V / 60 W, ungefaehr 5 A
- Polaritaetsumkehr ueber BTS7960-H-Bruecke
- zeitproportionaler Betrieb in langen Schaltfenstern
- kein hochfrequentes direktes Peltier-PWM
- 7,5-A-Ueberstromsicherung im Peltier-Leistungspfad
- ausreichend dimensionierte Leitungen, Stecker und Masseverbindung
- gemeinsame Bezugsmasse fuer ESP32-Logik und BTS7960, soweit das reale Modul dies
  verlangt

Vor dem ersten Peltieranschluss:

1. BTS7960-Logikversorgung und Masse pruefen.
2. Enable- und Richtungseingaenge unbelastet messen.
3. Hardware-Pulldowns oder gleichwertige sichere Freigabestufe nachweisen.
4. H-Brueckenausgang und Polaritaet mit Multimeter messen.
5. sicherstellen, dass beide Richtungen nie gleichzeitig aktiv werden.
6. Luefter, Kuehlkoerper, Sensoren und Sicherung vollstaendig montieren.
7. erste reale Freigabe nur als begrenzter Servicepuls.

BTS7960 `R_IS` und `L_IS` werden nur angeschlossen und verwendet, wenn
Pegelbereich, Beschaltung und diagnostischer Nutzen des gelieferten Moduls
praktisch bestaetigt wurden.

## Unabhaengige Schutzkomponenten

### Ueberstromsicherung

- geplantes Rating: 7,5 A
- Einbau im Peltier-Leistungspfad
- genauer Sicherungstyp, Halter, Leitungsquerschnitt und Kurzschlussverhalten:
  `TBD_HARDWARE`

### Einmalige Temperatursicherung

Release 1 verwendet eine einmalige Temperatursicherung als unabhaengige
thermische Notabschaltung.

Anforderungen:

- in der Peltier-Leistungsfreigabe oder im relevanten Leistungspfad
- arbeitet ohne ESP32, Firmware und DS18B20
- Montage an der thermisch kritischsten Stelle
- elektrisch und thermisch sicher befestigt
- nach Ausloesung zu ersetzen
- Ausloesetemperatur, Rating und Montageort erst nach thermischen Messungen:
  `TBD_COMMISSIONING`

Sie ersetzt weder den Kuehlkoerpersensor noch die Ueberstromsicherung.

## Temperatursensoren

Der erste Aufbau verwendet drei DS18B20.

### 1. Schrankluft

- fest eingebaut
- primaerer Regelsensor im luftgefuehrten Betrieb
- Begrenzungs- und Sicherheitssensor im produktgefuehrten Betrieb
- fuer jede Peltierfreigabe erforderlich
- Position wird so gewaehlt, dass weder direkter Luftstrahl noch Wandkontakt die
  Messung unbrauchbar machen

### 2. Produkt

- abnehmbarer Fühler
- optionaler primaerer Regelsensor
- getrennte externe Steckverbindung
- Hot-Plug wird softwareseitig behandelt
- gewoehnliche 3,5-mm-Klinke ist verworfen
- Kandidaten: M8 3-polig, GX12-3 oder anderer verriegelbarer, verpolungssicherer
  3-poliger Anschluss
- Lebensmitteltauglichkeit des eigentlichen Fühlers und Reinigbarkeit der
  Durchfuehrung sind zu bestaetigen

### 3. Aussenwaermetauscher/Kuehlkoerper

- fest an der thermisch relevanten Aussenseite montiert
- ueberwacht Temperatur und Aenderungsrate
- fuer jede Peltierfreigabe erforderlich
- dient der Erkennung fehlender Waermeabfuhr und unplausibler Aktorreaktionen
- aufgrund der umkehrbaren Polaritaet kann diese Seite je nach Betriebsrichtung
  warm oder kalt werden

### 1-Wire-Topologie

Bevorzugt:

- separater GPIO je Sensor

Zulaessiger Rueckfall bei GPIO-Knappheit:

- beide festen Sensoren auf einem internen Bus
- abnehmbarer Produktfuehler auf eigenem externen Bus

Die tatsaechlichen GPIOs bleiben `TBD_HARDWARE`.

Elektrische Anforderungen:

- 3-Leiter-Betrieb, kein parasitaerer Betrieb
- passende Pull-ups je Bus
- Steckverbindung verpolungssicher
- ESD- und Fehlsteckschutz fuer externen Produktanschluss pruefen
- ROM-Adressen bei Hardwareabnahme dokumentieren

## Luefter

### Innenluefter

- 12 V
- sorgt fuer gleichmaessige Schrankluft
- laeuft waehrend aller temperaturgeregelten Phasen und waehrend Peltier-Totzeiten
- besitzt einen Nachlauf nach Ende der Temperaturregelung
- im normalen Standby aus

### Aussenluefter

- 12 V
- kuehlt den aeusseren Waermetauscher
- wird ohne absichtliche Vorlaufzeit im selben Steuerzyklus wie die
  Peltierfreigabe eingeschaltet
- besitzt einen zwingenden Nachlauf
- bleibt bei geeigneten Sicherheitsfehlern zur Restwaermeabfuhr aktiviert

Vor Anschluss werden MOSFET-Ausgang, aktiver Pegel, Stromaufnahme und
Anlaufverhalten unbelastet beziehungsweise mit einzelnem Verbraucher gemessen.

Ob ein Tachosignal spaeter ergaenzt wird, bleibt `FUTURE_RELEASE`.

## Summer

Geplant ist ein aktiver 5-V- oder 12-V-Summer ueber einen geeigneten
MOSFET-/Treiberkanal.

Noch offen:

- Spannung und Stromaufnahme
- konkrete Kanalzuordnung
- aktiver Pegel
- akustische Lautstaerke und Montageort

Der Summer darf keine Sicherheitsaufgabe blockieren.

## Display und Touch

Bestellt beziehungsweise geplant:

- TZT MSP2807, 2,8 Zoll
- 320 x 240 Pixel
- SPI
- ILI9341 als Displaycontroller laut Produktbeschreibung
- resistiver Touch
- XPT2046 als wahrscheinlicher, aber praktisch zu bestaetigender Touchcontroller
- Querformat

Noch zu pruefen:

- Pinbelegung und SPI-Bus
- Controlleridentitaet
- Displayrotation
- Touchrohwerte und Kalibrierung
- Reset- und Bootverhalten
- Hintergrundbeleuchtung und Dimmung
- moegliche Konflikte mit Bootstrapping-Pins

## Versorgung

Vorgesehen:

- 12-V-Leistungspfad fuer Peltier und Luefter
- geeignete 5-V-/3,3-V-Versorgung fuer Controller und Peripherie
- stabile Massefuehrung
- lokale Abblockung nahe Controller, Display, Sensorbussen und H-Bruecke

Release 1 wertet die interne ESP32-Brownout-Erkennung und Resetursache aus.
Eine direkte Messung der 12-V-Leistungsspannung ueber einen geschuetzten
ADC-Spannungsteiler ist vorbereitet, aber nicht verpflichtend und standardmaessig
nicht bestueckt (`FUTURE_RELEASE`).

Versorgungsspannungen, Reglerleistung, Leitungsquerschnitte, Sicherungshalter und
Stecker werden am realen Aufbau dokumentiert.

## Nicht vorgesehene Hardware in Release 1

- Tuerkontakt
- verpflichtende batteriegepufferte RTC
- verpflichtende 12-V-ADC-Messung
- Luefter-Tachosignal
- externe Strommessung zusaetzlich zu optionalem R_IS/L_IS
- eigenes OTA- oder Recovery-Zusatzmodul

Die Software darf spaetere Ereignisse oder Adapter dafuer vorbereiten, aber keine
nicht vorhandene Hardware behaupten.

## Sichere Bootzustaende

Verbindliche Anforderungen:

- beide BTS7960-Richtungen durch Hardwarebeschaltung inaktiv
- Peltierfreigabe erst nach vollstaendiger Initialisierung und Validierung
- Onboard-MOSFET-Ausgaenge beim Boot praktisch messen
- ungeeignete Bootstrapping-Pins nicht fuer sicherheitskritische Freigaben nutzen
- keine automatische Aktorpruefung beim normalen Boot
- `esp32_bringup` startet mit `HARDWARE_UNVERIFIED`

## Update und Recovery

Release 1:

- initiales Flashen ueber FT232RL/UART
- normale Entwicklerupdates ueber UART
- Wiederherstellung ueber ESP32-ROM-Bootloader
- Single-App-Partitionsplan ist zulaessig
- keine reservierten dualen OTA-Slots erforderlich

Web-OTA und automatisches Firmware-Rollback sind `FUTURE_RELEASE` und duerfen nur
nach realem 4-MB-Budgetnachweis geplant werden.

## Verifikationsreihenfolge

1. Sichtpruefung, Versorgung, Masse und Sicherungen
2. Controllerboard ohne Aktoren
3. GPIO- und Bootpegelmessung
4. Sensoren, Display und Touch
5. Luefter und Summer einzeln
6. BTS7960 ohne Peltier
7. begrenzte Peltier-Heiz- und Kuehlpulse
8. thermische Grundvermessung
9. Sicherheits- und Fehlerpruefungen
10. vollstaendige Hardwareabnahme

Die zugehoerigen Issues sind #29 bis #36.
