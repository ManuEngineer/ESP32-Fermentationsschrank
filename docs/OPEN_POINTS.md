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
- [ ] Mindestens zwei Sensoren mit plausibler Übereinstimmung auswählen
- [ ] Luftfuehler fest zuordnen
- [ ] Abnehmbaren Produktfuehler festlegen
- [ ] Kabelfarben/Pinbelegung der gelieferten Sonden prüfen
- [ ] Steckverbinder fuer den Produktfuehler auswaehlen
- [ ] Hot-Plug-Verhalten des 1-Wire-Busses pruefen
- [ ] Feuchte-, Kondensat- und Zugentlastungskonzept fuer den Anschluss festlegen
- [ ] Lebensmitteleignung und Reinigbarkeit bei direktem Produktkontakt klaeren
- [ ] Optionales Referenzgefaess oder andere thermische Kopplung nur bei Bedarf
      festlegen

## Peltier und BTS7960

- [ ] Tatsächlichen Peltierstrom messen
- [ ] Heizrichtung und Kühlrichtung bestimmen
- [ ] BTS7960-Modulrevision und Pinbeschriftung prüfen
- [ ] Enable-Verhalten prüfen
- [ ] Totzeit testen
- [ ] 7,5-A-Sicherung und Sicherungshalter vorsehen
- [ ] unabhängige Übertemperaturabschaltung festlegen
- [ ] Prüfen, ob 60 W das Peltier oder das gesamte Originalgerät bezeichnet

## Lüfter, Summer und Thermik

- [ ] Stromaufnahme und Anlaufstrom beider Lüfter messen
- [ ] Luftführung und Nachlaufzeit festlegen
- [ ] Temperaturgleichmässigkeit an mehreren Positionen messen
- [ ] Kondensatführung im Kühlbetrieb vorsehen
- [ ] aktiven 5-V- oder 12-V-Summer auswaehlen
- [ ] Stromaufnahme und Lautstaerke des Summers pruefen
- [ ] freien MOSFET-Kanal oder separate Treiberstufe fuer den Summer festlegen
- [ ] sichere Ausgangslage des Summers bei Boot und Reset pruefen

## Programme

- [x] Vier Standardprogramme festgelegt
- [x] Produkt- und luftgefuehrten Betrieb als zulaessige Modi festgelegt
- [x] Optionales Vorheizen mit zweiter Startbestaetigung festgelegt
- [x] Zielqualifikation von der Fermentationszeit getrennt
- [ ] Zieltemperaturen und Zeiten festlegen
- [ ] Zielband, Qualifikationsdauer und Ausreisser-Gnadenzeit festlegen
- [ ] maximale Zielerreichungszeit pro Programm festlegen
- [ ] Warnschwellen fuer Temperaturabweichungen festlegen
- [ ] Kühlziel je Programm festlegen
- [ ] Verhalten nach `FINISHED` je Programm festlegen

## Mechanik

- [ ] Position des Displays in der Tür festlegen
- [ ] Elektronik vor Feuchtigkeit/Kondensat schützen
- [ ] Sensor- und Leistungskabel räumlich trennen
- [ ] Servicezugang für FT232RL und Sicherung vorsehen
- [ ] Anschlussposition fuer den abnehmbaren Produktfuehler festlegen
- [ ] Position und Schallaustritt fuer den Summer festlegen

## Firmware

- [ ] Bestaetigte Pins und aktive Pegel in lokaler `config/pins.yaml` erfassen
- [ ] Sichere GPIO-Initialisierung erst nach dieser Verifikation implementieren
- [ ] Zustands- und Fehlercode-Modell implementieren und nativ testen
- [ ] Persistenzformat und Validierungsstrategie festlegen
- [ ] Verhalten des Aussenluefters je Fehlerart festlegen
- [ ] Sensorwechsel oder Sensorausfall waehrend eines Laufs festlegen
- [ ] Warnungen, Quittierung und Summermuster festlegen
- [ ] Web-API, OTA-Schutz und Access-Point-Einrichtung festlegen
