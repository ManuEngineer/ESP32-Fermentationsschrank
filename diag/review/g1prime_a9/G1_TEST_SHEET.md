# G1′ IMAGE01 – konkretes Testblatt (noch nicht ausgeführt)

Gate `PR203_RES01_G1PRIME_A9_20261018_IMAGE01`; maßgebend [G1_GATE.md](G1_GATE.md),
[Coverage](COVERAGE.md), [Manifest](G1_ARTIFACT_MANIFEST.json).
Hardwarestatus überall PENDING/NOT_PROVEN; keine Vorab-PASS-Einträge.

## Vorbedingungen nach exakter Ownerfreigabe

Owner am Display/Touch und im einen normalen Chrome verfügbar. Alle bekannten
anderen Testclients geschlossen; unbekannte Clients als Limit. Keine bekannte
laufende Capture unkontrolliert schließen. Chrome-Version/Zugangsweg und UTC
über PowerShell erfassen, Host UTC+monoton und UART us/uptime zuordnen, Unsicherheit
festhalten. UniFi nur vorhandene Anzeigen mit Zeit/Zeitzone lesen; keine Probe.
NetLog im Defaultmodus ohne sensitive Header/Raw Bytes in einer nachweislich
existierenden privaten Datei; leerer Network-Tab mit Preserve log, HAR-Export
mit Antwortinhalten lokal geprüft (ohne ESP-Zugriff). Cache/Drosselung unverändert.
Keine Passwörter/Token im Chat; keine Run-/Start-/Bestätigungsaktion im Browser.

Bootnachweis vollständig im neuen Mitschnitt: Quell-SHA des Diagnoseimages,
esp32_release, real actuators: disabled, application: ready, HomeConnected,
mindestens zwei frische Heartbeats nach genau einem gültigen initialen POWERON_RESET ≤5 s;
Identität ≤30 s. Erst dann Browser öffnen, normal anmelden, API 200 prüfen.
Vorher gepufferte Zeilen qualifizieren keinen neuen Boot.

## Referenz, 18 Minuten Coverage, Erholung

Nach Login eine lebende Session behalten, Ausgangssprache/Home notieren,
ESP-Tab schließen und 60 s Home-Idle als R_UI_ref erfassen. Danach denselben
Browser/Cookiekontext einmal öffnen, ohne Neuladen und ohne zusätzliche Session.
Falls Login erneut nötig/Session unklar: Referenz nicht vergleichbar, markieren;
keinen technischen Reparatur- oder Referenz-Wiederholungslauf improvisieren.

Die 18 Minuten beginnen erst mit Online und drei vollständigen API-200-JSONs.
Ein Browser mit unverändertem normalem 5-s-Polling ist die einzige Netzlast.
Pro physischem Einzelschritt vorab genaue sichtbare Schaltfläche, Reaktion und
Dauer nennen; Ownerbestätigung abwarten (`wie erwartet / keine Reaktion /
abweichender Bildschirm / unklar`). Host UTC/monoton vor Anweisung und nach
Bestätigung notieren. Zwischen Kontakten vollständig loslassen und ≥2 s
verweilen, damit Records/Seitenwechsel ablesbar werden. Keine simulierten
Touches. UI-Seiten-/Locale-IDs und Phase-1/2-Daten tatsächlich korrelieren;
fehlende Records = NOT_PROVEN, kein nachträglicher Ersatz durch Erinnerung.

| Coveragefenster | Konkrete Navigation, nur wenn sichtbar/angeboten | Erwartung und Guard |
|---|---|---|
| 0–3 min | Home → obere Uhr → Zurück; obere Netzwerkanzeige → nur ansehen → Zurück; Home → Einstellungen → Webzugang → Zurück; Status → Diagnose → Zurück | Seiten 23/22/24/17 + aktueller Anker 0/16; keine Netzmodus-/Einrichten-/Webzugang-öffnen-Schaltfläche berühren |
| 3–7 min | Obere Sprachkennung → Español; Home/Status → Technisch/Meldungen; Uhr/Zurück; vorhandene Programmliste → vorhandener Eintrag | Tatsächlich locale=3, keine bloße Listenvorschau; Programm nur ansehen, nie Start/Bestätigen. Leere Meldungsseite statt erfundener Meldung dokumentieren |
| 7–11 min | Programmliste Start-Ansicht → Eintrag → Übersicht; zurück; Home → Programme-Verwaltung → Eintrag → Bearbeiten; vorhandenes Zahlenfeld/Textfeld öffnen, Keyboard-Modi ansehen → Abbrechen; Start-Programmliste → Manuell → Halten/Zeitbetrieb ansehen → Zurück | Seiten 2/6/3/25/27/7/8/9 soweit erreichbar. Keine Wertübernahme, kein Speichern/Löschen/Programmstart. Keine künstlichen Maximaltexte; tatsächlich vorhandene Texte/längste sichtbare Labels protokollieren |
| 11–14 min | Sprache → English: Home/Uhr und noch fehlende kurze Text-/Editoransicht; Sprache → Deutsch: entsprechende fehlende Ansicht | de/en nur gezielte fehlende Kombinationen und Anker; historische gesamte Seitenfolge nicht wiederholen. Persistente Sprachwahl ist expliziter Teil dieses Gatevorschlags |
| 14–17 min | Einstellungen → Service → PIN (falls gesperrt) → PIN vergessen → Warnung → Bestätigen → Bestätigen bis Hold-Ansicht, jeweils separat bestätigen; erst danach kurze Hold-Berührung höchstens 1 s → vollständig loslassen ≥2 s → Abbrechen | Nur wenn vorhandener Resetflow korrekt angeboten. Keine PIN-Eingabe/Lease-Änderung. Kein 5-s-Halten, keine Ausführung/Finished. Unterbrechung bei unklarer Bedienung. Kurzer Hold/Release ist kein Hold-A/B-Nachweis nach Skip |
| 17–18 min | Ausgangssprache über normale Sprachseite wiederherstellen; Home, Hände vollständig weg; offene fehlende sichere Ansicht nur bei verbleibender Zeit | Nicht hektisch nachholen, 18 min Ziel/20 min harte Obergrenze der Coverage. Fehlende Fälle bleiben NOT_PROVEN |

Falls der Zustand Home/Standby oder die Navigation abweicht, nicht durch
Sensor-/Commissioning-/Programm-/Kalibrierungsaktionen herstellen. Fehlende
Fälle auslassen und bezeichnen; D9 ist separat. Bei vorhandener gültiger
Kalibrierung im sicheren Menü zusätzlich 0,5–1 s Kontakt, frisches physisches
Loslassen und neuer kurzer Kontakt an einer nicht destruktiven Navigation
beobachten. Dabei `held`, `release`, `sample_age_ms`, `allowed` mit den
Zeitfenstern abgleichen; es handelt sich ohne Skip nicht um eine gesperrte
Hold-Wiederfreigabe. Wenn kein natürlicher Skip auftritt, keine Erzeugung
versuchen. Bei natürlichem Skip Hände lösen, UI-Erholung und frische
Release-/Kontaktmessung dokumentieren, keine 5-s-Reset-Hold-Sequenz erzwingen.

Nach Coverage vollständigen HAR vor Tab-Schluss sichern; NetLog weiterführen.
Home und ursprüngliche Locale, gleiche eine Session, Browser geschlossen:
120 s Erholung; freier interner 8-Bit-Heap und größter Block gegen R_UI_ref,
Main-/HTTPD-HWM, min_free nur Kontext. Referenzbedingungen/Sessionabweichung
explizit prüfen. Keine 90%-Freigabegrenze erfinden: dies ist nicht R1-U-R_ref.
Danach NetLog stoppen, Dateien prüfen, Host-STOP für Capture; keine weitere
Geräteabfrage und kein automatischer Rückflash. Port-Schließeffekt protokollieren,
keine Wiederöffnung zu seiner Prüfung.

## Fehler, Guard und Auswertung

An Messpunkten 0/5/10/15/18 min UI/HTTP-Zustand, lange Requests, Gate-Zähler,
Blockreserve, Ressourcen/HWM und UniFi-Werte soweit vorhanden notieren.
Probe-/Stack-/Identitäts-/Heartbeatfehler sowie unerwartete HTTP-/Transport-/
Timeoutfehler sofort STOP/HOLD. Erst vorhandenen Browsertrace/HAR sichern,
Tab schließen; kein Reload/zweiter Versuch. Host-ABORT und STOP setzen.
Browser-Aborts als Folge des absichtlichen Tab-Schlusses zeitlich kennzeichnen;
erwartete anonyme/abgelaufene 401 und /favicon.ico 404 nur als im Gate benannte
Ausnahmen. Interne Netzwerkfehler mit vollständiger Klasse/Retryfolge berichten,
nicht still als Erfolg wegerklären. Capture liest nach Anomalie höchstens 5 s
passiv auf bereits offenem Port, sendet nichts; danach keine Geräteaktionen.

PASS nur für zeitlich/raw belegte Pfade im geprüften Image. FAIL für Stopereignis;
NOT_PROVEN für fehlende Kontakte/Locale/Varianten/Spitzen/Skip/Hold oder Vergleich.
Checkpoint-Deltas nicht kausal als UI-b ausgeben, kein 33536-Vorschlag.
Endbericht: Image-/Flashidentität, UTC/monotone Zuordnung, HTTP/Dauern/
Pollinggruppen, UI-/Touch-/Holdmatrix, Heap/Block/HWM/Recovery samt Limits,
Fehlerreihenfolge/Stopverhalten. Ohne ausreichendes b/Skip-/Hold-/Peak-Coverage
`F4_THRESHOLD_NOT_QUALIFIED=HOLD`. Genau ein Lauf; unabhängiger Ergebnisreview,
kein G2, kein Pre-Ready und keine automatische Schwellenänderung.
