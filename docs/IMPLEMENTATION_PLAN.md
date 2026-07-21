# Implementierungsplan und Entwicklungsreihenfolge

## Status

Phase 10B und 10C sind abgeschlossen. Die Software wird vor Eintreffen der
Hardware weitgehend als nativ testbarer Kern entwickelt. Reale Hardware wird
spaeter ueber dieselben klar getrennten Adapter integriert.

Die konkrete Struktur steht in [`IMPLEMENTATION_ISSUES.md`](IMPLEMENTATION_ISSUES.md).
Die Implementierung beginnt erst nach Merge von PR #38. Erstes Issue ist #9.

## Grundentscheidung

```text
Softwarekern zuerst und mit Simulation entwickeln
+ reale Hardware hinter klaren Schnittstellen kapseln
+ dieselbe Codebasis spaeter in einem geschuetzten Bring-up-Profil verwenden
+ Aktoren erst nach elektrischer Verifikation schrittweise freigeben
```

Es gibt kein separates Wegwerf-Testprojekt.

## Entwicklungsprofile

### `native`

- fachliches Datenmodell
- Zustandsmaschine
- Zeit- und Phasenlogik
- Programmvalidierung
- Sensorstatus und Filterlogik
- PI-Reglerkern
- Aktorplaner ohne GPIOs
- Fehler- und Wiederherstellungslogik
- Persistenztests mit Testbackend
- Simulation mit virtueller Zeit

### `esp32_bringup`

- alle Aktoren nach Boot gesperrt
- kein automatischer Fermentationsstart
- keine automatische Peltier-, Luefter- oder Summerpruefung
- einzelne Hardwaretests nur ueber den geschuetzten Serviceablauf
- noch nicht bestaetigte Ausgaenge gesperrt
- Diagnose von Flash, Heap, Resetursache, Sensorbussen, Display und Eingaben
- sichtbarer Zustand `HARDWARE_UNVERIFIED`

### `esp32_release`

Aktoren sind nur zulaessig, wenn Hardwareprofil, GPIOs, aktive Pegel,
Bootverhalten, Sensoren, Luefter und Hardwareabnahme bestaetigt sind. Ein Wechsel
des Buildprofils darf unbekannte Hardware nicht automatisch freigeben.

## Architektur fuer Entwicklung ohne Hardware

Der fachliche Kern verwendet keine direkten Aufrufe von GPIO, 1-Wire, Display,
WLAN, Dateisystem oder realer Systemzeit.

Konzeptionelle Ports:

```text
ITimeSource
ITemperatureSource
IActuatorSink
IStateStore
IEventJournal
INetworkStatus
IUserNotificationSink
IResourceMonitor
```

ESP32-Adapter und native Mockadapter implementieren dieselben Schnittstellen.

## Simulierte Hardware

Vor Hardwareankunft werden mindestens simuliert:

- Schrankluft-, Produkt- und Kuehlkoerpersensor
- `VALID`, `STALE`, `FAILED`
- langsame und schnelle Temperaturverlaeufe
- Heizen, Neutralbereich und Kuehlen
- Innen- und Aussenluefter
- BTS7960 als abstrakter Aktor
- Stromausfall und Neustart
- fehlende und spaeter eintreffende NTP-Zeit
- Persistenzfehler
- Aktionen von Display und Web

Das thermische Simulationsmodell prueft Softwareablaeufe. Es liefert keine realen
PI- oder Sicherheitsparameter.

## Aktorkette vor Hardwareankunft

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

Der letzte Schritt endet bei einem Mock oder Ereignisprotokoll.

## Vor Hardwareankunft entwickelbar

- Repositorystruktur, PlatformIO-Profile und CI
- native Tests und virtuelle Zeit
- Datenmodelle, Fehlercodes und Versionen
- Programmschema und Standardprogramme mit `TBD_COMMISSIONING`
- Zustandsmaschine und Prozessablaeufe
- Start, Stop, Quittierung und Laufanpassungen
- Sensorqualitaet, Filter und Plausibilitaet
- PI-Regler und Aktorplaner
- Totzeit und Richtungswechsel
- Sicherheits- und Verriegelungslogik
- Konfiguration, Persistenz und Wiederanlauf
- Journale, Aufbewahrung und Bereinigung
- Sprachressourcen
- lokale UI-Modelle und Navigation
- Web-API-Vertraege und Weboberflaeche gegen Simulation
- Diagnose, Exporte und Serviceablauf mit gesperrtem Hardwarebackend
- Fehlerinjektionen und vollstaendige simulierte Laeufe

## Erst real verifizierbar

- Boardrevision, Flash und Partition
- GPIOs, Pegel und Bootverhalten
- BTS7960-Pins, Pulldowns und R_IS/L_IS
- 1-Wire-Busse und Hot-Plug
- Display- und Touchcontroller
- reale Heap-, Flash- und Laufzeitwerte
- Luefterstrom und Anlauf
- Peltierstrom, Polaritaet und thermische Leistung
- PI-Parameter, Filterzeiten und Grenzwerte
- Temperatursicherung und Montageort
- reale Standardprogrammwerte

Diese Punkte bleiben `TBD_HARDWARE`, `TBD_COMMISSIONING` oder
`TBD_IMPLEMENTATION_BUDGET`.

## Softwarefolge vor der Hardware

### SW0: Grundlage

- Profile `native`, `esp32_bringup`, `esp32_release`
- CI und Tests
- Hardwareabstraktionen und Mocks
- virtuelle Zeit
- Fehler- und Ergebnistypen

### SW1: Fachlicher Kern

- Programme und Laufschnappschuss
- Zustandsmaschine
- Start, Stop und Abschluss
- Meldungen, Quittierung und Reset
- simulierte Standardablaeufe

### SW2: Persistenz

- Factory-, Benutzer- und Laufdaten
- atomare Revisionen und Rueckfall
- Kontrollpunkte und Wiederanlauf
- Aufbewahrung und Bereinigung

### SW3: Sensor, Regelung und Sicherheit

- Sensorqualitaet und Filter
- PI-Regler und Luftbegrenzung
- Aktorplaner, Mindestzeiten und Totzeit
- Fehlerklassen, Verriegelung und `SAFE_BOOT`
- Fehlerinjektionen

### SW4: Bedienung

- lokale View-Modelle und Navigation
- Programmeditor und Laufanzeige
- Weboberflaeche und lokale API
- Mehrsprachigkeit
- Bedienkonflikte und Berechtigungen

### SW5: Diagnose und Exporte

- Diagnosemodell
- Lauf-, Diagnose- und Servicebericht
- gefuehrter Serviceablauf mit Mockbackend
- vorlaeufige Ressourcenbudgets

Die Abschnitte werden in kleinen vertikalen Funktionsscheiben umgesetzt und
duerfen teilweise parallel laufen.

## Hardware-Bring-up

### H0: Sichtpruefung

- Platinenrevision und Modulbeschriftung
- Verdrahtungsplan und Fotos
- Versorgung, Masse, Stecker, Sicherung und Leitungen

### H1: Controller ohne Aktoren

- Flashen und Recovery
- Flash, Partition und Ressourcen
- GPIOs unbelastet messen
- Boot-, Reset- und Bootloaderpegel
- Onboard-MOSFET-Ausgaenge unbelastet

### H2: Sensoren, Display und Touch

- drei DS18B20 und ROM-Adressen
- Bustopologie und Produkt-Hot-Plug
- Displaycontroller und Rotation
- Touchcontroller und Kalibrierung

### H3: Luefter und Summer

- Ausgang unbelastet messen
- Verbraucher einzeln anschliessen
- Strom, Anlauf, Pegel und Nachlauf
- Boot und Reset mit Verbraucher

### H4: BTS7960 ohne Peltier

- Logikversorgung und Masse
- Enable- und Richtungseingaenge
- Pulldowns
- Ausgang und Polaritaet mit Multimeter
- R_IS/L_IS erst nach Pegelpruefung

### H5: Begrenzte Peltierpulse

Voraussetzungen:

- 7,5-A-Sicherung
- beide Luefter geprueft
- Schrankluft- und Kuehlkoerpersensor gueltig
- Waermetauscher und thermische Kopplung montiert
- Richtung bestaetigt

Ablauf:

```text
begrenzter Heizpuls
-> AUS
-> Aussenluefternachlauf
-> Mindest-Auszeit und Totzeit
-> begrenzter Kuehlpuls
-> AUS
-> Nachlauf
```

Die erste reale Peltierfreigabe erfolgt nur im Bring-up-/Servicemodus.

## Entwicklungsstil

Kleine vertikale Funktionsscheiben:

```text
Produktfuehler-Mock
-> Messung und Qualitaet
-> VALID/STALE/FAILED
-> Ersatzbetrieb
-> Diagnosemodell
-> UI-Anzeige
-> automatische Tests
```

Spaeter wird der reale DS18B20-Adapter hinter derselben Schnittstelle ergaenzt.

## Branch- und PR-Strategie

- PR #38 zuerst nach `main` mergen
- danach Branch pro Implementierungs-Issue
- kleine pruefbare PRs
- keine umfangreiche direkte Implementierung auf `main`
- Hardwareblockaden als `BLOCKED_HARDWARE`

Erster Branch:

```text
foundation/platformio-profiles
```

zu Issue #9.

## Definition of Done

- Implementierung vollstaendig
- passende Tests bestanden
- ESP32-Build erfolgreich, soweit relevant
- Ressourcenwirkung geprueft oder sichtbar offen
- Fehlerfaelle behandelt
- Dokumentation aktualisiert
- keine Geheimnisse
- keine unbestaetigten Hardwareannahmen als Fakten
- Akzeptanzkriterien erfuellt

Ein hardwareunabhaengiges Issue darf vor Hardwareankunft abgeschlossen werden,
wenn die reale Verifikation in einem separaten Issue sichtbar bleibt.

## Meilensteine

| Meilenstein | Ergebnis |
|---|---|
| M0 | Softwaregrundlage und simuliertes System |
| M1 | Getesteter Softwarekern |
| M2 | Bedienbarer Simulator |
| M3 | Hardware-Bring-up |
| M4 | Sichere reale Temperatursteuerung |
| M5 | Vollstaendige Integration |
| M6 | Release 1 nach thermischer Abnahme und siebentaegigem Test |

Die konkrete Issuezuordnung steht in `IMPLEMENTATION_ISSUES.md`.
