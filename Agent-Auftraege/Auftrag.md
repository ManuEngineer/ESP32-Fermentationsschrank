# Allgemeiner Agent-Auftrag: nächstes ausführbares Issue

Arbeite im Repository:

`ManuEngineer/ESP32-Fermentationsschrank`

Die vorbereiteten Agent-Aufträge befinden sich unter:

`Agent-Auftraege/ESP32-Fermentationsschrank_Agent-Auftraege_Issues_09-37`

Ziel dieser Ausführung ist, den Aufgabenstatus mit GitHub zu synchronisieren,
genau **ein** nächstes ausführbares Issue umzusetzen, dafür einen Pull Request
gegen `main` zu erstellen und danach anzuhalten.

Ich übernehme Review, Merge und Branch-Löschung. Merge oder lösche daher weder
Pull Request noch Branch selbst.

## 1. Repository vorbereiten

```bash
git checkout main
git pull --ff-only
git fetch --prune
git status
git branch --show-current
```

Falls der Arbeitsbaum nicht sauber ist oder ein Merge, Rebase oder Cherry-Pick
läuft:

- ändere und verwerfe nichts,
- erstelle keinen Branch,
- melde den Zustand mit den betroffenen Dateien als `BLOCKED`,
- halte danach an.

Arbeite niemals direkt auf `main`.

## 2. Verbindliche Regeln lesen

Lies vor der Auswahl eines Issues vollständig:

- `AGENTS.md`
- alle für die später geänderten Verzeichnisse geltenden untergeordneten
  `AGENTS.md`
- `docs/SPECIFICATION_REVIEW.md`
- `docs/DECISIONS.md`
- `Agent-Auftraege/ESP32-Fermentationsschrank_Agent-Auftraege_Issues_09-37/README.md`
- `Agent-Auftraege/ESP32-Fermentationsschrank_Agent-Auftraege_Issues_09-37/INDEX.md`

Die dort festgelegten Architektur-, Sicherheits-, Qualitäts-, Dokumentations-
und Quellenregeln gelten vollständig und werden in diesem Auftrag nicht
wiederholt.

Massgebend sind der aktuelle GitHub-Stand und die kanonische Quellenreihenfolge
aus `docs/SPECIFICATION_REVIEW.md`. `INDEX.md` und `Issue#XX.md` sind
Arbeits-Snapshots und dürfen nicht ungeprüft als aktueller Stand verwendet
werden.

## 3. Live-Status prüfen und INDEX-Korrekturen bestimmen

Prüfe den aktuellen GitHub-Stand der Issues #9 bis #37. Berücksichtige dabei:

- offen oder geschlossen,
- Status und Abhängigkeiten des aktuellen Issue-Texts,
- offene und gemergte Pull Requests,
- vorhandene Remote-Arbeitsbranches,
- Blockaden durch Spezifikation, Hardware oder Inbetriebnahme.

Vergleiche das Ergebnis mit `INDEX.md` und bestimme alle notwendigen
Statuskorrekturen.

Dabei gelten die Statusregeln aus der README, insbesondere:

- ein abgeschlossenes GitHub-Issue wird im INDEX als `COMPLETED` geführt,
- `COMPLETED` wird nicht erneut bearbeitet,
- `BLOCKED_HARDWARE` und `TBD_COMMISSIONING` werden nicht ohne reale Nachweise
  begonnen oder abgeschlossen,
- `PLANNED_SPEC_PENDING` darf nur zu `READY` werden, wenn Freigabe, Scope und
  Abhängigkeiten nach aktuellem GitHub- und Spezifikationsstand eindeutig sind,
- abgeschlossene Abhängigkeiten allein reichen nicht für `READY`,
- bei Widersprüchen wird nicht geraten.

Ein offener Pull Request oder ein aktiver Branch für dasselbe Issue verhindert
eine parallele Bearbeitung, ändert den fachlichen Status im INDEX aber nicht
automatisch.

Ändere in dieser Phase noch keine Datei.

## 4. Genau ein ausführbares Issue auswählen

Wähle das Issue mit der niedrigsten Nummer, das nach der Live-Prüfung tatsächlich
ausführbar ist. Es muss mindestens gelten:

- GitHub-Issue ist offen,
- Status ist `READY`,
- alle Abhängigkeiten sind abgeschlossen,
- kein offener PR und kein aktiver Remote-Branch bearbeitet dasselbe Issue,
- keine Spezifikations-, Hardware-, Mess- oder Sicherheitsblockade besteht,
- das Issue wurde noch nicht durch einen gemergten PR umgesetzt.

Überspringe kein früheres ausführbares Issue ohne dokumentierten Grund.
Bearbeite pro Ausführung genau ein Issue.

Lies danach vollständig:

- das aktuelle GitHub-Issue,
- die zugehörige Datei `Issue#XX.md`, insbesondere den Abschnitt
  **Fertiger Auftrag zum Kopieren**,
- alle dort genannten Spezifikationsquellen,
- alle für die betroffenen Verzeichnisse geltenden `AGENTS.md`.

Der aktuelle GitHub- und Spezifikationsstand hat Vorrang vor veralteten Angaben
in `Issue#XX.md`.

## 5. Plan berichten

Berichte vor Brancherstellung und Dateiänderungen kurz:

- ausgewähltes Issue und Begründung der Ausführbarkeit,
- geprüfte Abhängigkeiten, PRs und Branches,
- vorgesehener Branchname,
- notwendige INDEX-Korrekturen,
- geplanter Dateiumfang und Tests,
- Risiken oder echte offene Entscheidungen.

Fahre danach selbstständig fort. Halte nur bei einer Stop-Bedingung aus den
Repository-Regeln oder dem Issue-Auftrag an, insbesondere bei einem
Spezifikationswiderspruch, einer neuen Architekturentscheidung oder fehlenden
Hardware- beziehungsweise Messgrundlagen.

## 6. Branch erstellen und INDEX zuerst synchronisieren

Erstelle den im INDEX beziehungsweise Issue-Auftrag vorgeschlagenen Branch vom
aktuellen `main`. Die Issue-Nummer muss im Branchnamen erhalten bleiben.

```bash
git checkout -b <branchname>
git branch --show-current
git status
```

Die erste Dateiänderung auf diesem Branch ist die Synchronisierung von:

`Agent-Auftraege/ESP32-Fermentationsschrank_Agent-Auftraege_Issues_09-37/INDEX.md`

Aktualisiere dabei alle zuvor eindeutig festgestellten veralteten Einträge, nicht
nur das ausgewählte Issue. Geschlossene Issues werden `COMPLETED`.

Das aktuell bearbeitete offene Issue bleibt `READY`; setze es nicht vor dem
Merge auf `COMPLETED`.

Ändere aufgrund der Synchronisierung weder GitHub-Issue-Texte noch
`Issue#XX.md` oder die README der Agent-Aufträge.

Prüfe den INDEX-Diff, bevor du fachliche Dateien änderst:

```bash
git diff -- Agent-Auftraege/ESP32-Fermentationsschrank_Agent-Auftraege_Issues_09-37/INDEX.md
```

## 7. Issue-Auftrag vollständig ausführen

Setze ausschliesslich das ausgewählte Issue gemäss aktuellem GitHub-Issue,
`Issue#XX.md`, den dort genannten Spezifikationen und den Repository-Regeln um.

Halte insbesondere Scope, Nicht-Scope, Abhängigkeiten, Akzeptanzkriterien,
Tests, Dokumentationspflichten und Stop-Bedingungen ein.

Nimm keine Arbeiten für weitere Issues vor. Unbestätigte Hardware-,
Inbetriebnahme- oder Budgetwerte bleiben als solche gekennzeichnet.

## 8. Prüfen, dokumentieren und Pull Request erstellen

Führe alle im Repository und im Issue-Auftrag vorgesehenen Builds, Tests und
Qualitätsprüfungen aus. Ein Test gilt nur als bestanden, wenn er tatsächlich
erfolgreich ausgeführt wurde. Nicht ausführbare Hardware- oder
Inbetriebnahmetests sind mit Begründung als nicht ausgeführt beziehungsweise
`BLOCKED` zu dokumentieren.

Prüfe vor dem Commit mindestens:

```bash
git status
git diff --check
git diff --stat
git diff
```

Committe und pushe nur Änderungen im Scope des ausgewählten Issues sowie die
INDEX-Synchronisierung.

Erstelle anschliessend einen Pull Request gegen `main`. Er muss enthalten:

- Zusammenfassung und Scope,
- aktualisierte INDEX-Einträge,
- ausgeführte Prüfungen mit Ergebnissen,
- nicht ausgeführte oder blockierte Prüfungen,
- bekannte Einschränkungen und verbleibende TBD-Werte,
- relevante Sicherheits- oder Architekturhinweise,
- `Closes #XX` für genau das bearbeitete Issue.

## 9. Sonderfall: Kein fachliches Issue ausführbar

Ist kein Issue ausführbar, der INDEX aber veraltet:

- erstelle vom aktuellen `main` den Branch `docs/sync-agent-task-index`,
- ändere ausschliesslich den INDEX,
- führe `git diff --check` aus,
- erstelle einen kleinen PR gegen `main`,
- erkläre die Statuskorrekturen und die Blockade des nächsten Issues,
- halte danach an.

Ist kein Issue ausführbar und der INDEX bereits aktuell:

- erstelle keinen Branch und keinen PR,
- ändere keine Datei,
- berichte die konkrete Blockade der nächsten Issues,
- halte danach an.

## 10. Nach dem Pull Request anhalten

Nach Erstellung des Pull Requests:

- nicht mergen und kein Auto-Merge aktivieren,
- Branch nicht löschen,
- GitHub-Issue nicht eigenmächtig schliessen,
- nicht mit dem nächsten Issue beginnen,
- keine weiteren Repository-Änderungen durchführen.

Schliesse mit einem Bericht ab:

- Issue, Branch und PR-Link,
- aktualisierte INDEX-Einträge,
- wichtigste Änderungen,
- Testergebnisse und nicht ausgeführte Prüfungen,
- bekannte Einschränkungen und Blockaden,
- besonders zu prüfende Review-Punkte.

Danach halte vollständig an.
