# Issue #27 – R1-Web-Restumfang (integrierter Gesamtplan, ein Draft-PR)

**2026-10-21 · PLAN-REVISION 2.3 (HTTPD-Stackbefund und begrenzter Korrekturvorschlag) · D1–D9/Q1/Q2 UNVERAENDERT · ERWEITERUNG NICHT FREIGEGEBEN · WEITERE PRODUKTARBEIT GESPERRT.**
[Issue #27](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/issues/27)
bleibt OPEN. Branch `agent/issue-27-r1-web-navigation-plan` (Name historisch), Draft-PR
[#209](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/pull/209). Dieser Plan ist
die einzige kanonische, vollstaendige Fassung. Historisch und nicht mehr massgeblich:
Revision 1 (Plan-SHA `4927e856df8cb3474a7b0ed74aa92917dd23cc6b`), Revision 2 (Plan-SHA `d940c2db329151c4a55a63385cce6bd18473e6f7`) und Revision 2.1 (Plan-SHA `7c3bbae6fd29bbe1679c39929f086c2c92e76bf0`, beide ersetzt durch diese Fassung) und der fruehere reine
Navigationsplan (`0614c054e3303ef2aacd495b8221e512eb4c0714`); deren Messbelege bleiben
unter `audits/WEB_NAV_BUDGET_20261020/`. Die Ownerentscheide unten sind **keine** Freigabe
dieser Revision: Umgesetzt wird erst nach ausdruecklicher Freigabe der exakten Plan-SHA.

**Freigegebene Implementierungsbaseline:** Revision 2.2 auf
`b7d5e2dbc1f5281c764d4a75f2c37f1c4b6397c2` wurde am 2026-10-21 einschliesslich
C0 → C1 → C2 → C3 → C5 vom Owner freigegeben. Der Softwarekandidat liegt auf
`4f6f0c9abad9e1c354fc67e140302adcf62680e9`; funktionale Tests und beide Profilbuilds
sind PASS, das neue Settings-HTTPD-Stackgate ist **FAIL / HOLD** (§12).
Diese vollstaendige Revision ersetzt den Planungsstand fuer die noch offene
Ressourcenkorrektur; sie ist selbst **nicht freigegeben**. Die bisherige Freigabe
erlaubt weder den zusaetzlichen Eingriff in den gemeinsamen Konfigurations-
Commit-Pfad noch eine Erhoehung des HTTPD-Stacks.

## 0. Bestaetigte Ownerentscheide (Revision 2)

| ID | Entscheid |
|---|---|
| **D1 = A** | Oeffentlicher, versionierter statischer JS-Asset fuer den weiteren R1-Webumfang; keine weitere Erhoehung des bisherigen Shell-Limits (11 456 B entfaellt). |
| **D2** | Getrennte feste Obergrenzen: normale Asset-Shell **2 048 B**, Asset **16 384 B**, Setup-Shell **eigene** Grenze (Kandidat **4 096 B**, Reservenachweis §4.2). Normalreserve und Fehlervertraege je Shell erhalten. Die 25-%-Abweichung vom geschaetzten Slice-Aufwand ist ein **Analyse-/Warnsignal, kein automatischer HOLD**; absolute Ressourcen- und Vertragsgrenzen bleiben harte Gates. |
| **D3** | Der Asset ist ohne Session oeffentlich abrufbar und enthaelt keine Session-, CSRF-, Mutationssequenz-, Credential- oder Konfigurationsdaten. |
| **D4** | Versionspfad mit CRC-32 und `Cache-Control: private, max-age=31536000, immutable` ausschliesslich fuer den exakten Asset; alte/falsche Pfade fail-closed. |
| **D5 = a** | Web-Einstellungen fuer den **Geraetenamen**, nur ueber zustaendige Application-/Konfigurations-Owner und revisionsgeschuetzte Mutationsvertraege. Die **Zeitzone** wird gemaess Q2 nur **lesbar** gezeigt (Schreiben zurueckgestellt, §6.3). Netzwechsel-/Aussperrrisiken werden behandelt. |
| **D6 = a** | Kein Benutzerprogramm-Editor in diesem PR; separater spezifizierter Folgescope. |
| **D7 = a** | Keine Web-Service-PIN-Route/-Seite ohne tatsaechlichen Funktionsverbraucher; **C4 entfaellt**, es wird keine Scheinseite angelegt. |
| **D8** | Vollstaendiger Werksreset ausschliesslich lokal; die widersprechende Web-Beispielaussage in `docs/WEB_UI.md` wird mit dem Safety-Vertrag abgeglichen (C5). |
| **D9** | Gezielte aktorfreie Browser-/Geraetemessung vor Ready fuer den geaenderten Asset-/HTTPD-Pfad – **erst nach gesonderter ausdruecklicher Hardware-/Flash-/Browserfreigabe** (§9). Bis dahin `NOT_PROVEN`, kein implizites Gate-PASS. |

**Weitere bestaetigte Ownerentscheide (Revision 2.2).**

| ID | Entscheid |
|---|---|
| **Q1** | Bestehendes Verhalten beibehalten: Eine Aenderung des Geraetenamens aendert Hostname/mDNS/SoftAP-SSID **erst beim naechsten normalen Netzwerkstart bzw. Geraetestart**. Keine sofortige Rekonfiguration, keine Unterbrechung der laufenden Verbindung; die Web-UI weist klar auf die verzoegerte Wirkung hin. Der native Test zur ausbleibenden Netzwerkrekonfiguration bleibt. |
| **Q2** | Zeitzonenaenderung **zurueckgestellt** (KISS/YAGNI): Solange `kSupportedTimeZones` nur `Europe/Zurich` enthaelt, zeigt die Web-UI die kanonische Zone lesbar, nicht editierbar und ohne wirkungslosen Speichern-Knopf. `applyTimeZone`, der Intent `set-time-zone` und die nur dafuer geplanten Implementierungs-/Mutationstests sind **aus diesem PR gestrichen**; die schreibende Zeitzonenfunktion wird erst bei einem tatsaechlich erweiterten, freigegebenen Katalog geplant. Die bestehende Anzeige-/Konfigurationsgrundlage wird nicht zurueckgebaut. |

D1–D9 und Q1/Q2 bleiben entschieden; es gibt keinen Q1-/Q2-HOLD. Neu offen ist
**Q3: Freigabe der begrenzten Stackkorrektur in §12 auf der exakten SHA dieser
Revision**. Bis dahin gilt Ressourcen-HOLD; D9 bleibt separat freizugeben.

**Kernaussagen.** (1) Ein PR (C0, C1, C2, C3, C5). (2) Abgetrennt bleiben Manueller Betrieb,
Diagnose/Diagramme/Exporte (#28), Journale/Protokolle/Backup/Werksreset (#19),
Service-Funktionen und -Route, Programmeditor, Favicon (#204). (3) Q1/Q2 sind entschieden
(§0); die Ressourcenqualifikation des Settings-Schreibpfads ist FAIL/HOLD (§12). (4) Keine Hardware-, Flash-, Browser-, UART-, Reset- oder Aktorfreigabe;
`ACTUATOR_RELEASE=NO`, `RAM_PASS=NOT_PROVEN`.

## 1. Live-Basis

Implementierungsstand 2026-10-21: C0 `d1170b2`, C1 `d2446ee`, C2 `27aa12c`,
C3 `57fa250`, C5 `75970d7`, Integrationskorrektur `4f6f0c9`. Das ist ein
nicht hardwarequalifizierter Draft-Kandidat, keine abgeschlossene C5-Freigabe.
Messungen und Grenzen stehen in der [Evidence](../audits/PR209_WEB_REMAINING_SCOPE_IMPLEMENTATION.md).
Die folgende Ausgangsbasis bleibt fuer die bereits implementierten Anforderungen
massgeblich; die neue Abweichung wird ausschliesslich in §12 behandelt.

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

| Zeitzonen | `kSupportedTimeZones` (device_platform) enthaelt **genau einen** Eintrag, `Europe/Zurich`; die Zone wird heute nur angezeigt (`/status` `clock.zone`); es gibt keinen Application-Befehl zum Aendern der Zone und er wird in diesem PR **nicht** geschaffen (Q2) |

| Baustein | Beleg |
|---|---|
| Read-only-API | `GET /api/v1/status` (Revisionen, Lauf-/Standby-/Netzwerk-/Zeitinfo), `/temperatures`, `/alerts` (id, code, severity, active, acknowledged, resolved, decisionRequired, muted, revision; ≤ 16), `/programs` (nur `id`/`name`, `catalogRevision`) |
| Mutationsroute | `POST /internal/ui/run`, geschlossene Intents im Codec: `start-program` (`c` mit `p,x,d,h,s,c,k,l`), `stop-run`, `complete-run`, `ack-message`, `mute-message`, `adjust-run`, `recovery-time-correction`, `reset-fault`, `sensor-selection`, `start-manual-timed`, `start-manual-holding`; Revisionsschluessel `s,r,m,f,e,u,c`; Application entscheidet (Outcome-Matrix `applied/stale/busy/confirmation-required/rejected/too-large/write-failed`) |
| Programmpraesentation | `FermentationUiProgramSummaryView` und `makeProgramSummary(...)` in `fermentation_touch_workspace.*` (kanonische Zusammenfassung mit angewendeten Overrides ueber `applyStartCandidateOverrides`; fehlende Werte bleiben `nullopt`, nie 0); `makeProgramSummary` ist heute **dateilokal** |
| Konfigurationsweg | `FermentationApplication::applyUserSettings(change, expectedRevision)` (nur `deviceName`; lehnt bei aktivem Lauf ab; Rueckgabe `ApplicationConfigurationChangeResult{Preview, Commit}`); der dateilokale Helper `commitUserConfigurationChange(...)` persistiert fest die Herkunft `{ChangeOriginKind::LocalDisplay, 2U}`; das kanonische Modell kennt `{ChangeOriginKind::WebInterface, 3U}` (`configuration_graph.*`). Bei fehlgeschlagener Vorschau ist der Commit-Status nur ein Platzhalter (`ConfigurationRuntimeFailure`) |
| Mutationsrouten-Aufbau | `WebRunMutationDto` enthaelt ausschliesslich `FermentationUiEnvelopePayload` (Run-/Persistence-`CommandEnvelope`); `WebRunMutationHandler::handle` prueft in fester Reihenfolge Methode, Metadaten, Content-Type, Same-Origin, Body, Dekodierung, Digest, Session, CSRF, Sequenz, `touch`, `reserveMutation`, fuehrt dann immer `prepareEnvelope` → `confirmPrepared` → `applyConfirmedPrepared` aus und schliesst mit `completeMutation` und einem Wert aus dem **geschlossenen** `ReplayOutcomeCode` {Applied, Unavailable, Stale, Rejected, Busy, ConfirmationRequired, WriteFailed, TooLarge}; Konfigurations-, Netzwerk- und unbekannte Details sind fuer diese Route ausdruecklich `Unavailable` |
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
| **Einstellungen** – Geraetename | **F** | Touch-Aenderung vorhanden (`applyUserSettings`); Web ohne Zugriff | C3 |
| Einstellungen – Zeitzone | **U/H** | wird als lesbarer Wert gezeigt; Schreiben zurueckgestellt (Q2), erst bei erweitertem, freigegebenem Katalog planbar | C3 (nur Anzeige) |
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

### 6.3 Einstellungen (C3, D5 = a) – Geraetename; Zeitzone nur lesbar

**Grundregel.** Der HTTP-Handler schreibt **nie** direkt auf Konfigurationsdaten und beruehrt
den Run-/Persistence-`CommandEnvelope` nicht. Konfigurationsoperationen werden **nicht** in
`FermentationUiEnvelopePayload` hineinmodelliert (dieser Vertrag schliesst sie ausdruecklich
aus); eine implizite Erfolgsprojektion ueber `DeviceUiCommandOutcomeCategory` ist ausgeschlossen.

**Lesepfad.** `GET /api/v1/settings` (nur GET, Session, Body ≤ 3 072 B) liefert, aus einer
Lease: `{"version":1,"revision":"<userConfiguration>","deviceName":"…","timeZone":"Europe/Zurich","nameEditable":<bool>}`
(`nameEditable:false` bei aktivem Programm-/Manuallauf, wie Touch; `timeZone` ist der
kanonische Wert der Lease und ausschliesslich zur Anzeige). Die Seite laedt es beim Betreten und bei Aenderung von
`revisions.userConfiguration`; Fehler/503 sperren die Aktionen (kein Raten). Die Zone wird als lesbarer Wert ohne
Bedienelement und ohne Speichern-Knopf gezeigt (Q2).

**C3-F1 – geschlossener typisierter Dispatch in der bestehenden Route (B3-F1).**
`POST /internal/ui/run` bleibt die einzige Mutationsroute. Der Codec dekodiert einen
geschlossenen Typ `WebMutationDto = std::variant<WebRunMutationDto, WebConfigurationMutationDto>`
(`WebConfigurationMutationDto` = erwartete `userConfiguration`-Revision + genau eine Aenderung,
in diesem PR ausschliesslich `DeviceName{name}`; die Variante ist geschlossen und wird nicht
generisch erweitert); unbekannte
Intent-Typen und fehlende/ueberzaehlige Schluessel schlagen fail-closed fehl (`400`). **Alle**
gemeinsamen Pruefungen bleiben **vor** der Verzweigung und fuer beide Befehlsfamilien
identisch und in derselben Reihenfolge: Methode, Metadaten, Content-Type, Same-Origin, Body,
Dekodierung, Digest, Session, CSRF, Mutationssequenz, `touch`, `reserveMutation` (inkl.
Replay-Treffer) und der Abschluss `completeMutation`. Erst danach unterscheidet sich der
Zweig: die Run-Familie laeuft unveraendert `prepareEnvelope` → `confirmPrepared` →
`applyConfirmedPrepared`; die Konfigurationsfamilie ruft einen Application-Eintrag
`applyWebConfigurationChange(const WebConfigurationChange&, expectedRevision)` und projiziert
dessen typisiertes Ergebnis mit der unten stehenden **vollstaendigen** Abbildung auf den
geschlossenen `ReplayOutcomeCode`. Kein zweiter Auth-/Replay-Mechanismus, keine neue
Route, kein direkter Konfigurationszugriff aus dem Handler. Replay (gleiche Sequenz und
gleicher Digest) liefert die gespeicherte Antwort und fuehrt **keine** zweite
Konfigurationsmutation aus; ein Replay mit anderem Digest ist ein Sequenzkonflikt wie bisher.
Abweichender Payload (anderer Digest) bei derselben Sequenz wird wie bisher als Sequenzkonflikt verworfen. Replay-Budget: Body ≤ 1 024 B, gespeicherte Antwort wie fuer Run-Intents (nativer
Maximalfall).

**C3-F2 – serverseitig bestimmte Herkunft (B3-F2).** `commitUserConfigurationChange`
persistiert heute fest `{LocalDisplay, 2U}`; `applyUserSettings` fuer Web unveraendert zu
rufen waere falsch, weil Mutationsquellen als `display` oder `web` unterscheidbar bleiben
muessen. Regel: die Herkunft ist ein **interner, typisierter Parameter** des Application-
Eintritts und nie ein Feld der Anfrage. Umsetzung ohne doppelte Logik: die bestehende Logik
(Laufsperre, Validierung, Revisionspruefung, Preview/Commit ueber `commitConfigurationChange`)
wird in **einen** privaten Eintritt `applyUserConfigurationChange(ChangeOrigin, change,
expectedRevision)` gezogen; `applyUserSettings(...)` bleibt als duenner Touch-Eintritt mit
fest `{LocalDisplay, 2U}` (Signatur und Verhalten unveraendert), der neue
`applyWebConfigurationChange(...)` ist der duenne Web-Eintritt mit fest
`{WebInterface, 3U}`. Der Wert `WebInterface` steht allein im Web-Eintritt (nicht aus DTO,
Header oder Cookie).

**Vollstaendige Ergebnisabbildung auf `ReplayOutcomeCode`.** Massgeblich ist zuerst der
Preview-Status; bei einem Preview-Fehler ist der Commit-Status nur ein Platzhalter und wird
ignoriert. Jede Enumeratorstufe erscheint in einer `switch` ohne `default` (Compiler
erzwingt Vollstaendigkeit) und in einem Test, der **alle** Werte durchlaeuft:

| Status | Outcome | Begruendung |
|---|---|---|
| Preview `Success` | weiter mit Commit-Status | – |
| Preview `ConfigurationRuntimeUnavailable` | `Unavailable` | nichts geaendert |
| Preview `ConfigurationModelBudgetBusy` | `Busy` | Wiederholung moeglich, nichts geaendert |
| Preview `InvalidCandidate` | `Rejected` | Name ungueltig |
| Preview `NotAllowed` | `Rejected` | aktiver Lauf beim Namen |
| Preview `StateChanged` | `Stale` | Revision veraltet/fehlt |
| Preview `PreviewNotFound` | `Unavailable` | interne Inkonsistenz, fail-closed |
| Preview `PreviewSuperseded` | `Busy` | konkurrierende Vorschau, nichts geaendert |
| Commit `Activated` | `Applied` | – |
| Commit `NoChange` | `Applied` | angeforderter Zustand ist bereits aktuell; kein Manifest wird geschrieben |
| Commit `ConfigurationMutationBusy` | `Busy` | Wiederholung moeglich |
| Commit `ConfigurationConflictFailure` | `Stale` | – |
| Commit `ConfigurationValidationFailure` | `Rejected` | – |
| Commit `PersistenceFailure` | `WriteFailed` | sicher nicht geschrieben |
| Commit `CapacityFailure` | `WriteFailed` | bestehender fail-closed Outcome; `TooLarge` meint die Groesse der **Anfrage** und waere hier irrefuehrend (Name/Zone haben feste Groessen, der Speicher ist voll) |
| Commit `ConfigurationCommitIndeterminate` | `Unavailable` | **niemals** `Applied` und **nie** als sicher fehlgeschrieben ausgewiesen; die UI laedt Settings und Status neu und zeigt „Ergebnis unbestimmt, bitte pruefen“ |
| Commit `ConfigurationRuntimeFailure` | `Unavailable` | – |
| Commit `PreviewNotFound` / `PreviewSuperseded` | `Unavailable` / `Busy` | wie oben |
| Commit `ReadyForConfirmation` | `Unavailable` | Zwischenstatus, nie ein Endzustand an dieser Stelle |

Unbekannte/kuenftige Enumeratoren fuehren zu `Unavailable`, nie zu `Applied`.

**Geraetename.** Intent-Koerper `{"v":1,"r":{"u":"<revision>"},"i":{"t":"set-device-name","n":"<name>"}}`,
Application-Eintritt `applyWebConfigurationChange` mit `DeviceName`. Validierung
(`validateVisibleName`, ≤ 96 B, kein Steuerzeichen) und Laufsperre (aktiver Programm-/Manuallauf
→ `NotAllowed` → `Rejected`) liegen in der gemeinsamen Application-Logik.

**Wirkung einer Namensaenderung (Q1 = Ist-Stand beibehalten).** Hostname/mDNS und
SoftAP-SSID werden nur bei `initializeNetwork` und `applyNetworkMode` aus dem Namen abgeleitet;
`applyUserSettings` und der Web-Eintritt rufen weder `setHostname` noch
`setAccessPointCredentials`. Die Aenderung wirkt daher **erst beim naechsten normalen
Netzwerkstart bzw. Geraetestart** und unterbricht keine laufende AP-/WLAN-Verbindung oder
Sitzung. Die Web-UI zeigt neben dem Namensfeld und im Ergebnis einen klaren, uebersetzten Hinweis
(„Hostname und WLAN-Name aendern sich erst nach dem naechsten Netzwerk- bzw. Geraetestart“). Der
native Charakterisierungstest (`MockNetworkLifecycle`: nach dem Web-Commit unveraenderter
Hostname/AP-Name, keine `start`/`stop`-Aufrufe, Session bleibt gueltig) gehoert zu **C3.1**.
Netzwerkmodus, Heim-WLAN-Zugang, Webzugang und Display-Sprache werden **nicht** per Web
angeboten (lokal, Sitzungsentzug/Aussperrrisiko).

**Zeitzone (Q2 = zurueckgestellt).** Die Web-UI zeigt die kanonische Zone aus der Lease lesbar,
nicht editierbar und ohne Speichern-Knopf. `applyTimeZone`, `set-time-zone` und alle nur dafuer
geplanten Codec-/Mapping-/Application-/Mutationstests sind **nicht** Teil dieses PR. Die schreibende
Zeitzonenfunktion wird erst bei einem tatsaechlich erweiterten, freigegebenen Katalog geplant; die
bestehende Anzeige-/Konfigurationsgrundlage bleibt unveraendert.


### 6.4 Konfliktdarstellung (C5)

Das Ergebnis `stale`/`busy`/`rejected`/`confirmation-required` wird heute zu „Nicht
ausgefuehrt“ vereinheitlicht. C5 zeigt eine kurze, uebersetzte Ursache je Outcome – nur
Anzeige, keine Aenderung der Outcome-Matrix.

## 7. Verbindliche Slice-/Commit-Abfolge (ein PR)

0. **C0 – Ressourcen-, Navigations- und Vertragsgrundlage** (vor C1–C3, mit den Vertrags-
   voraussetzungen B1 und B2):
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
   *Zwischenreview optional (gezielt).*
1. **C1 – Navigation und Meldungen.** Kandidat B1 als Messbasis; Fragment-Routing; Labels;
   Zeilenknoepfe.
2. **C2 – Programme.** `GET /api/v1/programs/<id>` (Codec/Handler/Tests), Detail- und
   Anpassungsansicht, Anbindung an `start-program`.
3. **C3 – Einstellungen** (Q1/Q2 entschieden; Ressourcen-HOLD nach §12):
   - **C3.0 Konfigurationsmutations-Infrastruktur** (zusammen mit dem Verbraucher C3.1, kein
     Vorbau): `WebMutationDto`-Variante und geschlossener Dispatch (C3-F1), vollstaendige
     Ergebnisabbildung, privater Eintritt mit serverseitiger Herkunft (C3-F2), `GET /api/v1/settings`.
   - **C3.1 Geraetename** (`set-device-name`, `applyWebConfigurationChange`, UI mit Bestaetigung und
     Hinweis auf die verzoegerte Netzwerkwirkung, Charakterisierungstest, Zone nur lesbar).
   Der bestehende C3-Konfigurationspfad benoetigt zusaetzlich die in §12
   beschriebene, erneut freizugebende Stackkorrektur. Keine weitere
   Produktimplementierung vor Freigabe dieser Revision.
4. **C5 – Integration, Regression, Evidence.** Outcome-Anzeige, WEB_UI-Angleichung
   (Werksreset nur lokal, Asset/Navigation/Seiten), ACCEPTANCE_TESTS, ROADMAP, Issue-Status,
   vollstaendige Offline-Ressourcenevidence, Builder-Self-Check; danach **ein**
   unabhaengiger Full Review ueber den gesamten PR (optionale gezielte Zwischenreviews nach C0
   und C2, kein Full Review je Commit). **C4 entfaellt (D7).**

Jeder Schnitt bleibt quell-/testcommit-gebunden und einzeln revertierbar. C0–C3 und die C5-Software sind im Kandidaten umgesetzt; vor Abschluss der C5-Ressourcenpruefung ist Q3 (§12) zu klaeren. Die bestehenden C0-/Q1-/Q2-Entscheidungen werden nicht erneut geoeffnet.

## 8. Gezielte Nachweise

| Bereich | Faelle |
|---|---|
| Echter extrahierter JS-Test (`scripts/test_web_shell_polling.mjs`, Fake-DOM erweitert) | bisherige 74 Faelle erhalten; je Slice: Navigation nur mit `LP`, Fragment gueltig/ungueltig, Sichtwechsel ohne Request, DE/EN/ES vollstaendig, `sl()`-Umrendern; Meldungen: Reihenfolge, 16 Eintraege, richtige `id`/Intent, Sperren bei `B`/`!P`/Stale/Offline/erledigt; Programme: Detail aus Beispielantwort, `null`/`startable:false` → Start gesperrt, Revisionsvergleich verwirft veraltete Auswahl, Anpassungen werden nur bei Abweichung gesendet, Bestaetigung an Eingaben gebunden; Einstellungen: Revisionsbindung, Zwei-Klick, kein Request ohne Bestaetigung, `nameEditable:false`; Mutationsgegenproben (fehlende `P`-Pruefung, falsche `id`, ungueltiges Fragment, Override ohne Bestaetigungsbindung) |
| Native `test_web_application_routes` | Grenztests je Shell/Asset (§4.2); Asset-Route (GET-only, 405, 404 fuer falsche/alte Version, Header nur beim exakten Pfad, byte-identisch in allen Auth-Zustaenden, kein Cookie); Programmdetail-Route (alle Fehlerfaelle der Tabelle, Identifier-Grenzen, Bodygrenze mit 96-B-Name, Revisionsbindung, Auth); Settings-Route; positiver Implementierungs-/Testumfang ausschliesslich Intent `set-device-name` (Codec-Grenzen, Replay-Budget, Outcome-Mapping-Tabelle, kein direkter Konfigurationszugriff im Handler, Laufsperre, Session bleibt nach Namensaenderung gueltig); `set-time-zone` ausschliesslich als unbekannter Intent im Negativtest: fail-closed `400`, kein Application-Aufruf und keine Mutation (Q2: Zeitzone nur lesbar, keine schreibende API, kein `applyTimeZone`) |
| Application/Konfiguration (C3) | **Dispatch/Gates:** Settings-Intents passieren dieselben realen Session-/CSRF-/Sequenz-/Digest-/Replay-Gates (jeder Gate-Fehler ohne Konfigurationsaufruf, gezaehlt am Application-Mock/Testzugriff); Replay (gleiche Sequenz + Digest) fuehrt zu **keiner** zweiten Mutation; unbekannte Intents, fehlende/ueberzaehlige Schluessel und unzulaessige Revisionen (fehlt, nicht dezimal, > Bereich) fail-closed `400`; kein Konfigurationszugriff im Handler (Architekturtest: Handler ruft nur `applyWebConfigurationChange`). **Outcome-Mapping:** Test durchlaeuft **alle** `ConfigurationPreviewStatus`- und `ConfigurationCommitStatus`-Werte gegen die Tabelle in §6.3 (Compile-Zeit-Vollstaendigkeit); `ConfigurationCommitIndeterminate` wird nie `Applied` und nie als sicher fehlgeschrieben (`WriteFailed`) ausgewiesen; Preview-Fehler ignoriert den Commit-Platzhalter. **Herkunft:** erfolgreicher Web-Namenscommit persistiert `{WebInterface, 3U}` (Manifest ueber den Store gelesen und mit `decodeChangeOrigin` geprueft); Touch-Namenscommit (`applyUserSettings`) bleibt `{LocalDisplay, 2U}`; ein `stale`/abgelehnter Web-Commit (und `NoChange`) schreibt **kein** neues Manifest (Store-Generation unveraendert); die Herkunft ist nicht aus Anfrage-Feldern ableitbar. **Gate-Gegenproben:** abweichender Payload bei derselben Sequenz wird verworfen; unbekannter Intent oder fehlendes `r.u` fuehrt zu keiner Mutation (kein Application-Aufruf). **C3.1:** Namensaenderung ohne Netzwerkaufrufe (Charakterisierung, Q1); Zone nur lesbar (kein Zonen-Intent im Codec: `set-time-zone` wird als unbekannter Intent abgelehnt); bestehende `applyUserSettings`-Tests unveraendert |
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

## 10. Offene Ownerfragen

**Q3:** Freigabe dieser exakten Planrevision fuer die begrenzte Stackkorrektur
nach §12. D1–D9 und Q1/Q2 bleiben unveraendert; D9 benoetigt weiterhin eine
getrennte Hardware-/Flash-/Browserfreigabe. Kein Ready-/Merge-/Release-GO.

## 11. Risiken

- Zwei Requests beim Erstaufruf erhoehen kurzzeitig HTTPD-/WLAN-Last; nur per D9 messbar.
- Globale `const`/`let` des Inline-Skripts (`S`, `A`, `U`, `Q`) muessen im nachfolgenden
  klassischen Script sichtbar sein; Test mit zwei getrennten `vm`-Skripten.
- Veralteter Tab nach Firmwareupdate: alte Asset-URL → 404; Hinweis, Neuladen genuegt.
- Asset-Wachstum: Limit 16 384 B; danach seitenweise Assets (eigener Plan), keine stille
  Erhoehung.
- Einstellungen sind die erste neue Mutationsfamilie ausser dem Lauf und teilen sich Session-/Replay-Mechanik mit der
  Run-Familie (geschlossener Dispatch, kein zweiter Mechanismus); Revisionsschutz, Replay-Budget,
  Laufsperre, vollstaendige Ergebnisabbildung (insbesondere `ConfigurationCommitIndeterminate`) und die
  persistierte Herkunft `WebInterface` muessen im Test belegt sein.
- Matrixzeilen H/X bleiben offen; R1-U/D9/G2 und Aktorfreigabe unveraendert offen.

## 12. HTTPD-Settings-Stackbefund und vorgeschlagene Korrektur (Q3, NICHT FREIGEGEBEN)

### Nachgewiesene Abweichung

Der HTTPD-Task hat unveraendert 8192 B. Im synchronen normalen Namenscommit
sind bereits diese fuenf Frames gleichzeitig aktiv: Adapter `handleRequest`
704 B, gemeinsamer Mutationshandler 544 B, `commitConfigurationChange` 272 B,
`ConfigurationService::confirmPreview` 3728 B und
`ConfigurationGraphStore::prepareCommit` 3280 B. Teil-Summe **8528 B**, also
336 B ueber dem gesamten Taskstack. Application-/Dispatcher-/SDK-/NVS-Frames
sind darin noch nicht enthalten. Messquelle: GCC-15.2-`-fstack-usage` beider
Profile auf sauberem Source-Commit `4f6f0c9`; Adapter mit dessen unveraendertem
Compile-Befehl und zusaetzlichen Analyseflags. Der Run-Zweig ist bereits in
`applyRunMutation` getrennt (2048 B); er ist nicht Teil dieser Settings-Summe.
Dies ist ein echter Ressourcenblocker, kein D2-Schaetzungswarnsignal.

### Begrenzter vorgeschlagener Scope

Die Empfehlung ist eine feste, transiente Heap-Scratch-Ablage im bestehenden
Konfigurations-Commit-Pfad. Der 8192-B-HTTPD-Stack, die bestehenden F4-Schwellen,
Konfigurationsbudgets, Persistenz-/Wireformate und App-/Web-Vertraege bleiben
unveraendert. Kein Worker, keine zweite Queue, keine neue Mutationsroute.
Betroffen sind `configuration_service.*`, `configuration_graph_store.*` und
deren direkte Tests; die bestehenden Settings-/Touch-Verbraucher bleiben.

1. Die grossen bisherigen Commit-Vorbereitungsobjekte werden als **ein**
   fest typisierter, pro Commit begrenzter Scratch-Owner gehalten. Bestehende
   Typen `LoadedConfigurationGraph`, `PreparedConfigurationCommit` und
   `ConfigurationCommitPrepareResult` werden wiederverwendet; es entsteht
   kein zweites Transaktions- oder Persistenzmodell. Die konkrete Xtensa-
   `sizeof`-Groesse und der maximale gleichzeitig lebende Zusatzbedarf werden
   vor der Freigabebewertung ausgewiesen; kein stilles Erhoehen eines Budgets.
2. `ConfigurationGraphStore` erhaelt einen typisierten In-place-Eintritt fuer
   diese Scratch-Ablage. Er schreibt den vorhandenen Ergebnis-/Prepared-Typ
   direkt in die Ablage und erzeugt keine gleich grosse Rueckgabe-Temporary
   auf dem Stack. Der bestehende Return-by-value-Eintritt bleibt kompatibel
   und delegiert an dieselbe Implementierung. Validierung, Slotwahl, Bytes,
   CRC, Herkunft, Generationen, Schreibreihenfolge und Fehlersemantik bleiben
   an genau einer Stelle.
3. `ConfigurationService::confirmPreview` nutzt die Ablage fuer die grossen
   laufenden Vorbereitungswerte. Bereits vorhandene `ResolutionContext`-/
   Epoch-/Lease-Vertraege werden weiterverwendet; bei Indeterminate geht der
   Zustand ohne grosse Kopie in den vorhandenen Resolution-Owner ueber.
   Keine Aenderung der Aktivierungs-/Fail-closed-/Ruecklesepolicy.
4. Allokation fehlertolerant und vor dem ersten persistenten Schreibschritt;
   kein neuer manifestierter Zustand bei Scratch-OOM. Ein fehlender
   temporaerer Scratch-Slot liefert den bestehenden Busy-Status und gibt
   Preview-/Mutationseigentum korrekt frei. NoChange, stale und abgelehnte
   Kandidaten behalten ihre bisherigen Ergebnisse und schreiben nichts.
5. Nach der Aenderung werden die **vollstaendige** Settings-HTTPD-Kette und
   der bestehende Run-Pfad gegen die unveraenderten 8192 B geprueft, einschliesslich
   aufgerufener Validierungs-/Store-/SDK-Pfade. Einzelne verkleinerte Frames
   sind kein Stack-PASS. Ist die Grenze oder ausreichende belegte Reserve
   weiterhin nicht nachweisbar, wieder STOP und Ownerbefund; keine automatische
   Stack-/Heap-/Scopeerweiterung. Die reale HWM-/Heapqualifikation bleibt D9.

### Gezielte Nachweise und Abschluss

- Fehler vor Scratch-Allokation/bei OOM: kein persistenter Schreibaufruf,
  keine neue Generation, keine verlorenen Preview-/Mutationleases.
- Bestehende Konfigurations-/Graph-Store-Goldenbytes, Slot-/CRC-/Root-/
  Indeterminate- und Epochenfehlerfaelle bleiben gruen; bestehender und
  In-place-Eintritt liefern dieselben typisierten Ergebnisse.
- Alle bisherigen Settings-Gates, Replay, Herkunft Web/Touch, NoChange/stale,
  Netzwerkwirkung und der unveraenderte Touch-Pfad werden direkt mitgeprueft.
- Host-Allokationsfenster mit maximalem zulaessigem Katalog/Name; neue
  Scratch-Spitzen, statische Frames und beide ESP-Profile auf sauberem
  Implementierungscommit. Keine Umdeutung zu RAM-/Hardware-PASS.
- C5-Evidence/ROADMAP/Issue/PR aktualisieren, Builder-Self-Check auf finalem
  HEAD; danach fuer den unabhaengigen Full Review anhalten. Pre-Ready,
  Ready/merge und D9 bleiben eigenstaendige Owner-Gates.

**Anhalten:** Dieser zusaetzliche Scope beginnt erst nach ausdruecklicher
Ownerfreigabe der exakten SHA dieser vollstaendigen Planrevision. Bis dahin
bleibt der aktuelle Implementierungskandidat Draft/HOLD.
