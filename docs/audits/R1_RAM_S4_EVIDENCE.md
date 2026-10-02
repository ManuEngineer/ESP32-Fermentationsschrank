# R1-RAM – S4: Render-Key vor Screen-Modell, allokationsfreier UI-Steady-State

Plan: `docs/tasks/memory-platform-course-plan.md` (Plan-SHA
`2c20fe13d4d51a2bfabc00e630f1ad7c90c0e48a`), Abschnitt 5.2. Nur Host-Nachweis
und Builds; **keine Hardware-Re-Messung** vor der Independent Verification.

```text
S4_CODE_COMMIT=dc4d4afb476ed6e9dab8a23626ea2de3b5164482
S4_HOST_ALLOCATION_PROOF=PASS_UNCHANGED_STEADY_STATE_0_ALLOCATIONS
S4_RESIDUAL_ALLOCATIONS=DOCUMENTED_NOT_SILENTLY_CLAIMED
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
| `networkAccessPointInfo()` | 0 im Hosttest ohne AP; in der Firmware 2 Strings by value | nur noch im Fehlpfad bzw. auf `HeaderNetwork` (siehe 5) |
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
  zeichnen nicht mehr neu); Identitäts-Hash der AP-Daten (nur auf
  `HeaderNetwork`).
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
  schlanker Getter. Keine Port- oder ADR-Änderung, keine Änderung an
  `INetworkLifecycle`, kein weiterer Zähler außer der einen
  Workspace-Revision.

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

Neu `test_ui_steady_state_allocations` (13/13 PASS):
unveränderter Zustand nach Warm-up über 100 Loops → kein Redraw und
**0 Allokationen** (Snapshot, Cacheentscheidung, Key bilden/speichern/
vergleichen); dasselbe auf `HeaderNetwork` mit vorgegebenem AP-Fingerprint
(Presentation-Kopie evicted); recycelter Snapshot ist semantisch gleich dem
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
zusätzlich die gesamte native Suite: 1324/1324 PASS.

## 5. Nicht abgedeckt / Reste (bewusst nicht verschwiegen)

- **HeaderNetwork:** Die AP-Daten (`ssid`/`password` by value) werden dort
  weiterhin pro Loop für den Identitäts-Hash kopiert (zwei kurzlebige
  Strings; im Fehlpfad ein zweites Mal). Die Daten sind nicht nur Konstanten:
  `EspIdfNetworkLifecycle` setzt `config_.softApSsid/-Password` zur Laufzeit
  und schreibt/leert `accessPointInfo_` an mehreren Stellen, ein Proxy aus
  `(Modus, Zustand, IPv4)` wäre daher keine belegte Obermenge. Eine
  allokationsfreie Identität bräuchte eine Änderung an `INetworkLifecycle`;
  das ist nicht Teil dieses Plans und wird als gemessener Rest für S11
  vermerkt.
- **Zustandsabhängig:** Vorhandene Meldungen im Snapshot oder lange
  `TextKey`-Werte (`unavailableReason`) allokieren weiterhin im Snapshot;
  der Hosttest deckt den Zustand `ready` ohne Meldungen ab.
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
Warnungen, Profilvalidierung PASS. Quell-SHA `dc4d4afb…`, Release-App-BIN
`b59b742f2d38c322489bc346d8020091b8cc5334b6f90f78e512a1137b089e94`, ELF
`f794bc56af443c8a3123d22151d2c1f41def1320916cd71a4a09d4b523150e72`.
`app_main`-Stackframe (`.su`): 3856 B (S3: 3136 B, S2: 2864 B); der
`UiRenderGate` (Snapshot, Cache, zwei Keys) liegt auf dem Main-Stack
(24576 B konfiguriert). Der beobachtete minimale Main-Stack-HWM in `AP_ONLY`
lag bei 6864 B (S3); er ist auf Hardware neu zu messen.
