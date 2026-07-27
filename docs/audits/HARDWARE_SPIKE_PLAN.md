# Hardware-Spike-Plan

Zur Auditnavigation: [`RELEASE_1_ADOPT_OR_BUILD_AUDIT.md`](RELEASE_1_ADOPT_OR_BUILD_AUDIT.md).

## Zweck und Sicherheitsgrenze

Die Spikes entscheiden nur ueber Hardwaretreiber und technische Adapter. Sie
implementieren weder fachliche UI, Sensor-Safety, Regelung noch produktive
Aktorfreigaben. Alle Peltier-, H-Bruecken-, Luefter- und MOSFET-Ausgaenge
bleiben waehrend dieser Spikes elektrisch getrennt oder nachweislich AUS.

Gemeinsame Basis:

- ESP32-32E-Boardrevision, Flashgroesse und Versorgung vorab dokumentieren;
- PlatformIO `espressif32@7.0.1`, Arduino-ESP32 `2.0.17` (`dcc1105b`), C++17;
- 4 MB Flash, keine PSRAM-Nutzung;
- je Kandidat eigener wegwerfbarer Spike-Branch oder isolierter Build, keine
  Vermischung der Kandidaten;
- identische Compilerflags, Displayinhalte, Sensorablaeufe und Messmethode;
- Version/Commit, Konfiguration und Buildartefakte archivieren;
- keine Kandidatenaussage als bestaetigte Produktverdrahtung uebernehmen.

## Spike A: Display und Touch

### Ziel

Den kleinsten stabilen Treiber fuer das tatsaechliche MSP2807-Modul bestimmen
und ILI9341, Touchcontroller, Rotation, Reset, Shared-SPI-Verhalten sowie
Ressourcen auf der Zieltoolchain belegen.

### Kandidaten

1. LovyanGFX `1.2.26` (`3f78b705`)
2. TFT_eSPI Manifest `2.5.44` (`16e37595`)
3. LCDWiki-Paket aus
   `references/datasheets/Display/2.8inch_SPI_Module_ILI9341_MSP2807_V1.1.zip`

Arduino_GFX plus XPT2046_Touchscreen und Adafruit GFX plus ILI9341 plus
XPT2046_Touchscreen sind Reservekandidaten. Sie werden nur nachgezogen, wenn
die drei Hauptkandidaten scheitern oder keine nachvollziehbare Entscheidung
erlauben.

### Hardwareaufbau und Buskonfiguration

| Punkt | Festlegung |
|---|---|
| Display | genau das gelieferte TZT/LCDWiki MSP2807 |
| Controller | vor Test praktisch identifizieren; ILI9341/XPT2046 nicht allein aus Lieferantenangabe als bestaetigt markieren |
| Versorgung | gemaess gemessener Modulvariante; Spannung und Strom protokollieren |
| SPI | derselbe Hardware-SPI-Controller und dieselbe gemessene Pinbelegung fuer alle Kandidaten |
| Chip Select | getrennte Display-/Touch-CS nur nach Boardpruefung; inaktiv sichere Pegel messen |
| Reset/DC/Backlight | reale Pins, aktive Pegel und Bootzustand messen und als `TBD_HARDWARE` bis dahin offen lassen |
| Weitere Verbraucher | Peltier, BTS7960, Luefter und Summer getrennt/gesperrt |

Es werden keine Pinzahlen aus einem aehnlichen Board uebernommen. Die
verwendeten Pins und Busfrequenzen gehoeren in das Spikeprotokoll und erst nach
Messung in ein spaeteres Hardwareprofil.

### Identische Testfaelle

Jeder Kandidat durchlaeuft in derselben Reihenfolge:

1. Kaltstart und Initialisierung mit schwarzem, aktorfreiem Startbild.
2. Querformat 320 x 240 und eindeutige Eckmarkierungen.
3. je 100 Vollbildfuellungen in Schwarz, Weiss, Rot, Gruen und Blau.
4. ASCII sowie deutsche, spanische und englische Beispieltexte mit identischem
   Fontumfang.
5. identische einfache UI-Elemente: Titel, zwei Temperaturwerte, Statuszeile,
   vier grosse Schaltflaechen und Meldungsdialog.
6. Touch-Rohwerte an Ecken, Kanten und Mitte; Druck-/Kontaktstatus erfassen.
7. Fuenfpunktkalibrierung, Neustart und erneute Pruefung.
8. abwechselnde Display- und Touchtransaktionen auf dem gemeinsamen SPI-Bus.
9. 1.000 Zyklen aus Touch lesen, Schaltflaeche zeichnen und Status aktualisieren.
10. kontrollierter Reset waehrend Leerlauf, Zeichnen und Touchabfrage.
11. Betrieb ohne PSRAM und ohne Vollbild-Framebuffer.
12. Fehlen beziehungsweise Stoeren des Touchcontrollers: Anzeige und
    Regelkern duerfen nicht blockieren.

### Messwerte

Pro Kandidat und Baseline werden erfasst:

- erfolgreiche/fehlgeschlagene Initialisierungen und Resetdauer;
- Rotation, Farbfolge, Textdarstellung und Touchkoordinaten;
- Touch-Latenz, Fehl-/Doppelausloesungen und Busfehler;
- Flash und statisches RAM aus dem identischen Build;
- `firmware.bin` und `firmware.elf`;
- freier Heap nach Boot, niedrigster freier Heap und groesster freier
  Heapblock;
- groesster konfigurierter Display-/Sprite-/Transferpuffer;
- Laufzeit und Fehlerzahl der 1.000 Zyklen;
- Bibliothekskonfiguration und eingeschlossene Treiber/Fonts.

### Erfolgskriterien

- reale Controller und 320-x-240-Querformat funktionieren reproduzierbar;
- alle Testelemente sind lesbar, Touchkalibrierung bleibt nach Neustart
  reproduzierbar und der erste Aufweckkontakt kann getrennt werden;
- Shared-SPI verursacht in 1.000 Zyklen keinen Haenger, Watchdog oder
  unerklaerten Reset;
- kein PSRAM und kein Vollbildpuffer werden vorausgesetzt;
- Build passt in die gemessenen Release-1-Budgets mit dokumentiertem Abstand;
- der Adapter kann Bibliothekstypen aus der Anwendung fernhalten;
- Herkunfts- und Lizenzpruefung ist fuer den konkreten Dateisatz abschliessbar.

### Abbruchkriterien

- unkontrollierte GPIO- oder Aktorwirkung;
- wiederholter Haenger, Watchdog oder Buszustand, der nur durch Power-Cycle
  geloest wird;
- falscher oder nicht reproduzierbar identifizierbarer Controller;
- notwendige PSRAM-Abhaengigkeit oder nicht begrenzbarer grosser Puffer;
- nicht aufloesbare Lizenz-/Herkunftsfrage fuer den konkret benoetigten Code;
- Kandidat erfordert einen Toolchainwechsel ohne separaten Ownerentscheid.

### Nicht-Scope und Artefakte

Nicht-Scope: komplette Release-UI, finale Pins, Touchgehause, echte
Serviceaktionen, Aktorbedienung und LVGL-Auswahl. Ergebnisartefakte:

- Schaltplan/Fotos des Testaufbaus und gemessene Pin-/Pegelmatrix;
- je Kandidat fixierte Abhaengigkeits- und Konfigurationsdatei;
- Quellcommit, Lizenzpaket und Buildlog;
- Screenshots/Fotos, Touchroh- und Kalibrierdaten;
- CSV oder Markdown mit allen Messwerten;
- Base-/Kandidaten-Ressourcenvergleich;
- begruendete Ownerentscheidung mit Rueckfallkandidat.

Notwendige Owner-/Hardwareaktion: reale Boardrevision und Modul bereitstellen,
Verdrahtung nach Messung freigeben und den aus dem identischen Vergleich
hervorgehenden Kandidaten auswaehlen.

## Spike B: DS18B20 und 1-Wire

### Ziel

Den kleinsten stabilen Sensorstack fuer die reale Topologie bestimmen und
Adressierung, asynchrone 12-Bit-Konvertierung, mehrere Sensoren, Hot-Plug,
Fehleruebersetzung und Ressourcen belegen.

### Kandidaten

1. DallasTemperature `4.0.6` (`dadbbf7d`) plus OneWire `2.3.8`
   (`800f26f3`)
2. Espressif `onewire_bus 1.1.1` (`a269e1fe`) plus `ds18b20 0.4.0`
   (`bf92b0b3`)

Der Espressif-Kandidat darf nur getestet werden, wenn er mit der bestehenden
Arduino-ESP32-2.0.17-/PlatformIO-Toolchain ohne verdeckten Frameworkwechsel
gebaut werden kann. Andernfalls lautet das Ergebnis reproduzierbar
`INCOMPATIBLE_WITH_CURRENT_TOOLCHAIN`, nicht "Treiber ungeeignet".

### Hardwareaufbau und Buskonfiguration

| Punkt | Festlegung |
|---|---|
| Sensoren | genau drei reale DS18B20: Schrankluft, Produkt, Kuehlkoerper |
| Betrieb | 3-Leiter, keine Parasitspeisung |
| Adressen | 64-Bit-ROM jedes Sensors vor Rollenbindung erfassen |
| Topologie A | je Sensor separater Bus, sofern GPIO-Budget nach #29 bestaetigt |
| Topologie B | beide festen Sensoren gemeinsam, Produkt auf getrenntem externem Bus |
| Pull-ups/Leitungen | reale Werte, Laengen und Steckverbindung messen und protokollieren; nicht vorgeben |
| Aktoren | Peltier/H-Bruecke gesperrt; Spike erzeugt nur Messereignisse |

### Identische Testfaelle

1. Ein Sensor: ROM lesen, 9/10/11/12 Bit konfigurieren, Temperatur und CRC
   wiederholt lesen.
2. Drei Sensoren einzeln auf getrennten Bussen.
3. Zwei feste Sensoren auf einem Bus und Produktfuehler separat.
4. Enumeration und stabile 64-Bit-Adressen ueber zehn Neustarts.
5. asynchrone 12-Bit-Konvertierung: Trigger, andere Arbeit, fruehester gueltiger
   Read; keine blockierende Wartezeit im Anwendungspfad.
6. Produktfuehler vor Boot fehlend, waehrend Betrieb entfernen und wieder
   anschliessen.
7. festen Sensor entfernen und wieder anschliessen; nur Treiberstatus erfassen,
   keine Safety-Regel im Spike erfinden.
8. Bus kurzschliessen/unterbrechen nur mit sicherer strombegrenzter Methode;
   Timeout und Wiederherstellung messen.
9. CRC- beziehungsweise Datenfehler soweit reproduzierbar injizieren.
10. 1.000 Messzyklen bei ungefaehr zwei Sekunden mit Zeitstempeln.
11. absichtlich ungueltige Rolle/Adresse: Adapter lehnt eindeutig ab.
12. Reset waehrend Konvertierung und anschliessende Neuinitialisierung.

### Messwerte

- Erkennungs-, Konvertierungs- und Lesedauer pro Aufloesung;
- Anzahl erfolgreicher Reads, CRC-/Bus-/Timeoutfehler und Wiederanschlusszeit;
- ROM-Stabilitaet und Reihenfolge der Enumeration;
- blockierte CPU-Zeit und maximaler Adapterpuffer;
- Flash, statisches RAM, `firmware.bin`, `firmware.elf`;
- freier/niedrigster Heap und groesster freier Heapblock;
- zusaetzliche Framework-/Buildkomplexitaet und transitive Abhaengigkeiten;
- Verhalten auf beiden zulaessigen Topologien.

### Erfolgskriterien

- alle realen Sensoren sind ueber stabile 64-Bit-Adressen unterscheidbar;
- mehrere Sensoren und beide Topologien funktionieren;
- 12-Bit-Konvertierung ist ohne blockierende Wartephase integrierbar;
- fehlender Sensor, Busfehler, CRC-Fehler und Wiederanschluss sind typisiert und
  verursachen keine veraltete Messung als neuen gueltigen Wert;
- 1.000 Zyklen ohne Haenger, Watchdog oder unerklaerten Reset;
- Adapter bleibt schmal; Rollen-/Safety-Logik bleibt ausserhalb;
- Ressourcen und Toolchainkomplexitaet sind gemessen.

### Abbruchkriterien

- Bibliothek blockiert laenger als der dokumentierte und begrenzte Messvertrag
  ohne unterbrechbare Alternative;
- Fehler oder Hot-Plug erfordern einen Geraetereset;
- stabile ROM-Adressierung oder Mehrsensorbetrieb ist nicht moeglich;
- Toolchainwechsel, ungebundene Task-/Heapnutzung oder nicht aufloesbare
  transitive Abhaengigkeiten;
- unkontrollierte elektrische oder thermische Situation.

### Nicht-Scope und Artefakte

Nicht-Scope: `VALID`/`STALE`/`FAILED`, Filter, Offsets, Ersatzbetrieb,
PI-Regelung, Aktorfreigabe und finale Sensorposition. Ergebnisartefakte:

- Aufbau-/Topologiefotos, Pull-up-/Leitungsdaten und ROM-Liste;
- identischer Testcode je Kandidat hinter demselben Adaptervertrag;
- Messdaten und Fehlerprotokoll;
- Base-/Kandidaten-Ressourcenvergleich;
- Toolchain-/Abhaengigkeitsbericht;
- begruendete Ownerentscheidung und Rueckfallkandidat.

Notwendige Owner-/Hardwareaktion: alle drei realen Sensoren, Produktstecker und
Leitungen bereitstellen; zulaessige Stoer-/Hot-Plug-Tests bestaetigen; Auswahl
erst anhand des identischen Messprotokolls treffen.

## Reihenfolge und Entscheidungsprotokoll

1. #29 bestaetigt Board, Flash, sichere Pins und Ressourcenbaseline ohne Aktoren.
2. Display-/Touch- und DS18B20-Spikes laufen unabhaengig, aktorfrei und mit
   fixierter Toolchain.
3. Herkunfts-/Lizenzpruefung wird fuer die zwei technisch besten Kandidaten
   aktualisiert.
4. Owner waehlt je Gruppe genau einen Produktivkandidaten und einen
   dokumentierten Rueckfallkandidaten.
5. Erst danach implementieren #30 und #31 die schmalen Adapter.
