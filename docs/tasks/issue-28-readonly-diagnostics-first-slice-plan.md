# Issue #28 – Plan: erster hardwareunabhaengiger, rein lesender Diagnoseschnitt (D1)

**Status: Plan-SHA `81f180490ff2dd2522686ed08413a0643e8194c8` vom Owner fuer die Umsetzung von D1 freigegeben (2026-10-25); D1 umgesetzt, Independent Review offen (§11). Keine Geraete-/Flash-Aktion.**
Basis: `main` `70f44310bebea2cd99cf99982ec08e35230f8fcf` (nach PR #209). Branch `agent/issue-28-readonly-diagnostics-plan`, Draft-PR.
Auftrag: `Auftrag_Issue28_hardwarefrei_Plan.md` (Planung, keine Implementierung). `ACTUATOR_RELEASE=NO`, keine Safety- oder Aktorfreigabe durch Diagnose.
**D-28-1 = A, D-28-2 = A – Quelle: direkte Owner-Chatanweisung an den Builder-Agenten vom 2026-10-25, Wortlaut „D-28-1 = A und D-28-2 = A“ (§8, dort Nachvollziehbarkeit und Rueckfallregel).** Das ist keine Freigabe der Plan-SHA und keine Geraete-/Flash-Freigabe.
Begriff „hardwareunabhaengig“: die **Funktion benoetigt keine Sensor-/Aktorhardware**; sie wird zusaetzlich **auf dem realen ESP32 (Display/Touch/Web) verifiziert** (§6.4a). Es fehlen Sensor-/Aktorhardware und deren Inbetriebnahmenachweise, nicht die Testmoeglichkeit der UI auf dem Board.
Erste Aenderung dieses Branches war die Post-Merge-Roadmappflege (`docs/ROADMAP.md`, Commit `28438f7`); sie ist kein Produktscope.

## 1. Ergebnis in Kurzform

**Vorgeschlagener einziger Implementierungsschnitt D1:** Die lokale **Touch-Diagnoseseite** (`Status → Diagnose`) zeigt statt des heutigen Platzhalters
„zurueckgestellt (#28)“ eine **rein lesende, begrenzte Liste bereits vorhandener Werte**: Resetursache, Zustand/SAFE_BOOT-Hinweis, aktiver Fehlercode und je
Temperaturrolle Qualitaet, Alter und Roh-/korrigierter/gefilterter Wert. Jeder fehlende Wert erscheint als `nicht verfuegbar`, nie als `0` oder `OK`.
Kein neuer Producer, keine neue Web-API, kein neues Asset, kein neuer Port, keine Persistenz, keine Aktion, kein Servicepfad.

**Web wird in einen Folgeschnitt verschoben** (JS-Asset hat nur 30 B Spielraum; die Web-API liefert heute weder Roh-/Korrigiert-/Gefiltert-Werte noch Alter).
**Ressourcendiagnose (Heap/Flash) und Firmware-/Konfigurationsrevision sind nicht Teil von D1**, weil es dafuer keinen Producer hinter einem Port gibt.

**Entscheidung laut Owner-Chatanweisung (§8, vom Reviewer gegenzuzeichnen):** D-28-1 = A (Teilstart D1 trotz formaler #19-Abhaengigkeit, K1 unveraendert offen) und D-28-2 = A (Web-Diagnose ist ein eigener, spaeter zu planender und budgetierter Folgeschnitt – **keine** Voraussetzung fuer D1 oder dessen Browser-Regressions-Smoke).
**Verifikation:** Host-Tests fuer die volle Zustandsmatrix (Pflicht) **plus** Touch- und Browser-Smoke auf dem realen Board nach Implementierung und gesondert genehmigter Testausfuehrung (§6.4a).

## 2. Quellen und gepruefter Ist-Stand

Gelesen/verifiziert am Stand `70f4431`: Issue #28 (Scope/Akzeptanzkriterien), `docs/DIAGNOSTICS_AND_MAINTENANCE.md`, `docs/SPECIFICATION_REVIEW.md` (R1-Grenzen),
`docs/tasks/issue-19-journals-retention-backup-import-plan.md` §0/§6/§7 (O-R3, K1–K7), `docs/ROADMAP.md`, sowie Code und Tests:
`lib/fermentation_app/src/fermentation_touch_workspace.{hpp,cpp}`, `fermentation_ui_models.hpp`, `fermentation_ui_text.{hpp,cpp}`, `presentation_state.hpp`,
`lib/device_platform/src/{reset_cause.hpp,sensor_quality_snapshot.hpp,sensor_quality.hpp}`, `main/fermentation_ui_renderer.cpp`, `main/app_main.cpp`,
`lib/fermentation_app/src/web_json_codec.cpp`, Tests `test_local_touch_ui`, `test_renderer_boundary`, `test_ui_steady_state_allocations`, `test_sensor_quality_pipeline`.

Befunde, die den Plan bestimmen:

1. Die Touch-Seite `Diagnostics` ist heute ein **Platzhalter**: Renderer zeigt `deferred-28` (`main/fermentation_ui_renderer.cpp`, Zweig `Diagnostics || Service`); `Diagnostics` hat bereits Pager-Slots (hoch/runter) und den `Service`-Slot, aber `pager.itemCount` ist nicht gesetzt. Test `SIM-172-S7-01` (`test_renderer_boundary::test_deferred_pages_show_the_hint_in_all_locales_and_keep_owner_reason`) fixiert den Hinweis.
2. Die Seite ist ueber `Status → Diagnose` erreichbar (`pageStack_`: Status → Diagnostics); der `diagnostics`-Slot der Statusseite ist unbedingt aktiviert und verlangt keinen Service-Zustand. **Die Erreichbarkeit der Statusseite waehrend eines aktiven Laufs ist nicht aus dem Code allein belegt** und wird in D1 durch Test 1 (§6.4) nachgewiesen; ergibt sie sich nicht, ist das ein Navigationsbefund fuer den Owner, der nicht still umgebaut wird.
3. `snapshot.temperatures` (`TemperatureView`: Rolle, `valueCelsius`, vollstaendiger `device_platform::SensorQualitySnapshot`) enthaelt bereits: Qualitaet `Valid/Stale/Failed`, Rohwert, korrigierten und gefilterten Wert, angewendeten Offset, Alter der letzten akzeptierten und der letzten gueltigen Probe, letzten Fehlergrund, Zaehler. Die Technik-Seite zeigt davon heute nur Rolle, Wert und Qualitaet.
4. `ApplicationStatusView.presentation` (`PresentationState`) traegt `faultCode`, `acknowledged`, **`resetCause`** (nur Diagnoseevidenz, nicht persistiert, nie Eingang der `ActuationInterlock`) und `applicationAllocationFailure`. **Kein UI- oder Web-Consumer rendert `resetCause`** (nur Gleichheitsvergleich in `fermentation_ui_models.cpp`).
5. Es gibt **keinen Producer hinter einem Port** fuer: Heap/Flash-/Ressourcenwerte (nur `logResources()` in `main/app_main.cpp` per UART), Firmware- und Konfigurationsrevision, Reglerausgang/P-/I-Anteil/Impulsakkumulator (Regelkomposition fehlt, #35), 1-Wire-Busfehler/ROM/CRC-Zaehler als Diagnose (Sampling-Task budget-gesperrt, #30/#190), einen **passiven Boot-Selbsttest als Modell** (die Spezifikation beschreibt ihn; im Code existiert kein `SelfTest`-Modell).
6. Die Web-API (`/api/v1/temperatures`) liefert je Rolle nur `role`, `valid`, `valueCelsius`, `quality` – **keine** Roh-/Korrigiert-/Gefiltert-Werte, kein Alter. Ein Web-Diagnosebereich braeuchte API-Erweiterung **und** Asset-Code.
7. Webbudget: JS-Asset 15 330 B bei Limit 16 384 B und Pflichtreserve ≥ 1 024 B ⇒ **30 B** Spielraum (PR #209, Plan §15.11). Shell-/HTTPD-Grenzen unveraendert.
8. Auf dem Ziel ist kein Sensorproducer komponiert (Technik-Seite zeigt `--.- C`); auf Hardware waeren die Sensorzeilen von D1 daher bis #30/#190 `nicht verfuegbar`. D1 braucht dafuer keine Sensorhardware: die volle Zustandsmatrix ist per Host-Fakes pruefbar, die Anzeige zusaetzlich per Smoke auf dem Board (Sensorzeilen dort wahrheitsgemaess `nicht verfuegbar`); das ist **kein** Nachweis fuer DS18B20-Hardware.

## 3. #19-/R1-Abgrenzung: Abhaengigkeits-/Vertragsmatrix

Issue #28 fuehrt #19 (sowie #20, #22–#25) formal als Abhaengigkeit. #19 hat Journal, Mess-/Laufhistorie, Bereinigung und Laufexport mit
`DEFERRED_BY_OWNER_PENDING_R1_CONTRACT_RECONCILIATION` zurueckgestellt; **K1 (Journal-Startbedingung) bleibt unveraendert offen und wird hier nicht als erfuellt behandelt.**
O-R3/K2–K7 sind vom Owner genehmigt (u. a. „#28 fachlich von spaeteren Journal-/Historienfunktionen entkoppeln“), die formale Aenderung der SSOT-, #28- und #37-Vertraege steht **ausdruecklich aus**.

| #28-Element | Abhaengig von | Unabhaengig realisierbar (hardwarefrei, nur lesend)? | Einordnung |
|---|---|---|---|
| Touch-Diagnose: Reset-/Fault-/Zustandsanzeige | – (nur Bestandsmodelle) | **ja** | **D1** |
| Touch-Diagnose: Sensorstatus (Qualitaet, Alter, Roh/korrigiert/gefiltert) | Bestand (`SensorQualitySnapshot`); reale Werte erst mit #30/#190 | ja (Host-Fakes) | **D1** |
| Passiver Boot-Selbsttest (Ergebnis `PASS/PASS_WITH_WARNINGS/SAFE_BOOT/FAILED`) | #24-Bootklassifikation, #31, Persistenz/Config-Gesundheit; Modell fehlt | teilweise (nur Teilmengen) | Folgeschnitt; eigener Plan, kein Teil von D1 |
| Fehler-/Persistenz-/Reset-/Brownout-/Watchdog-**Journal** | #19 Journal (zurueckgestellt, K1) | **nein** | Deferred (Owner O-R3) |
| Aktueller Laufchart, begrenzte **Historie** | #19 Mess-/Laufhistorie (zurueckgestellt) | **nein** | Deferred |
| Lauf-, Diagnose-, Servicebericht-**Export** (versioniert, ohne Geheimnisse) | #19 Laufexport, Exportformat-Vertrag, Ressourcen-/Exportgrenzen (`TBD_IMPLEMENTATION_BUDGET`) | Diagnoseexport theoretisch, aber kein Producer-/Budgetvertrag | Deferred; eigener Plan |
| Netz-/Zeit-/Ressourcenstatus (Web/Touch) | Netz-/Zeit: Bestand teilweise; Ressourcen: kein Port | Netz-/Zeitmodus ja (nur Modus), Ressourcen **nein** | Netzmodus optional D1+; Ressourcen Deferred |
| Regler-/Aktor-/Interlock-Diagnose | #35 (Regelkomposition), #32/#33 (Aktorik) | **nein** | Deferred (Hardware/Commissioning) |
| Gefuehrter PIN-**Serviceablauf** mit Mockaktoren | PIN-/Auth (#19/#188 Ablauf A), E5-Servicegate, gesperrtes Hardwarebackend | nein (Aktor-/Servicepfad, ausgeschlossen) | Deferred |
| Webdiagnose | JS-Asset-Budget (30 B), API-Erweiterung | **nein** (Budget) | Folgeschnitt, Deferral laut D-28-2 = A |
| Kein benutzeraktivierbarer UART-Diagnosemodus | – | – | Out (R1-Grenze, `FUTURE_RELEASE`) |

Aenderungen, die einen Ownerentscheid brauchen (nicht still aufgeloest): (a) **ob #28 mit einem Teilumfang beginnen darf**, obwohl #19 formal Abhaengigkeit bleibt und die Vertragsanpassung aussteht – **Option A laut Owner-Chatanweisung (D-28-1, §8)**; (b) jede **Aenderung des Issue-/SSOT-Textes** von #28 (Abhaengigkeiten, Akzeptanz „Serviceablauf … vollstaendig durchlaufen“, „aktueller Laufchart und begrenzte Historie“); (c) K1 – bleibt offen.

## 4. Wertinventar fuer D1 (Producer, Owner, Gueltigkeit, Consumer, Test)

| Diagnosewert | Producer / Owner (heute) | Datenvertrag, Gueltigkeit | Consumer heute | Testnachweis heute | In D1 |
|---|---|---|---|---|---|
| Resetursache | `device_platform::IResetCauseSource` (Adapter `EspResetCauseSource`), von `FermentationApplication` in `PresentationState.resetCause` | Enum `ResetCause` (17 Werte); `std::optional` – leer ⇒ unbekannt; nur Evidenz, nicht persistiert, nie `ActuationInterlock` | **keiner** | `MockResetCauseSource`; Application-Tests belegen das Durchreichen (zu bestaetigen) | ja (Zeile 1) |
| Aktiver Fehlercode | `PresentationState.faultCode` (`FaultCode`) aus `ActuationInterlock`/Application | `None` ⇒ kein Fehler; sonst Code | Meldungsseite ueber `RuntimeMessage` | `test_local_touch_ui`, Meldungstests | ja (Zeile 2), nur als Anzeige |
| Zustand / SAFE_BOOT | `ApplicationLifecycleState` und Prozess-/`homeMode` (`process-state-safe-boot`) im Snapshot | Enum; bereits lokalisiert | Statusseite/Home | `SIM-26-07` u. a. | ja (Zeile 3), vorhandene Textschluessel |
| Sensorqualitaet je Rolle | `TemperatureView.quality.quality` (Sensor-Pipeline, `test_sensor_quality_pipeline`) | `Valid/Stale/Failed`; Standard `Stale` ohne Probe | Technik-Seite | Pipeline- und UI-Tests | ja |
| Alter der letzten gueltigen Probe | `quality.lastValidSampleAgeMs` (optional) | leer ⇒ nie gueltig/unbekannt | – | Pipeline-Tests | ja |
| Roh-/korrigierter/gefilterter Wert | `quality.rawCelsius/correctedCelsius/filteredCelsius` (optional) | leer ⇒ `nicht verfuegbar`; Rohwert bleibt auch bei Unplausibilitaet sichtbar | – | Pipeline-Tests | ja (`--.-`/`nicht verfuegbar` bei leer) |
| Letzter Fehlergrund | `quality.lastFaultReason` | Enum `SensorFaultReason` | – | Pipeline-Tests | **nicht in D1** (zusaetzliche Texte, kein Mehrwert fuer den kleinsten Schnitt) |
| Heap/Flash/Ressourcen | nur `logResources()` (UART) | kein Port, kein Gueltigkeitsvertrag | – | – | **nein** (kein Producer) |
| Firmware-/Konfigurationsrevision | kein Port | – | – | – | **nein** (`not available`, neuer Port waere ein Architekturentscheid) |
| Netzmodus | `snapshot.network.currentMode` | Enum `NetworkMode` | Header/Netzwahl | bestehende Tests | optional (nur Modus, vorhandene Schluessel) |
| Regler-/Aktor-/Interlock-Detail | – | – | – | – | **nein** (#35/#32/#33) |

Regeln fuer jede Zeile: fehlender oder nicht hardwarequalifizierter Wert ⇒ Text `nicht verfuegbar` (vorhandener Schluessel `unavailable`) bzw. bei Sensorwerten `--.-`; ein `FAILED`/`STALE`-Wert erscheint nie wie eine aktuelle Messung (**jede** Sensorzeile traegt den Qualitaetsstatus selbst, siehe Darstellungsregel §6.1); Vermutungen werden nicht angezeigt.

## 5. Darstellungsvergleich und Budget

| Variante | Aufwand/Risiko | Budget | Urteil |
|---|---|---|---|
| **Touch (Diagnoseseite)** | Bestehende Seite, Pager und Slots vorhanden; Renderer-Zweig, ein Listenmodell, neue Textschluessel; bekannte Allokationsregeln der Content-Seiten | Flash: wenige constexpr-Texte; Allokationsvertrag siehe unten (Steady-State ohne Redraw allokationsfrei, Redraw begrenzt); kein Webbudget | **gewaehlt (D1)** |
| Web (neue Diagnoseansicht) | API-Erweiterung (`/api/v1/...`), Asset-Code, Sprachtexte DE/EN/ES, Codec-/Routen-Tests | **30 B** Asset-Spielraum, Shell 2048 B, HTTPD-/Heapgrenzen aus #209 | **verschoben** (D-28-2 = A laut Owner-Chatanweisung: Deferral, keine D1-Voraussetzung) |
| Web ueber vorhandene API | `/api/v1/temperatures` liefert nur Rolle/`valid`/Wert/Qualitaet – bereits im Dashboard | – | kein Mehrwert, keine Diagnose |

**Allokationsvertrag (praezisiert):** `main/fermentation_ui_renderer.cpp` baut bei einem noetigen Redraw `ScreenDrawCommand`s mit `std::vector`/`std::string` und `commands.reserve(kMaximumDrawCommands)`; ein vollstaendiger Redraw ist **nicht** allokationsfrei, und D1 behauptet das nicht. Die tatsaechlich vorhandene Garantie ist: bei **unveraendertem UI-Zustand** laeuft der `UiRenderGate`-Pfad **ohne Redraw** mit null Allokationen (`test_ui_steady_state_allocations`); diese Invariante muss D1 weiter erfuellen. Fuer einen ausgeloesten Redraw gilt im D1-Scope: Draw-Command-Zahl, Texte und Textrechtecke sind deterministisch und begrenzt, es entsteht keine zusaetzliche persistente oder allokierende Datenhaltung, kein ungebundenes Wachstum und kein neuer Allokationspfad ausser dem bestehenden Redraw-Aufbau. Keine Umstellung der Renderer-Architektur, kein neuer globaler Zero-Allocation-Anspruch.

Keine stille Vergroesserung von Asset, Shellgrenzen oder HTTPD-Stack; keine neue Web-API und keine zweite Datenhaltung. Eine hypothetische Webdiagnose rechtfertigt keine Vorabstraktion.

## 6. D1 – gewaehlter erster Implementierungsschnitt

**Name:** D1 – Lokale Touch-Diagnoseseite, nur lesend, Bestandswerte.

### 6.1 Umfang (Zeilenmodell, deterministisch, maximal begrenzt)
Die Seite `Diagnostics` zeigt ein Fenster ueber eine **feste, begrenzte Zeilenliste** (3 sichtbare Zeilen wie die Technik-Seite, Pager hoch/runter, vorhandene Geometrie):

1. `Resetursache` – Wert aus `PresentationState.resetCause` (lokalisierter Text je Ursache, leer ⇒ `nicht verfuegbar`).
2. `Fehler` – `kein Fehler` bzw. bestehender Meldungs-/Fehlertext zum `faultCode` (nur Anzeige; keine Quittierung hier).
3. `Zustand` – bestehender Prozess-/SAFE_BOOT-Text (`process-state-safe-boot` u. a.).
4.–9. je Temperaturrolle (Schrankluft, Produkt, Kuehlkoerper) zwei Zeilen: (a) Rolle · Qualitaet · Alter der letzten gueltigen Probe; (b) **Rolle (kurz) · Qualitaet** · Roh · korrigiert · gefiltert (je `--.-`, wenn leer).

**Darstellungsregel (Sensorwerte bleiben beim Scrollen eindeutig zugeordnet und qualifiziert):** Der Pager verschiebt um eine Zeile, daher kann eine Wertezeile (b) allein ohne ihre Zeile (a) sichtbar sein. Deshalb gilt fuer **jede** Sensorzeile, auch isoliert:
1. **Rolle:** Die Wertezeile (b) beginnt mit der **Rolle in Kurzform** (neue Textschluessel, ≤ 5 Zeichen: DE `Luft`/`Prod`/`Kuehl`, EN `Air`/`Prod`/`Cool`, ES `Aire`/`Prod`/`Frio`; Zeile (a) behaelt die bestehende Langform `temperature-*`). Die Rollenzuordnung darf nie nur aus der Position oder aus Zeile (a) folgen.
2. **Qualitaet:** direkt danach der Qualitaetstext (bestehende Schluessel `quality-valid`/`quality-stale`/`quality-failed` ueber `sensorQualityTextKey`, bereits DE/EN/ES, ≤ 8 Zeichen).
3. **Werte:** drei Felder `R`/`K`/`F` (Roh/korrigiert/gefiltert), je Feld Label + hoechstens 5 Zeichen (insgesamt ≤ 6, z. B. `R-99.9`): eine Nachkommastelle unter |99.95| (`R21.3`), ab |99.95| ohne Nachkommastelle bis |999.5| (`R100`, `R-127`), darueber (und nicht darstellbar) der Marker `+OVF`/`-OVF`; leer oder nicht endlich `R--.-`. Nie `0` als Ersatzwert und nie eine abgeschnittene Zahl.
4. **Farbe/Ton:** bei `Valid` normaler Textton; bei `Stale` und `Failed` stehen **Qualitaetstext und alle Zahlen** im bestehenden Warnton (`StatusWarning`, wie die Technik-Seite). Die Zahlen bleiben als diagnostische Rohinformation sichtbar, erscheinen aber nie im Ton eines gueltigen aktuellen Werts; die Qualitaet steht zusaetzlich als Text in derselben Zeile (nicht nur ueber Farbe).
5. **Zeile (a)** zeigt Rolle (Langform) · Qualitaet · Alter (`--` wenn unbekannt).

**Geometrie (innerhalb der bisherigen Zeilenhoehe und des 304-px-Rasters, keine Vergroesserung von Grenzen):** Zeichenbudget der Wertezeile 5 + 1 + 8 + 1 + 3 × 6 + 2 = **35 Zeichen** (Rolle, Qualitaet, drei Wertefelder, Luecken), vier getrennte Textrechtecke ohne Ueberlappung. Der Layouttest (§6.4, Test 2) prueft die laengsten Texte je Sprache gegen die tatsaechlichen Rechtecke. **Deterministischer Rueckfall, nur falls dieser Test die 304 px nicht belegt:** die Wertezeile (b) wird **geteilt** in (b1) Rolle · Qualitaet · `R` · `K` und (b2) Rolle · Qualitaet · `F`; jede Teilzeile bleibt selbstenthaltend (Rolle **und** Qualitaet). Das Zeilenmodell hat dann 12 statt 9 Zeilen (`currentIndex` 0–11); Pager, Fenstergroesse (3) und Zeilenhoehe bleiben unveraendert. Keine neue Sensor-, Diagnose-, Persistenz- oder Renderer-Abstraktion; nur zwei (bzw. drei) Zeilentypen im bestehenden Zeilenmodell, vorhandene Qualitaetsschluessel sowie drei neue Kurzrollenschluessel und drei Kurzlabels.

Die Liste hat damit maximal **neun Zeilen, drei gleichzeitig sichtbar**; der vorhandene Pager (`device_platform::VerticalPager`) verschiebt das Fenster um **eine Zeile** je Druck (`currentIndex` 0–8 in der Primaerdarstellung bzw. 0–11 im Rueckfall (§6.1), das Fenster zeigt die Zeilen `currentIndex` bis `currentIndex+2`, wie die Technik-Seite), alle Zeilen sind erreichbar; **keine** Sprung-/Seitenlogik und kein neues Paging-Verhalten. Die Zeilenanzahl ist zur Compilezeit begrenzt. Der Service-Slot (`Diagnose → Service`) bleibt unveraendert (PIN-Seite, weiterhin ohne Aktorwirkung); die Service-Seite behaelt den `deferred-28`-Hinweis.

### 6.2 Betroffene bestehende Module (keine neuen Module/Ports)
- `lib/fermentation_app/src/fermentation_touch_workspace.{hpp,cpp}`: `pager.itemCount` der Seite `Diagnostics` aus der festen Zeilenzahl, Page-Views unveraendert; **keine** neue Navigation.
- `lib/fermentation_app/src/fermentation_ui_text.{hpp,cpp}`: neue lokalisierte Schluessel fuer Resetursachen, Seitenzeilenlabels und ggf. `kein Fehler` (DE/EN/ES; bestehender Konsistenztest fuer alle Sprachen), Hilfsfunktion `resetCauseTextKey(ResetCause)` analog zu `sensorQualityTextKey`.
- `main/fermentation_ui_renderer.cpp` (bzw. dessen nativ getesteter Renderer-Kern): Zweig `Diagnostics` getrennt von `Service`; zeilenweise Darstellung mit begrenzten Draw-Commands und Textrechtecken (kein zusaetzlicher Allokationspfad, §5).
- Tests: `test_local_touch_ui`, `test_renderer_boundary`, `test_ui_steady_state_allocations`, `test_sensor_quality_pipeline` (nur Fixture-Nutzung). **Nicht** angefasst: `device_platform*`-Ports, ESP-IDF-Adapter, Application-Producer, Web-Routen/-Asset/-Shell, Persistenz, `ActuationInterlock`.

### 6.3 Akzeptanzkriterien (pruefbar)
- **A1** Die Seite `Diagnostics` zeigt statt `deferred-28` die Zeilenliste in DE/EN/ES; `Service` zeigt unveraendert den Hinweis.
- **A2** Jeder fehlende Wert (kein `resetCause`, leere optionale Sensorwerte, kein Sensor) wird als `nicht verfuegbar` bzw. `--.-` dargestellt – nie `0`, nie `OK`; jede Sensorzeile – auch eine **isoliert sichtbare Wertezeile** – traegt den Qualitaetstext selbst; `Failed`/`Stale`-Werte erscheinen im Warnton und nie wie eine aktuelle Messung (Darstellungsregel §6.1).
- **A3** Die Diagnose ist bei **laufendem Prozess** (sofern die Statusseite dort erreichbar ist, §2 Befund 2) erreichbar und lesbar (aktiver Lauf, Wartezustand, Abschluss, SAFE_BOOT); es entsteht kein Aktionspfad, keine Mutation, kein Intent (`ActuationInterlock`, Regelung, Watchdogs, Mutationsrechte unveraendert).
- **A4** Anzeige ist deterministisch, begrenzt (Zeilen, Draw-Commands, Texte, Rechtecke) und ueberlappungsfrei; bei **unveraendertem Zustand** laeuft der `UiRenderGate`-Pfad ohne Redraw mit **null Allokationen** (bestehende Invariante bleibt erfuellt); ein ausgeloester Redraw ist begrenzt, aber **nicht** als allokationsfrei zugesichert.
- **A5** Kein Geheimnis erscheint (Passwoerter, PIN, Sessions, Tokens, SoftAP-Zugangsdaten sind nicht Teil des Zeilenmodells; strukturell durch die Datenquellen ausgeschlossen, durch einen Test belegt).
- **A6** Service-/Aktoraktionen bleiben gesperrt: kein neuer Slot, kein neuer Intent; bestehende Sperren/Service-Tests bleiben gruen.

### 6.4 Gezielte Host-Regressionen (Pflicht; volle Zustandsmatrix mit Fake-Sensorwerten)
1. `test_local_touch_ui`: Navigation `Status → Diagnose` in Standby und bei aktivem Lauf; Pager ueber alle Zeilen; kein Intent/Mutation durch Druck auf Diagnosezeilen; Service-Slot und Sperrgrund unveraendert.
2. `test_renderer_boundary`: Zeilenbild je Sprache; `Diagnostics` ohne `deferred-28`, `Service` mit; Reset-Ursachen-Matrix (alle 17 Enumwerte → definierter Text, `nullopt` → `nicht verfuegbar`); Sensorzeilen fuer `Valid/Stale/Failed` und leere optionale Werte; keine `0`-Ersatzwerte; Ueberlappungsfreiheit und Einhaltung der 304-px-Rechtecke je Sprache fuer die **laengsten** Texte (laengste Kurzrolle, laengster Qualitaetstext, Wert `-127`); entscheidet ueber Primaerdarstellung oder Rueckfall (§6.1).
3. `test_ui_steady_state_allocations`: bei **unveraendertem** Zustand mit geoeffneter Diagnoseseite null Allokationen im `UiRenderGate`-Pfad **ohne Redraw** (bestehende Invariante); fuer den Redraw selbst nur Begrenzungen (Draw-Command-Zahl, Text-/Rechteckgrenzen), kein Allokationsfreiheitstest.
4. Text-/Locale-Konsistenz: neue Schluessel in allen drei Sprachen (ueber die vorhandene Textpack-Pruefung in `test_renderer_boundary`; der konkrete Testname wird in der Umsetzung benannt, bei fehlender Abdeckung wird ein gezielter Test ergaenzt).
5. Strukturtest: Zeilenmodell enthaelt keine Authentifizierungs-/Sitzungs-/Netzzugangsdaten (Quellenliste gegen verbotene Felder).
6. Pager: neun Zeilen, Fenster verschiebt um eine Zeile (`currentIndex` 0–8), erste/letzte Fensterposition, `canMoveUp/Down` an den Raendern, alle Zeilen erreichbar.
7. **Isolierte Wertezeile:** Pager auf jede Position, an der die erste sichtbare Zeile eine Wertezeile (b) ist (Primaerdarstellung `currentIndex` 4, 6, 8; bei Rueckfall alle (b1)/(b2)-Positionen), mit Fake-Sensoren `Valid`/`Stale`/`Failed` in **allen drei Rollen**, vollen und leeren optionalen Werten (inkl. `-127`/`85.0`), in DE/EN/ES: die **korrekte Kurzrolle** und der **korrekte Qualitaetstext** desselben Sensors stehen in derselben Zeile (Gegenprobe: vertauschte Rollen werden erkannt), `Stale`/`Failed` nutzen den Warnton fuer Text und Zahlen, `Valid` nicht, leere Werte `--.-`, nie `0`; die Zeile ist ohne die darueberliegende Zeile (a) eindeutig, die 304-px-Rechtecke werden eingehalten.
Aktualisiert wird `SIM-172-S7-01` (Platzhaltertest fuer `Diagnostics`); neue `SIM-28-D1-*`-Eintraege in `docs/ACCEPTANCE_TESTS.md`. Host-Tests ersetzen den Board-Smoke nicht und umgekehrt (§6.4a).

### 6.4a Smoke-Tests auf dem realen ESP32 (nach Implementierung; Ausfuehrung gesondert genehmigt)
Das ESP32-Board mit Display/Touch und Webzugang steht fuer Software-/UI-Tests zur Verfuegung (manuelle Touch- und Browsertests, Zeit vorhanden).
1. **Touch-Smoke:** `Status → Diagnose` erreichen; Resetursache-, Fehler- und Zustandszeile, alle neun Zeilen ueber den Pager, DE/EN/ES, fehlende Sensoren wahrheitsgemaess `nicht verfuegbar`; Rueckweg und der bestehende `Diagnose → Service`-Zugang unveraendert. **Keine** neue Aktion, Aktorfreigabe oder Resetfunktion von der Diagnoseseite aus. Zustaende, die ohne Sensoren/aktive Aktoren auf dem Board nicht sicher erreichbar sind (z. B. aktiver Lauf, `Stale`/`Failed` mit Werten), bleiben **Host-Nachweise** und werden **nicht** auf dem Board erzwungen.
2. **Browser-Smoke (Regression, keine neue Funktion):** bestehende R1-Weboberflaeche im erreichbaren Standby-/Provisioning-Zustand (Anmeldung, Navigation, Status-/Temperaturanzeige, bekannte Fehlzustaende nur soweit gefahrlos ausloesbar). **Keine** Web-Diagnosefunktion, **kein** vollstaendiges Wiederholen von D9, **keine** Lauf-/Aktorkommandos, keine destruktiven Tests allein wegen D1.
3. **Nachweisgrenzen:** Host, reales Touchdisplay, realer Browser und simulierte Faelle werden getrennt protokolliert. Board-Tests sind **kein** Nachweis fuer DS18B20-Hardware (#190), Aktorik, Run-/Recovery-Qualifikation, vollstaendige RAM-/Stack-Reserve oder `D9=PASS`.
4. **Ausfuehrungsgates:** Diese Plananpassung ist keine Flash-/Erase-/UART-/Resetfreigabe. Vor jeder konkreten Geraeteaktion gelten die bestehenden Owner-/Hardwaregates (benanntes Firmware-/Messimage, Ziel-HEAD, bestehender Backup-/Restore-Workflow, `docs/AGENT_WORKFLOW.md`, `docs/CI_AND_QUALITY_GATES.md`); keine automatische Erase-/NVS-Veraenderung; `ACTUATOR_RELEASE=NO`.

### 6.5 Ressourcen- und Coverage-Risiko (ehrlich)
- **Flash:** wenige zusaetzliche constexpr-Textschluessel (Groessenordnung: ~17 Resetursachen kollabiert auf eine kleine Zahl von Schluesseln + ~8 Labels, je 3 Sprachen); `TBD_IMPLEMENTATION_BUDGET` – Wert wird in der Umsetzung gemessen (`.flash.rodata`-Delta beider Profile), nicht geschaetzt.
- **RAM/Stack:** `UiRenderGate`-Steady-State ohne Redraw bleibt allokationsfrei (bestehende Invariante, getestet); ein Redraw baut wie bisher begrenzte Draw-Commands auf (nicht als allokationsfrei zugesichert, nicht neu gemessen). Stackpfad der Touch-Aufgaben nicht neu gemessen (`NOT_PROVEN`). Web-/HTTPD-Gates unberuehrt.
- **Coverage:** Host-Matrix (Pflicht) plus Touch- und Browser-Smoke auf dem Board nach gesonderter Genehmigung (§6.4a). Auf dem Geraet sind Sensorzeilen bis #30/#190 `nicht verfuegbar`; die `resetCause`-Zeile haengt am ESP-Adapter (bestehend, nicht neu qualifiziert). Kein Nachweis fuer Sensor-/Aktorhardware, **keine** `D9`-/`C5`-/RAM-Aussage; ohne das Board-Smoke bleibt die reale Darstellung `NOT_RUN`. Passiver Boot-Selbsttest, Journal, Exporte, Charts, Service und Web bleiben sichtbar offen.

## 7. In-/Out-/Deferred-Matrix zu Issue #28

| Issue-#28-Scope / Akzeptanz | D1 | Status |
|---|---|---|
| kompakte Touchdiagnose | Teil (Reset, Fehler, Zustand, Sensorstatus) | **In (D1)** |
| vollstaendige Webdiagnose | – | **Deferred** (D-28-2, API + Asset + Budget) |
| Roh-/korrigierte/gefilterte Sensorwerte | Touch-Anzeige der vorhandenen optionalen Werte | **In (D1)**; Web Deferred |
| Regler-, Aktor-, Fehler-, Speicher-, Ressourcenstatus | nur Fehlercode | Regler/Aktor: **Deferred** (#35/#32/#33); Speicher/Ressourcen: **Deferred** (kein Port) |
| passiver Boot-Selbsttest | nur Resetursache/SAFE_BOOT-Anzeige, kein Selbsttestmodell | **Deferred** (eigener Plan) |
| gefuehrter PIN-Serviceablauf, gesperrtes Hardwarebackend | – | **Out** fuer D1 / **Deferred** (Aktor-/Servicepfad) |
| Lauf-, Diagnose-, Servicebericht-Exporte | – | **Deferred** (#19-Laufexport, Exportvertrag) |
| aktueller Laufchart, begrenzte Historie | – | **Deferred** (#19 Historie) |
| kein benutzeraktivierbarer UART-Diagnosemodus | – | **Out** (R1) |
| AK: Diagnose bleibt waehrend eines Laufes lesbar | A3 | **In (D1)** fuer Touch |
| AK: veraendernde Servicepruefungen im Lauf gesperrt | unveraenderte Sperren (A6) | bestehender Stand, **kein neuer Nachweis ueber D1 hinaus** |
| AK: Exporte versioniert, ohne Geheimnisse | – | **Deferred** |
| AK: Serviceablauf mit Mockaktoren vollstaendig durchlaufbar | – | **Deferred** (offen, nicht erfuellt) |
| AK: fehlende Messwerte nicht als Nullwerte | A2 | **In (D1)** fuer die gezeigten Werte |
| Tests: Bootstatus, Sensorzustand, verweigerte Freigabe, Export ohne Geheimnisse, abgebrochener Serviceablauf, Ressourcenwarnung | Sensorzustand; Reset-/Zustandsanzeige | uebrige **Deferred** |

**Issue #28 bleibt nach D1 offen.** D1 darf nicht als Abschluss von #28 oder als Erfuellung der Definition of Done gekennzeichnet werden.

## 8. Ownerentscheide

**Quelle und Nachvollziehbarkeit:** Beide Entscheide stammen aus einer **direkten Chatnachricht des Owners an den Builder-Agenten (Claude-Code-Session, 2026-10-25)** mit dem Wortlaut „D-28-1 = A und D-28-2 = A“; sie wurde zweimal gesendet, zusammen mit den Auftraegen „Planreview-Korrekturdelta“ (v1 und v2) fuer PR #210. Der Wortlaut steht im Sitzungstranskript der Builder-Session. Er ist **nicht** aus der Reviewer-Empfehlung abgeleitet; die Reviewer-Empfehlung war lediglich dieselbe Option. Die Owner-Aussage, dass das ESP32-Board fuer UI-Smokes verfuegbar ist, ist davon getrennt und ersetzt keine der beiden Entscheidungen.
**Rueckfallregel:** Kann der Reviewer/Owner diese Chatanweisung nicht als die massgebliche Ownerentscheidung bestaetigen, gelten D-28-1 und D-28-2 als **„Option A empfohlen, Ownerbestaetigung ausstehend“**; dann beginnt keine Umsetzung, und der Plan bleibt unveraendert gueltig als Entwurf.

| ID | Entscheidung | Stand |
|---|---|---|
| **D-28-1** | Darf #28 mit dem rein lesenden Teilschnitt D1 beginnen, obwohl #19 formale Abhaengigkeit bleibt und die formale Vertragsanpassung (O-R3: SSOT/#28/#37) aussteht? | **Option A laut Owner-Chatanweisung** (Quelle oben): D1 darf nach Planfreigabe starten; die formale #28-/SSOT-Vertragsanpassung bleibt getrennt und spaeter; **K1 bleibt unveraendert offen**; D1 beruehrt Journal/Historie/Export nicht. |
| **D-28-2** | Web-Diagnosefunktionsumfang und Webbudget | **Option A laut Owner-Chatanweisung** (Quelle oben): Web-Diagnose ist **kein Teil von D1** und ein **Deferral fuer einen spaeteren, eigenen, budgetierten Plan**; **keine** Umsetzungsvoraussetzung fuer D1. Die Verfuegbarkeit eines Browsers fuer Tests (Browser-Regressions-Smoke, §6.4a) beseitigt das Asset-Budget nicht: JS-Asset 15 330 B bei 16 384 B Limit und 1 024 B Mindestreserve = nur 30 B Spielraum. |

Beide Entscheide sind keine Freigabe der Plan-SHA, keine Implementierungsfreigabe und keine Geraete-/Flash-/Erase-/UART-/Resetfreigabe; die getrennten Geraetegates (§6.4a) bleiben unveraendert. Offen bleibt die **Planreview und Ownerfreigabe der exakten Plan-SHA**.
Kein weiterer Ownerentscheid ist fuer D1 noetig: keine neue Persistenz, keine Aktor-/Safety-/Auth-Aenderung, kein neuer Port.

## 9. Ausgeschlossen und Risiken

Ausgeschlossen fuer D1: aktive Service-/Aktorpruefungen, PIN-/Auth-Neuentwicklung, Sensor-Commissioning, Sensor-/Aktor-Hardwareevidence (das Board-Smoke §6.4a ist davon getrennt), neue Journale/Historie/Laufexporte, Backup/Import, thermische Kennwerte, Ressourcenqualifikation, neue Web-API/Asset, neue Ports/Frameworks/Diagnose-Orchestratoren/Telemetrie-Busse, neue Persistenz, Aenderungen an `ActuationInterlock`/Regelung/Watchdogs.

Risiken: (1) Resetursachen-Text wird als „Sicherheitsaussage“ missverstanden – Mitigation: Textlabel nennt nur Ursache, Hinweis in Doku/Test, kein Einfluss auf Freigaben. (2) Zeilenmodell wird spaeter zum Parallelmodell – Mitigation: Zeilen nur aus Snapshot-Feldern, keine eigene Datenhaltung. (3) Sensorzeilen wirken auf Hardware leer – dokumentiert (`nicht verfuegbar`), kein Defekt. (4) Textumfang/Renderer-Layout (3 Zeilen, 304 px) begrenzt Lesbarkeit – im Test auf Ueberlappungsfreiheit festgenagelt; ggf. kuerzere Labels, kein Layoutumbau.

## 10. Governance, Reihenfolge, naechster Schritt

- Dieser Plan wird als Plan-Commit auf dem Branch zur **Planreview und Ownerfreigabe der exakten Plan-SHA** vorgelegt; erst danach Implementierung (`docs/AGENT_WORKFLOW.md`).
- Board-Smoke (§6.4a) erst nach Implementierung und gesonderter Ausfuehrungsfreigabe fuer das benannte Image; Host-Tests zuerst.
- Reihenfolge nach Freigabe: (1) Textschluessel + Mapping `resetCause` → Text mit Test, (2) Zeilenmodell/Pager im Workspace, (3) Renderer-Zweig, (4) `SIM-28-D1-*`, `ACCEPTANCE_TESTS.md`/`ROADMAP.md`. Pre-Ready/Ready/Merge nur nach separater Ownerfreigabe.
- Roadmap-Statuspflege nach PR #209 ist bereits als erster Commit des Branches erledigt und kein Produktscope.
- `ACTUATOR_RELEASE=NO`; keine Hardware-/Flash-/Aktorfreigabe aus diesem Plan.

## 11. Umsetzungsstand D1 (nach Planfreigabe `81f1804`)

Umgesetzt in `lib/fermentation_app` (Textschluessel `fermentation_ui_text.{hpp,cpp}`: 22 Schluessel je Sprache, `resetCauseTextKey`, `diagnosticRoleShortTextKey`; `fermentation_touch_workspace.{hpp,cpp}`: `kFermentationUiDiagnosticsRowCount = 9`, Pager-Zaehler der Seite `Diagnostics`) und `main/fermentation_ui_renderer.cpp` (Zweig `Diagnostics`, getrennt von `Service`). Primaerdarstellung der Wertezeile (Kurzrolle · Qualitaet · `R`/`K`/`F`); der Rueckfall (Wertezeile teilen) wurde **nicht** benoetigt. Tests: `SIM-28-D1-01..06` (`docs/ACCEPTANCE_TESTS.md`), native Gesamtsuite 1857/1857 PASS, beide ESP-IDF-Profile PASS, `bash scripts/run_pre_ready_gates.sh self-check` PASS (`CLANG_TIDY=PASS`, `BUILDER_STATIC_ANALYSIS_SELF_CHECK=PASS`), Architekturgrenzen PASS. Gegenproben: absichtlich verfaelschter Warnton und vertauschte Kurzrolle werden von den Tests erkannt.
Ressourcen (Release, vor/nach, gemessene Linkerwerte): `.flash.rodata` 244 124 → 246 220 B (+2 096 B), `.flash.text` 1 408 866 → 1 410 954 B (+2 088 B), `.dram0.*`/`.iram0.text` unveraendert; Image 1 774 384 → 1 778 576 B (+4 192 B). Bringup entsprechend (+2 096 / +2 192 B). Kein RAM-/Stackmesswert, keine Web-/HTTPD-Aenderung.
**Implementation-Review-Delta (HEAD `8a73968`):** (1) Die Zustandszeile zeigt den etablierten `ProcessState` ueber `processStateTextKey()` – `SafeBoot` immer als solcher, ohne etablierten Zustand (home `Unavailable`) nur `nicht bereit`/`nicht verfuegbar`, der Default `Boot` wird nicht als Zustand angezeigt (`SIM-28-D1-07`). (2) Das Zahlenformat der Wertefelder schneidet nichts mehr ab und haelt das Rechteck: eine Nachkommastelle unter |99.95|, darueber ganzzahlig bis |999.5|, sonst `+OVF`/`-OVF` (`SIM-28-D1-08`); die Wertefelder umfassen damit Label + hoechstens 5 Zeichen (≤ 6 insgesamt, z. B. `R-99.9`; §6.1 angepasst, Zeichenbudget 35).
Nicht ausgefuehrt/`NOT_RUN`: Touch-/Browser-Smoke auf dem realen ESP32 (§6.4a, `HW-28-D1-01`, braucht gesonderte Ausfuehrungsfreigabe fuer ein benanntes Image), `D9`, Sensor-/Aktorhardware. Die 304-px-Grenzen sind hostseitig gegen ein konservatives 7-px-Zeichenbudget geprueft; die reale Schrift ist erst im Board-Smoke belegbar. Issue #28 bleibt offen.
