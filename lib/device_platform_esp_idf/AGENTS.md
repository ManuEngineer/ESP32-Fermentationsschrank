# AGENTS.md – device_platform_esp_idf

Diese Regeln gelten unter `lib/device_platform_esp_idf/`. Massgebend ist
ADR-013.

- Enthalten sind ausschliesslich konkrete ESP-IDF-Adapter fuer Ports aus
  `device_platform`.
- Eine Abhaengigkeit auf `device_platform` ist erlaubt; Abhaengigkeiten auf
  `fermentation_app` und `device_platform_test_support` sind nicht erlaubt.
- Fach-, Prozess- und Composition-Root-Logik sind nicht erlaubt.
- Adapter bleiben klein, explizit und gegen ihre portseitigen Vertraege
  testbar.
- Hardwarefreigaben bleiben fail-closed und verwenden keine unbestaetigten
  GPIO-, Pegel- oder Ressourcenannahmen.
