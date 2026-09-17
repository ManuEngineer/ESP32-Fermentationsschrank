# Plan – Issue #164: R1-WLAN AP-only und Heim-WLAN ueber nativen ESP-IDF-HTTP-Pfad integrieren

## Planstatus und Provenienz

```text
ISSUE=164
SOURCE_EVALUATION_ISSUE=89
BASE_BRANCH=main
CURRENT_MAIN_SHA=7029df3997bb92e60379eb218f1894f86c5f7d55
APPROVED_EVALUATION_PLAN_SHA=74474268391b47718aa3c751d16a0d5e815efc5c
OWNER_SELECTED_CANDIDATE=NATIVE_ESP_IDF_HTTP
R1_NETWORK_SCOPE_SOURCE_COMMIT=c77049ddbf8ecba53da9693d1430bd818cb20ca4
ESP_IDF=v6.1@fff9895c82d744c7237be8847347bdd1b07c6643
PLAN_STATUS=OWNER_APPROVAL_REQUIRED
IMPLEMENTATION_AUTHORIZATION=NO
PRODUCTIVE_CONNECTIVITY_PERSISTENCE=NOT_STARTED
ACTUATOR_RELEASE=NO
```

Dieser Plan ist die eigenstaendige Umsetzungsgrundlage fuer die spaetere
produktive R1-Integration. Er ersetzt nicht die Ownerfreigabe seiner exakten
Plan-SHA. Bis zu dieser Freigabe werden weder Produktionscode noch produktive
WLAN-Persistenz, Hardwarelaeufe oder neue Clienttests ausgefuehrt.

Die Kandidatenevaluation bleibt in [Issue #89](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/issues/89),
PR #158 und
[`docs/audits/ISSUE_89_WLAN_ONBOARDING_EVIDENCE.md`](../audits/ISSUE_89_WLAN_ONBOARDING_EVIDENCE.md)
nachvollziehbar. Die daraus abgeleitete R1-Entscheidung und der Future Scope
stehen in [`docs/NETWORK.md`](../NETWORK.md) und
[`docs/FUTURE_SCOPE.md`](../FUTURE_SCOPE.md).

## 1. Ziel

Der bestehende Fermentationskern soll eine lokale R1-Netzwerkbedienung mit
genau zwei vom Benutzer am Display waehlbaren Modi erhalten:

- `AP_ONLY`: persistenter, geschuetzter SoftAP mit direkter normaler lokaler
  R1-Weboberflaeche;
- `HOME_WIFI`: temporaerer geschuetzter Setup-SoftAP zur browserbasierten
  Einrichtung von genau einem Heim-WLAN und anschliessender normaler
  Erreichbarkeit im Heim-LAN.

Der gewaehlte technische Transport ist der native ESP-IDF-Weg mit Wi-Fi,
SoftAP/STA/APSTA, Scan, `esp_http_server`, DHCP/netif und mDNS, soweit die
konkreten ESP-IDF-6.1-Adapter dafuer erforderlich sind. Die bestehende lokale
R1-Weboberflaeche bleibt die fachliche Weboberflaeche; es entsteht kein zweiter
Setup-Webserver.

## 2. Verbindlicher R1-Vertrag

### 2.1 Displayauswahl und AP-only

Beim Setup und in den normalen Netzwerkeinstellungen stehen am lokalen Display
die Optionen zur Verfuegung:

```text
INITIAL_NETWORK_MODE_SELECTION=DISPLAY
R1_NETWORK_MODE_AP_ONLY=SUPPORTED
R1_NETWORK_MODE_HOME_WIFI=SUPPORTED
HOME_WIFI_COUNT=1
HOME_WIFI_MODE=RECOMMENDED
AP_ONLY_MODE=SUPPORTED_EXPLICITLY
```

`AP_ONLY` bedeutet:

- persistenter geschuetzter SoftAP;
- normale lokale R1-Weboberflaeche direkt auf dem AP;
- mDNS beziehungsweise `*.local` als bevorzugter Komfortzugang, soweit der
  Client dies unterstuetzt;
- direkte AP-IP als verbindlicher Fallback;
- kein Captive Portal, keine App und kein CLI als Voraussetzung.

Der WLAN-QR darf den Beitritt zum geschuetzten SoftAP vereinfachen. Ein QR zum
reinen Oeffnen der Webseite ist nicht R1-pflichtig.

### 2.2 Heim-WLAN-Setup

Bei `HOME_WIFI` bleibt der temporaere geschuetzte Setup-SoftAP aktiv, waehrend
der Browserablauf stattfindet:

```text
Display waehlt HOME_WIFI
  -> Setup-SoftAP starten
  -> SSID, Passwort, QR und lokale Setup-IP anzeigen
  -> Client verbindet sich manuell oder per WLAN-QR
  -> bestehende lokale R1-Weboberflaeche per mDNS oder direkter IP oeffnen
  -> Heim-WLAN scannen und auswaehlen oder SSID manuell eingeben
  -> Passwort eingeben
  -> Kandidat vollstaendig validieren und nur volatil halten
  -> AP+STA-Verbindung mit dem Kandidaten testen
  -> bei Erfolg ueber den Projekt-Konfigurationsvertrag committen
  -> danach ueber DHCP/mDNS beziehungsweise direkte IP im Heim-LAN arbeiten
```

Der Browserzugang ist manuell und direkt; ein Captive Portal ist nicht
erforderlich. Ein fehlgeschlagener Test darf die bisherige aktive
Konfiguration nicht veraendern. Der grundlegende Reconnect zum selben
gespeicherten Heim-WLAN nach einem kurzzeitigen Ausfall gehoert zu R1.

### 2.3 Credential- und Persistenzgrenze

```text
R1_NETWORK_WEB_TRANSPORT=NATIVE_ESP_IDF_HTTP
WIFI_CREDENTIAL_OWNER=PROJECT_CONFIGURATION_DOMAIN
TEST_BEFORE_COMMIT=REQUIRED
FAILED_TEST_PRESERVES_ACTIVE_CONFIGURATION=YES
SECOND_CREDENTIAL_STORE=NO
```

ESP-IDF-Wi-Fi ist Transport und Treiber, nicht die zweite produktive
Credential-Wahrheit. Ein Kandidat darf fuer den Test volatil beziehungsweise
mit `WIFI_STORAGE_RAM` gehalten werden. Die aktive produktive Konfiguration
liegt ausschliesslich im bestehenden Projekt-Konfigurations-/Persistenzvertrag.

Der erste reale Connectivity-Konsument muss gemaess #57 ein eigenes
typisiertes, versioniertes und an `StorageEpoch` gebundenes Schema sowie seine
Commit-, Widerrufs-, Reset-, Recovery- und Redaction-Semantik definieren. Die
Integration darf keine leeren vorgezogenen Connectivity-/Authentication-
Manifeste, keinen parallelen Component-NVS-Store und keine persistenten
Pending- oder Aktivierungszweige einfuehren.

## 3. Nicht-Ziele

Nicht Teil dieser R1-Integration sind:

- Captive Portal oder automatische OS-/Browser-Erkennung;
- automatisches Ersatz-WLAN bei Verlust des Heim-WLANs;
- mehrere gespeicherte Heim-WLANs oder eine Prioritaetsliste;
- zusaetzlicher Webseiten-QR;
- neue App oder CLI;
- zweiter Webserver, zweiter Credentialstore oder paralleler Persistenzvertrag;
- produktive Internet-, Cloud- oder Fernzugriffsloesung;
- grosse Stress-, Langzeit-, Leak-, Handle-, Watchdog- oder Jittermatrix ohne
  konkreten Bedarf;
- physischer Display-/Kamera-QR-Nachweis als Voraussetzung fuer diesen Plan;
- neue Sensor-, Aktor-, GPIO-, Pegel- oder Hardwarefreigabe.

Die deferred Punkte werden nur ueber [`FUTURE_SCOPE.md`](../FUTURE_SCOPE.md)
und ein neues ownerfreigegebenes Vorhaben reaktiviert. Der physische QR-Test
bleibt `PHYSICAL_DISPLAY_QR_TEST=DEFERRED_NOT_BLOCKING_ISSUE89_SELECTION`.

## 4. Verifizierte Ausgangslage und Quellen

Die Umsetzung muss vor dem ersten Codecommit gegen den dann aktuellen Stand
von `main` revalidiert werden. Fuer diesen Plan wurde `main` live auf
`7029df3997bb92e60379eb218f1894f86c5f7d55` festgestellt. Relevant sind:

| Quelle | Verbindliche Rolle |
|---|---|
| `docs/NETWORK.md` am Scope-Sync-Commit `c77049ddbf8ecba53da9693d1430bd818cb20ca4` | aktuelle R1-Modi, lokale Adressierung, kein Captive/Fallback-AP, ein Heim-WLAN |
| `docs/WEB_UI.md` | bestehende lokale Web-UI, Sessions, CSRF, Revisionen, Servicefreigabe und Web-/Touch-Gleichheit |
| `docs/SETTINGS_AND_STORAGE.md` | fluechtige Vorschau, Test-vor-Commit, atomare Aktivierung und Rueckfallgrenze |
| `docs/CONFIGURATION_PERSISTENCE.md` und #57 | `IStateStore`, Active/Fallback, `StorageEpoch`, Root-Commit und Connectivity-Konsumentvertrag |
| `docs/ARCHITECTURE.md` | Composition Roots, Buildprofile, Safety-/Aktorgrenze und Schichten |
| `docs/ADR-013_REUSABLE_DEVICE_PLATFORM.md` | Modulrichtung und Trennung von Ports, ESP-IDF-Adaptern und App |
| `docs/ESP_IDF_UPGRADE_CONTRACT.md` | fixierter ESP-IDF-6.1-Stand, Profilisolation und finale Hardware-/Quality-Gates |
| `docs/ENGINEERING_PRINCIPLES.md` | Espressif-first, Adopt-or-build, Testbarkeit und Ressourcenprovenienz |

Vor der Umsetzung sind insbesondere die aktuelle Implementierung von
`IStateStore`, Konfigurationsgraph, `ConfigurationPreview`,
`ConfigurationMutationCoordinator`, vorhandene Web-Commands sowie die
Composition Roots erneut zu lesen. Der Plan fuehrt dafuer keine parallele
Schatten-API ein.

## 5. Architektur- und Modulgrenzen

Die Umsetzung folgt ADR-013:

```text
src/main.cpp
    native-only Test-Composition-Root

main/app_main.cpp
    ESP-IDF-Composition-Root

device_platform
    schmale, anwendungsneutrale Ports fuer die benoetigte Netzwerkkapazitaet

device_platform_esp_idf
    konkrete Wi-Fi-, netif-, DHCP/mDNS- und HTTP-Adapter

fermentation_app
    keine ESP-IDF-, WLAN-, Dateisystem- oder direkten HTTP-Aufrufe;
    nur fachliche Konfiguration/Commands, falls nach Quellenpruefung dort
    erforderlich

device_platform_test_support
    nur native Mocks, Simulation und Fehlerinjektion
```

Die konkrete Aufteilung neuer Typen wird erst nach der Revalidierung der
vorhandenen Ports entschieden. Sie muss folgende Regeln einhalten:

- kein ESP-IDF-Header im Fachkern;
- keine Fermentationsbegriffe im anwendungsneutralen Plattformport;
- keine Test-Support-Abhaengigkeit im Produktionsgraphen;
- keine Netzwerkabhaengigkeit fuer Regelung, Safety oder Aktorplanung;
- kein zweiter HTTP-Lifecycle neben der bestehenden R1-Weboberflaeche;
- keine Composition-Root-Logik in UI-, Adapter- oder Fachmodulen.

## 6. Persistenz-, Preview- und Commitvertrag

Vor produktivem Code ist ein kleiner Vertragsdelta-Commit erforderlich, falls
die bestehenden kanonischen Konfigurationsdokumente die folgenden Werte noch
nicht aufnehmen. Der Delta muss mindestens definieren:

- Netzwerkmodus `AP_ONLY | HOME_WIFI`;
- genau eine aktive Heim-WLAN-Identitaet und die zugehoerige geschuetzte
  Credential-Domaene;
- Schema, Inhaltsrevision, `StorageEpoch`, Redaction und Schluesselraum;
- Trennung von nicht-geheimem Netzwerkstatus, Webkonfiguration und WLAN-Secret;
- volatile Preview fuer SSID/Passwort und erwartete aktive Basis;
- vollstaendige Validierung vor jedem Write;
- Teststatus und Commitgrenze ohne persistentes Pending- oder Aktivierungsintent;
- Verhalten bei Write-, Readback- und `CommitOutcomeUnknown`-Fehler;
- Widerruf bei Modus-/Credentialwechsel, Neustart, Reset und Recovery;
- Ausschluss der WLAN-Secrets aus normalem Backup, Diagnose und Logs.

Der fachliche Ablauf lautet:

```text
Browser-/Displayentwurf
  -> typisierte Eingabe und Wertepruefung
  -> fluetige Preview mit erwarteter aktiver Revision
  -> Ressourcen und Kandidatenpfad vollstaendig vorbereiten
  -> Kandidat im AP+STA-/RAM-Pfad testen
  -> bei Fehler Preview verwerfen, aktive Konfiguration unveraendert lassen
  -> bei Erfolg final validieren und ueber den bestehenden Root-/Graphvertrag
     atomar aktivieren
  -> vorbereiteten Runtime-Snapshot ohne neue fallible Arbeit publizieren
```

Ein Neustart verwirft einen uncommitteten Kandidaten. Ein unbestimmter
persistenter Commitzustand wird nicht als Erfolg oder Rueckfall geraten,
sondern bleibt gemaess #56/#57 fail-closed, bis ein vollstaendiger Readback den
Zustand eindeutig aufloest. Alte `StorageEpoch`s duerfen nie reaktiviert
werden.

## 7. Umsetzungs- und Commit-Schnitte nach Planfreigabe

Die folgenden Schnitte sind eine Reihenfolge, keine bereits erteilte
Implementierungsfreigabe:

1. **Vertragsrevalidierung und Delta** – aktuellen `main` pruefen, bestehende
   Konfigurations-/Web-/Portvertraege abgleichen und nur notwendige typisierte
   Netzwerkfelder, Fehler und Commitgrenzen ergaenzen.
2. **Port- und Preview-Schicht** – schmale anwendungsneutrale Faehigkeiten fuer
   Modus, Scan, Kandidatenverbindung, Status und Lifecycle definieren; native
   Mocks und Fehlerinjektionen fuer Commit-/Reconnect-Faelle ergaenzen.
3. **ESP-IDF-Adapter** – SoftAP, STA/APSTA, Scan, DHCP/netif, mDNS und
   Ressourcen-/Fehlerpfade am fixierten ESP-IDF-6.1-Stand adaptieren. AP-only
   und Setup-AP muessen denselben expliziten Adapter-Lifecycle verwenden.
4. **Bestehende Web-UI anbinden** – vorhandene rendererunabhaengige Commands,
   Sessions, CSRF, Revisionen und Servicegrenzen verwenden; Setup-Route und
   normale AP-only-Route in denselben Webserver integrieren.
5. **HOME_WIFI-Workflow** – SSID-Auswahl/manuelle Eingabe, fluechtige
   Credentials, AP+STA-Test, positive Testauswertung, Fehlerabbruch und
   Preserve-on-failure implementieren. Kein Commit vor dem Test.
6. **Aktivierung, Boot und Reconnect** – atomare Aktivierung ueber den
   Projektvertrag, Boot des gespeicherten Heim-WLANs, mDNS/DHCP/direct-IP-
   Anzeige und grundlegenden Reconnect zum selben WLAN integrieren.
7. **AP-only-Modus** – persistente Modusentscheidung, geschuetzten SoftAP,
   normale R1-Web-UI und direkte AP-IP als verbindlichen Zugang integrieren.
8. **Nachweise und Dokumentation** – geaenderte SSOTs, Roadmap, Issue, PR und
   Handover synchronisieren; keine historische Spike-Evidence als
   Implementierungsnachweis umetikettieren.

Jeder Schnitt bleibt reviewbar und darf keine zweite Konfigurationswahrheit
einfuehren. Falls die Vertragsrevalidierung eine materielle Abweichung von
diesem Plan zeigt, ist der Plan vor weiterer Umsetzung zu revidieren und neu
freizugeben.

## 8. Safety, Security, Recovery und Hardware

- Regelung, Safety und lokale Bedienung bleiben ohne Netzwerk lauffaehig.
- Boot, Reset, unbekannter Zustand, unlesbare Persistenz und fehlgeschlagener
  Commit fuehren zu keiner Aktorfreigabe.
- Der Netzwerkadapter darf niemals den `ActuationInterlock`, Aktorplaner,
  Watchdog-Latch oder Safety-Fehlerzustand umgehen.
- AP- und Setup-Passwoerter sind geraetespezifisch und geschuetzt; Werte werden
  nur volatil angezeigt/eingegeben und nicht in Repository, PR, normalen UART-
  Logs, URLs, Diagnose oder Backups geschrieben.
- Direktes lokales HTTP hat die in `NETWORK.md` und `WEB_UI.md` beschriebene
  lokale Vertrauensgrenze; kein Internetzugriff oder TLS-Versprechen wird
  hinzugefuegt.
- Reset-, Recovery- und Werksresetverhalten bindet die neue Domaene an die
  bestehende `StorageEpoch`; kein automatischer Reset und kein stiller
  Credential-Rollback.
- Keine eFuse-, Secure-Boot-, Flash-Encryption- oder ROM-Download-Aenderung.
- Keine GPIO-, Display-, Sensor-, 12-V-, MOSFET-, Aktor- oder
  Hardwarefreigabeentscheidung in diesem Plan.
- Reale ESP32-Nachweise erfolgen nur auf dem finalen autorisierten
  Implementierungs-HEAD und mit gesperrten Aktoren gemaess ESP-IDF-Vertrag.

## 9. Gezielte Tests und Evidence nach Ownerfreigabe

Die folgenden Nachweise fuer die #164-Umsetzung werden erst nach Planfreigabe
und autorisierter Umsetzung erhoben; der vorliegende Auftrag fuehrt sie nicht
aus. Historische Kandidaten-Evidence aus #89 bleibt davon getrennt. Die neuen
Nachweise sind proportional und auf dem exakten finalen HEAD zu erheben.

### Native und Vertragsnachweise

- Modusvalidierung und genau-ein-Heim-WLAN-Regel;
- volatile Preview, Abbruch und Neustartverwerfung;
- Commit erst nach erfolgreichem Kandidatentest;
- falsches Passwort, Scan-/Apply-Fehler, Timeout und Abbruch ohne Mutation der
  aktiven Konfiguration;
- Write-/Readback-/`CommitOutcomeUnknown`-Pfad, `StorageEpoch` und Recovery;
- Redaction sowie Ausschluss aus Backup, URL, Diagnose und UART;
- kein zweiter Store und keine Netzwerkabhaengigkeit des Fach-/Safety-Kerns;
- native Konsumententests und Architekturgrenzen.

### ESP-IDF-, Host- und Clientnachweise

- getrennte `esp32_bringup`-/`esp32_release`-Builds mit fixiertem ESP-IDF 6.1;
- Buildprofilpruefung, gezielte Static Analysis und relevante Quality Gates;
- unbelasteter ESP32-Transportstart mit AP-only und Setup-AP, ohne
  Aktorfreigabe;
- AP+STA-Kandidatentest bei erhaltenem Setup-Pfad;
- Android als bestehende minimale Clientreferenz am finalen Produktpfad;
- mDNS, direkte lokale IP, Boot mit gespeichertem Heim-WLAN und grundlegender
  Reconnect zum selben WLAN;
- iOS/iPadOS und Windows bleiben gemaess Ownerentscheidung waived, sofern der
  Owner das Gate nicht ausdruecklich neu oeffnet;
- physischer Display-/Kamera-QR bleibt deferred und nicht auswahlblockierend;
- vollstaendige Heap-/Stack-/Clientlast-/Leak-/Watchdog-/Jitterqualifikation
  erst nach produktiver Integration und nur bei belastbarem Bedarf.

Bei jeder Evidenz werden Candidate-/Host-/Client-/Hardware- und
Produktionsnachweis getrennt ausgewiesen. Nicht ausgefuehrte Tests werden
nicht als bestanden bezeichnet.

## 10. Owner- und Review-Gates

Die Reihenfolge ist verbindlich:

1. unabhängige Planpruefung dieser eigenstaendigen Planfassung;
2. Ownerfreigabe der exakten `INTEGRATION_PLAN_SHA`;
3. separater Implementierungsbranch und Draft-PR fuer #164;
4. Umsetzung nur innerhalb dieses Plans und seiner freigegebenen Dateigrenzen;
5. Builder-Self-Check auf dem finalen Implementierungs-HEAD;
6. unabhängiger Full Review mit `OPEN_BLOCKERS=0`;
7. ausdrueckliche Ownerautorisierung des lokalen Pre-Ready-Laufs;
8. Owner setzt gegebenenfalls `Ready for review`, danach GitHub-CI und
   Merge-Gate gemaess `docs/CI_AND_QUALITY_GATES.md`.

Der Builder setzt den PR nicht selbst auf Ready, merged nicht und aktiviert
kein Auto-Merge. Eine Kandidatenauswahl findet in diesem Plan nicht erneut
statt; sie ist mit `NATIVE_ESP_IDF_HTTP` abgeschlossen. Bis zur
Implementierungsfreigabe bleibt:

```text
PRODUCTIVE_CONNECTIVITY_PERSISTENCE=NOT_STARTED
PRODUCTIVE_IMPLEMENTATION=NOT_STARTED
ACTUATOR_RELEASE=NO
```

## 11. Abnahmekriterien fuer Issue #164

Issue #164 ist erst fachlich integriert, wenn mindestens Folgendes am
freigegebenen finalen Implementierungs-HEAD nachgewiesen ist:

- Displayauswahl zwischen `AP_ONLY` und `HOME_WIFI` funktioniert ohne
  Netzwerkvoraussetzung;
- AP-only stellt einen geschuetzten persistenten SoftAP und die normale lokale
  R1-Web-UI bereit; direkte AP-IP ist erreichbar;
- HOME_WIFI bietet Setup-AP, Browserformular, Scan oder manuelle SSID,
  volatilen Kandidaten und AP+STA-Test;
- ein Testfehler bewahrt die aktive Konfiguration; Commit erfolgt erst nach
  positivem Test ueber den Projektvertrag;
- genau ein Heim-WLAN wird unterstuetzt, beim Boot geladen und zum selben WLAN
  grundlegend wiederverbunden;
- DHCP/mDNS und direkte IP entsprechen `NETWORK.md`;
- bestehende Web-Session-, CSRF-, Revisions-, Service- und Safetyregeln bleiben
  wirksam;
- keine zweite Web-/Credential-/Persistenzwahrheit und keine Aktorfreigabe
  wurden eingefuehrt;
- relevante native, ESP-IDF-, Host-, Client-, Security- und Dokumentations-
  Nachweise sind am exakten finalen HEAD synchronisiert.

Nachweisliche offene Future-Punkte bleiben `FUTURE_SCOPE` und blockieren diese
R1-Abnahme nicht. Eine spaetere Erweiterung benoetigt ein neues Issue, einen
neuen Plan und eine neue Ownerentscheidung.
