# Issue #27 / PR #203 – RES-01: Heap-Erschoepfung der Web-Shell-Auslieferung beheben (F1 + F4) (Plan)

Status: `PLAN_REVISION_R1_ABNAHME_PENDING_OWNER_APPROVAL` (Planrevision vom
2026-10-15 auf Ownerentscheid; ersetzt keine historischen Ergebnisse). Diese Datei ist
die vollstaendige, eigenstaendige Fassung; fuer die Umsetzung ist nur sie massgeblich.

**Ownerentscheid zum Abnahmescope (2026-10-15):** Die Weboberflaeche dient zum
Einstellen und gelegentlichen Kontrollieren; danach arbeitet der Fermenter weitgehend
standalone. Vier gleichzeitig intensiv pollende Sessions mit vielen neuen
TCP-Verbindungen sind **keine** R1-Pflichtanforderung. P-A/P-A2 und vier reale Browser
sind **diagnostische Stresstests**, nicht zwingende R1-Abnahme. Die R1-Abnahme
beschreibt Abschnitt 7.2 (ein realer Browser mit dem vorhandenen 5-s-Polling).

**Bestaetigte Stuetzpunkte (unveraendert gueltig):**
- Plan `6222efce976dc3bea3cf397c2dca56a5fb21859d` ownerfreigegeben (2026-10-13, D1–D5 = JA
  fuer die damalige Fassung), C0 = GO nach unabhaengigem R3-Ergebnisreview (2026-10-14,
  [R3-Evidence](../audits/PR203_G0_R3_20261014_RESULT.md), [akzeptierte R2-Referenz](../audits/PR203_G0_R2_20261014_RESULT.md)).
- F1 (statischer Shell-Anhang per `httpd_resp_send_chunk`) und der gemessene Heapgewinn
  bleiben erhalten: medianes Build-Phasendelta −7 104 B, bessere minimale/mediane
  Heap-/Blockreserven (G0-R3).
- Software-C1–C3 sind umgesetzt (Produktstand `d35ee85`, [Softwarestand](../audits/PR203_RES01_SOFTWARE_20261014.md));
  F4-Schwelle ownerfreigegeben **vorlaeufig** (Skip 13 312 B / Resume 16 640 B,
  `PROVISIONAL_PENDING_G1`); sie wird nach Abschnitt 4/2 und 7.5 nur in den relevanten
  Pfaden qualifiziert.

**Historische Ergebnisse (unveraendert, keine nachtraegliche Umwertung):**
- G1 IMAGE01 (P-A, F4-/Diagnose-Overlay): `FAIL/HOLD` (`heap_alloc_failed`, 311,9 s).
- G1b/F2-Diagnose: R1 `HOLD`, R1b `FAIL/HOLD` (1 696-B-WLAN-RX-Allokation, dekodiert),
  Stufe E: B1/B2 `FAIL/HOLD`, OBS1R `FAIL/HOLD` (Klasse O-B), NETPROBE1 vom Owner nach 12 von
  20 Bursts abgebrochen (Phase 2 nicht ausgefuehrt; 15 von 36 ICMP-Antworten, 4 Bursts ohne
  Antwort, 3 von 12 Bursts wegen Neighbor-/ARP-Zustand `INDETERMINATE`, davon 2 ohne Antwort;
  Teilergebnis, kein bewiesener Fehler in ESP-IDF/lwIP oder Funk). Details: Review-Branch `agent/pr203-res01-g1-review-nonmerge`.
- Die dabei beobachteten **WLAN-/TCP-Stillstaende** (verzoegerte Quittungen/SYN-ACKs,
  teils auch im Leerlauf, bei gesundem Heap) bleiben als **offene historische Evidence**
  erhalten; ihre Ursache ist **nicht** belegt (AP, Funkweg, WLAN-Treiber, lwIP offen).
- **Neue Funkbeobachtung (Owner, UniFi-Controller):** am ESP32-Client zunaechst **−86 dBm**, danach
  **−69 dBm**. Messzeitpunkt, AP-Zuordnung und Messrichtung sind nur soweit gesichert, wie vorhandene
  Controller-Daten sie belegen; der bessere Wert ist **kein Kausalnachweis** und keine Garantie
  langfristiger WLAN-Stabilitaet. Es wird keine feste RSSI-Grenze festgelegt.
- Der nicht qualifizierte F2-Kandidat O1 (`CONFIG_LWIP_TCP_SND_BUF_DEFAULT=2880`) wird
  kontrolliert zurueckgebaut (Abschnitt 5 C5; [F2-Plan](issue-27-res01-f2-escalation-plan.md)).

Basis: PR #203, Diagnose-Evidence D3 (`hw-evidence-pr203/diag/run/`).
`RES-01=BLOCKER` (Ownerentscheid), `PR203_FINAL_GO=HOLD`, `ACTUATOR_RELEASE=NO`,
`G1_G2=NOT_AUTHORIZED`. Es gibt keine Hardwarefreigabe aus diesem Plan.

## 0. Revisionsuebersicht der Akzeptanzkriterien (transparent)

| Gegenstand | Bisher (Plan `6222efc`, Abschnitt 7/9) | Neu (diese Revision) |
|---|---|---|
| P-A 3 × 20 min (4 Sessions, neue Verbindung je Request) | Pflicht-Abnahme | **diagnostischer Stresstest**, nicht Pflicht; historische `FAIL`-Laeufe bleiben `FAIL` |
| P-A2, P-B (4 reale Browser) | Pflicht-Abnahme | **diagnostisch**; Ersatz fuer die Abnahme: R1-U (ein Browser) |
| Erholung P-R | nach jedem P-A | **U4/U5** im R1-Nutzungsprofil (Heap-Erholung nach Schliessen des Browsers) |
| Abnahmekriterien 1–6 | ueber alle Stresslaeufe | gelten **unveraendert in der Sache** fuer **R1-U**; reale Fehler/Resets sind weiter Blocker |
| G1 (Diagnose-Overlay) | Messung unter P-A-Last | **G1'**: F4-Qualifikation in den relevanten UI-/Touch-/Hold-Pfaden mit R1-Nutzung, keine Stresslast |
| G2 (reines Produktimage) | P-A/P-A2/P-B-Bestaetigung | **R1-U-Abnahme** auf dem reinen Produktimage |
| F2 (O1, Sendepuffer 2880) | Eskalation, Kandidat | nicht qualifiziert; **Rueckbau auf 5760** (C5); keine weitere Netzwerkdiagnostik in diesem PR |
| Restgefahrakzeptanz F4 (D3) | akzeptiert | unveraendert; kein allgemeiner OOM-Schutz behauptet |

## 1. Ziel und Nicht-Ziele

Ziel: Die in D3 nachgewiesene Kette "ueberlappende Web-Antworten erschoepfen den
Heap -> WLAN-RX-/TX-Puffer scheitern -> anhaltende Erschoepfung -> `abort()` im
UI-Rendering" mit dem kleinsten wirksamen Eingriff entschaerfen und die
Fehlerfreiheit unter den definierten Lastprofilen nachweisen:

- **F1:** Spitzenheapbedarf der Shell-Auslieferung nachhaltig senken.
- **F4:** Begrenzte Risikominderung des belegten OOM-Absturzpfads im UI-Rendering:
  Erkennt das Gate Speichermangel, entfaellt der UI-Schritt und der Main-Loop
  laeuft weiter. Nach bestandenem Gate bleibt ein OOM mit `abort()`/Reset moeglich;
  keine allgemeine OOM-Sicherheitszusage. Diese Restgefahr benoetigt ausdrueckliche
  Ownerakzeptanz (D3); Regelung/Safety bleiben architektonisch UI-unabhaengig.
- **F2** (TCP-Sendepuffer / lwIP-Konfiguration) ist **nicht** Teil der R1-Abnahme;
  der erprobte, nicht qualifizierte Kandidat O1 wird zurueckgebaut (Abschnitt 8).

Nicht-Ziele: Nachweis der Robustheit gegenueber mehreren gleichzeitig intensiv pollenden
Sessions oder vielen neuen TCP-Verbindungen (kein R1-Pflichtprofil, Abschnitt 7.3); Aenderung von Authentifizierung, CSRF, Session, Replay, Revisionsschutz,
Web-/JSON-Vertraegen oder Polling; Sessionlimit; WLAN-/lwIP-/sdkconfig-Werte (ausser
F2-Eskalation mit eigener Freigabe); Aktorfreigabe; Merge von Diagnosecode; weitere
Funktionen. Diagnosecode bleibt vom Produktcode getrennt (Abschnitt 6).

## 2. Verifizierte Ausgangslage (Evidence, keine Annahmen)

Quellen: `diag/RES01_ROOT_CAUSE_RESULT.md`, `diag/run/uart_D*.log`, ELF `3ab98630…`.

| Befund | Beleg |
|---|---|
| Fehlerstelle der 1696-B-Fehler: `wDev_ProcessRxSucData -> wDev_IndicateFrame -> esf_buf_alloc -> esf_buf_alloc_dynamic -> wifi_malloc -> malloc` (WLAN-RX, Task-Kontext, Core 0); 1626 B `caps=0x80c` ist der DMA-TX-Puffer des WLAN-Treibers | decodierter Hook-Backtrace D3, ESP-IDF `overview.rst` |
| Heap-Zustand beim Fehler: `pre` 22 696 B, nach Shell-Erzeugung 13 988 B, nach `httpd_resp_send` 7 656 B (groesster Block 1 728 B); danach 2 700 B frei fuer > 55 s | `res01_http`-Zeilen, `diag_30s` bei 243/273 s |
| Folge im Anwendungscode: `abort()` im Main-Task `updateProductUi -> ProductiveLvglRenderer::render -> makeRepresentativeScreen -> addFill -> std::vector<ScreenDrawCommand>::push_back -> operator new -> __cxa_allocate_exception` | decodierter Abort-Backtrace; `CONFIG_COMPILER_CXX_EXCEPTIONS` ist **aus**, jede fehlschlagende `operator new` bricht ab |
| Heap-Kosten eines Shell-Abrufs (Median aus 33/26 Abrufen): Shell-Erzeugung (`std::string` 8 048 B + Overhead) **−8,7 KB**, `httpd_resp_send` weitere **−6,9 KB** (lwIP-/WLAN-Sendepuffer bis zur ACK) | `res01_http` `pre/built/sent` |
| **Korrektur frueherer Aussage:** Der String wird nur **waehrend des Handlers** gehalten (er ist nach der Rueckkehr von `handleRequest` freigegeben, der HTTP-Server ist ein Task, `max_uri_handlers=1`); der Netzanteil (6,9 KB) wird bis zur ACK gehalten und skaliert mit der Zahl gleichzeitiger Verbindungen. Die in `RES01_ROOT_CAUSE_RESULT.md` genannte Ersparnis "−56 % je gleichzeitiger Antwort" gilt daher nur fuer die **Spitze (einmal 8,7 KB)**, nicht je Verbindung | `sent`-Zeile liegt vor der Rueckgabe des Handlers; der naechste `pre` (1 s spaeter) zeigt wieder 41 KB |
| Ohne Shell-Abrufe (4 Sessions, nur API, D1): kein Fehler, `min_free` 21 156 B; mit einem Shell-Abruf im Idle: `min_free` 18 360 B, kein Fehler | D1, Boot-Test |
| Grobe Spitzenbilanz bei 4 Sessions: Idle 42,3 KB − API-Anteil (≈ 21 KB, D1-Tiefststand seit Boot, grobe obere Schaetzung) − Shell (8,7 + 6,9 KB) ≈ 5–7 KB Reserve; beobachtet 6,6–7,9 KB (D2/D3) | Rechnung aus gemessenen Werten |
| `esp_http_server`: `max_open_sockets=7` = **7 Client-Sockets** (die 3 internen Sockets kommen hinzu; `httpd_start` prueft `max_open_sockets + 3 <= CONFIG_LWIP_MAX_SOCKETS=10`), `backlog_conn=5`, `lru_purge_enable=false`, `recv_wait_timeout=send_wait_timeout=5 s`; kein `TCP_NODELAY`; `httpd_resp_send` sendet Statuszeile und jedes Headerfeld in eigenen `send()`-Aufrufen | IDF-Quelle `esp_http_server.h`, `httpd_main.c:524-527`, `httpd_txrx.c`; Adapter nutzt `HTTPD_DEFAULT_CONFIG` mit `max_uri_handlers=1` |
| Unser Lasttreiber oeffnete je Request eine neue Verbindung (bis zu 12 gleichzeitig = genau 7 Sockets + 5 Backlog); die 1–3-s-Latenzen passen zu TCP-Retransmits (Hypothese, nicht gemessen). Das Verbindungsmodell der Clients (neue Verbindung je Request vs. Keep-Alive des Browsers) beeinflusst Heap und Latenz; deshalb misst P-B mit echten Browsern | `load2.py`; Latenzverteilung |
| HTTPD-Stack: Hochwassermarke min. 2 336 B frei von 8 192 B; Main-Task-Stack 11 416 B frei | `res01_http`, `diag_30s` |
| Aktuelle Antwortstruktur: `webShell()` baut den ganzen Body in einem `std::string` (`reserve(8192)`), `handleShell` uebergibt ihn per `HttpResponse::body`, der Adapter sendet ihn mit `httpd_resp_send(body.data(), size)`. Dynamisch sind nur CSRF-Token, `const A`, `const U`, `let Q` (≈ 120 B); ≈ 1 300 B davor (CSS/HTML) und ≈ 6 700 B danach (Skript) sind statisch | `web_application_routes.cpp`, `esp_idf_http_server_lifecycle.cpp` |
| `HttpResponse` hat Nutzer in `fermentation_app` (`web_application_routes`, `network_setup_routes`), Adapter und Tests (≈ 96 Stellen in `test_web_application_routes`, 16 in `test_network_configuration`, 6 in `test_web_session`, 2 im Adapter-Hosttest) | `grep` |

## 3. F1 – Optionen (einfachste zuerst) und Entscheidung

Bewertungsgrundlage: Wirkung auf den Spitzenheapbedarf (Ist ≈ 8,7 KB transient +
6,9 KB bis ACK je Shell-Antwort), Eingriffstiefe, Vertragswirkung.

| Option | Idee / vorhandene IDF-Funktion | Wirkung auf die Spitze | Eingriff | Bewertung |
|---|---|---|---|---|
| **F1-0** | Nur Anpassung ohne Vertragsaenderung: String exakt dimensionieren, Reserve entfernen, weiter kuerzen | < 0,2 KB (`reserve(8192)` vs 8 048 B Body) | klein | **Unwirksam**; Shell ist bereits auf die Schranke gekuerzt (Plan a2c04b0 D1) |
| **F1-1** | Chunk-Senden mit vorhandener `httpd_resp_send_chunk` aus dem **bestehenden** `std::string`-Body | 0 fuer den String, ggf. kleinere Segmentbildung | klein | **Unwirksam** fuer die Spitze: der Body bleibt voll im Heap |
| **F1-2** | Statischen Skriptteil in eine eigene, cachebare Route auslagern | Shell-Antwort ≈ 1,5 KB statt 8 KB; Asset nur beim ersten Laden | **neue Route + Cache-Header-Feld im Antwortvertrag**; Browser laedt das Asset erneut bei jeder Cache-Invalidierung (volle 15 KB dann) | Wirkung gross, aber Webvertrag erweitert (neue oeffentliche Route, Cache-Semantik, CSP/Offline-Verhalten) – **nicht** "Web-Vertraege unveraendert" |
| **F1-3** | gzip der statischen Teile (`Content-Encoding: gzip`) | ≈ −50…60 % | Build-Schritt + Header-Feld im Antwortvertrag | Zusatzwerkzeug und Header; dynamische Teile muessen getrennt bleiben; **nicht minimal** |
| **F1-4 (empfohlen)** | **Nicht besitzender statischer Antwortteil**: `HttpResponse` erhaelt ein optionales `std::string_view`-Feld fuer einen konstanten, nicht im Heap liegenden Anhang; Adapter sendet `body` und Anhang mit der vorhandenen **`httpd_resp_send_chunk`** (Chunked, Terminator mit 0 Byte) | Spitze −(Anhang ≈ 6,65 KB, mit dem gemessenen Overhead-Faktor 1,08) = **≈ −7,2 KB** (String bleibt ≈ 1,4–1,5 KB); Netzanteil unveraendert | 1 neues Feld im Portvertrag (additiv, Standard leer), Adapter-Zweig, `webShell` liefert Kopf + konstanten Anhang | **kleinster wirksamer Schnitt ohne Aenderung der Web-Inhalte**: der dechunkte Body ist byteweise identisch (Header und Chunk-Framing aendern sich) |
| F1-5 | Voll gestreamte Erzeugung (Callback-Port) | wie F1-4 plus String ≈ 0 | grosser neuer Portvertrag, Zustand im Adapter | zu gross fuer den Nutzen (≈ 1,4 KB mehr) |

**Entscheidung (Empfehlung): F1-4.** Begruendung: einzige Option, die den
dominierenden transienten Verbraucher (Shell-String) um ≈ 80 % senkt, ohne einen
Web-Vertrag (Routen, Inhalt, Auth, Session, CSRF) zu aendern. Der **dechunkte
Body** der Shell bleibt byteweise identisch; geaendert wird das Transferformat
(von `Content-Length` auf `Transfer-Encoding: chunked`, HTTP/1.1, von Browsern
unterstuetzt) und damit Antwortheader und Framing auf der Leitung. Die
Groessenpruefung in `handleShell` (503 bei Ueberschreitung der Schranke) gilt
fuer **Body plus Anhang**.

**Antwortinvariante (alle Routen/Konsumenten):** JSON- und Fehlerantworten haben
immer einen leeren statischen Anhang. Jeder Helfer, der eine Antwort ersetzt
(insbesondere `setJsonResponse` und `setResponse`), loescht den optionalen Anhang
neben den bisherigen Feldern; nur die erfolgreiche Shell-Erzeugung setzt ihn.
Das gilt auch dann, wenn `handleShell()` zunaechst Kopf und Anhang aufbaut und
erst danach wegen Gesamtlaenge > 8 192 B auf 503 umschaltet: vollstaendiger Body
`{"error":"web-shell-unavailable"}`, Status 503, JSON-Content-Type, keine
HTML-/JS-Suffixbytes. Die bestehende Metadatenpolitik bleibt erhalten.

Erwartete Wirkung (grobe Rechnung aus Messwerten, **zu verifizieren**):
Spitzenreserve bei 4 Sessions ≈ 42,3 − (21,1 + 6,9 + 1,5) ≈ **12,8 KB** statt 5–7 KB
(≈ +7,2 KB). Der Wert 21,1 KB ist der Tiefststand seit dem Boot im D1-Lauf (er
sank dort ueber 4 min von 29,3 auf 21,2 KB und enthaelt 4 KDF-Logins), also nur eine
grobe obere Schaetzung des API-Anteils, keine Konstante. Die Retained-Anteile
(API-Burst, Sendepuffer) aendern sich nicht; die Wirkung ist ein zusaetzlicher
Abstand zur Erschoepfung, kein Beweis der Immunitaet. Ob 14 KB reichen, entscheidet die
Hardwareverifikation (Abschnitt 7), nicht diese Rechnung.

Risiken F1-4 (offen, im Plan zu pruefen): (a) Chunked-Segmentierung kann mehr
kleine WLAN-TX-Puffer (je 1,6 KB) erzeugen als ein `Content-Length`-Senden
(`httpd_resp_send_chunk` sendet Chunk-Laenge, Daten und CRLF in getrennten
`send()`-Aufrufen); deshalb **Spike C0** (unten) vor dem Produktvertrag. (b) Der
Anhang muss dauerhaft im Flash/DRAM liegen (`constexpr`-Literal, kein Heap, nicht
im Cache-gesperrten Kontext relevant). (c) Alle Tests, die `response.body` der
Shell vergleichen, brauchen den zusammengesetzten Body (Testhelfer); die
Groessenpruefung (503 `web-shell-unavailable`) deckt Body plus Anhang.

## 4. F4 – Begrenzte OOM-Risikominderung des UI-Renderings

Befund: Weder `makeRepresentativeScreen` (reserviert nur fuer die Netzwerkseite),
noch `ProductiveLvglRenderer::render`, noch die Touch-Pfade (`processWorkspaceTouch`
baut denselben Screen fuer die Zielfindung) sind gegen Heapmangel geschuetzt;
`operator new` bricht ab (Exceptions aus). Der Absturz ist der einzige beobachtete
**fatale** Effekt der Heap-Erschoepfung.

Entwurf (kleinster wirksamer Schnitt):

1. **Speicher-Gate** in der Composition (`main/`, ESP-IDF erlaubt): Eine kleine
   Klasse `UiMemoryGate` mit eingespeistem Messpunkt (`largestFreeBlock()` als
   `std::function`/Schnittstelle, in Produktion `heap_caps_get_largest_free_block(
   MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT)`, im Test ein Fake). `updateProductUi`
   fragt es **zu Beginn** und ueberspringt bei Unterschreitung den gesamten UI-Schritt
   (Snapshot, Touch-Zielfindung, Render) fuer diese Schleifeniteration; Anzeige und
   Zustand bleiben unveraendert, `application.update()` und der Rest des
   Main-Loops laufen weiter. Hysterese: Wiederaufnahme erst bei `Schwelle + Marge`,
   damit kein Flattern. Ratenbegrenzte Protokollzeile (`ui_skipped_low_memory`,
   Zaehler, hoechstens 1 pro 10 s) und ein Zaehler fuer die Evidence.
2. **Schwelle nicht raten:** `kUiMinimumLargestBlockBytes` wird als empirische
   Risikominderung aus dem beobachteten Bedarf des UI-Schritts abgeleitet:
   (a) obere Schranke fuer den reservierten Vektorblock = `kMaximumDrawCommands ×
   sizeof(ScreenDrawCommand)` mit einem nativen Test, der ueber alle Seiten die
   maximale Befehlsanzahl prueft; (b) Messung des Heapbedarfs je Render/Touch-Screen
   auf Hardware mit dem Diagnose-Overlay (Heap vor/nach). Schwelle = hoeherer Wert aus (a)
   und (b) mit Faktor 2 (dokumentiert). Erwartung (Groessenordnung, **nicht** Wert):
   einige KB bis niedrige zweistellige KB. Diese Vektorschranke begrenzt weder
   andere Einzelallokationen noch den kumulativen Bedarf von Vektor, Strings und
   LVGL. Heap-vor/nach-Messungen koennen transiente Spitzen verfehlen; Schwelle
   und Faktor 2 sind keine Garantie fuer den gesamten Render-/Touch-Pfad.
   **Ownerfreigegebenes Korrekturdelta vom 2026-10-14 (Option 1):** Die
   Render-/Touch-Messung (b) liegt vor Software-C1–C3 nicht vor; G0 hat
   Shell-Phasen gemessen. Die ausdrücklich vorläufige Softwareanbindung
   nutzt zunächst (a) × 2, Status **`PROVISIONAL_PENDING_G1`**. Der geprüfte
   strukturelle ESP32-Vektorblock ist 104 × 64 = 6656 B; vorläufige
   Skip-Schwelle **13312 B**, Resume ab **16640 B** (Schwelle + halber
   Vektorblock, 25%). Die Werte sind aus der jeweiligen ABI abgeleitet,
   nicht als gesamte Renderpfad-Garantie zu verstehen.
   Nach separat ownerfreigegebener G1-Messung wird die oben festgelegte
   Formel max(a, b) × 2 abschließend bestimmt. Die Diagnose muss auch
   Render-/Touch-Pfade erfassen, die das vorläufige Gate unter Speicherlast
   überspringt: allokationsfreie Skip-/Pfad-Coverage im Overlay; ein Skip
   ist keine Null-Bedarfsmessung. Die betreffenden Pfade werden gezielt
   bei ausreichender Reserve mit repräsentativen Seiten-/Zustands-/Text-/
   Kontaktfällen ausgeführt und tatsächlich gemessen, etwa in separaten
   kontrollierten Referenz-/Recoveryphasen des G1-Testblatts. Fehlende
   Coverage hält die Schwellenqualifikation HOLD. Kein stilles Umgehen
   des Produktgates unter OOM-Last, keine Diagnoseänderung im Produkt.
   Diese begrenzte Softwarefreigabe erlaubt keine Hardwarehandlungen
   und keine finale RES-01-/PR-Freigabe vor G1/G2.
3. **Reserve statt Wachstum:** `makeRepresentativeScreen` reserviert die geprueft
   begrenzte Befehlsanzahl **einmal** fuer alle Seiten (Test erzwingt, dass keine Seite
   die Schranke ueberschreitet), damit Vektorwachstum (Verdopplung, zwei Puffer
   gleichzeitig) im Normalbetrieb entfaellt. Das Gate beobachtet einen geeigneten
   freien Block nur zum Messzeitpunkt, reserviert ihn aber nicht; auch `reserve`
   kann bei konkurrierendem Heapverbrauch weiterhin abbrechen.
4. **Skip-Semantik (Werksreset-Hold, fail-closed):** `updateProductUi` stellt die
   Loslass-Meldung des Werksreset-Holds zu (`releaseFactoryResetHold`,
   `fermentation_ui_press_dispatcher.cpp`). Ein uebersprungener UI-Schritt wird fuer einen
   armierten Hold wie eine **Kontaktunterbrechung** behandelt: bei jedem
   uebersprungenen Schritt wird `releaseFactoryResetHold(application, now)` aufgerufen
   (allokationsfrei, vorhandene Funktion), bevor der Schritt entfaellt.
   **Allein genuegt dies nicht:** `FactoryResetFlow::updateHold(false)` verwirft
   nur den Zeitbeginn; ein spaeteres `true` startet trotz unveraendert gehaltenem
   Finger erneut. Deshalb eine minimale allokationsfreie Sperre im bestehenden
   Touch-/Hold-Adapter unter `main/` vorsehen: nach Skip zunaechst auf beobachtetes
   physisches Loslassen warten, danach auf einen neuen Kontakt; bis dahin darf
   kein `updateFactoryResetHold(true, ...)` das Hold-Ziel aktivieren. Der
   bestehende Flow bleibt alleiniger Owner von Stufen, 5-s-Timer und Reset.
   Ein kleiner lokaler Zustand (warte auf Loslassen / neuen Kontakt / freigegeben)
   genuegt; erneuter Skip setzt die Sperre wieder auf "warte auf Loslassen".
   Die Sperre betrifft nur den Hold-Pfad, nicht andere UI-Kommandos.
   Loslassen ist nur durch einen frischen, nach dem letzten Skip tatsaechlich
   beobachteten `RawTouchSampleStatus::NoContact` aus dem vorhandenen Sampling-Pfad
   belegt; ein alter publizierter No-Contact-Zustand reicht nicht.
   Ein Default-`TouchPollResult` bei Lock-Timeout, Controllerfehler, fehlender
   Kalibrierung oder ein nicht beobachteter Kontaktwechsel waehrend des Skips
   ist kein Nachweis. Die lokale Touch-Publikation/`pollTouch` muss diese Evidenz
   allokationsfrei unterscheidbar liefern; keine zweite Hardware-Abtastung.
   Erst ein nach diesem Loslassen beobachteter neuer gueltiger Kontakt darf den
   vorhandenen Hold-Timer wieder starten (keine vor dem Skip vorgemerkte Presskante).
   Native Regression durch die echte gemeinsame Schleifen-/Touch-Entscheidung:
   Hold beginnen, Skip bei weiter gehaltenem Finger, nach Wiederaufnahme > 5 s
   weiterhin auf dem Ziel halten -> **kein Reset**, keine Epochenaenderung und
   kein Netzwerk-/HTTP-Stopp. Erst beobachtetes Loslassen + neuer Kontakt:
   < 5 s kein Reset, bei 5 s ununterbrochen auf dem Hold-Ziel genau ein Reset.
   Fehlende/fehlerhafte Samples loesen die Sperre nicht. Fuer Variante A und B
   pruefen; bisherige `SIM-19-R08`-Regeln (Loslassen, Verschieben, Abbruch,
   kurzer Neukontakt) bleiben erhalten.
   Das Gate liegt **vor** `renderGate.beginStep`; ein uebersprungener Schritt veraendert weder
   den Render-Schluessel noch die Praesentationskopie (kein Eviction-/Network-Page-Sampling-Nebeneffekt);
   nach der Wiederaufnahme zeichnet `renderRequired` aus dem aktuellen Snapshot neu, falls sich
   etwas geaendert hat (Test).
5. **Sicherheitsbetrachtung (Pflicht-Abschnitt in der Umsetzung):** UI und Touch sind
   keine Safety-Funktion (AGENTS.md: Regelung/Safety brauchen Netzwerk, Web, Anzeige
   nicht). Ein uebersprungener Schritt bedeutet bei Speichermangel: Anzeige friert ein,
   Touch ist voruebergehend ohne Wirkung, danach automatische Wiederaufnahme. Das
   vermeidet den UI-bedingten Abort in den vom Gate ausgelassenen Schritten,
   garantiert aber keinen unterbrechungsfreien Geraetebetrieb nach bestandenem Gate. Der
   Gate-Messaufruf allokiert nicht (`heap_caps_get_largest_free_block` ist
   allokationsfrei, haelt kurz das Heap-Lock). Die Gegenmassnahme gegen dauerhaft
   eingefrorene Anzeige ist F1 (Reserve), nicht F4.
6. **Vertragsentscheidung: begrenzte Risikominderung mit Ownerakzeptanz (D3).**
   Das Gate **senkt** das Abort-Risiko, **schliesst es nicht aus**:
   zwischen Pruefung und Allokation koennen WLAN- oder httpd-Task Heap verbrauchen (TOCTOU),
   und der Render-Schritt enthaelt weitere Allokationen (Strings/LVGL).
   Realistisches Hardware-Fehlerszenario: Gate passiert bei ausreichendem groessten
   Block, anschliessend bindet ein WLAN-/HTTP-Burst Speicher vor `reserve` oder
   waehrend weiterer String-/LVGL-Allokationen; `operator new` scheitert bei
   ausgeschalteten Exceptions -> `abort()`/unerwarteter Geraetereset. Boot und
   Recovery bleiben fail-closed; D3 muss dieses Restrisiko ausdruecklich akzeptieren,
   weder Aktorfreigabe noch ein beobachteter Reset werden dadurch akzeptiert.
   Native Tests beweisen Gate-/Sperrlogik und Vektorschranke, nicht konkurrierende
   Heapverfuegbarkeit oder Renderer-Absturzfreiheit. Fehlerfreie Hardwarelaeufe
   belegen nur die getesteten Profile, Zeitraeume und Images (Abschnitt 7).
   Alternative **F4-B** waere eine nachweisbar absturzfreie Allokations-/Fallback-
   Strategie fuer den belegten Pfad, etwa wiederverwendete/feste Puffer; ein
   vollstaendiger allokationsfreier UI-Umbau ist keine automatische Pflicht.
   F4-B ist hier **nicht** geplant. Ohne Ownerakzeptanz von D3: HOLD und
   neu freizugebende Planrevision statt Umsetzung mit OOM-Sicherheitszusage.
7. Nicht Teil von F4: andere Heapnutzer im Main-Task (z. B. `application.update()`); sie
   sind nicht als Absturzpfad belegt.

Tests (nativ): Gate-Logik mit Fake (Schwelle, Hysterese, Ratenbegrenzung, Wiederaufnahme),
Befehls-Obergrenze ueber alle Seiten, `updateProductUi`-Verzweigung (soweit nativ
testbar; sonst Hardwareverifikation), keine Verhaltensaenderung bei ausreichendem
Speicher (bestehende UI-Tests bleiben unveraendert gruen).

## 5. Umsetzungsschnitte (eine Planfreigabe vor C5 und der Hardwareabnahme; Hardware-Gates getrennt)

- **C0 – Spike (nur Diagnosebranch, kein Produktcode, eigenes Hardware-Gate G0):**
  Auf `agent/pr203-res01-diag-local` (Basis v3) die Chunk-Variante von F1-4 als
  Wegwerfpatch messen: Spitzenheap und Segmentierung (`res01_http`, `diag_30s`)
  unter dem P-A-Profil (Abschnitt 7). Go/No-Go: Spitzenbedarf sinkt messbar
  (erwartet ≈ −6,6 bis −8,7 KB); die Entscheidung folgt den messbaren
  C0-Kriterien in Abschnitt 7. Bei No-Go
  zurueck zum Plan (F1-2/F1-3/F2 neu bewerten).
- **C1 – F4:** `UiMemoryGate`, Anbindung in `updateProductUi`, Reserve in
  `makeRepresentativeScreen`, native Tests.
- **C2 – F1-4:** `HttpResponse`-Feld (additiv), Adapter-Zweig (`httpd_resp_send_chunk`),
  `webShell`/`provisioningShell` mit konstantem Anhang, Testhelfer fuer den
  zusammengesetzten Body, Anpassung der Tests (byteweise Gleichheit des
  zusammengesetzten Bodys zur bisherigen Shell, 8 192-B-Schranke weiter auf die
  Gesamtlaenge, Provisioning-Shell unveraendert in den Bytes). Antworthelfer und
  direkte Antwort-Ersetzungen aller Konsumenten auf die Anhangsinvariante aus
  Abschnitt 3 pruefen/anpassen. Native Regressionen: vorbelegter Shell-Anhang ->
  JSON-/Fehlerantwort sowie tatsaechlicher `handleShell`-Wechsel bei Gesamtlaenge
  > 8 192 B -> vollstaendiger 503-Body, Status, Content-Type und leerer Anhang
  (eng begrenzte native Fehlerinjektion der Shell-Laenge, keine Aenderung der
  produktiven 8 192-B-Schranke).
  Adapter-Hosttests pruefen die gesendeten Bytes desselben 503 ohne HTML-/JS-Suffix;
  im Erfolgszweig Kopf + Anhang in korrekter Reihenfolge und genau einen
  0-Byte-Terminator nach den Daten. Sendefehler in Kopf, Anhang und Terminator
  injizieren: erster Fehler wird propagiert, danach keine weiteren Sendeschritte
  und kein faelschlicher Erfolgsabschluss. Antworten ohne Anhang behalten den
  bisherigen Sendezweig.
- **C3 – Dokumentation:** `WEB_UI.md` (Antwortvertrag "statischer Anhang", Ressourcen-
  vertrag), `ACCEPTANCE_TESTS.md` (neue IDs, Abschnitt 7), `ROADMAP.md`, Evidence-Verweise.
- **C4 – Hardwareverifikation (Abschnitt 7):** G1' (F4-Qualifikation, Diagnose-Overlay)
  und G2 (R1-U-Abnahme auf dem reinen Produktimage); Evidence als PR-Kommentar und
  Rohartefakte, kein Produktcode. P-A/P-A2/P-B sind **kein** Teil von C4.
- **C5 – O1-Rueckbau (Produktcommit, nur Konfiguration):** den Block
  `CONFIG_LWIP_TCP_SND_BUF_DEFAULT=2880` samt Kommentar aus `sdkconfig.defaults` entfernen
  (Rueckkehr zum ESP-IDF-Default **5760**, dem Wert vor O1). Gezielte Tests: Build **beider**
  ESP-IDF-Profile ueber `scripts/build_esp_idf_profiles.py all` mit Profil-/Herkunftspruefung;
  aufgeloeste Einstellung `CONFIG_LWIP_TCP_SND_BUF_DEFAULT=5760` (und Alias
  `CONFIG_TCP_SND_BUF_DEFAULT=5760`) in beiden `build/<profil>/sdkconfig` und
  `config/sdkconfig.h`; Vergleich der aufgeloesten Konfiguration gegen den Stand vor O1
  (Unterschied ausschliesslich diese zwei Zeilen); vorhandene generierte
  `build/<profil>/sdkconfig` haben Vorrang vor den Defaults und werden vor dem Build entfernt;
  Architekturgrenzen, Build-Profil-Selbsttest, Quality-Gate-Selbsttest, Secretscan.
  Keine Aenderung vor ausdruecklicher Freigabe der Plan-SHA dieser Revision.
Reihenfolge: C1 vor C2 (F4 senkt das Risiko des fatalen Effekts unabhaengig von F1).
Materielle Abweichung (z. B. C0 No-Go) -> Plan aktualisieren und neu freigeben lassen.

## 6. Trennung von Diagnosecode und Produktcode

- Produktcommits (C1–C3) enthalten **keinen** Diagnosecode (kein Failed-Alloc-Backtrace,
  keine `res01_http`-/`diag_30s`-Zeilen).
- Messungen mit Heap-/Stack-Zahlen nutzen einen **Diagnose-Overlay-Build**: Patch
  (aus `diag/pr203_res01_diag_v3_delta_*.patch`) auf den jeweiligen Produktcommit
  gelegt, eigener lokaler Branch, eigene Image-/ELF-Hashes, eigenes Hardware-Gate.
  Der Overlay wird nie in PR #203 oder `main` uebernommen.
- Die R1-Abnahme (R1-U, Abschnitt 7.2) laeuft auf dem **reinen Produktimage**
  (nur vorhandene Boot-/30-s-/120-s-Zeilen, Fehler-/Reset-Freiheit, HTTP-Verhalten).

## 7. Hardwareverifikation (Definition, nicht ausgefuehrt)

### 7.1 Voraussetzungen je Lauf

Frischer dokumentierter App-Boot (RTS-Reset), UART-Mitschnitt, frische Session (< 30 min
Inaktivitaetsablauf), Passwort nur aus `~/pw`, aktorfreies Devboard (kein Aktor angeschlossen
oder ausdruecklich getrennt), Owner anwesend, harter Abbruch bei jeder Anomalie. Vor dem
Teststart im freizugebenden Testblatt je Lauf festhalten: Browserversion, Verbindungsmodell
(Keep-Alive), Session-/Socketzustand, Netzwerkbedingungen, Referenzphasen nach 7.2. Latenzen sind Befunde
fuer den Owner, keine automatische Abnahmebedingung, solange keine vorab festgelegte Schranke
existiert; die 3-s-Timeouts und die HTTP-Fehlerkriterien gelten weiterhin.

Messgroessen je Phase: `free`, `min`, groesster 8-Bit-Block (`diag_30s`, `res01_http`
`pre/built/sent` im Diagnose-Overlay), HTTPD- und Main-Stack-HWM, `heap_alloc_failed`/
Backtraces, Resetursache, Heartbeat-Luecken, HTTP-Statusklassen, Latenz je Minute, Timeouts,
Zaehler `ui_skipped_low_memory`.

### 7.2 R1-Nutzungsprofil R1-U (Abnahme, ein realer Browser)

Ziel: die realistische Nutzung — einstellen, kontrollieren, schliessen, autonom weiterlaufen,
spaeter wieder verbinden — mit **einem** realen Browser und dem in der Shell vorhandenen
5-s-Polling (drei Read-only-Abrufe je Takt, `docs/WEB_UI.md`). Einmaliger Lauf auf einem frischen
Boot des **reinen Produktimages**; ein Wiederholungsprogramm ist nicht vorgesehen.

| Schritt | Inhalt | Zeitfenster |
|---|---|---|
| U0 | frischer Boot, 120 s Idle; liefert nur **Boot-/Leerlaufkontext** (freier Heap, groesster Block, zweimal im Abstand von 30 s), **keine** Erholungsbasis | ≈ 3 min |
| U1 | Browser oeffnen, Login, Shell laedt, Status/Temperaturen sichtbar | ≤ 2 min |
| U2 | Programm einstellen und starten (soweit in R1 vorhanden, `docs/WEB_UI.md`/PR #201) und am Geraet bestaetigt sehen | ≤ 5 min |
| U2R | **Referenzphase:** Browser **schliessen**, Programm laeuft, kein Client verbunden; 120 s Einschwingzeit, dann zwei Messungen im Abstand von 30 s (freier Heap und groesster Block) = **Erholungsreferenz R_ref** | ≈ 3 min |
| U3 | Browser **erneut oeffnen** (bestehende Session weiterverwenden, sonst Login; Zustand im Testblatt vermerken), offen lassen, Status/Temperaturen mit 5-s-Polling pruefen, gelegentliche Bedienung am Geraet (Touch: Home/Uhr/Menues) | 15 min |
| U4 | Browser **schliessen** (Tab bzw. Browserprozess beenden), kein Client verbunden: autonome Regelung weiter, keine Webaktivitaet | 20 min |
| U5 | Browser erneut oeffnen, **wieder verbinden** (Login, falls die Session abgelaufen ist), aktuellen Zustand pruefen (Programmstatus, Temperaturen, Alarme konsistent mit dem Geraet) | ≤ 10 min Beobachtung |

Gesamt etwa 60–65 min. **Umgebungsnachweis (vorhandene Daten, kein zusaetzlicher Geraetezugriff):** vor und
waehrend U0–U5 werden, soweit im UniFi-Controller verfuegbar, AP/BSSID, Signal, Verbindungs-/Retry-Ereignisse und
das Zeitfenster festgehalten (Export oder Screenshot); fehlende Daten werden als `NOT_AVAILABLE` ausgewiesen, nichts
nachtraeglich erfunden. Eine RSSI-Schranke wird nicht definiert. U4/U5 werden gezielt bewertet: Wiederverbinden
nach 20 min Standalone-Betrieb und aktueller Zustand. **Notwendige Evidence:** UART-Mitschnitt des gesamten Laufs
(Heap-/Blockverlauf, `heap_alloc_failed`, Reset-/Panic-Freiheit, Heartbeats, Regelzyklus-Lebenszeichen,
Stack-HWM, `ui_skipped_low_memory`); Browser-Netzwerk-/Konsolenprotokoll (Statuscodes, fehlgeschlagene
Anfragen, Timeouts) und Screenshots der Zustaende U1/U3/U5; Zustandsvergleich Geraet/Web vor und nach U4;
Heap-Erholung: nach Beginn von U4 kehren aktueller Heap und groesster Block innerhalb 120 s auf **≥ 90 %** von
**R_ref** zurueck (Messung wie in U2R: zwei Werte im Abstand von 30 s; Bewertung am schlechteren der beiden).
R_ref ist **vergleichbar** mit dem U4-Zustand: gleiches Programm aktiv, gleicher Boot, gleiche Session, alle
Client-/Socketverbindungen abgebaut. Die Abweichung zwischen R_ref und U4 (Session-Alter, Zahl der seither
gelaufenen Requests) wird im Testblatt festgehalten. U0 ist wegen anderem Zustand (kein Programm, keine
Session, Boot-Leerlauf) **nicht** die Erholungsbasis; es wird nur berichtet. **Voraussetzung:** Fuer den Programmstart sind
gueltige Commissioning-/Sensorwerte noetig (siehe `SIM-27-W-04`: im Fixture ohne diese nicht
startbar); der Testaufbau dafuer ist eine **offene Ownerentscheidung** (Abschnitt 9, D9). Ohne startbares
Programm wird U2 als nicht ausfuehrbar ausgewiesen und die Abnahme bleibt offen, nicht bestanden.

### 7.3 Diagnostische Stresstests (nicht Pflicht der R1-Abnahme)

| ID | Beschreibung | Status |
|---|---|---|
| P-A | 4 Sessions, 3 parallele API-GETs je 5 s und Session (gestaffelt 1,2 s), Shell-GET je Session alle 30 s, neue Verbindung je Request | diagnostisch; historische Ergebnisse (G1, G1b, Stufe E) bleiben `FAIL` bzw. `FAIL/HOLD` |
| P-A2 | wie P-A, Shell-GETs aller 4 Sessions zeitgleich | diagnostisch, nicht geplant |
| P-B | 4 echte Browserkontexte mit Keep-Alive | diagnostisch, nicht geplant |

Diese Profile bleiben als Werkzeug zur Untersuchung von Randverhalten erhalten; sie werden
**nicht** durch diese Revision zu bestandenen Laeufen umgewertet, und es werden in diesem PR **keine**
weiteren P-A-/A-B-Laeufe, NETPROBE1-Proben oder sonstige Netzwerkdiagnostik eingeplant, **sofern nicht
ein Fehler im realen R1-Nutzungsprofil (R1-U) konkret nachgewiesen wird**.

### 7.4 Abnahmekriterien R1-U (Fehlerfreiheit, keine willkuerlichen Heap-Grenzwerte)

1. **Kein** `heap_alloc_failed`, **kein** `abort`/Panic/Watchdog/Brownout, **kein** unerwarteter Reset im
   gesamten R1-U-Lauf.
2. HTTP: 100 % 2xx fuer alle Anfragen des Browsers im Lauf, **keine** Timeouts, keine fehlgeschlagenen
   Anfragen (Browser-Netzwerkprotokoll); Latenzen berichtet, Befund fuer den Owner.
3. **Autonome Regelung** laeuft in U4 ohne Browser unveraendert weiter (Lebenszeichen, kein Reset); der
   Zustand nach dem Wiederverbinden (U5) ist konsistent.
4. **Heap-Erholung** nach Beginn von U4 ≥ 90 % von R_ref (Referenzphase U2R, vergleichbarer Zustand) innerhalb 120 s (aktueller Heap und groesster Block). Eine Unterschreitung ist ein Befund mit Stopp und Ownerbewertung (Hinweis auf, aber kein Beweis fuer ein Leck); ein spaeterer Wert unterhalb 90 % nach weiterer Zeit wird berichtet, nicht still neu bewertet.
5. **Stackreserve:** HTTPD- und Main-Stack-HWM nicht schlechter als die Baseline (2 336 B bzw. 11 416 B frei)
   um mehr als 256 B; sonst Stopp und Befund. `min_free` wird berichtet (Befund, kein automatischer Fail).
6. F4: Falls das Gate ausloest, ist es geloggt, bei ausreichender Reserve erholt sich die Anzeige, ein
   gesperrter Reset-Hold bleibt bis Loslassen + neuem Kontakt wirkungslos; Gate-Ausloesen ist ein Befund.

**Erreichbarkeitsproblem im Lauf:** Tritt beim Browserzugriff (insbesondere U1 oder U5) erneut ein
Erreichbarkeitsproblem auf, wird der konkrete Browserfehler dokumentiert und das Ergebnis ist **HOLD**, nicht PASS;
die Ursache (Funk/AP gegenueber Geraet) bleibt offen. Weist der Umgebungsnachweis eine gestoerte Testumgebung nach,
wird keine fehlerfreie Produktabnahme behauptet; der Owner entscheidet getrennt ueber den Testumgebungsbefund. Einzelne
alte ICMP-Verluste (NETPROBE1) sind **kein** automatischer R1-U-FAIL. Bei reproduzierbarer Stoerung im normalen
Browserablauf wird ein offener Abnahmebefund `HOLD` vorgelegt und mit vorhandenen Daten eingegrenzt; O2, O3 oder
`WIFI_PS_NONE` werden nicht eigenmaechtig umgesetzt.

**Reale Fehler oder Resets unter diesem Profil bleiben Blocker.** Ein fehlerfreier R1-U-Lauf belegt nur
das getestete Profil, den Zeitraum und das Image; er beweist weder allgemeine OOM-Sicherheit noch
Renderer-Absturzfreiheit (Restgefahr D3 unveraendert) noch Robustheit gegenueber intensiven
Mehrfach-Clients.

### 7.5 Hardware-Gates (je eigene Ownerfreigabe, konkret mit Commit, Image-/ELF-Hash, App-only `0x10000`, Rueckfall, Stoppbedingungen)

- **G0** (C0-Spike, Diagnosebranch): ausgefuehrt, C0 = GO.
- **G1'** (Diagnose-Overlay auf dem F4-Fixcommit): **F4-Schwelle und Reset-Hold nur in den relevanten Pfaden
  qualifizieren**: Home, Menues, Uhr/Zurueck, Programm-/Statusseiten, Locale-Varianten, Skip→Resume mit
  gehaltenem Finger und neuer Kontakt (Hold A/B), jeweils mit repraesentativen Seiten-/Zustands-/Text-/
  Kontaktfaellen und gemessenem Render-/Touch-Heapbedarf; ein **Browser mit 5-s-Polling parallel** ist die
  einzige Netzwerklast. Keine Stresslast, kein Werksreset. Ergebnis: abschliessende Bestimmung der Schwelle nach
  Abschnitt 4/2 (`max(a, b) × 2`) **nur fuer diese Pfade**; fehlende Coverage haelt die Schwellenqualifikation
  `HOLD`; **kein** allgemeiner OOM-Schutz wird behauptet.
- **G2** (reines Produktimage): R1-U-Abnahme nach Abschnitt 7.2/7.4.
Keine Flash-/Resetvorgaenge ohne Freigabe.

### 7.6 Native/Build-Nachweise

Neue IDs `SIM-27-H-01..` (Gate-Logik, Befehlsobergrenze, Reset-Hold-Sperre fuer A/B durch die echte
Schleifenentscheidung inkl. > 5 s weiter gehaltenem Finger und fehlendem Loslass-Nachweis, Regression
`SIM-19-R08`, byteweise Gleichheit der Shell, Shell-zu-503-Wechsel/Anhangsloeschung,
Adapter-Bytes/Status/Content-Type, Chunk-Terminierung/Fehlerpropagation, Provisioning-Shell
unveraendert), bestehende Web-/Session-/Codec-/UI-Tests gruen, Architekturcheck, beide ESP-IDF-Profile,
`self-check`, spaeter Pre-Ready nach unabhaengigem Review. Unveraendert respektiert werden Safety-,
Auth-, Session- und Aktorvertraege.

## 8. F2 – nicht Teil der R1-Abnahme; O1 wird zurueckgebaut

F2 (Sendepuffer) wurde als Kandidat O1 (`CONFIG_LWIP_TCP_SND_BUF_DEFAULT` 5760 → 2880) erprobt, ist
**nicht qualifiziert** (kein stabiler Nutzen, kein Schaden belegt, n klein, verschiedene Tage) und wird
nach Schnitt C5 auf den ESP-IDF-Default 5760 zurueckgebaut. Die beobachteten WLAN-/TCP-Stillstaende bleiben
offene historische Evidence; **weitere Netzwerkdiagnostik, O2/O3 und A/B-Programme sind in diesem PR nicht
vorgesehen**, ausser ein Fehler im R1-Nutzungsprofil wird konkret nachgewiesen (dann neuer Plan). Details im
[F2-Plan](issue-27-res01-f2-escalation-plan.md).

## 9. Ownerentscheidungen (D1–D3 freigegeben; D4/D5 ersetzt; D6–D9 offen)

| ID | Frage | Stand / Empfehlung |
|---|---|---|
| D1 | F1-Form: **F1-4** (nicht besitzender statischer Anhang + `httpd_resp_send_chunk`) | freigegeben, umgesetzt |
| D2 | C0-Spike (Hardware-Gate G0) vor dem Produktvertrag | freigegeben, ausgefuehrt (C0 = GO) |
| D3 | F4 als **begrenzte Risikominderung** mit ausdruecklicher Akzeptanz des TOCTOU-/kumulativen OOM-Risikos (weiterhin moeglicher Geraetereset); F4-B nicht geplant | freigegeben, unveraendert; beobachteter Reset bleibt Fail/HOLD |
| D4 | *ersetzt* (frueher: P-A 3×, P-A2, P-B als Pflicht) | durch D6 ersetzt |
| D5 | *ersetzt* (frueher: 90-%-Messbasis fuer vier Sessions) | durch D7 ersetzt |
| **D6** | **R1-Abnahmeprofil R1-U** (Abschnitt 7.2) mit den Abnahmekriterien 7.4 an Stelle von P-A/P-A2/P-B; P-A/P-A2/P-B nur diagnostisch, historische `FAIL` bleiben | **offen**, Empfehlung Ja |
| **D7** | Messbasis: Heap-Erholung ≥ 90 % von **R_ref** (Referenzphase U2R: Programm aktiv, Browser geschlossen, gleiche Session) innerhalb 120 s nach Beginn U4; U0 nur Kontext. Alternative zur Entscheidung: nur Trend/Plateau-Bericht ohne 90-%-Schwelle | **offen**, Empfehlung Ja |
| **D8** | **O1-Rueckbau** (C5): Entfernen des 2880-Blocks, Rueckkehr zu 5760, Tests nach Abschnitt 5 C5; keine Netzwerkdiagnostik/A-B in diesem PR | **offen**, Empfehlung Ja |
| **D9** | Testaufbau fuer R1-U-Schritt U2: Bereitstellung gueltiger Commissioning-/Sensorwerte (oder alternative Programm-Startbarkeit am Devboard); G1' (F4-Qualifikation, Abschnitt 7.5) als Diagnose-Overlay-Messung | **offen** |

**Ownerentscheide vom 2026-10-15 (nach G1P-2-Retest, `FAIL/HOLD` historisch, nicht umgewertet):**
D6–D8 = JA; Stack = **A**: Der Hard-Stop `main-stack-reserve` (kleinster HWM 10 984 B frei von 24 576 B, Offline-Eingrenzung im Review-Branch `diag/review/g1p/G1P2_OFFLINE_EINGRENZUNG.md`) wird als akzeptierter Befund der erweiterten Touch-Navigation erfasst; die relative Grenze 11 160 B (D3-Baseline, nicht gleichwertige Abdeckung) gilt im G2-Web-Teiltest nur als Trend-/Reservebericht, nicht als Abbruchgrenze; Stacküberlauf, Panic, Watchdog, Reset oder sonstiger konkreter Fehler bleiben Hard-Stop. F4 = **A**: Skip 13 312 B / Resume 16 640 B unveraendert, `F4_THRESHOLD=PROVISIONAL_PENDING_G1`, `G1_QUALIFICATION=NOT_PROVEN`; die beobachteten 16 768 B sind gemeinsame Heap-Schwankung, keine belastbare Formelbasis. G2-Web = **JA, einmalig, `PARTIAL`**: ein App-only-Flash des C5-Produktimages `esp32_release` (App `ebb296c0…6633`, Commit `162a667`) auf dem sensor- und aktorlosen Devboard mit Display/Touch; ein Browser, 15 min 5-s-Polling, 20 min ohne Client, Wiederverbinden; Ergebnis hoechstens `PASS_PARTIAL` (kein U2/U2R/`R_ref`, kein `G2=PASS`, kein `PR203_FINAL_GO=GO`).

## 10. Risiken

- Chunked-Transfer kann Segmentierung/TX-Puffer erhoehen (Hypothese ohne Pufferzaehlung); C0 hat anhand
  messbarer Ressourcenwerte GO ergeben.
- F4-Gate kann bei knappem Speicher die Anzeige voruebergehend einfrieren; nach bestandenem Gate bleiben
  OOM/Abort/Reset moeglich (D3 ownerakzeptiert, Testgrenze Abschnitt 4/6). Die F4-Qualifikation gilt nur fuer die
  getesteten Pfade.
- **R1-U belegt nicht** die Robustheit gegenueber mehreren intensiv pollenden Sessions, vielen neuen
  TCP-Verbindungen oder dem beobachteten WLAN-/TCP-Stillstand; ein solcher Stillstand im realen
  Nutzungsprofil bleibt Blocker, seine Ursache ist offen (AP, Funkweg, WLAN-Treiber, lwIP).
- Ein einzelner R1-U-Lauf ist keine Fehlerrate; sporadische Ereignisse koennen unentdeckt bleiben. Ein
  Wiederholungsprogramm ist bewusst nicht vorgesehen (Ownerentscheid), der Owner trifft die Abnahmeentscheidung.
- Der O1-Rueckbau aendert eine gemeinsame Konfiguration beider Profile; Abdeckung ueber Build-/Konfigurationstests
  (C5), keine Hardwarewirkung behauptet.
- Die Erwartung (≈ 14 KB) war Rechnung, nicht Messung.
