# Plan Issue #30 – reale DS18B20-Sensoradapter

```text
PLAN_REVISION=1
PLAN_STATUS=DRAFT_AWAITING_OWNER_APPROVAL (exakte Plan-SHA steht im Draft-PR)
ISSUE=30 (E5.2)
BASE_MAIN=7b16dbeb95ab09fdbe13d6c524c7a09ec721fb7d (PR #187 gemergt)
TOOLCHAIN=ESP-IDF v6.1 (fff9895c82d744c7237be8847347bdd1b07c6643)
ACTUATOR_RELEASE=NO
IMPLEMENTATION=NOT_STARTED
```

## 1. Ziel und Nicht-Ziele

**Ziel.** Die drei DS18B20 des R1-Aufbaus werden ueber einen schmalen ESP-IDF-
Adapter als `device_platform::ITemperatureSource` (je Rolle eine Instanz) in den
bestehenden Sensorkern (#20) eingespeist: ROM-ID, CRC, 12-Bit-Messung etwa alle
zwei Sekunden ohne Blockieren der Hauptschleife, Hot-Plug des Produktfuehlers,
Wiedererkennung, typisierte Fehler und ROM-bezogene Offsets. Die verbindliche
R1-Zwei-Bus-Topologie aus dem Boardprofil wird unveraendert uebernommen.

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
| Persistenz | `ServiceConfiguration` enthaelt nur `actuatorPlannerParameters`. Es gibt **keinen** persistierten Speicher fuer ROM-Rollenzuordnung oder Offsets je ROM. | `lib/fermentation_app/src/configuration_documents.hpp` |
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
nur Rueckfall) plus `espressif/ds18b20`, konkrete Version erst nach Spike S0
(Kandidat `onewire_bus 1.1.2`, Begruendung siehe Befund 5). Keine Eigenimplementierung
des 1-Wire-Protokolls, keine Sensor-Abstraktionshierarchie.
DallasTemperature/OneWire (Arduino-Laufzeit) wird nur dann bewertet, wenn der
Espressif-Pfad in S0 scheitert; bis dahin genuegt der Papier-Check des
Evaluationsgates (`REQUIRES_UNAPPROVED_FRAMEWORK_CHANGE` ist zu erwarten, aber im
Spike zu bestaetigen, nicht zu unterstellen).

**Schichten (ADR-013).**

| Baustein | Modul | Inhalt |
|---|---|---|
| `IDs18b20Bus` (technischer Port, 4–5 Methoden) | `device_platform` | `presence()`, `enumerate(out ROMs)`, `startConversionAll()`, `readScratchpad(rom) -> {celsius, DriverResult}`, `setResolution12(rom)`; `DriverResult` = `Ok/NotFound/Timeout/InvalidCrc/PowerOnValue/Other`. Keine IDF-Typen, kein Rollenbegriff (Kanalindex). |
| `Ds18b20SamplingEngine` | `device_platform` | Reiner Zyklus-/Zustandsautomat: 2-s-Takt, Trigger -> Wartezeit >= Konvertierungszeit -> Lesen je gebundenem ROM, ROM-Pruefung gegen Erwartung, Presence-Entprellung, Mapping `DriverResult` -> `TemperatureSampleStatus`, begrenzte Neuinitialisierung nach Fehlern. Veroeffentlicht je Kanal genau ein unveraenderliches `TemperatureReading` mit dem Zeitstempel der abgeschlossenen Konvertierung; ohne neue Konvertierung wird **keine** alte Messung erneut als neu ausgegeben. |
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
| `NotFound` (kein Presence-Puls) | `MissingSample`, `identity` leer | Sensor/Bus ohne Teilnehmer; feste Rollen: Fehler, Produktrolle: optional abwesend |
| `Timeout`, sonstige Busfehler | `BusFault` | Bus elektrisch/zeitlich gestoert |
| `InvalidCrc` | `CrcFault` | Scratchpad-/ROM-CRC falsch |
| `PowerOnValue` (85,0 °C) | `KnownInvalidMeasurement` | Einschaltwert, nie als Messwert |
| unerwartetes/unbekanntes ROM auf festem Bus | `MissingSample` fuer die gebundene Rolle (fail-closed), ROM nur im Diagnosebericht | keine stille Rollenuebernahme |

"Fehlender optionaler Produktfuehler unterscheidbar von Fehler" folgt daraus ohne
Aenderung des bestehenden Vertrags: `MissingSample` ohne `BusFault`/`CrcFault`
(abwesend) gegenueber `BusFault`/`CrcFault` (Fehler). Die fachliche Auslegung
(Rolle fest/optional) bleibt im Konsumenten.

**Sampling-Task.** Ein Task, Prioritaet/Stack/Core **werden in S0 gemessen**,
nicht geraten (`TBD_IMPLEMENTATION_BUDGET` ist nie Laufzeitwert). Start erst nach
Plattform-/Application-Begin; Ende/Fehler des Tasks fuehrt zu `MissingSample`
statt zu altem Wert (Zeitstempel wird nicht fortgeschrieben). Hot-Plug und
Wiedererkennung laufen vollstaendig im Task: Presence-Poll des Produktbusses
im selben Takt, bei Wiederkehr Enumeration, ROM-Vergleich, 12-Bit setzen, mehrere
gueltige Proben in Folge, bevor `Ok` veroeffentlicht wird (die Plausibilitaets-/
Stabilitaetsentscheidung selbst bleibt in der Pipeline #20).

**Safety-Grenze.** Die Adapterausgabe ist ausschliesslich Messdaten. Hot-Plug,
Wiedererkennung und Fehler koennen keine Aktorfreigabe erzeugen oder aufheben;
`ACTUATOR_RELEASE=NO` bleibt, die Interlock-Logik (#24) wird nicht beruehrt, und
ohne Konsumenten (#35) hat der Adapter keinen Pfad zu Aktoren.

## 5. Umsetzungs- und Commit-Schnitte

Nach jedem Schnitt: gezielte Tests, Stop und Owner-Review gemaess Workflow.

### S0 – Auswahl und Spike (kein Produktcode)

Dauerhafte Artefakte nur unter `docs/audits/` (Probe-Patch, Evidence), analog zu
den bisherigen Probes (#170, #181). Reihenfolge nach `HARDWARE_SPIKE_PLAN.md`
(Spike B), angepasst auf v6.1 und die feste Topologie:

1. **Stufe 1 (ohne Hardware).** Isolierter Build von `onewire_bus {1.1.1, 1.1.2}`
   + `ds18b20 0.4.0` im Release-Profil (`esp32_release`, ESP-IDF v6.1); Ergebnis
   je Kandidat aus dem vorhandenen Ergebnisvokabular (`PASS_BUILD_GATE`,
   `INCOMPATIBLE_WITH_CURRENT_TOOLCHAIN`, ...); Lizenz-/Notice-Pruefung gegen die
   mitgelieferten Dateien; transitive Abhaengigkeiten; Flash/statisches
   RAM/Heap (Base gegen Kandidat); Papier-Check DallasTemperature/OneWire.
2. **Stufe 2 (ein realer Sensor, Owner am Geraet).** ROM lesen, 9–12 Bit,
   wiederholt messen, CRC, Sensor entfernen/anstecken, Neustart, Konvertierung
   ohne Pause der Hauptschleife, keine alte Messung als neu.
3. **Stufe 3 (reale Zielverdrahtung, Matrix).** Nur auf der festen SSOT-Topologie
   (GPIO32 zwei Sensoren, GPIO33 Produkt); keine Topologievarianten A/B/C mehr.
   Messung: Zyklusdauer (Trigger + Wartezeit + Lesen beider Busse im 2-s-Takt),
   blockierte CPU-Zeit der Hauptschleife, Stack-HWM/Heap des Tasks, RMT-Kanaele,
   1000 Zyklen, 10 Neustarts mit stabilen ROMs, Reset waehrend Konvertierung.
4. **Entscheidung innerhalb S0.** Trigger-Variante: (A) Treiberfunktion
   `ds18b20_trigger_temperature_conversion_for_all` je Bus nacheinander
   (rechnerisch 2 x 800 ms, 0,4 s Reserve im 2-s-Takt) oder (B) Convert-Kommando
   beider Busse ueber die oeffentliche `onewire_bus_write_bytes`-API (SKIP-ROM +
   0x44) mit **einer** gemeinsamen Wartezeit, danach Lesen ueber
   `ds18b20_get_temperature`. Vorzug (B), falls der Spike sie bestaetigt; (A)
   nur, wenn die Messung die Reserve belegt. Beide nutzen ausschliesslich
   Espressif-APIs.

**Gate nach S0:** `docs/audits/ISSUE30_S0_SPIKE_EVIDENCE.md` mit Messwerten,
gewaehlter Version/Variante, Task-Budget (Stack, Prioritaet, Core) und
Ressourcenbudget als Owner-Entscheid; weicht das Ergebnis vom Plan ab (andere
Komponente, kein Espressif-Pfad, Budget nicht einhaltbar), folgt eine Planrevision
mit neuer Plan-SHA. Ohne Freigabe keine Produktimplementierung.

### S1 – Port, Engine, ESP-IDF-Adapter, native Matrix

* `device_platform`: `IDs18b20Bus`, `Ds18b20SamplingEngine`, `Ds18b20ChannelSource`
  (+ ggf. ein `DriverResult`-Mapping als freie Funktion).
* `device_platform_test_support`: skriptbarer Fake-Bus.
* `device_platform_esp_idf`: Adapter + Task; `idf_component.yml` mit **fest
  gepinnten** Versionen, `PRIV_REQUIRES`, `dependencies.lock`;
  `docs/THIRD_PARTY_COMPONENTS.md` und `docs/LICENSE_STATUS.md`/Notices,
  `docs/ESP_IDF_UPGRADE_CONTRACT.md` (Komponentenliste) aktualisieren.
* `main/app_main.cpp`: Instanziierung und Start nach Application-Begin;
  GPIO32/33 ausschliesslich aus `main/generated/board_profile_r1.hpp`
  (der Header hat heute keine OneWire-Konstanten; die Erweiterung erfolgt ueber
  den vorhandenen Generator `scripts/generate_board_profile_header.py` aus der
  YAML-SSOT, nicht von Hand).
* Native Tests (Fake-Bus): Boot-Enumeration, ROM-Erwartung, Presence/Hot-Plug,
  Entprellung, Wiedererkennung, jede Fehlermapping-Zeile, 85,0-°C-Einschaltwert,
  keine wiederholte alte Messung, unbekanntes ROM, Zyklus-Takt, Task-Ausfall,
  Zusammenspiel mit `SensorQualityPipeline` (Bus-/CRC-/Missing-Proben fuehren zu
  den erwarteten Pipeline-Zustaenden), kein Pfad zu Aktoren (Architektur-Check).
* Builder-Self-Check (`bash scripts/run_pre_ready_gates.sh self-check`) und
  `esp32_bringup`/`esp32_release`-Build; Ressourcen-Ist gegen S0-Budget.

### S2 – ROM-/Rollen-/Offset-Datensatz (**bedingt auf Ownerentscheid O1**)

Die Akzeptanzkriterien "feste Sensoridentitaeten werden bei Boot geprueft" und
"individuelle Offsets je ROM-Adresse" setzen einen persistierten Datensatz
(Rolle, ROM, Offset, Bedienquelle, Revision) voraus; heute existiert keiner.
Ist O1 = A (in #30), folgt **vor** der Umsetzung eine Planrevision 2, die den
Datensatz, seine Revision/Wire-Werte, den Application-Owner-Eintrag
(`ConfigurationService` Preview/Commit wie `applyUserSettings`), die Migration und
den Commissioning-Ausloeser (O2) vollstaendig festlegt. Dieser Plan beschreibt den
Schnitt bewusst nicht weiter, um keine ungefragte Schema-/Persistenzentscheidung
vorwegzunehmen.

### S3 – Gezielte Hardwareverifikation und ROM-Dokumentation

Auf dem exakten finalen Implementierungs-Head (`ESP_IDF_UPGRADE_CONTRACT`), Peltier/
BTS7960/Luefter/MOSFET physisch getrennt oder gesperrt: Sensoren einzeln abziehen,
Bus stoeren (Unterbruch, strombegrenzter Kurzschluss nur nach Ownerfreigabe),
Produktfuehler hot-pluggen (Stillstand und waehrend Konvertierung), ROM-
Zuordnung und Wiedererkennung, Offset-Anwendung, reale Messwerte gegen das
Diagnosemodell, Ressourcen-Log (`logResources`), kein Panic/Watchdog/OOM.
Ergebnis in `docs/audits/ISSUE30_HW_EVIDENCE_<datum>.md` mit exakt geflashtem
Commit; ROM-Liste (je Rolle) dokumentieren und in `docs/OPEN_POINTS.md`
abhaken. Ein materieller Hardwarewiderspruch zur SSOT wird nicht still geloest,
sondern als eigener Boardprofil-/Plan-Scope gemeldet.

## 6. Akzeptanzkriterien -> Nachweis

| Kriterium aus #30 | Nachweis |
|---|---|
| feste Sensoridentitaeten bei Boot geprueft | S1 native Boot-Enumeration/ROM-Erwartung; S2 Datensatz; S3 Hardware |
| fehlender optionaler Produktfuehler unterscheidbar von Fehler | S1 Mapping-Matrix (`MissingSample` ohne Busfehler vs. `BusFault`/`CrcFault`); S3 Abziehen |
| Schrankluft- und Kuehlkoerpersensor fuer Peltierfreigabe erforderlich | Adapter liefert fuer feste Rollen bei Abwesenheit/Fehler nie `Ok`; die Peltierentscheidung bleibt #24/#35 (nur Daten, kein Ersatz) |
| Hot-Plug erzeugt keine unkontrollierte Aktorfreigabe | kein Aktorpfad im Adapter (Architekturgrenzen-Check), `ACTUATOR_RELEASE=NO`, S3 Hot-Plug ohne Aktorverbindung |
| reale Messwerte stimmen mit Diagnosemodell ueberein | S3 Vergleich gegen die Pipeline-Ausgabe/Referenz (Messprotokoll, keine neuen Grenzwerte) |

Tests laut Issue (einzeln abziehen, Bus stoeren, Hot-Plug, ROM-Zuordnung,
Wiedererkennung) liegen in S1 (nativ) und S3 (real).

## 7. Risiken

1. **Blockierende Treiber** (Befund 1): nur im Task; Task-Haenger -> Zeitstempel
   laeuft nicht weiter -> Pipeline meldet `STALE`; TWDT-Ueberwachung des Tasks.
2. **Zyklusbudget:** zwei Busse nacheinander koennen den 2-s-Takt sprengen; daher
   Variante (B) mit gemeinsamer Wartezeit (S0 misst).
3. **RAM:** der Aufbau erreichte bei PR #187 erst nach einer Textpack-Korrektur
   wieder 44 kB freien Heap; Task-Stack, RMT-Puffer und Handles werden in S0
   gemessen und als Owner-Budget freigegeben (D10: keine vorsorgliche
   Optimierung, kein erfundenes Budget). Wiederholte Handle-Neuanlage bei
   Wiedererkennung ist ratenbegrenzt und wird auf Heap-Churn geprueft.
4. **Klon-/Fremdsensoren** (85-°C-Verhalten, abweichende Scratchpads): hinge vom
   realen Sensor ab (Frage H2); die Behandlung bleibt auf CRC + Einschaltwert +
   Pipeline-Plausibilitaet begrenzt.
5. **Shared Bus mit zwei Sensoren:** ein Teilnehmer, der den Bus klemmt, stoert
   beide festen Sensoren; das Boardprofil legt dies als gewollte Topologie fest
   (Fehler wirkt fuer die Peltierfreigabe fail-closed).
6. **Unbekannte Hardware:** Pull-ups, Kabellaengen und ESD sind real unbekannt;
   Ergebnis kann ein Topologie-/Boardprofil-Thema sein (eigener Scope).

## 8. Ownerfragen und -entscheide

### Hardware (nicht im Repo belegt)

* **H1.** Wie viele DS18B20 liegen **jetzt** vor, und welcher ist wie
  verdrahtet (zwei Sensoren auf GPIO32, einer auf GPIO33 mit abnehmbarem
  Anschluss)? Sind die 4,7-kOhm-Pull-ups je Bus tatsaechlich bestueckt und die
  Sensoren im 3-Leiter-Betrieb (VDD verbunden, nicht parasitaer)?
* **H2.** Typ und Herkunft der Fuehler (wasserdichte Sonden, Originalware oder
  Klone) und Kabellaengen je Strecke.
* **H3.** Welche Stoerfaelle der S3-Matrix sind erlaubt (Unterbruch ja/nein,
  strombegrenzter Kurzschluss ja/nein), und ist ein dauerhaft beobachtetes
  Geraet fuer 1000 Messzyklen/10 Neustarts verfuegbar?
* **H4.** ROM-Erfassung/Zuordnung: Ist es akzeptabel, die beiden festen Sensoren
  durch gezieltes Erwaermen des Kuehlkoerperfuehlers (Adapter meldet, welches ROM
  die Temperatur aendert) zu unterscheiden und die Zuordnung physisch zu
  beschriften?

### Entscheidungen

* **O1 (Scope).** Wird der persistierte ROM-/Rollen-/Offset-Datensatz samt
  Boot-Pruefung in #30 umgesetzt (Empfehlung, da er Akzeptanzkriterium ist) oder
  bis zu einem eigenen Issue/#34 verschoben? Bei Verschiebung wird das
  Boot-Pruef-Kriterium in #30 nur mit einer bring-up-lokalen, nicht
  persistierten Zuordnung nachgewiesen und die Abweichung im PR ausgewiesen.
* **O2 (nur bei O1 = A).** Wie wird die Zuordnung ausgeloest, solange Service/PIN
  fehlen: ausschliesslich ueber den `esp32_bringup`-Pfad (UART/Harness,
  Empfehlung) oder ueber einen minimalen lokalen Pfad? Keine Bedien-UI in #30.
* **O3.** Austausch des Produktfuehlers: darf auf dem dedizierten Produktbus
  jedes einzelne gueltige ROM uebernommen werden (Offset je ROM, unbekanntes ROM
  ohne Offset), oder ist eine erneute Zuordnung noetig (Empfehlung: jedes
  einzelne gueltige ROM akzeptieren, Offset nur bei bekanntem ROM)?
* **O4.** Freigabe eines eigenen Sampling-Tasks (Befund 1) als
  Architekturentscheid; das Budget (Stack/Prioritaet/Core/Heap) wird nach S0
  separat freigegeben.

Keine Entscheidung ist zur bereits festgelegten Zwei-Bus-Topologie, zu den
GPIOs oder zur Espressif-first-Richtung noetig.

## 9. Dokumentationswirkung

* `docs/ROADMAP.md`: Synchronisierung nach dem Merge von PR #187 (Issue #172 ist
  abgeschlossen, bisheriger Zustand "in Arbeit" war veraltet) und #30-Zeile mit
  Planstatus; kein zweites Implementierungsthema.
* Mit S0/S1: `docs/audits/COMPONENT_EVALUATIONS.md` (DS18B20-Zeilen auf
  v6.1/Messwerte), `docs/THIRD_PARTY_COMPONENTS.md` (Version, Component-Hash,
  Lizenz), Notices, `docs/ESP_IDF_UPGRADE_CONTRACT.md`; mit S3:
  `docs/ACCEPTANCE_TESTS.md` (neue SIM-IDs), `docs/OPEN_POINTS.md` (ROM-
  Adressen), Hardware-Evidence. Der vorbestehende Fehler SIM-26-21/65 gehoert zu
  Issue #188 und wird hier nicht beruehrt.

## 10. Reihenfolge, Gates, Abgrenzung

```text
Plan-Freigabe (exakte SHA) -> S0 (Spike, Owner-Hardware H1-H4) -> S0-Gate (Owner)
 -> S1 (+ Builder-Self-Check) -> Independent Review -> [O1=A: Planrevision 2 -> S2]
 -> S3 Hardwareverifikation -> finaler Pre-Ready -> Owner
```

Abhaengigkeiten #20/#21/#29 sind erledigt; #31 ist gemergt. Es wird nicht auf
#28/#35 gewartet. Keine Branch-/Merge-/Ready-Aktion durch den Agenten.
