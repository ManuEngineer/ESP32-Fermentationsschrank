# Engineering Principles

Diese Prinzipien gelten fuer Entwicklung, Architekturentscheidungen und Reviews im Repository.

Sie ergaenzen die bestehenden Architektur-, Sicherheits- und Workflowregeln in `AGENTS.md`.

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
