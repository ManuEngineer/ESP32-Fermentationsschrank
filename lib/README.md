# Modulstruktur

Die Firmware trennt wiederverwendbare Geraetedienste von der konkreten
Fermentationsanwendung.

```text
src/main.cpp
    Composition Root: instanziiert Plattform und Anwendung

lib/device_platform/
    anwendungsneutrale Dienste, Ports und Adapter

lib/fermentation_app/
    konkrete Fermentationsprogramme und Prozesslogik
```

## Abhaengigkeitsrichtung

```text
main -> DevicePlatform
main -> FermentationApplication
FermentationApplication -> IPlatformServices
DevicePlatform -X-> FermentationApplication
```

`-X->` bedeutet: Diese Abhaengigkeit ist nicht erlaubt.

Neue allgemeine Module werden zuerst innerhalb dieses Repositories entwickelt
und nativ getestet. Eine Auslagerung in eine eigenstaendig versionierte
ESP32-Geraeteplattform erfolgt erst mit einem zweiten realen Anwendungsfall oder
einem anderweitig klaren Wartungsvorteil.

Die verbindliche Entscheidung steht in
[`docs/ADR-013_REUSABLE_DEVICE_PLATFORM.md`](../docs/ADR-013_REUSABLE_DEVICE_PLATFORM.md).
