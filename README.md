# ESP32-Fermentationsschrank

Universeller, lokal bedienbarer Fermentationsschrank zum geregelten Heizen und
Kuehlen von Joghurt, Milch- und Wasserkefir sowie eigenen Fermentationsprozessen.

Owner: `ManuEngineer`

## Projektstatus

Die Anforderungen fuer Release 1 sind spezifiziert und mit Pull Request #38 nach
`main` uebernommen. Die Implementierung erfolgt issueweise, beginnend mit der
PlatformIO- und Projektgrundlage aus Issue #9.

Die geplante Implementierung ist in den Epics #2 bis #8 und den Arbeits- und
Abnahme-Issues #9 bis #37 strukturiert. Das erste Implementierungs-Issue ist #9.

## Zielhardware

Verbindliche Planungsbasis:

- ESP32-WROOM-32E beziehungsweise bestellte ESP32-32E-Boardvariante
- 4 MB Flash
- keine PSRAM-Abhaengigkeit
- vier Onboard-MOSFET-Ausgaenge, Belegung und aktive Pegel noch zu messen
- Peltier mit etwa 12 V / 60 W ueber BTS7960-H-Bruecke
- Innen- und Aussenluefter
- 2,8-Zoll-Touchdisplay, 320 x 240 Pixel, ILI9341; Touchcontroller praktisch zu
  bestaetigen
- drei DS18B20 im ersten Aufbau:
  1. Schrankluft
  2. abnehmbarer Produktfuehler
  3. Aussenwaermetauscher beziehungsweise Kuehlkoerper
- 7,5-A-Ueberstromsicherung im Peltierpfad
- einmalige Temperatursicherung als unabhaengige thermische Notabschaltung
- aktiver Summer fuer lokale Meldungen
- FT232RL/UART fuer initiales Flashen, Updates und Wiederherstellung

Keine GPIO-Zuordnung und kein aktiver Pegel gilt vor der realen Messung als
bestaetigt.

## Funktionsumfang von Release 1

- vier allgemeine Standardprogramme:
  - Joghurt mild
  - Joghurt stichfest
  - Milchkefir
  - Wasserkefir
- dynamisch erweiterbare Benutzerprogramme
- produkt- oder luftgefuehrte Temperaturregelung
- optionales Vorheizen und getrennte Zielqualifikation
- zeitproportionale PI-Regelung fuer Heizen, neutralen Bereich und Kuehlen
- vollstaendige lokale Bedienung am Touchdisplay
- responsive lokale Weboberflaeche
- Deutsch, Spanisch und Englisch
- Betrieb ohne Cloud, Internet, Heimserver oder funktionierendes WLAN
- phasenbezogener automatischer Wiederanlauf nach Stromunterbrechung
- atomare Konfiguration und Laufpersistenz mit Rueckfallrevisionen
- Fehlerklassen, Verriegelungen, `SAFE_BOOT` und gefuehrte Servicepruefungen
- Lauf-, Diagnose- und Servicebericht-Exporte ohne Geheimnisse

Nicht Bestandteil von Release 1 sind unter anderem Web-OTA, Cloud- oder
Push-Benachrichtigungen, ein Tuerkontakt, ein eigener WireGuard-Client, eine
Pflicht-RTC, eine 12-V-ADC-Messung, Kaskadenregelung, PID-Autotuning und
automatische Wartungserinnerungen.

## Entwicklungsstrategie

Die Software wird vor der Hardwareintegration weitgehend als nativ testbarer
fachlicher Kern entwickelt.

Vorgesehene Profile:

```text
native          Fachlicher Kern, Simulation und automatische Tests
esp32_bringup   Reale Hardwarepruefung mit gesperrten Aktoren
esp32_release   Freigegebene Zielkonfiguration nach Hardwareabnahme
```

Die Projektgrundlage setzt diese Profile wie folgt um:

| Profil | Ziel und sichere Startpolitik |
|---|---|
| `native` | Hostbuild ohne Arduino-Hardware; nur Fachlogik und Tests |
| `esp32_bringup` | generisches `esp32dev`-Ziel, Start als `HARDWARE_UNVERIFIED`, Aktoren gesperrt |
| `esp32_release` | generisches `esp32dev`-Ziel, Freigabepolitik verlangt ein spaeter bestaetigtes Hardwareprofil; standardmaessig bleiben Aktoren gesperrt |

Beide ESP32-Profile planen mit 4 MB Flash und setzen keine PSRAM voraus. Web-OTA
ist als `FUTURE_RELEASE` deaktiviert. Es ist noch keine projektspezifische
Partitionstabelle festgelegt; Layout und Budgets bleiben bis zu realen Build- und
Hardwaremessungen `TBD_IMPLEMENTATION_BUDGET`. Ebenso sind keine Hardwaretreiber,
GPIOs oder aktiven Pegel Bestandteil dieser Grundlage.

Die Grundlage trennt Projektkonfiguration, wiederverwendbare Plattform und
konkrete Anwendung:

```text
include/
    gemeinsame Projekt- und Buildkonfiguration

src/main.cpp
    Composition Root; verbindet Plattform und Anwendung

lib/device_platform/
    anwendungsneutrale Geraetedienste und Schnittstellen

lib/fermentation_app/
    Fermentationsprogramme und konkrete Prozesslogik

test/
    native Unit-, Integrations- und Konfigurationspruefungen
```

Die Fermentations-App verwendet nur die Schnittstelle `IPlatformServices` und
kennt weder Arduino noch die konkrete Klasse `DevicePlatform`. Die
projektspezifische `app_config.hpp` bleibt in `main.cpp` und wird nicht in die
wiederverwendbare Plattform gezogen. Verbindliche Details und die spaetere
Auslagerungsstrategie stehen in
[`ADR-013`](docs/ADR-013_REUSABLE_DEVICE_PLATFORM.md).

Der fachliche Zustandsautomat in `lib/fermentation_app` ist ebenfalls frei von
Hardware- und Persistenzzugriffen. Er berechnet mit monotoner virtueller Zeit
reversible Uebergangsentscheidungen fuer Vorheizen, Produktwartephase,
Zielqualifikation, Fermentation, Kuehlen und Halten. Erst eine getrennte
Bestaetigung uebernimmt den neuen Zustand; veraltete Entscheidungen werden ohne
Teilmutation abgelehnt.

Darauf aufbauend verarbeitet die fachliche Kommandoschicht bestaetigte Starts,
manuelle Laufplaene, Stoppen, Abschluss, Laufanpassungen, Meldungsaktionen und
qualifizierte Fehlerresetabsichten ebenfalls zweistufig. Display und Web sind
gleichberechtigte Quellen; Kommando-IDs sowie Zustands- und Fachrevisionen
verhindern doppelte oder veraltete Anwendungen. Persistenz, konkrete UI und
Aktorwirkung bleiben getrennten Folge-Issues vorbehalten.

Alle Profile bauen und die nativen Tests laufen mit:

```bash
pio run
pio test -e native
python scripts/check_platformio_config.py
```

Seit Issue #10 pruefen CI und lokal zusaetzlich Formatierung, Static Analysis,
Geheimnisse und einen Firmware-/Ressourcen-Groessenbericht:

```bash
clang-format --dry-run --Werror $(find src include lib test -type f \( -name "*.cpp" -o -name "*.hpp" -o -name "*.h" \))
pio run -e native -t compiledb && clang-tidy -p . include/app_config.hpp \
  lib/device_platform/src/device_platform.cpp \
  lib/device_platform/src/virtual_time_source.cpp \
  lib/fermentation_app/src/fermentation_application.cpp \
  lib/fermentation_app/src/process_state_machine.cpp \
  lib/fermentation_app/src/run_commands.cpp src/main.cpp
python scripts/check_secrets.py
python scripts/build_report.py --output build-report.md native esp32_bringup esp32_release
```

Details, Werkzeugversionen, die virtuelle Zeitquelle `ITimeSource`/
`VirtualTimeSource` sowie die PASS-/FAILED-/BLOCKED-Konvention stehen in
[`docs/CI_AND_QUALITY_GATES.md`](docs/CI_AND_QUALITY_GATES.md).

Die reproduzierbare CI-Grundlage verwendet PlatformIO Core `6.1.19` und
`espressif32` `7.0.1`. Das Pruefskript liest sowohl die effektiv aufgeloeste
Projektkonfiguration als auch die Boardmetadaten von PlatformIO. Dadurch werden
4 MB Flash und der interne 320-KiB-RAM-Rahmen ohne vorausgesetzte PSRAM
unabhaengig von den Anwendungs-Makros kontrolliert. Bei einem fehlgeschlagenen
Firmwarebuild stellt CI den kompakten PlatformIO-Buildlog als Artefakt bereit.

Der Kern greift nicht direkt auf GPIO, 1-Wire, Display, WLAN oder Flash zu,
sondern verwendet klar getrennte Adapter. Dadurch koennen Zustandsmaschine,
Persistenz, Sensorlogik, PI-Regler, Sicherheit, UI-Modelle und Weboberflaeche
bereits vor Eintreffen der Hardware gegen simulierte Sensoren und Aktoren getestet
werden.

Bei der realen Inbetriebnahme werden Ausgaenge zuerst unbelastet gemessen. Danach
werden Sensoren, Display, Luefter und Summer einzeln integriert. Die BTS7960 wird
ohne Peltier geprueft. Erst anschliessend sind zeitlich und leistungsmassig
begrenzte Peltierpulse im geschuetzten Bring-up-/Servicemodus erlaubt.

## Verbindliche Dokumentation

Einstieg:

- [`docs/SPECIFICATION_PLAN.md`](docs/SPECIFICATION_PLAN.md)
- [`docs/SPECIFICATION_REVIEW.md`](docs/SPECIFICATION_REVIEW.md)
- [`docs/IMPLEMENTATION_PLAN.md`](docs/IMPLEMENTATION_PLAN.md)
- [`docs/IMPLEMENTATION_ISSUES.md`](docs/IMPLEMENTATION_ISSUES.md)
- [`docs/ACCEPTANCE_TESTS.md`](docs/ACCEPTANCE_TESTS.md)
- [`docs/ADR-013_REUSABLE_DEVICE_PLATFORM.md`](docs/ADR-013_REUSABLE_DEVICE_PLATFORM.md)
- [`docs/CI_AND_QUALITY_GATES.md`](docs/CI_AND_QUALITY_GATES.md)
- [`lib/README.md`](lib/README.md)

Fachliche Spezifikation:

- [`docs/PRODUCT_VISION.md`](docs/PRODUCT_VISION.md)
- [`docs/PROGRAMS.md`](docs/PROGRAMS.md)
- [`docs/STANDARD_PROGRAMS.md`](docs/STANDARD_PROGRAMS.md)
- [`docs/STATE_MACHINE.md`](docs/STATE_MACHINE.md)
- [`docs/RECOVERY_AND_INTERRUPTION.md`](docs/RECOVERY_AND_INTERRUPTION.md)
- [`docs/LOCAL_UI.md`](docs/LOCAL_UI.md)
- [`docs/LOCAL_UI_PROGRAMS.md`](docs/LOCAL_UI_PROGRAMS.md)
- [`docs/LOCAL_RUNTIME_UI.md`](docs/LOCAL_RUNTIME_UI.md)
- [`docs/WEB_UI.md`](docs/WEB_UI.md)
- [`docs/NETWORK.md`](docs/NETWORK.md)
- [`docs/SETTINGS_AND_STORAGE.md`](docs/SETTINGS_AND_STORAGE.md)
- [`docs/CONFIGURATION_PERSISTENCE.md`](docs/CONFIGURATION_PERSISTENCE.md)
- [`docs/RUN_PERSISTENCE.md`](docs/RUN_PERSISTENCE.md)
- [`docs/TEMPERATURE_CONTROL.md`](docs/TEMPERATURE_CONTROL.md)
- [`docs/ACTUATOR_TIMING.md`](docs/ACTUATOR_TIMING.md)
- [`docs/SENSOR_TUNING_COMMISSIONING.md`](docs/SENSOR_TUNING_COMMISSIONING.md)
- [`docs/SAFETY_AND_FAULTS.md`](docs/SAFETY_AND_FAULTS.md)
- [`docs/SAFETY_COMPONENT_FAULTS.md`](docs/SAFETY_COMPONENT_FAULTS.md)
- [`docs/SYSTEM_SAFETY_AND_RECOVERY.md`](docs/SYSTEM_SAFETY_AND_RECOVERY.md)

Hardware und offene Punkte:

- [`docs/HARDWARE.md`](docs/HARDWARE.md)
- [`docs/OPEN_POINTS.md`](docs/OPEN_POINTS.md)
- [`references/LINKS.md`](references/LINKS.md) – Hardwarequellen und lokal
  archivierte Datenblaetter
- [`config/hardware.example.yaml`](config/hardware.example.yaml)
- [`config/pins.example.yaml`](config/pins.example.yaml)

## Prioritaet bei Widerspruechen

Die verbindliche und vollstaendige Reihenfolge steht ausschliesslich in
[`docs/SPECIFICATION_REVIEW.md`](docs/SPECIFICATION_REVIEW.md) im Abschnitt
`Dokumentationsprioritaet`. Dadurch gibt es keine zweite, verkuerzte Liste, die
spaeter von der kanonischen Reihenfolge abweichen kann.

Unbestaetigte Hardwarewerte bleiben `TBD_HARDWARE`; thermische Werte bleiben
`TBD_COMMISSIONING`; Speicher- und Ressourcengrenzen bleiben
`TBD_IMPLEMENTATION_BUDGET`.
