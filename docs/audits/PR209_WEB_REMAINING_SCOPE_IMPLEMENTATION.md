# PR #209 – R1-Web-Restumfang: Implementierungsnachweis

Stand: 2026-10-21; C0/C1/C2 umgesetzt und gezielt geprueft; C3/C5 nicht begonnen.
Freigegebener Plan: `b7d5e2dbc1f5281c764d4a75f2c37f1c4b6397c2`,
[`issue-27-r1-web-remaining-scope-plan.md`](../tasks/issue-27-r1-web-remaining-scope-plan.md).
Der Owner hat diese exakte SHA und die Implementierung C0 → C1 → C2 → C3 → C5
am 2026-10-21 im Auftrag freigegeben. Die Sperrangaben in der freigegebenen
Planfassung dokumentieren deren damaligen Planungsstand.

`CONTEXT_BASELINE_BRANCH=agent/issue-27-r1-web-navigation-plan`
`CONTEXT_BASELINE_SHA=b7d5e2dbc1f5281c764d4a75f2c37f1c4b6397c2`
`CONTEXT_PLAN_SHA=b7d5e2dbc1f5281c764d4a75f2c37f1c4b6397c2`
`CONTEXT_REFRESH_MODE=INCREMENTAL`; Branch/HEAD/PR und main live bestaetigt,
keine Aenderungen seit dem Plan-Fix, betroffene Code-/Vertragsquellen neu gelesen.
Issue #27 OPEN; dessen Statuskopf vor Merge #206 ist historisch ueberholt.

Keine Hardware-, Flash-, Browser-, UART-, Reset- oder Aktorfreigabe.
`ACTUATOR_RELEASE=NO`, `RAM_PASS=NOT_PROVEN`, `D9=NOT_RUN`,
`PRE_READY_LOCAL_GATES=NOT_RUN`, Independent Full Review offen.

## C0 – historischer Schnittnachweis

- Getrennte Shell-Grenzen 2048 B / Setup 4096 B, Asset 16384 B; exakte
  Grenz-/Grenze+1-Tests mit JSON-Ersetzung ohne Anhang. Normalreserve mit
  maximalem Sequenzaufschlag geprueft. Setup inline unveraendert.
- Vorhandenes Script bytegleich ausgelagert (9773 B reines JS), CRC-32-Pfad,
  oeffentliche byte-konstante Antwort ohne Sitzung/Cookie, GET-only,
  Metadaten-/Bodypruefung, Fehlerhinweis in der Shell. Geschlossenes
  Cache-Metadatum nur fuer exakte Asset-Route.
- `makeProgramSummary` unveraendert fuer beide UI-Verbraucher deklariert;
  keine zweite Projektion. Programmdetail-Groessenbeispiel mit maximaler
  Revision, 48-B-Identifier und 96-B-Name aus Quotes: unter 3072 B;
  der ausfuehrbare Codec-Nachweis folgt mit C2. 16 Katalogeintraege aendern
  die Einzelantwort nicht; die vorhandene Listen-API bleibt begrenzt.
- `node scripts/test_web_shell_polling.mjs`: 74/74 PASS, Shell und Asset in
  zwei getrennten VM-Skripten (Fake-DOM, kein Browser).
- `pio test -e native -f test_web_application_routes -f test_http_server_lifecycle
  -f test_local_touch_ui`: 177/177 PASS; davon Web 111, Touch 61, HTTP 5.
- ESP-IDF-Linux-Adapterhost: Build PASS; realer Loopback-HTTP-Test PASS,
  Chunk-/Sendefehlervertrag und Immutable-/Default-/Invalid-Cache geprueft.
- Architekturguard PASS, `git diff --check` PASS. Keine Firmware-/Hardware-
  Qualifikation daraus; vollstaendige Offline-Ressourcenmessung in C5.

## C1 – historischer Schnittnachweis

Navigation Uebersicht/Meldungen nur mit LP, Fragment-Allowlist und aria-current,
umbrechende Leiste; Sichtwechsel ohne Request. Meldungsansicht zeigt alle
maximal 16 gelieferten Eintraege in API-Reihenfolge mit Schweregrad und Zustand;
Zeilenaktionen sind bei Busy/Stale/Offline/erledigt/erreichtem Zustand gesperrt.
Die Uebersichts-Aktionsleiste bleibt erhalten.

JS 77/77 PASS (74 bestehende plus DE/EN/ES-Faelle fuer Navigation und 16 Meldungen,
korrekte ID/Intent sowie Sperren); native Webtests 111/111 PASS.
Asset 11023 B, Delta gegen C0 +1250 B. **D2-Warnsignal:** mehr als 25 % ueber der
0,6-KB-Schaetzung; die Messbasis enthielt weder Fragment-/aria-current-Vertrag
noch vollstaendige Zustandsanzeige. Diese erforderlichen Ergaenzungen erklaeren
das Delta; kein zusaetzlicher Scope. 5361 B bis Absolutlimit; C5 muss weiterhin
mindestens 1024 B Reserve nachweisen. Kein automatischer HOLD gemaess D2.

## C2 – historischer Schnittnachweis

Programmdetail-API mit Session, Methode/Body-/Identifiergrenzen, 404 fuer
unbekannte/deaktivierte/nicht installierte Eintraege und 503 ohne Lease.
Die Codec-Projektion verwendet genau `makeProgramSummary`, keine Overrides im
GET, null fuer fehlende Werte und `startable` aus der Runnable-Validierung.
Nativer Nachweis fuer 96-B-UTF-8-Namen, JSON-Escaping, max. Revision, 16 Eintraege
im Katalog und alle vier Anzeige-/Intent-Completion-Bindungen; Body <= 3072 B.

Programmseite mit Detail, unveraendertem Programm als Basis und nur abweichenden
Startoptionen; Eingaben/Katalogrevision/Programm sind an die Bestaetigung gebunden.
Alte/fehlgeschlagene Detailantworten verwerfen die Auswahl ohne Mutation.
JS 79/79 PASS; native Web-/Touch-Tests 174/174 PASS. Keine Browserqualifikation.
Der abschliessende Asset-/Ressourcenwert wird nach C3/C5 ausgewiesen.
