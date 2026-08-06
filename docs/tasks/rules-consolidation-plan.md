# Plan: Agentenregeln und Regelquellen konsolidieren

## Status

`TARGET_STRUCTURE_REVIEW`

Der Draft-PR inventarisiert, entscheidet und plant zuerst. Normative Regeln,
Statusdokumente und Workflows werden erst nach Freigabe der vollständigen
Zielstruktur geändert.

## Ziel

Jede notwendige Information bleibt verfügbar, aber genau einmal und an der
fachlich zuständigen Stelle.

- keine doppelten Regeln, Verträge oder Statusangaben;
- keine historischen Projektstände in automatisch geladenen Dateien;
- kurze Verweise statt kopierter Detailverträge;
- Pflichtlektüre nur für den betroffenen Aufgabentyp;
- keine Kürzung von Safety-, Security-, Recovery- oder Owner-Gates.

## Beschlossene Quellenrollen

| Quelle | Rolle |
|---|---|
| `AGENTS.md` | kompakte globale Pflichten, Owner-Gates, Quellenzugang und Lesematrix |
| `CLAUDE.md` | ausschliesslich `@AGENTS.md` |
| lokale `AGENTS.md` | nur zusätzliche Regeln des jeweiligen Verzeichnisbaums |
| `docs/ROADMAP.md` | einzige aktuelle Status- und Taskübersicht |
| `docs/IMPLEMENTATION_PLAN.md` | stabiler technischer Phasenplan ohne Live-Status |
| `docs/IMPLEMENTATION_ISSUES.md` | Issue-/Abhängigkeitsstruktur ohne PR-Chronik |
| `docs/OPEN_POINTS.md` | nachweisgebundene Hardware-, Commissioning- und Budgetcheckliste |
| `docs/SPECIFICATION_REVIEW.md` | Dokumentationspriorität, Release-1-Basis und vollständige Release-1-Abgrenzung |
| `docs/DECISIONS.md` | zentrales ADR-Register |
| ausführliche ADRs | notwendige Detailverträge komplexer Architekturentscheidungen |
| `docs/ENGINEERING_PRINCIPLES.md` | Repository-first, Kontextaktualisierung, SOLID, DRY, KISS und Espressif-first |
| `docs/AGENT_WORKFLOW.md` | Plan-first, Freigabe, Umsetzung, Abweichung, Review, Tests und Handover |
| `docs/CI_AND_QUALITY_GATES.md` | Testbefehle, Buildprofile, Werkzeuge und CI-Verhalten |
| `docs/HARDWARE.md` | konkrete Hardware, Statuskennzeichnung und Verifikationsreihenfolge |
| spezialisierte Fachdokumente | vollständige fachliche, Safety-, Recovery-, Persistenz- und UI-Verträge |
| `lib/README.md` | kurzer nicht normativer Modulindex |
| `Agent-Auftraege/Auftrag.md` | einzige allgemeine Markdown-Auftragsvorlage; nur auftragsspezifischer Inhalt und Verweise |
| PR, Issue, freigegebener Plan, neuester Handover | aktueller auftragsbezogener Arbeitsstand |

## Beschlossene Entscheidungen

| ID | Entscheidung |
|---|---|
| R-00 | `CLAUDE.md` enthält im Zielstand nur `@AGENTS.md`. |
| R-01 | `docs/ROADMAP.md` wird die einzige aktuelle Status- und Taskquelle. |
| R-02 | Notwendige Information bleibt vollständig, aber kompakt, nicht doppelt und nicht fachfremd. |
| R-03 | `ENGINEERING_PRINCIPLES.md` wird alleinige Engineering-Regelquelle; `ENGINEERING_LEARNINGS.md` wird nach Übernahme einzigartiger Inhalte entfernt. |
| R-04 | ADR-013 bleibt alleinige vollständige Modularchitektur; Root und lokale `AGENTS.md` enthalten nur Kurzfassung beziehungsweise Deltas. |
| R-05 | Root enthält nur den fail-closed-Grundsatz; Safety-, Hardware- und Release-Details bleiben in Fachquellen. OTA ist nicht Release 1, aber für ein späteres Release vorgesehen; jetzt keine vorsorglichen Ressourcen. |
| R-06 | Während Draft nur gezielte lokale Tests und keine GitHub-CI. Vollständige CI startet bei `Ready for review` und erneut bei späteren Pushes eines nicht als Draft markierten PR. Vollständiger lokaler Lauf nur auf Owner-Anweisung. |
| R-07 | `DECISIONS.md` bleibt ADR-Register. Historischen Codex-Handover und Auftragspaket #9 bis #37 entfernen. `Agent-Auftraege/Auftrag.md` kompakt neu aufbauen. |
| R-07a | `docs/CODEX_TASK_TEMPLATE.md` entfernen; keine zweite oder agentspezifische Auftragsvorlage. |

## Roadmap-Pflegevertrag

`docs/ROADMAP.md` enthält nur:

- zuletzt abgeschlossene grössere Arbeitspakete;
- aktuellen PR und Phase;
- nächste startbereite und zulässige parallele Arbeit;
- Blocker und offene Ownerentscheidungen;
- Links auf kanonische Details.

Sie wird zu Beginn jedes neuen PR, nach jedem Merge und bei materiellen
Reihenfolgeänderungen aktualisiert. Anforderungen und Issue-Inhalte werden nicht
kopiert.

## Test- und CI-Vertrag

- Planung: keine Builds oder vollständigen Tests.
- Draft-Umsetzung: nur gezielte Tests des geänderten Bereichs.
- Nach vollständigem Review: vollständiger lokaler Lauf nur auf ausdrückliche
  Owner-Anweisung und auf dem finalen Head.
- GitHub-CI: nicht während Draft; vollständiger Lauf bei `Ready for review` und
  bei späteren Pushes, solange der PR nicht Draft ist.
- Markdown-only: keine vollständige Firmware-CI.
- Nach Merge: kein identischer automatischer Wiederholungslauf.

## Zu entfernende historische oder doppelte Quellen

- `docs/CODEX_HANDOFF.md`
- `docs/CODEX_TASK_TEMPLATE.md`
- `docs/ENGINEERING_LEARNINGS.md` nach Übernahme einzigartiger Inhalte
- `Agent-Auftraege/ESP32-Fermentationsschrank_Agent-Auftraege_Issues_09-37/`

Die Historie bleibt in Git und den zugehörigen PRs erhalten.

## Bestätigte Korrekturen

- `lib/device_platform/AGENTS.md` darf keine ESP32-spezifischen Adapter erlauben;
  diese gehören nach `lib/device_platform_esp_idf/`.
- Für `lib/device_platform_esp_idf/` fehlt eine kurze lokale Delta-Regel.
- Die aktuelle PR-Vorlage und der GitHub-Workflow entsprechen nicht dem
  beschlossenen Plan-/Review-/CI-Vertrag.
- `IMPLEMENTATION_PLAN.md`, `IMPLEMENTATION_ISSUES.md`, der alte Auftragsindex
  und die Audit-Roadmap enthalten konkurrierende oder veraltete Statusstände.
- `OPEN_POINTS.md` wird nur anhand konkreter Mess-, Test- oder PR-Nachweise
  verändert; nichts wird durch Annahme abgehakt.

## Audit-Taskliste

- [x] A0: Branch und Draft-PR erstellt.
- [x] A1: Ziel, Scope und Arbeitsweise festgelegt.
- [x] A2: automatisch und hierarchisch geladene Agentendateien erfasst.
- [ ] A3: abschliessender Vollständigkeitssweep aller verlinkten Regel- und Templatequellen.
- [x] A4: zentrale veraltete Statusquellen identifiziert.
- [ ] A5: endgültige Quellen-/Lesematrix fertigstellen.
- [x] A6: bisherige materielle Widersprüche mit dem Owner entschieden.
- [ ] A7: Zielinhalt und Zielgrösse von Root- und lokalen `AGENTS.md` freigeben.
- [x] A8: Roadmap-Rolle und Pflegevertrag festgelegt.
- [ ] A9: freigegebene Konsolidierung umsetzen.
- [ ] A10: typische Planungs-, Umsetzungs-, Review- und Handoverabläufe prüfen.
- [ ] A11: finale Grössen-, Link-, Doppelungs- und Widerspruchsprüfung.

## Abnahmekriterien

- Jede verbindliche Regel und jede aktuelle Statusangabe besitzt genau eine
  kanonische Quelle.
- Root- und lokale `AGENTS.md` enthalten nur dauerhaft notwendige Pflichten.
- Die Lesematrix verhindert pauschale Volllektüre, ohne relevante Verträge
  auszulassen.
- Historische Dokumente sind keine aktuelle Handlungsquelle.
- Safety-, Security-, Recovery- und Owner-Gates bleiben vollständig.
- Claude und Codex können nach Kontextreset anhand Repository, PR, Plan und
  neuestem Handover korrekt weiterarbeiten.
- Die endgültige Struktur ist messbar kleiner und frei von semantischen
  Doppeldefinitionen.

## Nächster Schritt

R-08/A7: vollständigen Zielentwurf der Root-`AGENTS.md`, Zielgrösse und
bedingte Lesematrix mit dem Owner freigeben. Danach folgt vor der Umsetzung noch
der abschliessende Vollständigkeitssweep A3/A5.
