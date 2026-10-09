# Issue #188 A – Service-/PIN-Einstieg aus den Einstellungen

Status: Planvorschlag zur Ownerfreigabe (Plan-PR, keine Produktimplementierung)
Datum: 2026-10-09
Issue: #188, nur Produktfehler A (`Refs #188`; das Issue bleibt offen)
Plan-Basis: `origin/main` `8a9e4451874f0b6c9074c1e826d7067157999b10`
(Merge PR #197)

```text
SCOPE=ISSUE188_A_NAVIGATION_AND_RECOVERY_ENTRY
ISSUE188_B_DOC_FINDING=OUT_OF_SCOPE_STAYS_OPEN
LOCAL_SERVICE_PIN_VERIFICATION=NOT_IN_THIS_PLAN_OWNER_DECISION_O1
HARDWARE=NOT_RUN
ACTUATOR_RELEASE=NO
```

## 1. Ziel und Nicht-Ziele

Ziel: Der Eintrag `Home → Einstellungen → Service (PIN)` ist auch ohne
Servicefreigabe antippbar und führt **direkt** zur PIN-Seite. Dort ist der
vorhandene PIN-unabhängige Einstieg „PIN vergessen?“ nach dem bestehenden
Factory-Reset-Owner (`snapshot.factoryReset.available`) verfügbar. Das Öffnen
der Seite erteilt keine Serviceberechtigung.

Nicht-Ziele:

- keine lokale PIN-Eingabe und keine PIN-Verifikation (Teil b, Abschnitt 4,
  Ownerentscheidung O1);
- keine Freischaltung von Service-, Diagnose- oder Recovery-Funktionen; das
  Gate `snapshot.service.available` bleibt für alle geschützten Ziele
  unverändert;
- keine zweite Navigations- oder Auth-Zustandsmaschine, kein PIN-Bypass, kein
  isolierter PIN-Reset, kein Remote-Reset;
- keine Änderung am Factory-Reset-Ablauf B aus #19, an Ablauf A, an
  `AuthenticationDomain`, an GPIO, CI-/Runner-Gates, Servicecharts oder
  Exporten;
- keine Hardwaretests (kein Flash, Werksreset, Powercut oder Peltier);
- keine Korrektur der Trace-Referenzen `SIM-26-21`/`SIM-26-65` (Befund B,
  Abschnitt 7).

## 2. Verifizierte Ausgangslage

### 2.1 Fehlerursache im Code

| Stelle | Befund |
|---|---|
| `lib/fermentation_app/src/fermentation_touch_workspace.cpp:2527-2533` (`pressImpl()`) | Settings-Hit-Test: `FermentationUiSettingsRow::Service` ist nur aktiv, wenn `current.settings->serviceAvailable` |
| `fermentation_touch_workspace.cpp:3168-3170` (`pressSettingsRow()`) | Service-Zeile navigiert nach `NavigateService`, nicht zur PIN-Seite |
| `fermentation_touch_workspace.cpp:3033-3037` (`makeSettingsView()`) | `settings.serviceAvailable = snapshot.service.available`, `serviceReason` aus `unavailableReason` |
| `main/fermentation_ui_renderer.cpp:981-983` | Zeile `service-protected` wird nur bei `settings.serviceAvailable` aktiv dargestellt, sonst mit Grundtext |
| `fermentation_touch_workspace.cpp:924-928` (Route) | Breadcrumb der PIN-Seite ist fest `settings › service › pin` |
| `fermentation_touch_workspace.cpp:1058-1065` (`setCanonicalPageStack()`) | kanonische Stapel `Settings → Service` und `Settings → Service → Pin` |

Zusätzlicher Befund: Im Produktpfad setzt kein Owner `service.available`
(`FermentationUiProjectionInput::service` wird in `FermentationApplication`
nicht befüllt; nur Tests setzen `true`). Auf dem Gerät ist die Service-Zeile
deshalb **immer** gesperrt. Das bestätigt die Ownerbeobachtung aus PR #194.

`FermentationUiSettingsView::serviceAvailable`/`serviceReason`
(`fermentation_touch_workspace.hpp:353-354`) werden nur im Hit-Test, in der
Renderer-Zeile und in einem Test
(`test_local_touch_ui.cpp:2395-2398`) verwendet. `setPage()` hat keinen
Produktaufrufer; es ist eine Testhilfe für die kanonischen Stapel
(`fermentation_touch_workspace.cpp:3276-3279`).

### 2.2 Was bereits korrekt existiert

- PIN-Seite (`fermentation_touch_workspace.cpp:1686-1697`): Slot 1 `cancel`
  (`NavigateBack`), Slot 2 `status`, Slot 3 `forgot-pin`
  (`FactoryResetBegin`, aktiv nur bei `snapshot.factoryReset.available`).
- Service-Seite (`:1670-1685`): `pin`-Slot immer aktiv (R0-h aus #19),
  `recovery` nur bei `service.available`, Grundtext bei gesperrtem Service.
- Diagnose-Seite (`:1659-1668`): Slot 3 `NavigateService`; damit ist die
  Service-Seite auch nach dem Fix über `Status → Diagnose → Service` erreichbar.
- Der Factory-Reset-Ablauf B (#19, PR #191) mit Warning → Confirm → Hold,
  Abbruch über Zurück und `factoryResetAvailableUnlocked()` bleibt der einzige
  Recovery-Owner.

### 2.3 Fehlender PIN-Produktpfad (belegt)

- `device_platform::PinEntryModel` (`lib/device_platform/src/device_ui_pin.hpp`)
  ist im Produkt nirgends verwendet.
- `AuthenticationDomain::verifyServicePin` wird nur intern von
  `changeServicePin` aufgerufen; `FermentationApplication` hat keine
  lokale PIN-Prüf-Methode (`docs/audits/ISSUE19_R0_RESET_PREFLIGHT.md`, Punkt a).
- Eine Service-PIN entsteht nur über die Web-Provisionierung
  (`FermentationApplication::provisionWebAccess`). Ohne Auth-Record liefert
  `verifyServicePin` `RecoveryRequired`.
- `verifyServicePin` schreibt Fehlversuche und Lockout persistent
  (`authentication_records.cpp:891-935`).
- Die PIN-Seite zeigt heute nur den Hinweis `zurückgestellt (#28)`
  (`docs/LOCAL_UI_SETTINGS_SERVICE.md:61-63`). Das bleibt nach diesem Plan so.

### 2.4 Vertragsbasis

- `docs/LOCAL_UI_SETTINGS_SERVICE.md`: Service vierstellig PIN-geschützt;
  „PIN vergessen?“ auf der PIN-Seite ohne PIN-Eingabe erreichbar (O-R1 = B+);
  kein isolierter PIN-Bypass; 10 Minuten Inaktivitätssperre des Service.
- `docs/tasks/issue-19-journals-retention-backup-import-plan.md`: O-R1 = B+,
  O-R2 = B (Ablauf A erst mit produktivem lokalem Service-/PIN-Zugang,
  bestehende `PinEntryModel`-/`verifyServicePin`-Verträge, keine provisorische
  PIN-Lösung); SIM-R-01, SIM-R-14.
- #28: Owner des geführten PIN-Serviceablaufs.
- #172 / PR #187: Ursprung der Settings-Integration (D14).

## 3. Teil a – Umsetzung jetzt

### 3.1 Änderungen

1. `pressImpl()`: die Bedingung `row == FermentationUiSettingsRow::Service`
   → `enabled = current.settings->serviceAvailable` entfällt; die Zeile ist
   wie Sprache, Zeit, Netzwerk und Webzugang immer ein Ziel.
2. `pressSettingsRow()`: `Service` navigiert mit dem bestehenden
   `navigate(FermentationUiWorkspaceSlotAction::NavigatePin)` direkt zur
   PIN-Seite. Der Stapel ist danach `Home → Settings → Pin`; Zurück/Abbrechen
   auf der PIN-Seite führt zu `Einstellungen`.
3. Renderer (`fermentation_ui_renderer.cpp:979-984`): Die Zeile
   `service-protected` wird immer aktiv dargestellt, ohne Grundtext (O2).
4. `FermentationUiSettingsView::serviceAvailable`/`serviceReason` und ihre
   Befüllung in `makeSettingsView()` entfallen, weil sie danach keinen
   Verwender mehr haben.
5. Route der PIN-Seite: `settings › pin`. Route der Service-Seite:
   `status › diagnostics › service` (O3).
6. `setCanonicalPageStack()`: `Pin` → `Home, Settings, Pin`; `Service` →
   `Home, Status, Diagnostics, Service` (O3).

Unverändert: Service-, Diagnose-, PIN- und FactoryReset-Seiten-Slots, die
Bindung von `forgot-pin` an `snapshot.factoryReset.available`, die Bindung von
`recovery` an `service.available`, der Ablauf B und alle Application-Owner.

### 3.2 Abbildung auf die Akzeptanz aus #188 A

| #188-A-Kriterium | Durch Teil a |
|---|---|
| 1 Einstieg erreichbar, direkt zur PIN-Seite | **teilweise**: die PIN-Seite öffnet direkt; eine **nutzbare PIN-Abfrage** gibt es erst mit Teil b. Die Seite zeigt weiter den Hinweis `zurückgestellt (#28)` und wird nirgends als PIN-Anmeldung bezeichnet |
| 2 „PIN vergessen?“ ohne PIN, nur Ablauf B | erfüllt (bestehender Slot, bestehender Owner) |
| 3 keine Serviceberechtigung durch Öffnen | erfüllt (keine Owner- oder Gate-Änderung) |
| 4 Regression über echten Hit-Test/Dispatcher | erfüllt (Abschnitt 5) |
| 5 Hardware-Reverifikation | offen, separater Schritt nach Merge (Abschnitt 6) |

## 4. Teil b – was für eine nutzbare PIN-Abfrage fehlt (Ownerentscheidung O1)

Nicht Teil dieses Plans. Fakten für die Entscheidung:

- **UI:** Ziffernfeld, Löschen, Abbrechen, Bestätigen und maskierte Anzeige
  auf der PIN-Seite; Zustand über `PinEntryModel` im Workspace. Neues Layout
  im Renderer.
- **Application-Grenze:** eine Methode wie „lokale Service-PIN prüfen“ unter
  `ApplicationCallSerializer` und `AuthOperationGate`, die
  `AuthenticationDomain::verifyServicePin` mit Kontext, `nowMs` und
  `retryAfterMs` aufruft und `AuthCheckStatus` (`Authenticated`, `Invalid`,
  `LockedOut`, `RecoveryRequired`, `KdfUnavailable`) auf `PinEntryOwnerState`
  abbildet. Der Kandidat bleibt transient (`PinEntryModel::candidate()`).
- **Persistenz:** Jede Prüfung schreibt Lockout und Sequenz persistent; das
  ist ein Auth-Write und betrifft SIM-R-01 („Auth-Records ändern sich nur durch
  eine tatsächlich erfolgte PIN-Prüfung“).
- **Servicefreigabe:** Erst eine lokale Service-Lease mit 10 Minuten
  Inaktivitätssperre und Lauf-/Safety-Gating setzt `service.available`. Das
  ist der geführte PIN-Serviceablauf von #28.
- **Ablauf A (#19, O-R2 = B):** verbraucht nur das Ergebnis „PIN verifiziert“.
- **Unprovisioniertes Gerät:** ohne Web-Provisionierung keine Service-PIN;
  der Benutzer kann dann nur „PIN vergessen?“ nutzen.

Empfehlung zu O1: Teil b als eigene, später freizugebende Planrevision
gemeinsam mit dem minimalen #28-Service-Lease-Schnitt und Ablauf A, nicht in
diesem PR. Das ist eine materielle Auth-/Security-Erweiterung.

## 5. Tests

Alle Tests im Implementierungsschnitt C1. Neue und umgestellte Pfade laufen
über den echten Hit-Test und Dispatcher (`processWorkspaceTouch`, Koordinaten
aus dem gerenderten Screen über `targetAt`, jede Berührung als frische Kante
mit Loslassen), nicht über `setPage(Pin)`.

### 5.1 Neu (`test/test_press_dispatcher`)

`test_settings_service_row_opens_the_pin_page_through_touch` mit
`OwningAppFixture` (Produkt-Default `service.available == false`):

1. Home-Slot 3 → `Settings`; `down` antippen, bis die Service-Zeile
   sichtbar ist; Content-Zelle der Service-Zeile antippen → `Pin`.
2. `uiSnapshot().service.available` bleibt `false`; Pin-Slot 3 ist
   `FactoryResetBegin` und aktiv.
3. „PIN vergessen?“ → `FactoryReset`, Stufe `Warning`.
4. Slot 0 `cancel` (`FactoryResetCancel`, bestehend
   `fermentation_touch_workspace.cpp:2286-2290`: Cancel-Kommando und
   `goBack()`) → Ablauf `Idle`, Seite `Pin`; danach Slot 1 `cancel` der
   PIN-Seite → `Settings`.
5. Kein Reset: Storage-Epoch per bestehendem `epochOf`-Muster
   (`ConfigurationBootstrapStore(store).scan()`) vor und nach unverändert.

`test_settings_service_row_keeps_service_locked_with_provisioned_pin` mit
`WebAccessFixture` und `provisionWebAccess(..., "1234")`:

- derselbe Touchweg bis `Pin`; `service.available` bleibt `false`;
  Service-/Recovery-Ziele bleiben gesperrt;
- der Auth-Credential-Record (über `AuthenticationRecordStore::readCredentials`)
  ist vor und nach Navigation, „PIN vergessen?“ und Abbruch gleich, inklusive
  `servicePinLockout`.

`factoryReset.available == false` über `setFactoryResetHoldMillis(std::nullopt)`:
derselbe Touchweg bis `Pin`; Slot 3 ist deaktiviert; Antippen bewirkt nichts.

Grenze: Ein aktiver Auth-Lockout-Zustand ist ohne neue Testinfrastruktur
nicht herstellbar (kein lokaler Prüfpfad). Er wird in diesem Plan nicht
nachgewiesen; es wird keine neue Store- oder Auth-Testhilfe gebaut.

### 5.2 Umgestellt

| Test | Änderung |
|---|---|
| `test_press_dispatcher::test_forgot_pin_entry_runs_the_whole_flow_through_touch` | Einstieg über den echten Touchweg Home → Settings → Service-Zeile → Pin statt `setPage(Service)`/`setPage(Pin)`; restlicher Ablauf unverändert |
| `test_local_touch_ui::test_standby_slot_three_opens_settings_and_service_lives_below_it` | Stapel `Home → Settings → Pin` |
| `test_local_touch_ui::test_settings_rows_are_in_the_decided_order_and_open_their_pages` | Zeile 5 öffnet `Pin` |
| `test_local_touch_ui::test_settings_service_and_device_name_rows_state_when_disabled` | Service-Zeile ist auch bei `service.available == false` ein Ziel und öffnet `Pin`; Gerätename-Teil unverändert |
| `test_renderer_boundary::test_settings_disabled_rows_show_their_reason` | Service-Zeile aktiv, kein „Service unavailable“ auf der Einstellungen-Seite; Gerätename-Grund unverändert |
| 4 Aufrufer von `setPage(Service)` (1 `test_local_touch_ui`, 1 `test_press_dispatcher`, 2 `test_renderer_boundary`) | nur falls Assertions auf Stapel/Route der Service-Seite beruhen, an `Status → Diagnose → Service` anpassen |

Unverändert grün bleiben müssen u. a.
`test_renderer_boundary::test_deferred_pages_show_the_hint_in_all_locales_and_keep_owner_reason`
und `test_press_dispatcher::test_safe_boot_entry_and_every_exit_end_the_flow_without_a_reset`.

### 5.3 Ausführung

- Native: `test_local_touch_ui`, `test_press_dispatcher`,
  `test_renderer_boundary` (Ergebniszahlen im Nachweis).
- ESP-IDF-Builds `esp32_bringup` und `esp32_release` (Renderer unter `main/`).
- Builder-Self-Check (`scripts/run_pre_ready_gates.sh self-check`) vor dem
  abschließenden Independent Full Review.

## 6. Hardware und Abgrenzung

Keine Hardware in diesem PR. `HW-19-R01` bleibt nicht bestanden,
`HW-19-R02=NOT_RUN`, `HW-19-R03=BLOCKED`. Nach Merge folgt eine separat
freizugebende Gerätereverifikation über #192 (Navigation Einstellungen → PIN →
„PIN vergessen?“ → Warnung → Abbruch, ohne Werksreset). `ACTUATOR_RELEASE=NO`.

## 7. Befund B – getrennt halten

`docs/ACCEPTANCE_TESTS.md` SIM-26-21 und SIM-26-65 bleiben in diesem PR
unverändert; ihr offener Status in #188 bleibt erhalten.

Tracking-Empfehlung (O4): ein eigenes Doku-Issue „SIM-26-21/65 auf
existierende Testpfade korrigieren“ mit Verweis auf #188 B und ein kleiner
Markdown-only-PR, der beide Referenzen gegen die tatsächliche Testsemantik
prüft und Lücken ehrlich markiert. #188 wird erst geschlossen, wenn A
(einschließlich Hardware-Reverifikation) und B nachvollziehbar erledigt oder
übergeben sind. Das Issue legt der Owner an.

## 8. Commit-Schnitte

Nach jedem Commit wird angehalten.

| Schnitt | Inhalt | Nachweis |
|---|---|---|
| C1 | Abschnitt 3.1 Punkte 1–6 und alle Tests aus Abschnitt 5 (atomar, da Tests das alte Verhalten prüfen) | Native-Suites und beide ESP-Profile |
| C2 | Doku: `docs/LOCAL_UI_SETTINGS_SERVICE.md:56-63` („Service“ öffnet direkt die PIN-Seite; die PIN-Seite bietet „PIN vergessen?“, eine PIN-Prüfung ist noch nicht vorhanden); `docs/ACCEPTANCE_TESTS.md` Trace-Zeilen SIM-172-S10-01 (Service-Zeile ist kein deaktivierter Eintrag mehr, Testnamen) und SIM-19-R07 (neue Tests); `docs/ROADMAP.md` | Doku-Diff, `git diff --check` |

SIM-26-21 und SIM-26-65 werden in C2 nicht angefasst.

## 9. Offene Ownerentscheidungen

- [ ] O0: Freigabe dieser Plan-SHA.
- [ ] O1: Teil b (PIN-Eingabe und lokale Verifikation) – Empfehlung: eigene
      spätere Planrevision mit #28-Service-Lease und Ablauf A; nicht in diesem
      PR.
- [ ] O2: Darstellung der Service-Zeile – Empfehlung: immer aktiv, ohne
      Grundtext; der Sperrgrund bleibt auf der Service-Seite sichtbar.
      Alternative: aktiv mit Grundtext als Zweitzeile.
- [ ] O3: Service-Seite nur noch über `Status → Diagnose → Service`
      (Route und kanonischer Stapel entsprechend) – Empfehlung: ja, weil
      `Einstellungen` danach nicht mehr zur Service-Seite führt.
      Alternative: Stapel/Route der Service-Seite unverändert lassen.
- [ ] O4: Tracking von Befund B (Abschnitt 7).

## 10. Risiken

- Benutzer könnten die geöffnete PIN-Seite für eine funktionierende Anmeldung
  halten; abgesichert durch den unveränderten Hinweis `zurückgestellt (#28)`
  und die ehrliche Abbildung in 3.2.
- Ein zu breiter Eingriff in Stapel/Routen der Service-Seite; begrenzt auf
  O3 und die vier genannten Testaufrufer.
