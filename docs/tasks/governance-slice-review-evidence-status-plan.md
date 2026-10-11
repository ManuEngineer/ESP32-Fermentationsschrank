# Governance – Slice-Review, Evidence-Status und Post-Merge-Statuspflege (Plan, Revision 2)

Eigenstaendige Fassung; sie ersetzt Revision 1 (`d11c1de`) vollstaendig (Delta: Live-Statusabgleich
von Hardware-Issue #192, Abschnitte 1, 4, 5, 6).

Nur Markdown. Keine Produktcode-, GPIO-, ADC-, Safety-, Build-, Toolchain-, Runner-, CI-,
Hardware-, Modell-/Compute- oder Mergevertragsaenderung; keine neue Pruefebene, kein Checker,
kein Template, keine zweite Status-SSOT. `ACTUATOR_RELEASE=NO`. Der fruehere Auftrag
„Governance Reviewphasen/Evidence“ ist bezueglich des PR-#196-Gates ueberholt; es gibt keine
rueckwirkende Gate-Freigabe und keine PR-Wiedereroeffnung.

## 1. Ausgangslage (verifiziert, `main` = `f33e8593c79920aca9084528547a220f284b9f42`)

- Gemergt: PR #189 (`9beb68f`), #191 (`df5a3dd`), #195 (`92d6b823`), #196 (`f33e8593`;
  Software-HEAD `1a525217865f97e318211bba4e2b04e809b19123`, `pre-ready/local=success`
  mit Beschreibung „host+esp PASS“).
- `AGENTS.md` (Gatefolge Independent Review → `OPEN_BLOCKERS=0` → Ownerfreigabe →
  Pre-Ready → Ready) ist bereits eindeutig und bleibt unveraendert.
- `docs/AGENT_WORKFLOW.md` Abschnitt 7 verlangt den Builder-Self-Check „vor jeder normalen
  Uebergabe an den Independent Review“; Abschnitt 10 und `docs/CI_AND_QUALITY_GATES.md`
  wiederholen diese Formulierung („vor jeder normalen Uebergabe an den Independent Review“
  bzw. „Vor der Uebergabe an den Independent Review“). Eine Unterscheidung zwischen einem
  vom Owner gewuenschten Slice-/Zwischenreview und dem abschliessenden Independent Full
  Review fehlt.
- Evidence: `docs/audits/ISSUE33_EXECUTION_EVIDENCE.md` nennt im Kopf
  `Hardware, Flash, Pre-Ready-Lauf, Self-Check = NOT_RUN`, obwohl Abschnitt S5
  `BUILDER_SELF_CHECK=PASS` (HEAD `3a6483a`) und der PR-HEAD `1a52521`
  `pre-ready/local=success` hat. `docs/audits/ISSUE32_S1_EXECUTION_EVIDENCE.md` hat dasselbe
  Muster (Reviewblock `PRE_READY_LOCAL_GATES=NOT_RUN`, obwohl `pre-ready/local=success` auf
  dem PR-HEAD `1c15da6` vorlag).
- Tracking: `docs/ROADMAP.md` fuehrt #19/#32/#33/#192 teils noch mit „Draft-PR“-/
  „Review ausstehend“-Staenden; die Tabelle der gemergten PRs enthaelt #191/#195/#196 nicht.
  Live-Issues: #30 und #190 nennen PR #189 noch als offen, #19 und #192 nennen PR #191 als
  nicht gemergt bzw. S1 (`ResetEligibleNoRuntime`) als noch nicht implementiert, #33 fuehrt `REAL_ESP_IDF_BTS7960_ADAPTER_EXISTS=NO`, #32 hat keinen
  Merge-Statusabgleich.

## 2. Issue-Zuordnung (Ownerentscheid O-G1 = Ja: zugeordnet, Issue #198)

O-G1 ist entschieden: das Issue ist als **#198**
(<https://github.com/ManuEngineer/ESP32-Fermentationsschrank/issues/198>) angelegt und diesem
Plan/PR #197 zugeordnet; es wird nicht erneut erstellt. Der folgende Entwurf bleibt als
Ursprungstext erhalten.

Es existiert kein inhaltlich passendes **offenes** Governance-Issue (#177 = Lizenz, nicht
passend; die verwandten #145/#154/#150 sind geschlossen und werden nicht wiedereroeffnet).
Vorschlag (umgesetzt als #198): neues Issue

> **[Governance] Slice-Review vs. finaler Independent Review, Evidence-Status und
> Post-Merge-Statuspflege praezisieren**
> Scope: (1) eine kanonische Unterscheidung Slice-/Zwischenreview ↔ abschliessender Independent
> Full Review in `docs/AGENT_WORKFLOW.md`; (2) Evidence-Zeitbezug (Kopf = aktueller Stand,
> Abschnitte = Schnittstand) und Bereinigung der Statuswidersprueche in den Evidence-Dateien
> #32/#33; (3) Post-Merge-Roadmap-/Issue-Statusabgleich fuer #19/#30/#32/#33/#190/#192. Nur
> Markdown. Nicht-Scope: Runner, CI, Hardware, Safety, Merge-/Modellvertraege.

Die Zuordnung zu #198 ist administrativ in Plan und PR-Beschreibung nachgetragen; sie ist
keine neue Planentscheidung.

## 3. Textdelta (minimal)

1. **`docs/AGENT_WORKFLOW.md` Abschnitt 7 (einzige kanonische Stelle fuer die
   Unterscheidung):** Der Satz „Nach einer tatsaechlichen Implementation und vor jeder
   normalen Uebergabe an den Independent Review …“ wird auf die **abschliessende**
   Uebergabe an den Independent Full Review bezogen. Neuer Kurzabsatz:
   „Ein vom Owner gewuenschter Slice-/Zwischenreview einzelner Umsetzungsschnitte ist eine
   gezielte Code-, Vertrags- und Evidence-Pruefung des jeweiligen Schnitts. Er erzwingt weder
   den Builder-Self-Check noch einen neuen Full Review. Der Self-Check bleibt der
   einmalige Pflichtnachweis vor der Uebergabe an den abschliessenden Independent Full
   Review und wird bei materiellen spaeteren Aenderungen nach den bestehenden Regeln
   (Fix Verification, Materialitaet) wiederholt. Der Self-Check enthaelt nicht das statische
   Stack-Gate; dieses gehoert zur `esp`-Phase des Pre-Ready-Laufs.“
   Fix Verification, Materialitaetsregeln, Owner-/Pre-Ready-/Ready-/Merge-Gates bleiben
   unveraendert.
2. **Echo-Formulierungen nur bei tatsaechlichem Widerspruch:** `AGENT_WORKFLOW.md`
   Abschnitt 10 und `CI_AND_QUALITY_GATES.md` (Abschnitt Self-Check, eine bis zwei Stellen)
   sagen „vor jeder normalen Uebergabe an den Independent Review“; sie werden auf „vor der
   Uebergabe an den abschliessenden Independent Full Review“ angeglichen. `AGENTS.md`
   bleibt unveraendert (kein Widerspruch).
3. **Evidence-Zeitbezug (eine Regel, `AGENT_WORKFLOW.md` Abschnitt 7, direkt nach dem
   Absatz aus Punkt 1):** „Bei laufend fortgeschriebenen Evidence-Dateien zeigt der
   allgemeine Statuskopf den neuesten verifizierten Stand mit Datum/HEAD; Statusangaben in
   Schnittabschnitten sind historische Schnittnachweise und bleiben als solche erkennbar.
   Ein Gate-Status wird nur mit dem tatsaechlich verifizierten Nachweis (Runner-Ausgabe oder
   GitHub-Commit-Status) als `PASS` ausgewiesen; Einzelphasen ohne eigenen Nachweis werden
   nicht einzeln als `PASS` gefuehrt.“
4. **Evidence-Bereinigung (keine Loeschung historischer Nachweise):**
   - `ISSUE33_EXECUTION_EVIDENCE.md`: Kopfblock ersetzt `Self-Check = NOT_RUN` /
     `Pre-Ready-Lauf = NOT_RUN` durch den aktuellen Stand: `BUILDER_SELF_CHECK=PASS`
     (HEAD `3a6483a`, S5), `pre-ready/local=success` am PR-HEAD `1a52521` (GitHub-Commit-
     Status „host+esp PASS“, am 2026-10-09 per API verifiziert), PR #196 gemergt
     (`f33e8593`); Hardware, Flash und reale Peltiertests weiter `NOT_RUN`; Hinweis, dass die
     Abschnitte S1–S4 Schnittstaende sind und deren `NOT_RUN` damals galt. Kein Einzel-PASS
     fuer Host-/ESP-/Stack-Ausfuehrung ueber die genannte Commit-Status-Beschreibung hinaus.
   - `ISSUE32_S1_EXECUTION_EVIDENCE.md`: der Review-Block und die Kopfangaben erhalten den
     aktuellen Stand (PR #195 gemergt `92d6b823`, `pre-ready/local=success` auf `1c15da6`
     verifiziert) mit gleichem Zeitbezug; der Reviewblock bleibt als historischer Stand
     erkennbar.

## 4. Post-Merge-Tracking

- **`docs/ROADMAP.md`** (nur die tatsaechlich veralteten Zeilen): #19 (PR #191 gemergt,
  `df5a3dd`; Ablauf A, Journal usw. unveraendert offen), #30 (Software gemergt, #190 offen),
  #32 (Software in PR #195 gemergt; Hardware `NOT_RUN`), #33 (Software in PR #196 gemergt;
  Hardware `NOT_RUN`, `REAL_PELTIER_TEST=NOT_RUN`), #192 (Beginn nicht mehr „erst nach
  Merge von #191“; Hardware weiter `NOT_RUN`/`BLOCKED_HARDWARE`, keine Evidence aus dem
  geschlossenen PR #194 in `main`), #190 (Voraussetzung PR #189 erfuellt). In der Tabelle
  der gemergten PRs werden #191, #195 und #196 mit ihren Merge-Commits ergaenzt. Reihenfolge
  der realen Tests unveraendert `#190 → #32 → #33`; Gesamtabnahme/Belastung #36/#37.
- **Live-Issue-Texte** (administrativ, nur ein vorangestellter datierter Statusabgleich-
  Block nach dem bereits bei #30/#190 verwendeten Muster; Originaltext und Akzeptanzkriterien
  bleiben; Schreibweg `gh api` PATCH; betroffen sind #19, #30, #32, #33, #190 und #192): #30
  (Software in #189 gemergt; Hardware H1–H7 in #190 offen), #190 (PR #189 gemergt, Software-Voraussetzung erfuellt, H1–H7 `NOT_RUN`),
  #32 (Software in #195 gemergt; Hardware `NOT_RUN`), #192 (PR #191 gemergt, Merge-Commit
  `df5a3ddf41889b46f9b4d3c64085092a88e25a0a`; S1 `ResetEligibleNoRuntime` softwareseitig
  implementiert; `HW-19-R01..R03` weiterhin `NOT_RUN`, `ACTUATOR_RELEASE=NO`; die bestehenden
  Hardware-Akzeptanzkriterien, historischen Aussagen und urspruenglichen Gate-Texte werden
  weder geloescht noch als bestanden deklariert), #33 (Software in #196 gemergt;
  `REAL_ESP_IDF_BTS7960_ADAPTER_EXISTS=NO` ist ueberholt: Adapter existiert in `main`,
  Hardware-/Adaptersicherheitsverifikation `PENDING`, `REAL_PELTIER_TEST=NOT_RUN`), #19
  (PR #191 gemergt). **Keine** Issue wird geschlossen, keine Hardware-, Safety- oder
  Aktorfreigabe behauptet.
- **#30:** vor einer Empfehlung wird der vorhandene Kriterientransfer #30 → #190 (DoD,
  H1–H7, Scope) gegen den Live-Text geprueft; Ergebnis: Hinweis an den Owner, ob #30 aus
  Sicht der Agentenpruefung administrativ schliessbar ist. Der Agent schliesst nicht.
- Historische PR-Beschreibungen werden nicht als Statusquelle umgeschrieben. Es werden keine
  neuen Test-Issues angelegt (reale Tests: #190 DS18B20, #32 Luefter/MOSFET, #33
  BTS7960/Peltier, #36/#37 Gesamtabnahme); keine R_IS/L_IS-Arbeit fuer R1.

## 5. Schnitte und Pruefungen

1. **S1** Governance-Text (Abschnitte 3.1–3.3: `AGENT_WORKFLOW.md`, ggf. `CI_AND_QUALITY_GATES.md`).
2. **S2** Evidence-Bereinigung (3.4).
3. **S3** ROADMAP und Live-Issue-Statusbloecke (4, inkl. #192); #30-Befund an den Owner.

Pruefungen (nur Markdown): `git diff --check`; Textsuche, dass keine Restformulierung
„vor jeder normalen Uebergabe an den Independent Review“ im Widerspruch verbleibt; Abgleich
jeder geaenderten Statusaussage mit dem Live-Stand (`gh pr view`, Commit-Status,
`gh issue view`, einschliesslich der tatsaechlichen Live-Texte von #192 und #19);
keine Pfade/Links gebrochen. Der Pre-Ready-Lauf dieses PR ergibt voraussichtlich `NOT_REQUIRED_MARKDOWN_ONLY` und erfolgt erst nach Review und
ausdruecklicher Ownerfreigabe.

## 6. Offene Ownerentscheidungen

- **O-G1** Issue-Zuordnung (Abschnitt 2): entschieden, Ja, #198.
- **O-G2 (Ja, Ownerfreigabe)** Der Agent darf die Statusbloecke der Live-Issues #19/#30/#32/#33/#190/#192 nach dem
  vorhandenen Muster voranstellen (Empfehlung: ja, rein administrativ, Originaltext
  unveraendert). Andernfalls liefert der Agent die Texte im PR und der Owner uebernimmt sie.

## 7. Risiken

Die Praezisierung darf die Pflicht zum Self-Check vor dem **finalen** Review nicht
aufweichen: der Text benennt ihn ausdruecklich als einmaligen Pflichtnachweis. Roadmap-
und Issue-Aussagen duerfen nur den gemergten Software-Stand ausweisen, keinen
Hardwarestand. Die Evidence-Regel fuehrt keinen Pruefmechanismus ein.
