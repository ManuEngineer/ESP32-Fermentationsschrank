# Issue #27 / PR #203 – RES-01 F2-Eskalation: Heap-Reserve in der HTTP-Sendephase (Plan)

Status: `PLAN_ONLY_PENDING_INDEPENDENT_PLAN_REVIEW`. Keine Umsetzung, kein Build,
kein Flash, kein Reset, keine Last, kein Touch, kein G2, kein Merge.
`G1_IMAGE01=FAIL/HOLD`, `RES-01=BLOCKER`, `F4_THRESHOLD=PROVISIONAL_PENDING_G1`,
`PR203_FINAL_GO=HOLD`, `ACTUATOR_RELEASE=NO` bleiben unveraendert.

Dieser Plan ist der "eigene Plan" nach Abschnitt 8 von
`docs/tasks/issue-27-res01-heap-hardening-plan.md` (im Folgenden **Plan F1/F4**).
Er ist eigenstaendig lesbar; die dort freigegebenen Abschnitte 7 (Hardwareverifikation,
Lastprofile, Abnahmekriterien) und 9 (D1–D5) gelten unveraendert und werden hier
nur referenziert, nicht kopiert und nicht reduziert.

## 1. Ziel und Nicht-Ziele

**Ziel:** Mit dem kleinsten wirksamen Eingriff klaeren und, falls belegt, beheben,
dass waehrend HTTP-Sendephasen (insbesondere der Shell-Antwort) der freie interne
Heap so weit sinkt, dass der WLAN-Treiber eine Allokation (`size=1626 caps=0x80c`)
nicht mehr erhaelt. Die Wahl des Hebels wird erst nach einem engen
Mess-/Entscheidungsgate getroffen.

**Nicht-Ziele (unveraendert aus Plan F1/F4 und Auftrag):**

- keine eigene Drossel-/Puffer-/Scheduler-Architektur, keine Abstraktion auf Vorrat;
- keine automatische Kombination von Massnahmen; je Hardwarearm genau eine Aenderung;
- keine Aenderung der F4-Schwelle, von Web-/Auth-/Session-/JSON-Vertraegen, Polling
  oder Sessionlimit;
- WLAN-RX-/TX-Puffergrenzen nur als ausdruecklich ownerpflichtige Option O3
  (Abschnitt 3), nicht Teil der bevorzugten Option;
- kein Testumfang-Abbau: P-A/P-A2/P-B/P-R, G1 und G2 aus Plan F1/F4 Abschnitt 7
  bleiben massgeblich.

## 2. Verifizierte Ausgangslage

Quellen: Plan F1/F4 (Abschnitte 2, 7, 8); `docs/audits/PR203_G0_R3_20261014_RESULT.md`;
sealed G1-Paket `diag/review/g1-image01/` (Evidence-Commit `a361f51edb9a1d3175ac5af5dea32838b9075cef`,
Branch `agent/pr203-res01-g1-review-nonmerge`); Offline-Analyse
`diag/review/g1-image01-followup/FOLLOWUP_OFFLINE_ANALYSIS_20261014.md`
(`d4cbce88723b0ef61c3a5e4cb523a1ccfa3b9d56`) und
`LOAD_ABORT_PROVENANCE.md` (`7485f1a`); ESP-IDF 6.1
(`opt/espressif/esp-idf-v6.1`); Image-`sdkconfig` des G1-Builds. Produkt-HEAD
`d35ee85edf4f93f20e9edbe13f8851e406d1b82b`.

### 2.1 Trigger

Plan F1/F4 Abschnitt 8: `heap_alloc_failed` trotz F1 + F4. G1 IMAGE01 stoppte nach
311,879 s von 1200 s mit `heap_alloc_failed size=1626 caps=0x0000080c`
(`INTERNAL|DMA|8BIT`), `function=heap_caps_mall…` (abgeschnitten, weil der Host den
UART nach der Anomalie nicht weiterlas). Damit ist Kriterium 5 (Fail, F2-Bewertung)
erfuellt.

### 2.2 Belegte Befunde (Beweisklassen: BELEGT / ABGELEITET / HYPOTHESE / NOT_PROVEN)

| # | Befund | Klasse |
|---|---|---|
| a | Der Aufrufer der **1626-B-Allokation ist nicht dekodiert**. Der in Plan F1/F4 Abschnitt 2 genannte WLAN-TX-Puffer ist aus der IDF-Dokumentation geschlossen, der D3-Backtrace dekodierte nur die 1696-B/`caps=0x1800`-Allokation (`esf_buf_alloc_dynamic → wifi_malloc → malloc`, WLAN-RX). 1626 B/`0x80c` kam in D3 4 × vor, ohne Backtrace. Eine statische IDF-Quellsuche passt auf `esp_coex_common_malloc_internal_wrapper` (Wi-Fi-/Coex-Blob); geschlossene Bibliotheken sind nicht pruefbar. | NOT_PROVEN / HYPOTHESE |
| b | Dem Ereignis ging eine ca. 43 s lange Lieferstoerung voraus (1,5–5,1 s je GET), zuletzt 2,8 s ohne jeden Request-Abschluss; Shell A: Sendephase 2,6 s; Shell B startete mit 14 608 B frei (`built`), ≈ 11 KB unter dem bisherigen Laufminimum der `built`-Werte (25 628 B). Alles Phasenstichproben. | BELEGT (Stichprobe) |
| c | Bedarf der Sendephase (`free(built) − free(sent)`): G1 Median 8 244 B, p90 14 696 B, Max 16 348 B; C0-R3 Median 8 220 B, p90 14 476 B, Max 17 268 B. Kleinstes `free(built)`: G1 14 608 B, C0-R3 17 984 B. Die Differenz ist die Summe aller gleichzeitigen Verbraucher, nicht einem Request zuordenbar. | BELEGT (Stichprobe) |
| d | Ob die gebundene Speichermenge in lwIP-Sendewarteschlangen, Wi-Fi-TX-Puffern oder beidem liegt, ist **nicht gemessen**. Mit `MEM_LIBC_MALLOC=1` und `MEMP_MEM_MALLOC=1` (`lwipopts.h:98,105`) liefert `CONFIG_LWIP_STATS` fuer `tcp_write`-Kopien keine Bytezaehlung (Daten liegen in libc-Heap-Bloecken); verwertbar waeren nur TCP-Zaehler (Retransmit, Speicherfehler). | ABGELEITET (Quelle) |
| e | Obere Schranke gebundener TCP-Sendedaten: `max_open_sockets × TCP_SND_BUF` = 7 × 5760 B = **40 320 B** bei Idle-Heap ≈ 41 KB (G1 41 132 B). Reine Obergrenze, nicht erwarteter Wert (API-Antworten sind klein; nur die Shell fuellt den Puffer). | ABGELEITET |
| f | `HTTPD_DEFAULT_CONFIG()` ist unveraendert (`max_open_sockets=7`, `backlog_conn=5`, `lru_purge_enable=false`, `recv/send_wait_timeout=5 s`; `esp_idf_http_server_lifecycle.cpp:192-199`). Beide Laeufe erreichten mehr als 7 gleichzeitig offene Client-Requests (G1 max 8, C0-R3 max 11); C0-R3 ueberlebte das. Die Parallelitaet ist deshalb Risikofaktor, nicht unterscheidend. | BELEGT |
| g | G1 und C0-R3 sind je n = 1. Es gibt keine Fehlerrate und keine Zurechnung zu F4/Instrumentierung (Idle −1 400 B, statisch +1 452 B DRAM bei gleichem Bedarfsmuster). | NOT_PROVEN |
| h | Ursache der Lieferstoerung (Funk, Access Point, lwIP, Wi-Fi-Puffer): unbekannt. Die UART-Logs zeigen keine Trennung, keinen Scan und kein Roaming; Wi-Fi-Powersave `WIFI_PS_MIN_MODEM` ist in beiden Laeufen aktiv. | NOT_PROVEN |

### 2.3 Evidence-Errata (Auftrag Abschnitt 4)

- Das sealed Paket `g1-image01/` auf `a361f51` bleibt unveraendert; die Analyse
  `d4cbce8` bleibt bestehen.
- Der erste Abbruchgrund `abort = g1-supervisor-stop` ist mit minimaler, bereinigter
  Provenienz in `LOAD_ABORT_PROVENANCE.md` (`7485f1a`) belegt (Datei-Hash, `marks`,
  keine Secrets).
- Die sechs `http-protocol-error` werden als **stark gestuetzte Supervisor-
  Abbruchfolgen** gefuehrt, nicht als unabhaengig nachgewiesene Produktfehler und
  nicht als unabhaengig bewiesen (kein Drahtbeleg). Die Zeitordnung beruht auf der
  Codekette und dem Abbruchgrund, **nicht** auf millisekundengenauer Gerätezeit aus
  Datei-mtime; mtime ist nur Plausibilitaetspruefung. Der siebte `timeout`
  (Session 1, `temperatures`, 3,001 s) bleibt realer Clientbefund mit offener Ursache.
- Plan F1/F4 Abschnitt 2 (Zeile "1626 B … DMA-TX-Puffer") wird in diesem Plan nicht
  editiert; die Praezisierung steht in Befund a. Eine Korrektur dort ist Teil der
  Dokumentationswirkung (Abschnitt 8) und braucht keine Ownerentscheidung.

## 3. Optionen (einfachste zuerst) und Bewertung

Alle Optionen sind Kandidaten; keine ist freigegeben. Savings sind rechnerische
Obergrenzen je gleichzeitig volle Sendewarteschlange, **keine Erfolgsbehauptung**
(ungleichzeitige Peaks werden nicht gemittelt).

| Option | Aenderung | Wirkung (Hypothese) | Nachteile / Risiken (gegen IDF-6.1-Vertrag) | Scope |
|---|---|---|---|---|
| **O0** | keine Konfigurationsaenderung; nur Diagnose (Abschnitt 4) | erzeugt die Entscheidungsgrundlage | kostet einen Hardwareabschnitt; `RES-01` bleibt Blocker | im Plan F1/F4-Rahmen |
| **O1** | `CONFIG_LWIP_TCP_SND_BUF_DEFAULT` 5760 → 2880 (`sdkconfig.defaults`, gilt fuer `esp32_release` und `esp32_bringup`) | begrenzt je Verbindung gebundene Sendedaten um bis 2 880 B; obere Schranke aus Befund e halbiert sich auf 20 160 B. Nur Antworten > 2 880 B (Shell) profitieren. | Gueltig (Kconfig-Minimum 2440). `TCP_SND_QUEUELEN` skaliert mit (17 → 9 pbufs). Mit 2 Segmenten (MSS 1440) im Flug kann lwIP kein Fast Retransmit ausloesen (benoetigt 3 Duplicate-ACKs, `tcp_in.c:1214-1243`); ein Verlust wird nur per RTO behoben (Initialwert `CONFIG_LWIP_TCP_RTO_TIME=1500`). Mehr Sendeschritte je Shell-Antwort: Shell-Latenz/Durchsatz koennen steigen. Globaler lwIP-Default (nur der HTTP-Server nutzt TCP im Produkt). | Plan F1/F4 Abschnitt 8 nennt genau diesen Kandidaten; Ownerfreigabe des Hardwarearms noetig |
| **O2** | HTTP-Parallelitaet begrenzen: `max_open_sockets` (Default 7) und/oder `lru_purge_enable` im Adapter (`esp_idf_http_server_lifecycle.cpp`, ein Konfigurationsfeld, keine neue Abstraktion) | begrenzt Anzahl gleichzeitig gebundener Sendewarteschlangen (Obergrenze N × 5 760 B) | Produktcode- und Vertragsaenderung. Kleineres N laesst Browser-Keep-Alive-Verbindungen Slots belegen; weitere Verbindungen warten im Backlog (5) und laufen bei SYN-Verlust in Wiederholungen (Sekunden) oder Clientfehler; `lru_purge_enable=true` schliesst die aelteste Verbindung auch mitten in einer Antwort (Abbrueche beim Client). Vier geforderte Sessions koennen mit je mehreren Browserverbindungen den Wert uebersteigen; ohne P-B nicht beurteilbar. | im Text von Plan F1/F4 Abschnitt 8 ("Begrenzung der Verbindungsparallelitaet"); Ownerfreigabe noetig |
| **O3** | `CONFIG_ESP_WIFI_DYNAMIC_RX_BUFFER_NUM` / `…_DYNAMIC_TX_BUFFER_NUM` (Image: 32 / 32) | begrenzt die Wi-Fi-Spitze (RX theoretisch bis ≈ 50 KB bei ≈ 1,6 KB/Puffer) | Paketverlust/Durchsatz; beruehrt die vom Wi-Fi-Treiber selbst gebundenen Puffer; ausserhalb des genehmigten F2-Textes | **nur ausdruecklich ownerpflichtig und separat**; nicht Teil der bevorzugten Option |

Nicht vorgeschlagen: Eigenentwicklung (Drossel, Pool, Antwortscheduler),
Wi-Fi-Powersave-Aenderung (kein Beleg), F4-Schwellenaenderung (Abschnitt 1).

### 3.1 Bevorzugte kleinste Option

**Gestufte Reihenfolge, je Stufe eigenes Owner-Gate:**

1. **Stufe D (O0):** Diagnose nach Abschnitt 4. Sie unterscheidet, ob die 1626-B-
   Allokation zur HTTP-Sendephase gehoert und ob die Lieferstoerung am Draht
   (Retransmits, Zero-Window, SYN-Wiederholungen) oder im Geraet entsteht.
2. **Stufe E (O1):** Nur wenn Stufe D die sendebezogene Deutung stuetzt: **O1 als
   einzige Aenderung** (konfigurationsbasiert, kein Code, blast radius = TCP-Sockets
   des HTTP-Servers), vergleichend gemessen nach Abschnitt 5.
3. O2 nur, wenn O1 laut Abschnitt 5 nicht wirkt oder harte Kriterien reissst; O3
   nur auf ausdruecklichen Ownerentscheid. Nie gleichzeitig mit O1.

Begruendung: O1 ist reversibel, hat keinen Codeanteil, ist im freigegebenen Plan
benannt und ist am engsten an den belegten Verbraucher gekoppelt (Sendewarteschlange).
O2 aendert Produktcode und kann mit der geforderten Vier-Session-Nutzung durch
Keep-Alive/Backlog neue Fehlerklassen erzeugen. Die offene Unsicherheit von O1
(Retransmit-Verhalten bei 2 Segmenten) wird durch das Gate gemessen, nicht
angenommen.

## 4. Mess- und Entscheidungsgate (Stufe D, schlank)

**Zielfragen, die die Stufe entscheiden muss:** (Q1) Wer fordert 1626 B/`0x80c` an?
(Q2) Gehen der Anomalie Retransmits/Zero-Window/Verlust am Draht voraus oder bleibt
das Geraet ohne Drahtbefund langsam? (Q3) Bleibt der Heap nach dem Ereignis
erschoepft (selbstverstaerkend wie in D3) oder erholt er sich?

Nicht gleichzeitig: jede Massnahme hat eigene Begruendung und Kosten.

| ID | Massnahme | Beantwortet | Kosten (vorab quantifiziert) | Status |
|---|---|---|---|---|
| **D1** | Backtrace im bestehenden Failed-Alloc-Hook nach der bereits statisch geprueften v3-Technik (`diag/check_hook_cache_safety.py`, `agent/pr203-res01-diag-local@3e94cea`): Flash-Referenzen im Aufrufabschluss = 0, Cache-Abfrage vor Dereferenzierung, ≤ 4 Ereignisse, atomarer Zaehler, kein Heap/Lock/`ESP_LOGx`. Nur auf dem Diagnosebranch (Basis `bb4f43a`), nie nach `main`. | Q1 | Gemessen in v3 (`REVIEW_PACKAGE_V3.md`): Image **+832 B Flash**; Stack in der **fehlschlagenden Task zusaetzlich ≈ +112 B** (Hook 48 B + Aufrufkette ≤ 96 B, nie gleichzeitig); Laufzeit **+10,5 ms je Ereignis**, ≤ 4 Ereignisse, ≤ 70 ms gesamt gegen Interrupt-Watchdog 300 ms; keine statische DRAM-Zunahme ausser wenigen Bytes Zaehler. **Abbruchkriterium:** Die Pruefung am neuen ELF muss 0 Flash-Referenzen und diese Stack-/Zeitwerte reproduzieren; sonst keine Freigabe. | vorgeschlagen |
| **D2** | **Hostseitiger Header-Mitschnitt** des Lastlaufs (`tcpdump` auf der Host-NIC, `snaplen` nur bis einschliesslich TCP-Header, kein Payload, keine Cookies). Auswertung: Retransmits, Duplicate-ACKs, Zero-Window, RTT/ACK-Abstaende, SYN-Wiederholungen (Erklaerung des 3-s-Verbindungstimeouts), Zeitpunkt der Sperrung durch den Supervisor. Veroeffentlicht werden nur abgeleitete Zaehler. | Q2 (und trennt Abbruchfolgen vom echten Befund) | **0 B / 0 µs auf dem Geraet.** Host: Privileg fuer Paketmitschnitt noetig, Hostlast gering. | vorgeschlagen |
| **D3** | UART-Nachlauf: nach der Anomalie den UART weitere 5 s lesen und jeden Rohchunk mit Hostzeit stempeln, bevor der Port geschlossen wird. Lasttreiber: je Sample Start **und** Ende, `guard.reason` mit Zeitpunkt; Verbindungs- und Lesetimeout unterscheiden. | Q3, Zeitordnung | **0 B / 0 µs auf dem Geraet** (reines Host-Verhalten); die harte Lastbeendigung bei Anomalie (Plan F1/F4 Abschnitt 7) bleibt unveraendert. Begruendung: Ohne Nachlauf bleibt unbekannt, ob ein Einzelereignis wie in G1 folgenlos bleibt oder wie in D3 zum `abort()` fuehrt. | vorgeschlagen |
| *(nicht Default)* | `CONFIG_HEAP_TASK_TRACKING` (Speicher je Task) | trennt Verbraucher grob | Overhead haengt von der Zahl der Heap-Bloecke ab (ein zusaetzlicher Zeiger je Block). **Nur nach Ownerentscheid und nach Messung von `heap_caps_get_info().allocated_blocks` in D1/D3** (dann Kosten = Bloecke × Blockzusatz, vorab berechnet; Abbruch, wenn sie 10 % des Idle-Heaps (≈ 4 KB) uebersteigen). | zurueckgestellt |
| *(nicht vorgesehen)* | `CONFIG_LWIP_STATS` | – | Liefert wegen Befund d keine Bytezaehlung der Sendewarteschlange; TCP-Zaehler duplizieren D2. | verworfen |
| *(nicht vorgesehen)* | `CONFIG_HEAP_ABORT_WHEN_ALLOCATION_FAILS` | – | Auftrag: nicht als Default; ersetzt das gesicherte Ereignis durch Reset. | verworfen |

**Entscheidungsregel nach Stufe D (vorab, mechanisch):**

| Ergebnis | Folge |
|---|---|
| D1: 1626 B stammt aus dem Wi-Fi-TX-/Sendepfad (`esf_buf_alloc`/Wi-Fi-Task) **und** D2 zeigt Retransmits/Verzoegerungen oder Zero-Window vor der Anomalie **oder** zeigt keine Drahtanomalie bei gleichzeitig stockender Antwortlieferung des Geraets | Stufe E (O1) wird dem Owner zur Freigabe vorgelegt |
| D1: Aufrufer ausserhalb von Wi-Fi/Sendepfad (z. B. App-Task) | F2 ist die falsche Eskalation; Rueckfall in den Plan (neue Ursachenbewertung) |
| D1: kein Ereignis in den freigegebenen Laeufen | **keine** Entlastung (Plan F1/F4 Abschnitt 7: ein Ausbleiben unter Diagnose ist kein Entlastungsbeweis); Entscheidung ueber Wiederholung/O1 beim Owner mit der gemessenen Wiederholbarkeit |
| D3 zeigt selbstverstaerkende Erschoepfung (Heap > 55 s erschoepft wie D3) | Schweregrad F2 steigt, Hinweis an den Owner zur F4-Restgefahr-Akzeptanz (D3 des Plans F1/F4) |

**Stufe-D-Lauf (eigenes Owner-Gate G1b, nicht Teil dieses Auftrags):** Diagnoseimage
`G1 IMAGE01 (bb4f43a) + v3-Hook-Delta`, App-only `0x10000`, Rueckfall wie G1,
aktorfrei, P-A bis zur ersten Anomalie oder 1200 s + 300 s P-R, bis zu 3 frische Boots
(Plan F1/F4 Abschnitt 7 verlangt 3 × P-A; die Wiederholung liefert zugleich die
erste Fehlerrate dieses Images). Concrete Gate-Angaben (Commit, Hashes, Stopp) legt
der Owner-Gate-Auftrag fest.

## 5. Vergleichs- und Erfolgskriterien (vorab, nicht nachtraeglich waehlbar)

**Rahmen:** Plan F1/F4 Abschnitt 7 bleibt massgeblich (P-A 3 ×, P-A2, P-B, P-R, G1,
G2; harte Kriterien 1–6). Dieser Abschnitt ergaenzt nur die Vergleichslogik fuer
Stufe E und verkuerzt nichts.

**Arme und Bedingungen:**

- Arm A = Diagnoseimage der Stufe D (`bb4f43a` + D1-Hook), unveraendert.
- Arm B = A + **genau eine** Aenderung O1 (`CONFIG_LWIP_TCP_SND_BUF_DEFAULT=2880`),
  gleicher Commit-Stand sonst, Images gepinnt (SHA-256 in der Evidence).
- Identisch zwischen A und B: Board/AP/Kanal (UART-Beleg der Anbindung), Treiber- und
  Capture-Hashes (`c0_load.py`, `g1_capture.py` bzw. die in Stufe D gehaerteten
  Fassungen), Sessionzustand (vier frische Sessions, Altersangaben), Uhrzeitfenster
  (A und B am selben Tag, in wechselnder Reihenfolge), Idle-Referenz je Boot,
  Netzwerkbedingungen laut Testblatt (RSSI/Kanalauslastung sind nicht instrumentiert und
  bleiben als Einschraenkung ausgewiesen).

**Harte Kriterien (absolut, aus Plan F1/F4 Abschnitt 7, je Lauf von B):**

1. kein `heap_alloc_failed`, kein `abort`/Panic/Watchdog/Brownout/unerwarteter Reset;
2. HTTP 100 % 2xx, keine Timeouts, keine Verbindungsabbrueche waehrend der Last
   (Supervisor-Abbruchfolgen zaehlen nicht als Produktfehler, werden aber getrennt
   ausgewiesen);
3. Erholung nach 300 s ≥ 90 % des Vier-Session-Idle-Referenzwerts fuer freien Heap und
   groessten Block innerhalb 120 s;
4. Stack-HWM (httpd/Main) nicht schlechter als die Baseline um mehr als 256 B;
5. F4-Verhalten wie Kriterium 6 des Plans F1/F4 (Gate-Ausloesen ist Befund).

**Vergleichsmessgroessen (berichtet; gelten nur gegen die gleichzeitig gemessene
Referenz A):** freier interner 8-Bit-Heap und groesster Block (`pre/built/sent`,
`diag_30s`); Allokationsfehler; Abfall `built→sent` (Median/p90/Max); Latenz je
Pfad (API und Shell getrennt) mit p50/p95/p99/max je Minute und Anzahl > 3 s;
Shell-Gesamtdauer und Durchsatz (Byte/s); HTTP-Erfolgsquote und Socketfehler; D2-Zaehler
(Retransmits, Duplicate-ACKs, Zero-Window); Stackreserven; Recovery.

**Latenz-/Durchsatztoleranz:** Eine Toleranz fuer B gegenueber A ist eine
**Ownerentscheidung vor dem ersten Flash** und wird im Testblatt festgeschrieben
(Plan F1/F4 Abschnitt 7: keine nachtraeglich angepasste Schwelle). Vorschlag zur
Entscheidung: p95 je Pfad und Shell-Gesamtdauer von B hoechstens das Eineinviertelfache
des groesseren der beiden A-Laeufe; jede Ueberschreitung ist Befund fuer den Owner,
kein automatischer Fail; ein Timeout bleibt harter Fail (Kriterium 2).

**Aussagegrenzen (vorab festgehalten):** Der freie Heap wird als Minimum/Verteilung
berichtet, nicht aus rechnerischen Mittelwerten ungleichzeitiger Spitzen abgeleitet.
Drei fehlerfreie P-A-Laeufe von B sind kein statistischer Nachweis: Ohne eine bekannte
Fehlerrate von A liegt die obere 95-%-Grenze der Fehlerwahrscheinlichkeit bei
n = 3 und 0 Fehlern bei ≈ 63 % (exakte Binomialgrenze). Der Plan fordert
deshalb, die in Stufe D gemessene A-Fehlerrate im Bericht neben B zu nennen; die
Abnahme bleibt eine Ownerentscheidung unter ausdruecklicher Restgefahr-Akzeptanz
analog D3 des Plans F1/F4. Eine fehlerfreie B-Serie beweist keine allgemeine
OOM-Sicherheit. Bei fehlgeschlagenem harten Kriterium von B: kein automatischer
Wechsel auf O2/O3, Rueckfall in den Plan.

## 6. Umsetzungs- und Commit-Schnitte (nach Planfreigabe der exakten SHA)

Hardware-Handlungen sind je eigene Owner-Gates (Commit, Image-/ELF-Hash, App-only
`0x10000`, Rueckfall, Stoppbedingungen); dieser Auftrag fuehrt keine aus.

| Schnitt | Inhalt | Branch | Gate |
|---|---|---|---|
| **S0** | Diagnoseimage: `bb4f43a` + D1-Hook-Delta (v3-Technik); D2/D3-Host-Werkzeuge (Mitschnitt, gehaerteter Capture/Treiber) mit Hosttests (`diag/test_*.py`) | Diagnosebranch, **nie Merge** | Owner-Freigabe Diagnoseimage; Statische Pruefung (0 Flash-Referenzen, Stack/Zeit) am neuen ELF |
| **S1** | Stufe-D-Hardwarelauf (G1b) | – | eigenes Owner-Gate G1b |
| **S2** | Bericht Stufe D (BLOCKER/FOLLOW-UP/NO-ACTION, Hypothesenmatrix) und Entscheidungsregel Abschnitt 4 | Review-/Nicht-Merge-Branch | unabhaengiger Ergebnisreview; Owner entscheidet Stufe E |
| **S3** | Nur bei Freigabe: `sdkconfig.defaults` um `CONFIG_LWIP_TCP_SND_BUF_DEFAULT=2880` (produktiver Branch #203); Build beider ESP-IDF-Profile, Konfigurationsvertrags-/Architekturpruefung | `agent/issue-27-r1-web-dashboard-plan` | Planfreigabe + Owner-Gate Stufe E |
| **S4** | A/B-Hardwarearme (Abschnitt 5), P-A 3 ×, P-A2, P-B, P-R | – | eigenes Owner-Gate je Arm; danach G2 gemaess Plan F1/F4 |

## 7. Offene Entscheidungen (Owner)

| ID | Entscheidung | Empfehlung |
|---|---|---|
| F2-D1 | Freigabe dieses Plans (exakte SHA) | Ja |
| F2-D2 | Stufe D (D1 + D2 + D3, nicht gleichzeitig mit Zusatzmassnahmen) als verbindliches Gate vor jeder Konfigurationsaenderung | Ja; `HEAP_TASK_TRACKING` zurueckgestellt |
| F2-D3 | O1 als einziger Stufe-E-Kandidat; O2 nur nach negativem O1-Ergebnis; O3 nur nach ausdruecklichem Ownerentscheid | Ja |
| F2-D4 | Latenz-/Durchsatztoleranz von B gegenueber A (Abschnitt 5, Vorschlag 1,25×) | Wert vor dem ersten Flash festlegen |
| F2-D5 | Umgang mit der Restgefahr/Statistik (Abschnitt 5, Aussagegrenzen) | Abnahme nur mit ausdruecklicher Ownerakzeptanz |
| F2-D6 | Hostprivileg fuer Header-Mitschnitt (D2) und Aufbewahrung der privaten Pcap-Dateien (nicht veroeffentlichen) | Ja, nur abgeleitete Zaehler publizieren |

## 8. Dokumentationswirkung und Risiken

- Nach Planfreigabe: `docs/ROADMAP.md` (RES-01-Zeile) um Verweis auf diesen Plan und
  Status ergaenzen; Plan F1/F4 Abschnitt 2 (1626-B-Zeile) um die Praezisierung aus
  Befund a korrigieren. Beides gehoert nicht zu diesem Plan-only-Commit.
- Risiken: (1) O1 verlaengert Shell-Dauer/Latenztail (Abschnitt 3); (2) die
  Stichprobe n ≤ 3 bleibt statistisch schwach (Abschnitt 5); (3) D1 laeuft auf dem
  Stack der fehlschlagenden Task (+ ≈ 112 B, Abbruchkriterium Abschnitt 4);
  (4) Funkbedingungen sind nicht instrumentiert; (5) das Ergebnis kann zeigen, dass F2
  die falsche Eskalation ist (dann Rueckfall in den Plan, kein stilles Weiterbauen).
- Safety: Alle Laeufe aktorfrei auf dem Dev-Board (`real actuators: disabled`);
  keine Aktorfreigabe wird vorausgesetzt oder eingefuehrt.
