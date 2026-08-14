# Versorgung, Softwarefehler und sichere Systemzustaende

## Status

Dieses Dokument beschreibt die in Phase 8C akzeptierten Regeln fuer
Versorgungsueberwachung, unabhaengige thermische Abschaltung, sichere Bootpegel,
Software-Watchdogs, Datenintegritaet, Speicherfehler, Fehlerprotokoll und den
validierten Wiederanlauf.

Es ergaenzt [`SAFETY_AND_FAULTS.md`](SAFETY_AND_FAULTS.md),
[`SAFETY_COMPONENT_FAULTS.md`](SAFETY_COMPONENT_FAULTS.md),
[`RUN_PERSISTENCE.md`](RUN_PERSISTENCE.md) und
[`RECOVERY_AND_INTERRUPTION.md`](RECOVERY_AND_INTERRUPTION.md).

Konkrete Spannungs-, Temperatur- und Zeitgrenzen bleiben bis zur Hardwarepruefung
und Inbetriebnahme `TBD_COMMISSIONING`, soweit keine firmwarefeste Grenze vorher
festgelegt werden muss.

## Versorgungskonzept des ersten Releases

### Verbindliche Basis

Das erste Release verwendet mindestens:

- ESP32-Brownout-Erkennung
- Auswertung der Resetursache
- sichere Ausgangsinitialisierung nach jedem Reset
- persistente Erfassung wiederholter Brownout- oder Neustartereignisse, soweit der
  Speicher noch verlaesslich ist

Ein separater ADC-Messkanal fuer die 12-V-Leistungsspannung ist **keine
Pflicht-Hardware** des ersten Aufbaus.

### Vorbereitete optionale 12-V-Messung

Die Softwarearchitektur und Pinbudgetpruefung sollen eine spaetere Messung der
12-V-Leistungsspannung ermoeglichen. Vorgesehen waere:

- geschuetzter Spannungsteiler
- geeignete Begrenzung und Filterung fuer den ESP32-ADC
- kalibrierte Umrechnung in Versorgungsspannung
- Unterspannungs- und Instabilitaetsbewertung getrennt vom internen Brownout

Solange diese Hardware nicht bestaetigt eingebaut und kalibriert ist:

- wird keine 12-V-Spannung angezeigt oder behauptet,
- darf keine Sicherheitsentscheidung auf einem nicht vorhandenen ADC-Signal
  beruhen,
- wird nur mit Brownout-, Reset- und indirekten Aktor-/Temperaturdiagnosen
  gearbeitet.

Die optionale ADC-Erweiterung darf spaeter ergaenzt werden, ohne die
Fehlerzustandsmaschine neu zu entwerfen.

## Reaktion auf Brownout oder Versorgungseinbruch

Bei erkanntem Brownout, instabilem Neustart oder einer ausreichend begruendeten
Versorgungsstoerung gilt:

```text
Peltierfreigabe sperren
  -> beide H-Brueckenrichtungen AUS
  -> Impulsakkumulator verwerfen
  -> Integralanteil sperren
  -> Luefter nur weiterbetreiben, solange Versorgung und Ausgangszustand dies
     sicher erlauben
  -> Lauf- und Fehlerzustand sichern, soweit noch verlaesslich moeglich
  -> Resetursache fuer den naechsten Boot hinterlegen
```

Es wird nicht versucht, bei zusammenbrechender Versorgung noch eine neue
Peltier-Gegenrichtung zu starten.

Nach Rueckkehr der Versorgung erfolgt keine Wiederherstellung alter GPIO-Zustaende.
Der validierte Wiederanlauf aus diesem Dokument ist erforderlich.

## Unabhaengige thermische Notabschaltung

### Entscheidung fuer eine einmalige Temperatursicherung

Zwischen einer wiederverwendbaren Thermoschalter-Loesung und ausschliesslicher
Softwareueberwachung wird fuer den ersten Aufbau eine **einmalige
Temperatursicherung** als bessere unabhaengige Rueckfallebene festgelegt.

Die Temperatursicherung ist kein weiterer Messsensor. Sie:

- arbeitet ohne ESP32 und ohne Firmware,
- benoetigt keinen GPIO,
- oeffnet bei ihrer spezifizierten Ausloesetemperatur dauerhaft,
- muss nach Ausloesung ersetzt werden,
- unterbricht die Peltier-Leistungsfreigabe beziehungsweise den Peltier-Leistungspfad
  so, dass eine fehlerhafte Software das Peltier nicht weiter betreiben kann.

Ein vierter oder fuenfter vorhandener DS18B20 darf spaeter fuer zusaetzliche
Diagnose verwendet werden, ersetzt diese unabhaengige Abschaltung aber nicht.
Alle DS18B20 bleiben von ESP32, Versorgung, 1-Wire-Treiber und Firmware
abhaengig.

### Einbauanforderungen

Die Temperatursicherung wird thermisch an der kritischsten zu schuetzenden Stelle
montiert. Diese Stelle wird durch die Inbetriebnahmemessungen bestimmt,
voraussichtlich am heissen Aussenwaermetauscher beziehungsweise nahe der
Peltier-Heissseite.

Verbindlich zu pruefen sind:

- Ausloesetemperatur mit ausreichendem Abstand zum normalen Betrieb
- elektrische Spannungs- und Strombelastbarkeit fuer den Peltierpfad
- Gleichstrom-Eignung
- thermische Kopplung und mechanische Fixierung
- elektrische Isolation
- Schutz vor Kondensat und Zug
- sichere Austauschbarkeit im Servicefall
- Verhalten bei Heiz- und Kuehlrichtung

Die Temperatursicherung ersetzt nicht:

- den Kuehlkoerper-DS18B20,
- Softwaregrenzen,
- die 7,5-A-Sicherung gegen Ueberstrom,
- Luefterdiagnose,
- H-Bruecken- und Stromdiagnose.

Sie ist eine letzte unabhaengige thermische Abschaltschicht.

### Verhalten nach Ausloesung

Die Firmware kann eine geoeffnete Temperatursicherung je nach vorhandener
Diagnose indirekt erkennen, beispielsweise durch:

- fehlende elektrische Reaktion trotz Freigabe,
- fehlende thermische Reaktion,
- verifiziertes Stromsignal,
- Servicepruefung.

Nach bestaetigter oder stark vermuteter Ausloesung gilt:

- verriegelter Sicherheitsfehler,
- keine automatische Wiederfreigabe,
- Peltier bleibt gesperrt,
- Service-PIN und physische Pruefung erforderlich,
- Sicherung muss ersetzt und Ursache geklaert werden.

## Sicherer Ausgangszustand bei Boot, Reset und Bootloader

Hardware und Software muessen gemeinsam sicherstellen, dass das Peltier waehrend
Boot, Reset, Watchdog und UART-Bootloader nicht unkontrolliert aktiviert wird.

### BTS7960

Vorgesehen sind:

- Hardware-Pulldowns beziehungsweise eine nachgewiesen sichere Beschaltung an den
  Enable- und Richtungseingaengen
- beide Richtungen standardmaessig inaktiv
- keine Freigabe allein durch undefinierte oder floatende GPIOs
- kontrollierte Freigabe erst nach vollstaendiger Initialisierung von Sensoren,
  Zustand, Fehlerlogik und Aktor-Watchdog

### Onboard-MOSFET-Ausgaenge

Die tatsaechlichen Pegel waehrend:

- Einschalten
- Reset
- Brownout
- normalem Boot
- UART-Bootloader
- fehlender Firmware

werden am gelieferten Board praktisch gemessen.

Ist ein fuer Luefter oder Summer verwendeter Ausgang dabei nicht nachweislich
sicher, wird eine externe Pull-Beschaltung, Invertierung, Freigabestufe oder andere
Hardwareloesung vorgesehen.

`setup()` allein gilt nicht als ausreichende Sicherheitsmassnahme, weil die
Ausgaenge bereits vor Ausfuehrung der Firmware einen Pegel besitzen koennen.

## Software-Watchdogs und kontrollierter Neustart

### Getrennte Ueberwachung

Mindestens getrennt ueberwacht werden:

- Sensorerfassung
- Regelaufgabe
- Sicherheits- und Fehleraufgabe
- Aktoranforderungs-Watchdog
- Persistenzaufgabe, soweit fuer den Lauf kritisch
- Hauptsystem beziehungsweise Task-Watchdog

Ein blockierter Webserver, fehlendes WLAN oder eine langsame Anzeige darf nicht
unnoetig einen sicher laufenden lokalen Regler neu starten.

### Erste Reaktion

Kann die Sicherheits- oder Aktorlogik keinen aktuellen verlaesslichen Zustand mehr
liefern:

```text
Peltier sofort AUS
  -> beide Richtungen deaktivieren
  -> sichere Luefterreaktion ausfuehren
  -> Fehlerzustand sichern, soweit moeglich
  -> einmaligen kontrollierten Neustart vorbereiten
```

### Neustartbegrenzung und SAFE_BOOT

Ein automatischer Neustart ist nur begrenzt zulaessig. Wiederholte abnormale
Neustarts duerfen keine Endlosschleife erzeugen.

Nach einer firmwarefest beziehungsweise eng begrenzt definierten Anzahl
abnormaler Neustarts innerhalb eines Zeitfensters wechselt das Geraet in
`SAFE_BOOT`.

Ausgangspunkt fuer die spaetere technische Festlegung:

```text
3 abnormale Neustarts innerhalb eines definierten Zeitfensters
```

Der exakte Wert und das Zeitfenster werden vor Implementierung als firmwarefeste
oder nur eng servicekonfigurierbare Grenze festgelegt.

In `SAFE_BOOT` gilt:

- Peltier und H-Bruecke bleiben AUS,
- kein Fermentationslauf wird automatisch fortgesetzt,
- direkte Aktortests sind standardmaessig gesperrt,
- Diagnose, Fehlerexport, Firmwareupdate und geschuetzte Servicefunktionen bleiben
  verfuegbar, soweit das System stabil genug ist,
- der Grund fuer `SAFE_BOOT` wird lokal sichtbar angezeigt,
- ein normaler Neustart allein verlaesst `SAFE_BOOT` nicht automatisch,
- Freigabe verlangt bestandene Integritaetspruefungen und je nach Ursache die
  Service-PIN.

### Issue #24 R2 – Restart-Episode und Reset-Boot

Die firmwarefeste Episodegrenze ist ein Zaehler von drei abnormalen
Neustartursachen innerhalb einer offenen Episode. Brownout sowie
Watchdog/Panic werden beim Boot jeweils genau einmal als persistente
`RestartEvidenceId` nachgetragen; ein kontrollierter Safety-Neustart schreibt
seine Evidenz vor dem Neustart und wird beim Folge-Boot als `Consumed`
markiert. Ein autorisierter Faultreset verwendet stattdessen einen einmaligen
`FaultResetBootIntent`, zaehlt nicht als abnormal und schliesst keine offene
Episode.

Die Episode wird erst nach 30 Minuten stabiler, monotone Zeit im laufenden
Boot geschlossen. Zeitablauf, NTP, Stromlosigkeit oder ein normaler Neustart
loeschen weder Episode noch Safety-Latch. Fehlende, widerspruechliche oder
unbekannte Reset-/Persistenzevidenz fuehrt vor normaler Freigabe zu
`SAFE_BOOT`; der bestehende Prozessautomat bleibt der einzige Zustandsautomat.

## Beschaedigte Konfiguration oder Laufdaten

Alle gespeicherten Revisionen werden vor Verwendung durch Schema-, Versions-,
Laengen- und Integritaetspruefungen validiert.

### Konfiguration

Ist die neueste Konfigurationsrevision beschaedigt:

1. letzte vollstaendig gueltige Revision laden,
2. Rueckfall sichtbar protokollieren,
3. betroffene neue Revision nicht teilweise verwenden,
4. keine einzelnen zufaellig lesbaren Werte mit alten Daten vermischen.

Ist keine gueltige Konfiguration vorhanden, gilt ein sicherer Systemzustand. Ein
automatischer Werksreset ist verboten.

### Aktiver Lauf

Ist nur der aktive Laufdatensatz nicht sicher rekonstruierbar:

- Peltier und H-Bruecke bleiben AUS,
- der Lauf wird sicher beendet beziehungsweise als nicht wiederherstellbar
  markiert,
- vorhandene Diagnose- und Historiendaten bleiben erhalten,
- Benutzerprogramme und ungefaehrliche Einstellungen werden nicht geloescht,
- es werden keine Sollwerte, Phasen oder Restzeiten geraten.

Ein automatischer Werksreset ist auch hier verboten.

## Speicherfehler waehrend eines Laufes

Speicherfehler werden nach ihrer Sicherheitsrelevanz getrennt behandelt.

### Nichtkritischer Historienfehler

Beispiele:

- einzelnes Diagrammfenster kann nicht gespeichert werden
- nichtkritische Langzeithistorie ist voll oder beschaedigt
- alte Laufzusammenfassung kann nicht geschrieben werden

Reaktion:

- Prozess darf weiterlaufen, sofern kritische Laufpersistenz weiterhin sicher ist,
- sichtbare Warnung,
- Ereignis im verbleibenden Fehlerjournal erfassen,
- Historienaufzeichnung begrenzen oder deaktivieren,
- aktiven Laufkontrollpunkt priorisieren.

### Kritischer Persistenzfehler

Beispiele:

- aktueller Laufkontrollpunkt kann nicht mehr atomar gespeichert werden
- Fehlerjournal fuer verriegelte Ereignisse ist nicht mehr verlaesslich
- Konfigurationsspeicher oder Rueckfallrevision ist kritisch beschaedigt
- keine sichere Speicherrevision mehr aktivierbar

Die unmittelbare Reaktion ist verbindlich:

1. neue Aktoranforderungen sperren;
2. Peltier und beide H-Brueckenrichtungen AUS;
3. erforderlichen sicheren Luefternachlauf ausfuehren;
4. einen RAM-seitigen Persistenzfehler-Latch setzen;
5. versuchen, einen minimalen Fehler-Latch in einem reservierten, redundant
   ausgelegten Bereich ausserhalb des normalen Laufjournals zu persistieren;
6. in einen schweren verriegelten Systemfehler wechseln;
7. automatische Prozessfortsetzung und Lauf-Recovery sperren.

Scheitert auch das Schreiben des minimalen persistenten Latches, bleibt die
RAM-seitige Verriegelung bis zum Neustart wirksam. Beim naechsten Boot verhindert
jeder nicht eindeutig gueltige kritische Speicherzustand die Aktorfreigabe und
fuehrt zu `SAFE_BOOT`. Ein fehlgeschlagener Latch-Schreibversuch darf niemals als
Entwarnung behandelt werden.

Der reservierte Latch liegt getrennt vom normalen Laufjournal, aber weiterhin im
selben physischen ESP32-Flash. Ein vollstaendiger physischer Flashdefekt kann ohne
unabhaengigen externen Speicher nicht redundant ueberlebt werden. Die Firmware
darf keine hoehere Ausfallsicherheit behaupten, als die Hardware bietet.

Der sichere Ausgangszustand hat Vorrang vor weiteren wiederholten
Flash-Schreibversuchen.

### Transaktionale Freigabe und Reset des Persistenzfehlers

Ein zustandsaendernder Schritt, der unmittelbar oder spaeter Aktoren freigeben
kann, wird erst angewendet, nachdem Transaktionsabsicht und neue Revision
erfolgreich persistiert wurden. Ein unvollstaendiger oder nicht eindeutig
aufgeloester Transaktionsmarker fuehrt beim Boot zu `SAFE_BOOT`.

Vor jeder Recovery-Aktorfreigabe muessen beide Bedingungen erfuellt sein:

1. der kritische Speicher besteht eine Lesen-Schreiben-Integritaetspruefung;
2. die Recoveryentscheidung ist als neue Revision erfolgreich gespeichert und
   wieder gelesen beziehungsweise verifiziert.

Ein Persistenzfehler-Latch darf nur in einem geschuetzten Serviceablauf
zurueckgesetzt werden, nachdem:

- die Ursache behoben oder eindeutig eingegrenzt wurde;
- die kritische Lesen-Schreiben-Integritaetspruefung bestanden ist;
- kein unvollstaendiger Transaktionsmarker verbleibt;
- der Reset als eigenes Ereignis dokumentiert wurde.

Quittierung, Neustart oder ein erfolgreicher einzelner Schreibversuch allein
setzen den Latch nicht zurueck.

## Fehler- und Resetprotokoll

Das Fehlerprotokoll ist begrenzt und verwendet ein append-only-, Journal- oder
Ringpufferprinzip. Es darf den 4-MB-Flash nicht unbegrenzt fuellen.

Jeder relevante Eintrag enthaelt mindestens:

- stabilen Fehler- oder Ereigniscode
- Fehlerklasse und Prioritaet
- monotone Zeitbasis
- UTC-Zeit und Zeitzoneninformation, sofern verlaesslich
- Prozessphase und Laufrevision
- Sensorrohwerte, gefilterte Werte und Qualitaetsstatus im notwendigen Umfang
- Regleranforderung
- tatsaechliche Aktorfreigabe und Richtung
- Luefterzustaende
- Resetursache
- Brownout-, Watchdog- oder SAFE_BOOT-Bezug
- Quittierung, Resetversuch und Wiederfreigabe
- Bedienquelle
- Primaer- und Folgefehlerbeziehung

Aufbewahrungsprioritaet:

1. harte Sicherheits-, System-, Brownout-, Watchdog- und Resetereignisse
2. verriegelte Fehler und ihre Reset-/Freigabeversuche
3. relevante Betriebsfehler
4. normale Warnungen und Hinweise

Bei Platzmangel werden zuerst die aeltesten niedrig priorisierten normalen
Warnungen verdichtet oder entfernt. Kritische Ereignisse werden laenger behalten,
bleiben aber ebenfalls innerhalb eines festen Speicherbudgets.

Das Protokoll ist exportierbar und enthaelt keine Passwoerter, PINs oder Tokens.

## Validierter Wiederanlauf

Nach Unterspannung, Brownout, Software-Watchdog, kontrolliertem Neustart oder
normalem Versorgungsausfall gilt dieselbe sichere Grundreihenfolge:

```text
Boot
  -> alle Peltier- und H-Brueckenausgaenge sicher AUS
  -> Resetursache und Neustartzaehler pruefen
  -> persistierte verriegelte Fehler validieren
  -> Konfiguration und Laufrevision validieren
  -> Schrankluft- und Kuehlkoerpersensor validieren
  -> Produktfuehler und Ersatzstrategie pruefen
  -> Versorgungslage mit verfuegbaren Mitteln bewerten
  -> phasenbezogene Wiederanlaufentscheidung bestimmen
  -> Entscheidung atomar speichern
  -> erst danach Regelung und Aktoren kontrolliert freigeben
```

Verbindliche Regeln:

- Alte GPIO-Zustaende werden nie wiederhergestellt.
- Ein Brownout oder Watchdog zaehlt als relevante Unterbrechung.
- Wiederholte Brownouts, Watchdogs oder Bootfehler fuehren zu `SAFE_BOOT`.
- Ein verriegelter Fehler bleibt ueber den Neustart erhalten.
- Eine unterbrochene Zielqualifikation beginnt erneut.
- Fermentationsfortschritt wird nur gemaess der temperatur- und
  qualitaetsgewichteten Wiederanlaufregeln fortgesetzt.
- Ist die Lauf- oder Sicherheitslage nicht eindeutig, wird nicht automatisch
  fortgesetzt.
- Der Neustart selbst gilt nicht als Ursachenbehebung.

## Akzeptierte Entscheidungen aus Phase 8C

- [x] ESP32-Brownout und Resetursache im ersten Release auswerten
- [x] separate 12-V-ADC-Ueberwachung nur architektonisch und im Pinbudget
      vorbereiten, nicht als Pflicht-Hardware
- [x] ohne eingebauten Spannungsteiler keine 12-V-Messwerte behaupten
- [x] einmalige Temperatursicherung als unabhaengige thermische Notabschaltung
- [x] vorhandener zusaetzlicher DS18B20 ersetzt keine unabhaengige Abschaltung
- [x] BTS7960-Eingaenge durch Hardware-Pulldowns beziehungsweise nachgewiesen
      sichere Beschaltung inaktiv halten
- [x] Boot-, Reset- und Bootloaderpegel aller verwendeten Ausgaenge praktisch
      messen
- [x] einmaliger kontrollierter Neustart bei begruendetem Softwarefehler
- [x] wiederholte abnormale Neustarts fuehren zu `SAFE_BOOT`
- [x] letzte gueltige Konfigurationsrevision als Rueckfall verwenden
- [x] niemals automatischen Werksreset wegen Datenfehler ausloesen
- [x] nicht rekonstruierbaren aktiven Lauf sicher stoppen, ohne Benutzerprogramme
      zu loeschen
- [x] nichtkritische Historienfehler erlauben Weiterbetrieb mit Warnung
- [x] kritische Persistenzfehler schalten das Peltier aus und verriegeln das System
- [x] Persistenzfehler-Latch und unvollstaendige Transaktionen sperren Recovery
- [x] Latch-Reset nur im Service nach bestandener Speicherpruefung
- [x] begrenztes priorisiertes Fehler- und Resetjournal
- [x] nach jedem relevanten Neustart vollstaendig validierter Wiederanlauf
- [x] alte GPIO-Zustaende werden niemals wiederhergestellt

## Noch offen fuer Phase 9, Hardwarepruefung und Inbetriebnahme

- Brownout- und Resetursachen auf dem konkreten ESP32-Board verifizieren
- entscheiden, ob ein ADC-Pin fuer eine spaetere 12-V-Messung reserviert werden kann
- optionalen Spannungsteiler, ADC-Schutz und Kalibrierung spezifizieren
- Typ, Ausloesetemperatur, Strom- und Spannungsrating der Temperatursicherung
- thermisch kritischste Montageposition der Temperatursicherung
- Nachweis der sicheren Unterbrechung des Peltierpfades
- BTS7960-Pulldowns beziehungsweise externe Freigabestufe auslegen
- Boot-/Resetpegel aller Ausgaenge messen
- konkrete Watchdog-Zeiten und Neustartzaehlergrenzen
- Definition und Freigabeablauf von `SAFE_BOOT`
- Speicherbudget und konkrete Journalgroesse
- genaue Fehleraufbewahrungs- und Verdichtungsregeln
- Tests fuer Brownout waehrend Flash-Schreibvorgang und Aktorumschaltung
- Tests fuer beschaedigte Konfiguration, Laufrevision und Fehlerjournal
