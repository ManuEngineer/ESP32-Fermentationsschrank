# Issue #33 – BTS7960-H-Bruecke: Softwareplan (Revision 2)

Eigenstaendig ausfuehrbare Fassung; sie ersetzt Revision 1 (`6c8065c`) vollstaendig.
Gilt nur fuer den Software-/Adapterteil von #33. Die Hardwareabnahme
(`ADAPTER_SAFETY_VERIFICATION`, `FUNCTIONAL_HARDWARE_VERIFICATION`, begrenzte
Peltierpulse) bleibt offen; #33 wird durch den Software-Merge nicht geschlossen.
`ACTUATOR_RELEASE=NO`, `REAL_PELTIER_TEST=NOT_RUN`, `SSOT_CONFORMANCE=PENDING`,
`FUNCTIONAL_HARDWARE_VERIFICATION=PENDING`.

## 1. Ziel und Nicht-Ziele

Ziel: der kleinste reale Adapterpfad, der `device_platform::IBidirectionalActuatorSink`
(`setForward`/`setReverse`) auf RPWM (GPIO13), LPWM (GPIO14) und gemeinsames R_EN/L_EN
(GPIO25) abbildet, mit den harten H-Bruecken-Invarianten aus Issue #33 auf
Command-/GPIO-Ebene; zusaetzlich die Interlock-Luecke „verworfenes Aussenluefter-EIN ist
dem Driver unsichtbar“ (Vorbedingung aus #32) an ihrer Entstehungsstelle schliessen.

Nicht-Ziele: keine reale Heiz-/Kuehlansteuerung, kein Flash, keine Hardwaretests; keine
Anbindung an Planner, Driver-Aufrufer oder Regelschleife in der Composition Root
(#35/#90); keine produktive `ActuatorSafetyGateStatus::Allowed`-Freigabe; **R_IS/L_IS
und GPIO34/35 bleiben fuer R1 deaktiviert, unbeschaltet und ohne Testpflicht** (kein ADC,
keine Verdrahtung, kein Generator-Output); kein LEDC/Hardware-PWM (das Schaltfenster ist
zeitproportional im Planner, `ACTUATOR_TIMING.md`); keine zweite Zeitbasis, kein
Delay/Timer/Busy-Wait, keine neuen Commissioning-Zeitwerte, keine zweite Aktorplanung;
keine Aenderung von Boardprofil, GPIO-Matrix oder Pulldowns; keine neue Bibliothek.

## 2. Verifizierte Ausgangslage

```text
BASELINE=main 92d6b823d566443226952917ec36c6b4f4125ef8 (enthaelt PR #195 / #32 S1–S3)
ISSUE_33=OPEN, BLOCKED_HARDWARE, REAL_ESP_IDF_BTS7960_ADAPTER_EXISTS=NO
```

- SSOT `config/board_profiles/esp32_32e_quad_mosfet_r1.yaml`: GPIO13 `bts7960_rpwm`,
  GPIO14 `bts7960_lpwm`, GPIO25 `shared_r_en_l_en` (alle `active_level: high`,
  `safe_boot_level: low`, externer 10-kOhm-Pulldown nach GND, `assignment_status:
  planned`); GPIO34/35 `reserved_disabled`, `ris_lis_enabled: false`.
- Ports: `IBidirectionalActuatorSink` (`setForward`/`setReverse`, bewusst ohne erzwungene
  Exklusivitaet); `IBinaryOutputSink::setEnabled(bool)` ist heute `void`.
- Vorhanden und nur **konsumiert**: `ActuatorPlanSinkDriver` (Gegenrichtung zuerst aus,
  Aussenluefter vor der Richtung ein, `Unknown`/ungueltig = alles aus), `ActuatorPlanner`
  (Mindest-Auszeit und Polaritaetswechsel-Totzeit; ein Richtungswechsel laeuft nur ueber
  physisch `Idle`; konkrete Werte `TBD_COMMISSIONING`), `ActuationInterlock`.
- Vorhandener GPIO-Adapter `EspIdfBinaryOutputSink` (#32): rollenfrei, fail-closed,
  `Unconfirmed` = keine GPIO-Operation, `begin()` setzt den inaktiven Pegel vor dem
  Ausgangsmodus; Pins/Polaritaet kommen ueber `scripts/generate_board_profile_header.py`
  aus der SSOT; Linux-CMock-Hosttest `test/esp_idf_binary_output_sink_host`.
- Direkte Konsumenten/Implementierer von `IBinaryOutputSink`: Port, `EspIdfBinaryOutputSink`,
  `MockBinaryOutputSink` (Test-Support), `AllOffBinarySink` (`main/issue_29_bringup_probe.cpp`),
  `TracingBinarySink` (`test/test_actuator_plan_sink_driver`), `ActuatorPlanSinkDriver`
  (einziger Aufrufer in `lib/`); indirekt `TemperatureControlApplicationOrchestrator`
  (haelt den Driver) und Tests mit Driver+Mock: `test_configuration_service`,
  `test_factory_reset_flow` (Quelltext-Guard auf `setEnabled(`/`setForward(`),
  `test_run_persistence_coordinator`, `test_sensor_actuator_mocks`.
- Das Produkt komponiert Planner/Driver/Peltier noch nicht (`main/app_main.cpp`; nur
  `issue_29_bringup_probe.cpp` mit `AllOff*`-Sinks). Die #32-Ausgaenge sind nicht an den
  Planner angeschlossen.

## 3. Kernentwurf

### 3.1 Portvertrag (Ownerentscheid O1 = A)

`IBinaryOutputSink::setEnabled(bool)` wird `[[nodiscard]] bool`. `true` bescheinigt
**ausschliesslich**, dass der Befehl in einem betriebsbereiten Zustand erfolgreich am
Ausgang ausgefuehrt wurde (der Treiberaufruf war erfolgreich). Es ist **keine** Aussage ueber
Lueferdrehung oder elektrische Lastfunktion.

| Fall | Rueckgabe | Wirkung |
|---|---|---|
| bereit, Befehl ausgefuehrt (EIN oder AUS) | `true` | Pegel gesetzt |
| bereit, Treiberfehler | `false` | Fehler verriegelt (`Faulted`), einmaliger Best-effort-Inaktivversuch |
| `NotStarted` (vor `begin()`) | `false` | kein GPIO-Zugriff |
| `Unconfirmed` | `false` | kein GPIO-Zugriff |
| `Faulted`, EIN | `false` | verworfen, kein GPIO-Zugriff |
| `Faulted`, AUS | `false` | ein Best-effort-Versuch zum inaktiven Pegel; **hebt die Verriegelung nie auf** |
| ungueltiger Pin/ungueltige Polaritaet | `false` | wie `Faulted` |

Umsetzung je Implementierer:

- `EspIdfBinaryOutputSink`: Rueckgaben gemaess Tabelle (bestehende Zustandsmaschine,
  keine neue Logik); Hosttest wird um die Rueckgabewerte je Pfad erweitert.
- `MockBinaryOutputSink`: uebernimmt den Befehl nur, wenn er „akzeptiert“; neuer
  Test-Schalter (Standard: akzeptiert) fuer Ablehnung; Rueckgabe = akzeptiert.
- `AllOffBinarySink` (#29-Bring-up): meldet ein EIN **nie** als angewendet
  (`setEnabled(true)` → `false`, `releaseObserved` bleibt wie bisher), AUS → `true`.
- `TracingBinarySink` (Test): reicht das Ergebnis des inneren Mocks durch.
- Keine zusaetzlichen Parallelports.

### 3.2 Driver: Aussenluefter-Sperre (Interlock-Luecke schliessen)

In `ActuatorPlanSinkDriver::apply` bei `Heating`/`Cooling`: Gegenrichtung aus →
`outerFan_.setEnabled(true)`; liefert es `false`, wird **keine** Peltierrichtung freigegeben
(`setForward(false)`, `setReverse(false)`), der Innenluefter folgt dem Ergebnis best effort.
Nur bei `true` folgt die angeforderte Richtung (Reihenfolge unveraendert). `Idle`,
`Unknown` und ungueltige Richtung bleiben wie bisher; deren Ergebnisse werden explizit
verworfen (AUS-/Nachlaufbefehle, es gibt nichts weiter zu sperren). `apply` bleibt `void`.
Nicht Teil des Plans (#35/#90): Meldung eines Luefterfehlers nach oben, Klassifikation,
Wiederanlauf.

### 3.3 `SharedEnableBridgeSink` (portabel, `device_platform`)

Rollenfreie Klasse `SharedEnableBridgeSink : IBidirectionalActuatorSink`, konstruiert aus
drei `IBinaryOutputSink&` (Vorwaertsschenkel, Rueckwaertsschenkel, gemeinsamer Enable).
Keine GPIO-, ESP-, Rollenkenntnis, kein Zeitgeber. Zustaende `NotStarted`, `Ready`,
`Faulted` (intern; Flags `forwardOn`, `reverseOn`). Jeder Teilbefehl wird je Sequenz genau
einmal versucht; `false` eines Teilbefehls = Fehler.

```text
begin():                      // einmalig; zweiter Aufruf ohne Wirkung
  nur gueltig, wenn alle drei Sinks bereits erfolgreich initialisiert wurden (3.4);
  Enable AUS -> Vorwaerts AUS -> Rueckwaerts AUS, alle muessen true liefern
  alle true -> Ready;  sonst shutdown() + Faulted
  (setEnabled(false) auf einem nicht initialisierten Sink liefert false und zaehlt
   daher NIE als erfolgreiche All-off-Initialisierung)

setForward(true) [Ready]:     // symmetrisch setReverse(true)
  Rueckwaerts aktiv       -> Konflikt: shutdown() + Faulted     (kein stilles Umschalten)
  Vorwaerts bereits aktiv -> ohne Wirkung
  sonst: Vorwaerts EIN (Enable noch AUS, Ausgang gesperrt) -> Enable EIN (Enable zuletzt)
  Fehler -> shutdown() + Faulted

setForward(false) [Ready]:    // symmetrisch setReverse(false)
  Vorwaerts aktiv: Enable AUS (zuerst, Master-Sperre) -> Vorwaerts AUS;  Fehler -> shutdown() + Faulted
  nicht aktiv: ohne Wirkung

setX(true) in NotStarted/Faulted -> verworfen; setX(false) in NotStarted -> ohne Wirkung
setX(false) in Faulted           -> shutdown() best effort (nie EIN, Verriegelung bleibt)
shutdown():  Enable AUS -> Vorwaerts AUS -> Rueckwaerts AUS     (je einmal, unabhaengig vom Ergebnis)
```

Invarianten: (1) Enable ist die Master-Sperre: zuerst aus, zuletzt ein; (2) beide Schenkel
werden nie gleichzeitig angefordert (Konflikt = Abschaltung + Verriegelung); (3) ein
Richtungswechsel laeuft nur ueber den vollstaendigen All-off-Zustand (Enable und beide
Schenkel erfolgreich AUS); ein unmittelbar folgender Gegenrichtungsbefehl umgeht das
nicht; (4) `Faulted` bleibt bis zum Neustart (O3), ohne Re-Arm-Operation; ein Neustart ist
keine automatische Aktorfreigabe; (5) Fehler, unbekannter Zustand, falsche
Aufrufreihenfolge = alles AUS, nie EIN. „AUS“ ist ein softwareseitiger Befehl, keine
garantierte physische Abschaltung bei Treiber-/Hardwarefehler.

**Zusammenwirken mit der Planner-Totzeit (O2, bestehender Vertrag):** Die Bruecke erzwingt
die **Reihenfolge** (Break-before-make als tatsaechliche All-off-Befehlsfolge), nicht eine
Dauer. Mindest-Auszeit und Polaritaetswechsel-Totzeit bleiben allein im Planner und sind
zusaetzlicher Schutz; sie ersetzen den Low-Level-Interlock nicht (Issue-#33-Vertrag
Punkt 7). Es gibt keine zweite Zeitbasis. Restrisiko (dokumentiert): ein Aufrufer, der die
Gegenrichtung unmittelbar nach AUS anfordert, wird von der Bruecke nur ueber Reihenfolge/
Konflikt begrenzt; die zeitliche Sperre liefert der Planner.

### 3.4 Realer ESP-IDF-Adapter `EspIdfSharedEnableBridge` und Boot-Initialisierung (O4 = A)

Kleinster realer Adapter (`device_platform_esp_idf`): besitzt drei `EspIdfBinaryOutputSink`
(Enable, RPWM, LPWM) und eine `SharedEnableBridgeSink`; implementiert
`IBidirectionalActuatorSink` durch Delegation; Konstruktor nimmt Pins und `OutputPolarity`
(kein Hardwarezugriff im Konstruktor). `begin()` ist die einzige Initialisierung:

```text
1. Enable.begin()   // GPIO25: inaktiv LOW vorsetzen, dann Ausgang, keine Pulls
2. RPWM.begin()     // GPIO13
3. LPWM.begin()     // GPIO14   (alle drei werden immer versucht, in dieser Reihenfolge)
4. alle drei == Ready ->  Bridge.begin() (All-off-Pruefung, 3.3) -> Ready
   sonst -> Bridge.begin() wird NICHT aufgerufen (Bruecke bleibt NotStarted: jedes EIN
            wird verworfen); begrenzt je Sink einmal setEnabled(false) (best effort,
            Ergebnis ignoriert, hebt keine Verriegelung auf); Rueckgabe Failed
```

`begin()` liefert `Ready`/`Failed`. Bei `Unconfirmed` eines Sinks (TBD_HARDWARE) ist der
Adapter inert. Auch ein `Ready` ist **keine** Aussage ueber Modulpolaritaet oder
Boot-/Reset-Hardwareverhalten; die LOW-Pegel sind der SSOT-Designzustand.

`main/app_main.cpp` konstruiert den Adapter und ruft `begin()` beim Boot im normalen
`app_main()` (nach den #32-Ausgaengen, vor NVS); er wird nicht an Driver/Planner angeschlossen,
`setForward`/`setReverse` werden in `main/` nicht aufgerufen (Quelltext-Pruefung als Test).
Damit bleibt der Peltier im Produkt unerreichbar.

### 3.5 Generator

`scripts/generate_board_profile_header.py` wird um `bts7960_rpwm`, `bts7960_lpwm`,
`shared_r_en_l_en` erweitert (`kBtsRpwmPin`=13, `kBtsLpwmPin`=14, `kBtsEnablePin`=25 plus
je `OutputPolarity` aus `active_level`, heute `high` → `ActiveHigh`). GPIO34/35 werden nicht
erzeugt (Selftest-Fall). Alle bisherigen Konstanten bleiben unveraendert; Pinquelle bleibt
ausschliesslich die SSOT.

## 4. Umsetzungsschnitte (je ein Commit, danach Stopp zur Ownerpruefung)

1. **S1 Portvertrag + Driver-Sperre:** Port, `EspIdfBinaryOutputSink`, `MockBinaryOutputSink`
   (+ Ablehnungsschalter), `AllOffBinarySink`, `TracingBinarySink`, Driver (3.2); Tests und
   Hosttest angepasst/erweitert (Abschnitt 5).
2. **S2 `SharedEnableBridgeSink`:** Klasse + native Tests (gate-gefuehrt).
3. **S3 `EspIdfSharedEnableBridge`:** realer Adapter, Boot-Initialisierung, Linux-CMock-
   Hosttest (`test/esp_idf_shared_enable_bridge_host`).
4. **S4 Generator, Header, Composition Root, Doku:** Rollen/Selftest/Header, `app_main`
   (ohne Planner-Anbindung), `ACCEPTANCE_TESTS.md` (SIM-33-xx, HW-33-xx `NOT_RUN`),
   `ARCHITECTURE.md`, ROADMAP.
5. **S5 Builder-Self-Check** (`run_pre_ready_gates.sh self-check`), Build beider ESP-IDF-
   Profile, Beweisdokumentation (nur tatsaechlich Ausgefuehrtes mit exaktem HEAD).

## 5. Tests (software, deterministisch, hardwarefrei)

- **S1:** `test_actuator_plan_sink_driver`: verworfenes Aussenluefter-EIN bei Heizen und
  Kuehlen → beide Peltierrichtungen bleiben AUS, Befehlsreihenfolge im gemeinsamen Trace;
  uebrige Faelle unveraendert; `Idle`-Nachlauf unveraendert. `test_sensor_actuator_mocks`,
  `test_configuration_service`, `test_run_persistence_coordinator`, `test_factory_reset_flow`
  (Guard-Text) laufen als Konsumenten-Regression mit dem geaenderten Mock; `AllOffBinarySink`
  meldet EIN nie als angewendet (Test im Bring-up-Probe-Kontext oder Trace-Test);
  `esp_idf_binary_output_sink_host`: Rueckgabe `true/false` je Pfad der Tabelle 3.1.
- **S2 `test_shared_enable_bridge_sink`** (nativ, ablaufprotokollierende Testdoppel mit
  Ablehnungsschalter je Sink): `begin()` mit allen drei bereiten Sinks → Ready und genau die
  AUS-Folge Enable, Vorwaerts, Rueckwaerts; **`begin()` mit nicht initialisierten Sinks
  (`setEnabled(false)` → `false`) → Faulted, nicht Ready** (je Sink einzeln); Fehler an
  jeder `begin()`-Stufe (Enable, Vorwaerts, Rueckwaerts) → shutdown-Folge + Faulted;
  EIN-Sequenz (Schenkel vor Enable) und AUS-Sequenz (Enable vor Schenkel); Mutual
  Exclusion (Konflikt → Abschaltung + Verriegelung, nie beide Schenkel EIN in der
  Befehlsfolge); Break-before-make (Richtungswechsel nur ueber All-off, auch bei
  unmittelbar folgendem Gegenrichtungsbefehl); Fehler an jedem Teilbefehl bei EIN und bei
  AUS → shutdown, Faulted, keine Reaktivierung; Faulted-AUS = Best-effort ohne Entriegelung;
  Aufrufe vor `begin()`, doppeltes `begin()`; Shutdown unabhaengig von Teilfehlern.
- **S3 `esp_idf_shared_enable_bridge_host`** (Linux-CMock): GPIO-Befehlsreihenfolge der
  Initialisierung exakt Enable(25) → RPWM(13) → LPWM(14), je inaktiv vorgesetzt dann
  Ausgang; Fehler an **jeder** der drei `begin()`-Stufen → die uebrigen Stufen werden
  trotzdem versucht, `Bridge.begin()` wird nicht aufgerufen (nachgewiesen: `setForward(true)`
  danach erzeugt keinen GPIO-Aufruf auf RPWM/Enable), je Sink genau ein Best-effort-AUS,
  Rueckgabe `Failed`; Erfolgsfall → Ready und EIN/AUS-Sequenz am GPIO.
- **S4:** Generator `--selftest`/`--check` (RPWM/LPWM/EN, keine GPIO34/35, bisherige
  Konstanten unveraendert); Quelltext-Pruefung: kein `setForward(`/`setReverse(` in `main/`
  ausser der Bring-up-Probe; `check_architecture_boundaries.py` (Bruecke rollen- und
  ESP-frei).
- **Regression durchgehend:** `test_actuator_planner` (Nachlauf, Mindest-Auszeit, Totzeit),
  `test_actuation_interlock`, native Orchestrator-/Driver-Tests, beide ESP-IDF-Profile,
  Self-Check. Hardware bleibt `NOT_RUN`.

## 6. Spaetere Hardware-Gates (nicht Teil dieses Software-PR)

Vor Abschluss von #33 bzw. jeder realen Peltieraktivierung:

- **H1** konkrete IBT-2-/BTS7960-Modulvariante, Eingangsbuffer, Logikschwellen,
  3,3-V-HIGH-Kompatibilitaet, Versorgung/gemeinsame Masse.
- **H2** Enable-Funktion und reale Polaritaet (R_EN/L_EN gemeinsam); Pulldowns (RPWM, LPWM,
  EN) real vorhanden — funktional, keine neue Pegelmessung.
- **H3** Boot/Reset/Brownout/Bootloader: Richtungen und Enable bleiben aus (Verhalten von
  GPIO13/14/25 waehrend Reset/Boot gegen Datenblatt/TRM pruefen und funktional bestaetigen).
  Die bei Boot gesetzten LOW-Pegel sind dafuer kein Nachweis.
- **H4** unbelasteter Adapterpfad; Sicherung, einmalige Temperatursicherung, Kuehlkoerper,
  funktionsgepruefter Aussenluefter, Pflichtsensoren vor realem Peltier.
- **H5** begrenzte Heiz-/Kuehl-Servicepulse im geschuetzten Service-/Bring-up-Modus,
  Abbruch, Nachlauf, reale Mindest-Auszeit/Totzeit (Werte `TBD_COMMISSIONING`),
  Sensorentfernung → Sicherheitsreaktion.
- **H6** Release-Kette ausserhalb von #33: Aktor-Gate `Allowed`, Planner-/Regelschleifen-
  Komposition (#35/#90), Luefterfehlermeldung/-klassifikation nach oben. Die #32-
  Hardwareverifikation bleibt Voraussetzung fuer die Luefterkanaele.

Bestehende waivierte elektrische Pegelmessungen werden nicht zur neuen Pflicht.

## 7. Ownerentscheidungen (entschieden) und Risiken

Entschieden und in diesen Plan integriert: **O1 = A** (`setEnabled` → `bool`, Semantik 3.1),
**O2 = bestehender Vertrag** (Reihenfolge am Adapter, Zeit im Planner), **O3** (`Faulted`
bis Neustart, kein Re-Arm), **O4 = A** (Boot-Initialisierung LOW in Reihenfolge Enable,
RPWM, LPWM; Bridge erst nach drei erfolgreichen Sink-`begin()`; kein Planner-Anschluss).
Keine weiteren offenen Ownerentscheidungen.

Risiken: Die `bool`-Aenderung beruehrt den frisch gemergten #32-Pfad (Mock, Adapter,
Probe, Tests). „Angewendet“ ist keine physische Bestaetigung der Last. Die zeitliche
Mindest-Auszeit liegt ausschliesslich im Planner. Die Linux-Hosttests (`esp_idf_*_host`) sind
nicht gate-gefuehrt; die Bruecken-Logik (S2) ist es (nativ).
