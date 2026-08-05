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
17=Abschnitt 18 (Risiken), 18=Abschnitt 19 (Punkte A/B/C - entschieden,
siehe unten), 19=Abschnitt 20 (Implementierungsreihenfolge),
20=Abschnitt 21 (Stopbedingung), 21=Taskleiste am Dateiende. Abschnitt 11
dieser Datei (Widerspruchs-/Plausibilitaetsbewertung) ist eine
Praezisierung von Abschnitt 8/10 des urspruenglichen Auftrags (dort explizit
als Pflichtinhalt "Zustandsmodell" und "Verarbeitungspipeline" gefordert)
und keine eigene der 21 Pflichtnummern.

### Nachkorrektur-Historie

```text
Runde 1 (Kurze Nachkorrektur):
  PREVIOUS_PLAN_COMMIT: f52741170726dafc6d6ea056f55c3cb04c095de2
  PREVIOUS_HEAD:        16dc12d661a7c387193c141ba1f0d18bc556ee25
  -> Entscheidungen A/B/C entschieden; filteredCelsius-Platzhalter entfernt;
     SensorQualityConfig definiert; einziger Zeitvertrag; Zeitstempelregeln
     korrigiert; DS18B20-Konstanten aus generischer Plattform entfernt;
     generischer Eingangsvertrag; optionaler/redundanzfreier Diagnosevertrag;
     Disposition/Fault-Enums getrennt; SensorIdentity/SensorOffset ohne
     storage_types.hpp; Teststruktur aufgeteilt.
  Ergebnis dieser Runde: 6805df1, dann Advisor-Fix auf 75f6a168fd11f30dc35000df8f93b7d2b675bc19
  (STALE/FAILED-Ableitung ueber snapshot(now) statt gespeichertem Zustand;
  correctedCelsius als Medianfilter-Ausgang + Offset statt Rohwert + Offset).

Runde 2 (Letzte Plan-Nachkorrektur, dieser Stand):
  PREVIOUS_PLAN_COMMIT: 75f6a168fd11f30dc35000df8f93b7d2b675bc19
  PREVIOUS_HEAD:        75f6a168fd11f30dc35000df8f93b7d2b675bc19
  Ausgeloest durch "AUFTRAG_PR95_Letzte_Plan_Nachkorrektur.md".
```

Beide Runden korrigieren einen noch NICHT freigegebenen Plan (keine
"PLAN APPROVED"-Ownerfreigabe lag je vor); es handelt sich um gewoehnliche
Plankorrekturen vor Erstfreigabe, nicht um materielle Abweichungen von einem
bereits freigegebenen Plan. Jede inhaltliche Aenderung dieser zweiten Runde
ist an ihrer jeweiligen Stelle im Dokument als "Korrektur Runde 2" markiert.

## 2. Live-Issue- und Abhaengigkeitspruefung

```text
#20: state="OPEN", Status-Feld im Body="READY", 0 Kommentare (erneut geprueft)
#30: state="OPEN", Status-Feld im Body="BLOCKED_HARDWARE", 0 Kommentare (erneut geprueft)
#10: state="CLOSED" (Body-Statusfeld "READY" ist redaktionell veraltet;
     massgeblich ist der GitHub-Issue-Zustand CLOSED)
#11: state="CLOSED" (Body-Statusfeld "PLANNED_SPEC_PENDING" ebenso veraltet;
     massgeblich ist CLOSED)
```

Beide Abhaengigkeiten `#10` und `#11` sind ueber den GitHub-Issue-Zustand
(nicht ueber das interne Body-Statusfeld) als abgeschlossen verifiziert.
Zusaetzlich gelesen: `#5` (Epic, `OPEN`), `#21` (`OPEN`,
`PLANNED_SPEC_PENDING`), `#24` (`OPEN`, `PLANNED_SPEC_PENDING`).

Keines dieser Live-Issues wird durch diesen Plan-PR veraendert.

## 3. Gelesene Quellen

```text
- AGENTS.md (Repository-Wurzel)
- lib/device_platform/AGENTS.md
- lib/device_platform_test_support/AGENTS.md
- lib/fermentation_app/AGENTS.md
- docs/ADR-013_REUSABLE_DEVICE_PLATFORM.md (erneut, wortwoertlich fuer Runde 2)
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
- Code: lib/device_platform/src/temperature_source.hpp,
  lib/device_platform_test_support/src/mock_temperature_source.hpp/.cpp,
  test/test_sensor_actuator_mocks/test_sensor_actuator_mocks.cpp (erneut,
  woertlich fuer Runde 2), siehe Abschnitt 4 fuer den vollstaendigen
  Codebestand
```

`SOURCES_READ: 21`

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
  VALID/STALE/FAILED und Filterung NICHT Aufgabe dieses Ports sind. DIESE
  DATEI WIRD VON #20 GEZIELT ERWEITERT (Korrektur Runde 2, siehe Abschnitt 9:
  kein paralleler Eingangstyp mehr, sondern Weiterentwicklung dieses
  bestehenden Ports zum kanonischen Rohprobenvertrag).
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
  Mehrfachfehlerinjektion. WIRD VON #20 AUF DEN ERWEITERTEN PORT NACHGEZOGEN
  (Korrektur Runde 2, siehe Abschnitt 9/14).
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

### Bereits vorhanden (Tests, betroffen von Runde 2)

```text
test/test_sensor_actuator_mocks/test_sensor_actuator_mocks.cpp
  Enthaelt genau drei temperaturbezogene Testfunktionen
  (test_temperature_source_reports_configured_value,
  test_temperature_source_can_change_value,
  test_temperature_source_fault_injection_marks_unavailable), die auf der
  heutigen {available,celsius}-Form aufsetzen. WERDEN VON #20 AUF DEN
  ERWEITERTEN PORT NACHGEZOGEN (Abschnitt 14/17a). Alle uebrigen
  Testfunktionen dieser Datei (Aktuator-Mocks) sind nicht betroffen.
```

Repository-weite Pruefung (Runde 2): `ITemperatureSource`/`TemperatureReading`
werden ausserhalb dieser drei Dateien nirgends verwendet - insbesondere nicht
in `src/main.cpp`, `main/app_main.cpp` oder `lib/fermentation_app/`. Die
Erweiterung des Ports hat damit einen vollstaendig bekannten, auf drei
Dateien begrenzten Blastradius (siehe Abschnitt 18).

### Nicht vorhanden (greenfield fuer #20)

```text
- kein Sensorqualitaets-/Diagnosezustand (VALID/STALE/FAILED) irgendwo im Code
- kein Medianfilter, kein Tiefpassfilter irgendwo in lib/
- kein ROM-/Sensoridentitaetstyp
- kein Fehlerursachen- oder Trendmodell fuer Sensoren
- kein Fehlersequenz-Injektor (nur binaeres available/unavailable im
  bestehenden Mock)
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
- ITemperatureSource / TemperatureReading: KORREKTUR RUNDE 2 - wird NICHT
  unveraendert gelassen, sondern gezielt zum kanonischen, generischen
  Rohprobenvertrag weiterentwickelt (siehe Abschnitt 9). Kein paralleler
  Eingangstyp neben diesem Port.
- StrongId<Tag,Underlying>-Muster (storage_types.hpp): dient nur als
  STILISTISCHE Vorlage fuer den neuen, eigenstaendigen SensorIdentity-Typ;
  SensorIdentity haengt bewusst NICHT auf storage_types.hpp (siehe
  Abschnitt 13a – unpassende Domaenenkopplung, AGENTS.md-DRY-Ausnahme fuer
  oberflaechlich aehnlichen, fachlich fremden Code).
- StateStoreKey-Muster ("gueltig-by-construction" mit privatem Konstruktor und
  statischem create()): Vorlage fuer SensorIdentity, TemperatureReading
  (Abschnitt 9) und den Kalibrier-Offset-Werttyp SensorOffset (Abschnitt
  13a) - alle drei setzen ihre Invarianten jetzt bereits bei Erzeugung durch.
- storage_slot_limits.hpp-Muster: Vorlage fuer eine neue
  lib/device_platform/src/sensor_limits.hpp mit firmwarefesten Obergrenzen.
- enum class ... : uint8_t mit Pro-Variante-Dokumentationskommentar
  (StateStoreKeyStatus, CheckedIncrementStatus): Vorlage fuer SensorQuality
  und einen Fehlerursachen-Enum.
- test/<thema>/test_<thema>.cpp-Konvention: direkt fuer neue Testverzeichnisse
  uebernehmen.
```

`DUPLICATE_TYPES_AVOIDED: PASS` – #20 erweitert `device_platform` um neue,
bislang nicht existierende fachliche Typen (Qualitaetszustand, Filter) und
evolviert genau einen bestehenden Port (`ITemperatureSource`/
`TemperatureReading`), statt ihn zu duplizieren.

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
Rohprobe (ueber den erweiterten ITemperatureSource-Port, Abschnitt 9)
-> Zeitstempel-/Dispositionspruefung (Abschnitt 9b)
-> Transport-/Messstatus (generisch, keine sensor-/treiberspezifischen
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

Wortgleich aus Abschnitt 15 des urspruenglichen Auftrags uebernommen und
verbindlich:

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
  ausschliesslich Dateien aus Abschnitt 14 - EINSCHLIESSLICH der drei
  bestehenden Dateien um ITemperatureSource/MockTemperatureSource, siehe
  Korrektur Runde 2)
```

`SCOPE_21_BOUNDARY_DEFINED: PASS`
`SCOPE_24_BOUNDARY_DEFINED: PASS`
`SCOPE_30_BOUNDARY_DEFINED: PASS`

Konkrete Grenzziehung:

```text
zu #21 (Regelsensorauswahl/Ersatzbetrieb):
  #20 liefert je Sensorrolle einen Qualitaets- und Diagnosevertrag
  (VALID/STALE/FAILED, gefilterter Wert, Fehlerursache). KORREKTUR RUNDE 2:
  #20 liefert KEINE Verdachtsmarkierung bei rollenuebergreifendem
  Sensorwiderspruch - das war ein Restwiderspruch zu Abschnitt 11 Punkt 4
  (dort bereits korrekt als ausserhalb #20 liegend beschrieben) und wird
  hier entfernt. #20 entscheidet NICHT, welcher Sensor gerade der primaere
  Regelsensor ist, wechselt NICHT automatisch zwischen Produkt- und
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
  KORREKTUR RUNDE 2 (ersetzt die Vorversion vollstaendig): #20 verarbeitet
  Proben ueber den kanonischen, in Abschnitt 9 erweiterten
  `ITemperatureSource`-Port (`TemperatureReading`). #30 implementiert
  GENAU DIESEN Port mit einem realen DS18B20-Adapter - es gibt keine
  zweite, parallele Eingangsform mehr, die #30 zusaetzlich uebersetzen
  muesste. #20 selbst enthaelt weiterhin keine 1-Wire-, GPIO- oder
  DS18B20-Bibliothekskenntnis; die reale Bus-/CRC-Erkennung bleibt
  vollstaendig #30s Aufgabe, sie muss nur auf denselben, bereits von #20
  definierten Vertrag (Ok/BusFault/CrcFault/MissingSample/
  KnownInvalidMeasurement, Abschnitt 9) abbilden.
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

Alle folgenden Uebergaenge beschreiben das FACHLICHE Verhalten; wann genau
`quality` diesen Wert widerspiegelt, regelt Abschnitt 9a (dort als
`snapshot(now)`-Ableitung aus rohen, bei `ingest()` fortgeschriebenen
Zaehl-/Zeitgroessen definiert - kein Hintergrundprozess, kein bei jedem
Uebergang separat gesetztes Feld, keine ITimeSource-Abhaengigkeit der
Pipeline selbst, siehe Abschnitt 9a fuer die vollstaendige Praezisierung
aus Runde 2).

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
  physischen Sensors darf nicht in den neuen Sensor hineinwirken). KORREKTUR
  RUNDE 2 (siehe Abschnitt 9): ein ROM-Wechsel wird nur erkannt, wenn ZWEI
  aufeinanderfolgende akzeptierte Proben je eine BEKANNTE (nicht-nullopt)
  Identitaet tragen und diese sich unterscheiden. Ein Wechsel von/zu
  "Identitaet unbekannt" (nullopt) ist fuer sich allein KEIN ROM-Wechsel
  (fehlende Evidenz in beide Richtungen, siehe Abschnitt 9b).

Reset/Fortfuehrung der Filterzustaende (KORREKTUR RUNDE 4 - vorher
widerspruechlich, siehe unten):
  kurzer STALE-Zustand durch einzelne/kurze Fehler (Valid -> Stale ->
    Valid, OHNE dass FAILED zwischenzeitlich erreicht wurde): Filterzustand
    bleibt WAEHREND UND NACH der Wiedererkennung vollstaendig erhalten,
    Median-/Tiefpassberechnung wird mit den bestehenden Werten fortgesetzt.
  FAILED -> Valid (nach Wiedererkennung) ODER bestaetigter
    Sensoridentitaetswechsel (ROM-Wechsel, Abschnitt 9b/11): Filterzustand
    wird verworfen und beginnt mit den Proben der erfolgreichen
    Wiedererkennungsfolge neu.

  Widerspruch der Vorversion: Dort hiess es gleichzeitig "Valid -> Stale
  (kurzzeitig): Filterzustand bleibt erhalten" UND "Stale/Failed -> Valid
  (nach Wiedererkennung) ... Filterzustand wird verworfen" - fuer den
  haeufigsten Fall (eine einzelne ungueltige Probe fuehrt kurz nach Stale,
  direkt gefolgt von kMinConsecutiveValidSamples gueltigen Proben zurueck zu
  Valid, OHNE je Failed zu erreichen) widersprachen sich diese beiden Regeln:
  die erste verspricht Erhalt, die zweite verlangt Verwerfen desselben
  Zustands beim Ruecksprung nach Valid. Jetzt eindeutig: NUR ein
  tatsaechlich erreichtes FAILED oder ein bestaetigter ROM-Wechsel loest den
  Reset aus - deckungsgleich mit Abschnitt 10.3 ("Nach FAILED/ROM-Wechsel:
  Fenster wird geleert") und Abschnitt 10.5 ("nach FAILED/ROM-Wechsel wird
  gefiltert_alt ... gesetzt"), die beide von jeher nur FAILED/ROM-Wechsel
  nannten, nie ein einfaches Stale->Valid.

  Kein zusaetzlicher zeitbasierter Reset INNERHALB von STALE noetig: Ein
  STALE-Zustand kann strukturell nicht unbegrenzt andauern, ohne FAILED zu
  erreichen - Abschnitt 8 definiert bereits zwei firmwarefeste, in
  SensorQualityConfig validierte Obergrenzen (kMaxStaleAgeMs,
  kMaxConsecutiveInvalid, siehe Abschnitt 10.0), von denen JEDE
  ueberschritten STALE zwingend nach FAILED ueberfuehrt. Ein Filter kann
  also nie "beliebig lange in STALE mit stillschweigend veraltetem
  Filterzustand verharren" - er erreicht spaetestens bei einer dieser
  Grenzen FAILED und wird dort regulaer zurueckgesetzt. Eine zusaetzliche,
  dritte Schwelle nur fuer einen Reset "innerhalb" STALE waere redundant zu
  bereits vorhandenen, bereits validierten Grenzen (DRY/KISS) und wird
  deshalb NICHT eingefuehrt.
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
`TRANSIENT_STALE_PRESERVES_FILTER: PASS`
`FAILED_OR_IDENTITY_CHANGE_RESETS_FILTER: PASS`

## 9. Eingangsvertrag: Erweiterung des bestehenden ITemperatureSource-Ports

KORREKTUR RUNDE 2 (ersetzt die Vorversion vollstaendig): Die Vorversion
plante einen neuen, von `ITemperatureSource`/`TemperatureReading`
entkoppelten Typ `RawSensorSample`. Das ist eine unzulaessige PARALLELE
Eingangsarchitektur: zwei konkurrierende Vorstellungen davon, "was eine
Temperaturprobe ist", von denen eine (der bestehende Port) faktisch
unbenutzt verkuemmert waere. Stattdessen wird der bestehende Port GEZIELT
zum kanonischen, generischen Rohprobenvertrag weiterentwickelt. Der spaetere
Adapter aus #30 implementiert genau diesen (dann bereits existierenden)
Port. `RawSensorSample` entfaellt ersatzlos; alle seine Faelle werden vom
erweiterten `TemperatureReading` abgedeckt.

Betroffene, im Diff dieses Issues zu aendernde BESTEHENDE Dateien (siehe
Abschnitt 14): `lib/device_platform/src/temperature_source.hpp`,
`lib/device_platform_test_support/src/mock_temperature_source.hpp/.cpp`,
`test/test_sensor_actuator_mocks/test_sensor_actuator_mocks.cpp` (nur die
drei temperaturbezogenen Testfunktionen).

### 9.0 SensorIdentity (eigenstaendig, gueltig-by-construction, kein 0-Sentinel)

KORREKTUR RUNDE 2: `SensorIdentity` liess in der Vorversion `0` als
"unbekannte Identitaet" konstruierbar zu - ein unklarer Sentinel innerhalb
eines Typs, der ansonsten wie eine echte Sensor-ID aussieht (nichts an
`SensorIdentity(0)` unterscheidet sich strukturell von einer echten,
zufaellig auf `0` lautenden ID). Jetzt gueltig-by-construction, `0` wird
abgelehnt; "Identitaet (noch) nicht bekannt" wird stattdessen ausschliesslich
ueber `std::optional<SensorIdentity>` auf der naechsthoeheren Ebene
(`TemperatureReading`, Abschnitt 9.1) modelliert - niemals als Sonderwert
innerhalb von `SensorIdentity` selbst:

```cpp
// sensor_identity.hpp – eigenstaendig, keine Abhaengigkeit auf storage_types.hpp
enum class SensorIdentityStatus : uint8_t {
    Success,
    // 0 ist kein gueltiger Identitaetswert - er waere von einer "unbekannt"-
    // Bedeutung nicht unterscheidbar. "Unbekannt" wird ausschliesslich durch
    // Abwesenheit (std::optional<SensorIdentity>) dargestellt, nie durch
    // einen Wert innerhalb dieses Typs.
    ZeroIsNotAValidIdentity,
};

struct SensorIdentityCreateResult;  // vorwaertsdeklariert

class SensorIdentity {
   public:
    [[nodiscard]] static SensorIdentityCreateResult create(uint64_t value);
    [[nodiscard]] constexpr uint64_t value() const { return value_; }
    friend constexpr bool operator==(SensorIdentity a, SensorIdentity b) {
        return a.value_ == b.value_;
    }
    friend constexpr bool operator!=(SensorIdentity a, SensorIdentity b) {
        return !(a == b);
    }
   private:
    constexpr explicit SensorIdentity(uint64_t value) : value_(value) {}
    uint64_t value_;
};

struct SensorIdentityCreateResult {
    SensorIdentityStatus status{SensorIdentityStatus::ZeroIsNotAValidIdentity};
    std::optional<SensorIdentity> identity;
};
```

`sensor_identity.hpp` haengt weiterhin bewusst NICHT auf `storage_types.hpp`
(siehe Abschnitt 13a fuer die unveraendert gueltige Begruendung der
Domaenentrennung).

### 9.1 TemperatureReading (erweitert, gueltig-by-construction)

```cpp
// temperature_source.hpp (ERWEITERUNG der bestehenden Datei)
enum class TemperatureSampleStatus : uint8_t {
    Ok,
    BusFault,
    CrcFault,
    MissingSample,
    // Der Adapter hat den Rohwert bereits selbst als einen ihm bekannten
    // ungueltigen Messwert erkannt (z. B. einen Sensor-/Treiber-
    // spezifischen Einschalt- oder Diskonnektwert). Die KONKRETE Erkennung
    // bleibt Aufgabe des jeweiligen Adapters (#30); #20 kennt nur diesen
    // generischen Status, keine Sensor-/Treiberkonstanten (Abschnitt 10.1).
    KnownInvalidMeasurement,
};

enum class TemperatureReadingStatus : uint8_t {
    Success,
    // status == Ok, aber celsius fehlt, ODER status != Ok, aber celsius ist
    // gesetzt. Genau eine der beiden Kombinationen ist gueltig.
    InconsistentValuePresence,
};

struct TemperatureReadingCreateResult;  // vorwaertsdeklariert

// Kanonischer Rohprobenvertrag: gueltig-by-construction erzwingt
//   status == Ok             <=> celsius().has_value() == true
//   status != Ok              <=> celsius().has_value() == false
// identity ist unabhaengig davon optional (nullopt = dem Adapter noch nicht
// bekannt, z. B. vor erster Busenumeration); wenn gesetzt, ist es bereits
// ein gueltiges SensorIdentity (Abschnitt 9.0, kein 0-Sentinel moeglich).
class TemperatureReading {
   public:
    [[nodiscard]] static TemperatureReadingCreateResult create(
        std::optional<SensorIdentity> identity,
        uint64_t monotonicTimestampMs,
        TemperatureSampleStatus status,
        std::optional<double> celsius);

    [[nodiscard]] std::optional<SensorIdentity> identity() const { return identity_; }
    [[nodiscard]] uint64_t monotonicTimestampMs() const { return monotonicTimestampMs_; }
    [[nodiscard]] TemperatureSampleStatus status() const { return status_; }
    [[nodiscard]] std::optional<double> celsius() const { return celsius_; }

   private:
    TemperatureReading(std::optional<SensorIdentity> identity,
                        uint64_t monotonicTimestampMs,
                        TemperatureSampleStatus status,
                        std::optional<double> celsius);
    std::optional<SensorIdentity> identity_;
    uint64_t monotonicTimestampMs_;
    TemperatureSampleStatus status_;
    std::optional<double> celsius_;
};

struct TemperatureReadingCreateResult {
    TemperatureReadingStatus status{TemperatureReadingStatus::InconsistentValuePresence};
    std::optional<TemperatureReading> reading;
};

// Anwendungsneutraler Port fuer eine einzelne Temperaturfuehlerrolle.
// Signatur UNVERAENDERT gegenueber der bisherigen Fassung (kein Konsument
// ausserhalb dieser drei Dateien existiert, siehe Abschnitt 4); nur der
// Rueckgabewerttyp TemperatureReading ist jetzt reichhaltiger.
class ITemperatureSource {
   public:
    ITemperatureSource() = default;
    virtual ~ITemperatureSource() = default;
    ITemperatureSource(const ITemperatureSource&) = delete;
    ITemperatureSource& operator=(const ITemperatureSource&) = delete;
    ITemperatureSource(ITemperatureSource&&) = delete;
    ITemperatureSource& operator=(ITemperatureSource&&) = delete;

    [[nodiscard]] virtual TemperatureReading read() const = 0;
};
```

Begruendung fuer die Erweiterung statt Neubau: Die bisherige
`TemperatureReading{bool available; double celsius;}` unterscheidet weder
Bus- von CRC-Fehlern noch eine fehlende von einer verfuegbaren, aber
unplausiblen Probe, und traegt keinen Zeitstempel. #20 braucht diese
feinere Unterscheidung; sie gehoert an genau die Stelle, an der #30 sie
spaeter ohnehin liefern muss - den Port selbst. `driverFaultCode` (aus der
ersten Nachkorrekturrunde erwogen) bleibt weiterhin entfernt: kein
Konsument innerhalb von #20 verwendet ein solches Feld.

Verdrahtung (KORREKTUR RUNDE 3, macht "kanonischer Port" konkret): #20
selbst ruft `read()` nirgends auf und verdrahtet keine Composition Root.
Die spaetere Composition Root (#21/#24) erzeugt den Aufrufpfad
`pipeline.ingest(source.read(), now)` - `source` ist ein konkreter,
in #30 gelieferter `ITemperatureSource`. Innerhalb von #20 werden
`TemperatureReading`-Werte fuer Pipeline-Tests ausschliesslich direkt ueber
`TemperatureReading::create(...)` konstruiert (siehe Abschnitt 17a), nicht
ueber einen echten oder gemockten Port gelesen.

`CANONICAL_TEMPERATURE_INPUT_PORT: PASS`
`PARALLEL_INPUT_CONTRACTS: 0`
`RAW_SAMPLE_VALID_BY_CONSTRUCTION: PASS`
`SENSOR_IDENTITY_SENTINEL_FREE: PASS`

### 9a. Einziger monotoner Zeitvertrag

```text
Es existiert GENAU EINE Uhr im Gesamtsystem (produktiv: die konkrete
ITimeSource-Instanz der Composition Root bzw. des #30-Adapters; nativ:
VirtualTimeSource). Ihr monotoner Millisekundenwert fliesst AUSSCHLIESSLICH
als expliziter Parameter oder als Feld einer bereits konstruierten
TemperatureReading in die #20-Typen ein - niemals als gespeicherte/
injizierte ITimeSource-Abhaengigkeit innerhalb von SensorQualityPipeline.

Drei Eintrittspunkte, alle mit explizitem Zeitbezug:

  TemperatureReading::monotonicTimestampMs()
    - die Erfassungszeit EINER Probe, vom erzeugenden Adapter/Mock/Test aus
      der einen Uhr gelesen und beim create()-Aufruf uebergeben.

  SensorQualityPipeline::ingest(const TemperatureReading& sample,
                                 uint64_t nowMonotonicMs) -> SampleDisposition
    - nowMonotonicMs ist derselbe Uhrwert, zum Zeitpunkt des ingest()-Aufrufs
      vom Aufrufer gelesen; dient der Zukunftspruefung (Abschnitt 9b) UND
      (Korrektur Runde 5) als Referenzzeitpunkt fuer die interne
      deriveQuality(nowMonotonicMs)-Vorzustandsermittlung (siehe unten,
      "Eine Ableitungsfunktion, zwei Aufrufstellen") - wird selbst NICHT
      gespeichert. Rueckgabewert: die klar definierte SampleDisposition
      dieser einen Probe (Abschnitt 9b/12) - kein `void`, kein unklarer
      Seiteneffekt.

  SensorQualityPipeline::snapshot(uint64_t nowMonotonicMs) const
    -> SensorQualitySnapshot
    - liefert Qualitaet und Altersfelder relativ zu diesem explizit
      uebergebenen "jetzt", nicht relativ zu einer intern gespeicherten Uhr.

SensorQualityPipeline selbst haelt KEIN ITimeSource-Feld, keine Referenz und
keinen Zeiger auf eine Uhr - sie ist bezueglich Zeit eine reine Funktion der
uebergebenen Werte (siehe Abschnitt 15, Punkt D: KEINE Abhaengigkeit auf
ITimeSource, auch nicht als Abstraktion).

Der Tiefpass (Abschnitt 10.5) verwendet AUSSCHLIESSLICH die Zeitstempel der
akzeptierten Proben (`TemperatureReading::monotonicTimestampMs()`) fuer
`dtSeconds` - NICHT `nowMonotonicMs` und NICHT irgendeine andere
ITimeSource-Quelle. `nowMonotonicMs` wird ausschliesslich fuer
Zukunftspruefung UND die interne Vorzustandsermittlung (beide in ingest(),
Korrektur Runde 5) sowie fuer die Altersberechnung (snapshot) verwendet,
niemals fuer die Filterdynamik selbst.

### Eine Ableitungsfunktion, zwei Aufrufstellen, keine zweite Qualitaetswahrheit

Ohne gespeicherte Uhr kann `quality` nicht als Hintergrundprozess "von
selbst" nach kMaxStaleAgeMs auf Failed wechseln, waehrend gar kein ingest()
mehr aufgerufen wird. Deshalb ist `quality` eine ABGELEITETE Groesse:

  - Eine einzige interne Hilfsfunktion `deriveQuality(referenceTimeMs)`
    berechnet quality/Alter/Regelwertfaehigkeit ausschliesslich aus den
    gespeicherten rohen Zaehl-/Zeitgroessen (letzter akzeptierter/gueltiger
    Zeitstempel, consecutiveInvalidCount, recoveryProgressCount) KOMBINIERT
    MIT dem uebergebenen referenceTimeMs. Es gibt NUR diese eine
    Implementierung.

  - snapshot(nowMonotonicMs) ruft deriveQuality(nowMonotonicMs) auf und
    bleibt dadurch eine reine, lesende (`const`) Ableitung - kein
    verstecktes Mutieren bei reinem Lesezugriff.

  - ingest(sample, nowMonotonicMs) DARF intern deriveQuality(nowMonotonicMs)
    aufrufen, um zu entscheiden, ob die neu eingetroffene, plausible Probe
    eine FORTSETZUNG eines bereits Valid laufenden Zustands ist (Filter
    normal fortfuehren) oder den BEGINN einer Wiedererkennung nach
    Stale/Failed markiert (recoveryProgressCount beginnt in BEIDEN Faellen
    bei 1). Der FILTERZUSTAND selbst wird dabei NUR verworfen, wenn die
    Wiedererkennung von FAILED aus beginnt oder ein bestaetigter ROM-Wechsel
    vorliegt - eine Wiedererkennung aus einem kurzen STALE heraus (ohne je
    FAILED erreicht zu haben) fuehrt den bestehenden Filterzustand
    unveraendert fort (Korrektur Runde 4, siehe Abschnitt 8 "Reset/
    Fortfuehrung der Filterzustaende" fuer die vollstaendige, vorher
    widerspruechliche Regel).

    KORREKTUR RUNDE 5 (behebt einen Fehler in der Referenzzeit dieser
    internen Ableitung): Die Referenzzeit fuer diese EINE Entscheidung
    ("war der Zustand gerade eben, JETZT, bereits FAILED?") muss
    `nowMonotonicMs` sein, NICHT `sample.monotonicTimestampMs()`. Eine
    geordnete, aber gegenueber der tatsaechlichen Uhrzeit stark verspaetet
    zugestellte Probe hat einen Zeitstempel deutlich VOR `nowMonotonicMs`
    (aber immer noch NACH dem zuletzt akzeptierten Zeitstempel, sonst waere
    sie RejectedRetrograde, Abschnitt 9b). Mit `sample.
    monotonicTimestampMs()` als Referenz wuerde das seit der letzten Probe
    verstrichene Alter SYSTEMATISCH UNTERSCHAETZT (da
    sample.monotonicTimestampMs() <= nowMonotonicMs immer gilt) - ein
    Zustand, der gemessen an der tatsaechlichen aktuellen Zeit laengst
    FAILED waere, koennte so faelschlich noch als blosses STALE erkannt und
    dadurch der notwendige Filterreset (Abschnitt 8) uebersprungen werden.
    `nowMonotonicMs` bleibt dagegen ausschliesslich fuer DIESE EINE
    Vorzustands-Entscheidung zustaendig; Reihenfolge/Duplikate (9b),
    Aenderungsrate (10.2), Median-/Tiefpass-Zeitabstand (10.3/10.5) und die
    Altersfelder im Snapshot (12) bleiben unveraendert
    `sample.monotonicTimestampMs()`-basiert (bzw. bei den Snapshot-
    Altersfeldern selbst wieder relativ zum spaeteren snapshot(now)-Aufruf).

    Diese interne Berechnung wird NICHT als zweites, persistentes
    `quality`-Feld gespeichert - sie ist ein einmaliger Entscheidungsschritt
    innerhalb des ingest()-Aufrufs, der dieselbe deriveQuality()-Funktion
    wiederverwendet, nur mit der Aufrufer-"now" statt eines spaeteren,
    separaten snapshot(now)-Aufrufs als Referenzzeitpunkt. Ein
    anschliessender snapshot(now)-Aufruf leitet quality unabhaengig und
    konsistent aus denselben rohen Groessen neu ab.

    VERBINDLICHE REIHENFOLGE innerhalb von ingest() (praezisiert Runde 5 -
    diese Reihenfolge ist die tragende Voraussetzung der obigen Korrektur
    und war vorher nicht explizit festgelegt): deriveQuality(nowMonotonicMs)
    wird AUSGEWERTET, BEVOR irgendein gespeicherter Zaehl-/Zeitwert dieser
    Probe (letzter akzeptierter/gueltiger Zeitstempel,
    consecutiveInvalidCount, recoveryProgressCount) durch die aktuelle Probe
    aktualisiert wird. Wuerde stattdessen zuerst z. B. der letzte
    akzeptierte Zeitstempel auf sample.monotonicTimestampMs() vorgezogen
    und ERST DANACH deriveQuality(nowMonotonicMs) ausgewertet, ergaebe die
    Ableitung faelschlich ein Alter nahe Null (Valid/Stale statt des real
    erreichten Failed) - genau die soeben behobene Fehlklassifikation waere
    dann durch die Aktualisierungsreihenfolge wieder eingefuehrt, nur mit
    korrektem `nowMonotonicMs`-Parameter. Der Vorzustand wird also IMMER aus
    den Groessen VOR dieser Probe ermittelt.

Damit erkennt JEDER spaetere snapshot(now)-Aufruf einen laengst verstummten
Sensor korrekt als Failed, unabhaengig davon, wie lange zuvor kein ingest()
mehr aufgerufen wurde, UND der Filterreset bei Wiedererkennung funktioniert
korrekt, ohne eine zweite, potenziell abweichende Qualitaetswahrheit zu
speichern.
```

`SINGLE_MONOTONIC_TIME_CONTRACT: PASS`
`INGEST_RESULT_DEFINED: PASS`
`INGEST_PREVIOUS_QUALITY_USES_NOW: PASS`
`TIME_AND_DERIVED_QUALITY_CONSISTENT: PASS`

### 9b. Zeitstempel- und Dispositionsregeln

```text
Jede eingehende Probe erhaelt GENAU EINE der folgenden Dispositionen (siehe
SampleDisposition, Abschnitt 12); erst bei Accepted durchlaeuft sie die
Plausibilitaetsstufen aus Abschnitt 10:

  identischer Zeitstempel + VOLLSTAENDIG identische semantische Felder
    (identity() UND status() UND celsius()/Abwesenheit, siehe
    sampleValuesEqual() unten) wie die zuletzt akzeptierte Probe ->
    DuplicateIgnored. Kein Effekt auf Alter, Zustandsmaschine oder
    Filterinhalt (reiner Doppelversand, keine neue Information). KORREKTUR
    RUNDE 4: vorher verglich diese Regel nur Rohwert und status, NICHT
    identity() - zwei Proben mit gleichem Zeitstempel, gleichem Messwert,
    aber UNTERSCHIEDLICHER Identitaet waeren faelschlich als reiner
    Doppelversand durchgegangen, obwohl ein ROM-Wechsel exakt zum selben
    Erfassungszeitpunkt keine plausible Dopplung, sondern ein
    widerspruechliches Signal ist.

  identischer Zeitstempel UND (identity() ODER status() ODER
    celsius()/Abwesenheit weicht ab) wie die zuletzt akzeptierte Probe ->
    RejectedTimestampConflict. Zwei widerspruechliche Werte fuer denselben
    Erfassungszeitpunkt sind kein gueltiger Messvorgang; es wird KEINE der
    beiden Varianten bevorzugt akzeptiert. Da RejectedTimestampConflict-
    Proben nie akzeptiert werden, bleiben aufeinanderfolgende akzeptierte
    Zeitstempel immer strikt steigend; die Aenderungsratenpruefung
    (Abschnitt 10.2) braucht deshalb keine erfundene Mindest-Zeitdifferenz,
    sondern kann echte Division durch Null durch Konstruktion ausschliessen.

  sampleValuesEqual(a, b) (Korrektur Runde 4 - Definition der
    "identischer Rohwert"-Vergleichsfunktion fuer die beiden Regeln oben,
    damit sie NICHT unkommentiert von gewoehnlicher double-Gleichheit
    abhaengt):
    a.celsius() und b.celsius() sind gleich, wenn
      (a) beide nullopt sind, ODER
      (b) beide einen Wert tragen UND
          (std::isnan(*a.celsius()) UND std::isnan(*b.celsius())) ODER
          *a.celsius() == *b.celsius() (gewoehnlicher double-Vergleich,
          fuer endliche Werte UND fuer +Inf/+Inf bzw. -Inf/-Inf bereits
          korrekt gemaess IEEE 754 - NUR NaN == NaN ist in IEEE 754 false
          und wird deshalb explizit als Sonderfall behandelt: zwei NaN-
          Proben zum selben Zeitstempel sind semantisch "derselbe defekte
          Messwert nochmal gesendet", kein neuer Widerspruch).
    identity() wird ueber std::optional<SensorIdentity>::operator==
    verglichen (SensorIdentity ist ein einfacher Werttyp ohne NaN-Analogon).
    status() wird ueber gewoehnliche Enum-Gleichheit verglichen.

  Zeitstempel < zuletzt akzeptierter Zeitstempel (ruecklaeufig/verspaetet)
    -> RejectedRetrograde.

  Zeitstempel > nowMonotonicMs (aus der Zukunft, siehe Abschnitt 9a) ->
    RejectedFuture.

  ERSTE Probe ueberhaupt (kein vorheriges Accepted vorhanden) -> die
    Duplikat-/Konflikt-/Retrograde-Regeln oben SETZEN alle eine
    "zuletzt akzeptierte Probe" voraus und sind daher auf die erste Probe
    nicht anwendbar (kein Vergleichspartner). Es verbleibt ausschliesslich
    die Zukunftspruefung: Zeitstempel <= nowMonotonicMs -> Accepted;
    Zeitstempel > nowMonotonicMs -> RejectedFuture. KORREKTUR RUNDE 4: diese
    Randbedingung war vorher nicht explizit benannt - "Zeitstempel >
    zuletzt akzeptierter Zeitstempel" ist ohne einen vorherigen Wert
    unterspezifiziert.

  sonst, mit vorhandener zuletzt akzeptierter Probe (Zeitstempel > zuletzt
    akzeptierter Zeitstempel UND <= nowMonotonicMs) -> Accepted, durchlaeuft
    Abschnitt 10.

Keine der Ablehnungsdispositionen (DuplicateIgnored, RejectedTimestampConflict,
RejectedRetrograde, RejectedFuture) zaehlt als "ungueltige Probe" im Sinne der
Zustandsmaschine (Abschnitt 8) UND KEINE aktualisiert
lastAcceptedSampleAgeMs (Abschnitt 12) - es sind Transport-/
Protokollanomalien der Zufuhr, keine Evidenz gegen die Sensorgesundheit und
keine neue "letzte Probe" im Sinne des Diagnosevertrags. Nur eine
tatsaechlich Accepted, aber danach in Abschnitt 10.1/10.2 als unplausibel
erkannte Probe zaehlt in consecutiveInvalidCount.

ROM-Wechsel-Erkennung (Praezisierung Runde 2): identity() zweier
aufeinanderfolgender AKZEPTIERTER Proben wird verglichen. Sind BEIDE
identity()-Werte gesetzt (nicht nullopt) und unterschiedlich ->
IdentityMismatch (Abschnitt 8/11). Ist mindestens einer der beiden nullopt,
wird KEIN ROM-Wechsel gemeldet (fehlende Evidenz; ein Uebergang von
"unbekannt" zu "erstmals bekannt" ist normales Anlaufverhalten, kein
Fehler). Explizit (Korrektur Runde 4, Klarstellung statt Verhaltensaenderung):
Ein Identitaetswechsel zum EXAKT GLEICHEN Zeitstempel wie die zuletzt
akzeptierte Probe erreicht diese Pruefung nie, weil die abweichende
identity() bereits oben als RejectedTimestampConflict disponiert wurde und
damit gar nicht erst akzeptiert wird - IdentityMismatch setzt zwei
AKZEPTIERTE, also zeitlich verschiedene Proben voraus. Das ist beabsichtigt,
nicht lueckenhaft: zwei widerspruechliche Identitaeten zum selben
Erfassungszeitpunkt sind ein Zeitstempel-/Protokollproblem der Zufuhr
(Abschnitt 9b), kein sachlicher Beleg fuer einen echten physischen
ROM-Wechsel.

Weitere behandelte Faelle:

  extrem grosse Zeitluecke: erlaubt (kein Fehler an sich, Accepted), fuehrt
    aber bei Ueberschreiten von kMaxStaleAgeMs zum Zustandsuebergang wie in
    Abschnitt 8 beschrieben; nach der Luecke beginnt die
    Aenderungsratenpruefung ohne Vorwert (erste Probe nach Luecke kann nicht
    als "Sprung" bewertet werden, da kein unmittelbar vorheriger gueltiger
    Wert existiert).
  nicht endlicher Wert (NaN/Inf) in celsius(): Accepted auf
    Zeitstempelebene (Zeitstempelregeln kennen den Rohwert nicht), aber in
    Abschnitt 10.2 als eigene SensorFaultReason::NonFinite erkannt
    (std::isfinite) - siehe Abschnitt 12 fuer den vollstaendigen Enum.
```

`GENERIC_INPUT_CONTRACT: PASS`
`DUPLICATE_COMPARES_FULL_SAMPLE: PASS`
`NONFINITE_DUPLICATE_SEMANTICS_DEFINED: PASS`
`FIRST_SAMPLE_DISPOSITION_DEFINED: PASS`

## 10. Verarbeitungspipeline

Reihenfolge exakt wie in Abschnitt 5 des urspruenglichen Auftrags und in
`docs/ARCHITECTURE.md`/`docs/SENSOR_TUNING_COMMISSIONING.md` vorgegeben.
Jede Stufe ist eine eigene kleine, testbare Komponente (SRP), orchestriert
von einer duennen `SensorQualityPipeline`. Eine Probe durchlaeuft Abschnitt
10 ueberhaupt nur, wenn sie gemaess Abschnitt 9b als `Accepted` disponiert
wurde.

### 10.0 SensorQualityConfig (vollstaendig, gueltig-by-construction, NaN/Inf-sicher)

Nach dem `StateStoreKey`-Muster (privater Konstruktor, statisches `create()`,
`...CreateResult{status; optional<T>}`) validiert die Konfiguration alle
Instanzparameter bereits bei Erzeugung, statt fehlerhafte Kombinationen erst
zur Laufzeit der Pipeline zu bemerken. KORREKTUR RUNDE 2: `create()` prueft
JEDEN `double`-Parameter zuerst explizit mit `std::isfinite()`, bevor
irgendeine Bereichs-/Beziehungspruefung erfolgt - NaN oder Inf duerfen keine
der nachfolgenden Vergleiche (z. B. `min >= max`) unbemerkt passieren
lassen (`NaN >= x` ist immer `false`, wuerde also eine Bereichsverletzung
verschleiern):

```cpp
// sensor_quality_config.hpp
enum class SensorQualityConfigStatus : uint8_t {
    Success,
    InvalidMedianWindowSize,       // 0, gerade, oder > sensor_limits::kMaxMedianWindowSize
    // Mindestens einer der double-Parameter ist NaN oder Inf. Wird VOR
    // jeder Bereichs-/Beziehungspruefung erkannt.
    NonFiniteParameter,
    InvalidLowPassTimeConstant,    // <= 0.0
    InvalidPlausibleRange,         // min >= max, oder ausserhalb sensor_limits-Aussengrenze
    InvalidRateOfChangeLimit,      // <= 0.0
    InvalidStaleAgeThreshold,      // 0, oder > sensor_limits::kMaxStaleAgeCeilingMs
    InvalidConsecutiveInvalidLimit, // 0, oder > sensor_limits-Obergrenze
    // minConsecutiveValidSamples < 2 (KORREKTUR RUNDE 5: vorher nur == 0,
    // aber die kanonische Spezifikation verlangt "MEHRERE gueltige Proben",
    // 1 ist keine Mehrzahl), ODER minRecoveryStabilityDurationMs == 0, ODER
    // minRecoveryStabilityDurationMs >
    // sensor_limits::kMaxRecoveryStabilityDurationCeilingMs (Korrektur
    // Runde 4: vorher wurde nur minConsecutiveValidSamples == 0 geprueft,
    // minRecoveryStabilityDurationMs blieb vollstaendig unvalidiert - ein
    // Aufrufer haette 0 oder eine willkuerlich grosse Zahl uebergeben
    // koennen, obwohl docs/SENSOR_TUNING_COMMISSIONING.md ausdruecklich
    // "mehrere gueltige Proben UND eine Stabilitaetszeit" verlangt).
    InvalidRecoveryThresholds,
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

Validierungsreihenfolge in `create()`: (1) `std::isfinite()` auf
`lowPassTauSeconds`, `minPlausibleCelsius`, `maxPlausibleCelsius`,
`maxRateOfChangeCelsiusPerSecond` - jede Verletzung -> `NonFiniteParameter`;
(2) `medianWindowSize` (0/gerade/zu gross); (3) `lowPassTauSeconds <= 0`;
(4) `minPlausibleCelsius >= maxPlausibleCelsius` ODER ausserhalb der
firmwarefesten Aussengrenze aus `sensor_limits.hpp`; (5)
`maxRateOfChangeCelsiusPerSecond <= 0`; (6) `maxStaleAgeMs`; (7)
`maxConsecutiveInvalid`; (8) `minConsecutiveValidSamples < 2` ODER
`minRecoveryStabilityDurationMs == 0` ODER `minRecoveryStabilityDurationMs
> sensor_limits::kMaxRecoveryStabilityDurationCeilingMs` (Korrektur Runde 5:
`< 2` statt `== 0` - siehe unten; Korrektur Runde 4 - vorher pruefte Schritt
8 nur `minConsecutiveValidSamples`, siehe unten). Jede Stufe prueft nur, was
vorher nicht schon verworfen wurde - keine doppelte Fehlerklassifikation.

Wiedererkennungsparameter vollstaendig validiert (Korrektur Runde 4,
`docs/SENSOR_TUNING_COMMISSIONING.md` verlangt ausdruecklich "mehrere
gueltige Proben UND eine Stabilitaetszeit" - beide Grenzen muessen daher
tatsaechlich existieren, nicht nur eine):

```text
minConsecutiveValidSamples >= 2 (KORREKTUR RUNDE 5: vorher nur > 0,
  also 1 als Untergrenze zugelassen)
  (0 wuerde bedeuten: Wiedererkennung ohne eine einzige neue Probe; 1
  waere ebenfalls unzulaessig, denn die kanonische Spezifikation
  (docs/SENSOR_TUNING_COMMISSIONING.md) verlangt woertlich "mehrere
  gueltige Proben" - eine einzelne Probe ist keine Mehrzahl. 0 UND 1
  werden daher gleichermassen abgelehnt, ab 2 gilt die Anforderung als
  erfuellt)
minRecoveryStabilityDurationMs > 0
  (0 wuerde die Stabilitaetszeit-Anforderung vollstaendig entwerten -
  vorher UNVALIDIERT, echte Luecke gegenueber SENSOR_TUNING_COMMISSIONING.md)
minRecoveryStabilityDurationMs <= sensor_limits::kMaxRecoveryStabilityDurationCeilingMs
  (firmwarefeste Obergrenze nach demselben Muster wie
  kMaxStaleAgeCeilingMs, verhindert eine versehentlich praktisch nie
  erfuellbare Wiedererkennungsbedingung)
```

Keine weitere Beziehung zwischen den Wiedererkennungsparametern und den
uebrigen Config-Feldern ist fachlich zwingend: `minConsecutiveValidSamples`
(Laenge der geforderten GUELTIGEN Folge fuer eine Rueckkehr zu Valid) und
`maxConsecutiveInvalid` (Laenge der zulaessigen UNGUELTIGEN Folge WAEHREND
Valid, bevor Failed erreicht wird) zaehlen strukturell unterschiedliche
Ereignisse in unterschiedlichen Zustaenden und muessen einander nicht
beschraenken; `minRecoveryStabilityDurationMs` misst ab dem Beginn der
Wiedererkennungsfolge selbst und ist damit unabhaengig von
`maxStaleAgeMs` (das das Alter des LETZTEN gueltigen Werts vor Beginn der
Folge misst). Eine erzwungene numerische Kopplung ohne fachlichen Grund
waere ein KISS-Verstoss (unnoetige Kopplung ohne Nutzen).

`create()` prueft ausserdem gegen firmwarefeste Aussengrenzen aus
`sensor_limits.hpp` (siehe Abschnitt 14); die konkreten Tuning-Werte selbst
(Fenstergroesse, Tau, Plausibilitaetsband, Ratenlimit, Alters-/
Wiedererkennungsschwellen) bleiben `TBD_COMMISSIONING` (Abschnitt 18) und
werden ausschliesslich ueber diese Konfiguration injiziert, nie im Pipeline-
Code selbst verzweigt (siehe Abschnitt 15, Punkt O). `SensorOffset`
(Abschnitt 13a) ist bewusst NICHT Teil von `SensorQualityConfig`: Es ist ein
Kalibrierwert pro Probe/Sensorinstanz, keine Pipeline-Verhaltensparametrierung
(SRP-Trennung).

`SENSOR_QUALITY_CONFIG_DEFINED: PASS`
`NONFINITE_CONFIG_REJECTED: PASS`
`RECOVERY_DURATION_VALIDATED: PASS`
`RECOVERY_REQUIRES_MULTIPLE_SAMPLES: PASS`

### 10.1 Transport-/CRC-/Messstatusstufe (generisch, keine sensor-/treiberspezifischen Konstanten)

```text
status() != Ok -> Probe ungueltig, Ursache uebernimmt direkt den
  TemperatureSampleStatus (BusFault/CrcFault/MissingSample/
  KnownInvalidMeasurement, siehe Abschnitt 9.1/12), keine weitere Stufe
  verarbeitet celsius().

#20 selbst kennt KEINE sensor-/treiberspezifischen Zahlenkonstanten (z. B.
keinen DS18B20-Einschalt- oder Diskonnektwert) und sensor_limits.hpp
enthaelt keine solche Konstante. Ein spaeterer Adapter (#30) erkennt
sensor-/treiberspezifische bekannte Fehlerwerte selbst und liefert dafuer
bereits status = KnownInvalidMeasurement; #20 behandelt diesen Status
generisch wie jeden anderen Transportfehler. Damit bleibt device_platform
frei von Sensor-/Treiberkenntnis (ADR-013, lib/device_platform/AGENTS.md).
```

`DS18B20_CONSTANTS_OUTSIDE_GENERIC_PLATFORM: PASS`

### 10.2 Physikalischer Wertebereich und Aenderungsrate

```text
Nur erreicht, wenn status() == Ok UND celsius().has_value() (durch
Konstruktion immer gemeinsam wahr, siehe Abschnitt 9.1).

nicht endlicher Rohwert (!std::isfinite(*celsius())) -> ungueltig,
  Ursache = NonFinite.
celsius() ausserhalb [kAbsoluteMinCelsius, kAbsoluteMaxCelsius]
  (firmwarefest, sensor_limits.hpp) -> ungueltig, Ursache = OutOfRange.
Aenderungsrate = |celsius() - letzterGueltigerRohwert| /
  ((monotonicTimestampMs() - letzterGueltigerTimestampMs) / 1000.0), nur
  berechnet wenn ein unmittelbar vorheriger gueltiger Wert existiert. Da
  RejectedTimestampConflict-Proben (Abschnitt 9b) nie akzeptiert werden,
  ist die Zeitdifferenz zwischen zwei aufeinanderfolgenden akzeptierten
  Proben immer echt > 0 - keine kuenstliche Mindest-Zeitdifferenz noetig.
  Ueberschreitet die Rate den in SensorQualityConfig konfigurierten
  kMaxRateOfChangeCelsiusPerSecond -> ungueltig, Ursache =
  RateOfChangeExceeded. Ein einzelner Sprung fuehrt NICHT sofort zu FAILED
  (siehe Akzeptanzkriterium "einzelne Fehlerwerte stoppen nicht sofort
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
  Phantomwerten. Bereits NACH DEM ERSTEN gueltigen Beitrag liefert der
  Median einen echten Wert (den einzigen bisher vorhandenen) - er ist NICHT
  erst nach vollstaendig gefuelltem Fenster verfuegbar (Korrektur Runde 2,
  siehe Abschnitt 12).
Nur PLAUSIBLE Proben (nach 10.1/10.2 gueltig) gelangen ins Fenster.
Nach FAILED/ROM-Wechsel: Fenster wird geleert (siehe Abschnitt 8).
Feste Ringpuffer-Kapazitaet, keine Heapallokation (std::array, kein
std::vector).
```

### 10.4 Kalibrier-Offset

`SensorOffset` ist bereits in Abschnitt 13a vollstaendig,
gueltig-by-construction und NaN/Inf-sicher definiert; die Bindung an eine
`SensorIdentity` erfolgt ueber `SensorCalibration` (Abschnitt 13b, Korrektur
Runde 4 - keine erneute Definition hier).

```text
#20 nimmt ausschliesslich eine bereits erfolgreich konstruierte, optionale
SensorCalibration entgegen (SensorQualityPipeline::setCalibration(), siehe
Abschnitt 13b); #20 selbst validiert nicht erneut und nimmt keine
Persistenz- oder UI-Validierung vor (siehe Nicht-Scope). Der PRO PROBE
tatsaechlich angewendete Offset wird bei jeder akzeptierten Probe neu
bestimmt (Regel siehe Abschnitt 13b: identity()-Uebereinstimmung noetig,
sonst intern 0.0 neutral auf den Medianfilter-Ausgang angewendet OHNE dies
als appliedOffset im Snapshot auszuweisen - appliedOffset bleibt in diesem
Fall nullopt statt 0.0, Abschnitt 12/13b) - nicht einmalig
zwischengespeichert. Ein Wechsel der gesetzten Kalibrierung (setCalibration())
waehrend laufender
Filterung wirkt erst auf die naechste eingehende Probe; bereits im
Medianfenster befindliche Rohwerte werden NICHT rueckwirkend korrigiert
(Medianfenster enthaelt Rohwerte, Offset wird NACH dem Medianfilter
angewendet, siehe Reihenfolge oben – ein Offsetwechsel kann daher nie zu
einer Vermischung unterschiedlich korrigierter Werte innerhalb eines
Medianfensters fuehren).
```

Effektiver Offsetwechsel bei gleichbleibender Identitaet (KORREKTUR RUNDE
5 - vorher unspezifiziert):

```text
"effektiver Offset" (interne Rechengroesse, NICHT dasselbe wie das
Snapshot-Feld appliedOffset - siehe Begriffsklaerung unten) ist der Offset-
Wert, der tatsaechlich zur Korrektur des Medianfilter-Ausgangs verwendet
wird: entweder calibration->offset().celsius() bei passender identity(),
oder 0.0 (neutraler Fallback) bei fehlender/nicht passender Kalibrierung -
siehe Abschnitt 13b. Der zuletzt fuer den Tiefpass verwendete effektive
Offset wird als interner Zustand `lastFilterEffectiveOffsetCelsius`
zusammen mit gefiltert_alt gefuehrt (Abschnitt 10.5).

Der Medianpuffer enthaelt ausschliesslich ROHWERTE (Abschnitt 10.3); der
Tiefpasszustand gefiltert_alt (Abschnitt 10.5) enthaelt dagegen bereits
offsetkorrigierte Werte. Ein reiner Offsetwechsel bei GLEICHER
Sensoridentitaet ist deshalb, anders als ein physischer ROM-Wechsel, KEIN
Grund fuer einen vollstaendigen Filterreset.

REIHENFOLGE-/VORRANGREGEL (praezisiert gegenueber der ersten Fassung dieser
Korrektur, die den Vorrang nur in Prosa, nicht operational festlegte): Fuer
JEDE Probe, die den Filter aktualisiert, wird ZUERST wie bisher (Abschnitt
8/9a) geprueft, ob diese Probe einen VOLLSTAENDIGEN Filterreset ausloest
(Wiedererkennung aus FAILED heraus ODER bestaetigter ROM-Wechsel/
IdentityMismatch). Nur wenn KEINER der beiden Faelle zutrifft, kommt die
folgende Verschiebungsregel ueberhaupt zur Anwendung - sie ist also nicht
nebenlaeufig zum vollstaendigen Reset, sondern dessen ausdruecklicher
ELSE-Zweig:

  KEIN vollstaendiger Reset fuer diese Probe (siehe oben) UND effektiver
  Offset aendert sich zwischen dieser und der vorherigen, tatsaechlich in
  den Filter eingeflossenen Probe (delta = neuer_effektiver_Offset -
  alter_effektiver_Offset, delta != 0):
    VOR der ueblichen Tiefpass-Berechnung (Abschnitt 10.5) wird
    gefiltert_alt um delta verschoben (gefiltert_alt' = gefiltert_alt +
    delta), ERST DANACH erfolgt die normale Exponentialglaettung mit dem
    neuen korrigierten Wert. Ohne diese Verschiebung wuerde eine reine
    Rekalibrierung dem Tiefpass wie ein echter Temperatursprung erscheinen
    und langsam statt sofort korrekt "eingeschwungen" wirken - genau der im
    Auftrag beschriebene Fehler.

  KEIN vollstaendiger Reset UND delta == 0 (kein tatsaechlicher Wechsel,
  z. B. Wechsel von "keine Kalibrierung" zu einer expliziten Kalibrierung
  mit Offset 0.0, oder gar kein setCalibration()-Aufruf zwischen zwei
  Proben): keine Verschiebung, normale Fortsetzung.

  VOLLSTAENDIGER Reset fuer diese Probe (FAILED-Wiedererkennung ODER
  ROM-Wechsel): die Verschiebungsregel entfaellt vollstaendig - sowohl
  gefiltert_alt ALS AUCH `lastFilterEffectiveOffsetCelsius` werden
  gemeinsam verworfen (Abschnitt 8) und beim ersten neuen
  Wiedererkennungswert direkt neu gesetzt (kein "Nachschleppen" eines
  Deltas aus dem Zustand vor dem Reset in den neuen). Ein delta, das
  ALLEIN aus dem Identitaetswechsel selbst entstuende (z. B. weil der neue
  Sensor keine passende Kalibrierung hat und der effektive Offset dadurch
  auf 0.0 faellt), wird NICHT als Verschiebung interpretiert - der volle
  Reset hat in jedem Fall Vorrang und macht die Delta-Berechnung fuer
  diese Probe gegenstandslos.

  erste Probe, die ueberhaupt den Filter initialisiert (kein vorheriges
  gefiltert_alt vorhanden): keine Verschiebung noetig - gefiltert_alt wird
  wie bisher direkt auf den korrigierten Rohwert gesetzt (Abschnitt 10.5).

Begriffsklaerung (verhindert ein Wiederaufleben der in Runde 4 behobenen
Sentinel-Mehrdeutigkeit): "effektiver Offset" ist eine interne, immer
konkrete double-Rechengroesse (0.0 bei fehlender Kalibrierung ist hier
gleichbedeutend mit einer expliziten Kalibrierung auf 0.0 - beide fuehren
zu delta == 0 im obigen Vergleich, wenn sich sonst nichts aendert). Das
Snapshot-Feld `appliedOffset` (Abschnitt 12/13b) bleibt UNVERAENDERT
std::optional<double> und zeigt weiterhin ausschliesslich, OB eine
tatsaechlich passende Kalibrierung vorlag - diese beiden Groessen duerfen
nicht verwechselt werden.
```

`CALIBRATION_CHANGE_FILTER_STATE_CONSISTENT: PASS`
`MISSING_VS_ZERO_CALIBRATION_DISTINCT: PASS`

### 10.5 Sensorbezogener Tiefpass

```text
Einfacher zeitkonstantenbasierter Exponentialfilter:
  gefiltert_neu = gefiltert_alt + (korrigierterWert - gefiltert_alt) *
                  (1 - exp(-dtSeconds / tauSeconds))
  dtSeconds = Differenz zwischen dem monotonicTimestampMs() der aktuellen
  PLAUSIBLEN Probe (die den Tiefpass ueberhaupt erreicht, siehe 10.1-10.3)
  und dem monotonicTimestampMs() der VORHERIGEN Probe, die tatsaechlich in
  den Filter eingeflossen ist - NICHT einfach der vorherigen AKZEPTIERTEN
  Probe. NICHT aus nowMonotonicMs oder irgendeiner anderen Zeitquelle
  (Korrektur Runde 2 - siehe Abschnitt 9a: die Pipeline haelt ohnehin keine
  ITimeSource-Referenz; "aus ITimeSource-Zeitstempeln" war eine
  irrefuehrende Restformulierung). KORREKTUR RUNDE 4 (behebt einen durch die
  Filterreset-Korrektur in Abschnitt 8 neu entstandenen Fehler): "vorherige
  AKZEPTIERTE Probe" ist seit der transienten-STALE-Filtererhaltung
  (Abschnitt 8) NICHT mehr dasselbe wie "vorherige Probe, die den Filter
  aktualisiert hat" - eine akzeptierte, aber unplausible Probe (z. B.
  OutOfRange/RateOfChangeExceeded) durchlaeuft 10.1/10.2 und erreicht den
  Tiefpass gar nicht (10.3: "Nur PLAUSIBLE Proben ... gelangen ins Fenster").
  Bei Verwendung von "letzte akzeptierte" statt "letzte tatsaechlich in den
  Filter eingeflossene" Probe wuerde dtSeconds bei Valid -> kurz Stale (eine
  oder mehrere unplausible Proben, Filter bleibt erhalten) -> Valid die
  Zeitluecke systematisch UNTERSCHAETZEN (dt gemessen ab der letzten
  unplausiblen statt der letzten tatsaechlich verarbeiteten Probe), der
  Filter wuerde dadurch zu LANGSAM statt korrekt auf die neue Probe
  reagieren. Deckungsgleich mit Abschnitt 10.2, das fuer die
  Aenderungsratenpruefung bereits korrekt "letzterGueltigerTimestampMs"
  verwendet, nicht "letzter akzeptierter Zeitstempel". Keine feste
  Zykluszeit-Annahme, auch wenn der Regelzyklus nominal ~2 s betraegt
  (robust gegen Jitter/Luecken).
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
Effektiver Offsetwechsel bei GLEICHER Sensoridentitaet (kein FAILED/ROM-
  Wechsel): gefiltert_alt wird VOR der obigen Formel um die Differenz des
  effektiven Offsets verschoben, NICHT komplett zurueckgesetzt - siehe
  Abschnitt 10.4 fuer die vollstaendige Regel und Begruendung (Korrektur
  Runde 5).
```

`FILTER_PIPELINE_DEFINED: PASS`
`ITIME_SOURCE_CONTRADICTIONS: 0`
`CALIBRATION_CHANGE_FILTER_STATE_CONSISTENT: PASS`

## 11. Widerspruchs- und Plausibilitaetsbewertung (rollenuebergreifend)

Der urspruengliche Auftrag (Abschnitt 8) verlangt, vier Faelle zu
unterscheiden. Diese werden hier vollstaendig durchdekliniert, statt
implizit zu bleiben:

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
      changeRateCelsiusPerSecond, lastAcceptedSampleAgeMs/
      lastValidSampleAgeMs, lastFaultReason (alle bereits Teil des
      Diagnosevertrags, Abschnitt 12) - und trifft selbst KEINE
      Schuldzuweisung/Verdaechtigung zwischen Rollen (KEINE
      Verdachtsmarkierung, siehe Korrektur in Abschnitt 7), da #20 (siehe
      Abschnitt 13) keine Rollenkenntnis besitzt und daher strukturell nicht
      wissen kann, welche andere Pipeline-Instanz "die andere Rolle" ist.
      #21/#24 berechnen Punkt 4 ausschliesslich aus den hier bereits
      bereitgestellten Feldern mehrerer Instanzen, ohne dass #20 dafuer
      erweitert werden muss.
```

Zusaetzlich, innerhalb einer einzelnen Rolle: Eine dauerhaft dem Prozess
entsprechend dennoch nahezu unveraenderte Produkttemperatur wird innerhalb
der EIGENEN Pipeline nicht als Fehler gewertet (kein "unveraenderter
Wert"-Kriterium in der Aenderungsratenpruefung – nur Sprung-, nicht
Stillstandserkennung, siehe 10.2).

Sensoridentitaets-/ROM-Wechsel (innerhalb einer Rolle) IST Teil von #20:
identity() aendert sich zwischen zwei aufeinanderfolgenden akzeptierten
Proben, sofern BEIDE bekannt sind (Abschnitt 9b) -> als
Plausibilitaetsereignis markiert, Filterzustand wird verworfen (Abschnitt
8/10.3), Wiedererkennung wie nach FAILED erforderlich.

Zusammengefasst (Klarstellung, unveraendert gueltig): #20 liefert
ausschliesslich Evidenz je EINZELNER Pipeline-Instanz. Jede
rollenuebergreifende Bewertung, jeder Vergleich zweier Rollen und jede
daraus abgeleitete Schuldzuweisung oder Verdaechtigung bleibt vollstaendig
bei #21/#24 (siehe auch Abschnitt 7, Scopegrenzen).

`CROSS_ROLE_SCOPE_CONSISTENT: PASS`
`CROSS_ROLE_CONTRADICTIONS: 0`

## 12. Ausgabe- und Diagnosevertrag

Bewertung Mega-Struktur vs. mehrere kleine Typen: Ein einzelner
`SensorQualitySnapshot` pro Pipeline-Instanz (nicht pro Rolle als
Sammelstruktur ueber alle drei Sensoren) ist gerechtfertigt, weil alle
Felder zu GENAU EINER Sensorrolle gehoeren und immer gemeinsam als ein
konsistenter Zustand entstehen (ein Snapshot = eine Momentaufnahme).
Eine Aufteilung in mehrere Kleintypen wuerde hier nur kuenstliche Kopplung
zwischen Aufrufern erzeugen, ohne eine echte Verantwortungsgrenze
abzubilden (KISS). Eine Sammelstruktur ueber ALLE Sensorrollen wird bewusst
NICHT gebaut, weil "welche Rollen es gibt" ein fermentation_app-Konzept ist
(siehe Abschnitt 13) – `device_platform` kennt nur "eine Pipeline-Instanz
pro injizierter Quelle".

`SampleDisposition` (Zeitstempel-/Zufuhranomalien, Abschnitt 9b) und
`SensorFaultReason` (fachliche Sensorfehlerursachen, Abschnitt 10) bleiben
zwei getrennte kleine Enums, keine vermischte Klassifikation (SRP):

```cpp
// sensor_quality_pipeline.hpp (SampleDisposition ist ingest()s
// Rueckgabewert - hier co-lokalisiert statt in einer separaten
// Eingangsvertragsdatei, siehe Abschnitt 9a)
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
    std::optional<SensorIdentity> identity;         // zuletzt bekannte Identitaet, nullopt falls nie eine akzeptierte Probe mit bekannter Identitaet vorlag
    SensorQuality quality;
    std::optional<double> rawCelsius;               // Wert der letzten AKZEPTIERTEN Probe mit Messwert (status==Ok), UNABHAENGIG von Plausibilitaet; nullopt nur vor der allerersten solchen Probe. KORREKTUR RUNDE 3: vorher faelschlich an "Accepted-und-plausibel" gekoppelt - das haette OutOfRange-/RateOfChangeExceeded-Proben trotz vorhandenem Rohwert verschwiegen und Abschnitt 10.5 sowie SENSOR_TUNING_COMMISSIONING.md widersprochen ("ein extremer Rohwert darf nicht durch einen langsamen Filter verdeckt werden").
    std::optional<double> correctedCelsius;         // Medianfilter-Ausgang + Offset; bereits ab dem ERSTEN plausiblen Medianbeitrag vorhanden (nicht erst bei vollem Fenster, siehe Abschnitt 10.3), NICHT rawCelsius + Offset
    std::optional<double> filteredCelsius;          // Tiefpass-Ausgang; ab demselben ersten Beitrag wie correctedCelsius vorhanden (Tiefpass ist ab der ersten Probe wohldefiniert)
    // std::optional statt double (KORREKTUR RUNDE 4): nullopt bedeutet
    // "keine zur aktuellen Identitaet passende Kalibrierung angewendet"
    // (Abschnitt 13b); hat einen Wert genau dann, wenn eine tatsaechlich
    // passende SensorCalibration angewendet wurde (auch wenn dieser Wert
    // zufaellig 0.0 ist). Ein reines `double appliedOffset` koennte
    // "kalibriert auf 0.0" nicht von "keine passende Kalibrierung, neutraler
    // Fallback" unterscheiden - genau der 0-Sentinel-Fehler, der in Runde 3
    // bereits fuer SensorIdentity vermieden wurde. correctedCelsius (oben)
    // verwendet INTERN unabhaengig davon immer 0.0 als Fallback, wenn
    // appliedOffset == nullopt ist - der Regelpfad wird durch eine fehlende
    // Kalibrierung nicht blockiert, nur die Diagnoseevidenz macht den
    // Unterschied sichtbar.
    std::optional<double> appliedOffset;
    // Alter der letzten AKZEPTIERTEN Probe (Abschnitt 9b) - abgelehnte und
    // doppelte Proben (DuplicateIgnored/RejectedTimestampConflict/
    // RejectedRetrograde/RejectedFuture) aktualisieren dieses Feld NICHT.
    // Umbenannt gegenueber der Vorversion (dort "lastSampleAgeMs", mehrdeutig).
    std::optional<uint64_t> lastAcceptedSampleAgeMs;
    std::optional<uint64_t> lastValidSampleAgeMs;   // Alter der letzten ZUSAETZLICH plausiblen (10.1/10.2-gueltigen) Probe
    SensorFaultReason lastFaultReason;
    uint16_t consecutiveInvalidCount;
    uint16_t recoveryProgressCount;   // aufeinanderfolgende gueltige Proben waehrend Wiedererkennung
    std::optional<double> changeRateCelsiusPerSecond; // nullopt ohne unmittelbaren gueltigen Vorwert
};
```

`controlValueUsable`/`diagnosticOnly` bleiben entfallen (bereits in Runde 1
entfernt): Der Regelwert wird von JEDEM Konsumenten direkt und
ausschliesslich aus bereits vorhandenen Feldern abgeleitet:

```text
Regelwert verwendbar  <=>  quality == SensorQuality::Valid
                           UND filteredCelsius.has_value()
```

Kein Trend-/Aenderungsratenfeld wird weggelassen: `SAFETY_COMPONENT_FAULTS.md`
verlangt fuer den Kuehlkoerpersensor ausdruecklich eine Aenderungsraten-
/Trendueberwachung - das Feld ist daher verpflichtend (als `optional`).

KORREKTUR RUNDE 3 - Reichweite des `filteredCelsius.has_value()`-Konjunkts:
Seit Abschnitt 10.3/12 den Medianbeitrag ab der ERSTEN plausiblen Probe
liefern und `quality` erst nach `kMinConsecutiveValidSamples` aufeinander-
folgenden gueltigen Proben `Valid` erreicht, gibt es nach Slice 2 keinen
erreichbaren Zustand mit `quality == Valid` UND `filteredCelsius == nullopt`
mehr - der Konjunkt ist dort eine defensive Invariante, kein tatsaechlicher
Unterscheider. In Slice 1 dagegen existiert `filteredCelsius` ueberhaupt noch
nicht (kein Tiefpass implementiert, kein Konsument verdrahtet), daher ist die
Regel dort bewusst und dauerhaft `false`, bis Slice 2 den Filter liefert. Die
Regel bleibt trotzdem so formuliert (nicht auf `quality == Valid` allein
verkuerzt), weil sie ohne Codeaenderung automatisch korrekt wird, sobald
Slice 2 landet - keine zwei getrennten Definitionen fuer "vor" und "nach"
Slice 2 noetig.

`DIAGNOSTIC_CONTRACT_DEFINED: PASS`
`OPTIONAL_DIAGNOSTIC_VALUES: PASS`
`REDUNDANT_STATUS_FLAGS_REMOVED: PASS`
`SNAPSHOT_SEMANTICS_CONSISTENT: PASS`

## 13. Modul- und Abhaengigkeitsrichtung

Die GESAMTE fachliche Pipeline aus #20 (Zustandsmaschine, Plausibilitaet,
Filter, Diagnosevertrag) wird in `lib/device_platform/` verortet, nicht in
`lib/fermentation_app/` - Entscheidung C, siehe Abschnitt 19.

Begruendung:

```text
- lib/device_platform/AGENTS.md erlaubt im Abschnitt "Erlaubt" ausdruecklich
  "allgemeine Sensorqualitaet, Filter und begrenzte Reglerbausteine".
- Keine der #20-Bullet-Points (Zustaende, CRC/Bus, Wertebereich, Median,
  Tiefpass, ROM-Offset, Wiedererkennung) nennt Joghurt, Kefir, Kombucha oder
  eine Fermentationsphase.
- ADR-013 (Wiederverwendbare ESP32-Geraeteplattform) begruendet exakt diesen
  Schnitt und nennt "Sensorqualitaet" explizit als geraeteuebergreifende
  Grundfunktion.
- KEIN geraetespezifischer Name (z. B. "Schrankluft", "Produkt") wird als
  Typ oder Methode in device_platform verwendet. Die drei konkreten Rollen
  bleiben ein fermentation_app-/Composition-Root-Konzept.
- SensorQualitySnapshot (Abschnitt 12) enthaelt bewusst KEIN Rollenfeld, nur
  eine anwendungsneutrale SensorIdentity.
```

Datei-Zuordnung:

```text
lib/device_platform/src/
  temperature_source.hpp          (BESTEHEND, erweitert - Abschnitt 9.1)
  sensor_identity.hpp              (neu, eigenstaendig, Abschnitt 9.0/13a)
  sensor_quality.hpp               (neu, enum SensorQuality)
  sensor_quality_config.hpp        (neu, Abschnitt 10.0)
  sensor_quality_snapshot.hpp      (neu, enum SensorFaultReason + Snapshot)
  sensor_offset.hpp                (neu, Abschnitt 13a)
  sensor_calibration.hpp           (neu, Abschnitt 13b, Korrektur Runde 4)
  sensor_median_filter.hpp/.cpp    (neu)
  sensor_lowpass_filter.hpp/.cpp   (neu)
  sensor_quality_pipeline.hpp/.cpp (neu, Orchestrator + SampleDisposition)
  sensor_limits.hpp                (neu, firmwarefeste Grenzen)

lib/device_platform_test_support/src/
  mock_temperature_source.hpp/.cpp (BESTEHEND, auf erweiterten Port
                                     nachgezogen - Abschnitt 9.1/14)

test/ (siehe Abschnitt 17a fuer die vollstaendige, nach Topics aufgeteilte
       Liste statt einer einzelnen Testdatei)
```

`fermentation_app` und `src/main.cpp`/`main/app_main.cpp` werden von #20
NICHT geaendert: Es gibt noch keinen Verbraucher (die Verdrahtung von drei
rollenbenannten Pipeline-Instanzen ist #21s bzw. der spaeteren Composition-
Root-Aufgabe).

### 13a. SensorOffset: gueltig-by-construction, NaN/Inf-sicher, ohne storage_types.hpp-Abhaengigkeit

`SensorIdentity` ist bereits vollstaendig in Abschnitt 9.0 definiert
(dorthin verschoben, da sie jetzt Teil des kanonischen Eingangsvertrags
ist, nicht mehr eines separaten `sensor_sample.hpp`).

```cpp
// sensor_offset.hpp – gueltig-by-construction wie StateStoreKey
enum class SensorOffsetStatus : uint8_t {
    Success,
    // celsius ist NaN oder Inf. Wird VOR der Bereichspruefung erkannt.
    NonFinite,
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
    SensorOffsetStatus status{SensorOffsetStatus::NonFinite};
    std::optional<SensorOffset> offset;
};
```

`SensorOffset::create()` prueft zuerst `std::isfinite(celsius)` (sonst
`NonFinite`), danach den Betrag gegen
`[-kMaxAbsoluteOffsetCelsius, +kMaxAbsoluteOffsetCelsius]`
(`sensor_limits.hpp`, sonst `OutOfFirmwareRange`). `SensorOffset::create(0.0)`
ist eine gewoehnliche, immer erfolgreiche Konstruktion wie jeder andere
Wert innerhalb der Firmwaregrenze - KEIN impliziter Default fuer "kein
Offset gesetzt" (KORREKTUR RUNDE 5: die vorherige Formulierung "Ein
fehlender Offset bleibt SensorOffset::create(0.0)" suggerierte, eine FEHLENDE
Kalibrierung wuerde automatisch zu einer echten `SensorOffset(0.0)`-Instanz
werden - das widerspricht der in Abschnitt 13b eingefuehrten Unterscheidung
zwischen "keine Kalibrierung" (`std::nullopt` auf SensorCalibration-Ebene)
und "explizite Kalibrierung mit Offset 0.0" (`SensorCalibration(identity,
SensorOffset::create(0.0))`). Ein SensorOffset-Wert entsteht ausschliesslich
durch einen expliziten `create()`-Aufruf mit einem konkreten Kalibrierwert;
er wird nie implizit fuer eine fehlende Kalibrierung erzeugt).

`sensor_offset.hpp` und `sensor_identity.hpp` haengen bewusst NICHT auf
`storage_types.hpp`, sondern `sensor_offset.hpp` nur auf `sensor_limits.hpp`
(dieselbe fachliche Domaene). Begruendung unveraendert aus Runde 1: Ein
Leser von `sensor_identity.hpp`/`sensor_offset.hpp` muesste sich sonst
fragen, warum ein Sensorwerttyp vom Speichermodul abhaengt, obwohl beide
Domaenen nichts miteinander zu tun haben (AGENTS.md-DRY-Ausnahme fuer
oberflaechlich aehnlichen, fachlich fremden Code).

`NONFINITE_CONFIG_REJECTED: PASS` (gilt fuer SensorQualityConfig UND
SensorOffset, siehe Abschnitt 10.0/13a)

### 13b. SensorCalibration: bindet SensorOffset an eine SensorIdentity (Korrektur Runde 4)

`SensorOffset` allein (13a) erfuellt den im urspruenglichen Auftrag
geforderten Scope "individueller Offset je ROM-Adresse" NICHT: Ein reiner
`double celsius_`-Offset ohne Identitaetsbezug kann nicht unterscheiden,
FUER WELCHEN physischen Sensor er gilt - bei einem ROM-Wechsel (Abschnitt
8/9b/11) wuerde ein zuvor gesetzter Offset unveraendert auf einen komplett
anderen physischen Sensor weiterwirken, ohne dass die Pipeline dies erkennen
koennte. Deshalb ein kleiner, eigenstaendiger Kalibriervertrag, der beide
Werte verbindlich zusammen fuehrt:

```cpp
// sensor_calibration.hpp
// Bindet einen bereits gueltigen SensorOffset an eine bereits gueltige
// SensorIdentity. Beide Bestandteile sind schon einzeln gueltig-by-
// construction (Abschnitt 9.0/13a); die Kombination zweier bereits
// gueltiger Werte kann selbst nicht ungueltig werden - deshalb bewusst KEIN
// eigenes CreateResult/create() nach dem StateStoreKey-Muster (das waere
// Overhead ohne fachlichen Grund, KISS).
class SensorCalibration {
   public:
    SensorCalibration(SensorIdentity identity, SensorOffset offset)
        : identity_(identity), offset_(offset) {}
    [[nodiscard]] SensorIdentity identity() const { return identity_; }
    [[nodiscard]] SensorOffset offset() const { return offset_; }
   private:
    SensorIdentity identity_;
    SensorOffset offset_;
};
```

Uebergabe an die Pipeline: `SensorQualityPipeline::setCalibration(
std::optional<SensorCalibration> calibration)`. #20 selbst beschafft,
persistiert oder validiert keine Kalibrierdaten (Nicht-Scope, siehe
Abschnitt 7) - der Aufrufer (spaeter #21/#24 bzw. die Composition Root)
liest die passende Kalibrierung aus ihrer eigenen Quelle (z. B. NVS) und
setzt sie explizit. Ein Aufruf ersetzt eine vorherige Kalibrierung
vollstaendig; `nullopt` bedeutet "keine Kalibrierung vorhanden" (z. B. vor
der ersten Zuweisung durch die Composition Root).

Verhalten bei fehlender oder nicht passender Kalibrierung (Abschnitt 10.4
verwendet dies bei der Offset-Anwendung):

```text
calibration hat einen Wert UND calibration->identity() == identity() der
  aktuellen akzeptierten Probe (sofern deren identity() bekannt ist) ->
  calibration->offset().celsius() wird auf den Medianfilter-Ausgang
  angewendet UND appliedOffset (Abschnitt 12) wird auf diesen Wert gesetzt
  (has_value() == true, auch wenn der Wert zufaellig 0.0 ist).

calibration ist nullopt, ODER die aktuelle Probe hat identity() == nullopt,
  ODER calibration->identity() != identity() der aktuellen Probe -> INTERN
  wird 0.0 (neutral) auf den Medianfilter-Ausgang angewendet (der Regelpfad/
  correctedCelsius wird NICHT blockiert), ABER appliedOffset wird auf
  nullopt gesetzt. KEIN Fehlerzustand, KEINE Plausibilitaetsverletzung,
  KEINE eigene SensorFaultReason: eine fehlende passende Kalibrierung ist
  kein Sensorfehler.

KORREKTUR RUNDE 4 (behebt einen im vorherigen Entwurf dieser Regel selbst
eingefuehrten Fehler): `appliedOffset` als reines `double` haette
"kalibriert auf 0.0" nicht von "keine passende Kalibrierung, neutraler
Fallback" unterscheiden koennen - beide Faelle haetten denselben Snapshot
erzeugt, obwohl der zweite Fall bedeutet, dass der Regelwert um bis zu
kMaxAbsoluteOffsetCelsius falsch sein kann, waehrend keine Diagnoseevidenz
dies zeigt (genau der 0-Sentinel-Fehler, den Runde 3 fuer SensorIdentity
bereits vermieden hat). appliedOffset ist deshalb std::optional<double>
(Abschnitt 12): nullopt macht "keine passende Kalibrierung angewendet" fuer
JEDEN Konsumenten direkt und ohne zusaetzliches Flag sichtbar. OB eine
fehlende Kalibrierung selbst ein Problem darstellt (z. B. verriegelungs-
wuerdig), ist eine Bewertungsfrage, die wie jede rollenuebergreifende
Bewertung ausserhalb von #20 liegt (Abschnitt 11) - #20 liefert nur die
Evidenz.
```

`ROM_BOUND_CALIBRATION_CONTRACT: PASS`
`CALIBRATION_MISMATCH_BEHAVIOR_DEFINED: PASS`

## 14. Geplante Dateien mit Begruendung

```text
lib/device_platform/src/temperature_source.hpp  [BESTEHENDE DATEI, ERWEITERT]
  TemperatureSampleStatus + TemperatureReading (gueltig-by-construction,
  Abschnitt 9.1) ersetzen die bisherige {bool available; double celsius;}
  -Form. ITemperatureSource-Interface bleibt in Signatur unveraendert
  (gleiche read()-Methode, reichhaltigerer Rueckgabewert). Einziger
  betroffener Blastradius: siehe Abschnitt 4 (repository-weit verifiziert,
  nur 3 Dateien).

lib/device_platform/src/sensor_identity.hpp
  Eigenstaendiger, gueltig-by-construction Sensoridentitaetstyp (Abschnitt
  9.0, KEINE Abhaengigkeit auf storage_types.hpp, kein 0-Sentinel); noetig
  fuer ROM-Wechsel-Erkennung (Abschnitt 8/11).

lib/device_platform/src/sensor_quality.hpp
  SensorQuality-Enum (Abschnitt 8).

lib/device_platform/src/sensor_quality_config.hpp
  SensorQualityConfig, gueltig-by-construction, NaN/Inf-sicher
  (Abschnitt 10.0).

lib/device_platform/src/sensor_quality_snapshot.hpp
  SensorFaultReason + SensorQualitySnapshot; Ausgabevertrag (Abschnitt 12).

lib/device_platform/src/sensor_offset.hpp
  SensorOffset-Werttyp, gueltig-by-construction, NaN/Inf-sicher
  (Abschnitt 13a).

lib/device_platform/src/sensor_calibration.hpp
  SensorCalibration: bindet SensorOffset an eine SensorIdentity (Abschnitt
  13b, Korrektur Runde 4). Kein eigenes CreateResult (Kombination bereits
  gueltiger Werte).

lib/device_platform/src/sensor_median_filter.hpp/.cpp
  Medianfilter mit fester Kapazitaet (Abschnitt 10.3), eigene SRP-Einheit,
  eigenstaendig testbar (rohe double-Folgen, kein TemperatureReading noetig).

lib/device_platform/src/sensor_lowpass_filter.hpp/.cpp
  Tiefpassfilter mit Zeitkonstante (Abschnitt 10.5), eigene SRP-Einheit.

lib/device_platform/src/sensor_quality_pipeline.hpp/.cpp
  SampleDisposition-Enum + Orchestrator: verkettet 9b/10.1-10.5, fuehrt
  Zustandsmaschine (Abschnitt 8, quality als deriveQuality()-Ableitung,
  Abschnitt 9a) und erzeugt SensorQualitySnapshot. Nimmt SensorQualityConfig
  im Konstruktor entgegen; haelt selbst KEINE ITimeSource-Referenz -
  ingest(sample, now) -> SampleDisposition, snapshot(now) ->
  SensorQualitySnapshot UND setCalibration(optional<SensorCalibration>)
  (Abschnitt 13b, Korrektur Runde 4) sind die einzigen oeffentlichen
  Methoden.

lib/device_platform/src/sensor_limits.hpp
  Firmwarefeste Obergrenzen (max. Medianfenster, absoluter
  Temperaturbereich, max. Offsetbetrag, max. STALE-Alter-Obergrenze, max.
  Wiedererkennungs-Probenzahl-Obergrenze, max.
  Wiedererkennungs-Stabilitaetszeit-Obergrenze
  `kMaxRecoveryStabilityDurationCeilingMs` - Korrektur Runde 4, Abschnitt
  10.0) nach dem Muster von
  storage_slot_limits.hpp. Enthaelt bewusst KEINE sensor-/
  treiberspezifischen Zahlenkonstanten. Konkrete, am realen Schrank
  ermittelte Werte bleiben TBD_COMMISSIONING (Abschnitt 18).

lib/device_platform_test_support/src/mock_temperature_source.hpp/.cpp  [BESTEHENDE DATEIEN, ANGEPASST]
  API auf den erweiterten Port nachgezogen: setCelsius()/setAvailable()
  werden durch setReading(identity, timestampMs, celsius) (Ok-Fall) und
  setFault(identity, timestampMs, status) (Fehlerfall) ersetzt - beide
  konstruieren intern eine gueltige TemperatureReading ueber deren create().
  Konstruktor entsprechend erweitert. Reine Testhilfe, unveraendert nicht
  von fermentation_app/src/main.cpp/main/app_main.cpp eingebunden.

test/test_sensor_actuator_mocks/test_sensor_actuator_mocks.cpp  [BESTEHENDE DATEI, TEILWEISE ANGEPASST]
  Ausschliesslich die drei temperaturbezogenen Testfunktionen
  (test_temperature_source_reports_configured_value,
  test_temperature_source_can_change_value,
  test_temperature_source_fault_injection_marks_unavailable) werden auf die
  neue Mock-API/den neuen Snapshot-Zugriff (status()/celsius()) nachgezogen.
  Alle Aktuator-Mock-Testfunktionen dieser Datei bleiben unveraendert.

test/
  siehe Abschnitt 17a fuer die vollstaendige, nach Topics aufgeteilte Liste.

docs/tasks/issue-20-sensor-quality-filtering-plan.md
  Dieser Plan (bereits Teil des Plan-PR-Diffs).
```

`EXPECTED_FILE_DIFF_DEFINED: PASS`
`PARALLEL_INPUT_CONTRACTS: 0`

## 15. SOLID-/DRY-/KISS-Bewertung

```text
S (Single Responsibility):
  Getrennte Klassen (MedianFilter, LowPassFilter, die Zustandsmaschine/
  Plausibilitaetslogik innerhalb der Pipeline, der reine Diagnose-Snapshot-
  Typ, TemperatureReading als reiner Werttyp) statt einer Monsterklasse.
  Jede Klasse ist unabhaengig mit synthetischen Werten testbar.

O (Open/Closed):
  Neue Sensorrollen entstehen ausschliesslich durch eine neue
  SensorQualityConfig-Instanz (unterschiedliche tau/Fensterwerte), NICHT
  durch neue Codepfade oder if/switch auf eine Rolle.

L (Liskov Substitution):
  Jede Implementierung von ITemperatureSource (MockTemperatureSource JETZT,
  der reale DS18B20-Adapter aus #30 SPAETER) muss ausschliesslich ueber
  TemperatureReading::create() gueltige Werte erzeugen und denselben Vertrag
  (Abschnitt 9.1) einhalten - austauschbar ohne Vertragsverletzung, weil es
  nur EINEN Vertrag gibt (kein paralleler Typ, den #30 zusaetzlich haette
  einhalten muessen).

I (Interface Segregation):
  TemperatureReading und SensorQualitySnapshot sind reine Werttypen ohne
  virtuelle Schnittstelle; SensorQualityPipeline hat genau drei
  oeffentliche Methoden - eine Verarbeitungs- (ingest), eine Abfrage-
  (snapshot) und eine schmale Konfigurationsmethode (setCalibration,
  Abschnitt 13b, Korrektur Runde 4) - keine davon ueberfluessig oder mit
  Verantwortung einer anderen ueberlappend. ITemperatureSource bleibt mit
  genau einer Methode (read()) minimal.

D (Dependency Inversion):
  Die Pipeline haengt auf KEINE Zeitabstraktion ab - weder ITimeSource noch
  eine andere Uhr-Schnittstelle; Zeit fliesst ausschliesslich als expliziter
  Wert (Abschnitt 9a). Sie haengt nie von DS18B20-, Arduino- oder
  ESP-IDF-Typen ab. Ein spaeterer DS18B20-Adapter haengt in Richtung
  device_platform (ueber ITemperatureSource), nicht umgekehrt.

DRY:
  Eine einzige parametrisierte SensorQualityPipeline-Implementierung fuer
  alle drei Rollen; keine kopierten Filter-/Zustandsmaschinen-
  Implementierungen pro Rolle. Firmwarefeste Grenzen liegen zentral in
  sensor_limits.hpp. Genau EIN kanonischer Eingangstyp
  (TemperatureReading) statt einer parallelen Architektur.

KISS:
  Deterministischer, linearer Datenfluss ohne generische Rules-Engine, ohne
  Plugin-/Registrierungsmechanismus, ohne externe Filterbibliothek. Genau
  drei oeffentliche Qualitaetszustaende statt vier. Testhilfe fuer
  geskriptete Fehlerfolgen bleibt lokal im einzigen tatsaechlichen
  Konsumenten (Abschnitt 17a), nicht als vorzeitig herausgezogenes Modul.

Modulzuordnung (Entscheidung C):
  device_platform statt fermentation_app, durch ADR-013 (Abschnitt "Regeln
  fuer neue Module") sowie lib/device_platform/AGENTS.md wortwoertlich
  gedeckt (siehe Abschnitt 13/19).
```

`SOLID_REVIEW: PASS`
`DRY_REVIEW: PASS`
`KISS_REVIEW: PASS`
`TEST_HELPER_KISS: PASS`

## 16. Ressourcen-, Laufzeit- und CI-Nachweise der spaeteren Umsetzung

```text
Speicher pro Pipeline-Instanz (Schaetzung, spaeter im Implementierungs-PR
  per scripts/build_report.py nachzuweisen):
  - TemperatureReading: ~40-48 Byte (optional<SensorIdentity>,
    optional<double>, uint64_t Zeitstempel, Statusbyte)
  - Medianpuffer: kMaxMedianWindowSize * sizeof(double), bei einer
    erwarteten Obergrenze von z. B. 9 Werten ~72 Byte
  - SensorQualitySnapshot: ~130-160 Byte (mehrere optional<double>/
    optional<uint64_t>-Felder)
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
  Stufen O(1). Kein Sensorzyklus schreibt in Flash.

Verhalten bei Bursts/zu schnellen Proben:
  Wird ausschliesslich ueber den Zeitstempelvertrag (Abschnitt 9b) geregelt,
  keine zusaetzliche Ratenbegrenzung noetig; ein zu kurzer dt fuehrt
  hoechstens zu einer grossen Aenderungsrate (10.2), nicht zu einem
  Pufferueberlauf, da alle Puffer fest dimensioniert sind.

statisches RAM/Flash:
  std::array statt std::vector fuer den Medianpuffer; keine dynamische
  Allokation im Verarbeitungspfad. Flash-Wirkung durch die 13 neuen kleinen
  Header/Source-Dateien aus Abschnitt 14 (Korrektur Runde 4: Zaehlung
  berichtigt und um sensor_calibration.hpp ergaenzt) plus die Erweiterung
  einer bestehenden Datei wird als gering eingeschaetzt, aber erst durch
  scripts/build_report.py belastbar.

Auswirkung auf native Tests und beide ESP32-Profile:
  Reiner device_platform-Code, im native-Profil ohne Aenderung testbar.
  Kein neuer Code in main/app_main.cpp oder src/main.cpp -> keine
  Auswirkung auf esp32_bringup/esp32_release ausser dem zusaetzlichen
  (unverdrahteten) Uebersetzungsergebnis der neuen/erweiterten Dateien.

Base-/Head-Ressourcenvergleich:
  Im Implementierungs-PR mit
  "python scripts/build_report.py --output build-report.md" (native) sowie
  nach ESP-IDF-Export mit den Profilen bringup/release durchzufuehren, wie
  in docs/CI_AND_QUALITY_GATES.md beschrieben. Verbindliche Byte-Budgets
  bleiben TBD_IMPLEMENTATION_BUDGET bis zur realen Messung.
```

`RESOURCE_PLAN_DEFINED: PASS`

## 17. Vollstaendige Testmatrix

Jede Gruppe nennt Testsuite (siehe Abschnitt 17a fuer die Aufteilung) und
fachliches Orakel.

```text
Start/Normalbetrieb (Orakel: Abschnitt 8 Zustandsmaschine, SENSOR_TUNING_
COMMISSIONING.md "Messzyklus"):
  - Start ohne Probe -> Stale, kein Regelwert, rawCelsius/correctedCelsius/
    filteredCelsius/lastAcceptedSampleAgeMs/lastValidSampleAgeMs/
    changeRateCelsiusPerSecond alle == std::nullopt (keine erfundenen
    Platzhalterwerte)
  - erste gueltige Probe -> weiterhin Stale (noch nicht genug Folgeproben),
    rawCelsius/lastAcceptedSampleAgeMs jetzt gesetzt; correctedCelsius UND
    filteredCelsius bleiben nullopt bis Slice 2 Medianfilter und Tiefpass
    liefert (danach: bereits ab dem ERSTEN plausiblen Beitrag gesetzt,
    nicht erst bei vollem Medianfenster, siehe Abschnitt 10.3/12)
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
  - KnownInvalidMeasurement-Sample (status direkt so geskriptet,
    unabhaengig vom konkreten Rohwert - #20 kennt keine sensor-/
    treiberspezifischen Zahlenkonstanten) -> ungueltig, Ursache
    KnownInvalidMeasurement
  - NaN/Inf als celsius() (durch die lokale Testhilfe konstruierbar) ->
    ungueltig, Ursache NonFinite (eigene Ursache, nicht OutOfRange)

Eingangsvertrag (Orakel: Abschnitt 9.0/9.1, neu in Runde 2):
  - TemperatureReading::create(): status=Ok ohne celsius -> abgelehnt
    (InconsistentValuePresence)
  - TemperatureReading::create(): status!=Ok mit celsius -> abgelehnt
  - TemperatureReading::create(): status=Ok mit celsius, identity=nullopt
    -> erfolgreich (Identitaet optional und unabhaengig)
  - SensorIdentity::create(0) -> abgelehnt (ZeroIsNotAValidIdentity)
  - SensorIdentity::create(<positiver Wert>) -> erfolgreich
  - SensorQualityConfig::create() mit minConsecutiveValidSamples == 0 ->
    abgelehnt (InvalidRecoveryThresholds)
  - SensorQualityConfig::create() mit minConsecutiveValidSamples == 1 ->
    abgelehnt (InvalidRecoveryThresholds, neu Runde 5 - Korrektur von `> 0`
    auf `>= 2`: "mehrere gueltige Proben" per
    SENSOR_TUNING_COMMISSIONING.md ist keine Einzelprobe)
  - SensorQualityConfig::create() mit minConsecutiveValidSamples == 2
    (Randwert) -> erfolgreich (neu Runde 5)
  - SensorQualityConfig::create() mit minRecoveryStabilityDurationMs == 0 ->
    abgelehnt (InvalidRecoveryThresholds, neu Runde 4 - vorher UNGEPRUEFT)
  - SensorQualityConfig::create() mit minRecoveryStabilityDurationMs >
    sensor_limits::kMaxRecoveryStabilityDurationCeilingMs -> abgelehnt
    (InvalidRecoveryThresholds, neu Runde 4)
  - SensorQualityConfig::create() mit minRecoveryStabilityDurationMs ==
    kMaxRecoveryStabilityDurationCeilingMs (Randwert) -> erfolgreich

Zeit und Alter (Orakel: Abschnitt 9, SAFETY_COMPONENT_FAULTS.md
Sensorzustandsfolge):
  - Stale durch Altersgrenze (kMaxStaleAgeMs) ueberschritten -> Failed
  - Failed durch maximale Fehlerdauer -> bleibt Failed bis Wiedererkennung
  - Zeitstempel < letztem akzeptierten Zeitstempel -> RejectedRetrograde,
    Zustand/Filter/lastAcceptedSampleAgeMs unveraendert
  - Zeitstempel > nowMonotonicMs (an ingest() uebergeben) -> RejectedFuture,
    gleiche Wirkungslosigkeit wie oben
  - identischer Zeitstempel + identischer Wert + identische identity() +
    identischer status() -> DuplicateIgnored, lastAcceptedSampleAgeMs
    unveraendert
  - identischer Zeitstempel + unterschiedlicher Wert -> RejectedTimestampConflict
    (nicht akzeptiert), lastAcceptedSampleAgeMs unveraendert; Test belegt,
    dass danach weiterhin nur der vorherige Wert als "letzter akzeptierter"
    gilt
  - identischer Zeitstempel + identischer Wert + identischer status(), aber
    ABWEICHENDE identity() -> RejectedTimestampConflict, NICHT
    DuplicateIgnored (neu Runde 4 - Identitaet ist jetzt Teil des
    Duplikatvergleichs, siehe Abschnitt 9b)
  - identischer Zeitstempel + BEIDE Proben celsius() == NaN (unterschiedliche
    NaN-Bitmuster erlaubt), identische identity()/status() ->
    DuplicateIgnored (neu Runde 4 - sampleValuesEqual() behandelt NaN==NaN
    explizit als gleich, nicht als IEEE-754-Ungleichheit)
  - identischer Zeitstempel, eine Probe celsius() == NaN, die andere ein
    endlicher Wert -> RejectedTimestampConflict (neu Runde 4 - keine
    zufaellige Gleichheit zwischen NaN und einem endlichen Wert)
  - ALLERERSTE Probe ueberhaupt (kein vorheriges Accepted), Zeitstempel <=
    nowMonotonicMs -> Accepted (neu Runde 4 - Duplikat-/Konflikt-/
    Retrograde-Pruefung ist auf die erste Probe nicht anwendbar, siehe
    Abschnitt 9b)
  - ALLERERSTE Probe ueberhaupt MIT Zeitstempel > nowMonotonicMs ->
    RejectedFuture (neu Runde 4 - Zukunftspruefung gilt auch ohne
    Vorgaenger)
  - keine der vier Ablehnungsdispositionen erhoeht consecutiveInvalidCount
    ODER aktualisiert lastAcceptedSampleAgeMs (je Disposition ein
    eigener Testfall)
  - grosse Messluecke -> Uebergang wie in Abschnitt 8 beschrieben
  - Wiederaufnahme nach Luecke: erste Probe nach Luecke wird nicht als
    "Sprung" gegen den (veralteten) Vorwert bewertet
  - geordnete, aber gegenueber nowMonotonicMs stark verspaetet zugestellte
    Probe (neu Runde 5, behebt Abschnitt 9a): letzte akzeptierte Probe bei
    t=0, kMaxStaleAgeMs klein, naechste Probe erst bei
    sample.monotonicTimestampMs() = t=1 (also chronologisch geordnet,
    RejectedRetrograde greift NICHT), aber ingest() wird erst bei
    nowMonotonicMs weit jenseits von kMaxStaleAgeMs nach t=0 aufgerufen ->
    ingest() erkennt den VOR dieser Probe bereits real erreichten
    FAILED-Zustand (deriveQuality(nowMonotonicMs), NICHT
    deriveQuality(sample.monotonicTimestampMs())) und loest den
    Filterreset/die Wiedererkennungslogik wie bei einer regulaeren
    FAILED->Valid-Erkennung aus, statt die Probe faelschlich als blosse
    Fortsetzung eines noch-Stale-Zustands zu behandeln
  - ingest() gibt in jedem der obigen Faelle die jeweils korrekte
    SampleDisposition zurueck (expliziter Rueckgabewert, kein void)

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
    sichtbar (kein dauerhaftes Verstecken eines echten Trends)
  - Aenderungsrate mit kurzem vs. langem Zeitabstand ergibt unterschiedliche
    Bewertung desselben absoluten Delta
  - dauerhaft nahezu unveraenderte "Produkt"-Konfiguration (dt gross, sehr
    kleines Delta ueber lange Zeit) bleibt erlaubt/Valid

Median und Tiefpass (Orakel: Abschnitt 10.3/10.5):
  - ungefuelltes Medianfenster liefert Median aus den vorhandenen Werten;
    bereits nach dem ERSTEN gueltigen Beitrag ein echter Wert (kein
    "wartet auf volles Fenster")
  - ungerade/gerade Fensterentscheidung: nur ungerade Fenstergroessen sind
    gueltige Konfiguration (Abschnitt 10.3); Test prueft die Ablehnung
  - Einzelspitze wird entfernt (isoliert fuer MedianFilter getestet)
  - echter Trend bleibt ueber mehrere Zyklen im gefilterten Wert sichtbar
  - ungueltige Probe gelangt nachweislich NICHT ins Medianfenster
  - Reset des Filterzustands nach Failed->Valid-Wiedererkennung und nach
    ROM-Wechsel (kein "Nachschleppen" alter Werte)
  - Filterzustand bleibt VOLLSTAENDIG erhalten bei Valid -> Stale (einzelne
    ungueltige Probe) -> Valid (Wiedererkennung), OHNE dass Failed
    zwischenzeitlich erreicht wurde: derselbe Median-/Tiefpasszustand wird
    fortgesetzt, kein Neustart der Einschwingphase (neu Runde 4, behebt den
    vorherigen Widerspruch aus Abschnitt 8 - Test unterscheidet diesen Fall
    explizit vom direkt darueberliegenden Failed/ROM-Wechsel-Resettest)
  - unterschiedliche tau-Werte zweier Konfigurationen ergeben messbar
    unterschiedliche Einschwingzeit bei identischer Sprungeingabe
  - extremer Rohwert bleibt in rawCelsius sichtbar, auch wenn
    filteredCelsius (noch) nicht nachgezogen ist
  - dtSeconds fuer den Tiefpass stammt nachweislich aus den
    Probenzeitstempeln (nicht aus nowMonotonicMs): zwei ingest()-Aufrufe
    mit identischem nowMonotonicMs, aber unterschiedlichen
    Probenzeitstempeln, ergeben unterschiedliche Filterreaktionen
  - dtSeconds bei Valid -> kurz Stale (eine akzeptierte, aber UNPLAUSIBLE
    Probe, Filter bleibt erhalten) -> Valid wird ab der letzten Probe
    berechnet, die TATSAECHLICH in den Filter eingeflossen ist, NICHT ab
    der dazwischenliegenden unplausiblen akzeptierten Probe (neu Runde 4,
    behebt einen durch die Filtererhaltungs-Korrektur in Abschnitt 8 neu
    entstandenen Fehler in Abschnitt 10.5 - Test belegt den korrekten,
    groesseren dt-Wert statt einer Unterschaetzung)

Kalibrierung, Offset und Identitaet (Orakel: Abschnitt 10.4, 10.5, 13a,
13b, 8, 11 - Gruppe seit Runde 4 identitaetsgebunden, seit Runde 5 inkl.
Tiefpass-Verschiebung bei Offsetwechsel):
  - Offset 0.0 -> correctedCelsius == Medianfilter-Ausgang unveraendert
  - positiver und negativer Offset veraendern correctedCelsius korrekt
  - Offset am Rand von kMaxAbsoluteOffsetCelsius wird noch akzeptiert
  - Offset NaN/Inf -> SensorOffset::create() liefert NonFinite
  - Offset ausserhalb der Firmwaregrenze -> OutOfFirmwareRange
  - SensorCalibration(identity, offset).identity()/.offset() liefern die
    uebergebenen Werte unveraendert zurueck (neu Runde 4)
  - setCalibration(calibration) mit calibration->identity() ==
    identity() der aktuellen Probe -> appliedOffset.has_value() == true UND
    appliedOffset == calibration->offset().celsius() (neu Runde 4)
  - setCalibration(calibration) mit calibration->offset().celsius() == 0.0
    UND passender identity() -> appliedOffset.has_value() == true UND
    appliedOffset == 0.0 (neu Runde 4 - UNTERSCHEIDUNGSFALL: dieselbe
    correctedCelsius-Wirkung wie der naechste Testfall unten, aber
    appliedOffset macht sichtbar, DASS eine passende Kalibrierung
    tatsaechlich angewendet wurde, statt nur zufaellig neutral zu sein)
  - setCalibration(nullopt) (keine Kalibrierung gesetzt) -> appliedOffset ==
    std::nullopt (Korrektur Runde 4 - appliedOffset ist std::optional<double>
    genau damit dieser Fall vom vorherigen Testfall unterscheidbar bleibt,
    siehe Abschnitt 12/13b)
  - setCalibration(calibration) mit calibration->identity() !=
    identity() der aktuellen Probe -> appliedOffset == std::nullopt (KEINE
    SensorFaultReason, KEINE Plausibilitaetsverletzung, neu Runde 4)
  - Probe mit identity() == nullopt UND gesetzter Kalibrierung ->
    appliedOffset == std::nullopt (keine Identitaet zum Abgleich vorhanden,
    neu Runde 4)
  - in JEDEM appliedOffset == std::nullopt-Testfall bleibt correctedCelsius
    dennoch gesetzt (Medianfilter-Ausgang + intern 0.0, Regelpfad nicht
    blockiert, neu Runde 4)
  - setCalibration() waehrend laufender Filterung wirkt erst auf die
    naechste eingehende Probe, nicht rueckwirkend (neu Runde 4)
  - identity()-Wechsel zwischen zwei bekannten Werten -> IdentityMismatch,
    Filterzustand verworfen, Wiedererkennung wie nach Failed erforderlich
  - identity()-Uebergang von/zu nullopt -> KEIN IdentityMismatch (fehlende
    Evidenz, siehe Abschnitt 9b)
  - Rollenwechsel ist kein #20-Konzept (Abschnitt 13) und daher NICHT Teil
    dieser Testgruppe; stattdessen zeigt eine zweite unabhaengige
    Pipeline-Instanz, dass sich zwei Instanzen nicht gegenseitig
    beeinflussen
  - gleiche Sensoridentitaet, effektiver Offset wechselt zwischen zwei
    tatsaechlich in den Filter einfliessenden Proben (z. B. von 0.0
    neutral auf einen gesetzten Kalibrierwert) -> gefiltert_alt wird exakt
    um delta verschoben, filteredCelsius springt NICHT auf den neuen
    korrigierten Rohwert und schwingt NICHT langsam ein (neu Runde 5)
  - gleiche Sensoridentitaet, setCalibration(nullopt) -> explizite
    SensorCalibration(identity, SensorOffset::create(0.0)) (delta == 0,
    beide effektiv 0.0) -> KEINE Verschiebung, filteredCelsius unveraendert,
    OBWOHL appliedOffset im Snapshot von nullopt auf has_value(0.0) wechselt
    (neu Runde 5 - Unterscheidungsfall zwischen interner Rechengroesse und
    Snapshot-Evidenz, siehe Abschnitt 10.4)
  - ROM-Wechsel UEBERSTIMMT die Verschiebungsregel: bei erkanntem
    Identitaetswechsel voller Filterreset wie in Abschnitt 8 (KEINE
    Verschiebung, auch wenn sich der effektive Offset gleichzeitig aendert)
  - nach einem ROM-Wechsel-Reset wird `lastFilterEffectiveOffsetCelsius`
    GEMEINSAM mit gefiltert_alt verworfen (Abschnitt 10.4): die erste
    plausible Probe des neuen Sensors nach dem Reset berechnet KEIN delta
    gegenueber dem vor dem Reset zuletzt verwendeten effektiven Offset des
    ALTEN Sensors (neu Runde 5 - belegt die zweite Haelfte der
    Reihenfolge-/Vorrangregel, nicht nur, dass keine Verschiebung
    stattfindet, sondern dass kein stale delta ueber den Reset hinweg
    ueberlebt)
  - erste den Filter ueberhaupt initialisierende Probe: keine Verschiebung
    (gefiltert_alt existiert noch nicht), unabhaengig vom effektiven Offset

Zustandsmaschine und Wiedererkennung (Orakel: Abschnitt 8):
  - Valid -> Stale bei einzelner ungueltiger Probe
  - Stale -> Valid nach kMinConsecutiveValidSamples UND
    kMinRecoveryStabilityDurationMs
  - Stale -> Valid (ohne dass Failed zwischenzeitlich erreicht wurde) setzt
    die Einschwingphase des Filters NICHT zurueck (neu Runde 4 - auf
    Zustandsmaschinenebene derselbe Nachweis wie der Filterinhaltstest in
    der Gruppe "Median und Tiefpass", hier zusaetzlich am
    SensorQuality-Uebergang selbst verifiziert, siehe Abschnitt 8)
  - Stale -> Failed bei Altersueberschreitung
  - Failed -> Wiedererkennung -> Valid (gleiche Bedingung wie Stale->Valid)
  - erneute ungueltige Probe waehrend Wiedererkennung setzt Fortschritt
    zurueck
  - abgeleitete Regelwertfreigabe (quality == Valid UND
    filteredCelsius.has_value()) ist in Stale/Failed in jedem Testfall
    dieser Gruppe explizit false
  - letzter gueltiger Wert bleibt im Snapshot sichtbar, auch waehrend
    Stale/Failed
  - EIN snapshot(now)-Aufruf mit weit in der Zukunft liegendem now, OHNE
    zwischenzeitlichen ingest()-Aufruf, erkennt einen zuvor Valid/Stale
    gemeldeten Sensor korrekt als Failed (Nachweis fuer die
    snapshot(now)-Ableitung aus Abschnitt 9a, nicht auf ingest() angewiesen)

Widersprueche und Scopegrenzen (Orakel: Abschnitt 11, 7):
  - #20 trifft bei zwei unabhaengigen Pipeline-Instanzen mit stark
    abweichenden Werten KEINE Schuldzuweisung - Test prueft, dass keine
    Pipeline-Instanz allein durch den Wert der anderen beeinflusst wird
  - Test/Kommentar dokumentiert explizit, dass #20 keinen Ersatzsensor
    waehlt, keine Verriegelung setzt, keinen Aktorbefehl erzeugt und KEINE
    Verdachtsmarkierung fuer rollenuebergreifende Widersprueche liefert
    (durch Abwesenheit entsprechender oeffentlicher API nachgewiesen)

Robustheit (Orakel: allgemein, AGENTS.md Ressourcenregeln):
  - Parametergrenzen werden bei Konfiguration geprueft
  - Zaehlerueberlauf: consecutiveInvalidCount/recoveryProgressCount als
    uint16_t mit Saettigung statt Wraparound bewiesen
  - Zeitdifferenzueberlauf: monotonicTimestampMs nahe UINT64_MAX bleibt
    korrekt/saturiert
  - deterministische Wiederholung derselben Eingabefolge liefert exakt
    denselben Snapshot-Verlauf
  - keine Abhaengigkeit von realer Uhrzeit oder Hardware (nur
    VirtualTimeSource im Test, nie in der Pipeline selbst)
  - keine deaktivierten/uebersprungenen Tests (Unity ohne TEST_IGNORE)
```

`TEST_MATRIX_COMPLETE: PASS`
`DELAYED_ORDERED_SAMPLE_TEST_DEFINED: PASS`

### 17a. Testdatei-Aufteilung (KISS, keine Monsterdatei, keine verfrueht ausgelagerte Testhilfe)

Testaufteilung folgt derselben SRP-Trennung wie der Produktionscode - ein
Testtopic pro eigenstaendig testbarer Komponente:

```text
test/test_temperature_source/test_temperature_source.cpp
  TemperatureReading::create() (Abschnitt 9.1): Ok<=>Wert-Konsistenz,
  optionale Identitaet. Neues Topic (Runde 2), da der Port jetzt eine
  eigene Gueltigkeitspruefung besitzt.

test/test_sensor_identity/test_sensor_identity.cpp
  SensorIdentity::create(): Ablehnung von 0, Erfolg fuer positive Werte,
  Gleichheit, keine Fremdtypvermischung.

test/test_sensor_offset/test_sensor_offset.cpp
  SensorOffset::create(): Erfolg innerhalb der Grenze, NaN/Inf-Ablehnung,
  Ablehnung ausserhalb der Firmwaregrenze, Randwerte. Zusaetzlich (Korrektur
  Runde 4): SensorCalibration-Konstruktion (identity()/offset() liefern die
  uebergebenen Werte unveraendert zurueck) - KEIN eigenes Testtopic fuer
  SensorCalibration, da der Typ selbst kein create()/keine eigene
  Validierung besitzt (reine Bindung zweier bereits gueltiger Werte, siehe
  Abschnitt 13b) und damit kein eigenstaendig zu testendes Verhalten ausser
  den Beziehungen hat, die ohnehin schon hier bzw. integrativ in
  test_sensor_quality_pipeline.cpp (Kalibrierungs-Anwendung) geprueft
  werden - ein eigenes fast-leeres Testtopic waere ein KISS-Verstoss
  (dieselbe Abwaegung wie bei der lokalen sensor_fault_sequence-Testhilfe,
  siehe test_sensor_quality_pipeline.cpp unten in diesem Abschnitt).

test/test_sensor_quality_config/test_sensor_quality_config.cpp
  SensorQualityConfig::create(): NaN/Inf-Ablehnung, jede InvalidXxx-
  Ablehnung einzeln, Erfolgsfall.

test/test_sensor_median_filter/test_sensor_median_filter.cpp
  MedianFilter isoliert mit rohen double-Folgen: ungefuelltes Fenster,
  Einzelspitze, Trend, gerade/ungerade Konfiguration.

test/test_sensor_lowpass_filter/test_sensor_lowpass_filter.cpp
  LowPassFilter isoliert mit rohen double-Folgen + dt-Werten: Einschwingen,
  unterschiedliche Tau-Werte, Messluecken.

test/test_sensor_quality_pipeline/test_sensor_quality_pipeline.cpp
  SensorQualityPipeline als Orchestrator: Start/Normalbetrieb, Transport-/
  Messfehler, Zeit/Alter/Disposition, Wertebereich/Dynamik, Median-/
  Tiefpass-INTEGRATION, Offset-Integration, Zustandsmaschine/
  Wiedererkennung, Widersprueche/Scopegrenzen, Robustheit. Enthaelt eine
  LOKALE (nur in dieser Datei definierte, nicht exportierte) kleine
  Testhilfsfunktion/-tabelle fuer geskriptete TemperatureReading-Folgen
  (ersetzt das in der Vorversion als eigenes device_platform_test_support-
  Modul geplante sensor_fault_sequence.hpp/.cpp - KORREKTUR RUNDE 2: solange
  nur dieses eine Testtopic eine solche Folge braucht, ist ein eigenes,
  produktionsnahes Testhilfsmodul verfrueht; die Datei kann bei einem
  zweiten realen Konsumenten spaeter als device_platform_test_support-
  Modul herausgezogen werden, KISS/YAGNI).

test/test_sensor_actuator_mocks/test_sensor_actuator_mocks.cpp  [BESTEHEND, TEILWEISE ANGEPASST]
  Nur die drei temperaturbezogenen Testfunktionen werden auf die neue
  Mock-API (setReading/setFault, status()/celsius()) nachgezogen.
```

`TEST_STRUCTURE_KISS: PASS`
`TEST_HELPER_KISS: PASS`

## 18. Risiken und Gegenmassnahmen

```text
Risiko: Die Erweiterung von ITemperatureSource/TemperatureReading trifft
  einen heute unbekannten Konsumenten, der von der alten {available,
  celsius}-Form abhaengt.
  Gegenmassnahme: repository-weit verifiziert (Abschnitt 4) - genau drei
  Dateien verwenden den Port (der Header selbst, der Mock, ein Testfile);
  keine Verwendung in src/main.cpp, main/app_main.cpp oder
  lib/fermentation_app/. Alle drei betroffenen Dateien sind explizit im
  Plan-Diff (Abschnitt 14) aufgefuehrt.

Risiko: Firmwarefeste Grenzen in sensor_limits.hpp werden ohne reale
  Messung zu eng oder zu weit gewaehlt.
  Gegenmassnahme: Grenzen bleiben bewusst konservativ/breit (reine
  Sicherheitsaussenkante), waehrend die eigentliche Abstimmung
  TBD_COMMISSIONING bleibt und ueber SensorQualityConfig (nicht durch
  Codeaenderung) erfolgt.

Risiko: Der Umfang (Zustandsmaschine + Plausibilitaet + 2 Filter +
  Diagnosevertrag + vollstaendige Testmatrix + Porterweiterung) ist fuer
  eine einzelne Reviewrunde zu gross.
  Gegenmassnahme: entschiedene Teilung in zwei interne Reviewslices
  innerhalb desselben Draft-PR (Entscheidung A, Abschnitt 19/20); die
  Porterweiterung (Abschnitt 9) gehoert zu Slice 1, da Slice 1 der erste
  tatsaechliche Konsument des neuen Vertrags ist.

Risiko: Eine spaetere ADR-Notwendigkeit wird uebersehen.
  Gegenmassnahme: bereits durch Entscheidung B (Abschnitt 19) explizit
  gepruft und verneint; eine tatsaechlich neue Grundsatzfrage waehrend der
  Implementierung waere eine materielle Planabweichung (AGENTS.md) und
  erfordert einen neuen Plan-Commit samt erneuter Freigabe.
```

## 19. Bereits entschiedene Punkte

### Entscheidung A (entschieden): ein Issue, ein Branch, derselbe Draft-PR, zwei interne Reviewslices

```text
Entschieden: #20 wird in genau einem Issue, einem Branch und demselben
  Draft-PR (#95) umgesetzt. Zwei interne Reviewslices:
    1. Qualitaetszustand + Plausibilitaet (Porterweiterung Abschnitt 9,
       SensorIdentity, SampleDisposition, SensorQuality-Zustandsmaschine,
       Plausibilitaet 10.1/10.2, SensorQualityConfig, Diagnosevertrag -
       filteredCelsius/correctedCelsius bleiben hier std::nullopt, da Median
       und Tiefpass noch nicht existieren, KEIN Platzhalterverhalten).
    2. Median + Offset + Tiefpass + vollstaendige Integration.
  Kein zweiter PR, keine Uebergangs-/Platzhalter-API zwischen den Slices.
Begruendung: So festgelegt; deckt sich mit der unabhaengig
  uebereinstimmenden Empfehlung aus
  docs/audits/PROPOSED_RELEASE_1_ROADMAP.md ("#20: Status-/
  Plausibilitaetsmodell, danach Filterpipeline").
```

`DECISION_A_ONE_PR_TWO_SLICES: PASS`

### Entscheidung B (entschieden): keine neue ADR

```text
Entschieden: Die Umsetzung von #20 legt KEINE neue ADR in docs/DECISIONS.md
  an. #20 setzt bereits akzeptierte Architektur und Spezifikation um; es
  gibt keine neue, bisher unentschiedene Grundsatzfrage. Die Porterweiterung
  aus Runde 2 ist eine additive, ruecksichtsvolle Evolution eines bereits
  bestehenden, noch nicht produktiv verdrahteten Ports - keine neue
  Architekturentscheidung.
```

`DECISION_B_NO_NEW_ADR: PASS`

### Entscheidung C (entschieden): device_platform

```text
Entschieden: Die anwendungsneutrale Sensorqualitaets- und Filterpipeline aus
  #20 gehoert nach lib/device_platform/. Konkrete Rollen wie Schrankluft,
  Produkt und Kuehlkoerper bleiben ausserhalb der Plattform, in Anwendung
  beziehungsweise Composition Root.
Begruendung: ADR-013, Abschnitt "Regeln fuer neue Module", zaehlt
  "Sensorqualitaet, Filter, begrenzte Reglerbausteine" ausdruecklich zu den
  "Allgemeine[n] Bausteine[n]", die "in der Plattform liegen" duerfen;
  ADR-013 nennt "Sensorqualitaet" im Kontextabschnitt sogar explizit als
  Beispiel fuer eine ueber Fermentationsschrank UND eine zukuenftige
  Smokersteuerung hinweg gemeinsam benoetigte Grundfunktion.
  lib/device_platform/AGENTS.md deckt sich damit.
```

`DECISION_C_DEVICE_PLATFORM: PASS`
`OWNER_DECISIONS_OPEN: 0`

## 20. Genaue Implementierungsreihenfolge (nach Planfreigabe)

Zwei interne Reviewslices im selben Draft-PR #95, kein zweiter PR, keine
Uebergangs-/Platzhalter-API zwischen den Slices.

```text
Slice 1 (Qualitaetszustand + Plausibilitaet, inkl. Porterweiterung):
  1. sensor_limits.hpp (firmwarefeste Grenzen, ohne sensor-/
     treiberspezifische Konstanten)
  2. temperature_source.hpp erweitern: TemperatureSampleStatus,
     TemperatureReading (gueltig-by-construction), ITemperatureSource
     unveraendert in der Signatur (Abschnitt 9.1)
  3. sensor_identity.hpp (eigenstaendig, gueltig-by-construction,
     Abschnitt 9.0)
  4. mock_temperature_source.hpp/.cpp auf den erweiterten Port nachziehen
     (setReading/setFault statt setCelsius/setAvailable)
  5. test_sensor_actuator_mocks.cpp: die drei temperaturbezogenen
     Testfunktionen nachziehen
  6. sensor_quality.hpp
  7. sensor_quality_config.hpp (NaN/Inf-sicher, Abschnitt 10.0)
  8. sensor_quality_snapshot.hpp (SensorFaultReason, SensorQualitySnapshot;
     correctedCelsius UND filteredCelsius bleiben in Slice 1 stets
     std::nullopt)
  9. sensor_quality_pipeline.hpp/.cpp: SampleDisposition, Zeitstempel-/
     Dispositionspruefung (9b), Transport-/Wertebereichs-/
     Aenderungsratenpruefung (10.1/10.2), Zustandsmaschine mit
     deriveQuality() (Abschnitt 8/9a); ingest(sample, now) ->
     SampleDisposition, snapshot(now) -> SensorQualitySnapshot
  10. test/test_temperature_source/, test/test_sensor_identity/,
      test/test_sensor_quality_config/, test/test_sensor_quality_pipeline/
      (Teilmenge: Start/Normalbetrieb, Transport-/Messfehler, Zeit/Alter/
      Disposition, Wertebereich, Zustandsmaschine/Wiedererkennung,
      Robustheit - inkl. lokaler Testhilfe fuer geskriptete Folgen)
  11. Dokumentation/Changelog-Eintrag fuer Slice 1
  12. Ressourcen-/CI-Nachweise fuer Slice 1

Slice 2 (Median + Offset + Tiefpass + vollstaendige Integration):
  13. sensor_offset.hpp (gueltig-by-construction, NaN/Inf-sicher,
      Abschnitt 13a)
  14. sensor_calibration.hpp (bindet SensorOffset an SensorIdentity,
      Abschnitt 13b, Korrektur Runde 4)
  15. sensor_median_filter.hpp/.cpp
  16. sensor_lowpass_filter.hpp/.cpp
  17. sensor_quality_pipeline.hpp/.cpp: setCalibration(), Integration von
      Offset/Median/Tiefpass, ROM-Wechsel-/FAILED-Filterreset (transiente
      STALE-Wiedererkennung erhaelt den Filterzustand, Abschnitt 8),
      Tiefpass-Verschiebung um das Offset-Delta bei gleichbleibender
      Identitaet (Abschnitt 10.4/10.5, Korrektur Runde 5),
      deriveQuality(nowMonotonicMs) als Referenzzeit fuer die interne
      Vorzustandsermittlung in ingest() (Abschnitt 9a, Korrektur Runde 5);
      correctedCelsius/filteredCelsius werden ab hier ab dem ersten
      plausiblen Beitrag tatsaechlich gesetzt
  18. test/test_sensor_offset/ (jetzt inkl. SensorCalibration-
      Konstruktionsfaellen, siehe Abschnitt 17a), test/test_sensor_median_filter/,
      test/test_sensor_lowpass_filter/, verbleibende Faelle in
      test/test_sensor_quality_pipeline/ (Median/Tiefpass-Integration,
      Kalibrierung/Offset, Identitaet/ROM-Wechsel, Filterreset-
      Feinunterscheidung, Widersprueche/Scopegrenzen)
  19. Dokumentation/Changelog-Eintrag fuer Slice 2
  20. Ressourcen-/CI-Nachweise fuer Slice 2, vollstaendiger Testlauf
```

## 21. Stopbedingung und freizugebender Plan-Commit

```text
Nach Commit und Push dieser Plan-Datei sowie Aktualisierung des Draft-PR:
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

Hinweis: Diese Taskleiste ist die urspruengliche 20-Punkte-
Planerstellungs-Checkliste aus dem ersten Plan-Auftrag (unveraendert
abgeschlossen). Die Taskleisten der beiden Nachkorrekturrunden sind ein
jeweils eigener, in der PR-Beschreibung #95 gezaehlter Scope und werden
nicht zusaetzlich hier eingetragen.
