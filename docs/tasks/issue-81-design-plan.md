# Plan: ManuEngineer-Branding, Header und Boot-Splash festlegen

## Status

`IMPLEMENTED_PENDING_OWNER_REVIEW`

## Issue

#81

## Ziel

Die in PR #80 verabschiedete rendererunabhaengige Device-UI-Architektur um eine konkrete, aber weiterhin rendererfreie R1-Designbasis ergaenzen.

## Planumfang

1. Die drei vom Owner gelieferten SVG-Dateien als unveraenderte Masterquellen unter `assets/branding/manuengineer/` versionieren.
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

- `assets/branding/manuengineer/ManuEngineer.svg`
- `assets/branding/manuengineer/ManuEngineer-short.svg`
- `assets/branding/manuengineer/ManuEngineer-Boot-splash.svg`
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

Vor der Freigabe blieb der PR Draft und enthielt nur diesen Plan. Auch nach der
Umsetzung bleiben Ready for Review, Merge und Auto-Merge untersagt, bis ein
unabhaengiges Ownerreview abgeschlossen ist.

## Umsetzung und Planabschluss

Die commitgebundene Ownerfreigabe fuer
`9d4f484eff43f9969e888617e2e9ae8c41ca8978` liegt vor. Die aktuelle
Ownervorgabe praezisiert den im Plan noch generischen Assetpfad
`assets/branding/source/` auf `assets/branding/manuengineer/`; diese
Pfadpraezisierung erweitert weder Produkt- noch Architekturscope.

- `4b81d81` versioniert ausschliesslich die drei bytegleichen SVG-Masterassets.
- `2caebdf` fuegt ausschliesslich die rendererunabhaengige visuelle
  Designspezifikation hinzu.
- Dieser Planabschluss aktualisiert Status, Zielpfade und Nachweisreferenzen.

Kein Produktcode, Renderer, Treiber, generiertes Firmwareasset, Font,
Bibliothek, GPIO- oder Hardwareentscheidung wurde eingefuehrt. Der PR bleibt
bis zu einem unabhaengigen Ownerreview Draft.
