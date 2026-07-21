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
- R_IS/L_IS nur nach realer Verifikation
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

- NTP ist die primaere absolute Zeitquelle
- eine spaetere RTC ist optional
- der sichere phasenbezogene Wiederanlauf beginnt vor beziehungsweise ohne NTP
- fehlende absolute Zeit allein fuehrt nicht zum Abbruch
- bis eine verlaessliche Zeit vorliegt, wird konservativ mit niedriger
  Vertrauensstufe bewertet
- spaeter eintreffende NTP-Zeit darf die Unterbrechungsdauer und den Fortschritt
  nachtraeglich korrigieren

## Nachverfolgung

Reale Hardwarepruefungen:

- #29 ESP32-Bring-up, Partition und Pegel
- #30 DS18B20-Busse
- #31 Display und Touch
- #32 Luefter, Summer und MOSFET-Ausgaenge
- #33 BTS7960 und Peltierpulse

Thermische Festlegung:

- #34 Sensorvergleich und Grundvermessung
- #35 Regel- und Sicherheitsparameter
- #36 Hardwareabnahme und Fehlerinjektionen
