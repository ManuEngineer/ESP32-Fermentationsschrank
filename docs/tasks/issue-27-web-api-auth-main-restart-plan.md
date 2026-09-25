# Planrevision – Issue #27 auf kanonischem main neu aufsetzen

## 1. Status, Ziel und Quellen

Diese Fassung ist der vollstaendige, eigenstaendig reviewfaehige Delta-Plan fuer
Web-API, lokale Weboberflaeche, Authentication, Sessions und Bedienkonflikte.
Sie beginnt auf `main`, nicht auf einem PR-167-Commit. In dieser Planrunde
werden nur diese Datei und der aktuelle Roadmap-Status geaendert. Der Commit
mit dieser Datei ist die neue, separat freizugebende Plan-SHA; seine exakte SHA
steht nach dem Commit im neuen Draft-PR und in Issue #27. Weder die alte
Planfreigabe noch alte Implementationstests autorisieren Produktarbeit auf dem
neuen Branch.

```text
ISSUE=27
BASE_BRANCH=main
BASE_SHA=b871375f494701bed1834013cfeb789856983e3a
NEW_BRANCH=agent/issue-27-web-api-auth-main-restart
OLD_PR167_HEAD=4ee5839eb9f88311fd2107fe8a347e9d09dfc737
OLD_PR167=SUPERSEDED_REFERENCE_ONLY
OLD_PR167_MERGED=NO
PR156=MERGED_AT_b871375f494701bed1834013cfeb789856983e3a
ISSUE31=CLOSED_COMPLETED_OUTSIDE_SCOPE
PR169=MERGED_AT_b8d963e8d830b95b160dfb7e5cc9c2d033ad53e9
ESP_IDF=v6.1@fff9895c82d744c7237be8847347bdd1b07c6643
HOST_CLANG_MAJOR=21
PYYAML_CI_PIN=6.0.3
PLAN_STATUS=INDEPENDENT_PLAN_FIX_VERIFICATION_PENDING
OWNER_PLAN_APPROVAL=REQUIRED_AFTER_FIX_VERIFICATION
IMPLEMENTATION_AUTHORIZATION=NO
PRODUCT_IMPLEMENTATION_THIS_ROUND=NO
ACTUATOR_RELEASE=NO
CONTEXT_BASELINE_BRANCH=main
CONTEXT_BASELINE_SHA=b871375f494701bed1834013cfeb789856983e3a
CONTEXT_REFRESH_MODE=FULL
CONTEXT_DELTA=PR156_PRODUCT_TOUCH_CLANG21_AND_CI_ON_TOP_OF_PR169
SOURCE_OF_TRUTH_CONFLICT=OLD_PR167_AUTH_RECORD_ID_10_OBSOLETE_NUMERIC_NAMESPACE_CONFLICT
```

Quellen in Prioritaetsreihenfolge: `docs/SPECIFICATION_REVIEW.md`, akzeptierte
ADR-013/017/018/019 in `docs/DECISIONS.md`, die betroffenen Teile von
`docs/WEB_UI.md`, `docs/NETWORK.md`,
`docs/NETWORK_DIAGNOSTICS_INTEGRATION.md`, `docs/SETTINGS_AND_STORAGE.md`,
`docs/CONFIGURATION_PERSISTENCE.md`, `docs/BACKUP_SECURITY_RETENTION.md`,
`docs/ADOPT_OR_BUILD.md`, `docs/RESOURCE_BUDGET_AND_MAINTENANCE.md`,
`docs/ESP_IDF_UPGRADE_CONTRACT.md`, `docs/CI_AND_QUALITY_GATES.md`,
`docs/AGENT_WORKFLOW.md`, die lokalen `AGENTS.md` und der Code samt Tests auf
`BASE_SHA`. PR #167 und seine Audits sind historische Entwurfs- und
Evidence-Quellen; sie ueberschreiben keine aktuelle `main`-Schnittstelle.

## 2. Repository-first-Inventur und Portgrenze

| Baustein | Befund auf `BASE_SHA` | Auftrag nach Planfreigabe |
|---|---|---|
| `IHttpServerLifecycle`, `IHttpRouteSink`, `HttpRequest/Response`, ESP-IDF-`esp_http_server` und `NetworkSetupRoutes` | `ALREADY_ON_MAIN` aus #164; genau ein Server und schmale Setup-Routen | Vorhandenen Lifecycle und Setup-Owner konsumieren; nur benoetigte begrenzte Browser-Metadaten und einen kleinen Route-Dispatcher ergaenzen. |
| `FermentationApplication::uiSnapshot()`, `uiPresentationSource()`, `publishOwningRuntimeEvidence()`, `prepare*()`, `confirmPrepared()`, `applyConfirmedPrepared()` | `ALREADY_ON_MAIN` aus #25/#26/#168/#31; Apply liefert `FermentationUiCommandResult` | Diese Grenze als einzigen Run-Mutationsowner nutzen; Web liefert Werte und erwartete Revisionen, keine Runtime-/Safety-Evidence. |
| `FermentationUiCommandBridge`, `RunPersistenceCoordinator`, RAM-Ack/Mute, `ServiceSessionLease` und `fermentationWebServicePolicy()` | `ALREADY_ON_MAIN` | Fach-, Owning-/Decision- und Persistenzsemantik unveraendert konsumieren. |
| Authentication-Records/KDF, `web_session.*`, `web_browser_policy.*`, `web_api_codec.*`, `web_application_routes.*`, `web_assets.hpp`, ihre Tests | `MISSING` auf `main`, in PR #167 vorhanden | `SELECTIVE_PORT_CANDIDATE`: einzeln gegen neue Ports, Epoch-/Recovery-, UI- und Ressourcenvertraege pruefen; nur kompatibles #27-Delta portieren. |
| HTTP-Metadaten fuer Cookie, CSRF, Origin und Mutation-Sequence | `MISSING` auf `main`; PR #167 erweitert den Port/Adapter | `REIMPLEMENT_AGAINST_CURRENT_CONTRACT`: bounded Allowlist, Header-/Responsevalidierung und native/ESP-IDF-Tests; keine allgemeine Header-Map oder zweiter Server. |
| ArduinoJson `7.4.3` | `MISSING` auf `main`; in PR #167 gelockt | Nur nach aktueller ESP-IDF-6.1-Build-, Grenzwert-, Fuzz-, Lizenz- und Ressourcenpruefung als konkreter DTO-Codec adoptieren; Lockfile durch Component Manager erzeugen. |
| Alte `applyPreparedRequest()`, `decidePreparedCommand()`, `persistenceResponse()` als allgemeiner Web-Run-Vertrag; alte Kopien von `fermentation_application.*`, `fermentation_ui_commands.*`, `main/app_main.cpp`, CMake und Architekturguard | `OBSOLETE_FROM_PR167` | Nicht transplantieren oder als zweiten Application-/Command-/Persistence-Owner einfuehren. |
| Alte clang-18-, CI-, Display-/Touch- und Kalibrierungsbaseline | `OBSOLETE_FROM_PR167` | Aktuelle clang-21- und PyYAML-6.0.3-Vertraege sowie gemergtes #31 unveraendert lassen. |

`EXISTING_ON_MAIN=HTTP_SETUP_UI_APPLICATION_RUNTIME_COMMAND_PERSISTENCE_SERVICE_LEASE`;
`PORT_FROM_PR167=AUTH_SESSION_BROWSER_POLICY_CODEC_ROUTES_ASSETS_TEST_IDEAS_ONLY_AFTER_COMPATIBILITY_CHECK`;
`REIMPLEMENT_AGAINST_CURRENT_CONTRACT=HTTP_METADATA_WEB_RUN_RESULT_MAPPING_COMPOSITION`;
`DROP_AS_OBSOLETE=SECOND_APPLY_OWNER_OLD_APP_UI_COMMANDS_OLD_TOOLCHAIN_BASELINE`;
`REMAINING_NEW_DELTA=AUTH_FIRST_CONSUMER_WEB_UI_READ_API_INTERNAL_WEB_MUTATIONS_TARGETED_EVIDENCE`.

Der alte Adapter liest zur Header-Duplikaterkennung `esp_httpd_priv.h`.
Dieser private ESP-IDF-Header ist keine uebernehmbare Produktgrundlage.
Vor dem Port ist ein begrenzter Nachweis mit oeffentlichen v6.1-APIs und
fail-closed Behandlung mehrdeutiger/ueberlanger Metadaten erforderlich. Wenn
der bestehende Port das nicht sicher leisten kann, wird die Implementation
angehalten und der Plan erneut zur Entscheidung vorgelegt.
Der vorhandene Adapter begrenzt Request-Bodies auf 4096 Bytes. Die
vorgeschlagene Metadaten-Allowlist bleibt insgesamt innerhalb 2048 Bytes;
Einzelfeldgrenzen aus dem PR-167-Prototyp sind `Host`/`Origin` 256,
`Content-Type`/CSRF 64, `Cookie`/`Referer` 512, Mutation-Sequence 20 und
`Sec-Fetch-Site` 32 Bytes. Diese Werte sind vor dem Port mit der realen
DTO-/Cookie-Groesse und dem oeffentlichen Adapter-API nachzuweisen; eine
Erhoehung braucht messbare Begruendung und eine Planrevision.

## 3. Architektur und verbindlicher Application-/Web-Pfad

```text
Web-Intent / bounded DTO + erwartete Revisionen
  -> FermentationApplication::prepare*() / prepareEnvelope()
  -> FermentationApplication::confirmPrepared()
  -> FermentationApplication::applyConfirmedPrepared()
  -> FermentationUiCommandResult
  -> interne Web-HTTP-Outcome-Abbildung
```

`applyConfirmedPrepared()` ist bereits der gemeinsame Owner fuer lokale UI
und kuenftige Adapter. Nur die Application erzeugt Command-ID, bindet aktuelle
Runtime-/Safety-Evidence, entscheidet und ruft fuer persistente Run-Kommandos
den vorhandenen `RunPersistenceCoordinator` auf. `AcknowledgeMessage` und
`MuteMessage` bleiben RAM-owned und liefern nach tatsaechlichem Apply ein
`OwningOutcome` ohne erfundenen Run-Persistenzstatus. `DecisionOnly` bleibt
eine Entscheidung, nie ein bestaetigter Web-Erfolg. Ein Web-Boolean darf kein
bestaetigtes Envelope konstruieren; der Application-Confirmation-Guard und
seine Revalidierung bleiben massgebend.

```text
APPLICATION_MUTATION_OWNER=applyConfirmedPrepared
APPLICATION_RESULT_CONTRACT=FermentationUiCommandResult
WEB_RUNTIME_EVIDENCE_INJECTION=NO
SECOND_APPLICATION_OWNER=NO
SECOND_COMMAND_BUS=NO
SECOND_PERSISTENCE_TRUTH=NO
SECOND_HTTP_SERVER=NO
SECOND_CONNECTIVITY_TRUTH=NO
SECOND_AUTH_STORE=NO
```

Die interne Route verarbeitet `DeviceUiCommandOutcomeCategory`,
`FermentationUiCommandPhase` **und** den konkreten Variant-`detail`-Status.
Die Kategorie allein ist keine Erfolgsfreigabe: `Accepted/Proposed` kann
`DecisionOnly` sein. Die erste Implementierung verwendet folgende
fail-closed HTTP-Matrix fuer den internen Run-Pfad; Antwortkoerper bleiben
begrenzt und secret-frei:

| Phase / typisierter Zustand | HTTP | Wirkung |
|---|---:|---|
| `OwningOutcome` + `Accepted` + `CommandStatus::Applied/NoChange/AlreadyProcessed` (RAM-Ack/Mute) | 200 | tatsaechliches owning Ergebnis, kein Persistenzanspruch |
| `OwningOutcome` + `Accepted` + `RunPersistenceResultStatus::Applied/CheckpointWritten/AlreadyProcessed/AlreadyPersisted` | 200 | tatsaechlicher vorhandener Persistenz-Owner-Outcome |
| `DecisionOnly` + `NotConfirmed`/`ConfirmationRequired` | 409 | keine Mutation |
| `DecisionOnly` + `Proposed` oder anderes `Accepted` ohne owning Apply | 503 | fehlender owning Outcome, niemals Erfolg |
| `StaleState`/`StaleDecision`, revalidierte bestaetigte Anfrage nicht mehr aktuell | 409 | neu laden und erneut bestaetigen |
| `Busy` | 409 | keine zweite Mutation |
| invalid, not eligible, not allowed, SafetyRejected, fehlende fachliche Voraussetzung | 422 | abgelehnt; kein Erfolg |
| `ContextMissing`, `NotInitialized`, fehlende Runtime-/Safety-Evidence, `RecoveryPending`, `Blocked` | 503 | nicht verfuegbar/fail-closed |
| `PersistenceIndeterminate`, `PersistenceCommittedApplyFailed` | 503 | Recovery erforderlich; keine erfolgreiche Antwort |
| `WriteFailed` / `CapacityExceeded` | 500 / 413 | technischer Fehler / begrenzter Speicher |
| unbekannte Detailvariante, Kategorie-/Phasen-Widerspruch oder nicht owning-fremder Detailtyp | 503 | fail-closed; keine Erfolgskategorie als Ersatz |

Die Web-Route reicht keine `RunPersistenceResult`-Struktur als ihre direkte
Application-Grenze durch. Vor HTTP-Erfolg werden Phase, Kategorie und die
zulaessige Detailmenge konsistent geprueft. Die bisherige PR-167-Matrix ist
nur eine Regressionserinnerung, nicht dieser neue Vertrag. Tests beweisen
insbesondere unbestaetigt, stale, busy, invalid, `ContextMissing`, beide
indeterminierten Persistenzfaelle, Recovery, Blocked, fehlende Safety-Evidence
und erfolgreichen RAM-Ack/Mute ohne Run-Persistenz.

`device_platform` enthaelt nur allgemeine HTTP-Ports; die ESP-IDF-Umsetzung
bleibt in `device_platform_esp_idf`. Die #27-Route/Policy bleibt in
`fermentation_app`; `main/` verdrahtet sie mit dem bereits vorhandenen
#164-Setup-Router auf **einem** `IHttpRouteSink`-/Serverlebenszyklus. Setup
behaelt seine Modus-Routen. Keine zweite Credential-, Netzwerk- oder
Safety-Wahrheit. Die Plattform-Metadaten-Allowlist umfasst nur `Host`,
`Content-Type`, `Cookie`, `X-CSRF-Token`, `X-UI-Mutation-Seq`, `Origin`,
`Referer`, `Sec-Fetch-Site` und ausgehend `Set-Cookie`/`Retry-After`.
Ungueltige, doppelte oder ueberlange sicherheitsrelevante Metadaten scheitern
vor der Fachroute; `Forwarded`/`X-Forwarded-*` werden nicht vertraut.

## 4. Authentication, Wireformat und Recovery

Die bestehende Ownerentscheidung gilt fuer R1:

```text
KDF_ALGORITHM=PBKDF2_HMAC_SHA256
KDF_WORK_FACTOR=10000
KDF_WORK_FACTOR_FALLBACK=NONE
```

Webpasswort und vierstellige Service-PIN sind getrennte Credentials mit
getrennten zufaelligen Salts, Verifiern, Epochen und Lockout-Zustaenden.
Der Webpasswortvertrag aus `docs/WEB_UI.md` gilt: 15–64 Unicode-Codepoints,
maximal 256 gueltige UTF-8-Bytes, keine Zeichenklassenpflicht, kein Abschneiden,
Paste/Passwortmanager erlaubt. Passwort, PIN und Session-Geheimnisse werden
nie geloggt, diagnostiziert oder exportiert. Direkter lokaler HTTP-Betrieb
ist keine Vertraulichkeitsgarantie; Flash-/NVS-Verschluesselung bleibt ein
separates Release-Gate. Nur dieselbe ESP-IDF-/PSA-PBKDF2-HMAC-SHA-256-Primitive
mit Work-Factor 10000 darf die alte reale KDF-Primitive-Messung erben.

Neue Auth-Daten verwenden denselben `IStateStore`/Envelope-Backend und die
aktuelle `StorageEpoch`; weder LittleFS noch Konfigurationsdokumente werden
zweite Auth-Stores. Vorgeschlagene **neue** Recordzuordnung auf dieser
Baseline (mit Ownerfreigabe dieses Plans verbindlich, sofern die
repositoryweite Freiheitspruefung auf dem Implementierungs-Baseline-HEAD
PASS ist):

| Key | RecordTypeId | Schema | Inhalt |
|---|---:|---:|---|
| `auth0` | 11 | 1 | gemeinsamer Credential-Record: Passwort/PIN-Algorithmus, Parameter, getrennte Salts/Verifier/Epochen, Lockout, monotone Recordsequenz |
| `authroot0` | 12 | 1 | Provisionierungszustand, Domain-Generation, Epoch, Sequenz; keine Credentials |

```text
TOUCH_CALIBRATION_RECORD_TYPE=10
TOUCH_CALIBRATION_KEYS=tc0,tc1
TOUCH_CALIBRATION_SCHEMA=1
TOUCH_CALIBRATION_CHANGE=NO
TOUCH_RECALIBRATION=NO
OLD_PR167_AUTH_RECORD_ID_10=OBSOLETE_CONFLICT_WITH_CANONICAL_RECORD_TYPE_NAMESPACE
AUTH_RECORD_TYPES=11,12
TOUCH_CALIBRATION_RECORD_TYPE_10=UNCHANGED
```

Der Konflikt betrifft ausschliesslich die historische numerische
RecordTypeId-Zuordnung im nicht gemergten PR #167: Dort war Typ 10 fuer Auth
vorgesehen; Typ 10 gehoert kanonisch und unveraendert der Touchkalibrierung.
Es gibt weder einen funktionalen Konflikt noch Touch-Aenderungs- oder
Rekalibrierungsbedarf. Die aktuelle Code-/Key-Inventur zeigt keine Nutzung
von `auth0`/`authroot0` oder Recordtyp 11/12 auf der Baseline.
`AUTH_RECORD_TYPES=11,12` bleibt nur verbindlich, wenn die repositoryweite
Freiheitspruefung auf dem Implementierungs-Baseline-HEAD erneut PASS ist.
Es gibt keine automatische Migration alter PR-167-Prototyp-Records. Ein
unerwarteter alter Record auf einem Testgeraet ist kein positiver
Auth-Bootstrapnachweis und fuehrt bis zu einem explizit autorisierten
Recoveryweg fail-closed.

Provisionierung beginnt nur mit positiv readback-validiertem
`UNPROVISIONED`-Root in der aktuellen Epoch. Ein fehlender, korrupter,
falsche-Epoch- oder bereits provisionierter Root wird nie als Fabrikzustand
interpretiert. Ein typisierter lokaler Application-/UI-Bootstrap mit
`UiSurface::LocalDisplay` und ausdruecklicher Bestaetigung legt Passwort
und PIN gemeinsam an; es gibt kein Defaultpasswort und keine Factory-PIN.
LAN/Web duerfen nicht Erstschreiber sein. Die Zustands-
folge ist `UNPROVISIONED -> PROVISIONING -> PROVISIONED`; unklarer Commit,
Readback-, CRC-, Schema- oder Epochfehler fuehrt zu
`PROVISIONING_INDETERMINATE`/`RECOVERY_REQUIRED`, sperrt Sessions und
unterbindet stilles Reprovisionieren oder Factory-Fallback.

Fuer bereits bestehende Epochs konsumiert die Auth-Domaene den vorhandenen
`ConfigurationBootstrapRecord` kontrolliert einmal: die geplante
Schema-3-Handoff-Markierung `UNCONSUMED -> IN_PROGRESS -> CONSUMED` bindet
die Initialisierung an Bootstrap-Sequenz und Epoch. Nur eine nachweislich
gueltige Migration der bisherigen Schema-2-Baseline eroeffnet
`UNCONSUMED`; unklarer Handoff bleibt `INDETERMINATE`/Recovery-required.
Diese Erweiterung aendert weder Konfigurationsgraph noch Connectivity- oder
Run-Persistenzwahrheit. Das bestehende Reset-/Epoch-Gate wird konsumiert;
keine neue globale Recoveryplattform entsteht.

Jeder beantwortete fehlgeschlagene Passwort-/PIN-Versuch wird mit Zaehler,
Stufe und Sperre atomar geschrieben, exakt rueckgelesen und validiert **vor**
der Antwort. Webpasswort: 5 Fehler, initial 30 s, exponentiell bis 15 min;
PIN: 3 Fehler, initial 30 s, bis 30 min. Waehrend aktiver Sperre keine KDF-
Arbeit. Unklarer Commit sperrt statt einen Versuch zu verlieren. Erfolgreicher
Credentialwechsel darf nie auf den alten Verifier zurueckfallen und widerruft
die betroffenen Sessions/Servicefreigaben. Lockout und Credential-Epochen
bleiben neustartfest; Session-ID, CSRF und Servicelease bleiben fluechtig.

## 5. HTTP-, Session-, UI- und API-Grenzen

Normale und bewusst anonyme lokale Sessions sind serverseitig und fluechtig,
maximal vier gleichzeitig, 30 min inaktiv / 12 h absolut. Sessionkennung und
sitzungsgebundenes CSRF-Token besitzen je mindestens 128 Bit
kryptografischen Zufall. Cookie: `HttpOnly`, `SameSite=Strict`, `Path=/`, kein
`Domain`; `Secure` nur bei geeignetem TLS-Transport. Neustart, Logout,
Credential-/Moduswechsel, Reset und sicherheitsrelevante Recovery widerrufen
passende Sessions. Bewusst deaktivierter Webpasswortschutz laesst nur die
Passwortpruefung entfallen und verlangt eine dauerhafte sichtbare Warnung;
Service-PIN bleibt Pflicht. Web-Servicelease nutzt die vorhandene Policy:
5 min Inaktivitaet / 15 min absolut und gilt nur fuer die eine Session.

Jede interne Mutation ist methoden-, Content-Type-, Cookie-/CSRF-,
Origin/Referer-/Fetch-Metadata-, Revisions- und Konflikt-geschuetzt.
Login ohne bestehende Session nutzt die passende Login-/Origin-/Lockout-
Grenze. Eine positive `X-UI-Mutation-Seq` wird pro Session atomar reserviert;
ein bounded Fenster von hoechstens acht abgeschlossenen Outcomes und eine
`InFlight`-Mutation geben identischen Retrys exakt den frueheren Outcome,
waehrend Payloadwechsel, alte Sequenzen, konkurrierende Tabs, Luecken und
`uint64`-Overflow ohne zweite Mutation scheitern. Der Browser erhaelt die
naechste autoritative Sequenz beim Session-/Snapshot-Handoff und fuehrt nach
einem Konflikt kein blindes Replay aus. Das ist Transport-Idempotenz, keine
zweite fachliche Command-ID.

Die offiziell dokumentierte lokale API bleibt **read-only** und versioniert:
`GET /api/v1/status`, `/temperatures`, `/alerts`, aus den vorhandenen
secret-freien Application-Projektionen. Fehlende oder untrusted Werte sind
ungueltig/fehlend gekennzeichnet. Interne UI-POST-Routen sind keine externe
Write-API. Bei aktiviertem Passwort gilt Session-Authentisierung, bei
bewusst deaktiviertem Passwort der dokumentierte anonyme lokale Modus.

Die responsive DE/EN/ES-Weboberflaeche konsumiert denselben
Application-Snapshot und die bestehenden Preview-/Commit- und Command-Pfade:
Login/Logout, momentaner Status, Start/Manuell/Stop/Completion,
Programm-Bearbeitung, Meldungen mit Ack/Mute, Einstellungen,
Servicefreigabe und vorhandene System-/Recovery-Informationen. Die
Browser-Sprache ist unabhaengig vom Display; englischer, dann technischer
Fallback. Bounded Polling zeigt Offline/Stale und laedt nach Reconnect den
vollstaendigen aktuellen Snapshot. Es entsteht keine neue Zeitreihe, kein
Chart, keine History und kein Diagnose-/Exportpfad aus Issue #28.

```text
WEB_COMMAND_SOURCE=WebInterface
WEB_SERVICE_LEASE_IS_AUTHORIZATION_ONLY=YES
SERVICE_WEB_WIRE_EXTENSION=NO
COMMAND_SOURCE_SCHEMA_CHANGE=NO
RUN_CHANGE_SOURCE_SCHEMA_CHANGE=NO
CHANGE_ORIGIN_SCHEMA_CHANGE=NO
```

Die sitzungsgebundene Web-Servicefreigabe ist ausschliesslich eine
Autorisierungs-/Lease-Grenze und kein eigener persistenter
Command-Provenienzkanal. R1 verwendet die kanonische Quelle
`WebInterface`; diese Planrevision autorisiert keine Enum-, Codec- oder
Wire-Aenderung. Sollte eine spaetere konkrete Anforderung eine getrennte
persistent gespeicherte Service-Web-Provenienz verlangen, ist das eine neue
materielle Wire-/Persistenzentscheidung mit eigener Planrevision und
Ownerfreigabe.

## 6. Evidence und spaetere Verifikation

| Klasse | Nachweis und gueltige Grenze |
|---|---|
| Weiter verwendbare Ownerentscheidung | PBKDF2-HMAC-SHA-256, 10000 Iterationen, kein Fallback. |
| Bedingt verwendbare Hardware-/Primitive-Evidence | PR-167-KDF-Messung mit derselben PSA-Primitive: bei 10000 Iterationen 3.781637–3.781645 s, kein beobachteter Timer-Late-/WDT-/Resetfehler. Nur nach exaktem Primitive-/Parameterabgleich; kein Beweis fuer neue integrierte Weblast oder Offline-Angriffsschutz. |
| Historische, nicht aktuelle Final-Evidence | Alte PR-167-Native-Suite, Firmware-/Assetgroesse, statischer RAM, Heap, Boot- und Browser-/Ressourcenwerte; alle gehoeren zum alten Head und vor-#156-Stand. |
| Neu zu erheben nach Implementation | gezielte Auth-/Epoch-/Cutpoint-/Lockout-, Session-/CSRF-/Replay-, Codec-/Route-/HTTP-Metadaten-, gemeinsamer Application-Owner-, Ack/Mute-, Safety-/Recovery- und Web-Asset-/Client-Nachweise auf neuem Head. |
| Neu zu erheben vor finaler Freigabe | beide ESP-IDF-Profile, aktueller Ressourcenbericht einschliesslich vier Sessions/Polling, Heap-Minimum/groesster Block, Jitter/WDT, Lizenz-/Lock-Provenienz, reale Browser-/Netzwerk-Client-Evidence soweit owning Gate verfuegbar. Fehlende physische Mittel bleiben `BLOCKED`, nicht PASS. |

Der KDF-Kandidatenvergleich 100..10000 und #31-Display-/Touch-Tests werden
nicht ritualistisch wiederholt. Wenn Primitive, Toolchain, Work-Factor oder
Produktlast die Uebertragbarkeit aendert, wird genau der betroffene Nachweis
neu geplant. Historische Audit-Dateien bleiben unveraendert.

## 7. Umsetzungsschnitte nach gesonderter Ownerfreigabe

1. Baseline/Recordtypen/Port-API erneut verifizieren. Den Auth-First-Consumer
   mit Schema-/Epoch-/Readback-/Recovery- und KDF-Grenzen als kleinen Schnitt
   implementieren; native Codec-, Cutpoint-, Lockout- und Factory-Reset-
   Regressionen. KDF-Ownerentscheidung in den betroffenen kanonischen
   Security-/Adopt-Dokumenten synchronisieren, ohne alte Audits umzuschreiben.
2. Den bestehenden HTTP-Port/ESP-IDF-Adapter nur um die notwendige bounded
   Browser-Metadaten-Allowlist erweitern. Einen einzigen #164/#27-Dispatcher
   verdrahten; Setup-Prioritaet und Fehler-/Header-Verhalten mit nativen
   Konsumententests und realem ESP-IDF-Adapter-Dispatch pruefen.
3. Vorhandene PR-167-Session-, Browserpolicy- und DTO-/Codec-Ideen gegen
   aktuelle Verträge selektiv portieren. ArduinoJson nur nach dem beschlossenen
   Build-/Lizenz-/Ressourcengate pinnen; Session-/CSRF-/Mutation-Sequence-
   Tests einschließlich zwei Tabs, Reload, Retry und Overflow.
4. Die interne Web-Run-Route an `prepare -> confirm -> applyConfirmedPrepared`
   anbinden und die Matrix aus Abschnitt 3 pruefen. Direkt betroffene
   Application-/Touch-Konsumententests sichern denselben Ownerpfad, aber
   erzeugen keine neue #31-Hardwaremessung. Die read-only API und responsive
   Assets nutzen dieselben Application-Projektionen und bounded Daten.
5. Gezielte UI-/API-/Auth-/Recovery-/Ressourcen- und Lizenz-Evidence sammeln.
   `bash scripts/run_pre_ready_gates.sh self-check` auf dem Implementierungs-
   HEAD unter clang-format/tidy 21 ausfuehren; danach fuer unabhaengigen
   Implementation Review anhalten. Vollstaendiger Pre-Ready erst nach
   abgeschlossenem Review, `OPEN_BLOCKERS=0` und separater Owner-Anweisung
   auf dem finalen HEAD; Ready, CI und Merge bleiben Owner-/Workflow-Gates.

Jeder Schnitt ist separat review- und testbar. Materielle Abweichung bei
Schema, Wirewerten, HTTP-/Application-Grenze, Security, Recovery,
Bibliothekswahl oder Akzeptanzkriterien bedeutet STOP, neue Plan-SHA und
erneute Ownerfreigabe. Die neue #27-Implementation wird nicht aus der
PR-167-Commitserie uebernommen.

## 8. Nicht-Ziele und aktuelles Stop-Gate

Keine #31-Display-/Touch-/LVGL-/Kalibrierungsarbeit, kein #28-Chart/History/
Export, keine neue #164-Connectivity-/HTTP-Plattform, keine Aktorfreigabe,
kein OTA, Cloud, OAuth/RBAC, WebSocket/SSE oder spekulatives Framework.
Regelung und Safety bleiben ohne Netzwerk, Web oder Anzeige funktionsfaehig;
unbekannter Zustand bleibt fail-closed.

In **dieser** Runde: nur Plan-/Roadmap-Diff, `git diff --check` und
Baseline-/Quellenkontrolle. Keine Produkt-, Build-, Host-Full-Suite-,
ESP-IDF-, Hardware- oder Browser-Gates. Nach Commit, Push, neuem Draft-PR,
Issue-/PR-Statusabgleich und Handover STOP fuer unabhaengigen Planreview und
Freigabe der exakten neuen Plan-SHA.
