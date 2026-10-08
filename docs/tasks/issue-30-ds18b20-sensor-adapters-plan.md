# Plan Issue #30 – reale DS18B20-Sensoradapter

```text
PLAN_REVISION=4 (konsolidiert; Softwarepfad vor Hardware, O1-O5 als Entscheidblock)
PLAN_STATUS=DRAFT_AWAITING_OWNER_APPROVAL (exakte Plan-SHA steht im Draft-PR)
SUPERSEDES=Revision 3 (freigegebener Plan-Commit de6d2a0b3c77e1da13c2f830ba5865e11784185b)
ISSUE=30 (E5.2)
BASE_MAIN=7b16dbeb95ab09fdbe13d6c524c7a09ec721fb7d (PR #187 gemergt)
TOOLCHAIN=ESP-IDF v6.1 (fff9895c82d744c7237be8847347bdd1b07c6643)
S0_STAGE_1=DONE (PASS_BUILD_GATE, unveraendert: docs/audits/ISSUE30_S0_STAGE1_EVIDENCE.md)
SOFTWARE_PATH=HARDWARE_FREE_IN_PR_189
HARDWARE_GATE_H=NOT_RUN_DEFERRED (Abschnitt 6)
ACTUATOR_RELEASE=NO
IMPLEMENTATION=NOT_STARTED
```

## 0. Owner-Entscheidblock (vor Implementierung ausdruecklich zu bestaetigen)

Die bisher offenen Entscheidungen O1–O4 sind **nicht** stillschweigend entschieden;
die Empfehlungen gelten erst mit der Ownerbestaetigung, zusammen mit der exakten
Plan-SHA dieser Revision.

| ID | Entscheidung | Empfehlung | Wirkung bei Abweichung |
|---|---|---|---|
| O1 | Persistierter Datensatz (ROM, Rolle, Offset je ROM; ohne Referenzmessdaten, siehe 5.3) und Boot-Bindung in #30 | **ja** | "Verschieben" ist eine Scope-/Akzeptanzaenderung: Boot-Identitaet und ROM-Offsets wandern in ein Folgeissue, feste Rollen bleiben im Produkt ungebunden (kein `Ok`), #30 erreicht seine DoD nicht |
| O2 | Schreibpfad nur im `esp32_bringup` (UART-Harness hinter Compile-Option, Release `FATAL_ERROR`), Lesen auch im Release | **ja** | anderer Schreibpfad = Planrevision |
| O3 | Produktbus: jedes **einzelne** gueltige ROM akzeptieren; Offsets nur bei bekanntem ROM (hoechstens 4 bekannte Produktfuehler) | **ja** | Neuzuordnung je Fuehler = Planrevision |
| O4 | Ein Sampling-Task fuer beide Busse; Stack/Prioritaet/Core/Heap erst nach Hardwaremessung. Bis dahin ist der Task budget-gesperrt (wird nicht erzeugt, Kanaele bleiben `MissingSample`); keine erfundenen Budgets | **ja** | anderer Task-Zuschnitt = Planrevision |
| O5 | Verbleib der Hardware-Restarbeit (Abschnitt 6) in #30 oder eigenes Hardware-Issue mit explizitem Transfer der Akzeptanzkriterien; Entscheid **vor dem Software-Merge**, kein Issue wird automatisch angelegt | #30 beibehalten (`Refs #30`), bis der Owner anders entscheidet | getrennt = Owner legt Issue und Kriterien-Transfer fest |


## 1. Ziel und Nicht-Ziele

**Ziel.** Die drei DS18B20 des R1-Aufbaus werden ueber einen schmalen ESP-IDF-
Adapter als `device_platform::ITemperatureSource` (je Rolle eine Instanz) in den
bestehenden Sensorkern (#20) eingespeist (Softwarepfad hardwarefrei, reale Verifikation im spaeteren Hardware-Gate H): ROM-ID, CRC, 12-Bit-Messung etwa alle
zwei Sekunden ohne Blockieren der Hauptschleife, Hot-Plug des Produktfuehlers,
Wiedererkennung, typisierte Fehler und ROM-bezogene Offsets. Die verbindliche
R1-Zwei-Bus-Topologie aus dem Boardprofil wird unveraendert uebernommen.

**Scope-Entscheid (Owner, Revision 4).** Alle ohne reale Hardware sinnvoll implementierbaren und pruefbaren Anteile von #30 werden im bestehenden PR #189 fertiggestellt; reale Hardwaretests sind ausdruecklich verschoben (Abschnitt 6). Kein zweiter Software-PR. Der PR darf #30 wegen des offenen Hardware-Kriteriums nicht schliessen (`Refs #30`).

**Nicht-Ziele.** Keine Topologieentscheidung, keine Aktor-/Peltier-/PI-Arbeit
(#32/#33/#34/#35), keine Aenderung der Safety-/Interlock-Logik (#24), keine
Service-/PIN-/Offset-Bedienoberflaeche (#28), kein neuer Abnahmeumfang fuer die
bei PR #187 akzeptierten `NOT_RUN`-UI-Tests, kein Fix von Issue #188. Der Adapter
trifft keine Rollenprioritaet, keine `VALID/STALE/FAILED`-Entscheidung und keine
Peltierfreigabe.

## 2. Verifizierte Ausgangslage

| Thema | Befund | Quelle |
|---|---|---|
| Topologie (SSOT) | GPIO32 `one_wire_internal` = Schrankluft + Kuehlkoerper (Multidrop); GPIO33 `one_wire_product` = abnehmbarer Produktfuehler; je Bus 3-Leiter, Pull-up 4,7 kOhm nach 3,3 V am Receiver/Bus. Status `planned`, `board_revision: TBD_HARDWARE`. Beide GPIOs sind keine Boot-Strap-/Flash-Pins. | `config/board_profiles/esp32_32e_quad_mosfet_r1.yaml` |
| Hardwarestatus | Verdrahtung, Pull-ups, Kabel, Sensoranzahl und ROM-IDs sind im Repo **nicht** als vorhanden belegt (`OPEN_POINTS`: ROM-Adressen offen). | `docs/HARDWARE.md`, `docs/OPEN_POINTS.md` |
| Toolchain | ESP-IDF v6.1 ist fixierte Produktionsbasis (Issue #159). Die Komponenten-Audits nennen noch `6.0.2`; diese Aussagen sind historisch und **keine** v6.1-Verifikation. | `docs/ESP_IDF_UPGRADE_CONTRACT.md`, `docs/audits/COMPONENT_EVALUATIONS.md` |
| Sensorkern | `ITemperatureSource::read() const` liefert `TemperatureReading` (Status `Ok/BusFault/CrcFault/MissingSample/KnownInvalidMeasurement`, optionale `SensorIdentity` = ROM, monotoner Zeitstempel). `SensorQualityPipeline` (#20) nimmt Proben je Rolle auf, `SensorCalibration` bindet Identitaet + `SensorOffset`. Der Port nennt den DS18B20-Adapter aus #30 ausdruecklich als vorgesehenen Produzenten. | `lib/device_platform/src/temperature_source.hpp`, `sensor_quality_pipeline.hpp` |
| Konsumenten | Kein Produktcode instanziiert eine `ITemperatureSource` oder die Pipeline; die Komposition folgt mit #35. Nur `MockTemperatureSource` (Testsupport) existiert. | `rg ITemperatureSource lib main` |
| Persistenz | `ServiceConfiguration` (Schema 2, Payload max. 81 B) enthaelt nur `actuatorPlannerParameters`. Es gibt **keinen** persistierten Speicher fuer ROM-Rollenzuordnung oder Offsets je ROM. Praezedenz fuer bring-up-only Provisionierung: Touch-Kalibrierung (`APP_ISSUE_31_TOUCH_CALIBRATION_PROVISIONER`). | `lib/fermentation_app/src/configuration_documents.hpp`, `configuration_limits.hpp`, `main/CMakeLists.txt` |
| S0 Stufe 1 | Erledigt: `onewire_bus 1.1.1`/`1.1.2` + `ds18b20 0.4.0` bauen auf ESP-IDF v6.1 (`PASS_BUILD_GATE`, reproduziert); Image +23,8 kB, IRAM +2,3 kB; DallasTemperature/OneWire `REQUIRES_UNAPPROVED_FRAMEWORK_CHANGE` (Papier-Check). | `docs/audits/ISSUE30_S0_STAGE1_EVIDENCE.md` |
| Bedienpfad | Service/PIN sind bis #28 zurueckgestellt (`service.available` hat keinen Producer, siehe PR #187). | `docs/ROADMAP.md`, PR #187 |
| Ressourcen | Produktbasis `3918c50`: stabil Home/WLAN 44'752 B frei (Minimum 40'176 B, groesster Block 43'008 B), LVGL aktiv. Task-Stack, RMT-Puffer und Heap des Adapters sind **nicht** bekannt (`TBD_IMPLEMENTATION_BUDGET`). | `docs/audits/PR187_HW_SMOKE_3918C50_20261007_EVIDENCE.md` |
| Registry (live, 2026-10-08) | `espressif/onewire_bus`: `1.1.2` (2026-09-15, Apache-2.0, `idf >=5.0`), `1.1.1` (2026-05-11). `espressif/ds18b20`: `0.4.0` (2026-05-29, Apache-2.0, haengt von `onewire_bus ^1.1.0` ab; optional `sensor_hub` nur bei `DS18B20_SENSOR_HUB`, Default `n`). Die Audits fuehren `1.1.1`; `1.1.2` ist neuer. | `components.espressif.com` API |

## 3. Technische Befunde (aus dem Quelltext von onewire_bus 1.1.2 / ds18b20 0.4.0)

Die Komponenten wurden nur gelesen (nicht in das Repo uebernommen):

1. **Blockierend.** `ds18b20_trigger_temperature_conversion(_for_all)` ruft
   `vTaskDelay` (100/200/400/800 ms je Aufloesung; `_for_all` fest 800 ms). Alle
   Busoperationen blockieren den aufrufenden Task (RMT-Warteschlange bis 1000 ms,
   Bus-Mutex mit `portMAX_DELAY`). Folge: **nie aus der Hauptschleife**; "ohne
   Blockieren" ist nur mit einem eigenen Sampling-Task zu erfuellen.
2. **Lesen.** `ds18b20_get_temperature` prueft das Scratchpad per CRC8
   (`ESP_ERR_INVALID_CRC`) und erkennt den Einschaltwert 85,0 °C
   (`ESP_ERR_INVALID_STATE`); Ergebnis `float` (Aufloesung 1/16 °C, exakt in
   `double`). Es prueft die Aufloesung nicht, deshalb setzt der Adapter sie nach
   jeder (Wieder-)Erkennung explizit.
3. **Aufloesung.** `ds18b20_set_resolution` schreibt nur das Scratchpad, **ohne**
   EEPROM-Kopie (kein Verschleiss); Power-on-Default ist 12 Bit.
4. **Anwesenheit.** `onewire_bus_reset` liefert `ESP_ERR_NOT_FOUND` (kein
   Presence-Puls) getrennt von `ESP_ERR_TIMEOUT`; das ist die vorhandene
   Primitive, um "Produktfuehler abwesend" von "Busfehler" zu unterscheiden.
5. **Enumeration.** `onewire_new_device_iter` liefert ROM-Adressen mit CRC; die
   Version `1.1.2` behebt die ROM-Suche (1-basierte Bitnummerierung), verlaengert
   `tRSTH` auf 480 µs und lehnt ungueltige GPIO-/Puffergroessen ab.
6. **Heap.** Je Bus werden RMT-TX/RX-Kanal, Encoder, Mutex und ein
   `max_rx_bytes * 8 * sizeof(rmt_symbol_word_t)`-Puffer allokiert; je Sensor ein
   `calloc`-Handle. Zwei Busse belegen vier RMT-Kanaele (ESP32: acht).

## 4. Zielarchitektur (Vorzugsloesung)

**Software.** Espressif-first: `espressif/onewire_bus` (RMT-Backend, UART-Backend
nur Rueckfall) plus `espressif/ds18b20`, konkrete Version: Arbeitsannahme
`onewire_bus 1.1.2` (S0 Stufe 1, Befund 5), endgueltig im Hardware-Gate H. Keine Eigenimplementierung
des 1-Wire-Protokolls, keine Sensor-Abstraktionshierarchie.
DallasTemperature/OneWire (Arduino-Laufzeit) ist per Papier-Check als
`REQUIRES_UNAPPROVED_FRAMEWORK_CHANGE` bestaetigt (S0 Stufe 1) und wird nur bei einem
nachgewiesenen Misserfolg des Espressif-Pfads vertieft.

**Schichten (ADR-013).**

| Baustein | Modul | Inhalt |
|---|---|---|
| `IDs18b20Bus` (technischer Port, 4–5 Methoden) | `device_platform` | `presence()`, `enumerate(out ROMs)`, `startConversionAll()`, `readScratchpad(rom) -> {celsius, DriverResult}`, `setResolution12(rom)`; `DriverResult` = `Ok/NotFound/Timeout/InvalidCrc/PowerOnValue/Other`. Keine IDF-Typen, kein Rollenbegriff (Kanalindex). |
| `Ds18b20SamplingEngine` | `device_platform` | Reiner Zyklus-/Zustandsautomat: 2-s-Takt, Trigger -> Wartezeit >= Konvertierungszeit -> Lesen je gebundenem ROM (MATCH ROM), Bindungspruefung nach Abschnitt 4a, Mapping `DriverResult` -> `TemperatureSampleStatus`, begrenzte Neuinitialisierung nach Fehlern. Keine eigene Anzahl guter Proben, keine eigene `VALID`-Schwelle, keine Presence-Entprellung: jeder Zyklus meldet das tatsaechliche technische Ergebnis. Veroeffentlicht je Kanal genau ein unveraenderliches `TemperatureReading` mit dem Zeitstempel der abgeschlossenen Konvertierung; ohne neue Konvertierung wird **keine** alte Messung erneut als neu ausgegeben. |
| `Ds18b20ChannelSource` | `device_platform` | `ITemperatureSource`-Sicht auf ein Engine-Ergebnis (`read() const`, kopiert den letzten Stand unter kurzem kritischen Abschnitt). |
| ESP-IDF-Adapter | `device_platform_esp_idf` | `IDs18b20Bus`-Implementierung auf `onewire_bus`+`ds18b20`, Sampling-Task (ein Task fuer beide Busse, TWDT-ueberwacht), Buserzeugung auf GPIO32/33 aus dem generierten Boardprofil-Header. |
| Fake-Bus | `device_platform_test_support` | Skriptbarer `IDs18b20Bus` fuer die native Matrix. |
| Komposition | `main/app_main.cpp` | Nur Instanziierung/Start des Adapters und Bereitstellung der Quellen. **Keine** Anbindung an Regelung/Safety/Peltier (kommt mit #35). |

Die Aufteilung in Port + Engine + duenner Adapter ist die kleinste Form, in der
Mehrbus-/Hot-Plug-/Fehlerlogik **nativ** pruefbar ist (AGENTS: Adapter "gegen
ihre portseitigen Vertraege testbar"); der Port hat genau zwei Implementierungen
(ESP-IDF, Fake) und waechst nicht ueber den benoetigten Vertrag hinaus.

**Fehlermapping (Engine).**

| Treiber/Bus | `TemperatureSampleStatus` | Bedeutung |
|---|---|---|
| `Ok` | `Ok` | `identity` = ROM, `celsius` mit Offset **nicht** angewandt (Offset bleibt in #20) |
| jedes andere Ergebnis | siehe Zeilen unten | `identity` immer nach 4b (nie nur wegen eines Fehlers verworfen) |
| `NotFound` (kein Presence-Puls) | `MissingSample`; `identity` nach 4b: erwartetes ROM (feste Rolle), zuletzt gesehenes ROM (Produktkanal), nur ohne jede Kenntnis leer | Sensor/Bus ohne Teilnehmer; feste Rollen: Fehler, Produktrolle: optional abwesend |
| `Timeout`, sonstige Busfehler | `BusFault` | Bus elektrisch/zeitlich gestoert |
| `InvalidCrc` | `CrcFault` | Scratchpad-/ROM-CRC falsch |
| `PowerOnValue` (85,0 °C) | `KnownInvalidMeasurement` | Einschaltwert, nie als Messwert |
| unerwartetes/unbekanntes ROM auf festem Bus | `MissingSample` fuer die gebundene Rolle (fail-closed), ROM nur im Diagnosebericht | keine stille Rollenuebernahme |

"Fehlender optionaler Produktfuehler unterscheidbar von Fehler" folgt daraus ohne
Aenderung des bestehenden Vertrags: `MissingSample` ohne `BusFault`/`CrcFault`
(abwesend) gegenueber `BusFault`/`CrcFault` (Fehler). Die fachliche Auslegung
(Rolle fest/optional) bleibt im Konsumenten.

**Sampling-Task.** Ein Task; Prioritaet/Stack/Core werden **nach Hardwaremessung (Gate H)**
vom Owner freigegeben, nicht geraten (`TBD_IMPLEMENTATION_BUDGET` ist nie Laufzeitwert);
bis dahin ist der Task budget-gesperrt (5.4). Start erst nach
Plattform-/Application-Begin; Ende/Fehler des Tasks fuehrt zu `MissingSample`
statt zu altem Wert (Zeitstempel wird nicht fortgeschrieben). Hot-Plug und
Wiedererkennung laufen vollstaendig im Task und sind rein technisch:
Presence-Poll des Produktbusses im selben Takt, bei Wiederkehr Enumeration,
ROM-Pruefung nach Abschnitt 4a, 12 Bit neu setzen, frische Konvertierung mit
CRC-Pruefung. Die **erste** frische, korrekt konvertierte, CRC-gueltige Probe
wird als `Ok` gemeldet. Die fachliche Recovery-Entscheidung (Zaehler
`minConsecutiveValidSamples`, `minRecoveryStabilityDurationMs`, Plausibilitaet,
ROM-Wechsel-Erkennung `IdentityMismatch`) liegt ausschliesslich in
`SensorQualityPipeline` (#20); der Adapter fuehrt dafuer keine zweite
konfigurierbare Schwelle ein.

### 4a Rollenbindung und Aktivierung (fail-closed)

Auf GPIO32 liegen zwei feste Sensoren; ohne verbindliche Zuordnung `ROM -> Rolle`
ist Schrankluft nicht von Kuehlkoerper unterscheidbar. Daher gilt, unabhaengig von
der Umsetzungsreihenfolge:

1. **Einzige Quelle** der Bindung fester Rollen ist der persistierte Datensatz
   (Rolle, ROM, Offset; C2, Abschnitt 5.3). Verboten sind: Enumerationsreihenfolge, "erstes ROM =
   Luft", ROM-Konstanten im Code, eine temporaere Bring-up-Zuordnung im
   Produktpfad.
2. **Bootablauf:** (a) Datensatz lesen und validieren, (b) beide Busse
   enumerieren (ROM-CRC), (c) Bindung pruefen, (d) erst danach eine Rolle
   aktivieren.
3. **Ungebunden = nie `Ok`.** Ein fester Kanal veroeffentlicht ausschliesslich
   `MissingSample` ohne Identitaet (es gibt keine erwartete Identitaet), solange
   keine gueltige Bindung vorliegt (4b regelt die Identitaet bei gueltiger Bindung).
   Das gilt bei: leerem oder unlesbarem Datensatz, ungueltigem Schema/ungueltiger
   Revision, fehlender Rolle, Null-ROM, demselben ROM in zwei Rollen, ungueltigem
   ROM-CRC.
4. **Fester Bus, Laufzeit/Boot:**
   * erwartetes ROM fehlt (nie gefunden, verschwunden, abgezogen) -> genau diese
     Rolle `MissingSample` bzw. das Treiberergebnis (`BusFault`/`CrcFault`), jeweils
     mit der erwarteten Identitaet (4b);
     Wiederkehr desselben ROM liest die Rolle wieder (die Recovery-Entscheidung
     trifft #20);
   * **unbekanntes zusaetzliches ROM** auf dem festen Bus -> `BindingConflict`:
     beide festen Rollen `MissingSample`, bis die Enumeration wieder genau dem
     Datensatz entspricht oder der Datensatz bewusst geaendert wurde;
   * ein ROM kommt in der Enumeration eines Busses nur einmal vor; Duplikate gibt
     es nur im Datensatz (Punkt 3).
5. **Aktivierungszeitpunkt.** Eine Rolle liefert erst dann ihr erstes `Ok`, wenn
   Datensatz gueltig, erwartetes ROM bestaetigt, kein `BindingConflict`, 12 Bit
   gesetzt und eine frische CRC-gueltige Konvertierung vorliegt. Der Datensatz
   wird **nur beim Boot** geladen; eine Aenderung wirkt nach einem Neustart und
   erneuter Enumeration/Pruefung (5.3).
6. **Produktbus (GPIO33, abnehmbar, ein Fuehler):** genau ein Geraet -> dessen
   ROM ist die Identitaet des Produktkanals (O3); kein Geraet -> `MissingSample`
   ohne Fehlerstatus (abwesend); **zwei oder mehr Geraete -> `BusFault`**, kein
   `Ok`, keine Auswahl nach Reihenfolge; anderer Fuehler angesteckt ->
   neue Identitaet wird gemeldet. Erkennung des ROM-Wechsels (`IdentityMismatch`,
   Filter-Reset) und Offset-Zuordnung (nur bei `calibration.identity ==
   sample.identity`, unbekanntes ROM ohne Offset) leistet die bestehende
   `SensorQualityPipeline`; der Adapter dupliziert das nicht. Damit der
   ROM-Wechsel auch **nach einer Abwesenheit** erkannt wird, tragen Nicht-Ok-Proben
   die Identitaet nach 4b. Der Produktkanal
   benoetigt keinen Datensatz fuer `Ok`; Offsets gibt es nur mit Datensatz.
7. **Gate.** Ohne gueltigen Datensatz (C2) und ohne freigegebenes Task-Budget (O4)
   kann im Produktpfad keine feste Rolle je `Ok` liefern; dieser Zustand ist
   bewusst fail-closed. Die Hardwareverifikation (Gate H) setzt beides voraus.

### 4b Identitaet bei Nicht-Ok-Proben (ROM-Wechsel nach Abwesenheit)

**Grenzfall.** `SensorQualityPipeline` erkennt `IdentityMismatch` nur, wenn die
vorherige **akzeptierte** Probe (`lastAccepted_`, gleich welchen Status) und die
neue Probe beide eine gesetzte, unterschiedliche Identitaet haben. Die Folge
`Ok(A) -> MissingSample(leer) -> Ok(B)` loest daher **keinen** Mismatch aus: der
Filterzustand von A bliebe erhalten, die Rate-Referenz ist durch die ungueltige
Probe ohnehin geloescht, und ein Offsetwechsel (A -> B) wuerde den Tiefpass von A
nur verschieben statt zu verwerfen. Das ist unzulaessig: ein neu erkannter
Produktfuehler darf keine Filter-/Offsetwerte eines frueheren ROM uebernehmen.

**Bewertete Optionen.**

| Option | Bewertung |
|---|---|
| A. Nicht-Ok-Proben tragen die zuletzt bekannte bzw. erwartete Identitaet | Nutzt den vorhandenen Vertrag (`TemperatureReading::identity` ist laut `temperature_source.hpp` unabhaengig vom Status; `Ok` verlangt nur `celsius`). Die Pipeline sieht `Missing(A) -> Ok(B)` als zwei gesetzte, verschiedene Identitaeten und verwirft den Filter. Keine Aenderung an #20, keine zweite Zustandsmaschine. **Gewaehlt.** |
| B. Bindungs-/Generationszaehler im Reading oder Adapter-seitiger Filter-Neustart | zusaetzliches Vertragsfeld bzw. zweite fachliche Recovery-Logik im Adapter; verworfen (widerspricht B2). |
| C. Pipeline-Vertrag erweitern (z. B. Vergleich gegen `lastKnownIdentity_` oder Reset-Methode) | materielle Aenderung am Sensorkern #20 mit eigener Plan-/Ownerentscheidung. **Nur Reserve**, falls die C1-Tests (5.2) mit Option A an der unveraenderten Pipeline scheitern; dann Stop und Planrevision. |

**Regel (Option A).**

* **Feste Rollen:** Die `identity` jeder Probe (auch `MissingSample`, `BusFault`,
  `CrcFault`, `KnownInvalidMeasurement`, `BindingConflict`) ist das **erwartete ROM
  der Rolle** aus dem gueltigen Datensatz. Ohne gueltige Bindung gibt es keine
  erwartete Identitaet, die Proben tragen keine (4a Punkt 3).
* **Produktkanal:** Die `identity` jeder Probe ist das **zuletzt auf diesem Kanal
  gesehene einzelne ROM** (Engine-RAM seit Boot). Sie bleibt ueber `MissingSample`,
  Bus-/CRC-Fehler, Wiederinitialisierung und "zwei oder mehr Geraete" erhalten und
  wechselt erst, wenn ein anderes einzelnes ROM gelesen wird. Leer ist sie nur,
  wenn seit dem Boot nie ein ROM auf dem Kanal gesehen wurde.
* Die Engine fuehrt dafuer genau **einen** Wert (letztes ROM) und keinen Zaehler,
  keine Schwelle und keine Recovery-Entscheidung.

**Erwartetes Pipeline-Verhalten (unveraenderte `SensorQualityPipeline`).**

| Folge | Ergebnis |
|---|---|
| `Ok(A) -> Missing(A) -> Ok(B)` | erste B-Probe = `IdentityMismatch`: Filter-Reset, Rate-Referenz geloescht, Recovery-Fortschritt 0; kein A-Filterwert, kein A-Offset (Offset nur bei `calibration.identity == B`) |
| `Ok(A) -> Missing(A) -> Ok(A)` | kein Mismatch; Filter bleibt (Qualitaet `STALE`/`FAILED` und deren Resets nach #20), Offset von A bleibt angewandt |
| `Missing(leer) -> Ok(A)` (Boot) | keine Historie, normaler Start |
| dieselben Folgen mit `BusFault`/`CrcFault`/>=2 Geraeten statt `Missing` | identisch |

**Verbindliche C1-Tests (5.2)** (Engine + Fake-Bus + **echte** `SensorQualityPipeline`
mit Kalibrierung): (1) `Ok(A)->Missing(A)->Ok(B)` je einmal mit Kalibrierung fuer A
und fuer B: `lastFaultReason == IdentityMismatch` bei der ersten B-Probe, gefilterter
Wert und `appliedOffset` ausschliesslich aus B (Vergleich mit einer frischen
Pipeline, die nur B sieht), Recovery-Fortschritt neu gestartet, Snapshot-Identitaet
B; (2) `Ok(A)->Missing(A)->Ok(A)`: kein Mismatch, Filterverlauf und angewandter
Offset gleich einer Referenzpipeline ohne Adapter; (3) Wiederholung beider mit
`BusFault`, `CrcFault`, Mehrfachgeraet; (4) Boot `Missing(leer)->Ok(A)`; (5)
Engine-Vertrag: Nicht-Ok-Proben verlieren Produkt-/Rollenidentitaet nie. Scheitert
(1) an der unveraenderten Pipeline, wird nicht lokal nachgebessert, sondern die
Umsetzung angehalten und Option C als materielle Plan-/Ownerentscheidung
vorgelegt.

**Safety-Grenze.** Die Adapterausgabe ist ausschliesslich Messdaten. Hot-Plug,
Wiedererkennung und Fehler koennen keine Aktorfreigabe erzeugen oder aufheben;
`ACTUATOR_RELEASE=NO` bleibt, die Interlock-Logik (#24) wird nicht beruehrt, und
ohne Konsumenten (#35) hat der Adapter keinen Pfad zu Aktoren.

## 5. Umsetzung

### 5.1 Stand und Reihenfolge

```text
S0 Stufe 1 (erledigt, unveraendert)  -> docs/audits/ISSUE30_S0_STAGE1_EVIDENCE.md
Softwarepfad (hardwarefrei), alle Schnitte im selben PR #189:
  C1 Port/Engine/Kanalquellen/Fake-Bus  ->  C2 Datensatz + Owner-Eintrag
  ->  C3 ESP-IDF-Adapter, Task (budget-gesperrt), Komposition, Commissioning-Harness
  ->  C4 Dokumentation, Nachweise, Ressourcendifferenz
  ->  Builder-Self-Check -> STOP fuer den unabhaengigen Review
Hardware-Gate H (spaeter, getrennt, Abschnitt 6): S0 Stufe 2/3 + Verifikation
```

Nach der Freigabe dieser Revision und der Bestaetigung von O1–O5 (Abschnitt 0) wird
der Softwarepfad ohne Zwischenstopp durchgefuehrt; Stopp nur bei einem der
Abbruchkriterien (5.6). Governance (Gates, Review, Merge) steht in
`docs/AGENT_WORKFLOW.md` und wird hier nicht wiederholt.

### 5.2 C1 – Technischer Port, Engine, Kanalquellen, Fake-Bus

Dateien: `lib/device_platform/src/ds18b20_bus.hpp`, `ds18b20_sampling_engine.{hpp,cpp}`,
`ds18b20_channel_source.{hpp,cpp}`; `lib/device_platform_test_support/src/fake_ds18b20_bus.{hpp,cpp}`.

* **Port `IDs18b20Bus`** (keine IDF-Typen): `presence() -> Ds18b20DriverResult`,
  `enumerate() -> Ds18b20Enumeration`, `startConversionAll() -> Ds18b20DriverResult`,
  `read(OneWireRom) -> Ds18b20ScratchpadRead{result, celsius}`,
  `setResolution12(OneWireRom) -> Ds18b20DriverResult`. `OneWireRom` = `uint64_t`
  (!= 0), `Ds18b20DriverResult` = `Ok/NotFound/Timeout/InvalidCrc/PowerOnValue/Other`,
  `Ds18b20Enumeration` = Ergebnis + hoechstens 4 ROMs in einem festen Array +
  `overflow`-Flag (mehr als 4 Teilnehmer = Fehlanschluss, kein Heap im Betrieb).
* **Engine `Ds18b20SamplingEngine`** (bekommt `ITimeSource&` sowie je einen Port fuer den
  festen und den Produktbus; Bindung ueber `Ds18b20Binding{chamberAir, heatsink}`
  als `optional<OneWireRom>`; Engine validiert die Bindung selbst erneut):
  `step()` fuehrt bei Faelligkeit die Busarbeit durch (**blockiert**, nur im
  Sampling-Task oder im Test aufrufen) und liefert `nextDueMillis()`. Zyklus alle
  2000 ms: Presence/Enumeration/Bindungspruefung (4a) -> `setResolution12` bei jeder
  (Wieder-)Erkennung -> `startConversionAll()` beider Busse hintereinander -> eine
  gemeinsame Wartezeit von 800 ms (Datenblatt 750 ms max.; Treiberwert, keine
  Hardware-Budgetgroesse) -> `read(rom)` je gebundenem/erkanntem ROM -> Mapping nach
  Abschnitt 4 und 4b. Produktkanal- und Festbusregeln exakt nach 4a/4b. Der
  Zeitstempel einer Probe ist der Zeitpunkt des **abgeschlossenen Lesens** nach der
  frischen Konvertierung (`ITimeSource::monotonicMillis()`); ohne neue Konvertierung
  entsteht keine neue Probe, ein Wiederholungslesen derselben Konvertierung ist
  ausgeschlossen. Keine Zaehler, keine Schwellen, keine `VALID`-Logik.
* **Kanalquellen** `Ds18b20ChannelSource` (drei Instanzen: Schrankluft, Kuehlkoerper,
  Produkt) implementieren `ITemperatureSource::read() const` und liefern den
  zuletzt veroeffentlichten Stand (kleiner Mutex/kritischer Abschnitt, Kopie). Vor
  dem ersten Engine-Schritt und bei deaktiviertem Task liefern sie
  `MissingSample` mit leerer Identitaet.
* **Diagnosebericht** `Ds18b20EnumerationReport` (je Bus: Ergebnis, ROMs, `overflow`,
  `BindingConflict`, Bindungszustand, zuletzt gesehenes Produkt-ROM) fuer das
  Commissioning und spaetere Diagnose; keine Bedienoberflaeche.
* **Fake-Bus**: skriptbare Ergebnisse je Aufruf und Zeitpunkt (Sensor 0/1/2,
  Entfernen/Wiederkehr, CRC-/Bus-/Presence-/85-°C-Fehler, ROM-Tausch).

Native Tests (Suite `test_ds18b20_sampling`, plus Pipeline-Integration):

1. **Bindung (4a):** leerer Datensatz; unlesbar/ungueltig; Null-ROM; dasselbe ROM in
   zwei Rollen; erwartetes ROM fehlt; unbekanntes Zusatz-ROM = `BindingConflict`;
   Reihenfolge-Unabhaengigkeit der Enumeration; Aktivierungszeitpunkt (erstes `Ok`
   erst nach bestaetigter Bindung, 12 Bit gesetzt und frischer CRC-gueltiger Probe);
   ungebunden = nie `Ok`.
2. **Produktbus:** 0/1/2/>=5 Geraete (`overflow`); Entfernen und Wiederkehr; ROM-Tausch;
   `Ok(A)->Missing(A)->Ok(B)` und `Ok(A)->Missing(A)->Ok(A)` mit **echter**
   `SensorQualityPipeline` einschliesslich Kalibrierung (4b: `IdentityMismatch` bei der
   ersten B-Probe, Filter-/Offset-Vergleich mit Referenzpipeline), dieselben Folgen mit
   `BusFault`/`CrcFault`/Mehrfachgeraet; Boot `Missing(leer)->Ok(A)`; Nicht-Ok-Proben
   verlieren Rollen-/Produktidentitaet nie.
3. **Fehler:** jede Zeile des Fehlermappings, 85,0 °C nie als Messwert, CRC, Timeout,
   Presence (`NotFound`), Fehler eines festen Sensors beeintraechtigt den anderen
   nicht, Busfehler des gemeinsamen festen Busses trifft beide festen Rollen.
4. **Zeit/Frische:** 2-s-Takt, gemeinsame Wartezeit, kein erneutes Ausgeben einer alten
   Probe, kein Zeitstempel ohne abgeschlossene Konvertierung, monotone Reihenfolge,
   erste frische CRC-gueltige Probe ist `Ok` (keine zweite Recovery-Schwelle).
5. **Fail-closed:** Engine nie gesteppt, Task-Ausfall (kein Fortschritt des
   Zeitstempels) -> `MissingSample` bzw. Pipeline `STALE`; Architekturgrenzen-Check
   (`scripts/check_architecture_boundaries.py`): kein Pfad zu Aktoren/Safety.

### 5.3 C2 – Persistierter Datensatz, Owner-Eintrag, Bindungsquelle

Dateien: `lib/fermentation_app/src/configuration_documents.{hpp,cpp}`,
`configuration_document_codec.{hpp,cpp}`, `configuration_limits.hpp`,
`configuration_graph_store.cpp` (Referenzpruefung), `fermentation_application.{hpp,cpp}`,
neues `sensor_commissioning.{hpp,cpp}` (Datenmodell, Validierung, Kommando-Parser),
`docs/CONFIGURATION_PERSISTENCE.md`.

* **Datenmodell.** `SensorCommissioningRecord{ optional<SensorRomOffset> chamberAir;
  optional<SensorRomOffset> heatsink; vector<SensorRomOffset> productProbes; }`,
  `SensorRomOffset{OneWireRom rom; SensorOffset offset;}` (Offset ueber das bestehende
  `SensorOffset::create`, also endlich und innerhalb der Firmwaregrenze). Gueltig nur,
  wenn **beide** festen Rollen gesetzt, alle ROMs != 0 und paarweise verschieden sind
  (auch gegen `productProbes`) und `productProbes.size() <= 4`
  (`kMaximumKnownProductProbes`, Entwurfsgrenze fuer begrenzte Payload, kein
  Hardwarebudget). Ein Teilzustand (nur eine feste Rolle) ist ungueltig. Der
  Datensatz ist `std::optional` in `ServiceConfiguration::sensorCommissioning`; leer =
  ungebunden (4a).
* **Umfang bewusst klein:** gespeichert werden ROM, Rolle (durch die Position) und
  Offset. Referenzmessgeraet, Referenztemperatur, Datum und Bedienquelle aus
  `SENSOR_TUNING_COMMISSIONING.md` gehoeren zu #34/#28 und sind **nicht** Teil von #30
  (O1 bestaetigt diese Abgrenzung); die ServiceConfiguration-Revision liefert die
  monotone Versionsinformation.
* **Wire / Schema.** `ServiceConfigurationSchema::Version3` wird aktuell
  (`kCurrentServiceConfigurationSchemaVersion = 3`). Payload = unveraenderter Schema-2-Teil
  (Optionaltag + optionale Aktorplanerparameter, 81 B max.) gefolgt vom Sensorabschnitt:
  Optionaltag (1 B); falls gesetzt: Schrankluft `uint64` ROM + `binary64` Offset (16 B),
  Kuehlkoerper (16 B), `uint8` Anzahl Produktfuehler (0..4), je Eintrag 16 B. Maximal
  81 + 1 + 16 + 16 + 1 + 4*16 = **179 Byte** (`kMaximumServiceConfigurationPayloadBytes`
  81 -> 179, abgeleitet, keine Schaetzung). Der Decoder verwirft Trailing Bytes, ungueltige
  Tags, Null-/Doppel-ROMs, Anzahl > 4 und nicht endliche/ausser-Bereich-Offsets.
  Schema 1 und 2 bleiben lesbar (Sensorabschnitt = leer) und werden beim naechsten
  Commit als Schema 3 geschrieben.
* **Graph-Store.** Die Referenz-/Semantikpruefung (`validateServiceReferenceSemantically`)
  akzeptiert neben Schema 1 nun auch gespeicherte Schema-2-Referenzen (kanonischer
  Vergleich ohne Sensorabschnitt), damit bestehende Geraete mit Schema-2-Dokument beim
  Boot weiterhin verifizieren; Commit schreibt Schema 3. Pflichttests: V1->V3 und
  V2->V3 Migration, Roundtrip, kanonische Re-Encoding-Pruefung, Kapazitaet 179 B,
  jede Decoder-Ablehnung, Neustart-Persistenz im Store, Fail-closed bei Korruption
  (Bindung leer).
* **Owner-Eintraege (Application).** `applySensorCommissioning(request,
  expectedServiceRevision)` ersetzt den Datensatz (oder loescht ihn mit `nullopt`):
  verweigert bei aktivem Programm-/Manuallauf (`NotAllowed`, vor dem Preview-Slot),
  fehlender Revision (`StateChanged`), ungueltigem Datensatz (`InvalidCandidate`);
  Preview/Commit ueber `ConfigurationService` mit den bestehenden kanonischen
  Wire-Werten (`{InternalSystem,1U}`, `{NormalEdit,1U}`), Serializer-Gate und
  Slot-Freigabe wie `applyUserSettings`. `sensorCommissioning()` liefert eine
  Kopie aus dem Runtime-Lease (leer, wenn Konfiguration nicht verfuegbar:
  fail-closed). Zusatz: `SensorCommissioningRecord::calibrationFor(rom)` gibt eine
  `SensorCalibration` fuer die spaetere Pipeline-Komposition (#35) zurueck.
* **Wirkung.** Die Bindung wird **nur beim Boot** geladen; eine Aenderung wirkt nach
  einem Neustart (kein Hot-Rebind, keine Laufzeit-Revisionspruefung).
* **Kommando-Parser** (rein, nativ getestet): `report`, `bind air=<16 Hex>
  heatsink=<16 Hex>`, `offset air|heatsink|product=<16 Hex> <ganze Milli-°C>`, `clear`;
  jede Eingabe wird auf das Datenmodell abgebildet oder abgelehnt.

Native Tests: Modellvalidierung (alle Ungueltigkeiten), Codec/Migration/Persistenz
wie oben, Application-Pfade (Erfolg, stale/fehlende Revision, aktiver Lauf,
ungueltig, Persistenzfehler, Neustart), Parser, `test_configuration_*`-Konsumenten und
bestehende Service-Config-/Planner-Parameter-Tests unveraendert gruen.

### 5.4 C3 – ESP-IDF-Adapter, Task, Komposition, Commissioning-Harness

Dateien: `lib/device_platform_esp_idf/src/ds18b20_onewire_bus.{hpp,cpp}`,
`ds18b20_sampler_task.{hpp,cpp}`, `idf_component.yml`, `CMakeLists.txt`,
`dependencies.lock`, `scripts/generate_board_profile_header.py` (+ Test) und
`main/generated/board_profile_r1.hpp`, `main/app_main.cpp`, `main/CMakeLists.txt`,
`main/Kconfig.projbuild`, `main/issue_30_sensor_commissioning_harness.{hpp,cpp}`.

* **Abhaengigkeiten** fest gepinnt: `espressif/onewire_bus` **1.1.2**, `espressif/ds18b20`
  **0.4.0** (S0-Stufe-1-Evidence; Hashes aus `dependencies.lock`);
  `PRIV_REQUIRES espressif__onewire_bus espressif__ds18b20`; `sensor_hub` bleibt
  aus. Die endgueltige Version bestaetigt Gate H; bis dahin ist 1.1.2 die dokumentierte
  Arbeitsannahme.
* **`Ds18b20OnewireBus`** (`IDs18b20Bus` auf `onewire_bus`+`ds18b20`): RMT-Backend,
  je ein Bus auf GPIO32 und GPIO33, `max_rx_bytes` nach Treiberdokumentation fuer das
  9-Byte-Scratchpad, `en_pull_up` aus (extern 4,7 kOhm laut SSOT). Trigger-**Variante B**:
  SKIP-ROM + 0x44 ueber `onewire_bus_write_bytes`, Lesen ueber `ds18b20_get_temperature`
  mit MATCH-ROM-Handle (`ds18b20_new_device_from_enumeration`); Handles werden nur
  bei (Wieder-)Erkennung erzeugt/freigegeben (ratenbegrenzt, kein Betriebs-Heap-Churn).
  Treiberfehler -> `Ds18b20DriverResult` (`ESP_ERR_NOT_FOUND`->`NotFound`,
  `ESP_ERR_TIMEOUT`->`Timeout`, `ESP_ERR_INVALID_CRC`->`InvalidCrc`,
  `ESP_ERR_INVALID_STATE` (85,0 °C)->`PowerOnValue`, sonst `Other`). Variante A
  bleibt als dokumentierter Rueckfall im Adapter austauschbar (Port unveraendert).
  Keine IDF-Typen ausserhalb dieser Dateien.
* **Pins** ausschliesslich aus der SSOT: der Generator
  `scripts/generate_board_profile_header.py` erzeugt die Konstanten
  `kOneWireInternalPin` (32) und `kOneWireProductPin` (33) aus
  `one_wire_internal`/`one_wire_product` (Test-/Selftest-Erweiterung des Generators,
  `BOARD_PROFILE_SINGLE_SOURCE`-Check bleibt gruen); keine handgepflegte Pinliste.
* **Sampling-Task.** Ein Task fuer beide Busse, der `Ds18b20SamplingEngine::step()` bei
  Faelligkeit ausfuehrt und bis `nextDueMillis()` schlaeft. Stack, Prioritaet und
  Core sind **Hardware-Budgetgroessen** (O4) und stehen in
  `std::optional<Ds18b20TaskBudget> kApprovedDs18b20TaskBudget = std::nullopt`
  (nur `device_platform_esp_idf`/Komposition). **Solange der Wert leer ist,
  erzeugt `start()` keinen Task**, die Engine wird nie gesteppt, alle Kanaele bleiben
  `MissingSample`, und der Boot protokolliert "ds18b20 sampler disabled: budget not
  approved". Der Wert wird erst im Gate H nach Messung und Ownerfreigabe gesetzt;
  kein `TBD_*` ist je Laufzeitwert und kein Budget wird geraten. (Die Messung selbst
  verwendet einen Probe-Patch mit Override, nicht den Produktpfad.)
* **Komposition (`app_main.cpp`).** Nach Application-Begin: Bindung aus
  `sensorCommissioning()` laden, Engine/Adapter/Kanalquellen aufbauen, `start()`
  aufrufen. Die Quellen werden bereitgestellt; **keine** Anbindung an Regelung,
  Safety, Interlock oder Peltier (kommt mit #35). Die Hauptschleife ruft nie eine
  blockierende Busfunktion.
* **Commissioning-Harness (O2).** Nur `esp32_bringup`: CMake-Option
  `APP_ISSUE_30_SENSOR_COMMISSIONING` (analog zu `APP_ISSUE_31_TOUCH_CALIBRATION_PROVISIONER`,
  im Release-Profil `FATAL_ERROR`), UART-Zeilenkommandos aus C2 ueber einen kleinen
  Ringpuffer (Muster `issue_90_slice7_harness`), Aufruf von
  `applySensorCommissioning`, Ausgabe von Bericht und Ergebnis. Das Release-Profil
  enthaelt weder Harness noch Schreibpfad-Aufrufer; es liest nur.
* **Builds.** `esp32_bringup` und `esp32_release` bauen (mit und ohne
  Commissioning-Option im Bring-up), Architekturgrenzen-Check, Builder-Self-Check.

### 5.5 C4 – Dokumentation, Nachweise, Ressourcendifferenz

* `docs/CONFIGURATION_PERSISTENCE.md` (Service Schema 3, Grenzen, Migration),
  `docs/SENSOR_TUNING_COMMISSIONING.md` (gespeicherte Felder, Abgrenzung zu #34),
  `docs/ACCEPTANCE_TESTS.md` (neue `SIM-30-*`-IDs mit echten Testnamen, Hardware-
  Zeilen `HW-30-*` als `NOT_RUN`), `docs/THIRD_PARTY_COMPONENTS.md` (gepinnte Versionen,
  Component-Hashes, Notices), `docs/ESP_IDF_UPGRADE_CONTRACT.md` (Komponentenliste),
  `docs/LICENSE_STATUS.md`, `docs/HARDWARE.md` (Verweis, Hardwarestatus unveraendert),
  `docs/ROADMAP.md`.
* **Ressourcen.** Statische Differenz der finalen Builds gegen Base
  (Image, `.flash.text/rodata`, `.iram0.text`, `.dram0.data/bss`) fuer `esp32_bringup`
  und `esp32_release`, dokumentiert in `docs/audits/ISSUE30_SOFTWARE_RESOURCE_DELTA.md`.
  Heap, Task-Stack, RMT-Laufzeitpuffer und Zyklusdauer bleiben **nicht gemessen**.
* **Regression.** Betroffene native Suiten (Konfiguration/Codec/Graph-Store/Service,
  Sensorkern, Application, Architektur) und die bisher moeglichen Regression Checks;
  nicht ausgefuehrte Pruefungen werden als `NOT_RUN` ausgewiesen.
* **Ehrlichkeit der Nachweise.** Native Fake-Bus-Tests heissen `SIM-30-*` und sind
  **kein** Hardwarenachweis; kein simulierter Test wird als reale Evidenz ausgegeben.

### 5.6 Abbruchkriterien (Stopp mit Befund und Planrevision)

1. Der Pipeline-Test aus 4b scheitert mit Option A an der unveraenderten
   `SensorQualityPipeline` (Option C = materielle Plan-/Ownerentscheidung).
2. Die Schema-2->3-Migration verlangt eine Aenderung der Graph-Store-/Manifest-
   Invarianten ueber die Referenzpruefung hinaus.
3. Die Espressif-API passt nicht zum Port (z. B. Variante B nicht ueber die
   oeffentliche API abbildbar) oder der Release-/Bring-up-Build scheitert an der
   Komponente.
4. Ein Befund zeigt, dass die SSOT-Annahmen (GPIO32/33, RMT-Kanaele) im Build
   widerspruechlich sind: Befund und Alternativen werden vorgelegt, keine
   Hardwareaenderung ohne Ownerfreigabe.

## 6. Hardware-Gate H (spaeter, getrennt, `NOT_RUN`)

**Hardwaregrenze.** Der Softwarepfad umfasst weder Flashen, physische Verdrahtung,
Messzyklen, Neustart-/Watchdog-/Heap-HWM-Messungen, ROM-Erfassung, Stoerversuche noch
eine Aktorfreigabe. GPIO32/33, zwei 3-Leiter-Busse und je 4,7 kOhm Pull-up nach 3,3 V
bleiben die SSOT-Ausgangsbasis und sind bei **nachgewiesenem** Problem nach
Ownerfreigabe aenderbar (eigener Boardprofil-/Plan-Scope).

**Restliste (separat abgrenzbar):**

| Nr. | Inhalt |
|---|---|
| H1 | S0 Stufe 2: ein realer Sensor (ROM, 9–12 Bit, CRC, Entfernen/Anstecken, Neustart, Konvertierung ohne Pause der Hauptschleife, keine alte Messung als neu) |
| H2 | S0 Stufe 3: Zielverdrahtung (GPIO32 zwei Sensoren, GPIO33 Produkt), Zyklusdauer beider Busse im 2-s-Takt, 1000 Zyklen, 10 Neustarts mit stabilen ROMs, Reset waehrend Konvertierung, Wiederinitialisierung |
| H3 | Messung von Task-Stack-HWM, Heap, groesstem freien Block, RMT-Kanaelen, blockierter CPU-Zeit; Ownerfreigabe von `kApprovedDs18b20TaskBudget` (aktiviert den Task) |
| H4 | Bestaetigung oder Austausch von `onewire_bus`-Version (1.1.2) und Konvertierungsvariante (B/A) |
| H5 | ROM-Erfassung ueber `report`, Zuordnung der festen Sensoren (z. B. durch gezieltes Erwaermen), Eintrag per Commissioning, ROM-Liste in `docs/audits/ISSUE30_HW_EVIDENCE_<datum>.md` und `docs/OPEN_POINTS.md` |
| H6 | Verifikationsmatrix aus #30: Sensoren einzeln abziehen, Bus stoeren (nur erlaubte Faelle), Produktfuehler hot-pluggen (Stillstand und waehrend Konvertierung), ROM-Zuordnung und Wiedererkennung, Offset-Wirkung, reale Messwerte gegen das Diagnosemodell |
| H7 | Hardware-Evidence auf dem exakten Implementierungs-Head (Aktoren physisch getrennt oder gesperrt), `logResources`-Vergleich, kein Panic/Watchdog/OOM |

Hardwarefragen an den Owner (fuer H, jetzt nicht blockierend): Anzahl und Verdrahtung der
vorhandenen DS18B20, bestueckte Pull-ups, Sensortyp und Kabellaengen, erlaubte
Stoerfaelle, ein Dauergeraet fuer 1000 Zyklen/10 Neustarts, Zuordnung per Erwaermen.

**Issue-Abschluss.** #30 darf mit offenem Hardware-Abnahmekriterium **nicht** als
vollstaendig erledigt gelten; der PR verwendet `Refs #30`, kein `Closes #30`. Ob H in
#30 verbleibt oder in ein eigenes Hardware-Issue mit explizitem Transfer der
Akzeptanzkriterien (Boot-Identitaet real, Hot-Plug real, Messwerte stimmen) uebergeht,
entscheidet der Owner vor dem Software-Merge (O5); es wird kein Issue automatisch
angelegt.

## 7. Akzeptanzkriterien -> Nachweis

| Kriterium aus #30 | Softwarepfad (`SIM-30-*`, nativ) | Hardware-Gate H |
|---|---|---|
| feste Sensoridentitaeten bei Boot geprueft | Bindungsmatrix (4a), Datensatz-Codec/Persistenz/Neustart im Store, Application-Pfad | H5/H6 an realen Sensoren |
| fehlender optionaler Produktfuehler unterscheidbar von Fehler | Mapping-Matrix, Pipeline-Folgetests (4b) | H6 Abziehen |
| Schrankluft-/Kuehlkoerpersensor fuer Peltierfreigabe erforderlich | feste Rollen nie `Ok` ohne Bindung/bei Abwesenheit; Entscheidung bleibt #24/#35, kein Aktorpfad | — |
| Hot-Plug erzeugt keine unkontrollierte Aktorfreigabe | kein Aktorpfad (Architekturcheck), `ACTUATOR_RELEASE=NO` | H6 Hot-Plug ohne Aktorverbindung |
| reale Messwerte stimmen mit Diagnosemodell ueberein | nicht softwareseitig nachweisbar | H6 |
| individuelle Offsets je ROM | Datensatz, Kalibrierung je ROM, Pipeline-Anwendung nur bei bekannter Identitaet | H6 Offset-Wirkung |
| 12 Bit etwa alle 2 s ohne Blockieren | Engine-Takt/Frische, Hauptschleife ruft keine Busfunktion | H2/H3 Zyklus-/Blockiermessung |

## 8. Risiken

1. **Blockierende Treiber** (Befund 1): nur im Task; Task-Haenger -> Zeitstempel laeuft
   nicht weiter -> Pipeline meldet `STALE`; TWDT-Anbindung gehoert zum freigegebenen
   Task-Budget (H3).
2. **Zyklusbudget:** zwei Busse mit gemeinsamer Wartezeit (Variante B) ist
   rechnerisch machbar; die reale Dauer wird in H2 gemessen.
3. **RAM:** Stack, RMT-Puffer und Handles sind ungemessen; bis H3 laeuft der Task nicht
   (budget-gesperrt), die statische Differenz wird dokumentiert. D10: keine vorsorgliche
   Optimierung.
4. **Persistenzaenderung:** Schema 2->3 beruehrt Codec, Graph-Store-Referenzpruefung und
   Migration; abgefedert durch Pflichttests und Abbruchkriterium 2.
5. **Ungebundener Zustand ist nutzlos, aber sicher:** ohne Bindung oder Task liefern die
   Kanaele nie `Ok`.
6. **Gemeinsamer fester Bus:** ein klemmender Teilnehmer stoert beide festen Sensoren
   (SSOT-Topologie, fail-closed fuer die Peltierfreigabe).
7. **Unbekannte Hardware:** Pull-ups, Kabel und ESD sind real unbekannt (H); ein
   Widerspruch zur SSOT waere ein eigener Boardprofil-Scope.
8. **Klone/Fremdsensoren:** Behandlung bleibt auf CRC + Einschaltwert +
   Pipeline-Plausibilitaet begrenzt.

## 9. Dokumentationswirkung und Abgrenzung

* In diesem Plan-PR bereits synchronisiert: `docs/audits/HARDWARE_SPIKE_PLAN.md` (Spike B,
  Tabelle "Geltende Vorgaben"), `COMPONENT_EVALUATIONS.md` (DS18B20),
  `THIRD_PARTY_COMPONENTS.md`, S0-Stufe-1-Evidence. Historisch und **nicht** angepasst
  (Stand vor PR #131): `OD-03a/OD-03b` und Topologie-A/B-Aussagen in
  `RELEASE_1_ADOPT_OR_BUILD_AUDIT.md`, `RELEASE_1_FUNCTION_MATRIX.md`,
  `OPEN_BACKLOG_CLASSIFICATION.md`; fuer #30 gilt die Boardprofil-SSOT.
* Mit dem Softwarepfad: siehe 5.5. Der Doku-Fehler SIM-26-21/65 gehoert zu Issue #188
  und wird nicht beruehrt.
* Nicht-Ziele bleiben: #32/#33/#34/#35, Safety-/Interlock (#24), Service-/PIN-/Offset-UI
  (#28), kein neues Issue ohne Ownerentscheid.
