# Offene Punkte und Inbetriebnahme-Checkliste

## Controllerboard

- [ ] Tatsächliche Platinenrevision dokumentieren
- [ ] Prüfen, ob USB-C nur Versorgung ist
- [ ] GPIO-Zuordnung OUT1–OUT4 messen
- [ ] Active-high/active-low der MOSFET-Kanäle bestimmen
- [ ] Ausgangszustände bei Boot, Reset und Bootloader messen
- [ ] 5-V-Ausgangsspannung und verfügbare Reserve unter Last messen
- [ ] Pruefen, ob `esp32dev` Flashparameter der gelieferten Revision korrekt
      abbildet
- [ ] GPIO4 und alle weiteren Strapping-Pins aus der Verdrahtung ausschliessen
      oder ihr Bootverhalten explizit nachweisen

## Display und Touch

- [ ] Touchcontroller anhand Chipbeschriftung bestätigen
- [ ] Bibliothek und Initialisierung prüfen
- [ ] Displayrotation festlegen
- [ ] Touchkalibrierung durchführen
- [ ] Backlight-Versorgung und Helligkeit prüfen
- [ ] Entscheiden, ob Display-RESET an GPIO oder Resetnetz gelegt wird

## Temperatursensoren

- [ ] Fünf Sensoren prüfen und ROM-Adressen erfassen
- [ ] Zwei Sensoren mit plausibler Übereinstimmung auswählen
- [ ] Sensorrollen fest zuordnen
- [ ] Kabelfarben/Pinbelegung der gelieferten Sonden prüfen
- [ ] Referenzflasche und thermische Kopplung festlegen

## Peltier und BTS7960

- [ ] Tatsächlichen Peltierstrom messen
- [ ] Heizrichtung und Kühlrichtung bestimmen
- [ ] BTS7960-Modulrevision und Pinbeschriftung prüfen
- [ ] Enable-Verhalten prüfen
- [ ] Totzeit testen
- [ ] 7,5-A-Sicherung und Sicherungshalter vorsehen
- [ ] unabhängige Übertemperaturabschaltung festlegen
- [ ] Prüfen, ob 60 W das Peltier oder das gesamte Originalgerät bezeichnet

## Lüfter und Thermik

- [ ] Stromaufnahme und Anlaufstrom beider Lüfter messen
- [ ] Luftführung und Nachlaufzeit festlegen
- [ ] Temperaturgleichmässigkeit an mehreren Positionen messen
- [ ] Kondensatführung im Kühlbetrieb vorsehen

## Programme

- [ ] Namen der finalen 4–5 Programme festlegen
- [ ] Zieltemperaturen und Zeiten festlegen
- [ ] Stabilisierungskriterien festlegen
- [ ] Kühlziel je Programm festlegen
- [ ] Verhalten nach `FINISHED` festlegen

## Mechanik

- [ ] Position des Displays in der Tür festlegen
- [ ] Elektronik vor Feuchtigkeit/Kondensat schützen
- [ ] Sensor- und Leistungskabel räumlich trennen
- [ ] Servicezugang für FT232RL und Sicherung vorsehen

## Firmware

- [ ] Bestaetigte Pins und aktive Pegel in lokaler `config/pins.yaml` erfassen
- [ ] Sichere GPIO-Initialisierung erst nach dieser Verifikation implementieren
- [ ] Zustands- und Fehlercode-Modell implementieren und nativ testen
- [ ] Persistenzformat und Validierungsstrategie festlegen
- [ ] Verhalten des Aussenluefters je Fehlerart festlegen
- [ ] Web-API, OTA-Schutz und Access-Point-Einrichtung festlegen
