# AGENTS.md

Diese Datei gilt fuer das gesamte Repository.

## Ziel

Entwickle eine robuste, nachvollziehbare und wartbare ESP32-Firmware fuer den
`ESP32-Fermentationsschrank` (Owner: ManuEngineer) mit PlatformIO, Arduino
Framework und einem generischen ESP32-WROOM-32E-Ziel.

## Verbindliche Arbeitsreihenfolge

1. Lies zuerst `README.md` sowie alle Dateien unter `docs/` und `config/`.
2. Pruefe `config/pins.example.yaml` beziehungsweise die lokale
   `config/pins.yaml`.
3. Behandle alle nicht bestaetigten Pins, aktiven Pegel und Anschlussbelegungen
   als unbekannt.
4. Fuehre nach jeder relevanten Aenderung mindestens `pio run` aus.
5. Fuehre bei Aenderungen an hardwareunabhaengiger Logik `pio test -e native`
   aus.
6. Aktualisiere Dokumentation und Code gemeinsam.

## Allgemeine Hardware- und Sicherheitsregeln

- Niemals GPIOs, aktive Pegel oder Anschlussbelegungen erraten.
- Nicht bestaetigte Angaben mit `candidate`, `unconfirmed` oder `unknown`
  kennzeichnen und nicht als Fakt im Produktivcode verwenden.
- Aktoren muessen beim Booten, Reset, Sensorausfall und Fehlerzustand AUS sein.
- Gegensaetzliche Ausgaenge duerfen nie gleichzeitig aktiv sein.
- Richtungswechsel von H-Bruecken muessen lastfrei mit Totzeit erfolgen.
- Keine Hardwaretests mit angeschlossener Hochleistungslast, bevor die Ausgaenge
  mit Multimeter oder geeigneter Testlast geprueft wurden.
- Blockierende lange `delay()`-Aufrufe in der eigentlichen Anwendung vermeiden.
- Sensorwerte auf Aktualitaet und Plausibilitaet pruefen.
- Fehler muessen sicher abschalten und sichtbar protokolliert werden.

## Projektspezifische Hardware-Regeln

### BTS7960-H-Bruecke und Peltier-Element

- RPWM und LPWM nie gleichzeitig aktivieren; Enable-Leitungen gehoeren in die
  sichere Abschaltkette.
- Vor jedem Wechsel zwischen Heizen und Kuehlen PWM auf null setzen, die Bruecke
  deaktivieren und eine dokumentierte Totzeit abwarten.
- Boot-, Reset- und Floating-Zustaende duerfen das Peltier nicht bestromen.
- PWM-Frequenz, aktive Pegel, Enable-Logik, Stromgrenze und Kuehlkoerperauslegung
  erst nach Datenblatt- und Hardwarepruefung festlegen.
- Heizen und Kuehlen sind gegenseitig verriegelt. Sensorfehler, veraltete Werte,
  Uebertemperatur oder unplausible Messungen schalten beide Richtungen AUS.
- Das Peltier darf nicht ohne nachgewiesene Waermeabfuhr und abgesicherte
  Stromversorgung unter Leistung getestet werden.

### DS18B20

- OneWire-Pin, Pull-up-Wert, Versorgung (parasitaer oder extern) und Sensoranzahl
  bleiben bis zur Messung `unknown` oder `unconfirmed`.
- Sensoren ueber ihre eindeutige ROM-Adresse zuordnen; Reihenfolge am Bus ist
  keine stabile Identitaet.
- CRC, Disconnect-Wert, Einschaltwert 85 Grad Celsius, Wertebereich, Spruenge und
  Messalter pruefen. Ein einzelner Messfehler darf keine Aktorfreigabe erzeugen.
- Aufloesungsabhaengige Wandlungszeit nicht mit langen blockierenden `delay()`
  abwarten.

### ILI9341 und XPT2046

- SPI-Bus, Chip-Select-, Data/Command-, Reset-, Interrupt- und
  Hintergrundbeleuchtungs-Pins bleiben bis zur realen Bestaetigung unassigned.
- Geteilte SPI-Leitungen sind nur mit getrennten, definiert inaktiven
  Chip-Select-Signalen zulaessig.
- Touch-Kalibrierung, Rotation und Displayausrichtung gehoeren zur konkreten
  Hardwarekonfiguration und duerfen nicht geraten werden.
- Das Display oder dessen Ausfall darf keine sicherheitskritische Freigabe
  steuern; Bedienhandlungen brauchen eindeutige Rueckmeldung.

### Vier Onboard-MOSFET-Ausgaenge

- GPIOs, aktive Pegel, Bootverhalten, Lastart, Maximalstrom und vorhandene
  Schutzbeschaltung jedes Kanals einzeln bestaetigen.
- Alle vier Kanaele vor sonstiger Initialisierung sicher AUS schalten; solange
  der AUS-Pegel unbekannt ist, darf kein GPIO als Ausgang aktiviert werden.
- Induktive Lasten nur mit bestaetigter Freilauf-/Schutzbeschaltung betreiben.
- Strom-, Spannungs- und thermische Grenzen der Platine nicht aus der Anzahl der
  Kanaele ableiten.

## Code-Regeln

- C++17-kompatiblen, klar strukturierten Code schreiben.
- Hardwareabhaengigkeiten hinter kleinen Schnittstellen kapseln.
- Zustandsmaschinen explizit als `enum class` modellieren.
- Zeitangaben intern mit `millis()`-sicherer Differenzbildung verarbeiten.
- Keine dynamische Speicherallokation in schnellen Regelpfaden.
- Keine Geheimnisse, WLAN-Passwoerter oder Tokens in Git einchecken.
- Einstellungen validieren, bevor sie gespeichert oder angewendet werden.
- Ein laufendes Programm verwendet eine unveraenderliche Kopie seiner
  Startparameter.

## Dateien und Verantwortlichkeiten

- `src/main.cpp`: Initialisierung und minimaler sicherer Hardwaretest-Einstieg
- `lib/`: wiederverwendbare, testbare Projektkomponenten
- `include/`: gemeinsame Typen und Konfiguration
- `data/`: Weboberflaeche fuer LittleFS
- `docs/`: Quelle der Wahrheit fuer Hardware und Anforderungen
- `config/`: maschinenlesbare Hardware-, Pin- und Standardkonfiguration
- `test/`: hardwareunabhaengige Unit-Tests

## Vor einem Commit

- `pio run`
- `pio test -e native`, sofern anwendbar
- keine Zugangsdaten oder lokalen Konfigurationen
- keine unbestaetigten Pins als Fakten
- Dokumentation aktualisiert
- Fehlerpfade und sichere Ausgangszustaende geprueft
