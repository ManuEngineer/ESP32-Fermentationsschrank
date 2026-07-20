# Übergabe an Codex

## Empfohlene Entwicklungsreihenfolge

### Phase 1: Repository und Build

- PlatformIO-Projekt mit Arduino-Framework für `esp32dev` anlegen
- zentrale Hardwarekonfiguration erstellen
- seriellen Bootlog und Heartbeat ausgeben
- `pio run` erfolgreich ausführen

### Phase 2: Controllerboard verifizieren

- GPIO-Testprogramm
- Onboard-MOSFET-Ausgänge einzeln prüfen
- tatsächliche Zuordnung in `config/pins.yaml` dokumentieren
- aktive Pegel und Bootzustände messen
- alle Ausgänge beim Booten auf den danach bestätigten AUS-Pegel setzen

### Phase 3: Temperaturfühler

- 1-Wire-Bus initialisieren
- alle ROM-Adressen ausgeben
- Sensorrollen persistent zuweisen
- CRC-, Timeout- und Plausibilitätsfehler behandeln

### Phase 4: Display und Touch

- ILI9341 initialisieren
- Touchcontroller prüfen und kalibrieren
- einfache Statusseite
- Kalibrierwerte persistent speichern

### Phase 5: BTS7960 ohne Peltier

- sichere H-Brückenabstraktion
- gegenseitige Verriegelung
- Richtungswechsel mit Totzeit
- Tests mit Multimeter oder kleiner Testlast

### Phase 6: Zustandsmaschine

- Prozesszustände implementieren
- Programmdatenmodell und Persistenz
- zeitproportionale Regelung
- Fehlerbehandlung

### Phase 7: Weboberfläche und OTA

- lokaler Webserver
- Programmverwaltung
- Livewerte und Verlauf
- OTA-Update
- Access-Point-Fallback

## Startprompt für Codex

```text
Arbeite in diesem Repository als Embedded-Softwareentwickler.

Lies zuerst vollständig:
- AGENTS.md
- docs/HARDWARE.md
- docs/REQUIREMENTS.md
- docs/OPEN_POINTS.md
- config/hardware.example.yaml
- eine gegebenenfalls vorhandene lokale config/hardware.yaml
- config/pins.example.yaml
- config/programs.example.yaml

Erstelle zunächst nur Phase 1 und Phase 2 aus docs/CODEX_HANDOFF.md:

1. Lege ein PlatformIO-Projekt mit Arduino-Framework für einen generischen ESP32-WROOM-32E an.
2. Verwende keine unbestätigten GPIO-Zuordnungen als endgültige Wahrheit.
3. Übernimm Kandidaten erst nach Messung in eine lokale Pin-Konfiguration und ergänze Compile-Time-Prüfungen gegen Doppelbelegungen.
4. Implementiere einen sicheren Hardware-Basiszustand erst mit bestätigten Pins und aktiven Pegeln. Bis dahin darf der Build keine Aktor-GPIOs konfigurieren.
5. Erstelle danach einen seriellen Diagnosemodus, mit dem die vier Onboard-MOSFET-Kanäle einzeln und zeitlich begrenzt getestet werden können.
6. Es darf noch kein Peltier angeschlossen oder automatisch angesteuert werden.
7. Dokumentiere, wie das Board mit dem FT232RL geflasht wird.
8. Führe pio run aus und behebe alle Buildfehler.
9. Aktualisiere die Dokumentation nur mit tatsächlich verifizierten Ergebnissen; markiere ungetestete Punkte weiterhin als unverified.
```
