# Plan – Issue #89: WLAN-Onboarding und Provisionierung evaluieren

## Planstatus und harte Basis

Dies ist ein Plan-only-Artefakt. Es waehlt keine WLAN-Onboarding-Loesung,
fuehrt keine produktive Provisionierung ein und startet weder einen vollstaendigen
Pre-Ready-Lauf noch einen Ready-, Merge-, Issue-Schluss- oder
Aktorfreigabevorgang. Die Umsetzung beginnt erst nach ausdruecklicher
Ownerfreigabe der exakten Commit-SHA dieses vollstaendigen Plans.

~~~
ISSUE=89
TITLE=[E5.6] ESP-IDF-WLAN-Onboarding und Provisionierung evaluieren
BASE_BRANCH=main
BASE_SHA=2c010e8a8be8e351f89b79ae6c74f665d24a1f0e
EXPECTED_BASE_SHA=2c010e8a8be8e351f89b79ae6c74f665d24a1f0e
PLAN_STATUS=OWNER_PLAN_FIX_VERIFICATION_PENDING
PLAN_SHA=EXACT_COMMIT_RECORDED_IN_PR_AND_SESSION_HANDOVER
REVIEWED_PLAN_SHA=d0307728a7d587236414c3489638206f29ebd811
REUSE_BEFORE_BUILD=REQUIRED
BROWSER_ONLY_REMAINS_HARD_REQUIREMENT=OWNER_GATE_PENDING
IMPLEMENTATION=NOT_STARTED
CANDIDATE_SELECTION=OWNER_PENDING_AFTER_COMPARABLE_EVIDENCE
ACTUATOR_RELEASE=NO
~~~

Verifizierte Kontextbaseline:

~~~
CONTEXT_BASELINE_BRANCH=agent/issue-89-wlan-onboarding-plan
CONTEXT_BASELINE_SHA=2c010e8a8be8e351f89b79ae6c74f665d24a1f0e
CONTEXT_HEAD_SHA=2c010e8a8be8e351f89b79ae6c74f665d24a1f0e
CONTEXT_PLAN_SHA=EXACT_COMMIT_RECORDED_IN_PR_AND_SESSION_HANDOVER
CONTEXT_REFRESH_MODE=FULL
CONTEXT_DELTA=origin/main auf Basis-SHA, Live-Issue #89, Live-PR #157,
  Issue #154 sowie die direkt betroffenen Netzwerk-, Konfigurations-,
  Architektur-, Hardware- und Spike-Vertraege
SOURCE_OF_TRUTH_CONFLICT=NONE
~~~

Der Live-Abgleich vor der Planerstellung ergibt:

- Issue #89 ist offen und traegt PLANNED_SPEC_PENDING.
- PR #157 ist gemergt; PR157_MERGE_COMMIT ist exakt
  2c010e8a8be8e351f89b79ae6c74f665d24a1f0e.
- Die strukturelle #106-Arbeit ist damit in main; der produktive #106-Abschluss
  bleibt an #35 gebunden und ACTUATOR_RELEASE bleibt NO.
- Issue #154 ist geschlossen und keine offene parallele Governance-Arbeit.
- Issue #27 bleibt offen und ist ein getrennter Web-API-/Weboberflaechen- und
  Authentisierungsscope.
- Issues #31, #30, #32 und #33 sind offen und hardwareblockiert. #89 besitzt
  die abgeschlossenen digitalen Grundlagen aus #29 und #57, benoetigt fuer
  Host-/Actor-free-Evidence keine neue Sensor-, Display- oder Aktorverkabelung,
  fuer reale WLAN-/QR-Abnahme aber einen nachweisbaren ESP32-Aufbau und die
  jeweiligen Clientgeraete.

## 1. Ziel und Nicht-Ziele

### Ziel

Issue #89 liefert eine vergleichbare, reproduzierbare Entscheidungsgrundlage
fuer einen R1-Onboardingpfad und die offene Frage, ob der browserbasierte
Zugang harte R1-Anforderung bleibt. Dieselbe R1-Anforderung wird gegen alle
vier Kandidaten geprueft:

1. espressif/network_provisioning 1.2.4 auf Basis von protocomm;
2. direkter protocomm-/ESP-IDF-SoftAP-/HTTP-/DNS-Pfad ohne
   network_provisioning;
3. kleiner eigener nativer ESP-IDF-SoftAP-/DNS-/HTTP-Adapter;
4. WiFiManager v2.0.17 als zusaetzlicher konditionaler
   Drittanbieter-Evaluationskandidat.

Der Nachweis muss zeigen, ob der jeweilige Kandidat den browserbasierten
R1-Vertrag ohne verpflichtende App, Cloud oder separates CLI-Werkzeug erfuellt.
Ein ESP-IDF- oder Espressif-Herkunftsnachweis allein ist kein Browser-PASS.

Der Plan umfasst:

- den gemeinsamen browserbasierten SoftAP-/Captive-Portal-Vertrag;
- ausdruecklichen Portalstart, individuelle geschuetzte SoftAP-Zugangsdaten,
  gaengigen WLAN-QR und direkten IP-Fallback;
- WLAN-Scan, Eingabe, Validierung, Verbindungstest und bewussten Commit;
- Secret-Redaction, Credential-Lebenszyklus, Neustart, Fehler und Recovery;
- identische Funktion-, Kompatibilitaets-, Lizenz-, Abhaengigkeits-,
  Ressourcen-, Testbarkeits- und Integrationsmessungen;
- actor-free Host-/ESP32-Evidence sowie reale Tests mit Android, iOS/iPadOS
  und Windows;
- einen harten Owner-Entscheidungspunkt nach der gemeinsamen Evidence.

### Nicht-Ziele

- keine Vorabentscheidung fuer einen der vier Kandidaten;
- keine vollstaendige normale Web-API, Weboberflaeche, Webanmeldung, Service-
  PIN-, Session-, CSRF- oder Konfliktimplementierung aus #27;
- keine zweite Konfigurations-, Credential- oder Webserverwahrheit;
- keine Credentialspeicherung durch eine Bibliothek oder ein Framework ohne
  projektspezifische Validierung und Commit;
- kein BLE-, SmartConfig-, Cloud-, App- oder OTA-Provisioning in R1;
- keine automatische Portaleroeffnung nur wegen eines kurzen Router-,
  Access-Point-, WLAN- oder Internetausfalls;
- keine Aenderung an GPIO-, Sensor-, Aktor-, Display- oder Verdrahtungs-SSOT;
- keine produktive Webserver-, DNS-, QR- oder Provisioningplattform auf Vorrat;
- kein Claim von Flashverschluesselung, realer Flashatomizitaet,
  Flashlebensdauer, Hardwarefunktion oder Aktorfreigabe ohne den jeweiligen
  Nachweis.

### Harte R1-Ergebnisse und offene Implementierungswahl

Die folgenden Ergebnisse sind Produktziele und bleiben fuer jeden Kandidaten
gleich:

- lokale Einrichtung ohne Cloudzwang;
- geschuetzter, geraetespezifischer Einrichtungszugang;
- eine bestehende funktionierende Heim-WLAN-Konfiguration wird bei einem
  fehlgeschlagenen Wechsel nicht unbemerkt zerstoert;
- kein Secret-Leak in Logs, URLs, Diagnose oder Backups;
- definierter Werksreset und definierte Recovery;
- Netzwerk bleibt unabhaengig von Regelung und Safety;
- direkte lokale Recovery- und Zugriffsmöglichkeit.

Diese Ziele legen weder eine Connectivity-Domaene noch deren Persistenz- oder
Lifecyclebesitz fest. Vor dem Owner-Gate werden insbesondere kein eigener
Credential-Record, keine Slotrotation, kein eigener DNS-Responder, kein
eigener Reconnect-Automat und kein eigener HTTP-/Portalserver als
Umsetzungsrichtung geplant. Eine vorhandene Loesung darf diese Teile nur dann
uebernehmen, wenn ihre native Semantik den bestaetigten R1-Ergebnissen und den
nachfolgend zu pruefenden #57-/Security-/Resetvertraegen genuegt.

## 2. Verbindliche Quellen und wiederzuverwendende Grundlagen

Die Umsetzung liest und verwendet diese Quellen unveraendert, soweit der
Plan nicht ausdruecklich eine additive #89-Ergaenzung vorsieht:

| Verantwortung | Kanonische Quelle / Wiederverwendung |
|---|---|
| R1-WLAN-Verhalten | docs/NETWORK.md, insbesondere Ersteinrichtung, Ersatz-WLAN, QR, direkte IP, DHCP/mDNS und lokaler HTTP |
| Browser-/Webgrenzen | docs/WEB_UI.md und Issue #27; #89 liefert nur den Onboarding-Transportvertrag |
| Konfigurationspersistenz | docs/CONFIGURATION_PERSISTENCE.md, docs/SETTINGS_AND_STORAGE.md und ADR-016 |
| erster Connectivity-Konsument | Issue #57; native Credential-/Persistenz-/Resetsemantik zuerst gegen die R1-Ergebnisse pruefen, projektspezifischen Delta-Vertrag erst nach Owner-Gate |
| Recovery und fail-closed | docs/RECOVERY_AND_INTERRUPTION.md, docs/SYSTEM_SAFETY_AND_RECOVERY.md und bestehende ConfigurationRecovery-/ActuationInterlock-Producer |
| Architektur | ADR-013, docs/ARCHITECTURE.md und die lokalen Modulregeln |
| Adopting vor Eigenbau | docs/ENGINEERING_PRINCIPLES.md, docs/ADOPT_OR_BUILD.md und das Komponenteregister |
| WLAN-Spike | docs/audits/HARDWARE_SPIKE_PLAN.md, Spike C |
| Toolchain und Hardwarebaseline | ESP-IDF v6.0.2, Commit 7101770dc6db2667b3c477cc31365dd1acd6db4e, Profile esp32_bringup/esp32_release, 4 MB Flash, kein PSRAM, APP_REAL_ACTUATORS_ENABLED=0 |
| persistenter Port | device_platform::IStateStore, StateStoreKey, StorageEnvelope, StorageEpoch und die vorhandene ConfigurationMutationCoordinator-Instanz |
| Zufall | der vorhandene device_platform::ISecureRandomSource-Port; die konkrete ESP-IDF-Quelle wird erst im Adapter-/Spike-Nachweis verifiziert |
| aktueller Arbeitsstand | docs/ROADMAP.md und Live-Issues/PRs; historische Planangaben werden nicht als aktueller Status verwendet |

Das native Fachmodul kennt weder ESP-IDF-, protocomm-, WiFiManager-,
HTTP-, DNS-, QR- noch Bibliothekstypen. Konkrete Frameworktypen enden an der
ESP-IDF-Adaptergrenze. Eine allgemeine IWebTransport-, Provisioning-,
Provider- oder Pluginplattform wird nicht eingefuehrt.

## 3. Gemeinsamer R1-Browservertrag

Jeder Kandidat wird mit demselben actor-free Prototypvertrag und denselben
Eingaben, Zeitlimits, Clients, Fehlern und Cut-Points bewertet. Der
Prototyp ist ein Evidence-Artefakt, keine zweite Produktionsanwendung. Der
Browserablauf ist bis zum Gate in Abschnitt 4.4 die Vergleichsbasis; dort
entscheidet der Owner, ob `BROWSER_ONLY_REMAINS_HARD_REQUIREMENT=YES` bleibt
oder ein ausdruecklich geaenderter Standardclientvertrag gilt.

### 3.1 Portalstart und Lebenszyklus

Der Ablauf besitzt klar unterscheidbare Gruende:

| Anlass | Verhalten |
|---|---|
| kein bestaetigter Connectivity-Record beim fabrikneuen beziehungsweise vollstaendig initialisierten Geraet | geschuetztes individuelles Einrichtungs-SoftAP mit Portal anbieten |
| ausdrueckliche lokale Benutzeraktion | Onboarding-Portal kontrolliert starten; bestehende funktionierende Credentials bleiben bis zum Commit aktiv |
| kurzer oder voruebergehender Heim-WLAN-Ausfall | kein Portalstart und keine Credentialmutation |
| langer Heim-WLAN-Ausfall nach noch festzulegender TBD_COMMISSIONING-Wartezeit | separates geschuetztes Ersatz-WLAN; weiterhin kein automatischer Credential-Commit |
| Read-/Write-/Recoveryfehler oder unklarer Speicherzustand | kein sicherer Credential- oder Runtimeclaim; fail-closed klassifizieren und keinen Factory-Fallback erfinden |

Portal, DNS und SoftAP werden explizit gestartet und kontrolliert beendet.
Ein Bibliothekscallback darf keinen versteckten automatischen
Credential-Commit und keinen ungeprueften Portalstart ausloesen. Ein
Ersatz-WLAN ist ein eigener Netzwerklebenszyklus und keine Umdeutung eines
kurzen Netzausfalls in eine Ersteinrichtung.

### 3.2 Browserablauf

Der Browserablauf muss ohne Spezialsoftware funktionieren:

1. SoftAP mit individueller SSID und individuellem geschuetztem Passwort
   starten;
2. SSID, SoftAP-Passwort, aktuelle Portaladresse beziehungsweise AP-IP und
   eine Aktion zum erneuten Anzeigen des QR sichtbar machen;
3. WLAN-QR im gaengigen WLAN-QR-Format mit korrektem Escaping erzeugen;
4. nach dem Beitritt die Captive-Portal-Erkennung bedienen, soweit der Client
   sie anbietet;
5. dieselbe Seite zusaetzlich direkt ueber die angezeigte lokale IP-Adresse
   erreichen koennen;
6. verfuegbare Heim-WLANs scannen und anzeigen;
7. SSID auswaehlen oder manuell eingeben, Passwort verdeckt eingeben und
   Eingabe abbrechen koennen;
8. eine begrenzte Verbindung pruefen, ohne die alte funktionierende
   Konfiguration zu ueberschreiben;
9. erst nach erfolgreichem Test und ausdruecklicher Bestaetigung den
   projektspezifischen Commitpfad aufrufen;
10. Erfolg, Fehler, Abbruch, Timeout und Wiedereroeffnung ohne geheime Werte
    anzeigen.

Die Captive-Portal-Erkennung wird als Clientverhalten gemessen, nicht als
universelle Garantie behauptet. Der direkte IP-Aufruf bleibt der
verbindliche manuelle Rueckfall. Zugangsdaten stehen nicht in URLs,
Redirect-Parametern, HTML-Fehlermeldungen, Browserhistory oder Logs.

### 3.3 Ersatz-WLAN

Die Evaluation prueft den in NETWORK.md beschriebenen separaten
Ersatz-WLAN-Lebenszyklus:

- kurzer Ausfall startet nichts;
- langer Ausfall startet nach dem dokumentierten,
  bis zur Messung weiterhin TBD_COMMISSIONING-Wert;
- Heim-WLAN-Reconnect laeuft parallel;
- offene Requests und Speichervorgaenge werden kontrolliert beendet oder
  abgeschlossen;
- normale Web-, Auth-, CSRF-, Lauf- und Safetygrenzen bleiben wirksam;
- stabile Heim-WLAN-Rueckkehr beendet das Ersatz-WLAN nach kontrollierter
  Uebergangszeit;
- Neustart im Ersatz-WLAN aktiviert keine unbestaetigte neue Credentialversion.

Die Warte- und Uebergangszeiten werden im Spike gemessen und als
Produktparameter nur nach separatem Ownerentscheid festgelegt. Sie werden
nicht als erfundene Produktivwerte in diesen Plan geschrieben.

## 4. Ergebnisoffene Kandidatenpruefung

### 4.1 Kandidaten und Browsernachweis

| Kandidat | Wiederzuverwendender Anteil | Browsernachweis im identischen Spike | Harte Ablehnung |
|---|---|---|---|
| network_provisioning 1.2.4 | offizieller Espressif-Baustein auf protocomm, ESP-IDF-WLAN-/Event-/Netif-Dienste und esp_http_server soweit erforderlich | zeigen, dass der komplette R1-Ablauf im Browser ueber SoftAP, DNS, HTTP-Formular, QR und direkte IP funktioniert; eine zwingende mobile App, Cloud oder CLI ist ein FAIL | kein kontrollierbarer Browserpfad, versteckter Auto-Commit, unkontrollierbare Credentialablage oder kein ESP-IDF-6.0.2-Nachweis |
| direkter protocomm-/SoftAP-/HTTP-/DNS-Pfad | protocomm, esp_wifi, esp_netif, esp_event, esp_http_server und vorhandene LWIP-/DNS-Funktionen | protocomm-Transport und Browserseite mit denselben Requests, Formularen, Scan-/Test-/Commit- und Fehlerfällen nachweisen; Browser darf kein separates CLI benoetigen | protocomm-only-Appvertrag, fehlende Browserinteroperabilitaet, fehlende Begrenzbarkeit oder wesentliche Zusatzabhaengigkeit |
| kleiner eigener nativer ESP-IDF-Adapter | nur nach dem Reuse-Screen und Owner-Gate fuer eine konkret nachgewiesene Restluecke; bis dahin ausschliesslich isolierte Spike-Harness | kleiner nativer Browserablauf mit identischen Seiten-/Endpoint-/Fehler- und IP-Fallback-Anforderungen | eigener Parallelserver ohne Lueckennachweis, fehlender Browser-/Clientnachweis oder unvertretbare Ressourcen-/Wartungslast |
| WiFiManager v2.0.17, Commit d82d0a1b | nur nachgewiesener ESP-IDF-6.0.2-Integrationspfad; keine stillschweigende Rueckkehr zum Arduino-Produktionspfad | Standard-/angepasster Portalablauf muss ebenfalls individuelle SoftAP-Credentials, QR, DNS, direkte IP, expliziten Start und Projekt-Commit im Browser zeigen | kein direkter ESP-IDF-6.0.2-Build/Betrieb beziehungsweise kein dokumentierter Integrationsweg ohne Arduino-Produktionspfad; App-/Cloud-/CLI-Zwang; unkontrollierbare Bibliotheksdefaults |

Der WiFiManager-Test ist konditional, aber nicht vorab abgewertet. Das
Evaluationsgate ist inhaltlich identisch; die Espressif-first-Reihenfolge
bestimmt die Pruefprioritaet, nicht das Ergebnis.

### 4.2 Reuse-/Capability-Screen vor eigenem Code

Vor jeder produktiven projektspezifischen Festlegung wird je Funktion in
folgender Reihenfolge
geprueft und dokumentiert:

1. eingebaute ESP-IDF-6.0.2-Dienste: esp_wifi, esp_netif, esp_event,
   esp_http_server, LWIP-DNS-/Socketpfad, esp_timer und
   esp_fill_random beziehungsweise die verifizierte Zufallsquelle;
2. offizielle Espressif-Komponenten und Repositories, insbesondere
   network_provisioning und protocomm;
3. die bereits registrierten oder aktuell gescreenten gepflegten
   Drittkomponenten mit nachvollziehbarer Lizenz;
4. erst nach dem Owner-Gate der kleinstmoegliche projektspezifische Adapter
   fuer eine belegte Restluecke.

Der QR-Code wird als gesonderte, begrenzte Presentation-/Codecentscheidung
behandelt. Er darf weder einen WLAN-Kandidaten noch eine eigene Web-/Storage-
Architektur erzwingen. Jeder Screen dokumentiert verwendete, deaktivierte und
transitive Komponenten sowie die Frage, ob native Persistenz, Lifecycle,
Reset und HTTP-Server-Sharing kontrollierbar an die Projektgrenzen
uebergeben werden koennen.

### 4.3 Aktueller nativer ESP-IDF-Drittanbieter-Screen

Dieser kurze Screen wurde am 2026-09-14 read-only gegen die jeweiligen
Repository-Metadaten, README, Component-Manifest und die angegebene
Quellcodebasis durchgefuehrt. Er ist kein vollstaendiger Hardware-Spike und
keine Produktivauswahl. Die angeforderten Koordinaten
`thorrak/esp_wifi_config`, `tuanpmt/esp_wifi_manager` und
`nordesems/esp-captive-portal` werden mit ihrem live aufgeloesten
Repository-/Commitstand festgehalten.

| Screen-Kandidat und Snapshot | Bereits sichtbarer Vorteil / relevante Teile | Native Semantik und Integrationsrisiko | Lizenz, IDF, Wartung und Ressourcen | Weiteres Gate |
|---|---|---|---|---|
| `thorrak/esp_wifi_config` -> live `WiFiConfig/esp_wifi_config`; Version 0.4.0; [HEAD 32c78805e9fc206610b7debe31d06638cbe5da09](https://github.com/WiFiConfig/esp_wifi_config/commit/32c78805e9fc206610b7debe31d06638cbe5da09) | Liefert SoftAP, Captive Portal/DNS, eingebettete Web-UI, Scan, Reconnect/Lifecycle, NVS-Netzwerk-/AP-/Authspeicher und einen dokumentierten Shared-HTTPD-Einstieg (`examples/with_shared_httpd`). | Die Bibliothek besitzt eigene NVS-Keys, Credentials, AP-Konfiguration, Auto-Commit-/Reconnect- und Factory-Resetpfade; Kompatibilitaet mit projektverwaltetem #57-Commit, Redaction, Widerruf, Recovery und Reset ist nicht belegt. | MIT; Component-Manifest `idf >=5.4`, IDF-6.x zieht `espressif/network_provisioning` bedingt hinzu; aktive Pflege am Snapshotdatum, Ressourcen und genaue IDF-6.0.2-Ausfuehrung noch ungemessen. | `DEEP_SPIKE=CONDITIONAL_YES`: nur wenn ein no-parallel-NVS-/Credentialpfad, Shared-HTTPD und kontrollierter Lifecycle gegen #57 bestehen; sonst Screen-Fail, keine Hardwarematrix. |
| `tuanpmt/esp_wifi_manager`; Version 1.1.0; [HEAD 20f77d79e9cdde9e4d3f0c3c7a3bd3babfaf893a](https://github.com/tuanpmt/esp_wifi_manager/commit/20f77d79e9cdde9e4d3f0c3c7a3bd3babfaf893a) | Liefert SoftAP, Captive Portal/DNS, Web-UI, Scan, Multi-Network-Reconnect/Lifecycle, REST/CLI/BLE und Factory-Reset; ein bestehender HTTPD kann laut API geteilt werden. | Eigene NVS-Wahrheit fuer Netzwerke, AP, Variablen und Auth; `esp_bus`-Eventarchitektur, Default-AP mit leerem Passwort und unredigierte REST-/Config-Oberflaechen sind gegen R1/#57/Security/Reset zu pruefen. | MIT; Component-Manifest `idf >=5.0`, zusaetzlich `tuanpmt/esp_bus` und `espressif/mdns`; Pflege und Codeaktivitaet vorhanden, aber IDF-6.0.2, Ressourcen und kontrollierte Abschaltung ungemessen. | `DEEP_SPIKE=CONDITIONAL_NO`: nur aufnehmen, wenn der Nachweis einen konkreten Vorteil gegenueber WiFiConfig oder dem nativen Teilkomponentenpfad zeigt; sonst Screen-only, keine Hardwarematrix. |
| `nordesems/esp-captive-portal`; Version 1.3.0; [HEAD b937ee88b86de47b40cd195f829cfd70e5af03c0](https://github.com/Nordesems/esp-captive-portal/commit/b937ee88b86de47b40cd195f829cfd70e5af03c0) | Kleine Teilkomponente fuer Captive-Portal-DNS, DHCP Option 114 und Standard-OS-Probes; registriert sich auf einem bereits laufenden `esp_http_server` und unterstuetzt direkte IP-Weiterleitung. SoftAP, Credentialeingabe, Scan, Reconnect und Portalinhalt bleiben beim aufrufenden Projekt. | Keine Credential-Persistenz, kein WLAN-Scan, kein Reconnect und kein vollstaendiger Portal-/Commitablauf; Reset und Storage bleiben Projektbesitz. DNS-Lifecycle und URI-Slots/Handler-Reihenfolge bei HTTP-Sharing muessen geprueft werden. | MIT; Component-Manifest `idf >=5.0`; kein eigener NVS-/Storagebestand und keine externen Drittdeps laut Manifest, aber ESP-IDF-HTTP-/Event-/WiFi-/FreeRTOS-Dienste; DNS-Task und Handlerressourcen sind noch zu messen. | `DEEP_SPIKE=CONDITIONAL_SUBCOMPONENT`: nur als Teilkomponente des direkten/native Pfads, wenn sie gegen eigenen DNS-/Probe-Code einen realen Vorteil liefert; kein eigenstaendiger End-to-End-Kandidat. |

Die Tabelle trennt dokumentierte Capability vom noch unbewiesenen
Produktvertrag. Insbesondere sind NVS-Keybestand, Reset-API oder ein
automatischer Reconnect kein Nachweis fuer sichere Projektpersistenz. Nur ein
Kandidat mit dem jeweils genannten realen Vorteil erreicht einen vertieften
actor-free Spike; dadurch bleiben die vier bestehenden Hauptkandidaten und
hoechstens die begruendet shortlisted Teilkomponenten im Vergleich.

### 4.4 Browser-only-Gate fuer network_provisioning

`espressif/network_provisioning` ueber SoftAP verwendet Protocomm/HTTP und ist
nicht automatisch ein normales Captive-Portal-Web-UI. Vor jeder Auswahl werden
daher drei Varianten mit gleichem R1-Ergebnisrahmen geschaetzt und, soweit
erforderlich, minimal gespiked:

1. browserbasierte Verwendung des offiziellen Stacks: exakter
   Zusatzcode, HTTP-/Protocomm-Endpunkte, Formular-/Scan-/Test-/Commitfluss,
   Browserfehler und Pflegegrenze;
2. Wiederverwendung einer nativen Captive-Portal-Komponente, insbesondere
   nur fuer DNS/OS-Probes auf einem geteilten HTTP-Server;
3. eine vereinfachte Anforderung, falls der Owner einen Espressif-Standardclient
   oder eine Espressif-App akzeptiert.

Keine grosse eigene Browser-Protocomm-Schicht wird gebaut, nur um den
offiziellen Transport formal durch den bisherigen Browservertrag zu zwingen.
Nach der vergleichbaren Aufwand-/Capability-Evidence entscheidet der Owner
explizit:

~~~
BROWSER_ONLY_REMAINS_HARD_REQUIREMENT=YES|NO
~~~

Bis zu diesem Gate bleibt jede Produktivauswahl offen. Bei `YES` ist der
Browsernachweis aus Abschnitt 3 ein hartes Auswahlkriterium. Bei `NO` muss der
Owner den zulaessigen Standardclient und die dadurch geaenderten
R1-/Security-/Recoveryvertraege explizit festlegen; App-/Cloud-/CLI-Zwang wird
nicht still als bestehender Browservertrag weitergefuehrt.

### 4.5 Identische Vergleichsmatrix

Die vier Hauptkandidaten erhalten die vollstaendige Matrix. Eine zusaetzliche
Komponente aus Abschnitt 4.3 wird nur nach dem dort beschriebenen
Vorteilsnachweis und als klar markierte Teilkomponente aufgenommen. Jede Zeile
bekommt PASS, FAIL, NOT_RUN oder BLOCKED sowie reproduzierbare Evidence.
Numerische Werte werden als Base und Kandidatenwert mit identischer Firmware-,
Last-, Zeit- und Messmethode protokolliert.

| Kriterium | Gleiches Akzeptanzkriterium fuer alle Kandidaten |
|---|---|
| Funktion | alle Schritte aus Abschnitt 3: Portalstart, SoftAP, DNS, Browser, Scan, Eingabe, Test, Commit, Abbruch, Timeout, Reconnect, Ersatz-WLAN, Abbau, direkte IP und QR |
| Browservertrag | Android, iOS/iPadOS und Windows koennen ohne Pflicht-App, Cloud oder CLI ueber Captive-Erkennung oder direkte IP arbeiten; Unterschiede werden clientweise dokumentiert |
| ESP-IDF-6.0.2 | reproduzierbarer Build und, soweit fuer den Kandidaten erforderlich, actor-free Betrieb auf der fixierten ESP-IDF-6.0.2-Produktionsbasis |
| Lizenz/Herkunft/Wartung | Quelle, exakter Version-/Commitstand, Abrufdatum, Lizenzdateien, Notices, eingebettete Assets, transitive Lizenzen, Maintaineraktivitaet und Update-/Fixpfad |
| Abhaengigkeiten | direkte und transitive Komponenten, Arduino-/Cloud-/CLI-Anteil, aktivierte Features, Buildgraph und spaetere Entfernung nicht ausgewaehlter Kandidaten |
| Flash | firmware.bin, firmware.elf, Komponenten-/Symbolvergleich gegen exakt gleiche Base |
| statisches RAM | statische Daten, BSS, eingebettete Assets und Task-/Bufferanteile |
| Heap | freier Heap, niedrigster Heap, groesster freier Block und Fragmentierungsverhalten vor, waehrend und nach Portal-/Scan-/Fehlerzyklen |
| Stack | High-Water-Mark jeder beteiligten Task, inklusive Portal-, DNS-, WiFi- und Callbackpfad; kein ungemessener Reserveclaim |
| Laufzeit | Portalstart, Scan, Antwort, Commit, Abbau, Reconnect, Regelzyklus-Jitter, Watchdog, Reset und Stabilitaet |
| Testbarkeit | Hostsimulation, deterministische Fakes, Fehler-/Cut-Point-Injektion, Browser-/Clientbeobachtbarkeit, keine Bibliothekstypen im Fachkern |
| Integrationsrisiko | Lifecyclebesitz, Callback-/Threadmodell, Fehleruebersetzung, Abbau, Zusammenspiel mit #27/#57, API-/Schemawirkung und spaeterer Wechselaufwand |
| Safety-Isolation | Regelung und Safety bleiben bei WLAN-, DNS-, HTTP-, Browser-, Speicher- und Bibliotheksfehlern aktiv und fail-closed; kein Aktorpfad wird benoetigt |

Ein Build-PASS ohne den vollstaendigen Browser-, Fehler-, Speicher- und
Clientnachweis ist keine Auswahl. Unausgefuehrte Zeilen bleiben NOT_RUN oder
BLOCKED und werden nicht als bestanden behandelt.

## 5. R1-Ergebnisvertrag, native Semantik und Owner-Gate

### 5.1 Produktziele getrennt von Implementierungsentscheidungen

Issue #89 ist der erste reale Connectivity-Konsument. Der Plan legt vor dem
Owner-Gate jedoch keine neue Connectivity-Domaene, keinen projektseitigen
Credential-Record, keine Slotnamen oder Active-/Fallback-Semantik fest. Die
folgenden Ergebnisse muessen unabhaengig vom spaeteren Persistenzbesitzer
erreicht werden:

- lokale Einrichtung ohne Cloudzwang;
- geschuetzter, geraetespezifischer Einrichtungszugang;
- die bestehende funktionierende Heim-WLAN-Konfiguration bleibt bei einem
  fehlgeschlagenen Wechsel unbemerkt unangetastet;
- kein Secret-Leak in Logs, URLs, Diagnose oder Backups;
- definierter Werksreset und definierte Recovery;
- Netzwerk bleibt unabhaengig von Regelung und Safety;
- direkte lokale Recovery- und Zugriffsmöglichkeit.

Nicht als Produktanforderung vorweggenommen werden eigener Credential-Record,
eigene Slotrotation, eigener DNS-Responder, eigener Reconnect-Automat, eigener
HTTP-/Portalserver oder eigene Credential-Persistenz. Sie duerfen erst nach
dem Owner-Gate als moeglicher Restdelta beschrieben werden, falls die
ausgewaehlte vorhandene Loesung den Ergebnisvertrag nicht vollstaendig
abdeckt.

### 5.2 Native Persistenz-, Lifecycle-, Browser-, Recovery- und Resetsemantik

Fuer jeden der vier Hauptkandidaten und jede begruendet shortlisted
Teilkomponente wird vor einer Produktivauswahl dieselbe Semantik-Inventur
angelegt:

| Semantik | Nachweisfrage | Auswahlfolge |
|---|---|---|
| Credentialbesitz und Persistenz | Wer speichert SSID/Passwort/AP-Zugang, in welchem Store/Namespace/Format, mit welcher Verschluesselungs- und Redactionaussage? | vorhandenen Besitzer wiederverwenden, wenn #57/Security/Backup nachweisbar erfuellt; sonst als Vertragskonflikt vorlegen |
| Commitidentitaet und Fehlerausgang | Gibt es eine eindeutige Version-/Commitidentitaet, Readback und einen sicheren Ausgang fuer WriteError, ReadError und CommitOutcomeUnknown? | native Semantik beweisen; keine projektspezifische Parallelpersistenz als stillen Ausweg bauen |
| Superseded-/Recoveryverhalten | Was geschieht bei Korruption, unvollstaendigem Write, Neustart, Rueckkehr aus Recovery und mehreren lesbaren Credentialstaenden? | kein automatischer Rueckfall auf die naechstaeltere Credentialversion, wenn sie superseded oder widerrufen ist; Luecke als Ownerentscheidung markieren |
| StorageEpoch, Reset und Forward-Progress | Werden alte Epochen unerreichbar, ist Factory-Reset vollstaendig und entsteht kein stiller Dummy-/Defaultzugang? | native Resetsemantik gegen #57 pruefen oder explizite Vertragsanpassung einholen |
| Start/Stop und Reconnect | Wer startet Portal, SoftAP, DNS, HTTP und Reconnect, wer beendet sie, und wie werden kurze/lange Ausfaelle unterschieden? | bestehendes Lifecyclemodell wiederverwenden, nur mit kontrollierbarer Ownership und R1-Nachweis |
| Browser und direkte IP | Ist SoftAP-Portalzugriff ohne Pflicht-App/Cloud/CLI moeglich, einschliesslich direkter IP? | Abschnitt 4.4 und Clientmatrix entscheiden lassen |
| HTTP-Server-Sharing | Werden vorhandene Handler, URI-Slots, Socket-/Taskressourcen und Abbau mit #27 kompatibel geteilt? | vorhandenen Server wiederverwenden; Parallelserver nur bei belegter Luecke nach Owner-Gate |

Die Untersuchung bewertet native Semantik als Teilkomponente oder als
Gesamtloesung. Ein Component-README, eine Reset-API oder ein vorhandener
NVS-Store ist noch kein Nachweis fuer die fachlich sichere Projektsemantik.

### 5.3 #57-, Security-, Backup- und Resetgrenze

Falls eine ausgewählte native Loesung ihren eigenen Store beziehungsweise die
ESP-WiFi-Persistenz als kanonische Wahrheit behalten soll, legt der
Entscheidungspunkt die erforderlichen Anpassungen an #57, Security, Backup,
Reset und Recovery explizit vor. Sie werden nicht still durch eine zweite
projektverwaltete Credential-Wahrheit umgangen.

In jedem Fall bleiben folgende Pruefziele hart:

- superseded Credentials werden nach Korruption oder Recovery nicht allein
  deshalb wieder aktiv, weil sie der naechstaeltere noch lesbare Stand sind;
- Widerruf, Forward-Progress und Commitidentitaet sind eindeutig und durch
  Cut-Point-/Readback-Evidence belegt;
- alte StorageEpochs bleiben unerreichbar;
- es existiert kein paralleler ESP-WiFi-/Library-NVS-Secretbestand neben einer
  projektverwalteten Credential-Wahrheit;
- Werksreset, Neustart und unklarer Writeausgang fuehren nicht zu einem
  geratenen Erfolg, versteckten Fallback oder Secret-Leak;
- Netzwerk-, Portal-, DNS- und Reconnectfehler koennen Regelung und Safety
  weder blockieren noch eine Aktorfreigabe umgehen.

Erst wenn der Owner eine projektverwaltete Connectivity-Domaene waehlt, wird
ein separater Detailplan fuer den kleinsten verbleibenden Vertrag erstellt.
Er muss dann die bestehende IStateStore-/ConfigurationMutationCoordinator-
Grenze, StorageEnvelope, StorageEpoch, Redaction, Reset und die eindeutige
Commitauswertung verwenden. Recordtyp, Schluessel, Slots, Schema und
Active-/Fallbackregeln sind bis dahin bewusst offen und werden nicht in
diesem Plan vorweggenommen.

### 5.4 Credential-Kandidat und Secretregeln als Ergebnispruefung

Der Credential-Kandidat bleibt bis zur Ownerentscheidung ein begrenztes
fluechtiges RAM-Objekt des jeweiligen Spikes. Eine Bibliothek darf es fuer
Transport und Verbindung temporaer verwenden; sie darf seine Persistenz- oder
Aktivierungsentscheidung nur dann besitzen, wenn der Owner dies nach der
Semantik-Evidence ausdruecklich akzeptiert.

Verbindlich zu pruefen:

- genau ein Home-WLAN in R1; mehrere bekannte Netze sind nicht Teil der
  Benutzeroberflaeche;
- SSID wird als WLAN-Bytefolge ohne stille Normalisierung angenommen;
  Leereingabe, nicht darstellbare beziehungsweise vom IDF-/R1-Vertrag
  ausgeschlossene Werte und Laengenueberschreitung werden abgelehnt;
- Passwortgrenzen und Authentisierungsmodi werden aus der fixierten
  ESP-IDF-/802.11-Schnittstelle und dem R1-Vertrag abgeleitet, explizit
  getestet und nicht geraten; ungueltige oder unbekannte Modi werden
  abgelehnt;
- Home-Credentials werden nur nach erfolgreichem Verbindungstest und
  ausdruecklicher Bestaetigung an den vom Owner erlaubten Commitbesitzer
  uebergeben;
- SoftAP-SSID und SoftAP-Passwort sind geraetespezifisch und ausreichend
  zufaellig; die bestehende ISecureRandomSource-Grenze wird verwendet;
  Zufallsfehler verhindern die sichere Ausgabe;
- die Lebensdauer, Rotation und Frage, ob Einrichtungs- und Ersatz-WLAN
  denselben persistenten Zugang verwenden, werden als explizite
  Ownerentscheidung mit dem Spike nachgewiesen. Ein allgemeines Default- oder
  Quellcodepasswort ist in keinem Fall zulaessig;
- Home-Passwort, SoftAP-Passwort, Salts, Token und vergleichbare Geheimnisse
  werden nie in normalen Logs, Events, Diagnose, Exporten, URLs, QR-Test-
  artefakten, Fehlermeldungen oder CI-Ausgaben wiederholt;
- sichtbare SSID/Passwort/QR-Ausgabe ist nur die bewusst lokale
  Einrichtungsanzeige des aktuellen SoftAP-Zugangs; sie wird nicht als
  Diagnose- oder Exportfeld modelliert;
- Tests verwenden synthetische Testgeheimnisse, erzeugen redigierte Logs und
  pruefen auch Fehlermeldungen, Redirects, Exception-/Callbacktexte und
  Speicher-/Artefaktdateien auf Secretleaks;
- NVS-/Flashintegritaet durch Envelope/CRC ist keine Vertraulichkeitsgarantie.
  Eine Aussage zum Schutz bei physischem Flashzugriff bleibt bis zum
  separaten EVALUATE_BEFORE_RELEASE-Gate fuer Plattformverschluesselung offen.

Webpasswort, Service-PIN, KDF, Sessions und CSRF gehoeren fachlich zu #27 und
seinen Authquellen. #89 darf dafuer keine zweite Credentialablage anlegen.

### 5.5 Validierung, Commit, Neustart und Recovery nach dem Owner-Gate

Vor dem Owner-Gate werden diese Abläufe nur als vergleichbare Evidence-
Szenarien modelliert. Kein Szenario darf produktive Connectivity-Persistenz
oder einen neuen projektspezifischen Record implementieren. Der gemeinsame
Spikestrom lautet:

~~~
Browser-/Touch-Kandidat nur im RAM
  -> typisierte Feld-, Laengen-, Modus- und Sicherheitsvalidierung
  -> native beziehungsweise bestehende Projektbasis des Kandidaten lesen
  -> Heim-WLAN testen, ohne den bisher funktionierenden Stand unbemerkt zu ersetzen
  -> erneute Validierung und ausdrueckliche Bestaetigung
  -> Commit des vom Owner zugelassenen Persistenzbesitzers
  -> exakten Commit-/Readback-/Epoch-/Resetausgang bestimmen
  -> bei Erfolg erst dann Laufzeit-WLAN anwenden oder bestaetigen
~~~

Fuer jede Variante werden WriteError, CapacityError, ReadError und
CommitOutcomeUnknown mit Cut-Points, Readback und klarer Statusklassifikation
geprueft. Ein unklarer Ausgang behauptet weder Erfolg noch Misserfolg und
loest keine weitere Credentialmutation oder Freigabe aus, bis der gewaehlte
Vertrag dies sicher aufloest.

Ein Verbindungstestfehler, Browserabbruch, Timeout oder Neustart vor dem
Commit muss die bisher funktionierende Heim-WLAN-Konfiguration erhalten. Nach
einem eindeutig neuen Commit wird geprueft, dass Neustart, Recovery,
Superseded-/Widerrufsstatus und StorageEpoch den #57-Entscheid nicht
unterlaufen. Ein alter lesbarer Stand darf nicht automatisch reaktiviert
werden, nur weil er der naechstaeltere ist.

Bei Readfehler, ungueltigem Format, falscher Epoch, unbekanntem Writeausgang
oder unvollstaendigem Reset gibt es keinen stillen Factory-Fallback und keine
Aktorwirkung. Netzwerkstart, Scan, DNS, HTTP, Portalabbruch und NTP warten
nicht blockierend aufeinander; #124 bleibt app-neutral. Bei jedem Boot bleiben
alle Aktoren AUS und Netzwerkfehler koennen ActuationInterlock-/SAFE_BOOT-
Entscheidungen nicht umgehen. Fluechtige Portal-/Browserkandidaten verschwinden
bei Neustart, sofern der ausgewaehlte native Vertrag nichts anderes explizit
und sicher festlegt.

### 5.6 Owner-Entscheidungspunkt

Nach Screen, Browser-only-Vergleich, Kandidatenspikes und identischer
Evidence entscheidet der Owner in einem dokumentierten Gate ueber:

1. `BROWSER_ONLY_REMAINS_HARD_REQUIREMENT=YES|NO`;
2. den Produktkandidaten oder die Ablehnung aller Kandidaten;
3. den zulaessigen Besitzer von Credential-Persistenz, Lifecycle, Reset und
   Recovery;
4. jede notwendige Anpassung an #57, Security, Backup, Reset oder #27;
5. erst danach den kleinsten verbleibenden projektspezifischen Delta-Vertrag.

Ohne diese Entscheidung bleiben Produktivabhaengigkeit, eigener
Connectivity-Record, Slot-/Active-/Fallback-Semantik und eigener
DNS-/Reconnect-/HTTP-/Portalpfad ausgeschlossen. Ein materieller
Vertragsunterschied erfordert eine neue Planrevision und erneute Freigabe,
bevor Code entsteht.

## 6. Architektur- und Dateischnitt nach dem Owner-Gate

Die folgenden Schnitte sind erwartete, vor der Umsetzung gegen den exakten
freigegebenen Plan-Head zu verifizieren. Eine materielle Abweichung stoppt
und benoetigt eine neue Plan-SHA.

### Vor dem Owner-Gate: nur kandidatenneutrale Evidence

Vor der Auswahl werden keine produktiven Connectivity-Modelle,
Storage-Keys/-Slots, Credential-Recordtypen, Active-/Fallbackregeln oder
projektseitigen DNS-/Reconnect-/HTTP-/Portalpfade eingefuehrt. Erlaubt sind
nur:

- ein isolierter actor-free Testvertrag mit fluechtigen, synthetischen
  Credentials und redigierter Evidence;
- vorhandene portable Status-/Storage-/Random-Ports als Testgrenzen, ohne
  neue produktive Connectivity-Persistenz;
- deterministische Fakes fuer WLAN, Scan, DNS/HTTP-Ereignisse, Zeit und
  Fehler-/Cut-Points;
- isolierte ESP-IDF-6.0.2-Builds mit jeweils exakt gelocktem Kandidaten-
  beziehungsweise Teilkomponentenbestand;
- vorhandene ESP-IDF-Dienste und ein bereits vorhandener HTTP-Server, wenn
  der jeweilige Spike deren Ownership kontrolliert nachweist.

Die bestehende `fermentation_app`-Persistenz, `IStateStore`,
`StorageEnvelope`, `StorageEpoch` und `ConfigurationMutationCoordinator` sind
Pruef- und Integrationsgrenzen. Sie werden vor dem Owner-Gate nicht um einen
Connectivity-Record erweitert. `device_platform_test_support` bleibt reine
Testhilfe und nimmt keine Produktionsabhaengigkeit auf.

### Nach dem Owner-Gate: kleinster konditionaler Integrationsdelta

Erst nach expliziter Kandidaten- und Vertragsentscheidung wird festgestellt,
ob ueberhaupt projektspezifischer Code verbleibt:

- Bei akzeptierter nativer Persistenz bleiben deren Store, Commit-, Reset- und
  Recoverypfade die eine Wahrheit; die beschlossenen #57-/Security-/Backup-
  Anpassungen werden dokumentiert und umgesetzt.
- Bei ausdruecklich projektverwalteter Persistenz wird ein neuer Detailplan
  fuer den kleinsten notwendigen Vertrag erstellt. Erst dieser Detailplan
  bestimmt Typen, Schema, Schluessel, Slots, Epoch-/Widerrufssemantik und die
  Nutzung der bestehenden Storage-/Coordinator-Grenze.
- Fuer SoftAP, DNS, HTTP, QR und Reconnect werden nur nachgewiesene Luecken
  gegen vorhandene ESP-IDF-/Espressif-/Drittkomponenten als Delta umgesetzt.
- Im Produktionsgraphen existiert danach genau der vom Owner gewaehlte
  Adapter beziehungsweise die gewaehlte Teilkomponente. Nicht ausgewaehlte
  Kandidaten werden entfernt und bilden keine zweite Wahrheit.

### Isolierter Spike

Der Spike verwendet denselben begrenzten Browservertrag fuer die vier
Hauptkandidaten und nur begruendet shortlisted Teilkomponenten. Er ist keine
allgemeine Portalplattform und keine Vorstufe eines eigenen Produktions-
vertrags. Kandidatenabhaengigkeiten werden nur dort eingebunden, mit exakter
Quelle gelockt und nach der Ownerentscheidung aus dem Produktionsgraphen
entfernt, wenn sie nicht gewaehlt werden.

### #27-Grenze

Die Onboardingseite ist eine schmale, temporare Setup-Seite. Sie ist keine
normale lokale Weboberflaeche und keine oeffentliche Schreib-API. Nach Auswahl
eines Produktionspfads wird der Transport an der gemeinsamen
ESP-IDF-/Composition-Root-Grenze mit #27 abgestimmt. #89 kopiert weder
Routing-, Auth-, Session-, CSRF-, JSON- noch Webserverwahrheit aus #27.

## 7. Ausfuehrungsphasen und Commits nach dem Reuse-/Owner-Gate

### Phase A – Reuse-/Capability-Screen und isolierte minimale Spikes

1. Baseline, Live-Issue/PR, Toolchain und Quellen auf dem freigegebenen
   Plan-Head erneut verifizieren.
2. Die drei aktuellen nativen Drittanbieter-Repositories aus Abschnitt 4.3
   sowie die offiziellen Espressif-Pfade auf aktuelle Quelle, Lizenz,
   IDF-6.0.2-Kompatibilitaet, HTTP-Sharing, NVS/Storage, Reset, Lifecycle,
   Ressourcen und Wartung screenen.
3. Nur reale Vorteile gegen die vier Hauptkandidaten als minimale
   actor-free Teilkomponenten- oder End-to-End-Spikes aufnehmen.
4. Browser-Protocomm-Aufwand, native Captive-Portal-Wiederverwendung und
   Standardclient-Alternative getrennt bewerten.

Phase A baut keine produktive Connectivity-Persistenz, keine projektspezifische
Credentialdomäne und keinen eigenen DNS-/Reconnect-/HTTP-/Portalpfad. Spikes
verwenden nur fluechtige synthetische Credentials, kontrollierte bestehende
Stores der Kandidaten und redigierte Evidence.

### Phase B – vergleichbare Evidence der aussichtsreichen Pfade

1. Fuer die vier Hauptkandidaten und nur die begruendet shortlisted
   Teilkomponenten denselben Browser-, Fehler-, Recovery-, Reset-,
   Ressourcen- und Clienttest mit identischen Zeit-/Lastgrenzen ausfuehren.
2. Native Persistenz-, Commit-, Superseded-, Epoch-, Reconnect- und
   Resetsemantik gegen die #57-/Security-/Backup-Ergebnisziele pruefen;
   Unterschiede und Vereinfachungen als Ownerentscheid vorbereiten.
3. `BROWSER_ONLY_REMAINS_HARD_REQUIREMENT` noch nicht setzen, sondern
   Aufwand und Evidence fuer `YES` und `NO` transparent gegenueberstellen.
4. Kandidaten ohne realen Vorteil im Screen belassen; keine weitere
   Bibliothek in die Hardwarematrix aufnehmen.

Ein Spike-Commit darf keinen Kandidaten in die normale Composition Root
eintragen und keinen projektspezifischen Persistenz- oder Lifecyclevertrag
festlegen.

### Phase C – Ownerentscheid ueber Anforderungen, Kandidat und Vertrag

Der Builder haelt nach der vergleichbaren Evidence an und legt vor:

- Matrix mit PASS/FAIL/NOT_RUN/BLOCKED je vier Hauptkandidaten und
  begruendeter Teilkomponenten;
- Browser-only-Aufwand, Clientmatrix, QR-/Captive-/direkte-IP- und
  Standardclientnachweis;
- native Storage-/Commit-/Recovery-/Resetsemantik sowie #57-/Security-/
  Backup-Abweichungen;
- reproduzierbare Builds, Quellen, Lizenzen, Notices, Abhaengigkeiten,
  Ressourcen, Wartung, Testbarkeit und Integrationsrisiko;
- offene Risiken und eine begruendete Empfehlung ohne automatische Auswahl.

Der Owner entscheidet explizit:

1. `BROWSER_ONLY_REMAINS_HARD_REQUIREMENT=YES|NO`;
2. den Produktkandidaten oder die Ablehnung aller Kandidaten;
3. den zulässigen Persistenz-, Lifecycle-, Recovery- und Resetbesitzer;
4. alle erforderlichen Anpassungen an #57, Security, Backup, Reset und #27.

Bis zu diesem Gate gibt es keine Produktivauswahl und keine eigene
Connectivity-Persistenz.

### Phase D – kleinster verbleibender produktiver Integrationsdelta

Erst nach Phase C wird der minimale Delta-Vertrag geplant und implementiert:

- native Persistenz und native Lifecyclesemantik werden direkt integriert,
  wenn sie als eine sichere Wahrheit akzeptiert sind;
- ein projektspezifischer Credential-/Storagevertrag entsteht nur, wenn der
  Owner ihn nach dem Semantikvergleich ausdruecklich verlangt, und wird in
  einem separaten Detailplan gegen #57 konkretisiert;
- eigener DNS-, Reconnect-, HTTP- oder Portalcode entsteht nur fuer die
  belegte Restluecke nach Reuse der vorhandenen Dienste;
- nicht ausgewaehlte Bibliotheken und Spike-Harnesses werden entfernt oder
  als Evidence markiert; keine zweite Web-/Credential-/Storagewahrheit.

Danach gelten die normalen Independent-Review-, OPEN_BLOCKERS-, Owner-
Pre-Ready- und CI-Gates. Ein materieller Vertragsunterschied erfordert vor
der Implementation eine neue Planrevision und Ownerfreigabe.

## 8. Tests und Evidence

### 8.1 Ohne zusaetzliche Verkabelung moeglich

Diese Nachweise koennen actor-free auf Host beziehungsweise mit der
vorhandenen ESP32-/USB-/UART-/FT232RL-Basis erfolgen:

- Wertebereichs-, Authmodus-, Bytekanonizitaets- und
  kandidatenbezogene Storage-/Schluesseltests;
- Hostsimulation von WLAN, Scan, Verbindung, DNS, HTTP, Timeout, Abbruch,
  Browserabbruch, Read-/Writefehler, falscher Epoch, Korruption und
  CommitOutcomeUnknown;
- exakte alte/neue/ungueltige Credentialzustandsvergleiche sowie
  native beziehungsweise bereits vorhandene StorageEpoch-/Werksreset-
  Orakel;
- Secret-Redaction- und Repository-/CI-Artefakt-Scans;
- isolierte ESP-IDF-6.0.2-Builds, Lizenz-/Noticepruefung,
  Abhaengigkeitsinventur und Base-/Kandidatenmessung;
- actor-free ESP32-Station/SoftAP/DNS/HTTP-/direkte-IP-Ablauf mit
  vorhandener Stromversorgung und UART, wenn die Hardwarebaseline
  nachweisbar ist. Sensoren, Display, Peltier, BTS7960, Luefter, MOSFET-
  Verbraucher und Summer bleiben getrennt beziehungsweise nachweislich
  inaktiv;
- synthetische QR-Encoding-/Decodingtests mit Testgeheimnissen sowie
  Escaping-, Groesse-, Rotation- und Redactiontests.

Diese Nachweise beweisen weder reale Displaylesbarkeit noch eine physische
QR-Scan-Abnahme. Das wird separat ausgewiesen.

### 8.2 Zwingende reale Clientmatrix

Auf exakt demselben actor-free ESP32-Stand und mit identischem Testfallset fuer
die vier Hauptkandidaten sowie nur begruendet shortlisted Teilkomponenten:

| Client | Pflichtnachweise |
|---|---|
| Android-Telefon | QR-Beitritt, Captive-Portal-Angebot, manuelle direkte IP, Scan/Formular, falsches Passwort, Abbruch, Reconnect, Neustart |
| iPhone beziehungsweise iPad mit iOS/iPadOS | dieselben Nachweise; OS-Captive-Ansicht und Safari/direkte IP getrennt protokollieren |
| Windows-PC | WLAN-Beitritt, Edge beziehungsweise der vereinbarte Standardbrowser, Captive-Portal-Erkennung, direkte IP, lange Eingabe, Fehler/Abbruch und Reconnect |

Für jeden Client werden Modell/OS/Browserstand, Uhrzeit,
automatische Portalentdeckung JA/NEIN, direkte-IP-Ergebnis,
Antwortzeiten, sichtbare Fehlermeldung, Credential-Commitstatus,
Verbindungsabbruch und Secret-Redaction protokolliert. Ein fehlender
Captive-Redirect wird nicht als Gesamtfail bewertet, wenn der direkte
angezeigte IP-Fallback voll funktioniert; ein fehlender direkter IP-Fallback
ist dagegen ein R1-FAIL.

### 8.3 QR- und Hardwaregrenze

Die reale Anzeige und das Scannen des QR auf dem vorgesehenen 320-x-240-
Display benoetigen die bestaetigte Displayhardware und den #31-Pfad. Bis
dahin gilt:

- Host-/synthetische QR-Evidence darf PASS sein, wenn sie vollstaendig
  redigiert und mit Testgeheimnissen ausgefuehrt ist;
- physischer Display-/Kamera-Scan bleibt BLOCKED_HARDWARE oder NOT_RUN;
- kein UART- oder Logdump mit einem produktiven Passwort ersetzt den
  Display-/QR-Nachweis;
- Android/iOS/Windows-Portal- und direkte-IP-Evidence kann mit dem
  vorhandenen actor-free ESP32-/UART-Aufbau getrennt davon erfolgen.

### 8.4 Fehler-, Recovery- und Safetytests

Mindestens:

- kein bestaetigter WLAN-Zugang, gueltiger nativer beziehungsweise
  projektseitiger Kandidatenstand, falsches Passwort, unerreichbarer AP,
  kurzer Ausfall, langer Ausfall und stabiler Reconnect;
- Portalstart vor und nach ausdruecklicher lokaler Aktion;
- Portal-/DNS-/HTTP-Abbruch, Browserabbruch, Timeout und parallele
  Start-/Stopanfragen;
- Cut vor und nach dem jeweiligen nativen oder projektseitigen Write, vor
  Readback, nach Readback, vor Runtime-Apply, waehrend Reset und nach
  Epochwechsel;
- WriteError, CapacityError, ReadError, NotFound nach begonnenem Write,
  CommitOutcomeUnknown sowie Format-/CRC-/Schema-/Length-/Epoch- und
  Revisionsfehler des jeweiligen Besitzers;
- Neustart vor Commit, nach eindeutigem unveraendertem Stand, nach eindeutigem
  neuen Commit, im Ersatz-WLAN und mit unklarem Ausgang;
- keine Wiederbelebung alter Epochcredentials, keine automatische
  Factory-Neuanlage bei korrupten/alten Bytes und keine neue Mutation bei
  indeterminiertem Zustand;
- laufende actor-free Regel-/Safety-Simulation bleibt bei allen
  Netzwerkfehlern bedienbar beziehungsweise fail-closed, ohne Aktorwirkung;
- kontrollierter Portalabbau gibt Heap, Stack, Socket, DNS und Callback-
  Ressourcen frei und hinterlaesst keine Secretkopien in Test-/Diagnosepfaden.

### 8.5 Ressourcen- und Stabilitaetsmessung

Pro Hauptkandidat und Base sowie fuer jede begruendet shortlisted
Teilkomponente, mit identischem Build und identischer Last:

- firmware.bin, firmware.elf, Flashkomponenten und statisches RAM;
- freier Heap, niedrigster Heap, groesster freier Block und Fragmentierung
  nach wiederholtem Start/Stop, Scan, Fehler und maximal gueltigem Formular;
- Task-Stack-High-Water-Mark fuer App, WiFi/Event, HTTP, DNS und
  kandidatspezifische Tasks;
- Zeit fuer SoftAP-/Portalstart, Scan, Browserantwort, Verbindungstest,
  Commit, Readback, Reconnect und Abbau;
- Regelzyklus-Jitter, Watchdog, Reset, Leak-/Handle-/Socketrest und
  Langzeitstabilitaet;
- direkte und transitive Abhaengigkeitszahl sowie projektspezifischer
  Integrationscode.

Rohwerte, Messmethode, Toolversion, Profil, Boardrevision und exakter
Source-SHA werden mit den Evidence-Artefakten abgelegt. Hostwerte ersetzen
keine realen Heap-, Jitter-, Watchdog-, WiFi- oder Flashclaims.

## 9. Dokumentations- und Abschlusswirkung

Im aktuellen Plan-PR werden nur der vollstaendige Plan und die geforderte
Roadmap-Synchronisierung geaendert. Es gibt keine Kandidatenauswahl,
keine neue Produktionsabhaengigkeit und keine Firmwareaenderung.

Nach dem spaeteren Spike beziehungsweise Auswahlentscheid sind nur die
tatsaechlich belegten Dokumente zu aktualisieren:

- NETWORK.md fuer bestaetigte Lebenszyklus-, QR-, IP-, Ersatz-WLAN- und
  Credentialregeln;
- CONFIGURATION_PERSISTENCE.md und SETTINGS_AND_STORAGE.md fuer den nach
  Owner-Gate bestaetigten #89-Persistenzbesitzer sowie dessen Epoch-, Reset-,
  Commit- und Redactionvertrag;
- THIRD_PARTY_COMPONENTS.md und die Auditdokumente fuer exakte Quelle,
  Version/Commit, Lizenz, Notices, Abhaengigkeiten, Status und Evidence;
- NETWORK_DIAGNOSTICS_INTEGRATION.md fuer die projektseitige Status-/Fehler-
  projektion, ohne Geheimnisse;
- CHANGELOG.md und ROADMAP.md fuer den nachgewiesenen Status;
- keine Aenderung an Issue #27s owning Web-/Auth-Vertrag ausser explizit
  abgestimmter gemeinsamer Adaptergrenze.

## 10. Stopregeln und Definition of Done

Sofort anhalten und Ownerentscheidung einholen bei:

- abweichender main-Basis, fehlendem Live-Issue/PR-Abgleich oder nicht
  reproduzierbarer Toolchain;
- Widerspruch zwischen #57, ADR-016, NETWORK.md, #27 oder ADR-013;
- Bedarf nach zweitem Store, zweitem Coordinator, zweiter Webserver- oder
  Credentialwahrheit;
- Bibliotheks- oder Callbackpfad mit unkontrollierbarem Auto-Commit,
  unredigiertem Secret, nicht begrenzbarer Allokation oder Safetywirkung;
- Materialabweichung an Schema, Persistenz, Security, Recovery, Architektur,
  Toolchain, Hardware oder Acceptance Criteria;
- fehlender Browsernachweis, fehlender direkter IP-Fallback oder
  App-/Cloud-/CLI-Zwang;
- Hardware-/UART-/Power-/Reset-Baseline fehlt fuer den jeweils beanspruchten
  realen Nachweis. Der Status ist dann BLOCKED oder NOT_RUN, nicht PASS.

Issue #89 ist aus Plan-/Spike-Sicht erst abgeschlossen, wenn:

- alle vier Kandidaten im identischen Vergleichsrahmen bewertet oder
  begruendet als BLOCKED/NOT_RUN ausgewiesen sind;
- bei `BROWSER_ONLY_REMAINS_HARD_REQUIREMENT=YES` der browserbasierte Vertrag
  einschliesslich QR-/direkte-IP-Fallback nachgewiesen ist; bei `NO` der
  explizit ownergenehmigte Standardclientvertrag einschliesslich direkter
  lokaler Recovery-/Zugriffsmöglichkeit nachgewiesen ist;
- Android, iOS/iPadOS und Windows getrennte Ergebnisse besitzen;
- Base-/Kandidaten-Ressourcen, Stack, Jitter, Watchdog, Lizenz,
  Abhaengigkeiten, Wartung, Testbarkeit und Integrationsrisiko dokumentiert
  sind;
- keine geheime Information in Code, Logs, URLs, Diagnosen, Exporten oder
  Evidence verbleibt;
- der Owner nach der vergleichbaren Evidence explizit Anforderungen,
  Produktivkandidat und Persistenz-/Lifecyclebesitzer waehlt oder die Auswahl
  offen beziehungsweise abgelehnt laesst;
- erst danach eine produktive Abhaengigkeit festgelegt wird. Bis zu diesem
  Gate bleibt CANDIDATE_SELECTION=OWNER_PENDING_AFTER_COMPARABLE_EVIDENCE
  und ACTUATOR_RELEASE=NO.
