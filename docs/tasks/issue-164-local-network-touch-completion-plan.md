# Plan – Issue #164: lokale Touch-Netzwerkbedienung vervollständigen

## Planstatus und Provenienz

```text
ISSUE=164
BASE_BRANCH=main
BASE_SHA=b871375f494701bed1834013cfeb789856983e3a
PR165=MERGED
PR165_MERGE_COMMIT=1f1755e5e706fb668472920545b5302fcef1df16
PREVIOUS_COMPLETION_PLAN_SHA=a514544321807ad47abce5de27a68595085079d3
PREVIOUS_OWNER_APPROVED_PLAN_SHA=931db488125a6e7eca76b1e8bb88ffc98b97a073
PLAN_REVISION_BASE_HEAD=e84555baea8ccbf99aacf2898ff3333cd33464ec
PLAN_REVISION_REASON=HARDWARE_TOUCH_FAILURE_MAIN_STACK_AND_WIFI_NVS
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
PLAN_STATUS=REVISION_REQUIRES_INDEPENDENT_PLAN_FIX_VERIFICATION
OWNER_APPROVAL_FOR_THIS_PLAN_REVISION=REQUIRED
IMPLEMENTATION_AUTHORIZATION_FOR_FOLLOWUP=NO_PENDING_PLAN_APPROVAL
LAST_PRODUCT_IMPLEMENTATION_HEAD=e84555baea8ccbf99aacf2898ff3333cd33464ec
STACK_CORRECTION=24576_BYTES_BOTH_ESP32_PROFILES
WIFI_NVS_POLICY=DISABLED_VIA_WIFI_INIT_CONFIG_NVS_ENABLE_ZERO
DEFAULT_NVS_INIT_FOR_WIFI=NOT_REQUIRED
STATE_STORE=SOLE_CREDENTIAL_PERSISTENCE
WIFI_STORAGE=WIFI_STORAGE_RAM
PRIOR_TARGETED_NATIVE_TESTS=PASS_ON_PREVIOUS_HEADS
PRIOR_BUILDER_SELF_CHECK=PASS_ON_PREVIOUS_HEADS
PRIOR_ESP32_PROFILE_BUILDS=PASS_ON_PREVIOUS_HEADS
CURRENT_REVISION_TESTS=NOT_RUN_PLAN_ONLY
CURRENT_REVISION_BUILDS=NOT_RUN_PLAN_ONLY
HARDWARE_TESTS=PARTIAL_BRINGUP_BOOT_ONLY_PRODUCT_TOUCH_NOT_VERIFIED
HARDWARE_CLIENT_EVIDENCE=NOT_RUN_PENDING_APPROVED_RELEASE_RUN
HARDWARE_REVIEW=CHANGES_REQUIRED
OPEN_HARDWARE_BLOCKERS=2
HARDWARE_PROFILE=esp32_release
ACTUATOR_RELEASE=NO
```

Diese vollständige Revision ersetzt die bisherige Completion-Plan-Fassung
für PR #171 und Issue #164. Sie übernimmt deren bestätigte R1-Abgrenzung und
QR-Vertrag und ergänzt die seitdem erforderlichen Touch-/Layout-, Main-Task-
Stack- und Wi-Fi-Initialisierungskorrekturen samt Hardware-Evidence. Sie
ändert nicht rückwirkend den gemergten Implementierungsplan oder historische
Evidence. Vor Ownerfreigabe dieser exakten Planrevision erfolgen keine weitere
Produktcodeänderung und kein Flash. Diese Planrevision selbst führt keine
Tests oder Builds aus.

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
- Der aktuelle PR-Branch steht auf
  `e84555baea8ccbf99aacf2898ff3333cd33464ec`. Die jüngsten Korrektur-Commits
  erweitern den 320x240-WLAN-Header-Hit-Test, setzen den gemeinsamen
  ESP-Main-Task-Stack auf 24 KiB und initialisieren aktuell Default-NVS vor
  `esp_wifi_init()`. Gezielte native Tests, Builder-Self-Check und beide
  ESP-IDF-Profilbuilds sind auf diesem HEAD laut Live-PR PASS; diese digitale
  Evidence ersetzt nicht die jetzt geforderte reale Touch-/Mode-Evidence.
- Die Hardwarebeobachtung war ein kurz weißer und danach wieder auf Home
  stehender Bildschirm bei `AP_ONLY`/`HOME_WIFI`; der Owner berichtete
  zusätzlich einen schwarzen Bildschirm nach `AP_ONLY`. Die Diagnose ordnet
  die synchrone Netzwerkmodus-/NVS-Arbeit dem Main-Task zu: dessen bisherige
  16 KiB reichen dafür nicht aus. Der Stack wurde auf 24 KiB für beide
  ESP32-Profile erhöht. Diese Korrektur ist digital gebaut, aber auf dem
  Produkt-Touchpfad noch nicht real bestätigt.
- Der letzte `esp32_bringup`-Boot auf dem aktuellen HEAD initialisierte die
  Default-NVS und startete den SoftAP, bevor der separate Issue-29-Probe seine
  98.304-Byte-Task nicht erzeugen konnte und vor Displayinitialisierung
  endete. Issue #29 ist für diese #164-Evidence ausdrücklich out of scope;
  die Ownerentscheidung bestimmt `esp32_release` als Hardwareprofil. Es gab
  nach dieser Entscheidung keinen weiteren Flash.
- Auf dem aktuellen HEAD initialisiert `app_main` die ESP-IDF-Default-NVS
  vor `esp_wifi_init()`. Das ist keine Credential-Persistenz: Credentials
  bleiben Eigentum des vorhandenen `state_store`, und der Wi-Fi-Adapter setzt
  bereits `WIFI_STORAGE_RAM`. Die Prüfung des einfacheren offiziellen
  `wifi_init_config_t.nvs_enable = 0`-Pfads ergibt, dass er den R1-Vertrag
  erfüllt: Die v6.1-API beschreibt `nvs_enable` als Wi-Fi-NVS-Flash-Schalter,
  Espressifs v6.1-Testcode initialisiert Wi-Fi mit `nvs_enable=false`, und
  `WIFI_STORAGE_RAM` hält Wi-Fi-Konfiguration ausschließlich im RAM. R1
  braucht keine zweite Wi-Fi-Flash-Konfiguration, weil der bestehende
  `state_store` Credentials lädt/speichert und der Lifecycle sie zur Laufzeit
  an den RAM-Adapter übergibt. Daher bevorzugt dieser Plan `nvs_enable=0` und
  entfernt die dafür allein eingeführte Default-NVS-Initialisierung. Das ist
  eine aus den offiziellen API-/Testverträgen und dem vorhandenen Ownerpfad
  abgeleitete Entscheidung; sie wird nach Planfreigabe durch gezielte Tests
  und `esp32_release`-Hardware-Evidence verifiziert. Es gibt kein NVS-Erase,
  keine NVS-Reparatur und keinen neuen Persistenzpfad.
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
DEFAULT_NVS_INIT_FOR_WIFI=NO
WIFI_CONFIGURATION_STORAGE=WIFI_STORAGE_RAM
CREDENTIAL_PERSISTENCE=EXISTING_STATE_STORE_ONLY
NVS_ERASE_OR_REPAIR=NO
HARDWARE_STACK_HWM_HEAP_EVIDENCE=REQUIRED_AP_ONLY_AND_HOME_WIFI
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
- `main/app_main.cpp`, um die nicht benötigte Default-NVS-Initialisierung
  nach Umstellung des Wi-Fi-Init-Vertrags zu entfernen
- `lib/device_platform_esp_idf/src/esp_idf_network_lifecycle.*`, nur um den
  bestehenden `WIFI_INIT_CONFIG_DEFAULT()`-Wert gezielt mit
  `nvs_enable=0` zu initialisieren; `WIFI_STORAGE_RAM` bleibt bestehen
- `sdkconfig.defaults`, nur mit dem gemeinsamen Main-Task-Stack von 24 KiB
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
`EspIdfNetworkLifecycle` erhält ausschließlich die beschriebene
`nvs_enable=0`-Konfiguration; die existierende `WIFI_STORAGE_RAM`-Auswahl und
der bestehende NetworkConfigurationService-/Application-Pfad bleiben
unverändert. Falls die Implementierung einen bislang fehlenden fachlichen
Ownervertrag offenlegt, stoppt der Builder vor dieser Änderung und meldet den
Befund.

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

### Slice 5 – Main-Task-Stack und Wi-Fi-NVS-Lifecycle

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
- ESP-IDF v6.1 bietet `wifi_init_config_t.nvs_enable`; der offizielle
  `nvs_enable=false`-Initpfad wird verwendet. `WIFI_STORAGE_RAM` bleibt
  bestehen, `state_store` bleibt einziger Credential-Persistenzowner. Die
  mit der aktuellen Korrektur eingeführte separate `nvs_flash_init()`-
  Initialisierung der Defaultpartition wird entfernt; die unabhängige
  `state_store`-Partitionsinitialisierung bleibt bestehen. Kein NVS-Erase,
  keine NVS-Reparatur und kein zweiter Speicherpfad.
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

- Alle gezielten Tests aus Slice 1 bis 3 auf dem finalen Implementierungs-HEAD
  sowie die Korrekturen aus Slice 4 und 5 ausführen; danach `git diff --check` und
  `bash scripts/run_pre_ready_gates.sh self-check` auf exakt diesem HEAD.
- ESP-IDF-Profile `bringup` und `release` mit den dokumentierten
  Projektbefehlen bauen. Die Builds sind Softwareevidence; kein Flash und
  keine Hardware-/Clientbehauptung daraus ableiten.
- Der Builder-Self-Check ist kein unabhängiger Review. Nach gezielten Tests,
  Builds und Self-Check für unabhängige Fix Verification anhalten. Erst nach
  deren Abschluss, `OPEN_BLOCKERS=0` und ausdrücklicher Ownerautorisierung den
  Hardwarelauf gemäß Abschnitt 5 beginnen.
- Nur tatsächlich benötigte Status-/Vertragsdokumentation synchronisieren,
  insbesondere `docs/NETWORK.md`, `docs/FUTURE_SCOPE.md`,
  `docs/REQUIREMENTS.md`, Roadmap, PR #171 und Issue #164. Issue #164 bleibt
  offen.
- Commits pushen und danach für unabhängige Review/Fix Verification des
  begrenzten Completion-Diffs stoppen. Kein Ready-Wechsel oder Merge.

## 5. Verifikation und Hardware-Evidence

Softwareevidence auf früheren Heads bleibt historische Evidence; die
Planrevision selbst führt keine Tests, Builds oder Hardwareläufe aus. Nach
Ownerfreigabe der neuen exakten Plan-SHA wird die Default-NVS-Umstellung
implementiert. Danach sind gezielte Regressionen und beide Profilbuilds auf
dem finalen Implementierungs-HEAD erforderlich. `esp32_bringup` ist für
diesen Hardwarelauf ungeeignet, weil sein Issue-29-Probe vor
Displayinitialisierung endet. Für #164 ist ausschließlich `esp32_release` zu
verwenden; das Releaseprofil hebt Hardware- oder Aktorgates nicht auf.

Vor dem Flash müssen Planfreigabe, unabhängige Fix Verification und die
erforderliche Ownerautorisierung für den Hardwarelauf vorliegen. Dann wird
genau der finale, gebaute Implementierungs-HEAD mit dem kanonischen
ESP-IDF-/Repository-Flashpfad als `esp32_release` geflasht. Keine
Rebuild-on-flash-, Vollerase- oder NVS-Erase-/Repair-Aktion. Source-HEAD,
Buildartefakt, Profil, Flashverifikation und UART-Log werden gemeinsam
protokolliert. Der Testaufbau ist ein Entwickler-/Bring-up-Aufbau am PC; er
weist weder einen Fermentationslauf noch funktionale Aktor- oder
Leistungspfad-Evidence nach.

Erster UART-Lauf klassifiziert jeden Bildschirmabbruch als MCU-Reset,
Brownout, Panic, Watchdog oder UI-/Renderer-/Network-Lifecycle-Fehler.
Während Touch und Moduswechsel sind UART und Display gemeinsam zu beobachten.
Bei Reset, Panic, Watchdog, Brownout, erneut schwarzem/weißem Bildschirm oder
unerwarteter Aktoraktivität abbrechen, UART-Evidence sichern und keine
Erfolgswerte für nachgelagerte Schritte setzen.

Die Owner-geführten realen Abnahmeschritte auf exakt diesem HEAD:

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
7. Während `AP_ONLY` und `HOME_WIFI` Main-Task-Stack-High-Water-Mark sowie
   freien, niedrigsten und größten freien Heap dokumentieren. Je Modus
   Messpunkte vor/nach Moduswechsel und im stabilen Zustand erfassen; der
   Stack-HWM ist kumulativ seit Boot und ist als solcher zu kennzeichnen.
   Keine neuen numerischen Freigabeschwellen erfinden. Fehlgeschlagene
   Allokationen, Stackerschöpfung oder Reset bedeuten FAIL.

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
FREE_MIN_LARGEST_HEAP_AP_ONLY=RECORDED_BYTES_OR_NOT_RUN
FREE_MIN_LARGEST_HEAP_HOME_WIFI=RECORDED_BYTES_OR_NOT_RUN
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
- `docs/tasks/issue-164-r1-wlan-native-http-integration-plan.md` als
  unveränderte Historie und Referenz der bestehenden #164-Verträge
- Issue #164 sowie der gemergte PR #165
- ADR-013 und die bestehenden Touch-/Renderer-/Network-Tests auf
  `BASE_SHA=b871375f494701bed1834013cfeb789856983e3a`
- ESP-IDF v6.1 Wi-Fi API für `wifi_init_config_t.nvs_enable` und
  `WIFI_STORAGE_RAM`:
  [Wi-Fi API Reference](https://docs.espressif.com/projects/esp-idf/en/v6.1/esp32/api-reference/network/esp_wifi.html),
  und Espressifs [v6.1 Wi-Fi-Test mit `nvs_enable=false`](https://github.com/espressif/esp-idf/blob/v6.1/components/esp_wifi/test_apps/wifi_function/main/test_wifi_country.c)
- `docs/HARDWARE.md`, `docs/OPEN_POINTS.md` und
  `docs/ESP_IDF_UPGRADE_CONTRACT.md` für Hardware-, Ressourcen- und Profil-
  Grenzen

```text
OWNER_APPROVAL_REQUIRED_FOR_EXACT_PLAN_SHA=YES
PREVIOUS_OWNER_APPROVED_PLAN_SHA=931db488125a6e7eca76b1e8bb88ffc98b97a073
PLAN_REVISION_BASE_HEAD=e84555baea8ccbf99aacf2898ff3333cd33464ec
PRODUCT_CODE_CHANGE_DURING_PLAN_REVISION=NO
FLASH_DURING_PLAN_REVISION=NO
PRODUCT_TESTS_DURING_PLAN_REVISION=NOT_RUN_PLAN_ONLY
PRODUCT_BUILDS_DURING_PLAN_REVISION=NOT_RUN_PLAN_ONLY
OWNER_DECISION_VARIANT_B_QR_RETAINED=RECORDED
INDEPENDENT_PLAN_FIX_VERIFICATION=REQUIRED
FOLLOWUP_IMPLEMENTATION_AUTHORIZATION=REQUIRES_OWNER_APPROVAL_OF_THIS_EXACT_PLAN_SHA
HARDWARE_PROFILE=esp32_release
HARDWARE_EVIDENCE=REQUIRED_BEFORE_ISSUE164_CLOSE
HARDWARE_CLIENT_SCAN_BEFORE_ISSUE_CLOSE=REQUIRED
PR_READY=NO
PR_MERGE=NO
ISSUE164_CLOSE=NO
ACTUATOR_RELEASE=NO
```
