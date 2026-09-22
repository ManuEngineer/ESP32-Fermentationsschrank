# Planrevision – Issue #27: Web-API, Weboberflaeche, Anmeldung und Bedienkonflikte

## Status, Zweck und harte Basis

Dies ist die vollständige, eigenständig reviewfähige Planrevision für Issue
#27 auf dem aktuellen kanonischen `main`. Sie ist ein Delta-Plan zur
Fortsetzung des bereits implementierten PR-167-Stands. Die frühere
Implementation/Fix-Runde bleibt erhalten; diese Reviewrunde ändert keine
Produktlogik. Nach Freigabe dieser neuen Plan-SHA werden ausschließlich die
unten als verbleibendes Delta bezeichneten Lücken weitergeführt.

Die Live-Prüfung wurde am 2026-09-22 in einem frischen Arbeitsbaum gegen
GitHub-`origin/main` durchgeführt. Diese Revision aktualisiert PR #167 nach
dem Merge von PR #169 und konsumiert dessen kanonische Application-/Runtime-
Evidence-Grenzen. Die frühere Issue-Beschreibung enthielt noch die historische
ESP-IDF-6.0.2-Bezeichnung. Dieser Plan verwendet ausschließlich den nach #159,
#165 und #169 kanonischen ESP-IDF-6.1-Stand.

```text
ISSUE=27
PR=167
PR_STATUS=DRAFT
BASE_BRANCH=main
BASE_SHA=b8d963e8d830b95b160dfb7e5cc9c2d033ad53e9
CURRENT_HEAD=b0c52f44a5d6aa8dcbab473b9d9b6407f4442fcc
PLAN_REVISION=CONTINUATION_DELTA_ON_EXISTING_PR167_AFTER_PR169
PLAN_STATUS=DRAFT_OWNER_APPROVAL_REQUIRED
PLAN_SHA=EXACT_COMMIT_RECORDED_AFTER_THIS_REVISION
IMPLEMENTATION_AUTHORIZATION=NO
IMPLEMENTATION_STATUS=PAUSED_PLAN_REVALIDATION
EXISTING_IMPLEMENTATION_PRESENT=YES
EXISTING_WEB_UI=LOGIN_OVERVIEW_NETWORK_SERVICE_PASSWORD_ALERTS_LOGOUT_BOUNDED_POLLING_BASE
REMAINING_WEB_UI_DELTA=OWNER_APPROVAL_REQUIRED
REMAINING_IMPLEMENTATION_DELTA=OWNER_APPROVAL_REQUIRED
ESP_IDF_VERSION=v6.1.0
ESP_IDF_COMMIT=fff9895c82d744c7237be8847347bdd1b07c6643
TARGET=ESP32-WROOM-32E
FLASH=4_MB
PSRAM=NOT_REQUIRED
CXX_STANDARD=GNU++17
PR165_STATUS=MERGED
PR165_MERGE_SHA=1f1755e5e706fb668472920545b5302fcef1df16
PR169_STATUS=MERGED
PR169_MERGE_SHA=b8d963e8d830b95b160dfb7e5cc9c2d033ad53e9
PREVIOUS_APPROVED_PLAN_SHA=da30f0526b6ffd76e51297d00291afe5808a4815
APPLICATION_RUNTIME_EVIDENCE_PREDECESSOR_ISSUE=168
APPLICATION_RUNTIME_EVIDENCE_PREDECESSOR_PR=169
APPLICATION_RUNTIME_EVIDENCE_MAIN=b8d963e8d830b95b160dfb7e5cc9c2d033ad53e9
APPLICATION_RUNTIME_EVIDENCE_STATUS=AVAILABLE
PR142_ISSUE25_STATUS=MERGED
PR143_ISSUE26_STATUS=MERGED
ISSUE164_HTTP_FOUNDATION=AVAILABLE_ON_BASE
ISSUE31=OUTSIDE_SCOPE
ACTUATOR_RELEASE=NO
HARDWARE_IMPLEMENTATION=NOT_STARTED
```

Die exakte Plan-SHA ist die SHA des Commits, der diese Datei und die
minimalen Roadmap-Synchronisierungen enthält. Die Freigabe dieser Plan-SHA
autorisiert noch keine Umsetzung, keinen Ready-Wechsel, keinen Merge und
keine Hardware- oder Aktorfreigabe. Erst nach Ownerfreigabe der exakten SHA
beginnt die Umsetzung in einem separaten, ausdrücklich begrenzten Builder-
Schritt.

## 1. Verifizierte Baseline und vorhandene Bausteine

### 1.1 Kanonischer Stand

- PR #165 / Issue #164 ist auf `main` gemergt. Der Merge-Commit ist zugleich
  die angegebene Baseline-SHA.
- ESP-IDF ist im Produktionsvertrag auf `v6.1` mit Commit
  `fff9895c82d744c7237be8847347bdd1b07c6643` fixiert. Beide Profile planen
  mit 4 MB Flash und ohne PSRAM-Abhängigkeit; der native Hostpfad bleibt
  PlatformIO.
- PR #142 / Issue #25 liefert die rendererunabhängigen UI-Modelle,
  Revisionen, Commands und strukturierten Ergebnisse.
- PR #143 / Issue #26 liefert die lokale Shell-/Anwendungsgrenze. Der Webpfad
  wird kein zweiter UI- oder Fachkern.
- PR #169 / Issue #168 ist auf `main` gemergt und liefert die kanonische
  Application-owned Runtime-Evidence: `FermentationApplication` nimmt keine
  Sensor-, Planner- oder Safety-Evidence von UI/Web entgegen, publiziert die
  owning Evidence an der Application-Grenze und revalidiert vorbereitete
  Requests bei `confirmPrepared()` fail-closed.
- Issue #31 ist nicht Teil dieser Arbeit. Weder Display-/Touchhardware noch
  LVGL, SPI, Controller, Kalibrierung, Fonts oder ein Renderer werden für #27
  vorausgesetzt oder implementiert.
- Die historische #89-Provenienz in `docs/ROADMAP.md` bleibt unverändert und
  verwendet exakt `PREVIOUS_APPROVED_PLAN_SHA=524dbaaf34f2350f407872640124a356155e4a1d`;
  sie wird nicht mit der aktuellen #27-Planrevision vermischt oder rückwirkend
  umetikettiert.
- Issue #164 bleibt der Besitzer der WLAN-/Setup-Wahrheit und kann für die
  vollständige reale WLAN-Evidence weiterhin ein eigenes Gate haben. Das ist
  keine Rechtfertigung für einen zweiten Connectivity- oder HTTP-Vertrag in
  #27.

### 1.2 Aktuelle Codeinventur

Die frühere, bereits implementierte PR-167-Runde und die Mergeauflösung auf
PR-169 haben folgende Bestandteile nachweisbar in diesem Branch hinterlassen.
Sie sind Bestand, keine neue Umsetzungsliste:

| Bestand | Verifiziert vorhanden | Verwendung in #27 |
|---|---|---|
| HTTP-Port und bounded Metadaten | `device_platform::IHttpServerLifecycle`, `IHttpRouteSink`, `HttpRequest`, `HttpResponse`, `http_server_lifecycle.*` | einzige Request-/Response-Grenze; keine zweite Serverinstanz |
| ESP-IDF-Adapter | `EspIdfHttpServerLifecycle` mit `esp_http_server`, PIMPL und begrenztem Body | technische Start-/Stop-/Übersetzung bleibt Adapter-/#164-Eigentum |
| #164-Routen | `NetworkSetupRoutes` für `/`, `/api/network/status`, `/api/network/scan` und Kandidat/Commit | Setup-Routen bleiben aktiv und werden vor normalen Webrouten delegiert |
| Netzwerk | `FermentationApplication`, `NetworkConfigurationService`, `ConnectivityCredentialStore`, `NetworkAccessPointInfo` | #27 liest Status; Modus, Candidate/Commit und Credentials werden nicht dupliziert |
| UI-Snapshot | `FermentationUiSnapshot`, `FermentationUiProjector`, `FermentationUiExpectedRevisions`, secret-freies `FermentationNetworkModeView` | Web und Touch erhalten dieselbe Projektion; fehlende Werte bleiben ungültig/unverfügbar |
| UI-Commands | `FermentationUiCommand`, `FermentationUiCommandBridge`, Konfigurations-/Netzwerkcommands und erwartete Revisionen | Webadapter liefert nur typisierte Werte und Revisionen; die Anwendung entscheidet |
| Application-Runtime-Evidence | `FermentationApplication::publishOwningRuntimeEvidence()`, `uiSnapshot()`, `prepare*()` und `confirmPrepared()` | #168/#169 besitzen Runtime-/Sensor-/Safety-Provenienz und Confirmation-Revalidierung; Web erzeugt oder übergibt keine Evidence und rekonstruiert keine Safetyentscheidung |
| Authentication und Bootstrap | `authentication_records.*`, `configuration_bootstrap.*`, `configuration_bootstrap_store.*`, Auth-Root/`auth0`-Store und Schema-3-Handoff | bestehende Auth-/Epoch-/Readback-/Recovery-Semantik behalten; keine zweite Persistenzwahrheit |
| Sessions und Browserpolicy | `web_session.*`, `web_browser_policy.*`, bounded CSRF/Cookie-/Mutation-Sequence-Handling | bestehende flüchtige Session- und Replay-Verträge behalten; kein neues Sessionmodell |
| Web/API/Assets | `web_application_routes.*`, `web_api_codec.*`, `web_assets.hpp` sowie `/api/v1/*` und interne Auth-/Network-Routen | bestehende compile-time Weboberfläche, Read-only-API und ein Dispatcher bleiben erhalten |
| Service-Policy | `ServiceSessionPolicy`, `ServiceSessionLease`, `fermentationWebServicePolicy()` | 5 Minuten Inaktivität / 15 Minuten absolut, getrennt von Touch |
| Persistenz | `IStateStore`, `StorageEnvelope`, `StorageEpoch`, Readback und `CommitOutcomeUnknown` | Authentifizierungsdomäne bleibt erster realer Auth-Consumer auf diesem Backend |
| JSON-Abhängigkeit | ArduinoJson `7.4.3`, Registry-/Tag-Commit `77771d3c07668e01d8f52acb03910c1110bb373f`, bounded Codec in `web_api_codec.*` | integriert und gelockt; Resource-Evidence bleibt offen |
| Tests | `test_authentication_records`, `test_http_server_lifecycle`, `test_web_api_codec`, `test_web_browser_policy`, `test_web_session` und bestehende native Konsumententests | bestehende Nachweise behalten; verbleibende Ownerpfad-/Ressourcenlücken gezielt ergänzen |

Damit ist `EXISTING_IMPLEMENTATION_PRESENT=YES` repository-first belegt:
Auth-Domäne/Store, Schema-3-First-Consumer-Handoff, Sessions,
Browserpolicy, Web/API-Codecs, compile-time Assets, HTTP-Metadatenübersetzung,
Composition und die zugehörigen nativen Tests existieren bereits. Sie werden
nicht erneut als neue Module geplant. Das verbleibende Delta ist ausschließlich
die nachstehend präzisierte Application-Anbindung und die noch offenen
Nachweise.

#### 1.2.1 Bounded HTTP-Metadatenvertrag vor der Umsetzung

Der vorhandene plattformneutrale HTTP-Vertrag wird vor jeder #27-Route um
eine kleine typisierte Metadatenstruktur erweitert. Es gibt keine allgemeine
Header-Map und kein generisches Middleware-Framework. Die Request-Metadaten
tragen ausschließlich diese einzelnen Felder: `Host`, `Content-Type`,
`Cookie`, `X-CSRF-Token`, `X-UI-Mutation-Seq`, `Origin`, `Referer` und
`Sec-Fetch-Site`. Die Response-Metadaten tragen ausschließlich `Set-Cookie`
und `Retry-After`; Statuscode, Response-Content-Type und Body bleiben Teil des
bestehenden `HttpResponse`.

Die Grenzen sind fest und werden bereits im ESP-IDF-Adapter geprüft: maximal
acht bekannte Request-Metadatenfelder, höchstens ein Vorkommen je Feld,
insgesamt 2048 Bytes und je Feld maximal `Host=256`, `Content-Type=64`,
`Cookie=512`, `X-CSRF-Token=64`, `X-UI-Mutation-Seq=20`, `Origin=256`,
`Referer=512` und `Sec-Fetch-Site=32` Bytes. `Set-Cookie` ist auf 512 und
`Retry-After` auf acht ASCII-Ziffern begrenzt. NUL, CR, LF, Steuerzeichen,
ungültige Kodierung, Duplikate und Überschreitung werden vor dem Route-Sink
fail-closed mit einer passenden 400-/431-Antwort beendet. Es gibt keinen
Fallback auf ungeprüfte Rohheader.

Der Adapter übersetzt mindestens die bereits verwendeten Statuscodes 200,
400, 404, 409, 422 und 503 sowie zusätzlich 204, 401, 403, 405, 413, 415,
429 und 431 in gültige ESP-IDF-HTTP-Antworten. `401` ist für fehlende oder
ungültige Authentisierung, `403` für verweigerte CSRF-/Origin-/Fetch-/Policy-
Prüfung, `405` für falsche Methode, `413` für zu große Bodies, `415` für
falschen Content-Type, `429` für aktiven Lockout und `431` für ungültige oder
zu große Security-Metadaten reserviert; fachlicher Konflikt bleibt `409`,
persistente Nichtverfügbarkeit `503`. Auth-/Policyfehler dürfen nicht als
pauschaler `500` erscheinen.

CSRF wird nach erfolgreicher Sessionbildung über eine bounded JSON-Antwort
ohne weitere Secrets mit dem sessiongebundenen 16-Byte-Token in 32
kleingeschriebenen
Hexzeichen an den same-origin Browser übergeben; alternativ darf derselbe
Handoff in der erfolgreichen Loginantwort erfolgen. Der Token steht weder in
URL, Cookie, Log, Diagnose, Export noch im persistenten Auth-Record. Die
Session-ID bleibt ausschließlich im `HttpOnly; SameSite=Strict; Path=/`-
Cookie; das `Set-Cookie`-Feld enthält keine Session- oder CSRF-Daten außerhalb
des vorgesehenen Cookiewerts und nie ein `Domain`-Attribut. Ein späterer
TLS-Transport darf zusätzlich `Secure` setzen.

Frontend-Requests verwenden relative, dokumentbasierte URLs und keine
festen Scheme-/Host-/Root-Annahmen, damit ein explizit konfigurierter lokaler
Reverse-Proxy-Pfad möglich bleibt. `Forwarded` und `X-Forwarded-*` gehören
nicht zur Allowlist und werden standardmäßig nicht vertraut; eine spätere
Proxy-Unterstützung ist nur mit eigener expliziter Konfiguration zulässig.

### 1.3 Verbindliche Quellen

Die Umsetzung verwendet als fachliche Quellen, ohne ihre Verträge zu kopieren:

- `docs/WEB_UI.md`: responsive Oberflächen, Sprache, Session-/CSRF-/Service-
  Regeln, Live-Verhalten, Konflikte und aktueller Laufchart;
- `docs/NETWORK.md`: #164-Transport, AP_ONLY/HOME_WIFI, Setup-Abgrenzung,
  direkte HTTP-/mDNS-Grenzen und kein zweiter Credential-Speicher;
- `docs/NETWORK_DIAGNOSTICS_INTEGRATION.md`: read-only API und
  Secret-Redaction;
- `docs/SETTINGS_AND_STORAGE.md`, `docs/CONFIGURATION_PERSISTENCE.md` und
  `docs/BACKUP_SECURITY_RETENTION.md`: Active-/Fallback-/Epoch-/Readback-
  Semantik, erster Auth-Consumer und Ausschluss von Secrets aus Exporten;
- `docs/DECISIONS.md`, insbesondere ADR-013, ADR-017, ADR-018 und ADR-019;
- `docs/RESOURCE_BUDGET_AND_MAINTENANCE.md`,
  `docs/ESP_IDF_UPGRADE_CONTRACT.md`, `docs/ADOPT_OR_BUILD.md` und die
  Release-1-Audits;
- die lokalen `AGENTS.md` für `fermentation_app`, `device_platform`,
  `device_platform_esp_idf` und `device_platform_test_support`.

Historische Auditzeilen mit ESP-IDF 6.0.2 werden nicht als aktuelle
Abhängigkeit oder Evidence übernommen. Im Implementierungs-PR wird jede
betroffene technische Quelle gegen den fixierten v6.1-Lock geprüft, ohne
historische Nachweise rückwirkend umzubenennen.

## 2. Ziel und Ownership

### 2.1 Ziel

Issue #27 liefert eine lokale, responsive R1-Weboberfläche mit einer
dokumentierten, ausschließlich lesenden API, serverseitigen flüchtigen
Sessions, getrennter Service-PIN-Freigabe, Browser-Request-Schutz und
konfliktsicheren internen Schreibwegen. Die Weboberfläche projiziert den
kanonischen Anwendungszustand und sendet typisierte UI-Intents. Sie entscheidet
weder über Safety noch über Persistenz, Credentials, Netzwerklebenszyklus oder
Aktorfreigabe.

### 2.2 Ownership-Matrix

| Verantwortung | Eigentümer | Grenze zu #27 |
|---|---|---|
| einziger `esp_http_server`-Lifecycle, technisches Start/Stop/Bind, Request-/Response-Übersetzung | #164 / `device_platform_esp_idf` | #27 registriert Delegation/Routen an diesen Lifecycle und startet keinen zweiten Server |
| AP_ONLY, HOME_WIFI, APSTA, Setup-AP, Scan, Kandidatentest, Credential-Commit, Connectivity-Recovery, mDNS und direkte IP | #164 / `fermentation_app` | #27 zeigt den von #164 gelieferten Status und verlinkt Setup; keine zweite WLAN-/Credential-Wahrheit |
| `UserConfiguration.networkMode`, `ConnectivityCredential` `cc0`/RecordType 9, StorageEpoch | #164 und bestehende Persistenzverträge | #27 behandelt diese Daten nur über vorhandene Application-/UI-Grenzen |
| rendererunabhängige Snapshots, typed Commands, expected revisions, results, Permission-/UI-Semantik | #25/#26 und bestehende `fermentation_app`-Contracts | Web ist Consumer; ein notwendiger minimaler Source-Enum-Ausbau muss als gemeinsamer Contract-Slice geprüft werden, nicht als Web-Schattenvertrag |
| Runtime-/Sensor-/Safety-Evidence und Confirmation-Revalidierung | #168 / PR #169, `FermentationApplication` auf kanonischem `main` | #27 konsumiert `uiSnapshot()` sowie die bestehenden `prepare*()`-/`confirmPrepared()`-Pfade; keine Web-/Renderer-Evidence und keine parallele Runtime-Wahrheit |
| normaler Webtransportadapter, Webnavigation und responsive WebUI | #27 | keine Touch-/Display-Rendererlogik |
| Webpasswort, Authentication-Persistenz, Verifier/KDF-Vertrag, Websessions und Web-Lockout | #27 als erster Auth-Consumer | eine typisierte Authentication-Domäne im bestehenden `IStateStore`; kein zweiter physischer Store |
| CSRF, Origin/Referer/Fetch-Metadata, Cookie- und Methodengrenze | #27 | vor fachlicher Mutation; nicht als ESP-IDF-Serverfeature delegieren |
| Web-Servicefreigabe über die bestehende vierstellige Service-PIN | gemeinsame Application-/Permission-Grenze, konsumiert durch #27 | identischer Service-PIN-Verifier für lokale und Web-Servicepfade; keine separate Web-PIN |
| normale Web-Seiten: Übersicht, Programme, Lauf, manuell, Meldungen, Einstellungen, Diagnose, Service, System | #27 | Daten und Aktionen kommen aus den #25/#26-Projektionen und Commands |
| dokumentierte `/api/v1/`-Lese-API | #27 | stabil read-only; interne UI-Schreibwege sind keine öffentliche Write-API |
| Display-/Touch-Treiber, LVGL, Fonts, Controller, Kalibrierung und physische Evidence | #31 | vollständig ausgeschlossen |

### 2.3 Einziger Datenfluss

```text
esp_http_server / #164 lifecycle
        -> platform-neutrale HttpRequest/HttpResponse-Grenze
        -> gemeinsamer Route-Dispatcher
           -> #164 SetupRoutes, wenn SetupFlow aktiv
           -> #27 WebAuth/WebUI/API-Routes
        -> Session-/CSRF-/Origin-Prüfung
        -> DTO/Codec mit festen Grenzen
        -> FermentationApplication::uiSnapshot()
        -> typisierte `FermentationApplication::prepare*()`-/`prepareEnvelope()`-
           Intents mit erwarteten Revisionen
        -> `FermentationApplication::confirmPrepared()`
        -> bestehende `FermentationUiCommandBridge` / owning Services
        -> fachliche Validierung, Revision, Persistenz, Safety, Publish
```

Die Route-Reihenfolge verhindert, dass die normale WebUI den Setup-Flow
übernimmt oder #164-Routen dupliziert. Die normale WebUI wird erst ausgeliefert,
wenn kein aktiver #164-Setup-Flow die angeforderte Route besitzt. Ein
Netzwerkfehler, HTTP-Fehler oder Web-Heapfehler darf `application.update()`,
Regelung, Persistenz-Recovery oder Safety nicht beenden.

Web-/HTTP-Code darf keine `FermentationApplicationOwningEvidence`, keine
`safetyAllowsStart`-/`safetyAllowsCooling`-/`safetyAllowsChange`-Werte, keine
generische `FaultResetEvaluation`/`FaultResetRequest`-Wahrheit und keine
Sensor-/Planner-/Recovery-Snapshots in Application-Commands einspeisen. Die
Anwendung liest ihre aktuelle owning Evidence ausschließlich von der
Application-/Orchestrator-Grenze. `ResetFault` bleibt im aktuellen
Compositionpfad `Unavailable`, solange kein kanonischer produktiver
Planner-Owner aus dem vorgesehenen Downstream-Scope gebunden ist.

## 3. Authentisierung, Sessions und Berechtigungen

### 3.0 Erstprovisionierung und unprovisionierter Zustand

`AUTH_BOOTSTRAP_UNPROVISIONED` darf nur aus positiver, kanonischer Evidenz
abgeleitet werden: Der bestehende Storage-Epoch-Initialisierungs-/Factory-
Resetpfad hat für die aktuelle `StorageEpoch` einen gültigen, erfolgreich
readback-validierten `AuthProvisioningRoot` mit Zustand `UNPROVISIONED`
angelegt. Das bloße Fehlen eines Authentication-Records ist niemals diese
Evidenz. Ein beschädigter, inkompatibler, unbestimmter oder unerwartet
fehlender Root-/Credentialzustand führt stattdessen fail-closed zu
`AUTH_RECOVERY_REQUIRED`/Nichtverfügbarkeit.

Im Zustand `AUTH_BOOTSTRAP_UNPROVISIONED` gibt es keine normale Websession,
keinen anonymen Normalbetrieb und keinen LAN-Erstschreiber. Normale Web-/API-
Routen liefern nur eine feste, secret-freie `503 AUTH_NOT_PROVISIONED`-
Antwort bzw. eine statische lokale „Provisionierung erforderlich“-Seite ohne
mutierenden Webpfad. Bei `AUTH_RECOVERY_REQUIRED` bleibt auch der lokale
Bootstrap-Command gesperrt; es gibt keine automatische Reprovisionierung,
keinen stillen Factory-Fallback und keine Umdeutung von `NotFound` nach einem
unklaren Write als frischen Zustand.

Die erste Einrichtung von Webpasswort und Service-PIN erfolgt gemeinsam über
einen typisierten Bootstrap-Command an der bestehenden lokalen,
rendererunabhängigen Application-/UI-Grenze. Er akzeptiert ausschließlich
`UiSurface::LocalDisplay` mit expliziter lokaler Bestätigung; `WebInterface`
und `WebService` werden vom Application-Eigentümer abgewiesen. Damit kann der
Pfad ohne #31 über native Commands, einen deterministischen Store und
Testdoubles implementiert und getestet werden, ohne einem beliebigen
LAN-Absender Erstvertrauen zu geben. Es gibt weder Factory-PIN noch
Defaultpasswort.

Webpasswort und Service-PIN werden gemeinsam in einem atomaren
Authentication-Credential-Record angelegt. Der bestehende Factory-Resetpfad
erhöht die `StorageEpoch`, legt im selben `IStateStore`-Backend einen neuen
`AuthProvisioningRoot` mit `UNPROVISIONED` an und validiert dessen Readback;
erst diese positive neue-Epoch-Evidence öffnet den lokalen Bootstrap. Ein
fehlender Root, ein fehlender erwarteter Credential-Record nach
`PROVISIONED`, ein Parse-/CRC-/Schema-/Epoch-/Readback-/Commitfehler oder ein
`PROVISIONING_INDETERMINATE`-Zustand bleibt `AUTH_RECOVERY_REQUIRED`. Kein
alter Epoch-Record und kein Fallback-Credential wird wieder aktiviert. Erst
nach erfolgreichem Credential- und Root-Readback-Commit steht der normale
Webpfad zur Verfügung.

Für eine bereits gültige, vor #27 angelegte `StorageEpoch` gibt es genau einen
First-Consumer-Handoff. Der bestehende, historisch kanonische
`ConfigurationBootstrapRecord` wird über seinen vorhandenen
`ConfigurationBootstrapStore` und die bestehende Mutation-/Recovery-Lease auf
`SchemaVersion=3` fortgeschrieben. Die Schema-3-Erweiterung trägt ausschließlich
den versionierten Marker
`AuthDomainHandoff=UNCONSUMED|IN_PROGRESS|CONSUMED|INDETERMINATE` und bindet
ihn an die bestehende Bootstrap-Sequenz und `StorageEpoch`.

Nur die gültige Schema-2-zu-Schema-3-Migration auf der unveränderten
Pre-#27-Baseline liefert die positive Evidenz `UNCONSUMED`, dass diese
Authentication-Domäne in der aktuellen Epoch noch nie initialisiert wurde.
`IN_PROGRESS` wird vor dem ersten `authroot0`-Write readback-validiert;
danach wird `authroot0=UNPROVISIONED` für dieselbe Epoch geschrieben und
readback-validiert, erst anschließend wird der Handoff-Marker als
`CONSUMED` persistiert und validiert. Jeder Write-/Readback-/Commitfehler
führt zu `INDETERMINATE` bzw. `AUTH_RECOVERY_REQUIRED`. Ein `CONSUMED`-
Marker darf nie wieder zu `UNCONSUMED` werden. Dadurch ist ein späteres
fehlendes oder korruptes `authroot0` kein erneuter First-Consumer-Fall.

Diese einmalige Migration verändert weder bestehende Konfiguration,
Connectivity-Credentials, Run-Persistenz noch die `StorageEpoch`; sie nutzt
nur den bestehenden Bootstrap-/Recoveryvertrag. Ein stiller Factory Reset,
eine Ableitung allein aus `authroot0 == NotFound` oder eine zweite globale
Recoveryplattform ist unzulässig. Auf neu erzeugten Epochs wird derselbe
versionierte Handoff im bestehenden Epoch-Initialisierungspfad als
`UNCONSUMED` angelegt.

### 3.1 Normales Webpasswort

- Ein gemeinsames normales Webpasswort, kein Konto-, Benutzer-, Rollen- oder
  Multiuser-Modell.
- Passwort ist bei Eingabe verdeckt, wird nie im Klartext gespeichert,
  geloggt, exportiert, diagnostiziert oder per API ausgegeben.
- Der Plan verwendet einen gesalzenen, einseitigen Verifier über dem
  vorhandenen ESP-IDF-6.1/mbedTLS-Pfad. Eine einzelne schnelle SHA-256-Prüfung,
  ein im Firmwaretext hinterlegtes Passwort oder ein Pepper im selben Flash
  sind unzulässig.
- Der technische KDF-Parameter wird als Algorithmus-/Parameter-ID und
  Work-Factor im typisierten Record gespeichert. PBKDF2-HMAC-SHA-256 ist der
  erste zu messende Kandidat. Die endgültige Iterationszahl wird nur nach
  reproduzierbarer v6.1-Messung von KDF-Laufzeit, Stack, Heap, Jitter,
  Watchdog und Fehlversuchen festgelegt; bei fehlender sicherer Messbasis
  bleibt die Auth-Integration blockiert. Es gibt keinen unsicheren KDF-
  Fallback.
- Der Initialwert ist standardmäßig passwortgeschützt und muss bewusst
  eingerichtet werden. Deaktivierung ist eine geschützte, warnende und
  bestätigungspflichtige Mutation; sie ist kein anonymer Dauerzugang.
- Die Ownerentscheidung vom 2026-09-21 ist kanonisch in
  `docs/WEB_UI.md` festgehalten und gilt für R1:
  ```text
  WEB_PASSWORD_MIN_LENGTH=15_CODEPOINTS
  WEB_PASSWORD_MAX_LENGTH=64_CODEPOINTS
  WEB_PASSWORD_MAX_UTF8_BYTES=256
  PASSWORD_COMPOSITION_RULES=NO
  PASSWORD_PASTE_ALLOWED=YES
  PASSWORD_MANAGER_ALLOWED=YES
  PASSWORD_TRUNCATION=NO
  ```
  Die Länge wird in Unicode-Codepoints über einer vollständig gültigen UTF-8-
  Eingabe bewertet; zugleich gilt die 256-Byte-Obergrenze. Leerzeichen und
  Unicode-Zeichen sind grundsätzlich erlaubt, leere Eingaben nicht. Es gibt
  keine Zeichenklassenpflicht und kein stilles Abschneiden. Die Zahlen
  orientieren sich an NIST SP 800-63B-4, sind aber keine Behauptung
  vollständiger NIST-Konformität; direkter lokaler HTTP-Betrieb bleibt die
  dokumentierte Transportgrenze.

### 3.2 Passwortschutz bewusst deaktiviert

Bei bewusst deaktiviertem Webpasswort:

- entfällt nur die Passwortprüfung;
- der Server erstellt weiterhin eine begrenzte, flüchtige anonyme lokale
  Session mit mindestens 128 Bit kryptografisch zufälliger Kennung und
  sitzungsgebundenem CSRF-Token;
- es gibt keine Identität, keine Rolle und keinen Servicezugriff ohne
  separate Service-PIN;
- die UI zeigt dauerhaft eine sichtbare Warnung;
- Cookie-, CSRF-, Origin-/Referer-/Fetch-Metadata-, Methoden-, Content-Type-,
  Revisions-, Konflikt-, Idempotenz-, Fach- und Safety-Gates bleiben aktiv;
- Neustart, Passwortaktivierung, Moduswechsel, Factory Reset und der jeweils
  definierte Credentialwechsel widerrufen die anonymen Sessions.

### 3.3 Websessions

- Sessionzustand bleibt serverseitig und flüchtig. Es werden keine
  persistenten Session-, Login-, Refresh-, Remember-me- oder
  Browsergeräte-Tokens erzeugt.
- Session-ID enthält mindestens 128 Bit kryptografischen Zufall, steht nie in
  URL, HTML, Log, Diagnose, Export oder JSON-Antwort und wird nicht aus MAC,
  SSID, Zeit oder Passwort abgeleitet.
- Cookie: `HttpOnly`, `SameSite=Strict`, `Path=/`, kein `Domain`; `Secure` ist
  bei direktem lokalem HTTP nicht erzwingbar und wird nur bei einem späteren
  TLS-Transport gesetzt. Direkter HTTP-Betrieb behauptet keine TLS-Sicherheit.
- Normale Websession: 30 Minuten Inaktivität, 12 Stunden absolute Dauer.
  Gültige relevante Browseraktivität erneuert nur das Inaktivitätslimit, nie
  das absolute Limit. Neustart widerruft alle Sessions.
- Die erste Implementierung reserviert eine feste, gemessene Tabelle für
  mindestens vier parallele lokale Browser-/Tab-Sessions. Bei voller Tabelle
  wird fail-closed abgelehnt; aktive Sessions werden nicht still verdrängt.
  Die Obergrenze wird vor Produktionsfreigabe mit Heap-/Fragmentierungs- und
  Regelzyklusmessungen belegt.
- Logout, Passwortwechsel, Passwortschutz-Moduswechsel, expliziter
  Netzwerkmoduswechsel, Factory Reset und sicherheitsrelevante Recovery
  widerrufen mindestens die betroffenen und bei Modus-/Credentialwechsel alle
  normalen Websessions. Die Web-Servicefreigabe verfällt dabei mit.

### 3.4 Service-PIN und Web-Servicefreigabe

- Die vierstellige Service-PIN bleibt vom normalen Webpasswort getrennt.
- #27 legt keine eigene Web-PIN an. Der bestehende Service-PIN-Vertrag wird
  über eine gemeinsame Application-/Permission-Grenze konsumiert; falls der
  aktuelle Stand den PIN-Verifier noch nicht besitzt, wird #27 dessen erster
  produktiver Authentication-Consumer, ohne eine lokale/Web-Kopie zu bilden.
- Erfolgreiche Web-Freigabe gilt nur für die konkrete normale oder anonyme
  Websession: 5 Minuten Inaktivität, 15 Minuten absolut. Sie ist getrennt von
  der lokalen Touch-Freigabe und vom normalen Websession-Timer.
- Jede kritische Serviceaktion braucht zusätzlich ihre eigene ausdrückliche
  Bestätigung. Service-PIN hebt niemals Safety, Hardware- oder
  Aktorzustände auf.
- PIN-Fehlversuche werden global pro PIN-Credential geführt: drei Fehler,
  anschließend 30 Sekunden Sperre; weitere Sperren verdoppeln bis maximal
  30 Minuten. Keine Umgehung über Session, Browser, IP, Tab oder Neustart.

### 3.5 Lockout und Credentialwechsel

Für Webpasswort und Service-PIN wird jeweils derselbe deterministische
Persistenzmechanismus mit eigenen Feldern verwendet:

- global pro Credential, nicht pro Browser oder IP;
- Vor-Sperr-Fehlversuchszähler, Sperrstufe, aktiver Sperrstatus,
  verbleibende Sperrdauer zum letzten sicheren Persistenzpunkt,
  Credential-Epoche/-Generation und Integritätsdaten werden atomar
  neustartfest geschrieben;
- Passwort: fünf falsche Prüfungen, 30 Sekunden, exponentiell bis 15 Minuten;
- Service-PIN: drei falsche Prüfungen, 30 Sekunden, exponentiell bis 30 Minuten;
- erfolgreiche Authentisierung setzt Zähler und Sperrstufe atomar zurück;
- ein Persistenz-/Readback-/Commitfehler erlaubt niemals eine Authentisierung;
  ein unklarer Commit bleibt recovery-required und wird sichtbar als
  Authentifizierungsdienst nicht verfügbar behandelt;
- wenn beim Neustart keine verlässliche absolute Zeit vorliegt, wird die
  zuletzt persistierte volle Sperrdauer konservativ erneut abgewartet, nie
  verkürzt;
- nach erfolgreichem Credentialwechsel ist die neue Credential-Epoche
  kanonisch. Configuration-Fallback oder Recovery darf niemals auf den alten,
  erfolgreich ersetzten Verifier zurückfallen;
- bei Passwortwechsel werden alle Sessions widerrufen; ein PIN-Wechsel
  widerruft mindestens alle Web-Servicefreigaben und erfordert erneute
  Authentisierung.

Die Prüf- und Schreibreihenfolge ist für jeden falschen Versuch verbindlich:

1. aktiven Lockout aus dem kanonischen Zustand prüfen; während eines aktiven
   Lockouts gibt es keine KDF-Arbeit und keinen Persistenzschreibvorgang, nur
   die aus dem kanonischen Zustand berechnete Restdauer und `429`;
2. andernfalls Credential prüfen und bei einem Fehlversuch den neuen Zähler,
   die Sperrstufe und den Lockoutzustand berechnen;
3. diesen Zustand atomar persistieren, exakt zurücklesen und vollständig
   validieren;
4. erst danach den Fehler abschließen und die Antwort senden.

Ein erfolgreicher Versuch setzt Zähler/Sperrstufe nach demselben
Write--Readback--Validate-Muster zurück und erzeugt erst danach eine Session
oder Servicefreigabe. `CommitOutcomeUnknown` oder ein Readbackfehler führt
statt einer scheinbar beantworteten Fehlanmeldung zu
`RecoveryRequired`/`503`, widerruft Berechtigungen und darf den fehlenden
Fehlversuch nach einem unmittelbaren Neustart nicht verlieren.

## 4. Authentication-Persistenz als erster realer Consumer

### 4.1 Speichergrenze

Die Authentication-Domäne nutzt das vorhandene `IStateStore`/Envelope-
Backend. Es gibt weder einen zweiten physischen Store, eine zweite NVS-
Partition, LittleFS nur für Auth noch eine Auth-Wahrheit in
`UserConfiguration`/`ServiceConfiguration`.

Der Implementierungsschnitt führt einen typisierten Credential-Record und
einen kleinen typisierten Provisionierungs-Root ein. Beide liegen im selben
bestehenden `IStateStore`/Envelope-Backend; der Root enthält keine Credentials
und ist keine normale Konfigurationswahrheit:

```text
StateStoreKey=auth0
RecordTypeId=10
SchemaVersion=1
StorageEpoch=current active epoch
```

```text
StateStoreKey=authroot0
RecordTypeId=11
SchemaVersion=1
StorageEpoch=current active epoch
ProvisioningState=UNPROVISIONED|PROVISIONING|PROVISIONING_INDETERMINATE|PROVISIONED|RECOVERY_REQUIRED
AuthDomainGeneration=1
```

Die Baseline-Prüfung auf `main@1f1755e5e706fb668472920545b5302fcef1df16`
ergibt für `auth0`/`RecordTypeId=10` und `authroot0`/`RecordTypeId=11`
keine Key- oder Recordtyp-Kollision. Diese Zuordnungen sind Bestandteil
dieser Planrevision und werden nicht erst nach Beginn der Umsetzung
entschieden:

```text
AUTH_STATE_STORE_KEY=auth0
AUTH_RECORD_TYPE_ID=10
AUTH_SCHEMA_VERSION=1
AUTH_KEY_COLLISION=NONE_ON_BASELINE
AUTH_RECORD_TYPE_COLLISION=NONE_ON_BASELINE
AUTH_ROOT_STATE_STORE_KEY=authroot0
AUTH_ROOT_RECORD_TYPE_ID=11
AUTH_ROOT_SCHEMA_VERSION=1
AUTH_ROOT_KEY_COLLISION=NONE_ON_BASELINE
AUTH_ROOT_RECORD_TYPE_COLLISION=NONE_ON_BASELINE
CONFIGURATION_BOOTSTRAP_AUTH_HANDOFF_SCHEMA=3
```

Vor dem ersten Auth-Commit wird nur noch verifiziert, dass die Implementierung
auf genau dieser unveränderten Baseline arbeitet. Eine spätere Belegung auf
`main` wäre ein Baseline-/Plan-Konflikt und erfordert Planrevision, nicht eine
Entscheidung des Builders. Der `auth0`-Record enthält nur technische
Authentication-Daten:

- `webPasswordEnabled`;
- Webpasswort: Algorithmus-/Parameter-ID, Salt, Verifier, Credential-Epoche;
- Service-PIN: eigene Algorithmus-/Parameter-ID, eigener Salt, Verifier,
  Credential-Epoche;
- je Credential Lockout-Zähler, Sperrstufe, aktiver Zustand, konservative
  Restdauer und Integritäts-/Recordsequenzdaten;
- Schema-/Längen-/CRC-/StorageEpoch-Prüfung nach dem bestehenden Envelope-
  Vertrag.

Der `authroot0`-Payload enthält ausschließlich den aktuellen
Provisionierungszustand, die aktuelle `StorageEpoch`, die
Authentication-Domain-Generation und seine Integritäts-/Sequenzdaten. Er enthält
keine Passwörter, PINs, Verifier, Sessiondaten oder CSRF-Tokens. Auf einer
frisch initialisierten oder per Factory Reset neu erzeugten Epoch wird
`UNPROVISIONED` über den bestehenden Epoch-Initialisierungspfad geschrieben
und readback-validiert; ein fehlender Root wird nie als `UNPROVISIONED`
interpretiert.

Passwörter, PINs, Session-IDs, CSRF-Tokens, Roh-Eingaben und aktive
Serviceleases gelangen nicht in den Record, Export, Diagnose- oder Backup-
Pfad. Salts und Verifier sind keine auslesbaren Zugangsdaten, bleiben aber
ebenfalls außerhalb normaler UI-/API-Projektionen.

### 4.2 Commit-/Recovery-Semantik

Credentialänderung und Lockout-Schreibvorgänge folgen dem vorhandenen
Write–Readback–Validate–Publish-Muster:

1. Eingabe vollständig und bounded validieren;
2. KDF/Verifier und Kandidatenrecord flüchtig erzeugen;
3. `StorageEpoch`, Recordtyp, Schema, Sequence und Integrität prüfen;
4. in den bestehenden Store schreiben;
5. exakt den geschriebenen Record zurücklesen und vollständig validieren;
6. erst danach neue Auth-/Session-Semantik veröffentlichen;
7. alte Credential-Epoche nicht als Fallback reaktivieren.

`CommitOutcomeUnknown`, fehlendes/mismatching Readback, Capacity-, CRC-,
Epoch- oder Schemafehler werden nie als „nicht geschrieben“ behandelt. Ist die
Auflösung nicht eindeutig, bleiben neue und alte Laufzeitberechtigungen
gesperrt, Sessions werden widerrufen und der Dienst wird
`RecoveryRequired`/nicht verfügbar. Kein stiller Runtime-/Persistenz-Split,
kein Factory-Fallback und kein Löschen zur vermeintlichen Heilung.

Factory Reset verwendet die bestehende StorageEpoch- und Recoverysemantik,
löscht Authentication-Daten logisch und widerruft Sessions; er wird durch #27
nicht neu erfunden und bleibt nur über den bestehenden lokalen, aktorsicheren
und separat geschützten Fachpfad erreichbar.

Die mehrstufige Verwendung des Roots ist ebenfalls fail-closed und nutzt nur
die vorhandenen per-Key-Write-/Readback-Semantiken des `IStateStore`:

1. Der Epoch-Initialisierungspfad schreibt `authroot0=UNPROVISIONED` und
   validiert den Readback. Ein fehlender oder unklarer Root blockiert statt
   Bootstrap zu erlauben.
2. Vor einer lokalen Erstprovisionierung wird der Root auf `PROVISIONING`
   geschrieben und readback-validiert. Ein Fehler oder unklarer Commit setzt
   den Dienst auf `AUTH_RECOVERY_REQUIRED`; der Root darf nicht automatisch
   auf `UNPROVISIONED` zurückgesetzt werden.
3. Danach wird der gemeinsame Webpasswort-/Service-PIN-Record `auth0` als
   ein Credential-Record geschrieben und vollständig readback-validiert. Ein
   Parse-, CRC-, Schema-, Epoch-, Readback- oder Commitfehler lässt den Root
   in einem nicht bootstrapfähigen Zustand.
4. Erst nach gültigem `auth0`-Readback wird `authroot0=PROVISIONED` für die
   aktuelle `StorageEpoch` und `AuthDomainGeneration=1` geschrieben und
   readback-validiert. Der Root referenziert keine veränderliche `auth0`-
   Recordsequenz.
   `PROVISIONING`, `PROVISIONING_INDETERMINATE` und
   `RECOVERY_REQUIRED` öffnen niemals den Bootstrap. Ein nach
   `PROVISIONED` fehlender oder ungültiger `auth0`-Record bleibt Recovery-
   required; er wird nicht neu angelegt.

Lockout-Zähler, Lockout-Stufen, erfolgreiche Prüfungen und Credentialwechsel
schreiben ausschließlich den vollständig validierten `auth0`-Record mit
seiner eigenen monotonen Recordsequenz. `authroot0` bleibt dabei unverändert;
`AuthDomainGeneration` ändert sich nur bei der definierten neuen
`StorageEpoch`/Domain-Initialisierung.

Damit ist der Root ein schmaler, persistenter Zustands-/Generationsanker im
gleichen Store und keine zweite Credential-Wahrheit. Eine Implementierung
darf weder die beiden Records als unbestätigte neue Transaktion behandeln
noch einen unklaren Ausgang durch Löschen, Reprovisionierung oder Factory-
Fallback „heilen“.

## 5. HTTP-/Browsergrenze und Routen

### 5.1 Gemeinsame Route-Komposition

Die bestehende `IHttpRouteSink`-Instanz erhält einen einzigen Dispatcher. Seine
Reihenfolge ist deterministisch:

1. #164 Setup-Routen erhalten `/` und `/api/network/*`, solange der Setup-Flow
   aktiv ist;
2. die normale WebUI beantwortet `/`, `/login` und statische UI-Ressourcen,
   wenn kein Setup-Flow diese Route besitzt;
3. #27 beantwortet `/api/v1/*` als öffentliche read-only API und einen
   eindeutig als intern dokumentierten Web-UI-Writebereich;
4. unbekannte oder im aktuellen Modus nicht zulässige Wege enden mit einer
   begrenzten, secret-freien Fehlerantwort.

Es gibt keinen zweiten `httpd_handle_t`, keinen zweiten Port, keinen zweiten
Route-Registry-Lebenszyklus und keine ESP-IDF-Typen in
`fermentation_app`-Contracts. Der bestehende HTTP-Body-/Response-Rahmen wird
für #27 vor Implementierung auf die maximalen DTOs geprüft und nicht durch
unbegrenztes JSON-Parsing umgangen.

### 5.2 Schutz vor Browsermutationen

Alle zustandsändernden internen Webrequests, einschließlich Login-/Logout-
Folgezuständen, Passwort-/PIN-/Modusänderungen und Fachkommandos, müssen:

- die vorgesehene Methode verwenden; `GET` mutiert niemals;
- den passenden begrenzten Content-Type verwenden, für JSON
  `application/json`;
- bei bestehender Session den Header `X-CSRF-Token` mit einem mindestens
  128-Bit-sitzungsgebundenen Token tragen; der Token wird ausschließlich über
  den in Abschnitt 1.2.1 beschriebenen bounded same-origin-Response-Handoff
  bereitgestellt und steht nie in URL, Cookie, Log oder Export;
- `Origin`, ersatzweise `Referer`, gegen den lokalen Host/Scheme-Kontext
  prüfen und bei vorhandenen Fetch-Metadata-Headern eine fremde Site ablehnen;
- die erwartete fachliche Revision bzw. den erwarteten State-/Run-/Message- /
  Configuration-Stand mitführen;
- durch Auth-/Service-, Fach-, Safety-, Conflict- und Idempotenzprüfungen
  laufen.

Login ohne bestehende Session darf keinen CSRF-Token voraussetzen, muss aber
  Methode, Content-Type, Origin-/Referer-/Fetch-Metadata-Regeln, Bodygrenze und
  Lockout einhalten. Eine Loginantwort darf nur den für die neu erzeugte
  Session bestimmten CSRF-Handoff aus Abschnitt 1.2.1 enthalten; sie erzeugt
  keine URL-, persistenten oder sessionübergreifend wiederverwendbaren Tokens.
Kein CORS-Wildcard, keine URL-Credentials und keine URL-Session.

### 5.3 Öffentliche read-only API

Die offiziell dokumentierte API bleibt lokal, versioniert und ausschließlich
lesend:

```text
GET /api/v1/status
GET /api/v1/temperatures
GET /api/v1/alerts
```

Die Ressourcen liefern mindestens Geräte-/Betriebsstatus, aktuelle Phase,
aktiven Lauf/Programmkennung, Temperaturen mit Einheit und Qualitätsstatus,
Zeit-/Laufstatus, Warnungen/Fehler sowie nichtgeheime Netzwerk-/Zeitdaten.
Fehlende Werte bleiben als fehlend/ungültig gekennzeichnet. JSON ist UTF-8,
maschinell stabile Codes und lokalisierte Texte werden getrennt ausgegeben.

Bei aktiviertem Webpasswort folgt die API der normalen Session-Authentisierung;
bei bewusst deaktiviertem Passwort ist sie im lokalen Netz über die anonyme
Session lesbar und zeigt die Warnung. Die Service-PIN ist kein API-Schlüssel.

Es gibt keine offizielle externe Write-API für Start, Stop, Programmänderung,
Quittierung, Servicefreigabe, WLAN-/Systemänderungen, Aktortests,
Firmwareupdate oder Factory Reset. Interne UI-Schreibwege bleiben als
implementation-private Route/DTO-Grenze dokumentiert und werden nicht als
stabile externe API beworben oder in der read-only API registriert.

### 5.4 Interne Web-Schreibwege

Der interne Adapter übersetzt nur auf bestehende Commands und Services:

- `FermentationUiCommandContext.surface=WebInterface` und aktuelle
  `expected`-Revisionen;
- Netzwerkmodus/Reconfiguration über die vorhandenen typisierten
  Network-Commands, ohne Credentialpayload oder Persistenzduplikat;
- Programm-, Einstellungs- und Preview-/Commit-Änderungen über den bestehenden
  Preview-/Commitpfad;
- Start, Stop, Quittieren, Mute, Abschluss und Recovery über die bestehenden
  Application-`prepare*()`-/`confirmPrepared()`- und Command-Bridge-Pfade;
- Serviceaktionen erst nach gültiger sessiongebundener Web-Servicelease und
  zusätzlicher Bestätigung; sie tragen dabei die gemeinsame Quelle
  `UiSurface::WebService`;
- Sicherheits-/Fachablehnung wird strukturiert als Ergebnis zurückgegeben,
  nicht in HTTP-Erfolg umgedeutet.

Der gemeinsame Contract-Slice erweitert `UiSurface` um `WebService` und die
zugehörigen gemeinsamen Provenienztypen um `ServiceWeb`; ein ad-hoc-String
`service_web` und eine private Web-Quelle sind unzulässig. Die bestehenden
Wirewerte bleiben stabil: `CommandSource::ServiceWeb` erhält den nächsten
freien Wert `3`, `RunChangeSource::ServiceWeb` den nächsten freien Wert `4`
(nach `Recovery=3`) und `ChangeOriginKind::ServiceWeb` den nächsten freien
Wert `4`. Mapping, Validierung und Persistenzcodec werden gemeinsam mit den
#25/#26-Contracts aktualisiert. #27 erhält weder eine parallele Command-Quelle
noch eine zweite Audit-/Revisionstruth.

Jede Mutation bringt Schutz gegen doppelte/retryte Ausführung mit:

- vorhandene Command-ID-/Run-ID-/Envelope-Semantik wird verwendet;
- Konfigurations- und Netzwerkpfade verwenden ihre vorhandenen
  Preview-/Commit-/Revisionsergebnisse;
- gleiche Anfrage mit identischer Korrelation darf keine zweite Aktion
  auslösen;
- stale Revision führt zu `Conflict`/`StateChanged`, nicht zu last-write-wins;
- Antwort und anschließender Snapshot enthalten den tatsächlichen
  owning outcome.

Für HTTP-Retries führt der Browser bei jedem internen Schreibrequest eine
sessiongebundene monotone `X-UI-Mutation-Seq` als positive Dezimalzahl mit
maximal 20 ASCII-Zeichen mit. Die Browser-Shell reserviert die Sequenzwerte
sessionweit auch über mehrere Tabs; die lokale Monotonie ist aber keine
Securitygrenze. Die Sequenz steht nicht in URL, Body, Log oder Persistenz. Der
Web-Transport hält pro Session nur `highWater`, einen kleinen Replay-Floor,
höchstens acht kürzlich abgeschlossene Outcomes und die aktuelle `InFlight`-
Mutation. Jeder Outcome enthält die Sequenz, einen bounded Fingerprint aus
Methode, Pfad, Bodylänge/-inhalt und relevanten erwarteten Revisionen sowie
den secret-freien typisierten Outcome.

Jede gültige Session erhält über den bestehenden bounded Session-/Snapshot-
Handoff ein autoritatives Transportfeld `nextMutationSeq` sowie den Zustand
`AVAILABLE`, `IN_FLIGHT` oder `EXHAUSTED`. Dieser Handoff wird bei Login,
Sessionbildung, Reload, Browser-Restore und Reconnect mit dem ohnehin
benötigten Session-/UI-Snapshot aktualisiert; er ist weder Teil der
Fachdatenprojektion noch Session-ID, Auth-Token oder persistentes Credential.
Bei `AVAILABLE` ist `nextMutationSeq=highWater+1`. Bei `IN_FLIGHT` wird die
reservierte Sequenz zusätzlich als `inFlightMutationSeq` ausgewiesen; bis zu
ihrem Outcome wird keine weitere neue Sequenz zugelassen. Nach Abschluss
liefert der nächste Handoff wieder die neue autoritative `nextMutationSeq`.

Der Server prüft und reserviert den erwarteten Wert atomar unter dem
Sessionzustand. Ein Browser-/Tab-Rennen kann deshalb höchstens eine
Mutation reservieren; der unterlegene Tab erhält einen typisierten
`409 mutation_sequence_conflict`, lädt `nextMutationSeq` und die aktuellen
Fachrevisionen neu und muss die Aktion gegen den neuen Zustand erneut
auslösen/bestätigen. Kein Tab führt nach diesem Konflikt ein blindes
automatisches Replay aus. Ein identischer Retry nach nachweislich verlorenem
Response nutzt weiterhin das begrenzte Outcome-Fenster.

`X-UI-Mutation-Seq` wird ausschließlich als `uint64` mit Overflowprüfung
geparst. Bei `highWater=UINT64_MAX` ist `nextMutationSeq` nicht darstellbar;
der Zustand wird `EXHAUSTED`, neue Mutationen liefern fail-closed
`503 mutation_sequence_exhausted` und mutieren nichts. Es gibt kein Wraparound
und keinen stillen Wechsel auf eine zweite Sequenzwahrheit.

- Die nächste neue Sequenz muss genau `highWater + 1` sein und wird vor dem
  Application-Command reserviert. Eine höhere Sequenz liefert
  `409 mutation_sequence_gap` ohne Mutation; eine kleinere Sequenz außerhalb
  des Outcome-Fensters liefert `409 mutation_replay_expired` ohne Mutation.
- Ein identischer Retry der aktuellen `InFlight`-Sequenz liefert
  deterministisch `409`/`503` ohne zweite Ausführung. Ein identischer Retry
  innerhalb des Outcome-Fensters liefert exakt denselben owning Outcome.
- Dieselbe Sequenz mit anderem Fingerprint wird innerhalb des Fensters als
  `409 mutation_sequence_reused` abgelehnt und erreicht die Anwendung nicht.
  Für bereits retirierte Sequenzen wird unabhängig vom Payload immer
  `409 mutation_replay_expired` geliefert; sie kann niemals erneut mutieren.
- Nach erfolgreichem Abschluss wird das älteste abgeschlossene Outcome bei
  Bedarf sicher retired und der Replay-Floor monoton erhöht. `InFlight` wird
  nie verdrängt. Dadurch bleiben mehr als acht sequenzielle Mutationen über
  die gesamte 30-Minuten-/12-Stunden-Session möglich, während der Speicher
  konstant bounded bleibt.
- Sequenzlücken, ungültige/überlange Werte und eine zweite neue Sequenz
  während einer `InFlight`-Mutation mutieren nichts und werden fail-closed
  beantwortet. Sessionablauf, Logout, Widerruf und Neustart verwerfen die
  flüchtige Sequenz; der alte Session-Cookie ist nach Neustart ungültig.
- Die fachliche `CommandId` und deren Persistenz-/Owning-Semantik bleiben
  ausschließlich Anwendungseigentum. Der Transport-Replayschutz erzeugt
  keinen zweiten Fachcommandbus und ersetzt nicht die fachliche
  Revisions-/Konfliktprüfung.

## 6. Responsive WebUI, Projektion und Live-Daten

### 6.1 Gemeinsames View-Modell

Der Webadapter erzeugt keine Web-Schattenmodelle für Prozess-, Temperatur-,
Meldungs-, Netzwerk-, Recovery- oder Berechtigungszustände. Er konsumiert den
von `FermentationApplication::uiSnapshot()` gelieferten kanonischen
`FermentationUiSnapshot`; dessen Application-owned Projektion verwendet die
bestehenden Owner und `FermentationUiProjector`. `refreshRevision` und die
vorhandenen `FermentationUiExpectedRevisions` werden unverändert als Webbasis
verwendet.

Wenn ein kanonischer Producer einen Wert nicht liefert, zeigt die WebUI
`unavailable`/`unknown` mit Qualitätsstatus. Sie erfindet keine Null-
Temperatur, keine sichere Zeit, keinen Aktorstatus und keine Safetyfreigabe.
Die WebUI leitet keine Fachentscheidung aus Roh-GPIO-, Adapter- oder
Netzwerkdaten ab.

### 6.2 Seiten und Funktionen

Die normale WebUI ist eine schlanke, responsive lokale Anwendung für
Mobiltelefon, Tablet und Desktop mit gleichem Funktionsumfang:

1. Übersicht: Gerätename, Prozessmodus, Produkt-/Schrank-/Solltemperatur,
   Sensorqualität, Phase, Laufzeiten, wichtigste Meldungen, Netzwerk-/Zeit-
   status, Start/Manuell;
2. Programme: Liste, Erstellung/Änderung/Löschung im bestehenden
   Configuration-Preview-/Commitpfad, erwartete Revision und Konfliktansicht;
3. manueller Betrieb: bestehende typisierte Werte/Validierungen;
4. Meldungen/Protokolle: secret-freie Meldungen, Quittieren/Mute über Commands;
5. Einstellungen: Sprache pro Browser, Geräteeinstellungen und Netzwerk-
   Verweise über bestehende Grenzen;
6. Diagnose: read-only status-, Ressourcen-, Netzwerk- und Zeitprojektionen;
7. Service: separate PIN-Freigabe, sichtbare Lease/Timeout-Information,
   Bestätigung für kritische Aktionen;
8. System: Firmware-/Build-/Reset-/Recovery-Informationen ohne Secrets.

Browserlokale Sprache ist unabhängig vom Touchdisplay und unterstützt DE/EN/ES.
Fehlende Übersetzungen fallen zuerst auf Englisch und dann auf den stabilen
technischen Schlüssel zurück. Browser- und Webpräferenzen verändern keine
Fachdaten ohne expliziten bestehenden Command.

### 6.3 Auswahl der Live-Aktualisierung

Für ESP-IDF 6.1, den vorhandenen synchronen `esp_http_server`, 4 MB Flash und
kein PSRAM werden die drei denkbaren Verfahren wie folgt bewertet:

| Verfahren | Vorteil | R1-Risiko/Mehrumfang | Entscheidung |
|---|---|---|---|
| kontrolliertes Polling | nutzt vorhandene Request-/Response-Grenze, einfache Abbrüche/Reconnects, bounded RAM, universelle Browserunterstützung, kein Stream-State | zusätzliche GETs, sichtbare Poll-Latenz, Intervall muss gemessen werden | **gewählt** |
| SSE | zeitnahe Pushdaten | langlebige HTTP-Verbindungen, Stream-Abbruch/Backpressure, zusätzlicher Adapter-/Reconnect-State und ESP-IDF-Lifecyclecode | nicht gewählt |
| WebSocket | bidirektional und zeitnah | größter eigener State-/Parser-/Verbindungs- und Ressourcenumfang; für #27 keine externe Write-API nötig | nicht gewählt |

Der konkrete R1-Vertrag ist:

- Polling-GET auf einen vollständigen, bounded Snapshot mit normalem
  `/api/v1/`-Read-Schutz;
- aktive Laufansicht nominal alle 2 Sekunden, Standby nominal alle 10 Sekunden,
  sofortiger Refresh nach eigener Commandantwort und nach Reconnect;
- Backoff bei Fehlern bis höchstens 30 Sekunden, sichtbarer Offline-/Stale-
  Zustand mit Alter des letzten gültigen Snapshots;
- nach jeder Wiederverbindung vollständiger Snapshot, kein Delta- oder
  verloren geglaubter Eventstream;
- Server kann alte/abgebrochene Requests ohne persistenten Streamstate beenden;
- keine SSE-/WebSocket-Route und keine allgemeine `IWebTransport`-
  Abstraktion im R1.

Der aktuelle Laufchart wird aus bounded, aktuellen Mess-/Ereignisdaten
projektiert. Produkttemperatur, Schranktemperatur und Sollwert enthalten
Einheit, Zeitbasis, Qualitätsstatus und sichtbare Lücken; Phasenwechsel,
Warnungen, Unterbrechungen und Laufzeitkorrekturen werden als Ereignismarker
angezeigt. Fehlende Messwerte werden nicht verbunden. Vollständige historische
Charts und eine neue Langzeitdatenbank gehören nicht zu #27.

## 7. Frontend-, JSON- und Abhängigkeitsstrategie

### 7.1 Assets

- Keine React-/Vue-/Angular-/SPA- oder sonstige große Frontendplattform.
- Kleine handgeschriebene HTML-/CSS-/JavaScript-Assets mit responsivem Layout,
  keyboard-/touchfreundlichen Controls und DE/EN/ES-Katalogen.
- Assets werden zunächst als begrenzte, compile-time eingebundene Firmware-
  Ressourcen ausgeliefert. Es wird kein zweites LittleFS-/Dateisystem-
  Credential-/Assetmodell nur aus Bequemlichkeit angelegt.
- Asset-Anfragen verwenden feste Allowlist-Pfade, Content-Type, Größen- und
  Cachegrenzen; keine Pfadtraversierung, kein Upload, kein dynamischer
  Template-Code mit Secretinhalten.
- Die Einbindung bleibt so klein, dass Firmware-/Flash-/Heapmessungen gegen
  die #29/#90-Baseline möglich sind. Bei Überschreitung des realen
  4-MB-/Heap-/Jitter-Rahmens wird der Scope begrenzt oder angehalten, nicht
  automatisch PSRAM, OTA-Slots oder ein Dateisystem ergänzt.

### 7.2 JSON

JSON bleibt ausschließlich an der HTTP-/DTO-Grenze. Fachmodelle, Persistenz-
Wireformat, Commands, Rechte, Konflikte und Redaction bleiben projektseitig.

ArduinoJson `7.4.3` ist im bestehenden PR-167-Stand bereits als bounded
HTTP-/API-Codec integriert und über den Registry-/Tag-Commit
`77771d3c07668e01d8f52acb03910c1110bb373f` gelockt. Herkunft, MIT-Lizenz und
fehlende transitive Runtime-Abhängigkeiten sind in den bestehenden
Komponenten-/Auditdokumenten festgehalten. Bibliothekstypen bleiben auf
`web_api_codec.cpp` begrenzt und leaken nicht in `fermentation_app`, Storage-
oder gemeinsame UI-Contracts. Offen bleibt ausschließlich die noch nicht
abgeschlossene integrierte Resource-Evidence (Flash, Heapspitze/-minimum,
größter Block, Fragmentierung und Laufzeit pro Profil). Es wird kein zweiter
JSON-Provider oder Parserframework eingeführt; ein Ersatz wäre eine neue
Ownerentscheidung.

### 7.3 Kryptografie und Herkunft

- `esp_http_server` bleibt der bereits gemergte ESP-IDF-6.1-Transport;
  technische Lizenz-/Lockdaten kommen aus dem fixierten ESP-IDF-Vertrag.
- KDF-/Hash-/Random-Primitive kommen aus dem fixierten ESP-IDF-/mbedTLS-
  Pfad und werden nicht als neue allgemeine Crypto-Abstraktion verpackt.
- `ISecureRandomSource` ist die einzige App-seitig benötigte Zufallsgrenze.
- Jede neue direkte Komponente erhält vor Einbindung eine Herkunfts-,
  exakte Versions-/Commit-, SPDX- und Transitivlizenznotiz. Unfixierte
  Versionen, direkte Download-Skripte und unaufgelöste transitive Lizenzen
  blockieren die Umsetzung.
- `dependencies.lock` wird ausschließlich mit dem fixierten ESP-IDF-6.1-
  Component Manager regeneriert; er wird nicht von Hand kosmetisch editiert.

## 8. Bestehender Bestand und verbleibendes Delta nach Planfreigabe

Die frühere Umsetzungsschnittfolge ist nicht erneut auszuführen. Die folgenden
Teile sind in PR #167 bereits vorhanden und bleiben unverändert erhalten:

### 8.1 Bereits implementiert und zu behalten (`EXISTING_IMPLEMENTATION`)

- bounded HTTP-Request-/Response-Metadaten und ESP-IDF-Übersetzung in
  `device_platform`/`device_platform_esp_idf`;
- `ServiceWeb`-/UI-Provenienzwerte und die gemeinsame UI-/Command-Grenze;
- Authentication-Records, `auth0`/`authroot0`, Schema-3-First-Consumer-Handoff,
  Storage-Epoch-/Mutation-Lease-/Readback-/Recoverysemantik, Lockouts und
  Credentialwechsel;
- flüchtige Websessions, Cookie-/CSRF-/Origin-/Referer-/Fetch-Metadata-Schutz,
  bounded `X-UI-Mutation-Seq` inklusive Resync-/Replay-Fenster;
- read-only `/api/v1/`-Routen, interne Auth-/Network-Routen, compile-time
  DE/EN/ES-Webassets, bounded Polling-/Snapshot-Codec und Webdispatcher vor
  den #164-Setup-Routen;
- ArduinoJson `7.4.3` als gelockter bounded Codec, ohne Bibliothekstypen in
  Application-/Storage-/UI-Contracts;
- native Auth-, HTTP-, Browserpolicy-, Session-, API- und bestehende
  Application-/UI-Regressionen sowie die zugehörigen Dokumentations- und
  Lockstände.

Die aktuell ausgelieferten compile-time-Assets bilden jedoch noch nicht den
vollständigen §6.2-/§6.3-R1-Scope ab. Der verifizierte Bestand
`EXISTING_WEB_UI` umfasst derzeit:

- Login/Logout sowie die bestehende DE/EN/ES-Sprachumschaltung;
- eine read-only-Übersichtsprojektion aus dem vorhandenen Snapshot;
- Netzwerkmoduswahl und explizite WLAN-Rekonfiguration über die bestehenden
  Routen;
- Service-PIN-Unlock, Lease-Status und Passwortmodus;
- die vorhandene Meldungsfläche ohne vollständige Acknowledge-/Mute-Bedienung;
- den vorhandenen bounded Polling-/Offline-/Stale-Grundmechanismus.

Die ausgelieferten Assets konsumieren den neuen `/internal/ui/run`-Endpoint
noch nicht. Damit bleibt als fachlicher Web-Scope
`REMAINING_WEB_UI_DELTA` offen: Übersicht mit Start/Manuell, Programme mit
den bestehenden Preview-/Commitpfaden, manueller Betrieb, vollständige
Meldungs-/Protokollaktionen, Einstellungen, read-only Diagnose, Service mit
sichtbarer Lease-/Timeout-Bestätigung, System-/Recovery-/Build-Informationen
sowie die aktuelle Laufansicht mit Ist/Soll, Qualitätslücken,
Ereignismarkern und dem vertraglich begrenzten Polling-/Reconnect-Verhalten.
Diese Lücke wird ausschließlich mit den bestehenden
`uiSnapshot()`-/typed-Command-/Preview-/Commitpfaden und den kleinen
compile-time HTML/CSS/JavaScript-Assets geschlossen; es entsteht kein zweiter
UI-Kern, kein Framework, kein WebSocket/SSE und kein LittleFS-Assetmodell.

PR #169 hat innerhalb dieses Bestands die kanonische Application-Grenze
ersetzt bzw. bestätigt: `uiSnapshot()` projektiert aus Application-owned
Quellen, `confirmPrepared()` revalidiert gegen aktuelle Evidence fail-closed,
und Web-/Renderer-Code liefert keine Sensor-, Planner- oder Safety-Evidence.
Diese Grenze ist nicht erneut zu modellieren.

### 8.2 Kanonischer Mutation-Owner für das verbleibende Delta

Der branch-eigene Pfad
`FermentationApplication::applyPreparedRequest()` ist auf
`main@b8d963e...` nicht vorhanden und gehört zum vorhandenen #27-Delta. Er ist
der beabsichtigte schmale Application-Owner für interne Web-Run-Mutationen.
Der verbleibende Adapterpfad ist deshalb verbindlich:

```text
Web-Request/DTO
  -> typisierte FermentationUi*-Intent + erwartete Revisionen
  -> FermentationApplication::prepare*()/prepareEnvelope()
  -> FermentationApplication::confirmPrepared()
  -> FermentationApplication::applyPreparedRequest()
  -> FermentationUiCommandBridge::decidePreparedCommand()
  -> RunPersistenceCoordinator::persistCommand() /
     persistFreshStartCommand()
  -> bestehender Domain-/Persistenz-Owner und tatsächlicher Outcome
```

`FermentationUiCommandBridge::decidePreparedCommand()` bleibt dabei die
kanonische fachliche Entscheidungsprojektion; sie ist kein zweiter Commandbus.
`FermentationApplication::applyPreparedRequest()` ist jedoch selbst die
letzte Application-Mutationsgrenze und prüft vor jeder
`decidePreparedCommand()`- oder Persistenzausführung das
`request.commandEnvelope().confirmed`-Bit fail-closed. Ein unbestätigtes
PreparedRequest liefert den bestehenden typisierten abgelehnten
`RunPersistenceResult`-Outcome (`InvalidDecision`, im UI-Contract nicht als
Owning-Erfolg) und erreicht weder Domain-Decision noch
`RunPersistenceCoordinator`; `RunCommandState` und Persistenz bleiben
unverändert. Web muss weiterhin `prepare -> confirm -> apply` einhalten, ist
aber nicht der einzige Sicherheitsgarant. Web konstruiert weder
`CommandDecision`, Runtime-/Sensor-/Safety-Evidence noch Persistenzresultate
und wendet keine Fachmutation selbst an. Es werden keine verteilten
Confirmation-Checks in allen Domain-Decidern eingeführt.

Der Application-Pfad besitzt stale-Confirmation-/Revision-Prüfung sowie die
Anbindung an den bestehenden `RunPersistenceCoordinator`; seine
`RunPersistenceResult`-Durability-/Recoveryzustände werden unverändert an den
internen Adapter zurückgegeben.

Damit bleibt der Pfad mit dem gemergten #168-Vertrag vereinbar: aktuelle
owning Evidence kommt ausschließlich aus der Application-/Orchestrator-Grenze,
`confirmPrepared()` blockiert eine zwischenzeitliche Regression, und
`ACTUATOR_RELEASE=NO` bleibt bestehen. `#24`-Persistenz-/Recovery-Owner werden
nicht dupliziert; die Grenzen von `#106` und `#35` bleiben außerhalb von #27.
`ResetFault` bleibt `Unavailable`, solange kein kanonischer Planner-Owner aus
dem vorgesehenen Downstream-Scope gebunden ist.

### 8.3 Verbleibendes Implementierungsdelta (`REMAINING_IMPLEMENTATION_DELTA`)

Nach Freigabe dieser Plan-SHA ist ausschließlich Folgendes noch auszuführen:

- die bestehenden internen Web-Run-Intents für Start, Stop, Completion und
  die bereits im Contract vorgesehenen Aktionen an den oben beschriebenen
  `prepare -> confirm -> applyPreparedRequest`-Pfad binden;
- den zentralen Confirmation-Guard in `applyPreparedRequest()` vor jeder
  Domain-Decision/Persistenzmutation umsetzen; kein bestätigungsabhängiges
  Verhalten in einzelne Domain-Decider duplizieren;
- Application- und Route-Resultate so abbilden, dass Stale-Confirmation,
  `RunPersistenceResult`-Durability, `PersistenceIndeterminate`,
  `PersistenceCommittedApplyFailed`, Recovery-/Blocked-Zustände und fehlende
  Aktorfreigabe nicht in einen HTTP-Erfolg umgedeutet werden;
- `REMAINING_WEB_UI_DELTA` mit den vorhandenen compile-time-Assets umsetzen:
  Übersicht mit Start/Manuell, Programme über die bestehenden
  Preview-/Commitpfade, manueller Betrieb, Acknowledge/Mute für Meldungen,
  Einstellungen, read-only Diagnose, Service-Lease/Timeout und Bestätigung,
  System-/Recovery-/Build-Informationen sowie die aktuelle Laufansicht mit
  Ist/Soll, Qualitätslücken und Ereignismarkern;
- die Assets müssen `/internal/ui/run` für die bestehenden Start-, Stop-,
  Completion-, Adjustment-, Acknowledge- und Mute-Intents tatsächlich über
  `prepare*()`/`confirmPrepared()`/`applyPreparedRequest()` verwenden;
- bounded Polling, sichtbarer Offline-/Stale-Zustand, Reconnect-Snapshot und
  der aktuelle Laufchart werden aus `FermentationApplication::uiSnapshot()`
  und den vorhandenen Projektionen konsumiert; Browsercode erfindet keine
  Runtime-, Sensor-, Safety- oder Persistenz-Evidence;
- ausschließlich die noch fehlende integrierte ArduinoJson-/Firmware-
  Ressourcen-Evidence sowie die im Plan offenen KDF-/Hardware-/Browser-
  Nachweise erheben; keine bereits bestandenen Auth-/Session-/Codec-Verträge
  neu implementieren.

Keine neue Commandbus-, Service-, Provider-, Runtime-Evidence- oder
Snapshot-Abstraktion und keine zweite Persistenz-/Safety-/Recovery-Wahrheit.

### 8.4 Gezielte Regressionen für den verbleibenden Ownerpfad

Der verbleibende Delta-Schnitt muss mindestens nachweisen:

- Web-Intent erreicht ausschließlich die Application-Prepare-/Confirm-/Apply-
  Kette; externe Evidence-Injektion bleibt unmöglich;
- `prepareEnvelope(...) -> applyPreparedRequest(unconfirmed)` mit einem
  vorhandenen `AcknowledgeMessage`- oder `MuteMessage`-Requesttyp, dessen
  Domain-Decider nicht selbst generell das Envelope-Confirmation-Bit prüft,
  liefert den typisierten abgelehnten Outcome, ruft keine Domain-Decision und
  keine Persistenz auf und lässt `RunCommandState` unverändert;
- `prepareEnvelope(...) -> confirmPrepared(...) ->
  applyPreparedRequest(confirmed)` erreicht dagegen den bestehenden
  Decision-/Persistence-Pfad und liefert dessen tatsächlichen Outcome;
- unbestätigte, stale oder nach `Valid -> Stale/Failed` revalidierte Requests
  mutieren nichts;
- bestätigte Requests erzeugen genau eine `CommandId`-/Persistenzmutation;
  identische Wiederholung liefert den bestehenden owning Outcome ohne zweite
  Mutation;
- `PersistenceIndeterminate`, `PersistenceCommittedApplyFailed`,
  `RecoveryPending`, `Blocked` und fehlende Runtime-/Aktorfreigabe bleiben
  fail-closed und werden korrekt auf den internen HTTP-Outcome abgebildet;
- ein produktnaher nativer Route-/Adaptertest ruft den internen
  `/internal/ui/run`-Pfad mit bounded Web-DTO, Session-/CSRF-/Mutation-Seq-
  Kontext und erwarteten Revisionen auf und weist die vollständige Kette
  `Web-Intent -> prepare -> confirm -> apply -> RunPersistenceCoordinator`
  nach; er deckt zusätzlich Replay ohne zweite Mutation, gleiche Sequenz mit
  anderem Payload, fehlende Runtime-/Aktorfreigabe sowie die Outcome-Status
  für Stale, Indeterminate, CommittedApplyFailed, Recovery und Blocked ab;
- Startpfade `ProgramStartRequest`/Stored-Program und `ManualStartRequest`
  bleiben über die bestehende Decision-Matrix Eigentum der Domain, ohne
  Fallbackregeln im Web zu duplizieren.

Die bereits vorhandenen Auth-/Session-/HTTP-/API-Regressionen werden als
Bestand weiter ausgeführt; neue Tests sind auf diesen Ownerpfad und die noch
offenen Ressourcen-/Nachweisgates begrenzt.

## 9. Test- und Evidence-Matrix

Nicht ausgeführte Nachweise gelten nicht als bestanden. Für die spätere
Umsetzung sind mindestens folgende Fälle mit Status und exakter HEAD zu
führen:

| Bereich | Nachweis |
|---|---|
| Baseline | `BASE_SHA`, ESP-IDF-Commit, C++17, 4 MB, kein PSRAM, beide Profile |
| Erstprovisionierung | aktuelle Epoch mit positivem `UNPROVISIONED`-Root -> Bootstrap erlaubt; korrupter Auth-Record, unsupported Schema, indeterminate Commit/Readback oder zuvor provisionierter fehlender Authzustand -> `AUTH_RECOVERY_REQUIRED` und Bootstrap verboten; Factory Reset/neue Epoch -> definierter unprovisionierter Zustand |
| First-Consumer-Handoff | gültige bestehende Pre-#27-Epoch ohne Auth-Domain -> genau einmal Schema-2-zu-3-Handoff und `UNPROVISIONED`; danach fehlender/korruptierter `authroot0` -> `AUTH_RECOVERY_REQUIRED`; Handoff-CommitOutcomeUnknown/Readbackfehler -> fail-closed; bestehende Konfiguration, WLAN-Credentials und Run-Persistenz bleiben erhalten |
| Auth-Root/Generation | `PROVISIONED` plus mehrere Lockout-Writes -> Root bleibt gültig; erfolgreicher Passwort- oder PIN-Wechsel erzeugt gültige neue `auth0`-Generation ohne Root-Mutation; fehlender/korruptierter `auth0` -> `AUTH_RECOVERY_REQUIRED` |
| Login | korrekt/falsch; 14/15/64/65 Unicode-Codepoints; UTF-8 bei 255/256/>256 Bytes; Unicode/Leerzeichen; keine Trunkierung/Kompositionspflicht; Passwortmanager/Paste; sessiongebundener CSRF-Handoff ohne URL-/Log-Secret |
| Passwort-Lockout | 5 Fehler, 30 s, exponentielle Blöcke bis 15 min, aktiver Lockout ohne KDF/Write, Fehler erst nach Write/Readback, Erfolg resetet atomar |
| Lockout-Recovery | Neustart bei aktivem Lockout, Persistenz-/Readbackfehler, kein Bypass |
| Service-PIN | korrekt/falsch, 3 Fehler, 30 s, exponentiell bis 30 min, global |
| Service-Weblease | 5 min Inaktivität, 15 min absolut, genau eine Session, zusätzliche Bestätigung |
| Session | Aktivität vor Timeout, 30-min Inaktivität, 12-h absolut, Logout, Neustartwiderruf |
| Passwortlos | anonyme Session, Warnung, kein Nutzer/Rollenmodell, Service-PIN bleibt nötig |
| Widerruf | Passwort-/PIN-/Moduswechsel, Factory Reset, Recovery, keine alte Session |
| Cookie/CSRF | Cookieflags, Set-Cookie, gültiger/missing/falscher Token, same-origin Handoff, keine URL-Tokens |
| Browsergrenze | Methode, Content-Type, Origin, Referer-Ersatz, Fetch-Metadata, fehlende/duplizierte/zu große Header fail-closed, Statusmapping statt 500, CORS-Ablehnung |
| Revision | stale User-/Program-/Run-/Message-/Network-Revision -> Conflict, kein Überschreiben |
| Application-Ownerpfad | Web-Intent -> `prepare*()` -> `confirmPrepared()` -> `applyPreparedRequest()` -> `RunPersistenceCoordinator`; unbestätigt/stale/indeterminate/committed-apply-failed ohne zweite Mutation und ohne fehlende Aktorfreigabe als Erfolg zu melden |
| Confirmation-Guard | unbestätigtes `prepareEnvelope()` erreicht weder `decidePreparedCommand()` noch Persistenz; bestätigtes Request erreicht genau den bestehenden Ownerpfad |
| Idempotenz | mehr als acht sequenzielle Mutationen in derselben Session funktionieren; identischer aktueller Retry erzeugt keine zweite Mutation; retirierter Replay und gleiche Sequenz mit anderem Payload werden ohne Mutation abgelehnt; `InFlight` wird nicht verdrängt; bounded Speicher bleibt konstant; parallele Display-/Webrevision bleibt konfliktfest |
| Mutation-Resync | Reload/Browser-Restore/Reconnect derselben Session erhält autoritatives `nextMutationSeq`; zwei Tabs reservieren gleichzeitig höchstens eine Mutation; unterlegener Tab resynchronisiert Fachrevisionen ohne blindes Replay; `UINT64_MAX`/Overflow bleibt fail-closed |
| Safety | formal gültige Webaktion wird bei fehlender Safety-/Fach-Evidenz abgelehnt |
| API | `/api/v1/status`, `/temperatures`, `/alerts`, Authmodus, stabile Codes, keine Secrets |
| API-Grenze | keine offizielle externe Write-Operation in OpenAPI-/Route-/Dokumentationsfläche |
| Live | 2-s/10-s Polling, Backoff, Offline/Stale, vollständiger Snapshot nach Reconnect |
| Chart | Ist/Soll, Einheit/Zeitbasis, Qualitätslücken, Phasen-/Warn-/Unterbrechungsmarker |
| UI | mobile/tablet/desktop, Übersicht mit Start/Manuell, Programme, manueller Betrieb, Meldungen/Acknowledge/Mute, Einstellungen, Diagnose, Service-Lease/Bestätigung, System/Recovery/Build, DE/EN/ES, englischer dann technischer Fallback, kein Horizontalzwang |
| Web-Route-Integration | `/internal/ui/run` wird aus den compile-time-Assets mit bounded DTO, Session-/CSRF-/Mutation-Seq-Schutz und aktuellen Revisionen aufgerufen; confirmed/unconfirmed, stale, Replay, Indeterminate, CommittedApplyFailed, Recovery und Blocked werden end-to-end truthful abgebildet |
| Netzwerk | #164 Setup-Routen gewinnen im Setup-Flow; kein zweiter Server/Store/SSID-/Passwortpfad |
| Isolation | Browserabbruch, WLANverlust, langsame/zu große Anfrage beeinflusst Prozess/Safety nicht |
| Ressourcen | Flash, statischer RAM, freier/minimaler Heap, größter Block, Pollinglast, Jitter, Watchdog, max. 4 Sessions |
| Abhängigkeiten | exakte v6.1-Locks, SPDX/transitive Notices, JSON/KDF-Quelle, keine unfixierten Downloads |
| Secret-Scan | keine Secrets in HTML/JS/JSON/Logs/URLs/Diagnose/Backup/Fehlertexten |

Die ESP-IDF-Tests müssen `esp32_bringup` und `esp32_release` mit derselben
relevanten Web-/Auth-Implementierung bauen. Die Hardware-Smoke-Gates bleiben
die bestehenden #29/#90-/Upgrade-Verträge; für #27 werden keine Aktoren,
Displays oder Touchbedienungen aktiviert.

## 10. Ressourcen-, Lizenz- und Freigabegates

Vor produktiver Web-/Auth-Freigabe müssen die realen integrierten Werte gegen
die vorhandene Baseline vorliegen:

- Firmware-/Asset-/JSON-Größe bei maximalem vorgesehenem UI-Inhalt;
- statischer RAM, freier Heap, niedrigster Heap, größter zusammenhängender
  Block bei Regelbetrieb plus mindestens vier Websessions;
- Antwort-/KDF-/Pollingzeiten, Regelzyklus-Jitter, Watchdog-/Resetstatus;
- bounded Requests, Abbrüche, parallele Browser und Reconnects;
- Auth-Store-Write-/Readback-/Power-Cut-/Epoch-Recovery;
- exakte Herkunft, Versions-/Commitauflösung, SPDX und transitive Notices.

`TBD_IMPLEMENTATION_BUDGET` darf nicht durch einen erfolgreichen Hosttest oder
ESP-IDF-Build in PASS umbenannt werden. Eine Ressourcenüberschreitung führt zu
einer begrenzten Planrevision oder einem Ownerentscheid, nicht zu stiller
PSRAM-, OTA-, Cloud-, Filesystem- oder Frameworkausweitung.

## 11. Abhängigkeiten, Reihenfolge und Gates

1. Planfreigabe der exakten `PLAN_SHA`.
2. Implementierungs-Dependency-/KDF-/JSON-Revalidierung gegen ESP-IDF 6.1;
   bei materiellem Widerspruch Planrevision vor Code.
3. Bounded DTO-/Route-/Asset-Prototyp sowie die produktnahe native
   Web-Route-/Adapter-Regression; Web-UI-Assets konsumieren danach denselben
   bestehenden Command-/Snapshot-Vertrag.
4. Authentication-Record-/Readback-/Lockout-Schnitt mit nativen Cutpoint-
   Tests; erst danach Web-Mutation.
5. gemeinsamer Snapshot-/Command-Adapter und read-only API.
6. Cookie-/CSRF-/Browserpolicy und interne Writewege.
7. responsive Assets, Polling, Chart und Sprache.
8. beide ESP-IDF-Profile, Architektur-/Secret-/Diff-Gates und integrierte
   Ressourcenmessung.
9. Builder-Self-Check; danach Independent Full Review mit
   `OPEN_BLOCKERS=0`.
10. Erst nach Ownerautorisierung des finalen Heads lokaler Pre-Ready-Lauf;
    anschließend Owner-Ready, GitHub-CI, Merge-Gate gemäß Workflow.

Ein Review- oder Ressourcenbefund darf nicht durch #31-Hardwareevidence,
einen historischen 6.0.2-Build oder eine zusätzliche Web-/Auth-Plattform
umgangen werden.

## 12. Dokumentationsdelta dieser Planrevision

Das tatsächliche Delta dieser Runde ist bewusst kleiner als der bereits
implementierte PR-167-Bestand:

1. diese Datei als vollständige versionierte Delta-Planrevision;
2. der PR-Body, Issue #27 und genau ein bestehender `SESSION HANDOVER` als
   Status-/Provenienz-Metadaten;
3. die explizite Entscheidung für `applyPreparedRequest()` als schmalen
   Application-Owner und die dazugehörige verbleibende Regression-/Outcome-
   Matrix;
4. die repository-first Trennung von `EXISTING_WEB_UI` und
   `REMAINING_WEB_UI_DELTA` einschließlich der fehlenden
   Route-/Asset-/Laufansicht-Regressionen.

Die frühere Mergeauflösung hat den doppelten `uiRefreshTracker_`-Member als
rein technische Konfliktbereinigung entfernt; daraus wird keine neue
Produktänderung abgeleitet. `docs/ROADMAP.md`, `docs/WEB_UI.md`, alle
Auth-/API-/Storage-Dokumente, Produktcode, CMake, Lockfiles, Tests, Workflows,
ESP-IDF-Konfiguration, Partitionen, Hardware, Ready-/Merge-/Issue-Close-Status
und Aktorfreigaben werden durch diese Planreview nicht erneut geändert.

## 13. Nicht-Ziele und Hardware-/Clientaussage

Ausdrücklich nicht Teil von #27:

- Display-/Touchhardware, SPI, ILI9341/XPT2046, Kalibrierung, LVGL, Fonts,
  Renderer- oder Bibliotheksauswahl aus #31;
- erneute WLAN-/SoftAP-/Credentialimplementierung, Setup-Server oder mDNS-
  Lebenszyklus aus #164;
- Cloud, Internetbetrieb, Accounts, Benutzer/RBAC, OAuth/OIDC, VPN,
  Remember-me, persistente Sessions, externe Write-API, Web-OTA oder
  Firmwaredownload;
- WebSocket/SSE-/Streamplattform, allgemeines Middleware-/Provider-/Plugin-
  Framework, zweiter JSON-Codec, zweiter Store oder zweite UI-/Commandtruth;
- vollständige historische Laufcharts, unbegrenzte Uploads/Exporte oder
  Millisekunden-Echtzeit;
- direkte GPIO-/Aktor-/Safetysteuerung aus HTTP oder JavaScript.

**Kann #27 ohne #31 umgesetzt werden? Ja.** Die WebUI und ihre native
Auth-/API-/Command-/Conflict-Tests können auf dem bestehenden
rendererunabhängigen #25/#26-Vertrag und dem gemergten #164-HTTP-/WLAN-
Fundament implementiert und in beiden ESP-IDF-Profilen gebaut werden. Für
die funktionale Freigabe bleiben reale Browser-/WLAN-Clientnachweise vom
jeweiligen #164-Connectivity-/Owner-Gate abhängig; sie benötigen aber keinen
physischen Touch-/Display- oder LVGL-Pfad.

**Hardware-/Client-Evidence, die vertagt werden kann:** physische
Display-/Touchfunktion, Renderer-Smoke, Kalibrierung, lokale Touchbedienung
und jede Anzeige-/QR-Evidence aus #31 bleiben vertagt. Ebenfalls bis zum
verfügbaren #164-Evidencepfad vertagbar sind reale AP_ONLY/HOME_WIFI-
Browser-/mDNS-/Direct-IP-Langläufe. Sie dürfen nicht durch native Tests,
historische #89-Evidence oder simulierte Clientresultate als PASS ersetzt
werden. Native Route-/Auth-/Conflict-/Redaction-/Polling-Tests und
ESP-IDF-Build-/Ressourcennachweise sind davon unabhängig planbar.

## 14. Offene Ownerentscheidungen nach vollständiger Analyse

Es verbleiben nur technische Freigabepunkte, die durch die verbindlichen
Quellen noch nicht abschließend entschieden oder gemessen sind:

1. Ownerfreigabe dieser exakten Plan-SHA als Voraussetzung für jede
   Implementation.
2. nach v6.1-KDF-Messung: finaler PBKDF2-/mbedTLS-Work-Factor und eventuelle
   dokumentierte Rest-Risiken, falls Plattformverschlüsselung weiterhin nicht
   aktiviert ist;
3. finale Messbestätigung der maximal vier parallelen Websessions, Asset-/DTO-
   Grenzen, Pollinglast und KDF-Laufzeit gegen die bestehende Ressourcen-
   baseline.

Die Passwortgrenzen sind keine offene Ownerentscheidung mehr:
`15..64 Unicode-Codepoints`, maximal `256 UTF-8-Bytes`, keine
Kompositionsregeln oder Trunkierung, Passwortmanager/Paste erlaubt.

Diese Punkte sind keine Einladung zu Scope-Erweiterung. Ohne eine notwendige
Freigabe oder einen reproduzierbaren Nachweis bleibt der betroffene Teil
`NOT_RUN`/`BLOCKED`; es wird kein unsicherer Ersatz implementiert.

## 15. Planabschlussmarker

```text
ISSUE27_PLAN_REVISION=COMPLETE_FOR_OWNER_REVIEW
BASELINE_MAIN=b8d963e8d830b95b160dfb7e5cc9c2d033ad53e9
PREVIOUS_APPROVED_PLAN_SHA=da30f0526b6ffd76e51297d00291afe5808a4815
APPLICATION_RUNTIME_EVIDENCE_PREDECESSOR_ISSUE=168
APPLICATION_RUNTIME_EVIDENCE_PREDECESSOR_PR=169
APPLICATION_RUNTIME_EVIDENCE_MAIN=b8d963e8d830b95b160dfb7e5cc9c2d033ad53e9
APPLICATION_RUNTIME_EVIDENCE_STATUS=AVAILABLE
ESP_IDF=v6.1.0@fff9895c82d744c7237be8847347bdd1b07c6643
ISSUE164_HTTP_LIFECYCLE_REUSED=YES
SECOND_HTTP_SERVER=NO
SECOND_CONNECTIVITY_TRUTH=NO
SECOND_AUTH_STORE=NO
SHARED_UI_CONTRACTS_REUSED=YES
LIVE_UPDATE_SELECTION=BOUNDED_POLLING
PUBLIC_API=READ_ONLY
WEB_MUTATIONS=INTERNAL_COMMAND_ADAPTER_ONLY
ISSUE31_REQUIRED=NO
ISSUE31_IMPLEMENTATION=EXCLUDED
IMPLEMENTATION_STATUS=PAUSED_PLAN_REVALIDATION
EXISTING_IMPLEMENTATION_PRESENT=YES
REMAINING_IMPLEMENTATION_DELTA=OWNER_APPROVAL_REQUIRED
ACTUATOR_RELEASE=NO
PASSWORD_POLICY_OWNER_DECISION=ACCEPTED_2026-09-21
OPEN_REVIEW_BLOCKERS=6
PLAN_REVISION_REASON=IMPLEMENTATION_REVIEW_WEB_UI_ROUTE_TEST_CONFIRMATION_GUARD_SELF_CHECK_KDF_RESOURCE
WEB_FULL_SCOPE=PLAN_REVALIDATED_ON_APPLICATION_OWNED_CONTRACT
PLAN_REVIEW_REQUIRED=YES
OWNER_PLAN_APPROVAL_REQUIRED=YES
PLAN_FIX_VERIFICATION=REQUIRED
PLAN_STATUS=DRAFT_OWNER_APPROVAL_REQUIRED
NEXT_GATE=OWNER_PLAN_APPROVAL_OF_REVISED_PLAN
IMPLEMENTATION_AUTHORIZATION=NO
```
