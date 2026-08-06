# Plan: Agentenregeln und Regelquellen konsolidieren

## Status

`INVENTORY_IN_PROGRESS`

Dieser Draft-PR inventarisiert und plant zuerst. Normative Regeln und bestehende
Statusdokumente werden erst nach der jeweiligen Ownerentscheidung geändert.

## Ziel und Leitprinzip

Jede notwendige Information bleibt verfügbar, aber genau einmal und an der
fachlich zuständigen Stelle.

- keine doppelten Regeln oder Statusangaben;
- keine historischen Projektstände in automatisch geladenen Dateien;
- keine Detailkopien in Roadmap, Aufträgen oder Handovers;
- kurze Verweise statt wiederholter Verträge;
- Pflichtlektüre nur für den betroffenen Aufgabentyp;
- Kürzung nie auf Kosten von Safety-, Security-, Recovery- oder Owner-Gates.

## Scope

- Root- und lokale `AGENTS.md`, `CLAUDE.md` und verlinkte Regelquellen;
- Workflow-, Plan-, Roadmap-, Handover- und Auftragstemplates;
- Abgrenzung zwischen normativen, informativen und historischen Dokumenten;
- eine kompakte, zuverlässig gepflegte Status-/Taskübersicht.

Nicht enthalten sind neue Produkt-, Architektur-, Hardware- oder
Implementierungsentscheidungen sowie Änderungen an Code, Tests, Build oder
Abhängigkeiten.

## Arbeitsweise

1. Quellen und normative Aussagen vollständig erfassen.
2. Doppelungen, Widersprüche und veraltete Aussagen markieren.
3. Materielle Abweichungen einzeln dem Owner vorlegen.
4. Pro Thema genau eine kanonische Quelle festlegen.
5. Erst nach Entscheidung konsolidieren oder entfernen.
6. Nach jedem Schritt Entscheidungsliste und Taskstatus aktualisieren.

## Beschlossene Zielarchitektur

| Quelle | Künftige Rolle |
|---|---|
| Root-`AGENTS.md` | kompakte globale Pflichten, unverhandelbare Grenzen, Owner-Gates, Quellenpriorität und Lesematrix |
| `CLAUDE.md` | ausschliesslich `@AGENTS.md` |
| lokale `AGENTS.md` | nur zusätzliche Regeln für den jeweiligen Verzeichnisbaum |
| `docs/ROADMAP.md` | einzige aktuelle Status- und Taskübersicht |
| `IMPLEMENTATION_PLAN.md` | stabiler technischer Phasen- und Reihenfolgeplan, ohne Live-Status |
| `IMPLEMENTATION_ISSUES.md` | Issue-, Epic- und Abhängigkeitsstruktur, ohne laufende PR-Chronik |
| `OPEN_POINTS.md` | nachweisgebundene Hardware-, Commissioning- und Budgetcheckliste |
| `docs/audits/PROPOSED_RELEASE_1_ROADMAP.md` | historischer Audit- und Begründungsnachweis, kein Live-Status |
| `docs/ENGINEERING_PRINCIPLES.md` | einzige kanonische Quelle für Repository-first, Kontextaktualisierung, SOLID, DRY, KISS und Espressif-first |
| `docs/ENGINEERING_LEARNINGS.md` | nach Übernahme einzigartiger Inhalte entfernen; Historie bleibt in Git/PR nachvollziehbar |
| `docs/DECISIONS.md` | zentrales ADR-Register; ausführliche ADRs nur bei notwendiger Detailtiefe |
| `docs/ADR-013_REUSABLE_DEVICE_PLATFORM.md` | alleinige vollständige Definition der Modulrollen und Abhängigkeitsrichtung |
| `lib/README.md` | kurzer nicht normativer Modulindex mit Verweis auf ADR-013 |
| `docs/SPECIFICATION_REVIEW.md` | Dokumentationspriorität, verbindliche Release-1-Basis und alleinige vollständige Release-1-Abgrenzung |
| `docs/HARDWARE.md` | konkrete Hardware, Statuskennzeichnung und Verifikationsreihenfolge |
| spezialisierte Safety-/Recovery-Dokumente | vollständige Fehler-, Safety-, Boot-, Persistenz- und Recovery-Verträge |
| `docs/AGENT_WORKFLOW.md` | alleinige vollständige Beschreibung von Plan-first, Planfreigabe, Umsetzung, Abweichung, Review, Abschluss, Tests und Handover |
| `docs/CI_AND_QUALITY_GATES.md` | Testbefehle, Buildprofile, Werkzeuge, CI-Schritte und PASS/FAILED/BLOCKED |
| `.github/pull_request_template.md` | kompakte Nachweisliste ohne kopierte Testbefehle oder Prozessregeln |
| `Agent-Auftraege/Auftrag.md` | kurze konkrete Auftragsdatei mit Verweisen, ohne kopierte Projekt- oder Workflowregeln |
| PR, Issue, freigegebener Plan, neuester Handover | aktueller auftragsbezogener Arbeitsstand |

Zu entfernen sind `docs/CODEX_HANDOFF.md`, das historische Auftragspaket
`Agent-Auftraege/ESP32-Fermentationsschrank_Agent-Auftraege_Issues_09-37/`
und nach Übernahme einzigartiger Inhalte `docs/ENGINEERING_LEARNINGS.md`.
Die Historie bleibt über Git und die zugehörigen PRs nachvollziehbar.

### Pflegevertrag für `docs/ROADMAP.md`

Die Roadmap enthält nur:

- zuletzt abgeschlossene grössere Arbeitspakete;
- aktuellen PR und Phase;
- nächste startbereite Arbeit;
- zulässige parallele Arbeit;
- Blocker und offene Ownerentscheidungen;
- Links auf kanonische Details.

Sie wird zu Beginn jedes neuen PR, nach jedem Merge und bei einer materiellen
Reihenfolgeänderung aktualisiert. Anforderungen, Begründungen und vollständige
Issue-Inhalte werden nicht kopiert.

### Test- und CI-Vertrag

- In der Planungsphase werden keine Builds oder vollständigen Tests ausgeführt.
- Während der Draft-Umsetzung laufen nur gezielte Tests für den geänderten
  Bereich.
- Nach vollständigem Review ohne Befund führt der Agent einen vollständigen
  lokalen Testlauf nur auf ausdrückliche Owner-Anweisung und nur auf dem finalen
  Head aus.
- GitHub-CI läuft nicht während der Draft-Phase.
- Der Owner setzt den PR nach abgeschlossenem Review auf `Ready for review`;
  dieses Ereignis startet den vollständigen GitHub-Testlauf.
- Spätere Pushes auf einen nicht mehr als Draft markierten PR lösen den Lauf
  erneut aus. Nach Rückstufung auf Draft bleibt CI wieder aus.
- Reine Markdown-Änderungen bleiben von der vollständigen CI ausgenommen.
- Kein erneuter vollständiger Lauf nach dem Merge; ein manueller Lauf bleibt für
  begründete Sonderfälle möglich.

## Audit-Taskliste

- [x] A0: Branch vom aktuellen `main` erstellt.
- [x] A1: Audit- und Plan-first-Scope festgelegt.
- [x] A2: Grundstruktur der automatisch und hierarchisch geladenen Agentendateien erfasst.
- [ ] A3: normative Aussagen in Regeln, Workflows und Templates vollständig erfassen.
- [x] A4: zentrale veraltete Statusquellen identifiziert.
- [ ] A5: Regelmatrix mit Fundstellen und kanonischer Zielquelle erstellen.
- [ ] A6: Widersprüche blockweise mit dem Owner entscheiden.
- [ ] A7: Zielinhalt und Zielgrösse von Root- und lokalen `AGENTS.md` freigeben.
- [x] A8: Roadmap-Rolle und Pflegevertrag festgelegt.
- [ ] A9: genehmigte Konsolidierung umsetzen.
- [ ] A10: typische Claude- und Codex-Abläufe gegen die neue Struktur prüfen.
- [ ] A11: finale Grössen-, Doppelungs- und Widerspruchsprüfung.

## Bestätigte Inventarfunde

- Root-`AGENTS.md` ist rund 18 KB gross und mischt globale Pflichten,
  historische Zustände, Engineering-Erklärungen, Architekturdetails und
  Workflowregeln.
- Drei lokale `AGENTS.md` sind sinnvoll, wiederholen aber Teile von Root und
  ADR-013; für `device_platform_esp_idf` fehlt eine lokale Delta-Regel.
- `lib/device_platform/AGENTS.md` erlaubt noch ESP32-spezifische Adapter, obwohl
  ADR-013, Modulindex und Architekturguard sie nach `device_platform_esp_idf`
  trennen.
- `CLAUDE.md` lädt `@AGENTS.md`, enthält aktuell zusätzlich drei
  Claude-spezifische Hinweise.
- SOLID, DRY, KISS, Repository-first und Kontextaktualisierung stehen teilweise
  parallel in Root-`AGENTS.md`, `ENGINEERING_PRINCIPLES.md` und
  `ENGINEERING_LEARNINGS.md`.
- Modulrollen und Abhängigkeitsrichtung stehen mehrfach in Root, ADR-013,
  `lib/README.md` und lokalen `AGENTS.md`.
- Root-`AGENTS.md` kopiert Hardware-, Safety- und Release-1-Listen, obwohl die
  vollständigen Verträge bereits in spezialisierten Fachquellen stehen.
- Die Release-1-Ausschlusslisten sind nicht vollständig identisch; OTA und
  automatischer Firmwaredownload sind für Release 1 ausgeschlossen, bleiben
  aber ausdrücklich für ein späteres Release vorgesehen.
- Der vollständige Plan-first-Workflow steht derzeit in der automatisch
  geladenen Root-`AGENTS.md` statt in einer bedarfsweise gelesenen Workflowquelle.
- Die PR-Vorlage enthält veraltete pauschale PlatformIO-Prüfpunkte und kennt
  Plan-Commit, gezielte Tests, Roadmap-Pflege und offene Gates nicht.
- Der aktuelle GitHub-Workflow startet bei jedem nicht reinen Markdown-Push auf
  einen Draft-PR und wiederholt den vollständigen Testlauf zusätzlich nach
  weiteren Draft-Pushes.
- `docs/CODEX_HANDOFF.md` enthält einen historischen Stand ab PR #38/Issue #9
  und wiederholt Architektur-, Safety- und Hardwareinformationen.
- Das historische Auftragspaket #9 bis #37 und sein INDEX sind veraltet und
  bilden eine zusätzliche Status- und Regelquelle.
- `Agent-Auftraege/Auftrag.md` erzwingt noch den alten INDEX-Workflow,
  Volllektüre und vollständige Tests vor jedem PR.
- `IMPLEMENTATION_PLAN.md` und `IMPLEMENTATION_ISSUES.md` enthalten veraltete
  Statusangaben.
- `docs/audits/PROPOSED_RELEASE_1_ROADMAP.md` ist derzeit faktisch die
  aktuellste Roadmap, obwohl sie die zentralen Plandokumente ausdrücklich nicht
  ersetzt.
- `OPEN_POINTS.md` muss Punkt für Punkt gegen reale Mess-, Test- oder
  PR-Nachweise geprüft werden; unbelegte Punkte werden nicht abgehakt.

## Ownerentscheidungen

| ID | Thema | Entscheidung |
|---|---|---|
| R-00 | `CLAUDE.md` | im Zielstand nur `@AGENTS.md` |
| R-01 | Live-Roadmap | neue kompakte `docs/ROADMAP.md` als einzige aktuelle Statusquelle |
| R-02 | Dokumentationsprinzip | notwendige Information vollständig, aber kompakt, nicht doppelt und nicht an fachfremden Stellen |
| R-03 | Engineering-Grundsätze | `ENGINEERING_PRINCIPLES.md` ist alleinige kanonische Regelquelle; Root verweist kurz darauf; `ENGINEERING_LEARNINGS.md` wird nach Übernahme einzigartiger Inhalte entfernt |
| R-04 | Modularchitektur | ADR-013 bleibt alleinige vollständige Quelle; Root fasst nur Kernrichtung und Lesepflicht zusammen; lokale `AGENTS.md` enthalten nur Deltas; neue kurze Delta-Regel für `device_platform_esp_idf`; `lib/README.md` wird zum knappen Index |
| R-05 | Safety, Hardware und Release-Scope | Root enthält nur fail-closed-Grundsatz und Lesepflicht; Details bleiben in den Fachquellen; `SPECIFICATION_REVIEW.md` führt die alleinige vollständige Release-1-Abgrenzung; OTA und automatischer Firmwaredownload sind nicht Teil von Release 1, aber für ein späteres Release vorgesehen; jetzt keine vorsorglichen Slots, Bibliotheken oder Ressourcen reservieren |
| R-06 | Workflow, Tests und CI | vollständiger Prozess nach `AGENT_WORKFLOW.md`; Root enthält nur Owner-Gates; während Draft nur gezielte lokale Tests und keine GitHub-CI; vollständiger lokaler Lauf nur auf Owner-Anweisung nach Review; vollständige GitHub-CI startet beim Ownerwechsel auf `Ready for review` und erneut bei späteren Pushes eines nicht als Draft markierten PR; kein automatischer Wiederholungslauf nach Merge |
| R-07 | ADRs, Handovers und Aufträge | `DECISIONS.md` bleibt zentrales ADR-Register; Dokumentationspriorität bleibt nur in `SPECIFICATION_REVIEW.md`; historischen Codex-Handover und Auftragspaket #9 bis #37 entfernen; `Agent-Auftraege/Auftrag.md` als kurze aktuelle Auftragsdatei mit Verweisen neu aufbauen |

## Abnahmekriterien

- Jede verbindliche Regel und jede aktuelle Statusangabe besitzt genau eine
  kanonische Quelle.
- Automatisch geladene Regeln enthalten nur dauerhaft notwendige Pflichten.
- Bedingte Lesematrix verhindert pauschale Pflichtlektüre.
- Lokale `AGENTS.md` enthalten nur echte lokale Deltas.
- Historische Dokumente sind klar als historisch erkennbar und keine
  Handlungsquelle.
- `docs/ROADMAP.md` ist kurz, aktuell und verweist statt zu duplizieren.
- Safety-, Security-, Recovery- und Owner-Gates bleiben vollständig erhalten.
- Claude und Codex können Planung, Umsetzung, Review und Fortsetzung nach
  Kontextreset ohne alten Chatverlauf korrekt durchführen.
- Die endgültige Fassung ist messbar kleiner und enthält keine semantischen
  Doppeldefinitionen.

## Nächster Schritt

A7: vollständigen Zielaufbau und Zielgrösse der Root-`AGENTS.md` sowie die
bedingte Lesematrix dem Owner vorlegen. Erst nach Freigabe beginnt die
Konsolidierung.
