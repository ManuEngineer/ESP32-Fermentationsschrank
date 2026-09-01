# Hardware-Spike-Plan

Zur Auditnavigation: [`RELEASE_1_ADOPT_OR_BUILD_AUDIT.md`](RELEASE_1_ADOPT_OR_BUILD_AUDIT.md).

## Zweck und Sicherheitsgrenze

Die Spikes entscheiden nur ueber Hardwaretreiber, technische Adapter und
begrenzte Integrationscodecs. Sie implementieren weder fachliche UI,
Sensor-Safety, Regelung noch produktive Aktorfreigaben. Alle Peltier-,
H-Bruecken-, Luefter- und MOSFET-Ausgaenge bleiben waehrend dieser Spikes
elektrisch getrennt oder nachweislich AUS. Der Summer bleibt ebenfalls
getrennt und wird nicht angesteuert.

Die Spikes muessen nicht auf den Abschluss von #20–#24 warten. Nach der Audit-
und Planungsbereinigung genuegt die folgende minimale sichere Hardwarebaseline;
die hardwareunabhaengige Safety-Kette kann parallel weiterlaufen.

## Minimale sichere Hardwarebaseline vor den Spikes

Vor jedem Display-, Touch-, Sensor-, WLAN-Onboarding- oder realen
JSON-Ressourcenspike werden
dokumentiert und nachgewiesen:

- reale ESP32-Boardrevision;
- erfolgreiche Verbindung ueber UART beziehungsweise FT232RL;
- reproduzierbarer Flash-, Boot- und Resetablauf;
- reale Flashgroesse;
- Versorgungsspannungen und sichere Einspeisung;
- verwendete ESP-IDF-Version (`6.0.2`, Produktionsprofile `esp32_bringup`/
  `esp32_release`);
- Betrieb ohne PSRAM;
- Baselinewerte fuer Firmwaregroesse, statisches RAM, freien Heap und groessten
  freien Heapblock;
- verfuegbare GPIOs und grundsaetzlich moegliche Busse, ohne eine produktive
  Belegung festzulegen;
- physische Trennung oder nachweisliche Inaktivitaet aller Aktorpfade.

Mindestens Peltier, BTS7960, Innenluefter, Aussenluefter, alle
MOSFET-Verbraucher und der Summer bleiben getrennt oder nachweislich inaktiv.
Der Summer wird in keinem Display- oder Sensorspike angesteuert.

Diese Baseline ist der kleinste vorziehbare Anteil von #29. Sie ist ein sicheres
Mess-, Build- und Flashfundament, kein vollstaendiges Hardware-Bring-up. Der
Audit empfiehlt eine spaetere Aufteilung von #29, aendert das Issue aber nicht.

### Nicht Bestandteil der minimalen Baseline

Die Baseline legt noch nicht fest:

- endgueltige produktive Pinbelegung;
- finale Display-, Touch- oder DS18B20-Bibliothek;
- endgueltige Sensorbustopologie;
- Luefter-, Summer- oder BTS7960-Adapter;
- Peltierbetrieb oder andere produktive Aktorfreigaben;
- Safety-Grenzwerte oder PI-Parameter;
- finale produktive Partitionierung.

## Gemeinsame Mindestgates pro Kandidat

Jeder fuer die Evaluation freigegebene Kandidat durchlaeuft die folgenden Gates
in dieser Reihenfolge. Ein negatives Ergebnis wird dokumentiert; es wird weder
durch einen Toolchainwechsel noch durch einen verfruehten Hardwaretest umgangen.

Der Display-/Touch-Spike verfeinert diesen Mindestvertrag in die Stufen 0 bis 4:
Seine Stufe 1 umfasst Quellen-, Lizenz-, Kompatibilitaets- und Buildpruefung,
Stufe 2 ist ein kurzer Hardware-Smoke-Test und erst Stufe 3 die vollstaendige
identische Hardwarematrix. Der DS18B20-/1-Wire-Spike verwendet analog die
Stufen 1 bis 3: Quellen-/Lizenz-/Buildpruefung, Sensorsmoke-Test und erst danach
die vollstaendige identische Topologie- und Fehlermatrix.
Der WLAN-Onboardingspike verwendet die Stufen 1 bis 4 fuer Quellen-/Lizenz-/
Toolchainpruefung, einen identischen begrenzten Prototyp je Kandidat (drei
Espressif-Pfade plus WiFiManager als ergebnisoffener konditionaler
Evaluationskandidat), eine gemeinsame Messmatrix und den endgueltigen
Ownerentscheid.
Der aktorfreie JSON-Codecspike verwendet eigene Stufen 1 bis 5 fuer
Quelle/Lizenz/Toolchain, begrenzten Codecprototyp, Grenz-/Fuzztests, reale
ESP32-Ressourcenmessung und den endgueltigen Ownerentscheid. Seine Hosttests
koennen vor der Hardwarebaseline beginnen; seine ESP32-Messung nicht.

### Gate 1 – Quelle, Lizenz und Kompatibilitaetsvertrag

Zu dokumentieren sind:

- offizielle Projektquelle oder lokale Herstellerquelle;
- konkrete Version beziehungsweise Commit;
- Lizenz und betroffene Dateien;
- transitive Abhaengigkeiten;
- deklarierte ESP32-Unterstuetzung;
- Unterstuetzung der benoetigten Controller beziehungsweise Sensoren;
- erwartete Kompatibilitaet mit der fixierten ESP-IDF-`6.0.2`-Toolchain;
- erforderliche Konfigurationsdateien oder Buildflags;
- moegliche Veroeffentlichungseinschraenkungen.

Fuer das LCDWiki-Paket bleibt die interne technische Evaluation freigegeben.
Fehlende paketweite Lizenzklarheit blockiert die Untersuchung nicht. Direkte
Codeuebernahme, abgeleitete Eigenimplementierung und reine Referenznutzung
werden getrennt dokumentiert. Vor einer oeffentlichen Veroeffentlichung direkt
uebernommener Dateien ist eine konkrete Dateipruefung erforderlich; der Spike
erteilt keine allgemeine Publikationsfreigabe.

### Gate 2 – Reproduzierbarer Build ohne reale Aktoren

Der Kandidat wird in einem isolierten Spike-Build mit der bestehenden
ESP-IDF-`6.0.2`-Toolchain eingebunden. Konfiguration, Buildflags und
transitive Abhaengigkeiten muessen reproduzierbar sein. Ein Toolchain- oder
Frameworkwechsel im Rahmen eines einzelnen Spikes ist nicht zulaessig; ein
projektweiter Toolchainwechsel bleibt ein separater Ownerentscheid und loest
gemaess Espressif-first (`docs/ENGINEERING_PRINCIPLES.md`) eine inkrementelle
Neubewertung zuvor ausgeschlossener Kandidaten aus, wie beim Wechsel auf
ESP-IDF 6.0.2 selbst geschehen. Der Build und jeder Lauf bleiben ohne
Peltier-, BTS7960-, Innen-/Aussenluefter-, MOSFET- oder Summeraktivierung und
ermoeglichen einen Base-/Kandidaten-Ressourcenvergleich.

Ein Kandidat, der Gate 2 nicht besteht, erreicht Gate 3 nicht. Zulaessige
typisierte Ergebnisse sind insbesondere:

```text
INCOMPATIBLE_WITH_CURRENT_TOOLCHAIN
BUILD_CONFIGURATION_NOT_REPRODUCIBLE
UNRESOLVED_TRANSITIVE_DEPENDENCY
REQUIRES_UNAPPROVED_TOOLCHAIN_CHANGE
```

### Gate 3 – Identischer realer Hardwaretest

Nur Kandidaten, die Gate 1 und Gate 2 ausreichend bestehen, erreichen einen
realen Hardwaretest. Beim Display-/Touch-Spike ist zunaechst nur Stufe 2
zulaessig; die vollstaendige Matrix der Stufe 3 folgt ausschliesslich nach
bestandenem Smoke-Test. Zwischen den jeweils verglichenen Kandidaten bleiben
gleich:

- dasselbe ESP32-Board;
- dasselbe Display beziehungsweise dieselben Sensoren;
- dieselbe Versorgung und dieselben Leitungen;
- dieselbe gemessene Pinbelegung und Buskonfiguration;
- dieselben Compilerflags und Testinhalte;
- dieselbe Messmethode und Anzahl Wiederholungen.

## Gemeinsame Spikebedingungen

Zusaetzlich gilt:

- ESP-IDF `6.0.2` (Produktionsprofile `esp32_bringup`/`esp32_release`), C++17;
- 4 MB Flash, keine PSRAM-Nutzung;
- je Kandidat ein isolierter Build, keine Vermischung der Kandidaten;
- identische Compilerflags, Displayinhalte, Sensorablaeufe und Messmethode;
- Version/Commit, Konfiguration und Buildartefakte archivieren;
- keine Kandidatenaussage als bestaetigte Produktverdrahtung uebernehmen.

## Spike A: Display und Touch

### Ziel

Den kleinsten stabilen Treiber fuer das tatsaechlich gelieferte, als MSP2807
bestellte Modul bestimmen und Displaycontroller, Touchcontroller, Rotation,
Reset, Shared-SPI-Verhalten sowie Ressourcen auf der Zieltoolchain belegen. Das
Touchdisplay ist die einzige
lokale Bedien- und Anzeigeoberflaeche; der Spike plant keine parallelen lokalen
Eingaben oder Anzeigen.

Die gemeinsamen #25-Praesentationsmodelle und Sprachressourcen werden zuvor
beziehungsweise parallel nativ und hardwareunabhaengig getestet. Der Spike
verwendet sie nur als fachlich identische Eingabe fuer repraesentative
Darstellungen. Er entscheidet weder ihre Struktur noch Navigation oder
Layout. Reale Pixel-/Textpassung, Zeilenumbrueche, Kuerzungen und Schriftgroessen
werden erst auf der bestaetigten Displayhardware gemessen; daraus entsteht
keine LVGL-, Treiber- oder Uebersetzungsbibliothekswahl fuer #25.

Auch die nach dem OD-07-Teilentscheid in fuenf Bereiche geschnittene
#26-Screen-, Navigations-, Dialog-, Aktions- und Fehlerlogik ist kein
Hardware-Spike. Sie wird nativ mit simulierten Praesentationsmodellen und
Touchereignissen geprueft. Der Display-/Touch-Spike liefert erst die reale
technische Integrationsgrundlage und spaeter den identischen repraesentativen
Screen fuer OD-05; er implementiert weder die fuenf Bedienbereiche noch ein
allgemeines UI-Framework.

### Stufe 0 – Reale Hardware identifizieren

Vor jeder Bibliotheksbewertung wird am tatsaechlich gelieferten Modul
dokumentiert:

- genaue Modulvariante;
- Displaycontroller und Touchcontroller;
- Versorgungsspannung und Logikpegel;
- Display-Chip-Select und Touch-Chip-Select;
- Reset, Data/Command und Hintergrundbeleuchtung;
- gemeinsamer oder getrennter SPI-Bus;
- reale Pinbelegung des Moduls;
- Bootzustaende aller relevanten Leitungen.

ILI9341 und XPT2046 gelten nicht allein aufgrund einer Lieferantenbeschreibung
als bestaetigt. Bei abweichenden realen Controllern wird der Kandidatenvergleich
vor der Fortsetzung angepasst. Pinzahlen oder Pegel eines nur aehnlich
aussehenden Moduls werden nicht uebernommen.

### Haupt- und Reservekandidaten

1. Espressif-Stack: `esp_lcd` (Built-in) + `espressif/esp_lcd_ili9341 2.0.2`
   + `espressif/esp_lcd_touch 1.2.1` + `atanisoft/esp_lcd_touch_xpt2046
   1.0.6` (Espressif-first-Erstkandidat)
2. LovyanGFX `1.2.26` (`3f78b705`)
3. TFT_eSPI Manifest `2.5.44` (`16e37595`)
4. LCDWiki-Paket aus
   `references/datasheets/Display/2.8inch_SPI_Module_ILI9341_MSP2807_V1.1.zip`

Kandidaten 2–4 sind ergebnisoffene Evaluationskandidaten unter demselben
Espressif-first-Evaluationsgate wie Kandidat 1 (direkter ESP-IDF-6.0.2-Build/
-Betrieb oder dokumentierter Integrationspfad ohne Arduino-Produktionspfad,
`AGENTS.md`); Espressif-first bestimmt nur die Pruefreihenfolge, nicht die
Auswahl. Arduino_GFX plus geeigneter Touchadapter und Adafruit GFX plus
ILI9341 plus geeigneter XPT2046-Touchadapter sind Reservekandidaten. Sie
werden nur einbezogen, wenn mindestens eine der folgenden Bedingungen
eintritt:

- weniger als zwei der vier Kandidaten 1–4 bestehen den
  Hardware-Smoke-Test der Stufe 2;
- alle vier Kandidaten 1–4 besitzen ein wesentliches Ressourcen-, Wartungs-,
  Stabilitaets- oder Integrationsproblem;
- fuer den konkret benoetigten, spaeter zu veroeffentlichenden Dateisatz bleibt
  eine nicht aufloesbare Lizenz- oder Herkunftsfrage;
- der Vergleich der Kandidaten 1–4 erlaubt keine belastbare Auswahl;
- ein Reservekandidat besitzt nachweislich einen fuer Release 1 wesentlichen
  Vorteil, den kein Kandidat aus 1–4 erfuellt.

Es werden nicht vorsorglich sechs vollstaendige Implementierungen erstellt.
Alle vier Kandidaten 1–4 durchlaufen dagegen zunaechst Stufe 1.

### Stufe 1 – Quellen-, Lizenz- und Buildpruefung

Fuer jeden Hauptkandidaten und jeden spaeter begruendet nachgezogenen
Reservekandidaten werden geprueft:

- offizielle Quelle beziehungsweise lokale Herstellerquelle;
- konkrete Version oder Commit;
- Lizenzstatus der tatsaechlich benoetigten Dateien;
- transitive Abhaengigkeiten;
- benoetigte Konfigurationsdateien und Buildflags;
- Unterstuetzung des in Stufe 0 bestaetigten Display- und Touchcontrollers;
- Kompatibilitaet mit ESP-IDF `6.0.2`, C++17, ESP32-32E, 4 MB Flash und Betrieb
  ohne PSRAM; fuer Arduino-Bibliothekskandidaten zusaetzlich das
  Espressif-first-Evaluationsgate (direkter ESP-IDF-6.0.2-Build/-Betrieb oder
  dokumentierter Integrationspfad ohne Arduino-Produktionspfad, `AGENTS.md`);
- reproduzierbarer isolierter Build und dessen Compilerwarnungen;
- Base-/Kandidatenvergleich fuer Flash, statisches RAM, `firmware.bin`,
  `firmware.elf` und transitive Abhaengigkeiten.

Fuer LCDWiki werden interne technische Referenznutzung, direkte Uebernahme
einzelner Dateien, abgeleitete Eigenimplementierung und eine spaetere
oeffentliche Veroeffentlichung getrennt erfasst. Fehlende paketweite
Lizenzklarheit blockiert den internen technischen Test nicht. Vor einer
oeffentlichen Veroeffentlichung direkt uebernommener Dateien bleibt eine
konkrete Dateipruefung erforderlich.

Moegliche Ergebnisse der Stufe 1:

```text
PASS_BUILD_GATE
INCOMPATIBLE_WITH_CURRENT_TOOLCHAIN
BUILD_CONFIGURATION_NOT_REPRODUCIBLE
REQUIRES_UNAPPROVED_TOOLCHAIN_CHANGE
CONTROLLER_NOT_SUPPORTED
LICENSE_SCOPE_UNRESOLVED_FOR_REQUIRED_FILES
UNRESOLVED_TRANSITIVE_DEPENDENCY
```

Nur Kandidaten mit einem fuer den internen Test ausreichenden Ergebnis erreichen
Stufe 2. `LICENSE_SCOPE_UNRESOLVED_FOR_REQUIRED_FILES` kann die interne
LCDWiki-Untersuchung erlauben, ist aber kein Publikationsnachweis.

### Stufe 2 – Kurzer Hardware-Smoke-Test

Der Smoke-Test prueft mit identischem kleinem Testinhalt, ob ein Kandidat auf
dem realen Modul grundsaetzlich funktioniert:

1. reproduzierbarer Kaltstart;
2. Displayinitialisierung;
3. Querformat 320 x 240;
4. eindeutige Eckmarkierungen;
5. je eine Vollbildfuellung in Schwarz, Weiss, Rot, Gruen und Blau;
6. einfacher ASCII-Text;
7. deutscher, spanischer und englischer Beispieltext;
8. Touchcontroller initialisieren;
9. Touch-Rohwerte an vier Ecken und in der Mitte;
10. fuenf aufeinanderfolgende Neustarts;
11. abwechselnd eine Displayoperation und eine Touchabfrage;
12. Betrieb ohne PSRAM und ohne Vollbild-Framebuffer.

Noch nicht Bestandteil sind eine vollstaendige UI, langfristige Kalibrierung,
1.000 Zyklen, umfangreiche Resetinjektionen, LVGL, produktive Touchnavigation
oder Aktorsteuerung.

Moegliche Ergebnisse der Stufe 2:

```text
PASS_SMOKE_TEST
DISPLAY_INITIALIZATION_FAILED
TOUCH_INITIALIZATION_FAILED
ROTATION_OR_COLOR_FORMAT_INVALID
SHARED_SPI_UNSTABLE
RESET_NOT_REPRODUCIBLE
REQUIRES_UNACCEPTABLE_BUFFER
```

Nur Kandidaten mit `PASS_SMOKE_TEST` erreichen Stufe 3.

### Stufe 3 – Vollstaendige identische Hardwarematrix

#### Hardwareaufbau und Buskonfiguration

| Punkt | Festlegung |
|---|---|
| Display | genau das gelieferte, als TZT/LCDWiki MSP2807 bestellte Modul; reale Variante in Stufe 0 bestimmen |
| Controller | vor Test praktisch identifizieren; ILI9341/XPT2046 nicht allein aus Lieferantenangabe als bestaetigt markieren |
| Versorgung | gemaess verifizierter Modulvariante; Spannung und Strom nur dokumentieren, soweit ein konkretes Gate und geeignete Mittel dies erfordern |
| SPI | derselbe Hardware-SPI-Controller und dieselbe am realen Aufbau funktional verifizierte Pinbelegung fuer alle Kandidaten |
| Chip Select | getrennte Display-/Touch-CS nur nach Boardpruefung; inaktives fail-closed Verhalten funktional pruefen, ohne vorgeschriebene Pegelmessung |
| Reset/DC/Backlight | reale Pins, SSOT-Zuordnung und funktionales Bootverhalten pruefen; keine vorgeschriebene Spannungs- oder Boot-Pegelmessung, nicht getestete Eigenschaften bleiben `TBD_HARDWARE` |
| Weitere Verbraucher | Peltier, BTS7960, Innen-/Aussenluefter, MOSFET-Verbraucher und Summer getrennt/gesperrt |

Es werden keine Pinzahlen aus einem aehnlichen Board uebernommen. Die
verwendeten Pins und Busfrequenzen gehoeren in das Spikeprotokoll und erst nach
Messung in ein spaeteres Hardwareprofil.

#### Identische Testfaelle

Jeder nach Stufe 2 verbleibende Kandidat durchlaeuft in derselben Reihenfolge:

1. Kaltstart und Initialisierung mit schwarzem, aktorfreiem Startbild.
2. Querformat 320 x 240 und eindeutige Eckmarkierungen.
3. je 100 Vollbildfuellungen in Schwarz, Weiss, Rot, Gruen und Blau.
4. ASCII sowie deutsche, spanische und englische Beispieltexte mit identischem
   Fontumfang.
5. identische einfache UI-Elemente: Titel, zwei Temperaturwerte, Statuszeile,
   vier grosse Schaltflaechen und Meldungsdialog.
6. Touch-Rohwerte an Ecken, Kanten und Mitte; Druck-/Kontaktstatus erfassen.
7. Fuenfpunktkalibrierung, Neustart und erneute Pruefung.
8. Aufweckkontakt und eigentlichen Bedienkontakt reproduzierbar unterscheiden.
9. abwechselnde Display- und Touchtransaktionen auf dem gemeinsamen SPI-Bus.
10. 1.000 Zyklen aus Touch lesen, Schaltflaeche zeichnen und Status
    aktualisieren.
11. kontrollierter Reset im Leerlauf, waehrend Zeichnen und waehrend einer
    Touchabfrage.
12. Betrieb ohne PSRAM und ohne Vollbild-Framebuffer.
13. Fehlen beziehungsweise Stoeren des Touchcontrollers: Anzeige, Regel- und
    Safety-Kern duerfen nicht blockieren.
14. Unter allen Display- und Touchoperationen nachweisen, dass geplante Regel-
    und Safety-Arbeit nicht blockiert wird.
15. Raw-Touch bereits vor oder waehrend der Controllerinitialisierung halten
    und im ersten Zehn-Sekunden-Boot-/`SAFE_BOOT`-Fenster erkennen.
16. Erkennung ohne gespeicherte beziehungsweise mit unbrauchbarer Kalibrierung;
    keine False Trigger ohne Beruehrung, bei Rauschen, Reset oder kurzem Touch.
17. Halten ueber Bootgrenzen; Freigeben vor der mehrstufigen langen
    Bestaetigung bricht sicher ab; nach Fensterende kein spaeter Trigger.
18. Recovery haelt Peltier und alle Leistungsaktoren aus; Neustart waehrend
    Recovery bleibt sicher.
19. Fehlender/defekter Touchcontroller fuehrt zu sicherem Hinweis und UART als
    letztem Recoveryweg; kein Netzwerkweg und keine Service-PIN loesen den
    Modus aus.
20. Ein abgeschlossener normaler Werksreset behaelt die geraetespezifische
    Touchkalibrierung; der gesonderte Kalibrierungs-Recoveryfall bleibt davon
    getrennt.

#### Messwerte

Pro Kandidat und Baseline werden erfasst:

- erfolgreiche/fehlgeschlagene Initialisierungen und Resetdauer;
- Rotation, Farbfolge, Textdarstellung und Touchkoordinaten;
- Touch-Latenz, Fehl-/Doppelausloesungen und Busfehler;
- Flash und statisches RAM aus dem identischen Build;
- `firmware.bin` und `firmware.elf`;
- freier Heap nach Boot, niedrigster freier Heap und groesster freier
  Heapblock;
- groesster konfigurierter Display-/Sprite-/Transferpuffer;
- groesster tatsaechlich verwendeter Display-, Sprite- oder Transferpuffer;
- Laufzeit und Fehlerzahl der 1.000 Zyklen sowie Watchdog- und Resetereignisse;
- Bibliothekskonfiguration, eingeschlossene Treiber/Fonts und
  Wartungsaufwand.

#### Erfolgskriterien

- reale Controller und 320-x-240-Querformat funktionieren reproduzierbar;
- alle Testelemente sind lesbar, Touchkalibrierung bleibt nach Neustart
  reproduzierbar und der erste Aufweckkontakt kann getrennt werden;
- Shared-SPI verursacht in 1.000 Zyklen keinen Haenger, Watchdog oder
  unerklaerten Reset;
- kein PSRAM und kein Vollbildpuffer werden vorausgesetzt;
- Build passt in die gemessenen Release-1-Budgets mit dokumentiertem Abstand;
- der Adapter kann Bibliothekstypen aus der Anwendung fernhalten;
- Herkunfts- und Lizenzpruefung ist fuer den konkreten Dateisatz abschliessbar.
- der PIN-unabhaengige Raw-Touch-Boot-Recoveryvertrag ist aktorsicher und ohne
  brauchbare gespeicherte Kalibrierung nachgewiesen; genaue Geste und
  Schwellen bleiben `TBD_HARDWARE`.

#### Abbruchkriterien

- unkontrollierte GPIO- oder Aktorwirkung;
- wiederholter Haenger, Watchdog oder Buszustand, der nur durch Power-Cycle
  geloest wird;
- falscher oder nicht reproduzierbar identifizierbarer Controller;
- notwendige PSRAM-Abhaengigkeit oder nicht begrenzbarer grosser Puffer;
- nicht aufloesbare Lizenz-/Herkunftsfrage fuer den konkret benoetigten Code;
- Kandidat erfordert einen Toolchainwechsel ohne separaten Ownerentscheid.
- Kandidat kann den Raw-Touch-Boot-Recoveryvertrag nicht erfuellen.

### Stufe 4 – Ownerentscheid und Rueckfallkandidat

Der Ownerentscheid bewertet mindestens:

- Funktionsabdeckung und Stabilitaet;
- Touchintegration und Shared-SPI-Verhalten;
- Ressourcen- und Pufferbedarf sowie Abstand zu den Release-1-Ressourcenlimits;
- Build-, Konfigurations- und Wartungsaufwand;
- Upstreamaktivitaet, Lizenz und Herkunft;
- notwendige eigene Anpassungen und Eignung fuer einen schmalen Adapter.

Das Ergebnis benennt genau einen bevorzugten Kandidaten, genau einen
Rueckfallkandidaten, alle verworfenen Kandidaten mit nachvollziehbarem Grund und
noch offene Hardwarefragen. Popularitaet, Sterne oder README-Aussagen allein
begruenden keine Auswahl.

### LVGL getrennt vom Treibervergleich behandeln

LVGL ist weder Display- noch Touchtreiberkandidat und nimmt an den Stufen 0 bis
4 dieses Treibervergleichs nicht teil. Die Reihenfolge lautet:

```text
Display-/Touchtreiber auswaehlen
        |
schmalen Adaptervertrag festlegen
        |
repraesentativen Release-1-Screen erstellen
        |
schlanke eigene Views gegen LVGL vergleichen
```

Der spaetere UI-Frameworkvergleich verwendet denselben ausgewaehlten Treiber,
dieselbe Hardware, denselben repraesentativen Screen, dieselben Texte und
Eingabeelemente sowie dieselbe Messmethode. LVGL wird fuer Release 1 nur
uebernommen, wenn ein klar gemessener Vorteil bei Bedienbarkeit, Wartbarkeit
oder Umsetzung die zusaetzlichen Ressourcen und die Komplexitaet rechtfertigt.

### Sicherheitsgrenze waehrend aller Display-/Touchstufen

Peltier, BTS7960, Innenluefter, Aussenluefter, MOSFET-Verbraucher und Summer
bleiben waehrend aller Stufen physisch getrennt oder nachweislich gesperrt. Kein
Display- oder Touchereignis darf eine reale Aktorfreigabe ausloesen.

### Nicht-Scope und Artefakte

Nicht-Scope: komplette Release-UI, fachliche #25-Projektionen und
Sprachkataloge, die hardwareunabhaengige #26-Navigations-/Screenlogik,
Touch-/Webnavigation, finale Pins, Touchgehause, echte
Serviceaktionen, Aktorbedienung und LVGL-Auswahl. Encoder,
Programmwahlschalter, Start-/Stop-Taster und Status-LED werden auch nicht als
spaetere Spikevarianten untersucht, weil sie kein Bestandteil dieses Projekts
sind. Der microSD-/SD-Karten-Slot wird fuer Release 1 ebenfalls nicht
evaluiert; er erzeugt keinen Spike, Speicherpfad oder Adapter. Der
230-V-AC-Hauptschalter ist kein Firmwareeingang. Ergebnisartefakte:

- Schaltplan/Fotos des Testaufbaus und gemessene Pin-/Pegelmatrix;
- je Kandidat fixierte Abhaengigkeits- und Konfigurationsdatei;
- Quellcommit, Lizenzpaket und Buildlog;
- Screenshots/Fotos, Touchroh- und Kalibrierdaten;
- CSV oder Markdown mit allen Messwerten;
- Base-/Kandidaten-Ressourcenvergleich;
- begruendete Ownerentscheidung mit Rueckfallkandidat.

Notwendige Owner-/Hardwareaktion: reale Boardrevision und Modul bereitstellen,
Verdrahtung nach Messung freigeben und den aus dem identischen Vergleich
hervorgehenden Kandidaten auswaehlen.

## Spike B: DS18B20 und 1-Wire

### Ziel und getrennte Entscheidungen

Der Spike bestimmt den kleinsten stabilen Softwarestack und bewertet getrennt
davon die elektrische Bustopologie. Beide Softwarekandidaten muessen die
zulaessigen Topologien A und B unter identischen Bedingungen pruefen. Ein gutes
Ergebnis einer Bibliothek waehlt keine Topologie, und eine gute Topologie waehlt
keine Bibliothek.

Der Treiber liefert ausschliesslich technische Bus-, Adress-, Mess- und
Fehlerinformationen. Rollenprioritaet, fachliche Sensorqualitaet und
Peltierfreigabe bleiben vollstaendig ausserhalb.

### Verbindliche Sensorrollen fuer die Spikegrenze

- Der Produktfuehler ist optional, bei Verwendbarkeit primaerer Regelsensor und
  darf im Stillstand sowie in einem dafuer zulaessigen Lauf fehlen. Entfernen
  und Wiederanschliessen werden als Anwesenheits- und Treiberereignisse erkannt.
  Sein eigener Fehler darf die festen Sensorbusse nicht elektrisch
  beeintraechtigen.
- Der Raum-/Luftsensor ist fest montiert und regulaerer Ersatz-Regelsensor, wenn
  kein verwendbarer Produktfuehler verfuegbar ist. Er ist nicht pauschal
  primaerer Regelsensor.
- Der Kuehlkoerper-/Peltier-Schutzsensor ist fest montiert und verpflichtende
  Sicherheitsgrundlage. Bei fehlendem, ungueltigem, veraltetem oder nicht
  ausreichend vertrauenswuerdigem Signal gibt es keine Peltierfreigabe. Diese
  Safety-Semantik wird nicht im 1-Wire-Treiber implementiert.

### Verbindliche Softwarekandidaten

1. DallasTemperature `4.0.6` (`dadbbf7d`) plus OneWire `2.3.8`
   (`800f26f3`)
2. Espressif `onewire_bus 1.1.1` (`a269e1fe`) plus `ds18b20 0.4.0`
   (`bf92b0b3`)

Die bestehende Produktionstoolchain ist ESP-IDF `6.0.2`; die
`onewire_bus`-Mindestanforderung ESP-IDF >=5.0 ist damit erfuellt. Der
Espressif-Kandidat wird dennoch nur weitergefuehrt, wenn er ohne verdeckten
Toolchain- oder Frameworkwechsel reproduzierbar in `main/app_main.cpp`
gebaut werden kann. Ein Konflikt lautet weiterhin
`INCOMPATIBLE_WITH_CURRENT_TOOLCHAIN`; dies ist keine Aussage, dass der Treiber
allgemein technisch ungeeignet waere.

### Stufe 1 – Quelle, Lizenz und Build

Fuer beide Kandidaten werden geprueft:

- offizielle Quelle, Version beziehungsweise Commit und Lizenz;
- transitive Abhaengigkeiten und Toolchainkompatibilitaet;
- mehrere 1-Wire-Busse und mehrere Sensoren je Bus;
- stabile 64-Bit-ROM-Adressen;
- nicht blockierende beziehungsweise asynchron integrierbare Konvertierung;
- CRC-, Busfehler-, Trennungs- und Wiederkehrvertrag;
- reproduzierbarer isolierter Build;
- Flash-, statische RAM- und Heapwirkung.

Moegliche Ergebnisse:

```text
PASS_BUILD_GATE
INCOMPATIBLE_WITH_CURRENT_TOOLCHAIN
BUILD_CONFIGURATION_NOT_REPRODUCIBLE
MULTIBUS_NOT_SUPPORTED
MULTISENSOR_NOT_SUPPORTED
UNRESOLVED_TRANSITIVE_DEPENDENCY
REQUIRES_UNAPPROVED_FRAMEWORK_CHANGE
```

Nur Kandidaten mit ausreichendem Ergebnis erreichen Stufe 2.

### Stufe 2 – Sensorsmoke-Test

Mit einem einzelnen realen DS18B20 wird fuer jeden verbliebenen Kandidaten
identisch geprueft:

1. 64-Bit-ROM-Adresse lesen;
2. Aufloesungen 9, 10, 11 und 12 Bit setzen;
3. wiederholt messen;
4. CRC-Status erfassen;
5. Sensor entfernen;
6. Sensor wieder anschliessen;
7. Neustart durchfuehren;
8. Konvertierung ohne blockierende Anwendungspause integrieren;
9. keine alte Messung als neuen gueltigen Wert ausgeben.

Nur Kandidaten, die diesen Sensorsmoke-Test bestehen, erreichen Stufe 3.

### Stufe 3 – Vollstaendige Topologie- und Fehlermatrix

Alle folgenden Tests verwenden 3-Leiter-Betrieb ohne Parasitspeisung. Reale
Pull-ups, Leitungslaengen, Steckverbindung und Schutzmassnahmen werden gemessen,
nicht vorgegeben. Peltier, BTS7960, Innen-/Aussenluefter, MOSFET-Verbraucher und
Summer bleiben getrennt oder gesperrt.

#### Topologie A – drei getrennte Busse

```text
Bus 1 -> Produktfuehler
Bus 2 -> Raum-/Luftsensor
Bus 3 -> Kuehlkoerper-/Peltier-Schutzsensor
```

Zu bewerten sind GPIO-Bedarf, drei Pull-ups, Fehlerisolation, Trennung/Wiederkehr,
Diagnose, Wartbarkeit, Ressourcen sowie Pinqualitaet und Bootstrapping-Risiken
des konkreten ESP32-Boards.

#### Topologie B – Produkt separat, feste Sensoren gemeinsam

```text
Bus 1 -> Produktfuehler
Bus 2 -> Raum-/Luftsensor
         Kuehlkoerper-/Peltier-Schutzsensor
```

Zu bewerten sind geringerer GPIO-Bedarf, Mehrsensorbetrieb, stabile
ROM-Zuordnung, gemeinsamer Busfehler der festen Sensoren, Fehlererkennung,
Wiederherstellung und die sichere Sperrung der Peltierfreigabe ausserhalb des
Treibers.

#### Topologie C – alle drei Sensoren gemeinsam

Topologie C wird nicht als regulaere Zieltopologie weiterverfolgt. Der
abnehmbare Produktfuehler wuerde denselben Bus wie der verpflichtende
Schutzsensor verwenden. Kurzschluss, beschaedigtes Kabel, halb eingesteckter
Stecker oder ein anderer externer Busfehler koennten dadurch die festen
Sensoren und insbesondere die Safety-Sensorverfuegbarkeit beeintraechtigen.

Topologie C darf hoechstens als negativer Referenztest dokumentiert werden.
Dafuer entsteht keine produktive Planung.

#### Bevorzugte Zielrichtung und Rueckfall

Der Produktfuehler erhaelt verbindlich einen eigenen 1-Wire-Bus. Bevorzugt
erhaelt auch der Kuehlkoerper-/Peltier-Schutzsensor einen eigenen Bus, also
Topologie A. Topologie B ist der zulaessige Rueckfall, falls die reale
ESP32-Pinpruefung zeigt, dass ein dritter geeigneter GPIO nur mit problematischen
Boot-, SPI-, Flash- oder Hardwarekonflikten verfuegbar waere.

Die endgueltige Entscheidung erfolgt erst nach minimaler Hardwarebaseline,
realem GPIO-Inventar, realer Pinpruefung, identischem Test von A und B sowie
Fehlerisolationsvergleich. Es werden vorab keine drei GPIOs verbindlich
reserviert.

### Allgemeine Trennungs-, Fehler- und Wiederkehrpruefungen

Das konkrete mechanische und elektrische Stecksystem des Produktfuehlers ist
keine Entscheidung dieses Softwareaudits. Es werden weder Steckertyp,
Kontaktreihenfolge, Anschlussbelegung noch steckerspezifische Schutzschaltung
vorgegeben. Erhalten bleiben die vom Anschluss unabhaengigen Software- und
Buspruefungen:

- Sensor vor Boot getrennt und vor Boot verbunden;
- Trennen und Wiederanschliessen im Stillstand;
- Trennen und Wiederanschliessen waehrend einer laufenden Konvertierung;
- Leitungsunterbruch und simulierte Kontaktunterbrechung;
- keine Auswirkung auf die festen Sensorbusse;
- kein ESP32-Neustart und kein Watchdog;
- keine alte Messung als neue gueltige Messung;
- Wiedererkennung ueber dieselbe ROM-Adresse.

Der Spike erfasst dabei nur Anwesenheit, ROM-Adresse, Konvertierungsstatus,
Messwert, CRC, Busstatus, Timeout und Wiederanschluss. Er implementiert weder
automatische Rollenumschaltung noch Safety-Freigabe. Die spaetere konkrete
Hardwareausfuehrung und ihre elektrische Abnahme bleiben ausserhalb dieses
Plans.

### Identische Volltests pro Softwarekandidat

1. ein Sensor auf einem Bus;
2. drei Sensoren auf drei getrennten Bussen;
3. zwei feste Sensoren gemeinsam und Produktfuehler separat;
4. zehn Neustarts mit stabilen ROM-Adressen;
5. asynchrone 12-Bit-Konvertierung;
6. 1.000 Messzyklen;
7. Produktfuehler trennen und wieder anschliessen;
8. Fehler eines festen Sensors;
9. gemeinsamer Busfehler in Topologie B;
10. Unterbruch;
11. strombegrenzter Kurzschlusstest;
12. CRC-Fehlerinjektion, soweit reproduzierbar;
13. Reset waehrend einer Konvertierung;
14. Wiederinitialisierung;
15. Flash-, RAM-, Heap- und Puffervergleich;
16. blockierte CPU-Zeit;
17. transitive Abhaengigkeiten;
18. Wartungs- und Konfigurationsaufwand.

### Messwerte und Erfolgskriterien

Erfasst werden Erkennungs-, Konvertierungs- und Lesedauer pro Aufloesung,
erfolgreiche Reads, CRC-/Bus-/Timeoutfehler, Wiederanschlusszeit,
ROM-Stabilitaet, blockierte CPU-Zeit, maximaler Adapterpuffer, Flash, statisches
RAM, `firmware.bin`, `firmware.elf`, freier und niedrigster Heap, groesster
freier Heapblock, transitive Abhaengigkeiten sowie das Verhalten auf A und B.

Erfolgreich ist ein Kandidat nur, wenn beide Topologien, Mehrsensorbetrieb,
stabile 64-Bit-Adressen und asynchrone 12-Bit-Konvertierung funktionieren,
Fehler und Wiederanschluss typisiert sind, keine alte Messung als neu gilt, der
Produktfuehlerbus die festen Busse nicht beeintraechtigt und 1.000 Zyklen ohne
Haenger, Watchdog oder unerklaerten Reset laufen.

### Architekturgrenze des spaeteren Adapters

Der produktive Adapter darf ausschliesslich liefern:

- Bus-ID;
- ROM-Adresse;
- Messwert;
- Zeitpunkt;
- Aufloesung;
- CRC-Status;
- Anwesenheit;
- Timeout;
- technischen Fehlerstatus.

Nicht im Adapter liegen Produkt-, Luft- oder Schutzrolle, Auswahl des primaeren
Regelsensors, Ersatzregelung, stabile Rueckkehr zum Produktfuehler,
`VALID`/`STALE`/`FAILED` als fachliche Qualitaetsentscheidung,
Peltierfreigabe oder Safety-Verriegelung. Diese Semantik bleibt in #20, #21 und
#24.

### Abbruchkriterien, Nicht-Scope und Artefakte

Ein Kandidat wird abgebrochen, wenn er nur mit Toolchain-/Frameworkwechsel,
ungebundener Task-/Heapnutzung oder unaufgeloesten Abhaengigkeiten funktioniert,
keine stabile ROM-/Mehrbus-/Mehrsensorunterstuetzung besitzt, Trennung/Wiederkehr einen
Geraetereset erfordert oder eine unkontrollierte elektrische Situation erzeugt.

Nicht-Scope sind fachliche Qualitaet, Filter, Offsets, Rollenwahl,
Ersatzregelung, PI-Regelung, Aktorfreigabe, finale Sensorposition, finale GPIOs
und finale Schutzbauteilwerte. Artefakte sind Aufbau-/Topologiefotos,
Pull-up-/Leitungsdaten, ROM-Liste, identischer Testcode,
Mess- und Fehlerdaten, Base-/Kandidaten-Ressourcenvergleich,
Toolchain-/Abhaengigkeitsbericht sowie getrennte Empfehlungen fuer Softwarestack
und Bustopologie.

Notwendige Owner-/Hardwareaktion: alle drei realen Sensoren und geeignete
Testleitungen bereitstellen; allgemeine Trennungs-, Stoer- und Wiederkehrtests
bestaetigen; Software- und Topologieentscheidung erst anhand der identischen
Messprotokolle treffen. Die Anschlussausfuehrung bleibt eine spaetere separate
Hardwareentscheidung.

## Aktorfreier Webserver-Baselineprototyp fuer #27

Der kleine lokale HTTP-Dienst ist `REQUIREMENT_DECIDED`. ESP-IDF
`esp_http_server` ist `FIRST_EVALUATION_CANDIDATE`, `SPIKE_REQUIRED` und die
bedingte Produktivrichtung; seine endgueltige Auswahl bleibt
`FINAL_SELECTION_PENDING`. `ESPAsyncWebServer` bleibt ein ergebnisoffener
Evaluationskandidat mit `EVALUATE_LATER` und wird nur bei einem dokumentierten
R1-Problem des ersten Prototyps unter identischen Bedingungen nachgezogen.

Der erste Prototyp prueft statische lokale HTML-/CSS-/JavaScript-Testassets,
eine begrenzte Statusabfrage, eine Konfigurationsabfrage, einen simulierten
begrenzten Aenderungsrequest, Export und Upload/Import. Normale, mehrere
parallele, langsame, abgebrochene, ungueltige und uebergrosse Requests,
wiederholtes nicht ueberlappendes Polling, WLAN-Unterbruch und Neustart werden
reproduzierbar getestet. Erfasst werden Firmwaregroesse, statisches RAM,
freier/niedrigster Heap, groesster freier Heapblock, Antwort- und maximale
Bearbeitungszeit, Regelzyklus-Jitter, Watchdog-/Resetereignisse sowie
Abhaengigkeiten und Lizenzwirkung.

Der Prototyp legt noch keine Pollingintervalle, Clientzahl, Antwort-/
Chartgroesse, Timeouts oder Heap-/Jitterbudgets fest; er misst diese Werte.
WebSocket und SSE werden nicht vorsorglich umgesetzt. Die spaetere fachliche
#27-Umsetzung bleibt in HTTP-Transport/interne API, Status/Polling/aktueller
Laufchart, Mutationen, Webassets und Authentisierung gemaess OD-09 getrennt. Es
gibt keine oeffentliche externe Schreib-API und keine allgemeine Web-/
Frontend-/Pluginplattform. OD-06-Onboarding besitzt einen getrennten
fachlichen Lebenszyklus.

## Spike C: WLAN-Onboarding

### Ziel und feste Grenze

Espressif-first (`docs/ENGINEERING_PRINCIPLES.md`) verlangt drei gleichwertig
zu messende ESP-IDF-`6.0.2`-Pfade: `espressif/network_provisioning` auf
`protocomm`, ein direkter `protocomm`-/SoftAP-/HTTP-/DNS-Ansatz ohne
`network_provisioning`, sowie ein kleiner eigener nativer
ESP-IDF-SoftAP-/DNS-/HTTP-Adapter (nutzt denselben zuerst evaluierten
`esp_http_server`-Kandidaten). WiFiManager `v2.0.17` (`d82d0a1b`) bleibt ein
zusaetzlicher ergebnisoffener konditionaler Evaluationskandidat unter
demselben Espressif-first-Evaluationsgate (direkter ESP-IDF-6.0.2-Build/
-Betrieb oder dokumentierter Integrationspfad ohne Arduino-Produktionspfad,
`AGENTS.md`). Alle vier Kandidaten sind `SPIKE_REQUIRED` und
`FINAL_SELECTION_PENDING`; der Spike waehlt noch keinen als
Produktionsabhaengigkeit aus. Es wird kein Kandidat vorsorglich voll
implementiert. Details stehen im Backlog-Issue #89 und im
[Third-Party-Components-Register](../THIRD_PARTY_COMPONENTS.md).

Der Portalbaustein entscheidet weder ueber den erlaubten Start noch ueber
Credential-Kandidaten, Commit, Secrets, Recovery oder Safety. Ein Portal darf
nur beim fabrikneuen Geraet ohne bestaetigte Zugangsdaten oder nach
ausdruecklicher Touchaktion starten, nie allein aufgrund eines temporaeren
Router-, Access-Point-, WLAN- oder Internetausfalls. Fehler, Timeout oder
Abbruch erhalten den bisherigen funktionierenden WLAN-Stand. Regelung, Safety
und Touchbedienung laufen ohne WLAN, Portal oder Browser weiter.

### Stufe 1 – Quelle, Lizenz und Toolchain

Fuer alle vier Kandidaten werden Version/Commit, Lizenz, eingebettete
Webassets (soweit vorhanden), transitive Abhaengigkeiten sowie verwendete und
deaktivierte Funktionen dokumentiert. Der isolierte Build verwendet ESP-IDF
`6.0.2`, ESP32-32E, 4 MB Flash und kein PSRAM; fuer WiFiManager gilt
zusaetzlich das Espressif-first-Evaluationsgate. Zu pruefen sind insbesondere
kontrollierbarer Portalstart, abschaltbarer automatischer Fallback und
Credential-Commit sowie die Behandlung framework- oder bibliotheksseitig
gespeicherter WLAN-Daten. Ein bestandener Build ist noch keine Auswahl.

### Stufe 2 – identischer begrenzter Prototyp je Kandidat

Der identisch reproduzierbare Ablauf prueft fuer jeden der vier Kandidaten:

1. normalen Verbindungsversuch mit bestaetigten Zugangsdaten;
2. ausdruecklich gesteuerten Portalstart und keinen automatischen Start bei
   gewoehnlichem temporaerem WLAN-Ausfall;
3. SoftAP-Start, DNS-Umleitung, direkten Aufruf ueber angezeigte IP und
   vollstaendigen Abbau von Portal, DNS und AP;
4. WLAN-Scan, gueltige Daten, falsches Passwort und nicht erreichbaren Access
   Point;
5. Abbruch, Timeout, Browserabbruch, WLAN-Unterbruch, Neustart und geeignete
   Stromunterbruch-Cut-Points;
6. erneutes Oeffnen des Portals und kontrollierte Wiederaufnahme;
7. Erhalt des bisherigen funktionierenden Credential-Stands bei jedem
   fehlgeschlagenen Kandidaten;
8. keinen unkontrollierten kanonischen Credential-Commit durch die Bibliothek;
9. keine Secrets in Logs, URLs, Diagnose, Exporten oder Fehlermeldungen;
10. keine relevante Blockierung von Regelung oder Safety sowie vollstaendige
    Ressourcenfreigabe nach Portalende.
11. primaeren QR mit individueller SoftAP-SSID und individuellem
    SoftAP-Passwort im gaengigen WLAN-QR-Format; Escaping, Sonderzeichen,
    sichtbare/versteckte SSID soweit unterstuetzt, 320-x-240-Scannbarkeit,
    Abstand, Helligkeit, Rotation und Credentialwechsel ohne Secretlogs;
12. SSID, Passwort, Portaladresse/IP und lokale QR-Wiederanzeige separat; die
    Portaladresse bleibt der manuelle Rueckfall nach dem WLAN-Beitritt;
13. separaten geschuetzten Ersatz-WLAN-Lifecycle: kurzer Ausfall startet
    nichts, langer Ausfall startet nach `TBD_COMMISSIONING`, Heim-Reconnect
    laeuft parallel, Lauf/Web/Auth-/CSRF-/Service-/Safetygates bleiben wirksam,
    und stabile Heim-WLAN-Rueckkehr beendet ihn nach kontrollierter
    Uebergangszeit ohne Abbruch offener Requests oder Speichervorgaenge;
14. Neustart waehrend Ersatz-WLAN und Erhalt aller bestaetigten Credentials.

Auf realer ESP32-Hardware werden Android, iOS beziehungsweise iPadOS und
Windows verwendet. Fuer jeden Client werden automatische Captive-Portal-
Erkennung und der direkte IP-Aufruf getestet. Der direkte Aufruf bleibt der
verlaessliche Rueckfall, weil die automatische Erkennung nicht garantiert ist.

Erfasst werden Firmwaregroesse, statisches RAM, freier und niedrigster Heap,
groesster freier Heapblock, Portalstart-, Verbindungs- und Antwortzeiten,
Regelzyklus-Jitter, Watchdog-/Resetereignisse, Zahl und Umfang der
Abhaengigkeiten sowie der notwendige projektspezifische Integrationscode.

### Stufe 3 – gemeinsame Messmatrix

Alle Kandidaten, die Stufe 2 bestehen, durchlaufen dieselbe Messmatrix aus
denselben Clients, Cut-Points und Messungen sowie dem Nachweis des
browserbasierten R1-Vertrags ohne verpflichtende App, Cloud oder
Kommandozeilenwerkzeug. Kein Kandidat wird vorab wegen blosser Espressif- oder
Drittanbieterherkunft ausgeschlossen. Es entsteht kein allgemeines
Captive-Portal-, Provisioning-, Provider- oder Pluginframework. BLE,
SmartConfig, Cloud- und App-Provisioning bleiben ausserhalb Release 1.

### Stufe 4 – endgueltiger Ownerentscheid und Artefakte

Erst nach dem Spike waehlt der Owner zwischen den drei Espressif-Pfaden und
WiFiManager. Der Bericht enthaelt Quell-/Lizenznachweis, reproduzierbare
Buildkonfiguration, Client-/Fehler-/Cut-Protokoll, Redactionnachweis,
Base-/Kandidaten-Ressourcenvergleich, Lebenszyklus- und Jittermessung,
festgestellte Ausloeser sowie eine begruendete Empfehlung. Ein spaeterer
Wechsel bleibt ueber die konkrete ESP32-Integrationsschicht an der Composition
Root moeglich; Bibliotheks-, DNS-, HTTP- und Callbacktypen gelangen nicht in
Fach-, Safety-, Persistenz-, Secret- oder gemeinsame View-Modelle.

## Spike D: begrenzter JSON-Codec

### Ziel und feste Grenze

ArduinoJson `7.4.3` (Tag-Commit `77771d3c07668e01d8f52acb03910c1110bb373f`)
ist `FIRST_EVALUATION_CANDIDATE`, `SPIKE_REQUIRED` und
`FINAL_SELECTION_PENDING`, also bis zum bestandenen Spike keine ausgewaehlte
Produktionsabhaengigkeit. Der Spike prueft ausschliesslich
begrenzte externe API-, Konfigurations-, Programm-, Diagnose-, Export-,
secret-freie Backup- und Importvertraege. Interne Kontrollpunkte, Roots,
Safety-Zustaende, Lauf-Recovery und binaere Records bleiben unveraendert.

ArduinoJson-Typen enden an einer kleinen konkreten DTO-/Codecgrenze. Schema-,
Werte-, Berechtigungs-, Konflikt-, Secret-, Redaction- und Importaktivierungs-
logik bleiben projektspezifisch. Es entstehen weder ein Eigenparser noch ein
`IJsonProvider`, ein Pluginregister oder ein vorsorglicher Zweitcodec.

### Stufe 1 – Quelle, Lizenz und Toolchain

Zu dokumentieren sind offizieller Tag und Commit, MIT-Lizenz, Paketmanifest,
tatsaechlich verwendete Header/Features, deaktivierte Funktionen, transitive
Bestandteile und Notices. Der isolierte reproduzierbare Build verwendet
ESP-IDF `6.0.2`, C++17, ESP32-32E, 4 MB Flash und keine
PSRAM-Abhaengigkeit. Ein bestandener Build ist noch keine Auswahl.

### Stufe 2 – begrenzter Codecprototyp

Der Prototyp bildet mindestens ab:

1. ein kleines gueltiges Request-DTO;
2. die maximale gueltige Struktur jedes Profils;
3. ein Status-/Response-DTO;
4. einen begrenzten Export;
5. einen vollstaendigen R1-Importkandidaten;
6. eine technische und fachliche Importvorschau ohne Aktivierung;
7. direkte Serialisierung in ein begrenztes `Print`-/Streamziel, soweit
   sinnvoll;
8. die vollstaendige Uebersetzung in stabile projektspezifische Fehler;
9. den Nachweis, dass keine ArduinoJson-Typen ausserhalb der Codec-/
   Integrationsschicht sichtbar werden.

Initial gelten 1 KiB fuer nachgewiesene kleine Kommandos und 4 KiB fuer
nachgewiesene einzelne Programm-/Konfigurationsaenderungen. Fuer den
vollstaendigen R1-Import gilt `DERIVE_FROM_MAXIMUM_VALID_EXTERNAL_SCHEMA` und
`MEASUREMENT_REQUIRED`: Der Spike erzeugt zuerst deterministisch den maximal
gueltigen externen Kandidaten aus maximal 16 Programmen, IDs bis 48 Byte,
Namen bis 96 Byte, Notizen bis 1.024 Byte je Programm, weiteren Feldern,
Konfiguration, Schemaversionen, JSON-Struktur,
Worst-Case-Escaping, UTF-8, Metadaten sowie Integritaets-/Referenzfeldern. Der
Maximalfall muss importierbar sein. Die maximale Verschachtelung
betraegt zunaechst 6. Root-Typ, Methode, Content-Type, Strings, Arrays,
Objektfelder, Zahlen, unbekannte Felder und Schemaversionen werden je Vertrag
begrenzt. Grosse Antworten und Verlaeufe werden paginiert oder gestreamt.
Backupausgabe und Importrequest sind getrennte Vertraege. Erst der Spike waehlt
zwischen begrenztem Gesamtbody mit begruendeter Reserve und begrenztem
Streaming-/Chunkpfad; beide bauen vor Aktivierung einen vollstaendigen
typisierten, vollvalidierten Kandidaten und bleiben ohne Teilwirkung bei
Abbruch oder Stromunterbruch.

### Stufe 3 – Grenz-, Negativ- und Fuzztests

Reproduzierbar und begrenzt zu pruefen sind mindestens:

- leerer und abgeschnittener Body, ungueltige Syntax und Escapes sowie
  ungewoehnliche UTF-8-Eingaben;
- falscher Root-Typ, unbekannte/doppelte/fehlende Felder, falsche Datentypen,
  negative Werte, Ueberlaeufe und Werte an/ueber ihren Grenzen;
- lange Strings, grosse Arrays, Verschachtelung und Bodygroesse jeweils an und
  ueber der Grenze;
- unbekannte Schemaversion, `NaN`, `Infinity` und nicht erlaubte Zahlenformen;
- langsame oder abgebrochene Quellen, wiederholte ungueltige sowie maximale
  gueltige Requests;
- Import mit Secrets oder geschuetzten Feldern, secret-freier Export und
  Parseerfolg mit anschliessendem Fachfehler.

Jeder Fehler endet ohne Teilaktivierung. Oeffentliche Fehler enthalten keine
Secrets, Bibliotheksdetails, Speicheradressen oder ungefilterten Eingaben.
Der spaetere #19-D-Nachweis prueft zusaetzlich den synchronen Run-Gate vor
Annahme, Vorschau/Bestaetigung und Commit fuer `Unknown`,
`NoActiveOrRecoverableRun`, `ActiveRunPresent` und `RecoverableRunPresent`,
einen konkurrierenden Runstart, Neustarts und Cut-Points. Nur
`NoActiveOrRecoverableRun` erlaubt; die Serialisierung fuehrt weder Pending
noch Intent oder einen parallelen Active-Zweig ein.

### Stufe 4 – Ressourcen und Laufzeit

Auf der minimalen sicheren ESP32-Baseline werden Base und Kandidat verglichen:
Firmwaregroesse, statisches RAM, freier und niedrigster Heap, groesster freier
Heapblock, maximale gleichzeitige Speicherbelegung, moegliche Fragmentierung
bei wiederholten kleinen und maximalen Parse-/Serialize-Zyklen, Parse- und
Serialisierungszeit, Regelzyklus-Jitter sowie Watchdog-, Reset- und
Stabilitaetsauffaelligkeiten. Fehlerhafte und abgebrochene Eingaben gehoeren
zur Messmatrix. Alle Aktorpfade und der Summer bleiben getrennt oder
nachweislich inaktiv.

### Stufe 5 – endgueltiger Ownerentscheid und Artefakte

Erst nach bestandenem Nachweis wird ArduinoJson zur endgueltigen Uebernahme
vorgelegt. Eine Alternative wird nur bei reproduzierbarer Toolchain-
inkompatibilitaet, unvertretbarem Flash-/RAM-/Heapbedarf, nicht begrenzbarem
Speicherverhalten oder Fragmentierung, Instabilitaet, relevanter Jitter-/
Safetywirkung, nicht robust abbildbarer R1-Anforderung oder konkretem Lizenz-/
Publikationsproblem untersucht. Modernitaet, Vorliebe und hypothetischer
Zukunftsbedarf sind keine Ausloeser.

Der Bericht enthaelt Quell-/Lizenznachweis, Buildkonfiguration, DTO-/
Fehlervertrag, Grenz-/Fuzzprotokoll, Base-/Kandidatenmessungen und eine
begruendete Empfehlung. Ein spaeterer Codecwechsel ersetzt nur die kleine
konkrete Integrationsgrenze und keine Fachmodelle oder interne Persistenz.

## Spike E: Speicher-, Retention- und Bereinigungsnachweis fuer #19

### Ziel und Grenze

Dieser aktorfreie Nachweis bestimmt die reale Kapazitaet fuer das typisierte
Ereignisjournal und die begrenzte persistente Laufhistorie. Er ist kein
Produktivimport und legt weder eine neue Partitionierung noch ungepruefte
Retentionszahlen fest. Der aktive Lauf, die letzten 5 detaillierten Laeufe und
50 Laufzusammenfassungen bleiben bis zur Messung ein R1-Ziel und keine
Speichergarantie. Detaillierte Laeufe verwenden begrenzte, verdichtete
Messreihen statt jeder etwa zweisekundlichen Rohmessung.

Zu dokumentieren sind reale NVS-/Partitionsgroesse, Recordformate und
-groessen, Messpunktgroesse, Verdichtungsstrategie, Worst-Case-Laufdauer,
Flashverbrauch sowie der gleichzeitig benoetigte Platz fuer Konfiguration,
Active/Fallback, Lauf-Recovery und kritisches Safety-/Recoveryjournal.

### Kapazitaets- und Cut-Point-Matrix

Mindestens zu pruefen sind:

1. Speicher schrittweise bis zur Bereinigung fuellen;
2. aktiven Lauf und notwendige Recoverydaten jederzeit schuetzen;
3. kritisches Safety-/Recoveryjournal vor Zusammenfassungen und detaillierten
   Komfort-/Diagrammdaten schuetzen;
4. Loeschkandidaten deterministisch auswaehlen;
5. Unterbruch vor, waehrend und nach jedem Bereinigungsfortschritt;
6. Neustart und idempotente Wiederaufnahme nach jedem Cut-Point;
7. wiederholte Journal-/Historien-/Bereinigungszyklen bis zum stabilen
   Langzeitverhalten;
8. vollstaendigen Speicher, Read-/Writefehler und unbekannten Fortschrittsstand
   ohne stilles Verwerfen kritischer Daten behandeln;
9. Flash-, statisches RAM-, Heap-, Laufzeit- und Schreibmengenwirkung erfassen.

Der Test weist auch nach, dass periodische Temperaturrohwerte nicht als
einzelne Journalrecords persistiert werden und dass Secrets oder unredigierte
sensible Eingaben weder im Journal noch in Exportartefakten erscheinen. Alle
Aktorpfade und der Summer bleiben getrennt oder nachweislich inaktiv. Ergebnisse
werden als gemessene Budgetgrundlage dokumentiert; aus Hostsimulationen wird
keine reale Flashlebensdauer oder Stromausfallgarantie abgeleitet.

## Spike F: Diagnose- und Ressourcennachweis fuer #28

Dieser Nachweis waehlt keine Diagnose-, Logging-, Telemetrie-, Metrics-, Chart-
oder Exportbibliothek aus. Die passiven #28-A-Modelle werden zuerst nativ
geprueft. Nach der realen Ressourcenmessung und dem OD-09-Vertrag wird der
gefuehrte #28-C-Serviceablauf nativ mit Mocks geprueft. Der reale ESP32 dient
fuer #28-B zur Erfassung der Plattform- und Gesundheitsmetriken sowie spaeter
zur Abnahme bereits gegateter Adapter.

Nach der minimalen Hardwarebaseline werden mindestens freier und niedrigster
freier Heap, groesster freier Heapblock soweit verfuegbar, statisches RAM,
Firmwaregroesse, Flash-/Partitionsstatus, Persistenz-/Journal-/Historien-
auslastung, Reset-/Watchdog-/Stabilitaetsereignisse sowie die Grenzen technischer
Zaehler und Diagnosepuffer unter identischen Lastfaellen erfasst. Rohmessungen
und abgeleitete Warnstatus werden getrennt protokolliert. Schwellen, Reserven,
Partitionen und Budgets bleiben bis zum Nachweis `MEASUREMENT_REQUIRED` und
werden nicht als Garantie aus Hostwerten abgeleitet.

Der simulierte Serviceablauf prueft Voraussetzungen, getrennte Auth-/Safety-
Gates, Bestaetigung, Fortschritt, Abbruch, Fehler und sichere Rueckkehr ohne
reale Aktoren. Reale Aktortests folgen erst nach #24 und den jeweiligen
Hardware-/Inbetriebnahmegates; bis dahin bleiben Peltier, BTS7960, beide
Luefter, MOSFET-Verbraucher und Summer getrennt oder nachweislich inaktiv. Der
microSD-/SD-Karten-Slot wird nicht evaluiert. #28-D verwendet spaeter den
bereits redigierten Fachbericht mit der #19-C-Exportinfrastruktur; dieser Spike
implementiert weder Exportframework noch Berichtsimport.

## Spike G: Authentisierung und Plattformschutz

OD-09 ist fachlich entschieden; dieser aktorfreie Spike weist die technische
Eignung nach und implementiert keine produktiven Endpunkte. PBKDF2-HMAC-SHA-256
aus der fixierten mbedTLS-/ESP32-Toolchain ist
`FIRST_EVALUATION_CANDIDATE`, `SPIKE_REQUIRED` und
`FINAL_SELECTION_PENDING`. Mindestens geprueft werden:

- reproduzierbarer Build und bekannte KDF-Testvektoren;
- getrennte zufaellige Salts mit mindestens 128 Bit, 256-Bit-Verifier,
  KDF-Parameterkennung und zeitkonstanter Vergleich;
- richtige und falsche Passwort- und PIN-Pruefungen, auch parallel;
- KDF-Laufzeit, Stack, Heap, niedrigster freier Heap, groesster freier
  Heapblock, Regelzyklus-Jitter, Watchdog und Stabilitaet;
- globale Fehlversuchsserien fuer Passwort und PIN; Vor-Sperr-Zaehler,
  Sperrstufe, aktiver Sperrzustand, Credential-Epoche/-Generation und
  Integritaet vor der Fehlerantwort atomar persistieren/validieren; Erfolg
  atomar zuruecksetzen, Persistenzfehler `fail closed`, Neustart ohne Bypass;
- waehrend aktiver Sperre weder KDF noch einen Write je abgewiesener Anfrage;
- passwortgeschuetzte normale Sessions und bewusst passwortlose anonyme lokale
  Sessions mit gleicher Cookie-/CSRF-/Ressourcenpolicy, sichtbarer Warnung,
  geschuetztem Moduswechsel und vollstaendigem Sessionwiderruf;
- `esp_fill_random()` oder korrekt gesaeter mbedTLS-DRBG fuer Salts,
  Sessionkennungen und CSRF-Tokens, einschliesslich sicherem Fehlerpfad;
- Credentialwechsel, Readback, Validierung, atomarer Epochcommit, Widerruf und
  Stromunterbruch an allen relevanten Cut-Points;
- keine Teilaktivierung, kein Rueckfall auf eine alte Credential-Epoche und
  keine Secrets in Testlogs oder Artefakten.

Der Spike legt keine Iterationszahl, KDF-Produktionswahl, Stack-/Heapgarantie
oder Schedulingstrategie fest. Danach entscheidet der Owner KDF und Work
Factor. NVS-/Flashverschluesselung wird separat als
`EVALUATE_BEFORE_RELEASE` auf Toolchain, Partitionierung, Provisionierung,
Schluesselverlust, Entwicklungs-/Produktionsflash, Recovery, Werksreset,
Updatepfad, Ressourcen und dokumentierte physische Schutzgrenze geprueft. Sie
wird hier weder aktiviert noch zugesagt. Alle Aktorpfade bleiben getrennt oder
nachweislich inaktiv.

Dieses separate Gate liegt zwingend vor #37 und prueft zusaetzlich Boot,
Schluesselentstehung/-speicherung, UART-Neuflash sowie Migrations- und
Stabilitaetsauswirkungen. Das Ergebnis ist entweder eine produktive Auswahl mit
Provisionierungsprozess und Recovery-/Regressionstests oder eine begruendete
Nichtauswahl mit dokumentierten Rest-Risiken, klaren Schutzgrenzen und
ausdruecklicher Ownerfreigabe. Weder Ergebnis, eFuse-/Secure-Boot-Nutzung,
Schluesselmodell noch Partitionierung werden in diesem Audit festgelegt.

Der Policyentscheid allein gibt keine Webmutation frei. Der Nachweis umfasst
je Modus auch Methoden-, Content-Type-, Origin-/Referer-/Fetch-Metadata-,
Revisions-, Konflikt-, Wiederholungs-, Abbruch- und Neustartpruefungen sowie die
Webserver-/JSON-Gates. Servicefaelle verlangen zusaetzlich PIN-KDF und
neustartfeste PIN-Sperre; reale Serviceaktoren bleiben hinter Safety- und
Hardwaregates.

## Reihenfolge und Entscheidungsprotokoll

1. Audit- und Planungsbereinigung abschliessen.
2. Den minimalen Baseline-Anteil von #29 nachweisen; der vollstaendige Abschluss
   von #24 oder #29 ist keine Voraussetzung fuer die aktorfreien Spikes.
3. Beim Display-/Touch-Spike zuerst die reale Hardware in Stufe 0
   identifizieren und alle vier gleichrangigen Kandidatengruppen (Espressif-
   Stack, LovyanGFX, TFT_eSPI, LCDWiki) durch Stufe 1 fuehren.
4. Nur ausreichend erfolgreiche Display-/Touchkandidaten in Stufe 2 kurz auf
   der Hardware pruefen; ausschliesslich Kandidaten mit `PASS_SMOKE_TEST`
   durchlaufen die vollstaendige identische Matrix der Stufe 3.
5. In Stufe 4 genau einen bevorzugten Display-/Touchkandidaten und einen
   Rueckfallkandidaten bestimmen. Reservekandidaten werden nur bei einem
   dokumentierten Ausloeser nachgezogen.
6. Beide DS18B20-/1-Wire-Kandidaten durch Stufe 1 fuehren, nur ausreichende
   Kandidaten mit einem einzelnen Sensor in Stufe 2 pruefen und nur deren
   Erfolge in Stufe 3 identisch auf Topologie A und B testen. Softwarestack und
   elektrische Bustopologie getrennt entscheiden; Topologie C nicht produktiv
   planen.
7. Herkunfts-/Lizenzpruefung fuer die technisch geeigneten Kandidaten
   aktualisieren.
8. Die drei Espressif-Pfade und WiFiManager gleichwertig durch Stufe 1 und 2
   fuehren, in Stufe 3 gemeinsam messen und danach die endgueltige
   Onboardingauswahl dem Owner vorlegen (Backlog-Issue #89).
9. Den `esp_http_server`-Baselineprototyp ausfuehren. `ESPAsyncWebServer` nur
   bei dokumentiertem R1-Problem mit identischem Umfang nachziehen und danach
   die endgueltige Serverauswahl dem Owner vorlegen.
10. ArduinoJson durch Quelle-/Toolchain-, Codec-, Grenz-/Fuzz- und
   Ressourcenstufen pruefen. Eine Alternative nur bei dokumentiertem Problem
   untersuchen und die endgueltige Codec-Uebernahme dem Owner vorlegen.
11. Den OD-09-Authspike fuer KDF, Zufall, Ressourcen, Sperrserien und
    Credential-Cut-Points ausfuehren; danach KDF und Work Factor dem Owner
    vorlegen. Plattformverschluesselung separat vor Release evaluieren.
12. Fuer den spaeteren #19-B-Schnitt die reale Speicher-, Retention- und
    Bereinigungsmatrix einschliesslich Cut-Points ausfuehren und das
    5-/50-Ziel erst anhand dieser Messung dimensionieren.
13. #28-A nativ pruefen und nach der Baseline die realen
    #28-B-Ressourcenmesspunkte erfassen; danach #28-C gemaess entschiedenem
    OD-09-Vertrag mit Mocks pruefen. Reale Serviceadapter bleiben hinter #24,
    den OD-09-Technikgates und den
    jeweiligen Hardwaregates.
14. Owner waehlt je Hardwaregruppe genau einen Produktivkandidaten und einen
    dokumentierten Rueckfallkandidaten.
15. Erst danach implementieren #30 und #31 die schmalen Adapter. Die
   hardwareunabhaengige #26-Logik wird separat nativ/simuliert entwickelt; der
   UI-Frameworkvergleich mit LVGL folgt erst auf den ausgewaehlten
   Display-/Touchtreiber, den schmalen Adaptervertrag und einen repraesentativen
   Release-1-Screen.

Parallel dazu darf die hardwareunabhaengige Kette #20 Sensorqualitaet, #21
Regelsensorauswahl, #22 PI/Luftbegrenzung, #23 Aktorplaner und #24 Fehlerkern/
`SAFE_BOOT` weiterlaufen. Bibliothekstypen und reale GPIOs gelangen nicht in
diesen Kern; Treiber- und Fachstatus bleiben getrennt, und ungemessene
Hardwarewerte bleiben `TBD_COMMISSIONING`.

Produktive Aktoradapter und reale Aktortests bleiben von ihren Safety-Gates
abhaengig. Ein bestandener aktorfreier Spike ist keine Aktorfreigabe.
