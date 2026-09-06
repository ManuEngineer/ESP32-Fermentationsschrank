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
| LVGL-Auswahl | `EVALUATE_LATER` |
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
| `device_platform` | Bleibt bei anwendungsneutralen, schmalen Ports und Diensten. Neue Typen sind nur zulaessig, wenn ein konkreter neutraler Contract-Gap nachgewiesen ist; keine LVGL-, Display-, Touch-, GPIO- oder ESP-IDF-Abhaengigkeit. |
| `device_platform_esp_idf` | Konkrete ESP-IDF-Adapter fuer SPI-Panel, Touch-Sampling, Backlight und ggf. ESP-IDF-Komponenten. Keine Fachlogik, keine Navigation, keine App- oder Test-Support-Abhaengigkeit. |
| Composition-Grenze (`main` bzw. vorhandener Root) | Ein schmaler Binder verbindet vorhandene App-Snapshots/Workspace-Views mit dem konkret gewaehlten Renderer und mapped Press-Ergebnisse zurueck auf vorhandene Commands. Keine neue allgemeine Provider- oder Plugin-Schicht. |
| Test-Support | Renderer- und Adaptertests bleiben von Produktions-App-Abhaengigkeiten getrennt. Hardware-Smoke- und Ressourcennachweise laufen als actor-free, reproduzierbare Profile. |

Der Binder darf die #26-Projektion lesen und `press(...)` mit einem
vorhandenen `FermentationUiInteraction`-/Commandpfad aufrufen. Er darf keine
zweite Route, Aktion, PIN-Pruefung, Recovery-Policy oder Fachzustandskopie
besitzen. Bei fehlendem Display oder Touch wird die UI-Faehigkeit
degradiert; Regelung, Safety und Aktorfreigabe laufen unabhaengig und
fail-closed weiter.

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
unveraendert. Jede Stufe erzeugt ein reproduzierbares Evidence-Artefakt mit
`PASS`, `FAIL`, `BLOCKED` oder `NOT_RUN`; fehlende Messungen sind nicht
bestanden.

### Stufe 0 – reale Hardware identifizieren

Vor jedem aktiven Hardwaretest legt der Owner die konkrete Modul- und
Verdrahtungsidentitaet fest:

- Boardrevision und exakte MSP2807-/Displaymarkierung, Fotos und
  Liefer-/Bestellinformationen;
- praktisch ermittelter Displaycontroller und Touchcontroller; ILI9341 und
  XPT2046 bleiben bis dahin Kandidatennamen, nicht PASS;
- reale TFT-/Touch-CS-, D/C-, Reset-, Backlight-, IRQ-, SPI- und
  Masseverbindungen gegen das Boardprofil;
- Versorgung und Logikkompatibilitaet als konkrete Modulfrage, kein
  generisches neues Pegelmessgate;
- Resetnetz `EN_CHIP_PU -> MSP2807_RESET`, Boot-/Reset-Safe-Zustaende,
  UART/FT232-Recoverypfad und Trennung aller Aktoren.

Abweichungen vom Boardprofil stoppen #31. Ein Lieferantentext ohne
praktische Identifikation ist `BLOCKED`, nicht `PASS`.

### Stufe 1 – Quelle, Lizenz, Kompatibilitaet und reproduzierbarer Build

Vor Hardware moeglich und zuerst fuer den offiziellen Stack auszufuehren:

1. Registry-/Upstreamquelle, exakte Version und aufgeloesten Commit fuer jede
   direkte Komponente erfassen; keine schwebenden Git-Referenzen.
2. SPDX-/Lizenznachweis und Lizenz-/Notice-Dateien fuer direkte und relevante
   transitive Abhaengigkeiten erfassen; Fonts, Assets und generierte Dateien
   einschliessen.
3. ESP-IDF 6.0.2, ESP32-32E, C++17, 4 MB Flash und **kein PSRAM** in den
   Kandidatenprofilen reproduzierbar bauen; Build-Warnungen und Konfiguration
   festhalten.
4. Baseline gegen Kandidat mit denselben Buildflags, Boardprofilen und
   Evidence-Skripten messen. Der Lock-/Manifeststand wird versioniert, sobald
   eine Komponente fuer Umsetzung angenommen wird.
5. `esp_bsp_generic` nur auf konkrete Mehrwerte und zusaetzliche
   Abhaengigkeiten pruefen; kein BSP-Scaffold nur fuer Bequemlichkeit.

Diese Stufe benoetigt keine echte Displayfunktion. Sie darf als
quell-/buildseitiger Vorlauf vorbereitet werden, erzeugt aber keinen
Hardware-PASS.

### Stufe 2 – kurzer identischer actor-free Hardware-Smoke

Erst nach Stufe 0 und mit allen Aktoren getrennt/inaktiv, fuer jeden ernsthaft
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
- Raw-Touch an Ecken, Kanten und Mitte inklusive Kontakt-/Druckverlauf;
- fuenf-Punkt-Kalibrierung, Neustart und Wiederholungsmessung;
- korrekte Rotation und Touch-Transformation gemeinsam pruefen;
- 1000 wechselnde Touch-/Draw-/Statuszyklen mit Fehler- und Latenzprotokoll;
- gemeinsamer SPI-Bus abwechselnd fuer Display und Touch.

**Wake, Recovery und Fehlerisolation**

- erster Touch nach Dimmung/Schlaf ist immer `WakeOnly` und loest keine
  Fachaktion, Navigation oder PIN-/Recoveryaktion aus;
- Start im fruehen Bootfenster mit fehlender oder ungueltiger Kalibrierung:
  Raw-Touch-Recovery ist PIN-unabhaengig erreichbar, hat False-Trigger-
  Schutz, eine definierte Release-/Abbruchbedingung und keine spaete
  Nachausloesung;
- fehlender, unlesbarer oder fehlerhafter Touch darf Regelung, Safety und
  Aktorfreigabe nicht blockieren und nicht freigeben;
- Display-/Backlight-/SPI-/Touchfehler werden isoliert, geloggt und fuehren
  zu einem sicheren UI-/Recovery-Fallback; UART bleibt der moegliche
  Recoverypfad;
- Resettests im Idle, bei Darstellung und bei Touch, inklusive Watchdog-/Boot-
  Verhalten und actor-free Wiederanlauf;
- autorisierter Factory Reset behaelt die geraetespezifische Touchkalibrierung
  gemaess ADR-010; ein Touch-Kalibrierungsreset bleibt davon getrennt.

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

### Stufe 4 – genau eine bevorzugte Produktivrichtung und ein Rueckfall

Erst nach vollstaendiger Stage-3-Matrix, Lizenz-/Herkunftsnachweis und
unabhaengiger Bewertung wird genau eine bevorzugte Richtung und hoechstens der
bereits im Audit vorgesehene Rueckfallkandidat dokumentiert. Kriterien sind
Funktion, Stabilitaet, Ressourcenreserve, Buildreproduzierbarkeit, Lizenz,
Upstreampflege und Adapter-/Wartungsumfang.

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

Die Entscheidung zwischen schlanker projektspezifischer Projektion und LVGL
faellt erst nach Stufe 2 und einem identischen repraesentativen #26-Screen.
Beide Varianten muessen denselben Display-/Touchtreiber, dieselben DE/EN/ES-
Texte, dieselben Eingaben und dieselbe Messmethode verwenden.

Wenn LVGL den Vergleich gewinnt, bleibt die Integration klein:

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

Wenn die schlanke Projektion gewinnt, wird kein allgemeiner Rendererrahmen
gebaut: Es gibt nur eine konkrete, lokal gebundene Darstellung fuer die
vorhandene Workspace-View mit demselben typed Event-Rueckweg.

In beiden Varianten gilt:

- `fermentation_app` sieht keine LVGL-, ESP-IDF-, Display- oder Touchtypen;
- der konkrete Adapter lebt in `device_platform_esp_idf`; die Komposition
  wird am bereits vorhandenen Root verdrahtet;
- `main/app_main` bleibt die Lebenszyklus-/Composition-Grenze und erzeugt
  keine neue Fachzustandsmaschine;
- eine UI-Stoerung setzt nur die UI-/Input-Faehigkeit herab. Die
  Regelungs-/Safety-Schleife und Aktorfreigabe werden weder auf UI-Callbacks
  angewiesen noch durch UI-Fehler freigegeben;
- Assets und Fonts bleiben auf den benoetigten DE/EN/ES-Umfang begrenzt; ihre
  Groesse, Lizenz und Herkunft gehen in die Ressourcenmatrix ein.

## 8. Touch-Rohdaten, Kalibrierung und Recovery

### Daten- und Validierungsmodell

Der Adapter liefert ein neutrales Rohereignis mit mindestens Roh-X, Roh-Y,
Kontakt-/Druckinformation, Controller-/Samplestatus und monotonem
Zeitbezug. Exakte Rohgrenzen, Z-Schwellen, Gesten und Transformparameter
bleiben bis zur Messung `TBD_HARDWARE`.

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

### Bedienung und Raw-Touch-Recovery

- Der normale Kalibrierungsablauf ist ueber den bestehenden Service-/Workspace-
  Vertrag erreichbar, benoetigt die vorhandene Bestaetigungs-/Sessionlogik
  und erzeugt nur nach erfolgreicher Validierung einen persistierbaren Satz.
- Bei fehlender/ungueltiger Kalibrierung ist ein fruehes, lokales
  Raw-Touch-Recoveryfenster vorgesehen. Es ist PIN-unabhaengig erreichbar,
  benoetigt keine brauchbare Normaltransformation und darf nur eine
  klar begrenzte Recovery-/Kalibrierungsaktion ausloesen.
- False Trigger im ersten Bootfenster werden durch sichere Bootphase,
  Kontaktstabilitaet, explizite Release-/Abbruchsemantik und die spaeter
  gemessenen Roh-/Zeitgrenzen verhindert. Ein gehaltenes Touchsignal erzeugt
  keine wiederholte spaete Aktion.
- Fuenf-Punkt-Ablauf, Raw-Grenzen, Druckschwelle, Gesten und Timeout werden
  erst in Stage 3 aus realen Messungen abgeleitet. Vorher bleiben sie
  `TBD_HARDWARE`, nicht Defaultwerte.
- Der erste Touch nach Dimmung/Schlaf ist unabhaengig von Kalibrierung und
  PIN immer `WakeOnly`; erst ein spaeteres, neues Touchereignis darf die
  vorhandene Fachaktion erreichen.

## 9. Owner-Hardwarecheckliste fuer die spaetere Durchfuehrung

Vor Stage 2 benoetigt der Builder vom Owner:

1. Exakte Fotos/Markierungen, Modulvariante, Boardrevision und praktisch
   bestaetigten Display- sowie Touchcontroller.
2. Reale Verdrahtung von SCK/MISO/MOSI, TFT-CS, D/C, Reset, Backlight,
   Touch-CS, IRQ und GND gegen das Boardprofil; Abweichungen zuerst als
   Ownerentscheidung behandeln.
3. Bestaetigung der kompatiblen Versorgung-/Logikdomain sowie des
   `EN_CHIP_PU -> MSP2807_RESET`-Netzes, soweit dies fuer den konkreten offenen
   Hardwarepunkt erforderlich ist. Kein pauschales Spannungs-/GPIO-Gate.
4. Aktorfreie Testfreigabe: Peltier, BTS7960, Innen-/Aussenluefter,
   MOSFET-Verbraucher und Summer physisch getrennt oder nachweislich inaktiv;
   kein Test darf eine produktive Aktorfreigabe herstellen.
5. Verfuegbarer UART/FT232-Recoverypfad, reproduzierbarer Boot-/Resetablauf,
   reale Flashgroesse und die Moeglichkeit, Logs sowie Reset-/Watchdogdaten
   mitzuschneiden.
6. Freigabe fuer die identische Stage-2-/Stage-3-Matrix und die
   dazugehoerigen actor-free Wiederholungen. Fehlt ein Punkt, wird nur der
   betroffene Nachweis `BLOCKED`/`NOT_RUN`.

## 10. Spaetere Umsetzungsschnitte nach Planfreigabe

Jeder Schnitt bleibt klein, wird gezielt verifiziert und darf den freigegebenen
Plan nicht materiell ueberschreiten:

| Schnitt | Inhalt | Ergebnis / Grenze |
|---:|---|---|
| 1 | Live-Rebaseline von Quellen, Versionen, Lizenzen und Kandidaten; Stage-0-Aufnahme | Keine Produktivauswahl; Audit-/Komponentenregister nur mit belegten Daten aktualisieren. |
| 2 | Gepinnte Stage-1-Buildprofile und neutrale Adapter-/Composition-Skizze | Reproduzierbarer ESP-IDF-6.0.2-Build, keine App-/UI-Duplikation, keine Aktoren. |
| 3 | Identischer Stage-2-Smoke fuer den offiziellen Stack und begruendete Alternativen | Nur Hardware-/Treiberpass; keine produktive Navigation oder Auswahl. |
| 4 | Eine konkrete Low-Level-Adapterintegration und repraesentativer #26-Screen | `device_platform_esp_idf`/Root-Grenze; `fermentation_app` bleibt frameworkfrei. |
| 5 | LVGL-vs.-schlanke-Projektion mit identischem Screen und identischer Messung | Ownerentscheidung fuer genau eine Rendererichtung; kein Vorratsframework. |
| 6 | Workspace-/Command-Rueckweg, Backlight/WakeOnly und Kalibrierungs-/Persistenzpfad | Bestehende #25/#26-/Recoveryvertraege konsumieren; neue parallele Logik verboten. |
| 7 | Vollstaendige Stage-3-Matrix inklusive Fehler, Raw-Recovery, Ressourcen und Lizenz | Evidence-Matrix, keine Hardware-PASS-Aussage ausserhalb realer Nachweise. |
| 8 | Stage-4-Auswahl, Dokumentation, Builder-Self-Check und unabhaengiger Review | Genau eine bevorzugte Richtung plus hoechstens ein Rueckfall; danach Owner-Gates. |

Geplante Dateien werden erst nach Planfreigabe und gegen den dann live
verifizierten Schnitt festgelegt. Voraussichtliche Grenzen sind ein konkreter
Adapter in `lib/device_platform_esp_idf`, der vorhandene Root in `main`,
gezielte Tests sowie notwendige IDF-Komponenten-/Lockdateien. Aenderungen an
`device_platform` oder `fermentation_app` sind nur additive, neutrale
Contract-Gaps und nicht automatisch Teil des Scopes.

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

1. **Hardwareidentitaet:** reale Modulvariante, Boardrevision, Display-/
   Touchcontroller und Verdrahtung gegen die R1-SSOT liefern bzw. bestaetigen.
2. **Stage-4-Auswahl:** nach den identischen Nachweisen genau eine bevorzugte
   Treiber-/Rendererichtung und hoechstens ein Rueckfallkandidat bestimmen.
3. **LVGL oder schlanke Projektion:** erst nach dem identischen Screen-/
   Ressourcenvergleich entscheiden; LVGL ist aktuell nicht angenommen.
4. **Hardwareabhaengige Parameter:** Rotation, Raw-Grenzen, Druckschwelle,
   Transformationsform, Touch-Timeouts und Recovery-Gesten erst nach realer
   Messung festlegen.
5. **Widerspruch zum Boardprofil:** bei jeder materiellen Abweichung vor
   Umsetzung einen separaten SSOT-/Ownerentscheid einholen.

Es gibt aktuell keinen nachgewiesenen fundamentalen ESP-IDF-Blocker. Sollte
Stage 1 einen solchen zeigen, wird ein Frameworkwechsel als separate
Ownerentscheidung behandelt und nicht in #31 implementiert.

### Plan-Abnahmekriterien

Der Plan gilt als vollstaendig, wenn die exakte Ownerfreigabe vorliegt und
folgende spaetere Evidence ohne unbelegte Vorannahmen abbildbar ist:

- aktuelle Espressif-/LVGL-Quelle, Version, Lizenz und Abhaengigkeiten;
- offizielle Stackpruefung vor eigener Entwicklung;
- unveraenderte #25/#26-Contracts und schmale Modulgrenzen;
- konkrete Stage-0-bis-4-Matrix mit actor-free Hardwarebedingungen;
- Display, Touch, Rotation, Backlight, WakeOnly, Raw-Recovery,
  PIN-unabhaengige Kalibrierung und Fehlerisolation;
- Persistenz ueber den bestehenden Pfad und Factory-Reset-Erhalt gemaess
  ADR-010;
- gleiche Ressourcen-/Stabilitaetsmessung ohne PSRAM und ohne willkuerliche
  harte Budgets;
- genau eine spaetere Auswahl plus hoechstens ein Rueckfall, keine
  vorgezogene Hardware- oder Produktiv-PASS-Aussage.
