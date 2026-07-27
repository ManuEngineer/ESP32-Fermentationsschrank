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
dem Auditreview von PR #63, Kommentar `5088383783`. Der verbindliche
Persistenzentscheid OD-01 folgt Kommentar `5088636861`.

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

Diese aktorfreien Spikes warten nicht auf den vollstaendigen Abschluss von
#20–#24. Nach Audit-/Planungsbereinigung und einer minimalen sicheren
ESP32-Hardwarebaseline laufen sie parallel zur hardwareunabhaengigen
Safety-Kette. Produktive Aktoren bleiben bis zu ihren Safety-Gates gesperrt.

OD-01 ist entschieden: Release 1 verwendet das schlanke, stromausfallsichere
Modell der Variante B. #56 und #57 duerfen nicht unveraendert umgesetzt werden.
Vor ihrer Implementierung muessen Spezifikation, Issues und gegebenenfalls ADRs
in einem separaten ownerfreigegebenen Planungs-/ADR-Schritt auf den hier
festgehaltenen R1-Vertrag zugeschnitten werden. Dieser Audit selbst aendert
keine dieser Quellen. Variante A bleibt als spaetere additive Erweiterung des
stabilen Active-/Fallback-Kerns offen, nicht als alternativer R1-Auftrag.

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

### 6. Minimale Hardwarebaseline ermoeglicht fruehe aktorfreie Spikes

Vor Display-/Touch- und DS18B20-/1-Wire-Spikes wird nur der minimale sichere
Anteil von #29 benoetigt: reale Boardrevision, UART/FT232RL, reproduzierbares
Flashen/Booten/Resetten, reale Flashgroesse und Versorgung, fixierte Toolchain,
Betrieb ohne PSRAM, Firmware-/RAM-/Heapbaseline sowie GPIO-/Businventar. Peltier,
BTS7960, Innen-/Aussenluefter, MOSFET-Verbraucher und Summer bleiben physisch
getrennt oder nachweislich inaktiv; der Summer wird nicht angesteuert.

Die Baseline bestimmt keine finalen Pins, Partitionierung, Bibliotheken,
Sensorbustopologie, Aktoradapter, Safety-Grenzen oder PI-Parameter. Jeder
Kandidat durchlaeuft danach drei Gates: Quellen-/Lizenz-/Kompatibilitaetsvertrag,
isolierter reproduzierbarer Build mit der fixierten Toolchain ohne Aktoren und
erst dann der identische reale Hardwaretest. Ein Gate-2-Scheitern wird typisiert
dokumentiert und nicht durch einen Toolchainwechsel oder verfruehten
Hardwaretest umgangen.

Parallel laufen #20 Sensorqualitaet, #21 Regelsensorauswahl, #22 PI/
Luftbegrenzung, #23 Aktorplaner und #24 Fehlerkern/`SAFE_BOOT`. Bibliothekstypen,
reale GPIOs und erfundene Messwerte gelangen nicht in den Safety-/Fachkern;
Treiber- und Fachstatus bleiben getrennt. Der Audit empfiehlt eine spaetere
Aufteilung von #29 in minimalen Baseline- und produktiven Hardwareanteil, aendert
das Issue aber nicht.

## Entschiedener Persistenzvertrag OD-01

### Variante B: verbindlicher Release-1-Kern

Release 1 enthaelt:

- ein vollstaendig validiertes `ActiveConfigurationManifest`;
- einen atomar umgeschalteten kanonischen Root als persistenten
  Linearisierungspunkt;
- genau eine vorherige vollstaendig nutzbare Fallbackgeneration;
- sichere Slotrotation fuer Active, Fallback und die laufende Mutation;
- vollstaendige technische und fachliche Graphvalidierung;
- Validierung und Vorbereitung aller falliblen Runtimewerte und Ressourcen vor
  dem Root-Commit;
- einen unveraenderlichen vorbereiteten Runtime-Snapshot;
- Sichtbarkeit ausschliesslich der vollstaendig alten oder vollstaendig neuen
  Runtimegeneration, ohne Teilaktivierung;
- typisierte Fehler ohne stille Ruecksetzung oder Teilwirkung;
- Uebergabe eines Publish-Vertragsfehlers an den spaeteren Safety-/Fehlerkern.

Der wiederverwendbare Transaktionsablauf bleibt klar getrennt:

1. Kandidat erzeugen;
2. technisch und fachlich validieren;
3. Runtimewerte und Ressourcen vorbereiten;
4. persistent committen;
5. Runtime veroeffentlichen.

### Fluechtige Vorschau und Konfliktschutz

Eine R1-Vorschau darf fluechtig sein, muss aber vollstaendig validiert sein. Die
Bestaetigung prueft mindestens die erwartete aktive Basisgeneration, einen
unveraenderten validierten Kandidaten und die weiterhin erfolgreiche fachliche
und technische Validierung. Eine veraltete Basis oder ein veraenderter Kandidat
wird ohne Teilwirkung abgelehnt.

Nicht erforderlich sind persistente Preview-Slots, Preview-Owner,
Preview-Tokens oder Ablaufzeiten. Eine eigenstaendige persistente
`MutationSequence` ist nur dann entbehrlich, wenn Dokumentrevisionen und
Rootsequenz den benoetigten eindeutigen Konfliktvertrag nachweislich vollstaendig
abdecken. Diese verbleibende Funktion ist vor dem Neuschnitt separat zu pruefen;
die Sequenz wird nicht allein entfernt, weil sie nicht mehr zwingend erscheint.
Diese Detailpruefung oeffnet OD-01 nicht erneut.

### Bootstrap und Werksreset

Release 1 enthaelt:

- sicheren Bootstrap;
- automatische Factory-Initialisierung ausschliesslich bei nachweislich
  fabrikneuem und vollstaendig fehlerfrei lesbarem Speicher;
- gespeicherte Bootstrapzustaende mindestens `Initializing`, `Initialized` und
  `Resetting`;
- eine `StorageEpoch`;
- keine Behandlung beschaedigter, unbekannter oder unlesbarer Daten als
  fabrikneuen Speicher;
- keinen stillen Factory-Fallback bei Korruption oder unbekanntem Schema;
- einen ausdruecklich ausgeloesten, wiederaufnehmbaren Werksreset;
- idempotente Wiederaufnahme nach Stromausfall;
- logische Unerreichbarkeit alter Epochen nach abgeschlossenem Reset;
- keine unbelegte Behauptung sicherer physischer Loeschung alter Flashbytes.

### Aus Release 1 verschoben

Bis zum ersten echten fachlichen Konsumenten werden nicht implementiert:

- persistentes Pending und ein eigener Pending-Root;
- Aktivierungsintent, `ConfigurationActivationRunAssessment` sowie Sperr- und
  Abschlusslogik fuer Pending-Aktivierungen;
- persistente Preview-Slots, Preview-Owner, Preview-Tokens und Ablaufzeiten;
- vorbereitete Connectivity-Manifeste ohne echte Secret-Payload;
- vorbereitete Authentication-Manifeste ohne echte Nachweise;
- Prepared-/Committed-Authentication-Roots;
- `CredentialEpoch` und Secret-Rootwechsel ohne produktive Credentials;
- kombinierte Konfigurations-/Secret-Transaktionen.

Persistentes Pending wird erst mit dem ersten tatsaechlich neustartpflichtigen
Konfigurationswert eingefuehrt. Connectivity- und Authentication-Domaenen
werden erst mit den ersten realen WLAN-, Passwort- oder PIN-Nachweisen
festgelegt. R1 erzeugt dafuer keine leeren Manifeste, Dummyrecords oder
Dummy-Slots. Reale R1-Secrets bleiben getrennt von `UserConfiguration`,
`ServiceConfiguration` und `ProgramCatalog` und werden mit ihrem ersten
produktiven Konsumenten gesondert spezifiziert.

### Verbindlicher additiver Ausbaupfad zu Variante A

Variante A erweitert den R1-Kern spaeter additiv:

1. Dokumente, Manifeste, Roots und Envelopes bleiben schema-versioniert;
   Record-Type-IDs, Revisionen und Schluessel bleiben stark typisiert.
   Unbekannte neuere Schemas werden ohne Teilwirkung abgelehnt und bestehende
   Schema-1-Daten nicht still umgedeutet.
2. Neue Funktionen verwenden neue Recordtypen, neue Manifest- oder
   Root-Schemaversionen und explizite Copy-Migrationen. Sie ersetzen den
   R1-Vertrag nicht durch ein inkompatibles Speichermodell.
3. Der Active-/Fallback-Graph bleibt gemeinsame Basis. Spaeteres Pending,
   Aktivierungsintent und Secret-Domaenen werden daran angefuegt.
4. Ein spaeter persistierter Pending-Kandidat kann dieselbe Erzeugungs-,
   Validierungs- und Runtime-Vorbereitungspipeline wiederverwenden.
5. WLAN-Passwoerter, Webpasswort-Nachweise, Service-PINs und vergleichbare
   Secrets werden nicht in die drei R1-Konfigurationsdokumente eingebettet.
6. `StorageEpoch` bleibt die gemeinsame Resetgrenze, an die spaetere persistente
   Domaenen gebunden werden koennen, ohne jetzt leere Secret-Manifeste zu
   erzeugen.
7. Es entstehen keine ungenutzten Pending-Ports, Intentmodelle,
   Secret-Manifeste, Authentication-Roots, Dummy-Slots oder hypothetischen
   Zukunftsservices. Erweiterbarkeit wird durch Vertraege, Versionierung,
   Register, Migrationstests und Dokumentation gesichert.
8. NVS-faehiger Schluesselraum und Record-Type-Register bleiben eindeutig und
   kollisionsfrei erweiterbar; ungenutzte Schluessel oder Slots werden nicht im
   Voraus reserviert, sofern dies technisch nicht zwingend ist.
9. R1-Tests weisen nach, dass unbekannte neuere Root-/Manifest-Schemas ohne
   Teilwirkung abgelehnt werden, R1-Daten deterministisch lesbar bleiben,
   Copy-Migrationen Quelldaten nicht in-place veraendern und der
   Active-/Fallback-Graph spaeter um referenzierte Domaenen erweitert werden
   kann, ohne Schema-1-Daten umzudeuten. Vollstaendige Pending- oder
   Secret-Dummytests sind nicht erforderlich.

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
- Variante-B-Konfigurationsaktivierung mit Active-/Fallback-Graph, fluechtiger
  validierter Vorschau, sicherem Bootstrap, `StorageEpoch` und
  wiederaufnehmbarem Werksreset;
- lokale Authentication und die mit ihren ersten realen Konsumenten
  spezifizierten Secrets, Diagnose, Exporte und secret-freies Backup/Import;
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
2. Nach Audit-/Planungsbereinigung den minimalen sicheren Baseline-Anteil von
   #29 nachweisen.
3. Display-/Touch- und DS18B20-/1-Wire-Spikes aktorfrei ausfuehren und parallel
   #20–#24 als hardwareunabhaengige Safety-Kette fortsetzen.
4. #16/#56/#57 vor Implementierung in einem separaten ownerfreigegebenen
   Planungs-/ADR-Schritt auf Variante B zuschneiden; die Issues nicht
   unveraendert umsetzen.
5. #17 und #24 nur von den tatsaechlich benoetigten schmalen R1-Vertraegen
   abhaengig machen, nicht von spaeterem Pending oder leeren Secret-Domaenen.
6. Treiber erst nach drei bestandenen beziehungsweise ausreichend bewerteten
   Kandidatengates und identischen Messungen fixieren und in kleinen
   Adapter-PRs einbinden.
7. Produktive Aktoradapter und reale Aktortests weiterhin erst nach den
   zugeordneten Safety-Gates freigeben.
8. Web-/UI-/Backupissues vor Umsetzung in kleine, ressourcenmessbare Scheiben
   schneiden.
9. Jede spaetere Drittkomponente mit Version, Lizenz, Abhaengigkeiten,
   Base-/Head-Messung und Hardwarestatus im Komponentenregister nachfuehren.

## Erkannte Ueberdimensionierungen

| Bereich | Befund | Empfohlene Korrektur ausserhalb dieses Audits |
|---|---|---|
| #29 | minimale sichere Spikebaseline und spaetere produktive Hardwareintegration sind in einem breiten Issue verbunden | nach Auditfreigabe in Baseline-Anteil vor den Spikes und produktiven Anteil hinter den jeweiligen Safety-Gates schneiden; Issue im Audit nicht aendern |
| #56/#57 | bestehender Umfang mischt den notwendigen Active-/Fallback- und Bootstrapkern mit Pending-/Intentpfaden und Secret-Domaenen ohne ersten Konsumenten | nach entschiedenem OD-01 in separatem Planungs-/ADR-Schritt auf Variante B zuschneiden; Variante A spaeter additiv planen |
| #19 | vier grosse Verantwortungsbereiche in einem Issue | in Journal/Retention, Export und Backup/Import schneiden |
| #25–#28 | UI-, Web-, Auth-, Diagnose- und Servicepakete sind zu breit fuer kleine PRs | nach stabilen DTO-/Portgrenzen in vertikale Scheiben teilen |
| LVGL | vollstaendiges UI-Framework fuer wenige feste 320-x-240-Screens waere vorsorglich | schlanke Views als Baseline, LVGL nur nach Messnachweis |
| ESPAsyncWebServer | Async-/WebSocket-/SSE-Umfang koennte groesser als der reale R1-Bedarf sein | Frameworkserver zuerst messen; Async nur bei belegtem Vorteil |
| Espressif Provisioning/BLE | umfangreicher Provisioningstack wuerde BLE und Toolchainkomplexitaet einbringen | SoftAP/Captive-Portal-Anforderung mit kleinstem Adapter erfuellen |
| PID-Bibliotheken | allgemeine PID-/Autotune-Funktionen passen nicht zum spezifizierten begrenzten PI-/Safety-Vertrag | kleinen deterministischen PI-Kern selbst implementieren |

## Auswirkungen auf #16, #56 und #57

- **#16:** bleibt unveraendert offen und `TRACKING`. #54/#55 werden als
  vorhandene Basis wiederverwendet. Nach dem Audit muss ein separater
  ownerfreigegebener Planungs-/ADR-Schritt das Tracking und seine Abhaengigkeiten
  auf Variante B zuschneiden. Dieser Audit editiert #16 nicht.
- **#56:** bleibt `BLOCKED_DEPENDENCY` und darf nicht unveraendert umgesetzt
  werden. Sein spaeter korrigierter R1-Scope umfasst Active/Fallback,
  Graphvalidierung, fluechtige Vorschau, Konfliktschutz und Runtime-Publish.
  Persistentes Pending und Intent werden in spaetere eigene Arbeit verschoben.
- **#57:** bleibt `BLOCKED_DEPENDENCY` und darf nicht unveraendert umgesetzt
  werden. Sein spaeter korrigierter R1-Scope umfasst Bootstrap, `StorageEpoch`,
  Korruptionssperre und wiederaufnehmbaren Werksreset. Connectivity- und
  Authentication-Domaenen entstehen erst mit realen Konsumenten in eigener
  spaeterer Planung.
- **#17/#24:** duerfen nur von den tatsaechlich benoetigten schmalen
  Variante-B-Vertraegen abhaengen, nicht von Pending-, Intent- oder vorbereiteter
  Secret-Infrastruktur.

Die vorgeschlagene Reihenfolge steht in
[`PROPOSED_RELEASE_1_ROADMAP.md`](PROPOSED_RELEASE_1_ROADMAP.md).

## Offene Ownerentscheidungen

| ID | Entscheidung |
|---|---|
| OD-02 | Display-/Touchstack nach dem identischen Hardware-Spike waehlen |
| OD-03 | DS18B20-/1-Wire-Stack nach Toolchain- und Hardware-Spike waehlen |
| OD-04 | Arduino-Framework-Webserver oder ESPAsyncWebServer nach identischem Last-/Ressourcentest |
| OD-05 | schlanke eigene Screens oder LVGL nach representativem Screen-/Ressourcenvergleich |
| OD-06 | WiFiManager oder kleiner Framework-Onboardingadapter |
| OD-07 | R1-Mindestumfang und PR-Schnitt von #19 und #25–#28 |
| OD-09 | KDF-, Work-Factor-, Sitzungs-, CSRF-, Sperr- und Secret-at-rest-Vertrag vor #27 |

OD-01 ist mit Variante B entschieden. Offen bleibt nur die technische
Detailpruefung, ob Dokumentrevisionen und Rootsequenz die bisherige Funktion
einer eigenstaendigen persistenten `MutationSequence` vollstaendig abdecken.
Sie ist keine erneute Auswahl zwischen Variante A und B.

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
