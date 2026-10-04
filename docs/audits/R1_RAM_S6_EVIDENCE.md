# R1-RAM – S6: persistenten 5120-B-DMA-Scratchpuffer aus dem Adapter entfernen

Plan: `docs/tasks/memory-platform-course-plan.md` (Plan-SHA
`2c20fe13d4d51a2bfabc00e630f1ad7c90c0e48a`), Hotspot 4. Nur Code, Host-Tests und
Builds; **keine Hardware-Re-Messung**, kein S7, keine Planänderung.

```text
S5=NO_ACTION_OWNER_DECISION
OWNER_DECISION_S5_SKIP=YES
S6_PERSISTENT_DMA_BUFFER_REMOVED=YES
PERSISTENT_ADAPTER_DMA_BYTES_REMOVED=5120
PRODUCTIVE_LVGL_DIRECT_HANDLE_PATH=CONFIRMED
FILLRECT_SCOPED_DMA_BUFFER=YES
FLUSHRGB565_SCOPED_DMA_BUFFER=YES
S6_CODE_COMMIT=1970041d0017f59b0a478298721c63b0712fc667
S7_TO_S11=NOT_STARTED
```

## 1. Nutzungsnachweis (vor der Änderung)

- `ProductiveLvglRenderer` (`main/fermentation_ui_lvgl_renderer.cpp`) ruft vom
  Adapter nur `initialize()`, `setBacklight()` und `sampleTouch()` auf (und
  `setRotation()` innerhalb von `initialize()`), bindet die ESP-LCD-Handles
  über `detail::bindEspIdfDisplayTouchHandles()` und übergibt
  `handles.displayIo`/`handles.panel` an `lvgl_port_add_disp()`. Es ruft weder
  `fillRect()` noch `flushRgb565()` auf. LVGL zeichnet über esp_lvgl_port und
  überschreibt dabei den `on_color_trans_done`-Callback des Adapters (die
  bekannte Boot-Warnung `Callback on_color_trans_done was already set`).
- Der persistente `dmaPixels`-Puffer des Adapters (`320 × 8 × 2 = 5120 B`,
  `MALLOC_CAP_DMA`) wird im produktiven Pfad also nicht benutzt.
- Aufrufer von `fillRect()`: ausschließlich
  `main/issue_31_touch_calibration_harness.cpp` (16 Aufrufstellen, nur im
  Bring-up-Harness-Build). `flushRgb565()` hat keinen Aufrufer außer der
  Portdefinition (`device_ui_hardware_ports.hpp`) und der Adapterimplementierung.
  Beide Pfade bleiben erhalten.

## 2. Änderung

Datei: `lib/device_platform_esp_idf/src/esp_idf_display_touch_adapter.cpp`
(sonst keine).

- `Impl::dmaPixels` und die Allokation in `initialize()` entfallen; der
  Destruktor gibt keinen Dauerpuffer mehr frei.
- Neue lokale RAII-Klasse `ScopedDmaBuffer` (`MALLOC_CAP_DMA`, gleiche Größe
  `kPartialBufferPixels * 2 = 5120 B`). `fillRect()` und `flushRgb565()` legen
  sie nach der Argumentprüfung an (Fehlschlag → `false`), verwenden denselben
  Puffer für alle Chunks des Aufrufs und geben ihn beim Verlassen des Aufrufs
  frei. Keine dauerhafte Lazy-Allokation, kein Pool, kein Cache.
- Unverändert: `spi_bus_config_t::max_transfer_sz` (5120 B, für die direkten
  Transfers weiter erforderlich), LVGL `buffer_size`, `trans_size`,
  `buff_dma`, `buff_spiram`, SPI-Takt, Queue-Tiefe, Rotation, Touchpfad,
  `transferPending`-/`transferFaulted`-Zustand.

### Lifetime und Ownership

Der Puffer gehört genau einem Zeichenaufruf. `esp_lcd_panel_draw_bitmap()`
liest ihn asynchron, bis der `on_color_trans_done`-Callback `transferPending`
löscht. Der Aufruf wartet nach jedem Chunk mit `waitForTransfer()` und gibt den
Puffer erst nach dem letzten erfolgreichen Warten (Callback gelaufen) frei.
Fehlerpfade:

- Allokation schlägt fehl → `false`, nichts zu geben.
- `draw_bitmap` meldet einen Fehler → der Transfer wurde nicht eingereiht;
  `transferFaulted` wird gesetzt, der RAII-Puffer wird freigegeben.
- `waitForTransfer()` läuft in den Timeout (1000 ms) → der Transfer kann den
  Puffer noch lesen; er wird **nicht** freigegeben, sondern an
  `Impl::faultedDmaPixels` übergeben und im Destruktor freigegeben, nachdem
  dieser auf den Transfer gewartet hat. Der Zustand ist danach dauerhaft
  `transferFaulted` (jeder weitere Zeichenaufruf liefert `false`), es entsteht
  also höchstens ein übergebener Puffer, und nichts leckt.

### Zusatzänderung: `waitForTransfer()` wartet auf das Flag

Beim Entwurf zeigte sich eine Lücke, die beim bisherigen Dauerpuffer
folgenlos, beim freigegebenen Puffer aber ein Use-after-free wäre: Lief der
Callback (setzt `transferPending = false`, gibt die Semaphore) vor der
Prüfung in `waitForTransfer()`, blieb eine Semaphore-Marke zurück, und der
nächste Transfer konnte durch diese verspätete Marke zu früh „fertig“ melden,
während sein Transfer noch lief. `waitForTransfer()` (und der Destruktor)
warten deshalb in einer Schleife, bis `transferPending` gelöscht ist; eine
verspätete Marke verlängert nur das Warten. Das ist die einzige
Verhaltensänderung am Synchronisationspfad und ist nicht auf dem Gerät
getestet.

## 3. Verifikation

Gezielte native Tests (alle PASS): `test_display_rotation` 4/4,
`test_touch_calibration` 16/16, `test_renderer_boundary` 31/31,
`test_local_touch_ui` 15/15, `test_press_dispatcher` 20/20,
`test_ui_steady_state_allocations` 16/16, `test_device_ui_contracts` 8/8,
`test_raw_touch_recovery_detector` 8/8, `test_smoke` 14/14.
`check_architecture_boundaries.py` PASS, `clang-format` 21 sauber,
`check_secrets.py` und `git diff --check` PASS. Der ESP-IDF-Adapter ist nicht
nativ kompilierbar (`lib_ignore`); er ist daher nur durch die ESP32-Builds
abgedeckt. Kein Full-CI.

Builds aus einem sauberen Git-Worktree auf `1970041d…` (`git status --short`
leer, `--require-clean-source-tree`), ESP-IDF v6.1, 0 Warnungen,
Profilvalidierung PASS:

| Build | App-BIN SHA-256 | ELF SHA-256 | sdkconfig SHA-256 |
|---|---|---|---|
| `esp32_release` | `7b8d4c8e43a3ebe8ac6c08950bc81ef1c13efac0c2573805a5b2d8361aa9af28` | `486c02fe6f1c398c8756bae19ae6013c08c5b3ae611fe8f3cfbcf481b8e849f2` | `6edbf61555023d42da33cdf212fbd1b6030b5af3c4b1203f5440cd6055b8e09f` |
| `esp32_bringup` | `703dbdec5fdb1acbd95b00a061ee94cccb0b9d0d250d1dda4e089f963a5dbe34` | `da298a3c28f0c3ad96fb7d3e1fb77ca15a2bade939f1cf8d06f280b80c6fa6a7` | `044f9dcd1de26bd006e5208601479748c22667f074e2cebf3d38592989caa1f5` |
| Kalibrier-Harness (`esp32_bringup`, `APP_ISSUE_31_TOUCH_CALIBRATION_HARNESS`) | `6a7ede8c99c995df7f8aadaf617d52634909b3027dc9a457d1e9b7d8736e7fa9` | `3dd801e12d28b0f097231e0fe80dce4484713b4703630feb4d224804f32063c5` | – |

Der direkte Kalibrier-/Bring-up-Pfad kompiliert: Der Harness-Build enthält
`fillRect()` und `flushRgb565()` und 16 Aufrufstellen von `fillRect()`.
Quell-SHA in allen drei Builds `1970041d…`.

## 4. Grenzen

- Es wird **nicht** behauptet, dass auf der Hardware nun exakt 5120 B mehr
  frei sind. Der Puffer war im produktiven Pfad dauerhaft belegt und ungenutzt;
  der tatsächliche Effekt auf freien Heap, DMA-Reserve und größten Block ist
  erst durch die Hardware-Re-Messung belegbar und wurde nicht ausgeführt.
- Die direkten Draw-Pfade sind nur kompiliert, nicht auf dem Gerät
  ausgeführt (kein Kalibrier-Harness-Lauf).
- Das Synchronisationsverhalten von `waitForTransfer()` (Schleife auf das
  Flag) ist nur durch Code-Review und Build abgesichert.
