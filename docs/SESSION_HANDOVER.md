# Verbindliche Session- und Auftragsuebergabe

Diese Regel gilt fuer alle Agentenarbeiten an einem bestehenden Branch oder Pull
Request. Ihre Verbindlichkeit wird durch `AGENTS.md` hergestellt. Claude Code
uebernimmt sie ebenfalls, weil `CLAUDE.md` die zentrale `AGENTS.md` laedt.

## Zweck

Ein Kontextreset, eine neue Session oder ein Agentenwechsel darf keinen
relevanten Arbeitsstand verlieren. Der Agent muss deshalb am Ende jedes Auftrags
oder abgeschlossenen Arbeitsslices automatisch eine vollstaendige, aktuelle
Uebergabe als Kommentar im zugehoerigen Pull Request veroeffentlichen. Der Owner
muss diese Uebergabe nicht jeweils separat anfordern.

Ein Kontextreset wie `/clear` wird ausschliesslich durch den Owner ausgefuehrt.
Der Agent darf keinen Kontextreset vortaeuschen, keine alte Session als
verbindliche Quelle fortschreiben und den Owner nicht auffordern, vor der
veroeffentlichten Uebergabe den Kontext zu loeschen.

## Ausloeser

Eine Uebergabe ist insbesondere zwingend vor:

- dem Ende eines Agentenauftrags;
- dem Anhalten nach einer Planungsphase;
- dem Wechsel von Planung zu Umsetzung;
- dem Wechsel von Umsetzung zu Review oder Nachreview;
- der Uebergabe an einen anderen Agenten;
- einer laengeren Unterbrechung;
- einem empfohlenen Kontextreset oder `/clear`;
- dem Erreichen eines in sich abgeschlossenen Umsetzungsslices.

Ein kurzer Zwischenhinweis ersetzt die Uebergabe nicht.

## Verbindliches Verhalten

Der Agent muss vor der Uebergabe:

1. den aktuellen Branch, Pull Request und HEAD erneut ermitteln;
2. den Arbeitsbaum und den tatsaechlichen Diff pruefen;
3. Commits und Push-Status verifizieren;
4. ausgefuehrte Tests und Pruefungen anhand der realen Befehle erfassen;
5. offene Reviewthreads, Risiken, Blocker und nicht erledigte Punkte erfassen;
6. den Kommentar im Pull Request veroeffentlichen;
7. danach anhalten und dem Owner mitteilen, dass ein Kontextreset oder eine neue
   Session nun gefahrlos moeglich ist.

Der Agent darf den Auftrag nicht nur mit einer Chat-Zusammenfassung beenden,
wenn ein Pull Request existiert und er dort kommentieren kann.

Kann der Agent den Kommentar technisch nicht veroeffentlichen, muss er den
vollstaendigen, sofort einsetzbaren Kommentar im Chat ausgeben, den Grund nennen
und anhalten. Er darf in diesem Fall nicht behaupten, die Uebergabe sei
veroeffentlicht worden.

## Anforderungen an den PR-Kommentar

Jede neue Uebergabe muss eigenstaendig verstaendlich sein und den aktuellen
Gesamtstand des Auftrags enthalten. Sie ersetzt fuer die Fortsetzung alle
frueheren Session-Uebergaben. Weiterhin relevante Informationen aus aelteren
Kommentaren muessen deshalb knapp in die neueste Uebergabe uebernommen werden.

Die Uebergabe muss:

- kurz, konkret und nachpruefbar sein;
- ausschliesslich den tatsaechlichen Repository- und PR-Stand wiedergeben;
- Commit-SHAs, Befehle und Fundstellen nennen, wo sie den Nachweis tragen;
- zwischen erledigt, offen, blockiert und bewusst nicht begonnen unterscheiden;
- den naechsten konkreten Schritt so beschreiben, dass eine neue Session ohne
  alten Chatverlauf fortsetzen kann;
- keine vollstaendigen Logs, langen Diffs oder wiederholten Plantexte enthalten;
- keine unbelegten Aussagen wie `alles gruen`, `vollstaendig erledigt` oder
  `Review abgeschlossen` enthalten.

## Verbindliche Vorlage

```markdown
## SESSION HANDOVER

### Bezug
- Auftrag / Slice:
- Pull Request:
- Branch:
- Aktueller HEAD:
- Freigegebener Plan-Commit: <SHA oder nicht zutreffend>
- Letzter relevanter Arbeits-Commit:

### Aktueller Stand
- ...

### Erledigt
- ...

### Geaenderte Bereiche
- `pfad/datei`: kurze Bedeutung

### Ausgefuehrte Pruefungen
- `<exakter Befehl>`: bestanden / fehlgeschlagen / nicht vollstaendig ausfuehrbar

### Offene Punkte, Risiken und Blocker
- ...

### Bewusst nicht begonnen oder nicht geaendert
- ...

### Naechster konkreter Schritt
1. ...
2. ...

### Einstieg fuer die neue Session
1. `AGENTS.md` sowie werkzeugspezifische Zusatzregeln lesen.
2. Aktuellen PR und neuesten `SESSION HANDOVER`-Kommentar lesen.
3. Freigegebenen Plan und aktuellen Diff gegen den genannten HEAD pruefen.
4. Repository- und PR-Stand selbst verifizieren; keine alte Chatannahme
   uebernehmen.
```

## Zusatz fuer Reviews

Nach einem Review oder Nachreview enthaelt die Uebergabe zusaetzlich:

- alle gefundenen Befunde mit Schweregrad;
- betroffene Dateien und konkrete Fundstellen;
- bereits behobene und erneut gepruefte Befunde;
- noch offene Befunde und Reviewthreads;
- das genaue Reviewurteil zum aktuellen HEAD.

Ein Review darf nicht als abgeschlossen bezeichnet werden, wenn noch ungepruefte
Dateien, offene Threads, nicht ausgefuehrte relevante Tests oder ungeklaerte
Abweichungen bestehen.

## Abgrenzung zur PR-Beschreibung

Session-Uebergaben dokumentieren den operativen Zwischenstand. Sie ersetzen
nicht die abschliessende PR-Beschreibung, den freigegebenen Plan oder die
verbindliche Abschlussdokumentation vor `Ready for Review`.
