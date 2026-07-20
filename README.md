# ESP32-Fermentationsschrank

Universeller Fermentationsschrank für Joghurt, Milch- und Wasserkefir sowie Kombucha.

Owner: `ManuEngineer`

Die Steuerung basiert auf einem ESP32-32E-Board mit vier MOSFET-Ausgängen. Ein vorhandenes 12-V-/60-W-Peltiermodul kann über eine BTS7960-H-Brücke sowohl heizen als auch kühlen. Zwei DS18B20 erfassen Schrank-/Lufttemperatur und eine Referenztemperatur, die die Produkttemperatur annähert. Die lokale Bedienung erfolgt über ein 2,8-Zoll-Touchdisplay; zusätzlich ist eine lokale Weboberfläche vorgesehen.

## Dokumentation

- [`AGENTS.md`](AGENTS.md): verbindliche Arbeitsanweisungen für Codex
- [`docs/HARDWARE.md`](docs/HARDWARE.md): Hardware, Schnittstellen und Verdrahtungsregeln
- [`docs/REQUIREMENTS.md`](docs/REQUIREMENTS.md): gewünschtes Geräteverhalten
- [`docs/CODEX_HANDOFF.md`](docs/CODEX_HANDOFF.md): empfohlene Entwicklungsreihenfolge und Startprompt
- [`docs/OPEN_POINTS.md`](docs/OPEN_POINTS.md): vor der finalen Verdrahtung zu prüfende Punkte
- [`config/hardware.example.yaml`](config/hardware.example.yaml): versionierte maschinenlesbare Hardwarebeschreibung
- [`config/pins.example.yaml`](config/pins.example.yaml): vorläufige GPIO-Zuordnung
- [`config/programs.example.yaml`](config/programs.example.yaml): Schema für die Fermentationsprogramme
- [`references/LINKS.md`](references/LINKS.md): Hersteller- und Lieferantendokumentation

## Geplanter Software-Stack

- PlatformIO
- Arduino-Framework für ESP32
- lokale Touch-Bedienung
- lokaler Webserver ohne Cloud-Zwang
- persistente Konfiguration im Flash
- OTA-Updates über WLAN nach erfolgreichem Erst-Flashen

Buildziel ist das generische PlatformIO-Board `esp32dev` fuer ein
ESP32-WROOM-32E mit Arduino-Framework. Diese Auswahl bestaetigt keine GPIO-
Zuordnung der konkreten Quad-MOSFET-Platine.

## Bauen und testen

```bash
pio run
pio test -e native
```

Ein Upload auf reale Hardware ist erst zulaessig, nachdem Pinbelegung und
sichere Ausgangspegel bestaetigt und in der lokalen, ignorierten
`config/pins.yaml` dokumentiert wurden.

## Projektstruktur

```text
config/   Maschinenlesbare Hardware- und Beispielkonfiguration
data/     Spaetere lokale LittleFS-Weboberflaeche
docs/     Hardware, Anforderungen, Architektur und Entscheidungen
include/  Projektweite Header und lokale, ignorierte Geheimnisse
lib/      Hardwareunabhaengige und wiederverwendbare Komponenten
src/      Minimaler Firmware-Einstieg
test/     Native Unit-Tests
```

## Projektstatus

**Hardware bestellt, elektrische und GPIO-Verifikation noch ausstehend.**

Vor dem Anschluss von Peltier und Lüftern muss jede Schnittstelle einzeln geprüft werden. Insbesondere sind die GPIO-Zuordnung der vier Onboard-MOSFET-Kanäle, der Touchcontroller des MSP2807 und die Belastbarkeit der 5-V-Schiene des ESP32-Boards noch am realen Exemplar zu bestätigen.

Lokale Messergebnisse koennen in den von Git ignorierten Dateien
`config/hardware.yaml` und `config/pins.yaml` gepflegt werden. Bestaetigte,
allgemein gueltige Ergebnisse muessen anschliessend auch in den versionierten
Dokumenten und Beispielen nachgefuehrt werden.
