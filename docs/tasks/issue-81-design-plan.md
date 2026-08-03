# Plan: ManuEngineer-Branding, Header und Boot-Splash festlegen

## Status

`PLAN_DRAFT_PENDING_OWNER_APPROVAL`

## Issue

#81

## Ziel

Die in PR #80 verabschiedete rendererunabhaengige Device-UI-Architektur um eine konkrete, aber weiterhin rendererfreie R1-Designbasis ergaenzen.

## Planumfang

1. Die drei vom Owner gelieferten SVG-Dateien als unveraenderte Masterquellen unter `assets/branding/source/` versionieren.
2. `docs/DEVICE_UI_VISUAL_DESIGN.md` als verbindliche visuelle R1-Spezifikation anlegen.
3. Folgende Ownerentscheidungen festhalten:
   - Header 32 px;
   - lange Logo-Variante, 24 px hoch, proportional etwa 168 px breit;
   - kompakte rechte Headerzone fuer Sprache, WLAN und Uhrzeit;
   - Boot-Splash maximal etwa 300 x 122 px, zentriert;
   - etwa drei Sekunden, nicht blockierend;
   - prozedurale Animation aus einem Asset, keine Framefolge;
   - statischer Splash als verpflichtender Ressourcenfallback.
4. Brandfarben aus den SVG-Mastern als Anker erfassen und semantische Theme-Tokens definieren, ohne Safetyfarben ungeprueft aus dem Logo abzuleiten.
5. Konvertierungs-, Ressourcen- und Herkunftsvertrag dokumentieren.
6. Noch keine generierten Firmwareassets, Bibliotheken oder Produktcode einfuehren.

## Nicht-Ziele

- keine LVGL- oder Rendererwahl;
- kein SVG-Laufzeitrendering;
- keine Display-, Touch-, DMA-, SPI- oder Pufferimplementierung;
- keine C-Arrays, PNG-Duplikate oder RGB565-Binaerdateien;
- keine Animation ohne spaetere Messung;
- keine Aenderung an PR #80.

## Ressourcenannahme und Gate

Die Groessenrechnung ist nur eine Obergrenzenorientierung:

- Splash 300 x 122 RGB565: 73 200 Byte;
- Header 168 x 24 RGB565: 8 064 Byte.

Es werden keine Vollbildframes gespeichert. #31 muss spaeter Flash, statisches RAM, freien und groessten Heapblock, Render-/Framezeit, SPI/DMA, Bootzeit und Fehlerverhalten real messen. Bei Budgetverletzung ist der statische Splash ohne Designverlust der verbindliche Fallback.

## Vorgesehene Dateien

- `assets/branding/source/ManuEngineer.svg`
- `assets/branding/source/ManuEngineer-short.svg`
- `assets/branding/source/ManuEngineer-Boot-splash.svg`
- `docs/DEVICE_UI_VISUAL_DESIGN.md`
- `docs/tasks/issue-81-design-plan.md`

## Pruefungen nach Planfreigabe

- SVG-XML parsebar und ViewBoxen unveraendert;
- keine externen Referenzen, eingebetteten Rasterbilder oder Skripte;
- Farb- und Dimensionsinventar reproduzierbar;
- relative Markdown-Links;
- Markdown-Tabellen;
- Schweizer Schreibweise;
- Secrets- und Architekturpruefung;
- `git diff --check`;
- CI auf exakt dem Implementierungs-Head.

## Freigabegate

Vor jeder weiteren Umsetzung ist folgender commitgebundener Kommentar notwendig:

```text
PLAN APPROVED
Approved plan commit: <commit-sha>
```

Bis dahin bleibt der PR Draft und enthaelt nur diesen Plan. Kein Ready for Review, Merge oder Auto-Merge.
