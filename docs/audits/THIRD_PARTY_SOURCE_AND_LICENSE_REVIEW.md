# Herkunfts- und Lizenzregister

Zur Auditnavigation: [`RELEASE_1_ADOPT_OR_BUILD_AUDIT.md`](RELEASE_1_ADOPT_OR_BUILD_AUDIT.md).

## Zweck und Grenze

Stand: 2026-08-28. Dieses technische Register ist keine Rechtsberatung und
keine allgemeine Publikationsfreigabe. Es dokumentiert die beim Audit sichtbare
Quelle und Lizenzlage. Vor einer tatsaechlichen Einbindung werden die konkret
bezogene Version, alle mitgelieferten Lizenzdateien, Drittbestandteile und das
Releaseartefakt erneut geprueft.

Die meisten aufgefuehrten Bibliotheken bleiben Evaluationskandidaten. Die
Ausnahme ist der für Issue #126 digital integrierte DS3231-/I2C-Pfad; dessen
physische Hardwareabnahme steht noch aus. Technische Bewertungen stehen in
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
| `SPIKE_REQUIRED` | bevorzugter oder plausibler Kandidat; vor Auswahl sind begrenzter technischer Nachweis und erneute Publikationspruefung erforderlich |
| `PUBLICATION_REVIEW_REQUIRED` | vor direkter Uebernahme oder Publikation konkreten Dateisatz pruefen |
| `FRAMEWORK_PRESENT` | bereits Teil der fixierten Toolchain, aber Verpflichtungen fuer das Gesamtartefakt bleiben |
| `IMPLEMENTED_DIGITAL_PENDING_HARDWARE` | digitale Integration, Builds und gezielte Gates bestanden; reale Hardwareabnahme fehlt |
| `DEFERRED` | fuer Release 1 keine Komponente auswaehlen |

Technische Status wie `FIRST_EVALUATION_DIRECTION` und
`EVALUATE_BEFORE_RELEASE` werden im Komponentenregister definiert. Insbesondere
ist `EVALUATE_BEFORE_RELEASE` ein zwingendes Release-Gate mit dokumentiertem
Ownerentscheid und Nachweis, keine optionale Verschiebung und keine
automatische Produktivauswahl. Diese Quellenpruefung ersetzt den technischen
Securitynachweis nicht.

### Issue-126-Digitalnachweis

Der generierte `dependencies.lock` ist die deterministische Source of Truth
fuer die aufgeloesten Component-Versionen und Hashes. Fuer den aktuellen
digitalen Nachweis gilt:

- `esp-idf-lib/ds3231` `1.1.7`: Component-Hash
  `474f5cc0e8e02ffaca0f21e3b1bbc5d4a20d0679368cd9e21de4203f1b8016f6`;
- `esp-idf-lib/i2cdev` `2.1.2`: Component-Hash
  `ad8981cc64533dcaced5107d72e42bcebe79345e194e82795792af531b300ce3`;
- `esp-idf-lib/esp_idf_lib_helpers` `1.4.0`: Component-Hash
  `689853bb8993434f9556af0f2816e808bf77b5d22100144b21f3519993daf237`;
- ESP32-Projektbuild: `PASS_BUILD`;
- ESP32-S3: `REGISTRY_DECLARED_NOT_PROJECT_BUILD` (die Registry deklariert
  das Ziel, aber dieser Projektlauf baut nur das konfigurierte ESP32-Ziel).

Die Hashwerte werden nicht manuell in diesem Register aufgeloest oder
veraendert; jede Abweichung vom Lockfile blockiert den License-/Dependency-
Gate.

Der gepruefte Cleanup-Vertrag bleibt fuer den ESP-IDF-Adapter relevant:
`ds3231_free_desc()` delegiert an `i2c_dev_delete_mutex()`. Dieser Upstream-
Pfad kann vor der Deregistrierung mit einem Fehler wie `ESP_ERR_TIMEOUT`
zurueckkehren. Der Adapter behaelt deshalb Descriptor-Speicher und
Port-Claim bis zu einem bestaetigten `ESP_OK`; erst dann werden Descriptor und
Claim freigegeben. Der Destruktor quarantänisiert bei einem nicht mehr
retryfaehigen Fehler den Speicher und laesst den Claim bestehen, damit
`i2cdev_done()` nicht auf ungeklärte Descriptor-Ownership zugreift.

## Register

Alle Onlinequellen des ursprünglichen Audits wurden am 2026-07-27 abgerufen;
die am 2026-08-05 im Zuge
der Espressif-first-Synchronisierung (ESP-IDF-6.0.2-Produktionsbasis aus
Issue #71 / PR #79) ergaenzten Zeilen sind einzeln mit diesem spaeteren Datum
markiert. "Aktivitaet" beschreibt nur den sichtbaren Upstream-Stand, nicht die
Qualitaet oder eine Supportgarantie.

| Komponente | Projekt/Hersteller und Quelle | Gepruefter Stand | Lizenzquelle und Drittbestandteile | Interne Nutzung / Codeuebernahme / Anpassung | Pruefung vor oeffentlicher Veroeffentlichung | Status |
|---|---|---|---|---|---|---|
| Zielframework | [Espressif ESP-IDF](https://github.com/espressif/esp-idf) | `v6.0.2`, Commit `7101770dc6db2667b3c477cc31365dd1acd6db4e` (Issue #71 / PR #79, aktive Produktionsbasis) | Apache-2.0 plus dokumentierte Drittbestandteile | bestehende Buildgrundlage (nativer Hosttestpfad ueber PlatformIO, Produktionsprofile ueber ESP-IDF); spaetere Adapter verwenden nur benoetigte APIs | Lizenz-/Notice-Dateien der tatsaechlich verwendeten ESP-IDF-Komponenten und des Firmware-Distributionswegs pruefen | `FRAMEWORK_PRESENT` |
| esp-idf-lib ds3231 | [ESP Component Registry](https://components.espressif.com/components/esp-idf-lib/ds3231/versions/1.1.7/readme?language=en), [Quellrepository](https://github.com/esp-idf-lib/ds3231/tree/cbe14063d3f2bf39489e18d896c725b8111b5cc4) | `1.1.7`, Component-Hash `474f5cc0e8e02ffaca0f21e3b1bbc5d4a20d0679368cd9e21de4203f1b8016f6`; direkte Dependency von `lib/device_platform_esp_idf`, Lockfile `dependencies.lock` | Paket-`LICENSE`: MIT; transitive `i2cdev`- und `esp_idf_lib_helpers`-Lizenzen separat erfassen | unveränderte Treibernutzung hinter dem DS3231SN-Adapter plus dokumentierter schmaler Health-Shim für EOSC-/Rohregisterprüfung; kein vollständiger Eigenbau | vor Distribution `LICENSE`, Notices, exakten verwendeten Dateisatz, R1-API-Subset und DS3231SN-Hardwarebezug erneut prüfen | `IMPLEMENTED_DIGITAL_PENDING_HARDWARE` |
| esp-idf-lib i2cdev | [ESP Component Registry](https://components.espressif.com/components/esp-idf-lib/i2cdev/versions/2.1.2/readme?language=en), [Quellrepository](https://github.com/esp-idf-lib/i2cdev/tree/7113b661fa467ad266e0b97b4e649ffa23d18cc5) | `2.1.2`, Component-Hash `ad8981cc64533dcaced5107d72e42bcebe79345e194e82795792af531b300ce3`; direkte Dependency in `lib/device_platform_esp_idf/idf_component.yml`, Lockfile `dependencies.lock`; ESP32-Projektbuild `PASS_BUILD`, ESP32-S3 `REGISTRY_DECLARED_NOT_PROJECT_BUILD` | Paket-`LICENSE`: MIT | thread-sicherer Shared-I2C-Transport; `i2cdev` bleibt Port-/Device-Lifecycle-Owner | vor Distribution `LICENSE`, Notices und den tatsächlich genutzten Transportpfad prüfen | `IMPLEMENTED_DIGITAL_PENDING_HARDWARE` |
| esp-idf-lib esp_idf_lib_helpers | [ESP Component Registry](https://components.espressif.com/components/esp-idf-lib/esp_idf_lib_helpers/versions/1.4.0/readme?language=en), [Quellrepository](https://github.com/esp-idf-lib/esp_idf_lib_helpers/tree/918d82cafb1f00fd86f1ad8571271cb3e910588b) | `1.4.0`, Component-Hash `689853bb8993434f9556af0f2816e808bf77b5d22100144b21f3519993daf237`; transitive Dependency, Lockfile `dependencies.lock` | Paket-`LICENSE`: ISC | transitive Hilfsfunktionen des `i2cdev`-Pfads; keine direkte Plattform- oder App-API | vor Distribution `LICENSE`, Notices und den tatsächlich genutzten Dateisatz prüfen | `IMPLEMENTED_DIGITAL_PENDING_HARDWARE` |
| Arduino-ESP32 WebServer (historisch) | [Espressif Arduino-ESP32](https://github.com/espressif/arduino-esp32) | Fruehere Projektevaluation auf `2.0.17`, Framework-SHA `dcc1105b`; Arduino-ESP32 ist seit Issue #71 / PR #79 keine aktive Produktionsbasis mehr | LGPL-2.1 und eingebettete Drittkomponentenhinweise des damaligen Pakets galten | keine aktive Nutzung mehr; ESP-IDF `esp_http_server` ist der aktuelle Espressif-first-Primaerkandidat (siehe eigene Zeile) | keine, solange keine erneute Arduino-Einbindung erfolgt | `DEFERRED` |
| ESP-IDF esp_http_server | [Espressif ESP-IDF](https://github.com/espressif/esp-idf/tree/v6.0.2/components/esp_http_server) | Bestandteil ESP-IDF `v6.0.2`; `FIRST_EVALUATION_CANDIDATE`, `SPIKE_REQUIRED`, `FINAL_SELECTION_PENDING` | Apache-2.0 (Bestandteil der ESP-IDF-Zielframework-Zeile) | interne begrenzte Baselineevaluation fuer lokalen HTTP-Transport; noch keine Produktivuebernahme | konkret verwendete ESP-IDF-Komponentendateien sowie Lizenz-/Notice-Abdeckung des Firmwareartefakts im umsetzenden PR erfassen | `FRAMEWORK_PRESENT` |
| LovyanGFX | [lovyan03/LovyanGFX](https://github.com/lovyan03/LovyanGFX) | `1.2.26`, `3f78b705`; aktuelle Aktivitaet 2026-07 | `license.txt`: FreeBSD fuer LovyanGFX; dokumentiert Adafruit-/FreeBSD-Ursprungsteile | interne Evaluation und unveraenderte Bibliotheksnutzung moeglich; Anpassungen bevorzugt im eigenen Adapter | vollstaendige `license.txt` und Notices mitliefern; verwendete Version fixieren | `AUDIT_ONLY` |
| TFT_eSPI | [Bodmer/TFT_eSPI](https://github.com/Bodmer/TFT_eSPI) | Manifest `2.5.44`, `16e37595`; aktuelle Aktivitaet 2026-04 | `license.txt`: Adafruit-Ursprung und FreeBSD fuer Originalcode | interne Evaluation; projektspezifische Setupkonfiguration statt Fork bevorzugt | Lizenztext, Setupdiff und gegebenenfalls eingebettete Fonts/Assets einzeln pruefen | `AUDIT_ONLY` |
| Arduino_GFX | [moononournation/Arduino_GFX](https://github.com/moononournation/Arduino_GFX) | `1.6.7`, `fe33cad8`; aktiv 2026-07 | `license.txt`: BSD/Adafruit-Ursprung; weitere Treiberherkunft in konkretem Dateisatz pruefen | interne Evaluation, spaeter unveraenderte Abhaengigkeit plus Adapter | konkretes Paket und alle genutzten Treiber-/Fontdateien pruefen | `AUDIT_ONLY` |
| Adafruit GFX | [Adafruit-GFX-Library](https://github.com/adafruit/Adafruit-GFX-Library) | `1.12.6`, `ac6d7c38`; aktiv 2026-04 | `license.txt`: BSD; Abhaengigkeit Adafruit BusIO separat | interne Evaluation und Bibliotheksnutzung | GFX, BusIO, Fonts und Notices des tatsaechlichen Pakets gemeinsam pruefen | `AUDIT_ONLY` |
| Adafruit ILI9341 | [Adafruit_ILI9341](https://github.com/adafruit/Adafruit_ILI9341) | `1.6.3`, `dbb447af`; aktiv 2026-02 | kein Top-Level-Lizenzendpunkt gefunden; Quellheader nennt BSD und Erhalt des Headers; Abhaengigkeiten separat | interne Evaluation und unveraenderte Bibliotheksnutzung | Quellheader, Paketinhalt und alle aufgeloesten Adafruit-Abhaengigkeiten pruefen | `PUBLICATION_REVIEW_REQUIRED` |
| XPT2046-Touch | [PaulStoffregen/XPT2046_Touchscreen](https://github.com/PaulStoffregen/XPT2046_Touchscreen) | `1.4`, `f956c5d8`; letzte sichtbare Aktivitaet 2024-06 | kein Top-Level-Lizenzendpunkt gefunden; Quellheader enthaelt MIT-Bedingungen und Funding-Notice | interne Evaluation; direkter Fork vermeiden | alle uebernommenen Dateien auf Headerabdeckung und Notice-Erhalt pruefen | `PUBLICATION_REVIEW_REQUIRED` |
| LCDWiki MSP2807 | [LCDWiki-Produktseite](https://www.lcdwiki.com/2.8inch_SPI_Module_ILI9341_SKU:MSP2807) und lokale Ownerdatei `references/datasheets/Display/2.8inch_SPI_Module_ILI9341_MSP2807_V1.1.zip` | Paketzeitstempel 2018; vom Owner bereitgestellt, am 2026-07-27 untersucht | `LCDWIKI_GUI`, `LCDWIKI_SPI` und `LCDWIKI_TOUCH` enthalten MIT-Dateien; es fehlt ein eindeutig versioniertes Upstream-Repository und eine paketweite Herkunfts-/Abdeckungsmatrix | interne Verwendung, Untersuchung und Anpassung sind gemaess Ownerentscheidung freigegeben; direkte Uebernahme, reine Referenznutzung und eigener neu geschriebener Code werden getrennt protokolliert | vor Publikation jeder direkt uebernommenen Datei konkrete Herkunft, Lizenzabdeckung, Notice und Aenderung erneut pruefen; keine allgemeine rechtliche Freigabe behaupten | `PUBLICATION_REVIEW_REQUIRED` |
| DallasTemperature | [milesburton/Arduino-Temperature-Control-Library](https://github.com/milesburton/Arduino-Temperature-Control-Library) | `4.0.6`, `dadbbf7d`; aktiv 2026-04 | Top-Level `LICENSE`: MIT | interne Evaluation und spaeter moegliche unveraenderte Abhaengigkeit | Version, LICENSE und Abhaengigkeit OneWire fixieren | `AUDIT_ONLY` |
| OneWire | [PaulStoffregen/OneWire](https://github.com/PaulStoffregen/OneWire) | `2.3.8`, `800f26f3`; aktiv 2025-06 | kein Top-Level-Lizenzendpunkt gefunden; `OneWire.cpp` enthaelt MIT-Bedingungen und mehrere Urheber-/Beitragsangaben | interne Evaluation | alle verwendeten Header-/Quelldateien und Notices auf vollstaendige Lizenzabdeckung pruefen | `PUBLICATION_REVIEW_REQUIRED` |
| Espressif onewire_bus | [Component Registry](https://components.espressif.com/components/espressif/onewire_bus/versions/1.1.1/readme?language=en), [Quellrepository](https://github.com/espressif/idf-extra-components/tree/master/onewire_bus) | `1.1.1`, Registry-Commit `a269e1fe`; aktiv 2026 | Registry: Apache-2.0; Abhaengigkeit ESP-IDF >=5.0 (durch ESP-IDF 6.0.2 erfuellt) | Espressif-first-Primaerkandidat fuer #30; interne API-Evaluation | Registryarchiv, Apache-Lizenz und konkrete ESP-IDF-Abhaengigkeiten pruefen | `SPIKE_REQUIRED` |
| Espressif ds18b20 | [Component Registry](https://components.espressif.com/components/espressif/ds18b20/versions/0.4.0/readme?language=en), [Quellrepository](https://github.com/espressif/esp-bsp/tree/master/components/ds18b20) | `0.4.0`, Registry-Commit `bf92b0b3`; aktiv 2026 | Registry: Apache-2.0; `onewire_bus`, optional `sensor_hub` | Espressif-first-Primaerkandidat fuer #30; interne API-Evaluation | konkrete Abhaengigkeiten und deren Lizenzen/Versionen aufloesen | `SPIKE_REQUIRED` |
| Espressif esp_lcd_ili9341 | [Component Registry](https://components.espressif.com/components/espressif/esp_lcd_ili9341) | `2.0.2`; aktiv 2026 (vor 7 Monaten) | Registry: Apache-2.0; Abhaengigkeit ESP-IDF >=4.4 (durch ESP-IDF 6.0.2 erfuellt), `cmake_utilities` | Espressif-first-Primaerkandidat fuer #31; interne API-Evaluation | Registryarchiv, Apache-Lizenz und konkrete ESP-IDF-Abhaengigkeiten pruefen | `SPIKE_REQUIRED` |
| Espressif esp_lcd_touch | [Component Registry](https://components.espressif.com/components/espressif/esp_lcd_touch) | `1.2.1`; aktiv 2026 (vor 7 Monaten) | Registry: Apache-2.0; Abhaengigkeit ESP-IDF >=4.4.2 (durch ESP-IDF 6.0.2 erfuellt) | Espressif-first-Primaerkandidat fuer #31 (generische Touch-Abstraktion, kein XPT2046-Treiber selbst); interne API-Evaluation | Registryarchiv, Apache-Lizenz und konkrete ESP-IDF-Abhaengigkeiten pruefen | `SPIKE_REQUIRED` |
| atanisoft esp_lcd_touch_xpt2046 | [Component Registry](https://components.espressif.com/components/atanisoft/esp_lcd_touch_xpt2046) | `1.0.6`; letzte sichtbare Aktivitaet vor 1 Jahr | Registry: MIT; Abhaengigkeit ESP-IDF >=4.4 und `espressif/esp_lcd_touch` >=1.0.4 | kein offizieller `espressif/*`-XPT2046-Treiber vorhanden; am besten belegter kompatibler Registry-Kandidat fuer #31; interne Evaluation | Registryarchiv, MIT-Lizenz und Abhaengigkeiten pruefen | `SPIKE_REQUIRED` |
| Espressif esp_lvgl_port | [Component Registry](https://components.espressif.com/components/espressif/esp_lvgl_port) | `2.8.0~1`; aktiv 2026 (vor 2 Monaten) | Registry: Apache-2.0; Abhaengigkeit ESP-IDF >=5.2, `lvgl/lvgl >=8, <10` | Espressif-first-Integrationsport fuer LVGL auf `esp_lcd`/`esp_lcd_touch`; interne Ressourcen-/API-Evaluation, keine Auswahl | Registryarchiv, Apache-Lizenz und LVGL-Versionsabhaengigkeit pruefen | `AUDIT_ONLY` |
| Espressif network_provisioning | [Component Registry](https://components.espressif.com/components/espressif/network_provisioning), [Quellrepository](https://github.com/espressif/idf-extra-components/tree/master/network_provisioning) | `1.2.4`; aktiv 2026 (vor 3 Monaten) | Registry: Apache-2.0; Abhaengigkeit ESP-IDF >=5.1, `espressif/cjson`; Nachfolger des in ESP-IDF 6.0 entfernten `wifi_provisioning` | Espressif-first-Primaerkandidat fuer das neue WLAN-Onboarding-Issue; interne API-Evaluation | Registryarchiv, Apache-Lizenz und konkrete ESP-IDF-Abhaengigkeiten pruefen | `SPIKE_REQUIRED` |
| ESP-IDF protocomm | [Espressif ESP-IDF](https://github.com/espressif/esp-idf) | Bestandteil ESP-IDF `v6.0.2`; keine eigene Component-Registry-Version | Apache-2.0 (Bestandteil der ESP-IDF-Zielframework-Zeile) | gleichwertiger zweiter Espressif-first-Pfad (direkte Provisionierung ohne `network_provisioning`) fuer das WLAN-Onboarding-Issue #89; interne API-Evaluation | konkret verwendete ESP-IDF-Komponentendateien und Notices pruefen | `FRAMEWORK_PRESENT` |
| ArduinoJson | [bblanchon/ArduinoJson](https://github.com/bblanchon/ArduinoJson) | Tag `v7.4.3`, Tag-Commit `77771d3c07668e01d8f52acb03910c1110bb373f`; `FIRST_EVALUATION_CANDIDATE`, `SPIKE_REQUIRED`, `FINAL_SELECTION_PENDING` | `LICENSE.txt`: MIT; Paketmanifest und allfaellige enthaltene Drittbestandteile im bezogenen Paket pruefen | interne Evaluation und spaeter begrenzter Codecspike an externen API-/Export-/Backup-/Importgrenzen; keine Einbindung oder Bibliotheksfork im Audit | vor Distribution exakte Paketquelle, LICENSE, verwendete Header/Features, deaktivierte Optionen, Notices, Abhaengigkeiten und konkret publizierten Umfang dokumentieren | `SPIKE_REQUIRED` |
| ESPAsyncWebServer | [ESP32Async/ESPAsyncWebServer](https://github.com/ESP32Async/ESPAsyncWebServer) | `3.12.0`, `a008cccf`; aktiv 2026-07; `CONDITIONAL_FALLBACK`, `EVALUATE_LATER` | Top-Level `LICENSE`: LGPL-3.0; asynchrone TCP- und optionale JSON-Abhaengigkeiten separat | ergebnisoffener konditionaler Evaluationskandidat (gleiches Evaluationsgate wie andere Rueckfallkandidaten); nur bei nachgewiesenem Problem des ersten `esp_http_server`-Prototyps; nicht ausgewaehlt | vor einem identischen Vergleich und jeder Einbindung dynamische/kompilierte Verlinkung, Source-Angebot/Notices und alle Abhaengigkeiten fuer den konkreten Distributionsweg pruefen | `PUBLICATION_REVIEW_REQUIRED` |
| WiFiManager | [tzapu/WiFiManager](https://github.com/tzapu/WiFiManager) | Tag `v2.0.17`, `d82d0a1b`; Upstream-HEAD `4131fe61`, aktiv 2026-02; ergebnisoffener zusaetzlicher konditionaler Evaluationskandidat, `SPIKE_REQUIRED`, `FINAL_SELECTION_PENDING` | Top-Level `LICENSE`: MIT; eingebettete Webassets und transitive Abhaengigkeiten im konkreten Paket aufloesen | ergebnisoffener zusaetzlicher Drittanbieter-Evaluationskandidat (gleiches Evaluationsgate wie andere Rueckfallkandidaten) neben `network_provisioning`, `protocomm` und dem nativen Eigenbau-Adapter; begrenzter, von #27 getrennter R1-Onboardingspike auf dem fixierten Tag; noch keine endgueltige Uebernahme, kein direkter Bibliotheksfork geplant | vor Distribution exakte Version, LICENSE, verwendete/veraenderte Dateien, Assets, Notices, Abhaengigkeiten und publizierten Umfang nachweisen; Anpassungen getrennt dokumentieren | `SPIKE_REQUIRED` |
| mbedTLS-/ESP-IDF-Authentisierungspfad | [Espressif ESP-IDF](https://github.com/espressif/esp-idf) und darin verwendete mbedTLS-Komponenten | fixierte ESP-IDF-Toolchain `v6.0.2`; konkrete PBKDF2-HMAC-SHA-256-, Zufalls- und Vergleichsfunktionen noch im Spike zu bestimmen | Apache-2.0 (ESP-IDF) sowie Lizenzen/Notices der konkret verwendeten eingebetteten mbedTLS-Dateien pruefen | nur interne Evaluation fuer KDF-Testvektoren, kryptografischen Zufall und konstanten Vergleich; keine neue Bibliothek, keine endgueltige Produktionswahl und kein festgelegter Work Factor | konkret verwendete Funktionen/Dateien, Versionen, Buildkonfiguration, Lizenz-/Noticepflichten und publizierten Umfang vor einer Uebernahme dokumentieren | `SPIKE_REQUIRED` |
| ESP32-NVS-/Flashverschluesselung | [Espressif ESP-IDF](https://github.com/espressif/esp-idf) als Toolchain-/Herstellerreferenz | nicht aktiviert oder projektbezogen getestet; `EVALUATE_BEFORE_RELEASE` vor #37 | Apache-2.0 plus eingebettete Drittbestandteile; konkreter Toolchainpfad und Produktionsprozess offen | separater interner Security-Spike zu Boot, Partitionierung, Provisionierung, Schluesselverwaltung, Recovery, UART-Neuflash und Update/Migration; keine Aktivierung oder Schutzbehauptung im Audit | verwendete Komponenten, Produktions-/Flashprozess, Schluesselmodell, Notices und dokumentierte physische Schutzgrenze vor der ergebnisoffenen Ownerentscheidung festhalten; Auswahl oder begruendete Nichtauswahl muessen vor #37 nachgewiesen sein | `AUDIT_ONLY` |
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
