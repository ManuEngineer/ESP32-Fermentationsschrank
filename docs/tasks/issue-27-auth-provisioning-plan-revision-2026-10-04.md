# Issue #27 / PR #170 – Planrevision Auth-Provisionierungs-Produktpfad

```text
ISSUE=27
PR=170
PLAN_REVISION=AUTH_PROVISIONING_2026-10-04
CONTEXT_BASELINE_BRANCH=agent/issue-27-web-api-auth-main-restart
CONTEXT_BASELINE_SHA=926c3431cc07c79af5627482d3cb7840ba2622f6
CONTEXT_HEAD_SHA=926c3431cc07c79af5627482d3cb7840ba2622f6
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
OWNER_DECISION_REQUIRED=YES
ACTUATOR_RELEASE=NO
NEXT=STOP_FOR_INDEPENDENT_PLAN_REVIEW
```

Diese Revision ist die einzige kanonische Arbeitsgrundlage für den
Auth-Provisionierungs-Produktpfad von Issue #27. Sie autorisiert weder Produktcode
noch Tests, Flash oder Hardwarelauf. Die Umsetzung beginnt erst nach
Ownerfreigabe der exakten Plan-SHA und der Entscheidungen D1–D3 (Abschnitt 8).

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
- **G4 – Erreichbarkeit/Takeover.** Ein ungebundener Provisionierungs-Endpunkt im
  LAN ist „first come, first served“. Während `networkSetupFlowActive()` reicht
  der Dispatcher nur `/` und die drei Netzwerkpfade durch.
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
  Optionen, Warnung, Bestätigung, Service-PIN-Feld) als *ein* gemeinsames
  Fragment, das (a) die bestehende Setup-Seite von `NetworkSetupRoutes` als
  Schritt 7 des Assistenten einbindet und (b) die normale Shell im Zustand
  `unprovisioned` anzeigt. DE/EN/ES über das bestehende Übersetzungsverfahren der
  Shell.

### 3.2 Zugriffsbindung gegen Takeover (Entscheidung D1)

Die Route ist nur zulässig, wenn die Anwendung selbst bestätigt:

```text
webProvisioningAllowed =
    webAuthenticationState() == Unprovisioned
    && inspect() == BootstrapAllowed
    && (networkSetupFlowActive() || networkMode() == AP_ONLY)
```

Begründung: In beiden Kontexten ist der Zugriff über das lokal am Display
angezeigte, pro `StorageEpoch` individuelle SoftAP-Passwort (`NETWORK.md`,
WLAN-QR) gebunden; das ist der in R1 vorhandene Nachweis physischer Anwesenheit.
Im normalen `HOME_WIFI`-Betrieb außerhalb des Setup-Flows ist die Route **nicht**
verfügbar (Antwort `403 provisioning-not-allowed`). Der Benutzer startet die
Einrichtung dann über die bestehende lokale Aktion „Heim-WLAN neu einrichten“
(`beginHomeWifiReconfiguration()`), die auch den heutigen Bench-Zustand
(`HOME_WIFI` + `Unprovisioned`) erreichbar macht.

**Zu verifizieren in Schnitt S2 (nicht angenommen):** Dass die HTTP-Route im
Setup-Flow/AP_ONLY nur über das SoftAP-Interface erreichbar ist, ist eine
Annahme. S2 prüft anhand von `esp_idf_network_lifecycle.cpp` /
`esp_idf_http_server_lifecycle.cpp`, ob bei parallel verbundenem STA-Interface
ein LAN-Client die Route ebenfalls erreichen könnte. Ist das so, wird D1-Option B
(lokales Freigabefenster) erforderlich und die Umsetzung hält an.

Entscheidungsoptionen für D1 siehe Abschnitt 8.

### 3.3 HTTP-Vertrag `POST /api/v1/provision`

| Aspekt | Festlegung |
|---|---|
| Methode | nur `POST`; sonst `405 method-not-allowed` |
| Metadaten | `validWebMetadata` und `web_browser_policy::sameOrigin` unverändert wie Login (Origin hat Vorrang, Referer nur als Fallback bei fehlendem Origin, `Sec-Fetch-Site` ∈ {same-origin, same-site, none}) |
| Content-Type | `application/json` (optional `charset=utf-8`) über `exactJsonContentType`; sonst `415` |
| Max. Body | `kMaximumWebProvisionBodyBytes = 1024` (Konstante in `web_json_codec.hpp` neben `kMaximumWebLoginBodyBytes=768`); größer → `413` |
| Request-DTO | JSON-Objekt, genau diese Schlüssel (`hasOnlyKeys`, Duplikate/NUL wie Login abgelehnt): `mode` (`"protect"` \| `"disable"`, Pflicht), `servicePin` (Pflicht, genau 4 ASCII-Ziffern), `password` (Pflicht bei `protect`, **verboten** bei `disable`; Länge nach `validateWebPassword`, max. 256 Byte im Decoder), `confirmDisable` (Pflicht `true` bei `disable`, **verboten** bei `protect`) |
| Erfolg | `200` `{"provisioned":true,"passwordProtection":"enabled"\|"disabled"}`; **keine** Session, **kein** Cookie, **kein** CSRF-Token |
| Fehler | `400 invalid-json` (Schema/Modus/Felder), `413 request-too-large`, `415`, `403 origin-rejected`, `403 provisioning-not-allowed` (Gate), `409 already-provisioned`, `422 invalid-credentials` (Passwort-/PIN-Regelverletzung), `503 recovery-required` (`RecoveryRequired`, `Indeterminate`, `CommitOutcomeUnknown`, Root nicht `Unprovisioned`), `503 provisioning-failed` (KDF- oder Persistenzfehler) |
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

### 3.4 Anwendungsschicht

Eine neue öffentliche Methode am Application-Owner (alle Webaufrufe laufen
serialisiert über ihn, `APPLICATION_CALL_SERIALIZATION`):

```cpp
enum class WebProvisionMode : std::uint8_t { Protect, Disable };
enum class WebProvisionStatus : std::uint8_t {
    Provisioned, NotAllowed, AlreadyProvisioned, InvalidCredentials,
    RecoveryRequired, Failed
};
WebProvisionStatus FermentationApplication::provisionWebAccess(
    WebProvisionMode mode, const std::string& webPassword,
    const std::string& servicePin);
```

Ablauf (nach dem Muster von `authenticateWebPassword`): unter dem Serializer
Gate (`webProvisioningAllowed`) prüfen, `domain` und `context` kopieren, Serializer
**freigeben**, dann `domain->bootstrap(...)` (PBKDF2 darf mehrere Sekunden dauern
und blockiert den Application-Serializer nicht), anschließend Ergebnis
abbilden. Wird `bootstrap` mit `RecoveryRequired` beantwortet, ruft die Methode
erneut `inspect()` auf: ist das Ergebnis `AlreadyProvisioned` (Retry oder
konkurrierender Request nach erfolgreichem Abschluss), lautet der Status
`AlreadyProvisioned` → HTTP 409 und nicht fälschlich „Recovery“. Parallele
Anfragen werden durch den Domain-Mutex serialisiert; der zweite Aufruf sieht den
neuen Rootzustand.

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
  durch `NETWORK.md`/ADR); Passwort und PIN reisen im Klartext im SoftAP-WPA2-Netz.
  (b) Ohne Session kein CSRF-Token; Schutz über Origin-/Fetch-Metadata und
  JSON-only. (c) Race zwischen zwei Setup-Clients: der zweite erhält `409`.

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

- Dauerhaftes zusätzliches RAM: keine neuen Member; Flash-Konstante für das
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
   gemäß 3.5, `Disabled`-Record mit Zufallsverifier; vorhandene Tests angepasst,
   neue Domaintests. Betroffene Dateien: `authentication_records.hpp/.cpp`,
   `test/test_authentication_records`.
2. **S2 – Application.** `provisionWebAccess`, `webProvisioningAllowed`,
   Statusabbildung inkl. `AlreadyProvisioned`-Reinspektion; Prüfung der
   SoftAP-Erreichbarkeitsannahme (3.2) mit schriftlichem Befund im Plan-Status.
   Betroffene Dateien: `fermentation_application.hpp/.cpp`,
   `test/test_web_application_routes` (Fixture).
3. **S3 – HTTP/Codec.** `decodeWebProvision`, Konstante
   `kMaximumWebProvisionBodyBytes`, `handleProvision`, Dispatcher-Durchlass
   während des Setup-Flows nur für diesen Pfad. Betroffene Dateien:
   `web_json_codec.hpp/.cpp`, `web_application_routes.hpp/.cpp`, Tests.
4. **S4 – Formular.** Gemeinsames statisches Fragment (zwei Optionen,
   Vorauswahl „Passwortschutz“, Warnung, Bestätigung, PIN-Feld, DE/EN/ES) in
   Setup-Seite und Shell; Statusanzeige nach Erfolg. Betroffene Dateien:
   `network_setup_routes.cpp`, `web_application_routes.cpp`, Tests auf
   Seiteninhalt/Größenschranke.
5. **S5 – Dokumentation.** `docs/WEB_UI.md` (Ersteinrichtungsablauf,
   Provisionierungsroute), `docs/NETWORK.md` (Schritt 7 konkretisiert),
   `docs/ROADMAP.md` minimal; keine neuen ADRs, sofern D3 nicht ausdrücklich einen
   ADR verlangt.

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
- Gate: außerhalb von Setup-Flow/AP_ONLY → `403 provisioning-not-allowed`.
- Persistenzfehler (Root-Write, Credential-Write, Readback, `CommitOutcomeUnknown`)
  → kein Erfolg, Zustand `RecoveryRequired`, keine Session; Fehler vor dem
  ersten Write → weiterhin `Unprovisioned`.
- KDF-/Random-Fehler fail-closed.
- Wiederholung nach Erfolg → `409`; konkurrierender zweiter Request → `409`
  (nicht „recovery“).
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

**D1 – Surface-/Takeover-Bindung der Web-Ersteinrichtung.**

- *Option A (empfohlen):* Route nur im Setup-Flow oder `AP_ONLY` (3.2); der
  SoftAP-Passwortbesitz gilt als Nachweis physischer Anwesenheit. Kein neuer
  Touch-Dialog. Voraussetzung: S2 bestätigt, dass die Route in diesen Kontexten
  nicht über ein parallel verbundenes Heimnetz erreichbar ist.
- *Option B:* Zusätzlich ein lokales, zeitlich begrenztes Freigabefenster am
  Touchdisplay („Web-Einrichtung freigeben“). Sicherer gegen LAN-Zugriff, aber
  neuer Touch-Dialog, Zustand und Timer (Mehraufwand, eigener Schnitt).
- *Option C (abgelehnt):* Provisionierung jederzeit im LAN ohne Bindung
  (first-come takeover) oder ein Web-Handler, der `LocalDisplay` vortäuscht.

**D2 – Herkunft der ersten Service-PIN.**

- *Option A (empfohlen):* getrenntes PIN-Feld im selben Web-Dialog (3.6).
- *Option B:* PIN erst lokal am Display setzen; benötigt eine neue numerische
  Touch-Eingabe und widerspricht dem Zeitrahmen von R1.
- *Option C:* PIN-loses Bootstrap; erfordert ein Schema-/Zustandsmodell für „PIN
  nicht gesetzt“ und ist abgelehnt (neue Persistenzsemantik).

**D3 – Bewusste Änderung des Domainvertrags** (genau eine Funktion):
`bootstrap` akzeptiert `WebInterface` und kennt `WebPasswordMode::Disabled` mit
Zufallsverifier (3.5). Alternative wäre ein paralleler Domainpfad, der
ausdrücklich nicht empfohlen wird (Parallelmodell). Ob dazu ein ADR/Vermerk in
`docs/DECISIONS.md` gewünscht ist, entscheidet der Owner.

Die Revision setzt keine dieser Entscheidungen still um. Fehlt eine Freigabe,
beginnt die Umsetzung nicht.

## 9. Risiken

- Annahme zur SoftAP-Erreichbarkeit (3.2) ist offen und in S2 zu belegen.
- Die Domain-Folge „Root `RecoveryRequired` nach KDF-/Schreibfehler“ kann einen
  Benutzer nach einem seltenen Hardwarefehler zum Werksreset zwingen; das ist
  bestehendes, bewusst fail-closed Verhalten.
- Die Provisionierungsdauer (zwei PBKDF2-Läufe) ist auf der Hardware noch nicht
  gemessen.
- Das Formularfragment vergrößert die Setup-Seite; die bestehende Seiten-/
  Bodygrenze ist im Schnitt S4 zu prüfen, nicht zu erhöhen.

## 10. Dokumentations- und Statuswirkung

Nach Freigabe und Umsetzung: `WEB_UI.md` und `NETWORK.md` konkretisieren den
Ersteinrichtungsablauf; `ROADMAP.md` wird nur minimal geführt. Diese Revision
wird als Plan-Commit ausgewiesen; Planpfad, exakte Plan-SHA und die offenen
Entscheidungen stehen im Draft-PR. Danach wird angehalten.
