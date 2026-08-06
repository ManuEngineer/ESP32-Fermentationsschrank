# AGENTS.md – device_platform

Diese Regeln gelten unter `lib/device_platform/`. Massgebend ist ADR-013.

- Enthalten sind nur portable, anwendungsneutrale Ports und Dienste.
- ESP-IDF-, Arduino-, GPIO-, RTOS- und andere konkrete Hardwareadapter sind
  nicht erlaubt; sie gehoeren nach `device_platform_esp_idf`.
- Fermentationsbegriffe und Abhaengigkeiten auf `fermentation_app` sind nicht
  erlaubt.
- Testhilfen, Mocks und Abhaengigkeiten auf `device_platform_test_support` sind
  nicht erlaubt.
- Oeffentliche Ports bleiben schmal, allgemein benannt und fuer andere
  Geraeteanwendungen sinnvoll.
- Neue Module muessen im Profil `native` testbar sein.
