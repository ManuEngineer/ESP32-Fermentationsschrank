# Issue #31 – actor-free Touch-Kalibrierungs-Capture-Harness

## Zweck und Grenze

Der Harness ist ein bring-up-only Diagnosepfad fuer die Owner-Messung der
Touchkalibrierung. Er verwendet den bestehenden
`EspIdfDisplayTouchAdapter` mit dem generierten R1-Boardprofil und liest nur
controller-native `RawTouchSample`-Werte des XPT2046-Pfads.

Er startet keine `FermentationApplication`, schreibt keine NVS-/Touch-
Kalibrierungsrecords, berechnet keine Kalibrierkoeffizienten und aktiviert
keine Aktoren. Er ist kein zweiter Produkt- oder UI-Vertrag.

## Build

Mit aktivierter gepinnter ESP-IDF-6.1-Umgebung und sauberem Worktree:

```bash
python3 scripts/build_issue31_touch_calibration_harness.py
```

Der Build verwendet ausschließlich `esp32_bringup` und definiert
`APP_ISSUE_31_TOUCH_CALIBRATION_HARNESS`. Der normale Firmware-/CI-Pfad
kompiliert den Harness nicht. Zusätzlich wird nur in diesem privaten
Harness-Build `sdkconfig.defaults.issue31_touch_calibration` als letzte
Defaults-Schicht verwendet. Sie setzt den Treiber-Vorfilter
`CONFIG_XPT2046_Z_THRESHOLD=1`, dem kleinsten nicht-null Wert, den der
Treiber ohne `-Wtype-limits` akzeptiert. Damit wird der
produktartige Vorfilterwert 400 nicht vorweggenommen und der Harness kann die
Kontakt-/No-Contact- und Near-Contact-Verteilung messen. Das ist kein
Produktwert und wird nicht in Bring-up-/Release-Profile übernommen.

Nach dem Build kann der Owner den erzeugten Bring-up-Stand ueber den ueblichen
UART-/`idf.py flash monitor`-Pfad flashen und beobachten. Das Flashen ist
actor-free; die reale Touchinteraktion beginnt erst nach der
`CALIBRATION_CAPTURE_HARNESS=READY`-Meldung.

## Referenzpunkte und Ablauf

Der Harness zeigt nacheinander Crosshairs auf 320x240:

| ID | Rolle | Zielpunkt |
|---|---|---:|
| `FIT_TOP_LEFT` | FIT | `(32,32)` |
| `FIT_TOP_RIGHT` | FIT | `(287,32)` |
| `FIT_BOTTOM_LEFT` | FIT | `(32,207)` |
| `FIT_BOTTOM_RIGHT` | FIT | `(287,207)` |
| `VALIDATION_CENTER` | VALIDATION | `(160,120)` |
| `VALIDATION_TOP_MID` | VALIDATION | `(160,48)` |

Die vier Eckpunkte sind ein kleines, nichtkollineares, ueberbestimmtes
Affine-Fit-Set. Die beiden Validation-Punkte werden nicht fuer den Fit
verwendet. Dadurch wird keine dogmatische Fuenf-Punkt-Produktkalibrierung
eingefuehrt.

Fuer jeden Punkt:

1. Warten, bis das Crosshair sichtbar ist und die UART-Meldung
   `CALIBRATION_POINT_READY` erscheint.
2. Die Mitte bewusst beruehren und mindestens die angegebene Haltezeit
   halten.
3. Erst danach loslassen und den Release abwarten. Bei zu kurzer Beruehrung
   wird derselbe Punkt fail-closed erneut angefordert.
4. Nicht verschieben oder wischen; pro Punkt genau eine bewusste Beruehrung.

Nach den sechs Punkten folgt `HOLD_PROBE` in der Mitte als separate
Hold-/Release-Sequenz fuer Kontaktstabilitaet, Strength-/Z-Verteilung und
Release-/Debounce-Evidence.

## UART-Evidence-Vertrag

Der Harness schreibt pro Sample mindestens:

```text
CALIBRATION_SAMPLE id=... role=FIT|VALIDATION|HOLD target_x=... target_y=...
  status=CONTACT|RELEASE raw_x=... raw_y=... strength=... time_us=... contact=...
```

Zusammenfassungen enthalten Sampleanzahl, Raw-X/Y-Minimum/Maximum,
Strength-Minimum/Maximum sowie erste/letzte monotone Zeit. Controllerfehler
werden separat als `status=CONTROLLER_ERROR` geloggt und niemals in eine
Kontaktserie eingerechnet.

Erwartete Abschlussmarker:

```text
CALIBRATION_CAPTURE_HARNESS=READY
CALIBRATION_CAPTURE_PREFILTER=MEASUREMENT_SAFE XPT2046_Z_THRESHOLD=1
CALIBRATION_CAPTURE_LAYOUT=FIT_4_NONCOLLINEAR_PLUS_VALIDATION_2_INDEPENDENT
CALIBRATION_HOLD_RELEASE_SEQUENCE=READY
CALIBRATION_CAPTURE_HARNESS=COMPLETE
CALIBRATION_RECORD_WRITTEN=NO
FIT_VALUES_COMPUTED=NO
ACTUATOR_RELEASE=NO
```

Die Rohdaten werden danach offline gegen die separat markierten FIT- und
VALIDATION-Punkte ausgewertet. Erst eine Owner-freigegebene Auswertung darf
einen `TouchCalibrationRecord` erzeugen; der Harness selbst schreibt keinen.

## Statusgrenze

Der deterministische `esp32_bringup`-Build wurde mit ESP-IDF 6.1 erfolgreich
ausgefuehrt. Der Harness ist damit fuer die bewusst getrennte Owner-Messung
bereit; ein Flash-/UART-/Touchnachweis ist noch nicht ausgefuehrt.

Aktueller Status:

```text
CALIBRATION_CAPTURE_HARNESS_READY=PASS
NEW_OWNER_HARDWARE_INTERACTION_REQUIRED=YES
OWNER_UART_CAPTURE=NOT_RUN
CALIBRATION_RECORD=NOT_WRITTEN
```
