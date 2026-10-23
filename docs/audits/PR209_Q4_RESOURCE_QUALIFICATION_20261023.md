# PR #209 / Q4 – Ressourcenqualifikation (reine Analyse, keine Produktänderung)

Auftrag: *Ressourcenabschluss statt weiterer Optimierungsproben*. Plan: [§15](../tasks/issue-27-r1-web-remaining-scope-plan.md).
Es wurde **kein Produkt-, Test- oder Buildkonfigurationscode geändert**, nichts geflasht, keine Hardware angesprochen.

## 0. Identität und Reproduktion

| Größe | Wert |
|---|---|
| PR-HEAD (Analysebasis) | `75fd006a35a7e99337b5d9fd92ea09b8b7e94c85`; Produktquellen (`lib/`, `main/`, `src/`) identisch zu `3a1e664` (§14) |
| ELF `esp32_release` | `d1d141a223ec784fbf41ba24b5e44be2a6074547e18250ab78eaa2f5c9c9af52` |
| ELF `esp32_bringup` | `007be4bcdd3099df5319b238d156fb7146d712f4df3c4e278bc9df66c556fabc` |
| Build | `python3 scripts/build_esp_idf_profiles.py all --require-clean-source-tree` (PASS, siehe [§14-Evidence](PR209_Q4_VALUE_DELTA_20261023.md)) |
| Toolchain / IDF | xtensa-esp32-elf GCC 15.2.0 (`esp-15.2.0_20251204`); ESP-IDF `fff9895c82d744c7237be8847347bdd1b07c6643` |
| ROM-ELFs | `esp-rom-elfs 20241011`: `esp32_rev0_rom.elf` (`bef39889…`), `esp32_rev300_rom.elf` (`920b7063…`), jeweils Maximum |
| Graphbasis | `-fcallgraph-info=su`-Graph aus [PR209_Q4_VALUE_DELTA_20261023_PATHS.py.txt](PR209_Q4_VALUE_DELTA_20261023_PATHS.py.txt) (Ausgabe `<profil>-graph.json`/`-paths.json`; SDK-`.ci` wie in [COMPILE.py.txt](PR209_Q4_IMPLEMENTATION_20261023_COMPILE.py.txt)) |
| Auflösungsskript | [RESOLVE.py.txt](PR209_Q4_RESOURCE_QUALIFICATION_20261023_RESOLVE.py.txt) – Kopfkommentar nennt die `nm`/`objdump`/`objcopy`-Aufrufe und Eingabepfade; Ergebnis [STACK.tsv](PR209_Q4_RESOURCE_QUALIFICATION_20261023_STACK.tsv) |

## 1. Stack

### 1.1 Methode (gemessen am realen Link, kein neuer Build)
1. Compiler-Frames (`.ci`) wie bisher.
2. Aufrufziele ohne Compiler-Frame (release: 1 089 verschiedene Symbole; 1 892 Zeilen in [UNKNOWN.tsv](PR209_Q4_VALUE_DELTA_20261023_UNKNOWN.tsv)) werden gegen das gelinkte ELF aufgelöst: `nm` → Adresse, Prolog `entry a1, N` (`N` = Frame). Funktionen, die objdump im Flash-Text als Datenwörter ausgibt, werden aus dem Opcode `36 s1 imm` decodiert (`N = imm12·8`). ROM-Funktionen (`memcpy`, `esp_rom_printf`, libgcc-Softfloat) über ihre feste Adresse in den ROM-ELFs.
3. **Aufrufkanten der ELF-aufgelösten Funktionen werden mit ergänzt** (vorher wurden sie als Blätter behandelt): direkte `CALLn`/`J`-Sprünge. Wo objdump dekodiert, dient dessen Ausgabe als Referenz; sonst ein lineares Sweep mit Instruktionslänge aus `op0`. Validierung: auf 6 185 dekodierbaren Funktionen deckt das Sweep 6 073 (98,2 %) exakt gleich ab; ein erster Byte-Scan ohne Längensynchronisation erzeugte Phantomkanten und wurde verworfen.
4. **Terminale Pfade** (`abort`, `__assert_func`, Panic, `__cxa_throw`-Stubs, `__throw_*`) enden im Geräte-Reset und sind aus den Betriebsmaxima ausgenommen (338 Kanten, 12 Knoten); sie sind im OOM-Abschnitt (§2.3) behandelt.
5. Aus dem Quellcode aufgelöste indirekte Ziele: `_Sp_counted_base::_M_release_last_use` → alle `_M_dispose/_M_destroy` im Link; `heap_caps_alloc_failed` → `logFailedHeapAllocation` (`ESP_DRAM_LOGE` → `esp_rom_printf`, ROM-Kette `ets_printf` 144 + `_cvt` 80 + `__udivdi3` 32 = 256 B; Zweig in der Tabelle); `ConfigurationService::invokeTestHook` ruft nur `testHook_`, im Produkt nie gesetzt.
6. Rekursion: die Zyklen sind `_Rb_tree::_M_erase` und `__introsort_loop` (32 B/Ebene, Tiefe durch Containergrösse logarithmisch begrenzt; auf dem `.ci`-Graphen blieb jeder Pfad mit 8 Zusatzebenen unter dem Maximum) und JSON (`CONFIG_CJSON_NESTING_LIMIT=4`, `static_assert` im Codec). Dynamische Frames: im `.ci`-Graphen genau ein Knoten (`spi_flash_mmap_pages`), von keiner Phase erreichbar; `movsp` kommt in 6 Funktionen des Links vor (`heap_caps_init`, `pp_timer_process`, `s_i2c_start_end_command`, `tx_paon_set`, `soc_get_available_memory_regions`, `esp_system_get_time`), keine liegt auf einem Pfad.

### 1.2 Feste Abzüge am Stack (gemessen per Compilerprobe mit den Profil-Flags, plus Quelle)
| Anteil | Wert | Quelle |
|---|---|---|
| Coprozessor-Save-Area oben im Task-Stack | **96 B** (`XT_CP_SIZE`) | Probe; `port.c` `pxPortInitialiseStack` (CPSA zuerst, danach TLS, danach Startframe) |
| TLS-Bereich | **16 B** (`.tdata` 16, `.tbss` 0) | `nm` `_thread_local_*` |
| Stack-Overflow-Canary | **16 B** (`CONFIG_FREERTOS_CHECK_STACKOVERFLOW_CANARY`, Muster am unteren Ende) | Konfig; `portSTACK_LIMIT_PADDING` nicht definiert |
| ISR-Frame (Level-1-Interrupt an der tiefsten Stelle) | **192 B** (`XT_STK_FRMSZ`, `sizeof(XtExcFrame)`=112) | Probe; danach wechselt `_frxt_int_enter` auf `port_IntStack` (`portasm.S` Z. 157) |
| Startframe | 0 | nach dem ersten Dispatch freigegeben (`a1` = Rahmenobergrenze) |
| **Summe fest + ISR** | **128 + 192 = 320 B** | |

### 1.3 Ergebnis (beide Profile identisch, [STACK.tsv](PR209_Q4_RESOURCE_QUALIFICATION_20261023_STACK.tsv))
| Phase (einzelner Aufrufpfad) | Prefix | aufgelöst (ci+ELF+ROM+Closure) | + 320 B | Rest zu 8 192 B |
|---|---|---|---|---|
| Prepare | 3 056 | 6 352 | 6 672 | 1 520 |
| Confirm | 3 056 | 4 784 | 5 104 | 3 088 |
| Decision (alle neun Intents) | 3 056 | **7 440** | **7 760** | **432** |
| Run-Encode | 5 248 | 6 784 | 7 104 | 1 088 |
| **Run-Store / Write-Readback** | 5 248 | **7 440** | **7 760** | **432** |
| Settings (Namens-Commit) | 1 856 | 7 024 | 7 344 | 848 |
| Sensorselektion (Entscheidung) | 4 576 | 6 816 | 7 136 | 1 056 |
| Alloc-Failure-Zweig (Hook + ROM-printf), Run-Store | | 7 104 | 7 424 | 768 |
| GET read-only (zum Kalibrieren) | 1 280 | 5 952 | 6 272 | 1 920 |

- **Maximum 7 440 B**; gegenüber der reinen `.ci`-Bilanz (7 216 B) +224 B aus SDK/ROM-Frames und den vorher nicht verfolgten Aufrufkanten. Pfad: `writeSnapshotTransaction` 672 → `writeHeadExact` → `writeExact` → `NvsStateStore::write` → `nvs_set_blob` → … → `esp_flash_*` → `spi_flash_disable_interrupts_caches_and_other_cpu` → `esp_ipc_call_nonblocking` → `vPortYieldOtherCore` → `esp_cpu_compare_and_set` (Teilkette unterhalb `NvsStateStore::write`: 1 392 B, Prefix bis zum Aufruf 6 048 B).
- **Reserve 432 B** unterschreitet den 512-B-Planungsboden aus [§13](../tasks/issue-27-r1-web-remaining-scope-plan.md) um **80 B**. Der Boden ist ein Planungswert, keine Safety-Grenze; der ISR-Frame ist konservativ an der tiefsten Stelle angesetzt.
- **Restunsicherheit, namentlich:** `__indirect_call` und `block_absorb_post_hook` (je ein erreichbarer unaufgelöster Knoten pro Pfad). Betroffen sind laut Graph Aufrufstellen der NVS-/SPI-Flash-Kette (Treibertabelle `spi_flash_chip_*`) und ungelistete Callbacks. Der grösste Frame aller `spi_flash_*`/`esp_flash_*`-Symbole im Link ist 144 B (`esp_flash_write_encrypted`, nicht auf dem Pfad), auf dem Pfad 128 B. **Geschätzte** maximale Wirkung ≤ 144 B je zusätzlichem Hop, ≤ 2 Hops ⇒ ≤ ~290 B ⇒ Worst-Case-Schätzung 8 050 B, **Reserve ≈ 140 B**. Das ist eine begründete Schätzung, kein Nachweis.
- Gemessen (Compiler/ELF/ROM/Probe), abgeleitet (Call-Closure) und geschätzt (Restunsicherheit) sind getrennt ausgewiesen; unbekannte Frames wurden nie als 0 B gezählt (siehe Spalte *unresolved* der TSV).

### 1.4 Kalibrierung gegen vorhandene Hardware (Plausibilität, andere Commit-Identität)
H-RAM-1 ([Evidence](PR206_H_RAM_1_EVIDENCE_20261019.md); Image `0062dea` = `49dc35f`+Overlay M, **nicht dieser HEAD**) maß HTTPD-HWM frei 2 196 B = **5 996 B belegt** bei Shell-/Status-/Temperatur-/Alert-Abrufen. Der Overlay-Logpfad (`handleRequest` → `logHramHttp` → `esp_log`) ist ein **Geschwisterpfad** zum Routenpfad (`handleRequestInner`), er addiert sich nicht; er liegt mit Prefix ≈ 480 B + Wrapper/`logHramHttp` + `esp_log`/`vprintf`-Kette (≈ 560–700 B) deutlich unter dem Routenpfad. Statische Obergrenze für die GET-Routen auf dem aktuellen Link: **6 272 B** (5 952 + 320). Messwert 5 996 ≤ 6 272: das Modell wird nicht widerlegt und ist hier um ≈ 276 B (4,4 %) konservativ. Eine Modelllücke ist für diese Routen nicht erkennbar. Das ist kein Nachweis für die Mutationspfade und gilt nur unter der Annahme, dass sich die GET-Routen zwischen `a9a1156` und HEAD nicht wesentlich verändert haben.

## 2. RAM-Risiko am integrierten Produkt

Alle Hardwarewerte stammen von **a9a1156 / Overlay M** (andere Commit-Identität). Statische Abweichung zu HEAD: `.dram0.data` 18 975 = 18 975, `.dram0.bss` 74 488 → 74 520 (+32 B), `.iram0.text` 100 815 → 100 963 (+148 B) ([WEB_REMAINING_SCOPE-Evidence](PR209_WEB_REMAINING_SCOPE_IMPLEMENTATION.md)). Die Idle-Baseline ist damit statisch repräsentativ; dynamische Allokationen der späteren Slices sind es nicht.

| Posten | Wert | Quelle / Status |
|---|---|---|
| Statisches DRAM (data+bss) HEAD | 93 495 B (release), davon LVGL-Pool `work_mem_int` 49 152 B (`CONFIG_LV_MEM_SIZE`) | `size -A`, `nm -S` – gemessen |
| ISR-Stack `port_IntStack` | 3 072 B | `nm -S` – gemessen |
| Main-Task-Stack | 24 576 B aus dem Heap (`CONFIG_ESP_MAIN_TASK_STACK_SIZE`); HWM frei 11 272 B (Overlay M) | Konfig gemessen, HWM historisch |
| HTTPD-Task-Stack | 8 192 B aus dem Heap (`kHttpServerTaskStackBytes`) | Quellcode |
| Heap Idle nach Netzwerkstart (Standby) | frei 42 760 B, grösster Block 40 960 B, Minimum seit Boot 33 900 B | H-RAM-1, historisch |
| Heap unter 1-Client-Web | Minimum 19 896 B (Browser, Shell-Abruf); grösster Block min. 22 528 / 26 624 B | H-RAM-1, historisch |
| Erholung nach Last | 99,0–99,1 % frei, Block 40 960 B | H-RAM-1, historisch |
| Host-Spitzen „live“ (Maximalfixtures) | Start 45 094 B, Adjustment 43 698 B, Settings-Namenscommit 69 724 B | Host64, ohne Allocator-Overhead, nicht übertragbar |

### 2.1 Befund F-H1: Die 32 769-B-Anforderung ist inhaltsunabhängig und trifft jeden Programm-Snapshot
`encodeSingleProgramDocumentPayload` ([configuration_document_codec.cpp:704](../../lib/fermentation_app/src/configuration_document_codec.cpp#L704)) legt `ByteWriter writer(kMaximumProgramCatalogPayloadBytes)` an; der Konstruktor ruft `buffer_.reserve(capacity)` ([byte_buffer.hpp:16](../../lib/device_platform/src/byte_buffer.hpp#L16)) auf, also **immer 32 768 B (+NUL = 32 769 B) zusammenhängend, unabhängig von der Programmgrösse**. Aufrufer: `encodeProgramDocumentPayload` ← [run_persistence_codec.cpp:1207](../../lib/fermentation_app/src/run_persistence_codec.cpp#L1207), d. h. **bei jeder Kodierung eines Laufsnapshots mit Programmquelle** (Start, Command-Persistenz, Checkpoints). Dagegen sind die Katalogkodierung (`ByteWriter writer(size.payloadSize)`, Z. 1000) und `NvsStateStore::read` (`std::string value(requiredSize)`) inhaltsgrössenbezogen. Das erklärt, weshalb die Host-Fixtures mit dem Start-/Replay-Pfad immer 32 769 B melden, auch ohne Maximalkatalog.
**Folge:** Das Largest-Block-Risiko betrifft den Normalbetrieb mit Lauf, nicht nur den Maximalkatalog: ein Block ≥ 32 769 B (zzgl. Allocator-Overhead) muss bei jedem Snapshot frei sein. Im Idle (40 960 B) passt er mit ≈ 8 KB Spiel, unter dem gemessenen Web-Verkehr (grösster Block min. 22 528–26 624 B, andere Identität) nicht. Ob das auf diesem HEAD zeitgleich eintritt, ist **nicht gemessen**.
**Kleinste Massnahme (nicht umgesetzt, separat zu genehmigen):** den Einzelprogramm-Writer wie die Katalogkodierung nach der berechneten Nutzlastgrösse dimensionieren (die Grössenfunktion existiert für den Katalogpfad). Wirkung: Einzelanforderung sinkt von 32 769 B auf die tatsächliche Programmgrösse (Grössenordnung ≤ 1,5 KB), ohne Änderung von Persistenzformat oder Fehlersemantik.

### 2.2 Maximalkatalog
D6 (kein Programmeditor) betrifft **nur das Web**. Der lokale Editor (`FermentationUiCommandBridge::applyProgramEdit` → `fermentation_ui_editing.cpp`) legt bis `kMaximumProgramCount`=16 Programme mit Notizen bis 1 024 B an. Der Maximalkatalog ist daher am Gerät erreichbar. Die Host-Spitzen (45–70 KB) liegen über dem freien Idle-Heap von 42,8 KB; sie zählen angeforderte Bytes mit 64-bit-Zeigern (`std::string` 32 B statt 24 B, `std::vector` 24 B statt 12 B auf dem Ziel), enthalten bereits lebende Katalogkopien und sind nicht übertragbar. Ein Zielwert ist nicht abgeleitet; ein Maximalkatalog-Heapnachweis fehlt.

### 2.3 Zielverhalten bei Allokationsfehler (`-fno-exceptions`) – im Link nachgewiesen
`operator new(size_t)` (`_Znwj`) ruft bei `malloc == nullptr` `__wrap___cxa_allocate_exception` und `__wrap___cxa_throw`; Letzteres ist im Binary ein reiner `callx8 abort`. `std::__throw_bad_alloc`/`__throw_length_error` (Wachstum von `std::string`/`std::vector`) enden auf derselben Stelle. `CONFIG_ESP_SYSTEM_PANIC_PRINT_REBOOT=1`, `CONFIG_ESP_SYSTEM_PANIC_REBOOT_DELAY_SECONDS=0`.
**Konsequenz:** String-/Vektor-/`new`-OOM führt zu Abort und sofortigem Geräte-Neustart, nicht zu einem Fehlerstatus. `cJSON`-Allokationen laufen über Null-Rückgaben (bestehender 400-Ausgang). Das Verhalten **besteht produktweit vor PR #209** (Folge von `-fno-exceptions` unter ESP-IDF) und wurde durch dieses PR nicht eingeführt. Dass der Neustartpfad aktorsicher ist (Boot = Ausgänge aus, Fail-Closed), ist Gegenstand des Safety-/Recovery-Vertrags und wurde hier **nicht** neu bewiesen. Der Failed-Alloc-Hook (`heap_alloc_failed`, ROM-Print) ist registriert und läuft im Kontext des allozierenden Tasks (Zweig in §1.3).

## 3. Vorhandene Messmittel (vor jeder Neuinstrumentierung geprüft)
- **Im Produkt:** `logResources()` in [app_main.cpp](../../main/app_main.cpp) (freier Heap, Minimum, grösster Block `MALLOC_CAP_8BIT`, Main-HWM in **Bytes**; `StackType_t` ist unter ESP-IDF `uint8_t`) und `heap_caps_register_failed_alloc_callback` als erste Anweisung von `app_main`.
- **Als Diagnosepatch (nicht im Produkt):** [Overlay M](PR206_H_RAM_1/overlay_M.patch) (HTTPD-Handler loggt vor/nach Anfragen Heap-Tripel + `uxTaskGetStackHighWaterMark(nullptr)`; 30-s-Ressourcenprobe). Nur vier GET-Routen, keine Mutationsrouten, für `a9a1156` erstellt.
- **Nicht eignungsfähig für diesen HEAD:** alle Hardwarewerte oben (Commit-Identität ≠ HEAD, Standby, 1 Client, keine Mutation, kein Lauf).

## 4. Gate-Werte

| Gate | Wert | Begründung |
|---|---|---|
| `HTTPD_FULL_CHAIN_STACK` | **NOT_PROVEN** | aufgelöstes Maximum 7 440 B (+ 320 B = 7 760 B < 8 192 B); formal offen: `__indirect_call`-Zielmengen (Schätzung ≤ ~290 B). Kein Hardware-HWM auf Mutationspfaden. |
| `HTTPD_STACK_RESERVE` | **FAIL** (gegen den 512-B-Planungsboden) | aufgelöste Reserve 432 B (80 B Fehlbetrag); Worst-Case-Schätzung ≈ 140 B. Planungswert, keine Safety-Grenze; ändert nichts an der rechnerischen Tragfähigkeit. |
| `ESP32_HEAP_PEAK` | **NOT_PROVEN** | kein Zielwert für Mutation/Lauf/Maximalkatalog; Host-Spitzen nicht übertragbar |
| `ESP32_LARGEST_FREE_BLOCK` | **NOT_PROVEN** (mit Risikoindikation F-H1) | 32 769-B-Anforderung bei jedem Laufsnapshot vs. gemessene Minima 22 528–26 624 B unter Web-Verkehr auf anderer Identität |
| `TARGET_OOM_BEHAVIOR` | **FAIL** (kein Fehlerstatus; Abort + Neustart) | siehe §2.3; produktweit vorbestehend, nicht in PR #209 entstanden; blockiert `C5_RESOURCE_GATE` nicht eigenständig – C5 hängt an den Heap-Gates; die Akzeptanz des Reset-Pfades als Fehlervertrag bleibt eine bereits bestehende, separate Ownerentscheidung und ist nicht Teil von `H-RAM-2` |
| `C5_RESOURCE_GATE` | **HOLD** | keine hardware- oder produktsichere PASS-Evidence für Mutationspfade und Heap-Gleichzeitigkeit |

Compilerwerte und Host-Tests sind in keinem dieser Gates als Hardware-HWM gewertet.

## 5. Optionen (nur Entscheidungsgrundlage)

| Option | Stack-Rest (aufgelöst, inkl. 320 B) | RAM-Wirkung | Bewertung |
|---|---|---|---|
| **A – 8 192 B behalten** | 432 B (Worst-Case-Schätzung ≈ 140 B) | 0 | rechnerisch tragfähig, Planungsboden um 80 B verfehlt; Bestätigung durch Hardware-HWM offen |
| B′ – 8 704 B (+512 B) | 944 B (Worst-Case ≈ 650 B) | −512 B internes RAM (≈ −1,2 % des freien Idle-Heaps) | kleinste Massnahme, die den 512-B-Boden auch unter der Worst-Case-Schätzung erfüllt; braucht eigene Freigabe |
| C – 10 240 B (+2 048 B) | ≈ 2 480 B | −2 048 B (−4,8 %; grösster Block 40 960 → ≈ 38 900 B) | verschärft den knappsten Wert (Heap-Block); nicht empfohlen |
| D – 10 240 B, finanziert durch Main-Task 24 576 → 22 528 B | ≈ 2 480 B | netto 0 B | Main-HWM frei 11 272 B (historisch) bliebe ≈ 9 200 B; Buildkonfigurationsänderung |

**Heap:** Massnahme M-H1 (§2.1) beseitigt die inhaltsunabhängige 32-KB-Anforderung ohne zusätzliches RAM; sie ist die kleinste Massnahme für das knappste Gate und unabhängig von der Stackfrage. Settings-Decode-/UI-Katalogumbau (Option B aus §13) wird nicht vorgeschlagen; er bleibt nur bei einem auf Hardware nachgewiesenen Heap-Engpass die priorisierte nächste Option.

## 6. Das eine Gate (Hardware/Instrumentierung) – `H-RAM-2`
Siehe [§15.5](../tasks/issue-27-r1-web-remaining-scope-plan.md#155-ein-gate-h-ram-2). Kein Versuch ohne Ownerfreigabe der exakten Plan-SHA **und** der getrennten Flash-/Hardwareautorisierung.
