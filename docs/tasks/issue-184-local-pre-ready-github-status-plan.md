# Plan – Issue #184: lokalen Pre-Ready-PASS als GitHub-Merge-Gate nutzen

Status: Planungsphase (`IMPLEMENTATION_BLOCKED_PENDING_PLAN_APPROVAL`)

Issue: `#184`
Pull Request: `#185` (Draft)
Basis-Branch: `main`
Basis-SHA: `dc938b2bc5cc9f9f2e93065ed5a9009eb99fcfda`
Auftrag: `Issue184_PreReady_GitHub_Gate_Auftrag.md` (Owner)
Planrevision: `1`
Implementation: `NOT_STARTED`
PRODUCT_CODE_CHANGE: `NO`
ACTUATOR_RELEASE: `NO`

Dieser Plan ist ein eigenstaendiger CI-/Governance-Plan. Er aendert weder
Produktcode noch Hardware-, Safety- oder Aktorvertraege.

## 1. Ziel und Nicht-Ziele

Ziel (Owner-Vorgabe, hier nur konkretisiert):

```text
Independent Review / Fix Verification
-> OPEN_BLOCKERS=0
-> Owner autorisiert finalen lokalen Pre-Ready
-> lokaler host + esp Lauf auf exakt finalem, gepushtem PR-HEAD
-> Commit Status `pre-ready/local=success` auf genau diesem SHA
-> Owner setzt Ready for review
-> Required Status Check `pre-ready/local` + strict/up-to-date
-> Merge-Gate
```

Normale Feature-PRs fuehren die vollstaendigen Engineering-Gates nur einmal
aus (lokal). Die schwere GitHub-CI bleibt als Clean-Room-/Artefaktpfad fuer
Gate-/Build-/Toolchain-Aenderungen und manuell verfuegbar. Ein Status eines
aelteren SHA gibt einen spaeteren Push nie frei.

Nicht-Ziele (Owner-Grenzen): kein Server, keine Datenbank, keine GitHub App,
keine signierte Attestierungs-PKI, kein Self-Hosted Runner, keine zweite
Testdefinition, keine automatische Mergefunktion, keine Produktcodeaenderung,
keine separaten `pre-ready/host`-/`pre-ready/esp`-Required-Checks.

Ehrliche Grenze des Mechanismus: Der Status ist keine kryptografische
Attestierung. Jeder mit Schreibrecht und Token kann einen Status manuell
setzen. Der Wrapper ist Ablaufdisziplin fuer ein Owner-gefuehrtes Einzelrepo,
kein Sicherheitsbeweis gegen den Owner selbst (so vom Auftrag festgelegt).

## 2. Verifizierte Live-Ausgangslage

Alle Punkte am 2026-10-07 gegen Repository und GitHub geprueft.

- `origin/main` = `dc938b2bc5cc9f9f2e93065ed5a9009eb99fcfda` (Merge PR #183);
  keine offenen PRs; Issue #184 `OPEN`.
- Branch-Protection von `main` (per `gh api` lesbar): Pull-Request-Pflicht
  mit 0 Approvals, `enforce_admins=true`,
  `required_conversation_resolution=true`, keine Force-Pushes/Loeschungen.
  **Es gibt heute keinen `required_status_checks`-Block** – weder die schwere
  CI noch strict/up-to-date sind technisch required; `GitHub-CI PASS` ist
  heute ein reines Dokumentations-/Ownergate. Rulesets: keine. Folge fuer die
  Migration: es existiert kein schwerer Required Check, der entfernt werden
  muesste (Abschnitt 7).
- `.github/workflows/build.yml`: Trigger nur `pull_request`
  (`opened`, `ready_for_review`, `synchronize`, `reopened`) mit
  `paths-ignore: **/*.md`; Job nur bei `draft == false`; Concurrency-Gruppe,
  `SOURCE_GIT_SHA` und der Draft-Guard lesen `github.event.pull_request.*`.
  Der Job ruft `run_pre_ready_gates.sh host` und `esp` auf, danach die
  CI-only Artefakt-Scanabdeckung und den Artefakt-/Privacy-Scan, dann Uploads.
- `scripts/run_pre_ready_gates.sh` (SSOT der Engineering-Gates) kennt `host`,
  `esp`, `self-check`, prueft `PRE_READY_EXPECTED_HEAD` und ruft u. a.
  `generate_board_profile_header.py`, `build_report.py`,
  `build_esp_idf_profiles.py`, `check_architecture_boundaries.py`,
  `check_secrets.py`, `selftest_quality_gates.py`,
  `run_esp_idf_static_analysis.py` auf.
- `scripts/selftest_quality_gates.py` ist der bestehende Selbsttest-Sammler
  (Teilpruefungen via `--selftest` der Einzelskripte, in der `host`-Phase).
- Lokale Voraussetzungen vorhanden: `gh` angemeldet (`ManuEngineer`),
  PyYAML installiert, `shellcheck` nicht installiert (nicht vorausgesetzt).
- Der lokale Arbeitsbaum enthielt eine Aenderung an `.codex/config.toml`
  (projektlokale Modellvorgaben entfernt). Auf Owner-Anweisung ist sie als
  eigener Commit Teil dieses PR; sie hat keine Gate-/Produktwirkung.

## 3. Quellen

`AGENTS.md`, `docs/AGENT_WORKFLOW.md`, `docs/CI_AND_QUALITY_GATES.md`,
`.github/workflows/build.yml`, `scripts/run_pre_ready_gates.sh`,
`scripts/selftest_quality_gates.py`, `docs/ROADMAP.md`,
`docs/tasks/issue-150-pre-ready-ci-parity-plan.md` (Runner-Vertrag).
GitHub-Vertrag: Commit Status API (`POST /repos/{owner}/{repo}/statuses/{sha}`
mit `state` = `pending|success|failure|error`, `context`, `description` bis
140 Zeichen); Required Status Checks muessen auf dem neuesten PR-Commit
erfolgreich sein; strict verlangt einen zum Base-Branch aktuellen PR.

## 4. Umsetzungsschnitte (Stopp nach jedem Commit)

Nach jedem Commit wird angehalten und die Ownerfreigabe abgewartet.

| Commit | Inhalt |
|---|---|
| C0 | dieser Plan + ROADMAP-Startstatus (Plan-Phase) |
| C1 | `scripts/run_pre_ready_and_publish.sh` + Fixture-Selbsttest |
| C2 | `build.yml`: Trigger/Paths/Dispatch + Trigger-Vertragscheck im Selbsttest-Sammler |
| C3 | Governance-Dokumente + ROADMAP |

### 4.1 C1 – Wrapper `scripts/run_pre_ready_and_publish.sh`

Der Runner `run_pre_ready_gates.sh` bleibt unveraendert und ohne GitHub-API.
Der Wrapper enthaelt keine Testliste und keine Toolchain-Wahrheit; er ruft
ausschliesslich `bash scripts/run_pre_ready_gates.sh host` und `... esp`.

Ablauf (jede Pruefung fail-closed; Abbruch ohne `success`):

1. Voraussetzungen: `git`, `gh`, `gh auth status` erfolgreich; Aufruf ohne
   Argumente (sonst Usage, Exit 2).
2. Normaler Branch (kein detached HEAD), Branch ist nicht `main`.
3. `git status --porcelain` leer (ignorierte Buildausgaben zaehlen nicht).
4. Upstream gesetzt und `origin/<branch>` (`@{upstream}` muss auf `origin`
   zeigen).
5. `git fetch origin main` und `git fetch origin <branch>`.
6. `HEAD == origin/<branch>` (der getestete Stand ist bereits gepusht).
7. `origin/main` ist Ancestor von `HEAD` (PR ist aktuell zu `main`).
8. Merken: `TESTED_HEAD=$(git rev-parse HEAD)`,
   `MAIN_BEFORE=$(git rev-parse origin/main)`.
9. Status `pending` auf `TESTED_HEAD` publizieren; schlaegt das fehl: Abbruch
   (ohne Gatelauf – ohne publizierbaren Status ist der Lauf wertlos).
10. `export PRE_READY_EXPECTED_HEAD=$TESTED_HEAD`.
11. `bash scripts/run_pre_ready_gates.sh host`; danach `esp` (Aktivierung der
    ESP-IDF-Umgebung siehe Entscheidung D1).
12. Abschlusspruefung vor SUCCESS: erneut `git fetch origin main` und
    `git fetch origin <branch>`; Arbeitsbaum sauber; `HEAD == TESTED_HEAD`;
    `origin/<branch> == TESTED_HEAD`; `origin/main == MAIN_BEFORE` **und**
    Ancestor von `HEAD`. Ist `main` weitergelaufen: kein SUCCESS.
13. Nur wenn Schritt 11 beide Phasen mit PASS beendet hat und Schritt 12
    vollstaendig besteht: Status `success` auf `TESTED_HEAD`.

Fehlerpfad: Ein `EXIT`-Trap publiziert bei jedem Abbruch nach Schritt 9, bei
dem noch kein `success` gesetzt wurde, best-effort `failure` auf
`TESTED_HEAD` (Gatefehler, Abschlusspruefung, Signal).
Ist GitHub nicht erreichbar, bleibt der zuvor gesetzte `pending` stehen; das
ist fail-closed, da `pending` keinen Required Check erfuellt. Der Wrapper
beendet in jedem Nicht-Erfolgsfall mit Exit != 0 und gibt maschinenlesbar
`PRE_READY_LOCAL_GATES=PASS|FAILED|BLOCKED` und `PRE_READY_TESTED_HEAD=<sha>`
aus (`BLOCKED` = fehlende Voraussetzung vor dem Gatelauf, `FAILED` =
ausgefuehrt und fehlgeschlagen oder Abschlusspruefung verletzt).

Status-Vertrag: Context genau `pre-ready/local`; Beschreibung
`pre-ready/local pending <short-sha>`, `<short-sha> host+esp PASS` bzw.
`<short-sha> FAILED|BLOCKED: <Grund kurz>` (<= 140 Zeichen); kein
`target_url`; Aufruf ausschliesslich ueber
`gh api -X POST repos/{owner}/{repo}/statuses/<sha> -f state=... -f
context=... -f description=...`. Keine Tokens/Secrets im Repo oder in
Ausgaben. Es gibt nur diesen einen aggregierten Kontext.

Repo-Identitaet: `{owner}/{repo}` wird von `gh` aus dem Arbeitsverzeichnis
aufgeloest; der Wrapper vergleicht `gh repo view --json nameWithOwner` mit der
Origin-URL und bricht bei Abweichung ab, damit der Status nicht auf einem
Fork/Upstream landet.

Entscheidung D1 (ESP-IDF-Umgebung, Empfehlung A): Das Dokument beschreibt
heute `host` ohne und `esp` mit aktiviertem `export.sh` (wie in GitHub-CI).
Empfehlung **A**: Der Wrapper verlangt gesetztes `IDF_PATH`/`IDF_TOOLS_PATH`
(sonst `BLOCKED`), fuehrt `host` in der aufrufenden Umgebung aus und aktiviert
fuer die `esp`-Phase `. "$IDF_PATH/export.sh"` in einer Subshell – exakt die
bereits dokumentierte Sequenz, keine Installation, keine neue Pfadlogik. Der
Runner prueft die Provenienz danach unveraendert. Alternative **B**: Der
Owner aktiviert die Umgebung vor dem Wrapperaufruf fuer beide Phasen; das
setzt voraus, dass die `host`-Phase unter aktivem `export.sh` identisch
besteht (Python-/PyYAML-Aufloesung), was in C1 empirisch gezeigt werden
muesste. Auswahl durch den Owner mit der Planfreigabe.

### 4.2 C1 – Fixture-Selbsttest

Erweiterung des bestehenden `scripts/selftest_quality_gates.py` (keine zweite
Testlogik, kein neues Framework): eine Funktion `selftest_pre_ready_attestation`
erzeugt je Szenario ein temporaeres Git-Repo mit lokalem Bare-`origin`, kopiert
den echten Wrapper nach `scripts/`, legt einen Stub-Runner
`scripts/run_pre_ready_gates.sh` (steuerbar fuer Phase-Ergebnis, optionalen
Push/`main`-Fortschritt/Commit/Arbeitsbaumaenderung waehrend des Laufs) und
ein Stub-`gh` auf den `PATH` (protokolliert alle Aufrufe in eine Datei und
kann Auth- und API-Fehler simulieren). Der echte Runner und das echte `gh`
werden nie aufgerufen; im Fixture entsteht kein Netzwerkzugriff. Die
Temp-Repos setzen Git-Identitaet und `init.defaultBranch` explizit und
isolieren die Konfiguration (`GIT_CONFIG_GLOBAL=/dev/null`,
`GIT_CONFIG_NOSYSTEM=1`), damit der Test auf dem GitHub-Runner ohne
Identitaet und unabhaengig von lokaler Config reproduzierbar ist. Ein
Signal-Szenario ist bewusst nicht Teil des Katalogs (flakeanfaellig, ueber den
Mindestkatalog hinaus); der `EXIT`-Trap wird ueber den Gatefehlerpfad
getestet.

Szenarien (je mit Erwartung auf dem Aufrufprotokoll):

| Szenario | Erwartung |
|---|---|
| host+esp PASS, alles stabil | Exit 0; Reihenfolge `pending`, dann genau ein `success` auf `TESTED_HEAD` mit Kurz-SHA und `host+esp PASS` |
| `host` FAIL | Exit != 0; `failure`, kein `success`; `esp` wird nicht gestartet |
| `esp` FAIL | Exit != 0; `failure`, kein `success` |
| HEAD aendert sich waehrend des Laufs | kein `success`; `failure` |
| Arbeitsbaum schmutzig (vorher / nachher) | kein `success`, vorher: kein Gatelauf |
| Upstream-Abweichung (`HEAD != origin/<branch>`) | kein Gatelauf, kein `success` |
| kein Upstream / detached HEAD / Branch `main` | `BLOCKED`, kein Gatelauf |
| `origin/main` nicht Ancestor von `HEAD` | kein Gatelauf, kein `success` |
| `main` laeuft waehrend des Laufs weiter | kein `success`; `failure` |
| Remote-Branch wird waehrend des Laufs weitergeschoben | kein `success` |
| `gh` fehlt / `gh auth status` schlaegt fehl | `BLOCKED`, kein Gatelauf, Exit != 0 |
| API-Fehler beim `pending` | Abbruch ohne Gatelauf |
| API-Fehler beim `success` | Exit != 0, nie ein stilles PASS |
| Status-Kontext/State-Werte | nur `pre-ready/local`; nur `pending|success|failure` |

Zusaetzlich `bash -n scripts/run_pre_ready_and_publish.sh`. `shellcheck` wird
nicht vorausgesetzt.

### 4.3 C2 – `build.yml`: Trigger, Paths, Dispatch

Neuer Trigger-Vertrag:

- `workflow_dispatch` (manueller Full-CI-Lauf);
- `pull_request` (Typen unveraendert) mit **positiver** `paths`-Liste
  (`paths-ignore` entfaellt, beide zusammen sind fuer ein Event nicht
  zulaessig; Markdown-only loest damit weiterhin nichts aus);
- kein `push`-Trigger (unveraendert).

Pfadliste, aus dem realen Repo abgeleitet (nur tatsaechlich gate-/
buildrelevante Pfade, keine Wildcard ueber `scripts/`):

```text
.github/workflows/build.yml
scripts/run_pre_ready_gates.sh
scripts/run_pre_ready_and_publish.sh
scripts/esp_idf_contract.py
scripts/run_esp_idf_static_analysis.py
scripts/build_esp_idf_profiles.py
scripts/check_build_profiles.py
scripts/check_ci_artifact_scan_coverage.py
scripts/check_architecture_boundaries.py
scripts/check_secrets.py
scripts/generate_board_profile_header.py
scripts/build_report.py
scripts/selftest_quality_gates.py
.clang-format
.clang-tidy
platformio.ini
CMakeLists.txt
sdkconfig.defaults
sdkconfig.defaults.bringup
sdkconfig.defaults.release
main/idf_component.yml
lib/device_platform_esp_idf/idf_component.yml
lib/fermentation_app/idf_component.yml
```

Gegenueber der Mindestliste des Auftrags kommen drei Eintraege hinzu, die der
Runner real aufruft und die damit zum Gate-Vertrag gehoeren:
`build_esp_idf_profiles.py`, `check_architecture_boundaries.py` und
`generate_board_profile_header.py`.
Bewusst **nicht** enthalten: Komponenten-`CMakeLists.txt` unter `lib/`/`main/`
(Quelllisten sind Featurearbeit und lokal durch die `esp`-Phase gedeckt),
`sdkconfig.defaults.issue*`/Testpartitionen/`spikes/` (nicht im
Produktions-Gatepfad), `partitions/issue_90_state_store.csv` (Produktions-
Partitionstabelle: **Entscheidung D2**, Empfehlung: nicht aufnehmen, da sie
Produktdaten-Layout und kein Gate-/Toolchainvertrag ist und die lokale
`esp`-Phase sie baut), `dependencies.lock` (gitignored).

Weitere Workflowanpassungen fuer `workflow_dispatch` (dabei ist
`github.event.pull_request` leer):

- Job-`if`: `github.event_name == 'workflow_dispatch' ||
  github.event.pull_request.draft == false`. Der explizite Zweig dient der
  Lesbarkeit; GitHub-Expressions vergleichen lose und wandeln `null` und
  `false` beide in `0`, der bisherige Guard wuerde Dispatch daher voraussichtlich
  ohnehin durchlassen. Das wird in C2 anhand der GitHub-Expression-Dokumentation
  belegt, nicht angenommen;
- `concurrency.group`: `firmware-ci-${{ github.event.pull_request.number ||
  github.ref }}` (sonst haetten alle Dispatch-Laeufe eine leere Nummer und
  wuerden sich gegenseitig abbrechen);
- `SOURCE_GIT_SHA`: `${{ github.event.pull_request.head.sha || github.sha }}`
  (Artefaktname und Manifest `git_sha`; ohne Fallback waere er leer).

Der Job selbst (Clean-Room, Provenienz, `host`, `esp`, Artifact-Scan-Coverage,
Artefakt-/Privacy-Scan, Uploads) bleibt unveraendert. Hinweise: (a) Ein
`workflow_dispatch` gegen einen Branch benoetigt die Workflowdatei auf dem
Default-Branch; fuer #184 selbst greift der `pull_request`-Trigger, weil die
Aenderung an `build.yml` den eigenen Pfadfilter trifft. (b) Da der schwere
Workflow nicht Required wird, entsteht nicht das bekannte Problem eines
wegen Pfadfilter nie gemeldeten Required Checks; `pre-ready/local` ist ein
Commit Status ohne Pfadfilter.

**Trigger-Vertragscheck ohne neues Skript und ohne Runner-Aenderung:**
`run_pre_ready_gates.sh` bleibt unveraendert. Der bestehende Sammler
`scripts/selftest_quality_gates.py` (laeuft in der `host`-Phase) erhaelt eine
weitere Teilpruefung, die die reale `build.yml` mit PyYAML (bereits gepinnte
Host-Abhaengigkeit, keine neue) liest und **ableitend statt dupliziert**
prueft:

- `workflow_dispatch` vorhanden, kein `push`-Trigger, kein `paths-ignore`;
- jeder Eintrag der Pfadliste existiert im Repo (faengt Tippfehler und
  Umbenennungen);
- jede im `run_pre_ready_gates.sh` per `scripts/<name>` aufgerufene Datei sowie
  Runner, Wrapper und Workflow selbst sind in der Pfadliste enthalten
  (SSOT = Runner, keine zweite Liste);
- Draft-Guard laesst `workflow_dispatch` zu; `SOURCE_GIT_SHA` und Concurrency
  haben Fallbacks.

Die Pruefung ist eine reine Funktion ueber (Workflow-Dict, Runner-Text,
Dateimenge); die Fixtures reichen bewusst fehlerhafte Eingaben durch
(Pfad fehlt, Runner-Skript nicht abgedeckt, `push`-Trigger, `paths-ignore`,
fehlender Dispatch-Guard) und erwarten jeweils FAILED. Falls der Owner
stattdessen ein eigenes Skript wuenscht, waere das eine Runner-Aenderung und
damit eine ausdrueckliche Ownerentscheidung (nicht empfohlen).
`check_ci_artifact_scan_coverage.py` bleibt unveraendert und muss gegen die
neue `build.yml` weiterhin bestehen (Regression im Selbsttest).

### 4.4 C3 – Governance-Dokumente und ROADMAP

Eine Policy, ein Ort: Details nur in `docs/CI_AND_QUALITY_GATES.md`; `AGENTS.md`
und `docs/AGENT_WORKFLOW.md` erhalten die neue Reihenfolge und einen
kompakten Verweis.

Neuer normaler Mergevertrag (ersetzt die bisherige Reihenfolge ab
`PRE_READY_LOCAL_GATES=PASS`):

```text
Independent Review abgeschlossen
-> OPEN_BLOCKERS=0
-> Owner autorisiert finalen lokalen Pre-Ready
-> PRE_READY_LOCAL_GATES=PASS auf exakt finalem HEAD
-> `pre-ready/local=success` auf exakt demselben HEAD
-> Owner setzt Ready for review
-> Required Status Check erfuellt
-> Merge-Gate
```

`GitHub Full CI PASS` ist kein allgemeines Pflichtgate mehr; es bleibt
Pflicht, wenn der schwere Workflow gemaess Pfadfilter fuer den PR ausgeloest
wurde oder der Owner ihn ausdruecklich anordnet.

Betroffen (konkret):

- `AGENTS.md`: Reihenfolgeblock und der Absatz „GitHub-Firmware-CI laeuft
  nicht im Draft …“ (nur die Reihenfolge und ein Verweis).
- `docs/AGENT_WORKFLOW.md`: Reihenfolgeblock in Abschnitt 8 und Abschnitt 10
  (CI-Absaetze) – Verweis statt Wiederholung.
- `docs/CI_AND_QUALITY_GATES.md`: Abschnitt „Vollstaendiger lokaler Lauf“
  (Wrapper als Ausfuehrungsform; Runner bleibt Gate-SSOT), Abschnitt
  „GitHub-CI“ (neue Trigger inkl. `workflow_dispatch`, Pfadfilter-Prinzip
  statt Liste – die Liste steht im Workflow), „CI-Pipeline“, Statusfelder, Zweck-Absatz
  (Wrapper-Verweis), Status-Vertrag `pre-ready/local` und
  Branch-Protection-Sollzustand; `GITHUB_CI=PASS` wird als nur bedingt
  erforderlich neu definiert.
- `.github/pull_request_template.md`: Zeile `GitHub-CI:` auf
  `pre-ready/local` plus „schwere CI nur bei Trigger/Anordnung“.
- `docs/ROADMAP.md`: C0 Startstatus; in C3 Endstatus.

Die Doku wird erst in C3 geaendert; bis dahin gilt weiter der alte Vertrag. Die
Roadmap erhaelt in C0 nur den Eintrag unter „Parallele Governance-Arbeit“.

## 5. Tests und Nachweise (gezielt, Draft-Phase)

- `bash -n scripts/run_pre_ready_and_publish.sh`;
- `python3 scripts/selftest_quality_gates.py` (alle Wrapper-Szenarien aus 4.2
  und Trigger-Vertrag aus 4.3 gegen die reale `build.yml`);
- `python3 scripts/check_ci_artifact_scan_coverage.py` (Regression);
- Workflow-Syntax: PyYAML-Parse und Trigger-Vertragscheck; `actionlint` wird
  nicht vorausgesetzt;
- `git diff --check`;
- Builder-Self-Check (`run_pre_ready_gates.sh self-check`) nach C1/C2 gemaess
  Workflow (Runner entscheidet `REQUIRED/NOT_REQUIRED`).

Der vollstaendige lokale Lauf erfolgt nur nach Independent Review,
`OPEN_BLOCKERS=0` und ausdruecklicher Owneranweisung; **#184 wird dafuer noch
nach dem bisherigen Verfahren qualifiziert** (Abschnitt 7).

## 6. Risiken und offene Entscheidungen

| ID | Frage / Risiko | Empfehlung |
|---|---|---|
| D1 | ESP-IDF-Umgebung im Wrapper (A: Subshell-`export.sh` fuer `esp`; B: Owner aktiviert vorab) | A |
| D2 | Produktions-Partitionstabelle (`partitions/issue_90_state_store.csv`) in den Pfadfilter? | nein |
| D3 | Folge der Required-Check-Umstellung: Mit `pre-ready/local` als Required Check, strict und `enforce_admins=true` benoetigt **jeder** PR vor dem Merge einen vollen host+esp-Lauf – auch Markdown-only-PRs, reine ROADMAP-Syncs und reine Plan-PRs (bisher bewusst von der Firmware-CI ausgenommen). Der Plan erfindet keine Abkuerzung: jede Variante, die `success` ohne host+esp setzt, braeche die Statusbedeutung „host+esp PASS“. Owner entscheidet, ob er diese Last akzeptiert oder fuer solche PRs eine eigene, separat zu planende Regel (z. B. anderer Kontext oder Admin-Bypass) wuenscht. | Owner-Entscheidung, nicht Teil dieses PR |
| R1 | `.codex/config.toml` ist auf Owner-Anweisung als eigener Commit im PR; der Arbeitsbaum ist damit fuer den Wrapper sauber. | erledigt |
| R2 | Status ist eine unsignierte Behauptung (siehe Abschnitt 1) | akzeptiert laut Auftrag |
| R3 | Branch-Upstream eines frisch angelegten Branches zeigt auf `origin/main`; der Wrapper verlangt `origin/<eigener Branch>` und bricht sonst ab | Push mit `-u origin <branch>` |
| R4 | `main` laeuft waehrend eines langen Laufs weiter -> bewusst kein SUCCESS; Nachziehen von `main` erzeugt neuen SHA und erneuten Lauf | gewollt (strict) |

## 7. Branch-Protection-Migration (Owner-/GitHub-Adminarbeit)

Gelesener Ist-Zustand siehe Abschnitt 2: es existiert **kein**
`required_status_checks`-Block; es ist daher nichts zu entfernen. Die
Einstellung wird vom Agenten nicht veraendert.

1. #184 wird nach dem bisherigen Verfahren qualifiziert (Independent Review,
   Owner-autorisierter lokaler Pre-Ready-Lauf nach heutigem Runner-Vertrag,
   `Ready for review`, schwere CI – sie wird durch die Aenderung an
   `build.yml` ohnehin ausgeloest).
2. Auf dem finalen PR-HEAD (nach Pre-Ready, vor Ready) einmal
   `bash scripts/run_pre_ready_and_publish.sh` erfolgreich ausfuehren, damit
   `pre-ready/local` als realer Kontext auf GitHub existiert (erst dann ist er
   in den Branch-Protection-Einstellungen auswaehlbar). Wird dadurch ein
   weiterer Commit noetig, gilt die Regel „Status gilt nur fuer genau diesen
   SHA“ und der Nachweis wird auf dem finalen HEAD wiederholt. Dieser **eine**
   Wrapperlauf (er ruft denselben Runner auf und gibt
   `PRE_READY_LOCAL_GATES=PASS` aus) ist zugleich der Owner-autorisierte
   Pre-Ready-Lauf nach dem alten Verfahren aus Schritt 1; host+esp laeuft fuer
   #184 nicht zweimal. Voraussetzung: Owner-Autorisierung des Laufs liegt vor
   und der Wrapper ist auf dem finalen HEAD vollstaendig bestanden.
3. **Owner-Handaktion (Gate):** fuer `main` Required Status Check
   `pre-ready/local` setzen und
   `Require branches to be up to date before merging` (strict) aktivieren;
   `enforce_admins` bleibt `true`. Die schwere CI wird **nicht** als Required
   Check eingetragen (nicht noetig und wegen Pfadfilter nicht zulaessig).
4. Erst danach gilt der neue Vertrag fuer Folge-PRs als aktiv; der Agent
   dokumentiert den Zustand erst nach Owner-Bestaetigung als `ACTIVE`.

Bis zu Schritt 3 bleibt `BRANCH_PROTECTION_MIGRATION=OWNER_ACTION_PENDING`
im PR sichtbar.

## 8. Dokumentationswirkung und Abschluss

Keine Anforderungen werden in die ROADMAP kopiert. Nach C3 haelt der Agent fuer
den Independent Review an. Ready, Merge, Issue-Abschluss und die
Branch-Protection-Umstellung bleiben Ownerhandlungen. Der PR bleibt Draft.

## 9. Abnahme dieses Plans

Freigabe erfolgt ueber die exakte Plan-SHA des Commits, der diese Datei
enthaelt (`PLAN_STATUS=AWAITING_OWNER_APPROVAL`, `IMPLEMENTATION=NOT_STARTED`),
einschliesslich der Entscheidungen D1, D2 und D3.
