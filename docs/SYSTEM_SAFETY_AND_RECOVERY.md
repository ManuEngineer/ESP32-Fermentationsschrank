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

## Issue #24 Release-1-KISS-Abgrenzung

Fuer #24 gilt: Resetursache ist reine Diagnose. Power-on, externer Reset,
Brownout, Watchdog, Panic und unbekannte Ursache starten mit abstraktem
all-off/`Unresolved` und vollstaendiger Revalidierung. R1 fuehrt keinen
Restart-Zaehler, kein Resetzeitfenster, keinen allgemeinen persistenten
Safety-Latch und keinen persistenten Watchdog-/Sensor-/Thermal-Latch ein.
`SAFE_BOOT` entsteht aus aktuell nicht vertrauenswuerdigem System-, Config- oder
Persistenzzustand, nicht aus einer Neustartakkumulation.

Kontrollierter automatischer Restart, Service-PIN-Reset und automatische
thermische `SAFETY_RECOVERY` bleiben ausserhalb des #24-R1-Produktpfads.
Hardware- und thermische Nachweise gehoeren zu E5 und den dort genannten
Inbetriebnahme-Gates.

Issue #124 ersetzt die historische #24-Entscheidung fuer den eng begrenzten
Current-`FERMENTING`-Fall: Ist der aktuelle Run-/Checkpointgraph vollstaendig
validiert, `priorBootPhaseElapsed` exakt fuer `FERMENTING` getaggt und eine
vertrauenswuerdige aktuelle UTC vorhanden, wird die Phase mit der Wandzeit
seit dem Checkpoint logisch automatisch fortgesetzt. Das ist keine
Benutzerentscheidung und keine Aktorfreigabe. Die Aktoren bleiben nach jedem
Boot AUS, bis die bestehenden frischen Config-/Sensor-/Hardware-/Safety- und
Planner-Gates erfolgreich sind.

Fehlt UTC unmittelbar nach Boot, bleibt der FSM-Zustand
`RecoveryEvaluation` mit der RAM-Disposition `WaitingForTrustedTime`. Es
werden weder Tombstone noch Recoverykandidat nur fuer das Warten persistiert;
die gleiche revisionsgebundene Evidenz wird spaeter erneut bewertet. Eine
negative UTC-Differenz, nicht exakte Zeitbasis oder untrusted Persistenz bleibt
fail-closed. `wall_clock_since_checkpoint_seconds` bezeichnet Wandzeit seit
dem Record und wird nicht als exakte physische Ausfalldauer ausgegeben.

### R5.9-Produkt-Recovery-Gate und Backendcharakterisierung

Die technische Backendcharakterisierung ist keine Produktfreigabe. Fuer
Issue #90 bleiben beide Ebenen getrennt und beobachtbar:

```text
backend_characterization:
    observed | known_limitation | unexpected_change

product_recovery_gate:
    PASS | FAIL | NOT_RUN
```

Callback 12/`NotFound` bleibt als
`BACKEND_POWER_CUT_CHARACTERIZATION` / `KNOWN_BACKEND_LIMITATION` sichtbar.
Ein Backend-FAIL oder eine bekannte Limitation darf nur dann mit einem
Produkt-Recovery-PASS koexistieren, wenn die hoehere Recoverylogik den
Record-/Generations-/Runzustand vollstaendig validiert, unklare Zustaende
korrekt klassifiziert und fail-closed bis zum logischen Gate bleibt. Kein
Recoverystatus, keine Projektion und keine UI erzeugt daraus allein eine
physische Aktorfreigabe; #90 bleibt actor-free.

## Versorgungskonzept des ersten Releases

### Verbindliche Basis

Das erste Release verwendet mindestens:

- ESP32-Brownout-Erkennung
- Auswertung der Resetursache
- sichere Ausgangsinitialisierung nach jedem Reset
- Resetursache als begrenzte Diagnoseevidenz; #24 fuehrt keine
  Neustartakkumulation ein

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

## E5/#35/Future: unabhaengige thermische Notabschaltung

Dieser Abschnitt beschreibt eine spaetere physische Schutzschicht. Die
Temperatursicherung, thermisch verriegelte Fehler und Serviceanforderungen sind
kein implementierter #24-R1-Producer und erzeugen in R1 keine neuen FaultCodes.

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
- spaeteres Hardware-/Service-Gate mit physischer Pruefung erforderlich;
  kein #24-R1-Service-PIN-Vertrag,
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

## Software-Watchdogs und kein automatischer Neustart in #24-R1

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
  -> Fehlerzustand diagnostisch melden, soweit moeglich
  -> bei Bedarf normal fail-closed booten
```

### SAFE_BOOT im #24-Release-1-Pfad

Issue #24 fuehrt hier keinen Neustartzaehler und kein Resetzeitfenster ein.
Jede Resetursache startet all-off und wird vollstaendig neu validiert.
`SAFE_BOOT` entsteht nur aus aktuell nicht vertrauenswuerdigem System-, Config-
oder Persistenzzustand. Kontrollierter automatischer Restart und spaetere
Bootloop-Politik sind keine #24-R1-Funktion.

In `SAFE_BOOT` gilt:

- Peltier und H-Bruecke bleiben AUS,
- kein Fermentationslauf wird automatisch fortgesetzt,
- direkte Aktortests sind standardmaessig gesperrt,
- passive Diagnose und Fehlerexport bleiben verfuegbar, soweit das System stabil
  genug ist; Firmwareupdate und geschuetzte Servicefunktionen sind spaetere
  E5-/Future-Gates ohne #24-R1-Safety-Clear,
- der Grund fuer `SAFE_BOOT` wird lokal sichtbar angezeigt,
- ein normaler Neustart allein verlaesst `SAFE_BOOT` nicht automatisch,
- Freigabe verlangt bestandene aktuelle Integritaets-/Config-/Persistenz-
  pruefungen und den bestehenden positiven Producer-/FaultCode-Pfad; ein
  Service-PIN ist kein #24-R1-Vertrag.

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
4. den RAM-/Gate-Zustand blockiert halten;
5. das kanonische #17-Gesamttransaktionsergebnis als
   `BlockedIndeterminate`/`PersistenceIndeterminate` uebernehmen;
6. automatische Prozessfortsetzung und Lauf-Recovery sperren.

Issue #24 fuehrt dafuer keinen neuen persistenten Fehler-Latch oder
Safety-Schluessel ein. Der bestehende #17-Storevertrag entscheidet, ob ein
kanonischer Zustand sicher geschrieben wurde; ein untrusted Zustand bleibt
`SAFE_BOOT` und wird nicht durch einen neuen Tombstone verdeckt.

Der sichere Ausgangszustand hat Vorrang vor weiteren wiederholten
Flash-Schreibversuchen.

### Transaktionale Freigabe und Reset des Persistenzfehlers

Ein zustandsaendernder Schritt, der unmittelbar oder spaeter Aktoren freigeben
kann, wird erst angewendet, nachdem Transaktionsabsicht und neue Revision
erfolgreich persistiert wurden. Ein unvollstaendiger oder nicht eindeutig
aufgeloester Transaktionsmarker fuehrt beim Boot zu `SAFE_BOOT`.

Vor jeder Recoverykandidaten-, Resume- oder Fresh-Start-Freigabe muessen die
bestehende Write-before-Apply-Transaktion den Gesamtstatus `Applied`, die
anschliessende FSM-Anwendung und frische Config-/Sensor-/Planner-Evidenz
liefern. Ein kanonisches `Success` benoetigt keinen zweiten Readback; nur
`CommitOutcomeUnknown` wird nach dem vorhandenen `writeExact()`-Vertrag durch
Readback aufgeloest. Nach `PreparedHead` oder Slot-Teilmutation bleibt der
Coordinator bei `BlockedIndeterminate`/unknown-safe.

Quittierung, Neustart oder ein erfolgreicher einzelner Schreibversuch allein
setzen keinen Fault-Lifecycle zurueck. Der #23-Watchdog ist nur ein
Current-Boot-RAM-Latch und wird ausschliesslich ueber
`applyExternalWatchdogFaultReset()` mit aktueller Evidenz geloescht; #24 fuehrt
keinen neuen persistenten Latch ein.

## Fehler- und Resetprotokoll

Das Fehlerprotokoll ist begrenzt und verwendet ein append-only-, Journal- oder
Ringpufferprinzip. Es darf den 4-MB-Flash nicht unbegrenzt fuellen.

Jeder relevante Eintrag enthaelt mindestens:

- stabilen Fehler- oder Ereigniscode
- FaultCode-/Disposition-Prioritaet
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
  -> Resetursache diagnostisch erfassen
  -> Konfiguration und kanonischen Persistenzstatus frisch validieren
  -> aktuelle Sensor-/Safety-/Planner-Evidenz frisch validieren
  -> nur explizite Start-/Resume-Entscheidung bewerten
  -> erst danach abstrakten Planner-/Sink-Pfad freigeben
```

Verbindliche Regeln:

- Alte GPIO-Zustaende werden nie wiederhergestellt.
- Brownout, Watchdog und Bootfehler werden diagnostisch unterschieden, aber
  nicht akkumuliert.
- Ein Neustart erzeugt keine implizite Freigabe und keinen neuen persistenten
  Safety-Latch.
- Ein exakt rekonstruierbarer Current-`FERMENTING`-Run wird nach #124 logisch
  automatisch fortgesetzt; der Stromausfall allein verlangt keine
  Benutzerbestaetigung und gibt keine Aktoren frei.
- Fehlt fuer diesen Pfad die aktuelle UTC, bleibt
  `RecoveryEvaluation/WaitingForTrustedTime` RAM-only bestehen. Ist die Lauf-
  oder Sicherheitslage anderweitig nicht eindeutig, wird fail-closed weder
  fortgesetzt noch als `NoActiveRun` umetikettiert.
- Charge-Rettung, temperaturgewichteter Ausfallfortschritt und automatische
  Fallback-Promotion sind nicht #124-R1.
- Der Neustart selbst gilt nicht als Ursachenbehebung.

## Akzeptierte Entscheidungen aus Phase 8C

- [x] ESP32-Brownout und Resetursache im ersten Release auswerten
- [x] separate 12-V-ADC-Ueberwachung nur architektonisch und im Pinbudget
      vorbereiten, nicht als Pflicht-Hardware
- [x] ohne eingebauten Spannungsteiler keine 12-V-Messwerte behaupten
- E5/#35/Future: einmalige Temperatursicherung als spaetere unabhaengige
      thermische Notabschaltung
- [x] vorhandener zusaetzlicher DS18B20 ersetzt keine unabhaengige Abschaltung
- [x] BTS7960-Eingaenge durch Hardware-Pulldowns beziehungsweise nachgewiesen
      sichere Beschaltung inaktiv halten
- [x] Boot-, Reset- und Bootloaderverhalten aller verwendeten Ausgaenge
      fail-closed funktional pruefen; eine generelle elektrische
      Pegelmessung ist fuer R1 nicht erforderlich
- [x] Resetcause diagnostisch auswerten, ohne Neustartakkumulation
- [x] letzte gueltige Konfigurationsrevision als Rueckfall verwenden
- [x] niemals automatischen Werksreset wegen Datenfehler ausloesen
- [x] nicht rekonstruierbaren aktiven Lauf sicher stoppen, ohne Benutzerprogramme
      zu loeschen
- [x] nichtkritische Historienfehler erlauben Weiterbetrieb mit Warnung
- [x] kritische Persistenzfehler schalten das Peltier aus und halten das Gate
      unknown-safe
- [x] unvollstaendige Transaktionen sperren Recovery; kein neuer Safety-Latch
- [x] #23-Watchdog-Reset nur explizit im aktuellen Boot nach frischer Evidenz
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
- Boot-/Resetverhalten aller Ausgaenge funktional fail-closed pruefen; keine
  generelle R1-Pegelmesspflicht
- E5/Future: konkrete Watchdog-Zeiten und Neustartzaehlergrenzen fuer spaetere
  Hardware-/Betriebsvertraege; nicht #24-R1
- spaetere Hardware-/Service-Definition des `SAFE_BOOT`-Zugangs; der #24-R1-
  SAFE_BOOT-Ausgang bleibt durch aktuelle System-/Config-/Persistenzevidenz
  bestimmt
- Speicherbudget und konkrete Journalgroesse
- genaue Fehleraufbewahrungs- und Verdichtungsregeln
- Tests fuer Brownout waehrend Flash-Schreibvorgang und Aktorumschaltung
- Tests fuer beschaedigte Konfiguration, Laufrevision und Fehlerjournal
