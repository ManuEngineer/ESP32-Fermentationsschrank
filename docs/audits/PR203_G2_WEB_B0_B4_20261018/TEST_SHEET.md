# PR #203 – B0–B4, ein Lauf nach mindestens 24 h Vorruhe

Massgeblich: [Ownerauftrag](OWNER_TASK.md), ergänzender Ownerentscheid und tatsächlicher Start in [PREPARATION.md](PREPARATION.md). Dieses Blatt erlaubt keine erneute UART-Portöffnung. Produktquelle `a9a1156a1d44d4679957526bbc34a30be6422e4e`; Monitor `893f3bb73a50b484ce79b3b80722873452d998fc`, `g2-trend`, Gate-ID `PR203_SINGLE_FLIGHT_G2_WEB_20261016_IMAGE01`. Hashes: [Manifest](ARTIFACT_MANIFEST.json).

## B0 – denselben Recorder fortsetzen, dann Browser vorbereiten

1. Repository/Live-PR/kanonischen Handover erneut lesen. Der Produkt-HEAD darf seit Vorbereitung nur dieses Dokumentationspaket enthalten; bei unerwartetem Produkt-/Monitor-/Artefaktdelta HOLD. Private Prozessreferenz, laufendes RUNTIME und unveränderte Monitorsnapshots verwenden.
2. **Nur lesen:** Host-Boot-ID, Recorder-PID **und Startticks**, lebender Prozess, gleiche Kommando-/Quellhashes, vorhandene kontinuierliche UART-/Chunkdaten. Kein `ABORT`/Abschlussresultat, letzter qualifizierter Heartbeat ≤5 s alt. Gesamten Verlauf seit dem vollständigen initialen POWERON_RESET prüfen: keine weitere Reset-/Fatalmarkierung, streng steigende Uptime ohne >5000-ms-Sprung zwischen qualifizierten Heartbeats, keine >5-s-Livenesslücke. Epoch-/Monotonic-Zeitpaare der Chunks ebenfalls auf Host-Suspend-/Aufzeichnungslücken prüfen; eine frische Endzeile allein qualifiziert nicht die Nacht. Vor dem Marker gepufferte Zeilen weiterhin unqualifiziert lassen. Bei Unterbruch HOLD; niemals den Monitor starten/neu starten, kein Terminalprogramm/`esptool`/`idf.py monitor` am Port öffnen.
3. Aktueller Host-Monotonic minus gespeicherter Vorruhestart ≥86400 s. Zusätzlich den geplanten Beginn **18.10.2026 frühestens 12:00 CEST** einhalten. Keine aktive ESP32-Anfrage zur Prüfung. Letzte vorhandene UART-Netzwerkereignisse und passive UniFi-Daten notieren; fehlende Kontinuitäts-/WLANbelege offen ausweisen. Unbekannte Clients als Einschränkung erhalten.
4. Ownerverfügbarkeit und bekannte geschlossene ESP32-Clients bestätigen. Tatsächlichen Windows-Chrome/Version, Zugangsweg und Cache-/Netzwerkbedingungen notieren, unverändert lassen. Host-UTC/Monotonic und Windowszeit sowie UniFi-Zeitzone/Zeitabweichung soweit messbar festhalten; Nachrichtentransportzeit nicht als Uhrabweichung ausgeben.
5. **Noch ohne Geräteaufruf:** separaten `chrome://net-export`-Tab öffnen, Defaultmodus ohne sensitive Header/Raw Bytes; Datei in neuem privaten Laufordner starten, im Explorer Existenz prüfen. Leeren Testtab mit DevTools Network-Aufnahme/Preserve log vorbereiten. HAR-Export mit Inhalt an einem unkritischen Eintrag (z. B. `https://example.com/`, keine ESP32-Adresse) tatsächlich üben: Datei gültig, mindestens ein Eintrag mit Inhalt. Probleme hier vor B1 beheben. NetLog ab jetzt einschliesslich B3 durchgehend weiterlaufen lassen. Keine Log-Inhalte oder Zugangsdaten in Chat/GitHub.
6. Erst wenn Kontinuität, Mindestfrist und Recorderbedienung belegt sind, B0 abschliessen und ohne weitere technische Freigabefrage B1 anweisen. Die organisatorische Wartezeit bis zur Ownerbereitschaft ist kein neuer Boot und kein neuer Lauf.

## B1/B2 – öffnen, anmelden, 15 Minuten

- Genau eine Navigation im instrumentierten ESP32-Tab. Root-Status, vollständigen HTML-Abschluss, Ladezeit und UTC notieren; sichtbare Seite/200-Header allein genügen nicht. Normal mit korrektem Passwort anmelden; keine Mutationen/Programmstarts. Online/Bereit und erwartete fehlende Sensoren erfassen. Anonyme/abgelaufene 401 gesondert dokumentieren.
- B2-Zeitnull nach bestätigter Anmeldung und frischem vollständigem Snapshot markieren; 900 s normales 5-s-Polling. Bei 0/300/600/900 s UI-/Frischezustand, offene Requests, Start-/End-/Body-/JSON-Zeiten und Fehler plus passive UART/UniFi-Werte notieren. Grouping aus status/temperatures/alerts einschliesslich Bodyabschluss beurteilen, keine blossen Header-PASS. Keine künstliche Last und keine zweite Testseite.
- Latenzen im tatsächlichen Profil berichten, keine neue nachträgliche R1-Grenze. 30-s-JS-Deadline ist ein bestehender Browsermechanismus, keine neue Abnahmegrenze; ein beobachteter Timeout/Body-/JSON-/Transportfehler bleibt Fehler. Main-HWM unter 11160 B ist nur Trendbefund; echte Overflow-/Fatalmarker und bestehende HTTPD-Grenze bleiben Hard-Stop.
- **Vor Tab-Schluss:** B0–B2-HAR dauerhaft speichern und Datei prüfen. Erst dann ESP32-Tab schliessen; NetLog-Tab/Chrome bleiben offen. Schliesszeit, letzte Requests und Host-Monotonic markieren.

## B3 – zusätzliche 20 Minuten Geräte-HTTP-Ruhe

- 1200 s ab dokumentiert geschlossenem ESP32-Tab, ohne ESP32-HTTP/Pings/Erreichbarkeitsproben. Bei 0/600/1200 s UniFi rein passiv notieren. Der Browser-NetLog und derselbe UART-Recorder laufen weiter. Die heutige 24-h-Vorphase wird nicht auf B3 angerechnet.
- Vor B4 wiederum nur vorhandene Monitor-/Evidencedateien prüfen. Keine neue Portöffnung oder Geräteprobe. Bei Fehler keine automatische nächste Phase.

## B4 – einmal verbinden, bei Erfolg zehn Minuten

- Neuen leeren ESP32-Testtab erst mit Network-Aufnahme/Preserve log vorbereiten, dann genau eine Rootnavigation. Ersten Request samt Transferabschluss/Fehlerklasse sichern; falls erforderlich normal neu anmelden. Keine unprotokollierte weitere Navigation.
- Bei Erfolg 600 s normales authentifiziertes Polling; Messpunkte 0/300/600 s wie B2. Keine Mutationen. B4-HAR auf Platte exportieren und prüfen; dann ESP32-Tab schliessen/keine weiteren Anfragen bestätigen lassen. NetLog erst danach stoppen und Datei prüfen, damit auch das Schliessen erfasst ist. Anschliessend **nur die bestehende** UART-Capture über deren STOP-Marker geordnet beenden. Tatsächliche Uhrzeiten/Reihenfolge und geplante Schliess-Abbrüche dokumentieren.

## Stoppen und geordnete Fehler-Evidence

- **Geräte-/Safety-/Monitor-Hard-Stop:** sofort weitere Browserlast stoppen, keine Erholungsnavigation, kein Logout/Reset/Flash. Vorhandene Dateien sichern; der Monitor sichert sein begrenztes passives Ende. Ein noch laufender NetLog erhält den Ablauf bis Tab-Schluss. Kein zweiter Hardwarelauf.
- **Nur Browser-/HTTP-/Transportfehler bei sicherem Monitor:** Phase FAIL_OBSERVED, nachfolgende Phasen NOT_RUN. Tab/DevTools zunächst offen behalten; vollständige Fehlerklasse, UTC, Screenshot/Console und HAR auf Platte sichern. Monitorzustand erneut aus bestehenden Dateien prüfen. Erst danach genau **eine** ausdrückliche Erholungsnavigation im selben Trace, auch deren HAR speichern. Danach Lauf beenden; kein dritter Aufruf/automatischer Retry. Automatische Requests der unveränderten Shell in der tatsächlichen Chronologie offen erfassen.
- Ausnahmen ausschliesslich bekannter `/favicon.ico`-404 und eindeutig erwartete anonyme/abgelaufene Session-401. Absichtlich durch den gemeinsamen Controller abgebrochene Geschwister nachvollziehbar von echten Transportfehlern trennen; Zweifelsfälle nicht als PASS ausgeben.

## Abschluss

Originale privat, unverändert, mit Hash/Grösse und Zeitreferenzen sichern; öffentliche Daten nur allowlist-bereinigt. Pro B0–B4 PASS/FAIL/NOT_RUN/NOT_PROVEN; 24-h-Kontinuität separat beurteilen. Header ohne Body-/JSONabschluss reichen nicht. Heap-/HTTPD-Stack-/Recovery-/F4-/Touchbelege, die das reine Produktimage nicht liefert, bleiben NOT_PROVEN; kein Bootminimum als Recovery verwenden. Historische FAIL/HOLDs bleiben unverändert.

Ergebnisbericht auf PR #203 veröffentlichen und für Independent Ergebnisreview anhalten. Kein selbstständiges G2-Gesamt-PASS, Produktfix, Pre-Ready, Ready-Wechsel oder Merge. `PR203_FINAL_GO=HOLD`, `ACTUATOR_RELEASE=NO`.
