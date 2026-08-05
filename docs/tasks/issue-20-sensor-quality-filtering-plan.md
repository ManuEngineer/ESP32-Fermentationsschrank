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

Abschnittszuordnung zu den 21 in Abschnitt 16 des urspruenglichen
Plan-Auftrags geforderten Pflichtinhalten (Reihenfolge hier teils
zusammengefasst/umbenannt, aber vollstaendig): 1=Abschnitt 1, 2=Abschnitt 2,
3=Abschnitt 3, 4=Abschnitt 4, 5=Abschnitt 5, 6=Abschnitt 6, 7=Abschnitt 7,
8=Abschnitt 8, 9=Abschnitt 9, 10=Abschnitt 10, 11=Abschnitt 12 (Ausgabe-/
Diagnosevertrag), 12=Abschnitt 13 (Modul-/Abhaengigkeitsrichtung),
13=Abschnitt 15 (SOLID/DRY/KISS), 14=Abschnitt 14 (geplante Dateien),
15=Abschnitt 17 (Testmatrix), 16=Abschnitt 16 (Ressourcen/CI),
17=Abschnitt 18 (Risiken), 18=Abschnitt 19 (Punkte A/B/C - inzwischen
entschieden, siehe unten), 19=Abschnitt 20 (Implementierungsreihenfolge),
20=Abschnitt 21 (Stopbedingung), 21=Taskleiste am Dateiende. Abschnitt 11
dieser Datei (Widerspruchs-/Plausibilitaetsbewertung) ist eine
Praezisierung von Abschnitt 8/10 des urspruenglichen Auftrags (dort explizit
als Pflichtinhalt "Zustandsmodell" und "Verarbeitungspipeline" gefordert)
und keine eigene der 21 Pflichtnummern.

### Nachkorrektur (PR #95, "Kurze Nachkorrektur")

```text
PREVIOUS_PLAN_COMMIT: f52741170726dafc6d6ea056f55c3cb04c095de2
PREVIOUS_HEAD: 16dc12d661a7c387193c141ba1f0d18bc556ee25
```

Dieser Plan-Stand ist eine gezielte Nachkorrektur des oben genannten
vorherigen Plan-Commits, ausgeloest durch den Auftrag
"AUFTRAG_PR95_Kurze_Nachkorrektur.md". Die vorherige Version wurde noch
nicht freigegeben (keine "PLAN APPROVED"-Ownerfreigabe lag vor); es handelt
sich daher um eine gewoehnliche Plankorrektur vor Erstfreigabe, nicht um
eine materielle Abweichung von einem bereits freigegebenen Plan. Jede
inhaltliche Aenderung ist an ihrer jeweiligen Stelle im Dokument als
"Korrektur gegenueber der Vorversion" markiert. Entscheidungen A, B und C
(vormals Abschnitt 19 "Offene Ownerentscheidungen") sind seit dieser
Nachkorrektur verbindlich entschieden, siehe Abschnitt 19.

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
- StrongId<Tag,Underlying>-Muster (storage_types.hpp): dient nur als
  STILISTISCHE Vorlage fuer den neuen, eigenstaendigen SensorIdentity-Typ;
  SensorIdentity haengt bewusst NICHT auf storage_types.hpp (siehe
  Abschnitt 13a – unpassende Domaenenkopplung, AGENTS.md-DRY-Ausnahme fuer
  oberflaechlich aehnlichen, fachlich fremden Code).
- StateStoreKey-Muster ("gueltig-by-construction" mit privatem Konstruktor und
  statischem create()): Vorlage fuer den Kalibrier-Offset-Werttyp
  SensorOffset (Abschnitt 13a), das die firmwarefeste Offsetgrenze bereits
  bei Erzeugung durchsetzt.
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
-> Zeitstempel-/Dispositionspruefung (Abschnitt 9b)
-> Bus-/Transportstatus (generisch, keine sensor-/treiberspezifischen
   Konstanten, Abschnitt 10.1)
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
Abschnitt 4/18 "Portgrenze"). Neuer, von `ITemperatureSource` entkoppelter,
bewusst generischer Werttyp in `device_platform` (Korrektur gegenueber der
Vorversion: kein `StrongId`-Import aus `storage_types.hpp`, kein
`driverFaultCode` ohne nachgewiesenen Konsumenten, roher Messwert optional
statt eines immer-vorhandenen, nur bedingt gueltigen `double` – siehe
Abschnitt 9a/9b und Abschnitt 13 fuer die Begruendung der
`SensorIdentity`-Eigenstaendigkeit):

```cpp
// sensor_sample.hpp
enum class SensorTransportStatus : uint8_t {
    Ok,
    BusFault,
    CrcFault,
    MissingSample,
    // Der (spaetere) Adapter hat den Rohwert bereits selbst als einen ihm
    // bekannten ungueltigen Messwert erkannt (z. B. einen
    // Sensor-/Treiber-spezifischen Einschalt- oder Diskonnektwert). Die
    // KONKRETE Erkennung bleibt Aufgabe des jeweiligen Adapters (#30);
    // #20 kennt nur diesen generischen Status, keine Sensor-/
    // Treiberkonstanten (siehe Abschnitt 10.1).
    KnownInvalidMeasurement,
};

struct RawSensorSample {
    SensorIdentity identity;
    uint64_t monotonicTimestampMs;      // Erfassungszeit dieser Probe
    std::optional<double> rawCelsius;   // nullopt, wenn transportStatus != Ok
    SensorTransportStatus transportStatus;
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

`driverFaultCode` (Vorversion) entfaellt ersatzlos: kein Konsument innerhalb
von #20 verwendet dieses Feld (auch der bisherige Diagnosevertrag,
Abschnitt 12, hat es nie gelesen). Ein roher Treibercode ohne nachgewiesenen
heutigen Konsumenten ist spekulative Flaeche und wird bei Bedarf mit dem
Issue ergaenzt, das ihn tatsaechlich braucht (voraussichtlich #30).

### 9a. Einziger monotoner Zeitvertrag

```text
Es existiert GENAU EINE Uhr im Gesamtsystem (produktiv: die konkrete
ITimeSource-Instanz der Composition Root; nativ: VirtualTimeSource). Ihr
monotoner Millisekundenwert fliesst AUSSCHLIESSLICH als expliziter Parameter
in die neuen #20-Typen ein - niemals als gespeicherte/injizierte
ITimeSource-Abhaengigkeit innerhalb von SensorQualityPipeline (Korrektur:
die Vorversion nannte in Abschnitt 14 faelschlich eine zusaetzliche, in der
Pipeline gehaltene "ITimeSource-Referenz" - das war eine versteckte zweite
Zeitquelle und entfaellt ersatzlos).

Zwei Eintrittspunkte, beide mit explizitem Zeitbezug:

  RawSensorSample.monotonicTimestampMs
    - die Erfassungszeit EINER Probe, vom Aufrufer beim Erzeugen der Probe
      aus der einen Uhr gelesen.

  SensorQualityPipeline::ingest(const RawSensorSample& sample,
                                 uint64_t nowMonotonicMs)
    - nowMonotonicMs ist derselbe Uhrwert, zum Zeitpunkt des ingest()-Aufrufs
      vom Aufrufer gelesen; dient AUSSCHLIESSLICH der Zukunftspruefung
      (Abschnitt 9b) und wird nicht gespeichert.

  SensorQualityPipeline::snapshot(uint64_t nowMonotonicMs) const
    - liefert Altersfelder (lastSampleAgeMs/lastValidSampleAgeMs) relativ zu
      diesem explizit uebergebenen "jetzt", nicht relativ zu einer intern
      gespeicherten Uhr. Ein Aufrufer, der lange nicht snapshot() aufruft,
      bekommt beim naechsten Aufruf trotzdem ein korrektes, aktuelles Alter.

SensorQualityPipeline selbst haelt KEIN ITimeSource-Feld, keine Referenz und
keinen Zeiger auf eine Uhr - sie ist bezueglich Zeit eine reine Funktion der
uebergebenen Werte. Das erfuellt Dependency Inversion (Abschnitt 15, Punkt D)
sogar staerker als in der Vorversion.
```

### 9b. Zeitstempel- und Dispositionsregeln

```text
Jede eingehende Probe erhaelt GENAU EINE der folgenden Dispositionen (siehe
SampleDisposition, Abschnitt 12); erst bei Accepted durchlaeuft sie die
Plausibilitaetsstufen aus Abschnitt 10:

  identischer Zeitstempel + identischer Rohwert wie die zuletzt akzeptierte
    Probe -> DuplicateIgnored. Kein Effekt auf Alter, Zustandsmaschine oder
    Filterinhalt (reiner Doppelversand, keine neue Information).

  identischer Zeitstempel + abweichender Rohwert (oder abweichender
    transportStatus) wie die zuletzt akzeptierte Probe -> RejectedTimestampConflict.
    Zwei widerspruechliche Werte fuer denselben Erfassungszeitpunkt sind kein
    gueltiger Messvorgang; es wird KEINE der beiden Varianten bevorzugt
    akzeptiert (Korrektur gegenueber der Vorversion, die dies faelschlich als
    naechste Probe akzeptiert und dafuer eine kuenstliche
    Mindest-Zeitdifferenz fuer die Aenderungsratenpruefung noetig gemacht
    haette). Da RejectedTimestampConflict-Proben nie akzeptiert werden,
    bleiben aufeinanderfolgende akzeptierte Zeitstempel immer strikt
    steigend; die Aenderungsratenpruefung (Abschnitt 10.2) braucht deshalb
    keine erfundene Mindest-Zeitdifferenz, sondern kann echte Division durch
    Null durch Konstruktion ausschliessen.

  Zeitstempel < zuletzt akzeptierter Zeitstempel (ruecklaeufig/verspaetet)
    -> RejectedRetrograde.

  Zeitstempel > nowMonotonicMs (aus der Zukunft, siehe Abschnitt 9a) ->
    RejectedFuture.

  sonst (Zeitstempel > zuletzt akzeptierter Zeitstempel UND
    <= nowMonotonicMs) -> Accepted, durchlaeuft Abschnitt 10.

Keine der Ablehnungsdispositionen (DuplicateIgnored, RejectedTimestampConflict,
RejectedRetrograde, RejectedFuture) zaehlt als "ungueltige Probe" im Sinne der
Zustandsmaschine (Abschnitt 8): Es sind Transport-/Protokollanomalien der
Zufuhr, keine Evidenz gegen die Sensorgesundheit. Nur eine tatsaechlich
Accepted, aber danach in Abschnitt 10.1/10.2 als unplausibel erkannte Probe
zaehlt in consecutiveInvalidCount.

Weitere behandelte Faelle:

  extrem grosse Zeitluecke: erlaubt (kein Fehler an sich, Accepted), fuehrt
    aber bei Ueberschreiten von kMaxStaleAgeMs zum Zustandsuebergang wie in
    Abschnitt 8 beschrieben; nach der Luecke beginnt die
    Aenderungsratenpruefung ohne Vorwert (erste Probe nach Luecke kann nicht
    als "Sprung" bewertet werden, da kein unmittelbar vorheriger gueltiger
    Wert existiert).
  Sensor-/ROM-Wechsel: siehe Abschnitt 8 und 11.
  nicht endlicher Wert (NaN/Inf) in rawCelsius: Accepted auf Zeitstempelebene
    (Zeitstempelregeln kennen den Rohwert nicht), aber in Abschnitt 10.2 als
    eigene SensorFaultReason::NonFinite erkannt (std::isfinite) - siehe
    Abschnitt 12 fuer den vervollstaendigten Enum.
```

`INPUT_CONTRACT_DEFINED: PASS`
`GENERIC_INPUT_CONTRACT: PASS`
`SINGLE_MONOTONIC_TIME_CONTRACT: PASS`

## 10. Verarbeitungspipeline

Reihenfolge exakt wie in Abschnitt 5 des Auftrags und in
`docs/ARCHITECTURE.md`/`docs/SENSOR_TUNING_COMMISSIONING.md` vorgegeben.
Jede Stufe ist eine eigene kleine, testbare Komponente (SRP), orchestriert
von einer duennen `SensorQualityPipeline`. Eine Probe durchlaeuft Abschnitt
10 ueberhaupt nur, wenn sie gemaess Abschnitt 9b als `Accepted` disponiert
wurde.

### 10.0 SensorQualityConfig (vollstaendig, gueltig-by-construction)

Korrektur gegenueber der Vorversion: `SensorQualityConfig` wurde bisher nur
an mehreren Stellen ERWAEHNT, aber nie als konkreter Typ definiert. Nach dem
`StateStoreKey`-Muster (privater Konstruktor, statisches `create()`,
`...CreateResult{status; optional<T>}`) validiert die Konfiguration alle
Instanzparameter bereits bei Erzeugung, statt fehlerhafte Kombinationen erst
zur Laufzeit der Pipeline zu bemerken:

```cpp
// sensor_quality_config.hpp
enum class SensorQualityConfigStatus : uint8_t {
    Success,
    InvalidMedianWindowSize,     // 0, gerade, oder > sensor_limits::kMaxMedianWindowSize
    InvalidLowPassTimeConstant,  // <= 0.0
    InvalidPlausibleRange,       // min >= max, oder ausserhalb sensor_limits-Aussengrenze
    InvalidRateOfChangeLimit,    // <= 0.0
    InvalidStaleAgeThreshold,    // 0, oder > sensor_limits::kMaxStaleAgeCeilingMs
    InvalidConsecutiveInvalidLimit, // 0, oder > sensor_limits-Obergrenze
    InvalidRecoveryThresholds,   // minConsecutiveValidSamples == 0
};

struct SensorQualityConfigCreateResult;  // vorwaertsdeklariert

class SensorQualityConfig {
   public:
    [[nodiscard]] static SensorQualityConfigCreateResult create(
        std::size_t medianWindowSize,
        double lowPassTauSeconds,
        double minPlausibleCelsius,
        double maxPlausibleCelsius,
        double maxRateOfChangeCelsiusPerSecond,
        uint64_t maxStaleAgeMs,
        uint16_t maxConsecutiveInvalid,
        uint16_t minConsecutiveValidSamples,
        uint64_t minRecoveryStabilityDurationMs);

    // schreibgeschuetzte Zugriffsmethoden je Feld, analog StateStoreKey::bytes()
   private:
    explicit SensorQualityConfig(/* ... bereits geprueft ... */);
    // Felder wie oben
};

struct SensorQualityConfigCreateResult {
    SensorQualityConfigStatus status{SensorQualityConfigStatus::InvalidMedianWindowSize};
    std::optional<SensorQualityConfig> config;
};
```

`create()` prueft ausschliesslich gegen firmwarefeste Aussengrenzen aus
`sensor_limits.hpp` (siehe Abschnitt 14); die konkreten Tuning-Werte selbst
(Fenstergroesse, Tau, Plausibilitaetsband, Ratenlimit, Alters-/
Wiedererkennungsschwellen) bleiben `TBD_COMMISSIONING` (Abschnitt 18) und
werden ausschliesslich ueber diese Konfiguration injiziert, nie im Pipeline-
Code selbst verzweigt (siehe Abschnitt 15, Punkt O). `SensorOffset`
(Abschnitt 13a) ist bewusst NICHT Teil von `SensorQualityConfig`: Es ist ein
Kalibrierwert pro Probe/Sensorinstanz, keine Pipeline-Verhaltensparametrierung
(SRP-Trennung).

`SENSOR_QUALITY_CONFIG_DEFINED: PASS`

### 10.1 Transport-/CRC-/Messstatusstufe (generisch, keine sensor-/treiberspezifischen Konstanten)

```text
transportStatus != Ok -> Probe ungueltig, Ursache uebernimmt direkt den
  Transportstatus (BusFault/CrcFault/MissingSample/KnownInvalidMeasurement,
  siehe Abschnitt 9/12), keine weitere Stufe verarbeitet rawCelsius.

Korrektur gegenueber der Vorversion: #20 selbst kennt KEINE
sensor-/treiberspezifischen Zahlenkonstanten (z. B. keinen DS18B20-
Einschalt- oder Diskonnektwert) und sensor_limits.hpp enthaelt keine solche
Konstante. Ein spaeterer Adapter (#30) erkennt sensor-/treiberspezifische
bekannte Fehlerwerte selbst und liefert dafuer bereits
transportStatus = KnownInvalidMeasurement; #20 behandelt diesen Status
generisch wie jeden anderen Transportfehler. Damit bleibt device_platform
frei von Sensor-/Treiberkenntnis (ADR-013, lib/device_platform/AGENTS.md).
```

`DS18B20_CONSTANTS_OUTSIDE_GENERIC_PLATFORM: PASS`

### 10.2 Physikalischer Wertebereich und Aenderungsrate

```text
Nur erreicht, wenn transportStatus == Ok UND rawCelsius.has_value() (durch
Konstruktion immer gemeinsam wahr, siehe Abschnitt 9).

nicht endlicher Rohwert (!std::isfinite(*rawCelsius)) -> ungueltig,
  Ursache = NonFinite.
rawCelsius ausserhalb [kAbsoluteMinCelsius, kAbsoluteMaxCelsius]
  (firmwarefest, sensor_limits.hpp) -> ungueltig, Ursache = OutOfRange.
Aenderungsrate = |rawCelsius - letzterGueltigerRohwert| /
  ((timestampMs - letzterGueltigerTimestampMs) / 1000.0), nur berechnet wenn
  ein unmittelbar vorheriger gueltiger Wert existiert. Da RejectedTimestampConflict-
  Proben (Abschnitt 9b) nie akzeptiert werden, ist die Zeitdifferenz zwischen
  zwei aufeinanderfolgenden akzeptierten Proben immer echt > 0 - keine
  kuenstliche Mindest-Zeitdifferenz noetig (Korrektur gegenueber der
  Vorversion). Ueberschreitet die Rate den in SensorQualityConfig
  konfigurierten kMaxRateOfChangeCelsiusPerSecond -> ungueltig,
  Ursache = RateOfChangeExceeded. Ein einzelner Sprung fuehrt NICHT sofort zu
  FAILED (siehe Akzeptanzkriterium "einzelne Fehlerwerte stoppen nicht sofort
  dauerhaft"), sondern zaehlt als eine ungueltige Probe im Sinne von
  Abschnitt 8.
```

### 10.3 Medianfilter

```text
Fixe Kapazitaet kMedianWindowSize (aus SensorQualityConfig, Abschnitt 10.0;
ungerade, validiert innerhalb eines firmwarefesten Maximalfensters
kMaxMedianWindowSize, sensor_limits.hpp).
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

`SensorOffset` ist bereits in Abschnitt 13a vollstaendig und
gueltig-by-construction definiert (keine erneute Definition hier).

```text
#20 nimmt ausschliesslich einen bereits erfolgreich per
SensorOffset::create() erzeugten Wert entgegen; #20 selbst validiert nicht
erneut und nimmt keine Persistenz- oder UI-Validierung vor (siehe
Nicht-Scope). Fehlender Offset = SensorOffset::create(0.0) (neutral, immer
erfolgreich). Ein Offsetwechsel waehrend laufender Filterung wirkt erst auf
die naechste eingehende Probe; bereits im Medianfenster befindliche
Rohwerte werden NICHT rueckwirkend korrigiert (Medianfenster enthaelt
Rohwerte, Offset wird NACH dem Medianfilter angewendet, siehe Reihenfolge
oben – ein Offsetwechsel kann daher nie zu einer Vermischung unterschiedlich
korrigierter Werte innerhalb eines Medianfensters fuehren).
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
  tauSeconds-Werte in der je Instanz uebergebenen SensorQualityConfig (siehe
  Abschnitt 10.0) – keine rollenspezifische Codeverzweigung (DRY).
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
      {BusFault, CrcFault, MissingSample, KnownInvalidMeasurement, NonFinite,
      OutOfRange, RateOfChangeExceeded, IdentityMismatch} (Abschnitt 12),
      erzeugt durch die Pipeline selbst (Abschnitt 10.1/10.2). Getrennt davon
      beschreibt SampleDisposition (Abschnitt 9b/12) reine Zeitstempel-/
      Zufuhranomalien (Duplikat, Zeitkonflikt, ruecklaeufig, aus der
      Zukunft), die KEINE Sensorfehlerevidenz sind.

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

Zusammengefasst (Klarstellung gemaess Nachkorrekturauftrag Punkt 11): #20
liefert ausschliesslich Evidenz je EINZELNER Pipeline-Instanz. Jede
rollenuebergreifende Bewertung, jeder Vergleich zweier Rollen und jede
daraus abgeleitete Schuldzuweisung oder Verdaechtigung bleibt vollstaendig
bei #21/#24 (siehe auch Abschnitt 7, Scopegrenzen).

`CROSS_ROLE_SCOPE_CONSISTENT: PASS`

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

Korrektur gegenueber der Vorversion, zwei getrennte kleine Enums statt
eines vermischten (Punkt 9 des Nachkorrekturauftrags): `SampleDisposition`
beschreibt, was mit EINER eingehenden Probe auf Zeitstempel-/Protokollebene
geschah (Abschnitt 9b); `SensorFaultReason` beschreibt den zuletzt
massgeblichen fachlichen Grund, warum der SENSOR aktuell nicht `Valid` ist
(Abschnitt 8/10). Beides zu vermischen wuerde SRP verletzen (zwei
unterschiedliche Fragen in einem Typ).

```cpp
// sensor_sample.hpp (Ergaenzung zu Abschnitt 9)
enum class SampleDisposition : uint8_t {
    Accepted,
    DuplicateIgnored,
    RejectedTimestampConflict,
    RejectedRetrograde,
    RejectedFuture,
};

// sensor_quality_snapshot.hpp
enum class SensorFaultReason : uint8_t {
    None,
    BusFault,
    CrcFault,
    MissingSample,
    KnownInvalidMeasurement,
    NonFinite,
    OutOfRange,
    RateOfChangeExceeded,
    IdentityMismatch,
};

struct SensorQualitySnapshot {
    SensorIdentity identity;
    SensorQuality quality;
    std::optional<double> rawCelsius;              // nullopt vor der ersten Accepted-Probe mit Rohwert
    std::optional<double> correctedCelsius;         // rawCelsius + Offset; nullopt wenn rawCelsius nullopt ist
    std::optional<double> filteredCelsius;          // nullopt bis der Tiefpass einen ersten Wert erzeugt hat
    double appliedOffset;
    std::optional<uint64_t> lastSampleAgeMs;        // nullopt vor der ersten jemals eingegangenen Probe
    std::optional<uint64_t> lastValidSampleAgeMs;   // nullopt vor der ersten gueltigen Probe
    SensorFaultReason lastFaultReason;
    uint16_t consecutiveInvalidCount;
    uint16_t recoveryProgressCount;   // aufeinanderfolgende gueltige Proben waehrend Wiedererkennung
    std::optional<double> changeRateCelsiusPerSecond; // nullopt ohne unmittelbaren gueltigen Vorwert
};
```

Korrektur gegenueber der Vorversion (Punkt 8 des Nachkorrekturauftrags):
`controlValueUsable` und `diagnosticOnly` entfallen ersatzlos. Beide waren
redundant zu bereits vorhandenen Feldern und haetten bei unsorgfaeltiger
Pflege der Pipeline-Implementierung aus dem Takt geraten koennen (zwei
Quellen der Wahrheit fuer dieselbe Aussage, DRY-Verstoss). Der Regelwert
wird stattdessen von JEDEM Konsumenten direkt und ausschliesslich aus
bereits vorhandenen Feldern abgeleitet:

```text
Regelwert verwendbar  <=>  quality == SensorQuality::Valid
                           UND filteredCelsius.has_value()
```

Kein Konsument darf einen Regelwert verwenden, wenn diese Bedingung nicht
erfuellt ist; `quality != Valid` ODER ein (noch) fehlender Filterwert
(z. B. unmittelbar nach einer Wiedererkennung, bevor der Tiefpass erneut
eingeschwungen ist) bedeuten beide "kein Regelwert", ohne dass ein
zusaetzliches gespeichertes Flag dafuer noetig ist.

Kein Trend-/Aenderungsratenfeld wird weggelassen: `SAFETY_COMPONENT_FAULTS.md`
verlangt fuer den Kuehlkoerpersensor ausdruecklich eine Aenderungsraten-
/Trendueberwachung ("ueberwacht Temperatur und Aenderungsrate der
Leistungsbaugruppe") – das Feld ist daher verpflichtend (als `optional`,
siehe oben), nicht ersatzlos gestrichen.

`DIAGNOSTIC_CONTRACT_DEFINED: PASS`
`OPTIONAL_DIAGNOSTIC_VALUES: PASS`
`REDUNDANT_STATUS_FLAGS_REMOVED: PASS`

## 13. Modul- und Abhaengigkeitsrichtung

Die GESAMTE fachliche Pipeline aus #20 (Zustandsmaschine, Plausibilitaet,
Filter, Diagnosevertrag) wird in `lib/device_platform/` verortet, nicht in
`lib/fermentation_app/`. Dies ist Entscheidung C aus Abschnitt 19 und durch
den Nachkorrekturauftrag zu PR #95 ausdruecklich entschieden (nicht mehr nur
eine vom Plan selbst vorgeschlagene Default-Lesart).

Begruendung:

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
  sensor_identity.hpp            (neu, eigenstaendig, siehe Abschnitt 13a)
  sensor_sample.hpp               (neu, RawSensorSample + SampleDisposition)
  sensor_quality.hpp              (neu, enum SensorQuality)
  sensor_quality_config.hpp       (neu, siehe Abschnitt 10.0)
  sensor_quality_snapshot.hpp     (neu, enum SensorFaultReason + Snapshot)
  sensor_offset.hpp               (neu, siehe Abschnitt 13a)
  sensor_median_filter.hpp/.cpp   (neu)
  sensor_lowpass_filter.hpp/.cpp  (neu)
  sensor_quality_pipeline.hpp/.cpp (neu, Orchestrator)
  sensor_limits.hpp               (neu, firmwarefeste Grenzen)

lib/device_platform_test_support/src/
  sensor_fault_sequence.hpp/.cpp (neu, deterministischer Mehrfach-
                                   Fehlerinjektor: geskriptete Folge von
                                   RawSensorSample inkl. Bus-/CRC-/
                                   Zeitstempelanomalien fuer Tests; wird
                                   ausschliesslich vom Pipeline-Testtopic
                                   verwendet, siehe Abschnitt 17a)

test/ (siehe Abschnitt 17a fuer die vollstaendige, nach Topics aufgeteilte
       Liste statt einer einzelnen Testdatei)
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

### 13a. SensorIdentity und SensorOffset: gueltig-by-construction, ohne storage_types.hpp-Abhaengigkeit

Korrektur gegenueber der Vorversion: `SensorIdentity` wurde bisher als
`using SensorIdentity = StrongId<detail::SensorIdentityTag, uint64_t>;`
geplant und haette damit `storage_types.hpp` (thematisch: Speicher-/
Persistenzbezeichner wie `StorageEpoch`, `Revision`, `SlotId`) in den
Sensorcode importiert. Das ist eine unpassende Modulabhaengigkeit: ein Leser
von `sensor_identity.hpp` muesste sich fragen, warum ein Sensorwerttyp vom
Speichermodul abhaengt, obwohl beide Domaenen nichts miteinander zu tun
haben. AGENTS.md haelt fuer genau diesen Fall fest: "DRY verlangt keine
gemeinsame Abstraktion fuer nur oberflaechlich aehnlichen Code mit
unterschiedlichen fachlichen Verantwortungen." Die oberflaechliche
Aehnlichkeit ("beides ist ein starker Ganzzahl-Wrapper") rechtfertigt hier
keine Modulkopplung.

```cpp
// sensor_identity.hpp – eigenstaendig, keine Abhaengigkeit auf storage_types.hpp
class SensorIdentity {
   public:
    constexpr SensorIdentity() = default;              // 0 = unbekannte Identitaet
    constexpr explicit SensorIdentity(uint64_t value) : value_(value) {}
    [[nodiscard]] constexpr uint64_t value() const { return value_; }
    friend constexpr bool operator==(SensorIdentity a, SensorIdentity b) {
        return a.value_ == b.value_;
    }
    friend constexpr bool operator!=(SensorIdentity a, SensorIdentity b) {
        return !(a == b);
    }
   private:
    uint64_t value_{0};
};
```

Kein `create()`/Validierungsvertrag noetig: jeder darstellbare `uint64_t`-Wert
ist eine gueltige Identitaet (0 eingeschlossen, als bewusst gueltiger
"unbekannt"-Sentinel, analog zum bestehenden `StorageEpoch`-Muster, aber
lokal reimplementiert statt importiert). "Gueltig-by-construction" bedeutet
hier: ein eigenstaendiger, nicht mit rohen `uint64_t` verwechselbarer Typ
ohne jede erreichbare inkonsistente Zwischenrepraesentation – nicht
zusaetzliche Ablehnungslogik, fuer die es keinen fachlichen Grund gibt.

```cpp
// sensor_offset.hpp – gueltig-by-construction wie StateStoreKey
enum class SensorOffsetStatus : uint8_t {
    Success,
    // |value| > sensor_limits::kMaxAbsoluteOffsetCelsius.
    OutOfFirmwareRange,
};

struct SensorOffsetCreateResult;  // vorwaertsdeklariert, siehe state_store_key.hpp-Muster

class SensorOffset {
   public:
    [[nodiscard]] static SensorOffsetCreateResult create(double celsius);
    [[nodiscard]] double celsius() const { return celsius_; }
   private:
    explicit SensorOffset(double celsius) : celsius_(celsius) {}
    double celsius_;
};

struct SensorOffsetCreateResult {
    SensorOffsetStatus status{SensorOffsetStatus::OutOfFirmwareRange};
    std::optional<SensorOffset> offset;
};
```

`SensorOffset::create()` lehnt jeden Betrag ausserhalb
`[-kMaxAbsoluteOffsetCelsius, +kMaxAbsoluteOffsetCelsius]`
(`sensor_limits.hpp`) bereits bei der Erzeugung ab (Korrektur gegenueber der
Vorversion, die dies nur als unspezifizierte "Programmfehler"-Konvention an
der Aufrufstelle behandelt hatte, ohne den Typ selbst zu schuetzen). Ein
fehlender Offset bleibt `SensorOffset::create(0.0)` (immer erfolgreich, da
`0.0` firmwarefest immer innerhalb der Grenze liegt).

`sensor_offset.hpp` haengt bewusst NICHT auf `storage_types.hpp`, sondern nur
auf `sensor_limits.hpp` (dieselbe fachliche Domaene) – dieselbe Abgrenzung
wie bei `SensorIdentity`.

## 14. Geplante Dateien mit Begruendung

```text
lib/device_platform/src/sensor_identity.hpp
  Eigenstaendiger, minimaler Sensoridentitaetstyp (Abschnitt 13a, KEINE
  Abhaengigkeit auf storage_types.hpp); noetig fuer ROM-Wechsel-Erkennung
  (Abschnitt 8/11), reine Wertsemantik, keine 1-Wire-Kenntnis.

lib/device_platform/src/sensor_sample.hpp
  RawSensorSample + SensorTransportStatus + SampleDisposition;
  Eingangsvertrag (Abschnitt 9/9a/9b).

lib/device_platform/src/sensor_quality.hpp
  SensorQuality-Enum (Abschnitt 8).

lib/device_platform/src/sensor_quality_config.hpp
  SensorQualityConfig, gueltig-by-construction (Abschnitt 10.0).

lib/device_platform/src/sensor_quality_snapshot.hpp
  SensorFaultReason + SensorQualitySnapshot; Ausgabevertrag (Abschnitt 12).

lib/device_platform/src/sensor_offset.hpp
  SensorOffset-Werttyp, gueltig-by-construction (Abschnitt 13a).

lib/device_platform/src/sensor_median_filter.hpp/.cpp
  Medianfilter mit fester Kapazitaet (Abschnitt 10.3), eigene SRP-Einheit,
  eigenstaendig testbar.

lib/device_platform/src/sensor_lowpass_filter.hpp/.cpp
  Tiefpassfilter mit Zeitkonstante (Abschnitt 10.5), eigene SRP-Einheit.

lib/device_platform/src/sensor_quality_pipeline.hpp/.cpp
  Orchestrator: verkettet 9b/10.1-10.5, fuehrt Zustandsmaschine (Abschnitt 8)
  und erzeugt SensorQualitySnapshot. Nimmt SensorQualityConfig im
  Konstruktor entgegen; haelt selbst KEINE ITimeSource-Referenz (Abschnitt
  9a) - Zeit kommt ausschliesslich explizit ueber ingest(sample, now) und
  snapshot(now) herein.

lib/device_platform/src/sensor_limits.hpp
  Firmwarefeste Obergrenzen (max. Medianfenster, absoluter
  Temperaturbereich, max. Offsetbetrag, max. STALE-Alter-Obergrenze, max.
  Wiedererkennungs-Probenzahl-Obergrenze) nach dem Muster von
  storage_slot_limits.hpp. Enthaelt bewusst KEINE sensor-/
  treiberspezifischen Zahlenkonstanten (Abschnitt 10.1). Konkrete, am realen
  Schrank ermittelte Werte bleiben TBD_COMMISSIONING (siehe Abschnitt 18);
  diese Datei liefert nur die aeusseren, nie ueberschreitbaren
  Sicherheitsgrenzen.

lib/device_platform_test_support/src/sensor_fault_sequence.hpp/.cpp
  Deterministischer, skriptbarer Mehrfach-Fehlerinjektor: liefert eine
  vordefinierte Folge von RawSensorSample-Werten (inkl. Transportstatus-,
  Zeitstempelanomalien, ROM-Wechseln) fuer die Testmatrix aus Abschnitt 17.
  Ersetzt/ergaenzt NICHT MockTemperatureSource (bleibt fuer #20 unberuehrt,
  da #20 nicht ueber ITemperatureSource einspeist, siehe Abschnitt 9). Wird
  ausschliesslich vom Pipeline-Testtopic verwendet (Abschnitt 17a) - echte
  Wiederverwendung, kein erzwungenes gemeinsames Testhilfsmittel fuer
  Topics, die es nicht brauchen.

test/
  siehe Abschnitt 17a fuer die vollstaendige, nach Topics aufgeteilte Liste.

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

Modulzuordnung (Entscheidung C):
  device_platform statt fermentation_app, durch den Nachkorrekturauftrag zu
  PR #95 ausdruecklich entschieden und durch ADR-013 (Abschnitt "Regeln fuer
  neue Module") sowie lib/device_platform/AGENTS.md wortwoertlich gedeckt
  (siehe Abschnitt 13/19).
```

`SOLID_REVIEW: PASS`
`DRY_REVIEW: PASS`
`KISS_REVIEW: PASS`

## 16. Ressourcen-, Laufzeit- und CI-Nachweise der spaeteren Umsetzung

```text
Speicher pro Pipeline-Instanz (Schaetzung, spaeter im Implementierungs-PR
  per scripts/build_report.py nachzuweisen):
  - RawSensorSample: ~40 Byte (std::optional<double> statt double, siehe
    Korrektur Abschnitt 9 - typabhaengig groesser als in der Vorversion
    geschaetzt)
  - Medianpuffer: kMaxMedianWindowSize * sizeof(double), bei einer
    erwarteten Obergrenze von z. B. 9 Werten ~72 Byte
  - SensorQualitySnapshot: ~130-160 Byte (mehrere optional<double>/
    optional<uint64_t>-Felder, Korrektur Abschnitt 12)
  - Pipeline-interner Zustand (letzter Wert, Zaehler, Zeitstempel):
    < 100 Byte
  - Gesamt < 500 Byte je Instanz, statisch (kein Heap), 3 Instanzen (Luft,
    Produkt, Kuehlkoerper) < 1,5 KB RAM – vernachlaessigbar gegenueber dem
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
  Allokation im Verarbeitungspfad. Flash-Wirkung durch 9 neue kleine
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
  - Start ohne Probe -> Stale, kein Regelwert, rawCelsius/correctedCelsius/
    filteredCelsius/lastSampleAgeMs/lastValidSampleAgeMs/
    changeRateCelsiusPerSecond alle == std::nullopt (keine erfundenen
    Platzhalterwerte)
  - erste gueltige Probe -> weiterhin Stale (noch nicht genug Folgeproben),
    rawCelsius/correctedCelsius/lastSampleAgeMs jetzt gesetzt,
    filteredCelsius bleibt nullopt bis Slice B einen ersten Tiefpasswert
    erzeugt
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
  - KnownInvalidMeasurement-Sample (transportStatus direkt so geskriptet,
    unabhaengig vom konkreten Rohwert - #20 kennt keine sensor-/
    treiberspezifischen Zahlenkonstanten, siehe Abschnitt 10.1) -> ungueltig,
    Ursache KnownInvalidMeasurement
  - NaN/Inf als rawCelsius (durch den Testinjektor konstruierbar, da
    rawCelsius ein optional<double> ist und ein nicht endlicher Wert
    weiterhin ein technisch darstellbarer double bleibt) -> ungueltig,
    Ursache NonFinite (nicht OutOfRange - eigene Ursache, siehe Abschnitt 12)

Zeit und Alter (Orakel: Abschnitt 9, SAFETY_COMPONENT_FAULTS.md
Sensorzustandsfolge):
  - Stale durch Altersgrenze (kMaxStaleAgeMs) ueberschritten -> Failed
  - Failed durch maximale Fehlerdauer -> bleibt Failed bis Wiedererkennung
  - Zeitstempel < letztem akzeptierten Zeitstempel -> RejectedRetrograde,
    Zustand/Filter/Alter unveraendert (kein Effekt auf consecutiveInvalidCount)
  - Zeitstempel > nowMonotonicMs (an ingest() uebergeben) -> RejectedFuture,
    gleiche Wirkungslosigkeit wie oben
  - identischer Zeitstempel + identischer Wert -> DuplicateIgnored
  - identischer Zeitstempel + unterschiedlicher Wert -> RejectedTimestampConflict
    (NICHT akzeptiert - Korrektur gegenueber der Vorversion, siehe Abschnitt 9b);
    Test belegt, dass danach weiterhin nur der vorherige Wert als "letzter
    akzeptierter" gilt
  - keine der vier obigen Dispositionen erhoeht consecutiveInvalidCount
    (eigener Testfall je Disposition)
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
  - abgeleitete Regelwertfreigabe (quality == Valid UND
    filteredCelsius.has_value(), Abschnitt 12) ist in Stale/Failed in jedem
    einzelnen Testfall dieser Gruppe explizit false
  - letzter gueltiger Wert (rawCelsius/correctedCelsius/filteredCelsius)
    bleibt im Snapshot sichtbar, auch waehrend Stale/Failed, obwohl die
    abgeleitete Regelwertfreigabe dort false ist

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

### 17a. Testdatei-Aufteilung (KISS, keine Monsterdatei)

Korrektur gegenueber der Vorversion: Statt einer einzigen
`test/test_sensor_quality/test_sensor_quality.cpp`, die alle neun
Testgruppen aus Abschnitt 17 (voraussichtlich 40+ Einzeltests) buendeln
wuerde, folgt die Testaufteilung derselben SRP-Trennung wie der
Produktionscode (Abschnitt 13/14) – ein Testtopic pro eigenstaendig
testbarer Komponente, wie es die bestehende Konvention
(`test/<thema>/test_<thema>.cpp`) bereits fuer andere Module vorsieht:

```text
test/test_sensor_identity/test_sensor_identity.cpp
  SensorIdentity: Gleichheit, Default (0/"unbekannt"), keine
  Fremdtypvermischung. Klein, unabhaengig, keine sensor_fault_sequence.hpp
  noetig.

test/test_sensor_offset/test_sensor_offset.cpp
  SensorOffset::create(): Erfolg innerhalb der Grenze, Ablehnung ausserhalb,
  Randwerte. Unabhaengig, keine sensor_fault_sequence.hpp noetig.

test/test_sensor_quality_config/test_sensor_quality_config.cpp
  SensorQualityConfig::create(): jede InvalidXxx-Ablehnung einzeln,
  Erfolgsfall. Unabhaengig, keine sensor_fault_sequence.hpp noetig.

test/test_sensor_median_filter/test_sensor_median_filter.cpp
  MedianFilter isoliert mit rohen double-Folgen (kein RawSensorSample
  noetig): ungefuelltes Fenster, Einzelspitze, Trend, gerade/ungerade
  Konfiguration.

test/test_sensor_lowpass_filter/test_sensor_lowpass_filter.cpp
  LowPassFilter isoliert mit rohen double-Folgen + dt-Werten: Einschwingen,
  unterschiedliche Tau-Werte, Messluecken.

test/test_sensor_quality_pipeline/test_sensor_quality_pipeline.cpp
  SensorQualityPipeline als Orchestrator: Start/Normalbetrieb, Transport-/
  Messfehler, Zeit/Alter/Disposition, Wertebereich/Dynamik, Median-/
  Tiefpass-INTEGRATION (nicht die Filter-Algorithmen selbst - die sind
  bereits in den beiden Filtertests oben abgedeckt), Offset-Integration,
  Zustandsmaschine/Wiedererkennung, Widersprueche/Scopegrenzen, Robustheit.
  Einziger tatsaechlicher Konsument von sensor_fault_sequence.hpp - echte
  Wiederverwendung ueber mehrere Testgruppen dieser einen Datei hinweg,
  nicht ueber alle sechs Testtopics erzwungen.
```

Diese sechs Topics wachsen ueber Slice 1/2 (Abschnitt 20) inkrementell,
bleiben aber jeweils bei ihrem eigenen abgegrenzten Thema – kein einzelnes
Testtopic deckt mehr als eine Komponente ab.

`TEST_STRUCTURE_KISS: PASS`

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
  Diagnosevertrag + vollstaendige Testmatrix) ist fuer eine einzelne
  Reviewrunde zu gross.
  Gegenmassnahme: entschiedene Teilung A/B in zwei interne Reviewslices
  innerhalb desselben Draft-PR (Entscheidung A, Abschnitt 19/20).

Risiko: Eine spaetere ADR-Notwendigkeit wird uebersehen.
  Gegenmassnahme: bereits durch Entscheidung B (Abschnitt 19) explizit
  gepruft und verneint; sollte eine tatsaechlich neue Grundsatzfrage waehrend
  der Implementierung auftauchen, ist das eine materielle Planabweichung
  (AGENTS.md) und erfordert einen neuen Plan-Commit samt erneuter Freigabe.
```

## 19. Bereits entschiedene Punkte (vormals "Offene Ownerentscheidungen")

Korrektur: Der Nachkorrekturauftrag zu PR #95 entscheidet A, B und C
ausdruecklich und verbindlich. Diese drei Punkte werden hier nicht mehr als
offene Ownerentscheidungen gefuehrt; `OWNER_DECISIONS_OPEN: 0`. Die
folgenden Unterabschnitte dokumentieren die Entscheidung samt Begruendung,
nicht mehr Optionen/Empfehlung.

### Entscheidung A (entschieden): ein Issue, ein Branch, derselbe Draft-PR, zwei interne Reviewslices

```text
Entschieden: #20 wird in genau einem Issue, einem Branch und demselben
  Draft-PR (#95) umgesetzt. Innerhalb dieses einen PR gibt es zwei interne
  Reviewslices:
    1. Qualitaetszustand + Plausibilitaet (SensorIdentity, RawSensorSample,
       SampleDisposition, SensorQuality-Zustandsmaschine, Plausibilitaet
       10.1/10.2, SensorQualityConfig, Diagnosevertrag - filteredCelsius
       bleibt hier std::nullopt, da der Tiefpass noch nicht existiert, KEIN
       Platzhalterverhalten, siehe Korrektur in Abschnitt 12/20).
    2. Median + Offset + Tiefpass + vollstaendige Integration
       (SensorOffset, MedianFilter, LowPassFilter, vollstaendige
       Pipelineintegration, verbleibende Tests).
  Kein zweiter PR, keine Uebergangs-/Platzhalter-API zwischen den Slices
  (siehe Abschnitt 20 fuer die korrigierte Reihenfolge).
Begruendung: So vom Nachkorrekturauftrag ausdruecklich festgelegt; deckt
  sich mit der bereits im Ursprungsplan zitierten, unabhaengig
  uebereinstimmenden Empfehlung aus
  docs/audits/PROPOSED_RELEASE_1_ROADMAP.md ("#20: Status-/
  Plausibilitaetsmodell, danach Filterpipeline").
```

`DECISION_A_ONE_PR_TWO_SLICES: PASS`

### Entscheidung B (entschieden): keine neue ADR

```text
Entschieden: Die Umsetzung von #20 legt KEINE neue ADR in docs/DECISIONS.md
  an. #20 setzt bereits akzeptierte Architektur und Spezifikation um
  (ARCHITECTURE.md, SENSOR_TUNING_COMMISSIONING.md, SAFETY_COMPONENT_
  FAULTS.md, ADR-013); es gibt keine neue, bisher unentschiedene
  Grundsatzfrage, die eine eigene ADR rechtfertigen wuerde.
```

`DECISION_B_NO_NEW_ADR: PASS`

### Entscheidung C (entschieden): device_platform

```text
Entschieden: Die anwendungsneutrale Sensorqualitaets- und Filterpipeline aus
  #20 gehoert nach lib/device_platform/ (wie in Abschnitt 13/13a bereits
  ausgearbeitet). Konkrete Rollen wie Schrankluft, Produkt und Kuehlkoerper
  bleiben ausserhalb der Plattform, in Anwendung beziehungsweise Composition
  Root.
Begruendung (durch den Nachkorrekturauftrag bestaetigt und durch ADR-013
  wortwoertlich gedeckt): ADR-013, Abschnitt "Regeln fuer neue Module",
  zaehlt "Sensorqualitaet, Filter, begrenzte Reglerbausteine" ausdruecklich
  zu den "Allgemeine[n] Bausteine[n]", die "in der Plattform liegen"
  duerfen; ADR-013 nennt in seinem Kontextabschnitt "Sensorqualitaet" sogar
  explizit als Beispiel fuer eine ueber Fermentationsschrank UND eine
  zukuenftige Smokersteuerung hinweg gemeinsam benoetigte Grundfunktion.
  lib/device_platform/AGENTS.md deckt sich damit ("allgemeine
  Sensorqualitaet, Filter und begrenzte Reglerbausteine" im Abschnitt
  "Erlaubt"). Diese Quellen entscheiden die Frage eindeutig zugunsten von
  device_platform; die vormals in dieser Datei dokumentierte Gegenlesart
  von Abschnitt 11 des urspruenglichen Auftrags ("nur vorhandene generische
  Ports") gilt als durch den Nachkorrekturauftrag ausdruecklich aufgeloest.
```

`DECISION_C_DEVICE_PLATFORM: PASS`
`OWNER_DECISIONS_OPEN: 0`
`IMPLEMENTATION_BLOCKED_BY_OWNER_DECISION: NO`

## 20. Genaue Implementierungsreihenfolge (nach Planfreigabe)

Zwei interne Reviewslices im selben Draft-PR #95 (Entscheidung A, Abschnitt
19), kein zweiter PR, keine Uebergangs-/Platzhalter-API zwischen den
Slices: `filteredCelsius` ist waehrend Slice A schlicht `std::nullopt`
(echtes "noch nicht vorhanden", kein erfundener Gleichstand mit
`correctedCelsius`, siehe Abschnitt 12).

```text
Slice 1 (Qualitaetszustand + Plausibilitaet):
  1. sensor_limits.hpp (firmwarefeste Grenzen, ohne sensor-/
     treiberspezifische Konstanten)
  2. sensor_identity.hpp (eigenstaendig, Abschnitt 13a)
  3. sensor_sample.hpp (RawSensorSample, SensorTransportStatus,
     SampleDisposition)
  4. sensor_quality.hpp
  5. sensor_quality_config.hpp (Abschnitt 10.0)
  6. sensor_quality_snapshot.hpp (SensorFaultReason, SensorQualitySnapshot;
     filteredCelsius als std::optional<double>, bleibt in Slice 1 stets
     std::nullopt, da MedianFilter/LowPassFilter noch nicht existieren)
  7. sensor_quality_pipeline.hpp/.cpp: Zeitstempel-/Dispositionspruefung
     (9b), Transport-/Wertebereichs-/Aenderungsratenpruefung (10.1/10.2),
     Zustandsmaschine (Abschnitt 8); ingest(sample, now) und snapshot(now)
     mit explizitem Zeitparameter, keine gespeicherte ITimeSource (9a)
  8. sensor_fault_sequence.hpp/.cpp (so weit fuer Slice-1-Tests benoetigt)
  9. test/test_sensor_identity/, test/test_sensor_quality_config/,
     test/test_sensor_quality_pipeline/ (Teilmenge: Start/Normalbetrieb,
     Transport-/Messfehler, Zeit/Alter/Disposition, Wertebereich,
     Zustandsmaschine/Wiedererkennung, Robustheit - siehe Abschnitt 17a)
  10. Dokumentation/Changelog-Eintrag fuer Slice 1
  11. Ressourcen-/CI-Nachweise fuer Slice 1

Slice 2 (Median + Offset + Tiefpass + vollstaendige Integration):
  12. sensor_offset.hpp (gueltig-by-construction, Abschnitt 13a)
  13. sensor_median_filter.hpp/.cpp
  14. sensor_lowpass_filter.hpp/.cpp
  15. sensor_quality_pipeline.hpp/.cpp: Integration von Offset/Median/
      Tiefpass, ROM-Wechsel-Filterreset; filteredCelsius wird ab hier fuer
      Valid-Proben tatsaechlich gesetzt
  16. verbleibende sensor_fault_sequence.hpp/.cpp-Ergaenzungen
  17. test/test_sensor_offset/, test/test_sensor_median_filter/,
      test/test_sensor_lowpass_filter/, verbleibende Faelle in
      test/test_sensor_quality_pipeline/ (Median/Tiefpass-Integration,
      Offset, Identitaet/ROM-Wechsel, Widersprueche/Scopegrenzen -
      Abschnitt 17a)
  18. Dokumentation/Changelog-Eintrag fuer Slice 2
  19. Ressourcen-/CI-Nachweise fuer Slice 2, vollstaendiger Testlauf
```

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
