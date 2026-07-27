# Release-1-Scope und Adopt-or-build-Audit

## Anlass und Ziel

Dieser Audit konsolidiert den verbindlichen Release-1-Umfang mit einer
technisch und lizenzseitig nachvollziehbaren Adopt-or-build-Bewertung. Er soll
verhindern, dass Standardtreiber und Frameworkdienste unnoetig neu entwickelt
werden, ohne den projektspezifischen Safety- und Fachkern an externe
Bibliotheken abzugeben.

Der Audit implementiert keine Empfehlung. Er aendert weder Produktionscode,
Tests, Abhaengigkeiten, Buildflags, Hardwarekonfigurationen, bestehende Issues,
Issue-Status noch akzeptierte ADRs.

## Gepruefter Stand

| Gegenstand | Stand |
|---|---|
| Repository | `ManuEngineer/ESP32-Fermentationsschrank` |
| Basisbranch | `main` |
| Basis-Commit | `7713a66cbf51eb078bd0f5e43c1163d1e0f47e1f` |
| Abruf-/Auditdatum | 2026-07-27 |
| Toolchain im Repository | PlatformIO `espressif32@7.0.1`, Arduino-ESP32 `2.0.17` (`dcc1105b`), C++17 |
| Zielbasis | ESP32-32E, 4 MB Flash, keine PSRAM-Abhaengigkeit |
| offene Implementierungs-/Tracking-Issues | #16–#37 sowie #56/#57: 24 Eintraege |
| Konfigurationsstand | #54 und #55 gemergt; #16 bleibt Tracking; #56/#57 `BLOCKED_DEPENDENCY` |
| Audit-Issue | #62 |

Geprueft wurden die Live-Issues, die Dokumentationsprioritaet und akzeptierten
ADRs, die Release-/Safety-/Hardware-/Persistenz-/UI-/Netzwerkspezifikation, der
offene Backlog, die aktuelle Quell- und Teststruktur, die fixierte Toolchain und
die vorhandenen Hardwarequellen unter `references/`.

Die Bedien- und Sensorrollengrenze folgt dem verbindlichen Ownerentscheid aus
dem Auditreview von PR #63, Kommentar `5088383783`.

## Methodik

1. Verbindliche Quellen wurden in der Reihenfolge aus
   `SPECIFICATION_REVIEW.md` gelesen. Historische "noch offen"-Abschnitte
   ersetzen keine spaeter akzeptierte Entscheidung.
2. Fuer jede Release-1-Funktion wurde erfasst, was bereits im Repository
   vorhanden ist, was projektspezifisch bleibt und wo eine vorhandene Loesung
   adoptiert oder adaptiert werden kann.
3. Jedes offene Implementierungsissue wurde genau einer Primaerkategorie und
   gegebenenfalls einer Sekundaerkategorie zugeordnet.
4. Bibliotheksbewertungen verwenden offizielle Projekt-/Herstellerquellen,
   konkrete Versionen oder Commits, Lizenzquellen und das Abrufdatum.
5. Hersteller-/Projektbehauptungen, Repositorybefunde und noch ausstehende
   Hardwaremessungen werden getrennt. README- oder Marketingaussagen gelten
   nicht als reale Hardwarebestaetigung.
6. Hardwarekandidaten erhalten identische Spikeplaene, bevor ein Produktivstack
   gewaehlt wird.

Es wurden keine Bibliotheken installiert, keine Spikes ausgefuehrt und keine
Ressourcenwerte fuer noch nicht eingebundene Komponenten erfunden.

## Executive Summary

Die akzeptierte Release-1-Grenze ist grundsaetzlich stimmig: Das Geraet muss
lokal und ohne Netzwerk sicher fermentieren. Das Touchdisplay ist die einzige
lokale Bedien- und Anzeigeoberflaeche; die Weboberflaeche ist sekundaer. Der
230-V-AC-Hauptschalter schaltet das Geraet rein elektrisch und ist kein
Firmwareeingang. Der Summer ist das einzige zusaetzliche lokale Ausgabeelement.
Release 1 benoetigt ausserdem begrenzte Persistenz/Recovery, Diagnose,
secret-freies Backup/Import und UART-Recovery. OTA, Bluetooth, Cloud/Push,
PID-Autotuning, Kaskadenregelung und PSRAM-Abhaengigkeit bleiben ausserhalb.

Die Plattform muss nicht Display-, 1-Wire-, JSON-, HTTP-, WLAN-, NVS-, Zeit-
oder UART-Grundfunktionen neu entwickeln. Diese Aufgaben sollen aus der
fixierten Plattform oder aus konkret geprueften Bibliotheken uebernommen und
hinter schmalen Adaptern gekapselt werden. Dagegen bleiben Sensorqualitaet,
Regelsensorauswahl, PI/Luftbegrenzung, Aktorplanung, Safety, Prozess-, Recovery-
und Berechtigungslogik eigene deterministische Module.

Zwei Hardwareentscheidungen duerfen nicht am Schreibtisch fallen:

- Display/Touch: LovyanGFX, TFT_eSPI und das LCDWiki-Paket werden identisch am
  realen MSP2807 verglichen.
- DS18B20: DallasTemperature+OneWire und die Espressif-Komponenten werden
  identisch verglichen; der Espressif-Stack muss zuerst seine Kompatibilitaet
  mit der aktuellen Arduino-ESP32-2.0.17-Toolchain beweisen.

Die wichtigste offene Ownerentscheidung betrifft #16/#56/#57. #54/#55 liefern
bereits eine starke Persistenzbasis. Der noch spezifizierte Manifest-, Root-,
Pending-, Intent-, Secret- und End-to-End-Recoverygraph ist fuer Release 1
umfangreich und blockiert ueber #16 potenziell Laufpersistenz und Safety. Vor
Freigabe von #56 muss der Owner entweder den vollen Vertrag ausdruecklich
bestaetigen oder ihn in einem separaten, ADR-konformen Prozess auf das kleinste
sichere R1-Modell reduzieren. Dieser Audit aendert die bestehende Entscheidung
nicht.

## Wichtigste Erkenntnisse

### 1. Adoptieren unter schmaler Kontrolle

- NVS/Preferences ist durch ADR-016 bereits als produktives Backend festgelegt;
  eine eigene Flashdatenbank waere unnoetige Risiko- und Wartungslast.
- Display-, Touch- und 1-Wire-Protokolltreiber sollen adoptiert, nicht neu
  geschrieben werden.
- ArduinoJson ist der bevorzugte Kandidat fuer begrenzte JSON-Grenzen; interne
  kritische Persistenz bleibt beim vorhandenen binaeren Wireformat.
- Framework-WLAN, Zeit/NTP, mDNS/DNS, GPIO und UART werden konfiguriert und
  adaptiert.
- Beim Webserver ist der kleinere Frameworkserver die Baseline; ein
  asynchroner Server muss seinen Mehrwert und seine Ressourcen-/Lizenzwirkung
  im identischen Prototyp beweisen.

### 2. Safety und Fachlogik nicht delegieren

Treiberbibliotheken duerfen Sensorbytes, Pixel oder HTTP-Verbindungen
verarbeiten. Sie entscheiden nicht ueber Rollenbindung, Ersatzbetrieb,
Heiz-/Kuehlfreigabe, Totzeit, Fehlerreset, Recovery oder Authberechtigung. Der
optionale, verwendbare Produktfuehler ist der primaere Regelsensor; ohne ihn
uebernimmt der Raum-/Luftsensor regulaer die Regelung, sofern der Lauf dies
zulaesst. Der Kuehlkoerper-/Peltier-Schutzsensor ist fuer jede
Peltierfreigabe verpflichtend. Die Issues #20–#24 bleiben deshalb
`CUSTOM_SAFETY_CORE`; #17–#19, #25–#28 und der fachliche Teil von #56/#57
bleiben `CUSTOM_APPLICATION`.

### 3. Toolchainkompatibilitaet ist ein eigener Nachweis

Das Projekt verwendet Arduino-ESP32 2.0.17 auf der fixierten
PlatformIO-Plattform. Ein aktueller Upstream-Release ist nicht automatisch
kompatibel. Besonders die offiziellen Espressif-`onewire_bus`-/`ds18b20`-
Komponenten zielen auf ESP-IDF >=5.0, waehrend die aktuelle Arduinobasis auf
einer aelteren IDF-Generation beruht. Ein Toolchainwechsel gehoert nicht in
einen Treiber-PR.

### 4. Breite Issues gefaehrden kleine pruefbare PRs

#19 vereint Journal, Retention, Backup und Import. #25–#28 vereinen jeweils
mehrere UI-, Web-, Auth-, Diagnose- und Serviceverantwortungen. Der fachliche
R1-Umfang bleibt, aber die Umsetzung sollte innerhalb der Issues in kleine
vertikale PRs oder nach Ownerfreigabe in kleinere Folgeissues zerlegt werden.

### 5. Hardwarequellen sind keine Hardwarebestaetigung

Das lokale LCDWiki-Paket enthaelt MIT-Lizenzdateien fuer seine drei
Bibliotheksordner. Weil jedoch kein eindeutig versioniertes Upstream-Repository
und keine paketweite Herkunftsmatrix vorliegt, bleibt eine konkrete
Publikationspruefung erforderlich. Lieferantenunterlagen bleiben
`confirmed_order`; Pins, Pegel, Controller und Verdrahtung werden weiterhin
real gemessen.

## Bestaetigte Release-1-Grenze

### Zwingend

- drei DS18B20-Rollen: der optionale Produktfuehler ist primaerer Regelsensor,
  wenn er vorhanden und verwendbar ist; der Raum-/Luftsensor ist der regulaere
  Ersatz-Regelsensor; der Kuehlkoerper-/Peltier-Schutzsensor ist verpflichtende
  Sicherheitsgrundlage fuer jede Peltierfreigabe;
- Peltier ueber BTS7960, Innen-/Aussenluefter und Summer;
- deterministischer Safety-, PI-, Aktor-, Prozess- und Laufkern;
- Touchdisplay 320 x 240 als einzige lokale Bedien- und Anzeigeoberflaeche,
  drei Sprachen und eine sekundaere Weboberflaeche;
- WLAN-Onboarding mit lokalem Fallback, lokale Zeit/NTP und Zeitzonenanzeige;
- versionierte Konfiguration, Laufpersistenz, Recovery und begrenzte Journale;
- lokale Authentication/Secrets, Diagnose, Exporte und secret-freies
  Backup/Import;
- UART/FT232RL als Update- und letzter Recoveryweg;
- Hardware-, thermische und siebentaegige Releaseabnahme.

### Nicht Release 1

- Web-OTA, duale OTA-Slots und automatischer Firmwaredownload;
- Bluetooth/BLE als Produktfunktion;
- Cloud-, Push- oder Telegram-Pflicht;
- PID-Autotuning und aktive Kaskadenregelung;
- LVGL ohne belegten R1-Vorteil;
- Tuerkontakt, verpflichtende RTC, 12-V-ADC, Lueftertacho;
- vorsorgliche grosse Puffer, Ports oder Bibliotheken fuer spaetere Funktionen.

### Nicht Bestandteil dieses Projekts

Encoder, Programmwahlschalter, Start-/Stop-Taster und Status-LED gehoeren
verbindlich nicht zu diesem Fermentationsprojekt (`NOT_PART_OF_THIS_PROJECT`).
Dafuer entstehen weder Ports noch GPIO-Zuordnungen, Adapter oder vorsorgliche
Interfaces. Der 230-V-AC-Hauptschalter ist ebenfalls kein Firmwareeingang; er
schaltet das gesamte Geraet elektrisch ein oder aus. Der Summer bleibt das
einzige zusaetzliche lokale Ausgabeelement fuer akustische Warnungen und
Hinweise. Diese Festlegung ist kein Aufschub auf eine spaetere Release.

Die vollstaendige Zuordnung steht in der
[`Release-1-Funktionsmatrix`](RELEASE_1_FUNCTION_MATRIX.md).

## Zentrale Empfehlungen

1. [`ADOPT_OR_BUILD.md`](../ADOPT_OR_BUILD.md) nach Ownerreview als dauerhaften
   Grundsatz uebernehmen.
2. #20–#23 als naechste unabhaengige Safety-Kette priorisieren.
3. #17 von nicht benoetigten Komfortteilen des Tracking-Issues #16 entkoppeln,
   sofern eine separate Ownerpruefung dies bestaetigt.
4. OD-01 vor jeder Freigabe von #56 entscheiden; #57 bleibt danach strikt
   abhaengig.
5. #29 als sichere Hardwarebasis ausfuehren, danach die zwei aktorfreien Spikes.
6. Treiber erst nach identischen Messungen fixieren und in kleinen
   Adapter-PRs einbinden.
7. Web-/UI-/Backupissues vor Umsetzung in kleine, ressourcenmessbare Scheiben
   schneiden.
8. Jede spaetere Drittkomponente mit Version, Lizenz, Abhaengigkeiten,
   Base-/Head-Messung und Hardwarestatus im Komponentenregister nachfuehren.

## Erkannte Ueberdimensionierungen

| Bereich | Befund | Empfohlene Korrektur ausserhalb dieses Audits |
|---|---|---|
| #56/#57 | viele Slots, Roots, Pending-/Intentpfade, zwei Secret-Domaenen und vollstaendige Cut-Matrix vor dem ersten Produktivbackend | Owner bestaetigt Nutzen oder reduziert per separatem Entscheid auf kleinstes sicheres R1-Modell |
| #19 | vier grosse Verantwortungsbereiche in einem Issue | in Journal/Retention, Export und Backup/Import schneiden |
| #25–#28 | UI-, Web-, Auth-, Diagnose- und Servicepakete sind zu breit fuer kleine PRs | nach stabilen DTO-/Portgrenzen in vertikale Scheiben teilen |
| LVGL | vollstaendiges UI-Framework fuer wenige feste 320-x-240-Screens waere vorsorglich | schlanke Views als Baseline, LVGL nur nach Messnachweis |
| ESPAsyncWebServer | Async-/WebSocket-/SSE-Umfang koennte groesser als der reale R1-Bedarf sein | Frameworkserver zuerst messen; Async nur bei belegtem Vorteil |
| Espressif Provisioning/BLE | umfangreicher Provisioningstack wuerde BLE und Toolchainkomplexitaet einbringen | SoftAP/Captive-Portal-Anforderung mit kleinstem Adapter erfuellen |
| PID-Bibliotheken | allgemeine PID-/Autotune-Funktionen passen nicht zum spezifizierten begrenzten PI-/Safety-Vertrag | kleinen deterministischen PI-Kern selbst implementieren |

## Auswirkungen auf #16, #56 und #57

- **#16:** bleibt unveraendert offen und `TRACKING`. #54/#55 werden als
  vorhandene Basis wiederverwendet. Der Audit empfiehlt nur eine
  Abhaengigkeits- und Umfangspruefung; er schliesst oder editiert #16 nicht.
- **#56:** bleibt `BLOCKED_DEPENDENCY`. Keine Manifest-, Root-, Pending-,
  Preview- oder Runtimeaktivierungslogik wurde implementiert. Freigabe erst nach
  OD-01.
- **#57:** bleibt `BLOCKED_DEPENDENCY`. Keine Bootstrap-, Secret-, Reset- oder
  Recoverylogik wurde implementiert. Freigabe erst nach #56 und OD-01/OD-09.

Die vorgeschlagene Reihenfolge steht in
[`PROPOSED_RELEASE_1_ROADMAP.md`](PROPOSED_RELEASE_1_ROADMAP.md).

## Offene Ownerentscheidungen

| ID | Entscheidung |
|---|---|
| OD-01 | vollen #56/#57-Vertrag fuer R1 bestaetigen oder separat auf ein kleineres sicheres Modell reduzieren; daraus #17/#24-Abhaengigkeiten klaeren |
| OD-02 | Display-/Touchstack nach dem identischen Hardware-Spike waehlen |
| OD-03 | DS18B20-/1-Wire-Stack nach Toolchain- und Hardware-Spike waehlen |
| OD-04 | Arduino-Framework-Webserver oder ESPAsyncWebServer nach identischem Last-/Ressourcentest |
| OD-05 | schlanke eigene Screens oder LVGL nach representativem Screen-/Ressourcenvergleich |
| OD-06 | WiFiManager oder kleiner Framework-Onboardingadapter |
| OD-07 | R1-Mindestumfang und PR-Schnitt von #19 und #25–#28 |
| OD-09 | KDF-, Work-Factor-, Sitzungs-, CSRF-, Sperr- und Secret-at-rest-Vertrag vor #27 |

Hardwarewerte, Pins, Pegel und thermische Parameter sind keine freien
Ownerpraeferenzen; sie bleiben Mess- und Gateentscheidungen in #29–#37.

## Detaildokumente

- [`RELEASE_1_FUNCTION_MATRIX.md`](RELEASE_1_FUNCTION_MATRIX.md) – jede
  Release-1-Funktion und ihre Behandlung
- [`OPEN_BACKLOG_CLASSIFICATION.md`](OPEN_BACKLOG_CLASSIFICATION.md) – alle 24
  offenen Implementierungs-/Tracking-Issues
- [`COMPONENT_EVALUATIONS.md`](COMPONENT_EVALUATIONS.md) – technische
  Kandidaten, Versionen, Adapter und Risiken
- [`THIRD_PARTY_SOURCE_AND_LICENSE_REVIEW.md`](THIRD_PARTY_SOURCE_AND_LICENSE_REVIEW.md)
  – Herkunft, Lizenzen und Publikationspruefung
- [`HARDWARE_SPIKE_PLAN.md`](HARDWARE_SPIKE_PLAN.md) – identische Display-/Touch-
  und DS18B20-Spikes
- [`PROPOSED_RELEASE_1_ROADMAP.md`](PROPOSED_RELEASE_1_ROADMAP.md) – Reihenfolge,
  Gates, kleine PRs und spaetere Funktionen
- [`../ADOPT_OR_BUILD.md`](../ADOPT_OR_BUILD.md) – Entwurf des dauerhaften
  Entwicklungsgrundsatzes
- [`../THIRD_PARTY_COMPONENTS.md`](../THIRD_PARTY_COMPONENTS.md) – Entwurf des
  dauerhaften Komponentenregisters

## Auditabschlusskriterien

- alle Dokumente sind gegenseitig verlinkt;
- Funktionsmatrix, Backlogklassifikation und Roadmap verwenden dieselbe
  Release-1-Grenze;
- jede externe Bewertung nennt Quelle, Stand, Lizenzstatus und unbestaetigte
  Hardwaregrenzen;
- keine Empfehlung ist implementiert;
- #16, #56, #57 und PR #61 bleiben unveraendert;
- der Audit-PR enthaelt ausschliesslich die neun Markdown-Dokumente.
