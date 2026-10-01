# Plan – Issue #164: lokale Touch-Netzwerkbedienung vervollständigen

## Konsolidierte Planrevision – QR-/SoftAP-UX und begrenzter WLAN-Scan

```text
ISSUE=164
PR=171
PLAN_REVISION=CONSOLIDATED_QR_SOFTAP_SCAN_PLAN_B1_B2_FIX_2026-09-30
BASE_HEAD=b24f39fffffaf8fd4db21e94d1625ecca2660e5e
PLAN_FIX_BASE_HEAD=e5d64fa1923863406863db1dc4f29564201037c3
PR_STATE=OPEN_DRAFT
WORKTREE=/tmp/issue164-network-ui.M4vluy
BRANCH=agent/issue-164-network-ui-completion
PLAN_HEAD=1fd1f307f3ae1eb0ed31d7a62f082c0d7c487413
OWNER_PLAN_APPROVAL=PASS
LVGL_PIN=9.6.0~1_UNCHANGED
SOFTAP_SSID=Fermentation
SOFTAP_PASSWORD_LENGTH=16
SOFTAP_PASSWORD_ALPHABET_SIZE=42
SOFTAP_PASSWORD_ENTROPY_TARGET_BITS=86.27
SOFTAP_PASSWORD_RANDOM_SOURCE=EXISTING_SECURE_RANDOM_SOURCE
SOFTAP_PASSWORD_DERIVATION=NONE
SOFTAP_AUTH=WPA2_PSK_UNCHANGED
QR_PURPOSE=JOIN_SOFTAP
QR_PAYLOAD=SSID_AND_PASSWORD_ONLY
QR_QUIET_ZONE=REQUIRED
QR_VERSION=4
QR_MODULES_WITH_QUIET_ZONE=41
QR_MODULE_SCALE=4
QR_SIZE=164X164_EXACT
QR_RECT_X=156
QR_RECT_Y=34
PAGE_TITLE_RECT={8,34,140,18}
QR_CONTRAST=BLACK_ON_WHITE
QR_INTERPOLATION=NO
QR_ANTIALIASING=NO
NETWORK_SCAN_RESULT_BOUND=16
NETWORK_SCAN_HTTP_BOUND=16
NETWORK_SCAN_HTTP_MAX_BYTES=528
FTDI_DISPLAY_COUPLING=REPRODUCED
FIRMWARE_CRASH=NOT_PROVEN
PRODUCT_CODE_CHANGED=YES
BUILD=ESP32_BRINGUP_AND_RELEASE_PASS_ON_IMPLEMENTATION_HEAD
FLASH=NOT_RUN_OWNER_FULL_IMPLEMENTATION_REVIEW_GATE
IMPLEMENTATION_ALLOWED=OWNER_APPROVED
ACTUATOR_RELEASE=NO
```

Diese konsolidierte Revision ist der owner-freigegebene Vertrag für die
Umsetzung im bestehenden PR #171 und ersetzt die bisherige offene
Completion-Plan-Fassung. Der bestehende LVGL-Stack bleibt unverändert:
Im Checkout ist `lvgl/lvgl` auf `9.6.0~1` gepinnt. Es gibt keinen Downgrade auf
9.5, kein Upgrade, keine neue QR-/ECC-Bibliothek und keine eigene
QR-Implementierung. Die bestehende LVGL-QR-Komponente mit ECC Medium bleibt
der einzige QR-Pfad.

Die Planfreigabe für `PLAN_HEAD` liegt vor. Die Umsetzung, gezielten
Regressionstests, der Builder Self-Check sowie die beiden ESP-IDF-Profilbuilds
werden im aktuellen Status unten nachgewiesen; ein Gerät wurde nicht
geflasht. Historische Evidence und frühere Hardwareberichte werden nicht
rückwirkend umgeschrieben. PR #171 bleibt Draft; es gibt keinen neuen Branch
oder PR, keinen Merge, keinen Ready-Wechsel, keine Issue-Schließung und keine
Aktuatorfreigabe.

### 1. Bereits bestätigte Korrekturen erhalten

Die Umsetzung baut auf dem exakten `BASE_HEAD` auf und erhält ohne
Neuentwurf:

- die verkürzten Presentation-/UI-Lifetimes vor Owning-Mutationen;
- die Zerstörung von `RepresentativeScreen` vor `dispatchWorkspacePress()`;
- den HeaderNetwork-Pfad ohne unnötig lebenden vollständigen `ProgramCatalog`;
- die Vorreservierung der maximal 21 `ScreenDrawCommand`-Einträge;
- das Verschieben der QR-Payload beim Einfügen statt einer zusätzlichen
  Stringkopie;
- den lokalisierten Auswahlhinweis für den internen Zustand `UNSELECTED`.

Es werden keine neue Graph-, Persistence-, Configuration-, Codec- oder
Network-Ownership-Architektur, keine zweite State-Machine und kein neuer
Netzwerk-Task eingeführt.

## Aktueller Korrekturvertrag nach Owner-Plan `3cedb448`

Der neuere, unabhängig verifizierte Korrekturplan
`docs/tasks/issue-164-softap-credential-persistence-and-device-name-ssid-correction-plan.md`
ist für die Umsetzung maßgeblich und überschreibt die widersprechenden
früheren aktiven SoftAP-Aussagen dieses Dokuments. Historische Plan- und
Hardware-Evidence weiter unten bleibt unverändert.

Aktiv gilt damit:

- Die SoftAP-SSID wird ausschließlich aus dem aktuellen
  `UserConfiguration.deviceName` abgeleitet, am vollständigen UTF-8-Codepoint-
  Ende auf höchstens 24 rohe beziehungsweise 28 QR-escaped Bytes begrenzt und
  erhält keinen Suffix, Hash, MAC-, Zeit- oder Zufallsanteil.
- Das SoftAP-Passwort hat exakt 16 Zeichen aus dem festgelegten 42er Alphabet,
  wird mit der bestehenden sicheren Zufallsquelle und Rejection Sampling
  erzeugt und pro `StorageEpoch` in `cc0`/RecordTypeId 9 persistiert. Schema 1
  bleibt lesbar und wird nach Schema 2 migriert; Schema 2 enthält das
  SoftAP-Passwort ohne Present-Tag und schreibt es zwingend.
- V1/V2 haben die Maxima 100/145 beziehungsweise 118/163 Byte. V2 ohne
  gültiges 16-stelliges SoftAP-Passwort ist ungültig; der aktuelle Writer
  schreibt ausschließlich Schema 2.
- `HOME_WIFI`-Verfügbarkeit wird ausschließlich aus
  `currentCredential.homeWifi.has_value()` entschieden. `AP_ONLY` mit einem
  SoftAP-only-V2 bleibt AP-only; HOME_WIFI ohne Home-Credentials bleibt Setup.
- Boot, `applyNetworkMode()` und `beginHomeWifiReconfiguration()` verwenden
  den jeweils aktuellen Device-Name; ein Neustart ändert die SSID, nicht das
  persistierte SoftAP-Passwort. Netzwerkfehler blockieren den Fermentations-
  und Safety-Kern nicht und starten keinen globalen Service-Required-Pfad.
- Die QR-Geometrie bleibt `{156,34,164,164}` mit Quiet-Zone und bestehendem
  LVGL-9.6.0~1-Pfad. Scanlimit, Heap-Vector-Pfad, 16/528-Byte-HTTP-Grenze,
  `503` bei Fehler und leerer `200`-Antwort bei Empty-Scan bleiben bestehen;
  `ROOT_CAUSE=UNPROVEN` wird nicht geändert.

### 2. SoftAP-Zugangsdaten

`makeNetworkConfig()` setzt die SoftAP-SSID ohne MAC-Lesen und ohne Suffix
exakt auf `Fermentation`. Die WPA2-PSK-Konfiguration im ESP-IDF-Adapter bleibt
unverändert.

Das Passwort wird bei jedem App-Boot ausschließlich über die bestehende
`ISecureRandomSource`-Schnittstelle erzeugt. Der bestehende
`EspIdfSecureRandomSource`/`esp_fill_random()`-Pfad wird wiederverwendet; es
werden keine neue Random-Abstraktion und keine Ableitung aus MAC, Geräte-ID,
Zeit oder sonstigen vorhersagbaren Werten eingeführt. Das Passwort hat exakt
16 Zeichen.

Als einheitliches, sicht- und QR-freundliches Alphabet wird für die Umsetzung
folgende ASCII-Menge festgelegt:

```text
ACDEFHJKMNPQRTUVWXYacdefhjkmnpqrtuvwxy3479
```

Sie enthält 42 Symbole und lässt die leicht verwechselbaren Gruppen `0/O`,
`1/I/l`, `2/Z`, `5/S`, `6/G`, `8/B` sowie `i/l/o` aus. Die Auswahl erfolgt
gleichverteilt mit Rejection Sampling, damit kein Modulo-Bias entsteht. Bei
16 unabhängigen Zeichen beträgt der Zielraum
`16 * log2(42) = 86.27` Bit und liegt damit über dem Owner-Ziel von ca. 80
Bit. Bei einem Fehler der Zufallsquelle wird kein vorhersehbarer Ersatzwert
gebildet; der bestehende fail-closed Startup-/Konfigurationspfad bleibt
maßgeblich.

Das Passwort bleibt ausschließlich in der flüchtigen SoftAP-Konfiguration
und der lokalen `networkAccessPointInfo()`-Projektion für die Display-/QR-
Darstellung. Es wird nicht geloggt, nicht über HTTP/API oder Diagnose
ausgegeben und nicht in Persistenz kopiert. SSID, Passwort und direkte IP
bleiben manuell sichtbar.

### 3. QR-Layout und produktiver LVGL-Pfad

Die vorhandene WLAN-Payload bleibt unverändert semantisch:
`WIFI:T:WPA;S:<escaped-SSID>;P:<escaped-password>;;`. Sie enthält exakt die
aktuellen SoftAP-Werte, keine URL und keine IP.

Der Renderer verwendet für den neuen festen Payload und ECC Medium QR-Version
4. Die 33 QR-Module je Seite erhalten eine Quiet-Zone von 4 Modulen je Seite;
damit sind 41 Module je Seite vorhanden. Bei exakt 4 px je Modul ergibt sich
eine `164x164`-Command-Fläche. Der bisherige `184x184`-Vorschlag ist damit
verworfen.

Die konkrete Netzwerkseiten-Geometrie lautet:

```text
DISPLAY_BOUNDS             = {0, 0, 320, 240}
HEADER_BOUNDS              = {0, 0, 320, 32}
QR_RECT                    = {156, 34, 164, 164}
PAGE_TITLE_RECT            = {8, 34, 140, 18}
CURRENT_MODE_RECT          = {8, 52, 140, 18}
SSID_RECT                  = {8, 72, 140, 36}
PASSWORD_RECT              = {8, 110, 140, 54}
IP_RECT                    = {8, 166, 140, 18}
BOTTOM_CONTROLS_BOUNDS     = {0, 200, 320, 40}
```

Die rechte QR-Fläche liegt damit vollständig zwischen Header und Bottom-
Controls (`y=34..197`). Current-Mode-Text, SSID, Passwort und IP liegen in
der linken Spalte (`x=8..147`) und überlappen den QR nicht. Der bestehende
Header-Touchbereich und die bestehenden Bottom-Control-Rechtecke bleiben
unverändert; es entsteht keine neue Layoutarchitektur.

Im produktiven LVGL-Zweig werden für jedes QR-Objekt explizit gesetzt:

```cpp
lv_qrcode_set_size(qrCode, <gepruefte_ganzzahlige_qr_groesse>);
lv_qrcode_set_quiet_zone(qrCode, true);
lv_qrcode_set_dark_color(qrCode, lv_color_black());
lv_qrcode_set_light_color(qrCode, lv_color_white());
```

Die vorhandene I1-/LVGL-Darstellung bleibt ohne Interpolation und ohne
Antialiasing. Es wird kein Bild-Scaling außerhalb des LVGL-QR-Widgets
eingeführt. Die Aufrufreihenfolge stellt sicher, dass Quiet-Zone, Größe und
Payload vor der gültigen QR-Renderprüfung wirksam sind.

### 4. WLAN-Scan begrenzen

Der ESP-IDF-Scan erhält einen gemeinsamen, kleinen R1-Grenzwert von **16
AP-Einträgen**. Das genügt für die Auswahl eines Heimnetzes und begrenzt bei
dem bestehenden ESP32-Heap die `wifi_ap_record_t`-Arbeitsmenge auf ungefähr
16 Datensätze statt auf die unbeschränkte vom Treiber gemeldete Anzahl. Die
Grenze wird als geteilter Netzwerkvertrag definiert, damit Adapter und
HTTP-Route nicht auseinanderlaufen.

Die Umsetzung liest die vollständige Treiberanzahl nur als Zählwert und klemmt
vor jeder Result-Allokation auf `min(driver_count, 16)`. Der bestehende
Heap-Vector-Pfad wird wiederverwendet: `wifi_ap_record_t`-Records werden in
einem Heap-`std::vector` mit genau dieser begrenzten Größe gelesen; es wird
kein neuer großer lokaler Stackpuffer (`std::array`, C-Array oder äquivalent)
im synchronen HTTP-/Scanpfad eingeführt. Die anschließende
`NetworkScanEntry`-Liste wird ebenfalls höchstens 16 Einträge groß. Eine
zweite ungebundene Record-Liste wird nicht aufgebaut. Die SSID-Längenbegrenzung
von 32 Bytes bleibt bestehen. Eine zusätzliche Sortier- oder Worker-
Architektur wird nicht eingeführt; es werden die ersten vom ESP-IDF-Scan
gelieferten begrenzten Ergebnisse übernommen.

Die HTTP-Route wendet dieselbe Grenze defensiv nochmals an und reserviert für
die Antwort höchstens `16 * (32 + 1) = 528` Bytes für SSID-Zeilen. Damit können weder
Adapter noch Test-/Mockdaten mehr als 16 Einträge in die Browserantwort
durchreichen. Fehler-Scan und leerer Scan bleiben deterministisch getrennt:
Fehler liefern weiterhin `503 scan unavailable`, ein erfolgreicher leerer
Scan eine leere `200`-Antwort.

Die Owner-Beobachtung eines Ausfalls beim Browser-`Scan` bleibt
`ROOT_CAUSE=UNPROVEN`. Der bisherige ungebundene Heap-Pfad wird als realer
Defekt behoben, aber nicht nachträglich als bewiesene Ursache des konkreten
Hardwarefehlers behauptet.

### 5. Dokumentationssynchronisation in der Umsetzung

Nach Planfreigabe synchronisiert der Implementierungsdiff mindestens:

- `docs/REQUIREMENTS.md`;
- `docs/NETWORK.md`;
- dieses Task-/Plan-Dokument.

Aktive R1-Aussagen werden auf feste SSID `Fermentation`, individuelles
flüchtiges 16-Zeichen-Passwort, 42er Alphabet, mindestens 80 Bit Zielraum,
`QR_QUIET_ZONE=REQUIRED`, die feste `164x164`-Geometrie mit 41 Modulen und
4 px je Modul sowie den 16er-Scan-Grenzwert gebracht. Veraltete aktive
Aussagen zu individueller SSID, 32 Hexzeichen, 112x112 oder 184x184 werden
korrigiert. Historische Evidence,
frühere Messwerte und alte HEAD-/Flashberichte bleiben unverändert und werden
als historisch kenntlich gehalten. Weitere aktive Vertragsstellen werden nur
nach `git grep` auf dieselbe Weise synchronisiert; Secrets werden nicht in
Dokumentation, Logs oder Testausgaben eingetragen.

### 6. Gezielte Regressionen nach Planfreigabe

Die Tests werden auf den neuen Contract begrenzt und decken mindestens ab:

- SoftAP-SSID exakt `Fermentation`;
- Passwort exakt 16 Zeichen;
- jedes Zeichen liegt im definierten 42er Alphabet;
- Alphabetgröße und berechneter Zielraum erfüllen mindestens ca. 80 Bit;
- zwei kontrolliert unterschiedliche Eingabesequenzen der bestehenden
  Zufallsquelle ergeben unterschiedliche Passwörter;
- Zufallsquellenfehler erzeugt keinen deterministischen Ersatzwert;
- QR-Payload enthält exakt aktuelle SSID und aktuelles Passwort;
- QR-Payload enthält weder URL noch IP;
- QR-Draw-Command ist exakt `{156,34,164,164}` mit 41 Modulen und bleibt in
  320x240;
- Header-, Page-Title-, Current-Mode-, SSID-, Passwort-, IP- und
  Bottom-Control-Rechtecke
  bleiben innerhalb der Bounds; QR und manuelle Informationsrechtecke sowie
  QR und Bottom-Controls überlappen nicht;
- manuelle SSID-/Passwort-/IP-Commands bleiben sichtbar;
- der produktive LVGL-Pfad aktiviert die Quiet-Zone und Schwarz/Weiß-
  Konfiguration;
- Scanresultate oberhalb des Limits werden bereits vor großen Heap-Vector-
  Allokationen und nochmals in der HTTP-Antwort auf 16 begrenzt;
- kein neuer großer lokaler Stackpuffer im synchronen HTTP-/Scanpfad;
- Fehler- und Empty-Scan bleiben deterministisch;
- bestehende direkt betroffene Netzwerk-, Renderer- und Touch-Regressionen
  bleiben grün.

Gezielt zu ermitteln sind die konkreten Testdateien beim Implementierungsstart;
voraussichtlich berührt werden die bestehenden Renderer-/Netzwerk-Tests und
ein Adapter-/Route-Test für den begrenzten Scan. Unveränderte, nicht direkt
betroffene Tests werden nicht unnötig erneut ausgeführt.

### 7. Hardware-Gate nach Implementation

Erst nach Ownerfreigabe dieser exakten Plan-Commit-SHA und nach Umsetzung,
Independent Fix Verification sowie Builder Self-Check folgen die nativen
Regressionen, beide `esp32_bringup`-/`esp32_release`-Builds und der
autorisierte exakte `esp32_release`-Flash. Es werden ausschließlich die
notwendigen drei Firmware-Images geschrieben; kein `erase-all`, kein Löschen
oder Reparieren von NVS/state_store und kein Rebuild während des Flashens.

Der UART-Capture wird vor dem Beginn des interaktiven Tests einmal geöffnet
und danach kontinuierlich offen gehalten. Es gibt keinen zusätzlichen
Port-Open mitten im Test. Source-SHA, Profil, `application: ready`, LCD-/LVGL-
Initialisierung, Heartbeats sowie Panic, Abort, Watchdog, Brownout und
unerwartete Resetmarker werden aus dem Rohmitschnitt geprüft. Die bestehende
FTDI-/Display-Kopplung bleibt getrennt zu bewerten:

```text
FTDI_DISPLAY_COUPLING=REPRODUCED
FIRMWARE_CRASH=NOT_PROVEN
```

Wenn QR oder Browser-Scan Abort, Panic oder Reset auslösen, werden der exakte
UART-Ausschnitt, Resetursache und symbolisierte PC/Backtrace gesichert und
der Lauf anschließend beendet. Wenn nur das Display schwarz wird, während
Heartbeats weiterlaufen und keine Resetmarker erscheinen, wird dies als
FTDI-/Display-Kopplungsbefund dokumentiert, nicht als Firmware-Crash.

Nach stabilem Boot werden ausschließlich mit Owner am Gerät AP_ONLY,
Netzwerkseite, QR-Anzeige, Smartphone-Scan innerhalb weniger Sekunden,
SoftAP-Join, DHCP, `192.168.4.1`, Browser-Scan mit Heap-/größtem-
8-Bit-Block-Messung vor und nach dem Browser-Scan sowie dem bestehenden
Task-Stack-HWM, sofern dieser im vorhandenen Ressourcenpfad zugänglich ist,
sowie HOME_WIFI-Test-before-Commit, Persistenz, Reboot und Reconnect
durchgeführt. Nicht tatsächlich ausgeführte Felder bleiben
`NOT_RUN`. Bei schwarzem Display oder einem nicht erklärten Hardwarefehler
gilt STOP ohne spekulative Folgeänderung.

### 8. Plan-Gate

```text
ISSUE=164
PR=171
BASE_HEAD=b24f39fffffaf8fd4db21e94d1625ecca2660e5e
PLAN_HEAD=1fd1f307f3ae1eb0ed31d7a62f082c0d7c487413
LVGL_PIN=9.6.0~1_UNCHANGED
SOFTAP_SSID=Fermentation
SOFTAP_PASSWORD_LENGTH=16
SOFTAP_ENTROPY_TARGET_BITS>=80
QR_QUIET_ZONE=REQUIRED
QR_VERSION=4
QR_MODULES_WITH_QUIET_ZONE=41
QR_MODULE_SCALE=4
QR_SIZE=164X164_EXACT
QR_RECT={156,34,164,164}
NETWORK_SCAN_RESULT_BOUND=16
NETWORK_SCAN_HTTP_MAX_BYTES=528
SCAN_LOCAL_STACK_BUFFER=FORBIDDEN
FTDI_BLACK_DISPLAY_FIRMWARE_CRASH=NOT_PROVEN
PRODUCT_CODE_CHANGED=YES
IMPLEMENTATION_HEAD=f39eabe1265a56215582e94d197b764d1ca2e0a0
BUILD=ESP32_BRINGUP_AND_RELEASE_PASS
BUILDER_SELF_CHECK=PASS
ESP_STATIC_ANALYSIS=FAILED_PRE_EXISTING_APP_MAIN_BASELINE_FINDINGS
FLASH=NOT_RUN_OWNER_FULL_IMPLEMENTATION_REVIEW_GATE
IMPLEMENTATION_ALLOWED=OWNER_APPROVED
PR_DRAFT=YES
MERGE=NO
READY=NO
ISSUE164_CLOSE=NO
ACTUATOR_RELEASE=NO
```

Die frühere Planphase stoppte für Owner Review der exakten Plan-Commit-SHA;
deren Freigabe liegt mit `OWNER_PLAN_APPROVAL=PASS` vor. Die Umsetzung im
bestehenden PR ist abgeschlossen und wartet nun auf den Owner Full
Implementation Review.

## Vorherige Planbasis und Provenienz

Der folgende Abschnitt bleibt als unveränderte Provenienz der vorherigen
Completion-Plan- und Hardware-Gates erhalten. Seine unqualifizierten
SoftAP-/QR-Aussagen werden für die nächste Umsetzung durch die obenstehende
konsolidierte Revision ersetzt; historische Evidence wird nicht rückwirkend
umgeschrieben.

## Planstatus und Provenienz

```text
ISSUE=164
BASE_BRANCH=main
BASE_SHA=b871375f494701bed1834013cfeb789856983e3a
PR165=MERGED
PR165_MERGE_COMMIT=1f1755e5e706fb668472920545b5302fcef1df16
PREVIOUS_COMPLETION_PLAN_SHA=744a1f29478c41e9ced42a77a0e6336c63bf8724
PREVIOUS_OWNER_APPROVED_PLAN_SHA=744a1f29478c41e9ced42a77a0e6336c63bf8724
PLAN_REVISION_BASE_HEAD=d65420fe6573411f3bfef1596fc09b5865404e52
PLAN_REVISION_REASON=INDEPENDENT_FIX_REVIEW_PHY_NVS_AND_RESOURCE_EVIDENCE
CURRENT_COMPLETION_SCOPE=LOCAL_NETWORK_TOUCH_PAGE_SOFTAP_QR_AND_HARDWARE_CORRECTION_EVIDENCE
OWNER_DECISION=VARIANT_B_QR_RETAINED
R1_LOCAL_NETWORK_MODE_SELECTION=YES
R1_LOCAL_SOFTAP_SSID_DISPLAY=YES
R1_LOCAL_SOFTAP_PASSWORD_DISPLAY=YES
R1_LOCAL_DIRECT_IP_DISPLAY=YES
R1_HOME_WIFI_BROWSER_SETUP=YES
R1_TOUCH_HOME_WIFI_CREDENTIAL_ENTRY=DEFERRED
R1_TOUCH_WIFI_KEYBOARD=DEFERRED
R1_WLAN_QR_TO_JOIN_SOFTAP=REQUIRED
R1_WEBSITE_QR=DEFERRED
PRIMARY_R1_HOME_WIFI_CREDENTIAL_INPUT=BROWSER_SETUP
ISSUE164_STATUS=OPEN
PR171_STATUS=OPEN_DRAFT
PLAN_HEAD=1fd1f307f3ae1eb0ed31d7a62f082c0d7c487413
PLAN_STATUS=OWNER_APPROVED_IMPLEMENTATION_COMPLETE_PENDING_FULL_IMPLEMENTATION_REVIEW
OWNER_APPROVAL_FOR_THIS_PLAN_REVISION=PASS
IMPLEMENTATION_AUTHORIZATION_FOR_FOLLOWUP=OWNER_APPROVED
REVIEWED_HEAD=d65420fe6573411f3bfef1596fc09b5865404e52
IMPLEMENTATION_HEAD=f39eabe1265a56215582e94d197b764d1ca2e0a0
LAST_PRODUCT_IMPLEMENTATION_HEAD=f39eabe1265a56215582e94d197b764d1ca2e0a0
FOLLOWUP_PRODUCT_CORRECTION=IMPLEMENTED_PENDING_FULL_IMPLEMENTATION_REVIEW
STACK_CORRECTION=24576_BYTES_BOTH_ESP32_PROFILES
WIFI_NVS_POLICY=DISABLED_VIA_WIFI_INIT_CONFIG_NVS_ENABLE_ZERO_AND_WIFI_STORAGE_RAM
DEFAULT_NVS_INIT=REQUIRED_FOR_ESP_IDF_PHY_CALIBRATION_SYSTEM_DATA_ONLY
PHY_CALIBRATION_NVS=ESP_IDF_DEFAULT_ENABLED_UNCHANGED
STATE_STORE=SOLE_CREDENTIAL_PERSISTENCE
WIFI_STORAGE=WIFI_STORAGE_RAM
NVS_ERASE_OR_REPAIR=NO
RESOURCE_LOG_FIELDS=FREE_HEAP_MINIMUM_FREE_HEAP_LARGEST_8BIT_BLOCK_MAIN_TASK_STACK_HWM
RESOURCE_LOG_POINTS=BEFORE_AFTER_NETWORK_MODE_CHANGE_AND_STABLE_AP_ONLY_HOME_WIFI
PRIOR_TARGETED_NATIVE_TESTS=PASS_ON_PREVIOUS_HEADS
PRIOR_BUILDER_SELF_CHECK=PASS_ON_PREVIOUS_HEADS
PRIOR_ESP32_PROFILE_BUILDS=PASS_ON_PREVIOUS_HEADS
CURRENT_REVISION_TESTS=PASS_TARGETED_NATIVE_NETWORK_RENDERER_TOUCH_SOFTAP
BUILDER_SELF_CHECK=PASS_ON_IMPLEMENTATION_HEAD
CURRENT_REVISION_BUILDS=PASS_ESP32_BRINGUP_AND_ESP32_RELEASE
ESP_STATIC_ANALYSIS=FAILED_PRE_EXISTING_APP_MAIN_BASELINE_FINDINGS
HARDWARE_TESTS=PARTIAL_BRINGUP_BOOT_ONLY_ON_HISTORICAL_HEAD_PRODUCT_TOUCH_NOT_VERIFIED
HARDWARE_CLIENT_EVIDENCE=NOT_RUN_PENDING_INTERACTIVE_OWNER_EVIDENCE
HARDWARE_REVIEW=NOT_RUN_FOR_THIS_IMPLEMENTATION
OPEN_HARDWARE_BLOCKERS=NOT_REVIEWED
HARDWARE_PROFILE=esp32_release
OWNER_AUTHORIZATION_RELEASE_FLASH_AND_AUTOMATED_BOOT_UART=GRANTED_AFTER_PLAN_APPROVAL_AND_IMPLEMENTATION_GATES
INTERACTIVE_HARDWARE_TESTS=OWNER_PRESENCE_REQUIRED_ELSE_NOT_RUN
ACTUATOR_RELEASE=NO
```

Diese vollständige Revision ersetzt die bisherige Completion-Plan-Fassung
für PR #171 und Issue #164. Sie übernimmt deren bestätigte R1-Abgrenzung und
QR-Vertrag und ergänzt die seitdem erforderlichen Touch-/Layout-, Main-Task-
Stack-, Wi-Fi-/PHY-NVS- und Ressourcen-Evidence-Korrekturen. Sie
ändert nicht rückwirkend den gemergten Implementierungsplan oder historische
Evidence. Vor Ownerfreigabe dieser exakten Planrevision erfolgen keine weitere
Produktcodeänderung und kein Flash. Diese Planrevision selbst führt keine
Tests oder Builds aus. Die Ownerautorisierung umfasst nach Freigabe dieser
exakten Plan-SHA und Abschluss der Umsetzungs-/Review-Gates den
`esp32_release`-Flash sowie automatisierbare Flash-, Boot- und UART-Evidence,
auch wenn der Owner bei diesen automatisierten Schritten nicht vor Ort ist.
Sie autorisiert keine interaktiven Touch-, QR-, Client-, Netzwerk- oder
Reconnect-Ergebnisse ohne tatsächliche Ausführung.

Die Ownerentscheidung präzisiert `VARIANT_B`: Die lokale Eingabe von
HOME_WIFI-SSID und -Passwort am Touchdisplay sowie die dafür erforderliche
Bildschirmtastatur werden aus R1/#164 deferiert. Der WLAN-QR zum Beitritt in
den geschützten Setup-/AP-only-SoftAP mit individuellen SoftAP-Zugangsdaten
bleibt dagegen R1/#164-Pflicht. `HOME_WIFI` selbst, die Display-Moduswahl, die
lokale Anzeige der individuellen SoftAP-SSID/-Zugangsdaten und direkten IP
sowie der browserbasierte Setup-/Test-before-Commit-Pfad bleiben R1/#164. Ein
separater QR zum Öffnen der Webseite bleibt Future Scope. Der Browser ist der
einzige R1-Eingabepfad für Heim-WLAN-Credentials; der WLAN-QR erzeugt keine
zweite Credential-, UI- oder Persistenzwahrheit.

## 1. Verifizierte Ausgangslage

- `origin/main` steht auf `b871375f494701bed1834013cfeb789856983e3a`.
- PR #165 ist auf `main` unter Merge-Commit
  `1f1755e5e706fb668472920545b5302fcef1df16` integriert.
- Issue #164 bleibt offen. Seine Live-Beschreibung hält fest, dass die
  rendererunabhängigen Netzwerkverträge und der HTTP-Unterbau vorhanden sind,
  der physische Modus-Einstieg aber bislang als deferred Rendererpfad offen
  blieb. Reale WLAN-/Client-Nachweise sind nicht erbracht und bleiben separat.
- Issue #31 / PR #156 ist abgeschlossen und auf dem aktuellen `main`; dieser
  Folgeplan ändert weder dessen Touchkalibrierungs- noch Displaygeometrie-
  Vertrag.
- Der Auftrag weist die begrenzte Touch-Erreichbarkeit der bestehenden
  #164-Netzwerkbedienung jetzt ausdrücklich Issue #164 zu. Das ist eine
  Scope-Klarstellung gegenüber der bisherigen #164-Plan-/Issue-Formulierung,
  nach der physische Touchinteraktion vollständig Issue #31 zugeordnet war.
- Der geprüfte PR-Branch-HEAD ist
  `d65420fe6573411f3bfef1596fc09b5865404e52`. Die jüngsten Korrektur-Commits
  erweitern den 320x240-WLAN-Header-Hit-Test, setzen den gemeinsamen
  ESP-Main-Task-Stack auf 24 KiB und konfigurieren Wi-Fi mit
  `nvs_enable=0`; am geprüften HEAD wurde die Default-NVS-Initialisierung
  entfernt. Gezielte native Tests, Builder-Self-Check und beide
  ESP-IDF-Profilbuilds sind auf dem vorherigen Implementierungs-HEAD laut
  Live-PR PASS; diese digitale Evidence ersetzt nicht die jetzt geforderte
  reale Touch-/Mode-Evidence.
- Die Hardwarebeobachtung war ein kurz weißer und danach wieder auf Home
  stehender Bildschirm bei `AP_ONLY`/`HOME_WIFI`; der Owner berichtete
  zusätzlich einen schwarzen Bildschirm nach `AP_ONLY`. Die Diagnose ordnet
  die synchrone Netzwerkmodus-/NVS-Arbeit dem Main-Task zu: dessen bisherige
  16 KiB reichen dafür nicht aus. Der Stack wurde auf 24 KiB für beide
  ESP32-Profile erhöht. Diese Korrektur ist digital gebaut, aber auf dem
  Produkt-Touchpfad noch nicht real bestätigt.
- Beim letzten historischen `esp32_bringup`-Boot auf HEAD
  `8ead3dd871af120abe78d98bb8b6fde8356f9a99` wurde Default-NVS initialisiert
  und der SoftAP gestartet, bevor der separate Issue-29-Probe seine
  98.304-Byte-Task nicht erzeugen konnte und vor Displayinitialisierung
  endete. Issue #29 ist für diese #164-Evidence ausdrücklich out of scope;
  die Ownerentscheidung bestimmt `esp32_release` als Hardwareprofil. Seit
  dieser Profilentscheidung gab es keinen weiteren Flash.
- Der Wi-Fi-Adapter setzt auf dem aktuellen HEAD bereits
  `wifi_init_config_t.nvs_enable=0` und `WIFI_STORAGE_RAM`; damit entsteht
  keine zweite persistente Wi-Fi-Konfigurationswahrheit. Davon getrennt ist
  die ESP-IDF-PHY-Kalibrierung: In ESP-IDF v6.1 ist
  `CONFIG_ESP_PHY_CALIBRATION_AND_DATA_STORAGE` standardmäßig aktiviert. Der
  PHY-Code liest und schreibt RF-Kalibrierdaten im Default-NVS-Namespace
  `phy` und verlangt initialisiertes NVS vor Wi-Fi/BT. Die Partitionstabelle
  hat dafür bereits die Systempartition `nvs` getrennt von der
  produkt-eigenen `state_store`-Partition. Deshalb wird die Default-
  `nvs_flash_init()`-Initialisierung ausschließlich für die von ESP-IDF
  erwarteten PHY-/Systemdaten beibehalten; sie begründet keinen
  Credential-Owner. `state_store` bleibt alleiniger Credential-
  Persistenzpfad. Kein NVS-Erase/-Repair und keine vorsorgliche Änderung von
  `CONFIG_ESP_PHY_CALIBRATION_AND_DATA_STORAGE` weg vom ESP-IDF-Default.
- Die Ownerentscheidung `VARIANT_B_QR_RETAINED` nimmt nur die lokale
  Credentialeingabe samt Bildschirmtastatur aus diesem Completion-Scope und
  aus R1 heraus. Der WLAN-QR zum SoftAP-Beitritt bleibt R1-Pflicht und wird in
  einem eigenen kleinen Slice umgesetzt; der browserbasierte Setup-Pfad bleibt
  unverändert R1.

Relevante aktuelle Bausteine auf der Baseline:

- `FermentationNetworkModeView` enthält `currentMode`, `selectionRequired`
  und genau die Optionen `AP_ONLY` und `HOME_WIFI`; `UNSELECTED` ist interner
  Bootstrapzustand.
- `FermentationUiApplyNetworkModeCommand`,
  `FermentationUiBeginHomeWifiReconfigurationCommand`, die bestehenden
  `FermentationUiCommandBridge`- und `FermentationApplication`-Pfade sowie
  `FermentationApplication::networkAccessPointInfo()` existieren bereits.
- Der lokale Dispatcher muss diese beiden Netzwerk-Commandvarianten über die
  vorhandenen `FermentationUiCommandBridge::applyNetworkMode()`- und
  `beginHomeWifiReconfiguration()`-Aufrufe an `FermentationApplication`
  weiterleiten und deren `FermentationUiCommandResult` erhalten. Die
  generische Run-Command-Pipeline wird dafür nicht dupliziert oder erweitert.
- `FermentationTouchWorkspace` routet `HeaderNetwork` aktuell nur zu den
  vorhandenen Headerseiten. Der bestehende Press-Dispatcher übergibt typed
  Touch-Intents an die Application-Ownergrenze.
- `networkAccessPointInfo()` ist ein eigener Application-Accessor und
  absichtlich kein Teil der gemeinsamen, geheimnisfreien
  `FermentationNetworkModeView`.
- `sdkconfig.defaults` wird von beiden kanonischen ESP-IDF-Profilen verwendet;
  `sdkconfig.defaults.bringup` und `sdkconfig.defaults.release` überschreiben
  die Stackgröße nicht. `CONFIG_ESP_MAIN_TASK_STACK_SIZE=24576` gilt damit
  gemeinsam für `esp32_bringup` und `esp32_release`.

## 2. Ziel und Abnahmekriterien

Die bestehende lokale `HeaderNetwork`-Seite macht die #164-Moduswahl und die
explizite Heim-WLAN-Neukonfiguration über Touch erreichbar. Sie verwendet
ausschließlich die vorhandenen typisierten Commands und den bestehenden
Application-Ownerpfad. Die Heim-WLAN-Credentials werden für R1 weiterhin über
die bestehende Browser-Setup-Seite eingegeben; lokale Touch-Credentialeingabe
und Bildschirmtastatur bleiben nach der Ownerentscheidung `VARIANT_B`
deferiert. Der WLAN-QR zum SoftAP-Beitritt ist dagegen ein eigenes
R1-Abnahmekriterium dieses Plans.

```text
UNSELECTED -> AP_ONLY oder HOME_WIFI auswählen
AP_ONLY    -> aktuellen Modus erkennen; bei Bedarf HOME_WIFI auswählen
HOME_WIFI  -> aktuellen Modus erkennen; AP_ONLY auswählen;
              explizite HOME_WIFI-Neukonfiguration starten
UNSELECTED_NOT_USER_SELECTABLE=YES
SECOND_NETWORK_STATE_MACHINE=NO
SECOND_NETWORK_COMMAND_PATH=NO
DIRECT_ESP_IDF_CALL_FROM_UI=NO
R1_HOME_WIFI_CREDENTIAL_INPUT=BROWSER_SETUP
R1_TOUCH_HOME_WIFI_CREDENTIAL_ENTRY=DEFERRED
R1_TOUCH_WIFI_KEYBOARD=DEFERRED
R1_WLAN_QR_TO_JOIN_SOFTAP=REQUIRED
R1_WEBSITE_QR=DEFERRED
NETWORK_MODE_SELECTION_MUST_KEEP_PAGE_RENDERED=YES
WHITE_OR_BLACK_SCREEN_AFTER_MODE_SELECTION=FAIL
MCU_RESET_PANIC_WATCHDOG_BROWNOUT=NO
NETWORK_LABELS_FULLY_VISIBLE_AT_320X240=YES
```

Wenn `networkAccessPointInfo()` aktuelle Informationen des aktiven SoftAP
liefert, zeigt ausschließlich die lokale Netzwerkseite SSID, SoftAP-Passwort
und – falls vorhanden – die direkte lokale Setup-/AP-IP an. Bei fehlenden
Informationen zeigt sie einen lokalisierten Verfügbarkeitszustand.

```text
SOFTAP_INFO_SOURCE=EXISTING_APPLICATION_ACCESSOR
SOFTAP_INFO_LOCAL_TOUCH_DISPLAY_ONLY=YES
SOFTAP_SECRET_LOGGING=NO
SOFTAP_SECRET_WEB_API_EXPOSURE=NO
SOFTAP_INFO_PERSISTENCE_COPY=NO
QR_PURPOSE=JOIN_SOFTAP
QR_SOURCE=networkAccessPointInfo()
QR_PAYLOAD=INDIVIDUAL_SOFTAP_SSID_AND_PASSWORD
QR_CONTAINS_WEB_URL=NO
QR_CONTAINS_AP_IP=NO
MANUAL_FALLBACK=SSID_PASSWORD_DIRECT_IP_VISIBLE
SECOND_CREDENTIAL_SOURCE=NO
SECRET_LOGGING=NO
SECOND_CREDENTIAL_STORE=NO
SECOND_HTTP_SERVER=NO
CONFIG_ESP_MAIN_TASK_STACK_SIZE=24576
STACK_SIZE_APPLIES_TO=ESP32_BRINGUP_AND_ESP32_RELEASE
NETWORK_MODE_PATH_REMAINS_SYNCHRONOUS=YES
NEW_NETWORK_WORKER_TASK=NO
WIFI_INIT_CONFIG_NVS_ENABLE=0
DEFAULT_NVS_INIT_FOR_WIFI_CREDENTIALS=NO
DEFAULT_NVS_INIT_FOR_ESP_IDF_PHY_SYSTEM_DATA=YES
CONFIG_ESP_PHY_CALIBRATION_AND_DATA_STORAGE=ESP_IDF_DEFAULT_ENABLED_UNCHANGED
WIFI_CONFIGURATION_STORAGE=WIFI_STORAGE_RAM
CREDENTIAL_PERSISTENCE=EXISTING_STATE_STORE_ONLY
NVS_ERASE_OR_REPAIR=NO
RESOURCE_LOG_FIELDS=FREE_HEAP_MINIMUM_FREE_HEAP_LARGEST_FREE_8BIT_BLOCK_MAIN_TASK_STACK_HWM
RESOURCE_LOG_STAGES=BEFORE_AFTER_NETWORK_MODE_CHANGE_STABLE_AP_ONLY_STABLE_HOME_WIFI
RESOURCE_LOG_IMPLEMENTATION=EXISTING_LOGRESOURCES_AND_EXISTING_MAIN_LOOP_ONLY
NEW_TASK_OR_GENERAL_DIAGNOSTICS_ABSTRACTION=NO
HARDWARE_RESOURCE_EVIDENCE=REQUIRED_AP_ONLY_AND_HOME_WIFI
```

Die Anzeige bleibt eine kurzlebige lokale Projektion. Geheimnisse werden
weder in den gemeinsamen normalen UI-/Web-Snapshot aufgenommen noch in
Logs, Diagnose, Export, URL oder Persistenz kopiert. Der WLAN-QR nutzt
ausschließlich dieselbe lokale `networkAccessPointInfo()`-Projektion und
enthält nur die individuellen SoftAP-SSID-/Passwortdaten im üblichen
WLAN-QR-Format mit korrektem Escaping relevanter Sonderzeichen. Es gibt keine
Secret-Kopie in allgemeine UI-Snapshots, Web/API, Logs, Diagnose, Export oder
Persistenz. Der lokale Touch-Credentialpfad und seine Tastatur werden nicht
eingeführt; die vorhandene browserbasierte Setup-Seite, der AP-/STA-Lifecycle,
Test-before-commit und die Netzwerkpersistenz bleiben unverändert.

## 3. Scope und Nicht-Ziele

Im Scope liegen nur die bestehende lokale Touch-Workspace-/Renderer- und
Composition-Anbindung, die dafür nötige Textprojektion sowie gezielte Tests.
Die Umsetzungsdateien werden auf `main` erneut inventarisiert; voraussichtlich
betroffen sind:

- `lib/fermentation_app/src/fermentation_touch_workspace.*`
- `main/fermentation_ui_renderer.*`
- `main/fermentation_ui_text.*`
- `main/fermentation_ui_press_dispatcher.*`
- `main/app_main.cpp` für die ESP-IDF-Default-NVS-Initialisierung vor
  Wi-Fi/BT ausschließlich zur PHY-Kalibrierung sowie die minimale Erweiterung
  des bestehenden `logResources()` und seiner vorhandenen Main-Loop-Messpunkte
- `lib/device_platform_esp_idf/src/esp_idf_network_lifecycle.*`, nur um den
  bestehenden `WIFI_INIT_CONFIG_DEFAULT()`-Wert gezielt mit
  `nvs_enable=0` zu initialisieren; `WIFI_STORAGE_RAM` bleibt bestehen und
  `CONFIG_ESP_PHY_CALIBRATION_AND_DATA_STORAGE` bleibt auf ESP-IDF-Default
- `sdkconfig.defaults`, mit dem gemeinsamen Main-Task-Stack von 24 KiB; keine
  vorsorgliche PHY-Kalibrierungs-Konfigurationsänderung
- `test/test_press_dispatcher`, `test/test_local_touch_ui`,
  `test/test_renderer_boundary` und – falls der Ownerpfad durch den
  Korrekturdiff betroffen ist – `test/test_fermentation_ui_commands`
- `docs/NETWORK.md`, nur für die nötige Klarstellung zwischen abgeschlossenem
  physischem Touch-/Kalibrierungsnachweis aus #31 und der jetzt #164
  zugeordneten Netzwerkseitenbedienung
- `docs/FUTURE_SCOPE.md` und `docs/REQUIREMENTS.md` für die synchronisierte
  `VARIANT_B`-Abgrenzung der deferierten Touch-Credentialeingabe,
  Bildschirmtastatur sowie den R1-WLAN-QR und den deferierten Webseiten-QR
- vorhandene passende Text-/UI-Contracts und Tests unter `test/`

`FermentationApplication`, `NetworkConfigurationService`,
Credential-Schema und `state_store`-Ownervertrag werden nicht geändert.
`EspIdfNetworkLifecycle` behält ausschließlich die beschriebene
`nvs_enable=0`-Wi-Fi-NVS-Konfiguration; `WIFI_STORAGE_RAM` sowie der bestehende
NetworkConfigurationService-/Application-Pfad bleiben unverändert. Die
Default-NVS-Initialisierung dient nur ESP-IDF-PHY-Systemdaten. Falls die
Implementierung einen bislang fehlenden fachlichen Ownervertrag offenlegt,
stoppt der Builder vor dieser Änderung und meldet den Befund.

Nicht-Ziele:

- Touchkoeffizienten, `tc0`/`tc1`, Rotation, Paneltransformation,
  Displaygeometrie, LVGL- oder Treiberauswahl;
- neue Netzwerk-, Command-, Credential-, HTTP- oder Persistenzpfade;
- Änderungen an `RecordTypeId=9`, `cc0`, Credential-Schema,
  Setup-Routen oder Test-before-commit;
- lokale HOME_WIFI-SSID-/Passworteingabe und Bildschirmtastatur (`DEFERRED`),
  ein separater Webseiten-QR sowie #27
  Auth/Session/CSRF/Weboberfläche und #28-Diagnostik;
- Issue-29-Probe-/Bring-up-Diagnostik; der reale #164-Hardwarelauf nutzt nach
  Ownerentscheidung `esp32_release`;
- neue Tasks, asynchrone Netzwerk-/NVS-Pipelines, Navigation oder allgemeine
  Layout-/QR-Abstraktionen;
- Aktorfreigabe unter allen Umständen (`ACTUATOR_RELEASE=NO`).

## 4. Umsetzungs- und Commit-Slices

Jeder Slice wird nach seiner gezielten Änderung lokal geprüft und als eigener
Commit auf demselben Draft-Branch gepusht. Vor Ownerfreigabe dieses Plans
beginnt kein Slice.

### Slice 1 – Touch-Moduswahl und Commands

- Die bestehende `HeaderNetwork`-Workspace-Seite zeigt aktuellen Modus und
  genau die bestehenden wählbaren Modi.
- Touch-Aktionen liefern die bereits vorhandenen typisierten Commands; die
  Ausführung läuft durch den bestehenden Press-Dispatcher und
  Application-Owner.
- Die HOME_WIFI-Neukonfiguration bleibt eine explizite separate Aktion.
- Der Dispatcher leitet nur die zwei bestehenden Netzwerk-Commandtypen an die
  zugehörigen bestehenden Bridge-/Application-Ownerfunktionen weiter und
  liefert deren typisiertes Ergebnis zurück.
- Gezielte Regressionen: `test_local_touch_ui`, `test_press_dispatcher` und
  `test_fermentation_ui_commands`.

```bash
pio test -e native -f test_local_touch_ui
pio test -e native -f test_press_dispatcher
pio test -e native -f test_fermentation_ui_commands
```

### Slice 2 – lokale SoftAP-Verbindungsdaten

- Die Display-Composition reicht nur die aktuelle optionale
  `networkAccessPointInfo()`-Projektion an die lokale Netzwerkseite weiter.
- SSID, Passwort und optionale direkte AP-IP werden lokal und ohne
  Persistenzkopie angezeigt. Secret-freie Netzwerk-/Web-Modelle bleiben
  unverändert.
- Renderer-/Composition-Tests beweisen Anzeige nur auf der lokalen
  Netzwerkseite, Verfügbarkeit/Fehlen optionaler Werte und Secret-Freiheit
  außerhalb dieser sichtbaren Projektion.
- Gezielte Regression: `test_renderer_boundary` sowie die direkt betroffenen
  `test_local_touch_ui`-/Press-Dispatcher-Tests.

```bash
pio test -e native -f test_renderer_boundary
```

### Slice 3 – WLAN-QR zum SoftAP-Beitritt

- Der lokale Renderer zeigt nach der SSID-/Passwort-/IP-Projektion den QR zum
  Beitritt in den geschützten Setup-/AP-only-SoftAP.
- Die Payloadquelle ist ausschließlich `networkAccessPointInfo()`; enthalten
  werden individuelle SoftAP-SSID und individuelles SoftAP-Passwort im
  üblichen WLAN-QR-Format mit korrektem Escaping relevanter Sonderzeichen.
- Der QR enthält weder Webadresse noch AP-IP. SSID, Passwort und direkte IP
  bleiben als manueller Fallback sichtbar.
- Es gibt keine zweite Credentialquelle und keine Secret-Kopie in allgemeine
  UI-Snapshots, Web/API, Logs, Diagnose, Export oder Persistenz.
- Reuse before Build erfolgt in dieser Reihenfolge:
  1. bestehender ausgewählter LVGL-/`esp_lvgl_port`-Produktstack;
  2. falls ungeeignet, die bereits evaluierte gepflegte QR-Komponente, mit
     Project Nayuki als dokumentiertem bevorzugtem Gegenkandidaten und den
     vorgesehenen Lizenz-, Ressourcen- und Integrationsprüfungen;
  3. eigener QR-Encoder: `NO`.
- Es wird keine allgemeine QR-Abstraktionsplattform eingeführt.
- Softwareevidence umfasst Payload, Escaping/Sonderzeichen, deterministische
  Änderung bei geändertem individuellem SoftAP-Credential, Ausschluss von
  Webadresse/IP, Secret-Freiheit und den 320x240-Renderer-/Layoutnachweis ohne
  Touchkalibrierungsänderung. Gezielte Regressionen ergänzen
  `test_renderer_boundary` um den bestehenden beziehungsweise neu anzulegenden
  QR-Payload-/Renderer-Test in der vorhandenen Testumgebung.

### Slice 4 – produktiver Touch-Hit-Test und 320x240-Lesbarkeit

- Der bestehende gerenderte WLAN-Headerbereich ist über den produktiven
  `pollTouch()` → `processWorkspaceTouch()` → `targetAt()` → `routePress()` →
  `FermentationTouchWorkspace::press()`-Pfad erreichbar. Der Hit-Test bleibt
  deckungsgleich mit dem gerenderten WLAN-Symbol; Bottom-Slots,
  Touchkalibrierung und Paneltransformation bleiben unverändert.
- Regression vom normalen Home-/Standby-Zustand: frischer Header-Touch setzt
  `pressedTarget=HeaderNetwork` und wechselt zur `HeaderNetwork`-Seite; danach
  löst derselbe produktive Pfad `AP_ONLY` aus und erreicht den bestehenden
  Bridge-/Application-Ownerpfad.
- Die bereits vorhandenen Moduslabels werden auf 320x240 vollständig lesbar
  dargestellt. Es entsteht keine neue Layoutabstraktion.
- Gezielte Regressionen: `test_press_dispatcher`, `test_local_touch_ui` und
  `test_renderer_boundary`; `test_fermentation_ui_commands` nur, falls der
  Korrekturdiff diesen Ownervertrag tatsächlich berührt.

### Slice 5 – Main-Task-Stack, getrennte Wi-Fi-/PHY-NVS-Owner und Ressourcenlogs

- Reale Diagnose: der bisherige 16-KiB-Main-Task-Stack reicht für den
  synchronen Touch → Netzwerkmodus → `NetworkConfigurationService` → NVS-
  Credentialpfad nicht aus. Die Korrektur bleibt synchron im bestehenden
  Ownerpfad und setzt `CONFIG_ESP_MAIN_TASK_STACK_SIZE=24576` in den
  gemeinsamen `sdkconfig.defaults`; damit gilt derselbe Wert für beide
  ESP32-Profile.
- 24 KiB ist die kleinste robuste Korrektur im vorhandenen Design: 16 KiB sind
  durch die reale Stackerschöpfung als unzureichend belegt; der gemeinsame
  24-KiB-Wert ist die bereits gewählte und profilübergreifend gebaute
  Konfiguration. Ein kleinerer Zwischenwert wäre ohne einen Messnachweis
  willkürlich. Ein zusätzlicher Worker, asynchrone Übergabe oder geänderte
  Command-/NVS-Ownership würde deutlich mehr Zustands- und Testoberfläche
  schaffen, ohne für diesen synchronen Pfad erforderlich zu sein.
- `wifi_init_config_t.nvs_enable=0` und `WIFI_STORAGE_RAM` bleiben bestehen;
  Wi-Fi speichert also keine zweite Konfigurationswahrheit. `state_store`
  bleibt alleiniger Credential-Persistenzowner. Davon getrennt initialisiert
  `app_main` die Defaultpartition via `nvs_flash_init()` vor Wi-Fi/BT nur für
  ESP-IDF-PHY-Systemdaten/RF-Kalibrierung. Die `nvs`-Systempartition bleibt
  von `state_store` getrennt. `CONFIG_ESP_PHY_CALIBRATION_AND_DATA_STORAGE`
  bleibt beim ESP-IDF-v6.1-Default `enabled`; keine vorsorgliche Abweichung,
  kein NVS-Erase/-Repair und kein zweiter Produkt-Credentialpfad. Ein Fehler
  der Default-NVS-Initialisierung wird sichtbar protokolliert und fail-closed
  behandelt; es gibt keine automatische Erase-/Repair- oder Credential-
  Migration.
- Das vorhandene `logResources()` wird minimal um
  `esp_get_minimum_free_heap_size()` und
  `heap_caps_get_largest_free_block(MALLOC_CAP_8BIT)` ergänzt. Die Ausgabe
  enthält außerdem den bereits erfassten freien Heap und Main-Task-Stack-HWM.
  Im bestehenden Main-Loop werden dieselben Messwerte mit einem knappen
  Messpunkt-Tag und den vorhandenen Netzwerkmodus-/Lifecycle-Statuswerten
  erfasst: unmittelbar vor und nach der synchronen Modus-/Reconfiguration-
  Aktion; beim Übergang in den stabilen `AccessPointOnly`-Zustand; und bei
  `HOME_WIFI` im stabilen `HomeConnected`-Zustand beziehungsweise – solange
  noch keine Heim-Credentials übernommen wurden – im stabilen
  `SetupAccessPoint`-Zustand. Die bestehenden Boot-/30-s-Messpunkte bleiben.
  Messungen werden nicht pro Loop wiederholt; es entsteht kein neuer Task,
  Issue-29-Probe, periodischer Diagnosepfad oder allgemeine Abstraktion.
  Minimum-Free-Heap und Stack-HWM sind Low-Watermarks seit Boot und werden
  entsprechend gekennzeichnet; der größte freie 8-bit-Block ist der aktuelle
  zusammenhängende Block.
- Gezielte Hostregressionen decken Modus-/Credential-Commit und den
  produktiven Dispatcherpfad ab: `test_network_configuration`,
  `test_press_dispatcher`, `test_local_touch_ui` und
  `test_renderer_boundary`; `test_fermentation_ui_commands` nur falls der
  Korrekturdiff dessen Ownervertrag berührt. Danach beide ESP-IDF-Profile
  bauen und validieren; kein Profilbuild gilt als Hardwareevidence.

```bash
pio test -e native -f test_network_configuration
pio test -e native -f test_press_dispatcher
pio test -e native -f test_local_touch_ui
pio test -e native -f test_renderer_boundary
```

### Slice 6 – Konvergenz, Builds und Status

- Alle gezielten Tests aus Slice 1 bis 5 auf dem finalen Implementierungs-HEAD
  sowie die Korrekturen aus Slice 4 und 5 ausführen; danach `git diff --check` und
  `bash scripts/run_pre_ready_gates.sh self-check` auf exakt diesem HEAD.
- ESP-IDF-Profile `bringup` und `release` mit den dokumentierten
  Projektbefehlen bauen. Die Builds sind Softwareevidence; kein Flash und
  keine Hardware-/Clientbehauptung daraus ableiten.
- Der Builder-Self-Check ist kein unabhängiger Review. Nach gezielten Tests,
  Builds und Self-Check für Independent Fix Verification anhalten. Flash und
  automatisierte Boot-/UART-Evidence sind durch den Owner für den späteren
  exakten finalen `esp32_release`-HEAD bereits freigegeben, aber erst nach
  Planfreigabe sowie Implementierungs- und Review-Gates aus Abschnitt 5.
- Nur tatsächlich benötigte Status-/Vertragsdokumentation synchronisieren,
  insbesondere `docs/NETWORK.md`, `docs/FUTURE_SCOPE.md`,
  `docs/REQUIREMENTS.md`, Roadmap, PR #171 und Issue #164. Issue #164 bleibt
  offen.
- Commits pushen und danach für unabhängige Review/Fix Verification des
  begrenzten Completion-Diffs stoppen. Kein Ready-Wechsel oder Merge.

## 5. Verifikation und Hardware-Evidence

Softwareevidence auf früheren Heads bleibt historische Evidence; die
Planrevision selbst führt keine Tests, Builds oder Hardwareläufe aus. Nach
Ownerfreigabe der neuen exakten Plan-SHA werden die Default-NVS-/Ressourcen-
Korrekturen implementiert. Danach sind gezielte Regressionen und beide
Profilbuilds auf dem finalen Implementierungs-HEAD erforderlich.
`esp32_bringup` ist für
diesen Hardwarelauf ungeeignet, weil sein Issue-29-Probe vor
Displayinitialisierung endet. Für #164 ist ausschließlich `esp32_release` zu
verwenden; das Releaseprofil hebt Hardware- oder Aktorgates nicht auf.

Der Owner hat den `esp32_release`-Flash und automatisierbare Flash-/Boot-/UART-
Evidence genehmigt, auch bei eigener Abwesenheit. Ausführungsbedingung bleiben
die Freigabe der neuen exakten Plan-SHA, die geplante Umsetzung samt gezielter
Verifikation und Independent Fix Verification ohne offene Blocker. Danach
wird genau der finale, gebaute Implementierungs-HEAD mit dem kanonischen
ESP-IDF-/Repository-Flashpfad als `esp32_release` geflasht. Keine
Rebuild-on-flash-, Vollerase- oder NVS-Erase-/Repair-Aktion. Source-HEAD,
Buildartefakt, Profil, Flashverifikation und UART-Log werden gemeinsam
protokolliert. Der Testaufbau ist ein Entwickler-/Bring-up-Aufbau am PC; er
weist weder einen Fermentationslauf noch funktionale Aktor- oder
Leistungspfad-Evidence nach. Ein erfolgreicher Flash, Boot oder UART-Smoke
belegt weder Touch, QR, Client-Join, AP_ONLY, HOME_WIFI noch Reconnect.

Erster UART-Lauf klassifiziert jeden Bildschirmabbruch als MCU-Reset,
Brownout, Panic, Watchdog oder UI-/Renderer-/Network-Lifecycle-Fehler.
Während Touch und Moduswechsel sind UART und Display gemeinsam zu beobachten.
Bei Reset, Panic, Watchdog, Brownout, erneut schwarzem/weißem Bildschirm oder
unerwarteter Aktoraktivität abbrechen, UART-Evidence sichern und keine
Erfolgswerte für nachgelagerte Schritte setzen.

Die realen Interaktionsabnahmen auf exakt diesem HEAD benötigen Owner-Präsenz;
bei Abwesenheit bleiben sie `NOT_RUN`, nicht abgeleitet aus Flash-/Boot-
Evidence:

1. Normalen Home-/Standby-Zustand booten und das WLAN-Symbol im produktiven
   Touchpfad öffnen; `HeaderNetwork` muss sichtbar bleiben.
2. Prüfen, dass `AP_ONLY` und `HOME_WIFI` vollständig lesbar und auswählbar
   sind, `UNSELECTED` nicht auswählbar ist und beide Modusaktionen die
   Netzwerkseite nicht verlassen oder den Bildschirm löschen.
3. `AP_ONLY` auswählen; SoftAP-Start, individuelle SSID, Passwort, direkte
   lokale IP und WLAN-QR visuell prüfen. Alle Labels müssen auf 320x240 ohne
   Abschneiden lesbar sein.
4. WLAN-QR mit einem geeigneten Handy scannen und den Client-Join prüfen;
   danach direkte AP-IP im Browser aufrufen. Manueller Join mit angezeigter
   SSID/Passwort bleibt verfügbar.
5. `HOME_WIFI` auswählen und Browser-Setup öffnen; Credentials über den
   bestehenden Browserpfad eingeben, Test-before-commit erfolgreich
   durchlaufen und nur danach committen.
6. Neustart mit gespeicherter HOME_WIFI-Konfiguration ausführen, Verbindung
   bestätigen, das Heim-WLAN kurz unterbrechen und den grundlegenden
   Reconnect bestätigen.
7. Während `AP_ONLY` und `HOME_WIFI` die bestehenden UART-Ressourcenlogs
   erfassen: `free_heap_bytes`, `minimum_free_heap_bytes`,
   `largest_free_block_8bit_bytes` und `stack_hwm_bytes`. Messpunkte müssen
   vor/nach Moduswechsel sowie im stabilen AP_ONLY-/HOME_WIFI-Zustand
   vorliegen. Minimum-Free-Heap und Stack-HWM sind kumulative Low-Watermarks
   seit Boot; der größte freie Block ist ein aktueller Messwert. Keine neuen
   numerischen Freigabeschwellen erfinden. Fehlgeschlagene Allokationen,
   Stackerschöpfung oder Reset bedeuten FAIL.

Der Owner-abwesende automatisierte Flash-/Boot-/UART-Lauf darf unabhängig von
den Interaktionsabnahmen abgeschlossen werden. Danach bleiben alle nicht
ausgeführten Touch-, UI-, QR-, Client-, Browser-, Netzwerkmodus- und
Reconnect-Felder ausdrücklich `NOT_RUN`. Insbesondere muss der reale Scan des
auf dem Produktdisplay angezeigten WLAN-QR mit geeignetem Client weiterhin
vor dem finalen Abschluss von Issue #164 nachgeholt werden.

```text
PROFILE=esp32_release
FLASH_SOURCE_HEAD=EXACT_FINAL_IMPLEMENTATION_HEAD
BUILD_ARTIFACT_HEAD=MUST_MATCH_FLASH_SOURCE_HEAD
ERASE_ALL=NO
NVS_ERASE_OR_REPAIR=NO
REBUILD_DURING_FLASH=NO
PRODUCT_TOUCH_NETWORK_ENTRY=PASS/FAIL/NOT_RUN
NETWORK_PAGE_STAYS_RENDERED_FOR_BOTH_MODES=PASS/FAIL/NOT_RUN
AP_ONLY_SELECTION=PASS/FAIL/NOT_RUN
HOME_WIFI_SELECTION=PASS/FAIL/NOT_RUN
UNSELECTED_NOT_USER_SELECTABLE=PASS/FAIL/NOT_RUN
NETWORK_LABELS_READABLE_320X240=PASS/FAIL/NOT_RUN
SOFTAP_STARTED=PASS/FAIL/NOT_RUN
SOFTAP_SSID_PASSWORD_DIRECT_IP_DISPLAY=PASS/FAIL/NOT_RUN
WLAN_QR_DISPLAY=PASS/FAIL/NOT_RUN
WLAN_QR_CAMERA_SCAN=PASS/FAIL/NOT_RUN
SOFTAP_CLIENT_JOIN_QR=PASS/FAIL/NOT_RUN
DIRECT_AP_IP_BROWSER=PASS/FAIL/NOT_RUN
HOME_WIFI_BROWSER_SETUP=PASS/FAIL/NOT_RUN
HOME_WIFI_TEST_BEFORE_COMMIT=PASS/FAIL/NOT_RUN
HOME_WIFI_COMMIT_AFTER_SUCCESS_ONLY=PASS/FAIL/NOT_RUN
HOME_WIFI_BOOT_WITH_STORED_CREDENTIAL=PASS/FAIL/NOT_RUN
HOME_WIFI_BASIC_RECONNECT=PASS/FAIL/NOT_RUN
MAIN_TASK_STACK_HWM_AP_ONLY=RECORDED_BYTES_OR_NOT_RUN
MAIN_TASK_STACK_HWM_HOME_WIFI=RECORDED_BYTES_OR_NOT_RUN
RESOURCE_AP_ONLY_BEFORE_MODE_CHANGE=ALL_FOUR_FIELDS_OR_NOT_RUN
RESOURCE_AP_ONLY_AFTER_MODE_CHANGE=ALL_FOUR_FIELDS_OR_NOT_RUN
RESOURCE_AP_ONLY_STABLE=ALL_FOUR_FIELDS_OR_NOT_RUN
RESOURCE_HOME_WIFI_BEFORE_MODE_CHANGE=ALL_FOUR_FIELDS_OR_NOT_RUN
RESOURCE_HOME_WIFI_AFTER_MODE_CHANGE=ALL_FOUR_FIELDS_OR_NOT_RUN
RESOURCE_HOME_WIFI_STABLE=ALL_FOUR_FIELDS_OR_NOT_RUN
FREE_HEAP_BYTES=RECORDED_VALUE_OR_NOT_RUN
MINIMUM_FREE_HEAP_BYTES=RECORDED_VALUE_OR_NOT_RUN
LARGEST_FREE_BLOCK_8BIT_BYTES=RECORDED_VALUE_OR_NOT_RUN
MAIN_TASK_STACK_HWM_BYTES=RECORDED_VALUE_OR_NOT_RUN
DEFAULT_NVS_PHY_INIT=PASS/FAIL/NOT_RUN
UART_BOOT=PASS/FAIL/NOT_RUN
UART_PANIC=NO/YES/NOT_RUN
UART_WATCHDOG=NO/YES/NOT_RUN
UART_BROWNOUT=NO/YES/NOT_RUN
UART_UNEXPECTED_RESET=NO/YES/NOT_RUN
ACTUATOR_RELEASE=NO
```

Lokale HOME_WIFI-SSID-/Passworteingabe und Bildschirmtastatur bleiben durch
die Ownerentscheidung `VARIANT_B` aus R1/#164 deferiert. Der browserbasierte
Setup-/Test-before-Commit-Pfad und der WLAN-QR zum SoftAP-Beitritt bleiben
R1. Der reale Kamera-/Client-Scan des auf dem Produktdisplay angezeigten
WLAN-QR ist vor dem finalen Abschluss von Issue #164 erforderlich. Nicht
ausgeführte Hardwarepunkte bleiben `NOT_RUN`. Der Hardwarelauf gibt keine
Aktorfreigabe und behauptet keinen Fermentationslauf.

## 6. Quellen und Owner-Gates

- `docs/AGENT_WORKFLOW.md`, `docs/ENGINEERING_PRINCIPLES.md`
- `docs/NETWORK.md`, `docs/FUTURE_SCOPE.md`, `docs/REQUIREMENTS.md`,
  `docs/ROADMAP.md`
- `docs/THIRD_PARTY_COMPONENTS.md` und die bestehende QR-Komponenten-
  Evaluation als Reuse-before-Build-Referenz
- `partitions/issue_90_state_store.csv` für die getrennten `nvs`- und
  `state_store`-Partitionen
- `docs/tasks/issue-164-r1-wlan-native-http-integration-plan.md` als
  unveränderte Historie und Referenz der bestehenden #164-Verträge
- Issue #164 sowie der gemergte PR #165
- ADR-013 und die bestehenden Touch-/Renderer-/Network-Tests auf
  `BASE_SHA=b871375f494701bed1834013cfeb789856983e3a`
- ESP-IDF v6.1 Wi-Fi API für `wifi_init_config_t.nvs_enable` und
  `WIFI_STORAGE_RAM`:
  [Wi-Fi API Reference](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32/api-reference/network/esp_wifi.html),
  und Espressifs [v6.1 Wi-Fi-Test mit `nvs_enable=false`](https://github.com/espressif/esp-idf/blob/v6.1/components/esp_wifi/test_apps/wifi_function/main/test_wifi_country.c)
- ESP-IDF v6.1 [PHY-Kconfig](https://github.com/espressif/esp-idf/blob/v6.1/components/esp_phy/Kconfig)
  und [PHY-Initialisierung](https://github.com/espressif/esp-idf/blob/v6.1/components/esp_phy/src/phy_init.c)
  für standardmäßig aktivierte RF-Kalibrierungsdaten in NVS und den
  `phy`-Namespace; die NVS-Initialisierung ist von Wi-Fi-Credentialpersistenz
  getrennt
- ESP-IDF v6.1 [System-Heap-API](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32/api-reference/system/misc_system_api.html)
  und [Heap-Capabilities-API](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32/api-reference/system/mem_alloc.html)
  für Minimum-Free-Heap und größte freie Allokation
- `docs/HARDWARE.md`, `docs/OPEN_POINTS.md` und
  `docs/ESP_IDF_UPGRADE_CONTRACT.md` für Hardware-, Ressourcen- und Profil-
  Grenzen

```text
OWNER_APPROVAL_REQUIRED_FOR_EXACT_PLAN_SHA=YES
PREVIOUS_OWNER_APPROVED_PLAN_SHA=744a1f29478c41e9ced42a77a0e6336c63bf8724
PLAN_REVISION_BASE_HEAD=d65420fe6573411f3bfef1596fc09b5865404e52
PRODUCT_CODE_CHANGE_DURING_PLAN_REVISION=NO
FLASH_DURING_PLAN_REVISION=NO
PRODUCT_TESTS_DURING_PLAN_REVISION=NOT_RUN_PLAN_ONLY
PRODUCT_BUILDS_DURING_PLAN_REVISION=NOT_RUN_PLAN_ONLY
OWNER_DECISION_VARIANT_B_QR_RETAINED=RECORDED
INDEPENDENT_PLAN_FIX_VERIFICATION=REQUIRED
FOLLOWUP_IMPLEMENTATION_AUTHORIZATION=REQUIRES_OWNER_APPROVAL_OF_THIS_EXACT_PLAN_SHA
HARDWARE_PROFILE=esp32_release
OWNER_FLASH_AND_AUTOMATED_BOOT_UART_AUTHORIZATION=GRANTED_AFTER_PLAN_APPROVAL_AND_IMPLEMENTATION_GATES
OWNER_PRESENCE_FOR_FLASH_BOOT_UART=NOT_REQUIRED
UNEXECUTED_INTERACTIVE_HARDWARE_TESTS=NOT_RUN
HARDWARE_EVIDENCE=REQUIRED_BEFORE_ISSUE164_CLOSE
HARDWARE_CLIENT_SCAN_BEFORE_ISSUE_CLOSE=REQUIRED
PR_READY=NO
PR_MERGE=NO
ISSUE164_CLOSE=NO
ACTUATOR_RELEASE=NO
```
