# PR #203 / RES-01 – Browser-Fernnachweis 2026-10-16

**BROWSER_REMOTE=HOLD.** Der einmalige reale Browserversuch wurde in B0 beim ersten unerwarteten HTTP-Fehler gestoppt: Chromium forderte automatisch `/favicon.ico` an; das Produkt antwortete mit **404**. Shell **200** und drei erwartete anonyme API-**401** sind getrennt erfasst. **Kein Login wurde gesendet; B1–B4 wurden nicht ausgeführt.** Keine Wiederholung, Firmwareänderung, UART-Portöffnung, Reset- oder Flashaktion durch diesen Auftrag.

## Verifizierter Stand

- Live-PR #203: offen/Draft, Branch `agent/issue-27-r1-web-dashboard-plan`, HEAD bei Testbeginn `0374797d1bd15bd72363409174487d44d8188f6b`, Arbeitsbaum sauber.
- Freigegebene Planrevision laut Ownerauftrag: `8ce57d78f1544728bf1868d965551d3de7c302f0`, SSOT `docs/tasks/issue-27-res01-heap-hardening-plan.md`. Commit `0374797` ergänzt die Ownerentscheide Stack A, F4 A und einmaliges G2-Web partial. Die älteren Pending-Texte im Plan und Handover sind nicht die aktuelle Freigabegrundlage; sie werden hierdurch nicht zu Testergebnissen umgewertet.
- Der Auftrag vermutete als letztes Image IMAGE01. **Vorhandene jüngere Evidence belegt C5:** gespeicherter App-only-Flash vom **2026-10-15 20:15:20–20:17:01 UTC**, esptool rc 0 und Hashverifikation erfolgreich; nachfolgender privater 4-MiB-Readback mit rc 0. Offline erneut berechneter SHA-256 des App-Prefixes (1 756 528 B ab `0x10000`) ist exakt `ebb296c08b79045e6e081f328c9f5fd7e576e20722670968b131e722a8b36633`.
- Produktquelle C5: `162a6670526a10b2fd957fe90ab665a1fae837b7`, `esp32_release`; ELF laut gepinntem G2-Web-Gate `45bc809ed67b19d49f1f6bcc89386c8cfc83f3e7e103729e237293cfac5403e3`. Der ELF wurde in diesem Fernauftrag nicht neu gebaut oder erneut auf dem Gerät geprüft. Gegenüber `d35ee85` gibt es in `162a667` nach O1-Rückbau keine verbleibende Produkt-/Konfigurationsdifferenz; eingebettete Versions-/Buildidentität ist C5.
- Die seit **2026-10-15 20:23:48 UTC** bereits laufende G2-Web-UART-Aufzeichnung enthält `App version: 162a667`, volle Quell-SHA und kontinuierliche aktuelle Heartbeats. Der Prozess wurde nur hostseitig erkannt; vorhandene Dateien wurden gelesen. **Keine neue serielle Verbindung und keine frische Flash-Identifikationsabfrage.** Zuletzt nachgewiesenes installiertes Image ist damit C5, nicht IMAGE01. Ein vollständiger früherer G2-Web-Browser-Ergebnisbericht liegt in den geprüften Artefakten nicht vor.
- Browser: **Chromium 149.0.7827.55**, realer Headless-Browser, 1280 × 900, `de-DE`, genau eine neue Browserinstanz und ein Geräte-Tab, vorhandenes lokales CDP-Werkzeug. Verbindung über vorhandene Route im lokalen LAN; keine Netzwerksuche oder Ping-Probe.
- Vor Testbeginn keine lokale TCP-Verbindung zum Gerät in `ss -tn`; kein bekannter lokaler Lasttreiber. Andere Geräte/Owner-Browser nicht ausgeschlossen. Für dieses Zeitfenster keine UniFi-Daten verfügbar; historische −86/−69 dBm sind keine aktuelle Messung.

## Tatsächlicher Ablauf

Alle Zeiten UTC; Europe/Zurich = UTC+2 an diesem Datum. Hostmonotonic in Sekunden, keine Geräteuptime.

| Phase | UTC / monotonic | Tatsächlicher Nachweis |
|---|---|---|
| Vorbereitung | 11:45:56.825 / 342361.787206 | Einmaliger Runner gestartet; vorhandenen UART-Dateifluss beobachtet. |
| B0 Navigation | 11:45:57.721 / 342362.682824 | Browser öffnet die Shell. `GET /` = 200; vollständiger Transfer 294.729 ms. |
| B0 Authzustand | Request-Ereignisse ab 11:45:58.060 | Status/Temperaturen/Alerts jeweils 401 **vor** Login, erwartete Zugriffssperre; kein Authentifizierungsfehlversuch. |
| B0 Stopauslöser | CDP-Responsezeit 342363.137352 | `GET /favicon.ico` = 404. Original-UART-Zeile 342363.148333 bestätigt `404 Not Found - not found`. |
| Stopmarke | 11:45:58.255 / 342363.216543 | Screenshot gesichert; kein Retry oder weiterer Geräteschritt. |
| Browser beendet | 11:45:58.370 / 342363.332454 | Prozess geschlossen; Gesamtdauer einschließlich Browserstart 1.545 s. |
| B1 | **NOT_RUN** | Keine Login-Anfrage, keine authentifizierte Startseite. |
| B2 | **NOT_RUN – 0/900 s** | Kein Nachweis von 15 min Normalbetrieb oder 5-s-Takt über einen längeren Zeitraum. |
| B3 | **NOT_RUN – 0/1200 s** | Keine geplante 20-min-Ruhephase; Browserbeendigung nach Fehler zählt nicht als B3. |
| B4 | **NOT_RUN – 0/600 s** | Kein Wiederverbindungs-/Sessionablaufnachweis und kein Screenshot nach Ruhephase. |

`HTTP.json` enthält fünf **GET**-Records, keine POST-/Login-/Logout-/Mutationsanfrage: 1 × 200, 3 × erwartete 401, 1 × unerwartete 404. Die UTC-Felder der HTTP-Records bezeichnen den Empfang des **Request-Ereignisses**, nicht den Responsezeitpunkt; CDP-Responsezeiten und -Differenzen stehen separat. Vollständige Transferdauer ist nur für die Shell verarbeitet. Fehlende `loadingFinished`-Dauern nach dem Stop werden nicht ergänzt. Es wurde keine Latenzschranke nachträglich eingeführt.

Die Konsole enthält drei Netzwerkfehlereinträge zu den erwarteten 401. Der Runner stoppte beim 404-Responseereignis; ein nachfolgender Konsoleneintrag zum 404 wurde nicht mehr verarbeitet. Das 404 ist durch HTTP-Record und UART-Warnung unmittelbar belegt, kein Konsolenfehler wird hinzuerfunden. Der Screenshot zeigt eine erreichbare, nicht angemeldete Shell mit leerem Passwortfeld und `Offline`-Anzeige; diese Vor-Login-Anzeige ist **kein Beweis eines WLAN-Ausfalls**. Temperatur-/Sensorqualitäts-/Meldungsanzeige nach Login blieb ungetestet, keine künstlichen Sensorwerte wurden eingeführt.

## Einordnung und Grenzen

| Befund / Nachweis | Ergebnis |
|---|---|
| Shell vom vorhandenen C5-Image erreichbar | **PASS**, einmaliger GET 200 im kurzen Fenster. |
| Zugriffssperre vor Login | **PASS**, drei erwartete anonyme API-401; keine Aussage über erfolgreichen Login. |
| Automatischer Favicon-Request | **FAIL**, Produkt liefert 404; Runner wendet die vorgegebene Stopregel an. |
| Browser-Fernprofil insgesamt | **HOLD / unvollständig**, kein B1–B4-Nachweis. |
| Authentifizierter Status, Temperaturen/Sensorqualität, Meldungen | **NOT_PROVEN**. |
| 15-min-Polling, 20-min-Ruhe, Wiederverbindung/Sessionverhalten | **NOT_PROVEN**. |
| Heap-/Block-Erholung ≥ 90 % von R_ref | **NOT_PROVEN**: keine Referenz/Erholungsphase; bestehende UART liefert in diesem Fenster keine Ressourcenstichprobe. |
| Main-/HTTPD-Stackreserve im Fernlauf | **NOT_PROVEN**: keine aktuelle Stackmessung. |
| Heapfehler-/Resetfreiheit über ein R1-U-Profil | **NOT_PROVEN**. Das kurze beigelegte UART-Fenster zeigt Heartbeats und die 404-Warnung; es ersetzt kein vollständiges Profil. |
| F4-Bedarf, reale Hold-/Skip-/Resume-Coverage | **NOT_PROVEN**, unverändert 13 312 B Skip / 16 640 B Resume, `PROVISIONAL_PENDING_G1`. |
| Programmstart, autonome Regelung, vollständiges R1-U | **BLOCKED / NOT_PROVEN**: gültige Sensor-/Commissioning-Voraussetzungen fehlen; kein Programmstart oder Safety-Bypass. |
| G2-Web / Gesamt-G2 / PR-Freigabe | **HOLD / unvollständig**; kein `G2=PASS`, kein `PR203_FINAL_GO=GO`, kein Pre-Ready. |

**Konkreter Softwarebefund:** der fehlende `/favicon.ico`-Pfad ist im realen Browser bestätigt; die vorhandene Firmware registriert dafür weder Asset noch Route. Das ist ein Browser-Nebenressourcenfehler und der konkrete Stopauslöser. Bereits die lokale Browser-Evidence vom 13.10. enthält einen 404; eine neu eingeführte Regression wird damit nicht behauptet. Er beweist **keinen** Ressourcen-, KDF-, Session-, Stack- oder Regelungsdefekt. Aus diesem kurzen Lauf kann kein zusätzlicher kritischer Softwarefehler abgeleitet werden; die bestehenden Abnahmeblocker bleiben offen. Ein allgemeiner Favicon-Scope oder eine neue Ressourcenverwaltung wird nicht begonnen.

**Empfehlung: PR #203 weiterhin HOLD.** Keine automatische Wiederholung. Vor einem weiteren Browserlauf braucht es eine ausdrückliche Ownerentscheidung zum konkret gesicherten Favicon-Befund (begrenzte Korrektur mit Review und späterem Imagegate oder ausdrücklich benannte Behandlung des Nebenressourcenfehlers im Testblatt) und eine neue Laufanweisung nach diesem Stop. Die Abnahmeverträge werden nicht eigenmächtig geändert.

Ohne physischen Zugang können bestehende Software-/Evidence-Reviews abgeschlossen und ein gezieltes Favicon-Korrekturdelta vorbereitet werden, falls beauftragt. Die erfolgreiche authentifizierte Browsersequenz B1–B4 ist weiterhin möglich, sobald das begrenzte weitere Vorgehen entschieden und andere bekannte Clients geschlossen sind. **Physisch/offen:** F4-Render-/Touch-/Hold-/Skip-Qualifikation und G1′-Ergebnisreview; vollständige G2-/R1-U-Qualifikation auf dem finalen reinen Produktimage mit gültigem Sensor-/Commissioning-Aufbau, frischem beaufsichtigtem Boot, Geräte/Web-Zustandsvergleich, kontinuierlicher UART-Ressourcen-/Stack-/Fehler-Evidence und vergleichbarer R_ref. Ein Auslagern dieser Gates aus PR #203 benötigt eine separate Owner-Scopeentscheidung.

## Evidence und Datenschutz

- [IMAGE_IDENTITY.json](IMAGE_IDENTITY.json): offline neu berechneter App-Prefix-Hash und originale vorhandene Schreib-/Boot-Identitätszeilen.
- [BROWSER_EVIDENCE.json](BROWSER_EVIDENCE.json): Browser/Umgebung, Phaseergebnisse, echte Zeitmarken und Messgrenzen.
- [HTTP.json](HTTP.json): allowlist-bereinigte Netzwerkereignisse, Status und vorhandene Latenzfelder.
- [CONSOLE.json](CONSOLE.json): bereinigte Konsolenereignisse; keine ungeprüften Logtexte oder Argumente.
- [STOP_browser_state.png](STOP_browser_state.png): echter Vor-Login-Screenshot beim Stop; Passwortfeld leer.
- [UART_WINDOW.log](UART_WINDOW.log): unveränderte Originalzeilen aus der bereits laufenden Aufzeichnung, inklusive 404-Warnung und Heartbeats mit Zeitkontext.
- [PROVENANCE.json](PROVENANCE.json): Quell-/Werkzeughashes, Ableitungen und Auslassungen.
- [SHA256SUMS](SHA256SUMS): Manifest aller veröffentlichten Paketdateien außer sich selbst.

Private Rohartefakte und Readbacks bleiben lokal unverändert; keine HAR-, Header-, Cookie-, CSRF-, Passwort-, Token- oder Storage-Ausgabe wird veröffentlicht. Keine Firmwareänderung und kein Neubuild. Die bereits laufende UART-Aufzeichnung wurde weder neu geöffnet noch beendet. Historische G1 IMAGE01-/G1P-/F2-FAIL/HOLD-Ergebnisse bleiben unverändert.

`C0=GO` · `BROWSER_REMOTE=HOLD` · `G2_WEB=HOLD_INCOMPLETE` · `G1_QUALIFICATION=NOT_PROVEN` · `F4_THRESHOLD=PROVISIONAL_PENDING_G1` · `RES-01=BLOCKER` · `PR203_FINAL_GO=HOLD` · `ACTUATOR_RELEASE=NO`.
