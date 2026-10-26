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
- `esp32_bringup` und `esp32_release` als ESP-IDF-6.1-Produktionsprofile;
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
- #124-Current-`FERMENTING`-Recovery mit exakter Evidenz und trusted UTC als
  automatische logische Fortsetzung ohne Aktorfreigabe;
- bei fehlender trusted UTC `RecoveryEvaluation/WaitingForTrustedTime`
  RAM-only und ohne Persistenzmutation; kein erfundener Fortschritt, keine
  automatische Fallback-Promotion oder Charge-Recovery.

### Bedienung und Konnektivitaet

- lokale Touchbedienung mit 320 x 240 Pixel im Querformat;
- lokale responsive Weboberflaeche;
- Deutsch, Spanisch und Englisch;
- Betrieb ohne Cloud, Internet, Heimserver oder funktionierendes WLAN;
- generische Zeitplattform RTC-optional und NTP-only-fähig; konkretes
  Fermenter-R1-Profil mit lokaler DS3231-Familien-RTC für neue Offline-Läufe,
  deren tatsächliche Variante `TBD_HARDWARE_CONFIRMATION` bleibt;
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

## R1/R2-Zuschnitt nach YAGNI

R1 konzentriert sich auf die sichere, bedienbare und real qualifizierte
Fermenter-Kernfunktion. Vier Standardprogramme, lokaler Programmeditor,
lokaler manueller Betrieb, Touch-Grundbedienung, vorhandene Webbedienung
(einschliesslich Start/Stop) und DE/EN/ES bleiben R1. Bestehende sichere
Implementierungen werden nicht entfernt; technische Laufgrenzen und reale
Freigaben bleiben bei #34/#35 und den jeweiligen Hardware-/Safety-Ownern.

- `R2_VORGEMERKT`: Web-Programmeditor und zusaetzliche Weboberflaeche fuer
  manuellen Betrieb (#27); vollstaendige technische Webdiagnose und neue
  Detail-APIs, aktueller Web-Laufchart und historische Diagramme (#28);
  persistentes Ereignis-/Fehler-/Resetjournal, Lauf-/Messhistorie mit
  Bereinigung/Archiv und Lauf-, Diagnose- und Serviceberichtsexporte (#19/#28);
  portables Backup und atomarer Import (#19). B0/O-BI ist kein R1-Gate mehr.
- `R2_KANDIDAT`: zusaetzliches Boot-Selbsttest-Dashboard/-modell und gefuehrte
  Service-UI/Mock-Aktor-Sequenz (#28), nur bei konkretem Zusatznutzen; keine
  zweite Boot-/Safetyentscheidung und keine Ersatzqualifikation fuer Hardware.
- `FUTURE_BEI_BEDARF`: Web-Favicon (#204), keine R1-Releasepflicht.

`R2_VORGEMERKT` benennt eine spaetere Bewertung, keine automatische R2-Pflicht.
`R2_KANDIDAT` und `FUTURE_BEI_BEDARF` enthalten keine Release-Zusage. Es werden
keine Schnittstellen, Schemas oder Ressourcenreserven vorgebaut. Die historische
5-Detail-/50-Zusammenfassungs-Aufbewahrung bleibt eine bei Wiederaufnahme erneut
zu pruefende Referenz. Die Sammlung in [FUTURE_SCOPE](FUTURE_SCOPE.md) ist
nicht normativ; die Releasegrenze steht ausschliesslich hier, Detailvertraege
bleiben bei ihren Fach-SSOTs und bestehenden Issues.

Die Journal-Start-/Safety-/Recoverygrenze wird ausschliesslich durch den
konkret freigegebenen K1-Normentscheid und den Fachvertrag
[SYSTEM_SAFETY_AND_RECOVERY](SYSTEM_SAFETY_AND_RECOVERY.md) bestimmt; ein
fehlendes Journal wird weder als verlaesslich noch als Safety-Nachweis behauptet.

Boot-/Config-/Run-/Sensor-/Planner-/Safety-Revalidierung, SAFE_BOOT,
Write-before-Apply, kritische Persistenz und Watchdog bleiben Pflicht. Reale
Hardware-Service-/Peltier-/Thermikpruefungen und mindestens sieben Tage
Abnahme bleiben Pflicht (#32/#33/#34/#35/#36/#37), ebenso echte Web-/Touch-/
WLAN-Last und RAM-/Stack-/Flash-Reserven. Ein zusaetzliches Dashboard oder
Exportwerkzeug ist dafuer keine Voraussetzung. Lizenz/Distribution muss vor
offizieller R1-Weitergabe entschieden sein (#177, [LICENSE_STATUS](LICENSE_STATUS.md)).

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
