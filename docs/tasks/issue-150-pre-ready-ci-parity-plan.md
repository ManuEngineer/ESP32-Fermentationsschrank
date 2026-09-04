# Plan – Issue #150: Pre-Ready-CI-Parity-Gate

Status: Planungsphase (`IMPLEMENTATION_BLOCKED_PENDING_PLAN_APPROVAL`)

Issue: `#150`
Pull Request: `#151` (Draft)
Basis-Branch: `main`
Basis-SHA: `e84dfa8abf220220a33e6e21b95dbd0d7bd9ac90`
Roadmap-Sync: `7e8aea9d5a7958cb6e7b84e269d9ab89ef8c62cb`
Implementation: `NOT_STARTED`
ACTUATOR_RELEASE: `NO`

Dieser Plan ist ein eigenstaendiger Governance-/Build-Tooling-Plan. Er ist
nicht Teil von PR #147 / Issue #144 und aendert weder deren Code noch deren
Review- oder Ownerstatus.

## 1. Ziel und Anlass

Vor `Ready for review` muss der Owner auf dem exakt finalen PR-HEAD einen
autorisierten lokalen Lauf aller lokal reproduzierbaren CI-Pflicht-Gates
verifizieren koennen. Der Lauf muss insbesondere denselben
`clang-format`- und `clang-tidy`-Pfad verwenden wie GitHub-CI, damit ein
format- oder analysebedingter CI-Fehler nicht erst nach dem Ownerwechsel in
einer seriellen CI-Schleife entdeckt wird.

Der bestehende Firmwarejob besitzt die benoetigten Einzelpruefungen bereits,
aber ihre Orchestrierung ist zwischen `.github/workflows/build.yml` und der
Dokumentation verteilt. Der Plan fuehrt deshalb eine kleine gemeinsame,
versionierte Ausfuehrung ein. Bestehende fachliche Gate-Skripte bleiben deren
jeweilige Owner; es entsteht kein neues CI-Framework.

## 2. Verbindliche Status- und Owner-Reihenfolge

Die folgenden Zustandsuebergaenge sind normativ:

```text
Independent Review abgeschlossen
-> INDEPENDENT_REVIEW=PASS und OPEN_BLOCKERS=0
-> Owner autorisiert finalen lokalen Pre-Ready-Lauf
-> PRE_READY_LOCAL_GATES=PASS auf exakt finalem HEAD
-> Owner setzt Ready for review
-> GITHUB_CI=PASS
-> Merge-Gate
```

`OPEN_BLOCKERS=0` ist nur das Ergebnis des Independent Reviews. Es ist keine
Erlaubnis fuer den vollstaendigen lokalen Lauf und keine automatische
Berechtigung, `Ready for review` zu setzen. Der lokale Lauf benoetigt eine
separate ausdrueckliche Owner-Autorisierung.

Die Statusbegriffe bleiben getrennt:

- `INDEPENDENT_REVIEW=PASS` / `OPEN_BLOCKERS=0`: fachlicher und technischer
  Review ist ohne offene Blocker abgeschlossen;
- `PRE_READY_LOCAL_GATES=PASS`: alle im lokalen Pre-Ready-Lauf geforderten
  reproduzierbaren Gates wurden auf dem finalen HEAD ausgefuehrt und bestanden;
- `GITHUB_CI=PASS`: der durch den Owner ausgeloeste kanonische GitHub-CI-Lauf
  ist bestanden.

`NOT_RUN`, `SKIPPED` oder `BLOCKED` ist kein `PASS`. Ein fehlendes Werkzeug,
eine fehlende ESP-IDF-Umgebung oder ein nicht ausgefuehrtes Teilgate bleibt
sichtbar und verhindert `PRE_READY_LOCAL_GATES=PASS`.

## 3. Verifizierte Ausgangslage und Quellen

Die Planung basiert auf `main@e84dfa8abf220220a33e6e21b95dbd0d7bd9ac90`.
Der aktuelle Workflow `.github/workflows/build.yml` enthaelt bereits diese
kanonischen Schritte:

- `clang-format` ueber den vollstaendigen C/C++-Baum;
- nativer Build ueber `scripts/build_report.py` und nativer Testlauf;
- native Compile-Datenbank und die feste clang-tidy-Dateiliste;
- Architekturguard, Secretpruefung und Quality-Gate-Selbsttests;
- ESP-IDF-6.0.2-Installation und Herkunftspruefung, Bring-up-/Release-Build,
  Ressourcenbericht und esp-clang-Static-Analysis;
- Artefakt-Scanabdeckungspruefung und Secretpruefung der erzeugten
  Textartefakte.

Die bestehenden Fach- und Gate-Owner bleiben:

- `scripts/build_report.py` bleibt Owner fuer nativen Build und
  Ressourcenbericht sowie die offizielle ESP-IDF-Groessen-/Manifestauswertung;
- `scripts/build_esp_idf_profiles.py` bleibt Owner fuer Profilbuild und
  Profilvalidierung;
- `scripts/run_esp_idf_static_analysis.py` bleibt Owner fuer den
  esp-clang-/ESP-IDF-Analysepfad;
- `scripts/check_architecture_boundaries.py`, `check_secrets.py`,
  `selftest_quality_gates.py` und `check_ci_artifact_scan_coverage.py` bleiben
  Owner ihrer vorhandenen Pruefvertraege;
- `.clang-tidy` bleibt die alleinige clang-tidy-Konfiguration.

Der gemeldete CI-Verlauf von PR #147 (`clang-format`-Fehler in Run #1032,
anschliessender `clang-tidy`-Fehler in Run #1033) ist Anlass und Regression-
Motivation, aber kein neuer Firmware- oder Architektur-Scope.

## 4. KISS-Loesung gegen Gate-Drift

### 4.1 Gemeinsamer Runner

Eine neue, schmale Datei
`scripts/run_pre_ready_gates.sh` wird die kanonische Reihenfolge der lokal
reproduzierbaren Gates enthalten. Sie ist ein fail-fast Shell-Orchestrator mit
`set -euo pipefail`, keine neue Abstraktions- oder Testplattform.

Sowohl die lokale Dokumentation als auch `.github/workflows/build.yml`
starten nach Bereitstellung ihrer jeweiligen Toolchain mit exakt:

```bash
bash scripts/run_pre_ready_gates.sh
```

Die Gate-Befehle und die clang-tidy-Dateiliste stehen nur in diesem Runner.
Die Dokumentation beschreibt Phasen, Voraussetzungen und Statusbegriffe; der
Workflow ruft den Runner auf und enthaelt keine zweite Gate-Befehlsliste.

Die weiterhin notwendigen Workflow-Schritte fuer Checkout, Python/PlatformIO-,
clang- und ESP-IDF-Installation sowie die nachgelagerten
`actions/upload-artifact`-Schritte bleiben CI-Umgebung beziehungsweise
Artefakttransport. Sie werden nicht als abweichende Gateausfuehrung kopiert.

### 4.2 Exakte Gate-Reihenfolge im Runner

Der Runner fuehrt in dieser Reihenfolge aus:

1. voller Formatcheck mit dem im PATH bereitgestellten
   `clang-format-18`-Alias:

   ```bash
   clang-format --dry-run --Werror \
     $(find src include lib test main -type f \( -name "*.cpp" -o -name "*.hpp" -o -name "*.h" \))
   ```

2. nativer Build und nativer Ressourcenbericht ueber den bestehenden Aufruf:

   ```bash
   python scripts/build_report.py --output build-report.md
   ```

   Der stdout/stderr-Strom wird zusaetzlich in `platformio-build.log`
   geschrieben, damit der bestehende CI-Fehlerlog-Upload erhalten bleibt.

3. vollstaendige native Tests:

   ```bash
   pio test -e native
   ```

4. native Compile-Datenbank und der unveraenderte kanonische clang-tidy-18-
   Lauf. Die bisher in `build.yml` gepflegte Produktionsdateiliste wandert
   ohne inhaltliche Aenderung in den gemeinsamen Runner. `.clang-tidy` bleibt
   die Checkkonfiguration.

5. bestehende Architektur-, Secret- und Quality-Gate-Pruefungen:

   ```bash
   python scripts/check_architecture_boundaries.py
   python scripts/check_secrets.py
   python scripts/selftest_quality_gates.py
   ```

6. beide ESP-IDF-Profile ueber den vorhandenen Buildtreiber:

   ```bash
   python scripts/build_esp_idf_profiles.py all
   ```

7. finalen kombinierten Ressourcenbericht ueber die vorhandene
   `build_report.py --append`-Schnittstelle erzeugen. In GitHub-CI wird
   `SOURCE_GIT_SHA` weiter als reviewbarer PR-HEAD uebergeben; lokal wird bei
   fehlendem Wert die bestehende Defaultsemantik des Berichts verwendet.

8. den bestehenden ESP-IDF-Static-Analysis-Pfad vorbereiten und ausfuehren:

   ```bash
   python "$IDF_PATH/tools/idf_tools.py" install esp-clang
   python scripts/run_esp_idf_static_analysis.py all
   ```

   Der Runner setzt `IDF_PATH`, `idf.py`, die exakte ESP-IDF-Herkunft und den
   exportierten Toolchain-Kontext voraus; deren Installation und
   Herkunftsverifikation bleibt der bestehende Umgebungsscope des Workflows.
   Fehlt eine Voraussetzung, bricht der Runner fail-closed ab.

9. die bestehende CI-Artefakt-Scanabdeckung pruefen und danach dieselbe
   Secretpruefung fuer alle durch die CI hochgeladenen Textartefakte ausfuehren:

   ```bash
   python scripts/check_ci_artifact_scan_coverage.py
   python scripts/check_secrets.py --scan-path ...
   ```

   Die konkreten Artefaktpfade werden einmal im Runner in derselben Menge wie
   die bestehenden erfolgreichen CI-Uploads gepflegt. Binaerdateien bleiben
   entsprechend dem bestehenden Vertrag vom Textscan ausgenommen.

### 4.3 Artefakt-Scanabdeckung ohne zweite Pfadwahrheit

`check_ci_artifact_scan_coverage.py` wird minimal erweitert: Neben dem
Workflow liest es die `--scan-path`-Deklarationen des gemeinsamen Runners.
Damit vergleicht es weiterhin jeden erfolgreichen Textartefakt-Upload aus
`build.yml` gegen die kanonische Scanmenge, obwohl die Secret-Befehle nicht
mehr im Workflow dupliziert werden.

Die Uploadpfade muessen fuer die GitHub-Actions-Uploadaktionen weiterhin im
Workflow stehen; das ist Transportkonfiguration, keine zweite Gatebefehls-
oder Scanliste. Die Coverage-Pruefung bleibt der bestehende Owner dieser
Beziehung.

## 5. Dokumentations- und Workflowaenderungen

### 5.1 `AGENTS.md`

Die Abschnitte zu Owner-Gates, Tests/CI und dem vollstaendigen lokalen Lauf
werden auf dieselbe normative Sequenz aus Abschnitt 2 gebracht. Insbesondere
wird klargestellt:

- `OPEN_BLOCKERS=0` erlaubt keinen automatischen Ready-Wechsel;
- der Owner autorisiert den finalen lokalen Pre-Ready-Lauf separat;
- erst `PRE_READY_LOCAL_GATES=PASS` erlaubt dem Owner die Ready-Aktion;
- danach sind `GITHUB_CI=PASS` und das bestehende Merge-Gate getrennte
  Nachweise.

### 5.2 `docs/AGENT_WORKFLOW.md`

Die Convergence-, Test-/CI- und Ownerabschlussabschnitte erhalten dieselbe
Reihenfolge. Der Plan bewahrt die Rollen- und Rechteverteilung: Der Agent
setzt weder Ready noch Merge; der Owner autorisiert den Lauf und setzt Ready.
Die bestehende Fix-Verification-/Full-Review-Regel bleibt unveraendert.

### 5.3 `docs/CI_AND_QUALITY_GATES.md`

Der Abschnitt zum vollstaendigen lokalen Lauf wird als vollstaendiger
lokaler Pre-Ready-Lauf praezisiert. Er nennt:

- die vier Voraussetzungen `FULL_REVIEW_COMPLETE`, `OPEN_BLOCKERS=0`,
  Owner-Autorisierung und finalen HEAD;
- den einzigen lokalen Einstieg `bash scripts/run_pre_ready_gates.sh`;
- alle neun Gatephasen aus Abschnitt 4.2, einschliesslich Format, clang-tidy,
  ESP-IDF-Static-Analysis und Artefakt-Scanabdeckung;
- die getrennten Ergebnisfelder `INDEPENDENT_REVIEW`,
  `PRE_READY_LOCAL_GATES` und `GITHUB_CI`.

Die Datei fuehrt keine zweite Liste der konkreten CI-Befehle. Sie verweist fuer
die ausfuehrbare Befehlsquelle auf den versionierten Runner und dokumentiert
nicht ausgefuehrte Teile weiterhin als `NOT_RUN`/`BLOCKED`.

### 5.4 `.github/workflows/build.yml`

Die einzelnen Gate-Run-Schritte werden durch einen gemeinsamen
`bash scripts/run_pre_ready_gates.sh`-Schritt ersetzt. Die vorhandene
Toolchain-/ESP-IDF-Umgebungsinstallation und die erfolgreichen bzw.
fehlerbedingten Artefakt-Uploads bleiben erhalten. Der Workflowjob bleibt
weiter an `draft == false` gebunden; es wird kein neuer Trigger und keine
Owneraktion automatisiert.

## 6. Geplanter Produktions- und Testscope

### Produktionsdateien

- `scripts/run_pre_ready_gates.sh` – neue, schmale gemeinsame Gateausfuehrung;
- `scripts/check_ci_artifact_scan_coverage.py` – liest die gemeinsame
  Scanquelle zusaetzlich zum Upload-Workflow;
- `.github/workflows/build.yml` – ruft den Runner auf und entfernt die
  duplizierten Gatebefehle;
- `AGENTS.md`;
- `docs/AGENT_WORKFLOW.md`;
- `docs/CI_AND_QUALITY_GATES.md`.

### Dokumentations-/Statusdateien

- `docs/ROADMAP.md` – bereits im ersten PR-Commit um den parallelen
  Issue-#150-Status ergaenzt (`7e8aea9...`); fachliche Firmwarereihenfolge
  und Issue-#147-Inhalt werden nicht kopiert.
- `docs/tasks/issue-150-pre-ready-ci-parity-plan.md` – dieses Dokument.

Es werden keine Firmware-, Safety-, Hardware-, ADR-, Fach- oder
Persistenzdateien veraendert. Es werden keine neuen Bibliotheken, Runner-
Frameworks, Registries, Dispatcher oder Buildprofile eingefuehrt.

## 7. Umsetzungsschnitte nach Planfreigabe

1. Den gemeinsamen Runner mit der exakt aus dem aktuellen Workflow
   uebernommenen Gatefolge erstellen. Die bestehende Artefakt-Scanpruefung
   minimal auf die neue gemeinsame Scanquelle erweitern.
2. `.github/workflows/build.yml` auf den Runner umstellen; Toolchain-Setup,
   ESP-IDF-Checkout/-Verifikation und Artefaktuploads erhalten.
3. `AGENTS.md`, `docs/AGENT_WORKFLOW.md` und
   `docs/CI_AND_QUALITY_GATES.md` auf Reihenfolge, Einstieg und Statusbegriffe
   synchronisieren.
4. Mit dem Builder-Self-Check den Scope gegen diesen Plan und gegen die
   bestehenden Gate-Skripte pruefen. Erst nach Independent Review,
   `OPEN_BLOCKERS=0` und ausdruecklicher Owner-Autorisierung darf der
   vollstaendige Runner auf dem finalen HEAD ausgefuehrt werden.

Der Implementation-PR bleibt bis zur Ownerfreigabe des exakten Plan-Commits
bei `IMPLEMENTATION=NOT_STARTED`. Die Implementation darf nicht in PR #147
oder dessen Branch erfolgen.

## 8. Gezielte Nachweise und Akzeptanzkriterien

### Vor dem Pre-Ready-Lauf

- `bash -n scripts/run_pre_ready_gates.sh` ist erfolgreich;
- `python scripts/check_ci_artifact_scan_coverage.py --selftest` ist
  erfolgreich und deckt sowohl Workflow-Uploads als auch Scanpfade aus dem
  Runner ab;
- `python scripts/check_ci_artifact_scan_coverage.py` meldet keine reale
  Upload-/Scanluecke;
- `python scripts/selftest_quality_gates.py` ist erfolgreich;
- ein statischer Workflow-/Scope-Check bestaetigt, dass die Gatebefehle nicht
  neben dem gemeinsamen Runner in `build.yml` dupliziert sind;
- `git diff --check` und die Dokumentationspruefung sind erfolgreich.

### Vollstaendiger, Owner-autorisierter Lauf

Auf exakt demselben finalen HEAD muss der gemeinsame Runner erfolgreich
durchlaufen und mindestens folgende beobachtbare Nachweise liefern:

- der volle `clang-format-18`-Check erkennt absichtlich einen lokalen
  Formatverstoss, wenn er in einer temporären Fixture vorliegt, und meldet
  den sauberen Baum als `PASS`;
- die native Build-/Ressourcen- und komplette Testphase besteht;
- Compile-Datenbank und exakt dieselbe clang-tidy-18-Dateiliste wie in CI
  bestehen;
- Architekturguard, Secretcheck und Quality-Gate-Selbsttests bestehen;
- beide ESP-IDF-Profile sowie esp-clang-Static-Analysis bestehen;
- alle erfolgreichen Textartefakt-Uploads sind durch die gemeinsame
  Scanmenge abgedeckt und der Artefakt-Secretcheck besteht;
- kein Teilgate wird aus Statusgruenden uebersprungen oder als `PASS`
  protokolliert, wenn es `NOT_RUN`, `SKIPPED` oder `BLOCKED` ist.

### CI-Paritaet

- `.github/workflows/build.yml` ruft fuer die Gatephase denselben
  versionierten Runner auf wie die lokale Dokumentation;
- die alte Fehlerklasse des Auftrags – `clang-format` oder `clang-tidy` erst
  in CI zu entdecken – ist bei korrekter Ausfuehrung des lokalen Runners
  reproduzierbar vor Ready sichtbar;
- die nachgelagerten GitHub-CI-Uploads bleiben funktionsfaehig;
- `GITHUB_CI=PASS` wird erst nach dem tatsaechlichen Nicht-Draft-CI-Lauf
  dokumentiert und nicht aus dem lokalen Ergebnis abgeleitet.

## 9. Test- und Ausfuehrungsgrenzen

In der Planungsphase werden keine Builds und keine vollstaendigen Testlaeufe
ausgefuehrt. Nach Planfreigabe sind im Draft nur die gezielten Syntax-,
Coverage-, Selftest- und Dokumentationspruefungen zulaessig. Der
vollstaendige `run_pre_ready_gates.sh`-Lauf bleibt bis zum abgeschlossenen
Independent Review mit `OPEN_BLOCKERS=0` und ausdruecklicher
Owner-Autorisierung angehalten.

GitHub-CI laeuft wie bisher nur bei einem Nicht-Draft-PR. Der Owner setzt
Ready erst nach `PRE_READY_LOCAL_GATES=PASS`; der Agent setzt Ready nicht
selbst. Nach einem CI-Fehler gilt die bestehende Fehler-/Korrektur- und
Fix-Verification-Regel.

## 10. Nicht-Ziele und Sicherheitsgrenzen

- keine Aenderung von Finding-Klassen, Builder-/Reviewerrollen,
  Ownerrechten oder Merge-/Issue-Close-Rechten;
- keine Aenderung an Firmware-, Safety-, Recovery-, Sensor-, Aktor-,
  Hardware-, Persistenz- oder UI-Vertraegen;
- keine neue CI-Plattform, kein neuer Dispatcher und kein allgemeiner
  Workflow-/Test-Runner;
- keine zweite manuelle Befehlsliste fuer die Gateausfuehrung;
- keine automatische Ready-, Merge-, Auto-Merge- oder Issue-Schlussaktion;
- keine Hardwarefreigabe und keine Gleichsetzung von lokalem PASS mit
  GitHub-CI- oder Owner-PASS.

## 11. Offene Entscheidungen und Risiken

Es gibt keine fachliche Ownerentscheidung. Die einzige Umsetzungsvoraussetzung
ist die Ownerfreigabe des exakten versionierten Plans.

Der lokale Lauf benoetigt fuer die ESP-IDF-Phasen dieselbe festgelegte
ESP-IDF-6.0.2-Umgebung wie CI. Ist diese Umgebung nicht vorhanden, ist der
Lauf `BLOCKED`; der Runner darf nicht auf einen anderen Compiler oder eine
andere ESP-IDF-Version ausweichen. Ebenso bleibt ein fehlendes clang-format-
oder clang-tidy-18-Werkzeug ein nicht bestandenes Pre-Ready-Gate.

Die Artefakt-Uploadpfade muessen im Workflow aus technischen Gruenden sichtbar
bleiben. Die minimale Erweiterung der bestehenden Coverage-Pruefung ist daher
erforderlich, damit daraus keine zweite Scanpfadquelle entsteht.

## 12. Abschlussprovenienz

```text
ISSUE=150
PR=151
BASE_BRANCH=main
BASE_SHA=e84dfa8abf220220a33e6e21b95dbd0d7bd9ac90
ROADMAP_SYNC_COMMIT=7e8aea9d5a7958cb6e7b84e269d9ab89ef8c62cb
PLAN_REVISION=INITIAL
SUPERSEDES_PLAN_COMMIT=NONE
IMPLEMENTATION=NOT_STARTED
ACTUATOR_RELEASE=NO
```

Nach dem Plan-Commit werden die exakte Plan-SHA, der PR-HEAD und der
Planstatus im PR-Body sowie im einzigen aktuellen `SESSION HANDOVER`
eingetragen. Danach haelt der Agent fuer die Ownerfreigabe an.
