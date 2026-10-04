# R1-RAM – S4: Render-Key vor Screen-Modell, allokationsfreier UI-Steady-State

Plan: `docs/tasks/memory-platform-course-plan.md` (Plan-SHA
`2c20fe13d4d51a2bfabc00e630f1ad7c90c0e48a`), Abschnitt 5.2. Nur Host-Nachweis
und Builds; **keine Hardware-Re-Messung** vor der Independent Verification.

```text
S4_CODE_COMMIT=68acea04d0545bb41cb4e8ef2bc4ee81664ff179
S4_FIX1=HEADERNETWORK_AP_REVISION_AND_SNAPSHOT_STRING_CAPACITY
S4_HOST_ALLOCATION_PROOF=PASS_UNCHANGED_STEADY_STATE_0_ALLOCATIONS
S4_RESIDUAL_ALLOCATIONS=EVENT_BOUND_ONLY_DOCUMENTED
S4_HARDWARE_REMEASURE=NOT_DONE_PENDING_INDEPENDENT_VERIFICATION
S5_TO_S11=NOT_STARTED
```

## 1. Ergebnis der Quellenmessung (zählender `operator new`)

Vor jeder Änderung wurde die echte Pipeline gemessen
(`FermentationApplication` im Zustand `ready`, Host):

| Quelle | Allokationen je Loop vorher | Maßnahme |
|---|---|---|
| `uiSnapshot()` | 2 (Eingabevektor `input.temperatures = {…}` und `output.temperatures.reserve`) | recycelnder `refreshUiSnapshot()` mit wiederverwendeten Puffern → 0 |
| S3-Cacheentscheidung (unverändert) | 0 | keine |
| `networkAccessPointInfo()` | in der Firmware 2 Strings by value je Loop auf `HeaderNetwork` (Hosttest mit nicht-SSO-Werten belegt >0) | nur noch für einen tatsächlichen Redraw; der Key nutzt die AP-Änderungsrevision (Abschnitt 2a) |
| `home.activeRunId` im recycelten Snapshot (Run-ID bis 48 Byte) | 1 je Loop (Reset gab die Kapazität frei, belegt per Mutationstest: 100 Allokationen in 100 Loops) | Kapazität des Run-ID-Strings wird mit den Vektorpuffern behalten |
| `ClockViewInput` (kopiert `TimeZoneId`) | 0 bei kurzer Zeitzone (SSO), sonst 1 | nur noch bei gehaltenem Kontakt bzw. im Zeichenpfad |
| `makeRepresentativeScreen()` + `makeScreenRenderKey(screen)` | viele (Befehlsvektor, Strings, `view()`/Programmliste) | Key wird vorher gebildet; bei gleichem Key kein Screen-Modell |

Zählende Tests ersetzen alle globalen `new`/`new[]`/nothrow/aligned in der
Test-TU und zählen nur zwischen Warm-up und Messschleife
(`test/test_ui_steady_state_allocations`); ein Kontrolltest belegt, dass der
Zähler die frühere By-Value-Pipeline (`uiSnapshot()`) tatsächlich als
Allokation erkennt.

## 2. Umsetzung

- **Ein Key statt zwei:** `ScreenRenderKey` wird neu definiert und ersetzt den
  bisherigen screen-basierten Key (kein Parallelvertrag). Er enthält nur
  Revisionen, Enum-/Index-Werte, Flags und Hashes (keine Strings):
  `snapshot.refreshRevision`; eine einzige monotone
  `FermentationTouchWorkspace::renderRevision()` (bei jedem öffentlichen
  Mutator erhöht: `press`, `setPage`, `selectProgram`, alle `set*` inklusive
  `setProgramEditDirty`, `movePagerUp/Down`) plus aktuelle Seite; die vom
  S3-Cache **übernommene** Katalogrevision (fehlt bei Eviction/Unavailable);
  Hash der tatsächlich verwendeten Locale; gedrücktes Ziel (Art und Slot);
  Netzwerkstatus; UTC-Minute (die Uhr zeigt HH:MM aus UTC, Sekundenwechsel
  zeichnen nicht mehr neu); die Änderungsrevision der AP-Daten
  (`INetworkLifecycle::accessPointInfoRevision()`, nur auf `HeaderNetwork`;
  im ersten S4-Stand war es ein Hash, siehe 2a).
- `ProductiveLvglRenderer::render()` entscheidet nicht mehr selbst: Es baut
  Modell und Widgets nur noch für eine echte sichtbare Änderung und liefert
  `false` bei fehlendem LVGL-Lock (der Key wird dann nicht übernommen, der
  nächste Loop zeichnet erneut).
- `UiRenderGate` (`main/fermentation_ui_press_dispatcher.hpp`): der Teil von
  `updateProductUi()` vor dem Renderer (recycelter Snapshot, die eine
  Presentation-Kopie aus S3, Key bilden/vergleichen/speichern). Firmware und
  Hosttest rufen genau diese Schritte auf; `app_main.cpp` ruft
  `beginStep()`, `renderRequired()` und `markRendered()`.
- `FermentationApplication::refreshUiSnapshot()` (zusätzlich zu `uiSnapshot()`,
  das darauf aufsetzt) und `FermentationUiProjector::projectInto()` mit
  Puffer-Recycling; ein wiederverwendeter Projektionseingang als `mutable`
  Member. `uiSnapshot()` bleibt für alle bisherigen Aufrufer und Tests
  unverändert nutzbar.
- `FermentationUiPresentationCache::adoptedProgramCatalogRevision()` als
  schlanker Getter. Im ersten S4-Stand gab es keine Port-Änderung; Fix 1
  (Abschnitt 2a) ergänzt auf Owner-Wunsch `INetworkLifecycle::
  accessPointInfoRevision()`. Weiterhin keine ADR-Änderung und kein weiterer
  Zähler außer der einen Workspace-Render-Revision und der AP-Revision.

## 2a. Fix 1 (Independent Review)

**Blocker 1 – HeaderNetwork:** Der Key nutzte im Hosttest einen vorgegebenen
Fingerprint, die Firmware kopierte SSID und Passwort je Loop. Jetzt:

- `INetworkLifecycle::accessPointInfoRevision()` (Owner-gewünschte Erweiterung
  des bestehenden Network-Lifecycle-Vertrags): monotoner, geheimnisfreier
  Zähler, der sich genau ändert, wenn sich die semantische
  `NetworkAccessPointInfo` (SSID, Passwort, IPv4) ändert oder die Daten
  gesetzt/gelöscht werden, und sonst nie. Umgesetzt in `EspIdfNetworkLifecycle`
  (alle sechs Schreibstellen laufen über `setAccessPointInfoLocked()`, unter
  `stateMutex_`; ein Neustart mit unveränderten Daten ändert die Revision
  nicht) und im `MockNetworkLifecycle`; durchgereicht über
  `NetworkConfigurationService` und `FermentationApplication::
  networkAccessPointRevision()` ohne Stringkopie.
- Der `ScreenRenderKey` enthält diese Revision (nur auf `HeaderNetwork`)
  statt eines Hashes; `UiRenderGate::renderRequired()` liest sie selbst.
  `networkAccessPointInfo()` mit SSID/Passwort wird erst beim tatsächlichen
  Redraw geholt. Keine zweite SSID-/Passwort-Wahrheit, kein Cache im Renderer,
  kein Logging.
- Nachweis (zählender `operator new`, echter Pfad mit Mock-Lifecycle und
  nicht-SSO-Werten, im Test auf >15 Byte geprüft): `HeaderNetwork`, Warm-up,
  100 unveränderte Loops → 0 Allokationen, kein Redraw; Kontrolltest, dass der
  By-Value-Abruf allokiert; SSID-, Passwort-, IPv4-Änderung, Löschen und
  Wiedersetzen ändern die Revision und erzwingen Redraw (unveränderter
  Neustart nicht); beim Redraw trägt der Screen die echten Daten inklusive
  Wi-Fi-QR-Payload.

**Blocker 2 – Snapshot-Strings:** Mit einer aktiven Run-ID von 48 Byte (über
der Small-String-Grenze) allokierte der Reset-/Kopierpfad je Loop
(Mutationstest: ohne Korrektur 100 Allokationen in 100 Loops). Korrektur:
`projectInto()` behält zusätzlich die Kapazität von `home.activeRunId`.
Die einzigen von `uiSnapshot()` befüllten stringtragenden Felder sind
`home.activeRunId`; `primaryAction`, `semanticActions` und
`service.unavailableReason` werden dort nicht befüllt (Defaults, leer),
`PresentationState`, Meldungen (`RuntimeMessage`), Temperaturen und
Empfehlungs-/Recoverywerte enthalten keine Strings. Nachweis: Active Run mit
48-Byte-Run-ID, 100 Gate-Loops → 0 Allokationen, kein Redraw; recycelter
Snapshot semantisch gleich `uiSnapshot()`; eine geänderte Run-ID erzwingt
Redraw.

**Restart-Semantik der AP-Revision.** Der `MockNetworkLifecycle` ändert die
Revision bei einem Neustart mit identischen AP-Daten nicht (der Test belegt
das). Der `EspIdfNetworkLifecycle` löscht und setzt die Daten beim Neustart
nacheinander (Clear, dann Set) und kann dadurch zwei neue Revisionen
erzeugen. Das ist ein ereignisgebundener zusätzlicher Redraw bei einem
echten Netzwerkereignis und kein Bruch des Steady-State-Vertrags: Im
unveränderten Zustand ändert sich die Revision nicht (auf der Hardware im
120-s-Idle belegt, siehe `R1_RAM_S4_HARDWARE_REMEASURE_EVIDENCE.md`).

## 3. Obermengen-Audit

Vom Workspace und Screen gelesene Snapshot-Felder (`home.mode`,
`home.processState`, `home.primaryAction`, `recovery.mode`,
`service.available`, `service.unavailableReason`, `network.currentMode`,
`messages`, `temperatures`, `revisions`) sind alle im semantischen Vergleich
des `FermentationUiRefreshRevisionTracker` enthalten
(`equalFermentationUiSemanticSnapshot`, `equalRuntimeMessage` vergleicht alle
Nachrichtenfelder). Es wurde keine ungedeckte Eingabe gefunden. Die Katalog-
und Locale-Anteile stammen aus der Presentation-Kopie und sind separat im
Key.

## 4. Tests

Neu `test_ui_steady_state_allocations` (16/16 PASS):
unveränderter Zustand nach Warm-up über 100 Loops → kein Redraw und
**0 Allokationen** (Snapshot, Cacheentscheidung, Key bilden/speichern/
vergleichen); dasselbe auf `HeaderNetwork` über den echten Pfad mit Mock-Lifecycle und
nicht-SSO-Werten (ab Fix 1; im ersten S4-Stand mit vorgegebenem
Fingerprint; Presentation-Kopie evicted); recycelter Snapshot ist semantisch gleich dem
frischen Snapshot; je ein Fall für Änderung von Anwendungszustand,
Workspace-Seite, Locale, gedrücktem Ziel, Netzwerkstatus, Uhrminute (Sekunden
nicht, Minute und Verlust der Vertrauenszeit ja), AP-Identität und
Katalogrevision (inklusive Eviction); ein Test, dass jeder öffentliche
Workspace-Mutator die Render-Revision erhöht; Kontrolltest des Zählers.
`test_renderer_boundary` (31/31) wurde von den Screen-Key-Tests auf den neuen
Key migriert (Seite, Pager, Locale, Netzwerk/Uhr, Manual-Holding-/
Program-Edit-Slots, AP-Rotation). Regression PASS: `test_local_touch_ui`,
`test_press_dispatcher`, `test_fermentation_ui_models`,
`test_fermentation_ui_commands`, `test_issue144_run_identity`,
`test_fermentation_ui_presentation_cache` (S3-Vertrag unverändert),
`test_fermentation_ui_editing`, `test_device_ui_contracts`, `test_smoke`;
zusätzlich die gesamte native Suite: 1327/1327 PASS.

## 5. Nicht abgedeckt / Reste (bewusst nicht verschwiegen)

- **HeaderNetwork:** durch Fix 1 erledigt (AP-Änderungsrevision, siehe 2a); es verbleibt nur der ereignisgebundene By-Value-Abruf bei einem Redraw.
- **Zustandsabhängig:** Eine wachsende Zahl von Meldungen vergrößert den
  Meldungsvektor einmalig (danach wiederverwendet). Text-Keys in
  `home.primaryAction`/`service.unavailableReason`/`semanticActions` werden von
  `uiSnapshot()` heute nicht befüllt und wurden nicht vorsorglich recycelt;
  würde der Snapshot sie künftig befüllen, wäre der Nachweis zu erweitern.
- **Ereignisbezogen (zulässig):** geänderter Snapshot, Katalog oder Locale,
  Seiten-/Pager-/Dialogwechsel, Press und Release, Minutenwechsel, jeweils
  mit Screen-Modell und LVGL-Update. Außerdem ein zusätzlicher Redraw beim
  ersten Loop nach der Initialzeichnung, weil der Gate-Key dort noch leer ist.
- **Nicht durch den Hosttest belegbar:** C-/ESP-IDF-/LVGL-/cJSON-Allokationen
  und der LVGL-Pool (Hardware-Laufzeitmessung), sowie die tatsächliche
  Verdrahtung in `app_main.cpp` (Hardware).

## 6. Verhaltensänderungen zur Prüfung

- Uhrzeit wird nur noch bei Minutenwechsel neu gezeichnet (Anzeige HH:MM
  unverändert).
- Das Drücken des Header-/Netzwerk-Ziels erzeugt einen zusätzlichen,
  inhaltsgleichen Redraw (der Key enthält Art und Slot des gedrückten Ziels).
- `render()` entscheidet nicht mehr selbst über „unverändert“ (das tut der
  Aufrufer); die Touch-Abfrage, `platform.update()` und `application.update()`
  sind unverändert.

## 7. Builds und Stack

`esp32_release` und `esp32_bringup` frisch gebaut (ESP-IDF v6.1), 0
Warnungen, Profilvalidierung PASS. Quell-SHA `68acea04…`, Release-App-BIN
`ab6b357cce026821146c49591db02fa53d35750fb6bf7d3683f26cc7638ceda0`, ELF
`f29a4da1de243e33113aa1758134c65a5b832fffb885ce6dc3e0884d744b41b0`.
`app_main`-Stackframe (`.su`): 3872 B (vorher S4: 3856 B, S3: 3136 B, S2:
2864 B); der `UiRenderGate` (Snapshot, Cache, zwei Keys) liegt auf dem
Main-Stack (24576 B konfiguriert). Der beobachtete minimale Main-Stack-HWM in
`AP_ONLY` lag bei 6864 B (S3) und ist auf Hardware neu zu messen. Keine
Hardware-Re-Messung vor der Independent Verification.
