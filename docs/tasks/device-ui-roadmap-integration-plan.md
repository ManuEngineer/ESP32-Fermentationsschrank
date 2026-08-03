# Plan: Device-UI-Entscheidungen in Spezifikation und Roadmap integrieren

## Status

`IMPLEMENTED_PENDING_OWNER_REVIEW`

Verbindliche Quellentscheidung: [`DEVICE_UI_ARCHITECTURE_DECISIONS.md`](../DEVICE_UI_ARCHITECTURE_DECISIONS.md).

Dieser Plan aendert weder Produktcode noch Abhaengigkeiten, Renderer, GPIOs,
Treiber, Bibliotheken oder Hardwareannahmen. Die in ihm enthaltenen
Issue-Body-Entwuerfe wurden erst nach der commitgebundenen Ownerfreigabe auf
GitHub uebernommen.

## Ziel und Nicht-Ziele

Ziel ist die widerspruchsfreie Einordnung von UI-01 bis UI-24 in die bestehende
Spezifikation sowie der testbare Zuschnitt von #25, #26 und #31. Touch und Web
teilen fachliche Projektionen und Commands, behalten aber getrennte Navigation,
Layout und Sitzungskontexte.

Nicht-Ziele sind UI-Implementierung, LVGL-Auswahl oder -Einbindung,
Treiber-/Controllerannahmen, GPIO-Entscheidungen, neue Font- oder Assetdateien,
neue Service- oder Web-Sicherheitsgrenzen und ein neuer Issue ohne
Ownerentscheidung.

## Ausgangslage und Quellen

- #25 und #26 sind `PLANNED_SPEC_PENDING`; #31 ist `BLOCKED_HARDWARE`.
- #25 haengt von #12, #14, #15, #20 und #24 ab. #20 und #24 sind noch offen.
- #31 haengt von #25, #26 und #29 ab; die ESP-IDF-6.0.2-Umstellung aus #74 / PR
  #79 ist beim Planstand noch nicht nach `main` gemergt.
- Die produktive ESP32-Plattform ist erst nach #74 verbindlich ESP-IDF 6.0.2;
  #31 darf keinen Arduino-Rueckfall einplanen.
- Verbindliche Quellen: `AGENTS.md`, `docs/SPECIFICATION_REVIEW.md`,
  `docs/DECISIONS.md`, `docs/ARCHITECTURE.md`, `docs/ADOPT_OR_BUILD.md`,
  `docs/HARDWARE.md`, `docs/SETTINGS_AND_STORAGE.md`, `docs/ACCEPTANCE_TESTS.md`,
  die fuenf UI-/Web-Spezifikationen sowie die Hardware- und
  Komponenten-Audits.

## Konfliktmatrix

| Entscheidung | Befund | Betroffene Artefakte | geplante Bereinigung und Wirkung |
|---|---|---|---|
| UI-01 Shell | widerspruechlich | `LOCAL_UI.md`, #25, #26 | feste Header-/Bottom-Shell und enge Vollbildausnahmen definieren; ersetzt keine Fachlogik |
| UI-02 Home/Zurueck | teilweise | alle lokalen UI-Dokumente, #25/#26 | Hierarchie und Draft-/Sicherheitsnavigationsschutz als gemeinsamen Vertrag ergaenzen |
| UI-03 vier Slots | widerspruechlich | `LOCAL_UI.md`, `LOCAL_UI_PROGRAMS.md`, #26 | Aussage gegen feste Leiste entfernen; genau vier Slots und rechte Auf/Ab-Navigation normieren |
| UI-04 Status/Service | teilweise | `LOCAL_UI_SETTINGS_SERVICE.md`, #25/#26 | Plattformseiten plus fehlerisolierte App-Erweiterungspunkte festlegen |
| UI-05 Branding/Sprachen | teilweise | Local-/Web-UI, #25/#31 | ManuEngineer-Buildbranding, Namespaces, Buildpakete, EN->Key-Fallback und Fontdeklaration festlegen |
| UI-06 Themes | fehlt | #25/#26/#31, Einstellungen | semantische Tokens, Buildmenge, Laufzeitauswahl und Dark-Default aufnehmen |
| UI-07 Renderer | teilweise | Adopt-or-build, #31 | LVGL bevorzugt, aber `FINAL_SELECTION_PENDING`; keine Typen ausserhalb Adapter |
| UI-08 Modelle/Commands | teilweise | #25, #26, `WEB_UI.md` | Snapshot-/Update-, Command-Ergebnis- und Bestaetigungsvertrag zentralisieren |
| UI-09 Komponenten | fehlt | #25/#26 | kleines rendererunabhaengiges Komponenten-/Layoutvokabular statt Widgetframework |
| UI-10 Touch | teilweise | Local UI, #26/#31 | 44x40-Richtwert, Pressfeedback, keine Pflichtgesten, Entprellung und sichtbare Sperrgruende |
| UI-11 Header | fehlt | #25/#26 | Sprach-, WLAN-, Zeitinteraktion, ruhige Optik und kritische Sperren definieren |
| UI-12 Start | teilweise | Local UI, #26/#31 | Splash, nichtblockierender Startstatus, kein frueher Doppelpfad |
| UI-13 Meldungen/Recovery | teilweise | Runtime UI, Service UI, #25/#26 | gemeinsame Prioritaets-/Dialogmodelle; aktorfreier Diagnose-/Recoveryzugang bleibt erhalten |
| UI-14 Home-Prioritaet | teilweise | Local/Runtime/Web UI, #25/#26 | Reihenfolge und 320x240-Sprachlayoutvalidierung festlegen |
| UI-15 Home-Modi | teilweise | Local/Runtime/Web UI, #25/#26 | typisierte Modi statt rendererabgeleiteter Plausibilitaet |
| UI-16 Primaeraktion | teilweise | #15, lokale UI, #26 | erster App-Slot inkl. Vorheizen/Start/Stop/Fortsetzen/Abschluss und strukturierter Bestaetigung |
| UI-17 zweiter Slot | fehlt | #26 | appgelieferter zweiter Slot; Fermentation: Rezepte und Snapshotschutz |
| UI-18 Status/Service-Navigation | fehlt | Service UI, #25/#26 | vier Slots auch dort, Plattform vor Apps, tiefe Ebene gemass UI-02 |
| UI-19 Service-Schutz | teilweise | Service-/Web-UI, #26/#27 | Freier, bestaetigter und PIN-geschuetzter Bereich; Safety bleibt separates Gate |
| UI-20 PIN-Parameter | widerspruechlich | Service UI, `SETTINGS_AND_STORAGE.md` | lokale Inaktivitaet: zentraler Build-/Produktparameter, initial 10 min, nicht per UI aenderbar, keine R1-Maximaldauer |
| UI-21 Servicesitzung | widerspruechlich | Service-/Web-UI, #26/#27 | getrennte Touch-/Web-Sitzungen; Touch nur inaktivitaetsbasiert, Web verbindlich 5 min inaktiv plus 15 min absolut |
| UI-22 Ruhezustand | teilweise | Local UI, #26/#31 | Plattformvertrag, erster Touch nur wake, Werte hardwareabhaengig |
| UI-23 Feedback | teilweise | Runtime UI, #26/#32 | semantische Plattformmuster, App-Intent, Touchton standardmaessig aus |
| UI-24 nur Touch | teilweise | Hardware, Audits, #26/#31 | Ausschluss physischer Controls zentral dokumentieren; keine erfundene Resetgeste |

## Gefundene Widersprueche und Aufloesung

1. `LOCAL_UI.md` sagt, eine feste Leiste werde nicht vorausgesetzt. Das wird
   durch UI-01/UI-03 ersetzt; die vier Slots sind stets sichtbar, leer bleibende
   Slots werden nicht verbreitert.
2. Die Anzeige nennt heute `Luft`/`Schrankluft`. Die UI verwendet konsistent
   `Schrank` fuer die benutzernahe Temperaturanzeige und behält
   `Schrankluftfuehler` fuer die technische Sensorrolle. `Produkt` bleibt
   unveraendert.
3. Lokale Service-Inaktivitaet ist keine normale Serviceeinstellung. Sie ist
   ein zentraler Build-/Produktparameter mit initial 10 Minuten, nicht ueber
   die UI aenderbar und ohne absolute Maximaldauer in Release 1. Die lokale
   Freigabe endet bei Neustart, explizitem Abmelden, Inaktivitaetsablauf und
   definierten sicherheitsrelevanten Zustandswechseln.
4. Deutsch als Fallback wird fuer Touch und Web durch `aktive Sprache ->
   Englisch -> sichtbarer technischer Schluessel` ersetzt. Touch-Sprache bleibt
   persistent geraetebezogen, Web-Sprache browser-/sitzungsbezogen.
5. Die 5-Minuten-Inaktivitaet und 15-Minuten-Absolutgrenze im Web werden als
   strengere netzwerkseitige Securitygrenze bewusst unveraendert beibehalten.
   Touch und Web verwenden getrennte Sessions und werden in Release 1 nicht
   vereinheitlicht.
6. `SAFE_BOOT` bekommt keinen normalen PIN-Service. Er erhaelt nur den
   reduzierten, aktorfreien Diagnose-/Recoverybereich. Dies praezisiert, statt
   die bestehende Sperre zu lockern.
7. PIN-unabhaengiger Vollreset und Kalibrierungs-Recovery bleiben verschiedene
   Zwecke. Beide benoetigen einen praktisch nachgewiesenen Raw-Touch-Vertrag;
   Geste, Schwellen und Verwechselungsschutz bleiben `TBD_HARDWARE`/#31. Es
   wird keine neue Geste erfunden.
8. Das bestehende 10-Sekunden-Fenster bedeutet nur
   Touchkalibrierungs-Recovery. Es darf nicht den Splash ersetzen und keinen
   fruehen separaten Grafikpfad begruenden.

## Repositoryweite Pruefung physischer Bedienelemente

| Fundklasse | Artefakte | Klassifikation | Massnahme |
|---|---|---|---|
| verbindlicher Ausschluss | `docs/audits/OPEN_BACKLOG_CLASSIFICATION.md`, `docs/audits/RELEASE_1_FUNCTION_MATRIX.md`, `docs/audits/PROPOSED_RELEASE_1_ROADMAP.md`, `docs/audits/HARDWARE_SPIKE_PLAN.md`, `docs/audits/COMPONENT_EVALUATIONS.md`, `docs/audits/RELEASE_1_ADOPT_OR_BUILD_AUDIT.md` | aktuell und korrekt: Encoder, Programmwahlschalter, Start-/Stop-Taster und Status-LED gehoeren nicht zu R1 | mit UI-24 gegen `HARDWARE.md`, lokale UI und Issue #31 zentral verlinken; keine Ports, Pins oder Ersatzabläufe hinzufuegen |
| Summer | `docs/HARDWARE.md`, `docs/audits/RELEASE_1_FUNCTION_MATRIX.md`, #32 | aktuell: einziger weiterer lokaler Ausgabekanal, Hardwaredetails offen | semantische UI-Signale aus UI-23; reale Rolle/Pegel/Hardwaretest bleiben #32 |
| Start/Stop-Text | `Agent-Auftraege/.../Issue#32.md` | nicht physisch: beschreibt Aktor-Testablauf Start/Stop/Nachlauf | keine Korrektur |
| `Encoder` in Code/Persistenz | `CHANGELOG.md`, `docs/CONFIGURATION_PERSISTENCE.md`, `docs/tasks/issue-56-implementation-plan.md`, `docs/tasks/issue-57-implementation-plan.md`, `lib/**`, `test/**` | nicht physisch: Serialisierungsencoder | keine Korrektur |

Die Suche nach deutsch- und englischsprachigen Varianten von Taster, Button,
Encoder, Rotary, Selector, Switch und LED erzeugte keine produktive Annahme
eines lokalen Hardware-Bedienelements. `enabled`, `switch` und gleichlautende
Codebegriffe wurden als technische Fehlertreffer ausgeschlossen.

## Issue-Schnitt und Abhaengigkeitsgraph

```text
#20 Sensorqualitaet --+                      +--> #26 lokale Shell/Screens --+
#24 Safety/SAFE_BOOT ---+--> #25 UI contracts+                                 +--> #31 realer Renderer/Display/Touch
#12/#14/#15 Commands ---+                      +--> #27 Web (gemeinsame Vertr.) +
#29 reale Grundplattform --------------------------------------------------------+
#74 ESP-IDF-6.0.2 nach main -----------------------------------------------------+
#32 realer Pieper (semantischer Intent aus #25/#26) ----------------------------+
```

Verbindlicher Schnitt: **#25 wird nicht gesplittet.** #25 bleibt die einzige
Quelle fuer rendererunabhaengige gemeinsame Vertraege. Sein spaeterer
Implementierungsplan gliedert die Arbeit in kleine, testbare Commit-Schritte
(Contracts/Snapshots, Navigation/Locale/Theme, Tests). Ein Split darf erst nach
konkreter Aufwandsschaetzung und erneuter Ownerentscheidung vorgeschlagen
werden.

## Vorgeschlagene neue Bodies

### #25 – [E4.1] Gemeinsame UI-Modelle, Device-Shell-Vertraege und Mehrsprachigkeit

```markdown
## Status
`PLANNED_SPEC_PENDING`

## Epic und Abhaengigkeiten
- Epic #6
- #12, #14, #15, #20 und #24 abgeschlossen beziehungsweise als erforderliche
  fachliche Producer integriert; kein produktiver UI-Start vor ihren
  erforderlichen Contracts.

## Ziel
Rendererunabhaengige Device-UI-Vertraege fuer Touch und Web bereitstellen.
Die Vertraege projizieren kanonische Fach-, Safety-, Berechtigungs-, Sensor- und
Meldungsentscheidungen, leiten sie jedoch nicht neu her.

## Scope
- feste Device-Shell: Header (Branding, Sprache, WLAN, Uhrzeit), exakt vier
  kontextabhaengige Bottom-Slots, Home-/Zurueck-Hierarchie und sichtbare leere
  Slots;
- rendererunabhaengige Status-/Service-Erweiterungspunkte mit Plattform-vor-App
  und Fehlerisolation;
- vollstaendige, typisierte Snapshots/View-Modelle fuer Home-Modus, Navigation,
  Temperaturen/Qualitaet, Meldungen, Recovery, Status und Service;
- typisierte Commands, erwartete Revisionen, strukturierte Ergebnisse
  (angenommen, abgelehnt, Bestaetigung erforderlich, beschaeftigt,
  nicht verfuegbar), Bestaetigungsmodelle und Aktualisierungsvertrag;
- gemeinsame fachliche Verträge fuer Touch und Web ohne gleiches Layout oder
  gleiche Navigation zu erzwingen;
- Brandingvertrag mit ManuEngineer als R1-Standard, Build-Zeit-Auswahl
  enthaltener Brandings/Sprachen/Themes und Laufzeitwahl von Sprache/Theme;
- Plattform-/App-Textnamensraeume, DE/EN/ES-Sprachpakete, Fallback aktive
  Sprache -> Englisch -> sichtbarer technischer Schluessel, deklarierter
  Zeichensatz, Font-/Textlaengen- und 320x240-Layoutvertrag;
- semantische Theme-Tokens, Dark Theme als einziges R1-Theme und fail-closed
  Standardtheme-Fallback;
- zentraler UI-Sitzungsvertrag: lokale Touch-Servicefreigabe endet nach 10
  Minuten Inaktivitaet sowie bei Neustart, explizitem Abmelden und definierten
  sicherheitsrelevanten Zustandswechseln; keine lokale R1-Maximaldauer und
  keine UI-Aenderung dieses Build-/Produktparameters; Web bleibt als getrennte,
  strengere 5/15-Minuten-Policy spezifiziert;
- kleines Komponenten-/Layoutvokabular ohne Widget- oder Rendererframework;
- native Tests fuer Modi, Navigation, Fallbacks, Commands, Meldungen,
  Aktualisierung und alle Sprachpakete.

## Grenzen
- keine LVGL-/HTML-/Treiber-/GPIO-/Hardwaretypen und keine Rendererwahl;
- keine Pixelimplementierung, Screens, Touchrohwerte, Kalibrierung, Fonts,
  Assets oder reale Displayhardware;
- kein erneutes Entscheiden von Safety, Sensorrolle, Authentisierung,
  Persistenz oder Fachzustand.

## Abnahme
- Shell, vier Slots, Home/Zurueck und Erweiterungspunkte sind nativ testbar;
- Touch und Web koennen denselben Snapshot und dieselben Commands verwenden;
- der gemeinsame Vertrag unterscheidet lokale 10-Minuten-Inaktivitaet ohne
  Maximaldauer von der getrennten Web-5/15-Minuten-Policy; die durch #24
  definierten Sicherheitszustandswechsel sperren lokal fail closed;
- fehlende Texte sind nie still deutsch, sondern EN oder sichtbarer Key;
- unvollstaendiges Theme wird sicher auf Standard abgebildet;
- alle Contracts bleiben klein, stark typisiert und rendererunabhaengig.
```

### #26 – [E4.2] Lokale Touch-Shell und Fermentations-Workspace simuliert umsetzen

```markdown
## Status
`PLANNED_SPEC_PENDING`

## Abhaengigkeiten
- #25; #12, #14 und #15 fuer Programme, Zustaende und Commands;
- #20/#24 fuer vollstaendige Sensor-/Safety-/Recoveryprojektionen.

## Scope
- lokale Shell gegen #25, getrennt vom Fermentations-App-Workspace;
- Home-Modi bereit, aktiv, wartet, abgeschlossen, eingeschraenkt und Recovery;
- erster App-Slot fuer Vorheizen/Start/Stop/Fortsetzen/Bestaetigen/Abschluss,
  zweiter App-Slot `Rezepte`, Status-/Service-Unterseiten und App-Erweiterungen;
- exakt vier Bottom-Slots, Home-/Zurueck-Hierarchie sowie vertikale Listen-
  und Informationsnavigation mit rechten Auf/Ab-Tasten;
- resistive Touchinteraktion mit sichtbarem Pressfeedback, Sperrgruenden,
  Bestaetigungen und ohne notwendige Geste, Doppel-Tap oder Long-Press;
- integrierte Header-Touchflaechen fuer Sprache/WLAN/Uhrzeit ohne permanente
  Buttonoptik;
- Splash, nichtblockierender Startstatus, aktorfreie Recoveryanzeige, normale
  PIN-UI, eine lokale 10-Minuten-Inaktivitaetssitzung ohne absolute R1-
  Maximaldauer, explizites Abmelden und Sperre bei Neustart beziehungsweise
  definiertem sicherheitsrelevantem Zustandswechsel, UI-Ruhezustand und
  semantische Pieper-/Feedbackintents;
- vollstaendige Simulation ohne Displayhardware und 320x240
  Layout-/Screenshot-/Golden- oder gleichwertige Zustands-/Layouttests.

## Grenzen
- keine Renderer-/Treiber-/Bibliotheksauswahl, kein ILI9341/XPT2046-Annehmen,
  keine Touchrohwerte, Kalibrierungsmathematik, DMA-/Puffer- oder reale
  Backlight-Integration;
- keine physische Ersatzbedienung; Raw-Touch-Recovery bleibt #31 und
  `TBD_HARDWARE`;
- SAFE_BOOT bietet nur reduzierten aktorfreien Diagnose-/Recoveryzugang, keinen
  normalen PIN-Service oder Aktortest.

## Abnahme
- jeder lokale Bedienpfad ist simuliert pruefbar; erster Wake-Touch fuehrt kein
  Command aus; kritische Aktionen sind bestaetigt und nicht doppelt ausloesbar;
- lokale Servicefreigabe wird nach 10 Minuten ohne relevante Bedienhandlung,
  nach Abmelden, Neustart und jedem definierten sicherheitsrelevanten
  Zustandswechsel verworfen; Hintergrundupdates verlaengern sie nicht und es
  existiert keine absolute R1-Maximaldauer;
- kein Screen leitet Fach-/Safetyzustand aus Rohwerten ab; Programme aendern
  keinen aktiven Schnappschuss; alle Texte/Theme-Tokens stammen aus #25.
```

### #31 – [E5.3] Renderer, Display-/Touchadapter und Kalibrierung nach Hardwarebeweis

```markdown
## Status
`BLOCKED_HARDWARE`

## Abhaengigkeiten
- #25, #26, #29 sowie die nach `main` uebernommene ESP-IDF-6.0.2-Plattform aus
  #74; reale Hardware- und Pin-/Controllerbestaetigung.

## Scope
- ergebnisoffene Renderer-/Bibliotheksentscheidung mit LVGL als bevorzugtem,
  nicht vorentschiedenem Kandidaten; Lizenz, IDF-6.0.2-Kompatibilitaet,
  Ressourcenbaseline, Testbarkeit und Integrationsaufwand vergleichen;
- ILI9341 und XPT2046 praktisch nachweisen statt aus Lieferantenangaben
  anzunehmen;
- schmale Renderer-, Flush-, Tick-, Touch-, Backlight- und Assetadapter hinter
  #25/#26-Contracts; `device_platform` bleibt renderer- und appfrei;
- Flash-, Heap-, statisches-RAM-, DMA-, SPI- und Pufferstrategie mit realen
  Messwerten; gezielter Font-/Asset-Build fuer enthaltene Sprachen und Branding;
- reale 320x240-Lesbarkeits-, Touchziel-, Rotation-, Dimm- und Bedienbarkeitstests;
- persistente Touchkalibrierung, klar getrennte 10-Sekunden-Raw-Touch-
  Kalibrierungsrecovery und PIN-unabhaengiger Reset-Recoveryvertrag;
- erster Touch weckt nur Display, keine Doppelausloesung; Displayfehler
  beeinflussen niemals Regelung oder Aktorfreigabe.

## Grenzen
- kein separater frueher Grafikpfad ausser ein einfacher, robuster Nachweis
  zeigt ohne Doppelpfad einen Vorteil; Splash erst nach Rendererbereitschaft;
- keine unbestaetigten GPIOs, Controller oder Pegel, keine Safetyentscheidung
  und keine physische Control-Annahme.

## Abnahme
- Auswahlentscheidung mit Kandidatenmatrix, Lizenz-/Noticepruefung,
  Ressourcenvergleich und Ownerfreigabe; reale Hardwareprotokolle belegen
  Controller, Verdrahtung, Kalibrierung, Wake, Fehlerisolation und Recovery.
```

## Renderer-Evaluationsmatrix

| Kandidat | Rolle | Status | Pflichtnachweis |
|---|---|---|---|
| schlanker eigener Renderer ueber finalen Treiberadapter | Referenz | zu messen | gleicher repräsentativer Screen, 320x240, Ressourcen, Tests |
| LVGL 9.5.0 | bevorzugter UI-Framework-Kandidat | `FINAL_SELECTION_PENDING` | MIT/Abhaengigkeiten, IDF 6.0.2, ILI9341/XPT2046, Font/Asset, Flash/RAM/CPU |
| LovyanGFX, TFT_eSPI, LCDWiki | Display-/Touch-Stack-Kandidaten | `SPIKE_REQUIRED` bzw. Lizenzgate | realer Controller, Adaptervertrag, Lizenz-/Noticepruefung |
| Arduino_GFX / Adafruit GFX + ILI9341 / XPT2046_Touchscreen | Reserve | `NOT_SELECTED`/`SPIKE_REQUIRED` | nur bei dokumentiertem Ausloeser, identische Messung |

Kein Kandidat ist vor #31 ausgewaehlt. Die Evaluation misst Base gegen
Kandidaten: Flash, statisches RAM, freier/groesster Heapblock, CPU-/Framezeit,
SPI-/DMA-Verhalten, Boot-/Wake-/Fehlerverhalten sowie Testabdeckung.

## Dokumentaenderungen nach Freigabe

- `LOCAL_UI.md`: Shell, vier Slots, Schrankterminologie, Header, Home-Prioritaet,
  Touch-/Wake-Vertrag und Vollbildausnahmen.
- `LOCAL_UI_PROGRAMS.md`, `LOCAL_RUNTIME_UI.md`: Slot-/Pagernavigation,
  Primaer-/Rezepteaktion, Meldungs-/Recoverymodelle.
- `LOCAL_UI_SETTINGS_SERVICE.md`: Plattformservice, verbindliche lokale
  10-Minuten-Inaktivitaet als nicht per UI aenderbarer Build-/Produktparameter,
  keine R1-Maximaldauer, Sperrereignisse, SAFE_BOOT-Recoverytrennung,
  Sprachfallback.
- `WEB_UI.md`: gemeinsamer Key-/Fallbackvertrag; getrennte Browserlanguage und
  verbindlich getrennte, strengere Web-5/15-Policy klar dokumentieren.
- `HARDWARE.md`, `ACCEPTANCE_TESTS.md`, `SETTINGS_AND_STORAGE.md`,
  `IMPLEMENTATION_ISSUES.md`, `DECISIONS.md` und relevante Audits: Hardware-
  ausschluesse, Renderer-/Lizenz-/Ressourcengates, Issuegraph, Akzeptanztests
  und kompakter ADR-/Registereintrag mit Verweis auf die ausfuehrliche
  `DEVICE_UI_ARCHITECTURE_DECISIONS.md`.

## Risiken, Safety, Security und Ressourcen

- Alle Shell-/Recoverywege bleiben aktorfrei bis #24 und reale Hardwaregates;
  Service-PIN hebt keine Begrenzung auf.
- Die Web-Servicesitzung bleibt bewusst die strengere netzwerkseitige Policy
  von 5 Minuten Inaktivitaet und 15 Minuten absolut; sie wird nicht mit der
  lokalen 10-Minuten-Inaktivitaetssitzung vereinheitlicht.
- Keine Font-Vollunicode-, PSRAM-, Puffer- oder Assetannahme. Der Build muss
  je enthaltenem Sprach-/Branding-/Theme-Set Ressourcen messen.
- Jede spaetere Drittkomponente braucht Herkunft, exakte Version, Lizenz,
  Notice, transitive Pruefung und Ownerauswahl gemaess `ADOPT_OR_BUILD.md`.

## Verbindlich entschiedene Planparameter

- Touch-Service: 10 Minuten Inaktivitaet, zentraler Build-/Produktparameter,
  keine UI-Aenderung, keine absolute R1-Maximaldauer; Sperre bei Neustart,
  Abmelden, Inaktivitaet und definierten sicherheitsrelevanten
  Zustandswechseln.
- Web-Service: getrennte strengere Securitypolicy mit 5 Minuten Inaktivitaet
  und 15 Minuten absolut.
- #25 bleibt unsplittet; ein neuer Vorschlag verlangt konkrete Aufwandsdaten
  und eine erneute Ownerentscheidung.
- `DEVICE_UI_ARCHITECTURE_DECISIONS.md` bleibt die ausfuehrliche Quelle.
  `DECISIONS.md` erhaelt nach Planfreigabe einen kompakten Registereintrag mit
  Verweis auf diese Quelle.

## Umsetzungsnachweis

- Freigegebener Plan-Commit:
  `cfedbf2ca31e317a0869a9e474a68b93b01f321e`.
- Umsetzungscommit: `9b97595` – Spezifikations-, Roadmap-, Test- und
  Registerabgleich in zehn Dokumenten; kein Produktcode.
- Live-Issues aktualisiert: #25, #26 und #31, einschliesslich der im Plan
  vorgesehenen Titel, Abhaengigkeiten, Scopes, Grenzen, Tests und Quellen.
- `DEVICE_UI_ARCHITECTURE_DECISIONS.md` bleibt die ausfuehrliche Quelle;
  ADR-019 in `DECISIONS.md` verweist kompakt darauf.
- Keine Planabweichung: kein neues Issue, kein #25-Split, keine Aenderung an
  PR #79, keine Renderer-/Bibliotheks-/Hardware-/GPIO-Auswahl.
- Nachweise: `python3 scripts/check_secrets.py` PASS,
  `python3 scripts/check_architecture_boundaries.py` PASS,
  `git diff cfedbf2ca31e317a0869a9e474a68b93b01f321e..HEAD --check` PASS.
- SOLID/DRY/KISS: Die Aenderung dokumentiert schmale gemeinsame Contracts und
  klare Adaptergrenzen; sie fuegt weder ein Widgetframework noch parallele
  Fach-, Safety- oder Hardwarelogik hinzu.

## Planfreigabe und Abnahme

Nach Commit und Push dieses Plans darf erst folgender Ownerkommentar die
Umsetzung freigeben:

```text
PLAN APPROVED
Approved plan commit: <commit-sha>
```

Die genehmigten Issues und Dokumente sind auf diesem Branch aktualisiert. Vor
Ready for Review folgen weiterhin unabhängiges Abschlussreview und die im
Repository festgelegten PR-Gates; dieser PR bleibt bis dahin Draft.
