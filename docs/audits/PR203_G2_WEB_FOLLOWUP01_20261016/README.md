# PR #203 – G2-Web-Folgelauf 01, 2026-10-16

**G2_WEB_FOLLOWUP01=FAIL/HOLD. B0–B4=NOT_RUN. Independent Ergebnisreview=PENDING.**

Die einmalige Ownerfreigabe wurde für genau eine Portöffnung des korrigierten Monitors genutzt. Der Monitor stoppte vor dem Bootnachweis mit `uart-unexpected-reset`. Es folgten ausschliesslich sein bestehender begrenzter passiver Nachlauf und die Offline-Sicherung der Evidence. Kein Browserlauf, weiterer Portzugriff, Reset, Flash, Retry oder Rollback wurde ausgeführt. Der vorherige UART-STOP und dessen Evidence bleiben unverändert.

## Gepinnter Stand und Ausführung

- Produkt-PR vor dem Lauf: `0a665bbe659841883733a6c10b69f12c1462b543`, offen/Draft; Softwarequelle `a9a1156a1d44d4679957526bbc34a30be6422e4e`.
- Monitor: separater Nicht-Merge-Diagnosebranch `agent/pr203-res01-g1-diag-local`, Commit `a5afe56104a9f5a8a5d673b23f5ba9cd68d54096`; SHA-256 `3af98e77234efb90f7fafa1e9488702a65851efabf55055b7ddf44014fbd32d9`.
- Gate-ID `PR203_SINGLE_FLIGHT_G2_WEB_20261016_IMAGE01`, `--main-stack-policy g2-trend`, ohne `--load`. Keine Änderung an Monitor, Firmware oder Konfiguration.
- Das vorhandene lokale Releaseimage und das privat gesicherte C5-Rückfallartefakt wurden erneut offline gegen die bekannten Hashes geprüft: PASS. Kein Neubuild und keine neue Flash-/Readbackprüfung am Gerät. Die frühere App-only-Flash-/Nicht-App-Evidence wurde wiederverwendet.
- Der Owner bestätigte vor dem Start: ESP32-Tab und bekannte Testclients geschlossen, für die manuelle Bedienung bereit. Unbekannte Clients sind dadurch nicht unabhängig ausgeschlossen.
- Genau eine UART-Portöffnung mit freigegebenem kontrolliertem Boot. Capturestart **17:26:51.806545 UTC**, Captureende **17:26:56.907595 UTC**; Prozessende mit Exitcode 1.

## Beobachtete Reihenfolge und STOP

Im regulären Parserpfad wurden zunächst **88 Heartbeatzeilen mit demselben `uptime_ms=3594033`** verarbeitet. Danach folgte die vollständige Zeile:

```text
rst:0x1 (POWERON_RESET),boot:0x13 (SPI_FAST_FLASH_BOOT)
```

Ihr Parsezeitpunkt lag rund **0,077392 s** nach Capturestart und damit innerhalb von 5 s. Ursache und Zeitfenster allein erlauben diese Zeile aber nicht: Nach dem bereits beobachteten Heartbeat ist jeder Reset laut korrigierter Regel unerwartet. Daher `uart-unexpected-reset`, `ABORT`, kein Browserstart. Der Zähler `expected_initial_reset_lines=1` im Capture-JSON zählt die erfasste Resetzeile; er bedeutet hier keine akzeptierte Resetfreigabe.

Der Monitor sicherte danach den bestehenden begrenzten passiven Nachlauf (5,022 s; 7 118 Bytes). Nach Schliessen des Monitors gab es keine weitere Geräteaktion. Der vollständige Rohstrom umfasst 13 393 Bytes und 30 Chunks (2 regulär, 28 Nachlauf). Die [vollständige bereinigte Parserfolge vor STOP](UART_PRESTOP.sanitized.log) enthält auch die wiederholten Heartbeats und den Original-Resettext; in diesem Abschnitt musste keine Zeile ausgelassen werden. Die Zeilenenden der bereinigten Textauszüge sind auf LF normalisiert; bei leeren UART-Zeilen ist das abschliessende Leerzeichen nach der Hostzeitmarke entfernt. Ereignistext und Hostzeitmarken bleiben erhalten.

**Bewiesen:** die oben dokumentierte Empfangs-/Parserreihenfolge und der darauf beruhende fail-closed STOP. **Plausible Hypothese:** Der erste Datenbestand könnte aus der Zeit vor dem kontrollierten Portöffnungs-Boot stammen; die wiederholten Heartbeats haben bereits fast eine Stunde Uptime, während der Nachlauf einen neuen Boot zeigt. Herkunft, Wiederholungsursache und physischer Ereigniszeitpunkt dieser Bytes sind nicht unabhängig belegt. Ein zusätzlicher spontaner Produktreset oder ein B4-Fehler ist durch diesen Lauf nicht bewiesen. Es wird keine neue Parserkorrektur oder Hardwarekampagne daraus abgeleitet.

## Bootmerkmale und Ressourcen: nur Nachlaufbeobachtung

Der reguläre Capture-Ergebnisstatus hat `source_proven=false`, `ready=false`, `actors_disabled=false`, `release_profile=false` und `network_home_connected=false`, weil der STOP vor diesen Prüfstellen erfolgte. Erst der [ausgewählte Nachlauf](UART_TAIL_SELECTED.sanitized.log) zeigt den erwarteten Quell-SHA, `esp32_release`, `real actuators: disabled`, vollständiges `application: ready`, `HomeConnected` am Ressourcenpunkt und zwei neue Safe-Mode-Heartbeats. Das ist **OBSERVED_POST_STOP**, kein bestandener zusammenhängender Boot-/Überwachungsnachweis.

Die ebenfalls erst im Nachlauf aufgezeichneten Ressourcenwerte lauten:

| Bootpunkt | Freier Heap B | Grösster 8-Bit-Block B | Boot-Minimum B | Main-Stack-HWM B |
|---|---:|---:|---:|---:|
| after_platform_begin | 169 284 | 110 592 | 169 284 | 18 536 |
| after_application_begin | 71 220 | 65 536 | 67 248 | 11 704 |
| after_ui_init | 44 724 | 40 960 | 36 076 | 11 704 |
| stable_home_wifi | 42 656 | 40 960 | 35 372 | 11 704 |

Diese Stichproben sind weder Idle-/Recovery-Referenz noch Last-/Peak-/F4-Qualifikation. Insbesondere ist das Boot-Minimum kein aktueller Recovery-Wert. Die konfigurierte Main-Grenze 11 160 B war `record-only`; im regulären Parserpfad wurde vor STOP kein Stackwert ausgewertet. HTTPD-Stackreserve und belastete Heap-/Blockerholung sind **NOT_PROVEN**.

## Browser-/Netzwerk-Nachweise

B0, B1, B2, B3 und B4 sind sämtlich **NOT_RUN**. Keine durch diesen Lauf ausgelöste Browser-Geräteanfrage, keine Anmeldung, keine Polling-Gruppe, keine HAR-/NetLog-Aufzeichnung und keine aktuelle UniFi-Messung. HTTP-Erfolg, Fehlerklassen, Latenzen, Single-Flight-Verhalten und Wiederverbindung bleiben für diesen Versuch **NOT_PROVEN**. Die genehmigten Favicon-404- und erwarteten anonymen/abgelaufenen Session-401-Ausnahmen kamen nicht zur Anwendung. Die vorbereitete private Browser-Ablage blieb unbenutzt.

Die früheren Funk-/Retrybefunde sowie lange GET-Dauern und `B4_RECONNECT=FAIL_OBSERVED` werden durch diesen abgebrochenen Lauf weder bestätigt noch widerlegt. Es wurden keine neuen Grenzwerte gesetzt.

## Evidence und Review

- [CAPTURE.json](CAPTURE.json): unveränderte, auf private Inhalte geprüfte Monitorzusammenfassung.
- [TIMELINE.json](TIMELINE.json): UTC-/monotone Referenz, Parse-/Stopreihenfolge, Uptime und Rohdatenoffset der Resetzeile. Zeitmarken bezeichnen Hostempfang/Parsing, nicht den physischen Gerätezeitpunkt; die Uhrunsicherheit ist nicht separat vermessen.
- [UART_CHUNKS.jsonl](UART_CHUNKS.jsonl): unveränderte Zeit-/Längen-/Nachlaufmetadaten ohne UART-Nutzdaten.
- [HTTP_SUMMARY.json](HTTP_SUMMARY.json): expliziter Nichtausführungsstatus.
- [ARTIFACT_MANIFEST.json](ARTIFACT_MANIFEST.json), [SHA256SUMS](SHA256SUMS): Provenienz, Auswahl-/Auslassungsregeln und Integrität.

Vollständiger Roh-UART, kompletter Nachlauf, private Socketadressen, Readbacks und Rückfallimage bleiben geschützt lokal. MAC, Zugangsdaten und private Netzwerkkonfiguration wurden nicht übernommen; Browser-Secrets wurden in diesem Lauf nicht erzeugt. Die öffentlichen UART-Auszüge sind eine ausgewählte Evidence und ersetzen nicht den privaten Rohstrom.

**Nächster Schritt:** unabhängiger Ergebnisreview dieses STOP. Die einmalige Ausführung ist verbraucht. Jede weitere Geräte-/UART-/Browseraktion benötigt eine neue konkrete Ownerentscheidung; es gibt keinen automatischen Folgeversuch.

`G2_WEB_FOLLOWUP01=FAIL/HOLD` · `G2_WEB=FAIL/HOLD` · `B0_B4=NOT_RUN` · `B4_RECONNECT=FAIL_OBSERVED` · `G1_QUALIFICATION=NOT_PROVEN` · `F4_THRESHOLD=PROVISIONAL_PENDING_G1` · `PR203_FINAL_GO=HOLD` · `ACTUATOR_RELEASE=NO`
