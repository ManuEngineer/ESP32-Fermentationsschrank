# Vorgeschlagene Release-1-Roadmap

Zur Auditnavigation: [`RELEASE_1_ADOPT_OR_BUILD_AUDIT.md`](RELEASE_1_ADOPT_OR_BUILD_AUDIT.md).

## Status

Ownerfreigegebene risikoorientierte Arbeitsreihenfolge. Der Adopt-or-build-
Audit und die Espressif-first-Synchronisierung wurden mit PR #88 gemergt; PR #92
regelt das Überspringen vollständiger CI bei ausschliesslichen Markdown-
Änderungen. Diese Roadmap ersetzt weder `docs/IMPLEMENTATION_PLAN.md` noch
bestehende Issues oder akzeptierte ADRs. Bei Widersprüchen bleiben Live-Issues
und später akzeptierte ADRs vorrangig. Änderungen an Scope oder Reihenfolge
benötigen weiterhin einen Ownerentscheid.

Der ausführliche historische Audittext weiter unten bleibt als Begründungs- und
Nachweistext erhalten. Die folgende aktuelle Synchronisierung ist für den
heutigen Arbeitsstand massgeblich.

## Aktuelle Synchronisierung

Die direkte Variante-B-Konfigurations- und Persistenzgrundlage ist abgeschlossen:

| Arbeit | aktueller Stand |
|---|---|
| #54 – IStateStore und Wireformat | abgeschlossen |
| #55 – typisierte Konfigurationsdokumente | abgeschlossen |
| #56 – Active-/Fallback-Graph und Runtimeaktivierung | abgeschlossen |
| #57 – Bootstrap, StorageEpoch und Recovery | abgeschlossen durch PR #68 (`b6d2385934db288cb25b125abbcbe5e307aca294`) |
| #17 – Laufpersistenz und Kontrollpunkte | abgeschlossen durch PR #84 |
| #16 | nur noch Trackingcontainer für übergreifende Abnahmegates |
| #90 | produktiver ESP-IDF-NVS-Adapter und reale Messungen |

Der abgeschlossene Kern #54–#57 wird nicht erneut eingeplant. Das
`CONFIGURATION_SAFETY_INTEGRATION_GATE` bleibt ein Abschlussbestandteil von
#24; #90 bleibt für den produktiven ESP-IDF-NVS-Adapter und die reale
Flash-/Partitionsverifikation zuständig.

## Leitende Reihenfolge

```text
#20 Sensorqualität
  -> #21 Regelsensorauswahl
  -> #22 PI-Regelung
  -> #23 Aktorplaner
  -> #24 Fehlerkern und SAFE_BOOT

#17 abgeschlossen + #20
  -> #18 Wiederanlauf

#20 + #21 + sichere #29-Baseline
  -> #30 reale DS18B20-Integration

parallel: #29 sichere, aktorfreie Minimalbaseline (eigener Plan-first-PR)
erst nach den jeweiligen Safety-Gates:
produktive Aktoren -> thermische Abnahme -> Releasegate
```

Der Display-/Touchpfad ist innerhalb des aktorfreien Spikes weiter gestuft:
reale Hardware identifizieren, alle vier gleichrangigen Kandidatengruppen
(Espressif-Stack als Espressif-first-Erstkandidat, LovyanGFX, TFT_eSPI,
LCDWiki) durch Quellen-/Lizenz- und Buildpruefung fuehren, ausreichend
erfolgreiche Kandidaten kurz auf der
Hardware pruefen und nur nach bestandenem Smoke-Test die vollstaendige
identische Matrix ausfuehren. Danach entscheidet der Owner ueber bevorzugten
Treiber und Rueckfallkandidat. Reservekandidaten werden nur bei dokumentiertem
Ausloeser nachgezogen.

## Phase 1: Audit- und Persistenzkern abgeschlossen; Backlog-Teilschnitte offen

### Abgeschlossen

- Audit und Espressif-first-Synchronisierung durch PR #88;
- #54–#57 als direkter Variante-B-Konfigurations- und Persistenzkern, #57
  abgeschlossen durch PR #68;
- #17 als Laufpersistenz und Kontrollpunkte, abgeschlossen durch PR #84;
- #24 bleibt ein nachgelagertes Integrations- und Safety-Gate; #17 benötigt
  keine weitere #16-Abhängigkeitskorrektur.

### Noch offen

- ownerfreigegebener Planungsschnitt von #19 in Journal/Retention,
  Laufhistorie/Bereinigung, lesenden Laufexport/secret-freies Backup und
  Importvorschau/atomare Aktivierung;
- ownerfreigegebene Teilschnitte von #25–#28 für Präsentationsmodelle,
  lokale Bedienung, Web und Diagnose/Service;
- die jeweiligen technischen Pläne und Abnahmekriterien dieser Teilschnitte.

Der OD-07-Rahmen ist dokumentiert, ersetzt aber nicht die noch ausstehenden
ownerfreigegebenen Planungsschritte. Persistentes Pending sowie echte
Connectivity-/Authentication-Domaenen werden weiterhin erst mit ihrem ersten
fachlichen Konsumenten geplant und nicht als leere R1-Infrastruktur vorbereitet.
Treiber/Frameworkdienste werden adoptiert, Safety-/Fachlogik wird selbst
entwickelt.

Der Audit-PR hat selbst keine Audit-Empfehlungen implementiert; das ist eine
historische Einordnung und keine aktuelle Handlungsanweisung für PR #94.

## Phase 2: zwingende hardwareunabhaengige Safety-, Persistenz- und Fachlogik

Diese Ketten laufen parallel zur minimalen Hardwarebaseline und zu den
aktorfreien Display-/Touch- und Sensorspikes. Sie muessen nicht zuerst
abgeschlossen werden:

```text
#20 Sensorqualitaet
  -> #21 Regelsensorauswahl
  -> #22 PI und Luftbegrenzung
  -> #23 Aktorplaner

abgeschlossene Persistenzbasis #54/#55
  -> abgeschlossener Variante-B-Kern #56/#57
  -> #16 bleibt Tracking für übergreifende Gates

schmale benoetigte Variante-B-Vertraege
  -> #17 Laufpersistenz [ABGESCHLOSSEN, PR #84]
  -> #18 Wiederanlauf

#17 [abgeschlossen] + stabiler Variante-B-Kern
  -> #19-A typisiertes Ereignisjournal und Retention
  -> #19-B begrenzte Laufhistorie und stromausfallsichere Bereinigung
  -> #19-C nur lesender Laufexport und secret-freies Backup
  -> #19-D Importvorschau und atomare Aktivierung mit synchronem Run-Gate

#17 [abgeschlossen] + #20..#23 + bestehender Laufkern
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

OD-09-Policy
  -> Authspike: PBKDF2/Zufall/Ressourcen/Cut-Points
  -> Ownerentscheid KDF und Work Factor
  -> erster Authkonsument mit Credentialdomaene
  -> #27-E normale oder anonyme fluechtige Sessions, CSRF und Servicefreigabe
  -> technische Auth-/Credential-/Ressourcen-, Webserver- und JSON-Gates
  -> #27-C schreibende Kommandos und Revisionskonflikte erst nach Ownerfreigabe

Fach-, Sensor- und Safetymodelle
  -> #28-A passive Diagnosemodelle und Boot-Selbsttest
  -> #28-B Ressourcen- und Gesundheitsdiagnose mit realen Messpunkten

OD-09-Integrationsnachweis + #28-A/#28-B
  -> #28-C gefuehrter Serviceablauf zuerst vollstaendig mit Mocks
  -> #24- und Hardwaregates vor realen Serviceadaptern

#19-C Exportinfrastruktur + #28-A/#28-B
  -> #28-D nur lesender Diagnose- und Servicebericht
```

#26-A bis #26-E bezeichnen den spaeter ownerfreizugebenden Schnitt, keine im
Audit erstellten Issues. Ihre Screen-, Dialog-, Aktions- und Fehlerlogik ist
nativ beziehungsweise mit simulierten Touchereignissen pruefbar. Reales
Rendering und Touchintegration folgen #31/OD-02; der Frameworkvergleich folgt
OD-05, reale Authentisierung dem entschiedenen OD-09-Vertrag und seinen
technischen Gates und die Resetmechanik dem #57-Vertrag.

Vorgeschlagene kleine PRs:

- #20: Status-/Plausibilitaetsmodell, danach Filterpipeline;
- #21: optionalen Produktfuehler als primaeren Regelsensor, regulaeren
  Raum-/Luft-Ersatzsensor und sichere Rueckkehr modellieren; der verpflichtende
  Kuehlkoerper-/Peltier-Schutzsensor bleibt unabhaengige Freigabegrundlage;
- #22: begrenzter PI-Kern, danach Luftbegrenzung/Diagnose;
- #23: Peltierplaner, danach Luefter/Nachlauf;
- #17: abgeschlossen durch PR #84 (Kontrollpunktcodec, Revisionen,
  Rueckfall, Korruptionserkennung); kein weiterer PR in diesem Scope;
- #18: phasenbezogener Restart, danach Zeit-/Fortschrittskorrektur, auf dem
  abgeschlossenen #17-Fundament;
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
  Bestaetigung und atomare Aktivierung erst nach stabiler OD-01-Basis; den
  synchronen Run-Gate `Unknown`/`NoActiveOrRecoverableRun`/
  `ActiveRunPresent`/`RecoverableRunPresent` vor Annahme, Vorschau/Bestaetigung
  und unmittelbar vor Commit pruefen und Runstart gegen Importcommit
  serialisieren; nur `NoActiveOrRecoverableRun` erlaubt, ohne Pending/Intent;
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
- #27-E/#27-C: nach dem OD-09-Authspike zuerst getrennte Webpasswortpruefung,
  vollstaendig neustartfeste Fehlversuchs-/Sperrpersistenz, fluechtige normale
  oder anonyme lokale Sessions, CSRF und sitzungsgebundene Servicefreigabe
  integrieren; schreibende Kommandos erst nach allen erforderlichen
  technischen Auth-/CSRF-/Credential-/Ressourcen-, Webserver- und JSON-Gates
  und Ownerfreigabe. ADR-017 schliesst dauerhafte Anmeldung aus; R1 verspricht
  keine oeffentliche externe Schreib-API.
- #28-A: passive typisierte Diagnoseprojektionen und Boot-Selbsttest ohne
  Aktoransteuerung; Roh-/Korrektur-/Filterwerte, Qualitaet, Regelsensor,
  Regler-/Aktorstatus, Fehler, Verriegelungen und Systemstatus getrennt testen;
- #28-B: begrenzte Plattform-, Build-, Persistenz-, Heap-, Flash-, Reset- und
  Gesundheitsmesspunkte anbinden; Schwellen und Reserven bleiben bis zur realen
  Messung `MEASUREMENT_REQUIRED`;
- #28-C: Auswahl, Voraussetzungen, Sperrgruende, Bestaetigung, Fortschritt,
  sicheren Abbruch und Ergebnis zuerst mit Mocks testen; Auth gemaess OD-09
  nach technischen Gates und
  reale Aktorpruefungen erst hinter #24 und Hardwaregates;
- #28-D: versionierten, redigierten, nur lesenden Diagnose-/Servicebericht auf
  #28-A/#28-B aufbauen und fuer Serialisierung/Download die #19-C-Infrastruktur
  wiederverwenden; kein zweiter Exportpfad und kein Berichtsimport.

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
   Leitungen und Bootzustaende identifizieren. Danach alle vier gleichrangigen
   Kandidatengruppen (Espressif-Stack, LovyanGFX, TFT_eSPI, LCDWiki)
   durch Stufe 1, ausreichend erfolgreiche Kandidaten durch den kurzen
   Hardware-Smoke-Test der Stufe 2 und nur `PASS_SMOKE_TEST`-Kandidaten durch
   die vollstaendige identische Matrix der Stufe 3 fuehren. Stufe 4 benennt
   bevorzugten Treiber und Rueckfallkandidat. Reservekandidaten bleiben
   bedarfsabhaengig. Dieselbe Matrix muss den PIN-unabhaengigen Raw-Touch-
   Boot-Recoverypfad im ersten Zehn-Sekunden-Fenster ohne brauchbare
   Kalibrierung, False Trigger, sicheren Abbruch, aktorfreien Zustand und UART-
   Rueckfall nachweisen. Geste und Schwellen bleiben `TBD_HARDWARE`; ein
   Kandidat ohne diesen Nachweis scheidet aus. Details stehen im
   [`HARDWARE_SPIKE_PLAN.md`](HARDWARE_SPIKE_PLAN.md).
4. Beide DS18B20-/1-Wire-Stacks gestuft pruefen: Stufe 1 Quelle/Lizenz/Build,
   Stufe 2 Sensorsmoke-Test mit einem realen Sensor und nur nach Erfolg Stufe 3
   mit identischer Topologie- und Fehlermatrix. Die fixierte ESP-IDF-`6.0.2`-
   Toolchain erfuellt die `onewire_bus`-Mindestanforderung IDF >=5.0; ein
   dennoch nicht reproduzierbar baubarer Espressif-Kandidat endet als
   `INCOMPATIBLE_WITH_CURRENT_TOOLCHAIN`, nicht als allgemein ungeeignet.
   Softwarestack und Bustopologie getrennt entscheiden: Produktfuehler immer
   separat, Topologie A mit drei Bussen bevorzugt, Topologie B mit gemeinsamem
   festen Bus als pinabhaengiger Rueckfall und Topologie C hoechstens als
   negativer Referenztest. Allgemeine Trennungs-, Unterbruch-, Fehler- und
   Wiederkehrtests bleiben erhalten; Anschlussart, Anschlussbelegung, drei
   GPIOs und Schutzbauteilwerte werden in diesem Softwareaudit nicht festgelegt.
5. Den `FIRST_EVALUATION_CANDIDATE` ESP-IDF `esp_http_server` in einem kleinen
   aktorfreien, `SPIKE_REQUIRED`-Baselineprototyp
   fuer statische Ressourcen, begrenzte API-/Import-/Exportpfade, wenige
   Clients, Abbruch-, WLAN-, Jitter- und Ressourcenmessung vorbereiten, ohne
   #27 vorwegzunehmen. Die endgueltige Produktivauswahl bleibt
   `FINAL_SELECTION_PENDING`. `ESPAsyncWebServer` bleibt ein ergebnisoffener
   Evaluationskandidat mit `EVALUATE_LATER` und wird nur dann mit identischem
   Umfang nachgezogen, wenn der Baselineprototyp ein konkretes Release-1-Risiko
   offen laesst.
6. Davon fachlich getrennt, aber mit dem Webserver-Baselineprototyp koordiniert,
   gemaess Espressif-first drei gleichwertige ESP-IDF-`6.0.2`-Pfade
   (`espressif/network_provisioning` auf `protocomm`, ein direkter
   `protocomm`-/SoftAP-Ansatz und ein kleiner eigener nativer
   SoftAP-/DNS-/HTTP-Adapter) sowie WiFiManager als ergebnisoffenen
   konditionalen Evaluationskandidaten unter demselben Evaluationsgate pruefen:
   Toolchain, Quellen/Lizenz/Webassets, ausdruecklicher Portalstart, reale
   Android-/iOS-/Windows-Clients, primaerer QR mit individuellen SoftAP-
   Zugangsdaten, sichtbarer direkter IP-Rueckfall, Credential-Erhalt,
   Secret-/Fehler-/Lifecyclegrenzen, Cut-Points, Jitter, Ressourcen sowie den
   Nachweis des browserbasierten R1-Vertrags ohne verpflichtende App, Cloud
   oder Kommandozeilenwerkzeug. Dieser Spike setzt weder #27 um noch nimmt er
   eine endgueltige Bibliothekswahl vorweg; Details stehen im Backlog-Issue
   #89.
7. ArduinoJson `7.4.3` als `FIRST_EVALUATION_CANDIDATE`, `SPIKE_REQUIRED` und
   `FINAL_SELECTION_PENDING` in
   einem kleinen aktorfreien Prototyp pruefen: isolierter Build, konkrete
   DTO-/Codecgrenze, 1-/4-KiB-Profile fuer nachgewiesene kleine DTOs,
   Vollimportgrenze aus dem maximal gueltigen externen Schema,
   Verschachtelungstiefe 6,
   String-/Array-/Feld-/Schemagrenzen, Importvorschau ohne Aktivierung,
   Streaming/Pagination, reproduzierbare Negativ-/Fuzztests sowie ESP32-
   Ressourcen-, Laufzeit- und Jittermessung. Dieser Spike wird mit dem
   `esp_http_server`-Baselineprototyp und dem spaeteren Schnitt von #19/#27/#28
   koordiniert, implementiert aber keines dieser breiten Issues. Eine
   Alternative wird nur bei einem konkret belegten R1-Problem untersucht.
8. Den OD-09-Authspike ohne produktive Endpunkte ausfuehren: PBKDF2-HMAC-
   SHA-256 aus der fixierten Toolchain mit bekannten Testvektoren, getrennten
   Salts und Parameterkennung pruefen; Work Factor, Laufzeit, Stack, Heap,
   Jitter, Watchdog und parallele Anfragen messen. `esp_fill_random()` oder
   einen korrekt gesaeten mbedTLS-DRBG samt Fehlerpfad evaluieren. Globale
   Fehlversuchsserien mit atomar persistiertem Vor-Sperr-Zaehler, Sperrstufe,
   aktivem Sperrzustand, Credential-Epoche/-Generation und Integritaet,
   Neustartpersistenz, normale und anonyme Session-/CSRF-Tokenbildung,
   Credentialwechsel und Widerruf an Cut-Points pruefen. Danach entscheidet
   der Owner KDF und Work Factor. NVS-/Flashverschluesselung bleibt ein
   getrennter `EVALUATE_BEFORE_RELEASE`-Security-Spike.
9. Fuer den spaeteren #19-B-Schnitt reale Speicher- und Ressourcenmessungen
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
- nach der gemeinsamen Messmatrix der drei Espressif-Pfade und WiFiManager
  genau einen WLAN-Onboardingkandidaten endgueltig uebernehmen;
- ArduinoJson `7.4.3` als bevorzugten Kandidaten erst nach bestandenem
  Build-, Grenzwert-, Fuzz- und Ressourcennachweis endgueltig uebernehmen;
  Alternative nur bei dokumentiertem Problem;
- PBKDF2-HMAC-SHA-256 nur nach bestandenem KDF-/Ressourcenspike und separatem
  Ownerentscheid samt Work Factor uebernehmen; den kryptografischen
  Zufallsintegrationspfad ebenfalls erst nach Nachweis festlegen. Die
  Plattformverschluesselung bleibt davon getrennt.

Jede Auswahl erhaelt Version/Commit, Lizenznachweis, Build-/Hardwaremessung,
Adaptervertrag und ein eigenes umsetzendes Issue/PR. Keine Auswahl nur aufgrund
von Sternen, Marketing oder README-Beispielen.

LVGL ist keine Treiberoption dieser Phase. Der UI-Frameworkentscheid folgt erst
nach Treiberauswahl, schmalem Adaptervertrag und einem repraesentativen
Release-1-Screen.

Beim Webserver besteht keine offene Gleichwahl: Der kleine lokale HTTP-Dienst
ist `REQUIREMENT_DECIDED`; ESP-IDF `esp_http_server` ist der
`FIRST_EVALUATION_CANDIDATE`, `SPIKE_REQUIRED` und die bedingte
Produktivrichtung. Die endgueltige Auswahl bleibt `FINAL_SELECTION_PENDING`.
Nur wenn sein begrenzter Prototyp eine konkrete R1-Anforderung nicht stabil und
ressourcengerecht erfuellt, wird der ergebnisoffene Evaluationskandidat
`ESPAsyncWebServer` unter identischen Bedingungen evaluiert. Ein
vollstaendiger Zweifachprototyp ist keine Pflicht.

## Phase 5: produktive Adapter

Kleine adapterbezogene PRs:

1. ESP-IDF-NVS-Adapter fuer den vorhandenen `IStateStore` (Backlog-Issue
   #90), sofern nicht bereits in einem zuvor ownerfreigegebenen
   Persistenzpaket enthalten;
2. #30 DS18B20-/1-Wire-Adapter hinter `ITemperatureSource`;
3. #31 Display- und Touchadapter hinter getrennten schmalen Grenzen;
4. WLAN-, Zeit-/Zeitzonenadapter und nach bestandenem Server-Spike genau ein
   kleiner konkreter lokaler HTTP-Adapter; die Zeile waehlt `esp_http_server`
   noch nicht produktiv aus;
5. nach bestandenem OD-06-Spike genau eine konkrete Onboardingintegration aus
   den drei Espressif-Pfaden oder WiFiManager (Backlog-Issue #89); der
   gewaehlte Kandidat bleibt technischer Portalbaustein, die uebrigen bleiben
   nur bei dokumentiertem Ausloeser erneut gepruefte Alternativen;
6. nach bestandenem JSON-Spike einen kleinen konkreten ArduinoJson-Codec nur an
   begrenzten API-, Konfigurations-, Programm-, Diagnose-, Export-,
   secret-freien Backup- und Importgrenzen; interne Persistenz bleibt binaer;
7. nach Authspike und Ownerwahl mit dem ersten produktiven Credentialkonsumenten
   stark typisierte versionierte Credentialrecords und eine vorwaertsgerichtete
   Credential-Epoche einfuehren; danach fluechtige Sessions, CSRF und
   sitzungsgebundene Servicefreigabe. #57 bereitet keine leeren Authstrukturen
   vor. Schreibende Webendpunkte folgen erst nach bestandenem technischem
   Auth-/CSRF-/Credential-/Ressourcen-, Webserver- und JSON-Gate; ohne normales
   Webpasswort bleiben anonyme Session, Cookie, CSRF, Revision und Warnung
   verbindlich.

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
  vergleichen; Auth gemaess OD-09 erst nach Technikgates und atomare
  Resetmechanik nur ueber #57
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
  Authentisierung/Sessions/CSRF/Service folgen gemaess OD-09 erst nach den
  Technikgates. Es gibt keine
  Last-write-wins-Strategie, globale Bearbeitungssperre oder versprochene
  oeffentliche externe Schreib-API;
- OD-06-Onboarding getrennt von #27 nach dem begrenzten gleichwertigen Spike
  der drei Espressif-Pfade und WiFiManager umsetzen: kein Portalstart bei
  gewoehnlichem temporaerem WLAN-Ausfall, neue Zugangsdaten bis zum Nachweis
  nur als Kandidat behandeln und den bisherigen funktionierenden Stand bei
  Fehler, Timeout oder Abbruch erhalten;
- den akzeptierten geschuetzten Ersatz-WLAN-Lebenszyklus getrennt vom
  Onboarding umsetzen und pruefen: kurzer Ausfall startet nichts; langer
  Ausfall startet nach `TBD_COMMISSIONING`, waehrend Heim-Reconnect, Lauf,
  normale Weboberflaeche und Auth-/CSRF-/Service-/Safetygates weiterlaufen;
  stabile Rueckkehr beendet ihn nach kontrollierter Uebergangszeit ohne offene
  Requests oder Speichervorgaenge abzuschneiden;
- OD-09 fachlich umsetzen, aber technische Auswahl nicht vorwegnehmen: zuerst
  KDF-/Zufalls-/Ressourcen-/Cut-Point-Spike, danach Ownerentscheid zu KDF und
  Work Factor, erster realer Authkonsument mit Credentialdomaene, fluechtige
  normale oder anonyme fluechtige Sessions/CSRF, Servicefreigabe und erst nach
  erfolgreichem Abschluss aller modusbezogenen Auth-/Credential-/Ressourcen-,
  Webserver- und JSON-Gates produktive Webmutationen; ADR-017 verbietet
  persistente Anmeldung;
- Connectivity- und Authentication-Domaenen erst mit den ersten realen WLAN-,
  Passwort- oder PIN-Nachweisen spezifizieren; keine vorbereiteten leeren
  Manifeste, Roots oder `CredentialEpoch` aus #57 uebernehmen;
- #28 in der entschiedenen Reihenfolge umsetzen: A passive Diagnosemodelle und
  Boot-Selbsttest, B Ressourcen-/Gesundheitsdiagnose, C gefuehrter
  Serviceablauf zuerst mit Mocks, D nur lesender Diagnose-/Servicebericht.
  Der aktuelle Laufchart bleibt #27-B, persistente Historie #19-B und die
  generische Export-/Download-/Streaminginfrastruktur #19-C. Reale
  Serviceaktoren bleiben hinter #24 und Hardwaregates; Auth folgt dem
  entschiedenen OD-09-Vertrag und seinen technischen Gates;
- #19 in der entschiedenen Reihenfolge umsetzen: typisiertes Journal/Retention,
  begrenzte verdichtete Laufhistorie/stromausfallsichere Bereinigung, nur
  lesender Laufexport/secret-freies Backup und erst danach Importvorschau/
  atomare Aktivierung. Interne Journale und Kontrollpunkte bleiben binaer,
  und ein JSON-Import aktiviert nie direkt aus dem Parser. #19-D prueft seinen
  synchronen Run-Gate vor Annahme, Vorschau/Bestaetigung und Commit; aktive,
  pausierte/unterbrochene, recoverable oder unbekannte Laufzustaende blockieren,
  und Runstart/Importcommit sind ohne Pending oder Intent serialisiert. Der Werksreset
  bleibt bei #57; der Werksreset behaelt die geraetespezifische
  Touchkalibrierung gemaess ADR-010, waehrend ein gesonderter Recoveryfall fuer
  unbrauchbare Kalibrierung getrennt bleibt;
- fuer #19/#27/#28 1 KiB und 4 KiB nur gegen die jeweiligen maximalen kleinen
  DTOs pruefen; fuer den Vollimport zuerst den maximal gueltigen externen
  Kandidaten deterministisch erzeugen und die Grenze daraus ableiten. Danach
  Gesamtbody oder begrenztes Streaming/Chunking entscheiden; Tiefe 6 sowie
  Root-, String-, Array-, Feld-, Werte- und Schemagrenzen pruefen.

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
  -> Security-Gate Plattformverschluesselung
  -> #37 sieben Tage Belastung und Releaseentscheid
```

Das `EVALUATE_BEFORE_RELEASE`-Security-Gate vor #37 umfasst einen isolierten
NVS-/Flashverschluesselungs-Spike mit Toolchain-, Boot- und Partitionspruefung,
Provisionierungs- und Produktionsflashablauf, Schluesselentstehung,
-speicherung und -verlust, Recovery, Werksreset, UART-Neuflash,
Update-/Migrationsauswirkungen sowie Ressourcen- und Stabilitaetsmessung. Der
Owner entscheidet danach vor #37 explizit zwischen:

1. **Auswahl:** produktive Umsetzung, dokumentierter Provisionierungsprozess
   sowie Recovery- und Regressionstests werden vor #37 abgeschlossen;
2. **begruendeter Nichtauswahl:** Rest-Risiken und Schutzgrenzen werden
   dokumentiert und vom Owner vor #37 ausdruecklich freigegeben.

Diese Roadmap waehlt oder aktiviert keine Plattformverschluesselung und legt
weder eFuses, Secure Boot, Schluesselmodell noch Partitionierung fest.

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
| #27 | nach Auditfreigabe in fuenf Bereiche schneiden: A HTTP-Transport/interne API, B Status/begrenztes Polling/aktueller Laufchart, C schreibende Kommandos/Revisionskonflikte, D responsive lokale Webassets, E Anmeldung/Sessions/CSRF/Service gemaess OD-09 nach Technikgates; Onboarding bleibt OD-06, Issue im Audit nicht aendern |
| #28 | nach Auditfreigabe in vier Bereiche schneiden: A passive Diagnosemodelle/Boot-Selbsttest, B Ressourcen-/Gesundheitsdiagnose, C gefuehrter Serviceablauf, D nur lesender Diagnose-/Servicebericht; aktuellen Laufchart nach #27-B, Historie nach #19-B und Exportinfrastruktur nach #19-C abgrenzen; Ressourcen `MEASUREMENT_REQUIRED`, reale Aktoren hinter #24/Hardwaregates und Auth hinter OD-09; Issue im Audit nicht aendern |

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

OD-01 ist entschieden: Variante B ist der verbindliche R1-Vertrag, Variante A
der additive spaetere Ausbaupfad. Vor #56/#57 bleibt als technische
Detailpruefung offen, ob Dokumentrevisionen und Rootsequenz die Funktion einer
eigenstaendigen persistenten `MutationSequence` vollstaendig abdecken. Diese
Pruefung darf die Sequenz nicht ohne Gleichwertigkeitsnachweis entfernen und
oeffnet OD-01 nicht erneut.

OD-04 ist als Evaluationsrichtung entschieden: Der lokale HTTP-Dienst ist
`REQUIREMENT_DECIDED`; ESP-IDF `esp_http_server` ist
`FIRST_EVALUATION_CANDIDATE`, `SPIKE_REQUIRED` und bedingte Produktivrichtung,
aber `FINAL_SELECTION_PENDING`. `ESPAsyncWebServer` bleibt ein
ergebnisoffener Evaluationskandidat mit `EVALUATE_LATER`; nur ein konkretes
offenes R1-Risiko loest den identischen Vergleich aus.

OD-07 ist vollstaendig entschieden. #19 ist mit vier, #25 mit zwei, #26 mit
fuenf lokalen, #27 mit fuenf Web- und #28 mit vier Diagnose-/Servicebereichen
geschnitten. Im urspruenglichen Audit vom 2026-07-27 blieben die Live-Issues
dabei unveraendert; die Espressif-first-Synchronisierung vom 2026-08-05
ergaenzte #27 spaeter gezielt um technische Kandidaten, ohne diesen
Fuenferschnitt zu aendern. Der Werksreset
bleibt im zentralen #57-Recoveryvertrag und behaelt gemaess ADR-010 die
geraetespezifische Touchkalibrierung; #19 veraendert sie nicht. Ein gesonderter
Recoveryfall fuer unbrauchbare Kalibrierung bleibt davon getrennt.

OD-06 ist als Richtungsentscheid entschieden: Gemaess Espressif-first werden
drei gleichwertige ESP-IDF-`6.0.2`-Pfade (`espressif/network_provisioning`
auf `protocomm`, ein direkter `protocomm`-/SoftAP-Ansatz und ein kleiner
eigener nativer SoftAP-/DNS-/HTTP-Adapter) sowie WiFiManager als
ergebnisoffener konditionaler Evaluationskandidat unter demselben
Evaluationsgate begrenzt geprueft. Die endgueltige Uebernahme bleibt das
Ergebnis dieses Spike-Gates; eine offene vorsorgliche Vollimplementierung
aller Kandidaten besteht nicht. Details stehen im Backlog-Issue #89.

Der JSON-Richtungsentscheid besitzt bewusst kein neues OD-Kuerzel:
ArduinoJson `7.4.3` ist der bevorzugte Kandidat. Die endgueltige Uebernahme
bleibt das Ergebnis des Build-, Grenzwert-, Fuzz- und Ressourcen-Spike-Gates;
eine Alternative wird nur bei einem dokumentierten Problem untersucht.

OD-09 ist fachlich entschieden. Offen bleiben keine Policyfragen, sondern der
KDF-/Zufalls-/Ressourcenspike, der anschliessende Ownerentscheid zu KDF und
Work Factor sowie der getrennte `EVALUATE_BEFORE_RELEASE`-Entscheid zur
Plattformverschluesselung. Das bestehende Zwischenreview und ein erneutes
Review bleiben vor einer Mergefreigabe zwingende Gates.
