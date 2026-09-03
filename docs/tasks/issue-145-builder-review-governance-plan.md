# Plan: Builder-/Reviewer-, Convergence- und Compute-Governance konsolidieren

## Status

`PLAN_REVISION_OWNER_APPROVAL_PENDING`

Dies ist die vollständige revidierte Planfassung für Issue #145. Sie revidiert
den freigegebenen Plan-Commit
`de6f5ddef42324244d9d4bba4362ed6e0cc1580f` ausschließlich um die nach dem
Independent Full Review erforderliche Governance-/Statuskonsistenz. Diese
Planrevision ist erst nach ausdrücklicher Ownerfreigabe ihrer exakten Commit-
SHA eine Umsetzungserlaubnis. Der Draft-PR bleibt bis zum Owner-Schritt Draft.

Die bisherige Implementation auf `f8f8f963eca68ee346d693cfb1656e39bf7a9962`
bleibt die verifizierte Ausgangsbasis. Die Korrekturimplementation dieses
revidierten Plans ist bis zur neuen Ownerfreigabe nicht autorisiert.

## Planrevision nach Independent Full Review

Der unabhängige Full Review des aktuellen PR-HEAD hat offene Review-Blocker
festgestellt. Die Korrektur ist auf die dafür notwendige Konsistenz begrenzt:

- `docs/CI_AND_QUALITY_GATES.md` wird an `OPEN_BLOCKERS=0`, die Finding-Klassen
  und die Fix-Verification-/Materialitätsregel angeglichen;
- die gleichbedeutenden widersprüchlichen Reviewnachweis-Formulierungen in
  Root-`AGENTS.md` und `docs/AGENT_WORKFLOW.md` werden präzisiert;
- `docs/ROADMAP.md` erhält den tatsächlichen Issue-#145-Status von freigegebenem
  Plan und abgeschlossener Implementation mit offenen Review-Blockern.

Es werden keine weiteren Scope-, Governance-, Produkt-, Test-, Build-, CI-,
Safety-, Hardware- oder Releaseänderungen aufgenommen.

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
CONTEXT_HEAD_SHA=f8f8f963eca68ee346d693cfb1656e39bf7a9962
CONTEXT_PLAN_SHA=de6f5ddef42324244d9d4bba4362ed6e0cc1580f
CONTEXT_REFRESH_MODE=INCREMENTAL
CONTEXT_DELTA=Live main/integration/PR/Issue/Checkout, Erstimplementation und
  Independent-Review-Stand geprüft; AGENTS.md, docs/AGENT_WORKFLOW.md,
  docs/CI_AND_QUALITY_GATES.md, docs/ENGINEERING_PRINCIPLES.md,
  docs/ROADMAP.md und der freigegebene Plan gezielt gelesen
SOURCE_OF_TRUTH_CONFLICT=B1: drei veraltete CI-/Reviewregeln und zusätzlich
  gleichbedeutende Reviewnachweis-Formulierungen in Root/Workflow; B2: veralteter
  Roadmap-Status. Die Korrekturen sind in diesem Plan abgegrenzt.
```

Live geprüft wurden PR #146, Issue #145, der aktuelle Branch
`docs/issue-145-builder-review-governance`, der Implementierungs-HEAD
`f8f8f963eca68ee346d693cfb1656e39bf7a9962` und der einzige aktuelle Handover.
Der PR bleibt Draft; Owner-Review, Ready, CI, Merge und Issue-Abschluss bleiben
außerhalb der Agentenrechte.

Die kanonischen Governancequellen bleiben Root-`AGENTS.md`,
`docs/AGENT_WORKFLOW.md` und `docs/ENGINEERING_PRINCIPLES.md`;
`docs/CI_AND_QUALITY_GATES.md` bleibt die kanonische Quelle für Befehle und
Quality-Gates. Die Roadmap erhält nur einen knappen Statusverweis und keine
zweite Detailbeschreibung.
Die Auftragsschablone `Agent-Auftraege/Auftrag.md` wird nicht mit diesem
Auftrag wiederholt oder geändert.

## Umfang

### Plan-/Status-Schnitt

1. Diese vollständige revidierte Plan-Datei unter `docs/tasks/` versionieren.
2. Den revidierten Plan committen, pushen und im Draft-PR den exakten Planpfad
   und die neue exakte Plan-SHA ausweisen.
3. Umsetzung bis zur ausdrücklichen Ownerfreigabe genau dieser neuen Plan-SHA
   anhalten.

### Implementierung nach Planfreigabe

Die Korrekturimplementation bleibt nach Planfreigabe auf genau diese vier
vorhandenen Governance-/Statusquellen begrenzt. `.codex/config.toml` und
`docs/ENGINEERING_PRINCIPLES.md` werden in dieser Korrekturrunde nicht geändert:

- `docs/CI_AND_QUALITY_GATES.md`:
  - den Gate für den vollständigen lokalen Lauf auf abgeschlossenes
    Independent Full Review mit `OPEN_BLOCKERS=0`, finalem `HEAD` und
    ausdrücklicher Owner-Anordnung ausrichten;
  - klarstellen, dass `FOLLOW-UP` und `NO-ACTION` diesen Lauf nicht blockieren;
  - die pauschale Regel ersetzen, nach jeder semantischen Änderung erneut einen
    vollständigen Review zu verlangen: lokal begrenzte Korrektur bedeutet Fix
    Verification plus erforderlichen Regression Check, materielle Änderung
    oder breiter neuer Diff bedeutet neuer Full Review;
  - nach CI-Korrekturen dieselbe Fix-Verification-/Materialitätsregel anwenden.
- `docs/AGENT_WORKFLOW.md`:
  - die Formulierung zum vollständigen lokalen Lauf auf abgeschlossenes Review
    mit `OPEN_BLOCKERS=0` präzisieren;
  - die pauschale Verwerfung des Reviewnachweises bei jeder semantischen
    Änderung durch die bestehende Fix-Verification-/Materialitätsregel
    ersetzen. Die bereits definierte vollständige Independent-Review-Prüfung,
    Finding-Klassifikation, Convergence Gate und CI-Korrekturregel bleiben
    ansonsten unverändert.
- Root-`AGENTS.md`:
  - die Formulierung „Review ohne offene Befunde“ auf abgeschlossenes
    Independent Full Review mit `OPEN_BLOCKERS=0` präzisieren;
  - den Reviewnachweis bei semantischen Änderungen nur gemäß der im Workflow
    definierten Fix-Verification-/Materialitätsregel verwerfen, ohne die
    Detailklassifikation zu duplizieren.
- `docs/ROADMAP.md`:
  - die bestehende Issue-#145-Zeile knapp auf
    `PLAN_APPROVED=YES`, `IMPLEMENTATION=COMPLETE`,
    `INDEPENDENT_REVIEW=OPEN_BLOCKERS` und `PRODUCTION_CODE_CHANGE=NO`
    aktualisieren;
  - als nächstes Gate ausschließlich die Ownerfreigabe dieser Planrevision
    ausweisen. Keine Anforderungen und keine Workflowdetails kopieren.

Keine weitere Datei, kein neuer Agentenauftrag, keine neue ADR und kein
technischer Vertrag wird erzeugt. `.codex/config.toml` und
`docs/ENGINEERING_PRINCIPLES.md` bleiben gegenüber dem Implementierungs-HEAD
unverändert.

## Umsetzungsschnitte und Owner-Gates

Nach Planfreigabe werden die vier Korrekturdateien als ein kleiner,
nachvollziehbarer Governance-/Status-Implementierungsschnitt aktualisiert. Vor
dem Commit werden die gezielten Text- und Diff-Nachweise ausgeführt, der Diff
vollständig gegen diesen Plan geprüft und der Builder-Self-Check dokumentiert.

Der PR bleibt Draft. Nur der Owner entscheidet über `Ready for review`, führt
den unabhängigen Full Review im normalen ChatGPT-Kanal durch, entscheidet über
Korrekturen, setzt den PR auf `Ready for review`, nimmt einen Merge vor und
schließt Issue #145. Der Agent nimmt keinen dieser Owner-Schritte vor.

## Nachweise und Konsistenzprüfungen

Nur die direkt betroffenen Dokumentations-/Statusnachweise werden ausgeführt:

1. Prüfen, dass der aktuelle Implementierungs-HEAD, die Branch, der PR-Status
   und der freigegebene Plan-Commit weiterhin der Ausgangslage entsprechen.
   Die Korrekturimplementation darf erst nach Ownerfreigabe der neuen exakten
   Plan-SHA beginnen.
2. In `docs/CI_AND_QUALITY_GATES.md` gezielt prüfen, dass der vollständige
   lokale Lauf nur nach abgeschlossenem Independent Full Review mit
   `OPEN_BLOCKERS=0`, finalem `HEAD` und ausdrücklicher Owner-Anordnung
   zulässig ist und dass `FOLLOW-UP` sowie `NO-ACTION` diesen Lauf nicht
   blockieren.
3. In `docs/CI_AND_QUALITY_GATES.md` gezielt prüfen, dass semantische
   Änderungen nach Materialität behandelt werden: lokal begrenzte Korrektur
   führt zu Fix Verification plus erforderlichem Regression Check; materielle
   Änderung oder breiter neuer Diff führt zu neuem Full Review. Nach
   CI-Korrekturen muss dieselbe Regel ausdrücklich gelten.
4. In Root-`AGENTS.md` und `docs/AGENT_WORKFLOW.md` gezielt prüfen, dass keine
   Formulierung „Review ohne offene Befunde“ als eigenes Gate verbleibt,
   sondern der vollständige lokale Lauf an abgeschlossenes Review mit
   `OPEN_BLOCKERS=0` gebunden ist. Die Workflowregel darf weiterhin weder
   lokale Korrekturen pauschal zum Full Review hochstufen noch den vollständigen
   Independent Review verkürzen.
5. Den vollständigen Independent-Review-Abschnitt, die drei Finding-Klassen,
   das Convergence Gate und die Compute-Eskalationsregel auf unbeabsichtigte
   Widersprüche prüfen. Der Full Review muss vollständig bleiben und darf
   keine Salamitaktik ermöglichen.
6. Die kanonischen Workflow-/CI-Dokumente gezielt nach der alten pauschalen
   Full-Review-Wiederholung und der alten „keine offenen Befunde“-Sperre
   durchsuchen. Es darf keine weitere kanonische Workflow-/CI-Datei mit dem
   alten aktuellen Vertrag verbleiben; historische Plantexte werden nicht als
   aktuelle Regel interpretiert.
7. Prüfen, dass `docs/ROADMAP.md` die bestehende Issue-#145-Zeile knapp mit
   `PLAN_APPROVED=YES`, `IMPLEMENTATION=COMPLETE`,
   `INDEPENDENT_REVIEW=OPEN_BLOCKERS` und `PRODUCTION_CODE_CHANGE=NO` führt
   und als nächstes Gate nur die Ownerfreigabe dieser Planrevision nennt.
8. `git diff --check` sowie eine Dateiliste ausführen. Es dürfen nur
   `docs/tasks/issue-145-builder-review-governance-plan.md`,
   `docs/CI_AND_QUALITY_GATES.md`, `docs/AGENT_WORKFLOW.md`, Root-`AGENTS.md`
   und `docs/ROADMAP.md` geändert sein. Produktionscode, Tests, Buildsystem,
   CI-Workflow, `.codex/config.toml`, Engineering Principles, ADRs und
   Fachverträge bleiben unverändert.

Die Korrekturkonsistenz wird im Implementierungs- und Handover-Evidence
getrennt festgehalten:

```text
FULL_REVIEW_GATE=FULL_REVIEW_COMPLETE+OPEN_BLOCKERS=0
FOLLOW_UP_NO_ACTION_BLOCKING=NO
LOCAL_CORRECTION=FIX_VERIFICATION+REGRESSION_CHECK
MATERIAL_CHANGE_OR_BROAD_DIFF=NEW_FULL_REVIEW
CI_CORRECTION=SAME_FIX_VERIFICATION_MATERIALITY_RULE
ROADMAP_STATUS=PLAN_APPROVED+IMPLEMENTATION_COMPLETE+OPEN_BLOCKERS
PRODUCTION_CODE_CHANGED=NO
```

Ein Firmware-Build, vollständiger lokaler Lauf, Hardwarelauf und Firmware-CI
sind für diese Draft-Phase nicht angeordnet und werden als `NOT_RUN` geführt.
Nach der Korrektur bleibt der PR bis zum unabhängigen Review und den
unveränderten Owner-Gates Draft.

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

- keine Firmware-, Test-, Build- oder CI-Workflow-Änderung; die notwendige
  Dokumentationskorrektur in `docs/CI_AND_QUALITY_GATES.md` ist der einzige
  zusätzliche Konsistenzumfang;
- keine ADR, Produkt-, Safety-, Hardware- oder Releaseentscheidung;
- kein automatisches Modellrouting oder Agent-Orchestrator;
- kein technischer Aufruf des externen ChatGPT-Reviewkanals aus Codex;
- kein automatisches `Ready for review`, Merge, Auto-Merge, Force-Push,
  Branch-Löschen oder Issue-Schließen durch den Agenten;
- keine Erweiterung des korrekten Scopes wegen optionaler Verbesserungen.
