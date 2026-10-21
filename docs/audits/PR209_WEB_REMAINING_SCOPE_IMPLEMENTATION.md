# PR #209 – R1-Web-Restumfang: Implementierungsnachweis

Stand: 2026-10-21; C0/C1/C2/C3 umgesetzt; C5 Software integriert, abschliessende Nachweise laufen.
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

## C3 – historischer Schnittnachweis

Geschlossener `WebMutationDto`-Dispatch hinter den gemeinsamen Gates; Run-
Envelope unveraendert. Settings-Body bis 1024 B im gemeinsamen Digest, Run-Codec
weiterhin 480 B. Bestehende lokale Namenslogik in einen privaten Application-
Eintritt gezogen; Herkunft ausschliesslich intern: WebInterface/3, Touch und
Sprachwechsel LocalDisplay/2. Settings-GET liest Name, Zone und Revision aus einer
Lease, Editable aus demselben Application-Lock; keine schreibende Zeitzonen-API.

- JS 83/83 PASS, inklusive DE/EN/ES, Bestaetigungsbindung, nameEditable und
  Stale-/Busy-Sperren.
- Native Web/Session/Konfiguration/Touch: 244/244 PASS. Alle 8 Preview- und
  12 Commit-Status plus unbekannte Werte geprueft; Preview-Fehler ignorieren
  Commit-Platzhalter. Indeterminate wird ausschliesslich Unavailable.
- 14 Fehler-/Gatevarianten ohne Application-Aufruf und ohne Manifestaenderung;
  Replay ohne zweite Mutation, anderer Payload bei gleicher Sequenz verworfen,
  max. 1024-B-Body akzeptiert, +1 verworfen. Testzaehler nur im nativen Profil.
- Persistiertes Manifest ueber GraphStore aus dem Store dekodiert:
  WebInterface/3 bzw. LocalDisplay/2. NoChange, stale und aktiver Lauf ohne
  neues Manifest; Hostname/SSID/start-/stop-Zaehler unveraendert, Session bleibt.
- Asset 15670 B (714 B bis Absolutlimit). Die C5-Abschlussreserve von 1024 B
  ist damit **noch nicht erfuellt**; in C5 zu reduzieren, keine Limiterhoehung.
  C2-Delta +3176 B gegen C1 ist ebenfalls ein D2-Warnsignal gegen die 1,5–2-KB-
  Schaetzung: Bestaetigungs-/Stale-Bindung und editierbare Felder benoetigen mehr
  Code als geschaetzt. Grenzen und Scope bleiben unveraendert.

## C5 – Integration und abschliessende Nachweise

C0 `d1170b2`, C1 `d2446ee`, C2 `27aa12c`, C3 `57fa250`.
Die neue UI teilt Elementerzeugung, Konfigurations-/Katalog-Lesehelper und
Bestaetigungs-/Renderpfade; keine Abhaengigkeit oder Minifier in Build/Laufzeit.
Die feste Asset-Grenze und ihre Abschlussreserve wurden nicht erhoeht.

Aktueller Script-Nachweis: JS 87/87 PASS (einschliesslich der bisherigen 74);
Webtests 117/117 PASS. Asset **15360 B**, Reserve **1024 B**. Normale Shell
1751 B authentifiziert (20-stellige Sequenz: 1770 B), 1750 B Passwortschutz aus;
Setup-Shell unveraendert 3916 B. Beide Shells halten die 128-B-Normalreserve,
Setup 180 B frei. Asset-CRC `c29ab140`, exakt aus den ausgelieferten Bytes.

Host-Allokationsfenster (64-Bit, angeforderte Bytes ohne Allocatoroverhead;
keine ESP32-Messung, alle cJSON-Allokationen freigegeben; kalt/warm identisch):

| Handler | Anzahl | angefordert | groesster Block | max. lebend | Body |
|---|---:|---:|---:|---:|---:|
| Shell (gueltige Session) | 13 | 3025 B | 2049 B | 2082 B | 1751 B |
| Asset | 1 | 51 B | 51 B | 51 B | 15360 B Flash, 0 B besessener Koerper |
| Settings | 28 | 1598 B | 193 B | 604 B | 111 B |
| Programmdetail (aktueller 4-Eintrag-Katalog) | 64 | 4027 B | 928 B | 2769 B | 347 B |

Programmdetail-Worst-Case-Body mit 16 Eintraegen, 96-B-Name und max. Revision
ist separat nativ geprueft. Das Handlerfenster oben ist kein Maximal-Heapnachweis
fuer alle moeglichen 16-Eintrag-Kataloge; deren Kopien und Fragmentierung bleiben
bei der Hardware-/R1-Qualifikation zu bewerten. Kein `RAM_PASS` daraus.

Offline-Builds beider Profile, Sektions-/Stackvergleich und abschliessender
Builder-Self-Check stehen noch aus. Pre-Ready und D9 bleiben NOT_RUN.
