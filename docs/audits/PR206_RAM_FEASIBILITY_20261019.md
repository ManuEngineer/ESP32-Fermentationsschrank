# PR #206 / Issue #27 – RAM-Tauglichkeit des Laufdaten-Slice C1–C3, 2026-10-19

**C1=FAIL_BUDGET / HOLD unveraendert; C2/C3=NOT_STARTED. `RAM_PASS=NOT_CLAIMED`.**
*Stand-Hinweis: Dieses Dokument ist die historische Bilanz vor Umsetzung und Hardwaremessung. Aktueller Stand: C1–C3 umgesetzt, Review `GO`, H-RAM-1 ausgefuehrt ([Evidence](PR206_H_RAM_1_EVIDENCE_20261019.md)). Die Aussagen unten bleiben als damalige Aussagen stehen.*
`DECISION=GO_FOR_OWNER_GATE` heisst ausschliesslich: Die Offline-Bilanz traegt es,
dem Owner **zwei Entscheidungen** vorzulegen (einmaliges Shell-Limit D1-C,
Hardware-Gate H-RAM-1). Es ist weder RAM-Qualifikation noch Implementierungs-
freigabe. Letzte freigegebene Plan-SHA bleibt
[`f013ec0`](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/blob/f013ec0c8428a204b5c53b70d7fd6fcbb088c75d/docs/tasks/issue-27-r1-run-dashboard-info-plan.md);
D2=A bleibt. C1-Evidence unveraendert: [PR206_C1_BUDGET_20261019.md](PR206_C1_BUDGET_20261019.md).
`ACTUATOR_RELEASE=NO`; G1' `NOT_RUN/NOT_PROVEN`; F4 `PROVISIONAL`; historische
`FAIL/HOLD` und die offene R1-U/D9-Abnahme unveraendert. Kein Geraete-, UART-,
Browser-, Flash- oder Resetzugriff in diesem Auftrag.

Statusbegriffe je Posten: **MEASURED** (hier am ELF/Compiler gemessen),
**ESTIMATED** (aus Code/Konfiguration abgeleitet, mit Bandbreite, nicht
gemessen), **NOT_PROVEN** (kein Nachweis vorhanden).

## 1. Kernbefund

1. **Das 8192-B-Limit ist seit F1 kein RAM-Limit mehr.** Der Shell-Suffix liegt als
   `const`-Literal in `.flash.rodata` (DROM). Fuer den **isolierten C1-Suffixkandidaten**
   (nur das Literal waechst auf 9013 B) bleiben `.dram0.data`, `.dram0.bss`,
   `.iram0.text` und `.flash.text` um **0 B** veraendert (MEASURED, Abschnitt 2).
   Das gilt **nicht** fuer die noch nicht implementierten C2/C3 (Abschnitt 4). RAM-relevant bleibt der besessene Kopf
   (1397 B, reserviert 1536 B), und der ist unveraendert.
2. **Sendepfad (ESTIMATED, Abschnitt 2.3):** `TCP_SND_BUF=5760` ist die
   Sendepuffer-Konfiguration je Verbindung, **keine belegte Obergrenze** des
   gesamten gleichzeitigen HTTP-/WLAN-Heapverbrauchs. Der heutige Body (8059 B)
   uebersteigt sie bereits; das Vor-F1-Image zeigte gemessen rund −6,9 KB
   Sendephasenkosten. Der Suffix waechst um 2351 B (+29 % **Shell-Bytes**); eine
   Sendedauer wurde **nicht gemessen**. Die Messung in H-RAM-1 wird dadurch nicht
   ersetzt.
3. **C2/C3 kosten dauerhaft rund 0,5 KB in der Summe (Dauerzustand ca. 152 B,
   Main-Stack 136 B, Heap bis ca. 210 B bei langen Namen; Aufschluesselung in
   Abschnitt 4), transient rund 1,2–2,6 KB Heap pro `/status`-Antwort und
   0,24–0,34 KB HTTPD-Stack**
   (alles ESTIMATED, Abschnitt 4). Engste Grenze ist der **HTTPD-Stack**
   (zuletzt 2336 B frei), nicht der Heap.
4. **Kein Peak-Nachweis existiert** fuer HTTP-Heap, HTTPD-Stack und Erholung auf dem
   aktuellen Stand (NOT_PROVEN). Deshalb Gate H-RAM-1 (Abschnitt 7), mit einem
   Mess-Overlay-Vorschlag, weil das Produktimage diese Messpunkte nicht liefert.
5. **Bevorzugte Minimalvariante: A** = bestehender F1-Flashsuffix, die schon
   implementierte schlichte Anzeige, einmalig neues Limit **10752 B** (Vorschlag).
   Variante B (separates Asset) ist **nicht ausgeloest** (Abschnitt 5).

## 2. Shell: Flash-/RAM-Pfad

### 2.1 Offline-Sektionsprobe (MEASURED)

Zwei Release-Profil-Builds (`esp32_release`, ESP-IDF v6.1, Toolchain
esp-15.2.0_20251204): **BASE** = Planstand ohne Produktaenderung, **CAND** = BASE +
[CANDIDATE.patch](PR206_C1_BUDGET_20261019/CANDIDATE.patch) (der gescheiterte
C1-Messkandidat). `diff -rq` BASE/CAND: genau zwei Dateien
(`web_application_routes.cpp`, `test_web_shell_polling.mjs`). Rohtabellen:
[SECTIONS.txt](PR206_RAM_FEASIBILITY_20261019/SECTIONS.txt), Hashes und
Herkunft: [MEASUREMENT.json](PR206_RAM_FEASIBILITY_20261019/MEASUREMENT.json).

| Sektion / Wert | BASE | CAND | Delta |
|---|---:|---:|---:|
| `.flash.rodata` (DROM, Adresse `0x3f40...`) | 233 884 | 236 236 | **+2 352** |
| `kWebShellSuffix` (Symbol in Sektion 16 = `.flash.rodata`) | 6 663 | 9 014 | **+2 351** (inkl. NUL) |
| `.dram0.data` | 18 975 | 18 975 | **0** |
| `.dram0.bss` | 74 488 | 74 488 | **0** |
| `.iram0.text` / `.iram0.vectors` | 100 815 / 1 028 | 100 815 / 1 028 | **0** |
| `.flash.text` | 1 401 410 | 1 401 410 | **0** |
| `.bin` (Image) | 1 756 544 | 1 758 896 | **+2 352** |
| Partition `factory` 0x2F0000 = 3 080 192 B | 57 % belegt | 57 % belegt | – |

Flash ist damit kein Engpass (rund 1,32 MB frei). Die Groesse des C2/C3-Codes
(`.flash.text`) ist **NOT_PROVEN**, weil C2/C3 nicht implementiert sind.

**Herkunft und Grenzen der Probe (kein ESP-Gate-Build):** Ad-hoc-`idf.py` in zwei
Wegwerfkopien aus `git archive HEAD`, jeweils mit lokalem Wegwerf-Git-Repo (die
eingebettete Source-SHA ist eine Probe-SHA). `scripts/build_esp_idf_profiles.py`
und `check_build_profiles.py` sind **nicht** gelaufen; nur `esp32_release` wurde
gebaut, `esp32_bringup` ist `NOT_RUN`. IDF-Commit `fff9895c...` (Tag v6.1, Arbeits-
baum sauber). ELF/BIN/Map bleiben lokal und werden nicht committet. Der 10410-B-
Kandidat wird nie ausgeliefert: Der Groessenguard haette zur Laufzeit 503 geliefert;
fuer die Sektionsmessung ist das ohne Belang. Kein Produktdiff.

### 2.2 Auslieferungspfad F1 (Code gelesen, ESP-IDF-Quelle geprueft)

`handleShell` baut nur den Kopf (`html.reserve(1536)`, Kopf 1397 B + CSRF) und setzt
`staticBodySuffix = kWebShellSuffix` (`string_view` auf das Flashliteral). Der Adapter
sendet Kopf und Suffix mit `httpd_resp_send_chunk`; ESP-IDF 6.1
`httpd_resp_send_chunk` kopiert die Nutzdaten nicht, sondern gibt sie ueber
`httpd_send_all` direkt an `send_fn` (`httpd_txrx.c`). Es gibt keine Heap-Kopie des
Suffix. Server: `stack_size=8192`, `max_uri_handlers=1`,
`send_wait_timeout=recv_wait_timeout=5 s`, `max_open_sockets=7` (Default).

### 2.3 Transient: Sendepuffer (ESTIMATED)

Generierte Release-`sdkconfig`: `LWIP_TCP_SND_BUF_DEFAULT=5760`,
`LWIP_TCP_WND_DEFAULT=5760`, `LWIP_TCP_MSS=1440`, `LWIP_MAX_ACTIVE_TCP=16`,
`LWIP_MAX_SOCKETS=10`, `ESP_WIFI_DYNAMIC_TX_BUFFER_NUM=32`,
`ESP_WIFI_DYNAMIC_RX_BUFFER_NUM=32`, `ESP_WIFI_STATIC_RX_BUFFER_NUM=10`; kein PSRAM.
Der lwIP-Socket kopiert Sendedaten in Heap-pbufs; `TCP_SND_BUF` (5760 B je
Verbindung) ist dafuer die Sendepuffer-Konfiguration, aber **keine belegte Obergrenze**
des gesamten gleichzeitigen HTTP-/WLAN-Heapverbrauchs (Segment-/pbuf-Overhead,
WLAN-TX/RX-Puffer, mehrere Verbindungen). Gemessen ist nur der Sendeanteil des
Vor-F1-Images (Abschnitt 3, D3: median −6,9 KB bis zur ACK, unabhaengig davon, wo
der Body liegt). Ob die Spitze durch 2351 zusaetzliche Suffix-Bytes steigt, ist
**nicht gemessen**; angenommen wird nur, dass sie nicht proportional steigt, weil
schon der heutige Body (Kopf 1397 + Suffix 6662 = 8059 B) die Sendepuffergroesse
uebersteigt. Der Suffix waechst um 2351 B, das sind rund **+29 % Shell-Bytes**
gegenueber 8059 B; eine Sendedauer oder Luftzeit wurde **nicht gemessen**. Die
Annahme ersetzt die Messung in H-RAM-1 nicht.
Code-Befund zum Sendetimeout (lwIP `api_msg.c`): `SO_SNDTIMEO` (5 s) wird je
`send()`-Aufruf ab dessen Start gemessen; ohne jeden geschriebenen Byte nach 5 s
folgt `EAGAIN`, bei Teilfortschritt ein Teilschreiben mit Fortsetzung in
`httpd_send_all`. Ein groesserer Chunk fuehrt deshalb nicht allein zu EAGAIN; dazu
braucht es 5 s ohne Fortschritt (Linkstall), unabhaengig von der Chunkgroesse.
Die historische Ursache der Chunkabbrueche ist **NOT_PROVEN** (Abschnitt 3).

## 3. Einordnung der vorhandenen Hardware-Evidence

Alle Werte stammen aus Dokumenten im Repository; nichts wurde neu gemessen.
**Kein gemessenes Image entspricht dem heutigen `main` `79eb9b9`** (der
Header-Fix #205 kam nach allen Laeufen). Pro Zeile sind Image, F1/F4-Stand und
Profil genannt; gemeinsame Bezeichner: F1 = Flash-Suffix per `httpd_resp_send_chunk`,
F4 = UI-Skip bei knappem groesstem Block.

| Evidence | Image / F1 / F4 / Profil | Aussage | Grenze fuer diese Frage |
|---|---|---|---|
| RES-01-Diagnose D3 [Plan §2](../tasks/issue-27-res01-heap-hardening-plan.md) | Baseline `3e94cea`, **vor F1** (8048-B-Body im Heap), kein F4 | Wurzelkette: 1696-B-WLAN-RX-Allokation (`wDev_ProcessRxSucData -> esf_buf_alloc -> wifi_malloc`) scheitert bei 7 656 B frei / 1 728 B groesstem Block nach dem Senden; danach `abort()` im UI-Render (C++-Exceptions aus). **MEASURED (vor F1):** Shell-Erzeugung median −8,7 KB, `httpd_resp_send` weitere **−6,9 KB** (lwIP-/WLAN-Sendepuffer bis zur ACK, skaliert mit gleichzeitigen Verbindungen) | zeigt den Sendeanteil als Netzpuffer, nicht als Bodylaenge; anderes Image als heute |
| G0-R2 (2026-10-14) [Ergebnis](PR203_G0_R2_20261014_RESULT.md) | Baseline `3e94cea`, **vor F1**, kein F4; 4 Sessions, Stress-Profil P-A | `BASELINE=FAIL`: erster `heap_alloc_failed` (1626 B) 126,8 s nach Lastbeginn, 14 Fehler (4x1626, 10x1696 B), Treiber `FAIL/HOLD` | nicht repraesentativ fuer den heutigen Sendepfad; Vier-Session-Stress |
| G0-R3 (2026-10-14) [Ergebnis](PR203_G0_R3_20261014_RESULT.md) | Spike C0 `9a9a8bd`, **F1, ohne F4**; 4 Sessions, P-A (3 GET/5 s je Session, Shell alle 30 s) | F1 senkt das Build-Phasendelta im Median um 7 104 B; Idle 42 532 B frei / 38 912 B groesster Block; im 120-s-Fenster (n=48 Phasenproben je Image) min. 21 592 B frei / 12 288 B Block; Shell-Phasen im Gesamtlauf min. 10 336 B frei / 3 584 B Block; Boot-Heapminimum **5 228 B**; Main-HWM mind. **11 752 B**, HTTPD-HWM **2 336 B** (Plan-Untergrenzen 11 160 / 2 080 B); Erholung 100 % nach 73,8 s; 3044/3044 Antworten HTTP 200 | Spike-Image, kein F4, Vier-Session-Stress statt Normalprofil, Phasenstichproben statt Momentanspitzen, **keine** aktuelle Recovery-Messung |
| G1 IMAGE01 [RES-01-Plan](../tasks/issue-27-res01-heap-hardening-plan.md) | Produktkandidat mit **F1 + F4** und Diagnose-Overlay; Profil P-A | `FAIL/HOLD`: `heap_alloc_failed` nach 311,9 s | zeigt: F1+F4 verhindern OOM unter Vier-Session-Stress **nicht** zuverlaessig; Owner hat den Mehrclient-Fall ausserhalb des Profils gestellt (eine Person), das Restrisiko D3 akzeptiert |
| G1b/F2-Diagnose, Stufe E (ebenda) | Diagnosebranches | R1b/B1/B2/OBS1R `FAIL/HOLD`; WLAN-/TCP-Stillstaende teils auch im Leerlauf bei gesundem Heap, Ursache offen | historische Evidence, unveraendert |
| B0–B4 (2026-10-18) [Addendum](PR203_G2_WEB_B0_B4_20261018/REVIEW_ADDENDUM.md) | PR-#203-Stand `a9a1156` (**F1 + F4 + Single-Flight-Polling**), nicht `main`; 1 Browser; Boot bestaetigt `esp32_release`, Source `a9a1156` | funktional PASS_OBSERVED: erster Root 282 ms, Login, 161 + 40 vollstaendige JSON-Dreiergruppen; Ressourcenstichproben **vor** dem Browserlauf: 42 572 B frei, 32 768 B Block, Boot-Minimum 22 080 B, Main-HWM 11 448 B; 0 `heap_alloc_failed` in 87,7 min Idle | Heap-/HTTPD-Stackspitzen, Recovery, F4-Skips: **NOT_PROVEN**; formale Abnahme `HOLD`; Transport: drei nichtterminale `ERR_CONNECTION_RESET` |
| Follow-up 02 (2026-10-17) [README](PR203_G2_WEB_FOLLOWUP02_20261017/README.md) | Produktimage `a9a1156` | `ERR_INCOMPLETE_CHUNKED_ENCODING` nach 34,88 s beim ersten Dokumentabruf: **FAIL/HOLD**; Folgeabruf nicht reproduziert ([First-GET](PR203_G2_WEB_FIRST_GET_20261017/README.md)) | Ursache **NOT_PROVEN** (EAGAIN/Funk/Socketrecycling nicht belegt) |
| #174/#27 Idle-Befund (ROADMAP) | frueherer Stand, S6-Kampagne | einmalig nicht-fatales `HEAP_ALLOC_FAILED` 1696 B im Idle, nicht reproduziert | Backtrace-Zuordnung dieses Einzelereignisses hier nicht geprueft; die dekodierte WLAN-RX-Kette gehoert zu D3 |
| F4 `PROVISIONAL` | `main` | Skip bei groesstem Block < 13 312 B, Wiederaufnahme ab 16 640 B | keine empirische Kalibrierung; kein OOM-Schutz |

Daraus: Die verfuegbaren Daten belegen **Idle-Reserven und einen funktionierenden
1-Browser-Betrieb**, aber keine Spitzen, und sie zeigen, dass die historischen
OOM-Faelle durch **gleichzeitige** WLAN-RX- und HTTP-Allokation bei knappem Heap
entstehen. Aenderungen mit transientem Heap-/Stack-Zuwachs (C2/C3) sind damit
nicht durch bestehende Evidence abgedeckt.

## 4. C2/C3 als Gesamtsystem – RAM-Bilanz je Posten

Zielarchitektur-`sizeof` (Xtensa, MEASURED,
[XTENSA_SIZEOF.txt](PR206_RAM_FEASIBILITY_20261019/XTENSA_SIZEOF.txt),
Quelle [xtensa_sizeof_probe.cpp](PR206_RAM_FEASIBILITY_20261019/xtensa_sizeof_probe.cpp)):
`FermentationUiSnapshot` 328 B, `FermentationHomeView` 104 B,
`FermentationUiRefreshRevisionTracker` 344 B, `FermentationUiProjectionInput` 256 B,
`std::string` 24 B (SSO 15 B), cJSON-Knoten 40 B. Das neue Laufdetail
(`programName` <= 96 B, `sourceKind`, `target`, `completion`, `time`) ist **nicht
implementiert**; als Scratch-Struct nach Plan §3 gemessen: **128 B**, als
`optional` 136 B (ESTIMATED; das reale Layout kann abweichen). Ein 96-B-Name liegt
ueber der SSO-Grenze: je Kopie eine Heap-Allokation von 97 B plus Blockkopf
(angesetzt rund 105 B), nur bei Namen > 15 Byte.

Bestehende Randbedingung (Code gelesen): `test_ui_steady_state_allocations`
verlangt 0 Allokationen im stationaeren UI-Schritt. Ein langer Programmname im
Snapshot darf diesen Test nicht brechen; `projectInto` muss die Kapazitaet des
Namensstrings wie bei `activeRunId` erhalten, sonst allokiert jeder Schritt neu.
C2 ergaenzt dafuer einen Fall mit 96-B-Namen im selben Test.

Kopierstellen von `FermentationUiSnapshot` (Code gelesen):

| Halter | Lebensdauer | Ort |
|---|---|---|
| `FermentationUiRefreshRevisionTracker::published_` (`optional<Snapshot>`) | dauerhaft | `FermentationApplication` |
| `UiRenderGate::snapshot_` | dauerhaft | `uiRenderGate` ist **lokale Variable in `app_main`** (`app_main.cpp:766`), also im 24 576-B-Main-Stack |
| `const auto snapshot = application_.uiSnapshot()` im Web-Handler | pro Request | HTTPD-Stack |
| Einmal-Temporary beim Boot-Render (`app_main.cpp:455`) | einmalig | Main-Stack |
| Renderer | – | nimmt `const&`, keine Kopie gefunden |

| Posten | Art | Wert | Status | Grundlage |
|---|---|---:|---|---|
| Tracker `published_` waechst um Laufdetail | Dauerzustand (Ort in `FermentationApplication`, DRAM/Heap nicht geprueft) | +136 B | ESTIMATED | Scratch-Struct, Halter oben |
| `uiProjectionInput_` (wiederverwendet) + aktueller Monotonwert | Dauerzustand | +ca. 16 B | ESTIMATED | `optional<uint64>` |
| `UiRenderGate::snapshot_` | **Main-Stack** (dauerhaft) | +136 B | ESTIMATED | wie oben |
| Namenskopien Tracker + Gate (je 97 B + Blockkopf, nur bei Name > 15 B) | **Heap** (dauerhaft) | bis ca. 2 x 105 = 210 B | ESTIMATED | SSO-Grenze 15 B |
| **Dauerhaft, getrennt** | Dauerzustand 152 B / Main-Stack 136 B / Heap bis 210 B | **Summe der Beispielzahlen 498 B** (kein belastbarer Hoechstwert) | ESTIMATED | bis das echte C2-Layout gemessen ist |
| Web-Snapshot im `/status`-Handler | pro Request, **HTTPD-Stack** | +136 B (+ ca. 105 B Heap) | ESTIMATED | |
| `std::array<char,3079>` in `serializeBounded` | pro Request, HTTPD-Stack | unveraendert 3 079 B | MEASURED (Code) | steckt bereits in der HTTPD-HWM, da alle JSON-Routen ihn nutzen |
| cJSON-Baum (Knoten 40 B + Blockkopf, Key per `strdup`, String-Werte dupliziert) heute | pro Request, Heap | rund 1,1 KB (15 Knoten) | ESTIMATED | Codec gelesen |
| cJSON-Baum, neues `run`-Objekt (ca. 14 Knoten + 14 Keys + Strings) | pro Request, Heap | **+0,9 … +1,3 KB** | ESTIMATED | je Skalarmember ca. 72 B, Strings bis ca. 230 B |
| Ausgabe-`std::string` (`output.assign`, danach `std::move`, keine zweite Kopie) | pro Request, Heap | heute ca. 0,3 KB, neu ca. 0,6 … 1,5 KB; harte Grenze 3 072 B (darueber 503) | ESTIMATED | Bodylimit unveraendert |
| Spitze beim Encodieren (Baum + String, Baum wird vor dem Senden freigegeben) | transient Heap | heute ca. 1,4 KB, neu ca. 2,7 … 4,0 KB, **Delta +1,2 … +2,6 KB** | ESTIMATED | Summe oben |
| groesster zusammenhaengender Einzelblock dieses Pfads | Heap | <= ca. 1,5 KB (String), Baum aus kleinen Bloecken | ESTIMATED | vgl. Mindestblock 3 584 B in Phasenproben (Baseline 120 s; C0 Gesamtlauf), 12 288 B im C0-120-s-Fenster |
| Codec-/Projektions-Frames im HTTPD-Task | HTTPD-Stack | +0,1 … 0,2 KB zusaetzlich zum Snapshot | ESTIMATED | keine `-fstack-usage`-Messung vorhanden |
| **HTTPD-Stack gesamt** | | **ca. +0,24 … +0,34 KB** gegen zuletzt **2 336 B frei** (Plan-Untergrenze 2 080 B, Spielraum 256 B) | ESTIMATED / **NOT_PROVEN** | engste Grenze |
| Main-Stack gesamt (Gate-Snapshot 136 B + Projektionsframes) | Main-Stack | rund +0,14 … 0,5 KB gegen mind. 11 752 B frei (Untergrenze 11 160 B) | ESTIMATED | |
| API-Bodylimit | Vertrag | 3 072 B unveraendert | – | |
| F4 | Schwellen | 13 312 / 16 640 B unveraendert; Einzelblock <= ca. 1,5 KB liegt weit darunter | – | F4 haengt am groessten Block, nicht an dieser Allokation |
| Flash fuer C2/C3-Code | Flash | nicht gemessen | **NOT_PROVEN** | 1,32 MB frei |

**Allokationsnachweis in C2 (Methode und Grenzen).** Der vorhandene Zaehler in
`test_ui_steady_state_allocations` ersetzt nur globales C++-`operator new` und
sieht **keine** cJSON-Allokationen (cJSON nutzt `malloc`/`free`). Fuer den nativen
`/status`-Worst-Case-Test erfasst eine isolierte Testinstrumentierung beide Wege:
den bestehenden `operator new`/`delete`-Zaehler und cJSON ueber die oeffentliche
Schnittstelle `cJSON_InitHooks` (nur im Test installiert und danach zurueckgesetzt,
keine Produktions-Allocator-Abstraktion). Jeder Block traegt seine Groesse in einem
Testkopf, damit Freigaben bilanziert werden. Ausgewiesen werden Allokationsanzahl,
angeforderte Bytes, groesster Einzelblock und **maximal gleichzeitig lebende
Bytes** ueber beide Wege (Baum und Ausgabestring). Messfenster: von vor
`encodeWebApiStatus` bis nach Rueckkehr, getrennt davon der gesamte Handler
(Snapshot, Encode, Response-Move). Aussagegrenzen: Host-Lauf (64-Bit, cJSON-Knoten
dort 48 B statt 40 B auf Xtensa), angeforderte statt vom ESP-Heap belegte Bytes,
kein Blockkopf/Alignment, keine Fragmentierung, keine Nebenlaeufigkeit mit WLAN,
daher kein ESP32-Heapbeweis; Zielwerte werden ueber die Blockzahlen und die
Xtensa-`sizeof`-Werte umgerechnet und bleiben `ESTIMATED`. Der
`operator new`-basierte 0-Allokations-Test des UI-Steady-State bleibt separat und
unveraendert gueltig.

**Sektionsdeltas C2/C3.** Die Nulldifferenz bei `.dram0.*`/`.iram0.*` gilt nur fuer
den isolierten Suffixkandidaten. Fuer C2/C3 sind tatsaechliche Deltas (Sektionen
und Symbole, BASE gegen den Umsetzungscommit) zu messen und mit Grund zu berichten
(z. B. `.flash.text` fuer neuen Code, `.dram0.bss/.data` fuer neue statische Objekte
oder Tabellen, `.iram0.text`), nicht vorauszusetzen.

**Wertung.** Der Heapzuwachs liegt im Bereich weniger KB bei beobachteten
Idle-Reserven von rund 42 KB und groesstem Block 32–39 KB; der Zuwachs besteht aus
vielen kleinen Bloecken plus einem String <= ca. 1,5 KB. Damit ist er plausibel
tragbar, aber **nicht bewiesen**: Die Minima im Betrieb (10 336 B frei / 3 584 B
Block in Shell-Phasenproben des C0-Gesamtlaufs; Boot-Minimum 5 228 B) und die
`heap_alloc_failed`-Historie liegen in derselben Groessenordnung wie die
Spitzensumme. Der **HTTPD-Stack** koennte die frueheren Planuntergrenze (2 080 B)
unterschreiten. Diese Grenze war ein Testkriterium des G0-R3-Laufs, keine
Safety-Grenze; die absolute Reserve bleibt dennoch erwartungsgemaess rund 2 KB. Sie
ist trotzdem Pflichtmesswert, weil sie nicht aus dem Produktimage abgelesen werden
kann. `sizeof`-Summen allein sind kein `RAM_PASS`.

## 5. Variantenvergleich

| Variante | RAM | Aufwand / Vertrag | Wertung |
|---|---|---|---|
| **A** F1-Flashsuffix, vorhandene schlichte Anzeige, einmaliges Limit | Kopf/Heap unveraendert (MEASURED 0 B DRAM, nur C1-Suffixkandidat); Sendespitze nicht proportional angenommen, nicht gemessen (ESTIMATED); +2,35 KB Flash, +2,35 KB Leitung je Shell-Abruf | Konstante `kMaximumWebShellBytes`, Budget-/Golden-Assertions, WEB_UI; kein neuer Pfad | **bevorzugt** |
| **B** separates cachebares Flash-Asset | kein RAM-Vorteil nachgewiesen: kein Nachweis, dass die Sendespitze je Request kleiner waere | zusaetzlicher GET mit Route-, Auth-, Cache- und Gesamtbudgetvertrag; mehr Verbindungen je Seitenaufruf; Kopf mit Session-Werten bleibt inline | **nicht ausgeloest** |

Im 1-Client-Normalprofil wird die Shell einmal je Seitenaufruf geholt; ein
Asset-Cache saeubert nur wiederholte Aufrufe. B nur, falls das Gate H-RAM-1 eine
Shell-spezifische RAM- oder Uebertragungsgrenze belegt. Keine Gzip-/Socket-/WLAN-/
Minifier-Massnahme vorsorglich.

**Frontend-Funktion.** Der C1-Kandidat enthaelt bereits kein Ornament, keine
Animation, kein Framework, keinen neuen Pollrequest, Timer oder DOM-Container; die
Masse sind die fachlich geforderten DE/EN/ES-Texte (15 Phasen, vier Abschlussmodi,
Labels). Er verwendet exakt die Feldnamen aus Plan §3 (`run.programName`,
`completion.{mode,coolingTargetCelsius,holdDurationMinutes}`,
`time.{scope,elapsedSeconds,remainingSeconds,remainingKind}`, `targetCelsius`,
`sourceKind`); das C3-Delta zur Bindung ist daher klein erwartet, **bis C3 NOT_PROVEN**.
Keine weitere Minifizierungs- oder Layoutkampagne; kein Informationsverlust.

## 6. Limitvorschlag D1-C (nicht freigegeben)

| Groesse | Bytes |
|---|---:|
| Kandidat angemeldet, einstellige Sequenz (MEASURED C1) | 10 410 |
| Kandidat, 20-stellige Sequenz (MEASURED C1) | 10 429 |
| bestehende Normalreserve | 128 |
| **Vorschlag harte Grenze** | **10 752** (10,5 KiB) |
| Gesamtreserve zum 10 410-B-Kandidaten | 342 |
| davon ueber der 128-B-Reserve (Spielraum fuer C3, Plan-Logik: Reserve enthaelt den 19-B-Sequenzaufschlag) | **214** |
| konservativ: Spielraum ueber 10 429 B und 128 B Reserve | **195** |

Zum Vergleich: 10 240 B (D1-B) verfehlt den Kandidaten mit Reserve um 298 B;
10 538 B laesst null Spielraum. Der Vorschlag ist eine **Policygrenze fuer
Kopf plus Suffix**, keine RAM-Ableitung; der RAM-relevante Kopf bleibt <= 1 536 B.
Harte Regeln, falls der Owner D1-C freigibt: genau eine Erhoehung, keine zweite;
C3 darf den Normalbody um hoechstens 214 B (konservativ 195 B) gegenueber dem
gemessenen C1-Stand vergroessern, sonst **HOLD** statt weiterer Erhoehung oder
Kompaktierungsrunde; Konstante, Budget-/Golden-Assertions und WEB_UI nur fuer
diesen Wert nachfuehren; 19-B-Sequenztest bleibt.

## 7. Hardware-Gate-Vorschlag H-RAM-1 (nicht ausgefuehrt, nicht freigegeben)

Voraussetzung: C1–C3 auf dem freigegebenen neuen Plan umgesetzt, Builder-Self-
Check durch, exakt gepinnter Commit/Image; **ein vergroesserter Shell-Prototyp
genuegt nicht**, weil er den JSON-/Projektions-Heap nicht erzeugt. Aktorfreies
Board, `ACTUATOR_RELEASE=NO`. Vor jedem Schritt braucht es eine gesonderte
Ownerfreigabe fuer Flash, UART, Browser und Reset.

**Image-Wahl (Owner):** Das Produktimage liefert Heap-Spitzen und HTTPD-Stack nicht
(B0–B4/First-GET dokumentieren das). Entweder (M) **benanntes Mess-Overlay** auf
demselben Commit nach dem RES-01-Muster `pre/built/sent` plus HTTPD-HWM-Log, mit
ausgewiesenem Image-Unterschied (Ergebnis = Messbild-Evidence, kein Produkt-
nachweis), oder (P) reines Produktimage; dann bleiben Spitzen und HTTPD-HWM
**NOT_PROVEN** und nur Idle/Min-Heap, Main-HWM und Fehlerzaehler sind messbar.

**Profil (1 Client, normaler Browser, kein Vier-Client-/G1'-Stress):**
S0 Boot und 5 min Idle; S1 Login; S2 Shellabruf; S3 Standby-Polling im bestehenden
5-s-Takt 10 min; S4 **nur wenn auf dem aktorfreien Board ein aktiver kanonischer
Lauf darstellbar ist** (Start ohne gueltige Sensoren und Anlegen eines Programms
mit 96-B-Namen sind **NOT_CONFIRMED**; das ist eine Owner-Vorbedingung): Polling
10 min mit gefuelltem `run`-Block; S5 Tab schliessen, 5 min Erholung. Ist S4
nicht moeglich, bleibt der Laufpfad **ESTIMATED**; den Worst-Case-Body liefert dann
der native Codec-Test in C2 (maximaler Name inkl. UTF-8/Escaping, 3 072 B, 503).

**Messwerte:** freier und minimaler interner 8-Bit-Heap; groesster Block vor,
waehrend und nach Shell und Statuscodec (nur Overlay); Main- und HTTPD-Stack-HWM;
`ui_skipped_low_memory` (haeufigere Skips sind die F4-Wechselwirkung);
`heap_alloc_failed`; Reset/Panic/Watchdog; HTTP-Fehler und unvollstaendige Chunks;
Erholung gegen den eigenen Idle-Bezug. **Referenzen statt neuer Schwellen:** G0-R3-
Werte (Abschnitt 3) als Vergleich mit ausdruecklich benannten Unterschieden (Image,
F4, Session-Zahl, WLAN/RSSI); Abweichungen (z. B. HTTPD-Stack frei unter 2 080 B,
Main unter 11 160 B) sind Ownerentscheidungen, keine automatischen Fail-Kriterien.
Rueckfallimage und Hashes pinnt der Owner im Gate-Dokument.

## 8. Entscheidung und Restrisiken

**`DECISION=GO_FOR_OWNER_GATE`** unter diesen Bedingungen: (1) Owner entscheidet D1-C
(Limit 10 752 B einmalig, oder bleibt HOLD); (2) danach Umsetzung nur nach neuer
Plan-SHA; (3) H-RAM-1 mit Image-Wahl M/P und S4-Machbarkeit separat freigeben.
`NEEDS_ALTERNATIVE` ist nicht ausgeloest (kein RAM-Befund gegen A). `HOLD`, falls
der Owner kein Limit freigibt: C1 bleibt `FAIL_BUDGET`, kein C2/C3.

Restrisiken, ausdruecklich offen: (a) HTTPD-Stack-Spielraum klein, nur geschaetzt;
(b) keine Spitzen-/Erholungsmessung auf dem aktuellen Stand; (c) `heap_alloc_failed`
(WLAN-RX) und Boot-Minimum 5 228 B nicht ursaechlich geklaert; (d) Chunk-Transport:
einmal `ERR_INCOMPLETE_CHUNKED_ENCODING`, danach nichtterminale Resets, Ursache
`NOT_PROVEN`, +29 % Shell-Bytes (Sendedauer nicht gemessen); (e) alle C2/C3-Zahlen sind Schaetzungen mit
Scratch-Layout; (f) 195–214 B C3-Spielraum ist knapp; (g) Mehrclient- und
gleichzeitige Touch-/Web-Last bleiben ausserhalb des Ownerprofils (eine Person);
(h) nur `esp32_release` offline gebaut, `esp32_bringup` und der Pre-Ready-Pfad
`NOT_RUN`.
