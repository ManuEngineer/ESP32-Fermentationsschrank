# PR #203 – B0–B4: Vorbereitung mit mindestens 24 h Vorruhe

**Snapshot vom 17.10.2026: `B0_BOOT=PASS`, `PRETEST_IDLE_24H=IN_PROGRESS`, Browserphasen `NOT_RUN`.** Kein Ergebnis-PASS für den Gesamtlauf.

Der [einmalige Ownerauftrag](OWNER_TASK.md) gilt. Der zusätzliche Ownerentscheid im Chat vom 17.10. verlangt die Vorbereitung heute, Browsertests erst morgen und mindestens 24 h Ruhe **ohne** erneute UART-Portöffnung/Boot vor dem Test. Deshalb erfolgt der eine genehmigte kontrollierte Portöffnungsboot bereits heute; die anschliessende Aufzeichnung bleibt durchgehend offen. Dieser ausdrücklich angeordnete Aufschub ersetzt die ursprüngliche unmittelbare Browserfortsetzung nach Boot. Es ist keine erneute isolierte Erstabruf-Diagnose und keine zusätzliche Ausführungsfreigabe nötig.

## Tatsächlicher Start und feste Zeitgrenzen

- Produkt-PR-HEAD zu Beginn: `0049af07db2d7287955842ef70936fb70eac6862`; installierte Quelle `a9a1156a1d44d4679957526bbc34a30be6422e4e`, `esp32_release`.
- Unveränderter Monitor aus `893f3bb73a50b484ce79b3b80722873452d998fc`, byteidentische private Quellsnapshots; `g2-trend`, bestehende Gate-ID. App-/ELF-/C5-Rückfallhashes offline bestätigt; [vollständiges Manifest](ARTIFACT_MANIFEST.json).
- Genau eine UART-Portöffnung heute, frischer [Bootnachweis](BOOT_PROOF.txt): gültiger initialer POWERON_RESET, richtige Quelle, Release-Profil, reale Aktoren deaktiviert, vollständiges `application: ready`, HomeConnected und frische Heartbeats.
- **Vorruhe seit 17.10.2026 11:58:40,736746 CEST / 09:58:40,736746 UTC.** 86400 s sind frühestens am **18.10.2026 11:58:40,736746 CEST** erfüllt. Geplanter erster Geräteabruf **frühestens 18.10.2026 um 12:00 CEST / 10:00 UTC**, also mit zusätzlicher Marge. Späterer Beginn ist möglich, ohne den Recorder zu öffnen oder das Board zu booten.
- Zeitfristen und monotone Startreferenz stehen im [Vorbereitungs-Snapshot](PREPARATION_STATE.json); das geschützte lokale `RUNTIME.json` enthält die laufende Fortsetzungsreferenz. Der öffentliche Snapshot ist kein Abschlussnachweis der noch laufenden 24 h.

## Durchgehende Aufzeichnung

Der Recorder läuft als abgekoppelter Prozess mit eigener Sitzung/Prozessgruppe, ignoriert SIGHUP, hat keine offene Eingabepipe und schreibt ausschliesslich in die private Evidence-Ablage. Sein Elternprozess ist PID 1. Host-`KillUserProcesses=false` wurde live ausgelesen; keine Hostkonfiguration wurde geändert. User-Linger ist nicht aktiviert, deshalb wurde kein davon abhängiger User-Dienst verwendet. **Kein Watchdog-Neustart, keine Restart-Schleife, kein automatisches Wiederöffnen.** Der geprüfte Monitor selbst prüft Heartbeats/Fatalmarker und sichert bei Abbruch seine Evidence.

Die Ablage heisst lokal `run_20261018_b0_b4_24h/private`, enthält Auftrag, Ownerdelta, unveränderte Monitorsnapshots, Prozessidentität inklusive PID-Startticks/Host-Boot-ID, Live-Bootnachweis, Zeitreferenzen und laufende `uart_capture`-Dateien. Verzeichnisse sind 0700, private Dateien 0600, Quellsnapshots 0400. Vollständige Pfade, Host-UUID, MAC/SSID, Zugangsdaten und Readbacks werden nicht veröffentlicht. Der im Manifest gehashte Rohdatenpräfix ist eine private **Anfangsstichprobe**, keine Versiegelung des weiterlaufenden Rohmitschnitts.

Vor Sessionende wurde derselbe Prozess nach Ende des Startwerkzeugs erneut geprüft: laufend, Elternprozess PID 1, frische Heartbeats, kein ABORT/Abschluss, kein weiterer Reset oder HTTPD-Warnhinweis zu diesem Prüfzeitpunkt. Das ist keine Vorwegnahme des morgigen Kontinuitätsnachweises.

## Ruhephase und Wiedereinstieg morgen

Bis B1 keine ESP32-Tabs, HTTP-Anfragen, Pings oder aktiven Erreichbarkeitsproben. Board/USB und Host bleiben unverändert in Betrieb; Host nicht in Standby/Ruhezustand setzen. Normale Firmware-/WLAN-Hintergrundaktivität und passive UART-Aufzeichnung laufen weiter. Die zuletzt vom Owner geschlossenen bekannten Clients bleiben geschlossen; unbekannte Clients sind nicht unabhängig ausgeschlossen. Ohne zusätzlichen Netzwerkmitschnitt wird keine globale 24-h-HTTP-Freiheit aller unbekannten Clients behauptet.

**Morgen ausschliesslich vorhandene Prozess- und Evidencedateien lesen. Keine UART-Portöffnung zum „Prüfen“.** Gleiche PID allein genügt nicht: PID-Startticks und Host-Boot-ID müssen passen; Prozess muss leben; kein `ABORT`, kein abgeschlossener `capture.json`, frische Heartbeats (≤5 s) und der gesamte nach dem initialen Reset qualifizierte Verlauf müssen stimmen. Streng steigende Uptime ohne >5-s-Sprünge zwischen qualifizierten Heartbeats, Chunk-Epoch-/Monotonic-Abstände (auch mögliche Host-Suspend-Lücken), genau ein erwarteter initialer Reset, keine Fatal-/Liveness-Anomalie und die vollständige ≥86400-s-Frist prüfen. Das [Testblatt](TEST_SHEET.md) enthält die Fortsetzung.

Ist der Recorder beendet, der Port verloren, das Board/der Host neu gestartet oder die Evidence unterbrochen: **HOLD, keine erneute Portöffnung/kein Reset/kein automatischer Folgelauf.** Die gewünschte unveränderte 24-h-Vorbedingung wäre dann nicht belegt. Ein kaputter Recorder wird nicht vor dem Test unbemerkt neu gestartet.

Windows-NetLog und der HAR-Export werden morgen **lokal ohne ESP32-Zugriff** vorbereitet und verifiziert. Diese Browseraktionen öffnen keinen UART-Port und booten das Board nicht. Die zusätzliche 24-h-Vorphase wird durch UART und dokumentierte Clientruhe belegt; der Browser-NetLog beginnt erst vor B1 und läuft dann bis zum Ende einschliesslich der separaten B3-Ruhephase. Ein Browsertrace für die gesamte zusätzliche 24-h-Vorphase wird nicht behauptet.

## Umfang und Übergabe

Nach erfolgreichem Wiedereinstieg und verfügbarer manueller Browserbedienung derselbe einmalig autorisierte Ablauf: B1 öffnen/anmelden, B2 900 s, B3 1200 s ohne Geräte-HTTP, B4 einmal wiederverbinden und bei Erfolg 600 s beobachten. Keine technische Zwischenfreigabe. Die 24-h-Vorphase ersetzt **nicht** B3. Der zusätzliche lange Vorzustand wird im tatsächlichen Last-/Zeitprofil dokumentiert; er wird nicht als Test mit unmittelbar davor frisch gebootetem Board ausgegeben.

Bei reinem Browser-/Transportfehler zuerst Fehler/HAR/NetLog sichern, Phase FAIL_OBSERVED; nur bei weiter sicherem Monitor genau eine im Ownerauftrag erlaubte Erholungsnavigation, danach beenden. Bei Geräte-/Safety-/Monitor-Hard-Stop keine Erholungsnavigation. Kein Flash/Build/Erase/Rollback, kein Produkt-/Monitorfix, kein Netzwerk-Konfigurationswechsel. Morgen keine neue technische Freigabeschleife; fehlende praktische Mitwirkung abwarten.

Dieses Paket enthält ausschliesslich Vorbereitungsevidence; JSON-/Link-/Prüfsummen-/Secret-/Diffprüfung ersetzen keinen abgeschlossenen 24-h- oder Browsertest. Rohdaten bleiben privat, endgültige Hashversiegelung erst nach Ende. Ergebnisse danach pro Phase ausweisen und zum unabhängigen Ergebnisreview übergeben. Auch ein vollständig erfolgreicher Weblauf qualifiziert höchstens den G2-Web-Teilumfang; P-A/P-A2/P-B/P-R, G1/F4, Sensoren und Aktoren werden nicht dadurch abgenommen.

`G2_B0_B4_ONE_RUN=AUTHORIZED` · `PRETEST_IDLE_24H=IN_PROGRESS` · `B0_BOOT=PASS` · `B1_B4=NOT_RUN` · `FLASH=NO` · `G1_QUALIFICATION=NOT_PROVEN` · `F4_THRESHOLD=PROVISIONAL_PENDING_G1` · `RES-01=BLOCKER` · `PR203_FINAL_GO=HOLD` · `ACTUATOR_RELEASE=NO`
