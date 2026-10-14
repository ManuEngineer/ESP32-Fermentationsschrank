# Issue #27 / PR #203 – RES-01: Heap-Erschoepfung der Web-Shell-Auslieferung beheben (F1 + F4) (Plan)

Status: `C0_GO_C1_C3_SOFTWARE_IMPLEMENTED_PENDING_INDEPENDENT_REVIEW`.
Ownerfreigabe vom 2026-10-13 fuer die exakte Plan-SHA
`6222efce976dc3bea3cf397c2dca56a5fb21859d`: D1–D5 = JA, einschliesslich
TOCTOU-/OOM-Restgefahr mit moeglichem Reset (beobachteter Reset bleibt Testfehler).
C0-Firmware ausschliesslich Diagnosebranch @ `9a9a8bd`, Treiber `cfcf282`;
Owner akzeptiert am 2026-10-14 den unabhängigen R3-Ergebnisreview auf
`ab0ae55e78fbd1f27825338cf5913b0648b59c19`, entscheidet **C0=GO** und
genehmigt produktive Software-C1–C3 gemäß dieser freigegebenen Planbasis.
F2 bleibt außerhalb; keine weiteren Hardwarehandlungen, G1/G2 separat.
Owner genehmigt zusätzlich Option 1 des Schwellen-Korrekturdeltas auf dem
Software-/Planstand `7b229be806f5fd7d2978a4f55bcbbb42797c71ad`: vorläufig
Vektorblock × 2, `F4_PROVISIONAL_THRESHOLD=APPROVED`. Die Schwelle bleibt
**`PROVISIONAL_PENDING_G1`**; Software-C1–C3 einschließlich produktiver
Gate-Anbindung ist umgesetzt. G1 muss den wirklichen Render-/Touch-Bedarf
auch für vom Gate unter Speicherlast übersprungene Pfade messen und die
Schwelle nach Abschnitt 4/2 abschließend bestimmen. Keine Hardwarefreigabe,
keine finale RES-01-/PR-Freigabe.
[Softwarestand/Ownerentscheidung](../audits/PR203_RES01_SOFTWARE_20261014.md).
Owner akzeptiert nach unabhängigem Review die begrenzte R2-Referenz (Baseline bleibt FAIL) und genehmigt am
2026-10-14 separat G0-R3: Preflight, ein gepinnter C0-App-only-Flash, P-A/Recovery.
**G0-R3-Ergebnis, C0-GO Owner-akzeptiert:** 120-s-Phasenvergleich zeigt medianes
Build-Delta −7104 B, bessere minimale/mediane Heap-/Blockreserven; alle geplanten
240 API-/40 Shell-Ticks je Session (3044 Antworten HTTP 200) und 300-s-Recovery
ohne Anomalie. ≥90% des vergleichbaren Vier-Session-Idle nach 73,823 s,
Stackgrenzen erfüllt. Aktive Antworten enden im unveränderten 1200-s-Schedule
bei 1198,820 s; diese Ausführungsgrenze ist explizit dokumentiert.
Befunde: Boot-Heapminimum 5228 B, fünf GET-Gesamtdauern >3 s bei
unveränderten Sockettimeouts ohne Timeout-Exception; kein allgemeiner OOM-/F4-Nachweis.
[Ergebnis/Evidence R3](../audits/PR203_G0_R3_20261014_RESULT.md), versiegeltes Paket
`hw-evidence-pr203/diag/c0/g0_r3_20261014_run01/`. Ein App-Flash, keine Erase-all-
oder Nicht-App-Änderung. Nach Messung keine weitere Hardwarehandlung.
[Akzeptierte R2-Referenz](../audits/PR203_G0_R2_20261014_RESULT.md) und
[alter G0-Login-STOP](../audits/PR203_G0_20261013_RESULT.md) unverändert.
Nächster Schritt: Builder-Self-Check auf Implementierungs-HEAD, danach
unabhängiger Software-Review. Kein G1/G2/Ready/Merge.
`C0=GO`, `C0_OWNER_DECISION=GO`, `INDEPENDENT_G0_R3_RESULT_REVIEW=OWNER_ACCEPTED`,
`G1_G2=NOT_AUTHORIZED`.
Diese Fortschreibung aktualisiert den Owner-/Softwarestatus; die
übrigen freigegebenen Abschnitte bleiben unverändert. Abschnitt 4/2
enthält die ausdrücklich freigegebene vorläufige Schwellenanbindung;
die abschließende Messbasis und Hardwarefreigabe bleiben offen.
Basis: PR #203 (`ed29c9c`), Diagnose-Evidence D3
(`hw-evidence-pr203/diag/run/`). `RES-01=BLOCKER` (Ownerentscheid),
`PR203_FINAL_GO=HOLD`, `ACTUATOR_RELEASE=NO`.

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
- **F2** (TCP-Sendepuffer / lwIP-Konfiguration) nur als dokumentierte
  Eskalation (Abschnitt 8), nicht vorsorglich.

Nicht-Ziele: Aenderung von Authentifizierung, CSRF, Session, Replay, Revisionsschutz,
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

## 5. Umsetzungsschnitte (eine Planfreigabe vor C0–C4; Hardware-Gates getrennt)

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
- **C4 – Hardwareverifikation (Abschnitt 7)** mit zwei Gates G1/G2; Evidence als
  PR-Kommentar und Rohartefakte, kein Produktcode.
Reihenfolge: C1 vor C2 (F4 senkt das Risiko des fatalen Effekts unabhaengig von F1).
Materielle Abweichung (z. B. C0 No-Go) -> Plan aktualisieren und neu freigeben lassen.

## 6. Trennung von Diagnosecode und Produktcode

- Produktcommits (C1–C3) enthalten **keinen** Diagnosecode (kein Failed-Alloc-Backtrace,
  keine `res01_http`-/`diag_30s`-Zeilen).
- Messungen mit Heap-/Stack-Zahlen nutzen einen **Diagnose-Overlay-Build**: Patch
  (aus `diag/pr203_res01_diag_v3_delta_*.patch`) auf den jeweiligen Produktcommit
  gelegt, eigener lokaler Branch, eigene Image-/ELF-Hashes, eigenes Hardware-Gate.
  Der Overlay wird nie in PR #203 oder `main` uebernommen.
- Die abschliessende Bestaetigung laeuft zusaetzlich auf dem **reinen Produktimage**
  (nur vorhandene Boot-/30-s-/120-s-Zeilen, Fehler-/Reset-Freiheit, HTTP-Verhalten).

## 7. Hardwareverifikation (Definition, nicht ausgefuehrt)

Voraussetzungen je Lauf: frischer dokumentierter App-Boot (RTS-Reset), UART-Mitschnitt,
frische Sessions (< 30 min Inaktivitaetsablauf), kein erneutes Login waehrend der Phase,
harter Lastabbruch bei jeder Anomalie (alle Sockets schliessen, kein Logout, 3-s-Timeouts;
Korrektur gegenueber D3), Passwort nur aus `~/pw`.

Vor Teststart im freizugebenden Testblatt (D4/D5) je Profil festhalten:
Lasttreiber-/Browserversion, Verbindungsmodell, Session-/Socketzustand,
Netzwerkbedingungen, Latenzbewertung und Idle-Referenz. Fuer API-Latenzen eine
begruendete Fehlerschranke oder eine Referenz **unter gleicher Last** samt
zulaessiger Abweichung vorab bestimmen; keine nachtraeglich an Messwerte
angepasste Schwelle. Sind Altlaeufe nicht vergleichbar und fehlt eine solche
vorab festgelegte Schranke, bleiben Latenzen/Trend Befunde fuer den Owner,
keine automatische Abnahmebedingung. Die festgelegten 3-s-Timeouts und
HTTP-Fehlerkriterien gelten weiterhin.

**Lastprofile (reproduzierbar, Skripte in `hw-evidence-pr203/`, Version und Hash in der Evidence):**

| ID | Beschreibung | Dauer | Wiederholungen |
|---|---|---|---|
| **P-A** | 4 Sessions, 3 parallele API-GETs je 5 s und Session, gestaffelt (1,2 s), Shell-GET je Session alle 30 s (Profil D2/D3; reproduzierte den Fehler in 2 von 4 Altlaeufen innerhalb 4 min) | **20 min** | **3** frische Boots |
| **P-A2** | wie P-A, Shell-GETs aller 4 Sessions zeitgleich (Worst-Case-Ueberlappung, kein Flooding) | **20 min** | 1 |
| **P-B** | **4 echte Chromium-Kontexte** (CDP-Treiber, Keep-Alive, Seiten-JS pollt wie ausgeliefert), Shell einmal je Anmeldung plus Reload zweier Sessions alle 60 s | **20 min** | 1 |
| P-R | Erholung: 5 min ohne Last nach P-A (ohne Reset) | 5 min | nach jedem P-A |

**Messgroessen je Phase (Diagnose-Overlay):** `free`, `min`, groesster 8-Bit-Block
(`diag_30s`, `res01_http` je Shell-Antwort `pre/built/sent`), HTTPD- und Main-Stack-HWM,
`heap_alloc_failed`/Backtraces, Resetursache, Heartbeat-Luecken, HTTP-Statusklassen,
p50/p95/p99/max-Latenz je Minute, Timeouts, Zaehler `ui_skipped_low_memory`.
API- und Shell-Latenzen jeweils getrennt fuer P-A, P-A2 und P-B ausweisen;
API-only-D1 ist Kontext, kein harter Leistungsvergleich mit zusaetzlicher
Shell-Last oder anderem Verbindungsmodell.

**C0-Go/No-Go unter identischer Last:** Diagnose-Baseline und Chunk-Kandidat
mit gleichem P-A-Profil, Messaufbau und vergleichbarem Session-/Socket-/
Netzwerkzustand vergleichen; Referenz und Bewertung vor dem Lauf dokumentieren.
Go erfordert messbar geringeren Shell-Spitzenbedarf (Erwartung ≈ −6,6 bis −8,7 KB,
keine harte Schranke), keine schlechtere gemessene Heap-/Blockreserve, keine
Allokationsfehler/Resets und erfuellte HTTP-/Timeout-Kriterien; Latenzen gemaess
vorab festgelegter Bewertung (Schranke/gleiche Last oder Befundstatus) behandeln.
Ohne vergleichbare Ressourcenreferenz ist C0-Go nicht belegt.
"Es entstehen nicht mehr WLAN-TX-Puffer" ist mit der vorhandenen Heap-
Instrumentierung nur eine **Hypothese**. Als hartes Kriterium darf die Aussage
nur verwendet werden, wenn eine tatsaechliche Pufferzaehlung (gleiche Messpunkte,
Definition der gleichzeitig belegten Puffer, Referenz) instrumentiert und im
Diagnose-Overlay/G0 freigegeben ist. Sonst entscheiden die genannten messbaren
Werte; Heap-/Blockmessungen sind kein Nachweis der Pufferanzahl. Bei No-Go
zurueck zum Plan, keine automatische F2-Umsetzung.

**Abnahmekriterien (Fehlerfreiheit, keine willkuerlichen Heap-Grenzwerte):**
1. **Kein** `heap_alloc_failed`, **kein** `abort`/Panic/Watchdog/Brownout, **kein** unerwarteter Reset in allen Laeufen (P-A 3×, P-A2, P-B).
2. HTTP: 100 % 2xx fuer alle Read-/Shell-Requests, **keine** Timeouts, keine Verbindungsabbrueche waehrend der Last. API-Latenzen je Profil und Minute gegen die vorab bestimmte Fehlerschranke oder vergleichbare Referenz unter gleicher Last bewerten; Shell-Latenzen getrennt berichten. D1 ohne Shell-Last ist keine harte Referenz. Bei unvergleichbaren Altlaeufen ohne vorab festgelegte Schranke Latenzen und 20-min-Trend als Befund fuehren.
3. **Heap-Erholung bei vergleichbarem Idle-Zustand:** Vor Last einen Idle-Referenzwert desselben Images **mit denselben vier Sessions** und beruhigtem Netzwerk erfassen. Nach Lastende Polling/Reloads beenden, offene Keep-Alive-/sonstige Clientverbindungen kontrolliert schliessen und das Abklingen der Server-Sockets dokumentieren; Sessions ohne Logout erhalten (Anzahl, Gueltigkeit und Ablaufzustand dokumentieren), WLAN-Modus/-Verbindung vergleichbar halten. P-R beginnt beim dokumentierten Last-/Clientverbindungsende; fehlen vergleichbare Zustaende innerhalb von 120 s, ist die Erholung nicht belegt. Aktueller freier Heap und aktueller groesster Block muessen innerhalb dieser 120 s jeweils ≥ 90 % ihrer passenden Referenz erreichen. Alternativ ist eine andere Idle-Referenz nur bei belegbar gleichem Session-, Socket- und Netzwerkzustand zulaessig. Die alten ≈ 42,3–42,6 KB frei / ≈ 40,9 KB Block sind Kontext, keine automatische Vier-Session-Referenz. Das monotone Boot-Minimum `min_free` dient nie als Erholungsmessung. Vergleichbare aktuelle Heap-/Blockwerte ueber den Lauf auf fortschreitende Abnahme pruefen und dokumentieren.
4. **Stackreserve:** HTTPD- und Main-Stack-HWM nicht schlechter als die Baseline-Werte (2 336 B bzw. 11 416 B frei) um mehr als 256 B; sonst Stopp und Befund.
5. `min_free` unter P-A/P-A2/P-B wird berichtet und gegen den Altwert (6,6–7,9 KB) und die Erwartung (≈ 14 KB) verglichen; eine Unterschreitung der Erwartung bei fehlerfreiem Lauf ist **kein** automatischer Fail, sondern ein Befund fuer den Owner. Ein einzelnes `heap_alloc_failed` ist Fail und loest die F2-Eskalationsbewertung aus.
6. F4: Falls das Gate ausloest, ist es geloggt, bei ausreichender Reserve erholt sich die Anzeige, ein gesperrter Reset-Hold bleibt bis Loslassen + neuem Kontakt wirkungslos. Gate-Ausloesen in P-A/P-A2/P-B ist ein Befund (F1-Reserve zu gering), kein Fail. Jeder beobachtete Abort/Reset bleibt Fail nach Kriterium 1, auch bei akzeptiertem TOCTOU-Restrisiko; ein fehlerfreier Lauf beweist keine allgemeine Renderer-Absturzfreiheit.

**Hardware-Gates (je eigene Ownerfreigabe, konkret mit Commit, Image-/ELF-Hash, App-only `0x10000`,
Rueckfall, Stoppbedingungen):** G0 (C0 Spike, Diagnosebranch), G1 (Diagnose-Overlay auf Fixcommit,
Messung), G2 (reines Produktimage, Bestaetigung). Keine Flash-/Resetvorgaenge ohne Freigabe.

**Native/Build-Nachweise:** neue IDs `SIM-27-H-01..` (Gate-Logik, Befehlsobergrenze,
Reset-Hold-Sperre fuer A/B durch die echte Schleifenentscheidung inkl. > 5 s
weiter gehaltenem Finger und fehlendem Loslass-Nachweis, Regression `SIM-19-R08`,
byteweise Gleichheit der Shell, Shell-zu-503-Wechsel/Anhangsloeschung,
Adapter-Bytes/Status/Content-Type, Chunk-Terminierung/Fehlerpropagation,
Provisioning-Shell unveraendert),
bestehende Web-/Session-/Codec-/UI-Tests gruen, Architekturcheck, beide ESP-IDF-Profile,
`self-check`, spaeter Pre-Ready nach unabhaengigem Review.

## 8. F2 – nur Eskalation (nicht vorgesehen)

Auslöser: `heap_alloc_failed` oder fehlende Heap-Erholung in den Laeufen von Abschnitt 7
trotz F1+F4. Dann eigener Plan: `CONFIG_LWIP_TCP_SND_BUF_DEFAULT` 5760 -> 2880 (Ersparnis
≈ 3 KB je gleichzeitiger Antwort laut Rechnung), Hardwaremessung von Latenz und
Durchsatz; ggf. Begrenzung der Verbindungsparallelitaet im Adapter. Nicht Teil dieses Plans.

## 9. Ownerentscheidungen (D1–D5 freigegeben; Hardware-Gates separat)

| ID | Frage | Empfehlung |
|---|---|---|
| D1 | F1-Form: **F1-4** (nicht besitzender statischer Anhang + `httpd_resp_send_chunk`) | Ja, mit C0-Spike als Go/No-Go |
| D2 | C0-Spike (Hardware-Gate G0) vor dem Produktvertrag | Ja (Kosten ≈ 1 Flash/Messlauf, verhindert einen unnoetigen Vertragswechsel) |
| D3 | F4 als **begrenzte Risikominderung**: UI-Skip inkl. Touch, Hold-Timer freigeben und bis beobachtetem physischem Loslassen + neuem Kontakt sperren; ausdrueckliche Akzeptanz des in Abschnitt 4/6 beschriebenen TOCTOU-/kumulativen OOM-Risikos mit weiterhin moeglichem Geraetereset; F4-B nicht geplant | Ja, nur mit ausdruecklicher Restgefahrakzeptanz; beobachteter Reset bleibt Fail/HOLD |
| D4 | Verifikationsprofile P-A (3×20 min), P-A2, P-B, Erholung P-R und C0-/Abnahmekriterien; Latenzbewertung je Profil vor Teststart festlegen (gleiche Last oder begruendete Schranke, sonst Befund); TX-Pufferanzahl ohne Zaehlinstrumentierung nur Hypothese | Ja, Testblatt vor Lauf freigeben |
| D5 | Messbasis 90 % des passenden Idle-Referenzwerts fuer aktuellen Heap und groessten Block; bevorzugt dieselben vier Sessions vor Last, kontrollierter Socketabbau und vergleichbarer Netzwerkzustand | Ja, Referenz/Zustandsvergleich vor Lauf festlegen |

## 10. Risiken

- Chunked-Transfer kann Segmentierung/TX-Puffer erhoehen (Hypothese ohne Pufferzaehlung;
  C0 entscheidet anhand messbarer Ressourcen-/HTTP-/Latenzwerte; No-Go moeglich).
- F4-Gate kann bei knappem Speicher die Anzeige voruebergehend einfrieren;
  nach bestandenem Gate bleiben OOM/Abort/Reset moeglich (D3 ownerakzeptiert, Testgrenze Abschnitt 4/6).
- Der Lastprofil-Vergleich ist durch das Verbindungsmodell des Treibers beeinflusst (P-B mit echten
  Browsern ergaenzt das); keine Aussage ueber beliebige LAN-Clients.
- Die Erwartung (≈ 14 KB) ist Rechnung, nicht Messung.
