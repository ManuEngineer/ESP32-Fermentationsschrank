# Komponentenevaluationen fuer Release 1

Zur Auditnavigation: [`RELEASE_1_ADOPT_OR_BUILD_AUDIT.md`](RELEASE_1_ADOPT_OR_BUILD_AUDIT.md).

## Einordnung

Stand: 2026-07-27. Repository-Basis:
`7713a66cbf51eb078bd0f5e43c1163d1e0f47e1f`.

Dieses Dokument bewertet Kandidaten, bindet aber keine Bibliothek ein und trifft
keine endgueltige Auswahl. "Unterstuetzt" bezeichnet eine Aussage der
offiziellen Projektquelle; reale Kompatibilitaet mit dem bestellten Board ist
bis zum Spike unbestaetigt. Die Zieltoolchain ist PlatformIO
`espressif32@7.0.1` mit Arduino-ESP32 `2.0.17` (`dcc1105b`), ESP32-32E, 4 MB
Flash und ohne PSRAM. Herkunft und Lizenzen stehen im
[`Third-Party-Review`](THIRD_PARTY_SOURCE_AND_LICENSE_REVIEW.md), die Spikes im
[`Hardware-Spike-Plan`](HARDWARE_SPIKE_PLAN.md).

Nachweisarten:

- **Hersteller:** Datenblatt oder offizieller Herstellerdienst;
- **Projekt:** Repository, Paketmanifest oder Projektdokumentation;
- **Repository:** bereits implementierter und getesteter Projektstand;
- **Messung:** erst nach einem definierten Hardware-Spike; derzeit offen.

### Upstream-Aktivitaet und deklarierte Plattformbreite

Die Aktivitaet ist nur ein Wartungsindikator. Sie beweist weder Fehlerfreiheit
noch Kompatibilitaet mit der fixierten Projekttoolchain.

| Kandidat | Letzte sichtbare Upstream-Aktivitaet | Deklarierte Plattform-/Frameworkgrenze |
|---|---|---|
| LovyanGFX | 2026-07 | Arduino-Manifest nennt ESP32 |
| TFT_eSPI | 2026-04 | Arduino-Manifest `architectures=*`, README nennt ESP32 |
| LCDWiki-Paket | Paketzeitstempel 2018, kein versioniertes Upstream-Repository nachgewiesen | Arduino-Demos vorwiegend fuer UNO/Mega; ESP32/PlatformIO unbestaetigt |
| Arduino_GFX | 2026-07 | Arduino-Manifest `architectures=*`, Beschreibung nennt ESP32 |
| Adafruit GFX / ILI9341 | 2026-04 / 2026-02 | `architectures=*`; ILI9341-Quelltext besitzt ESP32-Pfad |
| XPT2046_Touchscreen | 2024-06 | `architectures=*` |
| DallasTemperature / OneWire | 2026-04 / 2025-06 | `architectures=*`; konkrete ESP32-Anpassungen in OneWire dokumentiert |
| Espressif onewire_bus / ds18b20 | 2026-07 / 2026-07 | ESP-IDF-Komponenten; onewire_bus fordert IDF >=5.0 |
| ArduinoJson | 2026-07 | `architectures=*` |
| ESPAsyncWebServer | 2026-07 | Arduino-Manifest nennt ESP32/ESP8266/RP2040 |
| WiFiManager | 2026-02 | Arduino-Manifest nennt ESP32 und ESP8266 |
| ricmoo QRCode / Nayuki QR | Default-HEAD 2020 / 2025-01 | portabler Arduino- beziehungsweise C-Code; Projektcore konkret bauen |
| LVGL | 2026-07 | portables Embedded-Framework; Display-/Speicherintegration projektspezifisch |
| Arduino PID / QuickPID | 2024-05 / 2023-06 | Arduino-Manifeste `architectures=*` |

## ILI9341 und XPT2046

### Release-1-Aufgabe und Vertrag

Das bestaetigte Ziel ist die einzige lokale Bedien- und Anzeigeoberflaeche: ein
320-x-240-Touchdisplay im Querformat. Die Weboberflaeche ist sekundaer;
Encoder, Programmwahlschalter, Start-/Stop-Taster und Status-LED sind kein
Bestandteil des Projekts. ILI9341 stammt bisher aus der Lieferantenbeschreibung;
XPT2046 ist nur wahrscheinlich. Vor der Kandidatenbewertung identifiziert
Stufe 0 am gelieferten Modul Variante, beide Controller, Versorgung,
Logikpegel, Chip-Selects, Reset, Data/Command, Hintergrundbeleuchtung,
SPI-Topologie, reale Modulbelegung und Bootzustaende. Controller, Pins,
Rotation, Reset, gemeinsamer SPI-Bus, Kalibrierung, Boot-Recovery und Ressourcen
bleiben bis zu diesem Nachweis `TBD_HARDWARE`.

Alle drei Hauptkandidaten durchlaufen Stufe 1 fuer Quellen, konkret benoetigte
Dateien, Lizenzen, Abhaengigkeiten und reproduzierbaren Build. Nur ausreichend
erfolgreiche Kandidaten erreichen den kurzen identischen Hardware-Smoke-Test in
Stufe 2; nur `PASS_SMOKE_TEST` fuehrt zur vollstaendigen Matrix in Stufe 3.
Stufe 4 benennt genau einen bevorzugten Treiberstack und einen
Rueckfallkandidaten. Die zwei Reservekombinationen werden nur bei einem
dokumentierten Ausloeser nachgezogen und nicht vorsorglich voll implementiert.

| Kandidat | Hersteller-/Referenzbezug | Gepruefter Stand und Lizenz | ESP32-/PlatformIO-Aussage | Erwartete Ressourcenwirkung | Notwendiger Adapter | Risiken und Hardwaretest | Vorlaeufige Empfehlung |
|---|---|---|---|---|---|---|---|
| LovyanGFX | Projekt nennt ILI9341, ESP32 und Touchunterstuetzung | `1.2.26`, `3f78b705`; FreeBSD plus dokumentierte Ursprungslizenzen | `architectures=esp32`; konkrete Kompatibilitaet mit Arduino-ESP32 2.0.17 messen | Treiber, Fonts und optionale Sprites; kein Vollbildpuffer erzwingen | schmaler Display-/Touchadapter, feste Bus- und Pufferkonfiguration | Boardprofil, XPT2046, Shared-SPI, Heap und Reset real pruefen | verbindlicher Hauptkandidat fuer Stufe 1; nicht ausgewaehlt |
| TFT_eSPI | Projekt nennt ILI9341 und ESP32 | Manifest `2.5.44`, `16e37595`; FreeBSD plus Ursprungsbestandteile | `architectures=*`; User-Setup ist buildzeitnah und muss reproduzierbar gekapselt werden | optimierter Treiber und Fonts; Konfiguration kann ungenutzte Treiber einbeziehen | Displayadapter plus projektspezifische, versionierte Setupdatei; Touch separat oder integriert pruefen | globale Konfiguration, Shared-SPI, Touchkalibrierung und Upstream-Updates | verbindlicher Hauptkandidat fuer Stufe 1; nicht ausgewaehlt |
| LCDWiki-Paket | lokale Kopie der zum MSP2807 gelieferten Demos und Treiber | Paketdateien 2018; MIT-Dateien in `LCDWIKI_GUI`, `LCDWIKI_SPI`, `LCDWIKI_TOUCH`; Paketherkunft/Abdeckung erneut pruefen | Demos zielen vorwiegend auf Arduino UNO/Mega; ESP32- und PlatformIO-Tauglichkeit unbestaetigt | unbekannt; altes Paket mit mehreren Demos, Fonts und Controllerpfaden | bei positiver Untersuchung nur kleinster klar lizenzierter Teil hinter Adapter; keine direkte Gesamtuebernahme | fehlende moderne ESP32-Referenz, Paketalter, Abdeckung aller Dateien, Pins und Touch real pruefen | verbindlicher Hauptkandidat fuer Stufe 1 und interne Herstellerreferenz; keine allgemeine rechtliche oder technische Freigabe |
| Arduino_GFX plus geeigneter Touchadapter | Arduino_GFX nennt ILI9341 und ESP32-SPI; separater Touchadapter nach bestaetigtem Controller | `1.6.7`, `fe33cad8`, BSD; XPT2046-Touch `1.4`, `f956c5d8`, MIT im Header, falls der Controller bestaetigt wird | Arduino-Manifeste `architectures=*`; konkrete alte-Core-Kompatibilitaet messen | zwei Bibliotheken, Adapterschicht und moeglicherweise weniger integrierte Shared-SPI-Koordination | getrennte Display- und Touchadapter | zwei Lebenszyklen, Kalibrierung, Busarbitrierung, Reset | Reservekandidat; nur bei dokumentiertem Ausloeser nachziehen |
| Adafruit GFX + ILI9341 + geeigneter XPT2046-Touchadapter | Adafruit-Treiber dokumentiert ILI9341/ESP32; Touchadapter erst nach Controllerbestaetigung | GFX `1.12.6`/`ac6d7c38`, ILI9341 `1.6.3`/`dbb447af`, BSD; XPT2046-Touch MIT | `architectures=*`; zusaetzliche Adafruit-Abhaengigkeiten und Core-Kompatibilitaet pruefen | mehrere Bibliotheken und BusIO; Referenz eher Portabilitaet als minimales ESP32-Profil | Display- und Touchadapter, ungenutzte Abhaengigkeiten vermeiden | Abhaengigkeitsumfang, Shared-SPI und Performance messen | Reservekandidat; nur bei dokumentiertem Ausloeser nachziehen |

Reservekandidaten werden nur nachgezogen, wenn weniger als zwei Hauptkandidaten
Stufe 2 bestehen, alle Hauptkandidaten ein wesentliches Ressourcen-, Wartungs-,
Stabilitaets- oder Integrationsproblem besitzen, der erforderliche publizierte
Dateisatz eine ungeklaerte Lizenz-/Herkunftsfrage behaelt, keine belastbare
Auswahl moeglich ist oder ein Reservekandidat einen nachgewiesenen wesentlichen
R1-Vorteil besitzt.

Verbleibende eigene Logik: Touchkalibrierung, Ereignisentprellung,
Aufweckschutz, UI-Navigation, Sicherheits- und Kommandosemantik. Kein Treiber
darf eine Aktorfreigabe ausloesen. Entscheidungsstatus: `SPIKE_REQUIRED`.

Quellen: [LovyanGFX](https://github.com/lovyan03/LovyanGFX),
[TFT_eSPI](https://github.com/Bodmer/TFT_eSPI),
[Arduino_GFX](https://github.com/moononournation/Arduino_GFX),
[Adafruit GFX](https://github.com/adafruit/Adafruit-GFX-Library),
[Adafruit ILI9341](https://github.com/adafruit/Adafruit_ILI9341),
[XPT2046_Touchscreen](https://github.com/PaulStoffregen/XPT2046_Touchscreen),
[`references/LINKS.md`](../../references/LINKS.md). Alle Onlinequellen abgerufen
am 2026-07-27.

## DS18B20 und 1-Wire

### Release-1-Aufgabe und Vertrag

Drei DS18B20 werden im 3-Leiter-Betrieb bei 12 Bit ungefaehr alle zwei Sekunden
abgefragt. Der optionale Produktfuehler ist bei Verwendbarkeit primaerer
Regelsensor, darf im Stillstand und in einem dafuer zulaessigen Lauf fehlen und
muss bei Entfernen/Wiederanschliessen ein technisches Anwesenheitsereignis
erzeugen. Sein externer Bus darf die festen Sensorbusse nicht elektrisch
beeintraechtigen. Der feste Raum-/Luftsensor ist regulaerer Ersatz, nicht
pauschal primaer. Der feste Kuehlkoerper-/Peltier-Schutzsensor ist verpflichtende
Sicherheitsgrundlage; fehlendes, ungueltiges, veraltetes oder nicht ausreichend
vertrauenswuerdiges Signal sperrt die Peltierfreigabe ausserhalb des Treibers.

Softwarestack und elektrische Bustopologie sind getrennte Entscheidungen. Beide
Kandidaten durchlaufen Stufe 1 fuer Quelle/Lizenz/Build, Stufe 2 mit einem realen
Sensor und nur nach Erfolg die identische Topologie-/Fehlermatrix der Stufe 3.
Dabei werden Topologie A mit drei getrennten Bussen und Topologie B mit separatem
Produktfuehler sowie gemeinsamem festen Bus identisch verglichen. Topologie C
mit allen Sensoren auf einem Bus ist keine regulaere Zielvariante.

| Kandidat | Hersteller-/Referenzbezug | Gepruefter Stand und Lizenz | ESP32-/PlatformIO-Aussage | Erwartete Ressourcenwirkung | Notwendiger Adapter | Risiken und Hardwaretest | Vorlaeufige Empfehlung |
|---|---|---|---|---|---|---|---|
| DallasTemperature + OneWire | verbreitete Arduino-Abstraktion ueber den DS18B20- und 1-Wire-Vertrag | DallasTemperature `4.0.6`, `dadbbf7d`, MIT; OneWire `2.3.8`, `800f26f3`, MIT im Quelltext | beide `architectures=*`; OneWire nennt ESP32-Anpassungen | zwei kleine Bibliotheken; Flash-/RAM-/Heapwirkung in Stufe 1 und 3 messen | technischer Adapter mit Bus-ID, ROM, Mess-/Zeit-/CRC-/Anwesenheits-/Timeout-/Fehlerstatus | neue DallasTemperature-Hauptversion, alter Arduino-Core, Mehrbus/Mehrsensor, Hot-Plug und Timing pruefen | verbindlicher Kandidat 1; keine Auswahl vor Stufe 3 |
| Espressif onewire_bus + ds18b20 | offizielle Espressif-Komponenten, RMT/UART-Backend, Enumeration und CRC8 | `onewire_bus 1.1.1`, `a269e1fe`; `ds18b20 0.4.0`, `bf92b0b3`; Apache-2.0 | Registry fordert fuer onewire_bus ESP-IDF >=5.0; aktuelles Projekt nutzt Arduino-ESP32 2.0.17 auf IDF 4.4, direkte Integration daher unbestaetigt | RMT/UART-Ressourcen und optionale Sensor-Hub-Abhaengigkeit; messen | derselbe technische Plattformport; keine IDF-Typen in der Anwendung | Toolchain-Mismatch, Komponentenmanager in PlatformIO-Arduino, Mehrbus/Mehrsensor und optionale Sensor-Hub-Grenze pruefen | verbindlicher Kandidat 2; bei Toolchainkonflikt `INCOMPATIBLE_WITH_CURRENT_TOOLCHAIN`, keine allgemeine Untauglichkeitsaussage |

Der Produktfuehler erhaelt verbindlich einen eigenen Bus. Ein eigener Bus auch
fuer den Schutzsensor ist bevorzugt; der gemeinsame feste Bus aus Topologie B
bleibt Rueckfall, falls die reale Pinpruefung keinen dritten unproblematischen
GPIO ergibt. Vorab werden keine drei GPIOs reserviert.

Die vorgesehene 3,5-mm-TRS-Verbindung verwendet `Tip = VDD`, `Ring = DQ` und
`Sleeve = GND`. Die beabsichtigte Kontaktfolge mit VDD zuletzt beim Einstecken
und zuerst beim Herausziehen wird nicht aus der TRS-Bauform behauptet, sondern
an der konkreten Buchse inklusive Teilstecken, Kurzschluss-, Prell-, Last- und
mindestens 100 Steckzyklen praktisch geprueft. Pull-up, DQ-Serienwiderstand,
Strombegrenzung, Entkopplung, ESD-Schutz und sichere Bootzustaende werden erst
nach realem Signal- und Stecktest dimensioniert.

Verbleibende eigene Safety-/Fachlogik: Rollenbindung, Produktfuehler als
optionaler primaerer Regelsensor, Raum-/Luftsensor als regulaerer Ersatz,
Kuehlkoerper-/Peltier-Schutzsensor als verpflichtende Freigabegrundlage,
`VALID`/`STALE`/`FAILED`, Filter, Plausibilitaet, Offset, Rueckkehr und
Aktorfreigabe. Der Adapter liefert nur Bus-ID, ROM-Adresse, Messwert, Zeitpunkt,
Aufloesung, CRC, Anwesenheit, Timeout und technischen Fehlerstatus.
Entscheidungsstatus: `SPIKE_REQUIRED` fuer Softwarestack und Topologie getrennt.

Quellen: [DallasTemperature](https://github.com/milesburton/Arduino-Temperature-Control-Library),
[OneWire](https://github.com/PaulStoffregen/OneWire),
[Espressif onewire_bus 1.1.1](https://components.espressif.com/components/espressif/onewire_bus/versions/1.1.1/readme?language=en),
[Espressif ds18b20 0.4.0](https://components.espressif.com/components/espressif/ds18b20/versions/0.4.0/readme?language=en),
[DS18B20-Datenblatt](https://www.analog.com/media/en/technical-documentation/data-sheets/ds18b20.pdf).
Abgerufen am 2026-07-27.

## BTS7960

| Merkmal | Bewertung |
|---|---|
| Aufgabe | H-Brueckenadapter fuer exklusive Vorwaerts-/Rueckwaertsanforderung |
| Release-1-Anforderung | Boot AUS, beide Richtungen nie gleichzeitig, Pulldowns, Totzeit, begrenzte Servicepulse, optional R_IS/L_IS nach Messung |
| Kandidaten | Arduino-GPIO/LEDC aus dem fixierten Framework; kein hochstufiger BTS7960-Treiber erforderlich |
| Herstellerquelle | [Infineon BTS7960-Datenblatt](https://www.infineon.com/assets/row/public/documents/10/57/infineon-bts7960-ds-en.pdf), lokale Kopie unter `references/datasheets/` |
| Version/Lizenz | Framework Arduino-ESP32 `2.0.17`; Herstellerdatenblatt ist Referenz, kein uebernommener Code |
| Kompatibilitaet | Framework ist Teil des Zielbuilds; Modulvariante, Pegel und Beschaltung bleiben unbestaetigt |
| Ressourcen | kleiner GPIO-/Timeradapter; reale Timerbelegung und Builddelta messen |
| Adapter | `IBidirectionalActuatorSink`-Implementierung mit sicherer Initialisierung; keine Rollenbegriffe im Plattformport |
| Eigene Logik | gesamte Safety-Freigabe, Mindestzeiten, Totzeit, Fehlerreaktion und Servicebegrenzung |
| Risiken/Hardwaretest | Lieferantenboard ist nicht identisch mit dem Infineon-Bauteil; Modulschaltung, Enable, Polaritaet und R_IS/L_IS real messen |
| Empfehlung/Status | `CONFIGURE_FRAMEWORK` plus eigener Adapter; `BLOCKED_HARDWARE`, keine externe BTS7960-Bibliothek auswaehlen |

## Luefter und MOSFET-Ausgaenge

| Merkmal | Bewertung |
|---|---|
| Aufgabe | zwei 12-V-Luefter und der aktive Summer als einziges zusaetzliches lokales Ausgabeelement ueber bestaetigte binaere Ausgaenge |
| Release-1-Anforderung | sichere Bootpegel, Innenluefterbetrieb, Aussenluefter ohne absichtlichen Vorlauf und mit Nachlauf, Summer fuer nicht blockierende akustische Warnungen und Hinweise; keine Status-LED |
| Kandidaten | Arduino-ESP32 GPIO/LEDC; keine Geraeterollen in der Plattform |
| Quelle/Stand/Lizenz | [Arduino-ESP32](https://github.com/espressif/arduino-esp32), Projektstand `2.0.17`/`dcc1105b`, LGPL-2.1 mit Drittbestandteilen |
| Ressourcen | kleine `IBinaryOutputSink`-Adapter; Timer/PWM nur falls reale Hardware es verlangt |
| Eigene Logik | Nachlauf, Sicherheitsprioritaet, Meldungsmuster und Rollenbindung |
| Risiken/Hardwaretest | Kanalbelegung und aktive Pegel der Quad-MOSFET-Platine sind nur `confirmed_order`; Strom, Anlauf und Reset messen |
| Empfehlung/Status | `CONFIGURE_FRAMEWORK`; keine separate Luefterbibliothek; `BLOCKED_HARDWARE` |

Der 230-V-AC-Hauptschalter gehoert nicht zu diesen Ausgaengen und erhaelt auch
keinen Eingangsadapter. Er schaltet das Geraet rein elektrisch ein oder aus.

## NVS beziehungsweise Preferences

| Merkmal | Bewertung |
|---|---|
| Aufgabe | produktives Blob-Backend fuer den vorhandenen `IStateStore` |
| Release-1-Anforderung | kurze ADR-016-Schluessel, binaersichere Werte, typisierte Read-/Writefehler, atomarer einzelner Commit und Rueckfalllogik oberhalb des Backends |
| Kandidaten | Arduino-ESP32 Preferences ueber NVS; direkter ESP-IDF-NVS-Adapter nur bei nachgewiesenem Vertragsvorteil |
| Quelle/Stand/Lizenz | [Preferences-Dokumentation](https://docs.espressif.com/projects/arduino-esp32/en/latest/api/preferences.html), Projektstand Arduino-ESP32 `2.0.17`; Framework-/ESP-IDF-Lizenzen |
| Wartung/Kompatibilitaet | offizieller Plattformbestandteil; Keys bis 15 Zeichen und Bytewerte dokumentiert |
| Ressourcen | NVS-Partition und Runtime-Handles; Groesse bleibt bis #29 `TBD_IMPLEMENTATION_BUDGET` |
| Adapter | vorhandenen `IStateStore` implementieren; `CommitOutcomeUnknown` und Limits explizit uebersetzen |
| Eigene Logik | Envelope, Slots, fachliche Revisionen, Recovery und Secrets bleiben im Projekt |
| Risiken/Hardwaretest | reale Atomizitaet, Stromausfallverhalten, Kapazitaet und Flashlebensdauer nicht aus der Hostsimulation ableiten |
| Empfehlung/Status | gemaess ADR-016 `ADAPTER_EXISTING_LIBRARY`; keine eigene Flashdatenbank |

## JSON

| Merkmal | Bewertung |
|---|---|
| Aufgabe | begrenzte Web-API-, Export-, Backup- und Importformate; nicht interne Kontrollpunktpersistenz |
| Release-1-Anforderung | UTF-8, feste Groessen-/Tiefenlimits, deterministische Fehler, keine Secrets, Streaming wo sinnvoll |
| Kandidaten | ArduinoJson `7.4.3`/`7823e4a6`; kleine manuelle Ausgabe nur fuer nachweislich triviale feste Antworten |
| Quelle/Lizenz | [ArduinoJson](https://github.com/bblanchon/ArduinoJson), MIT, aktiv gepflegt |
| Kompatibilitaet | `architectures=*`; genaue Kompatibilitaet mit Arduino-ESP32 2.0.17 und PlatformIO isoliert bauen |
| Ressourcen | Dokumentmodell und Serialisierungspuffer koennen gleichzeitig leben; exakte Kapazitaeten pro Endpunkt messen |
| Adapter | projektspezifische DTO-/Codecgrenze; keine ArduinoJson-Typen im fachlichen Kern |
| Eigene Logik | Schema, Redaction, Berechtigung, Konflikte, Importvorschau und Validierung |
| Risiken/Hardwaretest | ungebundene dynamische Dokumente, tiefe Eingaben und grosse Strings; Fuzz-/Grenztests |
| Empfehlung/Status | `ADOPT_LIBRARY` als bevorzugter Kandidat fuer #19/#27/#28, erst im jeweiligen kleinen PR und mit Limits |

## Webserver

| Kandidat | Gepruefter Stand/Lizenz | Eignung | Ressourcen/Risiken | Empfehlung |
|---|---|---|---|---|
| Arduino-ESP32 `WebServer` | Teil von `2.0.17`; Framework-/Drittkomponentenlizenzen | synchroner, kleiner Frameworkkandidat fuer lokale HTTP-API und statische Ressourcen | darf Regel-/Safety-Aufgaben nie blockieren; Parallelitaet, Upload- und SSE-Bedarf pruefen | zuerst einen begrenzten Prototyp hinter `IWebTransport`-aehnlicher interner Grenze messen |
| ESPAsyncWebServer | `3.12.0`, `a008cccf`, LGPL-3.0 | SSE, WebSocket, statische Dateien und Auth-Helfer; aktiv gepflegt | weitere Abhaengigkeit, asynchroner Lebenszyklus, Callback-/Heaplast und LGPL-Pflichten | nur uebernehmen, wenn der identische Prototyp einen klaren Funktions-/Stabilitaetsvorteil zeigt |

Der Webserver implementiert keine fachlichen Kommandos, Authentisierungsregeln
oder Aktorlogik. Release 1 bleibt lokal per HTTP; keine direkte
Internetfreigabe. Entscheidungsstatus: `EVALUATE_LATER` in #27.

Quellen: [Arduino-ESP32](https://github.com/espressif/arduino-esp32),
[ESPAsyncWebServer](https://github.com/ESP32Async/ESPAsyncWebServer). Abgerufen
am 2026-07-27.

## WLAN-Onboarding

| Kandidat | Stand/Lizenz | Eignung und Grenzen | Empfehlung |
|---|---|---|---|
| WiFiManager | `2.0.17`, `4131fe61`, MIT | Arduino-ESP32-Captive-Portal und Laufzeitparameter; Secret-Lebenszyklus, Ersatz-WLAN, projektspezifische UI und Recovery bleiben eigene Logik | ernsthafter Kandidat fuer einen schmalen Onboardingadapter; exakten Core-Build, Flash/Heap und Portalverhalten pruefen |
| Arduino-ESP32 WiFi + DNS/WebServer | Framework `2.0.17` | kleinster Abhaengigkeitssatz, aber mehr eigener Portal- und Zustandsautomatencode | Gegenkandidat; bevorzugen, falls der spezifizierte Ablauf klein und mit weniger Risiko abbildbar ist |
| Espressif Unified/Network Provisioning | offizielle ESP-IDF-Loesung; aktuelle Dokumentation beschreibt SoftAP/BLE | flexibler und sicherheitsorientiert, aber der aktuelle Arduino-/Release-1-Ablauf verlangt weder BLE noch eine Smartphone-App | nicht ohne Toolchain- und Scopebegruendung uebernehmen; BLE bleibt ausserhalb Release 1 |

Quelle: [WiFiManager](https://github.com/tzapu/WiFiManager),
[Espressif Unified Provisioning](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/provisioning/provisioning.html).
Entscheidungsstatus: `SPIKE_REQUIRED` fuer #27; keine Connectivity-Secrets in
der Bibliothekskonfiguration offenlegen.

## QR-Code

| Kandidat | Stand/Lizenz | Ressourcen und Adapter | Empfehlung |
|---|---|---|---|
| ricmoo/QRCode | `0.0.1`, `eafbde49`, MIT; nennt Project Nayuki als Ursprung | kleine C/C++-Bibliothek; Versions- und Wartungsaktivitaet gering; Ausgabematrix begrenzen | als Arduino-nahe Referenz messen |
| Project Nayuki QR-Code-generator | `1.8.0`, `2c9044de`, MIT im Quellheader | portable C-Implementierung, auf feste QR-Version/ECC und caller-provided Buffer begrenzbar | bevorzugter technischer Gegenkandidat, aber erst nach Lizenz-/Ressourcenpruefung im umsetzenden PR |

Nur der lokale WLAN-QR-String wird erzeugt; Onboardingstatus, Secret-Schutz und
Displaydarstellung bleiben eigene Logik. Entscheidungsstatus: `NOT_SELECTED`.

Quellen: [ricmoo/QRCode](https://github.com/ricmoo/QRCode),
[Project Nayuki](https://github.com/nayuki/QR-Code-generator). Abgerufen am
2026-07-27.

## UI-Framework

LVGL ist kein Display- oder Touchtreiberkandidat. Der Treibervergleich wird
zuerst mit Stufe 4 abgeschlossen. Danach werden der schmale Adaptervertrag und
ein repraesentativer Release-1-Screen auf dem ausgewaehlten Treiber erstellt;
erst dann werden schlanke eigene Views und LVGL auf derselben Hardware mit
demselben Treiber, Screen, Texten, Eingabeelementen und derselben Messmethode
verglichen.

| Kandidat | Stand/Lizenz | Eignung | Ressourcen/Risiken | Empfehlung |
|---|---|---|---|---|
| LVGL | `9.5.0`, `8fd90bb1`, MIT | vollstaendiges Widget-, Layout-, Event- und Renderingframework; kein Display-/Touchtreiber | zusaetzliche Displaypuffer, Fonts, Widgetzustand und Integrationskomplexitaet; 4 MB/ohne PSRAM nach der Treiberwahl messen | nicht vorsorglich einbinden; nur waehlen, wenn der identische repraesentative Screen einen klar gemessenen Vorteil bei Bedienbarkeit, Wartbarkeit oder Umsetzung zeigt und die zusaetzlichen Ressourcen rechtfertigt |
| schlanke projektspezifische Views auf gewaehltem Treiber | keine Drittkomponente fuer Widgets | passt zu wenigen festen 320-x-240-Screens und bestehenden View-Modellen | mehr eigene Layout-/Fokuslogik, aber enger kontrollierbarer Umfang | Vergleichsbasis erst nach Treiberwahl und Adaptervertrag; bevorzugt, solange LVGL keinen belegten Vorteil bringt |

Touchnavigation und fachliche View-Modelle bleiben in der Anwendung. Status:
`EVALUATE_LATER`, keine LVGL-Abhaengigkeit im Treiberspike oder vor
Treiberwahl, Adaptervertrag und repraesentativem Screen.

Quelle: [LVGL](https://github.com/lvgl/lvgl), abgerufen am 2026-07-27.

## Regelung und PID

| Kandidat | Stand/Lizenz | Vertragsfit | Empfehlung |
|---|---|---|---|
| Arduino PID | Manifest `1.2.1`, `524a4268`, MIT im Quellheader | allgemeiner PID mit Arduino-Zeitbezug und PWM-artigem Ausgang; bildet Luftbegrenzung, Safety-Sperren, Richtungswechsel und projektweiten Zeitvertrag nicht ab | nicht fuer Release-1-Kern uebernehmen; nur als Referenz |
| QuickPID | `3.1.9`, `c3f64fa2`, MIT | mehr Anti-Windup-/Timingoptionen, aber weiterhin keine Projekt-Safety- und Aktorsemantik | nicht auswaehlen, solange der spezifizierte eigene PI-Kern kleiner und direkt testbar ist |
| eigener begrenzter PI-Kern | Repositoryvertrag in `TEMPERATURE_CONTROL.md`; noch nicht implementiert | genau ein deterministischer Ausgang `HEAT/OFF/COOL` plus Quote; virtuelle Zeit, Anti-Windup und Luftbegrenzung explizit testbar | `CUSTOM_SAFETY_CORE`; externe Formeln duerfen Referenz sein, keine PID-/Autotune-Funktion in Release 1 |

Ressourcen sind nach #22 Base gegen Head zu messen. Die eigentliche
Maschinenparametrierung bleibt #35 und kann keine unsicheren Defaultwerte aus
einer Bibliothek uebernehmen.

Quellen: [Arduino PID](https://github.com/br3ttb/Arduino-PID-Library),
[QuickPID](https://github.com/Dlloydev/QuickPID). Abgerufen am 2026-07-27.

## Noch nicht bestaetigte Aussagen

- Kein Display-/Touchkandidat ist auf dem gelieferten MSP2807 mit gemeinsamem
  SPI-Bus und der Projekttoolchain getestet.
- Kein DS18B20-Kandidat ist mit den drei realen Sensoren, Leitungen, Pull-ups und
  Hot-Plug getestet.
- Keine Web-/JSON-/UI-Bibliothek besitzt einen Base-/Head-Ressourcennachweis fuer
  die fertige Firmware.
- NVS-Kapazitaet, reale Flashatomizitaet und Lebensdauer sind nicht gemessen.
- Es wird keine reale Heapreserve, PSRAM, GPIO-Belegung oder aktive Polaritaet
  behauptet.
