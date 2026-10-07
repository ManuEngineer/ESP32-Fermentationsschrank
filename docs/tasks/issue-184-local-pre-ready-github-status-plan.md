# Plan – Issue #184: lokalen Pre-Ready-PASS als GitHub-Merge-Gate nutzen

Status: Planungsphase (`IMPLEMENTATION_BLOCKED_PENDING_PLAN_APPROVAL`)

Issue: `#184`
Pull Request: `#185` (Draft)
Basis-Branch: `main`
Basis-SHA: `dc938b2bc5cc9f9f2e93065ed5a9009eb99fcfda`
Auftrag: `Issue184_PreReady_GitHub_Gate_Auftrag.md` (Owner)
Planrevision: `2` (konsolidiert; Rev 1 `63c9291` nur historische Referenz)
REVIEWED_REV1_HEAD: `63c92914420d52d684bcd9dad360683301219a87`
Implementation: `NOT_STARTED`
PRODUCT_CODE_CHANGE: `NO`
ACTUATOR_RELEASE: `NO`

Dieser Plan ist ein eigenstaendiger CI-/Governance-Plan. Er aendert weder
Produktcode noch Hardware-, Safety- oder Aktorvertraege.

Ownerentscheidungen dieser Revision (aus dem Plan-Korrekturauftrag nach
Independent Review): `D1=OPTION_A`;
`D2=YES_INCLUDE_PRODUCTION_PARTITION_AND_DIRECT_BUILD_CONTRACT_FILES`;
`D3=MARKDOWN_ONLY_FULL_PRE_READY_NOT_REQUIRED`. Es sind keine
Ownerentscheidungen offen.

## 1. Ziel und Nicht-Ziele

Ziel (Owner-Vorgabe, hier nur konkretisiert):

```text
Independent Review / Fix Verification
-> OPEN_BLOCKERS=0
-> Owner autorisiert finalen lokalen Pre-Ready
-> lokaler Lauf auf exakt finalem, gepushtem PR-HEAD:
   host + esp PASS (semantischer Diff) bzw. NOT_REQUIRED_MARKDOWN_ONLY
-> Commit Status `pre-ready/local=success` auf genau diesem SHA
-> Owner setzt Ready for review
-> Required Status Check `pre-ready/local` + strict/up-to-date
-> Merge-Gate
```

Normale Feature-PRs fuehren die vollstaendigen Engineering-Gates nur einmal
aus (lokal). Die schwere GitHub-CI bleibt als Clean-Room-/Artefaktpfad fuer
Gate-/Build-/Toolchain-Aenderungen und manuell verfuegbar. Ein Status eines
aelteren SHA gibt einen spaeteren Push nie frei.

Semantik des Contexts: `pre-ready/local=success` bedeutet „lokales
Pre-Ready-/Merge-Gate erfuellt“, **nicht** zwingend „host+esp wurde
ausgefuehrt“. Bei einem Diff mit mindestens einer Nicht-Markdown-Datei heisst
SUCCESS `host+esp PASS` auf exakt dem gepushten HEAD; bei einem rein
Markdown-only Diff heisst es `MARKDOWN_ONLY_NOT_REQUIRED` (Abschnitt 4.1). Die
Statusbeschreibung weist den Grund jeweils sichtbar aus. Es bleibt bei diesem
einen Context; keine zweite Statusfamilie.

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
  eigener Commit Teil dieses PR (Owner-Ausnahme, sichtbar im PR-Body); sie ist
  nicht Teil des fachlichen Scopes von Issue #184 und hat keine
  Gate-/Produktwirkung.

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
9. **Diff-Klassifikation (automatisch, kein CLI-Flag, kein Umgebungsschalter):**
   `git diff --name-only --no-renames "$MAIN_BEFORE" "$TESTED_HEAD"`
   (`--no-renames`, damit bei Umbenennungen Alt- und Neupfad beide erscheinen).
   `MARKDOWN_ONLY` gilt nur, wenn der Diff nicht leer ist und **jeder**
   gelistete Pfad auf `.md` endet (dieselbe Abgrenzung wie bisher
   `paths-ignore: **/*.md`). Jeder andere Fall – Mischdiff, leerer Diff,
   Klassifikationsfehler – ist `FULL` (fail-closed zur strengeren Seite).
10. Status `pending` auf `TESTED_HEAD` publizieren; schlaegt das fehl: Abbruch
    (ohne Gatelauf – ohne publizierbaren Status ist der Lauf wertlos).
11. Nur bei `FULL`: Voraussetzungen der ESP-Phase pruefen (D1) und
    `export PRE_READY_EXPECTED_HEAD=$TESTED_HEAD`; dann
    `bash scripts/run_pre_ready_gates.sh host`, danach `esp`. Bei
    `MARKDOWN_ONLY` wird der Runner nicht aufgerufen und keine ESP-Umgebung
    verlangt.
12. Abschlusspruefung vor SUCCESS (fuer beide Klassen): erneut
    `git fetch origin main` und `git fetch origin <branch>`; Arbeitsbaum
    sauber; `HEAD == TESTED_HEAD`; `origin/<branch> == TESTED_HEAD`;
    `origin/main == MAIN_BEFORE` **und** Ancestor von `HEAD`; die Klassifikation
    wird auf demselben Diff wiederholt und muss identisch ausfallen. Ist `main`
    weitergelaufen: kein SUCCESS.
13. Nur wenn bei `FULL` beide Phasen mit PASS beendet haben (bei
    `MARKDOWN_ONLY` entfaellt dies) und Schritt 12 vollstaendig besteht:
    Status `success` auf `TESTED_HEAD`.

Fehlerpfad: Ein `EXIT`-Trap publiziert bei jedem Abbruch nach Schritt 9, bei
dem noch kein `success` gesetzt wurde, best-effort `failure` auf
`TESTED_HEAD` (Gatefehler, Abschlusspruefung, Signal).
Ist GitHub nicht erreichbar, bleibt der zuvor gesetzte `pending` stehen; das
ist fail-closed, da `pending` keinen Required Check erfuellt. Der Wrapper
beendet in jedem Nicht-Erfolgsfall mit Exit != 0 und gibt maschinenlesbar
`PRE_READY_LOCAL_GATES=PASS|NOT_REQUIRED_MARKDOWN_ONLY|FAILED|BLOCKED` und
`PRE_READY_TESTED_HEAD=<sha>` aus (`BLOCKED` = fehlende Voraussetzung vor dem
Gatelauf, `FAILED` = ausgefuehrt und fehlgeschlagen oder Abschlusspruefung
verletzt). Der Wert `PASS` wird ausschliesslich nach ausgefuehrtem host+esp
ausgegeben; `NOT_REQUIRED_MARKDOWN_ONLY` ist nie `PASS`.

Status-Vertrag: Context genau `pre-ready/local`; Beschreibung
`pre-ready/local pending <short-sha>`, `<short-sha> host+esp PASS`,
`<short-sha> MARKDOWN_ONLY_NOT_REQUIRED` bzw.
`<short-sha> FAILED|BLOCKED: <Grund kurz>` (<= 140 Zeichen); kein
`target_url`; Aufruf ausschliesslich ueber
`gh api -X POST repos/{owner}/{repo}/statuses/<sha> -f state=... -f
context=... -f description=...`. Keine Tokens/Secrets im Repo oder in
Ausgaben. Es gibt nur diesen einen aggregierten Kontext.

Repo-Identitaet: `{owner}/{repo}` wird von `gh` aus dem Arbeitsverzeichnis
aufgeloest; der Wrapper vergleicht `gh repo view --json nameWithOwner` mit der
Origin-URL und bricht bei Abweichung ab, damit der Status nicht auf einem
Fork/Upstream landet.

ESP-IDF-Umgebung (`D1=OPTION_A`, Ownerentscheidung): Das Dokument beschreibt
`host` ohne und `esp` mit aktiviertem `export.sh` (wie in GitHub-CI). Der
Wrapper verlangt bei `FULL` gesetztes `IDF_PATH`/`IDF_TOOLS_PATH` (sonst
`BLOCKED`, vor dem Gatelauf), fuehrt `host` in der aufrufenden Umgebung aus und
aktiviert fuer die `esp`-Phase `. "$IDF_PATH/export.sh"` in einer Subshell –
exakt die bereits dokumentierte Sequenz; keine Installation, keine zweite
Toolchain-Wahrheit, keine neue Pfadlogik. Der Runner prueft die Provenienz
danach unveraendert.

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
| Diff mit Nicht-Markdown-Datei, host+esp PASS, alles stabil | Exit 0; Reihenfolge `pending`, dann genau ein `success` auf `TESTED_HEAD` mit Kurz-SHA und `host+esp PASS`; Runner `host` und `esp` aufgerufen |
| rein Markdown-only Diff | Exit 0; `pending`, dann `success` mit `MARKDOWN_ONLY_NOT_REQUIRED`; Runner **nicht** aufgerufen; keine ESP-Umgebung noetig |
| Mischdiff (`.md` + irgendeine Nicht-Markdown-Datei, z. B. `.sh`, `.cpp`, `.yml`, `.toml`) | **kein** Markdown-Bypass: Runner `host`+`esp` laufen, Beschreibung `host+esp PASS` |
| Umbenennung `.md` <-> Nicht-Markdown, Loeschung einer Nicht-Markdown-Datei | kein Bypass (`--no-renames`) |
| leerer Diff (HEAD == `origin/main`) | `FULL`, kein Bypass |
| Markdown-only, aber Arbeitsbaum/Upstream/SHA/`main` verletzt | gleiche Abbrueche wie bei `FULL`; kein `success` |
| Diff aendert sich bis zur Abschlusspruefung (z. B. HEAD/`main` bewegt) | kein `success` |
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
| Status-Kontext/State-Werte | nur `pre-ready/local`; nur `pending|success|failure`; Beschreibung nennt je Klasse `host+esp PASS` bzw. `MARKDOWN_ONLY_NOT_REQUIRED` |

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
main/Kconfig.projbuild
main/CMakeLists.txt
lib/device_platform/CMakeLists.txt
lib/device_platform_esp_idf/CMakeLists.txt
lib/fermentation_app/CMakeLists.txt
lib/device_platform_esp_idf/idf_component.yml
lib/fermentation_app/idf_component.yml
partitions/issue_90_state_store.csv
```

Gegenueber der Mindestliste des Auftrags kommen drei Eintraege hinzu, die der
Runner real aufruft und die damit zum Gate-Vertrag gehoeren:
`build_esp_idf_profiles.py`, `check_architecture_boundaries.py` und
`generate_board_profile_header.py`.
Die Eintraege `main/Kconfig.projbuild`, die vier Produktions-`CMakeLists.txt`
unter `main/` und `lib/` sowie `partitions/issue_90_state_store.csv` sind
direkte Produktions-Buildinputs (`D2`). Die Partitionstabelle ist es ueber
`sdkconfig.defaults` (`CONFIG_PARTITION_TABLE_CUSTOM_FILENAME`). Bewusst
**nicht** enthalten: test-/spike-/harnessspezifische CMake-, sdkconfig- und
Partitionsdateien (`sdkconfig.defaults.issue*`,
`partitions/issue_90_state_store_test.csv`, `spikes/`, `test/esp_idf_*`), soweit
der kanonische normale Heavy-CI-Pfad sie nicht verwendet; vor C2 wird anhand
von `build_esp_idf_profiles.py` und `esp_idf_contract.py` belegt, dass dies fuer
jede ausgeschlossene Datei zutrifft, andernfalls wird sie aufgenommen.
`dependencies.lock` ist gitignored.

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
- die Produktions-Partitionstabelle, auf die `sdkconfig.defaults` per
  `CONFIG_PARTITION_TABLE_CUSTOM_FILENAME` zeigt, ist abgeleitet in der
  Pfadliste enthalten;
- die expliziten Buildvertragsdateien (`main/Kconfig.projbuild`,
  `main/CMakeLists.txt`, `lib/device_platform/CMakeLists.txt`,
  `lib/device_platform_esp_idf/CMakeLists.txt`,
  `lib/fermentation_app/CMakeLists.txt`, `CMakeLists.txt`,
  `sdkconfig.defaults*` der Produktionsprofile) stehen als bewusst kleine
  feste Schutzliste im Check; sie ist absichtlich redundant zum Workflow,
  damit niemand eine dieser Dateien unbemerkt aus der Positivliste entfernen
  kann (Entfernen aus dem Workflow ohne Aenderung der Schutzliste = FAILED);
- Draft-Guard laesst `workflow_dispatch` zu; `SOURCE_GIT_SHA` und Concurrency
  haben Fallbacks.

Die Pruefung ist eine reine Funktion ueber (Workflow-Dict, Runner-Text,
Dateimenge); die Fixtures reichen bewusst fehlerhafte Eingaben durch
(Pfad fehlt, Runner-Skript nicht abgedeckt, Partitionstabelle oder
Schutzlistendatei entfernt, `push`-Trigger, `paths-ignore`,
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
-> lokales Pre-Ready-Gate erfuellt auf exakt finalem HEAD
   (host+esp PASS, bzw. NOT_REQUIRED_MARKDOWN_ONLY bei rein Markdown-only Diff)
-> `pre-ready/local=success` auf exakt demselben HEAD
-> Owner setzt Ready for review
-> Required Status Check erfuellt
-> Merge-Gate
```

`PRE_READY_LOCAL_GATES=PASS` bleibt ausschliesslich dem ausgefuehrten host+esp
vorbehalten; die Markdown-only-Klasse wird als
`NOT_REQUIRED_MARKDOWN_ONLY` gefuehrt und nie als `PASS` bezeichnet. Der
Context `pre-ready/local` ist als „lokales Pre-Ready-/Merge-Gate erfuellt“
definiert.

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
| D1 | ESP-IDF-Umgebung im Wrapper | entschieden: Option A |
| D2 | Produktions-Partitionstabelle und direkte Buildvertragsdateien im Pfadfilter | entschieden: ja |
| D3 | Markdown-only-PRs | entschieden: voller Pre-Ready `NOT_REQUIRED`, automatische Diff-Klassifikation, Mischdiff -> voller Lauf |
| R5 | Markdown-Klassifikation ist eine Lockerung des Gates: ein `.md`-Diff mit semantischer Wirkung (z. B. von Skripten gelesene Markdown-Dateien) wird nicht lokal gebaut. Gleiche Abgrenzung wie bisher (`paths-ignore: **/*.md`); keine neue Lockerung gegenueber dem Ist-Stand. | akzeptiert (Ownerentscheidung D3) |
| R6 | Head-Status vs. Test-Merge-Commit-Semantik von GitHub (Abschnitt 7, Aktivierungsgate) | empirisch absichern |
| R1 | `.codex/config.toml` ist auf Owner-Anweisung als eigener Commit im PR; der Arbeitsbaum ist damit fuer den Wrapper sauber. | erledigt |
| R2 | Status ist eine unsignierte Behauptung (siehe Abschnitt 1) | akzeptiert laut Auftrag |
| R3 | Branch-Upstream eines frisch angelegten Branches zeigt auf `origin/main`; der Wrapper verlangt `origin/<eigener Branch>` und bricht sonst ab | Push mit `-u origin <branch>` |
| R4 | `main` laeuft waehrend eines langen Laufs weiter -> bewusst kein SUCCESS; Nachziehen von `main` erzeugt neuen SHA und erneuten Lauf | gewollt (strict) |

## 7. Branch-Protection-Migration (Owner-/GitHub-Adminarbeit)

Gelesener Ist-Zustand siehe Abschnitt 2: es existiert **kein**
`required_status_checks`-Block; es ist daher nichts zu entfernen. Die
Einstellung wird vom Agenten nicht veraendert.

Risiko B3: Der Wrapper setzt `pre-ready/local` auf den PR-**Head**. Ein
`pull_request`-Workflow erzeugt Checks dagegen auf dem ephemeren
Test-Merge-Commit; GitHub dokumentiert, dass fuer Required Checks der
Test-Merge-Commit massgebend sein kann, wenn dafuer Statuschecks vorhanden
sind. Fuer PRs mit Heavy-CI darf daher nicht vorausgesetzt werden, dass der
Head-Status das Required Gate automatisch erfuellt. Es wird **keine**
Mirror-/Bridge-Architektur vorsorglich gebaut; stattdessen gilt ein
empirisches Aktivierungsgate:

1. #185 wird nach dem bisherigen Verfahren qualifiziert (Independent Review,
   `OPEN_BLOCKERS=0`, Owner-autorisierter lokaler Pre-Ready-Lauf).
2. Auf dem finalen #185-Head einmal `bash scripts/run_pre_ready_and_publish.sh`
   erfolgreich ausfuehren, damit `pre-ready/local` als realer Kontext
   existiert (erst dann in den Branch-Protection-Einstellungen auswaehlbar).
   Dieser **eine** Wrapperlauf (derselbe Runner, `PRE_READY_LOCAL_GATES=PASS`)
   ist zugleich der Owner-autorisierte Pre-Ready-Lauf aus Schritt 1; host+esp
   laeuft fuer #185 nicht zweimal. Jeder weitere Commit macht den Nachweis auf
   dem neuen finalen HEAD erneut noetig.
3. Die durch diesen PR ohnehin ausgeloeste Heavy-CI (Aenderung an `build.yml`
   und Gate-Skripten trifft den eigenen Pfadfilter) erfolgreich abschliessen
   (nach `Ready for review` durch den Owner).
4. **Owner-Handaktion:** fuer `main` Required Status Check `pre-ready/local`
   setzen und `Require branches to be up to date before merging` (strict)
   aktivieren; `enforce_admins` bleibt `true`. Die schwere CI wird nicht als
   Required Check eingetragen.
5. **Vor dem Merge von #185** in der GitHub-Mergebox verifizieren, dass der
   Required Context als erfuellt gilt, obwohl Heavy-CI-Checks auf dem
   Test-Merge-Commit existieren.
6. Ergebnis dokumentieren: `REQUIRED_STATUS_HEAD_MERGE_COMMIT_INTEROP=PASS`.
7. Bleibt der Required Context wegen der Merge-Commit-Semantik offen: die
   Branch-Protection-Aenderung zuruecknehmen bzw. den neuen Required Check
   deaktivieren, `MIGRATION=BLOCKED`, **nicht mergen** und eine Planrevision
   fuer eine minimale Bridge-/Mirror-Loesung vorlegen.
8. Erst nach `INTEROP=PASS` und Owner-Bestaetigung gilt der neue Vertrag fuer
   Folge-PRs als aktiv (`ACTIVE`).

Bis Schritt 4 bleibt `BRANCH_PROTECTION_MIGRATION=OWNER_ACTION_PENDING`, danach
bis Schritt 6 `INTEROP_VERIFICATION_PENDING` im PR sichtbar. Der Agent dokumentiert
Ergebnisse erst nach Owner-Bestaetigung.

## 8. Dokumentationswirkung und Abschluss

Keine Anforderungen werden in die ROADMAP kopiert. Nach C3 haelt der Agent fuer
den Independent Review an. Ready, Merge, Issue-Abschluss und die
Branch-Protection-Umstellung bleiben Ownerhandlungen. Der PR bleibt Draft.

## 9. Abnahme dieses Plans

Freigabe erfolgt ueber die exakte Plan-SHA des Commits, der diese Datei
enthaelt (`PLAN_STATUS=AWAITING_OWNER_APPROVAL`, `IMPLEMENTATION=NOT_STARTED`),
Rev 2 enthaelt die Ownerentscheidungen D1 bis D3 und die Korrekturen B1 bis B3
aus dem Independent Review.
