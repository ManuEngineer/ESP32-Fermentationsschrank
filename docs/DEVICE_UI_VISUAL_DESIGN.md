# Device-UI Visual Design

## Status und Geltungsbereich

**Status:** Verbindliche rendererunabhaengige R1-Designbasis aus Issue #81;
produktive Umsetzung bleibt den abhaengigen Issues vorbehalten.

Dieses Dokument konkretisiert nur Assetquelle, semantisches Theme, Header- und
Splashgeometrie sowie Ressourcen- und Fallbackvertraege fuer das 320 x 240 px
Touchdisplay. Es entscheidet weder Renderer noch Treiber, Firmwareformat oder
Produktcode.

## Referenzen

- [Issue #81](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/issues/81)
  legt diese Designbasis fest.
- [Draft-PR #83](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/pull/83)
  versioniert Masterassets und Spezifikation.
- [PR #80](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/pull/80)
  integrierte die Device-UI-Architekturentscheidungen UI-01 bis UI-24.
- [ADR-019](DECISIONS.md#adr-019-rendererunabhaengige-device-ui-shell) definiert
  die rendererunabhaengige Shell sowie Branding- und Themegrenze.
- Die ausfuehrliche Ownerquelle bleibt
  [DEVICE_UI_ARCHITECTURE_DECISIONS.md](DEVICE_UI_ARCHITECTURE_DECISIONS.md).

## Masterassets, Herkunft und Rechte

Alle SVGs wurden vom Repository-Owner als projektspezifische Originalassets
bereitgestellt. SVG bleibt die einzige Masterquelle. Dieser PR vergibt keine
allgemeine Drittverwendungslizenz; die Assets sind keine frei lizenzierten
Drittkomponenten.

| Repositorypfad | Rolle | ViewBox | Seitenverhaeltnis | Groesse | SHA-256 |
|---|---|---:|---:|---:|---|
| assets/branding/manuengineer/ManuEngineer.svg | Verbindliches R1-Headerlogo, lange Variante | 557.17199 x 79.599998 | 7.0000:1 | 12 242 Byte | b35788628e5cbda7a82552d5b6969c34a813494392ef9545d69dae4489b1484b |
| assets/branding/manuengineer/ManuEngineer-short.svg | Dokumentierte kompakte Alternative, nicht R1-Standard | 493.17199 x 79.599998 | 6.1956:1 | 14 822 Byte | 6ef8379556bcab6d9ab8154e15bb56dab203a8ed10cb8108519d21719e2eb440 |
| assets/branding/manuengineer/ManuEngineer-Boot-splash.svg | Verbindliches R1-Boot-Splash-Masterasset | 150 x 60.779152 | 2.4680:1 | 16 930 Byte | f53eabc988f8117fb93c20bc55ac963cc967c4e010b71eed9d02eef6b5189d19 |

Die Umbenennung der dritten Datei entfernt nur das Leerzeichen im
Repositorynamen. Jede Datei ist bytegleich mit der Owner-Masterdatei.

Die SVGs enthalten Inkscape- und sodipodi-Editorinformationen. Sie werden nicht
still entfernt, weil die Masters unveraendert bleiben muessen. Eine spaetere
Optimierung ist nur als reproduzierbarer Buildschritt mit Toolversion,
Eingabe-/Ausgabehash und Grössenvergleich zulaessig.

Alle drei Dateien wurden als XML geparst. Sie besitzen eine ViewBox und enthalten
keine Skripte, externen Referenzen, eingebetteten Rasterbilder oder
Base64-Nutzlasten.

## Header und Shell

Der normale Header ist 32 px hoch. Die vier festen Bottom-Slots und alle
Shellregeln bleiben bei ADR-019 und der Ownerquelle; diese Spezifikation
verdoppelt sie nicht.

| Bereich | Geometrievertrag bei 320 x 240 px | Regel |
|---|---|---|
| Langes Headerlogo | sichtbarer Rahmen x=4, y=4, 168 x 24 px | ManuEngineer.svg ist bei 24 px Hoehe proportional rund 168 px breit; nie strecken, stauchen oder neu zeichnen. |
| Rechte Headerzone | x=176..316 innerhalb der 32-px-Headerhoehe | reserviert fuer kompakte Sprache, WLAN und Uhrzeit ohne Sekunden; sichtbare Elemente duerfen nicht in den Logorahmen ragen. |
| Sprache | kompakt, beispielsweise DE ▾ | aktive Sprache; sichtbarer Text und Hit-Target sind getrennte Konzepte. |
| WLAN | Symbol | ausreichend grosses, nicht ueberlappendes unsichtbares Touchziel; keine dauerhafte Buttonoptik. |
| Uhrzeit | Text ohne Sekunden | Anzeige oder vorhandene Zeiteinstellung gemaess Shellvertrag. |

Die lange Variante ist R1-Standard. Ergibt die spaetere reale Messung, dass sie
mit der rechten Headerzone nicht robust passt, darf der Renderer nicht
eigenmaechtig auf die kurze Variante wechseln; dies erfordert einen Ownerentscheid.

## Boot-Splash, Startstatus und Fallback

Das Splash-Master wird proportional in einen Zielrahmen von ungefaehr
300 x 122 px gesetzt: Bei 300 px Breite ergibt die tatsächliche ViewBox 121.5583
px Hoehe. Es liegt horizontal bei etwa x=10 und vertikal zentriert. Der
Zielrahmen rechtfertigt weder Abschneiden noch Verzerrung; reale Hardwaretests
duerfen ihn innerhalb des verfuegbaren Rahmens proportional korrigieren.

Die beabsichtigte Gesamtdauer betraegt ungefaehr drei Sekunden und blockiert
keine Plattforminitialisierung:

~~~text
bereit nach Splashdauer       Splash -> Home
noch nicht bereit             Splash -> kompakter Startstatus
kritischer Startfehler        Splash/Startstatus -> aktorfreier Recovery-/Servicepfad
~~~

Eine spaetere Animation nutzt nur dieses vorbereitete Bild oder wenige getrennte
Bestandteile mit berechneten Renderzustaenden, zum Beispiel Sichtbarkeit,
Clipping/Reveal, Position oder optionale Alpha. Ein kurzer Akzentlauf ist nur
zulaessig, wenn er ohne Vollbildfolge passt. Alpha ist weder vorentschieden noch
erforderlich. Video, GIF und gespeicherte Vollbild-Framefolgen sind verboten.

Der verbindliche Fallback ist ein statischer, zentrierter Splash aus demselben
Masterasset. Er haengt nicht von Animation, Alpha oder mehreren Puffern ab.

## ManuEngineer Dark Theme

Release 1 enthaelt genau dieses ManuEngineer Dark Theme. Screens und Apps
referenzieren nur semantische Token, niemals verteilte Hexwerte. Branding- und
Statusfarben bleiben getrennt. Ein unvollstaendiges oder ungueltiges Theme faellt
fail-safe auf das vollstaendige eingebaute Standardtheme zurueck. Branding ist in
Release 1 keine Laufzeitwahl.

| Token | Wert | Herkunft | Zweck |
|---|---|---|---|
| background | #1C1712 | aus Branding uebernommen: dunkle Logoform | Grundflaeche |
| surface | #2A231B | aus Branding abgeleitet: aufgehellte dunkle Grundflaeche | Karten und normale Container |
| surface_elevated | #362D23 | aus Branding abgeleitet: zweite dunkle Flaechenstufe | Dialoge und erhoehte Bereiche |
| surface_interactive | #443728 | aus Branding abgeleitet: gedrueckte/interaktive Flaeche | sichtbares Pressfeedback |
| border | #6C5A43 | aus Branding abgeleitet: gedämpfte Akzentmischung | Trennung und Fokusumfeld |
| text_primary | #FBF7EF | aus Branding uebernommen: helle Logoform | primaerer Text |
| text_secondary | #D9D2C7 | aus Branding abgeleitet: gedämpftes Hell | sekundaerer Text |
| text_disabled | #A69D91 | aus Branding abgeleitet: gedämpftes Hell | deaktivierter Text |
| brand_primary | #D89A3B | aus Branding uebernommen: Gold | Marke und primaere Betonung |
| brand_accent | #9A7C4E | aus Branding uebernommen: Bronze | untergeordnete Markenbetonung |
| focus | #F4C06E | ergaenzende funktionale Designfarbe, aus Goldhelligkeit abgeleitet | Fokusindikator |
| selection | #5D421F | aus Branding abgeleitet: dunkle Goldflaeche | Auswahlhintergrund |
| success | #5DBA82 | ergaenzende funktionale Designfarbe | positiver Status |
| warning | #E7AE57 | ergaenzende funktionale Designfarbe, nicht brand_primary | Warnung |
| error | #E36B6B | ergaenzende funktionale Designfarbe | Fehler |
| critical | #FF9B9B | ergaenzende funktionale Designfarbe | kritischer Fehler |
| info | #72B9E8 | ergaenzende funktionale Designfarbe | Information |
| disabled_surface | #252019 | aus Branding abgeleitet: abgedunkelte Grundflaeche | deaktivierter Container |
| disabled_content | #978D80 | aus Branding abgeleitet: gedämpftes Hell | deaktivierte Symbole und Inhalte |
| overlay | #000000B3 | ergaenzende funktionale Designfarbe: 70-%-Schwarz | Modal- und Recoveryueberlagerung |

Gold ist keine allgemeine Warnfarbe. Warnung, Fehler und kritisch benoetigen
zusätzlich Icon und Text; Farbe ist nie der einzige Informationstraeger.

### Rechnerische Kontrastbetrachtung

Die Werte sind WCAG-Kontrastverhaeltnisse mit sRGB-Relativluminanz. Sie sind ein
fruehes Designgate und ersetzen nicht die reale Abnahme von Lesbarkeit,
Helligkeit und Blickwinkel in #31.

| Paar | Mindestwert | Berechnetes Verhaeltnis | Bewertung |
|---|---:|---:|---|
| text_primary auf background | 4.5:1 | 16.65:1 | bestanden |
| text_secondary auf surface | 4.5:1 | 10.33:1 | bestanden |
| text_disabled auf disabled_surface | 4.5:1 | 6.04:1 | bestanden |
| brand_primary auf background | 3:1 | 7.29:1 | bestanden |
| focus auf background | 3:1 | 10.68:1 | bestanden |
| success, warning, error, critical, info auf background | 3:1 | 5.57:1 bis 8.97:1 | bestanden |

## Ressourcenmodell und spaetere Renderergrenze

Die folgenden Werte sind theoretische unkomprimierte RGB565-Pixeldaten, keine
Firmwareverbrauchsgarantie:

| Assetziel | Rechnung | Referenzwert |
|---|---:|---:|
| Splash | 300 x 122 x 2 Byte | 73 200 Byte |
| Headerlogo | 168 x 24 x 2 Byte | 8 064 Byte |

Vor Release prueft #31 zusaetzlich Flash, statisches RAM, freien Heap, groessten
zusammenhaengenden Heapblock, Displaypuffer, DMA-Faehigkeit, SPI- und
Uebertragungszeit, CPU- und Framezeit, Bootzeit, Alignment, Assetmetadaten und
eine moegliche Transparenzmaske. Die 4-MB-Flash- und ohne-PSRAM-Grenze bleibt
verbindlich. Bei Ressourcen-, Renderer- oder Messproblemen bleibt der statische
Splash der harte Fallback.

SVG wird nicht auf dem ESP32 zur Laufzeit gerendert. Der spaetere, testbare
Konvertierungsvertrag lautet:

~~~text
SVG-Master
-> deterministische Build-Konvertierung
-> rendererabhaengiges Zielformat
-> Groessen-/Hash-/Ressourcennachweis
~~~

Generated Assets sind keine manuell gepflegten Parallelquellen. Dieses Dokument
entscheidet weder PNG, RGB565-C-Arrays, LVGL-Imageconverter noch eine konkrete
Bibliothek. Renderer-, Flush-, Puffer-, DMA-, Treiber- und Displayentscheidungen
bleiben in #31 FINAL_SELECTION_PENDING beziehungsweise TBD_HARDWARE.

## Architektur-, SOLID-, DRY- und KISS-Grenzen

- **Single Responsibility:** Masterassets, Themevertrag, Layoutgeometrie und
  spaetere Rendererumsetzung sind getrennt.
- **Open/Closed:** Weitere Brandings oder Themes werden ueber Assets und
  Konfiguration ergaenzt, nicht durch Screenumbauten.
- **Liskov und Dependency Inversion:** UI- und Appvertraege kennen nur
  Asset-IDs und semantische Token, keine Renderer-, Treiber- oder Dateiformatdetails.
- **Interface Segregation:** Keine grosse Design-/Renderer-Allzweckschnittstelle.
- **DRY:** SVGs sind die einzigen Masters; Farbwerte und Geometrie stehen zentral
  hier und werden sonst nur referenziert.
- **KISS:** ein langes Headerlogo, eine dokumentierte kurze Alternative, ein
  Splashmaster, ein Dark Theme und ein statischer Fallback; kein Asset-Manager
  ohne zweiten Anwendungsfall.

## Offene technische Punkte

- Reale Lesbarkeit, Touchhitboxen, Helligkeit, Blickwinkel, Displaycontroller,
  Touchcontroller und Kalibrierung bleiben #31 und TBD_HARDWARE.
- Rendererwahl, Assetzielformat, Konvertierungstool, Transparenzstrategie,
  Puffer-, DMA- und Ressourcenmessung sind FINAL_SELECTION_PENDING.
- Die reale Messung entscheidet ueber optionale Alpha oder Akzentlauf; der
  statische Fallback bleibt davon unabhaengig.
- Ein Wechsel zur kurzen Headervariante bei Platzkonflikt ist ein Ownerentscheid.

## Akzeptanzkriterien

- Alle drei Owner-SVGs liegen bytegleich unter den dokumentierten
  Repositorypfaden und bestehen XML-, ViewBox-, Sicherheits- und Hashpruefung.
- Das lange Logo ist als R1-Standard mit proportionaler 168-x-24-px-Geometrie
  dokumentiert; die rechte Headerzone bleibt reserviert.
- Der Splash ist proportional, zentriert, nicht blockierend und besitzt den
  statischen Fallback aus demselben Master.
- Jedes Theme-Token besitzt einen konkreten Wert und nachvollziehbare Herkunft;
  funktionale Statusfarben sind klar von Brandingfarben getrennt.
- Wesentliche Text-/Hintergrund- und Statuskontraste sind rechnerisch belegt.
- #25 kann Asset-IDs und semantische Token als rendererfreie Vertraege nutzen,
  #26 die Geometrie im Simulator validieren und #31 Konvertierung sowie reale
  Ressourcen- und Hardwaregates pruefen.
- Es wurden kein Produktcode, Renderer, Treiber, Font, generiertes Firmwareasset
  oder neue Bibliothek eingefuehrt.

## Nicht-Ziele

- kein SVG-Laufzeitrendering auf dem ESP32;
- kein PNG als verbindliches Firmwareformat;
- keine RGB565-C-Arrays, Kompression oder Laufzeitdekompression;
- keine LVGL-, LovyanGFX-, TFT_eSPI- oder andere Treiberintegration;
- keine Displaypuffer-, DMA-, GPIO-, Touch- oder konkrete Animationsimplementierung;
- keine Laufzeitwahl des Brandings in Release 1.
