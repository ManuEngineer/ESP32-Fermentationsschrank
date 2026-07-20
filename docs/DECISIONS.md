# Technische Entscheidungen

## ADR-001: PlatformIO mit Arduino Framework

- **Status:** accepted
- **Datum:** 2026-07-20
- **Kontext:** Reproduzierbarer Build fuer lokalen Einsatz und CI.
- **Entscheidung:** PlatformIO mit Arduino Framework, C++17 und dem generischen
  Ziel `esp32dev` fuer das vorgesehene ESP32-WROOM-32E-Modul.
- **Alternativen:** ESP-IDF direkt, Arduino IDE.
- **Folgen:** Das Ziel bestaetigt keine konkrete Boardrevision oder Pinbelegung.

## ADR-002: Keine GPIO-Zuweisung vor Hardwarebestaetigung

- **Status:** accepted
- **Datum:** 2026-07-20
- **Kontext:** Importierte Komponentenangaben enthalten keine verifizierte
  Anschlussbelegung oder aktive Pegel.
- **Entscheidung:** Die Firmware verwendet keine Kandidatenpins und
  `src/main.cpp` konfiguriert auch keinen vermeintlichen Onboard-LED-Pin.
  Zahlenwerte duerfen nur in `config/pins.example.yaml` mit explizitem Status
  `candidate_unconfirmed` dokumentiert werden; die lokale bestaetigte
  `config/pins.yaml` bleibt ignoriert.
- **Alternativen:** Plausible Standardpins als Kandidaten in Firmware verwenden.
- **Folgen:** Der Build ist hardwareunabhaengig sicher, ein Hardwaretest ist bis
  zur Verifikation bewusst eingeschraenkt.

## ADR-003: Noch keine Fermentationssteuerung

- **Status:** accepted
- **Datum:** 2026-07-20
- **Kontext:** Elektrische Grenzen, Sensorzuordnung und Sicherheitsanforderungen
  des realen Aufbaus sind noch offen.
- **Entscheidung:** Zunaechst nur Projektstruktur, Dokumentation, Metadaten und
  ein serieller Diagnoseeinstieg.
- **Alternativen:** Vorlaeufige Regelungslogik implementieren.
- **Folgen:** Keine unbeabsichtigte Aktorfreigabe durch unbestaetigte Annahmen.
