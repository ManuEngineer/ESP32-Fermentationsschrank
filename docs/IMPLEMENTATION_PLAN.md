# Implementierungsplan und Entwicklungsreihenfolge

## Status

Dieses Dokument beschreibt die in Phase 10B akzeptierte Entwicklungsstrategie.

Die Hardware wird voraussichtlich erst etwa zwei Wochen nach Beginn der
Implementierung verfuegbar sein. Die Softwareentwicklung wartet deshalb nicht auf
den realen Aufbau. Der fachliche Kern, Simulationen, Tests, Persistenz,
Bedienlogik und grosse Teile von Web und Diagnose werden vorher entwickelt.

Gleichzeitig darf keine unbestaetigte GPIO-Zuordnung, Polaritaet oder
Hardwareannahme als verifizierte Funktion behandelt werden.

Die gewaehlte Loesung ist daher:

```text
Softwarekern zuerst und mit Simulation entwickeln
+ reale Hardware hinter klaren Schnittstellen kapseln
+ dieselbe Firmware spaeter in einem geschuetzten Bring-up-Profil verwenden
+ Aktoren erst nach elektrischer Verifikation schrittweise freigeben
```

Es wird keine separate Wegwerf-Testfirmware als eigenes Projekt verlangt.

## Warum keine reine Hardware-Testfirmware als blockierender erster Schritt

Eine kleine Bring-up-Firmware haette folgende Vorteile:

- sehr wenig gleichzeitig aktiver Code
- einfache Zuordnung eines Messwertes zu einer einzelnen Hardwareaktion
- geringeres Risiko, dass eine noch unfertige Zustandsmaschine einen Ausgang
  beeinflusst
- schnelle Pruefung von Flash, Pins, Pegeln, Sensoren und Display
- leichteres Debugging bei unbekannter Platinenrevision

Diese Vorteile verlangen aber **nicht**, dass vor der Ankunft der Hardware keine
Fachsoftware entwickelt wird.

Der gleiche Sicherheits- und Diagnosevorteil wird durch ein Bring-up-Profil in
der spaeteren Produktionscodebasis erreicht:

- Hardwareadapter sind getrennt vom fachlichen Kern.
- Unbestaetigte Aktorausgaenge sind technisch gesperrt.
- Jeder Hardwaretest ist einzeln, zeitlich begrenzt und sichtbar.
- Die normale Zustandsmaschine kann fuer reine Hardwaretests deaktiviert bleiben.
- Dieselben Treiber und Diagnosepfade werden spaeter weiterverwendet.
- Es entsteht kein zweites Projekt, dessen Testcode danach weggeworfen oder
  manuell uebertragen werden muss.

## Verbindliche Entwicklungsprofile

Die PlatformIO- und Teststruktur soll mindestens folgende Umgebungen
beziehungsweise Profile abbilden.

### `native`

Laeuft auf dem Entwicklungsrechner und enthaelt:

- fachliches Datenmodell
- Zustandsmaschine
- Zeit- und Phasenlogik
- Programmvalidierung
- Sensorstatus und Filterlogik
- PI-Reglerkern
- Aktorplaner ohne reale GPIOs
- Fehler- und Wiederherstellungslogik
- Persistenztests mit Testbackend
- Simulationen mit virtueller Zeit

### `esp32_bringup`

Laeuft auf dem ESP32 zur Hardwareverifikation.

Verbindliche Eigenschaften:

- alle Aktoren nach Boot gesperrt
- kein automatischer Fermentationsstart
- kein automatischer Peltier-, Luefter- oder Summertest
- einzelne Hardwaretests nur ueber den geschuetzten Serviceablauf
- noch nicht bestaetigte Ausgaenge bleiben compile-time oder runtime gesperrt
- Diagnose von Flash, Heap, Resetursache, Sensorbussen, Display und Eingaben
- sichtbarer Zustand `HARDWARE_UNVERIFIED`
- Hardwarefreigaben werden einzeln protokolliert

### `esp32_release`

Normale Zielkonfiguration fuer den Fermentationsschrank.

Sie darf Aktoren nur verwenden, wenn:

- Hardwareprofil und Platinenrevision bestaetigt sind
- GPIO-Zuordnung und aktive Pegel bestaetigt sind
- sichere Boot- und Resetpegel nachgewiesen sind
- benoetigte Sensoren und Luefter freigegeben sind
- die zugehoerigen Hardware-Abnahmetests bestanden sind

Ein blosses Umschalten des Buildprofils darf keine unbekannte Hardware automatisch
freigeben.

## Architektur fuer Entwicklung ohne Hardware

### Fachlicher Kern ohne Arduino-Abhaengigkeit

Der fachliche Kern wird so weit wie sinnvoll als normale, nativ testbare
C++-Logik entwickelt. Er verwendet keine direkten Aufrufe von:

- `digitalWrite`
- `analogRead`
- 1-Wire-Bibliothek
- Displaybibliothek
- WLANbibliothek
- Dateisystembibliothek
- realer Systemzeit

Stattdessen werden Schnittstellen verwendet, beispielsweise konzeptionell:

```text
ITimeSource
ITemperatureSource
IActuatorSink
IStateStore
IEventJournal
INetworkStatus
IUserNotificationSink
```

Die ESP32-spezifischen Adapter implementieren diese Schnittstellen spaeter mit
der realen Hardware.

### Simulierte Hardware

Vor Ankunft der Hardware werden mindestens simuliert:

- Schrankluft-, Produkt- und Kuehlkoerpersensor
- Sensorstatus `VALID`, `STALE`, `FAILED`
- langsame und schnelle Temperaturverlaeufe
- Heizen, Neutralbereich und Kuehlen
- Innen- und Aussenluefterzustand
- BTS7960-Freigabe als abstrakter Aktor
- Stromausfall und Neustart
- fehlende oder spaeter eintreffende NTP-Zeit
- Flash-/Persistenzfehler
- Benutzeraktionen von Display und Web

Der Simulator darf ein einfaches thermisches Modell verwenden. Dieses Modell
dient dem Test der Softwareablaeufe und **nicht** zur Bestimmung der realen
PI-Parameter oder Sicherheitsgrenzen.

### Aktorplaner statt direkter GPIO-Steuerung

Vor der Hardwareankunft kann die vollstaendige logische Aktorkette entwickelt
und getestet werden:

```text
Regleranforderung
  -> Luftbegrenzung
  -> Sicherheitsfreigabe
  -> Mindest-Ein-/Auszeit
  -> Richtungswechselbestaetigung
  -> Totzeit
  -> Impulsakkumulator
  -> abstrakter Aktorbefehl
```

Der letzte Schritt endet im nativen Test bei einem Mock beziehungsweise
Ereignisprotokoll und nicht an einem GPIO.

## Was vor Ankunft der Hardware sinnvoll fertiggestellt werden kann

### Vollstaendig oder weitgehend entwickelbar

1. Repositorystruktur, PlatformIO-Umgebungen und CI
2. native Testumgebung und virtuelle Zeit
3. Datenmodelle, Fehlercodes und Versionierung
4. Programmschema und Standardprogramme mit `TBD_COMMISSIONING`-Werten
5. Zustandsmaschine und Prozessablaeufe
6. Start, Stop, Quittierung und Laufanpassungen
7. Sensorstatus-, Filter- und Plausibilitaetslogik mit simulierten Messfolgen
8. PI-Reglerkern und zeitproportionaler Aktorplaner
9. Totzeit-, Mindestzeit- und Richtungswechsellogik
10. Sicherheits- und Verriegelungslogik auf abstrakter Ebene
11. Konfigurationsvalidierung und Revisionsmodell
12. Laufpersistenz und Wiederherstellungsentscheidungen mit Testbackend
13. Fehler- und Resetjournal als Datenmodell
14. Speicheraufbewahrung und automatische Bereinigungslogik
15. Sprachdateien Deutsch, Spanisch und Englisch
16. lokale UI-Navigation und View-Modelle ohne bestaetigte Touchkalibrierung
17. Web-API-Vertraege und Weboberflaeche gegen simulierte Daten
18. Diagnosemodelle und Exportformate
19. Serviceablauf als Zustandsmaschine ohne reale Aktorfreigabe
20. Fehlerinjektions- und Simulationstests

### Erst mit Hardware abschliessend pruefbar oder festlegbar

- tatsaechliche Flashgroesse und reale Partitionseigenschaften
- PSRAM-Erkennung
- GPIO-Zuordnung der Onboard-MOSFET-Ausgaenge
- aktive Pegel und Boot-/Resetverhalten
- reale BTS7960-Pinbelegung, Enable-Verhalten und R_IS/L_IS
- reale 1-Wire-Busse und Hot-Plug-Verhalten
- Displaycontroller, Touchcontroller, Rotation und Kalibrierung
- tatsaechlicher Heap- und Firmwareverbrauch auf der Zielhardware
- WLAN-, Web- und Displaylast auf dem ESP32
- Luefterstrom, Anlaufstrom und Nachlauf
- Peltierstrom, Polaritaet und thermische Leistung
- PI-Parameter, Filterzeiten und Luftbegrenzungen
- Sicherheits-Eingriffs- und harte Notgrenzen
- Temperatursicherung und Montageort
- thermische Gleichmaessigkeit und reale Fermentationswerte

Diese Punkte bleiben bis zum Nachweis sichtbar als `TBD_HARDWARE`,
`TBD_COMMISSIONING` oder `TBD_IMPLEMENTATION_BUDGET` markiert.

## Reihenfolge bis zur Hardwareankunft

### SW0: Projektgrundlage und Testbarkeit

- PlatformIO-Umgebungen `native`, `esp32_bringup`, `esp32_release`
- CI fuer native Tests und ESP32-Build
- keine Geheimnisse im Repository
- Hardware-Abstraktionen und Mockadapter
- virtuelle monotone und absolute Zeitquelle
- grundlegende Fehlercode- und Ergebnistypen

### SW1: Fachliches Datenmodell und Zustandsmaschine

- Programme und unveraenderlicher Laufschnappschuss
- Prozessphasen und Uebergaenge
- Start-, Stop- und Abschlussverhalten
- Meldungen, Quittierung und Resettrennung
- simulierte vollstaendige Standardablaeufe

### SW2: Konfiguration, Persistenz und Wiederherstellung

- Factory-, Benutzer- und Laufdaten trennen
- atomare Revisionen und Rueckfall
- Laufkontrollpunkte
- Wiederanlaufentscheidungen
- Speicheraufbewahrung und Bereinigung
- Tests fuer korrupte und aeltere Revisionen

### SW3: Sensor-, Regel- und Sicherheitskern

- Sensorqualitaetszustaende
- Filter und Plausibilitaet
- PI-Reglerkern
- Luftbegrenzung
- Aktorplaner, Mindestzeiten und Totzeit
- Fehlerklassen, Verriegelung und `SAFE_BOOT`-Logik
- Fehlerinjektionen mit simulierten Sensoren und Aktoren

### SW4: Bedienung gegen simulierten Kern

- lokale UI-View-Modelle und Navigation
- Programmauswahl und Programmeditor
- Laufanzeige, Meldungen und Diagnose
- Weboberflaeche und lokale API
- Mehrsprachigkeit
- Bedienkonflikte und Berechtigungen

### SW5: Diagnose, Service und Exporte

- Diagnosemodell
- Lauf-, Diagnose- und Servicebericht-Export
- gefuehrter Serviceablauf mit gesperrtem Hardwarebackend
- Ressourcenmodell und Speicherbudgets als vorlaeufige Grenzwerte

Diese Reihenfolge ist kein Zwang, jede Schicht vollstaendig abzuschliessen, bevor
die naechste beginnt. Innerhalb der Abschnitte werden kleine vertikale
Funktionsscheiben umgesetzt und automatisch getestet.

## Vorgehen bei Ankunft der Hardware

Die Hardwareankunft loest keinen Neustart des Projekts aus. Die realen Adapter
werden an den bereits getesteten Kern angeschlossen.

### H0: Sichtpruefung und Dokumentation

- Platinenrevision und Modulbeschriftung erfassen
- Verdrahtungsplan und Fotos aktualisieren
- Versorgung, Masse und Stecker pruefen
- Sicherungen und Leitungsquerschnitte pruefen

### H1: Controllerboard ohne angeschlossene Aktoren

- ESP32 flashen und Reset-/Bootloaderverfahren bestaetigen
- Flashgroesse, Partition und Ressourcen messen
- alle verwendbaren GPIOs im sicheren Zustand messen
- Boot-, Reset- und Bootloaderpegel mit Multimeter und bei Bedarf Logikanalysator
  pruefen
- Onboard-MOSFET-Ausgaenge unbelastet messen

### H2: Sensoren, Display und Touch

- DS18B20 einzeln anschliessen und ROM-Adressen erfassen
- feste und abnehmbare Busse testen
- Hot-Plug des Produktfuehlers pruefen
- Displaycontroller, Rotation und Reset pruefen
- Touchcontroller und Kalibrierung pruefen

### H3: Luefter und Summer einzeln

- Ausgang zuerst unbelastet messen
- Verbraucher einzeln anschliessen
- Stromaufnahme und Anlaufverhalten messen
- aktiven Pegel bestaetigen
- Ein-, Aus- und Nachlauf pruefen
- Boot- und Resetverhalten mit angeschlossenem Verbraucher wiederholen

### H4: BTS7960 ohne angeschlossenes Peltier

- Logikversorgung und Masse pruefen
- Enable- und Richtungseingaenge unbelastet messen
- Hardware-Pulldowns nachweisen
- niemals beide Richtungen gleichzeitig freigeben
- H-Brueckenausgang und Polaritaet mit Multimeter pruefen
- R_IS/L_IS nur nach sicherer Pegelpruefung anschliessen

### H5: Peltier als begrenzter Servicepuls

Erst nach bestandenen H0 bis H4:

- 7,5-A-Sicherung eingesetzt
- Aussenluefter angeschlossen und geprueft
- Innenluefter angeschlossen und geprueft
- Schrankluft- und Kuehlkoerpersensor gueltig
- Heatsink und thermische Kopplung montiert
- Richtung eindeutig bestaetigt

Dann:

```text
kurzer begrenzter Heizpuls
  -> AUS
  -> Nachlauf
  -> Mindest-Auszeit und Totzeit
  -> kurzer begrenzter Kuehlpuls
  -> AUS
  -> Nachlauf
```

Die erste reale Peltierfreigabe erfolgt ausschliesslich im Bring-up-/Servicemodus,
niemals als normaler automatischer Lauf.

## Verbindliche Entwicklungsreihenfolge nach der Hardwareankunft

Nach dem Bring-up werden die bis dahin simulierten Adapter schrittweise durch
reale Implementierungen ersetzt:

1. reale Zeit- und Persistenzadapter
2. reale DS18B20-Erfassung
3. Display- und Touchadapter
4. Luefter- und Summeradapter
5. BTS7960-Adapter mit gesperrtem Peltier
6. begrenzte Peltier-Servicepruefung
7. reale Sicherheits- und Fehlerpruefungen
8. thermische Inbetriebnahme
9. PI-Abstimmung und Grenzwerte
10. praktische Fermentationslaeufe

Der fachliche Kern wird dabei nicht neu geschrieben, sondern gegen reale Adapter
validiert.

## Entwicklungsstil

Die Entwicklung verwendet kleine vertikale Funktionsscheiben.

Beispiel vor Hardwareankunft:

```text
Produktfuehler-Mock
  -> Messung und Qualitaetsstatus
  -> VALID/STALE/FAILED
  -> Ersatzbetrieb in Zustandsmaschine
  -> Diagnosemodell
  -> Web- und Displayanzeige
  -> automatische Tests
```

Nach Hardwareankunft wird nur der Adapter ergaenzt:

```text
DS18B20-Produktfuehleradapter
  -> gleiche Schnittstelle
  -> bestehende Tests bleiben bestehen
  -> neue Hardwaretests kommen hinzu
```

## Branch- und Pull-Request-Strategie

Verbindlich:

- Spezifikationsbranch wird zuerst als eigener Pull Request nach `main` gebracht.
- Danach entsteht ein Branch pro kleinem Implementierungs-Issue.
- Jeder Branch erhaelt einen kleinen pruefbaren Pull Request.
- Keine umfangreiche direkte Implementierung auf `main`.
- Epics gruppieren Issues, enthalten aber nicht den gesamten Code in einem
  einzigen Langzeitbranch.
- Hardwareblockaden werden als `BLOCKED_HARDWARE` markiert und verhindern nicht
  die Entwicklung unabhaengiger Softwareteile.

Moegliche Branchnamen:

```text
foundation/native-test-harness
core/state-machine
storage/config-revisions
sensors/quality-model
control/pi-core
ui/local-navigation
web/runtime-dashboard
hardware/esp32-bringup
hardware/ds18b20-adapter
hardware/bts7960-bringup
```

## Definition of Done

Ein Implementierungs-Issue ist nur abgeschlossen, wenn alle zutreffenden Punkte
erfuellt sind:

- Implementierung vollstaendig
- native oder simulierte Tests vorhanden und bestanden
- ESP32-Zielbuild kompiliert, soweit das Issue ESP32-Code betrifft
- Ressourcenwirkung geprueft oder als noch hardwareabhaengig markiert
- Fehlerfaelle behandelt
- Dokumentation aktualisiert
- keine Geheimnisse eingecheckt
- keine unbestaetigte Hardwareannahme als Tatsache eingebaut
- Akzeptanzkriterien des Issues erfuellt

### Abschluss vor Hardwareverifikation

Ein Software-Issue darf vor Ankunft der Hardware als abgeschlossen gelten, wenn:

- sein fachlicher Inhalt vollstaendig durch native Tests oder Simulation abgedeckt
  ist,
- reale Hardwareadapter nicht Teil seines Scopes sind,
- verbleibende Hardwarevalidierung in einem eigenen verknuepften Issue steht.

Beispiel:

```text
PI-Reglerkern und Totzeitlogik: DONE
Reale BTS7960-Ausgabe und Polaritaet: BLOCKED_HARDWARE
Thermische PI-Parameter: TBD_COMMISSIONING
```

Damit wird nicht behauptet, der gesamte Fermentationsschrank sei fertig, obwohl
der Softwarekern bereits abgeschlossen ist.

## Meilensteine

### M0 – Softwaregrundlage und simuliertes System

- native Testumgebung
- CI
- Hardwareabstraktionen
- simulierte Sensoren und Aktoren
- ESP32-Buildprofile

### M1 – Getesteter Softwarekern

- Zustandsmaschine
- Programme und Konfiguration
- Persistenz und Wiederanlauf
- Sensor-, Regler- und Fehlerlogik
- vollstaendige simulierte Prozessablaeufe

### M2 – Bedienbarer Simulator

- lokale UI-Modelle
- Weboberflaeche gegen simulierten Kern
- Diagnose und Exporte
- Mehrsprachigkeit

M0 bis M2 koennen vor Ankunft der realen Hardware weitgehend erreicht werden.

### M3 – Hardware Bring-up

- Board, Pins und Pegel bestaetigt
- Sensoren, Display und Touch integriert
- Luefter und Summer bestaetigt
- BTS7960 und begrenzte Peltierpulse bestanden

### M4 – Sichere Temperatursteuerung

- reale Sensoren
- sichere Aktoren
- Fehlerreaktionen
- thermische Inbetriebnahme
- PI-Abstimmung

### M5 – Vollstaendige Integration

- lokale und Webbedienung auf realem ESP32
- Diagnose, Persistenz und Exporte
- Fehlerinjektionen und Hardwareabnahme

### M6 – Release 1

- praktische Standardprogramme
- fachliche Abnahme
- siebentaegiger Dauer- und Belastungstest
- keine offenen sicherheitsrelevanten Ursachen

## Akzeptierte Entscheidungen aus Phase 10B

- [x] Epics mit kleinen abhaengigen Implementierungs-Issues
- [x] Softwareentwicklung wartet nicht auf die in etwa zwei Wochen eintreffende
      Hardware
- [x] fachlichen Kern, Simulation, Persistenz, UI-Logik und Web vor Hardwareankunft
      weitgehend entwickeln
- [x] keine separate Wegwerf-Testfirmware als Voraussetzung
- [x] geschuetztes `esp32_bringup`-Profil innerhalb derselben Codebasis
- [x] reale Hardware hinter nativ mockbaren Schnittstellen kapseln
- [x] vor Anschluss von Lueftern und Peltier Ausgaenge mit Multimeter
      beziehungsweise geeigneter Messtechnik pruefen
- [x] Peltier erst nach bestaetigten Pins, Pegeln, Sensoren, Lueftern, BTS7960 und
      Sicherung als begrenzter Servicepuls freigeben
- [x] kleine vertikale Funktionsscheiben mit Tests und Dokumentation
- [x] Branch pro Implementierungs-Issue und kleine Pull Requests
- [x] Definition of Done erlaubt Abschluss hardwareunabhaengiger Software-Issues vor
      realer Hardwarevalidierung
- [x] Hardwarevalidierung bleibt als eigenes verknuepftes Issue sichtbar
- [x] Meilensteine M0 bis M6 mit vorgezogener Software- und Simulatorphase

## Noch offen fuer den Abschluss von Phase 10B

- konkrete Epic- und Issue-Liste
- Abhaengigkeiten zwischen den Issues
- Kennzeichnung `BLOCKED_HARDWARE`, `TBD_COMMISSIONING` und
  `TBD_IMPLEMENTATION_BUDGET`
- Festlegung des ersten Implementierungs-Issues nach Merge der Spezifikation
- Entscheidung, welche UI-Teile vor Hardwareankunft als echter ESP32-Build und
  welche nur gegen View-Modelle beziehungsweise Simulator umgesetzt werden
