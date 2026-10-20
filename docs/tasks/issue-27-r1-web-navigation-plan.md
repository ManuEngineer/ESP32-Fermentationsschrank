# Issue #27 – R1-Web: Navigation Übersicht / Meldungen

**2026-10-20 · PLAN-REVISION 1 · NICHT FREIGEGEBEN · UMSETZUNG GESPERRT (Ownerentscheid D1 offen).**
[Issue #27](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/issues/27)
bleibt OPEN. Branch `agent/issue-27-r1-web-navigation-plan`, Draft-PR zu Issue #27.
Dieser Plan ist die einzige kanonische Fassung. Eine Umsetzung beginnt erst nach
unabhaengigem Planreview und ausdruecklicher Freigabe der exakten Plan-SHA.

**Kernbefund.** Navigation und Meldungsseite brauchen **+601 B** Shell (gemessen); die
Shell hat **16 B** Spielraum ueber der 128-B-Normalreserve (11 312 / 11 456 B, Kopf
1 534 / 1 536 B). Eine weitere Limiterhoehung ist ausgeschlossen. Im Shell-Anhang ist der
Slice nur mit einer Kette von Zusatzmassnahmen darstellbar, die **1 B** Spielraum
uebrig laesst (Option B3, §5): Build-Werkzeug `terser` plus Verlagerung der bisherigen
Quittieren-/Stumm-Knoepfe. Das ist weder robust noch erweiterbar – und alle weiteren
R1-Bereiche (Programme, manueller Betrieb, Einstellungen, Diagnose, Service, System) haben
im Shell-Anhang keinen Platz mehr. Der Plan empfiehlt deshalb einen **kleinen statischen
Script-Asset** mit den vorhandenen ESP-IDF-/HTTPD-Mitteln (Option A, §6): die Shell wird
auf rund 1,6 KB verkleinert, der Dashboard-Code liegt unveraendert in einem
byte-konstanten Flash-Asset. Das ist ein neuer Route-/Cache-/Fehlervertrag und damit eine
**Ownerentscheidung** (§9); der Planer fuehrt ihn nicht eigenmaechtig ein.

## 1. Live-Basis

- `origin/main` = `086873ac3a6ea0d8134810a744e61862e0528a6a` (Merge PR #208,
  2026-10-20T15:13:41Z); Branch von dort abgezweigt. Erste Branch-Aenderung `ac037e8`:
  Post-Merge-Status von #208 in der [Roadmap](../ROADMAP.md).
- Shell heute: authentifiziert **11 312 B** (Kopf mit Sequenz „1“ 1 515 B + Suffix
  9 797 B), Limit **11 456 B**, 128 B Normalreserve (enthaelt 19 B Sequenzaufschlag),
  **16 B** Spielraum; Kopf mit maximaler Sequenz 1 534 B (Testlimit 1 536 B).
- Unveraendert bindend: Auth/Session/CSRF/Mutationssequenzen, 5-s-Polling mit
  Single-Flight, Stale-/Offline-Gates (`P`/`LP`, 15-s-Frische, 30-s-Frist),
  `localStorage.lang`/Sprachwahl, Safety-Sperren; `ACTUATOR_RELEASE=NO`,
  `RAM_PASS=NOT_PROVEN`, F4 PROVISIONAL, G1′ storniert. Keine Hardware-, Flash-,
  Browser-, UART-, Reset- oder Aktorfreigabe durch diese Planung.

Fachquellen: [WEB_UI](../WEB_UI.md) (Navigation, Responsive Darstellung, Startseite,
Shell-Transport RES-01), [LOCAL_RUNTIME_UI](../LOCAL_RUNTIME_UI.md) (Meldungsliste,
Quittieren/Stummschalten), [Sprachplan #208](issue-27-r1-web-language-plan.md) und
[#207](issue-27-r1-web-standby-info-plan.md) samt Evidence,
[ADR-013](../ADR-013_REUSABLE_DEVICE_PLATFORM.md), [CI/Gates](../CI_AND_QUALITY_GATES.md),
[Workflow](../AGENT_WORKFLOW.md), [Engineering](../ENGINEERING_PRINCIPLES.md).

## 2. Ziel und Nicht-Ziele

**Ziel:** Eine responsive, klar beschriftete Navigation zwischen **Übersicht** und
**Meldungen** in der angemeldeten Web-Shell. Übersicht bleibt die bestehende Startseite
(Zustand, Temperaturen, Lauf-/Standbyinformationen, Aktionsleiste). Meldungen zeigt alle
von `/api/v1/alerts` gelieferten Meldungen (höchstens 16) mit ihrem Zustand und je
Meldung den Aktionen **Quittieren** und **Stummschalten** ueber den vorhandenen
Mutationspfad. Die Auswahl ist per `#alerts` im URL-Fragment wiederherstellbar.

**Nicht-Ziele:** Programme, manueller Betrieb, Einstellungen, Diagnose, Service, System
und Protokolle/Historie bleiben **offen** und werden weder umgesetzt noch als Platzhalter
oder deaktivierte Menuepunkte vorgetaeuscht (die Navigation zeigt genau die zwei
umgesetzten Bereiche). Keine neuen Datenfelder, Meldungstitel, Zeitpunkte, Details oder
Sortierung im Browser (die API liefert keine; die Reihenfolge der Application bleibt
massgeblich), keine neue Fachlogik, kein neuer Mutationstyp, keine Aenderung an Auth,
Session, CSRF, Polling, Revisionsschutz, Safety-Sperren oder `localStorage.lang`, keine
Aenderung der Setup-Shell, keine SPA-/Framework-/Dateisystem-Migration, keine
Limiterhoehung der bestehenden Shell.

## 3. Ist-Analyse (Wiederverwendung)

Pfade in `lib/fermentation_app/src/`, `lib/device_platform*/src/`.

| Baustein | Ist-Zustand | Folge |
|---|---|---|
| Daten | `/api/v1/alerts` liefert je Meldung `id`, `code`, `severity`, `active`, `acknowledged`, `resolved`, `decisionRequired`, `muted`, `revision` (max. 16, 3 072-B-Body); wird bereits im 5-s-Takt der drei Abrufe geholt (`LP[2]`) | **kein** API-/Codec-Ausbau, kein vierter Request |
| Aktionen | `send({t:'ack-message'|'mute-message',id})` ueber `/internal/ui/run`; heute nur fuer die erste passende Meldung in der Aktionsleiste (`al()`, `ops.onclick`); Revisionen `r.messages` aus dem letzten Status, Replay-/Sequenz-/CSRF-Pfad | pro Zeile derselbe `send()`; keine neue Route |
| Gates | `P` (Aktionsgrundlage, `null` bei Fehler/Stale/> 15 s), `B` (Mutation laeuft); Aktionsleiste deaktiviert bei `B\|\|!P` | Zeilenknoepfe nutzen dieselben `P`/`B` |
| Darstellung | `dr()` rendert `#alerts` (Zaehler + aktive Meldungen), `#cols` zwei Spalten ab 40 rem; Verbindungs-/Alterstext, Daempfung `bd.className='x'` | Navigation schaltet Sichten, keine zweite Datenhaltung |
| Texte | `L`-Tabelle (DE/EN/ES), `sl()` rendert um; `alerts`, `ack`, `mute`, `qd`, `sm` existieren | nur `ov` (Uebersicht) und `rs` (erledigt) kommen neu hinzu |
| Auslieferung | Kopf (Heap-String, `reserve(1536)`) + `staticBodySuffix` (Flash, Chunk-Send ohne Heap-Kopie); **ein** Wildcard-`httpd_uri_t` (`max_uri_handlers = 1`, `httpd_uri_match_wildcard`); Routing in `WebRouteDispatcher::handle` ueber `request.path`; Antwort-Metadaten nur `Set-Cookie`/`Retry-After` | ein neuer Pfad braucht **keine** httpd-Konfigurationsaenderung; ein Cache-Header braucht eine kleine Metadatenerweiterung |
| Pfad/Query | `request->uri` wird unveraendert als `request.path` weitergegeben (Query bleibt im Pfad) | Version im **Pfad**, nicht als Query |
| Werkzeuge | `terser` ist lokal installiert (`/usr/bin/terser`), aber nicht Teil der Repository-Toolchain | jede Nutzung waere eine neue Build-/Toolchain-Entscheidung |

## 4. Gemessener Kapazitaetsvergleich (MEASURED)

Verfahren, Kandidaten und Basisabgleich (Suffix 9 797 B, Shell 11 312 B) dauerhaft in
[`audits/WEB_NAV_BUDGET_20261020/`](../audits/WEB_NAV_BUDGET_20261020/) (`measure.py`,
`candidates/`). Der Kandidat B1 (Navigation + Meldungsseite) laeuft im extrahierten
Produktsuffix mit den bestehenden **74** JS-Faellen PASS (nach Erweiterung des Fake-DOM um
`append`/`after`) und in einem Ad-hoc-Test (Wechsel der Sicht, je Meldung zwei Knoepfe,
Klick sendet genau `ack-message` fuer die Meldung); Fake-DOM, kein Browser.

| Option | Suffix gegenueber 9 797 B | Kopf | Shell gesamt | Spielraum ueber Normalreserve (Soll ≥ 0) |
|---|---:|---:|---:|---:|
| Heute | – | – | 11 312 B | **+16 B** |
| **B1** Navigation + Meldungen im Shell-Anhang | **+601 B** | ±0 | 11 913 B | **−585 B** |
| B2 = B1 + `terser` (Compress/Mangle, Konstanten `S,A,U,Q` reserviert) | +357 B | ±0 | 11 669 B | −341 B |
| **B3** = B2 + Quittieren/Stumm aus der Aktionsleiste in die Meldungsseite verlagert | +78 B | −63 B | 11 327 B | **+1 B** |
| **A** statischer Asset (Option §6) | Asset **10 374 B** (Dashboard-Code unveraendert + Navigation/Meldungen) | – | Shell **1 583 B** (Kopf 1 515 + Anhang 68) | Asset-Limit offen (§9) |

Lesehilfe: B3 erreicht den Slice nur durch (a) ein neues Build-Werkzeug fuer den Shell-
Anhang (reproduzierbare Minifizierung mit Review-Pflicht fuer generierten Code), (b) den
Wegfall der Quittieren-/Stumm-Knoepfe auf der Startseite (funktionale Verlagerung, vom
Auftrag nicht vorgesehen) und (c) **1 B** Reserve – jeder Nachbesserungsbedarf (Wortlaut,
`decisionRequired`-Anzeige, Barrierefreiheit) fuehrt zu HOLD. B1/B2 passen nicht.

## 5. Bewertete Alternativen

1. **B1/B2/B3 (im Shell-Anhang).** B1/B2 passen nicht, B3 hat 1 B Reserve und verbraucht
   die letzte Erweiterungsmoeglichkeit der Shell; die uebrigen sechs R1-Bereiche koennten
   danach nicht mehr in der Shell liegen. Nicht empfohlen.
2. **Lazy-Feature-Asset (nur Meldungsseite nachladen).** Braucht trotzdem den neuen
   Asset-Vertrag (§6), der Shell-Anhang muesste fuer die Navigation dennoch ≥ 200 B
   freigeben (Verlagerung der Aktionsleisten-Knoepfe), und Quittieren waere vom Asset
   abhaengig. Mehr Vertrag als A bei weniger Ertrag. Nicht empfohlen.
3. **Serverseitig gewaehlte Einzelsprachen-Suffixe (Weg E aus dem Sprachplan).** Spart rund
   1,9 KB je Antwort, braucht aber Sprach-Cookie, drei Suffix-Varianten und drei Goldens;
   fuer 600 B Navigation uebergross. Nicht empfohlen.
4. **SPA/Framework/Dateisystem/gzip.** Ausdruecklich ausgeschlossen (keine vorsorgliche
   Migration).
5. **Vorhandene ESP-IDF-Mittel fuer A.** `httpd_resp_send_chunk` (bereits fuer den Shell-
   Anhang im Einsatz), Wildcard-Handler und `WebRouteDispatcher` genuegen; kein
   `esp_http_server`-Konfigurations- oder Partitionsumbau, kein Dateisystem, kein
   `httpd_resp_send_file`. Eigenbau beschraenkt sich auf eine Route, ein CRC-Token und ein
   Metadatenfeld.

## 6. Empfohlene Option A – statischer Script-Asset

**Prinzip.** Der Dashboard-Script-Anhang (heute `kWebShellSuffix`) wird als reines
JavaScript in einen **byte-konstanten Flash-Asset** verschoben. Die Shell behaelt Kopf und
den kleinen Inline-Teil (`const S=…;const A=…;const U=…;let Q=…` mit CSRF-Token,
Authentifizierungszustand und Mutationssequenz – die einzigen sitzungsbezogenen Daten) und
referenziert den Asset. Die Setup-Shell bleibt unveraendert inline.

**Route-Vertrag.**

- Pfad `GET /assets/app.<crc8>.js`, `<crc8>` = acht Hex-Ziffern des CRC-32 der Asset-Bytes
  (vorhandenes `crc32`, einmal je Prozess berechnet). Genau dieser Pfad liefert den Asset;
  jeder andere `/assets/…`-Pfad faellt durch zur Adapter-404 (kein Verzeichnis, keine
  Wildcard-Auslieferung). Die Version steht im Pfad, weil der Adapter die Query im Pfad
  belaesst.
- Methoden: nur `GET` (andere → 405 JSON wie die Shell). Gleiche
  Metadatenpruefung wie die Shell (`validWebMetadata`: Host, Fetch-Site/Origin); kein
  Cookie-Zugriff, kein `Set-Cookie`.
- Auslieferung: `Content-Type: text/javascript; charset=utf-8`, Koerper leer,
  `staticBodySuffix` = Asset (derselbe Chunk-Pfad wie der Shell-Anhang, keine Heap-Kopie).
- **Oeffentlich ohne Sitzung** (Anmeldeseite braucht das Script vor der Anmeldung). Der
  Asset enthaelt **keine** Sitzungs-, CSRF-, Sequenz- oder Konfigurationsdaten; ein Test
  belegt byte-identische Antworten in allen Authentifizierungszustaenden.
- Waehrend des Netzwerk-Setups gehoert `/` der Setup-Seite; `/assets/…` faellt dort durch
  (Setup-Seite braucht ihn nicht).

**Cache-Vertrag.** Neues optionales Metadatenfeld (geschlossene Aufzaehlung statt freier
Zeichenkette, kein Header-Injection-Risiko): `HttpResponseMetadata::cache =
ImmutableAsset` → `Cache-Control: private, max-age=31536000, immutable`; Standard bleibt
ohne Header (unveraendertes Verhalten aller anderen Antworten). Nur die exakte aktuelle
Versions-URL erhaelt den Header; ein veralteter Tab bekommt fuer die alte URL `404` (kein
Vermischen alter und neuer Bytes). `Cache-Control` erscheint nie bei Shell, API oder
Fehlern. Aenderungen: `device_platform` (Typ, Validierung), `device_platform_esp_idf`
(Header setzen), Host-Tests der Adapter-/Metadatenpruefung.

**F1-/Fehlervertrag.** Wie der Shell-Anhang: Sendefehler → keine weiteren Chunks, kein
Terminator, Adapter gibt den Fehler zurueck. Jede Antwort-Ersetzung (zu grosser Asset,
Pruefsummenfehler) loescht den Anhang und liefert vollstaendig `503`,
`application/json; charset=utf-8`, `{"error":"web-asset-unavailable"}` ohne
JS-Bytes. Faellt der Asset aus, zeigt die Shell einen **sichtbaren** Hinweis
(`<script … onerror=…>` setzt einen sprachneutralen englischen Text im vorhandenen
`#auth-state`: „UI not loaded – reload“), statt still leer zu bleiben; die Seite loest
keine Aktion aus.

**Limits (Ownerentscheidung, §9).** Shell: neues, **kleineres** Limit (Vorschlag **2 048 B**;
Kandidat 1 583 B, mit max. Sequenz 1 602 B, Kopf ≤ 1 536 B). Asset: eigenes Limit
(Vorschlag **12 288 B**; Kandidat 10 374 B, Spielraum 1 914 B fuer Wortlaut, Barriere-
freiheit und `decisionRequired`-Anzeige). Die bisherige Kombi-Grenze 11 456 B entfaellt;
die Summe bei Erstaufruf liegt bei 11 957 B (+645 B gegenueber heute), bei gecachtem Asset
bei rund 1,6 KB.

**Ressourcenfolgen (ohne Hardware; Zahlen ESTIMATED bis zur Umsetzung).**

| Posten | Folge |
|---|---|
| Flash | `.flash.rodata` +601 B Navigation/Meldungen +  kleine Zusatzstrings; `.flash.text` rund +0,4…0,8 KB (Route, CRC-Token, Metadatenfeld); der Asset selbst ersetzt den bisherigen Suffix 1:1 |
| Dauer-RAM / Stack | `.dram0.*`/`.iram0.*` ±0 erwartet; kein neuer Dauerzustand; `handleAsset` hat keinen Antwort-String und damit keinen groesseren Frame als `handleShell` (352 B); die tiefste HTTPD-Kette (Status-Handler) bleibt unveraendert |
| Heap pro Aufruf | Asset-Antwort: **0 B** Koerper (nur Flash-Chunks, wie der heutige Suffix); Shell-Kopf-String bleibt unter `reserve(1536)`; CRC einmalig, kein Heap |
| Netz / Last | Erstaufruf zwei Requests (Shell 1,6 KB + Asset 10,4 KB) statt einem 11,3-KB-Request; danach nur die Shell (Asset gecached). Der einzige httpd-Task bedient sie nacheinander; Socket-/F4-Verhalten bei der Seitenerstellung ist **offline nicht beweisbar** und wird als `NOT_PROVEN` ausgewiesen |

## 7. Navigation und Meldungsseite (Funktion, gilt fuer A)

- **Navigation.** Eine `<nav>` mit zwei beschrifteten Knoepfen „Übersicht“ und
  „Meldungen (n)“ (n = aktive Meldungen), sichtbar nur mit gueltigem `LP` (nicht auf der
  Anmeldeseite, nicht vor dem ersten Status); `aria-current` auf der aktiven Sicht;
  Touchflaechen >= 2,75 rem wie die bestehenden Knoepfe; auf Mobil einspaltig und
  umbrechend, ab 40 rem dieselbe Leiste (ein Seitenmenue ist laut WEB_UI optional und
  bleibt einem spaeteren Slice mit mehr Bereichen vorbehalten).
- **Sicht und Adresse.** Sicht in einer Variablen, gespiegelt im Fragment: `#alerts` →
  Meldungen, sonst Übersicht; jedes andere Fragment wird ignoriert. Wechsel loest keinen
  Request aus und rendert nur um (`dr()`); Zurueck-Taste/Reload stellen die Sicht her.
- **Übersicht.** Wie heute (inklusive Aktionsleiste), plus nur die hoechste aktive
  Meldung und der Zaehler; die Aktionsleiste (Quittieren/Stumm fuer die erste passende
  Meldung) bleibt unveraendert.
- **Meldungen.** Alle gelieferten Meldungen in API-Reihenfolge (nicht umsortiert), je Zeile
  Code (wie heute, unlokalisiert), Schweregrad-Klasse und Zustand (aktiv/quittiert/stumm/
  erledigt) sowie die Knoepfe Quittieren und Stummschalten. Ein Knopf ist deaktiviert bei
  `B`, `!P`, erledigter Meldung oder bereits erreichtem Zustand und ruft nur dann
  `send({t:'ack-message'|'mute-message',id})` auf, wenn `P && !B`. Der Anwendungspfad
  entscheidet weiter allein (Revisionen, Replay, `applied`/`stale`/`rejected`).
- **Stale/Offline.** Gedaempfte Anzeige mit Abrufalter unveraendert; alle Zeilenknoepfe
  deaktiviert; keine neue Aktionsfreigabe aus Anzeigedaten.
- **Sprache.** Neue Labels `ov`, `rs` in DE/EN/ES in der vorhandenen `L`-Tabelle; `sl()`
  rendert Navigation und Meldungsseite beim Sprachwechsel um (`dr()`).

## 8. Umsetzungsschnitte nach Freigabe der exakten Plan-SHA und D1 (Option A)

0. **C0 – Plattform: Cache-Metadatum.** `HttpResponseMetadata::cache` (Aufzaehlung,
   Standard keine), Validierung, Adapter setzt `Cache-Control` genau fuer
   `ImmutableAsset`; Tests in `test_http_server_lifecycle` und
   `esp_idf_http_server_adapter_host`. Keine Aenderung an bestehenden Antworten.
1. **C1 – Asset-Route und reine Verschiebung.** `WebRouteDispatcher` liefert den Asset
   (Pfad mit CRC-Token, GET-only, Metadatenpruefung, Fehlervertrag); `kWebShellSuffix`
   wird unveraendert zum Asset-Literal, die Shell referenziert ihn; Limits
   `kMaximumWebShellBytes`/`kMaximumWebAssetBytes`; JS-Test und Goldens auf den Asset
   umgestellt. **Kein Verhaltensunterschied:** alle 74 JS-Faelle und alle nativen
   Web-Tests bleiben ohne inhaltliche Aenderung gruen.
2. **C2 – Navigation und Meldungsseite** im Asset (Kandidat B1 als Messbasis), Labels,
   Fragment, Zeilenknoepfe, `sl()`-Anbindung.
3. **C3 – Tests, Doku, Evidence.** WEB_UI, ACCEPTANCE_TESTS, ROADMAP, Offline-
   Ressourcenevidence; Builder-Self-Check; danach Independent Review.

## 9. Ownerentscheidungen (offen)

| ID | Frage | Empfehlung |
|---|---|---|
| **D1** | Wie wird die Kapazitaet geschaffen? **(A)** statischer Asset gemaess §6 (neuer Route-/Cache-/Fehlervertrag) **(B3)** im Shell-Anhang mit `terser` und Verlagerung der Aktionsleisten-Knoepfe (1 B Reserve) **(C)** zurueckstellen, Slice nicht umsetzen | **(A)** |
| D2 | Limits bei (A): Shell **2 048 B**, Asset **12 288 B** (Kandidaten 1 583 B / 10 374 B); die bisherige Kombi-Grenze 11 456 B entfaellt | wie vorgeschlagen |
| D3 | Asset oeffentlich ohne Sitzung (noetig fuer die Anmeldeseite), garantiert frei von Sitzungs-/Konfigurationsdaten | ja |
| D4 | Cache: unveraenderlich ueber CRC-Pfad (mit Metadatenerweiterung) **oder** ohne Cache-Header (kein Adaptereingriff, jeder Aufruf holt 10,4 KB neu) | unveraenderlich cachen |
| D5 | Hardware: nach der Umsetzung eine kleine Messung auf dem Geraet (ein Client, kalt/gecacht, Seitenaufbau mit zwei Requests, Heap/HWM) vor Ready, oder offline `NOT_PROVEN` mit FOLLOW-UP | kleine Messung auf Ownerwunsch; nicht zwingend |

Bei (B3) entfallen C0/C1; die Umsetzung braeuchte stattdessen eine Entscheidung zu
`terser` als Build-Abhaengigkeit und zur Verlagerung der Knoepfe und liefe mit 1 B Reserve.
Bei (C) bleibt `ACTUATOR_RELEASE=NO` und der Stand unveraendert.

## 10. Gezielte Nachweise

| Bereich | Faelle |
|---|---|
| Echter extrahierter JS-Test (`scripts/test_web_shell_polling.mjs`, Fake-DOM um `append`/`after`/Fragment erweitert, Asset-Literal als Quelle) | alle 74 bisherigen Faelle erhalten; Navigation sichtbar nur mit `LP`; Wechsel Übersicht ↔ Meldungen ohne Request; Fragment `#alerts` stellt die Sicht her, beliebige andere Fragmente (`#x`, `#constructor`, leer) werden ignoriert; DE/EN/ES vollstaendige Labels (`ov`, `rs`, Zaehler) und Umrendern per `sl()`; Meldungsseite zeigt alle Meldungen in API-Reihenfolge, Zustandsmarker, 16 Meldungen; je Zeile genau ein `ack-message`/`mute-message` fuer die richtige `id`; Knoepfe deaktiviert bei `B`, `!P`, Stale (> 15 s), Offline/Fehler, erledigt und bei erreichtem Zustand; `P` und alle Aktionsleisten-Sperren in allen `homeMode`-Werten unveraendert; stale/abgelehnte Antworten (`stale`, `rejected`, `busy`) behalten die Sicht; Mutationsgegenproben (Knopf ohne `P`-Pruefung, falsche `id`) schlagen fehl |
| Native `test_web_application_routes` | Asset-Route: GET-only (405 sonst), Host-/Fetch-Metadatenpruefung, genau ein gueltiger Pfad (falsches/altes Token, `/assets/`, Querypfade → 404), Asset-Bytes byte-identisch in allen Authentifizierungszustaenden und ohne Cookie/`Set-Cookie`, Content-Type, `Cache-Control` nur beim exakten Pfad, Fehlerpfad (503 JSON ohne Anhang), Shell referenziert exakt den aktuellen Pfad und bleibt <= Shell-Limit (Kopf <= 1 536 B mit max. Sequenz), Asset <= Asset-Limit, eingefrorener Generator fuer Shell und Asset byteweise, Setup-Shell und `localStorage.lang`-Vertrag unveraendert, Netzwerk-Setup-Fluss unveraendert |
| Plattform / HTTP-Adapter | `test_http_server_lifecycle`: Validierung des Cache-Metadatums; `esp_idf_http_server_adapter_host` (nach seinem README): Header nur beim Asset, Chunk-Reihenfolge, Fehler in Kopf/Anhang/Terminator, suffixlose Antworten unveraendert |
| Regression | `test_fermentation_ui_models`, `test_renderer_boundary`, `test_ui_steady_state_allocations`, `test_run_detail_projection` (Status-Pfad unveraendert), `check_architecture_boundaries.py` |
| Ressourcen offline | Sektionsvergleich (`.dram0.*`, `.iram0.*`, `.flash.*`) gegen `main`, `-fstack-usage` (`handleAsset`, `handleShell`, `WebReadOnlyApiHandler::handle`), Shell-/Asset-Bytes, beide ESP-Profile bauen; Ergebnis hoechstens `RAM_OFFLINE_ESTIMATED` |
| Mobil/Desktop | Layout nur strukturell belegt (Viewport-/Media-Query-Strings, Touchflaechen-Regel, einspaltig, ab 40 rem unveraendert); **echte Browser-/Geraeteabnahme bleibt `NOT_PROVEN`** |

Geplante Befehle (nicht jetzt):

```bash
node scripts/test_web_shell_polling.mjs
pio test -e native -f test_web_application_routes -f test_http_server_lifecycle -f test_fermentation_ui_models -f test_renderer_boundary -f test_ui_steady_state_allocations -f test_run_detail_projection
python3 scripts/build_esp_idf_profiles.py all --require-clean-source-tree
export PRE_READY_EXPECTED_HEAD="$(git rev-parse HEAD)"
bash scripts/run_pre_ready_gates.sh self-check
```

Pre-Ready nur nach Independent Review, `OPEN_BLOCKERS=0` und ausdruecklicher
Owneranweisung. Kein Flash, kein Browser-/UART-/Resettest in diesem Plan.

## 11. Risiken

- Zwei Requests beim Erstaufruf erhoehen kurzzeitig HTTPD-/WLAN-Last; wirkt nur auf dem
  Geraet messbar (D5), offline `NOT_PROVEN`.
- Veralteter Tab nach Firmwareupdate: alte Asset-URL → 404; sichtbarer Hinweis, ein
  Neuladen genuegt; kein Vermischen alter und neuer Bytes.
- Globale `const`/`let` der Inline-Skripte (`S`, `A`, `U`, `Q`) sind fuer das nachfolgende
  klassische Script sichtbar (gemeinsame globale lexikalische Umgebung); wird im JS-Test
  mit zwei getrennten `vm`-Skripten belegt.
- Der Asset ist oeffentlich lesbar (Code, keine Geheimnisse); Test erzwingt Freiheit von
  Sitzungsdaten.
- Weitere R1-Bereiche und die offene R1-U-/D9-/G2-Abnahme bleiben unveraendert offen.
