# Plan – Issue #164: lokale Touch-Netzwerkbedienung vervollständigen

## Planstatus und Provenienz

```text
ISSUE=164
BASE_BRANCH=main
BASE_SHA=b871375f494701bed1834013cfeb789856983e3a
PR165=MERGED
PR165_MERGE_COMMIT=1f1755e5e706fb668472920545b5302fcef1df16
PREVIOUS_APPROVED_PLAN_SHA=67fbc1786b42f8ca3dc0afe65cdb4343aed2d70e
OWNER_APPROVED_PLAN_SHA=931db488125a6e7eca76b1e8bb88ffc98b97a073
CURRENT_COMPLETION_SCOPE=LOCAL_NETWORK_TOUCH_PAGE_SOFTAP_ACCESS_INFO_AND_QR
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
HARDWARE_CLIENT_EVIDENCE=NOT_RUN_PENDING_HARDWARE
PLAN_STATUS=IMPLEMENTATION_COMPLETE_PENDING_INDEPENDENT_REVIEW
PLAN_FIX_VERIFICATION=PASS
IMPLEMENTATION_AUTHORIZATION=YES
IMPLEMENTATION_HEAD=2e31e1ca00df13180e3722237107fd4ed227f351
NATIVE_TESTS_HEAD=bdd1a12e3616b0f600413e856ade06edaa3e2922
SELF_CHECK_HEAD=bdd1a12e3616b0f600413e856ade06edaa3e2922
PRODUCT_IMPLEMENTATION=COMPLETE_DIGITAL
PRODUCT_TESTS=PASS_TARGETED_NATIVE
BUILDER_STATIC_ANALYSIS_SELF_CHECK=PASS
ESP32_BRINGUP_BUILD=PASS
ESP32_RELEASE_BUILD=PASS
HARDWARE_TESTS=NOT_RUN
ACTUATOR_RELEASE=NO
```

Dieser Folgeplan gilt nur für den nach PR #165 noch offenen Softwareabschluss
der lokalen Netzwerkseite. Er ist eine vollständige, eigenständige Grundlage
für diesen engeren Abschluss-Scope und ersetzt nicht rückwirkend den
gemergten Implementierungsplan oder dessen historische Evidence. Vor einer
Freigabe dieses exakten Plan-Commits werden weder Produktcode noch Tests,
ESP-IDF-Builds oder Hardwareläufe geändert beziehungsweise ausgeführt.

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
- `main/fermentation_ui_press_dispatcher.*`
- `main/app_main.cpp`, nur falls zur kurzlebigen lokalen Übergabe der bereits
  vorhandenen AP-Information zwingend erforderlich
- `docs/NETWORK.md`, nur für die nötige Klarstellung zwischen abgeschlossenem
  physischem Touch-/Kalibrierungsnachweis aus #31 und der jetzt #164
  zugeordneten Netzwerkseitenbedienung
- `docs/FUTURE_SCOPE.md` und `docs/REQUIREMENTS.md` für die synchronisierte
  `VARIANT_B`-Abgrenzung der deferierten Touch-Credentialeingabe,
  Bildschirmtastatur sowie den R1-WLAN-QR und den deferierten Webseiten-QR
- vorhandene passende Text-/UI-Contracts und Tests unter `test/`

`FermentationApplication`, `NetworkConfigurationService`,
`EspIdfNetworkLifecycle`, Persistenz und Credential-Code werden nicht geändert.
Falls die Implementierung einen bislang fehlenden fachlichen Ownervertrag
offenlegt, stoppt der Builder vor dieser Änderung und meldet den Befund.

Nicht-Ziele:

- Touchkoeffizienten, `tc0`/`tc1`, Rotation, Paneltransformation,
  Displaygeometrie, LVGL- oder Treiberauswahl;
- neue Netzwerk-, Command-, Credential-, HTTP- oder Persistenzpfade;
- Änderungen an `RecordTypeId=9`, `cc0`, Credential-Schema,
  Setup-Routen oder Test-before-commit;
- lokale HOME_WIFI-SSID-/Passworteingabe und Bildschirmtastatur (`DEFERRED`),
  ein separater Webseiten-QR sowie #27
  Auth/Session/CSRF/Weboberfläche und #28-Diagnostik;
- Hardwaretests, Flash oder Aktorfreigabe.

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

### Slice 4 – Konvergenz, Builds und Status

- Alle gezielten Tests aus Slice 1 bis 3 auf dem finalen Implementierungs-HEAD
  ausführen; danach `git diff --check` und
  `bash scripts/run_pre_ready_gates.sh self-check` auf exakt diesem HEAD.
- ESP-IDF-Profile `bringup` und `release` mit den dokumentierten
  Projektbefehlen bauen. Die Builds sind Softwareevidence; kein Flash und
  keine Hardware-/Clientbehauptung daraus ableiten.
- Nur tatsächlich benötigte Status-/Vertragsdokumentation synchronisieren,
  insbesondere `docs/NETWORK.md`, `docs/FUTURE_SCOPE.md`,
  `docs/REQUIREMENTS.md`, Roadmap, PR #171 und Issue #164. Issue #164 bleibt
  offen.
- Commits pushen und danach für unabhängige Review/Fix Verification des
  begrenzten Completion-Diffs stoppen. Kein Ready-Wechsel oder Merge.

## 5. Verifikation und offene Evidence

Abnahme nach Umsetzung:

```text
NETWORK_PAGE_TOUCH_PATH=IMPLEMENTED_NATIVE_TESTED_HARDWARE_NOT_RUN
AP_ONLY_SELECTION_UI=NATIVE_TESTED
HOME_WIFI_SELECTION_UI=NATIVE_TESTED
UNSELECTED_NOT_USER_SELECTABLE=NATIVE_TESTED
HOME_WIFI_RECONFIGURATION_UI=NATIVE_TESTED
NETWORK_MODE_COMMAND_OWNER=EXISTING_APPLICATION_PATH
SOFTAP_ACCESS_DATA_LOCAL_DISPLAY_PATH=NATIVE_TESTED
R1_HOME_WIFI_BROWSER_SETUP=RETAINED
R1_TOUCH_HOME_WIFI_CREDENTIAL_ENTRY=DEFERRED
R1_TOUCH_WIFI_KEYBOARD=DEFERRED
R1_WLAN_QR_TO_JOIN_SOFTAP=SOFTWARE_IMPLEMENTED_CLIENT_SCAN_NOT_RUN
R1_WEBSITE_QR=DEFERRED
QR_PURPOSE=JOIN_SOFTAP
QR_SOURCE=networkAccessPointInfo()
QR_PAYLOAD=INDIVIDUAL_SOFTAP_SSID_AND_PASSWORD
QR_CONTAINS_WEB_URL=NO
QR_CONTAINS_AP_IP=NO
MANUAL_FALLBACK=SSID_PASSWORD_DIRECT_IP_VISIBLE
SECOND_CREDENTIAL_SOURCE=NO
SOFTAP_SECRET_LOGGING=NO
QR_SOFTWARE_PAYLOAD_ESCAPING_CREDENTIAL_CHANGE=PASS
QR_SOFTWARE_NO_WEB_URL_OR_AP_IP=PASS
QR_320X240_RENDERER_LAYOUT=PASS
SECOND_NETWORK_STATE_MACHINE=NO
SECOND_HTTP_SERVER=NO
TOUCH_CALIBRATION_CHANGE=NO
TARGETED_NATIVE_REGRESSIONS=PASS
TEST_LOCAL_TOUCH_UI=PASS_15
TEST_PRESS_DISPATCHER=PASS_19
TEST_FERMENTATION_UI_COMMANDS=PASS_10
TEST_RENDERER_BOUNDARY=PASS_27
REPOSITORY_SECRET_SCAN=PASS
GIT_DIFF_CHECK=PASS
BUILDER_STATIC_ANALYSIS_SELF_CHECK=PASS
ESP32_BRINGUP_BUILD=PASS
ESP32_RELEASE_BUILD=PASS
HARDWARE_CLIENT_EVIDENCE=NOT_RUN_PENDING_HARDWARE
ACTUATOR_RELEASE=NO
```

Nicht Bestandteil dieser Runde sind lokale HOME_WIFI-SSID-/Passworteingabe und
Bildschirmtastatur; diese Funktionen sind durch die Ownerentscheidung
`VARIANT_B` aus R1/#164 deferiert. Der browserbasierte
Setup-/Test-before-Commit-Pfad und der WLAN-QR zum SoftAP-Beitritt bleiben
dagegen R1. Der reale Kamera-/Client-Scan des auf dem Produktdisplay
angezeigten WLAN-QR bleibt bis zur Hardwareverfügbarkeit `NOT_RUN`, ist aber
vor dem finalen Abschluss von Issue #164 nachzuholen. Ebenfalls nicht
Bestandteil dieser Runde sind reale `AP_ONLY`-/`HOME_WIFI`-Touchläufe,
Android-Client, direkter AP-IP-/mDNS-, Setup-, gespeicherter Boot- oder
Reconnect-Nachweise. Sie bleiben `NOT_RUN`, bis reale Hardware und Clients
verfügbar sind und ein passender Owner-Gate sie autorisiert. Die
Plan-Fix-Verifikation auf `OWNER_APPROVED_PLAN_SHA` war vor der autorisierten
Implementation erfolgreich; als nächstes folgt die unabhängige Review/Fix
Verification des Implementierungs-Diffs. Der reale Kamera-/Client-Scan bleibt
ein eigenes Abschluss-Gate vor Issue-Schließung.

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

```text
OWNER_APPROVAL_REQUIRED_FOR_EXACT_PLAN_SHA=NO
OWNER_APPROVED_PLAN_SHA=931db488125a6e7eca76b1e8bb88ffc98b97a073
PRODUCT_IMPLEMENTATION_BEFORE_PLAN_APPROVAL=NO
OWNER_DECISION_VARIANT_B_QR_RETAINED=RECORDED
INDEPENDENT_PLAN_FIX_VERIFICATION=PASS
INDEPENDENT_IMPLEMENTATION_REVIEW=REQUIRED
IMPLEMENTATION_AUTHORIZATION=YES
PRODUCT_IMPLEMENTATION=COMPLETE_DIGITAL
HARDWARE_CLIENT_SCAN_BEFORE_ISSUE_CLOSE=REQUIRED
PR_READY=NO
PR_MERGE=NO
ISSUE164_CLOSE=NO
ACTUATOR_RELEASE=NO
```
