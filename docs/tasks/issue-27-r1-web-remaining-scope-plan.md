# Issue #27 – R1-Web-Restumfang (integrierter Gesamtplan, ein Draft-PR)

**2026-10-20 · PLAN-REVISION 1 · NICHT FREIGEGEBEN · UMSETZUNG GESPERRT (Ownerentscheide D1–D9 offen).**
[Issue #27](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/issues/27)
bleibt OPEN. Branch `agent/issue-27-r1-web-navigation-plan` (Name historisch), Draft-PR
[#209](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/pull/209). Dieser Plan ist
die einzige kanonische Fassung; er **ersetzt** den frueheren reinen Navigationsplan
(Plan-SHA `0614c054e3303ef2aacd495b8221e512eb4c0714`, historisch; dessen Messbelege bleiben
unter `audits/WEB_NAV_BUDGET_20261020/` erhalten). Eine Umsetzung beginnt erst nach
unabhaengigem Planreview und ausdruecklicher Freigabe der exakten Plan-SHA.

**Kernaussagen.**

1. **Ein PR** ist moeglich und empfohlen (C0–C5, §7). Abgetrennt (anderes Issue / fehlende
   Spezifikation / Hardware- bzw. Ownerabhaengigkeit, §3) sind: Manueller Betrieb,
   Diagnose/Diagramme/Exporte (#28), Journale/Protokolle/Backup/Werksreset (#19),
   Service-Funktionen, Webprogramm-Editor, Favicon (#204). Nichts davon wird als erledigt
   oder entfallen bezeichnet.
2. **Ressourcen:** Der Shell-Anhang hat 16 B Spielraum; schon Navigation + Meldungen kosten
   +601 B. Die kleinste tragfaehige Auslieferung fuer *alle* weiteren Seiten ist ein
   **byte-konstanter statischer Script-Asset** mit vorhandenen ESP-IDF-/HTTPD-Mitteln
   (Empfehlung, D1). Das ist ein neuer Route-/Cache-/Fehlervertrag und damit eine
   Ownerentscheidung; ohne sie ist nur die Navigation mit 1 B Reserve darstellbar und der
   uebrige Restumfang nicht.
3. **Keine** Hardware-, Flash-, Browser-, UART-, Reset- oder Aktorfreigabe;
   `ACTUATOR_RELEASE=NO`, `RAM_PASS=NOT_PROVEN`.

## 1. Live-Basis

- `origin/main` = `086873ac3a6ea0d8134810a744e61862e0528a6a` (Merge PR #208,
  2026-10-20T15:13:41Z). Erste Branch-Aenderung `ac037e8`: #208 als `MERGED` in der
  [Roadmap](../ROADMAP.md).
- Gemergter Web-Stand auf `main`: #170 Foundation (Auth/Session/CSRF/Replay, Read-only-API,
  Setup-Pfad), #201 Run-Mutationsroute, #203 Dashboard/F1/F4/Single-Flight, #205
  Mutationsheader, #206 Laufinformationen, #207 Standbyinformationen, #208 Sprachwahl.
- Shell heute: authentifiziert **11 312 B** (Kopf 1 515 B + Suffix 9 797 B) bei Limit
  **11 456 B** = 128 B Normalreserve + **16 B** Spielraum; Kopf mit maximaler Sequenz 1 534 B
  (Testlimit 1 536 B).
- Unveraenderlich bindend: Session/CSRF/Mutationssequenz/Replay, 5-s-Polling mit
  Single-Flight und 30-s-Frist, 15-s-Frische, Stale-/Offline-/Bestaetigungsgates, DE/EN/ES,
  `localStorage.lang`, Displayunabhaengigkeit, Application als einziger Fach-Owner.
  Offene Gates: F4 PROVISIONAL, G1′ storniert, G2_WEB/R1-U/D9 offen,
  `ACTUATOR_RELEASE=NO`.

Quellen: [WEB_UI](../WEB_UI.md), [SPECIFICATION_REVIEW](../SPECIFICATION_REVIEW.md),
[LOCAL_UI_SETTINGS_SERVICE](../LOCAL_UI_SETTINGS_SERVICE.md),
[LOCAL_RUNTIME_UI](../LOCAL_RUNTIME_UI.md), [RUN_COMMANDS](../RUN_COMMANDS.md),
[PROGRAMS](../PROGRAMS.md), [NETWORK](../NETWORK.md),
[ADR-013](../ADR-013_REUSABLE_DEVICE_PLATFORM.md), [CI/Gates](../CI_AND_QUALITY_GATES.md),
[Workflow](../AGENT_WORKFLOW.md), [Engineering](../ENGINEERING_PRINCIPLES.md),
Evidence #203/#206/#207/#208.

## 2. Wiederverwendbarer Bestand (belegt)

| Baustein | Beleg |
|---|---|
| Read-only-API | `GET /api/v1/status` (Revisionen, Lauf-/Standby-/Netzwerk-/Zeitinfo), `/temperatures`, `/alerts` (id, code, severity, active, acknowledged, resolved, decisionRequired, muted, revision; ≤ 16), `/programs` (nur `id`/`name`, `catalogRevision`) |
| Mutationsroute | `POST /internal/ui/run`, geschlossene Intents im Codec: `start-program` (mit naechster-Lauf-Anpassung `x,d,h,s,c,k,l`), `stop-run`, `complete-run`, `ack-message`, `mute-message`, `adjust-run`, `recovery-time-correction`, `reset-fault`, `sensor-selection`, `start-manual-timed`, `start-manual-holding`; Application entscheidet (Revisionen, Replay, Outcome-Matrix) |
| Shell-Nutzung | genutzt: `start-program` (nur Programm-ID), `stop-run`, `complete-run`, `ack/mute-message` (erste passende Meldung) |
| Sitzung | `WebSessionManager` mit Service-Lease (`grantServiceLease`, `serviceLeaseStatus`, `revokeServiceLease`, 5 min/15 min); **keine** HTTP-Route und kein Statusfeld dazu |
| Application-Kommandos (Touch) | `SetDisplayLanguage`, `SetDeviceName`, `ApplyNetworkMode`, `BeginHomeWifiReconfiguration`, `OpenWebProvisioningWindow`, `VerifyServicePin`, `ConfigurationCommit`; `FactoryReset` ist **nicht** Teil der Operation-Variante („never reachable from a web or other non-local surface“) |
| Auslieferung | Kopf (Heap-String) + `staticBodySuffix` (Flash, Chunk-Send ohne Heap-Kopie); ein Wildcard-`httpd_uri_t`, Routing in `WebRouteDispatcher::handle`; Antwort-Metadaten nur `Set-Cookie`/`Retry-After`; `request.path` enthaelt die Query |

## 3. Restumfang-Statusmatrix

Legende: **U** umgesetzt · **T** teilweise · **F** fehlt (in diesem PR geplant) ·
**X** anderes Issue · **H** hardware-/ownerabhaengig, nicht ohne Entscheid planbar.

| Bereich (WEB_UI) | Status | Beleg / Abgrenzung | Plan |
|---|---|---|---|
| Responsive Basis, einspaltig/zweispaltig | **U** | #203, Shell-CSS | unveraendert |
| Anmeldung, Logout, Setup, Sitzungsregeln, kein Dauerlogin | **U** | #170, #203 | unveraendert |
| Sprache je Browser (DE/EN/ES) | **U** | #208 | neue Texte in allen Slices |
| Live-Aktualisierung (5-s-Polling, Stale/Offline) | **U** | #203 | Push/SSE bleibt offen (Spezifikation laesst es offen), nicht Teil |
| **Übersicht** – Standby (Name, Temperaturen, Meldungen, Netzwerk/Zeit, Start) | **U** | #207, #203 | Navigationseintrag C1 |
| Übersicht – laufender/abgeschlossener Prozess (Programm, Phase, Soll, Zeit, Abschluss, Stop, Abschluss quittieren) | **U** (phasenbezogene Zeit, D2-A) | #206, #201 | – |
| Übersicht – `Details` (Sensorstatus, Sollwert, Vorheizstatus) | **F/X** | LOCAL_RUNTIME_UI; benoetigt Sensor-/Aktordetail-API | als Teil Diagnose (#28), nicht hier |
| Übersicht – optional neuer manueller Kuehllauf | **H** | `start-manual-*`/Kuehlplaene liefern 503 ohne Grenzen-Owner (Test O5/#172 S9; #35) | nicht planbar |
| **Programme** – Auswahl + Start (nur ID) | **T** | `pg`-Auswahl, `start-program` | C2 |
| Programme – Details/Zusammenfassung, Anpassung fuer den naechsten Lauf | **F** | Codec kennt `x,d,h,s,c,k,l`, Application wendet an; API liefert keine Programmdetails | C2 |
| Programme – Benutzerprogramme anlegen/aendern/loeschen, Ein-/Ausschalten | **H** | WEB_UI spezifiziert es nicht; Touch hat Editor; neue Mutationsfamilie ueber Konfigurations-Commit | Ownerentscheid D6 |
| **Manueller Betrieb** | **H** | siehe oben; abhaengig #35 (Grenzen) und Hardware | nicht planbar, bleibt offen |
| **Meldungen** – Liste, Quittieren/Stumm je Meldung | **T** | Aktionsleiste nur fuer die erste Meldung | C1 |
| Meldungen – Titel/Zeitpunkt/Detail je Meldung, **Protokolle** | **X** | API liefert nur Code/Zustand; Journal/Historie ist #19 (DEFERRED) | nicht hier |
| **Einstellungen** (Sprache, Zeit/Zeitzone, Geraetename, Netzwerk, Webzugang) | **F** | Touch hat alle sechs; Web keine | C3 (Umfang D5) |
| **Diagnose**, Diagramme, Exporte | **X** | Issue #28 (`PLANNED_SPEC_PENDING`, Abhaengigkeit #19) | nicht hier |
| **Service** – PIN-Freigabe je Websitzung, Sperre, Abmeldung | **T** | Session-Lease vorhanden, keine Route/Status/UI | C4 nur bei Nutzer (D7) |
| Service – Funktionen (Aktortests, Sensorzuordnung, Parameter, Kalibrierung) | **X/H** | #28, #19, #32/#33 (Hardware) | nicht hier |
| **System** (Version, Ressourcen, Backup/Import, Werksreset) | **X/H** | Backup/Import und Journal #19; Werksreset ist nach Code und LOCAL_UI_SETTINGS_SERVICE nie per Web ausloesbar, WEB_UI nennt ihn dennoch als Beispiel einer Web-Aktion (Widerspruch, D8) | nicht hier |
| Temperaturdiagramm des aktuellen Laufs | **X** | #28 (aktueller Laufchart) | nicht hier |
| Konfliktdarstellung bei konkurrierender Bedienung | **T** | Revisionsschutz in der Application, UI zeigt nur „Nicht ausgefuehrt“ | C5 (kleine, beschriftete Outcome-Anzeige) |
| Favicon `GET /favicon.ico` | **X** | Issue #204 | nicht hier |

Konsequenz: Dieser PR schliesst **Issue #27 nicht automatisch**. Der verbleibende Rest
(Zeilen H/X) wird in Roadmap und Issue ausgewiesen; das Issue wird erst nach gesamtem
genehmigten Umfang oder ausdruecklicher Owner-Neuabgrenzung geschlossen.

## 4. Ressourcenentscheidung (MEASURED, D1/D2)

Messverfahren und Kandidaten: [`audits/WEB_NAV_BUDGET_20261020/`](../audits/WEB_NAV_BUDGET_20261020/)
(Basisabgleich Suffix 9 797 B / Shell 11 312 B). Kandidat B1 (Navigation + Meldungsseite)
besteht die 74 bestehenden JS-Faelle (Fake-DOM, kein Browser) und einen Ad-hoc-Test
(Sichtwechsel, Zeilenknoepfe senden genau `ack-message`).

| Option | Navigation + Meldungen | Shell | Bewertung |
|---|---:|---:|---|
| Heute | – | 11 312 B, **+16 B** Spielraum | – |
| B1 im Shell-Anhang | +601 B | 11 913 B | passt nicht (−585 B) |
| B2 = B1 + `terser` | +357 B | 11 669 B | passt nicht (−341 B) |
| B3 = B2 + Quittieren/Stumm aus der Aktionsleiste verlagert | +78 B (Kopf −63 B) | 11 327 B | **+1 B** Reserve; Build-Werkzeug `terser` noetig, Funktionsverlagerung, kein Platz fuer C2/C3 |
| **A statischer Asset** | Asset 10 374 B, Shell **1 583 B** | – | traegt alle Slices; neuer Vertrag |

Erwartete Asset-Groesse fuer den Gesamt-PR (**ESTIMATED**, je Slice zu messen und
nachzufuehren): Navigation/Meldungen +0,6 KB (gemessen), Programme +1,5…2,0 KB, Einstellungen
+1,0…1,5 KB, Outcome-Anzeige +0,3 KB, ggf. Service +0,8 KB ⇒ rund 14…15,5 KB. Vorschlag
Asset-Limit **16 384 B**, Shell-Limit **2 048 B** (die Kombi-Grenze 11 456 B entfaellt).
Pro Slice gilt: ueberschreitet die Messung den Slice-Schaetzwert um mehr als 25 % oder das
Asset-Limit, folgt **HOLD**, keine automatische Erhoehung.

**Option A – Vertrag.**

- *Route:* `GET /assets/app.<crc8>.js` (acht Hex-Ziffern CRC-32 der Asset-Bytes, einmal je
  Prozess mit dem vorhandenen `crc32`); jeder andere `/assets/…`-Pfad faellt durch zur
  Adapter-404. Version im Pfad, weil die Query im Pfad verbleibt. Nur GET (sonst 405 JSON),
  gleiche Metadatenpruefung wie die Shell (`validWebMetadata`), kein Cookie-Zugriff, kein
  `Set-Cookie`. Waehrend des Netzwerk-Setups gehoert `/` der Setup-Seite, `/assets/…` faellt
  durch.
- *Auth/Session/CSRF:* der Asset ist **oeffentlich** (die Anmeldeseite braucht das Script
  vor der Anmeldung) und enthaelt **keine** Sitzungs-, CSRF-, Sequenz- oder
  Konfigurationsdaten; diese bleiben im Inline-Teil der Shell (`const S,A,U,Q`, global
  sichtbar fuer das nachfolgende klassische Script). Test: byte-identische Antwort in allen
  Authentifizierungszustaenden, ohne Cookie. Mutationen, CSRF, Sequenz und Replay laufen
  unveraendert ueber `/internal/ui/run`.
- *Cache:* neues geschlossenes Metadatum `HttpResponseMetadata::cache = ImmutableAsset` →
  `Cache-Control: private, max-age=31536000, immutable`, nur bei der exakten Versions-URL;
  Standard bleibt ohne Header (alle anderen Antworten unveraendert). Veraltete URL → 404 (kein
  Vermischen alter und neuer Bytes). Aenderungen: `device_platform` (Typ, Validierung),
  `device_platform_esp_idf` (Header), Host-Tests.
- *F1-/Fehlervertrag:* wie der Shell-Anhang: Sendefehler → keine weiteren Chunks, kein
  Terminator, Adapter gibt den Fehler zurueck; jede Ersetzung (zu grosser Asset) loescht den
  Anhang und liefert vollstaendig `503` `application/json` `{"error":"web-asset-unavailable"}`;
  faellt der Asset aus, zeigt die Shell einen sichtbaren sprachneutralen Hinweis statt einer
  leeren Seite.
- *Flash/Heap/Stack (ESTIMATED bis Messung):* `.flash.rodata` ± Zusatzstrings der Slices (der
  Asset ersetzt den heutigen Suffix 1:1), `.flash.text` rund +0,4…0,8 KB (Route, CRC,
  Metadatenfeld), `.dram0.*`/`.iram0.*` ±0; Asset-Antwort **0 B** Koerper-Heap (Flash-Chunks
  wie heute); `handleAsset` ohne Antwort-String, kein groesserer Frame als `handleShell`
  (352 B); die tiefste HTTPD-Kette (Status-Handler) bleibt unveraendert. Erstaufruf zwei
  Requests (Shell 1,6 KB + Asset), danach gecacht; die Wirkung auf Sockets/F4 ist offline
  `NOT_PROVEN` (D9).
- *Bewertete und verworfene Alternativen:* B1–B3 (siehe Tabelle); Lazy-Feature-Asset (gleicher
  Vertrag, mehr Komplexitaet); serverseitige Einzelsprachen-Suffixe (Weg E aus #208:
  Sprach-Cookie, drei Suffixe/Goldens); SPA/Framework/Dateisystem/gzip/`httpd_resp_send_file`
  (ausgeschlossen); Setup-Shell bleibt inline und unveraendert.

## 5. Navigation (gilt fuer A)

Eine `<nav>` mit **nur den umgesetzten Bereichen** (Übersicht, Meldungen (n), Programme,
Einstellungen – jeweils erst mit ihrem Slice; kein Eintrag fuer Diagnose, Service, System
oder Manueller Betrieb, bis deren Funktionen existieren). Sichtbar nur mit gueltigem `LP`;
`aria-current`; Touchflaechen ≥ 2,75 rem; Mobil einspaltig/umbrechend, ab 40 rem dieselbe
Leiste (Seitenmenue bleibt optional und einem Stand mit mehr Bereichen vorbehalten). Sicht
in einer Variablen, gespiegelt im Fragment (`#alerts`, `#programs`, `#settings`; jedes
andere Fragment wird ignoriert); Wechsel loest keinen Request aus und rendert nur um.
Stale/Offline gedaempft mit Abrufalter; alle Aktionen weiter nur aus `P`.

## 6. Fachliche Inhalte der Slices

**Meldungen (C1).** Alle gelieferten Meldungen in API-Reihenfolge (kein Umsortieren im
Browser), Code (unlokalisiert wie heute), Schweregrad, Zustand (aktiv/quittiert/stumm/
erledigt/Entscheidung erforderlich), je Zeile `Quittieren`/`Stummschalten` ueber
`send({t:'ack-message'|'mute-message',id})`; deaktiviert bei `B`, `!P`, erledigt oder
erreichtem Zustand. Die Aktionsleiste der Übersicht bleibt unveraendert. Keine Titel,
Zeitpunkte oder Historie (nicht in der API).

**Programme (C2).** Liste der installierten, aktivierten Programme (wie `/programs`), je Auswahl
eine **Detailansicht** und die **Anpassung fuer den naechsten Lauf**
(`x,d,h,s,c,k,l` des vorhandenen `start-program`-Intents; Anwendung und Pruefung bleiben allein
in der Application; keine Browser-Domainregel). Die Details brauchen additive, schreibgeschuetzte
Daten: Weil 16 Programme mit je rund 200–250 B den 3 072-B-Body der Liste sprengen wuerden
(**ESTIMATED**, in C0 zu messen), kommt ein zweiter Lesepfad `GET /api/v1/programs/<id>`
(validierter Kleinbuchstaben-Identifier ≤ 48 B, Body ≤ 3 072 B, 404/503 fail-closed wie der
Katalog, an `catalogRevision` gebunden) statt einer Listenerweiterung. Start bleibt zwei
Klicks, gebunden an Modus, Laufrevision, Katalogrevision und gewaehltes Programm; die
vorhandene Start-Steuerung der Übersicht bleibt bestehen (keine Verlagerung der Standby-Aktionen).
**Nicht** enthalten: Anlegen/Aendern/Loeschen von Benutzerprogrammen (D6).

**Einstellungen (C3).** Umfang vom Owner zu bestimmen (D5). Empfehlung: **Gerätename** und
**Zeitzone** (aus der festen, validierten Zonenliste) aendern ueber die bestehenden
Application-Kommandos (`SetDeviceName`/`ConfigurationCommit` mit `expectedUserConfigurationRevision`),
mit eigener Bestaetigung; Geraetename nicht waehrend eines aktiven Laufs (wie Touch). Vor der
Umsetzung ist in C0 zu klaeren und zu belegen: (a) Wirkung auf mDNS/Hostname und SoftAP-SSID
(SSID wird in `network_configuration_service` aus dem Namen abgeleitet), damit ein Wechsel
ueber den AP die Verbindung nicht still kappt; (b) ob die Mutationsroute eine neue, geschlossene
Intent-Familie `set-device-name`/`set-time-zone` bekommt (Codec, Outcome-Matrix, Replay-Budget
3 072/1 024 B). **Nicht** enthalten und per Web nicht angeboten: Netzwerkmodus/Wi-Fi,
Webzugang/Passwortschutz, Display-Sprache (lokale Einstellung), Werksreset – diese gehoeren
dem lokalen Geraet bzw. dem Service-Bereich (Sitzungsentzug und Aussperrrisiko).

**Service (C4, bedingt).** Die Session-Lease-Bausteine existieren, aber es gibt noch keine
Servicefunktion, die sie nutzt (#28/#19/Hardware). Eine Route `POST /api/v1/service/unlock`,
Statusfeld und PIN-Seite ohne Nutzer waeren vorgezogene Infrastruktur und eine Scheinseite.
Empfehlung: **nicht in diesem PR**, sondern mit dem ersten Verbraucher (D7). Wenn der Owner
trotzdem die Grenze wuenscht: Route `unlock`/`lock`, PIN ueber `AuthenticationDomain::verifyServicePin`
(globale Fehlversuchs-/Sperrregeln unveraendert), Lease 5/15 min je Websitzung, Widerruf bei
Logout/Sitzungsende, Statusfeld `service`, UI nur Sperrzustand/Freigabe – ohne Aktionen.

**Konfliktdarstellung (C5).** Das Ergebnis `stale`/`busy`/`rejected`/`confirmation-required`
wird heute zu „Nicht ausgefuehrt“ vereinheitlicht. C5 zeigt eine kurze, uebersetzte Ursache je
Outcome (`stale` = Zustand hat sich geaendert, neu geladen) – nur Anzeige, keine Aenderung der
Outcome-Matrix.

## 7. Verbindliche Slice-/Commit-Abfolge (ein PR)

0. **C0 – Ressourcen- und Navigationskonzept** (nur bei D1 = A):
   a. Plattform: `HttpResponseMetadata::cache` (Validierung, Adapter-Header, Host-Tests);
   b. Asset-Route mit CRC-Pfad, GET-only, Metadatenpruefung, Fehlervertrag; **reine
   Verschiebung** von `kWebShellSuffix` in den Asset, Shell referenziert ihn (inkl. Ausfall-
   hinweis); Limits Shell 2 048 B / Asset 16 384 B; Goldens, JS-Test (Asset als Quelle),
   Native-Tests angepasst; **kein Verhaltensunterschied** (alle 74 JS-Faelle + 109 Native-
   Webtests gruen);
   c. Messungen/Analysen fuer C2/C3 (Programmdetail-Body, Einstellungs-Wirkung auf Hostname/SSID,
   Replay-Budget) als dokumentierte Evidence. *Zwischenreview optional (gezielt).*
1. **C1 – Navigation und Meldungen.** Kandidat B1 als Messbasis; Fragment-Routing; Labels
   `ov`, `rs`; Zeilenknoepfe.
2. **C2 – Programme.** `GET /api/v1/programs/<id>` (Codec/Handler/Tests), Detail- und
   Anpassungsansicht, Anbindung an den vorhandenen `start-program`-Intent.
3. **C3 – Einstellungen** (Umfang nach D5): Intent-Familie, Application-Anbindung, UI mit
   Bestaetigung; **Entfaellt**, wenn D5 = nicht in diesem PR.
4. **C4 – Service-Grenze** nur bei D7 = ja; sonst entfaellt der Schnitt.
5. **C5 – Integration, Regression, Evidence.** Outcome-Anzeige, WEB_UI/ACCEPTANCE_TESTS/ROADMAP/
   Issue-Status, vollstaendige Offline-Ressourcenevidence, Builder-Self-Check; danach
   **ein** unabhaengiger Full Review ueber den gesamten PR (optionale gezielte Zwischenreviews
   nach C0 und C2, kein Full Review je Commit).

Jeder Schnitt wird auf seinen Quell-/Testcommit gebunden und bleibt einzeln revertierbar;
nicht gewaehlte Schnitte werden im Plan-Delta gestrichen, nicht als erledigt dargestellt.

## 8. Gezielte Nachweise

| Bereich | Faelle |
|---|---|
| Echter extrahierter JS-Test (`scripts/test_web_shell_polling.mjs`, Fake-DOM erweitert) | bisherige 74 Faelle erhalten; je Slice: Navigation nur mit `LP`, Fragment gueltig/ungueltig, Sichtwechsel ohne Request, DE/EN/ES vollstaendig, `sl()`-Umrendern; Meldungen: Reihenfolge, 16 Eintraege, richtige `id`/Intent, Sperren bei `B`/`!P`/Stale/Offline/erledigt; Programme: Detail, Anpassung `x,d,h,s,c,k,l` Bereichsgrenzen als Eingabe-Huelle (Application entscheidet), Start-Bestaetigung wie bisher; Einstellungen: Revisionsbindung, Bestaetigung, kein Request ohne Bestaetigung; Mutationsgegenproben (fehlende `P`-Pruefung, falsche `id`, ungueltiges Fragment) |
| Native `test_web_application_routes` | Asset-Route (GET-only, 405, 404 fuer falsche/alte Version, Header nur beim exakten Pfad, byte-identisch in allen Auth-Zustaenden, kein Cookie, Fehlerpfad 503 ohne Anhang), Shell referenziert den aktuellen Pfad, Shell-/Asset-Limits, eingefrorene Generatoren, Setup-Shell unveraendert; Programmdetail-Route (Auth, GET-only, Identifier-Validierung, 404/503, Revisionsbindung, Bodygrenze mit 96-B-Namen und Escaping); neue Intents (Replay-Budget, Outcome-Matrix, Fehlerpfade) |
| Plattform/Adapter | `test_http_server_lifecycle` (Metadatenvalidierung), `esp_idf_http_server_adapter_host` (Header nur beim Asset, Chunk-Reihenfolge, Fehler in Kopf/Anhang/Terminator, suffixlose Antworten unveraendert) |
| Regression | `test_fermentation_ui_models`, `test_renderer_boundary`, `test_ui_steady_state_allocations`, `test_run_detail_projection`, `test_run_commands`, `test_configuration_service`, `check_architecture_boundaries.py` |
| Ressourcen offline | Sektionsvergleich (`.dram0.*`, `.iram0.*`, `.flash.*`) gegen `main`, `-fstack-usage` (`handleAsset`, `handleShell`, Status-/Programm-Handler, Codec), Asset-/Shell-Bytes, Allokationsfenster neuer Handler, beide ESP-Profile; hoechstens `RAM_OFFLINE_ESTIMATED` |
| Sicherheit | Asset ohne Sitzungsdaten, kein neuer unauthentifizierter Schreibpfad, CSRF/Sequenz/Replay fuer jede neue Mutation, kein Netzwerk-/Service-/Reset-Zugriff ueber die neuen Seiten |
| Abgrenzung der Evidence | Fake-DOM-/Native-Evidence belegt Logik und Vertraege; **Browser-, Mobil-/Desktop-Layout, Barrierefreiheit, Geraetelast und HWM bleiben `NOT_PROVEN`**, bis der Owner eine Browser-/Geraetemessung anordnet (D9); Layout nur strukturell (Viewport/Media-Query-Strings, Touchflaechen-Regel) |

Geplante Befehle (nicht jetzt):

```bash
node scripts/test_web_shell_polling.mjs
pio test -e native -f test_web_application_routes -f test_http_server_lifecycle -f test_fermentation_ui_models -f test_renderer_boundary -f test_ui_steady_state_allocations -f test_run_detail_projection -f test_run_commands -f test_configuration_service
python3 scripts/build_esp_idf_profiles.py all --require-clean-source-tree
export PRE_READY_EXPECTED_HEAD="$(git rev-parse HEAD)"
bash scripts/run_pre_ready_gates.sh self-check
```

Pre-Ready nur nach Independent Review, `OPEN_BLOCKERS=0` und ausdruecklicher Owneranweisung.

## 9. Ownerentscheidungen (offen, mit Empfehlung)

| ID | Frage | Empfehlung |
|---|---|---|
| **D1** | Auslieferung: **(A)** statischer Asset gemaess §4 · **(B3)** im Shell-Anhang mit `terser`, nur Navigation/Meldungen, 1 B Reserve, uebriger Restumfang abgetrennt · **(C)** zurueckstellen | **A** |
| D2 | Limits bei A: Shell 2 048 B, Asset 16 384 B, Slice-HOLD bei > 25 % Abweichung; die Kombi-Grenze 11 456 B entfaellt | wie vorgeschlagen |
| D3 | Asset oeffentlich ohne Sitzung (noetig fuer die Anmeldeseite), garantiert frei von Sitzungs-/Konfigurationsdaten | ja |
| D4 | Cache: unveraenderlich ueber CRC-Pfad (Metadatenerweiterung) oder ohne Cache-Header | unveraenderlich |
| **D5** | Einstellungen im Web: **(a)** Gerätename + Zeitzone, **(b)** nur Anzeige (keine Scheinseite, daher nicht empfohlen), **(c)** nicht in diesem PR | **a**, nach Klaerung SSID/Hostname in C0 |
| D6 | Webeditor fuer Benutzerprogramme (anlegen/aendern/loeschen/aktivieren): **(a)** nicht in diesem PR, eigenes Issue mit Spezifikation, **(b)** hier planen (Spezifikation fehlt) | **a** |
| D7 | Web-Service-Grenze (PIN-Freigabe) ohne Verbraucher: **(a)** mit dem ersten Verbraucher (#28/#19) **(b)** jetzt als C4 ohne Aktionen | **a** |
| D8 | Widerspruch WEB_UI (Werksreset als Web-Beispiel) ↔ Code/LOCAL_UI_SETTINGS_SERVICE (nie per Web): Spezifikation angleichen (Werksreset nur lokal) | angleichen, in C5 |
| D9 | Hardware/Browser: nach der Umsetzung eine kleine Browser-/Geraetemessung (zwei Requests, Heap/HWM, Mobil/Desktop) vor Ready oder `NOT_PROVEN` mit FOLLOW-UP | Messung auf Ownerwunsch |

**Bündelung.** Ein PR ist Default und wird empfohlen. Eine Abtrennung ist nur dort
begruendet, wo ein fremdes Issue oder eine fehlende Spezifikation es erzwingt (Manueller
Betrieb/#35, Diagnose/#28, Journale und Backup/#19, Service-Funktionen, Programmeditor D6,
Favicon #204); sie sind **nicht** Teil dieses PR und werden nicht stillschweigend
uebernommen. Eine zusaetzliche Aufspaltung der enthaltenen Schnitte wird nicht
vorgeschlagen.

## 10. Risiken

- Der Asset-Weg aendert die Auslieferung (zwei Requests beim Erstaufruf); Geraetewirkung nur
  per D9 messbar.
- Globale `const`/`let` des Inline-Skripts (`S`, `A`, `U`, `Q`) muessen im nachfolgenden
  klassischen Script sichtbar sein; Test mit zwei getrennten `vm`-Skripten.
- Veralteter Tab nach Firmwareupdate: alte Asset-URL → 404; sichtbarer Hinweis, Neuladen genuegt.
- Asset-Wachstum durch spaetere Bereiche: Limit 16 384 B; danach seitenweise Assets (eigener
  Plan), keine stille Erhoehung.
- Einstellungen (D5-a) sind die erste neue Mutationsfamilie ausser dem Lauf; Hostname/SSID-
  Wirkung und Aussperrrisiko muessen in C0 belegt sein, sonst entfaellt C3 (HOLD).
- Matrixzeilen H/X bleiben offen; R1-U/D9/G2 und Aktorfreigabe unveraendert offen.
