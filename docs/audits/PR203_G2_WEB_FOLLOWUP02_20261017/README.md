# PR #203 – G2-Web-Folgelauf 02: Browser-Transportfehler

**G2_WEB_FOLLOWUP02=FAIL/HOLD. B2–B4=NOT_RUN. Independent Ergebnisreview=PENDING.**

Der neue korrigierte UART-Monitor bestand den Live-Bootnachweis. Der Browserversuch scheiterte bereits beim ersten sichtbaren Dokumentabruf: **`net::ERR_INCOMPLETE_CHUNKED_ENCODING 200 (OK)` nach 34,88 s**. Der HTTP-200-Status ist bei fehlgeschlagenem Bodytransfer kein erfolgreicher Shellabruf. Nach Eingang dieses Screenshots wurde STOP/HOLD angeordnet, der vorhandene UART-Monitor geschlossen und der Owner bestätigte den geschlossenen ESP32-Tab. Kein weiterer Portzugriff, Reset, Flash, Retry oder Reparaturversuch durch Codex.

## Stand, Durchführung und Abweichungen

Produkt-PR zu Beginn `55b41a5bb60f9de048f2a28e4684fa13ed438347`; Produktquelle `a9a1156a1d44d4679957526bbc34a30be6422e4e`. Installiertes und vorher byteverifiziertes Releaseimage beibehalten. Vorhandene lokale App-/ELF-/C5-Rückfallhashes erneut offline PASS, frühere Flash-/Nicht-App-Evidence wiederverwendet. Kein Neubuild oder erneuter Readback/Flash.

Monitor auf separatem Nicht-Merge-Stand `893f3bb73a50b484ce79b3b80722873452d998fc`, SHA-256 `955f1c3b1cc12587a012b6ecae0740657cd991152090de801b0458944f9571c5`. Owner bestätigte unabhängige Fix Verification PASS und ordnete den einmaligen zusammenhängenden Lauf an. Genau eine Portöffnung, `g2-trend`, Gate-ID `PR203_SINGLE_FLIGHT_G2_WEB_20261016_IMAGE01`, ohne Lasttreiber. Bekannte Favicon-404 und erwartete anonyme/abgelaufene Session-401 waren ausgenommen.

**Tatsächlicher Browser war Windows-Chrome, nicht der vorgesehene Selkies-Chrome.** Chromeversion und tatsächlicher Netzwerkweg sind nicht nachgewiesen. Der Owner korrigierte die anfängliche Meldung „NetLog läuft“: Nur DevTools „Keep log“ war aktiviert; es wurde keine Datei gespeichert, und nach Tab-Schluss war auch die Netzwerkliste geschlossen. **NetLog/HAR: NOT_RECORDED / NOT_AVAILABLE.** B1 wurde begonnen, bevor Dateipfad, tatsächliche Browserumgebung und eine auf Platte laufende Browseraufzeichnung verifiziert waren. Das ist eine Ausführungs- und Nachweislücke; das vorgesehene Profil ist nicht erfüllt.

## PASS / FAIL / NOT_PROVEN

| Nachweis | Ergebnis |
|---|---|
| Gepinnter Monitor, lokale Artefakte, genau eine Portöffnung | PASS |
| Vollständiger neuer Live-Bootnachweis | PASS |
| UART innerhalb des aufgezeichneten Zeitfensters | Kein Fatal-/Reset-/Heartbeat-Hard-Stop erkannt; Capture regulär per STOP beendet |
| Initialer Browser-Dokumentabruf | **FAIL_OBSERVED:** unvollständige Chunkübertragung, 34,88 s |
| Erfolgreicher späterer Login / späteres Dokument | OBSERVED_ONLY aus Screenshot; kein Ersatz für den fehlgeschlagenen ersten Abruf |
| Browserprofil und gespeicherter vollständiger Requesttrace | NOT_MET / NOT_RECORDED |
| Stoppen beim ersten tatsächlichen Browserfehler | NOT_MET_OBSERVED: weitere Login-/Seiten-/API-Aktivität folgte vor Meldung und STOP |
| B2 15 min / B3 20 min / B4 10 min | NOT_RUN; das feste B2-Fenster wurde nie gestartet |
| Single-Flight-Hardwareverhalten, JSON-Bodyabschluss, vollständige HTTP-Statistik | NOT_PROVEN |
| Heap-/Blockreserve während des fehlerhaften Abrufs, HTTPD-Stack, Recovery | NOT_PROVEN |

## Browserfehler und Folgeaktivität

[Original-Screenshot 1](OWNER_SCREENSHOT_01.png) zeigt eine teilweise gerenderte Loginseite mit Offline-Anzeige, einen fehlgeschlagenen Dokumentabruf mit 34,88 s Dauer und die vollständige Console-Fehlerklasse `net::ERR_INCOMPLETE_CHUNKED_ENCODING 200 (OK)`.

Der Owner berichtet anschliessend eine falsche Passworteingabe und danach erfolgreichen Login. [Original-Screenshot 2](OWNER_SCREENSHOT_02.png) zeigt Online/Bereit, einen Login mit HTTP 200 / 2,76 s, einen späteren Dokumentabruf mit HTTP 200 / 86 ms sowie sichtbare erfolgreiche API-Antworten. Er enthält ausserdem 401 und abgebrochene Geschwisterrequests sowie den ausgenommenen Favicon-404. Ob die Abbrüche sämtlich zum vorgesehenen Single-Flight-Abbruch bei 401 gehören, ist ohne vollständigen Trace **NOT_PROVEN**. Der HTTP-Status der falschen Passworteingabe ist ebenfalls nicht belegt.

Diese spätere Aktivität erfolgte nach dem ersten dokumentierten Browserfehler. Der Operator meldete den Fehler erst danach; Codex ordnete bei Eingang der Screenshots STOP an. Genaue Fehler-, Folgeaktions- und Browser-Schliesszeiten lassen sich nicht rekonstruieren. Die damaligen Screenshotzeiten sind nur relative DevTools-Zeiten. Manueller Reload gegenüber programmseitiger Navigation ist nicht unterscheidbar. Die UART-Aufzeichnung endete vor der später empfangenen Schliessbestätigung; eine vollständige UART-Abdeckung bis zum tatsächlichen letzten Browserrequest ist daher nicht bewiesen.

Es werden keine p50/p95/p99 oder Fehlerraten aus den wenigen sichtbaren Zeilen abgeleitet. Es gibt keinen Nachweis aller Polling-Gruppen oder ihrer JSON-Abschlüsse und keine aktuelle UniFi-Aufzeichnung. Die historischen Funkwerte werden diesem Windows-Versuch nicht als aktuelle Messwerte zugeordnet.

## UART-, Zeit- und Ressourcen-Evidence

Capture: **17.10.2026 06:41:36,343210–06:52:12,506176 UTC**, entsprechend **08:41:36–08:52:12 Europe/Zurich**, Dauer **636,163 s**. Ein gültiger initialer POWERON_RESET, zwei unqualifizierte Vor-Boot-Zeilen, danach Source, `esp32_release`, deaktivierte Aktoren, vollständiges `application: ready`, HomeConnected und 632 qualifizierte Heartbeats. Capture-Maximallücke 3,502251 s einschliesslich Boot; grösste nachfolgende Empfangslücke rund 1,258503 s. Kein weiterer Reset, kein geloggter Heap-Fail/Panic/Abort/WDT/Brownout/Stackoverflow im aufgezeichneten Zeitraum.

Der Monitorprozess endete mit Exitcode 0 und `CAPTURE_COMPLETE_PENDING_REVIEW`, `failure=null`. **Dieser UART-interne Status ist kein G2-Web-PASS:** Ursache des externen STOP war der Browser-Transportfehler. Rohdaten- und Chunktimingreferenzen sind in [TIMELINE.json](TIMELINE.json), [CAPTURE.json](CAPTURE.json), [HEARTBEATS.json](HEARTBEATS.json) und [UART_CHUNKS.jsonl](UART_CHUNKS.jsonl) enthalten. Zeitmarken bezeichnen Hostempfang/Parsing; Clockunsicherheit und browserseitige absolute Zeitzuordnung sind nicht unabhängig bestimmt.

Im UART steht bei rund +280,925 s `httpd_sock_err: error in send : 11`. Später folgen zwei `404 Not Found`-Meldungen und drei `error in recv : 104`. Die [Original-HTTPD-Fehlermeldungen mit umgebenden Heartbeats](UART_HTTP_ERROR_CONTEXT.txt) sind veröffentlicht. Ihre Zuordnung zu konkreten Browserrequests ist ohne HAR/NetLog nicht bewiesen; insbesondere werden weder die Sendewarnung als bewiesene Ursache der Chunkfehlermeldung noch die 404-Meldungen ohne Requestkorrelation sicher dem Favicon zugeordnet.

Alle sechs vorhandenen [Ressourcenstichproben](UART_RESOURCES.txt) liegen vor der ersten aufgezeichneten HTTPD-Sendewarnung:

| Punkt | Freier Heap B | Grösster 8-Bit-Block B | Boot-Minimum B | Main-HWM B |
|---|---:|---:|---:|---:|
| after_platform_begin | 169284 | 110592 | 169284 | 18536 |
| after_application_begin | 71448 | 65536 | 67456 | 11704 |
| after_ui_init | 44740 | 40960 | 36088 | 11704 |
| stable_home_wifi | 42672 | 40960 | 35496 | 11704 |
| periodic_30s | 42664 | 40960 | 35196 | 11704 |
| idle_120s | 42704 | 40960 | 35196 | 11704 |

Main-HWM-Minimum der erfassten Stichproben: 11 704 B, kein erfasster Trendwert unter 11 160 B. Das ist keine kontinuierliche Stackmessung während des Webfehlers. HTTPD-Stackwerte fehlen vollständig. Letzte Heapprobe bei `idle_120s`, keine Probe am fehlgeschlagenen Sendepfad und kein Recoverylauf; aktuelle Reserve und transiente Spitzen dort sind **NOT_PROVEN**. Das Boot-Minimum wird nicht als aktuelle Recoverymessung verwendet. Historisches `min_free=5228 B` und lange GET-Dauern bleiben offene frühere Befunde; es werden keine nachträglichen Schwellen gesetzt.

## Provenienz, Auslassungen und Übergabe

[ARTIFACT_MANIFEST.json](ARTIFACT_MANIFEST.json) und [SHA256SUMS](SHA256SUMS) binden Quellstände, Artefakte, öffentliche Dateien und den privaten Rohstrom. Die beiden Screenshotdateien sind unveränderte vom Owner gelieferte PNG-Bytes, visuell auf Zugangsdaten geprüft; ihre einzige Textmetadatenangabe ist `Software=Greenshot`. [HTTP_BROWSER_EVIDENCE.json](HTTP_BROWSER_EVIDENCE.json) trennt Screenshotbefunde, Ownerangaben und fehlende Nachweise. [UART_BOOT_PROOF.txt](UART_BOOT_PROOF.txt) enthält die ausgewählten Boot-/ersten Heartbeatzeilen.

Vollständige 51 368 UART-Rohbytes, komplette UART-Logs, Socketdaten, Readbacks/Rückfallartefakte und private Ausführungsdaten bleiben lokal geschützt. Öffentliche UART-Auszüge enthalten nur ausgewählte Boot-, Ressourcen-, Heartbeat- und HTTPD-Zeilen; andere Kontextzeilen sind als ausgelassen markiert, Zeilenenden auf LF normalisiert. Keine Zugangsdaten, Cookies, CSRF-Werte oder HTTP-Bodies wurden veröffentlicht. Ein fehlender Browsertrace wird nicht durch hypothetische Daten ersetzt. Frühere FAIL/HOLD-Ergebnisse bleiben unverändert.

**Nächster Schritt: unabhängiger Ergebnisreview.** Ursache der unvollständigen Shellantwort und die fehlende Request-/UART-Korrelation bleiben offen. Keine weitere Hardwareaktion, kein automatischer Retry/Flash, keine Produktkorrektur oder Schwellwertänderung aus diesem Lauf.

`G2_WEB_FOLLOWUP02=FAIL/HOLD` · `G2_WEB=FAIL/HOLD` · `G2_RETRY=NOT_AUTHORIZED` · `B2_B4=NOT_RUN` · `B4_RECONNECT=FAIL_OBSERVED` (historisch) · `G1_QUALIFICATION=NOT_PROVEN` · `F4_THRESHOLD=PROVISIONAL_PENDING_G1` · `RES-01=BLOCKER` · `PR203_FINAL_GO=HOLD` · `ACTUATOR_RELEASE=NO`
