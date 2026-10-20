# Issue #27 – R1-Web-Restumfang (integrierter Gesamtplan, ein Draft-PR)

**2026-10-20 · PLAN-REVISION 2 · OWNERENTSCHEIDE D1–D9 BESTAETIGT · REVISION NICHT FREIGEGEBEN · UMSETZUNG GESPERRT.**
[Issue #27](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/issues/27)
bleibt OPEN. Branch `agent/issue-27-r1-web-navigation-plan` (Name historisch), Draft-PR
[#209](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/pull/209). Dieser Plan ist
die einzige kanonische, vollstaendige Fassung. Historisch und nicht mehr massgeblich:
Revision 1 (Plan-SHA `4927e856df8cb3474a7b0ed74aa92917dd23cc6b`) und der fruehere reine
Navigationsplan (`0614c054e3303ef2aacd495b8221e512eb4c0714`); deren Messbelege bleiben
unter `audits/WEB_NAV_BUDGET_20261020/`. Die Ownerentscheide unten sind **keine** Freigabe
dieser Revision: Umgesetzt wird erst nach ausdruecklicher Freigabe der exakten Plan-SHA.

## 0. Bestaetigte Ownerentscheide (Revision 2)

| ID | Entscheid |
|---|---|
| **D1 = A** | Oeffentlicher, versionierter statischer JS-Asset fuer den weiteren R1-Webumfang; keine weitere Erhoehung des bisherigen Shell-Limits (11 456 B entfaellt). |
| **D2** | Getrennte feste Obergrenzen: normale Asset-Shell **2 048 B**, Asset **16 384 B**, Setup-Shell **eigene** Grenze (Kandidat **4 096 B**, Reservenachweis §4.2). Normalreserve und Fehlervertraege je Shell erhalten. Die 25-%-Abweichung vom geschaetzten Slice-Aufwand ist ein **Analyse-/Warnsignal, kein automatischer HOLD**; absolute Ressourcen- und Vertragsgrenzen bleiben harte Gates. |
| **D3** | Der Asset ist ohne Session oeffentlich abrufbar und enthaelt keine Session-, CSRF-, Mutationssequenz-, Credential- oder Konfigurationsdaten. |
| **D4** | Versionspfad mit CRC-32 und `Cache-Control: private, max-age=31536000, immutable` ausschliesslich fuer den exakten Asset; alte/falsche Pfade fail-closed. |
| **D5 = a** | Web-Einstellungen fuer **Geraetename und Zeitzone**, nur ueber zustaendige Application-/Konfigurations-Owner und revisionsgeschuetzte Mutationsvertraege; Netzwechsel-/Aussperrrisiken werden behandelt (§6.3). |
| **D6 = a** | Kein Benutzerprogramm-Editor in diesem PR; separater spezifizierter Folgescope. |
| **D7 = a** | Keine Web-Service-PIN-Route/-Seite ohne tatsaechlichen Funktionsverbraucher; **C4 entfaellt**, es wird keine Scheinseite angelegt. |
| **D8** | Vollstaendiger Werksreset ausschliesslich lokal; die widersprechende Web-Beispielaussage in `docs/WEB_UI.md` wird mit dem Safety-Vertrag abgeglichen (C5). |
| **D9** | Gezielte aktorfreie Browser-/Geraetemessung vor Ready fuer den geaenderten Asset-/HTTPD-Pfad – **erst nach gesonderter ausdruecklicher Hardware-/Flash-/Browserfreigabe** (§9). Bis dahin `NOT_PROVEN`, kein implizites Gate-PASS. |

**Kernaussagen.** (1) Ein PR (C0, C1, C2, C3, C5). (2) Abgetrennt bleiben Manueller Betrieb,
Diagnose/Diagramme/Exporte (#28), Journale/Protokolle/Backup/Werksreset (#19),
Service-Funktionen und -Route, Programmeditor, Favicon (#204). (3) Zwei fachliche Fragen sind
vor dem betroffenen Teil von C3 zu beantworten und blockieren **nur diesen Teil** (§6.3).
(4) Keine Hardware-, Flash-, Browser-, UART-, Reset- oder Aktorfreigabe;
`ACTUATOR_RELEASE=NO`, `RAM_PASS=NOT_PROVEN`.

## 1. Live-Basis

- `origin/main` = `086873ac3a6ea0d8134810a744e61862e0528a6a` (Merge PR #208,
  2026-10-20T15:13:41Z). Erste Branch-Aenderung `ac037e8`: #208 als `MERGED` in der
  [Roadmap](../ROADMAP.md).
- Gemergter Web-Stand: #170 Foundation (Auth/Session/CSRF/Replay, Read-only-API,
  Setup-Pfad), #201 Run-Mutationsroute, #203 Dashboard/F1/F4/Single-Flight, #205
  Mutationsheader, #206 Laufinformationen, #207 Standbyinformationen, #208 Sprachwahl.
- Shell heute: authentifiziert **11 312 B** (Kopf 1 515 B + Suffix 9 797 B) bei Limit
  11 456 B = 128 B Normalreserve + 16 B; Kopf mit maximaler Sequenz 1 534 B (Testlimit 1 536 B).
  **Setup-Shell 3 916 B** (Kopf 1 029 B + Suffix 2 887 B), heute durch dasselbe Limit
  11 456 B abgedeckt.
- Unveraenderlich bindend: Session/CSRF/Mutationssequenz/Replay, 5-s-Polling mit
  Single-Flight und 30-s-Frist, 15-s-Frische, Stale-/Offline-/Bestaetigungsgates, DE/EN/ES,
  `localStorage.lang`, Displayunabhaengigkeit, Application als einziger Fach-Owner.
  Offene Gates: F4 PROVISIONAL, G1′ storniert, G2_WEB/R1-U/D9 offen.

Quellen: [WEB_UI](../WEB_UI.md), [SPECIFICATION_REVIEW](../SPECIFICATION_REVIEW.md),
[LOCAL_UI_SETTINGS_SERVICE](../LOCAL_UI_SETTINGS_SERVICE.md),
[LOCAL_RUNTIME_UI](../LOCAL_RUNTIME_UI.md), [RUN_COMMANDS](../RUN_COMMANDS.md),
[PROGRAMS](../PROGRAMS.md), [SETTINGS_AND_STORAGE](../SETTINGS_AND_STORAGE.md),
[NETWORK](../NETWORK.md), [ADR-013](../ADR-013_REUSABLE_DEVICE_PLATFORM.md),
[CI/Gates](../CI_AND_QUALITY_GATES.md), [Workflow](../AGENT_WORKFLOW.md),
[Engineering](../ENGINEERING_PRINCIPLES.md), Evidence #203/#206/#207/#208.

## 2. Wiederverwendbarer Bestand (belegt)

| Baustein | Beleg |
|---|---|
| Read-only-API | `GET /api/v1/status` (Revisionen, Lauf-/Standby-/Netzwerk-/Zeitinfo), `/temperatures`, `/alerts` (id, code, severity, active, acknowledged, resolved, decisionRequired, muted, revision; ≤ 16), `/programs` (nur `id`/`name`, `catalogRevision`) |
| Mutationsroute | `POST /internal/ui/run`, geschlossene Intents im Codec: `start-program` (`c` mit `p,x,d,h,s,c,k,l`), `stop-run`, `complete-run`, `ack-message`, `mute-message`, `adjust-run`, `recovery-time-correction`, `reset-fault`, `sensor-selection`, `start-manual-timed`, `start-manual-holding`; Revisionsschluessel `s,r,m,f,e,u,c`; Application entscheidet (Outcome-Matrix `applied/stale/busy/confirmation-required/rejected/too-large/write-failed`) |
| Programmpraesentation | `FermentationUiProgramSummaryView` und `makeProgramSummary(...)` in `fermentation_touch_workspace.*` (kanonische Zusammenfassung mit angewendeten Overrides ueber `applyStartCandidateOverrides`; fehlende Werte bleiben `nullopt`, nie 0); `makeProgramSummary` ist heute **dateilokal** |
| Konfigurationsweg | `FermentationApplication::applyUserSettings(change, expectedRevision)` (nur `deviceName`; lehnt bei aktivem Lauf ab; Rueckgabe `ApplicationConfigurationChangeResult{Preview, Commit}`), `commitUserConfigurationChange(...)` |
| Zeitzonen | `kSupportedTimeZones` (device_platform) enthaelt **genau einen** Eintrag, `Europe/Zurich`; `ITimeZoneResolver::prepare` validiert; `SETTINGS_AND_STORAGE`: Aenderung nicht neustartpflichtig und veraendert keinen Lauf; **kein** Application-Befehl zum Aendern der Zone |
| Sitzung | `WebSessionManager` mit Service-Lease; keine HTTP-Route (bleibt so, D7) |
| Auslieferung | Kopf (Heap-String) + `staticBodySuffix` (Flash, Chunk-Send ohne Heap-Kopie); ein Wildcard-`httpd_uri_t`, Routing in `WebRouteDispatcher::handle`; Antwort-Metadaten nur `Set-Cookie`/`Retry-After`; `request.path` enthaelt die Query |

## 3. Restumfang-Statusmatrix

Legende: **U** umgesetzt · **T** teilweise · **F** fehlt (in diesem PR) · **X** anderes Issue ·
**H** hardware-/ownerabhaengig oder nicht spezifiziert, nicht planbar.

| Bereich (WEB_UI) | Status | Beleg / Abgrenzung | Plan |
|---|---|---|---|
| Responsive Basis | **U** | #203, Shell-CSS | unveraendert |
| Anmeldung, Logout, Setup, Sitzungsregeln, kein Dauerlogin | **U** | #170, #203 | unveraendert |
| Sprache je Browser (DE/EN/ES) | **U** | #208 | neue Texte in allen Slices |
| Live-Aktualisierung (5-s-Polling, Stale/Offline) | **U** | #203 | Push/SSE bleibt offen, nicht Teil |
| **Übersicht** – Standby, laufender/abgeschlossener Prozess | **U** | #207, #206, #201 | Navigationseintrag C1 |
| Übersicht – `Details` (Sensorstatus, Vorheizstatus), Aktorstatus | **X** | benoetigt Sensor-/Aktor-API (#28 Diagnose) | nicht hier |
| Übersicht – optional neuer manueller Kuehllauf | **H** | `start-manual-*`/Kuehlplaene liefern 503 ohne Grenzen-Owner (Test O5/#172 S9; #35) | nicht planbar |
| **Programme** – Auswahl + Start (nur ID) | **T** | `pg`-Auswahl, `start-program` | C2 |
| Programme – Details, Anpassung fuer den naechsten Lauf | **F** | Codec kennt `x,d,h,s,c,k,l`, Application wendet an | C2 |
| Programme – Benutzerprogramme anlegen/aendern/loeschen | **H** | D6: separater Folgescope; Touch hat `applyProgramEdit` | nicht hier |
| **Manueller Betrieb** | **H** | siehe oben (#35/Hardware) | bleibt offen |
| **Meldungen** – Liste, Quittieren/Stumm je Meldung | **T** | Aktionsleiste nur erste Meldung | C1 |
| Meldungen – Titel/Zeitpunkt/Detail, **Protokolle** | **X** | API liefert nur Code/Zustand; Journal ist #19 | nicht hier |
| **Einstellungen** – Geraetename | **F** | Touch-Aenderung vorhanden (`applyUserSettings`) | C3 |
| Einstellungen – Zeitzone | **F** | kein Application-Befehl, Katalog hat eine Zone | C3 (Frage Q2) |
| Einstellungen – Netzwerk/Webzugang/Display-Sprache | **H** | lokale Einstellungen, Sitzungsentzug-/Aussperrrisiko | nicht per Web (§6.3) |
| **Diagnose**, Diagramme, Exporte | **X** | #28 | nicht hier |
| **Service** – PIN-Route/Seite | **H** | D7: ohne Verbraucher nicht; Service-Funktionen #28/#19/Hardware | **C4 entfaellt** |
| **System** (Version, Ressourcen, Backup/Import, Werksreset) | **X/H** | #19; Werksreset nur lokal (D8) | nicht hier |
| Temperaturdiagramm des aktuellen Laufs | **X** | #28 | nicht hier |
| Konfliktdarstellung bei konkurrierender Bedienung | **T** | Revisionsschutz in der Application; UI zeigt nur „Nicht ausgefuehrt“ | C5 |
| Favicon `GET /favicon.ico` | **X** | Issue #204 | nicht hier |

Dieser PR schliesst **Issue #27 nicht automatisch**. Der verbleibende Rest (Zeilen H/X) wird in
Roadmap und Issue ausgewiesen; das Issue wird erst nach gesamtem genehmigten Umfang oder
ausdruecklicher Owner-Neuabgrenzung geschlossen.

## 4. Auslieferung: Asset, Shell-Groessen und Fehlervertraege

### 4.1 Gemessener Optionsvergleich (MEASURED)

Verfahren und Kandidaten: [`audits/WEB_NAV_BUDGET_20261020/`](../audits/WEB_NAV_BUDGET_20261020/)
(Basisabgleich Suffix 9 797 B / Shell 11 312 B). Kandidat B1 (Navigation + Meldungen) besteht
die 74 bestehenden JS-Faelle (Fake-DOM) und einen Ad-hoc-Test (Sichtwechsel, Zeilenknoepfe).

| Option | Navigation + Meldungen | Shell | Bewertung |
|---|---:|---:|---|
| Heute | – | 11 312 B, +16 B | – |
| B1 im Shell-Anhang | +601 B | 11 913 B | passt nicht (−585 B) |
| B2 = B1 + `terser` | +357 B | 11 669 B | passt nicht (−341 B) |
| B3 = B2 + Quittieren/Stumm verlagert | +78 B (Kopf −63 B) | 11 327 B | +1 B; kein Platz fuer C2/C3 |
| **A statischer Asset** (D1) | Asset 10 374 B | Shell 1 583 B | traegt alle Slices |

Erwartete Asset-Groesse (**ESTIMATED**, je Slice zu messen): Navigation/Meldungen +0,6 KB
(gemessen), Programme +1,5…2,0 KB, Einstellungen +1,0…1,5 KB, Outcome-Anzeige +0,3 KB ⇒
rund 14…15 KB gegen das Limit 16 384 B. Weicht die Messung eines Slices um mehr als 25 %
vom Schaetzwert ab, wird das **ausgewiesen und im Review analysiert** (Warnsignal, D2); ein
HOLD entsteht erst bei Ueberschreitung der absoluten Grenzen (Asset-Limit, Shell-Limits,
Reserven) oder bei Vertragsverletzung.

### 4.2 Getrennte Groessenpruefungen je Shell (B1-Korrektur)

Die normale Shell und die weiterhin **inline** ausgelieferte Setup-Shell erhalten getrennte
Konstanten und getrennte Pruefungen; das Limit 2 048 B gilt **nicht** fuer den Setup-Pfad.

| Pfad | Grenze | Kandidat / Messung | Reserve | Fehlerfall |
|---|---:|---|---|---|
| Normale Asset-Shell (`kMaximumWebShellBytes`) | **2 048 B** (Kopf + Inline-Teil + Anhang mit Skript-Referenz, **ohne** Suffix) | 1 583 B mit Sequenz „1“ (Kopf 1 515 + Anhang 68), mit Ausfallhinweis rund 1,7 KB (ESTIMATED, in C0 messen); mit 20-stelliger Sequenz +19 B | gemessener Maximalwert + **128 B Normalreserve** (19 B Sequenz eingeschlossen) ≤ 2 048 B | Antwort-Ersetzung: `503`, `application/json; charset=utf-8`, `{"error":"web-shell-unavailable"}`, **kein** HTML/JS-Anhang |
| Setup-Shell (`kMaximumWebSetupShellBytes`) | **4 096 B** (Kopf + Suffix, unveraendert inline) | 3 916 B (Kopf 1 029 + Suffix 2 887) | 4 096 − 3 916 = 180 B ⇒ ≥ 128 B Normalreserve erfuellt, **52 B** Spielraum darueber; jede Setup-Aenderung muss ≤ 3 968 B bleiben | wie oben: `503` `web-shell-unavailable`, Suffix geloescht, keine HTML-/JS-Bytes |
| Asset (`kMaximumWebAssetBytes`) | **16 384 B** | Kandidat 10 374 B (Navigation/Meldungen); Gesamt-PR ESTIMATED 14…15 KB | ≥ 1 024 B bis Limit zum Abschluss (C5 weist den Wert aus) | zu grosser Asset: `503` `{"error":"web-asset-unavailable"}` ohne JS-Bytes; zusaetzlich Compile-Zeit-Pruefung gegen das Limit |

Grenztests (nativ, Testhaken `extraShellBytesForTest_`): fuer **jede** Shell je ein Fall
„genau am Limit → 200“ und „Limit + 1 → 503 mit leerem Suffix, vollstaendigem JSON-Body,
Content-Type und ohne HTML/JS“; Asset-Gegenstuecke ueber einen Test-Asset-Hook; ein Test,
der belegt, dass das Setup-Limit nicht auf die normale Shell und umgekehrt wirkt; ein
Reservenachweis je Shell (Messwert + 128 B ≤ Limit) mit maximaler Sequenz fuer die normale
Shell.

### 4.3 Asset-Vertrag (D1–D4)

- *Route:* `GET /assets/app.<crc8>.js` (acht Hex-Ziffern CRC-32 der Asset-Bytes, einmal je
  Prozess mit dem vorhandenen `crc32`); jeder andere `/assets/…`-Pfad (alte/falsche Version,
  Querystring, Gross-/Kleinschreibung, Verzeichnisse) faellt durch zur Adapter-404
  (fail-closed). Version im Pfad, weil die Query im Pfad verbleibt. Nur `GET` (sonst 405 JSON),
  gleiche Metadatenpruefung wie die Shell (`validWebMetadata`), kein Cookie-Zugriff, kein
  `Set-Cookie`. Waehrend des Netzwerk-Setups gehoert `/` der Setup-Seite, `/assets/…` faellt durch.
- *Auth/Session/CSRF (D3):* oeffentlich (die Anmeldeseite braucht das Script vor der
  Anmeldung), byte-konstant, **frei von** Sitzungs-, CSRF-, Sequenz-, Credential- und
  Konfigurationsdaten; diese bleiben im Inline-Teil der Shell (`const S,A,U,Q`, global fuer das
  nachfolgende klassische Script sichtbar). Test: byte-identische Antwort in allen
  Authentifizierungszustaenden, ohne Cookie; Mutationen laufen unveraendert ueber
  `/internal/ui/run`.
- *Cache (D4):* neues geschlossenes Metadatum `HttpResponseMetadata::cache = ImmutableAsset`
  → `Cache-Control: private, max-age=31536000, immutable` **nur** bei der exakten
  Versions-URL; Standard bleibt ohne Header (alle anderen Antworten unveraendert). Aenderungen:
  `device_platform` (Typ, Validierung), `device_platform_esp_idf` (Header), Host-Tests.
- *F1-/Fehlervertrag:* Sendefehler → keine weiteren Chunks, kein Terminator, Adapter gibt
  den Fehler zurueck; jede Ersetzung loescht den Anhang (§4.2); faellt der Asset aus, zeigt die
  Shell einen sichtbaren sprachneutralen Hinweis statt einer leeren Seite und loest keine Aktion aus.
- *Flash/Heap/Stack (ESTIMATED bis Messung):* der Asset ersetzt den heutigen Suffix 1:1;
  `.flash.text` rund +0,4…0,8 KB (Route, CRC, Metadatenfeld), `.dram0.*`/`.iram0.*` ±0;
  Asset-Antwort 0 B Koerper-Heap (Flash-Chunks wie heute); `handleAsset` ohne Antwort-String,
  Frame ≤ `handleShell` (352 B); die tiefste HTTPD-Kette (Status-Handler) bleibt unveraendert.
  Erstaufruf zwei Requests, danach gecacht; die Wirkung auf Sockets/F4 bleibt offline
  `NOT_PROVEN` (D9, §9).
- *Verworfene Alternativen:* B1–B3; Lazy-Feature-Assets; serverseitige Einzelsprachen-Suffixe;
  SPA/Framework/Dateisystem/gzip/`httpd_resp_send_file`.

## 5. Navigation

Eine `<nav>` mit **nur den umgesetzten Bereichen** (Übersicht, Meldungen (n), Programme,
Einstellungen – jeweils erst mit ihrem Slice; kein Eintrag fuer Diagnose, Service, System
oder Manueller Betrieb). Sichtbar nur mit gueltigem `LP`; `aria-current`; Touchflaechen
≥ 2,75 rem; Mobil einspaltig/umbrechend, ab 40 rem dieselbe Leiste (Seitenmenue bleibt
einem Stand mit mehr Bereichen vorbehalten). Sicht in einer Variablen, gespiegelt im
Fragment (`#alerts`, `#programs`, `#settings`; jedes andere Fragment wird ignoriert); Wechsel
loest keinen Request aus. Stale/Offline gedaempft mit Abrufalter; alle Aktionen weiter nur
aus `P`.

## 6. Fachliche Inhalte der Slices

### 6.1 Meldungen (C1)

Alle gelieferten Meldungen in API-Reihenfolge (kein Umsortieren im Browser), Code
(unlokalisiert wie heute), Schweregrad, Zustand (aktiv/quittiert/stumm/erledigt/Entscheidung
erforderlich), je Zeile `Quittieren`/`Stummschalten` ueber
`send({t:'ack-message'|'mute-message',id})`; deaktiviert bei `B`, `!P`, erledigt oder
erreichtem Zustand. Die Aktionsleiste der Übersicht bleibt unveraendert. Keine Titel,
Zeitpunkte oder Historie.

### 6.2 Programme (C2) – Programmdetail-API als vollstaendiger Vertrag (B2-Korrektur)

**Datenquelle.** Die Antwort wird in der Application aus dem kanonischen Programmkatalog
(Runtime-Lease, `ProgramDefinition`) mit der **einen** vorhandenen Praesentationsfunktion
`makeProgramSummary` erzeugt; diese wird in C2 aus der dateilokalen Definition in einen
gemeinsamen, deklarierten Ort verschoben (keine zweite Kopie, kein Browser-Fachwissen). Die
Zusammenfassung wird **ohne** Overrides projiziert (gespeicherte Programmwerte); Overrides
werden nie serverseitig vorab angewendet (GET ohne Query) und vom Browser nur als Eingabe an
`start-program` uebergeben.

**Route.** `GET /api/v1/programs/<id>` (nur GET, Session wie die anderen Lesepfade).
`<id>`: kanonischer Programm-Identifier, validiert mit den vorhandenen Grenzen
(`validateLowercaseIdentifier`, `kMinimumProgramIdBytes = 1` … `kMaximumProgramIdBytes = 48`);
Pfade mit Query, Unterpfaden oder ungueltigen Zeichen enden ohne Katalogzugriff. Nur
installierte **und** aktivierte Programme sind lesbar (wie `/programs`).

**Antwort `200` (`application/json; charset=utf-8`, Version 1, Body ≤ 3 072 B):**

| Feld | Typ | Pflicht | Bedeutung |
|---|---|---|---|
| `version` | Zahl `1` | ja | Vertragsversion |
| `catalogRevision` | String (dezimal) | ja | Katalogrevision derselben Lease; muss `revisions.programCatalog` aus `/status` und der geladenen Auswahl entsprechen |
| `program.id` | String | ja | Identifier |
| `program.name` | String ≤ 96 B | ja | sichtbarer Name (nur `textContent`) |
| `program.builtIn` | Bool | ja | Standardprogramm |
| `program.startable` | Bool | ja | gespeichertes Programm besteht die bestehende Runnable-Validierung (`ValidationPurpose::Runnable`); sonst `false` (z. B. fehlender Inbetriebnahmewert) |
| `program.targetCelsius` | Zahl oder `null` | ja | erste Gaerstufe; `null` = nicht gesetzt (nie 0) |
| `program.durationMinutes` | Ganzzahl oder `null` | ja | erste Gaerstufe in Minuten; `null` = nicht gesetzt |
| `program.preheat` | Bool | ja | Vorheizen |
| `program.sensor` | String | ja | `product-if-available-else-air` \| `air-product-optional` \| `product-required` \| `air-only` |
| `program.defaultSensorMode` | String oder `null` | ja | Ergebnis von `defaultProgramStartSensorMode`: `product` \| `air`; `null` wenn nicht bestimmbar |
| `program.completion.mode` | String | ja | `finish` \| `cool-finish` \| `cool-hold-duration` \| `cool-hold-manual` (dieselben Tokens und Labels wie `run.completion.mode`) |
| `program.completion.coolingTargetCelsius` | Zahl oder `null` | ja | nur sinnvoll bei Kuehlmodi, sonst `null` |
| `program.completion.holdDurationMinutes` | Ganzzahl oder `null` | ja | nur bei `cool-hold-duration`, sonst `null` |

Es werden nur diese Felder ausgeliefert (kein `notes`, keine Stufenliste, keine
Sensorausfall-/Qualifikations-/Technikparameter, keine Limits). Fehlende oder noch nicht
festgelegte Werte (`TBD_COMMISSIONING`) erscheinen ausschliesslich als `null`, nie als 0 oder
Platzhaltertext; ein Programm mit solchen Werten hat `startable:false`.

Beispiel (Normalfall):

```json
{"version":1,"catalogRevision":"7","program":{"id":"joghurt-mild","name":"Joghurt mild",
"builtIn":true,"startable":true,"targetCelsius":42.0,"durationMinutes":480,"preheat":false,
"sensor":"product-if-available-else-air","defaultSensorMode":"product",
"completion":{"mode":"cool-hold-duration","coolingTargetCelsius":6.0,"holdDurationMinutes":120}}}
```

Beispiel (fehlender Inbetriebnahmewert):

```json
{"version":1,"catalogRevision":"7","program":{"id":"sauerteig","name":"Sauerteig","builtIn":true,
"startable":false,"targetCelsius":null,"durationMinutes":null,"preheat":false,
"sensor":"air-only","defaultSensorMode":"air",
"completion":{"mode":"finish","coolingTargetCelsius":null,"holdDurationMinutes":null}}}
```

**Fehlerfaelle (fail-closed, kein Teil-Body):**

| Fall | Status | Body |
|---|---|---|
| keine/ungueltige Session | 401 | `{"error":"session-required"}` (wie die uebrigen Lesepfade) |
| Methode ausser GET | 405 | `{"error":"method-not-allowed"}` |
| Anfrage mit Body | 400 | `{"error":"body-not-allowed"}` |
| `<id>` verletzt die Identifier-Grenzen/Zeichensatz | 400 | `{"error":"invalid-program-id"}` |
| Programm unbekannt, nicht installiert oder deaktiviert | 404 | `{"error":"program-not-found"}` |
| Katalogquelle/Lease nicht verfuegbar, Ausgabe > 3 072 B | 503 | bestehender `unavailable()`-Body, nie eine Teilantwort |

**Stale-Bindung.** Der Browser vergleicht `catalogRevision` der Detailantwort mit
`revisions.programCatalog` aus `/status` und der geladenen Liste (`PC`); bei Abweichung
oder Fehler wird die Auswahl verworfen und Liste/Detail neu geladen (bestehende Regel), ohne
automatische Mutation. Die Application prueft die erwartete Katalogrevision beim Start
erneut (`stale`).

**Start mit Anpassung (bestehender Intent).** `start-program` bleibt
`{"t":"start-program","c":{"p":"<id>","x":<°C>,"d":<min>,"h":<bool>,"s":"product"|"air","c":"finish-without-cooling"|"cool-then-finish"|"cool-and-hold-for-duration"|"cool-and-hold-until-manual-stop","k":<°C>,"l":<min>}}`
mit nur den geaenderten Optionen; Felder werden nur gesendet, wenn der Benutzer sie von den
angezeigten Programmwerten abweichend setzt. Die Anzeige-Tokens der Detailantwort (`finish`,
`cool-finish`, …) werden ueber eine feste 4-Eintraege-Tabelle auf die Intent-Tokens abgebildet
(ein nativer Bindungstest sichert beide Seiten, wie SIM-27-R-05). Bereichsgrenzen
(`kMin/MaximumFermentationTemperatureCelsius` 4–45 °C, Dauer bis 20 160 min, Kuehlziel 4–25 °C)
prueft schon der Codec; die fachliche Gueltigkeit der angepassten Werte entscheidet die
Application (`rejected`). Revisions- und Bestaetigungsbindung bleiben: `r` traegt
`stateSequence`, `run`, `programCatalog` (`c`) aus dem letzten gueltigen Status, Start ist nur bei
`P`, `standby`, passender Katalogrevision und gewaehltem Programm angeboten, der erste Klick
bindet die Bestaetigung an Modus, Laufrevision, Katalogrevision, Programm **und die
eingegebenen Anpassungen**; jeder Wechsel davon verwirft sie; zwei Klicks, Single-Flight,
Mutationssequenz und CSRF unveraendert.

### 6.3 Einstellungen (C3, D5 = a) – Geraetename und Zeitzone (B3-Korrektur)

**Grundregel.** Der HTTP-Handler schreibt **nie** direkt auf Konfigurationsdaten. Er dekodiert
einen geschlossenen Intent und ruft ausschliesslich einen Application-Befehl; Preview, Commit,
Validierung, Revisionsschutz und Persistenz bleiben bei Application/Konfigurationsdienst.

**Lesepfad.** `GET /api/v1/settings` (nur GET, Session, Body ≤ 3 072 B) liefert, aus einer
Lease: `{"version":1,"revision":"<userConfiguration>","deviceName":"…","timeZone":"Europe/Zurich","timeZones":["Europe/Zurich"],"nameEditable":<bool>}`
(`nameEditable:false` bei aktivem Programm-/Manuallauf, wie Touch; `timeZones` aus der
kanonischen Tabelle). Die Seite laedt es beim Betreten und bei Aenderung von
`revisions.userConfiguration`; Fehler/503 sperren die Aktionen (kein Raten).

**Geraetename – bestehender Weg.** Neuer geschlossener Intent
`{"t":"set-device-name","n":"<name>"}` mit `r.u` = `expectedUserConfigurationRevision`.
Der Handler ruft `FermentationApplication::applyUserSettings({deviceName}, expectedRevision)`
unveraendert. Validierung (`validateVisibleName`, ≤ 96 B, kein Steuerzeichen) und
Laufsperre (aktiver Programm-/Manuallauf → `NotAllowed`) liegen dort.

**Zeitzone – ausdruecklich neuer Application-Befehl.** Im oeffentlichen
`FermentationUiUserSettingsChange` existiert nur `deviceName`; es gibt keinen Befehl zum
Aendern der Zone. Spezifikation: `FermentationApplication::applyTimeZone(canonicalId,
expectedRevision)` im selben Muster wie `applyUserSettings` (ueber
`commitUserConfigurationChange`, setzt `timeZoneId`; die Validierung der Konfiguration
(`UnknownTimeZoneId`/`TimeZoneRejected`) und `ITimeZoneResolver::prepare` laufen im
Konfigurationsdienst; die Aktivierung erfolgt, indem der Commit den Runtime-Snapshot mit der
neu vorbereiteten `PreparedTimeZone` austauscht). Laufzustand: erlaubt (die Zone aendert nur
Anzeige/Lokalzeit, nicht neustartpflichtig, veraendert keinen Lauf – SETTINGS_AND_STORAGE);
fehlt die Revision oder der Zustand, schlaegt der Befehl fail-closed fehl. Intent
`{"t":"set-time-zone","z":"<canonical id>"}` mit `r.u`; Wirkung sichtbar ab dem naechsten
Status (`clock.zone`, `clock.local`).

**Web-Vertrag fuer beide Intents.** Ausschliesslich `POST /internal/ui/run` (Session, CSRF,
`X-UI-Mutation-Seq`, Replay-Cache); die Revisionsschluessel-Tabelle erhaelt `userConfiguration:'u'`
bereits; neue Pflicht: `r.u` ist fuer diese Intents Pflicht. Codec: geschlossene Schluessel,
Laengen-/Zeichenvalidierung, Replay-Budget (Body ≤ 1 024 B, Antwort-Cache wie fuer Run-Intents)
mit nativem Maximalfall. Zwei-Klick-Bestaetigung im Browser (gebunden an Revision und
Eingabewert); eine Aenderung der Revision verwirft sie.

**Fehlermapping bis zur Application-Grenze (Tabelle als nativer Test, ohne neue Outcome-Namen).**

| Application-Ergebnis | Web-Outcome |
|---|---|
| Preview `StateChanged` / Commit `ConfigurationConflictFailure` (Revision veraltet/fehlt) | `stale` (409) |
| Preview `InvalidCandidate` / Commit `ConfigurationValidationFailure` (Name/Zone ungueltig) | `rejected` (422) |
| Preview `NotAllowed` (aktiver Lauf beim Namen) | `rejected` (422), UI zeigt „waehrend eines Laufs nicht aenderbar“ |
| Preview `ConfigurationRuntimeUnavailable` / Commit `ConfigurationRuntimeFailure` | `unavailable` (503), nichts geaendert |
| Persistenzfehler beim Commit | `write-failed` |
| Erfolg | `applied`; danach frischer Status/Settings |

**Wirkung einer Namensaenderung (Netz, Aussperrrisiko).** Nach dem Code wird der Hostname bzw.
die SoftAP-SSID aus dem Namen nur bei `initializeNetwork` und `applyNetworkMode` abgeleitet
(`deriveSoftApSsid`, `setHostname`); `applyUserSettings` ruft beides **nicht**. Eine
Namensaenderung laeuft also nicht in eine laufende AP-/WLAN-Verbindung und kappt keine
Sitzung; die neue Bezeichnung (mDNS/SSID) greift erst beim naechsten Netzwerkstart bzw.
Geraetestart. C0 belegt das mit einem nativen Test (`MockNetworkLifecycle`: nach
`applyUserSettings` unveraenderter Hostname/AP-Name, keine `start`/`stop`-Aufrufe,
Session bleibt gueltig) und die Seite zeigt einen entsprechenden Hinweis. Netzwerkmodus,
Heim-WLAN-Zugang, Webzugang und Display-Sprache werden **nicht** per Web angeboten (lokal,
Sitzungsentzug/Aussperrrisiko).

**Offene fachliche Fragen – HOLD nur fuer den jeweils betroffenen Teil von C3:**

- **Q1 (Geraetename).** Ist „Hostname/mDNS/SoftAP-Name wirksam erst beim naechsten
  Netzwerkstart/Geraetestart“ (aktuelles Verhalten) das gewollte R1-Verhalten, oder soll eine
  Namensaenderung den Netzwerkstack sofort neu konfigurieren? Letzteres waere ein
  Netzwerk-/Sitzungsvertrag (Aussperrrisiko im AP-Betrieb) und ist **nicht** Teil dieses
  Plans. Empfehlung: aktuelles Verhalten beibehalten. Ohne Antwort bleibt der Namensteil von C3
  im HOLD (der Zeitzonenteil laeuft unabhaengig).
- **Q2 (Zeitzone).** `kSupportedTimeZones` enthaelt nur `Europe/Zurich`; es gibt weder einen
  Application-Befehl noch eine zweite waehlbare Zone. Soll der Befehl `applyTimeZone` samt
  Intent jetzt gebaut werden (vertragskonform, die Auswahl hat heute genau einen Eintrag und
  wird als schreibgeschuetzter Wert ohne toten Knopf gezeigt, wirksam sobald der Katalog waechst),
  oder erst gemeinsam mit einer Katalogerweiterung? Empfehlung: Befehl und Intent jetzt, UI
  zeigt bei einem Katalogeintrag nur den Wert. Ohne Antwort bleibt der Zeitzonenteil von C3
  im HOLD; kein stilles Weglassen von D5-a.

### 6.4 Konfliktdarstellung (C5)

Das Ergebnis `stale`/`busy`/`rejected`/`confirmation-required` wird heute zu „Nicht
ausgefuehrt“ vereinheitlicht. C5 zeigt eine kurze, uebersetzte Ursache je Outcome – nur
Anzeige, keine Aenderung der Outcome-Matrix.

## 7. Verbindliche Slice-/Commit-Abfolge (ein PR)

0. **C0 – Ressourcen-, Navigations- und Vertragsgrundlage** (vor C1–C3, mit den drei
   Voraussetzungen davor):
   a. *Vertragsvoraussetzung B1:* getrennte Konstanten und Grenztests fuer normale Shell,
      Setup-Shell und Asset (§4.2), Reservenachweis je Shell.
   b. Plattform: `HttpResponseMetadata::cache` (Validierung, Adapter-Header, Host-Tests).
   c. Asset-Route mit CRC-Pfad, GET-only, Metadatenpruefung, Fehlervertrag; **reine
      Verschiebung** von `kWebShellSuffix` in den Asset, Shell referenziert ihn (inkl.
      Ausfallhinweis); Goldens, JS-Test (Asset als Quelle), Native-Tests angepasst; **kein
      Verhaltensunterschied** (alle 74 JS-Faelle + 109 Native-Webtests gruen); Setup-Shell
      unveraendert inline.
   d. *Vertragsvoraussetzung B2:* Messung des Programmdetail-Bodys (96-B-Namen, Escaping,
      16 Programme) und Verschiebung von `makeProgramSummary` in einen gemeinsamen Ort ohne
      Verhaltensaenderung (bestehende Touch-Tests gruen).
   e. *Vertragsvoraussetzung B3:* nativer Test „Namensaenderung rekonfiguriert das Netz nicht“,
      Spezifikation `applyTimeZone`, Fragen Q1/Q2 im Plan-Delta beantwortet.
   *Zwischenreview optional (gezielt).*
1. **C1 – Navigation und Meldungen.** Kandidat B1 als Messbasis; Fragment-Routing; Labels;
   Zeilenknoepfe.
2. **C2 – Programme.** `GET /api/v1/programs/<id>` (Codec/Handler/Tests), Detail- und
   Anpassungsansicht, Anbindung an `start-program`.
3. **C3 – Einstellungen.** `GET /api/v1/settings`, Intents `set-device-name`/`set-time-zone`,
   `applyTimeZone`, UI mit Bestaetigung; Teile mit offener Frage Q1/Q2 bleiben bis zur Antwort
   im HOLD, der uebrige Teil laeuft.
4. **C5 – Integration, Regression, Evidence.** Outcome-Anzeige, WEB_UI-Angleichung
   (Werksreset nur lokal, Asset/Navigation/Seiten), ACCEPTANCE_TESTS, ROADMAP, Issue-Status,
   vollstaendige Offline-Ressourcenevidence, Builder-Self-Check; danach **ein**
   unabhaengiger Full Review ueber den gesamten PR (optionale gezielte Zwischenreviews nach C0
   und C2, kein Full Review je Commit). **C4 entfaellt (D7).**

Jeder Schnitt wird auf seinen Quell-/Testcommit gebunden und bleibt einzeln revertierbar.

## 8. Gezielte Nachweise

| Bereich | Faelle |
|---|---|
| Echter extrahierter JS-Test (`scripts/test_web_shell_polling.mjs`, Fake-DOM erweitert) | bisherige 74 Faelle erhalten; je Slice: Navigation nur mit `LP`, Fragment gueltig/ungueltig, Sichtwechsel ohne Request, DE/EN/ES vollstaendig, `sl()`-Umrendern; Meldungen: Reihenfolge, 16 Eintraege, richtige `id`/Intent, Sperren bei `B`/`!P`/Stale/Offline/erledigt; Programme: Detail aus Beispielantwort, `null`/`startable:false` → Start gesperrt, Revisionsvergleich verwirft veraltete Auswahl, Anpassungen werden nur bei Abweichung gesendet, Bestaetigung an Eingaben gebunden; Einstellungen: Revisionsbindung, Zwei-Klick, kein Request ohne Bestaetigung, `nameEditable:false`; Mutationsgegenproben (fehlende `P`-Pruefung, falsche `id`, ungueltiges Fragment, Override ohne Bestaetigungsbindung) |
| Native `test_web_application_routes` | Grenztests je Shell/Asset (§4.2); Asset-Route (GET-only, 405, 404 fuer falsche/alte Version, Header nur beim exakten Pfad, byte-identisch in allen Auth-Zustaenden, kein Cookie); Programmdetail-Route (alle Fehlerfaelle der Tabelle, Identifier-Grenzen, Bodygrenze mit 96-B-Name, Revisionsbindung, Auth); Settings-Route; Intents `set-device-name`/`set-time-zone` (Codec-Grenzen, Replay-Budget, Outcome-Mapping-Tabelle, kein direkter Konfigurationszugriff im Handler, Laufsperre, Session bleibt nach Namensaenderung gueltig) |
| Application/Konfiguration | `applyTimeZone` (gueltig, unbekannte Zone, fehlende Revision, Konflikt, Zone im Lauf erlaubt, Aktivierung im naechsten Snapshot); Namensaenderung ohne Netzwerkaufrufe; bestehende `applyUserSettings`-Tests unveraendert |
| Plattform/Adapter | `test_http_server_lifecycle` (Metadatenvalidierung), `esp_idf_http_server_adapter_host` (Header nur beim Asset, Chunk-Reihenfolge, Fehler in Kopf/Anhang/Terminator, suffixlose Antworten unveraendert) |
| Regression | `test_fermentation_ui_models`, `test_renderer_boundary`, `test_ui_steady_state_allocations`, `test_run_detail_projection`, `test_run_commands`, `test_configuration_service`, `test_local_touch_ui` (Programmzusammenfassung), `check_architecture_boundaries.py` |
| Ressourcen offline | Sektionsvergleich (`.dram0.*`, `.iram0.*`, `.flash.*`) gegen `main`, `-fstack-usage` (`handleAsset`, `handleShell`, Programm-/Settings-Handler, Codec), Asset-/Shell-Bytes je Slice, Allokationsfenster neuer Handler, beide ESP-Profile; hoechstens `RAM_OFFLINE_ESTIMATED` |
| Sicherheit | Asset ohne Sitzungsdaten, kein neuer unauthentifizierter Schreibpfad, CSRF/Sequenz/Replay fuer jede neue Mutation, kein Netzwerk-/Service-/Reset-Zugriff ueber die neuen Seiten |
| Abgrenzung der Evidence | Fake-DOM-/Native-Evidence belegt Logik und Vertraege. **Browser, Mobil-/Desktop-Layout, Barrierefreiheit, Geraetelast, HWM und Heap bleiben `NOT_PROVEN`**; es wird keine Browser- oder Hardwarequalifikation behauptet. Layout nur strukturell (Viewport/Media-Query-Strings, Touchflaechen-Regel) |

Geplante Befehle (nicht jetzt):

```bash
node scripts/test_web_shell_polling.mjs
pio test -e native -f test_web_application_routes -f test_http_server_lifecycle -f test_fermentation_ui_models -f test_renderer_boundary -f test_ui_steady_state_allocations -f test_run_detail_projection -f test_run_commands -f test_configuration_service -f test_local_touch_ui
python3 scripts/build_esp_idf_profiles.py all --require-clean-source-tree
export PRE_READY_EXPECTED_HEAD="$(git rev-parse HEAD)"
bash scripts/run_pre_ready_gates.sh self-check
```

Pre-Ready nur nach Independent Review, `OPEN_BLOCKERS=0` und ausdruecklicher Owneranweisung.

## 9. D9 – spaeterer, getrennt freizugebender Testpunkt vor Ready

Nach dem Builder-Self-Check und dem Independent Review, **nur nach gesonderter
ausdruecklicher Hardware-/Flash-/Browserfreigabe** (Flash, UART und Browserzugriff je einzeln),
auf einem aktorfreien Board mit Messbild (Mess-Overlay oder Produktimage, im Auftrag zu
benennen), ein Client:

1. Erstabruf Shell + Asset (zwei Requests), Login, Navigation, Programme, Meldungen,
   Einstellungen; Mobil- und Desktop-Browser.
2. Cache-Treffer (zweiter Aufruf nur Shell), **Versionswechsel** (alte Asset-URL → 404, sichtbarer
   Hinweis, Neuladen genuegt), **Asset-Ausfall** (blockiert/503: Hinweis, keine Aktion).
3. Heap (frei/Minimum/groesster Block), Main-/HTTPD-Stack-HWM, `ui_skipped_low_memory`,
   `heap_alloc_failed`, Reset/Panic/Watchdog, HTTP-Fehler und Erholung gegen den eigenen
   Idle-Bezug; Referenz: Evidence #206/#207/#208 mit benannten Unterschieden, keine neuen Schwellen.

Bis zur Freigabe und Durchfuehrung bleibt das Ergebnis `NOT_PROVEN`; ein Offline-PASS ersetzt
dieses Gate nicht und es gilt kein implizites Gate-PASS.

## 10. Verbleibende Ownerfragen

Nur noch **Q1** (Hostname-/SSID-Wirkung einer Namensaenderung) und **Q2**
(`applyTimeZone` jetzt oder mit Katalogerweiterung), beide mit Empfehlung in §6.3 und mit
HOLD ausschliesslich fuer den betroffenen C3-Teil. Sie stehen nicht vor C0, C1, C2 oder C5.

## 11. Risiken

- Zwei Requests beim Erstaufruf erhoehen kurzzeitig HTTPD-/WLAN-Last; nur per D9 messbar.
- Globale `const`/`let` des Inline-Skripts (`S`, `A`, `U`, `Q`) muessen im nachfolgenden
  klassischen Script sichtbar sein; Test mit zwei getrennten `vm`-Skripten.
- Veralteter Tab nach Firmwareupdate: alte Asset-URL → 404; Hinweis, Neuladen genuegt.
- Asset-Wachstum: Limit 16 384 B; danach seitenweise Assets (eigener Plan), keine stille
  Erhoehung.
- Einstellungen sind die erste neue Mutationsfamilie ausser dem Lauf; Revisionsschutz,
  Replay-Budget und Laufsperre muessen im Test belegt sein.
- Matrixzeilen H/X bleiben offen; R1-U/D9/G2 und Aktorfreigabe unveraendert offen.
