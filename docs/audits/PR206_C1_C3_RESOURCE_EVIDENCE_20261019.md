# PR #206 / Issue #27 – C1–C3 Ressourcenevidence (offline), 2026-10-19

Plan [`3376a19`](../tasks/issue-27-r1-run-dashboard-info-plan.md) (Ownerfreigabe, D1-C und H-RAM-1 = M bestaetigt).
**`RAM_PASS=NOT_PROVEN`; Ergebnis hoechstens `RAM_OFFLINE_ESTIMATED`.** H-RAM-1 ist
**NOT_RUN** (kein Flash, UART, Browser, Reset). Offline-Messquelle: Build des
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

## Tests (alle `native`, voller Lauf `pio test -e native`: 1 804 PASS vor den C3-Tests, danach Teilmengen)

Projektion/Zeitmatrix/Codec/Allokationsfenster: `test_run_detail_projection` (10).
UI-Steady-State mit 96-Byte-Namen allokationsfrei und ohne Redraw durch
Phasensekunden: `test_ui_steady_state_allocations` (29, davon neu 1). Shell,
Golden, Label-Vertrag, Bindung Shell/Codec: `test_web_application_routes` (101,
davon neu 1). Ausfuehrender JS-Test `scripts/test_web_shell_polling.mjs`: 48 Faelle
PASS (Fake-Fetch, `browser_hardware=NOT_PROVEN`). Der vorherige Labelstand bleibt
als eingefrorene Referenz `test/test_web_application_routes/shell_frozen_labels.json`.

## Nicht ausgefuehrt / Grenzen

Pre-Ready, Independent Review, Browser, Hardware (H-RAM-1, HW-27-R01), Peak-Heap,
HTTPD-/Main-HWM, `ui_skipped_low_memory`, Recovery: `NOT_RUN`/`NOT_PROVEN`.
AdjustRun-Neubaseline der Phasenzeit ist nur ueber die bestehende FSM-/Kommando-
semantik abgedeckt (kein eigener Fall). `ACTUATOR_RELEASE=NO`.
