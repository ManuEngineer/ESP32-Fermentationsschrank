# Kurskorrektur: RAM-Budget, Hot-Path-Regeln und Hardware-Profile

Status: Vorschlag zur Ownerfreigabe (Plan-PR, keine Produktionscodeaenderung)
Datum: 2026-10-02
Bezug: PR #170 (Retest 2026-10-01/02), ADR-008, ADR-013, ADR-019
Neue ADR-Vorschlaege: `docs/ADR-020_RAM_BUDGET_UND_HOT_PATH.md`,
`docs/ADR-021_HARDWARE_PROFILE_PSRAM.md`

## 1. Anlass

Der freie Heap ist ueber mehrere Issues unbemerkt von 231 KB auf 12 KB
gefallen (Minimum 2,5 KB, groesster Block 5 KB). Der Abort in
`main/fermentation_ui_renderer.cpp:393` (`makeRepresentativeScreen`) ist ein
Out-of-Memory beim Bildaufbau, kein Fehler der Web-/Auth-Logik aus PR #170.

| Ausbaustand | Freier Heap | Evidenz |
|---|---|---|
| Nur App, ohne UI und WLAN (ESP-IDF 6.1) | 231 KB | `docs/audits/ISSUE_159_ESP_IDF_6_1_UPGRADE_EVIDENCE.md` |
| + LVGL-UI, ohne WLAN | 89 KB | `docs/audits/ISSUE_31_TEXT_WIFI_SMOKE_20260924_717776C_READABLE.txt` |
| + WLAN, mDNS, HTTP, Web/Auth (PR #170) | 12 KB, Min. 2,5 KB | `docs/ROADMAP.md`, Retest 2026-10-02 |

Die Hardware (ESP32-WROOM-32E, ohne PSRAM) ist nicht zu klein. Es fehlt ein
gefuehrtes RAM-Budget mit automatischem Gate, und einige Muster verbrauchen
unnoetig Speicher. Da noch kein Geraet im Feld ist, werden jetzt die Regeln
korrigiert statt Symptome geflickt.

## 2. Ziel

- Die Plattform laeuft auf dem kleinsten Hardware-Profil (WROOM-32E ohne
  PSRAM) mit Reserve.
- Dieselbe App laeuft spaeter ohne Codeaenderung auf einem ESP32-S3 mit PSRAM;
  nur das Hardware-Profil aendert Kapazitaeten und Speicherplatzierung.
- RAM-Verbrauch wird automatisch geprueft, nicht nachtraeglich dokumentiert.

Nicht-Ziel: In diesem PR wird kein Produktionscode geaendert. Jede Etappe
(Abschnitt 7) erhaelt einen eigenen kleinen PR.

## 3. Leitprinzipien

1. Referenzhardware ist das kleinste Profil.
2. Jedes Subsystem besitzt ein RAM-Budget in KB.
3. Allokieren beim Start, nicht im Betrieb: im Ruhezustand null
   Heap-Allokationen in Hauptschleife, Render-Pfad und Request-Handlern.
4. Feste Kapazitaeten aus dem Profil statt unbegrenztem Wachstum.
5. Lesen statt kopieren (Referenz, Lease, `std::string_view`).
6. Messen ist ein Gate.
7. Die Plattform entscheidet ueber die Speicherplatzierung (intern, DMA,
   PSRAM); Apps fordern nur Groesse und Zweck an.

## 4. RAM-Budget WROOM-32E (Planungswerte)

Basis: ca. 304 KB freier Heap nach dem Boot ohne Applikationskomposition.

| Subsystem | Ist heute | Budget | Wichtigste Hebel |
|---|---|---|---|
| IDF-Kern + App-Domaene | ca. 73 KB (gemessen) | 75 KB | keine grossen Kopien, Stacks messen |
| UI (LVGL-Pool, LVGL-Task, Zeichenpuffer, UI-Modell, Textpacks) | ca. 142 KB (gemessen) | 75 KB | Pool messen/verkleinern, Retained Widgets, geteilte Styles, Textpacks im Flash |
| WLAN + lwIP + mDNS + SNTP | ca. 60 KB (geschaetzt) | 45 KB | RX/TX-Puffer, AMPDU aus, mDNS optional |
| Web (httpd, Sockets, Sessions, JSON) | ca. 17 KB (geschaetzt) | 20 KB Spitze | 3 Sockets, Streaming, feste Puffer |
| Reserve im Ruhezustand | 12 KB | mind. 60 KB | |

Harte Laufzeitgrenzen: unter Last nie unter 40 KB freiem Heap; groesster
freier Block mindestens 16 KB. WLAN/Web-Aufteilung wird in Etappe E0 gemessen.

Durchsetzung:

- CI: Skript wertet `idf.py size --format json` aus; Build scheitert bei
  Ueberschreitung des statischen DRAM-Grenzwerts pro Profil.
- Host-Test: zaehlender `operator new` im nativen Test; `update()` und
  `render()` duerfen nach dem Aufwaermen nicht allokieren.
- Hardware: festes Smoke-Skript mit Messpunkten (Boot, nach UI-Init, nach
  Netzwerkstart, Ruhezustand 60 s, Worst Case: 20 Moduswechsel + 4 Sessions +
  Seitenwechsel). Log-Datei haengt am PR.

## 5. Technische Massnahmen

### 5.1 UI (Retained UI)

| Heute | Kuenftig |
|---|---|
| Alle 10 ms kopiert `uiPresentationSource()` den ganzen ProgramCatalog | Katalog nur auf Programmseiten, per Referenz/Lease |
| Alle 10 ms zweimal `makeRepresentativeScreen()` (Dispatcher + Renderer), Vektor mit `std::string` | Billiger Render-Key aus Eingaben zuerst; Modell nur bei Aenderung |
| Theme- und Build-Catalog in jedem Frame neu gebaut | Einmal als `constexpr`/`static const` |
| `lv_obj_clean()` + kompletter Neuaufbau bei jeder Aenderung | Widget-Baum pro Seite einmal; danach nur Text-/Zustandsupdates |
| Fuenf lokale Styles pro Objekt | Geteilte `static lv_style_t` pro Theme-Token |
| Textpacks als `std::vector<TextPackManifest>` mit `std::string` | Konstante Tabellen im Flash |
| Hauptschleife alle 10 ms | UI-Takt 30-50 ms |

Weitere Regeln: Screen-Modell bleibt rendererunabhaengig (ADR-019), aber mit
fester Kapazitaet (`std::array` + Zaehler). LVGL-Pool mit `lv_mem_monitor()`
messen und pro Profil festlegen (WROOM Startwert 32 KB). Ein Zeichenpuffer
320x10. Kein doppelter DMA-Puffer zwischen Adapter und LVGL. Netzwerkmodus-
Commit als asynchrones Kommando an den Netzwerk-Lifecycle; danach
Hauptstack zurueck auf 16 KB pruefen.

### 5.2 Netzwerk und Web

Startwerte fuer das WROOM-Profil (danach messen):

```text
CONFIG_ESP_WIFI_STATIC_RX_BUFFER_NUM=4
CONFIG_ESP_WIFI_DYNAMIC_RX_BUFFER_NUM=8
CONFIG_ESP_WIFI_DYNAMIC_TX_BUFFER_NUM=16
CONFIG_ESP_WIFI_AMPDU_TX_ENABLED=n
CONFIG_ESP_WIFI_AMPDU_RX_ENABLED=n
CONFIG_LWIP_MAX_SOCKETS=8
```

- httpd: `max_open_sockets = 3`, `lru_purge_enable = true`, Task-Stack gemessen.
- Antworten per `httpd_resp_send_chunk()` aus festem Puffer;
  `cJSON_PrintPreallocated()` statt dynamischer Strings.
- Statische Web-Assets gz-komprimiert blockweise aus dem Flash.
- mDNS pro Profil schaltbar; im WROOM-Profil aus, solange R1 es nicht braucht
  (Ownerentscheidung).
- AP/Heimnetz-Wechsel prueft vorher die Reserve und lehnt mit Meldung ab,
  statt abzustuerzen.

### 5.3 Diagnose

- `CONFIG_ESP_COREDUMP_ENABLE_TO_FLASH=y` mit 64-KB-Partition (aus `factory`,
  sofern das App-Image laut `idf.py size` passt).
- `heap_caps_register_failed_alloc_callback()` loggt Groesse, Caps, Task.
- Ressourcen-Log: interner und DMA-Heap getrennt, LVGL-Pool, HWM aller Tasks.
- Dev-Profil: `CONFIG_HEAP_TASK_TRACKING=y`.

## 6. Prozess

- Roadmap: eine Zeile pro Issue (Status, naechster Schritt, Blocker);
  SHAs und Einzelnachweise in PR-Beschreibung bzw. CI-Log.
- Kleine PRs: ein Slice pro PR, Richtwert unter 1500 geaenderten Zeilen ohne
  Tests.
- Automatische Gates (Budget-Skript, Null-Allokations-Test,
  Architekturgrenzen, Smoke-Skript) ersetzen manuelle `*_PASS`-Schluessel.
- Jeder Plan und jede PR-Beschreibung nennt die geschaetzte RAM-Wirkung in KB.
- Speicherregeln stehen als kurzer Block in `AGENTS.md`.

## 7. Etappen

| Etappe | Inhalt | Gate |
|---|---|---|
| E0 Messbasis | Core Dump, Failed-Alloc-Hook, erweitertes Ressourcen-Log, Budget-Skript (nur Bericht), Baseline-Messung | Ist-Spalte in Abschnitt 4 gemessen statt geschaetzt |
| E1 Regeln verankern | ADR-020/021 akzeptiert, AGENTS.md-Block, Null-Allokations-Testharness, Budget-Skript als CI-Gate | CI scheitert bei Budgetverletzung |
| E2 UI-Umbau | Retained Widgets, geteilte Styles, statische Kataloge, Textpacks im Flash, LVGL-Pool pro Profil | UI <= 75 KB; 0 Allokationen im Ruhezustand |
| E3 Netzwerk-Profil | WLAN-Kconfig, httpd-Sockets, mDNS-Entscheid, asynchroner Moduswechsel | Netz <= 45 KB; 20 Moduswechsel ohne Reset |
| E4 PR #170 abschliessen | Auf neuer Basis rebasen, Streaming-Antworten, Provisionierungspfad | 4 Sessions unter Last, Min. Heap >= 40 KB |
| E5 S3-Profil vorbereiten | Profil-Layering der sdkconfig, Platzierungsschnittstelle | `esp32s3`-Build in CI gruen (ohne Hardware) |

Umgang mit PR #170: offen; Ownerentscheidung zwischen (a) Merge der
Software-Slices mit aufgeschobenem Hardware-Gate und Ressourcenabnahme in E4,
oder (b) PR #170 haelt bis E2/E3 abgeschlossen sind.

## 8. Offene Ownerentscheidungen

- [ ] ADR-020 und ADR-021 annehmen oder anpassen
- [ ] Budgetwerte in Abschnitt 4 freigeben
- [ ] mDNS in Release 1 ja/nein
- [ ] Umgang mit PR #170: Variante (a) oder (b)
- [ ] Prozessvorschlaege in Abschnitt 6 ganz, teilweise oder nicht

## 9. Folgeaenderungen nach Freigabe

Dieser PR enthaelt bewusst nur neue Dateien. Nach Freigabe des Plans folgen
in einem kleinen Doku-PR (bzw. als Teil von E1):

- ADR-020 und ADR-021 in das Register `docs/DECISIONS.md` uebernehmen.
- Budgettabelle aus Abschnitt 4 in `docs/RESOURCE_BUDGET_AND_MAINTENANCE.md`.
- Block "Speicherregeln" in `AGENTS.md`:
  - Referenz ist das kleinste Hardware-Profil (WROOM-32E ohne PSRAM).
  - Im Ruhezustand keine Heap-Allokation in Hauptschleife, Render-Pfad und
    Request-Handlern; Allokationen nur beim Boot und bei seltenen Ereignissen.
  - Grosse Strukturen nicht pro Aufruf kopieren (Referenz, Lease,
    `std::string_view`).
  - Container mit fester Kapazitaet aus dem Profil.
  - Texte und Tabellen als Konstanten im Flash.
  - Keine grossen Objekte auf dem Stack; Stackwerte nur gemessen.
  - Speicherplatzierung nur ueber die Plattform.
  - Jeder Plan und jede PR-Beschreibung nennt die RAM-Wirkung in KB.
- Eine Zeile in `docs/ROADMAP.md`.
