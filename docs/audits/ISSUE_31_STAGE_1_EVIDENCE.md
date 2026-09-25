# Issue #31 – Stage-1-Evidence

Stand: 2026-09-18. Dieser Nachweis umfasst ausschließlich Stufe 1 des
ownerfreigegebenen Plans. Es wurde kein Produktcode geändert, kein Display-/
Touch-Smoke ausgeführt und kein Renderer ausgewählt.

## Provenienz und Gate-Status

```text
ISSUE=31
PR=156
PR_SOURCE_HEAD_BEFORE_EVIDENCE=e75df8e8e1cc15286e4030bc6d8f9a6347303893
APPROVED_PLAN_SHA=ec6d6b596bd0bc926dbcccb8b3d13b132ed81855
BASE=main@1f1755e5e706fb668472920545b5302fcef1df16
ESP_IDF=v6.1@fff9895c82d744c7237be8847347bdd1b07c6643
TARGET=esp32-WROOM-32E
FLASH=4MB
PSRAM=NONE
CXX_STANDARD=17
STAGE_0_OVERALL=PASS
STAGE_1=PASS
STAGE_2=NOT_RUN
STAGE_3=NOT_RUN
STAGE_4=NOT_RUN
PRODUCT_IMPLEMENTATION=NOT_STARTED
ACTUATOR_RELEASE=NO
```

Stage 1 ist für den offiziellen Low-Level-Stack bestanden. Die reale
Controlleridentität, Display-/Touch-Funktion und Laufzeit-Ressourcenmessung
bleiben wie geplant dem aktiven Stage-2-Smoke bzw. den späteren Stufen
vorbehalten. `DISPLAY_CONTROLLER_IDENTITY` und
`TOUCH_CONTROLLER_IDENTITY` werden durch diesen Build nicht vorgezogen.

## Direkte Komponenten und Lock-Auflösung

Die Prüfung verwendete ein isoliertes, nicht in das Repository eingechecktes
ESP-IDF-Projekt unter `/tmp/issue31-stage1-official`. Der Component-Manager
löste die direkte Kandidatenkette deterministisch auf. Die vollständige
Auflösung liegt in der temporären `dependencies.lock`; ihr SHA-256 ist
`9ef854367f503de7c8bdde3c555d2fb63d38cbcaff82cff411ee99c8f4752250`.

| Komponente | Quelle / Version | Aufgelöste Provenienz | Lizenz | Ergebnis |
|---|---|---|---|---|
| `esp_lcd` | Bestandteil von ESP-IDF v6.1 | `/components/esp_lcd`, IDF-Commit `fff9895c82d744c7237be8847347bdd1b07c6643`; kein separater Registry-Eintrag | Apache-2.0 über ESP-IDF | `PASS` |
| `espressif/esp_lcd_ili9341` | [ESP Component Registry](https://components.espressif.com/components/espressif/esp_lcd_ili9341), `2.1.0` | esp-bsp Commit `93460b932beba7022a8a4ec4186e0d6b7533d05f`, Pfad `components/lcd/esp_lcd_ili9341`; Component-Hash `840387ee0c90a473e43fdc75570f77c52f06423aa331d95479bde8b418c43547` | Apache-2.0; `license.txt` SHA-256 `cfc7749b96f63bd31c3c42b5c471bf756814053e847c10f3eb003417bc523d30` | `PASS` |
| `espressif/esp_lcd_touch` | [ESP Component Registry](https://components.espressif.com/components/espressif/esp_lcd_touch), `1.2.1` | esp-bsp Commit `c927778a85eed239dd403c1719d4f543ad56e693`, Pfad `components/lcd_touch/esp_lcd_touch`; Component-Hash `3f85a7d95af876f1a6ecca8eb90a81614890d0f03a038390804e5a77e2caf862` | Apache-2.0; `license.txt` SHA-256 `cfc7749b96f63bd31c3c42b5c471bf756814053e847c10f3eb003417bc523d30` | `PASS` |
| `atanisoft/esp_lcd_touch_xpt2046` | [ESP Component Registry](https://components.espressif.com/components/atanisoft/esp_lcd_touch_xpt2046), `1.0.6` | Git commit `05f4ecb82f19e4aa11b855a8e57539ba34e9629c`; Component-Hash `7e6381b67b6e487379118368b8e91624dc87036ef5734818de1db9eb35697998` | MIT; `LICENSE` SHA-256 `21a7c428a2b3cd90ac1f3c9c03ec3eea7d81f4ec01875ae0aa22a00d8cdbb74a` | `PASS` |
| `espressif/cmake_utilities` | [ESP Component Registry](https://components.espressif.com/components/espressif/cmake_utilities), `0.5.3`, transitive | Component-Hash `351350613ceafba240b761b4ea991e0f231ac7a9f59a9ee901f751bddc0bb18f`; die Manifestquelle nennt keinen Repository-Commit | Apache-2.0; `license.txt` SHA-256 `cfc7749b96f63bd31c3c42b5c471bf756814053e847c10f3eb003417bc523d30` | `PASS` |

Die Manifestanforderungen sind mit ESP-IDF 6.1 kompatibel: ILI9341 fordert
`idf >=5.2`, `esp_lcd_touch` `idf >=4.4.2`, XPT2046 `idf >=4.4` und die
transitive CMake-Komponente `idf >=4.1`. Relevante Notices/Lizenzen sind die
genannten Apache-2.0-/MIT-Dateien sowie die bestehende Apache-2.0-Lizenz des
ESP-IDF-Checkouts; zusätzliche Fonts oder Assets wurden nicht eingebunden.

## Reproduzierbarer Build

Verwendete Werkzeuge: `ESP-IDF v6.1`, `esptool v5.4.0`,
`xtensa-esp-elf-gcc 15.2.0`, Python 3.13.5. Der ESP-IDF-Checkout war sauber
und exakt auf `fff9895c82d744c7237be8847347bdd1b07c6643`.

Der reproduzierbare Kandidatenlauf war:

```text
source /var/lib/docker/data/ESP32-Projekte/start.sh
export IDF_TOOLS_PATH=/var/lib/docker/data/engineering/home/manuel/.espressif
export IDF_PYTHON_ENV_PATH=/var/lib/docker/data/engineering/home/manuel/.espressif/python_env/idf6.1_py3.13_env
export PATH="$IDF_PYTHON_ENV_PATH/bin:$PATH"
idf.py -B /tmp/issue31-stage1-official/build \
  -DSDKCONFIG=/tmp/issue31-stage1-official/build/sdkconfig \
  -DSDKCONFIG_DEFAULTS=/tmp/issue31-stage1-official/sdkconfig.defaults \
  -DCMAKE_CXX_STANDARD=17 build
```

Die Harness-Quellen deklarierten exakt die direkten Komponenten 2.1.0, 1.2.1
und 1.0.6, target `esp32`, 4 MB Flash, kein PSRAM und die bestehenden
relevanten Flags `APP_PROFILE_ESP32_BRINGUP=1`,
`APP_TARGET_FLASH_MB=4`, `APP_REQUIRE_PSRAM=0`,
`APP_WEB_OTA_ENABLED=0` und `APP_REAL_ACTUATORS_ENABLED=0`. Der finale
C++-Compilerlauf enthielt `-std=gnu++26 -std=gnu++17`; die letzte Option war
wirksam und erzwingt den beauftragten C++17-Vertrag. Das ist ausschließlich
eine Eigenschaft des isolierten Stage-1-Harnesses, keine Produktänderung.

| Build-Nachweis | Ergebnis |
|---|---|
| Component-Manager-Auflösung mit Lock | `PASS`; 5 Dependencies, Lock-SHA wie oben |
| Kandidatenbuild | `PASS`; `Project build complete` |
| erzeugtes App-Binary | `142096` B; `issue31_stage1_official.bin` |
| `idf.py size` | `PASS`; Gesamtbild `141983` B, IRAM `44031/131072`, DRAM `13618/180736`, RTC SLOW `64/8192` |
| `idf.py size-components` | `PASS`; `esp_lcd`, ILI9341, `esp_lcd_touch` und XPT2046 im Graph |
| Stage-0-Konfigurationsvergleich | `PASS`; ESP32, 4 MB, kein PSRAM und dieselbe Messmethode |
| Produktlaufzeit-/Hardwarevergleich | `NOT_RUN`; gehört ab Stage 2 zum aktiven Smoke und wurde nicht vorgezogen |

Die bestehende `esp32_bringup`-Baseline wurde mit demselben `idf.py size`
gemessen: App-Binary `1080304` B, Gesamtbild `1080180` B, IRAM
`89231/131072`, DRAM `41659/180736`, RTC SLOW `64/8192`. Wegen der bewusst
minimalen Kandidaten-Harness gegenüber der vollständigen Firmware sind diese
Größen kein Produkt-Delta und werden nicht als neues Budget verwendet. Die
gemeinsamen Stage-0-Vertragswerte (4 MB, kein PSRAM, actor-free) sind
identisch; neue willkürliche Ressourcenbudgets wurden nicht eingeführt.

## Warnungen und Ausführungshinweise

Der finale Kandidatenbuild war erfolgreich. Es gab keine beobachtete
Quellwarnung der direkten Kandidaten. Der Lauf meldete ausschließlich bereits
bekannte ESP-IDF-v6.1-Upstreamhinweise: private Include-Verzeichnisse zwischen
`esp_wifi`/`wpa_supplicant` sowie Kconfig-Hinweise zu `default 0` bei einzelnen
Bool-Einträgen in NimBLE/FATFS. Diese Hinweise stammen aus dem Framework und
änderten weder den Kandidatengraph noch den Repository-Code.

## `esp_bsp_generic` und Alternativen

`espressif/esp_bsp_generic` `3.1.1` wurde anhand der [Registry-
Dokumentation](https://components.espressif.com/components/espressif/esp_bsp_generic/versions/3.1.1/readme)
und der [Dependency-Liste](https://components.espressif.com/components/espressif/esp_bsp_generic/versions/3.1.1/dependencies)
geprüft. Die Registry führt Apache-2.0 als Lizenz; eine Lock-Auflösung wurde
für den verworfenen Kandidaten nicht erzeugt. Es bringt einen
generischen, menuconfig-gesteuerten BSP-Umfang sowie zusätzliche Button-, LED-,
SPIFFS-, Display- und mehrere I2C-Touch-Abhängigkeiten ein; die
Registry-Liste enthält keinen XPT2046-SPI-Touchpfad. Für das vorhandene Board
und den Stage-1-Low-Level-Scope gibt es daher keinen konkreten Mehrwert.
Ergebnis: `DROPPED_WITH_REASON` – kein BSP-Scaffold und keine zusätzliche
Abhängigkeit adoptiert.

`esp_lvgl_port` und LVGL bleiben planmäßig außerhalb Stage 1–4:
`DROPPED_WITH_REASON=OUTSIDE_STAGE_SCOPE`. LovyanGFX, TFT_eSPI, LCDWiki,
Arduino_GFX und Adafruit_GFX wurden nicht weitergebaut:
`DROPPED_WITH_REASON=OFFICIAL_STACK_PASS_NO_RESERVE_CONDITION`. Der
freigegebene offizielle Stack hat Stage 1 bestanden; damit ist keine
alternative kombinierte Treiber-/Zeichenkette für dieses Gate ein ernsthafter
verbleibender Kandidat. Für diese Reservepfade wird kein ungeprüfter
Versions-/Lizenz-PASS behauptet.

## Schlussfolgerung

```text
OFFICIAL_LOW_LEVEL_STACK=PASS
ESP_BSP_GENERIC=DROPPED_WITH_REASON
LVGL_AND_ESP_LVGL_PORT=DROPPED_WITH_REASON
ALTERNATIVE_STACKS=DROPPED_WITH_REASON
STAGE_1=PASS
STAGE_2=NOT_RUN
STAGE_3=NOT_RUN
STAGE_4=NOT_RUN
PRODUCT_CODE_CHANGED=NO
ACTUATOR_RELEASE=NO
```

Der nächste zulässige Schritt ist ausschließlich die ownerseitige Entscheidung
über Stage 2. Bis dahin bleiben Controller-Smoke, funktionale Identität,
Renderer-/LVGL-Auswahl, Produktimplementation, Ready-/Merge-Aktionen und
Aktorfreigabe angehalten.
