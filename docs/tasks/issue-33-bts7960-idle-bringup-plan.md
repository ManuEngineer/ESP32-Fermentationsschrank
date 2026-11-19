# Issue #33 – BTS7960-Leerlauf-Bring-up (manueller Softwaretestpfad, Plan Revision 1)

Status: **Plan zur Review und Ownerfreigabe der exakten Plan-Commit-SHA.** Keine
Implementierung, kein Testimage, kein Flash, keine reale Schaltung vor Freigabe.
Baseline: `origin/main` `47fbe25` (PR #216 gemergt; PR #214/#215 gemergt).

Gilt nur fuer #33 (Teilumfang H4 "unbelasteter Adapterpfad"). Der gemergte
Softwareplan `docs/tasks/issue-33-bts7960-hbridge-plan.md` (Rev. 2, `d55d2db`) und seine
Sicherheitsregeln bleiben unveraendert. `ACTUATOR_RELEASE=NO`,
`REAL_PELTIER_TEST=NOT_RUN`.

## 1. Ziel und Nicht-Ziele

Ziel: einen minimalen, **ausschliesslich manuell** aktivierbaren Bring-up-Testpfad fuer die
bestehende `peltierBridge` bereitstellen, damit der Owner spaeter im Leerlauf (kein Peltier,
kein Verbraucher an den Ausgangsklemmen) mit einem Multimeter die Brueckenfunktion
(`forward`/`reverse`/`off`) pruefen kann.

Nicht-Ziele: kein neuer BTS7960-Treiber, keine neue Schnittstelle, keine Planner-/
Driver-/Release-Anbindung, kein Peltieranschluss, keine Temperatur-/Lastversuche, keine
PWM, keine R_IS/L_IS-Auswertung, keine produktive `Allowed`-Freigabe, keine neue
Hardware- oder Spannungstestmatrix, keine Wiederholung bestandener Kampagnen. Verdrahtung,
gemeinsame Masse und externe Pulldowns gelten laut Owner als bestaetigt (keine neue
Messreihe).

## 2. Verifizierter Bestand (Reuse)

- `device_platform::SharedEnableBridgeSink` (Port `IBidirectionalActuatorSink`): Enable ist
  Master-Sperre (zuletzt ein, zuerst aus), nie beide Schenkel gleichzeitig (Widerspruch
  schaltet alles ab und verriegelt), Richtungswechsel nur ueber All-off, jeder Fehler/
  falsche Aufruf verriegelt bis Neustart; **kein Zeitgeber**; `setForward`/`setReverse`
  sind `void` (kein Fehlerrueckgabewert).
- `device_platform_esp_idf::EspIdfSharedEnableBridge`: ESP-Adapter, `begin()` setzt Enable
  (GPIO25) → RPWM (GPIO13) → LPWM (GPIO14) auf inaktiv; Pins/Polaritaet aus der generierten
  SSOT (`kBts*`, alle `ActiveHigh`).
- `main/app_main.cpp` erzeugt `peltierBridge` und ruft nur `begin()` (Log
  `peltier bridge: ready_all_off_not_connected_to_planner`); kein Richtungsbefehl.
- `test_peltier_composition_guard` (`SIM-33-05`): `main/` darf nirgends `setForward(`/
  `setReverse(` aufrufen (Ausnahme: `issue_29_bringup_probe.cpp` mit eigenen AllOff-Sinks);
  `app_main.cpp` ohne `ActuatorPlanSinkDriver`/`ActuatorPlanner`/ADC/R_IS/L_IS.
- UART-Bring-up-Muster: `main/issue_32_output_pulse_harness.*` (UART0-Treiber, zeilenweise
  Kommandos, begrenzter Puls per `esp_timer`, Abbruch, Log-Tags), Build-Muster
  `scripts/build_issue32_output_pulse.py`, CMake-Variable `APP_ISSUE_32_OUTPUT_PULSE`
  (nur `esp32_bringup`, exklusiv zu den anderen Harnesses).

## 3. Umsetzung (Code-Delta C1)

**Neuer, nur im Bring-up gebauter Harness** `main/issue_33_bridge_idle_harness.{hpp,cpp}`,
Hineinkompiliert nur mit `APP_ISSUE_33_BRIDGE_IDLE_TEST` (Bring-up-Profil, exklusiv zu
`APP_ISSUE_29/30/31/32/90`-Harnesses; Release-Profil enthaelt weder Datei noch Aufrufer).
Er haelt eine Referenz auf den vorhandenen `IBidirectionalActuatorSink` (die
`peltierBridge` aus `app_main`) und das Ergebnis von `begin()`; keine eigene
GPIO-Logik.

Kommandos (UART0, zeilenweise, im Log eindeutig mit Tag `ISSUE33_`):
`bridge forward`, `bridge reverse`, `bridge off`.

Verhalten:

- Kein Schaltvorgang bei Boot und kein automatischer Folge-/Richtungswechsel. Bei
  `begin()` ohne Erfolg (Bruecke nicht bereit) wird jede Aktivierung abgelehnt
  (`reason=BRIDGE_NOT_READY`).
- `forward`/`reverse` nur aus dem Leerlaufzustand des Harness (keine aktive Richtung);
  zuerst werden beide Richtungen explizit AUS befohlen, danach genau eine Richtung EIN
  (Gegenrichtung bleibt aus); die Bruecke erzwingt zusaetzlich Enable zuletzt und
  All-off-Wechsel.
- Feste Einschaltdauer **10 s** je Befehl (`esp_timer`, Pruefung in `update()`); Fristablauf
  und `bridge off` schalten beide Richtungen AUS (ueber den bestehenden Port). Jede andere
  Eingabe, ein Zeilenueberlauf oder ein ungueltiges Kommando waehrend einer aktiven
  Richtung bricht ab (AUS-Befehl, Log `ABORT`); im Leerlauf wird ein ungueltiges Kommando
  abgelehnt (`REJECTED`) ohne Schaltvorgang.
- Logs: `ISSUE33_READY` (bring_up_only, bridge_ready, Kommandos, Dauer),
  `ISSUE33_BRIDGE direction=<forward|reverse> state=ON_COMMANDED`, `... state=OFF_COMMANDED
  reason=<TIMEOUT|OFF|UNEXPECTED_INPUT|...>`. **Grenze:** `setForward/setReverse` liefern
  keinen Fehlerwert; die Logs belegen nur den *gegebenen Befehl*, nicht den physischen
  Pegel oder dass die Bruecke nicht intern verriegelt hat (dann bleibt der Ausgang AUS).
  Es wird keine Fehlerfreiheit der physischen Ausgaenge aus Softwarelogs behauptet.

`app_main.cpp`: nur zwei kleine Aenderungen unter `#ifdef APP_ISSUE_33_BRIDGE_IDLE_TEST`
bzw. neutral: das `begin()`-Ergebnis in einer Variable halten (statt nur im Log), und im
Bring-up-Block den Harness mit `peltierBridge` instanziieren, `start()` und `update()` in
der Hauptschleife (Muster #32). `app_main.cpp` selbst ruft weiterhin **kein**
`setForward(`/`setReverse(` auf.

**Build:** `scripts/build_issue33_bridge_idle_test.py` (abgeleitet von
`build_issue32_output_pulse.py`, Build-Verzeichnis `build/esp32_bringup_issue33`, keine
`--allow-dirty-source-tree`-Nutzung fuer Hardwareimages), `main/CMakeLists.txt` und
Top-Level-`CMakeLists.txt` um den Variablenblock erweitert (Muster #32, wechselseitig
exklusiv, `esp_driver_uart` optional).

**Composition-Guard (`SIM-33-05`) gezielt eingegrenzt statt abgeschaltet:** Die
Ausnahmeliste fuer `setForward(`/`setReverse(` in `main/` wird um genau
`issue_33_bridge_idle_harness.cpp` ergaenzt; neue Assertions: (a) die Harness-Dateien sind
vollstaendig in `#if defined(APP_ISSUE_33_BRIDGE_IDLE_TEST)` gekapselt, (b) `main/CMakeLists.txt`
fuehrt sie nur im Block `if(APP_ISSUE_33_BRIDGE_IDLE_TEST)`, (c) `app_main.cpp` ruft
weiterhin nirgends `setForward(`/`setReverse(` auf und bleibt ohne Planner/Driver/ADC/
R_IS/L_IS, (d) die Harness-Dateien enthalten kein `ActuatorPlanSinkDriver`/
`ActuatorPlanner`.

**Tests/Builds (gezielt):** `test_peltier_composition_guard`, `test_shared_enable_bridge_sink`,
`esp_idf_shared_enable_bridge_host`, `check_architecture_boundaries.py` (+ `--selftest`),
Builds `bringup`, `release` und `build_issue33_bridge_idle_test.py`, Release-Binary mit 0
Treffern fuer `ISSUE33`/`bridge forward`/`bridge reverse`. Ein Hosttest des UART-Kommando-/
Timeout-/Abbruchpfads ist **nicht** vorgesehen (wie beim bestandenen #32-Harness haengt er
an `esp_timer`/UART0; eine eigene Testinfrastruktur waere neue Harness-Infrastruktur,
siehe D3); der Pfad wird per Review und spaeter am Geraet nachgewiesen.

## 4. Spaeterer manueller Hardwaretest (nicht Teil dieser Freigabe)

Gates, je separat vom Owner freizugeben und vor Ort: **G1** BTS7960-12-V-Leistungsversorgung
anschliessen (geeignet abgesichert/strombegrenzt), **ohne Peltier und ohne sonstigen
Verbraucher** an den zwei Ausgangsklemmen; **G2** DEV-App-only-Flash des Bring-up-Images
(`build_issue33_bridge_idle_test.py`, sauberer HEAD, Hash-/Readback, kein Erase/NVS/
State-Store/Bootloader/Partitionstabelle, kein Backup; Freigabe je Flash); **G3**
Multimeter (DC-V) zwischen genau den zwei Ausgangsklemmen, unveraenderte Polung: je ein
manuell ausgeloestes `bridge forward`/`bridge reverse` mit `bridge off` dazwischen/danach
und genug Zeit zum Ablesen. Erwartung unter funktionierender Bruecke naeherungsweise
+12 V bzw. −12 V (Vorzeichen abhaengig von den Messleitungen). **Ein Wert nahe 0 V im
abgeschalteten, unbelasteten Zustand ist kein verlaesslicher Nachweis physischer
Abschaltung (Ausgang kann hochohmig/floating sein); ±12 V im Leerlauf ersetzt keinen
Belastungs-, Sensor- oder Peltier-Safety-Nachweis.**

Stopkriterien: unplausible Spannung, unerwartete Aktivierung (auch beide Richtungen
gleichzeitig oder ohne Kommando), verweigerte Abschaltung (Spannung bleibt nach `off`/
Fristablauf), Reset/Panic/WDT, ungeklaerte Versorgung/Polung, keine sichere
Trennmoeglichkeit der 12-V-Versorgung, Peltier oder Verbraucher angeschlossen. Keine
Wiederholungsautomatik.

## 5. Sicherheitsabgrenzung

- Der manuelle Pfad umgeht die Aussenluefter-Sperre des `ActuatorPlanSinkDriver` (der
  Driver ist nicht beteiligt). Das ist nur vertretbar, weil ausdruecklich **kein Peltier
  und kein Verbraucher** angeschlossen ist (Stopkriterium); sobald ein Peltier
  angeschlossen werden soll, ist dieser Pfad nicht mehr zulaessig und ein eigener Plan mit
  Aussenluefter-/Sensor-/Schutzvoraussetzungen (#33 H5, `HARDWARE.md`) noetig.
- Die R1-Bootabweichung der Luefter betrifft die BTS7960 nicht; GPIO13/14/25 bleiben nach
  Boot, Reset und ohne Kommando inaktiv (`begin()`-Zustand).
- `ACTUATOR_RELEASE=NO`; Release-Image unveraendert.

## 6. Dokumentation und Evidence

`docs/ACCEPTANCE_TESTS.md`: `SIM-33-05` auf den eingegrenzten Guard anpassen; neue Zeile
`HW-33-06` (Leerlauf-Brueckentest) `NOT_RUN`; `HW-33-03` bleibt `NOT_RUN` (Teilnachweis
spaeter). Neue Evidence-Datei `docs/audits/ISSUE33_BRIDGE_IDLE_EVIDENCE.md` erst nach einem
Hardwarelauf; `docs/ROADMAP.md` Zeile #33. Keine Aussage `REAL_PELTIER_TEST=PASS`.

## 7. Owner-Entscheidungen

- **D1:** Freigabe der exakten Plan-SHA.
- **D2:** Einschaltdauer 10 s je manueller Richtung und Kommandonamen `bridge forward|
  reverse|off` (alternativ andere Dauer/Namen).
- **D3:** Kein Hosttest fuer den Harness-Kommando-/Timeout-/Abbruchpfad (Begruendung §3);
  Alternative: kleine testbare Zustandsmaschine als eigene Datei (zusaetzlicher Umfang).
- **D4:** Umgehung der Aussenluefter-Sperre im manuellen Leerlaufpfad nur unter der
  Bedingung "kein Peltier/Verbraucher" (§5) akzeptiert.
- **D5:** Fuer G2 gilt die DEV-App-only-Flash-ohne-Backup-Regel (#190/#32-Praezedenz),
  Freigabe je Flash-Vorgang.
- **D6:** G1 (12 V anschliessen) und G3 (Messung) bleiben getrennte Owner-Gates.
