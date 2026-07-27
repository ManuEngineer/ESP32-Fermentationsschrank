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
  -> hardwareunabhaengiger Safety- und Fachkern
  -> aktorfreie Hardware-Spikes
  -> Bibliotheksentscheidungen
  -> produktive Adapter
  -> sicheres Hardware-Bring-up
  -> Web- und Bedienintegration
  -> thermische Abnahme und Releasegate
  -> spaetere Funktionen
```

## Phase 1: Auditfreigabe und Ownerentscheide

Vor weiterer Architekturverbreiterung:

1. Auditdokumente fachlich freigeben oder korrigieren.
2. OD-01 entscheiden: volle #56/#57-Manifest-/Root-/Pending-/Secret-Struktur
   fuer Release 1 bestaetigen oder in einem separaten ADR-/Issueprozess auf den
   kleinsten sicheren R1-Vertrag reduzieren.
3. Abhaengigkeit von #17/#24 auf das Tracking-Issue #16 anhand der tatsaechlich
   benoetigten Persistenzvertraege neu pruefen.
4. Grundsatz bestaetigen, dass Treiber/Frameworkdienste adoptiert und
   Safety-/Fachlogik selbst entwickelt werden.
5. breite Issues #19 und #25–#28 vor Implementierung in kleine PRs schneiden,
   ohne ihren akzeptierten R1-Mindestumfang still zu streichen.

Keine Empfehlung aus dem Audit wird im Audit-PR selbst implementiert.

## Phase 2: zwingende hardwareunabhaengige Safety- und Fachlogik

Empfohlene parallele Ketten:

```text
#20 Sensorqualitaet
  -> #21 Regelsensorauswahl
  -> #22 PI und Luftbegrenzung
  -> #23 Aktorplaner

Persistenzminimum aus #54/#55 und Ownerentscheid OD-01
  -> #17 Laufpersistenz
  -> #18 Wiederanlauf

#17 + #20..#23 + bestehender Laufkern
  -> #24 Fehlerkern und SAFE_BOOT
  -> #25 gemeinsame View-Modelle/Sprachen
```

Vorgeschlagene kleine PRs:

- #20: Status-/Plausibilitaetsmodell, danach Filterpipeline;
- #21: Auswahl/Fallback, danach Rueckkehr und Ereignisse;
- #22: begrenzter PI-Kern, danach Luftbegrenzung/Diagnose;
- #23: Peltierplaner, danach Luefter/Nachlauf;
- #17: Kontrollpunktcodec/-slots, danach Ereignis-/Rueckfallservice;
- #18: phasenbezogener Restart, danach Zeit-/Fortschrittskorrektur;
- #24: Fehlerdatenmodell, persistente Verriegelung/Boot, danach
  Fehlerinjektionsmatrix;
- #25: gemeinsame View-Modelle, Sprachressourcen und Navigation getrennt.

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
3. #32 Luefter, Summer und MOSFET-Kanaele einzeln;
4. #33 BTS7960 ohne Peltier;
5. erst danach #33 begrenzte Peltierpulse mit Sicherung, montierter einmaliger
   Temperatursicherung, Pflichtsensoren, Lueftern und bestaetigten Pegeln.

Ein bestandener Bibliotheksspike ist keine Freigabe fuer reale Aktoren.

## Phase 7: Web- und Bedienintegration

Nach stabilen Fachvertraegen und den relevanten Adapterentscheiden:

- #26 in Navigations-/Screen-Scheiben umsetzen; UI-Logik weiterhin nativ
  testen;
- #27 teilen in begrenzte Lese-API/Transport, mutierende Kommandos/Konflikte,
  Webassets, Onboarding und Authentication;
- vor Authentication OD-09 festlegen: KDF/Work-Factor, Sitzungsdauer,
  Sperrzeiten, CSRF und At-rest-Grenze;
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
- Encoder, Taster, LED, Tuerkontakt, RTC-Pflicht, 12-V-ADC und Lueftertacho;
- vorsorgliche Ports, Puffer und Bibliotheken fuer diese Funktionen.

## Zu verkleinern, zu ersetzen oder zu schliessen

| Bestehendes Issue | Vorschlag nach Ownerfreigabe |
|---|---|
| #16 | als Tracking behalten; keine Direktimplementierung; nach #54–#57 beziehungsweise ownerfreigegebenem Ersatzumfang schliessen |
| #56/#57 | nicht freigeben, bis OD-01 entschieden ist; danach entweder bestehenden Vertrag in kleinen Scheiben umsetzen oder durch separat akzeptierten kleineren R1-Vertrag ersetzen |
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
| OD-01 | voller oder verkleinerter R1-Umfang von #56/#57 und daraus folgende #16-Abhaengigkeiten | vor Freigabe #56 und vor Blockierung von #17/#24 |
| OD-02 | Display-/Touchstack | nach Display-Spike, vor #31 |
| OD-03 | DS18B20-/1-Wire-Stack | nach Sensorspike, vor #30 |
| OD-04 | Framework-WebServer oder ESPAsyncWebServer | nach begrenztem Last-/Ressourcenprototyp, vor #27-Transport |
| OD-05 | schlanke Views oder LVGL | nach Display-Spike und representativem Screenvergleich |
| OD-06 | WiFiManager oder kleiner Framework-Onboardingadapter | vor Onboardingteil von #27 |
| OD-07 | Mindestumfang und PR-Schnitt von #19/#25–#28 | vor dem jeweiligen Issue |
| OD-09 | KDF-, Sitzungs-, CSRF-, Sperr- und Secret-at-rest-Vertrag | vor produktiver Authentication in #27 |
