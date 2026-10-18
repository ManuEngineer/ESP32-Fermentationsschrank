# PR #203 – Single-Flight G2-Web: Flash-/Browser-Gate

**Ergebnis: `G2_WEB=FAIL/HOLD`.** Der freigegebene einmalige App-only-Flash
war erfolgreich; die UART-Überwachung stoppte unmittelbar danach mit
`uart-anomaly`. Gemäss Stopregel gab es keinen Browserlauf und keine weitere
Geräteaktion. Dieser Bericht dokumentiert den tatsächlichen Stand und die
Grenzen der aufgezeichneten Evidence.

## Geprüfte Softwarebasis und Image

| Gegenstand | Geprüfter Stand |
|---|---|
| Live-PR | #203 offen/Draft, Branch `agent/issue-27-r1-web-dashboard-plan` |
| PR-HEAD vor Ergebnisbericht | `7a9a395c5a37a712c8857ee7a5cf8811073ade50` |
| Freigegebener Single-Flight-Plan | `8094b7f1a149269b0ad3e1c49cf4366bdfc40e7c` |
| Produkt-/Implementierungsquelle | `a9a1156a1d44d4679957526bbc34a30be6422e4e` |
| Profil / Plattform | `esp32_release` / ESP32, vorhandene Partitionierung |
| App-Datei | `build/esp32_release/esp32_fermentationsschrank.bin`, 1 756 544 B |
| App SHA-256, lokal nachgerechnet | `2d069d2db793359177379f492559c69fa949cb470351d57f60dd095889619305` |
| ELF-Datei | `build/esp32_release/esp32_fermentationsschrank.elf`, 36 246 576 B |
| ELF SHA-256, lokal nachgerechnet | `dca3cc664f3c143797f1caf29ca51bd3b9afe88eb081f914bec595f065523601` |
| Offline-Build-/Quellnachweis | `docs/audits/PR203_SINGLE_FLIGHT_POLLING_20261016/VERIFICATION.json`; sauberer Quellbuild auf `a9a1156`, Image-Info PASS |

Die nachgerechneten Binärhashes stimmen mit dem versionierten Buildnachweis
überein. Es gab keinen Neubuild. Die implementierte Änderung ist die geprüfte
Single-Flight-Polling-Implementierung; dieser Auftrag nimmt keine weiteren
Produktcodeänderungen vor.

## Board, Installationsziel und bekannte Identität

- Ziel war das dauerhaft angeschlossene, aktor- und sensorlose ESP32-Devboard.
  Der frische Read-only-Preflight identifizierte ESP32-D0WD-V3 Rev 3.1 und
  dieselbe Board-MAC wie die vorhandene Evidence; die MAC bleibt privat.
- Installationsziel war ausschliesslich die App-Partition ab `0x10000`; genau
  ein App-only-Flash ohne Erase. Die Nicht-App-Bereiche im Readback vor/nach
  dem Flash waren bytegenau unverändert.
- Letzter dokumentierter Image-Stand: C5-Produktimage aus Quelle
  `162a6670526a10b2fd957fe90ab665a1fae837b7`, App-SHA-256
  `ebb296c08b79045e6e081f328c9f5fd7e576e20722670968b131e722a8b36633`,
  1 756 528 B. Die Identität stammt aus vorhandener Flash-/Readback-/UART-
  Evidence und wurde im frischen Preflight bytegenau bestätigt.
- **C5-Rückfallartefakt: PASS, privat gesichert.** Aus dem früheren privaten
  4-MiB-Readback wurde der App-Bereich ab `0x10000`, exakt 1 756 528 B,
  offline extrahiert. Sein SHA-256 ist
  `ebb296c08b79045e6e081f328c9f5fd7e576e20722670968b131e722a8b36633` und
  stimmt mit der bisherigen C5-App- und UART-Identität überein. Das Image und
  ein privates Provenienzmanifest liegen mit Modus `0600` in der geschützten
  privaten Evidence-Ablage; der Readback bleibt unverändert und ausserhalb
  GitHub. Kein älteres Image wurde verwendet.

## Tatsächliche Ausführung und Evidence

Die Einmalfreigabe des Owners lag vor. C5-Rückfallartefakt, Kandidaten-App
und ELF wurden vor dem Schreiben lokal byte-/hashverifiziert; das vorhandene
Board war ein ESP32-D0WD-V3 Rev 3.1. Der frische Read-only-Preflight am
16.10.2026 16:10:52–16:17:16 UTC ergab den erwarteten C5-App-Hash und dieselbe
Partitionstabelle wie die bisherige Evidence. Die Abweichung zum historischen
4-MiB-Readback lag mit 505 Bytes ausschliesslich in der veränderlichen
`state_store`-Partition. Der frische Readback wurde privat als Vorher-Referenz
gesichert.

Genau ein `write-flash`-Aufruf schrieb 1 756 544 Byte des freigegebenen
`esp32_release`-Images ab `0x10000` (16.10.2026 16:23:47–16:24:18 UTC). Esptool
meldete `Hash of data verified`. Danach waren der Nicht-App-Präfix
`0x000000–0x00FFFF` und die `state_store`-Partition `0x300000–0x3FFFFF` im
bytegenauen Readback identisch zu ihrer Vorher-Referenz. Es gab keinen zweiten
Flash, kein Erase und keinen Rollback.

Der UART-Monitor öffnete danach den Port und meldete nach rund fünf Sekunden
`uart-anomaly`; der Prozess schloss den Port und sicherte seinen begrenzten
Nachlauf. Der Rohmitschnitt enthält beim Portöffnen eine
`POWERON_RESET`-Startsequenz und danach den Kandidaten-Quell-SHA,
`esp32_release`, `real actuators: disabled`, `application: ready`,
`HomeConnected` sowie zwei Safe-Mode-Heartbeats. Im aufgezeichneten Nachlauf
stehen keine `heap_alloc_failed`-, Panic-, WDT- oder Brownout-Marker. Der
Monitorstatus bleibt dennoch **FAIL/HOLD**. Die lokale Capture-Implementierung
prüft auch einen noch nicht abgeschlossenen UART-Frame auf `rst:0x`; der
gesicherte Rohmitschnitt zeigt dieses Token im `POWERON_RESET`-Banner des
autorisierten Portöffnungsboots. Das spricht für eine vorzeitige
Fail-closed-Monitorauslösung, nicht für einen belegten unerwarteten Reset.
Der Monitor beendete die Überwachung, bevor sein regulärer Bootnachweis
abgeschlossen war. Diese wahrscheinliche Parser-Fehlklassifikation hebt den
STOP nicht auf und darf nicht als Monitor-PASS gewertet werden.

Die privaten Originale (vollständige Readbacks, UART-Rohbytes/-Logs,
Flashausgabe, Zeitmarken und Hashmanifest) bleiben lokal mit eingeschränkten
Dateirechten. Öffentlich werden weder MAC, vollständige Readbacks noch UART-
Zeilen mit privaten Netzwerkdetails abgelegt. Der Monitor stammte aus dem
separaten Diagnose-Commit `6abd685f51ca69f5e6a5b10a332ed014351d8e43`;
`g1_capture.py` SHA-256 `89212d6ace0fb77b8a0230df87900175be3952153db38a78f57f70c332573d36`,
`c0_load.py` SHA-256 `caef7a34a0f2b53629adf24af197052c8cc73fc0b34d5fa716c50ce6aa8d49ca`.
Dieser Diagnosecode wurde nicht in den Produkt-PR übernommen. Der frühere
UART-Capture wurde vor dem Lauf geordnet beendet und unverändert privat
archiviert.

## Browserablauf: nicht gestartet

Chrome `154.0.8037.97` im Selkies-Docker, ein ESP32-Tab im lokalen LAN.
Andere bekannte Browser/Testclients schliessen;
unbekannte Clients als Einschränkung festhalten. Keine Browserautomation,
Netzwerkprobe, Mehrfachclients oder künstlich verlangsamten Requests. Vor B0
ist ein privater Capture-Ordner mit Modus `0700` vorbereitet. Chrome DevTools
Network `Preserve log` bleibt aktiv; Cacheeinstellung bleibt unverändert. HAR
für B0–B2 vor Tab-Schluss sowie für B4 unmittelbar nach der Beobachtung lokal
im privaten Ordner speichern. Den ersten B4-Fehler zusätzlich mit kompletter
Chrome-Fehlerklasse aus Network/Console und Screenshot sichern. HAR kann
Zugangsdaten, Cookies, Header oder Bodies enthalten und bleibt ausschliesslich
privat; nur bereinigte Zeit-/Requestmetadaten und Fehlerklasse dürfen in den
PR-Bericht. Host-UTC/monotone Marken werden für Phasenwechsel parallel notiert.

| Phase | Umfang und Dauer |
|---|---|
| B0 | Zeitreferenz, Browser/Image, Host-/UniFi-Zustand und Trace vorbereiten; einzelne Navigation |
| B1 | Einmal Shell öffnen und normal anmelden; anonyme 401 getrennt werten; nach Login Screenshot und fehlende/stale Sensoren dokumentieren |
| B2 | Genau 15 min ein Tab mit normalem 5-s-Polling, keine Mutationen; Status/Fehler/Latenzen bei 0/5/10/15 min; Requestgruppen anhand Start, Ende und Bodyabschluss prüfen |
| B3 | Trace sichern, Tab vollständig schliessen, 20 min keinerlei Geräte-HTTP, Ping oder Erreichbarkeitstest; UniFi read-only bei 0/10/20 min |
| B4 | Nach 20 min Tab einmal öffnen, falls nötig einmal normal anmelden; erste Dokumentfehlerklasse vor jedem weiteren Versuch sichern; bei Erfolg normales Polling bis 10 min mit 0/5/10-Nachweisen |

Bekannte alte Funkbeobachtung: 2,4 GHz, RSSI etwa −84…−87 dBm und erhöhte
Retries. Das ist ein Repräsentativitätsrisiko, keine Ursachenfeststellung oder
RSSI-Abnahmegrenze. Aktuelle UniFi-Werte müssten bei einem neu freigegebenen
Lauf read-only und mit Zeitbezug erhoben werden. Keine Änderung an AP, Kanal,
WLAN-Powersave, TCP oder HTTPD.

Für diesen Lauf war ausschliesslich `/favicon.ico`-404 ausdrücklich als
erwartete Ausnahme freigegeben; diese Ausnahme kam nicht zur Anwendung.
Unerwartete HTTP-/Transport-/Konsolenfehler, Timeout, Nichterreichbarkeit,
Heap-Allokationsfehler, Panic/Abort, WDT, Brownout, Reset oder andere
sicherheitsrelevante Anomalie: sofort Trace/Uhrzeit sichern, Owner schliesst
den Tab, `STOP/HOLD`; kein Retry, Reset, Flash, Rollback oder Folgelauf.

## Browserlauf und Abnahmerahmen

Der freigegebene B0–B4-Browserablauf wurde **nicht gestartet**. Es gab daher
keine B0-Zeitreferenz, keinen Login, kein B2-Polling, keine B3-Unterbrechung,
keine B4-Wiederverbindung und keine UniFi-Beobachtung für diesen Lauf. Die
Favicon-404-Ausnahme kam nicht zur Anwendung. Nach dem UART-STOP gab es keine
weitere Browser- oder Geräteaktion. Ein neuer Monitor-/Flash-/Browserlauf
benötigt eine neue Ownerentscheidung; dieses Ergebnis erteilt keine
Folgefreigabe.

| Nachweis | Ergebnis |
|---|---|
| C5-Rückfallartefakt und frischer Preflight | PASS |
| Kandidaten-App/ELF-Hash | PASS |
| Einmaliger App-only-Flash auf `0x10000` und esptool-Hash | PASS |
| Nicht-App-Präfix und `state_store` nach Flash | PASS, bytegenau unverändert |
| Kandidaten-Bootmerkmale im gesicherten UART-Nachlauf | OBSERVED: Source, Release-Profil, Aktortrennung, ready, HomeConnected, Heartbeats |
| Fail-closed UART-Überwachung | FAIL/HOLD: `uart-anomaly` beim kontrollierten Portöffnungs-Reset |
| B0–B4 / HTTP / Browser / UniFi | NOT_RUN |
| Langzeit-/Recovery-/B4-Erreichbarkeit | NOT_PROVEN |

Statusgrenzen bleiben unverändert: `B4_RECONNECT=FAIL_OBSERVED`,
`G1_QUALIFICATION=NOT_PROVEN`, `F4_THRESHOLD=PROVISIONAL_PENDING_G1`,
`G2_WEB=FAIL/HOLD`, `G2_RETRY=NOT_AUTHORIZED`, `PR203_FINAL_GO=HOLD`,
`ACTUATOR_RELEASE=NO`.
