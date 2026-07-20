# AGENTS.md

Diese Datei gilt fuer das gesamte Repository.

## Ziel

Entwickle eine robuste, nachvollziehbare und wartbare ESP32-Firmware fuer `PROJECT_NAME`.

## Verbindliche Arbeitsreihenfolge

1. Lies zuerst:
   - `README.md`
   - `docs/HARDWARE.md`
   - `docs/REQUIREMENTS.md`
   - `docs/ARCHITECTURE.md`
   - `docs/DECISIONS.md`
   - `docs/OPEN_POINTS.md`
2. Pruefe `config/pins.example.yaml` beziehungsweise die projektspezifische `config/pins.yaml`.
3. Behandle alle nicht bestaetigten Pins als unbekannt.
4. Fuehre nach jeder relevanten Aenderung mindestens `pio run` aus.
5. Fuehre bei Aenderungen an hardwareunabhaengiger Logik `pio test -e native` aus.
6. Aktualisiere Dokumentation und Code gemeinsam.

## Hardware- und Sicherheitsregeln

- Niemals GPIOs, aktive Pegel oder Anschlussbelegungen erraten.
- Nicht bestaetigte Angaben mit `TODO`, `UNKNOWN` oder `unconfirmed` markieren.
- Aktoren muessen beim Booten, Reset, Sensorausfall und Fehlerzustand AUS sein.
- Gegensaetzliche Ausgaenge duerfen nie gleichzeitig aktiv sein.
- Richtungswechsel von H-Bruecken muessen lastfrei mit Totzeit erfolgen.
- Keine Hardwaretests mit angeschlossener Hochleistungslast, bevor die Ausgaenge mit Multimeter oder Testlast geprueft wurden.
- Blockierende lange `delay()`-Aufrufe in der eigentlichen Anwendung vermeiden.
- Sensorwerte auf Plausibilitaet pruefen.
- Fehler muessen sicher abschalten und sichtbar protokolliert werden.

## Code-Regeln

- C++17-kompatiblen, klar strukturierten Code schreiben.
- Hardwareabhaengigkeiten hinter kleinen Schnittstellen kapseln.
- Zustandsmaschinen explizit als `enum class` modellieren.
- Zeitangaben intern mit `millis()`-sicherer Differenzbildung verarbeiten.
- Keine dynamische Speicherallokation in schnellen Regelpfaden.
- Keine Geheimnisse, WLAN-Passwoerter oder Tokens in Git einchecken.
- Einstellungen validieren, bevor sie gespeichert oder angewendet werden.
- Ein laufendes Programm verwendet eine unveraenderliche Kopie seiner Startparameter.

## Dateien und Verantwortlichkeiten

- `src/main.cpp`: Initialisierung und Hauptschleife, moeglichst wenig Fachlogik
- `lib/`: wiederverwendbare, testbare Projektkomponenten
- `include/`: gemeinsame Typen und Konfiguration
- `data/`: Weboberflaeche fuer LittleFS
- `docs/`: Quelle der Wahrheit fuer Hardware und Anforderungen
- `config/`: maschinenlesbare Hardware-, Pin- und Standardkonfiguration
- `test/`: hardwareunabhaengige Unit-Tests

## Vor einem Commit

- `pio run`
- `pio test -e native`, sofern anwendbar
- keine Zugangsdaten
- keine unbestaetigten Pins als Fakten
- Dokumentation aktualisiert
- Fehlerpfade und sichere Ausgangszustaende geprueft
