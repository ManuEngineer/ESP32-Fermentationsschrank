# Independent Ergebnisreview – PR #203, B0–B4 vom 18.10.2026

**Builderergebnis: FAIL/HOLD. Kein weiterer Hardwarelauf autorisiert.**

1. [Ergebnis und Stopverhalten](RESULT.md): frühere Transportereignisse mit Chrome-interner Wiederaufnahme, Startmutation/HTTP 400, kein Live-Stopp; Folgebeobachtungen ohne Abnahme-PASS.
2. [Timeline](result/TIMELINE.json) und [Phasenfenster](result/PHASE_TIMING.json): Host/Browseruhren getrennt, NetLog-Stop/Neustart mit 87,526-s-Lücke, nachfolgend mindestens 20 min ohne Board-URL im zweiten Clienttrace.
3. [B0–B2 NetLog](result/NETLOG_B0_B2.json), insbesondere `diagnostic_requests`, und [B2-HAR](result/HAR_B2_FULL.json): Source-IDs 5096/5099/5102 mit −101 und später 200; Mutation Source-ID 7150 mit 400; Root-Reload aus `send`.
4. [B4-NetLog](result/NETLOG_B3_B4.json), [B4-Erstroot](result/HAR_B4_ROOT.json), [B4-HAR](result/HAR_B4_FULL.json): erfolgreicher vollständiger Root, später gültige JSON-Bodies; keine rückwirkende Qualifikation nach vorangegangenem Fehler.
5. [HTTP-Statistik ganzer Exporte](result/HTTP_STATISTICS.json), [nominale feste Fenster](result/NOMINAL_WINDOW_STATISTICS.json), [Exportzuordnung](result/HAR_EXPORT_PROVENANCE.json): unterschiedliche Nenner, Body-/Zeit-/Gruppierungsgrenzen offen.
6. [Capture](result/CAPTURE.json), [Boot](result/UART_BOOT_PROOF.txt), [Kontinuität](result/UART_CONTINUITY.json), [Ressourcen](result/UART_RESOURCES.txt), [HTTPD-Warnungen mit Kontext](result/UART_HTTP_WARNINGS.txt). Komprimierte vollständige numerische [Heartbeats](result/HEARTBEATS.csv.gz) und [Chunks](result/UART_CHUNKS.csv.gz); keine UART-Rohbytes publiziert.
7. [UniFi](result/UNIFI_OBSERVATIONS.json): numerische Werte, abweichender 5-GHz-Client ausgeschlossen, keine Funkkausalität behauptet.
8. [Manifest](result/RESULT_MANIFEST.json), [Prüfsummen](result/RESULT_SHA256SUMS), [Builderchecks und Offline-Wiederableitung](result/BUILDER_CHECK.md).

Die unveränderten [Owneranforderungen](OWNER_TASK.md) und das [Testblatt](TEST_SHEET.md) gelten weiterhin. Private Roh-HAR/NetLog/UART/PNG, Zugangsdaten und Readbacks bleiben lokal. Vorbereitungsdokumente und deren ursprüngliche SHA256SUMS sind historische Snapshots; RESULT_SHA256SUMS bindet separat die neue Ergebnisabgabe.

Reviewfragen: korrekte Fehler-/Stopklassifikation ohne Umdeuten der internen Retries zu terminalen Fetchfehlern; keine Aufwertung nach dem nicht vollzogenen Stopp; transparente Recording-/Lastprofilgrenzen; ausreichende Privacy/Provenienz. Keine Produktkorrektur oder Wiederholung allein aus dieser Abgabe. `PR203_FINAL_GO=HOLD`, `ACTUATOR_RELEASE=NO`.
