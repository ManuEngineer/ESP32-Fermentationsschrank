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
| `FIRST_EVALUATION_CANDIDATE` | verbindlich zuerst zu pruefende Richtung; noch keine Produktivauswahl |
| `FIRST_EVALUATION_DIRECTION` | zuerst zu untersuchender technischer Pfad, der mehrere konkrete Integrationsvarianten enthalten kann; keine Produktivauswahl |
| `SPIKE_REQUIRED` | technisch plausibel, aber auf Zielhardware nicht bestaetigt |
| `FINAL_SELECTION_PENDING` | endgueltige Uebernahme bleibt bis zum Nachweis und Ownerentscheid offen |
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
| ESP32-Plattform | ESP-IDF `v6.0.2` (`7101770dc6db2667b3c477cc31365dd1acd6db4e`) | aktive Produktionsbasis aus Issue #71 / PR #79 | Apache-2.0 mit Komponentenlizenzen; konkret verwendete Bestandteile vor einer Einbindung pruefen | `FRAMEWORK_CANDIDATE` | GPIO, SPI, WLAN, Zeit, NVS, UART hinter Adaptern; PlatformIO/Arduino ist keine aktive Produktionsbasis |
| Display/Touch | LovyanGFX `1.2.26`, Commit `3f78b705` | 2026-07-27 | FreeBSD plus dokumentierte Ursprungsbestandteile | `SPIKE_REQUIRED` | Hauptkandidat fuer Stufe 1; Controller erst an realer Hardware bestaetigen |
| Display | TFT_eSPI Manifest `2.5.44`, Commit `16e37595` | 2026-07-27 | FreeBSD plus dokumentierte Ursprungsbestandteile | `SPIKE_REQUIRED` | Hauptkandidat fuer Stufe 1; Controller erst an realer Hardware bestaetigen |
| Display/Touch | LCDWiki MSP2807-Paket, lokales Archiv vom Owner | Paketdateien von 2018, geprueft 2026-07-27 | MIT-Dateien in drei Bibliotheksordnern; Paketabdeckung und Herkunft erneut pruefen | `LICENSE_REVIEW_REQUIRED` | Hauptkandidat fuer Stufe 1 und interne Herstellerreferenz; keine ungepruefte Uebernahme |
| Display | Arduino_GFX `1.6.7`, Commit `fe33cad8` | 2026-07-27 | BSD | `NOT_SELECTED` | Reservekandidat mit geeignetem Touchadapter; nur bei dokumentiertem Ausloeser nachziehen |
| Display | Adafruit GFX `1.12.6` und Adafruit ILI9341 `1.6.3` | Commits `ac6d7c38`/`dbb447af` | BSD; Abhaengigkeiten separat pruefen | `NOT_SELECTED` | Reservekandidat mit geeignetem XPT2046-Touchadapter; nur bei dokumentiertem Ausloeser nachziehen |
| Touch | XPT2046_Touchscreen `1.4`, Commit `f956c5d8` | 2026-07-27 | MIT im Quellheader | `SPIKE_REQUIRED` | nur nach real bestaetigtem Touchcontroller |
| DS18B20 | DallasTemperature `4.0.6` plus OneWire `2.3.8` | Commits `dadbbf7d`/`800f26f3` | MIT; OneWire MIT im Quelltext | `SPIKE_REQUIRED` | Softwarekandidat 1 fuer Stufen 1–3; Topologie A und B identisch pruefen, Topologiewahl getrennt |
| DS18B20 | Espressif `onewire_bus 1.1.1` plus `ds18b20 0.4.0` | Commits `a269e1fe`/`bf92b0b3` | Apache-2.0 | `SPIKE_REQUIRED` | Softwarekandidat 2 fuer Stufen 1–3; nur ohne Toolchainwechsel, Topologie A und B identisch pruefen |
| Persistenz | unter ESP-IDF konkret zu evaluierender Persistenzdienst | konkrete API unter ESP-IDF noch offen | ESP-IDF-Komponentenlizenzen und tatsaechlich verwendete APIs im Spike pruefen | `FIRST_EVALUATION_CANDIDATE`, `SPIKE_REQUIRED`, `FINAL_SELECTION_PENDING` | produktives `IStateStore`-Backend gemaess ADR-016; keine Auswahl in #74 |
| JSON | ArduinoJson `7.4.3`, Tag-Commit `77771d3c` | 2026-07-27 | MIT; Paketmanifest, konkret verwendete Header/Features und Notices im Spike pruefen | `FIRST_EVALUATION_CANDIDATE`, `SPIKE_REQUIRED`, `FINAL_SELECTION_PENDING` | bevorzugter, noch nicht endgueltig ausgewaehlter R1-Kandidat fuer begrenzte externe API-, Konfigurations-, Diagnose-, Export-, secret-freie Backup- und Importvertraege; keine Bibliothekstypen im Fachkern und keine interne Kontrollpunktpersistenz |
| Webserver | unter ESP-IDF zu evaluierender lokaler HTTP-Dienst | Arduino-`WebServer` ist keine aktive Laufzeitbasis mehr | ESP-IDF-Komponentenlizenzen und Drittbestandteile im Spike pruefen | `FIRST_EVALUATION_CANDIDATE`, `SPIKE_REQUIRED`, `FINAL_SELECTION_PENDING` | begrenzter lokaler HTTP-Dienst; konkreter Adapter und Prototypnachweis fehlen, keine Auswahl in #74 |
| Webserver | ESPAsyncWebServer `3.12.0`, Commit `a008cccf` | 2026-07-27 | LGPL-3.0 | `CONDITIONAL_FALLBACK`, `EVALUATE_LATER` | identischer Vergleich nur bei konkretem Problem des ersten Kandidaten und klarem Vorteil; keine vorsorgliche SSE-/WebSocket-Reserve |
| WLAN-Onboarding | WiFiManager `v2.0.17`, Tag-Commit `d82d0a1b` | 2026-07-27 | MIT; Webassets und transitive Abhaengigkeiten im Spike konkret pruefen | `FIRST_EVALUATION_CANDIDATE`, `SPIKE_REQUIRED`, `FINAL_SELECTION_PENDING` | bevorzugter R1-Kandidat fuer den zuerst begrenzten, von #27 getrennten Onboardingspike; technischer Portalteil hinter projektspezifischer Start-, Kandidaten-, Commit-, Secret-, Recovery- und Safetylogik; Frameworkadapter ist konditionaler Rueckfall und keine Drittkomponente |
| Auth-KDF | PBKDF2-HMAC-SHA-256 aus der fixierten mbedTLS-/ESP32-Toolchain | konkrete Toolchainfunktion, Version und Dateisatz im Spike pruefen | Framework-/mbedTLS-Lizenz und Notices des tatsaechlich verwendeten Pakets pruefen | `FIRST_EVALUATION_CANDIDATE`, `SPIKE_REQUIRED`, `FINAL_SELECTION_PENDING` | erster Evaluationspfad fuer getrennte gesalzene Passwort-/PIN-Verifier; keine neue Bibliothek eingebunden, Iterationszahl und Produktionswahl bleiben bis Testvektor-, Laufzeit-, Stack-, Heap-, Jitter- und Watchdognachweis offen |
| Kryptografischer Zufall | `esp_fill_random()` oder korrekt gesaeter mbedTLS-DRBG aus der fixierten Toolchain | konkrete Integration offen | Bestandteil des ESP32-/mbedTLS-Pfads; konkret verwendete Dateien und Notices pruefen | `FIRST_EVALUATION_DIRECTION`, `SPIKE_REQUIRED` | zuerst zu untersuchender Pfad fuer Salts sowie fluechtige normale/anonyme Sessionkennungen und CSRF-Tokens; konkrete Integration nicht ausgewaehlt, Fehler fuehren zur Ablehnung, keine schwachen Ersatzwerte |
| Plattformverschluesselung | ESP32-NVS-/Flashverschluesselung | nicht aktiviert oder projektbezogen getestet | Toolchain-/ESP-IDF-Bestandteile und Produktionsprozess im separaten Spike pruefen | `EVALUATE_BEFORE_RELEASE` | zwingendes separates Security-Release-Gate fuer wiederverwendbare Secrets vor #37; Ergebnis ist produktive Auswahl samt Provisionierungs-/Recovery-/Regressionstest oder begruendete Nichtauswahl mit Rest-Risiken, Schutzgrenzen und Ownerfreigabe; keine automatische Auswahl, Aktivierung oder Schutzbehauptung im Audit |
| QR-Code | QRCode `0.0.1`, Commit `eafbde49` | 2026-07-27 | MIT, abgeleitet von Project Nayuki | `NOT_SELECTED` | lokaler WLAN-QR-Code |
| QR-Code | Project Nayuki QR-Code-generator `1.8.0`, Commit `2c9044de` | 2026-07-27 | MIT im Quellheader | `NOT_SELECTED` | alternative kleine C-Implementierung |
| UI-Framework | LVGL `9.5.0`, Commit `8fd90bb1` | 2026-07-27 | MIT | `EVALUATE_LATER` | kein Treiberkandidat; erst nach Treiberwahl und Adaptervertrag auf identischem repraesentativem Screen gegen schlanke Views messen |
| Regelung | Arduino PID `1.2.1`, Commit `524a4268`; QuickPID `3.1.9`, Commit `c3f64fa2` | 2026-07-27 | MIT im Quellheader beziehungsweise LICENSE | `NOT_SELECTED` | historische Referenzkandidaten; Release-1-Regelvertrag bleibt eigene PI-/Safety-Logik |
| Update | ESP-IDF/esptool und ESP32-ROM-Bootloader | fixierte ESP-IDF-Toolchain | jeweilige Tool-/Frameworklizenzen | `FRAMEWORK_CANDIDATE` | UART-Update und physische Recovery |
| OTA/Bluetooth/Cloud | keine Komponente | nicht bewertet | nicht anwendbar | `DEFER_AFTER_R1` | keine Release-1-Einbindung |

## Pflegevorschlag

Nach Ownerfreigabe soll jede tatsaechlich eingebundene Komponente zusaetzlich
die fixierte Version, den verwendeten Paketbezug, die Lizenzdateien im
Releaseartefakt, den umsetzenden PR und den letzten Hardware-/Ressourcennachweis
erhalten. Nicht ausgewaehlte Auditkandidaten bleiben nicht dauerhaft als
Abhaengigkeiten im Projekt.
