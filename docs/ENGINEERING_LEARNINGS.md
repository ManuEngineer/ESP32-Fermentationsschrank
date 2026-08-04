# Engineering Learnings

Diese Datei dokumentiert wiederverwendbare Erkenntnisse aus Planung,
Implementierung und Review. Verbindliche Regeln werden nicht nur hier abgelegt,
sondern in den durch `AGENTS.md` verbindlich gemachten Projektdokumenten
verankert.

## 2026-08-04 – Repository zuerst, keine Parallelwahrheit

### Beobachtung

Ein Plan oder Agentenauftrag kann trotz guter Absicht falsch werden, wenn er
bestehende Modelle, APIs, Tests und Architekturvertraege nochmals ausformuliert
oder durch neue Annahmen ersetzt. Lange Nachtraege fuehren leicht zu doppelten
Vertraegen, Widerspruechen und unnötiger Komplexitaet.

### Learning

Bestehender Code, bestehende Modelle, Tests, akzeptierte ADRs und kanonische
Dokumente sind immer zuerst die Quelle der Wahrheit.

Daraus folgt:

- aktuellen Repository-Stand vor Planung, Umsetzung und Review pruefen;
- vorhandene Typen, APIs, Validierungen und Architekturgrenzen wiederverwenden;
- keine Ersatzmodelle, Parallelvertraege oder erfundenen Abstraktionen bauen;
- bestehende Logik nicht im Auftrag duplizieren, wenn ein eindeutiger Verweis
  genuegt;
- nur nachweisliche Luecken planen oder implementieren;
- Widersprueche offen benennen und nicht still aufloesen;
- neue fachliche, sicherheitsrelevante oder persistente Entscheidungen dem Owner
  vorlegen;
- SOLID, DRY und KISS auch auf Plaene, Auftraege und Reviews anwenden.

## Effiziente Kontextaktualisierung

Die Regel "Repository zuerst" bedeutet nicht, dass bei jeder Folgeaenderung das
gesamte Repository erneut gelesen werden muss.

### Kontextbaseline

Nach einer vollstaendigen Erstorientierung dokumentiert der Agent mindestens:

```text
CONTEXT_BASELINE_BRANCH: <branch>
CONTEXT_BASELINE_SHA: <sha>
CONTEXT_PLAN_SHA: <sha oder NONE>
CONTEXT_REFRESH_MODE: FULL
```

Diese Baseline gilt nur fuer denselben Repository-, Branch- und Aufgabenbezug.

### Kontext wiederverwenden

Ist der Branch identisch, der Arbeitsbaum erwartungsgemaess und `HEAD` weiterhin
exakt die bekannte Baseline, wird der bereits gelesene Kontext wiederverwendet.
Unveraenderte Dateien werden nicht nur aus Ritual erneut vollstaendig gelesen.

### Inkrementelle Aktualisierung

Hat sich `HEAD` seit der bekannten Baseline geaendert, beginnt die Aktualisierung
mit:

```bash
git diff --name-status <baseline-sha>..HEAD
git diff --stat <baseline-sha>..HEAD
git log --oneline <baseline-sha>..HEAD
```

Danach werden gezielt gelesen:

1. alle geaenderten Dateien;
2. die fuer diese Dateien geltenden `AGENTS.md`;
3. direkt betroffene oeffentliche Schnittstellen, Modelle und Persistenzvertraege;
4. zugehoerige Tests und Architekturguards;
5. kanonische Dokumente nur dann erneut, wenn sie geaendert wurden oder der Diff
   ihre Grenzen beruehrt.

Nicht jede Include-Datei und nicht das gesamte Repository gehoeren automatisch
zum Aktualisierungsumfang. Die relevante Abhaengigkeits- und Vertragsumgebung
muss jedoch vollstaendig abgedeckt sein.

### Vollstaendige Neuorientierung

Ein Full Refresh ist erforderlich bei:

- unbekannter oder nicht belegbarer Baseline;
- Branchwechsel, Rebase, Merge oder stark veraenderter Commitbasis;
- Aenderungen an `AGENTS.md`, akzeptierten ADRs, Architektur- oder
  Engineering-Grundsaetzen;
- Aenderungen an Buildsystem, Toolchain oder Modulgrenzen;
- neuen oder veraenderten oeffentlichen APIs, Wireformaten, persistenten
  Schemata oder Abhaengigkeitsrichtungen;
- Security-, Safety-, Recovery- oder Hardwaregrenzen;
- breitem oder schwer ueberschaubarem Diff;
- Widerspruch zwischen bisherigem Kontext und aktuellem Repository;
- begruendetem Zweifel, dass die inkrementelle Sicht vollstaendig ist.

### Abschlussnachweis

Der Agent dokumentiert bei Folgearbeiten:

```text
CONTEXT_BASELINE_SHA: <vorher gepruefter sha>
CONTEXT_HEAD_SHA: <aktueller sha>
CONTEXT_REFRESH_MODE: REUSED | INCREMENTAL | FULL
CONTEXT_DELTA: <gepruefte Commits/Dateien>
SOURCE_OF_TRUTH_CONFLICT: NONE | <Beschreibung>
```

Damit bleibt nachvollziehbar, welche Quellen aktuell geprueft wurden, ohne
Credits durch unnoetiges wiederholtes Komplettlesen zu verbrauchen.

## Praktische Grenze

Eine neue Agentensitzung besitzt nicht automatisch den vollstaendigen Kontext
einer frueheren Sitzung. Deshalb muessen Branch, Baseline-SHA, freigegebener
Plan-SHA und offene Befunde im PR, Auftrag oder Uebergabeblock nachvollziehbar
sein. Das ersetzt keinen notwendigen Kontextcheck, ermoeglicht aber einen
gezielten Diff-basierten Wiedereinstieg statt eines blinden Vollscans.
