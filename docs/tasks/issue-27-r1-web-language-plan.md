# Issue #27 – R1-Web: browserindividuelle Sprachwahl (DE/EN/ES)

**2026-10-20 · PLAN-REVISION 1 · NICHT FREIGEGEBEN · UMSETZUNG GESPERRT (Ownerentscheid D1 zum Shell-Limit offen).**
[Issue #27](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/issues/27)
bleibt OPEN. Branch `agent/issue-27-r1-web-language-plan`, Draft-PR zu Issue #27.
Dieser Plan ist die einzige kanonische Fassung. Eine Umsetzung beginnt erst nach
unabhaengigem Planreview und ausdruecklicher Freigabe der exakten Plan-SHA.

**Kernbefund.** Die Sprachwahl ist rein clientseitig machbar und braucht keine neue
Uebersetzungsarchitektur. Sie passt aber **nicht** in die 120 B Spielraum oberhalb der
128-B-Normalreserve des Limits 11 264 B: die kleinste gemessene bedienbare Variante
kostet **+215 B**, die empfohlene (Endonyme, beschriftetes Auswahlfeld) **+278 B**. Es
gibt keine Variante, die ohne Streichen bestehender Inhalte oder ohne Architekturumbau
hineinpasst. Die Entscheidung ueber ein weiteres, einmaliges Limit (oder den
Architekturweg E, §6) ist eine Ownerentscheidung (§8); der Planer erhoeht nichts und
reduziert nichts eigenmaechtig.

## 1. Live-Basis

- `origin/main` = `43f0338aabb88b86e23d8af96d1cde8eeb1dec73` (Merge PR #207,
  2026-10-20T12:58:54Z); Branch von dort abgezweigt. Erste Branch-Aenderung `031215d`:
  Post-Merge-Status von #207 in der [Roadmap](../ROADMAP.md).
- Stand der Shell aus #207: authentifiziert **11 016 B** (Kopf 1 397 B, Suffix 9 619 B),
  Limit **11 264 B**, davon 128 B Normalreserve (enthaelt 19 B Sequenzaufschlag) und
  **120 B** Spielraum. Kopf-Reserve `html.reserve(1536)`; der Test begrenzt den Kopf auf
  <= 1 536 B auch mit maximaler Sequenz (20 Stellen).
- `ACTUATOR_RELEASE=NO`, `RAM_PASS=NOT_PROVEN`, F4 PROVISIONAL, G1′ storniert; keine
  Hardware-, Flash-, UART-, Reset- oder Aktorfreigabe durch diese Planung. Der Slice
  beruehrt keine Safety-/Sensor-/Aktor-/Recovery-/Persistenzlogik.

Fachquellen: [WEB_UI](../WEB_UI.md) (Abschnitt „Sprache je Browser“),
[Vorplan #207](issue-27-r1-web-standby-info-plan.md) und seine
[Ressourcenevidence](../audits/PR207_STANDBY_INFO_RESOURCE_EVIDENCE_20261020.md),
[ADR-013](../ADR-013_REUSABLE_DEVICE_PLATFORM.md), [CI/Gates](../CI_AND_QUALITY_GATES.md),
[Workflow](../AGENT_WORKFLOW.md), [Engineering](../ENGINEERING_PRINCIPLES.md).

## 2. Ziel und Nicht-Ziele

**Ziel (WEB_UI, „Sprache je Browser“):** Deutsch, Englisch und Spanisch sind im Web
**jederzeit** manuell waehlbar, unabhaengig von der Displaysprache. Beim ersten Aufruf
schlaegt die Browsersprache die Sprache vor. Die Auswahl wird im Browser gespeichert und
beim Reload wiederhergestellt. Es werden nur die drei gueltigen Codes akzeptiert; eine
Sprachwahl aendert weder Prozessdaten noch Geraetekonfiguration. Der Umfang umfasst die
**Provisionierungs-Shell (Setup)** und die **Anmelde-/Dashboard-Shell** (alle
Authentifizierungszustaende).

**Nicht-Ziele:** keine Aenderung der Geraete-/Displaysprache oder von
`UserConfiguration`, keine serverseitige Sprachspeicherung, kein neuer Endpunkt, keine
Aenderung an Auth/Session/CSRF/Replay/Polling/Mutation/Stale-/Fail-closed-Vertraegen,
kein neues Statusfeld, keine Uebersetzung von Programmnamen, Notizen oder Meldungscodes,
keine neue Sprache, keine Filesystem-/Asset-/Cache-/Architekturmigration, keine
Kompaktierungs- oder Frameworkrunde, keine Limiterhoehung ohne Ownerentscheid.

## 3. Ist-Analyse der vorhandenen Mechanik (Wiederverwendung)

Pfade in `lib/fermentation_app/src/web_application_routes.cpp`.

| Baustein | Ist-Zustand | Folge fuer den Plan |
|---|---|---|
| Uebersetzungstabelle Dashboard-Shell | `const L={key:["de","en","es"],…}` (67 Schluessel, 3 712 B im Suffix); `T[k]=L[k][{de:0,en:1,es:2}[lc]]` | **Wiederverwenden** – kein zweiter Katalog; nur die Wahl von `lc` und das Neu-Befuellen von `T` werden steuerbar |
| Spracherkennung | `g=(navigator.language||'en').slice(0,2)`, `lc=/^(de|en|es)$/.test(g)?g:'en'` – bereits gueltigkeitsgeprueft, Englisch als Rueckfall | wird zum **Vorschlag** (WEB_UI), nicht mehr zur einzigen Quelle |
| Einmalige statische Texte | beim Laden: Login-Knopf, Passwort-Label, Abmelden, Auth-Status, Warntext, `<html lang>` | muessen bei Sprachwechsel neu gesetzt werden → in eine Funktion `sl(code)` ziehen |
| Dynamische Texte | `dr()` (Verbindung, Ueberschrift, Zeilen, Meldungen) und `ui()` (Aktionsknoepfe, „Sicher?“) lesen bei jedem Aufruf `T[...]` | Sprachwechsel genuegt `sl(c); dr(); ui();`, **kein** Reload, keine neue Abfrage, Polling unberuehrt |
| Provisionierungs-Shell | eigene, kleine Tabelle `L={de:{…},en:{…},es:{…}}`, `T=L[g]||L.en` (letzteres akzeptiert beliebige zweistellige Praefixe als Tabellenschluessel) | gleiche Wahlmechanik, Gueltigkeit per Regex statt `L[g]`; `T` wird neu zuweisbar |
| Browser-Persistenz | keine; die Sitzung liegt als HttpOnly-Cookie `FSSESSION` serverseitig gebunden | Sprache gehoert **nicht** in Cookie/Server; `localStorage` (siehe §4) |
| Serverseitige Sprache | `Accept-Language` wird nicht erfasst; `HttpRequestMetadata` kennt Host, Content-Type, Cookie, CSRF, Mutationssequenz, Origin, Referer, Fetch-Site | clientseitige Loesung braucht **keine** Adapter-/Metadatenaenderung |
| Auslieferung | Kopf (Heap-String, `reserve(1536)`) + statischer Flash-Suffix (`staticBodySuffix`), gepruefte Gesamtgroesse `kMaximumWebShellBytes` | Zusatzbytes laufen ueber Kopf und Suffix; Flash waechst nur um die Zusatzbytes |

## 4. Vorgeschlagener Vertrag (nur Shell, kein API-Aenderung)

**Bedienelement.** Ein `<select id=lang aria-label=Language>` mit drei Optionen
(`de`/`en`/`es`, Anzeige als Endonyme **Deutsch / English / Español**) im Kopf, vor der
ersten Funktion der Seite und somit in jedem Authentifizierungszustand der Dashboard-Shell
sichtbar (nicht von Login, Session oder Daten abhaengig); in der Provisionierungs-Shell
gleich. Das Feld ist kein Formular-Submit und loest keinen Request aus.

**Wahl, Gueltigkeit und Persistenz.**

1. Start: gespeicherter Code (`localStorage.lang`, Lesen in `try/catch`) wenn gueltig
   (`/^(de|en|es)$/`), sonst Browsersprache (`navigator.language`, erste zwei Zeichen),
   sonst `en`. Ein ungueltiger oder manipulierter gespeicherter Wert wird ignoriert und nie
   weiterverwendet.
2. `sl(c)` validiert `c` erneut (ungueltig → `en`), setzt `lc`, fuellt `T`, `<html lang>`,
   das Auswahlfeld und alle einmaligen statischen Texte.
3. Wechsel durch den Benutzer: `sl(select.value); dr(); ui();`, danach
   `localStorage.lang = lc`. Es wird ausschliesslich der **bereits validierte** Code
   geschrieben; ein Schreibfehler (blockierter Speicher) ist folgenlos, weil die Sprache
   zuvor schon umgeschaltet wurde und der Rest der Sitzung korrekt bleibt.
4. Ohne verfuegbaren Speicher (Privatmodus, blockiert) bleibt die Wahl fuer die Lebensdauer
   der Seite wirksam und faellt beim naechsten Laden auf den Vorschlag zurueck – kein
   Fehler, kein Funktionsverlust.
5. Der Speicher ist **pro Browser und Ursprung** (Host des Geraets): zwei Browser/
   Endgeraete sind unabhaengig; Anzeige- und Browsersprache des Touchdisplays bleiben
   getrennt. Der Schluessel `lang` wird von Setup- und Dashboard-Shell gemeinsam genutzt, so
   dass die Wahl vom Setup in die Anmeldung mitgeht.

**Unveraenderte Vertraege (Beleg im Test).** `P` (Aktionsgrundlage), Generation/Reihenfolge,
15-s-Frische, 5-s-Takt, Single-Flight, Login/Logout, CSRF- und Mutationssequenz-Header,
Programmkatalogbindung und alle Aktionssperren; `sl` liest/aendert keinen dieser Zustaende.
Es gibt keinen Request durch den Sprachwechsel und keine Aenderung an Codec, Handler,
Application oder Display.

**Bekannte, bewusst akzeptierte Grenzen.** Der Ergebnistext der zuletzt ausgefuehrten
Aktion (`#note`) und ein bereits angezeigtes „Sicher?“ behalten bis zur naechsten
Aktualisierung die alte Sprache bzw. werden durch `ui()` neu gesetzt (Bestaetigung bleibt
erhalten, Text wird neu gerendert). Benutzerprogramm- und Geraetename, Meldungscodes und
Zeitzonen-Id bleiben unuebersetzt (WEB_UI).

## 5. Shell-Budget (MEASURED) und HOLD

Messbasis: 11 016 B (Basisabgleich `kWebShellSuffix` = 9 619 B reproduziert), Limit
11 264 B, Normalreserve 128 B ⇒ **120 B Spielraum**. Verfahren und Kandidaten dauerhaft in
[`audits/WEB_LANGUAGE_BUDGET_20261020/`](../audits/WEB_LANGUAGE_BUDGET_20261020/)
(`measure.py`, `candidates/`). Alle Kandidaten sind nach Einbau in den extrahierten
Produktsuffix mit dem ausfuehrenden JS-Test geprueft: der empfohlene Kandidat laeuft mit den
bestehenden **56/56** Faellen PASS (inklusive des Pfads ohne `localStorage`) und in einem
Ad-hoc-Wechseltest (de/en/es, ungueltiger Code → `en`, Schreiben des validierten Codes);
das ist Fake-DOM, kein Browser.

| Kandidat | Kopf+ | Suffix+ | Gesamt | Kopf mit max. Sequenz | Spielraum bei 11 264 (negativ = ueber) | Limit mind. (mit 128 B) |
|---|---:|---:|---:|---:|---:|---:|
| **A2 / Endonyme + `aria-label`** (empfohlen) | 118 | 160 | **11 294** | 1 534 (<= 1 536) | **−158** | **11 422** |
| A2 / Codes `de en es` + `aria-label` | 75 | 160 | 11 251 | 1 491 | −115 | 11 379 |
| A2 / Codes ohne `aria-label` | 55 | 160 | 11 231 | 1 471 | −95 | 11 359 |
| R (Reload statt In-Place) / Endonyme | 118 | 155 | 11 289 | 1 534 | −153 | 11 417 |
| R / Codes + `aria-label` | 75 | 155 | 11 246 | 1 491 | −110 | 11 374 |
| R / Codes ohne `aria-label` | 55 | 155 | 11 226 | 1 471 | −90 | 11 354 |

Ergebnis: **keine** Variante passt in die 120 B (kleinste: +215 B, Ueberschreitung 95 B).
R spart nur 5 B, ist aber schlechter (zusaetzlicher voller Shell-Abruf pro Wechsel, geht
verloren: laufende Bestaetigung/Anzeige, und ohne Speicher wirkungslos) und wird
verworfen. Codes ohne Beschriftung wuerden die Bedienbarkeit (Barrierefreiheit,
Verstaendlichkeit) einschraenken und werden nicht empfohlen. Die Provisionierungs-Shell
ist von der Grenze nicht betroffen (rund 3,6 KB gesamt, grosse Reserve unter 11 264 B;
exakte Messung bei der Umsetzung).

**Kopfgrenze.** Der empfohlene Kandidat belegt den Kopf mit maximaler Sequenz bis
1 534 B, 2 B unter dem Testlimit 1 536 B; jede weitere Kopfmehrung fuehrt zu HOLD (kein
stilles Anheben von `reserve`/Testlimit).

**Regel (HOLD).** Ohne ausdrueckliche Ownerentscheidung zu D1 wird nichts umgesetzt. Nach
einer Entscheidung gilt: die gemessene Shell darf den Messstand des gewaehlten
Kandidaten nicht um mehr als den dann ausgewiesenen Spielraum ueberschreiten; Ueberschreitung
fuehrt zu HOLD, nicht zu einer weiteren Erhoehung oder Kompaktierung.

## 6. Bewertete Alternativen (kleinste tragfaehige zuerst)

1. **Clientseitige Wahl A2 mit einmaliger Limiterhoehung (empfohlen, D1-A).** Kleinste
   Aenderung: ein Feld im Kopf, ca. 160 B JS, keine Server-/Adapter-/Tabellenaenderung,
   keine neue Laufzeitlast. Benoetigt mindestens 11 422 B, praktisch **11 456 B**
   (Vielfaches von 64, +192 B ueber 11 264 B; Reserve danach 162 B = 128 B + 34 B).
2. **Codes statt Endonyme (D1-B).** −43 B, Limit mind. 11 379 B, praktisch **11 392 B**
   (+128 B, Reserve 141 B). Funktioniert, ist aber weniger verstaendlich.
3. **Serverseitig gewaehlter Einzelsprachen-Suffix (Weg E, nicht empfohlen fuer diesen
   Slice).** Der Suffix wird heute als ein `string_view` ausgeliefert; drei vollstaendige
   Einzelsprachen-Suffixe (je ca. 7,6–7,8 KB statt 9,6 KB) liessen die Gesamtgroesse je
   Antwort um etwa 1,9 KB sinken und das Limit unangetastet. Voraussetzungen: Sprach-Cookie
   (lesbar fuer JS) mit exakt tokenisiertem Parsing aus `metadata.cookie`
   (<= 512 B), drei Suffix-Varianten (compile-time zusammengesetzt, +~14 KB Flash), drei
   eingefrorene Golden-Generatoren, Erstaufruf mit einem zusaetzlichen Reload (kein
   `Accept-Language`) und Aenderungen in Routing und Tests. Das ist ein Architektur-/
   Vertragsumbau mit eigenem Risiko und wird nicht vorsorglich vorgebaut; er bleibt eine
   Option fuer einen eigenen Plan.
4. **Kompaktierung vorhandener Labels.** Wuerde bestehende, vom Owner freigegebene Inhalte
   aendern und ist ausdruecklich nicht Teil dieses Auftrags.
5. **Separates cachebares Asset / neue Route.** Waere ein neuer Asset-/Cache-/Auth-
   Vertrag; weiterhin nicht gewaehlt.
6. **Reload-basierter Wechsel (R), `sessionStorage`.** Siehe §5 bzw. D2.

## 7. Ressourcenfolgen (ohne Hardware)

| Posten | Folge | Status |
|---|---|---|
| Shell-Flash / Antwortgroesse | A2: +160 B Suffix im Flash, +118 B Kopf pro Antwort (Heap-String < `reserve(1536)`, keine Reallokation bei <= 1 534 B), Gesamt +278 B je Shell-Abruf | MEASURED (Bytes) |
| Code-Flash / Dauer-RAM / Stack / Heap im Geraet | keine C++-Logik, kein Codec, kein Handler, keine Application: **0 B** Dauer-RAM, kein neuer Stackframe; Heap nur die +118 B im vorhandenen Kopf-String je Shell-Abruf (transient) | Designziel, per Sektions- und `-fstack-usage`-Vergleich offline zu belegen |
| Provisionierungs-Shell | ca. +300 B Kopf/Suffix, weit unter der Grenze | ESTIMATED, exakte Messung bei Umsetzung |
| Browser | Wechsel: ein Lauf von `sl`, `dr`, `ui`; kein Netzwerk | Fake-DOM-Test |

Eine Hardware-RAM-Nachmessung ist fuer diesen Slice nicht vorgesehen (kein neuer
Laufzeit-Codepfad im Geraet, nur statische Bytes); `RAM_PASS=NOT_PROVEN` bleibt, hoechstens
`RAM_OFFLINE_ESTIMATED`. Falls der Owner dennoch eine Messung wuenscht, ist das ein eigener
Auftrag.

## 8. Ownerentscheidungen (offen)

| ID | Frage | Empfehlung |
|---|---|---|
| **D1** | Einmaliges weiteres Shell-Limit: **(A)** 11 456 B mit A2 / Endonyme (mind. 11 422 B), **(B)** 11 392 B mit Codes + `aria-label` (mind. 11 379 B), **(C)** Weg E (Einzelsprachen-Suffix) in eigenem Plan, Limit unveraendert, **(D)** zurueckstellen | **(A)** |
| D2 | Speicherort: `localStorage` (bleibt nach Browser-Neustart, gilt in neuen Tabs) oder `sessionStorage` (nur Tab/Sitzung); WEB_UI erlaubt beides | `localStorage` |
| D3 | Platzierung des Auswahlfelds: am Seitenanfang vor allen Abschnitten (kostenneutral) | am Seitenanfang |

D2/D3 sind kleine Festlegungen mit Vorschlag; D1 ist das einzige Gate. Hardware-/Aktor-
freigabe gibt es nicht (`ACTUATOR_RELEASE=NO`).

## 9. Umsetzungsschnitte nach Freigabe der exakten Plan-SHA und D1

1. **C1 – Dashboard-Shell.** Kopf: Auswahlfeld; Suffix: `sl`, Speicherlesen/-schreiben,
   Validierung. `kMaximumWebShellBytes`, eingefrorenes Golden (`shell_golden.hpp`) und
   Test-`kShellLimit` auf den gewaehlten Wert (nur wenn D1 es entscheidet).
2. **C2 – Provisionierungs-Shell.** Gleiche Wahlmechanik fuer deren Tabelle, `T` neu
   zuweisbar, Regex-Validierung statt `L[g]`, gemeinsamer Speicherschluessel.
3. **C3 – Tests, Doku, Evidence.** JS-/Native-Tests, WEB_UI („Sprache je Browser“ umgesetzt,
   Groessenschranke), ACCEPTANCE_TESTS (neue SIM-IDs), ROADMAP, Offline-Ressourcenevidence.
   Ergebnis auf exakten Quell-/Testcommit binden; Builder-Self-Check, dann Independent Review.

## 10. Gezielte Nachweise

| Bereich | Faelle |
|---|---|
| Echter extrahierter JS-Test (`scripts/test_web_shell_polling.mjs`, Fake-DOM + `localStorage`-Mock) | Vorschlag aus `navigator.language` (de/en/es/fr→en); gespeicherter gueltiger Wert hat Vorrang; **ungueltig/manipuliert** (`xx`, `constructor`, leer, zu lang) wird ignoriert; Wechsel de→en→es: alle statischen Texte (Login, Passwort-Label, Abmelden, Auth-Status, Warntext, `<html lang>`), Dashboard-Ueberschrift, Zeilen, Verbindungsstatus, Aktionsknoepfe und „Sicher?“ in der neuen Sprache; **Reload-Simulation** (neue Fixture mit gleichem Speicher) stellt die Wahl wieder her; **zwei Fixtures mit getrenntem Speicher** bleiben unabhaengig; **blockierter Speicher** (Lesen und Schreiben werfen) → kein Fehler, Wechsel wirkt fuer die Seite; Wechsel loest **keinen** Request aus; `P`, Generation, Frische, Single-Flight und alle Aktionssperren in allen `homeMode`-Werten unveraendert; alle bisherigen 56 Faelle erhalten |
| Native `test_web_application_routes` | Kopf enthaelt genau ein Auswahlfeld mit drei Optionen in jedem Authentifizierungszustand; Provisionierungs-Shell enthaelt es ebenfalls; kein serverseitiger Sprachzustand (kein Cookie/Header, Schluessel `lang` nur im Shell-Text); jede Locale-Tabelle weiterhin vollstaendig (`labelCount`==3); eingefrorener Generator stimmt byteweise; Kopf <= 1 536 B mit maximaler Sequenz; Gesamtgroesse <= gewaehltes Limit und Reserve >= 128 B in allen Zustaenden; Fehlerpfade loeschen den Suffix unveraendert; `/api/v1/*` und Codec unveraendert |
| Setup-/Login-Texte | Provisionierungs-Shell: alle Texte (`a,b,c,e,x,y,g` plus Ergebnistexte) in allen drei Sprachen vorhanden und beim Wechsel neu gesetzt; Login-Text der Dashboard-Shell |
| Ressourcen offline | Sektionsvergleich (`.dram0.*`, `.iram0.*` Nulldelta erwartet), `-fstack-usage`-Frames unveraendert, Shell-Bytemessung; beide ESP-Profile bauen |

Geplante Befehle (nicht jetzt):

```bash
node scripts/test_web_shell_polling.mjs
pio test -e native -f test_web_application_routes -f test_fermentation_ui_models -f test_renderer_boundary
python3 scripts/build_esp_idf_profiles.py all --require-clean-source-tree
export PRE_READY_EXPECTED_HEAD="$(git rev-parse HEAD)"
bash scripts/run_pre_ready_gates.sh self-check
```

Pre-Ready nur nach Independent Review, `OPEN_BLOCKERS=0` und ausdruecklicher Owneranweisung.
Kein Browser-/Geraetetest in diesem Slice (Browserqualifikation weiter `NOT_PROVEN`).

## 11. Risiken

- Kopfgrenze 1 534/1 536 B (2 B Luft) – jede Kopfmehrung ist HOLD.
- `localStorage` kann fehlen oder werfen; Lesen ist geschuetzt, Schreiben folgenlos
  (siehe §4); beides wird getestet.
- Fake-DOM-Test beweist die Logik, nicht das Verhalten echter Browser (Autofill der
  Auswahl, assistive Technik); Browserqualifikation bleibt `NOT_PROVEN`.
- Entscheidung E (Weg 3) waere ein eigener, groesserer Vertrags-/Architekturplan; sie
  wird hier nicht vorgebaut.
- Historische Transport-/Latenzbefunde und alle R1-Restpunkte (Navigation, Service/System,
  weitere Webbereiche) bleiben offen und separat.
