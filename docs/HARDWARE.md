# Hardware

## Statuskennzeichnung

| Kennzeichnung | Bedeutung |
|---|---|
| `confirmed_order` | aus der bestellten Produktbeschreibung uebernommen |
| `confirmed_by_owner_reference_match` | reale Hardware vorhanden und durch den Owner der Repository-Boardreferenz zugeordnet; kein elektrischer Funktionsnachweis |
| `confirmed_test` | am realen Aufbau gemessen und dokumentiert |
| `planned` | fuer Release 1 verbindlich vorgesehen, aber noch nicht real bestaetigt |
| `board_fixed_pending_electrical_verification` | PCB-seitig fest verdrahtete Zuordnung; Pegel, Bootwirkung und Verbraucherwirkung sind noch offen |
| `candidate` | moegliche Loesung, noch nicht entschieden |
| `TBD_HARDWARE` | reale Komponente, Pin, Pegel oder Verdrahtung muss geprueft werden |
| `TBD_COMMISSIONING` | thermischer oder regelungstechnischer Wert wird am Schrank bestimmt |
| `FUTURE_RELEASE` | bewusst nicht Bestandteil von Release 1 |

Ein Designstatus `planned` oder
`board_fixed_pending_electrical_verification` ist kein
`confirmed_test`. Kein solcher Status setzt `pins_confirmed`,
`active_levels_confirmed`, `boot_levels_confirmed` oder
`actuator_release`. Reale Aktoren bleiben bis zu den owning Hardwaregates
fail-closed.

## Board-/Wiring-SSOT und Identitaet

Electrical/design SSOT for the R1 pin assignment:
config/board_profiles/esp32_32e_quad_mosfet_r1.yaml

Die reale Controllerplatine ist vorhanden und wurde vom Owner mit der im
Repository hinterlegten ESP32-WROOM-32E-Quad-MOSFET-Boardreferenz abgeglichen.
Damit sind reale Hardware und Boardfamilie identifiziert:

- real hardware present: yes
- board family: esp32_32e_quad_mosfet
- MCU module: ESP32-WROOM-32E
- board family matched to repository reference: confirmed by owner
- board revision: TBD_HARDWARE, solange keine eindeutige Kennung vorliegt

Diese Identitaetsfeststellung ist kein Nachweis aktiver Pegel, Bootpegel,
MOSFET-/BTS7960-/Display-/Touch-Funktion, GPIO-Funktionstest oder
Aktorfreigabe. Konkrete GPIO-Zahlen und Widerstandswerte werden in der SSOT
als Designzustand gefuehrt; elektrische Abnahme wird erst mit
`confirmed_test` am konkreten Aufbau dokumentiert.

## Controllerboard

Design-/Referenzbasis:

- ESP32-WROOM-32E auf der Boardfamilie esp32_32e_quad_mosfet
- 4 MB Flash laut bestellter Produktbeschreibung
- keine vorausgesetzte PSRAM
- vier Onboard-MOSFET-Ausgaenge
- 3,3-V-Logik
- Programmierung und Wiederherstellung ueber FT232RL/UART
- konkrete R1-GPIO-/Wiring-Zuordnung ausschliesslich aus dem Boardprofil
- board_revision bleibt TBD_HARDWARE, falls die Kennzeichnung nicht ermittelt
  werden kann

Noch zu messen:

- exakte Boardrevision und weitere reale Identitaetsmerkmale
- tatsaechliche Flashgroesse und Partitionseigenschaften
- PSRAM-Erkennung
- aktive Pegel und Gate-/Treiberbias der vier PCB-festen MOSFET-Kanaele
- Boot-, Reset-, Brownout- und Bootloaderpegel aller verwendeten Signale
- Verhalten der MOSFET-Ausgaenge ohne und mit angeschlossenen Verbrauchern

Die vier PCB-festen MOSFET-Kanalzuordnungen sind als
`board_fixed_pending_electrical_verification` im Boardprofil dokumentiert.
Die Boardfamilienidentitaet ist bestaetigt, die elektrische Kanalwirkung
jedoch nicht.

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

Die verbindliche R1-Zieltopologie wird nicht mehr pro Issue neu erfunden,
sondern ausschließlich aus dem Boardprofil gelesen:

- Schrankluft und Kühlkörper teilen sich den internen Multidrop-Bus
  one_wire_internal;
- der abnehmbare Produktfühler bleibt auf dem separaten Bus
  one_wire_product;
- beide Busse laufen im 3-Leiter-Betrieb und erhalten jeweils den im
  Boardprofil festgelegten Pull-up nach 3,3 V;
- feste Sensorrollen werden über ROM-ID unterschieden;
- ein Fehler des gemeinsamen festen Busses wirkt für die Peltierfreigabe
  weiterhin fail-closed;
- die elektrischen Buspegel, ROM-IDs, CRC- und Hot-Plug-Funktion sind noch
  am realen Aufbau zu verifizieren.

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

Das MSP2807-Display und der resistive Touchpfad verwenden gemäß dem
Boardprofil den gemeinsamen SPI-Bus. XPT2046 bleibt der zu verifizierende
Touchcontroller-Kandidat. TFT_CS und Touch_CS bleiben getrennte
Deselect-Signale; Backlight wird über den im Boardprofil festgelegten
PWM-Ausgang mit sicherem AUS-Zustand bei Boot/Reset betrieben.

Das Resetnetz ist:

    ESP32 EN / CHIP_PU -------- MSP2807 TFT_RESET

Es ist ein direktes gemeinsames active-low Netz. Gemeinsamer GND, der
hochohmige RESET-Eingang des realen Moduls, keine unabhängige Modul-
Rücktreibung und kompatible Power-/Logic-Domains bleiben elektrische
Verifikationsbedingungen. Eine Abweichung des realen Moduls vom
veröffentlichten Schaltbild führt zu STOP und Boardprofilrevision.

Noch zu verifizieren bleiben Controlleridentität, Roh-Touchwerte,
Kalibrierung, Boot-/Resetpegel, IRQ-Pull-up und die reale Funktion von
SPI, Touch und Backlight. Das Statusmodell unterscheidet dabei
Boardfamilien-Referenzabgleich, planned beziehungsweise
board_fixed_pending_electrical_verification und confirmed_test.

## Lokale Bedien- und Anzeigeelemente

Release 1 besitzt das Touchdisplay als einzigen lokalen Eingabekanal und den
Pieper als zusaetzlichen Ausgabekanal. Es gibt keine physischen Taster, keinen
Encoder, keinen Programmwahlschalter und keine Status-LEDs. Daher darf weder
normaler Betrieb noch Service, Recovery oder Touchkalibrierung ein solches
Element voraussetzen. Raw-Touch-Recovery bleibt bis zum Hardwarebeweis
`TBD_HARDWARE`; der Pieper ist kein Safetyentscheider.

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

## Optionale R1-RTC

Die RTC-Wiring-Familie ist DS3231, I2C-Adresse `0x68`, mit SDA/SCL und
INT/SQW gemäß Boardprofil. Die elektrische Verdrahtung ist für DS3231SN und
DS3231M vorgesehen, sofern die Primärquellenprüfung diese Variantenneutralität
trägt. Die tatsächlich gelieferte Variante bleibt davon getrennt:

    delivered_variant: TBD_DELIVERY
    software_supported_variant: DS3231SN

Die RTC ist optional: `present=false` ist ein gültiges R1-Produktprofil und
führt in den NTP-only-Modus. Eine trusted RTC ermöglicht sofortige
Offline-Recovery; ohne trusted RTC wartet die Anwendung auf NTP und startet
keinen neuen produktiven Lauf ohne trusted UTC.

Pull-ups, Versorgung, Batterie-/Ladepfad, physischer IC-Aufdruck,
I2C-Erreichbarkeit, OSF-/EOSC-Verhalten und Power-Loss-Zeitretention müssen
am realen Aufbau nachgewiesen werden. Die Wiring-Zuordnung ist ein
Boardprofil-Designstatus, kein `confirmed_test`.

Die Software akzeptiert RTC-Zeit nur nach Rohregister-, BCD-, Kalender-,
OSF-, EOSC-, EN32kHz- und R1-Jahresbereichprüfung. Der ungenutzte 32-kHz-
Ausgang wird deaktiviert; SQW/INT bleibt in R1 ungenutzt.

## Nicht vorgesehene Hardware in Release 1

- Tuerkontakt
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
- `esp32_bringup` startet weiterhin mit `HARDWARE_UNVERIFIED` beziehungsweise
  dem bestehenden fail-closed Bring-up-Vertrag
- `ELECTRICAL_VERIFICATION_PENDING` bezeichnet hier den Dokument- und
  Konfigurationsstatus der offenen elektrischen Abnahme und ist kein neuer
  Firmware-Startup-State
- Owner-bestaetigte Boardfamilienidentitaet ersetzt keinen Boot- oder
  Pegelnachweis

## Update und Recovery

Release 1:

- initiales Flashen ueber FT232RL/UART
- normale Entwicklerupdates ueber UART
- Wiederherstellung ueber ESP32-ROM-Bootloader
- Single-App-Partitionsplan ist zulaessig
- keine reservierten dualen OTA-Slots erforderlich

Web-OTA und automatisches Firmware-Rollback sind `FUTURE_RELEASE` und duerfen nur
nach realem 4-MB-Budgetnachweis geplant werden.

## Quellen

Die zentrale Liste der Hardwarequellen steht in
[`references/LINKS.md`](../references/LINKS.md). Lokal archivierte Datenblaetter
liegen unter [`references/datasheets/`](../references/datasheets/). Die Inhalte
werden hier nicht dupliziert, damit keine abweichende zweite Quellenliste
entsteht.

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
