# Diagnose, Servicepruefungen und Wartung

## Status

Dieses Dokument definiert die Release-1-Regeln fuer Diagnoseansichten,
Boot-Selbsttests, gefuehrte Servicepruefungen, begrenzte Peltier-Tests und
Exporte. Firmwareupdates stehen in `FIRMWARE_UPDATE_AND_ROLLBACK.md`, Ressourcen
in `RESOURCE_BUDGET_AND_MAINTENANCE.md`.

UART dient in Release 1 nur dem Flashen, der technischen Entwicklung und der
physischen Recovery. Ein benutzeraktivierbarer UART-Diagnosemodus ist
`FUTURE_RELEASE`.

## Grundsaetze

- Diagnose darf Regelung, Sicherheitsaufgaben und Aktor-Watchdogs nicht blockieren.
- Lesende Diagnose bleibt waehrend eines Laufes verfuegbar.
- Aktor- und Hardwaretests sind nur aus validiertem `STANDBY` im
  PIN-geschuetzten `SERVICE_MODE` erlaubt.
- `SAFE_BOOT` erlaubt keine leistungsbezogenen Aktortests.
- Ein normaler Boot fuehrt keine automatische Peltier-, Luefter- oder
  Summeraktivierung aus.
- Diagnose unterscheidet gemessen, berechnet, gefiltert, vermutet, nicht
  verfuegbar und nicht verifiziert.
- Nicht bestaetigte Hardwarewerte werden nicht als Messung dargestellt.
- Passwoerter, PINs, Sitzungen und Tokens werden weder angezeigt noch exportiert.
- Diagnose und Service passen in das 4-MB-Flashbudget ohne PSRAM.

## Diagnoseoberflaechen

### Lokales Touchdisplay

Mindestens sichtbar:

- Geraete- und Prozesszustand
- aktive Prozessphase
- aktive und verriegelte Fehler
- Schrankluft-, Produkt- und Kuehlkoerpertemperatur
- Sensorstatus `VALID`, `STALE`, `FAILED` oder nicht vorhanden
- primaere Regelsensorrolle
- angeforderte und tatsaechlich freigegebene Peltier-Richtung
- Innen- und Aussenluefterstatus
- WLAN- und Zeitstatus
- Resetursache und `SAFE_BOOT`
- Firmware- und Konfigurationsrevision

### Weboberflaeche

Zusaetzlich mindestens:

- Roh-, korrigierte und gefilterte Sensordaten
- Sensoralter, CRC und Plausibilitaet
- Reglerausgang, Begrenzung, Impulsakkumulator und Aktorfreigabe
- Mindest-Einschaltzeit, Mindest-Ausschaltzeit, Totzeit und Richtungswechsel
- Luefterzustand und Nachlauf
- Programmschnappschuss und Laufrevision
- Fehler-, Persistenz-, Reset-, Brownout- und Watchdogjournal
- Netzwerk-, NTP-, Flash-, Heap- und Ressourcenwerte
- erlaubte Exporte
- Servicepruefungen nur nach Service-PIN und gueltigem Zustand

## Diagnose waehrend eines Laufes

Erlaubt:

- Temperaturen und Historie ansehen
- Sensorstatus und Messwertalter ansehen
- Regleranforderung und Aktorfreigabe ansehen
- Fehler und Warnungen ansehen
- Lauf- und Diagnoseexport erstellen
- Netzwerkdiagnose und WLAN-Neuverbindung
- Ressourcen- und Speicherdaten ansehen

Gesperrt:

- Sensorrolle oder Offset aendern
- Kalibrierung starten
- Aktoren testen
- Peltier-, Luefter- oder Summerpulse starten
- BTS7960-Ausgaenge pruefen
- Regelungs-Serviceparameter aendern
- Werksreset oder Import ausfuehren

Die UI nennt den Sperrgrund und fordert zum sicheren Beenden des Laufes auf.

## Sensordiagnose

Je Sensor mindestens:

```text
Rolle
ROM-Adresse
1-Wire-Bus
Rohwert
korrigierter Wert
gefilterter Regelwert
Kalibrier-Offset
Status VALID / STALE / FAILED
Messwertalter
Zeit der letzten gueltigen Probe
CRC-Fehler aktuell und seit Boot
Plausibilitaet
Trend
Verwendung als Regelung / Begrenzung / Sicherheit / Anzeige
```

Regeln:

- Ein nicht benoetigter fehlender Produktfuehler ist `nicht angeschlossen`, nicht
  automatisch ein Fehler.
- `STALE` und Alter bleiben sichtbar.
- Ein letzter Wert eines `FAILED`-Sensors erscheint nie wie eine aktuelle Messung.
- Vermutete Ursachen werden als Vermutung bezeichnet.

## Regelungs- und Aktordiagnose

Mindestens sichtbar:

- Strategie und Version
- primaerer Regelsensor
- Soll- und gefilterter Istwert
- Regelabweichung
- P- und I-Anteil
- Anti-Windup
- Zeitquote und Impulsakkumulator
- aktive Luftbegrenzung
- angeforderte Richtung `HEAT`, `OFF` oder `COOL`
- bestaetigte Gegenrichtung
- Mindest-Einschaltzeit und Mindest-Ausschaltzeit
- Totzeit
- Alter der Regelanforderung
- tatsaechliche Aktorfreigabe und Sperrgrund
- R_IS/L_IS nur nach realer Verifikation

## Passiver Boot-Selbsttest

Der Boot-Selbsttest prueft ohne Aktoraktivierung:

1. Resetursache und abnormalen Neustartzaehler
2. persistierte Verriegelungen und `SAFE_BOOT`
3. Firmware-, Schema- und Partitionsdaten
4. Konfigurationsintegritaet und Rueckfallrevision
5. Laufkontrollpunkt, Transaktionsmarker und Persistenzgesundheit
6. Fehler- und Resetjournal
7. 1-Wire-Busse und feste Sensoridentitaeten
8. aktuelle Sensorwerte und Plausibilitaet
9. Flash-, Heap- und Ressourcenreserven
10. logisch sichere Ausgangszustaende
11. Netzwerk und Zeit als nicht sicherheitsblockierende Funktionen

Moegliche Ergebnisse:

```text
PASS
PASS_WITH_WARNINGS
SAFE_BOOT
FAILED
```

`PASS` startet keinen alten Prozess direkt. Eine separate validierte
Recoveryentscheidung bleibt erforderlich.

## SAFE_BOOT-Diagnose

In `SAFE_BOOT` erlaubt:

- passive Diagnose
- Lesen und Exportieren von Fehler-, Reset- und Persistenzinformationen
- Netzwerkrecovery ohne Aktorwirkung
- PIN-unabhaengiger lokaler Vollreset
- UART-Recovery beziehungsweise erneutes Flashen

In `SAFE_BOOT` gesperrt:

- Summer-, Luefter-, BTS7960- und Peltier-Test
- Wechsel in `SERVICE_MODE`
- Loeschen einer Verriegelung ohne bestandene Ursachen- und Integritaetspruefung

Nach Beseitigung der Ursache muss das Geraet erst bewusst und validiert nach
`STANDBY` zurueckkehren. Erst dort kann der Servicebereich geoeffnet werden.

## Issue #24 R2 – strukturierter Safety-Nachweis

Lokale Diagnose projiziert den stabilen Faultcode, die `FaultInstanceId`,
Fault-/Recordrevision, Dominanz, Restart-Episode, `RestartEvidenceId`,
Resetgrund und die Resetentscheidung. Die Ereignisse werden ueber das
bestehende `IEventJournal` geschrieben; es gibt kein paralleles Safetyjournal.
Ein `FaultResetCommitted`-Ereignis beweist nur den verifizierten Commit und
den anschliessenden autorisierten Resetpfad, nicht eine automatische
Freigabe in derselben Operation. Hardwaretests und ESP-IDF-Resetursachen
bleiben separate, noch nicht ausgefuehrte Nachweise.

## Gefuehrter Service-Hardwaretest

### Voraussetzungen

- validiertes `STANDBY`
- kein aktiver Lauf
- keine aktive oder ungeklaerte Verriegelung
- Service-PIN erfolgreich eingegeben
- kritischer Speicher gesund
- Schrankluft- und Kuehlkoerpersensor gueltig
- Versorgung stabil
- alle verwendeten GPIO-Pegel und Richtungen fuer die konkrete Hardware bestaetigt

Fuer einen Peltier-Puls zusaetzlich zwingend:

- 7,5-A-Ueberstromsicherung montiert
- einmalige Temperatursicherung montiert und auf Durchgang geprueft
- Montageort der Temperatursicherung dokumentiert
- Kuehlkoerper korrekt montiert
- Aussenluefter zuvor erfolgreich getestet
- BTS7960 ohne Peltier und Polaritaet mit Multimeter verifiziert

Rating und Montageort bleiben bis zur Inbetriebnahme `TBD_COMMISSIONING`; die
Installation vor dem ersten Peltier-Puls ist verbindlich.

### Reihenfolge

1. Firmware-, Hardware- und Konfigurationsrevision erfassen
2. Sensoren, ROM-Adressen, Busse, Rohwerte und Offsets pruefen
3. Summer kurz testen
4. Innenluefter kurz testen
5. Aussenluefter kurz testen
6. BTS7960 ohne angeschlossenes Peltier pruefen
7. Ausgang und Polaritaet mit Multimeter bestaetigen
8. Sicherheitskomponenten und Freigaben erneut pruefen
9. begrenzten Peltier-Heizpuls ausfuehren
10. sicher ausschalten, Mindest-Ausschaltzeit und Totzeit abwarten
11. begrenzten Peltier-Kuehlpuls ausfuehren
12. thermische Reaktion und gegebenenfalls R_IS/L_IS auswerten
13. Servicebericht speichern

Jeder Schritt besitzt Vorbedingungen, maximale Leistung, maximale Dauer,
laufende Sicherheitsueberwachung, automatische Abschaltung und jederzeitigen
Abbruch.

Abbruch:

```text
Peltier AUS
-> beide Richtungen AUS
-> Impulsanforderung verwerfen
-> erforderlicher Aussenluefternachlauf
-> Test als abgebrochen protokollieren
```

## Begrenzte Peltierpruefung

Freie dauerhafte Handsteuerung ist unzulaessig. Erlaubt sind nur gefuehrte Pulse
mit:

- gueltigen Pflichtsensoren
- bestaetigter Richtung
- begrenzter Leistung und Dauer
- firmwarefesten Sicherheits- und Notgrenzen
- Aussenluefter und Nachlauf
- Aktor-Watchdog
- sofortiger Abschaltung bei Fehler

Servicewerte umgehen keine Pulldowns, Verriegelungen, Mindest-Ausschaltzeiten,
Totzeiten oder Sicherheitsgrenzen. Konkrete Pulswerte bleiben
`TBD_COMMISSIONING`.

## PIN-unabhaengiger lokaler Vollreset

Bei vergessener Service-PIN ist ein physischer Recoveryweg erforderlich, der die
PIN nicht voraussetzt.

- nur lokal waehrend Boot oder `SAFE_BOOT`
- alle leistungsbezogenen Aktoren AUS
- nicht ueber Web oder Netzwerk ausloesbar
- mehrstufige Warnung und lange bewusste Bestaetigung
- Ausloesung ueber eine bei der Hardwareabnahme verifizierte rohe Touchgeste oder
  einen anderen eindeutig physischen Weg
- UART-Loeschen/Neu-Flashen als letzter physischer Recoveryweg
- vollstaendiger Werksreset; kein isolierter PIN-Reset

Die konkrete Geste oder Taste bleibt `TBD_HARDWARE`.

## Exporte

### Laufexport

- Laufkennung und Programmschnappschuss
- Firmware-, Konfigurations- und Tuningrevision
- Phasen und Phasenwechsel
- Temperaturen und verdichtete Diagrammdaten
- Sensorstatus und Sensorwechsel
- Regler- und Aktorereignisse
- Warnungen, Fehler, Quittierungen und Resets
- Unterbrechungsintervall und Recoveryentscheidung
- Fortschrittskorrekturen und Laufanpassungen
- Abschluss- oder Abbruchgrund

Formate: JSON und geeignete CSV-Tabellen.

### Diagnoseexport

- Firmware-, Hardware-, Schema- und Konfigurationsrevision
- Resetursache, Neustartzaehler und `SAFE_BOOT`
- Prozess-, Fehler- und Persistenzzustand
- Sensor-, Regel-, Aktor- und Luefterdaten
- Speicher- und Ressourceninformationen
- Netzwerk- und Zeitstatus
- relevante Fehler-, Brownout-, Watchdog- und Resetereignisse

### Servicebericht

- Testkennung und Zeitbasis
- Bedienquelle
- Revisionen
- ausgefuehrte und uebersprungene Schritte
- Vorbedingungen und Sicherheitsfreigaben
- Sensor- und Aktordaten
- Pulsleistung, Dauer und Richtung
- thermische Reaktion
- R_IS/L_IS nur falls verifiziert
- Warnungen, Abbrueche, Fehler und Ergebnis

Moegliche Ergebnisse:

```text
PASS
PASS_WITH_WARNINGS
FAILED
ABORTED
```

### Gemeinsame Regeln

Kein Export enthaelt Geheimnisse, PINs, Sitzungen, Tokens oder private
Schluessel. Fehlende Werte werden als fehlend codiert, nicht als `0`. Exporte sind
versioniert und besitzen eine Schema-ID.

## UART in Release 1

Erlaubt fuer:

- Flashen ueber FT232RL
- Entwicklung und Inbetriebnahme
- Bootloader- und Recoveryarbeiten

Nicht enthalten:

- normaler benutzeraktivierbarer UART-Diagnosemodus
- Aktorfreigabe durch UART-Befehle in der Releasefirmware
- Geheimnisse oder blockierende serielle Ausgaben

## Akzeptierte Entscheidungen

- [x] kompakte lokale und vollstaendige Webdiagnose
- [x] lesende Diagnose waehrend eines Laufes
- [x] keine Aktortests waehrend eines Laufes
- [x] normaler Boot fuehrt nur passive Tests aus
- [x] `SAFE_BOOT` erlaubt keine Aktortests
- [x] gefuehrter Hardwaretest nur aus validiertem `STANDBY`
- [x] Temperatursicherung vor dem ersten Peltier-Puls
- [x] jederzeitiger sicherer Testabbruch
- [x] Peltier nur als begrenzter gefuehrter Puls
- [x] getrennte Lauf-, Diagnose- und Serviceexporte
- [x] keine Geheimnisse in Diagnose oder Export
- [x] PIN-unabhaengiger lokaler Vollreset
- [x] UART in Release 1 nur fuer Flashen, Entwicklung und Recovery
