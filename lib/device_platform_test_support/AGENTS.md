# AGENTS.md – device_platform_test_support

Diese Regeln gelten fuer alle Dateien unter `lib/device_platform_test_support/`.

## Zweck

Dieses Verzeichnis enthaelt ausschliesslich deterministisch steuerbare
Mockadapter und Simulation fuer native Tests der Geraeteplattform. Massgebend
ist [`docs/ADR-013_REUSABLE_DEVICE_PLATFORM.md`](../../docs/ADR-013_REUSABLE_DEVICE_PLATFORM.md).

## Erlaubt

- Mockimplementierungen der Ports aus `lib/device_platform/`
- einfache, ausdruecklich unkalibrierte Simulationsmodelle
- Testhilfsmethoden (z. B. Befehlsjournale, `entries()`), die nicht Teil der
  Produktionsschnittstelle sind

## Nicht erlaubt

- Joghurt-, Kefir-, Kombucha- oder andere Fermentationsprogramme
- Fermentationszustaende und fermentationsspezifische Standardwerte
- Abhaengigkeiten auf `lib/fermentation_app/`
- eine Abhaengigkeit von `lib/device_platform/` auf dieses Verzeichnis
- Einbindung durch `src/main.cpp` oder in einem ESP32-Produktionsbuild
- reale GPIOs, Busse oder Aktorlogik

## Qualitaetsregel

Diese Bibliothek darf `device_platform` nicht unnoetig vergroessern oder deren
oeffentliche API bestimmen: Neue Produktionsports gehoeren in
`lib/device_platform/`, ihre Mocks hierher. Module muessen im Profil `native`
testbar sein.
