# Issue #31 – Korrekturplan: Displayrotation, Geometrie und Touch-Capture

## Planvertrag

| Feld | Wert |
|---|---|
| Issue | #31 – Renderer, Display, Touch und Kalibrierung |
| PR | #156, bestehender Draft-PR |
| Branch | `agent/issue-31-renderer-display-touch-plan` |
| Korrekturtyp | Materielle Planrevision nach realem Hardwarebefund |
| Ausgangs-HEAD | `9ed1dd6b729b95e92c62a36ff59dc5867dcd68da` |
| Vorgänger-Korrekturplan-HEAD | `c85aa595c43368f0faac5189fc0c12239aeacbeb` |
| Bisher freigegebener Plan | `63fd88372b883887668566047c8a6acac48addcd` |
| Neue Planfreigabe | `OWNER_APPROVAL_REQUIRED` |
| Erster Capture | `COMPLETE; RAW_TOUCH_VALID; POSITION_FIT_REJECTED` |
| Aktorfreigabe | `NO` |
| Produktive Kalibrierung | `NO` |
| Implementation | `NOT_STARTED_FOR_THIS_CORRECTION` |

Dieser Plan ergänzt den bisher freigegebenen #31-Plan ausschließlich um die
Korrektur des Display-Geometrievertrags und die danach erforderliche erneute
Hardwareerfassung. Der bisherige Plan-Commit bleibt als historische
Provenienz unverändert. Nach Commit dieses Dokuments wird angehalten; eine
Umsetzung beginnt erst nach Ownerfreigabe der exakten neuen Plan-SHA.

## 1. Befund und Evidence-Grenze

Der reale Capture auf dem Ausgangs-HEAD war technisch vollständig, lief aber
mit `Rotate0`. Damit wurden logische 320x240-Rechtecke gegen die native
240x320-Panelgeometrie ausgegeben. Der Ownerbefund bestätigt Clipping und eine
falsche sichtbare Orientierung.

Die bestehende Capture-Evidence wird nicht gelöscht und nicht umgeschrieben:

```text
CAPTURE_2026_09_24_RAW_TOUCH_EVIDENCE=VALID
CAPTURE_2026_09_24_POSITION_FIT_EVIDENCE=INVALID_DISPLAY_GEOMETRY
CAPTURE_2026_09_24_PRODUCTIZED=NO
```

Sie bleibt gültig für XPT2046-Kontakt/No-Contact, PENIRQ, Rohdatenverfügbarkeit,
Strength/Z-Verteilung, Kontaktstabilität, Releaseverhalten und
ControllerErrors. Sie wird nicht für affine Koeffizienten oder unabhängige
Positionsvalidation verwendet.

Das unveränderte Raw-Artefakt ist bereits dauerhaft unter einem nicht
ignorierten Textpfad gesichert; es hängt nicht von einem später noch
vorhandenen `/tmp`-Pfad ab:

```text
TARGET=docs/audits/ISSUE_31_TOUCH_CAPTURE_20260924_INVALID_DISPLAY_GEOMETRY_RAW.txt
TARGET_SHA256=e19b5caf3d6f19614a12b878ce8d973a0a3a2e0a263737b11c406f7ce9c20f08
```

Der bisherige Harness bleibt als Roh-Touch-Provenienz erhalten; kein
historischer Log wird durch eine Summary ersetzt.

## 2. Unveränderliche fachliche Grenzen

Der kanonische R1-Vertrag bleibt:

```text
DISPLAY_ORIENTATION=LANDSCAPE
LOGICAL_WIDTH=320
LOGICAL_HEIGHT=240
```

Es werden keine neuen Displayabstraktionen, Rendererframeworks,
Board-SSOT-Schichten, Touchkalibrierungsarchitekturen oder UI-
Koordinatenkompensationen eingeführt. LVGL bleibt logisch 320x240; Header,
Content, Bottom-Navigation und Vollbreite bleiben unverändert bei
`0..31`, `32..199`, `200..239` und `0..319`.

`CONFIG_XPT2046_INTERRUPT_MODE=y` und
`CONFIG_XPT2046_Z_THRESHOLD=1` bleiben ausschließlich im Harness wirksam.
Normaler Bring-up und Release bleiben bezüglich des Capture-Overlays
unverändert. Es gibt weiterhin keine NVS-Kalibrierung, keine produktiven
Koeffizienten, keinen produktiven Strength/Z-Threshold und keine
Aktorfreigabe.

## 3. Geplante kleinste Implementierung

### 3.1 Eine Rotationsquelle

Die Umsetzung benennt genau einen gemeinsamen R1-Wert
`kR1DisplayRotation` an der kleinsten bereits vorhandenen, von Product und
Harness erreichbaren R1-Konfigurationsstelle. Die konkrete Ablage wird bei
der Umsetzung anhand der bestehenden Composition-/Konfigurationsgrenzen
gewählt; es entsteht dafür keine neue Display- oder Board-Konfigurations-
architektur. Product-LVGL und Capture-Harness konsumieren diesen einen Wert;
kein Consumer setzt einen eigenen `Rotate*`-Wert.

Die bestehende `EspIdfDisplayTouchConfig` erhält, falls sie die gewählte
bestehende Ablage benötigt, genau ein
`device_platform::DisplayRotation`-Feld. Die bestehende Port-Enumeration und
Adapterlogik werden wiederverwendet.

- `kR1DisplayRotation` wird genau einmal nach der realen Probe auf den
  akzeptierten Landscape-Kandidaten gesetzt und von beiden Pfaden gelesen.
- `EspIdfDisplayTouchAdapter::initialize()` wendet die Konfiguration genau
  einmal nach Panel-Init an.
- Harte `setRotation(Rotate0)`-Aufrufe aus Product und Harness entfallen.
- LVGL erhält keine zusätzliche Rotation.
- `Rotate0` und `Rotate180` werden für den R1-320x240-Vertrag nicht als
  Landscape-Kandidaten verwendet.

Der Wert von `kR1DisplayRotation` wird nicht theoretisch vorentschieden. Die
zwei zulässigen Kandidaten werden ausschließlich real unterschieden:

```text
Rotate90  -> swap_xy=true, mirror_x=true,  mirror_y=false
Rotate270 -> swap_xy=true, mirror_x=false, mirror_y=true
```

Die erste Probe verwendet einen Kandidaten. Nur wenn die reale Darstellung
zwar vollständig landscape, aber um 180 Grad verkehrt ist, wird der andere
Kandidat ausgewählt. Es gibt keinen theoretischen Ownerentscheid zwischen
90 und 270.

### 3.2 Actor-free Geometrieprobe

Der Harness verwendet die vorhandenen `fillRect`-Primitive und zeichnet vor
der Touchmessung:

- schwarzen Hintergrund über `320x240`,
- einen sichtbaren Rahmen nahe allen vier logischen Rändern,
- eindeutig unterscheidbare Marker für TL, TR, BL, BR und CENTER.

Der Harness loggt einen eindeutigen Probe-Ready-Marker mit logischer
Geometrie und angewandter Rotation. Die Probe aktiviert keine Aktoren und
führt keine Touchkalibrierung aus. Die weitere Messung beginnt erst nach
Owner-Sichtbestätigung der Probe. Bei falscher Orientierung wird der Lauf
beendet, der zweite Kandidat gesetzt und ein neuer Build-/Probe-Lauf
durchgeführt.

Die Owner-Sichtprüfung muss mindestens belegen:

```text
DISPLAY_LANDSCAPE_320X240=PASS
DISPLAY_FULL_WIDTH=PASS
DISPLAY_FULL_HEIGHT=PASS
DISPLAY_CLIPPING=NONE
DISPLAY_TL=PASS
DISPLAY_TR=PASS
DISPLAY_BL=PASS
DISPLAY_BR=PASS
DISPLAY_CENTER=PASS
R1_DISPLAY_ROTATION=ROTATE90|ROTATE270
```

Erst mit dieser Evidence wird die Rotation für den neuen Touch-Capture
akzeptiert.

### 3.3 Produktrenderer und Capture

Nach der realen Kandidatenauswahl wird derselbe Rotationswert im produktiven
LVGL-Pfad und im Capture-Harness gebaut. Die Hardwareadapter-Geometrie wird
verifiziert als:

```text
LVGL_HRES=320
LVGL_VRES=240
PANEL_EFFECTIVE_GEOMETRY=320x240
DOUBLE_ROTATION=NO
```

Auf der bestätigten Geometrie wird der Capture mit unveränderten Punkten in
dieser Reihenfolge wiederholt:

```text
FIT_TOP_LEFT       (32,32)
FIT_TOP_RIGHT      (287,32)
FIT_BOTTOM_LEFT    (32,207)
FIT_BOTTOM_RIGHT   (287,207)
VALIDATION_CENTER  (160,120)
VALIDATION_TOP_MID (160,48)
HOLD_PROBE         (160,120)
```

Die vier FIT-Punkte sind die einzigen späteren Fit-Kandidaten. Center und
Top-Mid bleiben unabhängige Validation. Der bestehende PENIRQ-Idle-Gate-,
15-Sample-/20-ms-Kontakt-, 5-Sample-Release- und fail-closed
ControllerError-Vertrag bleibt unverändert.

## 4. Betroffene Dateien und Ownership

Die Umsetzung bleibt auf die vorhandenen Grenzen beschränkt:

| Bereich | Geplante Änderung |
|---|---|
| `lib/device_platform_esp_idf/src/esp_idf_display_touch_adapter.hpp` | Rotationsfeld in der bestehenden konkreten Adapterkonfiguration |
| `lib/device_platform_esp_idf/src/esp_idf_display_touch_adapter.cpp` | Konfigurationsrotation genau einmal nach Panel-Init anwenden |
| `main/fermentation_ui_lvgl_renderer.cpp` | produktiven `Rotate0`-Aufruf entfernen; bestehende Config verwenden |
| `main/app_main.cpp` | bestehende R1-Produktkonfiguration mit derselben Rotation versorgen |
| `main/issue_31_touch_calibration_harness.cpp` | dieselbe Rotation und actor-free Geometrieprobe; keine Fit-/NVS-Logik |
| `docs/audits/` | neuer Rohlog und SHA nach Hardwarelauf; bestehendes `.txt`-Artefakt bleibt unverändert |

Keine neue öffentliche Portfunktion und keine Änderung an
`fermentation_app`.

## 5. Korrektur-spezifische Reihenfolge

Die bestehenden direkt betroffenen Test-/Buildverträge werden nach der
Umsetzung gemäß dem Repository-CI-Vertrag ausgeführt und separat berichtet;
dieser Plan wiederholt die allgemeinen Gates nicht. Die Hardwarereihenfolge
ist verbindlich:

```text
exact HEAD / binary provenance
-> actor-free rotation probe, candidate 1
-> Owner visual geometry evidence
-> candidate 2 only if required
-> exact selected rotation re-build/provenance
-> Owner touch-capture FIT_4 + VALIDATION_2 + HOLD_PROBE
-> raw-log preservation and SHA256
-> stop for independent capture review
```

Ein abweichender HEAD, fehlende Owner-Sichtbestätigung, Clipping, falsche
Orientierung, unerwarteter Contact-Begin, ControllerError oder fehlende
Rohdaten stoppt den Lauf fail-closed. Thresholds und Parameter werden nicht
spontan geändert.

## 6. Abschluss- und Stop-Vertrag

Nach erfolgreichem neuen Capture werden mindestens folgende Zustände belegt:

```text
DISPLAY_GEOMETRY_BLOCKER=CLOSED
R1_DISPLAY_ROTATION=ROTATE90|ROTATE270
DISPLAY_LANDSCAPE_320X240=PASS
DISPLAY_CLIPPING=NONE
OLD_CAPTURE_RAW_TOUCH=RETAINED
OLD_CAPTURE_POSITION_FIT=REJECTED
NEW_OWNER_UART_CAPTURE=COMPLETE
NEW_CAPTURE_GEOMETRY_VALID=YES
CALIBRATION_FIT_NOT_YET_PRODUCTIZED=YES
ACTUATOR_RELEASE=NO
NEXT_STEP=INDEPENDENT_CAPTURE_REVIEW_AND_PARAMETER_DERIVATION
```

Danach werden keine Koeffizienten, NVS-Datensätze, produktiven Strength/Z-
Werte oder Aktorfreigaben vorgenommen.
Die unabhängige Auswertung erhält alle sieben Summaryzeilen, den vollständigen
Rohlog mit Samples, Idle-Baseline, ControllerErrors, Retries und Complete-
Marker sowie die Owner-Geometrieevidence.

## 7. Freigabepunkt

Dieses Dokument ist der versionierte Korrekturplan. Nach dem Plan-Commit ist
der exakte Commit dem Owner zur Freigabe vorzulegen. Bis dahin gilt:

```text
PLAN_STATUS=OWNER_APPROVAL_REQUIRED
IMPLEMENTATION=NOT_STARTED_FOR_THIS_CORRECTION
HARDWARE_ROTATION_PROBE=NOT_RUN
NEW_OWNER_UART_CAPTURE=NOT_RUN
ACTUATOR_RELEASE=NO
```

## 8. Plan-Ergänzung: Kalibrierungs-Ownership und finaler R1-Paneltransform

Diese kurze Revision ergänzt den freigegebenen Korrekturplan ausschließlich um
die zwei nachgelagerten Review-Blocker. Sie ändert weder die bereits bestätigte
Display-/Layout-/Branding-Evidence noch die Aktorgrenze. Die widersprechende
theoretische R1-Abbildung aus Abschnitt 3.1 wird für die konkrete R1-Hardware
durch den folgenden gemessenen Befund ersetzt.

```text
REVISION_BASE_HEAD=19c6779564f1a49b3b71ab11bbb121b9b372ea11
PREVIOUS_APPROVED_CORRECTION_PLAN_SHA=ff79695e39c9af933b2788a0d8777aa663a0347e
PLAN_STATUS=OWNER_APPROVAL_REQUIRED
IMPLEMENTATION=NOT_STARTED_FOR_THIS_REVISION
FINAL_R1_PANEL_TRANSFORM=swap_xy:true,mirror_x:true,mirror_y:true
R1_DISPLAY_ROTATION=ROTATE90
DISPLAY_LANDSCAPE_320X240=OWNER_CONFIRMED_PASS
DISPLAY_CLIPPING=NONE
RENDERER_TOUCH_COORDINATE_COMPENSATION=FORBIDDEN
TOUCH_MODEL_MUST_MATCH_FINAL_PANEL_GEOMETRY=YES
ACTUATOR_RELEASE=NO
```

### 8.1 Ownership und technische Grenze

Der bestehende `device_platform::applyTouchCalibration()`-Pfad bleibt der
einzige Owner der gespeicherten Raw->Screen-Kalibrierung. Die zusätzliche
produktseitige X-Spiegelung nach `applyTouchCalibration()` in
`main/fermentation_ui_lvgl_renderer.cpp` wird nach Freigabe vollständig
entfernt. Der Renderer darf keine zweite Touch-, Spiegelungs- oder
Rotationspolicy enthalten; er übernimmt ausschließlich das Ergebnis des
Kalibrierungsowners und begrenzt es auf die bestehende 320x240-Fläche.

Die bestehende technische Abbildung an der
`device_platform_esp_idf`-Adaptergrenze bleibt die einzige Paneltransform-
Quelle: `displayRotationTransform()` in
`lib/device_platform_esp_idf/src/esp_idf_display_touch_adapter.hpp` speist
Adapter und LVGL-Port. Für den bereits ausgewählten R1-Kandidaten wird dort
explizit die real bestätigte ILI9341-Transformabbildung dokumentiert und
getestet:

```text
R1 / DisplayRotation::Rotate90
swap_xy=true
mirror_x=true
mirror_y=true
```

`DisplayRotation` bleibt dabei ein bestehender Auswahlwert; es wird keine neue
Displayarchitektur und keine zweite R1-/Renderer-Konfiguration eingeführt. Die
R1-Auswahl kommt weiterhin ausschließlich aus dem gemeinsamen generierten
`kR1DisplayRotation`-Wert. Die konkrete ESP-IDF-Panelabbildung wird an dieser
bestehenden Adaptergrenze ausdrücklich als hardwareabhängiger R1-Befund
behandelt und nicht aus dem Enum-Namen theoretisch abgeleitet.

### 8.2 Nach Owner-Freigabe verbindliche Reihenfolge

Erst nach Freigabe der exakten Plan-SHA wird in kleinen Schnitten umgesetzt.
Eine neue FIT-/Validation-/Hold-Erfassung ist für diese Korrektur nicht
erforderlich.

1. Das bestehende reviewed `tc0`-Modell mit `recordSequence=1` und die
   aktuelle produktive X-Komposition unabhängig reproduzieren. Die bestehende
   Produkttransformation ist affin:

   ```text
   x_final = 319 - (a*raw_x + b*raw_y + c)
   y_final = d*raw_x + e*raw_y + f
   ```

   Deshalb wird ausschließlich das vorhandene Modell komponiert:

   ```text
   a' = -a
   b' = -b
   c' = 319 - c
   d' = d
   e' = e
   f' = f
   ```

   Für das aktuell reviewed Modell muss die unabhängige Rechnung exakt diese
   Werte reproduzieren; es werden keine neuen Messwerte erfunden:

   ```text
   a' = -0.00009043639686374949
   b' =  0.08753007024919984
   c' = -25.146273472211817
   d' =  0.06559259340326797
   e' =  0.0007485020274466806
   f' = -15.832052617145878
   ```

   Die vorhandene Raw-Capture-Evidence bleibt unverändert. Für eine
   mathematische Regression werden vorhandene Zielkoordinaten nur mit
   `x_final=319-x_old` umgerechnet; Center/Top-Mid werden nicht in einen
   neuen Fit aufgenommen.

2. Den bestehenden Provisioner um genau die kontrollierte `tc0`-Migration
   ergänzen. Der Vergleich umfasst Modell, Board-/Controller-ID und
   `recordSequence`; `tc1` wird nie beschrieben:

   ```text
   tc0 == old reviewed model, sequence=1
       -> new composed model, sequence=2 schreiben
       -> exakten Readback verifizieren

   tc0 == new composed model, sequence=2
       -> idempotent NOT_NEEDED

   tc0 == irgendetwas anderes, einschließlich NotFound/ungültigem Record
       -> STOP; nicht überschreiben

   tc1 -> UNCHANGED
   ```

   Es gibt kein NVS-Erase, kein generisches Force-Overwrite, keinen neuen
   Codec und keine zweite Provisionierungsstrecke.

3. Die Renderer-X-Kompensation vollständig entfernen und gezielte Tests
   ergänzen, die belegen, dass der Produktpfad ausschließlich
   `applyTouchCalibration()` verwendet. Der bestehende Paneltransform-Test
   prüft weiterhin die finale R1-Abbildung; Adapter und LVGL-Port konsumieren
   dieselbe technische Quelle.

4. Nach erfolgreichem `tc0`-Readback den normalen `esp32_bringup`-Produktpfad
   ohne Rendererkompensation bauen und laden lassen. `tc0` muss als
   `Available` erscheinen. Danach den ungefährlichen Product-Touch-Smoke mit
   `Home`, `System`, `App` und `Info` wiederholen; jeder Treffer muss allein
   über das komponierte Modell und den finalen Paneltransform erfolgen.

Die bestehenden Text-, WiFi-, `Service aus`- und Branding-Korrekturen, die
Rotation `ROTATE90`, der produktive Z-/Strength-Threshold, `tc1`, der
Issue-29-Probe und `ACTUATOR_RELEASE=NO` bleiben außerhalb dieses Deltas
unverändert.

### 8.3 Revalidierungs- und Stop-Vertrag

Die unabhängige Review erhält mindestens:

```text
FINAL_R1_PANEL_TRANSFORM=swap_xy:true,mirror_x:true,mirror_y:true
DISPLAY_LANDSCAPE_320X240=OWNER_CONFIRMED_PASS
DISPLAY_CLIPPING=NONE
RENDERER_TOUCH_COORDINATE_COMPENSATION=FORBIDDEN
TOUCH_MODEL_MUST_MATCH_FINAL_PANEL_GEOMETRY=YES
NEW_CALIBRATION_CAPTURE=NOT_REQUIRED
TC0_MIGRATION=SEQUENCE_1_TO_SEQUENCE_2
TC0_READBACK=PASS
TC1=UNCHANGED
PRODUCT_TOUCH_SMOKE=PASS
PRODUCT_Z_THRESHOLD=UNCHANGED
ACTUATOR_RELEASE=NO
NEXT_STEP=INDEPENDENT_FIX_VERIFICATION
```

Bis zur Owner-Freigabe dieser Planergänzung gilt fail-closed:

```text
IMPLEMENTATION=NOT_STARTED_FOR_THIS_REVISION
NEW_CALIBRATION_CAPTURE=NOT_REQUIRED
TC0_MIGRATION=NOT_RUN
PRODUCT_TOUCH_SMOKE=NOT_RUN_FOR_THIS_REVISION
ACTUATOR_RELEASE=NO
```


## 9. Owner-Scope-Erweiterung: Host-Clang auf Major 21 (revidiert)

Der Owner hat waehrend des finalen Pre-Ready-Laufs ausdruecklich entschieden,
die veraltete Host-Werkzeuglinie 18 noch in PR #156 zu bereinigen. Diese
Tooling-Erweiterung aendert keine Firmwarefunktion, keine Display-/Touch-
Semantik und keine Aktorgrenze, ist wegen Build-/Toolchain-Wirkung aber eine
materielle Planrevision und wird deshalb vor der Umsetzung erneut exakt
commitgebunden freigegeben.

```text
OWNER_SCOPE_DECISION=INCLUDE_HOST_CLANG21_IN_PR156
REVISION_BASE_HEAD=9ff03e25cd395fb7f6d8d617ff06123e2e49270d
HOST_CLANG_MAJOR=21
HOST_CLANG_PATCH=NOT_A_PROJECT_CONTRACT
ESP_CLANG_EXISTING_PIN=21.1.3
PLAN_REVISION_REASON=CLANG21_POLICY_DRIFT_IN_CHECK_FAMILIES
BASELINE_EVIDENCE=docs/audits/ISSUE_31_HOST_CLANG21_UNCHANGED_BASELINE_RAW.txt
PLAN_REVIEW_CORRECTIONS=THREE_LOCAL_CLANG21_COMPATIBILITY_FIXES_AND_FINAL_GATE_SPLIT
ACTUATOR_RELEASE=NO
PLAN_STATUS=OWNER_APPROVAL_REQUIRED
IMPLEMENTATION=NOT_STARTED_FOR_REVISED_CONTRACT
```

### 9.1 KISS-Toolchainvertrag

Der Hostvertrag fuer `clang-format` und `clang-tidy` wird gemeinsam von
Major 18 auf Major 21 angehoben. Der Patchlevel bleibt wie bisher kein
allgemeiner Hostvertrag. Der Versionswechsel darf keine neue Coding-Policy
einfuehren: bestehende fachliche, Correctness-, Safety- und Security-Checks
bleiben aktiv; neue reine Stil-, Praeprozessor- oder theoretische
Portabilitaetschecks werden nicht automatisch zum Merge-Gate.

Fuer GitHub-CI wird **keine** dritte LLVM-Quelle eingefuehrt und der Runner
bleibt auf `ubuntu-24.04`. Das dortige Standardimage liefert fuer diese
Werkzeuge weiterhin nur bis Major 18. CI verwendet deshalb den bereits fuer
ESP-IDF v6.1 kanonisch gepinnten und gecachten Espressif-`esp-clang`-
Werkzeugsatz `21.1.3` auch fuer die Host-`clang-format`-/`clang-tidy`-
Binaries. Lokal ist jeder nachweisliche Major-21-Werkzeugsatz zulaessig; die
aktuelle ESP-IDF-Umgebung liefert bereits `21.1.3`.

Es gibt keinen Wechsel auf Ubuntu 26.04, kein `apt.llvm.org`, keinen neuen
Toolchain-Downloader und keinen zweiten Versions-SSOT.

Die Baseline-Inventur auf `9ff03e2` ergab 75 als Fehler behandelte Diagnostics
ueber zehn kanonische Dateien. Die beiden neuen reinen Policy-Checks
`portability-avoid-pragma-once` und
`readability-use-concise-preprocessor-directives` werden fuer dieses Projekt
nicht uebernommen: `#pragma once` ist etablierter Projektstil und eine
Umstellung von mehr als 100 Headern waere Scope-Ausweitung; die
Praeprozessor-Umschreibungen waeren in diesem PR ebenfalls rein stilistisch.
Nur diese beiden konkreten Checks duerfen nach Owner-Freigabe gezielt in
`.clang-tidy` ausgeschlossen werden. Es werden keine ganzen Checkfamilien
pauschal abgeschaltet.

### 9.2 Betroffenes Delta

Nach Freigabe werden nur die aktiven Vertragsstellen angepasst:

- `scripts/run_pre_ready_gates.sh`: Host-Pruefung von Major 18 auf Major 21;
- `.github/workflows/build.yml`: die expliziten `clang-*-18`-Symlinks
  entfernen; nach der ohnehin vorhandenen Installation von `esp-clang`
  dessen `bin`-Verzeichnis fuer die Host-Phase voranstellen, danach die
  bestehende ESP-Phase unveraendert mit derselben gepinnten Toolchain fahren;
- `.clang-format` und `.clang-tidy`: aktive Werkzeugkommentare auf die
  neue Host-Major-Linie synchronisieren; `.clang-tidy` erhaelt nur die beiden
  oben genannten, begruendeten Einzel-Ausschluesse;
- `docs/CI_AND_QUALITY_GATES.md`: aktiven Hostvertrag und CI-Reihenfolge
  synchronisieren.
- `lib/fermentation_app/src/fermentation_application.cpp`: die bestehende
  fruehe `AbortAndCool && !intent.coolingPlan.has_value()`-Pruefung bleibt
  unveraendert vor `runIdentity_->allocateForApplication()`. Erst nach dieser
  erfolgreichen Pruefung wird lokal eine nachweisbar gueltige Referenz auf den
  Cooling-Plan gebunden und spaeter an `makeManualRunPlanRequest()` verwendet.
  Die Allocation-Reihenfolge, `InvalidInput`, Command-/Run-ID-Semantik und
  alle sonstigen Verhaltensvertraege bleiben unveraendert; kein `NOLINT` und
  keine Verlagerung der InvalidInput-Pruefung hinter die Allocation.
- `lib/fermentation_app/src/process_state_machine.hpp`: in
  `elapsedWithPrior()` nur die Addition nach der Division explizit klammern;
  keine Rechenlogik aendern.
- `lib/fermentation_app/src/run_commands.cpp`: `validStopOption()` in die
  bereits vorhandene anonyme Namespace-Grenze verschieben, ohne API oder
  Verhalten zu aendern.
- `test/test_storage_wireformat/test_storage_wireformat.cpp`: ausschliesslich
  die zwei von Clang 21 betroffenen kommentierten Byte-Array-Bloecke
  mechanisch formatieren.

Historische Plaene, Changelog-Eintraege und abgeschlossene Audit-Evidence
werden nicht rueckwirkend umgeschrieben.

### 9.3 Kompatibilitaetsgrenze

Vor irgendwelchen breitflächigen Codeaenderungen wird Major 21 gegen den
bestehenden Baum ausgefuehrt.

- Wenn `clang-format` 21 den bestehenden kanonischen Baum ohne neue
  Formatabweichungen akzeptiert, wird **kein** Source-Reformat erzeugt.
- Die vollstaendige Baseline-Ausgabe ist unter
  `docs/audits/ISSUE_31_HOST_CLANG21_UNCHANGED_BASELINE_RAW.txt` erhalten.
  Neben den beiden nicht uebernommenen Policy-Checks meldet Clang 21 auf dem
  unveraenderten Bestand `bugprone-unchecked-optional-access`,
  `readability-math-missing-parentheses` und `misc-use-internal-linkage`.
  Diese drei Befunde werden nach Freigabe dieses revidierten Plans mit den
  oben genannten kleinsten lokalen Korrekturen behoben; sie werden nicht
  unterdrueckt und nicht auf eine weitere Owner-/Review-Entscheidung
  verschoben.
- Fuer `prepareStop` werden gezielt beide Verhaltensfaelle regressiert:
  `AbortAndCool` ohne Cooling-Plan bleibt `InvalidInput`; ein gueltiger Plan
  bleibt erfolgreich und behaelt die bestehende Command-/Run-ID-Semantik.
  Die ungueltige Anfrage darf keine Command-ID verbrauchen; die naechste
  gueltige Command-ID bleibt unveraendert und der gueltige Stop behaelt seine
  bestehende Run-ID.
  Die bestehenden Process-State-Machine- und Run-Command-Tests muessen
  unveraendert PASS bleiben.
- Der Formatdiff beschraenkt sich auf zwei kommentierte Byte-Array-Bloecke in
  `test/test_storage_wireformat/test_storage_wireformat.cpp` und darf nach
  Freigabe mechanisch formatiert werden. Ein breiter Diff oder weitere
  Toolchain-/OS-Aenderungen bleibt ein **STOP**; keine Massennachformatierung
  und keine opportunistische Codebereinigung in PR #156.

### 9.4 Baseline-/Planphase

Die bereits gesicherte Baseline darf den Zwischenstand ausdruecken; sie ist
kein finaler Gate-Nachweis:

```text
HOST_CLANG_FORMAT_MAJOR=21
HOST_CLANG_TIDY_MAJOR=21
HOST_FORMAT_FULL_TREE=NOT_RUN_AFTER_REVISED_POLICY
HOST_CLANG_TIDY_CANONICAL_LIST=BLOCKED_BASELINE_CORRECTNESS_DIAGNOSTICS
HOST_CLANG21_BASELINE_DIAGNOSTIC_COUNT=75
HOST_CLANG21_POLICY_EXCLUSIONS=2_EXPLICIT_CHECKS_ONLY
PRE_READY_HOST=NOT_RUN
PRE_READY_ESP=NOT_RUN
PRE_READY_LOCAL_GATES=NOT_RUN
ACTUATOR_RELEASE=NO
```

### 9.5 Finaler Abschlussvertrag nach Implementation

Nach der Owner-Freigabe, den drei lokalen Korrekturen, den gezielten
Regressionstests und der Synchronisierung von Runner, CI und
Werkzeugdokumentation gilt ausschliesslich dieser finale Vertrag:

```text
HOST_CLANG_FORMAT_MAJOR=21
HOST_CLANG_TIDY_MAJOR=21
HOST_FORMAT_FULL_TREE=PASS
HOST_CLANG_TIDY_CANONICAL_LIST=PASS
ESP_CLANG_BRINGUP=PASS
ESP_CLANG_RELEASE=PASS
PRE_READY_HOST=PASS
PRE_READY_ESP=PASS
PRE_READY_LOCAL_GATES=PASS
ACTUATOR_RELEASE=NO
```

Gezielte Regression vor dem vollstaendigen Pre-Ready umfasst:

```text
PREPARE_STOP_ABORT_AND_COOL_WITHOUT_PLAN=INVALID_INPUT
INVALID_ABORT_AND_COOL_CONSUMES_COMMAND_ID=NO
NEXT_VALID_COMMAND_ID=UNCHANGED
PREPARE_STOP_ABORT_AND_COOL_WITH_VALID_PLAN=PASS
PREPARE_STOP_VALID_RUN_ID=UNCHANGED
PROCESS_STATE_MACHINE_TESTS=PASS
RUN_COMMAND_TESTS=PASS
```

Der bereits auf `8e7ee3f...` gestartete Pre-Ready-Lauf kann nach einem
Toolchain-Commit nicht als finaler Nachweis gelten, weil das verbindliche
Pre-Ready-Gate auf dem exakten finalen `HEAD` laufen muss. Hardware wird
dafuer nicht erneut geflasht oder wiederholt getestet. Nach Freigabe dieser
revidierten Plan-SHA muessen zuerst die zwei expliziten Policy-Ausschluesse
implementiert, die drei lokalen Korrekturen und die gezielten Regressionen
ausgefuehrt und danach die vollstaendige Verifikation auf dem finalen HEAD
ausgefuehrt werden.
