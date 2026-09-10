# Issue #31 – Plan: realer Renderer, Display, Touch und Kalibrierung

## Planstatus und unveraenderliche Basis

| Feld | Wert |
|---|---|
| Issue | #31 – `[E5.3] Renderer, Display-/Touchadapter und Kalibrierung nach Hardwarebeweis` |
| Basisbranch | `main` |
| Basis-SHA | `54c80d26416343495b4d9a8c4518e6137dc747c1` |
| Arbeitsbranch | `agent/issue-31-renderer-display-touch-plan` |
| Planpfad | `docs/tasks/issue-31-renderer-display-touch-calibration-plan.md` |
| Reviewter Plan-HEAD | `1f31e6f17ec8003072cbf7b040d5e08a17050039` |
| Planstatus | `OWNER_PLAN_APPROVAL_PENDING_AFTER_FULL_REVIEW` |
| Implementation | `NOT_STARTED` |
| Hardware-Spike | `NOT_STARTED` |
| Renderer-Auswahl | `FINAL_SELECTION_PENDING` |
| LVGL-Auswahl | `DEFERRED_UNTIL_POST_STAGE4_DRIVER_SELECTION` |
| Renderer-Owner-Architektur | `EXISTING_MAIN_COMPONENT` |
| Neue Produktions-Lib-Komponente | `NO` |
| ADR-013-Erweiterung | `MINIMAL_APPLICATION_ADAPTER_BOUNDARY_CORRECTED` |
| Treiber-/Renderer-Schichtung | `CORRECTED` |
| Kalibrierungs-/Recovery-/Persistenz-Ownership | `CORRECTED` |
| Kalibrierungs-StorageEpoch-Vertrag | `CORRECTED` |
| Stage-0-Board-Revisionsgate | `CORRECTED` |
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
- PR #156 ist der einzige Draft-PR fuer Issue #31 und verwendet den separaten
  Arbeitsbranch `agent/issue-31-renderer-display-touch-plan`.

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

#### Normative Application-Adapterrolle nach ADR-013

Die plan-only ergaenzte ADR-013-Regel lautet:

```text
main/app_main.cpp
    -> nur Composition, Lebenszyklus und Verdrahtung

main/<application-adapter-helper>.*
    -> darf fermentation_app-View-Modelle kennen
    -> darf den ausgewaehlten UI-Renderer kennen
    -> keine Fachentscheidung
    -> keine Persistenzpolicy
    -> keine Recovery-/Safetypolicy
    -> keine zweite UI-State-Machine
```

Die Abhaengigkeitsrichtung ist normativ:

```text
main application adapter
    -> fermentation_app
    -> device_platform
    -> device_platform_esp_idf / ausgewaehlter Renderer

device_platform_esp_idf -X-> fermentation_app
fermentation_app -X-> ESP-IDF/LVGL/konkreter Treiber
```

Der spaetere Architektur-Guard schuetzt diese Rollen gezielt mit
Dateiklassen-, Include-, Symbol- und Negativfixtures fuer die verbotenen
Policies; eine Erweiterung von CMake-Allowlisten allein ist kein ausreichender
Nachweis. Die bestehende kleine `main`-Grenze bleibt damit der einzige
Application-Adapter. Eine allgemeine Rendererplattform oder
`lib/fermentation_ui_renderer_esp_idf/` wird nicht eingefuehrt.

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
`device_platform_esp_idf` sind bereits im Guard abgebildet. Die spaetere
Guard-Korrektur darf sich jedoch nicht auf CMake-Allowlisten beschraenken: Sie
schuetzt zusaetzlich die normative Rolle der beiden `main`-Dateiklassen, die
erlaubte Kenntnis des ausgewaehlten Renderers und die verbotenen Fach-,
Persistenz-, Recovery-/Safety- und zweiten UI-State-Machine-Anteile. Nur falls
eine spaetere konkrete Auswahl eine neue direkte Registry-/Framework-
Abhaengigkeit wirklich benoetigt, werden deren CMake-Requirement und die
korrespondierende Dependency-Allowlist nach Stage 4 angepasst.

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

ADR-013 wird im Plan-only-Scope minimal um diese Application-Adapterrolle
ergaenzt. Eine neue `lib/fermentation_ui_renderer_esp_idf/`-Komponente wird
nicht eingefuehrt, solange die kleine `main`-Adaptergrenze genuegt. Sollte sich
`main` in der Implementierungsplanung nachweislich als ungeeignet erweisen,
stoppt der Scope vor einer neuen Produktionskomponente und benoetigt zuerst
einen expliziten Ownerentscheid mit normativer ADR-/Guard-/Modulindex-Revision.

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
| ESP-IDF `esp_lcd` | In der lokalen fixierten ESP-IDF 6.0.2 vorhanden; SPI-I/O, DMA-faehige Panel-I/O und Panel-APIs. Die lokale Version enthaelt keinen ILI9341-Paneltreiber. | Rendererunabhaengige Low-Level-Panel-/Busgrundlage fuer Stage 1–4; adoptieren, soweit der konkrete Panel-/Touchkandidat die Gates erfuellt. |
| `espressif/esp_lcd_ili9341` | Registry aktuell `2.1.0`, Apache-2.0; ILI9341 ueber `esp_lcd`. [Registry](https://components.espressif.com/components/espressif/esp_lcd_ili9341) | Low-Level-Paneladapter fuer Controllerinitialisierung und Pixel-/Rechteck-/Flaechentests; noch kein Controller- oder Hardware-PASS. |
| `espressif/esp_lcd_touch` | Registry aktuell `1.2.1`, Apache-2.0; XY-Lesen, Swap/Mirror, IRQ-Callback und Sleep; keine fertige Kalibrierung im Baustein. [Registry](https://components.espressif.com/components/espressif/esp_lcd_touch) | Rendererunabhaengige Raw-Touch-/IRQ-/Polling-Basis; Kalibrierung und App-Recovery bleiben ausserhalb dieses Stacks. |
| XPT2046 | Live Registry-Suche zeigt aktuell keinen geeigneten `espressif/*`-XPT2046-Kandidaten. `atanisoft/esp_lcd_touch_xpt2046` ist aktuell `1.0.6`, MIT; Raw-/Z-Schwelle, IRQ/Polling und abschaltbare Konvertierung sind vorgesehen. [Registry](https://components.espressif.com/components/atanisoft/esp_lcd_touch_xpt2046) | Konkreter Low-Level-Raw-Touchadapter erst nach Gate 1; kein Ersatz fuer reale Controlleridentifikation und kein Recovery-Owner. |
| `espressif/esp_lvgl_port` | Registry aktuell `2.9.0`, Apache-2.0; LVGL 8/9, LVGL-Task/Timer, `esp_lcd`-Display, `esp_lcd_touch`, Locking, Rotation und konfigurierbare DMA-/Partial-Buffer. [Registry](https://components.espressif.com/components/espressif/esp_lvgl_port) | Post-Stage-4-LVGL-Integrationsweg, bevorzugt bei ausgewaehlter `esp_lcd`/`esp_lcd_touch`-Grundlage; kein Stage-0–4-Treiberkandidat und keine Vorabauswahl. |
| `espressif/esp_bsp_generic` / `esp-bsp` | Registry aktuell `3.1.1`, Apache-2.0. Der generische BSP deckt u.a. SPI-ILI9341-Displays, aber die dokumentierten Touchpfade sind I2C-orientiert und nicht der konkrete XPT2046-SPI-Aufbau. [Registry](https://components.espressif.com/components/espressif/esp_bsp_generic), [README](https://github.com/espressif/esp-bsp/blob/master/bsp/esp_bsp_generic/README.md) | Scope-Pruefung erlaubt, aber voraussichtlich unnoetiger Umfang und kein passender Komplettfit fuer dieses Board. Nicht vorsorglich adoptieren. |
| LVGL | Upstream aktuell `9.5.0`, MIT. Der exakte aufgeloeste Commit wird im Stage-1-Lock festgehalten. [Releases](https://github.com/lvgl/lvgl/releases), [Lizenz/Version](https://github.com/lvgl/lvgl/blob/master/library.json) | Nur als Vergleichskandidat nach gleichem Screen, gleicher Hardware, gleichem Treiber und gleicher Messmethode. |
| LovyanGFX, TFT_eSPI, LCDWiki | Die kanonische Auditmatrix bleibt als Alternativmatrix erhalten; alte Versionsangaben werden nicht ungeprueft uebernommen. | Alternative kombinierte Treiber-/Zeichenstacks. In Stage 0–4 werden daraus nur identische rendererunabhaengige Pixel-/Rechteck-/Flaechen-/Raw-Touch-Fixtures bewertet; ihre UI-/Widget-APIs sind kein Auswahlkriterium. |

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
    -> Stufe 4: genau eine Low-Level-Grundlage und hoechstens ein Rueckfall
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

Die bestehende boardseitige Stage-0-Evidence des reviewten Plan-HEADs bleibt
gueltig. UART-, Chip-, Flash-, Boot-/Reset-, Toolchain-, No-PSRAM- und bereits
gemessene Ressourcenwerte werden fuer diese Planrevision nicht erneut
gemessen. Nach Anschluss der Display-/Touchhardware sind nur die fehlenden
Display-/Touch-Identitaets-, SSOT- und Funktionspunkte zu erheben und gegen
diese Baseline zu ergaenzen.

- reale Boardfamilie passend zur Repository-Referenz und das tatsaechliche
  ESP32-Modul/der Chip;
- vorhandene Board-/Modulmarkierungen und Fotos dokumentieren. Gibt es keine
  eindeutige Carrier-Revisionskennung, wird ausschliesslich
  `board_revision=TBD_HARDWARE_NO_IDENTIFIABLE_MARKING` dokumentiert; die
  unbekannte Nummer blockiert #31 nicht dauerhaft;
- praktisch ermittelten Displaycontroller und Touchcontroller; ILI9341 und
  XPT2046 bleiben bis dahin Kandidatennamen, nicht PASS;
- reale TFT-/Touch-CS-, D/C-, Reset-, Backlight-, IRQ-, SPI- und
  Masseverbindungen gegen das Boardprofil;
- keine widerspruechliche revisionsabhaengige Eigenschaft, insbesondere keine
  abweichende Pinbelegung, Versorgung, Resetverschaltung oder Logic-Domain;
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
ist `BLOCKED`, nicht `PASS`. Die unbekannte Carrier-Revisionsnummer allein ist
kein Stage-0-Blocker. Nur wenn daraus eine konkrete relevante Mehrdeutigkeit
entsteht, etwa bei Pinbelegung, Versorgung, Resetnetz oder Logic-Domain,
stoppt #31 und erfordert einen separaten SSOT-/Ownerentscheid. Abweichungen
vom Boardprofil bleiben ein solcher Blocker.

### Stufe 1 – Quelle, Lizenz, Kompatibilitaet und reproduzierbarer Build

Erst nach bestandenem Stufe-0-Evidencepaket und fuer den offiziellen Stack
zuerst auszufuehren:

Das Stage-1-bis-4-Kandidatenset umfasst ausschliesslich Low-Level-Panel-, Bus-
und Raw-Touch-Grundlagen. LVGL und `esp_lvgl_port` bleiben trotz der
vorbereitenden Quellenrecherche bis nach Stage 4 ausserhalb dieser Gates.

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
verbleibenden Low-Level-Kandidaten identisch:

- Kaltstart, Reset und Controllerinitialisierung;
- Pixel-, Rechteck- und Vollflaechenausgabe in 320x240 mit identischer
  Farbfolge Schwarz/Weiss/Rot/Gruen/Blau und vier Ecken;
- falls eine Sichtpruefung Text benoetigt, ausschliesslich derselbe kleine
  Test-Raster-/Bitmap-Fixture fuer alle Kandidaten, keine #26-Widgets,
  Dialoge, Navigation oder produktionsnahe Screenprojektion;
- Rotation und Reset-/Backlight-Zustaende;
- Touch-Initialisierung, Raw-Werte an Ecken und Mitte sowie Kontakt-/Druck-
  verhalten;
- getrennte CS-Zugriffe und alternierender gemeinsamer SPI-Bus;
- IRQ und Polling, mindestens fuenf Bus-/Reset-/Touch-Wiederholzyklen;
- definierte Fehlerreaktion bei Initialisierungs-, Bus- und Touchfehlern;
- kein Full-Framebuffer, keine PSRAM-Annahme, keine produktive Aktion und
  keine Aktorfreigabe.

Stufe 2 ist nur ein kurzer rendererunabhaengiger Smoke. Einzelne sichtbare
Zeichen, ein Lieferantensample oder ein UI-Screen beweisen weder Controller,
Treiberstabilitaet noch Produktreife.

### Stufe 3 – vollstaendige identische Funktions-, Fehler- und Ressourcenmatrix

Nach bestandenem Smoke werden fuer den offiziellen Stack und nur begruendete
Alternativen dieselben rendererunabhaengigen Low-Level-Tests ausgefuehrt:

**Controller, Bus und Roh-I/O**

- 100 wiederholte Pixel-, Rechteck- und Vollflaechenfolgen je Farbe mit dem
  identischen kleinen Raster-/Bitmap-Fixture, ohne #26-UI-Projektion;
- Rotation, Reset und Backlight jeweils einzeln sowie in Wiederholzyklen;
- Raw-Touch an Ecken, Kanten und Mitte inklusive Kontakt-/Druckverlauf;
- IRQ- und Pollingpfad, getrennte CS-Zugriffe und gemeinsamer SPI-Bus;
- 1000 wechselnde Raw-Touch-/Draw-/Buszyklen mit Fehler- und Latenzprotokoll;
- absichtlich eingebrachte Initialisierungs-, Bus-, CS-, Reset- und Touchfehler
  mit definierter, fail-closed Low-Level-Reaktion.

Kalibrierung, `WakeOnly`, Raw-Touch-Recovery, UI-Commands und die vollstaendige
Fehlerisolation gegen die bestehende Fachlogik sind keine Stage-0-bis-4-
Auswahlkriterien. Sie folgen erst nach der Low-Level-Auswahl, der schmalen
Flush-/Input-Grenze und dem identischen repräsentativen #26-Screen in Abschnitt
7/8.

**Ressourcen und Stabilitaet**

Fuer Baseline und jeden verbleibenden Kandidaten mit gleicher Messmethode:

- Flashgesamtbedarf und relevante Partition-/Binarygroesse;
- DRAM, IRAM, freier Heap direkt nach Boot, niedrigster freier Heap und
  groesster freier Heapblock;
- relevante Task-Stacks/HWM inklusive Low-Level-Treiber-/Bus-Task und Hauptloop;
- DMA-/Displaybuffer, Partial-Buffer und Touchpuffer;
- 320x240-Aktualisierungszeit, Fehlerrate und Stabilitaet bei Raw-Touch plus
  Low-Level-Panel-I/O und bestehender Firmware;
- Resets, Watchdog, Busfehler, Double Events, Drift und Speicherfehler;
- explizit keine PSRAM-Abhaengigkeit.

Es werden keine willkuerlichen neuen harten Budgets erfunden. Die Messwerte
werden gegen das Gesamtsystem, die bestehende Reserve und den Owner bewertet;
bis dahin bleibt `TBD_IMPLEMENTATION_BUDGET` ungueltig als Laufzeitwert.

### Stufe 4 – genau eine bevorzugte Low-Level-Grundlage und ein Rueckfall

Erst nach vollstaendiger Stage-3-Matrix, Lizenz-/Herkunftsnachweis und
unabhaengiger Bewertung wird genau eine bevorzugte Richtung und hoechstens der
bereits im Audit vorgesehene Rueckfallkandidat dokumentiert. Kriterien sind
Funktion, Stabilitaet, Ressourcenreserve, Buildreproduzierbarkeit, Lizenz,
Upstreampflege und Adapter-/Wartungsumfang.

Stufe 4 entscheidet ausschliesslich die rendererunabhaengige Display-/Touch-
Low-Level-Grundlage und die dafuer benoetigte schmale neutrale Flush-/Input-
Grenze. LVGL, Lean und der repräsentative #26-Screen nehmen an Stufe 0 bis 4
nicht teil und werden nicht als vorgezogene Frameworkentscheidung behandelt.

Die heutige Evaluationsreihenfolge ist daher:

1. offizieller `esp_lcd`-/`esp_lcd_ili9341`-/`esp_lcd_touch`-Stack mit dem
   verifizierten XPT2046-Kandidaten;
2. LovyanGFX als kombinierte Low-Level-Treiber-/Zeichenbasis;
3. TFT_eSPI als kombinierte Low-Level-Treiber-/Zeichenbasis;
4. LCDWiki-Paket als kombinierte Low-Level-Treiber-/Zeichenbasis;
5. Arduino_GFX oder Adafruit GFX/ILI9341/XPT2046 nur bei den bereits
   definierten Reservebedingungen (zu wenige Smoke-Passer, Lizenz-/Build-
   Blocker oder materialer R1-Vorteil).

Bei den kombinierten Kandidaten werden nur ihre Low-Level-Ausgaben und
Raw-Touchquellen gegen dieselben Fixtures bewertet. Ihre Zeichen-/Widget-
Abstraktion wird weder als #26-Darstellung noch als Lean-/LVGL-Entscheidung
verwendet.

Diese Reihenfolge ist keine Produktivauswahl. Es werden nicht vorsorglich alle
Kandidaten voll integriert.

## 7. Kleine Renderer-/LVGL-Integrationsgrenze

Die Reihenfolge nach der Low-Level-Auswahl ist verbindlich:

```text
ausgewählte Low-Level-Grundlage
    -> schmale Flush-/Input-Grenze
    -> identischer repräsentativer #26-Screen
    -> Lean vs. LVGL
    -> Ownerentscheidung
```

Erst nach der Stage-4-Grundlage und der festgelegten schmalen Flush-/Input-
Grenze wird derselbe repräsentative #26-Screen mit derselben Hardware, dem
ausgewaehlten Low-Level-Stack, denselben DE/EN/ES-Texten, denselben
Eingabeelementen und derselben Messmethode verglichen. Der Vorabstand von
`esp_lvgl_port` und LVGL ist nur Desk Research;
`LVGL_SELECTION=DEFERRED_UNTIL_POST_STAGE4_DRIVER_SELECTION`.

Bei einer `esp_lcd`-/`esp_lcd_touch`-Grundlage ist `esp_lvgl_port` der
bevorzugte LVGL-Integrationsweg. Bei jeder anderen Stage-4-Grundlage wird vor
einer LVGL-Auswahl zuerst geklaert, ob ein kleiner direkter LVGL-Flush-/Input-
Adapter sinnvoll und wartbar moeglich ist. Eine kuenstliche Doppelrenderer-
Schichtung wie `LVGL -> LovyanGFX -> Display` ist unzulaessig; ebenso wird
keine neue allgemeine Rendererplattform eingefuehrt.

Der repräsentative #26-Screen ist der erste renderingnahe Nachweis und bleibt
identisch fuer Lean und LVGL. Erst in diesem Schritt werden View-Modelle,
Text-/Theme-Contracts, Press-Rueckweg, `WakeOnly`, Touchtransformation,
Kalibrierungsworkflow, Raw-Recovery und UI-Fehlerisolation gegen die bestehende
Fachlogik bewertet. Kein Teil dieser Bewertung wird als Low-Level-Stage-0-bis-
4-Kriterium rueckwirkend verwendet.

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

### Ein Owner je Verantwortung

Jede Verantwortung hat genau einen Owner; Adapter, Renderer und Composition
duerfen dieselbe Policy nicht parallel besitzen:

| Verantwortung | Genau ein Owner | Vertrag / Grenze |
|---|---|---|
| Anwendungsneutrales Rohereignis | `device_platform` | `RawTouchSample` mit Roh-X, Roh-Y, Kontakt-/Druckinformation, Controller-/Samplestatus und monotonem Zeitbezug. |
| Kalibrierungsmodell, Transformation und Validierung | `device_platform` | Achstausch, Spiegelung, Rotation oder gemessenes lineares Modell; Plausibilitaet, Versions-/Schemakennung, Board-/Controllerbezug und Invalidierung. Keine Fachaktion und keine Recoveryentscheidung. |
| Technischer Kalibrierungsrecord, Codec und Store | `device_platform` | Eigener Record-Type, eigenes Schema und eigene Keys ueber den bestehenden `IStateStore`; kein zweiter allgemeiner Persistenzkern und kein User-/Service-/Programm-Konfigurationsgraph. |
| Konkrete XPT2046-/Touch-Sampling-Quelle | `device_platform_esp_idf` | ESP-IDF-/Treiberadapter fuer IRQ/Polling und Raw-Sampling; keine Recoverypolicy, keine Kalibrierungsworkflow- und keine `fermentation_app`-Abhaengigkeit. |
| Bedien-/Recoverysemantik | `fermentation_app` | Bestehende UI-/Command-/Recoverysemantik, `>=10 s`-Raw-Touch-Recoveryentscheidung, Service-/Kalibrierungsablauf und bestehende `SafeBoot`-Capability; keine Treiber-/ESP-IDF-Typen. |
| Konstruktion und Verdrahtung | `main` | Nur actor-free Initialisierung, Lebenszyklus, Portverdrahtung und Weitergabe der Ergebnisse; keine eigene Policy oder zweite Recoverykoordination. |

### Daten- und Validierungsmodell

Der unveraenderte Recoveryvertrag lautet: Geraet einschalten, eine actor-free
Raw-Touch-Quelle verfuegbar machen, gespeicherte Kalibrierung laden und
klassifizieren, Raw-Touch mindestens 10 Sekunden halten, Beruehrung ohne
brauchbare Kalibrierung erkennen und ausschliesslich Kalibrierungs-Recovery
starten. Exakte Rohgrenzen, Z-Schwellen, die konkrete Raw-Geste innerhalb
dieses Vertrags, Entprellung/Stabilitaet, Verwechslungs- und Kontaktgrenzen
sowie Transformparameter bleiben bis zur Messung `TBD_HARDWARE`.

Die Transformationsform wird erst anhand realer Daten festgelegt. Erfundene
Rohgrenzen oder scheinbare Lieferantenwerte sind unzulaessig. Eine fehlende,
unbekannte, ungueltige oder nicht zum realen Board-/Controllerbezug passende
Kalibrierung macht normale Touch-Fachaktionen fail-closed unbrauchbar,
beeinflusst aber weder Regelung noch Safety noch Aktorfreigabe.

### Persistenzvertrag und StorageEpoch

Der technische Datensatz wird als eigener `TouchCalibrationRecord` mit eigener
Schema-/Versionskennung und reservierten Keys `touch-calibration-active` und
`touch-calibration-fallback` geplant. Die Namen stehen fuer den dedizierten
Record-/Slotvertrag; sie werden nicht in den normalen User-, Service- oder
Programm-Konfigurationsgraphen aufgenommen. Der Record/Codec verwendet den
bestehenden `IStateStore`-/NVS-/versionierten Envelopepfad. Es entsteht kein
zweiter allgemeiner Persistenzkern und kein kalibrierungsspezifischer
Parallel-Codec ausserhalb dieses bestehenden technischen Pfads.

Die werksresetueberlebende Touchkalibrierung verwendet die Envelope-
Infrastruktur, aber **nicht** die normale Konfigurations-
`StorageEpoch`-Semantik. Der verpflichtende Envelopewert wird in einer
eigenen Kalibrierungsnamespace gegen einen festen
`TOUCH_CALIBRATION_STORAGE_EPOCH=StorageEpoch{1}` geprueft und nie gegen
`RuntimeConfigurationSnapshot::storageEpoch()` oder den Konfigurationsgraphen
verglichen. Die Konfigurations-StorageEpoch darf sich beim normalen Factory
Reset erhoehen; aktive und Fallback-Kalibrierung bleiben unveraendert. Eine
spaetere Aenderung dieses Kalibrierungsformats benoetigt ausdruecklich einen
neuen Record-/Schema-/Namespacevertrag und darf nicht still die
Konfigurationsepoche erben.

Ein unbekanntes neueres Schema, ein ungueltiger Envelope, ein unpassender
Board-/Controllerbezug oder ein unlesbarer Record macht die Kalibrierung sicher
ungueltig und blockiert nur den normalen Touchpfad. Der normale Factory Reset
behaelt die Kalibrierung. Ein separater Touch-Kalibrierungsreset darf nur die
beiden dedizierten Record-Slots invalidieren/loeschen; er veraendert nicht den
Konfigurationsgraphen und ist vom Vollreset getrennt.

### Boot-Recovery-Sequenz ohne zweiten Koordinator

```text
Boot
-> actor-free Raw-Touch-Quelle verfuegbar
-> gespeicherte Kalibrierung laden/klassifizieren
-> >=10-s-Hold-Detektor auf Rohdaten
-> bei erfuelltem Vertrag: RawTouchRecovery
-> gehaltener Kontakt loest keine normale UI-Aktion aus
-> nach erfolgreicher Kalibrierung normaler Touchpfad
```

Der `>=10 s`-Hold-Detektor und die Entscheidung fuer `RawTouchRecovery`
gehoeren ausschliesslich zu `fermentation_app` und nutzen nur
`RawTouchSample`; der Treiber liefert keine Recoveryentscheidung. Sichere
Bootphase, Kontaktstabilitaet, explizite Release-/Abbruchsemantik und keine
spaete Nachausloesung verhindern False Trigger. Die bestehende
`SafeBoot`-Capability bleibt der einzige Recoverykoordinator. Ein gehaltener
Kontakt wird waehrend dieses Ablaufs konsumiert und erreicht weder Workspace,
PIN, Navigation noch Fachaktion.

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
und wird nicht in Stage 0 bis 4 neu bestimmt; nur die konkrete Geste innerhalb
des Vertrags sowie Roh-/Kontakt-/Druckgrenzen, Entprellung und
Verwechslungsschutz bleiben spaetere Hardware-/Integrations-Evidence.

Der erste Touch nach Dimmung/Schlaf ist unabhaengig von Kalibrierung und PIN
immer `WakeOnly`; erst ein spaeteres, neues Touchereignis darf die vorhandene
Fachaktion erreichen.

## 9. Owner-Hardwarecheckliste fuer die spaetere Durchfuehrung

Vor Stage 0 benoetigt der Builder vom Owner Zugang und Testbedingungen, nicht
eine Owner-Bestaetigung physischer Tatsachen:

1. Reale Boardfamilie gegen die Repository-Referenz, tatsaechliches
   ESP32-Modul/Chip sowie vorhandene Fotos/Markierungen dokumentieren. Falls
   keine eindeutige Carrier-Revisionskennung vorhanden ist,
   `board_revision=TBD_HARDWARE_NO_IDENTIFIABLE_MARKING` setzen; dies stoppt
   #31 nur bei einer konkreten revisionsabhaengigen Mehrdeutigkeit. UART/
   FT232RL-Zugang und die Moeglichkeit zur praktischen Display-/Touch-
   Controlleridentifikation muessen vorhanden sein.
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
| 4 | Vollstaendige Stage-3-Matrix nur fuer Low-Level-Funktion, Raw-I/O, Fehler, Ressourcen und Stabilitaet | Identische rendererunabhaengige Evidence; keine LVGL-/Lean-Entscheidung und keine UI-/Recovery-PASS-Aussage. |
| 5 | Stage-4-Auswahl der Low-Level-Grundlage und eines Rueckfallkandidaten; schmale Flush-/Input-Grenze festschreiben | Genau eine bevorzugte Low-Level-Richtung plus hoechstens ein Rueckfall; keine neue Treiberarchitektur. |
| 6 | Ausgewaehlte Low-Level-Grundlage hinter die schmale Flush-/Input-Grenze legen und den identischen repräsentativen #26-Screen vorbereiten | Noch keine Lean-/LVGL-Entscheidung; `fermentation_app` bleibt rendererfrei, keine künstliche Doppelrenderer-Schichtung. |
| 7 | Lean-Projektion gegen LVGL vergleichen; bei `esp_lcd`/`esp_lcd_touch` bevorzugt ueber `esp_lvgl_port`, sonst direkten kleinen LVGL-Adapter zuerst klaeren | Erst jetzt Ownerentscheidung: LVGL nur bei klarem gemessenem R1-Vorteil, sonst `DEFER_AFTER_R1`. |
| 8 | Lokalen `main`-Application-Adapter mit bestehendem #26-Snapshot-/Workspace-/Commandpfad verdrahten und den Architektur-Guard gegen seine normative Rolle pruefen | Keine Fach-/Persistenz-/Recovery-/Safety-Policy oder zweite UI-State-Machine; Guard prueft Dateiklassen, Abhaengigkeiten und Negativfixtures, nicht nur CMake-Allowlisten. |
| 9 | Touchkalibrierungsrecord, StorageEpoch-Namespace, Boot-Recoverysequenz, WakeOnly und Fehlerisolation integrieren | Eigener technischer Record ueber `IStateStore`; normaler Factory Reset behaelt Kalibrierung; kein zweiter Recoverykoordinator; danach gezielte Tests, Builder-Self-Check und Independent Review. |

Die Dateigrenzen sind fuer die Umsetzung bereits festgelegt: neutraler
Hardwareport nur bei bestaetigtem Gap unter
`lib/device_platform/src/device_ui_hardware_ports.hpp`, Low-Level-Adapter unter
`lib/device_platform_esp_idf/src/esp_idf_display_touch_adapter.*`, lokale
app-spezifische Renderer-/Presentation-Helper unter
`main/fermentation_ui_renderer.hpp/.cpp` und reine Konstruktion/Verdrahtung in
`main/app_main.cpp`. Dazu kommen gezielte native/ESP-IDF-Tests sowie die
notwendigen IDF-Komponenten-/Lockdateien.

Der bestehende Guard `scripts/check_architecture_boundaries.py` wird erst nach
der tatsaechlichen Stage-4-/LVGL-Auswahl gegen den finalen Dependencygraphen
geprueft und nur bei einer legitimen neuen direkten CMake-Abhaengigkeit
angepasst. `device_platform_esp_idf` darf dann nur die tatsaechlich
ausgewaehlten `esp_lcd`-/Touch-Komponenten erhalten; `main` darf nur bei einer
ausgewaehlten LVGL-Richtung deren konkret benoetigte Abhaengigkeit erhalten.
Zusatzlich prueft der Guard gezielt: `main/app_main.cpp` ist nur Composition,
Lebenszyklus und Verdrahtung; der `main`-Application-Adapter darf App-
View-Modelle und den ausgewaehlten Renderer kennen, aber keine Fach-,
Persistenz-, Recovery-/Safety-Policy oder zweite UI-State-Machine besitzen;
`device_platform_esp_idf` bleibt app-frei; `fermentation_app` bleibt frei von
ESP-IDF/LVGL/konkreten Treibern. Die jeweiligen CMake-Allowlisten,
Dateiklassenregeln sowie positive und negative Guard-Selbsttests und der
Regressionnachweis werden im selben Umsetzungsschnitt aktualisiert. Vor Stage
4 werden keine Komponenten oder Dependencies allowgelistet. Keine dieser
Produktionsdateien oder Guard-Aenderungen existiert nach diesem Plan-Commit;
ihre spaetere Erstellung ist Implementation und bleibt bis zur Ownerfreigabe
verboten. `device_platform` und `fermentation_app` werden nur bei dem jeweils
nachgewiesenen neutralen bzw. app-eigenen Gap additiv angepasst, nicht um eine
zweite UI-Architektur zu schaffen.

## 11. Tests, Nachweise und Governance-Gates

### In dieser Planphase

Zulaessig sind nur Dokument-/Routingpruefungen, etwa Branch-/HEAD-/Issue-/PR-
Abgleich, `git diff --check` und die Pruefung der Plan-/Roadmapreferenzen.
Firmwarebuilds, native Volltests, ESP-IDF-Builds und reale Hardwaretests sind
in dieser Phase `NOT_RUN` und werden nicht als bestanden behauptet.

### Nach Ownerfreigabe der exakten Plan-SHA

- Stage-1-Kandidatenbuilds; nach Low-Level-Auswahl gezielte native
  Contract-/Kalibrierungstests;
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
   Grundlage und hoechstens einen Rueckfallkandidaten bestimmen.
3. **LVGL oder schlanke Projektion:** erst nach Stage 4 und dem identischen
   Screen-/Ressourcenvergleich entscheiden; LVGL nur bei klarem gemessenem
   R1-Vorteil, sonst `DEFER_AFTER_R1`.
4. **Widerspruch zum Boardprofil:** bei materieller Abweichung vor Umsetzung
   einen separaten SSOT-/Ownerentscheid einholen; die physische Tatsache selbst
   bleibt Evidence und wird nicht durch Ownerentscheidung bestaetigt.

Die konkrete Hardwareidentitaet, Boardrevision, Controller, Verdrahtung,
Rotation, Raw-Grenzen, Kontakt-/Druckwerte, Entprellung und
Verwechslungsschutz sind keine Ownerentscheidungen, sondern Stage-0-/Stage-2-/
Stage-3-Evidence. Eine fehlende identifizierbare Carrier-Revisionsmarkierung
wird als `TBD_HARDWARE_NO_IDENTIFIABLE_MARKING` dokumentiert und blockiert nur
bei einer daraus entstehenden konkreten Mehrdeutigkeit. Der `>=10 s`-Raw-Touch-Recoveryvertrag ist bereits
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
- die normative ADR-013-Rolle: `main/app_main.cpp` nur Composition,
  Lebenszyklus und Verdrahtung; kleiner `main`-Application-Adapter darf
  View-Modelle und den ausgewaehlten Renderer kennen, aber keine Fach-,
  Persistenz-, Recovery-/Safety-Policy oder zweite UI-State-Machine;
- konkrete rendererunabhaengige Stage-0-bis-4-Matrix mit actor-free
  Hardwarebedingungen;
- ausgewählte Low-Level-Grundlage, schmale Flush-/Input-Grenze und danach ein
  identischer repräsentativer #26-Screen fuer Lean-vs.-LVGL;
- Display, Touch, Rotation, Backlight, WakeOnly, Raw-Recovery,
  PIN-unabhaengige Kalibrierung mit `>=10 s`-Recovery und Fehlerisolation;
- keine feste Fuenf-Punkt-Produktivkalibrierung; Mess-/Validierungspunkte sind
  von Produktivpunkten getrennt;
- `esp_lvgl_port` als bevorzugter LVGL-Weg bei `esp_lcd`/`esp_lcd_touch`, sonst
  vorherige Pruefung eines kleinen direkten LVGL-Flush-/Input-Adapters; keine
  Doppelrenderer-Schichtung und keine allgemeine Rendererplattform;
- eigener TouchCalibrationRecord mit eigenem Schema/Keys ueber `IStateStore`,
  ausserhalb des normalen Konfigurationsgraphen, unbekanntes neueres Schema
  fail-closed, normaler Factory Reset behaelt die Kalibrierung;
- expliziter unabhaengiger Kalibrierungs-StorageEpoch-Namespace statt
  impliziter Bindung an die normale Konfigurations-`StorageEpoch`;
- eindeutige Ownership von RawSample/Transformation/Record, ESP-IDF-Sampling,
  App-Recovery/Service und `main`-Verdrahtung ohne zweiten Koordinator;
- gleiche Ressourcen-/Stabilitaetsmessung ohne PSRAM und ohne willkuerliche
  harte Budgets;
- genau eine spaetere Auswahl plus hoechstens ein Rueckfall, keine
  vorgezogene Hardware- oder Produktiv-PASS-Aussage.
