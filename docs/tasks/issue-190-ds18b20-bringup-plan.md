# Issue #190 – drei angeschlossene DS18B20 in Betrieb nehmen (Plan, Revision 2)

Status: **Freigabe dieser Plan-Commit-SHA durch den Owner ausstehend.** Revision 2
ersetzt Revision 1 (`1d28393`) vollstaendig (Korrektur der Erstzuordnung, §4 M1).
Bis dahin
keine Implementierung, kein Flash, keine Hardwareaktion. Ausgangsbasis: `main`
`09b704e`; PR #214 (MOSFET-Ausgaenge) ist noch Draft (`c0afd62`).

Gilt nur fuer #190. `ACTUATOR_RELEASE=NO`; Planner, Regelung, Safety-Gates und
Peltier/BTS7960 bleiben unveraendert und gesperrt. Die Uebergabe der drei
`ITemperatureSource`-Kanaele an die Regelungs-/Safety-Evidenz (#35) ist ein
**eigenes, kleines Folge-Integrationsdelta** und gehoert nicht in diesen PR.

## 1. Ziel und Nicht-Ziele

Ziel: die drei bereits angeschlossenen DS18B20 mit der **vorhandenen** Firmware
zuverlaessig messen und eindeutig Rollen zuordnen; ein begruendetes,
vom Owner freigegebenes Task-Budget aus echten Messwerten ableiten; den realen
Lese-/Bindungsweg im Release-Image ohne Bring-up-Harness nachweisen.

Nicht-Ziele: kein neuer 1-Wire-Treiber, keine neue Sensorbibliothek, kein
zweites Commissioning-System, keine neue Bedienoberflaeche, keine neue
Diagnose-API/Telemetrie, keine MOSFET-Aenderung und keine erneuten
OUT1/OUT4-Pulstests (PR #214), keine pauschale Testmatrix (kein 1000-Zyklen-/
10-Neustart-Pflichtlauf), keine absichtlichen Busfehler (Kurzschluss,
Stoerungsinjektion), keine Umbelegung von GPIOs oder Rollen.

## 2. Verifizierte Ausgangslage (`origin/main` `09b704e`)

- PR #189 gemergt: Engine `Ds18b20SamplingEngine`, ESP-IDF-Adapter
  `Ds18b20OnewireBus`, `Ds18b20Sampler`, Commissioning-Datensatz und
  UART-Harness `main/issue_30_sensor_commissioning_harness.*`
  (`APP_ISSUE_30_SENSOR_COMMISSIONING`, nur `esp32_bringup`,
  `scripts/build_issue30_sensor_commissioning.py`).
- `kApprovedDs18b20TaskBudget = std::nullopt`
  (`lib/device_platform_esp_idf/src/ds18b20_sampler_task.hpp`): `start()` liefert
  `BudgetNotApproved`, erzeugt weder Task noch Bus; alle Kanaele bleiben
  `MissingSample` (fail-closed). `app_main.cpp` (~Z. 788) reicht diese Konstante
  durch.
- SSOT: GPIO32 `one_wire_internal` (Schrankluft + Kuehlkoerper, 2 ROMs), GPIO33
  `one_wire_product` (Produktfuehler, 1 ROM), je 4,7 kOhm nach 3,3 V,
  3-Leiter; Pins aus dem generierten Header (`kOneWireInternalPin/ProductPin`).
- Harness-Kommando `ds18b20 report` gibt ROMs, Presence/Enumeration und
  Bindungsstatus aus, **keine Temperaturen**; `bind air=<16hex>
  heatsink=<16hex>`, `offset …`, `clear` schreiben den Datensatz, die Bindung
  wirkt nach Neustart.
- Historische Pflichtzahlen: Issue #190 (H2: 1000 Zyklen, 10 Neustarts), Plan
  #30 §6.1 H2, `docs/ACCEPTANCE_TESTS.md` `HW-30-02`.

## 3. Umsetzungsschnitte

**C1 – Probe-Budget und kleinste lesende Bring-up-Ausgabe (ein Commit).**
- *Probe-Budget (nur Bring-up, nie Release):* in `app_main.cpp` unter
  `#ifdef APP_ISSUE_30_SENSOR_COMMISSIONING` eine lokale, als `PROBE` benannte
  Konstante `Ds18b20TaskBudget{stackBytes=6144, priority=2, coreId=1}`, die
  statt `kApprovedDs18b20TaskBudget` an `start()` geht; Boot-Log
  `ds18b20 sampler: PROBE budget (not approved for product)`. Die Werte sind
  bewusst grosszuegige **Startwerte zur Messung**, kein validiertes Budget
  (Begruendung: ausreichend Luft, damit der Stack-High-Water-Mark aussagekraeftig
  ist; Prioritaet 2 knapp ueber dem Main-Task, Core 1 abseits des WLAN-Cores).
  `kApprovedDs18b20TaskBudget` bleibt in C1 unveraendert leer; im
  Release-Pfad aendert sich nichts (per `strings`/Build-Vergleich geprueft).
- *Rein lesende Ausgabe (kein neues API):* in der bestehenden Harness-Schleife
  (`update()`) eine Zeile alle 2 s je Kanal aus `sampler.source(channel).read()`
  (Status, Celsius, ROM, Zeitstempel, Alter gegenueber `monotonicMillis`), und
  alle 10 s eine Ressourcenzeile: Stack-HWM des Tasks `ds18b20_sampler`
  (`xTaskGetHandle` + `uxTaskGetStackHighWaterMark`), freier Heap und groesster
  freier Block (`heap_caps_*`). Keine Aenderung an Parser, Engine, Adapter,
  Datensatz oder `Ds18b20Sampler`-API. **Platzierung:** `Harness::update()`
  verlaesst die UART-Abfrageschleife per `return`, sobald kein Byte vorliegt;
  die periodische 2-s-/10-s-Ausgabe wird deshalb **vor** der UART-Schleife
  (zeitgesteuert ueber `monotonicMillis`) ausgefuehrt und laeuft auch ohne
  UART-Eingabe. UART-Parser und bestehende Kommandos bleiben unveraendert.
- *Tests:* bestehende Engine-/Commissioning-Hosttests unveraendert gruen
  (direkt betroffene Konsumenten); `build_issue30_sensor_commissioning.py`,
  Release-Build (Budget leer, Log "budget not approved", keine
  `ISSUE30`-/`PROBE`-Strings), `check_architecture_boundaries.py`.

**C2 – Hardwaremessung (Code unveraendert, Evidence als Docs-Commit).**
Mit separaten Ownerfreigaben, siehe §4. Ergebnis: ROMs/Rollen, Temperaturen mit
Zeitstempeln, Ausfall-/Hot-Plug-Befunde, Ressourcenwerte, Budgetvorschlag.

**C3 – Produktbudget und Release-Nachweis (nach Ownerfreigabe der Werte).**
`kApprovedDs18b20TaskBudget` mit den vom Owner freigegebenen Werten belegen,
Doku/Acceptance aktualisieren, Release-Image bauen und flashen (eigenes Gate),
siehe §5. Ohne Freigabe bleibt das Release-Budget gesperrt.

**C4 – Doku-Abgleich der Pflichtzahlen (Docs-Commit, kann mit C1 starten).**
Ownerentscheid "keine pauschalen 1000 Zyklen / 10 Neustarts" gezielt abgleichen:

| Stelle | Aenderung |
|---|---|
| `docs/ACCEPTANCE_TESTS.md` `HW-30-02` | Text auf den reduzierten Nachweis (§4, M2–M5) umstellen; Status bleibt `NOT_RUN` bis zur Ausfuehrung, historisches `NOT_RUN` wird nicht zu `PASS` |
| Issue #190 Body | datierten Ownerentscheid-Block voranstellen (Muster der vorhandenen Statusabgleiche); H2 verweist auf den reduzierten Umfang; Originaltext bleibt darunter |
| `docs/tasks/issue-30-ds18b20-sensor-adapters-plan.md` §6.1 H2/H6 | historisch, **unveraendert**; der Abgleich steht in diesem Plan und in Acceptance/Issue |
| `docs/ROADMAP.md` | Zeile #190 |

## 4. Hardwaremessung (C2): wenige Owner-Handgriffe

Gates (jeweils unmittelbar vor Ausfuehrung, mit dem Owner am Geraet): Flash des
Bring-up-Probe-Images (App-Partition, Hash-Verifikation, kein `erase_flash`/NVS/
OTA; DEV-Backup nicht erforderlich gemaess Ownerentscheid PR #214, siehe D4);
Aktoren/BTS7960 physisch getrennt; Verdrahtung wie SSOT; Autoreset am FT232
(RTS→EN, DTR→IO0). Port waehrend der Sitzung durchgehend offen halten.

| Nr. | Schritt | Erwartung / Abbruch |
|---|---|---|
| M1 | **Erstzuordnung in drei Schritten.** Vor einer gueltigen Bindung publiziert die Engine fuer Kanal 0/1 nur `MissingSample` ohne Temperatur (`planExpectedRomBus`, `!bindingValid_`); eine Erwaermungszuordnung ist **vor** dem `bind` daher nicht beobachtbar und braucht weder Rohdaten-API noch neuen Treiber. (a) `ds18b20 report`: beide ROMs des festen Busses (GPIO32) und den Produkt-ROM (GPIO33) erfassen. (b) Die zwei festen ROMs **vorlaeufig** per `bind air=<ROM-A> heatsink=<ROM-B>` zuordnen, einmal koordiniert neu starten; diese Zuordnung ist ausdruecklich **noch nicht verifiziert**. (c) Beide Temperaturkanaele beobachten und die Sensoren einzeln vorsichtig lokal erwaermen; widerlegt die Reaktion die vorlaeufige Zuordnung, mit demselben `bind`-Kommando tauschen und einmal neu starten. Die **endgueltige** Zuordnung wird erst nach eindeutiger Temperaturreaktion dokumentiert; Report, ROMs, Zeitstempel und Reaktionen kommen in die Evidence. Produktfuehler = einziger ROM auf GPIO33 | Keine ROM geraten. Nicht 2+1 ROMs oder sporadische Fehler: Ist-Diagnose vorlegen, keine Umbelegung, keine neuen Treiber. Keine separate umfangreiche Testserie |
| M2 | Kurze zusammenhaengende Messreihe (ca. 2 min, 12 Bit, ~2-s-Takt): alle drei Temperaturen plausibel, Zeitstempel fortschreitend, keine eingefrorenen Werte | Zykluszeit aus Zeitstempeln; CRC-/Busfehler nur aus realer Logbeobachtung |
| M3 | Je **ein** fester Sensor (Luft, dann Kuehlkoerper) kurz trennen und wieder anschliessen. Beide teilen GPIO32; beim Trennen kann der gemeinsame Bus kurzzeitig **insgesamt** ausfallen | Kriterium: korrekte Fehler-/Nicht-Ok-Reaktion (kein gueltiger Altwert) und **keine vertauschte Identitaet**; ein unveraendert gueltiger zweiter Messwert waehrend der Busstoerung wird **nicht** verlangt. Wiederkehr mit gleichem ROM und richtiger Rolle |
| M4 | Produktfuehler einmal abziehen und wieder anstecken | `Missing`/Wiederkehr korrekt, kein ROM-Tausch |
| M5 | Ein koordinierter Neustart | Persistierte Bindung/Rollen identisch, ROM-Liste unveraendert |
| M6 | Ressourcenwerte aus der laufenden Messreihe (M2–M4): Stack-HWM, Heap/groesster Block vor/nach `start()`, Zyklusdauer, Main-Loop-Heartbeat-Luecken waehrend Busarbeit (Hauptschleife ruft nie Busfunktionen) | Task laeuft stabil ohne Panic/WDT; sonst enges Delta zur Entscheidung, **kein** geratener Wert |

RMT: die Belegung ergibt sich aus der Treiberkonfiguration (zwei Busse); eine
direkte IDF-Abfrage der RMT-Kanalzahl existiert nicht und wird nicht per neuer
Instrumentierung nachgebaut. Dokumentiert wird: erfolgreiche Initialisierung
beider Busse (`start()` = `Started`) und fehlerfreier Betrieb; die Kanalzahl wird
als aus der Konfiguration abgeleitet, nicht gemessen, gekennzeichnet.

Nicht Teil: 1000 Zyklen, 10 Neustarts, Reset waehrend Konvertierung als Serie,
Buskurzschluss, laengere Dauerlaeufe. Ein Einzelfall davon nur, wenn ein
konkreter technischer Befund es erfordert.

## 5. Produktbudget und Release-Nachweis (C3)

Aus M6 wird ein Vorschlag fuer `stackBytes`, `priority`, `coreId` mit
nachvollziehbarer Reserve (HWM plus Reserve, Begruendung der Prioritaet/des
Cores) zur **Ownerentscheidung** vorgelegt. Erst danach wird
`kApprovedDs18b20TaskBudget` belegt. Nachweis im Release-Image ohne Harness
(D3): Boot-Log `ds18b20 sampler: Started`, Bindung aus dem persistierten Datensatz
geladen, kein Panic/WDT, Ressourcenlog unveraendert unkritisch, fehlende/ungueltige
feste Sensoren liefern weiterhin nicht verwendbare Daten (`MissingSample`), keine
Aktorfreigabe. Temperatur-/ROM-Funktionsnachweise stammen aus dem Bring-up mit derselben
Engine und demselben Treiber; im Release-Image ohne Harness wird **kein**
eigenstaendiger Nachweis frischer Temperaturwerte behauptet, und es gibt keine
neue Release-Telemetrie. Die Sichtbarkeit kommt mit der Integration (#35).

## 6. Tests, Nachweise, Dokumentation

Lokal gezielt: direkt betroffene Hosttests (Engine/Commissioning), Builds Bring-up
mit Commissioning und Release, Architekturgrenzen. Hardware: nur M1–M6. Nicht
Ausgefuehrtes bleibt `NOT_RUN`. Evidence-Datei `docs/audits/ISSUE190_DS18B20_EVIDENCE.md`
(SHA-gebunden: Source-HEAD, Image-Hash, ROMs/Rollen, Temperaturen mit
Zeitstempeln, Ausfall-/Hot-Plug-Befunde, Ressourcenwerte, Endzustand des Boards).
Issue #190 und Acceptance-Zeilen werden nur auf tatsaechlich belegte Ergebnisse
synchronisiert, Offenes bleibt ausgewiesen. Voller Pre-Ready-Lauf erst nach
Independent Review auf Owner-Anweisung.

## 7. Abhaengigkeit zu PR #214, Risiken

- Ueberlappende Dateien mit #214: `main/app_main.cpp`, `main/CMakeLists.txt`
  (Harness-Verriegelungen), `docs/ACCEPTANCE_TESTS.md`, `docs/ROADMAP.md`. Planung
  ist unabhaengig; **vor Arbeit an diesen Dateien** nach Merge von #214 den Branch
  auf das neue `main` aktualisieren. Das Pulse-Harness (#32) und das
  Commissioning-Harness (#30) sind per CMake gegenseitig ausgeschlossen; kein
  gemeinsames Image.
- Probe-Start kann am Bus scheitern (`BusUnavailable`): dann Ist-Diagnose, nicht
  umbelegen.
- Startwerte 6144/2/1 sind nicht validiert; schlechte Reserve oder instabiler Task
  fuehrt zu einem engen Entscheidungsdelta, nicht zu einem geratenen Releasewert.
- Das Release-Image zeigt keine Temperaturen (siehe §5); das ist beabsichtigt.

## 8. Offene Owner-Entscheidungen

- **D1:** Freigabe dieser Plan-SHA.
- **D2:** Probe-Startwerte `stackBytes=6144`, `priority=2`, `coreId=1` (nur
  Bring-up, als Probe gekennzeichnet) – freigeben oder andere Werte nennen.
- **D3:** Release-Nachweis auf Task-Start, geladene Bindung und stabilen Lauf
  begrenzt (Ownervorgabe im Planreview); keine Release-Temperaturausgabe.
- **D4:** Gilt fuer das Flashen der Probe-/Release-Images in #190 dieselbe
  Ownerentscheidung "DEV-Image ohne Backup" wie in PR #214?
- **D5:** Dauer der Messreihe M2 (Vorschlag ca. 2 min).
