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
PLAN_STATUS=OWNER_PLAN_APPROVAL_PENDING
PLAN_SHA=EXACT_COMMIT_RECORDED_IN_PR_AND_SESSION_HANDOVER
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
fuer einen browserbasierten R1-Onboardingpfad. Dieselbe R1-Anforderung wird
gegen alle vier Kandidaten geprueft:

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

## 2. Verbindliche Quellen und wiederzuverwendende Grundlagen

Die Umsetzung liest und verwendet diese Quellen unveraendert, soweit der
Plan nicht ausdruecklich eine additive #89-Ergaenzung vorsieht:

| Verantwortung | Kanonische Quelle / Wiederverwendung |
|---|---|
| R1-WLAN-Verhalten | docs/NETWORK.md, insbesondere Ersteinrichtung, Ersatz-WLAN, QR, direkte IP, DHCP/mDNS und lokaler HTTP |
| Browser-/Webgrenzen | docs/WEB_UI.md und Issue #27; #89 liefert nur den Onboarding-Transportvertrag |
| Konfigurationspersistenz | docs/CONFIGURATION_PERSISTENCE.md, docs/SETTINGS_AND_STORAGE.md und ADR-016 |
| erster Connectivity-Konsument | Issue #57; keine dort reservierten Secret-Slots, sondern ein jetzt eigener typisierter #89-Vertrag |
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
Prototyp ist ein Evidence-Artefakt, keine zweite Produktionsanwendung.

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
| kleiner eigener nativer ESP-IDF-Adapter | zuerst alle vorhandenen ESP-IDF-/Espressif-Dienste; eigener Code nur fuer nachgewiesene Luecken wie ein begrenzter DNS-Captive-Responder oder die projektspezifische Lifecycle-Uebersetzung | kleiner nativer Browserablauf mit identischen Seiten-/Endpoint-/Fehler- und IP-Fallback-Anforderungen | eigener Parallelserver ohne Lueckennachweis, fehlender Browser-/Clientnachweis oder unvertretbare Ressourcen-/Wartungslast |
| WiFiManager v2.0.17, Commit d82d0a1b | nur nachgewiesener ESP-IDF-6.0.2-Integrationspfad; keine stillschweigende Rueckkehr zum Arduino-Produktionspfad | Standard-/angepasster Portalablauf muss ebenfalls individuelle SoftAP-Credentials, QR, DNS, direkte IP, expliziten Start und Projekt-Commit im Browser zeigen | kein direkter ESP-IDF-6.0.2-Build/Betrieb beziehungsweise kein dokumentierter Integrationsweg ohne Arduino-Produktionspfad; App-/Cloud-/CLI-Zwang; unkontrollierbare Bibliotheksdefaults |

Der WiFiManager-Test ist konditional, aber nicht vorab abgewertet. Das
Evaluationsgate ist inhaltlich identisch; die Espressif-first-Reihenfolge
bestimmt die Pruefprioritaet, nicht das Ergebnis.

### 4.2 Komponenten vor eigenem Code

Vor einer eigenen Implementierung wird je Funktion in dieser Reihenfolge
geprueft und dokumentiert:

1. eingebaute ESP-IDF-6.0.2-Dienste: esp_wifi, esp_netif, esp_event,
   esp_http_server, LWIP-DNS-/Socketpfad, esp_timer und
   esp_fill_random beziehungsweise die verifizierte Zufallsquelle;
2. offizielle Espressif-Komponenten und Repositories, insbesondere
   network_provisioning und protocomm;
3. geeignete gepflegte Drittkomponente mit nachvollziehbarer Lizenz;
4. erst danach der kleinstmoegliche eigene Adapter fuer die belegte Luecke.

Der QR-Code wird als gesonderte, begrenzte Presentation-/Codecentscheidung
behandelt. Die bereits registrierten QR-Kandidaten bleiben bis Scan- und
Ressourcennachweis nicht ausgewaehlt. Der WLAN-Kandidat darf die QR-Wahl
nicht erzwingen.

Jeder Prototyp dokumentiert verwendete, deaktivierte und transitive
Komponenten. Eine Bibliothek, die intern WLAN-Daten, NVS, Serverzustand oder
Sessions speichert, wird entweder so konfiguriert, dass nur der projekt-
eigene Vertrag aktiv ist, oder als FAIL beziehungsweise Architekturabweichung
bewertet. Es gibt keine Parallelkomponente nur fuer den Fall eines spaeteren
Wechsels.

### 4.3 Identische Vergleichsmatrix

Jede Zeile bekommt PASS, FAIL, NOT_RUN oder BLOCKED sowie reproduzierbare
Evidence. Numerische Werte werden als Base und Kandidatenwert mit identischer
Firmware-, Last-, Zeit- und Messmethode protokolliert.

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

## 5. Eigener #89-Credential- und Persistenzvertrag

### 5.1 Eine neue, aber einzige Connectivity-Wahrheit

Issue #89 ist der erste reale Connectivity-Konsument. Nach Planfreigabe wird
deshalb ein eigener, typisierter ConnectivityCredential-Vertrag definiert,
ohne WLANfelder in UserConfiguration, ServiceConfiguration, ProgramCatalog,
ActiveConfigurationManifest oder dem spaeteren Webserververtrag zu erfinden.

Planbindung fuer die konkrete Recordausarbeitung:

- neuer Recordtyp nach dem bestehenden 1–8-Raum, vorlaeufig RecordTypeId 9;
  die Verfuegbarkeit wird vor dem ersten Codecommit erneut gegen die aktuelle
  Storage-SSOT geprueft;
- zwei kurze, ausschliesslich fuer diesen Record reservierte Slots,
  vorlaeufig cc0 und cc1;
- Envelope-Version, CRC, StorageEpoch und VersionValue werden unveraendert
  aus dem bestehenden Wire-/Portvertrag wiederverwendet;
- Schema 1 enthaelt genau eine begrenzte Connectivity-Konfiguration mit
  optional genau einem Home-WLAN und genau einem individuellen
  Provisioning-Zugang; keine freien Key/Value- oder Reservefelder;
- SSID, Authentisierungsmodus und Passwort werden als begrenzte, typisierte
  Werte mit expliziten Laengen, Wire-IDs und kanonischen Bytes gespeichert;
  der ESP-IDF-wifi_config-Typ gelangt nicht in das Fach- oder Speicherformat;
- die SoftAP-IP ist Laufzeitstatus und kein geheimes persistentes
  Credentialfeld;
- Recordrevision, StorageEpoch und ein ggf. erforderlicher eigener
  Credential-/Widerrufzaehler bleiben starke, getrennte Typen; 0 und
  Ueberlauf sind ungueltig;
- der aktive Record ist der vollstaendig validierte hoechste gueltige
  Record der aktuellen StorageEpoch; der vorherige gueltige Record bleibt
  als Rueckfall beobachtbar;
- es gibt keinen persistenten Pending-Zweig, keinen Aktivierungsintent und
  keine Bibliotheks- oder NVS-Nebenwahrheit.

Die vorlaeufigen IDs sind ein planinterner, kandidatenneutraler Vorschlag fuer
die fehlende #57-Fortschreibung, keine Auswahl eines WLAN-Kandidaten. Ein
Widerspruch bei Recordtyp, Keyraum, Payloadbudget oder Architektur stoppt die
Umsetzung vor Code und erfordert Planrevision und erneute Ownerfreigabe.

### 5.2 Credential-Kandidat und Secretregeln

Der Credential-Kandidat ist ein projektverwaltetes, begrenztes RAM-DTO. Eine
Bibliothek darf dieses DTO fuer Transport und Verbindung temporaer verwenden,
aber nicht seine Persistenz- oder Aktivierungsentscheidung besitzen.

Verbindlich zu pruefen und zu implementieren:

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
  ausdruecklicher Bestaetigung vorgeschlagen;
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

### 5.3 Validierung, Test und atomarer Commit

Der verbindliche Ablauf fuer einen neuen Home-Credential-Kandidaten ist:

~~~
Browser-/Touch-Entwurf im RAM
  -> typisierte Feld-, Laengen-, Modus- und Sicherheitsvalidierung
  -> gegenwaertige StorageEpoch und aktuelle Connectivity-Basis lesen
  -> Home-WLAN testen, ohne den kanonischen Record zu ersetzen
  -> erneute Validierung und ausdrueckliche Bestaetigung
  -> bestehende ConfigurationMutationCoordinator-Lease erwerben
  -> Kandidat und unveraenderten SoftAP-/Connectivity-Anteil vollstaendig bilden
  -> Record auf dem inaktiven Slot schreiben
  -> exakte Bytes, Envelope, CRC, Revision und StorageEpoch lesen und validieren
  -> neuen Record als kanonisch aktiv publizieren
  -> erst danach Laufzeit-WLAN anwenden beziehungsweise bestaetigen
  -> alte gueltige Revision als Rueckfall behalten
~~~

Alle Plattform- und Allokationsarbeiten enden vor dem Persistenz-Commit. Ein
write mit WriteError oder CapacityError behauptet keine neue Konfiguration.
CommitOutcomeUnknown wird ausschliesslich durch den vollstaendigen
Record-/Slot-/Epoch-Readback auf exakt alt oder exakt neu aufgeloest. Bleibt
der Ausgang unklar, gilt ConfigurationRecordOutcomeIndeterminate beziehungsweise
der bestehende typisierte ConfigurationCommitIndeterminate-Pfad; es gibt
keinen geratenen Erfolg, keine weitere Mutation und keine Credentialfreigabe.

Ein Verbindungstestfehler, Browserabbruch, Timeout oder Neustart vor dem
Commit verwirft den Kandidaten und erhaelt den alten funktionierenden Record.
Eine bereits eindeutig committed neue Revision bleibt nach Neustart der
kanonische Kandidat; der alte Record bleibt als Rueckfall erhalten. Ein
Laufzeitfehler nach dem Commit darf weder einen geheimen Ersatzwert erfinden
noch automatisch auf eine andere Credentialversion umschreiben. Die
Recoveryentscheidung wird lokal, typisiert und fail-closed getroffen.

### 5.4 #57-, Reset- und Neustartgrenze

Der #89-Record wird in die bestehende Storage-/Recoveryinventur aufgenommen:

- ConfigurationRecoveryService beziehungsweise sein bestehender
  Factory-empty-Scan beruecksichtigt cc0/cc1. Vorhandene, unlesbare,
  beschaedigte, unbekannte oder fremde Epochendaten sind nie fabrikneu;
- der Record verwendet dieselbe logische IStateStore-Instanz und dieselbe
  MutationCoordinator-Instanz wie die vorhandenen Konfigurationsmutationen;
  kein zweiter Store, Mutex, Lease- oder allgemeiner Transaktionsdienst;
- der #57-Factory- und Bootpfad aktiviert Connectivity erst nach
  vollstaendigem eigenem Recordscan. Fehlt ein gueltiger Connectivity-Record
  bei ansonsten vertrauenswuerdigem Konfigurationsboot, wird der
  Onboardingzustand explizit dargestellt;
- ein Werksreset macht Records alter StorageEpochs logisch unerreichbar,
  erzeugt keinen leeren Dummy-Secretrecord und reaktiviert nie alte
  Credentials. Nach dem Reset darf ein neuer individueller SoftAP-Zugang
  nur ueber den neuen, erfolgreich validierten #89-Pfad entstehen;
- jeder Cut vor, waehrend und nach einem Connectivity-Write, Readback,
  Epochwechsel, Reset und Laufzeitanwendungs-Handoff bekommt ein eigenes
  Recoveryorakel;
- bei Readfehler, ungueltigem Schema, falscher Epoch, unbekanntem
  Writeausgang oder unvollstaendigem Reset gibt es keine normale
  Connectivityfreigabe, keinen stillen Factory-Fallback und keine
  Aktorwirkung;
- Netzwerkstart, Scan, DNS, HTTP, Portalabbruch und NTP warten nicht
  blockierend aufeinander. #124 bleibt app-neutral; WLAN ist Komfort- und
  Connectivityfunktion und keine Bedingung fuer Regelung, Safety oder
  trusted UTC;
- bei jedem Boot bleiben alle Aktoren AUS. Die Firmware setzt keinen alten
  elektrischen Aktorzustand wieder ein. Netzwerkfehler duerfen die
  ActuationInterlock-/SAFE_BOOT-Entscheidung nicht umgehen;
- fluechtige Portal-/Browser-/Sessionkandidaten verschwinden bei Neustart.
  Nur der vollstaendig validierte, persistierte ConnectivityRecord ist
  wiederverwendbar.

## 6. Architektur- und Dateischnitt nach Planfreigabe

Die folgenden Schnitte sind erwartete, vor der Umsetzung gegen den exakten
freigegebenen Plan-Head zu verifizieren. Eine materielle Abweichung stoppt
und benoetigt eine neue Plan-SHA.

### Kandidatenneutraler Fach- und Persistenzkern

Voraussichtlich:

- fermentation_app: typisierte ConnectivityCredential-/ProvisioningAccess-
  Modelle, Validierung, Redaction, Recordcodec, Slotstore,
  Recoveryklassifikation und schmale Orchestrierung;
- Erweiterung von configuration_storage_contract.hpp um den bestaetigten
  neuen Recordtyp und die kurzen Slots;
- vorhandene ConfigurationRecoveryService-/Factory-empty-/Resetpfade nur so
  erweitern, dass der #57-Vertrag die neue bekannte Domaene korrekt
  klassifiziert; keine zweite Recoverymaschine;
- vorhandene ConfigurationMutationCoordinator-, IStateStore-,
  StorageEnvelope-, CRC-, StorageEpoch- und Readbackpfade wiederverwenden;
- neue portable Ports nur, wenn der vorhandene INetworkStatus-Port die
  nachgewiesene Luecke nicht abdeckt. Ein Port bleibt typisiert,
  anwendungsneutral und frei von ESP-IDF-/HTTP-/DNS-/Bibliothekstypen;
- device_platform_test_support: nur erforderliche Fakes fuer Zufall, WLAN,
  Uhr, DNS/HTTP-Ereignisse und Storage-Cut-Points; keine Produktionsabhaengigkeit.

### Konkreter ESP-IDF-Adapter

In device_platform_esp_idf beziehungsweise der Composition Root:

- ein einziger ausgewaehlter Adapter fuer Station, SoftAP, Events, DNS,
  HTTP-Portal und kontrollierten Abbau;
- Wiederverwendung von esp_wifi, esp_netif, esp_event, esp_http_server,
  LWIP-/Socket- und Zufallsdiensten vor eigenem Code;
- genau ein Kandidatenadapter im Produktpfad. Nicht ausgewaehlte
  Kandidaten bleiben im isolierten Spike und gelangen nicht in den
  Produktionsbuild;
- protocomm-, network_provisioning- oder WiFiManager-Typen enden an dieser
  Grenze. Der Fachkern bekommt nur stabile Projekttypen und Fehler;
- SoftAP-IP und Netzwerkstatus werden als Laufzeitstatus an die lokale
  Anzeige-/spatere Webprojektion gegeben. Keine Persistenzkopie der
  Netzwerkstackobjekte.

### Isolierter Spike

Der Spike verwendet denselben begrenzten Browservertrag fuer vier
Compile-/Laufvarianten, aber keine allgemeine Portalplattform. Er darf eine
private actor-free Harness- beziehungsweise Testkonfiguration und
synthetische Testdaten besitzen. Kandidatenabhaengigkeiten werden nur dort
eingebunden, mit exakter Quelle gelockt und nach der Entscheidung aus dem
Produktionsgraphen entfernt, wenn sie nicht gewaehlt werden.

### #27-Grenze

Die Onboardingseite ist eine schmale, temporare Setup-Seite. Sie ist keine
normale lokale Weboberflaeche und keine oeffentliche Schreib-API. Nach Auswahl
eines Produktionspfads wird der Transport an der gemeinsamen
ESP-IDF-/Composition-Root-Grenze mit #27 abgestimmt. #89 kopiert weder
Routing-, Auth-, Session-, CSRF-, JSON- noch Webserverwahrheit aus #27.

## 7. Ausfuehrungsphasen und Commits nach Ownerfreigabe

### Phase A – kandidatenneutrale Vertragsanpassung

1. Baseline erneut auf exakt freigegebenem Plan-Head und aktuellem main
   verifizieren.
2. Storage-SSOT auf Recordtyp-/Key-/Payloadkonflikte pruefen und den
   ConnectivityRecord mit exakter Schema-/Revision-/Epoch-/Redaction- und
   Resetsemantik festlegen.
3. Native Validator-, Codec-, Slot-, Readback-, Redaction- und
   #57-Recoverytests zuerst erweitern.
4. Nur die bestehende Konfigurationskomposition um den einen neuen
   Connectivitykonsumenten ergaenzen.

Moegliche kleine Commits:

- kandidatenneutraler Connectivity-/Credential-Vertrag und Codec;
- Slot-/Readback-/Recoveryintegration mit #57 und bestehendem Coordinator;
- native Negativ-, Cut-Point- und Secret-Redactiontests.

### Phase B – gemeinsamer actor-free Browser-Spike

1. Gemeinsamen Testablauf, synthetische Credentials, redigierte Evidence und
   identische Zeit-/Lastgrenzen festlegen.
2. Kandidat 1 bis 3 in identischen ESP-IDF-6.0.2-Profilen pruefen.
3. WiFiManager nur ueber das explizite konditionale
   ESP-IDF-6.0.2-Evaluationsgate hinzunehmen.
4. Je Variante denselben Browservertrag, dieselben Storage-Fakes und
   dieselben Fehler-/Abbruchpfade ausfuehren.

Jeder Kandidat bleibt bis zum Matrixabschluss nicht produktiv. Ein Spike-
Commit darf keinen Kandidat in die normale Composition Root eintragen.

### Phase C – reale, actor-free Client- und Ressourcen-Evidence

Nach bestaetigter Hardwarebaseline werden dieselben Varianten auf demselben
ESP32-Aufbau und mit denselben Konfigurationen betrieben. Es folgen die
Clientmatrix, die QR-/Captive-/direkte-IP-Pruefung, Ersatz-WLAN,
Neustart-/Cut-Points und Ressourcenmessungen aus Abschnitt 8.

### Phase D – harter Owner-Entscheidungspunkt

Der Builder haelt nach der vollstaendigen vergleichbaren Evidence an und legt
vor:

- Matrix mit PASS/FAIL/NOT_RUN/BLOCKED je Kandidat;
- reproduzierbare Builds, Quellen, Lizenzen, Notices und Abhaengigkeiten;
- Client-/Browser-/QR-/IP-/Ersatz-WLAN-Protokolle;
- Storage-/Redaction-/Recovery-/Cut-Point-Orakel;
- Base-/Kandidatenvergleich fuer Flash, statisches RAM, Heap, Fragmentierung,
  Stack, Jitter, Watchdog und Integrationscode;
- offene Risiken und eine begruendete Empfehlung ohne automatische Auswahl.

Der Owner waehlt danach explizit einen R1-Pfad, verwirft alle Kandidaten oder
fordert einen begrenzten Nachspike. Erst die explizite Auswahl darf eine
Produktivabhaengigkeit beziehungsweise einen produktiven Adapter festlegen.
Eine Abweichung bei Architektur, Persistenz, Security, Browservertrag,
Toolchain oder Teststrategie erfordert vorher eine neue Planrevision und
Ownerfreigabe.

### Phase E – nur nach Auswahl

Erst nach dem Auswahlentscheid werden der eine produktive Adapter,
die endgueltigen Komponenteneintraege, die dauerhafte Netzwerkdokumentation
und die #27-Anbindung umgesetzt. Nicht ausgewaehlte Bibliotheken,
Prototypabhaengigkeiten und temporare Harnesspfade werden entfernt oder
explizit als historische Evidence markiert. Der PR bleibt bis zum
Independent Review Draft; der vollstaendige Self-Check und Pre-Ready-Lauf
folgen ausschliesslich nach den geltenden Owner-Gates.

## 8. Tests und Evidence

### 8.1 Ohne zusaetzliche Verkabelung moeglich

Diese Nachweise koennen actor-free auf Host beziehungsweise mit der
vorhandenen ESP32-/USB-/UART-/FT232RL-Basis erfolgen:

- Schema-, Wertebereichs-, Authmodus-, Bytekanonizitaets- und
  StorageKeytests;
- Hostsimulation von WLAN, Scan, Verbindung, DNS, HTTP, Timeout, Abbruch,
  Browserabbruch, Read-/Writefehler, falscher Epoch, Korruption und
  CommitOutcomeUnknown;
- exakte alte/neue/ungueltige Recordvergleichstests sowie
  StorageEpoch-/Werksreset-Orakel;
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

Auf exakt demselben actor-free ESP32-Stand und mit identischem Testfallset:

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

- kein Connectivity-Record, gueltiger Record, falsches Passwort,
  unerreichbarer AP, kurzer Ausfall, langer Ausfall und stabiler Reconnect;
- Portalstart vor und nach ausdruecklicher lokaler Aktion;
- Portal-/DNS-/HTTP-Abbruch, Browserabbruch, Timeout und parallele
  Start-/Stopanfragen;
- Cut vor jedem Recordwrite, nach Write, vor Readback, nach Readback,
  vor Runtime-Apply, waehrend Reset und nach Epochwechsel;
- WriteError, CapacityError, ReadError, NotFound nach begonnenem Write,
  CommitOutcomeUnknown, CRC-/Schema-/Length-/Epoch-/Revisionfehler;
- Neustart vor Commit, nach eindeutigem alten Commit, nach eindeutigem
  neuen Commit, im Ersatz-WLAN und mit unklarem Ausgang;
- keine Wiederbelebung alter Epochcredentials, keine automatische
  Factory-Neuanlage bei korrupten/alten Bytes und keine neue Mutation bei
  indeterminiertem Zustand;
- laufende actor-free Regel-/Safety-Simulation bleibt bei allen
  Netzwerkfehlern bedienbar beziehungsweise fail-closed, ohne Aktorwirkung;
- kontrollierter Portalabbau gibt Heap, Stack, Socket, DNS und Callback-
  Ressourcen frei und hinterlaesst keine Secretkopien in Test-/Diagnosepfaden.

### 8.5 Ressourcen- und Stabilitaetsmessung

Pro Kandidat und Base, mit identischem Build und identischer Last:

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
- CONFIGURATION_PERSISTENCE.md und SETTINGS_AND_STORAGE.md fuer den
  konkreten #89-Record, Epoch-, Reset-, Commit- und Redactionvertrag;
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
- der browserbasierte Vertrag, QR-/direkte-IP-Fallback, Credential-,
  Redaction-, Commit-, Restart- und Recoverypfad nachgewiesen sind;
- Android, iOS/iPadOS und Windows getrennte Ergebnisse besitzen;
- Base-/Kandidaten-Ressourcen, Stack, Jitter, Watchdog, Lizenz,
  Abhaengigkeiten, Wartung, Testbarkeit und Integrationsrisiko dokumentiert
  sind;
- keine geheime Information in Code, Logs, URLs, Diagnosen, Exporten oder
  Evidence verbleibt;
- der Owner nach der vergleichbaren Evidence explizit einen produktiven
  R1-Pfad waehlt oder die Auswahl offen beziehungsweise abgelehnt laesst;
- erst danach eine produktive Abhaengigkeit festgelegt wird. Bis zu diesem
  Gate bleibt CANDIDATE_SELECTION=OWNER_PENDING_AFTER_COMPARABLE_EVIDENCE
  und ACTUATOR_RELEASE=NO.
