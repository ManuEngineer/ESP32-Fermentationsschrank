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
   `::create(...)`), `RunPersistenceCoordinator` und
   `RunRecoveryCoordinator`, alle über den bereits geöffneten
   `NvsStateStore` (als `IStateStore&`). Das ist ADR-013-konform: der
   Composition Root darf konkrete Anwendungsmodule instanziieren.
2. `FermentationApplication::begin(...)` erhält diese bereits
   konstruierten Recoverykomponenten als zusätzliche, explizite
   Referenzparameter (analog zum bestehenden
   `const IResetCauseSource*`-Muster) statt sie selbst zu besitzen oder
   über `IPlatformServices` zu beziehen. `FermentationApplication` bleibt
   Eigentümer nur der Verknüpfung zu `SafetyCore`, nicht der
   Store-/Persistenzobjekte.
3. Kein neuer Port, kein neues Schema, keine neue generische
   Application-API. Exakte Parameterreihenfolge, Fehlerpfad bei
   `ConfigurationRecoveryService::create(...) == nullptr` und die
   endgültige Methodensignatur werden in Schritt 1 (Abschnitt 8)
   implementiert, nicht in diesem Plan vorweggenommen.

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

Die in Abschnitt 5 festgelegte kleinste Verdrahtung umsetzen:
vorhandenen `NvsStateStore`/`IStateStore` verwenden; vorhandene
app-seitigen Recoverykomponenten verwenden; kleinste notwendige
Ownership-/Lifetime-Verdrahtung im Composition Root. Keine neue
generische Application-API, kein Service Locator, kein DI-Framework,
kein zweiter Recovery-Orchestrator, keine alternative
Diagnose-Composition.

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
| Architekturentscheidung getroffen und begründet (Abschnitt 5) | `PASS` |
| `MATERIAL_ARCHITECTURE_DECISION_OPEN` | `NO` |
| Akzeptanzkriterium definiert | `PASS` |
| Neue Produktions-/Test-/Compositionimplementation | `NOT_RUN` / nicht enthalten |
| Reale Boardverifikation (Schritt 2) | `NOT_RUN` bis eigene Freigabe |
| Exakte R1.0-Planfreigabe | `BLOCKED` bis Ownerentscheidung |

STOP nach Freigabe dieser Datei; keine Implementation vor Freigabe von
Schritt 1.
