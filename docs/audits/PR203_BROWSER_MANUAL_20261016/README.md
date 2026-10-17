# PR #203 / RES-01 – Manueller Browser-Fernnachweis, 2026-10-16

**Gesamtergebnis: `BROWSER_MANUAL=HOLD`.** Die Wiederverbindung in B4 war zunächst nicht erreichbar. Der Screenshot sichert eine fehlgeschlagene Dokumentanfrage (`net::ERR_AD…`, vollständiger Text abgeschnitten) und mehrere abgebrochene Dokumentanfragen. Nach manuellem Neuaufruf/Neuladen erschien die Loginseite mit HTTP 200. Der Owner bestätigte anschließend den geschlossenen ESP32-Tab und keine weiteren Geräteaktionen. **Die spätere Erreichbarkeit hebt den vorherigen Fehler nicht auf; B4 bleibt HOLD.** Kein B4-Login oder anschließendes 10-min-Polling wurde durchgeführt/nachgewiesen.

## Stand, Freigabe und Grenzen

- Produkt-PR #203 offen/Draft, Branch `agent/issue-27-r1-web-dashboard-plan`, Produkt-PR-HEAD bei diesem manuellen Lauf `f7eddf526541b603c7a8f9e2978d027effd2354d` (Dokumentation); Produkt-Firmware C5 `162a6670526a10b2fd957fe90ab665a1fae837b7`.
- Freigegebene Planrevision `8ce57d78f1544728bf1868d965551d3de7c302f0`; Ownerentscheide Stack A, F4 A und G2-Web partial in `0374797`. F1 bleibt; O1 ist auf TCP-Default 5760 zurückgebaut. F4 unverändert **13 312 B Skip / 16 640 B Resume**, `PROVISIONAL_PENDING_G1`; keine OOM-Sicherheitszusage.
- Letztes vorhandenes Flash-/Readback-/Boot-Identitätsartefakt bestätigt das C5-Produktimage: App `ebb296c08b79045e6e081f328c9f5fd7e576e20722670968b131e722a8b36633`, ELF laut gepinntem Gate `45bc809ed67b19d49f1f6bcc89386c8cfc83f3e7e103729e237293cfac5403e3`. Keine neue Geräteidentifikationsabfrage. [Vorhandene Identitätsevidence](../PR203_BROWSER_REMOTE_20261016/IMAGE_IDENTITY.json).
- Nach dem früheren [B0-Stop am Favicon-404](../PR203_BROWSER_REMOTE_20261016/README.md) genehmigte der Owner ausdrücklich **einen neuen manuellen B0–B4-Lauf**, bei dem ausschließlich der bekannte Favicon-404 als dokumentierter Nebenressourcenbefund zulässig ist; erwartete anonyme/abgelaufene Session-401 werden getrennt erfasst. Alle anderen unerwarteten Fehler führen zum Stop. Diese Ausnahme gilt **nicht** für die B4-Dokument-/Transportfehler. Der frühere Lauf bleibt HOLD.
- Testbrowser laut Owner: **Google Chrome 154.0.8037.97 (Official Build, x86_64)** im Selkies-Docker auf dem Dev-Host, direktes Heimnetz ohne VPN, ein ESP32-Testtab. Die Versionsnummer ist Owner-Angabe, nicht unabhängig aus dem Browserprozess ausgelesen. Screenshots zeigen Chrome und die reale Geräteoberfläche.
- B0 wurde vom Owner als bereit bestätigt (andere bekannte ESP32-Clients sollten geschlossen sein). Ein vollständiger Clientbestand und der Ausschluss unbekannter Clients sind **nicht unabhängig belegt**. UniFi-Beobachtungen liegen als Owner-Screenshots vor. Keine weiteren Agentenbrowser oder Lasttreiber wurden gestartet.
- Keine Firmware-/Konfigurations-/Schwellenänderung, kein Neubuild, kein Flash/Reset, keine neue UART-Portöffnung, kein Programm-/Service-/Aktorzugriff. Der Agent las ausschließlich bereits bestehende Capture-Dateien. Die bestehende UART-Aufzeichnung blieb unverändert in Betrieb.

## Zeitbasis und tatsächlicher Ablauf

UTC ist die primäre Hostzeit; am 16.10.2026 entspricht Europe/Zurich UTC+2. **Host-Referenzmarken stammen vom Empfang/Auswerten der Ownernachrichten und sind keine sekundengenau gemessenen Touch-/Browseraktionen.** Die Bildschirmuhren zeigen nur Minuten. Die Verlängerungen werden erhalten, nicht nachträglich auf Sollzeiten gekürzt.

| Phase | Referenz / sichtbare Zeit | Tatsächliches Ergebnis |
|---|---|---|
| Freigabe / Vorbereitung | 11:57:53.882 UTC / 343078.844062 s; B0-bereit empfangen 12:04:12.801 UTC | Owner genehmigt manuellen Folgelauf; Browser/Netzwerk vorbereitet. |
| B1a vor Login | Screenshot empfangen 12:07:35.990 UTC | Loginseite, leeres Passwortfeld, Shell 200 / 1,63 s; sichtbare API-401 vor Login erwartet. Zähler 34 Requests / 33 Konsolenfehler. Kein vollständiger Trace. |
| B1b Login | Bildschirmuhr 14:09, empfangen 12:11:42.876 UTC | Login 200 / **2,82 s**, Reload-Shell 200 / **1,53 s**. Sichtbare API-200; `programs` 200 / 154 ms. Online / Ready, drei Temperaturen `-- Stale`, Alerts 0. |
| B2 offen | Referenz 12:11:42.876–12:37:53.038 UTC; **1 570,162 s = 26 min 10,162 s** zwischen Empfangsmarken statt geplanter 900 s | Owner-Screenshots bei sichtbarer Uhr 14:16, 14:24 und 14:35: Online / Ready, Temperaturen weiter `-- Stale`, Alerts 0, sichtbare fertige API-Antworten 200. Kein Programm gestartet. Exakte Browserexposition nicht separat vermessen. |
| B2 Zwischenpunkt 1 | Screenshot empfangen 12:18:20.051 UTC, sichtbare Uhr 14:16 | 332 Requests; Konsolenzähler 150. Sichtbare erfolgreiche Antworten **8,23 / 10,99 / 12,44 s**. |
| B2 Zwischenpunkt 2 | Empfangen 12:27:02.477 UTC, sichtbare Uhr 14:24 | Verspäteter +10-min-Punkt; 575 Requests, Konsolenzähler 150; sichtbare API-200 mit 234–609 ms. |
| B2 Abschlussbild | Empfangen 12:37:53.038 UTC, sichtbare Uhr 14:35 | 773 Requests, Konsolenzähler 150. Sichtbare erfolgreiche Statusantworten **23,99 s und 22,51 s**; weitere erfolgreiche Antworten bis 12,06 s; zwei Requests noch `pending`. Pending ist kein nachgewiesener Timeout. |
| B3 Ruhe | Startanweisung empfangen 12:37:53.038 UTC / 345477.999862 s; Ende empfangen 13:02:48.325 UTC / 346973.287297 s | **1 495,287 s = 24 min 55,287 s** zwischen Referenzmarken statt geplanter 1 200 s. Owner erklärt Ruhephase beendet; exakter Tab-Schließzeitpunkt und vollständige HTTP-/Socketfreiheit sind nicht unabhängig gemessen. |
| B4 Wiederverbinden | Screenshot-Uhr 15:06; Bericht empfangen 13:08:13.888 UTC | Owner: zuerst Seite nicht erreichbar, danach beim Versuch der Screenshotaufnahme erreichbar. Netzwerk zeigt Fehlanfrage und mehrere Cancellations, dann Shell 200 / 4,53 s und anonyme API-401. **HOLD**. |
| Stopbestätigung | 13:09:38.627 UTC / 347383.589257 s | Owner bestätigt ESP32-Tab geschlossen, keine weiteren Geräteaktionen und manuelles Neuladen/erneutes Aufrufen nach erstem Fehler. Kein weiterer Versuch. |

Die tatsächlichen B2-/B3-Dauern sind dokumentierte manuelle Ablaufabweichungen. Die 150 Konsolenfehler waren bereits beim ersten Loginbild vorhanden und blieben in den B2-Screenshots gleich; die sichtbaren früheren 401 stehen vor dem Login. **Das beweist nicht, dass jeder einzelne gespeicherte Konsoleneintrag erwartet war oder alle Requests erfolgreich waren.** Ein Roh-HAR beziehungsweise vollständiges Browser-Netzwerkprotokoll steht nicht zur Verfügung. Deshalb keine vollständige Erfolgsquote, keine p95/p99 und keine erfundene vollständige Requestzählung. Lange erfolgreiche Antworten sind Befunde; es wurde keine nachträgliche 3-s-Gesamtdauergrenze für diesen nativen Browserlauf eingeführt.

## B4-Fehler und Stopablauf

Der letzte Screenshot zeigt in der Geräte-Dokumentliste:

| Sichtbare Reihenfolge | Ergebnis | Angezeigte Dauer |
|---|---|---:|
| 1 | `canceled` | 2,04 s |
| 2 | `failed`, `net::ERR_AD…` | 3,08 s |
| 3 | `canceled` | 2,03 s |
| 4 | `canceled` | 3,08 s |
| 5 | HTTP 200, Loginseite | 4,53 s |

Dazwischen sichtbare `data:image/...`-200-Zeilen aus `chrome-error://chromewebdata/` sind **browserinterne Fehlerseiten-Assets**, keine erfolgreichen ESP32-Antworten. Der vollständige Fehlertext ist abgeschnitten; `ERR_ADDRESS_UNREACHABLE` oder eine andere konkrete Fehlerklasse wird **nicht geraten**. Die Dauerangaben sind Browseranzeigen, keine nachgewiesenen 3-s-Socket-Timeouts. Die Cancellations werden nicht pauschal als Firmware-/Serverabbrüche bezeichnet.

Der Owner bestätigte auf Nachfrage **manuelles Neuladen beziehungsweise erneutes Aufrufen** nach der ersten Nichterreichbarkeit. Damit wurde die vorgesehene Stopgrenze bereits vor dem Bericht überschritten; **kein Claim „Stop beim ersten Fehler, kein Retry“** für die reale B4-Bedienung. Anzahl, genaue Zeit und Ursache jedes einzelnen Abbruchs bleiben offen. Nach Eingang des Befunds gab der Agent sofort die Anweisung zum Schließen ohne Login/Reload; der Owner bestätigte sie. Der Agent führte keine zusätzliche Geräteanfrage, Wiederholung oder Hardwareaktion aus.

Die 401 nach dem späteren Shell-Erfolg sind vor Login nach längerer Inaktivität erwartbar. Die Anzeige `Offline` vor Login beweist für sich keinen WLAN-Ausfall. Beides entkräftet den separat sichtbaren fehlgeschlagenen Dokumentaufruf nicht. Die erste Fehlerseite musste nicht erneut reproduziert werden: Der erhaltene Netzwerk-Screenshot sichert den Fehler trotz fehlender Aufnahme des ursprünglichen Fehlerbildschirms.

## UniFi-Umgebung

Alle Bilder ordnen den Client dem AP **AC Pro Wohnzimmer**, Kanal **1**, 2,4 GHz / 20 MHz, MIMO 1×1 zu. Die Original-Aufnahmezeit fehlt meist; Reihenfolge und Bezug stammen aus dem Chat. AP-/Client-Retries haben verschiedene Bezugsgrößen.

| Beobachtung | Signal | Client-TX-Retries | AP-TX-Retries | AP-Airtime |
|---|---:|---:|---:|---:|
| Anfangsbild | −86 dBm | 0 % | 27 % | 31 % |
| Loginbild | −86 dBm | 44 % | 36 % | 31 % |
| B2 erster Zwischenpunkt | −87 dBm | 46 % | 30 % | 32 % |
| B2 zweiter Zwischenpunkt | −87 dBm | 46 % | 42 % | 34 % |
| B2 Ende / B3-Start | −87 dBm | 46 % | 42 % | 34 % |
| B3 Ende | −87 dBm | 46 % | 13 % | 32 % |
| B4 Fehlerbericht | −84 dBm | 0 % | 11 % | 31 % |

Externe Interferenz jeweils 0 % angezeigt. Im sichtbaren Verbindungsereignis-Ausschnitt stehen historische Einträge vom 15.10., letzter sichtbarer Connect 22:23:30; keine vollständige Controller-Ereignishistorie für den Fernlauf liegt vor. Die Site-Health-Anzeigen 0 ms / 0 % belegen keine ESP32-HTTP-Latenz oder Fehlerfreiheit. **Keine RSSI-Abnahmegrenze und kein Kausalnachweis**, weder „Funkursache“ noch „ESP-IDF/lwIP-Defekt“. Keine neue Netzwerk-/F2-Kampagne durchgeführt.

## Vorhandene UART-Evidence und Messgrenzen

Ein eingefrorener Ausschnitt aus der schon seit dem Vortag laufenden G2-Web-UART-Aufzeichnung deckt die Host-Referenzen **11:57:53.882–13:09:38.627 UTC** ab. Innerhalb dieses Fensters:

- **4 294 Heartbeats**, maximaler Abstand **1,357436 s**, kein Uptime-Rücksprung.
- Kein erfasster `heap_alloc_failed`-, Panic-/Abort-/Watchdog-/Brownout-/Resetmarker.
- **Keine aktuellen Heap-/Largest-Block-/Stack-Stichproben.** Keine vergleichbare `R_ref`; damit weder 90-%-Erholung noch Stackreserve oder Regelungsqualifikation belegt.
- 13 sonstige Informationszeilen `wifi:[ADDBA]RX DELBA, reason:39 ...`; sie sind als vorhandener Kontext enthalten. Sie fallen in frühere Referenzphasen und belegen nicht die Ursache des späteren B4-Fehlers. Keine Interpretation als Reset oder erwiesener Treiberdefekt.
- B3-Unterfenster: 1 491 Heartbeats, größter Abstand 1,308082 s, ebenfalls keine Ressourcenzeile. Die UART belegt keine vollständige HTTP-/Socketfreiheit während B3.

Die Markerfreiheit gilt **nur für das erhaltene UART-Intervall**. Kein allgemeiner OOM-Sicherheits-, autonomer Regelungs- oder Gesamt-R1-U-PASS. Es gab keinen frischen Boot im Fernauftrag; die bestehende Aufzeichnung wurde nicht geöffnet, beendet oder verändert.

## PASS / FAIL / NOT_PROVEN

| Gegenstand | Ergebnis und praktische Grenze |
|---|---|
| B1 Login und authentifizierte Startseite | **PASS – beobachtete Requests/Owner-Screenshots**; Login 200, sichtbare APIs 200; kein vollständiger Trace. |
| B2 sichtbarer Online-/Ready-Zustand und Polling-Antworten | **PASS – beobachtete Stichproben**; Standby ohne Programm, verlängertes Referenzfenster. |
| Vollständige HTTP-Fehlerfreiheit / 100 % 2xx über B2 | **NOT_PROVEN**, keine vollständigen Browserlogs; anfängliche Auth-401 separat. |
| B3 Ruhephase | **Owner-beendet**, mindestens 20 min zwischen Referenzmarken; exakte Tab-/Socket-/Clientfreiheit **NOT_PROVEN**. |
| B4 initialer Dokument-/Erreichbarkeitsfehler | **FAIL – konkreter Browserbefund**; exakte Transportklasse/Ursache offen. |
| B4 Stop beim ersten Fehler ohne Wiederholung | **FAIL – Ablaufabweichung**, Owner bestätigt manuellen Neuaufruf; spätere Stopbestätigung vorhanden. |
| B4 später erreichbare Loginseite | **PASS – einzelner späterer Shell-200-Befund**, hebt Fehler nicht auf. |
| B4 Login und 10-min-Normalbetrieb | **NOT_PROVEN / NOT_RUN nach Stop**. |
| UART-Heartbeat-/Markerprüfung | **PASS für das erhaltene Intervall**, kein Uptime-Rücksprung oder entsprechender Fehlermarker. Keine Aussage außerhalb des Fensters. |
| Aktuelle Heap-/Block-Erholung, Stackreserve, autonome Regelung | **NOT_PROVEN**. |
| F4-Bedarf, finale Schwelle, reales Touch/Hold/Skip/Resume | **NOT_PROVEN**; vorläufige Schwelle unverändert. |
| Vollständiges G2 / R1-U | **NOT_PROVEN / BLOCKED**, gültige Sensor-/Commissioning-/Programmstart-/R_ref-Voraussetzungen fehlen. |
| Gesamter Browserteilnachweis | **HOLD**, weil Wiederverbindung zunächst fehlschlug. Kein `PASS_PARTIAL` als Gesamtergebnis. |

**Konkreter Abnahmeblocker: ja – Wiederverbindungs-/Erreichbarkeitsfehler im realen Einzelbrowserablauf.** Ein bestimmter codepflichtiger Softwaredefekt ist damit **nicht lokalisiert**. Die fehlenden Sensorwerte wurden als `-- Stale` dargestellt, nicht durch künstliche gültige Temperaturen ersetzt; kein Programmstart und keine Aktorfreigabe. Die unveränderten Meldungen `Alerts: 0` sind ein Anzeige-/Screenshotbefund, kein vollständiger Safety- oder Sensorvalidierungsnachweis.

**Empfehlung: PR #203 weiterhin HOLD; zuerst unabhängiger Ergebnisreview.** Keine automatische Wiederholung, kein neuer Flash, kein G2-Vollnachweis, kein Pre-Ready, keine F2-/WLAN-/TCP-/HTTPD-/Schwellenänderung. Offline sinnvoll: Review dieses vorhandenen Befunds und gegebenenfalls Auswertung bereits vorhandener privater Browserlogs, falls solche zugänglich werden; keine neue Root-Cause-Kampagne automatisch auslösen. Hardware-/Gate-Auslagerung aus PR #203 bleibt eine gesonderte Ownerentscheidung.

**Physisch beziehungsweise mit gültigem Testaufbau offen:** G1′ reale Render-/Touch-/Hold-/Skip-/Resume-Pfade und belastbare F4-Qualifikation; G2/R1-U auf finalem reinem Produktimage mit gültigen Sensor-/Commissioning-Werten, Programmstart, Geräte/Web-Vergleich, frischem freigegebenem Boot, kontinuierlichen Ressourcen-/Stack-/Fehlernachweisen und vergleichbarer `R_ref`/Erholung. Historische G1 IMAGE01-, G1P-2- und F2-FAIL/HOLD-Ergebnisse werden nicht verändert. `C0=GO` bleibt erhalten.

## Review-Evidence und Datenschutz

- [OWNER_LOG.json](OWNER_LOG.json): Owner-Bestätigungen, Screenshottranskripte, Zeitmarken und deklarierte Freigabegrenzen.
- [TIMELINE.json](TIMELINE.json): Host-Referenzzeiten, geplante/tatsächliche Phasendauern, Stop und bestätigter manueller Retry.
- [HTTP_SCREENSHOT_OBSERVATIONS.json](HTTP_SCREENSHOT_OBSERVATIONS.json): beobachtete HTTP-/UI-Ausschnitte; ausdrücklich **kein** vollständiger Netzwerk-Trace.
- [UNIFI_OBSERVATIONS.json](UNIFI_OBSERVATIONS.json): getrennte Client-/AP-Werte und Unsicherheiten.
- [UART_HEARTBEATS.csv](UART_HEARTBEATS.csv): vollständige Heartbeat-Ausleitung innerhalb des eingefrorenen Referenzfensters.
- [UART_OTHER_RECORDS.txt](UART_OTHER_RECORDS.txt) und [UART_WINDOW_SUMMARY.json](UART_WINDOW_SUMMARY.json): übrige Originalinhalte mit normalisierten Zeilenenden, Quelle und Prüfgrenzen.
- [PROVENANCE.json](PROVENANCE.json) / [SHA256SUMS](SHA256SUMS): Quellen, Transformationen, Grenzen und öffentliche Prüfsummen.

Die Screenshotoriginale wurden inline im Chat übermittelt; im Workspace waren keine zugehörigen Bilddateien verfügbar. **Keine Veröffentlichung oder Hashbindung dieser Originalbilder wird behauptet.** Die öffentlichen Transkripte sind dadurch in ihrer unabhängigen Bildprüfung begrenzt; die Bilder bleiben im Gespräch erhalten. Es werden keine Screenshots nachgebaut. Private Rohlogs/-auszüge bleiben lokal unverändert. Keine Passwörter, Cookies, CSRF-/Auth-Tokens, Header/Bodies, HAR, Browserprofile oder vollständigen Flash-Readbacks im öffentlichen Paket.

`BROWSER_MANUAL=HOLD` · `B4_RECONNECT=FAIL_OBSERVED` · `G2_WEB=HOLD_INCOMPLETE` · `G1_QUALIFICATION=NOT_PROVEN` · `F4_THRESHOLD=PROVISIONAL_PENDING_G1` · `RES-01=BLOCKER` · `PR203_FINAL_GO=HOLD` · `ACTUATOR_RELEASE=NO`.
