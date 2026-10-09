# Issue #33 – BTS7960-H-Bruecke: Softwareplan (Revision 1)

Gilt nur fuer den Software-/Adapterteil von #33. Die Hardwareabnahme
(`ADAPTER_SAFETY_VERIFICATION`, `FUNCTIONAL_HARDWARE_VERIFICATION`, begrenzte
Peltierpulse) bleibt offen; #33 wird durch den Software-Merge nicht
geschlossen. `ACTUATOR_RELEASE=NO`, `REAL_PELTIER_TEST=NOT_RUN`,
`SSOT_CONFORMANCE=PENDING`, `FUNCTIONAL_HARDWARE_VERIFICATION=PENDING`.

## 1. Ziel und Nicht-Ziele

Ziel: der kleinste reale Adapterpfad, der `device_platform::IBidirectionalActuatorSink`
(`setForward`/`setReverse`) auf die drei BTS7960-Signale RPWM (GPIO13), LPWM
(GPIO14) und gemeinsames R_EN/L_EN (GPIO25) abbildet — mit den harten
H-Bruecken-Invarianten aus Issue #33 auf Command-/GPIO-Ebene — und die im #32-Review
benannte Interlock-Luecke „verworfenes Aussenluefter-EIN ist unsichtbar“ an der
Stelle schliesst, an der sie entsteht.

Nicht-Ziele: keine reale Heiz-/Kuehlansteuerung, kein Flash, keine Hardwaretests;
keine Anbindung an Planner, Driver-Aufrufer oder Regelschleife in der Composition Root
(#35/#90); keine produktive `ActuatorSafetyGateStatus::Allowed`-Freigabe; **R_IS/L_IS
vollstaendig deaktiviert und unbeschaltet** (GPIO34/35 `reserved_disabled`, kein ADC,
keine Verdrahtung, keine Messpflicht, kein Generator-Output); kein Hardware-PWM/LEDC
(das Schaltfenster ist zeitproportional im Planner, `ACTUATOR_TIMING.md`); keine neue
Totzeit, kein Busy-Wait, keine zweite Aktorplanung; keine Aenderung von Boardprofil,
GPIO-Matrix oder Pulldowns; keine neue Bibliothek.

## 2. Verifizierte Ausgangslage

```text
BASELINE=main 92d6b823d566443226952917ec36c6b4f4125ef8 (enthaelt PR #195 / #32 S1–S3)
ISSUE_33=OPEN, BLOCKED_HARDWARE, REAL_ESP_IDF_BTS7960_ADAPTER_EXISTS=NO
```

- SSOT `config/board_profiles/esp32_32e_quad_mosfet_r1.yaml`: GPIO13 `bts7960_rpwm`,
  GPIO14 `bts7960_lpwm`, GPIO25 `shared_r_en_l_en` (alle `active_level: high`,
  `safe_boot_level: low`, externer 10-kOhm-Pulldown nach GND, `assignment_status:
  planned`); GPIO34/35 `reserved_disabled`, `ris_lis_enabled: false`.
- Port: `IBidirectionalActuatorSink` (`setForward`/`setReverse`, bewusst ohne erzwungene
  Exklusivitaet), `IBinaryOutputSink::setEnabled(bool)` ist `void`.
- Vorhanden und nur **konsumiert**: `ActuatorPlanSinkDriver` (setzt vor Heizen/Kuehlen die
  Gegenrichtung aus und schaltet den Aussenluefter vor der Richtung ein; `Unknown`/
  ungueltig = alles aus), `ActuatorPlanner` (Mindest-Auszeit und Polaritaetswechsel-
  Totzeit; ein Richtungswechsel laeuft nur ueber physisch `Idle`, bemessen ab dem realen
  Active→Idle; konkrete Werte `TBD_COMMISSIONING`), `ActuationInterlock`.
- #32-Adapter `EspIdfBinaryOutputSink` (rollenfrei, fail-closed, `Unconfirmed` = keine
  GPIO-Operation, GPIO-Quelle ueber den SSOT-Generator `scripts/generate_board_profile_
  header.py`) mit Linux-CMock-Hosttest.
- Das Produkt komponiert Planner/Driver/Peltier noch nicht (`main/app_main.cpp`; nur
  `issue_29_bringup_probe.cpp` mit `AllOff*`-Sinks).
- Vorbedingung aus #32 (Evidence): `setEnabled` liefert nichts zurueck; ein verworfenes
  Aussenluefter-EIN ist fuer den Driver unsichtbar, der Driver wuerde den Peltier
  trotzdem freigeben.

## 3. Kernentwurf

### 3.1 Portaenderung (kleinste notwendige, Owner-Entscheidung O1)

`IBinaryOutputSink::setEnabled(bool)` liefert `[[nodiscard]] bool`:
`true` genau dann, wenn der angeforderte Zustand am Ausgang **angewendet** wurde (der
Treiberaufruf war erfolgreich); `false` bei verworfenem EIN, Fehler, `Unconfirmed`
oder nicht gestartetem Ausgang. Das ist keine physische Bestaetigung der Last.
Betroffen: Port, `EspIdfBinaryOutputSink`, `MockBinaryOutputSink`, `AllOffBinarySink`
(Bring-up-Probe), `TracingBinarySink` (Test), Driver. Ohne diese Aenderung kann weder
die H-Bruecke einen Fehler eines Teilsignals erkennen noch der Driver ein verworfenes
Aussenluefter-EIN.

### 3.2 Driver: Aussenluefter-Interlock (Luecke schliessen)

In `ActuatorPlanSinkDriver::apply`: bei `Heating`/`Cooling` wird der Peltier nur
freigegeben, wenn `outerFan_.setEnabled(true)` `true` liefert; sonst werden beide
Peltierrichtungen ausgeschaltet (`setForward(false)`, `setReverse(false)`), der
Innenluefter folgt dem Ergebnis best effort. Reihenfolge bleibt: Gegenrichtung aus →
Aussenluefter → Richtung. Unveraendert: `Idle`/`Unknown`/ungueltige Richtung, kein
Nachlauf-/Planner-Eingriff. Nicht Teil des Plans (bleibt #35/#90): Meldung eines
Luefterfehlers nach oben, Fehlerklassifikation, Wiederanlauf; `apply` bleibt `void`.

### 3.3 `SharedEnableBridgeSink` (portabel, `device_platform`)

Rollenfreie Klasse `SharedEnableBridgeSink : IBidirectionalActuatorSink`, konstruiert
aus drei `IBinaryOutputSink&` (Vorwaertsschenkel, Rueckwaertsschenkel, gemeinsamer
Enable). Keine GPIO-, ESP- oder Rollenkenntnis, kein Zeitgeber. Die „reale“ Abbildung
ist die Komposition: drei `EspIdfBinaryOutputSink` (RPWM, LPWM, EN) + diese Klasse. Es
entsteht kein zweiter GPIO-/Polaritaetscode (DRY).

Zustaende: `NotStarted`, `Ready`, `Faulted` (intern, flags `forwardOn`, `reverseOn`).

Sequenzen (jeder Teilbefehl wird genau einmal versucht; jedes `false` = Fehler):

```text
begin():                     // einmalig, idempotent
  Enable AUS -> Vorwaerts AUS -> Rueckwaerts AUS
  alle true -> Ready; sonst shutdown() + Faulted

setForward(true) [Ready]:    // symmetrisch setReverse(true)
  Rueckwaerts aktiv      -> Konflikt: shutdown() + Faulted     (kein stilles Umschalten)
  Vorwaerts bereits aktiv -> No-op
  sonst: Vorwaerts EIN  (Enable noch AUS, Ausgang gesperrt)
         -> Enable EIN                                           (Enable zuletzt)
  Fehler -> shutdown() + Faulted

setForward(false) [Ready]:   // symmetrisch setReverse(false)
  Vorwaerts aktiv: Enable AUS (zuerst, Master-Sperre) -> Vorwaerts AUS
  Fehler -> shutdown() + Faulted

setX(true) in NotStarted/Faulted -> verworfen (nichts wird geschaltet)
setX(false) in Faulted           -> shutdown() best effort (nie EIN)

shutdown():  Enable AUS -> Vorwaerts AUS -> Rueckwaerts AUS     (je einmal, unabhaengig vom Ergebnis)
```

Invarianten: (1) Enable ist die Master-Sperre: zuerst aus, zuletzt ein; (2) beide Schenkel
werden nie gleichzeitig aktiv angefordert (Konflikt = Abschaltung + Verriegelung);
(3) ein Richtungswechsel laeuft nur ueber den vollstaendigen All-off-Zustand (Enable
und beide Schenkel erfolgreich AUS); (4) `Faulted` bleibt bis zum Neustart (O3);
(5) Fehler/unbekannter Zustand/falsche Aufrufreihenfolge = alles AUS, nie EIN.
`Unconfirmed` eines Teilsignals liefert `false` und macht die Bruecke damit nie
`Ready` (inert).

**Zusammenwirken mit der Planner-Totzeit (kein erdachtes Timing):** Die Bruecke
erzwingt die **Reihenfolge** (Break-before-make als All-off-Zwischenzustand), nicht
eine Dauer. Die Mindest-Auszeit/Polaritaetswechsel-Totzeit bleiben allein im Planner
(`TBD_COMMISSIONING`); sie ist zusaetzlicher Schutz und ersetzt den Low-Level-Interlock
nicht (Issue-#33-Vertrag Punkt 7). Eine GPIO-Mikrosekundenluecke zwischen AUS und
EIN wird nicht eingefuehrt (kein Busy-Wait, kein Wert ohne Messgrundlage). Restrisiko:
ein Aufrufer, der Rueckwaerts unmittelbar nach Vorwaerts AUS anfordert, wird von der
Bruecke nicht gebremst; der Planner verhindert das (Richtungswechsel nur ueber `Idle`
mit Totzeit). Eine Bruecken-Mindestauszeit mit injizierter Zeit waere eine spaetere,
messbasierte Option (O2).

### 3.4 Generator und Composition Root

Der Generator (`scripts/generate_board_profile_header.py`) wird um die Rollen
`bts7960_rpwm`, `bts7960_lpwm`, `shared_r_en_l_en` erweitert (`kBtsRpwmPin`=13,
`kBtsLpwmPin`=14, `kBtsEnablePin`=25 plus je `OutputPolarity` aus `active_level`; heute
`high` → `ActiveHigh`). GPIO34/35 werden **nicht** erzeugt (Selftest-Fall). Die
bisherigen Konstanten (Display/Touch/OneWire, drei MOSFET-Ausgaenge) bleiben unveraendert.

`main/app_main.cpp` konstruiert drei `EspIdfBinaryOutputSink` und die Bruecke und ruft
`begin()` beim Boot (O4); die Bruecke wird **nicht** an Driver/Planner angeschlossen und
`setForward`/`setReverse` werden in `main/` nicht aufgerufen (Textpruefung als Test).
Damit bleibt der Peltier im Produkt unerreichbar. Mit `begin()` werden GPIO13/14/25 als
Ausgaenge auf LOW gesetzt — der SSOT-Designzustand (externe Pulldowns, aktiv HIGH); das
ist keine elektrische Freigabe und keine Hardwarebestaetigung.

## 4. Umsetzungsschnitte (je ein Commit, danach Stopp zur Ownerpruefung)

1. **S1 Portaenderung + Driver-Interlock:** `setEnabled` → `bool` in Port, Mock,
   `EspIdfBinaryOutputSink` (Rueckgabewerte nach Abschnitt 3.1), `AllOffBinarySink`,
   Testdoppel; Driver-Fail-Closed bei verworfenem Aussenluefter-EIN; angepasste/neue
   Tests (nativ `test_actuator_plan_sink_driver`, Linux-Hosttest `esp_idf_binary_
   output_sink_host`).
2. **S2 `SharedEnableBridgeSink`:** Klasse + native Tests mit ablaufprotokollierenden
   Testdoppeln (gate-gefuehrt, kein ESP noetig).
3. **S3 Generator, Header, Composition Root, Doku:** Rollen, Selftest, Header,
   `app_main` (ohne Planner-Anbindung), `ACCEPTANCE_TESTS.md` (SIM-33-xx, HW-33-xx
   `NOT_RUN`), `ARCHITECTURE.md`, ROADMAP.
4. **S4 Builder-Self-Check** (`run_pre_ready_gates.sh self-check`), Build beider
   ESP-IDF-Profile, Beweisdokumentation (nur tatsaechlich Ausgefuehrtes mit exaktem HEAD).

## 5. Tests (software, deterministisch, hardwarefrei)

Neu `test_shared_enable_bridge_sink` (nativ, im Gate): begin-Reihenfolge und -Fehler
(je Teilsignal), Initialisierung = alles AUS; Sequenz EIN (Schenkel vor Enable) und AUS
(Enable vor Schenkel); Mutual Exclusion (Konflikt → Abschaltung + Verriegelung, nie beide
EIN in der Befehlsfolge); Break-before-make (Richtungswechsel nur ueber All-off, in der
Befehlsfolge nachgewiesen); Fehler an jedem einzelnen Teilbefehl (Vorwaerts, Rueckwaerts,
Enable, bei EIN und bei AUS) → shutdown-Folge, `Faulted`, keine Reaktivierung; Aufrufe
vor `begin()`, doppeltes `begin()`; `Unconfirmed`-Teilsignal → inert; unbekannte/leere
Folgen; Shutdown unabhaengig von Teilfehlern.

Angepasst/neu `test_actuator_plan_sink_driver`: verworfenes Aussenluefter-EIN bei
Heizen/Kuehlen → Peltier bleibt AUS, Reihenfolge der Befehle; bestehende Faelle
unveraendert. `esp_idf_binary_output_sink_host`: Rueckgabewerte `true/false` je Pfad.
Generator `--selftest`/`--check` (RPWM/LPWM/EN, keine GPIO34/35, bestehende Konstanten
unveraendert). Textpruefung: kein `setForward(`/`setReverse(` in `main/` ausser
Bring-up-Probe/Test. Regression: `test_actuator_planner` (Nachlauf, Mindest-Auszeit,
Totzeit), `test_actuation_interlock`, `check_architecture_boundaries.py` (Bruecke
rollen- und ESP-frei), beide ESP-IDF-Profile, Self-Check. Hardware bleibt `NOT_RUN`.

## 6. Spaetere Hardware-Gates (nicht Teil dieses Software-PR)

Vor Abschluss von #33 bzw. jeder realen Peltieraktivierung:

- **H1** konkrete IBT-2-/BTS7960-Modulvariante, Eingangsbuffer, Logikschwellen und
  3,3-V-HIGH-Kompatibilitaet, Versorgung/gemeinsame Masse.
- **H2** Enable-Funktion und reale Polaritaet (R_EN/L_EN gemeinsam), Pulldown-
  Beschaltung real vorhanden (RPWM, LPWM, EN) — funktional, keine neue Pegelmessung.
- **H3** Boot/Reset/Brownout/Bootloader: beide Richtungen und Enable bleiben aus
  (Verhalten von GPIO13/14/25 waehrend Reset/Boot gegen Datenblatt/TRM pruefen und
  funktional bestaetigen).
- **H4** unbelasteter Adapterpfad; Sicherung, einmalige Temperatursicherung,
  Kuehlkoerper, funktionsgepruefter Aussenluefter, Pflichtsensoren vor realem Peltier.
- **H5** begrenzte Heiz-/Kuehl-Servicepulse im geschuetzten Service-/Bring-up-Modus,
  Abbruch, Nachlauf, reale Mindest-Auszeit/Totzeit (Werte `TBD_COMMISSIONING`),
  Sensorentfernung → Sicherheitsreaktion.
- **H6** Release-Kette ausserhalb von #33: Aktor-Gate `Allowed`, Planner-/Regelschleifen-
  Komposition (#35/#90), Luefterfehlermeldung/-klassifikation nach oben.

Bestehende waivierte elektrische Pegelmessungen werden nicht zur neuen Pflicht.

## 7. Offene Ownerentscheidungen

- **O1** `IBinaryOutputSink::setEnabled` → `bool` (Empfehlung A: kleinste Aenderung, die
  Bruecken-Fehlererkennung **und** Aussenluefter-Interlock zugleich ermoeglicht; DRY ueber
  den vorhandenen GPIO-Adapter). B: Port unveraendert, Interlock bis #35/#90 als harte
  Freigabevoraussetzung dokumentieren und die Bruecke mit eigenem GPIO-Code bauen
  (Duplizierung von Polaritaet/Fehlerlogik, Luecke bleibt offen).
- **O2** Break-before-make = Reihenfolge (All-off-Zwischenzustand), Dauer allein im
  Planner (Empfehlung). Alternative: Bruecken-Mindestauszeit mit injizierter Zeit — erst
  nach Messgrundlage, kein Wert jetzt.
- **O3** `Faulted` bleibt bis Neustart (Empfehlung; fail-closed, wie der #32-Adapter).
  Alternative: explizite Re-Arm-Operation — haengt an #35/#90-Recovery, hier nicht noetig.
- **O4** Composition Root konstruiert die Bruecke und ruft `begin()` (GPIO13/14/25 aktiv
  LOW) ohne Planner-Anbindung (Empfehlung; Boot-Zustandsanker). Alternative: bis zum
  Hardware-Gate nichts konstruieren.

## 8. Risiken

- Die `bool`-Aenderung beruehrt den frisch gemergten #32-Pfad (Mock, Adapter, Tests).
- „Angewendet“ ist keine physische Bestaetigung der Last (Hardware H1–H5).
- Zeitliche Mindest-Auszeit liegt ausschliesslich im Planner; ein fehlerhafter Aufrufer
  wird von der Bruecke nur ueber Reihenfolge/Konflikt begrenzt.
- Linux-Hosttest `esp_idf_binary_output_sink_host` ist weiterhin nicht gate-gefuehrt; die
  neue Bruecken-Logik ist es (nativ).
