# R1-RAM – LVGL builtin memory pool 64 KiB → 48 KiB (Code/Config)

Zusätzlicher, vom Owner vor O4/O5 freigegebener RAM-Schnitt
(`OWNER_PRE_O4_RAM_SLICE=LVGL_POOL_48K_APPROVED`). Diese Runde ändert nur die
reproduzierbare Konfiguration und verifiziert Build und Vertrag. **Keine
Hardwaremessung**, keine weitere RAM-Optimierung, S9–S11 nicht begonnen.

```text
PINNED_LVGL_VERSION=9.6.0~1
PINNED_LVGL_HASH=7d82410747bbfb319531449749e940f440f8414f041f9eaba13f0d1dd7f58cdd
PINNED_ESP_LVGL_PORT=2.9.0
LVGL_ALLOCATOR=BUILTIN_TLSF
LVGL_POOL_STORAGE=STATIC_INTERNAL_RAM_ARRAY
LVGL_MEM_SIZE_BEFORE_BYTES=65536
LVGL_MEM_SIZE_AFTER_BYTES=49152
LVGL_CONFIG_REDUCTION_BYTES=16384
STATIC_DRAM_DELTA_BYTES_BOTH_PROFILES=-16384

ESP32_RELEASE_CONFIG_49152=PASS
ESP32_BRINGUP_CONFIG_49152=PASS
ESP32_RELEASE_BUILD=PASS
ESP32_BRINGUP_BUILD=PASS

HARDWARE_FREE_HEAP_GAIN=NOT_YET_MEASURED
HARDWARE_LVGL_MAX_USED=NOT_YET_REMEASURED

S8=PASS
OWNER_PRE_O4_RAM_SLICE=LVGL_POOL_48K_APPROVED
O4=DEFERRED_PENDING_HARDWARE_REMEASURE
O5=DEFERRED_PENDING_HARDWARE_REMEASURE
S9_TO_S11=NOT_STARTED
```

## 1. Source-of-Truth (vor der Änderung bestätigt)

- `dependencies.lock`: `lvgl/lvgl` Hash
  `7d82410747bbfb319531449749e940f440f8414f041f9eaba13f0d1dd7f58cdd`
  (lokale Komponente `managed_components/lvgl__lvgl`, `version: 9.6.0~1`,
  `.component_hash` identisch); `espressif/esp_lvgl_port` `2.9.0`.
- Allocator: `CONFIG_LV_USE_BUILTIN_MALLOC=y` (builtin/TLSF).
- Pool-Speicher: `src/stdlib/builtin/lv_mem_core_builtin.c` legt bei
  `LV_MEM_ADR == 0` ohne `LV_MEM_POOL_ALLOC` ein statisches Array an
  (`static MEM_UNIT work_mem_int[LV_MEM_SIZE / sizeof(MEM_UNIT)]`,
  `lv_tlsf_create_with_pool(work_mem_int, LV_MEM_SIZE)`). `LV_MEM_POOL_ALLOC`
  ist in der gepinnten Quelle nur in einer Testdatei definiert, nicht im
  Produktbuild; `CONFIG_LV_MEM_ADR=0x0`.
- Größenquelle: `include/lvgl/config/lv_conf_internal.h` setzt
  `LV_MEM_SIZE` auf `CONFIG_LV_MEM_SIZE`, Kconfig-Default 65536;
  `CONFIG_LV_MEM_SIZE_KILOBYTES=0` (veraltet, überschreibt nichts).
- Bisheriger Projektstand setzte `CONFIG_LV_MEM_SIZE` nicht explizit
  (`sdkconfig.defaults` ohne `LV_MEM`, Treffer nur `LV_USE_QRCODE`) und nutzte
  den 65536-B-Default. Kein PSRAM (`# CONFIG_SPIRAM is not set`).

Alle Annahmen stimmen mit dem aktuellen Branch überein.

## 2. Änderung

Einzige Datei: `sdkconfig.defaults` (gemeinsame Basis für `esp32_release` und
`esp32_bringup`): `CONFIG_LV_MEM_SIZE=49152` mit erklärendem Kommentar.
Unverändert: Main-Stack `24576`, LVGL-Display `buffer_size`, `trans_size`,
`buff_dma`, `buff_spiram`, SPI-Takt, Display-/Touch-Profile, QR-Konfiguration,
WLAN-/lwIP-Werte, Web-/Session-Code. Keine 32-KiB-Variante, kein
dynamisches Pool-Sizing.

## 3. Buildvertrag

Frische Builds beider Profile aus einem sauberen Git-Worktree auf
`6c1e159bc14159eb144f3564c1d2a87154c8ea71` (`git status --short` leer,
`--require-clean-source-tree`), ESP-IDF v6.1, **0 Warnungen**,
Profilvalidierung PASS. Beide generierten `sdkconfig` enthalten:

```text
CONFIG_LV_MEM_SIZE=49152
CONFIG_LV_MEM_SIZE_KILOBYTES=0
CONFIG_ESP_MAIN_TASK_STACK_SIZE=24576
# CONFIG_SPIRAM is not set
CONFIG_LV_USE_QRCODE=y
CONFIG_LV_USE_BUILTIN_MALLOC=y
```

Gegenüber den S6-Builds (`0beafdf`) unterscheidet sich je Profil genau **eine**
Zeile des generierten `sdkconfig` (`CONFIG_LV_MEM_SIZE=65536` →
`49152`); es gibt keine weitere Profilabweichung. Die Firmware trägt die
Quell-SHA `6c1e159bc14159eb144f3564c1d2a87154c8ea71`.

| Build | App-BIN SHA-256 | ELF SHA-256 | sdkconfig SHA-256 |
|---|---|---|---|
| `esp32_release` | `305da6d49d89dc3a5406c50caf1eb9a11fb4c43187801d96a0a2cd63c16d3e19` | `2dc88b3b3883f22e533ded9b14acd3c0bff44e5f7a4987004ba4b038abe1f630` | `e8d9202435a37b5872818f51d31efe13530f52c4b73b40668db5ec6b88b1eac6` |
| `esp32_bringup` | `8ba7a1a5ae8bf47a54f785368ef82e2f200934313fc74ff97723c25a5ecc5ff8` | `d1b1c966fdba8c5f368afc70c3890ecacd6e52392ebb8c7ede041b08be1fde05` | `723b60f7a004993c857d81288c340014509eac161a1ab8daeb3a0687a975122c` |

BIN/ELF/Map/sdkconfig/Bootloader/Partitionstabelle liegen schreibgeschützt
außerhalb des Repositories unter
`~/esp32-fermentationsschrank-backups/lvgl48_6c1e159/` für die folgende
Hardwaremessung.

## 4. Software-Verifikation

Native Tests (PASS): `test_renderer_boundary` 31/31,
`test_ui_steady_state_allocations` 16/16, `test_local_touch_ui` 15/15,
`test_press_dispatcher` 20/20, `test_fermentation_ui_models` 9/9,
`test_smoke` 14/14. `check_architecture_boundaries.py`, `check_secrets.py`
und `git diff --check` PASS. Ein bestehender Selftest für
`CONFIG_LV_MEM_SIZE` existiert nicht; es wurde keiner gebaut, der
Buildvertrag ist durch die Auswertung der generierten `sdkconfig` belegt.
Kein Full-CI.

## 5. Statische RAM-Auswirkung

Aus `idf.py size --format json2` (bestehender JSON2-Pfad):

| Größe | S6-Build `0beafdf` | LVGL-Pool 48 KiB `6c1e159` | Differenz |
|---|---:|---:|---:|
| statisches DRAM, `esp32_release` | 108907 B | 92523 B | **−16384 B** |
| statisches DRAM, `esp32_bringup` | 108907 B | 92523 B | **−16384 B** |
| IRAM (beide Profile) | 99579 B | 99579 B | 0 |
| konfigurierter LVGL-Pool | 65536 B | 49152 B | −16384 B |

Das ist die einzige in diesem Schritt direkt beweisbare Aussage: Die
statisch reservierte Poolgröße und das statische DRAM sinken um exakt
16384 B. Davon zu unterscheiden sind (und hier **nicht** behauptet werden):

- der tatsächlich zusätzlich verfügbare ESP-IDF-Heap zur Laufzeit,
- der tatsächliche Laufzeit-Low-Water-Mark,
- die nutzbare Poolgröße nach TLSF-Overhead (bei 64 KiB gemessen 63384 B) und
  der gemessene `max_used` im 48-KiB-Pool.

Rechnerisch: Der bisher gemessene LVGL-Spitzenwert von 15856 B beträgt
rund 32 % des konfigurierten 49152-B-Pools (bisher 24 % laut `lv_mem_monitor`,
bezogen auf den nutzbaren 63384-B-Pool); der konfigurierte Pool ist damit etwa das
3-Fache des bisherigen Spitzenwerts. Ob der 48-KiB-Pool alle Betriebsfälle
(Seitenwechsel, Sprachpaket, QR-Darstellung, Netzwerkwechsel) ausreichend
bedient, belegt erst die Hardware-Re-Messung (`lv_mem_monitor`).

```text
FREE_HEAP_GAIN=NOT_CLAIMED_BEFORE_HARDWARE_MEASUREMENT
```

## 6. Nächster Schritt (separat, nach Independent PASS)

Hardwaremessung mit derselben Matrix wie nach S6: Direct Display/UI Smoke,
`after_ui_init`, `stable_home_wifi`, `stable_ap_only`, 2× No-Client-Wechsel,
2× Client ohne Seite, 1× Client + Browser, längerer `HOME_WIFI`-Idle, LVGL
`max_used`, Heap frei/Minimum/größter Block und das 1696-B-Follow-up. Danach
erst O4/O5 final entscheiden.
