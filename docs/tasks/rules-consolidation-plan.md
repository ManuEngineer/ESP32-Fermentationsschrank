# Plan: Agentenregeln und Regelquellen konsolidieren

## Status

`INVENTORY_IN_PROGRESS`

Dieser PR ist zunaechst ausschliesslich ein Audit- und Plan-PR. Normative Regeln,
Loader, Workflows und Projektdokumente werden erst nach ausdruecklicher
Ownerfreigabe geaendert.

## Ziel

- Jede verbindliche Regel besitzt genau eine kanonische Quelle.
- `AGENTS.md` wird eine kompakte, agentenuebergreifende Projektverfassung und
  Lesematrix statt eines vollstaendigen Handbuchs.
- `CLAUDE.md` wird im Zielstand ausschliesslich `@AGENTS.md` laden.
- Lokale `AGENTS.md` enthalten nur bereichsspezifische Zusatzgrenzen.
- Historische, informative und normative Inhalte werden klar getrennt.
- Pflichtlekture wird auf den tatsaechlichen Auftrag begrenzt.
- Eine kurze, aktuelle Roadmap beziehungsweise Taskliste zeigt sichtbar, was
  erledigt, in Arbeit und als Naechstes vorgesehen ist.

## Nicht-Ziele

- keine neue Produkt-, Architektur-, Safety-, Security- oder
  Hardwareentscheidung;
- keine Aenderung von Produktionscode, Tests, Build oder Abhaengigkeiten;
- keine stille Aufloesung widerspruechlicher Regeln;
- keine Loeschung historischer Dokumente, bevor relevanter Inhalt nachweislich
  erhalten oder bewusst als ueberholt freigegeben wurde;
- keine Zusammenlegung dieses Audits mit einem fachlichen Implementierungs-PR.

## Verbindliche Arbeitsweise

1. Regelquellen inventarisieren.
2. Jede normative Aussage einer Themenkategorie zuordnen.
3. Doppelungen, Widersprueche, historische Aussagen und unklare Prioritaeten
   dokumentieren.
4. Bei jeder materiellen Unstimmigkeit den Owner fragen; keine stille Auswahl.
5. Pro Themenblock eine Zielquelle und geplante Bereinigung vorschlagen.
6. Erst nach Ownerentscheid die betroffenen Dateien aendern.
7. Nach jedem Schritt Plan, Entscheidungsliste und Taskstatus aktualisieren.

## Geplante Zielarchitektur

### Automatisch geladene Regeln

- `AGENTS.md`: kurze globale Pflichten, unverhandelbare Grenzen, Owner-Gates,
  Quellenprioritaet und bedingte Lesematrix.
- `CLAUDE.md`: nur `@AGENTS.md`.
- lokale `AGENTS.md`: nur Deltas zur Root-Datei fuer den jeweiligen Verzeichnisbaum.

### Bedarfsweise gelesene Quellen

- Engineering-Prinzipien;
- akzeptierte ADRs und Architekturvertraege;
- CI- und Quality-Gates;
- Hardware-, Safety-, Persistenz- und Security-Vertraege;
- freigegebener Aufgabenplan und aktueller PR-Diff.

Diese Dokumente werden nicht pauschal fuer jeden Auftrag gelesen, sondern nur
ueber eine Lesematrix in `AGENTS.md` fuer passende Aufgabentypen verlangt.

## Roadmap und laufender Arbeitsstatus

Die Roadmap soll keine zweite Spezifikation und keine Kopie aller Issues sein.
Sie soll als knapper Statusindex enthalten:

- abgeschlossene Meilensteine beziehungsweise groessere Arbeitspakete;
- aktuell aktiven PR und dessen Phase;
- naechste startbereite Arbeitspakete in geplanter Reihenfolge;
- benannte Blocker und Ownerentscheidungen;
- Links auf GitHub-Issues, PRs und kanonische Detailquellen.

Pflegepflicht:

- zu Beginn jedes neuen PR aktualisieren;
- nach jedem PR-Merge aktualisieren;
- Status nur aus dem aktuellen GitHub- und Repository-Stand ableiten;
- keine detaillierten Anforderungen oder Vertraege duplizieren.

### Offene Strukturentscheidung R-01

Es existieren bereits drei teilweise ueberlappende Statusquellen:

- `docs/IMPLEMENTATION_PLAN.md`: technische Entwicklungsreihenfolge, aber mit
  historischen Statusangaben;
- `docs/IMPLEMENTATION_ISSUES.md`: geplante Issue- und Abhaengigkeitsstruktur
  sowie Status, aktuell teilweise veraltet;
- `docs/OPEN_POINTS.md`: Hardware-, Commissioning- und Budget-Checkliste, keine
  laufende PR-/Taskuebersicht.

Zu entscheiden ist, ob:

A. `IMPLEMENTATION_ISSUES.md` bereinigt und als alleinige Roadmap/Taskliste
   weiterentwickelt wird; oder
B. eine neue knappe `docs/ROADMAP.md` als aktueller Statusindex entsteht,
   waehrend `IMPLEMENTATION_PLAN.md`, `IMPLEMENTATION_ISSUES.md` und
   `OPEN_POINTS.md` nur ihre fachlichen Detailrollen behalten.

Vorlaeufige Empfehlung: **B**, weil eine kurze Statusseite leichter aktuell zu
halten ist und die umfangreiche Abhaengigkeitsstruktur nicht bei jedem PR
umgeschrieben werden muss. Die Roadmap darf jedoch nur Links und Status fuehren,
keine Regel- oder Anforderungsduplikate.

## Audit-Arbeitspakete

- [x] A0: neuen Branch vom aktuellen `main` erstellen.
- [x] A1: Scope und Plan-first-Vorgehen festlegen.
- [ ] A2: alle automatisch oder hierarchisch geladenen Agentendateien erfassen.
- [ ] A3: normative Aussagen in `docs/` und Templates erfassen.
- [ ] A4: historische oder veraltete Statusdokumente erfassen.
- [ ] A5: Regelmatrix mit Fundstellen und vorgeschlagener kanonischer Quelle
      erstellen.
- [ ] A6: Widersprueche einzeln mit dem Owner entscheiden.
- [ ] A7: Zielgroesse und Zielstruktur von Root- und lokalen `AGENTS.md`
      freigeben.
- [ ] A8: Roadmap-Struktur und Pflegevertrag freigeben.
- [ ] A9: genehmigte Konsolidierung umsetzen.
- [ ] A10: typische Agentenablaeufe gegen die neue Struktur pruefen.
- [ ] A11: finale Groessen-, Doppelungs- und Widerspruchspruefung.

## Bisherige Inventarfunde

### I-01 Root-AGENTS

Die Root-`AGENTS.md` ist rund 18 KB gross und enthaelt neben globalen Pflichten
auch historische Projektstaende, ausfuehrliche Engineering-Erklaerungen,
Architekturdetails und einen umfangreichen Plan-first-Workflow.

### I-02 Lokale AGENTS

Es bestehen lokale Dateien fuer:

- `lib/device_platform/`;
- `lib/device_platform_test_support/`;
- `lib/fermentation_app/`.

Sie sind grundsaetzlich sinnvoll, wiederholen aber Teile von Root-`AGENTS.md`
und ADR-013.

### I-03 CLAUDE Loader

`CLAUDE.md` laedt `@AGENTS.md`, enthaelt aktuell aber noch drei zusaetzliche
Claude-spezifische Hinweise. Der Owner hat bereits entschieden, dass der
Zielstand ein reiner Loader sein soll.

### I-04 Engineering-Regeln

SOLID, DRY, KISS, Repository-first und Kontextaktualisierung sind teilweise in
Root-`AGENTS.md`, `ENGINEERING_PRINCIPLES.md` und `ENGINEERING_LEARNINGS.md`
parallel beschrieben.

### I-05 Architektur

Modulrollen und Abhaengigkeitsrichtung sind in Root-`AGENTS.md`, ADR-013 und den
lokalen `AGENTS.md` mehrfach formuliert.

### I-06 Historische Uebergabe

`docs/CODEX_HANDOFF.md` beschreibt einen fruehen Projektstand mit PR #38 und
Issue #9 und wiederholt zusaetzlich Architektur-, Safety- und Hardwareregeln.

### I-07 Statusdokumente

`IMPLEMENTATION_PLAN.md` und `IMPLEMENTATION_ISSUES.md` enthalten bereits
veraltete Aussagen zu fruehen Issues und Draft-PRs. Eine zentrale, konsequent
aktualisierte Statusquelle fehlt.

## Ownerentscheidungen

| ID | Thema | Status | Entscheidung |
|---|---|---|---|
| R-00 | `CLAUDE.md` im Zielstand | entschieden | nur `@AGENTS.md` |
| R-01 | kuenftige Roadmap-/Tasklistenquelle | offen | Variante A oder B |

## Abnahmekriterien des Gesamtvorhabens

- jede verbindliche Regel besitzt eine kanonische Quelle;
- keine widerspruechlichen Regelkopien verbleiben;
- historische Projektstaende sind nicht Teil automatisch geladener Regeln;
- `CLAUDE.md` ist ein reiner Loader;
- lokale `AGENTS.md` enthalten nur lokale Zusatzgrenzen;
- eine bedingte Lesematrix verhindert unnoetige Pflichtlekture;
- Safety-, Security-, Recovery- und Owner-Gates bleiben vollstaendig erhalten;
- Roadmap/Taskliste zeigt aktuellen Stand, naechste Schritte und Blocker, ohne
  Anforderungen zu duplizieren;
- der Pflegepunkt fuer Roadmap und Handover ist im PR-Workflow eindeutig;
- typische Claude- und Codex-Aufgaben koennen ohne alten Chatverlauf korrekt
  gestartet, umgesetzt und reviewed werden.

## Planfreigabe

Die Inventur darf fortgesetzt und dieser Plan iterativ ergaenzt werden.
Normative Dateien werden erst geaendert, wenn die zugehoerigen
Ownerentscheidungen dokumentiert und der Umsetzungsstand ausdruecklich
freigegeben wurde.
