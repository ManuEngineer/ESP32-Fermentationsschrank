# PR #203 – B4: gezielte Offline-Eingrenzung, 2026-10-16

**Ergebnis: Polling-Überlagerung ist im Code belegt; eine Verursachung des B4-Fehlers ist nicht belegt.** `B4_RECONNECT=FAIL_OBSERVED`, `BROWSER_MANUAL=HOLD`, `PR203_FINAL_GO=HOLD`, `ACTUATOR_RELEASE=NO` bleiben unverändert. Nur Quellen und vorhandene/festgeschriebene Evidence ausgewertet; kein Browser-/Hardwaretest, Gerätezugriff, Reset, Flash, Produktfix oder Konfigurationsdelta.

Basis: PR #203 offen/Draft, Branch `agent/issue-27-r1-web-dashboard-plan`, Live-HEAD bei Analysebeginn `4091be8a8faeb6c32513d746626567736242706c`; freigegebener RES-01-Plan `8ce57d78f1544728bf1868d965551d3de7c302f0`. Die Produktquelle dieses Befunds ist mit dem installierten C5-Stand `162a667` identisch. [Browserbericht](PR203_BROWSER_MANUAL_20261016/README.md), [HTTP-Ausschnitte](PR203_BROWSER_MANUAL_20261016/HTTP_SCREENSHOT_OBSERVATIONS.json), [Zeitmarken](PR203_BROWSER_MANUAL_20261016/TIMELINE.json).

## Bewiesen

1. **Keine Sperre einer laufenden Polling-Gruppe.** [rf()/Timer im geprüften Quellstand](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/blob/4091be8a8faeb6c32513d746626567736242706c/lib/fermentation_app/src/web_application_routes.cpp#L197-L245):

   ```js
   async function rf() {
     const my = ++G, t0 = Date.now();
     // ... Anzeigealter aktualisieren ...
     const r = await Promise.all(['status','temperatures','alerts']
       .map(x => fetch('/api/v1/'+x, {cache:'no-store'})));
     // ... Statusprüfung, anschließend await Promise.all(r.map(x=>x.json())) ...
   }
   rf(); setInterval(rf, 5000);
   ```

   Jeder ausgeführte Timeraufruf kann drei neue Fetches starten, während frühere Gruppen noch auf Header oder Body/JSON warten. Derselbe Endpunkt kann somit mehrfach gleichzeitig ausstehen; ebenso seine Geschwister `status`, `temperatures`, `alerts` aus mehreren Takten. `G/H` verwirft ältere Ergebnisse nach Abschluss, stoppt aber keine Requests. Die 15-s-Frischeprüfung begrenzt die Anzeigeverwendung, nicht die Requestlaufzeit. `rf()` hat weder In-flight-Sperre noch Abort-Signal. Bei einem Promise-Fehler werden die übrigen Fetches durch `Promise.all` nicht automatisch abgebrochen. `LB` begrenzt ausschließlich den separaten Programmkatalogabruf `lp()`; ein solcher GET kann zusätzlich zum Polling laufen. Aufrufpfade nach Mutationen existieren ebenfalls, waren im manuellen Lauf aber nicht beauftragt/belegt.

2. **Offline nachvollzogen, ohne Netzwerk:** Die tatsächliche `rf()`-Funktion wurde aus den C++-Stringliteralen extrahiert und in einer Node-VM mit einem ausschließlich lokalen, niemals auflösenden Fetch-Stub zweimal bei simulierten 0/5 000 ms aufgerufen. Ergebnis: **zwei Gruppen, sechs noch ausstehende Fetch-Aufrufe**. [Ausgabe mit Quell-/Funktionshash](PR203_B4_POLLING_DRY_RUN_20261016.json). Kein Browser, echtes Fetch oder Timer-/Gerätetest; dies beweist die Codeeigenschaft, keine historische Socketbelegung.

3. **Die dokumentierten Laufzeiten überschreiten den Takt deutlich.** B2-Screenshots enthalten HTTP-200-Antworten bis **23,99 s** und zwei `pending`-Einträge. Bei regelmäßig gelieferten 5-s-Ticks könnte eine 23,99 s dauernde Gruppe mit vier später gestarteten Gruppen überlappen (Starts 0/5/10/15/20 s). **Bis zu 15 ausstehende logische Fetches** ist nur das Beispiel, wenn alle drei Abrufe jeder Gruppe entsprechend lange offen bleiben; es ist kein gemessener Maximalwert. Browser-Timerdrosselung, Warteschlangen und frühere Abschlüsse beeinflussen den tatsächlichen Verlauf. Die sichtbare Gesamtdauer enthält möglicherweise Queue/Stall/Verbindungsaufbau und ist keine reine ESP32-Handlerzeit.

4. **Die Serverkapazität ist begrenzt.** Der [Produktadapter](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/blob/4091be8a8faeb6c32513d746626567736242706c/lib/device_platform_esp_idf/src/esp_idf_http_server_lifecycle.cpp#L188-L206) übernimmt `HTTPD_DEFAULT_CONFIG()` ohne Änderung von Socketzahl/LRU/Send-/Receivezeit. Lokal aus dem C5-Build referenziertes ESP-IDF ist exakt `fff9895c82d744c7237be8847347bdd1b07c6643`: [Default](https://github.com/espressif/esp-idf/blob/fff9895c82d744c7237be8847347bdd1b07c6643/components/esp_http_server/include/esp_http_server.h#L56-L82) **7 Clientsessions**, `lru_purge_enable=false`, Send/Receive jeweils 5 s pro Socketoperation. Aufgelöste C5-Konfiguration: `CONFIG_LWIP_MAX_SOCKETS=10`, TCP-Sendepuffer 5 760 B. **Sieben Clients plus drei interne HTTPD-Sockets**, nicht vier Clients: [Startprüfung](https://github.com/espressif/esp-idf/blob/fff9895c82d744c7237be8847347bdd1b07c6643/components/esp_http_server/src/httpd_main.c#L516-L530) und [Sessionannahme](https://github.com/espressif/esp-idf/blob/fff9895c82d744c7237be8847347bdd1b07c6643/components/esp_http_server/src/httpd_main.c#L280-L285). Bei voller Sessiontabelle und LRU=false nimmt die HTTPD-Schleife vorerst keine neue Session an. Ein langsamer synchroner Handler kann außerdem die Bearbeitung anderer Sockets verzögern. Die Operationstimeouts sind keine 5-s-Gesamtfrist eines Browserrequests.

## Plausible Hypothese, kein B4-Kausalnachweis

- Wiederholte Gruppen können mehr Browserarbeit aufstauen und vorhandene Verbindungen länger beanspruchen. Auf Geräteseite sind Konkurrenz um begrenzte Sessions, TCP-/WLAN-Puffer und gemeinsamen Heap sowie Verzögerungen anderer Verbindungen plausibel. **Fetch-Anzahl ist nicht Socket-Anzahl:** Browserqueueing und Wiederverwendung begrenzen/verschieben die echte Netzwerklast; die tatsächliche Zahl offener HTTPD-Sessions wurde nicht gemessen. Ebenso kein Nachweis von 15 gleichzeitigen Serverhandlern, bestimmter TX-Pufferanzahl oder einem festen Heapverbrauch pro Request.
- Funklage **−84 bis −87 dBm** und die getrennt dokumentierten Client-/AP-Retries sind mögliche Umgebungsstörungen. Die Screenshots beweisen weder einen Funkfehler als Ursache noch einen Defekt in HTTPD/lwIP. Keine RSSI-Grenze wird nachträglich eingeführt.

## Für B4 nicht belegt

Der erste Fehler betrifft das **Dokument der neu geöffneten Seite**. Deren `rf()` kann erst laufen, nachdem die Shell mit dem Script angekommen ist; das neue Polling erklärt diesen ersten Dokumentfehler nicht unmittelbar. Damit altes B2-Polling B4 nach der gemeldeten Tab-Schließung und rund 25 min Ruhe verursacht hätte, müssten beispielsweise Verbindungen oder ein Server-/Netzwerkzustand über die Ruhephase fortbestanden haben. Dafür fehlen Belege. Ein geschlossener Tab startet selbst keine weiteren Intervalle; Zeitpunkt, serverseitige Bereinigung und vollständige Fremdclientfreiheit sind aber nicht unabhängig gemessen.

Es fehlen vollständige Request-Start-/End-/Connection-Zeiten, exakte Tab-Schließ-/Wiederöffnungszeiten, HTTPD-Sitzungs-/Socketzustände und aktuelle Heap-/Blockwerte. Der gespeicherte Screenshot erlaubt nur **`net::ERR_AD…`**; vollständige Fehlerklasse und Zuordnung der Cancellations bleiben offen. Der Owner bestätigte manuelles Neuladen/Neuaufrufen, dann Shell 200; diese Ablaufabweichung und HOLD bleiben erhalten. Die vorhandenen **4 294 Heartbeats**, max. Abstand **1,357436 s**, keine Uptime-Rücksprünge/Heap-/Panic-/Resetmarker im begrenzten Fenster widerlegen weder Socketstillstand noch Netzwegstörung; Ressourcenstichproben fehlen.

Kein vollständiges Browser-HAR/NetLog wurde in den vorhandenen PR203-Evidenceverzeichnissen sowie den zugänglichen lokalen Downloads-/Konfigurationsablagen gefunden. Die Suche war auf Dateinamen begrenzt; andere, nicht zugängliche Browserablagen sind dadurch nicht ausgeschlossen. Historische `s1_results.json`/`s2_results.json` betreffen frühere Läufe, nicht diesen B4-Versuch. Private Rohlogs/Manifeste unverändert und geprüft; keine laufende Capture-Datei oder Geräteverbindung für neue Messungen verwendet.

## Kleinste Empfehlung: fehlende Evidence vor einer B4-Fixbehauptung schließen

**Kein Produktfixplan als B4-Lösung aus dieser Evidence.** Eine In-flight-Begrenzung wäre eine eng begrenzbare Polling-Risikominderung, aber noch kein nachgewiesener Fix der Nichterreichbarkeit. Weil die B4-Ursache offen bleibt, ist die Empfehlung die folgende minimale zusätzliche Evidence für eine **später separat freizugebende** kontrollierte Wiederholung:

1. **Ein vollständiger, von Anfang an gesicherter Browsertrace desselben einzelnen Testbrowsers:** pro Request ID, Pfad/Methode, Initiator, Protokoll/Verbindungs-ID, Start/Ende oder Abbruch, vollständige nicht geheime Fehlerklasse sowie Queue/Stall/Connect/TTFB/Download-Zeiten. Bis `loadingFinished`/Fehler erfassen, nicht nur bis Headerempfang. Daraus reale Überlagerungen/ausstehende Requests ableiten. Export vor Tab-Schließen sichern; öffentliche Ausleitung nur per Allowlist ohne Header, Bodies, Cookies oder Tokens. Bestehende private Logs wären zuerst offline zu prüfen, falls sie noch bereitgestellt werden.
2. **Exakte lokale Aktionsmarken** für Tab-Schließen, Beginn/Ende der Ruhe und ersten B4-Aufruf sowie ein gesicherter erster Fehlerbildschirm/Fehlerdatensatz **vor jedem Neuaufruf**. Alle bekannten Geräteclients vorher schließen und vollständig dokumentieren; Unbekannte als Einschränkung erhalten. Erster unerwarteter Fehler beendet den Versuch; kein manueller oder automatischer Retry. Dadurch bleiben initialer Fehler und spätere Wiederholung unterscheidbar.
3. **Zeitkorrelation mit vorhandener UART und UniFi:** gleiche UTC-/monotone Referenz, tatsächliche AP/Client-Ereignisse im Fehlerfenster statt nur älterer Connect-Einträge. Keine aktive Netzprobe. Das schließt die jetzige Korrelationslücke; ohne geräteseitige Session-/Heapmessung ist damit weiterhin keine Socket-/OOM-Verursachung bewiesen. Eine solche zusätzliche Diagnose braucht, falls später erforderlich, einen eigenen begrenzten Auftrag/Gate.

Das ist eine Evidenceanforderung, **keine Lauf- oder Hardwarefreigabe**. Keine vorsorgliche TCP-/WLAN-/HTTPD-Umstellung, kein geänderter Timeout und kein F4-Schwellenwert. Falls stattdessen eine eigenständige Polling-Risikominderung gewünscht wird, wäre dafür ein separater kurzer Fixplan nötig; auch dessen Abnahme dürfte B4 ohne neuen Ergebnisnachweis nicht freigeben.

**Restunsicherheit:** bewiesene ungebremste Fetch-Starts treffen auf dokumentierte lange Antworten; tatsächliche Verbindungs-/Heapauslastung und die Ursache des späteren Dokumentfehlers bleiben unbewiesen. **B4 bleibt Abnahmeblocker/HOLD.**
