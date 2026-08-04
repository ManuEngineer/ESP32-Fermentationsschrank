# Engineering Principles

Diese Prinzipien gelten fuer Entwicklung, Architekturentscheidungen und Reviews im Repository.

Sie ergaenzen die bestehenden Architektur-, Sicherheits- und Workflowregeln in `AGENTS.md`.

## Repository als Quelle der Wahrheit

Bestehender Code, bestehende Modelle, Tests, akzeptierte ADRs und kanonische
Dokumente sind vor jeder Planung, Implementierung und jedem Review die erste
Quelle der Wahrheit.

- Vorhandene Typen, APIs, Validierungen und Architekturgrenzen werden
  wiederverwendet.
- Ersatzmodelle, Parallelvertraege und erfundene Abstraktionen sind unzulaessig,
  solange der bestehende Code die Verantwortung bereits abdeckt.
- Bestehende Logik wird in Plaenen und Auftraegen nicht nochmals vollstaendig
  definiert, wenn ein eindeutiger Verweis genuegt.
- Aenderungen werden nur fuer eine nachweisliche Luecke geplant.
- Widersprueche werden offen benannt und nicht still aufgeloest.
- Neue fachliche, persistente, Security-, Safety-, Recovery- oder
  Hardwareentscheidungen benoetigen die vorgesehene Ownerfreigabe.

Die ausfuehrliche Herleitung steht in `docs/ENGINEERING_LEARNINGS.md`.

## Inkrementelle Kontextaktualisierung

Repositorypruefung bedeutet nicht, dass bei jeder Folgeaenderung das gesamte
Repository erneut gelesen werden muss.

Nach einer vollstaendigen Erstorientierung wird der gepruefte Branch und
Commit-SHA als Kontextbaseline dokumentiert.

- Ist Branch und `HEAD` unveraendert, darf der gelesene Kontext wiederverwendet
  werden.
- Hat sich `HEAD` geaendert, werden zuerst Diff, Commitliste und Dateiliste seit
  der Baseline ermittelt.
- Danach werden die geaenderten Dateien sowie direkt betroffene Schnittstellen,
  Modelle, Tests, Architekturguards und geltende `AGENTS.md` gezielt neu
  geprueft.
- Unveraenderte, nicht betroffene Dateien werden nicht rein ritualistisch erneut
  vollstaendig gelesen.
- Ein Full Refresh ist erforderlich, wenn die Baseline unbekannt ist oder sich
  Branch, Architektur, Modulgrenzen, Buildsystem, oeffentliche APIs,
  Wireformate, Persistenzschemas oder Security-/Safety-/Recoverygrenzen
  materiell geaendert haben.

Der Agent dokumentiert Baseline-SHA, aktuellen SHA und den Modus `REUSED`,
`INCREMENTAL` oder `FULL`. Die vollstaendige Regel steht in
`docs/ENGINEERING_LEARNINGS.md`.

## SOLID

Die Software soll nach Moeglichkeit die SOLID-Prinzipien einhalten.

### Single Responsibility Principle (SRP)

Jede Klasse, jedes Modul und jede Funktion soll eine klar abgegrenzte Verantwortung besitzen.

### Open/Closed Principle (OCP)

Erweiterungen sollen bevorzugt durch neue Implementierungen oder Erweiterungen von Abstraktionen erfolgen und nicht durch wiederholte Aenderungen stabiler Kernlogik.

### Liskov Substitution Principle (LSP)

Implementierungen einer Schnittstelle muessen austauschbar bleiben, ohne erwartetes Verhalten der Anwendung zu verletzen.

### Interface Segregation Principle (ISP)

Kleine, spezifische Schnittstellen sind grossen universellen Schnittstellen vorzuziehen.

### Dependency Inversion Principle (DIP)

Fachlogik soll von Abstraktionen abhaengen und nicht direkt von konkreten Hardware-, Bibliotheks- oder Infrastrukturimplementierungen.

## DRY (Don't Repeat Yourself)

Funktionalitaet soll nicht mehrfach unabhaengig implementiert werden.

Duplizierte Logik, widerspruechliche Implementierungen und Copy/Paste-Code sollen vermieden werden.

Gemeinsame Funktionalitaet soll eine eindeutige Zustaendigkeit besitzen.

## KISS (Keep It Simple)

Loesungen sollen so einfach wie moeglich bleiben.

Komplexitaet ist nur gerechtfertigt, wenn sie einen nachweisbaren Vorteil bringt, beispielsweise:

- hoehere Sicherheit
- bessere Testbarkeit
- klare Erweiterbarkeit
- notwendige Hardwareabstraktion

Einfacher, verstaendlicher und gut getesteter Code ist gegenueber unnoetig abstrakten Loesungen zu bevorzugen.
