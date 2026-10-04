# Issue #27 / PR #170 – Planrevision Auth-Provisionierungs-Produktpfad

```text
ISSUE=27
PR=170
PLAN_REVISION=AUTH_PROVISIONING_2026-10-04
CONTEXT_BASELINE_BRANCH=agent/issue-27-web-api-auth-main-restart
CONTEXT_BASELINE_SHA=926c3431cc07c79af5627482d3cb7840ba2622f6
CONTEXT_HEAD_SHA=15e4a3f17631a82425fb5281fe82ce36a611b438
PLAN_FIX_ROUND=1_AFTER_INDEPENDENT_PLAN_REVIEW
CONTEXT_PLAN_SHA=46e0ea470b307a34867e24337b63a9d38166d771
CONTEXT_REFRESH_MODE=FULL
BASE_MAIN=8a734f62836f8c57263c76ceebfc36a49f4af77c
PR_STATE=OPEN_DRAFT
PR170_MAIN_SYNC=PASS
INDEPENDENT_MAIN_SYNC_REVIEW=PASS
AUTH_PROVISIONING_PRODUCT_PATH=MISSING
AUTH_PROVISIONING_PLAN=READY_FOR_INDEPENDENT_REVIEW
AUTH_PROVISIONING_IMPLEMENTATION=NOT_STARTED
IDLE_1696B_OWNER_WAIVER=APPROVED_FOR_PR170_CONTINUATION
PRODUCT_CODE_CHANGE_THIS_STEP=NO
D1=OWNER_DECISION_REQUIRED_REVISED
D2=OWNER_DECISION_REQUIRED_REVIEWER_RECOMMENDS_A
D3=OWNER_DECISION_REQUIRED_REVIEWER_RECOMMENDS_APPROVE
D4=OWNER_DECISION_REQUIRED_NEW_SETUP_ORDER
AUTH_LIFETIME_CONCURRENCY_CONTRACT=DEFINED
SOFTAP_ONLY_SECURITY_ASSUMPTION=REMOVED
ESP_HTTP_SERVER_BINDING_FACT=INADDR_ANY_WITH_CURRENT_ADAPTER
OWNER_DECISION_REQUIRED=YES
ACTUATOR_RELEASE=NO
NEXT=STOP_FOR_INDEPENDENT_PLAN_REVIEW
```

Diese Revision ist die einzige kanonische Arbeitsgrundlage für den
Auth-Provisionierungs-Produktpfad von Issue #27. Sie autorisiert weder Produktcode
noch Tests, Flash oder Hardwarelauf. Die Umsetzung beginnt erst nach
Ownerfreigabe der exakten Plan-SHA und der Entscheidungen D1–D4 (Abschnitt 8).

## 1. Ziel, Owner-Waiver und Nicht-Ziele

Ziel: Ein Gerät im Zustand `WebAuthenticationState::Unprovisioned` kann auf der
realen Hardware legitim und ohne Testzugang eingerichtet werden. Der Benutzer
wählt genau zwischen

1. **Passwortschutz einrichten** (empfohlen, vorausgewählt) → `PasswordProtected`;
2. **Passwortschutz bewusst deaktivieren** (nur nach sichtbarer Warnung und
   ausdrücklicher Bestätigung) → `PasswordDisabled`.

Damit wird der Vier-Session-/no-PSRAM-Ressourcennachweis auf der realen Hardware
erst ausführbar (derzeit `FOUR_SESSION_NO_PSRAM_RESOURCE_GATE=BLOCKED_PRODUCT_STATE`).

**Owner-Waiver 1696 B** (Ownerentscheidung, hier nur dokumentiert):

```text
IDLE_1696B_FOLLOW_UP=OPEN
IDLE_1696B_OWNER_WAIVER=APPROVED_FOR_PR170_CONTINUATION
IDLE_1696B_ROOT_CAUSE=UNKNOWN
IDLE_1696B_CLOSED=NO
IDLE_1696B_BLOCKS_PR170_CONTINUATION=NO
IDLE_1696B_REOPEN_AS_BLOCKER_IF_REPRODUCED=YES
```

Evidenz laut `docs/audits/`: der nichtfatale `heap_alloc_failed size=1696
caps=0x1800` trat in den Kampagnen S6 (2×), Clean-Network-Switch (1×) und
Client-Load-Commit (1×) auf, also 4× in 3 Evidenzquellen; ein verlässlicher
Aufrufer wurde nicht identifiziert und es existiert keine belastbare
WLAN-/lwIP-Zuordnung. Mit PR #174 (ca. +16 KiB reale Heapreserve,
`CONFIG_LV_MEM_SIZE=49152`) trat er im fokussierten Hardwarelauf in 87,7 min
HOME_WIFI-Idle 0× auf (`docs/audits/R1_RAM_LVGL48_HW_EVIDENCE.md`). Daraus wird
keine Ursachenbehebung abgeleitet. Es gibt keine neue Diagnoseinstrumentierung
und keine WLAN-/lwIP-Ursachenbehauptung. Tritt der Befund im integrierten
Hardwaretest erneut auf, wird er wieder zum Blocker mit eigener Diagnose.

Nicht-Ziele (ausdrücklich nicht Teil dieser Revision): TLS/HTTPS, OIDC, Rollen-
oder Benutzerverwaltung, Passwort-Reset per E-Mail, persistente Browser-Tokens,
Remember-Me, neue Service-PIN-Architektur, produktive Run-Mutationsroute
(`/internal/ui/run` bleibt unregistriert), neue Webframeworks, neue
JSON-Bibliothek, WLAN-/lwIP-Tuning, weitere LVGL-/Stack-Optimierung, S9–S11,
nachträglicher Moduswechsel Passwortschutz an/aus (WEB_UI.md „Moduswechsel“) und
Passwortänderung über das Web. Diese Revision plant ausschließlich die
**Ersteinrichtung**.

## 2. Verifizierte Ausgangslage (Repository-Stand 926c343)

Kanonische Vorgaben:

- `docs/WEB_UI.md` („Betrieb ohne normales Webpasswort“): Bei der Ersteinrichtung
  ist Passwortschutz empfohlen und vorausgewählt; Deaktivierung nur bewusst nach
  Warnung. Normale Mutationen verlangen Session, CSRF, Origin-/Fetch-Metadata
  usw.; der Servicebereich bleibt immer zusätzlich durch die Service-PIN
  geschützt.
- `docs/NETWORK.md` („Inhalt des Heim-WLAN-Setup-Assistenten“, Schritt 7 und
  „Normaler Webzugang“): „normalen Webzugang mit Passwort aktivieren oder bewusst
  deaktivieren“ ist Teil des browserbasierten Setup-Assistenten; Webpasswort und
  Service-PIN sind getrennte Zugangsdaten; Deaktivieren warnt, dass jedes Gerät
  im erreichbaren lokalen Netz die normale Weboberfläche bedienen kann; das
  Deaktivieren des Webpassworts deaktiviert niemals den PIN-Schutz.
- ADR-009/ADR-010 und `docs/SETTINGS_AND_STORAGE.md`: Webpasswort und Service-PIN
  als gesalzene Prüfinformation; vergessene Service-PIN nur über lokalen
  vollständigen Werksreset.

Vorhandene Bausteine (alle wiederverwendet, keine Parallelmodelle):

| Baustein | Fundstelle | Relevanz |
|---|---|---|
| `AuthenticationDomain::bootstrap(context, surface, confirmed, password, servicePin)` | `authentication_records.cpp:734` | Root `Unprovisioned→Provisioning→Provisioned`, Credential-Write mit `writeInitialCredentials`; bei KDF-/Schreibfehler nach dem ersten Root-Write Root `RecoveryRequired` |
| `AuthenticationDomain::inspect/webPasswordEnabled` | `authentication_records.hpp:290,314` | Zustandsableitung in `webAuthenticationStateUnlocked()` |
| `AuthenticationRecordStore`, `AuthenticationCredentialRecord.webPasswordEnabled`, `AuthProvisioningRoot` | `authentication_records.hpp` | Persistenz und Epoch/Recovery-Semantik; Feld `webPasswordEnabled` existiert bereits im Schema |
| `IAuthenticationKdf` / `ISecureRandomSource` + ESP-IDF-Adapter | `authentication_records.hpp`, `device_platform_esp_idf` | KDF und Zufall |
| `WebAuthenticationState` (`Unprovisioned`, `RecoveryRequired`, …) | `fermentation_application.hpp:76` | bereits getrennte Zustände |
| `WebRouteDispatcher` (`handleLogin` Prelude: POST, `validWebMetadata`, exakter JSON-Content-Type, `sameOrigin`, begrenzter cJSON-Decoder) | `web_application_routes.cpp:608ff` | Vorlage für den Provisionierungs-Handler |
| `WebSessionManager::create/revokeAll`, `MAX_WEB_SESSIONS=4`, Replay Candidate C | `web_session.hpp` | unverändert |
| `NetworkSetupRoutes` und Setup-Flow (`networkSetupFlowActive()`, `beginHomeWifiReconfiguration()`) | `network_setup_routes.cpp`, `fermentation_application.hpp` | bestehende Setup-Weboberfläche |
| `ConfigurationRecoveryService::resolveAuthenticationBootstrap`, Werksreset → neuer Auth-Kontext | `fermentation_application.cpp:1410ff` | Factory Reset führt bereits zu sauberem `Unprovisioned` (B3 geschlossen) |

Verifizierte **Lücken** (Ursache dafür, dass kein Produktpfad existiert):

- **G1 – Surface-Regel.** `bootstrap` liefert `InvalidInput`, wenn `surface !=
  UiSurface::LocalDisplay`; `test_authentication_records` prüft genau das
  (`WebInterface` → `InvalidInput`). Die Ersteinrichtung ist damit domänenseitig
  *nur lokal* vorgesehen, während `NETWORK.md` den Webpasswort-Schritt im
  *Browser-Assistenten* verortet. Die deferierte Touch-Tastatur
  (Ownerentscheidung `VARIANT_B_QR_RETAINED`) macht eine lokale Texteingabe in R1
  unmöglich. Der einzige heutige Aufrufer mit `LocalDisplay` ist der
  Testhelper `provision()`; ein produktiver Weg darf diesen Trick **nicht**
  kopieren (versteckte Backdoor).
- **G2 – Service-PIN ist Pflichtparameter.** `bootstrap` validiert und
  verschlüsselt (KDF) immer Webpasswort *und* vierstellige Service-PIN. Es gibt
  heute keine lokale Eingabe für die Service-PIN.
- **G3 – Deaktiviert hat keinen Domänenpfad.** `bootstrap` schreibt immer
  `webPasswordEnabled=true`; keine Domänenfunktion setzt `false`. Nur der Testhelper
  `setWebPasswordEnabled` schreibt das Feld direkt in den Store.
- **G4 – Erreichbarkeit/Takeover (verifiziert).**
  `EspIdfHttpServerLifecycle::start()` nutzt `HTTPD_DEFAULT_CONFIG()` und setzt
  `config.if_name` nicht (`esp_idf_http_server_lifecycle.cpp:190`). Mit
  `if_name == NULL` bindet der ESP-IDF-6.1-HTTP-Server an `INADDR_ANY`, also an
  alle verfügbaren Interfaces; der Netzwerkadapter nutzt im Kandidatentest
  `WIFI_MODE_APSTA` (`esp_idf_network_lifecycle.cpp:343,469`). Die
  Erreichbarkeit der Route über SoftAP **oder** Heimnetz ist daher *keine*
  Sicherheitsgrenze. Zusätzlich beendet `NetworkConfigurationService::
  testCandidate()` bei Erfolg den Kandidaten-Test *und* persistiert, startet
  `HOME_WIFI` und setzt `setupFlowActive_=false`
  (`network_configuration_service.cpp:202–265`). Der Setup-Flow endet damit
  vor jedem möglichen Webzugangsschritt; die in `NETWORK.md` beschriebene
  Reihenfolge „WLAN testen → Webzugang einrichten → Zusammenfassung/speichern“
  ist mit dem gemergten Stand (PR #171) nicht darstellbar (siehe D4). Während
  `networkSetupFlowActive()` reicht der Dispatcher nur `/` und die drei
  Netzwerkpfade durch.
- **G5 – Login im Zustand `Unprovisioned`** antwortet bereits korrekt `503
  provisioning-required`; die Shell zeigt dann nur Text („Lokale Provisionierung
  oder Recovery erforderlich“), aber keine Aktion.

## 3. Lösungsentwurf (kleinste sinnvolle R1-Lösung)

### 3.1 Entry Point

Kein neuer Webapp-Stack. Es werden genau **eine neue Route** und **ein kleiner
statischer Formularabschnitt** ergänzt:

- Route `POST /api/v1/provision` im vorhandenen `WebRouteDispatcher`
  (`handleProvision`, Schwester von `handleLogin`).
- Statischer, begrenzter Formularabschnitt „Webzugang einrichten“ (zwei
  Optionen, Warnung, Bestätigung, Service-PIN-Feld) in der normalen Shell im
  Zustand `unprovisioned`; DE/EN/ES über das bestehende Übersetzungsverfahren
  der Shell. Die Setup-Seite von `NetworkSetupRoutes` bleibt unverändert (D4,
  Option 1).
- Eine lokale Freigabeaktion am Touchdisplay (3.2).

### 3.2 Zugriffsbindung: lokal freigegebenes, begrenztes Provisionierungsfenster (Entscheidung D1)

Die frühere Bindung an `networkSetupFlowActive() || AP_ONLY` und jede
Interface-Annahme (SoftAP-only) sind **entfernt**. Sicherheitsgrundlage ist
ausschließlich eine lokale, physische Freigabe am Touchdisplay.

**Prüfung der vorhandenen Freigabeereignisse (Ergebnis: nicht ausreichend).**
Als Freigabe kämen `beginHomeWifiReconfiguration()` und die initiale lokale
Moduswahl (`ApplyNetworkModeApOnly/HomeWifi`) in Frage. Sie genügen nicht:
(a) Das Bench-Gerät steht bereits in `HOME_WIFI` + `Unprovisioned`; jede
Freigabe müsste eine unnötige WLAN-Neueinrichtung erzwingen. (b) Ein bereits
konfiguriertes `AP_ONLY`-Gerät hat kein Wiedereintrittsereignis außer einem
Moduswechsel. (c) Ein Moduswechsel hat andere Absicht; die Freigabe wäre ein
Nebeneffekt, nicht eine bewusste Entscheidung, Webzugriff auf die
Ersteinrichtung zu erlauben. Daher wird **genau eine** kleine, explizite
Touchaktion geplant:

| Aspekt | Festlegung |
|---|---|
| Aktion | neues Slot-Action `OpenWebProvisioningWindow` (Command `FermentationUiOpenWebProvisioningWindowCommand`, analog zu `BeginHomeWifiReconfiguration`) |
| Ort | neue Header-Seite `FermentationUiPage::HeaderWebAccess` (Slot 0 `back`, Slot 1 `web-access-open`). `HeaderNetwork` hat 4 von 4 Slots belegt und bleibt unverändert; der Einstieg ist ein bisher freier Slot 3 auf `HeaderLanguage` (`web-access`). Die Platzierung ist ein reines UI-Detail und wird mit D1 bestätigt |
| Sichtbarkeit/Aktivierung | Slot ist nur aktiv bei `Unprovisioned`, `inspect()==BootstrapAllowed` und geschlossenem Fenster; sonst deaktiviert mit `blockedReason` |
| Zeitbegrenzung | fest **10 Minuten** (600 000 ms, monotone Uhrzeit `ITimeSource`), **keine** Verlängerung; erneutes Drücken bei offenem Fenster ist ein No-op |
| Auswertung | lazy: `webProvisioningWindowOpen(now)` vergleicht beim Zugriff; kein Timer |
| Verbrauch | sofort bei erfolgreicher Provisionierung; zusätzlich geschlossen bei Ablauf, Werksreset, Neustart, Netzwerkmoduswechsel (`applyNetworkMode`), `beginHomeWifiReconfiguration()` und sobald der Auth-Zustand nicht mehr `Unprovisioned`/`BootstrapAllowed` ist |
| Candidate-Test-Erfolg | schließt das Fenster **nicht** (sonst wäre D4 Option 1 unmöglich) |
| Persistenz | rein flüchtig: ein `bool`/Deadline-Paar im Application-Owner, keine Persistenz, kein neuer Speicher |
| Anzeige | secret-freies Snapshot-Feld `webAccess ∈ {NotApplicable, Closed, WindowOpen}` im UI-Snapshot; Text DE/EN/ES über das bestehende Textpack |

Eindeutige Prüfreihenfolge der Route (unter L1, vor jedem Domain-Aufruf; die
erste zutreffende Zeile entscheidet):

```text
1. state == PasswordProtected || state == PasswordDisabled
       -> 409 already-provisioned
2. state == RecoveryRequired  || state == Indeterminate
       -> 503 recovery-required
3. state != Unprovisioned                      (jeder andere Wert)
       -> fail closed: 503 recovery-required
4. inspect() != BootstrapAllowed               (Zustand Unprovisioned)
       -> fail closed: 503 recovery-required, kein bootstrap-Aufruf
5. lokales Freigabefenster nicht offen
       -> 403 provisioning-not-allowed
6. sonst -> Provisionierung versuchen (tryBegin, bootstrap, ...)
```

Damit gilt konsistent und ohne Überschneidung:

```text
REPEAT_AFTER_SUCCESS=409_ALREADY_PROVISIONED
NO_LOCAL_WINDOW=403_PROVISIONING_NOT_ALLOWED
RECOVERY_STATE=503_RECOVERY_REQUIRED
```

Ein bereits provisioniertes Gerät antwortet immer `409`, unabhängig vom
Fensterzustand; `403` gilt nur im Zustand `Unprovisioned` ohne offenes Fenster.
`RecoveryRequired` und `Indeterminate` bleiben ausgeschlossen. Ohne lokale
Freigabe gibt es kein „first come, first served“, es gibt keine Touch-Tastatur und
keine neue Pairing-/Token-Domäne.

**Restrisiko (offen benannt, Entscheidung D1):** Innerhalb des 10-Minuten-
Fensters kann jeder Client, der den HTTP-Server erreicht (SoftAP oder
Heimnetz, `INADDR_ANY`), die Provisionierung abschließen. Das Fenster begrenzt
Zeit und Anlass (bewusste lokale Aktion, wird beim ersten Erfolg verbraucht), es
bindet sie aber nicht an eine Person. Eine personengebundene Lösung (Einmalcode
am Display) wäre eine neue Token-/Pairing-Domäne und ist laut Auftrag nicht
Teil von R1.

### 3.3 HTTP-Vertrag `POST /api/v1/provision`

| Aspekt | Festlegung |
|---|---|
| Methode | nur `POST`; sonst `405 method-not-allowed` |
| Metadaten | `validWebMetadata` und `web_browser_policy::sameOrigin` unverändert wie Login (Origin hat Vorrang, Referer nur als Fallback bei fehlendem Origin, `Sec-Fetch-Site` ∈ {same-origin, same-site, none}) |
| Content-Type | `application/json` (optional `charset=utf-8`) über `exactJsonContentType`; sonst `415` |
| Max. Body | `kMaximumWebProvisionBodyBytes = 1024` (Konstante in `web_json_codec.hpp` neben `kMaximumWebLoginBodyBytes=768`); größer → `413` |
| Request-DTO | JSON-Objekt, genau diese Schlüssel (`hasOnlyKeys`, Duplikate/NUL wie Login abgelehnt): `mode` (`"protect"` \| `"disable"`, Pflicht), `servicePin` (Pflicht, genau 4 ASCII-Ziffern), `password` (Pflicht bei `protect`, **verboten** bei `disable`; Länge nach `validateWebPassword`, max. 256 Byte im Decoder), `confirmDisable` (Pflicht `true` bei `disable`, **verboten** bei `protect`) |
| Erfolg | `200` `{"provisioned":true,"passwordProtection":"enabled"\|"disabled"}`; **keine** Session, **kein** Cookie, **kein** CSRF-Token |
| Fehler | `400 invalid-json` (Schema/Modus/Felder), `413 request-too-large`, `415`, `403 origin-rejected`, `403 provisioning-not-allowed` (nur Zustand `Unprovisioned`, `BootstrapAllowed`, aber kein offenes lokales Freigabefenster), `409 already-provisioned` (Zustand `PasswordProtected`/`PasswordDisabled`, auch ohne Fenster; ebenso konkurrierender Zweitrequest nach Erfolg), `422 invalid-credentials` (Passwort-/PIN-Regelverletzung), `503 recovery-required` (`RecoveryRequired`, `Indeterminate`, jeder sonstige Zustand, `inspect()!=BootstrapAllowed` im Zustand `Unprovisioned`, `CommitOutcomeUnknown`), `503 provisioning-failed` (KDF- oder Persistenzfehler) |
| Antworten | niemals Passwort, PIN oder Verifier; Passwort nie in URL/Log/Snapshot |

Es gibt vor der Provisionierung **keine** Session, daher weder CSRF-Token noch
Replay-Schutz; der Schutz entspricht dem bereits akzeptierten Login-Vertrag
(JSON-only-Content-Type erzwingt Preflight, den der Server nie beantwortet,
zusätzlich Origin-/Fetch-Metadata-Prüfung, `SameSite=Strict` ist hier mangels
Cookie nicht relevant). Das wird ausdrücklich als Restrisiko im
Sicherheitsabschnitt geführt.

Nach Erfolg: `WebSessionManager::revokeAll()` (defensiv; vor Provisionierung
existieren keine gültigen Sessions), danach neue Session ausschließlich über den
normalen Login (`PasswordProtected`) bzw. den bestehenden anonymen Disabled-Pfad
(`PasswordDisabled`). `MAX_WEB_SESSIONS=4` und Replay Candidate C bleiben
unverändert.

### 3.4 Anwendungsschicht und Auth-Operations-/Lifetime-Vertrag

**Befund (verifiziert).** `authenticationDomain_` und `authenticationRecordStore_`
sind `unique_ptr`. `beginAuthorizedFactoryReset()` läuft unter dem
Application-Serializer und ruft nach erfolgreichem Reset
`resetAuthenticationState()` auf (`fermentation_application.cpp:1681ff`, `1707`),
das Domain und RecordStore zerstört. Der bestehende `authenticateWebPassword()`
kopiert einen rohen `AuthenticationDomain*`, gibt den Serializer frei und läuft
danach mehrere Sekunden in PBKDF2 (`fermentation_application.cpp:1186ff`).
Wird dazwischen ein Werksreset ausgeführt, entstehen Use-after-free und
Epoch-Races (ein laufendes `verifyWebPassword` schreibt Lockout-Zähler, ein
laufendes `bootstrap` Credential-Records in einen bereits zurückgesetzten
Speicher). Dasselbe Muster darf `provisionWebAccess()` weder duplizieren noch
vergrößern; **beide Pfade und Reset/Reinitialisierung nutzen daher einen
gemeinsamen, schmalen Vertrag.**

**Vertrag: `AuthOperationGate` (application-owned, privat, kein neuer Port).**
Ein kleiner Member im Application-Owner (Mutex, Condition-Variable, Zähler,
Flag; kein Heap):

```text
mutex m;  condition_variable cv;  unsigned active = 0;  bool closed = false;

tryBegin()      : lock m; if (closed) return false; ++active; return true
end()           : lock m; if (--active == 0) cv.notify_all()
closeAndDrain() : lock m; closed = true; cv.wait(active == 0)
reopen()        : lock m; closed = false
```

Lock-Reihenfolge (einzige zulässige): **Application-Serializer (L1) → Gate-Mutex
`m` (L2)**. `m` wird nur kurz gehalten (nie während PBKDF2, nie während einer
Domain-/Store-Operation) und nie umgekehrt (L2 → L1) genommen.

Auth-Operation (Login-Verifikation oder Provisionierung):

```text
AUTH_DOMAIN_RAW_HANDLE_VALIDITY=ONLY_WHILE_AUTH_OPERATION_TOKEN_ACTIVE
AUTH_CONTEXT_COPY_VALIDITY_FOR_DOMAIN_ACCESS=ONLY_WHILE_AUTH_OPERATION_TOKEN_ACTIVE
USE_COPIED_DOMAIN_AFTER_END=FORBIDDEN
```

1. Unter L1: Zustand/Gate prüfen (Prüfreihenfolge 3.2), `domain` und `context`
   kopieren, `tryBegin()` (liefert `false` ⇒ Operation startet nicht, Ergebnis
   fail-closed `RecoveryRequired`). Danach L1 **freigeben**. Ab hier gilt der
   Auth-Operation-Token als aktiv.
2. Ohne irgendeine Sperre, **solange der Token aktiv ist**: ausschließlich hier
   darf der kopierte `domain`-Zeiger (und `context` für Domain-Zugriffe)
   dereferenziert werden: `domain->verifyWebPassword(...)` bzw.
   `domain->bootstrap(...)` (PBKDF2). Benötigt das Ergebnis eine Re-Inspektion
   (Provisionierung: `bootstrap` lieferte `RecoveryRequired`, um einen
   Retry/konkurrierenden Request nach Erfolg von echtem Recovery zu
   unterscheiden), erfolgt `domain->inspect(*context)` **noch vor `end()`**; das
   Ergebnis wird als lokaler Wert kopiert. L1 wird vor `end()` **nie** erneut
   genommen (ein bereits in `closeAndDrain()` wartender Reset hält L1 und würde
   sonst einen Lock-Zyklus bilden).
3. Eine RAII-Wache ruft `end()` in jedem Ausgang (auch Frühausstieg). Direkt
   danach werden `domain` und `context` nicht mehr verwendet; ein Reset darf
   unmittelbar nach der Rückkehr der Domain und vor jeder weiteren
   Ergebnisabbildung fortfahren.
4. Nach `end()`: nur noch lokale Ergebniswerte. Seiteneffekte, die
   Application-Zustand brauchen (Sessions widerrufen, Fenster verbrauchen,
   Statusabbildung), nehmen L1 **neu** und halten dabei `m` nicht; sie
   dereferenzieren keinen alten Domain-/Store-Zeiger. Dazwischen eingetretene
   Resets sind harmlos: sie haben das Fenster bereits geschlossen und alle
   Sessions widerrufen; ein Verbrauch/Widerruf auf dem neuen Zustand ist
   idempotent.

Reset/Reinitialisierung (Factory Reset, `initializeAuthentication`,
`resetAuthenticationState`):

- `beginAuthorizedFactoryReset()` ruft **vor** dem zerstörenden
  `configurationRecoveryService_->beginAuthorizedFactoryReset()`
  `closeAndDrain()` auf (unter L1). Neue Auth-Operationen starten nicht mehr
  (`tryBegin()==false`); laufende werden abgewartet. Erst danach wird der Store
  verändert. Kein laufender `bootstrap`/`verify` kann damit in den
  zurückgesetzten Speicher schreiben.
- Wartet der Reset, hält er L1; die laufenden Operationen **brauchen L1 nicht**
  und `m` nur in `end()`. Es gibt keine Zyklusabhängigkeit, also keinen
  Deadlock. Der Preis ist, dass lokale UI-Aufrufe höchstens eine PBKDF2-Dauer
  warten, wenn ein Werksreset mit einer laufenden Web-Authentifizierung
  zusammenfällt (seltener, lokaler, autorisierter Fall).
- Schlägt der Reset fehl (nicht `FactoryResetCompleted`), ruft die Methode
  `reopen()` auf; Domain und Kontext bleiben unverändert gültig.
- Nach erfolgreichem Reset zerstört `resetAuthenticationState()` Domain und
  Store (jetzt ohne laufende Operation), `initializeAuthentication()` baut sie
  neu und ruft am Ende `reopen()` auf (auch bei fehlgeschlagener Auflösung; die
  Operationen scheitern dann ohnehin am fehlenden Domain-Zeiger). Alle Aufrufer
  von `resetAuthenticationState()`/`initializeAuthentication()`
  (`begin`-Pfade, Reset, `1783`) werden in Schnitt S2 auditiert; Aufrufe vor
  Start des HTTP-Servers (Boot) sind trivial, da keine Operation läuft.
- Die Anwendung wird erst nach Stopp des HTTP-Servers zerstört; der Destruktor
  ruft `closeAndDrain()`, bevor Domain und Store zerstört werden.

Verworfene Alternative: `shared_ptr` auf ein Bundle aus Store+Domain. Das
verhindert den Use-after-free, aber nicht stale-epoch-Schreibzugriffe einer
laufenden Operation in den zurückgesetzten Speicher; der Drain ist die
vollständigere und kleinere Lösung.

**Neue API (nur Ersteinrichtung):**

```cpp
enum class WebProvisionMode : std::uint8_t { Protect, Disable };
enum class WebProvisionStatus : std::uint8_t {
    Provisioned, NotAllowed, AlreadyProvisioned, InvalidCredentials,
    RecoveryRequired, Failed
};
WebProvisionStatus FermentationApplication::provisionWebAccess(
    WebProvisionMode mode, const std::string& webPassword,
    const std::string& servicePin);
[[nodiscard]] bool FermentationApplication::openWebProvisioningWindow();
```

`provisionWebAccess` folgt dem Vertrag oben: Prüfreihenfolge unter L1 (3.2:
bereits provisioniert → `AlreadyProvisioned`; Recovery/sonstiger Zustand/
`inspect()!=BootstrapAllowed` → `RecoveryRequired`; Fenster geschlossen →
`NotAllowed`), dann `tryBegin()` und L1 freigeben; `bootstrap` innerhalb des
Tokens. Antwortet `bootstrap` mit `RecoveryRequired`, wird noch innerhalb des
Tokens `domain->inspect(*context)` ausgewertet: `AlreadyProvisioned` (Retry/
konkurrierender Request nach Erfolg) ergibt den lokalen Status
`AlreadyProvisioned` (HTTP 409), alles andere `RecoveryRequired`. Nach `end()`
wird unter L1 bei Erfolg das Fenster verbraucht und
`webSessionManager_->revokeAll()` aufgerufen. `openWebProvisioningWindow()`
setzt unter L1 Deadline = jetzt + 600 000 ms, wenn die Vorbedingungen aus 3.2
gelten.

### 3.5 Auth-Domain – genau eine kleine, begründete Erweiterung (Entscheidung D3)

`bootstrap` bleibt die einzige Provisionierungsfunktion. Unvermeidlich sind zwei
Verhaltensänderungen **in dieser einen Funktion** (nicht zwei neue Funktionen,
keine neue Abstraktion):

1. **Surface-Regel (G1).** `UiSurface::WebInterface` wird nicht mehr pauschal mit
   `InvalidInput` abgelehnt. Der Domain-Aufruf bleibt `confirmed`-pflichtig. Die
   *Autorisierung* (physische Anwesenheit/Setup-Kontext) liegt im
   Application-Gate `webProvisioningAllowed` (3.2), nicht in der Domain. Der
   bestehende Domaintest „WebInterface → InvalidInput“ wird bewusst ersetzt:
   künftig `WebInterface` ohne `confirmed` → `InvalidInput`, mit `confirmed` →
   zulässig. Das ist eine **bewusste Sicherheitsvertragsänderung** und wird vom
   Owner freigegeben (D3).
2. **Modus (G3).** Neuer Parameter `WebPasswordMode mode` (`Protected` default,
   `Disabled`). Bei `Disabled` wird `webPasswordEnabled=false` geschrieben und
   `password` muss leer sein. Das Feld `webPassword` im Record bleibt schema-
   konform (`isPlausible` verlangt gültige Algorithmus-/Workfactor-Felder): Salt
   und Verifier werden aus dem vorhandenen `ISecureRandomSource` erzeugt und
   **nie** bekanntgegeben oder verglichen, weil `verifyWebPassword` bei
   `webPasswordEnabled=false` vor jedem Vergleich `Disabled` liefert. Das spart
   einen PBKDF2-Lauf (nur die Service-PIN wird abgeleitet). Die Aktivierung
   später (Disabled→Enabled) ist eine eigene geschützte Mutation außerhalb dieser
   Revision und überschreibt den Zufallsverifier atomar mit einem echten.

Keine neue Persistenz, kein Schemawechsel (`webPasswordEnabled` ist bereits Teil
des Credential-Records), keine neue Domain-Klasse.

### 3.6 Service-PIN (Entscheidung D2)

Da `bootstrap` die Service-PIN zwingend verlangt und es in R1 keine lokale
Eingabe gibt, enthält der Provisionierungsdialog ein **eigenes, getrenntes Feld**
„Service-PIN festlegen“ (vier Ziffern, verdeckt, mit Hinweis ADR-010: bei
Vergessen nur lokaler Werksreset). Die PIN ist getrennt vom Webpasswort (wird
nicht daraus abgeleitet und kann nicht gleich dem Passwort sein müssen), wird
nur über dieselbe KDF abgelegt und auch im Modus `disable` verlangt (der
PIN-Schutz wird niemals deaktiviert). Der Service-*Bereich* selbst bleibt
Gegenstand späterer Service-Gates.

## 4. Sicherheitsvertrag

- Passwort/PIN nie in URL, Logs, Diagnose, Snapshots, Fehlertexten, Antworten.
- Kein Klartext-Persistieren; bestehende KDF (`IAuthenticationKdf`,
  PBKDF2-HMAC-SHA256, `workFactor=10000`) und Salt-/Random-Vertrag unverändert.
- Fail-closed: KDF-, Random-, Schreib- oder Readback-Fehler führen zu keiner
  Freigabe. Nach dem ersten Root-Write ist ein KDF-/Credential-Fehler die
  **bewusst fail-closed** Domainfolge „Root `RecoveryRequired`“; der einzige
  Rückweg ist der bestehende lokale Werksreset. Das ist bestehendes Verhalten und
  wird nicht aufgeweicht. Fehler *vor* dem ersten Root-Write (Eingabefehler,
  Gate, Kontext) lassen den Zustand `Unprovisioned` unverändert und sind
  wiederholbar.
- Keine Provisionierung aus `RecoveryRequired`/`Indeterminate`; beide Zustände
  bleiben von `Unprovisioned` getrennt (`inspect()` ≠ `BootstrapAllowed` → kein
  `bootstrap`-Aufruf).
- Wiederholte Provisionierung nach Erfolg ist nicht möglich (`409`).
- Deaktivierung nur mit `confirmDisable:true` plus sichtbarer Warnung;
  Standardvorauswahl im UI ist „Passwortschutz einrichten“. Warntext gemäß
  `NETWORK.md`.
- Keine Test-/Service-Backdoor: kein Debug-Parameter, keine Umgehung des Gates,
  kein `LocalDisplay`-Vortäuschen im Produktcode.
- **Restrisiken (offen benannt):** (a) HTTP im lokalen Netz ohne TLS (akzeptiert
  durch `NETWORK.md`/ADR); Passwort und PIN reisen im Klartext über das
  jeweilige WLAN. (b) Ohne Session kein CSRF-Token; Schutz über Origin-/
  Fetch-Metadata und JSON-only. (c) Innerhalb des 10-Minuten-Fensters gewinnt
  der erste Client, der das Gate erreicht (3.2); der zweite erhält `409`.
  (d) Der HTTP-Server ist an `INADDR_ANY` gebunden; es wird keine
  Interface-Bindung als Sicherheitsannahme verwendet.

## 5. RAM- und Stack-Bilanz (vor Implementierung)

Basis: PR-#174-Referenz `CONFIG_LV_MEM_SIZE=49152`; keine prophylaktische
Optimierung.

```text
NEW_LONG_LIVED_DYNAMIC_BUFFERS=NO
NEW_SESSION_CAPACITY=NO
REPLAY_MODEL_CHANGE=NO
LVGL_POOL_CHANGE=NO
STACK_CHANGE=NO
```

- Dauerhaftes zusätzliches RAM: kleine statische Zustände im Application-Owner (`AuthOperationGate`: Mutex, Condition-Variable, Zähler, Flag; Fenster: Flag und 64-Bit-Deadline; Snapshot-Feld `webAccess`), insgesamt im Bereich weniger Dutzend Bytes plus ein Mutex-/CV-Objekt, kein Heap. Im Implementierungsschnitt per `sizeof` zu belegen. Flash-Konstante für das
  statische Formularfragment (geschätzt unter 3 KiB, im Implementierungsschnitt
  per Buildreport zu belegen, kein Laufzeit-RAM).
- Request-lokales RAM: ein Body-`std::string` ≤ 1024 B, DTO mit zwei kurzen
  Strings (Passwort ≤ 256 B, PIN 4 B), PBKDF2-Zwischenpuffer wie im Login. Alles
  frei nach Rückkehr des Handlers.
- Stack: derselbe HTTP-Handler-Pfad wie `handleLogin`/`verifyWebPassword`
  (PSA-PBKDF2). `bootstrap` ruft höchstens zwei Ableitungen sequenziell (nicht
  geschachtelt) auf. Eine zusätzliche Stackmessung auf der Hardware ist Teil des
  späteren Hardwarenachweises, keine Stackänderung.
- Latenz: `protect` ≈ zwei PBKDF2-Läufe, `disable` ≈ ein Lauf. Die gemessene
  Dauer wird im Hardwarenachweis protokolliert; ein Überschreiten von HTTP-/
  Watchdog-Zeitbudgets wäre ein neuer Befund und kein stiller Fix.

## 6. Umsetzungs- und Commit-Schnitte

Nach **jedem** Schnitt wird angehalten und die Ownerfreigabe abgewartet
(Arbeitsregel des Owners). Gezielte Tests laufen pro Schnitt, der vollständige
Pre-Ready-Lauf erst nach Independent Review und Ownerfreigabe.

1. **S1 – Domain.** `bootstrap` um `WebPasswordMode` erweitern, Surface-Regel
   gemäß 3.5, `Disabled`-Record mit Zufallsverifier; vorhandene Tests
   angepasst, neue Domaintests. Dateien: `authentication_records.hpp/.cpp`,
   `test/test_authentication_records`.
2. **S2 – Auth-Lifetime-Vertrag (verhaltensneutral).** `AuthOperationGate`,
   Umbau von `authenticateWebPassword()`, Drain in
   `beginAuthorizedFactoryReset()`/`initializeAuthentication()`/Destruktor,
   Audit aller Aufrufer von `resetAuthenticationState()`; die Concurrency-Tests
   aus Abschnitt 7 für den Login-Pfad. Dateien: `fermentation_application.hpp/.cpp`,
   Tests.
3. **S3 – Provisionierung und Fenster.** `provisionWebAccess`,
   `openWebProvisioningWindow`, Fensterzustand inkl. Schließen bei Reset,
   Moduswechsel, Reconfiguration, Ablauf, Verbrauch; Concurrency-Test für die
   Provisionierung.
4. **S4 – HTTP/Codec.** `decodeWebProvision`, `kMaximumWebProvisionBodyBytes`,
   `handleProvision`, Dispatcher-Route (nicht im Setup-Flow, siehe D4). Dateien:
   `web_json_codec.hpp/.cpp`, `web_application_routes.hpp/.cpp`, Tests.
5. **S5 – Lokaler Touchpfad.** Seite `HeaderWebAccess`, Slot-Action
   `OpenWebProvisioningWindow`, Command + Bridge, Press-Dispatcher, Snapshot-Feld
   `webAccess`, Textpack-Schlüssel DE/EN/ES. Dateien:
   `fermentation_touch_workspace.*`, `fermentation_ui_commands.*`,
   `fermentation_ui_projector.*`, `main/fermentation_ui_press_dispatcher.*`,
   Tests (`test_local_touch_ui`, `test_press_dispatcher`,
   `test_ui_steady_state_allocations`, `test_renderer_boundary`).
6. **S6 – Webformular.** Statisches Formular in der Shell im Zustand
   `unprovisioned` (zwei Optionen, Vorauswahl „Passwortschutz“, Warnung,
   Bestätigung, PIN-Feld, Hinweis „am Gerät freigeben“ bei `403`; DE/EN/ES);
   Größenschranke geprüft, nicht erhöht.
7. **S7 – Dokumentation.** `docs/WEB_UI.md` (Ersteinrichtung), `docs/NETWORK.md`
   (Schritt 7 gemäß D4), `docs/ROADMAP.md` minimal; keine neuen ADRs (D3).

Zielstruktur: `device_platform` unverändert; `fermentation_app` nutzt nur die
vorhandenen abstrakten Ports; keine neue Bibliothek, kein neuer Komponentenstack
(ADR-013 bleibt unberührt).

## 7. Tests

**Native (pro Schnitt, plus direkt betroffene Konsumenten):**

- `Unprovisioned → PasswordProtected` (mode `protect`) und anschließender normaler
  Login.
- `Unprovisioned → PasswordDisabled` (mode `disable` mit `confirmDisable`) und
  anschließender anonymer Disabled-Sessionpfad; Service-PIN bleibt gesetzt.
- Ungültige Anfragen fail-closed ohne Persistenzänderung: falsche Methode,
  Content-Type, Origin, fremde/doppelte/fehlende Schlüssel, zu großer Body,
  unzulässige Passwort-/PIN-Werte, `disable` ohne Bestätigung, `protect` mit
  `confirmDisable`, `disable` mit `password`.
- Zustandsmatrix der Route (Prüfreihenfolge 3.2), jeweils mit und ohne offenes
  Fenster: `PasswordProtected`/`PasswordDisabled` → `409` (kein Bootstrap-Aufruf);
  `RecoveryRequired`/`Indeterminate`/`Unprovisioned` mit
  `inspect()!=BootstrapAllowed` → `503 recovery-required` (kein Bootstrap-Aufruf);
  `Unprovisioned`+`BootstrapAllowed` ohne Fenster → `403`; mit Fenster →
  Versuch.
- Gate: ohne offenes Fenster (Zustand `Unprovisioned`) → `403 provisioning-not-allowed`; Fenster öffnet nur
  bei `Unprovisioned`+`BootstrapAllowed`; Ablauf nach 10 min (virtuelle Uhr);
  Verbrauch bei Erfolg; Schließen bei Werksreset, Moduswechsel,
  `beginHomeWifiReconfiguration`; Candidate-Test-Erfolg schließt es nicht;
  erneutes Öffnen bei offenem Fenster ist No-op; kein Öffnen in
  `RecoveryRequired`/`Indeterminate`/`AlreadyProvisioned`.
- **Concurrency (nativ, `std::thread`, blockierbare Fake-KDF):**
  (1) Eine Fake-KDF hält Login-Verifikation *beziehungsweise* Provisionierung in
  der langsamen Auth-Operation (signalisiert „drin“, wartet auf Freigabe).
  (2) Parallel wird `beginAuthorizedFactoryReset()` ausgelöst; der Test belegt,
  dass der Reset wartet (kein Fortschritt, solange die KDF blockiert) und den
  Store erst nach der Freigabe verändert. (3) Nach Freigabe: kein Absturz/UAF,
  kein Deadlock (alle Threads enden innerhalb eines Zeitlimits), die
  Auth-Operation endet vollständig gegen die alte Epoch, der Reset danach
  erfolgreich; der Endzustand ist `Unprovisioned` der neuen Epoch, keine alten
  Credential-Records lesbar, alle Sessions widerrufen, Fenster geschlossen.
  (4) Eine nach `closeAndDrain()` gestartete Operation ruft die KDF nicht auf und
  ist fail-closed. (5) Schlägt der Reset fehl, öffnet `reopen()` das Gate und die
  Domain bleibt nutzbar. (6) Der Reset darf unmittelbar nach der Rückkehr der
  Domain-Operation und vor der HTTP-Ergebnisabbildung fortfahren (Test-Hook
  zwischen `end()` und Statusabbildung): die Abbildung verwendet nur lokale
  Ergebniswerte, greift auf keinen alten Domain-/Store-Zeiger zu und erzeugt
  weder UAF, Deadlock noch stale-epoch-Write; für den Re-Inspektionsfall
  (konkurrierende Provisionierung) wird belegt, dass `inspect()` vor `end()`
  stattfindet. Falls die Native-Testumgebung Threads nicht linkt, ist
  das ein Build-Konfigurationsbefund und wird dem Owner vor der Änderung
  vorgelegt.
- Persistenzfehler (Root-Write, Credential-Write, Readback, `CommitOutcomeUnknown`)
  → kein Erfolg, Zustand `RecoveryRequired`, keine Session; Fehler vor dem
  ersten Write → weiterhin `Unprovisioned`.
- KDF-/Random-Fehler fail-closed.
- Wiederholung nach Erfolg → `409` (Zustandsprüfung, unabhängig vom Fenster);
  konkurrierender zweiter Request, der das Gate noch im Zustand `Unprovisioned`
  passiert hat → `409` über die Re-Inspektion innerhalb des Tokens (nicht
  „recovery“).
- `RecoveryRequired`/`Indeterminate` nicht über Provisionierung umgehbar.
- Factory Reset → wieder `Unprovisioned` und erneut provisionierbar.
- Keine Regression: `test_web_session`, Replay Candidate C (`4×8`), 4-Session-Limit,
  Read-only-API, Login/Logout, Browser-Policy, HTTP-Lifecycle,
  Netzwerk-/Setup-Tests, Architekturguard, `/internal/ui/run` bleibt unerreichbar.
- Secret-Scanner (`scripts/check_secrets.py`, **ohne Scanneränderung**): Fixtures
  und Produktcode vermeiden direkte Zuweisungen der Muster
  `<password|token|secret|…> = "literal"` bzw. `…: "literal"`; Testwerte werden
  über Helferfunktionen oder benannte Konstanten ohne diese Schlüsselwörter
  gebildet, JSON-Schlüssel kommen nur in `hasOnlyKeys({...})`-Listen vor.

**ESP-IDF:** beide Profile bauen (`esp32_bringup`, `esp32_release`); bestehende
Architektur-, Secret- und Static-Analysis-Gates (`run_pre_ready_gates.sh host` und
`esp`); kein neuer Komponenten-/Library-Stack.

**Hardware – später, nicht in diesem Auftrag** (nach Implementierung und
Independent Fix Verification, auf ausdrücklichen Ownerauftrag):

1. reale Ersteinrichtung auf dem ESP32 (Bench-Gerät `HOME_WIFI` + `Unprovisioned`,
   Einstieg über „Heim-WLAN neu einrichten“);
2. Zustand `PasswordProtected` oder bewusst `PasswordDisabled`;
3. vier reale Sessions erzeugen; 4. fünfte Session fail-closed;
5. Read-only-Polling unter Last; 6. Ressourcenwerte gegen die PR-#174-Basis
(`docs/audits/R1_RAM_LVGL48_HW_EVIDENCE.md`); 7. 1696-B-Ereignis beobachten:
wiederholt → Blocker und Diagnose, sonst weiterhin keine Ursachenbehauptung.
Zusätzlich gemessen: Dauer der Provisionierungsanfrage und Stack-High-Water-Mark.

## 8. Offene Ownerentscheidungen (Freigabe vor Implementierung nötig)

**D1 – Autorisierung der Web-Ersteinrichtung (revidiert).**

- *Option B (Reviewer-Empfehlung, geplant in 3.2):* lokal freigegebenes,
  flüchtiges 10-Minuten-Fenster über **eine** neue Touchaktion
  („Webzugang einrichten freigeben“); keine Interface-Bindung als
  Sicherheitsannahme, keine Touch-Tastatur, keine Pairing-/Token-Domäne.
  Restrisiko: Innerhalb des Fensters gewinnt der erste erreichende Client.
  Bestätigt werden zusätzlich Fensterdauer (10 min) und die UI-Platzierung
  (`HeaderLanguage`-Slot 3 → `HeaderWebAccess`).
- *Option A (verworfen):* Bindung an `networkSetupFlowActive()`/`AP_ONLY` mit
  SoftAP-Annahme; durch den Befund `INADDR_ANY` und das Ende des Setup-Flows
  bei Candidate-Test-Erfolg widerlegt.
- *Option C (abgelehnt):* Provisionierung jederzeit im LAN ohne Bindung oder ein
  Web-Handler, der `LocalDisplay` vortäuscht.

**D2 – Herkunft der ersten Service-PIN.**
Reviewer-Empfehlung `D2=OPTION_A`, `SERVICE_PIN_SOURCE=SEPARATE_FIELD_IN_WEB_PROVISIONING_DIALOG`
(nutzt die bestehende Auth-Domain, keine Touch-Tastatur, keine neue
Persistenzsemantik). Option B (lokale numerische PIN-Eingabe) und Option C
(PIN-loses Bootstrap, neues Schema) bleiben verworfen.

**D3 – Bewusste Änderung des Domainvertrags** (genau eine Funktion `bootstrap`).
Reviewer-Empfehlung: `D3=APPROVE_EXISTING_DOMAIN_EXTENSION`,
`BOOTSTRAP_ACCEPTS_WEB_INTERFACE_WHEN_APPLICATION_AUTHORIZED=YES`,
`WEB_PASSWORD_MODE_DISABLED=SUPPORTED_BY_EXISTING_BOOTSTRAP_RECORD`,
`NEW_AUTH_DOMAIN=NO`, `NEW_SCHEMA=NO`, `NEW_ADR_REQUIRED=NO`. Löst den
Widerspruch zwischen Browser-Setup-SSOT und der heutigen `LocalDisplay`-only-
Prüfung; keine parallele Provisionierungsfunktion.

**D4 – Einrichtungsreihenfolge gegenüber `docs/NETWORK.md` (neu).**
`NETWORK.md` nennt: … WLAN testen → (6) Gerätename → (7) Webzugang mit Passwort
aktivieren/bewusst deaktivieren → (8) Zusammenfassung, „erst nach erfolgreichem
Test speichern“. Im gemergten Stand ist Speichern und Ende des Setup-Flows Teil
von `testCandidate()` (siehe G4); ein Schritt 7 *innerhalb* des Setup-Flows ist
nicht darstellbar.

- *Option 1 (empfohlen, geplant):* Der Webzugang wird **nach** dem erfolgreichen
  Heim-WLAN-Setup als eigener Schritt in der normalen Weboberfläche
  eingerichtet. R1-Ablauf: (1) lokale Moduswahl/Heim-WLAN-Setup bis
  `HOME_WIFI` aktiv; (2) Benutzer öffnet lokal `HeaderWebAccess` und gibt das
  Fenster frei; (3) Browser (im Heimnetz oder AP) öffnet die Shell, die im
  Zustand `unprovisioned` das Einrichtungsformular zeigt; (4) Passwortschutz
  einrichten oder bewusst deaktivieren; (5) normaler Login. `NETWORK.md`
  Schritt 7 wird in S7 auf diesen Ablauf konkretisiert – das ist eine
  SSOT-Änderung und braucht diese Freigabe. Ein *Sperrverhalten* (kein
  normaler Webzugriff vor Abschluss) bleibt unverändert: die Shell zeigt bis
  dahin nur das Einrichtungsformular, Login antwortet `503`.
- *Option 2:* Reihenfolge exakt erhalten, indem `testCandidate()` Persistenz und
  Ende des Setup-Flows von einer späteren Bestätigung trennt. Das ändert den
  gemergten Netzwerkvertrag von PR #171 (Scope, Persistenz, Tests) und wird
  nicht empfohlen.

Die Revision setzt keine dieser Entscheidungen still um. Fehlt eine Freigabe,
beginnt die Umsetzung nicht.

## 9. Risiken

- Der `AuthOperationGate`-Drain kann einen lokalen Werksreset um höchstens eine
  PBKDF2-Dauer verzögern; die Lock-Reihenfolge L1→L2 ist durch den Audit und die
  Concurrency-Tests in S2 zu belegen, bevor S3 beginnt.
- Neue Touchseite und Snapshot-Feld berühren die Render-/Allokationstests
  (`test_ui_steady_state_allocations`); ein neues Snapshot-Feld darf die
  allokationsfreie Steady-State-Eigenschaft nicht verletzen.
- Die Domain-Folge „Root `RecoveryRequired` nach KDF-/Schreibfehler“ kann einen
  Benutzer nach einem seltenen Hardwarefehler zum Werksreset zwingen; das ist
  bestehendes, bewusst fail-closed Verhalten.
- Die Provisionierungsdauer (zwei PBKDF2-Läufe) ist auf der Hardware noch nicht
  gemessen.
- Das Formular vergrößert die Shell; die bestehende Seiten-/Bodygrenze ist im
  Schnitt S6 zu prüfen, nicht zu erhöhen.

## 10. Dokumentations- und Statuswirkung

Nach Freigabe und Umsetzung: `WEB_UI.md` und `NETWORK.md` konkretisieren den
Ersteinrichtungsablauf; `ROADMAP.md` wird nur minimal geführt. Diese Revision
wird als Plan-Commit ausgewiesen; Planpfad, exakte Plan-SHA und die offenen
Entscheidungen stehen im Draft-PR. Danach wird angehalten.
