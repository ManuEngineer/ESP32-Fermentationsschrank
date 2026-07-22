Arbeite im Repository:

ManuEngineer/ESP32-Fermentationsschrank

Die vorbereiteten Agent-Aufträge befinden sich unter:

Agent-Auftraege/ESP32-Fermentationsschrank_Agent-Auftraege_Issues_09-37

Ziel dieses Auftrags ist, genau EIN nächstes ausführbares Issue zu ermitteln,
vollständig umzusetzen und dafür einen Pull Request zu erstellen.

Die erste Repository-Änderung jeder Ausführung ist bei Bedarf die
Synchronisierung des INDEX mit dem aktuellen GitHub-Stand; erst danach darf ein
neues Issue ausgewählt und bearbeitet werden.

## 1. Repository vorbereiten

Aktualisiere zuerst den lokalen Stand:

- git checkout main
- git pull --ff-only
- git fetch --prune
- git status

Falls lokale Änderungen, nicht eingecheckte Dateien oder ein nicht sauberer
Arbeitsbaum vorhanden sind, ändere nichts und melde den Zustand als BLOCKED.

Arbeite niemals direkt auf main.

## 2. Regeln und Auftragsunterlagen lesen

Lies vor der Auswahl eines Issues vollständig:

- AGENTS.md
- alle für die später geänderten Verzeichnisse geltenden untergeordneten AGENTS.md
- docs/SPECIFICATION_REVIEW.md
- docs/DECISIONS.md
- Agent-Auftraege/ESP32-Fermentationsschrank_Agent-Auftraege_Issues_09-37/README.md
- Agent-Auftraege/ESP32-Fermentationsschrank_Agent-Auftraege_Issues_09-37/INDEX.md

Die GitHub-Issues, akzeptierte ADRs und die kanonische
Dokumentationspriorität sind verbindlich. Der INDEX ist nur ein Snapshot und
darf nicht ungeprüft als aktueller Status behandelt werden.

## 3. Live-Status ermitteln und INDEX synchronisieren

Prüfe vor der Auswahl einer neuen Aufgabe den aktuellen Live-Stand aller
Arbeits-Issues #9 bis #37 im GitHub-Repository.

Prüfe für jedes Issue mindestens:

- GitHub-Zustand: offen oder geschlossen
- aktueller Status im Issue-Text
- abgeschlossene und offene Abhängigkeiten
- zugehörige offene oder gemergte Pull Requests
- bestehende Arbeitsbranches
- Hardware-, Mess- oder Spezifikationsblockaden

Der INDEX ist nur ein lokaler Snapshot und keine Quelle der Wahrheit.

Synchronisiere danach:

Agent-Auftraege/ESP32-Fermentationsschrank_Agent-Auftraege_Issues_09-37/INDEX.md

mit dem aktuellen GitHub-Stand.

### Verbindliche Statusermittlung

Verwende folgende Priorität:

1. `COMPLETED`

   Setze den INDEX-Status auf `COMPLETED`, wenn mindestens eines davon gilt:

   - das GitHub-Issue ist geschlossen und als abgeschlossen markiert
   - ein gemergter Pull Request hat das Issue geschlossen
   - die Definition of Done ist nachweislich erfüllt und GitHub zeigt das Issue
     als abgeschlossen

   Ein eventuell noch im Issue-Text oder INDEX stehendes `READY`,
   `PLANNED_SPEC_PENDING` oder anderes älteres Statusfeld wird dadurch
   überschrieben.

   Beispiel:
   Ist Issue #10 auf GitHub abgeschlossen, muss sein INDEX-Eintrag von `READY`
   auf `COMPLETED` geändert werden.

2. `BLOCKED_HARDWARE`

   Verwende diesen Status bei offenen Issues, deren vollständige Umsetzung reale,
   noch nicht verfügbare oder noch nicht geprüfte Hardware voraussetzt.

3. `TBD_COMMISSIONING`

   Verwende diesen Status bei offenen Issues, deren Abschluss reale Messreihen,
   Kalibrierungen, Belastungstests oder Inbetriebnahmeergebnisse voraussetzt.

4. `PLANNED_SPEC_PENDING`

   Verwende diesen Status nur, wenn das Issue offen ist und gemäss aktuellem
   GitHub-Inhalt oder Spezifikation noch nicht zur Umsetzung freigegeben ist.

   Abgeschlossene Abhängigkeiten allein reichen nicht aus, um diesen Status
   automatisch auf `READY` zu setzen, sofern die Freigabe nicht eindeutig aus
   dem aktuellen Issue oder den verbindlichen Dokumenten hervorgeht.

5. `READY`

   Verwende diesen Status nur, wenn:

   - das Issue offen ist
   - alle Abhängigkeiten abgeschlossen sind
   - keine Hardware-, Mess- oder Spezifikationsblockade besteht
   - kein anderer Agent oder offener PR dasselbe Issue bearbeitet
   - die Umsetzung gemäss aktuellem GitHub-Issue und Spezifikation freigegeben ist

6. Unklarer oder widersprüchlicher Status

   Bei einem echten Widerspruch zwischen Issue, Abhängigkeiten, PR-Stand und
   Spezifikation:

   - nicht raten
   - das Issue nicht auswählen
   - keine fachliche Implementierung beginnen
   - den Konflikt konkret melden

### Regeln für die INDEX-Aktualisierung

- Aktualisiere alle veralteten Statuswerte, nicht nur den ausgewählten Eintrag.
- Geschlossene Issues werden immer als `COMPLETED` eingetragen.
- Bereits gemergte oder geschlossene Issues werden nicht erneut ausgewählt.
- Branchvorschläge bleiben bei abgeschlossenen Issues zur Nachvollziehbarkeit
  bestehen, sofern keine Bereinigung ausdrücklich vorgesehen ist.
- Ändere nicht automatisch die vorbereiteten Issue#XX.md-Dateien, nur weil ihr
  Snapshot-Status veraltet ist.
- Das aktuelle GitHub-Issue hat bei Scope, Status und Abhängigkeiten Vorrang vor
  Issue#XX.md.
- Ändere den GitHub-Issue-Text oder dessen Status nicht allein aufgrund eines
  abweichenden INDEX-Eintrags.
- Eine eindeutige, rein administrative Statuskorrektur im INDEX benötigt keine
  Rückfrage.

Führe die INDEX-Aktualisierung auf demselben Branch wie die anschliessende
Issue-Umsetzung aus.

Falls kein fachliches Issue ausführbar ist, der INDEX aber veraltet ist:

- erstelle einen kleinen Dokumentationsbranch
  `docs/sync-agent-task-index`
- aktualisiere ausschliesslich den INDEX
- erstelle dafür einen kleinen Pull Request
- beginne kein blockiertes fachliches Issue
- halte danach an

## 4. Nächstes ausführbares Issue auswählen

Wähle erst nach der vollständigen INDEX-Synchronisierung das Issue mit der
niedrigsten Nummer, das aktuell tatsächlich ausführbar ist.

Es müssen alle folgenden Bedingungen erfüllt sein:

- Status nach der obigen Prüfung ist `READY`
- GitHub-Issue ist offen
- alle Abhängigkeiten sind abgeschlossen
- kein offener Pull Request bearbeitet dasselbe Issue
- kein aktiver Arbeitsbranch bearbeitet dasselbe Issue
- keine reale Hardware oder Messreihe fehlt
- keine Spezifikationsentscheidung ist offen
- das Issue ist noch nicht durch einen gemergten PR umgesetzt

Wähle pro Ausführung genau ein Issue.

Nachdem ein Issue ausgewählt wurde:

- öffne die zugehörige Datei `Issue#XX.md`
- lies das aktuelle GitHub-Issue vollständig
- lies alle dort genannten Spezifikationsquellen
- verwende den Abschnitt „Fertiger Auftrag zum Kopieren“
- passe veraltete Snapshot-Angaben an den aktuellen GitHub-Stand an
- bearbeite ausschliesslich dieses eine Issue

## 5. Issue-spezifischen Auftrag verwenden

Öffne für das ausgewählte Issue:

Agent-Auftraege/ESP32-Fermentationsschrank_Agent-Auftraege_Issues_09-37/Issue#XX.md

Lies zusätzlich das vollständige aktuelle GitHub-Issue und alle darin genannten
Spezifikationsquellen.

Der aktuelle GitHub-Inhalt hat Vorrang vor einem eventuell älteren Snapshot in
Issue#XX.md.

Befolge den Abschnitt "Fertiger Auftrag zum Kopieren" und alle darin enthaltenen:

- Scope-Vorgaben
- Nicht-im-Scope-Regeln
- Abhängigkeiten
- Akzeptanzkriterien
- Testanforderungen
- Sicherheitsregeln
- Stop-Bedingungen

## 6. Vor der Implementierung planen

Berichte zuerst kurz:

- welches Issue ausgewählt wurde
- weshalb es aktuell ausführbar ist
- welche Abhängigkeiten geprüft wurden
- welchen Branch du erstellen wirst
- welche Dateien oder Module voraussichtlich betroffen sind
- welche Tests vorgesehen sind
- welche Risiken oder offenen Entscheidungen bestehen

Fahre danach selbstständig mit der Umsetzung fort, sofern keine Stop-Bedingung
vorliegt.

Bei einer echten Architekturentscheidung, einem Spezifikationswiderspruch,
fehlender Hardware, fehlenden Messwerten oder einer sicherheitsrelevanten
Unklarheit musst du anhalten und nachfragen.

## 7. Branch und Umsetzung

Erstelle genau einen neuen Branch vom aktuellen main.

Verwende grundsätzlich den im INDEX beziehungsweise im Issue-Auftrag
vorgeschlagenen Branchnamen. Kürze ihn nur, wenn es technisch notwendig ist,
ohne die Issue-Nummer zu entfernen.

Setze ausschliesslich das ausgewählte Issue um.

Zwingende Grenzen:

- keine Bearbeitung weiterer Issues
- keine direkte Änderung auf main
- keine erfundenen Hardwarewerte
- keine TBD_HARDWARE- oder TBD_COMMISSIONING-Werte als bestätigt behandeln
- keine Secrets oder Zugangsdaten einchecken
- config/hardware.yaml, config/pins.yaml und andere lokale Konfigurationen nicht
  einchecken
- bestehende Architektur und ADR-013 einhalten
- main.cpp bleibt Composition Root ohne Fachlogik
- keine reale Aktorfreigabe ohne ausdrücklich bestätigte Voraussetzungen
- keine unnötigen Abhängigkeiten oder grossen Refactorings ausserhalb des Issues

## 8. Tests und Dokumentation

Führe alle im Issue-Auftrag verlangten Prüfungen aus.

Zusätzlich mindestens, soweit für den aktuellen Stand anwendbar:

- pio run -e native -e esp32_bringup -e esp32_release
- pio test -e native
- python scripts/check_platformio_config.py
- bestehende Format-, Lint-, Static-Analysis- und Secret-Prüfungen
- git diff --check

Dokumentiere:

- welche Prüfungen erfolgreich waren
- welche Prüfungen nicht ausgeführt werden konnten
- welche Punkte BLOCKED bleiben
- weshalb ein Test nicht ausführbar war

Nicht ausgeführte Hardwaretests dürfen niemals als bestanden dargestellt werden.

## 9. Pull Request erstellen

Erstelle nach erfolgreicher Umsetzung einen Pull Request gegen main.

Der Pull Request muss enthalten:

- klare Zusammenfassung
- Bezug zum bearbeiteten Issue
- Scope und bewusste Nicht-Scope-Punkte
- ausgeführte Tests mit Ergebnissen
- nicht ausführbare Tests oder BLOCKED-Punkte
- relevante Sicherheits- und Architekturhinweise
- "Closes #XX"

## 10. Danach anhalten

Nach dem Erstellen des Pull Requests:

- den PR nicht selbst mergen
- den Branch nicht löschen
- nicht automatisch mit dem nächsten Issue beginnen
- keine weiteren Repository-Änderungen durchführen

Ich übernehme anschliessend:

- Review
- allfällige Korrekturaufträge
- Merge
- Löschen des Branches

Schliesse mit einem Bericht ab, der enthält:

- ausgewähltes Issue
- Branchname
- PR-Link
- wichtigste Änderungen
- Testergebnisse
- bekannte Einschränkungen
- Punkte, die ich beim Review besonders prüfen sollte