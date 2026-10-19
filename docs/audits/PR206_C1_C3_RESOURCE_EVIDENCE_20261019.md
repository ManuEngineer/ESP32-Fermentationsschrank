# PR #206 / Issue #27 – C1–C3 Ressourcenevidence (offline), 2026-10-19

Plan [`3376a19`](../tasks/issue-27-r1-run-dashboard-info-plan.md) (Ownerfreigabe, D1-C und H-RAM-1 = M bestaetigt).
**Dieses Dokument ist der Offline-Stand vor H-RAM-1; die spaetere Hardwaremessung steht in
[PR206_H_RAM_1_EVIDENCE_20261019.md](PR206_H_RAM_1_EVIDENCE_20261019.md)** (begrenzter
positiver Teilnachweis, `RAM_PASS=NOT_PROVEN` fuer das vollstaendige Laufprofil). Aussagen
unten zu Hardware, HWM und Browser beschreiben den damaligen Offline-Stand.
`RAM_PASS=NOT_PROVEN`; Offline-Ergebnis hoechstens `RAM_OFFLINE_ESTIMATED`. Offline-Messquelle: Build des
sauberen Implementierungscommits `86f8ef0` (`build_esp_idf_profiles.py all
--require-clean-source-tree`: `esp32_bringup` und `esp32_release` PASS, Profil-/
Driftpruefung PASS); Vergleichsbasis ist der Planstand (Offline-Probe aus
[PR206_RAM_FEASIBILITY_20261019](PR206_RAM_FEASIBILITY_20261019.md), `esp32_release`).
Der spaetere Testcommit aendert keine Firmwarequelle.

## Shell-Budget (MEASURED, native `test_web_application_routes`, 101/101 PASS)

Authentifiziert **10 410 B**, Passwortschutz aus 10 409 B, Reserve 342/343 B
(Grenze 10 752 B). Die C3-Bindung (echte Codec-Feldnamen) hat den Body gegenueber
dem C1-Messstand um **0 B** veraendert; der Spielraum von 214 B (konservativ 195 B)
ist unverbraucht. `kMaximumWebShellBytes` ist die einzige Limitaenderung.

## Sektionsdeltas Implementierung gegen Basis (MEASURED, `esp32_release`)

| Sektion | Basis | Implementierung | Delta | Grund |
|---|---:|---:|---:|---|
| `.dram0.data` | 18 975 | 18 975 | **0** | keine neuen statischen Objekte |
| `.dram0.bss` | 74 488 | 74 488 | **0** | dito |
| `.iram0.text` | 100 815 | 100 815 | **0** | neuer Code liegt im Flash |
| `.flash.rodata` | 233 884 | 236 524 | +2 640 | Suffix +2 351 B, Token-/Schluesselstrings des Codec |
| `.flash.text` | 1 401 410 | 1 405 210 | +3 800 | Projektion, Zeitlogik, Codec-`run`-Objekt |
| `.bin` | 1 756 544 | 1 762 976 | +6 432 | Summe, rund 57 % der 3 080 192-B-Partition |

## Statische Stack-Frames (MEASURED, GCC `-fstack-usage`, einzelne Frames)

| Funktion | Basis | Impl. | Delta |
|---|---:|---:|---:|
| `WebReadOnlyApiHandler::handle` (HTTPD, haelt den Request-Snapshot) | 400 | 544 | +144 |
| `encodeWebApiStatus` / `serializeBounded` | 80 / 3 120 | 80 / 3 120 | 0 |
| `addRunDetail` (neu, nicht verschachtelt mit `serializeBounded`) | – | 32 | +32 |
| `refreshUiSnapshot` | 784 | 800 | +16 |
| `projectInto` | 496 | 672 | +176 |
| `projectRunDetail` (neu) / `projectRunTime` / `phaseElapsedSeconds` | – | 272 / 80 / 128 | neu |
| `app_main` (haelt `UiRenderGate`) / `initializeProductUi` | 5 024 / 528 | 5 312 / 672 | +288 / +144 |

Auswertung: Die Kette `handle -> encodeWebApiStatus -> serializeBounded` waechst nur
um **+144 B** (der Request-Snapshot im `handle`-Frame); die Projektionskette
`handle -> uiSnapshot -> refreshUiSnapshot -> projectInto -> projectRunDetail`
(+ Resolver) liegt mit rund 2,3 KB statisch unter der Encode-Kette (> 3,7 KB) und ist
daher nicht die tiefste. Der Main-Stack waechst im `app_main`-Frame um **+288 B**,
die Projektionskette dort um die Frames oben. Das ist eine **statische Frame-
Rechnung** (Callees der cJSON-/libstdc++-Aufrufe nicht eingerechnet), kein
Laufzeitnachweis: HTTPD-HWM (zuletzt 2 336 B frei, Testfloor 2 080 B) und Main-HWM
(mind. 11 752 B, Floor 11 160 B) bleiben **NOT_PROVEN** bis H-RAM-1. Damit ist die
fruehere Schaetzung +0,24…0,34 KB fuer den HTTPD-Stack fuer die tiefste Kette auf
+0,14 KB gesenkt; der Main-Stack liegt mit +0,29 KB innerhalb der Schaetzung.

## Allokationsfenster `/status`-Encode (MEASURED im Host-Test, ESTIMATED fuer ESP32)

`test_run_detail_projection::test_status_encoding_allocations_cover_cjson_and_operator_new`
erfasst `operator new` und cJSON (`cJSON_InitHooks`) gemeinsam; Fenster = ein
`encodeWebApiStatus`-Aufruf, vorgelegter Snapshot ausserhalb des Fensters.

| Fall | Allokationen (davon cJSON) | angeforderte Bytes | groesster Block | max. gleichzeitig lebend | Body |
|---|---:|---:|---:|---:|---:|
| Standby (`run:null`) | 23 (22) | 940 | 174 | 940 | 173 |
| typisch (Name 13 B, Kuehl-Abschluss) | 55 (54) | 2 274 | 453 | 2 274 | 452 |
| Worst Case (96-B-Steuerzeichen-Name, 48-B-Run-ID, alle Revisionen, Halteabschluss) | 63 (62) | 3 405 | 1 160 | 3 405 | 1 159 |

cJSON ist nach dem Fenster vollstaendig freigegeben (0 B lebend); nur der
Ausgabestring ueberlebt. Die Spitze liegt bei Baum plus Ausgabestring, weil der
Baum erst nach `serializeBounded` freigegeben wird. Delta gegenueber Standby:
**+1,3 KB (typisch), +2,5 KB (Worst Case)** angeforderte Bytes. Aussagegrenzen:
64-Bit-Host (cJSON-Knoten 48 B statt 40 B auf Xtensa), angeforderte statt vom
ESP-Heap belegte Bytes (der Allocator-Overhead kommt je Block hinzu, grob 63 x 8 B
= 0,5 KB), keine Fragmentierung, keine WLAN-/Task-Nebenlaeufigkeit. Der Worst-Case-
Wert liegt damit um rund 0,2–0,4 KB ueber der oberen Bandbreite der Planschaetzung
(+1,2…+2,6 KB); die Bandbreite wird auf **+1,3…+3,0 KB** (typisch bis Worst Case,
inklusive Allocator-Overhead) korrigiert. Weiterhin `ESTIMATED`. Der groesste
Einzelblock bleibt <= 1,2 KB (Mindestblock in den Altproben 3 584 B).

## Gesamter Handler `GET /api/v1/status` (MEASURED im Host-Test, ESTIMATED fuer ESP32)

Korrekturauftrag zum Implementierungsreview (B1): Das Encoder-Fenster oben deckt
nur `encodeWebApiStatus` ab. Zusaetzlich misst
`test_web_application_routes::test_status_handler_allocation_window_covers_snapshot_encode_and_response`
den **gesamten** Pfad `routes->handle(GET /api/v1/status)` im zusammengesetzten
Dispatcher (Session-Pruefung, `uiSnapshot()` mit Refresh-Tracker,
Encodierung, Uebernahme des Bodys in die Antwort). Beide Allokationswege
(`operator new` und cJSON-Hooks) laufen ueber die gemeinsame, wiederverwendete
Testinstrumentierung `test/common/allocation_window.hpp` (dieselbe wie im Encoder-
Test, keine Produktions-Allocator-Abstraktion). Request und Antwortobjekt werden
ausserhalb des Fensters angelegt; der Antwortbody bleibt nach dem Fenster am Leben.
Die Antwort ist gueltiges Status-JSON (`version`, `ready`, `revisions`), im Laufzustand
mit `run` aus der echten Application-Projektion (Run-ID 48 B, Name, `targetCelsius`
38, Abschluss `cool-hold-duration`, `scope:"phase"`).

Zustandskonformer Laufzustand: ein ueber `ActiveRun::start` gueltig gestartetes
Programm, Name 48 Skalare zu je 2 Bytes (96 B, das Maximum eines sichtbaren Namens),
48-B-Run-ID, Phase Fermenting mit konsistentem Run-Snapshot (ueber den
TestAccess des Application-Laufzustands gesetzt; einen echten Programmstart per
Kommando erlaubt das Fixture ohne Commissioning-Werte nicht). Der Name mit
Steuerzeichen (6 B pro Byte beim Escapen) aus dem Encoder-Worst-Case ist **nicht**
zustandskonform, weil sichtbare Namen validiert werden; er bleibt nur im Encoder-Test
als Grenzfall.

| Fall (Handler-Fenster) | Allokationen (davon cJSON) | angeforderte Bytes | groesster Block | max. gleichzeitig lebend | Body |
|---|---:|---:|---:|---:|---:|
| Standby, kalt (erster Abruf, fuellt die Tracker-Kopie) | 54 (36) | 4 654 | 672 | 3 156 | 261 |
| Standby, warm | 50 (36) | 2 974 | 504 | 1 980 | 261 |
| Lauf, 96-B-Name, kalt | 86 (68) | 4 858 | 669 | 3 864 | 668 |
| Lauf, 96-B-Name, warm | 84 (68) | 4 712 | 669 | 3 718 | 668 |

Unterschiede zu den Encoder-Fenstern: Das Handler-Fenster enthaelt zusaetzlich die
Snapshot-Beschaffung (Temperaturen, Meldungen, String-Kopien) und die
Antwortuebernahme (`std::move`, keine zweite Kopie) und im Standby alle
Revisionsmitglieder (auch `userConfiguration`, `programCatalog`), die der
Encoder-Standby-Fall nicht hatte (daher 36 statt 22 cJSON-Allokationen). Der
Encoder-Worst-Case mit Steuerzeichen-Name (Body 1 159 B) setzte diese beiden
Revisionen ebenfalls nicht; seine Werte sind deshalb als Grenzfall des Encoders zu
lesen, die Handler-Werte sind die realistischeren Gesamtzahlen. Delta Lauf gegen
Standby (warm): **+34 Allokationen, +1 738 B angeforderte Bytes, +1 738 B
Spitze**. cJSON ist in allen Faellen nach dem Fenster vollstaendig freigegeben.
Kalt gegen warm unterscheidet sich um die einmalige Tracker-Kopie (Standby +1,7 KB
angefordert, Lauf +0,15 KB).

Einordnung fuer ESP32 (**ESTIMATED**, `RAM_PASS=NOT_PROVEN`): Allocator-Overhead
(rund 8 B je Block, 84 Bloecke, also rund +0,7 KB) und kleinere cJSON-Knoten
(40 statt 48 B, rund −0,2 KB) ergeben eine transiente Spitze von etwa **4,2 KB** im
Lauf (Standby etwa 2,4 KB), das Delta durch den Lauf rund **+2,0 KB**. Das liegt
innerhalb der korrigierten Bandbreite +1,3…+3,0 KB. Es bleibt der groesste Einzelbefund
zur Heap-Seite: die absolute Spitze von etwa 4,2 KB ist gegen das niedrigste in den
Altproben beobachtete Shell-Phasen-Minimum (10 336 B frei, Vier-Session-Stresslauf,
anderes Image) nicht vernachlaessigbar, ein einzelner Abruf bleibt aber der
einzige gleichzeitige Handler (ein HTTPD-Task). Die groessten Einzelbloecke
(<= 0,7 KB) liegen weit unter den Skip-Schwellen (13 312 B). Kein Stopp-Befund; die
Messung auf dem Geraet (H-RAM-1) bleibt der einzige Nachweis. Grenzen unveraendert:
Xtensa-Layout, Fragmentierung, WLAN-Nebenlaeufigkeit, Stack-HWM.

## Tests (alle `native`, voller Lauf `pio test -e native`: 1 804 PASS vor den C3-Tests, danach Teilmengen)

Projektion/Zeitmatrix/Codec/Encoder-Allokationsfenster: `test_run_detail_projection` (10). Handler-Allokationsfenster: `test_web_application_routes` (102, davon neu 2).
UI-Steady-State mit 96-Byte-Namen allokationsfrei und ohne Redraw durch
Phasensekunden: `test_ui_steady_state_allocations` (29, davon neu 1). Shell,
Golden, Label-Vertrag, Bindung Shell/Codec: `test_web_application_routes`. Ausfuehrender JS-Test `scripts/test_web_shell_polling.mjs`: 48 Faelle
PASS (Fake-Fetch, `browser_hardware=NOT_PROVEN`). Der vorherige Labelstand bleibt
als eingefrorene Referenz `test/test_web_application_routes/shell_frozen_labels.json`.

## Builder-Self-Check

`bash scripts/run_pre_ready_gates.sh self-check` auf `0e2836d` (clang-format 21.1.3
aus dem gepinnten esp-clang, danach clang-tidy): `BUILDER_STATIC_ANALYSIS_SELF_CHECK=PASS`.
Der erste Lauf scheiterte an einem Formatverstoss (und anfangs an der falschen
clang-format-Version 18); die Formatkorrektur `0e2836d` aendert nur Layout. Der ESP-
Build oben stammt von `86f8ef0` (vor der reinen Formatierung); die Formatierung ist
kein Beleg ueber einen erneuten ESP-Build. Kein vollstaendiger Pre-Ready-Lauf.

## Nicht ausgefuehrt / Grenzen

Stand dieses Offline-Dokuments (historisch): Pre-Ready, Independent Review, Browser,
Hardware (H-RAM-1, HW-27-R01), Peak-Heap, HTTPD-/Main-HWM, `ui_skipped_low_memory`,
Recovery waren `NOT_RUN`/`NOT_PROVEN`. Seither: Independent Implementation Review
`GO`; H-RAM-1 ausgefuehrt (siehe Banner oben). HWM-Spitzen im **Produktimage P** und
der aktive Lauf (S4) bleiben `NOT_PROVEN`.
AdjustRun-Neubaseline der Phasenzeit ist nur ueber die bestehende FSM-/Kommando-
semantik abgedeckt (kein eigener Fall). `ACTUATOR_RELEASE=NO`.
