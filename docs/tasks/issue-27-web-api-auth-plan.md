# Planrevision – Issue #27: Web-API, Weboberflaeche, Anmeldung und Bedienkonflikte

## Status, Zweck und harte Basis

Dies ist die vollständige, eigenständig reviewfähige Planrevision für Issue
#27 auf dem aktuellen kanonischen `main`. Sie ist ein Plan-only-Artefakt. In
dieser Revision werden keine Produktmodule, Authentifizierungsdaten, HTTP-
Routen, Webassets, Tests, Abhängigkeiten, Persistenzschemas oder
Hardwarepfade implementiert.

Die Live-Prüfung wurde am 2026-09-21 in einem frischen Arbeitsbaum gegen
GitHub-`origin/main` durchgeführt. Diese Revision korrigiert den bestehenden
Draft-Plan in PR #167 nach der unabhängigen Planprüfung; die frühere
Issue-Beschreibung enthielt noch die historische ESP-IDF-6.0.2-Bezeichnung.
Dieser Plan verwendet ausschließlich den nach #159 und #165 kanonischen
ESP-IDF-6.1-Stand.

```text
ISSUE=27
PR=167
PR_STATUS=DRAFT
BASE_BRANCH=main
BASE_SHA=1f1755e5e706fb668472920545b5302fcef1df16
CURRENT_HEAD=1f1755e5e706fb668472920545b5302fcef1df16
PLAN_REVISION=FULL_WEB_API_AUTH_REVALIDATION_ON_POST_165_MAIN
PLAN_STATUS=OWNER_APPROVAL_REQUIRED
PLAN_SHA=EXACT_COMMIT_RECORDED_AFTER_THIS_REVISION
IMPLEMENTATION_AUTHORIZATION=NO
PRODUCT_IMPLEMENTATION=NOT_STARTED
ESP_IDF_VERSION=v6.1.0
ESP_IDF_COMMIT=fff9895c82d744c7237be8847347bdd1b07c6643
TARGET=ESP32-WROOM-32E
FLASH=4_MB
PSRAM=NOT_REQUIRED
CXX_STANDARD=GNU++17
PR165_STATUS=MERGED
PR165_MERGE_SHA=1f1755e5e706fb668472920545b5302fcef1df16
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

Die Planumsetzung muss diese vorhandenen Grenzen direkt verwenden:

| Bestand | Verifiziert vorhanden | Verwendung in #27 |
|---|---|---|
| HTTP-Port | `device_platform::IHttpServerLifecycle`, `IHttpRouteSink`, `HttpRequest`, `HttpResponse` | einzige Request-/Response-Grenze; keine zweite Serverinstanz |
| ESP-IDF-Adapter | `EspIdfHttpServerLifecycle` mit `esp_http_server`, PIMPL und begrenztem Body | technische Start-/Stop-/Übersetzung bleibt Adapter-/#164-Eigentum |
| #164-Routen | `NetworkSetupRoutes` für `/`, `/api/network/status`, `/api/network/scan` und Kandidat/Commit | Setup-Routen bleiben aktiv und werden vor normalen Webrouten delegiert |
| Netzwerk | `FermentationApplication`, `NetworkConfigurationService`, `ConnectivityCredentialStore`, `NetworkAccessPointInfo` | #27 liest Status; Modus, Candidate/Commit und Credentials werden nicht dupliziert |
| UI-Snapshot | `FermentationUiSnapshot`, `FermentationUiProjector`, `FermentationUiExpectedRevisions`, secret-freies `FermentationNetworkModeView` | Web und Touch erhalten dieselbe Projektion; fehlende Werte bleiben ungültig/unverfügbar |
| UI-Commands | `FermentationUiCommand`, `FermentationUiCommandBridge`, Konfigurations-/Netzwerkcommands und erwartete Revisionen | Webadapter liefert nur typisierte Werte und Revisionen; die Anwendung entscheidet |
| Service-Policy | `ServiceSessionPolicy`, `ServiceSessionLease`, `fermentationWebServicePolicy()` | 5 Minuten Inaktivität / 15 Minuten absolut, getrennt von Touch |
| Persistenz | `IStateStore`, `StorageEnvelope`, `StorageEpoch`, Readback und `CommitOutcomeUnknown` | Authentifizierungsdomäne wird erster realer Auth-Consumer auf diesem Backend |
| Zufall/Zeit | `ISecureRandomSource`, ESP-IDF-Zufallsadapter, `ITimeSource` | Session-/CSRF-Zufall und Zeitouts; keine URL-/Log-Secrets |
| Tests | native `test/`-Komponenten und bestehender Test-Support | Auth-, Route-, Redaction-, Konflikt- und Reconnecttests im vorhandenen Muster |

Es gibt aktuell keine normalen Webassets, keine Websessionverwaltung, keinen
Webpasswort-Verifier, keine Authentication-Domäne und keinen normalen
Web-API-Routenadapter. Diese Lücken sind der eigentliche #27-Scope; sie
werden nicht durch eine zweite HTTP-, UI- oder Persistenzplattform gefüllt.

#### 1.2.1 Bounded HTTP-Metadatenvertrag vor der Umsetzung

Der vorhandene plattformneutrale HTTP-Vertrag wird vor jeder #27-Route um
eine kleine typisierte Metadatenstruktur erweitert. Es gibt keine allgemeine
Header-Map und kein generisches Middleware-Framework. Die Request-Metadaten
tragen ausschließlich diese einzelnen Felder: `Host`, `Content-Type`,
`Cookie`, `X-CSRF-Token`, `X-UI-Mutation-Id`, `Origin`, `Referer` und
`Sec-Fetch-Site`. Die Response-Metadaten tragen ausschließlich `Set-Cookie`
und `Retry-After`; Statuscode, Response-Content-Type und Body bleiben Teil des
bestehenden `HttpResponse`.

Die Grenzen sind fest und werden bereits im ESP-IDF-Adapter geprüft: maximal
acht bekannte Request-Metadatenfelder, höchstens ein Vorkommen je Feld,
insgesamt 2048 Bytes und je Feld maximal `Host=256`, `Content-Type=64`,
`Cookie=512`, `X-CSRF-Token=64`, `X-UI-Mutation-Id=32`, `Origin=256`,
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
        -> FermentationUiProjector / bestehender Application-Read-Facade
        -> FermentationUiCommandBridge / bestehende owning Services
        -> fachliche Validierung, Revision, Persistenz, Safety, Publish
```

Die Route-Reihenfolge verhindert, dass die normale WebUI den Setup-Flow
übernimmt oder #164-Routen dupliziert. Die normale WebUI wird erst ausgeliefert,
wenn kein aktiver #164-Setup-Flow die angeforderte Route besitzt. Ein
Netzwerkfehler, HTTP-Fehler oder Web-Heapfehler darf `application.update()`,
Regelung, Persistenz-Recovery oder Safety nicht beenden.

## 3. Authentisierung, Sessions und Berechtigungen

### 3.0 Erstprovisionierung und unprovisionierter Zustand

Ein fehlender oder ungültiger Authentication-Record in der aktuellen
`StorageEpoch` bedeutet ausdrücklich `AUTH_BOOTSTRAP_UNPROVISIONED` und nicht
„Passwortschutz bewusst deaktiviert“. In diesem Zustand gibt es keine normale
Websession, keinen anonymen Normalbetrieb und keinen LAN-Erstschreiber.
Normale Web-/API-Routen liefern nur eine feste, secret-freie
`503 AUTH_NOT_PROVISIONED`-Antwort bzw. eine statische lokale
„Provisionierung erforderlich“-Seite ohne mutierenden Webpfad.

Die erste Einrichtung von Webpasswort und Service-PIN erfolgt gemeinsam über
einen typisierten Bootstrap-Command an der bestehenden lokalen,
rendererunabhängigen Application-/UI-Grenze. Er akzeptiert ausschließlich
`UiSurface::LocalDisplay` mit expliziter lokaler Bestätigung; `WebInterface`
und `WebService` werden vom Application-Eigentümer abgewiesen. Damit kann der
Pfad ohne #31 über native Commands, einen deterministischen Store und
Testdoubles implementiert und getestet werden, ohne einem beliebigen
LAN-Absender Erstvertrauen zu geben. Es gibt weder Factory-PIN noch
Defaultpasswort.

Webpasswort und Service-PIN werden in einem atomaren Authentication-Record
angelegt. Ein Factory Reset erhöht die bestehende `StorageEpoch`, invalidiert
den alten Auth-Record und alle Sessions logisch und führt erneut in
`AUTH_BOOTSTRAP_UNPROVISIONED`; kein alter Epoch-Record und kein Fallback-
Credential wird wieder aktiviert. Erst nach erfolgreichem Readback-Commit
steht der normale Webpfad zur Verfügung.

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
- Die minimale und maximale Eingabepolicy ist in dieser Planrevision noch
  nicht stillschweigend kanonisiert: `WEB_PASSWORD_MIN_LENGTH=
  OWNER_DECISION_REQUIRED` und `WEB_PASSWORD_MAX_UTF8_BYTES=
  OWNER_DECISION_REQUIRED`. Leere, nur aus Whitespace bestehende und nicht
  vollständig darstellbare Eingaben werden unabhängig davon abgelehnt. Die
  Policy ist keine Verschlüsselung und darf nicht durch den Browser ersetzt
  werden; die Umsetzung bleibt bis zur Ownerentscheidung für diese beiden
  Grenzen blockiert.

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

Der Implementierungsschnitt führt einen eigenen, typisierten V1-Record ein:

```text
StateStoreKey=auth0
RecordTypeId=10
SchemaVersion=1
StorageEpoch=current active epoch
```

Die Baseline-Prüfung auf `main@1f1755e5e706fb668472920545b5302fcef1df16`
ergibt für `auth0` und `RecordTypeId=10` keine Key- oder Recordtyp-Kollision.
Diese Zuordnung ist Bestandteil dieser Planrevision und wird nicht erst nach
Beginn der Umsetzung entschieden:

```text
AUTH_STATE_STORE_KEY=auth0
AUTH_RECORD_TYPE_ID=10
AUTH_SCHEMA_VERSION=1
AUTH_KEY_COLLISION=NONE_ON_BASELINE
AUTH_RECORD_TYPE_COLLISION=NONE_ON_BASELINE
```

Vor dem ersten Auth-Commit wird nur noch verifiziert, dass die Implementierung
auf genau dieser unveränderten Baseline arbeitet. Eine spätere Belegung auf
`main` wäre ein Baseline-/Plan-Konflikt und erfordert Planrevision, nicht eine
Entscheidung des Builders. Der Record enthält nur technische
Authentication-Daten:

- `webPasswordEnabled`;
- Webpasswort: Algorithmus-/Parameter-ID, Salt, Verifier, Credential-Epoche;
- Service-PIN: eigene Algorithmus-/Parameter-ID, eigener Salt, Verifier,
  Credential-Epoche;
- je Credential Lockout-Zähler, Sperrstufe, aktiver Zustand, konservative
  Restdauer und Integritäts-/Recordsequenzdaten;
- Schema-/Längen-/CRC-/StorageEpoch-Prüfung nach dem bestehenden Envelope-
  Vertrag.

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
- Start, Stop, Quittieren, Mute, Abschluss und Recovery über den bestehenden
  Application-/Command-Bridge-Pfad;
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
sessiongebundene `X-UI-Mutation-Id` aus 16 zufälligen Bytes als 32
kleingeschriebenen Hexzeichen mit. Sie steht nicht in URL, Body, Log oder
Persistenz. Der Web-Transport hält pro Session ein festes Ledger mit acht
Einträgen. Jeder Eintrag enthält ID, einen bounded Fingerprint aus Methode,
Pfad, Bodylänge/-inhalt und relevanten erwarteten Revisionen sowie den
secret-freien typisierten Outcome und `InFlight`-/`Completed`-Status.

- Die ID wird vor dem Application-Command reserviert. Ein identischer Retry
  während `InFlight` liefert deterministisch `409`/`503` ohne zweite
  Ausführung; ein identischer Retry nach `Completed` liefert exakt denselben
  owning Outcome zurück.
- Dieselbe ID mit anderem Fingerprint wird als `409 mutation_id_reused`
  abgelehnt und erreicht die Anwendung nicht. Fehlende, ungültige oder zu
  große IDs liefern `400` und mutieren nichts.
- Ein volles Ledger verdrängt keinen Eintrag: es antwortet fail-closed mit
  `503 retry_ledger_full`. Einträge enden erst mit Sessionablauf, Logout,
  Widerruf oder Neustart; der alte Session-Cookie ist nach Neustart ungültig.
- Die fachliche `CommandId` und deren Persistenz-/Owning-Semantik bleiben
  ausschließlich Anwendungseigentum. Das HTTP-Ledger korreliert nur die
  Transportwiederholung.

## 6. Responsive WebUI, Projektion und Live-Daten

### 6.1 Gemeinsames View-Modell

Der Webadapter erzeugt keine Web-Schattenmodelle für Prozess-, Temperatur-,
Meldungs-, Netzwerk-, Recovery- oder Berechtigungszustände. Eine kleine
Application-Facade assembliert aus den bestehenden Ownern die
`FermentationUiProjectionInput` und lässt `FermentationUiProjector` den
`FermentationUiSnapshot` erzeugen. `refreshRevision` und die vorhandenen
`FermentationUiExpectedRevisions` werden unverändert als Webbasis verwendet.

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

ArduinoJson `7.4.3` ist der aktuelle Evaluationskandidat aus dem
Adopt-or-build-Audit. Vor seiner Übernahme sind offizielle Herkunft,
MIT-Lizenz, exakter Registry-/Git-Commit, transitive Lizenzen, v6.1-Build,
begrenzte Tiefe/Bodygröße, Grenzwert-/Fuzz-/Redaction- und Ressourcenmessung
zu dokumentieren. Bibliothekstypen dürfen `fermentation_app`, Storage- oder
gemeinsame UI-Contracts nicht leaken. Scheitert der Kandidat, wird nicht
vorsorglich ein zweiter JSON-Provider oder Parserframework eingeführt; ein
konkreter Ersatz bedarf eines neuen Ownerentscheids.

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

## 8. Konkreter Umsetzungsschnitt nach Planfreigabe

Die folgende Liste ist geschlossen genug für die Umsetzung, ohne jetzt Code
anzulegen. Jede Erweiterung außerhalb dieser Liste erfordert Planrevision und
erneute Ownerfreigabe.

### 8.1 Gemeinsame Application-/UI-Grenze

- `lib/fermentation_app/src/fermentation_application.hpp/.cpp`: kleinste
  öffentliche Read-Facade für einen aktuellen `FermentationUiSnapshot`,
  gemeinsame Route-Komposition und Auth-/Session-Integration am Application-
  Boundary; keine Webtypen im Fachkern.
- `lib/fermentation_app/src/fermentation_ui_projector.*` und, falls der
  aktuelle Application-State es erfordert, `fermentation_ui_models.*`:
  nur fehlende kanonische Projektionsinputs/Revisionen ergänzen; keine
  Web-Schattenmodelle.
- `fermentation_ui_commands.*`/`run_commands.*` nur dann ändern, wenn die
  bestehende Source-/Service-Web-Auditsemantik den kleinen gemeinsamen
  Contract-Ausbau verlangt; keine private Web-Enum oder Stringquelle.
- `lib/device_platform/src/device_ui_contracts.hpp` und
  `fermentation_ui_commands.*` für `UiSurface::WebService`;
- `run_commands.*`, `run_snapshot.*`, `configuration_graph.*`,
  `run_persistence_codec.*` und die zugehörigen Tests für den stabilen
  `ServiceWeb`-Provenienzwert; keine private Web-Enum oder Stringquelle.

### 8.2 Authentication und Sessions

- neue kleine #27-Anwendungsmodule für typisierte Authentication-Record-
  Codec/Store, Verifier/KDF-Auswahl, Lockout und Credentialwechsel;
- neuer bounded Web-Session-Manager mit flüchtigen Session-/CSRF-Records und
  Wiederverwendung von `ServiceSessionLease` für die Web-Servicelease;
- `configuration_storage_contract.*` nur für den geprüften Auth-Recordtyp /
  Schema-/Keyvertrag;
- `IStateStore`, `StorageEnvelope`, `StorageEpoch`,
  `CommitOutcomeUnknown` unverändert verwenden; keine zweite
  Backendimplementierung;
- native Tests für Store-Cutpoints, Readback, Epochwechsel, Reset, Lockout und
  Secret-Redaction.

### 8.3 HTTP-/API-/Webadapter

- neue kleine `web_application_routes.*`, `web_api_codec.*` und
  `web_assets.*` innerhalb der zulässigen Application-/Composition-Grenze;
- gemeinsamer Dispatcher vor `NetworkSetupRoutes`, kein zweiter Lifecycle;
- `/api/v1/status`, `/api/v1/temperatures`, `/api/v1/alerts` und bounded
  vollständiger UI-Snapshot für Polling;
- interne, ausdrücklich nicht öffentliche UI-Write-DTOs mit Auth-/CSRF-/Origin-
  Prüfung und erwarteten Revisionen;
- `lib/device_platform/src/http_server_lifecycle.hpp` erhält nur den in
  Abschnitt 1.2.1 definierten bounded Request-/Response-Metadatenvertrag;
- `lib/device_platform_esp_idf/src/esp_idf_http_server_lifecycle.cpp` extrahiert
  und begrenzt die genannten ESP-IDF-Header, weist Response-Status und die
  beiden typisierten Response-Metadaten zu und leakt keine ESP-IDF-Typen in
  Application-Contracts;
- `main/app_main.cpp` und ggf. `lib/fermentation_app/CMakeLists.txt` nur für
  Composition und compile-time Assets; keine Produkt- oder Hardwareänderung;
- `lib/device_platform_esp_idf` wird nur geändert, falls der bestehende
  Adapter für die bereits vereinbarte Route-Delegation eine nachweislich
  notwendige, gekapselte Übersetzung braucht. Keine ESP-IDF-Typen in
  Application-Headern.

### 8.4 Tests und Dokumentation im späteren Implementierungs-PR

- neue native Testkomponenten nach bestehendem `test/`-Muster für Auth,
  Sessions, HTTP-Policy, API-Redaction, DTO-Grenzen, Konflikte, Idempotenz,
  Polling-Stale/Reconnect und Language-Fallback;
- direkte Konsumententests für bestehende UI-Command-/Configuration-/Network-
  Verträge;
- ESP-IDF-Build-/Lock-/Asset-/Heap-/Flash-/Jitterevidence in den beiden
  bestehenden Profilen;
- kanonische Dokumente nur mit dem nachgewiesenen Implementierungsdelta
  aktualisieren. Plan, PR, Issue und genau ein aktueller Handover bleiben
  Statusquellen; Anforderungen werden nicht in mehrere Fachverträge kopiert.

## 9. Test- und Evidence-Matrix

Nicht ausgeführte Nachweise gelten nicht als bestanden. Für die spätere
Umsetzung sind mindestens folgende Fälle mit Status und exakter HEAD zu
führen:

| Bereich | Nachweis |
|---|---|
| Baseline | `BASE_SHA`, ESP-IDF-Commit, C++17, 4 MB, kein PSRAM, beide Profile |
| Erstprovisionierung | fehlender Auth-Record ist nicht passwordlos, kein LAN-Erstschreiber, LocalDisplay-Bootstrap, atomarer Webpasswort-/PIN-Commit, Factory-Reset zurück zu `AUTH_BOOTSTRAP_UNPROVISIONED` |
| Login | korrekt/falsch, leere/zu lange/Whitespace-Eingabe, sessiongebundener CSRF-Handoff ohne URL-/Log-Secret |
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
| Idempotenz | verlorene Antwort plus identischer Retry erzeugt höchstens eine Mutation; gleiche ID mit anderem Payload wird abgelehnt; parallele Display-/Webrevision bleibt konfliktfest |
| Safety | formal gültige Webaktion wird bei fehlender Safety-/Fach-Evidenz abgelehnt |
| API | `/api/v1/status`, `/temperatures`, `/alerts`, Authmodus, stabile Codes, keine Secrets |
| API-Grenze | keine offizielle externe Write-Operation in OpenAPI-/Route-/Dokumentationsfläche |
| Live | 2-s/10-s Polling, Backoff, Offline/Stale, vollständiger Snapshot nach Reconnect |
| Chart | Ist/Soll, Einheit/Zeitbasis, Qualitätslücken, Phasen-/Warn-/Unterbrechungsmarker |
| UI | mobile/tablet/desktop, DE/EN/ES, englischer dann technischer Fallback, kein Horizontalzwang |
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
3. Bounded DTO-/Route-/Asset-Prototyp und native Auth-/Policy-Tests.
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

Plan-only werden geändert:

1. diese Datei als vollständige versionierte Planrevision;
2. `docs/ROADMAP.md` minimal auf `main @ 1f1755e…`, gemergtes #164 und den
   neuen #27-Plan-/Ownerstatus synchronisieren;
3. der PR-Body und genau ein aktueller `SESSION HANDOVER` als GitHub-
   Metadaten.

Nicht geändert werden in dieser Runde Produktcode, CMake, Lockfiles,
Configuration-/Auth-Schemas, API-Dokumente, Webassets, Tests, Workflows,
ESP-IDF-Konfiguration, Partitionen, Hardware, Ready-/Merge-/Issue-Close-
Status oder Aktorfreigaben.

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
Quellen noch nicht numerisch entschieden sind:

1. Ownerfreigabe dieser exakten Plan-SHA als Voraussetzung für jede
   Implementation.
2. Ownerentscheidung für `WEB_PASSWORD_MIN_LENGTH` und
   `WEB_PASSWORD_MAX_UTF8_BYTES`; bis dahin keine stillschweigende Wahl von
   `12`/`128` oder anderen Grenzen.
3. nach v6.1-KDF-Messung: finaler PBKDF2-/mbedTLS-Work-Factor und eventuelle
   dokumentierte Rest-Risiken, falls Plattformverschlüsselung weiterhin nicht
   aktiviert ist;
4. finale Messbestätigung der maximal vier parallelen Websessions, Asset-/DTO-
   Grenzen, Pollinglast und KDF-Laufzeit gegen die bestehende Ressourcen-
   baseline.

Diese Punkte sind keine Einladung zu Scope-Erweiterung. Ohne eine notwendige
Freigabe oder einen reproduzierbaren Nachweis bleibt der betroffene Teil
`NOT_RUN`/`BLOCKED`; es wird kein unsicherer Ersatz implementiert.

## 15. Planabschlussmarker

```text
ISSUE27_PLAN_REVISION=COMPLETE_FOR_OWNER_REVIEW
BASELINE_MAIN=1f1755e5e706fb668472920545b5302fcef1df16
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
PRODUCT_IMPLEMENTATION=NOT_STARTED
ACTUATOR_RELEASE=NO
OWNER_PLAN_APPROVAL_REQUIRED=YES
```
