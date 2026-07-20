# Anforderungen

## Muss-Anforderungen

- Das Geraet startet in einem sicheren Standby-Zustand.
- Kritische Ausgaenge sind bei Boot, Reset, Sensorfehler und Softwarefehler AUS.
- Alle Bedienhandlungen erhalten eine eindeutige Rueckmeldung.
- Einstellungen werden validiert, bevor sie angewendet werden.
- Laufende Prozesse werden durch nachtraegliche Konfigurationsaenderungen nicht
  unkontrolliert veraendert.
- Firmware muss ohne angeschlossene Hardware kompilieren.
- Fachlogik soll soweit sinnvoll nativ testbar sein.

## Soll-Anforderungen

- Lokale Bedienung ohne Cloud
- Weboberflaeche im lokalen Netzwerk
- Access-Point-Fallback fuer die Ersteinrichtung
- OTA-Updates
- persistente, exportierbare Einstellungen
- nachvollziehbare Fehlercodes
- serielle Diagnoseausgabe

## Nicht-Ziele

- TODO

## Akzeptanzkriterien

- [ ] `pio run` erfolgreich
- [ ] `pio test -e native` erfolgreich
- [ ] sichere Ausgangszustaende mit Messung bestaetigt
- [ ] Sensorfehler fuehrt zur Abschaltung
- [ ] Neustart fuehrt nicht zu unbeabsichtigtem Start
- [ ] Dokumentation entspricht dem realen Aufbau
