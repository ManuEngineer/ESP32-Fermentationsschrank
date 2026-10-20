# Issue #27 – R1-Web-Standbyinformationen (Geraetename, Netzwerk-, Zeitstatus)

**2026-10-20 · PLAN-REVISION 1 · NICHT FREIGEGEBEN · UMSETZUNG GESPERRT (HOLD, Ownerentscheid D1 offen).**
[Issue #27](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/issues/27)
bleibt OPEN. Branch `agent/issue-27-r1-web-standby-info-plan`, Draft-PR zu Issue #27.
Dieser Plan ist die einzige kanonische Fassung; der Issue-Text mit dem Stand
vor dem Merge von #206 ist **keine** Evidence dieses Slices.

**Kernbefund.** Der beauftragte Vertrag (drei Informationen, DE/EN/ES, beschriftet,
Modus und Verbindungszustand getrennt, Zeit „unbekannt/nicht synchronisiert“) passt
**nicht** in die 214 B, die oberhalb der 128-B-Normalreserve des 10 752-B-Limits
frei sind: gemessen **+365 B** (beschriftet, wiederverwendete Woerter) bis **+606 B**
(vollstaendig lokalisiert). Nur Varianten, die Darstellungsumfang aufgeben
(§5), passen ohne zweite Limiterhoehung. Die Wahl ist eine Ownerentscheidung
(§7, D1); der Planer reduziert den Scope nicht eigenmaechtig.

## 1. Live-Basis

- `main` = `45e7e63856ed6c609020ec1594d9634af6514e2e` (Merge PR #206, 2026-10-19T19:55:15Z);
  Branch von dort abgezweigt. Erste Branch-Aenderung `17f7f42`: Post-Merge-Status
  von #206 in der [Roadmap](../ROADMAP.md) nachgefuehrt.
- #206 liefert: Laufinformationen in `/api/v1/status`, Shell 10 410 B bei Limit
  10 752 B (D1-C, einmalig), H-RAM-1 nur **begrenzter Teilnachweis**
  (`RAM_PASS=NOT_PROVEN`, S4 `NOT_RUN`, Produktimage nicht hardwarequalifiziert).
  Es gibt **keine zweite Limiterhoehung** und keine vorsorgliche Kompaktierung.
- `ACTUATOR_RELEASE=NO`, G1′ storniert, F4 PROVISIONAL (13 312/16 640 B),
  R1-U/D9/G2 offen. Dieser Plan beruehrt keine Safety-/Sensor-/Aktor-/Recovery-
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
bei fehlender vertrauenswuerdiger Zeit „unbekannt“, nie eine Browser-Uhr).

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
| Verbindungszustand | `INetworkLifecycle::status()` (`NetworkLifecycleState`, 7 Werte; Application haelt `networkLifecycle_`), ESP-Adapter mutex-geschuetzt (`stateMutex_`) | **nicht** in der Application-Projektion; nur `main/app_main.cpp::toDeviceUiNetworkStatus` bildet auf `DeviceUiNetworkStatus` {Connected, Disconnected, Unavailable} ab (AccessPointOnly/HomeConnected → Connected; SetupAccessPoint/ConnectingHome/CandidateTesting → Disconnected; Stopped/Failed → Unavailable) | Mapping liegt im Composition Root, nicht wiederverwendbar von der Application aus |
| Zeit | `ITimeSource::unixTimeSeconds()` (nullopt bis RTC-Seed/SNTP vertrauenswuerdig), `PreparedTimeZone` (kanonische Id <= 64 B, `TimeZoneRule`) aus demselben Lease, `device_platform::toLocalTime()` als einziger UTC→Lokal-Owner (`nullopt` ohne UTC, bei Regel `Unavailable`, nicht darstellbar) | nirgends im Web; Display bildet `ClockViewInput` in `app_main.cpp`, formatiert „HH:MM“ im Renderer | Application hat `timeSource_`; kein neuer Zeitzustand noetig |

Folgerungen: (a) kein neues Zustands- oder Zeitmodell; (b) `FermentationUiSnapshot`
bleibt unveraendert, weil eine Minutenuhr sonst die UI-Refresh-Revision (Display-
Renders) triebe und der Tracker sowie der `UiRenderGate` (Main-Stack) weitere
Kopien hielten; (c) der Verbindungszustand braucht eine kleine Application-Lesung
plus ein in `device_platform` wiederverwendbares Mapping.

## 4. Vorgeschlagener Datenvertrag und Architektur

**Application-Accessor (schmal, transient).** `FermentationApplication` erhaelt eine
const-Lesung (Arbeitsname `deviceInfoView()`), die unter **einem**
`ApplicationCallSerializer`-Guard liefert: Geraetename (nullopt bei nicht
gewaehrtem Lease), kanonische Zeitzonen-Id, `std::optional<LocalTime>` (ein
`unixTimeSeconds()`-Lesen, dann `toLocalTime(utc, rule)`), und den Verbindungs-
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

**Additive `/status`-Felder, `version` bleibt 1:**

| Feld | Typ |
|---|---|
| `deviceName` | String oder `null` (Lease nicht verfuegbar); nur `textContent` im Browser |
| `networkState` | Token fuer Connected/Disconnected/Unavailable (Wertebereich siehe D1: `connected`/`disconnected`/`unavailable` oder an Shell-Schluessel gekoppelt `online`/`offline`/`unavailable`) |
| `clock` | `null` ohne vertrauenswuerdige Zeit oder darstellbare Zone, sonst `{local:"YYYY-MM-DD HH:MM", zone:"<kanonische Id>"}`; Minutenaufloesung, kein Sekundenzaehler, kein Offset |

Fehlende Infos erzeugen **kein** 503 des gesamten `/status` (reine Anzeige, keine
Gating-Grundlage): `null`/`unavailable`. Bestehende 503-Gruende (Enum/Bereich,
Bodygrenze 3072 B) bleiben. Es gibt keinen Browserzaehler: die Anzeige gilt zum
Abrufzeitpunkt und erbt Stale-Daempfung und „Stand vor N s“ aus `LP`; `P` (Aktions-
grundlage) wird nicht angefasst.

**Konsistenz.** Name/Zone kommen nicht atomar mit `revisions.userConfiguration`
(zwei Guard-Eintritte pro Request); ein Konfigurationswechsel dazwischen kann
Name und Revision um einen Abruf auseinanderfallen lassen, der naechste 5-s-Abruf
konvergiert. Reine Anzeige, ausdruecklich keine atomare Aussage (wie bereits die drei
getrennten API-Requests). Netzwerk und Zeit kommen aus derselben Lesung.

## 5. Harte Budgetrechnung (MEASURED)

Messbasis: Shell **10 410 B** (Kopf 1 397 B + Suffix 9 013 B), Limit **10 752 B**,
Normalreserve 128 B (enthaelt 19 B Sequenzaufschlag) ⇒ **214 B Spielraum**.
Verfahren und Kandidatentexte (Basisabgleich 9 013 B reproduziert) dauerhaft in
[`audits/STANDBY_INFO_BUDGET_20261020/`](../audits/STANDBY_INFO_BUDGET_20261020/)
(`measure.py`, `candidates/*.txt`). Alle Kandidaten sind in Node syntaktisch
lauffaehig; sie stoppen erst an den index-/zeilenzahlbasierten Assertions von
`scripts/test_web_shell_polling.mjs` (Zeilen 199/207), weil neue Zeilen die
Indizes verschieben. Das ist eine erwartete Testanpassung, **kein** PASS-Nachweis;
die Basis laeuft 48/48 PASS.

| Kandidat | Inhalt | Delta | Gesamt | Spielraum vs. 214 | Limit mind. (mit 128 B) |
|---|---|---:|---:|---:|---:|
| **F2** | voll lokalisiert, beschriftet, eigene Woerter inkl. „nicht synchronisiert“, Heimnetz/Eigener AP | +606 | 11 016 | −392 | 11 144 |
| **F1** | beschriftet (Netzwerk/Uhrzeit), „AP“/„Wi-Fi“, Zustand ueber vorhandene `online`/`offline`/`unavailable`, Zeit unbekannt = „Nicht verfuegbar“ | +365 | 10 775 | −151 | 10 903 |
| K1 | wie F1 ohne Zeilenlabels, Zeit unbekannt = `--:--`, strukturierte `clock` | +234 | 10 644 | −20 | 10 772 |
| **K2** | K1 + `networkState`-Tokens direkt als Shell-Schluessel (`online`/`offline`/`unavailable`) | **+189** | 10 599 | **+25** | passt |
| K3 | K1 mit flacher `clock`-Zeichenkette | +204 | 10 614 | +10 | passt |
| **K4** | K2 + flache `clock`-Zeichenkette | **+159** | 10 569 | **+55** | passt |
| R0 / R1 | nur Name / Name + Zeit (ohne Netzwerk) | +32 / +90 | 10 442 / 10 500 | +182 / +124 | passt |

Anzeige nur im Standby (statt immer) kostet nach Schaetzung rund 38 B zusaetzlich
(nicht gemessen) und verfehlte K2; K4 bliebe knapp.

**Was die passenden Varianten gegenueber dem Auftrag aufgeben:**

- **K2/K4/K3 (keine Zeilenlabels):** Netzwerk- und Zeitzeile ohne Beschriftung
  („Wi-Fi · Online“, „2026-10-20 14:03 Europe/Zurich“); Zeit unbekannt nur als
  `--:--` (Konvention des lokalen Displays), nicht als Wort „nicht synchronisiert“.
- **Zustandswoerter wiederverwendet:** „Online/Offline“ fuer den Netzwerkzustand
  sind dieselben Woerter wie der Abruf-Chip `#connection`. Im Stale-Fall stuende
  „Veraltet“ oben und gedimmt „Wi-Fi · Online“ darunter – genau die Verwechslungs-
  gefahr, vor der der Auftrag warnt. Im AP-Modus bedeutet „Online“ nur
  „AP aktiv“ (Abbildung AccessPointOnly → Connected des Displays), nicht Internet.
- **K2/K4 koppeln das Wire-Format an UI-Schluessel** (`networkState:"online"`);
  das ist eine API-Entscheidung, keine Implementierungsdetail.
- **K3/K4 senden eine praesentierte Zeichenkette** statt strukturierter Felder.
- Modusnamen „AP“/„Wi-Fi“ sind unlokalisierte Eigennamen (beide Sprachen gleich);
  Unbekannt/`selection-required` → `--`.

Nur **F1/F2 mit Limiterhoehung** erfuellen den Auftragstext (F1 nicht ganz:
unbekannte Zeit als „Nicht verfuegbar“; F2 voll). Sie brauchen mindestens 10 903 B
bzw. 11 144 B, also eine **zweite** Erhoehung – nur mit ausdruecklichem Owner-
entscheid. Keine Variante wird ohne Entscheid umgesetzt.

## 6. Ressourcenfolgen (ohne Hardware)

| Posten | Folge | Status |
|---|---|---|
| Shell-Flash | Kandidatendelta nach §5 (z. B. +189 B) in `.flash.rodata` | MEASURED (Bytes), Image-Delta bei Umsetzung |
| Code-Flash | Accessor, Mapping, Codec-Felder, Formatierung: grob +0,8…1,5 KB `.flash.text` (C2 von #206: +3,8 KB fuer deutlich mehr Logik); Partition 3 080 192 B, Image 57 % | ESTIMATED, ELF-Vergleich bei Umsetzung |
| Dauer-RAM | `.dram0.*` 0 B erwartet; **kein** Zusatz in Snapshot, Tracker, `UiRenderGate`, Main-Stack | Designziel, per Sektionsvergleich zu belegen |
| JSON-Body | Standby heute 261 B, Lauf 96-B-Name 668 B (Handlerfenster #206). Zusatz: Name <= 96 B + Schluessel, Zustand, `clock` ≈ +210 B ⇒ Standby ≈ 0,5 KB, Lauf ≈ 0,9 KB; auch mit 6-fach escaptem Grenzfallnamen < 3072 B | ESTIMATED, native Messung bei Umsetzung |
| Heap pro `/status` | +5 cJSON-Knoten (≈ 40 B + Schluessel/Wert) und kurzlebige Kopien von Name/Zone: ≈ +0,3…0,5 KB transient; Spitze Lauf zuletzt ≈ 4,2 KB geschaetzt | ESTIMATED, Allokationsfenster-Test (`operator new` + cJSON-Hooks) |
| HTTPD-Stack | By-Value-View im `handle`-Frame (2 `std::string` + `optional<LocalTime>` + Enum) geschaetzt **+70…90 B** in der tiefsten Kette `handle → encodeWebApiStatus → serializeBounded`. H-RAM-1 (Messbild M): 2 196 B frei, historischer Testfloor 2 080 B ⇒ nur **116 B** Luft | ESTIMATED |

**Harte Stoppregel (HOLD):** Messung per GCC `-fstack-usage` (`esp32_release`).
Waechst die Kette `handle → encodeWebApiStatus → serializeBounded` um mehr als
**+96 B** (2 196 − 96 = 2 100 B > 2 080 B Floor), folgt HOLD und Vorlage
einer Alternative (z. B. Heap-Objekt statt Frame-Wert), keine stille Anpassung.
Die HWM bleibt `NOT_PROVEN`; `RAM_PASS` wird nie ausgesprochen, hoechstens
`RAM_OFFLINE_ESTIMATED`.

## 7. Ownerentscheidungen (offen)

| ID | Frage | Empfehlung |
|---|---|---|
| **D1** | Welche Variante? **(A)** K2 (+189 B, kein neues Limit, Aufgaben/Folgen siehe §5) **(B)** K4 (+159 B, mehr Spielraum, flache `clock`) **(C)** F1 mit **zweiter** Limiterhoehung auf mind. 10 903 B (praktisch 10 944 B) **(D)** F2 mit Erhoehung auf mind. 11 144 B **(E)** R1: nur Name + Zeit, Netzwerk als eigener Slice | **(A)**, falls der Owner die Verwechslungsgefahr „Online“ im AP-/Stale-Fall akzeptiert; sonst **(C)** |
| D2 | Wertebereich/Name von `networkState`: domaenenrein (`connected`/`disconnected`/`unavailable`, kostet +45 B, nur mit Limit/K3) oder an Shell-Schluessel gekoppelt | an D1 gebunden |
| D3 | Anzeige in allen Modi (billigste, WEB_UI listet die Felder nur fuer Standby, schliesst andere Modi nicht aus) oder nur Standby (≈ +38 B, nur K4/F1+) | in allen Modi |
| D4 | Hardware-Nachmessung (H-RAM-2: Mess-Overlay, ein Client, Standby) vor Ready oder `NOT_PROVEN` mit FOLLOW-UP? Der HTTPD-Stack hat nur ≈ 116 B Luft | kein Blocker; nur auf Ownerwunsch oder wenn die Stoppregel §6 knapp erfuellt wird |

Ohne Entscheid zu D1 bleibt der Slice im HOLD; keine Teilumsetzung.

## 8. Umsetzungsschnitte nach Freigabe der exakten Plan-SHA und D1

1. **C1 – Mapping verschieben.** `toDeviceUiNetworkStatus` nach `device_platform`
   (rein, `noexcept`), `main/app_main.cpp` nutzt es; nativer Test ueber alle 7
   `NetworkLifecycleState`-Werte inkl. unbekanntem Enumwert → `Unavailable`.
   Keine Verhaltensaenderung des Displays.
2. **C2 – Accessor und Transport.** `FermentationApplication::deviceInfoView()`;
   `web_json_codec.*` additive Felder; `WebReadOnlyApiHandler` nur fuer `/status`.
   Native Tests siehe §9; Ressourcenevidence (Allokationsfenster, `-fstack-usage`,
   ELF-Sektionsdeltas) gegen die Stoppregel §6.
3. **C3 – Shell, Doku, Evidence.** Kandidat des gewaehlten D1 als Suffix, Limit
   nur wenn D1 es entscheidet; JS-Test anpassen (Indizes, neue Faelle), WEB_UI
   („Nicht Teil dieses Slices“ und Groessenschranke aktualisieren),
   ACCEPTANCE_TESTS (SIM-IDs bei Umsetzung), ROADMAP. Ergebnis auf exakten
   Quell-/Testcommit binden; Builder-Self-Check, dann Independent Review.

## 9. Gezielte Nachweise

| Bereich | Faelle |
|---|---|
| Accessor (nativ, `VirtualTimeSource`/`MockNetworkLifecycle`) | Name gesetzt/Lease nicht verfuegbar; Zeit vertrauenswuerdig/`nullopt`; Zone mit Regel `Unavailable`; DST-Grenzen via vorhandenem `toLocalTime`; alle 7 Lifecycle-Zustaende; Moduskonsistenz (konfiguriert ≠ Lifecycle → `unavailable`); fehlende `networkLifecycle_`/`timeSource_`; SSID/Passwort/IP nie in Ausgabe |
| Codec / produktiver `GET /status` | nullable Typen, 96-B-/UTF-8-/Escape-Namen, Bodygrenze 3072 B, 503 ohne Suffix unveraendert, Auth/Same-Origin/GET-only unveraendert, bestehende Felder byteweise unveraendert |
| Display-Regression | `FermentationUiSnapshot`/Refresh-Revision unveraendert; `test_ui_steady_state_allocations`, `test_renderer_boundary` gruen (Beweis, dass die Minutenuhr keine Renders treibt) |
| Echter extrahierter JS-Test | vorhandene 48 Faelle erhalten (nach Indexanpassung); DE/EN/ES vollstaendige Feldbindung; Name als `textContent`; fehlende/`null`-Felder; alte Antworten, Stale/Offline gedimmt, Aktionen weiterhin nur aus `P` |
| Shell/F1 | maximale Sequenz, Kopf <= 1 536 B, Budget-/Golden-Assertions fuer den gewaehlten Wert, Suffix-Fehlerpfad |
| Ressourcen offline | Sektionsvergleich (`.dram0.*`, `.iram0.*`, `.flash.*`), Frames, Allokationsfenster; Ergebnis hoechstens `RAM_OFFLINE_ESTIMATED` |

Geplante Befehle (nicht jetzt):

```bash
node scripts/test_web_shell_polling.mjs
pio test -e native -f test_web_application_routes -f test_fermentation_ui_models -f test_renderer_boundary -f test_ui_steady_state_allocations
python3 scripts/build_esp_idf_profiles.py all --require-clean-source-tree
export PRE_READY_EXPECTED_HEAD="$(git rev-parse HEAD)"
bash scripts/run_pre_ready_gates.sh self-check
```

Kein Flash, kein Browser-/UART-/Resettest, kein Pre-Ready vor Freigabe; Pre-Ready
nur nach Independent Review, `OPEN_BLOCKERS=0` und ausdruecklicher Owneranweisung.

## 10. Risiken und offene Punkte

- Budget: K2 laesst 25 B Spielraum; jede Mehrung durch Robustheitscode fuehrt zu
  HOLD, keine zweite Erhoehung ohne Entscheid.
- HTTPD-Stack: siehe §6; Hardwarewert bleibt `NOT_PROVEN`.
- `EspTimerTimeSource::unixTimeSeconds()` verschiebt die UTC-High-Water-Marke; der
  zusaetzliche Aufruf erfolgt unter dem Application-Guard, Reviewer pruefen
  Thread-Sicherheit gegen `trustMutex_`.
- Anzeige gilt zum Abrufzeitpunkt (<= 5 s alt plus Minutenaufloesung); keine Aussage
  ueber die Geraeteuhr zwischen Abrufen.
- Historische Transport-/Latenzbefunde (ein Client-Timeout in #206) bleiben offen;
  keine Ursachenaussage.
