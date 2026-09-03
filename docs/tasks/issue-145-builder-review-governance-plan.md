# Plan: Builder-/Reviewer-, Convergence- und Compute-Governance konsolidieren

## Status

`PLAN_ONLY_OWNER_APPROVAL_PENDING`

Dies ist die vollständige versionierte Planfassung für Issue #145. Der Plan
ist erst nach ausdrücklicher Ownerfreigabe dieser exakten Commit-SHA eine
Umsetzungserlaubnis. Der Draft-PR bleibt bis zum Owner-Schritt Draft.

## Ziel

Die bestehende Governance beschreibt den bereits gelebten Workflow als eine
widerspruchsfreie Single Source of Truth:

- Codex bzw. der ausführende Repository-Agent ist der Builder;
- der formale Full Review erfolgt grundsätzlich unabhängig über den externen
  Owner-/Reviewer-Kanal;
- lokale Korrekturen werden mit Fix Verification und dem erforderlichen
  Regression Check abgeschlossen, ohne bei begrenzter Wirkung automatisch den
  vollständigen Review zu wiederholen;
- ein erfüllter Scope mit bestandenen Pflichtgates und null offenen Blockern
  kann konvergieren;
- der Builder verwendet standardmäßig den kostenbewussten projektlokalen
  Codex-Default; teurere Compute-Eskalation bleibt begründet, eng begrenzt und
  Owner-gesteuert.

Produktionslogik, produktive Tests, Build-/CI-Workflows, ADRs, Fachverträge,
Hardware- und Releaseentscheidungen werden nicht geändert.

## Verifizierte Ausgangslage und Quellen

```text
ISSUE=145
BASE_BRANCH=integration/r1-development
CONTEXT_BASELINE_SHA=87bd668e45ab71a20ceb24ce65fcb5d1440725a8
CONTEXT_HEAD_SHA=87bd668e45ab71a20ceb24ce65fcb5d1440725a8
CONTEXT_PLAN_SHA=NONE
CONTEXT_REFRESH_MODE=FULL
CONTEXT_DELTA=Live main/integration/PR/Issue/Checkout geprüft; AGENTS.md,
  docs/AGENT_WORKFLOW.md, docs/ENGINEERING_PRINCIPLES.md,
  docs/ROADMAP.md und docs/tasks/README.md gelesen
SOURCE_OF_TRUTH_CONFLICT=NONE für Issue #145; der bekannte Workflowwiderspruch
  ist in diesem Plan als Korrekturumfang abgegrenzt
```

Live geprüft wurden der saubere, zu Issue #26 gehörende Ausgangscheckout sowie
PR #143. Dieser PR bleibt vollständig außerhalb des Scopes. Der Arbeitszweig
für Issue #145 basiert stattdessen auf dem aktuellen
`integration/r1-development` und wird nicht auf den bestehenden Issue-26-PR
gestapelt. Issue #145 hat keinen fachlichen Vorgänger und ändert keine
fachliche Reihenfolge.

Die kanonischen Quellen bleiben Root-`AGENTS.md`,
`docs/AGENT_WORKFLOW.md` und `docs/ENGINEERING_PRINCIPLES.md`; die Roadmap
erhält nur einen knappen Statusverweis und keine zweite Detailbeschreibung.
Die Auftragsschablone `Agent-Auftraege/Auftrag.md` wird nicht mit diesem
Auftrag wiederholt oder geändert.

## Umfang

### Plan-/Status-Schnitt

1. Diese vollständige Plan-Datei unter `docs/tasks/` versionieren.
2. `docs/ROADMAP.md` mit einem kurzen Issue-#145-Statusverweis als begonnene
   parallele Governance-Arbeit aktualisieren. Keine Anforderungen und keine
   Detailklassifikation in die Roadmap kopieren.
3. Den Plan committen, pushen und im Draft-PR den exakten Planpfad und die
   exakte Plan-SHA ausweisen.
4. Umsetzung bis zur ausdrücklichen Ownerfreigabe genau dieser Plan-SHA
   anhalten.

### Implementierung nach Planfreigabe

Die Umsetzung bleibt auf genau diese vorhandenen Governance-/Konfigurations-
quellen und die minimal erforderliche neue Konfigurationsdatei begrenzt:

- `.codex/config.toml` neu anlegen, falls sie auf dem freigegebenen Ziel-HEAD
  nicht existiert, mit ausschließlich:

  ```toml
  model = "gpt-5.6-luna"
  model_reasoning_effort = "xhigh"
  plan_mode_reasoning_effort = "xhigh"
  model_verbosity = "low"
  review_model = "gpt-5.6-luna"
  ```

  Die Datei ist nur der kostenbewusste Builder-Default. Ein Codex-Review bleibt
  höchstens Self-Check bzw. Zusatzreview und ersetzt nicht den unabhängigen
  Reviewkanal. Keine weiteren Codex-Schlüssel ohne konkreten Bedarf.
- `docs/AGENT_WORKFLOW.md` konsolidieren, nicht durch einen parallelen Abschnitt
  ergänzen:
  - Codex/ausführender Agent als Builder mit Plan-, Umsetzungs-, Diagnose-,
    Test-, Evidence- und PR-Aufgaben;
  - angemessener Implementation Self-Check vor Übergabe und ausdrücklicher
    Halt für Owner/Independent Review;
  - unabhängiger Full Review gegen Plan, Issue, Anforderungen, ADRs,
    Fachverträge, Architektur, Correctness/Fehlerfälle, Safety, Security,
    Recovery, Tests/Evidence, SOLID/DRY/KISS, Ressourcen, Hardware, Lizenzen,
    Dokumentationskonsistenz, unbeabsichtigte Dateien, Secrets und lokale
    Pfade;
  - operative Einordnung des aktuellen normalen ChatGPT-Kanals mit GPT-5.6
    Sol als Owner-Einstellung, ohne dauerhafte Architekturabhängigkeit;
  - verbindliche Finding-Klassen `BLOCKER`, `FOLLOW-UP`, `NO-ACTION` mit
    eindeutiger Sperrwirkung nur für erfüllungs-, Vertrags-, Sicherheits-,
    Regressions-, Test-, Quality-Gate- oder Scopefehler;
  - `FOLLOW-UP` nur für reale technische Schuld, konkreten zukünftigen Nutzen,
    bekanntes Risiko oder absehbare Anforderung; rein theoretische
    Verbesserungen als `NO-ACTION` ohne neues Issue;
  - Fix Verification nach lokal begrenzten Korrekturen: alle offenen Blocker
    verifizieren, Korrekturdiff prüfen, direkt betroffene Verträge und
    Regressionen prüfen sowie materielle Scopewirkung bewerten;
  - Full Review nach der Korrektur nur bei materieller Veränderung von Scope,
    Architektur, öffentlichen Verträgen, Persistenz/Wireformat,
    Safety/Security/Recovery oder Laufzeitverhalten oder bei breitem neuem
    Diff; dieselbe Regel nach CI-Korrekturen;
  - explizites Convergence Gate bei erfülltem genehmigtem Scope, bestandenen
    Pflichttests/-nachweisen/Quality Gates und `OPEN_BLOCKERS=0`; verbleibende
    `FOLLOW-UP`/`NO-ACTION` halten den PR nicht offen;
  - Standard-Builder als projektlokale Konfiguration und proportionaler,
    begründeter, Owner-vorgeschlagener Eskalations-Gate für deutlich teurere
    Modelle, insbesondere Terra, ohne eigenmächtigen Modellwechsel oder
    Modell-Hopping; nach dem Problemumfang Rückkehr zum Standard.
- `docs/ENGINEERING_PRINCIPLES.md` um genau eine kurze allgemeine Regel zu
  Konvergenz, YAGNI und proportionalem Engineering ergänzen. Keine
  Workflowdetails und keine Modellnamen duplizieren.
- Root-`AGENTS.md` nur um die früh sichtbare Kurzregel ergänzen: ausführender
  Agent = Builder, Full Review grundsätzlich unabhängig, Halt nach Self-Check,
  Fix Verification nach begrenzter Korrektur sofern keine materielle Änderung;
  Details ausschließlich im Workflow. Bestehende Ownerrechte und Gates bleiben
  unverändert.

Keine weitere Datei, kein neuer Agentenauftrag, keine neue ADR und kein
technischer Vertrag wird erzeugt.

## Umsetzungsschnitte und Owner-Gates

Nach Planfreigabe werden die vier Zieländerungen und die Konfigurationsprüfung
als ein kleiner, nachvollziehbarer Governance-Implementierungsschnitt erstellt.
Vor dem Commit wird der projektlokale Codex-Stand erneut geprüft. Danach werden
die gezielten Nachweise ausgeführt, der Diff vollständig gegen diesen Plan
geprüft und der Builder-Self-Check dokumentiert.

Der PR bleibt Draft. Nur der Owner entscheidet über `Ready for review`, führt
den unabhängigen Full Review im normalen ChatGPT-Kanal durch, entscheidet über
Korrekturen, setzt den PR auf `Ready for review`, nimmt einen Merge vor und
schließt Issue #145. Der Agent nimmt keinen dieser Owner-Schritte vor.

## Nachweise und Konsistenzprüfungen

Nur die direkt betroffenen Dokumentations-/Konfigurationsnachweise werden
ausgeführt:

1. Prüfen, dass der freigegebene Ziel-HEAD weiterhin der erwarteten Branch- und
   Planbaseline entspricht.
2. Vor dem Konfigurationscommit mit der installierten Codex-Version
   `codex --version` und einer strikt validierenden, projektlokalen
   `codex --strict-config --cd <checkout> --help`-Ausführung nachweisen, dass
   alle fünf Schlüssel akzeptiert werden. Bei einem unbekannten Schlüssel oder
   nicht auflösbarem Modell nicht committen, sondern anhalten.
3. TOML-Syntax und die exakten fünf Konfigurationswerte prüfen; keine
   zusätzlichen projektlokalen Codex-Schlüssel zulassen.
4. `git diff --check` ausführen.
5. Mit gezielter Textprüfung sicherstellen, dass die alte Pflicht zum
   vollständigen Review nach jeder lokalen Korrektur und nach jedem CI-Fix
   nicht mehr besteht, der unabhängige Full Review vollständig bleibt, die
   drei Finding-Klassen und ihre Abgrenzung eindeutig sind und das
   Convergence-/Eskalations-Gate vorhanden ist.
6. Prüfen, dass nur Plan-/Roadmap-/Governance-/Konfigurationsdateien geändert
   wurden und kein Produktionscode, Test, Buildsystem oder CI-Workflow im Diff
   liegt.

Ein Firmware-Build, vollständiger lokaler Lauf, Hardwarelauf und Firmware-CI
sind für diese Draft-Phase nicht angeordnet und werden als `NOT_RUN` geführt.
Markdown-only ändert nicht die Pflicht zum unabhängigen Review; semantische
normative Dokumentänderungen verwerfen den bisherigen Reviewnachweis.

## Review- und Abschlusskriterien

Der Builder prüft vor Übergabe den vollständigen aktuellen Diff gegen diesen
Plan und führt den Implementation Self-Check aus. Dieser Self-Check ist kein
unabhängiger Full Review. Der Independent Reviewer prüft den gesamten
relevanten aktuellen PR vollständig gegen alle im Workflow genannten Quellen
und Dimensionen und endet nicht beim ersten Befund.

Korrekturen folgen der im konsolidierten Workflow definierten
Fix-Verification-Regel. Nur bei materieller Änderung ist ein neuer Full Review
erforderlich. Der PR kann aus Review-Sicht `GO` erhalten, sobald alle
genehmigten Acceptance Criteria erfüllt, alle erforderlichen Nachweise und
Quality Gates bestanden und `OPEN_BLOCKERS=0` erreicht sind. `FOLLOW-UP` und
`NO-ACTION` dürfen diesen Zustand nicht künstlich offenhalten.

Ergebnisstatus nach erfolgreicher Umsetzung:

```text
BUILDER_DEFAULT=gpt-5.6-luna/xhigh
INDEPENDENT_REVIEW_ROLE=DEFINED
FIX_VERIFICATION_RULE=DEFINED
CONVERGENCE_GATE=DEFINED
MODEL_ESCALATION_GATE=DEFINED
GOVERNANCE_DUPLICATION=NONE
PRODUCTION_CODE_CHANGED=NO
```

## Nicht-Ziele

- keine Firmware-, Test-, Build- oder CI-Änderung;
- keine ADR, Produkt-, Safety-, Hardware- oder Releaseentscheidung;
- kein automatisches Modellrouting oder Agent-Orchestrator;
- kein technischer Aufruf des externen ChatGPT-Reviewkanals aus Codex;
- kein automatisches `Ready for review`, Merge, Auto-Merge, Force-Push,
  Branch-Löschen oder Issue-Schließen durch den Agenten;
- keine Erweiterung des korrekten Scopes wegen optionaler Verbesserungen.
