# Plan: Agentenregeln und Regelquellen konsolidieren

## Status

`IMPLEMENTATION_COMPLETE_REVIEW_IN_PROGRESS`

Audit und Umsetzung erfolgten blockweise: prüfen, materielle Abweichungen mit
dem Owner entscheiden, freigegebenen Block umsetzen, Diff/Links/Doppelungen
prüfen und erst danach fortfahren.

Der PR bleibt Draft. GitHub-CI läuft erst nach dem Ownerwechsel auf
`Ready for review`.

## Ziel

Jede notwendige Information bleibt verfügbar, aber genau einmal und an der
fachlich zuständigen Stelle. Historische Stände, doppelte Verträge und
ritualistische Pflichtlektüre werden entfernt, ohne Safety-, Security-,
Recovery- oder Owner-Gates zu kürzen.

## Kanonische Quellenrollen

| Quelle | Rolle |
|---|---|
| Root-`AGENTS.md` | globale Pflichten, Owner-Gates und Lesematrix |
| Root-/lokale `CLAUDE.md` | reine Importbrücken mit exakt `@AGENTS.md` |
| lokale `AGENTS.md` | nur Verzeichnis-Deltas |
| `docs/ROADMAP.md` | einzige aktuelle Status- und Taskübersicht |
| `docs/IMPLEMENTATION_PLAN.md` | stabiler technischer Phasenplan ohne Live-Status |
| `docs/IMPLEMENTATION_ISSUES.md` | Epic-/Issue-/Abhängigkeitsstruktur ohne PR-Chronik |
| `docs/OPEN_POINTS.md` | nachweisgebundene Hardware-, Commissioning- und Budgetcheckliste |
| `docs/SPECIFICATION_REVIEW.md` | Dokumentationspriorität, Release-1-Basis und Zukunftsabgrenzung |
| `docs/DECISIONS.md` | zentrales ADR-Register |
| `docs/ENGINEERING_PRINCIPLES.md` | Repository-first, Kontextrefresh, SOLID, DRY, KISS, Espressif-first |
| `docs/AGENT_WORKFLOW.md` | Plan-first, Umsetzung, Review, Tests und Handover |
| `docs/CI_AND_QUALITY_GATES.md` | Testbefehle, Buildprofile, Werkzeuge und CI-Verhalten |
| Fachverträge/ADRs | vollständige fachliche und architektonische Details |
| `lib/README.md` | kurzer nicht normativer Modulindex |
| `Agent-Auftraege/Auftrag.md` | einzige allgemeine Markdown-Auftragsvorlage |
| PR, Issue, freigegebener Plan, neuester Handover | aktueller auftragsbezogener Stand |

## Ownerentscheidungen

| ID | Entscheidung |
|---|---|
| R-00 | Root-`CLAUDE.md` enthält nur `@AGENTS.md`. |
| R-01 | `docs/ROADMAP.md` wird einzige Live-Statusquelle. |
| R-02 | Notwendige Information vollständig, kompakt und nicht doppelt. |
| R-03 | `ENGINEERING_PRINCIPLES.md` ist alleinige Engineering-Regelquelle; Learnings nach Übernahme einzigartiger Inhalte entfernen. |
| R-04 | ADR-013 bleibt alleinige vollständige Modularchitektur; Root/lokale Regeln nur Kurzfassung/Deltas. |
| R-05 | Root behält fail-closed; Details bleiben in Fachquellen. OTA ist später vorgesehen, aber nicht Release 1 und wird jetzt nicht vorgebaut. |
| R-06 | Draft: nur gezielte lokale Tests, keine Firmware-CI. Vollständige CI bei `Ready for review` und späteren Pushes eines Nicht-Draft-PR. Vollständiger lokaler Lauf nur auf Owner-Anweisung. |
| R-07 | Historischen Codex-Handover und Auftragspaket entfernen; eine kompakte allgemeine Auftragsvorlage behalten. |
| R-07a | `docs/CODEX_TASK_TEMPLATE.md` entfernen. |
| R-08 | Root-Zielgrösse etwa 5–7 KB, lokale Regeln möglichst unter 20 Zeilen; bedingte Lesematrix freigegeben. |
| R-08a | Alle notwendigen lokalen `CLAUDE.md` bleiben einzeilige Importbrücken; ESP-IDF-Adapterbaum erhält lokale Regeln. |
| R-09 | README kompakt; `SPECIFICATION_PLAN.md` entfernen; `SPECIFICATION_REVIEW.md` kanonisch aktualisieren; einzigartige PR-38-Korrekturen in Fachverträge übernehmen. |
| R-09a | `PR38_REVIEW_CORRECTIONS.md` bleibt als kurzer nicht normativer Kompatibilitätshinweis für historische Links bestehen; alle Regeln liegen in aktuellen Fachquellen. |

## Freigegebene Lesematrix

| Aufgabe | Zusätzlich zu lesen |
|---|---|
| jede Aufgabe | Markdown-Auftrag, Live-Issue/PR, Roadmap, lokale `AGENTS.md` |
| Planung, Umsetzung, Review | Agent-Workflow und Engineering-Grundsätze; bei Code/Build/Tests CI-Dokument |
| Produkt/Release/Scope | Spezifikationsreview und relevante ADRs |
| Architektur/Module | ADR-Register, relevante ADRs, Modulindex, lokale Regeln |
| Bibliotheken/Komponenten | Engineering-Grundsätze, Adopt-or-build, Lizenz-/Herstellerquellen |
| Safety/Recovery/Aktoren/Persistenz | betroffene Fachverträge und Akzeptanztests |
| Hardware/Bring-up | Hardware, Open Points, bei ESP-IDF zusätzlich Upgradevertrag |
| UI/Web/Netzwerk/Fachlogik | nur direkt betroffene Fachquellen |

## Blockstatus

### A9.1 – Agenten-, Engineering- und Modulregeln

`COMPLETED`

- Root-`AGENTS.md` konsolidiert und um rund 350 Zeilen reduziert.
- `ENGINEERING_PRINCIPLES.md` verbindlich konsolidiert; einzigartige
  Kontextbaseline-/Refreshregeln übernommen.
- `ENGINEERING_LEARNINGS.md` entfernt.
- Root-/lokale Claude-Brücken auf exakt `@AGENTS.md` reduziert.
- Lokale Modulregeln auf Deltas gekürzt; falsche ESP32-Adaptererlaubnis aus
  `device_platform` entfernt.
- Neue lokale Regeln/Claude-Brücke für `device_platform_esp_idf` ergänzt.
- `lib/README.md` zum nicht normativen Index gekürzt.

### A9.2 – Workflow, CI, PR und Agent-Aufträge

`COMPLETED`

- `docs/AGENT_WORKFLOW.md` als kanonischer Ablauf ergänzt.
- `docs/CI_AND_QUALITY_GATES.md` auf Teststrategie und Ready-for-review-CI
  konsolidiert.
- Workflow ohne Push-CI; Firmwarejob nur bei Nicht-Draft-PR.
- PR-Vorlage und `Agent-Auftraege/Auftrag.md` kompakt neu aufgebaut.
- `docs/CODEX_HANDOFF.md`, `docs/CODEX_TASK_TEMPLATE.md` und historisches
  Issue-Auftragspaket entfernt.

### A9.3 – README, Spezifikation und PR-38-Korrekturen

`COMPLETED`

- README zum kurzen nicht normativen Einstieg reduziert.
- `SPECIFICATION_REVIEW.md` als kanonische Release-/Prioritätsquelle aktualisiert.
- neue `docs/ROADMAP.md` als einzige Live-Statusquelle erstellt.
- `SPECIFICATION_PLAN.md` entfernt.
- Audit-Verzeichnis als historisch gekennzeichnet.
- kritischer Persistenzfehlervertrag vollständig in
  `SYSTEM_SAFETY_AND_RECOVERY.md` verankert.
- diskriminierende Persistenz-Latch-, Transaktions- und Reset-Orakel in
  `ACCEPTANCE_TESTS.md` ergänzt.
- `PR38_REVIEW_CORRECTIONS.md` auf einen nicht normativen historischen
  Kompatibilitätshinweis reduziert.

### A9.4 – Statusquellen

`COMPLETED`

- `IMPLEMENTATION_PLAN.md` enthält nur stabile Software-, Hardware- und
  Inbetriebnahmephasen sowie Gates.
- `IMPLEMENTATION_ISSUES.md` enthält nur Epic-/Issue-/Abhängigkeitsstruktur.
- `OPEN_POINTS.md` bleibt vollständig offen, soweit kein konkreter Nachweis
  verlinkt ist; keine Annahme wurde als erledigt markiert.
- historische Audit-Roadmap auf kurzen nicht normativen Index reduziert;
  frühere Fassung bleibt in Git.
- Live-Status und aktuelle Reihenfolge stehen ausschliesslich in `ROADMAP.md`.

## Abschlussprüfungen

- [ ] vollständige Liste aller geänderten und gelöschten Dateien prüfen;
- [ ] typische Planungs-, Umsetzungs-, Review- und Handoverabläufe simulieren;
- [ ] Links, Dateinamen und gelöschte Quellen prüfen;
- [ ] Grössen- und Doppelungsprüfung;
- [ ] vollständiges Review des finalen Diffs;
- [ ] PR-Beschreibung und Roadmap auf finalen Stand bringen;
- [ ] vollständiger lokaler Lauf nur auf Owner-Anweisung;
- [ ] Owner entscheidet über die externe GitHub-Regel „1 approving review“;
- [ ] Owner setzt danach bei Bedarf auf `Ready for review`.

## Nächster Schritt

A10/A11: vollständiger Diff-, Link-, Workflow- und Doppelungsreview. Materielle
Befunde werden vor weiteren Aenderungen dem Owner vorgelegt; rein redaktionelle
oder eindeutig beschlossene Korrekturen werden direkt bereinigt.
