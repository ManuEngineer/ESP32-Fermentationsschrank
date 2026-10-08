Status: Issue #126 / PR #127 digital gemergt; reale RTC-Varianten- und
Funktionsverifikation bleibt separat offen

# Third-Party-Komponentenregister

## Status

Dieses Register ist die laufend gepflegte Auswahl aus dem
[`Release-1-Adopt-or-build-Audit`](audits/RELEASE_1_ADOPT_OR_BUILD_AUDIT.md).
Die R1-Zeitplattform bindet den DS3231-Treiber über den Component Manager
direkt in `lib/device_platform_esp_idf` ein; die physische RTC-Abnahme steht
noch aus. Der
ausfuehrliche Quellen- und Lizenznachweis steht in
[`THIRD_PARTY_SOURCE_AND_LICENSE_REVIEW.md`](audits/THIRD_PARTY_SOURCE_AND_LICENSE_REVIEW.md),
die technische Bewertung in
[`COMPONENT_EVALUATIONS.md`](audits/COMPONENT_EVALUATIONS.md).
Der aktuelle digitale Zielnachweis ist `ESP32_TARGET_COMPATIBILITY=PASS_BUILD`;
`ESP32S3_TARGET_COMPATIBILITY=REGISTRY_DECLARED_NOT_PROJECT_BUILD` bleibt
solange bestehen, bis ein projektnaher S3-Compile-Gate ausgefuehrt wurde.

## Statuswerte

| Status | Bedeutung |
|---|---|
| `FRAMEWORK_CANDIDATE` | Bestandteil der fixierten Plattform; Adapter und Messung fehlen |
| `IMPLEMENTED_DIGITAL_PENDING_HARDWARE` | digitale Integration, Builds und gezielte Gates bestanden; reale Hardwareabnahme fehlt |
| `FIRST_EVALUATION_CANDIDATE` | verbindlich zuerst zu pruefende Richtung; noch keine Produktivauswahl |
| `FIRST_EVALUATION_DIRECTION` | zuerst zu untersuchender technischer Pfad, der mehrere konkrete Integrationsvarianten enthalten kann; keine Produktivauswahl |
| `SPIKE_REQUIRED` | technisch plausibel, aber auf Zielhardware nicht bestaetigt |
| `FINAL_SELECTION_PENDING` | endgueltige Uebernahme bleibt bis zum Nachweis und Ownerentscheid offen |
| `OWNER_SELECTED` | Owner hat die konkrete Komponente ausgewaehlt; separat genannte Implementierungs-, Composition- oder Hardwaregates bleiben bestehen |
| `CONDITIONAL_FALLBACK` | identische Evaluation nur bei einem dokumentierten Problem des ersten Kandidaten |
| `EVALUATE_LATER` | erst fuer ein spaeteres Issue oder nach einem anderen Gate relevant |
| `DEFER_AFTER_R1` | nicht Bestandteil von Release 1 |
| `LICENSE_REVIEW_REQUIRED` | Herkunft oder Abdeckung muss vor Veroeffentlichung konkret geprueft werden |
| `NOT_SELECTED` | gepruefter Kandidat, keine Auswahl getroffen |
| `EVALUATE_BEFORE_RELEASE` | zwingendes technisches beziehungsweise Security-Release-Gate; kein produktiver Release vor Evaluation, dokumentiertem Ownerentscheid und erforderlichem Nachweis |

`FIRST_EVALUATION_DIRECTION` und `EVALUATE_BEFORE_RELEASE` sind weder optionale
Aufschuebe noch Synonyme fuer `EVALUATE_LATER` oder `DEFER_AFTER_R1` und waehlen
keine konkrete Integration automatisch aus.

## Register

| Komponente | Kandidat oder Quelle | Gepruefter Stand | Lizenzstatus | Auditstatus | Vorgesehene Verwendung |
|---|---|---|---|---|---|
| ESP32-Plattform | ESP-IDF `v6.1` (`fff9895c82d744c7237be8847347bdd1b07c6643`) | aktive Produktionsbasis aus Issue #159 | Apache-2.0 mit Komponentenlizenzen; konkret verwendete Bestandteile vor einer Einbindung pruefen | `FRAMEWORK_CANDIDATE` | GPIO, SPI, WLAN, Zeit, NVS, UART hinter Adaptern; PlatformIO/Arduino ist keine aktive Produktionsbasis |
| RTC-Treiber | [`esp-idf-lib/ds3231`](https://components.espressif.com/components/esp-idf-lib/ds3231/versions/1.1.7/readme?language=en) | `1.1.7`, Component-Hash `474f5cc0e8e02ffaca0f21e3b1bbc5d4a20d0679368cd9e21de4203f1b8016f6`; `dependencies.lock` | MIT; mitgelieferte `LICENSE` muss im Distributionshinweis erhalten bleiben | `IMPLEMENTED_DIGITAL_PENDING_HARDWARE` | bestehender DS3231SN-Kalender-/I2C-Adapter mit verifiziertem API-Subset und Health-Shim; er ist keine Aussage über die reale DS3231-Variante und erhält keinen vorsorglichen DS3231M-/Multi-RTC-Ausbau |
| I2C-Transport | [`esp-idf-lib/i2cdev`](https://components.espressif.com/components/esp-idf-lib/i2cdev/versions/2.1.2/readme?language=en) | `2.1.2`, Component-Hash `ad8981cc64533dcaced5107d72e42bcebe79345e194e82795792af531b300ce3`; direkte Dependency in `lib/device_platform_esp_idf/idf_component.yml`, `dependencies.lock` | MIT; mitgelieferte `LICENSE` und Noticepflicht im Distributionshinweis erhalten | `IMPLEMENTED_DIGITAL_PENDING_HARDWARE` | einziger Port-/Device-Lifecycle-Owner für den geteilten I2C-Bus; kein zweiter ESP-IDF-Masterbus auf demselben Port |
| I2C-Hilfsfunktionen | [`esp-idf-lib/esp_idf_lib_helpers`](https://components.espressif.com/components/esp-idf-lib/esp_idf_lib_helpers/versions/1.4.0/readme?language=en) | `1.4.0`, Component-Hash `689853bb8993434f9556af0f2816e808bf77b5d22100144b21f3519993daf237`; transitive Dependency in `dependencies.lock` | ISC; mitgelieferte `LICENSE` und Noticepflicht im Distributionshinweis erhalten | `IMPLEMENTED_DIGITAL_PENDING_HARDWARE` | transitive Laufzeithilfe des adoptierten `i2cdev`-Pfads, keine direkte App- oder Plattform-API |
| Display | ESP-IDF `esp_lcd` (eingebaut) plus `espressif/esp_lcd_ili9341 2.0.2` | 2026-08-05 | Apache-2.0 (ESP-IDF/Registry) | `SPIKE_REQUIRED` | Espressif-first-Primaerkandidat fuer Stufe 1 (ESP-IDF >=4.4, durch ESP-IDF 6.0.2 erfuellt); Controller erst an realer Hardware bestaetigen |
| Touch | `espressif/esp_lcd_touch 1.2.1` | 2026-08-05 | Apache-2.0 | `SPIKE_REQUIRED` | generische Touch-Abstraktion (ESP-IDF >=4.4.2) fuer den Espressif-first-Displaypfad; kein XPT2046-Treiber selbst, siehe naechste Zeile |
| Touch | `atanisoft/esp_lcd_touch_xpt2046 1.0.6` | 2026-08-05 | MIT | `SPIKE_REQUIRED` | kein offizieller `espressif/*`-XPT2046-Treiber vorhanden; am besten belegter kompatibler Registry-Kandidat (ESP-IDF >=4.4 + `esp_lcd_touch` >=1.0.4); nur nach real bestaetigtem Touchcontroller |
| Display/Touch | LovyanGFX `1.2.26`, Commit `3f78b705` | 2026-07-27 | FreeBSD plus dokumentierte Ursprungsbestandteile | `SPIKE_REQUIRED` | ergebnisoffener Evaluationskandidat fuer Stufe 1 (Evaluationsgate: ESP-IDF-6.0.2-Build/-Betrieb oder dokumentierter Integrationsweg ohne Arduino-Produktionspfad); Controller erst an realer Hardware bestaetigen |
| Display | TFT_eSPI Manifest `2.5.44`, Commit `16e37595` | 2026-07-27 | FreeBSD plus dokumentierte Ursprungsbestandteile | `SPIKE_REQUIRED` | ergebnisoffener Evaluationskandidat fuer Stufe 1 (gleiches Evaluationsgate); Controller erst an realer Hardware bestaetigen |
| Display/Touch | LCDWiki MSP2807-Paket, lokales Archiv vom Owner | Paketdateien von 2018, geprueft 2026-07-27 | MIT-Dateien in drei Bibliotheksordnern; Paketabdeckung und Herkunft erneut pruefen | `LICENSE_REVIEW_REQUIRED` | ergebnisoffener Evaluationskandidat und interne Herstellerreferenz (gleiches Evaluationsgate); keine ungepruefte Uebernahme |
| Display | Arduino_GFX `1.6.7`, Commit `fe33cad8` | 2026-07-27 | BSD | `NOT_SELECTED` | Reservekandidat mit geeignetem Touchadapter; nur bei dokumentiertem Ausloeser nachziehen |
| Display | Adafruit GFX `1.12.6` und Adafruit ILI9341 `1.6.3` | Commits `ac6d7c38`/`dbb447af` | BSD; Abhaengigkeiten separat pruefen | `NOT_SELECTED` | Reservekandidat mit geeignetem XPT2046-Touchadapter; nur bei dokumentiertem Ausloeser nachziehen |
| Touch | XPT2046_Touchscreen `1.4`, Commit `f956c5d8` | 2026-07-27 | MIT im Quellheader | `SPIKE_REQUIRED` | ergebnisoffener Evaluationskandidat (gleiches Evaluationsgate); nur nach real bestaetigtem Touchcontroller |
| DS18B20 | Espressif `onewire_bus 1.1.2` plus `ds18b20 0.4.0` | `onewire_bus 1.1.2` Component-Hash `dcce4fb1c2fc43c3d02de1cc9abdf062127468a785578723696de3817c526f99`, `ds18b20 0.4.0` Component-Hash `38022d0c39c08b1df3e77f95a3f9544251549a1fd9b6263357bf714026f4bb6b`; `dependencies.lock` | Apache-2.0; mitgelieferte `LICENSE` (SHA-256 `cfc7749b96f63bd31c3c42b5c471bf756814053e847c10f3eb003417bc523d30`) muss im Distributionshinweis erhalten bleiben | `IMPLEMENTED_DIGITAL_PENDING_HARDWARE` | direkte Dependencies in `lib/device_platform_esp_idf/idf_component.yml`, fest gepinnt; nur hinter `Ds18b20OnewireBus` (`IDs18b20Bus`), RMT-Backend, `sensor_hub` aus; Hardware-Verifikation im Folgeissue; Stufe-1-Evidence `docs/audits/ISSUE30_S0_STAGE1_EVIDENCE.md` |
| DS18B20 | DallasTemperature `4.0.6` plus OneWire `2.3.8` | Commits `dadbbf7d`/`800f26f3` | MIT; OneWire MIT im Quelltext | `SPIKE_REQUIRED` | bedingter Evaluationskandidat: nur Papier-Check, Vertiefung nur bei konkret nachgewiesenem Misserfolg des Espressif-Pfads; jeder Arduino-Pfad bleibt ownerpflichtig (Issue #30 Plan) |
| Persistenz | ESP-IDF NVS / `nvs_flash` (eingebaut) | Bestandteil ESP-IDF 6.1; konkrete Adapter-API und Minor-Upgrade-Semantik in Issue #159 pruefen | Apache-2.0 (ESP-IDF) | `FRAMEWORK_CANDIDATE`, `SPIKE_REQUIRED` | durch ADR-016 festgelegtes produktives Backend fuer `IStateStore`; keine offene Produktauswahl mehr; Adapterimplementierung und reale Verifikation in Issue #90 |
| JSON | Ownerauswahl: Espressif `espressif/cjson 1.7.19~2`; Registry-Commit `1387cec28a9b40654be7892114bd7d26fcd3869c`, Upstream `b2890c8d76bbb64e710585ebc0a917196b9c67e7`; Component-Hash `e788323270d90738662d66fffa910bfe1fba019bba087f01557e70c40485b469` | 2026-09-30; `dependencies.lock` | MIT; bezogene `LICENSE` SHA-256 `a36dda207c36db5818729c54e7ad4e8b0c6fba847491ba64f372c1a2037b6d5c` | `OWNER_SELECTED`; digitaler Codec-/Profile-/Native-Testpfad PASS, Hardware-/integrierter Ressourcenbeleg ausstehend | private direkte Dependency und nur konkrete `web_json_codec.cpp`-Grenze; ArduinoJson nicht mehr Produktdependency; kein Route-/`main/app_main.cpp`-Composition; 4-Session/no-PSRAM-Ressourcengate vor produktiver Webmutation |
| Webserver | ESP-IDF `esp_http_server` (eingebaut) | 2026-08-05 | Apache-2.0 (ESP-IDF) | `FIRST_EVALUATION_CANDIDATE`, `SPIKE_REQUIRED`, `FINAL_SELECTION_PENDING` | Espressif-first-Primaerkandidat; Arduino-`WebServer` ist keine aktive Produktionsrichtung mehr; konkreter Adapter und Prototypnachweis fehlen, keine Auswahl in #74 |
| Webserver | ESPAsyncWebServer `3.12.0`, Commit `a008cccf` | 2026-07-27 | LGPL-3.0 | `CONDITIONAL_FALLBACK`, `EVALUATE_LATER` | ergebnisoffener konditionaler Evaluationskandidat (gleiches Evaluationsgate wie andere Rueckfallkandidaten); identischer Vergleich nur bei konkretem Problem des ersten Kandidaten und klarem Vorteil; keine vorsorgliche SSE-/WebSocket-Reserve |
| WLAN-Onboarding | `espressif/network_provisioning 1.2.4` auf Basis `protocomm` | 2026-08-05 | Apache-2.0 | `FIRST_EVALUATION_CANDIDATE`, `SPIKE_REQUIRED`, `FINAL_SELECTION_PENDING` | Espressif-first-Primaerkandidat (ESP-IDF >=5.1); Nachfolger des in der ESP-IDF-6.0-Linie entfernten `wifi_provisioning`; muss im Spike den browserbasierten R1-Vertrag ohne Pflicht-App/-Cloud/-CLI nachweisen |
| WLAN-Onboarding | `protocomm` (ESP-IDF-6.0.2-Bestandteil, keine eigene Registry-Version) | 2026-08-05 | Apache-2.0 (ESP-IDF) | `FIRST_EVALUATION_CANDIDATE`, `SPIKE_REQUIRED` | gleichwertiger zweiter Espressif-first-Pfad: direkte Provisionierung ohne `network_provisioning`; der native Eigenbau-Adapter (Pfad 3) ist keine Drittkomponente und erhaelt keine eigene Registerzeile |
| WLAN-Onboarding | WiFiManager `v2.0.17`, Tag-Commit `d82d0a1b` | 2026-07-27 | MIT; Webassets und transitive Abhaengigkeiten im Spike konkret pruefen | `SPIKE_REQUIRED`, `FINAL_SELECTION_PENDING` | ergebnisoffener zusaetzlicher konditionaler Drittanbieter-Evaluationskandidat (gleiches Evaluationsgate); ersetzt weder die Espressif-Pflichtkandidaten noch den nativen Eigenbau-Gegenkandidaten; technischer Portalteil hinter projektspezifischer Start-, Kandidaten-, Commit-, Secret-, Recovery- und Safetylogik |
| Auth-KDF | PBKDF2-HMAC-SHA-256 aus der fixierten mbedTLS-/ESP32-Toolchain | konkrete Toolchainfunktion, Version und Dateisatz im Spike pruefen | Framework-/mbedTLS-Lizenz und Notices des tatsaechlich verwendeten Pakets pruefen | `FIRST_EVALUATION_CANDIDATE`, `SPIKE_REQUIRED`, `FINAL_SELECTION_PENDING` | erster Evaluationspfad fuer getrennte gesalzene Passwort-/PIN-Verifier; keine neue Bibliothek eingebunden, Iterationszahl und Produktionswahl bleiben bis Testvektor-, Laufzeit-, Stack-, Heap-, Jitter- und Watchdognachweis offen |
| Kryptografischer Zufall | `esp_fill_random()` oder korrekt gesaeter mbedTLS-DRBG aus der fixierten Toolchain | konkrete Integration offen | Bestandteil des ESP32-/mbedTLS-Pfads; konkret verwendete Dateien und Notices pruefen | `FIRST_EVALUATION_DIRECTION`, `SPIKE_REQUIRED` | zuerst zu untersuchender Pfad fuer Salts sowie fluechtige normale/anonyme Sessionkennungen und CSRF-Tokens; konkrete Integration nicht ausgewaehlt, Fehler fuehren zur Ablehnung, keine schwachen Ersatzwerte |
| Plattformverschluesselung | ESP32-NVS-/Flashverschluesselung | nicht aktiviert oder projektbezogen getestet | Toolchain-/ESP-IDF-Bestandteile und Produktionsprozess im separaten Spike pruefen | `EVALUATE_BEFORE_RELEASE` | zwingendes separates Security-Release-Gate fuer wiederverwendbare Secrets vor #37; Ergebnis ist produktive Auswahl samt Provisionierungs-/Recovery-/Regressionstest oder begruendete Nichtauswahl mit Rest-Risiken, Schutzgrenzen und Ownerfreigabe; keine automatische Auswahl, Aktivierung oder Schutzbehauptung im Audit |
| QR-Code | QRCode `0.0.1`, Commit `eafbde49` | 2026-07-27 | MIT, abgeleitet von Project Nayuki | `NOT_SELECTED` | lokaler WLAN-QR-Code |
| QR-Code | Project Nayuki QR-Code-generator `1.8.0`, Commit `2c9044de` | 2026-07-27 | MIT im Quellheader | `NOT_SELECTED` | alternative kleine C-Implementierung |
| UI-Framework | [`lvgl/lvgl 9.6.0~1`](https://github.com/lvgl/lvgl/tree/60b614c23c816ca5edc5f2840c9945eff7da0ad4), Component-Hash `7d82410747bbfb319531449749e940f440f8414f041f9eaba13f0d1dd7f58cdd` (`dependencies.lock`) | 2026-09-25 | MIT; gebuendelte `qrcodegen.c` von Project Nayuki traegt MIT-Urheberrechts- und Lizenztext | `IMPLEMENTED_DIGITAL_PENDING_HARDWARE` | bestehender produktiver LVGL-Stack; #164 nutzt dessen `lv_qrcode`-Widget fuer den lokalen SoftAP-Beitritts-QR, keine separate QR-Abhaengigkeit |
| UI-Framework-Integration | [`espressif/esp_lvgl_port 2.9.0`](https://components.espressif.com/components/espressif/esp_lvgl_port), Component-Hash `d3c020b45c3dfc0d1706a83b63a1c34a6eb444a6e935f9afff79f92b7b94dcef` (`dependencies.lock`) | 2026-09-25 | Apache-2.0 | `IMPLEMENTED_DIGITAL_PENDING_HARDWARE` | bestehender produktiver LVGL-Port auf `esp_lcd`/`esp_lcd_touch`; QR-Slice verwendet ihn ohne zweite Display-/UI-Integration |
| Regelung | Arduino PID `1.2.1`, Commit `524a4268`; QuickPID `3.1.9`, Commit `c3f64fa2` | 2026-07-27 | MIT im Quellheader beziehungsweise LICENSE | `NOT_SELECTED` | historische Referenzkandidaten; Release-1-Regelvertrag bleibt eigene PI-/Safety-Logik |
| Update | ESP-IDF/esptool und ESP32-ROM-Bootloader | fixierte ESP-IDF-Toolchain | jeweilige Tool-/Frameworklizenzen | `FRAMEWORK_CANDIDATE` | UART-Update und physische Recovery |
| OTA/Bluetooth/Cloud | keine Komponente | nicht bewertet | nicht anwendbar | `DEFER_AFTER_R1` | keine Release-1-Einbindung |

## Pflegevorschlag

Nach Ownerfreigabe soll jede tatsaechlich eingebundene Komponente zusaetzlich
die fixierte Version, den verwendeten Paketbezug, die Lizenzdateien im
Releaseartefakt, den umsetzenden PR und den letzten Hardware-/Ressourcennachweis
erhalten. Nicht ausgewaehlte Auditkandidaten bleiben nicht dauerhaft als
Abhaengigkeiten im Projekt.
