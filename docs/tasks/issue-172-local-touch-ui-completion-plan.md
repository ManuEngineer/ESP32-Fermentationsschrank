# Issue #172 – R1 lokale Touch-UI funktional vervollstaendigen

## Planstatus und Baseline

```text
ISSUE=172
SCOPE=R1_LOCAL_TOUCH_UI_FUNCTIONAL_COMPLETION
BASE_BRANCH=main
BASE_SHA=02b7523b7dc3fdc82583c7939ecbab9eb9ec5dd7
PLAN_REVISION=3
PLAN_STATUS=REBASED_AND_REVALIDATED_AGAINST_MERGED_PR170_WAITING_INDEPENDENT_PLAN_REVIEW
IMPLEMENTATION=NOT_STARTED
IMPLEMENTATION_AUTHORIZATION=NO
PR171=MERGED
PR170=MERGED
PR170_MERGE_COMMIT=02b7523b7dc3fdc82583c7939ecbab9eb9ec5dd7
PR174=MERGED
PR167=SUPERSEDED_REFERENCE_ONLY
PR175_REBASE=PASS
PLAN_REVALIDATION_AGAINST_MERGED_PR170=PASS
PRODUCTION_CODE_CHANGED=NO
OWNER_DECISIONS_PENDING=9
OWNER_DECISIONS_O1_TO_O9=PENDING
OWNER_RECOMMENDATIONS_O1_TO_O9=RECOMMENDED_PENDING_OWNER_CONFIRMATION
PR175_CORRECTION_ORDER=B1_TO_B6_INCORPORATED
ACTUATOR_RELEASE=NO
```

Dieser Plan ist das Ergebnis des Audits aus dem Auftrag
`Issue172_Plan_Audit_Auftrag.md`. Er enthaelt **keine** Produktimplementation.
Revision 3 ist der auf den gemergten PR #170 (`02b7523b…`) rebasete und gegen
den tatsaechlichen Code revalidierte Stand (Abschnitt 9). Die Ownerempfehlungen
O1–O9 bleiben **empfohlene Entscheidungen, Owner-Bestaetigung ausstehend**; sie
sind keine beschlossenen Ownerentscheide. Die Umsetzung beginnt erst nach
Independent Plan Review und ausdruecklicher Ownerfreigabe der exakten Plan-SHA.
Dieser PR ist ein **reiner Plan-PR** (Praezedenzfall PR #171); die
Implementations-PRs entstehen danach von `main` (Abschnitt 6). Zeilenbezuege in
Abschnitt 2 gelten fuer `BASE_SHA`.

## 1. Ziel und Nicht-Ziele

Ziel: Jede auf R1-Hardware sichtbare, aktivierte Schaltflaeche und jede
verlangte lokale Eingabe erreicht ihren bereits bestehenden Fach-/Config-/
Persistenz-Owner, oder sie ist bewusst deaktiviert bzw. zurueckgestellt und
nennt einen Grund. Es entsteht keine neue Fach-, Config-, ProgramCatalog-,
Message-, Recovery- oder Persistenzwahrheit.

Nicht-Ziele (unveraendert aus dem Issue):

- #164: WLAN-/QR-/Browser-Setup und dessen Hardware-Evidence.
- #27 / PR #170 (gemergt): Web, API, Auth, Session, CSRF. `HeaderWebAccess`,
  `OpenWebProvisioningWindow`, `webAccessState()` und der Auth-/Session-Owner
  bleiben unberuehrt; #172 baut keine zweite Web-/Auth-/Provisionierungslogik.
- #28: Diagnose, Service, PIN-Ablauf, Charts, Exporte.
- #30/#32/#33/#34/#35: Sensor-, Ausgangs-, Peltier-, Commissioningwerte.
- Keine UI-Plattform, kein Widget-/Event-/Formframework, keine neue ADR.
- Keine Werte aus `TBD_HARDWARE`, `TBD_COMMISSIONING`,
  `TBD_IMPLEMENTATION_BUDGET` als Laufzeitwert.

## 2. Verifizierte Ausgangslage (Stand `BASE_SHA`, nach PR #170)

### 2.1 Strukturbefunde

| Nr. | Befund | Nachweis |
|---|---|---|
| F1 | `targetAt()` kennt `HeaderLanguage` (seit #170: `kHeaderLanguageHitRect{176,0,44,32}`, ueberlappungsfrei zu `HeaderNetwork`, Randpixel-Test vorhanden), `HeaderNetwork` (Rect 220/4/44/18) und die vier BottomSlots. `HeaderClock` ist im Renderer gezeichnet (Uhr x=264..316), aber **nicht treffbar**. Es gibt kein Content-Target. | `main/fermentation_ui_renderer.cpp:23-24`, `:351-359`, `:632-660`; `test_renderer_boundary.cpp:130-140` |
| F2 | `DeviceUiTargetKind` kennt `None, HeaderLanguage, HeaderNetwork, HeaderClock, BottomSlot, HomeOrBack, PagerUp, PagerDown, Confirm, Cancel, Back`. Kein Inhalts-/Zeilentarget. | `lib/device_platform/src/device_ui_interaction.hpp:10-22` |
| F3 | Alle zwoelf Workspace-Setter (`selectProgram`, `setStartCandidate`, `setManualHoldingValues`, `setManualTimedValues`, `setStopCoolingPlan`, `setCompletionCoolingPlan`, `setSelectedMessage`, `setProgramEditCandidate`, `setProgramEditOperation`, `setProgramEditDirty`, `setSensorSelectionAction`, `setRecoveryTimeCorrectionSeconds`) haben **0 produktive Aufrufer** (Suche in `main/`, `src/`, `lib/`), nur Testaufrufer. | Grep, Audit-Lauf |
| F4 | `NumericEditModel` / `TextEditModel` haben ausserhalb von `fermentation_ui_editing.*` und Tests keinen Konsumenten. `NumericEditAction::Plus/Minus` sind **Vorzeichenaktionen**, keine Schrittweiten. | `fermentation_ui_editing.cpp:48-93` |
| F5 | Der Dispatcher meldet `transitionAction` und `programEdit` als `UnavailableNoOwner`; alle anderen Press-Payloads sind verdrahtet. | `main/fermentation_ui_press_dispatcher.cpp:89-99` |
| F6 | Die Programmliste zeichnet immer die Eintraege 0..2 (18-px-Zeilen) und ignoriert `pager.currentIndex`; Auf/Ab aendert nur den Zaehler `n/N`. 18 px sind fuer resistives Touch zu klein (BottomSlots: 40 px). | `fermentation_ui_renderer.cpp:480-492`, `:523-530` |
| F7 | `NavigateMessageDetail` waehlt stets die **erste** aktive, unaufgeloeste Meldung und ignoriert den Pager. | `fermentation_touch_workspace.cpp:1160-1172` |
| F8 | Der Zeitzonenkatalog enthaelt genau einen Eintrag (`Europe/Zurich`); `PreparedTimeZone` traegt nur den Bezeichner, keinen Offset; der Port enthaelt ausdruecklich keine Zeitzonendatenbank. Der Header zeigt UTC (`formatClockText`). Eine Zeitzonen-„Auswahl“ waere ein No-op, obwohl das Issue die Aenderung der Zeitzone in normalen Einstellungen zulaesst. Die unmarkierte UTC-Anzeige darf nicht bleiben; S4 kennzeichnet sie (`HH:MMZ`). | `firmware_configuration_catalog.cpp:8-9`, `time_zone_resolver.hpp:15-26`, `fermentation_ui_renderer.cpp:185-198` |
| F9 | `FermentationUiPresentationSource` enthaelt `displayLocale`, `canonicalTimeZoneId`, `programCatalog`, aber **kein** `deviceName`. | `fermentation_ui_models.hpp:145-149` |
| F10 | `app_main` uebergibt `initialDisplayLocale` / `initialTimeZoneId` (beim Boot erfasst) an `renderGate.renderRequired()` und als Locale/Zeitzone der Netzwerkseite. Auf `HeaderNetwork` ist die Presentation-Kopie verdraengt; `get()` liefert dort die Defaults (Englisch). Nach einem Sprachwechsel zeichnet der Render-Key nicht neu bzw. die Netzwerkseite bliebe bis zum Reboot in der alten Sprache. | `main/app_main.cpp:442-443`, `:482-487`, `:511-532`; `fermentation_ui_presentation_cache.hpp` (`update`, `evict`, `get`) |
| F11 | Es gibt keine Settings-/Menue-Seite in `FermentationUiPage`. Die vier Home-Slots sind belegt (Standby: `start` und `programs` zeigen beide auf die Programmliste). | `fermentation_touch_workspace.cpp:317-331` |
| F12 | `SensorSelectionUserAction` wird nur von `setSensorSelectionAction()` gesetzt. Es gibt **keine reine Abfrage** „zulaessige Aktionen“: die Zulaessigkeit entscheidet `decideApplySensorSelectionAction` ueber `applySensorSelectionDecision` mit Program-Kontext, Plausibilitaet und `criticalSafetyEventPending`; `CommandDecision` traegt einen kompletten `RunCommandState`. `MessageView` traegt aber `code`/`decisionRequired` (`MessageCode::UserDecisionRequired`). | `run_commands.cpp:1343-1418`, `fermentation_ui_models.hpp:58-60` |
| F13 | Die Textpacks (DE/ES/EN) existieren; jede neue Taste braucht Eintraege in allen drei Packs; die Tabellengroesse ist als Literal in drei Arrays kodiert und betraegt seit #170 `70U` (fuenf `web-access*`-Schluessel). | `fermentation_ui_text.cpp:22`, `:102`, `:184` |
| F14 | **Latenter Defekt (durch Code-Lesung belegt, nicht ausgefuehrt):** `applyProgramEditPreview` installiert mit `{LocalDisplay, 0U}` und `{NormalEdit\|StandardProgramReset, 0U}`. `validChangeOrigin/Operation` verlangen `LocalDisplay==2U`, `NormalEdit==1U`, `StandardProgramReset==6U`; `encodeConfigurationManifestPayload` lehnt jedes Manifest mit `!isPlausible` ab. Kein Test bestaetigt ein von `applyProgramEditPreview` erzeugtes Preview (die Tests brechen es ab oder lehnen es vorher ab), es gibt keinen produktiven Aufrufer. Ein Commit eines solchen Previews scheitert daher voraussichtlich bei der Persistierung. `applyNetworkMode` nutzt die kanonischen Werte `{LocalDisplay,2U}`/`{NormalEdit,1U}`. | `fermentation_ui_editing.cpp` (`applyProgramEditPreview`), `configuration_graph.cpp:28-58,108-138`, `configuration_graph_codec.cpp:88-100`, `test_configuration_service.cpp:773-790,833-880` |
| F15 | Die Prozess-State-Machine hat in `FermentationApplication` **keinen zyklischen Pfad**: `update()` pollt nur `networkLifecycle_` und `reevaluateWaitingForTrustedTime()`. Das Application-Objekt komponiert weder `TemperatureControlApplicationOrchestrator` noch einen Fault-/Signalproduzenten. `ProcessSignals{}` ist die einzige bestehende Signalquelle (acht Stellen im `RunPersistenceCoordinator`). Der einzige Eintritt in `WaitingForProduct` ist die automatische Entscheidung `PreheatQualified` aus `Preheating`, die `signals.qualificationProgress == Complete` verlangt (`process_state_machine.cpp:417-421`), sowie `RecoveryResume` einer zuvor persistierten Wartephase (`:680`). `StartRun` fuehrt nicht direkt dorthin. Folge: Ohne Signalproduzent/Regelkreis (#30/#35) ist `WaitingForProduct` im Produktbuild nur nach einer wiederhergestellten Wartephase erreichbar. | `fermentation_application.cpp:1861-1866`, `process_state_machine.cpp:417-421,680`, `run_persistence_coordinator.cpp:1237,1285,1475,1503,1836` |
| F16 | **Abgrenzung Tastatur (kein Widerspruch):** `LOCAL_UI_PROGRAMS.md` verlangt fuer R1 eine lokale Bildschirmtastatur fuer Programmname und Notiz. `FUTURE_SCOPE.md` (`R1_TOUCH_WIFI_KEYBOARD=DEFERRED`) deferiert ausdruecklich nur die **HOME_WIFI-Credentialeingabe und die dafuer erforderliche Tastatur**, nicht jede Bildschirmtastatur. Die Tastatur fuer Programmname/Notiz/Geraetename ist daher R1-Scope von #172; die WLAN-Credentialtastatur bleibt deferred und wird nicht aufgenommen. | `docs/LOCAL_UI_PROGRAMS.md` (Abschnitt „Bildschirmtastatur“), `docs/FUTURE_SCOPE.md:74-82` |
| F17 | **Vorbestehende Abweichung in `ACCEPTANCE_TESTS.md`:** Die Definitionen SIM-26-04..07 (Zeilen 215-218) und die Testzuordnungstabelle (Zeilen 238-241) passen nicht zueinander (z. B. SIM-26-04 `ProductInsertedConfirmed` zeigt auf Locale-/Clock-Tests; SIM-26-06 `PIN` zeigt auf Editor-Tests). Berichtigung in S11 (O8-b), kein stilles Umhaengen in anderen Slices. | `docs/ACCEPTANCE_TESTS.md:212-241` |
| F18 | **Kein Produktions-Erzeuger fuer Laufmeldungen:** In `lib/fermentation_app/src` schreibt nichts in `RunCommandState::messages`/`messageCount`; es gibt nur Lesestellen (`run_commands.cpp:356,1511`). `MessageCode::UserDecisionRequired` und `ProductInsertionRequested` werden im Produktcode nur gelesen (`fermentation_ui_projector.cpp:16-17`); die `ProcessMessage`-Eintraege einer `TransitionDecision` werden nirgends in `RuntimeMessage` uebersetzt. Folge: Die Meldungsliste ist auf der Hardware leer; Auswahl, Quittieren und Stummschalten sind nur nativ mit Fixtures nachweisbar (O9). | Grep ueber `lib/fermentation_app/src/*.cpp/hpp` |

### 2.2 Inventar je Issue-Luecke (Ist-Pfad bis zum Owner)

| # | Luecke | Ist-Pfad | Bestehender Owner | Fehlt | Slice |
|---|---|---|---|---|---|
| 1 | Header-Sprache | Header zeigt Sprachcode; Hit-Zone existiert seit #170 (F1); `Workspace::press(HeaderLanguage)` navigiert (`:1310`); Seite `HeaderLanguage` hat nur Cross-Navigation inkl. #170-Slot 3 `web-access` (`:757-764`) | `UserConfiguration.displayLanguageId` via `ConfigurationService`; Muster `applyNetworkMode` (`fermentation_application.cpp:778-…`, mit `applicationCallSerializer_.enter()`); Sprachkatalog `kLanguages{de,es,en}` | Auswahlzeilen, Application-Entry, eviction-feste Locale-Kopie (F10); **keine** neue Hit-Zone | S3 |
| 2 | Header-Uhr/Zeitzone | `HeaderClock` Seite nur Cross-Navigation (`:803-809`); Uhr nicht treffbar | `ITimeSource`/`ClockViewInput{trustedUtc, tz}`; Zeitzonenkatalog (F8) | Hit-Zone, Status-Inhalt; Zeitzonenauswahl nicht sinnvoll moeglich (O2) | S4 |
| 3 | Normale Settings | keine Seite (F11); `UserConfiguration` hat `displayLanguageId`, `timeZoneId`, `deviceName`, `activeThemeId`, `networkMode` | `UserConfiguration` / `validateUserConfiguration` | Seite, Einstieg (O1), Geraetename-Editor, Application-Entry; Link auf die bestehende `HeaderWebAccess`-Seite (kein neuer Web-Owner) | S10 |
| 4 | Programmlisten-Auswahl | Liste gezeichnet; `selectProgram()` ohne Aufrufer (F3); kein Row-Target (F1/F2); Pager-Fenster fehlt (F6) | `Workspace::selectProgram` → `ProgramSummary` | Content-Target, Pager-Fenster, Zeilenhoehe | S1 |
| 5 | Programmverwaltung | Aktionen (Reset/Uninstall/Delete/Save) erzeugen `programEdit`-Request; Dispatcher: `UnavailableNoOwner` (F5); Helfer mit falschen Wire-Werten (F14) | `applyProgramEditPreview()` + `ConfigurationService` + `ProgramCatalogRevision`-Staleness | `FermentationApplication`-Entry, Wire-Wert-Korrektur, Dispatcher | S6 |
| 6 | Programmeditor/Startwerte | `setStartCandidate`, `setProgramEditCandidate` nur Tests (F3); Edit-Modelle ohne Konsument (F4) | `validateProgram`, `makeFermentationUiProgramList`, `openProgramEditSession`, Preview-Pfad, `prepareStartProgram` | Editor-Seiten, Tastenfeld/Tastatur, Feldzuordnung | S8, S10 |
| 7 | Manueller Betrieb/Kuehlplaene | `setManualHoldingValues`, `setManualTimedValues`, `setStopCoolingPlan`, `setCompletionCoolingPlan` nur Tests (F3); Confirm-Slots dauerhaft disabled | `validateManualRunPlan`, `prepareStartManual*`, `prepareStop`, `prepareCompletion` | Eingabe nur echter Laufwerte; technische Qualifikationswerte stammen aus einem spaeteren Commissioning-/Produktowner (O5), bis dahin Start fail-closed | S9 |
| 8 | `ProductInsertedConfirmed` | Slot + Intent vorhanden; Dispatcher `UnavailableNoOwner` (F5); `decideProductInsertedConfirmed` ist nur eine Entscheidung ohne Apply | `decideProcessTransition()` (`process_state_machine.cpp:746`), `RunPersistenceCoordinator::persistTransition()` (`:2798`, `ProductInserted` ist in `eligibleTransition`) | `FermentationApplication`-Entry, Bridge, Dispatcher | S5 |
| 9 | Meldungen | Seiten `Messages`/`MessageDetail`, Ack/Mute-Slots, Intents und RAM-Apply (`applyConfirmedPrepared`, `fermentation_application.cpp:553-…`) vorhanden; keine Auswahl (F3/F7); Sensorentscheidung ohne Datenquelle (F12) | `decideAcknowledgeMessage/MuteMessage` | Row-Target, Detail-Inhalt; Sensorentscheidung bewusst nicht in #172 (Follow-up, 4.1) | S2 |
| 10 | Recovery-Zeitkorrektur | Slot `ApplyRecoveryTimeCorrection` nur bei gesetztem Wert (F3); Owner `decideApplyRecoveryTimeCorrection` verlangt eine **vom Benutzer gelieferte** `secondsDelta` innerhalb der Ausfallgrenzen (`run_commands.cpp:1229-1290`) | `ApplyRecoveryTimeCorrectionRequest` | Kein R1-Benutzerpfad spezifiziert: nur `RECOVERY_AND_INTERRUPTION.md:407` und `RUN_PERSISTENCE.md:382` (Semantik), keine UI-Anforderung | S7: zurueckgestellt, siehe 4.1 |
| 11 | Leere/unvollstaendige Seiten | Renderer zeichnet fuer alle Seiten ausser Home, Programmliste, Bestaetigungsname, Netzwerk und `HeaderWebAccess` (#170) nur Titel und Slots (`fermentation_ui_renderer.cpp:422-492`) | Snapshots (`FermentationUiSnapshot`), `ProgramCatalog` | Inhalte je Seite | S7 |

### 2.3 Vollstaendiges Seiten-/Element-Inventar (Header, Seiten, Slots)

Legende: `OK` = Wirkung bis zum Owner verdrahtet und erreichbar;
`NAV` = reine Navigation, funktioniert; `DEAD` = sichtbar/aktiv, aber ohne
produktive Wirkung oder unerreichbar; `DEF` = zurueckgestellt mit Verweis;
Slot-Nummern `0..3` links nach rechts.

| Seite / Element | Slot / Zone | Ist-Wirkung | Owner / Ziel | Status → Slice bzw. Verweis |
|---|---|---|---|---|
| Header | Logo | nicht interaktiv (UI-11) | – | OK (bewusst) |
| Header | Sprache | gezeichnet, treffbar (#170, → `HeaderLanguage`); Seite ohne Auswahl | `UserConfiguration` | NAV; Auswahl DEAD → S3 |
| Header | WLAN | → `HeaderNetwork` | #164 | OK |
| Header | Uhr | gezeichnet, nicht treffbar, UTC unmarkiert | `ClockViewInput` | DEAD → S4 (inkl. Kennzeichnung `HH:MMZ`) |
| Home Standby | 0 `start`, 1 `programs` | → `ProgramList` (doppelt belegt, F11) | – | NAV; Slot 1 wird `settings` (O1-B, S10) |
| Home Standby | 2 `status`, 3 `service` | → `Status`, `Service` (nur wenn `service.available`) | #28 fuer Inhalt | NAV / DEF → #28 |
| Home ActiveRun | `stop`, `programs`, `details`, `status` | → `StopDialog`, `ProgramList`, `Process`, `Status` | – | NAV; Inhalt → S7 |
| Home Waiting | 0 `continue` | `ProductInsertedConfirmed` → `UnavailableNoOwner` (F5) | `persistTransition` | DEAD → S5 |
| Home Waiting (Nutzerentscheidung) | 0 | → `MessageDetail` | Messages | NAV → S2 |
| Home Completed | 0 `ok` | `Complete` ohne Kuehlen → Envelope | `prepareCompletion` | OK |
| Home Recovery | 0 `resume-fallback` / `recovery` | `ResumeFallback` verdrahtet / → `Recovery` | Recovery-Owner | OK |
| Home Restricted/Unavailable | alle | deaktiviert mit Grund | – | OK |
| ProgramList | 0 `home`, 3 `manual` | → Home, → `ManualModeSelection` | – | NAV |
| ProgramList | 1 `up`, 2 `down` | Pager, Renderer ignoriert Fenster (F6) | – | DEAD → S1 |
| ProgramList | Zeilen | nicht treffbar (F1) | `selectProgram` | DEAD → S1 |
| ProgramSummary | 0 `back`, 3 `status` | NAV | – | NAV; Inhalt → S7 |
| ProgramSummary | 1 `edit` | → `ProgramActions` | – | NAV |
| ProgramSummary | 2 `confirm` | `StartProgram`; Seite ist unerreichbar (F3) | `prepareStartProgram` | DEAD → S1; Overrides → S8 |
| ProgramActions | `edit`, `copy`, `new` | → `ProgramEdit` mit Operation | – | NAV; Speichern → S6 |
| ProgramEdit | 1 `reset`, 2 `delete`/`uninstall` | → Delete-Seiten → `programEdit` → `UnavailableNoOwner` | `applyProgramEditPreview` | DEAD → S6 |
| ProgramEdit | 3 `save` | `programEdit` → `UnavailableNoOwner`; Edit ohne Kandidat deaktiviert | dto. | DEAD → S6 (Copy/New), S10 (Edit) |
| ProgramEdit | Felder | keine | Programmmodell | DEAD → S10 |
| ProgramDeleteConfirmation/Final | `confirm`, `cancel`, `delete` | zweistufig; Endaktion → `UnavailableNoOwner` | dto. | DEAD → S6 |
| ManualModeSelection | `manual-holding`, `manual-timed` | → Eingabeseiten | – | NAV |
| ManualHolding / ManualTimed | 2 `confirm` | dauerhaft deaktiviert (F3) | `prepareStartManual*` | DEAD → S9; Start bleibt bis zum Technikwert-Producer fail-closed mit Grund |
| ManualHolding / ManualTimed | Felder | keine | `validateManualRunPlan` | DEAD → S9 |
| Process | `stop`, `programs`, `technical`, `status` | NAV; Inhalt leer | Snapshots | NAV; Inhalt → S7 |
| StopDialog | 1 `stop-turn-off` | `StopRun` verdrahtet | `prepareStop` | OK |
| StopDialog | 2 `stop-and-cool` | deaktiviert (kein Kuehlplan, F3) | `prepareStop` | DEAD → S9 |
| Technical | `up`, `down`, `messages` | kein Inhalt/Pager leer | Snapshots | DEAD → S7 |
| Messages | `up`, `down` | Pager, keine Zeilen | `snapshot.messages` | DEAD → S2 |
| Messages | 3 `details` | → `MessageDetail` mit „erster aktiver“ (F7) | – | DEAD → S2 |
| MessageDetail | 1 `acknowledge`, 2 `mute` | deaktiviert ohne Auswahl (F3) | `decideAcknowledge/MuteMessage` | DEAD → S2 |
| MessageDetail | 3 `continue` (Sensoraktion) | nur mit `setSensorSelectionAction` (F3/F12); bleibt unberuehrt | `decideApplySensorSelectionAction` | DEF → Follow-up Meldungs-/Sensorentscheidungs-Erzeuger (4.1) |
| MessageDetail | 3 `fault-reset` | `ResetFault` verdrahtet, nur im Zustand `Fault` | Fault-Owner | OK |
| Completion | 1 `details`, 2 `ok` | NAV / `Complete` verdrahtet | – | OK; Inhalt → S7 |
| Completion | 3 `cool-now` | deaktiviert (F3) | `prepareCompletion` | DEAD → S9 |
| Status | `messages`, `diagnostics`, `technical` | NAV | – | NAV |
| Diagnostics | `up`, `down`, `service` | kein Inhalt | #28 | DEF → #28 (Text „zurueckgestellt“, S7) |
| Service | 1 `pin`, 3 `recovery` | → `Pin`, → `Recovery` (nur wenn `service.available`) | #28 | DEF → #28 |
| Pin | `cancel`, `status`, `service` | keine PIN-Eingabe | #28 | DEF → #28 |
| Recovery | 1 `resume-fallback` | `ResumeFallback` verdrahtet | Recovery-Owner | OK |
| Recovery | 1 `confirm` (Zeitkorrektur) | nur mit `setRecoveryTimeCorrectionSeconds` (F3) | `decideApplyRecoveryTimeCorrection` | DEF → Folge-Scope (4.1) |
| Recovery | 3 `diagnostics` | → `Diagnostics` | #28 | DEF → #28 |
| HeaderLanguage | 1 `network`, 2 `clock`, 3 `web-access` (provisorischer #170-Uebergangspfad) | NAV; Slot 3 → `HeaderWebAccess` | – | NAV; Auswahl → S3; Slot 3 bleibt in PR A–C und entfaellt atomar in S10 (D13) |
| HeaderWebAccess | 0 `back`, 1 `web-access-open` | `OpenWebProvisioningWindow` → `FermentationApplication::openWebProvisioningWindow()`, Anzeige `snapshot.webAccess` | #27 / `FermentationApplication` | OK (#170, hardware-verifiziert); unberuehrt |
| HeaderNetwork | alle | verdrahtet | #164 | OK |
| HeaderClock | 1 `language`, 2 `network` | NAV; kein Inhalt | `ClockViewInput` | DEAD → S4 |

Zurueckgestellte Punkte ohne bestehendes Folge-Issue: **Zeitzonenauswahl/
Lokalzeit** (O2) und **Recovery-Zeitkorrektur als Benutzerpfad** (4.1). Der
Agent legt kein Issue eigenmaechtig an. Der Plan beantragt beim Owner je ein
Folge-Issue oder einen `FUTURE_SCOPE.md`-Eintrag (O2, 4.1). Bis dahin nennt die
Seite den Grund „zurueckgestellt“ (kein Funktionsversprechen).

### 2.4 Abdeckung durch bestehende Tests (Bestand, wird erweitert)

`test_local_touch_ui`, `test_press_dispatcher`, `test_renderer_boundary`,
`test_device_ui_contracts`, `test_fermentation_ui_editing`,
`test_fermentation_ui_commands`, `test_fermentation_ui_models`,
`test_fermentation_ui_presentation_cache`, `test_ui_steady_state_allocations`,
`test_network_configuration`, `test_configuration_service`,
`test_run_commands`, `test_run_persistence_coordinator`.

## 3. Entwurfsentscheidungen (ohne Ownerentscheidung)

**D1 – Ein app-neutrales Zell-Target.** `DeviceUiTargetKind::ContentCell` mit
`row` und `column` (je `std::uint8_t`, Listen nutzen `column=0`). Das ist der
kleinste Trefferbegriff, der Listen und – falls O3/S8/S10 freigegeben sind –
Tastenfeld und Tastatur abdeckt, ohne Widget-Objekt. Er liegt in
`device_platform` (ADR-013) und ist fachlich neutral (kein „Program“ im Namen).
Die Plattform fuehrt nur die Indizes; Kapazitaeten (Zeilen/Spalten) bestehen
ausschliesslich in Renderer/Workspace, deshalb aendert sich der Plattformvertrag
nicht erneut, wenn spaeter Tastenfelder folgen. Es wird nichts fuer Tastatur
oder Tastenfeld vorgebaut. Reviewfrage fuer den Independent Plan Review.

**D2 – Pager-Fenster ohne Plattformaenderung.** `VerticalPager` bleibt
unveraendert. Die sichtbaren Zeilen sind `currentIndex … currentIndex+2`;
Zeile `r` bedeutet Element `currentIndex + r`. Auf/Ab verschiebt um 1.

**D3 – Layout fuer grosse Touchziele.** Inhaltszeilen erhalten 40 px Hoehe
(= `kControlHeight` der BottomSlots), 3 sichtbare Zeilen (y=64..184). Der
Zaehler `n/N` wandert in die Titelzeile rechts; `blockedReason` in die Zeile
y=184..200. Die Netzwerkseite behaelt ihr hardware-validiertes Layout
unveraendert (#164). Tastengroessen fuer Tastenfeld/Tastatur sind **keine**
Planannahme, sondern Hardware-Abnahmekriterium (siehe S8/S10).

**D4 – Header-Hit-Zonen ohne Ueberlappung.** Die `HeaderLanguage`-Zone
(`kHeaderLanguageHitRect{176,0,44,32}`) und die `HeaderNetwork`-Zone
(x=220..264, y=4..22) sind seit #170 **Bestand und werden nicht veraendert**
(nicht erneut gebaut). #172 ergaenzt ausschliesslich die neue Zone
`HeaderClock` x=264..320 ueber die volle Headerhoehe y=0..32 (S4); sie
ueberlappt weder Netzwerk- noch Sprachzone (Test: Randpixel 263/264 und
319/320; die vorhandenen Sprach-/Netzwerk-Randtests bleiben unveraendert).

**D5 – Commit-Pfad fuer UserConfiguration/ProgramCatalog.** Genau ein Pfad:
neue `FermentationApplication`-Methoden nach dem Muster von `applyNetworkMode`
(seit #170 beginnt jede oeffentliche Application-Methode mit
`applicationCallSerializer_.enter()`, weil Touch-/Main-Loop und Web-Callbacks
dieselbe Instanz nutzen; die neuen Methoden `applyDisplayLanguage`,
`confirmProductInserted`, `applyProgramEdit`, `applyUserSettings` tun dasselbe
und ergaenzen weder ein zweites Lock noch ein neues Serializer-Konzept; der
Serializer ist rekursiv, geschachtelte oeffentliche Aufrufe bleiben zulaessig;
je neuer Methode prueft ein Test nach dem Muster von `test_application_call_serializer_blocks_cross_thread_and_allows_reentry` in `test_web_application_routes`, dass der Aufruf bei gehaltenem Guard blockiert) (`beginPreview` → Kandidat aendern → `installPreview` mit den **kanonischen**
Wire-Werten `{LocalDisplay,2U}`/`{NormalEdit,1U}` (bei Standardprogramm-Reset
`{StandardProgramReset,6U}`) → `validatePreviewForConfirmation` mit erwarteter
Revision → `confirmPreview`). Der gemeinsame private Helfer ist **nur fuer die
neuen Methoden** (Sprache, Einstellungen, Programmkatalog); er ruft auf jedem
Ausgang ohne `Activated`/`NoChange` `cancelPreview(handle)` auf (es gibt nur
einen sichtbaren Preview-Slot, den #27 spaeter mitbenutzt).
`applyNetworkMode` bleibt **unberuehrt** (hardware-validiert, in PR #174
OOM-relevant). Ergebnisse laufen ueber die vorhandenen Bridge-Abbildungen
(`fromConfigurationPreview`, `fromConfigurationCommit`); es entsteht kein neuer
Detail-Variant. `FermentationUiConfigurationCommitCommand` wird nicht
verwendet (verlangt `ConfigurationService&`, das `main/` nicht besitzt).

**D6 – Sprachwechsel „sofort“.** Es gibt keinen UI-lokalen Locale-Zustand
(zweite Wahrheit). Eine Auswahl ist ein einzelner Tap, der den Commit sofort
ausloest; „sofort“ heisst unmittelbar nach erfolgreichem Commit. Der
`FermentationUiPresentationCache` erkennt die neue
`UserConfigurationRevision` und fuellt neu, der Render-Key
(`localeFingerprint`) erzwingt den Redraw. Schlaegt der Commit fehl, bleibt die
Sprache unveraendert und das Ergebnis wird sichtbar gemeldet. Das ist mit dem
Wortlaut des Issues („aktive Anzeige sofort ueber den bestehenden Textpack-
vertrag, Persistenz ausschliesslich ueber ConfigurationPreview-/Commitpfad“)
vereinbar; der Independent Plan Review prueft diese Lesart.

**D7 – ProductInserted bleibt ausserhalb des Envelope-Variants.** Es ist ein
Prozessuebergang (`persistTransition`, keine `CommandId`), kein
CommandEnvelope-Befehl. `FermentationUiEnvelopePayload` und die Idempotenz-/
Wire-Semantik bleiben unveraendert. Belegte Randbedingungen (F15): Die
Application hat keinen zyklischen Prozess-/Fault-Pfad; der Application-Entry
verwendet `ProcessSignals{}` wie der `RunPersistenceCoordinator` an allen
bestehenden Aufrufstellen. `persistTransition` prueft selbst keine
Sensorevidenz fuer den Eintritt (`liveSensorEvidence` fliesst nur als
Recovery-Evidenz ein); es wendet die Entscheidung der Zustandsmaschine an
(`eligibleTransition` enthaelt `ProductInserted`). Aktorfreigabe liegt
downstream (Planner/Interlock/#35); `ACTUATOR_RELEASE=NO` bleibt.

**D8 – Editoren: Tastenfeld statt Schrittbedienung.** Weil `Plus/Minus`
Vorzeichenaktionen sind (F4), wird `NumericEditModel` unveraendert mit einem
Ziffernfeld ueber das `ContentCell`-Raster bedient: Raster 4 Zeilen × 3 Spalten
(`1 2 3 / 4 5 6 / 7 8 9 / . 0 ±`, `±` bildet auf `NumericEditAction::Minus`
(Vorzeichen umschalten) ab, `Plus` entfaellt als redundant). Die uebrigen
Aktionen liegen auf den vier BottomSlots: Slot 0 `Cancel`, Slot 1 `Backspace`,
Slot 2 `Clear`, Slot 3 `Commit`. Der Wert steht in einer Anzeigezeile
(y≈36..58), das Raster belegt y≈60..196 (34 px je Zeile). Die Validierung
erfolgt nur ueber bestehende Validatoren (`validateProgram(…Runnable)`,
`validateManualRunPlan`, `validateUserConfiguration`, `validateVisibleName`),
angewendet auf eine Kopie mit dem Kandidatenwert; der Application-Owner
validiert erneut beim Prepare/Commit. **Hardware-Abnahmekriterium:** Tasten
von 34 px Hoehe muessen mit dem Stylus/Finger zuverlaessig treffbar sein; wird
das bei der Abnahme verfehlt, ist eine Plan-Revision des Rasters noetig (kein
stilles Nachjustieren).

**D9 – Feldzuordnung explizit statt generisch.** Pro Seite eine kleine
`enum class …Field` mit explizitem `switch` (Startwerte, manuelle Werte,
Programmfelder). Kein reflexives Formmodell, keine Feldregistrierung.
Eine gemeinsame Editor-Seite (`ValueEdit`, `TextEdit`) nimmt Feld-Enum und
Rueckkehrseite entgegen.

**D10 – RAM (Kontext PR #174, ohne PSRAM).** Die geschlossene PR-#174-Richtung
`FURTHER_PROACTIVE_RAM_OPTIMIZATION=NO` gilt. #172 fuehrt **keine**
prophylaktische RAM-/Eviction-Architektur ein: kein neuer Mutations-Evict-Pfad
und keine Aufteilung von `processWorkspaceTouch()`.
- Die PR-#174-Baseline (`CONFIG_LV_MEM_SIZE=49152`, Main-Task-Stack 24576 B) bleibt
  unveraendert; die gemergte #170-Basis aendert beides nicht (httpd-Stack
  8192 B, neue `std::mutex`-/`condition_variable`-Objekte im Application-
  Serializer und `AuthOperationGate`). Referenz fuer spaetere Hardware-Logs ist
  der #170-Nachweis `docs/audits/PR170_HW_GATE_20261005/`
  (`MIN_FREE_HEAP_UNDER_WEB_LOAD=8148`, `MAIN_STACK_HWM=6056`,
  `IDLE_1696B_FOLLOW_UP=OPEN_NON_BLOCKING_NOT_REPRODUCED`). #172 leitet daraus
  keine Optimierung ab und erfindet keine Budgetgrenze.
- Neue Seiteninhalte nutzen den vorhandenen `ScreenDrawCommand`-Pfad;
  Befehlsanzahl je Seite wird begrenzt und im Test geprueft (Muster
  `kNetworkScreenDrawCommandCapacity`).
- `test_ui_steady_state_allocations` wird fuer jede neue Seite und jede neue
  Press-Art erweitert (kein Heap-Zuwachs im Steady State).
- S3, S6 und S10 (jeweils `ConfigurationService`-Commit) werden mit
  Steady-State-/Resource-Regressionen und spaeter mit einem Hardware-Log (`logResources` vor/nach Commit-Presses, analog
  `network_page_press_*`) gemessen; Hardware-Nachweise erst nach dem
  Software-/Reviewgate und mit `ACTUATOR_RELEASE=NO`.
- Nur bei einem **reproduzierten neuen** OOM-/Heap-Problem wird ein eigener,
  gezielter Fix geplant (materielle Planrevision). Budgetgrenzen werden nicht
  erfunden; sie bleiben `TBD_IMPLEMENTATION_BUDGET` bis zur finalen
  R1-Integrationsqualifikation (PR #174).
- Neue Textschluessel kosten Flash, keinen Heap.

**D11 – Eviction-feste Locale/Zeitzone (Korrektur F10).** Sie betrifft
ausschliesslich den **bereits bestehenden** `HeaderNetwork`-Evict-Pfad und ist
von jeder Mutations-Eviction entkoppelt (gestrichen, D10). Der Owner ist
`FermentationUiPresentationCache` (`fermentation_ui_presentation_cache.hpp`),
weil nur dort sichtbar ist, ob ein Fill erfolgreich war. Er haelt zwei kleine
Werte (`LocaleId` ≤ 16 Byte, `TimeZoneId` ≤ 64 Byte), die bei **jedem
erfolgreichen Fill** aktualisiert werden und von `evict()` **nicht** verworfen
werden (`evict()` verwirft nur Katalog und Revisionen). Zugriff ueber zwei
neue Lese-Accessoren; `UiRenderGate` (`main/fermentation_ui_press_dispatcher.hpp`)
und `app_main` verwenden sie statt `initialDisplayLocale`/`initialTimeZoneId`
fuer Render-Key und Netzwerkseite waehrend des bestehenden HeaderNetwork-Evicts. Damit bleibt
die Netzwerkseite nach einem Sprachwechsel in der neuen Sprache. Betroffen:
`test_fermentation_ui_presentation_cache`, `test_ui_steady_state_allocations`.

**D12 – Verwaltungsauswahl getrennt von der Startauswahl.** `selectProgram()`
verlangt heute `installed && enabled && validateProgram(Runnable)`. Mit
S1 waeren nicht startbare Eintraege (z. B. ein per `New` angelegtes Programm
mit zurueckgesetzten Laufparametern, oder ein mit `program-invalid` gelistetes)
nicht waehlbar und damit lokal weder bearbeit- noch loeschbar. Entscheidung:
Ein Tap waehlt jeden **installierten** Listeneintrag; `ProgramSummary` zeigt fuer
nicht startbare Eintraege den `blockedReason` und setzt den Startkandidaten
nicht (`confirm` bleibt deaktiviert); `edit`/`copy`/`delete` bleiben
erreichbar. Nicht installierte Standardprogramme stehen nicht in der Liste
(Wiederherstellung nur per Werksreset, Schluessel `factory-reset-required`).
Das aendert den bestehenden Vertrag von `selectProgram` (heute `false` fuer
nicht startbare Eintraege); die betroffenen Tests in drei Testdateien werden in
S1 bewusst angepasst und im PR als Vertragsaenderung ausgewiesen.

**D13 – Webzugang-Navigation (Uebergang und Ziel).** Der #170-Stand
`HeaderLanguage` Slot 3 `web-access` ist nur der testbare Uebergangspfad, nicht
die endgueltige UX. In PR A–C bleibt der Slot unveraendert, damit das Web-Setup
erreichbar bleibt; S3 behandelt ihn temporaer als Bestand und baut keine zweite
Sprachloesung. In S10/PR D wird `Webzugang` in `Einstellungen` eingehaengt und
im selben Commit der Slot aus `HeaderLanguage` entfernt; zu keinem Zwischenstand
existiert ein unerreichbarer Web-Setup-Pfad. Verwendet wird ausschliesslich die
bestehende Seite `HeaderWebAccess`, die Aktion `NavigateWebAccess` und der #170-
Owner (`openWebProvisioningWindow()`/`webAccessState()`); keine zweite
Auth-/Provisionierungslogik, keine neue WebAccess-Domain. Tests in S10: Slot 3
entfernt, `Webzugang` erreichbar, `OpenWebProvisioningWindow` unveraendert.

## 4. Zurueckgestellt und geschlossen mit Beleg

### 4.1 Geschlossen mit Beleg (keine Ownerentscheidung noetig)

- **Recovery-Zeitkorrektur als Benutzerpfad:** Das Issue verlangt ihn „nur
  soweit der aktuelle R1-Recoveryvertrag diesen Benutzerpfad tatsaechlich
  fordert“. Suche ueber `docs/*.md`: `ApplyRecoveryTimeCorrection` kommt nur
  in `RECOVERY_AND_INTERRUPTION.md:407` und `RUN_PERSISTENCE.md:382` vor (beide
  Semantik/Persistenz), keine UI-Anforderung. Folge: Der Slot bleibt fuer R1
  deaktiviert/ausgeblendet mit Grund „nicht verfuegbar“; `setRecoveryTime
  CorrectionSeconds` bleibt Testhilfe. Das Folge-Issue bzw. der Future-Scope-
  Eintrag wird beim Owner **beantragt** (nicht eigenmaechtig angelegt).
- **Sensorentscheidung zurueckgestellt (O9-A):** #172 bindet Meldungsauswahl
  sowie Ack/Mute an, aber **keine** pauschal aktivierte Drei-Aktions-Liste. Die
  Zulaessigkeit von `ContinueWithAir`/`ReturnToProduct`/`RecheckProduct`
  haengt von Sensorphase, Plausibilitaet und Safety-Zustand ab
  (`decideApplySensorSelectionAction`); `MessageView` enthaelt diese
  authoritative Eligibility nicht, und es gibt keinen Produktions-Erzeuger fuer
  `UserDecisionRequired` (F18). Ein eigenes Follow-up besitzt Erzeuger **und**
  authoritative Aktionsprojektion. Nur falls der Owner Sensorentscheidungen
  ausdruecklich in #172 behalten will, ist eine schmale Application-owned
  Eligibility-Projektion zulaessig (materielle Planrevision); eine duplizierte
  UI-Regelmatrix ist ausgeschlossen. `setSensorSelectionAction` und der
  vorhandene Slot 3 `continue` bleiben unberuehrt.

## 5. Empfohlene Ownerentscheidungen (Bestaetigung ausstehend)

Die Werte stammen aus den Ownerempfehlungen des Korrekturauftrags zu PR #175.
Sie sind **nicht** als Ownerentscheid beschlossen (`pending owner
confirmation`); der Plan arbeitet nur vorlaeufig mit ihnen. Nach ausdruecklicher
Ownerbestaetigung werden sie als beschlossen markiert. Der Agent trifft keine
davon selbst, `IMPLEMENTATION_AUTHORIZATION=NO` bleibt.

| Nr. | Gegenstand | Empfohlene Entscheidung (ausstehend) | Betrifft |
|---|---|---|---|
| O1 | Einstieg normale Einstellungen | **B**: der redundante Standby-Home-Slot 1 `programs` wird `settings`; Slot 0 `start` bleibt Zugang zur Programmliste. Andere Home-Modi unveraendert. Betrifft die Home-Aktionsmatrix (`test_local_touch_ui`, SIM-26-01). Revalidiert gegen #170: Standby-Slots unveraendert (`start`/`programs` doppelt belegt). Webzugang: Zielstruktur `Einstellungen` = Sprache, Zeit/Zeitzone, Geraetename, Netzwerk, Webzugang; der provisorische `HeaderLanguage`-Slot 3 entfaellt atomar mit dem Einhaengen in S10 (D13, Korrekturauftrag B3). | S10 |
| O2 | Zeitzone | **A**: lesend; keine vorgetaeuschte Auswahl, keine neue Zeitdatenbank. Header und Uhrseite kennzeichnen UTC eindeutig (`HH:MMZ`). Lokale IANA-Zeit/Offset/DST als eigener spaeterer Scope (Folge-Issue oder `FUTURE_SCOPE.md`, vom Owner anzulegen). | S4, S10 |
| O3 | Bildschirmtastatur | **A**: lokale Tastatur fuer Programmname, Notiz und Geraetename. Die HOME_WIFI-Credentialtastatur bleibt deferred und wird nicht aufgenommen (F16). | S10 |
| O4 | Geraetename | **A**: nur ohne aktiven Lauf; UI-Wert nach Commit sichtbar; SSID/Hostname/QR erst beim naechsten normalen Netzwerkstart; kein Auto-Restart. | S10 |
| O5 | Manuelle Qualifikations-/Technikwerte | **B**: Zielband, Qualifikationsdauer, maximale Zielerreichungszeit und vergleichbare Grenzwerte kommen aus einem spaeter freigegebenen Commissioning-/Produktowner (#34/#35); keine Benutzereingabe, keine erfundenen Defaults. | S9 |
| O6 | PR-Schnitt | **A modifiziert**: Plan-PR plus vier Implementations-PRs, **sequenziell A → B → C → D**, jeweils von aktuellem `main`. | alle |
| O7 | Reihenfolge #170/#172 | **B**: PR #170 zuerst abschliessen/mergen; danach PR #175 auf neuen `main` rebasen, revalidieren, erst dann exakte Plan-SHA freigeben. | alle |
| O8 | (a) Reset-Slot, (b) SIM-26-Zuordnung | (a) **A**: `ProgramSummary` Slot 3 wird `reset`, solange der Laufkandidat von den Programmwerten abweicht, sonst `status`. (b) die falsche SIM-26-Trace-Zuordnung wird in S11 als begrenzte Doku-Korrektur berichtigt; kein separates Issue. | S8, S11 |
| O9 | Meldungen/Sensorentscheidung | **A**: #172 liefert Meldungsauswahl und Ack/Mute; der fehlende RuntimeMessage-/Sensorentscheidungs-Erzeuger **und** die Sensoraktions-Eligibility sind ein eigenes Follow-up (oder eine spaeter ausdruecklich ownerfreigegebene #172-Erweiterung). | S2 |

## 6. PR-Schnitt und Gates

Elf Slices in einem PR wuerden dem Grundsatz „zusammenhaengender Scope, klein
und unabhaengig reviewbar“ widersprechen. Gemaess O6 ist **dieser PR plan-only**
(Praezedenzfall: PR #171 wurde als reiner Plan-PR gemergt). Nach dem
abgeschlossenen Rebase auf `main` nach PR #170 (Abschnitt 9), Independent Plan
Review und Freigabe der exakten Plan-SHA entstehen **vier Implementations-PRs,
strikt sequenziell** A → B → C →
D, jeweils von aktuellem kanonischem `main` nach dem Merge des Vorgaengers
(kein gestapelter PR ohne ausdrueckliche Ownerfreigabe). Parallelitaet wird
nicht geplant: auch S3 aendert `fermentation_application.*`,
`fermentation_ui_commands.*` und `fermentation_ui_press_dispatcher.cpp`, also
dieselben Grenzen wie S5/S6.

| PR | Slices | Inhalt |
|---|---|---|
| A | S1–S4 | Touch-Navigation: Content-Target, Programmlistenauswahl, Meldungsauswahl, Header-Sprache, Header-Uhr |
| B | S5–S6 | Application-Owner: ProductInserted, ProgramCatalog-Mutation, Wire-Wert-Korrektur |
| C | S7–S9 | Seiteninhalte, Tastenfeld, Startwerte, manuelle Eingabe (Start fail-closed bis Technikwert-Producer) |
| D | S10–S11 | Tastatur, Programmeditor, Settings, Dokumentation |

Jeder Implementations-PR zitiert die vom Owner freigegebene Plan-SHA und nennt
seinen Slice-Bereich. Vor jedem PR-Start wird der Stand aus Abschnitt 9
revalidiert und der Owner bestaetigt den Start; Planabweichungen folgen
`AGENT_WORKFLOW.md` §6. Pro PR gelten Independent Review, Pre-Ready-Gate und CI
getrennt.

## 7. Slices

Reihenfolge ist die empfohlene Umsetzungsreihenfolge. Jeder Slice ist ein
eigener Commit und wird einzeln lokal getestet (gezielte Tests, kein
vollstaendiger Pre-Ready-Lauf in Draft). Nach jedem Commit wird angehalten.

### S1 – Content-Target, Pager-Fenster, Programmlistenauswahl

- **Dateien/Owner:** `lib/device_platform/src/device_ui_interaction.{hpp,cpp}`
  (Target, `selectDeviceUiTarget`), `main/fermentation_ui_renderer.cpp`
  (`targetAt`, Zeilen, Layout, PressFeedback), `lib/fermentation_app/src/
  fermentation_touch_workspace.{hpp,cpp}` (`press(ContentCell)`, Seite
  `ProgramList`), `scripts/check_architecture_boundaries.py` (nur falls
  Regel noetig).
- **Verhalten:** `ContentCell`-Target (D1); `targetAt()` trifft sichtbare
  Zeilen; Pressfeedback fuer Zeilen; Zeilenhoehe 40 px, Layout D3;
  Programmliste zeichnet `currentIndex…+2`; Treffer auf Zeile `r` ruft
  `selectProgram(entries[currentIndex+r].id)` → `ProgramSummary` mit D12:
  jeder installierte Eintrag ist waehlbar, nicht startbare zeigen den
  `blockedReason` und lassen `confirm` deaktiviert. Keine LVGL-Fachlogik.
- **Tests:** `test_device_ui_contracts` (Target-Validitaet),
  `test_renderer_boundary` (Hit-Test Zeilen/Randwerte, Fenster, Zeilenhoehe),
  `test_local_touch_ui` (Pager + Auswahl → `ProgramSummary`; nicht startbarer
  Eintrag waehlbar mit deaktiviertem `confirm`; bewusst angepasste
  `selectProgram`-Erwartungen, D12), `test_press_dispatcher` (Auswahl liefert kein Payload),
  `test_ui_steady_state_allocations`.
- **Abhaengigkeiten:** keine. Basis fuer S2, S3, S7–S10.
- **Ownerentscheidung:** keine.

### S2 – Meldungsauswahl, Ack/Mute

- **Dateien/Owner:** `fermentation_touch_workspace.{hpp,cpp}`, Renderer,
  `fermentation_ui_text.cpp`. **Keine** Aenderung an `FermentationApplication`,
  Snapshot oder Projector.
- **Verhalten:** `Messages` zeigt das Fenster ueber `snapshot.messages`;
  Zeilentreffer setzt die kanonische Message-ID (`setSelectedMessage` wird
  produktiv); `NavigateMessageDetail` aus der Home-Warteansicht behaelt das
  bestehende „erste entscheidungspflichtige“ Verhalten, aus `Messages` erzwingt
  es die explizite Auswahl (F7). `MessageDetail` zeigt Code, Klasse,
  Quittiert/Stumm aus `MessageView` (Textschluessel je `MessageCode`; das Enum
  hat heute sieben Werte, im Umsetzungsschritt erneut gezaehlt). Ack/Mute laufen
  unveraendert ueber `prepareEnvelope` → `applyConfirmedPrepared`.
  **Keine** Sensoraktions-Zeilen und keine pauschale Drei-Aktions-Liste (4.1,
  O9-A); Slot 3 bleibt wie heute (`fault-reset`, bzw. `continue` nur mit dem
  unberuehrten Test-Setter).
- **Tests:** `test_local_touch_ui` (Auswahl, Detail, Ack/Mute-Payload),
  `test_press_dispatcher` (Ack/Mute → `OwningOutcome`), `test_renderer_boundary`,
  `test_ui_steady_state_allocations`.
- **Hardware:** Wegen F18 bleibt die Meldungsliste auf der Hardware leer; der
  Nachweis erfolgt nativ mit Fixtures, Hardware `NOT_APPLICABLE` (11.1).
- **Abhaengigkeiten:** S1. **Ownerentscheidung:** O9 (A).

### S3 – Header-Sprache DE/EN/ES

- **Dateien/Owner:** `fermentation_ui_renderer.cpp` (Zeilen der Seite
  `HeaderLanguage`; die Header-Hit-Zone ist Bestand aus #170 und wird
  **wiederverwendet, nicht neu gebaut**, D4; sie wird nur als Regression
  getestet),
  `fermentation_touch_workspace.{hpp,cpp}` (Seite `HeaderLanguage`: drei
  `ContentCell`-Zeilen, aktuelle Sprache markiert; **Slot 1–3 bleiben
  unveraendert**, insbesondere der provisorische #170-Slot 3 `web-access`),
  `fermentation_ui_commands.{hpp,cpp}` (neuer typisierter Intent
  `FermentationUiSetDisplayLanguageCommand{languageId, expected revision}` +
  Bridge), `fermentation_application.{hpp,cpp}` (Entry `applyDisplayLanguage`,
  D5), `fermentation_ui_press_dispatcher.cpp`, `main/app_main.cpp` (D11),
  `fermentation_ui_text.cpp` (Sprachnamen als Schluessel, ASCII-Endonyme).
- **Verhalten:** Zeilenliste kommt aus dem im Build enthaltenen Katalog
  (`makeFermentationR1DeviceUiBuildCatalog()`), nicht aus einem Literal;
  Treffer → Entry → Preview/Install/`validatePreviewForConfirmation`/Confirm;
  unbekannte ID und veraltete Revision werden abgelehnt
  (`UnknownLanguageId`, `StateChanged`); Sprachwechsel ist waehrend eines Laufs
  erlaubt (Komfortwert). Render-Key und Netzwerkseite verwenden die
  eviction-feste Kopie aus D11.
- **Tests:** neuer Application-Test (Commit, Stale, Unknown,
  `cancelPreview` auf Fehlerausgaengen, Persistenz ueber Neustart mit
  `SimulatedPersistentStateStore`: begin → apply → neu begin → Sprache),
  `test_local_touch_ui` (Zeilen, Markierung, Slot 3 unveraendert),
  `test_press_dispatcher`, `test_renderer_boundary` (Zeilen-Hit-Test der
  Sprachseite; die vorhandenen Randtests der Sprach-/Netzwerkzone bleiben
  unveraendert, Netzwerkseite folgt der neuen Sprache), `test_fermentation_ui_presentation_cache` (Refill bei
  Revisionswechsel), `test_ui_steady_state_allocations`.
  `test_network_configuration` bleibt unveraendert (D5).
- **Abhaengigkeiten:** S1. **Ownerentscheidung:** keine.
- **Konfliktflaeche #170 (revalidiert):** `HeaderLanguage` Slot 3 (`web-access`,
  bleibt), Textarray (`70U`), `FermentationUiWorkspacePress`/Dispatcher
  (`openWebProvisioningWindow`-Zweig bleibt, #172 ergaenzt einen eigenen
  Zweig), `FermentationUiCommand::operation` (+`SetDisplayLanguage`),
  Application-Serializer (D5).

### S4 – Header-Uhr (UTC-Kennzeichnung und Statusseite)

- **Dateien/Owner:** Renderer `targetAt` (nur neue Uhr-Zone, D4) und `formatClockText`,
  Workspace `HeaderClock`, Renderer-Inhalt, Textpacks.
- **Verhalten:** Hit-Zone Uhr. Der Header zeigt die Uhrzeit **eindeutig als UTC**
  (`HH:MMZ`; ohne vertrauenswuerdige Zeit unveraendert `--:--`). Die Seite zeigt
  ausschliesslich vorhandene Werte: UTC-Zeit mit Kennzeichnung,
  Vertrauensstatus (`trustedUtc` gesetzt oder nicht) und die kanonische
  Zeitzonen-ID aus der Presentation-Quelle. Keine Auswahl (O2-A), keine neue
  Zeitwahrheit, keine Zeitdatenbank, keine #126-Neuimplementierung; lokale
  Zeit/IANA-Offset/DST ist eigener spaeterer Scope.
- **Abnahmekriterium:** Die sechs Zeichen von `HH:MMZ` passen in das 52-px-
  Uhrfeld (x=264..316) ohne Abschneiden (Hardware-/Renderer-Test).
- **Tests:** `test_renderer_boundary` (`12:34Z`, `--:--`, Textbreite),
  `test_local_touch_ui`, `test_ui_steady_state_allocations` (Render-Key nutzt
  weiterhin die UTC-Minute).
- **Abhaengigkeiten:** S1 (Layout); keine `targetAt`-Kopplung mehr zu S3, da
  die Sprachzone Bestand ist. **Ownerentscheidung:** O2 (A).

### S5 – `ProductInsertedConfirmed`-Owner

- **Dateien/Owner:** `fermentation_application.{hpp,cpp}`
  (`confirmProductInserted(context)`), `fermentation_ui_commands.{hpp,cpp}`
  (Bridge-Funktion analog `resumeFallback`), `fermentation_ui_press_dispatcher.cpp`
  (nur `transitionAction`-Zweig), `docs/ACCEPTANCE_TESTS.md` (neue SIM-IDs).
- **Verhalten:** Ablauf: `runtimeRunState_` und
  `runPersistenceCoordinator_` muessen vorhanden sein (sonst
  `ContextMissing`, fail-closed); erwartete `transitionSequence` muss zur
  aktuellen passen (`StaleState`); `decideProcessTransition(…
  ProductInsertedConfirmed …)` mit `processRunSnapshot` und `ProcessSignals{}`
  (D7); Apply ausschliesslich ueber `persistTransition(…, owningRuntimeEvidence_)`;
  Ergebnis ueber `fromRunPersistenceResult` (OwningOutcome). Die UI liefert
  nur Intent und erwartete Revision.
- **Erreichbarkeit:** Wegen F15 ist `WaitingForProduct` im Produktbuild ohne
  Regelkreis nur nach einer wiederhergestellten Wartephase erreichbar. Der
  Hardware-Nachweis fuer S5 ist daher bis zur Komposition des Regelkreises
  (#30/#35) `NOT_APPLICABLE` (Abschnitt 11.1); der Nachweis erfolgt
  ausschliesslich nativ mit einem `WaitingForProduct`-Fixture.
- **Doku-Wirkung:** Neue SIM-IDs fuer den owning Pfad; die Berichtigung der
  vorbestehenden SIM-26-Fehlzuordnung (F17) erfolgt ausschliesslich in S11.
- **Tests:** neuer Application-Test (WaitingForProduct → ReachingTarget
  persistiert; falscher Zustand; stale; Persistenzfehler fail-closed;
  wiederholter Press abgewiesen), `test_press_dispatcher` (der bisherige
  `UnavailableNoOwner`-Test wird zum Owning-Test), `test_local_touch_ui`
  unveraendert, `test_process_state_machine` und
  `test_run_persistence_coordinator` Konsument.
- **Abhaengigkeiten:** PR A gemergt (sequenziell, Abschnitt 6).
  **Ownerentscheidung:** keine (die SIM-26-Zuordnung berichtigt S11, O8-b).

### S6 – ProgramCatalog-Mutations-Owner, Programmverwaltung ohne Editor

- **Dateien/Owner:** `fermentation_ui_editing.cpp` (`applyProgramEditPreview`
  Wire-Werte, F14), `fermentation_application.{hpp,cpp}`
  (`applyProgramEdit(request, expectedProgramCatalogRevision)`),
  `fermentation_ui_commands.{hpp,cpp}` (Bridge ueber vorhandene
  `ConfigurationPreviewStatus`/`ConfigurationCommitStatus`),
  `fermentation_ui_press_dispatcher.cpp` (nur `programEdit`-Zweig).
- **Reihenfolge im Slice:** Zuerst ein **fehlschlagender Test**, der ein von
  `applyProgramEditPreview` erzeugtes Preview bestaetigt und neu laedt
  (belegt F14, falls zutreffend); danach die Korrektur der Wire-Werte
  (`{LocalDisplay,2U}`, `{NormalEdit,1U}`/`{StandardProgramReset,6U}`) mit
  demselben Test gruen. Zeigt der Test keinen Fehler, wird F14 im PR als
  widerlegt dokumentiert.
- **Verhalten:** `makeFermentationUiProgramUsageEvidence(*runtimeRunState_)`
  aus dem Owner, nie aus der UI; `applyProgramEditPreview(service, expected,
  request, usage)` → `validatePreviewForConfirmation` (die erforderliche
  `UserConfigurationRevision` stammt aus
  `snapshot.revisions.expectedUserConfigurationRevision`) → `confirmPreview`;
  veraltete `ProgramCatalogRevision` → abgelehnt (`StateChanged`); aktives
  Programm: `NotAllowed` (vor Preview). Alle anderen Mutationsfehler liefert der
  vorhandene Helfer als gemeinsamen Status `InvalidCandidate`
  (`CapacityReached`/`NotFound`/`NotAllowed` werden dort zusammengefasst); S6
  uebernimmt diese Grobheit bewusst (kein neuer Detail-Variant). Zweistufiges
  Loeschen und Factory-Uninstall/-Reset behalten ihre Semantik; ein laufender
  Run-Snapshot bleibt unveraendert.
- **Verifiziertes Verhalten ohne Editor:** `Copy` ohne Kandidat/Name erzeugt
  den kanonischen Namen „<Name> copy“ (`ui_editing.cpp:285-308`); `New` ohne
  Kandidat erzeugt aus der Vorlage „water-kefir“ ein Benutzerprogramm „New
  program“ mit zurueckgesetzten Laufparametern. Es ist nicht startbar, aber
  dank D12 weiterhin waehlbar, loeschbar und (mit S10) bearbeitbar. `Edit` ohne Kandidat liefert `InvalidCandidate` und bleibt in
  der UI deaktiviert. Reset, Uninstall, Delete, Copy und New sind damit ohne
  Editor nutzbar.
- **Tests:** neue Application-Tests (Reset/Uninstall/Delete/Copy/New,
  aktives Programm blockiert, Stale, Capacity, Persistenz ueber Neustart,
  Commit bestaetigt + reload, `cancelPreview` auf Fehlerausgaengen),
  `test_fermentation_ui_editing` (Konsument), `test_press_dispatcher`,
  `test_local_touch_ui`, `test_ui_steady_state_allocations` (Steady-State-/Resource-Regression),
  `test_configuration_service` Konsument.
- **Abhaengigkeiten:** nach S5 (gleiche Dateien in Application/Dispatcher/
  Bridge). **Ownerentscheidung:** keine. **RAM:** D10 (Hardware-Log nach Gate;
  kein Evict-Pfad).

### S7 – Seiteninhalte read-only und bewusst zurueckgestellt

- **Dateien/Owner:** Renderer-Seiteninhalt je Seite, Workspace-`view`
  (`blockedReason`), Textpacks.
- **Verhalten:** Inhalte kommen ausschliesslich aus Snapshot/Katalog:
  `ProgramSummary` (Programmname, Zieltemperatur, Dauer, Vorheizen,
  Sensor, Abschluss aus dem gewaehlten `ProgramDocument` plus Kandidaten-
  Overrides; die verbindliche `StartSummary` bleibt das Ergebnis des
  Owners), `Process` (`home.effectiveValues`, Prozesszustand),
  `Technical` (Temperaturen mit Qualitaet), `Completion`, `Status`,
  `Recovery`, `Messages`-Kopfzeile. `Diagnostics`, `Service`, `Pin` zeigen
  einen Text „zurueckgestellt (#28)“ statt leerer Flaeche; ihre Slots bleiben,
  wo sie sind (kein Funktionsversprechen). Recovery-Zeitkorrektur (4.1): Seite
  zeigt den Grund „nicht verfuegbar“, kein aktivierter Slot. Kein neuer
  Fachwert, keine Safety-Ableitung im Renderer.
- **Tests:** `test_renderer_boundary` (Befehlsanzahl je Seite begrenzt,
  deterministischer Inhalt), `test_local_touch_ui`,
  `test_ui_steady_state_allocations`.
- **Abhaengigkeiten:** S1–S4 (Layout D3), unabhaengig von S5/S6.
  **Ownerentscheidung:** keine.

### S8 – Tastenfeld und Startwerte

- **Dateien/Owner:** Workspace (`ValueEdit`-Seite, `StartField`-Enum),
  Renderer (Tastenfeld 4×3 im `ContentCell`-Raster, D8),
  `fermentation_ui_editing` (nur Konsum), Textpacks.
- **Verhalten:** `ProgramSummary`-Zeilen: Zieltemperatur, Dauer,
  Kuehlziel, Haltedauer (numerisch via Tastenfeld, D8); Vorheizen,
  Sensorbetrieb, Abschlussmodus (Zyklus pro Tap). Validierung D8; Ergebnis
  nur in `setStartCandidate` (naechster Lauf, kein Mutieren des Programms).
  Geaenderte Werte sind sichtbar gekennzeichnet. `Zuruecksetzen` gemaess O8-a.
- **Hardware-Abnahmekriterium:** Tastenhoehe 34 px treffbar (D8).
- **Tests:** `test_fermentation_ui_editing` (Tastenfolge → committed Wert),
  `test_local_touch_ui` (Kandidat → `StartProgram`-Payload enthaelt Overrides;
  ungueltiger Wert deaktiviert `confirm`), `test_renderer_boundary`,
  `test_run_commands` Konsument, `test_ui_steady_state_allocations`.
- **Abhaengigkeiten:** S1, S7. **Ownerentscheidung:** O8-a.

### S9 – Manueller Betrieb und Kuehlplaene (echte Laufwerte, Start fail-closed)

- **Dateien/Owner:** Workspace (`ManualField`-Enum, wiederverwendete
  `ValueEdit`-Seite), Renderer, Textpacks; `FermentationApplication` nur fuer
  die fail-closed-Aufloesung (siehe unten).
- **Verhalten:** Die normale Bedienung erfasst ausschliesslich **echte
  Laufwerte**: Zieltemperatur, bei `ManualTimed` die Dauer, Sensorwahl,
  Vorheizen sowie ggf. Kuehlziel/Halteverhalten (Abschluss-/Kuehlplaene fuer
  `stop-and-cool` und `cool-now`). `qualificationBandCelsius`,
  `qualificationDurationMinutes`, `maximumTargetReachMinutes` und vergleichbare
  technische Grenzwerte (O5-B) bezieht die **Application** von einem
  kanonischen, spaeter durch Commissioning (#34/#35) freigegebenen
  Produkt-/Service-Owner; sie sind nie UI-Eingabe und werden nie erfunden.
  Solange dieser Producer fehlt (heute der Fall), bleibt der produktive
  manuelle Start sowie der Kuehlplan-Start fail-closed/deferred; `confirm` ist
  deaktiviert und die Seite nennt den Grund („technische Laufparameter nicht
  freigegeben“). Der vorhandene Status `FermentationApplicationRequestStatus::
  Unavailable` wird wiederverwendet, kein neues Statuskonzept.
- **Vertragsfolge (Reviewfrage):** Die UI-Intents
  (`FermentationUiManualRunPlanValues`, `FermentationUiStartManualTimedIntent`/
  `ManualTimedRunValues`) tragen heute die Qualifikationsfelder. Ob sie aus dem
  UI-Intent entfernt oder application-seitig ueberschrieben werden, wird zu
  Slice-Beginn gegen #34/#35 und die dann integrierte #170-Baseline
  revalidiert; beruehrt die Aenderung Typen in `run_commands.hpp`, folgt eine
  Planrevision (materiell) statt einer stillen Aenderung. Die Form des Producers
  wird in #172 nicht festgelegt.
- **Tests:** `test_local_touch_ui` (Felder, Gruende, `confirm` deaktiviert),
  `test_press_dispatcher` (`Unavailable` ohne Producer), `test_run_commands`
  Konsument, `test_ui_steady_state_allocations`.
- **Abhaengigkeiten:** S8. **Ownerentscheidung:** O5 (B).

### S10 – Bildschirmtastatur, Programmeditor, Settings

- **Empfohlene Grundlage (Owner-Bestaetigung ausstehend):** O1-B, O3-A, O4-A. Die WLAN-Credentialtastatur
  ist ausgeschlossen (F16). Home Standby: Slot 1 `programs` → `settings`
  (Home-Aktionsmatrix und `test_local_touch_ui` entsprechend angepasst).
- **Dateien/Owner:** Workspace (`TextEdit`-Seite, `ProgramField`-Enum,
  `Settings`-Seite), Renderer (Tastatur), `fermentation_application`
  (`applyUserSettings` fuer `deviceName`, D5/O4, Kandidat nur nach
  `validateVisibleName`), `FermentationUiPresentationSource` (+`deviceName`,
  F9), Textpacks.
- **Verhalten:** `TextEditModel` mit Modi Klein/Gross/Ziffern/Symbole (Spec:
  Buchstaben, Ziffern, Leerzeichen, Bindestrich, wenige Sonderzeichen,
  Rueckschritt, Eingabe loeschen). Bottom-Slots: `Cancel` (0), `Mode` (1),
  `Backspace` (2), `Commit` (3); `Clear` als eigene Zelle im Raster. Die
  Spaltenkapazitaet (vorgesehen 10) und die genaue Tastenbelegung werden **erst
  hier** festgelegt und gemaess O3-A; Tastengroesse ist
  Hardware-Abnahmekriterium. Programmeditor deckt die laut
  `LOCAL_UI_PROGRAMS.md` mindestens lokal bearbeitbaren Felder ab, soweit sie
  im Programmmodell existieren (Name, Notiz, Zieltemperatur, Dauer, Vorheizen,
  Sensorvorschlag, Produktfuehler-Ausfallverhalten, maximale
  Zielerreichungszeit, Abschluss-/Kuehlverhalten); Speichern ueber
  `SaveProgram` → S6-Entry mit `expectedProgramCatalogRevision`. Settings-Seite:
  Sprache (Link), Zeitzone (lesend, UTC-gekennzeichnet, O2), Geraetename
  (Editor, nur ohne aktiven Lauf, O4), Netzwerk (Link), Webzugang (Link auf die
  bestehende `HeaderWebAccess`-Seite per vorhandener Aktion `NavigateWebAccess`;
  Freigabe, Fenster und Anzeige bleiben beim #170-Owner
  `FermentationApplication::openWebProvisioningWindow()` /
  `webAccessState()`, keine zweite Web-/Auth-/Provisionierungslogik). Der
  provisorische #170-Zugang ueber `HeaderLanguage` Slot 3 wird in S10
  **atomar** (ein Commit) ersetzt: zuerst `Webzugang` in `Einstellungen`
  einhaengen, danach den `web-access`-Slot aus `HeaderLanguage` entfernen (D13). Dirty-Verwerfen ueber den vorhandenen `ConfirmDiscard`-Exit.
- **Tests:** `test_fermentation_ui_editing` (Textfolgen, Grenzen
  `validateVisibleName`, UTF-8), `test_local_touch_ui`, neuer
  Application-Test (Geraetename-Commit, Gating bei aktivem Lauf, ASCII- und
  Mehrbyte-Namen), `test_softap_credentials` Konsument (SSID-Ableitung),
  `test_ui_steady_state_allocations`, Hardware-Log (D10).
- **Abhaengigkeiten:** S6, S8, S1. **Ownerentscheidung:** O1 (B), O3 (A), O4 (A).

### S11 – Dokumentation und Abschluss

`docs/ACCEPTANCE_TESTS.md` (neue SIM-IDs; begrenzte, ausdruecklich
ausgewiesene Berichtigung der falschen SIM-26-04..07-Testzuordnung gemaess
O8-b, nur die Tabellenverschiebung, keine Definitionsaenderung), `docs/LOCAL_UI.md`/`LOCAL_UI_SETTINGS_SERVICE.md`
nur dort, wo das reale Verhalten (Settings-Einstieg, Sprachseite, Zeitseite) von
der Beschreibung abweicht, `docs/ROADMAP.md`. Hardware-Smoke (Display/Touch,
Sprachwechsel mit Neustart, Programmverwaltung, Ressourcen-Logs) erst nach
Software-/Reviewgate und mit `ACTUATOR_RELEASE=NO`; Ergebnisse bis dahin
`NOT_RUN`.

## 8. Abhaengigkeiten und Reihenfolge

```text
PR A:  S1 ──► S2
       S1 ──► S3
       S1 ──► S4
PR B:  S5 ──► S6          (nach PR A)
PR C:  S7 ──► S8 ──► S9   (nach PR B)
PR D:  S10 ──► S11        (nach PR C; benoetigt S6, S8, S1)
```

- Die vier Implementations-PRs laufen **sequenziell** A → B → C → D, jeweils von
  aktuellem `main` (O6). Es gibt keine Parallelitaetsannahme.
- Innerhalb eines PR gilt die Slice-Reihenfolge; alle Slices mit
  `fermentation_ui_text.cpp` teilen die Tabellengroesse (`70U` ab `BASE_SHA`).
- Eine Implementation startet fruehestens nach Independent Plan Review und
  ausdruecklicher Ownerfreigabe der exakten Plan-SHA (Rebase und Revalidierung
  gegen den gemergten #170 sind erledigt, Abschnitt 9).

## 9. Integration mit PR #170 (revalidiert gegen den gemergten Stand)

PR #170 ist gemergt (`PR170_MERGE_COMMIT=02b7523b7dc3fdc82583c7939ecbab9eb9ec5dd7`,
O7-B erfuellt). Dieser Plan-PR ist auf diesen `main` rebased; `git diff
origin/main..HEAD` enthaelt nur `docs/ROADMAP.md` und diese Plandatei. Die
Konfliktflaechen wurden gegen den **tatsaechlich integrierten** Code geprueft
(`git diff 8a734f6..02b7523`, 49 Dateien). Jede Aenderung in #172 bleibt
additiv, ohne Umbenennung oder Umformatierung bestehender Bloecke.

| Datei | Befund im gemergten #170-Stand | Konsequenz fuer #172 |
|---|---|---|
| `fermentation_touch_workspace.hpp/.cpp` | `HeaderWebAccess`-Seite, `NavigateWebAccess`, `OpenWebProvisioningWindow`, Press-Feld `openWebProvisioningWindow` vorhanden; `HeaderLanguage` Slot 3 = `web-access` | Slot 3 bleibt; S3 ergaenzt nur Zeilen; S10 verlinkt per vorhandener Aktion `NavigateWebAccess` |
| `fermentation_ui_commands.hpp/.cpp` | `operation`-Variant enthaelt `…OpenWebProvisioningWindowCommand`; `FermentationUiDetailStatus` hat zwei `WebProvisioningWindow*`-Werte, kein neuer Detail-Variant noetig | #172 ergaenzt Operation-Varianten (`SetDisplayLanguage` usw.) und Bridge-Funktionen, keine neuen Detail-Varianten |
| `main/fermentation_ui_press_dispatcher.cpp` | eigener `if`-Zweig `openWebProvisioningWindow` (Z. 62-72); `transitionAction`/`programEdit` weiterhin `UnavailableNoOwner` (Z. 89-99) | #172 aendert nur den bestehenden `transitionAction`/`programEdit`-Zweig und ergaenzt eigene Zweige |
| `main/fermentation_ui_renderer.cpp` | **`HeaderLanguage`-Hit-Zone bereits vorhanden** (`kHeaderLanguageHitRect{176,0,44,32}`, `targetAt` Z. 636-642, Tests); `HeaderWebAccess`-Seiteninhalt (Z. 422-433); `HeaderClock`-Zone fehlt weiterhin; Programmliste unveraendert | D4 angepasst: Sprachzone wiederverwendet, nur Uhr-Zone neu; Programmlisten-Layout (S1) unveraendert geplant |
| `fermentation_ui_text.cpp` | Tabellengroesse `70U` in drei Arrays | #172-Eintraege setzen auf `70U` auf (F13) |
| `fermentation_application.hpp/.cpp` | `ApplicationCallSerializer` (rekursiv, jede oeffentliche Methode betritt ihn), `AuthOperationGate`, `webAccessState()`, `openWebProvisioningWindow()`, neue `begin()`-Overloads mit KDF/Replay-Digest; `applyNetworkMode` unveraendert (Muster, jetzt Z. 778) | neue Methoden betreten den Serializer (D5); `applyNetworkMode` bleibt unberuehrt; Auth-/Session-/Web-Owner werden nicht beruehrt |
| `main/app_main.cpp` | nur Komposition (KDF, Replay-Digest) ergaenzt; Locale-/Zeitzonenkopie fuer `HeaderNetwork` unveraendert (Z. 482-487, 511-532) | D11 unveraendert gueltig (F10) |
| `FermentationUiSnapshot` / Projector | neues Feld `webAccess` (Application-owned, nur Anzeige) | wird unveraendert gelesen, nicht dupliziert |
| `configuration_service.*`, `configuration_graph.*`, `fermentation_ui_editing.*` | im #170-Diff **nicht** enthalten; ausser `applyNetworkMode` ruft kein Application-/Web-Code `beginPreview`/`installPreview`/`confirmPreview` auf (Web nutzt nur `prepareEnvelope` mit `UiSurface::WebInterface`) | F14 und D5 („genau ein Pfad“) bleiben gueltig; Abschnitt 10 unveraendert |
| `scripts/check_architecture_boundaries.py`, `check_build_profiles.py`, `sdkconfig.defaults`, `platformio.ini` | erweitert um `cjson`/`mbedtls` und `CONFIG_CJSON_NESTING_LIMIT=4` | keine Konfliktflaeche; #172 fuehrt keine neue Abhaengigkeit ein |
| `docs/ROADMAP.md` | PR-#174-Zeile steht auf `main` bereits auf `MERGED`, #170-Zeile aktuell | nur #172-Zeile ergaenzt, Stand-Datum aktualisiert |

RAM/Stack: LVGL-Pool (`49152`) und Main-Task-Stack (`24576`) sind unveraendert;
der #170-Hardwarenachweis nennt keinen offenen Ressourcenblocker
(`OPEN_RESOURCE_BLOCKERS=0`, 1696-B-Follow-up nicht reproduziert und
nicht blockierend). D10 bleibt: keine vorsorgliche RAM-/LVGL-/HTTP-Optimierung.

Vor jedem Implementations-PR wird der Stand erneut revalidiert.

## 10. Gemeinsame Application-Owner fuer #27

Die in S3, S5, S6, S10 entstehenden Application-Methoden
(`applyDisplayLanguage`, `confirmProductInserted`, `applyProgramEdit`,
`applyUserSettings`) sind renderer- und transportneutral (nur typisierte
Eingaben plus erwartete Revisionen, keine Surface-Annahme ausser
`ChangeOrigin`). #27 darf sie spaeter mit `ChangeOriginKind::WebInterface` und
Wire-Wert `3U` aufrufen; ein zweiter Programm- oder Settings-Owner entsteht
nicht. #172 implementiert keine Web-Route, keine Auth, keine Session.

## 11. Teststrategie und Nachweise

- Nach jedem Slice: gezielte native Tests der genannten Verzeichnisse plus
  direkt betroffene Konsumententests. Kein vollstaendiger Pre-Ready-Lauf in
  Draft; `bash scripts/run_pre_ready_gates.sh self-check` erst nach der
  letzten Implementationsslice eines PR vor der Uebergabe an den Independent
  Review.
- Pro neuer Seite/Press-Art ein Eintrag in `test_ui_steady_state_allocations`.
- Persistenz-Nachweise (Sprache, Programmkatalog, Geraetename) mit der
  simulierten Persistenz ueber Neustart; Reale-Hardware-Nachweise
  `NOT_RUN` bis nach dem Gate. Nicht ausgefuehrte Tests gelten nicht als
  bestanden.
- ESP-Build beider Profile (`esp32_bringup`, `esp32_release`) wird erst im
  Self-Check bewertet; RAM-Vergleich (`R1_RAM_REFERENCE_LVGL_MEM_SIZE_BYTES=49152`
  bleibt die Referenz).

### 11.1 Hardware-Nachweisumfang je Slice

Ohne Meldungs-, Signal- und Sensorproduzenten (F15, F18, #30/#35) sind viele
laufabhaengige Pfade auf der Hardware nicht ausloesbar. Der Hardware-Smoke
(erst nach Software-/Reviewgate, `ACTUATOR_RELEASE=NO`) gilt je Slice so:

| Slice | Hardware-Smoke | Grund |
|---|---|---|
| S1 | anwendbar | nur Katalog |
| S2 | `NOT_APPLICABLE` | keine Meldungen (F18) |
| S3 | anwendbar | Persistenz ueber Neustart pruefbar; zusaetzlich Hardware-Log vor/nach Sprachcommit (D10) |
| S4 | anwendbar | reine Anzeige, `HH:MMZ` passt ins Uhrfeld |
| S5 | `NOT_APPLICABLE` | `WaitingForProduct` ohne Regelkreis nicht erreichbar (F15) |
| S6 | anwendbar | reine Konfiguration; zusaetzlich Hardware-Log (D10) |
| S7 | nur Layout | Temperaturen zeigen ohne Sensorproduzent `--.- C` |
| S8 | Eingabe anwendbar | Start bleibt ohne Laufevidenz fail-closed |
| S9 | Eingabe anwendbar | Start/Kuehlplan bleibt ohne Technikwert-Producer und Laufevidenz fail-closed (Grund sichtbar) |
| S10 | anwendbar | Konfiguration; Tastengroesse als Abnahmekriterium |

## 12. Risiken

| Risiko | Gegenmassnahme |
|---|---|
| Config-Commits sind unter Netz-/RAM-Last schon einmal an OOM gescheitert (PR #174). | Keine prophylaktische Eviction (D10, `FURTHER_PROACTIVE_RAM_OPTIMIZATION=NO`); S3/S6/S10 mit Steady-State-/Resource-Regression und spaeterem Hardware-Log; nur bei reproduziertem neuem OOM ein eigener gezielter Fix. |
| F14: ein bestaetigtes Programm-Preview scheitert voraussichtlich an falschen Wire-Werten. | S6 beginnt mit einem fehlschlagenden Bestaetigungs-/Reload-Test; Korrektur mit denselben kanonischen Werten wie `applyNetworkMode`. |
| `WaitingForProduct` ist im Produktbuild ohne Regelkreis kaum erreichbar (F15); Laufmeldungen haben keinen Erzeuger (F18). | Nativer Nachweis; Hardware-Nachweis je Slice gemaess 11.1; im PR ausgewiesen; O9. |
| Verwaltungsauswahl (D12) aendert den bestehenden `selectProgram`-Vertrag. | Bewusste Testanpassung in S1, im PR als Vertragsaenderung ausgewiesen. |
| Technische Qualifikationswerte fuer manuelle Laeufe haben keinen Producer (`TBD_COMMISSIONING`, O5-B). | Keine Benutzereingabe, keine Defaults; Start fail-closed mit sichtbarem Grund; #34/#35 revalidieren. |
| Header zeigt UTC ohne Kennzeichnung (F8); Zeitzonenkatalog hat einen Eintrag. | S4 kennzeichnet `HH:MMZ` in Header und Uhrseite; Lokalzeit als Folge-Scope (O2-A); Textbreite im 52-px-Feld als Abnahmekriterium. |
| Geraetename-Aenderung beeinflusst SSID/Hostname/QR (#164 B4). | O4-A: nur ohne aktiven Lauf, Netzwerkname erst beim naechsten normalen Netzwerkstart, kein Auto-Restart. |
| Tastenraster 34 px koennte fuer resistives Touch zu klein sein (D8). | Hardware-Abnahmekriterium; bei Verfehlen Plan-Revision, kein stilles Nachjustieren. |
| `ContentCell` fixiert einen Plattformvertrag frueh (D1). | Nur Indizes in der Plattform, Kapazitaeten in App-Schicht; Reviewfrage im Independent Plan Review. |
| Sprachnamen/Texte mit Sonderzeichen (z. B. `n` mit Tilde) sind im Standardfont nicht abgedeckt. | ASCII-Endonyme (`Espanol`) wie die bestehenden Packs; Glyphen-Pruefung im Textpack-Test. |
| Vorbestehende Fehlzuordnung der SIM-26-Traces (F17). | Begrenzte, ausgewiesene Berichtigung in S11 (O8-b). |

## 13. Abschluss dieser Revision

Revision 3 ist der vollstaendige Planstand nach Rebase auf `main` nach PR #170
und Revalidierung gegen den gemergten Code. Es erfolgen keine
Produktaenderungen, keine Aenderungen an PR #170 und keine Aenderungen an
`.codex/config.toml`.

```text
PR170=MERGED
PR170_MERGE_COMMIT=02b7523b7dc3fdc82583c7939ecbab9eb9ec5dd7
PR175_REBASE=PASS
PLAN_REVALIDATION_AGAINST_MERGED_PR170=PASS
PRODUCTION_CODE_CHANGED=NO
IMPLEMENTATION=NOT_STARTED
OWNER_DECISIONS_O1_TO_O9=PENDING
IMPLEMENTATION_AUTHORIZATION=NO
ACTUATOR_RELEASE=NO
```

Aenderungen gegenueber Revision 2 (Delta durch #170):

- F1/D4/S3/2.2/2.3: `HeaderLanguage`-Hit-Zone ist Bestand und wird
  wiederverwendet; S4 baut nur die Uhr-Zone; S4 ist nicht mehr von S3
  abhaengig.
- S3/S10/O1: `HeaderLanguage` Slot 3 `web-access` bleibt; Settings verlinkt
  auf den bestehenden `HeaderWebAccess`-Owner; der Slot-3-Uebergangspfad entfaellt
  atomar in S10 (D13).
- D5: neue Application-Methoden betreten `ApplicationCallSerializer`.
- F13: Textarrays `70U`; Zeilenbezuege auf `BASE_SHA` aktualisiert.
- D10: Baseline und #170-Referenznachweis ergaenzt, keine neue Optimierung.
- Abschnitt 9: Konfliktflaechen gegen gemergten Stand beantwortet.

O2–O9 sind gegen den Code unveraendert gueltig; O1 ist um die
Webzugang-Zielstruktur (D13) ergaenzt. Naechster Schritt: Independent Plan Review der neuen exakten
Plan-SHA und ausdrueckliche Ownerentscheidung O1–O9.
