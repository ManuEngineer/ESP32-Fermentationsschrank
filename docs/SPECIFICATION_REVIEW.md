# Gesamtreview der Softwarespezifikation

## Ergebnis

Phase 10C hat die Spezifikation fuer Release 1 auf Konsistenz, Umfang,
Hardwareannahmen, Ressourcenrahmen und Implementierbarkeit geprueft. Die beiden
Reviews von PR #38 wurden anschliessend nochmals ausgewertet. Ihre verbindlichen
Korrekturen stehen in
[`PR38_REVIEW_CORRECTIONS.md`](PR38_REVIEW_CORRECTIONS.md).

Ergebnis des Reviews:

- Die fachlichen Produktentscheidungen fuer Release 1 sind ausreichend festgelegt.
- Die Hardwarearchitektur verwendet drei DS18B20 und keine Referenzflasche als
  allgemeines Regelkonzept.
- Release 1 verwendet UART/FT232RL fuer Updates und reserviert keine dualen
  OTA-Slots.
- Die Software wird vor der Hardwareintegration als nativ testbarer Kern mit
  Mockadaptern entwickelt.
- Unbestaetigte GPIOs, Pegel, Controllerdetails und reale Grenzwerte bleiben
  sichtbar offen.
- Die in den PR-Reviews gefundenen sicherheitsrelevanten Widersprueche wurden als
  verbindliche Korrekturen dokumentiert und in die betroffenen Kerndokumente
  uebernommen.
- Es sind nach Einbezug dieser Korrekturen keine bekannten ungeklaerten
  sicherheitsrelevanten Produktentscheidungen zur Implementierung freigegeben.
- Verbleibende Unsicherheiten sind Hardware-, Inbetriebnahme- oder
  Ressourcenmessungen und besitzen eigene Issues.

Die Spezifikation ist nach abgeschlossenem Owner-Review und Merge von PR #38
bereit fuer die issueweise Implementierung ab #9.

## Dokumentationsprioritaet

Bei scheinbaren Widerspruechen gilt folgende Reihenfolge:

1. spaeter datierte akzeptierte ADRs in `DECISIONS.md`
2. [`PR38_REVIEW_CORRECTIONS.md`](PR38_REVIEW_CORRECTIONS.md)
3. dieses Reviewdokument
4. thematisch spezialisierte Spezifikationsdokumente
5. `REQUIREMENTS.md`, `ARCHITECTURE.md` und `HARDWARE.md`
6. Beispielkonfigurationen
7. historische Phasen- und Revisionsnotizen

Historische Abschnitte mit Titeln wie `Noch offen fuer Phase ...` dokumentieren
den damaligen Stand. Sie definieren nach Phase 10C keine aktuelle Release-Luecke,
sofern der Punkt in einem spaeteren Dokument entschieden wurde.

Die aktuelle Liste echter offener Punkte steht ausschliesslich in
[`OPEN_POINTS.md`](OPEN_POINTS.md), den verbindlichen Reviewkorrekturen und den
Issues #29 bis #37.

## Verbindliche Release-1-Basis

### Plattform

- ESP32-32E
- 4 MB Flash als harte Planungsbasis
- keine PSRAM-Abhaengigkeit
- Profile `native`, `esp32_bringup`, `esp32_release`
- Single-App-Partitionsplan fuer Release 1 zulaessig
- UART/FT232RL als Update- und Recoveryweg

### Temperaturhardware

- Peltier etwa 12 V / 60 W ueber BTS7960
- Innen- und Aussenluefter
- 7,5-A-Ueberstromsicherung
- einmalige Temperatursicherung
- drei DS18B20:
  - Schrankluft
  - abnehmbares Produkt
  - Kuehlkoerper/Aussenwaermetauscher

Schrankluft- und Kuehlkoerpersensor sind fuer jede Peltierfreigabe erforderlich.
Die einmalige Temperatursicherung muss bereits vor dem ersten realen Peltier-Puls
montiert und geprueft sein; Rating und Montageort bleiben bis zur Inbetriebnahme
offen.

### Prozess

- vier Standardprogramme
- produkt- oder luftgefuehrter Betrieb
- optionales Vorheizen
- Zielqualifikation vor Timerstart
- zeitproportionale PI-Regelung
- Heizen, neutraler Bereich und Kuehlen auch waehrend der Fermentation
- unveraenderlicher Programmschnappschuss
- Zieltemperatur und Restdauer nur als explizite protokollierte Laufanpassung
- phasenbezogener sicherer Wiederanlauf nach Unterbrechung
- unbekannte Ausfallzeit wird nicht als exakter Fortschrittswert erfunden
- spaeter bestimmte Ausfallzeit wird als Unsicherheitsintervall bewertet

### Bedienung

- lokale Touchbedienung 320 x 240 im Querformat
- lokale responsive Weboberflaeche
- Deutsch, Spanisch und Englisch
- kein Cloudzwang
- gemeinsame fachliche Kommandos fuer Display und Web
- PIN-unabhaengiger lokaler Vollreset als Recoveryweg bei vergessener Service-PIN

### Sicherheit

- vier Fehlerklassen
- Quittierung getrennt von Fehlerreset
- persistente Verriegelungen
- `SAFE_BOOT` nach wiederholten abnormalen Neustarts
- Boot bewertet Verriegelungen und Speicherintegritaet vor `STANDBY` oder Recovery
- `SAFE_BOOT` erlaubt keine Aktortests
- keine Wiederherstellung direkter GPIO- oder H-Brueckenzustaende
- Richtungswechsel nur nach Abschaltung und Totzeit
- Sicherheitsabschaltung ueberstimmt Mindest-Einschaltzeit
- kritischer Persistenzfehler verhindert Recovery-Aktorfreigabe
- ein persistiertes `COMPLETED` wird nach Neustart wieder als `COMPLETED` angezeigt

## Bewusst offene Kategorien

### `TBD_HARDWARE`

Betrifft reale Board-, Pin-, Pegel-, Bus-, Stecker- und Moduldetails. Diese Werte
werden nicht aus Datenblaettern aehnlicher Boards geraten.

Nachverfolgung: #29 bis #33.

### `TBD_COMMISSIONING`

Betrifft Temperaturen, Zeiten, PI-Werte, Filter, Nachlaeufe,
Sicherheitsgrenzen, Sensoroffsets und Standardprogrammwerte. Diese Werte werden
am realen Schrank bestimmt.

Nachverfolgung: #34 bis #36.

### `TBD_IMPLEMENTATION_BUDGET`

Betrifft reale Firmwaregroesse, Partitionsaufteilung, Heapreserve,
Puffergroessen, Historienumfang und Exportgrenzen.

Nachverfolgung: #9, #10, #19, #28, #29 und #37.

Kein `TBD` darf als gueltiger produktiver Laufzeitwert akzeptiert werden.

## Zukunftsfunktionen ausserhalb von Release 1

- Web-OTA, duale Slots und automatisches Firmware-Rollback
- benutzeraktivierbare UART-Diagnose
- Produkt-Luft-Kaskadenregelung
- PID-Autotuning
- direkte 12-V-ADC-Messung
- batteriegepufferte RTC als Pflicht
- Tuerkontakt
- Luefter-Tachosignal
- Push- oder Telegram-Benachrichtigungen
- eigener WireGuard-Client
- automatische Wartungserinnerungen

Diese Funktionen duerfen Release 1 nicht durch ungenutzte grosse Bibliotheken,
Partitionen, Puffer oder versteckte Aktorpfade belasten.

## Konsolidierte Korrekturen aus Phase 10C

Folgende Altannahmen wurden ersetzt:

- zwei DS18B20 und Referenzflasche -> drei Sensorrollen mit abnehmbarem
  Produktfuehler und Kuehlkoerpersensor
- Release-1-Web-OTA -> UART/FT232RL, Web-OTA spaeter
- automatische Wiederaufnahme erst nach NTP -> sicherer phasenbezogener
  Wiederanlauf vor/ohne NTP mit spaeterer Korrektur
- keine Laufwertaenderung -> ausdrueckliche validierte Aenderung von Zieltemperatur
  und Restdauer ist erlaubt
- separate Wegwerf-Testfirmware -> gemeinsame Codebasis mit geschuetztem
  `esp32_bringup`
- Hardwarearbeit als Entwicklungsblockade -> Software-first mit getrennten
  `BLOCKED_HARDWARE`-Issues

## Zusaetzliche Korrekturen aus den PR-Reviews

- BOOT wertet Bootschleifen, persistierte Sperren und Speicherintegritaet vor
  jedem Wechsel nach `STANDBY` oder in die Recovery aus.
- Unbekannte Ausfallzeit erlaubt sichere phasenbezogene Regelung, aber keinen
  erfundenen Fortschritt oder automatischen Abschluss.
- NTP-basierte Unterbrechungsdauer wird als Unter-/Obergrenze und nicht als
  scheinbar exakter Wert behandelt.
- `SAFE_BOOT` bleibt aktorfrei; Servicepulse sind nur aus validiertem `STANDBY`
  erreichbar.
- Die einmalige Temperatursicherung ist Voraussetzung des ersten Peltier-Pulses.
- Kritische Persistenzfehler besitzen transaktionale Sperr- und Bootregeln.
- Der Vollreset bei vergessener Service-PIN besitzt einen PIN-unabhaengigen
  lokalen Ausloeseweg.
- `COMPLETED` wird beim Boot explizit wiederhergestellt.
- Zustandsnamen und die Begriffe Mindest-Einschaltzeit/Mindest-Ausschaltzeit sind
  vereinheitlicht.

Details und Abnahmetests stehen in `PR38_REVIEW_CORRECTIONS.md`.

## Verzeichnis- und Linkkonsolidierung

Verbindliche Dateinamen:

- `LOCAL_UI_PROGRAMS.md`
- `ACTUATOR_TIMING.md`

Aeltere Verweise auf `LOCAL_PROGRAMS.md` oder
`ACTUATOR_TIMING_AND_FANS.md` sind nicht verbindlich. Ein eventueller
Kompatibilitaetshinweis darf nur auf den kanonischen Dateinamen verweisen.

## Implementierungsfreigabe

Nach Merge des Spezifikations-PRs:

1. Issue #1 wird durch den PR geschlossen.
2. Issue #9 wird von `PLANNED_SPEC_PENDING` auf `READY` gesetzt.
3. Branch `foundation/platformio-profiles` wird von aktuellem `main` erstellt.
4. Implementierung erfolgt nur im Scope von #9.
5. Nachfolgende Issues werden gemaess `IMPLEMENTATION_ISSUES.md` freigegeben.

Hardware-Issues bleiben bis zum realen Aufbau `BLOCKED_HARDWARE`.
Inbetriebnahme-Issues bleiben bis zur thermischen Verifikation
`TBD_COMMISSIONING`.

## Review-Gate

Die Spezifikation gilt als reviewbereit, wenn:

- [x] zentrale Alt-Dokumente konsolidiert sind
- [x] drei Sensorrollen durchgaengig als Release-1-Basis festgelegt sind
- [x] UART-Update und fehlende Release-1-OTA-Reserve festgelegt sind
- [x] software-first Architektur und Bring-up-Profil festgelegt sind
- [x] offene Punkte nach Typ und Issue nachverfolgt sind
- [x] Akzeptanztests und Release-Gates dokumentiert sind
- [x] Epics und Arbeits-Issues erstellt sind
- [x] Draft-PR #38 erstellt ist
- [x] beide PR-Reviews ausgewertet und verbindliche Korrekturen dokumentiert
- [x] alle Inline-Reviewthreads beantwortet und geschlossen
- [ ] PR-Review durch den Repository-Owner abgeschlossen
- [ ] PR #38 nach `main` gemergt
