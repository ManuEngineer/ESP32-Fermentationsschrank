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
  ESP-IDF-Zielkonfiguration nach Hardwareabnahme
```

Das Umschalten des Buildprofils darf unbestaetigte Hardware nicht automatisch
freigeben.

## Issue #24 Release-1-Safetygrenze

Der produktive abstrakte Pfad ist `SafetyCore -> ActuatorPlanner -> Sink`.
`ActuatorSafetyGateStatus::Unresolved` ist der Boot-/Fehler-Default; ein
Aufrufer darf `Allowed` nicht als eigene Safety-Wahrheit einschleusen. Jeder
aktorfähige Orchestrator wird konstruktiv mit `SafetyCore&` erzeugt und
`tickActuatorPlan()` besitzt kein caller-supplied finales Gate.

Issue #24 enthaelt keine GPIO-, BTS7960-, MOSFET-, Luefter- oder
Thermal-Hardwareimplementierung und umgeht nicht #106. ResetCause wird ueber
den anwendungsneutralen Device-Platform-Port gelesen; der Adapter ist kein
Aktoradapter. SafetyCore besitzt keine allgemeine persistente Fehlerhistorie,
keinen Restartakkumulator und keine Charge-Recovery-Plattform.

### Umgesetzte Projektgrundlage

`platformio.ini` definiert ausschliesslich `native` fuer den
hardwareunabhaengigen Hosttestpfad. `esp32_bringup` und `esp32_release` werden
aus derselben Codebasis ausschliesslich mit ESP-IDF `v6.0.2`
(`7101770dc6db2667b3c477cc31365dd1acd6db4e`) in getrennten Buildverzeichnissen
gebaut. Beide ESP32-Profile planen mit 4 MB Flash ohne vorausgesetzte PSRAM.
PlatformIO Core `6.1.19` bleibt fuer den nativen Hostpfad fixiert.

Die Profile unterscheiden ihre Freigabepolitik explizit:

- `esp32_bringup`: `HARDWARE_UNVERIFIED`, Bring-up-Diagnosepolitik, reale Aktoren
  gesperrt
- `esp32_release`: Releasepolitik verlangt ein bestaetigtes Hardwareprofil, reale
  Aktoren bleiben in der unbestaetigten Standardkonfiguration gesperrt

Web-OTA ist in allen Release-1-Builds deaktiviert. Eine projektspezifische
Partitionstabelle wird erst nach den Build- und Hardwaremessungen aus #29
festgelegt und bleibt `TBD_IMPLEMENTATION_BUDGET`. Die Grundlage enthaelt weder
reale Hardwaretreiber noch GPIO- oder Pegelzuweisungen.

Die gemeinsame Struktur trennt `include/` fuer hardwareunabhaengige Typen und
Buildregeln, `lib/` fuer testbare Komponenten und `test/` fuer native Tests.
`main/app_main.cpp` ist die produktive ESP-IDF Composition Root und
Laufzeitorchestrierung; `src/main.cpp` ist ausschliesslich die native Host- und
Test-Composition-Root. Profilunabhaengige Sicherheitsinvarianten liegen in
`include/app_config.hpp` und werden sowohl zur Compilezeit in jedem Build als
auch durch native Tests geprueft.

`scripts/check_build_profiles.py` prueft die getrennten generierten
`sdkconfig`-Dateien und Compile-Definitionen beider ESP-IDF-Profile. Die
Kontrolle von 4 MB Flash, deaktiviertem Web-OTA und gesperrten realen Aktoren
bleibt damit von den Anwendungs-Makros getrennt.

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
ESP-IDF-Adapter oder native Mockadapter
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

- endliche stabile R1-FaultCode-Menge
- deterministische Primaerursache und bounded Multi-Fault-Maske
- Aktorsperren sowie getrennte Quittierung und Clear-Regeln
- aktueller #23-Watchdog-RAM-Latch
- diagnostische Resetcause ohne Restartakkumulator
- `SAFE_BOOT`

Thermal-/Hardware-Grenzen und spaetere automatische Recovery bleiben
E5/#35/Future und sind keine SafetyCore-Produzenten in #24-R1.

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
IBidirectionalActuatorSink
IBinaryOutputSink
IStateStore
IEventJournal
INetworkStatus
IUserNotificationSink
IResourceMonitor
```

`IBidirectionalActuatorSink` und `IBinaryOutputSink` ersetzen einen
urspruenglich gemeinsamen, geraetespezifisch benannten Aktorport (siehe
"Umgesetzte Schnittstellen" unten).

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

### Umgesetzte Schnittstellen und native Mockadapter

`ITimeSource` (virtuelle monotone und optionale UTC-Zeit) sowie
`ITemperatureSource`, `IBidirectionalActuatorSink`, `IBinaryOutputSink`,
`IStateStore`, `IEventJournal`, `INetworkStatus` und `IUserNotificationSink`
sind in `lib/device_platform/` umgesetzt. Dieses Verzeichnis enthaelt
ausschliesslich die anwendungsneutralen Produktionsschnittstellen und
allgemeinen Geraetedienste (siehe ADR-013).

`IBidirectionalActuatorSink` (`setForward`/`setReverse`) bildet einen
bidirektionalen Aktor mit zwei unabhaengig ansteuerbaren Richtungen ab, ohne
geraetespezifische Rollen wie Heizen/Kuehlen festzuschreiben.
`IBinaryOutputSink` (`setEnabled`) bildet genau einen binaeren Ausgang ab; die
Zuordnung zu einer konkreten Rolle wie Innenluefter, Aussenluefter oder Summer
ist Aufgabe der Anwendung beziehungsweise der Composition Root, nicht der
Plattform.

Deterministisch steuerbare Mockadapter und das einfache, ausdruecklich
unkalibrierte thermische Simulationsmodell (`ThermalSimulationModel`; prueft
nur Softwareablaeufe) liegen in der getrennten internen Bibliothek
`lib/device_platform_test_support/`. Diese Bibliothek darf von
`device_platform` abhaengen, nicht umgekehrt; `fermentation_app` und
`src/main.cpp` haengen nicht von ihr ab, und die ESP32-Produktionsbuilds binden
sie nicht ein. Der bidirektionale Mock macht eine gleichzeitige Aktivierung
beider Richtungen dauerhaft sichtbar, statt sie zu verhindern oder zu
verbergen.

Reale ESP32-Adapter dieser Schnittstellen sowie `IResourceMonitor` sind noch
nicht Teil dieser Grundlage. Eine Auslagerung von `device_platform` oder
`device_platform_test_support` in ein separates Repository erfolgt weiterhin
nicht (siehe ADR-013).

Persistenzports melden Fehler explizit: `IStateStore::read()` unterscheidet
erfolgreiches Lesen, einen fehlenden Schluessel und einen Speicherfehler.
`IEventJournal::record()` meldet fehlgeschlagene Schreibvorgaenge. Dadurch
koennen spaetere Sicherheits- und Recoverylogik kritische Speicherfehler von
normalen, noch nicht vorhandenen Daten unterscheiden.

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

## Diagnose und spaeterer Service (nicht #24-R1-Safety-Core)

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
- UTC/NTP dient Kalenderzeit; historische Unterbrechungs-/Progress-Recovery ist
  #18/C2 und kein aktiver #24-R1-Pfad
- Laufrevisionen speichern eine monotone Epoche. Jede Wiederherstellung
  eroeffnet eine neue Epoche, sodass die Uptime bei null beginnen darf, ohne die
  Reihenfolge der persistierten Revisionen zu verletzen.
- Ein zwischenzeitlich fehlender UTC-Wert verwirft den letzten bekannten
  UTC-Zeitbezug nicht; spaetere UTC-Werte duerfen dahinter nicht zurueckfallen.
- Wiederanlauf beginnt immer mit ausgeschalteten Aktoren
- fehlende NTP-Zeit erzeugt in #24-R1 keine Charge-Rettung oder Aktorfreigabe
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
