# Issue #26 – Lokale Touch-Shell und Fermentations-Workspace

## 1. Ausgangslage, Auftrag und Baseline

Dies ist die vollständige, eigenständig ausführbare Planrevision für Issue
#26. Sie ersetzt keinen bestehenden Fachvertrag. Für die Umsetzung ist diese
Fassung zusammen mit den unten genannten aktuellen Quellen zu verwenden; es
ist keine ältere #26-Planfassung bekannt oder heranzuziehen.

    ISSUE=26
    TITLE=[E4.2] Lokale Touch-Shell und Fermentations-Workspace
    PR=143_DRAFT
    BRANCH=feature/issue-26-local-touch-shell
    WORKFLOW=PLAN_FIRST_SINGLE_PR
    BASE_BRANCH=integration/r1-development
    BASE_SHA=87bd668e45ab71a20ceb24ce65fcb5d1440725a8
    ROADMAP_COMMIT=7bd0b6fe7ac7fc0f11505d1b1cd3b38d9f1fb714
    PLAN_PATH=docs/tasks/issue-26-local-touch-shell-plan.md
    PLAN_COMMIT=THIS_COMMIT
    PLAN_REVISION=F1_F6_PREDECESSOR_AND_CONTRACT_CORRECTIONS
    SUPERSEDES_PLAN_COMMIT=3de1d86180413896294f09bcfddb447df8c1e898
    RUN_IDENTITY_PREDECESSOR_REQUIRED=YES
    RUN_IDENTITY_PREDECESSOR_ISSUE=144
    PREDECESSOR_ISSUE=25
    PREDECESSOR_PR=142
    PREDECESSOR_SOURCE_HEAD=6ff0176651cf5f5dfe8b04d424377efa99ce551f
    PREDECESSOR_MERGE_COMMIT=87bd668e45ab71a20ceb24ce65fcb5d1440725a8
    PREDECESSOR_APPROVED_PLAN_SHA=3367b2990f45ac83854b4a2a23b8a8b289df139c
    PREDECESSOR_CI_RUN=1015
    PREDECESSOR_CI=PASS
    IMPLEMENTATION=NOT_STARTED
    OWNER_PLAN_APPROVAL_REQUIRED=YES
    ACTUATOR_RELEASE=NO
    NATIVE_TESTS=NOT_RUN
    ESP_IDF_BUILD=NOT_RUN
    HARDWARE_TEST=NOT_RUN
    ISSUE25_GITHUB_STATE=CLOSED
    ROADMAP_SYNC=ISSUE144_PREDECESSOR_BLOCKER

Vor diesem Plan-Commit wurden live geprüft:

- der Remote-Branch integration/r1-development steht auf BASE_SHA;
- PR #142 ist gemergt, sein Source-HEAD und Merge-Commit entsprechen den
  Vorgaben, und GitHub-CI #1015 ist PASS;
- Issue #26 ist offen mit dem Titel und der Scope-/Akzeptanzspezifikation
  dieses Auftrags;
- Issue #25 ist live CLOSED und liefert den gemergten UI-Vertrag als Basis;
- Issue #144 ist live OPEN und als konkreter Run-Identity-/Provenienz-
  Pflichtvorgänger vor #26 in der Roadmap eingetragen;
- der revidierte Plan basiert auf dem bisherigen Plan-Commit
  SUPERSEDES_PLAN_COMMIT; der Branch enthält davor den Roadmap-Sync, den
  ursprünglichen Plan und die bisherigen Planrevisionen, aber keine
  Implementation;
- PR #143 ist als Draft angelegt und zielt auf
  integration/r1-development;
- der neueste veröffentlichte SESSION HANDOVER des Vorgänger-PRs nennt
  PREDECESSOR_SOURCE_HEAD, Owner Full Review PASS und das damalige CI-Gate;
  nach dem Merge ist der Handover nur Provenienz und kein offener
  #25-Arbeitsschritt;
- der Arbeitsbaum war vor der Planbearbeitung sauber.

Kontextnachweis:

    CONTEXT_BASELINE_BRANCH=integration/r1-development
    CONTEXT_BASELINE_SHA=87bd668e45ab71a20ceb24ce65fcb5d1440725a8
    CONTEXT_HEAD_SHA=3de1d86180413896294f09bcfddb447df8c1e898
    CONTEXT_PLAN_SHA=3de1d86180413896294f09bcfddb447df8c1e898
    CONTEXT_REFRESH_MODE=FULL
    CONTEXT_DELTA=F1-F6 post-3de predecessor and contract corrections after Full Review
    SOURCE_OF_TRUTH_CONFLICT=NONE

Der erste Commit dieses PRs ist ausschließlich der Roadmap-Sync. Der
ursprüngliche vollständige Plan war der zweite Commit
c57be99bdce9d55ebb65b4c4c06e5210e84b7ed9; darauf folgten die F1/F2-Revision
2fbe85f41c2461575331c4c3afed73a447302d43, die F1-F10-Revision
d8a0a70983a6641e1edf27be08d655572164d995d und die Restkorrektur
dd64d92745ed7ad1b0e744a0e02e4e5b09cec3b9. Diese finale Integrationskorrektur
ist eine weitere versionierte Planrevision. Issue #144 ist nun der konkrete
Pflichtvorgänger für den Run-Identity-/Provenienzvertrag; #26 bleibt bis zu
seinem Merge blockiert. Bis zu einer ausdrücklichen Freigabe der
exakten revidierten PLAN_COMMIT bleibt die Implementation NOT_STARTED.

## 2. Ziel und Definition der Umsetzung

Issue #26 liefert die kleinste lokale, simulierte Touch-Bedienlogik auf Basis
der gemergten #25-Verträge. Das Ergebnis besteht aus:

1. einer generischen, rendererunabhängigen Device-Shell-Interaktion;
2. einem davon getrennten, fermentationsspezifischen Workspace;
3. einer deterministischen nativen Simulation, die beide Teile nur im Test
   zusammensetzt;
4. gezielten Zustands-, Layout- und Interaktionsnachweisen für 320 x 240;
5. der nötigen Dokumentation der Testfälle und der unveränderten Hardware- und
   Safety-Grenzen.

Die Shell erzeugt keine Fachentscheidung. Der Workspace projiziert den
vollständigen FermentationUiSnapshot und erzeugt nur bereits definierte
Intent-/Commandformen. Die bestehende Fachlogik, Sensorbewertung,
Safetybewertung, Recovery, Konfiguration, Persistenz und Aktorplanung bleiben
ihre jeweiligen Owner.

Die lokale Simulation muss mindestens folgende beobachtbare Bereiche
abdecken:

- Home-Modi bereit, aktiv, wartet, abgeschlossen, eingeschränkt und Recovery;
- genau vier feste Bottom-Slots sowie Home-/Zurück-Hierarchie;
- erster App-Slot als kontextabhängige Aktion für Vorheizen, Start, Stop,
  Fortsetzen, Bestätigen und Abschluss;
- zweiter App-Slot Rezepte;
- Status- und Service-Seiten sowie statische App-Erweiterungen;
- vertikale Listen- und Informationsnavigation mit sichtbarem Auf-/Ab-Ziel;
- Pressfeedback, Sperrgrund und strukturierte Bestätigung;
- integrierte Headerflächen für Sprache, WLAN und Uhrzeit;
- Splash und nichtblockierender Startstatus;
- aktorfreie Recoveryanzeige;
- normale maskierte PIN-Eingabe als UI-Modell;
- lokale Servicesitzung mit 10 Minuten Inaktivität ohne absolute R1-Dauer;
- explizites Abmelden und Lock bei Neustart beziehungsweise bei einer vom
  bestehenden Owner gemeldeten sicherheitsrelevanten Zustandsinvalidierung;
- UI-Dimmen/Ruhezustand mit Wake-only-Ersttouch;
- semantische Pieper-/Feedbackintents ohne Pieperhardware;
- reproduzierbare Zustands- und Layoutnachweise im 320-x-240-Simulator.

## 3. KISS-Grenze und Nicht-Ziele

Die folgenden Grenzen sind harte Planinvarianten:

    REAL_DISPLAY_DRIVER=NO
    ILI9341_XPT2046_ASSUMPTION=NO
    RAW_TOUCH=NO
    TOUCH_CALIBRATION=NO
    DMA_OR_FRAMEBUFFER_DESIGN=NO
    REAL_BACKLIGHT_INTEGRATION=NO
    NEW_WIDGET_FRAMEWORK=NO
    NEW_RENDERER_FRAMEWORK=NO
    DUPLICATE_DOMAIN_OR_SAFETY_LOGIC=NO

Zusätzlich gilt:

    LVGL_IMPLEMENTATION=NO
    HTML_IMPLEMENTATION=NO
    DISPLAY_DRIVER=NO
    TOUCH_DRIVER=NO
    GPIO_CHANGE=NO
    BOARD_PROFILE_CHANGE=NO
    REAL_HARDWARE=NO
    NEW_LIBRARY=NO
    NEW_PLUGIN_PLATFORM=NO
    RUNTIME_DISCOVERY=NO
    SECOND_PERSISTENCE=NO
    SECOND_RECOVERY_COORDINATOR=NO
    SECOND_AUTHENTICATION_BACKEND=NO
    ACTOR_RELEASE=NO
    NEW_SAFETY_POLICY=NO
    NEW_SENSOR_ROLE_POLICY=NO
    NEW_THERMAL_POLICY=NO

Nicht Bestandteil der Umsetzung sind insbesondere:

- Auswahl oder Einbindung eines Renderers, eines Treibers, eines
  Displaycontrollers, eines Touchcontrollers oder eines Backlightadapters;
- Touchrohwerte, Koordinatenkalibrierung, physische Recoverygesten, SPI,
  DMA, Framebuffer, Pixeltransfer und Laufzeit-SVG/Rasterisierung;
- reale Display-, Touch-, Pieper-, WLAN-, RTC-, Sensor- oder Aktortests;
- eine neue Persistenz für Screens, Navigation, UI-Requests, Layout,
  Meldungen oder PIN-Eingaben;
- eine neue Fach-, Sensor-, Regel-, Safety-, Recovery- oder
  Aktorfreigabelogik;
- eine zweite Widget-, Komponenten-, Plugin- oder Rendererplattform;
- Web-API, HTML, Webauthentisierung oder die Web-Session;
- ein produktiver PIN-Verifier, PIN-Hash, Credential-Record oder PIN-Reset;
- Touchkalibrierung und die PIN-unabhängige Raw-Touch-Recovery aus #31;
- eine absolute maximale Dauer für die lokale R1-Servicesitzung;
- Änderungen an aktiven Laufschnappschüssen durch den Rezepte-Workspace;
- erfundene Zeit-, Temperatur-, Sensor-, GPIO-, Hardware- oder
  Ressourcenwerte.

Die Simulation ist ein reproduzierbarer Nachweis und kein separater
Firmwarepfad. Sie erhält keinen Composition-Root-Eintrag und wird nicht in
esp32_bringup oder esp32_release eingebunden.

## 4. Verbindliche Quellen und Reuse

### 4.1 Normative Quellen

Vor der Umsetzung werden nur die direkt betroffenen Quellen erneut gegen den
freigegebenen Plan geprüft:

- Issue #26 und seine Live-Akzeptanzkriterien;
- docs/DEVICE_UI_ARCHITECTURE_DECISIONS.md, insbesondere UI-01 bis UI-24;
- docs/DEVICE_UI_VISUAL_DESIGN.md für 320-x-240-Header-, Splash- und
  Themegeometrie;
- docs/LOCAL_UI.md für Shell, Home, Startzusammenfassung, Touchregeln und
  Dimmung;
- docs/LOCAL_UI_PROGRAMS.md für Rezepte, Startänderungen,
  Programmeditor, Kopieren, Neuanlage, Löschen und Werksresetgrenzen;
- docs/LOCAL_RUNTIME_UI.md für Prozessdetails, Stop, Produkt einsetzen,
  Meldungen, Quittieren, Stummschalten, Sensorersatz, Recovery und Abschluss;
- docs/LOCAL_UI_SETTINGS_SERVICE.md für Menüs, Status, Service, PIN, SAFE_BOOT
  und Servicesperre;
- docs/ACCEPTANCE_TESTS.md für die vorhandenen SIM-/UT-/Safety-Orakel;
- docs/ARCHITECTURE.md, docs/REQUIREMENTS.md, docs/STATE_MACHINE.md,
  docs/RUN_COMMANDS.md, docs/RUN_PERSISTENCE.md und
  docs/SYSTEM_SAFETY_AND_RECOVERY.md für die kanonischen Ownerverträge;
- docs/RECOVERY_AND_INTERRUPTION.md für den #124-R1-Recoverypfad;
- lib/fermentation_app/src/temperature_control_orchestrator.hpp/.cpp,
  process_state_machine.hpp/.cpp und run_persistence_coordinator.hpp/.cpp
  für die bestehende Application-Handoff-Grenze und die getrennten
  Decision-/Persistenzpfade;
- docs/AGENT_WORKFLOW.md und docs/CI_AND_QUALITY_GATES.md für Plan-, Test-,
  Draft- und Owner-Gates;
- ADR-013_REUSABLE_DEVICE_PLATFORM.md und die aktuelle
  docs/DECISIONS.md für Modulgrenzen und akzeptierte Entscheidungen;
- docs/tasks/issue-25-device-ui-contracts-plan.md als vollständige
  #25-Provenienz und Reuse-Vertrag.

### 4.2 Bereits gemergte #25-Verträge

Folgende Typen und Regeln werden verwendet, nicht parallel nachgebaut:

| Bestehender #25-Vertrag | Verwendung in #26 |
|---|---|
| DeviceUiBuildCatalog, BrandingId, LocaleId, ThemeId, TextKey | Buildbranding, Auswahl und Schlüssel statt sichtbarer UI-Texte |
| DeviceShellHeader, ClockViewInput, DeviceUiNetworkStatus | Header mit Branding, Locale, WLANindikator und trusted-UTC-/Zone-Eingang |
| LocalDeviceShellState, BottomSlot, ShellRoute, PageExitRequirement | Shellstatus, vier Slots, Home/Back und geschützte Seitenwechsel |
| StaticUiExtensionCatalog, UiSectionDescriptor | Plattformsektionen zuerst, App-Erweiterungen danach, Fehlerisolation |
| FermentationUiSnapshot und FermentationUiProjector | gemeinsame fachliche Wahrheit für Touch und spätere Weboberfläche |
| FermentationUiExpectedRevisions | fachcommandbezogener Stalenesschutz |
| FermentationUiCommand, FermentationUiCommandBridge und Resulttypen | bestehende Commands, Confirmation und kanonische Ergebnisse |
| CommandEnvelope und UiRequestId-Abbildung | dieselbe Command-ID und bestehende Duplicate-/Idempotenzsemantik für Envelope-Commands; ProductInsertedConfirmed erhält keine künstliche Envelope-/Idempotenzsemantik |
| resolveText, getrennte Platform-/Fermentation-Textpacks | DE/EN/ES, Fallback und kein Textliteral in Fachlogik |
| ThemeDescriptor, ThemeToken und resolveTheme | semantisches ManuEngineer-Dark-Theme und sicherer Fallback |
| ServiceSessionPolicy, ServiceSessionLease und ServiceSessionEvent | lokale 10-Minuten-Lease ohne R1-Absolutlimit und explizite Invalidierung |
| bestehende UserConfiguration-/ConfigurationService-Preview | Sprache, Theme, Darstellung und Rezepte über den einzigen Config-Pfad |

Die Shell kennt weiterhin keine Fermentationsnamen und die App kennt keine
Renderer- oder Hardwaretypen. Ein lokaler Touch-Input wird als semantisches
Ziel modelliert. Kein Code liest einen Rohwert oder berechnet aus einem
Touchpunkt eine Kalibrierung.

### 4.3 Kanonische Ownergrenzen

- ProcessState, ProcessRuntimeState, RunProgramSnapshot,
  EffectiveRunValues, SensorQualitySnapshot, RuntimeMessage,
  PresentationState, RecoveryDisposition, RunPersistenceResultStatus und
  Service-/Configuration-Status werden nur projiziert.
- Ein Screen entscheidet weder Sensorrolle noch Sensorqualität, Safety,
  Aktorpermission, Recoveryvertrauen oder Zeitkorrektur.
- Die vorhandene decide*-Funktion beziehungsweise der vorhandene
  Configuration-/Persistence-Pfad läuft vor jeder UI-Confirmation, sofern
  dieser Pfad eine fachliche Mutation ist.
- Eine UI-Confirmation ist nur eine Darstellung und erneute Übergabe des
  kanonischen Confirmation-Vertrags. Sie ist keine zweite
  Confirmation-Engine.
- UiRefreshRevision bleibt Hint und Cacheinvalidierung. Sie ist kein
  Command-Gate.
- Der lokale Workspace mutiert niemals einen aktiven Snapshot. Änderungen
  vor einem Lauf gelten als Kandidat für den nächsten Lauf oder werden über
  den vorhandenen ConfigurationPreview- und Commitpfad dauerhaft gespeichert.
- SAFE_BOOT, RecoveryEvaluation, RecoveryRejectedOrFailClosed und
  FallbackSelectionRequired bleiben aktorfrei. Kein Simulatorergebnis setzt
  ActuationInterlock::Allowed.

### 4.4 Verbrauchervertrag des vorgelagerten Run-Identity-Scope

Issue #144 ist der konkrete, verpflichtende Pflichtvorgänger für den kleinen
Run-Identity-/Provenienzvertrag. Seine Schemaänderung, Codec-/Recoverytests,
konkreten Dateien und Allocatorimplementation gehören ausschließlich in den
eigenen Issue-#144-Plan. #26 beschreibt nur die nach dem Merge garantierten
Schnittstellen und Invarianten:

    RUN_IDENTITY_PREDECESSOR_ISSUE=144
    RUN_IDENTITY_PREDECESSOR_MERGE_HEAD=REQUIRED_BEFORE_26_IMPLEMENTATION
    PROGRAM_SOURCE_PROVENANCE=NEUTRAL_RUN_SOURCE_REVISION
    RUN_PROGRAM_FIELD=sourceProgramRevision

- `RunProgramSnapshot::sourceProgramRevision` ist nach #144 ein neutraler,
  eindeutig benannter starker 64-Bit-Typ wie `RunProgramSourceRevision`.
  Neue Starts erzeugen ihn an der Application-Grenze exakt aus der aktuell
  validierten `ProgramCatalogRevision`; diese Umwandlung behauptet keine
  per-program Revision.
- Schema 4 schreibt diesen neutralen 64-Bit-Wert. Unterstützte Schemas 1–3
  bleiben lesbar und erweitern ihren historischen 32-Bit-Wert nur numerisch
  verlustfrei in den neutralen Typ; #26 etikettiert Legacy-Werte nicht als
  `ProgramCatalogRevision`, dekodiert und migriert sie aber auch nicht selbst.
  Unbekannte neuere Schemas bleiben fail-closed.
- Der vollständige `ProgramDocument` im Run-Snapshot bleibt die unveränderliche
  tatsächlich verwendete Laufkopie einschließlich next-run-only-Overrides.
  Die lokale UI-/Editor-Staleness verwendet davon unabhängig ausschließlich
  die echte `RuntimeConfigurationSnapshot::programCatalogRevision()`.
- Die Application-Grenze stellt den gemeinsamen monotonen `CommandId`-
  Allocator für Touch und später Web bereit. Pro Fachrequest gilt
  `UiRequestId.value == CommandEnvelope::id`; neue Requests erhalten neue
  fortlaufende IDs, ein Confirmation-Replay behält exakt dieselbe ID.
- Jede Erzeugung eines neuen aktiven Runs bezieht die Lauf-ID ausschließlich
  dort. Das umfasst `StartProgram`, `StartManualHolding`, `AbortAndCool` und
  `CoolAfterCompletion`; alle verwenden dieselbe Ableitung aus
  `StorageEpoch + StartCommandId` im bestehenden 1..48-Byte-Limit. UI-
  Payloads liefern weder `runId` noch `CommandId`.

Vor der #26-Implementation muss Issue #144 einen eigenen Plan-/Review-/Merge-
Nachweis besitzen. Danach wird dieser #26-Plan auf dessen exakten Merge-HEAD
aktualisiert. Erst dann ist der normale Programmstart ohne Provenienz- oder
Identitäts-Ownerblocker ausführbar; #26 implementiert keinen Teil des
Vorgängerscope erneut.

## 5. Minimale Zielarchitektur

Die Umsetzung besteht aus drei bewusst getrennten Teilen:

    device_platform
        generische semantische lokale Shell-Interaktion,
        UI-Ruhezustand, Feedbackintents und PIN-Eingabemodell

    fermentation_app
        fermentationsspezifischer Workspace,
        Seitenprojektion und Abbildung auf bestehende UI-Commands

    device_platform_test_support plus native test
        deterministische Shell-/Layoutsimulation und Testzusammensetzung

### 5.1 Generische Device-Shell

Die Shell nimmt einen DeviceShellHeader, vier BottomSlots und eine ShellRoute
entgegen. Sie bietet nur semantische Ergebnisse:

- HeaderLanguage, HeaderNetwork und HeaderClock;
- BottomSlot mit Index 0 bis 3;
- HomeOrBack;
- PagerUp und PagerDown;
- Confirm, Cancel und Back als generische lokale Interaktionsziele.

Die Shell gibt bei einer Eingabe eine strukturierte Interaktionsantwort zurück:

- WakeOnly, wenn der erste Touch einen gedimmten oder schlafenden Zustand
  verlässt;
- TargetSelected, wenn das semantische Ziel nach dem Wake aktiv ist;
- Blocked, wenn ExitRequirement, CompletionLocked oder ein ausgeschaltetes
  Ziel die Bedienung sperrt;
- Ignored, wenn kein Ziel aktiv ist.

Die Shell entscheidet nicht, welches App-Command ein Bottom-Slot auslöst.
Sie liefert nur den Slotindex und den sichtbaren Press-/Feedbackstatus an den
Workspace. Dadurch bleibt die Shell von Fermentations- und Webnavigation
getrennt.

### 5.2 Fermentations-Workspace

Der Workspace liest einen vollständigen FermentationUiSnapshot und eine
explizit übergebene aktuelle Programmlisten-/Konfigurationsquelle. Er liefert
eine semantische Workspace-Ansicht:

- Seiten-ID und Route-Segment;
- Seitentitel als TextKey;
- priorisierte Banner-/Meldungsdaten aus dem Snapshot;
- Temperatur-, Prozess-, Technik-, Status- und Serviceinformationen;
- genau vier appseitige Slotinhalte zur Übergabe an die Shell;
- VerticalPager-Zustand;
- mögliche strukturierte Confirmation;
- möglicher Sperrgrund;
- App-Action beziehungsweise bestehende FermentationUiCommand-Intentform.

Der Workspace hält nur flüchtige Route-, Editor-, Dialog-, PIN- und
Pagerzustände. Er hält keine Domainwahrheit, keinen zweiten Snapshot, keine
persistente UI-Queue und keine zweite Revisionsquelle.

Jede mutierende Appaktion verlässt den Workspace über ihren bestehenden
Application-Handoff. Für Run-Mutationen ist
TemperatureControlApplicationOrchestrator die einzige Grenze:
persistCommand beziehungsweise persistFreshStartCommand,
persistTransition oder persistSensorSelection übernehmen Apply,
Persistenz und Post-Commit-/Lifecycle-Handoff. Der Workspace ruft keinen
RunPersistenceCoordinator direkt auf.

### 5.3 Simulation

Die Testsimulation komponiert:

    canonical source state
        -> FermentationUiProjector
        -> FermentationTouchWorkspace
        -> generic LocalDeviceShellState
        -> SimulatedDeviceShell
        -> existing Application-Handoff for mutating app actions
        -> deterministic frame/layout/feedback trace

Der Testtreiber führt mutierende Szenarien über die bestehenden
Domainentscheidungen und den TemperatureControlApplicationOrchestrator mit
seinen Test-/Persistenzpfaden. Er erfindet keine vereinfachte Safety- oder
Recoveryregel und baut keinen zweiten Dispatcher, Command-Bus,
Persistenzkoordinator oder Lifecycle-Handoff. Der Simulator kennt keine
Displaybytes und keine Aktor-GPIOs.

## 6. Semantische Zustands- und Seitenverträge

### 6.1 Home-Modi

Die Projektionsquelle führt den bestehenden kanonischen
ApplicationLifecycleState explizit mit:

    FermentationUiApplicationSource {
        ApplicationLifecycleState lifecycleState;
        PresentationState presentation;
    }

FermentationApplication::lifecycleState() wird unverändert in diese Quelle
übergeben. `FermentationUiProjectionInput` führt daneben unverändert die
separate `FermentationUiServiceSource service`; Serviceavailability wird nicht
in `FermentationUiApplicationSource` dupliziert. Ein boolesches ready-Feld,
eine Faultdarstellung oder eine andere indirekte UI-Ableitung ist keine
Eingabe für die Home-Projektion. Die
ApplicationStatusView darf ready höchstens als exakt abgeleitete
Darstellung von lifecycleState == Ready führen; die Projektionsentscheidung
verwendet den Enumwert selbst.

Die exakten FermentationHomeMode-Werte nach #26 sind:

    Standby       // sichtbares Label: Ready/Bereit
    ActiveRun     // sichtbares Label: Aktiv
    Waiting       // sichtbares Label: Wartet
    Completed     // sichtbares Label: Abgeschlossen
    Restricted    // sichtbares Label: Eingeschränkt
    Recovery      // sichtbares Label: Recovery
    Unavailable   // technischer Fallback, kein siebter sichtbarer Home-Modus

Standby bleibt als bestehender Codewert erhalten und wird nicht in einen
zweiten Fachzustand Ready umbenannt. FermentationHomeMode::ServiceRequired
wird aus dem Home-Enum entfernt; ApplicationLifecycleState::ServiceRequired
ist ausschließlich die kanonische Lifecyclequelle und wird zu
FermentationHomeMode::Restricted projiziert. Damit konkurrieren
ServiceRequired und Restricted nicht als zwei sichtbare Begriffe.
Unavailable bleibt außerhalb der sechs geforderten sichtbaren Modi und wird
nur bei fehlender oder technisch nicht verfügbarer Projektionsquelle
verwendet, nicht als Ersatz für Fault oder Recovery.

Die deterministische Priorität lautet:

1. ApplicationLifecycleState::Initializing -> Unavailable und
   nichtblockierender Startstatus;
2. ApplicationLifecycleState::ServiceRequired -> Restricted;
3. Nur bei ApplicationLifecycleState::Ready wird weiter ausgewertet:
   aktiver kanonischer RecoveryDisposition-, RecoveryEvaluation- oder
   FallbackSelectionRequired-Zustand -> Recovery;
4. Bei Ready ohne aktiven Recoveryzustand gilt
   ProcessState::SafeBoot, Fault, Boot oder ServiceMode -> Restricted;
5. ProcessState::Completed -> Completed;
6. ProcessState::WaitingForProduct -> Waiting;
7. eine kanonische offene decisionRequired-Meldung -> Waiting, aber nur wenn
   RuntimeMessage aktiv, nicht aufgelöst, mit decisionRequired=true und für
   den aktuellen Prozesskontext vom Fachowner zulässig;
8. andere laufende kanonische Phasen
   Preheating/ReachingTarget/QualifyingTarget/Fermenting/Cooling/
   CoolHolding/ManualHolding -> ActiveRun;
9. ProcessState::Standby bei ApplicationLifecycleState::Ready und ohne Lauf
   -> Standby;
10. fehlende oder nicht auswertbare kanonische Quelle -> Unavailable.

Die Anwendung darf bei ServiceRequired keine Recoverydaten mehr als
Recoverymodus projizieren. Die EntscheidungRequired-Ausnahme ist absichtlich
eng und gilt ebenfalls nur unter ApplicationLifecycleState::Ready: Sie
verwendet das kanonische Feld RuntimeMessage::decisionRequired zusammen mit
active, resolved und dem Ownerkontext. Eine beliebige Meldung, ein
Textschlüssel, eine Farbe oder ein UI-Fehler darf keinen Home-Modus ändern.
Alle anderen Warnungen bleiben Banner-/Detaildaten. So ist Waiting entweder
durch ProcessState::WaitingForProduct oder durch diesen ausdrücklich
kanonischen offenen Entscheidungsstatus begründet und wird nicht aus einer
einzelnen beliebigen Meldung erraten.

### 6.2 Feste vier Slots

Jeder Frame enthält eine std::array mit exakt vier sichtbaren Slots. Ein
leerer Slot bleibt sichtbar und hat keine Action. Slots werden niemals
verbreitert oder dynamisch entfernt.

Die feste primäre Zuordnung lautet:

| Ebene | Slot 0 | Slot 1 | Slot 2 | Slot 3 |
|---|---|---|---|---|
| Home, Bereit | Vorheizen/Start | Rezepte | Status | Service |
| Aktiver Lauf | Stop | Rezepte | Details | Status |
| Warten auf Produkt/Bestätigung | Weiter/Bestätigen | Rezepte | Details | Status |
| Abschluss | OK/Jetzt kühlen | Rezepte | Details | Status |
| Recovery/Restricted | Recoveryaktion oder leer | Rezepte oder leer | Status/Diagnose | Service/Recovery |

Die tatsächliche Verfügbarkeit kommt aus dem Snapshot und den bestehenden
Schutzregeln. Eine gesperrte Funktion bleibt als Slot sichtbar und trägt einen
TextKey für den Sperrgrund. In SAFE_BOOT wird kein normaler PIN-Service und
kein Aktortest als Slot angeboten.

Auf Unterseiten enthalten die vier Slots lokale Navigation, beispielsweise
Home/Zurück, Auf/Ab, Speichern, Abbrechen oder Weiter. Der erste
Navigationslevel unterhalb Home verwendet Home, tiefere Levels Back. Die
bestehende applyLocalNavigation-Logik bleibt maßgeblich; eine blockierte
PageExitRequirement öffnet nur den strukturierten lokalen Exitdialog und
überschreibt keine Fachentscheidung.

### 6.3 Seiten und Erweiterungen

Die minimale Seitenmenge ist:

- Home;
- Rezepte: Liste, Startzusammenfassung, Bearbeitung, Kopieren/Neu,
  Löschbestätigung und Ergebnis;
- laufender Prozess: Prozessseite, Technikseite, aktive Meldungen,
  Meldungsdetail und Stopdialog;
- Abschluss: Zusammenfassung, Details und optionale Kühlen-Bestätigung;
- Plattform-Status: Übersicht, Meldungen, Diagnose und Systeminformationen;
- Plattform-Service: PIN-Eingabe, Serviceübersicht, Einstellungen und
  Wiederherstellung;
- App-Erweiterungen unter Status und Service, jeweils nach den
  Plattformseiten;
- Headerziele Sprache, WLAN und Uhrzeit/Zeitzone.

Plattformsektionen werden im StaticUiExtensionCatalog zuerst angeordnet. Eine
Unavailable-Appsection zeigt ihren Grund, ohne Plattformstatus oder andere
Appsections zu verdecken. Es gibt keine Reflection, kein Pluginregister und
keine Runtime-Discovery.

### 6.4 Vertikale Navigation

Listen und Informationsseiten verwenden einen kleinen VerticalPager:

- explizite Gesamtanzahl und aktueller Eintrag beziehungsweise Seite;
- Auf und Ab als sichtbare semantische Ziele am rechten Inhaltsrand;
- keine Wischgeste, kein horizontales Scrollen und kein Long-Press;
- am Anfang beziehungsweise Ende ist das jeweilige Ziel disabled und zeigt
  keinen falschen Seitenwechsel;
- ein Wechsel verändert nur den lokalen Pager, keine Domainrevision und keine
  Command-ID.

Der Pager wird für Rezepte, Meldungen, Status-/Diagnosewerte,
Serviceoptionen, Startzusammenfassungen und technische Laufdetails
wiederverwendet. Es entsteht kein allgemeines Widgetframework.

## 7. Fachliche Aktions- und Commandgrenze

### 7.1 Primäraktion

Der Workspace zeigt den vollständigen kanonischen Zustand und wählt die
Beschriftung des Slot 0 über stabile TextKeys:

- ohne explizit gewählten Startkandidaten: Programm wählen; Slot 0 öffnet
  deterministisch die kanonische Programmliste und startet weder das letzte
  Programm noch einen erfundenen Datensatz;
- nach Auswahl eines gültigen Kandidaten: Startzusammenfassung; erst dort
  wird ein expliziter ProgramStartIntent erzeugt;
- bei einem vorheizenden Kandidaten: Vorheizen; fachlich ist dies der
  bestehende StartProgram-Pfad und kein neuer Vorheizautomat;
- bei einem unmittelbar startbaren Kandidaten: Start;
- im aktiven Lauf: Stop; die Auswahl zwischen Ausschalten und Kühlen erfolgt
  im bestehenden Stop-Vertrag;
- bei WaitingForProduct: Weiter beziehungsweise Produkt eingesetzt
  bestätigen; die Transition wird über den bestehenden
  ProductInsertedConfirmed-Prozessereignispfad delegiert;
- bei offener Benutzerentscheidung: die konkret im Snapshot angebotene
  Bestätigungs-/Quittieraktion;
- bei Completed: OK/Abschluss bestätigen oder Jetzt kühlen;
- bei FallbackSelectionRequired: ResumeFallback mit eigener Bestätigung;
- bei Restricted oder nicht verfügbar: keine nicht kanonisch angebotene
  Aktor- oder Servicefreigabe.

Vorheizen, Start, Stop, Fortsetzen, Bestätigen und Abschluss sind damit
sichtbare Kontextaktionen, aber keine fünf neuen Fachautomaten.

Der normale lokale Einstieg in den manuellen Betrieb liegt in der
Programmliste beziehungsweise Startauswahl als semantische Aktion
Manueller Betrieb. Er öffnet einen kleinen ManualRunPlan-Kandidaten mit den
bereits vorhandenen editierbaren Feldern Zieltemperatur, Sensorbetrieb,
optionalem Vorheizen und Qualifikationsdaten, aber ohne Lauf-ID. Nach
fachlicher Prüfung ergänzt die Application-Grenze die Identität und ruft
die App decideManualStart und nach Bestätigung
TemperatureControlApplicationOrchestrator::persistFreshStartCommand auf.
Der vorhandene owning Fachvertrag unterstützt dabei
ProcessKind::ManualHolding; der aktive Lauf bleibt unveränderlich.

Die Produktanforderung eines manuellen Zeit-/Temperaturlaufs ist im aktuellen
Baseline-Code von #26 dagegen nicht durch einen eigenen vollständigen
CommandKind-, Decide- und Application-Persistenzvertrag abgedeckt.
Der Plan kennzeichnet dies ausdrücklich als
MANUAL_TIME_TEMPERATURE_OWNER_CONTRACT=OPEN_OWNER_DECISION und
UNOWNED_R1_GAP, nicht als bereits vorhandenen Vorgänger- oder Ownerpfad.
Als Ownerentscheidung bleiben genau zwei Optionen sichtbar: (1) ein eigener
owning Fachscope wird vor produktiver R1-Fertigstellung eingeplant oder (2)
die Produktanforderung wird ausdrücklich aus R1 verschoben. Bis dahin zeigt
#26 diesen zweiten manuellen Modus typisiert als Unavailable mit dem Grund
OWNER_CONTRACT_MISSING und verbucht ihn nicht als R1-erledigt. Die Umsetzung
beschränkt sich auf den vorhandenen manuellen Haltebetrieb und legt keinen
neuen Fach-, Safety- oder Persistenzpfad an.

### 7.2 Bestehende Commandpfade

Die appseitige Workspaceaktion trennt stets die kanonische Entscheidung vom
tatsächlichen Application-Handoff und vom Persistenz-/Apply-Ergebnis:

| Workspaceaktion | Entscheidung, ohne Mutation | tatsächlicher owning Apply-/Persistenzpfad |
|---|---|---|
| Programmstart/Vorheizen | FermentationUiStartProgramIntent -> decideProgramStart | TemperatureControlApplicationOrchestrator::persistFreshStartCommand; bei einem nicht als Fresh Start qualifizierten Pfad gilt der passende bestehende persistCommand-Aufruf |
| manueller Lauf | FermentationUiStartManualHoldingIntent -> decideManualStart | TemperatureControlApplicationOrchestrator::persistFreshStartCommand |
| Stop ausschalten/kühlen | FermentationUiStopRunIntent -> decideStop | TemperatureControlApplicationOrchestrator::persistCommand |
| Abschluss/OK/Jetzt kühlen | FermentationUiCompleteRunIntent -> decideCompletion | TemperatureControlApplicationOrchestrator::persistCommand |
| Laufwerte nur für diesen Lauf | FermentationUiAdjustRunIntent -> decideRunAdjustment | TemperatureControlApplicationOrchestrator::persistCommand |
| Produkt eingesetzt bestätigen | FermentationUiProductInsertedConfirmedIntent; erwartete Zustandsrevision prüfen, dann decideProcessTransition mit ProcessEvent::ProductInsertedConfirmed | TemperatureControlApplicationOrchestrator::persistTransition; der Workspace ruft keinen RunPersistenceCoordinator direkt auf |
| Meldung quittieren | FermentationUiAcknowledgeMessageIntent -> decideAcknowledgeMessage | TemperatureControlApplicationOrchestrator::persistCommand |
| Akustik stummschalten | FermentationUiMuteMessageIntent -> decideMuteMessage | TemperatureControlApplicationOrchestrator::persistCommand |
| Fehlerreset | FermentationUiResetFaultIntent -> decideFaultReset | TemperatureControlApplicationOrchestrator::persistCommand |
| Sensorentscheidung | FermentationUiSensorSelectionIntent -> decideSensorSelection beziehungsweise canonical decideApplySensorSelectionAction | TemperatureControlApplicationOrchestrator::persistSensorSelection |
| Recovery-Zeitkorrektur | FermentationUiRecoveryTimeCorrectionIntent -> decideApplyRecoveryTimeCorrection | TemperatureControlApplicationOrchestrator::persistCommand |
| Fallback fortsetzen | bestehendes ResumeFallback mit FallbackSelectionRequired | bestehender FermentationApplication-/Recovery-Handoff, nicht der Workspace und nicht ein neuer Dispatcher |
| Konfigurations-/Rezeptcommit | bestehendes Preview-/ConfigurationService-Validation- und Confirmationmodell | bestehender ConfigurationService-Commitpfad |

Eine decide*-Funktion erzeugt ausschließlich eine kanonische Entscheidung.
Der bestehende `fromCommandStatus`-Vertrag aus #25 ist dafür in #26 als
Reuse-Punkt zu korrigieren: Die Phase wird nach dem tatsächlich ausgeführten
Pfad bestimmt, nicht nach dem Statusnamen. Jede Rückgabe eines reinen
decide*-Pfads ist `FermentationUiCommandPhase::DecisionOnly`, einschließlich
`NotConfirmed`, `StaleState`, `InvalidInput`, `SafetyRejected`,
`NotAllowedInState`, `ContextMissing`, `CapacityReached`, `NoChange`,
`AlreadyProcessed`, `Proposed` und weiterer `CommandStatus`-Werte. Auch eine
akzeptierte oder abgelehnte Entscheidung ist damit noch kein Apply- oder
Persistenznachweis. Ein `AlreadyProcessed` aus einer reinen Entscheidung ist
von einem gleichnamigen Ergebnis des owning Persistenzpfads zu unterscheiden.
Ein `OwningOutcome` darf erst die Rückgabe des tatsächlich owning
Apply-/Persistenz-/Commitpfads markieren; ein etwaiges `Applied` wird daher
nicht über den reinen Decide-Adapter vorweggenommen. Es entsteht keine zweite
Ergebnisfamilie. Die Post-Commit-/Lifecycle-Handoffs bleiben vollständig im
zuständigen bestehenden Owner.

### 7.2.1 Gemeinsamer ProductInsertedConfirmed-UI-Contract

Der Workspace erhält in fermentation_app einen kleinen, typisierten und
rendererunabhängigen Contract:

    struct FermentationUiProductInsertedConfirmedIntent {
        std::uint32_t expectedStateSequence;
    };

Die Typdefinition selbst beschreibt die Benutzerabsicht. Sie trägt nur die
notwendige erwartete kanonische Zustandsrevision. Sie trägt keine
CommandEnvelope-, UiRequestId-, Idempotenz-, Safety-, Sensor-, Planner- oder
Recoveryevidenz. Weil der bestehende ProcessEvent-/TransitionRequest-Vertrag
keine CommandEnvelope-/Idempotenzsemantik besitzt, wird dafür keine künstliche
zweite Semantik erfunden.

Der appseitige Bridgepfad prüft expectedStateSequence gegen den aktuellen
kanonischen ProcessRuntimeState, bevor eine Mutation begonnen wird. Diese
Prüfung ist nur ein enger app-owned Concurrency-Guard für diesen
nicht-envelope-basierten Contract; sie ersetzt keine kanonische
Entscheidung und führt keinen neuen FSM-Status ein.

Bei Abweichung verwendet der Guard das bestehende gemeinsame
FermentationUiCommandResult mit
DeviceUiCommandOutcomeCategory::Rejected, dem bereits definierten
CommandStatus::StaleState als Detail und der korrigierten
FermentationUiCommandPhase::DecisionOnly. Der bestehende
fromCommandStatus-Adapter wird dafür mit seinem Pfadvertrag wiederverwendet;
er erfindet keinen `DecisionStatus::Stale` und ruft weder
decideProcessTransition noch den Orchestrator-Applypfad mit einer mutierenden
Entscheidung auf. Der reine app-owned Revisionsvergleich ist hier nur ein
enger Concurrency-Guard für den nicht-envelope-basierten Übergang, kein
Ersatz für die kanonische Entscheidung.

Bei passender Revision erzeugt er ausschließlich die bestehende
TransitionRequest mit ProcessEvent::ProductInsertedConfirmed und ruft
decideProcessTransition auf. Das kanonische
TransitionDecision::status wird unverändert in das gemeinsame
FermentationUiCommandResult übernommen, indem dessen bestehende
Detail-Variante additiv um den bereits vorhandenen Typ DecisionStatus
ergänzt wird, etwa über einen fromTransitionDecision-Bridgehelfer. Jeder
FSM-Entscheidungsstatus bleibt auf diesem reinen Decide-Pfad
`DecisionOnly`, unabhängig davon, ob er `NoTransition`, `Rejected`,
`InvalidInput`, `TimeWentBackwards` oder `Proposed` lautet. Diese fünf Werte
sind die vollständige kanonische `TransitionDecision::status`-Menge. Die
`CommandStatus`-Werte `NotConfirmed`, `SafetyRejected` und
`NotAllowedInState` gehören ausschließlich in den allgemeinen
Command-/`fromCommandStatus`-Pfad und werden nicht als FSM-Status eingeführt.
Jeder nicht-proposed FSM-Status
beendet den Pfad ohne Aufruf von
TemperatureControlApplicationOrchestrator. Erst ein `Proposed` darf an
TemperatureControlApplicationOrchestrator::persistTransition übergeben
werden; ausschließlich dessen tatsächliches RunPersistenceResultStatus-
Ergebnis wird als `OwningOutcome` aufgezeichnet.

Ein Replay desselben UI-Contracts nach dem erfolgreichen Übergang verwendet
die bereits verbrauchte erwartete Revision und wird vor einer zweiten
Mutation abgelehnt; falls zusätzlich die kanonische Zustandsprüfung greift,
bleibt auch deren bestehende Ablehnung erhalten. Es gibt keinen privaten
Workspace-Sonderpfad, keinen zweiten Dispatcher, keinen zweiten
Command-Bus, keinen zweiten Persistenzkoordinator und keinen zweiten
Lifecycle-Handoff.

Die UI liefert für eine Start- oder Recoveryaktion niemals ProgramDocument,
Sensorqualitäts-, Planner-, Safety- oder Recoveryevidenz. Die Anwendung löst
IDs und aktuelle Evidenz an ihrer Ownergrenze auf.

### 7.3 Confirmation, Staleness und Doppelauslösung

- Für Envelope-Commands läuft die bestehende kanonische decide*- oder
  Preview-/Commitvalidierung zuerst. Beim ProductInsertedConfirmed-Contract
  ist der Vergleich von expectedStateSequence nur der enge app-owned
  Concurrency-Guard, der die Übergabe eines veralteten UI-Angebots verhindert;
  bei passender Revision folgt trotzdem immer zuerst
  decideProcessTransition. Der Guard darf keinen kanonischen
  DecisionStatus simulieren oder die FSM-Entscheidung ersetzen.
- `fromCommandStatus` liefert für jeden reinen Decide-Aufruf
  `FermentationUiCommandPhase::DecisionOnly`, nicht nur für
  `CommandStatus::Proposed`. Das gilt mindestens für
  `NotConfirmed`, `StaleState`, `InvalidInput`, `SafetyRejected`,
  `NotAllowedInState`, `ContextMissing`, `CapacityReached`, `NoChange`,
  `AlreadyProcessed` und `Proposed`; erst der jeweilige bestehende owning
  Apply-/Persistenz-/Commitadapter darf `OwningOutcome` liefern.
- Nur ein kanonisches NotConfirmed beziehungsweise
  ReadyForConfirmation erzeugt ConfirmationRequired.
- StaleState, InvalidInput, SafetyRejected, NotAllowedInState,
  ContextMissing, Busy, Unavailable und vergleichbare Zustände werden nicht
  durch ein UI-Precheck oder eine fehlende Bestätigung maskiert.
- Bridge- oder UI-seitige Ablehnungen ohne ausgeführten owning Mutationspfad,
  einschließlich `UnsupportedAppDetail`, sind ebenfalls
  `FermentationUiCommandPhase::DecisionOnly`. `makeResult()` darf dafür
  keinen Default-`OwningOutcome` liefern. Es gibt keinen dritten Phasenwert
  und keine neue Ergebnisfamilie.
- Bei erneuter Bestätigung bleiben UiRequestId und, wo vorhanden,
  CommandEnvelope::id identisch. Es gibt keinen zweiten Requestspeicher.
- Ein echter Duplicate erhält die bestehende AlreadyProcessed- oder
  AlreadyPersisted-Semantik und erzeugt keine zweite Nebenwirkung.
- ProductInsertedConfirmed ist kein Envelope-Command und erhält daher keine
  künstliche UiRequestId- oder Duplicate-Semantik. Der app-owned
  expectedStateSequence-Vergleich verhindert stale Wiederholung vor
  persistTransition; ein bereits erfolgreicher Übergang kann so keine zweite
  Mutation erzeugen.
- Ein Press bleibt sichtbar, während die Antwort Busy/Pending ist; derselbe
  Slot ist bis zur Antwort nicht erneut ausführbar.
- Eine Navigation, ein Pagerwechsel, ein Headerziel, ein Wake-Touch und ein
  reiner Read erzeugen weder CommandEnvelope noch UI-Request-ID.
- Kritische Aktionen wie Stop, dauerhafte Konfigurationsänderung,
  Werksreset, Löschen und Recoveryauswahl verwenden strukturierte
  Bestätigung. Ein zweistufiger Lösch-/Resetablauf bleibt im bestehenden
  Fach-/Konfigurationsvertrag und wird nicht als allgemeiner Dialogmotor
  dupliziert.

### 7.4 Programmschnappschuss und next-run-only-Kandidat

Die Rezeptliste und Startzusammenfassung lesen die aktuelle kanonische
Programmliste. Dafür wird ein kleiner app-owned, rendererunabhängiger
Startkandidatenvertrag verwendet:

    struct FermentationUiStartCandidate {
        std::string programId;
        ProgramCatalogRevision expectedProgramCatalogRevision;
        std::optional<double> targetTemperatureCelsius;
        std::optional<std::uint32_t> fermentationDurationMinutes;
        std::optional<bool> preheatEnabled;
        std::optional<RunSensorMode> sensorMode;
        std::optional<CompletionMode> completionMode;
        std::optional<double> coolingTargetCelsius;
        std::optional<std::uint32_t> holdDurationMinutes;
    };

Die Felder sind ausschließlich die bereits erlaubten next-run-only-
Änderungen: Zieltemperatur, Fermentationsdauer, Vorheizen, Sensorbetrieb,
Abschlussverhalten sowie Kühlziel und die dazu bereits erlaubte
Abschlussdauer. Es gibt kein frei geliefertes ProgramDocument, kein
generisches Patch-/Property-/Formularmodell und keine zweite Programmlogik.
Die bestehende `FermentationUiStartProgramIntent` wird dafür auf diesen
kleinen Kandidateninhalt erweitert beziehungsweise abgebildet und trägt keine
`runId`. Die `FermentationUiStartManualHoldingIntent` trägt ebenfalls nur die
editierbaren ManualHolding-Werte; ein `runId` darf nicht über
`ManualRunPlanRequest` aus der UI eingeschleust werden. Die Application-Grenze
bezieht für beide Startarten die neue `UiRequestId`/`CommandEnvelope::id` aus
dem gemeinsamen monotonen Allocator und erzeugt daraus die Lauf-ID, bevor sie
den vollständigen bestehenden `ProgramStartRequest` beziehungsweise
`ManualStartRequest` mit Commandquelle, Zeit und aktueller owning Evidenz
aufbaut.

Dieselbe Identitätsinvariante gilt für jede weitere Aktion, die einen neuen
aktiven Lauf erzeugt: `AbortAndCool` und `CoolAfterCompletion` ergänzen die
Lauf-ID ebenfalls erst an der Application-Grenze beim Aufbau des bestehenden
`StopRequest` beziehungsweise `CompletionRequest`. Die zugehörigen UI-
Intents tragen beim Kühlersatz nur die bereits erlaubten editierbaren
Cooling-/Manual-Werte, niemals `runId` oder `CommandId`. Für alle vier Pfade
gilt dieselbe Ableitung aus `StorageEpoch + StartCommandId`; eine zweite
Cooling-ID-Logik entsteht nicht. Eine Confirmation-Wiederholung verwendet
dieselbe Command-ID und damit denselben vorbereiteten Laufidentitätswert.

Der owning Ablauf ist strikt:

1. Die UI trägt Programm-ID, echte
   `ProgramCatalogRevision` und die expliziten Overrides.
2. Die App liest das Programm aus dem aktuellen
   `RuntimeConfigurationSnapshot` auf.
3. Die App vergleicht `expectedProgramCatalogRevision` mit
   `RuntimeConfigurationSnapshot::programCatalogRevision()`; eine Abweichung
   liefert das bestehende typisierte Stale-/Konfliktergebnis und mutiert nichts.
4. Die Overrides werden nur auf einen flüchtigen Run-Kandidaten angewendet;
   der aktive `ProgramCatalog` bleibt unverändert.
5. Der vollständige Kandidat durchläuft `validateProgram` und die bestehende
   ProgramStart-Validierung mit ihren vorhandenen Sensor-, Safety- und
   Laufregeln. Ungültige Werte verwenden deren bestehendes typisiertes
   InvalidInput-/Sperrresultat.
6. Erst ein gültiger Kandidat wird am bestehenden Start-/Orchestratorpfad in
   den unveränderlichen `RunProgramSnapshot` überführt.
7. Der tatsächliche Command bleibt bis zum owning
   `TemperatureControlApplicationOrchestrator`-Apply-/Persistenzpfad
   `DecisionOnly`; der Katalog wird durch diesen Start nicht geschrieben.

Zurücksetzen verwirft nur den flüchtigen Kandidaten. Eine gültige Auswahl
führt von Programm wählen deterministisch zur Startzusammenfassung; ohne
explizite Auswahl wird kein letzter oder erfundener Datensatz verwendet.

Die Provenienzbindung ist nach dem vorgelagerten Issue #144 ein fester
Verbrauchervertrag und kein offenes #26-Gate:

    PROGRAM_SOURCE_PROVENANCE=NEUTRAL_RUN_SOURCE_REVISION
    RUN_PROGRAM_FIELD=sourceProgramRevision

`RunProgramSnapshot::sourceProgramRevision` ist nach #144 ein neutraler,
verlustfreier 64-Bit-`RunProgramSourceRevision`. Neue Starts erzeugen ihn an
der Application-Grenze aus der aktuell validierten
`RuntimeConfigurationSnapshot::programCatalogRevision()`; diese Erzeugung
behauptet keine per-program Revision. Unterstützte Legacy-Schemas liefern
historische Werte nur in derselben neutralen Provenienzform, ohne sie als
Katalogrevision zu etikettieren; #26 dekodiert oder migriert diese Schemas
nicht selbst. Der vollständige `ProgramDocument` bleibt die unveränderliche
tatsächliche Laufkopie. Die lokale Start- und Bearbeitungsstaleness verwendet
unabhängig davon immer die echte `ProgramCatalogRevision` und missbraucht die
Run-Provenienz nicht als späteres Stale-Gate.

### 7.4.1 Vollständige lokale Programmverwaltung

Die Programmliste übernimmt die Reihenfolge des kanonischen `ProgramCatalog`:
die vier Standardprogramme stehen vor den Benutzerprogrammen. Die vier
Standardprogramme im aktiven Katalog sind gespeicherte, bearbeitbare
Arbeitskopien. Ein Editieren eines Standardprogramms ändert diese Arbeitskopie
über den bestehenden ConfigurationPreview-/ConfigurationService-Pfad; es
wird nicht zwangsweise in einen Benutzer-Copy-Pfad umgeleitet. Die
unveränderliche Firmware-Factory-Vorlage bleibt davon getrennt und wird durch
Bearbeiten niemals geändert.

Die lokalen Pfade sind:

- Bearbeiten öffnet für Standard- und Benutzerprogramme einen flüchtigen
  ProgramDocument-Kandidaten. Der dauerhafte Commit läuft ausschließlich über
  den bestehenden ConfigurationPreview-/ConfigurationService-Pfad;
- Kopieren ist für Standard- und Benutzerprogramme ein eigener
  Benutzer-Copy-Pfad und verwendet den kleinen deterministischen
  app-owned ID-Allocator aus Abschnitt 7.4.2;
- Zurücksetzen eines Standardprogramms ersetzt dessen gespeicherte
  Arbeitskopie über den bestehenden Configuration-/Factory-Resetpfad durch
  die unveränderliche Factory-Vorlage;
- Neu beginnt mit einem flüchtigen leeren Benutzerentwurf. Dauerhaftes
  Speichern ist zulässig, sobald `validateProgram(...,
  ValidationPurpose::CatalogTemplate)` den Katalogentwurf akzeptiert. Ein
  gültiges CatalogTemplate kann wegen fehlender Commissioning-/Laufwerte
  weiterhin nicht runnable sein: `START` bleibt bis zum erfolgreichen
  `validateProgram(..., ValidationPurpose::Runnable)` typisiert gesperrt.
  Beide Prüfungen und der Commit laufen über den bestehenden
  ConfigurationPreview-/ConfigurationService-Pfad; es gibt keinen Draft-
  Record, keine Draft-Persistenz und kein zweites Programmmodell;
- Löschen verwendet die bestehende zweistufige Bestätigung. Ein aktuell
  verwendetes oder laufendes Programm ist als nicht löschbar markiert und
  erzeugt keinen Commit;
- „Aus aktiver Liste löschen“ darf bei einem der vier reservierten
  Factory-Einträge die Factory-ID oder den Eintrag nicht physisch aus dem
  `ProgramCatalog` entfernen. Die bestehende `installed`-/`userDeletable`-
  Semantik bestimmt die zulässige Deinstallation beziehungsweise Sperre.
  Bei `installed == false` bleibt der reservierte Eintrag ausschließlich als
  Katalogbestand erhalten: Er erscheint nicht in der aktiven Programmliste,
  ist dort nicht startbar und seine Factory-ID bleibt für Benutzerprogramme
  gesperrt. Die Wiederherstellung/Neuinstallation nutzt den bestehenden
  Configuration-Pfad;
- spätere Änderung oder Löschung einer Quelle verändert nie den aktiven
  `RunProgramSnapshot`;
- bei Katalogrevision zwischen Lesen und Commit liefert der
  ConfigurationService-/Previewpfad einen typisierten Konflikt, insbesondere
  StateChanged, PreviewSuperseded oder PreviewNotFound, statt still zu
  überschreiben.

Die UI erfindet weder Validierungs-, Reserved-Factory- noch Löschregeln. Die
Factory-Vorlage bleibt von Benutzeraktionen getrennt; es entsteht keine neue
Factory- oder Programmpersistenz.

### 7.4.2 Kleine deterministische Benutzerprogramm-ID

Für Kopieren und Neu plant #26 eine einzige kleine app-owned Hilfsfunktion,
beispielsweise `allocateNextUserProgramId(const ProgramCatalog&)`. Ihre
Eingabe ist der aktuelle kanonische Katalog beziehungsweise die daraus
aufgelöste Menge der Benutzer-IDs; ihre Ausgabe ist genau eine katalogweit
freie, nach dem bestehenden ID-Validator gültige Benutzer-ID. Die
Kandidatenfolge ist deterministisch: ein fester zulässiger Benutzerpräfix mit
der kleinsten noch freien fortlaufenden Basis-36-Ordinalkennung wird gewählt;
Factory-IDs bleiben reserviert. Der technische Wert ist kein editierbares
UI-Feld.

Eine belegte Kandidaten-ID wird niemals überschrieben. Kollision, ungültige
Katalogbasis oder ausgeschöpfte Kapazität liefert ein typisiertes
Unavailable-/Capacity-Ergebnis ohne Fallback. Zwischen Allokation und Commit
gilt die erwartete echte `ProgramCatalogRevision`; eine geänderte Generation
wird als Stale-/Konfliktergebnis abgelehnt. Die Persistenz bleibt vollständig
im bestehenden ConfigurationPreview-/ConfigurationService-Pfad. Es gibt
keine UUID-Bibliothek, globale ID-Registry, persistierten Zähler oder
generischen ID-Service.

### 7.4.3 Katalogrevision für alle Programmänderungen

Jede flüchtige Programm-Arbeitssitzung führt neben ihrem Kandidaten die echte
`expectedProgramCatalogRevision`, aus der sie geöffnet wurde. Das gilt
gleichermassen für Bearbeiten, Löschen, Zurücksetzen, Kopieren und Neu. Die
Application vergleicht diese Revision unmittelbar vor `beginPreview()` und
vor jeder daraus folgenden Preview-/Katalogmutation mit der aktuellen
`RuntimeConfigurationSnapshot::programCatalogRevision()`.

Bei einer Abweichung liefert der bestehende gemeinsame Ergebnisvertrag ein
typisiertes Stale-/Conflictdetail, insbesondere
`ConfigurationPreviewStatus::StateChanged`, und ruft `beginPreview()` für den
alten Kandidaten nicht auf. Es entsteht keine Preview-Mutation und kein
Überschreiben der inzwischen neuen Arbeitskopie. Erst bei gleicher Revision
darf der vorhandene ConfigurationPreview-/ConfigurationService-Pfad beginnen;
seine weiteren Konflikte zwischen Preview-Erzeugung, Install und Commit,
insbesondere `PreviewSuperseded` und `PreviewNotFound`, bleiben allein dessen
Owner. Es gibt keine persistierte Editorrevision und keinen zweiten
Konfigurations-Revisionsdienst.

### 7.5 Minimale lokale Feld-Eingabeverträge

Für Rezept- und next-run-only-Editoren ergänzt fermentation_app nur zwei
kleine rendererunabhängige UI-Modelle, keine Form-, Widget- oder
Keyboardplattform:

    NumericEditAction =
        Plus | Minus | Digit | DecimalSeparator |
        Backspace | Clear | Cancel | Commit

    TextEditAction =
        Character | Mode | Backspace | Clear | Cancel | Commit

NumericEditModel hält ausschließlich einen flüchtigen Kandidaten,
einschließlich direkter Ziffernfolge und Dezimaltrennzeichen. Plus und Minus
sind sichtbare semantische Feldaktionen; Digit und DecimalSeparator erlauben
die direkte Eingabe. TextEditModel hält für Programmname und kurze Notiz
einen flüchtigen Textkandidaten. Character trägt ein semantisches Zeichen,
Mode wechselt zwischen den enthaltenen Zeichenmodi, und beide Modelle
unterstützen Rückschritt, Löschen, Abbrechen und Übernehmen.

Die Modelle besitzen keine Feldgrenzen, Programmvalidität, ID-Regeln,
Locale-Keyboardlogik oder Commitentscheidung. Diese bleiben bei
validateProgram, ConfigurationPreview, ConfigurationService und den
bestehenden Start-/Run-Contracts. Cancel verwirft nur den Kandidaten,
Commit übergibt ihn zur owning Validierung. Es entsteht kein allgemeines
Keyboard-, Formular- oder Widgetframework.

## 8. Header, Splash und Layoutvertrag

### 8.1 Header

Der Simulator verwendet die normierte 320-x-240-Designgeometrie:

| Bereich | Simulation |
|---|---|
| Viewport | x=0..319, y=0..239 |
| Header | x=0..319, y=0..31, Höhe 32 |
| langes ManuEngineer-Logo | x=4..171, y=4..27, 168 x 24 |
| Sprache | x=176..219, y=0..31, 44 x 32 |
| WLAN | x=220..263, y=0..31, 44 x 32 |
| Uhrzeit | x=264..315, y=0..31, 52 x 32 |
| rechter Rand | x=316..319, y=0..31 |

Diese Angaben stammen aus docs/DEVICE_UI_VISUAL_DESIGN.md. Die Simulation
prüft Sichtbarkeit, Nichtüberlappung, Header-Touchflächen und das ruhige
Erscheinungsbild semantisch. Sie rendert kein Logo und bindet kein Asset in
die Firmware ein.

Sprache öffnet die enthaltenen DE/EN/ES-Auswahlwerte und verwendet das
bestehende UserConfiguration-/Preview-/Commitmodell. WLAN öffnet die
Plattform-Netzwerkseite und zeigt Connected, Disconnected oder Unavailable.
Uhrzeit zeigt die ClockViewInput und führt zu Zeit-/Zeitzoneneinstellungen,
sofern der Plattformpfad vorhanden ist. Die Shell berechnet keine lokale
Zeit und erzeugt keinen künstlichen Wert ohne trustedUtc.

### 8.2 Simulationsrahmen für Inhalt und Footer

Für den reproduzierbaren Layoutnachweis definiert der Testsupport einen
simulationsinternen Rahmen:

    content: y=32..199
    bottom navigation: y=200..239, height=40
    each of four slots: width=80, height=40

Die 40 Pixel sind ausschließlich ein deterministischer Simulatorrahmen, der
den bestehenden Zielgrößenrichtwert von ungefähr 44 x 40 px prüfbar macht. Sie
sind keine reale Display-, Renderer- oder Touchhardwareannahme und werden
nicht als neuer Produktionsrenderer-Vertrag exportiert. Die Layoutprüfung
fordert:

- vier gleich breite Slotrechtecke ohne Überlappung;
- alle Slotrechtecke innerhalb des Viewports;
- Header, Content, Pagerbereich und Footer überlappen nicht;
- deaktivierte und leere Slots bleiben im Frame sichtbar;
- Meldungs-/Statuspriorität wird im Content vor Dekoration bewahrt;
- längere Texte bleiben als TextKey/Textpack-Fall prüfbar und werden nicht
  durch abgeschnittene Fachwerte ersetzt.

### 8.3 Splash und Startstatus

Der Simulator modelliert:

    Start -> Splash
    bis ungefähr 3 Sekunden -> parallel fortschreitende Initialisierung
    sicher bereit -> Home
    noch nicht bereit -> kompakter Startstatus
    kritischer Startfehler -> actor-free Recovery/Restricted

Der Splash verwendet nur die semantische Designreferenz eines proportional
zentrierten ungefähr 300 x 122 px großen Rahmens. Es gibt keine Animation,
keinen Vollbildframepuffer und keinen blockierenden Delay. Der Test weist
nach, dass ein langsamer Start die laufende Initialisierung nicht anhält und
dass ein nichtkritischer Status Home nicht unnötig blockiert.

## 9. Ruhezustand, Feedback, PIN und lokale Servicesitzung

### 9.1 UI-Ruhezustand

Der UI-Idle-Controller ist von ServiceSessionLease getrennt:

- Awake zeigt normale Helligkeit;
- Dimmed beziehungsweise optional Asleep ist ein semantischer Zustand ohne
  Backlightzugriff;
- der erste Touch in Dimmed/Asleep erzeugt ausschließlich WakeOnly;
- der zweite Touch kann das Ziel auslösen;
- automatische Uhr-, Netzwerk-, Sensor-, Message- und Refreshupdates zählen
  nicht als Benutzeraktivität;
- Warnung, Fehler, Completion oder erforderliche Benutzeraktion dürfen den
  UI-Zustand über ein explizites Systemereignis aufwecken;
- die konkrete produktive Dimm-/Ausschaltgrenze bleibt
  TBD_HARDWARE/TBD_COMMISSIONING und wird nicht in #26 erfunden.

### 9.2 Semantische Feedbackintents

Der Plattformvertrag führt nur semantische Intents, beispielsweise:

    TouchPress
    WakeOnly
    ActionAccepted
    ConfirmationRequired
    ActionRejected
    Warning
    Critical
    Completion

Der Simulator zeichnet diese Intents zusammen mit sichtbarem Pressfeedback
auf. Er wählt keinen Pieper, keine Frequenz, kein Timing und kein
Hardwareausgabesignal. RuntimeMessage::acousticIntent bleibt die bestehende
fachliche Quelle für Prozess-/Warn-/Fehlersignale. Ein optionaler Touchton
ist standardmäßig aus, nie die einzige Rückmeldung; sicherheitsrelevante
Signale werden nicht durch eine lokale UI-Einstellung still entfernt.

### 9.3 Normale PIN-UI

Die Plattform erhält ein kleines UI-only PinEntryModel:

- semantische Ziffern 0 bis 9, Rückschritt, Löschen, Abbrechen und
  Übernehmen;
- exakt vier Eingabestellen für die normale lokale Service-PIN-Anzeige;
- maskierte Darstellung ohne Klartext;
- sichtbare Zustände Empty, Incomplete, Pending, RetryWait, Rejected und
  Accepted;
- Retry-/Wartehinweis und gegebenenfalls ein ownergelieferter TextKey kommen
  als Ergebnis des owning Verifiers und werden nur dargestellt;
- PIN-Daten werden nicht persistiert, exportiert, geloggt oder in einem
  Snapshot abgelegt.

Für die native Simulation liefert ein deterministischer, secretsfreier
Test-Seam alle ownergelieferten Anzeigezustände Empty/Incomplete/Pending/
RetryWait/Rejected/Accepted. Er berechnet keine Wartezeit, keinen Backoff und
keine Credentialentscheidung. #26 implementiert damit die normale
PIN-Eingabe und die Darstellung des owning Retrypfads, aber keinen
produktiven Credential- oder Hashvertrag. Der spätere Auth-/Webscope bleibt
bei seinem owning Issue. SAFE_BOOT verwendet die PIN-UI nicht als normalen
Servicezugang.

Die lokale Schutzmatrix wird aus kanonischen Application-, Process- und der
separaten Servicequelle projiziert. Normaler PIN-Service ist nur nach dem
exakten kanonischen Eintrittszustand erreichbar:
`ApplicationLifecycleState::Ready + ProcessState::Standby + owning
Servicefreigabe`. „Kein aktiver Lauf“ allein ist keine Freigabe.

| kanonische Quelle | normaler PIN-Service | Status/Diagnose |
|---|---|---|
| ApplicationLifecycleState::Ready + ProcessState::Standby + owning Servicefreigabe | verfügbar nach PIN-Eingabe | verfügbar |
| ApplicationLifecycleState::Ready außerhalb ProcessState::Standby, einschließlich Completed | gesperrt/unavailable mit Grund | verfügbar, soweit passiv |
| Initializing, Boot, Recovery oder ApplicationLifecycleState::ServiceRequired | gesperrt/unavailable mit Grund | verfügbar, soweit passiv |
| aktiver Lauf inklusive ManualHolding | gesperrt/unavailable | verfügbar |
| ProcessState::Fault | gesperrt/unavailable | verfügbar |
| ProcessState::SafeBoot | gesperrt/unavailable | passive Diagnose/Recovery verfügbar |

Die Tabelle ist eine Projektion bestehender Ownerentscheidungen. #26
klassifiziert keinen Fault und hebt keine Sperre durch PIN-Erfolg auf.
Erfolgreiche PIN-Prüfung erzeugt ausschließlich die lokale
ServiceSessionLease und niemals Safety- oder Aktorfreigabe.

### 9.4 Lokale Servicesitzung

Die bestehende #25 ServiceSessionLease wird unverändert verwendet:

    TOUCH_SERVICE_POLICY=inactivity 10 minutes, absolute NONE
    WEB_SERVICE_POLICY=inactivity 5 minutes, absolute 15 minutes

Nur relevante Bedienhandlungen im geschützten lokalen Servicebereich rufen
RelevantUserActivity auf. Uhr-, Netzwerk-, Sensor-, Message-, Refresh- und
andere Hintergrundupdates verlängern die Lease nicht.

Der Workspace beziehungsweise sein Owner führt bei folgenden Ereignissen
die bestehende ServiceSessionEvent-Zuordnung aus:

- ExplicitSignOut;
- DeviceRestart;
- SafetyStateInvalidated, wenn der bestehende Safety-/Serviceowner die
  sicherheitsrelevante Zustandsinvalidierung meldet.

Der UI-Code klassifiziert keinen Fault und erfindet keine neue
Sicherheitsereignisliste. Bei Leaseende werden geschützte Aktionen gesperrt,
laufende reine UI-Edits nicht still verworfen und Speichern verlangt nach
erneuter PIN-Prüfung. Alle simulierten Nachweise trennen lokale Touch- von
Web-Policy und normaler Webanmeldung.

## 10. Recovery- und Safetyverhalten

### 10.1 Actor-free Recovery

Die folgenden kanonischen Werte werden sichtbar projiziert:

- RecoveryEvaluation und WaitingForTrustedTime;
- CurrentRunRecovered;
- RecoveryRejectedOrFailClosed;
- FallbackSelectionRequired als eigene Recovery-/Bootklassifikation;
- SAFE_BOOT und RunPersistence-untrusted;
- Completed nach Neustart.

Die Anzeige darf dabei:

- den Recoverygrund, trusted-UTC-Wartezustand und verfügbare nächste Aktion
  zeigen;
- den bestehenden Fallback-Resume-Dialog darstellen;
- passive Diagnose anbieten;
- Back/Home als reine Navigation ausführen.

`FallbackSelectionRequired` ist kein SAFE_BOOT-Unterfall. `ResumeFallback`
wird ausschließlich in der dafür kanonisch projizierten
FallbackSelectionRequired-Recoveryansicht angeboten. Ein echter
`ProcessState::SafeBoot` erhält daraus keine Fallback-Resume-Aktion und bleibt
auf den reduzierten aktorfreien Diagnose-/Recoveryumfang begrenzt.

Semantische Ziele der beiden Ansichten werden getrennt behandelt:

| kanonische Ansicht / Ziel | #26-Darstellung |
|---|---|
| `SafeBoot`: passive Diagnose, Persistenz-/Recoverystatus | ausführbar als Read, wenn der bestehende Presentation-/Recoverypfad den Wert liefert |
| `FallbackSelectionRequired`: bestehendes Fallback-Resume | ausführbar ausschließlich über `FermentationApplication::resumeFallback` und die bestehende Bestätigung |
| `SafeBoot`: ResumeFallback | nicht angeboten; kein Fallback-Resume aus SAFE_BOOT |
| beide Ansichten: PIN-Service und Aktortest | immer gesperrt/unavailable; kein Backendpfad |
| beide Ansichten: Vollreset, Netzwerk-Recovery, Export | nur ausführbar, wenn ein bestehender owning Application-/Platform-Aufruf mit Ergebnisvertrag nachgewiesen ist; im aktuellen Baseline-Code ist kein solcher SAFE_BOOT-UI-Pfad öffentlich nachgewiesen, daher typisiert Unavailable mit `OWNER_PATH_MISSING` |
| beide Ansichten: Raw-Touch-Recovery, Kalibrierung, physische Trigger | außerhalb #26 und weiterhin #31/TBD_HARDWARE |

SAFE_BOOT darf nicht:

- einen Lauf automatisch aus einem alten Fallback aktivieren;
- ohne trusted UTC eine Dauer oder lokale Uhrzeit erfinden;
- eine Aktorpermission oder ein Sensorvertrauen aus einer UI-Auswahl
  ableiten;
- `discardAsNoActiveRun` für FallbackSelectionRequired wiederverwenden;
- einen Aktortest oder normalen PIN-Service aus SAFE_BOOT anbieten;
- durch Quittieren eine Fehlerursache oder Sperre beseitigen.

ResumeFallback bleibt die bestehende bestätigte Appaktion. Vor Applied
bleiben FallbackRecoveryPending, RAM-/FSM-Aktivierung und Aktorpermission
unresolved. Erst der bestehende Write-before-Apply-/FSM-/Fresh-Evidence-
Pfad kann die spätere Interlockbewertung erreichen. Für jedes Ziel ohne
nachgewiesenen owning Pfad zeigt die Shell einen typisierten
OWNER_PATH_MISSING-/Unavailable-Grund statt einer scheinbaren ausführbaren
Aktion.

### 10.2 Aktor- und Safetygrenze

Der Simulator besitzt keinen Aktorausgang. Er darf nur abstrakte vorhandene
CommandEffects beziehungsweise Plannerresultate beobachten. Jeder Test
schlägt fehl, wenn aus Navigation, Pressfeedback, PIN-Erfolg, Recoveryanzeige,
Sessionablauf, Displayruhe, WLANstatus oder Headeraktion eine
ActuationInterlock::Allowed-Freigabe behauptet wird.

Boot, Reset, Fault, unbekannter Zustand, fehlende Hardware, SAFE_BOOT und
offene Sicherheitsgates bleiben all-off beziehungsweise unresolved gemäß den
bestehenden Verträgen. #26 ändert keine GPIO-, Board-, Pegel- oder
Adapterannahme.

## 11. Geplante Dateien und Ownership

### 11.0 Externe Vertragsabhängigkeit: Issue #144

Issue #144 ist der konkrete, vor #26 zu planende und zu mergende
Run-Identity-/Provenienzvorgänger. Der #26-Diff enthält keine Dateien,
Schemaänderungen, Codec-/Recoverytests oder Allocatorimplementation dieses
Issues. #26 konsumiert nach dem Merge ausschließlich diese Invarianten:

- `RunProgramSnapshot::sourceProgramRevision` ist der neutrale starke
  `RunProgramSourceRevision`-Typ; neue Starts erhalten ihn an der
  Application-Grenze aus der aktuellen `ProgramCatalogRevision`, während
  Legacy-Provenienz historisch neutral bleibt und unbekannte neuere Schemas
  fail-closed behandelt werden;
- derselbe app-owned `CommandId`-Allocator versorgt Touch und später Web;
  pro Fachrequest sind `UiRequestId.value` und `CommandEnvelope::id`
  identisch, neue Requests unterscheiden sich, und Replays behalten ihre
  Identität;
- alle neuen aktiven Runs, einschließlich `StartProgram`,
  `StartManualHolding`, `AbortAndCool` und `CoolAfterCompletion`, erhalten
  ihre Lauf-ID an der Application-Grenze aus
  `StorageEpoch + StartCommandId`; UI-Payloads liefern keine Identität.

Die owning Run-, Persistence-, Safety-, Recovery- und FSM-Grenzen bleiben
unverändert. Die exakte `Issue #144`-Merge-HEAD-Provenienz wird vor der
#26-Implementation in dieser Planfassung nachgetragen; erst danach beginnt
der #26-Diff. Der vollständige Vorgängerplan und seine konkreten Dateien und
Nachweise bleiben ausschließlich in Issue #144.

### 11.1 Generische Plattform

| Datei | geplante Verantwortung |
|---|---|
| lib/device_platform/src/device_ui_interaction.hpp/.cpp | semantische lokale Ziele, Interaktionsantwort, Pressstatus, generische Header-/Slot-/Pagerziele und Feedbackintents; keine App- oder Rendererbegriffe |
| lib/device_platform/src/device_ui_idle.hpp | kleiner monotonic-time-basierter UI-Idle-Controller mit WakeOnly-Ersttouch; keine Backlightintegration |
| lib/device_platform/src/device_ui_pin.hpp | maskiertes, vierstelliges UI-only PIN-Eingabemodell ohne Verifier, Persistenz oder Credential |
| lib/device_platform/src/device_ui_shell.hpp/.cpp | nur additive Shell-Hilfen, falls für Target-/Slotvalidierung oder exit-blockierte Interaktion erforderlich; bestehende Home/Back- und Extensionverträge bleiben Quelle |

device_platform bleibt frei von ProcessState, FermentationUiSnapshot,
Programmen, FaultCode, RecoveryDisposition, HTML, LVGL, GPIO, ESP-IDF und
device_platform_test_support.

### 11.2 Fermentation-App

| Datei | geplante Verantwortung |
|---|---|
| lib/fermentation_app/src/fermentation_ui_models.hpp/.cpp | additive Home-Modi Waiting/Completed/Restricted sowie kleine Workspace-/Programmlisten-/Detailmodelle, ohne zweiten Domainzustand |
| lib/fermentation_app/src/fermentation_ui_projector.hpp/.cpp | vollständige Home-/Recovery-/Message-/Temperatur-/Statusprojektion aus canonical Ownerwerten; keine Rohwertentscheidung |
| lib/fermentation_app/src/fermentation_touch_workspace.hpp/.cpp | lokale Route, Seiten, vier appseitige Slotinhalte, Pager, Dialoge, Sperrgründe und Mapping auf bestehende Intentformen; kein direkter Owner- oder Persistenzaufruf |
| lib/fermentation_app/src/fermentation_ui_commands.hpp/.cpp | app-owned rendererunabhängige ProductInsertedConfirmed- und Startkandidatenverträge mit echter Katalogrevision, Wiederverwendung von FermentationUiCommandPhase und DecisionOnly-/OwningOutcome-Abbildung über die bestehenden Application-Handoffs; keine künstliche Envelope-/Idempotenzsemantik |
| lib/fermentation_app/src/fermentation_ui_editing.hpp/.cpp | minimale NumericEditModel-/TextEditModel-Aktionen für Rezeptname, Notiz und Werte sowie deterministische freie Benutzerprogramm-ID-Allokation; nur flüchtige Kandidaten, keine Validierungs- oder Keyboardplattform |
| lib/fermentation_app/src/fermentation_ui_text.hpp/.cpp | zusätzliche fermentation-owned TextKeys für Rezepte, Phasen, Aktionen, Status, Service, PIN, Feedback und Sperrgründe in DE/EN/ES |

Die vorhandenen Ownerpfade werden als unveränderte Abhängigkeiten verwendet:
TemperatureControlApplicationOrchestrator für Run-Apply/Persistenz und
Lifecycle-Handoff, FermentationApplication für ApplicationLifecycleState und
Recoveryaktionen, ConfigurationService/ConfigurationPreview für den
Programmkatalog und Configuration-Commit, sowie die bestehende
Process-State-Machine-, Sensor-, Recovery- und Safetylogik. #26 plant keine
Änderung dieser Ownerimplementierungen und keinen direkten Zugriff des
Workspace auf den RunPersistenceCoordinator. Die konkrete Lücke für den
manuellen Zeit-/Temperaturlauf bleibt als `OPEN_OWNER_DECISION` /
`UNOWNED_R1_GAP` und die fehlenden SAFE_BOOT-UI-Aufrufe bleiben als
vorgelagerte Ownerlücke `OWNER_PATH_MISSING`/Unavailable; beides wird nicht in
#26 aufgelöst. Insbesondere ist eine fehlende owning Grenze kein Anlass für
eine offene Erlaubnis, Fach-, Safety- oder Persistenceimplementierungen zu
ändern.

### 11.3 Deterministischer Testsupport

| Datei | geplante Verantwortung |
|---|---|
| lib/device_platform_test_support/src/simulated_device_shell.hpp/.cpp | generischer semantischer Shellsimulator, 320-x-240-Rahmen, Header-/Footer-/Pagergeometrie, Wake-/Press-/Feedbacktrace; keine fermentation-Abhängigkeit |
| test/test_local_touch_ui/test_local_touch_ui.cpp | appseitige vollständige Simulation, canonical state setup, Workspace-/Shell-Komposition, UI-Commandtrace und Szenarioorakel |
| test/test_device_ui_contracts/test_device_ui_contracts.cpp | additive Tests für neue generische Interaktions-, Idle- und PIN-Verträge |
| test/test_fermentation_ui_models/test_fermentation_ui_models.cpp | Home-Modus-, Workspace- und Projektionstests |
| test/test_fermentation_ui_commands/test_fermentation_ui_commands.cpp | Transitionbrücke, Confirmationreihenfolge, Staleness, Duplicate und keine Navigation-IDs |
| test/test_fermentation_ui_editing/test_fermentation_ui_editing.cpp | NumericEditModel-/TextEditModel-Aktionen, Kandidatenlebensdauer und keine Validierungsduplikation |

Nur wenn der gemeinsame Testtreiber tatsächlich eine getrennte wiederverwendbare
Hilfe benötigt, darf eine kleine weitere Datei unter test/ entstehen. Eine
Production-Composition-Root-Datei und eine neue Bibliotheksabhängigkeit werden
nicht angelegt.

### 11.4 Dokumentation

Die bestehenden Fachquellen bleiben die normative Quelle und werden nicht
dupliziert. Die Umsetzung ergänzt in docs/ACCEPTANCE_TESTS.md nur eine
kompakte, nummerierte #26-SIM-Matrix mit Verweisen auf die vorhandenen
Abschnitte. docs/DEVICE_UI_VISUAL_DESIGN.md, die Local-UI-Dokumente und die
Safety-/Recoveryverträge werden nur geändert, wenn der vollständige Diff eine
konkrete veraltete Aussage nachweist. Eine neue UI- oder Renderer-
Dokumentationshierarchie ist ausgeschlossen.

### 11.5 Unveränderte Dateien und Systeme

Unverändert bleiben:

- platformio.ini, CMake-Abhängigkeiten, dependencies.lock und
  ESP-IDF-Komponenten;
- src/main.cpp, main/app_main.cpp und alle Hardwareadapter;
- Board-/GPIO-/Wiring-SSOT und Hardwaredokumente;
- echte Assets, SVGs, Fonts, Backlight- oder Touchkalibrierdateien;
- Web-API, Web-Session, Weblogin und Web-Auth;
- produktive Persistenzschemata, zusätzliche NVS-Records und
  Command-/Session-Queues.

## 12. Logische Umsetzungsschnitte

Die Schnitte beschreiben die technische Reihenfolge der späteren #26-Arbeit.
Jeder Schnitt erhält die in der Akzeptanzmatrix benannten gezielten Nachweise;
fachliche Abweichungen werden als neue Planprovenienz behandelt.

Vor Schnitt 1 muss Issue #144 auf einem eigenen Merge-HEAD abgeschlossen sein.
Danach wird dieser Plan auf genau diesen Merge-HEAD aktualisiert; erst dann
beginnt die #26-Implementation. Das ist eine festgelegte technische
Abhängigkeit, keine offene fachliche Entscheidung innerhalb des normalen
Programmstarts.

1. Generische Interaktion: semantische Targets, vier Slotindices,
   VerticalPager, Pressfeedback, Feedbackintents und additive Shellprüfung.
   Kein Commandaufruf in device_platform.
2. Idle und PIN: monotonic UI-Idle mit WakeOnly-Ersttouch, semantische
   Systemweckung, maskiertes vierstelliges PinEntryModel und
   secretsfreier Testverifier mit Pending-/RetryWait-Projektion.
   ServiceSessionLease bleibt die #25-Autorität.
3. Workspace-Projektion: expliziter ApplicationLifecycleState,
   deterministische Home-Moduspriorität, gemeinsame Snapshotnutzung,
   vollständige lokale Routen, Status-/Service-Plattformbereiche und
   isolierte App-Erweiterungen. TextKeys statt sichtbarer Literale.
4. Kontextaktionen: Slot-0-Matrix ohne implizite Programmauswahl,
   ManualHolding-Einstieg, ProductInsertedConfirmed-Contract,
   DecisionOnly-vs-Application-Apply, bestehende Orchestrator-Handoffs und
   strukturierte Dialoge/Sperrgründe.
5. Rezepte und Laufdetails: Startzusammenfassung, numerische und textuelle
   Kandidateneditoren, next-run-only-Werte, unveränderlicher Run-Snapshot,
   Bearbeiten, Kopieren, Neu, zweistufiges Löschen, Programmliste, Pager,
   Prozess-/Technikseite, Meldungs-/Quittier-/Mute-/Stop- und
   Abschlussabläufe über bestehende Commands und ConfigurationPreview.
6. Recovery, Header und Start: Splash/Startstatus, Headerziele,
   actor-free Recovery, ausführbare versus unavailable SAFE_BOOT-Ziele,
   FallbackSelectionRequired, WaitingForTrustedTime und sichtbare
   Restricted-Sperrgründe.
7. Simulation und Nachweis: generic test-support shell, 320-x-240-Frame,
   vollständige appseitige Szenarien einschließlich aller Ownerlücken,
   Layout-/Text-/Locale-/Theme- und Fehlbedienungsorakel; keine
   Hardwarepfade.
8. Issue-spezifische Dokumentation: SIM-Matrix und notwendige
   Architektur-/Secret-/Diffchecks gegen Issue, #25-Vertrag, ADR-013 und
   die bestehende Ownergrenze.

Die konkrete spätere Implementierung bleibt auf diese fachlichen Pfade
begrenzt. Fehlende Ownerverträge werden als BLOCKED/Unavailable
nachgewiesen und nicht durch einen zusätzlichen Dispatcher- oder Fachpfad
ersetzt.

## 13. Simulations- und Akzeptanzmatrix

Die folgenden Fälle sind verpflichtende native Nachweise. Sie sind
deterministisch mit expliziter virtueller Zeit auszuführen. Ein nicht
ausgeführter Fall bleibt NOT_RUN und ist kein PASS.

| ID | Nachweis |
|---|---|
| SIM-26-01 | Home ohne expliziten Kandidaten zeigt genau vier Slots; Slot 0 öffnet Programm wählen, startet nicht das letzte Programm und erfindet keinen Datensatz |
| SIM-26-02 | `Initializing` ergibt `Unavailable`/Startstatus; `ServiceRequired` ergibt vor jeder Recovery-/ProcessState-/Messageauswertung `Restricted`; nur bei `Ready` werden Recovery, Completed, Waiting, ActiveRun und Standby projiziert, mit der engen offenen decisionRequired-Regel; Unavailable bleibt technischer Fallback |
| SIM-26-03 | Initializing zeigt Splash/kompakten nichtblockierenden Startstatus, Ready plus Standby zeigt Standby/Bereit und ServiceRequired wird ausschließlich als Restricted projiziert; kein Modus wird aus ready oder Faulttext erraten |
| SIM-26-04 | Headerflächen für Sprache, WLAN und Uhrzeit sind nicht überlappend; Sprache nutzt enthaltene Locale, WLAN den Statusindikator und Uhr die ClockViewInput ohne erfundene Lokalzeit |
| SIM-26-05 | Home/Back-Hierarchie: erste Unterebene Home, tiefere Ebene Back, genau ein Segment pro Back, ExitRequirement verhindert stilles Verwerfen |
| SIM-26-06 | Rezepte-Listen verwenden große Einträge und Pager; Standardprogramme stehen vor Benutzerprogrammen, ungültige Programme sind mit Grund gesperrt |
| SIM-26-07 | Ohne expliziten Kandidaten führt Programm wählen deterministisch zur Liste, eine Auswahl mit echter ProgramCatalogRevision zur Startzusammenfassung und erst danach zu Vorheizen/Start; kein letzter oder impliziter Datensatz wird gestartet |
| SIM-26-08 | Vorheizen/Start verwendet den bestehenden ProgramStart-Pfad; Produkt eingesetzt verwendet den app-owned ProductInsertedConfirmed-Contract, decideProcessTransition und den TemperatureControlApplicationOrchestrator ohne zweite FSM |
| SIM-26-09 | Aktiver Lauf zeigt Stop im ersten Slot, Rezepte im zweiten, Prozess-/Technikdetails und aktuelle Snapshotwerte; Stop bietet Ausschalten oder Kühlen |
| SIM-26-10 | Abschluss zeigt OK/Details und optional Jetzt kühlen; Kühlen erzeugt nur über den bestehenden Completion-Pfad einen neuen manuellen Lauf |
| SIM-26-11 | Meldungsbanner zeigt höchste Priorität; Quittieren beseitigt Ursache nicht, Stummschalten beendet nur akustisches Signal, Faultreset bleibt getrennt |
| SIM-26-12 | Status und Service zeigen Plattformseiten vor App-Erweiterungen; eine unavailable Appsection isoliert Plattformstatus und andere Appsections; SAFE_BOOT bietet keinen normalen PIN-Service |
| SIM-26-13 | Lange Listen/Informationen navigieren ausschließlich über sichtbare Auf-/Ab-Ziele; kein Wischen, keine horizontale Route und keine Domainmutation |
| SIM-26-14 | Pressfeedback wird für jedes erkannte Ziel sichtbar; disabled action zeigt Sperrgrund; kritische Aktionen verlangen strukturierte Confirmation und sind nicht doppelt auslösbar |
| SIM-26-15 | Erster Touch im Dimmed/Asleep-Zustand erzeugt WakeOnly und kein Command; zweiter Touch kann erst danach das Ziel auslösen |
| SIM-26-16 | Hintergrundupdates und Refreshrevisionen verlängern keine Lease und lösen keinen Command aus; relevante Servicebedienung verlängert nur die lokale Lease |
| SIM-26-17 | PIN-UI maskiert vier Stellen, akzeptiert Ziffern/Backspace/Clear/Cancel/Submit, zeigt Pending/RetryWait/Rejected/Accepted und speichert keinen Secretwert |
| SIM-26-18 | Lokale Servicelease läuft nach 10 Minuten Inaktivität ab, ohne absolute Maximaldauer; ExplicitSignOut, DeviceRestart und SafetyStateInvalidated sperren sie sofort |
| SIM-26-19 | Web-5/15-Servicepolicy bleibt von Touch-10/ohne-Absolutlimit und normalem Weblogin getrennt |
| SIM-26-20 | WaitingForTrustedTime bleibt RAM-only und actor-free; Current-Recovery zeigt keine erfundene Dauer oder Zeit und Fallback bleibt Auswahlangebot |
| SIM-26-21 | Die eigene FallbackSelectionRequired-Ansicht erlaubt nur die bestehende bestätigte ResumeFallback-Aktion; Back/Home und Quittieren mutieren weder Persistenz noch Recoveryangebot |
| SIM-26-22 | Ein tatsächlicher SAFE_BOOT zeigt passive Diagnose/Recoveryhinweise ohne ResumeFallback; fehlende Vollreset-/Netzwerk-/Exportpfade erscheinen als Unavailable, PIN-Service/Aktortest bleiben gesperrt |
| SIM-26-23 | Recovery, PIN-Erfolg, Headeraktion, WLANstatus, UI-Ruhe und Pressfeedback können nie ActuationInterlock::Allowed erzeugen |
| SIM-26-24 | Vollständiger 320-x-240-Frame: Viewport, Header, Content, Footer, vier gleich breite Slots, Pagerbereich und Nichtüberlappung werden deterministisch geprüft |
| SIM-26-25 | Splashrahmen ist proportional/zentriert modelliert, statisch fallbackfähig und ohne Asset-, Renderer- oder Framebufferimplementierung |
| SIM-26-26 | DE/EN/ES-Textpacks liefern alle #26-Keys; aktive Locale fällt auf Englisch, dann sichtbaren technischen Key; Theme bleibt semantisch und dark-only R1 |
| SIM-26-27 | unveränderte fachliche Domainrevision plus reine Clock-/Network-/Refreshänderung lehnt keinen gültigen Command wegen UI-Refresh ab |
| SIM-26-28 | stale, invalid, safety-rejected, busy, unavailable und duplicate Ergebnisse bleiben typisiert; fehlende Confirmation maskiert keine kanonische Ablehnung |
| SIM-26-29 | UI-Reads, Navigation, Pager, Header und Wake erzeugen keine UiRequestId, keinen CommandEnvelope und keine Persistenzmutation |
| SIM-26-30 | Laufende Prozesse, persistierte COMPLETED-Zustände, Sensorersatz, Stopoptionen, Quittierung, Abschluss und Fehlerreset werden über canonical projections dargestellt; kein Screen berechnet Fachzustand aus Rohwerten |

Die folgenden F2-Regressionsfälle sind zusätzlich verpflichtend und müssen im
Simulationstrace die Entscheidung und den tatsächlichen Apply-/Persistenzpfad
getrennt ausweisen:

| ID | Nachweis |
|---|---|
| SIM-26-31 | Passende Revision: WaitingForProduct -> ProductInsertedConfirmed -> ReachingTarget läuft über den app-owned Intent, decideProcessTransition und TemperatureControlApplicationOrchestrator::persistTransition |
| SIM-26-32 | DecisionStatus::Proposed wird als DecisionOnly traciert; ein anschließender Fehler aus persistTransition lässt RAM-/Prozesszustand unverändert beziehungsweise fail-closed und erzeugt keinen vorgezogenen Lifecycle-Handoff |
| SIM-26-33 | Stale expectedStateSequence liefert CommandStatus::StaleState im gemeinsamen FermentationUiCommandResult mit Phase DecisionOnly und erzeugt keinen Decide- oder Persistenzaufruf |
| SIM-26-34 | Ein Replay nach erfolgreichem ProductInsertedConfirmed erzeugt wegen der verbrauchten Zustandsrevision oder der kanonischen Zustandsablehnung keine zweite Nebenwirkung |
| SIM-26-35 | Der Workspace enthält keinen direkten Aufruf von RunPersistenceCoordinator::persistTransition; Architektur-/Diffprüfung bestätigt den Application-Orchestrator als einzige Run-Mutationsgrenze |
| SIM-26-36 | CommandStatus::Proposed und DecisionStatus::Proposed erscheinen zunächst ausschließlich als DecisionOnly ohne Apply; erst das getrennte Ergebnis von persistCommand, persistFreshStartCommand, persistTransition oder persistSensorSelection erscheint als OwningOutcome |
| SIM-26-48 | Passende Revision mit kanonisch abgelehntem ProductInsertedConfirmed-Übergang bewahrt exakt einen der zulässigen `TransitionDecision::status`-Werte `Proposed`, `NoTransition`, `Rejected`, `InvalidInput` oder `TimeWentBackwards` als DecisionOnly und ruft bei jedem nicht-proposed Wert keine persistTransition-Funktion auf |

Weitere #26-Pfade werden deterministisch separat geprüft:

| ID | Nachweis |
|---|---|
| SIM-26-37 | ManualHolding ist über Manueller Betrieb, ManualRunPlanRequest, decideManualStart und TemperatureControlApplicationOrchestrator::persistFreshStartCommand bedienbar; es wird kein direkter Aktorpfad erzeugt |
| SIM-26-38 | ManualHolding ist owning bedienbar, der in REQUIREMENTS.md genannte manuelle Zeit-/Temperaturlauf bleibt als OPEN_OWNER_DECISION/UNOWNED_R1_GAP mit OWNER_CONTRACT_MISSING/Unavailable sichtbar und wird weder simuliert noch als R1-erledigt verbucht |
| SIM-26-39 | NumericEditModel unterstützt Plus/Minus, direkte Ziffern, Dezimaltrennzeichen, Backspace, Clear, Cancel und Commit; Grenzen und Gültigkeit kommen ausschließlich vom owning Validator |
| SIM-26-40 | TextEditModel unterstützt Zeichen-/Modusaktionen, Rückschritt, Löschen, Abbrechen und Übernehmen für Programmname und kurze Notiz; kein Keyboardframework wird benötigt |
| SIM-26-41 | Bearbeiten einer Standard-Arbeitskopie und eines Benutzerprogramms verwendet flüchtige Kandidaten und ConfigurationPreview/ConfigurationService für den dauerhaften Commit; active RunProgramSnapshot und unveränderliche Factory-Vorlage bleiben unverändert |
| SIM-26-42 | Standard- und Benutzerprogramm können kopiert werden; Zurücksetzen ersetzt nur die Standard-Arbeitskopie durch die Factory-Vorlage; Neu speichert ein gültiges, aber noch nicht `Runnable`-fähiges `CatalogTemplate` über den bestehenden ConfigurationService-Pfad sichtbar nicht startbereit, sperrt `START` typisiert und wird nach Vervollständigung über denselben Pfad startfähig; die vier reservierten Factory-Einträge bleiben im Katalog |
| SIM-26-43 | Löschen erfordert zwei Bestätigungen; ein aktuell verwendetes oder laufendes Programm ist nicht löschbar und erzeugt keinen Commit; Standard-Deinstallation entfernt weder Factory-ID noch reservierten Eintrag physisch, setzt die bestehende installed/userDeletable-Semantik um, blendet `installed == false` aus der aktiven Liste aus und sperrt den Start |
| SIM-26-44 | Echte ProgramCatalogRevision zwischen Lesen und Commit ergibt StateChanged, PreviewSuperseded oder PreviewNotFound typisiert; kein stilles Überschreiben und kein UI-Refresh-Surrogat |
| SIM-26-45 | PIN-Service ist ausschließlich bei Ready + validiertem ProcessState::Standby + owning Servicefreigabe erreichbar; Completed, Boot, Recovery, ServiceRequired, aktiver Lauf, Fault und SAFE_BOOT bleiben gesperrt, Status/Diagnose bleiben passiv erreichbar |
| SIM-26-46 | PIN-Accepted eröffnet nur die lokale ServiceSessionLease; keine Safety-/Aktorfreigabe und keine Veränderung von ActuationInterlock |
| SIM-26-47 | SAFE_BOOT und FallbackSelectionRequired werden getrennt nachgewiesen: SafeBoot bietet Read beziehungsweise OWNER_PATH_MISSING/Unavailable, aber kein ResumeFallback; nur FallbackSelectionRequired bietet den bestehenden Resume-Pfad; Raw-Touch, Kalibrierung und physische Trigger bleiben außerhalb #26 |

Die Restkorrekturen erhalten zusätzlich folgende direkte, beobachtbare
Nachweise:

| ID | Nachweis |
|---|---|
| SIM-26-49 | Der korrigierte `fromCommandStatus`-Reusepfad weist `NotConfirmed`, `StaleState`, `InvalidInput`, `SafetyRejected`, `NotAllowedInState`, `ContextMissing`, `CapacityReached`, `NoChange`, `AlreadyProcessed` und `Proposed` aus einem reinen Decide-Aufruf sämtlich als `DecisionOnly` nach; kein Statusname wird als Apply missverstanden |
| SIM-26-50 | ProductInsertedConfirmed mit stale expectedStateSequence liefert das gemeinsame `CommandStatus::StaleState` als `DecisionOnly`; passende Revision plus kanonisch abgelehnter `TransitionDecision::status` bleibt `DecisionOnly`; nur `Proposed` darf `persistTransition` erreichen und dessen Ergebnis erscheint getrennt als `OwningOutcome` |
| SIM-26-51 | Ein Persistenzfehler nach `DecisionStatus::Proposed` lässt RAM-/Prozesszustand unverändert beziehungsweise fail-closed; ein Replay nach erfolgreichem Apply erzeugt keine zweite Nebenwirkung |
| SIM-26-52 | Ein gültiger next-run-only-Kandidat löst Programm-ID und echte Katalogrevision aus dem aktuellen Snapshot auf, validiert die Overrides und erzeugt erst über den bestehenden Start-/Orchestratorpfad den Run-Snapshot; Quellprogramm und aktiver Katalog bleiben unverändert |
| SIM-26-53 | Unveränderter Katalog akzeptiert gültige Overrides, geänderte `ProgramCatalogRevision` wird stale abgelehnt und ungültige Overrides verwenden das owning InvalidInput-/Sperrresultat; der neutrale `RunProgramSourceRevision`-Wert wird für neue Starts an der Application-Grenze exakt aus der aktuellen Katalogrevision erzeugt, ohne Legacy-Umdeutung, Cast/Truncation/Surrogat oder per-program Revision |
| SIM-26-54 | Die deterministische ID-Allokation für Kopieren/Neu liefert eine gültige freie Benutzer-ID ohne editierbares ID-Feld; Kollision, ungültige Katalogbasis oder Kapazitätsende liefern typisiertes Unavailable/Capacity ohne Überschreiben oder persistierten Zähler, und eine geänderte Kataloggeneration wird stale abgelehnt |
| SIM-26-55 | NumericEditModel und TextEditModel decken alle vorgesehenen Plus/Minus-, Ziffern-/Dezimal-, Zeichen-/Modus-, Rückschritt-, Löschen-, Abbrechen- und Übernehmen-Aktionen ab; nur der owning Validator entscheidet Grenzen und Commit |
| SIM-26-56 | Der secretsfreie PIN-Test-Seam projiziert Pending und RetryWait sowie Accepted/Rejected; normaler Servicezugang entsteht nur aus validiertem Standby, PIN-Erfolg verändert keine Safety-/Aktorfreigabe |
| SIM-26-57 | ManualHolding durchläuft den bestehenden owning Startpfad; der fehlende Zeit-/Temperaturlauf zeigt die beiden OPEN_OWNER_DECISION-Optionen und bleibt unavailable; SafeBoot und FallbackSelectionRequired behalten ihre getrennten ausführbaren beziehungsweise unavailable Ziele |
| SIM-26-58 | Ein Editor aus Katalogrevision A wird nach Änderung auf B unmittelbar vor `beginPreview()` als typisierter Stale-/Conflictfall abgelehnt; die neue Arbeitskopie bleibt unverändert und es entsteht keine Preview-Mutation |
| SIM-26-59 | Die Kombination `ApplicationLifecycleState::ServiceRequired` plus verbliebene Recoverydaten projiziert deterministisch `Restricted`, nicht `Recovery`; bei `Ready` wird derselbe Recoverydatensatz als `Recovery` projiziert |
| SIM-26-60 | `UnsupportedAppDetail` und jede andere bridge-eigene Ablehnung ohne owning Mutation werden als `DecisionOnly` projiziert; `makeResult()` liefert dafür keinen `OwningOutcome` und es gibt keinen dritten Phasenwert |
| SIM-26-61 | StartProgram, StartManualHolding, AbortAndCool und CoolAfterCompletion erhalten ihre Lauf-ID ausschließlich an der Application-Grenze; die vier UI-Intents tragen nur erlaubte Werte, keine `runId`/`CommandId`, und Confirmation-Replay behält die vorbereitete Identität |

Vor der #26-Implementation wird ausschließlich die externe
Vertragsabhängigkeit geprüft:

| ID | Nachweis |
|---|---|
| DEP-26-01 | Issue #144 ist gemergt, und der exakte Merge-HEAD ist als `RUN_IDENTITY_PREDECESSOR_MERGE_HEAD` in der aktualisierten #26-Planprovenienz eingetragen; der neutrale `RunProgramSourceRevision`- und kompatible Run-Persistence-Vertrag ist verfügbar |
| DEP-26-02 | Die Application-Grenze stellt den gemeinsamen Command-/Run-Identity-Vertrag für alle vier neuen Runpfade bereit; UI-Payloads können weder `CommandId` noch `runId` einschleusen, und #26 enthält keinen Vorgänger-Allocator oder Codecpfad |

Zusätzlich werden die in docs/ACCEPTANCE_TESTS.md bereits geforderten
zustands- und safetybezogenen Simulationen gezielt wiederverwendet:

- Standby -> Preheating -> WaitingForProduct -> ReachingTarget ->
  QualifyingTarget -> Fermenting;
- luftgeführter Lauf ohne Produktfühler;
- Produktfühlerausfall und bereits definierter Luft-Ersatzbetrieb;
- Heizen, neutral, Kühlen und Richtungswechsel nur als bestehende
  abstrakte Anforderungen, ohne neue UI-Safetyentscheidung;
- Unterbrechung, Resetcause, Current-Recovery, Completed und SAFE_BOOT;
- kritischer Persistenzfehler, unvollständiger Marker und
  Fallback-Recovery, soweit der bestehende Testpfad sie bereitstellt;
- Benutzerentscheidung bei WARNING_REQUIRES_ACTION, Quittieren ohne
  Fehlerreset und keine Freigabe aus SAFE_BOOT.

Die Simulation behauptet keine elektrische, thermische oder funktionale
Hardwareverifikation. Solche Nachweise bleiben BLOCKED/PENDING bei den
owning Hardware- und Commissioning-Issues.

## 14. Tests und Qualitätsnachweise

### 14.1 Planung

In dieser Planungsphase werden keine Builds und keine produktiven Testläufe
ausgeführt. Zulässig sind Roadmap-, Diff-, Link- und Dokumentationsprüfungen.
Der Planstatus bleibt:

    NATIVE_TESTS=NOT_RUN
    ESP_IDF_BUILD=NOT_RUN
    HARDWARE_TEST=NOT_RUN

### 14.2 Gezielte Umsetzungstests

Die konkreten #26-Testziele sind:

    pio test -e native --filter test_device_ui_contracts
    pio test -e native --filter test_fermentation_ui_models
    pio test -e native --filter test_fermentation_ui_commands
    pio test -e native --filter test_fermentation_ui_editing
    pio test -e native --filter test_local_touch_ui

Die tatsächliche Filterform wird gegen die PlatformIO-Testnamen im finalen
Diff geprüft. Je nach geänderten Shared Contracts kommen nur die direkt
betroffenen bestehenden Tests hinzu, insbesondere:

- test_run_commands;
- test_process_state_machine;
- test_configuration_documents und test_configuration_service;
- test_run_persistence_coordinator;
- test_boot_classification und test_actuation_interlock.

Zusätzliche gezielte Prüfungen nach docs/CI_AND_QUALITY_GATES.md:

    clang-format --dry-run --Werror <geänderte C++-Dateien>
    python scripts/check_architecture_boundaries.py
    python scripts/check_secrets.py
    git diff --check

Die neue generische Plattform bleibt ohne fermentation_app-Abhängigkeit, der
Workspace bleibt ohne konkrete Adapterabhängigkeit und der Testsupport bleibt
ohne Produktionsabhängigkeit. Architektur- und Secretchecks beziehen sich
daher insbesondere auf diese #26-Grenzen.

### 14.3 Nicht ausgeführte Nachweise

In dieser Planrevision bleiben native Volltests, ESP-IDF-bringup/release,
Hardware-Smokes, Display-/Touchtests, WLAN-/RTC-Tests und physische
PIN-/Kalibrierungstests NOT_RUN. Sie werden nicht als PASS behauptet. Die
zugehörige allgemeine Test- und Review-Governance steht ausschließlich in
docs/CI_AND_QUALITY_GATES.md und docs/AGENT_WORKFLOW.md.

## 15. Risiken und issue-spezifische Grenzen

| Risiko/Grenze | Planbehandlung |
|---|---|
| Shell und Workspace könnten wieder gekoppelt werden | generische Shell liefert nur semantische Ziele; fermentation_app besitzt die Workspaceaktion; Testsupport hat keine Appabhängigkeit |
| Layout könnte still einen späteren Renderer vorwegnehmen | nur SimulationRect/Frame-Deskriptor, designreferenzierte Headergeometrie und 320 x 240; kein Pixelbuffer, DMA oder Widgetbaum |
| Home-Modus könnte aus Einzelwerten geraten werden | additive Projector-Matrix aus ProcessState, RecoveryDisposition, Lifecycle und bestehender Message-/Serviceprojektion |
| Confirmation könnte stale oder Safetyfehler maskieren | canonical decide*/Preview-/Recoveryvalidierung zuerst; nur NotConfirmed/ReadyForConfirmation wird zur ConfirmationRequired-Projektion |
| ProductInserted könnte eine zweite FSM oder ein direkter Persistenzpfad werden | kleiner app-owned Intent mit erwarteter Zustandsrevision; genau eine TransitionRequest über decideProcessTransition und TemperatureControlApplicationOrchestrator::persistTransition; kein direkter RunPersistenceCoordinator-Aufruf, keine eigene Topologie |
| UI könnte Proposed als ausgeführte Mutation melden | Resultphase trennt DecisionOnly von OwningOutcome; Simulation und UI dürfen Proposed nie als Persistenz-/Apply-Nachweis markieren |
| UI könnte die Application-Handoff-Grenze umgehen | Workspace-Contract und Architekturprüfung erlauben Run-Mutationen nur über persistCommand/persistFreshStartCommand, persistTransition und persistSensorSelection des TemperatureControlApplicationOrchestrator |
| Rezepte könnten active snapshots ändern | IDs und Kandidaten nur über bestehende ConfigurationService-/Start-/Snapshotpfade; aktiver Snapshot bleibt read-only |
| PIN-UI könnte Authentisierung vorwegnehmen | nur masked PinEntryModel und secretsfreier Ergebnis-Seam; kein Credential-, Hash- oder Persistenzvertrag |
| Session könnte vom Hintergrund verlängert werden | nur RelevantUserActivity aus geschützter Touchbedienung; Restart, Logout und Ownerinvalidierung terminal |
| Fallback könnte aktivierend wirken | Fallback bleibt SelectionRequired, actor-free und unverändert bis bestehendem Applied-/FSM-/Fresh-Evidence-Handoff |
| unbekannte UIwerte könnten als Recoveryfehler erscheinen | generisches Unavailable/technischer Key; nur kanonische Recoverywerte verwenden Recoveryprojektion |
| Text- und Themewerte könnten dupliziert werden | bestehende getrennte Packs, resolver- und theme-Fallback; keine screenlokalen Literale oder Hexwerte |
| Simulation könnte Hardware-PASS vortäuschen | Testtrace erlaubt nur abstrakte Beobachtung; Hardware-/elektrische-/thermische Ergebnisse bleiben NOT_RUN/PENDING/BLOCKED |

Issue-spezifische technische Grenzen:

1. Die Implementation bleibt bis zur Freigabe der exakten PLAN_COMMIT
   NOT_STARTED.
2. Keine Renderer-, Treiber-, Touch-, Kalibrierungs-, Asset-, GPIO- oder
   Backlightentscheidung wird in #26 eingeführt.
3. Kein normaler Auth-/Credential-Backend und kein zweiter
   Application-/Persistence-/Safety-/Recoverypfad wird in #26 eingeführt.
4. Der manuelle Zeit-/Temperaturlauf bleibt als
   OPEN_OWNER_DECISION/UNOWNED_R1_GAP und
   OWNER_CONTRACT_MISSING/Unavailable vorgelagert; die fehlenden
   SAFE_BOOT-UI-Aufrufe bleiben OWNER_PATH_MISSING/Unavailable.
5. Der bestehende Application-Orchestrator, ConfigurationService und
   Recovery-/Safetyowner bleiben unveränderte Ownergrenzen. Run-Identität und
   neutrale `RunProgramSourceRevision`-Provenienz sind im separaten
   vorgelagerten Issue #144 verbindlich festgelegt und werden vor #26-
   Implementation gemergt; #26 verändert diesen persistierten
   Vertrag nicht und verwendet keinen Cast, kein Surrogat und keinen stillen
   Ersatzpfad.

Die allgemeine Workflow-, Review-, Ready-, Merge-, CI- und Handover-Governance
wird nicht im Issue-Plan dupliziert; sie bleibt in den kanonischen
AGENT-/Workflow-/Quality-Gate-Dokumenten.

## 16. Planprovenienz und PR-Referenzen

Die Planprovenienz dieses PRs besteht aus genau acht eigenen Commits:

1. Roadmap-Sync-Commit 28a35b610020513460690d4b05e90bdec88e81d8;
2. ursprünglicher vollständiger Plan-Commit
   c57be99bdce9d55ebb65b4c4c06e5210e84b7ed9;
3. vorige F1/F2-Planrevision
   2fbe85f41c2461575331c4c3afed73a447302d43;
4. vorherige F1-F10-Planrevision d8a0a70983a6641e1edf27be08d655572164d995d;
5. die vorherige F1-F10-Restkorrektur
   dd64d92745ed7ad1b0e744a0e02e4e5b09cec3b9;
6. die vorherige F1-F6-Letztkorrektur
   3de1d86180413896294f09bcfddb447df8c1e898;
7. Roadmap-Synchronisierung für den konkreten Pflichtvorgänger #144
   7bd0b6fe7ac7fc0f11505d1b1cd3b38d9f1fb714;
8. diese F1-F6-Letztkorrektur mit exakter SHA nach dem Commit.

Issue #25 ist live CLOSED und liefert weiterhin den gemergten #25-Vertrag als
Basis. Issue #144 ist der konkrete Roadmap-geführte Pflichtvorgänger; für die
Planprovenienz von Draft-PR #143 gelten die folgenden issue-spezifischen
Referenzen:

    ROADMAP_COMMIT=7bd0b6fe7ac7fc0f11505d1b1cd3b38d9f1fb714
    PLAN_PATH=docs/tasks/issue-26-local-touch-shell-plan.md
    SUPERSEDES_PLAN_COMMIT=3de1d86180413896294f09bcfddb447df8c1e898
    PLAN_COMMIT=<exakte SHA dieses Plan-Commits>
    PR_HEAD=<exakte SHA dieses Plan-Commits>
    IMPLEMENTATION=NOT_STARTED
    OWNER_PLAN_APPROVAL_REQUIRED=YES
    ACTUATOR_RELEASE=NO

Der aktuelle PR-HEAD und der aktuelle Plan-Commit werden nach dem Push mit
ihren exakten SHAs in der PR-Provenienz und im gemäß
docs/AGENT_WORKFLOW.md geführten Handover referenziert:

    ISSUE=26
    PR=143_DRAFT
    BRANCH=feature/issue-26-local-touch-shell
    HEAD=<exakte PR-HEAD-SHA nach Planpush>
    ROADMAP_COMMIT=7bd0b6fe7ac7fc0f11505d1b1cd3b38d9f1fb714
    SUPERSEDES_PLAN_COMMIT=3de1d86180413896294f09bcfddb447df8c1e898
    PLAN_COMMIT=<exakte SHA dieses Plan-Commits>
    IMPLEMENTATION=NOT_STARTED
    TESTS=NOT_RUN_PLANNING_ONLY
    ACTUATOR_RELEASE=NO
    OPEN_GATE=OWNER_APPROVAL_OF_EXACT_PLAN_COMMIT

Die allgemeine Handover-Formatierung, Draft-/Ready-/Merge- und
Issue-Schluss-Governance sowie die übrigen Workflowregeln bleiben ausschließlich
in docs/AGENT_WORKFLOW.md und werden hier nicht als zweite Prozesswahrheit
gepflegt.

## 17. Definition of Done für die spätere #26-Umsetzung

- #26 nutzt den gemergten #25-Vertrag auf BASE_SHA und enthält keine zweite
  UI-Plattform.
- Shell und Fermentations-Workspace sind getrennt und über semantische
  rendererunabhängige Zustände verbunden.
- Home zeigt Bereit, Aktiv, Wartet, Abgeschlossen, Eingeschränkt und
  Recovery eindeutig aus canonical Ownerwerten.
- Header, Splash, genau vier Slots, Home/Back, Pager, Pressfeedback,
  Sperrgründe, Confirmation, PIN-UI, Sessionlock, UI-Ruhe und Feedbackintents
  sind nativ reproduzierbar geprüft.
- Rezepte, Startzusammenfassung, next-run-only-Werte, Stop-/Completion-/
  Message-/Recoveryaktionen und Status-/Service-Erweiterungen verwenden die
  bestehenden Fach-, Application- und ConfigurationServicepfade; Proposed
  sowie alle sonstigen reinen Decide-Ergebnisse sind nicht als ausgeführte
  Mutationen markiert; der gemeinsame `fromCommandStatus`-Pfad weist sie als
  `DecisionOnly` aus und erst owning Apply-/Persistenz-/Commitresultate als
  `OwningOutcome`.
- ProductInsertedConfirmed verwendet den app-owned Contract mit erwarteter
  Zustandsrevision, decideProcessTransition und ausschließlich
  TemperatureControlApplicationOrchestrator::persistTransition; der
  Workspace ruft keinen RunPersistenceCoordinator direkt auf.
- Startänderungen tragen nur den kleinen next-run-only-Kandidaten mit
  Programm-ID, echter ProgramCatalogRevision und erlaubten Overrides; der
  aktive Katalog bleibt unverändert. Der Run-Identity-/Provenienzvertrag aus
  Issue #144 ist vor der #26-Implementation gemergt; der Run-Snapshot
  verwendet den neutralen `RunProgramSourceRevision`-Vertrag, neue Starts
  leiten ihn an der Application-Grenze aus dem verwendeten
  `ProgramCatalogRevision`-Stand ab, ohne per-program Revision, Cast oder
  Surrogat. Die alten unterstützten Run-Schemas bleiben lesbar und unbekannte
  neuere Schemas fail-closed.
- Standardprogramme sind bearbeitbare Katalog-Arbeitskopien; Factory-Vorlage,
  Zurücksetzen, Kopieren, Neu, Deinstallation und Löschung folgen den
  bestehenden installed-/userDeletable- und ConfigurationService-Verträgen.
  Kopieren/Neu verwenden eine deterministische freie Benutzer-ID ohne
  Zählerpersistenz oder Registry.
- der erste Wake-Touch führt keinen Command aus; reine Navigation und Reads
  mutieren nichts; ein aktiver Run-Snapshot bleibt unverändert.
- Der normale PIN-Service ist ausschließlich an
  Ready + validiertes ProcessState::Standby + owning Servicefreigabe gebunden;
  SAFE_BOOT und FallbackSelectionRequired bleiben getrennte actor-free Pfade.
- ManualHolding ist bedienbar; der manuelle Zeit-/Temperaturlauf bleibt bis zur
  Ownerentscheidung als `UNOWNED_R1_GAP` unavailable und wird nicht als R1-
  Funktion gezählt.
- lokale Servicelease ist 10 Minuten inaktivitätsbegrenzt, ohne absolute
  R1-Maximaldauer, und wird bei Logout, Restart und Ownerinvalidierung
  verworfen; Websession bleibt getrennt.
- Recovery und SAFE_BOOT bleiben sichtbar und aktorfrei; keine UIaktion
  erzeugt oder behauptet eine Aktorfreigabe.
- die 320-x-240-Simulationsframes und alle SIM-26-Nachweise bestehen; nicht
  ausgeführte Hardware-/ESP-IDF-/Full-Run-Nachweise bleiben korrekt als
  NOT_RUN/BLOCKED/PENDING dokumentiert.
- keine Renderer-, Treiber-, Touchrohwert-, Kalibrierungs-, DMA-,
  Framebuffer-, Backlight-, Bibliotheks-, GPIO-, Hardware- oder
  Produktionsassetänderung ist enthalten.
- alle issue-spezifischen SIM-, Ownerpfad- und Architekturgrenzen sind im
  Implementierungsnachweis eindeutig als PASS, BLOCKED, PENDING oder
  NOT_RUN dokumentiert; die allgemeine Review-/Ready-Governance bleibt in
  den kanonischen Workflow-Dokumenten.
