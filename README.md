# ESP32 Project Template

Vorlage fuer eigenstaendige ESP32-Projekte mit:

- PlatformIO
- Arduino Framework
- GitHub Actions
- Grundstruktur fuer Firmware, Hardwaredokumentation und Tests
- `AGENTS.md` fuer Codex
- sicherer Trennung von Quellcode, Konfiguration und Geheimnissen

## Neues Projekt aus dieser Vorlage

1. Dieses Repository auf GitHub unter **Settings → General → Template repository** als Vorlage markieren.
2. Auf der Repository-Seite **Use this template** waehlen.
3. Einen neuen Namen vergeben, beispielsweise `esp32-fermentationsschrank`.
4. Folgende Platzhalter ersetzen:
   - `PROJECT_NAME`
   - `PROJECT_DESCRIPTION`
   - `PROJECT_OWNER`
5. Hardware und Pinbelegung zuerst dokumentieren.
6. Erst danach Firmwarefunktionen implementieren.

## Lokale Einrichtung

Voraussetzungen:

- Visual Studio Code
- PlatformIO-Erweiterung oder PlatformIO Core
- Git
- USB-Treiber fuer den verwendeten USB-UART-Adapter

Bauen:

```bash
pio run
```

Seriellen Monitor oeffnen:

```bash
pio device monitor
```

Firmware hochladen:

```bash
pio run --target upload
```

Native Logiktests ausfuehren:

```bash
pio test -e native
```

## Projektstruktur

```text
.
├── .github/
│   ├── ISSUE_TEMPLATE/
│   └── workflows/
├── config/             Maschinenlesbare Projektkonfiguration
├── data/               LittleFS-/Webdateien
├── docs/               Hardware, Anforderungen und Entscheidungen
├── include/            Projektweite Header
├── lib/                Projektspezifische Bibliotheken
├── src/                Firmware
├── test/               Unit-Tests
├── AGENTS.md            Verbindliche Hinweise fuer Codex
├── platformio.ini       PlatformIO-Konfiguration
└── README.md
```

## Grundsaetze

- Jedes physische Geraet erhaelt ein eigenes Repository.
- Pinbelegungen werden nie geraten.
- Sicherheitskritische Ausgaenge muessen beim Booten sicher ausgeschaltet sein.
- Zugangsdaten werden nie committed.
- `main` soll jederzeit kompilierbar bleiben.
