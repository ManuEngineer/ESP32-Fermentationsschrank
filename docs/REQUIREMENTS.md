# Anforderungen

## Muss-Anforderungen

- Das Geraet startet in einem sicheren Standby-Zustand.
- Peltier-H-Bruecke und alle vier MOSFET-Ausgaenge sind bei Boot, Reset,
  Sensorfehler und Softwarefehler AUS.
- Heizen und Kuehlen sind gegenseitig verriegelt; ein Richtungswechsel erfolgt
  nur stromlos und mit dokumentierter Totzeit.
- DS18B20-Werte werden anhand von CRC, Verbindung, Plausibilitaet und Messalter
  geprueft. Ungueltige Temperaturdaten verhindern jede Peltier-Freigabe.
- Display und Touch sind nicht Teil der Sicherheitskette. Ihr Ausfall darf keine
  Aktorfreigabe bewirken.
- Alle Bedienhandlungen erhalten eine eindeutige Rueckmeldung.
- Einstellungen werden validiert, bevor sie angewendet werden.
- Laufende Prozesse verwenden eine unveraenderliche Kopie ihrer Startparameter.
- Firmware muss ohne angeschlossene Hardware kompilieren.
- Fachlogik soll soweit sinnvoll nativ testbar sein.
- Nicht bestaetigte Pins oder Pegel werden nicht angesteuert.

## Soll-Anforderungen

- Lokale Bedienung ohne Cloud
- ILI9341-Anzeige mit XPT2046-Touch nach Hardwarebestaetigung
- Weboberflaeche im lokalen Netzwerk
- Access-Point-Fallback fuer die Ersteinrichtung
- OTA-Updates
- persistente, exportierbare Einstellungen
- nachvollziehbare Fehlercodes und serielle Diagnose

## Nicht-Ziele des aktuellen Stands

- eigentliche Fermentations-, Temperatur- oder Zeitsteuerung
- PID- oder Zweipunktregelung
- Ansteuerung unbestaetigter GPIOs oder angeschlossener Hochleistungslasten
- finale Displayoberflaeche, Web-API, OTA oder Persistenz

## Akzeptanzkriterien fuer das Grundgeruest

- [ ] `pio run` erfolgreich
- [ ] `pio test -e native` erfolgreich
- [x] keine endgueltige GPIO-Zuordnung im Repository
- [x] sicherer Einstieg konfiguriert keine unbekannten Ausgaenge
- [ ] sichere Ausgangszustaende spaeter am realen Aufbau gemessen
- [ ] Sensorfehler fuehrt nach Implementierung zur Abschaltung
- [x] Dokumentation kennzeichnet offene Hardwaredaten explizit
