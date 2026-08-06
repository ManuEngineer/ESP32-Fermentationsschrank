# Engineering-Grundsaetze

Diese Grundsaetze sind fuer Planung, Architektur, Implementierung, Tests und
Reviews verbindlich. Spezifische Safety-, Security-, Recovery-, Workflow- und
Architekturvertraege bleiben vorrangig und werden dadurch nicht ersetzt.

## Repository-first und keine Parallelwahrheit

Bestehender Code, Modelle, Tests, akzeptierte ADRs und kanonische Dokumente sind
vor jeder Planung, Umsetzung und jedem Review die erste Quelle der Wahrheit.

- Vorhandene Typen, APIs, Validierungen und Architekturgrenzen werden
  wiederverwendet.
- Ersatzmodelle, Parallelvertraege und erfundene Abstraktionen sind unzulaessig,
  solange die bestehende Verantwortung bereits abgedeckt ist.
- Bestehende Logik wird in Plaenen und Auftraegen nicht kopiert, wenn ein
  eindeutiger Verweis genuegt.
- Nur nachweisliche Luecken werden geplant oder implementiert.
- Widersprueche werden offen benannt und nicht still aufgeloest.
- Neue fachliche, persistente, Security-, Safety-, Recovery- oder
  Hardwareentscheidungen benoetigen das vorgesehene Owner-Gate.

## Inkrementelle Kontextaktualisierung

Nach einer vollstaendigen Erstorientierung werden Repository, Branch und
gepruefter Commit als Kontextbaseline dokumentiert.

- Unveraenderter Branch und `HEAD`: gelesenen Kontext wiederverwenden.
- Geaenderter `HEAD`: zuerst Commitliste, Dateiliste und Diff seit der Baseline
  pruefen; danach geaenderte Dateien und direkt betroffene Schnittstellen,
  Modelle, Tests, Architekturguards und lokale Regeln gezielt neu lesen.
- Unveraenderte, nicht betroffene Dateien werden nicht ritualistisch erneut
  vollstaendig gelesen.
- Ein Full Refresh ist erforderlich bei unbekannter Baseline, Branchwechsel,
  Rebase oder Merge sowie materiellen Aenderungen an Regeln, ADRs, Architektur,
  Modulgrenzen, Buildsystem, Toolchain, oeffentlichen APIs, Wireformaten,
  Persistenzschemas oder Security-, Safety-, Recovery- und Hardwaregrenzen.
- Bei breitem Diff, Widerspruch oder begruendetem Zweifel wird ebenfalls ein
  Full Refresh durchgefuehrt.

Der Nachweis verwendet mindestens:

```text
CONTEXT_BASELINE_BRANCH: <branch>
CONTEXT_BASELINE_SHA: <sha>
CONTEXT_HEAD_SHA: <sha>
CONTEXT_PLAN_SHA: <sha oder NONE>
CONTEXT_REFRESH_MODE: REUSED | INCREMENTAL | FULL
CONTEXT_DELTA: <gepruefte Commits und Dateien>
SOURCE_OF_TRUTH_CONFLICT: NONE | <Beschreibung>
```

Eine neue Agentensitzung uebernimmt frueheren Kontext nicht automatisch.
Branch, Baseline, freigegebener Plan und offene Befunde muessen deshalb in PR,
Auftrag oder aktuellem Handover nachvollziehbar sein.

## SOLID

- **Single Responsibility:** Funktionen, Klassen und Module besitzen eine klar
  abgegrenzte Verantwortung.
- **Open/Closed:** Stabile Kernlogik wird bevorzugt ueber vorhandene
  Abstraktionen erweitert, statt fuer jede Erweiterung wiederholt veraendert.
- **Liskov Substitution:** Implementierungen einer Schnittstelle bleiben ohne
  Verletzung ihres dokumentierten Vertrags austauschbar.
- **Interface Segregation:** Kleine zweckgebundene Schnittstellen sind grossen
  universellen Schnittstellen vorzuziehen.
- **Dependency Inversion:** Fachlogik haengt von Abstraktionen ab, nicht direkt
  von Hardware-, Framework-, Bibliotheks- oder Infrastrukturklassen.

## DRY

Fachliche Regeln, Validierungen und Vertraege besitzen eine eindeutige Quelle.
Copy-and-paste-Logik, parallele Implementierungen und semantisch doppelte
Dokumentation werden vermieden.

DRY verlangt keine gemeinsame Abstraktion fuer nur oberflaechlich aehnlichen
Code mit unterschiedlichen fachlichen Verantwortungen.

## KISS

Bevorzugt wird die einfachste verstaendliche, testbare und wartbare Loesung,
welche Anforderungen sowie Safety-, Security- und Recoverygrenzen vollstaendig
erfuellt.

SOLID rechtfertigt keine vorsorgliche Ueberabstraktion. KISS rechtfertigt keine
Vereinfachung von Safety, Security, Recovery, Testbarkeit oder dokumentierten
Vertraegen. Bewusste Abweichungen werden im freigegebenen Plan konkret
begruendet und im Review gegen den tatsaechlichen Diff bewertet.

## Adopt-or-build und Espressif-first

Vor einer Eigenentwicklung wird geprueft, ob eine bestehende Komponente die
Anforderung sicher, wartbar und ressourcengerecht erfuellt. Die detaillierten
Bewertungskriterien stehen in `ADOPT_OR_BUILD.md`.

Bei ESP32-Standardfaehigkeiten gilt folgende Rechercheprioritaet:

1. Built-ins der fixierten ESP-IDF-Version;
2. offizieller Namespace `espressif/*` im ESP Component Registry;
3. offizielle Repositories unter `github.com/espressif`;
4. geeignete gepflegte Drittkomponenten;
5. kleine Eigenentwicklung nur bei nachgewiesener Luecke.

Jeder Kandidat wird mindestens auf Funktionsumfang, Lizenz, Wartung,
Abhaengigkeiten, Ressourcenbedarf, Testbarkeit, Hardwareeignung und
Integrationsrisiko geprueft. Eine README-Aussage ersetzt keinen Nachweis.

Ein Framework- oder Toolchainwechsel fuehrt zu einer inkrementellen
Neubewertung betroffener Kandidaten im bestehenden Audit, nicht automatisch zu
einem neuen Gesamtaudit. Espressif-first bestimmt die Pruefreihenfolge, nicht
automatisch die Produktauswahl.

Espressif-MCP darf als Recherchewerkzeug verwendet werden. Es wird weder
Firmwareabhaengigkeit noch CI-Dienst noch produktiver Laufzeitbestandteil.
