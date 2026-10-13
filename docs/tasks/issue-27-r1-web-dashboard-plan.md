# Issue #27 – R1-Web-Startseite: lesbare, zustandsabhaengige Uebersicht (Plan)

Status: `PLAN_AWAITING_OWNER_APPROVAL`. Keine Implementierung vor Ownerfreigabe
der exakten Plan-SHA. Basis: `main` bei `b903de8` (nach Merge PR #202; PR #170
und PR #201 gemergt). Scope: genau ein kleiner R1-Slice von Issue #27; Issue
bleibt offen, weitere Web-Slices erst getrennt.

## 1. Ziel und Nicht-Ziele

Ziel: Die rohe JSON-Anzeige (`<pre id="status">` mit `JSON.stringify`) der
authentifizierten Web-Shell wird durch eine lesbare, responsive,
zustandsabhaengige Startseite ersetzt. Angezeigt wird ausschliesslich, was die
vorhandenen Read-only-APIs (`/api/v1/status`, `/temperatures`, `/alerts`)
tatsaechlich liefern. Bedienfunktionen (Quittieren, Stumm, Stop, Abschliessen,
Start), Login/Logout, Session, CSRF/Replay, 5-s-Polling, Revisionsschutz und
DE/EN/ES-Texte bleiben funktional unveraendert. Kein Framework, kein neuer
Endpunkt, keine zweite Fachlogik: die Shell uebersetzt Tokens in Text, sie
leitet keinen Zustand ab.

Nicht-Ziele: neue Backend-Mutationspfade; Fehlerreset ohne gebundenen
Watchdog-/Planner-Owner; manuelle Starts ohne Limits-Owner; Web-Service-PIN und
Serviceaktionen; Programm-Editor; Charts/Historie/Diagnose/Exporte (#28);
Backup/Import/Journale (#19); Hardware/Commissioning; neue Wireformate oder
Aenderung der JSON-Codecs; Navigation ueber mehrere Web-Seiten (siehe 2,
Luecken). `ACTUATOR_RELEASE=NO` bleibt.

## 2. Verifizierte Ausgangslage (`b903de8`)

| Befund | Beleg |
|---|---|
| `webShell(...)` rendert die Shell serverseitig pro Request (CSRF-Token, Mutationssequenz, Auth-Zustand eingebettet); Auth-Zustaende `Unprovisioned` -> `provisioningShell()` (eigene, unveraenderte Seite) | `web_application_routes.cpp:231-` |
| Die authentifizierte Shell pollt alle 5 s `/api/v1/status`, `/temperatures`, `/alerts` zusammen; jeder Fehler eines der drei Requests setzt `P=null`, deaktiviert alle Aktionen und zeigt `Stale`; Anzeige ist `JSON.stringify(p,null,2)` | `refresh()` in `webShell` |
| Bedienung (`#ops`: ack/mute/stop/done/start + Programmliste) ist erledigt (PR #201) und wird ausschliesslich aus `P[0].homeMode`, `P[0].revisions.*` und `P[2].alerts` gesteuert | `ui()` in `webShell` |
| **Groessenschranke:** `kMaximumWebShellBytes = 6656` (Ownerentscheid, `html.reserve`; `handleShell` liefert bei Ueberschreitung `503 web-shell-unavailable`). Gemessene authentifizierte Shell **6593 B**; der Test reserviert zusaetzlich 19 B fuer die 20-stellige Mutationssequenz -> **44 B Restspielraum** | Messung auf `b903de8` (temporaere Assertion, nicht committet); `test_shell_stays_within_the_limit_in_all_states` |
| API-Felder `/status`: `version`, `ready`, `homeMode` (`standby`,`active-run`,`waiting`,`completed`,`restricted`,`recovery`,`unavailable`), `processState` (Token, 15 Werte), `networkMode`, `selectionRequired`, `revisions.*`. **Kein** Geraetename, Programmname, Solltemperatur, Rest-/Laufzeit, Zeit-/Netzwerkdetail | `encodeWebApiStatus` |
| API-Felder `/temperatures`: je Rolle (`cabinet-air`, `product`, `cooling`, hoechstens 3) `quality` (`valid`/`stale`/`failed`), `valid`, `valueCelsius` (Zahl oder `null`). Eine Rolle kann ganz fehlen | `encodeWebApiTemperatures` |
| API-Felder `/alerts`: je Meldung `id`, `code` (Token), `severity` (`information`,`warning`,`recovery`,`decision-required`,`safety-fault`), `active`, `acknowledged`, `resolved`, `decisionRequired`, `muted`, `revision`; Zahl begrenzt durch `kMaximumWebApiAlertCount`, Body <= 3072 B | `encodeWebApiAlerts` |
| Der HTTP-Adapter hat genau einen Wildcard-Handler (`max_uri_handlers=1`); `HttpResponse::body` ist ein `std::string` (Heap-Kopie, keine Flash-Auslieferung). Statische Zusatzdateien (`/app.js`) sparen daher keinen Heap und brauchen eine neue Route | `esp_idf_http_server_lifecycle.cpp`, `http_server_lifecycle.hpp` |
| Hardwarelage (PR170-Gate, Home-WLAN, LVGL aktiv): `minimum_free_heap` ca. 26 KB, groesster freier 8-Bit-Block 25 600 B; Ressourcenqualifikation der **6656-B-Shell auf Hardware ist `NOT_RUN`** | `docs/audits/PR170_HW_GATE_20261005/UART_STAMPED.txt`, `docs/ROADMAP.md` |
| `docs/WEB_UI.md` verlangt Startseite je Zustand (Standby/Lauf/Abschluss), responsive Darstellung, sichtbaren Verbindungsabbruch, keine veralteten Werte als aktuell und Alter des letzten gueltigen Status; DE/EN/ES je Browsersprache | `docs/WEB_UI.md` (Startseite, Responsive, Live-Aktualisierung) |

**Luecken, die dieser Slice bewusst nicht schliesst** (jede brauchte neue
API-Felder oder Backend-Arbeit und damit einen eigenen Ownerentscheid): Geraete-
und Programmname, Solltemperatur, verstrichene/verbleibende Zeit,
Abschlussverhalten, Netzwerk-/Zeitstatus, Aktorstatus, Mehrseiten-Navigation
(Programme/Manuell/Meldungsprotokoll/Einstellungen/Diagnose/Service/System),
Moeglichkeit `Manueller Betrieb` (ohnehin Limits-Owner-gesperrt, 503).
Die Startseite zeigt daher **nur**: Verbindungs-/Altersstatus,
zustandsabhaengige Ueberschrift, Temperaturen, aktive Meldungen und die
bestehenden Aktionen.

## 3. Betroffene Module, Vertraege, Gates

- `fermentation_app`: ausschliesslich `webShell` und `kMaximumWebShellBytes` in
  `web_application_routes.cpp`. Keine Aenderung an Codecs, Routen,
  `web_session`, Application oder `device_platform*`; kein neuer Endpunkt.
- Unveraendert und per Regression gehalten: Session/CSRF/Replay, Same-Origin-
  Pruefung, Mutationssequenz (`let Q=`), Provisioning-Shell (eigene Funktion),
  Response-Budgets der JSON-APIs (3072 B), Polling (5 s, drei Requests).
- Gates: ADR-013-Architekturcheck, clang-tidy, beide ESP-IDF-Profile,
  Pre-Ready erst nach Independent Review und Ownerfreigabe.

## 4. Design des Slices

**Rendering.** Die statische Shell behaelt `#auth-state`, `#warning`, Login-
Formular, Logout, `#ops`. Der `<pre id="status">` entfaellt; an seine Stelle
treten `#connection` (Verbindungs-/Altersstatus), eine Ueberschrift `#mode`,
eine Temperaturliste `#temps` und eine Meldungsliste `#alerts`. Aller
dynamische Text wird ausschliesslich ueber `textContent`/`createElement`
gesetzt, **nie** ueber `innerHTML` (API-Strings wie `code` werden nicht als
Markup interpretiert).

**Zustandsabhaengigkeit** (nur Anzeige, keine neue Logik): `homeMode` waehlt
Ueberschrift und CSS-Zustandsklasse; `ui()` fuer die Aktionen bleibt
unveraendert.

| `homeMode` | Ueberschrift (de / en / es) |
|---|---|
| `standby` | Bereit / Ready / Listo |
| `active-run` | Lauf aktiv / Run active / Proceso activo |
| `waiting` | Wartet auf Eingabe / Waiting for input / Esperando accion |
| `completed` | Lauf abgeschlossen / Run completed / Proceso finalizado |
| `restricted` | Eingeschraenkt / Restricted / Restringido |
| `recovery` | Wiederherstellung / Recovery / Recuperacion |
| `unavailable` | Nicht verfuegbar / Unavailable / No disponible |

Unbekannte Tokens (spaetere Codec-Erweiterung) werden unveraendert als Token
angezeigt, nie verschwiegen; der Drift-Test (C2) faengt sie vorher ab.
`processState` wird in diesem Slice **nicht** lokalisiert angezeigt (siehe D2).

**Temperaturen.** Feste Reihenfolge Schrank (`cabinet-air`), Produkt
(`product`), Kuehlung (`cooling`); eine nicht gelieferte Rolle erscheint als
`nicht verfuegbar`, nie als `0` oder leer. Gueltiger Wert (`valid===true` und
Zahl): eine Nachkommastelle plus `°C`. Sonst `--` und lokalisierter Grund
(`stale` -> veraltet, `failed` -> Fehler); der Wert wird nie gezeigt, auch wenn
ein `valueCelsius` ankaeme.

**Meldungen.** Nur `active===true`. Zaehler (`n aktive Meldungen` /
`keine aktiven Meldungen`) plus je Meldung Schweregrad als CSS-Klasse und Text
`code` (Bindestriche zu Leerzeichen), plus Marker `quittiert`/`stumm`. Die
Lokalisierung der 7 Meldungscodes ist **nicht** Teil dieses Slices (D3).

**Verbindung, Alter und Aktionssperre (eine Regel).** Zustaende: `offline`
(noch nie ein gueltiger Snapshot), `online`, `stale`. Variablen:

- `P` (bestehend) ist der **Gating-Zustand**: `null` sperrt ueber `ui()` alle
  Aktionen und verwirft eine offene Zwei-Klick-Bestaetigung
  (`if(!P||KC&&c!==KC)K=''`, unveraendert). `P=null` entsteht nur bei einem
  **angewendeten** Fehler oder bei der Altersschwelle, nicht bei jedem
  eintreffenden Fehler.
- `LP`/`LT` (neu) sind nur die **Anzeigekopie** des letzten gueltigen Snapshots
  samt Zeitstempel. `ui()` liest sie nie; sie geben nie Aktionen frei.
- `G`/`A` (neu) sind Abrufgeneration und hoechste angewendete Generation.

**Reihenfolgeregel.** `setInterval(refresh,5000)` und `send()` (`await
refresh()` nach einer Mutation) koennen Abrufe ueberlappen; die Latenz liegt
gemessen ueber dem Pollintervall (bis 10,6 s). Ohne neuen Timer, Endpunkt oder
Polling-Rahmen gilt clientseitig:

- Jeder `refresh()` nimmt **vor** dem ersten `fetch` `my=++G` und `t0=Date.now()`
  (`t0` gilt fuer alle drei parallelen Abrufe gemeinsam).
- Nach `await`, im Erfolgs- **und** im Fehlerpfad (`catch`), und **vor jeder
  Wirkung** (Zuweisung an `P`, `LP`, `LT`, Verbindungsanzeige, `ui()`) gilt
  `if(my<A)return;A=my;`. Ein aelteres Ergebnis kann damit weder einen
  spaeter bereits angewendeten gueltigen Stand noch dessen Status
  ueberschreiben.
- Ein **angewendeter Fehler** setzt `P=null`, ruft `ui()` (Aktionssperre,
  Bestaetigung verworfen) und zeigt `stale`; die Anzeigewerte bleiben gedimmt.
- Ein **angewendeter Erfolg** setzt `P`, `LP`, `LT=t0` und `online`, wenn
  `Date.now()-t0<=15000`; ein zu alter Erfolg aktualisiert hoechstens die
  gedimmte Anzeige (`LP`/`LT`, nur wenn `t0>LT`), gibt aber keine Aktion frei.
  Wiederverbindung = voller Snapshot (Ist-Verhalten).

**Altersschwelle.** Zu Beginn jedes 5-s-Ticks wird `Date.now()-LT` geprueft;
ueber **15 s** wird `P=null` gesetzt und `stale` gezeigt, auch wenn kein `fetch`
je abgeschlossen oder fehlgeschlagen ist (ein haengender `fetch` hat im
Browser kein Zeitlimit). Die Schwelle haengt nur an `LT`, nie am Abschluss
eines Abrufs; ein haengender Abruf kann sie nicht unterdruecken. Begruendung:
drei Poll-Perioden und ueber der im PR170-Gate gemessenen Spitzenlatenz von
10,6 s (`POLL_600S.txt`, `lat_ms max=10648`, 3 Sessions); eine kuerzere
Schwelle wuerde im Normalbetrieb flackern.

**Bedeutung der Altersanzeige.** `Stand vor N s` (aus `LT=t0`, bei jedem Tick
aktualisiert) ist das Alter des **Statusabrufs** seit dessen Beginn; eine
verspaetet eintreffende Antwort wird nie auf `jetzt` datiert. Es ist **keine**
Aussage ueber das Alter des Sensor-Messwerts: die API liefert keinen
serverseitigen Messzeitstempel, und fuer die Messwertqualitaet bleibt
ausschliesslich die Sensor-`quality` bzw. `valid` der API massgebend.

Session-, Revisions-, Mutations- und Sicherheitslogik sowie der bestehende
`location.reload()`-Pfad in `send()` bleiben unveraendert. Nicht beendete
aeltere Abrufe werden nicht abgebrochen (kein `AbortController`: braucht
Merkmalspruefung fuer aeltere Browser und Zusatzbytes; die Anzahl haengender
Abrufe bleibt wie heute durch die 5-s-Taktung und das Browser-Verbindungslimit
begrenzt).

**Testvertrag der Fehlerpfad-Assertion.** Der bisherige exakte Text-Pin
`catch(e){P=null;ui();` in `test_shell_run_controls_text_contract` ist mit der
Reihenfolgeregel unvereinbar (die Generationspruefung steht im `catch` vor
`P=null`) und wird in C1 **gezielt angepasst**, nicht entfernt oder
uebersprungen: Die Assertion prueft stattdessen semantisch per
Positionsvergleich im Shell-Text, dass im `catch` zuerst `if(my<A)return;`,
danach `P=null`, danach `ui()` steht. Der Pin `b.disabled=B||!P||d[k]` bleibt
unveraendert erhalten.

**Responsiv.** Einspaltig mit grossen Bedienflaechen (`min-height:2.75rem`) auf
schmalen Anzeigen; ab `min-width:40rem` zwei Spalten (Temperaturen | Meldungen);
`prefers-color-scheme` wird nicht ausgeliefert (KISS, kein Platz).

**Groessenbudget (D1) – Beleg.** Alle Zahlen stammen aus Scratch-Messungen
auf `b903de8` (nichts davon committet) und sind **Schaetzungen mit benannter
Basis**, nicht Messwerte der fertigen Shell; der endgueltige Wert wird in C1
gemessen.

- Ist: authentifizierte Shell **6593 B** (Dump der echten Antwort), Schranke
  6656 B, 44 B frei (inklusive der 19 B Sequenzreserve des Tests).
- Freimachbar ohne Funktionsverlust: Die authentifizierte Shell ist weniger
  stark minifiziert als `provisioningShell()`. Kuerzen der Textschluessel
  (`passwordLabel`, `noSnapshot`, ...) ca. 290 B, lokale Namen (`connection`,
  `warning`, `refresh`, `locale`, ...) ca. 190 B, `addEventListener` -> `on...`
  ca. 27 B, `<pre>`/`JSON.stringify`/`noSnapshot` ca. 80 B: zusammen ca.
  **0,6 KB** (inklusive der 44 B). Davon ausgenommen sind Namen, die bestehende
  Textvertraege festschreiben (`login.addEventListener`, `T.sure`,
  `pg.onchange=()=>ui();`, `b.disabled=B||!P||d[k]`
  u. a. in `test_shell_run_controls_text_contract` und Nachbarn); C1 benennt
  nur Namen um, die kein Test festschreibt, und passt die uebrigen Pins nur
  an, wenn die Logik unveraendert bleibt (Anpassung, kein Aufweichen).
- Bedarf (Prototyp-Zaehlung, DE/EN/ES): neue Texte (7 `homeMode`, 3 Rollen,
  Qualitaets-/Alterstexte) ca. 970 B, Renderfunktion ca. 630 B,
  Stale-/Altersanzeige ca. 90 B, Reihenfolgeregel (`G`, `A`, `t0`, Pruefung)
  ca. 100 B, CSS ca. 110 B, Markup ca. 120 B: zusammen ca. **2,0 KB**. Netto
  also ca. **+1,4 KB**, erwartete Shell ca. 8,0 KB. Passt die fertige Shell
  nicht mit >= 128 B Spielraum (einschliesslich 19 B Sequenzreserve) unter
  die in D1 freigegebene Obergrenze, stoppt C1 und legt den Befund vor; die
  Schranke wird nie still ueber D1 hinaus angehoben.
- Variante ohne Schrankenaenderung (D1 = B): Selbst ohne neue Texte (rohe
  Tokens), ohne Spaltenlayout und ohne Altersschwelle liegt der Prototyp bei
  ca. 0,85-1,05 KB gegenueber ca. 0,6 KB Freimachbarem; sie verfehlt die
  Schranke um ca. 0,25-0,45 KB und verletzt zudem die Vorgabe DE/EN/ES fuer
  neue Inhalte. Sie ist damit nicht als lieferbar belegt.

## 5. Schnitte (eine Planfreigabe vor C1-C3; kein Owner-Gate je Commit)

Die Ownerfreigabe der exakten Plan-SHA gilt einmal fuer C1-C3; die kanonischen
Stopp-/Abweichungsregeln (`docs/AGENT_WORKFLOW.md`) bleiben massgeblich. Wird
die in D1 freigegebene Obergrenze ueberschritten oder zeigt sich, dass ein
Slice-Teil neue API-Felder braucht: anhalten und Befund vorlegen.

**C1 – Shell-Umbau und Budget.** `webShell` wie in Abschnitt 4; `<pre>` und
`JSON.stringify` entfallen; `kMaximumWebShellBytes` auf den in D1 freigegebenen
Wert (kleinster Wert aus {7168, 7680, 8192}, der mit >= 128 B Spielraum
einschliesslich 19 B Sequenzreserve passt, Obergrenze D1). Zuerst werden die
nicht festgeschriebenen Namen gekuerzt und die tatsaechliche Ersparnis
gemessen, danach wird die Anzeige ergaenzt. Test-Literale `6656`
in `test_web_application_routes` werden durch eine gemeinsame Konstante ersetzt
(Anpassung, kein Aufweichen: Obergrenzenpruefung in allen Zustaenden bleibt).
Die Assertion `catch(e){P=null;ui();` wird wie in Abschnitt 4
("Testvertrag der Fehlerpfad-Assertion") semantisch angepasst; alle anderen
bestehenden Shell-Vertragstests bleiben unveraendert.
Die Provisioning-Shell bleibt byteweise unveraendert und unter derselben
Schranke.

**C2 – Nachweis (nativ).** Siehe Abschnitt 6.

**C3 – Dokumentation.** `docs/WEB_UI.md` (Abschnitt "Startseite R1-Slice":
angezeigte Felder, Luecken, Schranke), `docs/ACCEPTANCE_TESTS.md`
(`SIM-27-W-01..06`), `docs/ROADMAP.md`. Browser-/Hardwarenachweis `NOT_RUN`.

## 6. Nachweis

Alle Tests nativ in `test_web_application_routes` (vorhandene Fixtures und
Request-Helfer, kein neues Framework). Die Shell-JavaScript-Logik wird wie bei
PR #201 nur als **Text-/Vertragstest** nachgewiesen; ein ausfuehrender
Browsertest bleibt `NOT_RUN`.

| ID | Pruefung |
|---|---|
| SIM-27-W-01 | Shell-Struktur: kein `JSON.stringify(p`/`<pre`/`innerHTML` im Anzeigepfad (API-Strings nur ueber `textContent`); `#mode`, `#temps`, `#alerts`, `#connection` je genau einmal; `viewport`-Meta und genau ein `@media(min-width:40rem)`; bestehende Kontrollen (`data-a` je Aktion genau einmal, Login/Logout, `let Q=`) unveraendert; alle bisherigen Shell-Vertragstests bleiben gruen |
| SIM-27-W-02 | Textvollstaendigkeit: jeder neue Textschluessel kommt genau je Sprache (de/en/es) einmal vor (Muster `test_unprovisioned_shell_texts_are_complete_in_all_locales`); Rueckfall `T=L[lang]||L.en` unveraendert |
| SIM-27-W-03 | Drift-Schutz ueber den echten Codec: `homeModeName`/`temperatureRoleName` liegen im anonymen Namespace und werden nur ueber `encodeWebApiStatus`/`encodeWebApiTemperatures` erreicht. Der Test iteriert den Wertebereich 0..15 der jeweiligen Enum (`static_cast`), sammelt alle Tokens, die erfolgreich kodiert werden, und prueft (1) die **Anzahl** (7 `homeMode`, 3 Rollen; ein neuer Enum-Wert aendert die Anzahl und faellt auf) und (2) dass jedes Token Schluessel der Shell-Labeltabelle ist |
| SIM-27-W-04 | Feldvertrag je Zustand. **Standby** ueber den echten Application-Pfad des Fixtures (`/status`, `/temperatures`, `/alerts` liefern die gelesenen Felder). **active-run, waiting, completed, restricted, recovery, unavailable** sind im Web-Fixture nicht ueber den echten Pfad erreichbar (kein Programm ohne Commissioning-Werte startbar, siehe Commit `0b7cea0`); sie werden ueber `encodeWebApi*` mit konstruierten `FermentationUiSnapshot`s nachgewiesen (Feldpraesenz-/Typvertrag, keine Annahme ueber nicht vorhandene Felder wie Programmname/Restzeit). Der Plan behauptet fuer diese Zustaende keinen Produktpfad-Nachweis |
| SIM-27-W-05 | Fehlende/stale Daten und Reihenfolge. **Codec (nativ, konstruierte Snapshots):** `quality` `stale`/`failed` -> `valid:false`, `valueCelsius:null`; fehlende Rolle; leere Temperaturliste. **Struktur-/Textvertrag der Shell (nativ):** Wert nie fuer `valid:false`; Offline-/Stale-/`Stand vor`-Pfad vorhanden; `P` und `LP` getrennte Variablen, `ui()` liest `LP` nie; der Pin `b.disabled=B||!P||d[k]` bleibt unveraendert, der Pin `catch(e){P=null;ui();` wird in C1 **gezielt angepasst** (nicht entfernt): im `catch` steht per Positionsvergleich zuerst `if(my<A)return;`, dann `P=null`, dann `ui()`; `my=++G` und `t0=Date.now()` stehen **vor** dem ersten `fetch`; `if(my<A)return;A=my;` steht in Erfolgs- **und** Fehlerpfad nach dem `await` und **vor** der ersten Zuweisung an `P`/`LP`/`LT`/Verbindungsanzeige (Positionsvergleich im Shell-Text); `LT=t0` (nie `Date.now()` bei Ankunft); Altersschwelle 15 s zu Beginn jedes Ticks setzt `P=null` ohne Abhaengigkeit vom Abschluss eines `fetch`. **Dynamische Szenarien (Sollverhalten, nativ nicht ausfuehrbar):** (1) *langsamer alter Erfolg nach schnellerem neuem Erfolg:* Generation 1 startet bei t=0 und antwortet bei t=12 s, Generation 2 startet bei t=5 s und antwortet bei t=6 s; nach t=12 s bleiben `LP`/`LT`/Anzeige bei Generation 2 (`LT=5 s`), Alter wird ab t=5 s gezaehlt, nicht ab 12 s; (2) *verspaeteter Fehler nach neuerem Erfolg:* Generation 1 schlaegt bei t=12 s fehl, nachdem Generation 2 bei t=6 s erfolgreich war: Status bleibt `online`, `P` unveraendert; (3) *haengende Anfrage:* kein `fetch` schliesst nach dem letzten Erfolg bei `LT` ab: bei `Date.now()-LT>15000` setzt der naechste Tick `P=null`, `stale` und `Stand vor N s` mit gedimmten Werten und gesperrten Aktionen; kommt die haengende Antwort danach doch an, aktualisiert sie `P` nicht (zu alt, `Date.now()-t0>15000`). Ein ausfuehrender JS-Test mit vorhandenen Projektmitteln ist nicht moeglich (PlatformIO/Unity, kein JS-Runner und kein Node in Projekt-Toolchain oder CI; ein JS-Runner waere eine neue Abhaengigkeit); der dynamische Nachweis der Szenarien (1)-(3) bleibt `NOT_RUN`. Optional darf der Builder sie einmalig mit lokalem `node` gegen die aus der Shell extrahierte Funktion mit gefaelschtem `fetch`/DOM durchspielen (Scratch, nicht committet, kein Gate); das Ergebnis wird im Commit-Text als `MANUAL_SCRATCH` benannt und ersetzt den Browsernachweis nicht |
| SIM-27-W-06 | Grenzen unveraendert: Shell <= neue Schranke in allen Zustaenden (unprovisioniert, geschuetzt anonym/angemeldet inkl. 19 B Sequenzreserve, Passwortschutz aus, Recovery); JSON-Response-Budgets 3072 B, 5-s-Polling mit genau drei Requests, kein neuer Endpunkt, Session/CSRF/Replay-Tests unveraendert gruen |
| Build/Statik | beide ESP-IDF-Profile (`build_esp_idf_profiles.py all`), `self-check`, Architekturcheck |
| `NOT_RUN` | ausfuehrende Browsertests (Layout, JS-Logik **einschliesslich der dynamischen Reihenfolge-/Stale-Szenarien (1)-(3) aus SIM-27-W-05**), Hardware-Ressourcen-/Heap-Messung der groesseren Shell, Flash; `ACTUATOR_RELEASE=NO` |

Gezielte lokale Tests im Draft: `test_web_application_routes`, `test_web_session`,
`test_web_browser_policy`, Architekturcheck, `self-check`, ESP-IDF-Build.
Voller Pre-Ready erst nach Independent Review, `OPEN_BLOCKERS=0` und
Ownerfreigabe.

## 7. Offene Ownerentscheidungen

| ID | Frage | Empfehlung |
|---|---|---|
| D1 | Die Shell-Schranke (6656 B, Ownerwert) laesst 44 B Raum; der Slice braucht netto ca. +1,4 KB (Schaetzung, Beleg in Abschnitt 4). **A:** Schranke auf bis zu **8192 B** anheben (Obergrenze; umgesetzt wird der kleinste passende Wert, mindestens 128 B Spielraum, erwartet ca. 8,0 KB). **B:** Schranke bleibt: als lieferbar nicht belegt (Abschnitt 4: Fehlbetrag ca. 0,25-0,45 KB selbst ohne neue Texte). **C:** Auslieferung ueber eigene Route(n): neuer Endpunkt, mehr Umfang, kein Heap-Gewinn, da `HttpResponse::body` ein `std::string` ist | **A.** Heap: ein Request haelt hoechstens eine Shell (der einzelne httpd-Task bedient Requests nacheinander); +1,4 KB gegenueber 6656 B bei groesstem freien Block 25 600 B ist rechnerisch klein, aber **nicht** auf Hardware gemessen (`NOT_RUN`); der Hardwarenachweis bleibt Owner-/Folgegate |
| D2 | `processState` (15 Tokens) lokalisiert anzeigen? **A:** nein, nur `homeMode` (7 Texte) als Ueberschrift. **B:** zusaetzlich `processState` (ca. +0,7 KB; ueberschreitet die D1-Obergrenze 8192 B und braucht einen eigenen Schrankenentscheid) | **A** (KISS; Standby/Lauf/Abschluss sind ueber `homeMode` unterscheidbar) |
| D3 | Meldungscodes (7) lokalisieren? **A:** nein, Code-Token lesbar gemacht (Bindestriche -> Leerzeichen), Schweregrad als Farbe/Klasse. **B:** vollstaendig DE/EN/ES (ca. +0,6 KB) | **A** |
| D4 | Fehlende Fachfelder (Programmname, Soll, Restzeit, Netzwerk/Zeit, Geraetename) als **eigener Folge-Slice** mit Codec-Erweiterung planen? | Ja, getrennt nach diesem Slice; hier nicht vorgebaut |

## 8. Risiken

- Ohne D1 = A ist der Slice nicht lieferbar (kein Rest-Spielraum); die
  Entscheidung ist ressourcenwirksam und die Hardware-Heapmessung fehlt
  weiterhin (`NOT_RUN`, bereits fuer die 6656-B-Shell offen).
- JavaScript-Logik ist nativ nur textuell/strukturell geprueft; Rendering-,
  Layout- und Reihenfolgefehler (Ueberlappung von Abrufen) zeigt erst ein
  ausfuehrender Browsertest (`NOT_RUN`). Der Struktur-Positionstest sichert nur,
  dass die Reihenfolgepruefung vorhanden und richtig platziert ist, nicht ihr
  dynamisches Verhalten.
- Ein dauerhaft abgelaufenes Session-Cookie (`401` bei den Read-APIs) wird wie
  heute als `Stale` dargestellt; eine eigene Session-abgelaufen-Anzeige ist
  nicht Teil dieses Slices (Befund, keine Aenderung).
- Spaetere Codec-Tokens ohne Shell-Label fallen im Betrieb auf die Token-Anzeige
  zurueck; der Drift-Test haelt das beim Bau auf.
