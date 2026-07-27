# Vorgeschlagene Release-1-Roadmap

Zur Auditnavigation: [`RELEASE_1_ADOPT_OR_BUILD_AUDIT.md`](RELEASE_1_ADOPT_OR_BUILD_AUDIT.md).

## Status

Auditentwurf, Ownerfreigabe ausstehend. Diese Roadmap ersetzt weder
`docs/IMPLEMENTATION_PLAN.md` noch bestehende Issues oder akzeptierte ADRs. Sie
zeigt eine risikoorientierte Reihenfolge und die Stellen, an denen der aktuelle
Backlog vor Umsetzung verkleinert oder geteilt werden sollte.

## Leitende Reihenfolge

```text
Auditfreigabe
  -> separater Spezifikations-/Issue-Neuschnitt fuer Variante B
  -> hardwareunabhaengiger Safety-, Persistenz- und Fachkern
  -> aktorfreie Hardware-Spikes
  -> Bibliotheksentscheidungen
  -> produktive Adapter
  -> sicheres Hardware-Bring-up
  -> Web- und Bedienintegration
  -> thermische Abnahme und Releasegate
  -> spaetere Funktionen
```

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
7. breite Issues #19 und #25–#28 vor Implementierung in kleine PRs schneiden,
   ohne ihren akzeptierten R1-Mindestumfang still zu streichen.

Keine Empfehlung aus dem Audit wird im Audit-PR selbst implementiert.

## Phase 2: zwingende hardwareunabhaengige Safety-, Persistenz- und Fachlogik

Empfohlene parallele Ketten:

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

#17 + #20..#23 + bestehender Laufkern
  -> #24 Fehlerkern und SAFE_BOOT
  -> #25 gemeinsame View-Modelle/Sprachen
```

Vorgeschlagene kleine PRs:

- #20: Status-/Plausibilitaetsmodell, danach Filterpipeline;
- #21: optionalen Produktfuehler als primaeren Regelsensor, regulaeren
  Raum-/Luft-Ersatzsensor und sichere Rueckkehr modellieren; der verpflichtende
  Kuehlkoerper-/Peltier-Schutzsensor bleibt unabhaengige Freigabegrundlage;
- #22: begrenzter PI-Kern, danach Luftbegrenzung/Diagnose;
- #23: Peltierplaner, danach Luefter/Nachlauf;
- #17: Kontrollpunktcodec/-slots, danach Ereignis-/Rueckfallservice;
- #18: phasenbezogener Restart, danach Zeit-/Fortschrittskorrektur;
- #24: Fehlerdatenmodell, persistente Verriegelung/Boot, danach
  Fehlerinjektionsmatrix;
- #25: gemeinsame View-Modelle, Sprachressourcen und Navigation getrennt.

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
Thermik.

## Phase 3: Hardwarebasis und aktorfreie Spikes

Nach #24 beziehungsweise sobald sichere Boot-/Fehlergrenzen verfuegbar sind:

1. #29 als kleinster Hardwarebaseline-PR: Boardrevision, Flash/Partition,
   Ressourcen, UART-Recovery und unbelastete sichere Ausgangszustaende.
2. Display-/Touch-Spike aus [`HARDWARE_SPIKE_PLAN.md`](HARDWARE_SPIKE_PLAN.md)
   mit LovyanGFX, TFT_eSPI und LCDWiki-Paket.
3. DS18B20-Spike mit DallasTemperature+OneWire und Espressif-Komponenten, soweit
   die aktuelle Toolchain letztere ohne Scopewechsel bauen kann.
4. getrennte kleine Webserver-/Onboarding-Prototypen koennen nativ/auf
   aktorfreiem ESP32 Ressourcen messen, ohne #27 vorwegzunehmen.

Hardwaregate: keine Peltier-, H-Bruecken-, Luefter- oder Summerfreigabe in den
Display-/Sensor-/Netzwerkspikes.

## Phase 4: Bibliotheksentscheidungen

Der Owner waehlt anhand identischer Messungen:

- genau einen Display-/Touchstack und einen Rueckfallkandidaten;
- genau einen DS18B20-/1-Wire-Stack und einen Rueckfallkandidaten;
- kleinsten geeigneten Webservertransport;
- WiFiManager oder einen kleinen Framework-Onboardingadapter;
- ArduinoJson nur mit endpunktspezifischen Grenzen;
- schlanke eigene Screens als Baseline; LVGL nur bei gemessenem, klarem
  Wartungs- und Ressourcenfit.

Jede Auswahl erhaelt Version/Commit, Lizenznachweis, Build-/Hardwaremessung,
Adaptervertrag und ein eigenes umsetzendes Issue/PR. Keine Auswahl nur aufgrund
von Sternen, Marketing oder README-Beispielen.

## Phase 5: produktive Adapter

Kleine adapterbezogene PRs:

1. NVS-/Preferences-Adapter fuer den vorhandenen `IStateStore`, sofern nicht
   bereits in einem zuvor ownerfreigegebenen Persistenzpaket enthalten;
2. #30 DS18B20-/1-Wire-Adapter hinter `ITemperatureSource`;
3. #31 Display- und Touchadapter hinter getrennten schmalen Grenzen;
4. WLAN-, Zeit-/Zeitzonen- und Webtransportadapter;
5. JSON nur an den konkreten API-/Export-/Importgrenzen.

Bibliothekstypen duerfen weder in `fermentation_app` noch in Safety- oder
Prozessmodelle durchsickern. Jeder Adapter uebersetzt Fehler und Limits
vollstaendig und besitzt eine Mock-/Hostgrenze.

## Phase 6: Hardware-Bring-up

Verbindliche Reihenfolge:

1. #29 Controller, UART, Ressourcen und unbelastete Ausgaenge;
2. #30 Sensoren und #31 Display/Touch aktorfrei;
3. #32 Luefter, den Summer als einziges zusaetzliches lokales Ausgabeelement
   und MOSFET-Kanaele einzeln;
4. #33 BTS7960 ohne Peltier;
5. erst danach #33 begrenzte Peltierpulse mit Sicherung, montierter einmaliger
   Temperatursicherung, gueltigem und ausreichend vertrauenswuerdigem
   Kuehlkoerper-/Peltier-Schutzsignal, einem gemaess Laufvertrag verwendbaren
   Regelsensor, Lueftern und bestaetigten Pegeln.

Ein bestandener Bibliotheksspike ist keine Freigabe fuer reale Aktoren.

## Phase 7: Web- und Bedienintegration

Nach stabilen Fachvertraegen und den relevanten Adapterentscheiden:

- #26 als einzige lokale Bedien- und Anzeigeoberflaeche in
  Navigations-/Screen-Scheiben umsetzen; UI-Logik weiterhin nativ testen;
- #27 teilen in begrenzte Lese-API/Transport, mutierende Kommandos/Konflikte,
  Webassets, Onboarding und Authentication; die Weboberflaeche bleibt
  sekundaer und ist keine Voraussetzung fuer den lokalen Betrieb;
- vor Authentication OD-09 festlegen: KDF/Work-Factor, Sitzungsdauer,
  Sperrzeiten, CSRF und At-rest-Grenze;
- Connectivity- und Authentication-Domaenen erst mit den ersten realen WLAN-,
  Passwort- oder PIN-Nachweisen spezifizieren; keine vorbereiteten leeren
  Manifeste, Roots oder `CredentialEpoch` aus #57 uebernehmen;
- #28 teilen in passive Diagnose, Exporte/Diagrammdaten und aktiven
  Serviceablauf;
- #19 teilen in kritisches Journal/Retention und secret-freies
  Backup/Import mit zentraler Vorschau.

Regelung und Safety muessen unter Web-, Export- und Netzwerklast
deterministisch bleiben. HTTP ist nur fuer das vertrauenswuerdige lokale Netz;
Cloud und Internet-Portfreigabe bleiben ausgeschlossen.

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
- LVGL, sofern der Hardware-/UI-Vergleich keinen zwingenden R1-Vorteil zeigt;
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
| #16 | als Tracking behalten, aber nach dem Audit in separatem Planungs-/ADR-Schritt auf den Variante-B-Kern und schmale Abhaengigkeiten neu schneiden; keine Direktimplementierung |
| #56 | nicht unveraendert freigeben; separat auf Active/Fallback, Graphvalidierung, fluechtige Vorschau, Konfliktschutz und Runtime-Publish reduzieren |
| #57 | nicht unveraendert freigeben; separat auf Bootstrap, `StorageEpoch`, Korruptionssperre und wiederaufnehmbaren Werksreset reduzieren |
| spaeteres Pending/Secrets | erst mit neustartpflichtigem Konfigurationswert beziehungsweise realen WLAN-/Passwort-/PIN-Nachweisen als eigene Issues planen; Variante A additiv anbinden |
| #19 | nicht streichen, aber in Journal/Retention, Export und Backup/Import teilen |
| #25 | View-Modelle, Texte und Navigation in getrennte kleine PRs schneiden |
| #26 | native UI-Logik von realem Display-/Touchadapter trennen |
| #27 | Webtransport, API, Webassets, Onboarding und Authentisierung trennen |
| #28 | passive Diagnose/Export vom aktiven Serviceablauf trennen |

Es gibt derzeit kein offenes Implementierungsissue, das allein wegen einer
Bibliotheksalternative sofort geschlossen werden sollte. Treiberbibliotheken
ersetzen nur Low-Level-Arbeit, nicht die fachlichen Issueziele.

## Offene Ownerentscheidungen

| ID | Entscheidung | Spaetester Zeitpunkt |
|---|---|---|
| OD-02 | Display-/Touchstack | nach Display-Spike, vor #31 |
| OD-03 | DS18B20-/1-Wire-Stack | nach Sensorspike, vor #30 |
| OD-04 | Framework-WebServer oder ESPAsyncWebServer | nach begrenztem Last-/Ressourcenprototyp, vor #27-Transport |
| OD-05 | schlanke Views oder LVGL | nach Display-Spike und representativem Screenvergleich |
| OD-06 | WiFiManager oder kleiner Framework-Onboardingadapter | vor Onboardingteil von #27 |
| OD-07 | Mindestumfang und PR-Schnitt von #19/#25–#28 | vor dem jeweiligen Issue |
| OD-09 | KDF-, Sitzungs-, CSRF-, Sperr- und Secret-at-rest-Vertrag | vor produktiver Authentication in #27 |

OD-01 ist entschieden: Variante B ist der verbindliche R1-Vertrag, Variante A
der additive spaetere Ausbaupfad. Vor #56/#57 bleibt als technische
Detailpruefung offen, ob Dokumentrevisionen und Rootsequenz die Funktion einer
eigenstaendigen persistenten `MutationSequence` vollstaendig abdecken. Diese
Pruefung darf die Sequenz nicht ohne Gleichwertigkeitsnachweis entfernen und
oeffnet OD-01 nicht erneut.
