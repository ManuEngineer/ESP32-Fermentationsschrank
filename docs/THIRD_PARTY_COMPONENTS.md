Status: DRAFT – Ownerfreigabe ausstehend

# Vorgeschlagenes Third-Party-Komponentenregister

## Status

Dieses Register ist ein Entwurf aus dem
[`Release-1-Adopt-or-build-Audit`](audits/RELEASE_1_ADOPT_OR_BUILD_AUDIT.md).
Es bindet keine Abhaengigkeit ein und trifft keine endgueltige Auswahl. Der
ausfuehrliche Quellen- und Lizenznachweis steht in
[`THIRD_PARTY_SOURCE_AND_LICENSE_REVIEW.md`](audits/THIRD_PARTY_SOURCE_AND_LICENSE_REVIEW.md),
die technische Bewertung in
[`COMPONENT_EVALUATIONS.md`](audits/COMPONENT_EVALUATIONS.md).

## Statuswerte

| Status | Bedeutung |
|---|---|
| `FRAMEWORK_CANDIDATE` | Bestandteil der fixierten Plattform; Adapter und Messung fehlen |
| `SPIKE_REQUIRED` | technisch plausibel, aber auf Zielhardware nicht bestaetigt |
| `EVALUATE_LATER` | erst fuer ein spaeteres Issue oder nach einem anderen Gate relevant |
| `DEFER_AFTER_R1` | nicht Bestandteil von Release 1 |
| `LICENSE_REVIEW_REQUIRED` | Herkunft oder Abdeckung muss vor Veroeffentlichung konkret geprueft werden |
| `NOT_SELECTED` | gepruefter Kandidat, keine Auswahl getroffen |

## Register

| Komponente | Kandidat oder Quelle | Gepruefter Stand | Lizenzstatus | Auditstatus | Vorgesehene Verwendung |
|---|---|---|---|---|---|
| ESP32-/Arduino-Plattform | PlatformIO `espressif32@7.0.1`, Arduino-ESP32 `2.0.17` (`dcc1105b`) | Repository-Basis 2026-07-27 | Arduino-ESP32 LGPL-2.1 mit Drittbestandteilen | `FRAMEWORK_CANDIDATE` | GPIO, SPI, WLAN, Zeit, NVS, UART hinter Adaptern |
| Display/Touch | LovyanGFX `1.2.26`, Commit `3f78b705` | 2026-07-27 | FreeBSD plus dokumentierte Ursprungsbestandteile | `SPIKE_REQUIRED` | ILI9341 und moeglicher XPT2046 hinter Display-/Touchadapter |
| Display | TFT_eSPI Manifest `2.5.44`, Commit `16e37595` | 2026-07-27 | FreeBSD plus dokumentierte Ursprungsbestandteile | `SPIKE_REQUIRED` | ILI9341-Darstellung |
| Display/Touch | LCDWiki MSP2807-Paket, lokales Archiv vom Owner | Paketdateien von 2018, geprueft 2026-07-27 | MIT-Dateien in drei Bibliotheksordnern; Paketabdeckung und Herkunft erneut pruefen | `LICENSE_REVIEW_REQUIRED` | Herstellernahe Referenz und Spike, keine ungepruefte Uebernahme |
| Display | Arduino_GFX `1.6.7`, Commit `fe33cad8` | 2026-07-27 | BSD | `NOT_SELECTED` | ILI9341; separate Touchbibliothek erforderlich |
| Display | Adafruit GFX `1.12.6` und Adafruit ILI9341 `1.6.3` | Commits `ac6d7c38`/`dbb447af` | BSD; Abhaengigkeiten separat pruefen | `NOT_SELECTED` | portable Referenzkombination |
| Touch | XPT2046_Touchscreen `1.4`, Commit `f956c5d8` | 2026-07-27 | MIT im Quellheader | `SPIKE_REQUIRED` | nur nach real bestaetigtem Touchcontroller |
| DS18B20 | DallasTemperature `4.0.6` plus OneWire `2.3.8` | Commits `dadbbf7d`/`800f26f3` | MIT; OneWire MIT im Quelltext | `SPIKE_REQUIRED` | Arduino-kompatibler Sensoradapter |
| DS18B20 | Espressif `onewire_bus 1.1.1` plus `ds18b20 0.4.0` | Commits `a269e1fe`/`bf92b0b3` | Apache-2.0 | `SPIKE_REQUIRED` | offizieller ESP-IDF-Kandidat; Integration in aktuelle Arduino-Toolchain offen |
| Persistenz | Arduino-ESP32 Preferences/NVS | Arduino-ESP32 `2.0.17` | LGPL-2.1/ESP-IDF-Komponentenlizenzen | `FRAMEWORK_CANDIDATE` | produktives `IStateStore`-Backend gemaess ADR-016 |
| JSON | ArduinoJson `7.4.3`, Commit `7823e4a6` | 2026-07-27 | MIT | `EVALUATE_LATER` | begrenzte API-, Export- und Importserialisierung |
| Webserver | Arduino-ESP32 `WebServer` | Arduino-ESP32 `2.0.17` | Frameworklizenz und Drittbestandteile | `FRAMEWORK_CANDIDATE` | kleinster Kandidat fuer lokalen HTTP-Dienst |
| Webserver | ESPAsyncWebServer `3.12.0`, Commit `a008cccf` | 2026-07-27 | LGPL-3.0 | `EVALUATE_LATER` | Alternative fuer SSE/WebSocket; Ressourcen und Lizenzpflichten messen |
| WLAN-Onboarding | WiFiManager `2.0.17`, Commit `4131fe61` | 2026-07-27 | MIT | `EVALUATE_LATER` | Captive-Portal-Kandidat hinter projektspezifischer Secret- und Zustandslogik |
| QR-Code | QRCode `0.0.1`, Commit `eafbde49` | 2026-07-27 | MIT, abgeleitet von Project Nayuki | `NOT_SELECTED` | lokaler WLAN-QR-Code |
| QR-Code | Project Nayuki QR-Code-generator `1.8.0`, Commit `2c9044de` | 2026-07-27 | MIT im Quellheader | `NOT_SELECTED` | alternative kleine C-Implementierung |
| UI-Framework | LVGL `9.5.0`, Commit `8fd90bb1` | 2026-07-27 | MIT | `EVALUATE_LATER` | nur falls Spike einen klaren Vorteil gegen schlanke eigene Screens zeigt |
| Regelung | Arduino PID `1.2.1`, Commit `524a4268`; QuickPID `3.1.9`, Commit `c3f64fa2` | 2026-07-27 | MIT im Quellheader beziehungsweise LICENSE | `NOT_SELECTED` | Referenzkandidaten; Release-1-Regelvertrag bleibt eigene PI-/Safety-Logik |
| Update | PlatformIO/esptool und ESP32-ROM-Bootloader | fixierte Toolchain | jeweilige Tool-/Frameworklizenzen | `FRAMEWORK_CANDIDATE` | UART-Update und physische Recovery |
| OTA/Bluetooth/Cloud | keine Komponente | nicht bewertet | nicht anwendbar | `DEFER_AFTER_R1` | keine Release-1-Einbindung |

## Pflegevorschlag

Nach Ownerfreigabe soll jede tatsaechlich eingebundene Komponente zusaetzlich
die fixierte Version, den verwendeten Paketbezug, die Lizenzdateien im
Releaseartefakt, den umsetzenden PR und den letzten Hardware-/Ressourcennachweis
erhalten. Nicht ausgewaehlte Auditkandidaten bleiben nicht dauerhaft als
Abhaengigkeiten im Projekt.
