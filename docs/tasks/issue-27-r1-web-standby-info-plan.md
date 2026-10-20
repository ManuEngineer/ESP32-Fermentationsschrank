# Issue #27 – R1-Web-Standbyinformationen (Geraetename, Netzwerk-, Zeitstatus)

**2026-10-20 · PLAN-REVISION 2 · OWNERENTSCHEIDE D1–D4 ERTEILT · REVISION NICHT FREIGEGEBEN · UMSETZUNG GESPERRT.**
[Issue #27](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/issues/27)
bleibt OPEN. Branch `agent/issue-27-r1-web-standby-info-plan`, Draft-PR
[#207](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/pull/207).
Dieser Plan ist die einzige kanonische Fassung; der Issue-Text mit dem Stand vor
dem Merge von #206 ist **keine** Evidence dieses Slices.

**Stand der Freigabe.** Der Owner hat am 2026-10-20 die Entscheide D1–D4 zu
Revision 1 (Plan-SHA `161f192f68f858f3f13cc0740492f22986e02f63`, historisch)
getroffen. Diese Entscheide sind **keine Freigabe** dieser Revision: Umgesetzt
wird erst nach ausdruecklicher Freigabe der exakten Plan-SHA dieser Revision
(§7). Keine Produkt-/Testcodeaenderung, kein Firmware-Build, keine Hardware-/
Browser-/UART-/Flash-/Resettests, kein Pre-Ready/Ready/Merge aufgrund dieser Revision.

## Entschiedener Umfang (Kurzfassung)

- **Variante F2** (§5): vollstaendig beschriftete, in DE/EN/ES lokalisierte Anzeige
  von Geraetename, **getrenntem** Netzwerkmodus und tatsaechlichem Netzwerkzustand
  sowie Lokalzeit mit eindeutigem Zustand „nicht synchronisiert“. Keine Reduktion
  nach K2/K4, keine Gleichsetzung von WLAN-/AP-Verbindung mit dem Browserstatus
  „Online“.
- **Einmaliges neues Shell-Limit `11264` Bytes** (statt 10 752 B), nur fuer diesen
  Slice; **keine automatische weitere Erhoehung**.
- **Domaenenreiner API-Vertrag:** `networkState` ∈ {`connected`, `disconnected`,
  `unavailable`}; die Shell uebersetzt mit eigenen Labels, keine Kopplung an
  UI-Schluessel.
- **Anzeige in allen Modi** (Standby und weitere Geraetezustaende), unveraendertes
  Stale-/Offline-Gating, keine zusaetzliche Aktor-/Bedienfreigabe.
- **Keine obligatorische Hardware-RAM-Messung vor Ready;** die Offline-Ressourcen-
  und Stacknachweise bleiben Pflicht, die +96-B-Stoppregel gilt (§6).
  `RAM_PASS=NOT_PROVEN`.

## 1. Live-Basis

- `main` = `45e7e63856ed6c609020ec1594d9634af6514e2e` (Merge PR #206, 2026-10-19T19:55:15Z);
  Branch von dort abgezweigt. Erste Branch-Aenderung `17f7f42`: Post-Merge-Status
  von #206 in der [Roadmap](../ROADMAP.md).
- #206 liefert: Laufinformationen in `/api/v1/status`, Shell 10 410 B bei Limit
  10 752 B (D1-C von #206, einmalig), H-RAM-1 nur **begrenzter Teilnachweis**
  (`RAM_PASS=NOT_PROVEN`, S4 `NOT_RUN`, Produktimage nicht hardwarequalifiziert).
  Das 11 264-B-Limit dieses Slices ist eine **eigene**, einmalige Ownerentscheidung.
  Keine vorsorgliche Kompaktierung.
- `ACTUATOR_RELEASE=NO`, G1′ storniert, F4 PROVISIONAL (13 312/16 640 B),
  R1-U/D9/G2 offen. Der Slice beruehrt keine Safety-/Sensor-/Aktor-/Recovery-
  oder Persistenzlogik.

Fachquellen: [WEB_UI](../WEB_UI.md) (Standby; Abschnitt Startseite der Shell),
[Vorplan #206](issue-27-r1-run-dashboard-info-plan.md) (Budgetregel, Projektionsmuster),
[ADR-013](../ADR-013_REUSABLE_DEVICE_PLATFORM.md), [CI/Gates](../CI_AND_QUALITY_GATES.md),
[Workflow](../AGENT_WORKFLOW.md), [Engineering](../ENGINEERING_PRINCIPLES.md).

## 2. Ziel und Nicht-Ziele

**Ziel:** Die angemeldete Startseite zeigt rein lesend (1) den Geraetenamen aus der
kanonischen `UserConfiguration`, (2) einen kompakten Netzwerkstatus mit getrennt
ausgewiesenem gewaehltem Modus und tatsaechlichem Verbindungszustand, (3) einen
kompakten Zeitstatus (Lokalzeit des vorhandenen Owners samt kanonischer Zeitzone,
bei fehlender vertrauenswuerdiger Zeit „nicht synchronisiert“, nie eine Browser-Uhr).

**Nicht-Ziele:** keine Konfigurations-/Namens-/Zeitaenderung, keine neue Route,
kein zweiter Pollzyklus, kein vierter Request, keine Navigationsseite, keine neuen
Bedienkommandos, keine SSID/Passwoerter/SoftAP-Daten/IP in der Ausgabe, keine
OTA-/Asset-/Cache-/Gzip-Massnahme, keine Aenderung an Auth/Session/CSRF/Replay/
Revisions-/Stale-/Fail-closed-Vertraegen, keine Aenderung der lokalen Display-
Darstellung, keine Hardware-/Touch-/Flash-Aktion. Weitere R1-Webbereiche (#27) und
#28 bleiben getrennt; nichts davon gilt als erledigt oder entfallen.

## 3. Nachgewiesene Datenkette auf 45e7e63

Pfade relativ zu `lib/fermentation_app/src/`, sofern nicht anders angegeben.

| Anzeige | Kanonische Quelle | Heutiger Transport | Befund |
|---|---|---|---|
| Geraetename | `FermentationApplication::uiPresentationSource()` liest `userConfiguration.deviceName` aus dem Runtime-Lease (`fermentation_application.cpp`, Validierung `validateVisibleName`, <= 96 B, `configuration_limits.hpp`) | `/status` ohne Namen; Display zeigt ihn nicht im Snapshot | `uiPresentationSource()` kopiert zusaetzlich den **gesamten ProgramCatalog** (bis 16 Programme) – im 5-s-Pfad nicht vertretbar |
| Modus | `networkMode()` (NetworkConfigurationService), `FermentationUiSnapshot.network.currentMode`, JSON `networkMode` (`ap-only`/`home-wifi`/`selection-required`) | bereits in `/status`, von der Shell **nicht** angezeigt | Wiederverwenden |
| Verbindungszustand | `INetworkLifecycle::status()` (`NetworkLifecycleState`, 7 Werte; Application haelt `networkLifecycle_`), ESP-Adapter mutex-geschuetzt (`stateMutex_`) | **nicht** in der Application-Projektion; nur `main/app_main.cpp::toDeviceUiNetworkStatus` bildet auf `DeviceUiNetworkStatus` {Connected, Disconnected, Unavailable} ab (AccessPointOnly/HomeConnected → Connected; SetupAccessPoint/ConnectingHome/CandidateTesting → Disconnected; Stopped/Failed → Unavailable) | Mapping liegt im Composition Root, nicht von der Application aus wiederverwendbar |
| Zeit | `ITimeSource::unixTimeSeconds()` (nullopt bis RTC-Seed/SNTP vertrauenswuerdig), `PreparedTimeZone` (kanonische Id <= 64 B, `TimeZoneRule`) aus demselben Lease, `device_platform::toLocalTime()` als einziger UTC→Lokal-Owner (`nullopt` ohne UTC, bei Regel `Unavailable`, nicht darstellbar) | nirgends im Web; Display bildet `ClockViewInput` in `app_main.cpp`, formatiert „HH:MM“ im Renderer | Application hat `timeSource_`; kein neuer Zeitzustand noetig |

Folgerungen: (a) kein neues Zustands- oder Zeitmodell; (b) `FermentationUiSnapshot`
bleibt unveraendert, weil eine Minutenuhr sonst die UI-Refresh-Revision (Display-
Renders) triebe und Tracker sowie `UiRenderGate` (Main-Stack) weitere Kopien
hielten; (c) der Verbindungszustand braucht eine kleine Application-Lesung plus
ein in `device_platform` wiederverwendbares Mapping.

## 4. Datenvertrag und Architektur

**Application-Accessor (schmal, transient).** `FermentationApplication` erhaelt eine
const-Lesung (Arbeitsname `deviceInfoView()`), die unter **einem**
`ApplicationCallSerializer`-Guard liefert: Geraetename (nullopt bei nicht
gewaehrtem Lease), kanonische Zeitzonen-Id, `std::optional<LocalTime>` (ein
`unixTimeSeconds()`-Lesen, dann `toLocalTime(utc, rule)`) und den Verbindungs-
zustand. Kein Katalog, kein Persistenzzugriff, nichts wird gehalten.
`WebReadOnlyApiHandler` ruft sie nur fuer `/status` auf.

**Moduskonsistenz.** Der Verbindungszustand wird nur gemeldet, wenn
`lifecycle.status().selectedMode == networkMode()` (konfiguriert); sonst
`unavailable`. Damit wird der Zustand eines anderen Modus (Umschaltung,
SetupAccessPoint bei HOME_WIFI) nicht als Zustand des gewaehlten Modus gezeigt.
Ohne `networkLifecycle_` ebenfalls `unavailable`. Das Mapping
`toDeviceUiNetworkStatus` wird **unveraendert** von `main/app_main.cpp` in
`device_platform` verschoben (DRY, ADR-013: beide Enums liegen dort schon) und von
Display und Application gemeinsam genutzt.

**Additive `/status`-Felder, `version` bleibt 1 (D2 entschieden):**

| Feld | Typ |
|---|---|
| `deviceName` | String oder `null` (Lease nicht verfuegbar); nur `textContent` im Browser |
| `networkState` | genau `connected` \| `disconnected` \| `unavailable` (Abbildung des Mappings oben, nach Moduskonsistenzregel). Keine Kopplung an Shell-Schluessel |
| `clock` | `null` ohne vertrauenswuerdige Zeit oder darstellbare Zone, sonst `{local:"YYYY-MM-DD HH:MM", zone:"<kanonische Id>"}`; Minutenaufloesung, kein Sekundenzaehler, kein Offset |

Der bestehende Modus `networkMode` bleibt das separate Feld; Modus und Zustand
werden nie zu einem Wert vermischt. Fehlende Infos erzeugen **kein** 503 des gesamten
`/status` (reine Anzeige, keine Gating-Grundlage): `null`/`unavailable`. Bestehende
503-Gruende (Enum/Bereich, Bodygrenze 3072 B) bleiben. Kein Browserzaehler: die
Anzeige gilt zum Abrufzeitpunkt und erbt Stale-Daempfung und „Stand vor N s“ aus
`LP`; `P` (Aktionsgrundlage) bleibt unangetastet.

**Darstellung (F2, D1/D3).** Alle Zeilen ueber `textContent`, DE/EN/ES, in **allen**
`homeMode`-Werten aus `LP[0]`:

- Geraetename (ohne Label, erste Zeile; `null` → „Nicht verfuegbar“).
- „Netzwerk: <Modus> · <Zustand>“. Modus: `ap-only` → „Eigener AP“/„Own AP“/„AP propio“,
  `home-wifi` → „Heimnetz“/„Home Wi-Fi“/„Wi-Fi domestico“, sonst „Nicht verfuegbar“.
  Zustand: `connected` → „verbunden“/„connected“/„conectado“, `disconnected` →
  „getrennt“/„disconnected“/„desconectado“, sonst „Nicht verfuegbar“. Die Woerter
  `Online`/`Offline` bleiben dem Abruf-Chip vorbehalten und werden fuer das Netzwerk
  **nicht** verwendet. Der Zustand beschreibt den Netzwerktransport des Geraets, weder
  Internet noch den Abrufstatus des Browsers.
- „Uhrzeit: <local> <zone>“ oder „Uhrzeit: nicht synchronisiert“/„not synchronized“/
  „no sincronizada“.

Die Texte des Messkandidaten F2 sind die Messbasis. Eine Wortlautkorrektur (z. B. fuer
AP-Verstaendlichkeit) ist nur zulaessig, wenn das gemessene Delta in der +120-B-
Spielraumgrenze (§5) bleibt, und wird im Review-Ergebnis ausgewiesen.

**Konsistenz.** Name/Zone kommen nicht atomar mit `revisions.userConfiguration`
(zwei Guard-Eintritte pro Request); ein Konfigurationswechsel dazwischen kann
Name und Revision um einen Abruf auseinanderfallen lassen, der naechste 5-s-Abruf
konvergiert. Reine Anzeige, ausdruecklich keine atomare Aussage (wie bereits die drei
getrennten API-Requests). Netzwerk und Zeit stammen aus derselben Lesung.

## 5. Budget (D1 = F2, Limit 11 264 B)

Messbasis: Shell **10 410 B** (Kopf 1 397 B + Suffix 9 013 B). Verfahren und
Kandidatentexte (Basisabgleich 9 013 B reproduziert) dauerhaft in
[`audits/STANDBY_INFO_BUDGET_20261020/`](../audits/STANDBY_INFO_BUDGET_20261020/)
(`measure.py`, `candidates/*.txt`).

| Groesse | Wert |
|---|---:|
| Gewaehlter Kandidat F2, gemessen | 11 016 B (+606 B, Kopf unveraendert 1 397 B) |
| Neues einmaliges Limit `kMaximumWebShellBytes` | **11 264 B** |
| Reserve bei F2 insgesamt | 248 B |
| davon Normalreserve (enthaelt 19 B Sequenzaufschlag) | 128 B |
| zusaetzlicher Spielraum fuer die Bindung | **120 B** |

**Regel (HOLD):** Die Bindung an die reale API darf den Normalbody gegenueber dem
F2-Messstand um hoechstens **120 B** vergroessern; die Normalreserve von 128 B
(inklusive Sequenzgrenzfall) bleibt bindend, der Kopf bleibt <= 1 536 B auch mit
maximaler Sequenz, F1 (Auslieferung) unveraendert. Wird eine dieser Grenzen
ueberschritten, folgt **HOLD**: keine weitere Limiterhoehung, keine Kompaktierungs-
runde, keine Scope-Reduktion ohne neuen Ownerentscheid. Das 11 264-B-Limit gilt
genau fuer diesen Slice.

**Historische Messbelege (nicht mehr zur Auswahl; erhalten, nicht Umsetzungsgrundlage):**
Gegenueber dem damaligen Spielraum von 214 B ueber der Normalreserve des 10 752-B-
Limits lagen F1 (+365 B), K1 (+234), K2 (+189), K3 (+204), K4 (+159), R0 (+32) und
R1 (+90). Die reduzierten Varianten K1–K4 wurden vom Owner verworfen, weil sie
Zeilenlabels, ein Wort fuer den Zeitzustand und eine saubere Trennung vom Browser-
status „Online“ aufgegeben haetten. Alle Kandidaten sind in Node syntaktisch
lauffaehig; sie stoppen erst an den index-/zeilenzahlbasierten Assertions von
`scripts/test_web_shell_polling.mjs` (Zeilen 199/207), weil neue Zeilen die Indizes
verschieben. Das ist eine erwartete Testanpassung, **kein** PASS-Nachweis; die
Basis laeuft 48/48 PASS.

## 6. Ressourcenfolgen (ohne Hardware, D4)

| Posten | Folge | Status |
|---|---|---|
| Shell-Flash | F2: +606 B `.flash.rodata` (Suffix 9 619 B) | MEASURED (Bytes), Image-Delta bei Umsetzung |
| Code-Flash | Accessor, Mapping, Codec-Felder, Formatierung: grob +0,8…1,5 KB `.flash.text` (C2 von #206: +3,8 KB fuer deutlich mehr Logik); Partition 3 080 192 B, Image 57 % | ESTIMATED, ELF-Vergleich bei Umsetzung |
| Dauer-RAM | `.dram0.*` 0 B erwartet; **kein** Zusatz in Snapshot, Tracker, `UiRenderGate`, Main-Stack | Designziel, per Sektionsvergleich zu belegen |
| JSON-Body | Standby heute 261 B, Lauf 96-B-Name 668 B (Handlerfenster #206). Zusatz: Name <= 96 B + Schluessel, Zustand, `clock` ≈ +210 B ⇒ Standby ≈ 0,5 KB, Lauf ≈ 0,9 KB; auch mit 6-fach escaptem Grenzfallnamen < 3072 B | ESTIMATED, native Messung bei Umsetzung |
| Heap pro `/status` | +5 cJSON-Knoten (≈ 40 B + Schluessel/Wert) und kurzlebige Kopien von Name/Zone: ≈ +0,3…0,5 KB transient; Spitze Lauf zuletzt ≈ 4,2 KB geschaetzt | ESTIMATED, Allokationsfenster-Test (`operator new` + cJSON-Hooks) |
| HTTPD-Stack | By-Value-View im `handle`-Frame (2 `std::string` + `optional<LocalTime>` + Enum) geschaetzt **+70…90 B** in der tiefsten Kette `handle → encodeWebApiStatus → serializeBounded`. H-RAM-1 (Messbild M): 2 196 B frei, historischer Testfloor 2 080 B ⇒ nur **116 B** Luft | ESTIMATED |

**Harte Stoppregel (HOLD, bleibt gueltig):** Messung per GCC `-fstack-usage`
(`esp32_release`). Waechst die Kette `handle → encodeWebApiStatus → serializeBounded`
um mehr als **+96 B** (2 196 − 96 = 2 100 B > 2 080 B Floor), oder ergibt sich ein
neuer konkreter RAM-/Stackbefund, folgt **HOLD und erneute Ownerentscheidung**; nicht
still optimieren und keine Grenzwerte anpassen. Die Offline-Nachweise (Allokations-
fenster, `-fstack-usage`, ELF-Sektionsdeltas) sind Pflicht. Eine Hardware-Nachmessung
ist vor Ready **nicht obligatorisch** (D4); die HWM bleibt `NOT_PROVEN`,
`RAM_PASS=NOT_PROVEN`, hoechstens `RAM_OFFLINE_ESTIMATED`.

## 7. Ownerentscheide und verbleibende Voraussetzungen

| ID | Entscheid (Owner, 2026-10-20) |
|---|---|
| **D1 = D** | Variante F2, vollstaendig lokalisiert; einmaliges Limit 11 264 B (§5); keine automatische weitere Erhoehung |
| **D2** | domaenenreine Tokens `connected`/`disconnected`/`unavailable`; Modus und Zustand getrennt; eigene Shell-Labels |
| **D3** | Anzeige in allen Modi, unveraendertes Gating |
| **D4** | keine obligatorische Hardware-RAM-Messung vor Ready; Offline-Nachweise und +96-B-Stoppregel verpflichtend |

**Weiterhin offen (nicht erledigt):**

1. **Freigabe der exakten Plan-SHA dieser Revision** durch den Owner. D1–D4 sind
   keine Freigabe der revidierten Plan-SHA.
2. Ergebnis der Stoppregel (§6) und der 120-B-Bindungsgrenze (§5): erst bei der
   Umsetzung messbar; Ueberschreitung fuehrt zu HOLD.
3. Hardware, Mehrclient, Touch, aktiver Lauf, Langzeit, Browserqualifikation:
   `NOT_PROVEN`/`NOT_RUN`; R1-U/D9/G2 und Aktorfreigabe unveraendert offen.

## 8. Umsetzungsschnitte nach Freigabe der exakten Plan-SHA

1. **C1 – Mapping verschieben.** `toDeviceUiNetworkStatus` nach `device_platform`
   (rein, `noexcept`), `main/app_main.cpp` nutzt es; nativer Test ueber alle 7
   `NetworkLifecycleState`-Werte inkl. unbekanntem Enumwert → `Unavailable`.
   Keine Verhaltensaenderung des Displays.
2. **C2 – Accessor und Transport.** `FermentationApplication::deviceInfoView()`;
   `web_json_codec.*` additive Felder (`deviceName`, `networkState`, `clock`);
   `WebReadOnlyApiHandler` nur fuer `/status`. Native Tests siehe §9;
   Ressourcenevidence (Allokationsfenster, `-fstack-usage`, ELF-Sektionsdeltas)
   gegen die Stoppregel §6.
3. **C3 – Shell, Doku, Evidence.** Kandidat F2 als Suffix, `kMaximumWebShellBytes`
   = 11 264; Budget-/Golden-/Overflow-Assertions und Kopfgrenze nachfuehren;
   JS-Test anpassen (Indizes, neue Faelle); `shell_frozen_labels.json` um die neuen
   Schluessel ergaenzen; WEB_UI („Nicht Teil dieses Slices“, Groessenschranke,
   neue Felder), ACCEPTANCE_TESTS (SIM-IDs bei Umsetzung), ROADMAP. Ergebnis auf
   exakten Quell-/Testcommit binden; Builder-Self-Check, dann Independent Review.

## 9. Gezielte Nachweise

| Bereich | Faelle |
|---|---|
| Accessor (nativ, `VirtualTimeSource`/`MockNetworkLifecycle`) | Name gesetzt/Lease nicht verfuegbar; Zeit vertrauenswuerdig/`nullopt`; Zone mit Regel `Unavailable`; DST-Grenzen via vorhandenem `toLocalTime`; alle 7 Lifecycle-Zustaende; Moduskonsistenz (konfiguriert ≠ Lifecycle → `unavailable`); fehlende `networkLifecycle_`/`timeSource_`; SSID/Passwort/IP nie in Ausgabe |
| Codec / produktiver `GET /status` | nullable Typen, genau die drei `networkState`-Tokens, 96-B-/UTF-8-/Escape-Namen, Bodygrenze 3072 B, 503 ohne Suffix unveraendert, Auth/Same-Origin/GET-only unveraendert, bestehende Felder unveraendert |
| Display-Regression | `FermentationUiSnapshot`/Refresh-Revision unveraendert; `test_ui_steady_state_allocations`, `test_renderer_boundary` gruen (Beweis, dass die Minutenuhr keine Renders treibt) |
| Echter extrahierter JS-Test | vorhandene 48 Faelle erhalten (nach Indexanpassung); DE/EN/ES vollstaendige Sprach-/Feldbindung aller neuen Labels; alle `homeMode`-Werte (D3); Zustand `connected`/`disconnected`/`unavailable`, Modus getrennt, AP-Betrieb nie als „Online“; Zeit `null` → „nicht synchronisiert“; Name als `textContent`; fehlende/`null`-Felder; Stale/Offline gedimmt mit Abrufalter; Aktionen weiterhin nur aus `P` |
| Shell/F1 | maximale Sequenz, Kopf <= 1 536 B, Limit 11 264 B, 120-B-Bindungsgrenze, Suffix-Fehlerpfad |
| Ressourcen offline | Sektionsvergleich (`.dram0.*`, `.iram0.*`, `.flash.*`), Frames, Allokationsfenster; Ergebnis hoechstens `RAM_OFFLINE_ESTIMATED` |

Geplante Befehle (nicht jetzt):

```bash
node scripts/test_web_shell_polling.mjs
pio test -e native -f test_web_application_routes -f test_fermentation_ui_models -f test_renderer_boundary -f test_ui_steady_state_allocations
python3 scripts/build_esp_idf_profiles.py all --require-clean-source-tree
export PRE_READY_EXPECTED_HEAD="$(git rev-parse HEAD)"
bash scripts/run_pre_ready_gates.sh self-check
```

Pre-Ready nur nach Independent Review, `OPEN_BLOCKERS=0` und ausdruecklicher
Owneranweisung.

## 10. Risiken

- Bindungsgrenze: jede Mehrung > 120 B (Robustheitscode, Wortlaut) fuehrt zu HOLD.
- HTTPD-Stack: siehe §6; Hardwarewert bleibt `NOT_PROVEN`.
- `EspTimerTimeSource::unixTimeSeconds()` verschiebt die UTC-High-Water-Marke; der
  zusaetzliche Aufruf erfolgt unter dem Application-Guard, Reviewer pruefen
  Thread-Sicherheit gegen `trustMutex_`.
- Anzeige gilt zum Abrufzeitpunkt (<= 5 s alt plus Minutenaufloesung); keine Aussage
  ueber die Geraeteuhr zwischen Abrufen.
- Historische Transport-/Latenzbefunde (ein Client-Timeout in #206) bleiben offen;
  keine Ursachenaussage.
