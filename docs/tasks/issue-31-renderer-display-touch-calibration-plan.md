# Issue #31 – Plan: realer Renderer, Display, Touch und Kalibrierung

## Planstatus und unveraenderliche Basis

| Feld | Wert |
|---|---|
| Issue | #31 – `[E5.3] Renderer, Display-/Touchadapter und Kalibrierung nach Hardwarebeweis` |
| Basisbranch | `main` |
| Basis-SHA | `54c80d26416343495b4d9a8c4518e6137dc747c1` |
| Arbeitsbranch | `agent/issue-31-renderer-display-touch-plan` |
| Planpfad | `docs/tasks/issue-31-renderer-display-touch-calibration-plan.md` |
| Planstatus | `OWNER_PLAN_APPROVAL_PENDING` |
| Implementation | `NOT_STARTED` |
| Hardware-Spike | `NOT_STARTED` |
| Renderer-Auswahl | `FINAL_SELECTION_PENDING` |
| LVGL-Auswahl | `DEFERRED_UNTIL_POST_STAGE4_DRIVER_SELECTION` |
| Renderer-Owner-Architektur | `EXISTING_MAIN_COMPONENT` |
| Neue Produktions-Lib-Komponente | `NO` |
| ADR-013-Erweiterung | `NOT_REQUIRED_FOR_MAIN_OWNER` |
| Hardwarestatus | `FUNCTIONAL_HARDWARE_VERIFICATION=PENDING` |
| GPIO-/SSOT-Status | `SSOT_CONFORMANCE=PENDING` |
| Elektrische Messung | `ELECTRICAL_LEVEL_MEASUREMENT=NOT_REQUIRED_WAIVED` |
| Aktorfreigabe | `NO` |

Dieser Plan ist auf dem oben genannten `main`-Stand erstellt. Die Plan-SHA ist
erst nach dem Plan-Commit bekannt und wird nicht in diesen Inhalt
vorgezogen. Eine Umsetzung, ein Hardware-Spike, ein Ready-Wechsel und ein
Merge sind bis zur Ownerfreigabe genau dieser Plan-SHA ausgeschlossen.

## 1. Ausgangslage und aktuelle Live-Baseline

Vor diesem Plan wurden Repository, Branch, `HEAD`, Issue #31, die vorhandenen
Draft-/Merge-Zustaende, Roadmap, Hardware-SSOT, die #25/#26-Vertraege, die
Audits und die Governancequellen live abgeglichen.

- `origin/main` und die Arbeitsbasis sind exakt
  `54c80d26416343495b4d9a8c4518e6137dc747c1`.
- Issue #31 ist offen und hardwareblockiert. Die offenen Werte bleiben
  `FUNCTIONAL_HARDWARE_VERIFICATION=PENDING` und
  `SSOT_CONFORMANCE=PENDING`; Lieferantentexte werden nicht als Controller-
  oder Funktionsnachweis akzeptiert.
- PR #155 / Issue #154 ist abgeschlossen: PR #155 ist gemergt und Issue #154
  geschlossen. Der in der Roadmap dokumentierte Plan ist
  `3824bf54f1aebc5e3453739fd083ab9c317ef868`; der PR-Source-Head war
  `4d759b381343f0d9f466ccd76f8c7504ee7177fb`; der Merge-Commit ist der
  aktuelle `main`-Head. Die parallele Governance-Arbeit ist kein Teil der
  #31-Fachlogik.
- Issue #25 / PR #142 und Issue #26 / PR #143 sind gemergt. #25 besitzt die
  rendererunabhaengigen Praesentations-, Text-, Theme-, Interaktions- und
  Commandvertraege; #26 besitzt die Workspace-/Navigation-/Press-/WakeOnly-
  Projektion. Diese Vertraege sind zu konsumieren, nicht zu duplizieren.
- Es gibt aktuell keinen vorhandenen #31-Draft-PR. Dieser Plan verwendet den
  separaten Arbeitsbranch und genau einen Draft-PR fuer Issue #31.

Die alten Audits sind Ausgangslage, aber keine aktuelle Versions- oder
Produktionsauswahl. Die Upstream-Pruefung dieses Plans wurde am 2026-09-06
gegen primaere Quellen neu gestartet und bindet die Auswahl an eine erneute
reproduzierbare Stage-1-Pruefung mit der fixierten lokalen ESP-IDF-6.0.2-
Toolchain.

## 2. Ziel, Nichtziele und unveraenderliche Grenzen

### Ziel

Issue #31 soll nach realem Hardwarebeweis die kleinste produktionsnahe
Integration bereitstellen, die die vorhandene #26-UI auf dem lokalen
320x240-Display darstellt, Touch-Ereignisse sicher an die bestehenden
Contracts zurueckgibt und Kalibrierung, WakeOnly, Raw-Touch-Recovery,
Fehlerisolation, Ressourcen- und Lizenznachweise reproduzierbar belegt.

Die technische Stackwahl bleibt bis zur Stage-4-Auswahl ergebnisoffen. Der
Plan bestimmt die Evaluationsreihenfolge, aber keine unbelegte
Produktivauswahl.

### Nichtziele

- Keine Implementation vor Ownerfreigabe der exakten Plan-SHA.
- Kein Hardware-Spike, keine Hardware-PASS-Aussage und keine elektrische
  Messung ohne einen konkret offenen, sinnvollen Hardwarepunkt.
- Keine neue UI-, Command-, Navigation-, PIN-, Recovery-, Programm- oder
  Persistenzlogik neben den gemergten #25/#26- und bestehenden Recovery-
  Vertraegen.
- Keine ESP-IDF-, Arduino-, LVGL-, Display- oder Touchtypen in
  `fermentation_app`; `device_platform` bleibt anwendungsneutral und
  rendererfrei.
- Keine neue generische Renderer-, Plugin-, Provider-, Widget- oder
  Discoveryarchitektur auf Vorrat.
- Keine GPIO-, Reset-, Backlight- oder Bus-Neuzuordnung. Das Boardprofil ist
  weiterhin die einzige R1-GPIO-SSOT.
- Kein projektweiter Wechsel zu Arduino oder Zephyr. Ein echter fundamentaler
  ESP-IDF-Blocker waere eine separate Ownerentscheidung, nicht Teil einer
  stillen #31-Umsetzung.
- Keine vorsorglichen mehreren vollstaendigen Rendererimplementierungen,
  keine OTA-Bibliotheken, keine Slots und keine PSRAM-Abhaengigkeit.
- Keine Aktorfreigabe. Peltier, BTS7960, Innen-/Aussenluefter,
  MOSFET-Verbraucher und Summer bleiben getrennt oder nachweislich inaktiv.

## 3. Vertrags- und Modulgrenzen

| Verantwortung | Bestehender Vertrag / spaetere Umsetzung |
|---|---|
| `fermentation_app` | Besitzt Fachzustand, `FermentationUiSnapshot`, `FermentationUiProjector`, `FermentationTouchWorkspace`, bestehende typed UI commands, Recovery-/Serviceintents und die Semantik von `WakeOnly`. Keine Treibertypen und keine Widget-State-Machine. |
| `device_platform` | Bleibt bei anwendungsneutralen, schmalen Ports und Diensten. Der aktuelle Repository-Schnitt besitzt noch keinen Display-/Touch-/Backlight-Port; der Builder darf nachgewiesen genau die benoetigten neutralen Hardwareports als additive Luecke ergaenzen, aber keine UI-/Fachtypen, LVGL-Typen, GPIO-Details oder ESP-IDF-Abhaengigkeit einfuehren. |
| `device_platform_esp_idf` | Implementiert die konkreten ESP-IDF-/Treiberadapter fuer die neutralen Ports, SPI-Panel, Touch-Sampling und Backlight. Keine Abhaengigkeit auf `fermentation_app`, keine Navigation, keine Fach- oder Composition-Root-Logik. |
| Bestehende ESP-IDF-Composition-Komponente `main/` | Ist der einzige app-spezifische konkrete Integrationsowner. Kleine lokale Helper-Dateien unter `main/` duerfen `fermentation_app`-Workspace-/Presentation-Modelle und den spaeter ausgewaehlten konkreten Renderer kennen. Sie enthalten keine Fachentscheidung und keine zweite UI-State-Machine. |
| `main/app_main.cpp` | Bleibt ausschliesslich Composition Root: instanziiert Plattform, App, konkrete Low-Level-Adapter und die lokalen Renderer-/Presentation-Helper, verdrahtet Lebenszyklus/Update und besitzt selbst keine Widget-, Layout-, Renderer- oder Touchlogik. |
| Test-Support | Renderer- und Adaptertests bleiben von Produktions-App-Abhaengigkeiten getrennt. Hardware-Smoke- und Ressourcennachweise laufen als actor-free, reproduzierbare Profile. |

### Explizite Boundary-Entscheidung

Der aktuelle Produktionsschnitt und der Guard wurden geprueft: Es gibt bereits
die rendererunabhaengigen `FermentationUiSnapshot`-,
`FermentationUiProjector`-, `FermentationTouchWorkspace`- und
`FermentationUiCommandBridge`-Vertraege in `fermentation_app`, aber noch kein
konkretes Display-/Touch-Integrationsmodul. `main/app_main.cpp` ist die
ESP-IDF-Composition Root; `device_platform_esp_idf` darf laut lokaler Regel
keine App- oder Composition-Abhaengigkeit erhalten. `main/CMakeLists.txt` ist
bereits die erlaubte konkrete Anwendungskomponente und besitzt die fuer die
bestehende Composition erforderlichen privaten Abhaengigkeiten.

Daher ist `main/` die bestehende und kleinste app-spezifische Ownergrenze
dieses Plans. Die erwarteten Grenzen nach Planfreigabe sind:

- `lib/device_platform/src/device_ui_hardware_ports.hpp`: nur falls der
  bestaetigte Port-Gap dies benoetigt, die schmalen neutralen Interfaces fuer
  Display-Flush/Rotation, Raw-Touch und Backlight; keine Widget-, Route-,
  PIN-, Kalibrierungs- oder ESP-IDF-Typen;
- `lib/device_platform_esp_idf/src/esp_idf_display_touch_adapter.hpp/.cpp`:
  konkrete SPI-/Panel-/Touch-/Backlight-Adapter hinter diesen Ports, ohne
  `fermentation_app`-Include und ohne App-Entscheidungen;
- `main/fermentation_ui_renderer.hpp` und
  `main/fermentation_ui_renderer.cpp` (oder eine gleich kleine lokale
  `main/`-Unterstruktur): genau ein app-spezifischer Integrationsowner. Er
  kennt die #26-Workspace-View, liest den bestehenden Snapshot/Projector,
  zeichnet den ausgewaehlten repraesentativen Screen und leitet Press-
  Ergebnisse an vorhandene typed Application-/Commandpfade weiter;
- `main/app_main.cpp`: nur Konstruktion, Referenz-/Portverdrahtung und
  `begin`/`update`; keine konkrete Renderlogik.

`main/CMakeLists.txt` nimmt diese Helper nur als zusaetzliche Quellen derselben
bereits erlaubten Composition-Komponente auf. Die bestehenden direkten
Abhaengigkeiten auf `fermentation_app`, `device_platform` und
`device_platform_esp_idf` sind bereits im Guard abgebildet. Nur falls eine
spaetere konkrete Auswahl eine neue direkte Registry-/Framework-Abhaengigkeit
wirklich benoetigt, werden deren CMake-Requirement und die korrespondierende
Guard-Allowlist nach Stage 4 angepasst.

Dieser bestehende `main`-Owner ist keine generische Renderer- oder
Providerplattform. Seine Abhaengigkeiten sind konkret und einseitig:
`fermentation_app` fuer die bestehenden Modelle/Commands,
`device_platform` fuer neutrale Ports und `device_platform_esp_idf` fuer die
konkrete Hardwareverdrahtung. Die ausgewaehlte Rendererbibliothek bleibt in
den lokalen `main`-Helpern und leakt nicht in `fermentation_app` oder
`device_platform`. Ein fehlender Snapshot-/Application-Zufluss wird, falls der
aktuelle oeffentliche App-Schnitt ihn nicht vollstaendig liefert, als additive
app-eigene Verwendung der vorhandenen Projector-/Commandvertraege geschlossen;
es wird kein zweiter Renderer- oder Plattformvertrag erfunden.

Der lokale `main`-Owner darf `press(...)` aufrufen und die bestehenden typed
`FermentationApplication`-/`FermentationUiCommandBridge`-Pfade verwenden. Er
besitzt keine zweite Route, Aktion, PIN-Pruefung, Recovery-Policy,
Fachzustandskopie oder Persistenz. Bei fehlendem Display oder Touch wird nur
die UI-Faehigkeit degradiert; Regelung, Safety und Aktorfreigabe laufen
unabhaengig und fail-closed weiter.

Eine Erweiterung von ADR-013, Modulindex oder lokalen Modulregeln ist fuer
diese `main`-Loesung nicht erforderlich. Sollte sich `main` in der
Implementierungsplanung nachweislich als ungeeignet erweisen, stoppt der Scope
vor einer neuen Produktionskomponente und benoetigt zuerst einen expliziten
Ownerentscheid mit normativer ADR-/Guard-/Modulindex-Revision.

## 4. Hardware-SSOT und Status ohne Vorwegnahme

Die reale Verdrahtung ist gegen
`config/board_profiles/esp32_32e_quad_mosfet_r1.yaml` zu pruefen. Das Profil
ist SSOT fuer die geplante Zuordnung, aber kein Funktionsnachweis:

| Signal | R1-SSOT | Status vor Hardware |
|---|---:|---|
| SPI SCK | GPIO18 | `PLANNED`, nicht bestaetigt |
| SPI MISO | GPIO19 | `PLANNED`, nicht bestaetigt |
| SPI MOSI | GPIO23 | `PLANNED`, nicht bestaetigt |
| TFT CS | GPIO5 | `PLANNED`, active-low/safe-high, nicht bestaetigt |
| TFT D/C | GPIO2 | `PLANNED`, nicht bestaetigt |
| TFT Backlight | GPIO4 | `PLANNED`, PWM/safe-off, nicht bestaetigt |
| Touch CS | GPIO15 | `PLANNED`, active-low/safe-high, nicht bestaetigt |
| Touch IRQ | GPIO39 | `PLANNED`, input-only/active-low, nicht bestaetigt |
| Display-Reset | `EN_CHIP_PU -> MSP2807_RESET` | Netzvertrag `PLANNED`, gemeinsame active-low-Resetsemantik nicht bestaetigt |

Es gibt in #31 keine Umverteilung dieser Signale. Ein realer Widerspruch
zwischen Modul, Verdrahtung und Boardprofil stoppt die Umsetzung und benoetigt
eine separate Ownerentscheidung bzw. einen separaten SSOT-Plan. `TBD_HARDWARE`
und `TBD_COMMISSIONING` sind keine Laufzeitwerte.

## 5. Aktuelle Adopt-or-build-Recherche

Die folgenden Staende sind eine live verifizierte Kandidaten- und
Evaluationsbasis, keine Auswahl. Die Stage-1-Dokumentation muss Version,
aufgeloesten Commit, Lizenz und transitive Abhaengigkeiten nochmals im
reproduzierbaren ESP-IDF-6.0.2-Build festhalten.

| Kandidat / Baustein | Aktueller Primarquellenstand | Vorlaeufige Einordnung |
|---|---|---|
| ESP-IDF `esp_lcd` | In der lokalen fixierten ESP-IDF 6.0.2 vorhanden; SPI-I/O, DMA-faehige Panel-I/O und Panel-APIs. Die lokale Version enthaelt keinen ILI9341-Paneltreiber. | Basis fuer die erste Evaluationsstufe; adoptieren, soweit der konkrete Panel-/Touchkandidat die Gates erfuellt. |
| `espressif/esp_lcd_ili9341` | Registry aktuell `2.1.0`, Apache-2.0; ILI9341 ueber `esp_lcd`. [Registry](https://components.espressif.com/components/espressif/esp_lcd_ili9341) | Erster Panelkandidat, noch kein Controller- oder Hardware-PASS. |
| `espressif/esp_lcd_touch` | Registry aktuell `1.2.1`, Apache-2.0; XY-Lesen, Swap/Mirror, IRQ-Callback und Sleep; keine fertige Kalibrierung im Baustein. [Registry](https://components.espressif.com/components/espressif/esp_lcd_touch) | Gemeinsame Touch-Basis, Kalibrierungs-/Validierungsvertrag bleibt projektspezifisch. |
| XPT2046 | Live Registry-Suche zeigt aktuell keinen geeigneten `espressif/*`-XPT2046-Kandidaten. `atanisoft/esp_lcd_touch_xpt2046` ist aktuell `1.0.6`, MIT; Raw-/Z-Schwelle, IRQ/Polling und abschaltbare Konvertierung sind vorgesehen. [Registry](https://components.espressif.com/components/atanisoft/esp_lcd_touch_xpt2046) | Bestehender Drittanbieter-Kandidat erst nach Gate 1; kein Ersatz fuer reale Controlleridentifikation. |
| `espressif/esp_lvgl_port` | Registry aktuell `2.9.0`, Apache-2.0; LVGL 8/9, LVGL-Task/Timer, `esp_lcd`-Display, `esp_lcd_touch`, Locking, Rotation und konfigurierbare DMA-/Partial-Buffer. [Registry](https://components.espressif.com/components/espressif/esp_lvgl_port) | Erst nach Treiber-/Screen-Smoke gegen eine schlanke Projektion vergleichen; noch keine LVGL-Auswahl. |
| `espressif/esp_bsp_generic` / `esp-bsp` | Registry aktuell `3.1.1`, Apache-2.0. Der generische BSP deckt u.a. SPI-ILI9341-Displays, aber die dokumentierten Touchpfade sind I2C-orientiert und nicht der konkrete XPT2046-SPI-Aufbau. [Registry](https://components.espressif.com/components/espressif/esp_bsp_generic), [README](https://github.com/espressif/esp-bsp/blob/master/bsp/esp_bsp_generic/README.md) | Scope-Pruefung erlaubt, aber voraussichtlich unnoetiger Umfang und kein passender Komplettfit fuer dieses Board. Nicht vorsorglich adoptieren. |
| LVGL | Upstream aktuell `9.5.0`, MIT. Der exakte aufgeloeste Commit wird im Stage-1-Lock festgehalten. [Releases](https://github.com/lvgl/lvgl/releases), [Lizenz/Version](https://github.com/lvgl/lvgl/blob/master/library.json) | Nur als Vergleichskandidat nach gleichem Screen, gleicher Hardware, gleichem Treiber und gleicher Messmethode. |
| LovyanGFX, TFT_eSPI, LCDWiki | Die kanonische Auditmatrix bleibt als Alternativmatrix erhalten; alte Versionsangaben werden nicht ungeprueft uebernommen. | Reihenfolge nach offiziellem Stack; nur die bestehende Stufenlogik, kein Ausbau zu parallelen Vollimplementierungen. |

Der aktuelle offizielle Stack wird deshalb zuerst gegen den R1-Vertrag
geprueft. Wenn er alle Gates erfuellt, ist keine eigene Display-/Touch-
Treiberimplementierung zulaessig. Projektspezifisch bleiben nur die schmale
Composition, vorhandene UI-Projektion, Wake-/Recovery-Semantik,
Kalibrierungsdaten und deren Tests. `esp_bsp_generic` wird nicht als
automatische Abkuerzung verwendet.

Die technische API-Basis fuer die Panelpruefung ist die offizielle
[ESP-IDF SPI-LCD-Dokumentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/peripherals/lcd/spi_lcd.html).
Die produktive Kompatibilitaet muss trotzdem mit der lokal fixierten
ESP-IDF-6.0.2-Toolchain und deren exakten Lockdaten bewiesen werden.

## 6. Stufenplan und Hardware-Evidence-Matrix

Die bestehende Stufenlogik aus `docs/audits/HARDWARE_SPIKE_PLAN.md` bleibt
unveraendert und ist eine sequentielle Ausfuehrungskette:

```text
Stufe 0: reale Hardware und minimale sichere Hardwarebaseline
    -> Stufe 1: Quelle/Lizenz/Kompatibilitaet/reproduzierbarer Build
    -> Stufe 2: kurzer identischer Hardware-Smoke
    -> Stufe 3: vollstaendige identische Funktions-/Fehler-/Ressourcenmatrix
    -> Stufe 4: genau eine Low-Level-Produktivrichtung und hoechstens ein Rueckfall
```

Jede Stufe erzeugt ein reproduzierbares Evidence-Artefakt mit `PASS`, `FAIL`,
`BLOCKED` oder `NOT_RUN`; fehlende Messungen sind nicht bestanden. Die
Upstream-/Lizenzrecherche in Abschnitt 5 ist ausschliesslich
Plan-/Vorbereitungsrecherche. Sie ist kein bestandenes Gate und darf keinen
Stage-1-PASS ersetzen. Ebenso darf ein Build erst als Stage-1-Evidence
gelten, wenn Stufe 0 einschliesslich der Baseline abgeschlossen ist.

### Stufe 0 – reale Hardware identifizieren und sichere Baseline nachweisen

Vor jeder Bibliotheksbewertung und vor jedem aktiven Display-/Touchtest wird
die minimale sichere Hardwarebaseline aus dem kanonischen Spikevertrag
dokumentiert und nachgewiesen. Das umfasst:

- reale ESP32-Boardrevision und exakte MSP2807-/Displaymarkierung, Fotos und
  Liefer-/Bestellinformationen;
- praktisch ermittelten Displaycontroller und Touchcontroller; ILI9341 und
  XPT2046 bleiben bis dahin Kandidatennamen, nicht PASS;
- reale TFT-/Touch-CS-, D/C-, Reset-, Backlight-, IRQ-, SPI- und
  Masseverbindungen gegen das Boardprofil;
- Versorgung und sichere Einspeisung sowie Logikkompatibilitaet des konkreten
  Moduls; kein generisches neues Pegelmessgate;
- reproduzierbare Verbindung ueber UART beziehungsweise FT232RL sowie
  Flash-, Boot- und Resetablauf;
- reale Flashgroesse;
- verwendete und fixierte ESP-IDF-6.0.2-Toolchain mit den Profilen
  `esp32_bringup`/`esp32_release`;
- Betrieb ohne PSRAM;
- Baseline-Firmwaregroesse, statisches RAM, freier Heap und groesster freier
  Heapblock;
- verfuegbare GPIOs und grundsaetzlich moegliche Busse gegen die SSOT, ohne
  eine produktive Belegung neu festzulegen;
- physische Trennung oder nachweisliche Inaktivitaet von Peltier, BTS7960,
  Innen-/Aussenlueftern, allen MOSFET-Verbrauchern und Summer;
- Resetnetz `EN_CHIP_PU -> MSP2807_RESET` und Boot-/Reset-Safe-Zustaende.

Die physische Identitaet und Verdrahtung sind Evidence, keine Owner-
Bestaetigung. Der Owner stellt Modul, Zugriff und actor-free Testbedingungen
bereit; der Builder dokumentiert die gemessenen/verifizierten Tatsachen mit
Quelle, Methode und Status. Ein Lieferantentext ohne praktische Identifikation
ist `BLOCKED`, nicht `PASS`. Abweichungen vom Boardprofil stoppen #31 und
erfordern einen separaten SSOT-/Ownerentscheid.

### Stufe 1 – Quelle, Lizenz, Kompatibilitaet und reproduzierbarer Build

Erst nach bestandenem Stufe-0-Evidencepaket und fuer den offiziellen Stack
zuerst auszufuehren:

1. Registry-/Upstreamquelle, exakte Version und aufgeloesten Commit fuer jede
   direkte Komponente erfassen; keine schwebenden Git-Referenzen.
2. SPDX-/Lizenznachweis und Lizenz-/Notice-Dateien fuer direkte und relevante
   transitive Abhaengigkeiten erfassen; Fonts, Assets und generierte Dateien
   einschliessen.
3. ESP-IDF 6.0.2, ESP32-32E, C++17, 4 MB Flash und **kein PSRAM** in den
   Kandidatenprofilen reproduzierbar bauen; Build-Warnungen und Konfiguration
   festhalten.
4. Die in Stufe 0 erhobene Baseline gegen den Kandidaten mit denselben
   Buildflags, Boardprofilen und
   Evidence-Skripten messen. Der Lock-/Manifeststand wird versioniert, sobald
   eine Komponente fuer Umsetzung angenommen wird.
5. `esp_bsp_generic` nur auf konkrete Mehrwerte und zusaetzliche
   Abhaengigkeiten pruefen; kein BSP-Scaffold nur fuer Bequemlichkeit.

Die Quellen-/Lizenzvorpruefung und ein geplanter Buildaufbau durften bereits in
der Planphase vorbereitet werden; dieses ausgefuehrte Gate steht aber
sequenziell nach Stufe 0. Ein vorbereiteter oder lokaler Build vor Stufe 0 ist
kein Stage-1-PASS und kein Grund, Stufe 0 zu ueberspringen.

### Stufe 2 – kurzer identischer actor-free Hardware-Smoke

Erst nach bestandenem Stufe-0-Baselinepaket **und** bestandenem Stage-1-
Buildgate, mit allen Aktoren getrennt/inaktiv, fuer jeden ernsthaft
verbleibenden Kandidaten identisch:

- Kaltstart, Reset, Panel-Init, Landschaft 320x240, vier Ecken;
- Schwarz/Weiss/Rot/Gruen/Blau, ASCII und DE/EN/ES-Text;
- Touch-Init, Raw-Werte an Ecken und Mitte, Kontakt-/Druckverhalten;
- getrennte CS-Zugriffe und alternierender gemeinsamer SPI-Bus;
- mindestens fuenf Resets, kein Full-Framebuffer und keine PSRAM-Annahme;
- sichere Backlight-/Reset-Zustaende und sichtbare Rotation;
- keine produktive Navigation, keine PIN-/Recoveryaktion und keine
  Aktorfreigabe.

Stufe 2 ist nur ein kurzer Smoke. Einzelne sichtbare Zeichen oder ein
Lieferantensample beweisen weder Controller, Kalibrierung noch Produktreife.

### Stufe 3 – vollstaendige identische Funktions-, Fehler- und Ressourcenmatrix

Nach bestandenem Smoke werden fuer den offiziellen Stack und nur begruendete
Alternativen dieselben Tests ausgefuehrt:

**Darstellung und Eingabe**

- 100 Vollflaechenaktualisierungen je Farbe, DE/EN/ES mit den vorgesehenen
  Fonts, Titel, zwei Temperaturwerte, Status, vier grossen Buttons und Dialog;
- Raw-Touch an Ecken, Kanten und Mitte inklusive Kontakt-/Druckverlauf. Diese
  Punkte sind Mess-/Validierungspunkte der Hardwarematrix, nicht automatisch
  Produktiv-Kalibrierpunkte;
- Kalibrierung mit der aus dem gewaehlten Transformationsmodell abgeleiteten
  Anzahl und Lage von Punkten, Neustart und Wiederholungsmessung; eine
  feste Fuenf-Punkt-Produktivkalibrierung wird nicht vorweggenommen;
- korrekte Rotation und Touch-Transformation gemeinsam pruefen;
- 1000 wechselnde Touch-/Draw-/Statuszyklen mit Fehler- und Latenzprotokoll;
- gemeinsamer SPI-Bus abwechselnd fuer Display und Touch.

**Wake, Recovery und Fehlerisolation**

- erster Touch nach Dimmung/Schlaf ist immer `WakeOnly` und loest keine
  Fachaktion, Navigation oder PIN-/Recoveryaktion aus;
- Geraet einschalten und Raw-Touch mindestens 10 Sekunden halten: Bei fehlender
  oder ungueltiger Kalibrierung wird im fruehen Boot-/`SAFE_BOOT`-Fenster
  ausschliesslich PIN-unabhaengige Kalibrierungs-Recovery gestartet. Der
  kanonische `>=10 s`-Vertrag wird nicht neu vermessen;
- Raw-Recovery hat False-Trigger-Schutz, eine definierte Release-/
  Abbruchbedingung und keine spaete Nachausloesung. Raw-Geste innerhalb des
  10-Sekunden-Vertrags, Roh-/Kontakt-/Druckgrenzen, Entprellung und
  Verwechslungsschutz bleiben hardwareabhaengige Evidence;
- fehlender, unlesbarer oder fehlerhafter Touch darf Regelung, Safety und
  Aktorfreigabe nicht blockieren und nicht freigeben;
- Display-/Backlight-/SPI-/Touchfehler werden isoliert, geloggt und fuehren
  zu einem sicheren UI-/Recovery-Fallback; UART bleibt der moegliche
  Recoverypfad;
- Resettests im Idle, bei Darstellung und bei Touch, inklusive Watchdog-/Boot-
  Verhalten und actor-free Wiederanlauf;
- normaler Kalibrierungsablauf: Service-PIN plus bewusste Bestaetigung;
- PIN-unabhaengige Raw-Touch-Kalibrierungs-Recovery: ausschliesslich
  Kalibrierung, actor-free und `>=10 s` Raw-Touch-Halten;
- PIN-unabhaengiger vollstaendiger Werksreset: eigener eindeutig
  unterscheidbarer Ablauf;
- autorisierter Factory Reset behaelt die geraetespezifische Touchkalibrierung
  gemaess ADR-010; ein gesonderter Touch-Kalibrierungsreset bleibt davon
  getrennt und darf die Daten entfernen/invalidieren.

**Ressourcen und Stabilitaet**

Fuer Baseline und jeden verbleibenden Kandidaten mit gleicher Messmethode:

- Flashgesamtbedarf und relevante Partition-/Binarygroesse;
- DRAM, IRAM, freier Heap direkt nach Boot, niedrigster freier Heap und
  groesster freier Heapblock;
- relevante Task-Stacks/HWM inklusive Renderer-/LVGL-Task und Hauptloop;
- DMA-/Displaybuffer, Partial-Buffer und Touchpuffer;
- 320x240-Aktualisierungszeit, Fehlerrate und Stabilitaet bei Touch plus
  Renderer plus bestehender Firmware;
- Resets, Watchdog, Busfehler, Double Events, Drift und Speicherfehler;
- explizit keine PSRAM-Abhaengigkeit.

Es werden keine willkuerlichen neuen harten Budgets erfunden. Die Messwerte
werden gegen das Gesamtsystem, die bestehende Reserve und den Owner bewertet;
bis dahin bleibt `TBD_IMPLEMENTATION_BUDGET` ungueltig als Laufzeitwert.

### Stufe 4 – genau eine bevorzugte Low-Level-Produktivrichtung und ein Rueckfall

Erst nach vollstaendiger Stage-3-Matrix, Lizenz-/Herkunftsnachweis und
unabhaengiger Bewertung wird genau eine bevorzugte Richtung und hoechstens der
bereits im Audit vorgesehene Rueckfallkandidat dokumentiert. Kriterien sind
Funktion, Stabilitaet, Ressourcenreserve, Buildreproduzierbarkeit, Lizenz,
Upstreampflege und Adapter-/Wartungsumfang.

Stufe 4 entscheidet zuerst ausschliesslich den Display-/Touch-Low-Level-Stack
und dessen schmalen neutralen Adaptervertrag. LVGL nimmt an Stufe 0 bis 4
nicht teil und wird nicht als vorgezogene Frameworkentscheidung behandelt.

Die heutige Evaluationsreihenfolge ist daher:

1. offizieller `esp_lcd`-/`esp_lcd_ili9341`-/`esp_lcd_touch`-Stack mit dem
   verifizierten XPT2046-Kandidaten;
2. LovyanGFX;
3. TFT_eSPI;
4. LCDWiki-Paket;
5. Arduino_GFX oder Adafruit GFX/ILI9341/XPT2046 nur bei den bereits
   definierten Reservebedingungen (zu wenige Smoke-Passer, Lizenz-/Build-
   Blocker oder materialer R1-Vorteil).

Diese Reihenfolge ist keine Produktivauswahl. Es werden nicht vorsorglich alle
Kandidaten voll integriert.

## 7. Kleine Renderer-/LVGL-Integrationsgrenze

Die Reihenfolge nach der Low-Level-Auswahl ist verbindlich:

```text
Stufe 0 bis 4: Display-/Touch-Low-Level-Stack qualifizieren und auswaehlen
    -> schmalen neutralen Adaptervertrag festlegen
    -> einen identischen repraesentativen #26-Screen vorbereiten
    -> schlanke konkrete Projektion und LVGL/esp_lvgl_port vergleichen
    -> Ownerentscheidung: LVGL nur bei klarem gemessenem R1-Vorteil,
       sonst LVGL=DEFER_AFTER_R1
```

Erst nach der Stage-4-Treiberwahl und der festgelegten Adaptergrenze werden
beide Varianten mit derselben Hardware, demselben ausgewaehlten Treiberstack,
denselben DE/EN/ES-Texten, denselben Eingabeelementen und derselben
Messmethode verglichen. Der Vorabstand von `esp_lvgl_port` und LVGL ist nur
Desk Research; `LVGL_SELECTION=DEFERRED_UNTIL_POST_STAGE4_DRIVER_SELECTION`.

Wenn LVGL nach diesem Vergleich den Ownerentscheid erhaelt, bleibt die
Integration klein:

- `esp_lvgl_port` besitzt die LVGL-Initialisierung, Tick-/Timer-Anbindung,
  den seriellen Lock/Unlock-Kontext und den LVGL-Task;
- `esp_lcd` besitzt Panel-IO/Flush und DMA-Teilbuffer; `esp_lcd_touch` und
  der XPT2046-Adapter liefern Touchdaten;
- der konkrete Renderer besitzt nur die Widgets/Drawables fuer die bestehende
  #26-Workspace-View und ruft fuer Bedienung `Workspace::press(...)` bzw. den
  vorhandenen typed Commandpfad auf;
- keine App-Fachlogik, kein zweiter UI-State und keine Parallelkopie des
  Recovery-/PIN-Zustands in LVGL;
- Rotation wird entweder hardwareseitig oder softwareseitig genau einmal
  angewandt und fuer Display und Touch gemeinsam verifiziert;
- Teilbuffer und DMA werden gegen Heap/IRAM/DRAM gemessen; Full-Framebuffer
  und PSRAM bleiben ausgeschlossen;
- Backlight-Dimming ist ein konkreter, fail-closed Composition-/Adapterpfad;
  Boot/Reset startet safe-off. Ein Touch beim Aufwachen bleibt `WakeOnly`.

Wenn kein klarer gemessener R1-Vorteil vorliegt, lautet die Entscheidung
`DEFER_AFTER_R1`. Dann wird kein allgemeiner Rendererrahmen gebaut: Es gibt nur
eine konkrete, lokal gebundene schlanke Darstellung fuer die vorhandene
Workspace-View mit demselben typed Event-Rueckweg.

In beiden Varianten gilt:

- `fermentation_app` sieht keine LVGL-, ESP-IDF-, Display- oder Touchtypen;
- `device_platform` exponiert nur die nachgewiesenen neutralen Display-/Touch-/
  Backlightports;
- der konkrete Low-Level-Adapter lebt in `device_platform_esp_idf`;
- der app-spezifische konkrete Renderer lebt ausschliesslich in den kleinen
  lokalen `main/fermentation_ui_renderer.*`-Helpern und kennt die
  #26-Workspace-View;
- `main/app_main.cpp` bleibt die Lebenszyklus-/Composition-Grenze und erzeugt
  keine neue Fachzustandsmaschine oder Renderlogik;
- eine UI-Stoerung setzt nur die UI-/Input-Faehigkeit herab. Die
  Regelungs-/Safety-Schleife und Aktorfreigabe werden weder auf UI-Callbacks
  angewiesen noch durch UI-Fehler freigegeben;
- Assets und Fonts bleiben auf den benoetigten DE/EN/ES-Umfang begrenzt; ihre
  Groesse, Lizenz und Herkunft gehen in die Ressourcenmatrix ein.

## 8. Touch-Rohdaten, Kalibrierung und Recovery

### Daten- und Validierungsmodell

Der Adapter liefert ein neutrales Rohereignis mit mindestens Roh-X, Roh-Y,
Kontakt-/Druckinformation, Controller-/Samplestatus und monotonem
Zeitbezug. Der unveraenderte Recoveryvertrag lautet: Geraet einschalten,
Raw-Touch mindestens 10 Sekunden halten, Beruehrung ohne gespeicherte
Kalibrierung erkennen und ausschliesslich Kalibrierungs-Recovery starten.
Exakte Rohgrenzen, Z-Schwellen, die konkrete Raw-Geste innerhalb dieses
Vertrags, Entprellung/Stabilitaet, Verwechslungs- und Kontaktgrenzen sowie
Transformparameter bleiben bis zur Messung `TBD_HARDWARE`.

Die Transformationsform wird erst anhand realer Daten festgelegt. Sie darf
Achstausch, Spiegelung und Rotation abbilden und bei Bedarf ein gemessenes
lineares Modell verwenden; erfundene Rohgrenzen oder scheinbare
Lieferantenwerte sind unzulaessig. Jede Kalibrierung hat eine eigene
Versions-/Schemakennung, Controller-/Boardbezug, Plausibilitaetspruefung und
eine eindeutige Invalidierungsregel.

### Bestehende Persistenz wiederverwenden

Es wird kein zweiter Speicherpfad, keine neue NVS-Datenbank und kein
kalibrierungsspezifischer Parallel-Codec angelegt. Die Implementierung muss
den bestehenden `IStateStore`-/NVS-/versionierten Envelope- und Recoverypfad
verwenden. Falls dort ein expliziter Datensatz fehlt, wird genau ein
geraetespezifischer Kalibrierungsdatensatz in diesem bestehenden Pfad als
kleine additive Erweiterung geplant und getestet.

Eine fehlende, unbekannte, ungueltige oder nicht zum realen
Board-/Controllerbezug passende Kalibrierung deaktiviert normale Touch-
Fachaktionen fail-closed, beeinflusst aber nicht Regelung oder Safety. Der
autorisierte Factory Reset behaelt den Kalibrierungsdatensatz gemaess ADR-010;
ein expliziter Touch-Kalibrierungsreset darf ihn loeschen oder invalidieren.

### Drei getrennte Kalibrierungs-/Recoverywege

1. **Normale Touchkalibrierung:** ueber den bestehenden Service-/Workspace-
   Vertrag, mit Service-PIN und bewusster Bestaetigung. Die Anzahl und Lage
   der Produktivpunkte wird aus dem gewaehlten Transformationsmodell sowie
   Fehler-/Reproduzierbarkeitsmessungen abgeleitet; keine feste Fuenf-Punkt-
   Vorgabe.
2. **PIN-unabhaengige Raw-Touch-Kalibrierungs-Recovery:** beim Einschalten
   mindestens `10 s` Raw-Touch halten, ohne brauchbare gespeicherte
   Kalibrierung. Dieser Weg startet ausschliesslich Kalibrierung, bleibt
   actor-free und darf weder Werksreset noch Fachaktion ausloesen.
3. **PIN-unabhaengiger vollstaendiger Werksreset:** eigener eindeutig
   unterscheidbarer Ablauf; er bleibt vom Raw-Touch-Kalibrierungsweg getrennt.

Fuer Weg 2 werden False Trigger durch sichere Bootphase,
Kontaktstabilitaet, explizite Release-/Abbruchsemantik und die spaeter
gemessenen Roh-/Zeitgrenzen verhindert. Ein gehaltenes Touchsignal erzeugt
keine wiederholte spaete Aktion. Der `>=10 s`-Wert ist kanonisch entschieden
und wird nicht in Stage 3 neu bestimmt; nur die konkrete Geste innerhalb des
Vertrags sowie Roh-/Kontakt-/Druckgrenzen, Entprellung und
Verwechslungsschutz bleiben Hardware-Evidence.

Der erste Touch nach Dimmung/Schlaf ist unabhaengig von Kalibrierung und PIN
immer `WakeOnly`; erst ein spaeteres, neues Touchereignis darf die vorhandene
Fachaktion erreichen.

## 9. Owner-Hardwarecheckliste fuer die spaetere Durchfuehrung

Vor Stage 0 benoetigt der Builder vom Owner Zugang und Testbedingungen, nicht
eine Owner-Bestaetigung physischer Tatsachen:

1. Modul, Fotos/Markierungen, Boardrevision, UART/FT232RL-Zugang und die
   Moeglichkeit, die reale Display-/Touchcontrolleridentitaet praktisch zu
   ermitteln.
2. Zugang zur realen Verdrahtung von SCK/MISO/MOSI, TFT-CS, D/C, Reset,
   Backlight, Touch-CS, IRQ und GND gegen das Boardprofil. Der Builder
   dokumentiert Konformitaet oder Abweichung als Evidence; eine Abweichung ist
   kein Owner-PASS und stoppt vor Stage 1.
3. Sichere Einspeisung und die Moeglichkeit, Versorgung/Logikkompatibilitaet
   nur fuer den konkret offenen Hardwarepunkt zu pruefen. Kein pauschales
   Spannungs-/GPIO-Gate.
4. Aktorfreie Testbedingungen: Peltier, BTS7960, Innen-/Aussenluefter,
   MOSFET-Verbraucher und Summer physisch getrennt oder nachweislich inaktiv;
   kein Test darf eine produktive Aktorfreigabe herstellen.
5. Reproduzierbarer Flash-, Boot- und Resetpfad, reale Flashgroesse,
   ESP-IDF-6.0.2-Toolchain, kein PSRAM sowie die Moeglichkeit, Baseline-
   Firmwaregroesse, statisches RAM, freien Heap, groessten Heapblock und
   Logs/Reset-/Watchdogdaten aufzuzeichnen.
6. Freigabe fuer die identische Stage-2-/Stage-3-Matrix und actor-free
   Wiederholungen. Fehlt ein Baselinepunkt, wird Stufe 0 `BLOCKED` und die
   folgenden Stufen `NOT_RUN`.

Die Owner-Hardwaremitwirkung stellt also Hardware, Zugriff und sichere
Testbedingungen bereit. Die reale Modul-/Board-/Controlleridentitaet,
Verdrahtungskonformitaet, Rotation, Raw-Grenzen, Druck-/Kontaktwerte und
Stabilitaetseigenschaften sind Builder-Evidence, keine Ownerentscheidungen.

## 10. Spaetere Umsetzungsschnitte nach Planfreigabe

Jeder Schnitt bleibt klein, wird gezielt verifiziert und darf den freigegebenen
Plan nicht materiell ueberschreiten:

| Schnitt | Inhalt | Ergebnis / Grenze |
|---:|---|---|
| 1 | Plan-/Vorbereitungsrecherche und Stage-0-Aufnahme | Registry-/Lizenz-/Versionsstand ist nur Desk Research; Stage 0 muss die vollstaendige sichere Hardwarebaseline als Evidence liefern. |
| 2 | Stage-1-Gate erst nach bestandenem Stage 0: gepinnte Kandidatenbuilds, Quellen, Lizenzen und Kompatibilitaet | Reproduzierbarer ESP-IDF-6.0.2-Build je Kandidat; kein Stage-1-PASS ohne Stage-0-Baseline, keine Aktoren. |
| 3 | Identischer Stage-2-Smoke fuer den offiziellen Stack und begruendete Alternativen | Nur Kandidaten mit bestandenem Stage 0 und Stage 1; keine produktive Navigation oder Auswahl. |
| 4 | Vollstaendige Stage-3-Matrix inklusive Low-Level-Funktion, Fehler, Raw-Recovery, Ressourcen und Lizenz | Identische Evidence; keine LVGL-Entscheidung und keine Hardware-PASS-Aussage ausserhalb realer Nachweise. |
| 5 | Stage-4-Auswahl des Low-Level-Display-/Touch-Stacks und eines Rueckfallkandidaten; neutralen Adaptervertrag festschreiben | Genau eine bevorzugte Low-Level-Richtung plus hoechstens ein Rueckfall; danach keine neue Treiberarchitektur. |
| 6 | Lokale `main/fermentation_ui_renderer.hpp/.cpp` mit bestehendem #26-Snapshot-/Workspace-/Commandpfad verdrahten | Bestehende `main`-Composition-Komponente bleibt Owner; `device_platform_esp_idf` bleibt app-frei, `fermentation_app` frameworkfrei, `main/app_main.cpp` bleibt reine Verdrahtung. |
| 7 | Identischer repräsentativer #26-Screen: schlanke konkrete Projektion gegen LVGL/`esp_lvgl_port` messen | Erst jetzt Ownerentscheidung: LVGL nur bei klarem gemessenem R1-Vorteil, sonst `DEFER_AFTER_R1`. |
| 8 | Tatsächliche konkrete Dependencies in den bestehenden Komponenten verdrahten und `scripts/check_architecture_boundaries.py` samt Guard-Selbsttests/Regressionnachweis aktualisieren | Nur ausgewählte `esp_lcd`-/Touch-/LVGL-Abhaengigkeiten allowlisten; keine vorsorgliche Allowlist; danach Workspace-/Command-Rueckweg, Backlight/WakeOnly, Kalibrierungs-/Persistenzpfad, Builder-Self-Check und Independent Review. |

Die Dateigrenzen sind fuer die Umsetzung bereits festgelegt: neutraler
Hardwareport nur bei bestaetigtem Gap unter
`lib/device_platform/src/device_ui_hardware_ports.hpp`, Low-Level-Adapter unter
`lib/device_platform_esp_idf/src/esp_idf_display_touch_adapter.*`, lokale
app-spezifische Renderer-/Presentation-Helper unter
`main/fermentation_ui_renderer.hpp/.cpp` und reine Konstruktion/Verdrahtung in
`main/app_main.cpp`. Dazu kommen gezielte native/ESP-IDF-Tests sowie die
notwendigen IDF-Komponenten-/Lockdateien.

Der bestehende Guard `scripts/check_architecture_boundaries.py` wird erst nach
der tatsächlichen Stage-4-/LVGL-Auswahl gegen den finalen Dependencygraphen
geprüft und nur bei einer legitimen neuen direkten CMake-Abhaengigkeit
angepasst. `device_platform_esp_idf` darf dann nur die tatsächlich
ausgewaehlten `esp_lcd`-/Touch-Komponenten erhalten; `main` darf nur bei einer
ausgewaehlten LVGL-Richtung deren konkret benoetigte Abhaengigkeit erhalten.
Die jeweiligen CMake-Allowlisten sowie die bestehenden Guard-Selbsttests und
der Regressionnachweis werden im selben Umsetzungsschnitt aktualisiert. Vor
Stage 4 werden keine Komponenten oder Dependencies allowgelistet. Keine
dieser Produktionsdateien oder Guard-Aenderungen existiert nach diesem
Plan-Commit; ihre spaetere Erstellung ist Implementation und bleibt bis zur
Ownerfreigabe verboten. `device_platform` und `fermentation_app` werden nur
bei dem jeweils nachgewiesenen neutralen bzw. app-eigenen Gap additiv
angepasst, nicht um eine zweite UI-Architektur zu schaffen.

## 11. Tests, Nachweise und Governance-Gates

### In dieser Planphase

Zulaessig sind nur Dokument-/Routingpruefungen, etwa Branch-/HEAD-/Issue-/PR-
Abgleich, `git diff --check` und die Pruefung der Plan-/Roadmapreferenzen.
Firmwarebuilds, native Volltests, ESP-IDF-Builds und reale Hardwaretests sind
in dieser Phase `NOT_RUN` und werden nicht als bestanden behauptet.

### Nach Ownerfreigabe der exakten Plan-SHA

- Stage-1-Kandidatenbuilds und gezielte native Contract-/Kalibrierungstests;
- Adapter-/Composition-Tests ohne App- oder Aktorfreigabe;
- gezielte ESP-IDF-Profiltests und actor-free Stage-2-/Stage-3-Evidence;
- Architektur-/Abhaengigkeitspruefung: kein LVGL/ESP-IDF in
  `fermentation_app`, kein neuer zweiter Contractpfad;
- `scripts/check_architecture_boundaries.py` auf dem tatsaechlichen finalen
  Dependencygraphen; CMake-Allowlisten nur fuer konkret ausgewaehlte direkte
  Abhaengigkeiten aktualisieren und die bestehenden Guard-Selbsttests sowie
  den Regressionnachweis mitpruefen;
- Ressourcen-, Lizenz- und Locknachweise auf demselben finalen Kandidaten;
- Builder-Self-Check und vollstaendiger unabhaengiger Review des aktuellen
  Diffs.

Die vollstaendigen lokalen Pre-Ready-Gates duerfen erst nach abgeschlossenem
Independent Full Review mit `OPEN_BLOCKERS=0` und ausdruecklicher Owner-
Autorisierung auf exakt dem finalen `HEAD` laufen. Der Builder setzt den PR
nicht auf Ready, mergt nicht und aktiviert kein Auto-Merge. Nach CI-/Review-
Abweichungen gilt die bestehende Fix-Verification-/Materialitaetsregel.

## 12. Akzeptanz des Plans und offene Ownerentscheidungen

Der Plan ist erst umsetzungsfreigabefaehig, wenn die exakte Ownerfreigabe vorliegt.
Die folgenden Punkte sind aktuell offen:

1. **Planfreigabe:** die neue exakte Plan-SHA als Ownerfreigabe erteilen.
2. **Stage-4-Auswahl:** nach Stage 0 bis 3 genau eine bevorzugte Low-Level-
   Treiberrichtung und hoechstens ein Rueckfallkandidat bestimmen.
3. **LVGL oder schlanke Projektion:** erst nach Stage 4 und dem identischen
   Screen-/Ressourcenvergleich entscheiden; LVGL nur bei klarem gemessenem
   R1-Vorteil, sonst `DEFER_AFTER_R1`.
4. **Widerspruch zum Boardprofil:** bei materieller Abweichung vor Umsetzung
   einen separaten SSOT-/Ownerentscheid einholen; die physische Tatsache selbst
   bleibt Evidence und wird nicht durch Ownerentscheidung bestaetigt.

Die konkrete Hardwareidentitaet, Boardrevision, Controller, Verdrahtung,
Rotation, Raw-Grenzen, Kontakt-/Druckwerte, Entprellung und
Verwechslungsschutz sind keine Ownerentscheidungen, sondern Stage-0-/Stage-2-/
Stage-3-Evidence. Der `>=10 s`-Raw-Touch-Recoveryvertrag ist bereits
entschieden; offen bleiben nur die hardwareabhaengigen Parameter innerhalb
dieses Vertrags.

Es gibt aktuell keinen nachgewiesenen fundamentalen ESP-IDF-Blocker. Sollte
Stage 1 einen solchen zeigen, wird ein Frameworkwechsel als separate
Ownerentscheidung behandelt und nicht in #31 implementiert.

### Plan-Abnahmekriterien

Der Plan gilt als vollstaendig, wenn die exakte Ownerfreigabe vorliegt und
folgende spaetere Evidence ohne unbelegte Vorannahmen abbildbar ist:

- aktuelle Espressif-/LVGL-Quelle, Version, Lizenz und Abhaengigkeiten als
  Plan-/Vorbereitungsrecherche sowie erneut als sequenzielles Stage-1-Gate;
- offizielle Stackpruefung vor eigener Entwicklung;
- unveraenderte #25/#26-Contracts und schmale Modulgrenzen;
- expliziter app-spezifischer Owner in der bestehenden `main`-Komponente ohne
  neue Produktions-Lib und ohne Ausweitung von `main/app_main.cpp` oder
  `device_platform_esp_idf`;
- konkrete Stage-0-bis-4-Matrix mit actor-free Hardwarebedingungen;
- Display, Touch, Rotation, Backlight, WakeOnly, Raw-Recovery,
  PIN-unabhaengige Kalibrierung mit `>=10 s`-Recovery und Fehlerisolation;
- keine feste Fuenf-Punkt-Produktivkalibrierung; Mess-/Validierungspunkte sind
  von Produktivpunkten getrennt;
- LVGL-Vergleich erst nach Low-Level-Stage-4 und neutraler Adaptergrenze;
- Persistenz ueber den bestehenden Pfad und Factory-Reset-Erhalt gemaess
  ADR-010;
- gleiche Ressourcen-/Stabilitaetsmessung ohne PSRAM und ohne willkuerliche
  harte Budgets;
- genau eine spaetere Auswahl plus hoechstens ein Rueckfall, keine
  vorgezogene Hardware- oder Produktiv-PASS-Aussage.
