# Issue #119 – produktive StateStore-/Application-Composition anbinden und real verifizieren (R1.1)

## Status, Ziel und Owner-Gate

Diese Datei ist die vollständige, eigenständig ausführbare kanonische
Planrevision **R1.1** für Issue #119. Sie ersetzt R1.0 in place; R1.0
bleibt über ihre freigegebene SHA historisch nachvollziehbar. Sie enthält
weiterhin **keine Produktionsimplementation**. Bis zur ausdrücklichen
Freigabe der exakten Commit-SHA dieser Datei gilt:

```text
COMPOSITION_PLAN_PENDING_R1_1_OWNER_APPROVAL
R1_1_IMPLEMENTATION=NOT_STARTED
PRODUCTION_CODE_CHANGED=NO
TEST_CODE_CHANGED=NO
HARDWARE_RETEST=NOT_RUN
```

Die Freigabe autorisiert noch keine Umsetzung.

```text
BASE_PR=#118
BASE_SHA=fd7e4e3ec58c9f3dda45710fd346752e083d7d19
APPROVED_R1_0_PLAN_SHA=5effae0d8b3ec82abe5e3864082b1ce237f8f03f
```

Keine #118-Implementation wird kopiert oder cherry-gepickt.

**Warum R1.1:** Schritt 1 (Abschnitt 8) wurde umgesetzt und ownerreviewt
(`PASS`). Schritt 2 (reale Boardverifikation) ist real durchgeführt worden
und real **fehlgeschlagen**: der Produktboot erreicht in
`NvsOwningContext::create()` bei `nvs_flash_init_partition("state_store")`
einen `Interrupt wdt timeout`, bevor Configuration-Recovery oder
Safety-Projektion erreicht werden. Eine eigene Ursachenanalyserunde
(PR #120) hat die Ursache real auf Hardware eingegrenzt und durch den
Owner akzeptiert. Abschnitt 13–19 dieser Datei sind die daraus folgende
**vollständige Planrevision** der Composition-Root-Struktur — sie ändern
nichts an der in Abschnitt 1–12 bereits getroffenen und weiterhin gültigen
Architekturentscheidung, korrigieren aber, **wie** die dort beschriebenen
boot-transienten Objekte im Composition Root gehalten werden:

```text
CAUSE_CLASS=#120_BINARY_OR_INTEGRATION_DEPENDENT
ROOT_CAUSE_DIRECTION=NOT_CONTENT_ONLY
BOOT_STACK_ROOT_CAUSE=OVERSIZED_APP_MAIN_ENTRY_FRAME
BOOT_STACK_FIX_DIRECTION=RESTRUCTURE_COMPOSITION_ROOT
MAIN_TASK_STACK_BLANKET_INCREASE=REJECTED_AS_PRIMARY_FIX
MATERIAL_ARCHITECTURE_DECISION_OPEN=NO
```

Die vollständige Ursachenanalyse-Evidenz (Watchdog-Typ, Backtrace,
A/B-Boardtests, `state_store`-Dump/Restore) steht im PR-#120-Kommentar
vom real durchgeführten Diagnoselauf; sie wird hier nicht kopiert, nur die
für die Planung tragenden Ergebnisse (Abschnitt 13) übernommen.

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
- `RunRecoveryCoordinator` existiert bereits im Code
  (`lib/fermentation_app/src/run_recovery.hpp`), mit Default-Konstruktor
  und `activate(...)`-Ueberladungen, die zwingend eine reale
  `CrossRolePlausibilityContext` (Sensorevidenz) verlangen. Diese
  Evidenz liefert #119 nicht (Nicht-Scope, Abschnitt 7); der Coordinator
  wird deshalb fuer #119 **nicht** konstruiert (YAGNI, 5.4), obwohl er
  technisch unabhaengig von einer gueltigen Epoche sicher konstruierbar
  waere.
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
   `::create(...)`) und bedingt `RunPersistenceCoordinator`, alle über
   den bereits geöffneten `NvsStateStore` (als `IStateStore&`). Das ist
   ADR-013-konform: der Composition Root darf konkrete Anwendungsmodule
   instanziieren. `RunRecoveryCoordinator` wird für #119 **nicht**
   konstruiert (YAGNI, 5.4): `activate(...)` verlangt reale
   Sensorevidenz, die #119 nicht liefert; die Dependency wird erst
   eingeführt, wenn ein späteres Issue sie produktiv braucht.
2. `FermentationApplication::begin(...)` erhält die bereits real
   erzeugten Boot-Ergebnisse und den bedingt konstruierten
   `RunPersistenceCoordinator` als zusätzliche, rein transiente
   Referenz-/Zeigerparameter (analog zum bestehenden
   `const IResetCauseSource*`-Muster) statt sie selbst zu besitzen,
   selbst zu erzeugen oder über `IPlatformServices` zu beziehen.
   `FermentationApplication` bleibt Eigentümer nur der Verknüpfung zu
   `SafetyCore`, nicht der Store-/Persistenz-/Configurationobjekte (5.5).
3. Kein neuer Port, kein neues Schema, keine neue generische
   Application-API. Exakte Parameterreihenfolge, Fehlerpfade und die
   endgültige Methodensignatur sind mit diesem Plan bereits jetzt
   festgelegt (5.1–5.6), nicht Schritt 1 überlassen.

### 5.1 Produktiver `ITimeZoneResolver`-Adapter

```text
MATERIAL_ARCHITECTURE_DECISION_OPEN=NO
```

Ein kleiner, generischer `device_platform_esp_idf::EspTimeZoneResolver`
wird als expliziter, minimaler Bestandteil von #119 geplant.

**Normative Quelle der Semantik** ist ausschließlich der bestehende
Portvertrag und Issue #55, **nicht** der Oracle-Test:

1. `ITimeZoneResolver` und sein Vertrag stammen aus Issue #55 bzw. dem
   bereits bestehenden Port (`lib/device_platform/src/time_zone_resolver.hpp`).
2. `validateUserConfiguration(...)` ruft `prepare()` erst **nach**
   struktureller Prüfung und Treffer im App-Firmwarekatalog auf
   (Abschnitt 4.1).
3. Issue #55 schließt reale ESP32-Zeitzonendatenbank,
   Betriebssystemintegration und lokale-Zeit-nach-UTC-Terminplanung
   ausdrücklich aus.
4. Der kleine Plattformadapter beantwortet deshalb nur die Frage, ob
   eine bereits app-seitig erlaubte kanonische ID auf dieser
   Geräteplattform vorbereitet/unterstützt ist — nicht mehr.
5. Der bereits reale, ownerseitig durchgelaufene
   `ProductionResolver` aus dem #90-R5.9-Slice-7-Oracle (Abschnitt 4.1)
   ist **bestätigende Testevidenz**, dass die hier geplante
   R1-Ausprägung mit der bereits geprüften Recovery-Evidenz konsistent
   ist — **nicht** die normative Quelle des Produktionsvertrags. Der
   Oracle-Test definiert nicht, was Produktion tun muss; der
   bestehende Port-/Issue-55-Vertrag tut das.

Design des Adapters:

- Eigentümerschaft: `device_platform_esp_idf` (analog
  `EspResetCauseSource`, `EspTimerTimeSource`, `NvsStateStore`); keine
  Abhängigkeit auf `fermentation_app`.
- Vertrag: eine kleine, statisch kompilierte, anwendungsneutrale Tabelle
  kanonischer IANA-Bezeichner, die diese Geräteplattform technisch als
  vorbereitet/unterstützt akzeptiert. Für R1 enthält sie exakt den
  einen Wert `"Europe/Zurich"`. Bekannte Bezeichner liefern `Success`
  mit demselben Bezeichner als `PreparedTimeZone::canonicalIdentifier`
  (Echo, keine Umwandlung); unbekannte liefern `UnsupportedIdentifier`.
  Das ist kein No-op/Always-success-Resolver: er unterscheidet real
  zwischen unterstützten und nicht unterstützten Bezeichnern anhand
  einer echten, wenn auch aktuell einelementigen, Plattformtabelle.
- ```text
  EspTimeZoneResolver does NOT configure system local time.
  ```
  Der Adapter ruft **bewusst kein** `setenv("TZ", …)`/`tzset()` und
  berührt keinen realen Betriebssystem-Zeitzonenzustand. Die offizielle
  ESP-IDF-Zeitzonenaktivierung über `TZ`/`tzset()` bleibt, wie in #55
  festgelegt, außerhalb dieses Adapters und außerhalb von #119.
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

**App-Katalog und Plattform-Support sind zwei getrennte
Verantwortungen**, auch wenn beide in R1 nur `Europe/Zurich` enthalten:

```text
fermentation_app firmware catalog
    -> welche TimeZoneIds diese konkrete App erlaubt

device_platform_esp_idf EspTimeZoneResolver
    -> welche kanonischen TimeZoneIds diese Plattform technisch
       als vorbereitet/unterstützt akzeptiert
```

Sie werden nicht zusammengelegt, nur weil sie zufällig deckungsgleich
sind. Fail-closed bleibt in jedem Fall:

```text
App erlaubt ID, Plattform unterstützt sie nicht
-> TimeZoneRejected
```

Mindestens ein gezielter Test in Schritt 1 muss belegen:

```text
Europe/Zurich -> Success + exact canonical echo
unknown ID -> UnsupportedIdentifier
```

Keine allgemeine IANA-Testmatrix; dieser Testumfang wird jetzt nur
benannt, nicht in dieser Planrunde implementiert (Abschnitt 8/9).

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
   `store()` liefert nur eine nicht besitzende Referenz; der Accessor
   überträgt keinerlei Ownership. `NvsOwningContext` bleibt alleiniger
   Lifetime-Owner des `NvsStateStore` und der initialisierten Partition
   (sein Destruktor zerstört den Store und deinitialisiert danach die
   Partition) und muss deshalb jeden `IStateStore`-Consumer überleben,
   der aus dieser Referenz entsteht (5.5).
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
9. `std::optional<RunPersistenceLoadResult> runPersistenceLoadResult;`
   auf `app_main()`-Ebene (**nicht** innerhalb eines inneren Scopes) —
   nur wenn `runPersistenceCoordinator` befüllt wurde:
   `runPersistenceLoadResult.emplace(runPersistenceCoordinator->loadAndInitialize());`.
   Damit gilt:
   ```text
   RunPersistenceCoordinator lifetime = app_main scope
   RunPersistenceLoadResult lifetime = mindestens bis application.begin() zurückkehrt
   ```
   Ein `const auto` innerhalb des `if`-Blocks aus Schritt 8 würde seinen
   Gültigkeitsbereich vor dem `begin()`-Aufruf verlassen — genau das
   wird hiermit ausgeschlossen.
10. `application.begin(platform, configurationService,
    configurationRecoveryResult, runPersistenceCoordinator ?
    &*runPersistenceCoordinator : nullptr, runPersistenceLoadResult ?
    &*runPersistenceLoadResult : nullptr, &resetCauseSource);`
    Kein `RunRecoveryCoordinator` wird konstruiert oder übergeben (siehe
    Kleinste-Composition-Punkt 1 und 5.4/5.5 — YAGNI: #119 hat keinen
    Aufrufer für `activate(...)`, der reale Sensorevidenz voraussetzt).
    Der Bootbefund muss nach `begin()` nicht dauerhaft in der App
    gespeichert werden (5.5).

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
ein.

**YAGNI: `RunRecoveryCoordinator` wird für #119 nicht konstruiert, nicht
referenziert und nicht als `begin()`-Parameter geführt.**
`RunRecoveryCoordinator::activate(...)` verlangt zwingend eine reale
`CrossRolePlausibilityContext` (Sensorevidenz), die #119 nicht liefert;
Sensor-/Plannerintegration ist ausdrücklich Nicht-Scope von #119
(Abschnitt 7). `begin()` ruft ihn deshalb schon strukturell nicht auf —
nicht weil eine Instanz zwar existiert, aber ungenutzt bliebe, sondern
weil in #119 gar keine Instanz existiert. Wenn ein späteres Issue reale
Sensor-/Planner-Evidenz besitzt und `RunRecoveryCoordinator::activate(...)`
produktiv braucht, wird die Dependency dann eingeführt — nicht jetzt auf
Vorrat. `FallbackRecovered` bleibt dadurch, wie durch den bereits
bestehenden `SafetyCore`-Vertrag garantiert, ein validiertes, nicht
aktivierendes Resume-Angebot; kein automatisches Resume und kein
`Allowed`-Gate allein durch einen erfolgreichen NVS-Read. Der #119-
Bootpfad benötigt für seine aktuelle Aufgabe nur:

```text
RunPersistenceCoordinator::loadAndInitialize()
-> RunPersistenceLoadResult
-> bestehende SafetyCore-Projektion
```

Keine zweite Recovery-/Safety-Regelimplementierung entsteht dafür; es
wird auch kein neuer Container oder eine neue Abstraktion geschaffen, um
`RunRecoveryCoordinator` trotzdem "bereitzuhalten".

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
    const device_platform::IResetCauseSource* resetCauseSource = nullptr);
```

`runPersistenceCoordinator`/`runPersistenceLoadResult` sind gemeinsam
entweder beide `nullptr` oder beide gesetzt (5.2/5.3). Kein
`RunRecoveryCoordinator`-Parameter (YAGNI, 5.4). Der bestehende
`const IResetCauseSource* = nullptr`-Parameter bleibt unverändert
letzter Parameter mit Default.

**Alle vier neuen Parameter (`configurationService`,
`configurationRecoveryResult`, `runPersistenceCoordinator`,
`runPersistenceLoadResult`) werden ausschließlich transient innerhalb
von `begin()` gelesen, um `SafetyCoreInput` zu befüllen (5.4) — keiner
wird als Member gespeichert.** Für #119 gibt es keinen nachweislichen
laufenden Zugriff nach `begin()`: `update()` bleibt unverändert ein
Platzhalter (Nicht-Scope: Sensor-/UI-/WLAN-/Aktorintegration, Abschnitt
7); Schritt 2 verifiziert reale Reboot-/Recovery-Zyklen, nicht
laufenden Zugriff aus einem aktiven Applikations-Loop heraus. Sollte ein
späteres Issue laufenden Zugriff auf `ConfigurationService` oder
`RunPersistenceCoordinator` demonstrierbar brauchen, wird die Dependency
dann eingeführt — nicht jetzt mit "künftige Issues brauchen das später"
begründet (dieselbe Regel wie für `RunRecoveryCoordinator`, 5.4).
`FermentationApplication` erhält damit für #119 **keine neuen Member**;
sie bleibt bei `platformServices_` und `safetyCore_`.

Ownership/Lifetime (jedes Objekt genau einmal klassifiziert):

| Objekt | Owner | Lifetime | Referenziert von | Nach `begin()` noch benötigt? |
|---|---|---|---|---|
| `NvsOwningContext` | `app_main()` | Gesamte Nutzungsdauer aller vom Store abhängigen Objekte / im aktuellen Composition Root Prozesslebenszeit (4.3) | Composition Root (liefert `store()`) | **Ja** — als alleiniger Lifetime-Owner des `NvsStateStore` und der initialisierten Partition (Destruktor zerstört Store, deinitialisiert danach Partition); muss jeden `IStateStore`-Consumer überleben, auch wenn der Context selbst nicht erneut aktiv abgefragt wird |
| `DevicePlatform` | `app_main()` | Prozesslebenszeit | Composition Root; `FermentationApplication` über `IPlatformServices&` (bestehend, unverändert) | Ja — bestehendes, unverändertes Muster (`platformServices_`) |
| `EspResetCauseSource` | `app_main()` | Prozesslebenszeit | Composition Root; einmalig an `begin(...)` übergeben (bestehend, unverändert) | Nein — bestehendes Muster: nur beim `begin()`-Aufruf gelesen |
| `EspTimeZoneResolver` | `app_main()` | Prozesslebenszeit | `ConfigurationGraphStore`, `ConfigurationService` (Konstruktorreferenz) | Nein von `FermentationApplication`; indirekt weiter von `ConfigurationGraphStore`/`ConfigurationService` gehalten |
| `ConfigurationMutationCoordinator` | `app_main()` | Prozesslebenszeit | `ConfigurationService`, `ConfigurationRecoveryService` | Nein von `FermentationApplication` |
| `ConfigurationBootstrapStore` | `app_main()` | Prozesslebenszeit | `ConfigurationRecoveryService` | Nein von `FermentationApplication` |
| `ConfigurationGraphStore` | `app_main()` | Prozesslebenszeit | `ConfigurationService`, `ConfigurationRecoveryService` | Nein von `FermentationApplication` |
| `ConfigurationService` | `app_main()` | Prozesslebenszeit | `ConfigurationRecoveryService` (Konstruktorreferenz); `begin(...)` erhält sie transient als Parameter | Nein als Member — nur transient während `begin()` gelesen (`mode()`, 5.4); kein nachweislicher laufender Zugriff in #119 |
| `ConfigurationRecoveryService` | `app_main()` (`std::unique_ptr`) | Prozesslebenszeit | Composition Root | Nein von `FermentationApplication`; Composition Root behält ihn (spätere Factory-Reset-Auslösung ist kein #119-Scope) |
| `std::optional<RunPersistenceCoordinator>` | `app_main()` | Prozesslebenszeit, falls befüllt | `begin(...)` erhält Zeiger transient | Nein als Member — nur transient während `begin()` gelesen; kein nachweislicher laufender Zugriff in #119 |
| `std::optional<RunPersistenceLoadResult>` | `app_main()` | Mindestens bis `begin()` zurückkehrt | `begin(...)` erhält Zeiger transient | Nein — reiner Bootbefund, nur zur Projektion |
| `FermentationApplication` | `app_main()` | Prozesslebenszeit | — | — |

`ConfigurationBootstrapStore`, `ConfigurationGraphStore`,
`ConfigurationMutationCoordinator` werden von `FermentationApplication`
**nicht** referenziert — sie sind reine interne Kollaborateure von
`ConfigurationService`/`ConfigurationRecoveryService` (KISS-Prüfung,
5.6) und nur intern friend-referenziert, nie von außen aufgerufen.
Keine widersprüchliche Aussage zwischen Bootreihenfolge (5.2), Signatur
(oben) und dieser Tabelle.

**R1.1-Korrektur dieser Tabelle:** Die Zeilen `std::optional<RunPersistenceCoordinator>`
und `std::optional<RunPersistenceLoadResult>` beschreiben den realen,
ownerreviewten Schritt-1-Stand (`app_main()`-Stack-Storage). Dieser Stand
ist real WDT-fehlgeschlagen (Abschnitt 13) und wird durch Schritt 1.1
(Abschnitt 14/15) auf `std::unique_ptr<RunPersistenceCoordinator>` bzw.
`std::unique_ptr<RunPersistenceLoadResult>` korrigiert — Owner bleibt
weiterhin `app_main()`, Referenzierung durch `begin(...)` bleibt
transient/rohe Zeiger, nur der Storage-Ort wechselt von Stack auf Heap.
Alle übrigen Zeilen dieser Tabelle bleiben unverändert gültig.

### 5.6 KISS-Prüfung der `FermentationApplication`-Schnittstelle

Nicht automatisch jedes Recoveryobjekt als eigener `begin()`-Parameter:
geprüft wurde für jedes Objekt aus Abschnitt 4, ob
`FermentationApplication` es nach `begin()` tatsächlich braucht.

| Objekt | Nach `begin()` gebraucht? | Grund |
|---|---|---|
| `ConfigurationService` | Nein als Member | Kein nachweislicher laufender Zugriff in #119 (`update()` bleibt Platzhalter, Sensor-/UI-/Aktorintegration ist Nicht-Scope); nur transient während `begin()` für `mode()`/`acquireRuntime()` gelesen (5.4). Nicht mit "künftige Issues brauchen das später" begründet — wird bei nachgewiesenem Bedarf dann eingeführt |
| `ConfigurationRecoveryResult` | Nein | Einmaliger Bootbefund, nicht laufend gültig; wird nur einmal in `SafetyCoreInput` projiziert |
| `ConfigurationBootstrapStore`/`ConfigurationGraphStore`/`ConfigurationMutationCoordinator` | Nein | Reine interne Kollaborateure von `ConfigurationService`/`ConfigurationRecoveryService`, nie direkt von außen aufgerufen |
| `ConfigurationRecoveryService` | Nein (für #119) | `beginAuthorizedFactoryReset()` hat in #119 keinen Aufrufer; Composition Root hält ihn ohnehin lebendig, falls ein späteres Issue ihn braucht — keine Signaturänderung nötig, da nicht über `FermentationApplication` geführt |
| `RunPersistenceCoordinator` | Nein als Member | Kein nachweislicher laufender Zugriff in #119 (dieselbe Begründung wie `ConfigurationService`); nur transient während `begin()` für `state()`/Projektion gelesen (5.4) |
| `RunPersistenceLoadResult` | Nein | Einmaliger Bootbefund, nur zur Projektion |
| `RunRecoveryCoordinator` | **Gar nicht konstruiert** (YAGNI, 5.4) | `activate(...)` verlangt reale Sensorevidenz, die #119 nicht liefert; keine Instanz "auf Vorrat" für ein späteres Issue |

Ergebnis: die in 5.5 festgelegte Signatur mit vier rein transienten
Referenz-/Zeigerparametern (keiner als Member gespeichert) bleibt nach
dieser Prüfung die kleinste Lösung. Kein neuer Container, kein Service
Locator, kein DI-Framework, keine neue generische Application-API.

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

**Status: `COMPLETED`, `STEP_1_OWNER_REVIEW=PASS`.**

### Schritt 1.1 (R1.1, neu) – Composition-Root-Stack-Restrukturierung

Ausschließlich im Composition Root (`main/app_main.cpp`), gemäß Abschnitt
14/15: `std::optional<RunPersistenceCoordinator>` und
`std::optional<RunPersistenceLoadResult>` durch heapbesitzende
`std::unique_ptr<...>` mit Boot-only-Lebenszeit ersetzen. Keine
Signaturänderung an `FermentationApplication::begin(...)` nötig (sie
nimmt bereits rohe Zeiger, Abschnitt 5.5) und keine Änderung an
`RunPersistenceCoordinator` selbst. Details, Zielarchitektur, Boot-only-
Lebenszeit und Kriterien: Abschnitt 13–18.

Gate: `APP_MAIN_ENTRY_FRAME_AFTER < CONFIGURED_MAIN_TASK_STACK` (Abschnitt
16, statisch nach dem Build gemessen); Architekturgrenzen weiterhin
`PASS`; kein Verhaltensunterschied in Configuration-/Run-Recovery,
Safety-Projektion, Wireformaten.

**Status: `NOT_STARTED`. Eigenes Owner-Gate, unabhängig von Schritt 1.**

### Schritt 2 – gezielte Verifikation (wiederholt nach Schritt 1.1)

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

**Status: erste Ausführung (unter Schritt-1-Stand, ohne Schritt 1.1)
real durchgeführt, `STEP_2_VERIFICATION=FAILED`
(`REAL_NVS_RECOVERY_GATE=FAIL`, Ursache: Abschnitt 13). Diese Runde
(PLAN ONLY) startet keinen neuen Schritt-2-Lauf. Nach Umsetzung von
Schritt 1.1 und eigener Freigabe wird Schritt 2 vollständig wiederholt,
nicht nur die zuvor blockierten Teilgates.**

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

## 12. Abnahmekriterien R1.0 (historisch, weiterhin gültig)

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
| `RunPersistenceLoadResult`-Lebenszeit korrigiert (app_main-Scope statt Blockscope, 5.2) | `PASS` |
| `RunRecoveryCoordinator` per YAGNI aus Composition/Signatur entfernt (5.4/5.5/5.6) | `PASS` |
| Ownership/Lifetime-Tabelle vollständig, widerspruchsfrei (`ConfigurationService` ergänzt, 5.5) | `PASS` |
| `ITimeZoneResolver`-Begründung auf Port-/Issue-55-Vertrag als normative Quelle korrigiert (5.1) | `PASS` |
| App-Katalog vs. Plattform-Support als getrennte Verantwortungen dokumentiert (5.1) | `PASS` |
| `NvsOwningContext`-Lifetime korrekt als Lifetime-Owner des Store/der Partition dokumentiert, `store()` als nicht besitzend klargestellt (5.2/5.5) | `PASS` |
| Architekturentscheidung getroffen und begründet (Abschnitt 5) | `PASS` |
| `MATERIAL_ARCHITECTURE_DECISION_OPEN` | `NO` |
| Akzeptanzkriterium definiert | `PASS` |
| Neue Produktions-/Test-/Compositionimplementation (Schritt 1) | `COMPLETED`, `STEP_1_OWNER_REVIEW=PASS` |
| Reale Boardverifikation (Schritt 2) | real durchgeführt, `STEP_2_VERIFICATION=FAILED` (Ursache: Abschnitt 13) |
| Exakte R1.0-Planfreigabe | `APPROVED` (`5effae0d8b3ec82abe5e3864082b1ce237f8f03f`) |

R1.0 bleibt damit strukturell/architektonisch weiterhin gültig
(Abschnitt 1–12); nur der reale Schritt-2-Befund macht die in Abschnitt
13–19 festgelegte R1.1-Korrektur nötig.

## 13. R1.1 – Root Cause (real verifiziert, keine Spekulation)

`nvs_flash_init_partition("state_store")` in `NvsOwningContext::create()`
existierte bereits unverändert auf #118 (`fd7e4e3`) und war dort real
mehrfach bootfähig (Abschnitt 1). #119 fügt vor diesem Aufruf **keine**
Recovery- oder Applicationlogik ein — `NvsOwningContext::create()` ist
nach wie vor der allererste Aufruf in `app_main()`. Der WDT tritt also
zwar sichtbar bei `nvs_flash_init_partition` auf, ist dort aber nicht
verursacht.

Realer Befund (PR #120, A/B auf Hardware, byteidentischer
`state_store`-Inhalt/Partitionstabelle/sdkconfig zwischen #118 und #120):

```text
#118 (fd7e4e3) app_main()-Entry-Frame:  112 Byte
#120 (a72cf14) app_main()-Entry-Frame:  16.432 Byte
CONFIG_ESP_MAIN_TASK_STACK_SIZE (beide identisch): 3.584 Byte
Realer main_task-Stackverbrauch am Panic-SP: 17.616 Byte (~4,9x Budget)

#118 gleicher Startzustand: 3/3 PASS
#120 gleicher Startzustand: WDT reproduzierbar (9/9 über alle Varianten)
#120 mit komplett gelöschtem state_store: 3/3 WDT
```

Der überlaufende main_task-Stack läuft laut Linker-Map durch den
gesamten Heap hindurch bis in den statisch verlinkten `.dram0.data`-
Bereich (u. a. `spi_flash`-, `esp_ipc_isr`-, `panic`-, `log`-Globals) —
das erklärt den beobachteten Interrupt-WDT/Spinlockzustand, ohne dass
NVS-Inhalt, NVS-Backend oder Watchdog-Konfiguration ursächlich beteiligt
sind.

```text
CAUSE_CLASS=#120_BINARY_OR_INTEGRATION_DEPENDENT
ROOT_CAUSE_DIRECTION=NOT_CONTENT_ONLY
```

**Ausdrücklich nicht Root Cause:** NVS-Inhalt; Callback 12;
NVS-Partitionsgröße; NVS-Backend; Watchdog-Timeout-Konfiguration. Keine
dieser Stellschrauben wird in R1.1 verändert.

## 14. R1.1 – Kleinste Zielarchitektur

Reale, am aktuellen `#120`-Release-ELF mit Debuginfo gemessene Größen
(Xtensa-/ESP32-Target, nicht Host/native — `sizeof` auf dem Host wäre
wegen abweichender Zeiger-/Alignment-Breite kein gültiger Proxy):

```text
RUN_PERSISTENCE_SNAPSHOT_SIZE=3792
RUN_PERSISTENCE_RAW_RECORD_SIZE=3840
RUN_PERSISTENCE_COORDINATOR_SIZE=8336
RUN_PERSISTENCE_LOAD_RESULT_SIZE=3808
CONFIGURATION_SERVICE_SIZE=232
```

`RunPersistenceCoordinator` (8.336 Byte) besteht überwiegend aus
`std::optional<RunPersistenceRawRecord> slots_[2]`
(`lib/fermentation_app/src/run_persistence_coordinator.hpp:281`, zwei
Slots à 3.840 Byte); `RunPersistenceRawRecord` enthält wiederum einen
vollständigen `RunPersistenceSnapshot` by-value
(`run_persistence_contract.hpp:130`). Diese Messung bestätigt den in
Abschnitt 4 des Auftrags erwarteten Befund: `ConfigurationService`
(232 Byte) ist klein — ihre größeren Zustände liegen bereits hinter
`std::unique_ptr`/`std::shared_ptr`-Indirektion
(`configuration_service.hpp`) — und ist **kein** Ziel dieser Korrektur.

Die beiden `app_main()`-Stack-Locals `std::optional<RunPersistenceCoordinator>`
und `std::optional<RunPersistenceLoadResult>` tragen zusammen real
gemessen 12.160 Byte (`sizeof(std::optional<RunPersistenceCoordinator>)`
= 8.344, `sizeof(std::optional<RunPersistenceLoadResult>)` = 3.816) von
16.432 Byte Gesamtframe — die mit Abstand größten Einzelbeiträge; alle
übrigen benannten `app_main()`-Locals (`DevicePlatform`,
`FermentationApplication`, `EspTimeZoneResolver`,
`ConfigurationRecoveryResult`, `ConfigurationMutationCoordinator`,
`ConfigurationBootstrapStore`, `ConfigurationGraphStore`) sind zusammen
real unter 100 Byte groß.

**Festgelegte Variante (genau eine, keine Alternative offen):**

In `main/app_main.cpp` werden ausschließlich

```cpp
std::optional<RunPersistenceCoordinator> runPersistenceCoordinator;
std::optional<RunPersistenceLoadResult> runPersistenceLoadResult;
```

durch

```cpp
std::unique_ptr<fermentation::RunPersistenceCoordinator> runPersistenceCoordinator;
std::unique_ptr<fermentation::RunPersistenceLoadResult> runPersistenceLoadResult;
```

ersetzt. Konstruktion weiterhin bedingt durch dasselbe
Recovery-Status-Gate wie heute (Abschnitt 5.2/5.4, unverändert):

```cpp
if (/* wie bisher: RuntimeReady | FactoryInitializationCompleted |
       FactoryResetCompleted, gefolgt von RuntimeLeaseGranted */) {
    runPersistenceCoordinator = std::make_unique<fermentation::RunPersistenceCoordinator>(
        store, runtimeRead.lease.get().storageEpoch(),
        fermentation::RunCheckpointSchedule{});
}
if (runPersistenceCoordinator != nullptr) {
    runPersistenceLoadResult.reset(new fermentation::RunPersistenceLoadResult(
        runPersistenceCoordinator->loadAndInitialize()));
}
```

**Warum `new T(...)` statt `std::make_unique<T>(...)` für
`runPersistenceLoadResult` (kein Stilfehler, technisch notwendig):**
Der reale Disassembly des aktuellen #120-Release-ELF
(`app_main`, Aufruf bei `0x400d8853`) zeigt, dass
`loadAndInitialize()` seinen Rückgabewert (3.808 Byte, By-Value-Return
über verstecktes Rückgabepointer-Argument) heute in einen **separaten**
Temporärpuffer innerhalb des `app_main()`-Frames schreibt
(`a1+16+0x3108`) und ihn danach per `std::optional::emplace(...)` in die
eigentliche Optional-Speicherung (`a1+16+0x2210`) verschiebt — zwei
zeitlich überlappende ~3.800-Byte-Bereiche für ein einziges Objekt.
`std::make_unique<T>(args...)` bindet seine Argumente über eine
Forwarding-Reference (`Args&&...`) und würde denselben
Temporärpuffer-Umweg erneut einführen. `new T(prvalue_of_T)` dagegen ist
eine Direktinitialisierung eines neuen Objekts aus einem Prvalue
**desselben** Typs; seit C++17 ist das erzwungene Copy-Elision (kein
Temporärobjekt entsteht) — eine Sprachregel, keine
optimierungsabhängige Heuristik, sie gilt unabhängig von `-Og`/`-O2`.
`loadAndInitialize()`s Rückgabewert wird damit direkt in den neu
allokierten Heap-Speicher geschrieben, ohne Zwischenkopie auf dem Stack.
`RunPersistenceCoordinator` braucht diese Sonderbehandlung nicht: sein
Konstruktor nimmt `(IStateStore&, StorageEpoch, RunCheckpointSchedule)`
entgegen, keinen Prvalue desselben Typs — `std::make_unique<...>(store,
epoch, schedule)` konstruiert direkt in-place, kein Temporärobjekt
möglich, da `RunPersistenceCoordinator` ohnehin nicht kopier-/
verschiebbar ist (gelöschte Kopier-/Move-Konstruktoren,
`run_persistence_coordinator.hpp:180-184`) und folglich nie als
Rückgabewert einer Funktion auftreten kann.

Der Aufruf von `application.begin(...)` ändert sich nicht inhaltlich:
`runPersistenceCoordinator.get()` statt `&*runPersistenceCoordinator`
(gleiche Zeigersemantik), ebenso für `runPersistenceLoadResult`. Keine
Signaturänderung an `FermentationApplication::begin(...)` (Abschnitt 5.5
bleibt unverändert gültig — sie nimmt bereits rohe Zeiger, kein
Ownership-Transfer). Keine Änderung an `RunPersistenceCoordinator`,
`RunPersistenceLoadResult`, `RunPersistenceSnapshot` oder
`RunPersistenceRawRecord` selbst; der Fix ist ausschließlich
Ownership/Ressourcenmanagement im Composition Root.

Kein `static`, kein globales Singleton, kein Service Locator, kein
DI-Container, keine neue `device_platform`-API. Ein privater,
`app_main.cpp`-lokaler Boot-Helper (z. B. eine kleine freie Funktion, die
die obige bedingte Konstruktion kapselt) ist zulässig, wenn er die
Boot-only-Lebenszeit klar ausdrückt — er ist keine neue Architektur/API
und bleibt intern zu `main/app_main.cpp`.

**Verbindlich für einen solchen Helper:** Er darf `RunPersistenceLoadResult`
ausschließlich über `std::unique_ptr<RunPersistenceLoadResult>`
zurückgeben (Zeigergröße als Rückgabewert), **nie** `RunPersistenceLoadResult`
by-value. Ein By-Value-Rückgabetyp würde denselben versteckten
Rückgabepuffer erneut im Frame der aufrufenden Funktion erzeugen (siehe
Begründung oben) und die `new T(...)`-Elision zunichtemachen, egal wie
der Helper intern konstruiert. Das `new T(...)`-Konstrukt selbst gehört
in den Helper hinein, nicht an die Aufrufstelle verschoben.

## 15. R1.1 – Boot-only-Lebenszeit

Real geprüft (`lib/fermentation_app/src/fermentation_application.hpp`):
`FermentationApplication` hat ausschließlich die Member
`platformServices_` und `safetyCore_`. Weder `runPersistenceCoordinator`
noch `runPersistenceLoadResult` (noch `configurationService`,
`configurationRecoveryResult`) werden von `begin(...)` als Member
gespeichert — sie sind bereits heute rein transient (Abschnitt 5.5,
unverändert bestätigt). Die R1.1-Korrektur ändert damit **nur den
Storage-Ort** (Stack -> Heap), nicht die Lebenszeit- oder
Zugriffssemantik.

Freigabe der Boot-transienten Heapobjekte in `app_main()`, sobald kein
realer Consumer sie mehr besitzt — spätestens direkt nach dem
`application.begin(...)`-Aufruf:

```cpp
const bool applicationStarted = application.begin(
    platform, configurationService, configurationRecoveryResult,
    runPersistenceCoordinator.get(), runPersistenceLoadResult.get(),
    &resetCauseSource);

runPersistenceLoadResult.reset();
runPersistenceCoordinator.reset();
```

`NvsOwningContext`-Vertrag bleibt unverändert: `store()` bleibt
nicht-besitzend; der Context bleibt alleiniger Owner von
`NvsStateStore` und Partition (Abschnitt 5.5, unverändert) — er wird
durch Schritt 1.1 nicht berührt, da `RunPersistenceCoordinator` den
Store nur referenziert, nicht besitzt.

## 16. R1.1 – Main-Task-Stack-Kriterium

Primäre R1.1-Korrektur bleibt bei unverändertem

```text
CONFIG_ESP_MAIN_TASK_STACK_SIZE=3584
```

Keine sdkconfig-Änderung in dieser Planrunde und nicht als erster Schritt
der Umsetzung. Nach der Umsetzung von Schritt 1.1 ist zwingend real zu
messen (nicht Teil dieser PLAN-ONLY-Runde):

```text
APP_MAIN_ENTRY_FRAME_BEFORE=16432
APP_MAIN_ENTRY_FRAME_AFTER=<measured>
CONFIGURED_MAIN_TASK_STACK=3584
```

**Überschlägige Restrechnung (Schätzung, kein Beweis, ersetzt die
Pflichtmessung nicht):** Die beiden entfernten Stack-Locals allein tragen
real gemessen 12.160 Byte
(`sizeof(std::optional<RunPersistenceCoordinator>)` = 8.344,
`sizeof(std::optional<RunPersistenceLoadResult>)` = 3.816); ohne die
`new T(...)`-Korrektur aus Abschnitt 14 bliebe damit ein Rest von
16.432 − 12.160 = 4.272 Byte — 688 Byte **über** dem 3.584-Byte-Budget.
Der reale Disassembly-Befund aus Abschnitt 14 zeigt aber eine dritte,
bislang nicht mitgezählte Region: der `loadAndInitialize()`-Rückgabewert
belegt zusätzlich zur Optional-Speicherung (`a1+16+0x2210`) einen
separaten Temporärpuffer bei `a1+16+0x3108` — 0xEF8 = 3.832 Byte
Abstand, konsistent mit `sizeof` 3.808 plus Alignment. Vollständig
gerechnet: 8.344 (Coordinator) + 3.816 (Optional) + ~3.808
(Rückgabepuffer) = 15.968 von 16.432 Byte, d. h. nur ~464 Byte
verbleiben für alle übrigen benannten Locals (< 100 Byte, Abschnitt 14)
und die Register-Save-Fläche. Die `new T(...)`-Korrektur entfernt genau
diesen dritten Puffer (garantierte Elision, Abschnitt 14) zusätzlich zu
den beiden Locals — die realistische Restgröße liegt damit eher bei
~464 Byte als bei 4.272 Byte, **falls** der Boot-Helper exakt wie in
Abschnitt 14 festgelegt implementiert wird (`std::unique_ptr`-Rückgabe,
nicht `RunPersistenceLoadResult`-by-value). `-Og`-Frames sind trotzdem
nicht strikt additiv/subtraktiv über Codeänderungen hinweg (Spill-/
Temporärflächen werden neu verteilt, nicht nur gelöscht) — beide Zahlen
(4.272 ohne, ~464 mit der `new T(...)`-Korrektur) sind Schätzungen; die
tatsächliche `APP_MAIN_ENTRY_FRAME_AFTER`-Zahl kann nur durch einen
echten Build mit Debuginfo (wie in Abschnitt 13/14 für die Vorher-Werte
verwendet) bestimmt werden. Diese Messung ist ausdrücklich **nicht**
Teil dieser PLAN-ONLY-Runde (Abschnitt 6 des Auftrags); ein nicht
commiteter Diagnose-Build in einem separaten
Arbeitsbaum (analog zum `fd7e4e3`-Baseline-Vorgehen aus PR #120) ist eine
Option, die der Owner nach Freigabe dieser Datei separat autorisieren
kann, bevor Schritt 1.1 committet wird. Sollte das Kriterium nach realer
Umsetzung nicht erfüllt sein, ist das ein neuer, eigener Befund für den
Owner (siehe unten) — keine Vorwegnahme in dieser Planrunde.

Notwendiges statisches Kriterium:

```text
APP_MAIN_ENTRY_FRAME_AFTER < CONFIGURED_MAIN_TASK_STACK
```

Keine erfundene Prozentreserve. Sollte dieses Kriterium nach Schritt 1.1
nicht erfüllt sein, ist das ein neuer, eigener Befund für den Owner —
kein automatischer Rückfall auf `MAIN_TASK_STACK_BLANKET_INCREASE`.

Erst danach real:

```text
FULL_RUNTIME_STACK_HEADROOM
RUNTIME_STACK_HWM_BYTES=<measured under real recovery workload>
```

Nur diese reale HWM (nicht der Entry-Frame-Wert) darf einen separaten,
gemessenen Stackgrößenbedarf begründen — und nur, nachdem die
strukturelle Korrektur bereits umgesetzt ist (Abschnitt 1 der
Owner-Entscheidung, unverändert).

## 17. R1.1 – Heapwirkung

Da `RunPersistenceCoordinator` (8.336 Byte) und
`RunPersistenceLoadResult` (bis zu 3.808 Byte) vom Stack auf den Heap
verschoben werden, ist für die spätere Umsetzung real zu messen (kein
Zielwert wird hier vorweggenommen):

```text
BOOT_HEAP_BEFORE_COMPOSITION
BOOT_HEAP_MIN_DURING_COMPOSITION
BOOT_HEAP_AFTER_BOOT_TRANSIENTS_RELEASED
```

Kein eigener Allocator, keine vorsorgliche neue Speicherplattform. Die
Freigabe nach Abschnitt 15 (`.reset()` direkt nach `begin()`) hält die
Heap-Spitzenlast auf die Dauer der Composition/Recovery begrenzt statt
für die gesamte Prozesslaufzeit.

## 18. R1.1 – Recovery-/Safety-Vertrag unverändert

Durch Schritt 1.1 nicht verändert: `StorageEpoch`-Quelle;
Configuration-Recovery-Reihenfolge (Abschnitt 5.2); reale
`SafetyCoreInput`-Projektion (Abschnitt 5.4);
`RunPersistenceCoordinator::loadAndInitialize()` und alle übrigen
`RunPersistenceCoordinator`-Methoden; SafetyCore-Projektion;
Fail-closed-Semantik (Abschnitt 10); `EspTimeZoneResolver` (Abschnitt
5.1); Callback-12-Vertrag; Run-/Configuration-Wireformate;
App-/Platform-Abhängigkeitsrichtung (Abschnitt 2/3); NVS-Partition und
-Backend (Abschnitt 9 unverändert). Der Fix ist ausschließlich
Ownership-/Ressourcenmanagement im Composition Root, keine neue
Recoverysemantik.

## 19. R1.1 – Schritt-2-Verifikation nach Umsetzung (Vertrag, nicht Teil dieser Runde)

Nach eigener Freigabe der Schritt-1.1-Umsetzung nimmt Schritt 2
(Abschnitt 8) den bestehenden Vertrag vollständig wieder auf, in dieser
Reihenfolge:

1. gezielte Build-/Architektur-/Tests (Abschnitt 8, unverändert);
2. realer normaler Produktboot inkl. `APP_MAIN_ENTRY_FRAME_AFTER`-Kriterium
   (Abschnitt 16);
3. `REAL_NVS_RECOVERY_GATE`;
4. `CALLBACK_12_REAL_NVS_PRODUCT_GATE`;
5. reale `FULL_RUNTIME_STACK_HEADROOM`/`RUNTIME_STACK_HWM_BYTES`
   (Abschnitt 16, unter realem Recovery-Workload);
6. erst bei stabilem Produktpfad `MANUAL_POWER_CUT_GATE` (Abschnitt 9).

Keine Aktortests (unverändert, Abschnitt 7/9).

## 20. Statussemantik bis zur Schritt-1.1-Freigabe

Bis zu einer ownerfreigegebenen Umsetzung bleibt korrekt:

```text
REAL_NVS_RECOVERY_GATE=FAIL
FULL_RUNTIME_STACK_HEADROOM=BLOCKED
RUNTIME_STACK_HWM_BYTES=NOT_MEASURED_BOOT_WDT_BEFORE_RECOVERY
PRODUCT_BOOT_WDT=FAIL
PRODUCT_RECOVERY_MISMATCH=NOT_EVALUATED_BOOT_WDT_BEFORE_RECOVERY
```

## 21. Abnahmekriterien R1.1 (diese Planrunde, PLAN ONLY)

| Nachweis | Status |
|---|---|
| Root Cause real verifiziert und im Plan festgehalten, nicht spekulativ (Abschnitt 13) | `PASS` |
| Ownerentscheidung `BOOT_STACK_FIX_DIRECTION=RESTRUCTURE_COMPOSITION_ROOT` übernommen (Abschnitt 1 des Auftrags) | `PASS` |
| `MAIN_TASK_STACK_BLANKET_INCREASE` als primärer Fix zurückgewiesen und nicht Teil dieser Revision | `PASS` |
| Reale Zielgrößen im Release-Target gemessen (nicht Host/native), genau die angefragten fünf Typen (Abschnitt 14) | `PASS` |
| Genau eine konkrete Zielvariante festgelegt, keine Alternativen offen (Abschnitt 14) | `PASS` |
| Boot-only-Lebenszeit technisch abgebildet, `FermentationApplication` weiterhin ohne neue Member (Abschnitt 15) | `PASS` |
| Keine Signaturänderung an `FermentationApplication::begin(...)` nötig, real anhand des Headers geprüft (Abschnitt 15) | `PASS` |
| `RunPersistenceCoordinator`/`RunPersistenceLoadResult`/Wireformate intern unverändert (Abschnitt 14/18) | `PASS` |
| Main-Task-Stack-Kriterium ohne erfundene Reserve definiert (Abschnitt 16) | `PASS` |
| Heapwirkung als Messpunkt (kein Zielwert) vorgesehen (Abschnitt 17) | `PASS` |
| Recovery-/Safety-Vertrag als unverändert bestätigt (Abschnitt 18) | `PASS` |
| Schritt-2-Vertrag nach Umsetzung vollständig wieder aufgenommen (Abschnitt 19) | `PASS` |
| `MATERIAL_ARCHITECTURE_DECISION_OPEN` | `NO` |
| Kein Service Locator, DI-Container, Singleton, `static`, neue Device-Platform-API | `PASS` |
| Neue Produktions-/Testimplementation in dieser Runde | `NOT_RUN` / nicht enthalten (PLAN ONLY) |
| `ROADMAP_SYNC` (`docs/ROADMAP.md`, gleicher Commit wie diese Datei) | `PASS` |
| `PR_120_BODY_SYNC` | wird nach diesem Commit per PR-Bearbeitung/-Kommentar nachgezogen, siehe SESSION HANDOVER |
| `ISSUE_119_SYNC` | wird nach diesem Commit nachgezogen, siehe SESSION HANDOVER |
| `CURRENT_HANDOVER` | wird nach diesem Commit als PR-Kommentar erstellt |
| Exakte R1.1-Planfreigabe | `BLOCKED` bis Ownerentscheidung |

STOP – Owner Full Review der exakten R1.1-Plan-SHA. Keine
Schritt-1.1-Implementation vor neuer Ownerfreigabe.
