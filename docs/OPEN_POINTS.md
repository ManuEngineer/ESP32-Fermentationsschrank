# Offene Punkte

## Controller und Pinbelegung

- [ ] Konkretes Board und ESP32-WROOM-32E-Modulrevision bestaetigen
- [ ] Schaltplan oder Leiterbahnzuordnung aller Anschluesse beschaffen
- [ ] Jeden GPIO und aktiven Pegel am realen Board messen
- [ ] Bootstrapping-, Flash- und intern belegte Pins ausschliessen
- [ ] Boot- und Resetverhalten aller Ausgaenge pruefen

## BTS7960 und Peltier

- [ ] RPWM-, LPWM-, REN- und LEN-Anschluesse sowie Pegel bestaetigen
- [ ] BTS7960-Modulvariante, Wahrheitstabelle und PWM-Grenzen bestaetigen
- [ ] Peltier-Nennspannung, Maximalstrom und Polaritaet dokumentieren
- [ ] Totzeit, Strombegrenzung, Sicherung und unabhaengige Abschaltung festlegen
- [ ] Kuehlkoerper, Luefter und Waermeabfuhr auslegen

## DS18B20

- [ ] Anzahl, ROM-Adressen und Einbauorte erfassen
- [ ] OneWire-GPIO, Pull-up-Wert und Versorgungsart bestaetigen
- [ ] Aufloesung, Abtastrate, Plausibilitaetsgrenzen und Timeout festlegen
- [ ] Verhalten bei Busfehler, CRC-Fehler und Sensorausfall definieren

## Display und Touch

- [ ] ILI9341-/XPT2046-Modulvarianten und Versorgung bestaetigen
- [ ] SPI-Leitungen, getrennte CS-Pins, DC, Reset, IRQ und Backlight bestaetigen
- [ ] SPI-Modus und maximal zulaessige Taktraten pruefen
- [ ] Rotation und Touchkalibrierung am montierten Display bestimmen

## Vier MOSFET-Ausgaenge

- [ ] GPIO und aktiven Pegel pro Kanal bestaetigen
- [ ] Last, Spannung, Dauer-/Spitzenstrom und thermische Grenze pro Kanal erfassen
- [ ] Gate-Pulldowns und sicheres Bootverhalten pruefen
- [ ] Freilauf-, Ueberspannungs- und Kurzschlussschutz je Last pruefen

## System

- [ ] Versorgungstopologie, Massefuehrung, Sicherungen und Leitungsquerschnitte
  dokumentieren
- [ ] Fachliche Fermentationsablaeufe und Grenzwerte spezifizieren
- [ ] Zustandsmaschine und Fehlercodes definieren
- [ ] Persistenz-, OTA-, Web-API- und Testkonzept definieren
- [ ] Gehaeuse, Feuchtigkeitsschutz, Zugentlastung und Wartungszugang klaeren
