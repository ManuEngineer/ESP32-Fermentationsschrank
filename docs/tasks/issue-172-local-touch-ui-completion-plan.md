# Issue #172 – R1 lokale Touch-UI funktional vervollstaendigen

## Planstatus und Baseline

```text
ISSUE=172
SCOPE=R1_LOCAL_TOUCH_UI_FUNCTIONAL_COMPLETION
BASE_BRANCH=main
BASE_SHA=8a734f62836f8c57263c76ceebfc36a49f4af77c
PLAN_STATUS=DRAFT_INDEPENDENT_PLAN_REVIEW_AND_OWNER_APPROVAL_REQUIRED
IMPLEMENTATION=NOT_STARTED
IMPLEMENTATION_AUTHORIZATION=NO
PR171=MERGED
PR170=OPEN_DRAFT_PARALLEL_SCOPE_NOT_MODIFIED
PR174=MERGED
PR167=SUPERSEDED_REFERENCE_ONLY
OWNER_DECISIONS_PENDING=9
ACTUATOR_RELEASE=NO
```

Dieser Plan ist das Ergebnis des Audits aus dem Auftrag
`Issue172_Plan_Audit_Auftrag.md`. Er enthaelt **keine** Produktimplementation.
Die Umsetzung beginnt erst nach Independent Plan Review, Ownerfreigabe der
exakten Plan-SHA und Revalidierung der Konfliktflaechen gegen den dann
aktuellen Stand von PR #170 und `main` (Abschnitt 9). Dieser PR ist ein
**reiner Plan-PR** (Praezedenzfall PR #171); die Implementations-PRs entstehen
danach von `main` (Abschnitt 6).

## 1. Ziel und Nicht-Ziele

Ziel: Jede auf R1-Hardware sichtbare, aktivierte Schaltflaeche und jede
verlangte lokale Eingabe erreicht ihren bereits bestehenden Fach-/Config-/
Persistenz-Owner, oder sie ist bewusst deaktiviert bzw. zurueckgestellt und
nennt einen Grund. Es entsteht keine neue Fach-, Config-, ProgramCatalog-,
Message-, Recovery- oder Persistenzwahrheit.

Nicht-Ziele (unveraendert aus dem Issue):

- #164: WLAN-/QR-/Browser-Setup und dessen Hardware-Evidence.
- #27 / PR #170: Web, API, Auth, Session, CSRF. `HeaderWebAccess` und
  `OpenWebProvisioningWindow` bleiben unberuehrt.
- #28: Diagnose, Service, PIN-Ablauf, Charts, Exporte.
- #30/#32/#33/#34/#35: Sensor-, Ausgangs-, Peltier-, Commissioningwerte.
- Keine UI-Plattform, kein Widget-/Event-/Formframework, keine neue ADR.
- Keine Werte aus `TBD_HARDWARE`, `TBD_COMMISSIONING`,
  `TBD_IMPLEMENTATION_BUDGET` als Laufzeitwert.

## 2. Verifizierte Ausgangslage (Audit, Stand `BASE_SHA`)

### 2.1 Strukturbefunde

| Nr. | Befund | Nachweis |
|---|---|---|
| F1 | `targetAt()` kennt nur `HeaderNetwork` (Rect 220/4/44/18) und die vier BottomSlots. `HeaderLanguage` und `HeaderClock` sind im Renderer gezeichnet (Sprachcode x=188..220, Uhr x=264..316), aber nicht treffbar. Es gibt kein Content-Target. | `main/fermentation_ui_renderer.cpp:345-355`, `:613-633` |
| F2 | `DeviceUiTargetKind` kennt `None, HeaderLanguage, HeaderNetwork, HeaderClock, BottomSlot, HomeOrBack, PagerUp, PagerDown, Confirm, Cancel, Back`. Kein Inhalts-/Zeilentarget. | `lib/device_platform/src/device_ui_interaction.hpp:10-22` |
| F3 | Alle zwoelf Workspace-Setter (`selectProgram`, `setStartCandidate`, `setManualHoldingValues`, `setManualTimedValues`, `setStopCoolingPlan`, `setCompletionCoolingPlan`, `setSelectedMessage`, `setProgramEditCandidate`, `setProgramEditOperation`, `setProgramEditDirty`, `setSensorSelectionAction`, `setRecoveryTimeCorrectionSeconds`) haben **0 produktive Aufrufer** (Suche in `main/`, `src/`, `lib/`), nur Testaufrufer. | Grep, Audit-Lauf |
| F4 | `NumericEditModel` / `TextEditModel` haben ausserhalb von `fermentation_ui_editing.*` und Tests keinen Konsumenten. `NumericEditAction::Plus/Minus` sind **Vorzeichenaktionen**, keine Schrittweiten. | `fermentation_ui_editing.cpp:48-93` |
| F5 | Der Dispatcher meldet `transitionAction` und `programEdit` als `UnavailableNoOwner`; alle anderen Press-Payloads sind verdrahtet. | `main/fermentation_ui_press_dispatcher.cpp:78-90` |
| F6 | Die Programmliste zeichnet immer die Eintraege 0..2 (18-px-Zeilen) und ignoriert `pager.currentIndex`; Auf/Ab aendert nur den Zaehler `n/N`. 18 px sind fuer resistives Touch zu klein (BottomSlots: 40 px). | `fermentation_ui_renderer.cpp:461-477`, `:503-511` |
| F7 | `NavigateMessageDetail` waehlt stets die **erste** aktive, unaufgeloeste Meldung und ignoriert den Pager. | `fermentation_touch_workspace.cpp:1126-1138` |
| F8 | Der Zeitzonenkatalog enthaelt genau einen Eintrag (`Europe/Zurich`); `PreparedTimeZone` traegt nur den Bezeichner, keinen Offset; der Port enthaelt ausdruecklich keine Zeitzonendatenbank. Der Header zeigt UTC (`formatClockText`). Eine Zeitzonen-„Auswahl“ waere ein No-op, obwohl das Issue die Aenderung der Zeitzone in normalen Einstellungen zulaesst. | `firmware_configuration_catalog.cpp:8-9`, `time_zone_resolver.hpp:15-26`, `fermentation_ui_renderer.cpp:178-193` |
| F9 | `FermentationUiPresentationSource` enthaelt `displayLocale`, `canonicalTimeZoneId`, `programCatalog`, aber **kein** `deviceName`. | `fermentation_ui_models.hpp:132-135` |
| F10 | `app_main` uebergibt `initialDisplayLocale` / `initialTimeZoneId` (beim Boot erfasst) an `renderGate.renderRequired()` und als Locale/Zeitzone der Netzwerkseite. Auf `HeaderNetwork` ist die Presentation-Kopie verdraengt; `get()` liefert dort die Defaults (Englisch). Nach einem Sprachwechsel zeichnet der Render-Key nicht neu bzw. die Netzwerkseite bliebe bis zum Reboot in der alten Sprache. | `main/app_main.cpp:440-441`, `:481-484`, `:510-527`; `fermentation_ui_presentation_cache.hpp` (`update`, `evict`, `get`) |
| F11 | Es gibt keine Settings-/Menue-Seite in `FermentationUiPage`. Die vier Home-Slots sind belegt (Standby: `start` und `programs` zeigen beide auf die Programmliste). | `fermentation_touch_workspace.cpp:311-324` |
| F12 | `SensorSelectionUserAction` wird nur von `setSensorSelectionAction()` gesetzt. Es gibt **keine reine Abfrage** „zulaessige Aktionen“: die Zulaessigkeit entscheidet `decideApplySensorSelectionAction` ueber `applySensorSelectionDecision` mit Program-Kontext, Plausibilitaet und `criticalSafetyEventPending`; `CommandDecision` traegt einen kompletten `RunCommandState`. `MessageView` traegt aber `code`/`decisionRequired` (`MessageCode::UserDecisionRequired`). | `run_commands.cpp:1343-1418`, `fermentation_ui_models.hpp:58-60` |
| F13 | Die Textpacks (DE/ES/EN) existieren; jede neue Taste braucht Eintraege in allen drei Packs; die Tabellengroesse ist als Literal (`65U`) in drei Arrays kodiert. | `fermentation_ui_text.cpp:22`, `:97`, `:174` |
| F14 | **Latenter Defekt (durch Code-Lesung belegt, nicht ausgefuehrt):** `applyProgramEditPreview` installiert mit `{LocalDisplay, 0U}` und `{NormalEdit\|StandardProgramReset, 0U}`. `validChangeOrigin/Operation` verlangen `LocalDisplay==2U`, `NormalEdit==1U`, `StandardProgramReset==6U`; `encodeConfigurationManifestPayload` lehnt jedes Manifest mit `!isPlausible` ab. Kein Test bestaetigt ein von `applyProgramEditPreview` erzeugtes Preview (die Tests brechen es ab oder lehnen es vorher ab), es gibt keinen produktiven Aufrufer. Ein Commit eines solchen Previews scheitert daher voraussichtlich bei der Persistierung. `applyNetworkMode` nutzt die kanonischen Werte `{LocalDisplay,2U}`/`{NormalEdit,1U}`. | `fermentation_ui_editing.cpp` (`applyProgramEditPreview`), `configuration_graph.cpp:28-58,108-138`, `configuration_graph_codec.cpp:88-100`, `test_configuration_service.cpp:773-790,833-880` |
| F15 | Die Prozess-State-Machine hat in `FermentationApplication` **keinen zyklischen Pfad**: `update()` pollt nur `networkLifecycle_` und `reevaluateWaitingForTrustedTime()`. Das Application-Objekt komponiert weder `TemperatureControlApplicationOrchestrator` noch einen Fault-/Signalproduzenten. `ProcessSignals{}` ist die einzige bestehende Signalquelle (acht Stellen im `RunPersistenceCoordinator`). Der einzige Eintritt in `WaitingForProduct` ist die automatische Entscheidung `PreheatQualified` aus `Preheating`, die `signals.qualificationProgress == Complete` verlangt (`process_state_machine.cpp:417-421`), sowie `RecoveryResume` einer zuvor persistierten Wartephase (`:680`). `StartRun` fuehrt nicht direkt dorthin. Folge: Ohne Signalproduzent/Regelkreis (#30/#35) ist `WaitingForProduct` im Produktbuild nur nach einer wiederhergestellten Wartephase erreichbar. | `fermentation_application.cpp:1360-1365`, `process_state_machine.cpp:417-421,680`, `run_persistence_coordinator.cpp:1237,1285,1475,1503,1836` |
| F16 | **Dokumentwiderspruch Bildschirmtastatur:** `LOCAL_UI_PROGRAMS.md` („Bildschirmtastatur“, Mindestanforderungen, lokal bearbeitbarer Programmname/Notiz) verlangt sie; `FUTURE_SCOPE.md` (R1_TOUCH_WIFI_KEYBOARD=DEFERRED) erklaert „die dafuer erforderliche Bildschirmtastatur“ fuer nicht Teil von R1, bezogen auf die HOME_WIFI-Credentialeingabe. Beide stehen nicht in `SPECIFICATION_REVIEW.md` als aufgeloest. | `docs/LOCAL_UI_PROGRAMS.md` (Abschnitt „Bildschirmtastatur“), `docs/FUTURE_SCOPE.md:74-82` |
| F17 | **Vorbestehende Abweichung in `ACCEPTANCE_TESTS.md`:** Die Definitionen SIM-26-04..07 (Zeilen 215-218) und die Testzuordnungstabelle (Zeilen 238-241) passen nicht zueinander (z. B. SIM-26-04 `ProductInsertedConfirmed` zeigt auf Locale-/Clock-Tests; SIM-26-06 `PIN` zeigt auf Editor-Tests). Dieser Plan haengt nichts um; Klaerung siehe O8 in Abschnitt 5 und 12. | `docs/ACCEPTANCE_TESTS.md:212-241` |
| F18 | **Kein Produktions-Erzeuger fuer Laufmeldungen:** In `lib/fermentation_app/src` schreibt nichts in `RunCommandState::messages`/`messageCount`; es gibt nur Lesestellen (`run_commands.cpp:356,1511`). `MessageCode::UserDecisionRequired` und `ProductInsertionRequested` werden im Produktcode nur gelesen (`fermentation_ui_projector.cpp:16-17`); die `ProcessMessage`-Eintraege einer `TransitionDecision` werden nirgends in `RuntimeMessage` uebersetzt. Folge: Die Meldungsliste ist auf der Hardware leer; Auswahl, Quittieren und Stummschalten sind nur nativ mit Fixtures nachweisbar (O9). | Grep ueber `lib/fermentation_app/src/*.cpp/hpp` |

### 2.2 Inventar je Issue-Luecke (Ist-Pfad bis zum Owner)

| # | Luecke | Ist-Pfad | Bestehender Owner | Fehlt | Slice |
|---|---|---|---|---|---|
| 1 | Header-Sprache | Header zeigt Sprachcode; `Workspace::press(HeaderLanguage)` navigiert (`:1276`); Seite `HeaderLanguage` hat nur Cross-Navigation (`:751-757`); `targetAt()` trifft nichts (F1) | `UserConfiguration.displayLanguageId` via `ConfigurationService`; Muster `applyNetworkMode` (`fermentation_application.cpp:706-735`); Sprachkatalog `kLanguages{de,es,en}` | Hit-Zone, Auswahlzeilen, Application-Entry, eviction-feste Locale-Kopie (F10) | S3 |
| 2 | Header-Uhr/Zeitzone | `HeaderClock` Seite nur Cross-Navigation (`:776-782`); Uhr nicht treffbar | `ITimeSource`/`ClockViewInput{trustedUtc, tz}`; Zeitzonenkatalog (F8) | Hit-Zone, Status-Inhalt; Zeitzonenauswahl nicht sinnvoll moeglich (O2) | S4 |
| 3 | Normale Settings | keine Seite (F11); `UserConfiguration` hat `displayLanguageId`, `timeZoneId`, `deviceName`, `activeThemeId`, `networkMode` | `UserConfiguration` / `validateUserConfiguration` | Seite, Einstieg (O1), Geraetename-Editor, Application-Entry | S10 |
| 4 | Programmlisten-Auswahl | Liste gezeichnet; `selectProgram()` ohne Aufrufer (F3); kein Row-Target (F1/F2); Pager-Fenster fehlt (F6) | `Workspace::selectProgram` → `ProgramSummary` | Content-Target, Pager-Fenster, Zeilenhoehe | S1 |
| 5 | Programmverwaltung | Aktionen (Reset/Uninstall/Delete/Save) erzeugen `programEdit`-Request; Dispatcher: `UnavailableNoOwner` (F5); Helfer mit falschen Wire-Werten (F14) | `applyProgramEditPreview()` + `ConfigurationService` + `ProgramCatalogRevision`-Staleness | `FermentationApplication`-Entry, Wire-Wert-Korrektur, Dispatcher | S6 |
| 6 | Programmeditor/Startwerte | `setStartCandidate`, `setProgramEditCandidate` nur Tests (F3); Edit-Modelle ohne Konsument (F4) | `validateProgram`, `makeFermentationUiProgramList`, `openProgramEditSession`, Preview-Pfad, `prepareStartProgram` | Editor-Seiten, Tastenfeld/Tastatur, Feldzuordnung | S8, S10 |
| 7 | Manueller Betrieb/Kuehlplaene | `setManualHoldingValues`, `setManualTimedValues`, `setStopCoolingPlan`, `setCompletionCoolingPlan` nur Tests (F3); Confirm-Slots dauerhaft disabled | `validateManualRunPlan`, `prepareStartManualHolding/Timed`, `prepareStop`, `prepareCompletion` | Eingabeseiten; **Herkunft der Qualifikationswerte ungeklaert** (O5) | S9 |
| 8 | `ProductInsertedConfirmed` | Slot + Intent vorhanden; Dispatcher `UnavailableNoOwner` (F5); `decideProductInsertedConfirmed` ist nur eine Entscheidung ohne Apply | `decideProcessTransition()` (`process_state_machine.cpp:746`), `RunPersistenceCoordinator::persistTransition()` (`:2798`, `ProductInserted` ist in `eligibleTransition`) | `FermentationApplication`-Entry, Bridge, Dispatcher | S5 |
| 9 | Meldungen | Seiten `Messages`/`MessageDetail`, Ack/Mute-Slots, Intents und RAM-Apply (`applyConfirmedPrepared`, `:562-567`) vorhanden; keine Auswahl (F3/F7); Sensorentscheidung ohne Datenquelle (F12) | `decideAcknowledgeMessage/MuteMessage`, `decideApplySensorSelectionAction` | Row-Target, Detail-Inhalt, Aktionsliste aus `MessageView` | S2 |
| 10 | Recovery-Zeitkorrektur | Slot `ApplyRecoveryTimeCorrection` nur bei gesetztem Wert (F3); Owner `decideApplyRecoveryTimeCorrection` verlangt eine **vom Benutzer gelieferte** `secondsDelta` innerhalb der Ausfallgrenzen (`run_commands.cpp:1229-1290`) | `ApplyRecoveryTimeCorrectionRequest` | Kein R1-Benutzerpfad spezifiziert: nur `RECOVERY_AND_INTERRUPTION.md:407` und `RUN_PERSISTENCE.md:382` (Semantik), keine UI-Anforderung | S7: zurueckgestellt, siehe 4.1 |
| 11 | Leere/unvollstaendige Seiten | Renderer zeichnet fuer alle Seiten ausser Home, Programmliste, Bestaetigungsname, Netzwerk nur Titel und Slots (`fermentation_ui_renderer.cpp:460-501`) | Snapshots (`FermentationUiSnapshot`), `ProgramCatalog` | Inhalte je Seite | S7 |

### 2.3 Vollstaendiges Seiten-/Element-Inventar (Header, Seiten, Slots)

Legende: `OK` = Wirkung bis zum Owner verdrahtet und erreichbar;
`NAV` = reine Navigation, funktioniert; `DEAD` = sichtbar/aktiv, aber ohne
produktive Wirkung oder unerreichbar; `DEF` = zurueckgestellt mit Verweis;
Slot-Nummern `0..3` links nach rechts.

| Seite / Element | Slot / Zone | Ist-Wirkung | Owner / Ziel | Status → Slice bzw. Verweis |
|---|---|---|---|---|
| Header | Logo | nicht interaktiv (UI-11) | – | OK (bewusst) |
| Header | Sprache | gezeichnet, nicht treffbar | `UserConfiguration` | DEAD → S3 |
| Header | WLAN | → `HeaderNetwork` | #164 | OK |
| Header | Uhr | gezeichnet, nicht treffbar | `ClockViewInput` | DEAD → S4 |
| Home Standby | 0 `start`, 1 `programs` | → `ProgramList` (doppelt belegt, F11) | – | NAV; Doppelbelegung dokumentiert (O1-Option B) |
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
| ManualHolding / ManualTimed | 2 `confirm` | dauerhaft deaktiviert (F3) | `prepareStartManual*` | DEAD → S9 |
| ManualHolding / ManualTimed | Felder | keine | `validateManualRunPlan` | DEAD → S9 |
| Process | `stop`, `programs`, `technical`, `status` | NAV; Inhalt leer | Snapshots | NAV; Inhalt → S7 |
| StopDialog | 1 `stop-turn-off` | `StopRun` verdrahtet | `prepareStop` | OK |
| StopDialog | 2 `stop-and-cool` | deaktiviert (kein Kuehlplan, F3) | `prepareStop` | DEAD → S9 |
| Technical | `up`, `down`, `messages` | kein Inhalt/Pager leer | Snapshots | DEAD → S7 |
| Messages | `up`, `down` | Pager, keine Zeilen | `snapshot.messages` | DEAD → S2 |
| Messages | 3 `details` | → `MessageDetail` mit „erster aktiver“ (F7) | – | DEAD → S2 |
| MessageDetail | 1 `acknowledge`, 2 `mute` | deaktiviert ohne Auswahl (F3) | `decideAcknowledge/MuteMessage` | DEAD → S2 |
| MessageDetail | 3 `continue` (Sensoraktion) | nur mit `setSensorSelectionAction` (F3/F12) | `decideApplySensorSelectionAction` | DEAD → S2 |
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
| HeaderLanguage | 1 `network`, 2 `clock`, 3 (#170: `web-access`) | NAV | – | NAV; Auswahl → S3 |
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

**D4 – Header-Hit-Zonen ohne Ueberlappung.** Neue Zonen
`HeaderLanguage` x=184..220 und `HeaderClock` x=264..320, jeweils ueber die volle
Headerhoehe y=0..32. Die bestehende `HeaderNetwork`-Zone (x=220..264, y=4..22)
wird **nicht** veraendert; die Zonen ueberlappen nicht (Test: Randpixel
219/220/263/264).

**D5 – Commit-Pfad fuer UserConfiguration/ProgramCatalog.** Genau ein Pfad:
neue `FermentationApplication`-Methoden nach dem Muster von `applyNetworkMode`
(`beginPreview` → Kandidat aendern → `installPreview` mit den **kanonischen**
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

**D10 – RAM (Kontext PR #174, ohne PSRAM, LVGL-Pool 48 KiB).**
- Neue Seiteninhalte nutzen den vorhandenen `ScreenDrawCommand`-Pfad;
  Befehlsanzahl je Seite wird begrenzt und im Test geprueft (Muster
  `kNetworkScreenDrawCommandCapacity`).
- `test_ui_steady_state_allocations` wird fuer jede neue Seite und jede neue
  Press-Art erweitert (kein Heap-Zuwachs im Steady State).
- **Eviction-Mechanik fuer mutierende Katalog-Commits (S6, S10).**
  `HeaderNetwork` verdraengt die Kopie fuer die ganze Seitendauer, weil sie
  keinen Katalog rendert. Programmseiten rendern den Katalog; deshalb muss die
  Verdraengung **zwischen Routing und Owner-Mutation** liegen. Heute buendelt
  `processWorkspaceTouch` beides: `makeRepresentativeScreen`/`routePress` laufen
  in einem Block, dessen Vektor-/String-Speicher vor dem Dispatch zerstoert wird;
  danach ruft es `dispatchWorkspacePress`. Entscheidung: `processWorkspaceTouch` wird in zwei
  Funktionen **geteilt** – `routeWorkspaceTouch(...)` (Screen-Block, Hit-Test,
  `routePress`; liefert `pressedTarget` und die typisierte `FermentationUiWorkspacePress`)
  und das bestehende `dispatchWorkspacePress(...)`. `updateProductUi` ruft
  dazwischen `renderGate.evictCatalogForMutation()` auf, wenn
  `press.programEdit` bzw. eine Settings-Mutation gesetzt ist. Ein Callback
  oder Hook wird nicht eingefuehrt (waere ein kleiner Event-Mechanismus). Der
  naechste `beginStep` fuellt die Kopie ueber den vorhandenen
  Revisionsmechanismus neu. **Lebensdauer:** Nach dem Haken darf kein Zugriff
  mehr auf Referenzen in die verdraengte Kopie erfolgen (die
  `touchDisplayLocale`/`touchTimeZoneId`/`touchProgramCatalog`-Referenzen in
  `updateProductUi` werden nur im Routing-Block benutzt); Locale/Zeitzone
  werden vor dem Haken als kleine Werte kopiert (siehe D11). Ein Test
  (Steady-State + Dispatch-Zyklus mit Evict/Refill) belegt, dass nach dem Haken
  kein Zugriff auf die verdraengte Kopie stattfindet.
- Hardware-Ressourcen-Logpunkte (`logResources`) vor/nach Commit-Presses
  analog `network_page_press_*`; Hardware-Nachweise erst nach dem
  Software-/Reviewgate und mit `ACTUATOR_RELEASE=NO`. Budgetgrenzen werden
  nicht erfunden; sie bleiben `TBD_IMPLEMENTATION_BUDGET` bis zur finalen
  R1-Integrationsqualifikation (PR #174).
- Neue Textschluessel kosten Flash, keinen Heap.

**D11 – Eviction-feste Locale/Zeitzone (Korrektur F10).** Der Owner ist
`FermentationUiPresentationCache` (`fermentation_ui_presentation_cache.hpp`),
weil nur dort sichtbar ist, ob ein Fill erfolgreich war. Er haelt zwei kleine
Werte (`LocaleId` ≤ 16 Byte, `TimeZoneId` ≤ 64 Byte), die bei **jedem
erfolgreichen Fill** aktualisiert werden und von `evict()` **nicht** verworfen
werden (`evict()` verwirft nur Katalog und Revisionen). Zugriff ueber zwei
neue Lese-Accessoren; `UiRenderGate` (`main/fermentation_ui_press_dispatcher.hpp`)
und `app_main` verwenden sie statt `initialDisplayLocale`/`initialTimeZoneId`
fuer Render-Key, Netzwerkseite und den Zeitraum nach einem Evict. Damit bleibt
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
- **Sensorentscheidung (Aktionsliste):** Statt einer neuen Application-
  Projektion listet `MessageDetail` fuer eine ausgewaehlte Meldung mit
  `code == UserDecisionRequired` die drei `SensorSelectionUserAction`-Werte als
  Zeilen; Slot 3 `continue` wendet die gewaehlte an. Die Zulaessigkeit
  entscheidet allein `decideApplySensorSelectionAction` (Ablehnung wird als
  `Rejected`/`NoChange` angezeigt, fail-closed). Damit entfallen neue
  Snapshot-Felder, Stackkosten pro `refreshUiSnapshot` und eine Konfliktflaeche
  zu #170 in `fermentation_ui_models`/`projector`. Reviewfragen: (1) Es gibt
  keinen Produktions-Erzeuger fuer `UserDecisionRequired`-Meldungen (F18); die
  feste Liste bleibt eine Annahme, bis ein Erzeuger existiert. (2) Je nach
  Sensorphase werden einzelne Zeilen stets abgelehnt; das ist nach
  Akzeptanzkriterium „reale Wirkung oder deaktiviert mit Grund“ nur mit
  sichtbarem Ablehnungsgrund zulaessig.

## 5. Offene Ownerentscheidungen

Jede Entscheidung nennt die Empfehlung; ohne Entscheidung bleibt der betroffene
Slice im genannten Zustand. Der Agent entscheidet keine davon selbst.

| Nr. | Frage | Optionen | Empfehlung | Betrifft | Ohne Entscheidung |
|---|---|---|---|---|---|
| O1 | Wo ist die normale Einstellungsseite erreichbar? | (A) Quernavigation von der Header-Uhrseite (Slot 3 `settings`), kein Home-Umbau, kein #170-Konflikt; (B) Home-Slot umwidmen (Standby: `start` und `programs` sind redundant); (C) ueber die Statusseite | A | S10 | S10 nicht startbar |
| O2 | Zeitzone: Das Issue erlaubt die Aenderung in normalen Einstellungen, aber der Katalog hat 1 Eintrag, es gibt keinen Offset-Vertrag, der Header zeigt UTC (F8). | (A) Zeitzone lesend anzeigen; Auswahl/Lokalzeit als eigenes Folge-Issue (Owner legt an oder nimmt in `FUTURE_SCOPE.md` auf); (B) Katalog-/Offsetvertrag in #172 (neue Zeitwahrheit, eigener Plan) | A | S4, S10 | Uhrseite nur Status; kein Eintrag in Settings |
| O3 | Bildschirmtastatur im R1-Touchscope? Dokumentwiderspruch F16 (`LOCAL_UI_PROGRAMS.md` verlangt sie, `FUTURE_SCOPE.md` schliesst sie aus R1 aus). | (A) in #172 fuer Programmname/Notiz/Geraetename (setzt Klaerung des Widerspruchs in `FUTURE_SCOPE.md` voraus); (B) deferred, Namen/Notizen bleiben deaktiviert mit Grund | A, mit Dokumentkorrektur durch den Owner | S10 | S10 nur ohne Texteingabe |
| O4 | Geraetename aendern: Aenderung wirkt erst beim naechsten Network-Restart auf SSID/Hostname/QR (#164 B4); laut `LOCAL_UI_SETTINGS_SERVICE.md` ist waehrend eines Laufs nur die Sprache Komfortwert. | (A) nur ohne aktiven Lauf, Hinweis „wirkt nach Netzwerk-Neustart“, kein Auto-Restart; (B) zusaetzlich sofortiger Hostname-/Netzwerk-Neustart | A | S10 | Geraetename read-only |
| O5 | Manuelle Laufplaene und Kuehlplaene: Zielband, Qualifikationsdauer, maximale Zielerreichungszeit haben keine Default-Quelle im Code (`ManualRunPlanRequest`). | (A) alle Werte explizit eingeben; Bestaetigen bleibt bis zur gueltigen Vollstaendigkeit deaktiviert; (B) benannter Produktkonstanten-Owner mit konkreten Werten (Werte noetig, evtl. #35) | A | S9 | S9 nicht startbar |
| O6 | PR-/Review-Schnitt (Abschnitt 6). | (A) dieser PR plan-only, danach vier Implementations-PRs von `main` (A∥B parallel); (B) ein PR mit allen Slices | A | alle | A als Arbeitsannahme fuer die Planung |
| O7 | Rebase-Reihenfolge #170/#172 (Abschnitt 9). | (A) #172 zuerst, #170 rebased und revalidiert; (B) #170 zuerst | A (PR #170 ist an die 1696-B-Diagnose bzw. einen Owner-Waiver gebunden, `docs/ROADMAP.md`) | alle | A als Arbeitsannahme |
| O8 | (a) `ProgramSummary`: `Zuruecksetzen` braucht einen Slot. (b) Vorbestehende Abweichung F17 in `ACCEPTANCE_TESTS.md`. | (a) A: Slot 3 wird `reset`, solange der Kandidat von den Programmwerten abweicht, sonst `status`; B: Zeile in der Feldliste. (b) Owner entscheidet, ob die SIM-26-Zuordnung in diesem Scope bereinigt wird oder ein eigenes Dokument-Issue entsteht. | (a) A; (b) separates Dokument-Issue | S8, S5, S11 | (a) S8 ohne Reset (deaktiviert mit Grund); (b) SIM-26-Zuordnung unveraendert, neue #172-IDs separat |
| O9 | Meldungserzeuger fehlt (F18): Das Akzeptanzkriterium „Meldungen koennen real ausgewaehlt, quittiert und stummgeschaltet werden“ ist auf Hardware nicht nachweisbar, solange nichts `RunCommandState::messages` fuellt. | (A) #172 liefert Auswahl/Ack/Mute/Sensorentscheidung nativ nachgewiesen; Erzeuger (Uebersetzung `ProcessMessage`/Fault → `RuntimeMessage`) als eigenes Folge-Issue, vom Owner angelegt; (B) Erzeuger in #172 aufnehmen (materieller Scope, neue Planrevision) | A | S2 | S2 nativ; Hardware-Nachweis `NOT_APPLICABLE` |

## 6. PR-Schnitt und Gates

Elf Slices in einem PR wuerden dem Grundsatz „zusammenhaengender Scope, klein
und unabhaengig reviewbar“ widersprechen. Empfehlung (O6-A): ein Issue, ein
freigegebener Plan, **dieser PR ist plan-only** (Praezedenzfall: PR #171
„docs(issue-164): plan local network touch completion“ wurde als reiner
Plan-PR gemergt). Nach der Freigabe der exakten Plan-SHA und dem Merge dieses
PR entstehen **vier Implementations-PRs**, jeder von kanonischem `main` (kein
gestapelter PR ohne ausdrueckliche Ownerfreigabe).

| PR | Slices | Inhalt | Basis |
|---|---|---|---|
| A | S1–S4 | Touch-Navigation: Content-Target, Programmlistenauswahl, Meldungsauswahl, Header-Sprache, Header-Uhr | `main` nach Plan-PR |
| B | S5–S6 | Application-Owner: ProductInserted, ProgramCatalog-Mutation, Wire-Wert-Korrektur | `main` nach Plan-PR; parallel zu A erlaubt |
| C | S7–S9 | Seiteninhalte, Tastenfeld, Startwerte, manuelle Eingabe | `main` nach A |
| D | S10–S11 | Tastatur, Programmeditor, Settings, Dokumentation | `main` nach A, B, C |

Jeder Implementations-PR zitiert die vom Owner freigegebene Plan-SHA des
Plan-PR und nennt seinen Slice-Bereich. Vor jedem PR-Start wird der Stand aus
Abschnitt 9 revalidiert und der Owner bestaetigt den Start; Planabweichungen
folgen `AGENT_WORKFLOW.md` §6. Pro PR gelten Independent Review,
Pre-Ready-Gate und CI getrennt. Wird stattdessen O6-B (ein PR) gewaehlt, bleibt
die Slice-Reihenfolge, aber alle Slices liegen als Commits in einem PR.

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
- **Ownerentscheidung:** keine. **Parallelisierbar zu S5/S6** (PR A ∥ PR B).

### S2 – Meldungsauswahl, Ack/Mute, Sensorentscheidung

- **Dateien/Owner:** `fermentation_touch_workspace.{hpp,cpp}`, Renderer,
  `fermentation_ui_text.cpp`. **Keine** Aenderung an `FermentationApplication`,
  Snapshot oder Projector (4.1).
- **Verhalten:** `Messages` zeigt das Fenster ueber `snapshot.messages`;
  Zeilentreffer setzt die kanonische Message-ID (`setSelectedMessage` wird
  produktiv); `NavigateMessageDetail` aus der Home-Warteansicht behaelt das
  bestehende „erste entscheidungspflichtige“ Verhalten, aus `Messages` erzwingt
  es die explizite Auswahl (F7). `MessageDetail` zeigt Code, Klasse,
  Quittiert/Stumm aus `MessageView` (Textschluessel je R1-erzeugbarem
  `MessageCode`; Anzahl im Umsetzungsschritt aus dem Enum gezaehlt, nicht
  geschaetzt). Ack/Mute laufen unveraendert ueber `prepareEnvelope` →
  `applyConfirmedPrepared`. Fuer `UserDecisionRequired` listet die Seite die
  drei Sensoraktionen als Zeilen (4.1); Slot 3 `continue` wendet die gewaehlte
  an, der Owner lehnt Unzulaessiges fail-closed ab.
- **Tests:** `test_local_touch_ui` (Auswahl, Detail, Ack/Mute-Payload,
  Sensoraktion waehlen → Payload), `test_press_dispatcher` (Ack/Mute →
  `OwningOutcome`; unzulaessige Sensoraktion → `DecisionOnly`),
  `test_sensor_selection` (Konsumentensicht), `test_renderer_boundary`,
  `test_ui_steady_state_allocations`.
- **Hardware:** Wegen F18 bleibt die Meldungsliste auf der Hardware leer; der
  Nachweis erfolgt nativ mit Fixtures, Hardware `NOT_APPLICABLE` (Abschnitt
  11.1, O9).
- **Abhaengigkeiten:** S1. **Ownerentscheidung:** O9.

### S3 – Header-Sprache DE/EN/ES

- **Dateien/Owner:** `fermentation_ui_renderer.cpp` (`targetAt` D4),
  `fermentation_touch_workspace.{hpp,cpp}` (Seite `HeaderLanguage`: drei
  `ContentCell`-Zeilen, aktuelle Sprache markiert; **Slot 3 bleibt
  unveraendert**, damit #170 dort `web-access` belegen kann),
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
  `test_press_dispatcher`, `test_renderer_boundary` (Hit-Zone Sprache,
  Netzwerkzone unveraendert, Randpixel 219/220/263/264, Netzwerkseite folgt
  der neuen Sprache), `test_fermentation_ui_presentation_cache` (Refill bei
  Revisionswechsel), `test_ui_steady_state_allocations`.
  `test_network_configuration` bleibt unveraendert (D5).
- **Abhaengigkeiten:** S1. **Ownerentscheidung:** keine.
- **Konfliktflaeche #170:** `HeaderLanguage` Slot 3, Textarray, Press-Struct.

### S4 – Header-Uhr (Statusseite)

- **Dateien/Owner:** Renderer `targetAt` (D4), Workspace `HeaderClock`,
  Renderer-Inhalt, Textpacks.
- **Verhalten:** Hit-Zone Uhr; Seite zeigt ausschliesslich vorhandene Werte:
  Uhrzeit als **UTC gekennzeichnet**, Vertrauensstatus (`trustedUtc` gesetzt
  oder nicht), kanonische Zeitzonen-ID aus der Presentation-Quelle. Keine
  Auswahl (O2-A), keine neue Zeitwahrheit, keine #126-Neuimplementierung.
  Die Kopfzeile selbst bleibt unveraendert (F8: sie zeigt UTC ohne
  Kennzeichnung; das ist ein Befund fuer den Owner, keine Aenderung in diesem
  Slice).
- **Tests:** `test_renderer_boundary`, `test_local_touch_ui`.
- **Abhaengigkeiten:** S3 (gemeinsame `targetAt`-Aenderung).
  **Ownerentscheidung:** O2.

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
- **Doku-Wirkung:** Neue SIM-IDs fuer den owning Pfad; die vorbestehende
  Abweichung F17 (SIM-26-04..07) wird **nicht** still umgehaengt (O8-b).
- **Tests:** neuer Application-Test (WaitingForProduct → ReachingTarget
  persistiert; falscher Zustand; stale; Persistenzfehler fail-closed;
  wiederholter Press abgewiesen), `test_press_dispatcher` (der bisherige
  `UnavailableNoOwner`-Test wird zum Owning-Test), `test_local_touch_ui`
  unveraendert, `test_process_state_machine` und
  `test_run_persistence_coordinator` Konsument.
- **Abhaengigkeiten:** keine. **Ownerentscheidung:** O8-b (Doku).
  **Parallelisierbar zu S1** (PR B ∥ PR A; disjunkte Dateien ausser
  Dispatcher/Commands-Header, dort nur additive Zweige).

### S6 – ProgramCatalog-Mutations-Owner, Programmverwaltung ohne Editor

- **Dateien/Owner:** `fermentation_ui_editing.cpp` (`applyProgramEditPreview`
  Wire-Werte, F14), `fermentation_application.{hpp,cpp}`
  (`applyProgramEdit(request, expectedProgramCatalogRevision)`),
  `fermentation_ui_commands.{hpp,cpp}` (Bridge ueber vorhandene
  `ConfigurationPreviewStatus`/`ConfigurationCommitStatus`),
  `fermentation_ui_press_dispatcher.cpp` (nur `programEdit`-Zweig),
  `main/app_main.cpp` (Evict-Haken D10).
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
  `test_local_touch_ui`, `test_ui_steady_state_allocations` inklusive Evict-/
  Refill-Zyklus, `test_configuration_service` Konsument.
- **Abhaengigkeiten:** sequenziell nach S5 (gleiche Dateien in
  Application/Dispatcher/Bridge); fachlich unabhaengig von S1.
  **Ownerentscheidung:** keine. **RAM:** D10 (HW-Logpunkte nach Gate).

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

### S9 – Manueller Betrieb und Kuehlplaene

- **Dateien/Owner:** Workspace (`ManualField`-Enum, wiederverwendete
  `ValueEdit`-Seite), Renderer, Textpacks.
- **Verhalten:** Felder fuer `ManualHolding`, `ManualTimed`,
  Stop-Kuehlplan, Completion-Kuehlplan nach `FermentationUiManualRunPlanValues`
  / `ManualTimedRunValues`. Gemaess O5-A gibt es keine erfundenen Defaults;
  `confirm` bleibt deaktiviert, bis alle Pflichtwerte gesetzt und mit
  `validateManualRunPlan` gueltig sind. Start ueber die bestehenden
  `prepareStartManualHolding/Timed`, `prepareStop`, `prepareCompletion`.
- **Tests:** `test_local_touch_ui`, `test_run_commands` Konsument,
  `test_press_dispatcher` (Prepare → Confirm → Owning),
  `test_ui_steady_state_allocations`.
- **Abhaengigkeiten:** S8. **Ownerentscheidung:** O5.

### S10 – Bildschirmtastatur, Programmeditor, Settings

- **Nur startbar nach O1, O3, O4.** Wird O3 mit B entschieden, entfallen
  Tastatur und Namensbearbeitung (Programmname/Notiz/Geraetename bleiben
  deaktiviert mit Grund); Settings zeigt dann Sprache/Zeitzone lesend.
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
  hier** festgelegt und nur, wenn O3 = A; Tastengroesse ist
  Hardware-Abnahmekriterium. Programmeditor deckt die laut
  `LOCAL_UI_PROGRAMS.md` mindestens lokal bearbeitbaren Felder ab, soweit sie
  im Programmmodell existieren (Name, Notiz, Zieltemperatur, Dauer, Vorheizen,
  Sensorvorschlag, Produktfuehler-Ausfallverhalten, maximale
  Zielerreichungszeit, Abschluss-/Kuehlverhalten); Speichern ueber
  `SaveProgram` → S6-Entry mit `expectedProgramCatalogRevision`. Settings-Seite:
  Sprache (Link), Zeitzone (lesend, O2), Geraetename (Editor, O4), Netzwerk
  (Link). Dirty-Verwerfen ueber den vorhandenen `ConfirmDiscard`-Exit.
- **Tests:** `test_fermentation_ui_editing` (Textfolgen, Grenzen
  `validateVisibleName`, UTF-8), `test_local_touch_ui`, neuer
  Application-Test (Geraetename-Commit, Gating bei aktivem Lauf, ASCII- und
  Mehrbyte-Namen), `test_softap_credentials` Konsument (SSID-Ableitung),
  `test_ui_steady_state_allocations`, HW-Resource-Logpunkt (D10).
- **Abhaengigkeiten:** S6, S8, S1. **Ownerentscheidung:** O1, O3, O4.

### S11 – Dokumentation und Abschluss

`docs/ACCEPTANCE_TESTS.md` (neue SIM-IDs und Testzuordnung ohne stilles
Umhaengen von SIM-26-04..07), `docs/LOCAL_UI.md`/`LOCAL_UI_SETTINGS_SERVICE.md`
nur dort, wo das reale Verhalten (Settings-Einstieg, Sprachseite, Zeitseite) von
der Beschreibung abweicht, `docs/ROADMAP.md`. Hardware-Smoke (Display/Touch,
Sprachwechsel mit Neustart, Programmverwaltung, Ressourcen-Logs) erst nach
Software-/Reviewgate und mit `ACTUATOR_RELEASE=NO`; Ergebnisse bis dahin
`NOT_RUN`.

## 8. Abhaengigkeits- und Parallelitaetsgraph

```text
PR A:  S1 ──► S2
       S1 ──► S3 ──► S4
PR B:  S5 ──► S6
PR C:  S1 ──► S7 ──► S8 ──► S9
PR D:  S1, S6, S8 ──► S10 ──► S11
```

- **Parallelisierbar (isolierte Dateimengen):** PR A (S1–S4) ∥ PR B (S5–S6).
  S5/S6 beruehren Application, Bridge, Dispatcher-Zweige und `ui_editing`; S1–S4
  beruehren `device_platform`, Renderer und Workspace. S2 aendert Application
  und Snapshot **nicht**.
- **Nicht parallel:** S5/S6 (gleiche Application-/Dispatcher-/Bridge-Dateien),
  S3/S4 (gleiche `targetAt`-Aenderung), alle Slices mit
  `fermentation_ui_text.cpp` (Tabellengroesse).
- Eine Implementation startet fruehestens nach Planfreigabe; keine Slice wird
  vorab umgesetzt.

## 9. Konfliktflaechen mit PR #170 (`dab2831c…`, 96 Dateien)

Revalidiert gegen `origin/agent/issue-27-web-api-auth-main-restart`. Diese
Flaechen aendern beide PRs; jede Aenderung in #172 ist additiv, ohne
Umbenennung oder Umformatierung bestehender Bloecke.

| Datei | Konflikt |
|---|---|
| `fermentation_touch_workspace.hpp/.cpp` | beide erweitern `FermentationUiPage` (`HeaderWebAccess` bei #170), `FermentationUiWorkspaceSlotAction`, `routeForPage`, `setCanonicalPageStack`, `makePageView`, `FermentationUiWorkspacePress` und die Feldkopie im `Confirm`-Zweig von `press()`. **`HeaderLanguage` Slot 3** gehoert bei #170 `web-access`; #172/S3 darf Slot 3 nicht belegen. |
| `fermentation_ui_commands.hpp/.cpp` | #170 erweitert den `operation`-Variant, `FermentationUiDetailStatus` und die Bridge; #172 ergaenzt weitere Bridge-Funktionen und Operation-Varianten (`SetDisplayLanguage`), keine neuen Detail-Varianten. |
| `main/fermentation_ui_press_dispatcher.cpp` | beide fuegen `if`-Zweige ein; #172 aendert zusaetzlich den bestehenden `transitionAction`/`programEdit`-Zweig. |
| `main/fermentation_ui_renderer.cpp` | beide erweitern die Seiteninhalts-`else if`-Kette; #172 aendert `targetAt` und das Programmlisten-Layout. |
| `fermentation_ui_text.cpp` | **sicher**: #170 aendert `65U` → `70U` in drei Arrays; #172 aendert ebenfalls die Tabellengroesse. Der zweite Merge loest die Zahl trivial auf. |
| `fermentation_application.hpp/.cpp` | #170 fuegt rund 785 Zeilen hinzu (Serializer, Auth, Web); #172 fuegt Methoden in einem eigenen, abgegrenzten Block hinzu. `ApplicationCallSerializer` aus #170 wird bei der Revalidierung fuer die neuen Methoden geprueft (Aufrufe aus Touch- und Web-Pfad). |
| `main/app_main.cpp` | beide aendern die Komposition (D10-Haken, D11-Kopie bei #172). |
| `scripts/check_architecture_boundaries.py`, `docs/ROADMAP.md` | beide koennen Regeln/Statuszeilen aendern. |

Nicht mehr betroffen (gegenueber dem ersten Entwurf): `fermentation_ui_models.*`
und `fermentation_ui_projector.cpp` (S2 benoetigt kein neues Snapshot-Feld).

Revalidierung nach Planfreigabe und vor jedem PR-Start: `git diff
origin/main...origin/agent/issue-27-web-api-auth-main-restart` ueber diese
Dateien; bei Abweichung wird der Slice angehalten und der Plan aktualisiert.
Wer rebased, entscheidet O7.

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
| S3 | anwendbar | Persistenz ueber Neustart pruefbar |
| S4 | anwendbar | reine Anzeige |
| S5 | `NOT_APPLICABLE` | `WaitingForProduct` ohne Regelkreis nicht erreichbar (F15) |
| S6 | anwendbar | reine Konfiguration; zusaetzlich Ressourcen-Logpunkte (D10) |
| S7 | nur Layout | Temperaturen zeigen ohne Sensorproduzent `--.- C` |
| S8 | Eingabe anwendbar | Start bleibt ohne Laufevidenz fail-closed |
| S9 | Eingabe anwendbar | `cool-now`/`stop-and-cool` setzen einen laufenden Lauf voraus (Evidenz fehlt) |
| S10 | anwendbar | Konfiguration; Tastengroesse als Abnahmekriterium |

## 12. Risiken

| Risiko | Gegenmassnahme |
|---|---|
| Config-Commits sind unter Netz-/RAM-Last schon einmal an OOM gescheitert (PR #174). | Evict-Haken vor Mutations-Commits (D10), HW-Logpunkte nach Gate, Slices S6/S10 einzeln messbar. |
| F14: ein bestaetigtes Programm-Preview scheitert voraussichtlich an falschen Wire-Werten. | S6 beginnt mit einem fehlschlagenden Bestaetigungs-/Reload-Test; Korrektur mit denselben kanonischen Werten wie `applyNetworkMode`. |
| `WaitingForProduct` ist im Produktbuild ohne Regelkreis kaum erreichbar (F15); Laufmeldungen haben keinen Erzeuger (F18). | Nativer Nachweis; Hardware-Nachweis je Slice gemaess 11.1; im PR ausgewiesen; O9. |
| Verwaltungsauswahl (D12) aendert den bestehenden `selectProgram`-Vertrag. | Bewusste Testanpassung in S1, im PR als Vertragsaenderung ausgewiesen. |
| Zielband-/Qualifikationswerte fuer manuelle Laeufe ohne Owner (O5). | O5-A: keine Defaults, Confirm erst bei Vollstaendigkeit. |
| Header zeigt UTC ohne Kennzeichnung (F8); Zeitzonenkatalog hat einen Eintrag. | Kennzeichnung auf der Uhrseite (S4); Lokalzeit als Folge-Scope (O2). |
| Geraetename-Aenderung beeinflusst SSID/Hostname/QR (#164 B4). | O4-A: Hinweis, kein Auto-Restart, nur ohne aktiven Lauf. |
| Tastenraster 34 px koennte fuer resistives Touch zu klein sein (D8). | Hardware-Abnahmekriterium; bei Verfehlen Plan-Revision, kein stilles Nachjustieren. |
| `ContentCell` fixiert einen Plattformvertrag frueh (D1). | Nur Indizes in der Plattform, Kapazitaeten in App-Schicht; Reviewfrage im Independent Plan Review. |
| Sprachnamen/Texte mit Sonderzeichen (z. B. `n` mit Tilde) sind im Standardfont nicht abgedeckt. | ASCII-Endonyme (`Espanol`) wie die bestehenden Packs; Glyphen-Pruefung im Textpack-Test. |
| Dokumentwiderspruch F16/F17. | Wird dem Owner vorgelegt (O3, O8-b); keine stille Aufloesung. |

## 13. Abschluss dieses Auftrags

Dieser Plan ist der vollstaendige Planstand. Nach dem Plan-Commit werden
Planpfad, exakte Plan-SHA und die offenen Ownerentscheidungen O1–O9 im
Draft-PR ausgewiesen; danach wird angehalten. Es erfolgen keine
Produktaenderungen, keine Aenderungen an PR #170 und keine Aenderungen an
`.codex/config.toml`.

`docs/ROADMAP.md` fuehrt die Zeile fuer PR #174 noch als `OPEN_DRAFT`, obwohl
der PR gemergt ist (`8a734f62836f8c57263c76ceebfc36a49f4af77c`). Dieser Plan
aendert diese Zeile nicht, um die owner-gepflegten Detailnachweise nicht
teilweise zu editieren; die Aktualisierung ist ein Owner-Hinweis an die
Roadmap-Pflege (Roadmap-Regel „nach jedem Merge“).
