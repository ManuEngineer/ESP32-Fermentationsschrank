# Issue #90 – generische StateStore-/NVS-Plattformfähigkeit (R6.0)

## Status, Ziel und Owner-Gate

Diese Datei ist die vollständige, eigenständig ausführbare kanonische
Planrevision R6.0 für Issue #90. Sie ersetzt R5.9
(`docs/tasks/issue-90-clean-restart-plan-r5.9.md @
baf0b2ae04cd42afa75dfa00e21d900116b38bc8`) als neue Ownerfreigabegrundlage,
sobald ihre exakte Commit-SHA freigegeben ist.

Bis zur ausdrücklichen Freigabe der exakten Commit-SHA dieser Datei gilt:

```text
CLEAN_RESTART_PLAN_PENDING_R6_0_OWNER_APPROVAL
```

R6.0 ändert keine technische Quelle von #90. Sie ordnet ausschließlich die
Abgrenzung von #90 gegenüber dem in R5.9 Slice 7 entdeckten
Composition-Gap neu, überträgt das reale Produkt-Composition-Gate an ein
eigenes Integrationsissue (#119) und korrigiert die Statusformulierung.

### Live-verifizierter Stand bei Planerstellung

| Quelle | Live-Stand |
|---|---|
| PR #116 | OPEN, Draft; Base `main`; unverändert seit R5.9 |
| PR #118 | OPEN, Draft; Branch `agent/issue-90-clean-restart-plan-r5.8`; Base `agent/issue-29-esp32-bringup-plan @ 25b41ae75ef8de04576623411f068876427a87cd`; HEAD vor R6.0 `812dabd860d3cf0ec02ee5ffc951a74dec77ebca` |
| Issue #90 | OPEN, `[E5.7] ESP-IDF-NVS-Adapter fuer IStateStore implementieren und verifizieren` |
| Issue #119 | NEU angelegt, OPEN, `[E5.8] Produktive StateStore-/Application-Composition anbinden und real verifizieren`, Epic #7 |
| #71, #73, #74 | CLOSED; NVS-Integration war in allen dreien ausdrücklich Nicht-Scope; nicht wiedereröffnet |
| #106 | OPEN; Aktorplaner-Per-Run-Parameter-/Recovery-Bindung, nicht der allgemeine StateStore-/Application-Composition-Vertrag; nicht zweckentfremdet |
| ADR-013 | accepted; unverändert verbindlich für die Modultrennung |
| `docs/ROADMAP.md` | wird durch diese Planrunde minimal synchronisiert |

```text
SOURCE_OF_TRUTH_CONFLICT: NONE
```

## 1. Warum R6.0

R5.9 Slice 7 (owner-freigegeben, real ausgeführt) fand real:

`FermentationApplication` (`lib/fermentation_app/src/fermentation_application.hpp`)
verdrahtet aktuell nur ein `SafetyCore` — keine Verbindung zu
`ConfigurationRecoveryService`, `RunRecoveryCoordinator`,
`ConfigurationService` oder zum real vorhandenen
`device_platform_esp_idf::NvsStateStore`. Der ESP-IDF-Composition-Root
(`main/app_main.cpp`) besitzt den konkreten Adapter bereits (`NvsOwningContext`
öffnet ihn real), gibt ihn dem Application-/Recovery-Graph aber nicht weiter.

Der reale Befund lautet **nicht** "`NvsStateStore` muss in
`FermentationApplication` eingebaut werden", sondern: der Composition Root
besitzt die Plattformfähigkeit bereits und muss sie dem aktuellen ersten
Consumer über die vorhandenen abstrakten Verträge zur Verfügung stellen.
Das ist ein Composition-Root-Schritt, keine Plattform- oder Adapteränderung.

Diese Unterscheidung ist der Grund für R6.0: #90 bleibt eine generische
Plattformfähigkeit; die reale Verdrahtung mit der konkreten Anwendung und
das darauf aufbauende reale Produkt-Recovery-Boardgate gehören in ein
eigenes Issue (#119), damit #90 nicht rückwirkend an `fermentation_app`
gekoppelt wird.

## 2. Verbindliche Architekturentscheidung (unverändert ADR-013)

```text
konkrete Anwendung
        |
        v
anwendungsneutrale Device-Platform-Vertraege
        |
        v
konkrete ESP-IDF-Adapter
```

Verboten bleibt: `device_platform -> fermentation_app`,
`device_platform_esp_idf -> fermentation_app`,
`NvsStateStore -> FermentationApplication`, ein generischer
Plattform-Default mit Fermentationsbezug. Zulässig und gewollt bleibt:
`main/app_main.cpp` erzeugt konkrete Plattformadapter und die konkrete
aktuelle Anwendung und verdrahtet beide über bestehende abstrakte
Verträge. Eine spätere andere App muss an derselben Stelle statt der
Fermentations-App komponiert werden können.

## 3. Bestätigte Inventur: #90 ist bereits vollständig anwendungsneutral

Am aktuellen `#118`-HEAD real geprüft:

- `device_platform::IStateStore` (`lib/device_platform/src/state_store.hpp`)
  kennt nur `StateStoreReadStatus`/`StateStoreWriteStatus`, Byteschlüssel und
  -werte; keine Fermentations-, Configuration- oder Run-Semantik.
- `device_platform_esp_idf::NvsStateStore`/`NvsStateStoreConfig`
  (`lib/device_platform_esp_idf/src/nvs_state_store.hpp`) tragen bereits den
  Kommentar: *"application-specific defaults; in particular, 'state_store'
  and 'fermentation' belong to the R1 composition root, not to this
  component."* Der Namespace-String `"fermentation"` wird ausschließlich in
  `main/app_main.cpp` als Aufrufargument übergeben, nicht im Adapter
  vorbelegt.
- Bereits vorhandene fachliche Verträge nehmen `IStateStore&` bereits direkt
  als expliziten Konstruktorparameter entgegen, ohne Umweg über eine
  Plattform-Applikations-Kopplung:
  - `ConfigurationBootstrapStore(IStateStore&)`
  - `ConfigurationGraphStore(IStateStore&, const ITimeZoneResolver&)`
  - `RunPersistenceCoordinator(IStateStore&, …)`
  - `ConfigurationRecoveryService::create(IStateStore&, ConfigurationBootstrapStore&, ConfigurationGraphStore&, ConfigurationService&, ConfigurationMutationCoordinator&)`

Damit bestätigt:

```text
device_platform -> fermentation_app dependency = NONE
device_platform_esp_idf -> fermentation_app dependency = NONE
NvsStateStore fermentation-specific defaults = NONE
```

Der fehlende Schritt ist ausschließlich die Instanziierung und Verdrahtung
dieser bereits store-fähigen Fachkomponenten im Composition Root — keine
neue Plattformschnittstelle, kein neuer Adaptervertrag.

## 4. Abgrenzung zu bestehenden Issues

- **#71** (CLOSED, ESP-IDF-6.0.2-Migration) und **#73** (CLOSED,
  ESP-IDF-Laufzeitadapter/Composition-Root-Parität): beide haben NVS
  ausdrücklich ausgeschlossen. Nicht wiedereröffnet.
- **#74** (CLOSED, CI/Ressourcenbaseline/Upgradevertrag): reale
  NVS-Integration war ausdrücklich Nicht-Scope. Nicht wiedereröffnet.
- **#106** (OPEN, Aktorplaner-Per-Run-Parameter-Snapshot und
  Recovery-Bindung): besitzt eine produktive, aber engere
  Recovery-Bindung für Aktorplanerparameter, nicht den allgemeinen
  StateStore-/Application-Composition-Vertrag. Nicht zweckentfremdet.

## 5. #90-Status nach R6.0

Slices 1–6 aus R5.9 bleiben inhaltlich und technisch wie nachgewiesen
gültig; keine ihrer Software-, Oracle-, Capacity-, Build- oder
Artefakt-Scan-Ergebnisse wird durch R6.0 zurückgenommen. Das bisher als
Slice 7 formulierte reale actor-free Produkt-Composition-/Recovery-Gate
wird an Issue #119 übertragen und ist damit kein #90-Abnahmekriterium
mehr.

```text
NVS_ADAPTER_GATE=PASS
SOFTWARE_ORACLE_GATE=PASS
CAPACITY_GATE=PASS
CI_ARTIFACT_SECRET_SCAN_GATE=PASS

GENERIC_PLATFORM_PERSISTENCE_GATE=PASS
PRODUCT_APPLICATION_COMPOSITION_GATE=TRANSFER_PENDING_OWNER_APPROVAL
```

`RELEASE_STACK_SCRATCH_GATE` bleibt unverändert `BLOCKED` (real
dokumentiert in `docs/ROADMAP.md` und der PR-#118-Historie: zwei benannte,
real noch nicht aufgelöste indirekte Aufrufe auf dem
Boot-/NVS-Init-/Log-Pfad — der konfigurierbare `esp_log`-`vprintf_like_t`-
Schreiberzeiger und ein STL-Lambda in
`NVSPartitionManager::lookup_storage_from_name`). Dieses Gate gehört
technisch zu #90 (statische Release-Callgraph-Analyse des vorhandenen
Adapters und Composition Roots), nicht zu #119, und wird durch R6.0 nicht
verändert.

`CALLBACK_12_REAL_NVS_PRODUCT_GATE`, `REAL_NVS_RECOVERY_GATE` (voller
Umfang), `MANUAL_POWER_CUT_GATE` sowie `BOARD_IDENTITY_GATE`/
`UART_RESET_GATE` (bereits real `PASS`, siehe PR-#118-Historie) werden mit
R6.0 als **Produkt-Composition-Nachweise** eingeordnet und in Issue #119
weitergeführt; sie sind kein #90-Abnahmekriterium mehr im engeren Sinn,
bleiben aber als bereits real erbrachte Teilevidenz dokumentiert und
werden in #119 nicht erneut von Grund auf wiederholt, soweit sie
weiterhin gültig sind.

## 6. Neues Integrationsissue

`#119` — `[E5.8] Produktive StateStore-/Application-Composition anbinden
und real verifizieren`, Epic #7. Eigener Plan unter
`docs/tasks/issue-119-platform-application-composition-plan.md`, eigener
Draft-PR, Base `#118`-HEAD `812dabd860d3cf0ec02ee5ffc951a74dec77ebca`.
`IMPLEMENTATION=NOT_STARTED` bis zur eigenen Ownerfreigabe der dortigen
exakten Plan-SHA.

## 7. Dynamische Statuskorrektur

Die frühere Formulierung *"Slice 6 ist softwareseitig vollständig
verifiziert, bleibt aber wegen evidenzseitiger Gates blockiert"* war zu
stark. Solange `RELEASE_STACK_SCRATCH_GATE` offen ist, gilt:

```text
funktionale Software-/Backend-/Buildtests: PASS
finale Slice-6-Softwareverifikation: BLOCKED
```

## 8. Umfang dieser Planrunde

Zulässig in dieser Runde sind ausschließlich:

1. diese Planrevision;
2. das neue Issue #119 und sein eigener Plan-/Draft-PR-Rahmen (kein
   Umsetzungscode);
3. die minimale `docs/ROADMAP.md`-Synchronisierung;
4. PR-Body, Issue-#90-Kommentar und genau ein aktueller
   `SESSION HANDOVER` als dynamische Metadaten.

Keine Produktions-, Test-, Oracle-, Harness-, CI-, Backend-, Partitions-
oder Hardwareänderung. Keine Implementation der in #119 skizzierten
Composition. STOP nach Owner-Review beider Plan-SHAs (dieser Datei und
der neuen #119-Plandatei).
