# Agent-Workflow

Dieses Dokument ist die kanonische Arbeitsweise fuer Planung, Umsetzung,
Review, Tests und Handover. Die unverhandelbaren Owner-Gates stehen zusaetzlich
in der Root-`AGENTS.md`.

## 1. Aufgabe und Live-Stand klaeren

Jeder Auftrag liegt als Markdown-Datei vor. Vor der Arbeit werden mindestens
geprueft:

- Repository, Branch, aktueller `HEAD` und Arbeitsbaum;
- Live-Issue, vorhandener Pull Request und Abhaengigkeiten;
- `docs/ROADMAP.md`;
- geltende Root- und lokale `AGENTS.md`;
- freigegebener Plan-Commit und neuester `SESSION HANDOVER`, sofern vorhanden;
- die laut Lesematrix und Aufgabe konkret betroffenen Fachquellen.

Die Kontextpruefung folgt `ENGINEERING_PRINCIPLES.md`. Widersprueche,
fehlende Entscheidungen und Scopeabweichungen werden sichtbar gemacht.

## 2. Triviale Ausnahme

Ohne eigenen Plan-Commit darf nur eine ausdruecklich freigegebene triviale
Aenderung erfolgen. Sie muss lokal begrenzt, reversibel und frei von
Produktionslogik, Architektur-, API-, Schema-, Persistenz-, Safety-, Security-,
Hardware-, Bibliotheks-, Build- oder Toolchainwirkung sein.

Sobald eine dieser Grenzen beruehrt wird oder Zweifel bestehen, gilt der
Plan-first-Workflow.

## 3. Planungsphase

Fuer nicht triviale Arbeit werden ein Branch und ein Draft-PR erstellt. Der
versionierte Plan liegt unter `docs/tasks/` und enthaelt mindestens:

- Ziel und Nicht-Ziele;
- Live-Ausgangslage und Kontextbaseline;
- verbindliche Quellen und bereits getroffene Entscheidungen;
- betroffene Module und voraussichtliche Dateien;
- Abhaengigkeiten, Blocker und Owner-Gates;
- Daten-, Zustands-, Schnittstellen- und Abhaengigkeitsvertraege;
- Safety-, Security-, Recovery- und Hardwaregrenzen, soweit betroffen;
- kleine Umsetzungs- und Commit-Schnitte;
- gezielte Tests und Nachweise;
- Dokumentations- und Roadmapwirkung;
- offene Fragen und materielle Risiken.

Jede freigegebene Planrevision ist ein vollstaendiges und eigenstaendig
ausfuehrbares Dokument. Historische Revisionen duerfen zur Nachvollziehbarkeit
referenziert werden, ersetzen aber keine fuer die Umsetzung notwendigen
Inhalte. Eine aktuelle Revision darf nicht voraussetzen, dass ein Agent
mehrere alte Revisionen zusammensetzt. Findet ein Agent einen nicht
konsolidierten Plan vor, rekonstruiert er ihn nicht aus den historischen
Revisionen, sondern meldet die fehlende Plan-Konsolidierung als Blocker.

Ein freigegebener Plan enthaelt ausserdem eine nachvollziehbare Struktur fuer
die Abarbeitung. Dafuer genuegen je nach Umfang kleine Umsetzungs- und
Commit-Schnitte, nummerierte Schritte, eine Checkliste oder eine vergleichbare
Struktur. Eine zusaetzliche starre Taskliste ist nicht erforderlich, wenn die
bestehende Planstruktur eine eigenstaendige und nachvollziehbare Umsetzung
ermoeglicht.

In der Planungsphase werden ohne ausdrueckliche Ownerfreigabe keine
Produktionslogik, produktiven Tests, Abhaengigkeiten, ADRs, Build-, Toolchain-,
Hardware-, GPIO- oder Bibliotheksentscheidungen umgesetzt.

Nach dem Plan-Commit werden Planpfad, exakte Plan-SHA und offene Entscheidungen
im Draft-PR ausgewiesen. Danach wird angehalten.

## 4. Planfreigabe

Umgesetzt wird nur nach ausdruecklicher Ownerfreigabe des exakten
Plan-Commits. Eine allgemeine Zustimmung ohne eindeutige Plan-SHA ersetzt dieses
Gate nicht.

Vor der Umsetzung werden Branch, `HEAD`, Plan-SHA und seit der Freigabe
eingegangene Aenderungen erneut geprueft. Bei veraenderter Grundlage wird der
Kontext inkrementell oder vollstaendig aktualisiert.

## 5. Umsetzung

Die Umsetzung folgt dem freigegebenen Plan in kleinen, pruefbaren Schnitten.

- Bestehende Modelle, APIs, Tests und ADRs werden wiederverwendet.
- Es werden keine Parallelvertraege oder vorsorglichen Erweiterungen erzeugt.
- Nach jedem Schnitt werden die gezielten Tests des betroffenen Bereichs
  ausgefuehrt.
- Bei gemeinsamen Vertraegen werden zusaetzlich die direkt betroffenen
  Konsumententests ausgefuehrt.
- Plan, PR-Beschreibung und `docs/ROADMAP.md` werden nur bei tatsaechlicher
  Status- oder Scopewirkung aktualisiert.
- Nicht ausgefuehrte Nachweise werden nicht als bestanden bezeichnet.

## 6. Materielle Abweichung

Materiell sind insbesondere Aenderungen an Scope, Architektur, oeffentlichen
APIs, Modulgrenzen, Abhaengigkeiten, Persistenz oder Wireformat, Safety,
Security, Recovery, Hardware, Bibliotheksauswahl, Build/Toolchain,
Akzeptanzkriterien oder Teststrategie.

Bei einer materiellen Abweichung:

1. Umsetzung anhalten;
2. Befund und Auswirkung dokumentieren;
3. Plan aktualisieren und committen;
4. neue exakte Plan-SHA vorlegen;
5. erneute Ownerfreigabe abwarten.

Kleine redaktionelle oder rein mechanische Anpassungen innerhalb des
freigegebenen Vertrags werden im Plan-/PR-Nachweis dokumentiert, ohne einen
Parallelvertrag zu erzeugen.

## 7. Vollstaendiges Review

Vor dem Abschluss wird der gesamte aktuelle Diff gegen freigegebenen Plan,
Issue, Anforderungen, relevante ADRs, Fachvertraege, Tests und Dokumentation
geprueft.

Das Review umfasst mindestens:

- funktionale Vollstaendigkeit und Fehlerfaelle;
- Safety-, Security- und Recoverygrenzen;
- Architektur- und Abhaengigkeitsrichtung;
- SOLID, DRY und KISS gegen den tatsaechlichen Diff;
- Testorakel, fehlende Grenzfaelle und nicht ausgefuehrte Nachweise;
- Ressourcen-, Hardware- und Lizenzwirkung;
- veraltete, doppelte oder widerspruechliche Dokumentation;
- Geheimnisse, lokale Pfade und unbeabsichtigte Dateien.

Ein Review endet nicht nach den auffaelligsten Befunden. Nach Korrekturen wird
der vollstaendige verbleibende Diff erneut geprueft.

## 8. Tests und GitHub-CI

Waehrend der Draft-Phase werden nur passende gezielte lokale Tests ausgefuehrt.
Die konkreten Befehle und Werkzeuge stehen in `CI_AND_QUALITY_GATES.md`.

Ein vollstaendiger lokaler Lauf erfolgt nur nach abgeschlossenem Review ohne
offene Befunde, auf dem finalen `HEAD` und nach ausdruecklicher Owner-Anweisung.

GitHub-CI fuehrt waehrend eines Draft-PR keine Firmwaretests aus. Der Owner setzt
den PR nach dem Review auf `Ready for review`; dadurch startet der vollstaendige
CI-Lauf. Spaetere semantische Pushes auf einen nicht als Draft markierten PR
starten CI erneut.

Markdown-only- und Kommentaraenderungen bleiben von der Firmware-CI ausgenommen.
Das ist keine Ausnahme vom Review: Jede semantische Aenderung, auch an
normativer Dokumentation, verwirft den bisherigen Reviewnachweis. Nur rein
redaktionelle Korrekturen ohne Bedeutungs-, Scope-, Vertrags- oder
Akzeptanzwirkung duerfen den Reviewnachweis behalten.

Nach einem CI-Fehlschlag dokumentiert der Agent Fehler, Auswirkung und gezielte
Korrektur. Nur der Owner entscheidet, ob der PR wieder als Draft gefuehrt wird.
Nach Korrektur und direkt abhaengigen Tests folgt erneut ein vollstaendiges
Review; den neuen Wechsel auf `Ready for review` fuehrt ebenfalls nur der Owner
aus.

## 9. Handover

Vor Sessionende, Kontextreset oder Agentenwechsel wird bei offenem PR genau ein
aktueller `SESSION HANDOVER`-Kommentar erstellt und danach angehalten.

Der Handover nennt kompakt:

- Branch, aktuellen `HEAD` und freigegebenen Plan-Commit;
- erledigte und geaenderte Bereiche;
- ausgefuehrte Tests mit Ergebnis;
- offene Befunde, Risiken und Gates;
- naechsten konkreten Schritt.

Der neueste Handover ersetzt fruehere. Diffs, Plaene und Logs werden
referenziert, nicht kopiert. Ist kein PR-Kommentar moeglich, wird der fertige
Text im Chat als nicht veroeffentlicht ausgegeben.

## 10. Abschluss und Ownerrechte

Der Agent aktualisiert Nachweise und haelt fuer das Ownerreview an. Er setzt
einen PR nicht selbst auf `Ready for review` oder Draft, mergt nicht, aktiviert
kein Auto-Merge, verwendet keinen Force-Push, loescht keinen Branch und
schliesst kein Issue eigenmaechtig.

Nach einem Owner-Merge wird `docs/ROADMAP.md` auf dem naechsten passenden Branch
aktualisiert, bevor neue Arbeit gestartet wird.
