# Offene Punkte und Inbetriebnahme-Checkliste

## Controllerboard

- [ ] Tatsaechliche Platinenrevision dokumentieren
- [ ] Exakte ESP32-WROOM-32E-Bestellvariante beziehungsweise Modulbeschriftung
      erfassen
- [x] Bestellte Produktvariante mit 4 MB Flash dokumentiert
- [ ] Tatsaechlich erkannte Flashgroesse per Test-Firmware bestaetigen
- [ ] Vorhandensein und Groesse einer eventuellen PSRAM pruefen; die Firmware darf
      PSRAM nicht voraussetzen
- [ ] Pruefen, ob USB-C nur Versorgung ist
- [ ] GPIO-Zuordnung OUT1–OUT4 messen
- [ ] Active-high/active-low der MOSFET-Kanaele bestimmen
- [ ] Ausgangszustaende bei Boot, Reset und Bootloader messen
- [ ] 5-V-Ausgangsspannung und verfuegbare Reserve unter Last messen
- [ ] Pruefen, ob `esp32dev` die Flashparameter der bestaetigten 4-MB-Variante
      korrekt abbildet
- [ ] GPIO4 und alle weiteren Strapping-Pins aus der Verdrahtung ausschliessen
      oder ihr Bootverhalten explizit nachweisen

## Display und Touch

- [ ] Touchcontroller anhand Chipbeschriftung bestaetigen
- [ ] Bibliothek und Initialisierung pruefen
- [ ] Displayrotation festlegen
- [ ] Touchkalibrierung durchfuehren
- [ ] Backlight-Versorgung und Helligkeit pruefen
- [ ] Entscheiden, ob Display-RESET an GPIO oder Resetnetz gelegt wird

## Temperatursensoren

- [ ] Fuenf Sensoren pruefen und ROM-Adressen erfassen
- [ ] Mindestens zwei Sensoren mit plausibler Uebereinstimmung auswaehlen
- [ ] Luftfuehler fest zuordnen
- [ ] Abnehmbaren Produktfuehler festlegen
- [ ] Kabelfarben/Pinbelegung der gelieferten Sonden pruefen
- [ ] Steckverbinder fuer den Produktfuehler auswaehlen
- [ ] Hot-Plug-Verhalten des 1-Wire-Busses pruefen
- [ ] Feuchte-, Kondensat- und Zugentlastungskonzept fuer den Anschluss festlegen
- [ ] Lebensmitteleignung und Reinigbarkeit bei direktem Produktkontakt klaeren
- [ ] Optionales Referenzgefaess oder andere thermische Kopplung nur bei Bedarf
      festlegen

## Peltier und BTS7960

- [ ] Tatsaechlichen Peltierstrom messen
- [ ] Heizrichtung und Kuehlrichtung bestimmen
- [ ] BTS7960-Modulrevision und Pinbeschriftung pruefen
- [ ] Enable-Verhalten pruefen
- [ ] Totzeit testen
- [ ] 7,5-A-Sicherung und Sicherungshalter vorsehen
- [ ] Unabhaengige Uebertemperaturabschaltung festlegen
- [ ] Pruefen, ob 60 W das Peltier oder das gesamte Originalgeraet bezeichnet

## Luefter, Summer und Thermik

- [ ] Stromaufnahme und Anlaufstrom beider Luefter messen
- [ ] Luftfuehrung und Nachlaufzeit festlegen
- [ ] Temperaturgleichmaessigkeit an mehreren Positionen messen
- [ ] Kondensatfuehrung im Kuehlbetrieb vorsehen
- [ ] Aktiven 5-V- oder 12-V-Summer auswaehlen
- [ ] Stromaufnahme und Lautstaerke des Summers pruefen
- [ ] Freien MOSFET-Kanal oder separate Treiberstufe fuer den Summer festlegen
- [ ] Sichere Ausgangslage des Summers bei Boot und Reset pruefen

## Programme

- [x] Vier Standardprogramme festgelegt
- [x] Produkt- und luftgefuehrten Betrieb als zulaessige Modi festgelegt
- [x] Optionales Vorheizen mit zweiter Startbestaetigung festgelegt
- [x] Zielqualifikation von der Fermentationszeit getrennt
- [ ] Zieltemperaturen und Zeiten festlegen
- [ ] Zielband, Qualifikationsdauer und Ausreisser-Gnadenzeit festlegen
- [ ] Maximale Zielerreichungszeit pro Programm festlegen
- [ ] Warnschwellen fuer Temperaturabweichungen festlegen
- [ ] Kuehlziel je Programm festlegen
- [ ] Verhalten nach `FINISHED` je Programm festlegen

## Mechanik

- [ ] Position des Displays in der Tuer festlegen
- [ ] Elektronik vor Feuchtigkeit/Kondensat schuetzen
- [ ] Sensor- und Leistungskabel raeumlich trennen
- [ ] Servicezugang fuer FT232RL und Sicherung vorsehen
- [ ] Anschlussposition fuer den abnehmbaren Produktfuehler festlegen
- [ ] Position und Schallaustritt fuer den Summer festlegen

## Firmware

- [ ] Bestaetigte Pins und aktive Pegel in lokaler `config/pins.yaml` erfassen
- [ ] Sichere GPIO-Initialisierung erst nach dieser Verifikation implementieren
- [ ] Zustands- und Fehlercode-Modell implementieren und nativ testen
- [x] Fachliches Persistenz-, Sicherungs- und Wiederherstellungsmodell festgelegt
- [ ] Konkreten 4-MB-Partitionsplan fuer Firmware, OTA, Konfiguration, Laufdaten
      und Historie erstellen
- [ ] Maximales Flash- und RAM-Budget fuer jede Hauptfunktion festlegen
- [ ] Test-Firmware zur Messung von freiem Heap, groesstem freien Block und
      Flashbelegung erstellen
- [ ] Laufpersistenz und Messhistorie auf 4 MB Flash ohne vorausgesetzte PSRAM
      nachweisen
- [ ] Wear-Leveling- beziehungsweise Journalstrategie festlegen und testen
- [ ] Verhalten bei vollem oder beschaedigtem Datenspeicher implementieren und
      testen
- [ ] Verhalten des Aussenluefters je Fehlerart vervollstaendigen
- [ ] Sensorwechsel oder Sensorausfall waehrend eines Laufs implementieren und
      testen
- [ ] Warnungen, Quittierung und Summermuster implementieren und testen
- [ ] Web-API, OTA-Schutz und Access-Point-Einrichtung implementieren und testen
- [ ] Entwicklerzugang ueber UART fuer Diagnose vorsehen; ein vollstaendiges
      Geheimnisbackup ist keine Release-Anforderung
