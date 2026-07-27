# Vorgeschlagene Release-1-Roadmap

Zur Auditnavigation: [`RELEASE_1_ADOPT_OR_BUILD_AUDIT.md`](RELEASE_1_ADOPT_OR_BUILD_AUDIT.md).

## Status

Auditentwurf, Ownerfreigabe ausstehend. Diese Roadmap ersetzt weder
`docs/IMPLEMENTATION_PLAN.md` noch bestehende Issues oder akzeptierte ADRs. Sie
zeigt eine risikoorientierte Reihenfolge und die Stellen, an denen der aktuelle
Backlog vor Umsetzung verkleinert oder geteilt werden sollte.

## Leitende Reihenfolge

```text
Auditfreigabe und Spezifikations-/Issue-Bereinigung
  -> minimale sichere ESP32-Hardwarebaseline
  -> aktorfreie Display-/Touch-, DS18B20-/1-Wire-, Webserver-,
     WLAN-Onboarding- und JSON-Codec-Spikes
  -> Bibliotheksentscheidungen und produktive Adapter

parallel zur Baseline und zu den Spikes:
#20 -> #21 -> #22 -> #23 -> #24

erst nach den jeweiligen Safety-Gates:
produktive Aktoren -> thermische Abnahme -> Releasegate
```

Der Display-/Touchpfad ist innerhalb des aktorfreien Spikes weiter gestuft:
reale Hardware identifizieren, alle drei Hauptkandidaten durch Quellen-/Lizenz-
und Buildpruefung fuehren, ausreichend erfolgreiche Kandidaten kurz auf der
Hardware pruefen und nur nach bestandenem Smoke-Test die vollstaendige
identische Matrix ausfuehren. Danach entscheidet der Owner ueber bevorzugten
Treiber und Rueckfallkandidat. Reservekandidaten werden nur bei dokumentiertem
Ausloeser nachgezogen.

## Phase 1: Auditfreigabe und verbindlicher Persistenz-Neuschnitt

Vor weiterer Architekturverbreiterung:

1. Auditdokumente fachlich freigeben oder korrigieren.
2. Den entschiedenen OD-01-Vertrag in einem separaten ownerfreigegebenen
   Planungs-/ADR-Schritt in Spezifikation, #16, #56 und #57 ueberfuehren. Der
   Audit-PR aendert diese Quellen nicht.
3. #56 auf Active/Fallback, Graphvalidierung, fluechtige Vorschau,
   Konfliktschutz und Runtime-Publish reduzieren; #57 auf Bootstrap,
   `StorageEpoch`, Korruptionssperre und wiederaufnehmbaren Werksreset.
4. Persistentes Pending sowie echte Connectivity-/Authentication-Domaenen als
   spaetere eigene Arbeit mit ihrem ersten fachlichen Konsumenten planen, nicht
   als leere R1-Infrastruktur.
5. Abhaengigkeiten von #17/#24 auf #16 anhand der tatsaechlich benoetigten
   schmalen Variante-B-Vertraege korrigieren.
6. Grundsatz bestaetigen, dass Treiber/Frameworkdienste adoptiert und
   Safety-/Fachlogik selbst entwickelt werden.
7. den verbindlichen OD-07-Teilschnitt von #19 in Journal/Retention,
   Laufhistorie/Bereinigung, nur lesenden Laufexport/secret-freies Backup und
   Importvorschau/atomare Aktivierung in einem separaten ownerfreigegebenen
   Planungsschritt umsetzen; #25 ebenso in oberflaechenneutrale
   Praesentationsmodelle und gemeinsame Sprach-/Formatierungsressourcen
   schneiden; #26 in lokale Navigation, Start, Programmeditor,
   Lauf-/Meldungsbedienung und Service-/Recovery-UI schneiden; #27 in
   HTTP-Transport/API, Status/Polling/Laufchart, schreibende Kommandos,
   responsive Webassets und Authentisierung nach OD-09 schneiden; #28 bleibt
   als letzter OD-07-Teil separat zu entscheiden.

Keine Empfehlung aus dem Audit wird im Audit-PR selbst implementiert.

## Phase 2: zwingende hardwareunabhaengige Safety-, Persistenz- und Fachlogik

Diese Ketten laufen parallel zur minimalen Hardwarebaseline und zu den
aktorfreien Display-/Touch- und Sensorspikes. Sie muessen nicht zuerst
abgeschlossen werden:

```text
#20 Sensorqualitaet
  -> #21 Regelsensorauswahl
  -> #22 PI und Luftbegrenzung
  -> #23 Aktorplaner

Persistenzbasis #54/#55
  -> separater Variante-B-Neuschnitt #16/#56/#57
  -> #56 Active/Fallback und atomarer Runtime-Publish
  -> #57 Bootstrap, StorageEpoch und Werksreset

schmale benoetigte Variante-B-Vertraege
  -> #17 Laufpersistenz
  -> #18 Wiederanlauf

#17 + stabiler Variante-B-Kern
  -> #19-A typisiertes Ereignisjournal und Retention
  -> #19-B begrenzte Laufhistorie und stromausfallsichere Bereinigung
  -> #19-C nur lesender Laufexport und secret-freies Backup
  -> #19-D Importvorschau und atomare Aktivierung

#17 + #20..#23 + bestehender Laufkern
  -> #24 Fehlerkern und SAFE_BOOT
  -> #25-A kleine oberflaechenneutrale Praesentationsmodelle
  -> #25-B gemeinsame Sprachressourcen und Formatierungsregeln
  -> #26-A lokale Navigation und Interaktion
  -> #26-B Standby, Programmauswahl und Start
  -> #26-C Programmverwaltung und Editor
  -> #26-D Lauf, Meldungen, Stop und Wiederanlauf
  -> #26-E Einstellungen, Diagnose, Service und Recovery-UI

Webserver-/JSON-Spikes und spaetere Ownerauswahl
  -> #27-A HTTP-Transport und interne API-Vertraege
  -> #27-B Status, begrenztes Polling und aktueller Laufchart
  -> #27-D responsive lokale Webassets

OD-09
  -> #27-C schreibende Kommandos und Revisionskonflikte
  -> #27-E Anmeldung, Sessions, CSRF und Servicefreigabe
```

#26-A bis #26-E bezeichnen den spaeter ownerfreizugebenden Schnitt, keine im
Audit erstellten Issues. Ihre Screen-, Dialog-, Aktions- und Fehlerlogik ist
nativ beziehungsweise mit simulierten Touchereignissen pruefbar. Reales
Rendering und Touchintegration folgen #31/OD-02; der Frameworkvergleich folgt
OD-05, reale Authentisierung OD-09 und die Resetmechanik dem #57-Vertrag.

Vorgeschlagene kleine PRs:

- #20: Status-/Plausibilitaetsmodell, danach Filterpipeline;
- #21: optionalen Produktfuehler als primaeren Regelsensor, regulaeren
  Raum-/Luft-Ersatzsensor und sichere Rueckkehr modellieren; der verpflichtende
  Kuehlkoerper-/Peltier-Schutzsensor bleibt unabhaengige Freigabegrundlage;
- #22: begrenzter PI-Kern, danach Luftbegrenzung/Diagnose;
- #23: Peltierplaner, danach Luefter/Nachlauf;
- #17: Kontrollpunktcodec/-slots, danach Ereignis-/Rueckfallservice;
- #18: phasenbezogener Restart, danach Zeit-/Fortschrittskorrektur;
- #19-A: stabile Ereignistypen, Prioritaeten, feste Recordgrenzen und native
  Retention-/Recoverytests; keine einzelne Journalisierung jeder periodischen
  Temperaturmessung und keine Secrets;
- #19-B: verdichtete Messreihen, Zusammenfassungen, feste Grenzen und
  wiederaufnehmbare Bereinigung; aktiver Lauf und Recovery, kritisches
  Safety-/Recoveryjournal, Zusammenfassungen und Komfortdetails in dieser
  Schutzreihenfolge;
- #19-C: begrenzte JSON-/CSV-Laufexporte und secret-freies Backup nur lesend,
  gestreamt oder stueckweise;
- #19-D: Importkandidat, Vollvalidierung, Vorschau, Konfliktpruefung,
  Bestaetigung und atomare Aktivierung erst nach stabiler OD-01-Basis;
- #24: Fehlerdatenmodell, persistente Verriegelung/Boot, danach
  Fehlerinjektionsmatrix;
- #25-A: kleine ansichtsbezogene Projektionen aus kanonischem Fach-, Prozess-,
  Sensorqualitaets-, Safety- und Berechtigungszustand mit Qualitaet/Alter,
  Meldungen, Aktionsverfuegbarkeit, Sperrgruenden und Revisionen; keine
  Mega-View und keine neue UI-Fachentscheidung;
- #25-B: stabile Textschluessel, sprachunabhaengige Fehler-/Meldungscodes,
  vollstaendige Deutsch-/Spanisch-/Englisch-Kataloge, deutscher Fallback und
  gemeinsame semantische Temperatur-/Dauer-/Datum-/Zeit-/Einheitenformatierung;
  benutzerdefinierte Namen bleiben unveraendert;
- #26-A: Screen-/Dialogzustand, Rueckweg, Abbruch, Bestaetigung, Aufweckschutz,
  Aktionskennungen und Sperrgruende ohne notwendige Wischgesten nativ testen;
- #26-B: Standby, Factory-/Benutzerprogrammliste, Startzusammenfassung,
  Run-only-Aenderungen, manuellen Start und Sensor-/Fehlerdarstellung;
- #26-C: typisierte Entwuerfe erstellen, kopieren, bearbeiten, speichern,
  zuruecksetzen und zweistufig loeschen; Validierung/Konflikte nur anzeigen;
- #26-D: Lauf-/Detailansicht, Meldungen, Quittieren, bestaetigte Stopoptionen,
  Sensorersatz, Completed und automatischen Wiederanlauf abbilden;
- #26-E: Einstellungen, passive Diagnose, Service- und Recoverydialoge nativ
  vorbereiten; Auth-, Safety-, Kalibrierungs-, Persistenz- und Resetmechanik
  bleiben ausserhalb und werden erst an ihren jeweiligen Gates integriert.
- #27-A: kleinen konkreten HTTP-Adapter, interne versionierte DTO-Vertraege,
  feste Grenzen und simuliertes Backend ohne Servertypen im Fachkern pruefen;
- #27-B: vollstaendige begrenzte Statussnapshots, nicht ueberlappendes Polling
  und einen punktbegrenzten aktuellen Laufchart umsetzen; Intervalle,
  Clientzahl, Antwortgroesse und Budgets erst messen;
- #27-D: schlanke lokale responsive HTML-/CSS-/JavaScript-Assets ohne CDN,
  Frontendframeworkvorwahl oder Fach-/Safetylogik im Browser;
- #27-C/#27-E: erst nach OD-09 erwartete Revisionen, Konflikt- und
  Doppelwirkungsschutz sowie Anmeldung, Sessions, CSRF und Servicefreigabe
  umsetzen. R1 verspricht keine oeffentliche externe Schreib-API.

Fuer den neu geschnittenen Variante-B-Kern gilt die feste Transaktionsfolge:

1. Kandidat erzeugen;
2. technisch und fachlich validieren;
3. fallible Runtimewerte und Ressourcen vorbereiten;
4. Active-/Fallback-Graph persistent ueber den kanonischen Root committen;
5. den unveraenderlichen Runtime-Snapshot veroeffentlichen.

Die R1-Vorschau bleibt fluechtig. Ihre Bestaetigung prueft die erwartete aktive
Basisgeneration, einen unveraenderten Kandidaten und erneut die vollstaendige
Validierung. Der Bootstrap akzeptiert nur nachweislich fabrikneuen,
fehlerfrei lesbaren Speicher. Korruption oder unbekannte Schemas fuehren nie zu
stillem Factory-Fallback. Cut-Point-, Korruptions-, Schema-, Migrations- und
Ressourcentests decken diesen R1-Kern direkt ab.

Hardwareparameter bleiben `TBD_COMMISSIONING`; Mocks behaupten keine reale
Thermik. Bibliothekstypen und reale GPIOs gelangen nicht in #20–#24;
Treiberstatus und Fachstatus bleiben strikt getrennt.

## Phase 3: Hardwarebasis und aktorfreie Spikes

Nach Audit- und Planungsbereinigung, aber ohne auf den Abschluss von #20–#24 zu
warten:

1. Den minimalen Baseline-Anteil von #29 nachweisen: reale Boardrevision,
   UART/FT232RL, reproduzierbares Flashen/Booten/Resetten, reale Flashgroesse,
   sichere Versorgung, fixierte Toolchain, Betrieb ohne PSRAM, Firmware-/RAM-/
   Heapbaseline sowie GPIO-/Businventar.
2. Peltier, BTS7960, Innen-/Aussenluefter, MOSFET-Verbraucher und Summer physisch
   trennen oder nachweislich inaktiv halten. Der Summer wird nicht angesteuert.
3. Beim Display-/Touch-Spike zuerst die reale Modulvariante, Controller, Pegel,
   Leitungen und Bootzustaende identifizieren. Danach alle drei Hauptkandidaten
   durch Stufe 1, ausreichend erfolgreiche Kandidaten durch den kurzen
   Hardware-Smoke-Test der Stufe 2 und nur `PASS_SMOKE_TEST`-Kandidaten durch
   die vollstaendige identische Matrix der Stufe 3 fuehren. Stufe 4 benennt
   bevorzugten Treiber und Rueckfallkandidat. Reservekandidaten bleiben
   bedarfsabhaengig. Details stehen im
   [`HARDWARE_SPIKE_PLAN.md`](HARDWARE_SPIKE_PLAN.md).
4. Beide DS18B20-/1-Wire-Stacks gestuft pruefen: Stufe 1 Quelle/Lizenz/Build,
   Stufe 2 Sensorsmoke-Test mit einem realen Sensor und nur nach Erfolg Stufe 3
   mit identischer Topologie- und Fehlermatrix. Ein nicht mit der fixierten
   Toolchain reproduzierbar baubarer Espressif-Kandidat endet als
   `INCOMPATIBLE_WITH_CURRENT_TOOLCHAIN`, nicht als allgemein ungeeignet.
   Softwarestack und Bustopologie getrennt entscheiden: Produktfuehler immer
   separat, Topologie A mit drei Bussen bevorzugt, Topologie B mit gemeinsamem
   festen Bus als pinabhaengiger Rueckfall und Topologie C hoechstens als
   negativer Referenztest. Die konkrete TRS-Buchse und Hot-Plug-Schutzmassnahmen
   werden praktisch geprueft; drei GPIOs und Bauteilwerte werden nicht vorab
   festgelegt.
5. Den `FIRST_EVALUATION_CANDIDATE` Arduino-ESP32 `WebServer` in einem kleinen
   aktorfreien, `SPIKE_REQUIRED`-Baselineprototyp
   fuer statische Ressourcen, begrenzte API-/Import-/Exportpfade, wenige
   Clients, Abbruch-, WLAN-, Jitter- und Ressourcenmessung vorbereiten, ohne
   #27 vorwegzunehmen. Die endgueltige Produktivauswahl bleibt
   `FINAL_SELECTION_PENDING`. `ESPAsyncWebServer` ist
   `CONDITIONAL_FALLBACK`/`EVALUATE_LATER` und wird nur dann mit identischem
   Umfang nachgezogen, wenn der Baselineprototyp ein konkretes Release-1-Risiko
   offen laesst.
6. Davon fachlich getrennt, aber mit dem Webserver-Baselineprototyp koordiniert,
   WiFiManager zuerst als begrenzten WLAN-Onboardingkandidaten pruefen:
   Toolchain, Quellen/Lizenz/Webassets, ausdruecklicher Portalstart, reale
   Android-/iOS-/Windows-Clients, direkter IP-Rueckfall, Credential-Erhalt,
   Secret-/Fehler-/Lifecyclegrenzen, Cut-Points, Jitter und Ressourcen. Einen
   Adapter aus `WiFi`, `DNSServer`, SoftAP und `WebServer` nur bei einem
   dokumentierten WiFiManager-Problem mit identischem Umfang nachziehen. Dieser
   Spike setzt weder #27 um noch nimmt er eine endgueltige Bibliothekswahl
   vorweg.
7. ArduinoJson `7.4.3` als `FIRST_EVALUATION_CANDIDATE`, `SPIKE_REQUIRED` und
   `FINAL_SELECTION_PENDING` in
   einem kleinen aktorfreien Prototyp pruefen: isolierter Build, konkrete
   DTO-/Codecgrenze, initiale Profile 1/4/16 KiB, Verschachtelungstiefe 6,
   String-/Array-/Feld-/Schemagrenzen, Importvorschau ohne Aktivierung,
   Streaming/Pagination, reproduzierbare Negativ-/Fuzztests sowie ESP32-
   Ressourcen-, Laufzeit- und Jittermessung. Dieser Spike wird mit dem
   `WebServer`-Baselineprototyp und dem spaeteren Schnitt von #19/#27/#28
   koordiniert, implementiert aber keines dieser breiten Issues. Eine
   Alternative wird nur bei einem konkret belegten R1-Problem untersucht.
8. Fuer den spaeteren #19-B-Schnitt reale Speicher- und Ressourcenmessungen
   planen: NVS-/Partitionskapazitaet, Fuellen bis zur Bereinigung,
   wiederholte Journal-/Historienzyklen sowie Cut-Points vor, waehrend und nach
   Bereinigungsfortschritt. Das Ziel aktiver Lauf plus 5 detaillierte Laeufe
   und 50 Zusammenfassungen wird dabei gemessen und erst danach verbindlich
   dimensioniert; produktive Aktoren sind dafuer nicht erforderlich.

Die minimale Baseline legt weder finale Pins, Partitionierung, Bibliotheken,
Sensorbustopologie, Aktoradapter, Safety-Grenzen noch PI-Parameter fest. Der
Audit empfiehlt, #29 spaeter in diesen Baseline-Anteil und den produktiven
Hardwareanteil zu schneiden; er aendert #29 nicht.

Hardwaregate: keine Peltier-, H-Bruecken-, Innen-/Aussenluefter-, MOSFET- oder
Summerfreigabe in den Display-/Sensor-/Netzwerkspikes. Der Abschluss von #24
bleibt Gate fuer produktive Aktoradapter und reale Aktortests, nicht fuer diese
aktorfreien Bibliotheksevaluationen.

## Phase 4: Bibliotheksentscheidungen

Der Owner waehlt anhand der jeweils geforderten gestuften und identischen
Messungen:

- in Display-/Touch-Stufe 4 genau einen Treiberstack und einen
  Rueckfallkandidaten aus den gestuft verbliebenen Kandidaten;
- genau einen DS18B20-/1-Wire-Stack und einen Rueckfallkandidaten nach Stufe 3;
- davon getrennt Topologie A oder den begruendeten Rueckfall B nach realem
  GPIO-/Pin- und Fehlerisolationsvergleich; der Produktfuehler bleibt immer auf
  eigenem Bus und Topologie C bleibt ausgeschlossen;
- WiFiManager als bevorzugten Onboardingkandidaten endgueltig uebernehmen oder
  bei dokumentiertem Spikeausloeser den identischen kleinen
  Frameworkgegenprototyp bewerten und danach den Owner entscheiden lassen;
- ArduinoJson `7.4.3` als bevorzugten Kandidaten erst nach bestandenem
  Build-, Grenzwert-, Fuzz- und Ressourcennachweis endgueltig uebernehmen;
  Alternative nur bei dokumentiertem Problem.

Jede Auswahl erhaelt Version/Commit, Lizenznachweis, Build-/Hardwaremessung,
Adaptervertrag und ein eigenes umsetzendes Issue/PR. Keine Auswahl nur aufgrund
von Sternen, Marketing oder README-Beispielen.

LVGL ist keine Treiberoption dieser Phase. Der UI-Frameworkentscheid folgt erst
nach Treiberauswahl, schmalem Adaptervertrag und einem repraesentativen
Release-1-Screen.

Beim Webserver besteht keine offene Gleichwahl: Der kleine lokale HTTP-Dienst
ist `REQUIREMENT_DECIDED`; Arduino-ESP32 `WebServer` ist der
`FIRST_EVALUATION_CANDIDATE`, `SPIKE_REQUIRED` und die bedingte
Produktivrichtung. Die endgueltige Auswahl bleibt `FINAL_SELECTION_PENDING`.
Nur wenn sein begrenzter Prototyp eine konkrete R1-Anforderung nicht stabil und
ressourcengerecht erfuellt, wird der `CONDITIONAL_FALLBACK`
`ESPAsyncWebServer` unter identischen Bedingungen evaluiert. Ein
vollstaendiger Zweifachprototyp ist keine Pflicht.

## Phase 5: produktive Adapter

Kleine adapterbezogene PRs:

1. NVS-/Preferences-Adapter fuer den vorhandenen `IStateStore`, sofern nicht
   bereits in einem zuvor ownerfreigegebenen Persistenzpaket enthalten;
2. #30 DS18B20-/1-Wire-Adapter hinter `ITemperatureSource`;
3. #31 Display- und Touchadapter hinter getrennten schmalen Grenzen;
4. WLAN-, Zeit-/Zeitzonenadapter und nach bestandenem Server-Spike genau ein
   kleiner konkreter lokaler HTTP-Adapter; die Zeile waehlt `WebServer` noch
   nicht produktiv aus;
5. nach bestandenem OD-06-Spike genau eine konkrete Onboardingintegration;
   WiFiManager bleibt technischer Portalbaustein, der Frameworkadapter bleibt
   ein nur bei dokumentiertem Ausloeser gepruefter Rueckfall;
6. nach bestandenem JSON-Spike einen kleinen konkreten ArduinoJson-Codec nur an
   begrenzten API-, Konfigurations-, Programm-, Diagnose-, Export-,
   secret-freien Backup- und Importgrenzen; interne Persistenz bleibt binaer.

Bibliothekstypen duerfen weder in `fermentation_app` noch in Safety- oder
Prozessmodelle durchsickern. Das gilt insbesondere fuer `JsonDocument`,
`JsonObject`, `JsonArray`, `JsonVariant` und ArduinoJson-Fehler in Fach-,
Persistenz- und gemeinsamen View-Modellen. Jeder Adapter uebersetzt Fehler und
Limits vollstaendig und besitzt eine Mock-/Hostgrenze. Es entsteht keine
allgemeine `IJsonProvider`-, Codec-Plugin- oder Zweitcodecarchitektur.

Der Webserveradapter kapselt nur Initialisierung, Routenbindung, feste
Request-/Responsegrenzen, Timeouts und HTTP-Uebersetzung. DTOs, fachliche
Queries/Kommandos, Validierung, Authpolicy, Konfliktsemantik, Persistenz und
Safety bleiben serverunabhaengig. Es entsteht keine allgemeine
`IWebTransport`-, Stream-, SSE-, WebSocket-, Middleware- oder Pluginhierarchie.
Ein spaeterer Serverwechsel ersetzt diese konkrete ESP32-Integrationsschicht
ueber die Composition Root, sofern dieselben begrenzten Endpunkt-/DTO-Vertraege
erfuellt bleiben und ein konkreter Vorteil nachgewiesen ist; ein Dummy-
Zweitadapter wird nicht erstellt.

Nach dem Display-/Touchadapter wird ein repraesentativer Release-1-Screen als
gemeinsame Grundlage fuer den spaeteren Vergleich schlanker Views mit LVGL
festgelegt. Dies ist keine erneute Treiberwahl.

## Phase 6: Hardware-Bring-up

Verbindliche Reihenfolge:

1. nach der bereits bestandenen minimalen Baseline den spaeteren produktiven
   #29-Anteil mit finaler Partitionierung und bestaetigter produktiver
   Boardkonfiguration abschliessen;
2. #30 Sensoren und #31 Display/Touch auf Basis der gewaehlten Kandidaten
   produktiv integrieren;
3. erst nach den zugeordneten Safety-Gates #32 Luefter, den Summer als einziges
   zusaetzliches lokales Ausgabeelement
   und MOSFET-Kanaele einzeln;
4. #33 BTS7960 ohne Peltier;
5. erst danach #33 begrenzte Peltierpulse mit Sicherung, montierter einmaliger
   Temperatursicherung, gueltigem und ausreichend vertrauenswuerdigem
   Kuehlkoerper-/Peltier-Schutzsignal, einem gemaess Laufvertrag verwendbaren
   Regelsensor, Lueftern und bestaetigten Pegeln.

Ein bestandener Bibliotheksspike ist keine Freigabe fuer reale Aktoren.

## Phase 7: Web- und Bedienintegration

Nach stabilen Fachvertraegen und den relevanten Adapterentscheiden:

- die nativ getesteten #25-A-/#25-B-Vertraege fuer Touch und Web verwenden,
  ohne Layout-, Navigations-, HTML-, Treiber-, Widget-, ArduinoJson- oder
  Webservertypen einzumischen; Display- und Browsersprache bleiben unabhaengig;
- auf dem ausgewaehlten Display-/Touchtreiber und seinem schmalen
  Adaptervertrag denselben repraesentativen Screen, dieselben Texte,
  Eingabeelemente und Messmethoden fuer schlanke Views und LVGL verwenden;
  LVGL nur bei einem klar gemessenen Vorteil uebernehmen;
- #26 als einzige lokale Bedien- und Anzeigeoberflaeche in der entschiedenen
  Reihenfolge A Navigation/Interaktion, B Standby/Programmauswahl/Start,
  C Programmverwaltung/Editor, D Lauf/Meldungen/Stop/Wiederanlauf und
  E Einstellungen/Diagnose/Service/Recovery-UI umsetzen. Die Logik bleibt
  ohne WLAN vollstaendig nutzbar und wird nativ mit simulierten Touchaktionen
  getestet; kein Aufwecktouch loest ein Kommando aus, keine Bedienung setzt
  Wischgesten voraus und die UI leitet keine Fach-, Safety-, Auth-, Persistenz-
  oder Aktorentscheidung neu her;
- reale Display-/Touchintegration erst nach #31 und OD-02 anbinden, danach den
  identischen repraesentativen Screen unter OD-05 fuer schlanke Views und LVGL
  vergleichen; Auth erst nach OD-09 und atomare Resetmechanik nur ueber #57
  integrieren. Der microSD-/SD-Karten-Slot erzeugt weder R1-UI noch Adapter,
  Persistenz, Import-/Exportweg oder Spike;
- #27 nach dem entschiedenen Fuenferschnitt umsetzen: zuerst #27-A kleiner
  HTTP-Transport/interne API, dann #27-B vollstaendige begrenzte
  Statussnapshots, nicht ueberlappendes Polling und aktueller punktbegrenzter
  Laufchart, danach #27-D schlanke responsive lokale Webassets. Polling ist die
  Funktionsrichtung; konkrete Intervalle, Clientzahl, Antwort-/Chartgroesse,
  Timeouts, Heap und Jitter bleiben bis zur Messung offen. WebSocket/SSE und ein
  Frontendframework werden nicht vorsorglich eingefuehrt. #27-C schreibende
  Kommandos mit erwarteten Revisionen, Konflikt-/Doppelwirkungsschutz und #27-E
  Authentisierung/Sessions/CSRF/Service folgen erst nach OD-09. Es gibt keine
  Last-write-wins-Strategie, globale Bearbeitungssperre oder versprochene
  oeffentliche externe Schreib-API;
- OD-06-Onboarding getrennt von #27 nach dem begrenzten WiFiManager-Spike
  umsetzen: kein Portalstart bei gewoehnlichem temporaerem WLAN-Ausfall, neue
  Zugangsdaten bis zum Nachweis nur als Kandidat behandeln und den bisherigen
  funktionierenden Stand bei Fehler, Timeout oder Abbruch erhalten; den
  Frameworkadapter nur bei dokumentiertem Ausloeser identisch vergleichen;
- vor Authentication OD-09 festlegen: KDF/Work-Factor, Sitzungsdauer,
  Sperrzeiten, CSRF und At-rest-Grenze;
- Connectivity- und Authentication-Domaenen erst mit den ersten realen WLAN-,
  Passwort- oder PIN-Nachweisen spezifizieren; keine vorbereiteten leeren
  Manifeste, Roots oder `CredentialEpoch` aus #57 uebernehmen;
- #28 teilen in passive Diagnose, Exporte/Diagrammdaten und aktiven
  Serviceablauf; grosse Daten begrenzen, paginieren oder streamen;
- #19 in der entschiedenen Reihenfolge umsetzen: typisiertes Journal/Retention,
  begrenzte verdichtete Laufhistorie/stromausfallsichere Bereinigung, nur
  lesender Laufexport/secret-freies Backup und erst danach Importvorschau/
  atomare Aktivierung. Interne Journale und Kontrollpunkte bleiben binaer,
  und ein JSON-Import aktiviert nie direkt aus dem Parser. Der Werksreset
  bleibt bei #57; Erhalt oder Loeschung der Touchkalibrierung wird als
  separater zentraler Resetpolicyentscheid geklaert;
- fuer #19/#27/#28 die initialen JSON-Bodyprofile 1 KiB, 4 KiB und 16 KiB sowie
  Tiefe 6 gegen die realen maximalen DTOs pruefen; Root-, String-, Array-,
  Feld-, Werte- und Schemaversiongrenzen pro Vertrag festlegen und nur nach
  fachlicher Begruendung und neuer Messung erhoehen.

Regelung und Safety muessen unter Web-, Export- und Netzwerklast
deterministisch bleiben. Request-, Antwort-, JSON-Tiefen-, String-, Array-,
Upload-, Zeit- und Parallelitaetsgrenzen gelten unabhaengig vom Server.
Langsame oder abgebrochene Clients und WLAN-Verlust werden kontrolliert
behandelt; sie stoppen weder Lauf noch Safety-Kern. Parsererfolg ersetzt keine
Schema-, Berechtigungs-, Konflikt- oder Fachvalidierung. HTTP ist nur fuer das
vertrauenswuerdige lokale Netz; Cloud und Internet-Portfreigabe bleiben
ausgeschlossen.

## Phase 8: Inbetriebnahme und Release

```text
#34 Sensorvergleich und Thermik
  -> #35 PI-/Safetyparameter
  -> #36 Hardware-/Fehlerinjektions- und Programmabnahme
  -> #37 sieben Tage Belastung und Releaseentscheid
```

Jedes Gate verwendet versionierte Messdaten, Hardwarestand, Firmwarecommit und
Konfigurations-/Tuningrevision. Kein gelungenes Fermentationsprodukt ersetzt
eine Safety- oder Hardwareabnahme.

## Zurueckzustellen

Bis nach Release 1:

- Web-OTA, duale Firmware-Slots und automatisches Rollback;
- Bluetooth/BLE als Produktfunktion;
- Cloud, Push, Telegram und direkter Fernzugriff;
- PID-Autotuning und Kaskadenregelung;
- LVGL, sofern der erst nach Treiberwahl und Adaptervertrag durchgefuehrte
  identische Screenvergleich keinen zwingenden R1-Vorteil zeigt;
- Tuerkontakt, RTC-Pflicht, 12-V-ADC und Lueftertacho;
- Variante-A-Funktionen bis zu ihrem ersten echten Konsumenten: persistentes
  Pending/Pending-Root, Aktivierungsintent,
  `ConfigurationActivationRunAssessment`, persistente Preview-Metadaten,
  vorbereitete Connectivity-/Authentication-Manifeste, Authentication-Roots,
  `CredentialEpoch`, Secret-Rootwechsel und kombinierte
  Konfigurations-/Secret-Transaktionen;
- vorsorgliche Ports, Puffer und Bibliotheken fuer diese Funktionen.

### Additiver Ausbaupfad zu Variante A

Der spaetere Ausbau verwendet neue Recordtypen, neue Manifest-/Root-
Schemaversionen und explizite Copy-Migrationen. Active/Fallback bleibt die
gemeinsame Basis; `StorageEpoch` bleibt die gemeinsame Resetgrenze. Bestehende
R1-Schema-1-Daten werden weder umgedeutet noch in-place migriert. Der Schluessel-
und Recordraum bleibt kollisionsfrei erweiterbar, ohne ungenutzte Schluessel,
Slots, Ports, Dummyrecords oder Zukunftsservices vorzubereiten.

## Dauerhaft nicht Bestandteil dieses Projekts

Encoder, Programmwahlschalter, Start-/Stop-Taster und Status-LED werden nicht
auf eine spaetere Release verschoben, sondern gehoeren verbindlich nicht zu
diesem Fermentationsprojekt. Dafuer werden keine Ports, GPIO-Zuordnungen,
Adapter oder vorsorglichen Interfaces geplant. Der 230-V-AC-Hauptschalter
schaltet das ganze Geraet elektrisch ein oder aus und ist kein Firmwareeingang.
Das Touchdisplay bleibt die einzige lokale Bedien- und Anzeigeoberflaeche, die
Weboberflaeche bleibt sekundaer und der Summer das einzige zusaetzliche lokale
Ausgabeelement.

## Zu verkleinern, zu ersetzen oder zu schliessen

| Bestehendes Issue | Vorschlag nach Ownerfreigabe |
|---|---|
| #29 | spaetere Aufteilung empfehlen: minimaler Baseline-Anteil mit Board/UART/Flash/Boot/Reset/Ressourcen/GPIO-/Businventar und sicher inaktiven Aktorpfaden vor den Spikes; produktiver Hardwareanteil mit finaler Partitionierung, Pins, Adaptern, Verbrauchern und Abnahmen erst nach den jeweiligen Gates; Issue im Audit nicht aendern |
| #16 | als Tracking behalten, aber nach dem Audit in separatem Planungs-/ADR-Schritt auf den Variante-B-Kern und schmale Abhaengigkeiten neu schneiden; keine Direktimplementierung |
| #56 | nicht unveraendert freigeben; separat auf Active/Fallback, Graphvalidierung, fluechtige Vorschau, Konfliktschutz und Runtime-Publish reduzieren |
| #57 | nicht unveraendert freigeben; separat auf Bootstrap, `StorageEpoch`, Korruptionssperre und wiederaufnehmbaren Werksreset reduzieren |
| spaeteres Pending/Secrets | erst mit neustartpflichtigem Konfigurationswert beziehungsweise realen WLAN-/Passwort-/PIN-Nachweisen als eigene Issues planen; Variante A additiv anbinden |
| #19 | nicht streichen; nach Auditfreigabe in vier Bereiche schneiden: A Journal/Retention, B begrenzte Laufhistorie/stromausfallsichere Bereinigung, C nur lesender Laufexport/secret-freies Backup, D Importvorschau/atomare Aktivierung; 5 detaillierte Laeufe/50 Zusammenfassungen sind Messziel, kein Versprechen; Issue im Audit nicht aendern |
| #25 | nach Auditfreigabe auf zwei Bereiche reduzieren: A kleine oberflaechenneutrale Praesentationsmodelle, B gemeinsame DE/ES/EN-Sprachressourcen und semantische Formatierung mit deutschem Fallback; Touch-/Webnavigation und Layout nach #26/#27 verschieben, keine Mega-View oder Frameworktypen, Issue im Audit nicht aendern |
| #26 | nach Auditfreigabe in fuenf Bereiche schneiden: A Navigation/Interaktion, B Standby/Programmauswahl/Start, C Programmverwaltung/Editor, D Lauf/Meldungen/Stop/Wiederanlauf, E Einstellungen/Diagnose/Service/Recovery-UI; nativ/simuliert vor Hardwareintegration testen, #25 verwenden, #31/OD-02, OD-05, OD-09 und #57 nicht vorwegnehmen; kein allgemeines UI-Framework und kein SD-Scope; Issue im Audit nicht aendern |
| #27 | nach Auditfreigabe in fuenf Bereiche schneiden: A HTTP-Transport/interne API, B Status/begrenztes Polling/aktueller Laufchart, C schreibende Kommandos/Revisionskonflikte, D responsive lokale Webassets, E Anmeldung/Sessions/CSRF/Service nach OD-09; Onboarding bleibt OD-06, Issue im Audit nicht aendern |
| #28 | passive Diagnose/Export vom aktiven Serviceablauf trennen |

Es gibt derzeit kein offenes Implementierungsissue, das allein wegen einer
Bibliotheksalternative sofort geschlossen werden sollte. Treiberbibliotheken
ersetzen nur Low-Level-Arbeit, nicht die fachlichen Issueziele.

## Offene Ownerentscheidungen

| ID | Entscheidung | Spaetester Zeitpunkt |
|---|---|---|
| OD-02 | Display-/Touchtreiberstack | in Stufe 4 nach der gestuften Hardwarematrix, vor #31 |
| OD-03a | DS18B20-/1-Wire-Softwarestack | nach Stufe 3, vor #30 |
| OD-03b | Bustopologie A oder begruendeter Rueckfall B | nach minimaler Hardwarebaseline, realem Pin-/GPIO-Inventar und identischem Fehlerisolationsvergleich; Produktbus separat, C ausgeschlossen |
| OD-05 | schlanke Views oder LVGL | nach OD-02, schmalem Adaptervertrag und identischem repraesentativem Screenvergleich |
| OD-07 | #19, #25, #26 und #27 als Teilentscheide verbindlich geschnitten; Mindestumfang und PR-/Issue-Schnitt von #28 bleibt offen | vor #28 |
| OD-09 | KDF-, Sitzungs-, CSRF-, Sperr- und Secret-at-rest-Vertrag | vor produktiver Authentication in #27 |

OD-01 ist entschieden: Variante B ist der verbindliche R1-Vertrag, Variante A
der additive spaetere Ausbaupfad. Vor #56/#57 bleibt als technische
Detailpruefung offen, ob Dokumentrevisionen und Rootsequenz die Funktion einer
eigenstaendigen persistenten `MutationSequence` vollstaendig abdecken. Diese
Pruefung darf die Sequenz nicht ohne Gleichwertigkeitsnachweis entfernen und
oeffnet OD-01 nicht erneut.

OD-04 ist als Evaluationsrichtung entschieden: Der lokale HTTP-Dienst ist
`REQUIREMENT_DECIDED`; Arduino-ESP32 `WebServer` ist
`FIRST_EVALUATION_CANDIDATE`, `SPIKE_REQUIRED` und bedingte Produktivrichtung,
aber `FINAL_SELECTION_PENDING`. `ESPAsyncWebServer` bleibt
`CONDITIONAL_FALLBACK`/`EVALUATE_LATER`; nur ein konkretes offenes R1-Risiko
loest den identischen Vergleich aus.

OD-07 ist nicht abgeschlossen. #19 ist mit vier, #25 mit zwei, #26 mit fuenf
lokalen und #27 mit fuenf Webbereichen entschieden; nur #28 folgt separat. Der Werksreset bleibt im
zentralen #57-Recoveryvertrag. Die Behandlung der Touchkalibrierung beim Reset
bedarf eines eigenen expliziten Policyentscheids zwischen Recovery und
Display-/Touchintegration und wird nicht in #19 versteckt.

OD-06 ist als Richtungsentscheid entschieden: WiFiManager ist der bevorzugte
Release-1-Onboardingkandidat und wird zuerst begrenzt geprueft. Die
endgueltige Uebernahme bleibt das Ergebnis dieses Spike-Gates. Der kleine
Frameworkadapter wird nur bei einem dokumentierten Ausloeser als identischer
Gegenprototyp nachgezogen; eine offene vorsorgliche Gleichwahl besteht nicht.

Der JSON-Richtungsentscheid besitzt bewusst kein neues OD-Kuerzel:
ArduinoJson `7.4.3` ist der bevorzugte Kandidat. Die endgueltige Uebernahme
bleibt das Ergebnis des Build-, Grenzwert-, Fuzz- und Ressourcen-Spike-Gates;
eine Alternative wird nur bei einem dokumentierten Problem untersucht.
