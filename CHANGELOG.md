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
  Analysis (clang-tidy 18.1.8), Geheimnis-/Lokalkonfigurationspruefung und
  Firmware-/Ressourcen-Groessenbericht als Buildartefakt
- Selbsttest, der beweist, dass Format-, Static-Analysis- und
  Geheimnispruefung absichtlich fehlerhafte Faelle erkennen
- `-Werror` fuer native und ESP32-Profile; `library.json` in
  `lib/device_platform/` und `lib/fermentation_app/`, damit
  `-Wall -Wextra -Werror` auch dort greifen (PlatformIOs `build_src_flags` galt
  bisher nur fuer `src/`)
- Anwendungsneutrale Schnittstellen `ITemperatureSource`, `IActuatorSink`,
  `IStateStore`, `IEventJournal`, `INetworkStatus` und `IUserNotificationSink`
  mit deterministisch steuerbaren nativen Mockadaptern
- einfaches, ausdruecklich unkalibriertes thermisches Simulationsmodell
  (`ThermalSimulationModel`) fuer deterministische Heiz-/Kuehlverlaeufe in
  nativen Tests
- native Tests fuer Sensor-/Aktorfehlerinjektion, Stromausfall-/Neustart-
  Verhalten, Persistenz-Fehlerinjektion und begrenzte Journal-/
  Benachrichtigungspuffer
