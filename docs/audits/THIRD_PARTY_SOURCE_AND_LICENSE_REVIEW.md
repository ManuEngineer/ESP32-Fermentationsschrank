# Herkunfts- und Lizenzregister

Zur Auditnavigation: [`RELEASE_1_ADOPT_OR_BUILD_AUDIT.md`](RELEASE_1_ADOPT_OR_BUILD_AUDIT.md).

## Zweck und Grenze

Stand: 2026-07-27. Dieses technische Register ist keine Rechtsberatung und
keine allgemeine Publikationsfreigabe. Es dokumentiert die beim Audit sichtbare
Quelle und Lizenzlage. Vor einer tatsaechlichen Einbindung werden die konkret
bezogene Version, alle mitgelieferten Lizenzdateien, Drittbestandteile und das
Releaseartefakt erneut geprueft.

Keine der aufgefuehrten Bibliotheken wurde installiert oder in das Projekt
eingebunden. Technische Bewertungen stehen in
[`COMPONENT_EVALUATIONS.md`](COMPONENT_EVALUATIONS.md); der dauerhafte Entwurf
fuer das Register steht in
[`../THIRD_PARTY_COMPONENTS.md`](../THIRD_PARTY_COMPONENTS.md).

Falls der Owner spaeter einen Kandidaten auswaehlt, ist als normaler
Veroeffentlichungsweg ein oeffentliches Quellrepository und ein daraus gebautes
Firmwarebinary anzunehmen. Unveraenderte Paketnutzung, eigene Adapter,
projektspezifische Konfiguration und direkt uebernommene oder veraenderte
Fremddateien werden dabei getrennt dokumentiert. Direkte Anpassungen sollen
moeglichst als nachvollziehbarer Patch oder eigener Adapter erfolgen.

## Freigabestatus

| Status | Bedeutung |
|---|---|
| `AUDIT_ONLY` | Quelle und Lizenz zur Evaluation erfasst; keine Einbindung |
| `PUBLICATION_REVIEW_REQUIRED` | vor direkter Uebernahme oder Publikation konkreten Dateisatz pruefen |
| `FRAMEWORK_PRESENT` | bereits Teil der fixierten Toolchain, aber Verpflichtungen fuer das Gesamtartefakt bleiben |
| `DEFERRED` | fuer Release 1 keine Komponente auswaehlen |

## Register

Alle Onlinequellen wurden am 2026-07-27 abgerufen. "Aktivitaet" beschreibt nur
den sichtbaren Upstream-Stand, nicht die Qualitaet oder eine Supportgarantie.

| Komponente | Projekt/Hersteller und Quelle | Gepruefter Stand | Lizenzquelle und Drittbestandteile | Interne Nutzung / Codeuebernahme / Anpassung | Pruefung vor oeffentlicher Veroeffentlichung | Status |
|---|---|---|---|---|---|---|
| Zielframework | [Espressif Arduino-ESP32](https://github.com/espressif/arduino-esp32) ueber PlatformIO | Projekt verwendet `2.0.17`, Framework-SHA `dcc1105b`; Upstream-HEAD `75989288` | Repository meldet LGPL-2.1; eingebettete ESP-IDF- und weitere Komponenten besitzen eigene Hinweise | bestehende Buildgrundlage; spaetere Adapter verwenden nur benoetigte APIs | Lizenz-/Notice-Dateien des tatsaechlichen PlatformIO-Pakets und des Firmware-Distributionswegs pruefen | `FRAMEWORK_PRESENT` |
| ESP-IDF-Referenz | [Espressif ESP-IDF](https://github.com/espressif/esp-idf) | Upstream-HEAD `10212290`, Release `v6.0.2` | Apache-2.0 plus dokumentierte Drittbestandteile | Herstellerreferenz; kein Toolchainwechsel im Audit | nur bei spaeterer direkter Komponentenuebernahme konkreten Teilbaum pruefen | `AUDIT_ONLY` |
| LovyanGFX | [lovyan03/LovyanGFX](https://github.com/lovyan03/LovyanGFX) | `1.2.26`, `3f78b705`; aktuelle Aktivitaet 2026-07 | `license.txt`: FreeBSD fuer LovyanGFX; dokumentiert Adafruit-/FreeBSD-Ursprungsteile | interne Evaluation und unveraenderte Bibliotheksnutzung moeglich; Anpassungen bevorzugt im eigenen Adapter | vollstaendige `license.txt` und Notices mitliefern; verwendete Version fixieren | `AUDIT_ONLY` |
| TFT_eSPI | [Bodmer/TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) | Manifest `2.5.44`, `16e37595`; aktuelle Aktivitaet 2026-04 | `license.txt`: Adafruit-Ursprung und FreeBSD fuer Originalcode | interne Evaluation; projektspezifische Setupkonfiguration statt Fork bevorzugt | Lizenztext, Setupdiff und gegebenenfalls eingebettete Fonts/Assets einzeln pruefen | `AUDIT_ONLY` |
| Arduino_GFX | [moononournation/Arduino_GFX](https://github.com/moononournation/Arduino_GFX) | `1.6.7`, `fe33cad8`; aktiv 2026-07 | `license.txt`: BSD/Adafruit-Ursprung; weitere Treiberherkunft in konkretem Dateisatz pruefen | interne Evaluation, spaeter unveraenderte Abhaengigkeit plus Adapter | konkretes Paket und alle genutzten Treiber-/Fontdateien pruefen | `AUDIT_ONLY` |
| Adafruit GFX | [Adafruit-GFX-Library](https://github.com/adafruit/Adafruit-GFX-Library) | `1.12.6`, `ac6d7c38`; aktiv 2026-04 | `license.txt`: BSD; Abhaengigkeit Adafruit BusIO separat | interne Evaluation und Bibliotheksnutzung | GFX, BusIO, Fonts und Notices des tatsaechlichen Pakets gemeinsam pruefen | `AUDIT_ONLY` |
| Adafruit ILI9341 | [Adafruit_ILI9341](https://github.com/adafruit/Adafruit_ILI9341) | `1.6.3`, `dbb447af`; aktiv 2026-02 | kein Top-Level-Lizenzendpunkt gefunden; Quellheader nennt BSD und Erhalt des Headers; Abhaengigkeiten separat | interne Evaluation und unveraenderte Bibliotheksnutzung | Quellheader, Paketinhalt und alle aufgeloesten Adafruit-Abhaengigkeiten pruefen | `PUBLICATION_REVIEW_REQUIRED` |
| XPT2046-Touch | [PaulStoffregen/XPT2046_Touchscreen](https://github.com/PaulStoffregen/XPT2046_Touchscreen) | `1.4`, `f956c5d8`; letzte sichtbare Aktivitaet 2024-06 | kein Top-Level-Lizenzendpunkt gefunden; Quellheader enthaelt MIT-Bedingungen und Funding-Notice | interne Evaluation; direkter Fork vermeiden | alle uebernommenen Dateien auf Headerabdeckung und Notice-Erhalt pruefen | `PUBLICATION_REVIEW_REQUIRED` |
| LCDWiki MSP2807 | [LCDWiki-Produktseite](https://www.lcdwiki.com/2.8inch_SPI_Module_ILI9341_SKU:MSP2807) und lokale Ownerdatei `references/datasheets/Display/2.8inch_SPI_Module_ILI9341_MSP2807_V1.1.zip` | Paketzeitstempel 2018; vom Owner bereitgestellt, am 2026-07-27 untersucht | `LCDWIKI_GUI`, `LCDWIKI_SPI` und `LCDWIKI_TOUCH` enthalten MIT-Dateien; es fehlt ein eindeutig versioniertes Upstream-Repository und eine paketweite Herkunfts-/Abdeckungsmatrix | interne Verwendung, Untersuchung und Anpassung sind gemaess Ownerentscheidung freigegeben; direkte Uebernahme, reine Referenznutzung und eigener neu geschriebener Code werden getrennt protokolliert | vor Publikation jeder direkt uebernommenen Datei konkrete Herkunft, Lizenzabdeckung, Notice und Aenderung erneut pruefen; keine allgemeine rechtliche Freigabe behaupten | `PUBLICATION_REVIEW_REQUIRED` |
| DallasTemperature | [milesburton/Arduino-Temperature-Control-Library](https://github.com/milesburton/Arduino-Temperature-Control-Library) | `4.0.6`, `dadbbf7d`; aktiv 2026-04 | Top-Level `LICENSE`: MIT | interne Evaluation und spaeter moegliche unveraenderte Abhaengigkeit | Version, LICENSE und Abhaengigkeit OneWire fixieren | `AUDIT_ONLY` |
| OneWire | [PaulStoffregen/OneWire](https://github.com/PaulStoffregen/OneWire) | `2.3.8`, `800f26f3`; aktiv 2025-06 | kein Top-Level-Lizenzendpunkt gefunden; `OneWire.cpp` enthaelt MIT-Bedingungen und mehrere Urheber-/Beitragsangaben | interne Evaluation | alle verwendeten Header-/Quelldateien und Notices auf vollstaendige Lizenzabdeckung pruefen | `PUBLICATION_REVIEW_REQUIRED` |
| Espressif onewire_bus | [Component Registry](https://components.espressif.com/components/espressif/onewire_bus/versions/1.1.1/readme?language=en), [Quellrepository](https://github.com/espressif/idf-extra-components/tree/master/onewire_bus) | `1.1.1`, Registry-Commit `a269e1fe`; aktiv 2026 | Registry: Apache-2.0; Abhaengigkeit ESP-IDF >=5.0 | interne Toolchain- und API-Evaluation | Registryarchiv, Apache-Lizenz und konkrete ESP-IDF-Abhaengigkeiten pruefen | `AUDIT_ONLY` |
| Espressif ds18b20 | [Component Registry](https://components.espressif.com/components/espressif/ds18b20/versions/0.4.0/readme?language=en), [Quellrepository](https://github.com/espressif/esp-bsp/tree/master/components/ds18b20) | `0.4.0`, Registry-Commit `bf92b0b3`; aktiv 2026 | Registry: Apache-2.0; `onewire_bus`, optional `sensor_hub` | interne Toolchain- und API-Evaluation | konkrete Abhaengigkeiten und deren Lizenzen/Versionen aufloesen | `AUDIT_ONLY` |
| ArduinoJson | [bblanchon/ArduinoJson](https://github.com/bblanchon/ArduinoJson) | Release `7.4.3`, `7823e4a6`; aktiv 2026-07 | `LICENSE.txt`: MIT | interne Evaluation; spaetere begrenzte Codecverwendung | Paketversion, LICENSE und deaktivierte/benoetigte Features dokumentieren | `AUDIT_ONLY` |
| ESPAsyncWebServer | [ESP32Async/ESPAsyncWebServer](https://github.com/ESP32Async/ESPAsyncWebServer) | `3.12.0`, `a008cccf`; aktiv 2026-07 | Top-Level `LICENSE`: LGPL-3.0; asynchrone TCP- und optionale JSON-Abhaengigkeiten separat | nur konditionale Rueckfallevaluation bei nachgewiesenem Problem der Arduino-ESP32-`WebServer`-Baseline; nicht ausgewaehlt | vor einem identischen Vergleich und jeder Einbindung dynamische/kompilierte Verlinkung, Source-Angebot/Notices und alle Abhaengigkeiten fuer den konkreten Distributionsweg pruefen | `PUBLICATION_REVIEW_REQUIRED` |
| WiFiManager | [tzapu/WiFiManager](https://github.com/tzapu/WiFiManager) | `2.0.17`, `4131fe61`; aktiv 2026-02 | Top-Level `LICENSE`: MIT | interne Portal-Evaluation | Version, LICENSE, eingebettete Webassets und Anpassungen pruefen | `AUDIT_ONLY` |
| ricmoo QRCode | [ricmoo/QRCode](https://github.com/ricmoo/QRCode) | `0.0.1`, `eafbde49`; letzter Commit des Default-HEAD 2020 | `LICENSE.txt`: MIT; nennt wesentliche Ableitung von Project Nayuki | interne Evaluation; keine Auswahl | Copyright beider Projekte und Lizenztext erhalten | `AUDIT_ONLY` |
| Nayuki QR-Code-generator | [nayuki/QR-Code-generator](https://github.com/nayuki/QR-Code-generator) | `1.8.0`, `2c9044de`; aktiv 2025-01 | kein Top-Level-Lizenzendpunkt gefunden; C-Quellheader enthaelt MIT-Bedingungen | interne Evaluation einer begrenzten C-Variante | exakten uebernommenen Teilbaum und Headerabdeckung pruefen | `PUBLICATION_REVIEW_REQUIRED` |
| LVGL | [lvgl/lvgl](https://github.com/lvgl/lvgl) | `9.5.0`, `8fd90bb1`; aktiv 2026-07 | `LICENCE.txt`: MIT; optionale Fonts, Demos und Drittkomponenten separat | interne Ressourcen-/UI-Evaluation, keine Auswahl | Konfiguration, verwendete Fonts/Assets, Notices und Abhaengigkeiten pruefen | `AUDIT_ONLY` |
| Arduino PID | [br3ttb/Arduino-PID-Library](https://github.com/br3ttb/Arduino-PID-Library) | Manifest `1.2.1`, `524a4268`; aktiv 2024-05 | kein Top-Level-Lizenzendpunkt gefunden; `PID_v1.cpp` nennt MIT | nur Referenzvergleich; keine geplante Uebernahme | bei Codeuebernahme jeden Quellheader und Ursprung pruefen | `PUBLICATION_REVIEW_REQUIRED` |
| QuickPID | [Dlloydev/QuickPID](https://github.com/Dlloydev/QuickPID) | `3.1.9`, `c3f64fa2`; letzte Aktivitaet 2023-06 | Top-Level `LICENSE`: MIT | nur Referenzvergleich; keine geplante Uebernahme | LICENSE und konkreten Dateisatz pruefen | `AUDIT_ONLY` |

## Hersteller- und Lieferantenquellen

Die lokalen Datenblaetter und Lieferantenunterlagen stehen in
[`references/LINKS.md`](../../references/LINKS.md). Herstellerdatenblaetter
dienen der technischen Vertragspruefung; sie sind keine Softwarelizenz.
Lieferantenangaben besitzen gemaess `docs/HARDWARE.md` nur den Status
`confirmed_order` und belegen weder Verdrahtung noch aktive Pegel.

| Quelle | Nutzung | Publikationsgrenze |
|---|---|---|
| Analog Devices DS18B20-Datenblatt | Protokoll-, Timing- und Grenzreferenz | nicht in Produktcode kopieren; lokale Archivkopie bleibt Quellenbeleg |
| Infineon BTS7960-Datenblatt | Bauteilreferenz; Lieferantenmodul separat messen | Modulschaltung nicht aus dem IC-Datenblatt erfinden |
| ESP32-32E-/MOSFET-Lieferanten-PDF | bestellte Produktbeschreibung | `confirmed_order`, keine gemessene Kanal-/Pegelbestaetigung |
| LCDWiki-Handbuecher und Paket | interne Untersuchung und Hardware-Referenz | direkte Dateien nur nach konkreter Herkunfts-/Publikationspruefung |

## Noch erforderliche Pruefung je spaeterem Komponenten-PR

1. Paket ausschliesslich aus der dokumentierten offiziellen Quelle beziehen.
2. Version/Commit und SHA des bezogenen Archivs fixieren.
3. Lizenz- und Notice-Dateien des tatsaechlichen Pakets erfassen.
4. transitive Abhaengigkeiten und eingebettete Fonts, Bilder, Demos oder
   Fremdquellen pruefen.
5. direkte Anpassungen als Patch oder eigener Adapter nachvollziehbar halten.
6. Distributionspflichten fuer Quellrepository, Firmwarebinary und
   Begleitdokumentation bewerten.
7. erst danach den Status im dauerhaften Komponentenregister auf eine
   ownerfreigegebene Auswahl setzen.
