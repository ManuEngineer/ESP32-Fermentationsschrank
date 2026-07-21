# Akzeptanztests und Release-Gates

## Status

Dieses Dokument definiert die verbindlichen Testebenen, Fehlerinjektionen,
Hardwareabnahmen und Release-Gates fuer Release 1. Die Korrekturen aus den
Reviews von PR #38 sind integriert. Exakte thermische Grenzwerte,
Regelparameter und Ressourcenschwellen bleiben bis zu den jeweiligen Messungen
`TBD_COMMISSIONING` beziehungsweise `TBD_IMPLEMENTATION_BUDGET`.

## Grundsaetze

- Sicherheitskritische Funktionen werden gezielt unter Fehlerbedingungen getestet.
- Native Tests ersetzen keine Hardwaretests; Hardwaretests ersetzen keine
  deterministischen Softwaretests.
- Jeder formelle Test verweist auf eine Anforderung, Entscheidung, Fehlernummer
  oder Sicherheitsregel.
- Ein Test darf keine unkontrollierte Aktorfreigabe oder Umgehung der normalen
  Sicherheitslogik verlangen.
- Hardwaretests verwenden den bestaetigten Hardwarestand, die dokumentierte
  Verdrahtung und einen gespeicherten Servicebericht.
- Ein nicht ausgefuehrter Test ist `BLOCKED` oder `NOT_RUN`, nicht bestanden.
- Ein bestandener Einzeltest hebt keinen anderen aktiven Sicherheitsfehler auf.
- Ein Neustart gilt nie als Fehlerreset.
- `SAFE_BOOT` bleibt in allen Tests aktorfrei.

## Testebenen

### Ebene 1: Native Unit-Tests

Mindestens:

- Programm- und Konfigurationsvalidierung
- kanonische Zustandsuebergaenge
- Bootprioritaet fuer Bootschleifen, persistierte Sperren und Speicherfehler
- Wiederherstellung eines persistierten `COMPLETED`
- virtuelle monotone und absolute Zeit
- Ausfallzeit als Unter-/Obergrenze
- kein automatischer Phasenabschluss bei ueberlappendem Unsicherheitsintervall
- Zielqualifikation und Gnadenzeit
- PI-Reglerkern und Luftbegrenzung
- Impulsakkumulator
- Mindest-Einschaltzeit, Mindest-Ausschaltzeit und Totzeit
- Sensorstatus `VALID`, `STALE`, `FAILED`
- Fehlerklassifikation, Quittierung und Fehlerreset
- Persistenzschema, atomare Revisionen und Rueckfall
- Transaktionsabsicht vor aktorwirksamer Zustandsaenderung
- Persistenzfehler-Latch und Bootauswertung
- Aufbewahrung und Bereinigung
- PIN-unabhaengiger Vollreset-Ablauf als Zustands- und Berechtigungslogik

Tests sind reproduzierbar und unabhaengig von realer Uhrzeit, Netzwerk und
zufaelliger Taskplanung.

### Ebene 2: Simulierte Gesamt- und Fehlerablaeufe

Mindestens:

- Standby -> Vorheizen -> Produkt einsetzen -> Zielqualifikation -> Fermentation
- luftgefuehrter Lauf ohne Produktfuehler
- Produktfuehlerausfall, validierter Luft-Ersatzbetrieb und Rueckkehr
- Heizen, Neutralbereich, Kuehlen und Richtungswechsel
- Stromunterbrechung in jeder Prozessphase
- fehlende NTP-Zeit ohne erfundenen Fortschritt
- spaeterer NTP-Abgleich mit Ausfallintervall
- Zeitintervall innerhalb einer Phase
- Zeitintervall ueber einer Abschluss- oder Haltegrenze
- persistierte Verriegelung plus Neustart -> `SAFE_BOOT`
- wiederholter Watchdog oder Bootschleife -> `SAFE_BOOT`
- unvollstaendige Persistenztransaktion -> `SAFE_BOOT`
- kritischer Persistenzschreibfehler -> sichere Abschaltung
- korrupter Kontrollpunkt mit sicherem Rueckfall
- `COMPLETED` bleibt nach Neustart `COMPLETED`
- kein Service- oder Aktortest aus `SAFE_BOOT`
- Quittierung ohne Fehlerreset
- Benutzerentscheidung bei `WARNING_REQUIRES_ACTION`

Die Simulation prueft erwartete Zustaende, Meldungen, Revisionen und abstrakte
Aktorbefehle. Eine verbotene Aktorfreigabe laesst den Test fehlschlagen.

### Ebene 3: Build- und statische Integrationstests

Mindestens:

- `native`, `esp32_bringup` und `esp32_release` bauen reproduzierbar
- reale Zielkonfiguration verwendet 4 MB Flash
- keine PSRAM-Abhaengigkeit
- dokumentierter Partitionsplan ohne Release-1-Web-OTA
- Firmware- und Ressourcenbericht
- Factory-Konfiguration und Schemaversionen
- Deutsch, Spanisch und Englisch
- Web- und lokale UI-Ressourcen
- keine eingebetteten Geheimnisse
- keine produktiv verwendeten `TBD`-Werte
- keine unbestaetigten Pins, Pegel oder Controller als freigegebene Werte
- Zukunftsfunktionen bleiben deaktiviert

Ein Build ist keine Hardwarefreigabe.

### Ebene 4: Elektrische und Hardwaretests

Vor einer thermischen Belastung:

- GPIO-Zuordnung und aktive Pegel
- Boot-, Reset-, Brownout- und Bootloaderverhalten
- sichere H-Bruecken- und MOSFET-Zustaende
- BTS7960-Pulldowns, Enable, Richtungen und Abschaltung
- drei DS18B20 mit ROM-Zuordnung
- 1-Wire-Bustopologie und Produkt-Hot-Plug
- Displaycontroller, Touchcontroller, Rotation und Kalibrierung
- Innen- und Aussenluefter
- Summer
- R_IS/L_IS nur bei nachgewiesener Nutzbarkeit
- PIN-unabhaengiger lokaler Vollreset ohne Aktorwirkung
- UART-Flash- und Recoveryweg

### Ebene 5: Thermische Inbetriebnahme

Mit leerem Schrank und definierten Testmassen:

- Aufheizen, Abkuehlen und Halten
- Temperaturverteilung
- Produkt-Luft-Differenz
- Kuehlkoerper- und Luefterreaktion
- PI-Parameter je Sensorrolle und Richtung
- Zielband, Qualifikation und Gnadenzeit
- Luftbegrenzungen
- Mindestimpuls, Mindestzeiten und Totzeit
- Sicherheits-Eingriffs- und harte Notgrenzen
- fehlende thermische Reaktion
- thermisches Modell fuer Unterbrechungen, sofern verwendet
- Temperatursicherung: Rating, Montageort und thermische Wirksamkeit

### Ebene 6: Praktische Fermentationslaeufe

Erst nach bestandenen Software-, Hardware- und thermischen Gates:

- Joghurt mild
- Joghurt stichfest
- Milchkefir
- Wasserkefir

Bewertet werden Bedienung, Vorheizen, Zielqualifikation, produkt- und
luftgefuehrter Betrieb, Temperaturverlauf, Abschluss, Kuehlen und Halten. Ein
gelungenes Produkt ersetzt keine technische Sicherheitspruefung.

## Automatische Pruefungen je relevantem PR

1. native Unit-Tests
2. simulierte Prozess- und Fehlerablaeufe
3. Konfigurations- und Schemavalidierung
4. Persistenz-, Transaktions-, Rueckfall- und Migrationspruefungen
5. PlatformIO-Builds
6. Geheimnis- und lokale-Konfigurationspruefung
7. Pruefung auf produktive `TBD`- oder unbestaetigte Hardwarewerte
8. Groessenbericht fuer Firmware und statische Ressourcen

Ein fehlgeschlagener Sicherheits-, Persistenz-, Recovery- oder Kernfunktionstest
blockiert den Merge und das Release.

## Release-Gates

### Gate 0: Spezifikation und Rueckverfolgbarkeit

Vor Implementierungsfreigabe:

- verbindliche Anforderung oder Entscheidung vorhanden
- zugehoerige Testidee vorhanden
- Hardware-, Inbetriebnahme- und Budget-TBDs sichtbar
- Reviewkorrekturen von PR #38 eingebunden
- keine sicherheitskritische Annahme als bestaetigte Tatsache

### Gate 1: Softwarekern

Vor realem Aktorbetrieb:

- Zustandsmaschine nativ getestet
- Bootreihenfolge und `SAFE_BOOT` getestet
- `COMPLETED`-Wiederherstellung getestet
- Sensor- und Fehlerlogik getestet
- Aktorfreigabelogik getestet
- Mindestzeiten, Totzeit und Watchdog getestet
- Persistenz, Transaktionsmarker und Rueckfall getestet
- Ausfallintervall und Zeitunsicherheit getestet
- kein Aktortest aus `SAFE_BOOT` erreichbar
- alle sicherheitsrelevanten automatischen Tests bestanden

### Gate 2A: Elektrische Freigabe ohne Peltier

Vor Anschluss beziehungsweise Bestromung des Peltiers:

- GPIOs und aktive Pegel bestaetigt
- sichere Boot-, Reset- und Bootloaderpegel bestaetigt
- BTS7960 ohne Peltier geprueft
- beide Richtungen koennen nie gleichzeitig aktiv sein
- Ausgang und Polaritaet mit Multimeter bestaetigt
- Schrankluft- und Kuehlkoerpersensor bestaetigt
- Aussenluefter und Nachlauf bestaetigt
- 7,5-A-Ueberstromsicherung installiert
- Kuehlkoerper und Waermetauscher montiert
- Servicebericht bis zu diesem Gate gespeichert

### Gate 2B: Erster realer Peltier-Puls

Vor **jedem ersten bestromten Peltier-Puls** muessen zusaetzlich erfuellt sein:

- einmalige Temperatursicherung installiert
- Temperatursicherung auf Durchgang geprueft
- Montageort dokumentiert
- Rating innerhalb der aktuellen Inbetriebnahmerevision freigegeben
- Aussenluefter unmittelbar zuvor erfolgreich getestet
- Pflichtsensoren aktuell `VALID`
- kein Fehler, keine Verriegelung und kein `SAFE_BOOT`
- validiertes `STANDBY` und PIN-geschuetzter Serviceablauf
- Leistung und Dauer firmwarefest begrenzt
- grosser jederzeit wirksamer Abbruch

Fehlt eine dieser Voraussetzungen, bleibt das Peltier spannungslos. Die
Temperatursicherung darf nicht erst nach ersten Pulsen nachgeruestet werden.

Nach dem Heizpuls folgen Peltier AUS, Nachlauf, Mindest-Ausschaltzeit und Totzeit,
bevor ein begrenzter Kuehlpuls erlaubt ist.

### Gate 3: Thermische Inbetriebnahme

Vor echten Fermentationslaeufen:

- leerer Schrank sowie kleine und grosse Testmasse vermessen
- Heizen, Kuehlen und Richtungswechsel abgestimmt
- Luftbegrenzungen festgelegt
- Sicherheits-Eingriffs- und harte Notgrenzen validiert
- Temperaturverteilung und kritischste Stellen bestimmt
- Temperatursicherung thermisch bewertet und dokumentiert
- keine unbekannte sicherheitsrelevante thermische Abweichung

### Gate 4: Dauer- und Belastungstest

Vor Release 1:

- mindestens sieben zusammenhaengende Tage
- keine unerklaerten Resets, Watchdogs oder Brownouts
- keine unerlaubte Aktorfreigabe
- keine relevante fortschreitende RAM-Leckage
- Speicherbereinigung innerhalb der Budgets
- kritische Persistenz und Sperren nach Unterbrechung wiederherstellbar
- Web, Display, Sensoren, Exporte und Regelung parallel stabil
- Fehler- und Resetjournal innerhalb des Budgets

### Gate 5: Releasekandidat

- Standardprogramme praktisch geprueft
- lokale und Webbedienung geprueft
- Stromunterbrechungs- und Recoveryablaeufe geprueft
- Exporte und Diagnose geprueft
- bekannte Abweichungen bewertet
- keine offene unbekannte Sicherheitsursache
- alle sicherheits- und kernfunktionsrelevanten Tests `PASS`

## Verpflichtende Fehlerinjektionen

### Sensoren

- Schrankluftfuehler in Standby, Vorheizen, Fermentation und Kuehlen ausfallen lassen
- Produktfuehler entfernen, Fallback und Rueckkehr pruefen
- Kuehlkoerpersensor im Peltierbetrieb ausfallen lassen
- CRC-Fehler, Busunterbrechung, `STALE` und unrealistische Spruenge
- widerspruechliche Produkt-, Luft- und Kuehlkoerperwerte

### Aktoren und Thermik

- veraltete Regelanforderung
- gleichzeitige Richtungsanforderung
- kurze und dauerhafte Gegenanforderung
- Mindest-Ausschaltzeit und Totzeit
- Aussen- und Innenluefterfehler
- fehlende thermische Peltierreaktion
- Sicherheits-Eingriffsgrenze und harte Notgrenze
- Abbruch eines Servicepulses
- Peltier-Test ohne Temperatursicherung muss blockiert werden
- Aktortest aus `SAFE_BOOT` muss blockiert werden

### Versorgung, Zeit und Boot

- Unterbrechung in jeder wesentlichen Phase
- Brownout und wiederholte Brownouts
- Watchdog und Bootschleife bis `SAFE_BOOT`
- Neustart mit persistierter Sicherheitsverriegelung
- Neustart mit persistiertem `COMPLETED`
- fehlende NTP-Zeit
- spaeterer NTP-Abgleich
- Ausfallintervall innerhalb und ueber einer Phasengrenze
- WLAN-Ausfall bei weiterlaufendem sicheren Prozess

### Persistenz und Speicher

- neuesten Kontrollpunkt beschaedigen
- neueste Konfigurationsrevision beschaedigen
- Rueckfallrevision pruefen
- Unterbrechung waehrend kritischem Schreibvorgang
- unvollstaendigen Transaktionsmarker hinterlassen
- kritischen Speicher nicht lesbar oder nicht schreibbar simulieren
- Persistenzfehler-Latch setzen und Neustart ausfuehren
- Historienspeicher bis zur Bereinigung fuellen
- nichtkritischen RAM- oder Exportfehler erzeugen

### Bedienung und Berechtigungen

- Quittieren ohne Fehlerreset
- Resetversuch bei bestehender Ursache
- Servicefunktion ohne PIN
- Aktortest waehrend Lauf und `SAFE_BOOT`
- konfliktierende Display- und Webaktion
- alle Stopoptionen
- vergessene Service-PIN mit lokalem PIN-unabhaengigem Vollreset
- Versuch eines isolierten PIN-Resets muss abgelehnt werden

## Hardware-Abnahme

Jeder relevante Hardwarestand dokumentiert mindestens:

1. Hardwarekennung und Platinenrevision
2. Verdrahtungsreferenz und Fotos
3. Versorgungsspannungen
4. GPIOs, aktive Pegel und Bootverhalten
5. BTS7960-Enable, Richtungen und Abschaltung
6. Innen- und Aussenluefter inklusive Nachlauf
7. Summer
8. drei DS18B20 mit Rolle und ROM-Adresse
9. Bustopologie und Produkt-Hot-Plug
10. Display, Touch und Kalibrierung
11. 7,5-A-Sicherung und Leitungsquerschnitte
12. Temperatursicherung vor dem ersten Puls: Typ, Rating, Montageort und
    Durchgangspruefung
13. Peltierstrom, Heiz- und Kuehlrichtung
14. begrenzter Heiztest
15. Mindest-Ausschaltzeit und Totzeit
16. begrenzter Kuehltest
17. thermische Reaktion
18. gespeicherter Servicebericht
19. Abweichungen und Freigabestatus

Eine Aenderung an Leistungspfad, Sensorbussen, Lueftern, Temperatursicherung,
Controllerboard oder Peltier kann eine neue Teil- oder Vollabnahme verlangen.

## Siebentaegiger Dauer- und Belastungstest

Belastungsprofil:

- kontinuierliche Sensorerfassung
- Displaybetrieb und wiederholte Bedienung
- parallele Webzugriffe und Live-Aktualisierung
- wiederholte Exporte
- periodische Kontrollpunkte und Bereinigung
- mehrere Heiz-, Kuehl- und Richtungswechsel
- WLAN- und NTP-Ausfall
- mindestens eine kontrollierte Stromunterbrechung
- Meldungen, Quittierungen und Diagnoseabrufe

Aufzuzeichnen:

- freier und niedrigster Heap
- groesster zusammenhaengender Block
- Task-, Watchdog- und Resetereignisse
- Sensor- und Busfehler
- Regler- und Aktorereignisse
- Flash- und Historienbelegung
- Bereinigungen und Schreibfehler
- Web- und Exportfehler
- Temperaturstabilitaet und Richtungswechsel

Der Test besteht nur ohne unerlaubte Aktorfreigabe, unerklaerten Reset,
unbehandelten Watchdog, relevante RAM-Leckage, verlorene kritische Persistenz
oder unbekannte Sicherheitsabweichung.

## Testnachweis

Jeder formelle Test enthaelt:

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

Test-ID-Gruppen:

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

`PASS_WITH_WARNINGS` kann in Serviceberichten vorkommen, ersetzt bei einem
formellen Gate aber kein `PASS`, wenn die Warnung eine Gate-Anforderung betrifft.

## Akzeptierte Entscheidungen

- [x] sechs Testebenen
- [x] automatische native, simulierte, Persistenz- und ESP32-Buildtests
- [x] sicherheits- und kernfunktionsrelevante Tests muessen bestanden sein
- [x] verpflichtende Fehlerinjektionen
- [x] dokumentierte Abnahme jedes relevanten Hardwarestands
- [x] Temperatursicherung vor dem ersten realen Peltier-Puls
- [x] `SAFE_BOOT` bleibt aktorfrei
- [x] Boot bewertet Verriegelungen und Persistenz vor Recovery
- [x] Ausfallzeit wird als Intervall getestet
- [x] `COMPLETED` wird nach Neustart wiederhergestellt
- [x] PIN-unabhaengiger lokaler Vollreset wird getestet
- [x] mindestens siebentaegiger Dauer- und Belastungstest
- [x] formeller versionierter Testnachweis
