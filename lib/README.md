# Modulindex

Dieser Index ist nicht normativ. Die verbindlichen Modulrollen und
Abhaengigkeitsrichtungen stehen in
[`ADR-013`](../docs/ADR-013_REUSABLE_DEVICE_PLATFORM.md).

| Verzeichnis | Verantwortung |
|---|---|
| `device_platform/` | portable, anwendungsneutrale Ports und Dienste |
| `device_platform_esp_idf/` | konkrete ESP-IDF-Adapter fuer `device_platform` |
| `device_platform_test_support/` | Mocks, Fehlerinjektion und Simulation fuer native Tests |
| `fermentation_app/` | Fermentationsprogramme, Prozesslogik und fachliche Bedienmodelle |

`src/main.cpp` ist der native Composition Root; `main/app_main.cpp` ist der
ESP-IDF Composition Root. Fuer Arbeiten in einem Modul gelten zusaetzlich die
dortigen lokalen `AGENTS.md`.
