# ESP32-Fermentationsschrank

Firmware-Grundgeruest fuer einen Fermentationsschrank mit einem generischen
ESP32-WROOM-32E, erstellt mit PlatformIO und dem Arduino Framework.

Der vorgesehene Aufbau umfasst eine BTS7960-H-Bruecke fuer ein Peltier-Element,
DS18B20-Temperatursensoren, ein ILI9341-Display mit XPT2046-Touchcontroller und
vier Onboard-MOSFET-Ausgaenge. Die reale Pinbelegung, aktive Pegel und elektrische
Grenzwerte sind noch nicht bestaetigt. Deshalb implementiert die Firmware derzeit
nur einen sicheren, hardwareunabhaengigen Build- und Diagnoseeinstieg und noch
keine Fermentationssteuerung.

## Werkzeugkette

- PlatformIO
- Arduino Framework
- generisches PlatformIO-Ziel `esp32dev` fuer ESP32-WROOM-32E
- C++17

## Bauen und testen

```bash
pio run
pio test -e native
```

Ein Upload auf reale Hardware ist erst zulaessig, nachdem die Pinbelegung und
die sicheren Ausgangspegel am konkreten Board bestaetigt und in einer lokalen,
nicht versionierten `config/pins.yaml` dokumentiert wurden.

## Projektstruktur

```text
config/  Maschinenlesbare Hardware- und Beispielkonfiguration
data/    Spaetere LittleFS-/Webdateien
docs/    Quelle der Wahrheit fuer Hardware, Anforderungen und Entscheidungen
include/ Gemeinsame Typen und Konfiguration
lib/     Wiederverwendbare, nativ testbare Komponenten
src/     Minimaler Firmware-Einstieg
test/    Hardwareunabhaengige Tests
```

## Sicherheit

- GPIOs und aktive Pegel werden nicht geraten.
- Nicht bestaetigte Angaben bleiben `candidate`, `unconfirmed` oder `unknown`.
- Heizen, Kuehlen und alle MOSFET-Ausgaenge muessen bei Boot, Reset und Fehlern
  AUS bleiben.
- Hochleistungslasten duerfen erst nach Messung mit Multimeter oder Testlast
  angeschlossen werden.
- Zugangsdaten und lokale Hardwarekonfiguration werden nicht versioniert.

Owner: ManuEngineer
