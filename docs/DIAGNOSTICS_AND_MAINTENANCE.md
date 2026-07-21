# Diagnose, Servicepruefungen und Wartung

## Status

Dieses Dokument beschreibt die in Phase 9A akzeptierten Regeln fuer
Diagnoseansichten, lesende Diagnose waehrend eines laufenden Prozesses,
Boot-Selbsttest, gefuehrte Servicepruefungen, begrenzte Peltier-Tests und
Diagnoseexporte.

Firmwareupdate, OTA, Rollback und Wartungsmodus werden in Phase 9B ergaenzt.
Ressourcenueberwachung, Speicherlebensdauer und vorbeugende Wartung folgen in
Phase 9C.

Die UART-Schnittstelle wird im ersten Release nur fuer Flashen und technische
Entwicklung verwendet. Ein spaeterer geschuetzter UART-Diagnosemodus wird
architektonisch vorbereitet, aber nicht als Benutzerfunktion des ersten Releases
implementiert.

## Grundsaetze

- Diagnose darf die Temperaturregelung, Sicherheitsaufgaben und Aktor-Watchdogs
  nicht blockieren.
- Lesende Diagnose bleibt waehrend eines laufenden Prozesses verfuegbar.
- Direkte oder gefuehrte Hardwaretests sind nur im Standby und im
  PIN-geschuetzten Servicebereich erlaubt.
- Ein normaler Boot fuehrt keine automatische Peltier-, Luefter- oder
  Summeraktivierung aus.
- Diagnosewerte unterscheiden klar zwischen gemessen, berechnet, gefiltert,
  vermutet, nicht verfuegbar und nicht verifiziert.
- Es werden keine Messwerte oder Hardwarefunktionen behauptet, fuer die keine
  bestaetigte Quelle vorhanden ist.
- Passwoerter, Service-PIN, Sitzungen und Tokens werden weder angezeigt noch
  exportiert.
- Diagnose und Servicefunktionen muessen in das festgelegte 4-MB-Flash- und
  RAM-Budget ohne vorausgesetzte PSRAM passen.

## Zweistufige Diagnoseoberflaeche

### Lokales Touchdisplay

Das Touchdisplay bietet eine kompakte Diagnose mit den wichtigsten Informationen:

- aktueller Geraete- und Prozesszustand
- aktive Prozessphase
- aktive und verriegelte Fehler
- Schrankluft-, Produkt- und Kuehlkoerpertemperatur
- Sensorstatus `VALID`, `STALE`, `FAILED` oder nicht vorhanden
- aktuelle Regelsensorrolle
- angeforderte und tatsaechlich freigegebene Peltier-Richtung
- Innen- und Aussenluefterstatus
- WLAN- und Zeitstatus in kompakter Form
- Resetursache und Hinweis auf `SAFE_BOOT`
- Firmwareversion und Konfigurationsrevision

Die lokale Ansicht verwendet mehrere kurze Seiten oder Detailansichten. Sie muss
keine grossen Rohdatentabellen oder langen historischen Listen darstellen.

### Weboberflaeche

Die Weboberflaeche bietet die vollstaendige technische Diagnose. Sie umfasst
mindestens:

- alle lokalen Diagnosewerte
- Roh-, korrigierte und gefilterte Sensordaten
- Sensoralter, CRC- und Plausibilitaetsstatus
- Reglerausgang, Begrenzung, Impulsakkumulator und Aktorfreigabe
- Mindest-Ein-/Auszeit, Totzeit und Richtungswechselstatus
- Innen- und Aussenluefterzustand sowie Nachlauf
- aktiven Programmschnappschuss und Laufrevision
- Fehler-, Reset-, Brownout- und Watchdogjournal
- Netzwerk-, NTP- und Speicherdetails
- Heap-, Flash- und Ressourcenwerte, soweit in Phase 9C festgelegt
- Zugriff auf erlaubte Exporte und PIN-geschuetzte Servicepruefungen

Die Weboberflaeche darf die Diagnose in thematische Gruppen aufteilen:

```text
Uebersicht
Sensoren
Regelung
Aktoren
Luefter
Versorgung und Reset
Speicher und Ressourcen
Netzwerk und Zeit
Fehlerjournal
Servicepruefungen
Exporte
```

## Diagnose waehrend eines laufenden Prozesses

Waehrend eines laufenden Prozesses bleiben alle sicheren lesenden Diagnosewerte
verfuegbar.

Erlaubt sind mindestens:

- aktuelle und historische Temperaturen ansehen
- Sensorstatus und Messwertalter ansehen
- Regleranforderung und tatsaechliche Aktorfreigabe ansehen
- Fehler und Warnungen ansehen
- Diagnose- und Laufexport erstellen
- Netzwerkdiagnose und WLAN-Neuverbindung gemaess `NETWORK.md`
- Ressourcen- und Speicherdaten ansehen

Gesperrt sind waehrend eines aktiven Laufes mindestens:

- Sensoroffset oder Sensorrolle veraendern
- Touch- oder Sensorkalibrierung starten
- Aktoren direkt oder gefuehrt testen
- Peltier-Heiz- oder Kuehlpulse starten
- Luefter- oder Summertest starten
- BTS7960-Ausgaenge pruefen
- Serviceparameter der Regelung veraendern
- Werksreset oder Wiederherstellungsimport ausfuehren

Die Oberflaeche zeigt bei einer gesperrten Funktion den Grund an:

```text
Diese Servicepruefung ist waehrend eines laufenden Prozesses gesperrt.
Lauf zuerst sicher beenden.
```

## Sensordiagnose

Fuer jeden erkannten Temperatursensor werden mindestens folgende Werte
bereitgestellt:

```text
Rolle:                 Schrankluft
ROM-Adresse:           28-...
1-Wire-Bus:            intern-1
Rohwert:               41,94 °C
Korrigierter Wert:     42,06 °C
Gefilterter Regelwert: 42,00 °C
Kalibrier-Offset:      +0,12 K
Status:                VALID
Messwertalter:         0,8 s
Letzte gueltige Probe: monotoner/UTC-Zeitbezug
CRC-Fehler aktuell:    0
CRC-Fehler seit Boot:  0
Plausibilitaet:        OK
Trend:                 +0,03 K/min
Verwendung:            primaer / Begrenzung / Sicherheit / Anzeige
```

Zusaetzlich koennen angezeigt werden:

- Anzahl aufeinanderfolgender Fehler
- Zeitpunkt des letzten Zustandswechsels
- Filterinitialisierung oder Wiedererkennungsstatus
- letzte Bus-Neuinitialisierung
- erwartete und tatsaechliche Sensor-ROM-Adresse
- aktiver Ersatzbetrieb
- Grund einer Sensorabwertung

Verbindliche Darstellungsregeln:

- Ein fehlender Produktfuehler wird als `nicht angeschlossen` und nicht als
  Temperaturfehler dargestellt, sofern der Lauf keinen Produktfuehler verlangt.
- Ein `STALE`-Wert wird sichtbar als veraltet gekennzeichnet.
- Ein `FAILED`-Sensor zeigt den letzten gueltigen Wert nur zusammen mit dessen Alter
  und darf nicht wie ein aktueller Messwert erscheinen.
- Ein Kalibrier-Offset wird getrennt vom Rohwert angezeigt.
- Vermutete Fehlerursachen werden als Vermutung gekennzeichnet.

## Diagnose der Regelung und Aktoren

Die technische Diagnose zeigt mindestens:

- aktive Regelstrategie und Strategieversion
- primaeren Regelsensor
- Solltemperatur
- gefilterten Istwert
- Regelabweichung
- proportionalen Anteil
- Integratorzustand beziehungsweise Integrationsbeitrag
- Anti-Windup- oder Integralsperrstatus
- resultierende Zeitquote
- Inhalt des begrenzten Impulsakkumulators
- aktive Luftbegrenzung
- angeforderte Richtung `HEAT`, `OFF` oder `COOL`
- bestaetigte Gegenrichtungsanforderung
- Mindest-Einzeit, Mindest-Auszeit und Totzeit
- Alter der letzten gueltigen Regelanforderung
- tatsaechliche Aktorfreigabe
- Grund einer verweigerten Freigabe
- R_IS/L_IS-Werte nur, falls am gelieferten Modul verifiziert

Beispiel fuer eine verweigerte Freigabe:

```text
Anforderung:        HEAT 35 %
Freigabe:           AUS
Grund:              Kuehlkoerpersensor STALE
Naechste Pruefung:  nach neuer gueltiger Sensorprobe
```

## Nicht aktiver Selbsttest beim Boot

Der normale Boot fuehrt ausschliesslich nicht aktive Pruefungen durch.

Mindestens geprueft werden:

1. Resetursache und Neustartzaehler
2. `SAFE_BOOT`- und verriegelter Fehlerstatus
3. Firmware-, Schema- und Partitionsinformationen
4. Konfigurationsintegritaet und Rueckfallrevision
5. Laufkontrollpunkt und Wiederherstellbarkeit
6. Fehler- und Resetjournal
7. 1-Wire-Busse und erwartete feste Sensoridentitaeten
8. Produktfuehlerstatus, sofern angeschlossen
9. aktuelle Sensorwerte und Plausibilitaet
10. verfuegbare Flash- und RAM-Ressourcen
11. sichere logische Ausgangszustaende
12. Netzwerk- und Zeitinitialisierung als nicht sicherheitsblockierende Funktion

Beim normalen Boot werden nicht automatisch eingeschaltet:

- Peltier
- Innenluefter
- Aussenluefter
- Summer
- freie Onboard-MOSFET-Kanaele

Ein Luefter kann nach einem Brownout oder Sicherheitsfehler nur dann im Rahmen
einer vorher eindeutig persistierten Restwaermestrategie aktiviert werden, wenn
dieser Start selbst als sichere Wiederanlaufaktion validiert ist. Dies ist kein
allgemeiner Boot-Selbsttest.

Der Boot-Selbsttest erzeugt einen strukturierten Status:

```text
PASS
PASS_WITH_WARNINGS
SAFE_BOOT
FAILED
```

Ein `PASS` allein startet keinen alten Prozess. Der phasenbezogene validierte
Wiederanlauf bleibt separat erforderlich.

## Gefuehrter Service-Hardwaretest

### Voraussetzungen

Der gefuehrte Hardwaretest ist nur verfuegbar, wenn:

- Geraet im Standby oder `SAFE_BOOT`-Servicezustand ist
- kein aktiver Lauf vorhanden ist
- Service-PIN erfolgreich eingegeben wurde
- Schrankluft- und Kuehlkoerpersensor gueltig sind
- keine unvereinbare verriegelte Hardwareursache aktiv ist
- Versorgung und gespeicherte Konfiguration ausreichend stabil sind
- alle verwendeten GPIO-Pegel und Richtungen fuer den konkreten Hardwarestand
  bestaetigt sind

Ist eine Voraussetzung nicht erfuellt, wird der Testschritt nicht angeboten oder
mit konkreter Begruendung abgelehnt.

### Gefuehrter Ablauf

Vorgesehene Reihenfolge:

1. Firmware-, Hardware- und Konfigurationsrevision erfassen
2. Sensoren, ROM-Adressen, Busse, Rohwerte und Offsets pruefen
3. Summer kurz und zeitlich begrenzt testen
4. Innenluefter zeitlich begrenzt testen
5. Aussenluefter zeitlich begrenzt testen
6. BTS7960-Enable- und Richtungsausgaenge ohne Peltierfreigabe pruefen, soweit
   elektrisch sinnvoll und sicher messbar
7. Peltier in Heizrichtung mit begrenzter Leistung und Dauer testen
8. Peltier sicher ausschalten und Mindest-Auszeit/Totzeit abwarten
9. Peltier in Kuehlrichtung mit begrenzter Leistung und Dauer testen
10. Schrankluft- und Kuehlkoerperreaktion auswerten
11. R_IS/L_IS auswerten, falls verifiziert
12. Testbericht mit Ergebnis und Abweichungen speichern

Jeder Schritt besitzt:

- klare Beschreibung der erwarteten Reaktion
- maximal erlaubte Dauer
- maximal erlaubte Leistung
- Vorbedingungen
- laufende Sensor- und Sicherheitsueberwachung
- automatische Abschaltbedingung
- sichtbares Zwischenergebnis
- grossen jederzeit erreichbaren `Abbrechen`-Befehl

Abbruch wirkt unmittelbar:

```text
Peltier AUS
-> beide Richtungen AUS
-> Impulsanforderung verwerfen
-> notwendiger Aussenluefternachlauf
-> Test als abgebrochen protokollieren
```

## Begrenzte manuelle Peltierpruefung

Eine freie dauerhafte Peltier-Handsteuerung ist nicht erlaubt.

Zulaessig sind nur gefuehrte Heiz- oder Kuehlpulse mit:

- gueltigem Schrankluftfuehler
- gueltigem Kuehlkoerpersensor
- bestandener Sensor- und Aktorvorpruefung
- eindeutig bestaetigter Richtung
- begrenzter Leistung
- begrenzter Dauer
- fester Sicherheits-Eingriffs- und Notgrenze
- Aussenluefterbetrieb und Nachlauf
- Innenluefterbetrieb gemaess Testziel
- Aktor-Watchdog
- sofortiger Abschaltung bei Sensor-, Luefter-, Strom- oder Softwarefehler

Vor einem Richtungswechsel gelten vollstaendige Mindest-Auszeit und Totzeit.
Ein Test darf keine normalen Sicherheitsgrenzen, Pulldowns, Verriegelungen oder
Fehlerreaktionen umgehen.

Konkrete Standardwerte fuer Testleistung und Testdauer bleiben
`TBD_COMMISSIONING`. Firmwarefeste Obergrenzen verhindern, dass Servicewerte
einen unkontrollierten Dauerbetrieb erzeugen.

## Getrennte Exporte

Das erste Release verwendet drei klar getrennte Exportarten.

### Laufexport

Enthaelt mindestens:

- Laufkennung und Programmschnappschuss
- Firmware-, Konfigurations- und Tuningrevision
- Phasen und Phasenwechsel
- Soll-, Produkt-, Schrankluft- und Kuehlkoerpertemperaturen
- verdichtete Diagrammdaten
- Sensorstatus und Sensorwechsel
- Regler- und Aktorereignisse im erforderlichen Umfang
- Warnungen, Fehler und Quittierungen
- Stromunterbrechungen und Wiederanlaufentscheidungen
- Laufzeit- und Fortschrittskorrekturen
- manuelle Laufanpassungen
- Abschluss- oder Abbruchgrund

Formate:

- JSON als vollstaendiges maschinenlesbares Format
- CSV fuer geeignete tabellarische Messreihen

### Diagnoseexport

Enthaelt mindestens:

- Geraetename und Firmwareversion
- Hardware-, Schema- und Konfigurationsrevision
- Resetursache und Neustartzaehler
- `SAFE_BOOT`-Status
- aktuelle Prozess- und Fehlerzustaende
- Sensoren, Rollen, Busse, Werte, Alter und Qualitaet
- Regler- und Aktorstatus
- Luefterstatus
- Speicher- und Ressourceninformationen
- Netzwerk- und Zeitstatus
- letzte relevante Fehler-, Brownout-, Watchdog- und Resetereignisse

### Servicebericht

Enthaelt mindestens:

- eindeutige Testkennung
- Start- und Endzeit beziehungsweise monotone Zeitbasis
- Bedienquelle
- Firmware-, Hardware- und Konfigurationsrevision
- ausgefuehrte und uebersprungene Testschritte
- Vorbedingungen und Sicherheitsfreigaben
- Sensorrohwerte und gefilterte Werte
- angeforderte und tatsaechliche Aktorzustaende
- Testleistung, Pulsdauer und Richtung
- thermische Reaktion und Trendbewertung
- R_IS/L_IS-Werte, falls verifiziert
- Warnungen, Abbrueche und Fehler
- Ergebnis je Schritt sowie Gesamtergebnis

Moegliche Gesamtergebnisse:

```text
PASS
PASS_WITH_WARNINGS
FAILED
ABORTED
```

### Gemeinsame Exportregeln

Kein Export enthaelt:

- WLAN-Passwort
- Webpasswort
- Service-PIN
- Sitzungskennung
- Anmelde- oder CSRF-Token
- private Schluessel
- nicht bestaetigte oder erfundene Messwerte

Exporte sind versioniert und enthalten eine Schema-ID. Ein fehlender Wert wird
als fehlend oder nicht verfuegbar codiert und nicht durch `0` ersetzt.

## UART im ersten Release

### Verbindliche Entscheidung

Die UART-Schnittstelle dient im ersten Release:

- dem Flashen ueber den FT232RL
- der Entwicklung und Inbetriebnahme durch den Firmwareentwickler
- notwendigen Bootloader- und Recovery-Arbeiten

Es gibt im ersten Release keinen normalen, ueber die Benutzeroberflaeche
aktivierbaren PIN-geschuetzten UART-Diagnosemodus.

Die produktive Firmware darf daher nicht von einem angeschlossenen UART-Terminal
abhaengen und muss auch ohne serielle Ausgabe vollstaendig diagnostizierbar sein.

### Entwicklungsausgaben

Entwicklerausgaben duerfen ueber Build-Konfigurationen oder definierte
Loggingstufen vorhanden sein. Dabei gilt:

- keine Passwoerter, PINs oder Tokens ausgeben
- keine unkontrollierten Rohdatenstroeme im normalen Releasebetrieb
- keine blockierenden seriellen Schreibvorgaenge in Regel- oder
  Sicherheitsaufgaben
- keine Aktorfreigabe durch UART-Befehle in der normalen Releasefirmware
- Debugausgaben duerfen Zeitverhalten und Speicherbudget nicht unkontrolliert
  veraendern

### Spaetere Erweiterung

Architektonisch vorbereitet wird ein spaeterer geschuetzter UART-Diagnosemodus
mit:

- ausdruecklicher lokaler Aktivierung
- Service-PIN
- begrenzter Aktivierungsdauer
- strukturierten Kategorien
- nicht blockierendem Ringpuffer
- klarer Trennung von lesender Diagnose und schreibenden Servicebefehlen
- automatischem Ende nach Inaktivitaet oder Neustart
- Geheimnisschutz

Diese Funktion wird erst in einem spaeteren Release spezifiziert und getestet.
Sie ist keine offene Pflicht fuer Release 1.

## Akzeptierte Entscheidungen aus Phase 9A

- [x] kompakte Diagnose am Touchdisplay und vollstaendige technische Diagnose im Web
- [x] sichere lesende Diagnose bleibt waehrend eines aktiven Laufes verfuegbar
- [x] veraendernde Service- und Aktortests waehrend eines Laufes gesperrt
- [x] ausfuehrliche Sensordiagnose mit Rohwert, Korrektur, Filterwert, Offset,
      Status, Alter, Fehlerzahl und Trend
- [x] normaler Boot fuehrt nur nicht aktive Selbsttests aus
- [x] keine automatische Peltier-, Luefter- oder Summeraktivierung als allgemeiner
      Boot-Selbsttest
- [x] PIN-geschuetzter gefuehrter Service-Hardwaretest nur im Standby
- [x] jederzeitiger sicherer Testabbruch
- [x] Peltierpruefung nur als zeitlich und leistungsmassig begrenzter gefuehrter Puls
- [x] getrennte Lauf-, Diagnose- und Servicebericht-Exporte
- [x] keine Geheimnisse in Diagnose oder Exporten
- [x] UART im ersten Release nur fuer Flashen und technische Entwicklung
- [x] kein benutzeraktivierbarer UART-Diagnosemodus im ersten Release
- [x] spaeteren PIN-geschuetzten UART-Diagnosemodus architektonisch vorbereiten

## Noch offen fuer Phase 9B und 9C

- OTA-Updatequelle und Signatur-/Integritaetspruefung
- Updateberechtigung und Wartungsmodus
- Verhalten bei Update waehrend eines aktiven Laufes
- duale Firmwarepartition oder alternatives Rollbackkonzept im 4-MB-Flash
- Umgang mit Schema- und Konfigurationsmigrationen bei Firmwarewechsel
- Updatefortschritt und Wiederherstellung nach abgebrochenem Update
- Ressourcenwarnschwellen fuer Heap, groessten freien Block und Flash
- Speicher- und Journalbudgets
- Lebensdauer- und Schreibzaehler
- vorbeugende Wartungshinweise fuer Sensoren, Luefter, Sicherung und Peltier
- konkrete Dauer und Leistungsgrenzen der Servicepruefungen
- konkretes Exportformat und maximale Dateigroessen
- spaetere detaillierte Spezifikation des UART-Diagnosemodus
