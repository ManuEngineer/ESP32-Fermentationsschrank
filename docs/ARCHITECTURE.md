# Softwarearchitektur

## Ziel

Die Firmware trennt fachliche Prozesslogik, Sicherheitsentscheidungen und reale
Hardwarezugriffe. Der Kern ist weitgehend nativ testbar und kann vor Ankunft der
Hardware mit virtueller Zeit, simulierten Sensoren und Mockaktoren entwickelt
werden.

## Entwicklungsprofile

```text
native
  Fachlicher Kern, Simulation, Unit- und Integrationstests

esp32_bringup
  Reale Hardwareadapter, alle Aktoren anfangs gesperrt,
  Zustand HARDWARE_UNVERIFIED

esp32_release
  Freigegebene Zielkonfiguration nach Hardwareabnahme
```

Das Umschalten des Buildprofils darf unbestaetigte Hardware nicht automatisch
freigeben.

## Schichten

```text
Lokale UI / Web / API
        |
Anwendungsdienste und Kommandos
        |
Prozess- und Laufkern
        |
Sensor-, Regel-, Sicherheits- und Persistenzlogik
        |
Abstrakte Ports
        |
ESP32-Adapter oder native Mockadapter
        |
Reale Hardware beziehungsweise Simulator
```

## Fachlicher Kern

Der fachliche Kern enthaelt keine direkten GPIO-, 1-Wire-, Display-, WLAN- oder
Dateisystemaufrufe.

### Programme und Laufmodell

- versionierte Programmmodelle
- unveraenderlicher Factory-Katalog
- Benutzerprogramme
- unveraenderlicher Programmschnappschuss je Lauf
- effektive Laufwerte und protokollierte Laufrevisionen
- Start-, Stop-, Abschluss- und Anpassungskommandos

### Zustandsmaschine

Wesentliche Zustaende und Kontexte verwenden exakt dieselben Namen wie
[`STATE_MACHINE.md`](STATE_MACHINE.md):

```text
BOOT
SAFE_BOOT
STANDBY
PREHEATING
WAITING_FOR_PRODUCT
REACHING_TARGET
QUALIFYING_TARGET
FERMENTING
COOLING
COOL_HOLDING
COMPLETED
RECOVERY_EVALUATION
RECOVERY_TIME_PENDING
WARNING_REQUIRES_ACTION
FAULT
SERVICE_MODE
MANUAL_HOLDING
```

`RECOVERY_TIME_PENDING` und `WARNING_REQUIRES_ACTION` koennen als zusaetzlicher
Kontext zu einem Prozesszustand auftreten und sind nicht zwingend eigenstaendige
blockierende Betriebszustaende.

Jeder Uebergang ist explizit und wird durch fachliche Voraussetzungen validiert.
Aktorpegel sind kein Bestandteil der Zustandsmaschine.

### Sensorverarbeitung

Die Sensorpipeline verarbeitet fuer jede Rolle:

```text
Rohprobe
-> Bus/CRC-Pruefung
-> Wertebereich und Aenderungsrate
-> Medianfilter
-> sensorbezogener Tiefpass
-> Qualitaet VALID / STALE / FAILED
-> korrigierter Regelwert
```

Rollen:

- Schrankluft: Regelung, Begrenzung und Sicherheit
- Produkt: optionale primaere Prozessregelung
- Kuehlkoerper/Aussenwaermetauscher: thermische Sicherheit und Diagnose

### Regelung

Der Regelkern erzeugt ausschliesslich eine abstrakte Anforderung:

```text
HEAT + Zeitquote
OFF
COOL + Zeitquote
```

Bestandteile:

- zeitproportionaler PI-Regler
- getrennte Parameter Luft/Produkt und Heizen/Kuehlen
- Neutralbereich
- Anti-Windup
- Luftbegrenzung bei Produktregelung
- Zielqualifikation und Gnadenzeit

Der Regler kennt keine GPIOs und keine BTS7960-Pins.

### Aktorplaner

Der Aktorplaner verarbeitet die Regleranforderung und prueft:

```text
Sicherheitsfreigabe
-> Luft- und Kuehlkoerpersensor gueltig
-> Richtung exklusiv
-> Gegenanforderung bestaetigt
-> Mindest-Einschaltzeit und Mindest-Ausschaltzeit
-> Polaritaetstotzeit
-> Impulsakkumulator
-> Luefteranforderung und Nachlauf
-> abstrakter Aktorbefehl
```

Sicherheitsabschaltungen ueberstimmen Mindest-Einschaltzeiten.

### Sicherheitskern

Der Sicherheitskern ist vom Komfort- und UI-Code unabhaengig.

Er enthaelt:

- vier Fehlerklassen
- stabile Fehlercodes
- Primaer- und Folgefehler
- Aktorsperren
- getrennte Quittierung und Fehlerreset
- persistente Verriegelungen
- Watchdog- und Neustartbewertung
- `SAFE_BOOT`
- Sicherheits-Eingriffsgrenze und harte Notgrenze

Eine automatische Wiederfreigabe ist nur fuer explizit definierte, sicher
pruefbare Betriebsfehler erlaubt.

### Persistenz

Persistiert werden fachliche Daten, nie direkte Ausgangspegel.

Bereiche:

- Factory-Konfiguration
- Benutzerkonfiguration
- Serviceparameter
- Geheimnisse in getrenntem Speicherbereich
- aktiver Laufschnappschuss und Laufrevisionen
- Kontrollpunkte
- Fehler- und Resetjournal
- begrenzte Messhistorie
- Laufzusammenfassungen

Schreibvorgaenge sind atomar, versioniert und besitzen mindestens eine gueltige
Rueckfallrevision.

## Ports und Adapter

Mindestens vorgesehene Schnittstellen:

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

### Native Adapter

- virtuelle monotone Zeit
- optionale simulierte UTC-Zeit
- steuerbare Temperatursensoren
- thermisches Testmodell
- Aktorjournal statt GPIO
- speicherbasiertes Persistenzbackend
- injizierbare Fehler und Stromunterbrechungen

### ESP32-Adapter

- monotone ESP32-Zeit und NTP
- DS18B20-Busse
- GPIO- und MOSFET-Ausgaenge
- BTS7960
- Display und Touch
- WLAN, DNS und lokaler Webserver
- nichtfluechtiger Speicher
- Resetursache, Brownout und Ressourcenmessung

Jeder ESP32-Adapter validiert die konkrete Hardwarekonfiguration. Unbestaetigte
Pins bleiben gesperrt.

## Bedienung

Touch und Web verwenden gemeinsame View-Modelle und dieselben fachlichen
Kommandos.

Sie duerfen:

- Zustand und Diagnose lesen
- Programme verwalten
- Laeufe starten und stoppen
- Meldungen quittieren
- erlaubte Laufanpassungen ausloesen
- Einstellungen gemaess Berechtigung aendern

Sie duerfen nicht:

- direkt GPIOs setzen
- Sicherheitsfreigaben ueberschreiben
- aktive Laufwerte still veraendern
- Fehlerursachen durch Quittierung entfernen

Gleichzeitige Aktionen werden atomar und ueber Revisionen beziehungsweise
Konflikterkennung behandelt.

## Netzwerk

Die Regelung ist netzwerkunabhaengig.

Netzwerkmodule umfassen:

- WLAN-Einrichtung und Ersatz-WLAN
- mDNS und lokale Namensaufloesung
- lokalen Webserver
- Anmeldung und Sitzungen
- lokale Lese-API
- NTP als primaere absolute Zeitquelle

Kein Cloudzwang, kein automatischer Firmwaredownload und keine offizielle externe
Schreib-API in Release 1.

## Diagnose und Service

Diagnose liest strukturierte Zustandsmodelle und keine beliebigen globalen
Variablen.

Der Serviceablauf ist eine eigene geschuetzte Zustandsmaschine:

- Aktortests ausschliesslich aus validiertem `STANDBY`
- `SAFE_BOOT` erlaubt nur passive Diagnose, Export und Wiederherstellung ohne
  Aktorfreigabe
- Service-PIN
- zeitlich und leistungsmassig begrenzte Aktortests
- jederzeitiger sicherer Abbruch
- automatische Sensor- und Sicherheitsueberwachung
- versionierter Servicebericht

## Zeit und Wiederanlauf

- monotone Zeit steuert aktive Ablaufe
- UTC/NTP dient Kalenderzeit und Unterbrechungsdauer
- Wiederanlauf beginnt immer mit ausgeschalteten Aktoren
- fehlende NTP-Zeit blockiert keine sichere phasenbezogene Entscheidung
- spaeter eintreffende Zeit kann die Unterbrechungsbewertung korrigieren
- eine spaetere batteriegepufferte RTC passt hinter dieselbe Zeitquellenschnittstelle

## Ressourcenmodell

- 4 MB Flash als harte Planungsbasis
- keine PSRAM-Abhaengigkeit
- feste oder begrenzte Puffer im Regel- und Sicherheitspfad
- proaktive Bereinigung nichtkritischer Historien
- Prioritaet: Firmware und sichere Bootfaehigkeit, Konfiguration, aktiver Lauf,
  Sicherheitsjournal, notwendige UI/Webressourcen, Zusammenfassungen, alte
  Diagrammdaten
- Web-OTA und duale OTA-Slots sind nicht Teil des Release-1-Layouts

## Erweiterungspunkte nach Release 1

Architektonisch moeglich, aber nicht aktiv implementiert:

- Produkt-Luft-Kaskadenregelung
- PID-Autotuning
- Web-OTA mit signierten Paketen und Rollback
- benutzeraktivierbare UART-Diagnose
- RTC
- 12-V-ADC-Messung
- Tuerkontakt
- Push-/Telegram-Benachrichtigungen
- automatische Wartungserinnerungen

## Implementierungsregel

Ein hardwareunabhaengiges Modul gilt als abgeschlossen, sobald seine fachlichen
Tests bestanden sind. Die reale Adapter- und Hardwareverifikation bleibt in einem
separaten `BLOCKED_HARDWARE`-Issue sichtbar. Details stehen in
[`IMPLEMENTATION_PLAN.md`](IMPLEMENTATION_PLAN.md) und
[`IMPLEMENTATION_ISSUES.md`](IMPLEMENTATION_ISSUES.md).
