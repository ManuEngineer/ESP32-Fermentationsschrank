# Offene Hardware-, Inbetriebnahme- und Budgetpunkte

## Zweck

Diese Datei ist die nachweisgebundene Checkliste fuer Punkte, die reale
Hardware, thermische Messungen oder reproduzierbare Build- und
Ressourcenergebnisse benoetigen.

Ein Punkt wird nur abgehakt, wenn der konkrete Nachweis im zugehoerigen Issue,
PR, Messprotokoll, Hardwarebericht oder Buildartefakt verlinkt ist. Annahmen,
Lieferantenangaben zu aehnlicher Hardware und erfolgreiche Simulationen ersetzen
keinen realen Nachweis.

Kennzeichnungen:

- `TBD_HARDWARE`: durch reale Komponente, Datenblatt und praktische Pruefung zu
  klaeren;
- `TBD_COMMISSIONING`: durch dokumentierte thermische oder regelungstechnische
  Messung zu klaeren;
- `TBD_IMPLEMENTATION_BUDGET`: durch reproduzierbare Builds und Belastungstests
  zu klaeren.

Die vollstaendige Abgrenzung zu Zukunftsfunktionen steht ausschliesslich in
[`SPECIFICATION_REVIEW.md`](SPECIFICATION_REVIEW.md).

## `TBD_HARDWARE`

### Controllerboard und Ressourcenbasis

- [ ] tatsaechliche Platinenrevision und Modulbeschriftung dokumentieren
- [ ] erkannte Flashgroesse bestaetigen
- [ ] PSRAM erkennen; Firmware darf sie nicht voraussetzen
- [ ] reale Partitionstabelle und Appgroesse messen
- [ ] freien Heap, niedrigsten Heap und groessten freien Block erfassen
- [ ] UART-/FT232RL-Flash- und Recoveryweg bestaetigen

Nachverfolgung: #29

### GPIOs und Bootpegel

- [ ] alle verwendeten GPIOs festlegen
- [ ] Bootstrapping-Eignung pruefen
- [ ] aktive Pegel der Onboard-MOSFET-Ausgaenge messen
- [ ] Boot-, Reset-, Brownout- und Bootloaderverhalten messen
- [ ] nachweisen, dass unbekannte Aktoren beim Boot gesperrt bleiben

Nachverfolgung: #29 und #32

### Temperatursensoren

- [ ] Bustopologie festlegen: drei getrennte Busse oder feste Sensoren gemeinsam
- [ ] Pull-ups und Leitungslaengen bestimmen
- [ ] ROM-Adressen der festen Sensoren dokumentieren
- [ ] Produktfuehleranschluss waehlen; M8-3, GX12-3 oder andere verriegelbare
      3-polige Loesung vergleichen
- [ ] Verpolungs-, Fehlsteck- und ESD-Schutz des externen Anschlusses pruefen
- [ ] Lebensmitteltauglichkeit und Reinigbarkeit des Produktfuehlers bestaetigen
- [ ] Hot-Plug praktisch testen

Nachverfolgung: #30

### Display und Touch

- [ ] Displaycontroller ILI9341 praktisch bestaetigen
- [ ] Touchcontroller praktisch identifizieren; XPT2046 nicht ungeprueft annehmen
- [ ] SPI-Pins, Rotation, Reset und Hintergrundbeleuchtung festlegen
- [ ] Touchrohwerte, Kalibrierung und 10-Sekunden-Boot-Recovery testen
- [ ] Speicher- und Geschwindigkeitsbedarf der realen Darstellung messen

Nachverfolgung: #31

### Luefter, Summer und MOSFET-Ausgaenge

- [ ] Innen- und Aussenluefterdaten, Strom und Anlaufverhalten erfassen
- [ ] MOSFET-Kanaele unbelastet messen
- [ ] Verbraucher einzeln anschliessen und Bootverhalten erneut pruefen
- [ ] Summer-Spannung, Strom, Kanal und Lautstaerke festlegen
- [ ] Nachlaufverhalten auf realer Hardware pruefen

Nachverfolgung: #32

### BTS7960 und Leistungspfad

- [ ] genaue Modulvariante und Pinbeschriftung dokumentieren
- [ ] Enable- und Richtungspins festlegen
- [ ] Hardware-Pulldowns oder gleichwertige Freigabestufe nachweisen
- [ ] Ausgangspolaritaet ohne Peltier mit Multimeter messen
- [ ] R_IS/L_IS-Pegel und diagnostische Nutzbarkeit pruefen
- [ ] Sicherungstyp, Halter, Stecker und Leitungsquerschnitte dokumentieren
- [ ] erste begrenzte Heiz- und Kuehlpulse sicher durchfuehren

Nachverfolgung: #33

## `TBD_COMMISSIONING`

### Sensoren und thermischer Aufbau

- [ ] Referenzmessgeraete dokumentieren
- [ ] individuelle Offsets aller drei DS18B20 bestimmen
- [ ] Sensorpositionen und Temperaturverteilung messen
- [ ] leeren Schrank vermessen
- [ ] kleine und grosse Referenzmasse vermessen
- [ ] Produkttraegheit und Luftreaktion dokumentieren

Nachverfolgung: #34

### Regelparameter

- [ ] PI-Parameter Luft/Produkt und Heizen/Kuehlen bestimmen
- [ ] Schaltfenster und Verhalten kleiner Impulse festlegen
- [ ] Mindest-Ein- und Mindest-Auszeit festlegen
- [ ] Richtungswechselhysterese und Totzeit validieren
- [ ] Innen- und Aussenluefternachlauf festlegen
- [ ] Filterstaerken, Zielband, Qualifikationsdauer und Gnadenzeit festlegen
- [ ] Stabilitaetszeit fuer automatische Produktfuehler-Rueckkehr bestimmen

Nachverfolgung: #35

### Sicherheitsgrenzen

- [ ] fruehe Luftbegrenzungen bestimmen
- [ ] Prozesswarnschwellen bestimmen
- [ ] Sicherheits-Eingriffsgrenzen bestimmen
- [ ] harte obere und untere Notgrenzen bestimmen
- [ ] begrenzte Gegenrichtungsversuche validieren
- [ ] Kuehlkoerpergrenzen und Temperaturtrends festlegen
- [ ] einmalige Temperatursicherung auswaehlen, Rating und Montageort dokumentieren

Nachverfolgung: #35

### Standardprogramme

- [ ] Temperaturen und Zeiten der vier Standards praktisch festlegen
- [ ] Vorheiz- und Zielerreichungsgrenzen pruefen
- [ ] Abschluss- und Kuehlhalteverhalten validieren
- [ ] mindestens einen praktischen Lauf je Standardprogramm dokumentieren

Nachverfolgung: #36

## `TBD_IMPLEMENTATION_BUDGET`

### Flash und Partition

- [ ] Single-App-Partitionsplan fuer Release 1 festlegen
- [ ] App-, Konfigurations-, Journal-, Lauf- und Historienbudget dokumentieren
- [ ] notwendigen freien Sicherheitsabstand definieren
- [ ] Webressourcen und drei Sprachen messen
- [ ] Exportgroessen und temporaeren Speicher begrenzen
- [ ] NVS-Partitionsgroesse fuer Konfiguration, Slots und Secrets bestimmen
      (Backend gemaess ADR-016)

Nachverfolgung: #9, #10, #19, #28, #29 und #90

### RAM und Laufzeit

- [ ] Warn- und kritische Schwellen fuer freien Heap festlegen
- [ ] Mindestwert fuer den groessten freien Block festlegen
- [ ] Ringpuffer- und Warteschlangengroessen dokumentieren
- [ ] Web, Display, Export und Regelung unter Parallelbelastung messen
- [ ] Betriebszaehler je nach verbleibendem Budget vollstaendig, reduziert oder
      nicht persistent umsetzen

Nachverfolgung: #10, #28, #29 und #37

### Aufbewahrung

- [ ] Standard `aktiver Lauf + 5 detaillierte Laeufe + 50 Zusammenfassungen`
      am realen Budget pruefen
- [ ] maximale Detailaufloesung der aktuellen Laufhistorie festlegen
- [ ] proaktive Bereinigung und Reserve praktisch testen
- [ ] Schreibzaehler und ungewoehnliche Schreibrate diagnostisch bewerten

Nachverfolgung: #19 und #37

## Hardware- und Releaseabnahme

- [ ] vollstaendige Hardwareabnahme gemaess `ACCEPTANCE_TESTS.md`
- [ ] verpflichtende Fehlerinjektionen am realen Aufbau
- [ ] Stromunterbrechungen in allen wesentlichen Phasen
- [ ] lokale und Webbedienung auf realem ESP32
- [ ] siebentaegiger Dauer- und Belastungstest
- [ ] alle Release-Gates bewerten

Nachverfolgung: #36 und #37
