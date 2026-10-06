# Komponentenevaluationen fuer Release 1

Zur Auditnavigation: [`RELEASE_1_ADOPT_OR_BUILD_AUDIT.md`](RELEASE_1_ADOPT_OR_BUILD_AUDIT.md).

## Einordnung

Stand: 2026-07-27 (Original-Audit). Repository-Basis:
`7713a66cbf51eb078bd0f5e43c1163d1e0f47e1f`.

Synchronisiert am: 2026-08-05 gemaess der Espressif-first-Regel
(`docs/ENGINEERING_PRINCIPLES.md`) nach dem Wechsel der Produktionstoolchain auf
ESP-IDF `6.0.2` (Issue #71 / PR #79, Basis-Commit
`7101770dc6db2667b3c477cc31365dd1acd6db4e`).

Dieses Dokument bewertet Kandidaten, bindet aber keine Bibliothek ein und trifft
keine endgueltige Auswahl. "Unterstuetzt" bezeichnet eine Aussage der
offiziellen Projektquelle; reale Kompatibilitaet mit dem bestellten Board ist
bis zum Spike unbestaetigt. Die Zieltoolchain fuer Produktionsprofile ist
ESP-IDF `6.0.2` (`esp32_bringup`/`esp32_release`, Composition Root
`main/app_main.cpp`); PlatformIO ist ausschliesslich der native Hosttestpfad.
Ein Arduino-Produktionspfad besteht nicht (`AGENTS.md`). Zielhardware bleibt
ESP32-32E, 4 MB Flash und ohne PSRAM. Herkunft und Lizenzen stehen im
[`Third-Party-Review`](THIRD_PARTY_SOURCE_AND_LICENSE_REVIEW.md), die Spikes im
[`Hardware-Spike-Plan`](HARDWARE_SPIKE_PLAN.md).

Nachweisarten:

- **Hersteller:** Datenblatt oder offizieller Herstellerdienst;
- **Projekt:** Repository, Paketmanifest oder Projektdokumentation;
- **Repository:** bereits implementierter und getesteter Projektstand;
- **Messung:** erst nach einem definierten Hardware-Spike; derzeit offen.

### Issue #27 ArduinoJson-Kandidatensupplement (2026-09-30)

Dieses Supplement dokumentiert ausschliesslich den softwareseitigen Spike; es
ersetzt weder das Originalaudit noch die ausstehende Ownerauswahl.

- Kandidat: ArduinoJson `7.4.3`, offizieller Tag-Commit
  `77771d3c07668e01d8f52acb03910c1110bb373f`; bezogene `LICENSE.txt` ist MIT
  (SHA-256 `4a7ee9c96b28cbf30c5bf7c2d211a0ef57179f0328e68ad7b7fa7d754b7da1a2`).
  Der ESP-IDF Component Manager lockt denselben Commit; das native PlatformIO-
  Manifest pinnt denselben Git-SHA.
- Konkrete Grenzen: Run-Mutation-Body 480 Byte (innerhalb des bestehenden
  512-Byte-Replay-Fingerprints), maximale JSON-Nestingtiefe 4, Program-ID
  48 Byte, Temperaturprojektion 3 Eintraege, Alerts 16 Eintraege und jede
  Read-only-Antwort maximal 3072 Byte. DTO-Objekte lehnen fehlende
  Pflichtfelder, falsche Typen, zusaetzliche Felder, NUL-in-Strings/-Keys sowie
  malformed/truncated JSON fail-closed ab. Doppelte Membernamen werden aber
  waehrend `deserializeJson()` kollabiert und vom DTO-Validator nicht mehr
  erkannt; deshalb erfuellt dieser Kandidatenpfad den strikten Duplicate-Key-
  Vertrag nicht. Der Maximaltest verwendet eine reale Program-Startmutation
  mit allen Overrides und maximalen Revisionswerten.
- Das interne Body-Schema hat Version 1: Root `v`/`r`/`i` (Version,
  erwartete Revisionen, Intent); Revisionsschluessel `s`/`r`/`m`/`f`/`e`/
  `u`/`c` (State, Run, Meldungen, Fault, Recovery-Episode,
  User-Konfiguration, Programmkatalog). Intent `t` ist einer von
  `start-program`, `start-manual-holding`, `start-manual-timed`, `stop-run`,
  `complete-run`, `adjust-run`, `recovery-time-correction`, `ack-message`,
  `mute-message`, `reset-fault` oder `sensor-selection`. Payloadfelder sind
  im Codec streng allowlisted; Programmstart nutzt Kandidat `c` mit `p` (ID),
  `x` (Ziel), `d` (Dauer), `h` (Vorheizen), `s` (Sensor), `c` (Completion),
  `k` (Kuehlziel), `l` (Haltezeit). Read-only-Ausgaben whitelisten Status-/
  Revisionsfelder, je Temperatur Rolle/Qualitaet/Gueltigkeit/Wert und je Alert
  ID/Code/Schwere/Aktiv-/Ack-/Decision-/Mute-Zustand; interne Sensorrohdaten,
  Kalibrierkoeffizienten und Laufzeitstempel werden nicht ausgegeben.
- Verifikation vor dem Duplicate-Key-Finding: Native-Codec-/API-/Handler-
  Regressionen und beide ESP-IDF-6.1-Profile bestanden. `esp32_bringup`
  erzeugte 1,554,240 Byte Firmwarebinary (ELF text/data/bss
  1,359,348/210,835/90,082 Byte); `esp32_release` 1,542,224 Byte
  (1,349,292/208,867/90,082 Byte). Codec und Handler sind nicht in
  `main/app_main.cpp` komponiert; ihre Symbole sind nicht im Firmware-ELF,
  daher ist daraus kein produktiver Flash-/RAM-Delta-Nachweis abzuleiten.
- Duplicate-Key-Reproduktion auf dem Slice-4B-HEAD
  `a236effc625cc992fbd16e4e6cc72e36d2829940`: vier neue Regressionen verlangen
  Ablehnung bei Root-, Revisions-, Intent- und Candidate-Duplikaten;
  alle vier wurden unerwartet akzeptiert. Der Native-Route-Lauf endete deshalb
  `ERRORED` (4 fehlgeschlagene Assertions, 15 andere Tests bestanden).
- Oeffentliche ArduinoJson-v7-API-Pruefung: dokumentierte
  [`deserializeJson()`-Optionen](https://arduinojson.org/v7/api/json/deserializejson/)
  umfassen Filter und NestingLimit, aber keinen Duplicate-Key-Hook oder
  Token-Visitor; die dokumentierten
  [`DeserializationError`-Werte](https://arduinojson.org/v7/api/misc/deserializationerror/)
  enthalten keinen Duplicate-Key-Fehler. `JsonObjectConst` stellt Iteration,
  Lookup und Groesse der fertig geparsten Objektprojektion bereit, keine
  Eingabeereignisse. Der oeffentliche Custom-Reader liefert nur Bytes; ein
  Duplicate-Tracker davor muesste selbst JSON-Strings und Objektgrenzen lexen
  und waere hier ein verbotener zweiter Parser.
- Ergebnis: `ARDUINOJSON_7_4_3=FAIL_CANDIDATE_FOR_STRICT_DUPLICATE_FIELD_CONTRACT`;
  `FINAL_SELECTION_PENDING=YES`. Keine alternative JSON-Bibliothek und kein
  eigener Parser wurden ausgewaehlt oder implementiert. Der fruehere
  `PASS_CANDIDATE`-Status ist durch dieses Finding ueberholt. Die offene
  Heap-/Vier-Session/no-PSRAM-Messung bleibt zusaetzlich ausstehend.

### Issue #27 Espressif-cJSON-Kandidatensupplement (2026-09-30)

Dieses Supplement dokumentiert den ergebnisoffenen zweiten JSON-Kandidatenspike
nach dem belegten ArduinoJson-Fehler. Es ist keine Produktbibliotheksauswahl
und aendert weder Codec noch produktive Dependencies oder Composition.

- Exakter Kandidat: Espressif Component Registry
  [`espressif/cjson 1.7.19~2`](https://components.espressif.com/components/espressif/cjson/versions/1.7.19~2/readme),
  Quellrepository `espressif/idf-extra-components/cjson`, Commit
  `1387cec28a9b40654be7892114bd7d26fcd3869c`; das SBOM identifiziert das
  Upstream-Submodul [DaveGamble/cJSON 1.7.19](https://github.com/DaveGamble/cJSON/commit/b2890c8d76bbb64e710585ebc0a917196b9c67e7)
  bei Commit `b2890c8d76bbb64e710585ebc0a917196b9c67e7`. Der isolierte Spike pinnt
  `=1.7.19~2`; das IDF-6.1-Lockfile enthaelt Component-Hash
  `e788323270d90738662d66fffa910bfe1fba019bba087f01557e70c40485b469`.
  Die Registry-Lizenz ist
  [MIT](https://components.espressif.com/components/espressif/cjson/versions/1.7.19~2/license);
  die bezogene Lizenzdatei hat SHA-256
  `a36dda207c36db5818729c54e7ad4e8b0c6fba847491ba64f372c1a2037b6d5c`.
- Kompatibilitaet: Der direkte ESP-IDF-6.1-Build fuer `esp32` kompiliert die
  C-Komponente und den C++17-Verbraucher. Der gleiche Probequelltext ist nativ
  mit C99-cJSON plus C++17-Consumer gebaut und ausgefuehrt. Das belegt den
  Hostpfad und den ESP32-Targetbuild, nicht ein exaktes WROOM-32E-Modul oder
  Laufzeit-/Hardwareverhalten.
- Die Probe verwendet die konkreten Slice-4B-Grenzen: Mutation-Body 480 Byte,
  Tiefe 4, Program-ID 48 Byte, hoechstens drei Temperaturprojektionen,
  sechzehn Alerts und je Read-only-Antwort 3072 Byte. Der maximale konkrete
  Mutationsbody ist 337 Byte; ein 480-Byte-Body wird akzeptiert und 481 Byte
  vor dem Parser abgewiesen. Maximale Status-/Temperatur-/Alertantworten
  serialisieren begrenzt in 346/247/3048 Byte und bleiben unter 3072 Byte.
- Die oeffentliche cJSON-Objektstruktur (`child`/`next`/`string`) behaelt
  doppelte Member; ein begrenzter Baumdurchlauf erkennt Duplikate in Root,
  Revision, Intent und verschachteltem Programmkandidaten. Allowlist-,
  Pflichtfeld- und Root-Typpruefungen koennen unbekannte, fehlende und falsch
  typisierte Felder ablehnen. Truncated/malformed JSON wird abgelehnt; die
  konfigurierte Nestingtiefe 4 wird erzwungen und Tiefe 5 abgewiesen.
- Drei wesentliche Vertragsfehler bleiben reproduziert: `uint64_t`-Maximum
  und `2^64` werden auf denselben `double`-Wert gerundet; escaped und raw NUL
  werden akzeptiert und als C-String abgeschnitten; ungueltiges UTF-8 wird
  akzeptiert. `1e999` wird als Infinity geparst und erfordert zusaetzliche
  `isfinite`-Validierung; `uint32_t`-Overflow ist durch Wertebereichspruefung
  erkennbar. Der Native-Probeprozess endet deshalb erwartungsgemaess mit
  Exit-Code 1 fuer die Vertragsfehler.
- Ergebnis: `ESPRESSIF_CJSON_1_7_19_2=FAIL_CANDIDATE` und
  `FINAL_SELECTION_PENDING=YES`. Keine dritte Bibliothek, kein eigener
  JSON-Lexer, Fork oder Workaround wurde ausgewaehlt/implementiert. Die
  ArduinoJson-Duplicate-Regressions bleiben unveraendert. Produktive Routes,
  `main/app_main.cpp`-Composition, Hardware und Flash bleiben unberuehrt.

### Issue #27 cJSON Candidate Completion gegen proportionalen JSON-R1-Vertrag (2026-09-30)

Dieser neue Probe-Nachweis korrigiert nicht die historische Messung oben,
sondern ergänzt den im Planreview verlangten kleinen Gate für den aktuellen
ASCII-only R1-Bodyvertrag.

- Basis-HEAD: `c792c4ac1791ddbeb96864ee32fcff249e3389ae`; isolierter
  Probe-Commit: `b5b26c204f00e63c4e3944feacde4dccdcb4ae06`.
- Revalidierte Planrevision: `aa695b43f5b68edefea23669678d877f5b830c17`;
  PR #170 bleibt Draft, finale Bibliothekswahl bleibt Ownerentscheidung.
- `containsForbiddenNulInput()` läuft erst nach der 480-Byte-Grenze und
  prüft rohe NUL-Bytes sowie die sechs Literalbytes: Backslash, `u`, vier
  Nullen. Das ist ein begrenzter Byte-Scan ohne JSON-Tokenizing,
  String-/Escape-State-Machine, Strukturwissen, private cJSON-Interna oder
  Fork.
- Native C99-cJSON/C++17-Probe: Exit-Code 0. `RAW_NUL_REJECTED_BY_BOUNDED_GATE`
  und `DECODED_NUL_ESCAPE_REJECTED_BY_BOUNDED_GATE` sind PASS. Ein separater
  Parse desselben escaped-NUL-Vektors bestätigt weiterhin die historische
  cJSON-Dekodierung/C-String-Kürzung; verworfen wird er vor dem Parser.
- Die aktuelle maximale Mutation mit Dezimalstring-`uint64`-Revisionen ist
  341/480 Byte. Gültiges `start-program`, Enumwerte `product` und
  `cool-and-hold-until-manual-stop` bleiben parsebar. Die 48-Byte-Program-ID
  besteht danach den tatsächlichen Projektvalidator
  `validateLowercaseIdentifier()`. Maximalantworten bleiben Status 346,
  Temperaturen 247 und Alerts 3048 von 3072 Byte.
- ESP-IDF 6.1 / ESP32 Build des isolierten cJSON-Probes und des realen
  Projekt-ID-Validators: PASS. Es wurde weder geflasht noch auf Hardware
  ausgeführt; daraus folgt kein integrierter Heap-/no-PSRAM-Ressourcennachweis.
- Duplicate-Member-Rejection und generische UTF-8-Ablehnung bleiben
  Hardening, nicht MUST. Der historische numerische `uint64`-Kollisionsbefund
  bleibt wahr, wird aber durch die gemeinsame Dezimalstring-Wiredarstellung
  aus dem R1-JSON-number-Pfad entfernt.
- Ergebnis gegen den proportionalen aktuellen Vertrag:
  `CJSON_REASSESSMENT=PASS_CANDIDATE_FOR_R1_MUST`. ArduinoJson bleibt
  `PASS_CANDIDATE_FOR_R1_MUST_WITH_SMALL_CODEC_DELTA`; dies ist keine finale
  Ownerauswahl. Es gibt keinen gemessenen Ressourcen-Sieger. Die offizielle
  Espressif-Herkunft ist ein legitimer ESP-IDF-Wartungs-/Wiederverwendungs-
  Tie-Breaker; vorhandener ungemergter Codeccode ist nur ein kleiner
  Migrationskostenfaktor. Produktcodec, Dependency, Composition und Hardware
  bleiben unverändert.

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

Vier gleichrangig zu vergleichende Kandidatengruppen durchlaufen Stufe 1 fuer
Quellen, konkret benoetigte Dateien, Lizenzen, Abhaengigkeiten und
reproduzierbaren Build: der Espressif-Stack (`esp_lcd` als Built-in plus
`espressif/esp_lcd_ili9341`, `espressif/esp_lcd_touch` und
`atanisoft/esp_lcd_touch_xpt2046`) als Espressif-first-Erstkandidat sowie
LovyanGFX, TFT_eSPI und LCDWiki als ergebnisoffene Evaluationskandidaten unter
demselben Espressif-first-Evaluationsgate (direkter ESP-IDF-6.0.2-Build/
-Betrieb oder dokumentierter Integrationspfad ohne Wiedereinfuehrung eines
Arduino-Produktionspfads, `AGENTS.md`). Espressif-first bestimmt nur die
Pruefreihenfolge, nicht die Auswahl. Nur ausreichend erfolgreiche Kandidaten
erreichen den kurzen identischen Hardware-Smoke-Test in Stufe 2; nur
`PASS_SMOKE_TEST` fuehrt zur vollstaendigen Matrix in Stufe 3. Stufe 4 benennt
genau einen bevorzugten Treiberstack und einen Rueckfallkandidaten. Arduino_GFX
und Adafruit GFX/ILI9341 bleiben Reservekombinationen und werden nur bei einem
dokumentierten Ausloeser nachgezogen und nicht vorsorglich voll implementiert.
Details und Quellen der Espressif-Kandidaten stehen im
[Third-Party-Components-Register](../THIRD_PARTY_COMPONENTS.md).

| Kandidat | Hersteller-/Referenzbezug | Gepruefter Stand und Lizenz | ESP-IDF-6.0.2-/ESP32-Kompatibilitaet | Erwartete Ressourcenwirkung | Notwendiger Adapter | Risiken und Hardwaretest | Vorlaeufige Empfehlung |
|---|---|---|---|---|---|---|---|
| Espressif-Stack: `esp_lcd` + `esp_lcd_ili9341` + `esp_lcd_touch` + `atanisoft/esp_lcd_touch_xpt2046` | offizielle Espressif-Komponenten (Touch-XPT2046-Treiber ohne eigenen `espressif/*`-Namespace) | `esp_lcd` Built-in; `esp_lcd_ili9341 2.0.2`, Apache-2.0; `esp_lcd_touch 1.2.1`, Apache-2.0; `esp_lcd_touch_xpt2046 1.0.6`, MIT | ESP-IDF >=4.4 (`esp_lcd_ili9341`), >=4.4.2 (`esp_lcd_touch`), >=4.4 + `esp_lcd_touch` >=1.0.4 (`esp_lcd_touch_xpt2046`); durch ESP-IDF 6.0.2 erfuellt | schmaler Panel-/IO-Treiber; Fonts/Rendering bleibt Projektsache | schmaler Display-/Touchadapter auf `esp_lcd`-Handles | Boardprofil, gemeinsamer SPI-Bus, Heap und Reset real pruefen; kein offizieller `espressif/*`-XPT2046-Treiber | Espressif-first-Erstkandidat fuer Stufe 1; `SPIKE_REQUIRED`, nicht ausgewaehlt |
| LovyanGFX | Projekt nennt ILI9341, ESP32 und Touchunterstuetzung | `1.2.26`, `3f78b705`; FreeBSD plus dokumentierte Ursprungslizenzen | `architectures=esp32`; Arduino-Manifest setzt eine Arduino-Laufzeit voraus, die kein aktueller Produktionspfad ist (`AGENTS.md`); Eignung nur ueber das Espressif-first-Evaluationsgate messen | Treiber, Fonts und optionale Sprites; kein Vollbildpuffer erzwingen | schmaler Display-/Touchadapter, feste Bus- und Pufferkonfiguration | Boardprofil, XPT2046, Shared-SPI, Heap und Reset real pruefen | ergebnisoffener Evaluationskandidat fuer Stufe 1; nicht ausgewaehlt |
| TFT_eSPI | Projekt nennt ILI9341 und ESP32 | Manifest `2.5.44`, `16e37595`; FreeBSD plus Ursprungsbestandteile | `architectures=*`; Arduino-Manifest, kein aktueller Produktionspfad (`AGENTS.md`); User-Setup, Reproduzierbarkeit und dasselbe Espressif-first-Evaluationsgate gemeinsam pruefen | optimierter Treiber und Fonts; Konfiguration kann ungenutzte Treiber einbeziehen | Displayadapter plus projektspezifische, versionierte Setupdatei; Touch separat oder integriert pruefen | globale Konfiguration, Shared-SPI, Touchkalibrierung und Upstream-Updates | ergebnisoffener Evaluationskandidat fuer Stufe 1; nicht ausgewaehlt |
| LCDWiki-Paket | lokale Kopie der zum MSP2807 gelieferten Demos und Treiber | Paketdateien 2018; MIT-Dateien in `LCDWIKI_GUI`, `LCDWIKI_SPI`, `LCDWIKI_TOUCH`; Paketherkunft/Abdeckung erneut pruefen | Demos zielen vorwiegend auf Arduino UNO/Mega; ESP-IDF-6.0.2-/ESP32-Tauglichkeit unbestaetigt | unbekannt; altes Paket mit mehreren Demos, Fonts und Controllerpfaden | bei positiver Untersuchung nur kleinster klar lizenzierter Teil hinter Adapter; keine direkte Gesamtuebernahme | fehlende moderne ESP32-Referenz, Paketalter, Abdeckung aller Dateien, Pins und Touch real pruefen | ergebnisoffene Hersteller-/Referenzoption fuer Stufe 1, `LICENSE_REVIEW_REQUIRED`; keine allgemeine rechtliche oder technische Freigabe |
| Arduino_GFX plus geeigneter Touchadapter | Arduino_GFX nennt ILI9341 und ESP32-SPI; separater Touchadapter nach bestaetigtem Controller | `1.6.7`, `fe33cad8`, BSD; XPT2046-Touch `1.4`, `f956c5d8`, MIT im Header, falls der Controller bestaetigt wird | Arduino-Manifeste `architectures=*`; kein aktueller Produktionspfad (`AGENTS.md`); Espressif-first-Evaluationsgate pruefen | zwei Bibliotheken, Adapterschicht und moeglicherweise weniger integrierte Shared-SPI-Koordination | getrennte Display- und Touchadapter | zwei Lebenszyklen, Kalibrierung, Busarbitrierung, Reset | Reservekandidat; nur bei dokumentiertem Ausloeser nachziehen |
| Adafruit GFX + ILI9341 + geeigneter XPT2046-Touchadapter | Adafruit-Treiber dokumentiert ILI9341/ESP32; Touchadapter erst nach Controllerbestaetigung | GFX `1.12.6`/`ac6d7c38`, ILI9341 `1.6.3`/`dbb447af`, BSD; XPT2046-Touch MIT | `architectures=*`; kein aktueller Produktionspfad (`AGENTS.md`); zusaetzliche Adafruit-Abhaengigkeiten und Espressif-first-Evaluationsgate pruefen | mehrere Bibliotheken und BusIO; Referenz eher Portabilitaet als minimales ESP32-Profil | Display- und Touchadapter, ungenutzte Abhaengigkeiten vermeiden | Abhaengigkeitsumfang, Shared-SPI und Performance messen | Reservekandidat; nur bei dokumentiertem Ausloeser nachziehen |

Reservekandidaten werden nur nachgezogen, wenn weniger als zwei der vier
gleichrangigen Kandidatengruppen Stufe 2 bestehen, alle diese Kandidaten ein
wesentliches Ressourcen-, Wartungs-, Stabilitaets- oder Integrationsproblem
besitzen, der erforderliche publizierte Dateisatz eine ungeklaerte
Lizenz-/Herkunftsfrage behaelt, keine belastbare Auswahl moeglich ist oder ein
Reservekandidat einen nachgewiesenen wesentlichen R1-Vorteil besitzt.

Verbleibende eigene Logik: Touchkalibrierung, Ereignisentprellung,
Aufweckschutz, UI-Navigation, Sicherheits- und Kommandosemantik. Kein Treiber
darf eine Aktorfreigabe ausloesen. Entscheidungsstatus: `SPIKE_REQUIRED`.

Der PIN-unabhaengige Raw-Touch-Boot-Recoverypfad ist ein Auswahlgate. Jeder
verbleibende Kandidat muss im ersten Zehn-Sekunden-Boot-/`SAFE_BOOT`-Fenster
vor der normalen UI und ohne brauchbare gespeicherte Kalibrierung erkennen,
False Trigger vermeiden, einen vorzeitig freigegebenen Kontakt sicher
abbrechen und alle Leistungsaktoren aus halten. Ein fehlender Touchcontroller
fuehrt zum UART-Rueckfall; Web, Session oder Service-PIN duerfen den Modus nicht
ausloesen. Genaue Geste und Schwellen sind `TBD_HARDWARE`. Der normale
Werksreset behaelt die Kalibrierung gemaess ADR-010; ein gesonderter
Kalibrierungs-Recoveryfall bleibt getrennt.

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

| Kandidat | Hersteller-/Referenzbezug | Gepruefter Stand und Lizenz | ESP-IDF-6.0.2-/ESP32-Kompatibilitaet | Erwartete Ressourcenwirkung | Notwendiger Adapter | Risiken und Hardwaretest | Vorlaeufige Empfehlung |
|---|---|---|---|---|---|---|---|
| DallasTemperature + OneWire | verbreitete Arduino-Abstraktion ueber den DS18B20- und 1-Wire-Vertrag | DallasTemperature `4.0.6`, `dadbbf7d`, MIT; OneWire `2.3.8`, `800f26f3`, MIT im Quelltext | beide `architectures=*`; Arduino-Manifest setzt eine Arduino-Laufzeit voraus, die kein aktueller Produktionspfad ist (`AGENTS.md`); Eignung nur ueber das Espressif-first-Evaluationsgate (direkter ESP-IDF-6.0.2-Build/-Betrieb oder dokumentierter Integrationspfad ohne Arduino-Produktionspfad) messen | zwei kleine Bibliotheken; Flash-/RAM-/Heapwirkung in Stufe 1 und 3 messen | technischer Adapter mit Bus-ID, ROM, Mess-/Zeit-/CRC-/Anwesenheits-/Timeout-/Fehlerstatus | neue DallasTemperature-Hauptversion, Espressif-first-Evaluationsgate, Mehrbus/Mehrsensor, Trennung/Wiederkehr und Timing pruefen | ergebnisoffener konditionaler Evaluationskandidat neben dem Espressif-first-Erstkandidaten `onewire_bus`/`ds18b20`; gleicher Funktions-, Ressourcen- und Hardwarevergleich, keine Auswahl vor Stufe 3; ein Arduino-basierter Integrationsweg benoetigt weiterhin den dokumentierten Ownerentscheid |
| Espressif onewire_bus + ds18b20 | offizielle Espressif-Komponenten, RMT/UART-Backend, Enumeration und CRC8 | `onewire_bus 1.1.1`, `a269e1fe`; `ds18b20 0.4.0`, `bf92b0b3`; Apache-2.0 | Registry fordert fuer onewire_bus ESP-IDF >=5.0; die Produktionstoolchain ist seit Issue #71/PR #79 ESP-IDF `6.0.2`, die Mindestanforderung ist damit erfuellt | RMT/UART-Ressourcen und optionale Sensor-Hub-Abhaengigkeit; messen | derselbe technische Plattformport; keine IDF-Typen in der Anwendung | ESP-Component-Manager-Integration in `main/app_main.cpp`, Mehrbus/Mehrsensor und optionale Sensor-Hub-Grenze real pruefen | Espressif-first-Erstkandidat fuer Stufe 1; `SPIKE_REQUIRED`, keine ausgewaehlte Abhaengigkeit |

Der Produktfuehler erhaelt verbindlich einen eigenen Bus. Ein eigener Bus auch
fuer den Schutzsensor ist bevorzugt; der gemeinsame feste Bus aus Topologie B
bleibt Rueckfall, falls die reale Pinpruefung keinen dritten unproblematischen
GPIO ergibt. Vorab werden keine drei GPIOs reserviert.

Das konkrete trennbare Produktfuehler-Stecksystem und seine elektrische
Ausfuehrung sind keine Komponente dieses Softwareaudits und bleiben eine
spaetere Hardwareentscheidung. Die Softwareevaluation behaelt allgemeine
Trennungs-, Unterbruch-, Fehler- und Wiederkehrtests bei, ohne Stecksystem,
Kontaktreihenfolge oder Anschlussbelegung vorzugeben.

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
| Release-1-Anforderung | Boot AUS, beide Richtungen nie gleichzeitig, Pulldowns, Totzeit und begrenzte Servicepulse; R_IS/L_IS sind in R1 deaktiviert und `FUTURE_RELEASE` |
| Kandidaten | ESP-IDF-GPIO/LEDC-Built-ins der fixierten ESP-IDF-`6.0.2`-Toolchain; kein hochstufiger BTS7960-Treiber erforderlich |
| Herstellerquelle | [Infineon BTS7960-Datenblatt](https://www.infineon.com/assets/row/public/documents/10/57/infineon-bts7960-ds-en.pdf), lokale Kopie unter `references/datasheets/` |
| Version/Lizenz | ESP-IDF `6.0.2`; Herstellerdatenblatt ist Referenz, kein uebernommener Code |
| Kompatibilitaet | Built-in ist Teil des Zielbuilds; Modulvariante, Pegel und Beschaltung bleiben unbestaetigt |
| Ressourcen | kleiner GPIO-/Timeradapter; reale Timerbelegung und Builddelta messen |
| Adapter | `IBidirectionalActuatorSink`-Implementierung mit sicherer Initialisierung; keine Rollenbegriffe im Plattformport |
| Eigene Logik | gesamte Safety-Freigabe, Mindestzeiten, Totzeit, Fehlerreaktion und Servicebegrenzung |
| Risiken/Hardwaretest | Lieferantenboard ist nicht identisch mit dem Infineon-Bauteil; Modulschaltung und Enable gegen reale Modulunterlagen pruefen, Pull-down/fail-low Beschaltung und Adapterinterlock nachweisen und Richtung/Polaritaet erst im begrenzten Servicepuls funktional bestimmen; R_IS/L_IS bleiben in R1 unverbunden und werden erst in einem eigenen spaeteren Issue/Plan/Owner-Gate bewertet |
| Empfehlung/Status | `CONFIGURE_FRAMEWORK` plus eigener Adapter; `BLOCKED_HARDWARE`, keine externe BTS7960-Bibliothek auswaehlen |

## Luefter und MOSFET-Ausgaenge

| Merkmal | Bewertung |
|---|---|
| Aufgabe | zwei 12-V-Luefter und der aktive Summer als einziges zusaetzliches lokales Ausgabeelement ueber bestaetigte binaere Ausgaenge |
| Release-1-Anforderung | sichere Bootpegel, Innenluefterbetrieb, Aussenluefter ohne absichtlichen Vorlauf und mit Nachlauf, Summer fuer nicht blockierende akustische Warnungen und Hinweise; keine Status-LED |
| Kandidaten | ESP-IDF-GPIO/LEDC-Built-ins; keine Geraeterollen in der Plattform |
| Quelle/Stand/Lizenz | ESP-IDF `6.0.2` (Issue #71 / PR #79, Basis-Commit `7101770dc6db2667b3c477cc31365dd1acd6db4e`), Apache-2.0 |
| Ressourcen | kleine `IBinaryOutputSink`-Adapter; Timer/PWM nur falls reale Hardware es verlangt |
| Eigene Logik | Nachlauf, Sicherheitsprioritaet, Meldungsmuster und Rollenbindung |
| Risiken/Hardwaretest | Kanalbelegung der Quad-MOSFET-Platine ist nur `confirmed_order`; Kanal-/Verbraucherfunktion und fail-closed Boot-/Resetverhalten funktional pruefen. Strom- und Anlaufdaten nur bei konkretem Gate und geeignetem Messmittel erfassen. |
| Empfehlung/Status | `CONFIGURE_FRAMEWORK`; keine separate Luefterbibliothek; `BLOCKED_HARDWARE` |

Der 230-V-AC-Hauptschalter gehoert nicht zu diesen Ausgaengen und erhaelt auch
keinen Eingangsadapter. Er schaltet das Geraet rein elektrisch ein oder aus.

## ESP-IDF NVS

| Merkmal | Bewertung |
|---|---|
| Aufgabe | produktives Blob-Backend fuer den vorhandenen `IStateStore` |
| Release-1-Anforderung | kurze ADR-016-Schluessel, binaersichere Werte, typisierte Read-/Writefehler, atomarer einzelner Commit und Rueckfalllogik oberhalb des Backends |
| Kandidaten | durch ADR-016 festgelegtes produktives Backend: direkter ESP-IDF-`nvs_flash`-Adapter, da kein Arduino-Produktionspfad besteht (`AGENTS.md`); keine offene Produktauswahl mehr, Adapterimplementierung und reale Verifikation in Backlog-Issue #90 |
| Quelle/Stand/Lizenz | [NVS-Dokumentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/nvs_flash.html), ESP-IDF `6.0.2`, Apache-2.0 |
| Wartung/Kompatibilitaet | offizieller Plattformbestandteil; Keys bis 15 Zeichen und Bytewerte dokumentiert |
| Ressourcen | NVS-Partition und Runtime-Handles; Groesse bleibt bis #29 `TBD_IMPLEMENTATION_BUDGET` |
| Adapter | vorhandenen `IStateStore` implementieren; `CommitOutcomeUnknown` und Limits explizit uebersetzen |
| Eigene Logik | Envelope, Slots, fachliche Revisionen, Recovery und Secrets bleiben im Projekt; der Backendadapter entscheidet weder Ereigniskategorien noch Retention-, Prioritaets-, Verdichtungs- oder Bereinigungssemantik |
| Risiken/Hardwaretest | reale Atomizitaet, Stromausfallverhalten, Kapazitaet und Flashlebensdauer nicht aus der Hostsimulation ableiten |
| Empfehlung/Status | gemaess ADR-016 `ADAPTER_EXISTING_LIBRARY`; keine eigene Flashdatenbank |

### Grenze zu Issue #19

Das interne Ereignisjournal und die begrenzte Laufhistorie sind typisierte,
versionierte Projektvertraege und keine JSON-Datenbank. NVS beziehungsweise
der `IStateStore` speichert binaere Records, entscheidet aber nicht, welche
Ereignisse kritisch sind, welche Messreihen verdichtet werden oder welche
Komfortdaten zuerst geloescht werden. Diese Retention-, Prioritaets- und
stromausfallsichere Bereinigungssemantik bleibt eigene Fachlogik.

JSON ist fuer begrenzte externe Laufexport-, secret-freie Backup- und
Importvertraege vorgesehen. Der nur lesende Export-/Backuppfad wird getrennt
vom spaeteren Import umgesetzt. Ein Import entsteht als vollstaendiger
typisierter Kandidat, durchlaeuft Vorschau, Konfliktpruefung und Bestaetigung
und verwendet fuer die Aktivierung den OD-01-Active-/Fallback-Kern. Ein
synchroner Run-Gate erlaubt Annahme, Vorschau/Bestaetigung und Commit nur bei
`NoActiveOrRecoverableRun`; `Unknown`, `ActiveRunPresent` und
`RecoverableRunPresent` blockieren. Runstart und Commit werden ohne Pending,
Intent oder parallelen Active-Zweig serialisiert. Weder das
Storagebackend noch ArduinoJson uebernimmt diese Transaktionssemantik. Fuer
diesen Teilschnitt wird keine neue Bibliothek ausgewaehlt.

## JSON

| Merkmal | Bewertung |
|---|---|
| Aufgabe | begrenzte Web-API-, Konfigurations-, Programm-, Diagnose-, nur lesende Laufexport-, secret-freie Backup- und getrennte Importformate; keine interne Journal-, Historien- oder Kontrollpunktpersistenz |
| Release-1-Anforderung | korrekte UTF-8-/Escape-/Zahlenverarbeitung, feste Byte-/Struktur-/Feldgrenzen, stabile Projektfehler, Redaction, Importvorschau ohne Aktivierung und Streaming/Pagination grosser Ausgaben |
| Bevorzugter Kandidat | ArduinoJson `7.4.3`, Tag-Commit `77771d3c07668e01d8f52acb03910c1110bb373f`; `FIRST_EVALUATION_CANDIDATE`, `SPIKE_REQUIRED`, `FINAL_SELECTION_PENDING` |
| Quelle/Lizenz | [ArduinoJson](https://github.com/bblanchon/ArduinoJson), offizieller Tag `v7.4.3`, MIT; Paketmanifest, konkret verwendete Header/Features, transitive Bestandteile und Notices im Spike pruefen |
| Kompatibilitaet | isolierter reproduzierbarer Build mit ESP-IDF `6.0.2` (`esp32_bringup`/`esp32_release`, Composition Root `main/app_main.cpp`), C++17, ESP32-32E, 4 MB Flash und ohne PSRAM-Abhaengigkeit erforderlich |
| Ressourcen | ArduinoJson 7 verwaltet Dokumente dynamisch; Modell, alter Ausgabezustand und neuer Serialisierungspfad koennen gleichzeitig leben. Flash, statisches RAM, Heapspitze/-minimum/-blockgroesse, Fragmentierung und Zeiten pro Profil real messen |
| Adapter | kleine konkrete DTO-/Codecgrenze in ESP32-/Transportintegration; Bibliotheksfehler vollstaendig in stabile Projektfehler uebersetzen; kein `IJsonProvider`, Pluginregister oder Dummy-Zweitcodec |
| Eigene Logik | Endpunkt- und Feldschema, Root-Typ, String-/Array-/Wertebereiche, Berechtigung, Redaction, Konflikte, Secretgrenzen, Trennung von Export/Backup und Import, vollstaendiger Importkandidat, Vorschau, Bestaetigung und atomare OD-01-Aktivierung |
| Alternative | andere Bibliothek oder Eigenloesung nur nach belegtem Toolchain-, Ressourcen-, Stabilitaets-, Limitierungs- oder Publikationsproblem; kein eigener allgemeiner Parser/Serializer |
| Empfehlung/Status | bevorzugter R1-Kandidat fuer #19/#27/#28 mit `FIRST_EVALUATION_CANDIDATE`, `SPIKE_REQUIRED`, `FINAL_SELECTION_PENDING`; Richtungsentscheid getroffen, endgueltige Uebernahme erst nach vollstaendigem Nachweis |

### Release-1-Nutzungs- und Architekturgrenze

JSON wird nur an externen, begrenzten Vertraegen eingesetzt. Atomare
Kontrollpunkte, Active-/Fallback-Roots, Safety-Zustaende, Lauf-Recovery und
interne Records verwenden weiterhin die vorhandenen typisierten Binaercodecs.
Parsererfolg ist keine fachliche Gueltigkeit. Ein Import wird zunaechst
technisch und danach fachlich validiert, als Vorschau dargestellt und erst
nach dem normalen Bestaetigungs-/Konfliktpfad aktiviert.

```text
Bytequelle
  -> Methode, Content-Type, Content-Length und harte Bytegrenze
  -> konkreter ArduinoJson-Codec
  -> stabiler projektspezifischer Parse-/Strukturfehler
  -> typisiertes DTO
  -> Schema-, Werte-, Berechtigungs-, Konflikt- und Fachvalidierung
```

`JsonDocument`, `JsonObject`, `JsonArray`, `JsonVariant` und
bibliotheksspezifische Fehler bleiben innerhalb dieser Codec-/Integrations-
grenze. Sie duerfen weder `fermentation_app`, Safety-, Sensor-, Regel-, Aktor-,
Prozess-, Lauf-, Persistenz-, Root-, Secret- oder gemeinsame View-Modelle noch
fachliche Ports oder Kommandos praegen. Fuer Antworten folgen auf typisierte
DTOs zuerst Feldfreigabe und Redaction, dann eine moeglichst direkte
Serialisierung in ein begrenztes `Print`-/Streamziel. Grosse Historien und
Diagnosedaten werden begrenzt, paginiert oder gestreamt.

Eine manuelle Ausgabe bleibt hoechstens fuer nachweislich triviale feste
Antworten zulaessig. Sie darf weder einen zweiten allgemeinen JSON-Pfad noch
abweichende Escape-, Redaction- oder Fehlerregeln begruenden.

### Initiale Grenzprofile

| Profil | Beispiele | harte Requestbodygrenze im Spike |
|---|---|---:|
| A | Start, Stop, Bestaetigung, einzelne kleine Einstellung | 1 KiB |
| B | Programm- oder zusammengehoerige Konfigurationsaenderung | 4 KiB |
| C | vollstaendiger R1-Import | `DERIVE_FROM_MAXIMUM_VALID_EXTERNAL_SCHEMA`; `MEASUREMENT_REQUIRED` |

Die maximale Verschachtelung betraegt zunaechst 6. Jedes Schema legt Root-Typ,
Pflichtfelder, String-, Array-, Objektfeld-, Zahlen- und Schemaversiongrenzen
sowie die einheitliche Behandlung unbekannter Felder fest. `NaN`, `Infinity`
und andere nicht standardkonforme oeffentliche Zahlenwerte werden abgelehnt.
Content-Type und Methode sind verbindlich; Content-Length wird vor dem Einlesen
geprueft, sofern vorhanden. Nicht verlaesslich begrenzbare Bodies werden
zeitlich und mengenmaessig begrenzt verarbeitet oder abgelehnt. Die Werte sind
initiale Spikegrenzen nur fuer A und B und brauchen den jeweiligen maximalen
DTO-Nachweis. Fuer C erzeugt der Spike deterministisch den maximal gueltigen
externen Kandidaten mit maximal 16 Programmen, IDs bis 48 Byte, Namen bis 96
Byte, Notizen bis 1.024 Byte je Programm, allen Programmfeldern,
Konfiguration, Schema, JSON-Struktur, Worst-Case-Escaping,
UTF-8, Metadaten sowie Integritaets-/Referenzfeldern. Erst daraus folgt eine
harte Grenze. Der vollstaendige Maximalfall muss importierbar sein; Gesamtbody
mit begruendeter Reserve und begrenztes Streaming/Chunking bleiben bis zur
Messung offen. Beide Wege validieren einen vollstaendigen typisierten
Kandidaten vor atomarer Aktivierung. Backupausgabe und Importrequest sind
getrennte Vertraege.

### Stabiler Fehlervertrag

Die Integrationsschicht uebersetzt mindestens:

- Requestbody zu gross, nicht unterstuetzter Content-Type und abgebrochene
  oder unvollstaendige Eingabe;
- syntaktisch ungueltiges JSON, zu tiefe Verschachtelung und erreichtes
  Speicher-/Ressourcenlimit;
- falschen Root-Typ, fehlende Pflichtfelder, unbekannte oder unzulaessige
  Felder und falsche Datentypen;
- zu lange Strings, zu grosse Arrays sowie Zahlen oder Werte ausserhalb ihres
  Bereichs;
- unbekannte Schema-/Schemaversion, Secret oder geschuetztes Feld im falschen
  Vertrag und unzureichende Berechtigung;
- Konflikt mit aktiver Revision, technisch gueltigen aber fachlich ungueltigen
  Import, noch nicht bestaetigten Import und interne Serialisierungsfehler.

Oeffentliche Fehler enthalten keine Secrets, Bibliotheksdetails,
Speicheradressen oder ungefilterten Eingaben.

### Gestufter Spike- und Auswahlpfad

1. **Quelle, Lizenz und Toolchain:** Tag/Commit, MIT-Lizenz, Manifest,
   verwendete Header/Features, Abhaengigkeiten und Notices erfassen und den
   isolierten fixierten ESP32-Build reproduzieren.
2. **Begrenzter Codecprototyp:** kleine und maximale gueltige Requests,
   Status-/Response-DTO, Export, vollstaendiger R1-Importkandidat,
   Importvorschau ohne Aktivierung, Streaming und Fehleruebersetzung abbilden.
3. **Grenz-, Negativ- und Fuzztests:** leer/abgeschnitten, Syntax/Escapes/UTF-8,
   Root/Felder/Typen/Zahlen, lange Strings/grosse Arrays, Tiefe und Bytegrenze
   an/ueber dem Limit, Schema, `NaN`/`Infinity`, langsame/abgebrochene Quellen,
   Wiederholungen, Secrets und Parseerfolg mit Fachfehler reproduzierbar
   pruefen. Jeder Fehler endet ohne Teilaktivierung.
4. **Ressourcen und Laufzeit:** Firmware, statisches RAM, freien/niedrigsten
   Heap, groessten freien Heapblock, gleichzeitige Speicherbelegung,
   Fragmentierung ueber wiederholte Zyklen, Parse-/Serialisierungszeit,
   Regelzyklus-Jitter sowie Watchdog-/Reset-/Stabilitaetsereignisse messen.
5. **Ownerentscheid:** ArduinoJson erst nach bestandenem Nachweis endgueltig
   uebernehmen. Eine Alternative wird nur bei konkret dokumentierter
   Toolchaininkompatibilitaet, unvertretbaren Ressourcen, nicht begrenzbarem
   Speicherverhalten/Fragmentierung, Instabilitaet, Safety-/Jitterwirkung,
   unerfuellbarer R1-Anforderung oder Publikationsproblem untersucht.

Ein spaeterer Codecwechsel bleibt durch die konkrete DTO-/Codecgrenze
moeglich. Er ersetzt diese Integration und nicht Fachmodelle oder interne
Persistenz; daraus folgt keine allgemeine Provider- oder Pluginarchitektur.

## Diagnose-, Ressourcen- und Servicegrenze

Der OD-07-Teilentscheid zu #28 waehlt keine technische Komponente aus. Die
passiven Diagnosemodelle, Ressourcenwarnungen, der gefuehrte Serviceablauf und
der typisierte Diagnose-/Servicebericht sind projektspezifische Fachlogik.
Plattformmetriken wie freier Heap, niedrigster freier Heap, groesster freier
Block, Firmware-/Buildgroessen, Flash-/Partitions- und Resetstatus gelangen nur
ueber schmale konkrete Messpunkte in diese Modelle.

Es wird weder eine Diagnose-, Logging-, Telemetrie-, Metrics-, Chart- noch
Exportplattform fuer R1 ausgewaehlt. Insbesondere entstehen kein generisches
Prometheus-/OpenTelemetry-Modell und keine unbeschraenkte Zeitreihendatenbank.
Zaehler, Ereignisse und Puffer bleiben fest begrenzt. Schwellen, Heap-/Flash-
reserven, Partitions- sowie Journal-/Historienbudgets sind
`MEASUREMENT_REQUIRED`; sie werden erst mit der realen ESP32-Firmware und den
tatsaechlichen Bibliothekskandidaten bestimmt und nicht als Produktgarantie
vorweggenommen.

Die Aufteilung verhindert parallele technische Pfade: Der aktuelle pollende
Laufchart bleibt #27-B, persistente verdichtete Historie #19-B und die
gemeinsame JSON-/CSV-/Download-/Streaminginfrastruktur #19-C. #28-D liefert
dafuer nur einen versionierten, bereits redigierten und nur lesenden
Fachbericht. Ein Codec bleibt hinter dem JSON-/Transport-Spike-Gate und erhaelt
keine Bibliothekstypen im Fachkern. Serviceablaeufe werden zuerst mit Mocks
geprueft; reale Aktortests bleiben hinter #24 und den Hardwaregates,
Authentisierung hinter OD-09.

Status: Diagnoseanforderung `REQUIREMENT_DECIDED`, technische Auswahl soweit
noetig `FINAL_SELECTION_PENDING`, Ressourcen `MEASUREMENT_REQUIRED`, reale
Hardwarefreigabe `HARDWARE_GATE_PENDING`.

## Webserver

| Kandidat | Gepruefter Stand/Lizenz | Eignung | Ressourcen/Risiken | Empfehlung |
|---|---|---|---|---|
| ESP-IDF `esp_http_server` | ESP-IDF-Built-in, ESP-IDF `6.0.2`, Apache-2.0 | synchroner lokaler HTTP-Server fuer statische Ressourcen, begrenzte JSON-Endpunkte und wenige Clients; keine zusaetzliche Serverbibliothek | langsame/abgebrochene Clients, Parallelitaet, Import/Export, Antwortzeit, Regelzyklus-Jitter, Watchdog, Flash/RAM/Heap und Verbindungslebenszyklus begrenzt messen | `FIRST_EVALUATION_CANDIDATE`, `SPIKE_REQUIRED`, bedingte Produktivrichtung und `FINAL_SELECTION_PENDING`; nur nach bestandenem begrenztem Prototyp produktiv integrieren |
| ESPAsyncWebServer | `3.12.0`, `a008cccf`, LGPL-3.0; asynchrone TCP- und optionale JSON-Abhaengigkeiten separat; setzt eine Arduino-Laufzeit voraus, die kein aktueller Produktionspfad ist (`AGENTS.md`) | kann bei belegtem Bedarf parallele Verbindungen oder Ereignis-/Streamingpfade anders behandeln; fuer R1 sind WebSocket und SSE nicht vorausgesetzt | zusaetzliche Abhaengigkeit, Callback-/Lebensdauerkomplexitaet, Heaplast, transitive Komponenten, LGPL-Pflichten und ein dokumentierter Integrationspfad ohne Arduino-Produktionspfad | ergebnisoffener Evaluationskandidat, `EVALUATE_LATER`; nur bei konkretem Scheitern des ersten Kandidaten und klarem Vorteil im identischen Prototyp uebernehmen |

### Release-1-Bedarf und Nichtbedarf

Die Weboberflaeche ist sekundaer; das Touchdisplay bleibt ohne WLAN und Browser
die vollstaendige lokale Bedien- und Anzeigeoberflaeche. Der lokale HTTP-Dienst
benoetigt statische HTML-/CSS-/JavaScript-Ressourcen, Status-, Temperatur-,
Lauf-, Programm- und Konfigurationsabfragen, begrenzte Aenderungs- und
Laufkommandos, Diagnose, begrenzte Exporte/Imports, Anmeldung/Sitzungen und die
HTTP-Oberflaeche des WLAN-Onboardings. Status und Diagramme duerfen begrenzt
pollend abgerufen werden.

Keine Release-1-Pflicht sind WebSocket, Server-Sent Events, ein permanenter
bidirektionaler Stream, hohe Clientzahlen, Cloudtransport, direkter
Internetbetrieb, unbeschraenkte Uploads oder Millisekunden-Echtzeitdarstellung.

### Baselineprototyp und konditionaler Vergleich

Der Frameworkserver-Prototyp muss nachweisen:

- statische Ressourcen, begrenzte JSON-Anfragen, Import und Export;
- stabile Bedienung durch einen bis wenige lokale Clients;
- feste Request-, Antwort-, JSON-Tiefen-, String-, Upload-, Zeit- und
  Parallelitaetsgrenzen;
- kontrollierte langsame, abgebrochene, ungueltige und uebergrosse Requests
  sowie WLAN-Unterbruch und Neustart;
- keine relevante Verzoegerung von Regel- und Safety-Ausfuehrung;
- gemessene Flash-, statische RAM-, freie/niedrigste Heap-, groesste freie
  Heapblock-, Antwortzeit-, Bearbeitungszeit-, Jitter-, Watchdog- und
  Resetwerte.

Nur wenn dabei ein konkretes R1-Risiko offenbleibt, erhalten `esp_http_server`
und `ESPAsyncWebServer` denselben begrenzten Vergleich mit Testseite,
`GET /api/status`, `GET /api/config`, simuliertem begrenztem
Aenderungsrequest, Export, Import/Upload, normalen und parallelen Clients,
langsamem und abgebrochenem Client, ungueltiger/uebergrosser Anfrage,
wiederholtem Polling, WLAN-Unterbruch und Neustart. Async ist nur bei
unvertretbarem Jitter, nicht sinnvoll begrenzbarer Blockierung, instabiler
tatsaechlich benoetigter Parallelitaet, nicht robust begrenzbarem Import/Export
oder klarem Stabilitaets-/Ressourcenmehrwert begruendet. "Moderner", populaerer
oder vorsorglich WebSocket-/SSE-faehig reicht nicht.

### Integrations- und Sicherheitsgrenze

Eine kleine konkrete ESP32-Schicht kapselt Serverinitialisierung,
Routenregistrierung, Methoden/Header, feste Bodygrenzen, Timeouts,
Request-/Responseuebersetzung und technische Fehler. Die Endpunktlogik
uebersetzt nur zwischen HTTP, begrenztem DTO, fachlicher Query oder fachlichem
Kommando und typisierter Antwort. Server-, Request-, Response-, Connection-,
Socket- und Callbacktypen gelangen nicht in `fermentation_app`, Safety-,
Persistenz- oder gemeinsame View-Modelle.

Es entsteht keine allgemeine `IWebTransport`-, Stream-, SSE-, WebSocket-,
Middleware-, Provider- oder Pluginhierarchie und kein Dummy-Zweitadapter. Ein
spaeterer Serverwechsel erfolgt an Composition Root und ESP32-
Integrationsgrenze und ersetzt Serverlebenszyklus sowie HTTP-Uebersetzung,
nicht DTOs, fachliche Queries/Kommandos, Validierung, Authpolicy,
Konfliktsemantik, Persistenz oder Safety-Core.

Webserver- und WLAN-Fehler stoppen Regelung und Safety nicht; WLAN-Verlust
beendet keinen Lauf. Webanfragen wirken nur ueber normale fachliche Kommando-
und Safety-Pfade. Authentisierung, CSRF, Sessions, Sperrlogik, Redaction und
Importvalidierung bleiben eigene Vertraege. Secrets gelangen nicht in Logs,
Exporte oder Fehlermeldungen. Entscheidungsstatus: Der lokale HTTP-Dienst ist
`REQUIREMENT_DECIDED`. ESP-IDF `esp_http_server` ist
`FIRST_EVALUATION_CANDIDATE`, `SPIKE_REQUIRED` und
`FINAL_SELECTION_PENDING`; `ESPAsyncWebServer` bleibt ein ergebnisoffener
Evaluationskandidat mit `EVALUATE_LATER`. Die Evaluationsrichtung ist
entschieden, die Produktivauswahl nicht.

Quellen: [ESP-IDF esp_http_server](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/protocols/esp_http_server.html),
[ESPAsyncWebServer](https://github.com/ESP32Async/ESPAsyncWebServer). Abgerufen
am 2026-08-05.

### Grenze zum OD-07-Teilentscheid fuer #27

Der Webserververgleich bewertet ausschliesslich den technischen HTTP-
Lebenszyklus und nimmt die fachliche Webumsetzung nicht vorweg. #27 wird
spaeter in diese fuenf Bereiche geschnitten:

1. HTTP-Transport und interne versionierte, begrenzte API-/DTO-Vertraege;
2. nur lesende Statussnapshots, nicht ueberlappendes begrenztes Polling und ein
   punktbegrenzter aktueller Laufchart;
3. schreibende Fachkommandos mit erwarteter Revision, Konflikt- und
   Doppelwirkungsschutz erst nach den OD-09-Technikgates;
4. schlanke lokale responsive HTML-/CSS-/JavaScript-Assets als
   `FIRST_EVALUATION_DIRECTION`, ohne ausgewaehltes Frontendframework;
5. Anmeldung, Sessions, CSRF und Servicefreigabe gemaess OD-09 erst nach den
   zugehoerigen KDF-, Zufalls- und Ressourcennachweisen.

R1 verspricht keine stabile oeffentliche externe Schreib-API. Konkrete
Pollingintervalle, parallele Clientzahl, Antwort-/Chartgroesse, Timeouts,
Heap- und Jitterbudgets werden erst im Prototyp festgelegt. WebSocket und SSE
werden nicht als Reserve implementiert. HTTP-Handler und Browsercode leiten
weder Fach-, Sensorrollen-, Safety-, Berechtigungs- noch Konfliktentscheidungen
neu her. OD-06-Onboarding bleibt ein getrennter fachlicher Arbeitsbereich,
selbst wenn technische Frameworkbausteine geteilt werden.

## WLAN-Onboarding

Espressif-first (`docs/ENGINEERING_PRINCIPLES.md`) verlangt, dass drei
ESP-IDF-6.0.2-Pfade gleichwertig gemessen werden, bevor ein
Drittanbieter-Fallback zur Auswahl steht. Details und Quellen der drei
Espressif-Pfade stehen im
[Third-Party-Components-Register](../THIRD_PARTY_COMPONENTS.md) und in
[`ADOPT_OR_BUILD.md`](../ADOPT_OR_BUILD.md#wlan-onboarding).

| Kandidat | Stand/Lizenz | Eignung und Grenzen | Empfehlung |
|---|---|---|---|
| `espressif/network_provisioning` auf `protocomm` | `network_provisioning 1.2.4`, ESP-IDF >=5.1, Apache-2.0; `protocomm` offizielle ESP-IDF-Komponente ohne separate Registry-Version | offizieller Espressif-Provisioning-Stack; ersetzt das in ESP-IDF 6.0 entfernte `wifi_provisioning`; BLE-Pfad fuer R1 nicht verpflichtend | Espressif-first-Erstkandidat; `SPIKE_REQUIRED`, `FINAL_SELECTION_PENDING` |
| direkter `protocomm`-/ESP-IDF-SoftAP-/HTTP-/DNS-Ansatz ohne `network_provisioning` | ESP-IDF `6.0.2`, Apache-2.0 | mehr eigener Integrationscode, aber keine zusaetzliche High-Level-Abhaengigkeit; nutzt denselben `esp_http_server`-Kandidaten | gleichwertiger Espressif-first-Kandidat; `SPIKE_REQUIRED`, `FINAL_SELECTION_PENDING` |
| kleiner eigener nativer ESP-IDF-SoftAP-/DNS-/HTTP-Adapter | ESP-IDF `6.0.2`, kein Drittanbieter | keine zusaetzliche Portalbibliothek, aber voller eigener technischer Portal-, DNS- und Clientlebenszykluscode; nutzt denselben `esp_http_server`-Kandidaten | gleichwertiger Espressif-first-Kandidat; `SPIKE_REQUIRED`, `FINAL_SELECTION_PENDING` |
| WiFiManager | `v2.0.17`, `d82d0a1b`, MIT | stellt den standardisierten Portalteil fuer SoftAP, DNS-Umleitung, Captive Portal, Netzwerkscan, Formulare, Timeouts und Clientverhalten bereit; setzt eine Arduino-Laufzeit voraus, die kein aktueller Produktionspfad ist (`AGENTS.md`); projektspezifische Start-, Secret-, Commit-, Recovery- und Safetysemantik bleibt ausserhalb | ergebnisoffener konditionaler Evaluationskandidat, `SPIKE_REQUIRED`, `FINAL_SELECTION_PENDING`; dasselbe Evaluationsgate wie die drei Espressif-Pfade (direkter ESP-IDF-6.0.2-Build/-Betrieb oder dokumentierter Integrationspfad ohne stilles Wiedereinfuehren eines Arduino-Produktionspfads) |

Jeder Kandidat muss im identischen Spike zusaetzlich nachweisen, ob und wie er
den browserbasierten R1-Vertrag ohne verpflichtende App, Cloud oder separates
Kommandozeilenwerkzeug erfuellt; offizielle Espressif-Herkunft allein
qualifiziert dafuer nicht. Die Bewertung dieses Abschnitts ist Teil des
Backlog-Issues #89.

### Release-1-Bedarf und Grenze

Das Onboarding wird nur bei einem fabrikneuen Geraet ohne bestaetigte
Zugangsdaten oder durch ausdrueckliche Benutzeraktion am Touchdisplay gestartet.
Ein voruebergehender Router-, Access-Point-, WLAN- oder Internetausfall startet
kein Portal. Release 1 zeigt am Touchdisplay Onboardingstatus, SoftAP-Name,
notwendige Zugangsinformation und direkte Portaladresse beziehungsweise IP.
Das lokale Portal nimmt WLAN-Auswahl und Zugangsdaten entgegen. Neue Daten
bleiben bis zu einem zeitlich begrenzten Verbindungs- und Funktionsnachweis ein
unbestaetigter Kandidat. Nach Erfolg werden Status und Erfolg am Touchdisplay
angezeigt und der Kandidat an die projektspezifische Secret-/Commitlogik
uebergeben. Fehler, Timeout oder Abbruch erhalten den bisherigen funktionierenden
WLAN-Stand. Offlinebetrieb, Fermentation, Regelung, Safety und lokale Bedienung
bleiben unabhaengig von WLAN, Portal und Browser.

BLE, Smartphone-App, SmartConfig, Cloud-Provisioning, mehrere Provider,
automatische Internetfreigabe und komplexe Netzwerkverwaltung sind keine
Release-1-Pflicht. Der primaere QR-Code enthaelt die individuelle SoftAP-SSID
und das individuelle SoftAP-Passwort im gaengigen WLAN-QR-Format. SSID,
Passwort, Portaladresse beziehungsweise IP und eine lokale Schaltflaeche zur
erneuten QR-Anzeige bleiben separat sichtbar; die Portaladresse ist der
manuelle Rueckfall nach dem WLAN-Beitritt, nicht der primaere QR-Inhalt. Diese
Anforderung waehlt keine QR-Bibliothek aus.

Jeder der vier Kandidaten dient ausschliesslich als technischer Portalbaustein.
Ausserhalb der jeweiligen Komponente bleiben Startentscheidung, Touchstatus
und Abbruch, der unbestaetigte Credential-Kandidat, Verbindungsnachweis und
ausdruecklicher Commit, Secret-Lebenszyklus, Redaction, Recovery,
Fehlersemantik sowie die Isolation von Regelung und Safety. Portal-, SoftAP-,
DNS-, HTTP-, Callback- und Frameworktypen enden in der konkreten
ESP32-Integrationsschicht. Es entsteht weder ein allgemeines
`IProvisioningProvider` noch eine Provider-, Plugin- oder vorsorgliche
Mehradapterarchitektur.

### Stufe 1 – Quelle, Lizenz und Toolchain

Fuer alle vier Kandidaten werden die exakte Version beziehungsweise der
Commit, Lizenz, eingebettete Webassets, transitive Abhaengigkeiten und
verwendete sowie deaktivierte Funktionen dokumentiert. Der isolierte Build
muss mit ESP-IDF `6.0.2` (`esp32_bringup`/`esp32_release`), ESP32-32E, 4 MB
Flash und ohne PSRAM reproduzierbar sein; fuer WiFiManager gilt zusaetzlich das
Espressif-first-Evaluationsgate (direkter ESP-IDF-6.0.2-Build/-Betrieb oder
dokumentierter Integrationspfad ohne Arduino-Produktionspfad). Insbesondere ist
zu pruefen, wie framework- oder bibliotheksseitig gespeicherte WLAN-Daten
verhindert beziehungsweise kontrolliert gekapselt werden und ob automatischer
Portalfallback sowie automatischer Credential-Commit abschaltbar sind. Ein
erfolgreicher Build ist noch keine Uebernahmeentscheidung.

### Stufe 2 – identischer begrenzter Prototyp je Kandidat

Jeder der vier Prototypen prueft mindestens:

- ausdruecklich gesteuerten Portalstart und keinen Portalstart bei gewoehnlichem
  temporaerem WLAN-Ausfall;
- vollstaendigen Start und Abbau von SoftAP, DNS und Portal;
- DNS-Umleitung und direkten Aufruf ueber die angezeigte IP;
- WLAN-Scan, gueltige Daten, falsches Passwort und nicht erreichbaren Access
  Point;
- Abbruch, Timeout, Browserabbruch, WLAN-Unterbruch, Neustart, geeignete
  Stromunterbruch-Cut-Points und erneutes Oeffnen;
- Erhalt bisher funktionierender Zugangsdaten bei Fehlschlag und keinen
  unkontrollierten kanonischen Commit durch die Bibliothek;
- keine Secrets in Logs, URLs, Diagnose, Exporten oder Fehlermeldungen;
- keine relevante Blockierung von Regelung oder Safety und vollstaendige
  Ressourcenfreigabe nach Portalende.

Der reale Clienttest umfasst Android, iOS beziehungsweise iPadOS und Windows.
Je Client werden automatische Captive-Portal-Erkennung und der direkte
IP-Aufruf getestet; nur der direkte Aufruf ist der verlaessliche Rueckfall.
Gemessen werden Firmwaregroesse, statisches RAM, freier und niedrigster Heap,
groesster freier Heapblock, Portalstart-, Verbindungs- und Antwortzeiten,
Regelzyklus-Jitter, Watchdog-/Resetereignisse, Abhaengigkeiten und Umfang des
projektspezifischen Integrationscodes.

### Stufe 3 – gemeinsame Messmatrix

Alle Kandidaten, die Stufe 2 bestehen, durchlaufen dieselbe Messmatrix aus
Firmwaregroesse, statischem RAM, freiem/niedrigstem Heap, groesstem freien
Heapblock, Portalstart-/Verbindungs-/Antwortzeiten, Regelzyklus-Jitter,
Watchdog-/Resetereignissen, Abhaengigkeiten, Umfang des projektspezifischen
Integrationscodes und dem Nachweis des browserbasierten R1-Vertrags ohne
verpflichtende App, Cloud oder Kommandozeilenwerkzeug. Kein Kandidat wird
vorab wegen blosser Espressif- oder Drittanbieterherkunft ausgeschlossen oder
vorsorglich voll implementiert.

### Stufe 4 – endgueltiger Ownerentscheid

Erst anhand des Spikeberichts entscheidet der Owner zwischen den drei
Espressif-Pfaden und WiFiManager. Bis dahin gelten alle vier als
gleichrangige Kandidaten mit `SPIKE_REQUIRED`, keiner ist ausgewaehlte
Abhaengigkeit. Ein spaeterer Wechsel ersetzt nur Portalinitialisierung,
konkrete Callbackbindung und Frameworklebenszyklus an der Composition Root;
fachliche Connectivity-, Secret-, Recovery- und Safetyvertraege bleiben
erhalten. Ein Dummy-Zweitadapter wird nicht erstellt.

Quellen: [network_provisioning 1.2.4](https://components.espressif.com/components/espressif/network_provisioning/versions/1.2.4/readme?language=en),
[protocomm (ESP-IDF)](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/provisioning/protocomm.html),
[WiFiManager](https://github.com/tzapu/WiFiManager). Abgerufen am 2026-08-05.
Entscheidungsstatus: OD-06-Richtung entschieden (drei gleichwertige
Espressif-first-Pfade plus WiFiManager als ergebnisoffener konditionaler
Kandidat), endgueltige Uebernahme `SPIKE_REQUIRED` und
`FINAL_SELECTION_PENDING` fuer den von #27 getrennten, im Backlog-Issue
#89 gefuehrten Onboarding-Arbeitsbereich.

### Getrennter Ersatz-WLAN-Lebenszyklus

Das geschuetzte Ersatz-WLAN ist `REQUIREMENT_DECIDED` und strikt vom
Onboarding getrennt. Es startet erst nach einem laengeren Ausfall eines bereits
konfigurierten Heim-WLANs; Aktivierungs- und Abschaltuebergang bleiben
`TBD_COMMISSIONING`. Heim-WLAN-Reconnect laeuft parallel, der Prozess bleibt
unveraendert und normale Weboberflaeche, Netzwerkdiagnose sowie erneute
Einrichtung bleiben unter den bestehenden Auth-, CSRF-, Service- und
Safetygates erreichbar. Nach stabiler Heim-WLAN-Rueckkehr endet das Ersatz-WLAN
kontrolliert, ohne offene Requests oder Speichervorgaenge abzuschneiden.
Clientzahl und Ressourcen sind `MEASUREMENT_REQUIRED`; dies ist kein neuer
Bibliotheksentscheid und keine Connectivity-Pluginplattform.

## QR-Code

| Kandidat | Stand/Lizenz | Ressourcen und Adapter | Empfehlung |
|---|---|---|---|
| LVGL `lv_qrcode` mit gebuendeltem Project Nayuki `qrcodegen.c` | LVGL `9.6.0~1`, Component-Hash `7d82410747bbfb319531449749e940f440f8414f041f9eaba13f0d1dd7f58cdd`; MIT und MIT-Text im Nayuki-Quellheader | bereits gepinnter und produktiv verwendeter LVGL-/`esp_lvgl_port`-Stack; keine neue Komponente | fuer #164 ausgewaehlte Reuse-Implementierung; digitale Integration/Builds nachweisen, realen Scan vor Issue-Abschluss nachholen |
| ricmoo/QRCode | `0.0.1`, `eafbde49`, MIT; nennt Project Nayuki als Ursprung | kleine C/C++-Bibliothek; Versions- und Wartungsaktivitaet gering; Ausgabematrix begrenzen | nicht ausgewaehlt; bestehende Referenz fuer den Fall, dass der LVGL-Pfad ungeeignet ist |
| Project Nayuki QR-Code-generator | `1.8.0`, `2c9044de`, MIT im Quellheader | portable C-Implementierung, auf feste QR-Version/ECC und caller-provided Buffer begrenzbar | bereits evaluierter bevorzugter Gegenkandidat, aber nicht ausgewaehlt; nur bei nachgewiesener Ungeeignetheit des vorhandenen LVGL-Pfads erneut bewerten |

Der Payloadvertrag ist `REQUIREMENT_DECIDED`: individuelle SoftAP-SSID und
individuelles SoftAP-Passwort im gaengigen WLAN-QR-Format. Escaping,
Sonderzeichen, sichtbare/versteckte SSID soweit unterstuetzt, Scannbarkeit auf
320 x 240 unter Android, iOS/iPadOS und soweit unterstuetzt Windows,
Kameraabstand, Helligkeit, Rotation, Fallback und Credentialwechsel werden
gemessen. Keine Zugangsdaten gelangen in Logs. Onboardingstatus, Secret-Schutz
und Displaydarstellung bleiben eigene Logik. Fuer #164 ist die Bibliothekswahl
auf den bereits vorhandenen LVGL-QR-Widgetpfad begrenzt; ricmoo und die separate
Nayuki-Komponente bleiben nicht ausgewaehlt. Software-Payload, Escaping,
Layout und Produktbuilds sind digitale Evidence; die reale Kamera-/Client-
Scannbarkeit auf dem Produktdisplay bleibt bis zur Hardwarepruefung offen.

Quellen: [ricmoo/QRCode](https://github.com/ricmoo/QRCode),
[Project Nayuki](https://github.com/nayuki/QR-Code-generator),
[LVGL QR-Widget v9.6.0](https://github.com/lvgl/lvgl/blob/v9.6.0/src/widgets/qrcode/lv_qrcode.c),
[gebuendelter Nayuki-Quelltext und Lizenz](https://github.com/lvgl/lvgl/blob/v9.6.0/src/libs/qrcode/qrcodegen.c).
Gepruefter LVGL-Paketstand: `9.6.0~1`, Component-Hash wie oben, 2026-09-25.

## Gemeinsame UI-Basis und Frameworkgrenze

Der OD-07-Teilentscheid zu #25 benoetigt keine Drittkomponente. Kleine
oberflaechenneutrale Praesentationsmodelle projizieren kanonische Fach-,
Prozess-, Sensorqualitaets-, Safety- und Berechtigungszustaende fuer Touch und
Web. Sie enthalten weder eine allumfassende Mega-View noch Navigation, Layout,
Pixelkoordinaten, HTML, CSS, Webrouten, Browserzustand oder Bibliothekstypen.
Insbesondere bleiben Display-/Touch-, LVGL-, Widget-, ArduinoJson- und
Webservertypen ausserhalb.

Gemeinsame Sprachressourcen verwenden stabile Textschluessel,
sprachunabhaengige Fehler-/Meldungscodes, vollstaendige Kataloge fuer Deutsch,
Spanisch und Englisch sowie Deutsch als Fallback. Semantische Formatierung
arbeitet erst an der Praesentationsgrenze auf typisierten Fachwerten.
Display- und Browsersprache duerfen unabhaengig gewaehlt werden;
oberflaechenspezifische Auswahl, Zeilenumbrueche, Kuerzungen, Schriftgroessen
und Pixelpassung sind keine Verantwortung von #25.

Touchnavigation und konkrete Screens bleiben in #26, Webnavigation und
responsive Layouts in #27, Display-/Touchadapter in #31. Diese Aufteilung
waehlt weder einen Treiber noch LVGL oder eine Uebersetzungsbibliothek aus.

Der OD-07-Teilentscheid schneidet #26 spaeter in Navigation/Interaktion,
Standby/Programmauswahl/Start, Programmverwaltung/Editor,
Lauf/Meldungen/Stop/Wiederanlauf und Einstellungen/Diagnose/Service/Recovery-UI.
Diese Screen-, Dialog-, Aktions- und Fehlerlogik ist projektspezifisch und wird
zuerst nativ mit simulierten Praesentationsmodellen und Touchereignissen
geprueft. Sie leitet weder Fach-, Sensorrollen-, Safety-, Auth-, Persistenz-
noch Aktorentscheidungen neu her. Ein allgemeines Widget-, Layout-, Screen-
oder UI-Pluginframework ist dafuer nicht erforderlich.

Reale Display-/Touchtechnik folgt separat #31 und OD-02; erst danach vergleicht
OD-05 schlanke Views und LVGL anhand desselben repraesentativen Screens. Reale
Authentisierung folgt OD-09 und die Resetmechanik dem #57-Vertrag. Der am Modul
vorhandene microSD-/SD-Karten-Slot ist weder R1-Komponente noch Speicherpfad:
keine Bibliothek, kein Adapter, kein Port, keine UI und kein Spike werden dafuer
vorgesehen.

## UI-Framework

LVGL ist kein Display- oder Touchtreiberkandidat. Der Treibervergleich wird
zuerst mit Stufe 4 abgeschlossen. Danach werden der schmale Adaptervertrag und
ein repraesentativer Release-1-Screen auf dem ausgewaehlten Treiber erstellt;
erst dann werden schlanke eigene Views und LVGL auf derselben Hardware mit
demselben Treiber, Screen, Texten, Eingabeelementen und derselben Messmethode
verglichen.

| Kandidat | Stand/Lizenz | Eignung | Ressourcen/Risiken | Empfehlung |
|---|---|---|---|---|
| LVGL | `9.6.0~1`, Component-Hash `7d82410747bbfb319531449749e940f440f8414f041f9eaba13f0d1dd7f58cdd`, MIT | vollstaendiges Widget-, Layout-, Event- und Renderingframework; kein Display-/Touchtreiber | zusaetzliche Displaypuffer, Fonts, Widgetzustand und Integrationskomplexitaet; Gesamtsystemressourcen bleiben Hardware-Evidence | bestehender produktiver Stack wird in #164 fuer den eingebauten WLAN-QR wiederverwendet; dies ist keine neue allgemeine QR-Plattform oder neue LVGL-Einfuehrung durch #164 |
| schlanke projektspezifische Views auf gewaehltem Treiber | keine Drittkomponente fuer Widgets | passt zu wenigen festen 320-x-240-Screens und bestehenden View-Modellen | mehr eigene Layout-/Fokuslogik, aber enger kontrollierbarer Umfang | Vergleichsbasis erst nach Treiberwahl und Adaptervertrag; bevorzugt, solange LVGL keinen belegten Vorteil bringt |

Die gemeinsamen #25-Praesentationsmodelle und die hardwareunabhaengige
#26-Navigations-/Screenlogik bleiben frameworkfrei. Die fruehere generische
Framework-Evaluation waehlt keine zusaetzliche Frameworkabstraktion fuer diese
Modelle aus; der bereits integrierte Produkt-Rendererpfad bleibt die konkrete
LVGL-/`esp_lvgl_port`-Nutzung. Issue #164 fuegt nur den QR-Widgetaufruf zu
diesem bestehenden Pfad hinzu.

Quelle: [LVGL v9.6.0](https://github.com/lvgl/lvgl/tree/v9.6.0), gepruefter
Component-Manager-Paketstand `9.6.0~1` am 2026-09-25.

## Authentisierung und Plattformschutz

OD-09 legt die fachliche Policy fest, nicht die endgueltige technische Auswahl.
Webpasswort und vierstellige Service-PIN verwenden getrennte, gesalzene,
einseitige Pruefnachweise; eine schnelle Einzel-SHA-256-Pruefung ist
unzulaessig. Sessions, CSRF-Tokens und Servicefreigaben bleiben fluechtig und
begrenzt. Credentialwechsel sind atomar und vorwaertsgerichtet; #57 erzeugt
keine leeren Authstrukturen. ADR-017 schliesst dauerhafte Anmeldung in R1 aus.
Ein bewusst deaktiviertes normales Webpasswort beseitigt nur die Passwort-KDF:
eine begrenzte anonyme lokale Session mit zufaelliger Kennung, Cookie, CSRF,
Revision/Konfliktschutz und sichtbarer Warnung bleibt erforderlich.

| Kandidat oder Pfad | Gepruefter Stand | Aufgabe | Status | Verbindlicher Nachweis |
|---|---|---|---|---|
| PBKDF2-HMAC-SHA-256 aus der fixierten mbedTLS-/ESP32-Toolchain | Bestandteil der fixierten Toolchain; konkrete Funktion, Version, Lizenz-/Noticeumfang und Build im Spike pruefen | langsamer KDF-Pfad fuer getrennte Passwort- und PIN-Verifier | `FIRST_EVALUATION_CANDIDATE`, `SPIKE_REQUIRED`, `FINAL_SELECTION_PENDING` | bekannte Testvektoren, mindestens 128-Bit-Salt und 256-Bit-Verifier mit Parameterkennung, konstanter Vergleich, Laufzeit, Stack, Heap, Jitter, Watchdog und parallele Anfragen; Iterationszahl erst danach |
| `esp_fill_random()` oder korrekt gesaeter mbedTLS-DRBG | fixierter ESP32-/mbedTLS-Pfad, konkrete Integration offen | kryptografischer Zufall fuer Salts, Sessionkennungen und CSRF-Tokens | `FIRST_EVALUATION_DIRECTION`, `SPIKE_REQUIRED` | Initialisierung/Fehlerpfad, wiederholte Bildung und Plausibilitaetspruefung; keine schwachen Ersatzwerte und keine Entropiebehauptung ueber die Plattformgarantie hinaus |
| NVS-/Flashverschluesselung | nicht aktiviert oder projektbezogen getestet | moeglicher Schutz wiederverwendbarer Secrets gegen physischen Flashzugriff | `EVALUATE_BEFORE_RELEASE`; zwingendes ergebnisoffenes Security-Gate vor #37 | Toolchain, Boot, Partitionierung, Provisionierung, Schluesselentstehung/-speicherung/-verlust, Entwicklungs-/Produktionsflash, Recovery, Werksreset, UART-Neuflash, Update/Migration, Ressourcen und Stabilitaet; danach vor #37 expliziter Ownerentscheid fuer produktive Auswahl samt Provisionierungs-/Recovery-/Regressionstest oder begruendete Nichtauswahl mit Rest-Risiken und Schutzgrenzen |

Der KDF-Spike prueft ausserdem richtige/falsche Passwort- und PIN-Pruefungen,
globale Fehlversuchsserien samt atomar neustartfestem Vor-Sperr-Zaehler,
Sperrstufe, aktivem Sperrzustand, Credential-Epoche/-Generation und Integritaet,
Credentialwechsel an Cut-Points, normale und anonyme Sessions sowie Widerruf.
Der neue Fehlerzustand wird vor der Antwort persistiert und validiert; Fehler
bleiben `fail closed`, waehrend aktiver Sperre gibt es weder KDF noch einen
Write je Ablehnung. Ein Pepper im selben ungeschuetzten Flash ist
keine Schutzgrenze. Ohne aktivierte und getestete Plattformverschluesselung wird
kein Schutz gegen physischen Flashzugriff behauptet. Es wird keine zusaetzliche
Kryptobibliothek, allgemeine Authplattform oder endgueltige Kryptokonfiguration
gewaehlt.

Der Plattformverschluesselungs-Spike und sein dokumentierter Ownerentscheid
sind zwingend vor #37 abzuschliessen. Eine Auswahl verlangt die produktive
Umsetzung samt Provisionierungs-, Recovery- und Regressionstests; eine
Nichtauswahl verlangt dokumentierte Rest-Risiken, klare Schutzgrenzen und
ausdrueckliche Ownerfreigabe. Diese Evaluation nimmt keines der Ergebnisse
vorweg.

Produktive Webmutationen bleiben ueber den Policyentscheid hinaus gesperrt,
bis die je Modus erforderlichen KDF-/Work-Factor-, Zufalls-, Credential-,
Sperr-, Session-/Cookie-, CSRF-/HTTP-, Revisions-/Wiederholungs-, Ressourcen-,
Jitter-, Watchdog-, Abbruch-, Neustart-, Webserver- und JSON-Nachweise
ownerfreigegeben sind. Servicefunktionen benoetigen zusaetzlich PIN-KDF,
PIN-Sperre, sitzungsgebundene Servicefreigabe und Safety-/Hardwaregates.

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
  Trennungs-/Wiederkehrfaellen getestet.
- Keine Web-/JSON-/UI-Bibliothek besitzt einen Base-/Head-Ressourcennachweis fuer
  die fertige Firmware.
- NVS-Kapazitaet, reale Flashatomizitaet und Lebensdauer sind nicht gemessen.
- Es wird keine reale Heapreserve, PSRAM, GPIO-Belegung oder aktive Polaritaet
  behauptet.

### Issue #27 – owner-ausgewaehlter cJSON-Codec implementiert (2026-09-30)

Plan `aa695b43f5b68edefea23669678d877f5b830c17` wurde fuer die finale
Bibliotheksauswahl ownerfreigegeben. Ausgewaehlt ist ausschliesslich
Espressif Registry `espressif/cjson 1.7.19~2`, Komponentencommit
`1387cec28a9b40654be7892114bd7d26fcd3869c`, Upstream cJSON Commit
`b2890c8d76bbb64e710585ebc0a917196b9c67e7`, Component-Hash
`e788323270d90738662d66fffa910bfe1fba019bba087f01557e70c40485b469`.
Die MIT-Lizenzdatei hat SHA-256
`a36dda207c36db5818729c54e7ad4e8b0c6fba847491ba64f372c1a2037b6d5c`.
ArduinoJson ist aus Manifest, Lockfile, CMake und PlatformIO entfernt.

- `web_json_codec.cpp` verwendet cJSON nur an der konkreten Codecgrenze;
  CMake deklariert `cjson` als private Dependency. `uint64`-Revisionen bleiben
  kanonische bounded Dezimalstrings; Zahlen, Pflichtfelder und geschlossene
  Schemas werden projektspezifisch validiert. Der 480-Byte-Bodybound wird vor
  NUL-Scan und Parse erzwungen; Responses sind auf 3072 Byte begrenzt.
- Native Regressionen nach Implementierung: Web-Application-Routes 16/16,
  Web-Session 16/16, Browser-Policy 4/4, HTTP-Lifecycle 5/5, lokale Touch-UI
  14/14, UI-Commands 10/10, Run-Identity 18/18 und
  Run-Persistence-Coordinator 163/163. Die geaenderte Route-/Codec-Suite
  bestand nach clang-format-21 nochmals 16/16.
- ESP-IDF `esp32_bringup` und `esp32_release` sowie abschliessender
  PlatformIO-Profilvertrag: PASS. Board-Profile-Single-Source und
  Builder-Static-Analysis-Self-Check unter clang-format/tidy 21: PASS;
  `git diff --check`: PASS.
- Read-only-Antwortgroessen im gemessenen nativen Produktpfad: Status 345,
  Temperaturen 247 und Alerts 3048 Byte, jeweils innerhalb 3072 Byte.
  Dies ist kein integrierter Vier-Session-/no-PSRAM-Ressourcennachweis.
- `GET /api/v1/*` und der interne Run-Handler bleiben unregistriert und
  unkomponiert; kein `main/app_main.cpp`-Delta. Die Auth-/Session-Composition,
  Application-Aufrufserialisierung und das Vier-Session-/no-PSRAM-Gate bleiben
  vor produktiver Webmutation offen. Hardware und Flash: `NOT_RUN`;
  `ACTUATOR_RELEASE=NO`. Ergebnis: softwareseitiger Slice abgeschlossen,
  STOP fuer Independent Slice Review.
