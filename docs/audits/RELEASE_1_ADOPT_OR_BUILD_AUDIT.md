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
6. Hardwarekandidaten erhalten vergleichbare, gestufte Spikeplaene, bevor ein
   Produktivstack gewaehlt wird. Beim Display/Touch erreichen nur Kandidaten mit
   bestandenem kurzem Smoke-Test die vollstaendige identische Hardwarematrix.

Es wurden keine Bibliotheken installiert, keine Spikes ausgefuehrt und keine
Ressourcenwerte fuer noch nicht eingebundene Komponenten erfunden.

## Executive Summary

Die akzeptierte Release-1-Grenze ist grundsaetzlich stimmig: Das Geraet muss
lokal und ohne Netzwerk sicher fermentieren. Das Touchdisplay ist die einzige
lokale Bedien- und Anzeigeoberflaeche; die Weboberflaeche ist sekundaer. Der
230-V-AC-Hauptschalter schaltet das Geraet rein elektrisch und ist kein
Firmwareeingang. Der Summer ist das einzige zusaetzliche lokale Ausgabeelement.
Release 1 benoetigt ausserdem begrenzte Persistenz/Recovery, Diagnose, nur
lesende Exporte, secret-freies Backup, vollvalidierten atomaren Import und
UART-Recovery. OTA, Bluetooth, Cloud/Push,
PID-Autotuning, Kaskadenregelung und PSRAM-Abhaengigkeit bleiben ausserhalb.

Die Plattform muss nicht Display-, 1-Wire-, JSON-, HTTP-, WLAN-, NVS-, Zeit-
oder UART-Grundfunktionen neu entwickeln. Diese Aufgaben sollen aus der
fixierten Plattform oder aus konkret geprueften Bibliotheken uebernommen und
hinter schmalen Adaptern gekapselt werden. Dagegen bleiben Sensorqualitaet,
Regelsensorauswahl, PI/Luftbegrenzung, Aktorplanung, Safety, Prozess-, Recovery-
und Berechtigungslogik eigene deterministische Module.

Zwei Hardwarebereiche duerfen nicht am Schreibtisch entschieden werden:

- Display/Touch: LovyanGFX, TFT_eSPI und das LCDWiki-Paket durchlaufen alle die
  Quellen-/Lizenz- und Buildpruefung. Ausreichend erfolgreiche Kandidaten
  erhalten einen kurzen identischen Smoke-Test; nur dessen erfolgreiche
  Kandidaten erreichen die vollstaendige identische Hardwarematrix.
- DS18B20: DallasTemperature+OneWire und die Espressif-Komponenten durchlaufen
  gestuft denselben Build-, Sensorsmoke-, Topologie- und Fehlervergleich. Die
  Softwarestackwahl bleibt von der elektrischen Bustopologiewahl getrennt; der
  Espressif-Stack muss zuerst seine Kompatibilitaet mit der aktuellen
  Arduino-ESP32-2.0.17-Toolchain beweisen.

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
- ArduinoJson `7.4.3` ist der bevorzugte Kandidat fuer begrenzte externe
  JSON-Grenzen, aber bis zum Build-, Grenzwert-, Fuzz- und Ressourcenspike
  keine ausgewaehlte Produktionsabhaengigkeit; interne kritische Persistenz
  bleibt beim vorhandenen typisierten binaeren Wireformat.
- Framework-WLAN, Zeit/NTP, mDNS/DNS, GPIO und UART werden konfiguriert und
  adaptiert.
- Der kleine lokale HTTP-Dienst ist als R1-Anforderung entschieden.
  Arduino-ESP32 `WebServer` ist `FIRST_EVALUATION_CANDIDATE` und bedingte
  Produktivrichtung, bleibt aber bis zum begrenzten Baselineprototyp
  `SPIKE_REQUIRED` und `FINAL_SELECTION_PENDING`. `ESPAsyncWebServer` bleibt
  `CONDITIONAL_FALLBACK` und `EVALUATE_LATER`; eine identische Evaluation
  beginnt nur bei einem dokumentierten R1-Problem des ersten Kandidaten.
- Beim WLAN-Onboarding ist WiFiManager der bevorzugte Release-1-Kandidat und
  wird zuerst in einem begrenzten Spike geprueft. Die endgueltige Uebernahme
  folgt erst nach bestandenem Spike. Ein kleiner Adapter aus Arduino-ESP32
  `WiFi`, `DNSServer`, SoftAP und dem zuerst evaluierten `WebServer` wird
  nur bei einem dokumentierten Problem als identischer Gegenprototyp
  nachgezogen.

### 2. Safety und Fachlogik nicht delegieren

Treiberbibliotheken duerfen Sensorbytes, Pixel oder HTTP-Verbindungen
verarbeiten. Sie entscheiden nicht ueber Rollenbindung, Ersatzbetrieb,
Heiz-/Kuehlfreigabe, Totzeit, Fehlerreset, Recovery oder Authberechtigung. Der
optionale, verwendbare Produktfuehler ist der primaere Regelsensor; er darf im
Stillstand und in einem dafuer zulaessigen Lauf fehlen. Ohne ihn uebernimmt der
Raum-/Luftsensor regulaer die Regelung. Der Kuehlkoerper-/Peltier-Schutzsensor
ist fuer jede Peltierfreigabe verpflichtend; ein fehlendes, ungueltiges,
veraltetes oder nicht ausreichend vertrauenswuerdiges Signal sperrt sie. Die
Issues #20–#24 bleiben deshalb
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

#19 vereint vier getrennt pruefbare Verantwortungsbereiche: typisiertes
Ereignisjournal und Retention, begrenzte persistente Laufhistorie und
Bereinigung, nur lesenden Laufexport und secret-freies Backup sowie
Importvorschau und atomare Aktivierung. Der fachliche R1-Umfang bleibt, aber
die Umsetzung wird nach Auditfreigabe in einem eigenen ownerfreigegebenen
Planungsschritt geschnitten. #25 wird nach dem verbindlichen OD-07-Teilentscheid
in oberflaechenneutrale Praesentationsmodelle sowie gemeinsame
Sprachressourcen/Formatierungsregeln getrennt. Navigation und Layout bleiben
ausserhalb. #26 bleibt als einzige lokale Bedienoberflaeche vollstaendig
R1-relevant, wird aber nach dem verbindlichen Teilentscheid in fuenf kleine
Bedienbereiche geschnitten. #27 wird nach dem verbindlichen OD-07-Teilentscheid
in HTTP-Transport/API, Status/Polling/Laufchart, schreibende Kommandos,
responsive Webassets und Authentisierung gemaess OD-09 getrennt. #28 vereint
mehrere Diagnose- und Serviceverantwortungen und wird nach dem verbindlichen
OD-07-Teilentscheid in vier kleine Bereiche geschnitten: passive
Diagnosemodelle/Boot-Selbsttest, Ressourcen-/Gesundheitsdiagnose, gefuehrter
Serviceablauf sowie ein nur lesender Diagnose-/Servicebericht. Damit ist OD-07
fachlich vollstaendig entschieden; die Live-Issues bleiben in diesem Audit
unveraendert.

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
Sensorbustopologie, Aktoradapter, Safety-Grenzen oder PI-Parameter. Der
Display-/Touchvergleich beginnt danach mit der realen Hardwareidentifikation in
Stufe 0. Alle drei Hauptkandidaten durchlaufen Stufe 1 fuer Quellen, Lizenzen,
Kompatibilitaet und reproduzierbaren Build. Nur ausreichend erfolgreiche
Kandidaten erreichen den kurzen Hardware-Smoke-Test in Stufe 2, und nur
`PASS_SMOKE_TEST` erreicht die vollstaendige identische Matrix in Stufe 3.
Stufe 4 bestimmt bevorzugten Treiber und Rueckfallkandidat. Reservekandidaten
werden nur bei einem dokumentierten Ausloeser nachgezogen. Der Sensorspike
verwendet die Stufen 1 bis 3: Quelle/Lizenz/Build, Sensorsmoke-Test und erst
danach die vollstaendige identische Topologie-/Fehlermatrix. Ein
Build-Scheitern wird typisiert dokumentiert und nicht durch Toolchainwechsel
oder verfruehten Hardwaretest umgangen.

Beim Sensorspike sind Softwarestack und Bustopologie getrennte Entscheidungen.
Der Produktfuehler besitzt verbindlich einen eigenen Bus. Drei getrennte Busse
sind bevorzugt; ein gemeinsamer Bus nur fuer die beiden festen Sensoren bleibt
pinabhaengiger Rueckfall. Alle drei Sensoren auf einem Bus werden nicht
produktiv geplant. Die mechanische und elektrische Ausfuehrung des trennbaren
Produktfuehleranschlusses bleibt eine spaetere Hardwareentscheidung. Dieser
Softwareaudit legt weder Stecksystem, Kontaktreihenfolge, Anschlussbelegung,
GPIOs noch Schutzbauteilwerte fest. Allgemeine Trennungs-, Fehler- und
Wiederkehrtests bleiben Bestandteil des Sensorspikes.

LVGL ist kein Display-/Touchtreiberkandidat. Es wird erst nach Treiberauswahl,
schmalem Adaptervertrag und einem repraesentativen Release-1-Screen mit
schlanken eigenen Views unter identischen Bedingungen verglichen.

Parallel laufen #20 Sensorqualitaet, #21 Regelsensorauswahl, #22 PI/
Luftbegrenzung, #23 Aktorplaner und #24 Fehlerkern/`SAFE_BOOT`. Bibliothekstypen,
reale GPIOs und erfundene Messwerte gelangen nicht in den Safety-/Fachkern;
Treiber- und Fachstatus bleiben getrennt. Der Audit empfiehlt eine spaetere
Aufteilung von #29 in minimalen Baseline- und produktiven Hardwareanteil, aendert
das Issue aber nicht.

### 7. Frameworkserver ist der erste Evaluationskandidat

Die Weboberflaeche bleibt sekundaer. Der Fermentationsschrank muss ohne WLAN
und Browser lokal ueber das Touchdisplay vollstaendig und sicher bedienbar
bleiben. Release 1 benoetigt einen kleinen lokalen HTTP-Dienst
(`REQUIREMENT_DECIDED`). Der in Arduino-ESP32 `2.0.17` enthaltene synchrone
`WebServer` ist dafuer `FIRST_EVALUATION_CANDIDATE` und die bedingte
Produktivrichtung. Er benoetigt keine zusaetzliche Serverbibliothek und ist fuer
wenige lokale Clients sowie einen begrenzten Endpunktsatz die kleinste plausible
Basis. Seine technische Eignung ist jedoch `SPIKE_REQUIRED`; die endgueltige
produktive Auswahl bleibt `FINAL_SELECTION_PENDING`.

Der Release-1-Bedarf umfasst statische HTML-/CSS-/JavaScript-Ressourcen,
begrenzte Status-, Temperatur-, Lauf-, Programm- und Konfigurationsabfragen,
Konfigurationsaenderungen mit Basisgenerations- und Konfliktpruefung,
Laufkommandos, Diagnose, begrenzte Exporte und Imports, Anmeldung/Sitzungen
sowie die lokale HTTP-Oberflaeche des WLAN-Onboardings. Status- und
Diagrammdaten duerfen ueber begrenztes Polling abgerufen werden. WebSocket,
Server-Sent Events, ein permanenter bidirektionaler Stream, hohe Clientzahlen,
Cloudtransport, direkter Internetbetrieb, unbeschraenkte Uploads und
Millisekunden-Echtzeitdarstellung sind keine Release-1-Pflicht.

Der Frameworkserver wird erst dann zur Produktivloesung, wenn ein begrenzter
Prototyp statische
Ressourcen, JSON-Anfragen, Import/Export und wenige lokale Clients innerhalb
fester Zeit-, Groessen-, Tiefen-, Parallelitaets- und Speichergrenzen stabil
bedient. Langsame oder abgebrochene Clients, WLAN-Unterbruch und ungueltige oder
uebergrosse Anfragen muessen kontrolliert enden; Regel- und Safety-Ausfuehrung
duerfen nicht relevant verzoegert werden. Flash, statisches RAM, freier und
niedrigster Heap, groesster freier Heapblock, Antwort- und Bearbeitungszeit,
Regelzyklus-Jitter sowie Watchdog-/Resetereignisse werden gemessen.

`ESPAsyncWebServer` bleibt `CONDITIONAL_FALLBACK` und `EVALUATE_LATER`. Er wird
nur bei einem konkreten offenen Release-1-Risiko mit
demselben Prototyp verglichen, etwa bei unvertretbarem Regelzyklus-Jitter,
nicht sinnvoll begrenzbarer Blockierung durch langsame Clients, instabiler
benoetigter Parallelitaet oder nicht robust begrenzbaren Import-/Exportpfaden.
Eine Uebernahme verlangt einen klaren Stabilitaets-, Funktions- oder
Ressourcenvorteil. Popularitaet, ein groesserer README-Funktionsumfang oder
vorsorgliche WebSocket-/SSE-/Cloudfaehigkeit reichen nicht.

Die R1-Architektur kapselt den konkreten Server in einer kleinen
ESP32-Integrationsschicht fuer Initialisierung, Routenbindung, begrenzte
Bodyannahme, Request-/Responseuebersetzung, Timeouts und Fehleruebersetzung.
Endpunkte uebersetzen HTTP ausschliesslich in begrenzte DTOs und fachliche
Queries oder Kommandos. Server-, Request-, Response-, Connection-, Socket- und
Callbacktypen gelangen nicht in `fermentation_app`, Safety-, Persistenz- oder
gemeinsame View-Modelle. Laufkommandosemantik, Validierung, Konflikte,
Berechtigung, Safety, Persistenz, Diagnose und Importvalidierung bleiben
serverunabhaengig.

Es entsteht keine allgemeine `IWebTransport`-Hierarchie, keine Stream-, SSE-
oder WebSocket-Abstraktion, kein Middleware-, Provider- oder Pluginframework und
kein Dummy-Zweitadapter. Ein spaeterer Serverwechsel bleibt trotzdem moeglich:
Er ersetzt an der Composition Root beziehungsweise ESP32-Integrationsgrenze
Serverinitialisierung, Routenbindung, Request-/Responseuebersetzung und
Verbindungslebenszyklus, nicht DTOs, Fachkommandos, Validierung, Authpolicy,
Konfliktsemantik, Persistenz oder Safety-Core. Voraussetzung sind dieselben
begrenzten Endpunkt-/DTO-Vertraege und ein belegter konkreter Vorteil.

> Ein spaeterer Wechsel des lokalen HTTP-Servers ist zulaessig, wenn der Ersatz
> dieselben begrenzten Endpunkt-/DTO-Vertraege erfuellt und der Vergleich einen
> konkreten Vorteil belegt. Die R1-Architektur kapselt Servertypen an der
> Integrationsgrenze, baut aber keine vorsorgliche allgemeine
> Transportplattform.

HTTP bleibt lokale sekundaere Bedienung ohne direkten Internet- oder Cloudzwang.
Webserver- oder WLAN-Fehler beenden keinen Lauf und stoppen den Regel-/Safety-
Kern nicht. Webanfragen koennen Aktoren nur ueber die normalen fachlichen
Kommando- und Safety-Pfade beeinflussen. Authentisierung, CSRF, Sessions,
Sperrlogik und Redaction bleiben eigene Vertraege; Serverhilfen ersetzen keine
Authpolicy, und Secrets gelangen weder in Logs noch Exporte oder
Fehlermeldungen.

### 8. OD-06: WiFiManager zuerst begrenzt pruefen

OD-06 ist als Richtungsentscheid geklaert: WiFiManager `v2.0.17`
(`d82d0a1b`) ist der bevorzugte technische Kandidat fuer das Release-1-
WLAN-Onboarding, aber noch keine ausgewaehlte Produktionsabhaengigkeit. Zuerst
werden Quelle, Lizenz, eingebettete Webassets, transitive Abhaengigkeiten und
der reproduzierbare Build mit PlatformIO `espressif32@7.0.1`, Arduino-ESP32
`2.0.17`, ESP32-32E, 4 MB Flash und ohne PSRAM geprueft. Danach folgt ein
begrenzter realer Spike mit Android, iOS beziehungsweise iPadOS und Windows.
Automatische Captive-Portal-Erkennung und der direkte Aufruf ueber die
angezeigte IP werden getrennt getestet.

Der Spike prueft einen ausdruecklich gesteuerten Portalstart, SoftAP-, DNS- und
Portallebenszyklus, WLAN-Scan, gueltige und falsche Zugangsdaten, nicht
erreichbaren Access Point, Abbruch, Timeout, Browser- und WLAN-Unterbruch,
Neustart sowie geeignete Stromunterbruch-Cut-Points. Ein gewoehnlicher
voruebergehender Router-, Access-Point- oder Internetausfall startet das Portal
nicht automatisch. Neue WLAN-Daten bleiben bis zum begrenzten Verbindungs- und
Funktionsnachweis ein unbestaetigter Kandidat; ein Fehlschlag veraendert den
bisherigen funktionierenden WLAN-Stand nicht. Secrets gelangen nicht in Logs,
URLs, Diagnose, Exporte oder Fehlermeldungen. Gemessen werden Firmwaregroesse,
statisches RAM, freier und niedrigster Heap, groesster freier Heapblock,
Portalstart-, Verbindungs- und Antwortzeiten, Regelzyklus-Jitter,
Watchdog-/Resetereignisse, Abhaengigkeiten und projektspezifischer
Integrationscode.

Der primaere Onboarding-QR-Code enthaelt die individuelle SoftAP-SSID und das
individuelle SoftAP-Passwort im gaengigen WLAN-QR-Format. SSID, Passwort,
Portaladresse beziehungsweise IP und eine lokale Schaltflaeche zum erneuten
Anzeigen des QR-Codes bleiben separat sichtbar; die Portaladresse ist nur der
manuelle Rueckfall nach dem WLAN-Beitritt. Der Spike prueft Escaping,
Sonderzeichen und Scannbarkeit auf 320 x 240 mit den relevanten Clients. Der
Payloadvertrag ist `REQUIREMENT_DECIDED`; Darstellung und Scannbarkeit sind
`SPIKE_REQUIRED`, die QR-Bibliothek bleibt `FINAL_SELECTION_PENDING`.

WiFiManager bleibt ein technischer Portalbaustein. Startentscheidung,
Touchstatus und Abbruch, Kandidatenpruefung, Credential-Commit,
Secret-Lebenszyklus, Redaction, Recovery, Fehlersemantik und Safetyisolation
bleiben projektspezifisch. WiFiManager-, SoftAP-, DNS-, HTTP-, Callback- und
Frameworktypen enden in der konkreten ESP32-Integrationsschicht. Es entsteht
weder ein allgemeines Provisioning-/Provider-/Pluginframework noch eine
vorsorgliche Mehradapterimplementierung.

Nur wenn dieser Spike ein konkretes Release-1-Problem nachweist, wird der
kleine Frameworkadapter aus Arduino-ESP32 `WiFi`, `DNSServer`, SoftAP und
`WebServer` mit demselben begrenzten Ablauf und derselben Messmatrix erstellt.
Ausloeser sind insbesondere unkontrollierter Portalstart oder Credential-
Commit, ein nicht beherrschbarer Secret- oder AP-/DNS-/Serverlebenszyklus,
relevante Clientinstabilitaet, Toolchainkonflikt, unvertretbare Ressourcen- oder
Jitterwirkung, problematische Abhaengigkeiten oder eine nicht robust
abbildbare Release-1-Anforderung. Erst danach waehlt der Owner WiFiManager oder
den Frameworkadapter endgueltig. Ein spaeterer Wechsel bleibt ueber die kleine
konkrete ESP32-Integrationsgrenze an der Composition Root moeglich, ohne leere
Zukunftsports vorzubereiten.

Das geschuetzte Ersatz-WLAN ist ein eigener akzeptierter R1-
Netzwerklebenszyklus und kein Onboarding-Fallback. Es startet erst, wenn ein
konfiguriertes Heim-WLAN laenger als eine konfigurierbare Zeit unerreichbar
bleibt. Reconnectversuche laufen parallel weiter; Fermentation und lokale
Bedienung bleiben unveraendert, waehrend normale Weboberflaeche, Laufanzeige,
zulaessige Bedienhandlungen, Netzwerkdiagnose und erneute WLAN-Einrichtung
erreichbar sind. Auth-, CSRF-, Service-PIN- und Safetygates gelten
unveraendert. Nach stabiler Rueckkehr des Heim-WLANs endet das Ersatz-WLAN nach
einer kontrollierten Uebergangszeit, ohne offene Requests oder
Speichervorgaenge unkontrolliert abzuschneiden. Funktion:
`REQUIREMENT_DECIDED`; Zeiten: `TBD_COMMISSIONING`; technische Umsetzung hinter
WLAN-, Webserver- und Auth-Spike-Gates. Kurze Ausfaelle duerfen weder
Ersatz-WLAN noch Onboardingportal starten.

### 9. Verbindlicher JSON-Richtungsentscheid

ArduinoJson `7.4.3` (Tag-Commit
`77771d3c07668e01d8f52acb03910c1110bb373f`, MIT) ist der bevorzugte
technische JSON-Codec fuer Release 1. Dieser Richtungsentscheid bindet noch
keine Produktionsabhaengigkeit: Vor der endgueltigen Uebernahme folgen ein
reproduzierbarer isolierter Build mit der fixierten Toolchain sowie begrenzte
Grenzwert-, Negativ-, Fuzz-, Laufzeit- und Ressourcenpruefungen. Eine andere
Bibliothek oder ein eigener Parser wird nur untersucht, wenn dieser Nachweis
ein konkretes Release-1-Problem belegt. Ein allgemeiner Eigenparser, ein
`IJsonProvider`, ein Codec-Pluginregister oder ein vorsorglicher Zweitcodec
entstehen nicht.

JSON bleibt auf begrenzte externe Vertraege beschraenkt: Web-API,
Konfigurations- und Programaenderungen, Diagnoseantworten, Exporte,
secret-freie Backups sowie Import und Importvorschau. Atomare Kontrollpunkte,
Active-/Fallback-Roots, Safety-Zustaende, Lauf-Recovery und interne Records
verwenden weiterhin das typisierte binaere Persistenzmodell. Grosse Historien
oder Diagnosedaten werden begrenzt, paginiert oder gestreamt und nicht als
unbeschraenktes Gesamtdokument aufgebaut.

Die Integrationsrichtung lautet:

```text
begrenzte HTTP-/Import-Bytequelle
  -> Content-Type-, Content-Length- und Bytegrenzen
  -> kleiner konkreter ArduinoJson-Codec
  -> projektspezifische Parse-/Strukturfehler
  -> typisiertes DTO
  -> Schema-, Werte-, Berechtigungs-, Konflikt- und Fachvalidierung
  -> fachliches Kommando oder validierte Importvorschau
```

Antworten und Exporte laufen nach projektspezifischer Redaction aus einem
typisierten Response-DTO ueber den kleinen Serializer moeglichst direkt in ein
begrenztes Streamziel. `JsonDocument`, `JsonObject`, `JsonArray`,
`JsonVariant` und bibliotheksspezifische Fehler enden an dieser konkreten
Codec-/ESP32-Integrationsgrenze. Sie gelangen weder in `fermentation_app`,
Safety-, Prozess-, Persistenz- und Secretmodelle noch in gemeinsame
View-Modelle oder fachliche Ports und Kommandos. Parsererfolg ist keine
fachliche Gueltigkeit; ein Import wird waehrend des Parsings nie aktiviert
oder als kanonischer Zustand veroeffentlicht.

Fuer Spike und Prototyp gelten zunaechst diese Eingabeprofile:

| Profil | Zweck | initiale Bodygrenze |
|---|---|---:|
| A | kleine Kommandos wie Start, Stop oder Bestaetigung | 1 KiB |
| B | Programm- und zusammengehoerige Konfigurationsaenderungen | 4 KiB |
| C | vollstaendiger R1-Import | `DERIVE_FROM_MAXIMUM_VALID_EXTERNAL_SCHEMA`; `MEASUREMENT_REQUIRED` |

Die maximale Verschachtelung betraegt im Prototyp zunaechst 6. Root-Typ,
Methode, Content-Type, String-, Array-, Objektfeld-, Zahlen-, Schema- und
Antwortgrenzen werden pro Vertrag festgelegt. Unbekannte Felder werden je
Schema einheitlich kontrolliert abgelehnt oder ausdruecklich ignoriert;
`NaN`, `Infinity` und andere nicht standardkonforme oeffentliche Zahlenwerte
sind unzulaessig. Bodies ohne verlaesslich begrenzbare Laenge werden
kontrolliert und zeitlich begrenzt verarbeitet oder abgelehnt. Fuer Profil A
und B muss das jeweilige maximale DTO die initiale Grenze bestaetigen. Fuer
Profil C erzeugt der Spike zuerst deterministisch den maximal gueltigen
externen Importkandidaten aus maximal 16 Programmen, IDs bis 48 Byte, Namen bis
96 Byte, Notizen bis 1.024 Byte je Programm und allen weiteren
Programmfeldern, Konfiguration, Schemaversionen, JSON-Struktur,
Worst-Case-Escaping, UTF-8, Metadaten sowie Integritaets- und Referenzfeldern.
Erst daraus wird die harte externe Grenze abgeleitet. Der vollstaendige
gueltige Maximalfall muss importierbar bleiben.

Der Spike entscheidet danach zwischen einem begrenzten Gesamtbody mit
begruendeter Reserve und einem begrenzten Streaming-/Chunkpfad. Auch beim
Streaming entstehen vor jeder Aktivierung ein vollstaendiger typisierter
Kandidat und dessen technische und fachliche Vollvalidierung; Gesamt-, Feld-,
Objekt-, String-, Array- und Zeitgrenzen bleiben fest. Abbruch oder
Stromunterbruch hat keine Teilwirkung. Backupausgabe und Importrequest sind
getrennte Vertraege; eine gestreamte Backuperzeugung beweist keinen
importierbaren Gesamtbody.

Die Codecgrenze uebersetzt mindestens Bodygroesse, Content-Type, Syntax,
Abbruch, Tiefe, Ressourcenlimit, Root-Typ, Pflicht-/unbekannte Felder,
Datentypen, String-/Arraygrenzen, Wertebereich, Schema, geschuetzte Felder,
Berechtigung, Revisionskonflikt, technische und fachliche Importfehler,
fehlende Bestaetigung sowie Serialisierungsfehler in stabile
projektspezifische Kategorien. Oeffentliche Fehler enthalten weder Secrets
noch Bibliotheksdetails, Speicheradressen oder ungefilterte Eingaben.

Der Nachweis erfolgt gestuft: Quelle/Lizenz/Toolchain, kleiner Codecprototyp,
reproduzierbare Grenz-/Negativ-/Fuzztests, ESP32-Ressourcen- und
Laufzeitmessung und erst danach die endgueltige Ownerfreigabe. Gemessen werden
Firmwaregroesse, statisches RAM, freier und niedrigster Heap, groesster freier
Heapblock, maximale gleichzeitige Speicherbelegung, moegliche Fragmentierung,
Parse-/Serialisierungszeit, Regelzyklus-Jitter sowie Watchdog-, Reset- und
Stabilitaetsauffaelligkeiten. Ein spaeterer Codecwechsel ersetzt nur die kleine
konkrete DTO-/Codecgrenze; er rechtfertigt keine allgemeine Providerarchitektur.

### 10. OD-07-Teilentscheid: Issue #19 in vier Bereiche schneiden

Der Teilentscheid fuer #19 ist verbindlich. Die Teilentscheide fuer #25 bis
#28 sind inzwischen ebenfalls verbindlich; damit ist OD-07 insgesamt
abgeschlossen. Der Audit aendert #19 nicht und erstellt
keine Folgeissues. Nach Auditabschluss soll ein eigener ownerfreigegebener
Planungsschritt den folgenden Schnitt festlegen:

1. **Typisiertes Ereignisjournal und Retentionpolicy:** stabile
   Ereigniskategorien, feste Recordgrenzen, eindeutige Reihenfolge, monotone
   Zeit und optional vertrauenswuerdige UTC-Zeit, deterministische
   Loeschkandidaten sowie stromausfallsichere Fortschritts- und
   Recoverysemantik. Zum R1-Journal gehoeren Boot-/Resetursachen,
   Bootstrap-/Recovery-, Korruptions-/Fallback-/Werksreset-, Safety-, relevante
   Sensor- und Regelsensorwechselereignisse, Laufstart/-wechsel/-abschluss/
   -abbruch, relevante Benutzerkommandos, bestaetigte Konfigurations- und
   Importaktivierungen sowie safety- und recoveryrelevante Diagnose. Regulaere
   periodische Temperaturmessungen sind keine einzelnen Journalereignisse.
   Secrets und unredigierte sensible Eingaben werden nie journalisiert.
2. **Begrenzte persistente Laufhistorie und stromausfallsichere Bereinigung:**
   aktiven Lauf und Recoverydaten schuetzen, Messreihen fuer Diagramme begrenzen
   und verdichten, abgeschlossene Laufzusammenfassungen erhalten sowie
   proaktiv und nach Unterbruch wiederaufnehmbar bereinigen. Der aktive Lauf,
   die letzten 5 detaillierten Laeufe und 50 Laufzusammenfassungen bleiben ein
   auf realem Speicher zu messendes R1-Ziel, keine ungepruefte absolute
   Speichergarantie. Ein detaillierter Lauf speichert nicht jede etwa
   zweisekundliche Rohmessung. Die Loeschprioritaet schuetzt in dieser
   Reihenfolge aktiven Lauf und Recoverydaten, kritisches Safety-/
   Recoveryjournal, Zusammenfassungen und erst danach detaillierte Komfort-
   und Diagrammdaten.
3. **Nur lesender Laufexport und secret-freies Backup:** Laufexporte verwenden
   begrenzte JSON- beziehungsweise fuer Messreihen geeignete CSV-Vertraege.
   Normale Backups enthalten nur exportierbare Benutzerkonfiguration,
   Programme, zulaessige Metadaten, Schemaversionen und notwendige Referenzen.
   WLAN-/Webpasswoerter, PINs, Tokens, Sessiondaten, ungefilterte Diagnose,
   vollstaendige interne Journale, Slots und Rohrecords bleiben ausgeschlossen.
   Journal, Laufexport und Backup sind getrennte Exportarten; groessere
   Ausgaben werden gestreamt oder stueckweise erzeugt.
4. **Importvorschau, Vollvalidierung und atomare Aktivierung:** Dieser
   risikoreichste Teil folgt erst auf den nur lesenden Export-/Backupteil. Nach
   harter Bytegrenze, Parsing-, Schema-, Struktur-, Typ-, Fach- und
   Secretpruefung entsteht ein vollstaendiger typisierter Kandidat. Vorschau,
   Konfliktpruefung gegen die erwartete aktive Basis, ausdrueckliche
   Bestaetigung und Vorbereitung eines unveraenderlichen Runtime-Snapshots
   gehen der atomaren Aktivierung ueber den OD-01-Active-/Fallback-Kern voraus.
   Fehler, Abbruch, Neustart oder Stromunterbruch lassen keine Teilwirkung
   zurueck; fremde Daten duerfen weder interne Roots/Slots noch Secrets still
   uebernehmen. Eine bestaetigte Aktivierung wird nachvollziehbar journalisiert.
   Ein schmaler synchroner Import-Run-Gate unterscheidet `Unknown`,
   `NoActiveOrRecoverableRun`, `ActiveRunPresent` und
   `RecoverableRunPresent`; nur `NoActiveOrRecoverableRun` erlaubt den Import.
   Der Gate wird vor Dateiannahme beziehungsweise Importstart, vor
   Vorschau/Bestaetigung und unmittelbar vor dem atomaren Commit unter
   derselben serialisierten Anwendungsentscheidung geprueft. Aktive,
   pausierte/unterbrochene, wiederherstellbare und unbekannte Laufzustaende
   blockieren sicher. Runstart und Importcommit sind gegeneinander
   serialisiert: Gewinnt der Runstart, wird der Import abgelehnt; gewinnt der
   Importcommit, sieht ein danach gestarteter Lauf nur die vollstaendig neue
   Konfiguration. Dafuer entstehen weder persistentes Pending noch
   Aktivierungsintent oder ein paralleler Active-Zweig. Tests decken alle
   Gatezeitpunkte, konkurrierenden Runstart, Neustarts und Cut-Points ab.

Der Werksreset bleibt Bestandteil des zentralen Bootstrap-/Recoveryvertrags
von #57. #19 darf nur seine Journal-/Historiedaten gemaess der zentralen
Resetpolicy behandeln, gegebenenfalls vorher exportieren und ein Resetereignis
protokollieren, soweit der zentrale Ablauf dies zulaesst. Gemaess ADR-010 und
der zentralen Persistenzspezifikation behaelt ein vollstaendiger Werksreset die
geraetespezifische Touchkalibrierung; #19 loescht oder veraendert sie nicht und
#57 implementiert diese Erhaltung. Ein gesonderter lokaler Recoveryfall fuer
unbrauchbare Kalibrierungsdaten bleibt davon getrennt. Touchkalibrierung bleibt
aus portablen Backups ausgeschlossen und wird nicht aus ungeprueften Werten
automatisch wiederhergestellt.

### 11. OD-07-Teilentscheid: Issue #25 auf gemeinsame UI-Basis begrenzen

Der Teilentscheid fuer #25 ist verbindlich. #19 sowie #25 bis #28 sind
innerhalb von OD-07 entschieden; damit ist OD-07 insgesamt abgeschlossen. Der
Audit aendert #25
nicht und erstellt keine Folgeissues. Nach Auditabschluss soll ein eigener
ownerfreigegebener Planungsschritt #25 auf zwei Bereiche reduzieren:

1. **Oberflaechenneutrale Praesentationsmodelle:** Kleine anwendungs- oder
   ansichtsbezogene Projektionen stellen Touchdisplay und Weboberflaeche
   dieselben fachlichen Fakten bereit. Dazu gehoeren Geraete- und Laufzustand,
   Prozessphase, Soll-, Produkt-, Raum-/Luft- und freigegebene
   Schutztemperatur, Messwertqualitaet `gueltig`, `fehlt`, `veraltet` oder
   `ungueltig` samt Alter, tatsaechlich verwendeter Regelsensor und
   Ersatzbetrieb, Lauf-/Restzeit, Programmzusammenfassungen, priorisierte
   Meldungen samt Schweregrad und Quittierbarkeit, zulaessige Aktionen mit
   Sperrgruenden sowie Revisionsinformationen. Text-, Meldungs- und
   Fehlerkennungen bleiben
   sprachunabhaengig. Die Projektion liest kanonischen Fach-, Prozess-,
   Sensorqualitaets-, Safety- und Berechtigungszustand und erfindet keine
   eigene Entscheidung darueber, welcher Sensor regelt, ob ein Wert
   vertrauenswuerdig ist, ob ein Aktor freigegeben, ein Fehler quittierbar oder
   ruecksetzbar, ein Kommando beziehungsweise Servicezugang zulaessig oder eine
   Revision konfliktfrei ist.
2. **Gemeinsame Sprachressourcen und Formatierungsregeln:** Release 1 umfasst
   Deutsch, Spanisch und Englisch mit Deutsch als kontrollierter
   Fallbacksprache. Stabile Textschluessel und sprachunabhaengige Fehler-/
   Meldungscodes speisen vollstaendig gepruefte Kataloge. Gemeinsame
   semantische Regeln formatieren Einheiten, Temperaturen, Dauern, Datum und
   Uhrzeit erst an der Praesentationsgrenze; Fachwerte bleiben typisiert.
   Benutzerdefinierte Programmnamen werden nicht automatisch uebersetzt.
   Display- und Browsersprache duerfen unabhaengig gewaehlt werden; ihre
   konkrete Auswahl und Persistenz bleibt oberflaechenspezifisch.

Es entsteht keine allumfassende `DeviceUiModel`-artige Mega-View und kein
globaler UI-Zustand, der Touch und Web koppelt. Gemeinsame Modelle enthalten
weder Navigation noch Pixelkoordinaten, Schriftgroessen, konkrete Farben,
Touchflaechen, HTML, CSS-Klassen, Webrouten, Browserzustaende, LVGL-, Widget-,
Displaytreiber-, ArduinoJson- oder Webservertypen. Touchnavigation, Dialoge,
Stop-/Servicefluss und Touchziele gehoeren zu #26; Webrouten, responsive
Navigation, Browserhistorie und Sessionbezug zu #27; Display-/Touchadapter zu
#31. Die Treiberentscheidung OD-02 und der spaetere Vergleich schlanker Views
mit LVGL unter OD-05 bleiben davon unabhaengig.

Native Tests der spaeteren Bereiche decken Standby, Startvorbereitung, aktiven
und abgeschlossenen beziehungsweise abgebrochenen Lauf, Recovery,
Produktfuehler vorhanden/fehlend, Luftsensor als Ersatz, ungueltigen oder
fehlenden Schutzsensor, veraltete Werte, Safetyfehler, quittierbare und nicht
quittierbare Meldungen, erlaubte/gesperrte Aktionen, Revisionskonflikte und
Serviceberechtigung ab. Fuer Sprachen werden alle Schluessel in allen drei
Katalogen, deutscher Fallback, nichtleere Texte, stabile Fehlercodes,
Temperatur-/Dauer-/Datum-/Zeitformatierung, unveraenderte benutzerdefinierte
Namen und das Fernhalten von Bibliotheks-/Treibertexten geprueft.

### 12. OD-07-Teilentscheid: Issue #26 in fuenf lokale UI-Bereiche schneiden

Issue #26 bleibt vollstaendig R1-relevant, weil das Touchdisplay die einzige
lokale Bedien- und Anzeigeoberflaeche ist. #19 sowie #25 bis #28 sind
verbindlich geschnitten; damit ist OD-07 insgesamt abgeschlossen. Der Audit
aendert #26 nicht und erstellt keine Folgeissues. Ein
spaeterer ownerfreigegebener Planungsschritt schneidet den Umfang in diese
Reihenfolge:

1. **Lokale Navigations- und Interaktionsbasis:** Screen- und Dialogzustand,
   eindeutige Vorwaerts-/Zuruecknavigation, Abbrechen, modale Bestaetigungen,
   Aufweckschutz, sprachunabhaengige Aktionskennungen, Sperrgruende sowie
   simulierte Touchaktionen. Grosse Touchziele, eindeutige Rueckwege und eine
   Bedienung ohne notwendige Wischgesten sind verbindlich. Ein Aufwecktouch
   loest kein Kommando aus; kritische Aktionen benoetigen eine bewusste
   Bestaetigung. Es entsteht weder ein zweiter fachlicher Zustandsautomat noch
   ein allgemeines Widget-, Layout-, Screen- oder UI-Pluginframework.
2. **Standby, Programmauswahl und Startablauf:** zustandsabhaengiger
   Hauptbildschirm, Programmliste mit klarer Factory-/Benutzertrennung,
   Startzusammenfassung, nur fuer den naechsten Lauf geltende Aenderungen,
   manueller Betrieb, Sensorstatus und Startbestaetigung. Antippen fuehrt zur
   Startzusammenfassung; es ueberschreibt kein gespeichertes Programm. Die UI
   zeigt typisierte Validierungs- und Safetyergebnisse und uebergibt an die
   bestehenden Kommandos. Den unveraenderlichen Lauf-/Programmsnapshot erzeugt
   die Fachlogik. Der Ablauf bleibt ohne WLAN vollstaendig nutzbar.
3. **Programmverwaltung und Editor:** Erstellen aus Vorlage oder leerem
   Entwurf, Name, Notizen, lokale Tastatur, Bearbeiten, Kopieren, Speichern,
   Factoryprogramm zuruecksetzen, zweistufig loeschen und Revisionskonflikte
   anzeigen. Die UI sammelt einen typisierten Entwurf, bildet den bestaetigten
   Ablauf ab und zeigt feldbezogene Fehler; sie dupliziert weder
   Programmvalidierung noch Factory-/Persistenzsemantik. Unvollstaendige
   Entwuerfe erscheinen nicht als startbereit, benutzerdefinierte Namen werden
   nicht automatisch uebersetzt.
4. **Lauf-, Meldungs-, Stop- und Wiederanlaufoberflaeche:** normale und
   technische Laufansicht, Temperaturen samt Qualitaet, Sollwert, Phase,
   Lauf-/Restzeit, Regelsensor und Ersatzbetrieb, Produkt-einsetzen-Ablauf,
   Warnungsbanner, Meldungsliste, Quittieren, gegebenenfalls Stummschalten,
   eindeutige bestaetigte Stopoptionen, Completed und Recoveryhinweise. Die UI
   sendet nur fachlich angebotene Kommandos und enthaelt keine Aktor-, Regel-
   oder Safetylogik. Quittieren beseitigt weder Ursache noch Fehler. Ein vom
   Fach-/Recoveryvertrag angeordneter automatischer Wiederanlauf wartet nicht
   auf die UI; sie informiert ueber den bereits erfolgten Ablauf.
5. **Einstellungen, Diagnose, Service und lokale Recovery-UI:** Screens,
   Navigation, Warnungen, Bestaetigungen, Eingaben und typisierte Status-/
   Sperrgruende fuer Einstellungen, passive Diagnose, Servicezugang,
   Aktortestdialoge, Wiederherstellungsmenue, Netzwerkeinstellungen,
   Factoryprogramme, Touchkalibrierungsablauf, normalen PIN-geschuetzten und
   PIN-unabhaengigen lokalen Vollreset, `SAFE_BOOT` und UART-Hinweise. PIN/KDF,
   Verifikation, Sperrzeiten, Sessions und Secretpersistenz bleiben bei OD-09;
   Safety und Aktortestgrenzen im Safety-Kern/Aktorplaner; Touchrohwerte,
   Controller und Kalibrierungsmessung bei #31; Kalibrierungspersistenz und
   Resetpolicy im Persistenz-/Recoveryvertrag; die atomare Resetmechanik bei
   #57. Eine Service-PIN hebt keine Safetypruefung auf.

#26 verwendet die kleinen Praesentationsmodelle, sprachunabhaengigen Codes und
DE-/ES-/EN-Ressourcen aus #25 und schafft keine konkurrierende gemeinsame
UI-Basis. Navigation und Dialoge bleiben in #26. Die hardwareunabhaengige
Screen-, Navigations-, Aktions- und Fehlerlogik wird mit simulierten Modellen
und Touchereignissen getestet. #31 liefert erst spaeter Display-/Touchadapter,
Rohwerte und reale Kalibrierung; OD-02 waehlt den Treiber, OD-05 vergleicht
danach schlanke Views mit LVGL. Weder konkrete Pixel/Fonts/Puffer noch LVGL oder
eine parallele Renderingabstraktion werden vorweggenommen.

Die simulierten Testgruppen umfassen Rueckwege, Abbruch ohne Teilwirkung,
Bestaetigung und Aufweckschutz; Standby, Factory-/Benutzerprogramm, manuellen
Start, Run-only-Aenderungen, Sensor- und Validierungsfehler sowie
Revisionskonflikte; Lauf, Produkt-einsetzen, Sensorersatz, Meldungen,
Verriegelung, Stop, Wiederanlauf, Completed und Recovery; ausserdem gesperrten
oder erlaubten Service, typisierte Authergebnisse, Safety-seitig verweigerten
Aktortest, Wiederherstellung, Kalibrierungsdialog, Resetbestaetigung,
`SAFE_BOOT` und UART-Hinweis. Sie ersetzen keine realen Display-, Touch-, Auth-,
Safety-, Persistenz- oder Aktortests.

Der vorhandene microSD-/SD-Karten-Slot erzeugt keinen R1-Scope: keine Menues,
Statusanzeige, Laufaufzeichnung, Backups/Imports, Diagnoseexporte, Updates,
Dateisystemadapter oder Persistenz. Das Geraet funktioniert vollstaendig ohne
SD-Karte; es entstehen weder Port, Adapter, Provider, Platzhalter noch Spike.

### 13. OD-07-Teilentscheid: Issue #27 in fuenf Webbereiche schneiden

Issue #27 bleibt als sekundaere lokale Weboberflaeche R1-relevant. Der
verbindliche Teilentscheid schneidet den spaeteren Umfang in fuenf kleine,
getrennt pruefbare Bereiche. Der Audit aendert #27 nicht und erstellt keine
Folgeissues. Die OD-09-Technik- und Integrationsnachweise bleiben Gates vor
produktiven schreibenden Endpunkten und realer Authentisierung.

1. **HTTP-Transport und interne API-Vertraege:** kleiner konkreter
   Serveradapter fuer Initialisierung, Routing, Methoden-, Content-Type-,
   Bodygroessen- und Timeoutpruefung sowie technische Fehlerabbildung. HTTP wird
   in begrenzte typisierte DTOs, fachliche Queries oder Kommandos und wieder in
   begrenzte Antworten uebersetzt. Ein simuliertes Backend, statische
   Testressourcen sowie Abbruch-, WLAN-, Jitter-, Watchdog- und
   Ressourcenpruefungen gehoeren zum Nachweis. Server-, Request-, Response-,
   Socket-, Connection- und Callbacktypen bleiben ausserhalb von
   `fermentation_app`, Safety, Persistenz, Sensorik/Regelung, gemeinsamen
   Praesentationsmodellen und fachlichen Ports. Es entsteht weder eine
   `IWebTransport`-, Middleware-, Stream-, Provider- oder Pluginplattform noch
   ein Dummy-Zweitadapter. Die interne API bleibt versioniert, begrenzt und
   getestet; R1 verspricht keine stabile oeffentliche externe Schreib-API,
   keine SDKs und keine Fremdclient-Langzeitkompatibilitaet.
2. **Nur lesender Status, begrenztes Polling und aktueller Laufchart:**
   vollstaendige begrenzte Snapshots liefern Geraete-/Laufzustand, Phase,
   Sollwert, freigegebene Temperaturwerte mit Qualitaet und Alter,
   tatsaechlichen Regelsensor, Ersatzbetrieb, Lauf-/Restzeit, Meldungen,
   Netzwerk-/Zeitstatus, freigegebenen Aktorstatus und das Alter des letzten
   gueltigen Snapshots. Pollingrequests ueberlappen nicht; der aktive Lauf darf
   tendenziell haeufiger, Standby langsamer und ein Browserhintergrund reduziert
   oder pausiert abfragen, mit kontrolliertem Backoff nach Fehlern. Konkrete
   Intervalle, Clientzahl, Antwortgroesse, Timeouts, Heap- und Jitterbudgets
   bleiben bis zum Prototyp offen. WebSocket und SSE werden nicht vorsorglich
   umgesetzt. Der aktuelle Chart enthaelt mindestens Produkttemperatur, soweit
   vorhanden, Luft- und Solltemperatur, Phasenwechsel, relevante Warnungs-/
   Unterbrechungsmarkierungen, sichtbare Messluecken und Sensorwechsel. Seine
   Punktzahl ist fest begrenzt beziehungsweise verdichtet; persistente
   Historie bleibt #19-B und gehoert nicht zu #28.
3. **Schreibende Kommandos und Revisionskonflikte:** Start, Stop, Quittieren,
   zulaessige Laufaktionen, Programm- und Einstellungsveraenderungen verwenden
   dieselben fachlichen Kommandos wie das Display. HTTP-Handler veraendern
   keinen Geraetezustand direkt. Jede Mutation traegt die erwartete Revision,
   veraltete Daten werden ohne Last-write-wins oder globale Bearbeitungssperre
   abgelehnt, und der aktuelle Stand wird erneut geladen. Wiederholte
   Browserrequests duerfen abgeschlossene Aktionen nicht unkontrolliert
   doppelt ausfuehren; die Bedienquelle wird protokolliert. Safety darf jedes
   Webkommando ablehnen. Produktive schreibende Endpunkte werden erst nach den
   erfolgreichen technischen Auth-/CSRF-/Credential-/Ressourcen-, Webserver-
   und JSON-Gates freigegeben; OD-09 allein ist keine Produktivfreigabe.
4. **Responsive lokale Webassets:** lokale, versionierte und groessenmaessig
   begrenzte HTML-, CSS- und JavaScript-Assets bilden Programme, Lauf,
   Einstellungen, Meldungen, Diagnose, System, Login und Service auf mobilen
   und breiten Ansichten ab. Es gibt kein CDN und keinen Cloudzwang. Die
   Weboberflaeche verwendet die gemeinsamen #25-Praesentationsmodelle, besitzt
   aber eigene Routen, Navigation und Browserhistorie; Touchnavigation wird
   nicht wiederverwendet. JavaScript leitet keine Fach-, Safety-, Sensorrollen-
   oder Berechtigungsentscheidung her. Schlanke eingebettete Assets sind
   `FIRST_EVALUATION_DIRECTION`; ein Frontendframework ist nicht ausgewaehlt
   und wird nur bei einem konkreten R1-Nachweis untersucht.
5. **Anmeldung, Sessions, CSRF und Servicefreigabe gemaess OD-09:** Dieser Teil
   setzt den in Abschnitt 15 entschiedenen Authvertrag um. Produktive
   schreibende Endpunkte bleiben hinter dem KDF-, Zufalls-, Ressourcen- und
   Authintegrationsnachweis. Eine dauerhafte Anmeldung ist kein R1-Scope;
   Webpasswort und Service-PIN, serverseitige fluechtige Sessions, die
   sitzungsgebundene Servicefreigabe, globale Sperrpolicy, CSRF und
   vorwaertsgerichtete Credentialwechsel bleiben getrennte Vertraege.

Die Kandidatenstatus bleiben eindeutig: Der lokale HTTP-Dienst ist
`REQUIREMENT_DECIDED`; Arduino-ESP32 `WebServer` ist
`FIRST_EVALUATION_CANDIDATE`, `SPIKE_REQUIRED` und
`FINAL_SELECTION_PENDING`. `ESPAsyncWebServer` ist
`CONDITIONAL_FALLBACK`/`EVALUATE_LATER`. ArduinoJson `7.4.3` und WiFiManager
sind jeweils erster beziehungsweise bevorzugter Evaluationskandidat,
`SPIKE_REQUIRED` und `FINAL_SELECTION_PENDING`. Der WiFiManager-Arbeitsbereich
aus OD-06 bleibt vom normalen Weblebenszyklus getrennt; technische
Basiskomponenten duerfen geteilt werden, nicht fachliche Zustandsautomaten.

R1 sieht keinen direkten Internetbetrieb und keine Portfreigabe als
Normalbetrieb vor. Lokales HTTP dient dem vertrauenswuerdigen lokalen Netz;
Fernzugriff erfolgt nur ueber externe sichere Infrastruktur wie VPN oder einen
TLS-Reverse-Proxy. Das Geraet bleibt vom Heimserver unabhaengig. WLAN- oder
Webserververlust beendet keinen Lauf, und Webanfragen koennen Aktoren nur ueber
die normalen fachlichen Kommando- und Safety-Pfade beeinflussen. Secrets
gelangen weder in Logs, Assets, Exporte noch Fehlerantworten. Dieser
Teilentscheid waehlt keine TLS-, Server-, JSON-, Onboarding- oder
Frontendbibliothek endgueltig aus.

### 14. OD-07-Teilentscheid: Issue #28 in vier Diagnosebereiche schneiden

Issue #28 bleibt fuer Release 1 relevant, ist aber als ein einzelner
Implementierungs-PR zu breit. Der verbindliche Teilentscheid schneidet den
spaeteren Umfang in vier kleine, getrennt pruefbare Bereiche. Der Audit aendert
#28 nicht und erstellt keine Folgeissues. Mit diesem Entscheid ist OD-07
fachlich vollstaendig entschieden. OD-09 ist in Abschnitt 15 ebenfalls
fachlich entschieden; seine technischen Auswahl- und Messgates bleiben vor
produktiver Authentisierung offen.

1. **Passive Diagnosemodelle und Boot-Selbsttest:** nur lesende, typisierte
   Projektionen fuer rohe, korrigierte und gefilterte Sensorwerte, Qualitaet und
   Alter, verwendeten Regelsensor/Ersatzbetrieb, Reglerstatus, geplanten und
   soweit verfuegbar bestaetigten Aktorzustand, Warnungen, Fehler,
   Verriegelungen, Konfiguration, Speicher, Netzwerk, Zeit, Firmware/Build und
   Resetursache. Fehlende Werte werden nie als `0` erfunden, Treiberfehler und
   fachliche Sensorqualitaet bleiben getrennt, und UI oder Diagnose lesen keine
   GPIOs oder Treiber direkt. Der passive Boot-Selbsttest schaltet keine
   produktiven Aktoren ein, umgeht keine Hardwarefreigabe und veraendert keinen
   Laufzustand.
2. **Ressourcen- und Gesundheitsdiagnose:** freier und niedrigster freier Heap,
   groesster freier Heapblock soweit sinnvoll verfuegbar, statisches RAM und
   Buildinformationen, Firmwaregroesse, Flash-/Partitionsstatus,
   Persistenz-/Journal-/Historienauslastung, Reset-/Watchdog-/Stabilitaets-
   ereignisse sowie begrenzte Fehlerzaehler und Diagnosepuffer. Rohmessung und
   daraus abgeleiteter Warnstatus bleiben unterscheidbar; die zentrale Fehler-
   und Safetylogik entscheidet ueber Auswirkungen. Schwellen, Reserven,
   Partitionen und Budgets bleiben bis zur realen Messung
   `MEASUREMENT_REQUIRED` und sind keine Produktgarantie.
3. **Gefuehrter Serviceablauf:** Auswahl, Voraussetzungen und Sperrgrund,
   bewusste Bestaetigung, kontrollierter Start, Fortschritt, sicherer Abbruch,
   Ergebnis und Rueckkehr in einen sicheren Zustand werden zuerst vollstaendig
   mit Mocksensoren/-aktoren simuliert. Authentisierung und Safety bleiben
   getrennte Gates; eine Service-PIN hebt keine Safetypruefung auf. Veraendernde
   Tests sind bei aktivem oder wiederherstellbarem Lauf gesperrt, reale
   Hardwarezugriffe folgen den normalen Fach-/Safety-/Aktorpfaden und reale
   Aktortests bleiben hinter #24 und den jeweiligen Hardwaregates. KDF,
   PIN-Hashing, Sessions, Cookies, CSRF, Lockout, Secretpersistenz, GPIO- und
   Treiberadapter sowie neue Safetylogik gehoeren nicht in diesen Bereich.
4. **Nur lesender Diagnose- und Servicebericht:** ein versionierter, typisierter
   Fachbericht fuer Firmware/Build, Resetursache, redigierte Konfiguration,
   Sensor-/Qualitaets-/Regelsensorstatus, Fehler/Verriegelungen, Ressourcen,
   Speicher, Netzwerk/Zeit, Serviceergebnis und begrenzte relevante Ereignisse.
   Redaction erfolgt vor der Serialisierung. Passwoerter, PINs, Verifier,
   Salts/Pepper, Session-/CSRF-Tokens, Credential-/Authentication-Roots,
   ungefilterte Eingaben, interne Rohrecords, Speicheradressen und geheime
   Payloads sind ausgeschlossen. #28 definiert den Bericht; die generische
   JSON-/CSV-/Download-/Streaminginfrastruktur stammt aus #19-C. Ein
   abgebrochener Export veraendert keinen Zustand, und Diagnoseberichte werden
   nicht importiert.

Die Verantwortungsgrenzen sind verbindlich: Der aktuelle pollende Laufchart
gehoert zu #27-B, persistente Laufhistorie und verdichtete historische
Messreihen zu #19-B, und die gemeinsame nur lesende Exportinfrastruktur zu
#19-C. Konkrete Touch- und Webansichten bleiben #26 beziehungsweise #27;
Authentisierung bleibt OD-09/#27-E. Reale Aktorfreigabe, Safetyverriegelungen,
GPIO, Treiber und thermische Grenzwerte bleiben #24 und den Hardwareissues
zugeordnet. UART bleibt Entwicklungs-, Recovery- und qualifizierter technischer
Serviceweg, nicht normales Endbenutzerfeature.

Der fachliche Diagnoseumfang ist `REQUIREMENT_DECIDED`. Eine technische
Implementierung bleibt dort `FINAL_SELECTION_PENDING`, wo Evaluation noetig
ist; Ressourcenbudgets sind `MEASUREMENT_REQUIRED` und reale Hardwarefreigaben
`HARDWARE_GATE_PENDING`. Es wird weder eine Diagnose-, Chart-, Logging-,
Telemetrie-, Metrics- noch Exportbibliothek ausgewaehlt und keine allgemeine
Plattform dafuer geplant. Ein externer Codec bleibt hinter den bereits
dokumentierten JSON-/Transport-Spike-Gates.

Die spaetere Abnahme prueft pro Bereich mindestens:

- #28-A gueltige, fehlende, veraltete und ungueltige Sensorwerte,
  unterscheidbare Roh-/Korrektur-/Filterwerte, Regelsensorwechsel,
  Ersatzbetrieb, Warnung/Fehler/Verriegelung, lesende Diagnose im aktiven Lauf
  sowie Bootfaelle mit und ohne verfuegbare Konfiguration, Sensoradapter,
  Speicher, Zeit und Netzwerk ohne Aktoransteuerung;
- #28-B normale/fehlende Metriken, Warnstatus, begrenzte Zaehler,
  Ueberlaufschutz sowie Reset/Wiederaufbau ohne ungepruefte Reservegarantie;
- #28-C getrennte Auth-/Safetyablehnung, aktiven und wiederherstellbaren Lauf,
  Bestaetigung, Abbruch, Hardwarefehler, Neustart und sichere Rueckkehr,
  zunaechst vollstaendig mit Mocks;
- #28-D vollstaendige und optionale Berichtsfelder, Redaction, Ausschluss von
  Secrets, stabile Schemaversion, feste Datengrenzen, abgebrochene
  Serialisierung und nachgewiesene Zustandsfreiheit des nur lesenden Exports.

### 15. OD-09: Authentisierung, Sessions, CSRF und Secret-at-rest

OD-09 ist als fachlicher R1-Vertrag entschieden. Technische Kandidaten und
Messwerte bleiben bewusst hinter Spikes: PBKDF2-HMAC-SHA-256 aus der fixierten
mbedTLS-/ESP32-Toolchain ist `FIRST_EVALUATION_CANDIDATE`, `SPIKE_REQUIRED` und
`FINAL_SELECTION_PENDING`; Iterationszahl, Verifikationsdauer, Stack-/Heapbudget
und Scheduling werden erst nach reproduzierbaren Messungen festgelegt. Eine
einzelne schnelle SHA-256-Pruefung ist unzulaessig. Der Vergleich abgeleiteter
Pruefnachweise muss zeitkonstant erfolgen. Initial werden mindestens 128 Bit
zufaelliger Salt, ein 256-Bit-Pruefnachweis und eine explizite
KDF-Parameterkennung beziehungsweise KDF-Schemaversion evaluiert. Ein Pepper
ist ohne einen vom normalen Flash getrennten geschuetzten Geraeteschluessel
keine belastbare Schutzgrenze.

R1 trennt das normale Webpasswort und die genau vierstellige Service-PIN ohne
Benutzerkonto- oder Rollenmodell. Beide besitzen eigene zufaellige Salts,
einseitige Pruefnachweise und getrennte Credentialrecords; Klartext und
Credentialwerte gelangen nie in Logs, Diagnose, Exporte, Backups oder
Fehlermeldungen. Es gibt keine feste Factory-PIN. Die PIN wird beim ersten
produktiven Credentialkonsumenten gesetzt, nicht angezeigt oder
zurueckgewonnen und kann bei Vergessen nur ueber den bestaetigten lokalen
vollstaendigen Recovery-/Werksreset zurueckgesetzt werden. Normale
Webanmeldung oder Netzwerk duerfen sie nicht zuruecksetzen. Die konkrete
physische Recoverygeste bleibt dem bestaetigten Recovery-/Touchvertrag
ueberlassen. Eine gueltige PIN hebt keine Safety-, Laufzustands- oder
Hardwarepruefung auf.

Die globale Fehlversuchsbegrenzung gilt pro Credential, nicht pro Browser,
IP-Adresse, Session oder Tab:

- Webpasswort: nach fuenf aufeinanderfolgenden Fehlern 30 Sekunden Sperre;
  weitere Fehlversuchsbloecke verdoppeln bis hoechstens 15 Minuten;
- Service-PIN: nach drei aufeinanderfolgenden Fehlern 30 Sekunden Sperre;
  weitere Fehlversuchsbloecke verdoppeln bis hoechstens 30 Minuten;
- der vollstaendige sicherheitsrelevante Zustand aus Vor-Sperr-
  Fehlversuchszaehler, Sperrstufe, aktivem Sperrzustand, Credential-Epoche
  beziehungsweise -Generation und Integritaet ist neustartfest. Nach einer
  falschen Pruefung wird der neue Zustand zuerst berechnet, atomar persistiert
  und validiert; erst danach endet die Fehlerantwort. Ein beantworteter
  Fehlversuch darf durch Neustart nicht verschwinden;
- waehrend einer aktiven Sperre erfolgen weder KDF-Arbeit noch ein Write pro
  abgewiesener Anfrage. Ohne vertrauenswuerdige UTC beginnt nach Neustart
  mindestens die volle zuletzt persistierte Sperrdauer erneut. Eine
  erfolgreiche Pruefung setzt Zaehler und Sperrstufe atomar zurueck;
- Persistenzfehler ergeben niemals `fail open`. Das konkrete Slot-/Wear-Modell
  bleibt Implementierungs- und Messgate;
- KDF-Arbeit wird begrenzt und serialisiert; Regelung, Safety, Watchdog und
  Stabilitaet haben Vorrang.

Normale Sessions werden serverseitig, fluechtig und begrenzt im RAM gehalten.
Ein ESP32-/Geraeteneustart verwirft alle Sessions. Ein reiner Browserneustart
ist dagegen kein garantiertes Widerrufsereignis: Stellt der Browser das
Sitzungscookie wieder her, bleibt die Session hoechstens bis zum
Inaktivitaets-/Absolutlimit oder einem serverseitigen Widerruf gueltig. Die
opake Sessionkennung enthaelt
mindestens 128 Bit kryptografisch zufaelligen Inhalt und keine Credential- oder
Rollendaten; sie erscheint nie in URL, Log, Export, Diagnose, `localStorage`
oder `sessionStorage`. R1 muss mindestens vier gleichzeitige normale
Browsersessions tragen; die endgueltige Obergrenze folgt der Ressourcenmessung.
Inaktivitaet beendet eine Session nach 30 Minuten, die absolute Hoechstdauer
betraegt 12 Stunden. Logout, Passwortaenderung, Werksreset oder Wechsel der
Credential-Epoche widerrufen die betroffenen Sessions. `Angemeldet bleiben`,
persistente Login-/Refresh-Tokens, persistente Browsergeraete und Login ueber
einen ESP32-/Geraeteneustart gehoeren nicht zu R1.

ADR-017 registriert diesen Entscheid gegenueber der bisherigen Anforderung in
`WEB_UI.md` als akzeptierte hoeherrangige Entscheidung.

Der normale Webpasswortschutz ist empfohlen und bei der Ersteinrichtung
vorausgewaehlt, darf aber bewusst nach Warnung deaktiviert werden. Ohne
Passwortpruefung erzeugt der Server weiterhin eine begrenzte fluechtige
anonyme lokale Session mit mindestens 128 Bit kryptografisch zufaelliger
Kennung, derselben Cookiepolicy, sitzungsgebundenem CSRF-Token sowie denselben
Session- und Ressourcenlimits. Sie enthaelt keine Benutzeridentitaet oder
Rolle und wird beim Neustart verworfen. Eine dauerhafte sichtbare Warnung
erklaert, dass jedes Geraet im erreichbaren lokalen Netz normale Funktionen
bedienen kann. Normale Mutationen behalten Session-, CSRF-, Methoden-,
Content-Type-, Origin-/Referer-/Fetch-Metadata-, Revisions-, Konflikt-,
Wiederholungs-, Fach- und Safetygates. Servicefunktionen verlangen zusaetzlich
Service-PIN-KDF, neustartfeste PIN-Sperre, sitzungsgebundene Servicefreigabe
und Safety-/Hardwaregates.

Der Wechsel vom aktivierten zum deaktivierten Passwortschutz verlangt eine
gueltige Session, CSRF, erwartete Revision sowie ausdrueckliche Warnung und
Bestaetigung und widerruft danach alle alten Sessions. Der Rueckwechsel setzt
ein neues Passwort atomar und widerruft alle anonymen Sessions.

Das Sessioncookie verwendet `HttpOnly`, `SameSite=Strict`, `Path=/` und kein
`Domain`-Attribut. `Secure` wird gesetzt, sobald der konkrete Zugriffsweg HTTPS
verwendet. Direkter lokaler HTTP-Betrieb bietet keinen behaupteten Schutz gegen
Mitschneiden im lokalen Netz; OD-09 waehlt weder TLS-Bibliothek noch
Zertifikatsloesung.

Eine Servicefreigabe entsteht erst nach gueltiger Service-PIN innerhalb genau
einer fluechtigen normalen oder anonymen lokalen Websession. Sie gilt weder
global noch fuer andere Browser, endet
nach 5 Minuten Inaktivitaet oder absolut nach 15 Minuten und wird mit der
zugrunde liegenden Session, bei Logout oder PIN-Aenderung widerrufen. Kritische Aktionen
verlangen weiterhin eine eigene Bestaetigung; eine spaetere konkrete Aktion
darf erneute PIN-Eingabe fordern. Authfreigabe und Safetyfreigabe bleiben
getrennte Gates: Session, PIN, zeitlich begrenzte Servicefreigabe, konkrete
Aktion sowie Zustands-/Safety-/Hardwarepruefung werden nacheinander geprueft.

Der CSRF-Vertrag verbietet Zustandsaenderungen ueber `GET`, verlangt die
vorgesehene Methode und fuer JSON-Endpunkte den vorgesehenen Content-Type und
erlaubt weder allgemeines CORS noch Wildcard-Origin. Jede normale oder anonyme
lokale Session
besitzt einen serverseitig zugeordneten CSRF-Token mit mindestens 128 Bit
kryptografischer Zufallsguete. Die Weboberflaeche sendet ihn bei jeder Mutation
im Header `X-CSRF-Token`; fehlende, falsche oder fremde Tokens werden ohne
Teilwirkung abgelehnt. Der Token steht nie in URL, Log, Cookie, Export oder
Diagnose. `Origin` wird gegen den erwarteten Zielorigin geprueft, bei fehlendem
`Origin` entsprechend `Referer`; eindeutig fremde
`Sec-Fetch-Site: cross-site`-Anfragen werden abgelehnt, soweit der Header
vorhanden ist. `SameSite=Strict` ist nur eine zusaetzliche Schutzschicht.
Fachliche Revisions- und CSRF-Pruefung bleiben getrennte Gates.

Sessionkennungen, CSRF-Tokens und Salts stammen aus einem geeigneten
kryptografischen Zufallspfad. Standard-C-PRNG, Zeitstempel, MAC-Adresse oder
Zaehler allein sind unzulaessig; ein Fehler fuehrt zur sicheren Ablehnung.
`esp_fill_random()` oder ein korrekt gesaeter mbedTLS-DRBG ist
`FIRST_EVALUATION_DIRECTION` und `SPIKE_REQUIRED`, der endgueltige
Integrationspfad bleibt offen.

Secret-at-rest unterscheidet persistente und fluechtige Kategorien sowie einen
ausdruecklichen R1-Ausschluss:

- persistente Credentialdaten: Webpasswort- und PIN-Verifier, eigene Salts,
  KDF-Parameterkennung, Credential-Epoche/-Generation, Vor-Sperr-
  Fehlversuchszaehler, Sperrstufe, aktiver Sperrzustand sowie Integritaets- und
  atomare Commitinformationen;
- persistente wiederverwendbare Connectivity-Secrets: WLAN-Passwort und
  gegebenenfalls als
  nichtoeffentlich behandelte SSID, die fuer Wiederverbindung lesbar bleiben;
- fluechtige Secrets: Sessionkennungen, CSRF-Tokens, Servicefreigaben und kurze
  Authaktionszustaende, die beim Geraeteneustart verworfen werden;
- nicht in R1: persistente Session-, Remember-me-, Login-, Refresh- oder
  Browsergeraetetokens und dauerhafte Sessionfortsetzung.

Ohne aktivierte und getestete NVS-/Flashverschluesselung wird kein Schutz gegen
physischen Flashzugriff behauptet. Gesalzene Verifier schuetzen nicht
automatisch wiederverwendbare Secrets. Plattformverschluesselung ist
`EVALUATE_BEFORE_RELEASE` in einem separaten Security-Spike; Partitionierung,
Provisionierung, Schluesselverwaltung/-verlust, Entwicklungs- und
Produktionsflash, Recovery, Werksreset, Updatepfad, Ressourcen und reale
Schutzgrenze werden vor einem ausdruecklichen spaeteren Ownerentscheid
geprueft. Der Audit aktiviert nichts und legt kein Schluesselmodell fest.
Dieses `EVALUATE_BEFORE_RELEASE`-Gate muss vor #37 abgeschlossen und vom Owner
entschieden sein. Zulaessig sind entweder die produktive Auswahl mit
Provisionierungsprozess sowie Recovery-/Regressionstests vor #37 oder eine
begruendete Nichtauswahl mit dokumentierten Rest-Risiken, klaren Schutzgrenzen
und ausdruecklicher Ownerfreigabe. Der Audit nimmt dieses Ergebnis nicht vorweg.

#57 erzeugt keine leeren Authentication-Manifeste, Credentialslots oder
Authentication-Roots. Die reale Authentisierungsdomaene entsteht erst mit dem
ersten produktiven Credentialkonsumenten. Sie verwendet stark typisierte,
versionierte Records und eine eigene vorwaertsgerichtete Credential-Epoche,
liegt weder in `UserConfiguration` noch `ProgramCatalog` und wird nicht in
allgemeine Backups aufgenommen. Ein atomarer Credentialwechsel bestimmt zuerst
die Zielgeneration, erzeugt Salt und Pruefnachweis samt KDF-Parameterkennung,
schreibt und validiert den vollstaendigen Record, committet dann atomar die neue
Epoche und widerruft alte Sessions und Servicefreigaben. Vor dem Commit bleibt
das alte Credential wirksam; nach dem Commit bleibt die neue Epoche kanonisch
und darf durch Fallback oder Recovery nie auf eine alte Epoche zurueckfallen.
Cut-Point-, Korruptions-, Wiederholungs- und Widerrufstests sind verpflichtend.

Der Authspike prueft Toolchainbuild und bekannte KDF-Testvektoren, getrennte
Salts/Parameter, richtige und falsche Passwort-/PIN-Pruefungen, zeitkonstanten
Vergleich, Laufzeit, Stack, Heap, niedrigsten freien Heap und groessten freien
Heapblock, parallele Anfragen, Regelzyklus-Jitter, Watchdog, globale
Fehlversuchsserien, Neustartpersistenz sowie Credentialwechsel an allen
Cut-Points. Eine Alternative wird nur bei einem konkret nachgewiesenen
R1-Problem untersucht; Policyaenderungen gehen erneut an den Owner.

OD-09 entscheidet die Securitypolicy, nicht die technische Produktfreigabe.
Im passwortgeschuetzten Normalbetrieb bleiben produktive Mutationen gesperrt,
bis KDF und Work Factor ownerfreigegeben, Zufallspfad, atomare
Credentialpersistenz, vollstaendige neustartfeste Fehlversuchs-/Sperrpersistenz,
Session/Cookie, CSRF samt Methoden-, Content-Type-, Origin-/Referer- und
Fetch-Metadata-Pruefung, Revision/Konflikt/Wiederholung, Widerruf sowie
Ressourcen-, Jitter-, Watchdog-, Abbruch-, Neustart-, Webserver- und
JSON-Nachweise bestanden sind. Im bewusst passwortlosen Modus entfaellt nur
die normale Passwort-KDF; die anonyme Session und alle uebrigen einschlaegigen
Gates bleiben erforderlich. Servicefunktionen verlangen zusaetzlich den
bestandenen Service-PIN-KDF-/Sperrnachweis und die Safety-/Hardwaregates.

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
- Erhaltung der geraetespezifischen Touchkalibrierung gemaess ADR-010;
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
  wenn er vorhanden und verwendbar ist, darf in Stillstand und zulaessigem Lauf
  fehlen und wird elektrisch von den festen Sensorbussen isoliert; der
  Raum-/Luftsensor ist der regulaere Ersatz-Regelsensor; der
  Kuehlkoerper-/Peltier-Schutzsensor ist verpflichtende Sicherheitsgrundlage,
  deren fehlendes, ungueltiges, veraltetes oder nicht ausreichend
  vertrauenswuerdiges Signal jede Peltierfreigabe sperrt;
- Peltier ueber BTS7960, Innen-/Aussenluefter und Summer;
- deterministischer Safety-, PI-, Aktor-, Prozess- und Laufkern;
- Touchdisplay 320 x 240 als einzige lokale Bedien- und Anzeigeoberflaeche,
  eine sekundaere Weboberflaeche, kleine gemeinsame oberflaechenneutrale
  Praesentationsmodelle sowie Deutsch, Spanisch und Englisch mit Deutsch als
  Fallback; Touch- und Webnavigation bleiben getrennt; die lokale Navigation,
  Start-/Programmbedienung, Lauf-/Meldungsbedienung und Service-/Recovery-UI
  bleiben ohne WLAN vollstaendig nutzbar;
- WLAN-Onboarding mit zuerst begrenzt geprueftem WiFiManager als bevorzugtem
  Kandidaten und einem nur bei dokumentiertem Ausloeser nachgezogenen lokalen
  Frameworkadapter als Rueckfall; primaer ein WLAN-Credential-QR mit sichtbarem
  manuellem Portaladress-Rueckfall; geschuetztes Ersatz-WLAN als eigener
  Netzwerklebenszyklus bei langem Heim-WLAN-Ausfall; lokale Zeit/NTP und
  Zeitzonenanzeige;
- PIN-unabhaengiger lokaler Raw-Touch-Boot-Recoverypfad im ersten
  Boot-/`SAFE_BOOT`-Fenster; genaue Geste/Schwellen bleiben `TBD_HARDWARE`, die
  Hardwaretauglichkeit `SPIKE_REQUIRED`;
- begrenzte externe JSON-Vertraege mit ArduinoJson `7.4.3` als bevorzugtem,
  noch spikepflichtigem Kandidaten; interne atomare Persistenz bleibt binaer;
- versionierte Konfiguration, Laufpersistenz, Recovery, ein typisiertes
  begrenztes Ereignisjournal und eine begrenzte verdichtete Laufhistorie;
- Variante-B-Konfigurationsaktivierung mit Active-/Fallback-Graph, fluechtiger
  validierter Vorschau, sicherem Bootstrap, `StorageEpoch` und
  wiederaufnehmbarem Werksreset;
- lokale Authentication und die mit ihren ersten realen Konsumenten
  spezifizierten Secrets, Diagnose, nur lesende Exporte und secret-freies
  Backup sowie vollstaendig validierte, atomar aktivierte Imports;
- UART/FT232RL als Update- und letzter Recoveryweg;
- Hardware-, thermische und siebentaegige Releaseabnahme.

### Nicht Release 1

- Web-OTA, duale OTA-Slots und automatischer Firmwaredownload;
- Bluetooth/BLE als Produktfunktion;
- Cloud-, Push- oder Telegram-Pflicht;
- PID-Autotuning und aktive Kaskadenregelung;
- LVGL ohne belegten R1-Vorteil;
- Tuerkontakt, verpflichtende RTC, 12-V-ADC, Lueftertacho;
- microSD-/SD-Karten-Menues, -Persistenz, -Import/-Export oder -Updatepfade;
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
6. Display-/Touchtreiber erst nach den Stufen 0 bis 4 und
   DS18B20-/1-Wire-Treiber erst nach den Sensorstufen 1 bis 3 fixieren. Beim
   Sensorpfad Softwarestack und Topologie A/B getrennt entscheiden; Produktbus
   immer separat und Topologie C nicht produktiv planen.
7. Produktive Aktoradapter und reale Aktortests weiterhin erst nach den
   zugeordneten Safety-Gates freigeben.
8. #19 nach dem entschiedenen OD-07-Teilschnitt in Journal/Retention,
   Laufhistorie/Bereinigung, Laufexport/secret-freies Backup und erst danach
   Importvorschau/atomare Aktivierung zerlegen; #25 in kleine
   Praesentationsmodelle und gemeinsame Sprach-/Formatierungsressourcen
   schneiden; #26 in die fuenf entschiedenen lokalen UI-Bereiche zerlegen;
   #27 in die fuenf entschiedenen Webbereiche zerlegen; #28 in passive
   Diagnose/Boot-Selbsttest, Ressourcen-/Gesundheitsdiagnose, gefuehrten
   Serviceablauf und nur lesenden Diagnose-/Servicebericht schneiden.
9. ArduinoJson erst nach isoliertem Build-, Grenzwert-, Fuzz- und
   Ressourcennachweis an der kleinen DTO-/Codecgrenze uebernehmen; die
   1-/4-/16-KiB-Profile und Tiefe 6 mit realen maximalen DTOs pruefen.
10. Jede spaetere Drittkomponente mit Version, Lizenz, Abhaengigkeiten,
   Base-/Head-Messung und Hardwarestatus im Komponentenregister nachfuehren.

## Erkannte Ueberdimensionierungen

| Bereich | Befund | Empfohlene Korrektur ausserhalb dieses Audits |
|---|---|---|
| #29 | minimale sichere Spikebaseline und spaetere produktive Hardwareintegration sind in einem breiten Issue verbunden | nach Auditfreigabe in Baseline-Anteil vor den Spikes und produktiven Anteil hinter den jeweiligen Safety-Gates schneiden; Issue im Audit nicht aendern |
| #56/#57 | bestehender Umfang mischt den notwendigen Active-/Fallback- und Bootstrapkern mit Pending-/Intentpfaden und Secret-Domaenen ohne ersten Konsumenten | nach entschiedenem OD-01 in separatem Planungs-/ADR-Schritt auf Variante B zuschneiden; Variante A spaeter additiv planen |
| #19 | vier grosse Verantwortungsbereiche in einem Issue | nach Auditfreigabe separat in Journal/Retention, begrenzte Laufhistorie/Bereinigung, nur lesenden Laufexport/secret-freies Backup und Importvorschau/atomare Aktivierung schneiden; 5 detaillierte Laeufe und 50 Zusammenfassungen bleiben Messziel |
| #25 | gemeinsame Praesentationssemantik, Sprachressourcen, Navigation und Layout sind vermischt | nach Auditfreigabe in kleine oberflaechenneutrale Praesentationsmodelle und gemeinsame Sprach-/Formatierungsressourcen schneiden; Navigation/Layout nach #26/#27 verschieben, Issue im Audit nicht aendern |
| #26 | lokale Navigation, Start, Programmeditor, Lauf-/Meldungsbedienung sowie Service-/Recovery-UI sind in einem Issue verbunden | nach Auditfreigabe in die fuenf entschiedenen UI-Bereiche schneiden; hardwareunabhaengige Logik nativ/simuliert vor #31 testen, Auth-/Safety-/Resetmechanik ausserhalb belassen und keinen SD-Scope schaffen |
| #27 | HTTP-Transport/API, Status/Polling/aktueller Laufchart, Mutationen, Webassets und Authentisierung sind fuer einen PR zu breit | nach Auditfreigabe in die fuenf entschiedenen Bereiche schneiden; Onboarding bleibt OD-06, produktive Mutationen bleiben bis zum erfolgreichen technischen Auth-/CSRF-/Credential-/Ressourcen-, Webserver- und JSON-Nachweis gesperrt, Issue im Audit nicht aendern |
| #28 | passive Diagnose, Ressourcenueberwachung, Serviceablauf, Bericht, Charts, Historie und Exportmechanik sind vermischt | nach Auditfreigabe in vier Bereiche schneiden: passive Diagnose/Boot-Selbsttest, Ressourcen-/Gesundheitsdiagnose, gefuehrter Serviceablauf und nur lesender Diagnose-/Servicebericht; aktuellen Chart nach #27-B, Historie nach #19-B und Exportinfrastruktur nach #19-C abgrenzen |
| LVGL | vollstaendiges UI-Framework fuer wenige feste 320-x-240-Screens waere vorsorglich und ist kein Treiberkandidat | erst nach Treiberauswahl, Adaptervertrag und identischem repraesentativem Screen gegen schlanke Views messen |
| ESPAsyncWebServer | Async-/WebSocket-/SSE-Umfang koennte groesser als der reale R1-Bedarf sein | ersten `WebServer`-Kandidaten messen; Async nur bei belegtem Vorteil |
| Vorsorgliche Mehradapter-/Provisioningarchitektur | zwei produktive Portalwege oder allgemeine Provider-/Pluginvertraege waeren ohne zweiten realen Bedarf ueberdimensioniert | WiFiManager zuerst begrenzt pruefen; Frameworkadapter nur bei dokumentiertem Ausloeser als identischen Gegenprototyp nachziehen |
| Eigener JSON-Parser oder allgemeine JSON-Providerarchitektur | Standardparser-, UTF-8-, Escape-, Zahlen-, Ressourcen- und Serialisierungsprobleme wuerden ohne zweiten realen Codecbedarf dupliziert | ArduinoJson `7.4.3` zuerst begrenzt pruefen; Fachschema selbst validieren, Alternative nur bei belegtem Problem |
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
| OD-02 | Display-/Touchtreiberstack in Stufe 4 nach gestuftem Hardwarevergleich waehlen |
| OD-03a | DS18B20-/1-Wire-Softwarestack nach Stufe 3 waehlen |
| OD-03b | Topologie A oder begruendeten Rueckfall B nach realem Pin-/GPIO- und Fehlerisolationsvergleich waehlen; Produktbus bleibt separat, C ausgeschlossen |
| OD-05 | LVGL als `EVALUATE_LATER` erst nach Treiberwahl, Adaptervertrag und identischem repraesentativem Screen-/Ressourcenvergleich gegen schlanke eigene Screens pruefen; nur bei klarem R1-Vorteil auswaehlen, andernfalls `DEFER_AFTER_R1` |

Die fachlichen Ownerentscheidungen der aktuellen Auditliste sind damit
vollstaendig bearbeitet. Die Tabelle enthaelt nur noch mess- und
evaluationsabhaengige Auswahlentscheide. OD-09 ist entschieden; PBKDF2,
Work Factor, Zufallspfad und Plattformverschluesselung bleiben technische
Spike-Gates und duerfen nicht als endgueltig ausgewaehlt gelten.

OD-01 ist mit Variante B entschieden. Offen bleibt nur die technische
Detailpruefung, ob Dokumentrevisionen und Rootsequenz die bisherige Funktion
einer eigenstaendigen persistenten `MutationSequence` vollstaendig abdecken.
Sie ist keine erneute Auswahl zwischen Variante A und B.

OD-04 ist als Evaluationsrichtung entschieden: Der kleine lokale HTTP-Dienst
ist `REQUIREMENT_DECIDED`, Arduino-ESP32 `WebServer` ist
`FIRST_EVALUATION_CANDIDATE`, `SPIKE_REQUIRED` und bedingte Produktivrichtung.
Die endgueltige Auswahl bleibt `FINAL_SELECTION_PENDING`.
`ESPAsyncWebServer` wird als `CONDITIONAL_FALLBACK`/`EVALUATE_LATER` nur bei
einem konkreten offenen R1-Risiko und unter identischen Bedingungen geprueft;
eine vorsorgliche Gleichwahl besteht nicht.

OD-06 ist als Richtungsentscheid ebenfalls entschieden: WiFiManager ist der
bevorzugte Release-1-Kandidat und wird zuerst begrenzt geprueft. Die
endgueltige Kandidatenwahl bleibt ein Spike-Gate; der kleine Frameworkadapter
wird nur bei einem dokumentierten Ausloeser als identischer Gegenprototyp
nachgezogen.

Der JSON-Richtungsentscheid ist ebenfalls getroffen, ohne ein neues OD-Kuerzel
zu vergeben: ArduinoJson `7.4.3` ist der bevorzugte Kandidat. Offen bleibt nur
die endgueltige Uebernahme nach dem dokumentierten Build-, Grenzwert-, Fuzz-
und Ressourcenspike; eine vorsorgliche Gleichwahl besteht nicht.

OD-07 ist fachlich vollstaendig entschieden: #19, #25, #26, #27 und #28 sind
in kleine, ownerfreizugebende Umsetzungsbereiche geschnitten. Dieser Auditstand
aendert oder erstellt keine Issues. OD-09 legt nun den fachlichen Authvertrag
fest; produktive schreibende Webendpunkte und authentisierte Serviceablaeufe
bleiben hinter seinen technischen Spike- und Integrationsgates.

Vor einer Mergefreigabe muessen das bereits vorliegende Zwischenreview
vollstaendig eingearbeitet und der konsolidierte Stand erneut unabhaengig
geprueft werden. Der PR bleibt bis dahin Draft.

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
- [`HARDWARE_SPIKE_PLAN.md`](HARDWARE_SPIKE_PLAN.md) – gestufte Display-/Touch-,
  DS18B20-/Topologie-, WLAN-Onboarding-, JSON-Codec-, Auth-/Security- sowie Speicher-/
  Retention-/Bereinigungsnachweise
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
