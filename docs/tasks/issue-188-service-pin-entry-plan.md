# Issue #188 A – Service-PIN End-to-End aus den Einstellungen

Revision 4 (ersetzt Revision 3 `9c6f20c`, Revision 2 `0b185d0` und Revision 1 `9ae345e`; keine davon ist freigegeben)
Status: Planvorschlag zur Ownerfreigabe (Plan-PR #199, keine Produktimplementierung)
Datum: 2026-10-09
Issue: #188, Produktfehler A (`Refs #188`; das Issue bleibt offen)
Plan-Basis: `origin/main` `8a9e4451874f0b6c9074c1e826d7067157999b10`

```text
SCOPE=ISSUE188_A_SERVICE_PIN_END_TO_END
ISSUE28_OVERLAP=MINIMAL_LOCAL_PIN_AND_LEASE_G1_OWNER_DECIDED_YES
ISSUE19_RESET_A=NOT_IN_SCOPE
ISSUE188_B_DOC_FINDING=OUT_OF_SCOPE_STAYS_OPEN_SEPARATE_ISSUE_G4
O0=PENDING_NEW_PLAN_SHA
G1_G5=OWNER_DECIDED
HARDWARE=NOT_RUN
ACTUATOR_RELEASE=NO
```

## 1. Ziel und Nicht-Ziele

Verbindliches Ownerziel:

```text
Home → Einstellungen → Service (PIN) → vierstellige PIN-Eingabe
     → [korrekte PIN] → geschützte Service-Seite
```

Auf derselben PIN-Seite: „PIN vergessen?“ → vorhandener PIN-unabhängiger
mehrstufiger Vollreset B aus #19, nur wenn dessen Application-Owner ihn zulässt
(`snapshot.factoryReset.available`). Falsche PIN, Sperre, fehlende
Provisionierung und ein unzulässiger Lauf-/Safetyzustand gewähren keinen
geschützten Zugang. Abbrechen und Zurück bleiben jederzeit möglich.

Nicht-Ziele:

- keine #28-Funktionen außer dem in Abschnitt 4 genannten minimalen Überlapp
  (keine Diagnose-, Chart-, Export-, Aktor- oder Hardware-Servicefunktionen);
- kein PIN-geschützter Werksreset A aus #19;
- kein Übergang des Prozesses in `ProcessState::ServiceMode`;
- keine Web-Servicelease, keine zweite PIN-Datenbank, keine Default-PIN, kein
  lokales Anlegen oder Ändern der Service-PIN;
- keine Änderung an `AuthenticationDomain`, KDF, Work Factor, Lockout-Regeln
  oder Bootstrap;
- keine Hardwaretests in diesem PR; keine Änderung an GPIO, CI-/Runner-Gates;
- keine Korrektur von `SIM-26-21`/`SIM-26-65` (Befund B, Abschnitt 10).

## 2. Verifizierte Ausgangslage

### 2.1 Vorhandene Bausteine

| Baustein | Stelle | Stand |
|---|---|---|
| Seiten `FermentationUiPage::Service` und `Pin` | `fermentation_touch_workspace.cpp:1670-1697` | Navigationsschalen: Service mit `pin` (immer aktiv), `status`, `recovery` (nur bei `service.available`); Pin mit `cancel`, `status`, `forgot-pin` (nur bei `factoryReset.available`) |
| Renderer für Service/Pin | `main/fermentation_ui_renderer.cpp:1329-1337` | zeigt nur `deferred-28`; keine Tastatur, keine Eingabe |
| `device_platform::PinEntryModel` | `lib/device_platform/src/device_ui_pin.hpp` | vierstellig, Ziffer/Löschen/Leeren/Abbrechen/Bestätigen, `masked()`, transienter `candidate()`, Ownerzustand `Empty…Accepted`; im Produkt unbenutzt |
| `device_platform::ServiceSessionLease` | `lib/device_platform/src/device_ui_session.hpp` | Inaktivität, optionale Absolutdauer, Ereignisse `RelevantUserActivity`, `DeviceRestart`, `ExplicitSignOut`, `SafetyStateInvalidated`; Ablauf terminal |
| `fermentationTouchServicePolicy()` | `fermentation_ui_models.cpp:168-170` | 10 min Inaktivität, keine Absolutdauer; im Produkt unbenutzt |
| `AuthenticationDomain::verifyServicePin` | `authentication_records.cpp:891-935` | prüft gegen den Credential-Record, persistiert Fehlversuche und Lockout (3 Fehlversuche → 30 s, exponentiell bis 30 min), überlebt Neustart; nur intern von `changeServicePin` aufgerufen |
| Muster Application-Grenze | `FermentationApplication::authenticateWebPassword()` (`fermentation_application.cpp:1505-1571`) | `ApplicationCallSerializer`, `AuthOperationGate::tryBegin()`, PBKDF2 außerhalb des Gates, Statusabbildung |
| Bildschirmtastatur | `fermentation_touch_workspace.hpp:67-87`, `fermentation_ui_renderer.cpp:72-80,1049-1090,1647ff.` | 4×10-Raster aus `ContentCell`-Zielen mit fester Geometrie und Hit-Test |
| Press-/Outcome-Muster | `main/fermentation_ui_press_dispatcher.cpp:62-71,216-237` | optionales Kommando im Press → `FermentationUiCommandBridge` → `workspace.note…Outcome()` |
| Steady-State-Allokationsregel | `docs/RESOURCE_BUDGET_AND_MAINTENANCE.md`, Abschnitt „Hauptschleife und lokale UI“; Testmuster `test/test_ui_steady_state_allocations` | keine wiederholte Allokation im unveränderten Main-/UI-Pfad |

### 2.2 Was fehlt

- Kein Owner setzt `service.available` im Produkt
  (`FermentationUiProjectionInput::service` bleibt Default); die
  Settings-Zeile ist auf dem Gerät deshalb immer gesperrt.
- Keine Application-Methode zur lokalen PIN-Prüfung; keine lokale Lease.
- Keine PIN-Eingabe auf der PIN-Seite.
- Kein Service-Menü-Producer. Von den in `docs/LOCAL_UI_SETTINGS_SERVICE.md`
  genannten geschützten Funktionen (Aktortests, Sensorzuordnung, Parameter,
  Wiederherstellungsmenü, Werksreset A, Touchkalibrierung) ist **keine**
  implementiert. Der vorhandene `recovery`-Slot der Service-Seite öffnet die
  Lauf-Recovery-Seite (`FermentationUiPage::Recovery`), nicht das „normale
  Wiederherstellungsmenü“.

### 2.3 Fehlerursache der Navigation

- `pressImpl()` sperrt die Service-Zeile über `current.settings->serviceAvailable`
  (`fermentation_touch_workspace.cpp:2527-2533`).
- `pressSettingsRow()` navigiert nach `NavigateService` (`:3168-3170`).
- Renderer stellt die Zeile nur bei `settings.serviceAvailable` aktiv dar
  (`fermentation_ui_renderer.cpp:981-983`).
- `FermentationUiSettingsView::serviceAvailable`/`serviceReason`
  (`fermentation_touch_workspace.hpp:353-354`) haben nur diese Verwender und
  einen Test (`test_local_touch_ui.cpp:2395-2398`).

### 2.4 Vertragsbasis

- `docs/LOCAL_UI_SETTINGS_SERVICE.md`: Service vierstellig PIN-geschützt;
  Eintritt nur aus validiertem `STANDBY`, nie aus aktivem Lauf, `FAULT` oder
  `SAFE_BOOT`; lokale Freigabe endet nach 10 min Inaktivität, bei Neustart,
  Abmelden und sicherheitsrelevanten Zustandswechseln; Fehlversuche begrenzt;
  „PIN vergessen?“ ohne PIN (O-R1 = B+); kein isolierter PIN-Bypass.
- #19-Plan: O-R2 = B (Ablauf A erst mit produktivem lokalem Service-/PIN-Zugang,
  `PinEntryModel` und `verifyServicePin` wiederverwenden, keine provisorische
  PIN-Lösung); SIM-R-01, SIM-R-14.
- Die Service-PIN entsteht nur über die Web-Provisionierung
  (`FermentationApplication::provisionWebAccess`).

### 2.5 Laufzeitrisiko der PIN-Prüfung

PBKDF2-HMAC-SHA256 mit `kAuthenticationPbkdf2Sha256WorkFactor = 10000`
(`authentication_records.hpp:20`) läuft im Web-Pfad im httpd-Task. Schon dort
zeigte das PR-#170-Hardwaregate eine Herzschlaglücke der Hauptschleife von
`LARGEST_GAP_MS=3605` während Login/PBKDF2
(`docs/audits/PR170_HW_TOUCH_PATH_FINDING.md:121`). Eine lokale Prüfung aus der
Touch-Dispatch läuft in der Hauptschleife selbst, die auch `platform.update()`
und `application.update()` taktet. Die reine KDF-Dauer auf dem Gerät ist nicht
gemessen. Daraus folgt der Ownerentscheid G2 mit Mess-Gate (Abschnitt 4).

## 3. Lösungsentwurf

### 3.1 Application-Grenze (autoritativ)

Neu in `FermentationApplication`, nach dem Muster von
`authenticateWebPassword()`:

```cpp
enum class LocalServicePinStatus : std::uint8_t {
    Authorized, Invalid, LockedOut, NotProvisioned,
    NotAllowedInState, Unavailable,
};
struct LocalServicePinResult {
    LocalServicePinStatus status{LocalServicePinStatus::Unavailable};
    std::uint64_t retryAfterMs{0U};
};
LocalServicePinResult verifyLocalServicePin(const std::string& pin);
void endLocalServiceSession();     // ausdrückliches Abmelden
void noteLocalServiceActivity();   // relevante Bedienung
```

Eine Uhr: Vergabe, Aktivität, `activeAt()` in `uiSnapshot()` und der
`nowMs`-Wert für `verifyServicePin` stammen alle aus
`timeSource_->monotonicMillis()` der Application. Ohne `timeSource_` ist das
Ergebnis `Unavailable` und `service.available` bleibt `false`.

Zulassungsprädikat `localServiceEntryAllowedUnlocked()` („validiertes
`STANDBY`“). Es ist die **einzige** Zulassungsquelle und wird vor der KDF,
nach der KDF, für `service.available`, für die Invalidierung einer bestehenden
Lease und für die Guards späterer geschützter Aktionen verwendet; es gibt
keine zweite, schwächere UI-Ableitung. Alle Bedingungen müssen gelten:

1. `lifecycleState_ == ApplicationLifecycleState::Ready` (bestehendes
   `ready()`, `fermentation_application.cpp:2528-2530`). `ServiceRequired`
   (gesetzt von `requireService()`, `:2540-2546`, auch zur Laufzeit) und
   `Initializing` sind ausgeschlossen, selbst wenn der Run-State formal noch
   `Standby` meldet.
2. `runtimeRunState_ != nullptr` und
   `runtimeRunState_->processState.state == ProcessState::Standby`.
3. Kein veröffentlichter Lauf: bestehendes `factoryResetRunGateOpenUnlocked()`
   (`:2566-2573`, weder `activeProgramRun` noch `activeManualRun`).
4. Kein offener Recovery-Kontext: `pendingRecoverySource_ == nullptr`, kein
   `recoveryDisposition_` und der Run-Persistence-Koordinator nicht in
   `FallbackRecoveryPending`. Das sind dieselben Quellen, aus denen
   `uiSnapshot()` `recovery.mode` projiziert (`:1433-1440`,
   `fermentation_ui_projector.cpp:97-130`).
5. Geladene Konfiguration (`storageEpoch_`).

Damit sind aktiver Lauf, `Fault`, `SafeBoot`, `RecoveryEvaluation`,
`ServiceRequired` und jede offene Recovery-Entscheidung ausgeschlossen.

Ablauf von `verifyLocalServicePin()` (Trennung wie in
`authenticateWebPassword()`, `fermentation_application.cpp:1505-1571`):

1. **Unter `ApplicationCallSerializer`:** Ist das Zulassungsprädikat nicht
   erfüllt, Ergebnis `NotAllowedInState` **vor** jeder KDF und jedem
   Auth-Write. Danach `webAuthenticationStateUnlocked()`
   (`:1473-1498`) einzeln abbilden: `PasswordProtected` und
   `PasswordDisabled` → weiter (beide Modi verlangen die Service-PIN);
   `Unprovisioned` → `NotProvisioned` ohne KDF; `RecoveryRequired` und
   `Indeterminate` → `Unavailable` ohne KDF. Fehlen Domain oder Kontext oder
   liefert `authOperationGate_.tryBegin()` keinen Token, Ergebnis
   `Unavailable`. Sonst Token, Domain-Zeiger und eine Kopie des
   `AuthenticationBootstrapContext` übernehmen und dessen `storageEpoch()` und
   `bootstrapSequence()` sowie die bestehende `webTrustGeneration_`
   festhalten. Serializer verlassen.
2. **Außerhalb des Serializers, Token gehalten:**
   `AuthenticationDomain::verifyServicePin(context, pin, nowMs, retryAfterMs)`.
3. **Token freigeben, bevor der Serializer erneut betreten wird:**
   `token.reset()`, Domain-Zeiger und Kontextkopie verwerfen. Grund: Werksreset
   und Auth-Reinitialisierung rufen `authOperationGate_.closeAndDrain()`
   **unter** dem Serializer auf (`:2327`, `:2037`). Hielte die PIN-Prüfung
   ihr Token beim erneuten Lock-Versuch, warteten beide aufeinander.
4. **Abbildung des Prüfergebnisses:** `Authenticated` → weiter mit Schritt 5;
   `Invalid` ohne `retryAfterMs` → `Invalid`; `Invalid` mit `retryAfterMs`
   oder `LockedOut` → `LockedOut`; `RecoveryRequired` (ungültiger Kontext oder
   unlesbare Credentials, kein unprovisioniertes Gerät) und `KdfUnavailable`
   → `Unavailable`. Nur `Authenticated` führt zu Schritt 5.
5. **Erneut unter dem Serializer, fail-closed:** Eine Lease wird nur vergeben,
   wenn
   - das Zulassungsprädikat weiterhin erfüllt ist (sonst
     `NotAllowedInState`), und
   - `authenticationDomain_` und `authenticationContext_` weiterhin vorhanden
     sind, `authenticationResolutionStatus_ ==
     AuthenticationBootstrapResolutionStatus::Ready` gilt und
     `storageEpoch()` sowie `bootstrapSequence()` des aktuellen Kontexts den
     in Schritt 1 festgehaltenen Werten entsprechen (sonst `Unavailable`), und
   - die aktuelle `webTrustGeneration_` der in Schritt 1 festgehaltenen
     entspricht (sonst `Unavailable`). `resetAuthenticationState()` erhöht sie
     bei jedem Ersetzen der Auth-Domain (`fermentation_application.cpp:2033-2047`,
     aufgerufen aus `initializeAuthentication()`, `:2003`). Damit wird auch eine
     Auth-Reinitialisierung **innerhalb derselben Epoche und
     Bootstrap-Sequenz** erkannt, die Epoche und Sequenz allein nicht zeigen.
   Ein Werksreset, Epochenwechsel oder eine Auth-Reinitialisierung zwischen
   Prüfung und Vergabe ergibt damit nie eine Lease. Auch ein Web-Trust-Wechsel
   (Netzwerkmodus, HOME_WIFI-Einrichtung) erhöht die Generation; eine noch
   laufende lokale Anmeldung wird dann konservativ mit `Unavailable`
   abgelehnt. Eine bereits rechtmäßig bestehende lokale Lease beendet ein
   solcher Wechsel nicht (siehe Trust-Boundary unten). Es entsteht keine neue
   Generation, Domain- oder Token-Architektur. Erst dann
   `localServiceLease_ = ServiceSessionLease(fermentationTouchServicePolicy(), nowMs)`.

Lease-Haltung und Projektion:

- `localServiceLease_` ist ein eigenes Mitglied der Application, getrennt vom
  `WebSessionManager`; die Web-Policy und Web-Leases bleiben unberührt.
- `uiSnapshot()` setzt `input.service.available` genau dann, wenn
  `localServiceLease_.activeAt(nowMs)` und das Zulassungsprädikat erfüllt ist;
  sonst `false` mit `unavailableReason` (`service-locked`). Beide Felder
  gehen bereits in `equalFermentationUiSemanticSnapshot()` ein
  (`fermentation_ui_models.cpp`); ein Wechsel, auch durch reinen
  Zeitablauf, erhöht deshalb die Refresh-Revision und erzwingt ein
  Neuzeichnen.
- `update()` meldet `SafetyStateInvalidated`, sobald das Zulassungsprädikat
  nicht mehr erfüllt ist; die Lease ist dann terminal beendet. Zusätzlich
  beendet `requireService()` die Lease sofort. Bis dahin verhindert das
  Prädikat in `service.available` und in jedem Guard, dass eine noch nicht
  beendete Lease wirkt.
- `resetAuthenticationState()` (Werksreset, Auth-Reinitialisierung) beendet die
  Lease.
- Trust-Boundary: `revokeWebSessionsAtTrustBoundary()` (Netzwerkmoduswechsel,
  Einstieg in die HOME_WIFI-Einrichtung, `fermentation_application.cpp:958-962,
  1193-1196`) beendet die lokale Lease **nicht**. Begründung: Diese Grenzen
  schützen Browser-Sessions vor einem neuen Transport; die lokale Lease hängt
  an keinem Netzwerk. Der Werksreset beendet sie über
  `resetAuthenticationState()`.
- Neustart: Die Lease liegt nur im RAM und entsteht nach dem Boot nie neu.
- `noteLocalServiceActivity()` meldet `RelevantUserActivity`, nur wenn die
  Lease aktiv ist; Hintergrundupdates rufen es nicht auf.
- Die PIN wird nicht geloggt, nicht persistiert und nur als Parameter
  durchgereicht. Persistiert werden ausschließlich die bestehenden
  Lockout-Felder durch `verifyServicePin`.

### 3.2 Touch-Workspace

- Settings-Zeile `Service`: immer ein Tap-Ziel. `pressSettingsRow()` navigiert
  bei `snapshot.service.available` zur Service-Seite, sonst zur PIN-Seite.
- `FermentationUiSettingsView::serviceAvailable`/`serviceReason` entfallen
  (keine Verwender mehr).
- PIN-Seite hält ein `device_platform::PinEntryModel` im Workspace. Beim
  Betreten und Verlassen der Seite wird es zurückgesetzt. Jede Mutation des
  Modells (Ziffer, Löschen, Leeren, Ownerzustand, Reset) ruft das bestehende
  `markRenderRelevantChange()` auf, damit der Render-Key über
  `workspaceRevision` wechselt.
- Tastatur: die vorhandene Rastergeometrie der Bildschirmtastatur. Zeile 1
  enthält die Ziffern `1 2 3 4 5 6 7 8 9 0` (10 Spalten), Zeile 2 `Löschen`
  (Backspace) und `Leeren`. Andere Zeilen sind keine Ziele.
- Bottom-Slots der PIN-Seite: Slot 1 `cancel` (bestehend, `NavigateBack`),
  Slot 2 `confirm` (ersetzt `status`; aktiv nur bei vollständiger Eingabe und
  nicht `RetryWait`), Slot 3 `forgot-pin` (bestehend, unverändert gebunden an
  `factoryReset.available`, unabhängig vom PIN- und Lockout-Zustand).
- `confirm` erzeugt im Press ein neues Kommando
  `FermentationUiVerifyServicePinCommand{candidate}`; danach wird das Modell
  sofort zurückgesetzt.
- Ergebnis über `workspace.noteServicePinOutcome(result)`:
  `Authorized` → liegt unter `Pin` bereits `Service` (Weg über
  `Status → Diagnose → Service → Pin`), genügt `goBack()`; sonst wird `Pin`
  im Stapel durch `Service` ersetzt (`Home → Settings → Service`; Zurück führt
  zu `Einstellungen`). So entsteht nie zweimal `Service` im Stapel;
  `Invalid` → `Rejected` mit Text `pin-wrong`;
  `LockedOut` → `RetryWait` mit Text `pin-locked` (ohne Countdown);
  `NotProvisioned` → Text `pin-not-provisioned` („Service-PIN über den
  Webzugang einrichten“); `NotAllowedInState` → Text `service-locked-state`;
  `Unavailable` → Text `pin-unavailable`.
- Service-Seite mit aktiver Lease: kein `blockedReason`; Slot 1 `sign-out`
  (ruft `endLocalServiceSession()`), Slot 2 `status`, Slot 3 `recovery`
  (bestehend, wird durch `service.available` aktiv; siehe G5). Inhalt bleibt
  der Hinweis `deferred-28`: Servicefunktionen sind zurückgestellt; es wird
  kein funktionsfähiger Eintrag vorgetäuscht.
- Service-Seite ohne Lease (auch über `Status → Diagnose → Service`):
  unverändert gesperrt mit Grund; Slot 1 `pin` führt zur PIN-Eingabe. Damit
  umgeht der Diagnoseweg die PIN nicht.
- Läuft die Lease ab, während die Service-Seite offen ist, zeigt die Seite
  wieder den gesperrten Zustand (sicherer Bildschirm).
- Kanonischer Testhilfe-Stapel `setPage(Pin)`: `Home → Settings → Pin`.
  Routen (Breadcrumbs) bleiben unverändert.

### 3.3 Dispatcher und Aktivität

- `dispatchWorkspacePress()` ruft für `verifyServicePin` die neue
  Application-Methode über `FermentationUiCommandBridge` und meldet das
  Ergebnis an den Workspace (Muster `setDeviceName`).
- `processWorkspaceTouch()` ruft bei einer frischen Berührung auf der
  Service- oder Recovery-Seite `application.noteLocalServiceActivity()`.
- Der Dispatcher loggt wie bisher nur den Outcome-Code
  (`main/app_main.cpp:544`); weder Kommando noch PIN werden geloggt.

### 3.4 Renderer

- PIN-Seite: maskierte Anzeige `PinEntryModel::masked()` in der Kopfzeile des
  Inhalts, Ziffern- und Löschzeile im Tastaturraster, Statustext aus dem
  Ownerzustand. Kein `deferred-28` mehr auf der PIN-Seite.
- Service-Seite: unverändert `deferred-28` plus Sperrgrund ohne Lease; Slot
  `sign-out` mit Lease.
- Settings-Zeile `service-protected` immer aktiv, ohne Grundtext.
- Steady-State: Im unveränderten Zustand auf PIN- und Service-Seite keine
  wiederholte Allokation (Regel `docs/RESOURCE_BUDGET_AND_MAINTENANCE.md`,
  Abschnitt „Hauptschleife und lokale UI“). Die maskierte Anzeige wird nur bei
  geändertem Render-Key aufgebaut.
- Vorhandener Textschlüssel `confirm` (en/de/es) wird wiederverwendet. Neue
  Textschlüssel in en/de/es: `pin-enter`, `pin-wrong`, `pin-locked`, `pin-not-provisioned`,
  `pin-unavailable`, `service-locked-state`, `sign-out`, `pin-backspace`,
  `pin-clear`.

### 3.5 Unverändert

Factory-Reset-Ablauf B und sein Owner, `AuthenticationDomain`, KDF und
Lockout-Regeln, Web-Sessions und Web-Policy, Lauf-Recovery-Seite und deren
Gates, Diagnose-Seite, Aktor- und Safetypfade. Die Lease ist nur in
`service.available` projiziert und mit keinem Aktorpfad verbunden.

## 4. Ownerentscheide (G1–G5 `OWNER_DECIDED`)

Die Entscheide G1–G5 sind getroffen. Sie sind keine vorweggenommene Freigabe
dieser Plan-SHA oder der Implementierung (O0, Abschnitt 9).

### G1 = JA – minimaler #28-Überlapp

Zugelassen ist genau: vierstellige lokale PIN-Eingabe und -Verifikation über
den bestehenden Auth-Owner, lokale Service-Lease mit 10-min-Inaktivität und
deren Invalidierung, Projektion `service.available`, Service-Seite als
geschützter Einstieg mit Abmelden. Nicht zugelassen: jede weitere Service-,
Diagnose-, Chart-, Export- oder Aktorfunktion aus #28 und der Werksreset-
Ablauf A aus #19. #28 bleibt offen.

### G2 = A – PBKDF2 synchron in der Hauptschleife

Kein vorsorglicher Worker-Task. **Vor dem Merge zwingendes
Owner-Hardware-Mess-Gate** (Abschnitt 7): reale Dauer einer lokalen
PIN-Prüfung, größte Hauptschleifen-/Herzschlaglücke, Watchdog- und
Resetverhalten, auch bei gleichzeitiger Web-Anmeldung. Die bisher im Web-Pfad
gemessenen 3605 ms (`docs/audits/PR170_HW_TOUCH_PATH_FINDING.md:121`, anderer
Task) sind kein Bestehensnachweis. Bei unvertretbarer Blockade oder einem
Watchdog: **STOPP** und erneuter Ownerentscheid; keine eigenmächtige
Umstellung auf einen Worker-Task. `ACTUATOR_RELEASE=NO`.

Fakten zur Beurteilung: Die Prüfung läuft nur bei erfülltem
Zulassungsprädikat, also ohne aktiven Lauf; über das Web kann währenddessen
kein Lauf starten, weil der produktive Run-Mutationspfad
`POST /internal/ui/run` nicht registriert ist (Roadmap #27). Das
`AuthOperationGate` ist ein Lifetime-/Drain-Gate mit mehreren gleichzeitig
möglichen Tokens und serialisiert nicht; die Verifikation serialisiert die
`AuthenticationDomain` über ihren Domain-Mutex
(`authentication_records.hpp:358`). Eine gleichzeitige Web-Anmeldung kann die
lokale Prüfung deshalb verzögern. Vor einer späteren Aktorfreigabe (#35) ist
die Blockade erneut zu bewerten.

### G3 = JA – Settings-Zeile

`Einstellungen → Service (PIN)` ist immer antippbar und hat **keinen**
Sperrgrund-Zweittext. Ohne Lease führt sie zur PIN-Eingabe, mit gültiger
Lease zur Service-Seite. Das Öffnen erteilt keine Berechtigung.

### G4 = JA – Tracking von Befund B

Befund B (`SIM-26-21`/`SIM-26-65`) wird in einem separaten Doku-Issue mit
Rückverweis auf #188 nachverfolgt. Das administrative Tracking wird nach der
Planfreigabe vorbereitet bzw. ausgeführt; PR #199 enthält keinen Doku-Fix und
keinen zweiten Implementierungsscope. #188 B wird erst nach nachvollziehbarer
Übergabe oder Erledigung als übertragen gekennzeichnet (Abschnitt 10).

### G5 = A – Slot `recovery` unverändert

Der bestehende Slot `recovery` der Service-Seite bleibt unverändert. Er öffnet
die Lauf-Recovery-Seite; im `Standby` bietet sie nichts Bedienbares
(`resume-fallback` nur in `FallbackSelectionRequired`, sonst `status` und
`diagnostics`, `fermentation_touch_workspace.cpp:1698-1715`). Die Doku (C3)
kennzeichnet ihn ausdrücklich als *Lauf-Recovery-Seite*; er ist **nicht** das
noch fehlende normale PIN-geschützte Wiederherstellungsmenü aus #28.

## 5. Abbildung auf die Akzeptanz aus #188 A

| #188-A-Kriterium | Umsetzung |
|---|---|
| 1 Einstieg erreichbar, direkt PIN-Abfrage | Settings-Zeile immer Ziel → PIN-Seite mit Eingabe (3.2) |
| 2 „PIN vergessen?“ ohne PIN, nur Ablauf B | bestehender Slot 3, gebunden an `factoryReset.available`, unabhängig vom Lockout |
| 3 keine Berechtigung durch Öffnen; Lockout, Persistenz und Zustand nicht umgangen | Lease nur nach `Authorized` und erfülltem Zulassungsprädikat (3.1) |
| 4 Regression über echten Hit-Test/Dispatcher | Abschnitt 6 |
| 5 Hardware-Reverifikation | Abschnitt 7 |

Die Service-Seite ist danach ein geschützter Einstieg ohne eigene
Servicefunktionen; das wird auf der Seite (`deferred-28`) und in der Doku so
benannt.

## 6. Tests

Alle Touchfälle laufen über `processWorkspaceTouch` mit Koordinaten aus dem
gerenderten Screen (`targetAt`), jede Berührung als frische Kante mit
Loslassen, nicht über `setPage(Pin)`. Fixtures: `WebAccessFixture` mit
`provisionWebAccess(..., "1234")` und `TestKdf`; `OwningAppFixture` ohne
Provisionierung; `VirtualTimeSource` für die Zeit; Storage-Epoch per
bestehendem `epochOf`-Muster; Auth-Record per
`AuthenticationRecordStore::readCredentials`.

### 6.1 Application (C1, `test/test_press_dispatcher`)

| Fall | Erwartung |
|---|---|
| korrekte PIN in validiertem `Standby` | `Authorized`; `uiSnapshot().service.available == true` |
| falsche PIN | `Invalid`; keine Lease; Fehlversuch persistiert (legitimer Auth-Write) |
| 3 falsche PINs, dann korrekte | `LockedOut` mit `retryAfterMs > 0`; korrekte PIN während der Sperre ergibt keine Lease |
| Lockout nach Neustart (neue Application auf demselben Store) | weiter `LockedOut`; keine Lease nach dem Boot |
| nicht provisioniert | `NotProvisioned`; keine KDF; Auth-Records unverändert |
| aktiver Lauf | `NotAllowedInState` vor KDF und Auth-Write; Zustand über `setActiveManualRun()`: diese Hilfe existiert im test-lokalen `FermentationApplicationTestAccess` von `test_factory_reset_flow` (`test_factory_reset_flow.cpp:476-503`) und wird im test-lokalen `FermentationApplicationTestAccess` von `test_press_dispatcher` gleich ergänzt (nur Testdatei) |
| `Fault`, `SafeBoot` | `NotAllowedInState` vor KDF und Auth-Write, sofern über den bestehenden Seam `FermentationApplicationTestAccess::runtimeState()` (`test_press_dispatcher.cpp:41`) konsistent herstellbar; sonst Nachweis des Zustandsprädikats auf Snapshot-Ebene und als Grenze in 6.5 benannt |
| Zustandswechsel während der Prüfung | keine Lease; ein test-lokaler Callback in der bestehenden `TestKdf` (`test_press_dispatcher.cpp:145-160`, nur Testdatei, kein Produktcode) ruft während `derive()` `setActiveManualRun(application, true)` auf |
| Inaktivität 10 min | `service.available == false`; Aktivität davor verlängert |
| Abmelden | Lease beendet |
| Laufstart bei aktiver Lease | Lease terminal beendet |
| Werksreset bzw. `resetAuthenticationState()` | Lease beendet |
| `ServiceRequired` bei weiterhin `Standby` im Run-State (test-lokaler Aufruf von `requireService()` über das friend-`FermentationApplicationTestAccess`) | PIN-Prüfung `NotAllowedInState` vor KDF und Auth-Write; eine vorher vergebene Lease ist beendet, `uiSnapshot().service.available == false` |
| offener Recovery-Kontext (`recoveryDisposition_` oder `pendingRecoverySource_` gesetzt, über bestehende Recovery-Fixtures bzw. test-lokalen friend-Zugriff) | wie `ServiceRequired`: verweigert vor KDF, bestehende Lease wirkt nicht |

### 6.1a Interleaving (C1, `test/test_web_application_routes`)

Wiederverwendung der bestehenden Seams `BlockableKdf` (`arm`, `waitEntered`,
`release`), `ComposedFixture`, `FermentationApplicationTestAccess::provision`
und `authRecordBytes()` nach dem Muster von
`test_factory_reset_drains_running_login_before_touching_the_store` und
`test_stale_protected_login_cannot_create_a_session_after_factory_reset`:

| Fall | Erwartung |
|---|---|
| lokale PIN-Prüfung blockiert in der KDF; ein zweiter Thread startet den Werksreset | Reset wartet auf das Token, hängt aber nicht; nach `release()` endet die Prüfung, der Reset läuft durch; die Prüfung liefert `Unavailable` (Epoche/Kontext geändert) und vergibt keine Lease |
| lokale PIN-Prüfung blockiert in der KDF; der Reset schlägt fehl und öffnet das Gate wieder | Kontext unverändert; Ergebnis wie ohne Reset |
| Prüfung nach geschlossenem Gate | `Unavailable` ohne KDF |
| lokale PIN-Prüfung blockiert in der KDF; ein zweiter Thread ersetzt die Auth-Domain innerhalb derselben Epoche und Bootstrap-Sequenz (test-lokaler friend-Aufruf von `initializeAuthentication(*stateStore_)` unter dem Serializer, bestehender Seam `trustGeneration()` zur Kontrolle) | Reinitialisierung wartet auf das Token, hängt aber nicht; nach `release()`: `storageEpoch()` und `bootstrapSequence()` unverändert, `trustGeneration()` erhöht; Ergebnis `Unavailable`, kein `Authorized`, keine Lease |
| bestehende lokale Lease, danach Netzwerkmoduswechsel (Web-Trust-Wechsel) | Lease bleibt aktiv |

### 6.2 Touch-Ende-zu-Ende (C2, `test/test_press_dispatcher`)

| Fall | Erwartung |
|---|---|
| Home → Einstellungen → Service-Zeile ohne Lease | PIN-Seite mit Eingabe, keine Service-Seite |
| Ziffern `1 2 3 4` + `confirm` (provisioniert) | Service-Seite, `service.available == true`, `recovery` aktiv, `sign-out` vorhanden; Zurück → Einstellungen |
| erneuter Settings-Tap mit aktiver Lease | direkt Service-Seite |
| falsche PIN | PIN-Seite bleibt, Text `pin-wrong`, Eingabe leer |
| Lockout | `confirm` deaktiviert, Text `pin-locked`; `forgot-pin` bleibt aktiv |
| `Status → Diagnose → Service` ohne Lease | gesperrt; `pin` → PIN-Seite; kein Zugang ohne PIN |
| `Status → Diagnose → Service → Pin` mit korrekter PIN | zurück auf die Service-Seite per `goBack()`; Stapel `Home → Status → Diagnostics → Service`, keine doppelte Service-Seite |
| „PIN vergessen?“ ohne Eingabe, auch im Lockout | `FactoryReset`, Stufe `Warning`; `cancel` → Stufe `Idle`, Seite `Pin`; keine Reset-, Bootstrap- oder Konfigurationsmutation nach dem Muster von `test_factory_reset_flow::test_cancel_at_every_stage_changes_no_reset_state` (Storage-Epoch unverändert, kein Netzwerk-/HTTP-Stopp) und zusätzlich `uiSnapshot().revisions.expectedUserConfigurationRevision` und `expectedProgramCatalogRevision` unverändert; Auth-Record gegenüber dem Stand vor „PIN vergessen?“ unverändert (frühere Lockout-Writes durch falsche PINs sind legitim) |
| `factoryReset.available == false` (`setFactoryResetHoldMillis(std::nullopt)`) | `forgot-pin` deaktiviert |
| abgelaufene Lease auf offener Service-Seite | gesperrter Zustand, kein geschützter Slot aktiv |

Umgestellte Bestandstests:

| Test | Änderung |
|---|---|
| `test_press_dispatcher::test_forgot_pin_entry_runs_the_whole_flow_through_touch` | Einstieg über den echten Touchweg statt `setPage(Service)`/`setPage(Pin)` |
| `test_local_touch_ui::test_standby_slot_three_opens_settings_and_service_lives_below_it` | Stapel `Home → Settings → Pin` |
| `test_local_touch_ui::test_settings_rows_are_in_the_decided_order_and_open_their_pages` | Zeile 5 öffnet `Pin` ohne Lease |
| `test_local_touch_ui::test_settings_service_and_device_name_rows_state_when_disabled` | Service-Zeile ohne Lease ist Ziel und öffnet `Pin`; mit `service.available` öffnet sie `Service`; Gerätename unverändert |
| `test_renderer_boundary::test_settings_disabled_rows_show_their_reason` | Service-Zeile aktiv, kein „Service unavailable“ auf der Einstellungen-Seite |
| `test_renderer_boundary::test_deferred_pages_show_the_hint_in_all_locales_and_keep_owner_reason` | PIN-Seite ohne `deferred-28`; Diagnose und Service unverändert |

### 6.3 Steady-State (C2, `test/test_ui_steady_state_allocations`)

Nach dem bestehenden Muster `test_unchanged_state_builds_no_render_and_allocates_nothing`:
unveränderte PIN-Seite (leer und mit Teil-Eingabe) und unveränderte
freigegebene Service-Seite bauen kein Screen-Modell und allokieren im
C++-Pfad nichts; eine Ziffer erzwingt genau ein Neuzeichnen.

### 6.4 Renderer (C2, `test/test_renderer_boundary`)

- PIN-Seite in en/de/es begrenzt, deterministisch, überlappungsfrei
  (bestehende Prüfmuster aus `test_s10_pages_are_bounded_deterministic_and_do_not_overlap`).
- exakte Hit-Zonen der Ziffern-, Lösch- und Leeren-Zellen.
- die Anzeige zeigt nur Maskenzeichen, nie Ziffern.
- alle neuen Textschlüssel existieren in allen Locales.

### 6.5 Grenzen des Nachweises

- C-/ESP-IDF-/LVGL-Allokationen und die reale KDF-Dauer sind nur auf Hardware
  messbar (Abschnitt 7).
- „Keine Aktorfreigabe“ wird strukturell nachgewiesen: die Lease ist nur in
  `service.available` projiziert; bestehende Interlock- und Planner-Suites
  bleiben grün.

### 6.6 Ausführung

- Native: `test_press_dispatcher`, `test_local_touch_ui`,
  `test_renderer_boundary`, `test_ui_steady_state_allocations`,
  `test_web_application_routes`, `test_authentication_records` (unverändert
  grün), `test_web_session` (unverändert grün), `test_factory_reset_flow`
  (unverändert grün).
- ESP-IDF-Builds `esp32_bringup` und `esp32_release`, inklusive
  Ressourcenbericht.
- Builder-Self-Check (`scripts/run_pre_ready_gates.sh self-check`) vor dem
  abschließenden Independent Full Review.

## 7. Hardware (nach Software-Review und ausdrücklicher Freigabe)

Auf dem Owner-ESP32, ohne Werksreset und ohne Powercut:

1. Einstellungen → Service → PIN-Eingabe sichtbar, Ziffern treffen.
2. korrekte PIN → Service-Seite; Messung der Prüfdauer und der größten
   Herzschlaglücke (G2), kein Watchdog, kein Reset.
3. falsche PIN, Lockout nach 3 Fehlversuchen.
4. „PIN vergessen?“ → Warnung → Abbrechen, ohne Reset.
5. lokale PIN-Prüfung, während eine Web-Anmeldung läuft: größte
   Herzschlaglücke, kein Watchdog, kein Reset (G2).
6. Inaktivitätsablauf (oder Abmelden) sperrt wieder.

`HW-19-R01` bleibt nicht bestanden, `HW-19-R02=NOT_RUN`, `HW-19-R03=BLOCKED`;
der tatsächliche Werksreset und Powercut nur separat über #192.
`ACTUATOR_RELEASE=NO`.

## 8. Commit-Schnitte

Nach jedem Commit wird angehalten.

| Schnitt | Inhalt | Nachweis |
|---|---|---|
| C1 | Application-Grenze 3.1 und Tests 6.1/6.1a | Native-Suites |
| C2 | Workspace 3.2, Dispatcher 3.3, Renderer 3.4, Textschlüssel, Tests 6.2–6.4 und umgestellte Bestandstests | Native-Suites, beide ESP-Profile |
| C3 | Doku: `docs/LOCAL_UI_SETTINGS_SERVICE.md:56-63` (Service-Zeile öffnet die PIN-Eingabe; nach korrekter PIN geschützte Service-Seite ohne eigene Funktionen bis #28; Slot `recovery` als *Lauf-Recovery-Seite*, nicht das normale Wiederherstellungsmenü, G5); `docs/ACCEPTANCE_TESTS.md` neue Zeilen `SIM-188-A01..` sowie Traces SIM-172-S10-01 und SIM-19-R07; `docs/ROADMAP.md` | Doku-Diff |

`SIM-26-21`/`SIM-26-65` werden in keinem Schnitt angefasst.

## 9. Ownerentscheidungen

```text
O0=PENDING_NEW_PLAN_SHA
G1=OWNER_DECIDED_YES
G2=OWNER_DECIDED_A_SYNC_MAIN_LOOP_HW_MEASUREMENT_GATE_BEFORE_MERGE
G3=OWNER_DECIDED_YES
G4=OWNER_DECIDED_YES_SEPARATE_DOC_ISSUE
G5=OWNER_DECIDED_A_UNCHANGED_LABELLED_RUN_RECOVERY
```

- [ ] O0: Freigabe dieser Plan-SHA (Revision 3 wurde nicht freigegeben).
- [x] G1–G5: entschieden (Abschnitt 4).

## 10. Befund B – getrennt halten

`docs/ACCEPTANCE_TESTS.md` SIM-26-21 und SIM-26-65 bleiben unverändert; ihr
offener Status in #188 bleibt erhalten. Entschieden (G4): ein separates
Doku-Issue mit Rückverweis auf #188 B; seine Erledigung erfolgt in einem
eigenen kleinen Markdown-only-PR, der beide Referenzen gegen die tatsächliche
Testsemantik prüft und Lücken ehrlich markiert. Das administrative Tracking
wird nach der Planfreigabe vorbereitet bzw. ausgeführt, nicht in PR #199.
#188 B gilt erst nach nachvollziehbarer Übergabe oder Erledigung als
übertragen; #188 wird erst geschlossen, wenn A (inklusive Hardware) und B
erledigt oder übergeben sind.

## 11. Risiken

- Hauptschleifen-Blockade durch PBKDF2 (G2, Hardware-Mess-Gate).
- Lockout-Writes durch falsche PINs verbrauchen Flash-Schreibzyklen; begrenzt
  durch den bestehenden exponentiellen Lockout.
- Benutzer erwarten hinter der Service-Seite Funktionen; abgesichert durch den
  sichtbaren Hinweis `deferred-28` und die Doku.
- Die PIN liegt kurz als `std::string` im RAM (Modell, Kommando,
  Domain-Parameter); kein Logging, kein Persistieren, Modell sofort
  zurückgesetzt. Ein sicheres Überschreiben des Speichers ist nicht Teil des
  bestehenden Vertrags.
