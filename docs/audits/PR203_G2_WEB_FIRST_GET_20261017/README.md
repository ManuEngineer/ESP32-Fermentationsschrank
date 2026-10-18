# PR #203 – Erstabruf nach Ruhephase: gezielte Diagnose

**`FIRST_GET_AFTER_IDLE=PASS_OBSERVED`, `FAILURE_REPRODUCTION=NOT_REPRODUCED`, `ROOT_CAUSE=NOT_PROVEN`.**
Der einzige Dokumentabruf war erfolgreich: HTTP 200, **336,196 ms**, vollständiger HTML-Body. Ein Zweitabruf wurde nicht ausgeführt, weil die dafür freigegebene Bedingung – Browser-/HTTP-Transportfehler – nicht eintrat. Der historische [Folgelauf 02](../PR203_G2_WEB_FOLLOWUP02_20261017/README.md) mit `ERR_INCOMPLETE_CHUNKED_ENCODING` nach 34,88 s bleibt **FAIL/HOLD**. Dieser Diagnoseversuch ist keine B4-, G2-, F4-/G1- oder R1-U-Freigabe.

## Durchführung und Identität

Ausgangs-PR-HEAD `2443dc96e40cc056bd38e7a932563b106ba5ca9d`, installiertes Produkt aus `a9a1156a1d44d4679957526bbc34a30be6422e4e`, `esp32_release`. Monitor unverändert auf dem separat geprüften Nicht-Merge-Stand `893f3bb73a50b484ce79b3b80722873452d998fc`; `g2-trend`, bestehende Gate-ID `PR203_SINGLE_FLIGHT_G2_WEB_20261016_IMAGE01`, ohne Lasttreiber. Vorhandene App-/ELF-/C5-Rückfallhashes offline bestätigt; vollständige Hashes und private Eingangsartefakte im [Manifest](ARTIFACT_MANIFEST.json). Frühere Flash-/Readback-Evidence wurde wiederverwendet. **Kein Flash, kein Neubuild, keine aktive Board-Netzwerkprobe und kein Produkt-/Monitor- oder Konfigurationsfix.**

Der Owner genehmigte für diesen zusammenhängenden Diagnoseversuch genau eine kontrollierte UART-Portöffnung und bei ausschliesslichem Browserfehler einen eng begrenzten Zweitabruf nach gesichertem Erst-HAR. Firmware-/Safety-Anomalien blieben harte Stopps. Bekannte Favicon-404 und erwartete anonyme/abgelaufene Session-401 waren separat zu behandeln. Reale Aktoren bleiben getrennt; der Owner bestätigte Display/Touch ohne Aktoren.

Windows-PC per LAN, ESP32 per WLAN im selben lokalen Netz; **Google Chrome 155.0.8059.85, offizieller 64-Bit-Build**, auch durch NetLog bestätigt. Vor der Portöffnung bestätigte der Owner die tatsächlich vorhandene private NetLog-Datei, geschlossene bekannte Geräteclients, den leeren Network-Tab mit Aufnahme/Preserve log sowie die HAR-Exportbedienung. Der NetLog läuft im Modus `Default`; HAR/NetLog enthalten Daten vor, während und nach dem Abruf. Es gab keinen Login und keine Mutation.

## PASS / FAIL / NOT_PROVEN

| Gegenstand | Ergebnis und Grenze |
|---|---|
| Neuer Boot / Source / Release / Aktortrennung / ready / HomeConnected | **PASS**, [Bootauszug](UART_BOOT_PROOF.txt), [Capture](CAPTURE.json) |
| Mindestens 1200 s Ruhephase vor Startsignal | **PASS**, 1216,423 s monoton vor dem Startsignal; bekannte Clients geschlossen und keine aktiven Board-Probes. Andere unbekannte Clients nicht unabhängig ausgeschlossen |
| Einziger Erstabruf `GET /` | **PASS_OBSERVED**, HAR und NetLog: 200, 336,196 ms, Body vollständig |
| Wiederholung des historischen Erstfehlers | **NOT_REPRODUCED**, kein `ERR_INCOMPLETE_CHUNKED_ENCODING` in diesem Dokumentabruf |
| Sofortiger Diagnose-Zweitabruf | **NOT_RUN / NOT_APPLICABLE**, Erstabruf erfolgreich; zwei HAR-Exporte sind keine zwei Navigationen |
| UART-Fatal-/Reset-/Heartbeat-Prüfung im Capturefenster | **PASS**, keine Guard-Auslösung; sechs nicht fatale HTTPD-Empfangswarnungen bleiben Befund |
| Anonyme Polling-Gruppen im erweiterten HAR | 67 kurze 401-/Abbruchgruppen ohne sichtbare Überlappung; keine Qualifikation erfolgreicher/authentifizierter JSON-Snapshots |
| Historische Fehlerursache / Funkursache / EAGAIN / Socket-Recycling als Fehlerursache | **NOT_PROVEN** |
| Exakte Windows–Host–UniFi-Uhrabweichung / gemeinsame Socketkennung | **NOT_PROVEN**; nominale Zeitassoziation ist kein Kausalitätsnachweis |
| Heapspitzen beim Abruf / HTTPD-Stack / Recovery / F4-Hold-/Skip-Nachweise | **NOT_PROVEN**, keine passenden Messpunkte in diesem reinen Produktimage-Lauf |

## Erstabruf und Browsertrace

Client-Zeit: **17.10.2026 07:49:30,648 UTC**, entsprechend **09:49:30,648 Europe/Zurich**, nach der am Host von 07:28:07,233562 UTC an gemessenen Ruhephase. Das Startsignal wurde erst nach 1216,423 s erteilt. Der genaue Abstand vom Startsignal bis zum tatsächlichen Browserstart ist wegen der nicht unabhängig gemessenen Uhrabweichung nicht als präzise hostübergreifende Messung qualifiziert. NetLog erfasst vor diesem Zeitpunkt keinen zum Board gehörenden `URL_REQUEST_START_JOB`.

| HAR-Phase | Dauer ms |
|---|---:|
| Blocked / Queueing | 1,265 |
| DNS | 0 |
| Connect | 116,945 |
| Send | 0,364 |
| Wait / TTFB nach Request-Send | 94,941 |
| Receive | 122,681 |
| Gesamt | **336,196** |

[HAR_SUMMARY.json](HAR_SUMMARY.json) enthält die vollständigen freigegebenen Timingfelder. Antwort `HTTP/1.1 200`, `Content-Type: text/html; charset=utf-8`, `Transfer-Encoding: chunked`; 87 Headerbytes, 8045 übertragene Bodybytes einschliesslich Framing, insgesamt 8132 B. Dekodierter HTML-Body **8025 B**, endet mit `</html>`. HAR dokumentiert keinen Root-Fehler; NetLog bestätigt 8025 gefilterte HTML-Bytes sowie reguläres Ende von `REQUEST_ALIVE`. Die Metadaten sind ein Beleg des vom Browser akzeptierten Antwortabschlusses. **Ein byteweiser Nachweis des terminalen Nullchunks ist nicht enthalten:** Default-NetLog enthält Byteanzahlen, keine vollständigen Wirebytes.

Die beiden privaten HAR-Dateien sind ein Initialexport mit **16 Einträgen** und ein erweiterter Export mit **202 Einträgen**. Die ersten 16 Einträge sind als JSON-Werte vollständig identisch. In beiden steht derselbe einzige Dokumentabruf mit identischer Startzeit, Dauer und Bodyhash; **kein zweiter `GET /`, kein POST**. Der Initialexport enthält Folgepolls bis 07:49:51 Client-UTC; sein genauer Exportzeitpunkt wurde nicht separat protokolliert. Die ursprünglichen Exporte bleiben unverändert erhalten.

Nach dem Root folgte bis zum Tab-Schluss normales anonymes 5-s-Polling. Der erweiterte HAR enthält 67 Dreiergruppen: 93 Antwortheader mit 401 und 108 Einträge ohne empfangenen HTTP-Status (`status=0`); alle 201 Fetch-Einträge tragen `net::ERR_ABORTED`. Der Produktcode auf `a9a1156` verwirft `!a.ok`, abortiert dann den gemeinsamen Controller und sammelt Geschwister mittels `allSettled` ein ([rf(), Zeile 202 ff.](../../../lib/fermentation_app/src/web_application_routes.cpp#L202)). Diese Abbrüche sind mit diesem erwarteten anonymen Pfad konsistent und kein Beleg eines fehlgeschlagenen Root-Abrufs. Maximale beobachtete Gruppendauer 335,823 ms, kleinster Abstand zum nächsten Gruppenstart 4662,177 ms; kein Überlappen in diesen kurzen Gruppen. Daraus folgt keine Last-, Deadline- oder JSON-Snapshotqualifikation.

Der weiterlaufende NetLog enthält **205 Board-GETs**: einmal `/`, 68-mal je `status`, `temperatures`, `alerts`. Die zusätzliche letzte Dreiergruppe begann um 07:55:06 Client-UTC nach dem erweiterten HAR. 94 API-Requests erhielten 401-Header; 110 endeten ohne Antwortheader mit `CANCELLED`. Kein Favicon-Request ist im aktuellen Boardtrace enthalten; die Ausnahme kam nicht zur Anwendung. Die 111 unterschiedlichen gebundenen Socket-IDs sind über die gesamte Tracelebensdauer gezählt, **keine gleichzeitige HTTPD-Socket- oder TX-Pufferanzahl**.

## UART, Verbindungen und UniFi

Capture **07:27:41,930685–08:03:52,841528 UTC** / **09:27:41–10:03:52 CEST**, Dauer 2170,911 s. Genau ein gültiger initialer `POWERON_RESET`, 137 unqualifizierte Vor-Boot-Zeilen, danach 2163 qualifizierte Heartbeats mit streng wachsender Uptime. Capture-Maximallücke inklusive Boot 3,454311 s; grösste Lücke zwischen qualifizierten Heartbeats 1,310186 s. Kein zweiter Reset, Panic/Abort/WDT/Brownout, Heap-Fatalmarker oder Stackoverflow im aufgezeichneten Fenster. Nach Owner-Bestätigung des geschlossenen Tabs/gestoppten NetLogs wurde ausschliesslich die vorhandene Capture per STOP-Marker geschlossen (Exit 0, `failure=null`). Kein erneuter Portzugriff. Der UART-interne Abschlussstatus ist kein Gesamt-G2-PASS.

[NETLOG_ROOT.json](NETLOG_ROOT.json) weist für den Root einen **neuen TCP-Connect** auf Socket-ID 227267 nach. Der Request erhielt den Socket über den Streamjob 227271; im Rootjob steht kein `SOCKET_POOL_REUSED_AN_EXISTING_SOCKET`. Nach diesem erfolgreichen Root wurde derselbe Socket auch für spätere API-Abrufe verwendet. Der neue Rootconnect liefert keinen Hinweis auf ein zuvor über 20 min gehaltenes, altes Browser-Keep-alive-Socket als Ursache dieses Abrufs. Ob beim historischen Fehler ein anderer Socketzustand vorlag, ist offen.

Im aktuellen UART steht **kein `error in send : 11`**. Es gibt sechs `httpd_sock_err: error in recv : 104`, vollständig mit umgebenden Heartbeats in [UART_HTTP_WARNINGS.txt](UART_HTTP_WARNINGS.txt). Nominale Host-UTC: 07:50:11,185; 07:51:51,215; 07:52:16,266; 07:53:16,213; zweimal 07:54:41,333. [CORRELATION.json](CORRELATION.json) zeigt die nächsten aufgezeichneten Browser-Socket-Schliessereignisse mit nominal 9,862–39,562 ms Abstand. Das ist **konsistent mit** browserseitigem Abbruch/Verbindungsrecycling während der anonymen 401-Abfragen. Es ist **keine** eindeutige Zuordnung: gemeinsame Socketkennungen im UART fehlen, zwei Warnungen können denselben nächsten Browsermarker haben, und die Windows–Host-Uhrabweichung wurde nicht unabhängig gemessen. Die Warnungen belegen weder EAGAIN noch die Ursache des früheren Chunk-Abbruchs.

UniFi-Originalscreenshots bleiben privat; [UNIFI_OBSERVATIONS.json](UNIFI_OBSERVATIONS.json) enthält nur bereinigte Werte. Kanal 6 / 2,4 GHz / 20 MHz, Client-Signal −72 und später −71 dBm, Client-TX-Retries 0 %, AP-Retries 16 bzw. 10 %. Angezeigter Verbindungsbeginn unverändert 09:01:40. Die Anzeige enthält historische Kanaländerung/Disconnect/Reconnect um 09:01:23–09:01:40, **vor** dem kontrollierten UART-Boot. „Verbunden seit“ ist kein Nachweis lückenloser Verbindung über den Boot hinweg. Screenshot-Erfassungszeiten und UniFi-Zeitzone sind nicht exakt qualifiziert. Diese Funkwerte erklären keinen historischen Fehler.

Der Host meldete NTP-Synchronisierung. Windows meldete manuell `2026-10-17T07:26:05.3613506Z`; Nachrichtenlaufzeit wurde nicht als Uhrenabweichung interpretiert. Browserzeiten werden aus HAR bzw. NetLog-`timeTickOffset` abgeleitet. UART-Zeiten sind Hostempfang/Parsing, mit dokumentierter Epoch-/Monotonic-Abbildung in [TIMELINE.json](TIMELINE.json). Die vollständige exakte Synchronisierung aller drei Quellen ist **NOT_PROVEN**; insbesondere werden die genannten nominalen Millisekundenabstände nicht als gemessene Uhrengenauigkeit ausgegeben.

## Ressourcen und Messgrenzen

Alle sechs [Ressourcenstichproben](UART_RESOURCES.txt) liegen innerhalb der ersten etwa 122 Bootsekunden, **vor dem Erstabruf**. Letzte Probe: freier Heap 42 624 B, grösster interner 8-Bit-Block 32 768 B, Boot-Minimum 32 592 B, Main-HWM 11 704 B. Kleinster erfasster Main-HWM 11 704 B; `g2-trend` meldete keine Unterschreitung von 11 160 B. **Keine** kontinuierliche Stackmessung, HTTPD-Stackprobe, Heapspitze am Sendepfad oder qualifizierte Recovery. Das Boot-Minimum wird nicht als aktuelle Erholung verwendet. Historisches `min_free=5228 B` sowie lange GET-Dauern bleiben offene frühere Befunde. Es gibt keine nachträgliche neue Latenzschwelle.

## Provenienz, Empfehlung und Übergabe

Review-Einstieg ist dieser Bericht; [SHA256SUMS](SHA256SUMS) bindet alle übrigen öffentlichen Paketdateien. [ARTIFACT_MANIFEST.json](ARTIFACT_MANIFEST.json) bindet die privaten Originale und Quell-/Imagehashes. [derive_evidence.py](derive_evidence.py) ist ausschliesslich die offline ausführbare, allowlistbasierte Ableitung für dieses Evidence-Paket; sie öffnet weder UART noch Netzwerk und ändert keine Eingangsdaten. Die Ableitung lief auf den Originaldateien, mit überprüften Prefix-/Navigations-/Body-/Heartbeat-Invarianten. JSON-, Link-, Manifest-, Secret- und Diffprüfung sind Bestandteil des Builder-Checks; keine erneuten Firmwarebuilds oder Software-/Hardwaretestläufe.

Öffentlich enthalten sind ausschliesslich freigegebene Requestpfade, Zeit-/Status-/Fehlerdaten, sichere Inhaltsheader, numerische NetLog-IDs und Byteanzahlen sowie ausgewählte UART-Zeilen. Vollständige HAR-/NetLog-/UART-Rohdaten, Browserkommandozeile, Header/Bodies mit möglichen Credentials, private Pfade, Netzwerkkennungen, Readbacks und Rückfallbinärdateien bleiben lokal geschützt. UART-Auszüge sind LF-normalisiert; Auslassungen markiert. Capture-Zusammenfassung und Chunkmetadaten sind bytegleich übernommen. Unabhängige private Rohdatenprüfung kann durch öffentliche Ableitungen nicht ersetzt werden.

**Minimale Empfehlung: kein Produkt-/WLAN-/TCP-/HTTPD-Fix aus diesem Versuch.** Bewiesen ist ein erfolgreicher neuer Erstconnect nach der Ruhephase; die angestrebte Fehlerreproduktion und damit eine begründete Root-Cause-Korrektur fehlen. EAGAIN, Funkstörung, HTTPD-Sendetimeout oder altes Browser-Keep-alive bleiben für den historischen Fehler unbewiesen. Ein weiterer Versuch ist nicht automatisch freigegeben. Falls der Owner später eine weitere gezielte Reproduktion anordnet, fehlt konkret ein **fehlgeschlagener** Root-Abruf im vollständigen Clienttrace, eine unabhängig bestimmte Windows–Host–UniFi-Uhrabweichung und eine Zuordnung des serverseitigen Sendefehlers zu genau dieser Verbindung/Transferphase. Erst daraus lässt sich ein enger Fix ableiten; weder grössere Puffer noch geänderte WLAN-/HTTPD-Timeouts werden vorsorglich vorgeschlagen.

**Independent Ergebnisreview: PENDING.** Kein weiterer Abruf, Portzugriff, Reset, Flash, automatischer Folgelauf, G2-Abschluss oder Pre-Ready.

`FIRST_GET_DIAGNOSIS=COMPLETED_NOT_REPRODUCED` · `ROOT_CAUSE=NOT_PROVEN` · `G2_WEB=FAIL/HOLD` (historisch unverändert) · `B4_RECONNECT=FAIL_OBSERVED` · `G1_QUALIFICATION=NOT_PROVEN` · `F4_THRESHOLD=PROVISIONAL_PENDING_G1` · `RES-01=BLOCKER` · `PR203_FINAL_GO=HOLD` · `ACTUATOR_RELEASE=NO`
