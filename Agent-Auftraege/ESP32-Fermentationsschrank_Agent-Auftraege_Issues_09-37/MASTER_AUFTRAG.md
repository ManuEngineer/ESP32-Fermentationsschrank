# Allgemeiner Kurzauftrag

Diese Kurzfassung eignet sich nur, wenn der Agent bereits zuverlaessig die Repository-Regeln kennt. Fuer produktive Arbeit ist die jeweilige `Issue#XX.md` vorzuziehen.

```text
Arbeite im Repository `ManuEngineer/ESP32-Fermentationsschrank` ausschliesslich am angegebenen GitHub-Issue.

Pruefe zuerst Live-Status, Abhaengigkeiten, aktuellen `main`, sauberen Git-Status, `AGENTS.md`, untergeordnete `AGENTS.md`, `docs/SPECIFICATION_REVIEW.md`, `docs/DECISIONS.md` und alle Issue-Quellen. Beginne nur bei freigegebenem Status und abgeschlossenen Abhaengigkeiten.

Lege vor Codeaenderungen eine Bestandsanalyse, einen konkreten Dateiplan, Tests, Risiken und offene Entscheidungen vor. Bei Architektur-, Sicherheits- oder Spezifikationskonflikten anhalten und fragen.

Erstelle danach einen eigenen Branch vom aktuellen `main`, arbeite nur im Issue-Scope, halte Hardwarewerte unbestaetigt, fuehre alle relevanten Builds/Tests/Analysen aus, aktualisiere Dokumentation und Changelog und erstelle einen kleinen PR gegen `main`. Verwende `Closes #XX` nur bei vollstaendiger Definition of Done. Merge den PR nicht selbst.
```
