# Issue #27 – R1-Web-Restumfang (integrierter Gesamtplan, ein Draft-PR)

**2026-10-23 · PLAN-REVISION 2.4 (gezieltes Run-Stack-Diagnose-/Plandelta §13, Ressourcen-Refactor ergaenzt) · D1–D9/Q1/Q2/Q3 UNVERAENDERT · Q4 NICHT FREIGEGEBEN · PRODUKTARBEIT WEITER HOLD.**
[Issue #27](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/issues/27)
bleibt OPEN. Branch `agent/issue-27-r1-web-navigation-plan` (Name historisch), Draft-PR
[#209](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/pull/209). Dieser Plan ist
die einzige kanonische, vollstaendige Fassung. Historisch und nicht mehr massgeblich:
Revision 1 (Plan-SHA `4927e856df8cb3474a7b0ed74aa92917dd23cc6b`), Revision 2 (Plan-SHA `d940c2db329151c4a55a63385cce6bd18473e6f7`) und Revision 2.1 (Plan-SHA `7c3bbae6fd29bbe1679c39929f086c2c92e76bf0`, beide ersetzt durch diese Fassung) und der fruehere reine
Navigationsplan (`0614c054e3303ef2aacd495b8221e512eb4c0714`); deren Messbelege bleiben
unter `audits/WEB_NAV_BUDGET_20261020/`. Die Ownerentscheide unten allein waren keine Implementierungsfreigabe; die
explizite Q3-Freigabe der exakten Plan-SHA `1b00baea77a6168f29172ffa91852e10fdfd0c7e`
wurde nach Review erteilt und am 2026-10-22 vom Owner nochmals bestaetigt.

**Freigegebene Implementierungsbaseline:** Revision 2.2 auf
`b7d5e2dbc1f5281c764d4a75f2c37f1c4b6397c2` wurde am 2026-10-21 einschliesslich
C0 → C1 → C2 → C3 → C5 vom Owner freigegeben. Der Softwarekandidat liegt auf
`4f6f0c9abad9e1c354fc67e140302adcf62680e9`; funktionale Tests und beide Profilbuilds
sind PASS, das neue Settings-HTTPD-Stackgate ist **FAIL / HOLD** (§12).
Die begrenzte Q3-Korrektur wurde auf `095f393f0eed4eb95944c164a8e967ce973c3fc9`
umgesetzt; [Ergebnis und neuer Stackbefund](../audits/PR209_Q3_IMPLEMENTATION_20261022.md).
Die Freigabe erlaubt keine Budgeterhoehung und keine weitere Produktkorrektur
ausserhalb von §12. Der unveraenderte Run-Pfad verletzt das Stackgate; HOLD bleibt.

**Neues Diagnose-/Plandelta:** §13 untersucht ausschliesslich den verbliebenen
HTTPD-Run-Stack. Gepruefter HEAD `84fa6696559ef89f9e46afb0157f818c92365cf8`,
Produkt `095f393f0eed4eb95944c164a8e967ce973c3fc9`; Q3 bleibt unveraendert.
Revision 2.4 ist keine zusaetzliche Produktfreigabe und kein neuer Gesamtplan.

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

D1–D9 und Q1/Q2 bleiben entschieden; es gibt keinen Q1-/Q2-HOLD.
**Q3 ist auf `1b00baea77a6168f29172ffa91852e10fdfd0c7e` freigegeben und begrenzt
umgesetzt (§12).** Das Vollketten-Stackgate bleibt FAIL/HOLD; D9 bleibt separat freizugeben.

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

## 12. HTTPD-Settings-Stackbefund und begrenzte Korrektur (Q3)

**Status 2026-10-22:** Q3 auf Plan-SHA `1b00baea77a6168f29172ffa91852e10fdfd0c7e`
freigegeben; begrenzte Umsetzung auf `095f393f0eed4eb95944c164a8e967ce973c3fc9`.
[Implementierungsevidence](../audits/PR209_Q3_IMPLEMENTATION_20261022.md): 1856-B-
Scratch, Alignment 8 B, Handle 4 B bestaetigt; Stack-Teilwerte verbessert,
Vollketten-8192-B-Gate FAIL wegen unveraendertem Run-Frame 10880 B.
Weitere Produktarbeit STOP, kein Run-Fix und keine Budgeterhoehung freigegeben.
Der folgende Vorschlags-/Analysevertrag ist die autorisierte Grundlage, kein
nachtraeglicher Ressourcen-PASS.

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
   kein zweites Transaktions- oder Persistenzmodell. Die unten gemessene
   Xtensa-Groesse und Lebensdauerbilanz begrenzen diesen Vorschlag;
   kein stilles Erhoehen eines Budgets.
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

### Quantitative Entscheidgrundlage vor Q3 (nur Analyse, 2026-10-21)

[Messverfahren, Rohwerte und Lebensdauerbilanz](../audits/PR209_SCRATCH_ANALYSIS_20261021.md)
beziehen sich auf Produktquellen `3816450f87aabc0a0ed3e00aa606cf4ab6644718`.
Gemessen mit den tatsaechlichen Compile-Befehlen beider ESP32-Profile,
Xtensa GCC 15.2.0 / `esp-15.2.0_20251204`, ESP-IDF v6.1, GNU++17,
Exceptions/RTTI aus; ausschliesslich temporaere Analyseobjekte, kein Firmwarebuild.

| Typ | `sizeof` B | `alignof` B (beide Profile) |
|---|---:|---:|
| `LoadedConfigurationGraph` | 552 | 8 |
| `PreparedConfigurationCommit` | 1288 | 8 |
| `ConfigurationCommitPrepareResult` | 1304 | 8 |
| Vorgesehener `ProposedCommitScratch` | **1856** | **8** |

Das analytische Ownerlayout ist genau `{ LoadedConfigurationGraph current;
ConfigurationCommitPrepareResult prepared; }`; der `PreparedConfigurationCommit`
ist im Optional des Ergebnisses bereits enthalten. Ein `unique_ptr`-Handle
belegt 4 B ausserhalb des Heapobjekts, ohne eigenen Kontrollblock. Die In-place-
Vorbereitung baut `prepared.prepared`/`newGraph` direkt dort auf; kein zweiter
Scratch, keine zusaetzliche Prepared-/Graph-Heapkopie. Vorhandene String-/
Modellallokationen werden mit gleicher Lebensdauer weiterverwendet bzw.
verschoben. Auch die Uebergabe an `ResolutionContext` erzeugt keine grosse
Stack-Aggregattemporary. Diese Layout-/Lebensdauerannahmen sind Grenzen des
Vorschlags und nach Autorisierung am Implementierungsdiff zu verifizieren.

Maximaler **zusaetzlicher angeforderter Heap: 1856 B pro Commit**, auch bei der
Uebergabe. Gleichzeitig leben dort Scratch 1856 B + vorhandener Publish-Graph
552 B + vorhandener `ResolutionContext` 1512 B = **3920 B Objektablagen**,
zuzueglich der bereits im Ist-Pfad vorhandenen Runtime-/String-/Modellpuffer.
Die letzten 2064 B sind bereits Ist-Heapbedarf, kein weiterer Scratch-Zuwachs.
Fuer den gepinnten TLSF-Pfad ohne Poisoning/Tracing/Tasktracking ergibt sich
einschliesslich Alignment, Groessenklassen und unsplittbarem Rest eine
konservative Obergrenze von **1936 B zusaetzlicher Allocator-Belegung**;
Herleitung und Unterschied zur erforderlichen zusammenhaengenden Luecke siehe
Evidence. Keine neue Heapreserve und keine Erfolgsgarantie fuer die Allokation.

Bei Scratch-OOM: 0 B erfolgreicher Zusatz, bestehendes Busy-Ergebnis ohne
Schreiben; vor Allokation abgewiesene/NoChange/stale-Faelle ebenfalls 0 B.
Bei spaeteren normalen Prepare-/Runtime-/Ausfuehrungsfehlern hoechstens ein
Scratch bis zum Ruecklauf, danach 0 B. Bei `ConfigurationCommitIndeterminate`
oder dem bestehenden aufbewahrten RuntimeFailure-Pfad wird nur der bestehende
Resolution-Owner uebertragen: maximal 1856 B waehrend der Uebergabe, **0 B
zusaetzlich nach Rueckkehr**. Nachgelagerte OOMs bestehender String-/Modell-/
Resolution-Allokationen sind dadurch nicht allgemein beherrscht oder bewiesen.

Historisches H-RAM-1 (anderes Overlayimage M): Heapminimum 19896 B, beobachteter
kleinster API-/Shell-Block 26624/22528 B. Rechnerisch entsprechen 1936 B rund
9,7 % jenes Heapminimums; eine Subtraktion ergibt 17960 B, **keine aktuelle
Restheap-Prognose**. F4 bleibt PROVISIONAL (Skip <13312 B, Wiederaufnahme
ab 16640 B), prueft groesste Bloecke und reserviert nichts fuer diesen Commit.
TOCTOU, Fragmentierung und gleichzeitige WLAN-/HTTP-/LVGL-Last bleiben offen;
historisches F1+F4-Stress-OOM bleibt relevant. Offline nicht bewiesen sind
aktuelle Maximalspitze, Allokationserfolg, F4-Wirkung, reale Heap-/HWM-Reserve
und die korrigierte vollstaendige Stackkette. **RAM_PASS=NOT_PROVEN**.

**Begrenzte Empfehlung:** Q3 kann ausschliesslich einen Implementierungsversuch
dieses quantifizierten Ein-Scratch-Deltas mit den unveraenderten Nachweisen
autorisieren. Die Zahlen erteilen keine Ressourcen-/Hardwarefreigabe; der
8528-B-Stackbefund bleibt FAIL/HOLD. Mehr Felder/Allokationen, laengere
Pufferueberlappung oder veraenderte Besitzuebergabe erfordern eine erneuerte
Bilanz vor Bewertung, keine stille Budgeterhoehung. Commit-/Recovery-/
Golden-Byte-Vertraege und das Vollketten-8192-B-Stackgate oben bleiben bindend.

### Gezielte Nachweise und Abschluss (erst nach Q3-Autorisierung)

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

**Anhalten:** Die freigegebene Q3-Umsetzung ist begrenzt abgeschlossen; wegen
des weiter verletzten Vollketten-Stackgates bleibt der Kandidat Draft/HOLD.
Weitere Produktkorrekturen benoetigen einen neuen expliziten Ownerentscheid;
Independent Full Review, Pre-Ready und D9 bleiben getrennte Gates.


## 13. HTTPD-Run-Stackdiagnose und minimaler Korrekturkandidat (Q4, NICHT FREIGEGEBEN)

### Diagnose und Quellen

[Run-Stack-Evidence](../audits/PR209_RUN_STACK_DIAGNOSIS_20261022.md) mit
[Frames](../audits/PR209_RUN_FRAMES_20261022.tsv),
[ABI](../audits/PR209_RUN_ABI_20261022.tsv) und
[Kanten](../audits/PR209_RUN_EDGES_20261022.tsv), beide tatsaechlichen Xtensa-
Profile auf Produkt095f393. Keine Produktdateien oder produktiven Tests
geaendert; nur isolierte Compilerproben, keine neue Firmware/Regressionen.
Gezielte Quellen: `run_commands.*`, `fermentation_ui_commands.*`,
`fermentation_application.*`, `run_persistence_coordinator.*`, Web-Codec/
-Routen/Replay; kanonisch `RUN_COMMANDS.md`, `RUN_PERSISTENCE.md`, `WEB_UI.md`,
ADR-013, bestehende Direct-Consumer-Tests. Kein allgemeiner C0–C5-Review.

- Application-Frame10880 B: lokale `CommandDecision`10104 B, davon zwei
  `RunCommandState` zu je4896 B. UI-/Persistenzresultate208/112 B und Referenz
  auf Prepared-Request sind nicht die Hauptursache. GCC ueberlagert kleine
  Rueckgabe-/Lease-Temporaries; keine einfache Summe aller Typgroessen.
- Tatsachlich erreichbare direkte Kanten in beiden `.ci`: Web-handler544 →
  applyRunMutation2048 → applyConfirmedPrepared10880 B = **13472 B Untergrenze**
  gegen8192 B. Bestaetigter Web-Program-Start erreicht weiter Visitor20240 B
  (lokale Decision plus Value-Return-Temporary) und StartInto8000 B (u.a.
  weiterer4896-B-Kandidat und2824-B-ActiveRun-Optional). Daraus mindestens
  **41712 B**, ohne weitere Wrapper/SDK-/indirekte Unteraufrufe.
- Prepare/Confirm, Decision und anschliessende Persistenz sind getrennte
  Phasen. Coordinator nutzt schon den bestehenden12752-B-WorkingSet-Owner;
  kein neuer Heapblock desselben Typs erforderlich. SDK/NVS/indirekte Ketten
  bleiben ohne belastbare Aufloesung **NOT_PROVEN**, nicht0 B oder PASS.
- Q3-Settings-Teilframes2928 B und Config-Scratch1856/8 B (+1936 B konservative
  Allocatorbelegung) unveraendert. Settings-Vollkette weiterhin NOT_PROVEN.

### Bevorzugte Richtung, nur gemeinsam sinnvoll

**Root-only-Fix verworfen. SAFE_SMALL_FIX=NOT_PROVEN; IMPLEMENTATION_READY=NO.**
Kleinste hier untersuchte plausible zusammenhaengende Richtung:

1. Ein transienter, aligned/nothrow-Owner fuer den bestehenden `CommandDecision`
   innerhalb Application, vor Entscheidungs-/Apply-/Persistenzarbeit. Guard,
   bestaetigter Request und fruehe Kontextpruefungen bleiben. Keine Queue,
   Worker, zweiter Persistenzpfad, neuer Domainzustand oder Budgeterhoehung.
2. Echter non-null In-place-Eintritt in der bestehenden UI-Bridge; Application
   ruft ihn direkt auf. Vorhandene Into-Starts wiederverwenden, fuer die
   uebrigen erreichbaren Command-Varianten denselben Algorithmus in Into-
   Kernen fuehren. Bestehende Value-/Nullable-APIs bleiben kompatibel und
   delegieren; kein lokaler grosser Fallback im aufgerufenen Owning-Pfad.
3. Zusaetzliche Run-State-Kandidaten nachweisbar in bereits vorhandenen
   `decision.after`-Storage aufbauen. Auf jedem abgelehnten/unbestaetigten
   Ruecklauf denselben kompletten Decisionwert wie bisher herstellen;
   keine Selbst-Move-Zuweisung, keine Mutation des echten Current. Kann diese
   Aequivalenz nicht bewiesen werden, STOP statt stillem Semantikwechsel.
4. Betroffen: `fermentation_application.*`, `fermentation_ui_commands.*`,
   `run_commands.*` und direkte Tests. Run-Persistence/Store/Wire bleiben
   unveraendert. Weitergehende noetige Korrekturen nicht vorab autorisiert.

Compile-only-Proben, jeweils beide Profile: Application10880 →784 B;
Start-/Manual-Visitor20240 →32 B; weitere Visitorvarianten behalten10144 B,
solange Value-Returns existieren. Start-Kandidat-Alias mit Restore und ohne
Selbst-Move: Program8000 →3104 B, Manual5216 →320 B. Kein Link-/Semantiktest;
Proben sind keine Produktimplementierung. Schon nur fuenf kuenftige
Program-Start-Teilframes ergaben6512 B: **maximal1680 B arithmetischer Rest**,
keine begruendete Reserve. Alle Webvarianten und die Vollkette fehlen noch.

### Heap, Lebensdauer und offener OOM-Vertrag

Ein neuer Decision-Owner: **10104 B Request, Alignment8, Handle4 B**;
konservative TLSF-Belegung **10256 B**, zusammenhaengende Suche bis10240 B
(Details in Evidence). Gegen Produkt095f393 maximal ein solcher fixer Block,
ueber Decision/Persistenz/Readback bis zum Ruecklauf. Bei frueher Ablehnung
vor Allokation/Scratch-OOM0 B erfolgreich; nach Ablehnung/Erfolg/Indeterminate
kein gehaltener zusaetzlicher Decisionowner. Bestehende Unterpuffer/WorkingSets
bleiben; keine weiteren Deepcopies oder verlaengerten Ueberlappungen erlaubt.
Ein absoluter Unterpuffer-Peak bzw. Allokationserfolg ist **NOT_PROVEN**.

Nicht globale Exklusivitaet mit allen Q3-/Recoveryclients behaupten: bei
Ueberlappung Q3+Q4 konservativ11960 B Requests /12192 B Allocatorbelegung gegen
Vor-Q3. Historisches H-RAM-1-Minimum19896 B (anderes Image, aktiver Run offen)
belegt keine aktuelle Reserve;10256 B sind bereits51,5 % davon. F4 ist keine
Reservierung; WLAN/HTTP/LVGL, TOCTOU und Fragmentierung bleiben Risiken.
Permanente Vorallokation oder Wiederverwendung des noch benoetigten
Coordinator-WorkingSet sind kein kostenloser Ersatz.

OOM darf keine falsche Owningsituation behaupten. Explizite **noch zu
entscheidende** Minimaloption: internes
`FermentationUiDetailStatus::ResourceUnavailable`, KategorieUnavailable,
PhaseDecisionOnly, bestehendes Web-Unavailable-Wireoutcome. Kein vorgetaeuschter
Persistence-Busy/OwningOutcome, kein TooLarge durch CapacityReached. Die
HTTP-Sequenz ist bereits reserviert und wird wie bisher mit dem Ergebnis
abgeschlossen/gecached; identischer Replay startet keinen neuen Versuch.
Keine neue Sequenz-/CommandID-/Retryregel, kein Ruecksetzen erzeugter IDs.
Diese additive interne Detailoption ist selbst Teil des nicht freigegebenen
Scopes; ohne ehrlichen akzeptierten OOM-Vertrag keine Umsetzung.

### Gezielte spaetere Regressionen und Ressourcenabschluss

Erst nach gezieltem Planreview und ausdruecklicher Produktfreigabe:

- Domain-/Bridge-Aequivalenz fuer alle erreichbaren Varianten: bestaetigt/
  unbestaetigt, NoChange, stale, ungueltige Sensor-/Safety-/Revisions-/Kapazitaets-
  und Zustandsgrenzen; kompletter `before`/`after`-/Summary-/Effects-/Statuswert,
  nullable/value und Into gleich. Insbesondere Restore auf Ablehnung.
- Scratch-OOM vor Apply/Store: Current unveraendert, keine Storewrites/neue
  Persistenzrevision/processed oder persisted CommandID; Guard/Owner frei,
  kein Leak. Bereits bestehende Prepare-/ID-Vergabe nicht rueckgaengig machen.
  Neues ehrliches DecisionOnly/Unavailable auch lokal/Touch korrekt anzeigen.
- Web Session/CSRF/Origin/Revision/Confirmation unveraendert; OOM als genau ein
  abgeschlossenes Unavailable-Replay-Ergebnis. Identischer Replay ohne
  weitere Allokation/Storewrite, Digestkonflikt/stale Sequenz/In-flight-
  Sperre unveraendert; kein erfolgreicher Replay einer DecisionOnly-Proposal.
- Persistierte Resultate/AlreadyPersisted/AlreadyProcessed, TimeMismatch,
  WriteFailed/CommitOutcomeUnknown, Readback OLD/NEW/Indeterminate, Recovery,
  RAM-Apply-nach-Write, Ack/Mute RAM-only und Post-Commit-Apply-Failure behalten
  unveraenderten Outcome-/Safetyvertrag. Spaetere Puffer-OOMs separat pruefen;
  nicht durch den Scratch-OOM-Test als beherrscht darstellen.
- Gezielte bestehende Bereiche: `test_run_commands`,
  `test_fermentation_ui_commands`, `test_run_persistence_coordinator`,
  `test_web_application_routes`, direkt betroffene Touch-Verbraucher. Kein
  allgemeiner C0–C5-Neureview und keine Wiederholung unveraenderter Q3-Suites.
- Allokationsfenster mit maximal gueltigem Run/Programm/Katalog, Messages/
  Effekten, bestaetigtem Request und maximalen Datengroessen; neuer Ownerpeak,
  groesster zusammenhaengender Block und dynamische Unterpufferueberlappungen
  fuer Erfolg, Ablehnung, OOM, Writefehler und Indeterminate. Host/Ziel trennen.
- Beide Profile auf sauberem Implementierungscommit: fuer **alle** erreichbaren
  Run-/Settings-HTTPD-Phasen Taskeintritt/SDK/Adapter/Dispatcher/Porttargets/
  Validierung/Codecs/Persistenz/NVS erfassen. Nur gleichzeitig lebende Frames
  summieren, sret/Inlining/indirekte und begrenzte rekursive Pfade explizit
  aufloesen. Keine Teilframes oder fehlenden Targets als0 B buchen.
  Nicht abschliessbar begrenzte Ketten **NOT_PROVEN/HOLD**, Ueberschreitung
  **FAIL/HOLD**. Erst ein dokumentierter Maximalpfad S mit **8192−S** und
  technisch begruendeter, im gezielten Review akzeptierter Reserve erlaubt
  statischen Ressourcenabschluss. Positiver arithmetischer Rest allein nicht.
  Keine automatische Budgeterhoehung, kein Vorwegnehmen eines Reserve-PASS.
- D9/reale Heap-/HWM-/Hardwarequalifikation bleibt getrennt und nicht durch
  statische Summen ersetzt. Boot/Reset/Fehler/unklare Hardware bleiben ohne
  Aktorfreigabe. Bei zusaetzlichem Ressourcen-/Semantikscope STOP/Neubilanz.

### Owner-Entscheidvorlage / Halt

Empfehlung: keine Root-only-Produktfreigabe. Die zusammenhaengende Richtung ist
konkret quantifiziert, aber noch nicht als kleine sichere Gesamtloesung belegt.
Vor Produktfreigabe gezielt entscheiden/nachweisen: kompletter Variantenscope,
OOM-Detailvertrag, dynamischer Heappeak/Blockbedarf und Vollkettenreserve.
Ein Scheitern fuehrt nicht automatisch zu Queue/Worker/groesserem Stack.

**Zusaetzliche Produktimplementierung NICHT FREIGEGEBEN.** Diese Revision ist
nur Diagnose-/Planergebnis; Q3 bleibt auf freigegebener1b00bae-Basis umgesetzt.
HTTPD-Vollkettengate FAIL, Ressourcen-HOLD, RAM_PASS=NOT_PROVEN,
ACTUATOR_RELEASE=NO. Exakte neue Plan-SHA in PR/Handover; danach anhalten.


### Q4-Machbarkeitsdelta zur kombinierten Probe (2026-10-22)

**Q4_FEASIBILITY_HOLD.** Basis47b782e, keine weitere Produktfreigabe.
[Neue Evidence](../audits/PR209_Q4_FEASIBILITY_20261022.md),
[einziger gemeinsamer temporaerer Patch](../audits/PR209_Q4_COMBINED_20261022.patch)
und rohe gemeinsame Frames/Phasenpfade sind versioniert. A/B/C wurden in
**einer** temporaeren Sourcekopie mit gemeinsamen Headern zusammengefuehrt;
Application/Bridge/Commands/Web jeweils mit beiden originalen Xtensa-
Profilbefehlen uebersetzt. Kein Link, Produkt-/Testcode oder OOM-Enum geaendert.

Alle sieben Web-erreichbaren Visitorvarianten jetzt32 B; zugehoerige Into-
Frames: Program3104, Manual320, Stop240, Completion240, Adjustment400,
Ack/Mute32 B. Application784 B, Bridge48 B; **applyRunMutation bleibt2048 B**.
Die gemeinsame Program-Decision ruft die bestehende ActiveRun-Factory mit
weiteren3136 B auf: **544+2048+784+32+3104+3136 =9648 B**, schon1456 B ueber
8192 B, noch ohne Adapter/Dispatcher/Bridge/SDK-/Validierungspraefixe.
Gemessene gemeinsame Frames, keine Addition isolierter A/B/C-Gewinne.
Prepare, Confirm, Decision und Persistence/Readback sind getrennt bilanziert;
fehlende indirekte/SDK-/NVS-Kanten **NOT_PROVEN**, nicht0 B oder PASS.
Settings/Q3 unveraendert, kein Ressourcenabschluss.

Der2048-B-Webframe haelt prepared/confirmed (je856 B aus vorhandener Evidence)
und Context/UI-Return-Temporary. Eine lokale Scope-/const-Aenderung beweist
keine Framefreigabe. Selbst vollstaendiges hypothetisches Streichen dieses
Frames ergäbe mit dem Adapter **9648−2048+704 =8304 B >8192 B**;
diese Gegenrechnung ist kein gemessener Optimierungsgewinn.

Fixe10104-B-Decision-/10256-B-Allocator-Komponente nicht neu gemessen und
jetzt nicht empfohlen. Keine neue explizite normale Deepcopy gegenueber
Produkt095f393 erkennbar; dynamische Spitzen auf Restore-/Request-/ActiveRun-/
Serialisierungs-/Readback-Ueberlappung weiter NOT_PROVEN. Coordinator-
WorkingSet12752 B wird beim Persistieren neben der Decision gebraucht und
ist kein nachgewiesen freier/exklusiver Ersatzspeicher.

OOM-Option unveraendert zur Entscheidung: internes ResourceUnavailable,
DecisionOnly/Unavailable, bestehendes Web-unavailable und Replay; kein
Persistence-Busy, CapacityReached/TooLarge oder falsches OwningOutcome.
Keine Enum-/API-Implementierung dieses OOM-Vertrags in der Compileprobe.

**STOP / Owneroption:** gezielte weitere Diagnose der `run_snapshot.*`-
ActiveRun-Factory mit ihren Caller-/Value-/Optional-Temporaries und der
nahen2592/1680-B-Prepare-Requestkonstruktion beauftragen oder HOLD beibehalten.
Bevorzugt bestehendes Decision-after-Storage untersuchen, kein weiterer
Heapowner/WorkingSet, kein Budgetwechsel/Worker/Queue. Keine solche weitere
Variante oder Produktkorrektur aus diesem Auftrag. Q3/C0–C5 und D9 bleiben.

### Q4-Wiederverwendung: Ownership-Stop (2026-10-23)

**IMPLEMENTATION_READY=HOLD; OWNERSHIP_GATE=NOT_PROVEN.** Basis
`d29f76965c4f5e2c91ddd2199523de79630a152d`, Produkt095f393 unveraendert.
[Gezielte Evidence](../audits/PR209_Q4_WORKSPACE_20261023.md) ist der historische
Entscheidstand dieses ersten Auftrags; die obigen Heap-/A/B/C-Vorschlaege bleiben historische,
nicht freigegebene Proben und sind keine Default-Implementierungsrichtung.

Der vorhandene4896-B-Kandidat im12752-B-WorkingSet koennte grundsaetzlich
das einzige mutierbare after sein: Snapshot und Record liegen daneben,
keine pauschale Behauptung, der gesamte WorkingSet sei frei. Die private
Application-Instanz und ihr Serializer sind belegt, ein durchgehender
Ausleih-/Reentranzvertrag fuer alle Coordinator-Konsumenten dagegen nicht.
Busy beginnt heute erst nach Candidate-Apply. Rekursive Application-Mutation
kann nicht allein durch recursive_mutex ausgeschlossen werden. Ein
before-Referenzalias auf current macht die heutigen Stale-/Safety-Vergleiche
zu Selbstvergleichen; nur Revisionen zu sichern genuegt insbesondere fuer
Sensor-/Safety-Locks nicht. Kein beobachteter produktiver Race behauptet.

Damit greift das ausdrueckliche erste Stop-Kriterium des neuen Auftrags:
**COMBINED_REUSE_PROBE=NOT_RUN_OWNERSHIP_STOP**, keine ungesicherte gemeinsame
Aliasvariante kompiliert oder als Loesung ausgegeben. Originale Xtensa-.su
beider Profile erneut gelesen: Application10880, Web2048, Handler544 B;
Untergrenze13472 B bleibt FAIL. Prepare-Teilpfad mit bekanntem Adapter/
Dispatcher/handleRun7744 B hat nur448 B arithmetischen Rest vor weiteren
unbestimmten Unterpfaden. Settings-Teilpfad2928 B unveraendert, Vollketten
Run/Settings einschliesslich SDK/NVS NOT_PROVEN. Keine neuen View-/Factory-/
Requestframes oder positive Reserve bewiesen; gestrige Proben nicht addieren.

Realisierter Netto-RAM-Effekt gegen095f393: Stack0 B / fester Heap0 B /
dynamischer Produkt-RAM0 B Aenderung. Kein10104-B-Decision- oder zusaetzlicher
Runowner. Peak-/Lifetime-Bilanz in der Evidence; Unterpuffer, Copy-/Move-/
Rollback-/Serialisierungsueberlappung und groesster Block einer abgesicherten
Variante bleiben NOT_PROVEN, keine angenommene Heapgutschrift.

**Owneroption:** durchgehenden Coordinator-eigenen Besitzvertrag fuer den
vorhandenen Kandidaten plus kleinen unveraenderlichen Guard der exakten
applyRunCommand-Vergleichsfelder verbindlich abgrenzen; Value/View verwenden
denselben Entscheidungskern. Erst dann waere eine gemeinsame Probe von
Domain/Bridge/Application/Coordinator, In-place-ActiveRun und Web-Prepare/
Confirm mit allen Konsumenten sinnvoll. Post-Commit-Pruefung und
PersistenceCommittedApplyFailed bleiben erhalten; kein frueher RAM-Apply.
Das ist eine neue interne API-/Lebensdauergrenze, noch kein belegter kleiner
Framefix. Alternativ HOLD beibehalten; keine automatische Folgeoptimierung.
Alle bestehenden Semantik-/Regression-/OOM-/Vollkettenvertraege gelten weiter;
keine neuen Tests/Builds/Hardwarelaeufe ausgefuehrt. **Zusaetzliche
Produktimplementierung NICHT FREIGEGEBEN**, auch nicht durch diesen Plan-SHA.

### Q4-Ownership-Prototyp: Vollkettenreserve HOLD (2026-10-23)

**Entscheidung (b): IMPLEMENTATION_READY=HOLD / FULL_CHAIN_RESERVE=NOT_PROVEN.**
Basis285d8378633f4d7a05f7c691997df4deb94bd63f; Produkt095f393 unveraendert.
Der ausdruecklich beauftragte [gemeinsame Prototyp](../audits/PR209_Q4_OWNERSHIP_PROTOTYPE_20261023.md)
bearbeitet den vorigen Ownership-Stop: eine temporaere Sourcekopie, gemeinsame
Header, neun betroffene App-/Konsumenten-TUs je Profil sowie unveraenderte
SDK-/Adapter-TUs mit echten Xtensa-Profilbefehlen. Ein Patch/Quellmanifest,
gemeinsame Frames/ABI/Phasenpfade versioniert; kein Produkt-/Testcode geaendert.

Privater instanzlokaler Borrow vor erstem Candidate-Zugriff, nicht kopierbares
Token bis Commit/Apply/Abbruch; externe Mutationseingaenge dieses Coordinators
werden vorher abgewiesen, die eigene private Fortsetzung bleibt zulaessig.
Keine globalen Locks fuer unabhaengige Instanzen/Value-Konsumenten. Eingefrorener
Guard der exakten Apply-Felder216 B, View inklusive Guard536 B, Token4 B /
optional8 B; alte Persistenzrevisionen separat8 B. Coordinator21352 B bleibt
gleich gross, kein neuer fester Heaprequest/-owner oder zusaetzlicher voller
Runzustand. Gemeinsame Domain-/Apply-/Persistenzkerne; zweiter Post-Write-Check
und CommittedApplyFailed erhalten, kein vorzeitiger RAM-Apply. Verhalten und
Fehlerinjektionen **NOT_TESTED**, keine semantische Qualifikation der Probe.

Gemeinsam gemessen, beide Profile: Application1312 B, Web1200 B, Program-Kern288,
Manual320, Stop/Completion256, Adjustment416, Ack/Mute48 B; sieben Web-Visitoren32 B.
StartInto96 / direkter Konstruktor32 B; ProgramPrepare2064 und Requestfactory48 B,
In-place-Confirm48 B. Internal waechst720→768 B, Write/Core bleiben208/1168 B.
Keine Addition isolierter Gewinne; verbliebene Optional-/Validatorunterframes
sind enthalten. Voller bekannter HTTPD-Prefix inklusive Taskwrapper/Adapter/
Dispatcher/Handler1856 B. Berechnete Phasen-Teilketten: Prepare6016, Confirm4288,
Decision6144, Ack/Mute4816; Run-Validierungszweig mit normalem Allocatorpfad
**7856 B**, damit nur **336 B arithmetischer Rest** vor weiteren unaufgeloesten
Targets/Pfaden. Settings konservativer bekannter Teilpfad7616 B, ebenfalls
keine Obergrenze. **Kein begruendeter Reserve-/Ressourcen-PASS.** SDK/NVS/Flash,
weitere libc-/Alias-/OOM-Unterpfade bleiben gegebenenfalls NOT_PROVEN.

Fester Heapdelta gegen095f393 **0 B**, groesster neuer fester Block0 B.
Absolute/differentielle dynamische String-/Vektor-/Snapshot-/Readback-Spitzen
und groesster insgesamt notwendiger Block NOT_PROVEN; keine ungemessene
Move-Heapersparnis behauptet. D9/reale Heap-/HWM-Evidence bleibt getrennt.

**Eine Alternativentscheidung:** Owner kann einen zusammenhaengenden groesseren
Ressourcen-Refactor des bestehenden Run-Persistenz-/Validierungskerns als Scope
freigeben oder HOLD beibehalten. Gemessener Ansatzpunkt Internal768 + Write208
+ Core1168 B mit ihren Unterpfaden/Lebensdauern; bestehender Schreib-/Recovery-
Vertrag bleibt. Keine automatische weitere Factory-/Webframe-Mikroprobe,
Queue/Worker, grosser Heapowner oder Budgeterhoehung. Gezielte spaetere
Regressionen fuer Value/View, Borrow/Reentranz, Post-Write-Safety/Stale,
ID/Replay/Origin, Ack/Mute, OOM/Writefehler/Indeterminate/Recovery/Golden-Bytes
stehen in der Evidence; jetzt keine davon ausgefuehrt. OOM-Detail/Replay nur
Vorschlag, keine Produkt-Enum. **Weitere Produktimplementierung NICHT FREIGEGEBEN.**

### Q4-Ressourcen-Refactor: Planungsdelta (2026-10-23)

**IMPLEMENTATION_READY=HOLD; FULL_CHAIN_RESERVE=NOT_PROVEN.** Ausgangs-HEAD
`c41a8076e9bafd5ab9040ba16b58200150dddf7b`, Produkt/Firmware095f393 unveraendert.
Der Owner hat die Planung dieses Deltas autorisiert, **keine Produktumsetzung**.
[Kurze Ressourcenevidence samt einer gemeinsamen temporaeren Variante](../audits/PR209_Q4_RESOURCE_REFACTOR_20261023.md)
konkretisiert den vorigen Refactor-Ansatz. Fruehere Q4-Holds bleiben historische
Entscheidstaende; die aktuelle Entscheidung steht hier. Q3/C0–C5, Q1/Q2,
Budgets und der bestehende Write-/Recovery-/Golden-Byte-Vertrag bleiben.

**Eine Variante / kleinster zusammenhaengender Umsetzungsscope, derzeit gesperrt:**

1. **Kandidat und Entscheidung:** die im gemeinsamen Prototyp abgegrenzte
   private Application-/Coordinator-Borrow-Grenze uebernehmen, nicht auf
   unabhaengige Instanzen/Value-Konsumenten ausweiten. Application-Serializer
   plus nicht kopierbares instanzgebundenes Token vor erstem Candidate-Touch,
   bis Decision→Persistenz/Readback→zweitem Apply-Check→Uebernahme/Abbruch.
   Alle in der Ausgangsevidence benannten mutierenden Coordinator-Eingaenge
   und Application-Lifecycle-Eingaenge weisen fremden/zweiten Zugriff vorher
   ab; eigene private Fortsetzung akzeptiert dasselbe Token. Flag ist kein
   Mutex und erweitert bestehende Thread-/Lifecycle-Garantien nicht.
   `run_commands.*`, `fermentation_ui_commands.*`,
   `fermentation_application.*`, `run_persistence_coordinator.*` verwenden
   denselben Value-/View-Domain-/Apply-Kern. Guard216 B friert die **exakten**
   bisherigen Vergleichsfelder ein: Prozess/Transition, Command-/Run-/Message-/
   Faultrevision, kritisches Safety-Pending, Run-ID, Sensormodus, Sensorruntime/
   persistierte Auswahl und Recoveryepisode; Persistenzaltrevisionen separat8 B.
   Kein before=current-Selbstvergleich, kein zweiter Apply des Kandidaten auf
   sich selbst. Private validierte ID-Grenze48 B nicht auf ungepruefte externe
   Valuewerte uebertragen. Bestehende Value-API und Konsumenten bleiben erhalten.
2. **Factory/Web-Temporaries:** `run_snapshot.*` konstruiert ActiveRun direkt
   in candidate.activeProgramRun; Value-Factory delegiert denselben Kern.
   `web_application_routes.cpp` und Application-Prepare/Confirm halten einen
   identitaetsgebundenen RequestResult856 B und bestaetigen diesen in place;
   bestehende const Value-Bestaetigung kopiert weiter unter dem Serializer.
   Keine zweite Requestidentitaet oder bestaetigungsfreie Abkuerzung.
   Summary/Notice/Effects, Ablehnungen/Unconfirmed, Returnwerte, Timing aus dem
   bestaetigten Envelope, CommandID/Origin und Replay bleiben aequivalent.
3. **Bestehenden Persistenz-/Codec-Kern nach Lebensdauer trennen:** in
   `run_persistence_coordinator.*` unveraenderter Preflight, dann
   Recordvalidierung/Encoding, dann **derselbe** vollstaendige
   PreparedHead→Slot→CommittedHead-/Readback-Kern, danach Post-Commit-Check
   und Uebernahme. Payload in vorhandenen Envelope verschieben, Recordbytes
   direkt ins vorhandene WS-record encodieren; physisches OLD nach oldSlot
   verschieben, Prepared-Bytes als OLD der finalen Headphase behalten,
   Committed-Bytes erst nach Written uebernehmen. Fruehe Slotadoption fuer
   bestehende Fehlerzustaende erhalten; nur die zweite identische erfolgreiche
   Slotkopie entfaellt. Kein Snapshot-/Record-/OLD-Puffer vorzeitig freigeben
   oder zum Kandidatenspeicher erklaeren. Slots behalten ihre benoetigten
   Snapshot-/Recordkopien. Kein weiterer grosser Scratchowner.
   `program_model.*` extrahiert die **bestehenden**4→5→6-Migrationsschritte
   in einen gemeinsamen Into-Kern; `configuration_document_codec.cpp` nutzt
   ihn ausschliesslich am noch unveroeffentlichten Programmkandidaten und
   verwirft diesen bei Fehler. Die Value-Migration erhaelt Status und
   unveraenderten Eingabewert; Unsupported-Version vor Kopie abweisen.
   Keine zweite Validierung/Migrationsregel, Schemaaenderung oder Q3-Korrektur.
4. **Verifikation und Stop vor Abschluss:** erst nach spaeterer Freigabe
   dieses dann konkret geschlossenen Scopes gemeinsame Quellen mit beiden
   echten Xtensa-Profilen bauen. Semantik und Ressourcen getrennt pruefen;
   keine isolierten Framegewinne addieren und keine weitere Optimierungsserie
   bei Nichterreichen der unten genannten Grenze beginnen.

**Messstand / Ressourcenziel:** die eine Compile-only-Variante enthaelt1–3
zusammen. Internal bleibt768 B, Write208 B; Core1168→224 B, separater
Record-Prepare128 B, Transaction976 B. Im Storezweig bleiben damit224+976 B
statt1168 B gleichzeitig aktiv. Migration-Into/Schritte je32 B, ReadProgram
448→240 B; CatalogValidator1088 B und Factory-/Allocatorzweige bleiben.
Mit echtem HTTPD-Prefix1856 B: bekannte Run-Validierung7040 B (ausgerichteter
Allocatorzweig7072 B), Store/NVS/Flashzweig**7760 B**, Settings**6960 B**.
Prepare6016 / Confirm4288 / Decision6144 / Ack-Mute4816 B unveraendert zum
Ownership-Prototyp. Gemessene Frames plus berechnete **Teilketten**, keine
Vollkettenobergrenzen/HWM; SDK-/NVS-/ROM-/libc-/Alias-/OOM-/Cleanup-Ziele
weiter explizit NOT_PROVEN, einschliesslich Envelope-Unterpfaden.

Akzeptanzboden fuer **vollstaendig** begrenzte Run- und Settings-Ketten:
S_Run≤7680 B **und** S_Settings≤7680 B, entsprechend mindestens512 B Reserve
(6,25% des8192-B-Stacks). Planungspuffer zweimal so gross wie der jetzt
nach Aufloesen bekannter NVS-Read-Ziele sichtbare Zuwachs256 B (848→1104 B),
fuer Frame-/Callsite-Sensitivitaet nach Pfadabschluss, **keine**
Pauschalbuchung unbekannter Targets. Indirekte Ziele, Allocator-/Write-/Readback-/Fehler-/Destruktorpfade
und begrenzte Zyklen muessen in S enthalten sein; Review bestaetigt die
Begruendung, reale D9/HWM bleibt separat. **7760 B ergeben nur432 B Rest:
80 B unter dem Planungsboden schon vor offenen Targets. STOP.** Die
Phasentrennung allein traegt keine Implementierungsreife.

**RAM-/OOM-Grenze:** Coordinator21352 B einschliesslich WorkingSet12752 B,
separater Current4896 B und Q3-CommitScratch1856 B bleiben; kein neuer grosser
fester Heaprequest oder Block. View536 B inkl. Guard/Token bleiben Stackwerte.
Die Evidence bilanziert Request-/String-/Vektor-/Snapshot-/Record-/Head-/
Store-/Readback-/Cleanup-Ueberlappungen gegen095f393: entfernbare logische
Bytekopien sind keine gemessene Heapgutschrift. Gesamtheappeak, Allocator-
Overhead und groesster erforderlicher Block bleiben NOT_PROVEN, einschliesslich
bestehender bis32813-B-Settings-Readstrings plus NUL und Envelope-Payloadkopie.
Spaetere Differentialtraces muessen fuer dieselben Eingaben/Fehlerstellen
maximale Live-Allokatorbelegung, zusaetzliche Requests und groessten Request
belegen; kein hoeherer Gesamtpeak oder neuer groesserer Block ohne erneuten
Ownerentscheid, keine Kapazitaet aus F4/H-RAM-1 annehmen.

OOM-/Borrowablehnungsvorschlag: intern ResourceUnavailable,
DecisionOnly/Unavailable, bestehendes Web-unavailable samt identischem Replay;
getrennt von Persistence-Busy, CapacityReached und OwningOutcome. Sobald der
Owning-Persistenzpfad tatsaechlich betreten ist, dessen echte Durability/
Indeterminate-/CommittedApplyFailed-Ergebnisse behalten. Keine neue Enum jetzt.
Standardcontainer/ByteWriter unter -fno-exceptions sind nicht allgemein
OOM-fallibel; ein Enum ersetzt keinen Fehlerpfad. Vor weiterer Freigabe muss
jede betroffene Allokationsgrenze ehrlich behandelt oder als Stop ausgewiesen
werden. Kein Persistenzerfolg, Retry oder Identitaetsreset aus OOM ableiten.

**Spaetere gezielte Regressionen:** `test_run_commands`,
`test_fermentation_ui_commands`, `test_run_snapshots`,
`test_run_persistence_coordinator`, `test_web_application_routes` vergleichen
Value/View und direkte Web-/Touch-Konsumenten fuer alle sieben Webvarianten,
Unconfirmed/Rejected/NoChange/Stale/Safety/Overflow, Summary/Notices/Effects,
IDs/Origin/Replay; Ack/Mute bleiben RAM-only. Rekursive/zweite Mutation vor
Candidate-Touch abweisen, eigene Fortsetzung zulassen, fremdes Token sperren,
unabhaengige Instanz unbeeinflusst, Cleanup auf allen Rueckwegen. Sensor-/
Safety-/Stale-Aenderung vor und **nach durablem Write** injizieren:
Originalzustand vorher unveraendert, danach weiterhin
PersistenceCommittedApplyFailed bei ungueltiger Basis; keine Aktorfreigabe.
OLD/NEW/fehlendes/abweichendes Readback, jeder Head-/Slot-Writefehler,
Indeterminate/Recovery, Trusted-UTC und Golden-Bytes behalten.
`test_program_models`, `test_configuration_documents`,
`test_configuration_graph_codecs` pruefen Value/Into4→5→6 einschliesslich
ungueltiger Eingaben/Teilfehler; direkt betroffene Settings-/Graph-Store-
Regressionen nur fuer dieses Delta. Allokations-/OOM-Injektionen an Candidate,
Request, Codec, Store und Cleanup samt ehrlichem Outcome/Replay planen.
Statische Vollketten, Native-/Golden-Tests, dynamische Heaptraces und spaetere
reale HWM-/Hardwareevidence sind verschiedene Gates; jetzt keine Tests davon
als bestanden buchen, keine allgemeinen C0–C5-/Q3-Laeufe wiederholen.

**Eine Ownerentscheidung / Halt:** Als einzige naechste technische Option den
Scope um eine phasenweise Nutzung **eines** Head-Valueobjekts statt gleichzeitig
Prepared und Committed begrenzen. Beide kodierten Headbytes muessen vor dem
ersten Write bereitstehen; OLD/NEW/Readback und Recovery bleiben derselbe
Kern. Der dominante Transactionframe976 B ist der gemessene Ansatzpunkt,
keine zugesagte Einsparung. Ohne diese explizite Scopeentscheidung bleibt
HOLD; hier keine weitere Probe automatisch beginnen. **Zusaetzliche
Produktimplementierung NICHT FREIGEGEBEN**, weder durch die Planung noch
durch deren neue exakte Commit-SHA. Danach anhalten.
