# ESP32-Fermentationsschrank

Universeller, lokal bedienbarer Fermentationsschrank zum geregelten Heizen und
Kuehlen von Joghurt, Milch- und Wasserkefir sowie eigenen Fermentationsprozessen.

Owner: `ManuEngineer`

## Projektstatus

Die Anforderungen fuer Release 1 sind spezifiziert. Auf dem Branch
`docs/software-specification` laeuft der abschliessende Review in Pull Request
#38. Die eigentliche Fermentationssteuerung wird erst nach dem Merge dieser
Spezifikation umgesetzt.

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
- [`config/hardware.example.yaml`](config/hardware.example.yaml)
- [`config/pins.example.yaml`](config/pins.example.yaml)

## Prioritaet bei Widerspruechen

1. spaetere akzeptierte ADRs und `SPECIFICATION_REVIEW.md`
2. thematisch spezialisierte Spezifikationsdokumente
3. `REQUIREMENTS.md`, `ARCHITECTURE.md` und `HARDWARE.md`
4. Beispielkonfigurationen

Unbestaetigte Hardwarewerte bleiben `TBD_HARDWARE`; thermische Werte bleiben
`TBD_COMMISSIONING`; Speicher- und Ressourcengrenzen bleiben
`TBD_IMPLEMENTATION_BUDGET`.
