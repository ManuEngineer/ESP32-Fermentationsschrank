# Issue #89 – Reuse-/Capability-Evidence

Dieser Bericht dokumentiert die autorisierte Phase-A/B-Umsetzung des
freigegebenen Plans. Er ist eine Entscheidungsgrundlage und keine
Produktivauswahl.

```text
ISSUE=89
PLAN_SHA=d8d506da1d5bde129c09d623263d7657c38f28a3
BASE=main@2c010e8a8be8e351f89b79ae6c74f665d24a1f0e
SCOPE=PHASE_A_B_ONLY
PRODUCTIVE_IMPLEMENTATION=NOT_STARTED
PRODUCTIVE_CANDIDATE=OWNER_PENDING
BROWSER_ONLY_REMAINS_HARD_REQUIREMENT=OWNER_GATE_PENDING
PRODUCTIVE_CONNECTIVITY_PERSISTENCE=NOT_STARTED
ACTUATOR_RELEASE=NO
```

## Ausfuehrungsgrenze

Die Artefakte unter `spikes/issue89_wlan_onboarding/` liegen ausserhalb des
Produktions-CMake-Graphs. Sie fuehren keinen projektspezifischen
`ConnectivityCredential`-Record, keine RecordTypeId 9, keine `cc0`-/`cc1`-
Semantik, keine Slotrotation, keinen Fallback-Selektor, keine
`StorageEpoch`-Mutation und keinen produktiven DNS-, Reconnect-, Portal- oder
Credential-Persistenzpfad ein. Die erzeugten SoftAP-Zugangsdaten sind nur
volatile Probe-Werte; ihre Schluessel werden nicht ausgegeben.

Damit ist insbesondere keine native Bibliotheks-Persistenz stillschweigend
zur zweiten Projektwahrheit geworden. Die offizielle Probe liest vorhandenen
nativen WiFi-Zustand nur, startet bei bereits provisioniertem Zustand keinen
Reset und veraendert keinen Projekt-Commit. Die native Probe persistiert
nichts.

## Reproduzierbare lokale Evidence

Die Builds wurden mit ESP-IDF 6.0.2, Commit
`7101770dc6db2667b3c477cc31365dd1acd6db4e`, fuer die vorhandene 4-MB-
ESP32-/kein-PSRAM-Baseline erzeugt. Beide Projekte verwenden actor-free
Probe-Code; `APP_REAL_ACTUATORS_ENABLED=0` und
`ACTUATOR_RELEASE=NO` bleiben unveraendert.

| Nachweis | Ergebnis | Detail |
|---|---|---|
| `python3 spikes/issue89_wlan_onboarding/host_contract_test.py` | PASS | 5/5 Tests: bytebasierte SSID-/WPA2-Validierung, volatile Commitgrenze, Erhalt der bestehenden Konfiguration, Redaction und WLAN-QR-Escaping |
| offizieller ESP-IDF-Probe-Build | PASS | `idf.py -C spikes/issue89_wlan_onboarding/official_network_provisioning -B build/issue89_official_network_provisioning build`; `network_provisioning` 1.2.4, Paket-Hash `72d27784e3daf807418a34fb00be136ec50c6db49d989ce981d22e031fc0e7f8`; 895276 Bytes, Partition frei 15 %, IRAM frei 43477, DRAM frei 145297 |
| nativer ESP-IDF-Probe-Build | PASS | `idf.py -C spikes/issue89_wlan_onboarding/native_http_adapter -B build/issue89_native_http_adapter build`; 792900 Bytes, Partition frei 24 %, IRAM frei 45593, DRAM frei 145609 |
| Secret-Scan | PASS | `python3 scripts/check_secrets.py` ueber die drei neuen Python-/C-Artefakte: keine Geheimnisse oder privaten Pfade |
| Produktionsgraph | PASS | keine neue WLAN-Komponente, Persistenz oder Laufzeitkopplung in `lib/` bzw. im Root-Produktionsbuild |
| Flash-/UART-/Reset-Lauf | NOT_RUN | kein Flash war fuer die lokale Capability-Evidence erforderlich; ein spaeterer actor-free Hardwarelauf benoetigt einen separat dokumentierten Zielaufbau |

`managed_components/`, `sdkconfig` und Build-Ausgaben sind lokale generierte
Artefakte. Der reproduzierbare offizielle Dependency-Stand ist in
`official_network_provisioning/dependencies.lock` festgehalten; die
Komponenten werden nicht als produktive Abhaengigkeit in den Root-Graphen
uebernommen.

## Vergleichbarer Reuse-Screen

Die vier Plan-Kandidaten bleiben die gemeinsame Hauptmatrix. Die drei
zusaetzlich angeforderten nativen Drittlösungen wurden gescreent, aber nicht
automatisch in die Hardwarematrix aufgenommen.

| Kandidat | Reuse-/Capability-Evidence | R1-/Vertragsluecke | Status fuer vertieften Spike |
|---|---|---|---|
| `espressif/network_provisioning` 1.2.4 | PASS fuer ESP-IDF-6.0.2-Build; native SoftAP-/Protocomm-/HTTP-Transportfunktionen | Standard-SoftAP-Schema startet Protocomm-HTTPD, liefert aber nicht automatisch ein normales Browser-Portal; native WiFi-Konfiguration wird vor erfolgreichem Verbindungstest in Flash gesetzt; Fehlerpfad stellt die bisherige funktionierende Konfiguration nicht als R1-Vertrag wieder her | JA, browser- und Persistenz-Gate offen |
| direkter `protocomm`-/ESP-IDF-SoftAP-/HTTP-/DNS-Pfad | ESP-IDF-Komponenten sind verfuegbar; die native Probe zeigt SoftAP und direkten HTTP-Zugriff ohne zusaetzliche Bibliothek | kein vollstaendiger Browser-/Captive-/DNS-/Scan-/Reconnect-/Commit-Nachweis; keine Produktsemantik vor Owner-Gate | JA, nur vergleichbar mit denselben Cut-Points |
| kleiner eigener nativer ESP-IDF-Adapter | PASS nur fuer isolierten SoftAP-/direkte-IP-HTTP-Capability-Build; kein produktiver Adapter | DNS/Captive Portal, Scan, Reconnect, Persistenz, Recovery und Commit sind absichtlich nicht implementiert; Eigenbau ist vor Reuse- und Owner-Gate keine Umsetzungsrichtung | JA, nur nach Gate und gegen Reuse-Evidence |
| WiFiManager v2.0.17 | Quellen-/Lizenzscreen; Arduino-Framework und `CMakeLists.txt`-Abhaengigkeit auf `arduino` | kein direkter nativer ESP-IDF-6.0.2-Pfad ohne Frameworkwechsel; keine gleichwertige ESP-IDF-Produktintegration belegt | `FAIL/BLOCKED_FOR_NATIVE_IDF_SPIKE`; nur bei Owner-Entscheid fuer Arduino nochmals pruefen |
| `thorrak/esp_wifi_config` v0.4.0 | liefert SoftAP, Captive Portal/DNS, Web-UI, Scan, Reconnect/Lifecycle und HTTPD-Sharing; MIT; aktueller Stand `32c78805e9fc206610b7debe31d06638cbe5da09`; Manifest ab IDF 5.4 und IDF-6-Hinweis auf `network_provisioning` | eigene NVS-/Auto-Commit-/Reconnect-/Reset-Semantik und Zusatzabhaengigkeiten muessen gegen #57, Security, Backup und Reset geprueft werden | `CONDITIONAL_YES`; nur bei realem Vorteil vertiefen |
| `tuanpmt/esp_wifi_manager` v1.1.0 | liefert SoftAP, Captive Portal/DNS, Web-UI, Scan, Multinetwork-Reconnect/Lifecycle und Reset; MIT; aktueller Stand `20f77d79e9cdde9e4d3f0c3c7a3bd3babfaf893a` | eigene NVS-/REST-/Config-Semantik, `esp_bus`-/mDNS-Abhaengigkeit, leeres Default-AP-Passwort und unredigierte Config-/REST-Risiken; kein belegter Vorteil gegenueber den Hauptkandidaten | `CONDITIONAL_NO`; kein Deep-Spike ohne neuen Vorteil |
| `nordesems/esp-captive-portal` v1.3.0 | MIT; aktueller Stand `b937ee88b86de47b40cd195f829cfd70e5af03c0`; DNS, DHCP Option 114, OS-Probes und Registrierung an bestehenden `esp_http_server` | liefert weder SoftAP, Credentialfluss, Scan, Reconnect, Storage noch Reset; Handler-Reihenfolge und Lifecycle muessen integriert geprueft werden | `CONDITIONAL_SUBCOMPONENT`; nur als kleiner DNS/OS-Probe-Teil vertiefen |

Quellen des Screens: [`network_provisioning`](https://components.espressif.com/components/espressif/network_provisioning),
[`esp_wifi_config`](https://github.com/thorrak/esp_wifi_config),
[`esp_wifi_manager`](https://github.com/tuanpmt/esp_wifi_manager),
[`esp-captive-portal`](https://github.com/nordesems/esp-captive-portal) und
[`WiFiManager`](https://github.com/tzapu/WiFiManager). Die genannten Stände
und die URL-Auswertung sind Bestandteil dieser Evidence; ein Screen ist kein
Produktfreigabenachweis.

## Browser-only-Gate

Die offizielle Probe bestaetigt den Transport-Baustein, nicht den
browserbasierten R1-Vertrag. Der Standardpfad von
`network_provisioning` startet Protocomm ueber HTTP; die offizielle
Dokumentation und das Beispiel verwenden einen spezialisierten Client wie
`esp_prov.py` beziehungsweise einen App-orientierten Provisioning-/QR-Fluss.
Eine normale HTML-Seite mit Scan, verdeckter Eingabe, Test, expliziter
Bestaetigung, direkter IP und Fehler-/Recoveryanzeige ist damit nicht
automatisch vorhanden.

Vor einer Produktentscheidung sind deshalb getrennt zu messen:

1. Zusatzaufwand und Angriffs-/Wartungsflaeche fuer einen browserbasierten
   Client des offiziellen Protocomm-Protokolls;
2. Aufwand und Nutzen einer vorhandenen nativen Captive-Portal-Komponente,
   einschliesslich DNS-/OS-Probes, HTTPD-Sharing, Handlerplaetzen und
   kontrolliertem Start/Stop;
3. die Vereinfachung, falls der Owner eine Espressif-App oder einen anderen
   Standardclient akzeptiert.

Es wird keine grosse eigene Browser-Protocomm-Schicht gebaut, bevor der
Owner entschieden hat, ob Browser-only wirklich hart bleibt:

```text
BROWSER_ONLY_REMAINS_HARD_REQUIREMENT=OWNER_GATE_PENDING
```

## Persistenz-, Commit- und Recovery-Befund

Der statische Review des offiziellen Managers ist fuer die bestehende #57-
Semantik entscheidend: `network_prov_mgr_configure_wifi_sta` setzt
`WIFI_STORAGE_FLASH` und schreibt die neue STA-Konfiguration mit
`esp_wifi_set_config`, bevor der Verbindungstest erfolgreich ist.
`NETWORK_PROV_WIFI_CRED_SUCCESS` wird erst spaeter nach dem IP-Ereignis
gemeldet. Ein Fehler nach diesem Vorab-Schreiben ist daher kein PASS fuer
das harte R1-Ergebnis, dass eine funktionierende Heim-WLAN-Konfiguration bei
fehlgeschlagenem Wechsel nicht unbemerkt zerstoert wird. Die Probe behauptet
keinen alten Credential-Fallback; ein solcher waere insbesondere kein
zulässiger Vertrag nach #57.

Vor dem Owner-Gate wird deshalb keine Connectivity-Persistenz implementiert.
Falls der Owner die native Persistenz eines ausgewaehlten Components oder des
ESP-WiFi-Stacks akzeptiert, muessen die davon betroffenen #57-, Security-,
Backup- und Resetvertraege vor der Umsetzung ausdruecklich angepasst werden.
Falls eine eigene Connectivity-Domaene verbleibt, muss ihr spaeterer
Detailvertrag Commitidentitaet, Widerruf/Forward-Progress und die
Unerreichbarkeit alter `StorageEpoch`s festlegen; ein
`hoechster-gueltiger-Record + vorherige-Revision`-Fallback ist nicht
freigegeben.

## R1-Testmatrix und Grenzen

| Nachweis | Status | Grenze / naechster Nachweis |
|---|---|---|
| Portal explizit starten und kontrolliert beenden | PARTIAL | offizieller Manager-/nativer Probe-Start belegt; Browser-UI, Stop, Timeout und Recovery fehlen |
| individuelle geschuetzte SoftAP-Zugangsdaten | PARTIAL | volatile individuelle Werte werden erzeugt und redigiert; keine reale Clientabnahme |
| WLAN-QR | PARTIAL | synthetisches Format und Escaping im Host-Oracle PASS; QR-Encoding, Anzeige und Kamera-Decoding NOT_RUN |
| direkte IP | PARTIAL | native Probe registriert eine direkte HTTP-Seite; realer Zugriff NOT_RUN |
| Captive Portal/DNS/OS-Erkennung | NOT_RUN | kein vollständiger Kandidatennachweis; zusätzlicher Portal-Screen bleibt konditional |
| Scan, Eingabe, Test, Abbruch, Timeout, Reconnect | NOT_RUN | keine Produktlogik vor Owner-Gate |
| Android | NOT_RUN | kein Client-/Hardwarelauf |
| iOS/iPadOS | NOT_RUN | kein Client-/Hardwarelauf |
| Windows | NOT_RUN | kein Client-/Hardwarelauf |
| Redaction in Logs, URLs, Diagnose, Backup | PARTIAL/PASS | Host-Oracle und statische Probeausgabe PASS; reale Bibliotheks-/Backuppfade nicht freigegeben |
| alte funktionierende Credentials bei Fehlversuch erhalten | PARTIAL | kandidatenneutrale Commitgrenze PASS; offizielle native Vorab-Flashsemantik ist Konfliktbefund, kein Produkt-PASS |
| Safety-/Regelungsunabhaengigkeit | PASS fuer Scope | keine Produktionskopplung; reale Laufzeitisolation bleibt Integrationsnachweis |
| Hardware-/UART-/Reset-Recovery | NOT_RUN/BLOCKED_HARDWARE | getrenntes Hardwarefenster mit ESP32, reproduzierbarem Boot/Reset und Clientmatrix erforderlich |

Die Nachweise ohne zusätzliche Verkabelung sind damit auf Host-Oracle,
statischen Source-/Manifest-Screen und actor-free IDF-Builds begrenzt. Reale
Android-, iOS- und Windows-Browser-, QR-, SoftAP-, Reset- und
Reconnect-Nachweise benötigen den ESP32-Aufbau, UART/Resetzugang und die
jeweiligen Clientgeraete. Sie werden nicht durch Host- oder Build-PASS
ersetzt.

## Owner-Gate vor produktiver Auswahl

Nach der vergleichbaren Evidence entscheidet der Owner ausdrücklich:

- `BROWSER_ONLY_REMAINS_HARD_REQUIREMENT=YES|NO`;
- welcher der vier Hauptpfade, gegebenenfalls mit einem begründeten
  Zusatz-Screen-Kandidaten als Teilkomponente, weiterverfolgt wird;
- ob native Component-/ESP-WiFi-Persistenz den Produktvertrag uebernehmen
  darf oder welche #57-/Security-/Backup-/Reset-Anpassung zulaessig ist;
- welche minimale Integrationsgrenze und welche realen Client-/Hardwaretests
  vor Phase D gelten.

Bis zu diesem Gate ist der Status:

```text
CANDIDATE_SELECTION=OWNER_PENDING_AFTER_COMPARABLE_EVIDENCE
PRODUCTIVE_CONNECTIVITY_PERSISTENCE=NOT_STARTED
IMPLEMENTATION=PHASE_A_B_SPIKE_EVIDENCE_ONLY
```

Erst danach darf Phase D den kleinsten verbleibenden projektspezifischen
Integrationsdelta planen. Normale Review-/Pre-Ready-Gates folgen erst nach
dieser Auswahl und der entsprechenden Plan-/Vertragsfreigabe.
