# Plan: Issue #20 – Sensorqualitaet, Filterung und Plausibilitaet

## 1. Metadaten und Baseline-SHA

```text
Issue: #20 [E3.1] Sensorqualitaet, Filterung und Plausibilitaet implementieren
Epic: #5 (E3)
Branch: plan/issue-20-sensor-quality-filtering
Baseline main (Auftragserstellung): 611dd6257a29c82a756212408a5faff9f516d93a
Baseline main (tatsaechlich verwendet, identisch): 611dd6257a29c82a756212408a5faff9f516d93a
PLAN_ONLY: YES
IMPLEMENTATION_STARTED: NO
```

`main` war beim Start dieses Plans exakt auf dem im Auftrag genannten Stand;
kein erneuter Baseline-Wechsel war noetig.

Abschnittszuordnung zu den 21 in Abschnitt 16 des Auftrags geforderten
Pflichtinhalten (Reihenfolge hier teils zusammengefasst/umbenannt, aber
vollstaendig): 1=Abschnitt 1, 2=Abschnitt 2, 3=Abschnitt 3, 4=Abschnitt 4,
5=Abschnitt 5, 6=Abschnitt 6, 7=Abschnitt 7, 8=Abschnitt 8, 9=Abschnitt 9,
10=Abschnitt 10, 11=Abschnitt 12 (Ausgabe-/Diagnosevertrag), 12=Abschnitt 13
(Modul-/Abhaengigkeitsrichtung), 13=Abschnitt 15 (SOLID/DRY/KISS),
14=Abschnitt 14 (geplante Dateien), 15=Abschnitt 17 (Testmatrix),
16=Abschnitt 16 (Ressourcen/CI), 17=Abschnitt 18 (Risiken),
18=Abschnitt 19 (offene Ownerentscheidungen), 19=Abschnitt 20
(Implementierungsreihenfolge), 20=Abschnitt 21 (Stopbedingung),
21=Taskleiste am Dateiende. Abschnitt 11 dieser Datei
(Widerspruchs-/Plausibilitaetsbewertung) ist eine Praezisierung von
Abschnitt 8/10 des Auftrags (dort explizit als Pflichtinhalt "Zustandsmodell"
und "Verarbeitungspipeline" gefordert) und keine eigene der 21 Pflichtnummern.

## 2. Live-Issue- und Abhaengigkeitspruefung

```text
#20: state="OPEN", Status-Feld im Body="READY"
#10: state="CLOSED" (Body-Statusfeld "READY" ist redaktionell veraltet;
     massgeblich ist der GitHub-Issue-Zustand CLOSED)
#11: state="CLOSED" (Body-Statusfeld "PLANNED_SPEC_PENDING" ebenso veraltet;
     massgeblich ist CLOSED)
```

`#20` traegt keine Kommentare. Beide Abhaengigkeiten `#10` und `#11` sind ueber
den GitHub-Issue-Zustand (nicht ueber das interne Body-Statusfeld) als
abgeschlossen verifiziert. Zusaetzlich gelesen: `#5` (Epic, `OPEN`), `#21`
(`OPEN`, `PLANNED_SPEC_PENDING`), `#24` (`OPEN`, `PLANNED_SPEC_PENDING`), `#30`
(`OPEN`, `BLOCKED_HARDWARE`, bereits mit einem "Espressif-first-Kandidaten
(Sync)"-Abschnitt aus PR #88 versehen).

Keines dieser Live-Issues wird durch diesen Plan-PR veraendert.

## 3. Gelesene Quellen

```text
- AGENTS.md (Repository-Wurzel)
- lib/device_platform/AGENTS.md
- lib/device_platform_test_support/AGENTS.md
- lib/fermentation_app/AGENTS.md
- docs/SPECIFICATION_REVIEW.md
- docs/DECISIONS.md (alle 19 ADR-Titel/-Status gepruept)
- docs/ARCHITECTURE.md
- docs/SENSOR_TUNING_COMMISSIONING.md (vollstaendig)
- docs/SAFETY_COMPONENT_FAULTS.md (vollstaendig)
- docs/SAFETY_AND_FAULTS.md (vollstaendig)
- docs/TEMPERATURE_CONTROL.md (vollstaendig)
- docs/ACCEPTANCE_TESTS.md (vollstaendig, gezielt Abschnitt "Sensoren")
- docs/IMPLEMENTATION_PLAN.md
- docs/IMPLEMENTATION_ISSUES.md
- docs/audits/PROPOSED_RELEASE_1_ROADMAP.md
- docs/CI_AND_QUALITY_GATES.md (Ressourcenbericht-Mechanismus)
- Live-Issues #5, #10, #11, #20, #21, #24, #30
- Code: siehe Abschnitt 4
```

`SOURCES_READ: 20`

## 4. Bestandsaufnahme des Codes

### Bereits vorhanden (device_platform)

```text
lib/device_platform/src/time_source.hpp
  ITimeSource: monotonicMillis(), unixTimeSeconds()
lib/device_platform/src/virtual_time_source.hpp/.cpp
  VirtualTimeSource: deterministische, monoton fortschreibbare Testzeit
lib/device_platform/src/temperature_source.hpp
  TemperatureReading{bool available; double celsius;}
  ITemperatureSource::read() – Kommentar im Header stellt bereits klar, dass
  VALID/STALE/FAILED und Filterung NICHT Aufgabe dieses Ports sind.
lib/device_platform/src/storage_types.hpp
  StrongId<Tag,Underlying>-Template (u. a. StorageEpoch, Revision, SlotId),
  CheckedIncrementStatus, checkedIncrement() – etabliertes Muster fuer starke,
  nicht verwechselbare Ganzzahltypen.
lib/device_platform/src/state_store_key.hpp
  Muster "gueltig-by-construction": privater Konstruktor, statisches create(),
  ...CreateResult{status; optional<T>}.
lib/device_platform/src/storage_slot_limits.hpp
  Etabliertes *_limits.hpp-Muster: eigener Namespace
  device_platform::storage_slot_limits, ein dokumentiertes
  inline constexpr-Limit mit Begruendungskommentar.
```

### Bereits vorhanden (device_platform_test_support)

```text
lib/device_platform_test_support/src/mock_temperature_source.hpp/.cpp
  MockTemperatureSource: setCelsius(), setAvailable() – nur binaerer
  Verfuegbarkeits-Fehlerinjektor, keine Zeitstempel-, CRC- oder
  Mehrfachfehlerinjektion.
lib/device_platform_test_support/src/thermal_simulation_model.hpp
  ThermalSimulationModel: explizit unkalibriertes Driftmodell, nur fuer
  Softwareablauf-Tests.
```

### Bereits vorhanden (fermentation_app)

```text
lib/fermentation_app/src/process_state_machine.hpp
  ProcessState-Aufzaehlung und ProcessSignals – Beispiel fuer den
  App-Ebenen-Stil, aber fachlich nicht wiederverwendbar fuer #20.
```

### Nicht vorhanden (greenfield fuer #20)

```text
- kein Temperatur-Werttyp (nur roher double in TemperatureReading)
- kein Sensorqualitaets-/Diagnosezustand (VALID/STALE/FAILED) irgendwo im Code
- kein Medianfilter, kein Tiefpassfilter irgendwo in lib/
- kein ROM-/Sensoridentitaetstyp
- kein Fehlerursachen- oder Trendmodell fuer Sensoren
- kein Fehlersequenz-Injektor in device_platform_test_support (nur binaeres
  available/unavailable)
```

### Testkonventionen

```text
test/<thema>/test_<thema>.cpp, z. B. test/test_time_source/,
test/test_sensor_actuator_mocks/. Unity-Framework, TEST_ASSERT_*-Makros.
Keine manuelle CMake-Registrierung; PlatformIO discover't test/* automatisch
im native-Profil. library.json je lib/*-Verzeichnis setzt
-Wall -Wextra -Werror.
```

`EXISTING_COMPONENTS_INVENTORIED: PASS`

## 5. Bereits vorhandene wiederverwendbare Bausteine

```text
- ITimeSource / VirtualTimeSource: direkt fuer Alters- und
  Aenderungsratenberechnung wiederverwenden, kein neuer Zeitport.
- ITemperatureSource / TemperatureReading: bleibt unveraendert; #20 baut eine
  neue Schicht OBERHALB dieses Ports, ersetzt ihn nicht.
- StrongId<Tag,Underlying>-Muster: Vorlage fuer einen neuen
  SensorIdentity-Typ (ROM-/Identitaetswert).
- StateStoreKey-Muster ("gueltig-by-construction" mit privatem Konstruktor und
  statischem create()): Vorlage fuer den Kalibrier-Offset-Werttyp, falls eine
  Bereichspruefung bei Erzeugung sinnvoll ist.
- storage_slot_limits.hpp-Muster: Vorlage fuer eine neue
  lib/device_platform/src/sensor_limits.hpp mit firmwarefesten Obergrenzen.
- enum class ... : uint8_t mit Pro-Variante-Dokumentationskommentar
  (StateStoreKeyStatus, CheckedIncrementStatus): Vorlage fuer SensorQuality
  und einen Fehlerursachen-Enum.
- test/<thema>/test_<thema>.cpp-Konvention: direkt fuer neue Testverzeichnisse
  uebernehmen.
```

`DUPLICATE_TYPES_AVOIDED: PASS` – keiner der oben genannten Bausteine wird
dupliziert; #20 erweitert `device_platform` um neue, bislang nicht
existierende fachliche Typen (Qualitaetszustand, Filter, Sensorwert), ohne
etwas bereits Vorhandenes zu duplizieren.

## 6. Fehlende Akzeptanzkriterien (Ausgangslage vs. #20)

Nichts aus dem Scope von #20 existiert bereits. Alle folgenden
Akzeptanzkriterien aus dem Issue sind vollstaendig offen:

```text
- einzelne Fehlerwerte stoppen nicht sofort dauerhaft, werden aber nie als
  aktuell ausgegeben
- alte Werte verlieren nach enger Grenze die Regelungsfreigabe
- Luft- und Produktfilter koennen getrennt parametriert werden
- Diagnosequalitaet ist eindeutig
```

## 7. Scope und Nicht-Scope

### Scope (#20)

Vollstaendige Verarbeitungskette auf abstrakten, nativ testbaren Eingaben:

```text
Rohprobe
-> Bus-/Transportstatus
-> CRC-/Messfehlerbewertung
-> DS18B20-Fehlerwertpruefung (bekannte Sentinelwerte als firmwarefeste
   Konstante, siehe Abschnitt 8)
-> physikalischer Wertebereich
-> Zeit- und Aenderungsratenpruefung
-> Medianfilter
-> Kalibrier-Offset (reiner Werttyp, keine Persistenz)
-> sensorbezogener Tiefpass
-> Qualitaetszustand (VALID/STALE/FAILED)
-> Diagnose- und Regelwertvertrag
```

### Nicht-Scope (#20)

Wortgleich aus Abschnitt 15 des Auftrags uebernommen und verbindlich:

```text
- keine Implementierung in diesem Plan-PR
- keine reale DS18B20-/1-Wire-Anbindung
- keine Auswahl einer DS18B20-Bibliothek
- keine GPIO- oder Pinaenderung
- keine ESP-IDF-Treiberintegration
- keine NVS- oder LittleFS-Implementierung
- keine Kalibrierungs-UI
- keine Service-PIN-Logik
- keine Regelsensorauswahl oder Ersatzbetriebslogik aus #21
- keine PI-Regelung aus #22
- keine Aktorplanung aus #23
- keine Verriegelung oder SAFE_BOOT aus #24
- keine Display-, Web- oder Diagnoseoberflaeche
- keine realen Inbetriebnahmewerte erfinden
- keine Aenderung an akzeptierten ADRs
- keine neue Drittanbieterabhaengigkeit ohne zwingenden belegten Bedarf
- keine neuen Issues
- keine Aenderungen an Live-Issue-Bodies
- keine Aenderung an Firmware-, Test-, Build-, Workflow- oder Scriptdateien
  (gilt fuer DIESEN Plan-PR; die spaetere Implementierung aendert
  ausschliesslich Dateien aus Abschnitt 14)
```

`SCOPE_21_BOUNDARY_DEFINED: PASS`
`SCOPE_24_BOUNDARY_DEFINED: PASS`
`SCOPE_30_BOUNDARY_DEFINED: PASS`

Konkrete Grenzziehung:

```text
zu #21 (Regelsensorauswahl/Ersatzbetrieb):
  #20 liefert je Sensorrolle einen Qualitaets- und Diagnosevertrag
  (VALID/STALE/FAILED, gefilterter Wert, Fehlerursache, Verdachtsmarkierung
  bei Sensorwiderspruch). #20 entscheidet NICHT, welcher Sensor gerade der
  primaere Regelsensor ist, wechselt NICHT automatisch zwischen Produkt- und
  Luftfuehrung und implementiert KEINE Rueckkehrstrategie
  (automatic_validated_return_to_product usw.). Diese Entscheidungen
  konsumieren #20s Output, liegen aber vollstaendig in #21.

zu #24 (Fehlerklassen/Verriegelung/SAFE_BOOT):
  #20 erzeugt Qualitaets- und Fehlerursachenevidenz auf Portebene
  (z. B. "Sensor X ist FAILED, Ursache CRC"). #20 selbst kennt KEINE
  Fehlerklassen 1-4, erzeugt KEINE persistente Verriegelung, keinen
  Watchdog- oder SAFE_BOOT-Bezug und entscheidet nicht ueber Quittierung
  oder Fehlerreset. Die Abbildung von Sensor-FAILED auf einen verriegelten
  Sicherheitsfehler ist #24s Aufgabe.

zu #30 (reale DS18B20-Busse):
  #20 verarbeitet ausschliesslich abstrakte, direkt im Test konstruierte
  Eingabewerte (RawSensorSample, siehe Abschnitt 9). #20 verwendet und
  aendert NICHT das bestehende ITemperatureSource/TemperatureReading (Port
  bleibt fuer #20 unangetastet, siehe Abschnitt 18 Punkt "Portgrenze"). Wie
  ein spaeterer realer DS18B20-Adapter aus #30 Bus-/CRC-/Fehlerdetails in
  RawSensorSample-Werte uebersetzt, ist ausdruecklich #30s Aufgabe und nicht
  Teil dieses Plans.
```

## 8. Fachliches Zustandsmodell

Oeffentliche Zustaende (Minimum aus dem Auftrag, keine zusaetzliche
Zustandsart):

```cpp
enum class SensorQuality : uint8_t { Valid, Stale, Failed };
```

Begruendung gegen einen zusaetzlichen `UNINITIALIZED`-Zustand: Der
Startzustand vor der ersten gueltigen Probe verhaelt sich gegenueber jedem
Konsumenten identisch zu `STALE` ohne zuletzt gueltigen Wert (kein
Regelwert, kein Aktorfreigabe-Kennzeichen, Alter "unendlich"/nicht gesetzt).
`SAFETY_COMPONENT_FAULTS.md` kennt selbst nur die Folge `VALID -> STALE ->
FAILED`. Ein separater vierter Zustand wuerde jeden Konsumenten (#21, #24)
zwingen, zwei Zustaende (`UNINITIALIZED` und `STALE`) gleich zu behandeln,
ohne einen fachlichen Unterschied zu begruenden – ein KISS- und DRY-Verstoss.
Der Startzustand wird deshalb intern als `Stale` mit leerem
"letzter gueltiger Wert" gefuehrt.

### Zustandsuebergaenge

```text
Start (keine Probe) = Stale, kein letzter gueltiger Wert, Alter = unendlich

Stale -> Valid:
  kMinConsecutiveValidSamples aufeinanderfolgende plausible Proben UND
  Zeitspanne >= kMinRecoveryStabilityDurationMs seit Beginn der Folge UND
  (bei vorherigem ROM-Wechsel) Identitaet entspricht der erwarteten Rolle

einzelne ungueltige Probe (Valid-Zustand):
  -> Stale, letzter gueltiger Wert und dessen Alter bleiben fuer Diagnose
     erhalten, Wiedererkennungszaehler beginnt

aufeinanderfolgende ungueltige Proben ODER Altersueberschreitung
(Stale-Zustand):
  Alter des letzten gueltigen Werts > kMaxStaleAgeMs
    ODER Anzahl aufeinanderfolgender ungueltiger Proben > kMaxConsecutiveInvalid
    -> Failed

Failed -> Valid:
  identische Bedingung wie Stale -> Valid (kMinConsecutiveValidSamples +
  kMinRecoveryStabilityDurationMs); Filterzustand (Median, Tiefpass) wird vor
  dem ersten neuen Beitrag zurueckgesetzt (siehe Abschnitt 10.3/10.5)

erneute ungueltige Probe waehrend Wiedererkennung:
  Wiedererkennungszaehler und -Startzeit werden zurueckgesetzt; der Zustand
  bleibt Stale bzw. Failed (kein Zwischenzustand, kein stiller Fortschritt)

Sensoridentitaetswechsel (ROM-Wechsel):
  wird als Plausibilitaetsereignis erkannt (siehe Abschnitt 11), fuehrt selbst
  NICHT automatisch zu Valid; die Wiedererkennungsbedingung muss danach neu
  und vollstaendig erfuellt werden. Filterzustand wird bei erkanntem
  ROM-Wechsel verworfen (ein alter Medianpuffer/Tiefpasswert eines anderen
  physischen Sensors darf nicht in den neuen Sensor hineinwirken).

Reset/Fortfuehrung der Filterzustaende:
  Valid -> Stale (kurzzeitig): Filterzustand bleibt erhalten (einzelne
    ungueltige Probe soll keinen bereits eingeschwungenen Filter verwerfen)
  Stale/Failed -> Valid (nach Wiedererkennung) ODER ROM-Wechsel:
    Filterzustand wird verworfen und beginnt mit den Proben der erfolgreichen
    Wiedererkennungsfolge neu
```

Sicherheitsgrenze (firmwarefest, nicht parametrierbar):

```text
Ein STALE- oder FAILED-Wert wird NIEMALS als aktueller Regelwert
zurueckgegeben. Der Diagnosevertrag unterscheidet strikt zwischen
"regelwertfaehig" (nur bei Valid) und "letzter bekannter Wert nur zu
Diagnosezwecken" (bei Stale/Failed, mit explizitem Alter und Status).
```

`STATE_MACHINE_DEFINED: PASS`
`RECOVERY_RULES_DEFINED: PASS`

## 9. Eingangsvertrag

Kein bestehender Typ deckt den geforderten Eingangsvertrag ab (siehe
Abschnitt 4/18 "Portgrenze"). Neuer, von `ITemperatureSource` entkoppelter
Werttyp in `device_platform`:

```cpp
// sensor_identity.hpp
namespace device_platform::detail { struct SensorIdentityTag {}; }
using SensorIdentity = StrongId<detail::SensorIdentityTag, uint64_t>;
// 0 ist reserviert ("unbekannte/keine Identitaet"), analog StorageEpoch.

// sensor_sample.hpp
enum class SensorTransportStatus : uint8_t { Ok, BusFault, CrcFault, MissingSample };

struct RawSensorSample {
    SensorIdentity identity;          // 0 = unbekannt (z. B. vor erster Enumeration)
    uint64_t monotonicTimestampMs;    // von ITimeSource::monotonicMillis()
    double rawCelsius;                // nur gueltig, wenn transportStatus == Ok
    SensorTransportStatus transportStatus;
    std::optional<uint16_t> driverFaultCode; // optionaler roher Treibercode, nur Diagnose
};
```

Begruendung: `TemperatureReading{available,celsius}` unterscheidet weder
Bus- von CRC-Fehlern noch eine fehlende von einer verfuegbaren, aber
unplausiblen Probe, und traegt keinen Zeitstempel. Der Auftrag verlangt in
Abschnitt 6 ausdruecklich diese feinere Unterscheidung. `RawSensorSample`
ist ein reiner Werttyp ohne Bus-/Treiberabhaengigkeit; die Uebersetzung von
`ITemperatureSource`/einem spaeteren realen DS18B20-Adapter in
`RawSensorSample` ist Aufgabe der Komposition bzw. von #30 und nicht Teil
dieses Plans (siehe Abschnitt 18, Portgrenze).

### Behandelte Eingangsfaelle

```text
verspaetete Probe: Zeitstempel wird gegen den zuletzt akzeptierten
  Zeitstempel geprueft; eine "verspaetete" Probe (aelterer Zeitstempel als
  bereits verarbeitet) wird verworfen und als Plausibilitaetsereignis
  gezaehlt, veraendert aber nicht rueckwirkend den Filterzustand.
doppelte Probe: identischer Zeitstempel UND identischer Rohwert wie die
  zuletzt akzeptierte Probe wird als Duplikat erkannt und ignoriert (kein
  Doppelzaehlen im Medianfenster), OHNE das Sensoralter neu zu setzen.
Zeitstempelruecksprung: Zeitstempel < zuletzt akzeptierter Zeitstempel wird
  wie "verspaetete Probe" behandelt (siehe oben); zusaetzlich wird ein
  ruecklaeufiger Zeitstempel als Plausibilitaetsverdacht auf Zeitbasisfehler
  markiert (Diagnose, keine Regelentscheidung).
identischer Zeitstempel, unterschiedlicher Wert: wird NICHT als Duplikat
  behandelt, sondern reihenfolge-stabil als naechste Probe akzeptiert
  (Aenderungsratenpruefung verwendet dann eine Mindest-Zeitdifferenz > 0, um
  Division durch Null zu vermeiden, siehe Abschnitt 10.2).
extrem grosse Zeitluecke: erlaubt (kein Fehler an sich), fuehrt aber bei
  Ueberschreiten von kMaxStaleAgeMs zum Zustandsuebergang wie in Abschnitt 8
  beschrieben; nach der Luecke beginnt die Aenderungsratenpruefung ohne
  Vorwert (erste Probe nach Luecke kann nicht als "Sprung" bewertet werden,
  da kein gueltiger Vorwert unmittelbar davor existiert).
Sensor-/ROM-Wechsel: siehe Abschnitt 8 und 11.
nicht endlicher Wert (NaN/Inf): wird als eigener Plausibilitaetsfehler
  (Wertebereichspruefung, std::isfinite) erkannt, sofern der zugrunde
  liegende double dies zulaesst; RawSensorSample selbst schraenkt den
  Werttyp nicht ein, das ist bewusst der Plausibilitaetsstufe zugeordnet und
  nicht Aufgabe des Eingangswerttyps.
```

`INPUT_CONTRACT_DEFINED: PASS`

## 10. Verarbeitungspipeline

Reihenfolge exakt wie in Abschnitt 5 des Auftrags und in
`docs/ARCHITECTURE.md`/`docs/SENSOR_TUNING_COMMISSIONING.md` vorgegeben.
Jede Stufe ist eine eigene kleine, testbare Komponente (SRP), orchestriert
von einer duennen `SensorQualityPipeline`.

### 10.1 Transport-/CRC-/DS18B20-Fehlerwertstufe

```text
transportStatus != Ok -> Probe sofort ungueltig (Ursache = BusFault/
  CrcFault/MissingSample), keine weitere Stufe verarbeitet den Rohwert.
transportStatus == Ok, aber rawCelsius entspricht einem konfigurierten
  bekannten Sentinelwert (firmwarefeste Liste in sensor_limits.hpp, z. B.
  DS18B20-Einschaltwert 85.0 und Diskonnekt-/Fehlerwert -127.0) -> ungueltig,
  Ursache = KnownFaultValue. Diese Werte sind Datenblattkonstanten des
  DS18B20 und damit firmwarefest, keine TBD_COMMISSIONING-Groesse; #20
  kennt nur die Zahlenkonstante, keine DS18B20-Treiber- oder Bibliothekstypen.
```

### 10.2 Physikalischer Wertebereich und Aenderungsrate

```text
rawCelsius ausserhalb [kAbsoluteMinCelsius, kAbsoluteMaxCelsius]
  (firmwarefest, sensor_limits.hpp) -> ungueltig, Ursache = OutOfRange.
Aenderungsrate = |rawCelsius - letzterGueltigerRohwert| /
  ((timestampMs - letzterGueltigerTimestampMs) / 1000.0), nur berechnet wenn
  ein unmittelbar vorheriger gueltiger Wert existiert und die Zeitdifferenz
  > 0 ist. Ueberschreitet sie den rollenabhaengig konfigurierten
  kMaxRateOfChangeCelsiusPerSecond -> als "Sprung" markiert. Ein einzelner
  Sprung fuehrt NICHT sofort zu FAILED (siehe Akzeptanzkriterium "einzelne
  Fehlerwerte stoppen nicht sofort dauerhaft"), sondern zaehlt als eine
  ungueltige Probe im Sinne von Abschnitt 8.
```

### 10.3 Medianfilter

```text
Fixe Kapazitaet kMedianWindowSize (ungerade, konfigurierbar innerhalb eines
firmwarefesten Maximalfensters kMaxMedianWindowSize, sensor_limits.hpp).
Ungerades Fenster gewaehlt, damit der Median stets ein tatsaechlich
gemessener Wert ist (kein Mittelwert zweier mittlerer Werte), was
Testorakel und Nachvollziehbarkeit vereinfacht (KISS).
Vor gefuelltem Fenster: Median aus den bisher vorhandenen (< voller
  Fenstergroesse) gueltigen Werten, NICHT aus mit 0 aufgefuellten
  Phantomwerten.
Nur PLAUSIBLE Proben (nach 10.1/10.2 gueltig) gelangen ins Fenster.
Nach FAILED/ROM-Wechsel: Fenster wird geleert (siehe Abschnitt 8).
Feste Ringpuffer-Kapazitaet, keine Heapallokation (std::array, kein
std::vector).
```

### 10.4 Kalibrier-Offset

```cpp
// sensor_offset.hpp – reiner Werttyp, keine Persistenz in #20
struct SensorOffset {
    double value;
};
```

```text
Firmwarefeste zulaessige Grenze kMaxAbsoluteOffsetCelsius
  (sensor_limits.hpp). Ein Offset ausserhalb dieser Grenze wird bereits an
  der Konstruktionsstelle (Aufrufer/Test) als Programmfehler behandelt;
  #20 selbst nimmt keine Persistenz- oder UI-Validierung vor (siehe
  Nicht-Scope). Fehlender Offset = SensorOffset{0.0} (neutral, keine
  Sonderzustandsunterscheidung noetig). Ein Offsetwechsel waehrend
  laufender Filterung wirkt erst auf die naechste eingehende Probe; bereits
  im Medianfenster befindliche Rohwerte werden NICHT rueckwirkend
  korrigiert (Medianfenster enthaelt Rohwerte, Offset wird NACH dem
  Medianfilter angewendet, siehe Reihenfolge oben – ein Offsetwechsel kann
  daher nie zu einer Vermischung unterschiedlich korrigierter Werte
  innerhalb eines Medianfensters fuehren).
```

### 10.5 Sensorbezogener Tiefpass

```text
Einfacher zeitkonstantenbasierter Exponentialfilter:
  gefiltert_neu = gefiltert_alt + (korrigierterWert - gefiltert_alt) *
                  (1 - exp(-dtSeconds / tauSeconds))
  dtSeconds = tatsaechlich vergangene Zeit seit letztem Filterschritt
  (aus ITimeSource-Zeitstempeln), keine feste Zykluszeit-Annahme, auch wenn
  der Regelzyklus nominal ~2 s betraegt (robust gegen Jitter/Luecken).
Rollenabhaengige Parametrierung ausschliesslich ueber unterschiedliche
  tauSeconds-Werte in der je Instanz uebergebenen Konfiguration (siehe
  Abschnitt 12) – keine rollenspezifische Codeverzweigung (DRY).
Bei Messluecke (grosse dtSeconds): Filter konvergiert entsprechend der
  Formel automatisch schneller gegen den neuen Wert (kein Sondercode
  noetig); nach FAILED/ROM-Wechsel wird gefiltert_alt beim ersten neuen
  Wiedererkennungswert auf den korrigierten Rohwert gesetzt (kein
  "Anschleichen" aus einem verworfenen alten Zustand).
Rohpfad (10.1-10.2, Sicherheitspruefung) bleibt strikt getrennt vom
  gefilterten Regelwert: die Diagnoseausgabe zeigt beide, ein extremer
  Rohwert wird nie allein durch den Tiefpass verdeckt.
```

`FILTER_PIPELINE_DEFINED: PASS`

## 11. Widerspruchs- und Plausibilitaetsbewertung (rollenuebergreifend)

Der Auftrag (Abschnitt 8) verlangt, vier Faelle zu unterscheiden. Diese
werden hier vollstaendig durchdekliniert, statt implizit zu bleiben:

```text
1. sicher erkannter technischer Fehler
   -> innerhalb #20 vollstaendig abgedeckt: SensorFaultReason
      {BusFault, CrcFault, MissingSample, KnownFaultValue, OutOfRange,
      RateOfChangeExceeded, IdentityMismatch} (Abschnitt 12), erzeugt durch
      die Pipeline selbst (Abschnitt 10.1/10.2).

2. unplausible Einzelprobe
   -> innerhalb #20 vollstaendig abgedeckt: eine einzelne ungueltige Probe
      (jede der obigen Ursachen) fuehrt zu Valid->Stale, OHNE sofort Failed
      auszuloesen (Abschnitt 8), und wird im Snapshot als
      lastFaultReason/consecutiveInvalidCount sichtbar.

3. anhaltender Verdacht
   -> innerhalb #20 TEILWEISE abgedeckt: consecutiveInvalidCount und
      lastValidSampleAgeMs (Abschnitt 12) bilden einen anhaltenden Verdacht
      GEGEN DIESELBE Rolle quantitativ ab (mehrere Folgefehler statt eines
      Ausreissers ist bereits an einem hohen consecutiveInvalidCount bzw.
      Stale mit wachsendem lastValidSampleAgeMs ablesbar, ohne dass #20
      selbst einen separaten "Verdacht"-Zustand fuehrt - das waere ein
      redundanter, aus denselben Feldern ableitbarer fuenfter Zustand und
      damit ein KISS-Verstoss, siehe Abschnitt 8).

4. nicht eindeutig zuordenbarer Sensorwiderspruch (rollenuebergreifend)
   -> AUSSERHALB #20: #20 stellt je Sensorrolle einen unabhaengigen
      Pipeline-Snapshot bereit. Der rollenuebergreifende VERGLEICH (z. B.
      "Produkt weicht stark von Luft ab") gehoert laut
      docs/SAFETY_COMPONENT_FAULTS.md zur Fehlerklassifizierung/
      -verriegelung (Punkte 1-5 im Abschnitt "Widerspruechliche
      Sensorwerte") und damit inhaltlich zu #21/#24. #20 liefert dafuer NUR
      die notwendige Evidenz je Rolle - rawCelsius, filteredCelsius,
      changeRateCelsiusPerSecond, lastSampleAgeMs/lastValidSampleAgeMs,
      lastFaultReason (alle bereits Teil des Diagnosevertrags, Abschnitt
      12) - und trifft selbst KEINE Schuldzuweisung/Verdaechtigung
      zwischen Rollen, da #20 (siehe Abschnitt 13) keine Rollenkenntnis
      besitzt und daher strukturell nicht wissen kann, welche andere
      Pipeline-Instanz "die andere Rolle" ist. #21/#24 berechnen Punkt 4
      ausschliesslich aus den hier bereits bereitgestellten Feldern
      mehrerer Instanzen, ohne dass #20 dafuer erweitert werden muss.
```

Zusaetzlich, innerhalb einer einzelnen Rolle: Eine dauerhaft dem Prozess
entsprechend dennoch nahezu unveraenderte Produkttemperatur wird innerhalb
der EIGENEN Pipeline nicht als Fehler gewertet (kein "unveraenderter
Wert"-Kriterium in der Aenderungsratenpruefung – nur Sprung-, nicht
Stillstandserkennung, siehe 10.2).

Sensoridentitaets-/ROM-Wechsel (innerhalb einer Rolle) IST Teil von #20:
RawSensorSample.identity aendert sich gegenueber der zuletzt bekannten
Identitaet -> als Plausibilitaetsereignis markiert, Filterzustand wird
verworfen (Abschnitt 8/10.3), Wiedererkennung wie nach FAILED erforderlich.

## 12. Ausgabe- und Diagnosevertrag

Bewertung Mega-Struktur vs. mehrere kleine Typen: Ein einzelner
`SensorQualitySnapshot` pro Pipeline-Instanz (nicht pro Rolle als
Sammelstruktur ueber alle drei Sensoren) ist gerechtfertigt, weil alle
Felder zu GENAU EINER Sensorrolle gehoeren und immer gemeinsam als ein
konsistenter Zustand entstehen (ein Snapshot = ein Verarbeitungsschritt
einer Probe). Eine Aufteilung in mehrere Kleintypen wuerde hier nur
kuenstliche Kopplung zwischen Aufrufern erzeugen, ohne eine echte
Verantwortungsgrenze abzubilden (KISS). Eine Sammelstruktur ueber ALLE
Sensorrollen wird bewusst NICHT gebaut, weil "welche Rollen es gibt" ein
fermentation_app-Konzept ist (siehe Abschnitt 13) – `device_platform`
kennt nur "eine Pipeline-Instanz pro injizierter Quelle".

```cpp
// sensor_quality_snapshot.hpp
enum class SensorFaultReason : uint8_t {
    None, BusFault, CrcFault, MissingSample, KnownFaultValue,
    OutOfRange, RateOfChangeExceeded, IdentityMismatch,
};

struct SensorQualitySnapshot {
    SensorIdentity identity;
    SensorQuality quality;
    double rawCelsius;               // letzte Rohprobe, auch wenn ungueltig
    double correctedCelsius;         // rawCelsius + Offset, nur bei Valid/Stale mit Wert
    double filteredCelsius;          // Regelwert nach Tiefpass, nur bei Valid mit Wert
    double appliedOffset;
    uint64_t lastSampleAgeMs;        // seit letzter EINGEGANGENER Probe
    uint64_t lastValidSampleAgeMs;   // seit letzter GUELTIGER Probe
    SensorFaultReason lastFaultReason;
    uint16_t consecutiveInvalidCount;
    uint16_t recoveryProgressCount;  // aufeinanderfolgende gueltige Proben waehrend Wiedererkennung
    double changeRateCelsiusPerSecond;
    bool controlValueUsable;         // true nur bei quality == Valid
    bool diagnosticOnly;             // true bei Stale/Failed
};
```

Kein Trend-/Aenderungsratenfeld wird weggelassen: `SAFETY_COMPONENT_FAULTS.md`
verlangt fuer den Kuehlkoerpersensor ausdruecklich eine Aenderungsraten-
/Trendueberwachung ("ueberwacht Temperatur und Aenderungsrate der
Leistungsbaugruppe") – das Feld ist daher verpflichtend, nicht optional.

`DIAGNOSTIC_CONTRACT_DEFINED: PASS`

## 13. Modul- und Abhaengigkeitsrichtung

Abweichend von der illustrativen Modulzuordnung in Abschnitt 11 des
Auftrags wird die GESAMTE fachliche Pipeline aus #20
(Zustandsmaschine, Plausibilitaet, Filter, Diagnosevertrag) in
`lib/device_platform/` verortet, nicht in `lib/fermentation_app/`.

Begruendung (normale Implementierungsentscheidung, hier begruendet, siehe
Abschnitt 16 des Auftrags "normale Implementierungsdetails entscheidet der
Agent selbst"):

```text
- lib/device_platform/AGENTS.md erlaubt im Abschnitt "Erlaubt" ausdruecklich
  "allgemeine Sensorqualitaet, Filter und begrenzte Reglerbausteine".
- Keine der #20-Bullet-Points (Zustaende, CRC/Bus, Wertebereich, Median,
  Tiefpass, ROM-Offset, Wiedererkennung) nennt Joghurt, Kefir, Kombucha oder
  eine Fermentationsphase. Die eigene Qualitaetsregel des Moduls verlangt,
  dass Namen/APIs auch fuer Smoker, Gewaechshaus oder Temperaturueberwachung
  sinnvoll bleiben – #20 erfuellt das unveraendert.
- ADR-013 (Wiederverwendbare ESP32-Geraeteplattform) begruendet exakt diesen
  Schnitt: generische, hardwareunabhaengige Geraetedienste gehoeren nach
  device_platform.
- KEIN geraetespezifischer Name (z. B. "Schrankluft", "Produkt") wird als
  Typ oder Methode in device_platform verwendet, da
  device_platform/AGENTS.md geraetespezifische ROLLEN als allgemeine
  Plattform-API ausdruecklich verbietet. Die drei konkreten Rollen
  Schrankluft/Produkt/Kuehlkoerper bleiben deshalb ein fermentation_app-
  bzw. Composition-Root-Konzept: fermentation_app haelt (spaeter, nicht in
  #20) drei benannte SensorQualityPipeline-Instanzen mit rollenspezifischer
  Konfiguration; #20 selbst kennt "Rollen" nicht als Typ.
- SensorQuality-Ausgabe (Abschnitt 12) enthaelt bewusst KEIN Rollenfeld,
  nur eine anwendungsneutrale SensorIdentity – konsistent mit der obigen
  Regel.
```

Datei-Zuordnung:

```text
lib/device_platform/src/
  sensor_identity.hpp          (neu)
  sensor_sample.hpp             (neu)
  sensor_quality.hpp             (neu, enum SensorQuality)
  sensor_quality_snapshot.hpp    (neu, enum SensorFaultReason + Snapshot)
  sensor_offset.hpp              (neu)
  sensor_median_filter.hpp/.cpp  (neu)
  sensor_lowpass_filter.hpp/.cpp (neu)
  sensor_quality_pipeline.hpp/.cpp (neu, Orchestrator)
  sensor_limits.hpp              (neu, firmwarefeste Grenzen)

lib/device_platform_test_support/src/
  sensor_fault_sequence.hpp/.cpp (neu, deterministischer Mehrfach-
                                   Fehlerinjektor: geskriptete Folge von
                                   RawSensorSample inkl. Bus-/CRC-/
                                   Zeitstempelanomalien fuer Tests)

test/
  test_sensor_quality/test_sensor_quality.cpp (neu)
```

`fermentation_app` und `src/main.cpp`/`main/app_main.cpp` werden von #20
NICHT geaendert: Es gibt noch keinen Verbraucher (die Verdrahtung von drei
rollenbenannten Pipeline-Instanzen ist #21s bzw. der spaeteren Composition-
Root-Aufgabe, sobald #21 tatsaechlich einen primaeren Regelsensor waehlt).
Dies vermeidet totes/verfrueht verdrahtetes Produktionsglue-Code ohne
Verbraucher.

`fermentation_app`-Regeln (`AGENTS.md`) bleiben unberuehrt, da #20 dort
nichts aendert. `device_platform_test_support`-Regeln bleiben eingehalten:
Der neue Fehlerinjektor ist eine Mockimplementierung/Testhilfe, keine reale
Bus-/Aktorlogik, und wird nicht von `src/main.cpp` oder einem
ESP32-Produktionsbuild eingebunden.

## 14. Geplante Dateien mit Begruendung

```text
lib/device_platform/src/sensor_identity.hpp
  Neuer StrongId-basierter Sensoridentitaetstyp; noetig fuer ROM-Wechsel-
  Erkennung (Abschnitt 8/11), reine Wertsemantik, keine 1-Wire-Kenntnis.

lib/device_platform/src/sensor_sample.hpp
  RawSensorSample + SensorTransportStatus; Eingangsvertrag (Abschnitt 9).

lib/device_platform/src/sensor_quality.hpp
  SensorQuality-Enum (Abschnitt 8).

lib/device_platform/src/sensor_quality_snapshot.hpp
  SensorFaultReason + SensorQualitySnapshot; Ausgabevertrag (Abschnitt 12).

lib/device_platform/src/sensor_offset.hpp
  SensorOffset-Werttyp (Abschnitt 10.4).

lib/device_platform/src/sensor_median_filter.hpp/.cpp
  Medianfilter mit fester Kapazitaet (Abschnitt 10.3), eigene SRP-Einheit,
  eigenstaendig testbar.

lib/device_platform/src/sensor_lowpass_filter.hpp/.cpp
  Tiefpassfilter mit Zeitkonstante (Abschnitt 10.5), eigene SRP-Einheit.

lib/device_platform/src/sensor_quality_pipeline.hpp/.cpp
  Orchestrator: verkettet 10.1-10.5, fuehrt Zustandsmaschine (Abschnitt 8)
  und erzeugt SensorQualitySnapshot. Nimmt SensorQualityConfig entgegen
  (siehe Abschnitt 15) und eine ITimeSource-Referenz fuer den Fall, dass die
  Pipeline selbst "jetzt" braucht (z. B. Altersberechnung gegenueber dem
  Zeitpunkt der Snapshot-Abfrage, nicht nur gegenueber der letzten Probe).

lib/device_platform/src/sensor_limits.hpp
  Firmwarefeste Obergrenzen (max. Medianfenster, absoluter
  Temperaturbereich, max. Offsetbetrag, DS18B20-Sentinelwerte, max.
  STALE-Alter-Obergrenze, max. Wiedererkennungs-Probenzahl-Obergrenze) nach
  dem Muster von storage_slot_limits.hpp. Konkrete, am realen Schrank
  ermittelte Werte bleiben TBD_COMMISSIONING (siehe Abschnitt 18); diese
  Datei liefert nur die aeusseren, nie ueberschreitbaren Sicherheitsgrenzen.

lib/device_platform_test_support/src/sensor_fault_sequence.hpp/.cpp
  Deterministischer, skriptbarer Mehrfach-Fehlerinjektor: liefert eine
  vordefinierte Folge von RawSensorSample-Werten (inkl. Bus-/CRC-Fehlern,
  Zeitstempelanomalien, ROM-Wechseln) fuer die Testmatrix aus Abschnitt 17.
  Ersetzt/ergaenzt NICHT MockTemperatureSource (bleibt fuer #20 unberuehrt,
  da #20 nicht ueber ITemperatureSource einspeist, siehe Abschnitt 9).

test/test_sensor_quality/test_sensor_quality.cpp
  Vollstaendige native Testsuite gemaess Abschnitt 17.

docs/tasks/issue-20-sensor-quality-filtering-plan.md
  Dieser Plan (bereits Teil des Plan-PR-Diffs).
```

Kein bestehender Header wird veraendert (`temperature_source.hpp`,
`time_source.hpp`, `storage_types.hpp`, `state_store_key.hpp` bleiben exakt
wie heute).

`EXPECTED_FILE_DIFF_DEFINED: PASS`

## 15. SOLID-/DRY-/KISS-Bewertung

```text
S (Single Responsibility):
  Vier klar getrennte Klassen (MedianFilter, LowPassFilter, die
  Zustandsmaschine/Plausibilitaetslogik innerhalb der Pipeline, und der
  reine Diagnose-Snapshot-Typ) statt einer Monsterklasse. Jede Klasse ist
  unabhaengig mit synthetischen Werten testbar (siehe Testmatrix,
  Abschnitt 17 nennt Filter-Tests getrennt von Zustandsmaschinen-Tests).

O (Open/Closed):
  Neue Sensorrollen entstehen ausschliesslich durch eine neue
  SensorQualityConfig-Instanz (unterschiedliche tau/Fensterwerte), NICHT
  durch neue Codepfade oder if/switch auf eine Rolle. Eine vierte
  Sensorrolle (falls je gebraucht) erfordert keine Aenderung an
  SensorQualityPipeline.

L (Liskov Substitution):
  Betrifft #20 nur mittelbar (keine neue Portabstraktion mit mehreren
  Implementierungen). Falls der spaetere reale DS18B20-Adapter (#30)
  RawSensorSample erzeugt, muss er exakt dieselbe Wertsemantik einhalten
  wie der in #20 gebaute Testinjektor (sensor_fault_sequence.hpp) – das wird
  hier bereits als Vertrag festgeschrieben (Abschnitt 9), nicht erst in #30.

I (Interface Segregation):
  RawSensorSample und SensorQualitySnapshot sind reine Werttypen ohne
  virtuelle Schnittstelle; SensorQualityPipeline hat genau eine
  oeffentliche Verarbeitungs- und eine Snapshot-Abfragemethode. Kein
  Konsument wird gezwungen, mehr zu kennen als er braucht.

D (Dependency Inversion):
  Die Pipeline haengt nur von ITimeSource (Abstraktion) ab, nie von
  DS18B20-, Arduino- oder ESP-IDF-Typen. Ein spaeterer DS18B20-Adapter
  haengt in Richtung device_platform, nicht umgekehrt.

DRY:
  Eine einzige parametrisierte SensorQualityPipeline-Implementierung fuer
  alle drei Rollen (Luft/Produkt/Kuehlkoerper); keine kopierten
  Filter-/Zustandsmaschinen-Implementierungen pro Rolle. Firmwarefeste
  Grenzen liegen zentral in sensor_limits.hpp, nicht als verstreute Magic
  Numbers (siehe AGENTS.md "Werte und Parametrierung").

KISS:
  Deterministischer, linearer Datenfluss (siehe Abschnitt 10) ohne
  generische Rules-Engine, ohne Plugin-/Registrierungsmechanismus, ohne
  externe Filterbibliothek fuer die vergleichsweise einfachen Median-/
  Tiefpassalgorithmen (kein realer Bedarf fuer eine Abhaengigkeit belegt).
  Genau drei oeffentliche Qualitaetszustaende statt vier (siehe Abschnitt 8).

Bewusste Abweichung von der Auftragsskizze:
  Die in Abschnitt 11 des Auftrags illustrativ vorgeschlagene
  fermentation_app-Zuordnung wird zugunsten von device_platform verlassen
  (Begruendung siehe Abschnitt 13). Dies ist keine Abweichung von einem
  bereits freigegebenen Plan (es gibt noch keinen), sondern eine im Rahmen
  dieses Plans selbst zu treffende und hier explizit begruendete
  Architekturentscheidung gemaess AGENTS.md/ADR-013.
```

`SOLID_REVIEW: PASS`
`DRY_REVIEW: PASS`
`KISS_REVIEW: PASS`

## 16. Ressourcen-, Laufzeit- und CI-Nachweise der spaeteren Umsetzung

```text
Speicher pro Pipeline-Instanz (Schaetzung, spaeter im Implementierungs-PR
  per scripts/build_report.py nachzuweisen):
  - RawSensorSample: ~24 Byte
  - Medianpuffer: kMaxMedianWindowSize * sizeof(double), bei einer
    erwarteten Obergrenze von z. B. 9 Werten ~72 Byte
  - SensorQualitySnapshot: ~80-96 Byte
  - Pipeline-interner Zustand (letzter Wert, Zaehler, Zeitstempel):
    < 100 Byte
  - Gesamt < 400 Byte je Instanz, statisch (kein Heap), 3 Instanzen (Luft,
    Produkt, Kuehlkoerper) < 1,2 KB RAM – vernachlaessigbar gegenueber dem
    4-MB-Flash-/PSRAM-freien Budget, keine belastbare Zahl vor realem Build.

maximale Anzahl gleichzeitig unterstuetzter Sensorrollen:
  #20 selbst kennt keine Obergrenze (eine Pipeline-Instanz pro injizierter
  Quelle); die tatsaechliche Anzahl (drei) legt fermentation_app spaeter
  fest. Kein unbeschraenkter Container in #20.

worst-case Laufzeit pro eingehender Probe:
  O(kMedianWindowSize) fuer die Medianberechnung (Sortierung eines festen,
  kleinen Fensters, z. B. Insertion Sort fuer < 10 Elemente), alle anderen
  Stufen O(1). Kein Sensorzyklus schreibt in Flash (AGENTS.md-Vorgabe
  "es wird nicht in jedem Sensorzyklus in Flash geschrieben" ist fuer #20
  automatisch erfuellt, da #20 keine Persistenz durchfuehrt).

Verhalten bei Bursts/zu schnellen Proben:
  Wird ausschliesslich ueber den Zeitstempelvertrag (Abschnitt 9) geregelt,
  keine zusaetzliche Ratenbegrenzung noetig; ein zu kurzer dt fuehrt
  hoechstens zu einer grossen Aenderungsrate (10.2), nicht zu einem
  Pufferueberlauf, da alle Puffer fest dimensioniert sind.

statisches RAM/Flash:
  std::array statt std::vector fuer den Medianpuffer; keine dynamische
  Allokation im Verarbeitungspfad. Flash-Wirkung durch 8 neue kleine
  Header/Source-Dateien wird als gering eingeschaetzt, aber erst durch
  scripts/build_report.py belastbar.

Auswirkung auf native Tests und beide ESP32-Profile:
  Reiner device_platform-Code, im native-Profil ohne Aenderung testbar.
  Kein neuer Code in main/app_main.cpp oder src/main.cpp -> keine
  Auswirkung auf esp32_bringup/esp32_release ausser dem zusaetzlichen
  (unverdrahteten) Uebersetzungsergebnis der neuen Header/Source-Dateien.

Base-/Head-Ressourcenvergleich:
  Im Implementierungs-PR mit
  "python scripts/build_report.py --output build-report.md" (native) sowie
  nach ESP-IDF-Export mit den Profilen bringup/release durchzufuehren, wie
  in docs/CI_AND_QUALITY_GATES.md beschrieben. Verbindliche Byte-Budgets
  bleiben TBD_IMPLEMENTATION_BUDGET bis zur realen Messung.
```

`RESOURCE_PLAN_DEFINED: PASS`

## 17. Vollstaendige Testmatrix

Jede Gruppe nennt Testsuite (neu: `test/test_sensor_quality/`) und
fachliches Orakel.

```text
Start/Normalbetrieb (Orakel: Abschnitt 8 Zustandsmaschine, SENSOR_TUNING_
COMMISSIONING.md "Messzyklus"):
  - Start ohne Probe -> Stale, kein Regelwert
  - erste gueltige Probe -> weiterhin Stale (noch nicht genug Folgeproben)
  - kMinConsecutiveValidSamples Folgeproben -> Valid
  - stabiler ~2s-Zyklus ueber mehrere Minuten (virtuelle Zeit) -> Valid
    bleibt stabil
  - getrennte Konfiguration (SensorQualityConfig) fuer zwei Instanzen
    (z. B. "Luft"-aehnlich vs. "Produkt"-aehnlich parametriert) erzeugt
    nachweisbar unterschiedliche Tiefpassdynamik bei identischer
    Eingangsfolge
  - Roh-, Korrektur- und Filterwert sind im Snapshot getrennt und
    nachvollziehbar unterschiedlich bei einem gesetzten Offset

Transport-/Messfehler (Orakel: Abschnitt 10.1, ACCEPTANCE_TESTS.md
"Verpflichtende Fehlerinjektionen/Sensoren"):
  - einzelner CRC-Fehler -> Stale, kein sofortiges Failed
  - mehrere aufeinanderfolgende CRC-Fehler bis kMaxConsecutiveInvalid
    ueberschritten -> Failed
  - BusFault-Sample -> gleiche Behandlung wie CRC, getrennt geloggte Ursache
  - MissingSample -> wie oben, eigene FaultReason
  - DS18B20-Einschaltwert (85.0) und Fehlerwert (-127.0) als rawCelsius bei
    transportStatus=Ok -> KnownFaultValue, ungueltig
  - NaN/Inf als rawCelsius (sofern durch den Testinjektor konstruierbar)
    -> OutOfRange

Zeit und Alter (Orakel: Abschnitt 9, SAFETY_COMPONENT_FAULTS.md
Sensorzustandsfolge):
  - Stale durch Altersgrenze (kMaxStaleAgeMs) ueberschritten -> Failed
  - Failed durch maximale Fehlerdauer -> bleibt Failed bis Wiedererkennung
  - verspaetete Probe wird verworfen, aendert Zustand nicht rueckwirkend
  - Zeitstempelruecksprung wird wie verspaetete Probe behandelt und als
    Verdachtsereignis markiert
  - identischer Zeitstempel + identischer Wert -> als Duplikat ignoriert
  - identischer Zeitstempel + unterschiedlicher Wert -> akzeptiert, keine
    Division durch Null in der Aenderungsratenberechnung
  - grosse Messluecke -> Uebergang wie in Abschnitt 8 beschrieben
  - Wiederaufnahme nach Luecke: erste Probe nach Luecke wird nicht als
    "Sprung" gegen den (veralteten) Vorwert bewertet

Wertebereich und Dynamik (Orakel: Abschnitt 10.2,
SENSOR_TUNING_COMMISSIONING.md "Plausibilitaetspruefungen"):
  - Wert unter kAbsoluteMinCelsius -> OutOfRange
  - Wert ueber kAbsoluteMaxCelsius -> OutOfRange
  - plausible langsame "Produktreaktion" (kleine Deltas ueber viele Zyklen)
    bleibt Valid, kein falscher Sprungalarm
  - Einzelspitze (ein Ausreisser-Sample) wird vom Medianfilter entfernt,
    filteredCelsius zeigt keinen Ausschlag, rawCelsius im Snapshot zeigt
    den Ausreisser weiterhin (Rohpfad bleibt sichtbar)
  - echter dauerhafter Sprung (mehrere Folgeproben auf neuem Niveau) bleibt
    NACH Einschwingen des Medianfensters sichtbar (kein dauerhaftes
    Verstecken eines echten Trends)
  - Aenderungsrate mit kurzem vs. langem Zeitabstand ergibt unterschiedliche
    Bewertung desselben absoluten Delta
  - dauerhaft nahezu unveraenderte "Produkt"-Konfiguration (dt gross, sehr
    kleines Delta ueber lange Zeit) bleibt erlaubt/Valid (kein
    Stillstands-als-Fehler-Kriterium in #20, siehe Abschnitt 11)

Median und Tiefpass (Orakel: Abschnitt 10.3/10.5):
  - ungefuelltes Medianfenster liefert Median aus den vorhandenen Werten
  - ungerade/gerade Fensterentscheidung: Test belegt, dass ein
    konfiguriertes gerades Fenster (falls ueberhaupt konstruierbar) entweder
    abgelehnt oder auf ungerade normalisiert wird (Entscheidung: nur
    ungerade Fenstergroessen sind gueltige Konfiguration, siehe Abschnitt
    10.3) – Test prueft die Ablehnung/den dokumentierten Vertrag
  - Einzelspitze wird entfernt (siehe oben, hier isoliert fuer den
    MedianFilter als Einzelkomponente getestet)
  - echter Trend bleibt ueber mehrere Zyklen im gefilterten Wert sichtbar
  - ungueltige Probe gelangt nachweislich NICHT ins Medianfenster
    (Fenster-Inhalt vor/nach einer ungueltigen Probe unveraendert)
  - Reset des Filterzustands nach Failed->Valid-Wiedererkennung und nach
    ROM-Wechsel (kein "Nachschleppen" alter Werte)
  - unterschiedliche tau-Werte zweier Konfigurationen ergeben messbar
    unterschiedliche Einschwingzeit bei identischer Sprungeingabe
  - extremer Rohwert bleibt in rawCelsius sichtbar, auch wenn
    filteredCelsius (noch) nicht nachgezogen ist

Offset und Identitaet (Orakel: Abschnitt 10.4, 8, 11):
  - Offset 0.0 -> correctedCelsius == gefilterte Eingabe vor Offset
  - positiver und negativer Offset veraendern correctedCelsius korrekt
  - Offset am Rand von kMaxAbsoluteOffsetCelsius wird noch akzeptiert
  - Offset ausserhalb der Grenze: Testfall dokumentiert den definierten
    Umgang (Konstruktionsstelle/Aufrufer, siehe Abschnitt 10.4)
  - fehlender Offset entspricht SensorOffset{0.0}
  - ROM-Wechsel (SensorIdentity aendert sich) -> IdentityMismatch-Ereignis,
    Filterzustand verworfen, Wiedererkennung wie nach Failed erforderlich
  - Rollenwechsel ist kein #20-Konzept (siehe Abschnitt 13) und daher NICHT
    Teil dieser Testgruppe; stattdessen wird eine zweite unabhaengige
    Pipeline-Instanz mit eigener Konfiguration als Ersatztest verwendet, um
    zu zeigen, dass zwei Instanzen sich nicht gegenseitig beeinflussen
  - Offsetaenderung waehrend laufender Filterung wirkt erst auf die naechste
    Probe (siehe 10.4), Testfall vergleicht Snapshot vor/nach Aenderung

Zustandsmaschine und Wiedererkennung (Orakel: Abschnitt 8):
  - Valid -> Stale bei einzelner ungueltiger Probe
  - Stale -> Valid nach kMinConsecutiveValidSamples UND
    kMinRecoveryStabilityDurationMs
  - Stale -> Failed bei Altersueberschreitung
  - Failed -> Wiedererkennung -> Valid (gleiche Bedingung wie Stale->Valid)
  - erneute ungueltige Probe waehrend Wiedererkennung setzt Fortschritt
    zurueck (recoveryProgressCount faellt auf 0, Zustand bleibt
    Stale/Failed)
  - keine Regelwertfreigabe (controlValueUsable) in Stale/Failed, in jedem
    einzelnen Testfall dieser Gruppe explizit geprueft
  - letzter gueltiger Wert bleibt im Snapshot sichtbar
    (diagnosticOnly == true), auch waehrend Stale/Failed

Widersprueche und Scopegrenzen (Orakel: Abschnitt 11, 7):
  - #20 trifft bei zwei unabhaengigen Pipeline-Instanzen mit stark
    abweichenden Werten (simuliert "Produkt" vs. "Luft") KEINE
    Schuldzuweisung – Test prueft, dass keine Pipeline-Instanz allein durch
    den Wert der anderen beeinflusst wird (Unabhaengigkeitsnachweis)
  - Test/Kommentar dokumentiert explizit, dass #20 keinen Ersatzsensor
    waehlt, keine Verriegelung setzt und keinen Aktorbefehl erzeugt (durch
    Abwesenheit entsprechender oeffentlicher API nachgewiesen, kein
    Laufzeittest noetig)

Robustheit (Orakel: allgemein, AGENTS.md Ressourcenregeln):
  - Parametergrenzen (kMaxMedianWindowSize, kMaxAbsoluteOffsetCelsius usw.)
    werden bei Konfiguration geprueft
  - Zaehlerueberlauf: consecutiveInvalidCount/recoveryProgressCount als
    uint16_t mit Saettigung statt Wraparound bewiesen (analoges Muster zu
    checkedIncrement, aber lokal einfacher: Saettigung reicht, da nur
    Vergleich gegen Schwellwerte noetig ist, kein persistenter Zaehler)
  - Zeitdifferenzueberlauf: monotonicTimestampMs nahe UINT64_MAX wird nicht
    fehlerhaft (Differenzbildung mit vorherigem Wert bleibt korrekt/saturiert
    analog VirtualTimeSource-Vertrag)
  - deterministische Wiederholung derselben Eingabefolge liefert exakt
    denselben Snapshot-Verlauf (reine Funktion der Eingabefolge + Konfig)
  - keine Abhaengigkeit von realer Uhrzeit oder Hardware (nur
    VirtualTimeSource in Tests)
  - keine deaktivierten/uebersprungenen Tests (Unity ohne TEST_IGNORE)
```

`TEST_MATRIX_COMPLETE: PASS`

## 18. Risiken und Gegenmassnahmen

```text
Risiko: RawSensorSample divergiert spaeter von dem, was ein realer
  DS18B20-Adapter (#30) tatsaechlich liefern kann (z. B. wenn CRC- und
  Busfehler auf Treiberebene nicht sauber trennbar sind).
  Gegenmassnahme: RawSensorSample bleibt ein reiner, absichtlich
  entkoppelter Werttyp; die Uebersetzung ist ausdruecklich #30s Aufgabe und
  dort mit dem dann echten Treiberverhalten abzugleichen. Portgrenze:
  ITemperatureSource bleibt unveraendert; #20 zwingt #30 NICHT, diesen Port
  zu erweitern, sondern laesst offen, ob #30 direkt RawSensorSample erzeugt
  oder ITemperatureSource dafuer erweitert wird - das ist explizit eine
  spaetere, in #30 zu treffende Entscheidung.

Risiko: Firmwarefeste Grenzen in sensor_limits.hpp werden ohne reale
  Messung zu eng oder zu weit gewaehlt.
  Gegenmassnahme: Grenzen bleiben bewusst konservativ/breit (reine
  Sicherheitsaussenkante), waehrend die eigentliche Abstimmung
  TBD_COMMISSIONING bleibt und ueber SensorQualityConfig (nicht durch
  Codeaenderung) erfolgt.

Risiko: Der Umfang (Zustandsmaschine + Plausibilitaet + 2 Filter +
  Diagnosevertrag + vollstaendige Testmatrix) ist fuer einen einzigen
  kleinen review-baren PR zu gross.
  Gegenmassnahme: siehe Abschnitt 19, vorgeschlagene Teilung A/B als offene
  Ownerentscheidung.

Risiko: Eine spaetere ADR-Notwendigkeit wird uebersehen.
  Gegenmassnahme: siehe Abschnitt 19, explizite offene Ownerentscheidung
  "Neue ADR noetig?".
```

## 19. Offene Ownerentscheidungen

### Entscheidung A: Aufteilung des Implementierungs-PR

```text
Frage: Wird #20 in genau einem Implementierungs-PR umgesetzt, oder in der
  vom Auftrag (Abschnitt 14) und vom Audit
  (docs/audits/PROPOSED_RELEASE_1_ROADMAP.md, Zeile "#20: Status-/
  Plausibilitaetsmodell, danach Filterpipeline") uebereinstimmend
  vorgeschlagenen Teilung A/B?
Warum nicht aus bestehenden Quellen abschliessend beantwortbar: AGENTS.md
  verlangt "kleine PRs", nennt aber keine Zeilen-/Dateiobergrenze; die
  Owner-Praeferenz fuer diesen konkreten Umfang ist nicht dokumentiert.
Optionen:
  1. EIN PR: SensorIdentity, RawSensorSample, SensorQuality-Zustandsmaschine,
     Plausibilitaet, Median, Offset, Tiefpass, Diagnosevertrag,
     vollstaendige Testmatrix, sensor_fault_sequence.hpp, in einem Schritt.
  2. Teilung A/B: A = SensorIdentity, RawSensorSample, SensorQuality-
     Zustandsmaschine, Plausibilitaet (10.1/10.2), Diagnosevertrag (ohne
     gefilterten Wert, filteredCelsius vorerst == correctedCelsius),
     zugehoerige Tests. B = Median, Offset, Tiefpass, vollstaendige
     Pipelineintegration, verbleibende Tests, sensor_fault_sequence.hpp
     (soweit fuer B-Tests benoetigt).
Empfehlung: Option 2 (Teilung A/B). Sowohl der Auftrag als auch der bereits
  akzeptierte Roadmap-Audit schlagen unabhaengig voneinander exakt diese
  Trennlinie vor; sie erzeugt zwei in sich abgeschlossene, ohne
  Uebergangs-API reviewbare Schritte (kein halbfertiger produktiver
  Zwischenzustand, da B lediglich filteredCelsius praezisiert, ohne A's
  bereits nutzbaren Zustandsmaschinen-/Plausibilitaetsvertrag zu brechen).
Auswirkung: Bestimmt, ob nach Planfreigabe ein oder zwei Commits/Reviewrunden
  im selben Draft-PR folgen. Keine Auswirkung auf den fachlichen Endzustand.
Blockierend: NEIN – ohne Owner-Antwort wird mit Option 2 (Teilung A/B)
  begonnen, da sie die im Auftrag selbst genannte Default-Erwartung ist.
```

### Entscheidung B: Neue ADR fuer Sensorqualitaet/-filterung?

```text
Frage: Soll die Umsetzung von #20 eine neue ADR in docs/DECISIONS.md
  anlegen, oder reichen die bereits akzeptierten Dokumente
  (ARCHITECTURE.md, SENSOR_TUNING_COMMISSIONING.md, SAFETY_COMPONENT_
  FAULTS.md, ADR-013) als Entscheidungsgrundlage?
Warum nicht aus bestehenden Quellen abschliessend beantwortbar: Es existiert
  keine ADR zu diesem Themenbereich; der Schwellenwert, ab dem ein neues
  Thema eine eigene ADR statt einer Ergaenzung bestehender Spezifikations-
  dokumente erhaelt, ist nicht formal definiert.
Optionen:
  1. Keine neue ADR: #20 implementiert eine bereits in ARCHITECTURE.md/
     SENSOR_TUNING_COMMISSIONING.md/SAFETY_COMPONENT_FAULTS.md
     spezifizierte Pipeline und Zustandsfolge; die Modulzuordnung ist
     bereits durch ADR-013 gedeckt (siehe Abschnitt 13). Es gibt keine neue,
     bisher unentschiedene Grundsatzfrage.
  2. Neue ADR: dokumentiert nachtraeglich die konkrete Typgestaltung
     (RawSensorSample-Entkopplung von ITemperatureSource, drei oeffentliche
     Zustaende statt vier, Modulzuordnung nach device_platform) als
     eigenstaendige Entscheidung.
Empfehlung: Option 1 (keine neue ADR). #20 ist Umsetzung bereits akzeptierter
  Architektur, keine neue Grundsatzentscheidung; die hier getroffenen
  Detailentscheidungen sind gemaess Abschnitt 15/16 des Auftrags "normale
  Implementierungsdetails", die im Plan begruendet werden.
Auswirkung: Bei Option 2 waere die Erstellung einer ADR zusaetzlicher, im
  Nicht-Scope (Abschnitt 7) aktuell ausgeschlossener Umfang und muesste vor
  Planfreigabe ergaenzt werden.
Blockierend: NEIN – ohne Owner-Antwort wird mit Option 1 (keine neue ADR)
  fortgefahren.
```

### Entscheidung C: Modulzuordnung device_platform vs. fermentation_app (blockierend)

```text
Frage: Liegt die gesamte fachliche Pipeline aus #20 (Zustandsmaschine,
  Plausibilitaet, Filter, Diagnosevertrag) in lib/device_platform/ (Plan-
  Default, Abschnitt 13) oder in lib/fermentation_app/, wie in Abschnitt 11
  des Auftrags illustriert?
Wortlaut des Auftrags (Abschnitt 11), beide Seiten zaehlen:
  "fermentation_app
   - fachliche Sensorqualitätszustände; Plausibilitätsregeln;
     Filterpipeline; Wiedererkennung; Diagnose- und Regelwertvertrag.

   device_platform
   - nur vorhandene generische Ports oder technische Grundtypen;"
  Die Formulierung "nur vorhandene" auf der device_platform-Seite ist als
  Beschraenkung lesbar (in #20 werden dort KEINE neuen Typen ergaenzt).
  Gleichzeitig erlaubt lib/device_platform/AGENTS.md im Abschnitt "Erlaubt"
  ausdruecklich "allgemeine Sensorqualitaet, Filter und begrenzte
  Reglerbausteine" - das ist eine Erlaubnis fuer das Modul, keine Zuweisung
  fuer DIESES Issue. Beide Aussagen sind gleichzeitig wahr; keine davon
  erzwingt zwingend die andere Lesart.
Warum nicht aus bestehenden Quellen abschliessend beantwortbar: Der Auftrag
  selbst enthaelt beide Signale nebeneinander (siehe Wortlaut oben); AGENTS.md
  und ADR-013 legen den GRUNDSATZ der Trennung fest, entscheiden aber nicht
  verbindlich, ob #20 konkret neue device_platform-Dateien anlegen darf.
Optionen:
  1. lib/device_platform/ (Plan-Default, Abschnitte 9-14 wie ausgearbeitet).
     Neun neue Dateien in device_platform + eine neue Testhilfe in
     device_platform_test_support. SensorQualitySnapshot enthaelt bewusst
     KEIN Rollenfeld (Abschnitt 12); die Zuordnung zu Schrankluft/Produkt/
     Kuehlkoerper geschieht erst dort, wo mehrere Pipeline-Instanzen benannt
     gehalten werden (spaeter, ausserhalb #20).
  2. lib/fermentation_app/: dieselbe fachliche Pipeline, aber als
     fermentation_app-interne Klassen. SensorQualitySnapshot koennte dann
     ZUSAETZLICH ein Sensorrolle-Feld (Schrankluft/Produkt/Kuehlkoerper)
     tragen, da fermentation_app geraetespezifische Rollen kennen darf.
     ITimeSource bliebe weiterhin die einzige device_platform-Abhaengigkeit
     (erlaubt laut lib/fermentation_app/AGENTS.md: "schmale Schnittstellen
     aus device_platform"). Kein Ports-/Adapter-Neubau noetig, aber die
     Pipeline waere fuer andere Geraetetypen (Smoker, Gewaechshaus) nicht
     wiederverwendbar, ohne sie spaeter nachtraeglich zu verschieben.
Empfehlung: Option 1 (device_platform), Begruendung wie in Abschnitt 13
  ausgefuehrt (ADR-013-Wiederverwendbarkeit, explizite Erlaubnis in
  lib/device_platform/AGENTS.md, keine Fermentationsbegriffe im gesamten
  #20-Scope). Bei Owner-Entscheidung fuer Option 2 werden Abschnitte 12
  (Sensorrolle-Feld ergaenzen), 13, 14 und 20 dieses Plans materiell
  geaendert und muessen vor erneuter Freigabe ueberarbeitet werden
  (Dateipfade lib/fermentation_app/src/... statt lib/device_platform/src/...,
  keine device_platform_test_support-Ergaenzung, ggf. abweichende
  Testverzeichniskonvention).
Auswirkung: Bestimmt jeden Dateipfad in Abschnitt 14 und die gesamte
  Reihenfolge in Abschnitt 20; nicht nachtraeglich ohne Planaenderung
  korrigierbar.
Blockierend: JA. Eine "PLAN APPROVED"-Freigabe dieses exakten Plan-Commits
  gilt als Zustimmung zu Option 1 (dem hier ausgearbeiteten Default). Ist
  Option 2 gewuenscht, muss der Owner dies vor der Freigabe ausdruecklich
  mitteilen; in diesem Fall wird der Plan materiell ueberarbeitet und erneut
  zur Freigabe vorgelegt (kein stiller Wechsel nach Freigabe, siehe AGENTS.md
  Abschnitt "Materielle Planabweichungen").
```

`OWNER_DECISIONS_OPEN: 3`
`IMPLEMENTATION_BLOCKED_BY_OWNER_DECISION: YES (Entscheidung C; Optionen A und B sind nicht blockierend und werden per Default fortgefuehrt, falls der Owner sie nicht ausdruecklich anders entscheidet)`

## 20. Genaue Implementierungsreihenfolge (nach Planfreigabe)

```text
Slice A (siehe Entscheidung A, Default):
  1. sensor_limits.hpp (firmwarefeste Grenzen)
  2. sensor_identity.hpp
  3. sensor_sample.hpp
  4. sensor_quality.hpp
  5. sensor_quality_snapshot.hpp (ohne gefilterten Wert / mit
     filteredCelsius == correctedCelsius als Platzhalterverhalten,
     dokumentiert)
  6. sensor_quality_pipeline.hpp/.cpp: Transport-/CRC-/Wertebereichs-
     /Aenderungsratenpruefung + Zustandsmaschine (ohne Median/Offset/
     Tiefpass)
  7. sensor_fault_sequence.hpp/.cpp (so weit fuer Slice-A-Tests benoetigt)
  8. test/test_sensor_quality/test_sensor_quality.cpp: Start/Normalbetrieb,
     Transport-/Messfehler, Zeit/Alter, Wertebereich, Zustandsmaschine/
     Wiedererkennung, Robustheit (Teilmenge ohne Median/Tiefpass-
     spezifische Faelle)
  9. Dokumentation/Changelog-Eintrag fuer Slice A
  10. Ressourcen-/CI-Nachweise fuer Slice A

Slice B:
  11. sensor_offset.hpp
  12. sensor_median_filter.hpp/.cpp
  13. sensor_lowpass_filter.hpp/.cpp
  14. sensor_quality_pipeline.hpp/.cpp: Integration von Offset/Median/
      Tiefpass, ROM-Wechsel-Filterreset
  15. verbleibende sensor_fault_sequence.hpp/.cpp-Ergaenzungen
  16. verbleibende Testfaelle: Median, Tiefpass, Offset, Identitaet/
      ROM-Wechsel, Widersprueche/Scopegrenzen
  17. Dokumentation/Changelog-Eintrag fuer Slice B
  18. Ressourcen-/CI-Nachweise fuer Slice B, vollstaendiger Testlauf
```

Bei Owner-Entscheidung fuer Option 1 (ein PR) entfaellt die Trennung; die
Reihenfolge 1-8 und 11-16 wird ohne Zwischenstopp durchlaufen.

## 21. Stopbedingung und freizugebender Plan-Commit

```text
Nach Commit und Push dieser Plan-Datei sowie Erstellung des Draft-PR:
HALTED_FOR_OWNER_REVIEW

Die Implementierung beginnt erst nach einem Ownerkommentar der Form:

PLAN APPROVED
Approved plan commit: <exakter Commit-SHA dieser Datei>
```

## Taskleiste

```text
/task
[x] aktuellen main-SHA und sauberen Arbeitsbaum verifizieren
[x] Live-Issue #20 inklusive Kommentare vollständig lesen
[x] Abhängigkeiten #10 und #11 live als abgeschlossen verifizieren
[x] AGENTS.md und alle betroffenen Unterverzeichnisregeln lesen
[x] kanonische Spezifikations- und Architekturquellen vollständig lesen
[x] bestehenden Sensor-, Temperatur-, Zeit-, Mock- und Testcode inventarisieren
[x] bestehende starke Typen und wiederverwendbare Bausteine identifizieren
[x] Scopegrenzen zu #21, #24 und #30 explizit festlegen
[x] offene fachliche oder sicherheitsrelevante Entscheidungen herausarbeiten
[x] konkreten SOLID-/DRY-/KISS-Implementierungsplan erstellen
[x] vollständige Test- und Fehlermatrix planen
[x] Ressourcen-, Laufzeit- und Speichergrenzen planen
[x] erwarteten Datei- und Modul-Diff begründen
[x] versionierte Plan-Datei erstellen und committen
[x] Draft-PR gegen main erstellen
[x] PR-Beschreibung mit vollständiger Abschlussmatrix ergänzen
[x] git diff --check ausführen
[x] reinen Markdown-Diff nachweisen
[x] PR als Draft belassen
[x] HALTED_FOR_OWNER_REVIEW
```
