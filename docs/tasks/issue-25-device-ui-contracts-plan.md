# Issue #25 – Gemeinsame UI-Modelle, Device-Shell-Vertraege und Mehrsprachigkeit

## 1. Ausgangslage und aktuelle Baseline

Dieser Plan ist die vollstaendige, eigenstaendig ausfuehrbare Planrevision fuer
Issue #25. Er ersetzt keine zu seiner Umsetzung notwendige Aussage durch einen
Verweis auf aeltere Planrevisionen.

```text
ISSUE=25
TITLE=[E4.1] Gemeinsame UI-Modelle, Device-Shell-Vertraege und Mehrsprachigkeit
WORKFLOW=PLAN_FIRST_SINGLE_PR
BASE_BRANCH=integration/r1-development
BASE_SHA=86e55499d9f0dd4dbd2d9fbc95d04549df4d429c
BRANCH=feature/issue-25-device-ui-contracts
PR=142_DRAFT
ROADMAP_COMMIT=e6c051e69eb84d578f6a662033548ff2cacc7771
ISSUE25=OPEN
ISSUE25_STATUS=PLANNING
ISSUE25_STARTED=YES
IMPLEMENTATION=NOT_STARTED
ACTUATOR_RELEASE=NO
PLAN_COMMIT=THIS_COMMIT
OWNER_PLAN_REVIEW_REQUIRED=YES
```

Die Live-Pruefung vor Commit 1 ergab `main` und
`integration/r1-development` beide auf `86e55499d9f0dd4dbd2d9fbc95d04549df4d429c`.
Issue #140 und Draft-PR #141 sind geschlossen. Der nicht gemergte Branch
`docs/post-promotion-roadmap-sync` wird nicht wiederverwendet. Commit 1 dieses
PR aendert ausschliesslich `docs/ROADMAP.md`: Er dokumentiert den erfolgreichen
PR-#135-Checkpoint, entfernt dessen aktive Promotionsgates und beginnt #25 im
selben PR.

Die Planungsanalyse ist ein Full Refresh auf `e6c051e69eb84d578f6a662033548ff2cacc7771`:

```text
CONTEXT_BASELINE_BRANCH=feature/issue-25-device-ui-contracts
CONTEXT_BASELINE_SHA=e6c051e69eb84d578f6a662033548ff2cacc7771
CONTEXT_HEAD_SHA=e6c051e69eb84d578f6a662033548ff2cacc7771
CONTEXT_PLAN_SHA=NONE
CONTEXT_REFRESH_MODE=FULL
CONTEXT_DELTA=ROADMAP_COMMIT_ONLY
SOURCE_OF_TRUTH_CONFLICT=NONE
```

Gepruefte verbindliche Quellen sind das Live-Issue #25, Roadmap, Root- und
Modulregeln, Workflow, Engineering- und Quality-Gates, ADR-013, ADR-017,
ADR-018 und ADR-019, die Device-UI-, Local-UI-, Local-Service-, Web- und
Settings-Vertraege, die R1-Spezifikationsreview- und Visual-Design-Basis sowie
der aktuelle Code und die direkten nativen Tests. Fuer die nicht neu zu
interpretierenden Grenzen wurden ausserdem die aktuellen #121-, #124- und
#126-Vertraege in `ARCHITECTURE.md`, `SYSTEM_SAFETY_AND_RECOVERY.md`,
`RECOVERY_AND_INTERRUPTION.md`, `RUN_PERSISTENCE.md`, `STATE_MACHINE.md` und
den gemergten Issue-/PR-Nachweisen geprueft.

## 2. Ziel und Nicht-Ziele

Ziel ist eine kleine, nativ testbare, rendererunabhaengige Vertragsschicht fuer
die spaetere lokale Touch- und Weboberflaeche. Sie liefert die gemeinsame
Device-Shell, Status-/Service-Erweiterungspunkte, Text-/Branding-/Theme- und
Sitzungsvertraege sowie fermentationsspezifische Snapshots und Commands. Touch
und Web konsumieren dieselben fachlichen Snapshots und Commands, duerfen aber
unterschiedliche Navigation, Layouts, Transport- und Sitzungsadapter besitzen.

Die Vertrage projizieren kanonische Fach-, Sensor-, Safety-, Persistenz- und
Recoveryentscheidungen. Sie erzeugen weder einen zweiten Fachzustandsautomaten
noch eine zweite Recovery-, Safety-, Authentisierungs- oder
Persistenzentscheidung.

Explizit nicht Bestandteil von #25 sind:

```text
LVGL_IMPLEMENTATION=NO
HTML_IMPLEMENTATION=NO
DISPLAY_DRIVER=NO
TOUCH_DRIVER=NO
GPIO_CHANGE=NO
BOARD_PROFILE_CHANGE=NO
PIXEL_LAYOUT_IMPLEMENTATION=NO
SCREEN_IMPLEMENTATION=NO
TOUCH_RAW_VALUES=NO
TOUCH_CALIBRATION=NO
FONT_ASSETS=NO
IMAGE_ASSETS=NO
REAL_DISPLAY_HARDWARE=NO
NEW_SAFETY_POLICY=NO
NEW_RECOVERY_POLICY=NO
NEW_SENSOR_ROLE_POLICY=NO
NEW_AUTH_POLICY=NO
NEW_PERSISTENCE_POLICY=NO
PLUGIN_FRAMEWORK=NO
UNIVERSAL_WIDGET_SYSTEM=NO
WIDGET_FRAMEWORK=NO
PLUGIN_PLATFORM=NO
RUNTIME_DISCOVERY=NO
UNIVERSAL_UI_ENGINE=NO
```

Renderer, reale 320x240-Darstellung, Touchkalibrierung, Assetkonvertierung,
Fonts, Treiber, Puffer, Displayhardware und ihre Ressourcen- und
Hardwareabnahme bleiben Issue #31. Die lokale simulierte Bedienlogik bleibt
Issue #26. Webtransport, HTTP, Authentisierung und Web-Sessionimplementierung
bleiben bei ihren owning Issues; #25 modelliert nur die gemeinsamen fachlichen
Vertraege und die bereits entschiedene getrennte Policy.

## 3. Bestehende Architektur und Reuse-Befund

### 3.1 Modulgrenze und vorhandene anwendungsneutrale Basis

ADR-013 und die lokalen Modulregeln bleiben unveraendert:

```text
device_platform
    portable, anwendungsneutrale UI-Bausteine und Ports
device_platform_esp_idf
    keine #25-Aenderung; keine Renderer- oder Hardwareadapter
fermentation_app
    fermentationsspezifische Projektionen und Command-Bridge
device_platform_test_support
    nur native Mocks/Fixtures fuer die schmalen Plattformvertraege
```

Folgende vorhandene Vertrage werden wiederverwendet statt ersetzt:

| Bestehende Quelle | Wiederverwendung in #25 | Nicht zulaessige Neuinterpretation |
|---|---|---|
| `device_platform::ITimeSource` | `monotonicMillis()` fuer lokale Sessionfristen; `unixTimeSeconds()` als optionale trusted UTC fuer die Headerprojektion | Keine RTC-, SNTP- oder Zeitqualitaets-API; `nullopt` wird nicht als Uhrzeit erfunden. |
| `device_platform::INetworkStatus` | generischer Verbindungsindikator im Shell-Header und Statusmodell | WLAN konfiguriert weder den Prozess noch die Safety. |
| `SensorQualitySnapshot` und `SensorQuality` | Temperatur-/Qualitaetsfelder werden unveraendert in eine Anzeigeprojektion uebernommen | Kein Rollenwechsel, keine Plausibilitaet oder Fehlerklassifikation in der UI. |
| `RuntimeMessage`, `MessageCode`, `MessageClass`, `MessageTrigger` | Meldungsmodell, Prioritaet, Quittier- und Mute-Command-Projektion | Quittieren behebt keinen Fehler und veraendert keine Safetyfreigabe. |
| `CommandEnvelope`, `CommandStatus`, `RunPersistenceResultStatus` und `ConfigurationCommitStatus` | erwartete fachliche Revisionen, Quelle und Ergebnisabbildung | Kein globaler UI-Zaehler ersetzt State-, Run-, Message-, Fault-, Config- oder Recoveryrevisionen. |
| `ProcessRuntimeState`, `ProcessState`, `RunCommandState`, `RunProgramSnapshot` und `EffectiveControlContext` | Home-, Status-, Lauf- und Aktionsmodelle | Der Renderer leitet weder Modus, Ziel, Sensorrolle noch Phase aus Einzelwerten her. |
| `PresentationState`, `ApplicationLifecycleState`, `RunLoadDisposition` und `RecoveryDisposition` | Status-, Service- und Recoveryprojektion | Keine neue Fault-, Boot-, SAFE_BOOT- oder Recoverypolicy. |
| `RunPersistenceCoordinator` | bestehender write-before-apply Pfad fuer bereits erlaubte Commands und die explizite Fallbackaktivierung | Kein zweiter Persistence-/Recovery-Coordinator, kein Tombstone oder Resume-Claim nur fuer Anzeige. |
| `UserConfiguration.displayLanguageId`, `RuntimeConfigurationSnapshot` und `ConfigurationService` | persistente lokale Sprachauswahl ueber den bestehenden manifestgebundenen Preview-/Commitpfad | Keine zweite UI-Persistenz, kein eigener Root, Slot, Schema- oder Commitpfad. |

Der vorhandene Sprachkatalog kennt bereits `de`, `en` und `es`. Die
Konfigurationsvalidierung, der User-Configuration-Codec und die Dokumenttests
werden deshalb erweitert, nicht kopiert. Heute besitzt `UserConfiguration`
noch keine Themeauswahl; die vorhandene, versionierte Konfigurationsdomane ist
die einzige passende Stelle fuer diese persistente Laufzeitwahl.

### 3.2 Safety-, Recovery- und Zeitgrenzen

- Jeder Boot beginnt all-off und `ActuatorSafetyGateStatus::Unresolved`.
  `ActuationInterlock` bleibt die einzige Freigabebewertung; kein UI-Ergebnis,
  keine Bestaetigung und keine Servicefreigabe kann `Allowed` erzeugen.
- `SAFE_BOOT`, `Fault`, technisch untrusted Persistenz und unklare
  Config-/Recoveryevidenz bleiben sichtbar, aber fail-closed und aktorfrei.
- Der enge #124-Current-`FERMENTING`-Fall bleibt unveraendert: nur exakt
  validierte Evidenz mit trusted aktueller UTC fuehrt zu automatischer
  **logischer** Recovery. Fehlende UTC bleibt
  `RecoveryEvaluation/WaitingForTrustedTime` im RAM, ohne Mutation;
  automatische Recovery gibt keine Aktoren frei.
- Die #124-Zeitrechnung, `priorBootPhaseElapsed`,
  `observedRunSeconds`, die normale FSM-Completion-Topologie und
  `ITimeSource` bleiben unveraendert. Der UI-Text behauptet weder eine
  physische Stromausfalldauer noch eine biologische Zeitgutschrift.
- Ein `FallbackRecovered`-/`FallbackRecoveryPending`-Befund ist nur als
  bestehendes, explizit zu bestaetigendes Fallback-Angebot sichtbar. Er wird
  nie automatisch aktiviert oder zu `Allowed` promoviert. Eine spaetere
  explizite Auswahl delegiert ausschliesslich an den vorhandenen
  `activateFallbackRecoveredRun`-/write-before-apply-Pfad und benoetigt seine
  bestehenden frischen Evidenzen.
- #126 bleibt die Quelle fuer trusted UTC: die App sieht ausschliesslich
  `ITimeSource`; RTC, I2C, DS3231, SNTP, WLAN, Credentials und Netzwerkgates
  gelangen nicht in die gemeinsamen UI-Typen.

## 4. Vorgeschlagene minimale Architektur

### 4.1 Plattform vor Anwendung

`device_platform` erhaelt nur kleine, fuer weitere Devices brauchbare
Werttypen und pure Regeln:

```text
DeviceUiBuildCatalog
    -> enthaltene Brandings, Locales und Themes; aktives Build-Branding
DeviceShellModel
    -> Header, Navigation, exakt vier Slots, Plattformbereiche
DeviceUiTextContract
    -> TextKey, Namespace, Packmanifest, Lookup und Fallback
DeviceUiThemeContract
    -> ThemeId, semantische Token, Vollstaendigkeit und Standardfallback
DeviceUiCommandContract
    -> Quelle, erwartete Snapshot-/Fachrevisionen, Bestaetigung und Ergebnis
DeviceUiSessionContract
    -> getrennte, reine Lease-/Ablaufregeln ohne Authentisierung
```

Diese Schicht kennt keine Fermentation, keine Programme, Sensorrollen,
FaultCodes, Recoverytypen, HTML, LVGL, Displaytreiber, GPIOs, Dateien,
Netzwerkprotokolle oder Laufzeitregistrierung. Ihre Registry ist eine bei der
Composition explizit gelieferte, begrenzte Liste; sie sucht, laedt oder
instanziiert keine Plugins und Widgets.

`fermentation_app` erhaelt eine schmale Projektions- und Command-Bridge:

```text
canonical application state
    -> FermentationUiProjector
    -> FermentationUiSnapshot
Touch adapter / Web adapter
    -> FermentationUiCommand request
    -> FermentationUiCommandBridge
    -> existing domain decision + persistence path
    -> DeviceUiCommandResult + current snapshot revision
```

Der Projector liest nur kanonische Werte. Die Command-Bridge besitzt keine
Safety-, Persistenz-, Sensor- oder Recoverylogik; sie bildet Requests auf die
vorhandenen Decision-/Coordinator-Vertraege ab und bewahrt deren Ergebnis.
Erforderliche frische Evidenz kommt ausschliesslich aus dem owning
Application-/Control-Pfad, nie aus Touch oder Web.

### 4.2 Shell, Navigation und Erweiterungen

`DeviceShellModel` enthaelt:

- einen Header mit `BrandingId`, aktiver `LocaleId`, generischem
  `NetworkIndicator` und optionalem UTC-Wert; fehlende trusted UTC bleibt
  explizit fehlend und wird renderer-/sprachspezifisch dargestellt;
- `std::array<BottomSlot, 4>` als genau vier unveraenderliche Slots. Jeder
  Slot ist sichtbar; ein leerer Slot besitzt den expliziten Zustand `Empty`
  und wird weder ausgeblendet noch verbreitert;
- eine typisierte Pfadhierarchie mit Home- und Back-Intent. Auf Ebene eins ist
  Home sichtbar; ab Ebene zwei ist Back sichtbar und entfernt genau ein
  Segment; Home fuehrt zur Home-Route der aktiven App;
- `PageExitRequirement` fuer ungespeicherte Aenderungen und kritische
  Ablaeufe. Navigation kann damit kein Verlassen still erzwingen oder einen
  Safety-/Bestaetigungsablauf umgehen;
- die festen Plattformbereiche Status und Service.

Ein `StaticUiExtensionCatalog` nimmt bei der Composition eine begrenzte,
explizite Folge von Plattform- und App-Section-Deskriptoren entgegen.
Plattformdeskriptoren werden immer vor Appdeskriptoren gelistet. Jede
App-Section wird einzeln als `Available` oder `Unavailable` mit technischem
Grund ausgewertet; ein Fehler oder eine Nichtverfuegbarkeit entfernt keine
Plattform-Section und verhindert keine anderen unabhängigen App-Sections.
Der Catalog besitzt keine Reflection, keine shared-library-Ladung, keine
Runtime-Discovery und keine Widget-API.

Das gemeinsame Komponenten-/Layoutvokabular bleibt absichtlich nur aus den
Modellbegriffen `Header`, `BottomSlot`, `PageTitle`, `StatusBadge`,
`TemperatureValue`, `ListRow`, `Action`, `Confirmation`, `MessageBanner` und
`VerticalPager` bestehen. Es beschreibt Semantik und Bedienzustand, keine
Pixels, Dimensionen, Rendererklassen oder Screens. Die 320x240-, Font-,
Textlaengen- und Hit-Target-Anforderungen bleiben rendererunabhaengige
Pack-/Renderer-Abnahmekriterien fuer #26/#31.

### 4.3 Snapshot-, Update- und Commandvertrag

Jeder gelesene `FermentationUiSnapshot` besitzt eine fluechtige,
monoton steigende `UiSnapshotRevision` und die fachlichen erwartbaren
Teilrevisionen mindestens fuer Process-State, Run, Message, Fault,
Recovery/Persistenz und Konfiguration. Die fluechtige Snapshotrevision ist
ein Transport-/Cache-Erkennungswert; sie ersetzt keinen bestehenden fachlichen
Revisionsgate und wird nicht persistiert.

Ein Command besteht aus:

- `UiCommandContext` mit `UiSurface` (`Touch` oder `Web`), Request-ID,
  erwarteter Snapshotrevision und den vom Snapshot stammenden fachlichen
  Teilrevisionen;
- einer kleinen, app-spezifischen `FermentationUiCommand`-Variante fuer
  vorhandene Lauf-, Meldungs-, Recovery-, Konfigurations- und
  Service-Navigationsintents. Die Varianten tragen keine rohen Aktor-, GPIO-,
  Sensor-, Safety- oder Webtransportdaten;
- einer optionalen strukturierten `ConfirmationRequest` mit stablem TextKey,
  Ziel, Revision und bestaetigungsbeduerftigem Intent.

`DeviceUiCommandResult` hat genau diese sichtbaren Oberergebnisse:

| Ergebnis | Bedeutet |
|---|---|
| `Accepted` | Der bestehende kanonische Pfad hat den Vorgang angenommen; das Ergebnis enthaelt die aktuelle Snapshotrevision. |
| `Rejected` | Fachliche Validierung, Safety, Permission, Revision oder Persistenz hat abgelehnt; ein stabiler technischer Grund ist sichtbar. |
| `ConfirmationRequired` | Der Zustand blieb unveraendert; die strukturierte Bestaetigung muss gegen dieselben erwarteten Revisionen erneut eingereicht werden. |
| `Busy` | Der bestehende Coordinator oder die Konfigurationsmutation besitzt den Vorgang; keine Nebenwirkung der UI. |
| `Unavailable` | Der benoetigte kanonische Dienst, frische Evidenz oder die Funktion ist nicht verfuegbar; keine Annahme und kein Fallback. |

`StaleSnapshot` und fachlich veraltete Teilrevisionen werden als
`Rejected` mit dem Grund `StaleRevision` abgebildet. Touch und Web muessen bei
einem Updatehinweis den vollstaendigen aktuellen Snapshot lesen; sie duerfen
nie aus einem Delta lokale Fachwerte nachrechnen. Hinweise duerfen
zusammenfallen oder verlorengehen, weil der folgende Vollsnapshot die aktuelle
Revision traegt. Ein erfolgreicher Command liefert ebenfalls die aktuelle
Revision; ein erneut gesendeter Command bleibt dem bestehenden
Command-ID-/Persistenzvertrag unterworfen.

## 5. Konkrete Typen, Ports, Modelle und Ownership

### 5.1 `device_platform`

| Typ/Regel | Ownership und Verantwortung |
|---|---|
| `BrandingId`, `LocaleId`, `ThemeId`, `TextNamespace`, `TextKey` | stabile, generische Identitaeten; `Platform` und `Application` sind zwingend getrennte Namespaces. |
| `DeviceUiBuildCatalog` | Build liefert enthaltene Brandings/Locales/Themes und das aktive Branding; mindestens `manuengineer`, `de`, `en`, `es` und das R1-Standardtheme sind deklarierbar. R1 waehlt ManuEngineer zur Build-Zeit, nie zur Laufzeit. |
| `TextPackManifest`, `TextLookupResult`, `resolveText` | statische Packdaten je Locale mit Zeichensatz-, Textlaengen- und 320x240-Eignungsmetadaten. Lookup: aktive Locale, dann `en`, dann sichtbarer vollqualifizierter Key. Keine stille Deutschannahme und keine i18n-Runtime. |
| `ThemeToken`, `ThemeDescriptor`, `resolveTheme` | nur semantische Tokens (inklusive `on_*`-Token und Overlay) und Vollstaendigkeit. Der Standard `manuengineer-dark` ist der einzige R1-Themeinhalt. Ungueltig, unvollstaendig oder nicht enthalten -> vollstaendiger Standarddescriptor; keine screenlokalen Farben oder Alphaannahmen. |
| `DeviceShellHeader`, `BottomSlot`, `ShellRoute`, `ShellNavigation` | Header und genau vier sichtbare Slots, Home-/Back- und Exitvertrag, ohne Pixel oder Renderer. |
| `StaticUiExtensionCatalog`, `UiSectionDescriptor`, `UiSectionAvailability` | explizite Plattform-vor-App-Sections und pro App-Section Fehlerisolation; keine dynamische Registry. |
| `UiSnapshotRevision`, `UiExpectedRevisions`, `UiCommandContext`, `ConfirmationRequest`, `DeviceUiCommandResult` | gemeinsame Revision-, Bestaetigungs-, Ergebnis- und Vollsnapshot-Aktualisierungssemantik; keine Fachcommandvariante. |
| `ServiceSessionPolicy`, `ServiceSessionLease`, `ServiceSessionEvent` | reine Zeit-/Invalidierungslogik. Der Aufrufer authentisiert und klassifiziert Safety; der Contract kann keine PIN pruefen oder Aktoren freigeben. |

Die Sessionregeln werden als zwei explizite, getrennte Konstanten modelliert:

```text
LOCAL_TOUCH_SERVICE_INACTIVITY_TIMEOUT=10_MIN
LOCAL_TOUCH_SERVICE_ABSOLUTE_TIMEOUT=NONE
WEB_SERVICE_INACTIVITY_TIMEOUT=5_MIN
WEB_SERVICE_ABSOLUTE_TIMEOUT=15_MIN
```

Nur relevante geschuetzte Benutzeraktionen erneuern die lokale Lease.
Hintergrundupdates, Messwerte und Updatehinweise tun dies nicht. Die lokale
Lease endet bei Inaktivitaet, Neustart, explizitem Logout oder einem vom
bestehenden Safety-/Servicepfad explizit gemeldeten sicherheitsrelevanten
Invalidierungsereignis. #25 definiert keine neue Liste oder Bewertung solcher
Safetyereignisse. Der Webwert wird als getrennte Policy nachgewiesen und nicht
mit Touch geteilt oder von Touchaktionen beeinflusst.

### 5.2 `fermentation_app`

`FermentationUiSnapshot` ist app-spezifisch und enthaelt mindestens:

| Teilmodell | Kanonische Quelle und Projektionsregel |
|---|---|
| `FermentationHomeView` | `ProcessState`, unveraenderlicher Run-/Programmsnapshot, effektive Werte, vorhandene Primaeraktion und kanonischer Laufstatus; Home-Modus wird nicht aus Temperaturen geraten. |
| `ShellNavigation` | gemeinsamer Plattformvertrag mit appgelieferten Slots/Routes. Der erste App-Slot enthaelt nur den kanonisch erlaubten Primaerintent; der zweite nur den appgelieferten Hauptbereich. |
| `TemperatureView` | Rolle, optionaler Wert und die vorhandene `SensorQualitySnapshot`-/Fehlerursache. Fehlende Werte bleiben fehlend und werden nicht interpoliert. |
| `MessageView` | `RuntimeMessage` einschliesslich Prioritaet, Aktivitaet, Quittierung, Entscheidungspflicht und Revision. Ack, Fix und Reset bleiben getrennt. |
| `RecoveryView` | unterscheidet mindestens `Normal`, `WaitingForTrustedTime`, `CurrentRunRecovered`, `FallbackSelectionRequired`, `RecoveryRejectedOrFailClosed`, `Completed` und `Cooling`, ausschliesslich aus `RecoveryDisposition`, Load-/Coordinator-Disposition und Prozesszustand. |
| `ApplicationStatusView` | `ApplicationLifecycleState`, `PresentationState`, FaultCode, Resetursache und die aktuelle vorhandene Gate-/Verfuegbarkeitsprojektion. Es ist kein zweites Safety-Gate. |
| `ServiceView` | Verfuegbarkeit, zugrundeliegende Schutzstufe, bestehende Session-Lease und aktorfreier SAFE_BOOT-Hinweis. Keine PIN-, Credential- oder Aktortestimplementierung. |

`FermentationUiProjector` ist eine reine appseitige Projektion. Er erhaelt
seine Inputs explizit aus `FermentationApplication` und den bestehenden
Koordinatoren; er greift nicht auf globale Variablen, ESP-IDF oder
Test-Support zu. `FermentationApplication` bleibt Eigentumer von
`runtimeRunState_`, `pendingResume_`, `pendingRecoverySource_`,
`RunPersistenceCoordinator`, `ConfigurationService` und dem
Recovery-Lebenszyklus. Falls eine UI-Bridge einen nicht verfuegbaren Owner oder
frische Evidence nicht erhaelt, liefert sie `Unavailable`, nicht eine
abgeschwaechte Ausfuehrung.

Die Command-Bridge mappt nur vorhandene semantische Aktionen auf
`CommandEnvelope`, die existierenden `decide*`-Funktionen, den
`RunPersistenceCoordinator` und den bestehenden Configuration-Preview-/Commit-
Pfad. Sie uebergibt Touch als `CommandSource::LocalDisplay` und Web als
`CommandSource::WebInterface`. Sie darf weder `safetyAllows*`,
Sensorvaliditaet, FaultReset-Evaluation noch Recoveryevidenz aus einer
Oberflaeche entgegennehmen oder erfinden.

Die explizite Fallbackauswahl bleibt nur dann als Command verfuegbar, wenn
der bestehende Coordinator den vollstaendig validierten Fallbackzustand und
seine frischen Evidenzen bereitstellt. Sie verlangt eine strukturierte
Bestaetigung und folgt danach ausschliesslich dem vorhandenen
write-before-apply-Fallbackpfad. Andernfalls bleibt sie `Unavailable` oder
`Rejected`; weder die Snapshotprojektion noch die Bestaetigung aktivieren
einen Run.

Damit ein bestehendes `FallbackRecovered` nicht wie ein unbestimmtes
`SAFE_BOOT`-Detail verschwindet, ergaenzt die Boot-/Application-Projektion eine
explizite nicht-aktivierende `FallbackSelectionRequired`-Disposition. Sie
verwendet ausschliesslich den bereits vorhandenen Fallbackrecord und bleibt
bis zum erfolgreichen bestehenden Fallback-Commit im Interlock nicht
freigabefaehig. Der Wechsel ist eine Sichtbarkeits- und
Command-Brueckenanpassung des bestehenden #90-Vertrags, keine neue
Recoveryklassifikation, kein automatisches Resume und kein Safety-Gate.

### 5.3 Sprache, Branding, Theme und bestehende Konfiguration

Die Build-Konfiguration beschreibt die enthaltenen Identitaeten, keine
Rendererassets:

```text
R1_ACTIVE_BRANDING=manuengineer
R1_INCLUDED_LOCALES=de,en,es
R1_INCLUDED_THEMES=manuengineer-dark
R1_RUNTIME_BRANDING_SELECTION=NO
R1_RUNTIME_LOCALE_SELECTION=YES
R1_RUNTIME_THEME_SELECTION=YES
```

`UserConfiguration.displayLanguageId` bleibt der persistente aktive
Localewert. Die Konfigurationsdomane wird von Schema V1 auf V2 erweitert und
ergaenzt `activeThemeId`. V1 wird bei der bestehenden, versionierten
Decode-/Migrationbehandlung auf `manuengineer-dark` abgebildet. V2 validiert
den Theme-Identifier strukturell und gegen den stabilen Firmware-Themekatalog;
der bestehende Active-/Fallback-Manifest-, Preview-, Integritaets- und
atomare Commitpfad bleibt unveraendert. Es entstehen weder ein neuer Recordtyp,
Root, Slot, Commitpfad, Persistenzdienst noch eine zweite Konfigurationsquelle.

Ein gespeicherter, bekannter Locale- oder Theme-Identifier, der in einem
konkreten Build nicht enthalten ist, wird beim **Anzeigen** nicht mutiert:
Text faellt aktiv -> Englisch -> sichtbarer Key zurueck, Theme auf das
vollstaendige Standardtheme. Die Laufzeit-Auswahl bietet nur enthaltene Werte
an. Damit fuehrt ein reduzierter Build nicht zu einer stillen
Konfigurationsumschreibung und kein Anzeige-Fallback veraendert Lauf, Safety
oder Persistenz.

Die drei R1-Packs enthalten alle von #25 eingefuehrten Plattform- und
App-TextKeys in Deutsch, Englisch und Spanisch. Jeder Pack deklariert seine
benoetigten Zeichen, Fontanforderung, maximale Textlaengenklasse und
320x240-Eignung. #25 erzeugt weder Fonts noch Assets. Renderer #31 muss diese
Deklarationen gegen die gewaehlte Font-/Asset-/Displaykette nachweisen.

## 6. Daten-, Command- und Snapshot-Fluesse

### 6.1 Lesen und Aktualisieren

```text
canonical app/platform state
  -> FermentationUiProjector
  -> complete FermentationUiSnapshot(revision, domain revisions, models)
  -> Touch adapter and Web adapter read the same snapshot
  -> update hint(revision only)
  -> adapter reads a new complete snapshot
```

Der Updatehinweis ist keine fachliche Deltaquelle, keine Uhr und keine
Sessionaktivitaet. Jede Oberflaeche darf eine andere Route und Darstellung
behalten, verwendet aber fuer dieselbe fachliche Situation denselben Snapshot.
Die Websprache bleibt browser-/sitzungsbezogen; die lokale persistente
Sprachwahl aendert sie nicht.

### 6.2 Commandfluss

```text
Touch/Web
  -> command(context + expected revisions + intent)
  -> stale check against current full snapshot
  -> confirmation check
  -> existing app decision / coordinator / config service
  -> Accepted | Rejected | ConfirmationRequired | Busy | Unavailable
  -> current snapshot revision and update hint
```

Bei `ConfirmationRequired` mutiert nichts. Bei `Rejected`, `Busy` oder
`Unavailable` bleibt der fachliche Zustand unveraendert; die Antwort nennt nur
den strukturierten Grund. Bei `Accepted` wird nicht behauptet, dass ein Aktor
freigegeben wurde. Bestehende write-before-apply-, Commit-indeterminate-,
Stale- und Safetyorakel bleiben die Autoritaet.

### 6.3 Recoveryfluss

```text
Current FERMENTING + missing trusted UTC
  -> RecoveryEvaluation / WaitingForTrustedTime snapshot
  -> all-off; no persistence mutation; later same-evidence reevaluation

Current FERMENTING + exact evidence + trusted UTC
  -> existing logical recovery path
  -> CurrentRunRecovered snapshot
  -> fresh independent gates still decide any future actuation

Fallback candidate
  -> FallbackSelectionRequired snapshot
  -> explicit confirmed selection only
  -> existing fallback coordinator path or Rejected/Unavailable

untrusted/invalid/negative-time evidence
  -> RecoveryRejectedOrFailClosed snapshot
  -> no auto-resume and no guessed display values
```

## 7. Fehler- und Fallbackverhalten

- Fehlende Plattform- oder App-Section wird als sichtbar nicht verfuegbar
  modelliert. Fehler einer App-Section isolieren Plattformstatus und
  Plattformservice.
- Der Header zeigt fehlende Netzverbindung und fehlende trusted Zeit als
  Zustand, nicht als nachgerechnete Daten. Regelung und Safety haengen davon
  nicht ab.
- Der Textresolver gibt nie eine leere oder still deutsch ersetzte
  Uebersetzung aus: aktive Locale, dann Englisch, dann qualifizierter Key.
- Themevollstaendigkeit wird vor Nutzung geprueft. Ein fehlendes Token,
  unbekanntes Theme oder ausgeschlossener Theme fuehrt deterministisch zum
  vollstaendigen `manuengineer-dark`-Standardtheme.
- Snapshot- oder Teilrevisionmismatch, fehlen frischer appseitiger Evidenz,
  Configuration Runtime Failure, Persistence Busy/Indeterminate und nicht
  vorhandene Funktion liefern den korrekten strukturierten Commandstatus,
  nie einen stillen Retry oder eine lokale Mutation.
- `SAFE_BOOT`, `ServiceRequired`, Recoveryrejection und Fault werden sichtbar,
  bleiben aber ohne Aktorfreigabe. Der lokale Service-Tracker schliesst nur auf
  vom owning Safety-/Servicepfad explizit gemeldete Invalidierung; er bewertet
  keinen Fault neu.
- Alle unbekannten Enum- oder Contractwerte werden fail closed als
  `Unavailable`/`RecoveryRejectedOrFailClosed` und sichtbarer technischer
  TextKey behandelt.

## 8. Tests und Akzeptanzkriterien

Die Umsetzung fuegt nur native, rendererunabhaengige Tests hinzu oder erweitert
direkt betroffene vorhandene Tests. Es gibt keine Hardware-, Display-, Touch-,
GPIO-, RTC-, WLAN-, NTP- oder Browsertests fuer #25.

| Nachweis | Prueffall |
|---|---|
| Shell | Headerinhalt, exakt vier sichtbare Slots, leere Slots und unveraenderte Slotzahl/-breite. |
| Navigation | Home auf erster Unterebene, Back ab tiefer Ebene, ein Segment pro Back, Home zur App-Home-Route, Exit-/Confirmation-Sperre. |
| Erweiterungen | Plattformsections vor Appsections; eine fehlerhafte/nicht verfuegbare Appsection isoliert Plattform und weitere Appsections. |
| Modelle | Home-Modi, Navigation, Temperatur mit unveraenderter `SensorQualitySnapshot`, Meldungen, Recovery, Status und Service enthalten keine Renderer-/Hardwaretypen. |
| Revisionen | korrekte Snapshot-/Teilrevision wird angenommen; stale Snapshot, State-, Run-, Message-, Fault-, Config- und Recoveryrevision wird ohne Mutation abgelehnt. |
| Commandergebnisse | jede der fuenf Kategorien `Accepted`, `Rejected`, `ConfirmationRequired`, `Busy`, `Unavailable`, einschliesslich Bestaetigungsreplay und ohne doppelte Nebenwirkung. |
| Snapshot/Update | Touch und Web lesen denselben vollstaendigen Snapshot; ein Hint aktiviert keine Session und ein anschliessender Read liefert die aktuelle Revision. |
| Recovery | `Normal`, Waiting, Current recovered, Fallback selection, rejected/fail-closed, Completed und Cooling werden nur aus kanonischen Quellen projiziert; kein Test behauptet Aktorfreigabe oder neue Recoverysemantik. |
| Sprache | alle #25-Keys in DE/EN/ES; aktiver Pack -> EN -> sichtbarer Key, getrennte Namespaces und fehlende/exkludierte aktive Locale ohne Persistenzmutation. |
| Branding/Theme | ManuEngineer als Buildstandard, Buildkatalog begrenzt Auswahllisten, Dark als einziger R1-Theme und unvollstaendiges/ausgeschlossenes Theme faellt auf den Standard zurueck. |
| Konfiguration | V1-zu-V2-Theme-Default, V2-Roundtrip, Manifest-/Preview-/Commitreuse, unbekannte Theme-ID abgelehnt und keine zweite Persistenzquelle. |
| Sessions | lokale 10 Minuten nur relevante Inaktivitaet, kein lokales R1-Absolutlimit, Ende bei Logout/Restart/expliziter Safetyinvalidierung; Web 5 Minuten Inaktivitaet plus 15 Minuten absolut, beide Leases getrennt. |
| Architektur | Plattformtypen enthalten keine Fermentations-, Renderer-, HTML-, LVGL-, GPIO-, ESP-IDF- oder Test-Supportabhaengigkeit; Apptypen enthalten keine konkreten Adapter. |

Nach einer freigegebenen Umsetzung sind mindestens die neuen UI-Contract- und
Fermentation-UI-Tests sowie die direkt beruehrten Konfigurations-, Command-,
Boot-/Recovery- und Persistenztests gezielt auszufuehren. Die exakten
Testverzeichnisse ergeben sich aus dem finalen Diff; der erwartete Befehl ist
`pio test -e native --filter <betroffenes-testverzeichnis>`. Zusaetzlich sind
`git diff --check`, `python3 scripts/check_architecture_boundaries.py` und
`python3 scripts/check_secrets.py` auszufuehren. Ein vollstaendiger nativer
oder ESP-IDF-Lauf sowie Hardwaretests erfolgen nur nach den kanonischen Gates
und ausdruecklicher Owneranweisung.

## 9. Geplante Dateien

| Datei | Aenderung |
|---|---|
| `lib/device_platform/src/device_ui_contracts.hpp/.cpp` | neue generische IDs, Revision-, Command-, Confirmation-, Result- und Updatewerttypen. |
| `lib/device_platform/src/device_ui_shell.hpp/.cpp` | Header, Navigation, vier Slots, statischer Extensioncatalog und Fehlerisolation. |
| `lib/device_platform/src/device_ui_text.hpp/.cpp` | Namespaces, Textpacks, DE/EN/ES-Fallback und Packmetadaten. |
| `lib/device_platform/src/device_ui_theme.hpp/.cpp` | semantische Theme-Tokens, Buildkatalog und vollstaendiger Standardfallback. |
| `lib/device_platform/src/device_ui_session.hpp/.cpp` | reine lokale/Web-Service-Leasepolicy ohne Credential- oder Safetyentscheidung. |
| `lib/fermentation_app/src/fermentation_ui_models.hpp/.cpp` | app-spezifische Home-, Temperatur-, Meldungs-, Recovery-, Status- und Servicemodelle. |
| `lib/fermentation_app/src/fermentation_ui_projector.hpp/.cpp` | reine Projektion der vorhandenen kanonischen Application-/Run-/Config-/Recoverywerte. |
| `lib/fermentation_app/src/fermentation_ui_commands.hpp/.cpp` | app-spezifische Commandvariante und verlustfreie Abbildung auf bestehende Command-/Coordinatorergebnisse. |
| `lib/fermentation_app/src/fermentation_application.hpp/.cpp` | schmale explizite UI-Facade auf den schon owned kanonischen Zustand; keine zweite Orchestrierung. |
| `lib/fermentation_app/src/boot_classification.hpp/.cpp` und direkt betroffene Interlock-/Applicationprojektion | explizite, nicht-aktivierende Fallback-Selection-Disposition statt versteckter Safe-Boot-Projektion; alle bestehenden Freigabegates bleiben unveraendert. |
| `lib/fermentation_app/src/configuration_documents.hpp/.cpp` | `UserConfiguration` V2 mit `activeThemeId` und Katalogvalidierung. |
| `lib/fermentation_app/src/configuration_document_codec.cpp`, `configuration_migration.*`, `configuration_limits.hpp`, `firmware_configuration_catalog.*` und direkt betroffene Graph-/Bootstrapstellen | V1-kompatible V2-Migration, Codec, Limits und bestehende Konfigurationskatalog-/Manifestintegration. Nur soweit der vorhandene V1-Pfad dies tatsaechlich erfordert. |
| `test/test_device_ui_contracts/test_device_ui_contracts.cpp` | generische Shell-, Text-, Theme-, Session-, Update- und Commandcontracttests. |
| `test/test_fermentation_ui_models/test_fermentation_ui_models.cpp` | kanonische Appprojektion, Recovery, Snapshots, Commands und Touch/Web-Gleichheit. |
| `test/test_configuration_documents`, `test/test_configuration_codecs`, `test/test_configuration_migration`, `test/test_configuration_service`, `test/test_boot_classification`, `test/test_run_commands`, `test/test_run_persistence_coordinator` | nur direkt erforderliche Regressionen fuer Schema V2, Revisionmapping und explizite Fallback-/Recoveryprojektion. |

`platformio.ini`, ESP-IDF-Komponenten, `dependencies.lock`, CMake-Abhaengigkeiten,
Boardprofile, Renderer-/Assetdateien und Hardwareadapter bleiben unveraendert.
Quellverzeichnisse werden bereits von den bestehenden Buildsystemen erfasst;
es wird keine Bibliothek eingebunden.

## 10. Kleine logische Implementierungsschnitte

1. **Generische Contracts und Tests:** Plattformwerttypen, Shell mit vier
   Slots, statischer Erweiterungskatalog, Command-/Updategrundtypen und reine
   Sessionpolicy implementieren und nativ testen.
2. **Text, Theme und bestehende Konfiguration:** Buildkatalog, DE/EN/ES-
   Packs, Fallback und semantische Themes implementieren; User Configuration
   V2 samt V1-kompatibler Migration ausschliesslich ueber den bestehenden
   Config-Graphen verifizieren.
3. **Fermentationsprojektion:** reine Home-, Temperatur-, Message-,
   Recovery-, Status- und Servicemodelle gegen die vorhandenen kanonischen
   Typen bauen. Insbesondere alle #124/#90-Faelle ohne Policyaenderung
   abdecken.
4. **Commandbridge und Vollsnapshotaktualisierung:** erwartete Revisionen,
   Bestätigungen und alle Ergebnisformen verlustfrei auf bestehende
   Command-/Persistenz-/Configvertraege abbilden; Touch und Web verwenden
   denselben Servicevertrag. Fehlende frische Evidence bleibt `Unavailable`.
5. **Gezieltes Abschlussreview:** gesamten Diff gegen diesen Plan, Quellen,
   Modulgrenzen, Safety-/Recoverygrenzen, KISS, Tests und Dokumentation
   pruefen; nur danach die betroffenen nativen Nachweise ausfuehren und im
   Draft-PR dokumentieren.

Diese Schnitte sind logische Review- und Risikogrenzen, keine separaten PRs.
Sie bleiben im selben Draft-PR #142 und werden nicht als neue Issue-,
Roadmap- oder Plan-PRs aufgeteilt.

## 11. Risiken und offene Ownerentscheidungen

| Risiko | Behandlung im Plan |
|---|---|
| UI koennte Fach- oder Safetylogik duplizieren. | Projector kopiert nur kanonische Entscheidungen; Bridge delegiert an bestehende Commands/Coordinatoren; Tests pruefen keine neue Freigabe. |
| Fallback oder fehlende UTC koennte als Resume missverstanden werden. | Explizite Recoverymodellierung, Bestaetigung, bestehender write-before-apply-Pfad und fail-closed Mapping; keine Autoaktivierung. |
| Theme-/Localeauswahl koennte einen neuen Persistenzpfad erzeugen. | Ausschliesslich User Configuration V2 im vorhandenen Manifest-/Preview-/Commitpfad; V1-Migration mit Standardtheme. |
| Generische UI koennte zum Framework anwachsen. | Werttypen, statischer begrenzter Catalog und kleines Vokabular; keine Plugin-, Widget-, Discovery-, Transport- oder Rendererplattform. |
| Web- und Touchsessions koennten vermischt werden. | Zwei getrennte Policy-/Leaseinstanzen, eigene Tests und keine gegenseitige Aktivitaet. |
| Sprachtexte/Fonts koennten #31 vorwegnehmen. | Nur Keys, Packmetadaten und Fallback; keine Fonts, Assets, Formate oder Messbehauptungen. |

Es gibt keine weitere fachliche Ownerentscheidung, die vor dem Planreview
aufgeloest werden muss. Offen und zwingend ist ausschliesslich die
Ownerfreigabe der exakten Plan-Commit-SHA. Rendererwahl, reale
Touch-/Display-/Font-/Asset-/Hardwareabnahme, Authentisierung und
Webtransport bleiben bewusst ausserhalb dieses Plans und bei ihren owning
Issues.

## 12. Lizenz- und Dependency-Bewertung

Die Analyse gegen `ADOPT_OR_BUILD.md`, das Release-1-Audit und das aktuelle
Dependency-Register ergibt fuer #25:

| Kandidat/Ansatz | Entscheidung fuer #25 | Begruendung |
|---|---|---|
| kleine eigene C++-Werttypen, feste Arrays und pure Funktionen | verwenden | Contracts, Fallback, vier Slots und Leaseablauf benoetigen keine externe Laufzeit; sie sind transparent, nativ testbar und haben keine Dritt-Lizenz- oder Ressourcenlast. |
| LVGL oder anderer Renderer | nicht auswaehlen | Rendererwahl, Lizenz, ESP-IDF-Integration, Font-/Assetworkflow, Flash/RAM/CPU und reale Displaymessung sind #31. |
| HTML-, Web-, HTTP-, JSON- oder Frontendbibliothek | nicht auswaehlen | #25 besitzt keinen Transport, kein HTML und keinen Codec. Die bestehenden getrennten Evaluationspfade bleiben unveraendert. |
| i18n- oder Pluginruntime | nicht auswaehlen | Drei statische Packs, stabile Keys und deterministischer Fallback sind kleiner, ohne Discovery und ohne Laufzeitabhaengigkeit ausreichend. |

Folglich aendert #25 weder `dependencies.lock` noch Third-Party-Notices oder
Build-/Toolchainkonfiguration. Eine spaetere externe Bibliothek benoetigt ihren
eigenen Kandidatenvergleich mit Version, Lizenz, Herkunft, Ressourcenmessung,
Integrationsgrenze und Ownerentscheidung.

## 13. Definition of Done

- Der eine Draft-PR #142 enthaelt Roadmap-Sync, diesen Plan und erst nach
  expliziter Ownerfreigabe derselben Plan-SHA die Umsetzung.
- Alle gemeinsamen Contracts sind klein, stark typisiert, nativ testbar,
  rendererunabhaengig und auf `device_platform`/`fermentation_app` korrekt
  zugeordnet.
- Shellheader, vier Slots, Home/Back, Plattform-vor-App und Fehlerisolation
  sind nachweisbar; leere Slots bleiben sichtbar.
- Fermentationsmodelle decken Home, Navigation, Temperatur/Qualitaet,
  Meldungen, Recovery, Status und Service ab und projizieren nur kanonische
  Zustandsentscheidungen.
- Expected revisions, stale rejection, Confirmation, Accepted, Rejected,
  Busy und Unavailable sowie Vollsnapshot-/Updateverhalten sind nativ
  nachgewiesen.
- Touch und Web teilen dieselben fachlichen Snapshots und Commands, ohne
  Layout, Navigation, Sprache oder Sessionadapter gleichzuschalten.
- ManuEngineer ist Buildstandard; DE/EN/ES, Namespace-, Fallback-,
  Theme- und Buildauswahlvertraege bestehen; Dark ist einziges R1-Theme und
  faellt vollstaendig fail closed zurueck.
- Lokale 10-Minuten-Inaktivitaet ohne R1-Maximaldauer und getrennte Web-5/15-
  Policy sind ohne neue Auth-/Safetypolicy nachgewiesen.
- Keine Renderer-, HTML-, Hardware-, GPIO-, Asset-, Font-, Plugin-,
  Bibliotheks-, neuen Safety-/Recovery-/Sensor-/Auth- oder neuen
  Persistenzpolicy-Aenderungen sind enthalten.
- Gezielte lokale Nachweise werden mit exaktem Befehl und Ergebnis im Draft-PR
  festgehalten. Nicht ausgefuehrte Nachweise werden als `NOT_RUN` berichtet.
- Vor Ownerfreigabe dieser exakten Plan-SHA bleibt `IMPLEMENTATION=NOT_STARTED`.
