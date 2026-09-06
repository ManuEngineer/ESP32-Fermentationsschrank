# Plan – Issue #154: Static-Analysis-Self-Check vor Independent Review

## Planstatus und harte Basis

Dieser Plan ist ausschließlich ein Plan-only-Artefakt. Er startet keine
Implementation, keinen vollständigen Pre-Ready-Lauf und keine Ready-, Merge-,
Issue-Schluss- oder Aktorfreigabe. Die Umsetzung darf erst nach ausdrücklicher
Ownerfreigabe der exakten Commit-SHA dieses vollständigen Plans beginnen.

```text
ISSUE=154
TITLE=[Governance] Static-Analysis-Self-Check vor Independent Review
BASE_BRANCH=main
BASE_SHA=253f6135b86d607d703d25bd3a2413b3d83fb54e
EXPECTED_BASE_SHA=253f6135b86d607d703d25bd3a2413b3d83fb54e
PLAN_STATUS=OWNER_PLAN_APPROVAL_PENDING
IMPLEMENTATION=NOT_STARTED
OWNER_PLAN_APPROVAL_REQUIRED=YES
ACTUATOR_RELEASE=NO
```

Verifiziert am Planbeginn:

```text
CONTEXT_BASELINE_BRANCH=agent/issue-154-static-analysis-self-check
CONTEXT_BASELINE_SHA=253f6135b86d607d703d25bd3a2413b3d83fb54e
CONTEXT_HEAD_SHA=253f6135b86d607d703d25bd3a2413b3d83fb54e
CONTEXT_PLAN_SHA=NONE
CONTEXT_REFRESH_MODE=FULL
CONTEXT_DELTA=origin/main, Issue #154, PR #143, PR #153, Issue #26,
  Issue #152, Roadmap, AGENTS.md, Agent-Workflow, Engineering-Principles,
  CI-/Quality-Gate-Vertrag, bestehender Runner und .clang-tidy
SOURCE_OF_TRUTH_CONFLICT=ROADMAP_STATUS_STALE_FOR_ISSUE26_AND_ISSUE152;
  corrected in this plan/governance scope
```

Der aktuelle Live-Abgleich ergibt: `origin/main` und die erwartete Basis stehen
auf `253f6135…`; PR #143 ist mit diesem Commit als Merge-Commit gemergt, Issue
#26 ist geschlossen, PR #153 ist gemergt und Issue #152 ist geschlossen. Issue
#154 ist offen und besitzt noch keinen Pull Request oder Kommentar. PR #151 /
Issue #150 ist als vorherige Pre-Ready-CI-Parity-Governance abgeschlossen.

## 1. Ziel und Nicht-Ziele

### Ziel

PR-eigene clang-format- und clang-tidy-Verstöße sollen im Builder-Self-Check
der Draft-/Handover-Phase fail-closed und reproduzierbar sichtbar werden,
bevor der unabhängige Full Review beginnt. Der Self-Check prüft nur den
veränderten Teil des bestehenden Static-Analysis-Vertrags:

1. clang-format-18 früh gegen die vom PR geänderten C/C++-Dateien im bereits
   geltenden Formatbereich ausführen;
2. nur wenn eine vom PR geänderte Datei in der bestehenden kanonischen
   clang-tidy-Menge liegt, die native Compile-Datenbank erzeugen;
3. clang-tidy-18 anschließend nur auf den geänderten Dateien aus dieser
   bestehenden Menge ausführen.

Der Nachweis erhält einen eigenen Self-Check-Status. Er ist kein
`PRE_READY_HOST_GATES`, kein `PRE_READY_ESP_GATES` und kein
`PRE_READY_LOCAL_GATES`.

### Nicht-Ziele

- keine Firmware-, Fachlogik-, Safety-, Recovery-, Hardware-, GPIO-,
  Persistenz-, UI- oder Architekturänderung;
- keine Änderung an `.clang-tidy`, `WarningsAsErrors`, Thresholds oder
  Unterdrückungen;
- keine zweite clang-tidy-Dateiliste und keine zweite Gatewahrheit;
- keine vollständige Native-Suite, kein `build_report.py`, kein ESP-IDF-Build
  und keine vollständige esp-clang-Gesamtausführung in der normalen
  Draft-/Handover-Phase;
- kein Vorverlegen des vollständigen `host`-/`esp`-Pre-Ready-Vertrags;
- keine Änderung der GitHub-CI-Auslösung, der CI-only-Artefakt-/Privacy-Gates
  oder der Ownerrechte;
- keine vorsorgliche Runner-Abstraktion, kein neues Framework und kein neuer
  Static-Analysis-Dienst;
- keine Implementation vor Ownerfreigabe dieser exakten Plan-SHA.

## 2. Kanonische Quellen und Owner

Die folgenden Verantwortungen werden wiederverwendet und nicht parallel
nachgebaut:

| Verantwortung | Kanonischer Owner / Vertrag |
|---|---|
| Toolversionen, portabler Hostlauf, clang-format und clang-tidy-Dateiliste | `scripts/run_pre_ready_gates.sh` |
| Checkkonfiguration und `WarningsAsErrors: "*"` | `.clang-tidy` |
| Ausführungszeitpunkt, Statusbegriffe und Trennung von Draft, Review, Pre-Ready und CI | `docs/CI_AND_QUALITY_GATES.md` |
| Builder-Self-Check, Independent Review und Owner-Gates | `AGENTS.md`, `docs/AGENT_WORKFLOW.md` |
| GitHub-CI-only Artefakt-/Privacy-Scanpfade und Uploadtransport | `.github/workflows/build.yml` |
| aktuelle Status- und Reihenfolgenübersicht | `docs/ROADMAP.md` |

Der Runner enthält derzeit die getrennten `host`- und `esp`-Phasen. `host`
prüft bereits Toolprovenienz, den vollständigen Formatbereich, native
Build-/Test-/Ressourcenpfade, die Compile-Datenbank, die eine bestehende
clang-tidy-Dateiliste sowie Architektur-, Secret- und Quality-Gates. `esp`
führt die bestehenden ESP-IDF-/esp-clang-Pfade aus. Die vollständige
clang-tidy-Dateiliste wird für diesen Plan nicht erneut aufgeschrieben,
sondern ausschließlich aus dem Runner wiederverwendet.

Die `.clang-tidy`-Konfiguration aktiviert die relevanten `bugprone-*`,
`cert-*`, `misc-*`, `modernize-*`, `performance-*`, `portability-*` und
`readability-*`-Checks, setzt `WarningsAsErrors: "*"` und begrenzt den
Headerfilter auf `include/`, `lib/` und `main/`. Die Konfiguration bleibt
unverändert. Die vorhandenen Ausnahmebegründungen und der getrennte
ESP-IDF-/esp-clang-Pfad bleiben ebenfalls unverändert.

Die Firmware-CI läuft nur für einen Nicht-Draft-PR. Der Builder-Self-Check
ist deshalb ein lokaler, gezielter Draft-Nachweis; er emuliert weder GitHub-CI
noch deren Artefakttransport. `Ready for review`, der vollständige lokale
Pre-Ready-Lauf, GitHub-CI, Merge und Issue-Abschluss bleiben Owner-Gates.

## 3. Gewählte KISS-Lösung

### Entscheidung: kleine zusätzliche Phase im bestehenden Runner

Gewählt wird eine zusätzliche `self-check`-Phase in
`scripts/run_pre_ready_gates.sh`, nicht ein neues Skript und kein Aufruf des
vollständigen `host`-Laufs mit versteckten Skip-Schaltern.

Die Phase ist die kleinste korrekte Lösung, weil sie:

- die bereits vorhandene Toolprüfung des Runner-Owners wiederverwendet;
- die vorhandene clang-tidy-Dateiliste an genau einer Stelle belässt;
- die bestehende clang-tidy-Ausführung nur um eine Auswahl der PR-geänderten
  Einträge ergänzt;
- clang-format vor der Compile-Datenbank ausführt;
- die Compile-Datenbank ausschließlich bei tatsächlich zu prüfenden
  clang-tidy-Dateien erzeugt;
- keinen neuen Workflow, keine neue Gatebefehlssammlung und kein neues
  Framework einführt;
- `host` und `esp` als vollständige, unveränderte Pre-Ready-Verträge
  beibehält.

Ein separater Runner würde Toolprovenienz und Dateiauswahl duplizieren. Ein
vollständiger `host`-Aufruf mit Umgebungsflags würde dagegen den
Draft-Self-Check und das verpflichtende Pre-Ready-Gate semantisch vermischen.
Beides ist nicht KISS und erzeugt eine konkurrierende Wahrheit.

## 4. Geplanter technischer Vertrag

### 4.1 Einstieg und Baseline

Nach Ownerfreigabe wird der Self-Check mit einem expliziten, exakt
verifizierten Basis-Commit aufgerufen:

```bash
PRE_READY_EXPECTED_HEAD="$(git rev-parse HEAD)" \
STATIC_ANALYSIS_BASE_SHA="<exakter-main-Basis-Commit>" \
bash scripts/run_pre_ready_gates.sh self-check
```

Für diesen PR ist `<exakter-main-Basis-Commit>`
`253f6135b86d607d703d25bd3a2413b3d83fb54e`. Der Runner prüft für die neue
Phase, dass `STATIC_ANALYSIS_BASE_SHA` ein Commit ist, dass der Commit
Ancestor von `HEAD` ist und dass der vorhandene
`PRE_READY_EXPECTED_HEAD`-Schutz nicht verletzt wird. Ein fehlender,
ungültiger oder nicht ancestraler Basis-Commit beendet den Self-Check mit
`FAILED`/`BLOCKED`; es gibt keinen stillen Fallback auf einen anderen Remote-
oder Merge-Stand.

Die PR-Dateiauswahl wird aus dem Diff zwischen dieser Basis und `HEAD`
abgeleitet. Damit werden keine fremden, bereits vor dem PR bestehenden
Diagnosen als PR-eigene Befunde ausgegeben. Der Self-Check bleibt auf dem
konkret geprüften Checkout.

### 4.2 clang-format-18

Die `self-check`-Phase verwendet dieselbe vom Runner bereits geprüfte
clang-format-Major-Linie 18. Sie filtert aus dem Basis-zu-`HEAD`-Diff nur
existierende, geänderte C/C++-Dateien aus dem bestehenden
`src/`, `include/`, `lib/`, `test/`, `main/`-Formatbereich und ruft darauf
`clang-format --dry-run --Werror` auf.

Der Formatlauf steht vor jeder Compile-Datenbankerzeugung. Bei einem
Formatverstoß oder fehlendem/falschem clang-format endet die Phase
fehlgeschlagen bzw. blockiert und darf keinen Self-Check-PASS melden. Gibt es
keine geänderte Datei in diesem Formatbereich, ist für diesen Teil
`NOT_REQUIRED` zulässig; es wird keine vollständige Formatprüfung als
Draft-Self-Check eingeschmuggelt.

### 4.3 clang-tidy-18 und Compile-Datenbank

Die bestehende kanonische clang-tidy-Dateiliste im Runner bleibt die einzige
Dateiliste. Für den Self-Check wird sie nicht kopiert oder erweitert. Die
Implementierung stellt diese vorhandene Liste lediglich so bereit, dass

- der vollständige `host`-Lauf weiterhin alle bisher vorgesehenen Einträge
  exakt ausführt; und
- der `self-check`-Lauf die Schnittmenge aus PR-geänderten Pfaden und genau
  dieser Liste an `clang-tidy` übergibt.

Nur bei einer nichtleeren Schnittmenge wird
`pio run -e native -t compiledb` ausgeführt. Diese native
Compile-Datenbankerzeugung ist der einzige native Buildschritt des
Self-Checks und dient ausschließlich dem danach erforderlichen clang-tidy-
Aufruf. `pio test -e native`, `build_report.py`, Architekturguard,
Repository-Secret-Check, Quality-Gate-Selbsttest sowie alle ESP-IDF-/esp-
clang-Schritte bleiben außerhalb dieser Phase.

`clang-tidy-18` wird mit `-p .` und genau der ermittelten Teilmenge aus der
kanonischen Runner-Liste aufgerufen. Ein clang-tidy-Fehler, ein fehlendes
Compile-Database-Artefakt oder ein fehlendes/falsches clang-tidy beendet die
Phase nicht erfolgreich. Bei einer leeren Schnittmenge wird die
Compile-Datenbank als `NOT_REQUIRED` dokumentiert; das Ergebnis bleibt nur
dann `BUILDER_STATIC_ANALYSIS_SELF_CHECK=PASS`, wenn alle tatsächlich
erforderlichen Teilprüfungen bestanden sind.

### 4.4 Unveränderter vollständiger Pre-Ready-Vertrag

Die vorhandenen `host`- und `esp`-Phasen bleiben die vollständigen
Pre-Ready-Gates. Ihre Reihenfolge, Toolprovenienz, vollständige Native-Suite,
vollständige Formatprüfung, vollständige kanonische clang-tidy-Dateiliste,
ESP-IDF-Profile, Ressourcenberichte, esp-clang-Analyse und Statusmarker
werden nicht in die Draft-Phase verschoben und nicht semantisch abgeschwächt.

Der Builder-Self-Check darf keinen `PRE_READY_HOST_GATES=PASS`,
`PRE_READY_ESP_GATES=PASS` oder `PRE_READY_LOCAL_GATES=PASS` ausgeben. Der
vollständige lokale Lauf bleibt ausschließlich nach abgeschlossenem
Independent Full Review mit `OPEN_BLOCKERS=0`, finalem `HEAD` und ausdrücklicher
Owner-Anordnung zulässig. Erst danach darf der Owner den PR auf Ready setzen;
GitHub-CI und Merge bleiben weitere getrennte Owner-Gates.

## 5. Geplanter Dateiscope nach Planfreigabe

### Zu ändern

- `scripts/run_pre_ready_gates.sh` – zusätzliche `self-check`-Phase, explizite
  Basisprüfung, geänderte-Dateien-Auswahl und Wiederverwendung der einen
  bestehenden clang-tidy-Liste; keine Änderung der vollständigen
  `host`-/`esp`-Semantik;
- `docs/CI_AND_QUALITY_GATES.md` – kanonische Beschreibung des neuen
  Draft-Self-Checks, seines Status und seiner Grenzen; keine zweite
  Befehlsliste und keine zweite Dateiliste;
- `docs/AGENT_WORKFLOW.md` – Verweis auf den Builder-Self-Check als gezielten
  Nachweis vor dem Independent Review und klare Trennung vom vollständigen
  Owner-autorisierten Pre-Ready-Lauf;
- `docs/ROADMAP.md` – die im selben Plan-/Governance-Scope erforderliche
  Post-#143-/Post-#153-Status- und Reihenfolgensynchronisierung.

### Unverändert

`AGENTS.md`, `.clang-tidy`, `.github/workflows/build.yml`, sämtliche
Firmware-/Test-/Fach-/Safety-/Recovery-/Hardware-/Persistenzdateien,
Buildprofile, Bibliotheksdefinitionen und ESP-IDF-Skripte bleiben unverändert.
Die vorhandene Root-Regel zu Builder-Self-Check und Independent Review wird
nicht durch eine neue parallele Governance ersetzt.

## 6. Umsetzungsschnitte und Owner-Gates

Nach Freigabe der exakten Plan-SHA:

1. Vor dem ersten semantischen Änderungscommit Branch, `HEAD`, Basis-SHA,
   Draft-PR, Plan-SHA und Roadmap-Stand erneut live verifizieren. Bei einer
   materiellen Baseline-Abweichung anhalten und den Plan aktualisieren.
2. Den Runner in einem kleinen Implementierungsschnitt um `self-check`
   erweitern. Dabei die bestehende clang-tidy-Liste in eine einmalige,
   gemeinsam verwendete Datenquelle überführen, ohne ihren Inhalt zu ändern.
3. Den gezielten Self-Check-Vertrag in `docs/CI_AND_QUALITY_GATES.md` und
   `docs/AGENT_WORKFLOW.md` dokumentieren. Keine Workflow-CI und kein
   vollständiges Gate in diesem Schnitt ergänzen.
4. Den Builder-Self-Check auf dem Implementierungs-HEAD und danach den
   vollständigen Diff gegen diesen Plan prüfen. Der Builder-Self-Check ist
   kein Independent Full Review.
5. Nach dem Self-Check mit `OPEN_BLOCKERS`- und Statusnachweis für den
   unabhängigen Full Review anhalten. Der PR bleibt Draft; Ready, Merge,
   Auto-Merge, Force-Push, Branch-Löschen und Issue-Schluss erfolgen nicht
   durch den Agenten.

Bei einer materiellen Abweichung an Scope, Runner-Owner, Toolprovenienz,
Dateiliste, Compile-Datenbankstrategie, Pre-Ready-Trennung,
Akzeptanzkriterien oder Teststrategie wird die Umsetzung angehalten, der Plan
revidiert und eine neue exakte Ownerfreigabe eingeholt.

## 7. Reproduzierbare Nachweise und Akzeptanzkriterien

### 7.1 Plan-/Implementierungs-Self-Check

Für die reine Planungsphase werden keine Builds und keine vollständigen
Testläufe ausgeführt. Nach einer Ownerfreigabe sind für den Draft-Scope nur
gezielte Nachweise vorgesehen:

- `bash -n scripts/run_pre_ready_gates.sh`;
- Prüfung des `self-check`-Usage-/Basis- und HEAD-Fail-Closed-Verhaltens;
- `git diff --check` und Scope-Dateiliste;
- Prüfung, dass die kanonische clang-tidy-Liste nur einmal im Runner
  vorhanden ist und `host` sowie `self-check` dieselbe Quelle verwenden;
- Prüfung, dass der Self-Check weder `pio test -e native` noch ESP-IDF-,
  esp-clang- oder vollständige Pre-Ready-Schritte aufruft;
- gezielter Builder-Self-Check auf dem sauberen Implementierungs-HEAD.

Ein normaler plan-/dokumentations-only PR ohne geänderte C/C++-Datei darf
keine Compile-Datenbank erzeugen müssen; der Self-Check muss diesen Fall
transparent mit `CLANG_TIDY=NOT_REQUIRED` und einem erfolgreichen eigenen
Self-Check-Status beenden.

### 7.2 Acceptance Fixture: absichtlicher clang-tidy-Verstoß

Der zentrale Nachweis wird in einem temporären, nach der Prüfung entfernten
Worktree auf dem freigegebenen Implementierungs-HEAD ausgeführt. Die
Produktionsdateien des eigentlichen PR bleiben dabei sauber; der absichtliche
Verstoß wird nur als Disposable-Fixture-Commit eingebracht.

1. Den Basis-Commit als exakt
   `STATIC_ANALYSIS_BASE_SHA=253f6135b86d607d703d25bd3a2413b3d83fb54e`
   setzen und `PRE_READY_EXPECTED_HEAD` auf den jeweils getesteten Fixture-
   HEAD setzen.
2. In `lib/fermentation_app/src/program_model.cpp`, einer bereits im
   kanonischen Runner-Scope liegenden Produktionsdatei, für den Fixture-Commit
   bei der bestehenden einteiligen `if (program.preheat)`-Verzweigung die
   Blockklammern entfernen. Die Änderung bleibt semantisch gleich, ist aber
   ein absichtlicher Verstoß gegen den aktuell aktivierten
   `readability-braces-around-statements`-Check; `.clang-tidy` wird nicht
   geändert.
3. Den Self-Check exakt mit

   ```bash
   PRE_READY_EXPECTED_HEAD="$(git rev-parse HEAD)" \
   STATIC_ANALYSIS_BASE_SHA="253f6135b86d607d703d25bd3a2413b3d83fb54e" \
   bash scripts/run_pre_ready_gates.sh self-check
   ```

   ausführen. Erwartet wird ein nicht erfolgreicher Lauf mit einem
   clang-tidy-Befund für den absichtlich geänderten Produktionspfad. Der
   Builder-Self-Check darf keinen PASS-Marker liefern. Damit ist bewiesen,
   dass eine PR-eigene Änderung aus der kanonischen clang-tidy-Menge vor dem
   Independent Review gefunden wird.
4. Den Fixture-Verstoß vollständig entfernen und denselben Self-Check auf dem
   sauberen Fixture-/Implementierungs-HEAD erneut ausführen. Erwartet wird
   `BUILDER_STATIC_ANALYSIS_SELF_CHECK=PASS`; die Compile-Datenbank wird nur
   für den Tidy-Nachweis erzeugt.

### 7.3 Ergänzender clang-format-Nachweis

In einem zweiten Disposable-Fixture-Commit wird eine minimale reine
Formatabweichung in derselben oder einer anderen geänderten Datei aus dem
bestehenden Formatbereich erzeugt. Der gleiche `self-check`-Aufruf muss vor
der Compile-Datenbank mit clang-format-18 fehlschlagen. Nach vollständigem
Entfernen der Formatabweichung muss er PASS liefern. Ein separates, nicht
kanonisches Format-Tool oder eine Ausnahme ist nicht zulässig.

### 7.4 Getrennte Pflichtgates

Der Draft-Nachweis dokumentiert ausdrücklich:

```text
BUILDER_STATIC_ANALYSIS_SELF_CHECK=PASS|FAILED|BLOCKED
PRE_READY_HOST_GATES=NOT_RUN_IN_DRAFT_SELF_CHECK
PRE_READY_ESP_GATES=NOT_RUN_IN_DRAFT_SELF_CHECK
PRE_READY_LOCAL_GATES=NOT_RUN_IN_DRAFT_SELF_CHECK
GITHUB_CI=NOT_RUN_WHILE_DRAFT
HARDWARE=NOT_RUN
ACTUATOR_RELEASE=NO
```

Der vollständige `run_pre_ready_gates.sh host`-/`esp`-Lauf wird in dieser
Draft-/Handover-Phase nicht vorgezogen. Er bleibt nach Independent Full
Review, `OPEN_BLOCKERS=0` und ausdrücklicher Owner-Autorisierung als
separates Pflichtgate auf finalem `HEAD` bestehen. Ein Self-Check-PASS ersetzt
weder dieses Gate noch GitHub-CI oder den Merge-Gate.

## 8. Offene Entscheidungen, Risiken und Abschlussstatus

Es gibt keine offene technische Auswahlentscheidung: Die KISS-Entscheidung
für eine kleine zusätzliche Phase im bestehenden Runner ist Bestandteil
dieses Plans. Offen ist ausschließlich die ausdrückliche Ownerfreigabe der
exakten versionierten Plan-SHA. Vor Umsetzung muss diese Freigabe live
verifiziert werden.

Risiken bleiben begrenzt und fail-closed:

- Ein veralteter oder nicht ancestraler Basis-Commit blockiert, statt einen
  falschen PR-Diff zu analysieren.
- Eine fehlende oder falsche clang-18-/PlatformIO-Umgebung blockiert bzw.
  schlägt fehl; kein anderer Compiler und kein anderer Patchlevelvertrag wird
  still verwendet.
- Die kanonische clang-tidy-Menge bleibt unverändert. Nicht in dieser Menge
  enthaltene Dateien werden nicht still durch eine neue Self-Check-Liste
  ergänzt; eine spätere Scopeänderung benötigt einen eigenen Plan-/Owner-
  Beschluss.
- Lokale Self-Check- und Compile-Database-Ergebnisse sind keine ESP-IDF-,
  Hardware-, GitHub-CI- oder Ready-Nachweise.

Der erwartete Abschluss dieses Plan-only-Auftrags ist:

```text
PLAN_COMMITTED=YES
ROADMAP_SYNC_COMMITTED=YES
IMPLEMENTATION=NOT_STARTED
OWNER_PLAN_APPROVAL_REQUIRED=YES
PR_DRAFT=YES
READY_FOR_REVIEW=NO
MERGE=NO
ISSUE_CLOSE_BY_AGENT=NO
ACTUATOR_RELEASE=NO
```
