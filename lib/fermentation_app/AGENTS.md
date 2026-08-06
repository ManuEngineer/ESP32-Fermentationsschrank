# AGENTS.md – fermentation_app

Diese Regeln gelten unter `lib/fermentation_app/`. Massgebend ist ADR-013.

- Enthalten sind Fermentationsprogramme, Prozesslogik, Zustandsmaschine und
  fermentationsspezifische Bedienmodelle.
- Abhaengigkeiten sind nur auf schmale, abstrakte Ports aus `device_platform`
  erlaubt.
- Konkrete Hardwareadapter, ESP-IDF, Arduino, GPIO, WLAN, Dateisystem und reale
  Systemzeit duerfen nicht direkt verwendet werden.
- Eine Abhaengigkeit auf `device_platform_test_support` ist nicht erlaubt.
- Hardwarewirkungen werden ausschliesslich als abstrakte Anforderungen oder
  Kommandos an Ports uebergeben.
