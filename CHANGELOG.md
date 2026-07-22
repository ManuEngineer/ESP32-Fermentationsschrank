# Changelog

Alle wesentlichen Aenderungen dieses Projekts werden hier dokumentiert.

## Unreleased

### Added

- Initiale Projektstruktur
- Template auf ESP32-Fermentationsschrank angepasst
- Hardwarekomponenten und Sicherheitsregeln ohne GPIO-Festlegung dokumentiert
- PlatformIO-Profile `native`, `esp32_bringup` und `esp32_release`
- getestete sichere Buildrichtlinien fuer 4 MB Flash, Betrieb ohne PSRAM,
  deaktiviertes Web-OTA und gesperrte reale Aktoren
- virtuelle Zeitquelle `ITimeSource`/`VirtualTimeSource` (monoton und optionale
  UTC-Zeit) mit nativen Tests fuer Zeitfortschaltung, Neustart und
  Zeitvorwaertssprung
- CI-Qualitaetspruefungen: Formatpruefung (clang-format 18.1.8), Static
  Analysis (clang-tidy 18.1.8), Geheimnis-/Lokalkonfigurationspruefung,
  ADR-013-Architekturgrenzen und Firmware-/Ressourcen-Groessenbericht als
  Buildartefakt
- Selbsttest, der beweist, dass Format-, Static-Analysis-, Geheimnis- und
  Architekturpruefung absichtlich fehlerhafte Faelle erkennen
- `-Werror` fuer native und ESP32-Profile; `library.json` in
  `lib/device_platform/`, `lib/device_platform_test_support/` und
  `lib/fermentation_app/`, damit `-Wall -Wextra -Werror` auch dort greifen
  (PlatformIOs `build_src_flags` galt bisher nur fuer `src/`)
- Der gemeinsame `IActuatorSink` wurde durch die kleinen anwendungsneutralen
  Ports `IBidirectionalActuatorSink` und `IBinaryOutputSink` ersetzt; zusammen
  mit `ITemperatureSource`, `IStateStore`, `IEventJournal`, `INetworkStatus` und
  `IUserNotificationSink` stehen deterministisch steuerbare native Mockadapter
  bereit
- einfaches, ausdruecklich unkalibriertes thermisches Simulationsmodell
  (`ThermalSimulationModel`) fuer deterministische Heiz-/Kuehlverlaeufe in
  nativen Tests
- native Tests fuer Sensor-/Aktorfehlerinjektion, Stromausfall-/Neustart-
  Verhalten, Persistenz-Fehlerinjektion und begrenzte Journal-/
  Benachrichtigungspuffer
- explizite Persistenzergebnisse, die fehlende Werte von Lesefehlern
  unterscheiden und fehlgeschlagene Journalschreibvorgaenge melden
- neue interne Bibliothek `lib/device_platform_test_support/` fuer
  Mockadapter und Simulation; `device_platform` enthaelt jetzt ausschliesslich
  anwendungsneutrale Produktionsschnittstellen und -dienste (ADR-013)
- `IEventJournal` von der Mock-Speicherstruktur entkoppelt: der Port kennt nur
  noch `record(...)`, `entries()` ist eine Testhilfe von `MockEventJournal`
- ADR-013 um die verbindliche Trennung von Produktionsplattform und
  `device_platform_test_support` sowie um kleine, rollenunabhaengige Ports
  praezisiert
- `scripts/check_architecture_boundaries.py` erzwingt die erlaubte
  Abhaengigkeitsrichtung und Modulplatzierung in CI
- unbenutzten Include `<cmath>` in `thermal_simulation_model.cpp` entfernt
- versioniertes Release-1-Programmmodell mit deterministischer Struktur- und
  Startvalidierung, explizitem Migrationsschritt von Schema 3 auf 4 sowie
  unveraenderlichem Factory-Katalog fuer Joghurt mild, Joghurt stichfest,
  Milch- und Wasserkefir
- getrennte aktive Programmauswahl und Benutzerkopien; offene
  `TBD_COMMISSIONING`-Werte bleiben als Katalogvorlage ladbar, werden aber als
  produktive Laufzeitwerte sicher abgelehnt
- unveraenderliche Laufschnappschuesse mit getrennten wirksamen Ziel-/Zeitwerten,
  atomaren und begrenzten Laufrevisionen sowie streng validierter
  Wiederherstellung durch deterministisches Wiederabspielen der Historie
