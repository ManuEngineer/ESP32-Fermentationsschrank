# Akzeptanztests und Release-Gates

## Status

Dieses Dokument beschreibt die in Phase 10A akzeptierten Testebenen,
automatischen Pruefungen, Release-Gates, Fehlerinjektionen, Hardwareabnahme,
Dauer- und Belastungstests, thermische Inbetriebnahme sowie den notwendigen
Testnachweis.

Die konkrete Implementierungsreihenfolge und die daraus abgeleiteten GitHub-Issues
werden in Phase 10B festgelegt. Exakte thermische Grenzwerte, Regelparameter und
Ressourcenschwellen bleiben bis zu den jeweiligen Messungen
`TBD_COMMISSIONING` beziehungsweise `TBD_IMPLEMENTATION_BUDGET`.

## Grundsaetze

- Sicherheitskritische Funktionen werden nicht nur im Normalfall, sondern gezielt
  unter Fehlerbedingungen getestet.
- Native und simulierte Tests ersetzen keine Hardwaretests; Hardwaretests ersetzen
  keine deterministischen Softwaretests.
- Jeder Test verweist auf eine Anforderung, Entscheidung, Fehlernummer oder
  Sicherheitsregel.
- Ein Test darf keine unkontrollierte Aktorfreigabe oder Umgehung der normalen
  Sicherheitslogik verlangen.
- Hardwaretests verwenden den bestaetigten Hardwarestand, die dokumentierte
  Verdrahtung und einen gespeicherten Servicebericht.
- Testergebnisse bleiben versioniert und der getesteten Firmware-, Hardware- und
  Konfigurationsrevision zugeordnet.
- Ein nicht ausgefuehrter Test ist nicht bestanden, sondern `BLOCKED` oder
  `NOT_RUN`.
- Ein bestandener Einzeltest hebt keinen anderen aktiven Sicherheitsfehler auf.

## Testebenen

### Ebene 1: Native Unit-Tests

Native Tests laufen auf dem Entwicklungsrechner ohne ESP32-Hardware und pruefen
kleine deterministische Einheiten.

Mindestens zu pruefen sind:

- Programm- und Konfigurationsvalidierung
- Zustandsuebergaenge
- Zeit- und Phasenberechnungen
- Zielqualifikation und Gnadenzeit
- PI-Reglerkern und Begrenzungen
- Impulsakkumulator
- Mindest-Ein-/Auszeit und Totzeitlogik
- Sensorstatus `VALID`, `STALE`, `FAILED`
- Plausibilitaets- und Filterlogik mit definierten Messfolgen
- Fehlerklassifikation und Verriegelung
- Berechtigungs- und Resetregeln
- Persistenzschema, Integritaetspruefung und Rueckfallrevision
- temperaturgewichtete Unterbrechungsbewertung
- Speicheraufbewahrung und Bereinigungsreihenfolge

Die Tests muessen reproduzierbar sein und duerfen nicht von realer Uhrzeit,
Netzwerk oder zufaelliger Taskplanung abhaengen.

### Ebene 2: Simulierte Zustandsmaschinen- und Fehlerablaeufe

Eine Simulationsschicht fuehrt vollstaendige Prozess- und Fehlerablaeufe mit
virtueller Zeit, simulierten Sensoren und abstrahierten Aktoren aus.

Beispiele:

- Standby -> Vorheizen -> Warten auf Produkt -> Zielqualifikation -> Fermentation
- luftgefuehrter Lauf ohne Produktfuehler
- Produktfuehlerausfall mit Wechsel auf Luft und validierter Rueckkehr
- Tag-/Nachtwechsel zwischen Kuehlen und Heizen
- Richtungswechsel mit Gegenanforderungsbestaetigung und Totzeit
- kurze und lange Stromunterbrechungen in jeder Prozessphase
- fehlende NTP-Zeit und spaetere Zeitkorrektur
- verriegelter Fehler mit Quittierung und getrenntem Fehlerreset
- Sicherheits-Eingriffsgrenze mit begrenztem Gegenrichtungsversuch
- harte Notgrenze ohne Gegenrichtung
- korrupter Kontrollpunkt mit Rueckfall auf aeltere gueltige Revision
- wiederholter Watchdog mit Uebergang zu `SAFE_BOOT`

Die Simulation protokolliert erwartete und tatsaechliche Zustaende und prueft,
dass keine verbotene Aktorfreigabe entsteht.

### Ebene 3: ESP32-Build- und Integrationstests

Der reale Zielbuild wird mit PlatformIO fuer die bestaetigte ESP32-32E-
Konfiguration erstellt.

Mindestens zu pruefen sind:

- fehlerfreier Releasebuild
- korrekte 4-MB-Flashkonfiguration
- verwendete Partitionstabelle
- Firmwaregroesse und definierter Sicherheitsabstand
- keine vorausgesetzte PSRAM
- korrekte Einbindung der Factory-Konfiguration
- Uebersetzungen Deutsch, Spanisch und Englisch
- Webressourcen und lokale UI-Ressourcen
- deaktivierte oder nicht implementierte Zukunftsfunktionen wie Web-OTA und
  benutzeraktivierbare UART-Diagnose
- keine eingebetteten Geheimnisse
- statische oder native Tests fuer bekannte Fehlercodes und Schemaversionen

Der Build allein gilt nicht als Hardwarefreigabe.

### Ebene 4: Tests am verdrahteten Hardwareaufbau

Diese Ebene prueft das reale Controllerboard, die Verdrahtung und alle Ein- und
Ausgaenge, bevor der komplette Schrank thermisch belastet wird.

Dazu gehoeren:

- GPIO-Zuordnung und aktive Pegel
- Boot-, Reset-, Brownout- und Bootloaderpegel
- Hardware-Pulldowns der BTS7960-Eingaenge
- sichere Ausgangszustaende ohne gestarteten Lauf
- drei DS18B20 mit ROM-Zuordnung
- getrennte beziehungsweise geschuetzte 1-Wire-Busse
- Display und resistiver Touch
- Innen- und Aussenluefter
- Summer
- BTS7960-Richtungen
- R_IS/L_IS nur, sofern am realen Modul brauchbar
- Peltier-Heiz- und Kuehlrichtung mit begrenzten Pulsen
- 7,5-A-Sicherung und Leistungspfad
- Temperatursicherung und deren Einbaukonzept

### Ebene 5: Thermische Tests des vollstaendigen Schranks

Der vollstaendige Schrank wird ohne Lebensmittel mit definierten Testmassen
vermessen. Diese Ebene liefert die Daten fuer PI-Parameter, Grenzwerte,
Qualifikationszeiten und Sicherheitsreaktionen.

### Ebene 6: Praktische Fermentationslaeufe

Erst nach bestandenen Software-, Hardware- und thermischen Tests werden echte
Fermentationslaeufe fuer die Standardprogramme durchgefuehrt.

Sie dienen zur fachlichen Validierung von:

- Bedienablauf
- Vorheizen und Einsetzen des Produkts
- Zielqualifikation
- produkt- und luftgefuehrtem Betrieb
- Temperaturstabilitaet
- Abschluss, Kuehlen und Halten
- praktischen Standardwerten
- Verstaendlichkeit der Meldungen

Ein gelungenes Fermentationsprodukt ersetzt keine sicherheitstechnischen Tests.

## Automatische Pruefungen bei Codeaenderungen

Bei jedem relevanten Commit beziehungsweise Pull Request werden mindestens
automatisch ausgefuehrt:

1. native Unit-Tests
2. simulierte Zustandsmaschinen- und Fehlerablaeufe
3. Konfigurations- und Schemavalidierung
4. Persistenz-, Rueckfall- und Migrationspruefungen
5. PlatformIO-Build fuer die reale Zielkonfiguration
6. Pruefung auf versehentlich eingecheckte Geheimnisse oder lokale
   Konfigurationsdateien
7. Pruefung auf unbestaetigte Platzhalter in sicherheitskritischen
   Implementierungsbereichen
8. Groessenbericht fuer Firmware und statische Ressourcen

Hardware-, thermische und siebentaegige Dauerpruefungen bleiben zunaechst
manuell beziehungsweise durch dokumentierte Testlaeufe ausgeloest.

### Verhalten bei fehlgeschlagener Automatik

- Ein fehlgeschlagener sicherheits- oder kernfunktionsrelevanter Test blockiert
  Zusammenfuehren und Release.
- Ein fehlgeschlagener reiner Komforttest wird bewertet und dokumentiert, darf
  aber nicht still ignoriert werden.
- Ein nicht ausfuehrbarer Test wird als `BLOCKED` markiert und benoetigt einen
  benannten Grund sowie einen Nachholplan.

## Release-Gates

### Gate 0: Dokumentation und Rueckverfolgbarkeit

Vor Implementierungsfreigabe:

- verbindliche Anforderung oder Entscheidung vorhanden
- zugehoerige Testidee vorhanden
- offene Hardware- oder Grenzwertannahmen sichtbar markiert
- keine sicherheitskritische Annahme als bestaetigte Tatsache formuliert

### Gate 1: Softwarekern

Vor erstem kontrolliertem Hardwarebetrieb:

- Zustandsmaschine nativ getestet
- Sensor- und Fehlerzustandslogik getestet
- Aktorfreigabelogik getestet
- Mindestzeiten, Totzeit und Watchdog getestet
- Persistenz und Rueckfall getestet
- alle sicherheitsrelevanten automatischen Tests bestanden

### Gate 2: Verdrahtete Hardware

Vor laengerem Peltierbetrieb:

- GPIOs und aktive Pegel bestaetigt
- sichere Boot- und Resetpegel bestaetigt
- BTS7960-Richtung und Abschaltung bestaetigt
- Sensorrollen und ROM-Adressen bestaetigt
- Luefterfunktion und Nachlauf bestaetigt
- Sicherung und Leistungspfad geprueft
- begrenzte Heiz- und Kuehlpulse bestanden
- Servicebericht gespeichert

### Gate 3: Thermische Inbetriebnahme

Vor echten Fermentationslaeufen:

- Schrank leer vermessen
- kleine und grosse Referenzmasse vermessen
- Heizen und Kuehlen abgestimmt
- Richtungswechsel validiert
- Luftbegrenzungen festgelegt
- Sicherheits-Eingriffs- und harte Notgrenzen validiert
- Temperaturverteilung und kritischste Stellen bestimmt
- Temperatursicherung ausgewaehlt und montiert
- keine unbekannte sicherheitsrelevante thermische Abweichung offen

### Gate 4: Dauer- und Belastungstest

Vor Release-1-Freigabe:

- siebentaegiger Belastungstest bestanden
- keine unerklaerten Resets, Watchdogs oder Brownouts
- keine relevante RAM-Leckage oder fortschreitende Fragmentierung
- Speicherbereinigung und Aufbewahrungsgrenzen funktionieren
- kritische Persistenz bleibt wiederherstellbar
- Web, Display, Sensoren und Regelung koennen parallel betrieben werden
- Fehler- und Resetjournal bleibt innerhalb des Budgets

### Gate 5: Fachlicher Releasekandidat

Vor Kennzeichnung als Release 1:

- Standardprogramme praktisch geprueft
- Bedienung lokal und im Web geprueft
- Stromunterbrechungs- und Wiederanlaufablaeufe geprueft
- Export und Diagnose geprueft
- bekannte Abweichungen bewertet
- keine offene unbekannte Sicherheitsursache
- alle sicherheits- und kernfunktionsrelevanten Tests bestanden

## Fehler- und Releaseklassifikation

### Releaseblockierend

Ein Release ist gesperrt bei:

- fehlgeschlagenem Sicherheits- oder Wiederherstellungstest
- nicht deterministischer oder unerwarteter Peltierfreigabe
- ungeklaertem Sensor-, H-Bruecken-, Luefter- oder Bootverhalten
- Datenkorruption ohne sicheren Rueckfall
- nicht reproduzierbarem Watchdog oder Reset mit moeglicher Aktorgefahr
- unbekannter sicherheitsrelevanter thermischer Ursache
- fehlendem Nachweis einer kritischen Anforderung

### Bedingt akzeptierbar

Nichtkritische Komfortfehler duerfen nur offenbleiben, wenn:

- keine Sicherheits- oder Wiederherstellungsfunktion betroffen ist
- die Einschraenkung dokumentiert ist
- ein Workaround vorhanden oder die Funktion fuer Release 1 deaktiviert ist
- ein separates Folge-Issue vorhanden ist
- die Releasehinweise die Einschraenkung nennen

### Nicht ausreichend

Folgende Begruendungen reichen nicht fuer eine Freigabe:

- "funktioniert meistens"
- "ist beim normalen Test nicht aufgetreten"
- "Neustart behebt es"
- "der Benutzer wird es vermutlich nicht ausloesen"
- "das Peltier ist nur 50 W"

## Verpflichtende Fehlerinjektionen

### Sensoren

- Schrankluftfuehler waehrend Standby, Vorheizen, Fermentation und Kuehlen abziehen
- Schrankluftfuehler kurz stoeren und Wiedererkennungsfenster pruefen
- Produktfuehler abziehen, Ersatzbetrieb pruefen und wieder anschliessen
- Kuehlkoerpersensor waehrend Peltierbetrieb abziehen
- CRC-Fehler und Busunterbrechung simulieren
- veralteten Messwert erzeugen
- unrealistischen Sprung und unplausible Aenderungsrate einspeisen
- grosse aber physikalisch moegliche Differenz zwischen Produkt und Luft testen
- widerspruechliche Sensorwerte mit unklarer Ursache simulieren

### Aktoren und Thermik

- veraltete Regelanforderung erzeugen
- gleichzeitige Richtungsanforderung simulieren
- Richtungswechsel bei kurzer und dauerhafter Gegenanforderung testen
- Totzeit und Mindest-Auszeit pruefen
- Aussenluefterfehler thermisch simulieren
- Innenluefterfehler simulieren
- fehlende thermische Reaktion des Peltiers simulieren
- Sicherheits-Eingriffsgrenze mit einem begrenzten Gegenrichtungsversuch pruefen
- harte Notgrenze ohne automatische Gegenrichtung pruefen
- Abbruch einer gefuehrten Peltierpruefung testen

### Versorgung, Zeit und Netzwerk

- Stromunterbrechung in jeder wesentlichen Prozessphase
- kurze, mittlere und lange Unterbrechung
- Unterbrechung waehrend kritischem Flash-Schreibvorgang
- Brownout und wiederholte Brownouts
- Watchdog und wiederholte abnormale Neustarts bis `SAFE_BOOT`
- WLAN-Verlust bei sicher weiterlaufendem Prozess
- NTP-Verlust und spaetere Rueckkehr
- Neustart ohne verlaessliche absolute Zeitquelle

### Persistenz und Speicher

- neuesten Laufkontrollpunkt beschaedigen
- neueste Konfigurationsrevision beschaedigen
- letzte Rueckfallrevision getrennt pruefen
- Fehlerjournal voll oder teilweise beschaedigt simulieren
- Historienspeicher bis zur automatischen Bereinigung fuellen
- temporare Exportdaten bei Stromunterbrechung pruefen
- RAM-Knappheit beziehungsweise Allokationsfehler in nichtkritischer Funktion
  simulieren
- kritische Persistenz nicht mehr schreibbar simulieren

### Bedienung und Berechtigungen

- Quittieren ohne Fehlerreset
- Resetversuch bei weiterhin bestehender Ursache
- Servicefunktion ohne PIN
- Aktortest waehrend aktivem Lauf
- konfliktierende gleichzeitige Display- und Webaktion
- Stoppen mit allen vorgesehenen Stopoptionen
- vergessenes Webpasswort und vollstaendiger lokaler Werksreset

## Hardware-Abnahme je realem Hardwarestand

Jeder veraenderte reale Hardwarestand erhaelt eine dokumentierte Abnahme.

Mindestens enthalten:

1. Hardwarekennung und Platinenrevision
2. Foto oder Verdrahtungsreferenz
3. gemessene Versorgungsspannungen
4. bestaetigte GPIO-Zuordnung
5. aktive Pegel und Boot-/Resetverhalten
6. BTS7960-Enable-, Richtungs- und Abschaltverhalten
7. Innen- und Aussenluefter inklusive Nachlauf
8. Summer
9. drei DS18B20 mit Rolle und ROM-Adresse
10. 1-Wire-Bustopologie und Hot-Plug-Verhalten des Produktfuehlers
11. Displayrotation, Touchfunktion und Kalibrierung
12. Peltierstrom und Heiz-/Kuehlrichtung
13. 7,5-A-Sicherung und Leitungsquerschnitte
14. Temperatursicherung: Typ, Montageort, Rating und Austauschbarkeit
15. begrenzter Heiztest
16. Totzeit
17. begrenzter Kuehltest
18. thermische Reaktion von Luft- und Kuehlkoerpersensor
19. gespeicherter Servicebericht
20. Abweichungen und Freigabestatus

Eine Hardwareaenderung an Leistungspfad, Sensorbussen, Lueftern,
Temperatursicherung, Controllerboard oder Peltier kann eine erneute Teil- oder
Vollabnahme verlangen.

## Siebentaegiger Dauer- und Belastungstest

### Mindestdauer

Der Releasekandidat wird mindestens sieben zusammenhaengende Tage unter
laufender Regelung getestet.

### Belastungsprofil

Der Test soll realistische und gezielte Last kombinieren:

- kontinuierliche Sensorerfassung
- Display eingeschaltet, gedimmt und wiederholt bedient
- regelmaessige Webzugriffe von mehreren Browsern beziehungsweise Geraeten
- Live-Aktualisierung der Weboberflaeche
- wiederholte Exporte
- periodische Laufkontrollpunkte
- aktive Speicherbereinigung
- mehrere Heizphasen
- mehrere Kuehlphasen
- mehrere bestaetigte Richtungswechsel
- zeitweiser WLAN- und NTP-Ausfall
- mindestens eine kontrollierte Stromunterbrechung mit Wiederanlauf
- Meldungen, Quittierungen und Diagnoseabrufe

### Aufzuzeichnende Werte

Mindestens:

- freier Heap ueber Zeit
- niedrigster freier Heap
- groesster zusammenhaengender Block
- Task- und Watchdogereignisse
- Neustarts und Resetursachen
- Sensorfehler und Bus-Neuinitialisierungen
- Regler- und Aktorereignisse
- Flash- und Historienbelegung
- Bereinigungsereignisse
- kritische und nichtkritische Schreibfehler
- Web- und Exportfehler
- Temperaturstabilitaet und Richtungswechsel

### Bestehenskriterien

Der Test ist nur bestanden, wenn:

- kein unerklaerter Neustart auftritt
- kein unbehandelter Watchdog auftritt
- keine unerlaubte Aktorfreigabe entsteht
- keine fortschreitende relevante RAM-Leckage erkennbar ist
- der groesste freie Block nicht fortschreitend bis unter die spaetere
  Mindestreserve faellt
- Speicherbereinigung die festgelegten Grenzen einhaelt
- aktive Laufpersistenz nach Unterbrechung wiederherstellbar bleibt
- Fehlerjournal und kritische Revisionen gueltig bleiben
- lokale Regelung trotz Web-, Export- und Netzwerklast stabil bleibt
- keine unbekannte sicherheitsrelevante Abweichung verbleibt

Die konkreten numerischen RAM- und Flashgrenzen werden nach den ersten realen
Build- und Lastmessungen als `TBD_IMPLEMENTATION_BUDGET` festgelegt.

## Thermische Inbetriebnahmetests

### Testaufbauten

Mindestens:

1. leerer Schrank
2. definierte kleine Referenzmasse
3. definierte groessere Referenzmasse
4. produktfuehlernahe Referenzmessung
5. mehrere zusaetzliche Messpunkte zur Beurteilung der Temperaturverteilung

Externe Referenzmessgeraete werden fuer die Abnahme dokumentiert.

### Versuchsarten

- Aufheizen aus kaltem Zustand
- Abkuehlen aus warmem Zustand
- Halten mehrerer typischer Solltemperaturen
- Stoerung durch kurzzeitiges Oeffnen
- Wechsel von Heizen zu Kuehlen
- Wechsel von Kuehlen zu Heizen
- Betrieb bei unterschiedlicher Umgebungstemperatur
- Test der Luftbegrenzung im produktgefuehrten Betrieb
- Test der Sicherheits-Eingriffsgrenzen
- Test der harten Notgrenzen nur mit sicherem kontrolliertem Aufbau
- Test der Kuehlkoerper- und Luefterdiagnose

### Zu bestimmende Parameter

- thermische Totzeiten
- Aufheiz- und Abkuehlraten
- Ueberschwingen
- Einschwingzeit
- Temperaturverteilung
- Produkt-Luft-Differenz ueber Zeit
- geeignete PI-Parameter je Sensorrolle und Richtung
- Schaltfenster und Mindestimpuls
- Mindest-Ein-/Auszeit
- Gegenanforderungsdauer und Umschalthysterese
- Luftbegrenzungen
- Sicherheits-Eingriffs- und harte Notgrenzen
- Zielband, Zielqualifikationsdauer und Gnadenzeit
- Luefternachlaufzeiten
- Kriterien fuer fehlende thermische Reaktion
- Montageposition und Ausloesetemperatur der Temperatursicherung

Kein Wert wird allein aus einem einzelnen Joghurtlauf abgeleitet.

## Praktische Fermentationsabnahme

Nach bestandener technischer Inbetriebnahme werden mindestens die vier
Standardprogramme praktisch geprueft:

- Joghurt mild
- Joghurt stichfest
- Milchkefir
- Wasserkefir

Dabei werden Bedienung, Temperaturverlauf, Programmschritte und Abschlussverhalten
bewertet. Exakte sensorische Produktqualitaet ist fachlich wichtig, wird aber
nicht als alleinige technische Sicherheitsfreigabe verwendet.

Mindestens ein Lauf soll produktgefuehrt und mindestens ein Lauf luftgefuehrt
erfolgen. Stromunterbrechungstests mit echten Lebensmitteln erfolgen erst, wenn
die simulierten und thermischen Wiederanlauftests bestanden sind.

## Testnachweis

Jeder formelle Abnahme- oder Release-Gate-Test besitzt mindestens:

```text
Test-ID
Titel
Anforderung / Entscheidung / Fehlercode
Testebene
Voraussetzungen
Hardwareversion
Firmwareversion und Commit
Konfigurations- und Tuningrevision
Testdaten und Referenzgeraete
Testschritte
erwartetes Ergebnis
gemessenes Ergebnis
Logs, Exporte, Bilder oder Servicebericht
PASS / FAILED / BLOCKED / NOT_RUN
Abweichungen
verantwortliche Person
Datum und Zeitbasis
```

### Test-ID-Schema

Vorgesehene Gruppen:

```text
UT-xxx    Native Unit-Tests
SIM-xxx   Simulation und Zustandsmaschine
BLD-xxx   Build und statische Integration
HW-xxx    Hardware und elektrische Abnahme
TH-xxx    Thermische Inbetriebnahme
FI-xxx    Fehlerinjektion
END-xxx   Dauer- und Belastungstest
FER-xxx   Praktische Fermentationslaeufe
REL-xxx   Release-Gates
```

### Statusdefinitionen

- `PASS`: erwartetes Ergebnis vollstaendig erreicht
- `FAILED`: erwartetes Ergebnis nicht erreicht
- `BLOCKED`: Test kann wegen benannter Abhaengigkeit nicht ausgefuehrt werden
- `NOT_RUN`: noch nicht ausgefuehrt, ohne Freigabewirkung

Ein `PASS_WITH_WARNINGS` darf fuer Serviceberichte verwendet werden, ersetzt bei
einem formellen Release-Gate aber kein `PASS`, sofern die Warnung eine
Gate-Anforderung betrifft.

## Akzeptierte Entscheidungen aus Phase 10A

- [x] sechs Testebenen von nativen Tests bis zu praktischen Fermentationslaeufen
- [x] automatische native, simulierte, Persistenz- und ESP32-Buildtests bei
      relevanten Codeaenderungen
- [x] alle Sicherheits-, Wiederherstellungs- und Kernfunktionstests muessen fuer
      Release 1 bestanden sein
- [x] nichtkritische Komfortfehler nur dokumentiert und ohne Sicherheitsauswirkung
      akzeptierbar
- [x] verpflichtende Fehlerinjektion fuer Sensoren, Aktoren, Versorgung,
      Persistenz, Speicher und Bedienung
- [x] dokumentierte Abnahme jedes relevanten realen Hardwarestands
- [x] mindestens siebentaegiger Dauer- und Belastungstest
- [x] strukturierte thermische Tests mit leerem Schrank sowie kleiner und grosser
      Referenzmasse
- [x] PI- und Sicherheitsparameter werden aus Messreihen und nicht nach Gefuehl
      festgelegt
- [x] formeller Testnachweis mit Test-ID, Voraussetzungen, Versionen, Messwerten,
      Belegen und eindeutigem Status

## Noch offen fuer Phase 10B und 10C

- konkrete Implementierungsreihenfolge
- Aufteilung in GitHub-Epics und Issues
- Abhaengigkeiten und Definition of Done je Implementierungsabschnitt
- erstes Minimalziel fuer Hardwareverifikation und Test-Firmware
- konkrete CI-Konfiguration
- Testframeworks fuer native und ESP32-Tests
- Ablageformat und Speicherort der Testnachweise
- konkrete Ressourcen- und Thermikgrenzwerte nach Messung
- Gesamtreview auf Widersprueche und doppelte Anforderungen
- Entscheidung ueber Pull Request und Zusammenfuehrung nach `main`
