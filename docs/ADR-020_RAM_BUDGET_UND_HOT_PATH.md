# ADR-020: RAM-Budget als Gate und allokationsfreier Hot Path

> Vorschlag. Nach Ownerfreigabe wird der Eintrag in das ADR-Register
> `docs/DECISIONS.md` uebernommen (siehe Plan, Abschnitt 9).

- **Status:** proposed
- **Datum:** 2026-10-02
- **Kontext:** Der freie Heap fiel ueber #31, #164 und #27 unbemerkt von
  231 KB auf 12 KB (Minimum 2,5 KB); der PR-#170-Retest endete mit einem
  Out-of-Memory-Abort im UI-Renderpfad. Fruehe Subsystembudgets waren nur
  Planungswerte ohne automatische Pruefung. Die Hauptschleife allokiert
  alle 10 ms Kopien von Katalog, Snapshot und Screen-Modell.
- **Entscheidung:** Jedes Subsystem (Kern+App, UI, Netzwerk, Web) erhaelt pro
  Hardware-Profil ein RAM-Budget in KB und eine Mindestreserve. Statischer
  DRAM wird in CI gegen das Budget geprueft. Im Ruhezustand allokieren
  Hauptschleife, Render-Pfad und Request-Handler keinen Heap; erlaubt sind
  Allokationen beim Boot und bei seltenen Ereignissen (Seitenwechsel,
  Konfigurations-Commit). Ein zaehlender `operator new` im Host-Test prueft
  das. Kapazitaeten sind feste Profilkonstanten; volle Container liefern
  einen definierten Fehler. Plaene und PR-Beschreibungen nennen ihre
  geschaetzte RAM-Wirkung.
- **Alternativen:** Budgets weiter nur als Planungswerte; Optimierung erst
  bei Problemen; groessere Hardware als Pflicht.
- **Folgen:** Umsetzung in Etappen gemaess
  [`tasks/memory-platform-course-plan.md`](tasks/memory-platform-course-plan.md).
  `RESOURCE_BUDGET_AND_MAINTENANCE.md` fuehrt die Budgettabelle. Ein PR,
  der das Budget ueberschreitet, wird nicht gemergt.
