# Issue #25 – Gemeinsame UI-Modelle, Device-Shell-Vertraege und Mehrsprachigkeit

## 1. Ausgangslage, Revision und Baseline

Dies ist die vollstaendige, eigenstaendig ausfuehrbare Planrevision fuer Issue
#25. Sie ersetzt den gesamten vorherigen Planinhalt; zur Umsetzung ist keine
aeltere Planfassung heranzuziehen.

    ISSUE=25
    TITLE=[E4.1] Gemeinsame UI-Modelle, Device-Shell-Vertraege und Mehrsprachigkeit
    PR=142_DRAFT
    BRANCH=feature/issue-25-device-ui-contracts
    WORKFLOW=PLAN_FIRST_SINGLE_PR
    BASE_BRANCH=integration/r1-development
    BASE_SHA=86e55499d9f0dd4dbd2d9fbc95d04549df4d429c
    ROADMAP_COMMIT=e6c051e69eb84d578f6a662033548ff2cacc7771
    SUPERSEDES_PLAN_SHA=49c7b86f600f9a79cd348030c3d4e68af2d5cbe6
    PLAN_COMMIT=THIS_COMMIT
    IMPLEMENTATION=NOT_STARTED
    OWNER_PLAN_REVIEW_REQUIRED=YES
    ACTUATOR_RELEASE=NO
    NATIVE_TESTS=NOT_RUN
    ESP_IDF_BUILD=NOT_RUN
    HARDWARE_TEST=NOT_RUN

Vor dieser Revision wurden live verifiziert:

- main und integration/r1-development zeigen beide auf BASE_SHA;
- Issue #25 ist offen und hat den oben genannten Titel;
- PR #142 ist offen, Draft, hat den genannten Head-Branch und die genannte
  Base;
- die Roadmap-Aenderung bleibt unveraendert in ROADMAP_COMMIT;
- der Arbeitsbaum ist vor der Planbearbeitung sauber;
- die geschlossenen Grundlagenissues #121, #124 und #126 und ihre aktuellen
  Vertraege sind weiterhin die relevante Safety-, Recovery- und Zeitbasis.

Der Roadmap-Commit dokumentiert den erfolgreichen kumulativen #135-Checkpoint
und beginnt #25 bewusst im selben Feature-PR. Diese Plankorrektur erzeugt
keinen neuen Roadmap-Statusgrund; docs/ROADMAP.md wird deshalb nicht veraendert.

Die Analyse fuer diese vollständige Revision umfasst das Live-Issue #25, die
Roadmap, AGENTS.md im Repository und in den beiden betroffenen Modulen,
docs/AGENT_WORKFLOW.md, docs/ENGINEERING_PRINCIPLES.md,
docs/CI_AND_QUALITY_GATES.md, ADR-013, ADR-017, ADR-018 und ADR-019 in
docs/DECISIONS.md sowie:

- docs/DEVICE_UI_ARCHITECTURE_DECISIONS.md;
- docs/LOCAL_UI.md und docs/LOCAL_UI_SETTINGS_SERVICE.md;
- docs/WEB_UI.md;
- docs/SETTINGS_AND_STORAGE.md und die aktuelle Konfigurationsimplementierung;
- die #121/#124/#126-Vertraege in docs/ARCHITECTURE.md,
  docs/REQUIREMENTS.md, docs/SYSTEM_SAFETY_AND_RECOVERY.md,
  docs/RECOVERY_AND_INTERRUPTION.md, docs/RUN_PERSISTENCE.md und
  docs/STATE_MACHINE.md;
- die aktuellen device_platform- und fermentation_app-Header, deren direkte
  Implementierungen und die vorhandenen nativen Command-, Persistenz-,
  Bootklassifikations-, Interlock- und Konfigurationstests.

## 2. Ziel, Scope und Nicht-Ziele

Ziel ist eine kleine, nativ testbare und rendererunabhaengige Vertragsschicht
fuer die spaetere lokale Touchoberflaeche und Weboberflaeche. Sie stellt eine
feste lokale Device-Shell, gemeinsame fachliche Fermentations-Snapshots,
typisierte Mutation-Commands mit bestehenden kanonischen Staleness- und
Idempotenzpfaden, Text-, Branding-, Theme- und Sessionwerttypen bereit.

Die Vertrage projizieren kanonische Fach-, Sensor-, Konfigurations-,
Persistenz-, Safety- und Recoveryentscheidungen. Sie berechnen weder
Fachzustand, Sensorrollen, Freigaben, Authentisierung noch Recovery neu.
Insbesondere gibt es genau eine kleine aktive Conformance-Korrektur: Der
bereits vollstaendig validierte Fallbackstatus wird nach dem bestehenden
R5.9-Vertrag als nicht-aktivierendes Resume-Angebot klassifiziert statt
irrefuhrend als pauschales Safe Boot. Das ist keine neue Recoverypolicy und
keine neue Aktor-Allow-Regel.

Die folgende Grenze ist verbindlich:

    device_platform
        portable, anwendungsneutrale Shell-, Text-, Theme-, Session- und
        Command-Ergebniswerttypen
    fermentation_app
        Fermentations-Snapshot, Fachrevisionen, Fachcommand-Bridge,
        Fallbackklassifikation und Composition
    device_platform_esp_idf
        unveraendert; kein neuer Zeit-, Renderer- oder Hardwareadapter

Nicht Teil von #25 sind:

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

Reale Display- und Touchintegration, Pixel-, Font- und Assetentscheidungen,
Treiber, Hardware und Ressourcenmessung verbleiben bei Issue #31. Die
simulierte lokale Bedienlogik ist nicht Teil dieses Contractschnitts. HTTP,
HTML, Browsertransport, normale Anmeldung und deren Implementierung bleiben
bei ihren owning Issues.

KISS-Konsolidierung:

    GLOBAL_DOMAIN_UI_SNAPSHOT_GATE=NO
    GENERIC_DYNAMIC_REVISION_REGISTRY=NO
    SECOND_IDEMPOTENCY_LAYER=NO
    SECOND_RETRY_REGISTRY=NO
    SECOND_PERSISTED_REQUEST_ID=NO
    SECOND_SESSION_OR_AUTH_LOGIC=NO
    SECOND_PERSISTENCE=NO
    SECOND_RECOVERY_LOGIC=NO
    PLUGIN_WIDGET_DISCOVERY_PLATFORM=NO

## 3. Bestehende Architektur und Reuse-Befund

### 3.1 Wiederzuverwendende Plattform- und Anwendungsgrundlagen

ADR-013 und die lokalen AGENTS.md bleiben unveraendert: device_platform bleibt
portabel und kennt keine Fermentation; fermentation_app verwendet nur schmale
abstrakte Plattformvertraege und keine ESP-IDF-, GPIO-, Netzwerk- oder
Dateisystemadapter.

| Vorhandene Quelle | Reuse in #25 | Keine Neuerfindung |
|---|---|---|
| device_platform::ITimeSource | monotonicMillis fuer reine Leaseablaufrechnung und optionale trusted UTC als Clock-Eingabe | keine RTC-, SNTP- oder Zeitqualitaetslogik |
| device_platform::INetworkStatus | semantischer Netzwerkindikator im lokalen Shellheader | kein WLAN- oder Prozessmanagement |
| ITimeZoneResolver | bereits validiert/vorbereitet einen kanonischen IANA-Identifier | kein UTC-zu-Lokalzeit-Konverter |
| SensorQualitySnapshot und SensorQuality | unveraendert in appseitige Temperaturanzeige projizieren | keine UI-Sensorbewertung oder Rollenwahl |
| RuntimeMessage, MessageCode, MessageClass und MessageTrigger | appseitige Meldungen, Quittierungs- und Mutekommandos | Quittierung behebt keinen Fault und gibt nichts frei |
| ProcessRuntimeState, ProcessState, RunCommandState, RunProgramSnapshot und EffectiveControlContext | Home-, Prozess-, Temperatur- und Statusprojektion | Renderer leitet keinen Modus oder Zielwert aus Einzelwerten her |
| CommandEnvelope, CommandStatus und die bestehenden decide/apply-Funktionen | kanonische Command-ID, Quelle, erwartete State-/Run-/Message-/Fault-/Recoveryrevisionen und Ergebnis | kein zweiter Command- oder Replayvertrag |
| RunPersistenceCoordinator und RunPersistenceResult | vorhandene Write-before-Apply- und unknown-safe-Semantik | kein zweiter Coordinator oder UI-Commitpfad |
| UserConfiguration, RuntimeConfigurationSnapshot, ConfigurationService und Manifest-/Preview-/Commitpfad | persistente lokale Displaysprache und geplantes activeThemeId | kein weiterer Record, Root, Slot oder NVS-Bereich |
| PresentationState, ApplicationLifecycleState, RunLoadDisposition und RecoveryDisposition | appseitige Status-, Service- und Recoveryprojektion | kein zweites Safety-Gate |

Die vorhandene UserConfiguration besitzt displayLanguageId und timeZoneId. Der
Sprachkatalog kennt die stabilen R1-IDs de, en und es. activeThemeId wird
deshalb als UserConfiguration-Schema-V2-Feld im vorhandenen
Konfigurationsgraphen geplant, nicht als UI-Sonderpersistenz.

### 3.2 Unveraenderte Safety-, Recovery- und Zeitgrenzen

- Jeder Boot beginnt all-off und mit ActuatorSafetyGateStatus::Unresolved.
  ActuationInterlock bleibt die einzige Freigabebewertung.
- Kein UI-Snapshot, Refreshhinweis, Text, Theme, Bestaetigung oder
  Servicelease kann Allowed erzeugen.
- SAFE_BOOT, Fault, untrusted Persistenz und unklare Konfigurations- oder
  Recoveryevidenz bleiben sichtbar, fail-closed und aktorfrei.
- Der enge #124-Current-FERMENTING-Fall bleibt unveraendert: nur exakte
  bestehende Evidenz und trusted aktuelle UTC erlauben automatische logische
  Recovery. Fehlt UTC, bleibt WaitingForTrustedTime im RAM ohne Mutation; dies
  gibt keine Aktoren frei.
- priorBootPhaseElapsed, observedRunSeconds, die bestehende FSM-Completion und
  die #124-Zeitrechnung werden nicht durch die UI geaendert oder beschrieben
  als biologische oder physische Ausfallrekonstruktion.
- #126 bleibt die Quelle der optionalen trusted UTC. Gemeinsame UI-Typen
  enthalten weder DS3231, I2C, SNTP, WLAN, Credentials noch Hardwarezeit.
- Der R5.9-#90-Fallback ist nur bei vollstaendig validiertem Head-, Slot-,
  CRC-, Schema-, Epoch- und Referenzschutz ein nicht-aktivierendes Angebot.
  Es gibt keine automatische Promotion, kein automatisches Resume und keine
  C2- oder Charge-Gutschrift.

## 4. Vorgeschlagene minimale Architektur

### 4.1 Drei getrennte Projektionen

Die gemeinsame fachliche Wahrheit ist ein renderer-, sprach- und
surfaceunabhaengiger FermentationUiSnapshot:

    FermentationUiSnapshot
        domain revisions
        FermentationHomeView / Prozessdaten
        TemperatureView mit vorhandener SensorQualitySnapshot
        MessageView
        RecoveryView
        ApplicationStatusView
        ServiceAvailabilityView und semantische Permissions
        stabile TextKeys und semantische ActionIds

Er enthaelt keine aktuelle Browserroute, keine lokale Shellroute, keine
aufgeloeste Uebersetzung und keine aktive Oberflaechenlocale. Touch und Web
lesen diesen selben Fachsnapshot und rufen dieselben fachlichen Commands auf.

Der lokale Device-/Touchvertrag ist ein eigener, zusammengesetzter
LocalDeviceShellState. Er besitzt:

- Branding, lokale persistente Displaysprache, Netzwerkindikator und
  ClockViewInput im Header;
- genau vier sichtbare BottomSlots;
- lokale Home-/Back-Hierarchie, lokale Route und PageExitRequirement;
- die festen Plattformbereiche Status und Service sowie statische
  App-Erweiterungen.

Der Webadapter darf aus demselben FermentationUiSnapshot eine andere Route,
Navigation und ein anderes Layout bauen. Seine Locale stammt aus Browser oder
Websession, nicht aus UserConfiguration:

    WEB_LOCALE_SOURCE=browser_or_web_session
    WEB_LOCALE_MUTATES_USER_CONFIGURATION=NO
    WEB_NAVIGATION_MAY_DIFFER=YES

Eine Aenderung von displayLanguageId betrifft nur die lokale Device-Shell.
Eine Websprachwahl aendert weder Prozessdaten noch lokale Displaysprache noch
Konfiguration.

### 4.2 Lokale Shell und begrenzte Erweiterungen

DeviceShellHeader ist rendererunabhaengig und enthaelt BrandingId, LocaleId,
NetworkIndicator und ClockViewInput. ClockViewInput besteht mindestens aus:

    trustedUtc: optional<int64_t>
    canonicalTimeZoneId: TimeZoneId

Die App/Composition liest die bereits kanonisch konfigurierte timeZoneId und
uebergibt sie als Wert. device_platform und ein kuenftiger Renderer lesen nicht
FermentationUserConfiguration. Fehlt trustedUtc, ist die Uhr explizit
Unavailable. #25 fuegt weder eine ESP-IDF-Zeitzonenumrechnung noch eine
Rendererformatierung hinzu und behauptet nicht, ITimeZoneResolver konvertiere
UTC in lokale Zeit.

BottomSlots sind ein std::array<BottomSlot, 4>. Jeder Slot bleibt sichtbar.
Nicht benoetigte Slots sind Empty, werden nie entfernt und vergroessern keine
Nachbarslots. Auf der ersten Ebene unterhalb Home ist Home sichtbar; auf einer
tieferen Ebene ersetzt Back Home und entfernt genau ein lokales Routensegment.
PageExitRequirement verhindert stilles Verwerfen ungespeicherter Aenderungen
und das Umgehen kritischer Bestaetigungen. Diese Shellnavigation ist kein
fachlicher oder persistenter Command.

Ein StaticUiExtensionCatalog erhaelt bei der Composition eine begrenzte,
statische Folge von Section-Deskriptoren. Plattformsections stehen stets vor
Appsections. Jede Appsection ist einzeln Available oder Unavailable mit
sichtbarem technischem Grund; ein Appfehler isoliert weder Plattformsection
noch andere unabhaengige Appsections. Der Catalog verwendet keine Reflection,
shared-library-Ladung, Plugin-API oder Runtime-Discovery.

Das gemeinsame minimale Vokabular ist Header, BottomSlot, PageTitle,
StatusBadge, TemperatureValue, ListRow, Action, Confirmation, MessageBanner
und VerticalPager. Es beschreibt Semantik und Bedienzustand, keine Pixel,
Koordinaten, Rendererklassen oder Screens. Anforderungen an Zeichensatz,
Textlaenge, Font und 320x240 bleiben rendererunabhaengige
Abnahmeanforderungen fuer Issue #31.

### 4.3 Refresh ist kein Fachcommand-Gate

UiRefreshRevision ist optionaler, fluechtiger Plattformwert fuer
Snapshot-/Cache-Invalidierung. Sie wird nicht persistiert und kann nur steigen,
wenn sich wirklich veroeffentlichter UI-Inhalt aendert; ein reiner Read erhoeht
sie nicht. Hinweise duerfen verlorengehen oder zusammengefasst werden. Der
naechste vollstaendige Snapshot ist autoritativ.

    UI_REFRESH_REVISION_IS_DOMAIN_GATE=NO
    UI_REFRESH_REVISION_PERSISTED=NO
    UI_REFRESH_REVISION_COMMAND_REQUIRED=NO
    DOMAIN_REVISIONS_REMAIN_AUTHORITY=YES

Eine Uhr- oder Netzwerkstatusaenderung darf einen Refreshhinweis ausloesen,
aber niemals einen fachlich weiterhin gueltigen Command ablehnen. Es wird
keine UiSnapshotRevision als erwartete Commandrevision eingefuehrt und kein
Fullsnapshot gegen einen Command verglichen. Die Bridge prueft ausschliesslich
die je Command relevante bestehende kanonische Fachrevision.

### 4.4 Commands, Ergebnisse und Bestaetigung

device_platform enthaelt nur die generischen Commandbausteine:

    UiSurface
    UiRequestId
    UiRefreshRevision
    GenericConfirmationMetadata
    DeviceUiCommandOutcomeCategory
    DeviceUiCommandResult

DeviceUiCommandOutcomeCategory hat genau Accepted, Rejected,
ConfirmationRequired, Busy und Unavailable. DeviceUiCommandResult transportiert
ausserdem stabilen technischen Grund, optionale generische
ConfirmationMetadata und einen optionalen Refreshhinweis, aber keine
Fermentationsrevisionen oder Fachcommandvarianten.

FermentationUiExpectedRevisions gehoert in fermentation_app. Es modelliert nur
die fuer die konkrete Variante erforderlichen bestehenden kanonischen
Revisionen, beispielsweise expectedStateSequence, expectedRunRevision,
expectedMessageRevision, expectedFaultRevision,
expectedRecoveryEpisodeRevision oder die passende
UserConfigurationRevision. Eine Variante fordert nur ihre tatsaechlich
relevanten Werte. Es gibt keine globale Liste aller Domainrevisionen und keine
dynamische Domain-Key-/Revision-Registry:

    GENERIC_DYNAMIC_REVISION_REGISTRY=NO

Die Bridge prueft diese Werte gegen die jeweilige kanonische owning
Entscheidung. Ein Mismatch wird als Rejected mit dem stabilen Grund
StaleDomainRevision abgebildet, ohne Mutation. Ein Refreshhint oder ein nur
geaenderter Shellheader ist kein Stale-Grund.

UiRequestId ist keine zweite Idempotenzschicht. Fuer jede Mutation, deren
bestehender kanonischer Pfad CommandEnvelope verwendet, bildet die Bridge die
gueltige UiRequestId ohne Hash, Zaehler oder Verlust genau auf
CommandEnvelope::id ab. CommandEnvelope::source wird weiterhin aus UiSurface
auf LocalDisplay oder WebInterface abgebildet. processedCommandIds,
AlreadyProcessed, bereits persistierte Command-ID-Semantik und die vorhandenen
Write-before-Apply-Ausgaenge bleiben die alleinige Autoritaet.

    SECOND_IDEMPOTENCY_LAYER=NO
    SECOND_RETRY_REGISTRY=NO
    SECOND_PERSISTED_REQUEST_ID=NO

UiRequestId ist nur Bestandteil einer envelopebasierten Commandvariante und
hat genau diesen einen Consumer. Eine Mutation ohne vorhandenen
CommandEnvelope-Vertrag traegt keine UiRequestId, statt einen zweiten
CommandEnvelope-Vertrag zu erfinden.

Ein kanonischer Pfad ohne CommandEnvelope, etwa die vorhandene
activateFallbackRecoveredRun-Operation, erhaelt keinen kuenstlichen zweiten
Envelope oder Requestspeicher. Er bleibt beim bereits owning
Recovery-/Persistenzvertrag. Die UI legt auch dort keine Retryregistry an.

Eine gefaehrdete Mutation wird beim ersten unbestaetigten Aufruf vor jeder
owning Mutation mit ConfirmationRequired beantwortet. Metadata bindet
semantische ActionId, TextKey, Zielbeschreibung und die appseitig geforderten
Revisionswerte. Die bestaetigte Wiederholung derselben envelopebasierten
Aktion verwendet dieselbe UiRequestId und damit dieselbe CommandEnvelope::id,
setzt nur das bestehende confirmed-Feld und durchlaeuft danach den kanonischen
Pfad. NotConfirmed mutiert nicht; ein echter Duplicate fuehrt nach dem
bestehenden AlreadyProcessed/AlreadyPersisted-Vertrag zu keiner zweiten
Nebenwirkung. Das strukturierte Ergebnis bewahrt den kanonischen
Detailstatus; ein bereits erfolgreich angewendeter Duplicate kann als Accepted
mit der expliziten Detailangabe AlreadyApplied dargestellt werden, nicht als
neuer Erfolg.

Reine lokale Shellnavigation und Webnavigation erzeugen weder UiRequestId noch
CommandEnvelope:

    PURE_NAVIGATION_USES_COMMAND_ENVELOPE=NO

Nur fachliche Run-, Message-, Konfigurations-, Recovery- oder geschuetzte
Servicezustandsmutationen gehen durch ihren jeweiligen vorhandenen kanonischen
Pfad. Die Bridge nimmt nie Sensor-, Safety-, Aktor- oder Recoveryevidenz aus
Touch oder Web entgegen.

## 5. Konkrete Typen, Ports, Modelle und Ownership

### 5.1 device_platform

| Typ oder Regel | Verantwortung |
|---|---|
| BrandingId, LocaleId, ThemeId, TimeZoneId, TextNamespace und TextKey | kleine generische Werttypen; keine Fermentationsnamen |
| DeviceUiBuildCatalog | statisch enthaltene Brandings, Locales und Themes sowie aktives Buildbranding |
| DeviceShellHeader, ClockViewInput, BottomSlot, ShellRoute und LocalShellNavigation | lokaler Device-Shellvertrag ohne Renderer oder Fachroute |
| StaticUiExtensionCatalog, UiSectionDescriptor und UiSectionAvailability | feste Plattform-vor-App-Reihenfolge und Fehlerisolation |
| UiSurface, UiRequestId, UiRefreshRevision, GenericConfirmationMetadata, DeviceUiCommandOutcomeCategory und DeviceUiCommandResult | generische Transport-, Bestaetigungs- und Ergebnistypen; keine Fachrevisionen |
| TextPackManifest, TextLookupResult und resolveText | Namespaces, statische Packs und deterministischer Fallback |
| ThemeToken, ThemeDescriptor und resolveTheme | semantische Tokens, Vollstaendigkeit und Standardfallback |
| ServiceSessionPolicy, ServiceSessionLease und ServiceSessionEvent | reine Zeit-/Invalidierungslogik ohne Credential, Login oder Safetybewertung |

device_platform kennt keine Fermentationsphase, Programmnamen, FaultCode,
RecoveryDisposition, UserConfiguration, ProcessState, Sensorrolle, HTML,
LVGL, GPIO, ESP-IDF, Persistenz- oder Test-Support-Abhaengigkeit. Kleine
Werttypen und constexpr-/pure Ablaufregeln bleiben header-only, wenn dies
klarer ist; eine leere cpp nur zur Symmetrie wird nicht angelegt.

### 5.2 fermentation_app

| Typ oder Anpassung | Verantwortung |
|---|---|
| FermentationUiSnapshot | gemeinsamer sprach- und surfaceunabhaengiger Fachsnapshot mit Domainrevisionen und Teilmodellen |
| FermentationUiExpectedRevisions | je Fachcommand statisch typisierte, relevante bestehende kanonische Revisionen |
| FermentationHomeView | ProcessState, Run-/Programmsnapshot, effektive Werte, kanonische Primaeraktion und Laufstatus |
| TemperatureView | Rolle, optionaler Wert, unveraenderte SensorQualitySnapshot und vorhandene Fehlerursache |
| MessageView | RuntimeMessage, Prioritaet, Aktivitaet, Quittierung, Entscheidungspflicht und kanonische Revision |
| RecoveryView | Normal, WaitingForTrustedTime, CurrentRunRecovered, FallbackSelectionRequired, RecoveryRejectedOrFailClosed, Completed oder Cooling ausschliesslich aus kanonischen Quellen |
| ApplicationStatusView und ServiceAvailabilityView | Lifecycle, PresentationState, Fault, Resetursache, vorhandene Gate-/Berechtigungsprojektion; kein neues Gate |
| FermentationUiProjector | reine Projektion explizit uebergebener owning Application-/Coordinatorwerte |
| FermentationUiCommand und FermentationUiCommandBridge | appseitige Varianten, Revisionsauswahl und verlustfreie Delegation an bestehende Pfade |
| fermentation_ui_text | fermentationseigene TextKeys und deren DE/EN/ES-Uebersetzungen |
| BootClassification, RunLoadDisposition und FermentationApplication | die nach Abschnitt 6.3 beschriebene R5.9-Fallback-Conformance-Korrektur |

FermentationUiProjector hat keinen globalen Zustand, keinen ESP-IDF-Zugriff und
keine Abhaengigkeit auf device_platform_test_support. FermentationApplication
bleibt Owner von runtimeRunState, pendingResume, der neuen
pendingFallbackResume-Aufbewahrung, pendingRecoverySource,
RunPersistenceCoordinator, ConfigurationService und des Recoverylebenszyklus.

### 5.3 Textpacks, Branding und Themes

Platform- und Apptexte sind auch physisch getrennt:

    device_platform
        TextKey/Namespace/Pack/Resolver
        platform-owned keys und platform-owned DE/EN/ES translations
    fermentation_app
        fermentation-specific TextKeys
        fermentation-specific DE/EN/ES translations
    static composition
        combines both fixed pack sets

    fermentation_app -> device_platform
    device_platform -X-> fermentation_app

Fermentationsphasen, Programmnamen und fachliche Benutzermeldungen werden
nicht in device_platform abgelegt. Der Resolver verwendet zuerst die fuer die
Oberflaeche aktive Locale, dann Englisch, danach den sichtbaren voll
qualifizierten technischen Key. Er gibt keine leere oder still deutsch
ersetzte Uebersetzung aus. Jeder Pack beschreibt nur rendererunabhaengig
benoetigten Zeichensatz, Textlaengenklasse und 320x240-Eignung; #25 liefert
keine Fonts oder Assets.

R1 hat folgende feste Buildcomposition:

    R1_ACTIVE_BRANDING=manuengineer
    R1_INCLUDED_LOCALES=de,en,es
    R1_INCLUDED_THEMES=manuengineer-dark
    R1_RUNTIME_BRANDING_SELECTION=NO
    R1_RUNTIME_LOCALE_SELECTION=YES
    R1_RUNTIME_THEME_SELECTION=YES
    ACTIVE_BUILD_BRANDING_MUST_BE_INCLUDED=YES
    ENGLISH_FALLBACK_MUST_BE_AVAILABLE=YES
    DEFAULT_THEME_MUST_BE_COMPLETE_AND_AVAILABLE=YES

ManuEngineer ist damit das Buildzeit-Standardbranding. Manuengineer-dark ist
das einzige R1-Theme und besitzt vollstaendige semantische Tokens, einschliesslich
on- und Overlay-Tokens. Fehlendes Token, unbekanntes Theme, unvollstaendiger
Descriptor oder ausgeschlossener Descriptor faellt fail-closed auf den
vollstaendigen im Build enthaltenen Standarddescriptor zurueck.

Persistente ID-Geltung, Buildinclusion, Auswahl und Aufloesung sind getrennt:

| Ebene | Vertrag |
|---|---|
| PERSISTED_ID_VALIDITY | de, en, es und manuengineer-dark sind stabile bekannte R1-IDs. Ein strukturell ungueltiger oder echt unbekannter gespeicherter Identifier wird weiterhin durch den bestehenden Konfigurationsvertrag abgelehnt. |
| BUILD_INCLUDED_ID | DeviceUiBuildCatalog beschreibt die konkret eingebaute Teilmenge. Der R1-Build enthaelt alle drei Locales und manuengineer-dark. |
| RUNTIME_SELECTION_ALLOWED | Auswahllisten enthalten nur im aktuellen Build enthaltene IDs. Branding ist nicht laufzeitwaehlbar. |
| DISPLAY_RESOLUTION | Ein bekannter, aber in einem anderen reduzierten Build ausgeschlossener Wert korrumpiert UserConfiguration nicht; die Anzeige faellt ohne Write auf Englisch beziehungsweise das vollstaendige Standardtheme zurueck. |

UserConfiguration wird von V1 auf V2 erweitert: activeThemeId wird im
bestehenden Dokument, Codec, Validierungs-, Migrations-, Manifest-, Preview-
und atomaren Commitpfad gefuehrt. V1 wird auf manuengineer-dark migriert. Es
entsteht kein neuer Konfigurationsrecord, Root, Slot, Service oder NVS-Bereich.

## 6. Daten-, Command-, Snapshot- und Recoveryfluesse

### 6.1 Lesen, lokale Shell und Web

    canonical app and platform state
        -> FermentationUiProjector
        -> complete FermentationUiSnapshot with domain revisions
        -> Touch and Web consume the same domain data

    local composition + local route + displayLanguageId
        -> LocalDeviceShellState with header and exactly four slots
        -> local Touch renderer later

    browser/web-session locale + web route
        -> Web presentation later

    changed published content
        -> optional UiRefreshRevision hint
        -> consumer reads next complete authoritative snapshot

Der lokale Shellheader darf sich wegen Netzwerk oder Uhr aktualisieren. Das
aendert keinen Fachsnapshotcommand und keine Domainrevision. Ein verlorener
oder zusammengefasster Hinweis ist erlaubt; Hintergrundaenderungen erneuern
keine Servicelease.

### 6.2 Fachcommandfluss

    Touch or Web domain mutation
        -> FermentationUiCommand with FermentationUiExpectedRevisions
        -> command-specific canonical revision check
        -> confirmation check when required
        -> existing run/configuration/recovery/service owner
        -> Accepted | Rejected | ConfirmationRequired | Busy | Unavailable
        -> optional refresh hint; consumer reads complete snapshot

Die Bridge prueft nie eine globale UI-Refreshrevision. Ein Command mit
unveraenderten fuer ihn relevanten kanonischen Revisionen bleibt bei einer
zwischenzeitlich geaenderten Uhr oder Netzwerkdarstellung fachlich zulaessig:

    CLOCK_OR_NETWORK_ONLY_UI_CHANGE
    + DOMAIN_REVISIONS_UNCHANGED
    -> DOMAIN_COMMAND_NOT_REJECTED_FOR_UI_REFRESH_REVISION

Rejected, Busy und Unavailable aendern den Fachzustand nicht. Accepted
behauptet keine Aktorfreigabe. ConfirmationRequired mutiert nicht. Die
existierenden Stale-, Safety-, Persistenz- und unknown-safe-Orakel bleiben
Autoritaet.

### 6.3 R5.9-Fallback: vollstaendige Conformance-Korrektur

Der heutige technische Befund ist:

    RunPersistenceLoadStatus::FallbackRecovered
    -> RunPersistenceCoordinatorState::FallbackRecoveryPending
    -> RunLoadDisposition::SafeBoot

Das letzte Mapping wird fuer den vollstaendig validierten Fallback an den
bestehenden R5.9-Produktvertrag angepasst. Dazu wird explizit eingefuehrt:

    RunLoadDisposition::FallbackSelectionRequired
    BootClassification::FallbackSelectionRequired

boot_classification::classifyRunLoad ordnet FallbackRecovered dieser neuen
Disposition zu; classify ordnet sie der neuen BootClassification zu.
RunPersistenceLoadStatus::FallbackRecovered und
RunPersistenceCoordinatorState::FallbackRecoveryPending bleiben unveraendert
erhalten. Alle anderen untrusted, unvollstaendigen oder technisch negativen
Loadstatus bleiben SafeBoot.

FermentationApplication ergaenzt eine schmale
prepareFallbackSelection-Funktion. Sie rekonstruiert den bereits vollstaendig
validierten Fallbacksnapshot in die app-owning Aufbewahrung
pendingFallbackResume. Sie setzt weder runtimeRunState noch einen aktiven Run,
haelt die neue RunLoadDisposition und publiziert nur
RecoveryView::FallbackSelectionRequired. Bis zu einem Applied bleibt die
Application in diesem unresolved/aktorfreien Angebot und der Coordinator in
FallbackRecoveryPending.

Die sichtbaren Aktionen sind vollstaendig begrenzt:

- ResumeFallback ist die einzige fachlich mutierende Aktion. Sie ist
  bestaetigungspflichtig.
- Back, Home oder das Verlassen der Ansicht sind reine Navigation und lassen
  pendingFallbackResume, Persistenz und den unresolved Zustand unveraendert.
- Es gibt keinen Discard-, Abort-, Tombstone- oder Auto-Promotion-Button.

Eine neue Discardsemantik wird nicht still erfunden:

    NEW_FALLBACK_DISCARD_POLICY_WITHOUT_OWNER=NO

Insbesondere wird discardAsNoActiveRun nicht fuer diesen Fall wiederverwendet:
sein heutiger Vertrag akzeptiert Ready oder LoadedActiveRun, nicht
FallbackRecoveryPending. Ein zusaetzlicher persistenter Discardpfad waere eine
neue Recovermutation und ist ausserhalb von #25; ein entsprechender Bedarf
stoppt die Umsetzung fuer eine Ownerentscheidung.

Nach bestaetigtem ResumeFallback entnimmt nur FermentationApplication die
retained Snapshotquelle, aktuelle Checkpointzeit und frische owning-app
Config-, Sensor-, Planner- und sonstige erforderliche Evidenz. Sie delegiert
an den vorhandenen RunPersistenceCoordinator::activateFallbackRecoveredRun-
Write-before-Apply-Pfad. Die UI liefert keine dieser Evidenzen. Bei jedem
anderen Ergebnis als Applied bleiben RAM und das Angebot fail-closed
unveraendert oder der vorhandene Coordinatorstatus ist sichtbar.

Nur bei Applied uebernimmt die Application den vom Coordinator
zurueckgegebenen Kandidaten in die bestehende FSM-/Application-Anwendung,
loescht die pending-Aufbewahrung und publiziert die aus dem jetzt
kanonisch-committeten Zustand abgeleitete vorhandene Disposition. Sie behaelt
die Bootdiagnose FallbackRecovered nicht ueber Applied hinaus, sondern leitet
den passenden Post-Commit-Loadstatus Current oder NoActiveRun und den vom
bestehenden Write-before-Apply-Pfad gelieferten Coordinatorstatus Ready oder
ReadyEmpty aus dem committeten Ergebnis ab. Erst danach wird
ActuationInterlock mit frischer owning-app Evidenz erneut und unveraendert
ausgewertet. FallbackSelectionRequired selbst ist eine ausdrueckliche
deny-Disposition; ActuationInterlock bekommt keine neue Allow-Regel. Ein
spaeteres Allowed kann ausschliesslich durch die bereits vorhandenen
Resume-/Fresh-Evidence, Applied, FSM-Anwendung und die bestehende Interlocklogik
entstehen.

    FallbackRecovered / FallbackRecoveryPending
        -> FallbackSelectionRequired, unresolved and all-off
        -> explicit confirmed ResumeFallback
        -> existing activateFallbackRecoveredRun
        -> Applied
        -> existing FSM application
        -> fresh evidence and existing ActuationInterlock evaluation

Das ist keine automatische Aktivierung, keine automatische Promotion, keine
C2-Gutschrift und keine neue Safety- oder Recoveryentscheidung.

### 6.4 Recoveryprojektion ausserhalb des Fallbacks

    Current FERMENTING + missing trusted UTC
        -> RecoveryEvaluation / WaitingForTrustedTime in RAM
        -> all-off; no persistence mutation; later same-evidence reevaluation

    Current FERMENTING + exact evidence + trusted UTC
        -> existing logical recovery path
        -> CurrentRunRecovered projection
        -> fresh independent gates still decide any future actuation

    untrusted/invalid/negative-time recovery evidence
        -> RecoveryRejectedOrFailClosed
        -> no guessed values and no auto-resume

RecoveryRejectedOrFailClosed bleibt ausschliesslich eine
recovery-spezifische Projektion. Ein unbekannter generischer Shell-, Text-,
Theme-, Locale- oder anderer UIwert wird als generisches Unavailable, mit
sicherem Darstellungsfallback und sichtbarem technischem Key behandelt. Er
erhaelt keine Recoverysemantik.

## 7. Sprach-, Theme-, Branding- und Uhrvertrag

Die lokale Device-Shell verwendet UserConfiguration.displayLanguageId als
persistente Displaysprache; der vorhandene Konfigurationsservice validiert und
committet sie. Der Webadapter verwendet seine Browser- oder Sessionlocale
eigenstaendig. Beide Oberflaechen nutzen denselben Textresolververtrag, aber
koennen unterschiedlich aufgeloeste Texte sehen.

Themewahl ist eine persistente lokale R1-Laufzeitwahl im bestehenden
UserConfiguration-V2-Pfad. Die Buildcomposition entscheidet, was enthalten
ist. resolveTheme schreibt nie in UserConfiguration. Das aktive Branding ist
die Buildentscheidung ManuEngineer und nicht laufzeitwechselbar.

ClockViewInput bewahrt trustedUtc und canonicalTimeZoneId zusammen. Bei
fehlender UTC bleibt die Uhr unverfuegbar. Bei vorhandener UTC und Zone wird
die Kombination an den spaeteren Renderer weitergegeben; #25 implementiert
weder lokale Zeitberechnung noch Format, und der Renderer liest keine
Fermentationskonfiguration.

## 8. Sessionvertrag

device_platform implementiert nur ServiceSessionPolicy,
ServiceSessionLease und pure Ablauf-/Invalidierungslogik. Die konkrete
Fermenter-R1-Composition liefert die Werte:

    TOUCH_SERVICE_POLICY={inactivity=10_MIN, absolute=NONE}
    WEB_SERVICE_POLICY={inactivity=5_MIN, absolute=15_MIN}

Relevante geschuetzte Touchbedienungen erneuern nur die lokale Lease.
Hintergrundupdates, Messwerte, Uhr, Netzwerk und Refreshhints nicht. Die
lokale Lease endet nach Inaktivitaet, Geraeteneustart, ausdruecklichem Logout
oder einem vom bestehenden Safety-/Serviceowner gemeldeten relevanten
Zustandswechsel. #25 definiert oder bewertet keine neue Sicherheitsereignisliste
und keine lokale R1-Maximaldauer.

Die Web-Servicepolicy ist nicht die normale Webloginpolicy:

    WEB_SERVICE_POLICY_IS_NORMAL_WEB_LOGIN_POLICY=NO
    NORMAL_WEB_SESSION_30MIN_12H_UNCHANGED=YES

Die bestehende normale fluechtige Webanmeldung mit 30 Minuten Inaktivitaet und
12 Stunden absolut, ihre Credentials, Widerrufe und Authentisierung bleiben
unveraendert. Die 5/15-Servicelease bildet keine neue Anmeldung und ersetzt
keine fachliche oder Safetypruefung. Touch und Web teilen weder Lease noch
Aktivitaet.

## 9. Fehler- und Fallbackverhalten

- Fehlende Plattform- oder Appsection wird sichtbar als Unavailable
  dargestellt. Ein Appsectionfehler isoliert Plattform und andere Appsections.
- Fehlende trusted UTC und Offline-Netzwerk werden als solche angezeigt; die UI
  rechnet keine Uhrzeit, Sensorwerte oder Safetyzustand nach.
- Texte fallen aktive Locale, Englisch, sichtbarer vollqualifizierter Key
  zurueck. Themefehler fallen vollstaendig auf das buildverfuegbare
  Standardtheme zurueck.
- Unbekannte generische Werte verwenden Unavailable und sichtbaren technischen
  Key; nur unbekannte Recoveryprojektion verwendet
  RecoveryRejectedOrFailClosed.
- Ein relevanter kanonischer Revisionsmismatch liefert Rejected ohne Mutation.
  Ein Refreshmismatch gibt es nicht.
- Configuration Runtime Failure, Persistence Busy oder Indeterminate, fehlende
  frische owning-app-Evidenz und nicht vorhandene Funktion werden strukturiert
  als Busy oder Unavailable gemeldet; es gibt keinen stillen UI-Retry.
- Safe Boot, Fault und Recoveryrejection bleiben sichtbar und aktorfrei.
  Der Sessiontracker reagiert nur auf explizite Invalidierungssignale seines
  Owners und klassifiziert keinen Fault selbst.

## 10. Tests und Akzeptanzkriterien

Die Umsetzung ergaenzt nur native rendererunabhaengige Contracttests und die
direkt betroffenen bestehenden Regressionstests. Es gibt keine Hardware-,
Display-, Touch-, GPIO-, RTC-, WLAN-, NTP- oder Browsertests fuer #25.

| Nachweis | Verpflichtender Testfall |
|---|---|
| Lokale Shell | Header mit Branding, Locale, Netzwerk und ClockViewInput; exakt vier sichtbare Slots; Empty bleibt sichtbar; keine Renderer-/Pixeltypen |
| Clock | fehlende trustedUtc ergibt unavailable; trustedUtc und canonicalTimeZoneId bleiben gemeinsam im Contract; keine erfundene Lokalzeit; Renderer greift nicht auf Fermentationskonfiguration zu |
| Lokale Navigation | Home auf erster Unterebene, Back auf tieferen Ebenen entfernt exakt ein Segment; ExitRequirement verhindert stilles Verlassen |
| Erweiterungen | Plattformsections vor Appsections; eine fehlerhafte Appsection isoliert Plattform und weitere Appsections; keine Runtime-Discovery |
| Gemeinsamer Snapshot | Home, Temperatur/SensorQuality, Meldungen, Recovery, Status und Service sind sprach-/surfaceunabhaengig; lokale Route und Locale fehlen; Touch und Web lesen dieselbe Fachprojektion |
| Refresh | UiRefreshRevision steigt nur bei geaendertem veroeffentlichten Inhalt, nicht pro Read; verlorene/zusammengefasste Hints sind erlaubt; Fullsnapshot ist autoritativ |
| Kein globales Stale-Gate | CLOCK_OR_NETWORK_ONLY_UI_CHANGE plus unveraenderte Domainrevisionen akzeptiert den Domaincommand; nur relevante kanonische Revisionen koennen StaleDomainRevision ausloesen |
| Fachrevisionen | jede Commandvariante prueft nur ihre typisierten bestehenden State-, Run-, Message-, Fault-, Recovery- oder Konfigurationsrevisionen; keine dynamische Revisionregistry |
| Ergebnisse | Accepted, Rejected, ConfirmationRequired, Busy und Unavailable einschliesslich stabiler Gruende und ohne neue Aktorfreigabe |
| Bestaetigung/ID | unbestaetigter Command mutiert nicht; bestaetigte Wiederholung nutzt dieselbe UiRequestId zu CommandEnvelope::id-Abbildung; echter Duplicate erzeugt keine zweite Nebenwirkung und bewahrt den bestehenden AlreadyProcessed/AlreadyPersisted-Ausgang |
| Navigation | lokale und Webnavigation erzeugen keinen CommandEnvelope und keine persistierte ID |
| Text | Plattform- und Apptypen/Packs sind physisch getrennt; alle #25-Keys DE/EN/ES; aktive Locale zu Englisch zu sichtbarem Key |
| Build und Theme | ManuEngineer ist enthaltenes aktives Branding; R1 hat de/en/es und manuengineer-dark; Auswahl nur aus Buildinclusion; English und vollstaendiges Standardtheme immer verfuegbar; bekannt-ausgeschlossenes Fixture korrumpiert keine Konfiguration und schreibt keinen Fallback |
| Konfiguration | V1-zu-V2-Defaulttheme, V2-Roundtrip, strukturell/echt unbekannte ID abgelehnt, vorhandener Manifest-/Preview-/Commitpfad, keine zweite Persistenzquelle |
| Sessions | Touch 10 Minuten relevante Inaktivitaet ohne Absolutlimit; Ende bei Restart, Logout und ownergemeldeter Safetyinvalidierung; Web 5/15 getrennt; normale Weblogin-30/12 unveraendert |
| Fallbackklassifikation | FallbackRecovered und FallbackRecoveryPending werden zu FallbackSelectionRequired, retained und unresolved; FallbackSelectionRequired kann nie Allowed sein |
| Fallbackaktion | nur bestaetigtes ResumeFallback ruft activateFallbackRecoveredRun mit frischer owning-app-Evidenz; vor Applied keine RAM-/FSM-/Aktoraktivierung; Applied vor FSM; danach frische bestehende Interlockevaluation |
| Fallbacknavigation | Back/Home/Verlassen mutiert nichts; kein Discard/Abortpfad; vorhandenes discardAsNoActiveRun wird nicht bei FallbackRecoveryPending aufgerufen |
| Regression | Bootklassifikation, RunPersistenceCoordinator und ActuationInterlock testen die angepasste Disposition; Current-FERMENTING, WaitingForTrustedTime, SafeBoot und alle bisherigen Allow-Gates bleiben unveraendert |
| Keine Neuinterpretation | kein Test darf aus UIwerten Sensor-, Safety- oder Recoveryevidenz ableiten, Fallback automatisch aktivieren oder #124-Current-FERMENTING anders klassifizieren |
| Architektur | device_platform enthaelt keine Fermentations-, Renderer-, HTML-, LVGL-, GPIO-, ESP-IDF- oder Test-Support-Abhaengigkeit; fermentation_app kennt keine konkreten Adapter |

Nach freigegebener Umsetzung werden mindestens die neuen Contracttests und die
direkt beruehrten vorhandenen Tests in test/test_boot_classification,
test/test_actuation_interlock, test/test_run_persistence_coordinator,
test/test_run_commands, test/test_configuration_documents,
test/test_configuration_codecs, test/test_configuration_migration und
test/test_configuration_service gezielt nativ ausgefuehrt. Der konkrete Befehl
wird nach dem finalen Diff gemaess docs/CI_AND_QUALITY_GATES.md als
pio test -e native mit den betroffenen Filtern dokumentiert. Daneben folgen
git diff --check, scripts/check_architecture_boundaries.py und
scripts/check_secrets.py. Vollstaendige native oder ESP-IDF-Laeufe und
Hardwaretests erfolgen nur nach den kanonischen Gates und ausdruecklicher
Owneranweisung.

## 11. Geplante Dateien

| Datei | Geplante Verantwortung |
|---|---|
| lib/device_platform/src/device_ui_contracts.hpp und bei nicht-trivialer Implementierung device_ui_contracts.cpp | generische IDs, UiSurface, UiRefreshRevision, ConfirmationMetadata, Ergebniswerte und Buildkatalog ohne Fachrevisionen |
| lib/device_platform/src/device_ui_shell.hpp und bei Bedarf device_ui_shell.cpp | ClockViewInput, lokaler Shellheader, vier Slots, lokale Navigation, statischer Extensioncatalog und Fehlerisolation |
| lib/device_platform/src/device_ui_text.hpp/.cpp | generische Namespace-/Pack-/Resolvertypen plus nur platform-owned Texte |
| lib/device_platform/src/device_ui_theme.hpp/.cpp | semantische Themewerte, Buildinclusion und vollstaendiger Standardfallback |
| lib/device_platform/src/device_ui_session.hpp | reine Policy-/Lease-/Invalidierungslogik; header-only soweit einfacher |
| lib/fermentation_app/src/fermentation_ui_models.hpp/.cpp | FermentationUiSnapshot, Teilmodelle und FermentationUiExpectedRevisions |
| lib/fermentation_app/src/fermentation_ui_projector.hpp/.cpp | reine kanonische Application-/Run-/Config-/Recoveryprojektion |
| lib/fermentation_app/src/fermentation_ui_commands.hpp/.cpp | fachliche Varianten, Confirmationfluss, UiRequestId-zu-CommandEnvelope-Abbildung und Delegation |
| lib/fermentation_app/src/fermentation_ui_text.hpp/.cpp | fermentation-owned TextKeys und DE/EN/ES-Packs |
| lib/fermentation_app/src/boot_classification.hpp/.cpp | FallbackSelectionRequired in RunLoadDisposition und BootClassification |
| lib/fermentation_app/src/fermentation_application.hpp/.cpp | retained pendingFallbackResume, prepare/resume handoff und app-owned frische Evidenzgrenze |
| lib/fermentation_app/src/actuation_interlock.hpp/.cpp | neue FallbackSelectionRequired-Disposition explizit deny behandeln, ohne Allow-Regel |
| lib/fermentation_app/src/configuration_documents.hpp/.cpp, configuration_document_codec.*, configuration_migration.*, configuration_limits.hpp und firmware_configuration_catalog.* | UserConfiguration V2, activeThemeId, Validierung, Migration und vorhandene Katalog-/Manifestintegration |
| test/test_device_ui_contracts/test_device_ui_contracts.cpp | generische Shell-, Text-, Theme-, Session-, Refresh- und Commandcontracttests |
| test/test_fermentation_ui_models/test_fermentation_ui_models.cpp | fachliche Snapshot-/Touch-Web-/Revisions-/Command-/Fallbackprojektion |
| test/test_boot_classification, test/test_actuation_interlock, test/test_run_persistence_coordinator, test/test_run_commands und Konfigurationstests | direkt erforderliche Regressionen fuer Fallback, IDs, V2 und bestehende Safety-/Persistenzvertraege |

Nur tatsaechlich benoetigte cpp-Dateien werden angelegt. platformio.ini,
ESP-IDF-Komponenten, dependencies.lock, CMake-Abhaengigkeiten, Boardprofile,
Renderer-/Assetdateien und Hardwareadapter bleiben unveraendert.

## 12. Kleine logische Implementierungsschnitte

1. Generische Plattformwerttypen und native Tests: Shell mit genau vier
   Slots, ClockViewInput, statischer Extensioncatalog, Text-/Themegrundtypen,
   Refreshhint und reine Sessionlease.
2. Text-, Theme- und Konfigurationsschnitt: physisch getrennte Packs,
   Buildinclusion, Resolverfallback und UserConfiguration V2 ausschliesslich
   ueber bestehenden Configuration-Graph/Migrationspfad.
3. Appprojektion: gemeinsamer FermentationUiSnapshot ohne Locale/Route,
   surfaceeigener Shellcomposer, appseitige ExpectedRevisions und
   Touch/Web-Gleichheit.
4. Commandbridge: command-spezifische kanonische Revisionpruefung,
   Confirmation-Replay und exakte UiRequestId-zu-CommandEnvelope-Abbildung,
   ohne Navigation oder zweiten Idempotenzspeicher.
5. R5.9-Conformance-Korrektur: FallbackSelectionRequired,
   pendingFallbackResume, bestaetigtes ResumeFallback über den vorhandenen
   Coordinator, unveraenderter Interlock-Deny vor Applied und direkte
   Boot-/Persistence-/Interlockregressionen.
6. Gezieltes Abschlussreview und die nach CI_AND_QUALITY_GATES erlaubten
   betroffenen nativen Nachweise. Alle Schnitte verbleiben im selben Draft-PR
   #142 und werden weder zu neuen Plan-, Roadmap- noch Feature-PRs.

## 13. Risiken, offene Ownerentscheidungen und Gates

| Risiko | Planbehandlung |
|---|---|
| Refreshinformationen koennten fachliche Commands blockieren. | Kein Command akzeptiert oder prueft UiRefreshRevision; nur relevante kanonische Domainrevisionen gelten. |
| Plattformtypen koennten Fermentationswissen aufnehmen. | Fachrevisionen, Snapshot, Commands und Apptexte liegen ausschliesslich in fermentation_app. |
| Touchshell koennte Webnavigation oder Locale erzwingen. | Gemeinsamer Fachsnapshot ohne Route/Locale; LocalDeviceShellState und Websession bleiben getrennt. |
| UTC ohne Zone oder falscher Resolververtrag. | ClockViewInput enthaelt beide Werte; keine Konvertierungsbehauptung oder Adapterimplementierung. |
| UI-IDs koennten Replay/Persistenz duplizieren. | Verlustfreie Abbildung nur auf bestehenden CommandEnvelope; keine neue Registry, Persistenz oder Envelope-Surrogate. |
| Fallback koennte als Freigabe missverstanden werden. | Retained unresolved Angebot, Resume nur bestaetigt, Applied vor FSM und anschliessend unveraenderte frische Interlockevaluation. |
| Ein Discard waere unbemerkt neue Recoverypolicy. | Kein Discard in #25; Bedarf an neuem persistierenden Pfad stoppt fuer Ownerentscheidung. |
| Theme- oder Localefallback koennte Konfiguration still umschreiben. | Known-vs-included getrennt; Anzeigefallback ohne Write im bestehenden V2-Pfad. |

Vor der Umsetzung gibt es keine weitere fachliche Ownerentscheidung innerhalb
dieses korrigierten Scopes. Zwingendes Gate bleibt die ausdrueckliche Freigabe
der exakten neuen Plan-Commit-SHA. Jede neue Discardpolicy, Rendererwahl,
Font-/Assetentscheidung, Webauthentisierung, ESP-IDF-Zeitadapter,
Hardwareaenderung oder abweichende Safety-/Recoverysemantik erfordert einen
neuen Ownerentscheid und gegebenenfalls eine erneute Planrevision.

## 14. Lizenz- und Dependency-Bewertung

Die Pruefung gegen docs/ADOPT_OR_BUILD.md und die bestehenden
Release-1-Dependencyvorgaben ergibt:

| Kandidat oder Ansatz | Entscheidung | Grund |
|---|---|---|
| kleine eigene C++-Werttypen, feste Arrays und pure Funktionen | verwenden | die statischen Contracts, vier Slots, Textfallback und Leases sind transparent, nativ testbar und haben keine Dritt-Lizenz- oder Ressourcenlast |
| LVGL oder anderer Renderer | nicht auswaehlen | Rendererwahl, Lizenz, ESP-IDF-Integration, Font/Assetworkflow und Ressourcenmessung sind #31 |
| HTML-, Web-, HTTP-, JSON- oder Frontendbibliothek | nicht auswaehlen | #25 besitzt weder Transport noch HTML noch Webcodec |
| i18n- oder Pluginruntime | nicht auswaehlen | drei statische Packs und deterministischer Fallback sind kleiner; Discovery und Runtimekomplexitaet waeren unnoetig |

Folglich aendert #25 weder dependencies.lock noch Third-Party-Notices,
Build-/Toolchainkonfiguration oder Lizenzbestand. Eine spaetere externe
Bibliothek braucht einen eigenen Kandidatenvergleich mit Herkunft, Version,
Lizenz, Ressourcenmessung, Integrationsgrenze und Ownerentscheidung.

## 15. Definition of Done

- PR #142 bleibt der eine Draft-PR mit Roadmap-Sync, dieser freigegebenen
  Planrevision und erst danach der Implementation.
- device_platform enthaelt nur generische Shell-, Text-, Theme-, Session-,
  Refresh- und Ergebnistypen; fermentation_app besitzt alle
  Fermentationssnapshots, Fachrevisionen, Fachcommands und Apptexte.
- Die lokale Shell zeigt Header und exakt vier sichtbare Slots, samt
  Home/Back, leeren Slots, Plattform-vor-App und Fehlerisolation, ohne
  Browsernavigation oder Pixelvertrag zu erzwingen.
- Touch und Web konsumieren denselben sprach- und surfaceunabhaengigen
  FermentationUiSnapshot und dieselben fachlichen Mutationcontracts, aber
  behalten eigene Route, Layout, Locale und Sessions.
- UiRefreshRevision ist nur Cache-/Hintinformation, nie persistiert oder
  Commandgate; alle Staleentscheidungen beruhen nur auf relevanten bestehenden
  kanonischen Domainrevisionen.
- UiRequestId erzeugt bei CommandEnvelope-Pfaden keine zweite Idempotenz,
  sondern exakt dessen ID; Confirmation, NotConfirmed, AlreadyProcessed und
  Persistenz haben keine doppelte Nebenwirkung.
- ManuEngineer, DE/EN/ES, aktive Locale zu Englisch zu sichtbarem Key,
  getrennte Textpacks, known-vs-included, dark-only R1 und fail-closed
  Standardtheme sind nativ nachweisbar.
- ClockViewInput bewahrt trusted UTC und kanonische Zone ohne
  Konverter-/Rendererimplementierung.
- Touch-Service 10 Minuten ohne Absolutlimit und getrennte Web-Service-5/15
  sind produktkonkret composed; normale Weblogin-30/12 bleibt unveraendert.
- FallbackRecovered/FallbackRecoveryPending wird korrekt als
  FallbackSelectionRequired retained, bis Applied all-off; nur bestaetigtes
  ResumeFallback nutzt den bestehenden Write-before-Apply-Pfad, und
  ActuationInterlock gewinnt keine neue Allow-Regel.
- Keine Renderer-, HTML-, Hardware-, GPIO-, Asset-, Font-, Plugin-,
  Bibliotheks-, neue Safety-/Recovery-/Sensor-/Auth- oder zweite
  Persistenzpolicy-Aenderung ist enthalten.
- Nach Umsetzung sind die beschriebenen gezielten nativen Nachweise mit
  Befehl und Ergebnis im Draft-PR dokumentiert; nicht ausgefuehrte Nachweise
  werden als NOT_RUN ausgewiesen.
- Vor ausdruecklicher Ownerfreigabe der exakten SHA dieser Planrevision bleibt
  IMPLEMENTATION=NOT_STARTED.
