# Plan: Agentenregeln und Regelquellen konsolidieren

## Status

`REVIEW_COMPLETE_OWNER_CONFIGURATION_PENDING`

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
| R-10 | Einzelentwickler-Workflow: Nur der Owner gibt PRs frei, setzt sie auf `Ready for review` und mergt. Agenten tun keines davon. Eine verpflichtende Freigabe durch eine zweite Person ist unpassend und muss aus der GitHub-Branch-Protection entfernt werden; PR- und CI-Gates bleiben bestehen. |

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

- Root-`AGENTS.md` auf 125 Zeilen und rund 5,9 KB konsolidiert.
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

### A10/A11 – Abschlussreview und Main-Synchronisierung

`COMPLETED`

- alle 76 geänderten beziehungsweise gelöschten Pfade geprüft;
- 40 historische oder doppelte Dateien entfernt;
- Importbrücken, lokale Regeln, Workflow, CI-Trigger, Safety-/Persistenzvertrag,
  Statusquellen, historische Indizes und Löschungen vollständig geprüft;
- PR #95 / Issue #20 aus aktuellem `main` übernommen;
- Konflikte in `AGENTS.md`, `.github/workflows/build.yml` und
  `docs/CI_AND_QUALITY_GATES.md` ohne Force-Push aufgelöst;
- echter Merge-Commit `fda29dd5be0467997e8de005881c2d02e4cb1cee` mit beiden
  Eltern erstellt;
- erstes vollständiges Review fand drei Punkte: zu breite Gültigkeit von
  Markdown-Reviewnachweisen, unklare Draft-Rückstufung und zu grosse Root-Regeln;
- alle drei Befunde korrigiert;
- zweites vollständiges Review des verbleibenden Diffs ohne neuen Befund;
- GitHub hat den Workflow erfolgreich geladen; der Lauf auf Head
  `69f369d43c784fdbe35438484f8c09d9ce5b32e9` wurde erwartungsgemäss wegen Draft
  als `skipped` abgeschlossen;
- PR #98 ist mergebar und bleibt Draft.

## Abschlussprüfungen

- [x] vollständige Liste aller 76 geänderten und gelöschten Pfade geprüft;
- [x] typische Planungs-, Umsetzungs-, Review- und Handoverabläufe simuliert;
- [x] Dateibaum, Dateinamen, Löschungen und kanonische Zielquellen über GitHub geprüft;
- [x] Grössen- und Doppelungsprüfung abgeschlossen;
- [x] zwei vollständige Reviewdurchgänge abgeschlossen;
- [x] GitHub-Workflow syntaktisch geladen und Draft-Skip bestätigt;
- [ ] lokaler Checkout-basierter Link-/YAML-/`git diff --check`-Lauf: `NOT_RUN`, da die Ausführungsumgebung den GitHub-Host nicht auflösen konnte;
- [ ] vollständiger lokaler Build-/Testlauf: `NOT_RUN`, nicht vom Owner angeordnet;
- [x] Owner bestätigt den Einzelentwickler-Workflow und verwirft die Pflicht zu einer zweiten Freigabe;
- [ ] Owner entfernt die GitHub-Regel `1 approving review` aus der Branch-Protection;
- [ ] Owner setzt den PR danach auf `Ready for review` und löst die vollständige CI aus.

## Nächster Schritt

Owner entfernt die unpassende GitHub-Regel `1 approving review` aus der
Branch-Protection für `main`. Danach setzt ausschliesslich der Owner PR #98 auf
`Ready for review`, wodurch die vollständige GitHub-CI startet. Ohne neuen
Befund sind keine weiteren Agentenänderungen vorgesehen.
