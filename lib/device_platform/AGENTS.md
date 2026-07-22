# AGENTS.md – device_platform

Diese Regeln gelten fuer alle Dateien unter `lib/device_platform/`.

## Zweck

Dieses Verzeichnis enthaelt ausschliesslich wiederverwendbare,
anwendungsneutrale Geraetedienste. Massgebend ist
[`docs/ADR-013_REUSABLE_DEVICE_PLATFORM.md`](../../docs/ADR-013_REUSABLE_DEVICE_PLATFORM.md).

## Erlaubt

- allgemeine Zeit-, Konfigurations-, Persistenz-, Netzwerk-, Diagnose- und
  UI-Grunddienste
- allgemeine Sensorqualitaet, Filter und begrenzte Reglerbausteine
- Ports sowie native und ESP32-spezifische Adapter
- schmale Schnittstellen, die von Anwendungen und Tests injiziert werden

## Nicht erlaubt

- Joghurt-, Kefir-, Kombucha- oder andere Fermentationsprogramme
- Fermentationszustaende und fermentationsspezifische Standardwerte
- Abhaengigkeiten auf `lib/fermentation_app/`
- direkte Freigabe unbestaetigter GPIOs oder Aktoren
- Arduino-Abhaengigkeiten in hardwareunabhaengigen Kernmodulen

## Qualitaetsregel

Neue Plattformmodule muessen im Profil `native` testbar sein. Namen und APIs
muessen auch fuer einen anderen Geraetetyp wie Smoker, Gewaechshaus oder
Temperaturueberwachung sinnvoll bleiben.
