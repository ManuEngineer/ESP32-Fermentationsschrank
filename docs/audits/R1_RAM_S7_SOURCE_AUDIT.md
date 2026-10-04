# R1-RAM – S7 Source-Audit: `trans_size` im gepinnten LVGL-9-Pfad

Prüfung gegen die **tatsächlich gepinnte lokale Komponentenquelle**, nicht
gegen einen Upstream-HEAD. Keine Codeänderung, keine Hardwaremessung.

```text
S7=NO_ACTION
S7_REASON=TRANS_SIZE_UNUSED_IN_PINNED_LVGL9_PATH
S7_CODE_CHANGE=NO
S7_HARDWARE_REMEASURE=NOT_REQUIRED
```

## 1. Pin und lokale Quelle

- `main/idf_component.yml`: `espressif/esp_lvgl_port` `2.9.0`.
- `dependencies.lock`: `espressif/esp_lvgl_port` `component_hash`
  `d3c020b45c3dfc0d1706a83b63a1c34a6eb444a6e935f9afff79f92b7b94dcef`,
  `lvgl/lvgl` `component_hash`
  `7d82410747bbfb319531449749e940f440f8414f041f9eaba13f0d1dd7f58cdd`.
- Lokale Quelle `managed_components/espressif__esp_lvgl_port`
  (`idf_component.yml`: `version: 2.9.0`, `.component_hash` identisch zum
  Lock) und `managed_components/lvgl__lvgl` (`version: 9.6.0~1`,
  `.component_hash` identisch zum Lock).
- Build-Belege: Das CMake-Konfigurationslog der Builds nennt
  `LVGL version: 9.6.0~1`; die `compile_commands.json` des Release-Builds
  enthält ausschließlich `src/lvgl9/esp_lvgl_port_disp.c` (kein `lvgl8`).
  `esp_lvgl_port/CMakeLists.txt` wählt den Ordner `lvgl8`, wenn die
  LVGL-Version kleiner als 9.0.0 ist, sonst `lvgl9`.

## 2. Befund zu `trans_size`

`grep -rn trans_size` über die gesamte Komponente:

| Fundstelle | Bedeutung |
|---|---|
| `include/esp_lvgl_port_disp.h:51` | Feld `trans_size` der Konfigurationsstruktur („Allocated buffer will be in SRAM to move framebuf (optional)“) |
| `README.md:336` | Beispielkonfiguration |
| `src/lvgl8/esp_lvgl_port_disp.c:55,289,318–337,442,461,537,557` | **nur LVGL-8-Pfad**: `buf3 = heap_caps_malloc(trans_size * sizeof(lv_color_t), MALLOC_CAP_DMA)` (Zeile 337) und Transferlogik |
| `src/lvgl9/esp_lvgl_port_disp.c` | **keine einzige Fundstelle** |

Beantwortung:

1. **Wird `trans_size` im produktiven LVGL-9-SPI-Displaypfad ausgewertet?**
   Nein. Die LVGL-9-Implementierung (`src/lvgl9/esp_lvgl_port_disp.c`) liest
   `disp_cfg->trans_size` nirgends.
2. **Führt `displayConfig.trans_size = RepresentativeScreen::kWidth * 20U`
   (= 6400 Pixel) zu einer zusätzlichen Heap-/DMA-Allokation?** Nein, weil der
   Wert im verwendeten Pfad nicht gelesen wird.
3. **Gibt es im verwendeten Pfad einen separaten `trans_buf`?** Nein. Die
   LVGL-9-Datei kennt weder `trans_buf` noch `trans_size`. Ihre Allokationen
   sind: `disp_ctx` (`sizeof(lvgl_port_display_ctx_t)`, intern/8-Bit), der
   Zeichenpuffer `buf1` (`buffer_size * color_bytes` mit den Caps aus
   `buff_dma`/`buff_spiram`), ein optionaler zweiter Puffer nur bei
   `double_buffer`, und ein Rotationspuffer `draw_buffs[2]` nur bei
   `sw_rotate`.
4. **Ist `trans_size` effektiv unbenutzt?** Ja, in dieser Konfiguration und im
   LVGL-9-Pfad.

## 3. Abgleich der Produktkonfiguration

`main/fermentation_ui_lvgl_renderer.cpp:263–281`:

| Feld | Wert | Wirkung im LVGL-9-Pfad |
|---|---|---|
| `buffer_size` | `320 * 20` Pixel (RGB565 → 12800 B) | ein Zeichenpuffer `buf1`, `MALLOC_CAP_DMA` |
| `trans_size` | `320 * 20` | **ungelesen** |
| `flags.buff_dma` | 1 | Caps `MALLOC_CAP_DMA` |
| `flags.buff_spiram` | 0 | kein SPIRAM |
| `double_buffer` | nicht gesetzt (0) | kein zweiter Puffer |
| `flags.sw_rotate` | nicht gesetzt (0) | kein Rotationspuffer |

Build-Konfiguration (`sdkconfig`): LVGL builtin allocator mit 64 KiB Pool,
`LV_COLOR_DEPTH=16`, `LV_DRAW_BUF_ALIGN=4`, kein PSRAM.

## 4. Entscheidung

Die lokale gepinnte Quelle bestätigt: `trans_size` allokiert im verwendeten
LVGL-9-Pfad keinen Transferbuffer und hat keinen RAM-Effekt. S7 ist
`NO_ACTION`; es wird kein Code geändert, auch kein vorsorgliches
`trans_size = 0`, und es ist keine zusätzliche Hardwaremessung nötig. Der
einzige sichtbare Rest in diesem Pfad ist der eine 12800-B-DMA-Zeichenpuffer
`buf1`, der außerhalb von S7 liegt.
