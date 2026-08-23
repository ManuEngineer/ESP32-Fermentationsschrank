# Issue #119 – produktive StateStore-/Application-Composition anbinden und real verifizieren (R1.0)

## Status, Ziel und Owner-Gate

Diese Datei ist die vollständige, eigenständig ausführbare kanonische
Planrevision R1.0 für Issue #119. Sie enthält **keine
Produktionsimplementation**. Bis zur ausdrücklichen Freigabe der exakten
Commit-SHA dieser Datei gilt:

```text
COMPOSITION_PLAN_PENDING_R1_0_OWNER_APPROVAL
```

Die Freigabe autorisiert noch keinen Umsetzungsslice; jeder Slice (A–D)
erhält ein eigenes Owner-Gate.

```text
BASE_PR=#118
BASE_SHA=903136f641712c2144eba6ffa8eb289dfb472af1
IMPLEMENTATION=NOT_STARTED
```

Keine #118-Implementation wird kopiert oder cherry-gepickt.

## 1. Herkunft und Abgrenzung

Issue #119 übernimmt das in Issue #90 R5.9 Slice 7 (owner-freigegeben,
real ausgeführt) entdeckte Composition-Gate. #90/PR #118 bleibt mit R6.0
(`docs/tasks/issue-90-clean-restart-plan-r6.0.md`) eine rein
anwendungsneutrale StateStore-/NVS-Plattformfähigkeit.

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

## 4. Bereits vorhandene Inventur (Ausgangslage für Slice A)

Am `#118`-HEAD real geprüft (siehe R6.0 Abschnitt 3):

- `IStateStore` ist bereits vollständig anwendungsneutral.
- `NvsStateStore`/`NvsStateStoreConfig` tragen bereits den Kommentar, dass
  `"state_store"`/`"fermentation"` Composition-Root-Konfiguration sind,
  keine Plattformdefaults.
- Bereits vorhandene Fachkomponenten akzeptieren `IStateStore&` bereits
  direkt als Konstruktorparameter, ohne Umweg über eine
  Plattform-Applikations-Kopplung:
  - `ConfigurationBootstrapStore(IStateStore&)`
  - `ConfigurationGraphStore(IStateStore&, const ITimeZoneResolver&)`
  - `ConfigurationService(ConfigurationMutationCoordinator&, ConfigurationGraphStore&, const ITimeZoneResolver&)`
  - `RunPersistenceCoordinator(IStateStore&, …)`
  - `ConfigurationRecoveryService::create(IStateStore&, ConfigurationBootstrapStore&, ConfigurationGraphStore&, ConfigurationService&, ConfigurationMutationCoordinator&)`
  - `RunRecoveryCoordinator(RunPersistenceCoordinator&)`
- `FermentationApplication::begin(IPlatformServices&, const IResetCauseSource*)`
  hält aktuell nur `platformServices_` (Zeiger) und `safetyCore_`
  (Member); keine der obigen Recoverykomponenten ist Mitglied.
- `IPlatformServices` hat aktuell genau eine Methode (`ready()`),
  implementiert ausschließlich von `DevicePlatform`.

Diese Inventur ist die Ausgangslage für Slice A, nicht deren Ergebnis;
Slice A muss sie am dann aktuellen HEAD erneut real bestätigen, bevor
irgendetwas verdrahtet wird.

## 5. Offene Entwurfsfrage (nicht vorentschieden)

Der Plan entscheidet **nicht**, ob:

1. `IStateStore` als separate Dependency an `FermentationApplication`
   bzw. deren owning Recovery-Kontext gegeben wird (analog zum
   bestehenden Muster `IResetCauseSource*` als zusätzlicher
   `begin()`-Parameter), oder
2. `IStateStore` fachlich Teil von `IPlatformServices` wird.

Vorläufiger Befund aus Abschnitt 4 (kein Vorentscheid): Da
`ConfigurationBootstrapStore`, `ConfigurationGraphStore`,
`RunPersistenceCoordinator` und `ConfigurationRecoveryService::create`
bereits alle `IStateStore&` als eigenständigen, expliziten Parameter
entgegennehmen — nicht über eine gebündelte Services-Abstraktion — spricht
das bestehende Vertragsmuster tendenziell für Variante 1 (separate
Dependency, analog `IResetCauseSource*`). `IPlatformServices` um
`IStateStore` zu erweitern wäre eine God-Interface-Erweiterung, für die
Abschnitt 4 keinen belegten Bedarf zeigt. Slice A muss diese Frage anhand
der dann realen Konstruktoren, Owner und Lebenszeiten abschließend
begründen und entscheiden; bis dahin bleibt sie offen.

Kriterien für die Slice-A-Entscheidung: SOLID, KISS, keine
Fermentationsbegriffe in Plattformports, keine unnötige
God-Interface-Erweiterung, native Testbarkeit, eindeutige
Objektlebenszeit, kein globaler Service Locator.

## 6. Keine vorschnelle neue generische Application-API

Kein automatisches Einführen von `IApplication`, `IRecoveryApplication`,
`IApplicationPersistence`, `ApplicationContainer` oder ähnlichem. Zuerst
werden vorhandene Konstruktoren, Owner, Services und Lebenszeiten
inventarisiert (Slice A). Kann der heutige Composition Root mit
bestehenden Verträgen sauber verdrahten, wird direkt verdrahtet — keine
neue Abstraktion. Nur ein konkreter, belegter Kopplungsmangel darf eine
kleine zusätzliche app-neutrale Schnittstelle rechtfertigen, mit
exaktem Bedarfsnachweis im Slice-A-Ergebnis und eigenem Owner-Gate vor
Umsetzung.

## 7. Scope

### In Scope

**Plattform-/Composition-Seite:** bestehende StateStore-Lifetime prüfen;
`NvsStateStore` bleibt im ESP-IDF-Composition-Root besessen;
anwendungsneutrale Übergabe über bestehende oder minimal notwendige
abstrakte Verträge; fail-closed Init/Open/Lifetime; keine konkrete App im
Plattformmodul.

**Aktueller erster Consumer:** die bereits vorhandenen Configuration
Boot/Recovery-, Run-Persistence/Recovery- und SafetyCore-/Gate-
Projektionskomponenten am Fermenter mit dem realen Store über den echten
Produktpfad verbinden. Diese Komponenten bleiben dort, wo ihre fachliche
Eigentümerschaft bereits liegt (`lib/fermentation_app/`); sie werden
nicht wegen Wiederverwendbarkeit nach `device_platform` verschoben.

**Reale Verifikation:** die aus #90 R5.9 Slice 7 übertragenen Gates,
actor-free:

```text
BOARD_IDENTITY_GATE          (bereits real PASS, wird nicht wiederholt,
                               sofern Board/Setup unverändert)
UART_RESET_GATE               (bereits real PASS, dito)
REAL_NVS_RECOVERY_GATE
CALLBACK_12_REAL_NVS_PRODUCT_GATE
FULL_RUNTIME_STACK_HEADROOM  (reale HWM unter dem dann existierenden
                               Recovery-Workload; ergänzt, ersetzt nicht,
                               das #90-eigene statische
                               RELEASE_STACK_SCRATCH_GATE)
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

## 8. Owner-gatete Slices

### Slice A – vollständige Composition-Inventur und Entscheidung

Nach Freigabe der exakten R1.0-Plan-SHA und separater Slice-A-Freigabe:
vollständige, am dann aktuellen HEAD erneut real geprüfte Inventur von
`main/app_main.cpp`, `DevicePlatform`, `IPlatformServices`, `IStateStore`,
`NvsStateStore`, `FermentationApplication`, Configuration-Boot/Recovery,
Run-Persistence/Recovery, `SafetyCore`, Objektlebenszeiten,
Bootreihenfolge, Task-/Stack-Besitz. Beantwortet abschließend die in
Abschnitt 5 offene Entwurfsfrage mit Begründung. Kein Code.

Gate: kleinste notwendige Verdrahtung benannt und begründet; belegt, ob
überhaupt eine zusätzliche Schnittstelle nötig ist.

### Slice B – minimale echte Produktcomposition

Plattformadapter im Composition Root; aktuelle konkrete App als
Consumer entsprechend der Slice-A-Entscheidung verdrahtet; keine
Rückwärtsabhängigkeit; keine alternative Diagnose-Composition; actor-free.

Gate: `scripts/check_architecture_boundaries.py` PASS; keine neue
Rückwärtsabhängigkeit; Akzeptanzkriterium aus Abschnitt 3 strukturell
erfüllt.

### Slice C – gezielte Software-/Architekturverifikation

Vorhandene Tests wiederverwenden; nur notwendige neue
Compositiontests; Architekturgrenzen explizit prüfen; beide
ESP-IDF-Profile; Ressourcen nur soweit durch die Composition betroffen
(inkl. Aktualisierung des #90-`RELEASE_STACK_SCRATCH_GATE`-Nachweises,
falls sich der Main-Task-Callgraph durch die Composition ändert).

### Slice D – reales actor-free Boardgate

Die in Abschnitt 7 gelisteten übertragenen realen Gates, actor-free,
inklusive Callback 12, Runtime-Stack-Headroom unter dem realen
Recovery-Workload und mindestens drei realen manuellen
Stromunterbrechungen je ausgewähltem Szenario während aktiver
Schreiblast (analog zum in #90 R5.9 Abschnitt 8/Slice 7 bereits
etablierten Vertrag).

## 9. Fail-closed Regeln (unverändert aus #90 R5.9 übernommen)

Kein `NOT_RUN -> PASS`, kein `BLOCKED -> PASS`, kein unerwarteter
NVS-Zustand als stiller `NoActiveRun`, kein unklarer
Configuration-Zustand als stiller Factory-New, kein unklarer Run als
stiller Discard, kein Fallback als automatisches Resume, kein
Recoverystatus als physische Aktorfreigabe. Der actor-free logische Gate
bleibt bei unvollständiger Evidenz geschlossen.

## 10. Dynamische Statuspflege

Nach jedem Slice werden HEAD, Plan-SHA, Ergebnisstatus, offene Befunde
und der nächste Owner-Schritt im neuen Draft-PR, in Issue #119 und in
`docs/ROADMAP.md` synchronisiert, sowie genau ein aktueller
`SESSION HANDOVER` gepflegt. Issue #90 wird bei Bedarf informativ
verlinkt, aber nicht durch #119-Fortschritt automatisch geschlossen.

## 11. Abnahmekriterien dieser Planrunde

| Nachweis | Status |
|---|---|
| Live-PR-/Issue-/Epic-/Roadmap-Abgleich | `PASS` |
| Architekturinventur (Abschnitt 4) | `PASS` als Planinhalt |
| Entwurfsfrage offen gehalten, nicht vorentschieden | `PASS` |
| Akzeptanzkriterium definiert | `PASS` |
| Neue Produktions-/Test-/Compositionimplementation | `NOT_RUN` / nicht enthalten |
| Reale Boardverifikation | `NOT_RUN` bis Slice D |
| Exakte R1.0-Planfreigabe | `BLOCKED` bis Ownerentscheidung |

STOP nach Freigabe dieser Datei; keine Implementation vor Slice-A-Freigabe.
