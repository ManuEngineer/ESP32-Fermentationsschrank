# Implementierungsplan und Entwicklungsreihenfolge

## Status

Die Software wird vor Eintreffen der Hardware weitgehend als nativ testbarer
Kern entwickelt. Reale Hardware wird spaeter ueber dieselben klar getrennten
Adapter integriert.

Die konkrete Issue-Struktur steht in
[`IMPLEMENTATION_ISSUES.md`](IMPLEMENTATION_ISSUES.md). Die verbindlichen
Korrekturen aus den Reviews von PR #38 stehen in
[`PR38_REVIEW_CORRECTIONS.md`](PR38_REVIEW_CORRECTIONS.md).

Die Implementierung beginnt erst nach Merge von PR #38. Erstes Issue ist #9.

## Grundentscheidung

```text
Softwarekern zuerst und mit Simulation entwickeln
+ reale Hardware hinter klaren Schnittstellen kapseln
+ dieselbe Codebasis spaeter in einem geschuetzten Bring-up-Profil verwenden
+ Aktoren erst nach elektrischer und thermischer Verifikation freigeben
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
- Fehler-, Persistenz- und Wiederherstellungslogik
- Simulation mit virtueller Zeit
- Fehlerinjektionen fuer Boot, Speicher und Unterbrechungen

### `esp32_bringup`

- alle Aktoren nach Boot gesperrt
- sichtbarer Zustand `HARDWARE_UNVERIFIED`
- kein automatischer Fermentationsstart
- keine automatische Peltier-, Luefter- oder Summerpruefung
- keine Aktortests aus `SAFE_BOOT`
- einzelne Hardwaretests nur aus validiertem `STANDBY` ueber den geschuetzten
  Serviceablauf
- noch nicht bestaetigte Ausgaenge bleiben gesperrt
- Diagnose von Flash, Heap, Resetursache, Sensorbussen, Display und Eingaben

### `esp32_release`

Aktoren sind nur zulaessig, wenn Hardwareprofil, GPIOs, aktive Pegel,
Bootverhalten, Pflichtsensoren, Luefter und Hardwareabnahme bestaetigt sind. Ein
Wechsel des Buildprofils darf unbekannte Hardware nicht automatisch freigeben.

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

## Simulierte Hardware und Fehler

Vor Hardwareankunft werden mindestens simuliert:

- Schrankluft-, Produkt- und Kuehlkoerpersensor
- Sensorstatus `VALID`, `STALE`, `FAILED`
- langsame und schnelle Temperaturverlaeufe
- Heizen, Neutralbereich und Kuehlen
- Innen- und Aussenluefter
- BTS7960 als abstrakter Aktor
- Stromausfall und Neustart
- fehlende und spaeter eintreffende NTP-Zeit
- Ausfallzeit als Unter-/Obergrenze
- persistierte Verriegelung und Bootschleife
- unvollstaendige Persistenztransaktion
- kritischer Persistenzschreibfehler
- Wiederherstellung von `COMPLETED`
- Aktionen von Display und Web

Das thermische Simulationsmodell prueft Softwareablaeufe. Es liefert keine realen
PI-, Prozess- oder Sicherheitsparameter.

## Aktorkette vor Hardwareankunft

```text
Regleranforderung
-> Luftbegrenzung
-> Sicherheitsfreigabe
-> Mindest-Einschaltzeit und Mindest-Ausschaltzeit
-> Richtungswechselbestaetigung
-> Totzeit
-> Impulsakkumulator
-> abstrakter Aktorbefehl
```

Der letzte Schritt endet bei einem Mock oder Ereignisprotokoll.

## Verbindliche Boot- und Persistenzkette

```text
BOOT
-> alle Ausgaenge AUS
-> Resetursache und Bootschleife pruefen
-> persistierte Verriegelungen pruefen
-> kritischen Speicher und Transaktionsmarker pruefen
-> Konfiguration und Laufrevisionen validieren
-> COMPLETED wiederherstellen oder aktive Recovery bewerten
-> Recoveryentscheidung atomar speichern
-> erst danach eine neue Aktoraktion freigeben
```

Ein Neustart ist kein Fehlerreset. `SAFE_BOOT` bleibt aktorfrei.

## Softwarefolge vor der Hardware

### SW0: Grundlage

- Profile `native`, `esp32_bringup`, `esp32_release`
- CI und native Tests
- Hardwareabstraktionen und Mocks
- virtuelle monotone und optionale UTC-Zeit
- Fehler-, Ergebnis- und Revisionsmodelle

### SW1: Fachlicher Kern

- Programme und unveraenderlicher Laufschnappschuss
- kanonische Zustandsmaschine
- Start, Stop und Abschluss
- Meldungen, Quittierung und Fehlerreset
- `COMPLETED`-Wiederherstellung
- simulierte Standardablaeufe

### SW2: Persistenz und Recovery

- Factory-, Benutzer- und Laufdaten
- atomare Revisionen und Rueckfall
- Transaktionsabsicht vor aktorwirksamen Zustandsaenderungen
- Kontrollpunkte
- reservierter Persistenzfehler-Latch
- Bootpruefung auf unvollstaendige Transaktionen
- Ausfallzeit als Unsicherheitsintervall
- Recovery ohne erfundenen Fortschritt
- Aufbewahrung und Bereinigung

### SW3: Sensor, Regelung und Sicherheit

- Sensorqualitaet und Filter
- PI-Regler und Luftbegrenzung
- Aktorplaner, Mindestzeiten und Totzeit
- Fehlerklassen und persistente Verriegelung
- `SAFE_BOOT`
- Fehlerinjektionen

### SW4: Bedienung

- lokale View-Modelle und Navigation
- Programmeditor und Laufanzeige
- Weboberflaeche und lokale API
- Mehrsprachigkeit
- Bedienkonflikte und Berechtigungen
- Anzeige des Ausfallintervalls und ausstehender Benutzerentscheidung
- PIN-unabhaengiger lokaler Vollreset als Recoveryablauf

### SW5: Diagnose und Exporte

- Diagnosemodell
- Lauf-, Diagnose- und Servicebericht
- gefuehrter Serviceablauf mit Mockbackend
- passive `SAFE_BOOT`-Diagnose
- Testprotokoll fuer elektrische und thermische Freigaben
- vorlaeufige Ressourcenbudgets

Die Abschnitte werden in kleinen vertikalen Funktionsscheiben umgesetzt und
duerfen teilweise parallel laufen.

## Hardware-Bring-up

### H0: Sichtpruefung

- Platinenrevision und Modulbeschriftung
- Verdrahtungsplan und Fotos
- Versorgung, Masse, Stecker, Sicherungen und Leitungen
- Montagekonzept fuer Kuehlkoerper und einmalige Temperatursicherung

### H1: Controller ohne Aktoren

- Flashen und UART-Recovery
- Flash, Partition und Ressourcen
- GPIOs unbelastet messen
- Boot-, Reset- und Bootloaderpegel
- Onboard-MOSFET-Ausgaenge unbelastet
- `SAFE_BOOT` und PIN-unabhaengigen Vollreset ohne Aktorwirkung pruefen

### H2: Sensoren, Display und Touch

- drei DS18B20 und ROM-Adressen
- Bustopologie und Produkt-Hot-Plug
- Displaycontroller und Rotation
- Touchcontroller und Kalibrierung
- physische Recoverygeste beziehungsweise alternativen lokalen Resetweg
  verifizieren

### H3: Luefter und Summer

- Ausgang unbelastet messen
- Verbraucher einzeln anschliessen
- Strom, Anlauf, Pegel und Nachlauf
- Boot und Reset mit Verbraucher
- Aussenluefter als Voraussetzung fuer den spaeteren Peltier-Test abnehmen

### H4: BTS7960 ohne Peltier

- Logikversorgung und Masse
- Enable- und Richtungseingaenge
- Pulldowns
- Ausgang und Polaritaet mit Multimeter
- gleichzeitige Richtungen hardware- und softwareseitig ausschliessen
- R_IS/L_IS erst nach Pegelpruefung
- Reset und `SAFE_BOOT` mit sicher deaktivierter H-Bruecke pruefen

### H5: Begrenzte Peltierpulse

Vor **jeder ersten realen Bestromung des Peltiers** muessen vorhanden und
geprueft sein:

- geeignete 7,5-A-Ueberstromsicherung
- montierte einmalige Temperatursicherung als von der Firmware unabhaengige
  thermische Abschaltung
- dokumentierter Montageort und bestandene Durchgangspruefung der
  Temperatursicherung
- korrekt montierter Kuehlkoerper und Waermetauscher
- funktionsgepruefter Aussenluefter; Innenluefter gemaess Testaufbau
- gueltiger Schrankluft- und Kuehlkoerpersensor
- bestaetigte BTS7960-Pinbelegung, AUS-Pegel, Enable-Verhalten und Polaritaet
- stabile, abgesicherte Versorgung
- validiertes `STANDBY` und PIN-geschuetzter Serviceablauf

Rating und genauer Montageort der Temperatursicherung bleiben
`TBD_COMMISSIONING`; ihre Installation vor dem ersten Puls ist verbindlich.

Ablauf:

```text
alle Vorbedingungen erneut pruefen
-> begrenzter Heizpuls
-> Peltier und beide Richtungen AUS
-> Aussenluefternachlauf
-> Mindest-Ausschaltzeit und Totzeit
-> begrenzter Kuehlpuls
-> Peltier und beide Richtungen AUS
-> Nachlauf
-> thermische Reaktion und Fehlerpfade dokumentieren
```

Die erste reale Peltierfreigabe erfolgt ausschliesslich im Bring-up-/Servicemodus
aus validiertem `STANDBY`. `SAFE_BOOT` kann keinen solchen Puls ausloesen.

## Entwicklungsstil

Kleine vertikale Funktionsscheiben, beispielsweise:

```text
Produktfuehler-Mock
-> Messung und Qualitaet
-> VALID/STALE/FAILED
-> Ersatzbetrieb
-> Diagnosemodell
-> UI-Anzeige
-> automatische Tests
```

Der reale Adapter wird spaeter hinter derselben Schnittstelle ergaenzt.

## Branch- und PR-Strategie

- PR #38 zuerst nach `main` mergen
- danach ein Branch pro Implementierungs-Issue
- kleine pruefbare PRs
- keine umfangreiche direkte Implementierung auf `main`
- Hardwareblockaden als `BLOCKED_HARDWARE` sichtbar lassen

Erster Branch:

```text
foundation/platformio-profiles
```

zu Issue #9.

## Definition of Done

- Implementierung vollstaendig
- passende native, simulierte oder reale Tests bestanden
- ESP32-Build erfolgreich, soweit relevant
- Ressourcenwirkung geprueft oder sichtbar offen
- Fehlerfaelle und Reviewkorrekturen behandelt
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
| M1 | Getesteter Softwarekern inklusive Boot-, Persistenz- und Recoveryregeln |
| M2 | Bedienbarer Simulator |
| M3 | Hardware-Bring-up ohne ungesicherte Peltierfreigabe |
| M4 | Sichere reale Temperatursteuerung |
| M5 | Vollstaendige Integration |
| M6 | Release 1 nach thermischer Abnahme und siebentaegigem Test |

Die konkrete Issuezuordnung steht in `IMPLEMENTATION_ISSUES.md`.
