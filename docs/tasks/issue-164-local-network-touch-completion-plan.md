# Plan – Issue #164: lokale Touch-Netzwerkbedienung vervollständigen

## Planstatus und Provenienz

```text
ISSUE=164
BASE_BRANCH=main
BASE_SHA=b871375f494701bed1834013cfeb789856983e3a
PR165=MERGED
PR165_MERGE_COMMIT=1f1755e5e706fb668472920545b5302fcef1df16
PREVIOUS_APPROVED_PLAN_SHA=67fbc1786b42f8ca3dc0afe65cdb4343aed2d70e
CURRENT_COMPLETION_SCOPE=LOCAL_NETWORK_TOUCH_PAGE_AND_SOFTAP_ACCESS_INFO
ISSUE164_STATUS=OPEN
HARDWARE_CLIENT_EVIDENCE=NOT_RUN_PENDING_HARDWARE
PLAN_STATUS=OWNER_APPROVAL_REQUIRED
IMPLEMENTATION_AUTHORIZATION=NO
PRODUCT_IMPLEMENTATION=NOT_STARTED
PRODUCT_TESTS=NOT_RUN_PLAN_ONLY
HARDWARE_TESTS=NOT_RUN
ACTUATOR_RELEASE=NO
```

Dieser Folgeplan gilt nur für den nach PR #165 noch offenen Softwareabschluss
der lokalen Netzwerkseite. Er ist eine vollständige, eigenständige Grundlage
für diesen engeren Abschluss-Scope und ersetzt nicht rückwirkend den
gemergten Implementierungsplan oder dessen historische Evidence. Vor einer
Freigabe dieses exakten Plan-Commits werden weder Produktcode noch Tests,
ESP-IDF-Builds oder Hardwareläufe geändert beziehungsweise ausgeführt.

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
  Diese Klarstellung wird erst nach Ownerfreigabe des exakten Plan-SHA in den
  betroffenen aktuellen Statusoberflächen synchronisiert.

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
Application-Ownerpfad.

```text
UNSELECTED -> AP_ONLY oder HOME_WIFI auswählen
AP_ONLY    -> aktuellen Modus erkennen; bei Bedarf HOME_WIFI auswählen
HOME_WIFI  -> aktuellen Modus erkennen; AP_ONLY auswählen;
              explizite HOME_WIFI-Neukonfiguration starten
UNSELECTED_NOT_USER_SELECTABLE=YES
SECOND_NETWORK_STATE_MACHINE=NO
SECOND_NETWORK_COMMAND_PATH=NO
DIRECT_ESP_IDF_CALL_FROM_UI=NO
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
SECOND_CREDENTIAL_STORE=NO
SECOND_HTTP_SERVER=NO
```

Die Anzeige bleibt eine kurzlebige lokale Projektion. Geheimnisse werden
weder in den gemeinsamen normalen UI-/Web-Snapshot aufgenommen noch in
Logs, Diagnose, Export, URL oder Persistenz kopiert. Es wird kein QR-Stack
eingeführt. Die vorhandenen Setup-Routen, der AP-/STA-Lifecycle,
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
- QR-Code, #27 Auth/Session/CSRF/Weboberfläche oder #28-Diagnostik;
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

### Slice 3 – Konvergenz, Builds und Status

- Alle gezielten Tests aus Slice 1 und 2 auf dem finalen Implementierungs-HEAD
  ausführen; danach `git diff --check` und
  `bash scripts/run_pre_ready_gates.sh self-check` auf exakt diesem HEAD.
- ESP-IDF-Profile `bringup` und `release` mit den dokumentierten
  Projektbefehlen bauen. Die Builds sind Softwareevidence; kein Flash und
  keine Hardware-/Clientbehauptung daraus ableiten.
- Nur tatsächlich benötigte Status-/Vertragsdokumentation synchronisieren,
  insbesondere Roadmap, PR #164 und Issue #164. Issue #164 bleibt offen.
- Commits pushen und danach für unabhängige Review/Fix Verification des
  begrenzten Completion-Diffs stoppen. Kein Ready-Wechsel oder Merge.

## 5. Verifikation und offene Evidence

Abnahme nach Umsetzung:

```text
NETWORK_PAGE_TOUCH_PATH=PASS
AP_ONLY_SELECTION_UI=PASS
HOME_WIFI_SELECTION_UI=PASS
UNSELECTED_NOT_USER_SELECTABLE=PASS
HOME_WIFI_RECONFIGURATION_UI=PASS
NETWORK_MODE_COMMAND_OWNER=EXISTING_APPLICATION_PATH
SOFTAP_ACCESS_DATA_LOCAL_DISPLAY_PATH=PASS
SOFTAP_SECRET_LOGGING=NO
SECOND_NETWORK_STATE_MACHINE=NO
SECOND_HTTP_SERVER=NO
TOUCH_CALIBRATION_CHANGE=NO
TARGETED_NATIVE_REGRESSIONS=PASS
GIT_DIFF_CHECK=PASS
BUILDER_STATIC_ANALYSIS_SELF_CHECK=PASS
ESP32_BRINGUP_BUILD=PASS
ESP32_RELEASE_BUILD=PASS
HARDWARE_CLIENT_EVIDENCE=NOT_RUN_OWNER_HARDWARE_UNAVAILABLE
ACTUATOR_RELEASE=NO
```

Nicht Bestandteil dieser Runde sind reale `AP_ONLY`-/`HOME_WIFI`-Touchläufe,
Android-Client, direkter AP-IP-/mDNS-, Setup-, gespeicherter Boot- oder
Reconnect-Nachweise. Sie bleiben `NOT_RUN` bis reale Hardware und Clients
verfügbar sind und ein passender Owner-Gate sie autorisiert. Der unabhängige
Review dieses Folgeplans muss vor jeglicher Produktimplementation erfolgen.

## 6. Quellen und Owner-Gates

- `docs/AGENT_WORKFLOW.md`, `docs/ENGINEERING_PRINCIPLES.md`
- `docs/NETWORK.md`, `docs/ROADMAP.md`
- `docs/tasks/issue-164-r1-wlan-native-http-integration-plan.md` als
  unveränderte Historie und Referenz der bestehenden #164-Verträge
- Issue #164 sowie der gemergte PR #165
- ADR-013 und die bestehenden Touch-/Renderer-/Network-Tests auf
  `BASE_SHA=b871375f494701bed1834013cfeb789856983e3a`

```text
OWNER_APPROVAL_REQUIRED_FOR_EXACT_PLAN_SHA=YES
PRODUCT_IMPLEMENTATION_BEFORE_PLAN_APPROVAL=NO
PR_READY=NO
PR_MERGE=NO
ISSUE164_CLOSE=NO
ACTUATOR_RELEASE=NO
```
