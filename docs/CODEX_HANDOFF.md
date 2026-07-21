# Uebergabe an Codex

## Status

Die Spezifikation fuer Release 1 wurde auf dem Branch
`docs/software-specification` erstellt. Pull Request #38 dient dem abschliessenden
Review. Die Implementierung beginnt erst nach dessen Merge nach `main`.

Danach ist Issue #9 der erste Arbeitsauftrag. Es wird nicht versucht, mehrere
Epics in einem einzigen Durchlauf umzusetzen.

## Arbeitsweise

Fuer jedes Implementierungs-Issue:

1. Issue und dort verlinkte Spezifikationsquellen lesen.
2. Abhaengigkeiten und Statuskennzeichnung pruefen.
3. eigenen Branch vom aktuellen `main` erstellen.
4. nur den definierten Scope umsetzen.
5. native, simulierte oder Hardwaretests ergaenzen.
6. ESP32-Zielbuild ausfuehren, soweit relevant.
7. Ressourcenwirkung und offene Hardwarewerte dokumentieren.
8. Dokumentation aktualisieren.
9. kleinen Pull Request mit Issue-Verweis erstellen.

Direkte umfangreiche Implementierung auf `main` ist nicht vorgesehen.

## Erste Implementierungsfolge

### Issue #9

`PlatformIO-Profile und Projektgrundlage einrichten`

Ergebnis:

- Umgebungen `native`, `esp32_bringup`, `esp32_release`
- getrennte Quell- und Teststruktur
- 4-MB-Ziel ohne PSRAM-Abhaengigkeit
- keine reale Aktorfreigabe
- Web-OTA im Release-1-Profil nicht enthalten

Vorgeschlagener Branch:

```text
foundation/platformio-profiles
```

### Danach

- #10: native Tests, CI, virtuelle Zeit und Buildberichte
- #11: Hardwareabstraktionen, Mockadapter und Simulator
- #12 bis #28: fachlicher Kern, Persistenz, Regelung, Sicherheit, UI und Web
- #29 bis #33: reale Hardwareintegration, bis zur Hardware als
  `BLOCKED_HARDWARE`
- #34 bis #37: Inbetriebnahme und Release-Abnahme als `TBD_COMMISSIONING`

Die vollstaendige Abhaengigkeitsstruktur steht in
[`IMPLEMENTATION_ISSUES.md`](IMPLEMENTATION_ISSUES.md).

## Verbindliche Architektur

Der fachliche Kern wird soweit sinnvoll nativ testbar und ohne direkte
Arduino-Hardwarezugriffe umgesetzt.

Vorgesehene Abstraktionen umfassen mindestens:

```text
ITimeSource
ITemperatureSource
IActuatorSink
IStateStore
IEventJournal
INetworkStatus
IUserNotificationSink
```

Direkte GPIO-, 1-Wire-, Display-, WLAN- und Speicherzugriffe gehoeren in
ESP32-spezifische Adapter.

Vor Ankunft der Hardware koennen Zustandsmaschine, Programme, Persistenz,
Sensorqualitaet, PI-Regler, Aktorplaner, Sicherheitslogik, UI-Modelle,
Weboberflaeche, Diagnose und Exporte gegen Mocks und simulierte Zeit entwickelt
werden.

## Hardwarefreigabe

Keine Pinbelegung oder Polaritaet aus Beispielkonfigurationen uebernehmen.

Verbindliche Reihenfolge:

1. Board und Ausgaenge ohne angeschlossene Aktoren messen.
2. Sensoren, Display und Touch einzeln integrieren.
3. Luefter und Summer einzeln anschliessen und messen.
4. BTS7960 ohne Peltier pruefen.
5. H-Brueckenausgang und Polaritaet mit Multimeter bestaetigen.
6. Peltier erst mit 7,5-A-Sicherung, geprueften Lueftern, Kuehlkoerper,
   Temperatursensoren und thermischer Kopplung anschliessen.
7. erste reale Freigabe ausschliesslich als begrenzter Servicepuls.

Das Profil `esp32_bringup` muss unbekannte Aktoren standardmaessig sperren und
`HARDWARE_UNVERIFIED` sichtbar machen.

## Release-1-Hardware

- ESP32-32E, 4 MB Flash, keine PSRAM-Voraussetzung
- Peltier ueber BTS7960
- Innen- und Aussenluefter
- drei DS18B20:
  - Schrankluft
  - abnehmbares Produkt
  - Aussenwaermetauscher/Kuehlkoerper
- ILI9341-Display; Touchcontroller erst nach Messung bestaetigen
- aktiver Summer
- 7,5-A-Ueberstromsicherung
- einmalige Temperatursicherung
- FT232RL/UART als Update- und Recoveryweg

## Zentrale Sicherheitsregeln

- Peltier und beide H-Brueckenrichtungen bei Boot und unklarer Lage AUS.
- Schrankluft- und Kuehlkoerpersensor fuer jede Peltierfreigabe erforderlich.
- Heizen und Kuehlen niemals gleichzeitig.
- Richtungswechsel nur nach Abschaltung, Mindest-Auszeit und Totzeit.
- Sicherheitsabschaltung ueberstimmt Mindest-Einschaltzeit.
- Direkte Ausgangszustaende nie persistieren oder wiederherstellen.
- `Quittieren` ist kein Fehlerreset.
- Neustart loescht keine Verriegelung.
- Wiederholte abnormale Neustarts fuehren zu `SAFE_BOOT`.
- Netzwerk, Web, Display und Export duerfen den lokalen Regler nicht blockieren.

## Wichtige Umfangsgrenzen

Nicht fuer Release 1 implementieren:

- Web-OTA und automatische Firmwaredownloads
- benutzeraktivierbare UART-Diagnose
- Cloud oder Pushbenachrichtigungen
- Tuerkontakt
- verpflichtende RTC oder 12-V-ADC-Messung
- Kaskadenregelung oder PID-Autotuning
- automatische Wartungserinnerungen

## Umgang mit offenen Punkten

- `TBD_HARDWARE`: nicht erfinden, in Hardware-Issue belassen
- `TBD_COMMISSIONING`: nicht durch vermeintlich plausible Zahlen ersetzen
- `TBD_IMPLEMENTATION_BUDGET`: messen und im zustaendigen Issue entscheiden
- `FUTURE_RELEASE`: keine versteckte Teilimplementierung

`TBD_COMMISSIONING` darf nie als gueltiger Laufzeitwert in einer produktiven
Konfiguration akzeptiert werden.

## Dokumente

Vor dem jeweiligen Issue die dort genannten Quellen lesen. Die wichtigsten
Einstiege sind:

- [`SPECIFICATION_REVIEW.md`](SPECIFICATION_REVIEW.md)
- [`IMPLEMENTATION_PLAN.md`](IMPLEMENTATION_PLAN.md)
- [`IMPLEMENTATION_ISSUES.md`](IMPLEMENTATION_ISSUES.md)
- [`ACCEPTANCE_TESTS.md`](ACCEPTANCE_TESTS.md)
- [`OPEN_POINTS.md`](OPEN_POINTS.md)
- [`../AGENTS.md`](../AGENTS.md)

Bei Widerspruechen gilt die in `SPECIFICATION_REVIEW.md` festgelegte
Dokumentationsprioritaet.
