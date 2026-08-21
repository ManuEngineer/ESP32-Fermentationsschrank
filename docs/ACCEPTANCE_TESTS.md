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

## Issue #24 Release-1-Testgrenze

Der #24-Schnitt testet nur reale R1-Pfade: ResetCause-Diagnose mit
all-off/`Unresolved`, frische Config-/Persistenz-/Sensorvalidierung,
`NoActiveRun` fuer integer nicht resumefaehige Laeufe, technisch untrusted
Load als `SAFE_BOOT`, die #17-Gesamttransaktion, die reale #20/#21-
Sensorprojektion, den #23-Current-Boot-Watchdog-Latch, Ack ohne Freigabe und
die E3/E5/#106-Negativgrenzen.

Ein Restart-Zaehler, Resetzeitfenster, persistenter allgemeiner Safety- oder
Watchdog-Latch, Service-PIN, automatische `SAFETY_RECOVERY`, Fallback-
Promotion, gewichteter Recoveryfortschritt sowie neue Thermal-/Hardwarefaults
sind keine #24-R1-Testfaelle. Historische Testpunkte dazu bleiben fuer ihre
spaeteren Issues/Hardware-Gates gekennzeichnet und gelten nicht als #24-
Abnahmekriterium.

## Issue #90 R5.9: getrennte Nachweise

Fuer die spaetere #90-Persistenz-/Recoveryverifikation werden technische
Backendcharakterisierung und das hoehere Produkt-Recovery-Gate getrennt und
maschinenlesbar ausgewiesen:

```text
backend_characterization:
    observed | known_limitation | unexpected_change

product_recovery_gate:
    PASS | FAIL | NOT_RUN
```

Callback 12/`NotFound` bleibt als sichtbare
`BACKEND_POWER_CUT_CHARACTERIZATION` / `KNOWN_BACKEND_LIMITATION` erhalten und
ist kein Backend-PASS. Slice 2 darf mit dem
`SimulatedPersistentStateStore` erwartete Produktoutcomes deterministisch im
Produktorakel pruefen. Ein finaler #90-Produkt-Recovery-PASS ist jedoch nur
zulaessig, wenn zusaetzlich jeder relevante real aus der gepinnten
NVS-/BDL-Charakterisierung hervorgehende Cut-/Recoveryzustand durch die
hoehere Produktions-Recovery laeuft:

```text
real charakterisierter NVS-/BDL-Cut-Zustand
-> Reinitialisierung / Reboot
-> vollstaendiges Reload
-> Record-/Envelope-/CRC-/Schema-/StorageEpoch-Pruefung
-> Generation/Root/Manifest/Fallback bzw. rc0/rc1/rh0
-> Prepared/Orphan/Indeterminate-Klassifikation
-> Produkt-Recovery-Outcome
-> SafetyCore / Safe-Boot / logischer Actuator-Gate
```

Ein Simulator-PASS plus separat dokumentierte reale Backendcharakterisierung
reicht nicht fuer den finalen Produkt-PASS, solange die realen Zustaende nicht
auf Produktebene geprueft wurden. Callback 12 darf Backend-FAIL / Known
Limitation bleiben und zugleich zu einem sicheren Produkt-Recovery-PASS
fuehren, wenn die hoehere Ebene den realen Zustand korrekt erkennt und
fail-closed behandelt. Die gesamte Verifikation bleibt actor-free; UI und
physische Aktorsicherheit sind kein #90-Gate.

## Testebenen

### Ebene 1: Native Unit-Tests

Mindestens:

- Programm- und Konfigurationsvalidierung
- kanonische Zustandsuebergaenge
- Bootprioritaet fuer jede Resetcause, aktuelle Persistenzintegritaet und
  fail-closed `SAFE_BOOT`
- Wiederherstellung eines persistierten `COMPLETED`
- virtuelle monotone und absolute Zeit
- `C2-Legacy/#18`: Ausfallzeit als Unter-/Obergrenze und kein automatischer
  Phasenabschluss bei ueberlappendem Unsicherheitsintervall; kein #24-R1-Gate
- Zielqualifikation und Gnadenzeit
- PI-Reglerkern und Luftbegrenzung
- Impulsakkumulator
- Mindest-Einschaltzeit, Mindest-Ausschaltzeit und Totzeit
- Sensorstatus `VALID`, `STALE`, `FAILED`
- Regelsensorauswahl (Issue #21): vollstaendige Startmatrix ueber alle
  Programmpraeferenzen und Produktvaliditaeten; kanonische
  Entscheidungsfunktion fuer automatischen und manuellen Pfad identisch;
  laufzeitseitiger Auswahlzustand ausserhalb des Wireformats, fail-closed
  nach Restore; strukturell ungueltige externe Kompatibilitaetsevidenz
  blockiert nur die Rueckkehr, nicht unabhaengige Sicherheitsreaktionen
- begrenzte FaultCode-/Disposition-Projektion, Mehrfachfehler, Quittierung ohne
  Safetywirkung und code-spezifische positive Clear-Pfade
- Persistenzschema, atomare Revisionen und Rueckfall
- Transaktionsabsicht vor aktorwirksamer Zustandsaenderung
- #17-Transaktionsstatus und Bootauswertung ohne neue Safety-Persistenz
- reale Config-Producerprojektion ohne zweite Configuration-FSM; normale
  abgelehnte Mutationen bei gueltigem Operational-Runtime bleiben ohne
  `SAFE_BOOT`
- Unknown-Producer-Bits bleiben bei fehlender Quelle aktiv und loeschen sich nur
  durch einen spaeteren bekannten Wert derselben Quelle
- kritischer Schreibfehler sperrt neue Aktoranforderungen vor weiteren
  Persistenzversuchen; #17-Status und RAM/FSM bleiben ohne neuen persistenten
  Safety-Latch unknown-safe
- ein Fehler vor dem ersten dauerhaften #17-Write bleibt `Unchanged`; ein
  Fehler nach `PreparedHead` bleibt `BlockedIndeterminate`/`Changed`
- unvollstaendiger Transaktionsmarker fuehrt beim Boot zu `SAFE_BOOT`
- Resume-Angebot bleibt `Unresolved`; Resume und Fresh Start werden erst nach
  dem bestehenden Gesamtstatus `Applied`, FSM-Anwendung und frischer Evidenz
  freigeschaltet
- echter Fresh-Start-Bridge vom Start-Command ueber den #17-Gesamtstatus bis
  SafetyCore `Allowed`; Fehler vor `PreparedHead` und unaufgeloestes
  `CommitOutcomeUnknown` bleiben `Unresolved`
- normaler `Success` benoetigt keinen zweiten Readback; Readback erfolgt nur
  zur Aufloesung von `CommitOutcomeUnknown` durch `writeExact()`
- Aufbewahrung und Bereinigung
- physische Vollreset-/Service-PIN-Tests gehoeren zu spaeteren E4/E5-/Service-
  Gates und sind kein #24-R1-Safety-Core-Gate
- Device-Shell mit Header, exakt vier festen Slots, Home-/Zurueck-Hierarchie
  und sichtbaren leeren Slots
- gemeinsame rendererunabhaengige View-Modelle, Commands, strukturierte
  Command-Ergebnisse, Bestaetigungen und Snapshotaktualisierung fuer Touch/Web
- Textfallback aktive Sprache -> Englisch -> sichtbarer technischer Schluessel,
  Theme-Standardfallback und 320-x-240-Textlaengenvertrag
- lokale Servicefreigabe: 10 Minuten Inaktivitaet, kein UI-Parameter, keine
  R1-Maximaldauer sowie Sperre bei Neustart, Abmelden und Safetyzustandswechsel

Tests sind reproduzierbar und unabhaengig von realer Uhrzeit, Netzwerk und
zufaelliger Taskplanung.

### Ebene 2: Simulierte Gesamt- und Fehlerablaeufe

Mindestens:

- Standby -> Vorheizen -> Produkt einsetzen -> Zielqualifikation -> Fermentation
- luftgefuehrter Lauf ohne Produktfuehler
- Produktfuehlerausfall, Luft-Ersatzbetrieb (manuell und automatisch nach
  Wartezeit) sowie manuelle und automatisch validierte Rueckkehr
  (Issue #21): im aktiven Luft-Ersatzbetrieb (`AirFallbackActive`) bleibt die
  Regelung ueber Luft weiterhin freigegeben, solange Schrankluft- und
  Kuehlkoerperfuehler gueltig sind - ein ungueltiger Produktfuehler allein
  sperrt dort nicht; erst die Rueckkehr zu `NormalProduct` verlangt Produkt-,
  Schrankluft- und Kuehlkoerperfuehler gemeinsam gueltig
  (Sicherheits-Vorrangregel). Ein einzelner Schrankluft-/Kuehlkoerperausfall
  waehrend Ersatzbetrieb sperrt dagegen sofort in den sicheren Zustand
  (`SafeLocked`); Re-Arm nach einem abgebrochenen Rueckkehrversuch nur bei
  neuer Evidenzgeneration (geaenderte Kompatibilitaetsrevision oder
  zwischenzeitlicher erneuter Produktausfall), kein unbegrenztes Wiederholen
- Heizen, Neutralbereich, Kuehlen und Richtungswechsel
- Stromunterbrechung in jeder Prozessphase
- jede Resetcause: all-off/`Unresolved`, vollstaendige Revalidierung, kein
  automatischer Resume und keine Restart-Akkumulation
- Resume-Phasenmatrix: nur eindeutig fortsetzbare Phasen als Angebot, alle
  zeit-/progressabhaengigen R1-Faelle als `NoActiveRun`
- unvollstaendige Persistenztransaktion -> `SAFE_BOOT`
- kritischer Persistenzschreibfehler -> sofortige Aktorsperre, sichere
  Abschaltung und bestehender #17-Coordinator-/unknown-safe-Zustand; kein
  neuer allgemeiner Current-Boot-RAM-Latch
- Watchdog: neue Request und Ack loeschen nicht; expliziter Reset nur ueber
  den bestehenden #23-Pfad mit aktueller Evidenz
- `NoActiveRun`-Abschluss: `PreparedHead -> CheckpointSlot -> CommittedHead`
  und erst nach `Applied` Standby anwenden
- korrupter Kontrollpunkt -> bestehender technischer #17-Speichervertrag und
  `FallbackRecovered -> SAFE_BOOT`; kein Fallback-Resume und keine Promotion
- `COMPLETED` bleibt nach Neustart `COMPLETED`
- kein Service- oder Aktortest aus `SAFE_BOOT`
- Quittierung ohne Fehlerreset
- Benutzerentscheidung bei `WARNING_REQUIRES_ACTION`
- Touchnavigation ohne Wischgeste, sichtbares Pressfeedback, keine
  Doppelausloesung und erster Wake-Touch ohne Command
- SAFE_BOOT mit reduziertem aktorfreiem Diagnose-/Recoveryzugang, getrennt von
  normalem PIN-Service und von Raw-Touch-Kalibrierungsrecovery

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
- konfiguriertes Branding, Sprach-/Theme-Pakete und gezielt erzeugte Fontassets
  innerhalb des 4-MB- und ohne-PSRAM-Budgets
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
- Raw-Touch-Kalibrierungsrecovery im 10-Sekunden-Fenster getrennt von
  PIN-unabhaengigem Vollreset; keine physische Bedienannahme
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
- Reviewkorrekturen von PR #38 in aktuelle kanonische Fachquellen integriert
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
- kritischer Schreibfehler sperrt Aktoren; #17-Status und RAM/FSM bleiben bis
  `Applied` unknown-safe, ohne neuen persistenten Safety-Latch
- der bestehende #23-Current-Boot-Watchdog-Latch wird nur ueber den
  vorhandenen expliziten Resetpfad mit frischer Evidenz geloescht
- Recoveryangebot, Resume und Fresh Start bleiben vor `Applied`/FSM/frischer
  Evidenz `Unresolved`; ein Fresh Start wird ueber den echten Application-Bridge
  bis SafetyCore nachgewiesen
- normale Config-Ablehnungen mit gueltigem Operational-Runtime erzeugen keinen
  Safety-Fault; echte Producer-/Integrity-/Indeterminate-Signale bleiben
  fail-closed
- `C2-Legacy/#18`: Ausfallintervall, alte Zeitunsicherheit und gewichtete
  Charge-Recovery sind kein #24-R1-Gate
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
- E5/#35/Future: Peltier-Test ohne bestaetigte Temperatursicherung muss
  blockiert werden
- Aktortest aus `SAFE_BOOT` muss blockiert werden

### Versorgung, Zeit und Boot

- Unterbrechung in jeder wesentlichen Phase
- Brownout und wiederholte Brownouts
- Watchdog-Trip und erneuter Boot: RAM-Latch ist nicht persistent, Boot bleibt
  trotzdem all-off und revalidiert vollstaendig
- Neustart mit persistiertem `COMPLETED`
- keine automatische Charge-Recovery, kein gewichteter/NTP-basierter R1-
  Fortschritt
- WLAN-Ausfall bei weiterlaufendem sicheren Prozess

### Persistenz und Speicher

- neuesten Kontrollpunkt beschaedigen
- neueste Konfigurationsrevision beschaedigen
- Rueckfallrevision pruefen
- Unterbrechung waehrend kritischem Schreibvorgang
- kritischen Schreibfehler bei aktiver Aktoranforderung injizieren und sofortige
  Sperre vor einem weiteren Aktorbefehl nachweisen
- unvollstaendigen Transaktionsmarker hinterlassen
- kritischen Speicher nicht lesbar oder nicht schreibbar simulieren
- #17-Cutpoints vor `PreparedHead`, nach `PreparedHead`/Slot und nach
  `CommittedHead` injizieren; Teiltransaktionen bleiben unknown-safe
- `Success` ohne zweiten Readback sowie `CommitOutcomeUnknown` mit allen drei
  vorhandenen `writeExact()`-Aufloesungen pruefen
- Historienspeicher bis zur Bereinigung fuellen
- nichtkritischen RAM- oder Exportfehler erzeugen

### Bedienung und Berechtigungen

- Quittieren ohne Fehlerreset
- Resetversuch bei bestehender Ursache
- Servicefunktion ohne PIN
- Aktortest waehrend Lauf und `SAFE_BOOT`
- konfliktierende Display- und Webaktion
- alle Stopoptionen
- Service-PIN- und Vollreset-Tests gehoeren zu den spaeteren Service-/Hardware-
  Gates, nicht zum #24-R1-Safety-Core

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
- [x] Boot bewertet aktuelle Producer-/Persistenzintegritaet vor dem
      Resume-Angebot; es gibt keine allgemeine persistente Verriegelung
- [x] `C2-Legacy/#18`: Ausfallzeit wird als Intervall dokumentiert, ist aber
      kein #24-R1-Safety-Gate
- [x] `COMPLETED` wird nach Neustart wiederhergestellt
- [x] kritischer Persistenzfehler sperrt Aktoren und haelt den bestehenden
      #17-Coordinator unknown-safe; kein neuer RAM-/Persistenz-Latch
- [x] der #23-Current-Boot-Watchdog-Latch bleibt bis zum bestehenden
      expliziten Resetpfad aktiv und wird mit frischer Evidenz geloescht
- [x] `E4/E5/Future`: physischer Vollreset, Service-Gate und PIN-Regeln sind
      kein #24-R1-Safety-Clear
- [x] mindestens siebentaegiger Dauer- und Belastungstest
- [x] formeller versionierter Testnachweis
