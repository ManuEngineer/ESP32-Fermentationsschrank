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

## 7. Builder und unabhängiger Full Review

Codex beziehungsweise der ausführende Repository-Agent ist der Builder. Zu
seinen Aufgaben gehören Planerstellung, Implementierung, gezielte Tests,
notwendige Diagnose, CI- und Evidence-Aufbereitung sowie die Aktualisierung
des PR.

Vor der Übergabe führt der Builder einen angemessenen Implementation
Self-Check durch. Dieser Self-Check prüft die Umsetzung gegen den freigegebenen
Plan und die unmittelbar betroffenen Nachweise, ist aber kein unabhängiger Full
Review. Danach hält der Builder für den externen Owner-/Reviewer-Schritt an.

Der formale Independent Review erfolgt grundsätzlich unabhängig vom Builder.
Der aktuelle Owner-Reviewkanal liegt ausserhalb von ChatGPT Work/Codex; aktuell
wird dafür normales ChatGPT mit GPT-5.6 Sol verwendet. Diese konkrete
Modellauswahl ist eine operative Owner-Einstellung und keine dauerhafte
Architekturabhängigkeit.

Der Independent Reviewer prüft den gesamten relevanten aktuellen PR gegen den
freigegebenen Plan und das Issue sowie gegen Anforderungen und relevante ADRs
und Fachverträge. Das vollständige Review umfasst mindestens:

- Architektur, Correctness und Fehlerfälle;
- Safety, Security und Recovery, soweit betroffen;
- Tests und Evidence;
- SOLID, DRY und KISS;
- Ressourcen-, Hardware- und Lizenzwirkung, soweit betroffen;
- Dokumentationskonsistenz;
- unbeabsichtigte Dateien, Secrets und lokale Pfade.

Der Review endet nicht beim ersten oder auffälligsten Befund.

Für den unabhängigen Review gelten verbindlich drei Finding-Klassen:

- `BLOCKER`: Ein Finding sperrt den aktuellen PR nur, wenn ein Acceptance
  Criterion nicht erfüllt ist, nachweislich falsches Verhalten oder eine
  Regression vorliegt, ein Safety-, Security- oder Datenintegritätsproblem
  besteht, ein verbindlicher Architektur-, ADR-, API-, Persistenz-, Wireformat-
  oder sonstiger Vertragsverstoss vorliegt, ein erforderlicher Test oder
  Nachweis fehlt, ein Pflicht-Build, CI- oder Quality Gate fehlschlägt oder der
  genehmigte Scope nicht korrekt umgesetzt ist.
- `FOLLOW-UP`: Reale technische Schuld, konkreter zukünftiger Nutzen, ein
  bekanntes Risiko oder eine absehbare Anforderung, die den aktuellen PR nicht
  als Blocker sperrt.
- `NO-ACTION`: Rein theoretische Verbesserungen, Stilpräferenzen und
  hypothetische Zukunftserweiterungen. Sie erzeugen kein neues Issue.

Nach lokal begrenzten Korrekturen erfolgt grundsätzlich Fix Verification:

1. Verifikation aller zuvor offenen Blocker;
2. Review des Korrekturdiffs;
3. Prüfung direkt betroffener Verträge und Regressionen;
4. Bewertung, ob die Korrektur materiell über den bisherigen Reviewumfang
   hinausgeht.

Ein neuer Full Review ist nur erforderlich, wenn die Korrektur Scope,
Architektur, öffentliche Verträge, Persistenz/Wireformat,
Safety/Security/Recovery oder Laufzeitverhalten materiell verändert oder einen
breiten neuen Diff erzeugt. Die gleiche Regel gilt nach einer CI-Korrektur.

## 8. Convergence Gate

Ein Independent Review erhält aus Review-Sicht `GO`, sobald der Review den
Scope, die Implementation und die fuer die Draft-/Reviewphase erforderlichen
gezielten Tests und Nachweise bewertet hat, alle dabei relevanten Acceptance
Criteria erfuellt sind und `OPEN_BLOCKERS=0` gilt. Der vollstaendige
Pre-Ready-Lauf ist kein Bestandteil dieses Review-GO und keine Voraussetzung
fuer `INDEPENDENT_REVIEW=PASS`; er ist das naechste, separat autorisierte
Owner-Gate.

Das ist noch keine Freigabe fuer `Ready for review`. Die verbindliche
Reihenfolge lautet:

```text
Independent Review abgeschlossen
-> OPEN_BLOCKERS=0
-> Owner autorisiert finalen lokalen Pre-Ready-Lauf
-> PRE_READY_LOCAL_GATES=PASS auf exakt finalem HEAD
-> Owner setzt Ready for review
-> GitHub-CI PASS
-> Merge-Gate
```

Nach Erreichen von `OPEN_BLOCKERS=0` dürfen `FOLLOW-UP` oder `NO-ACTION` den
autorisierten lokalen Lauf nicht blockieren. `Ready for review` bleibt aber
bis zum bestandenen `PRE_READY_LOCAL_GATES=PASS` ausgeschlossen. Das bestehende
Owner-Gate für Ready, Merge und Issue-Abschluss bleibt unverändert.

## 9. Modell-/Compute-Governance

Der Standard-Builder ist die projektlokale Codex-Konfiguration. Ein deutlich
kostenintensiveres Builder-Modell, insbesondere Terra, ist kein normaler
nächster Schritt und darf nicht vorsorglich verlangt werden.

Eine Eskalation darf dem Owner vorgeschlagen werden, wenn der Standard-Builder
wiederholt am gleichen klar abgegrenzten technischen Problem scheitert, ein
reproduzierbarer Fehler trotz geeigneter Diagnose ungeklärt bleibt, eine
aussergewöhnlich schwierige und weitreichende technische Entscheidung ansteht
oder ein kritisches Safety-, Security-, Concurrency- oder Recovery-Problem
zusätzliche Modellleistung rechtfertigt.

Vor einer Eskalation ist zu prüfen, ob bessere Eingrenzung, bessere Evidence
oder ein präziserer Auftrag ausreichen. Der Agent wechselt das
kostenintensivere Modell nicht eigenmächtig, sondern legt dem Owner den
Eskalationsgrund vor. Eine genehmigte Eskalation gilt nur für den konkreten
Problemumfang; danach gilt wieder der Standard-Builder.

## 10. Tests und GitHub-CI

Waehrend der Draft-Phase werden nur passende gezielte lokale Tests ausgefuehrt.
Der Independent Review bewertet Scope, Implementation und diese gezielten
Nachweise; der vollstaendige Pre-Ready-Lauf ist dafuer nicht erforderlich.
Zeitpunkt, Voraussetzungen und Toolvertraege stehen in
`CI_AND_QUALITY_GATES.md`; die vollstaendigen ausfuehrbaren Gatebefehle und
die clang-tidy-Dateiliste stehen ausschliesslich im dort referenzierten
`scripts/run_pre_ready_gates.sh`.

Ein vollstaendiger lokaler Pre-Ready-Lauf erfolgt nur nach abgeschlossenem
Independent Full Review mit `OPEN_BLOCKERS=0`, auf dem finalen `HEAD` und nach
ausdruecklicher Owner-Anweisung. Klassifizierte `FOLLOW-UP` und `NO-ACTION`
blockieren den Lauf nicht. Der Lauf verwendet die beiden Phasen des gemeinsamen
versionierten Runners aus `CI_AND_QUALITY_GATES.md`; erst beide Phasen ergeben
`PRE_READY_LOCAL_GATES=PASS`.

GitHub-CI fuehrt waehrend eines Draft-PR keine Firmwaretests aus. Der Owner setzt
den PR erst nach `PRE_READY_LOCAL_GATES=PASS` auf `Ready for review`; dadurch
startet der vollstaendige CI-Lauf. Spaetere semantische Pushes auf einen nicht
als Draft markierten PR starten CI erneut.

Markdown-only- und Kommentaraenderungen bleiben von der Firmware-CI ausgenommen.
Fuer den Reviewnachweis gilt bei semantischen Aenderungen die
Fix-Verification-/Materialitaetsregel: Eine lokal begrenzte Korrektur wird mit
Fix Verification und dem erforderlichen Regression Check abgeschlossen; eine
materielle Aenderung oder ein breiter neuer Diff erfordert einen neuen Full
Review. Nur rein redaktionelle Korrekturen ohne Bedeutungs-, Scope-, Vertrags-
oder Akzeptanzwirkung duerfen den bisherigen Reviewnachweis behalten.

Nach einem CI-Fehlschlag dokumentiert der Agent Fehler, Auswirkung und gezielte
Korrektur. Nur der Owner entscheidet, ob der PR wieder als Draft gefuehrt wird.
Nach einer CI-Korrektur werden die zuvor offenen Blocker, der
Korrekturdiff sowie direkt betroffene Vertraege und Regressionen per Fix
Verification geprueft. Ein neuer Full Review ist nur nach einer materiellen
Aenderung oder einem breiten neuen Diff erforderlich; den neuen Wechsel auf
`Ready for review` fuehrt ebenfalls nur der Owner aus.

## 11. Handover

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

## 12. Abschluss und Ownerrechte

Der Agent aktualisiert Nachweise und haelt fuer das Ownerreview an. Er setzt
einen PR nicht selbst auf `Ready for review` oder Draft, mergt nicht, aktiviert
kein Auto-Merge, verwendet keinen Force-Push, loescht keinen Branch und
schliesst kein Issue eigenmaechtig.

Nach einem Owner-Merge wird `docs/ROADMAP.md` auf dem naechsten passenden Branch
aktualisiert, bevor neue Arbeit gestartet wird.
