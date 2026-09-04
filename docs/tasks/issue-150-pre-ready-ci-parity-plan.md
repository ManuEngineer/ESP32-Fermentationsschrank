# Plan – Issue #150: Pre-Ready-CI-Parity-Gate

Status: Planungsphase (`IMPLEMENTATION_BLOCKED_PENDING_PLAN_APPROVAL`)

Issue: `#150`
Pull Request: `#151` (Draft)
Basis-Branch: `main`
Basis-SHA: `e84dfa8abf220220a33e6e21b95dbd0d7bd9ac90`
Roadmap-Sync: `7e8aea92278e70077bf4fb1b381d8bc12d0007ed`
Planrevision: `TOOLCHAIN_AND_PHASE_CORRECTION`
Implementation: `NOT_STARTED`
ACTUATOR_RELEASE: `NO`
HISTORICAL_APPROVED_PLAN_COMMIT: `5c97e61fed5399062e95d788ec3a378fb3d3b595`
OWNER_APPROVED_SCOPE_PRECISION: `FINAL_KISS_PRE_READY_TRENNUNG`

Dieser Plan ist ein eigenstaendiger Governance-/Build-Tooling-Plan. Er ist
nicht Teil von PR #147 / Issue #144 und aendert weder deren Code noch deren
Review- oder Ownerstatus.

## Owner-genehmigte Scope-Praezisierung

Die folgende Ausfuehrungsgrenze praezisiert den historisch freigegebenen
Planstand `5c97e61fed5399062e95d788ec3a378fb3d3b595` und fuehrt keine neue
Produkt-, Architektur- oder Toolchainentscheidung ein:

- `host` enthaelt den portablen Repository-Secret-Check
  `python3 scripts/check_secrets.py` ohne `--scan-path`;
- `esp` endet nach Profilbuild, Ressourcenbericht und esp-clang-Analyse;
- `check_ci_artifact_scan_coverage.py` sowie
  `check_secrets.py --scan-path ...` laufen ausschliesslich in GitHub-CI;
- die generierten Artefakt-Scanpfade stehen nur im GitHub-Workflow neben den
  Uploads;
- der lokale Pre-Ready-Lauf emuliert keinen GitHub-Artefakttransport.

Die historische Planfreigabe bleibt als Provenienz erhalten; diese
Owner-genehmigte Scope-Praezisierung ist die verbindliche Umsetzungsschaerfung
fuer die finale KISS-Trennung.

## 1. Ziel und Anlass

Vor `Ready for review` muss der Owner auf dem exakt finalen PR-HEAD einen
autorisierten lokalen Lauf aller lokal reproduzierbaren portablen
Engineering-Gates verifizieren koennen. Der Lauf muss insbesondere denselben
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
- Architekturguard, portabler Repository-Secret-Check und
  Quality-Gate-Selbsttests;
- ESP-IDF-6.0.2-Installation und Herkunftspruefung, Bring-up-/Release-Build,
  Ressourcenbericht und esp-clang-Static-Analysis;
- GitHub-CI-only Artefakt-Scanabdeckungspruefung und Secretpruefung der
  erzeugten Textartefakte.

Die Workflow-Installation garantiert fuer Format und Analyse nur die
kanonische Major-Linie 18. Ein Patchlevel wie `18.1.8` ist weder fuer
`clang-format-18` noch fuer `clang-tidy-18` Teil des Vertrags. Die aktuelle
CI-Umgebung liefert fuer `clang-tidy-18` nachweislich LLVM `18.1.3`; die
Dokumentation und Workflowbezeichnung duerfen daher keinen anderen Patchlevel
behaupten.

Die bestehenden Fach- und Gate-Owner bleiben:

- `scripts/build_report.py` bleibt Owner fuer nativen Build und
  Ressourcenbericht sowie die offizielle ESP-IDF-Groessen-/Manifestauswertung;
- `scripts/build_esp_idf_profiles.py` bleibt Owner fuer Profilbuild und
  Profilvalidierung;
- `scripts/run_esp_idf_static_analysis.py` bleibt Owner fuer den
  esp-clang-/ESP-IDF-Analysepfad;
- `scripts/check_architecture_boundaries.py`, `check_secrets.py` ohne
  `--scan-path` und `selftest_quality_gates.py` bleiben Owner der portablen
  Engineering-Pruefvertraege;
- `check_ci_artifact_scan_coverage.py` und `check_secrets.py --scan-path`
  bleiben Owner der GitHub-CI-only Artefakt-/Privacy-Pruefvertraege;
- `.clang-tidy` bleibt die alleinige clang-tidy-Konfiguration; nur eine
  veraltete Patchlevel-Kommentarangabe wird auf Major-18-Semantik bereinigt.

Der gemeldete CI-Verlauf von PR #147 (`clang-format`-Fehler in Run #1032,
anschliessender `clang-tidy`-Fehler in Run #1033) ist Anlass und Regression-
Motivation, aber kein neuer Firmware- oder Architektur-Scope.

## 4. KISS-Loesung gegen Gate-Drift

### 4.1 Gemeinsamer, phasenfaehiger Runner

Eine neue, schmale Datei
`scripts/run_pre_ready_gates.sh` wird die kanonische Ausfuehrung der lokal
reproduzierbaren Gates enthalten. Sie ist ein fail-fast Shell-Orchestrator mit
`set -euo pipefail`, keine neue Abstraktions- oder Testplattform.

Der Runner akzeptiert genau die Phasen `host` und `esp`:

```bash
bash scripts/run_pre_ready_gates.sh host
bash scripts/run_pre_ready_gates.sh esp
```

Beide Aufrufe verwenden denselben finalen Checkout. Ein optionaler
`PRE_READY_EXPECTED_HEAD` wird in jedem Aufruf gegen `git rev-parse HEAD`
geprueft; bei Abweichung oder fehlender Phase bricht der Runner fail-closed
ab. Der lokale Gesamtstatus `PRE_READY_LOCAL_GATES=PASS` darf erst nach
erfolgreichem `host`- und `esp`-Aufruf auf demselben erwarteten HEAD
dokumentiert werden.

Die portablen Gate-Befehle und die clang-tidy-Dateiliste stehen nur im Runner.
Die Dokumentation und `.github/workflows/build.yml` rufen dieselbe Datei mit
der jeweiligen Phase auf. Die separaten GitHub-CI-only Artefakt-/Privacy-
Aufrufe bleiben ausschliesslich im Workflow.

### 4.2 Host-Phase und kanonische Host-Provenienz

`run_pre_ready_gates.sh host` verifiziert vor jedem billigen Host-Gate:

- `pio --version` ist exakt PlatformIO `6.1.19`;
- `clang-format --version` und `clang-tidy --version` gehoeren jeweils zur
  Major-Linie 18;
- die verwendeten Befehle sind vorhanden; ein fehlendes oder nicht
  parsierbares Werkzeug ist `BLOCKED`/Fehler und kein `PASS`.

Patchlevel wird absichtlich nicht als Vertrag behauptet. Damit verwenden CI
und lokaler Lauf dieselbe nachweisbare Major-18-Semantik, ohne eine neue
Toolchainplattform oder eine unhaltbare `18.1.8`-Annahme einzufuehren.

Erst nach dieser Verifikation folgen in der Host-Phase:

1. voller Formatcheck:

   ```bash
   clang-format --dry-run --Werror \
     $(find src include lib test main -type f \( -name "*.cpp" -o -name "*.hpp" -o -name "*.h" \))
   ```

2. nativer Build und nativer Ressourcenbericht ueber
   `python scripts/build_report.py --output build-report.md`. stdout/stderr
   wird zusaetzlich in `platformio-build.log` geschrieben, damit der
   bestehende CI-Fehlerlog-Upload erhalten bleibt;
3. `pio test -e native`;
4. `pio run -e native -t compiledb` und der unveraenderte kanonische
   clang-tidy-18-Lauf mit der bisherigen Produktionsdateiliste, die
   ausschliesslich in den Runner wandert. `.clang-tidy` bleibt die alleinige
   Checkkonfiguration;
5. `python3 scripts/check_architecture_boundaries.py`,
   `python3 scripts/check_secrets.py` ohne `--scan-path` und
   `python3 scripts/selftest_quality_gates.py`.

### 4.3 Phasenhandoff der ESP-IDF-Umgebung

Nach erfolgreichem Host-Aufruf stellt CI die bestehende ESP-IDF-6.0.2-
Umgebung bereit bzw. verifiziert sie. Lokal stellt der Owner dieselbe
Umgebung bereit. Dieser Umgebungsschritt ist keine abweichende Gatefolge:

- Checkout, Python/PlatformIO, clang-18-Pakete, ESP-IDF-Checkout/-Installation
  und `export.sh` bleiben Provisionierung;
- die Provisionierung darf keine andere Version als die kanonische
  PlatformIO-/clang-Major-/ESP-IDF-Vorgabe verwenden;
- `python "$IDF_PATH/tools/idf_tools.py" install esp-clang` bleibt
  Installation und wird nicht als eigener Parallel-Runner implementiert;
- vor der ESP-Phase muessen `IDF_PATH`, `idf.py` und der exportierte
  Toolchainkontext vorhanden sein.

Der Workflow behält damit den fail-fast-Vorteil: Ein Format-, Build-,
Test- oder clang-tidy-Fehler beendet die Host-Phase, bevor ESP-IDF
bereitgestellt wird.

### 4.4 ESP-Phase und kanonische ESP-Provenienz

`run_pre_ready_gates.sh esp` prueft als ersten ESP-Gatekontext die bestehende
ESP-IDF-Herkunft: `v6.0.2` am exakten Commit
`7101770dc6db2667b3c477cc31365dd1acd6db4e`, aktives `idf.py` und gesetztes
`IDF_PATH`. Dafuer werden die vorhandenen Verifikationen der
ESP-IDF-Build-/Analyse-Skripte verwendet, nicht parallel nachgebaut.

Danach fuehrt der Runner aus:

1. `python scripts/build_esp_idf_profiles.py all` fuer Bring-up und Release;
2. `python scripts/build_report.py --append --esp-idf-profiles bringup release`
   mit `SOURCE_GIT_SHA`, wenn dieser in CI gesetzt ist, sonst mit der
   bestehenden lokalen Defaultsemantik;
3. die vorhandene esp-clang-Provenienzpruefung zusammen mit
   `python scripts/run_esp_idf_static_analysis.py all`. Diese bestaetigt den
   aktivierten ESP-IDF-/esp-clang-Kontext und dessen bestehenden exakten
   Tool-/Versionsvertrag vor der Analyse.

Die GitHub-CI-only Artefakt-Scanabdeckung und die Secretpruefung der erzeugten
Textartefakte laufen erst im Workflow nach diesem Runner und vor den Uploads.

Der gemeinsame `esp`-Runner enthaelt keine Artefaktpfade und emuliert keinen
GitHub-Artefakttransport. Die konkreten `--scan-path`-Angaben stehen nur im
Workflow neben der bestehenden Uploadkonfiguration; Binaerdateien bleiben
entsprechend dem bestehenden Vertrag vom Textscan ausgenommen.

### 4.5 Artefakt-Scanabdeckung ohne zweite Pfadwahrheit

`check_ci_artifact_scan_coverage.py` prueft ausschliesslich im GitHub-Workflow
die dortige `--scan-path`-Menge gegen die erfolgreichen Textartefakt-Uploads
aus `build.yml`. Dadurch gibt es keine zweite lokale Scanpfadquelle und keinen
lokalen GitHub-/Artefakttransport.

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
- die zwei lokalen Aufrufe `bash scripts/run_pre_ready_gates.sh host` und
  `bash scripts/run_pre_ready_gates.sh esp` mit dem bestehenden
  ESP-IDF-Umgebungshandoff dazwischen;
- die vorgeschaltete Verifikation von PlatformIO `6.1.19`, clang Major 18,
  ESP-IDF `v6.0.2` und dem aktivierten esp-clang-Kontext;
- alle portablen Host- und ESP-Gatephasen aus Abschnitt 4.2–4.4,
  einschliesslich Format, clang-tidy und ESP-IDF-Static-Analysis;
- die getrennten Ergebnisfelder `INDEPENDENT_REVIEW`,
  `PRE_READY_LOCAL_GATES` und `GITHUB_CI`.

Die Datei fuehrt keine zweite Liste der portablen Runner-Befehle. Sie verweist
fuer diese ausfuehrbare Befehlsquelle auf den versionierten Runner und
dokumentiert die GitHub-CI-only Artefakt-/Privacy-Gates getrennt. Nicht
ausgefuehrte Teile bleiben `NOT_RUN`/`BLOCKED`.

### 5.4 `.github/workflows/build.yml`

Die portablen Gate-Run-Schritte werden durch zwei Aufrufe des gemeinsamen
Runners ersetzt: zuerst `bash scripts/run_pre_ready_gates.sh host`, danach die
bestehende ESP-IDF-Installation/-Verifikation und anschliessend
`bash scripts/run_pre_ready_gates.sh esp`. Die bestehende billige
Host-fail-fast-Reihenfolge bleibt damit erhalten; ESP-IDF wird nicht vor
Format, nativer Build-/Testphase und clang-tidy provisioniert. Die GitHub-CI-
only Artefakt-/Privacy-Pruefungen bleiben als Workflow-Schritte vor den
Uploads bestehen.

Die vorhandene Toolchain-/ESP-IDF-Umgebungsinstallation und die erfolgreichen
bzw. fehlerbedingten Artefakt-Uploads bleiben erhalten. Die bisherige
Workflowbezeichnung `clang 18.1.8` wird auf `clang 18` korrigiert. Der
Workflowjob bleibt weiter an `draft == false` gebunden; es wird kein neuer
Trigger und keine Owneraktion automatisiert.

## 6. Geplanter Produktions- und Testscope

### Produktionsdateien

- `scripts/run_pre_ready_gates.sh` – neue, schmale gemeinsame Gateausfuehrung;
- `scripts/check_ci_artifact_scan_coverage.py` – prueft die eine im Workflow
  gepflegte Scanmenge gegen die erfolgreichen Uploads;
- `.github/workflows/build.yml` – ruft den Runner auf und entfernt die
  duplizierten Gatebefehle;
- `.clang-tidy` – korrigiert die veraltete Patchlevel-Kommentarangabe auf
  Major-18-Semantik, ohne die Checkkonfiguration zu aendern;
- `AGENTS.md`;
- `docs/AGENT_WORKFLOW.md`;
- `docs/CI_AND_QUALITY_GATES.md`.

### Dokumentations-/Statusdateien

- `docs/ROADMAP.md` – bereits im ersten PR-Commit um den parallelen
  Issue-#150-Status ergaenzt (`7e8aea92278e70077bf4fb1b381d8bc12d0007ed`);
  fachliche Firmwarereihenfolge
  und Issue-#147-Inhalt werden nicht kopiert.
- `docs/tasks/issue-150-pre-ready-ci-parity-plan.md` – dieses Dokument.

Es werden keine Firmware-, Safety-, Hardware-, ADR-, Fach- oder
Persistenzdateien veraendert. Es werden keine neuen Bibliotheken, Runner-
Frameworks, Registries, Dispatcher oder Buildprofile eingefuehrt.

## 7. Umsetzungsschnitte nach Planfreigabe

1. Den gemeinsamen Runner mit getrennten `host`- und `esp`-Phasen erstellen.
   Die kanonische Host-Toolchainpruefung steht vor allen Host-Gates; der
   portable Repository-Secret-Check bleibt im Runner.
2. `.github/workflows/build.yml` auf die zwei Runner-Aufrufe umstellen;
   Host-fail-fast, Toolchain-/ESP-IDF-Provisionierung und Artefaktuploads
   erhalten.
3. Die Patchlevel-Drift in `.clang-tidy`, Workflowname und
   `docs/CI_AND_QUALITY_GATES.md` auf Major-18-Semantik bereinigen.
4. `AGENTS.md`, `docs/AGENT_WORKFLOW.md` und
   `docs/CI_AND_QUALITY_GATES.md` auf Reihenfolge, Phasen, Einstieg und
   Statusbegriffe synchronisieren.
5. Mit dem Builder-Self-Check den Scope gegen diesen Plan und gegen die
   bestehenden Gate-Skripte pruefen. Erst nach Independent Review,
   `OPEN_BLOCKERS=0` und ausdruecklicher Owner-Autorisierung darf der
   vollstaendige Runner auf dem finalen HEAD ausgefuehrt werden.

Der Implementation-PR bleibt bis zur Ownerfreigabe des exakten Plan-Commits
bei `IMPLEMENTATION=NOT_STARTED`. Die Implementation darf nicht in PR #147
oder dessen Branch erfolgen.

## 8. Gezielte Nachweise und Akzeptanzkriterien

### Vor dem Pre-Ready-Lauf

- `bash -n scripts/run_pre_ready_gates.sh` ist erfolgreich;
- eine temporäre Toolchain-Fixture mit falschem PlatformIO-, clang- oder
  ESP-IDF-/esp-clang-Major-/Versionssignal wird vor dem jeweiligen Gate
  fail-closed abgelehnt und kann keinen `PRE_READY_LOCAL_GATES=PASS`
  erzeugen;
- `python3 scripts/check_ci_artifact_scan_coverage.py --selftest` ist
  erfolgreich und prueft den GitHub-CI-only Upload-/Scanvertrag;
- `python3 scripts/check_ci_artifact_scan_coverage.py` meldet in der
  Workflowdefinition keine reale Upload-/Scanluecke;
- `python3 scripts/selftest_quality_gates.py` ist erfolgreich;
- ein statischer Workflow-/Scope-Check bestaetigt, dass die Gatebefehle nicht
  neben dem gemeinsamen Runner in `build.yml` dupliziert sind und dass der
  `host`-Aufruf vor der ESP-IDF-Provisionierung sowie der `esp`-Aufruf danach
  liegt;
- `git diff --check` und die Dokumentationspruefung sind erfolgreich.

### Vollstaendiger, Owner-autorisierter Lauf

Auf exakt demselben finalen HEAD muessen beide gemeinsamen Runner-Phasen
erfolgreich durchlaufen und mindestens folgende beobachtbare Nachweise liefern:

- der volle `clang-format-18`-Check erkennt absichtlich einen lokalen
  Formatverstoss, wenn er in einer temporären Fixture vorliegt, und meldet
  den sauberen Baum als `PASS`;
- die Host-Provenienz meldet PlatformIO `6.1.19` sowie clang-format und
  clang-tidy aus Major 18;
- die native Build-/Ressourcen- und komplette Testphase besteht;
- Compile-Datenbank und exakt dieselbe clang-tidy-18-Dateiliste wie in CI
  bestehen;
- Architekturguard, portabler Repository-Secret-Check und Quality-Gate-
  Selbsttests bestehen;
- beide ESP-IDF-Profile sowie esp-clang-Static-Analysis bestehen;
- die ESP-Provenienz bestaetigt `v6.0.2` am Commit
  `7101770dc6db2667b3c477cc31365dd1acd6db4e` und den aktivierten
  esp-clang-Kontext;
- die GitHub-CI-only Artefakt-Scanabdeckung und der Artefakt-Secretcheck
  laufen im Workflow vor den Uploads;
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
vollstaendige `run_pre_ready_gates.sh host`-/`esp`-Lauf bleibt bis zum
abgeschlossenen Independent Review mit `OPEN_BLOCKERS=0` und ausdruecklicher
Owner-Autorisierung angehalten. Der lokale Ownerlauf verwendet fuer beide
Aufrufe denselben `PRE_READY_EXPECTED_HEAD`; ein einzelner bestandener
Phasenaufruf ist kein Gesamt-PASS.

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

Der lokale Lauf benoetigt fuer die ESP-IDF-Phase dieselbe festgelegte
ESP-IDF-6.0.2-Umgebung wie CI. Ist diese Umgebung nicht vorhanden oder
weichen PlatformIO, clang-format, clang-tidy, ESP-IDF oder esp-clang von der
kanonischen Provenienz ab, ist der Lauf `BLOCKED`/fehlgeschlagen; der Runner
darf nicht auf einen anderen Compiler, Patchlevelvertrag oder eine andere
ESP-IDF-Version ausweichen. Ein fehlendes clang-format- oder clang-tidy-18-
Werkzeug bleibt ebenfalls ein nicht bestandenes Pre-Ready-Gate.

Die Artefakt-Uploadpfade und ihre `--scan-path`-Angaben muessen im Workflow
aus technischen Gruenden sichtbar bleiben. Die Coverage-Pruefung liest diese
eine CI-Quelle; der gemeinsame Runner fuehrt keinen Artefakt-/Privacy-Check
und keine lokale Scanpfadliste.

## 12. Abschlussprovenienz

```text
ISSUE=150
PR=151
BASE_BRANCH=main
BASE_SHA=e84dfa8abf220220a33e6e21b95dbd0d7bd9ac90
ROADMAP_SYNC_COMMIT=7e8aea92278e70077bf4fb1b381d8bc12d0007ed
PLAN_REVISION=TOOLCHAIN_AND_PHASE_CORRECTION
SUPERSEDES_PLAN_COMMIT=c36756d51af03f13c13835adb15cec36b57d22a3
IMPLEMENTATION=NOT_STARTED
ACTUATOR_RELEASE=NO
```

Nach dem Plan-Commit werden die exakte Plan-SHA, der PR-HEAD und der
Planstatus im PR-Body sowie im einzigen aktuellen `SESSION HANDOVER`
eingetragen. Danach haelt der Agent fuer die Ownerfreigabe an.
