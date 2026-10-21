# PR #209 – R1-Web-Restumfang: Implementierungsnachweis

Stand: 2026-10-21; versionierte Offline-Messbasis `4f6f0c9abad9e1c354fc67e140302adcf62680e9`.
Der aktuellste HEAD-gebundene Test-/Build-/Self-Check-Stand steht im kanonischen PR-Handover.
**C0–C3/C5-Software implementiert, C5-Ressourcenabschluss FAIL/HOLD.**
**OPEN_BLOCKERS=1: Settings-HTTPD-Stack.** Weitere Produktarbeit angehalten;
Planrevision 2.3 fuer die zusaetzliche Korrektur ist noch nicht freigegeben.

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
`PRE_READY_LOCAL_GATES=NOT_RUN`, Independent Full Review offen; Builder-Self-Check auf dem nachfolgenden Dokumentations-HEAD wird im kanonischen Handover ausgewiesen.

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

Aktueller Script-Nachweis: JS 89/89 PASS (einschliesslich der bisherigen 74);
Webtests 117/117 PASS. Asset **15311 B**, Reserve **1073 B**. Normale Shell
1818 B authentifiziert (20-stellige Sequenz: 1837 B), 1817 B Passwortschutz aus;
Setup-Shell unveraendert 3916 B. Beide Shells halten die 128-B-Normalreserve,
Setup 180 B frei. Asset-CRC `e2476fdf`, exakt aus den ausgelieferten Bytes.

Host-Allokationsfenster (64-Bit, angeforderte Bytes ohne Allocatoroverhead;
keine ESP32-Messung, alle cJSON-Allokationen freigegeben; kalt/warm identisch):

| Handler | Anzahl | angefordert | groesster Block | max. lebend | Body |
|---|---:|---:|---:|---:|---:|
| Shell (gueltige Session) | 13 | 3025 B | 2049 B | 2082 B | 1818 B |
| Asset | 1 | 51 B | 51 B | 51 B | 15311 B Flash, 0 B besessener Koerper |
| Settings | 28 | 1598 B | 193 B | 604 B | 111 B |
| Programmdetail (aktueller 4-Eintrag-Katalog) | 64 | 4027 B | 928 B | 2769 B | 347 B |

Programmdetail-Worst-Case-Body mit 16 Eintraegen, 96-B-Name und max. Revision
ist separat nativ geprueft. Das Handlerfenster oben ist kein Maximal-Heapnachweis
fuer alle moeglichen 16-Eintrag-Kataloge; deren Kopien und Fragmentierung bleiben
bei der Hardware-/R1-Qualifikation zu bewerten. Kein `RAM_PASS` daraus.

Offline-Builds und Sektions-/Stackvergleich sind unten ausgewiesen.
Pre-Ready und D9 bleiben NOT_RUN; der Self-Check wird HEAD-gebunden im PR-Handover dokumentiert.

Gezielter Integrationsfix: Navbar-CSS liegt jetzt in der Shell und enthaelt
`nav[hidden]{display:none}`, damit Flex das LP-Gate nicht uebersteuert. Die
Bestätigungslabels werden nach der Invalidierung aktualisiert. Raw-HTTP-CSS-
Gegenprobe und synthetische Mutanten fuer fehlendes P-Gate, falsche Meldungs-ID,
ungueltiges Fragment und ungebundene Startanpassung schlagen gezielt an.
Der Architekturtest erlaubt dem Mutationshandler nur die vier vorgesehenen
Application-Aufrufe (einschliesslich des privaten Run-Zweigs).

Erster Profilbuild auf `75970d7`: beide Profile und Herkunfts-/Driftpruefung
PASS nach Korrektur des lokalen Python-Pfads (keine Buildvertragsaenderung).
Die Stackpruefung zeigte grosse bestehende Konfigurations-Commitframes; der
Run-Zweig wurde deshalb aus dem gemeinsamen Handlerframe getrennt. Erneute
Profil-/Stackmessung auf diesem Integrationsfix ist abgeschlossen: Builds PASS,
Settings-Stack weiterhin FAIL (unten). Der 8192-B-
HTTPD-Stack wird nicht eigenmaechtig erhoeht.

## Abschliessende Offline-Evidence auf `4f6f0c9`

- Beide ESP-IDF-Profile PASS inklusive Profil-/Herkunfts-/Driftpruefung und
  sauberem Source-Arbeitsbaum. IDF v6.1 `fff9895c82d744c7237be8847347bdd1b07c6643`,
  GCC 15.2.0; [kanonischer Buildbericht](PR209_OFFLINE_BUILD_REPORT_20261021.md).
- Gezielte native Suite: **437/437 PASS** in 11 betroffenen Bereichen
  (Web-Routen, HTTP-Port, UI-Modelle/Renderer/Steady-State, Laufdetail/Kommandos,
  Konfigurationsservice, Touch, Session und Browserpolicy). JS **89/89 PASS**,
  einschliesslich Architekturcheck und vier erkannten synthetischen Regressionen.
- IDF-Linux-HTTP-Adapterhost: Cache-/Chunk-/Sendefehlervertrag PASS (C0).
  Kein Browser-/Geraetelauf. Kein vollstaendiger Pre-Ready-Lauf.
- Messbasis fuer den Release-Sektionsvergleich: vorhandenes sauberes Artefakt
  `26173ab9f796d1539aa052b05e713272083a96cd`, produktionsquellgleich mit
  `main` `086873a` (dazwischen nur Markdown). Baseline-ELF SHA-256
  `18febc77c941af7baceeaf1f8d7aa04e01fe1fd9e7fc28a226ff04be996efd0c`.

| Release-Sektion | Basis | Kandidat | Delta |
|---|---:|---:|---:|
| `.dram0.data` | 18975 B | 18975 B | 0 |
| `.dram0.bss` | 74488 B | 74520 B | +32 B |
| `.iram0.text` | 100815 B | 100815 B | 0 |
| `.iram0.vectors` | 1028 B | 1028 B | 0 |
| `.flash.text` | 1405758 B | 1409254 B | +3496 B |
| `.flash.rodata` | 237868 B | 244060 B | +6192 B |

Die +32 B BSS stammen aus dem einmaligen CRC-Pfadcache samt Initialisierungs-
guard; der C0-Null-Delta-Wert war eine Schaetzung. Kein neuer Laufzeitpufferpool.
Release-BIN 1774560 B, SHA-256
`493aa109e0e946a285d2413e52be3d764ad8d6ec7ccb0f98fd23a6709e68545c`;
Bring-up-BIN 1786048 B, SHA-256
`68bbb73a7513fa1205a7dac2a9d6d3f72d039428bb084ad95aaf09c271993169`.
Artefaktmanifeste wurden mit dem vorhandenen Buildreport-Owner fuer `4f6f0c9`
erzeugt; keine Aussage ueber Flash, Deployment oder Freigabe.

### Stack: FAIL statt Gesamt-PASS

Beide Profile: `handleAsset` 96 B <= `handleShell` 352 B; Read-only-Handler
624 B unveraendert. `applyRunMutation` 2048 B, gemeinsamer Mutationshandler
544 B (vor Trennung 2496 B). Der neue Konfigurationszweig ruft synchron den
bestehenden Preview-/Commit-Pfad auf.

[Ausgewaehlte rohe Framewerte](PR209_HTTPD_STACK_20261021.tsv): bereits Adapter
704 + Mutationshandler 544 + `commitConfigurationChange` 272 + `confirmPreview`
3728 + `prepareCommit` 3280 = **8528 B > 8192 B**. Alle fuenf Aufrufer haben
nach dem jeweiligen Unteraufruf weiteren Code und sind hier nicht als
Tail-Calls entfallen. Die Summe laesst weitere Application-/Dispatcher-/SDK-/
NVS-Frames aus und ist deshalb eine Untergrenze, keine Vollkettenqualifikation.
Die Produkt-`.su`-Dateien stammen aus beiden kanonischen Profilbuilds; fuer den
Adapter wurde der unveraenderte Befehl aus dem jeweiligen `compile_commands.json`
mit `-fstack-usage -fcallgraph-info=su` und einem temporaeren Objektziel ausgefuehrt.
Das aendert weder Firmwarebuild noch Taskstack.

**HTTPD_SETTINGS_STACK=FAIL**, **C5_RESOURCE_GATE=HOLD**, **OPEN_BLOCKERS=1**.
Der Scope zur Korrektur des gemeinsamen Konfigurations-Commit-Pfads ist in der
vollstaendigen Planrevision 2.3 beantragt; kein solcher Fix ist implementiert.
Keine automatische HTTPD-/Heapbudgeterhoehung. Q1/Q2, F4 PROVISIONAL und alle
historischen Grenzen bleiben bestehen; `RAM_PASS=NOT_PROVEN`,
`ACTUATOR_RELEASE=NO`, `D9=NOT_RUN`.

Builder-Self-Check auf `e935597`: clang-format PASS, clang-tidy fand zwei fehlende
Klammerpaare in `webSettings()`. Rein mechanisch korrigiert; der erneute Lauf
wird im Handover auf den neuen HEAD gebunden. Kein Ressourcenfix damit und
keine Aenderung an Q1/Q2 oder an den Fehler-/Commitvertraegen.
