# Hardware

## Statuskennzeichnung

| Kennzeichnung | Bedeutung |
|---|---|
| `confirmed` | Durch Datenblatt oder Messung fuer die genannte Eigenschaft bestaetigt |
| `candidate` | Plausibler Vorschlag, aber nicht am realen Aufbau geprueft |
| `unconfirmed` / `unverified` | Noch nicht am gelieferten Exemplar bestaetigt |
| `unknown` / `TBD` | Offen; darf in Firmware und Verdrahtung nicht als Fakt verwendet werden |

Der Build verwendet das generische PlatformIO-Ziel `esp32dev` fuer ein
ESP32-WROOM-32E-Modul. Das bestaetigt weder Platinenrevision noch GPIO-
Zuordnung, aktive Pegel oder Flashparameter des realen Controllerboards.

## 1. Systemübersicht

Der Fermentationsschrank nutzt ein vorhandenes thermoelektrisches Kühl-/Heizmodul. Das Peltier wird mit 12 V betrieben und nimmt laut Geräteangabe ungefähr 60 W auf, entsprechend etwa 5 A bei Nennbetrieb. Die Stromrichtung bestimmt Heizen oder Kühlen.

Geplante Architektur:

```text
12-V-Netzteil
│
├── ESP32-32E Quad-MOSFET-Board
│   ├── MOS-Kanal 1 → Innenlüfter
│   ├── MOS-Kanal 2 → Aussenlüfter
│   ├── MOS-Kanal 3 → Reserve
│   ├── MOS-Kanal 4 → Reserve
│   ├── SPI → MSP2807 Display + Touch
│   ├── 1-Wire → 2 × DS18B20
│   └── GPIO → BTS7960-Steuersignale
│
└── Sicherung → BTS7960-H-Brücke → 12-V-/60-W-Peltier
```

## 2. Bestellte Hardware

| Komponente | Modell | Menge / Verwendung | Dokumentation |
|---|---|---:|---|
| Touchdisplay | TZT MSP2807, ILI9341, 2,8 Zoll, 320 × 240, resistiver Touch | 1 | [LCDWiki](https://www.lcdwiki.com/2.8inch_SPI_Module_ILI9341_SKU:MSP2807) |
| Controller | ESP32-32E Quad MOS Switch Module | 1 | [Lieferanten-PDF](https://ae-pic-a1.aliexpress-media.com/kf/S7c5b73fdea75479fb2d7ada7cce7530cp.pdf?spm=a2g0o.detail.0.0.7988Rg7IRg7IkP&file=S7c5b73fdea75479fb2d7ada7cce7530cp.pdf) |
| Temperaturfühler | DS18B20, wasserdichte 3-Leiter-Sonden | 5 vorhanden; 2 geplant | [Datenblatt](https://www.analog.com/media/en/technical-documentation/data-sheets/ds18b20.pdf) |
| Peltier-Leistungsstufe | Doppel-BTS7960 / IBT-2, 43-A-Angebotsbezeichnung | 1 | [BTS7960-Datenblatt](https://www.infineon.com/assets/row/public/documents/10/57/infineon-bts7960-ds-en.pdf) |
| Programmieradapter | FT232RL/FTDI USB-C zu TTL, 3,3/5 V | 1 | [Lieferanten-PDF](https://ae-pic-a1.aliexpress-media.com/kf/Sa1b31f527aa04ebeb8f318bef00224866.pdf?spm=a2g0o.detail.0.0.1135Jkg1Jkg1VI&file=Sa1b31f527aa04ebeb8f318bef00224866.pdf) |

## 3. ESP32-32E Quad-MOSFET-Board

### 3.1 Angaben aus der Lieferantendokumentation

- ESP32-WROOM-32E / ESP32-32E
- WLAN und Bluetooth/BLE
- 4 MB Flash
- vier N-Kanal-MOSFET-Schaltausgänge
- DC-Eingang 5–60 V; 12 V wird empfohlen
- alternativ 5-V-Versorgung über USB-C
- UART-Pins zum Flashen sind herausgeführt
- IO0 kann zum Start des UART-Bootloaders mit GND verbunden werden
- alle wesentlichen ESP32-I/Os sind auf Stiftleisten herausgeführt
- ungefähre Platinenabmessungen: 55 × 57,5 mm

### 3.2 Noch zu verifizieren

- USB-C ist voraussichtlich nur zur Versorgung vorgesehen; kein USB-UART-Chip ist auf den Produktbildern erkennbar.
- Exakte Zuordnung der vier MOSFET-Ausgänge zu GPIOs.
- Active-high oder active-low der MOSFET-Ausgänge.
- Strombelastbarkeit der einzelnen MOSFET-Ausgänge bei Dauerbetrieb.
- Stromreserve der internen 5-V-Schiene für Display und BTS7960-Logik.
- Verhalten der Ausgänge während Reset und Bootloaderbetrieb.

Auf den Produktbildern erscheinen GPIO16, GPIO17, GPIO26 und GPIO27 als
naheliegende Kandidaten für die vier MOSFET-Kanäle. Kanalzuordnung und aktive
Pegel sind `unverified`; die Zuordnung **darf erst nach Messung als verbindlich
übernommen werden**. Vorher gilt weder LOW noch HIGH als sicherer
Ausschaltpegel.

### 3.3 Vorgesehene Nutzung

| Onboard-Kanal | Last | Verhalten |
|---:|---|---|
| 1 | Innenlüfter | während des Programms, gegebenenfalls dauernd langsam |
| 2 | Aussenlüfter | mindestens bei aktivem Peltier, mit Nachlauf |
| 3 | Reserve | nicht belegt |
| 4 | Reserve | nicht belegt |

## 4. Touchdisplay TZT MSP2807

### 4.1 Dokumentierte Displaydaten

- 2,8-Zoll-TFT
- ILI9341
- 320 × 240 Pixel
- 65k Farben
- 4-Draht-SPI
- Versorgung: 3,3–5 V
- Logikpegel: 3,3 V
- Modulgrösse: ungefähr 50 × 86 mm
- resistiver Touch bei der Variante MSP2807
- Micro-SD-Steckplatz vorhanden, vorerst nicht benötigt

### 4.2 Anschlussbelegung des 14-poligen Moduls

| Pin | Bezeichnung | Funktion | Geplante Nutzung |
|---:|---|---|---|
| 1 | VCC | 3,3/5-V-Versorgung | vorzugsweise 5 V, nach Prüfung der Board-Schiene |
| 2 | GND | Masse | gemeinsame Masse |
| 3 | CS | LCD Chip Select | eigener GPIO |
| 4 | RESET | LCD Reset, active-low | eigener GPIO oder definierte Resetbeschaltung |
| 5 | DC/RS | Daten/Befehl | eigener GPIO |
| 6 | SDI/MOSI | SPI-Daten zum Display | gemeinsamer SPI-MOSI |
| 7 | SCK | SPI-Takt | gemeinsamer SPI-SCK |
| 8 | LED | Hintergrundbeleuchtung | zunächst 3,3 V dauerhaft; spätere Dimmung optional |
| 9 | SDO/MISO | SPI-Daten vom Display | gemeinsamer SPI-MISO |
| 10 | T_CLK | Touch-SPI-Takt | gemeinsam mit SCK |
| 11 | T_CS | Touch Chip Select | eigener GPIO |
| 12 | T_DIN | Touch-SPI-Eingang | gemeinsam mit MOSI |
| 13 | T_DO | Touch-SPI-Ausgang | gemeinsam mit MISO |
| 14 | T_IRQ | Touch-Interrupt, active-low | optional; Polling ist möglich |

Display und Touch teilen SCK, MOSI und MISO. Minimal erforderlich sind daher
sechs Signale: SCK, MOSI, MISO, LCD_CS, LCD_DC und TOUCH_CS. Alle konkreten
ESP32-GPIOs in `config/pins.example.yaml` sind `candidate_unconfirmed`. RESET
und T_IRQ sind optional beziehungsweise separat loesbar.

### 4.3 Noch zu verifizieren

- tatsächlicher Touchcontroller und kompatible Bibliothek; bei diesen Modulen ist XPT2046-kompatible Hardware üblich, aber am gelieferten Modul zu bestätigen
- Touchkalibrierung und Displayrotation
- tatsächliche VCC-/Backlight-Beschaltung des gelieferten Moduls

## 5. Temperaturmessung DS18B20

Zwei von fünf vorhandenen Sensoren werden eingesetzt:

1. **Luft-/Schranksensor:** schnelle Erfassung der Lufttemperatur und Begrenzung der maximalen Lufttemperatur.
2. **Produkt-/Referenzsensor:** in einer verschlossenen Referenzflasche mit Wasser; dient als Annäherung an die Produkttemperatur und startet den Fermentationstimer.

Beide Sensoren teilen sich einen 1-Wire-Datenpin. Jeder Sensor besitzt eine eindeutige 64-Bit-Adresse.

```text
ESP32 3,3 V ───────── beide Sensoren VDD
ESP32 GND   ───────── beide Sensoren GND
ESP32 GPIO  ───────── beide Sensoren DATA
                  │
                4,7 kΩ
                  │
                3,3 V
```

Anforderungen:

- 3-Leiter-Betrieb, kein Parasite-Power-Modus
- 4,7-kΩ-Pull-up nach 3,3 V
- Sensorrollen über gespeicherte ROM-Adressen eindeutig zuordnen
- bei Ausfall, CRC-Fehler oder unplausiblen Werten Leistungsstufe deaktivieren
- Metallsonden nicht ohne bestätigte Lebensmitteleignung direkt in Lebensmittel eintauchen

## 6. BTS7960-H-Brücke

Die BTS7960-/IBT-2-Platine dient ausschliesslich der Peltier-Leistungssteuerung:

- Ein/Aus
- Heizrichtung
- Kühlrichtung
- zeitproportionale Leistungsdosierung in langen Schaltfenstern

### 6.1 Standardanschlüsse

| Anschluss | Funktion |
|---|---|
| B+ / B- | 12-V-Leistungsversorgung |
| M+ / M- | Peltieranschluss |
| VCC / GND | Logikversorgung, üblicherweise 5 V |
| RPWM | Richtung 1 / Heiz- oder Kühlrichtung |
| LPWM | Gegenrichtung |
| R_EN / L_EN | Enable-Eingänge |
| R_IS / L_IS | Diagnose-/Strommesssignale, zunächst optional |

### 6.2 Steuerregeln

- RPWM und LPWM niemals gleichzeitig aktiv ansteuern.
- R_EN und L_EN können gemeinsam geschaltet werden.
- Vor Richtungswechsel Leistung deaktivieren und mindestens 2 s Totzeit einhalten.
- Das Peltier wird nicht mit hochfrequentem direktem PWM geregelt. Vorgesehen ist zeitproportionales Ein/Aus mit langen Fenstern, zum Beispiel 30–60 s.
- Leistungszweig mit 7,5-A-Sicherung absichern.
- Leitungsquerschnitt für den 5-A-Zweig mindestens 1,0 mm² bei kurzen Leitungen.

## 7. FT232RL-Programmieradapter

Der Adapter wird für das erstmalige Flashen und als UART-Diagnoseschnittstelle verwendet. Spätere Updates sollen über OTA möglich sein.

### 7.1 UART-Verbindung

```text
FT232 TX  → ESP32 RX0
FT232 RX  → ESP32 TX0
FT232 GND → ESP32 GND
```

- UART-Pegel auf 3,3 V einstellen.
- TX und RX gekreuzt anschliessen.
- IO0 beim Einschalten/Reset mit GND verbinden, um den Bootloader zu starten.
- Nach dem Flashen IO0 von GND trennen und das Board neu starten.
- Das ESP32-Board separat über USB-C 5 V oder den 12-V-Eingang versorgen. Die Versorgung nicht zusätzlich über den FT232RL einspeisen, solange die genaue Adapterbeschaltung nicht geprüft ist.

## 8. Stromversorgung

Vorhanden ist bisher eine 12-V-Versorgung aus der ursprünglichen Kühl-/Wärmebox.

Zu prüfen:

- tatsächliche Dauerstromfähigkeit des Netzteils
- ob die Angabe 60 W nur das Peltier oder das gesamte Originalgerät einschliesslich Lüfter betrifft
- Stromaufnahme beider Lüfter
- Spannungsabfall bei gleichzeitigem Peltierbetrieb, WLAN-Sendeimpulsen und Displaybeleuchtung

Empfohlene Verteilung:

```text
12 V
├── 7,5-A-Sicherung → BTS7960 → Peltier
├── ESP32-Quad-MOSFET-Board
├── MOS-Ausgang → Innenlüfter
└── MOS-Ausgang → Aussenlüfter
```

Display und BTS7960-Logik dürfen erst aus dem 5-V-Pin des ESP32-Boards versorgt werden, wenn die Schiene unter Last geprüft wurde. Bei instabilen 5 V ist ein separater 12→5-V-Buck vorzusehen.

## 9. Vorläufige GPIO-Planung

Die vorläufige Zuordnung steht in [`config/pins.example.yaml`](../config/pins.example.yaml). Sie ist so gewählt, dass:

- klassische ESP32-VSPI-Pins für Display und Touch genutzt werden,
- UART0 für den FT232RL frei bleibt,
- Boot-Strapping-Pins möglichst vermieden werden,
- die mutmasslichen Onboard-MOSFET-Pins nicht doppelt verwendet werden.

Die Datei darf erst nach Hardwareprüfung in eine verbindliche `pins.yaml` übernommen werden.

GPIO4 wird wegen seiner Strapping-Funktion nicht als Display-Reset-Kandidat
gefuehrt. Ob und wie LCD_RESET angeschlossen wird, bleibt `unknown`.

## 10. Verifikation vor Leistungsanschluss

1. Platinenrevision fotografieren und Beschriftungen dokumentieren.
2. ESP32 über UART flashen und seriellen Bootlog prüfen.
3. Alle freien GPIOs mit Testprogramm einzeln schalten.
4. Onboard-MOSFET-Kanäle ohne Last messen und GPIO-Zuordnung dokumentieren.
5. Display ohne Touch initialisieren.
6. Touchcontroller erkennen, kalibrieren und Rotation prüfen.
7. Beide DS18B20-Adressen auslesen und Rollen speichern.
8. BTS7960 ohne Peltier mit Multimeter bzw. kleiner Testlast prüfen.
9. Lüfter einzeln testen.
10. Erst danach Peltier über Sicherung anschliessen.
