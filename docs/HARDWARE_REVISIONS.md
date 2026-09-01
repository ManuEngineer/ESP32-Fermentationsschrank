# Historie der Hardware-Revisionen

## Status

Die in diesem Dokument waehrend der Spezifikationsphasen gesammelten
Hardwareaenderungen sind in [`HARDWARE.md`](HARDWARE.md) konsolidiert.
`HARDWARE.md` ist ab Phase 10C die verbindliche Hardwarequelle. Dieses Dokument
bleibt als kurze Aenderungshistorie erhalten und hat keinen Vorrang mehr.

## Gegenueber der urspruenglichen Projektgrundlage akzeptierte Aenderungen

### Produktfuehler

- abnehmbarer DS18B20 statt Referenzflasche als allgemeines Konzept
- eigener externer 1-Wire-Bus
- gewoehnliche 3,5-mm-Klinke verworfen
- verriegelbarer und verpolungssicherer 3-poliger Anschluss noch
  `TBD_HARDWARE`

### Dritter Sicherheitssensor

- fester DS18B20 am Aussenwaermetauscher beziehungsweise Kuehlkoerper
- fuer jede Peltierfreigabe erforderlich
- dient Temperatur-, Trend- und Aktordiagnose

### 1-Wire-Topologie

- bevorzugt ein GPIO pro Sensor
- bei GPIO-Knappheit duerfen die beiden festen Sensoren einen Bus teilen
- Produktfuehler bleibt separat

### Peltier und BTS7960

- 7,5-A-Ueberstromsicherung
- Hardware-Pulldowns oder gleichwertige sichere Freigabestufe
- R_IS/L_IS in R1 deaktiviert, nicht angeschlossen und nicht implementiert;
  spaetere Nutzung nur als `FUTURE_RELEASE` mit eigenem Issue, Plan und
  Owner-Gate
- keine direkte Wiederherstellung von H-Bruecken- oder GPIO-Zustaenden

### Unabhaengige thermische Abschaltung

- einmalige Temperatursicherung fuer Release 1
- Rating und Montageort werden durch thermische Inbetriebnahme bestimmt

### Versorgung

- ESP32-Brownout und Resetursache werden in Release 1 ausgewertet
- direkte 12-V-ADC-Messung bleibt optionale spaetere Hardware

### Update

- FT232RL/UART ist der verbindliche Release-1-Update- und Recoveryweg
- Web-OTA und duale Firmware-Slots sind kein Release-1-Hardwarebedarf

### Wiederanlauf und Zeitquelle

- die generische Zeitplattform bleibt RTC-optional und NTP-only-fähig
- das konkrete Fermenter-R1-Produkt verlangt eine lokale RTC der
  DS3231-Familie für neue Offline-Läufe; ihre tatsächliche Variante bleibt
  `TBD_HARDWARE_CONFIRMATION`
- ein neuer produktiver Lauf ohne trusted UTC ist nicht zulässig
- ein exakt validierter Current-`FERMENTING`-Run wird nur mit trusted UTC nach
  #124 logisch automatisch fortgesetzt; fehlt sie, bleibt er
  `RecoveryEvaluation/WaitingForTrustedTime` ohne Aktorfreigabe
- NTP bleibt zusätzliche Zeitquelle und korrigiert keine historische
  #18-Progressrechnung im aktuellen R1-Pfad

## Nachverfolgung

Reale Hardwarepruefungen:

- #29 ist geschlossene historische ESP32-Bring-up-/Ressourcen-Evidenz;
  verbleibende allgemeine Board-/Ressourcennachweise sind `UNASSIGNED`, bis
  der Owner einen passenden späteren Hardware- oder Release-Scope bestimmt
- #30 DS18B20-Busse
- #31 Display und Touch
- #32 Luefter, Summer und MOSFET-Ausgaenge
- #33 BTS7960 und Peltierpulse

Thermische Festlegung:

- #34 Sensorvergleich und Grundvermessung
- #35 Regel- und Sicherheitsparameter
- #36 Hardwareabnahme und Fehlerinjektionen
