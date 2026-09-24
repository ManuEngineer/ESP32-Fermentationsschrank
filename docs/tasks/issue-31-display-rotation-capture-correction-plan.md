# Issue #31 – Korrekturplan: Displayrotation, Geometrie und Touch-Capture

## Planvertrag

| Feld | Wert |
|---|---|
| Issue | #31 – Renderer, Display, Touch und Kalibrierung |
| PR | #156, bestehender Draft-PR |
| Branch | `agent/issue-31-renderer-display-touch-plan` |
| Korrekturtyp | Materielle Planrevision nach realem Hardwarebefund |
| Ausgangs-HEAD | `9ed1dd6b729b95e92c62a36ff59dc5867dcd68da` |
| Bisher freigegebener Plan | `63fd88372b883887668566047c8a6acac48addcd` |
| Neue Planfreigabe | `OWNER_APPROVAL_REQUIRED` |
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

Die lokal vorhandene Rohdatei des ersten Captures wird in der
Umsetzungsphase bytegenau in ein dauerhaftes Repository-Evidence-Artefakt
übernommen, bevor neue Messdaten entstehen:

```text
SOURCE=/tmp/issue31-plan-revalidation.Ao1NTF/build/issue31_owner_touch_capture_uart_20260924.log
SOURCE_SHA256=e19b5caf3d6f19614a12b878ce8d973a0a3a2e0a263737b11c406f7ce9c20f08
TARGET=docs/audits/ISSUE_31_TOUCH_CAPTURE_20260924_INVALID_DISPLAY_GEOMETRY_RAW.log
```

Die zugehörige SHA256-Datei wird neben dem Artefakt abgelegt. Der bisherige
Harness bleibt als Roh-Touch-Provenienz erhalten; kein historischer Log wird
durch eine Summary ersetzt.

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

Die bestehende `EspIdfDisplayTouchConfig` erhält genau ein
`device_platform::DisplayRotation`-Feld. Die bestehende Port-Enumeration und
die bestehende Adapterlogik werden wiederverwendet.

- Product-Composition und Issue-31-Capture-Harness setzen dasselbe
  R1-Rotationsfeld.
- `EspIdfDisplayTouchAdapter::initialize()` wendet die Konfiguration genau
  einmal nach Panel-Init an.
- Die harten `setRotation(Rotate0)`-Aufrufe aus Product und Harness entfallen.
- LVGL erhält keine zusätzliche Rotation.
- `Rotate0` und `Rotate180` werden für den R1-320x240-Vertrag nicht als
  Landscape-Kandidaten verwendet.

Die zwei zulässigen Kandidaten werden ausschließlich real unterschieden:

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
| `docs/audits/` | alter Rohlog unverändert sichern, neuer Rohlog und SHA nach Hardwarelauf |
| PR-/Issue-Handover | Rotations-, Geometrie-, Capture- und Evidence-Status synchronisieren |

Keine neue öffentliche Portfunktion und keine Änderung an
`fermentation_app`.

## 5. Verifikation und Gates

Nach der Umsetzung werden gezielt und getrennt ausgewiesen:

```text
NATIVE_TESTS=PASS
RENDERER_BOUNDARY_TESTS=PASS
ARCHITECTURE_GUARD=PASS
BOARD_PROFILE_SINGLE_SOURCE=PASS
GIT_DIFF_CHECK=PASS
ESP32_BRINGUP_BUILD=PASS
ESP32_RELEASE_BUILD=PASS
ISSUE31_CAPTURE_HARNESS_BUILD=PASS
```

Die Hardwarereihenfolge ist verbindlich:

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

Danach werden keine Koeffizienten, NVS-Datensätze, produktiven
Strength/Z-Werte, Ready-/Merge-Aktionen oder Aktorfreigaben vorgenommen.
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
