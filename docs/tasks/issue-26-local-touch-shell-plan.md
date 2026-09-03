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
    ROADMAP_COMMIT=28a35b610020513460690d4b05e90bdec88e81d8
    PLAN_PATH=docs/tasks/issue-26-local-touch-shell-plan.md
    PLAN_COMMIT=THIS_COMMIT
    PLAN_REVISION=F1_F2_PLAN_FINDINGS
    SUPERSEDES_PLAN_COMMIT=c57be99bdce9d55ebb65b4c4c06e5210e84b7ed9
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
    ROADMAP_F1_ACTION=NO_CHANGE_OWNER_CLOSED

Vor diesem Plan-Commit wurden live geprüft:

- der Remote-Branch integration/r1-development steht auf BASE_SHA;
- PR #142 ist gemergt, sein Source-HEAD und Merge-Commit entsprechen den
  Vorgaben, und GitHub-CI #1015 ist PASS;
- Issue #26 ist offen mit dem Titel und der Scope-/Akzeptanzspezifikation
  dieses Auftrags;
- Issue #25 ist live CLOSED; deshalb ist der bestehende
  ISSUE25_STATUS=CLOSED_COMPLETED-Eintrag der Roadmap für F1 korrekt und
  bleibt unverändert;
- der revidierte Plan basiert auf dem bisherigen Plan-Commit
  SUPERSEDES_PLAN_COMMIT; der Branch enthält davor genau den Roadmap-Sync und
  den ursprünglichen Plan, aber keine Implementation;
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
    CONTEXT_HEAD_SHA=c57be99bdce9d55ebb65b4c4c06e5210e84b7ed9
    CONTEXT_PLAN_SHA=c57be99bdce9d55ebb65b4c4c06e5210e84b7ed9
    CONTEXT_REFRESH_MODE=FULL
    CONTEXT_DELTA=Issue25 owner close, F1 Roadmap verification, F2 Application-Handoff correction
    SOURCE_OF_TRUTH_CONFLICT=NONE

Der erste Commit dieses PRs ist ausschließlich der Roadmap-Sync. Der
ursprüngliche vollständige Plan war der zweite Commit
c57be99bdce9d55ebb65b4c4c06e5210e84b7ed9. Diese F1/F2-Korrektur ist eine
weitere versionierte Planrevision. Da Issue #25 live geschlossen ist, bleibt
der Roadmap-Eintrag unverändert; ein Agent schließt kein Issue. Bis zu einer
ausdrücklichen Freigabe der exakten revidierten PLAN_COMMIT bleibt die
Implementation NOT_STARTED.

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

FermentationHomeMode wird additiv so vervollständigt, dass die sechs
geforderten sichtbaren Modi eindeutig unterscheidbar sind. Der bestehende
Standby-Begriff bleibt der kanonische interne Ready-Modus; eine Umbenennung
des bestehenden Fachzustands ist nicht erforderlich.

| Kanonische Quelle | sichtbarer Home-Modus | Primäraktion |
|---|---|---|
| validierter Standby ohne Lauf | Standby / Bereit | Vorheizen oder Start, abhängig vom aufgelösten Programm |
| Preheating, ReachingTarget, QualifyingTarget, Fermenting, Cooling, CoolHolding oder ManualHolding | ActiveRun / Aktiv | Stop |
| WaitingForProduct oder fachlich offene UserDecisionRequired-Meldung | Waiting / Wartet | Weiter, Bestätigen oder Quittieren entsprechend dem kanonischen Kontext |
| Completed | Completed / Abgeschlossen | OK, Abschluss bestätigen oder Jetzt kühlen |
| SafeBoot, Fault, ServiceRequired oder fehlende normale Freigabe | Restricted / Eingeschränkt | Diagnose, Fehlerdetails oder zulässige Recoveryaktion |
| RecoveryEvaluation, WaitingForTrustedTime oder FallbackSelectionRequired | Recovery | nur die vom Recoveryvertrag angebotene Info-/Resumeaktion |
| fehlende App-/Plattformbereitschaft ohne Recoveryangebot | Unavailable | keine Fachaktion |

Die Projektionsfunktion verwendet canonical ProcessState,
RecoveryDisposition, Lifecycle- und Servicewerte. Sie leitet keinen Zustand
aus Temperaturrohwerten, Farben, einer einzelnen Meldung oder einer
Touchaktion ab. Warning-/DecisionRequired-Kontexte bleiben in Banner und
Detaildaten erhalten, ohne einen zweiten parallel gepflegten Domainzustand zu
erzeugen.

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

- bei einem vorheizenden Programm: Vorheizen; fachlich ist dies der
  bestehende StartProgram-Pfad und kein neuer Vorheizautomat;
- bei einem unmittelbar startbaren Programm: Start;
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
Insbesondere ist CommandStatus::Proposed beziehungsweise
DecisionStatus::Proposed nur DecisionOnly und niemals ein Apply- oder
Persistenznachweis. Erst das passende
TemperatureControlApplicationOrchestrator-Handoff darf ein tatsächliches
RunPersistenceResultStatus-Ergebnis mit OwningOutcome liefern. Die
Post-Commit-/Lifecycle-Handoffs bleiben vollständig im Orchestrator.

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
kanonischen ProcessRuntimeState, bevor eine Mutation begonnen wird. Bei
Abweichung liefert er ein typisiertes stale Ergebnis und ruft weder
decideProcessTransition noch den Orchestrator-Applypfad mit einer mutierenden
Entscheidung auf. Bei passender Revision erzeugt er ausschließlich die
bestehende TransitionRequest mit
ProcessEvent::ProductInsertedConfirmed und ruft
decideProcessTransition auf. Ein DecisionStatus::Proposed wird als
DecisionOnly in den Trace übernommen; nur danach darf
TemperatureControlApplicationOrchestrator::persistTransition aufgerufen
werden. Das persistierte Ergebnis wird getrennt als OwningOutcome
aufgezeichnet.

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

- Die bestehende canonical decide*- oder Preview-/Commitvalidierung läuft
  zuerst.
- Nur ein kanonisches NotConfirmed beziehungsweise
  ReadyForConfirmation erzeugt ConfirmationRequired.
- StaleState, InvalidInput, SafetyRejected, NotAllowedInState,
  ContextMissing, Busy, Unavailable und vergleichbare Zustände werden nicht
  durch ein UI-Precheck oder eine fehlende Bestätigung maskiert.
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

### 7.4 Programmschnappschuss

Die Rezeptliste und Startzusammenfassung lesen die aktuelle kanonische
Programmliste. Die UI-Intentform trägt nur Programm-ID, Lauf-ID und
ausgewählte fachlich erlaubte Optionen. Die App löst daraus am
Anwendungsrand den aktuellen Programmdatensatz und die gültige
Quellrevision auf.

Änderungen auf der Startzusammenfassung:

- Zieltemperatur, Dauer, Vorheizen, Sensorbetrieb, Abschluss- und Kühlziel
  werden als Kennzeichnung für den nächsten Lauf dargestellt;
- Zurücksetzen verwirft nur den flüchtigen Kandidaten;
- Start erzeugt über den bestehenden Pfad den unveränderlichen
  Programmschnappschuss;
- Bearbeiten speichert dauerhaft nur über ConfigurationPreview und
  bestätigten ConfigurationService-Commit;
- eine laufende Rezeptrevision und ein aktiver Run-Snapshot werden niemals
  durch die Rezeptseite verändert;
- nach einer konkurrierenden Änderung wird die bestehende Revision als
  Konflikt/Stale angezeigt, nicht still überschrieben.

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
- sichtbare Zustände Empty, Incomplete, Pending, Rejected und Accepted;
- Sperr-/Retryhinweis kommt als Ergebnis des owning Verifiers und wird nur
  angezeigt;
- PIN-Daten werden nicht persistiert, exportiert, geloggt oder in einem
  Snapshot abgelegt.

Für die native Simulation liefert ein deterministischer, secretsfreier
Testverifier nur ein vorgegebenes Ergebnis Accepted oder Rejected. #26
implementiert damit die normale PIN-Eingabe und ihre Bedienpfade, aber keinen
produktiven Credential- oder Hashvertrag. Der spätere Auth-/Webscope bleibt
bei seinem owning Issue. SAFE_BOOT verwendet die PIN-UI nicht als normalen
Servicezugang.

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
- FallbackSelectionRequired;
- SAFE_BOOT und RunPersistence-untrusted;
- Completed nach Neustart.

Die Anzeige darf dabei:

- den Recoverygrund, trusted-UTC-Wartezustand und verfügbare nächste Aktion
  zeigen;
- den bestehenden Fallback-Resume-Dialog darstellen;
- passive Diagnose und zulässigen Export anbieten;
- Back/Home als reine Navigation ausführen.

Sie darf nicht:

- einen Lauf automatisch aus einem alten Fallback aktivieren;
- ohne trusted UTC eine Dauer oder lokale Uhrzeit erfinden;
- eine Aktorpermission oder ein Sensorvertrauen aus einer UI-Auswahl
  ableiten;
- discardAsNoActiveRun für FallbackSelectionRequired wiederverwenden;
- einen Aktortest oder normalen PIN-Service aus SAFE_BOOT anbieten;
- durch Quittieren eine Fehlerursache oder Sperre beseitigen.

ResumeFallback bleibt die bestehende bestätigte Appaktion. Vor Applied
bleiben FallbackRecoveryPending, RAM-/FSM-Aktivierung und Aktorpermission
unresolved. Erst der bestehende Write-before-Apply-/FSM-/Fresh-Evidence-
Pfad kann die spätere Interlockbewertung erreichen.

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
| lib/fermentation_app/src/fermentation_touch_workspace.hpp/.cpp | lokale Route, Seiten, vier appseitige Slotinhalte, Pager, Dialoge, Sperrgründe und Mapping auf bestehende Intentformen |
| lib/fermentation_app/src/fermentation_ui_commands.hpp/.cpp | app-owned rendererunabhängiger ProductInsertedConfirmed-Intent mit erwarteter Zustandsrevision, getrennte DecisionOnly-/OwningOutcome-Ergebnisse und Bridge über den bestehenden Application-Orchestrator; keine künstliche Envelope-/Idempotenzsemantik |
| lib/fermentation_app/src/fermentation_ui_text.hpp/.cpp | zusätzliche fermentation-owned TextKeys für Rezepte, Phasen, Aktionen, Status, Service, PIN, Feedback und Sperrgründe in DE/EN/ES |

TemperatureControlApplicationOrchestrator, FermentationApplication,
ConfigurationService, RunPersistenceCoordinator, Process-State-Machine,
Sensor-, Recovery- und Safetyimplementierungen werden nur angepasst, wenn der
konkrete Workspace-Einstieg einen bereits vorhandenen Ownerpfad benötigt.
Keine private UI-Fachlogik wird in diese Owner verschoben.

### 11.3 Deterministischer Testsupport

| Datei | geplante Verantwortung |
|---|---|
| lib/device_platform_test_support/src/simulated_device_shell.hpp/.cpp | generischer semantischer Shellsimulator, 320-x-240-Rahmen, Header-/Footer-/Pagergeometrie, Wake-/Press-/Feedbacktrace; keine fermentation-Abhängigkeit |
| test/test_local_touch_ui/test_local_touch_ui.cpp | appseitige vollständige Simulation, canonical state setup, Workspace-/Shell-Komposition, UI-Commandtrace und Szenarioorakel |
| test/test_device_ui_contracts/test_device_ui_contracts.cpp | additive Tests für neue generische Interaktions-, Idle- und PIN-Verträge |
| test/test_fermentation_ui_models/test_fermentation_ui_models.cpp | Home-Modus-, Workspace- und Projektionstests |
| test/test_fermentation_ui_commands/test_fermentation_ui_commands.cpp | Transitionbrücke, Confirmationreihenfolge, Staleness, Duplicate und keine Navigation-IDs |

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

Alle Schnitte bleiben nach der Ownerfreigabe im selben Draft-PR #143. Jeder
Schnitt erhält gezielte Tests; bei einer materiellen Abweichung wird angehalten
und der Plan neu versioniert.

1. Generische Interaktion: semantische Targets, vier Slotindices,
   VerticalPager, Pressfeedback, Feedbackintents und additive Shellprüfung.
   Kein Commandaufruf in device_platform.
2. Idle und PIN: monotonic UI-Idle mit WakeOnly-Ersttouch, semantische
   Systemweckung, maskiertes vierstelliges PinEntryModel und
   secretsfreier Testverifier. ServiceSessionLease bleibt die #25-Autorität.
3. Workspace-Projektion: Home-Modusmatrix, gemeinsame Snapshotnutzung,
   vollständige lokale Routen, Status-/Service-Plattformbereiche und
   isolierte App-Erweiterungen. TextKeys statt sichtbarer Literale.
4. Kontextaktionen: Slot-0-Matrix für Vorheizen/Start/Stop/
   Fortsetzen/Bestätigen/Abschluss, Rezepte-Slot, strukturierte Dialoge,
   Sperrgründe sowie den app-owned ProductInsertedConfirmed-Contract über
   decideProcessTransition und
   TemperatureControlApplicationOrchestrator::persistTransition. Für alle
   mutierenden Commands werden DecisionOnly und Application-Apply getrennt
   getestet.
5. Rezepte und Laufdetails: Startzusammenfassung, next-run-only-Werte,
   unveränderlicher Run-Snapshot, Programmliste, Pager, Prozess-/Technikseite,
   Meldungs-/Quittier-/Mute-/Stop- und Abschlussabläufe über bestehende
   Commands und ConfigurationPreview.
6. Recovery, Header und Start: Splash/Startstatus, Headerziele,
   actor-free Recovery, FallbackSelectionRequired, WaitingForTrustedTime,
   SAFE_BOOT und sichtbare Restricted-Sperrgründe.
7. Simulation und Nachweis: generic test-support shell, 320-x-240-Frame,
   vollständige appseitige Szenarien, Layout-/Text-/Locale-/Theme- und
   Fehlbedienungsorakel; keine Hardwarepfade.
8. Gezielte Dokumentation und Review: SIM-Matrix, vollständiger Diff-
   Review gegen Issue/#25-Vertrag/ADR-013, Architektur-/Secret-/Diffchecks
   und Aktualisierung des Draft-PR-Nachweises.

Der konkrete spätere Implementierungs-Commit-Schnitt darf diese Reihenfolge
mechanisch auf mehrere Commits verteilen, erzeugt aber keinen weiteren PR,
keine weitere Roadmap-Planwahrheit und kein vorgezogenes #31.

## 13. Simulations- und Akzeptanzmatrix

Die folgenden Fälle sind verpflichtende native Nachweise. Sie sind
deterministisch mit expliziter virtueller Zeit auszuführen. Ein nicht
ausgeführter Fall bleibt NOT_RUN und ist kein PASS.

| ID | Nachweis |
|---|---|
| SIM-26-01 | Ready/Standby zeigt genau vier Slots: primäre Startaktion, Rezepte, Status, Service; Header und leere Slots bleiben sichtbar |
| SIM-26-02 | Home-Modusmatrix ergibt eindeutig Bereit, Aktiv, Wartet, Abgeschlossen, Eingeschränkt und Recovery aus canonical states |
| SIM-26-03 | Splash läuft ungefähr drei Sekunden ohne blockierenden Tick; ready führt Home, langsamer Start den kompakten Startstatus und kritischer Startfehler actor-free in Restricted/Recovery |
| SIM-26-04 | Headerflächen für Sprache, WLAN und Uhrzeit sind nicht überlappend; Sprache nutzt enthaltene Locale, WLAN den Statusindikator und Uhr die ClockViewInput ohne erfundene Lokalzeit |
| SIM-26-05 | Home/Back-Hierarchie: erste Unterebene Home, tiefere Ebene Back, genau ein Segment pro Back, ExitRequirement verhindert stilles Verwerfen |
| SIM-26-06 | Rezepte-Listen verwenden große Einträge und Pager; Standardprogramme vor Benutzerprogrammen, ungültige Programme gesperrt und mit Grund sichtbar |
| SIM-26-07 | Programmzeile öffnet Startzusammenfassung; next-run-only-Änderungen überschreiben weder Katalog noch aktiven Schnappschuss; Reset verwirft nur den Kandidaten |
| SIM-26-08 | Vorheizen/Start verwendet den bestehenden ProgramStart-Pfad; Produkt eingesetzt verwendet den app-owned ProductInsertedConfirmed-Contract, decideProcessTransition und den TemperatureControlApplicationOrchestrator ohne zweite FSM |
| SIM-26-09 | Aktiver Lauf zeigt Stop im ersten Slot, Rezepte im zweiten, Prozess-/Technikdetails und aktuelle Snapshotwerte; Stop bietet Ausschalten oder Kühlen |
| SIM-26-10 | Abschluss zeigt OK/Details und optional Jetzt kühlen; Kühlen erzeugt nur über den bestehenden Completion-Pfad einen neuen manuellen Lauf |
| SIM-26-11 | Meldungsbanner zeigt höchste Priorität; Quittieren beseitigt Ursache nicht, Stummschalten beendet nur akustisches Signal, Faultreset bleibt getrennt |
| SIM-26-12 | Status und Service zeigen Plattformseiten vor App-Erweiterungen; eine unavailable Appsection isoliert Plattformstatus und andere Appsections |
| SIM-26-13 | Lange Listen/Informationen navigieren ausschließlich über sichtbare Auf-/Ab-Ziele; kein Wischen, keine horizontale Route und keine Domainmutation |
| SIM-26-14 | Pressfeedback wird für jedes erkannte Ziel sichtbar; disabled action zeigt Sperrgrund; kritische Aktionen verlangen strukturierte Confirmation und sind nicht doppelt auslösbar |
| SIM-26-15 | Erster Touch im Dimmed/Asleep-Zustand erzeugt WakeOnly und kein Command; zweiter Touch kann erst danach das Ziel auslösen |
| SIM-26-16 | Hintergrundupdates und Refreshrevisionen verlängern keine Lease und lösen keinen Command aus; relevante Servicebedienung verlängert nur die lokale Lease |
| SIM-26-17 | PIN-UI maskiert vier Stellen, akzeptiert Ziffern/Backspace/Clear/Cancel/Submit, zeigt Pending/Rejected/Accepted und speichert keinen Secretwert |
| SIM-26-18 | Lokale Servicelease läuft nach 10 Minuten Inaktivität ab, ohne absolute Maximaldauer; ExplicitSignOut, DeviceRestart und SafetyStateInvalidated sperren sie sofort |
| SIM-26-19 | Web-5/15-Servicepolicy bleibt von Touch-10/ohne-Absolutlimit und normalem Weblogin getrennt |
| SIM-26-20 | WaitingForTrustedTime bleibt RAM-only und actor-free; Current-Recovery zeigt keine erfundene Dauer oder Zeit und Fallback bleibt Auswahlangebot |
| SIM-26-21 | FallbackSelectionRequired erlaubt nur die bestehende bestätigte ResumeFallback-Aktion; Back/Home und Quittieren mutieren weder Persistenz noch Recoveryangebot |
| SIM-26-22 | SAFE_BOOT zeigt passive Diagnose/Recoveryhinweise, aber keinen normalen PIN-Service, Aktortest oder Aktorfreigabe |
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
| SIM-26-31 | WaitingForProduct -> ProductInsertedConfirmed -> ReachingTarget läuft über den app-owned Intent, decideProcessTransition und TemperatureControlApplicationOrchestrator::persistTransition |
| SIM-26-32 | Ein Fehler aus persistTransition lässt RAM-/Prozesszustand unverändert beziehungsweise fail-closed; es gibt keinen vorgezogenen Lifecycle-Handoff |
| SIM-26-33 | Eine stale erwartete Zustandsrevision wird vor der Mutation abgelehnt und erzeugt keinen Persistenzschreibvorgang |
| SIM-26-34 | Ein Replay nach erfolgreichem ProductInsertedConfirmed erzeugt wegen der verbrauchten Zustandsrevision oder der kanonischen Zustandsablehnung keine zweite Nebenwirkung |
| SIM-26-35 | Der Workspace enthält keinen direkten Aufruf von RunPersistenceCoordinator::persistTransition; Architektur-/Diffprüfung bestätigt den Application-Orchestrator als einzige Run-Mutationsgrenze |
| SIM-26-36 | CommandStatus::Proposed und DecisionStatus::Proposed erscheinen als DecisionOnly; erst das getrennte Ergebnis von persistCommand, persistFreshStartCommand, persistTransition oder persistSensorSelection erscheint als OwningOutcome |

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

Nach Ownerfreigabe und je Implementierungsschnitt:

    pio test -e native --filter test_device_ui_contracts
    pio test -e native --filter test_fermentation_ui_models
    pio test -e native --filter test_fermentation_ui_commands
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
ohne Produktionsabhängigkeit. Architektur- und Secretchecks sind deshalb
Teil des gezielten Nachweises.

### 14.3 Nicht ausgeführte Nachweise

Während des Drafts werden keine vollständigen nativen Läufe, ESP-IDF-
bringup/release Builds, Hardware-Smokes, Display-/Touchtests, WLAN-/RTC-Tests
oder physische PIN-/Kalibrierungstests als PASS bezeichnet. Ein vollständiger
lokaler Lauf erfolgt nur nach vollständigem Review, finalem HEAD und
ausdrücklicher Owneranweisung. GitHub-Firmware-CI bleibt im Draft
übersprungen; der Owner entscheidet später über Ready for review.

## 15. Risiken, offene Grenzen und Owner-Gates

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

Zwingende Owner-Gates:

1. ausdrückliche Freigabe genau der PLAN_COMMIT-SHA;
2. keine Implementation vor dieser Freigabe;
3. keine Renderer-, Treiber-, Touch-, Kalibrierungs-, Asset-, GPIO- oder
   Backlightentscheidung in #26;
4. kein normaler Auth-/Credential-Backend in #26;
5. jede Änderung an Safety-, Recovery-, Persistenz-, Command-,
   Architektur- oder Testvertrag als materielle Planabweichung stoppen und
   neu planen;
6. vollständiges Review vor einem eventuellen Ready-Gate;
7. Ready, Merge, Issue-Close, CI-Gateentscheidung und Aktorfreigabe bleiben
   Ownerrechte.

## 16. Commit-, PR- und Handover-Vertrag

Der PR enthält nach dieser Befundkorrektur genau drei eigene Commits:

1. Roadmap-Sync-Commit 28a35b610020513460690d4b05e90bdec88e81d8;
2. ursprünglicher vollständiger Plan-Commit
   c57be99bdce9d55ebb65b4c4c06e5210e84b7ed9;
3. diese F1/F2-Planrevision mit exakter SHA nach dem Commit.

Issue #25 ist live CLOSED; deshalb bleibt der Roadmap-Sync unverändert und
es wird kein Issue durch den Agenten geschlossen. Der Draft-PR #143 wird nach
dem dritten Commit mit den folgenden Feldern aktualisiert:

    ROADMAP_COMMIT=28a35b610020513460690d4b05e90bdec88e81d8
    PLAN_PATH=docs/tasks/issue-26-local-touch-shell-plan.md
    SUPERSEDES_PLAN_COMMIT=c57be99bdce9d55ebb65b4c4c06e5210e84b7ed9
    PLAN_COMMIT=<exakte revidierte Commit-SHA>
    PR_HEAD=<exakte revidierte Commit-SHA>
    IMPLEMENTATION=NOT_STARTED
    OWNER_PLAN_APPROVAL_REQUIRED=YES
    ACTUATOR_RELEASE=NO

Vor Sessionende wird genau ein aktueller SESSION HANDOVER-Kommentar im
offenen Draft-PR erstellt. Er enthält:

    ISSUE=26
    PR=143_DRAFT
    BRANCH=feature/issue-26-local-touch-shell
    HEAD=<PR-HEAD nach Planpush>
    ROADMAP_COMMIT=28a35b610020513460690d4b05e90bdec88e81d8
    SUPERSEDES_PLAN_COMMIT=c57be99bdce9d55ebb65b4c4c06e5210e84b7ed9
    PLAN_COMMIT=<exakte revidierte Commit-SHA>
    IMPLEMENTATION=NOT_STARTED
    TESTS=NOT_RUN_PLANNING_ONLY
    ACTUATOR_RELEASE=NO
    OPEN_GATE=OWNER_APPROVAL_OF_EXACT_PLAN_COMMIT
    NEXT_STEP=Owner prüft und gibt exakt PLAN_COMMIT frei; danach erst Umsetzung

Der Handover nennt die beiden maßgeblichen exakten Commit-SHAs
ROADMAP_COMMIT und die revidierte PLAN_COMMIT sowie den PR-HEAD; die
überschriebene Vorgänger-Plan-SHA bleibt zusätzlich als Provenienz genannt.
Er kopiert weder Diff noch Plan. Danach hält der Agent an. Es wird kein
Ready, Merge, Auto-Merge, Issue-Close, Force-Push oder Branch-Löschen
ausgeführt.

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
  Entscheidungen sind nicht als ausgeführte Mutationen markiert.
- ProductInsertedConfirmed verwendet den app-owned Contract mit erwarteter
  Zustandsrevision, decideProcessTransition und ausschließlich
  TemperatureControlApplicationOrchestrator::persistTransition; der
  Workspace ruft keinen RunPersistenceCoordinator direkt auf.
- der erste Wake-Touch führt keinen Command aus; reine Navigation und Reads
  mutieren nichts; ein aktiver Run-Snapshot bleibt unverändert.
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
- vollständiges Review gegen Issue, diesen Plan, #25-Vertrag, ADR-013,
  Safety-/Recovery-/Persistenzverträge, Tests und Dokumentation ist PASS;
  erst danach entscheidet der Owner über den Ready-Gate.
