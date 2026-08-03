# Modulstruktur

Die Firmware trennt wiederverwendbare Geraetedienste von der konkreten
Fermentationsanwendung.

```text
src/main.cpp
    native-only Composition Root

main/app_main.cpp
    ESP-IDF Composition Root: instanziiert ESP-IDF-Adapter und Anwendung

lib/device_platform/
    anwendungsneutrale Produktionsschnittstellen, Dienste und Adapter

lib/device_platform_test_support/
    deterministisch steuerbare Mockadapter und Simulation fuer native Tests;
    keine Produktionsabhaengigkeit

lib/fermentation_app/
    konkrete Fermentationsprogramme und Prozesslogik

lib/device_platform_esp_idf/
    konkrete ESP-IDF-Adapter; abhaengig von device_platform
```

## Abhaengigkeitsrichtung

```text
src/main.cpp -> native Plattform + FermentationApplication
main/app_main.cpp -> device_platform_esp_idf
main/app_main.cpp -> FermentationApplication
device_platform_esp_idf -> device_platform
FermentationApplication -> IPlatformServices
DevicePlatform -X-> FermentationApplication
device_platform_test_support -> device_platform
device_platform -X-> device_platform_test_support
FermentationApplication -X-> device_platform_test_support
main -X-> device_platform_test_support
device_platform_esp_idf -X-> FermentationApplication
device_platform_esp_idf -X-> device_platform_test_support
```

`-X->` bedeutet: Diese Abhaengigkeit ist nicht erlaubt. `device_platform_test_support`
wird ausschliesslich von nativen Tests eingebunden und darf die
Produktionsbibliothek `device_platform` nicht unnoetig vergroessern oder deren
oeffentliche API bestimmen.

Neue allgemeine Module werden zuerst innerhalb dieses Repositories entwickelt
und nativ getestet. Eine Auslagerung in eine eigenstaendig versionierte
ESP32-Geraeteplattform erfolgt erst mit einem zweiten realen Anwendungsfall oder
einem anderweitig klaren Wartungsvorteil.

Die verbindliche Entscheidung steht in
[`docs/ADR-013_REUSABLE_DEVICE_PLATFORM.md`](../docs/ADR-013_REUSABLE_DEVICE_PLATFORM.md).
