# AGENTS.md – device_platform_test_support

Diese Regeln gelten unter `lib/device_platform_test_support/`. Massgebend ist
ADR-013.

- Enthalten sind nur deterministische Mocks, Fehlerinjektionen, Simulationen
  und Testhilfen fuer Ports aus `device_platform`.
- Reale Hardware-, ESP-IDF- und Produktionsadapter sind nicht erlaubt.
- Fermentationslogik und Abhaengigkeiten auf `fermentation_app` sind nicht
  erlaubt.
- Produktionsmodule und Composition Roots duerfen nicht von diesem Verzeichnis
  abhaengen.
- Testhilfen duerfen die Produktions-API nicht unnoetig vergroessern.
