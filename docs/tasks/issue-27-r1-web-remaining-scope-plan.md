# Issue #27 – R1-Web-Restumfang (integrierter Gesamtplan, ein Draft-PR)

**2026-10-23 · PLAN-REVISION 3.3 mit Implementierungsnachtrag §13, Korrekturdelta §14 (FREIGEGEBEN `05f8d74`, UMGESETZT) und Ressourcenqualifikation §15 (M-H1 ABGESCHLOSSEN/GO, H-RAM-2 DURCHGEFUEHRT/PARTIAL, UNABHAENGIGER REVIEW DER ERGEBNISSE OFFEN) · D1–D9/Q1/Q2/Q3 UNVERAENDERT · Q4 OPTION A auf Plan-SHA `623bc4520bcf3601d3666e7c717ae00213047901` FREIGEGEBEN UND UMGESETZT · RESSOURCENABSCHLUSS HOLD.**
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
C0 → C1 → C2 → C3 → C5 vom Owner freigegeben. Der damalige Softwarekandidat liegt auf
`4f6f0c9abad9e1c354fc67e140302adcf62680e9`; funktionale Tests und beide Profilbuilds
waren PASS, das damalige Settings-HTTPD-Stackgate war **FAIL / HOLD** (§12).
Die begrenzte Q3-Korrektur wurde auf `095f393f0eed4eb95944c164a8e967ce973c3fc9`
umgesetzt; [Ergebnis und neuer Stackbefund](../audits/PR209_Q3_IMPLEMENTATION_20261022.md).
Die damalige Q3-Freigabe erlaubte keine Budgeterhoehung oder weitere Produktkorrektur
ausserhalb von §12. Der damalige Run-Pfad verletzte das Stackgate. Q4 Option A
ist jetzt separat freigegeben und auf `e31e952def3264ca419187b86a2988bd20e3a487`
umgesetzt; der Ressourcenabschluss bleibt HOLD (§13).

**Q4-Implementierungsnachtrag:** §13 dokumentiert die freigegebene Option A
und ihre tatsaechlichen Produkt-/Test-/Profil-/Ressourcenergebnisse.
Die vorherigen Diagnose-/Prototypstaende sind historische Belege. Q3 bleibt
unveraendert; keine Budget-, Hardware-, Ressourcen- oder Mergefreigabe.

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
(§0); die aktuelle Vollketten-Ressourcenqualifikation ist FAIL/HOLD (§13). (4) Keine Hardware-, Flash-, Browser-, UART-, Reset- oder Aktorfreigabe;
`ACTUATOR_RELEASE=NO`, `RAM_PASS=NOT_PROVEN`.

## 1. Live-Basis

Aktuelles Produkt: `e31e952def3264ca419187b86a2988bd20e3a487` (Q4 Option A).
696 gezielte native Tests, beide echten ESP32-Profile und Builder-Self-Check
PASS; Vollkettenstack FAIL (Web-erreichbarer Value-Fallback), ESP32-Heap/OOM-Abschluss
NOT_PROVEN, D9 NOT_RUN.
Die nachfolgenden C0–C5-/Q3-Staende bleiben historische Baselines.

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

**Q4-Delta (§14):** Freigabe des Korrekturdeltas fuer die zwei Web-erreichbaren
Value-Intents `recovery-time-correction` und `sensor-selection` auf der exakten
Plan-SHA dieser Revision. Bis dahin keine Produktaenderung, PR Draft/HOLD.

**Q4-Ressourcen (§15):** `H-RAM-2` (K0+K1) am 2026-10-24 auf HEAD `4afc6266` unter Ownerfreigabe
ausgefuehrt (Ergebnisse §15.9, `PARTIAL`, unabhaengiger Review offen). Quellenbefund Boot-Schreibpfad:
`P1_BOOT_WRITES_STATE_STORE=CONDITIONAL`, `K0_WITHOUT_G_S=NOT_ELIGIBLE` → `G-S` ist Voraussetzung
bereits fuer den ersten Diagnoseflash/Boot (K0 **und** K1); ohne `G-S` `HOLD` vor dem Flash. Zusaetzlich `G-S` deckt `nvs` nicht ab; `SYSTEM_NVS_WRITE_AT_P1=EXCLUDED` laut `PRE_P1_REPORT` (P0-Preflight 2026-10-24, nur lesend). `G-F`/`G-S` wurden am 2026-10-24 ausdruecklich freigegeben; P1 wurde ausgefuehrt (§15.9).
`G-R7`/`G-RUN` nicht freigegeben. Bis dahin `C5_RESOURCE_GATE=HOLD`, keine Hardwareaktion.

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


## 13. HTTPD-Run-Stackdiagnose und begrenzte Q4-Implementierung (Option A)

**Aktueller Ownerentscheid:** Plan-SHA
`623bc4520bcf3601d3666e7c717ae00213047901` ist fuer die zusammenhaengende
**Q4-Produktimplementierung Option A** freigegeben (Ownerauftrag 2026-10-23).
Die folgenden Diagnose-/Prototypabschnitte dokumentieren ihre jeweiligen
historischen Entscheidstaende; ihr damaliger Implementierungs-HOLD ist fuer
Option A aufgehoben. Ressourcen-, Merge- und Aktorfreigabe bleiben offen.
Q3 und C0–C5 sowie das 8192-B-HTTPD-Budget bleiben unveraendert.
Option B (Settings-Decode) ist nicht Teil dieser Umsetzung.


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

### Q4-Head-Recycling und RAM-Duplikatbilanz (2026-10-23)

**Historischer Entscheid vor Option-A-Freigabe: IMPLEMENTATION_READY=HOLD; FULL_CHAIN_RESERVE=NOT_PROVEN.**
Basis `a38ba893091350f1c325b0fc379f25883d87703c`, Produkt095f393 unveraendert.
Der neue Ownerauftrag autorisiert genau die fortgesetzte gemeinsame Headprobe
und gezielte Duplikatbilanz, **keine Produktumsetzung**. Vorige Q4-Abschluesse
sind historische Entscheidstaende. [Gezieltes Evidence-Delta](../audits/PR209_Q4_RESOURCE_REFACTOR_20261023.md#head-recycling-und-priorisierte-ram-bilanz-2026-10-23)
enthaelt Patch, finale Target-ABI/Frames/Pfade und die Bilanz aller fuenf
Prioritaeten; kein zweiter Plan und keine zweite Implementierungsvariante.

**Ein Head, derselbe Transaktionskern.** In der temporaeren
`run_persistence_coordinator.cpp::writeSnapshotTransaction` genau ein
optionaler Headowner: Prepared encodieren, denselben Speicher per
reset/emplace neu initialisieren, Committed encodieren.8 B unveraenderliche
erste Headrevision ersetzen den nach Reset ungueltigen Prepared-Wert;
Committedrevision exakt+1, bestehender Overflowcheck bleibt. Keine Headalias-
Referenz ueber Reset, keine versteckte zweite Valueflaeche. Beide unabhaengigen
kodierten Headbytes stehen vor dem ersten Write bereit; bei Prepared-
Encodefehler Committed wie bisher noch encodieren und identische Fehlerstufe
melden. Periodisch bleibt nur Committed erforderlich. Physisches OLD,
Head-OLD, NEW und Unknown-Readback bleiben eigenstaendige Byteevidenz.
Storefolge/Durability/Indeterminate/fruehe Slotadoption/Recovery und erst
nachfolgender Post-Commit-Guard/Current-Apply bleiben erhalten. Quelle/Compiler
belegen die Speicherform, nicht die noch ungetestete Laufzeitsemantik.

**Finale gemeinsame Messung, beide Originalprofile:**22 App-/Konsumenten-TUs
mit allen bisherigen Borrow/Guard/View-/ActiveRun-/Web-/Record-/Settings-
Migrationsaenderungen plus zwei Target-ABI-Objekte. SDK-Evidence unveraendert
hashgeprueft wiederverwendet; keine neuen Links oder Firmwarebuilds.
Head320 B/Alignment8; optionaler Head328/8 + eingefrorene Revision8 statt
zwei320-B-Heads:304 B weniger lokale Objektflaeche. Transaction **976→672 B**;
Core224 / RecordPrepare128 / Internal768 / Write208 B unveraendert. GIMPLE
zeigt keinen weiteren Head-Value-Temporary; Codec-Returns sind optionale
Strings28 B. **Der finale Stand mit explizitem Revisionsfreeze ist massgeblich**.

Bekannte gleichzeitig aktive Teilketten: **Run-Store/NVS/Flash7456 B**
(−304 gegen a38ba893), Run-Validierung7040/7072 B, **Settings6960 B**.
Prepare6016 / Confirm4288 / Decision6144 / Ack-Mute4816 B bleiben. Head-Encoding
bekannte6560 B und Post-Commit-Guard5344 B sind getrennte Phasen; folgende
Move-/Cleanup-/Freigabeziele bleiben offen. Keine Addition isolierter Gewinne.

**8-kB-Entscheidung:** Head-Recycling beseitigt den bisher bekannten
Reservefehlbetrag;8192−7456=736 B Rest, nach512-B-Boden nur **224 B** vor
weiteren unaufgeloesten Anteilen (Settings720 B). Ohne vollstaendige SDK-/ROM-/
libc-/NVS-/Envelope-/Readback-/OOM-/Cleanup-/Alias-/Zyklenobergrenzen ist eine
vollstaendige Loesung innerhalb8192 B **nicht belegt**. Keine Unmoeglichkeit
von8 kB behauptet, aber keine belastbare Implementierungs-/Reserveempfehlung.
Unknown bleibt NOT_PROVEN; weder Compilerframes noch dieser Rest sind HWM.

**RAM-Prioritaeten, maximal fuenf Massnahmen:**

1. Settings Read/Decode: bestaetigter moeglicher2R-Kollisionszweig bis65626
   logische Zeichen (zwei Requests mindestens je32814 B inkl NUL); davon
   getrennt R+P-Decode bis65581 logische Bytes. Exakten Bytevergleich behalten.
   Konsumierendes Decode im bestehenden Pruefkern ist separater Follow-up,
   kein pauschaler Loesungsnachweis: groesster Readblock/2R bleiben.
2. Run-Decision/ActiveRun: vorhandenen Kandidaten, Guard und gemeinsamen
   Value/View-Kern der Probe behalten; owning before/after fuer direkte
   Konsumenten und unveraenderliche Safety-/Stale-Basis nicht global entfernen.
3. Run-Persistenz: Head wie gemessen recyceln; Payload/Encoding endet vor
   Store, notwendige OLD/NEW-/Readback-/Slot-/Recoverykopien erhalten. WS-
   Snapshot3904 B und RawRecord3952 B sind keine belegte frei verfuegbare
   Austauschflaeche. IDfenster256 B ist ein konkret bezifferter weiterer
   Ownershipansatz, jetzt keine Implementierung.
4. UI/Web-Praesentationskatalog: ganze Katalogkopie beim Web-GET und alt+neu
   beim Cache-Refill bestaetigt; ProgramDocument200 B, bis16 Document-Slots
   plus Strings/Notes/Vektoren, Gesamtpeak NOT_MEASURED. Nur separater Follow-up
   fuer kurzlebige bestehende Runtime-Lease beim Read/Encode; keine neue
   langlebige Lease oder paralleles UI-Modell. Snapshot-Revisionsbasis,
   projectInto-/Input-Pufferrecycling und HeaderNetwork-Eviction erhalten.
5. Web Prepare/Confirm: ein856-B-Resultat /1200-B-Webframe wie im gemeinsamen
   Prototyp, keine zweite Bestaetigungsidentitaet; verbleibende Requestquellen
   nicht ohne Lebensdauervertrag vorzeitig freigeben. Kein neues Delta gegen
   a38ba893 und keine automatische weitere Webframe-Probe.

Besitzer, Quellstellen, maximale relevante Typ-/Formatgroessen bzw.
NOT_MEASURED, Ueberlappung, Safetybedarf, Vermeidbarkeit, konkrete Massnahme,
Stack-/Festheap-/Dynamikeffekt und Verifikationsweg stehen je Prioritaet in
der [kompakten Bilanz](../audits/PR209_Q4_HEAD_20261023_RAM.tsv). Theoretische
Maxima/verschiedene Phasen sind kein gemessener globaler Peak. Graphkopien
mit shared_ptr sind keine neuen Vollkataloge. Headdelta fester Heap0 B / neuer
Block0 B; Coordinator21352 B und Q3-Scratch1856/8 B unveraendert. Dynamischer
Peak und groesster insgesamt benoetigter Block NOT_PROVEN. String-/Vektor-OOM
bei -fno-exceptions nicht durch ein Enum behandeln; kein falscher Persistenz-
oder OwningOutcome-Erfolg, Replay nicht neu starten.

**Zwei Scopeoptionen, keine weitere Probeserie:** A priorisiert Run-
Persistenzphasen/Parametergrenze:256-B-IDduplikat in bestehender Snapshotflaeche
vermeiden; IDs erst nach Write und Post-Check uebernehmen. Spaetere gemeinsame
Compilerziele Internal≤512 statt768 und Write≤80 statt208 B, **keine gemessenen
Gewinne**, keine daraus addierte Kettenprognose; Snapshot-Aliasing/IDs/Recovery
sind Risiken, kein neuer grosser Heapowner. B separat Settings: konsumierendes
semantisches Envelope-Decode kann einen Payloadrequest bis32769 B sparen;
Stack/Totalpeak offen, grosser Read und2R-Kollisionsnachweis bleiben. Nur
nach validierter Header-/CRC-/Referenzbasis, nie OLD/NEW/Root-/Recoverybytes
vorzeitig veraendern. Kein zweiter Codec/Buffer-Manager. Der Owner entscheidet
einen zusammenhaengenden Implementierungsscope; hier keine weitere rein
analytische Optimierungsiteration beginnen.

Spaeter nur direkt betroffene Tests aus dem vorherigen §13 konkret ergaenzen:
beide Head-Goldenbytes und getrennte Encodefehler vor jedem Write, alle
Transaktions-/periodischen-/Recovery-Fallback-Konsumenten mit unveraenderten
Return-/Durabilitywerten; OLD/NEW/Absent/Other/Readback/Indeterminate,
Post-Write-Stale/Safety/CommittedApplyFailed; Value/View/Borrow/Reentranz/
Replay und kontrollierte Allocation-/Heap-/OOM-/Cleanup-Evidence. Keine
allgemeinen Tests/Governanceablaeufe wiederholen. Jetzt Compile-/ABI-/Hash-/
Patch-/Frameparitaets-/Arithmetik-/Linkpruefung, **keine** Semantik-/Golden-/
Heap-/OOM-/HWM-/Hardwarequalifikation. **STOP; Produktimplementierung bleibt
NICHT FREIGEGEBEN. Weitere Diagnoseiteration nur mit neuem Ownerentscheid.**


### Q4 Option A: Produktimplementierung und Ressourcen-HOLD (2026-10-23)

**Ownerfreigabe:** exakt `623bc4520bcf3601d3666e7c717ae00213047901`;
Produkt `e31e952def3264ca419187b86a2988bd20e3a487`, ersetzt Produkt095f393
fuer dieses begrenzte Delta. Die damaligen Q4-Implementierungs-HOLDs oben sind
historisch; Option A ist umgesetzt. Kein neuer Plan, keine Scope-/Budgeterhoehung,
keine Option B und keine Hardware-/Ressourcen-/Mergefreigabe.

Die private Application leiht ausschliesslich den bestehenden Coordinator-
Kandidaten, mit instanzlokalem Operationstoken bis Write/Readback, zweitem
unveraenderlichem Stale-/Safety-Check, RAM-Move oder Abbruch. Gemeinsame
Value/View-Kerne fuer die sieben umgestellten Web-Varianten, direkte ActiveRun-Konstruktion,
ein Prepare/Confirm-Resultat mit derselben Identitaet, gemeinsame 4→5→6-Migration,
getrennte Encoding-/Writephasen und ein recycelter Head sind produktiv.
Nur die Borrow-Variante nutzt die vorhandenen 256 B Snapshot-IDs: Reset fasst
dieses Array nicht an; IDs werden erst nach durablem Write und zweitem Check
in den Coordinator uebernommen. Direkte Value-Aufrufer behalten ihr lokales
IDfenster und ihre bisherigen Vertraege. Keine zweite State Machine/Persistenz,
kein grosser Heapowner, keine globale Sperre oder neue OOM-Enum.

**Verifikation:** 695/696 gezielte native Tests in 18 direkt betroffenen Suites,
beide echten Xtensa-Firmwareprofile und kanonischer Builder-Self-Check PASS.
Die [aktuelle Vergleichsevidence](../audits/PR209_Q4_RESOURCE_REFACTOR_20261023.md#q4-option-a-produktimplementierung-2026-10-23)
nennt Quellen/Flags/ABI/Artefakte, Cut-Point-/Golden-/Value/View-/Borrow-/
Post-Commit-/Replay-/Migrations-/OOM-Grenzen und native Heapfenster.
Das ist Builder-Evidence; Independent Review ist offen.

**Tatsaechliche Frames beider Profile:** Application1312 / Web1200 /
Borrow48 / Internal(View)512 statt768 / Write160 statt208 / Core160 /
RecordPrepare128 / Transaction672 B. Das Write-Ziel80 B wurde nicht erreicht.
HTTPD-Prefix einschliesslich Adapter/Dispatcher/Mutation1856 B.
Aktuelle bekannte phasenbezogene Summen: Prepare6016, Confirm4288,
Run-Encode/Validierung6704, Run-Store/Unknown-Write-Readback**7216**, Settings**6960**,
Post-Commit-Guard5088, RAM-Move5344, Borrowfreigabe4720 B. Alle sieben umgestellten
Decision-Visitor samt Unterpfaden sind erfasst (6000/6112/6048/6048/6176/5744/5744 B).
Keine Addition isolierter Compilergewinne; alle Summen bleiben **NOT_PROVEN**,
mit konkreten offenen SDK-/ROM-/libc-/indirekten/Allocator-/Cleanup-/Zyklenkanten.
8192−7216=976 B rechnerischer Rest, nach512 B Reserveboden nur **464 B fuer
unbestimmte Anteile**; Settings720 B. Keine Vollketten-Obergrenze oder HWM. **Diese Projektion gilt nur fuer die
sieben umgestellten Varianten, nicht fuer alle vom Web akzeptierten Intents.**

**Neuer nachgewiesener Blocker:** Der bestehende Codec akzeptiert auch
`recovery-time-correction` und `sensor-selection`; Prepare/Confirm reicht
diese an `applyConfirmedPreparedValue` weiter. Bereits dessen Frame allein
ist in beiden Profilen **10880 B**. Tatsaechlich gleichzeitig: HTTPD-Prefix1856
+ Web1200 + Application1312 + Value10880 = **mindestens15248 B >8192 B**
(7056 B ueber Stackbudget, noch ohne Domain-/SDK-Unterpfade). Auch eine
Standby-Ablehnung betritt diesen Frame; nativer Dispatcher-/Replaytest belegt
die Erreichbarkeit. Das Vollketten-Stackgate ist damit **FAIL**, unabhaengig
von offenen ROM-/SDK-Obergrenzen. Der bisherige Ausschluss dieser beiden
Intents als angeblich nicht Web-erreichbar war falsch; die frueheren
7-Varianten-Proben belegen keinen vollstaendigen Runpfad. Keine automatische
Scopeerweiterung oder Produktkorrektur aus diesem Befund.

**Heap:** gleiche native Maximalfixtures gegen095f393: Start samt Replay
Peak47484→45094 B, Adjustment mit32 Run-Revisionen46088→43698 B,
Settings69724→69724 B; groesster beobachteter Request jeweils unveraendert
32769/32769/19890 B. Keine gemessene Regression oder neue grosse feste
Ownerallokation. Host64, angeforderte Bytes ohne Allocator-/Fragmentierungs-
beleg; unveraenderte aligned Q3-Scratchallokation nicht in diesem Zaehler.
Absoluter ESP32-Peak, groesster freier zusammenhaengender Block, vollstaendige
Fehler-/OOM-Obergrenze **NOT_PROVEN**. String-/Vektor-OOM unter `-fno-exceptions`
bleibt ohne allgemeinen Statusreturn; native Exception-Cleanup ist kein
Zielsystem-OOM-Vertrag. Historische F4-/H-RAM-Belege qualifizieren diesen Stand nicht.

**STOP / eine Ownerentscheidung:** Ob die weiterhin Web-erreichbaren
Recovery-/Sensorrequests in den Ressourcenrefactor aufgenommen werden. Dieser
zusaetzliche Produktumfang ist nicht freigegeben; §14 enthaelt den konkreten
Umsetzungsvorschlag und wartet auf Ownerfreigabe der exakten Plan-SHA. Der aktuelle Implementierungsdiff wird mit dem konkreten
Blocker unabhaengig reviewbar uebergeben. Bis dahin `C5_RESOURCE_GATE=HOLD`,
`HTTPD_FULL_CHAIN_STACK=FAIL`, `FULL_CHAIN_RESERVE=NOT_PROVEN`,
`RAM_PASS=NOT_PROVEN`, `OPEN_BLOCKERS=1`, `ACTUATOR_RELEASE=NO`;
D9/Flash/Hardware gesondert **NOT_RUN**. Ein Hardwarelauf kann den bereits
statisch nachgewiesenen 10880-B-Frame nicht freigeben.

## 14. Q4-Korrekturdelta: Web-erreichbare Value-Stackpfade (Recovery-Zeit, Sensorselektion)

**Status: auf Plan-SHA `05f8d7482c52ada9f53e70ff7958d5a4e20689f6` vom Owner freigegeben
und umgesetzt (§14.7).** Auftrag: `PR209_Q4_Fixauftrag_Web_Value_Stackpfade.md` (Review-Basis
PR-HEAD `2d0fde5`, Produkt-HEAD `e31e952`). Loest die STOP-Frage am Ende von §13 **vorschlagend**:
die zwei Intents bleiben Web-erreichbar und sollen mit der bestehenden Option-A-Mechanik
(Borrow/Guard/Candidate, `CommandDecisionView`) gefuehrt, nicht deaktiviert werden. Das
8192-B-Budget, Q3 und C0–C5 bleiben unveraendert; Option B ist nicht Teil davon.

### 14.1 Befund (am Code verifiziert)

- `FermentationApplication::applyConfirmedPrepared` leitet `ApplyRecoveryTimeCorrectionRequest`
  und `SensorSelectionCommandRequest` an `applyConfirmedPreparedValue` (lokale
  `CommandDecision`, 10 880 B Frame).
- `FermentationUiCommandBridge::decidePreparedIntoCore` liefert fuer beide Typen bei
  `Decision = CommandDecisionView` `unsupportedAppDetail()`; nur der Value-Zweig ruft
  `decideApplyRecoveryTimeCorrection` bzw. `decideApplySensorSelectionAction`.
- Beide Domainfunktionen (`run_commands.cpp`) liefern `CommandDecision` per Wert; die
  Sensorselektion baut zusaetzlich einen lokalen `candidate = decision.before`
  (weiteres `RunCommandState`, 4896 B) und verschiebt ihn nach `decision.after`.
- Die sieben bereits umgestellten Varianten nutzen das Muster
  `decideXInto<Decision>` + `RestoreRejectedAfter<Decision>` + `beginDecisionInto`.

### 14.2 Delta (genau diese Aenderungen)

1. **Domain (`run_commands.cpp/.hpp`):** `decideApplyRecoveryTimeCorrection` und
   `decideApplySensorSelectionAction` werden zu Kernen `...IntoCore<Decision>` nach dem
   Muster der sieben Varianten; zwei neue oeffentliche Overloads je Funktion
   (`decideApplyRecoveryTimeCorrectionInto` / `decideApplySensorSelectionActionInto`, je
   fuer `CommandDecision&` und `CommandDecisionView&`). Die bestehenden Value-Funktionen
   bleiben als duenne Huellen (identische Signatur und Rueckgabe) – eine einzige
   Fachimplementierung, keine zweite Regel.
   - Recovery: `beginMutation` und das Setzen von `after.nominalRecoveryAdjustment`
     wirken direkt auf `decision.after`; Ablehnungen setzen nur `status`
     (`RestoreRejectedAfter` stellt `after = before` her).
   - Sensorselektion: kein lokales `candidate`-`RunCommandState`; die Mutation
     (`applySensorSelectionMutation`, `applySensorRoleChangeQualificationReset`) laeuft
     direkt auf `decision.after`. Fehlpfade (`InvalidInput` nach Reset-Fehlschlag,
     `NotConfirmed`) hinterlassen `after` ueber `RestoreRejectedAfter` unveraendert.
     Reihenfolge der Pruefungen bleibt exakt wie heute (Status-/Confirm-Semantik
     unveraendert: `NotConfirmed` erst nach Mutations- und Reset-Pruefung).
     `SensorSelectionStateView`/`SensorSelectionDecision`/`SensorSelectionMutation`
     bleiben lokal; ihre Frame-Groesse wird gemessen (14.4), nicht vermutet.
2. **Bridge (`fermentation_ui_commands.cpp`):** In `decidePreparedIntoCore` entfallen die
   beiden `unsupportedAppDetail()`-Zweige fuer `CommandDecisionView`; beide Typen rufen
   die neuen `...Into`-Funktionen (Plausibility-Pruefung `owningPlausibility()` bleibt
   vorgeschaltet und fail-closed).
3. **Application (`fermentation_application.cpp/.hpp`):** Der Sonderabzweig in
   `applyConfirmedPrepared` entfaellt, beide Typen laufen durch den bestehenden
   `beginCommandOperation`/`commandCandidate`/`persistBorrowedCommand`-Pfad.
   `applyConfirmedPreparedValue` hat nach Suche in `lib/`, `src/` und `test/` genau einen
   Aufrufer (der entfallende Abzweig) und wird samt Deklaration entfernt.
4. **Persistenz-Koordinator (`persistCommandInternal`, am Code geprueft):**
   `isPersistedRunCommand` ist fuer beide Kinds `true`; `applyRunCommand(..., const
   CommandDecisionView&)` existiert; `requireRecoveryEpisodeRevision` und `addEffect`
   sind bereits ueber `Decision` templatisiert; die Operation gibt Borrow/Candidate ueber
   `~CommandOperation` frei, auch nach RAM-only-Rueckgabe. **Einzige Luecke:** der
   `AppliedRamOnly`-Zweig (vor dem View-Identitaetscheck `&decision.after ==
   &workingSet_.candidate && commandBorrowed_`) verschiebt bei einer View
   `decision.after` per `applyRunCommand` in den Live-Zustand. Der gleiche
   Identitaetscheck wird fuer die View vor diesen Zweig gezogen (kleine, ausschliesslich
   defensive Koordinatoraenderung; Verhalten fuer die Application-Aufrufe unveraendert).
   Sonst bleiben Persistenz-Sonderfaelle unveraendert (`AppliedRamOnly` ohne
   Store-Write/persistierte CommandId; persistent: Write-before-Apply,
   `PersistenceCommittedApplyFailed`, `persistedIds_`-Vorrang `AlreadyPersisted`).
5. **Keine** neuen Buffermanager, Tasks, Heapowner, Routen-Abschaltungen oder
   `Unavailable`-Ersatzantworten. **STOP-Bedingung:** scheitert die Wiederverwendung an
   einem konkreten Vertrag (z. B. `persistBorrowedCommand` verlangt ein Feld, das die View
   nicht traegt), wird mit praezisem Gegenbeispiel angehalten, keine Teilunterstuetzung.

### 14.3 Unveraenderte Vertraege (Paritaetsbedingung)

Rueckgabewerte aller bisherigen Value-Konsumenten (Feld fuer Feld: `status`, `kind`,
`envelope`, `effects`, `sensorSelectionApplyStatus/-Event/-Notice`,
`committedControlContextTransition`, `before`/`after`); bestaetigte Identitaet, Origin,
CSRF/Session, Sequenz-/Digest-Replay, Ablehnung und `NoChange`.

### 14.4 Stackbilanz (Pflichtnachweis)

Gemeinsame Bilanz fuer **alle tatsaechlich Web-erreichbaren Intents** (nicht nur sieben):
Prepare, Confirm, Apply je Intent einschliesslich Applied/Rejected/NoChange/RAM-only/
Persistenz-/Readback-Phasen, auf dem Produktcommit, beide echten Xtensa-Profile, mit
`.ci`-Frames, SDK-/NVS-/Allocator-/Fehler-/Cleanup-Unterpfaden. Nicht belegte
Aufrufziele bleiben ausdruecklich `NOT_PROVEN`. Grenze 8192 B und begruendeter
Reserveboden gelten unveraendert; die frueheren Sieben-Varianten-Werte werden nicht als
Vollketten-PASS ausgegeben. Erwartung (Hypothese, nicht Nachweis): Wegfall von
10 880 B Value-Frame; Rest hangt vom Frame der Sensorselektions-Domainfunktion ab.

### 14.5 Tests (gezielt, in bestehenden Suiten)

- `test_run_commands`: Value-vs-View-Paritaet beider Funktionen inkl. vollstaendiger
  Ergebnisfelder; erlaubte/abgelehnte Zustaende; Rollback von `after` bei jeder
  Ablehnung (inkl. Reset-Fehlschlag und `NotConfirmed`).
- `test_run_persistence_coordinator`: Borrowed-View-Pfad fuer beide Kinds; RAM-only ohne
  Write und ohne `persistedIds_`-Eintrag; `AlreadyPersisted`-Vorrang; View mit fremdem
  `after` (nicht der geliehene Kandidat) wird vor RAM-only-Apply abgelehnt;
  Writefehler/Indeterminate.
- Bridge-/Application-Tests: beide Intents ueber den Borrow-Pfad; Recovery-/UTC-/ID-/
  Origin-/Replay-Paritaet; SensorSelection `AppliedRamOnly` ohne Write und ohne
  persistierte CommandId; persistente Mutation mit Writefehler/Indeterminate;
  Post-Commit-Safety/Stale (`PersistenceCommittedApplyFailed`).
- Web-Dispatcher: beide Intents vom echten Dispatcher bis zum Replay, auch bei
  fachlicher Ablehnung.
- Bereits umgestellte sieben Varianten nur bei tatsaechlicher Regression anfassen.
- Heap-/OOM-Semantik und groesster zusammenhaengender Block bleiben im bestehenden
  Ressourcengate; kein stilles PASS.

### 14.6 Abschlussgrenze und Reihenfolge

Eine Freigabe dieser Plan-SHA deckt eine Folge von vier Commits ab, die ohne weiteren
Zwischenhalt verkettet werden: (1) Domain-Kerne + Domain-Tests, (2) Bridge/Application/
Koordinator-Guard + zugehoerige Tests, (3) Web-Dispatcher-/Replay-Tests, (4)
Messnachweis (Audit-Dateien) + Doku/Roadmap. Danach Fix Verification statt neuem Full Review, solange Scope/Architektur/Vertraege nicht
materiell abweichen. Auch bei PASS dieses Deltas bleiben offen: vollstaendiger 8-kB-
Stacknachweis, dynamisches ESP32-RAM/Largest-Free/OOM, getrennte D9-/HWM-Qualifikation
(Hardware-/Flash-Freigabe separat). `ACTUATOR_RELEASE=NO`; PR bleibt Draft/HOLD;
kein Ready, Merge oder Hardwarelauf ohne Owner.

### 14.7 Umsetzungsergebnis

Freigegeben und umgesetzt auf Plan-SHA `05f8d7482c52ada9f53e70ff7958d5a4e20689f6`
(Commits `eeb23ff`, `3a1e664`, Tests `1427dea`). Der 10 880-B-Value-Fallback ist entfernt;
beide Intents laufen durch den Borrow-Pfad. Zielsystem-Entscheidungspfade: Recovery 5760 B,
Sensorselektion 6672 B (bekanntes Gesamtmaximum unveraendert 7216 B), alle Summen
**NOT_PROVEN**. Details: [Evidence](../audits/PR209_Q4_VALUE_DELTA_20261023.md). Die
statische Ueberlast aus §13 ist beseitigt; Vollkettenobergrenze, ESP32-RAM/Largest-Free/OOM und
D9/HWM bleiben offen (`C5_RESOURCE_GATE=HOLD`). Weiterer Stand und Gate: §15.

## 15. Q4-Ressourcenqualifikation: Entscheidungsstand, M-H1 (abgeschlossen) und Messplan H-RAM-2

**Status: PLAN – keine Produkt-, Test-, Buildkonfigurations- oder Hardwareaktion ohne
ausdrueckliche Freigabe der exakten Plan-SHA und der getrennten Flash-/Hardwareautorisierung.**
Auftrag: `PR209_Q4_Ressourcenqualifikation_Auftrag.md` (Basis HEAD `75fd006`; §14 `05f8d74` und
Fix Verification unveraendert). Evidence:
[PR209_Q4_RESOURCE_QUALIFICATION_20261023](../audits/PR209_Q4_RESOURCE_QUALIFICATION_20261023.md).

### 15.1 Ergebnis der Softwareanalyse (nur Analyse, keine Codeaenderung)

- **Stack:** die ohne Compiler-Frame verbliebenen Aufrufziele wurden gegen das gelinkte ELF und die
  ROM-ELFs aufgeloest, ihre direkten Aufrufkanten ergaenzt (zuvor waren sie Blaetter), terminale
  Abort-/Assert-Pfade getrennt behandelt. Maximum **7 440 B** (Run-Store/Write-Readback und Decision,
  beide Profile), Settings 7 024 B, Sensorselektion 6 816 B. Feste Abzuege **320 B** (Coprozessor-
  Save-Area 96 + TLS 16 + Canary 16 + Level-1-ISR-Frame 192, per Probe gemessen): **7 760 B < 8 192 B,
  Rest 432 B** – **80 B unter dem 512-B-Planungsboden** aus §13 (Planwert, keine Safety-Grenze).
  Formal offen: `__indirect_call`-Zielmengen (NVS-/SPI-Flash-Treibertabelle), geschaetzt ≤ ~290 B
  (Worst-Case 8 050 B, Rest ≈ 140 B). Rekursion bounded, keine dynamischen Frames auf den Pfaden.
  Kalibrierung: H-RAM-1 maß 5 996 B auf den GET-Routen (andere Commit-Identitaet) gegen eine
  statische Obergrenze von 6 272 B – Modell nicht widerlegt, ≈ 4 % konservativ; kein Nachweis fuer
  Mutationspfade. Der aufgeloeste Wert ist **kein HWM**.
- **Heap (Befund F-H1):** `encodeSingleProgramDocumentPayload` legt `ByteWriter(32768)` an und
  reserviert damit unabhaengig vom Programminhalt **32 769 B zusammenhaengend**; der Aufrufer
  `run_persistence_codec.cpp:1207` kodiert jeden Laufsnapshot mit Programmquelle (Start,
  Command-Persistenz, Checkpoints, Readback-Vergleiche). Im Idle passt der Block (grosster Block
  40 960 B), unter dem historisch gemessenen Web-Verkehr nicht (22 528–26 624 B, andere
  Identitaet). Das Risiko betrifft den Normalbetrieb mit Lauf, nicht nur den Maximalkatalog; der
  lokale Editor kann den Maximalkatalog (16 Programme) ebenfalls am Geraet anlegen (D6 gilt nur
  fuers Web). Zeitgleiche Messung auf diesem HEAD fehlt.
- **OOM:** `operator new` und `std::string`/`std::vector`-Wachstum enden unter `-fno-exceptions`
  im Link nachgewiesen in `abort()` (`PANIC_PRINT_REBOOT`): Neustart statt Fehlerstatus; `cJSON`
  liefert Null/400. Das Verhalten ist produktweit vorbestehend und nicht durch PR #209 entstanden.
- **Messmittel:** `logResources()` und der Failed-Alloc-Hook sind im Produkt; Overlay M (HTTPD-HWM
  + Heap-Tripel, nur vier GET-Routen) existiert als Diagnosepatch fuer `a9a1156`. Keine vorhandene
  Hardwareevidence gilt fuer diesen HEAD oder einen Mutationspfad.

### 15.2 Gate-Werte

`HTTPD_FULL_CHAIN_STACK=NOT_PROVEN`, `HTTPD_STACK_RESERVE=FAIL` (432 B gegen den 512-B-Planungsboden;
rechnerische Tragfaehigkeit bleibt), `ESP32_HEAP_PEAK=NOT_PROVEN`, `ESP32_LARGEST_FREE_BLOCK=NOT_PROVEN`
(Risikoindikation F-H1), `TARGET_OOM_BEHAVIOR=FAIL` (kein Fehlerstatus; vorbestehend; blockiert C5
nicht eigenstaendig – C5 haengt an den Heap-Gates; die Akzeptanz des Reset-Pfades als Fehlervertrag
ist eine bereits bestehende, hier nicht erneut zu treffende Ownerentscheidung und nicht Teil von
`H-RAM-2`), **`C5_RESOURCE_GATE=HOLD`**. Ein Compilerwert oder Host-Test ist kein Hardware-HWM.

### 15.3 M-H1 – Einzelprogramm-Writer nach exakter Nutzlastgroesse (Produktkorrektur, nicht freigegeben)

**Ursache (belegt):** `encodeSingleProgramDocumentPayload` (`configuration_document_codec.cpp`)
legt `ByteWriter writer(kMaximumProgramCatalogPayloadBytes)` an; der Konstruktor ruft
`buffer_.reserve(32768)` auf (32 769 B inkl. NUL, auf dem Ziel mindestens). Der Aufrufer
`encodeProgramDocumentPayload` wird von `encodeRunPersistenceSnapshot` (`run_persistence_codec.cpp`)
fuer jeden Laufsnapshot mit gespeicherter Programmquelle verwendet.

**Soll-Aenderung (nur diese Funktion, nur im bestehenden Codec):**
1. Nach der unveraenderten Validierung (`validateProgram(..., Runnable)` → `InvalidDocument`) die
   **exakte Einzeldokumentlaenge** mit dem vorhandenen `addProgramPayloadSize` und einem
   `ProgramCatalogPayloadSizeAccumulator` berechnen, der bei **0** startet. Die Katalogfunktion
   `calculateProgramCatalogPayloadSizeImpl` addiert zuerst das Katalog-Zaehlbyte (+1 B) und wird
   **nicht** wiederverwendet; keine Kopie in einen `ProgramCatalog`-Eintrag, keine neue generische
   Abstraktion. Die Akkumulatorobergrenze bleibt `kMaximumProgramCatalogPayloadBytes` (32 768) und
   entspricht damit exakt der bisherigen `ByteWriter`-Kapazitaet des Einzeldokuments.
2. `ByteWriter writer(berechneteLaenge)`; `writeProgram` bleibt die **einzige** Kodierung.
   Wie im Katalogpfad wird nach dem Schreiben `writer.size() == berechneteLaenge` geprueft
   (Abweichung → `CapacityExceeded`); das ist ein Regressionsschutz gegen auseinanderlaufende
   Gross-/Schreibfunktionen und im gueltigen Betrieb unerreichbar.
3. **Statuswerte bleiben:** Validierungsfehler → `InvalidDocument` vor jeder Allokation. Jeder
   Nicht-Erfolg der Groessenberechnung (nicht abbildbare Enum-Wire-ID, Ueberlauf 32 768, String
   > 65 535 B) wird auf `CapacityExceeded` abgebildet, weil genau diese Faelle bisher in
   `writeProgram` fehlschlugen und `CapacityExceeded` lieferten. `out` bleibt bei jedem Fehler
   unveraendert (`out.swap(encoded)` erst nach Erfolg). Kein Wire-/Persistenzformat, keine
   Laengenobergrenze, keine Schema-/Revisionsaenderung.
4. Kein geschaetzter Konstantenwert („1,5 KB“): die Groesse wird je Dokument berechnet.

**Maximale Einzelprogrammlaenge (gemessen, nicht extrapoliert):** am Referenzstand `76cb4ce`
liefert das vorhandene Maximalfixture `maximizeProgramPayload` (in `maximumValidCatalog()`,
Katalog gesamt 19 932 B fuer 16 Programme) fuer ein einzelnes Dokument **1 255 B** (Maximum,
Programme mit 48-B-Id) bzw. 1 217–1 218 B (maximierte Fabrikprogramme). [Korrektur nach Messung
in der Umsetzung: die im Plan-Commit `6a311d6` genannten 1 256 B/217 B waren ein Rechenfehler der
Zwischenprobe; das kleinste *gueltige* Programm im Test hat 89 B.] Die Umsetzung weist die Obergrenze zusaetzlich aus den
Modellgrenzen aus (Id 48 B, Name 96 B, Notizen 1 024 B, Stagezahl, Optionalfelder) und prueft im
Test, dass kein gueltiges Programm darueber liegt; die Grenze 32 768 bleibt unveraendert wirksam.

**Nachweise nach Freigabe (gezielt):**
- Byte-Paritaet gegen die Referenz `76cb4ce`: Golden-Bytes (vor der Aenderung erzeugt) fuer
  repraesentative und grenzwertige gueltige Programme (kleinstes Fabrikprogramm, Maximalprogramm,
  jeweils mit/ohne Optionalfelder) **und** unabhaengig: `encodeProgramDocumentPayload(d)` ==
  `encodeProgramCatalogPayload({d})` ohne das fuehrende Zaehlbyte.
- Unveraenderte Statusfaelle: `InvalidDocument` (Validierung), `CapacityExceeded` (nicht abbildbare
  Enum-Werte ueber Cast, soweit vor der Validierung ueberhaupt erreichbar, sowie
  Groessen-/Ueberlaufpfad am Unterbau), `out` bei Fehler byteweise unveraendert.
- Bestehende Run-Snapshot-/Recovery-/Golden-/Store-Suiten (`test_run_checkpoint_codec`,
  `test_run_persistence_coordinator`, `test_configuration_codecs`, `test_program_models`,
  `test_configuration_graph_*`) ohne Aenderung gruen.
- **Allokationsnachweis:** mit dem vorhandenen nativen Allokationsmessmittel
  (`operator new`-Hook in `test_configuration_codecs`, ggf. um „groesste Einzelanforderung“
  erweitert) die **groesste Einzelanforderung der Einzelprogrammkodierung** vor/nach M-H1
  ausweisen (erwartet: 32 769 → Dokumentgroesse + 1). Nicht beseitigt und nicht als beseitigt zu
  deklarieren: die 8-KiB-Reservierung des Run-Snapshot-Writers (`kMaximumCheckpointPayloadBytes`)
  und weitere Puffer; die ESP32-Out-of-Memory-Semantik (Abort + Neustart) bleibt unveraendert.
- Zielseitig: beide Profile bauen, Stack-Delta (`.ci`/ELF) der geaenderten Funktion; keine
  Hardwaretests, kein Flash.
`ESP32_LARGEST_FREE_BLOCK`/`ESP32_HEAP_PEAK` bleiben nach M-H1 `NOT_PROVEN`, `C5_RESOURCE_GATE=HOLD`.

### 15.4 Stackoptionen (nur Entscheidungsgrundlage)

| Option | Rest (aufgeloest, inkl. 320 B) | RAM-Wirkung |
|---|---|---|
| A: 8 192 B behalten | 432 B (Worst-Case-Schaetzung ≈ 140 B) | 0 |
| B′: 8 704 B (+512 B) | 944 B (Worst-Case ≈ 650 B) | −512 B internes RAM |
| C: 10 240 B (+2 048 B) | ≈ 2 480 B | −2 048 B |
| D: 10 240 B, Main-Task 24 576 → 22 528 B | ≈ 2 480 B | netto 0 B, Buildkonfigurationsaenderung |

**Der Stackentscheid (8 192/8 704 B) wird erst nach Pruefung der Gesamt-RAM-Auswirkung
getroffen** (freier Heap, groesster Block, Main-Task-Slack, F-H1-Wirkung nach M-H1), nicht allein
aus der Stackbilanz. Der Settings-Decode-/UI-Katalogumbau (Option B aus §13) bleibt nur bei auf
Hardware nachgewiesenem Heap-Engpass die priorisierte naechste Option. Nicht Teil von §15: Option B,
UI-Katalogumbau, weitere Run-Kopieroptimierungen, Pre-Ready/Merge, Aktor-/Hardware-Inbetriebnahme.

### 15.5 Status und Reihenfolge

```text
M_H1=CLOSED_GO              (Independent Review PASS/GO, M_H1_FIX_VERIFICATION=PASS,
                             M_H1_OPEN_BLOCKERS=0 auf HEAD 1c56b4d; Evidence PR209_Q4_MH1_20261024.md)
H_RAM_2=EXECUTED_PARTIAL    (K0+K1, Stack- und Heap-Profil ausgefuehrt auf HEAD 4afc626; Ergebnisse §15.9,
                             Evidence PR209_Q4_HRAM2_RESULTS_20261024.md; unabhaengiger Review OFFEN)
H_RAM_2_RESTORE=HASH_PASS   (state_store/Boot/App byteidentisch zu P0; Kontrollboot der Original-Firmware NOT_RUN)
C5_RESOURCE_GATE=HOLD
PR209_MERGE=HOLD            (M-H1-GO ist kein Merge-GO)
ACTUATOR_RELEASE=NO
```

Offen nach H-RAM-2 (§15.9): `HTTPD_FULL_CHAIN_STACK=NOT_PROVEN`, `HTTPD_STACK_RESERVE=NOT_PROVEN`
(statisch korrigiert 208 B Rest < 512-B-Planungsboden, gemessen 1 724 B Rest nur ohne den tiefsten
Pfad), `ESP32_HEAP_PEAK=PARTIAL`, `ESP32_LARGEST_FREE_BLOCK=PARTIAL`, `TARGET_OOM_BEHAVIOR=FAIL`
(vorbestehend). Reihenfolge: (1) unabhaengiger Review der H-RAM-2-Ergebnisse (§15.9); (2) Ownerentscheid zur
Stackgroesse (8 192 vs. 8 704 B) auf Basis dieser Ergebnisse; kein praeventiver Stack-/Partitionsumbau,
keine neue Optimierungsserie.

### 15.6 H-RAM-2: Messplan nach tatsaechlicher Abdeckung (Entwurf, nicht freigegeben)

Grundlage: aktueller Code (HEAD `1c56b4d`), `main/app_main.cpp`, `docs/RESOURCE_BUDGET_AND_MAINTENANCE.md`
(R1-RAM-Referenzbasis O4), vorhandene Hardware-Evidence
([H-RAM-1](../audits/PR206_H_RAM_1_EVIDENCE_20261019.md),
[PR #174 LVGL-48-KiB](../audits/R1_RAM_LVGL48_HW_EVIDENCE.md)) und das Issue-#90-Slice-7-Harness.
Keine Aussage hier ersetzt eine Messung; Quellen fuer jede Zeile stehen in der Evidence
([Coverage-Matrix](../audits/PR209_Q4_HRAM2_COVERAGE_20261024.md)).

#### 15.6.1 Welche kritischen Pfade sind auf vorhandener Hardware erreichbar?

| # | Pfad | erforderlicher Zustand | ausloesbare Aktion | Aktor-/Safety-Voraussetzung | Messmittel | Abdeckung |
|---|---|---|---|---|---|---|
| 1a | HTTPD GET Shell (unauthentifiziert, Login-Shell) und Asset | Standby, Netz verbunden, keine Session | GET | keine (aktorfrei); **nicht schreibend** (kein Store-Zugriff im Handler; durch Hash-Vergleich zu bestaetigen) | HTTPD-HWM + Heap-Tripel (Diagnose-Logging) | **erreichbar ohne `G-S`** |
| 1b | HTTPD GET Status/read-only (authentifiziert) | **erfolgreicher Login noetig:** `handleReadOnly` liefert ohne Session `401 session-required` vor `readOnly_.handle()` (`web_application_routes.cpp:1035`) | Login, dann GET | siehe Zeile 2 (Login schreibt `state_store`) | wie 1a | **erreichbar nur mit `G-S`**; ein unauthentifizierter Abruf ist nur eine 401-Probe und **kein** Nachweis des Read-only-Encoders |
| 2 | Login/Session (KDF) | Standby, Passwort gesetzt | POST login (nur korrektes Passwort) | keine Aktorwirkung; **jeder erfolgreiche Login schreibt `state_store`:** `verifyWebPassword` setzt unabhaengig vom Lockoutzustand `recordSequence + 1` und ruft `store_.writeCredentials(expected, target)` (`authentication_records.cpp:832–888`, `writeCredentials` in Z. 877); Fehlversuche aendern zusaetzlich den Lockout-Zaehler und werden nicht verwendet | wie 1a; Main-/HTTPD-HWM; NVS-Schreibkette (Credential-Record) | **erreichbar nur mit `G-S`**; ein Login je Boot ist ein persistenter NVS-Schreibpfad und gehoert in die NVS-/Heap-Messung |
| 3 | Prepare/Confirm aller neun Intents | Standby, **authentifiziert** (Login, siehe Zeile 2) | POST, fachlich **abgelehnt** (`422`) | keine; kein Apply; Login-Schreibpfad wie Zeile 2 | wie 1a | **erreichbar nur mit `G-S`**, nur Ablehnung; kein erfolgreicher Run-Store |
| 4 | Ack/Mute (RAM-only Erfolg) | aktive Meldung noetig | POST | keine | wie 1 | **nicht auslösbar** (keine Meldung erzeugbar) → `NOT_RUN` |
| 5 | Geraetenamen-Commit (Settings, Erfolg) | Standby | POST `set-device-name` | keine Aktorwirkung; **schreibt `state_store`** (Konfigurationsgraph, Revisionen); Wirkung des Namens erst beim naechsten Netzwerkstart (Q1); nur mit Rueckfallweg §15.6.5 | wie 1; Konfig-NVS-Kette | **erreichbar unter Bedingung G-S**; nur Konfig-Records, keine Mehrseiten-Programmblobs |
| 6 | **Erfolgreicher** Run-Start → Run-Store/Write-Readback mit Programmsnapshot (Web) | Start verlangt gueltige Luft-/Kuehlsensor-Evidenz (`airSensorValid && coolingSensorValid`), lauffaehiges Programm, trusted UTC | POST `start-program` | **im Produktimage nicht erreichbar:** `real actuators: disabled`, `publishOwningRuntimeEvidence` hat keinen Aufrufer ausser Tests, DS18B20-Sampler ist budget-gesperrt (`kApprovedDs18b20TaskBudget` leer, #30/#35) | – | **`NOT_RUN/NOT_PROVEN`**; ein abgelehnter Standby-Start gilt **nicht** als Run-Store |
| 7 | Periodischer Run-Checkpoint | aktiver Lauf | – | wie 6 | – | `NOT_RUN` |
| 8 | RAM-only-Sensorentscheidung | aktiver Lauf | – | wie 6 | – | `NOT_RUN` |
| 9 | Run-Store-/NVS-Teilkette real (Coordinator → NVS → Flash, Power-Cut, Readback) | Slice-7-Harness-Image, Testpartition `state_store_test` | UART-Befehle (`RUN_CONTROL_WRITE`, `ARM_RUN_WRITE_LOAD` …) | **destruktiv:** Harness-Partitionstabelle legt `state_store_test` auf dieselbe Flashregion `0x300000`/1 MiB wie den Produkt-`state_store`; Aktoren aus; synthetische Sensorgueltigkeit **nur im Harness** | Harness-`STATUS` (**kein** HWM-Feld heute) | **Teilabdeckung ohne HTTPD/Web-Prefix**, Manuallauf statt gespeichertem Programm; Diagnose-Ergaenzung (HWM-Feld) separat zu genehmigen |
| 10 | NVS-Fehler-/Unknown-/GC-Varianten (`requestNewPage`, Erase) | volle Seiten, Fehlerinjektion | – | im Produktimage keine Injektionsnaht | – | Fehler: nur Harness-Power-Cuts (Zeile 9); GC nur durch viele Konfig-Commits (Flash-Verschleiss, optional, begrenzt) → sonst `NOT_RUN` |
| 11 | Lokaler Maximalkatalog (16 Programme, Notizen 512 Zeichen) | lokaler Editor | Touch-Eingabe | keine | Heap-Tripel | technisch erreichbar (Notizfeld im lokalen Editor), manuell unrealistisch (12 × 512 Zeichen) → `NOT_RUN` ohne separat genehmigte Hilfe |
| 12 | Boot → Netz → LVGL/Touch/Web gleichzeitig | Normalbetrieb | Owner-Touch + Browser | keine | `logResources()` (im Produkt), Failed-Alloc-Hook | **erreichbar** (Referenz: PR #174, andere Identitaet) |

#### 15.6.2 Erfolgreicher Run-Pfad ohne unautorisierte Aktorwirkung

Befunde am Code: Das Produktimage startet mit `real actuators: disabled`; die Onboard-Ausgaenge sind
nicht an den Planner angebunden (`main/app_main.cpp`); die Sensorevidenz erreicht die Anwendung nicht
(`publishOwningRuntimeEvidence` ohne produktiven Aufrufer, Sampler budget-gesperrt). Ein
erfolgreicher Web-Run-Start ist damit auf dem Produktimage **nicht herbeifuehrbar**, ohne die
Sensor-/Safety-Voraussetzung zu umgehen. Das wird **nicht** umgangen (kein Safety-Bypass, keine
Sensorsimulation im Produkt, keine neue Simulationsarchitektur).

- **Bestehende sichere Moeglichkeit:** das Issue-#90-Slice-7-Harness (`APP_ISSUE_90_SLICE7_HARNESS`,
  eigenes Image, eigene Testpartition `state_store_test`, reale NVS-/Power-Cut-Kette). Es
  deckt die Coordinator-/NVS-/Flash-Teilkette des Run-Store ab (Zeile 9), **nicht** den
  HTTPD-/Web-Prefix und nicht den Programmsnapshot (es startet einen Manuallauf mit synthetischer
  Sensorgueltigkeit). Es ist ein **destruktives Charakterisierungs-Image**: seine
  Partitionstabelle (`partitions/issue_90_state_store_test.csv`) belegt dieselbe Region wie der
  Produktzustand; es braucht eine eigene Partitionstabelle sowie Backup/Restore des Zustands
  (bestehender Runner) und ist deshalb von H-RAM-2 klar getrennt. Es besitzt kein HWM-Feld; eine kleine
  Diagnoseergaenzung waere ein eigener Genehmigungsschritt (Gate `G-R7`).
- **Vollabnahme** des erfolgreichen Web-Run-Store/Checkpoint/Sensorpfads: braucht die spaetere
  Sensor-/Control-Komposition (#30/#35) und ein eigenes aktorrelevantes Gate (`G-RUN`).
  H-RAM-2 erklaert diesen Teil **nicht** zur Vollabnahme; der Standby-Teiltest (Zeilen 1–3, 5)
  ist ausdruecklich nur Teilnachweis.

#### 15.6.3 Messmethode

- **Stack-HWM ist seit Taskstart kumulativ** und gilt fuer **alle** seit dem Start des HTTPD-Tasks
  erreichten Pfade, nicht fuer die letzte Anfrage. Ein HWM-Wert wird daher **nie** einer
  einzelnen spaeteren Anfrage zugeordnet; Differenzen aufeinanderfolgender Stichproben sind nur
  neue Rekordtiefen, keine eigenstaendigen Anfragespitzen. Keine Stack-Neufuellung.
- **Zwei getrennte Diagnoseimages (non-merge):**
  1. **Stack-Diagnoseprofil** (nur HTTPD-Stack 12 288 B, Log-Overlay): liefert Stackwerte;
     seine Heapwerte gelten **nicht** als Produkt-Baseline (−4 096 B Heap gegenueber 8 192 B).
  2. **Heap-Image** (unveraenderte produktive Stackkonfiguration 8 192 B / Main 24 576 B, nur
     Log-Overlay): liefert alle Heapwerte.
  Beide mit Quell-/Build-/Image-Hash-Identitaet. Das Slice-7-Harness-Image (`G-R7`, Zeile 9) ist
  ein drittes, getrennt genehmigungspflichtiges Image.
- **Messgroessen:** freier interner 8-Bit-Heap, Low-Water-Mark, groesster freier
  zusammenhaengender interner Block (`INTERNAL|8BIT`), HTTPD- und Main-HWM (in **Bytes**),
  `heap_alloc_failed`, Resets/Watchdog/Brownout, statisches DRAM/IRAM (`size -A`), Firmware-/
  Partitionsbelegung (Image gegen App-Partition `0x2F0000`).
- **Trennung der Evidenzklassen:** Compiler (`.ci`), ELF/ROM-Aufloesung und offene indirekte
  Restkanten (statisch) einerseits, HWM (gemessen) andererseits.

**Auswertung des kumulativen HWM (verbindlich):** Fuer jeden Boot `B` wird vorab die Menge der
tatsaechlich erreichten Pfade festgelegt. Der Vergleichswert ist der **konservative Wert aller
seit Taskstart erreichten Pfade**
`R(B) = max(S_entry, S_log, S(P) fuer alle P in B) + 320 B` mit
`S_entry = 1 104 B` (HTTPD-Task-Eintritt, Accept-/Close-/Ctrl-Pfade, ohne die kontextunabhaengige
Callback-Kante; aus dem Link berechnet, [ENTRY_PATHS.tsv](../audits/PR209_Q4_HRAM2_ENTRY_PATHS_20261024.tsv)), `S_log` = Diagnose-Logpfad (Prefix bis `handleRequest`
+ `logHramHttp` + `esp_log`/`vprintf`-Kette; vor dem Lauf mit demselben Verfahren zu berechnen
und, soweit indirekt, ausdruecklich als Schaetzung zu kennzeichnen) und `S(P)` aus STACK.tsv.
`M(B) = Labor-Stack − HWM(B)`.
- `M(B) ≤ R(B)`: Modell fuer die Pfadmenge **nicht widerlegt** (kein Beweis fuer nicht erreichte
  Pfade, keine Aussage ueber indirekte Restkanten).
- `M(B) > R(B)`: **`NOT_PROVEN`** mit Untersuchung (Instrumentierung, HTTPD-/Initialisierungs-
  Vorlast, nicht modellierter Pfad, Modellfehler); **kein** pfadspezifisches `FAIL`. Ein `FAIL` fuer
  einen Einzelpfad ist nur aus einem **isolierten Einzelpfad-Boot** (B1 oder B3 in §15.6.4; B2 ist kumulativ ueber Login und GET und nicht isoliert) zulaessig, dessen
  Basislinie B0 und Logpfad die Ueberschreitung nachweislich nicht erklaeren; sonst bleibt es
  `NOT_PROVEN`.
- Eine einzelne Anfrage eines Mehrfachanfrage-Boots erhaelt **keinen** eigenen Wert.

#### 15.6.4 Szenarien: ein Boot je Pfadmenge (frischer HTTPD-Task)

Jeder Boot beginnt mit EN-Reset, wartet 60 s bis zum stabilen Serverzustand und fuehrt dann
genau die genannten Anfragen aus; der Boot-/Startzustand und die Image-Hash-Identitaet werden
im Protokoll festgehalten. Lastzustand ueberall: WLAN, LVGL im Normalbetrieb, ein Browserclient.

| Boot | Pfadmenge `B` | Zweck / Auswertung | Schreibt `state_store`? | Gruppe (Freigabe: siehe §15.7) |
|---|---|---|---|---|
| B0 | keine Anfrage, 5 min Idle | Basislinie **nur** Boot-/Heap-/Main-HWM aus `logResources()`/Bootlog; der HTTPD-HWM ist ohne Handleraufruf mit dem Overlay-Muster **nicht** messbar und wird fuer B0 nicht beansprucht, ausser die im Diagnoseumfang ausdruecklich genannte Task-HWM-Probe (30-s-Takt liest `uxTaskGetStackHighWaterMark(xTaskGetHandle("httpd"))`, nur lesend) ist genehmigt | nur falls der Boot selbst schreibt (Referenzboot, §15.6.5) | K0 (unter `G-S`, siehe §15.6.5a) |
| B1 | GET Shell **unauthentifiziert** (Login-Shell) | einzelner Pfad (isoliert); `S(GET shell)` | nein erwartet | K0 (unter `G-S`) |
| B1a | GET Asset (oeffentlich) | einzelner Pfad (isoliert); optional, kann an B1 gebunden werden | nein erwartet | K0 (unter `G-S`) |
| B1u | GET Status ohne Session (**401-Probe**) | nur Fehlerpfad; **kein** Nachweis des Read-only-Encoders | nein erwartet | K0 (unter `G-S`) |
| B3 | Login (korrektes Passwort), sonst nichts | einzelner Pfad (isoliert); `S(POST login)` | **ja** (`writeCredentials`) | K1 (`G-S`) |
| B2 | Login → authentifizierter GET Status/read-only | **nicht isoliert**: `M(B2)` nur kumulativ ueber Login **und** GET gegen `R(B2)=max(S_entry, S_log, S(login), S(GET read-only))+320` | **ja** (Login) | K1 (`G-S`) |
| B4 | Login + `sensor-selection` (abgelehnt) | tiefster abgelehnter Decision-Pfad; `R = max(S_login, S_prepare, S_confirm, S_visitor-sensor) + 320` | **ja** (Login) | K1 (`G-S`) |
| B5 | Login + die uebrigen acht Intents (abgelehnt) | konservativ ueber die Pfadmenge; kein Einzelwert je Intent | **ja** (Login) | K1 (`G-S`) |
| B6 | Login + Geraetenamen-Commit (zweimal) | Settings-/NVS-Konfigurationspfad; `R` ueber die Menge | **ja** (Login + Konfigurationsgraph) | K1 (`G-S`) |

Reihenfolge der Profile: **zuerst das 12-KiB-Stack-Diagnoseprofil** (alle Boots B0–B6), **danach**,
sofern sicher durchfuehrbar, das produktnahe 8-KiB-Heap-Profil. **K0** (B0, B1, B1a, B1u) ist die
Teilmenge ohne Login: stark reduzierte Coverage ohne authentifizierte Pfade, ohne Settings und ohne
Read-only-Encoder. **Auch K0 wird nur unter `G-S` ausgefuehrt**, weil schon der erste Diagnoseboot
(P1/B0) `state_store`/`nvs` schreiben kann (§15.6.5a); „K0 ohne `G-S`“ ist `NOT_ELIGIBLE`.

- **Heap-Lauf (8-KiB-Image):** ein durchgehender Lauf Boot → Netzwerk → LVGL → 5 min Idle → ein
  Browserclient: **ein Login** (persistenter Schreibpfad, mitzumessen) mit 5-s-Polling (die
  Session endet nach 30 min Idle, Polling erneuert sie nicht; bei laengerem Lauf zaehlt jeder
  weitere Login als Schreibzugriff) → optional Owner-Touch durch die vorhandenen Seiten (wie
  PR #174) → neun abgelehnte Mutationen → Geraetenamen-Commit → 5 min Erholung. Der
  authentifizierte Lauf gehoert zu K1; wie K0 laeuft er nur unter `G-S`. Eine
  K0-Variante des Heap-Laufs hat kein Polling der geschuetzten Routen. Heap-Tripel zu klar benannten Zeitpunkten, keine pfadweisen Stackwerte.
- Run-Persistenz/Checkpoint und Maximalkatalog sind **nicht** Teil (Zeilen 6–8, 11).
- **Abbruch:** jeder Reset/Panic/Watchdog/Brownout, jedes `heap_alloc_failed` in der Idle-/
  Polling-Phase, Geraet > 60 s nicht erreichbar, `M(B) > R(B)` (dann Stopp und Untersuchung,
  kein Weiterlauf). Es werden keine Schwellen erfunden; berichtet werden Werte mit Identitaet
  und Zeitpunkt.

#### 15.6.5 Persistenter Zustand: Sicherung, Verifikation, Rueckfall

Die Anwendung oeffnet `state_store` (Produktionspartition `0x300000`, `0x100000`, laut
`partitions/issue_90_state_store.csv`) ueber `NvsOwningContext`. **Ein App-only-Flash-Rueckfall
(`0x10000`–`0x2FFFFF`) stellt diese Daten nicht wieder her.** Persistente Bereiche und ihr
Umgang:

| Bereich | Wird beruehrt von | Behandlung |
|---|---|---|
| App `factory` `0x10000`–`0x2FFFFF` | Flash des Diagnoseimages | wie H-RAM-1: zweifach lesen, SHA-256, App-only-Flash, Readback-Hash, Rueckflash und Hash-Vergleich |
| `state_store` `0x300000`–`0x3FFFFF` | **jeder erfolgreiche Passwort-Login** (Credential-Record, `recordSequence + 1`), Geraetenamen-Commit (Konfigurationsgraph, Revisionen), bedingt der Diagnoseboot selbst (§15.6.5a) | getrennt gesichert und identifiziert (siehe unten); **nur ueber den hier definierten Rueckfall** wiederhergestellt |
| `nvs` `0x9000`–`0xEFFF`, `phy_init` `0xF000` | **Host:** nur lesende Zugriffe (`esptool read_flash`). **Firmware beim Boot:** moegliche eigene Schreibvorgaenge (`nvs_flash_init`-Bereinigung von Resten, PHY-Kalibrierung in `nvs`, §15.6.5a) | Host-Zugriffe nur lesend, Hash vorher/nachher; die firmwareinternen Schreibvorgaenge sind **nicht** ausgeschlossen, solange §15.6.5a nicht `EXCLUDED` ergibt; **kein** Rueckfall fuer `nvs` geplant, **`G-S` deckt `nvs` nicht ab** |
| Bootloader/Partitionstabelle `0x0`–`0x8FFF` | nicht beruehrt (Produktpartitionstabelle bleibt) | Hash-Vergleich vorher/nachher |

##### 15.6.5a Boot-Schreibpfad und Vor-Flash-Gate (Quellenbefund)

Quellengestuetzte Pruefung des Bootpfads bis zum stabilen Idle:
[BOOT_WRITE_PATHS](../audits/PR209_Q4_HRAM2_BOOT_WRITE_PATHS_20261024.md). Im Steady-State
(Bootstrap `Initialized`, Auth-Handoff `Consumed`, Credential-Schema v2, valide PHY-Kalibrierung,
keine NVS-Reste) enthaelt der Code **keinen** Schreibpfad. Schreiben koennen dagegen: leere
Partition/fehlender Namespace, Bootstrap `Empty`/`Initializing`/`Resetting`, Auth-Handoff
`None`/`Unconsumed`/`InProgress`, fehlendes bzw. v1-Credential, NVS-Reste unterbrochener Schreibvorgaenge
(Duplikate, `FREEING`-Seiten, verwaiste Blobs) und die PHY-Kalibrierung in `nvs` (fehlende/ungueltige
Daten, MAC- oder Versionsabweichung). Der Gerätezustand ist ohne lesendes `P0` **nicht belegt**.

```text
P1_BOOT_WRITES_STATE_STORE=CONDITIONAL    P1_BOOT_WRITES_NVS_PHY=CONDITIONAL
K0_WITHOUT_G_S=NOT_ELIGIBLE               (-> HOLD vor dem ersten Diagnoseflash ohne G-S)
```

- **Vor dem ersten Flash:** nur lesendes `P0` (doppelt, byteidentisch, SHA-256) und der
  quellengestuetzte Preflight. Zusaetzlich werden die `P0`-Abbilder von `state_store` und `nvs`
  **offline** mit den vorhandenen lesenden IDF-Werkzeugen
  (`components/nvs_flash/nvs_partition_tool/nvs_tool.py`, `nvs_check.py`) klassifiziert
  (Bootstrap-/Handoff-Zustand, Credential-Schema, NVS-Reste, `phy`-Namespace inkl.
  `cal_mac`/`cal_version`); die Klassifikation ist Evidenz, kein Freibrief und macht K0 **nicht** zu
  `ELIGIBLE`. Der Diagnoseflash mit anschliessendem Boot P1/B0 wird nur ausgefuehrt, wenn (a) `G-S`
  vorbereitet und freigegeben ist **und** (b) die folgende Vor-P1-Entscheidungsbedingung fuer `nvs`
  nicht auf `HOLD` steht. Es gibt keine Freigabe, die erst **nach** P1 festgestellte
  Schreibzugriffe nachtraeglich genehmigt.

**Vor-P1-Entscheidungsbedingung fuer die System-NVS (`nvs`).** `G-S` deckt ausschliesslich
`state_store` ab. Es gilt **weder** als Freigabe fuer Aenderungen an `nvs`/`phy_init` **noch** als
Rueckfall dafuer. Ob der Diagnoseboot die System-NVS beschreiben kann, wird deshalb **vor** dem
ersten Flash aus dem lesenden `P0` und der tatsaechlich geplanten Diagnoseimage-/PHY-Konfiguration
entschieden und im Vor-P1-Bericht (`PRE_P1_REPORT`) festgehalten:

| Bedingung (alle muessen belegt sein) | Nachweis |
|---|---|
| `nvs`-Abbild integer, keine NVS-Reste (keine Duplikate, keine `FREEING`-Seite, keine verwaisten/inkonsistenten Blobs), Standardpartition nicht leer | `nvs_tool.py`/`nvs_check.py` auf dem `P0`-Abbild |
| Namespace `phy` mit gueltigen Kaldaten vorhanden; Laenge passend; `cal_mac` gleich der Chip-MAC | `nvs_tool.py`-Auszug gegen die lesend ermittelte MAC |
| PHY-Kalibrierformat des Diagnoseimages gleich dem der installierten Firmware (`cal_version`-Pruefung in `esp_phy_load_cal_and_init`) | dokumentierter Build des installierten Images (Quell-SHA, IDF-Commit) gleich dem Diagnoseimage, und gleiche PHY-Bibliothek (Hash von `libphy.a` bzw. `phy_version`-Bootzeile aus vorhandenen Logs); sonst nicht belegt |
| PHY-Konfiguration des Diagnoseimages unveraendert (`CONFIG_ESP_PHY_CALIBRATION_AND_DATA_STORAGE=1`, `RF_CAL_PARTIAL`) | `sdkconfig` des Diagnoseimages gegen die bekannte Produktkonfiguration |

```text
SYSTEM_NVS_WRITE_AT_P1 = EXCLUDED | NOT_EXCLUDED | NOT_PROVEN        (derzeit: EXCLUDED laut PRE_P1_REPORT 2026-10-24)
EXCLUDED   -> P1 darf unter G-F + G-S laufen (Restunsicherheit nicht untersuchter IDF-Interna im Bericht benannt)
sonst      -> P1=HOLD vor Flash/Boot, bis der Owner den moeglichen System-NVS-Effekt und dessen
              Umgang ausdruecklich entschieden hat (z. B. Akzeptanz einer moeglichen PHY-Kalibrier-
              speicherung ohne Restore, oder eine eigens zu erbittende Freigabe fuer einen
              `nvs`-Rueckfall). Diese Entscheidung ist in `G-S` nicht enthalten.
```

**Ergebnis `PRE_P1_REPORT` (P0-Preflight 2026-10-24, nur lesend, Owner-Freigabe nur fuer diesen Schritt):**
[PRE_P1_REPORT](../audits/PR209_Q4_HRAM2_P0_PRE_P1_REPORT_20261024.md) – `P0_BYTE_IDENTICAL=YES`
(Boot/`nvs`/`phy_init`, Factory-App und `state_store` je zweimal unabhaengig gelesen, SHA-256 im Bericht;
Boot-Bereich und App hash-gleich zu H-RAM-1), offline: `nvs` integer ohne Reste, `phy`-Kaldaten gueltig,
`cal_mac` = Chip-MAC, PHY-Bibliothek/IDF-Pin/Konfiguration gleich dem installierten Image;
`state_store`: Bootstrap `Initialized`, Handoffs `Consumed`, Auth `Provisioned`, Credential v2, Run-Head
`Committed`/`NoActiveRun`, keine NVS-Reste → **`SYSTEM_NVS_WRITE_AT_P1=EXCLUDED`** (Voraussetzungen und
Restunsicherheit im Bericht: gilt nur fuer Diagnoseimages mit gleichem IDF-Pin/`sdkconfig`-Basis, beim Bau
erneut zu vergleichen; `phy_get_rf_cal_version()` des Diagnoseimages ist nicht ausgefuehrt belegt).
`EXCLUDED` ist **keine** Freigabe: `P1=HOLD`, bis `G-F` und `G-S` ausdruecklich freigegeben sind; die
Hashkette nach P1 bleibt verbindlich.

**Sprachliche Trennung (verbindlich):** **geplante Host-Zugriffe** auf `nvs`/`phy_init` sind nur
lesend; **moegliche firmwareinterne Schreibvorgaenge** beim Boot sind davon unabhaengig. Weder
Schreibfreiheit noch ein Rueckfall fuer die System-NVS wird behauptet, solange sie nicht belegt bzw.
genehmigt sind.

- **Nach P1 (und nach jedem weiteren Boot):** `state_store`-Hash gegen den jeweiligen Vorgaenger
  vergleichen, `nvs`/`phy_init` getrennt. Bei veraendertem `state_store`: keine weiteren K0-/K1-
  Boots, bevor Abweichung (Entry-Diff per `nvs_tool.py`), byte-exakter Rueckfall (RB-2) und
  definierte Ausgangsbasis geklaert sind. Bei unveraendertem Hash: keine pauschale
  Schreibfreiheits- oder Vollabnahmebehauptung fuer andere Zustaende/Boots.
- **Vergleichbarkeit (Hashkette):** jede Messung nennt ihren Ausgangs-Hash (`P0` oder den
  ausdruecklich dokumentierten Vorgaengerzustand). Erwartete Aenderungen (Login:
  `recordSequence + 1`, Namenscommit: Konfig-Records) werden per Entry-Diff klassifiziert. Ein
  Restore auf `P0` (RB-2) ist erforderlich: bei unerwarteter Abweichung, vor einer Messung, die
  `P0` als Ausgangsbasis braucht, und am Ende der K1-Serie. Keine stillschweigende Uebernahme
  eines durch P1, Login oder Commit veraenderten Zustands als neue Baseline.
- **`nvs`/`phy_init` nach P1:** Das `nvs`-/`phy_init`-Hashdelta wird getrennt gegen `P0` ausgewertet.
  Bei einem Delta gilt **Stopp**: kein stillschweigender Baselinewechsel, keine weiteren Boots, kein
  nicht autorisierter Restore oder Erase dieser Partitionen; die Abweichung wird berichtet (Entry-Diff),
  und der Owner entscheidet ausdruecklich ueber Umgang und Fortsetzung. Ein Rueckfall fuer die
  System-NVS ist nicht geplant und nicht durch `G-S` gedeckt.

**Vor der ersten Messung (nur lesen, `P0`):** Geraeteidentifikation (Chip/MAC/Flash-ID,
Boot-Quell-SHA); `0x0`–`0xFFFF`, App und `state_store` je **zweimal** auslesen, beide Lesungen
identisch, SHA-256 je Bereich. Der **logische Zustand** (Geraetename, Konfigurationsrevisionen,
Netzwerkmodus, Touchkalibrierung vorhanden) wird **ausschliesslich offline aus den
Sicherungsabbildern** gewonnen (vorhandene lesende NVS-Werkzeuge). **Kein Web-/API-Zugriff und
kein Login** fuer `P0`: jeder erfolgreiche Login schreibt `state_store` und ist damit kein lesender
Zugriff; ebenso kein Boot der Firmware. Felder, die sich ohne persistenten Schreibzugriff nicht
aus den Abbildern ermitteln lassen, werden als `NICHT_ERMITTELT` gekennzeichnet; es wird keine
zusaetzliche Instrumentierung nur hierfuer gebaut. Die Sicherungsabbilder
bleiben **privat lokal** (sie enthalten WLAN-/Zugangsdaten) und werden nie committet;
committet werden nur Hashes und bereinigte Protokolle.

**Referenzboot (`P1`, nur nach erfuellter Vor-P1-Bedingung fuer die System-NVS und freigegebenem `G-S`):** nach dem Flash des Diagnoseimages ein Boot mit 5 min Idle ohne Anfrage
(= B0); danach `state_store`, `nvs`/`phy_init`, Bootbereich lesen und mit `P0` vergleichen.
Das ergibt, ob schon der Boot persistent schreibt, bevor Szenarien laufen.

**Rueckfallweg (verifizierbar, zweistufig):**
- **RB-1 (logisch, ohne Zugriff auf `state_store`):** erlaubt nur fuer Szenarien, die den
  Namen aendern, und nur wenn (i) Originalname und Revisionen aus dem `P0`-Abbild (offline) ermittelt und dokumentiert sind (sonst ist RB-1 nicht verwendbar und nur RB-2 gilt),
  (ii) jeder Commit durch GET und Revision bestaetigt wird, (iii) der Rueckcommit des
  Originalnamens bestaetigt ist. Rohbytes von `state_store` weichen dabei erwartbar ab
  (Revisions-/Slotrotation); die Abweichung wird per Vorher/Nachher-Hash **sichtbar
  klassifiziert** (keine / erwartete Konfig-Record-Rotation / unerwartet), nicht verschwiegen.
  **RB-1 allein ist fuer den Abbruch-/Unerreichbarkeitsfall nicht ausreichend** (ein Web-Rueckcommit
  ist dort nicht moeglich).
- **RB-2 (byte-exakt, physisch):** Zurueckschreiben des gesicherten `state_store`-Abbilds
  (`0x300000`, 1 MiB; esptool mit Erase) und des App-Abbilds, danach Readback-Hash gleich `P0`.
  Das ist ein **physischer Erase-/Schreibzugriff auf eine Produktpartition** und braucht die
  **separate Ownerautorisierung `G-S`**.
- **Regel:** **Jeder authentifizierte Boot (B2–B6) und der authentifizierte Heap-Lauf starten nur,
  wenn `G-S` erteilt ist** und RB-2 mit den vorab verifizierten, doppelt gelesenen Backups
  bereitsteht; der Rueckfall RB-1 allein genuegt dafuer nicht (ein Login ist per Web nicht
  rueckgaengig zu machen, `recordSequence` steigt monoton). **Ohne `G-S` laeuft nichts am Geraet:** schon der erste Diagnoseboot (P1/B0) kann schreiben (§15.6.5a); K0 und K1 sind beide `HOLD` vor dem Flash. Nach jedem Ablauf wird `state_store` per Hash mit dem dokumentierten Vorgaenger (Hashkette, §15.6.5a) verglichen; eine unerwartete Abweichung stoppt die Messung und wird gemeldet, es folgt **kein** automatischer Restore ohne `G-S`.
- **Abbruchfall (Bootloop, unerreichbare Weboberflaeche, Reset):** UART-Beleg → App-Rueckflash
  (`G-F`) aus dem verifizierten Backup; da jeder Login `state_store` veraendert hat, folgt RB-2
  (`G-S`) mit Readback-Hash gleich `P0` – dieser Weg braucht weder Weboberflaeche noch laufende
  Firmware (esptool im Bootloader-Modus) und ist deshalb auch im Bootloop gangbar.
- Keine unnoetigen Zugriffe auf fremde Partitionen: nur App (und mit `G-S` `state_store`) werden
  geschrieben; `nvs`, `phy_init`, Bootloader und Partitionstabelle werden **vom Host nur gelesen** (moegliche firmwareinterne `nvs`-Schreibvorgaenge beim Boot: siehe Vor-P1-Bedingung in §15.6.5a).

#### 15.6.6 Reserve fuer das noch unfertige R1 (keine Bytewerte)

| Klasse | jetzt integriert | owner-bestaetigter R1-Umfang, noch offen | Ownerentscheidung offen / zurueckgestellt |
|---|---|---|---|
| Tasks/Stacks | Main (24 576 B), HTTPD (8 192 B), WLAN/lwIP/Systemtasks | DS18B20-Sampling-Task (#30/#190, Budget gesperrt), Regel-/Control-Komposition mit Planner und Sensorevidenz (#35, `TBD_COMMISSIONING`) | – |
| Residente Modelle | Konfig-/Programmkatalog, Run-State, Web-Sessions, LVGL-Pool 48 KiB (O4-Referenz), lokaler Werksreset (#19 R1-Umfang) | Sensor-/Regelzustand (#35), Ausgangs-/H-Bruecken-Verifikation (#32/#33, Hardware) | #28 Diagnose/Diagramme/Exporte (`PLANNED_SPEC_PENDING`, Abhaengigkeit von #19 vom Owner zu klaeren); #19-Journale/Aufbewahrung/Backup/Import (in #19 zurueckgestellt) |
| Temporaere Allokationen | Snapshot-/Katalog-/JSON-Kodierung (M-H1: Einzelprogramm nun exakt) | durch #35 | Export-/Backup-/Journalpfade (#28/#19, nicht bestaetigt) |
| Flash/Partitionen | Release-Image 1 774 368 B in der 3 080 192 B grossen App-Partition (HEAD `1c56b4d`), `state_store` 1 MiB | durch #35 | Journale/Export (nicht bestaetigt); ausserhalb R1: OTA/zweiter Slot (nicht vorgebaut) |

Nicht in R1 vorgebaut (`SPECIFICATION_REVIEW`): OTA, UART-Diagnose, Telegram/Push u. a. Fuer nicht
implementierte Funktionen werden **keine Bytewerte erfunden, keine Reserve fiktiv reserviert und
keine endgueltigen harten R1-Gesamtlimits** gesetzt (O4:
`FINAL_HARD_SYSTEM_LIMITS=DEFERRED_TO_FINAL_R1_INTEGRATION_QUALIFICATION`). Die LVGL-48-KiB-
Referenz und ihre Messgrenzen (Punktmessungen, kein kontinuierliches Minimum) bleiben
massgeblich; nach wesentlichen spaeteren Integrationen (#35 und, falls der Owner sie bestaetigt,
#28/#19-Teile) wird die Ressourcenmessung mit RAM-Delta-Pruefung wiederholt.

#### 15.6.7 Entscheidungsregeln

Technischer `PASS` nur bei tatsaechlicher Pfadabdeckung **und** begruendeter Reserve; sonst
`PARTIAL`, `NOT_PROVEN` oder `FAIL` mit Beleg.

- `HTTPD_FULL_CHAIN_STACK`: aus H-RAM-2 hoechstens `PARTIAL`; die statisch ungeloesten indirekten
  Aufrufe und der nicht messbare Run-Store-Pfad werden **nicht** per Schaetzung oder
  Risikoakzeptanz zu `PASS` erklaert. Eine Owner-Risikoakzeptanz wird getrennt dokumentiert.
- `HTTPD_STACK_RESERVE`: bleibt `FAIL` gegen den 512-B-Planungsboden, solange der aufgeloeste Rest
  432 B betraegt; eine Aenderung des Planungsbodens waere eine eigene Ownerentscheidung.
- Heap-Gates: `PASS` nur, wenn der Heap-Lauf fuer die erreichbaren Pfade ohne
  `heap_alloc_failed` laeuft und Minimum sowie groesster Block nachvollziehbar gegen die
  R1-Referenz (§15.6.6) begruendet sind; der nicht erreichbare Programmsnapshot-/Checkpoint-/
  Maximalkatalogteil bleibt `NOT_RUN/NOT_PROVEN`.
- **8 192 vs. 8 704 B** wird erst zusammen mit dem im Heap-Lauf belegten Effekt von −512 B auf
  freien Heap und groessten Block bewertet; kein praeventiver Umbau.

### 15.7 Ownerentscheidung und Freigabegates

**Ownerrichtung (bestaetigt, keine ausfuehrbare Freigabe):** `H-RAM-2` ist als Vorgehen
grundsaetzlich gebilligt und soll nach erfolgreicher Plan-Fix-Verification durchgefuehrt werden;
dem zusaetzlichen, separat autorisierten byte-genauen `state_store`-Rueckfall (`G-S`) stimmt der
Owner zu; `G-R7` und `G-RUN` sind **nicht** freigegeben. **Diese Richtung erteilt keine Hardware-,
Flash-, Reset-, NVS-Erase- oder Schreibfreigabe.** Die ausfuehrbare Freigabe folgt separat und
ausdruecklich auf der dann aktuellen Plan-SHA nach bestandener Fix Verification.

**Verbindliche Bedingungen (Owner):**
1. Vor **jedem** Flash- oder NVS-Schreibversuch zwei unabhaengig ausgelesene, byteidentische und
   hash-verifizierte Backups der betroffenen App- und `state_store`-Partitionen (§15.6.5 `P0`).
2. Verifizierbarer Rueckfall auch bei Bootloop oder unerreichbarer Weboberflaeche (esptool im
   Bootloader-Modus, unabhaengig von der Firmware).
3. Keine Aktorfreigabe, keine Safety-Umgehung, kein destruktives Harness.
4. Keine vorsorglichen Aenderungen an Stackgroessen, Partitionen oder Speicherarchitektur.
5. Keine Behauptung einer vollstaendigen RAM-Freigabe aus den Teilmessungen.
6. Die noch ausstehenden R1-Funktionen bleiben Teil der spaeteren Gesamtqualifikation (§15.6.6).
7. Reihenfolge: zuerst Stack-Diagnoseprofil (12 KiB), danach, sofern sicher, das produktnahe
   Heap-Profil (8 KiB).

**Zwei Zugriffstypen, logisch gekoppelt (zur ausdruecklichen Freigabe vorzulegen, noch nicht
erteilt):**
- **Teil K0 – ohne Login (B0, B1, B1a, B1u), ebenfalls nur unter `G-S`:** zwei non-merge
  Diagnoseimages auf Produktstand `80bfb28`; `G-F` app-only Flash `0x10000`–`0x2FFFFF` mit zwei
  byteidentischen Backups und Hash-verifiziertem Rueckflash; `G-U` UART; `G-R` EN-/BOOT-Reset;
  `G-B` HTTP; Lesezugriffe auf `0x0`–`0xFFFF`, App und `state_store` (`P0`, offline-
  Klassifikation, Hashkette). Die Szenarien selbst schreiben nicht, aber **der Diagnoseboot
  (P1/B0) kann `state_store`/`nvs` schreiben** (§15.6.5a, `K0_WITHOUT_G_S=NOT_ELIGIBLE`); deshalb
  wird `G-S` bereits **vor dem ersten Diagnoseflash** vorbereitet und freigegeben; zusaetzlich muss die Vor-P1-Bedingung fuer die System-NVS (§15.6.5a) `EXCLUDED` ergeben oder vom Owner ausdruecklich entschieden sein (`G-S` deckt `nvs` nicht ab). Ergebnis: stark
  reduzierte Coverage (unauthentifizierter Shell/Asset, Boot-/Heap-/Main-Basislinie, 401-Probe).
- **Teil K1 – authentifizierter Kern (B2–B6, authentifizierter Heap-Lauf):** ebenfalls `G-S`
  (physischer Erase-/Schreibzugriff auf `state_store`, `0x300000`, 1 MiB, byte-exakter Rueckfall
  RB-2 mit Readback-Hash gleich `P0`), weil zusaetzlich jeder erfolgreiche Login den
  Credential-Record schreibt. Kein authentifizierter Lauf und **kein `H-RAM-2`-Lauf (auch nicht K0)
  ohne freigegebenen Recovery-Pfad**. Die Szenarioauswahl (zuerst K0, dann K1) bleibt moeglich;
  `G-S` ist fuer beide dieselbe Voraussetzung.

**Zulaessige Szenarien und Abdeckung:**

| Szenario | ohne `G-S` | mit `G-S` | Abdeckung |
|---|---|---|---|
| P0 lesende Sicherung + offline-Klassifikation + `PRE_P1_REPORT` | ja (nur Host-Lesen, kein Boot der Firmware, kein Login) | ja | Ausgangszustand, Entscheidung zur System-NVS |
| Diagnoseflash + P1/B0 Boot-/Heap-/Main-Basislinie | **HOLD (nicht zulaessig)** | ja, **nur** bei `SYSTEM_NVS_WRITE_AT_P1=EXCLUDED` oder ausdruecklicher Ownerentscheidung zur System-NVS; sonst `HOLD` | Boot, Heap, Main-HWM (HTTPD-HWM nur mit genehmigter Task-HWM-Probe) |
| B1 / B1a unauthentifizierter Shell / Asset | HOLD | ja | Einzelpfade, isoliert |
| B1u 401-Probe Status | HOLD | ja | nur Fehlerpfad, kein Encoder |
| B3 Login | HOLD | ja | Einzelpfad + persistenter Schreibpfad |
| B2 Login → authentifizierter Status | HOLD | ja | kumulativ ueber Login+GET |
| B4 / B5 abgelehnte Intents | HOLD | ja | Prepare/Confirm/Decision nur Ablehnung |
| B6 Namenscommit | HOLD | ja | Settings-/Konfigurations-NVS |
| Authentifizierter Heap-Lauf | HOLD | ja | Teilnachweis Normalbetrieb |
| Run-Store, Checkpoint, RAM-only-Sensor, Maximalkatalog | `NOT_RUN` | `NOT_RUN` | siehe §15.6.1 |

**Ausfuehrbarer Umfang auf einer neuen Plan-SHA (nach Fix Verification dieses Deltas):**
`H-RAM-2` (K0 und K1, Stack-Diagnoseprofil zuerst, danach Heap-Profil) zusammen mit `G-F` und `G-S`
als **gemeinsam** zur ausdruecklichen Freigabe vorzulegender Umfang; der erste Diagnoseflash/Boot bleibt zusaetzlich an den `PRE_P1_REPORT` zur System-NVS gebunden; `G-U`/`G-R`/`G-B` wie
beschrieben. Ein Teilumfang „nur K0 ohne `G-S`“ wird nicht angeboten.

**Nicht erbeten:** `G-R7` (Slice-7-Harness, destruktiv), `G-RUN` (erfolgreicher Run-Pfad,
aktorrelevant, an #30/#35 gebunden), Maximalkatalog-Hilfe. Bis zu den ausdruecklichen Freigaben
keine Produkt-, Build-, Hardware-, Flash- oder NVS-Aktion. `PR209_MERGE=HOLD` bis zum
Ressourcenabschluss.

### 15.8 M-H1 Umsetzungsergebnis

Auf Plan-SHA `6a311d6174999e864c652e63a71f05271a49f5b3` umgesetzt (Produkt `80bfb28`): Bytes,
Statuswerte und `out`-Semantik gegen die Referenz `76cb4ce` identisch; groesste Einzelanforderung
der Einzelprogrammkodierung 32 769 B → Dokumentgroesse + 1; Web-Mutationsfixtures (Host) groesste
Anforderung 32 769 → 8 193 B (8-KiB-Run-Writer bleibt), Peak live 45 094 → 14 937 B. Stackbilanz
unveraendert (Maximum 7 440 B, Rest 432 B – Stand M-H1, durch das korrigierte Modell in §15.9 ueberholt: 7 664 B, Rest 208 B). Heap-Gates `NOT_PROVEN`, `C5_RESOURCE_GATE=HOLD`.
Details und Korrektur der Plan-Zahlen (Maximum 1 255 B): [Evidence](../audits/PR209_Q4_MH1_20261024.md).
Hardwaregate und Stackgroesse bleiben offen (§15.5/§15.6).

**Reviewstatus M-H1:** `M_H1_FIX_VERIFICATION=PASS`, `M_H1_OPEN_BLOCKERS=0` (Independent Review auf
HEAD `1c56b4de623ab22ae9ae31a1b805ba5cea4e6c4a`, Produktcommit `80bfb28`); keine Wiederholung des
Full Reviews; kein Merge-GO.

### 15.9 H-RAM-2 Ergebnis (ausgefuehrt, Teilnachweis)

Ausgefuehrt am 2026-10-24 auf HEAD `4afc6266` unter der Ownerfreigabe in PR-Kommentar 6383231599
(`G-F`, `G-S`, `G-U`, `G-R`, `G-B`; nicht `G-R7`/`G-RUN`). Rohdaten privat lokal; Hashes, bereinigte
Messwerte und Skripte in
[PR209_Q4_HRAM2_RESULTS_20261024](../audits/PR209_Q4_HRAM2_RESULTS_20261024.md).

- **Stack (12 KiB, ein Boot je Pfadmenge):** B0 928 B, B1 3 668 B, B1u 3 256 B, B3 4 724 B, B2 5 940 B,
  B4 5 940 B, B5b 6 004 B, B6 6 672 B (`M(B)` = 12 288 − HWM). Alle ausser **B3** innerhalb `R(B)`.
- **B3-Abweichung:** 4 724 B gegen 4 112 B (+612 B). Ursache: Modelllucke (`-mlongcalls`: `l32r`+`callx8`-Kanten
  fehlten in der ELF-Aufloesung). Modell korrigiert: Maximum **7 664 B** (statt 7 440 B), mit 320 B Zuschlag
  7 984 B, Rest zu 8 192 B = **208 B**; zwei virtuelle Aufrufe bleiben unaufgeloest.
- **Heap (8 KiB, authentifiziert, 15 min):** kleinster freier interner Heap 22 428 B, kleinster groesster Block
  22 528 B, kleinster HTTPD-Stack-Rest 1 724 B (Nutzung 6 468 B); 120/120 Pollinggruppen gueltig, kein
  `heap_alloc_failed`/Panic.
- **Persistenz:** Boot-Bereich (`nvs`/`phy_init`) nach jedem Boot gleich `P0` → `SYSTEM_NVS_WRITE_AT_P1=EXCLUDED`
  bestaetigt. `state_store` aenderte sich nur durch Login (`auth0`) und Namenscommit (Konfigurations-Records).
  RB-2 und App-Rueckflash byteidentisch zu `P0`; **Kontrollboot der wiederhergestellten Firmware `NOT_RUN`**
  (nur Byte-Gleichheit belegt, nicht die funktionale Startfaehigkeit).
- **Nicht gelaufen:** erfolgreicher Run-Pfad, Run-Store, Checkpoint, Maximalkatalog (`NOT_RUN`); der statisch
  tiefste Pfad ist damit nicht gemessen.

Gates: `HTTPD_FULL_CHAIN_STACK=NOT_PROVEN`, `HTTPD_STACK_RESERVE=NOT_PROVEN`, `ESP32_HEAP_PEAK=PARTIAL`,
`ESP32_LARGEST_FREE_BLOCK=PARTIAL`, `C5_RESOURCE_GATE=HOLD`, `PR209_MERGE=HOLD`, `ACTUATOR_RELEASE=NO`.
Eine Stackgroessen- oder Partitionsaenderung ist **nicht** vorgeschlagen und bleibt Ownerentscheid nach dem
unabhaengigen Review.
