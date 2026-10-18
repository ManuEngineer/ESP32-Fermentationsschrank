# PR #203 – Review-Addendum zum B0–B4-Lauf vom 18.10.2026

Reviewbasis: `9848fcda635b076d311d3ff6cc500b2ab8ee8c02`. Dieses Addendum dokumentiert die vom Owner übermittelte unabhängige Ergebniseinordnung. Es korrigiert die Klassifikation des [historischen Builderberichts](RESULT.md) und seiner Statuszusammenfassungen; Messwerte und Originale bleiben unverändert. Dessen `FAIL/HOLD`, die damalige Empfehlung und die `qualification`-Felder der Builder-Auswertungen bezeichnen das historische strikte Lauf-/Verfahrensresultat, **keinen allgemeinen Funktionsausfall der Weboberfläche**. Die Original-[Prüfsummen](result/RESULT_SHA256SUMS), Manifeste, Rohdaten und Builder-Auswertungen werden nicht umgeschrieben. Für diesen begrenzten Dokumentationsdiff steht die unabhängige **Fix Verification noch aus**.

## Beobachtete Produktfunktion

| Phase | Review-Einordnung | Beleg und Beobachtungsgrenze |
|---|---|---|
| **B0** | **PASS** | Gültiger Boot-/Imageidentitäts-/Aktortrennungsnachweis innerhalb der dokumentierten Beobachtbarkeit. Vorruhe 100180,574 s vor B1; derselbe UART-Recorder über 104692,310 s mit 104446 qualifizierten Heartbeats, ohne ungeplanten Reset oder Fatal-/Guardfehler. [Boot](result/UART_BOOT_PROOF.txt), [Capture](result/CAPTURE.json), [Kontinuität](result/UART_CONTINUITY.json). |
| **B1** | **PASS** | Erster `GET /` vollständig 200 nach 282,166 ms; Login 200 nach 2919,846 ms und vom Owner bestätigte normale Anmeldesicht. [HAR](result/HAR_B1_ROOT.json), [Login/HTTP](result/HAR_B2_FULL.json). |
| **B2** | **Funktional PASS_OBSERVED; formale Abnahme PARTIAL** | 161 vollständige authentifizierte gültige JSON-Dreiergruppen im gesamten B0–B2-HAR, 113 nominal im festen 900-s-Fenster. Keine beobachtete Überlagerung zugeordneter HTTP-Transportgruppen. Visibility-/JS-Settlementmessung fehlt; bis zu 64,002 s zwischen Gruppenstarts. Daher keine exakte Promise-/15-s-Frischequalifikation oder lückenlose tatsächliche 5-s-Taktqualifikation. [Gesamtexport](result/HAR_B2_FULL.json), [Nenner und Zeitgrenzen](result/NOMINAL_WINDOW_STATISTICS.json). |
| **B3** | **PASS_OBSERVED mit Evidencelimit** | Host-Ruhefenster 1221,677 s; zweiter NetLog 1279,380 s vor B4 ohne Board-URL-Request. Das belegt den beobachteten Client-Ruheabschnitt, nicht sämtliche unbekannten Clients oder einen exakten gemeinsamen Uhrenabgleich. [Phasen](result/PHASE_TIMING.json), [zweiter NetLog](result/NETLOG_B3_B4.json). |
| **B4** | **PASS_OBSERVED für reale Wiederverbindung/Polling; kein formales Gesamt-GO** | Erster Root vollständig 200 nach 10262,213 ms, Login 200 nach 6808,717 ms; 40 vollständige authentifizierte gültige JSON-Dreiergruppen im gesamten B4-HAR, 28 nominal im 600-s-Fenster. Keine beobachtete HTTP-Transportgruppenüberlappung, maximale Gruppendauer 18,550 s. Der historische B4-Fehler bleibt bestehen; Erfolg dieses Abrufs beweist keine behobene Root Cause. [Root](result/HAR_B4_ROOT.json), [B4-HAR](result/HAR_B4_FULL.json), [Statistik](result/HTTP_STATISTICS.json). |

Die drei Chrome-Ereignisse `ERR_CONNECTION_RESET` (Source-IDs **5096/5099/5102**) sind **nichtterminal und intern wiederaufgenommen**: jeweils anschliessend 200 und gültiger JSON-Body. Ein terminaler Fetch-/Requestfehler ist damit nicht nachgewiesen. Ursache **NOT_PROVEN**; als **Transport-FOLLOW-UP** beobachten, ohne vorsorglichen TCP-/WLAN-/HTTPD-Fix. Diese Details wurden erst offline erkannt und waren keine absichtlich ignorierten Live-Fehler. Ereignisse und Statusfolge bleiben vollständig unter `diagnostic_requests` im [ersten NetLog](result/NETLOG_B0_B2.json) erhalten.

## Verstösse gegen das Testverfahren

Das vom Owner bestätigte Betätigen von „Starten“ um **16:06:55 CEST** war eine **Abweichung vom passiven Testprofil**. Der Aufruf endete mit **400 / `sequence-required`** und hat nichts angewendet: Der [vorhandene Handler](../../../lib/fermentation_app/src/web_application_routes.cpp#L614) kehrt bei fehlender Sequenz vor Reservierung und Anwendung zurück; alle 161 erhaltenen erfolgreichen Status-Snapshots bleiben `standby`, reale Aktoren waren deaktiviert. Das ist kein erfolgreich geprüfter Startversuch, kein Safety- und kein B0–B4-Produktblocker. Der automatische Reload aus `send` bleibt dokumentiert. Die damalige strikte Stoppeinordnung und der nicht erfolgte Live-Stopp bleiben im Originalbericht historisch nachvollziehbar; daraus folgt kein pauschaler Produkt-Funktionsfehler und keine rückwirkende uneingeschränkte Abnahme.

Der bestätigte NetLog-Stop/Neustart mit **87,526 s Aufzeichnungslücke** verletzt die verlangte **lückenlose Recorderführung**. Das bleibt eine formale Verfahrens-/Evidenzabweichung. Es beweist weder einen UART-Unterbruch noch einen Gerätezugriff während B3; der unabhängige UART-Recorder lief durch. Unbekannte Clients, die Browser-Recordinglücke und die Grenzen des Uhrenabgleichs bleiben offen. [Originaltimeline](result/TIMELINE.json).

## Fehlende Nachweise und separate FOLLOW-UPs

Die Ressourcenstichproben stammen **vor** dem Browserlauf. Heap-/HTTPD-Stackspitzen und Recovery, finale F4-Schwelle, Touch/Skip/Hold, zeitgenaue UI-Frische, JS-Body-Settlement und G1-Gesamtqualifikation bleiben **NOT_PROVEN**. Die 60–64-s-Gruppenstartlücken und langen Antwortzeiten sind Befunde; Visibility-/Timerdrosselung und Funklage werden nicht als bewiesene Ursache ausgegeben. Keine neuen Latenz- oder Abnahmeschwellen.

**Web-Befehls-FOLLOW-UP innerhalb [Issue #27](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/issues/27), kein B0–B4-Testblocker:** Das vorhandene Browser-[`send()`](../../../lib/fermentation_app/src/web_application_routes.cpp#L238) sendet `X-Mutation-Seq`; der [ESP-IDF-Adapter](../../../lib/device_platform_esp_idf/src/esp_idf_http_server_lifecycle.cpp#L153) und die [Metadaten-Allowlist](../../../lib/device_platform/src/http_server_lifecycle.cpp#L12) erwarten `X-UI-Mutation-Seq`. Diese Abweichung erklärt die fehlende Sequenz und `sequence-required`. Der Browserheader ist bereits im [gemergten PR-201-Stand](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/blob/93116d2c8d9b3691c2b70a19c9e963b08d5c5722/lib/fermentation_app/src/web_application_routes.cpp#L357) enthalten; sie ist keine durch Single-Flight neu eingeführte Regression.

Die Live-Suche am 18.10.2026 fand kein spezifisches offenes Header-Issue/-Follow-up (offene Issues und Issue-27-Kommentare geprüft). Deshalb hier für den späteren Web-Bedienumfang vorgemerkt: **eine Zeile im Browser auf den bestehenden kanonischen Headernamen korrigieren und den vom echten `send()` gesendeten Header gegen den Adaptervertrag testen**. Keine Provisorien oder zusätzlichen Server-Header-Aliase. In diesem Auftrag keine Implementierung, keine neue Issueanlage und kein Firmware-/Hardware-Retest.

## Aktueller Status und Übergabe

```text
B0=PASS
B1=PASS
B2_FUNCTIONAL=PASS_OBSERVED
B2_FORMAL_ACCEPTANCE=PARTIAL
B3=PASS_OBSERVED_WITH_EVIDENCE_LIMIT
B4=PASS_OBSERVED
B0_B4_FUNCTIONAL_EVIDENCE=PASS_PARTIAL
B0_B4_FORMAL_ACCEPTANCE=HOLD
G2_WEB=HOLD
G1_QUALIFICATION=NOT_PROVEN
F4_THRESHOLD=PROVISIONAL_PENDING_G1
RES-01=BLOCKER
PR203_FINAL_GO=HOLD
ACTUATOR_RELEASE=NO
DOCUMENTATION_FIX_VERIFICATION=PENDING
```

Dies ist kein uneingeschränktes G2-PASS und keine neue Hardwarefreigabe. Ausschliesslich Dokumentationskonsistenz, Links, Privacy und `git diff --check` prüfen; Originale/Hashbeweise unverändert halten. Nach Veröffentlichung genügt **unabhängige Fix Verification dieses Dokumentationsdiffs**, kein neuer Full Review oder Testlauf. Produkt-, Monitor-, Netzwerk- und Testcode bleiben unverändert.

Builder-Nachweis dieses Deltas: alle 35 bisherigen Auditdateien bytegleich zur Reviewbasis, lokale Verweisziele und Statuskonsistenz geprüft, gezielte Privacy-Prüfung des Addendums und `git diff --check` PASS. Keine Builder-Auswertung oder Tests erneut ausgeführt.
