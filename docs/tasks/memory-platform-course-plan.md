# R1-RAM-Stabilisierung ESP32-WROOM-32E ohne PSRAM

Status: Planvorschlag zur Ownerfreigabe (Plan-PR #174, keine Produktionscodeänderung)
Datum: 2026-10-02
Plan-Basis: `origin/main` `7e3948652453ae97eede07956af466ef6dddb602`
Konsolidiert aus zwei Korrekturaufträgen zu PR-#174-HEAD
`52825fbfba22d8a0d79ea3d9f7b630d8d7ba6017` („PR #174 Plan korrigieren“ und
„Korrekturauftrag zum Optimierungsplan“, Blocker B1–B4).

## 1. Ziel und Nicht-Ziele

Ziel ist ein messungsgetriebener Nachweis, dass die vorhandene R1-Hardware
(ESP32-WROOM-32E, 4 MB Flash, ohne PSRAM) den R1-Funktionsumfang mit
begründeter RAM-Reserve trägt, und die Behebung der dafür nachweislich
nötigen, kleinsten Ursachen.

Reihenfolge:

```text
Baseline auf aktuellem Stand
-> kleinste Hotspot-Korrekturen, je einzeln neu gemessen
-> Re-Messung und begründeter System-Mindestabstand
-> WLAN-Tuning nur falls nötig
-> repräsentativer Hardware-Lasttest
-> erst bei verbleibender Lücke: struktureller UI-Umbau (eigene Planrevision)
```

Nicht-Ziele dieses Plans:

- keine neuen ADRs und keine Parallelverträge zu
  `docs/ENGINEERING_PRINCIPLES.md`, `docs/RESOURCE_BUDGET_AND_MAINTENANCE.md`
  und ADR-008;
- kein ESP32-S3-/PSRAM-Profil, kein `esp32s3`-Build, keine generische
  Speicherplatzierungsschnittstelle;
- keine harten Subsystembudgets vor der Baseline;
- keine Asynchronisierung des Netzwerkmodus-Commits und keine
  Main-Stack-Reduktion als Pflichtschritt;
- keine Flash-Core-Dump-Partition und keine Partitionsänderung;
- keine Prozess- oder Governanceänderung (PR-Größenlimit, RAM-Schätzpflicht,
  neue AGENTS-Regeln, Umbau der PASS-Key-/Gate-Governance);
- keine Merge-, Rebase- oder Supersede-Entscheidung zu PR #170.

## 2. Verifizierte Ausgangslage

### 2.1 Historische Messanker (nicht vergleichbar)

Die folgenden Werte stammen aus verschiedenen Firmware-, Konfigurations- und
Integrationsständen. Ihre Differenzen sind **keine** isolierte Messung eines
Subsystems (insbesondere sind 231444 B − 89036 B keine gemessenen 142 KB
UI-Verbrauch).

| Wert | Stand | Quelle |
|---|---|---|
| `free_heap_bytes=304764` | Issue-#74-Stand, vor ESP-IDF 6.1 | `docs/tasks/issue-74-implementation-plan.md:366` |
| `free_heap_bytes=231444` | Issue #159, ESP-IDF 6.1, ohne LVGL-UI und WLAN | `docs/audits/ISSUE_159_ESP_IDF_6_1_UPGRADE_EVIDENCE.md:287-288` |
| `free_heap_bytes=89036` | Issue-#31-Text-/WLAN-Smoke `717776c` | `docs/audits/ISSUE_31_TEXT_WIFI_SMOKE_20260924_717776C_READABLE.txt:84,115` |
| `free_heap≈12,1–12,2 kB`, `minimum_free_heap_bytes=2576`, größter Block 5120 B | PR-#170-Retest, Firmware `099ba8a981124f500cc85cd1b3e89a83a928467c`, `CONFIG_ESP_MAIN_TASK_STACK_SIZE=24576` | kanonische PR-#170-Roadmap/Handover-Evidence; PR-#170-HEAD beim Review `6fca5ebb17afcea0056a685be04b7e2a54a8b790` |

Die frühere Aussage „ca. 304 KB Basis, daraus 73 KB Kern+App“ ist auf aktuellem
Stand nicht reproduzierbar belegt und gilt nur als unbestätigte Schätzung.

### 2.2 Belegte Fehlerbilder

- PR-#170-Retest (`099ba8a`): `abort()` nach ca. 37 s ohne Bedienung;
  Backtrace `operator new` → `std::vector<ScreenDrawCommand>::push_back` in
  `makeRepresentativeScreen` (`main/fermentation_ui_renderer.cpp`). Belegt ist
  die **Absturzstelle** (fehlgeschlagene Allokation im UI). Die
  **Gesamtsystemursache** der Heap-Erschöpfung ist nicht belegt; die
  Web-/Auth-Komposition aus PR #170 ist dadurch weder be- noch entlastet.
- Issue-#164-Ownertest auf Firmware `e9f1f8b` (PR-#171-Stand, später in
  `main` gemergt; danach u. a. `722adcd` im Resource-Sampling-Kontrollfluss):
  acht `SW_CPU_RESET`, davon sechs bei Allokationen im Netzwerkmodus-Commit
  und zwei beim Aufbau des Netzwerkseiten-Zeichenmodells
  (`docs/tasks/issue-164-owner-hardware-test-evidence-2026-09-29.md`).
  Das RAM-Problem besteht damit bereits auf `main`, unabhängig von PR #170.

### 2.3 Verifizierte Code-Hotspots auf `main`

1. `FermentationApplication::uiPresentationSource()`
   (`lib/fermentation_app/src/fermentation_application.cpp:1021-1040`) kopiert
   `runtime.lease.get().programCatalog()` in
   `FermentationUiPresentationSource::programCatalog` (by value,
   `fermentation_ui_models.hpp:132-136`). `main/app_main.cpp:396,447` ruft das
   in jedem Schleifendurchlauf auf.
2. `ProductiveLvglRenderer::render()`
   (`main/fermentation_ui_lvgl_renderer.cpp:304-331`) baut
   `makeRepresentativeScreen()` (Vektor aus `ScreenDrawCommand` mit Strings)
   bei **jedem** Aufruf und prüft erst danach den unveränderten Render-Key.
   Der Dispatcher (`main/fermentation_ui_press_dispatcher.cpp:100-125`) baut
   das Modell nur bei gehaltenem Kontakt (`contactHeld`); die frühere Aussage
   „zweimal alle 10 ms“ war falsch.
3. Die Hauptschleife (`main/app_main.cpp:614-646`) ruft `platform.update()`,
   `application.update()` und `updateProductUi()` im selben Takt auf und gibt
   je Durchlauf `kCooperativeYieldTicks = 1` ab. Ein UI-Takt existiert nicht
   getrennt.
4. `EspIdfDisplayTouchAdapter` reserviert bei Initialisierung dauerhaft
   `kPartialBufferPixels = 320U * 8U` Pixel DMA-Speicher
   (`lib/device_platform_esp_idf/src/esp_idf_display_touch_adapter.cpp:29,232-233`),
   auch wenn im Produktpfad LVGL rendert.
5. LVGL-Port-Konfiguration (`main/fermentation_ui_lvgl_renderer.cpp:263-281`):
   `buffer_size = 320*20`, `trans_size = 320*20`, `buff_dma = 1`,
   `buff_spiram = 0`. Gepinnt: `espressif/esp_lvgl_port 2.9.0`,
   `lvgl/lvgl 9.6.0~1`, ESP-IDF `6.1.0` (`dependencies.lock`).
   `sdkconfig.defaults` setzt keinen LVGL-Allocator/-Poolwert; der effektiv
   generierte Wert ist noch nicht dokumentiert.
6. WLAN- und HTTP-Start erfolgen innerhalb von `application.begin()` →
   `beginPersistent()` → `initializeNetwork()`
   (`fermentation_application.cpp:655-705,1143`), also **vor**
   `initializeProductUi()` (`main/app_main.cpp:590`). Der einzige Bootmesspunkt
   `startup` (`main/app_main.cpp:603`) liegt nach allem.
7. Vor dem Render-Key entsteht in `updateProductUi()` je Schleifendurchlauf
   weitere dynamische C++-Arbeit:
   - `application.uiSnapshot()` (`main/app_main.cpp:412`):
     `FermentationUiProjectionInput::temperatures` ist ein `std::vector`
     (`fermentation_ui_projector.hpp:13`); `FermentationUiProjector::project()`
     reserviert und befüllt `output.temperatures` und ggf. `output.messages`
     (`fermentation_ui_projector.cpp:30-38`, `fermentation_ui_models.hpp:117-118`);
   - `application.networkAccessPointInfo()` (`main/app_main.cpp:374,459`)
     liefert `std::optional<NetworkAccessPointInfo>` by value; die Struktur
     enthält `std::string ssid` und `std::string password`
     (`network_lifecycle.hpp:41-44`), `EspIdfNetworkLifecycle::accessPointInfo()`
     kopiert diese Strings;
   - `ClockViewInput` (`device_ui_contracts.hpp:99-102`) enthält eine
     `TimeZoneId` und wird by value gebildet (`main/app_main.cpp:410`).

### 2.4 Bestehende Verträge und Werkzeuge (werden wiederverwendet)

- `docs/ENGINEERING_PRINCIPLES.md` „Ressourcenbudgets aus Messung und
  Produktkontext“: frühe Subsystembudgets sind Planungs-/Warnwerte; harte
  Grenzen nur begründet, gemessen und ownerfreigegeben; Gesamtsystembewertung
  inkl. Main-Task-HWM, Heap, größter Block, IRAM/DRAM; WLAN, LVGL und Web
  benötigen nach Integration eigene Ressourcenqualifikation.
- `docs/RESOURCE_BUDGET_AND_MAINTENANCE.md`: überwachte Ressourcen,
  `EARLY_SUBSYSTEM_BUDGETS=PLANNING_AND_WARNING_VALUES`, begrenzte
  dynamische Web-/JSON-Nutzung, „nachgewiesene Mindestreserve“ vor Laufstart.
- ADR-008: Release 1 ohne PSRAM-Voraussetzung, 4 MB Flash.
- `INetworkLifecycle` besitzt nur flüchtigen Transportzustand
  (`lib/device_platform/src/network_lifecycle.hpp:86-88`); Preview,
  persistenter Commit und Runtime-Publish liegen beim Application-/
  `ConfigurationService`-Pfad.
- `RuntimeConfigurationReadLease` erlaubt mehrere gleichzeitige Leser,
  begrenzt durch `configuration_limits::kMaxRuntimeConfigurationReadLeases`
  (aktuell 8, `configuration_limits.hpp:51`). Bei ausgeschöpfter Kapazität
  liefert `acquireRuntime()` `RuntimeReadLeaseBusy`
  (`configuration_service.cpp:635`). Der UI-Cache hält keine Runtime-Lease
  dauerhaft.
- `FermentationApplication::uiSnapshot()` füllt bereits
  `FermentationUiSnapshot::revisions.expectedUserConfigurationRevision` und
  `expectedProgramCatalogRevision` (`fermentation_application.cpp:978-986`);
  `main/app_main.cpp:412` erzeugt den Snapshot je Schleifendurchlauf.
- `FermentationTouchWorkspace::view()` → `makePageView()` baut dynamische
  View-Daten, auf der Programmseite zusätzlich
  `makeFermentationUiProgramList()` (`fermentation_touch_workspace.cpp:424-442,789-792`).
  `makeScreenRenderKey()` (`main/fermentation_ui_renderer.cpp:602-627`)
  kopiert u. a. Strings (`locale`, `confirmationProgramName`) aus dem fertigen
  Screen-Modell.
- `scripts/build_report.py` erzeugt bereits `idf.py size --format json2` und
  wertet DRAM/IRAM aus.
- `logResources()` (`main/app_main.cpp:237-254`) protokolliert freien Heap,
  Minimum, größten 8-Bit-Block und Main-Task-HWM an den Punkten `startup`,
  `periodic_30s`, `stable_*` und `network_page_press_before/after`.

## 3. Messprotokoll (gilt für Baseline und jede Re-Messung)

Jede Hardwaremessung ist nachvollziehbar durch:

- Firmware-Quell-SHA, Profil `esp32_release`, frischer Build aus
  `sdkconfig.defaults` + `sdkconfig.defaults.release`, SHA-256 von App-BIN und
  ELF;
- effektive Konfiguration: Main-Stack, LVGL-Allocator und Poolgröße,
  WLAN-/lwIP-Puffer, `CONFIG_SPIRAM` (aus dem generierten `sdkconfig`);
- Gerät, NVS-Zustand (gelöscht/nicht gelöscht), Netzwerkmodus und
  Netzwerkzustand je Messpunkt, Zeitstempel seit Boot;
- UART-Rohmitschnitt im Repository oder als PR-Anhang.

Gemessen werden:

| Größe | Quelle |
|---|---|
| statisches DRAM/IRAM (Build) | bestehender `scripts/build_report.py` (JSON2) |
| freier Heap, Minimum-Free-Heap, größter Block, je getrennt für `MALLOC_CAP_INTERNAL \| MALLOC_CAP_8BIT` und `MALLOC_CAP_DMA` | `logResources()` |
| Main-Task-Stack-HWM | `logResources()` |
| LVGL-Poolbelegung, -maximum und -Fragmentierung, nur falls der eingebaute LVGL-Allocator aktiv ist | `lv_mem_monitor()` unter `lvgl_port_lock()` |

Überlappende Heap-Capabilities werden nicht addiert. Eine Zuordnung zu
Subsystemen erfolgt nur aus Differenzen vergleichbarer Messpunkte desselben
Laufs.

Messpunkte in tatsächlicher Bootreihenfolge:

1. `after_platform_begin`: vor `application.begin()`;
2. `after_application_begin`: nach Application-Boot **einschließlich**
   WLAN-/HTTP-Start (siehe Abschnitt 2.3, Punkt 6);
3. `after_ui_init`: nach LVGL-/Display-Initialisierung;
4. `stable_ap_only` bzw. `stable_home_wifi`: bestehender Punkt nach erreichtem
   stabilen Netzwerkzustand;
5. `periodic_30s` und ein zusätzlicher Idle-Punkt nach 120 s ohne Bedienung;
6. repräsentativer R1-Lastpfad (Ownerentscheidung O2, Vorschlag):
   10 Seitenwechsel über alle Touch-Seiten, Sprachwechsel, 5 Netzwerkmodus-
   wechsel AP_ONLY ↔ HOME_WIFI, ein Browserzugriff auf die vorhandene
   Netzwerk-Setup-Seite; danach 120 s Idle. Mit Punkten vor/nach jedem
   Moduswechsel (bestehende `network_page_press_*`).

Application-Boot und WLAN-/HTTP-Start werden am kombinierten Messpunkt
`after_application_begin` erfasst. Die Differenz zu `after_platform_begin`
beschreibt ihre gemeinsame Speicherwirkung. Der `stable_*`-Punkt erfasst den
späteren stabilen Gesamtzustand. Diese Messpunkte liefern keine isolierte
WLAN-Verbrauchsmessung. Für die vorgesehene Systembaseline genügt der
kombinierte Punkt; eine zusätzliche Zerlegung ist nicht Bestandteil dieses
Scopes.

## 4. Fehlende Regel und Budgetstatus

### 4.1 Minimale Ergänzung bestehender Quellen

Nachweislich fehlt nur eine Regel für den Steady-State-Pfad außerhalb des
Regelkerns. Web/JSON-Grenzen, Main-Task-HWM und Teilpfad-Evidenz sind
bereits geregelt. Ergänzung in `docs/RESOURCE_BUDGET_AND_MAINTENANCE.md`,
Abschnitt „Begrenzte Speicherstrukturen“, neuer Unterabschnitt (Commit S1):

```text
### Hauptschleife und lokale UI

Im hochfrequenten Main-/UI-Pfad erfolgt nach dem Warm-up keine wiederholte
dynamische Allokation ohne sichtbare Zustandsänderung. Ereignisbezogene
Allokationen (Seiten-, Sprach-, Konfigurations- oder Netzwerkmoduswechsel)
bleiben zulässig, wenn sie begrenzt sind. Ein Zähler für C++-`operator new`
im Host-Test belegt nur die getesteten C++-Pfade; `malloc` aus C-, ESP-IDF-,
LVGL- oder cJSON-Code und LVGL-Poolallokationen benötigen eine
ESP32-Laufzeitmessung. Statische `idf.py size`-Werte belegen keinen
Laufzeit-Heap und keine Fragmentierung.
```

Request-Handler werden bewusst nicht in diese Regel aufgenommen; für sie gilt
weiterhin „Web, JSON und Exporte“ mit begrenzter request-lokaler Allokation.

### 4.2 Budgetwerte

Die früher vorgeschlagenen Werte (Subsysteme 75/75/45/20 KB; 60 KB
Idle-Reserve; 40 KB Laufzeitminimum; 16 KB größter Block) sind nur zu prüfende
Diagnose- und Planungswerte. Es wird keine Freigabe dieser Zahlen verlangt.

Nach Abschluss von S2–S7 leitet S8 aus den gemessenen Minima und Spitzen
einen begründeten **System-Mindestabstand** (freier Heap und größter Block,
intern und DMA getrennt) ab. Erst dieser Wert wird dem Owner zur Freigabe
vorgelegt (O4) und ersetzt dann den in
`RESOURCE_BUDGET_AND_MAINTENANCE.md` genannten Begriff „nachgewiesene
Mindestreserve“ durch eine Zahl. Ein automatisches CI-Gate für statisches
DRAM ist nicht Teil dieses Plans; statische und Laufzeitwerte bleiben getrennt.

## 5. Umsetzungs- und Commit-Schnitte

Nach jedem Commit wird angehalten. Schritte mit Hardwaremessung erfordern den
Owner am Gerät. Jede Messung folgt Abschnitt 3.

| Schnitt | Inhalt | Nachweis |
|---|---|---|
| S1 | Regelergänzung aus 4.1 in `RESOURCE_BUDGET_AND_MAINTENANCE.md` | Doku-Diff |
| S2 | Messbasis, nur Diagnose-Instrumentierung ohne Verhaltensänderung (daher misst die Baseline auf `main` + S2): `logResources()` um interne/DMA-Werte und optional LVGL-Pool erweitern; Messpunkte 1–3 und 120-s-Idle ergänzen; `heap_caps_register_failed_alloc_callback()` mit allokationsfreier Ausgabe (nur `esp_rom_printf`/`ESP_DRAM_LOGE`, Größe, Caps, Funktionsname); falls nötig `build_report.py` um `idf.py size --format json2` je Komponente erweitern (genaue Optionsform gegen gepinnte IDF 6.1 im Schnitt verifizieren) | Build beider Profile; Baseline-Hardwarelauf auf dem S2-Stand, Evidenzdatei unter `docs/audits/` |
| S3 | Hotspot 1: Katalogkopie mit explizitem Befüllungsergebnis | Host-Test + Re-Messung |
| S4 | Hotspot 2: Render-Key vor Screen-Modell; allokationsfreier unveränderter `updateProductUi()`-Pfad | Host-Test + Re-Messung |
| S5 | Hotspot 3: UI-Rendern vom Schleifentakt entkoppeln | Host-Test + Re-Messung inkl. Touch-Reaktion |
| S6 | Hotspot 4: Adapter-DMA-Puffer im LVGL-Produktpfad vermeiden | Display-Smoke + Re-Messung |
| S7 | Hotspot 5: `trans_size` prüfen | Display-Smoke + Re-Messung |
| S8 | Auswertung, abgeleiteter System-Mindestabstand, Entscheidung über S9–S11 | Evidenzdokument, Ownerfreigabe O4 |
| S9 | bedingt: WLAN-Speicherprofil | siehe 5.6 |
| S10 | bedingt: Stack-/Commitpfad | siehe 5.7 |
| S11 | bedingt: struktureller UI-Umbau | eigene Planrevision |

### 5.1 S3 – Katalogkopie

Die Invalidierung stützt sich auf die bestehenden
`FermentationUiSnapshot::revisions`:

- `expectedUserConfigurationRevision` für Sprache und Zeitzone;
- `expectedProgramCatalogRevision` für den Katalog.

Ablauf je Schleifendurchlauf in `app_main`: zuerst den bestehenden Snapshot
(`application.uiSnapshot()`) erzeugen, dann dessen beiden Revisionen mit den
zuletzt erfolgreich übernommenen Revisionen der Konsumentenkopie vergleichen.
Nur bei Abweichung wird `uiPresentationSource()` erneut aufgerufen und kopiert.
`ConfigurationService::stateRevision()` wird nicht neu durch
`FermentationApplication` durchgereicht. `loopPresentation` bleibt die eine
Konsumentenkopie; Runtime-Leases bleiben kurzlebig innerhalb von
`uiSnapshot()`/`uiPresentationSource()`.

Randfälle:

- Erstbefüllung: Die Kopie gilt erst als gültig, nachdem eine Befüllung mit
  erteilter Runtime-Lease gelungen ist; bis dahin wird bei jedem Durchlauf
  erneut versucht.
- Nicht verfügbare Revisionen (`std::nullopt`, z. B. `RuntimeReadLeaseBusy`
  oder Runtime nicht verfügbar): Der Vergleich gilt als nicht entscheidbar;
  die bisherige Kopie bleibt in Gebrauch, die gespeicherten Revisionen werden
  nicht überschrieben, und der nächste Durchlauf vergleicht erneut.
- Fehlgeschlagene Befüllung: Sie zählt nicht als erfolgreich aktualisierter
  Cache; gespeicherte Revisionen und bisherige Kopie bleiben, der nächste
  Durchlauf versucht erneut.

Erfolgsnachweis der Befüllung: Cache-Revisionen werden nur nach **explizit
nachgewiesen erfolgreicher** Befüllung übernommen. Der heutige Vertrag kann
das nicht ausdrücken: `FermentationApplication::uiPresentationSource()` gibt
immer eine `FermentationUiPresentationSource` zurück und liefert ohne
erteilte Runtime-Lease nur eine Default-Quelle ohne Status
(`fermentation_application.cpp:1021-1040`). S3 passt diesen Rückgabevertrag
deshalb minimal an ein explizites Success/Unavailable-Ergebnis an
(z. B. `std::optional<FermentationUiPresentationSource>`), ohne zweiten
Config-Owner und ohne neue Cache-Architektur. Bestehende Aufrufer
(`main/app_main.cpp:396,447,585`) werden auf das neue Ergebnis umgestellt.
Es gibt keine Heuristik über leeren Katalog, Default-Locale oder andere
Nutzdaten.

Eviction auf `FermentationUiPage::HeaderNetwork`: Der heutige Produktpfad hält
auf `HeaderNetwork` bewusst keinen vollständigen `ProgramCatalog` im Speicher
(`loopPresentation` ist lokal; bei `networkPageBeforeTouch == true` wird
`uiPresentationSource()` nicht aufgerufen, Renderer erhält `catalog == nullptr`,
`main/app_main.cpp:392-407,445-455`). Die eine S3-Konsumentenkopie darf diese
Eigenschaft nicht aufheben und den Katalog nicht über `HeaderNetwork` hinweg
halten, weil sonst zusätzlicher Heap im bekannten Netzwerkmodus-RAM-Peak
gebunden wäre:

- Beim Eintritt in `HeaderNetwork` wird die gecachte Presentation-/Katalogkopie
  vor dem Netzwerkmodus-Commit verworfen und freigegeben; die gespeicherten
  Revisionen gelten ab dann als ungültig.
- Auf `HeaderNetwork` bleibt der bestehende `catalog == nullptr`-Pfad erhalten.
- Beim Verlassen der Netzwerkseite wird die Kopie über den normalen expliziten
  Success/Unavailable-Vertrag erneut befüllt; erst nach Success werden die
  Revisionen wieder als gültig übernommen. Bei Unavailable bleiben sie
  ungültig, und der nächste Durchlauf versucht erneut.
- Kein zweiter Cache, kein zweiter Katalog-Owner, keine neue Zustandsmaschine:
  Eviction und Neubefüllung nutzen dieselbe eine Konsumentenkopie und deren
  Gültigkeitsmerkmal.

Test (im Implementierungsschnitt): Änderung von
`expectedProgramCatalogRevision` aktualisiert den Katalog; Änderung von
`expectedUserConfigurationRevision` aktualisiert Sprache und Zeitzone;
unveränderte Revisionen rufen `uiPresentationSource()` nicht auf;
Erstbefüllung und nicht verfügbare Revision übernehmen keine Revision als
aktualisiert; Runtime-Lease nicht verfügbar → `uiPresentationSource()`
meldet Unavailable, bestehende Konsumentenkopie und gespeicherte Revisionen
bleiben unverändert, und der nächste Durchlauf versucht erneut.

Zusätzlich gezielt für die `HeaderNetwork`-Eviction:

1. Eintritt in `HeaderNetwork`: gecachter `ProgramCatalog` ist nicht mehr
   lebend, der Renderer erhält `nullptr`.
2. Der Netzwerkmodus-Commit hält keine zusätzliche S3-Katalogkopie.
3. Verlassen von `HeaderNetwork`: erfolgreiche Neubefüllung stellt Locale,
   Zeitzone und Katalog wieder her.
4. Unavailable beim Verlassen: alte Revisionen werden nicht fälschlich als
   aktuell markiert; der nächste Durchlauf versucht erneut.

### 5.2 S4 – Render-Key vor Screen-Modell und allokationsfreier UI-Steady-State

Der Render-Key wird aus den Eingaben von `render()` gebildet, bevor
`makeRepresentativeScreen()` aufgerufen wird. Er muss eine Obermenge aller
sichtbaren Eingaben sein (Snapshot-/Refresh-Revision, Workspace-Seite,
Pager, Dialog, gedrücktes Ziel, Locale, Katalogrevision, Netzwerkstatus,
Uhrzeit in Anzeigeauflösung, AP-Info). Bei gleichem Key wird kein Modell
gebaut.

Bildung, Speicherung und Vergleich des vorgezogenen Keys erfolgen im
unveränderten Steady State ohne dynamische Allokation:

- Für den Key werden weder `FermentationTouchWorkspace::view()` noch
  `makeFermentationUiProgramList()` aufgerufen, weil `view()` dynamische
  View-Daten und auf der Programmseite die Programmliste erzeugt.
- Der Key enthält keine kopierten `std::string`-Werte, wie sie
  `makeScreenRenderKey()` heute aus dem Screen-Modell übernimmt. Er wird aus
  vorhandenen allokationsfreien Werten gebildet (Revisionen, Enum-/Index-/
  Zählerwerte, Flags, feste Arrays). Kann ein sichtbarer Wert nur über einen
  String verglichen werden, wird er über eine vorhandene Revision abgedeckt.
- Reichen vorhandene allokationsfreie Werte für renderrelevante
  Workspace-Mutationen nicht aus, erhält `FermentationTouchWorkspace` eine
  einzelne kleine monotone Presentation-/Render-Revision, die bei jeder
  renderrelevanten Mutation erhöht wird. Sie dient nur der
  Cache-Invalidierung, nicht als fachlicher Zustand.
- Die vollständige Abdeckung aller sichtbaren Eingaben bleibt Pflicht.

Der Nachweis gilt nicht nur für den Key, sondern für den vollständigen
unveränderten `updateProductUi()`-Steady-State. Vor dem Key entsteht heute
weitere dynamische C++-Arbeit (Abschnitt 2.3, Punkt 7: Snapshot-Erzeugung mit
Temperatur-/Meldungsvektoren, AP-Info mit `ssid`/`password`-Strings by value,
`ClockViewInput` mit `TimeZoneId`). Deshalb gilt:

- Nach Warm-up und ohne sichtbare Zustandsänderung erfolgt keine wiederholte
  C++-Heap-Allokation im gesamten lokalen UI-Kompositionspfad
  (`updateProductUi()` bis zum Early-Return des Renderers).
- Touch-Polling und Application-/Safety-Takt bleiben unverändert.
- Ereignisbezogene Allokationen bei tatsächlicher sichtbarer Änderung bleiben
  gemäß Regel 4.1 zulässig und begrenzt.
- Der Schnitt wählt je Quelle die kleinste Korrektur; es gibt keine
  vorsorgliche Umstellung aller UI-Container.
- Erlauben die vorhandenen Modelle diesen Nachweis nicht mit kleinem Delta,
  wird nur der betroffene Teil als gemessener struktureller UI-Umbau in S11
  geplant.
- C-, ESP-IDF- und LVGL-Allokationen bleiben über die Hardware-Laufzeitmessung
  (Abschnitt 3) abgedeckt.

Test (im Implementierungsschnitt):

- unveränderter Zustand: weder Aufbau eines Screen-Modells noch
  C++-Heap-Allokation im lokalen UI-Kompositionspfad, belegt mit einem
  zählenden `operator new` im Host-Test. Der Nachweis umfasst mindestens
  Snapshot-Erzeugung und Cacheentscheidung (S3), AP-Info-Zulieferung,
  `ClockViewInput` und Render-Key (Bildung, Speicherung, Vergleich) bis zum
  Early-Return;
- je sichtbarer Eingabe und je renderrelevanter Workspace-Mutation ein Fall,
  der eine Key-Änderung und damit Neuzeichnen nachweist.

### 5.3 S5 – UI-Takt

Nur das Rendern wird begrenzt: neu gezeichnet wird bei geändertem Key aus S4
und höchstens mit einem UI-Takt (Startwert 50 ms, im Schnitt gemessen).
Touch-Polling und Press-Edge-Erkennung (`pollTouch()`/`routePress()` aus #31)
sowie `platform.update()` und `application.update()` bleiben im bestehenden
Schleifentakt. Die kooperative Application-Schleife wird nicht verlangsamt.

Test: kein verlorener Press-Edge bei gedrosseltem Rendern; Hardware: subjektive
und geloggte Touch-Reaktionszeit vor/nach.

### 5.4 S6 – Adapter-DMA-Puffer

Der `320*8`-DMA-Puffer wird nicht mehr bei Initialisierung reserviert, sondern
von den direkten Fill-/Flush-Pfaden des Adapters bedarfsgerecht besessen
(z. B. bei erster Nutzung angelegt und nach direktem Zeichnen freigegeben).
Der produktive LVGL-Pfad nutzt ihn nicht. Vorher wird geprüft, welche Pfade
(Bring-up, Kalibrierharness) ihn verwenden.

### 5.5 S7 – `trans_size`

Anhand der gepinnten `esp_lvgl_port 2.9.0`-Quelle wird geprüft, ob bei
`buff_dma=1`, `buff_spiram=0` ein separater Transferpuffer mit `trans_size`
angelegt wird. Nur wenn ja und er entbehrlich ist, wird `trans_size` entfernt
(0). Änderung nur nach Hardware-Display-Smoke. Die LVGL-Zeilenpuffergröße
`320x20` bleibt unverändert; eine Halbierung wäre nur nach gemessenem
RAM-Gewinn gegen Refresh-/Touch-Reaktionszeit eine eigene Ownerentscheidung.

### 5.6 S9 – WLAN-Speicherprofil (bedingt)

Nur falls S8 eine Lücke zeigt. Keine Einzelwerte vorab. Geprüft werden die
offiziellen ESP-IDF-6.1-Kandidaten „Memory saving“/„Minimum“ als
zusammenhängende Sätze einschließlich gekoppelter WLAN-RX-/TX-Puffer und
TCP-Sende-/Empfangsfenster. Danach AP_ONLY, HOME_WIFI, HTTP und Sessionlast
real qualifizieren. Jede `sdkconfig`-Änderung ist eine Ownerentscheidung.

### 5.7 S10 – Stack und Netzwerkmodus-Commit (bedingt)

Zuerst werden Main-Task-HWM und der synchrone Commit-/Fehlerpfad nach S3–S7
gemessen. Nur bei weiter nachgewiesenem Stack- oder Spitzenproblem folgt eine
eigene Planrevision. Rahmen dafür:

- Preview, persistenter Commit und Runtime-Publish bleiben beim bestehenden
  Application-/`ConfigurationService`-Pfad; `INetworkLifecycle` bleibt
  flüchtiger Transport. Keine zweite Commit- oder Netzwerkzustandsmaschine.
- Wird Asynchronität gewählt, braucht der Slice einen konkreten Vertrag:
  Zuständigkeit, begrenzte Kommandokapazität, Busy/Fehler/
  `CommitIndeterminate`, persistenter Linearisierungspunkt, anschließende
  Transportaktivierung und Sessionwiderruf.
- Ein kleinerer Main-Stack zählt nur netto als Ersparnis, nach Abzug eines
  neuen Workerstacks und seiner temporären Daten. 16 KB werden erst nach
  Messung des gesamten Commit-/Fehlerpfads geprüft.

### 5.8 S11 – struktureller UI-Umbau (bedingt)

Retained Widgets, feste `ScreenDrawCommand`-Arrays, geteilte Styles,
Text-/Katalogdaten im Flash oder LVGL-Pool-Anpassungen werden nur geplant,
wenn S3–S7 die Reserve aus S8 nicht herstellen, und nur im gemessen nötigen
Umfang. Keine pauschale Umstellung gemeinsamer Modellcontainer.

## 6. Diagnoseumfang

- Die Messbasis benötigt nur UART/Backtrace mit ELF, die erweiterten
  Ressourcenlogs und den allokationsfreien Failed-Alloc-Callback.
- Ein persistenter Flash-Core-Dump ist nicht Teil dieses Plans. Bei späterem
  Bedarf braucht er eine eigene Bewertung: Dumpumfang, Tasks und Stacks,
  Diagnose-RAM, App-Image-Grenze, Erhalt der `state_store`-Offsets
  (`partitions/issue_90_state_store.csv`), und Abgrenzung zu Secrets, weil
  Taskstacks lokale `wifi_config_t`-Objekte mit Passwortdaten enthalten können
  (`esp_idf_network_lifecycle.cpp:171,192`), inkl. Schutz, Zugriff und Löschung.

## 7. Verhältnis zu PR #170

PR #170 ist abhängige Arbeit: keine weitere Featureausweitung, bis die
R1-RAM-Basis (S8) stabil ist. Für dessen Ressourcenabnahme gilt unverändert
das zehnminütige Vier-Session-/Replay-Ressourcengate aus
`docs/tasks/issue-27-replay-resource-plan-revision-2026-10-01.md` (PR-#170-
Branch). Hinweise für dort, ohne Vorwegnahme:

- `web_json_codec.cpp::serializeBounded()` nutzt bereits
  `cJSON_PrintPreallocated()`; verbleibende Allokationen entstehen im
  cJSON-Baum und in der Ergebnis-`std::string`;
- HTTP-Socketanzahl und logische Sessions sind verschieden; die
  Lastverträglichkeit ist gesondert zu prüfen.

Merge, Rebase oder Supersede von PR #170 entscheidet dieser Plan nicht.

## 8. Tests und Dokumentationswirkung

- Host: gezielte native Tests der geänderten Bereiche je Schnitt
  (`test_renderer_boundary`, `test_press_dispatcher`, `test_local_touch_ui`,
  `test_fermentation_ui_models`, bei S3 betroffene Application-Tests).
- Build: `esp32_release` und `esp32_bringup` je Codeschnitt.
- Hardware: Messprotokoll aus Abschnitt 3 nach S2 (Baseline) und nach S3–S7.
- Dokumentation: S1 ändert `RESOURCE_BUDGET_AND_MAINTENANCE.md`; S2 und S8
  legen Evidenz unter `docs/audits/` ab; `docs/ROADMAP.md` je Schnitt.
- Der vollständige lokale Pre-Ready-Lauf folgt nur dem Workflow aus
  `docs/AGENT_WORKFLOW.md` und `docs/CI_AND_QUALITY_GATES.md`.

Safety: Keine Änderung an Regelung, Interlock oder Aktorfreigabe;
`ACTUATOR_RELEASE=NO` bleibt. Die UI-Drosselung (S5) betrifft nur das
Zeichnen, nicht Regel- oder Safetytakt.

## 9. Offene Ownerentscheidungen

- [ ] O1: Freigabe dieser Plan-SHA.
- [ ] O2: repräsentativer R1-Lastpfad (Vorschlag in Abschnitt 3, Punkt 6).
- [x] O3: `FREIGEGEBEN` (Owner, 2026-10-02). Der kombinierte Messpunkt
      `after_application_begin` inklusive Application-Initialisierung, WLAN
      und HTTP ist ausreichend. Daraus wird keine isolierte WLAN-Wirkung
      abgeleitet (Abschnitt 3).
- [ ] O4: nach S8 der abgeleitete System-Mindestabstand.
- [ ] O5: nach S8 Freigabe von S9/S10/S11, jede `sdkconfig`-Änderung einzeln.
- [x] O6: `OWNER_GOVERNANCE_OVERRIDE` (Owner, 2026-10-02). Für PR #174 ist
      ausnahmsweise kein separates Issue erforderlich
      (`NO_SEPARATE_ISSUE_REQUIRED_BY_OWNER_OVERRIDE`); Scope ist die laufende
      R1-RAM-Stabilisierung. Die Ausnahme gilt nur für PR #174 und ändert die
      allgemeine Governance nicht.
- [x] O7: `FREIGEGEBEN / ERLEDIGT` (Owner, 2026-10-02). ADR-020 und ADR-021
      bleiben entfernt; keine weitere Ownerentscheidung erforderlich.

Spätere, getrennte Owneroptionen außerhalb dieses Plans: ESP32-S3-/PSRAM-Profil und
Speicherplatzierungsschnittstelle; Flash-Core-Dump; Prozessvereinfachungen.
Automatische Messnachweise ersetzen keine Ownerentscheidung und keinen
unabhängigen Review.

## 10. Materielle Risiken

- Die Hotspot-Korrekturen reichen nicht; dann S9/S11 mit eigener Planung.
- Die Revision aus S3 erfasst nicht jede sichtbare Änderung; abgesichert durch
  die Invalidierungstests aus S3/S4.
- Messwerte schwanken zwischen Läufen; deshalb identisches Protokoll und
  Ableitung des Mindestabstands aus Minima, nicht aus Einzelwerten.

## 11. Herkunft der Korrekturen

A = „PR #174 Plan korrigieren“, B = „Korrekturauftrag zum Optimierungsplan“.

| Punkt | Umsetzung im Plan |
|---|---|
| A1 Keine Parallelverträge, ADRs entfernen, kein S3/PSRAM | ADR-020/021 gelöscht; 1, 4.1, 9 |
| A2 Budgetwerte erst nach Messung | 3, 4.2 |
| A3 Bestehenden Buildreport nutzen | 2.4, S2 |
| A4 Hot-Path-Regel präzisieren | 4.1 |
| A5 Kleinste UI-Hotspots einzeln | 2.3, S3–S7, 5.8 |
| A6 WLAN als Messkandidat | 5.6 |
| A7 Keine Async-Architektur zur Stackreduktion | 5.7 |
| A8 Keine Core-Dump-Partition | 6 |
| A9 Kein Prozess-/Governance-Scope, #170 nur abhängig | 1, 7, 9 |
| B1 Messbasis, Ursachen, Budgetstatus | 2.1, 2.2, 3, 4.2 |
| B2 Allokationsregel und Nachweisgrenzen | 2.3, 3 (LVGL-Pool), 4.1, 5.1–5.3, 5.8, 7 |
| B3 Netzwerk-Commit beim bestehenden Owner | 2.4, 5.3, 5.7 |
| B4 Core Dumps nicht in der Messbasis | 6, S2 |
| B nicht blockierend: S3/Platzierung, E4-Gate, Prozess | 7, 9 |
| B-Punkte zu ADR-020/021 (Kontext, Entscheidung, No-PSRAM-Garantie) | durch Entfernung nach A gegenstandslos; Inhalte in 2.1, 4.1, 4.2 und 6 übernommen; O7 erledigt, ADRs bleiben entfernt |
