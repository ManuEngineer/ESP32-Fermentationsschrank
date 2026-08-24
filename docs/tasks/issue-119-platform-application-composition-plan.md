# Issue #119 – produktive StateStore-/Application-Composition anbinden und real verifizieren (R1.0)

## Status, Ziel und Owner-Gate

Diese Datei ist die vollständige, eigenständig ausführbare kanonische
Planrevision R1.0 für Issue #119. Sie enthält **keine
Produktionsimplementation**. Bis zur ausdrücklichen Freigabe der exakten
Commit-SHA dieser Datei gilt:

```text
COMPOSITION_PLAN_PENDING_R1_0_OWNER_APPROVAL
```

Die Freigabe autorisiert noch keine Umsetzung. Schritt 1 und Schritt 2
(Abschnitt 8) erhalten je ein eigenes Owner-Gate.

```text
BASE_PR=#118
BASE_SHA=fd7e4e3ec58c9f3dda45710fd346752e083d7d19
IMPLEMENTATION=NOT_STARTED
```

Keine #118-Implementation wird kopiert oder cherry-gepickt.

## 1. Herkunft und Abgrenzung

Issue #119 ist der von Issue #90 R5.9 Punkt 9 selbst vorgesehene
separate, eigens ownerfreizugebende Composition-Schritt: R5.9 verbietet
eine unnötige Vollprodukt-/Composition-Root-Integration in #90 und lässt
`app_main`/`FermentationApplication` nur bei belegtem, separat
freizugebendem Bedarf berühren. Der reale Slice-7-Versuch (owner
freigegeben, real ausgeführt) hat diesen Bedarf belegt. #119 ist damit
keine neue #90-Planrevision, sondern die Folgeaktion, die R5.9 selbst
verlangt. #90/PR #118 bleibt unverändert auf R5.9
(`docs/tasks/issue-90-clean-restart-plan-r5.9.md @
baf0b2ae04cd42afa75dfa00e21d900116b38bc8`) als alleiniger kanonischer
Umsetzungsgrundlage.

Realer Befund aus #90 R5.9 Slice 7: Der ESP-IDF-Composition-Root
(`main/app_main.cpp`) besitzt den konkreten `NvsStateStore` bereits
(`NvsOwningContext` öffnet ihn real, `application: ready` auf zwei
unabhängigen realen Reboots bestätigt), stellt ihn dem
Application-/Recovery-Graph aber nicht zur Verfügung.
`FermentationApplication` verdrahtet aktuell nur ein `SafetyCore`, keinen
`ConfigurationRecoveryService`, `RunRecoveryCoordinator` oder Store.

Nicht wiedereröffnet: #71, #73, #74 (NVS jeweils ausdrücklich
Nicht-Scope). Nicht zweckentfremdet: #106 (Aktorplaner-Recovery-Bindung,
enger als der hier behandelte allgemeine Composition-Vertrag).

## 2. Verbindliche Architektur (ADR-013)

```text
                    main/app_main.cpp
                     Composition Root
                     /             \
                    v               v
     device_platform_esp_idf      konkrete App
       NvsStateStore              (heute:
             |                  FermentationApplication)
             v                       |
      device_platform                 |
        IStateStore <-----------------+
             |
             v
      app-eigene Persistenz-/
      Recovery-Komponenten
```

Verboten: `device_platform -> fermentation_app`,
`device_platform_esp_idf -> fermentation_app`,
`NvsStateStore -> FermentationApplication`, ein generischer
Plattform-Default mit Fermentationsbezug. Die Device Platform bestimmt
nur technische Persistenzfähigkeit, technische Store-Fehler und
anwendungsneutrale Limits. Die konkrete Anwendung bestimmt Records,
Recoverysemantik, Namespace-/Key-Zuordnung und das Zusammenspiel von
Configuration-/Run-/Safety-Fachlogik. `scripts/check_architecture_boundaries.py`
bleibt die verbindliche automatisierte Prüfung.

## 3. Architektur-Akzeptanzkriterium

> Eine hypothetische zweite konkrete Anwendung kann den vorhandenen
> `device_platform::IStateStore` und
> `device_platform_esp_idf::NvsStateStore` verwenden, indem sie im
> Composition Root ihre eigene app-spezifische Namespace-/Record-/Recovery-
> Bedeutung liefert. Dazu darf weder `device_platform` noch
> `device_platform_esp_idf` geändert werden müssen, sofern keine neue
> technische Plattformfähigkeit benötigt wird.

Dieser Nachweis ist strukturell (Architektur-/Dependency-Checks), nicht
durch eine Dummy-Zweit-App zu erbringen.

## 4. Reale Inventur (jetzt geprüft, nicht auf später verschoben)

Am `#118`-HEAD `fd7e4e3ec58c9f3dda45710fd346752e083d7d19` real geprüft:

- `main/app_main.cpp`: `NvsOwningContext::create()` initialisiert die
  `state_store`-Partition und öffnet real einen
  `device_platform_esp_idf::NvsStateStore` (Namespace `"fermentation"`
  als Aufrufargument, nicht als Adapterdefault). Danach werden
  `device_platform::DevicePlatform platform;`,
  `fermentation::FermentationApplication application;` und
  `const device_platform_esp_idf::EspResetCauseSource resetCauseSource;`
  auf dem Stack konstruiert. `application.begin(platform,
  &resetCauseSource)` erhält den Store aktuell **nicht**.
- `IPlatformServices` hat genau eine Methode (`ready()`), implementiert
  ausschließlich von `DevicePlatform` (`final`).
- `FermentationApplication::begin(IPlatformServices&, const
  IResetCauseSource*)` hält aktuell nur `platformServices_` (Zeiger) und
  `safetyCore_` (Member). Keine Configuration-/Run-Recoverykomponente ist
  Mitglied.
- `device_platform::IStateStore` ist bereits vollständig
  anwendungsneutral (nur `StateStoreReadStatus`/`StateStoreWriteStatus`,
  Byteschlüssel/-werte).
- `device_platform_esp_idf::NvsStateStore`/`NvsStateStoreConfig` tragen
  bereits den Kommentar, dass `"state_store"`/`"fermentation"`
  Composition-Root-Konfiguration sind, keine Plattformdefaults.
- Bereits vorhandene `lib/fermentation_app/`-Fachkomponenten akzeptieren
  `IStateStore&` bereits direkt als expliziten Konstruktorparameter, ohne
  Umweg über eine Plattform-Applikations-Kopplung:
  - `ConfigurationBootstrapStore(IStateStore&)`
  - `ConfigurationGraphStore(IStateStore&, const ITimeZoneResolver&)`
  - `ConfigurationService(ConfigurationMutationCoordinator&, ConfigurationGraphStore&, const ITimeZoneResolver&)`
  - `RunPersistenceCoordinator(IStateStore&, …)`
  - `ConfigurationRecoveryService::create(IStateStore&, ConfigurationBootstrapStore&, ConfigurationGraphStore&, ConfigurationService&, ConfigurationMutationCoordinator&)`
  - `RunRecoveryCoordinator(RunPersistenceCoordinator&)`

### 4.1 `ITimeZoneResolver` besitzt keine produktive Implementierung

Real geprueft: es existieren ausschliesslich der Port selbst
(`lib/device_platform/src/time_zone_resolver.hpp`) und
`device_platform_test_support::MockTimeZoneResolver`
(standardmaessig immer `Success`, nur ueber `setStatus(...)` fuer Tests
umschaltbar). Es gibt **keinen** produktiven `device_platform_esp_idf`-
Adapter. `ConfigurationGraphStore` und `ConfigurationService` nehmen
`const ITimeZoneResolver&` bereits als Pflicht-Konstruktorparameter; ohne
produktive Implementierung ist die reale Composition nicht herstellbar,
ohne entweder den Testmock zu missbrauchen (verboten) oder eine echte
Instanz zu schaffen.

Einziger realer Aufrufer ist `validateUserConfiguration(...)`
(`lib/fermentation_app/src/configuration_documents.cpp`, Zeile 94):
`resolver.prepare(configuration.timeZoneId)` wird erst aufgerufen,
**nachdem** `firmware_configuration_catalog::containsTimeZoneId(...)`
bereits gegen den fest kompilierten Firmwarekatalog
(`lib/fermentation_app/src/firmware_configuration_catalog.cpp`) geprueft
hat. Dieser Katalog enthaelt fuer Release 1 exakt einen Eintrag:
`"Europe/Zurich"`. `prepare()` wird im realen Produktpfad also nie mit
einer beliebigen IANA-ID aufgerufen, sondern ausschliesslich mit bereits
katalogseitig bestaetigten Werten — aktuell genau diesem einen.

Der Interface-Header selbst dokumentiert: "Eine reale
ESP32-Zeitzonendatenbank ist nicht Bestandteil dieses Ports." Issue #55
schliesst "reale ESP32-Zeitzonendatenbank, Betriebssystemintegration und
lokale-Zeit-nach-UTC-Terminplanung" ausdruecklich als Nicht-Scope aus.

Realer, bereits vorhandener Beleg fuer den minimal noetigen
Produktionsvertrag: der im Rahmen von #90 R5.9 Slice 7 real gepruefte
Oracle-Test
(`test/test_issue90_product_recovery_oracle/test_issue90_product_recovery_oracle.cpp`,
Zeile 2991 ff.) modelliert das erwartete Produktionsverhalten explizit
ueber eine Klasse `ProductionResolver`, deren `prepare()`
**ausschliesslich einen exakten Stringvergleich gegen `"Europe/Zurich"`**
durchfuehrt und bei Nichtuebereinstimmung `UnsupportedIdentifier`
liefert — ohne jede Betriebssystem- oder Datenbankinteraktion. Diese
bereits ownerseitig durchgelaufene Testevidenz zeigt konkret, welchen
minimalen Vertrag ein produktiver Adapter tatsaechlich erfuellen muss;
siehe Entscheidung 5.1.

### 4.2 `StorageEpoch`-Quelle und `RunCheckpointSchedule`

- `RuntimeConfigurationSnapshot::storageEpoch()`
  (`lib/fermentation_app/src/runtime_configuration_snapshot.hpp`, Zeile
  15) liefert die gueltige, aus der real geladenen Configuration-Wahrheit
  stammende `StorageEpoch`. Sie wird ueber
  `ConfigurationService::acquireRuntime()` erreicht (Rueckgabe
  `RuntimeConfigurationReadResult` mit `RuntimeConfigurationReadLease`);
  `acquireRuntime()` liefert nur dann eine gueltige Lease
  (`RuntimeConfigurationReadStatus::RuntimeLeaseGranted`), wenn
  `mode() == ConfigurationServiceMode::Operational` und eine aktive
  Runtime vorhanden ist
  (`lib/fermentation_app/src/configuration_service.cpp`, Zeile 624 ff.);
  sonst `ConfigurationRuntimeUnavailable` bzw. bei erschoepftem
  Lease-Budget `RuntimeReadLeaseBusy`.
- `RunPersistenceCoordinator` hat keinen Default- oder epoch-losen
  Konstruktor
  (`RunPersistenceCoordinator(IStateStore&, StorageEpoch,
  RunCheckpointSchedule)`,
  `lib/fermentation_app/src/run_persistence_coordinator.hpp`, Zeile 176).
  Ein gueltiges Objekt kann folglich erst existieren, nachdem eine reale
  `StorageEpoch` vorliegt.
- `RunCheckpointSchedule` hat einen Default-Konstruktorparameter
  `kDefaultRunCheckpointIntervalMinutes`
  (`lib/fermentation_app/src/run_checkpoint_schedule.hpp`, Zeile 19) —
  ein bereits bestehender, fachlich vereinbarter fester Wert, kein neu zu
  erfindender Compositionswert. `RunCheckpointSchedule{}` genuegt.
- `ConfigurationRecoveryService::create(...)` liefert `nullptr`
  ausschliesslich bei Identitaetsverletzung der uebergebenen Referenzen
  (Store-/GraphStore-/MutationCoordinator-/Resolver-Identitaet stimmt
  nicht ueberein,
  `lib/fermentation_app/src/configuration_recovery_service.cpp`, Zeile
  210 ff.) — ein reiner Compositionsverdrahtungsfehler, kein Laufzeit-
  oder Umweltfehler. Bei korrekt implementierter Composition tritt dieser
  Fall nie ein; er bleibt dennoch ein zu behandelnder Fehlerpfad (siehe
  5.3).
- `RunRecoveryCoordinator` hat sowohl einen Default-Konstruktor
  (`RunRecoveryCoordinator() = default;`) als auch Ueberladungen, die den
  `RunPersistenceCoordinator&` explizit als Parameter statt als
  gebundenes Member erwarten
  (`lib/fermentation_app/src/run_recovery.hpp`). Er kann also unabhaengig
  von einer gueltigen Epoche immer sicher konstruiert werden.
- `NvsOwningContext` (`main/app_main.cpp`) kapselt den geoeffneten Store
  aktuell vollstaendig privat und bietet **keinen** Zugriff auf
  `device_platform::IStateStore&` nach aussen. Fuer die Composition ist
  ein minimaler lesender Accessor auf den bereits geoeffneten Store
  noetig (siehe 5.2); das ist eine Aenderung ausschliesslich an dieser
  bereits im Composition Root lebenden Klasse, keine neue
  Plattformfaehigkeit.

### 4.3 Boot-Orchestrierung und Safety-Projektion: bereits vorhandenes Referenzmuster

`SafetyCoreInput`
(`lib/fermentation_app/src/safety_core.hpp`, Zeile 62 ff.) verlangt neben
Configuration-/Persistence-Evidenz auch Sensor- und Planner-Evidenz
(`peltierSensor`, `sensorSelectionRuntime`, `actuatorPlanner`,
`sensorEvidenceValidated`). `RunRecoveryCoordinator::activate(...)`
verlangt zwingend `const CrossRolePlausibilityContext&
liveSensorEvidence` und wird deshalb im realen Produktpfad heute noch
nicht aufgerufen — nur `RunPersistenceCoordinator::loadAndInitialize()`
liefert bereits ohne Sensorevidenz verwertbare Recovery-Fakten
(`RunPersistenceLoadStatus`, optionaler `RunPersistenceSnapshot`). Der
bereits vorhandene Oracle-Test
(`evaluateProductionSafety`/`runConfigurationProduction`/
`runRunProduction`, Zeile 3022 ff.) bestaetigt real genau dieses Muster:
Sensor-/Planner-Felder bleiben leer/`false`, solange kein Snapshot
vorliegt; die Boot-Disposition bleibt dadurch bereits durch den
bestehenden `SafetyCore`-Vertrag nicht-aktivierend (`Standby`/
`ResumeOffer`/`NoActiveRun`/`SafeBoot`), nie `Allowed`.

`FermentationApplication::begin(...)`
(`lib/fermentation_app/src/fermentation_application.cpp`) konstruiert
heute explizit ein default-initialisiertes, leeres `SafetyCoreInput
bootEvidence;` mit dem Kommentar, dass die Composition Root reale
Evidenz noch nicht liefert. Genau diese Luecke schliesst #119 (siehe
5.4/5.5).

`app_main()` (`main/app_main.cpp`) kehrt im Erfolgsfall **nie** zurueck
(`for (;;) { ... }` nach dem Start); lokale Variablen auf oberster Ebene
von `app_main()` (wie `stateStoreContext`, `platform`, `application`)
leben damit bereits heute prozessweit. Das ist das bestehende, zu
verwendende Lebenszeitmuster fuer neu hinzukommende
Compositionsobjekte, kein neuer Mechanismus.

## 5. Architekturentscheidung (jetzt entschieden)

```text
IStateStore bleibt eine separate, explizite Dependency.
IPlatformServices wird NICHT um IStateStore erweitert.
```

Begründung anhand der realen Inventur aus Abschnitt 4: `IPlatformServices`
enthält aktuell ausschließlich `ready()`; keine der bereits vorhandenen
Fachkomponenten konsumiert `IStateStore` über eine gebündelte
Services-Abstraktion, sondern jede nimmt `IStateStore&` als eigenen,
expliziten Konstruktorparameter entgegen — dasselbe Muster wie das
bereits bestehende zusätzliche `begin()`-Argument
`const IResetCauseSource*`. Eine Erweiterung von `IPlatformServices`
wäre ohne belegten Gegenbedarf eine unnötige God-Interface-Aggregation
und würde `DevicePlatform` (die einzige Implementierung) mit einer
fachfremden Zuständigkeit belasten. Die reale Inventur zeigt keinen
Gegenbeleg; kein `STOP – MATERIAL_ARCHITECTURE_DECISION_REQUIRED`.

Kleinste damit festgelegte Composition:

1. `main/app_main.cpp` (Composition Root) konstruiert nach erfolgreichem
   `NvsOwningContext::create()` — mit derselben Lebenszeit wie der Store —
   die bereits vorhandenen Fachobjekte `ConfigurationBootstrapStore`,
   `ConfigurationGraphStore`, `ConfigurationMutationCoordinator`,
   `ConfigurationService`, `ConfigurationRecoveryService` (via
   `::create(...)`), bedingt `RunPersistenceCoordinator` und immer
   `RunRecoveryCoordinator`, alle über den bereits geöffneten
   `NvsStateStore` (als `IStateStore&`). Das ist ADR-013-konform: der
   Composition Root darf konkrete Anwendungsmodule instanziieren.
2. `FermentationApplication::begin(...)` erhält diese bereits
   konstruierten Recoverykomponenten und die bereits real erzeugten
   Boot-Ergebnisse als zusätzliche, explizite Referenz-/Zeigerparameter
   (analog zum bestehenden `const IResetCauseSource*`-Muster) statt sie
   selbst zu besitzen, selbst zu erzeugen oder über `IPlatformServices`
   zu beziehen. `FermentationApplication` bleibt Eigentümer nur der
   Verknüpfung zu `SafetyCore`, nicht der Store-/Persistenzobjekte.
3. Kein neuer Port, kein neues Schema, keine neue generische
   Application-API. Exakte Parameterreihenfolge, Fehlerpfade und die
   endgültige Methodensignatur sind mit diesem Plan bereits jetzt
   festgelegt (5.1–5.6), nicht Schritt 1 überlassen.

### 5.1 Produktiver `ITimeZoneResolver`-Adapter

```text
MATERIAL_ARCHITECTURE_DECISION_OPEN=NO
```

Ein kleiner, generischer `device_platform_esp_idf::EspTimeZoneResolver`
wird als expliziter, minimaler Bestandteil von #119 geplant:

- Eigentümerschaft: `device_platform_esp_idf` (analog
  `EspResetCauseSource`, `EspTimerTimeSource`, `NvsStateStore`); keine
  Abhängigkeit auf `fermentation_app`.
- Vertrag: eine kleine, statisch kompilierte, anwendungsneutrale Tabelle
  bereits als "vom Geräteplattform-Adapter verifiziert unterstützt"
  bekannter kanonischer IANA-Bezeichner. Für R1 enthält sie exakt den
  einen Wert, den der Oracle-Referenztest bereits real modelliert:
  `"Europe/Zurich"`. Bekannte Bezeichner liefern `Success` mit demselben
  Bezeichner als `PreparedTimeZone::canonicalIdentifier` (Echo, keine
  Umwandlung); unbekannte liefern `UnsupportedIdentifier`. Das ist kein
  No-op/Always-success-Resolver: er unterscheidet real zwischen
  unterstützten und nicht unterstützten Bezeichnern anhand einer
  echten, wenn auch aktuell einelementigen, Plattformtabelle — exakt das
  bereits vorhandene, ownerseitig durchgelaufene
  `ProductionResolver`-Verhalten aus dem #90-R5.9-Slice-7-Oracle
  (Abschnitt 4.1), nicht neu erfunden.
- Der Adapter ruft **bewusst kein** `setenv("TZ", …)`/`tzset()` und
  berührt keinen realen Betriebssystem-Zeitzonenzustand. Das ist keine
  Verkürzung, sondern deckt sich exakt mit dem, was `prepare()`
  laut Interface-Kommentar und Issue #55 tatsächlich leisten muss: eine
  bereits katalogseitig geprüfte kanonische ID plattformseitig als
  vorbereitbar bestätigen oder ablehnen — nicht die reale
  lokale-Zeit-nach-UTC-Terminplanung oder Systemzeitintegration
  herstellen. Beides bleibt, wie in #55 explizit festgelegt, außerhalb
  dieses Adapters und außerhalb von #119.
- Keine neue Datenbank: die Tabelle ist ein einzelner, im Code
  dokumentierter Fakt (dieselbe Größenordnung wie der bereits
  bestehende `kTimeZones`-Fixarray in
  `firmware_configuration_catalog.cpp`), keine allgemeine
  IANA-Zeitzonendatenbank und keine neue externe Abhängigkeit. Herkunft:
  der Bezeichner `"Europe/Zurich"` ist ein offizieller, gemeinfreier
  (public domain) Eintrag der IANA Time Zone Database; keine
  Lizenzpflicht, keine Drittkomponente im Sinne von
  `docs/THIRD_PARTY_COMPONENTS.md`.
- Erweiterung (weitere Bezeichner) erfolgt ausschließlich durch bewusste
  Erweiterung dieser Plattformtabelle in `device_platform_esp_idf`,
  unabhängig vom jeweiligen App-Katalog — konsistent mit dem
  Architektur-Akzeptanzkriterium aus Abschnitt 3.

### 5.2 `StorageEpoch`-Herkunft und Composition-/Boot-Reihenfolge

```text
MATERIAL_ARCHITECTURE_DECISION_OPEN=NO
```

Exakte Reihenfolge im Composition Root (`main/app_main.cpp`), alles vor
`application.begin(...)`:

1. `NvsOwningContext::create()`. Fehlschlag → unverändert bestehender
   Abbruch (kein Applikationsstart, wie heute).
2. `NvsOwningContext` erhält einen minimalen lesenden Accessor auf den
   bereits geöffneten Store (`device_platform::IStateStore& store()
   const`). Das ist eine Änderung ausschließlich an dieser bereits im
   Composition Root lebenden Klasse, keine neue Plattformfähigkeit.
3. `device_platform::DevicePlatform platform; platform.begin(...)`
   (unverändert bestehend).
4. `device_platform_esp_idf::EspTimeZoneResolver timeZoneResolver;`
   (zustandslos, kein Init nötig).
5. Auf oberster `app_main()`-Ebene (Prozesslebenszeit, siehe 4.3)
   konstruiert: `ConfigurationMutationCoordinator mutationCoordinator;`,
   `ConfigurationBootstrapStore bootstrapStore(store);`,
   `ConfigurationGraphStore graphStore(store, timeZoneResolver);`,
   `ConfigurationService configurationService(mutationCoordinator,
   graphStore, timeZoneResolver);`.
6. `auto configurationRecoveryService = ConfigurationRecoveryService::create(store, bootstrapStore, graphStore, configurationService, mutationCoordinator);`
   Fehlerpfad: `== nullptr` → identisch zum bestehenden
   `NvsOwningContext::create() == nullptr`-Muster: kompletter
   Anwendungsstart abgebrochen, kein `application.begin(...)`. Dieser
   Fall ist laut Abschnitt 4.2 ein reiner Verdrahtungsfehler der
   Composition, kein Umweltfehler, und wird deshalb nicht als
   Safety-Evidenz projiziert, sondern als Startfehler behandelt.
7. `const auto configurationRecoveryResult = configurationRecoveryService->boot();`
   Kein Abbruch hier: **jedes** Ergebnis, auch ein Fehlerstatus, wird
   unverändert an `application.begin(...)` weitergereicht (siehe 5.4) —
   fail-closed entsteht durch die bereits bestehende
   `SafetyCore`-Fehlerklassifikation, nicht durch frühen Abbruch.
8. Nur wenn `configurationRecoveryResult.status` einer der drei
   Erfolgsstatus ist (`RuntimeReady`, `FactoryInitializationCompleted`,
   `FactoryResetCompleted`):
   `auto runtimeRead = configurationService.acquireRuntime();`
   Nur wenn `runtimeRead.status == RuntimeConfigurationReadStatus::RuntimeLeaseGranted`:
   `const auto storageEpoch = runtimeRead.lease.get().storageEpoch();`,
   danach `runPersistenceCoordinator.emplace(store, storageEpoch, RunCheckpointSchedule{});`
   (`std::optional<RunPersistenceCoordinator>` auf `app_main()`-Ebene).
   Die Lease wird durch Scope-Ende der lokalen `runtimeRead` sofort
   wieder freigegeben (RAII), bevor `application.begin()` läuft — kein
   Lease wird über den Bootpfad hinaus gehalten.
   In jedem anderen Fall (Recovery nicht erfolgreich, oder Erfolg aber
   keine Lease erhalten) bleibt `runPersistenceCoordinator` leer. Es
   wird **kein** Ersatzepoche (`StorageEpoch{1}` o. ä.) erfunden, nur
   damit der Pfad kompiliert — genau das ist hiermit ausgeschlossen.
9. Nur wenn `runPersistenceCoordinator` befüllt wurde:
   `const auto runPersistenceLoadResult = runPersistenceCoordinator->loadAndInitialize();`.
10. `fermentation::RunRecoveryCoordinator runRecoveryCoordinator;`
    immer default-konstruiert (kostenlos, keine Epoche nötig, siehe
    4.2); bei vorhandenem `runPersistenceCoordinator` wird er über die
    explizite `RunPersistenceCoordinator&`-Parameterüberladung seiner
    Methoden verwendet (Abschnitt 4.2), nicht über den Konstruktor
    zwingend gebunden.
11. `application.begin(platform, configurationService,
    configurationRecoveryResult, runPersistenceCoordinator ?
    &*runPersistenceCoordinator : nullptr, runPersistenceCoordinator ?
    &runPersistenceLoadResult : nullptr, &runRecoveryCoordinator,
    &resetCauseSource);`

### 5.3 Fehlerpfade bei jedem Init-/Create-/Boot-/Load-Schritt

| Schritt | Fehlerfall | Ergebnis |
|---|---|---|
| `NvsOwningContext::create()` | `nullptr` | Anwendungsstart abgebrochen (unverändert bestehend) |
| `ConfigurationRecoveryService::create(...)` | `nullptr` (Identitätsverletzung, 4.2) | Anwendungsstart abgebrochen, kein `application.begin(...)` |
| `ConfigurationRecoveryService::boot()` | beliebiger Nicht-Erfolgsstatus | An `begin()` weitergereicht; `SafetyCoreInput.configurationValidated=false`; bestehende Fault-Klassifikation greift (`ConfigurationUnavailable`/`ConfigurationIntegrityFailure`/…) |
| `ConfigurationService::acquireRuntime()` nach Boot-Erfolg | kein `RuntimeLeaseGranted` | Kein `RunPersistenceCoordinator` konstruiert; `configurationValidated` bleibt dennoch am realen Boot-Status ausgerichtet, `persistenceValidated=false` |
| `RunPersistenceCoordinator::loadAndInitialize()` | jeder `RunPersistenceLoadStatus` ungleich der drei vertrauten Erfolgswerte (`NoPersistedRun`/`Current`/`NoActiveRun`) bzw. `FallbackRecovered` ohne Snapshot | `persistenceValidated=false`; Status unverändert real projiziert, keine stille Umdeutung |

Kein `NOT_RUN -> PASS`, kein Ersatzwert, kein stiller Erfolgsfall bei
irgendeinem dieser Schritte (siehe auch Abschnitt 10).

### 5.4 Produkt-Boot-Orchestrierung und Safety-Projektion

```text
MATERIAL_ARCHITECTURE_DECISION_OPEN=NO
```

Der Composition Root (`main/app_main.cpp`), **nicht**
`FermentationApplication::begin(...)`, führt die reale Recovery-Sequenz
aus (`boot()`, bedingt `loadAndInitialize()`) — zwingend so, weil
`RunPersistenceCoordinator` laut 4.2 erst nach einer real ermittelten
`StorageEpoch` konstruiert werden kann und der Composition Root diesen
bereits konstruierten Coordinator an `begin()` übergibt (5.1–5.2).
`begin()` orchestriert damit keinen zweiten Recovery-Ablauf; es
projiziert ausschließlich bereits real erzeugte Ergebnisse in
`SafetyCoreInput` und ruft `safetyCore_.evaluate(...)` — genau die
Rolle, die `begin()` heute schon hat (aktuell mit einem leeren
Platzhalter statt echter Evidenz, siehe 4.3).

Projektion (spiegelt exakt das bereits real geprüfte Oracle-Muster aus
Abschnitt 4.3 wider, keine neue Regelimplementierung):

```text
input.bootValidationComplete       = true   // ein realer Bootvalidierungslauf hat stattgefunden
input.configurationValidated       = status ∈ {RuntimeReady, FactoryInitializationCompleted, FactoryResetCompleted}
input.configurationRecoveryStatus  = configurationRecoveryResult.status
input.configurationServiceMode     = configurationService.mode()          // live
input.configurationProducer        = configurationRecoveryResult.safetyProducer
input.persistenceValidated         = persistenceLoadStatus ∈ {NoPersistedRun, Current, NoActiveRun}
                                      ∨ (persistenceLoadStatus = FallbackRecovered ∧ snapshot vorhanden)
input.persistenceLoadStatus        = runPersistenceLoadResult?.status
input.persistenceSnapshot          = runPersistenceLoadResult?.snapshot (Zeiger auf Wert oder nullptr)
input.persistenceCoordinatorState  = runPersistenceCoordinator ? coordinator.state() : Uninitialized
```

Sensor-/Planner-/Aktivierungsfelder (`sensorEvidenceValidated`,
`peltierSensor`, `sensorSelectionRuntime`, `actuatorPlanner`,
`explicitActivationRequested`, `plannerEvidenceValidated`,
`activationKind`, `activationPersistenceResult`,
`processActivationApplied`) bleiben unverändert auf ihren
Default-/Leerwerten: #119 führt keine neue Sensor- oder Planner-Quelle
ein. Damit bleibt `RunRecoveryCoordinator::activate(...)` (verlangt
`CrossRolePlausibilityContext`) außerhalb von `begin()` unaufgerufen —
`FallbackRecovered` bleibt dadurch, wie durch den bereits bestehenden
`SafetyCore`-Vertrag garantiert, ein validiertes, nicht aktivierendes
Resume-Angebot; kein automatisches Resume und kein `Allowed`-Gate allein
durch einen erfolgreichen NVS-Read. Der `RunRecoveryCoordinator` bleibt
in `FermentationApplication` referenziert, um genau diesen späteren,
sensorgestützten Aktivierungspfad zu bedienen, sobald ein zukünftiges
Issue reale Sensor-/Planner-Evidenz liefert — keine zweite
Recovery-/Safety-Regelimplementierung entsteht dafür.

Es wird keine bestehende Production-vs-Oracle-/Safety-Vertragslogik im
Composition Root neu nachgebaut; `evaluate(...)` bleibt die alleinige
Auswertung.

### 5.5 `FermentationApplication::begin(...)`-Signatur, Ownership und Lifetime

```cpp
[[nodiscard]] bool begin(
    device_platform::IPlatformServices& platformServices,
    ConfigurationService& configurationService,
    const ConfigurationRecoveryResult& configurationRecoveryResult,
    RunPersistenceCoordinator* runPersistenceCoordinator,
    const RunPersistenceLoadResult* runPersistenceLoadResult,
    RunRecoveryCoordinator* runRecoveryCoordinator,
    const device_platform::IResetCauseSource* resetCauseSource = nullptr);
```

`runPersistenceCoordinator`/`runPersistenceLoadResult` sind gemeinsam
entweder beide `nullptr` oder beide gesetzt (5.2/5.3).
`runRecoveryCoordinator` ist immer gesetzt (immer default-konstruierbar,
4.2). Der bestehende `const IResetCauseSource* = nullptr`-Parameter
bleibt unverändert letzter Parameter mit Default.

Ownership/Lifetime:

- Composition Root (`app_main()`, Prozesslebenszeit gemäß 4.3) **besitzt**
  weiterhin: den Store (`NvsOwningContext`), `ConfigurationBootstrapStore`,
  `ConfigurationGraphStore`, `ConfigurationMutationCoordinator`,
  `EspTimeZoneResolver`, `ConfigurationRecoveryService`
  (`std::unique_ptr`), `std::optional<RunPersistenceCoordinator>`,
  `RunRecoveryCoordinator`.
- `FermentationApplication` **referenziert nur** (rohe, nicht besitzende
  Zeiger/Referenzen, analog zum bestehenden `platformServices_`- und
  `IResetCauseSource*`-Muster): `configurationService_`,
  `runPersistenceCoordinator_`, `runRecoveryCoordinator_`. Sie überleben
  `FermentationApplication` in jedem Fall, da der Composition Root nie
  zurückkehrt (4.3).
- `ConfigurationBootstrapStore`, `ConfigurationGraphStore`,
  `ConfigurationMutationCoordinator` werden von
  `FermentationApplication` **nicht** referenziert — sie sind reine
  interne Kollaborateure von `ConfigurationService`/
  `ConfigurationRecoveryService` (KISS-Prüfung, 5.6) und nur intern
  friend-referenziert, nie von außen aufgerufen.
- `ConfigurationRecoveryService` wird von `FermentationApplication`
  für #119 **nicht** referenziert (siehe 5.6); der Composition Root
  behält ihn für eine spätere, in #119 nicht gebaute
  Factory-Reset-Auslösung.

### 5.6 KISS-Prüfung der `FermentationApplication`-Schnittstelle

Nicht automatisch jedes Recoveryobjekt als eigener `begin()`-Parameter:
geprüft wurde für jedes Objekt aus Abschnitt 4, ob
`FermentationApplication` es nach `begin()` tatsächlich braucht.

| Objekt | Nach `begin()` gebraucht? | Grund |
|---|---|---|
| `ConfigurationService` | Ja | Laufende Config-Reads/-Writes und `mode()`/`acquireRuntime()` in künftigen Issues; einzige laufende Wahrheit |
| `ConfigurationRecoveryResult` | Nein | Einmaliger Bootbefund, nicht laufend gültig; wird nur einmal in `SafetyCoreInput` projiziert |
| `ConfigurationBootstrapStore`/`ConfigurationGraphStore`/`ConfigurationMutationCoordinator` | Nein | Reine interne Kollaborateure von `ConfigurationService`/`ConfigurationRecoveryService`, nie direkt von außen aufgerufen |
| `ConfigurationRecoveryService` | Nein (für #119) | `beginAuthorizedFactoryReset()` hat in #119 keinen Aufrufer; Composition Root hält ihn ohnehin lebendig, falls ein späteres Issue ihn braucht — keine Signaturänderung nötig, da nicht über `FermentationApplication` geführt |
| `RunPersistenceCoordinator` | Ja (bedingt) | Laufende Persistenzoperationen (`persistCommand`, `checkpointPeriodic`, …) sind explizit Teil des #119-Ziels "Komponenten über den echten Produktpfad verbinden" |
| `RunPersistenceLoadResult` | Nein | Einmaliger Bootbefund, nur zur Projektion |
| `RunRecoveryCoordinator` | Ja | Später sensorgestützter Aktivierungspfad (5.4) |

Ergebnis: die in 5.5 festgelegte Signatur mit expliziten Referenz-/
Zeigerparametern bleibt nach dieser Prüfung die kleinste Lösung. Kein
neuer Container, kein Service Locator, kein DI-Framework, keine neue
generische Application-API.

## 6. Keine vorschnelle neue generische Application-API

Bestätigt durch Abschnitt 4/5: kein `IApplication`, `IRecoveryApplication`,
`IApplicationPersistence`, `ApplicationContainer`, kein Service Locator,
kein DI-Framework, kein zweiter Recovery-Orchestrator. Der heutige
Composition Root verdrahtet mit den bereits bestehenden Verträgen direkt.

## 7. Scope

### In Scope

**Plattform-/Composition-Seite:** `NvsStateStore` bleibt im
ESP-IDF-Composition-Root besessen; anwendungsneutrale Übergabe über die
in Abschnitt 5 festgelegte kleinste Verdrahtung; fail-closed
Init/Open/Lifetime; keine konkrete App im Plattformmodul.

**Aktueller erster Consumer:** die bereits vorhandenen Configuration
Boot/Recovery-, Run-Persistence/Recovery- und SafetyCore-/Gate-
Projektionskomponenten am Fermenter mit dem realen Store über den echten
Produktpfad verbinden. Diese Komponenten bleiben dort, wo ihre fachliche
Eigentümerschaft bereits liegt (`lib/fermentation_app/`); sie werden
nicht wegen Wiederverwendbarkeit nach `device_platform` verschoben.

**Reale Verifikation:** die aus #90 R5.9 Slice 7 übertragenen Gates,
actor-free (siehe Abschnitt 9 für den vollständigen R5.9-Vertrag):

```text
BOARD_IDENTITY_GATE          (bereits real PASS, wird nicht grundlos
                               wiederholt, sofern Board/Setup unverändert)
UART_RESET_GATE               (bereits real PASS, dito)
REAL_NVS_RECOVERY_GATE
CALLBACK_12_REAL_NVS_PRODUCT_GATE
FULL_RUNTIME_STACK_HEADROOM  (reale HWM unter dem dann existierenden
                               Recovery-Workload)
MANUAL_POWER_CUT_GATE
```

### Nicht Scope

Allgemeines App-Framework; Plugin-System; Service Locator; Dependency
Registry; generische Recovery-Engine für alle zukünftigen Apps;
Verschieben von Fermentations-Configuration-/Run-/Safetysemantik in
`device_platform`; neue Persistenzengine; neuer Store nur für den
Fermenter; neues Wireformat oder Schema ohne belegten Bedarf; Sensor-,
UI-, WLAN- oder Aktorintegration; reale GPIO-/MOSFET-/BTS7960-/
Peltierfreigabe.

## 8. Owner-gatete Umsetzungsschritte

Zwei fachliche Schritte, bewusst KISS gehalten — keine weitere
Inventur-/Entscheidungsphase nach der Freigabe, da Abschnitt 4/5 dies
bereits jetzt real geleistet haben.

### Schritt 1 – minimale echte Composition

Die in Abschnitt 5 vollständig festgelegte Verdrahtung 1:1 umsetzen,
ohne neue Designentscheidungen: neuen produktiven
`device_platform_esp_idf::EspTimeZoneResolver` (5.1); minimalen
`store()`-Accessor auf `NvsOwningContext` (5.2 Schritt 2); die exakte
Boot-/Composition-Reihenfolge inklusive `StorageEpoch`-Beschaffung
(5.2); die Fehlerpfadtabelle (5.3); die reale
`SafetyCoreInput`-Projektion (5.4); die neue
`FermentationApplication::begin(...)`-Signatur und ihre Ownership-/
Lifetime-Zuordnung (5.5/5.6). Vorhandenen `NvsStateStore`/`IStateStore`
verwenden; vorhandene app-seitigen Recoverykomponenten verwenden;
kleinste notwendige Ownership-/Lifetime-Verdrahtung im Composition
Root. Keine neue generische Application-API, kein Service Locator,
kein DI-Framework, kein zweiter Recovery-Orchestrator, keine
alternative Diagnose-Composition.

Gate: `scripts/check_architecture_boundaries.py` PASS; keine neue
Rückwärtsabhängigkeit; Akzeptanzkriterium aus Abschnitt 3 strukturell
erfüllt.

### Schritt 2 – gezielte Verifikation

Software/Build: direkt betroffene Tests wiederverwenden, nur notwendige
neue Compositiontests; Architekturgrenzen explizit prüfen; beide
ESP-IDF-Profile; nur durch die Composition betroffene Ressourcen (Stack/
Scratch nur dort, wo sich der Main-Task-Callgraph durch die Composition
tatsächlich ändert; die aus #90 bekannte statische Zusatzuntersuchung
bleibt informativ, kein eigenständiges hartes Gate).

Real actor-free, vollständig gemäß dem in Abschnitt 9 übernommenen
R5.9-Slice-7-Vertrag:

```text
REAL_NVS_RECOVERY_GATE
CALLBACK_12_REAL_NVS_PRODUCT_GATE
FULL_RUNTIME_STACK_HEADROOM
MANUAL_POWER_CUT_GATE
```

Bereits gültige Board-/UART-Evidenz (`BOARD_IDENTITY_GATE=PASS`,
`UART_RESET_GATE=PASS`) nicht grundlos wiederholen.

## 9. R5.9-Slice-7-Vertrag vollständig übernommen

Schritt 2 erfüllt den in #90 R5.9 Slice 7 bereits definierten realen
Vertrag, ohne eine neue Testmatrix zu erfinden:

- realer Boot/Reboot/NVS-Reload;
- repräsentative Configuration-/Run-Records;
- vollständige Record-/Envelope-/CRC-/Schema-/Epoch-/Referenzvalidierung;
- Recoveryoutcome;
- Safety-Projektion;
- logischer Gate;
- kein physischer Aktortest;
- Callback 12 bleibt Backend-Known-Limitation, separat vom Produktgate
  bewertet;
- mindestens drei reale manuelle Power-Cuts pro ausgewähltem relevantem
  Szenario während aktiver Schreiblast;
- reale Runtime-HWM unter dem dann existierenden Recovery-Workload;
- fail-closed bei unvollständiger Evidenz (siehe Abschnitt 10).

## 10. Fail-closed Regeln (unverändert aus #90 R5.9 übernommen)

Kein `NOT_RUN -> PASS`, kein `BLOCKED -> PASS`, kein unerwarteter
NVS-Zustand als stiller `NoActiveRun`, kein unklarer
Configuration-Zustand als stiller Factory-New, kein unklarer Run als
stiller Discard, kein Fallback als automatisches Resume, kein
Recoverystatus als physische Aktorfreigabe. Der actor-free logische Gate
bleibt bei unvollständiger Evidenz geschlossen.

## 11. Dynamische Statuspflege

Nach jedem Schritt werden HEAD, Plan-SHA, Ergebnisstatus, offene Befunde
und der nächste Owner-Schritt im Draft-PR #120, in Issue #119 und in
`docs/ROADMAP.md` synchronisiert, sowie genau ein aktueller
`SESSION HANDOVER` gepflegt. Issue #90 wird bei Bedarf informativ
verlinkt, aber nicht durch #119-Fortschritt automatisch geschlossen.

## 12. Abnahmekriterien dieser Planrunde

| Nachweis | Status |
|---|---|
| Live-PR-/Issue-/Epic-/Roadmap-Abgleich | `PASS` |
| Reale Architekturinventur (Abschnitt 4) | `PASS` |
| `ITimeZoneResolver`-Produktionslücke untersucht und entschieden (4.1/5.1) | `PASS` |
| `StorageEpoch`-Herkunft und Boot-/Composition-Reihenfolge festgelegt (4.2/5.2) | `PASS` |
| Fehlerpfade für jeden Init-/Create-/Boot-/Load-Schritt festgelegt (5.3) | `PASS` |
| Produkt-Boot-Orchestrierung und Safety-Projektion festgelegt (5.4) | `PASS` |
| `begin(...)`-Signatur, Ownership/Lifetime festgelegt, nicht auf Schritt 1 verschoben (5.5) | `PASS` |
| KISS-Prüfung der `FermentationApplication`-Schnittstelle (5.6) | `PASS` |
| Architekturentscheidung getroffen und begründet (Abschnitt 5) | `PASS` |
| `MATERIAL_ARCHITECTURE_DECISION_OPEN` | `NO` |
| Akzeptanzkriterium definiert | `PASS` |
| Neue Produktions-/Test-/Compositionimplementation | `NOT_RUN` / nicht enthalten |
| Reale Boardverifikation (Schritt 2) | `NOT_RUN` bis eigene Freigabe |
| Exakte R1.0-Planfreigabe | `BLOCKED` bis Ownerentscheidung |

STOP nach Freigabe dieser Datei; keine Implementation vor Freigabe von
Schritt 1.
