# Agent-Auftraege fuer ESP32-Fermentationsschrank

Dieses Paket enthaelt je eine vorbereitete Arbeitsanweisung fuer die 29 Arbeits- und Abnahme-Issues **#9 bis #37**.

## Verwendung

1. Die passende Datei `Issue#XX.md` oeffnen.
2. Den Abschnitt **Fertiger Auftrag zum Kopieren** an den Coding-Agenten geben.
3. Der Agent muss zuerst den aktuellen GitHub-Status, Abhaengigkeiten, Repository-Regeln und Spezifikationen lesen.
4. Der Agent legt vor Codeaenderungen einen Plan vor.
5. Pro Issue entsteht hoechstens ein eigener Branch und ein kleiner PR.
6. Der Agent mergt den PR nicht selbst.

## Wichtige Statusregeln

- `READY`: Umsetzung darf nach Planpruefung beginnen.
- `PLANNED_SPEC_PENDING`: nicht automatisch beginnen; Live-Status und Abhaengigkeiten pruefen.
- `BLOCKED_HARDWARE`: keine erfundenen Hardwarewerte oder Testresultate; ohne reale Hardware kein Abschluss.
- `TBD_COMMISSIONING`: keine Parameter oder Grenzwerte ohne reale Messreihe festlegen.
- `COMPLETED`: nicht erneut implementieren; nur bei ausdruecklichem Auftrag auf Regressionen pruefen.

## Struktur

- `Issue#09.md` bis `Issue#28.md`: Software-first Implementierung
- `Issue#29.md` bis `Issue#33.md`: reale Hardwareintegration
- `Issue#34.md` bis `Issue#37.md`: Inbetriebnahme und Release-Abnahme

Die GitHub-Issues und die kanonische Dokumentationsprioritaet in `docs/SPECIFICATION_REVIEW.md` bleiben immer massgebend. Die Dateien dieses Pakets sind bewusst so formuliert, dass veraltete Snapshots nicht ungeprueft umgesetzt werden.
