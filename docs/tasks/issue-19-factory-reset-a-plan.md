# Issue #19 – Normaler Werksreset A (PIN-geschuetzt, nur fehlendes Delta) (Plan)

Status: `PLAN_AWAITING_OWNER_APPROVAL`. Keine Implementierung vor Ownerfreigabe
der exakten Plan-SHA. Basis: `main` bei `93116d2`. Scope: ausschliesslich
Ablauf A des Werksresets aus Issue #19; ergaenzt Plan Revision 8
(`issue-19-journals-retention-backup-import-plan.md`), die A ausdruecklich
ausschliesst (O-R2 = B, "erst mit produktivem lokalen Service-UI/PIN-Zugang").
Dieser Zugang existiert seit Issue #188 A (PR #199).

## 1. Ziel und Nicht-Ziele

Ziel: Der bestehende lokale, PIN-geschuetzte Servicebereich bietet
"Vollstaendiger Werksreset" an und fuehrt ihn durch den **vorhandenen**
Warnungs-/Bestaetigungs-/Hold-/Ergebnisablauf und den **vorhandenen**
Resetkern. Ablauf B (PIN-unabhaengig, `SAFE_BOOT`, `ResetEligibleNoRuntime`),
Resetkern, Netzwerk-/HTTP-Stopp, Epochen- und Touchkalibrierungsvertraege
bleiben funktional unveraendert.

Nicht-Ziele: Journal, Historie, Bereinigung, Backup/Import, Web-/Remote-Reset
(SIM-19-R06 bleibt gueltig), Hardware, GPIO, Aktorfreigaben
(`ACTUATOR_RELEASE=NO`), neue Schichten/Tasks, eigene PIN- oder Reset-Modelle,
zusaetzliche Servicefunktionen, parallele Reset-UI, Zweckentfremdung des
`recovery`-Slots (Lauf-Recovery). Bei materieller Aenderung an B-/Safety-/
Recovery-Vertraegen: stoppen und Befund vorlegen.

## 2. Verifizierte Ausgangslage (`93116d2`)

| Befund | Quelle |
|---|---|
| `FactoryResetFlow` kennt `PinProtected`, Stufe `PinRequired` und `pinVerified()` bereits; `begin(PinProtected)` -> `PinRequired`, `pinVerified()` -> `Warning` | `factory_reset_flow.hpp/.cpp` |
| `beginFactoryReset` lehnt `PinProtected` hart ab ("bis lokale PIN-Pruefung existiert") | `fermentation_application.cpp` `beginFactoryReset` |
| Lokale Service-Freigabe existiert: `verifyLocalServicePin` -> `localServiceLease_`; `localServiceAvailableUnlocked()` = Lease aktiv **und** validierter `STANDBY` (`localServiceEntryAllowedUnlocked`: Ready, kein Lauf, keine Recoverydecision, Epoche geladen) | `fermentation_application.cpp` |
| Lease endet bei Sign-out, Inaktivitaet (Touch-Policy 10 min, kein Absolutlimit), Verlassen des validierten `STANDBY` und bei Ersatz der Authentisierungsdomaene | `endLocalServiceSession`, `resetAuthenticationState`, `fermentation_ui_models.cpp` |
| `updateFactoryResetHold` prueft `factoryResetAvailableUnlocked()` erneut unter dem `ApplicationCallSerializer` unmittelbar vor `beginAuthorizedFactoryReset()`; Abweisung -> `Rejected`, Kern nicht aufgerufen | `updateFactoryResetHold` |
| Service-Seite (`FermentationUiPage::Service`): Slot 0 Zurueck, Slot 1 Sign-out/PIN, Slot 2 Status, Slot 3 `recovery` (Lauf-Recovery, aktiv bei Lease). **Kein freier Slot, kein Werksreset-Eintrag** | `fermentation_touch_workspace.cpp` |
| `FermentationUiFactoryResetCommand::Step::Begin` und die Slot-Aktion `FactoryResetBegin` fuehren hart zu `PinIndependent`; Seite `FactoryReset` und Hold-Slot sind kind-unabhaengig und werden von B bereits genutzt | `fermentation_ui_commands.cpp`, Workspace |
| Aktivitaet verlaengert die Lease nur auf den Seiten `Service`/`Recovery` (`noteLocalServiceActivity` im Dispatcher) | `main/fermentation_ui_press_dispatcher.cpp` |
| `FermentationFactoryResetView.available` meint nur Ablauf B | `factory_reset_flow.hpp` |

## 3. Betroffene Module, Vertraege, Gates

- `fermentation_app` (ADR-013): `FermentationApplication` (Owner), `FactoryResetFlow`
  (unveraendert), Command-/Workspace-Schicht, ein Dispatcher-Detail in `main/`.
- Unberuehrt: Resetkern (`ConfigurationRecoveryService`), Persistenz,
  Netzwerk-/HTTP-Stopp, Authentisierungs-/PIN-Records, Epochenvertrag,
  Touchkalibrierung, Web.
- Sicherheitsgrundsatz: fail-closed. Keine Aktorwirkung, kein Hardwarezugriff.

## 4. Schnitte (je Commit anhalten, Ownerfreigabe)

**A1 – Application-Bindung (kein UI).**

- `beginFactoryReset(PinProtected)`: nur wenn `localServiceAvailableUnlocked()`
  **und** `factoryResetAvailableUnlocked()` unter demselben
  `ApplicationCallSerializer`; danach unmittelbar `begin(PinProtected)` und
  `pinVerified()` (Lease ist der Nachweis; keine PIN-Daten im Flow, keine
  zusaetzliche PIN-Ablage, kein Bypass). Der Ablauf steht danach in `Warning`.
- Lease-Bindung in jedem Schritt eines A-Ablaufs: `acknowledgeFactoryReset`
  und `updateFactoryResetHold` pruefen bei `kind == PinProtected` zuerst
  `localServiceAvailableUnlocked()`; bei Verlust wird der Ablauf ohne Wirkung
  abgebrochen (`cancel()`), der Schritt abgelehnt. Die bestehende
  Wiedervalidierung unmittelbar vor `beginAuthorizedFactoryReset()` wird um
  genau diese Lease-Bedingung fuer A erweitert (Ergebnis `Rejected`, Kern nicht
  aufgerufen). B-Pfade bleiben bytegleich im Verhalten.
- `FermentationFactoryResetView`: ein neues Feld `pinProtectedAvailable`
  (`!active && factoryResetAvailableUnlocked() && localServiceAvailableUnlocked()`),
  in `operator==` aufgenommen. `available` bleibt B.
- Tests (nativ, `test_factory_reset_flow`): siehe Abschnitt 5.

**A2 – UI-Anbindung.**

- Kind-tragender Beginn: `FermentationUiFactoryResetCommand` erhaelt
  `Step::BeginPinProtected` (Bestehendes `Begin` bleibt B); neue Slot-Aktion
  `FactoryResetBeginPinProtected` navigiert wie `FactoryResetBegin` zur
  vorhandenen Seite `FactoryReset`. Warnung/Bestaetigung/Hold/Ergebnis,
  Texte (`factory-reset` ist in de/en/es vorhanden) und Hold-Slot werden
  unveraendert wiederverwendet.
- Service-Seite (Lease aktiv): der Eintrag "Werksreset" bekommt einen Slot
  (Entscheidung D-A1), aktiv nur bei `pinProtectedAvailable`. Der
  `recovery`-Slot bleibt unveraendert Lauf-Recovery. Gesperrte Service-Seite
  (kein Lease) und PIN-Seite bleiben unveraendert ("PIN vergessen?" = B).
- Dispatcher: `noteLocalServiceActivity()` zusaetzlich auf der Seite
  `FactoryReset`, **nur** wenn `snapshot.factoryReset.kind == PinProtected`,
  damit eine laufende Bestaetigung die Lease nicht ablaufen laesst (Fehlerfall
  bleibt fail-closed, A1).
- Tests (nativ, `test_press_dispatcher`): siehe Abschnitt 5.

**A3 – Dokumentation.** Nur betroffene Eintraege: `docs/ACCEPTANCE_TESTS.md`
(neue Zeilen `SIM-19-A-01..`; `SIM-19-R02`/`R03`/`R07` ohne "A nicht
angeboten" nachfuehren), Status-Zeilen in
`docs/tasks/issue-19-journals-retention-backup-import-plan.md` (A umgesetzt,
Verweis auf diesen Plan; `IMPLEMENTATION_SCOPE_AUTHORIZED` nennt A erst nach
Freigabe dieser Plan-SHA), `docs/ROADMAP.md`. Hardware `NOT_RUN`.

## 5. Nachweis (nativ, ohne reale Reset-Ausfuehrung)

| ID | Pruefung |
|---|---|
| SIM-19-A-01 | Aktive Lease im validierten `STANDBY`: `pinProtectedAvailable`, A beginnt in `Warning`, vollstaendiger Ablauf bis `Finished`, **genau ein** Kernaufruf, Epoche +1, Netzwerk-/HTTP-Reihenfolge wie B |
| SIM-19-A-02 | Ohne Lease, nach falscher PIN, bei PIN-Sperre, nach Sign-out, nach Timeout: `beginFactoryReset(PinProtected) == false`, kein Slot aktiv, keine Store-Writes ausser tatsaechlich erfolgter PIN-Pruefung (Lockout) |
| SIM-19-A-03 | Lease endet **zwischen** Beginn/Warnung/Bestaetigung/Hold-Beginn und Hold-Ablauf (Sign-out, Timeout per virtueller Zeit, Lifecycle-/Run-/Recovery-Wechsel): Schritt abgelehnt bzw. `Rejected`, Kern nicht aufgerufen, keine Store-Aenderung |
| SIM-19-A-04 | Abbruch in jeder Stufe und unterbrochener Hold (Loslassen, Pause, Verschieben) ohne Reset-Write; kurzer Neukontakt loest nicht aus (Muster `SIM-19-R08`, echte `touchLoopAction`) |
| SIM-19-A-05 | UI ueber den echten Touch-Adapter: Service-Seite -> Werksreset -> Warnung -> Bestaetigung -> Hold; `recovery`-Slot bleibt Lauf-Recovery und startet nie den Reset; B-Eintraege ("PIN vergessen?", `SAFE_BOOT`, S1) unveraendert |
| Regression | `SIM-19-R01..R08`, `SIM-19-S1-01..06`, lokale PIN-/Lockout-Tests (`test_authentication_records`, #188-Servicetests in `test_press_dispatcher`), Touchkalibrierung, Netzwerk-/Epochentests; der Test "nur Variante B wird angeboten" wird durch SIM-19-A-01/02 ersetzt |
| Mutationsproben | ohne Lease-Pruefung vor dem Kernaufruf schlaegt SIM-19-A-03 fehl; ohne `Begin`-Lease-Pruefung schlaegt SIM-19-A-02 fehl |

Gezielte lokale Tests im Draft: `test_factory_reset_flow`, `test_press_dispatcher`,
direkt betroffene Konsumenten (`test_configuration_recovery_service`,
`test_web_application_routes` fuer `test_no_http_route_reaches_the_local_factory_reset`,
UI-Command-/Workspace-Tests), Architekturcheck, `self-check` mit
esp-clang 21, ESP-IDF-Build. Voller Pre-Ready erst nach Independent Review,
`OPEN_BLOCKERS=0` und Ownerfreigabe. Nicht ausgefuehrte Tests gelten als nicht
bestanden; Hardware `NOT_RUN`.

## 6. Offene Ownerentscheidung

| ID | Frage | Empfehlung |
|---|---|---|
| D-A1 | Die Service-Seite hat vier belegte Slots (Zurueck, Sign-out, Status, `recovery`). Wo wird "Werksreset" bedient? | **A:** Slot 2 "Status" wird auf der **entsperrten** Service-Seite durch "Werksreset" ersetzt (Status bleibt ueber Diagnose/Startseite erreichbar; gesperrte Seite unveraendert). **B:** Status bleibt, "Werksreset" erst auf einer zusaetzlichen Unterseite (mehr Umfang). **C:** `recovery`-Slot teilen (laut Auftrag ausgeschlossen). Empfehlung A. |

## 7. Risiken

- Lease-Ablauf mitten im Ablauf bricht fail-closed ab (gewollt); Verlaengerung
  nur durch Aktivitaet auf der Reset-Seite (A2).
- Aenderung der Service-Seite beruehrt bestehende Slot-Vertragstests (#188);
  sie werden gezielt nachgezogen, nicht aufgeweicht.
- Hardware-Nachweis fehlt (`NOT_RUN`); gehoert zum Folgeissue #192.
