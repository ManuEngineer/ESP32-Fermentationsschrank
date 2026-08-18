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

Fuer nicht triviale Arbeit werden ein Branch und ein Draft-PR erstellt. Bietet
der verwendete Agent einen nativen Planungsmodus, wird dieser fuer
Repositoryanalyse, Rueckfragen und Planerstellung bevorzugt verwendet; bei
Codex ist dies Plan Mode. Der native Planungsmodus ist ein Arbeitsmodus und
kein eigenes Freigabeartefakt. Er ersetzt weder `docs/tasks/`, den Plan-Commit
noch das Owner-Gate.

Der Markdown-Auftrag muss die folgende Planstruktur nicht wiederholen. Ein
Verweis auf diesen Workflow genuegt, wenn Ziel, Scope und besondere
Owner-Gates des konkreten Auftrags eindeutig sind. Damit bleibt der Auftrag
kurz und der Planvertrag an genau einer kanonischen Stelle.

Vor dem Plan-Commit werden die Ergebnisse des nativen Planungsmodus in genau
eine vollstaendige, eigenstaendig ausfuehrbare Planfassung unter `docs/tasks/`
konsolidiert. Nur diese versionierte Markdown-Fassung ist der kanonische Plan.
Ein nur im Chat oder Plan Mode vorhandener Plan darf weder als freigegeben
gelten noch als zweite Planwahrheit parallel weitergefuehrt werden.

Der Plan ist proportional zu Umfang und Risiko der Aufgabe; KISS gilt auch fuer
die Planung. Er enthaelt in angemessener Tiefe mindestens:

- Ziel und Nicht-Ziele;
- verifizierte Live-Ausgangslage, Kontextbaseline und relevante Quellen;
- betroffene Module, Vertraege, Abhaengigkeiten, Blocker und Owner-Gates;
- konkrete Umsetzungs- und Commit-Schnitte;
- gezielte Tests, Nachweise und Dokumentationswirkung;
- offene Entscheidungen und materielle Risiken;
- Safety-, Security-, Recovery- und Hardwaregrenzen nur soweit tatsaechlich
  betroffen.

Nicht betroffene Themen werden nicht als leere Pflichtkapitel erzeugt. Bei
Architektur-, API-, Schema-, Persistenz- oder Wireformatarbeit nennt der Plan
die dafuer notwendigen Daten-, Zustands-, Schnittstellen- und
Abhaengigkeitsvertraege explizit.

Jede freigegebene Planrevision ist ein vollstaendiges und eigenstaendig
ausfuehrbares Dokument. Historische Revisionen duerfen zur Nachvollziehbarkeit
referenziert werden, ersetzen aber keine fuer die Umsetzung notwendigen
Inhalte. Eine aktuelle Revision darf nicht voraussetzen, dass ein Agent
mehrere alte Revisionen zusammensetzt. Findet ein Agent einen nicht
konsolidierten Plan vor, rekonstruiert er ihn nicht aus den historischen
Revisionen, sondern meldet die fehlende Plan-Konsolidierung als Blocker.

Ein freigegebener Plan enthaelt eine nachvollziehbare Struktur fuer die
Abarbeitung. Dafuer genuegen je nach Umfang kleine Umsetzungs- und
Commit-Schnitte, nummerierte Schritte, eine Checkliste oder eine vergleichbare
Struktur. Eine zusaetzliche starre Taskliste ist nicht erforderlich, wenn die
bestehende Planstruktur eine eigenstaendige und nachvollziehbare Umsetzung
ermoeglicht.

In der Planungsphase werden ohne ausdrueckliche Ownerfreigabe keine
Produktionslogik, produktiven Tests, Abhaengigkeiten, ADRs, Build-, Toolchain-,
Hardware-, GPIO- oder Bibliotheksentscheidungen umgesetzt. Ein abgeschlossener
Plan Mode ist ausdruecklich keine Implementationserlaubnis.

Nach dem Plan-Commit werden Planpfad, exakte Plan-SHA und offene Entscheidungen
im Draft-PR ausgewiesen. Danach wird angehalten.

Ist kein nativer Planungsmodus verfuegbar, wird derselbe Planvertrag direkt
erfuellt; es wird kein tool-spezifischer Parallelprozess erfunden.

## 4. Planfreigabe

Umgesetzt wird nur nach ausdruecklicher Ownerfreigabe des exakten
Plan-Commits. Eine allgemeine Zustimmung oder eine Freigabe eines nur im Chat
beziehungsweise Plan Mode sichtbaren Plans ohne eindeutige Plan-SHA ersetzt
dieses Gate nicht.

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

Fuer die Planrevision darf erneut ein nativer Planungsmodus verwendet werden;
freigabefaehig ist wiederum nur die vollstaendige neue Markdown-Planrevision
mit exakter SHA.

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
