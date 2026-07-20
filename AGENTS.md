# AGENTS.md

Diese Datei gilt fuer das gesamte Repository.

## Projektziel

Entwickle eine robuste, nachvollziehbare und wartbare ESP32-Steuerung fuer den
`ESP32-Fermentationsschrank` (Owner: `ManuEngineer`) mit lokaler
Touch-Bedienung und lokaler Weboberflaeche. Das Geraet muss ohne Internet und
ohne Cloud vollstaendig funktionieren.

## Verbindliche Arbeitsreihenfolge

1. Lies vor jeder Aenderung `README.md` sowie alle Dateien unter `docs/` und
   `config/`.
2. Pruefe `config/pins.example.yaml` beziehungsweise eine spaetere lokale,
   verifizierte `config/pins.yaml`.
3. Behandle Angaben mit `candidate`, `unconfirmed`, `unverified`, `unknown`
   oder `TBD` als nicht bestaetigt. Weise daraus keine endgueltigen GPIOs,
   Pegel oder Anschlussbelegungen ab.
4. Fuehre nach jeder relevanten Aenderung mindestens `pio run` aus.
5. Fuehre bei Aenderungen an hardwareunabhaengiger Logik zusaetzlich
   `pio test -e native` aus.
6. Aktualisiere Dokumentation, maschinenlesbare Konfiguration und Code
   gemeinsam.

## Allgemeine Hardware- und Sicherheitsregeln

- Niemals GPIOs, aktive Pegel, Anschlussbelegungen oder Bootzustaende erraten.
- Aktoren muessen beim Booten, Flashen, Reset, Watchdog-Reset, Sensorausfall
  und internen Fehler in einen definierten sicheren Zustand wechseln.
- Gegensaetzliche Ausgaenge duerfen nie gleichzeitig aktiv sein.
- Keine Hardwaretests mit angeschlossener Hochleistungslast durchfuehren,
  bevor Logik und Ausgaenge mit Multimeter beziehungsweise geeigneter Testlast
  geprueft wurden.
- Blockierende lange `delay()`-Aufrufe in der Anwendung vermeiden.
- Sensorwerte auf CRC, Timeout, Bereich, Aktualitaet und Plausibilitaet pruefen.
- Fehler muessen sicher abschalten, einen typisierten Fehlercode setzen und
  lokal sichtbar protokolliert beziehungsweise angezeigt werden.
- Die Kernregelung darf nicht von WLAN, Browser, Touchdisplay oder Cloud
  abhaengen.
- Nach einem unkontrollierten Neustart darf kein Programm automatisch wieder
  Heizen oder Kuehlen starten, solange keine explizite sichere
  Wiederanlauflogik implementiert ist.

## Projektspezifische Hardware-Regeln

### BTS7960 und Peltier

- Die BTS7960-H-Bruecke zuerst ohne Peltier mit Multimeter oder kleiner
  Testlast pruefen.
- `RPWM` und `LPWM` niemals gleichzeitig aktiv setzen. Heizen und Kuehlen
  duerfen niemals gleichzeitig angefordert werden.
- Vor jedem Richtungswechsel: Enable beziehungsweise Leistung aus, beide
  Richtungssignale LOW, mindestens die konfigurierte Totzeit abwarten, neue
  Richtung setzen und erst danach Leistung freigeben.
- Eine Richtungsumkehr darf nur lastfrei erfolgen.
- In der ersten Implementierung keine hochfrequente direkte Peltier-PWM
  verwenden; vorgesehen sind lange zeitproportionale Schaltfenster.
- Peltier und Hochleistungszweig erst nach GPIO-, Pegel-, Richtungs-,
  Sicherungs-, Netzteil- und Waermeabfuhrpruefung anschliessen.

### DS18B20

- Geplant sind zwei von fuenf vorhandenen DS18B20 im 3-Leiter-Betrieb an einem
  gemeinsamen OneWire-Bus mit externem 4,7-kOhm-Pull-up nach 3,3 V. Die reale
  Verdrahtung bleibt `unconfirmed`; Parasite Power ist nicht vorgesehen.
- Sensoren anhand ihrer 64-Bit-ROM-Adresse persistent den Rollen Luft und
  Referenz zuordnen; niemals von der Erkennungsreihenfolge abhaengen.
- CRC-Fehler, Disconnect-Wert, Einschaltwert 85 Grad Celsius, fehlende
  Sensoren, Messalter, Spruenge und unplausible Werte pruefen. Fehler muessen
  die Peltier-Leistungsstufe sicher deaktivieren.
- Temperaturkonvertierungen nicht mit langen blockierenden `delay()`-Aufrufen
  abwarten.

### ILI9341 und XPT2046

- Das vorgesehene MSP2807-Modul verwendet dokumentiert einen ILI9341. Ob der
  Touchcontroller des gelieferten Exemplars XPT2046-kompatibel ist, bleibt bis
  zur Pruefung `unconfirmed`.
- Display und Touch als getrennte SPI-Teilnehmer mit getrennten
  Chip-Select-Signalen behandeln; nicht aktive Teilnehmer deselektieren.
- SPI-, Chip-Select-, Data/Command-, Reset-, IRQ- und Backlight-Pins bleiben bis
  zur realen Bestaetigung `unknown` oder `unconfirmed`.
- Rotation, Touchachsen, Kalibrierwerte, Versorgung und Backlight-Beschaltung
  am realen Modul pruefen. Kalibrierwerte spaeter persistent speichern.
- Ein Ausfall von Display oder Touch darf die sichere Regelung nicht stoppen.

### Vier Onboard-MOSFET-Ausgaenge

- GPIO-Zuordnung, Kanalreihenfolge, aktiven Pegel, Reset-/Bootloaderverhalten,
  Lastart, Schutzbeschaltung und Dauerstrombelastbarkeit jedes Kanals messen.
- Weder LOW noch HIGH gilt vor dieser Messung als sicherer Ausschaltpegel;
  solange der AUS-Pegel unbekannt ist, keinen Kanal als Ausgang aktivieren.
- Innen- und Aussenluefter sind nur als Lasten fuer Kanal 1 und 2 vorgesehen;
  Kanalzuordnung und aktive Pegel bleiben `unconfirmed`. Kanal 3 und 4 bleiben
  Reserve und AUS.
- Induktive Lasten nur mit bestaetigter Freilauf-/Schutzbeschaltung betreiben.

## Code-Regeln

- PlatformIO mit Arduino-Framework und dem generischen Ziel `esp32dev` fuer
  ESP32-WROOM-32E verwenden, solange keine dokumentierte technische
  Notwendigkeit fuer ESP-IDF besteht.
- C++17-kompatiblen, klar strukturierten Code schreiben.
- Eine nicht blockierende Zustandsmaschine verwenden.
- Zeitangaben mit `millis()`-sicherer Differenzbildung verarbeiten.
- Hardwareabhaengigkeiten hinter kleinen Schnittstellen kapseln.
- Zustaende, Fehlercodes und Programmschema als `enum class` und klar typisierte
  Structs modellieren.
- Regelung, Sicherheitslogik, Hardwareabstraktion, UI, Webserver und Persistenz
  trennen.
- Keine dynamische Speicherallokation in schnellen Regelpfaden.
- Hardware-Pins zentral konfigurieren und auf Doppelbelegung pruefen.
- Einstellungen validieren, bevor sie gespeichert oder angewendet werden.
- Ein laufendes Programm verwendet eine unveraenderliche Kopie seiner
  Startparameter. Aenderungen wirken erst beim naechsten Start.
- Programmdaten persistent speichern und validierte Werkseinstellungen im Code
  als Rueckfallebene vorhalten.
- Sicherheitslogik soweit moeglich hostseitig testen.
- Keine Geheimnisse, WLAN-Passwoerter, Tokens oder privaten Schluessel in Git
  einchecken.

## Dateien und Verantwortlichkeiten

- `src/main.cpp`: Initialisierung und minimaler sicherer Hardwaretest-Einstieg
- `lib/`: wiederverwendbare, testbare Projektkomponenten
- `include/`: gemeinsame Typen und Konfiguration
- `data/`: lokale Weboberflaeche fuer LittleFS
- `docs/`: Quelle der Wahrheit fuer Hardware und Anforderungen
- `config/`: maschinenlesbare Hardware-, Pin- und Standardkonfiguration
- `test/`: hardwareunabhaengige Unit-Tests

## Vor einem Commit

- `pio run`
- `pio test -e native`, sofern anwendbar
- keine Zugangsdaten oder lokale Konfiguration
- keine unbestaetigten Pins oder Pegel als Fakten
- Dokumentation und Konfiguration aktualisiert
- Fehlerpfade und sichere Ausgangszustaende geprueft

## Git

- Kleine, thematisch klare Commits erstellen.
- Lokale Geheimnisse nur ueber ignorierte Dateien wie `include/secrets.hpp`,
  `include/secrets.h`, `.env` oder lokale Build-Konfiguration einbinden.
- Ohne ausdruecklichen Auftrag nichts committen oder pushen.
