# PR #207 / Issue #27 – Standbyinformationen, Offline-Ressourcenevidence, 2026-10-20

Plan [`9656a18`](../tasks/issue-27-r1-web-standby-info-plan.md) (Ownerfreigabe der exakten
SHA; D1 = F2, Limit 11 264 B, D2 domaenenreine Tokens, D3 alle Modi, D4 keine
obligatorische Hardware-RAM-Messung). Offline-Messquelle: sauberer Implementierungscommit
`33c7b36` (`build_esp_idf_profiles.py all --require-clean-source-tree`: `esp32_bringup`
und `esp32_release` PASS, Profil-/Driftpruefung PASS). Vergleichsbasis: `main`
`45e7e63`, gebaut als `esp32_release` in einem separaten Worktree mit derselben
Toolchain (ESP-IDF 6.1). Der Basisabgleich reproduziert die #206-Evidence
(Frames 544/3120/80/32/800 B, Sektionen 18 975/74 488/100 815 B).

`RAM_PASS=NOT_PROVEN`; Ergebnis hoechstens `RAM_OFFLINE_ESTIMATED`. Keine Hardware-,
Browser-, Flash-, UART- oder Reset-Aktion. `ACTUATOR_RELEASE=NO`.

## Shell-Budget (MEASURED, native `test_web_application_routes`, 107/107 PASS)

Authentifiziert **11 016 B**, Passwortschutz aus 11 015 B; Limit 11 264 B, Reserve
248 B = 128 B Normalreserve (inklusive 19 B Sequenzaufschlag) + 120 B Bindungs-
spielraum. Die Bindung an die reale API hat den Body gegenueber dem F2-Messkandidaten
um **0 B** veraendert: der Spielraum ist unverbraucht. Kopf <= 1 536 B und
Auslieferungspfad (F1) unveraendert. `kMaximumWebShellBytes` ist die einzige
Limitaenderung (Produkt, eingefrorener Generator und Test: 11 264 B).

## Sektionsdeltas (MEASURED, `esp32_release`, `xtensa-esp32-elf-size -A`)

| Sektion | Basis `45e7e63` | Implementierung | Delta | Grund |
|---|---:|---:|---:|---|
| `.dram0.data` | 18 975 | 18 975 | **0** | keine neuen statischen Objekte |
| `.dram0.bss` | 74 488 | 74 488 | **0** | dito |
| `.iram0.text` | 100 815 | 100 815 | **0** | neuer Code liegt im Flash |
| `.flash.rodata` | 236 524 | 237 212 | +688 | Suffix +606 B, Token-/Schluesselstrings des Codec |
| `.flash.text` | 1 405 210 | 1 405 758 | +548 | Accessor, Mapping, Codec-`addDeviceInfo`, Formatierung (unter der Planschaetzung 0,8…1,5 KB) |
| `.bin` | 1 762 976 | 1 764 224 | +1 248 | Summe, rund 57 % der 3 080 192-B-Partition |

Das Designziel „kein Dauer-RAM“ ist offline bestaetigt: `.dram0.*` unveraendert; kein
Zusatz in `FermentationUiSnapshot`, Tracker, `UiRenderGate` oder Main-Stack
(`refreshUiSnapshot` weiter 800 B, `FermentationUiSnapshot` und Display-Refresh-
Revision unveraendert, `test_ui_steady_state_allocations` und `test_renderer_boundary`
PASS).

## Statische Stack-Frames (MEASURED, GCC `-fstack-usage`, einzelne Frames, `esp32_release`)

| Funktion | Basis | Impl. | Delta |
|---|---:|---:|---:|
| `WebReadOnlyApiHandler::handle` (HTTPD, haelt Snapshot und Geraeteinfo) | 544 | 624 | **+80** |
| `encodeWebApiStatus` | 80 | 80 | 0 |
| `serializeBounded` | 3 120 | 3 120 | 0 |
| `addDeviceInfo` (neu, kehrt vor `serializeBounded` zurueck, nicht verschachtelt) | – | 80 | neu |
| `FermentationApplication::deviceInfoView` (neu, nicht gleichzeitig mit der Encode-Kette) | – | 144 | neu |
| `refreshUiSnapshot` | 800 | 800 | 0 |

**Stoppregel §6 (+96 B fuer `handle → encodeWebApiStatus → serializeBounded`):
erfuellt, Delta +80 B.** Die Kette `handle → deviceInfoView` (624 + 144 plus die
Aufrufer-Frames von Lease/`toLocalTime`) liegt statisch unter der Encode-Kette
(> 3,8 KB) und ist nicht die tiefste. Rechnerisch bleibt gegenueber dem H-RAM-1-Wert
(HTTPD 2 196 B frei) ein Rest von etwa 2 116 B, ueber dem historischen Testfloor
2 080 B. Das ist eine **statische Frame-Rechnung** (Callees von cJSON/libstdc++ nicht
eingerechnet), kein Laufzeitnachweis: HWM bleibt **NOT_PROVEN**; eine
Hardware-Nachmessung ist laut D4 vor Ready nicht obligatorisch.

## Allokationsfenster `GET /api/v1/status` (MEASURED im Host-Test, ESTIMATED fuer ESP32)

Gemeinsame Testinstrumentierung (`operator new` und cJSON-Hooks), Fenster = gesamter
Handlerpfad im zusammengesetzten Dispatcher; Basiswerte aus der #206-Evidence.

| Fall | Allokationen Basis → neu | angeforderte Bytes Basis → neu | max. lebend Basis → neu | Body Basis → neu |
|---|---:|---:|---:|---:|
| Standby, warm | 50 → 59 | 2 974 → 3 324 (**+350**) | 1 980 → 2 330 (**+350**) | 261 → 337 |
| Lauf, 96-B-Name, warm | 84 → 93 | 4 712 → 5 062 (**+350**) | 3 718 → 4 068 (**+350**) | 668 → 744 |

Encoder-Grenzfall (Steuerzeichen-Name mit 6-facher Escape-Laenge, nicht zustands-
konform): Worst Case 3 699 B angefordert / Body 1 219 B ohne Geraeteinfo, mit
**96 Steuerzeichen-Bytes Geraetename (576 B escaped), 64-B-Zone und Zeit**: 77
Allokationen, 4 685 B angefordert, groesster Block 1 890 B, Body **1 889 B**
(< 3 072 B), cJSON vollstaendig freigegeben (`test_run_detail_projection`). Der
transiente Zuwachs liegt mit **+350 B** (zuzueglich rund 8 B Allocator-Overhead je
Block, also etwa +0,4 KB auf dem ESP32; cJSON-Knoten dort kleiner) innerhalb der
Planschaetzung +0,3…0,5 KB. Aussagegrenzen wie in #206: 64-Bit-Host, angeforderte
statt belegter Bytes, keine Fragmentierung, keine WLAN-/Task-Nebenlaeufigkeit.

## Weitere Nachweise (Builder-Self-Check)

- Native: `test_web_application_routes` 107/107, `test_run_detail_projection` 11/11,
  `test_device_ui_contracts` 11/11, `test_fermentation_ui_models`,
  `test_renderer_boundary`, `test_ui_steady_state_allocations`, `test_network_configuration`,
  `test_time_source`, `test_local_time` PASS; `check_architecture_boundaries.py` PASS.
- JS: `scripts/test_web_shell_polling.mjs` **56/56 PASS** (bisherige 48 mit angepassten
  Indizes plus 8 neue Faelle), Fake-Fetch, kein Browser (`browser_hardware=NOT_PROVEN`).
- Messskript und Kandidaten der Budgetwahl:
  [`STANDBY_INFO_BUDGET_20261020/`](STANDBY_INFO_BUDGET_20261020/).

## Nicht bewiesen

Browser-/Geraetelauf, HTTPD-/Main-HWM, Heap-Spitzen und -Erholung auf dem Geraet,
Mehrclient, Touch, aktiver Lauf, Langzeit; Produktimage nicht hardwarequalifiziert.
`RAM_PASS=NOT_PROVEN`.
