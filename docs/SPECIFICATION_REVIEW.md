# Release-1-Spezifikationsreview

## Zweck

Dieses Dokument ist die kanonische Quelle fuer:

- die Reihenfolge bei Dokumentationswiderspruechen;
- die verbindliche Release-1-Basis;
- die vollstaendige Abgrenzung zu spaeteren Funktionen;
- die Bedeutung offener Hardware-, Inbetriebnahme- und Ressourcenwerte.

Fachliche Details stehen in den jeweils spezialisierten Dokumenten und
akzeptierten ADRs. Dieses Review kopiert deren vollstaendige Vertraege nicht.

## Dokumentationsprioritaet

Bei einem scheinbaren Widerspruch gilt:

1. spaeter akzeptierte ADRs im Register `DECISIONS.md`;
2. dieses Dokument fuer Release-Scope, Quellenrollen und TBD-Kategorien;
3. der thematisch zustaendige spezialisierte Fachvertrag;
4. `REQUIREMENTS.md`, `ARCHITECTURE.md` und `HARDWARE.md`;
5. Beispielkonfigurationen;
6. klar historische Audit-, Plan- und Revisionsdokumente.

Bestehender Code und Tests belegen den implementierten Stand, duerfen aber eine
dokumentierte Produkt-, Safety- oder Architekturentscheidung nicht still
ersetzen. Ein Live-Issue und ein freigegebener Plan definieren den konkreten
Arbeitsscope; materielle neue Entscheidungen werden in der zustaendigen
kanonischen Quelle oder einem akzeptierten ADR festgehalten.

Aktueller Projekt- und Arbeitsstatus steht ausschliesslich in `ROADMAP.md`.

## Verbindliche Release-1-Basis

### Plattform

- ESP32-32E mit 4 MB Flash;
- keine PSRAM-Abhaengigkeit;
- `native` als PlatformIO-Hosttestprofil;
- `esp32_bringup` und `esp32_release` als ESP-IDF-6.0.2-Produktionsprofile;
- UART/FT232RL als Update- und Recoveryweg;
- keine OTA-Partitionen oder dualen Firmware-Slots.

### Temperatur- und Aktorhardware

- Peltier mit etwa 12 V / 60 W ueber BTS7960;
- Innen- und Aussenluefter;
- 7,5-A-Ueberstromsicherung;
- einmalige Temperatursicherung als firmwareunabhaengige Rueckfallebene;
- drei DS18B20-Rollen:
  - Schrankluft;
  - abnehmbarer Produktfuehler;
  - Kuehlkoerper beziehungsweise Aussenwaermetauscher.

Schrankluft- und Kuehlkoerpersensor sind fuer jede Peltierfreigabe
erforderlich. Unbestaetigte GPIOs, Pegel, Controller, Verdrahtung und
Grenzwerte werden nicht geraten.

### Prozess und Regelung

- vier allgemeine Standardprogramme;
- dynamisch erweiterbare Benutzerprogramme;
- produkt- oder luftgefuehrter Betrieb;
- optionales Vorheizen;
- Zielqualifikation vor Timerstart;
- zeitproportionale PI-Regelung fuer Heizen, neutralen Bereich und Kuehlen;
- unveraenderlicher Programmschnappschuss je Lauf;
- Zieltemperatur und Restdauer nur als ausdrueckliche, validierte und
  protokollierte Laufanpassung;
- phasenbezogener sicherer Wiederanlauf nach Unterbrechung;
- kein erfundener Fortschritt bei unbekannter Ausfallzeit;
- spaeter bestimmte Ausfallzeit als Unsicherheitsintervall.

### Bedienung und Konnektivitaet

- lokale Touchbedienung mit 320 x 240 Pixel im Querformat;
- lokale responsive Weboberflaeche;
- Deutsch, Spanisch und Englisch;
- Betrieb ohne Cloud, Internet, Heimserver oder funktionierendes WLAN;
- gemeinsame fachliche Kommandos fuer Touch und Web;
- PIN-unabhaengiger lokaler Vollreset als Recoveryweg bei vergessener
  Service-PIN.

Netzwerk, Web und Anzeige sind keine Voraussetzung fuer Regelung oder Safety.

### Persistenz und Recovery

- atomare, versionierte Konfiguration mit gueltiger Rueckfallgeneration;
- atomare Laufkontrollpunkte und klarer Recoveryzustand;
- kein Wiederherstellen direkter GPIO- oder H-Brueckenzustaende;
- ein Neustart ist kein Fehlerreset;
- kritische Persistenz- oder Integritaetsfehler verhindern normale
  Aktorfreigabe und Lauf-Recovery;
- ein unbestimmter oder unvollstaendiger kritischer Commit bleibt fail-closed;
- ein persistiertes `COMPLETED` wird nach Neustart wieder als `COMPLETED`
  angezeigt.

### Safety

- Peltier und beide Richtungen bleiben bei Boot, Reset, Fehler und unbekanntem
  Zustand AUS;
- Heizen und Kuehlen sind nie gleichzeitig freigegeben;
- Richtungswechsel erzwingt Abschaltung und Totzeit;
- Sicherheitsabschaltungen ueberstimmen Mindestlaufzeiten;
- Quittierung und Fehlerreset bleiben getrennt;
- aktuelle, rein fluechtige Safety-Latches muessen einen Neustart nicht
  ueberleben; jeder Neustart startet all-off und erzwingt die vollstaendige
  Neubewertung von Konfiguration, Persistenz, Sensorik und Safety-Evidenz;
- es gibt keine Restart-Akkumulation und kein Restart-Zeitfenster;
  `SAFE_BOOT` entsteht aus aktuell untrusted Boot-, Konfigurations- oder
  Persistenzevidenz, nicht aus einer vorsorglichen Neustartzaehlung;
- `SAFE_BOOT` erlaubt keine leistungsbezogenen Aktortests;
- firmwarefeste Grenzen koennen durch Konfiguration nur verschaerft werden;
- der erste reale Peltier-Puls verlangt die dokumentierten elektrischen,
  thermischen und sensorischen Vorbedingungen.

Die vollstaendigen Regeln stehen in den spezialisierten Safety-, Recovery-,
Persistenz- und Akzeptanzdokumenten.

## Offene Kategorien

### `TBD_HARDWARE`

Reale Board-, GPIO-, Pegel-, Bus-, Stecker-, Controller- oder Moduldaten fehlen.
Sie werden nur durch Datenblatt der exakten Komponente und praktische
Verifikation geschlossen.

### `TBD_COMMISSIONING`

Thermische, regelungstechnische oder prozessbezogene Werte werden am realen
Schrank bestimmt, beispielsweise PI-Parameter, Grenzwerte, Nachlaufzeiten,
Sensoroffsets und Standardprogrammwerte.

### `TBD_IMPLEMENTATION_BUDGET`

Reale Firmwaregroesse, Partitionen, Heapreserve, Puffer, Historien- und
Exportgrenzen benoetigen reproduzierbare Builds und Belastungsmessungen.

Kein TBD-Wert darf als gueltiger produktiver Laufzeitwert oder bestaetigte
Tatsache verwendet werden. Die nachweisgebundene Checkliste steht in
`OPEN_POINTS.md`.

## Zukunftsfunktionen ausserhalb von Release 1

- Web-OTA, duale OTA-Slots, automatischer Firmwaredownload und automatisches
  Firmware-Rollback;
- benutzeraktivierbare UART-Diagnose;
- Produkt-Luft-Kaskadenregelung;
- PID-Autotuning;
- direkte 12-V-ADC-Messung;
- batteriegepufferte RTC als Pflicht;
- Tuerkontakt;
- Luefter-Tachosignal;
- externe Strommessung sowie R_IS/L_IS sind ausserhalb von R1; R_IS/L_IS
  bleiben `FUTURE_RELEASE` und benoetigen fuer eine spaetere Nutzung ein
  eigenes Issue, einen vollstaendigen Plan und ein eigenes Owner-Gate;
- Push- oder Telegram-Benachrichtigungen;
- eigener WireGuard-Client;
- automatische Wartungserinnerungen.

OTA ist fuer ein spaeteres Release ausdruecklich vorgesehen. Release 1 baut
dafuer jedoch keine Bibliotheken, dualen Slots, Speicherreserven, Puffer oder
versteckten Updatepfade vor.

Zukunftsfunktionen duerfen erst mit eigenem Scope, Ressourcenbudget,
Safety-/Securityanalyse, Testvertrag und Ownerfreigabe umgesetzt werden.

## Dauerhaft nicht vorgesehene lokale Bedienelemente

Encoder, Programmwahlschalter, Start-/Stop-Taster und Status-LED gehoeren nicht
zum Projekt. Das Touchdisplay bleibt die lokale Bedienoberflaeche; der
230-V-Hauptschalter ist kein Firmwareeingang.
