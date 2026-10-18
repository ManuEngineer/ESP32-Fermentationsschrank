# Builderprüfung der Ergebnis-Evidence

**Offline-Evidenceprüfung PASS; Hardware-/Abnahmeergebnis FAIL/HOLD. Kein Independent Review ersetzt.**

- Unveränderte private Originale anhand der privaten Seals erneut bytegehasht: 20 Browseroriginale und geschlossene Capturedateien. Der ursprünglich dokumentierte Raw-Präfix passt zum finalen UART-Rawfile.
- 15 durch die drei Offline-Ableiter erzeugte Dateien in einem zweiten privaten Ausgabeverzeichnis erneut erzeugt und bytegleich verglichen. HAR-/NetLog-Auswertung zuvor zusätzlich gegen die historische Erstabruf-Evidence geprüft: 8025 HTML-Bytes, 67 anonyme Gruppen, 205 Boardrequests. Historische Tests sind keine neue Hardware-Evidence.
- JSON-Dateien und Python-Syntax geprüft. Gzip-Dekompression, vollständige CSV-Zeilenzahlen und unkomprimierte SHA-256 gegen UART_CONTINUITY verifiziert; Chunklängensumme entspricht privater UART-Rawgrösse. 104446 qualifizierte Heartbeats, streng steigende Uptime, kein weiterer Reset.
- Öffentliche Dateien per Allowlist abgeleitet und gegen die privat aus den Originalen gewonnenen Passwort-/Cookie-/CSRF-Werte sowie bekannte private Netzwerkkennungen/Pfade gescannt. Keine Treffer; ausgewählte UART-Zeilen sind LF-normalisiert, Auslassungen markiert, keine zusätzlichen Leerzeilen am Dateiende. Keine vollständigen HTTP-Bodies, Authheader oder Rohscreenshots publiziert. Dies ist keine allgemeine DLP-Garantie für beliebige unbekannte Secrets.
- Relative Dokumentlinks, Ergebnisprüfsummen, Quellhashes der Ableiter und `git diff --check` geprüft. Produkt-/Monitor-/Konfigurationsdiff leer; ausschliesslich neue Dokumentation und Offline-Evidence.
- Keine Firmwarebuilds, neue Hardwaretests, Portöffnungen oder Pre-Ready-Gates für die Evidence-Veröffentlichung.

Originale bleiben lokal geschützt. Die Ableiter benötigen Zugriff auf diese privaten Originale; öffentliche Hashes und Metadaten ersetzen deren vollständige unabhängige Einsicht nicht. Dateinamen in RESULT_MANIFEST sind Artefaktaliase ohne private Verzeichnispfade. Gzip-Dateien enthalten nur die im UART_CONTINUITY beschriebenen numerischen Spalten; zur Prüfung lokal mit Standard-Gzip dekomprimieren.

Beispiel für die reine Offline-Wiederableitung mit selbst gesetzten privaten Pfaden:

```sh
python3 inspect_har_offline.py "$RUN_PRIVATE/logs/B1_ROOT_18_10_06.har" "$RESULT_OUT/HAR_B2_FULL.json"
python3 inspect_har_offline.py "$RUN_PRIVATE/logs/B4_FINAL_18_10.har" "$RESULT_OUT/HAR_B4_FULL.json"
python3 inspect_netlog_offline.py --netlog "$RUN_PRIVATE/logs/chrome-net-export-log18.10.json" --target "$BOARD_ADDRESS_PRIVATE" --out "$RESULT_OUT/NETLOG_B0_B2.json"
python3 derive_uart_offline.py --run-dir "$RUN_PRIVATE" --out "$RESULT_OUT"
sha256sum -c RESULT_SHA256SUMS
```

Die zusammengesetzten Statistiken und manuellen Beobachtungen benennen ihre Auswahl-/Zeitgrenzen separat. Ohne Sichtbarkeits-/JSON-Settlementinstrumentierung darf aus HAR-Transportzeiten keine exakte JS-Promise-Lebensdauer konstruiert werden.
