# Planrevision – Issue #89: WLAN-Onboarding und Provisionierung auf ESP-IDF 6.1

## Planstatus, Revision und harte Basis

Dies ist die vollstaendige, eigenstaendig ausfuehrbare Planrevision fuer den
proportionalisierten Abschluss der Phase-B-Kandidatenevaluation auf dem
gemergten ESP-IDF-6.1-Stand. Sie ist ein Plan-only-Artefakt. In diesem
Revisionsschnitt werden keine weiteren Evidence-Laeufe, Builds, Clienttests,
Hardwaretests oder produktiven WLAN-/Connectivity-Aenderungen ausgefuehrt.
Nach Freigabe der exakten neuen Plan-SHA ist nur noch das Owner-
Kandidatengate offen.

```text
ISSUE=89
PR=158
BASE_BRANCH=main
BASE_SHA=7029df3997bb92e60379eb218f1894f86c5f7d55
EXPECTED_BASE_SHA=7029df3997bb92e60379eb218f1894f86c5f7d55
PR_PRE_SYNC_HEAD=2f64a1c3d09a086ac77bf4a5f4f5369524b4a1dd
PR_SYNC_MODE=NORMAL_MERGE_NO_REBASE_NO_FORCE_PUSH
PR_SYNC_MERGE_SHA=f6ffe1617a733d699d45b880df3ce9fb6ed9a5d6
ESP_IDF_TAG=v6.1
ESP_IDF_COMMIT=fff9895c82d744c7237be8847347bdd1b07c6643
HISTORICAL_APPROVED_PLAN_SHA=d8d506da1d5bde129c09d623263d7657c38f28a3
PARENT_APPROVED_PLAN_SHA=5c582aa179cd6e382dc4442a6824bb79a1e2b22f
OWNER_DECISION_BASE_HEAD=42a495d7139d9810086d5be77f81b2fccf3fc949
CURRENT_HEAD=807c59f90c95277cb34719c608a8c76b41def322
CURRENT_APPROVED_PLAN_SHA=6a0a83b3b037c47b89caa87f9d2cb1e65f498c59
PLAN_REVISION=ESP_IDF_6_1_PHASE_B_PROPORTIONAL_OWNER_GATE
PLAN_STATUS=OWNER_APPROVAL_REQUIRED
PLAN_SHA=EXACT_COMMIT_RECORDED_IN_PR_AND_SESSION_HANDOVER
IMPLEMENTATION=PHASE_B_EVIDENCE_COMPLETE_FOR_OWNER_GATE
EVIDENCE_EXECUTION=NO_FURTHER_RUN_REQUIRED_BEFORE_OWNER_GATE
HISTORICAL_6_0_2_EVIDENCE=RETAIN_AS_HISTORICAL_ONLY
PHASE_A_6_1_REVALIDATION=PASS
PHASE_B_TEST_SETUP=OWNER_AUTHORIZED
PHASE_B_FLASH_BOOT_EVIDENCE=PASS
ANDROID_CLIENT_EVIDENCE=PASS
IOS_CLIENT_EVIDENCE=WAIVED_BY_OWNER
WINDOWS_CLIENT_EVIDENCE=WAIVED_BY_OWNER
ANDROID_DIRECT_IP_TEST=PASS
ANDROID_CAPTIVE_PORTAL_AUTO_OPEN=NOT_OBSERVED
PHYSICAL_DISPLAY_QR_TEST=DEFERRED_NOT_BLOCKING_ISSUE89_SELECTION
PHASE_B_COMPARABLE_CLIENT_EVIDENCE=PASS_MINIMAL_PROPORTIONAL
OWNER_CANDIDATE_SELECTION_GATE=READY_FOR_OWNER_DECISION
CANDIDATE_SELECTION=OWNER_DECISION_PENDING
PRODUCTIVE_CONNECTIVITY_PERSISTENCE=NOT_STARTED
ACTUATOR_RELEASE=NO
```

### Ownerentscheid fuer den Phase-B-Testaufbau

Der Owner hat fuer den realen Issue-#89-Lauf auf dem Stand
`OWNER_DECISION_BASE_HEAD=42a495d7139d9810086d5be77f81b2fccf3fc949` den
vorhandenen ESP32-WROOM-32E-Dev-Aufbau ausdruecklich als entbehrlichen
Development-Testtraeger freigegeben. Diese Freigabe gilt nur fuer den
kontrollierten, isolierten Issue-#89-Spike und ist keine produktive
Persistenz-, Recovery- oder Loeschfreigabe.

```text
DEV_BOARD_IS_DISPOSABLE_TEST_TARGET=YES
FLASH_OVERWRITE_ALLOWED=YES
NVS_ERASE_ALLOWED_FOR_ISSUE89_TESTS=YES
EXISTING_DEV_STATE_PRESERVATION_REQUIRED=NO
PRE_TEST_FLASH_BACKUP_REQUIRED=NO
POWER_CUT_TESTS_REQUIRED=NO
POWER_CUT_TESTS=WAIVED_BY_OWNER
UART_RESET_TESTS_ALLOWED=YES
FLASH_TESTS_ALLOWED=YES
REAL_CLIENT_TESTS_ALLOWED=YES
```

Daraus folgt fuer Phase B: Ein zusaetzliches Test-NVS, eine separate physische
Testpartition und ein Backup des bisherigen Development-Stands sind nicht
erforderlich, solange ausschliesslich dieser freigegebene Dev-Testtraeger
verwendet wird. Die kontrollierte Testprozedur darf dessen Flash und Default-
NVS ueberschreiben oder loeschen. Automatisches oder unbeabsichtigtes Loeschen
bleibt in Produktionscode, Bibliotheksvertraegen und Recoveryvertraegen
unzulässig.

Power-Cut ist kein verpflichtendes Phase-B-Acceptance-Criterion und kein
offener Testpunkt. `Reset != Power-Cut` bleibt als Begriffsgrenze dokumentiert.
Die Owner-Entscheide erklaeren iOS/iPadOS, Windows und den physischen
Display-/Kamera-QR-Test fuer die Kandidatenauswahl als Waiver beziehungsweise
Deferred. Die vorhandene Android-Evidence reicht fuer das proportionale
Kandidatengate. Vollstaendige produktive Commit-/Recovery-Cutpoints,
Reconnect-Stress-, Langzeit- und Clientlast-Ressourcenqualifikation folgen
erst nach der Integration des gewaehlten Pfads.

Der alte Plan `d8d506da...` bleibt als historische Planrevision im
Git-Verlauf nachvollziehbar. Diese aktuelle Datei ersetzt ihn als alleinige
Ausfuehrungsbasis; ein spaeterer Builder muss keine zwei Planrevisionen
zusammensetzen.

### Verifizierter Live- und Repositorystand

- Issue #89 ist offen und traegt weiterhin den ergebnisoffenen Scope fuer
  browserbasiertes SoftAP-/Captive-Portal-Onboarding, vier Kandidaten und die
  getrennte Produktivitaetsentscheidung.
- Issue #159 ist live geschlossen. PR #160 ist gemergt; `main` fixiert ESP-IDF
  `v6.1` auf dem oben genannten Commit. PR #161 und PR #162 sind bereits in
  `main` enthalten. Die Roadmap weist Issue #159 als `CLOSED_COMPLETED` aus.
- PR #158 ist offen und Draft. Der Branch wurde ohne History-Umschreibung per
  normalem Merge auf `main` synchronisiert. Der Merge enthaelt die aktuelle
  `main`-Roadmap als Basis; die alte PR-Roadmap wurde nicht ueber `main`
  zurueckgeschrieben. Der Issue-#89-Status bleibt als aktuelle Arbeit
  erhalten.
- Die Produktionsfirmware ist nach Issue #159 auf ESP-IDF 6.1 umgestellt.
  `docs/CI_AND_QUALITY_GATES.md` und `docs/ESP_IDF_UPGRADE_CONTRACT.md` sind
  deshalb fuer neue ESP-IDF-nahe Evidence verbindlich.
- Die bestehende Issue-#89-Evidence stammt aus ESP-IDF 6.0.2. Sie wird nicht
  nachtraeglich zu 6.1-Evidence umetikettiert.

## 1. Ziel und Nicht-Ziele

### Ziel

Issue #89 liefert eine proportionale, nachvollziehbare
Entscheidungsgrundlage fuer den R1-Onboardingpfad. Die drei ausgefuehrten
Kandidaten werden anhand der vorhandenen Linux-/Android-Evidence und der
ausdruecklichen Owner-Waiver vergleichbar fuer das Owner-Kandidatengate
bewertet, ohne eine Auswahl vorwegzunehmen:

1. `espressif/network_provisioning` 1.2.4 auf Basis von `protocomm`;
2. direkter `protocomm`-/ESP-IDF-SoftAP-/HTTP-/DNS-Pfad ohne
   `network_provisioning`;
3. kleiner eigener nativer ESP-IDF-SoftAP-/DNS-/HTTP-Adapter;
4. WiFiManager v2.0.17 als konditionaler zusaetzlicher
   Drittanbieter-Kandidat.

Die Revision bestaetigt die technische Evidence-Basis auf ESP-IDF 6.1 und
trennt dabei:

- historisch gueltige 6.0.2-Evidence;
- fachliche und hostseitige Evidence, die IDF-unabhaengig wiederverwendet
  werden kann;
- ESP-IDF-, Build-, Ressourcen-, Komponenten- und Kandidaten-Evidence, die
  unter 6.1 gezielt neu auszufuehren ist;
- fuer die Auswahl bewusst deferred oder waived Nachweise;
- produktive Integrationsqualifikation, die erst nach der Owner-Auswahl
  erforderlich wird.

Der gemeinsame R1-Produktvertrag bleibt unveraendert: lokale Einrichtung ohne
Cloudzwang, individueller geschuetzter Einrichtungszugang, QR- und direkte-IP-
Moeglichkeit, WLAN-Scan und Eingabe, Test vor bestaetigtem Commit,
Secret-Redaction, definierte Recovery sowie vollstaendige Unabhaengigkeit von
Regelung und Safety. Die Kandidatenevaluation darf fehlende Capabilities als
`CAPABILITY_NOT_PRESENT` beziehungsweise Produkt-/Integrationsluecke
ausweisen; sie baut sie nicht nach.

### Nicht-Ziele dieser Planrevision

- keine Kandidatenauswahl und kein implizites Shortlisting; das
  `READY_FOR_OWNER_DECISION`-Gate bleibt eine Ownerentscheidung;
- keine produktive Connectivity-Domaene, kein Credential-Record, keine
  Slotrotation, keine `StorageEpoch`-Mutation und keine produktive WLAN-
  Persistenz;
- kein produktiver Webserver-, DNS-, QR-, Reconnect- oder Portalpfad;
- keine Wiederholung der vorhandenen Android-Tests und keine weiteren
  Client-/Hardware-/QR-/Recovery-/Runtime-Tests vor der Auswahl;
- keine Pflicht zu iOS/iPadOS, Windows, physischem Display-QR, Power-Cut,
  vollstaendiger Fehler-/Commit-/Recoverymatrix, Reconnect-Stress,
  Langzeit-Leak-/Handle-, Watchdog-, Jitter- oder vollstaendiger
  Clientlast-Ressourcenqualifikation vor der Auswahl;
- keine Aenderung an `fermentation_app`, Safety, Aktorfreigabe, GPIO-,
  Display-, Sensor- oder Verdrahtungs-SSOT;
- keine Ersetzung der Issue-#159-Evidence durch Issue-#89-Evidence und keine
  Wiederverwendung von Produktions-Hardware-Smokes als WLAN-Clientnachweis;
- kein vollstaendiger Pre-Ready-Lauf vor Independent Review, `OPEN_BLOCKERS=0`
  und ausdruecklicher Owner-Anordnung;
- keine produktive Integration nach Phase C. Ein danach erforderlicher
  Implementierungsdelta bekommt einen eigenen Detailplan beziehungsweise eine
  neue Planrevision und ein neues Owner-Gate.

## 2. Verbindliche Quellen und Grenzen

Die Ausfuehrung verwendet bestehende Vertraege und Modelle. Neue parallele
Vertraege oder Framework-Abstraktionen sind unzulaessig.

| Thema | Verbindliche Quelle / Grenze |
|---|---|
| Aktueller Status | `docs/ROADMAP.md`, Issue #89, PR #158 und der neueste SESSION-HANDOVER |
| R1-WLAN-Verhalten | `docs/NETWORK.md`: Einrichtungs-WLAN, QR, Captive-Portal, direkte IP, Ersatz-WLAN und kein Netzwerkzwang fuer Regelung/Safety |
| Release-Scope | `docs/SPECIFICATION_REVIEW.md` |
| Persistenz und Recovery | `docs/CONFIGURATION_PERSISTENCE.md`, `docs/SETTINGS_AND_STORAGE.md`, `docs/RECOVERY_AND_INTERRUPTION.md`, `docs/SYSTEM_SAFETY_AND_RECOVERY.md` |
| Erstkonsument und Commitgrenze | Issue #57 sowie vorhandene `ConfigurationMutationCoordinator`-/`IStateStore`-/`StorageEpoch`-Vertraege |
| Architektur | ADR-013, `docs/DECISIONS.md`, `docs/ARCHITECTURE.md` und lokale `AGENTS.md` |
| Adopt before build | `docs/ENGINEERING_PRINCIPLES.md`, `docs/ADOPT_OR_BUILD.md`, `docs/THIRD_PARTY_COMPONENTS.md` |
| Toolchain | `docs/ESP_IDF_UPGRADE_CONTRACT.md`, `docs/CI_AND_QUALITY_GATES.md`, ESP-IDF `v6.1` am exakten Commit |
| Bestehende Evidence | `docs/audits/ISSUE_89_WLAN_ONBOARDING_EVIDENCE.md`, `spikes/issue89_wlan_onboarding/README.md` und der historische Plan-Commit |
| Isolierte Spikes | `spikes/issue89_wlan_onboarding/`, ausserhalb des produktiven CMake-Graphs |
| Hardwaregrenze | `docs/HARDWARE.md`, `docs/OPEN_POINTS.md`, keine Behauptung ohne Board-, UART- und Reset-Nachweis; Power-Cut ist Owner-waived |

Der native Fachkern kennt weiterhin keine ESP-IDF-, WLAN-, HTTP-, DNS-, QR-
oder Kandidaten-Typen. Kandidatencode bleibt im isolierten Spike, bis der
Owner nach der Evidence ausdruecklich einen neuen Integrationsscope freigibt.

## 3. Geltende R1-Vergleichsanforderung und proportionale Gate-Grenze

Der vollstaendige R1-Produktvertrag bleibt fuer die spaetere produktive
Integration verbindlich. Die aktuelle Kandidatenevaluation nutzt jedoch die
vom Owner bestaetigte minimale Gate-Grenze: vorhandene Android-Evidence,
direkte-IP-Erreichbarkeit, dokumentierte Browser-/Capability-Befunde sowie
die ausdruecklichen Waiver fuer iOS/iPadOS, Windows, Display-QR und Power-Cut.
Der Spike ist Evidence und keine zweite Produktionsanwendung.

### 3.1 Portal- und Browserablauf

Der Vergleich prueft mindestens:

1. expliziter Start eines geschuetzten, individuellen SoftAP;
2. sichtbare SSID, Passwort, Portaladresse beziehungsweise AP-IP und erneute
   QR-Anzeige;
3. WLAN-QR im gaengigen Format mit korrekt escapten Werten;
4. Captive-Portal-Erkennung, soweit der jeweilige Client sie anbietet;
5. manueller direkter IP-Aufruf als verbindlicher Fallback;
6. Scan und Anzeige erreichbarer Heim-WLANs;
7. SSID-Auswahl beziehungsweise manuelle Eingabe und verdeckte
   Passworteingabe;
8. Verbindungstest ohne unbestaetigte Zerstoerung einer funktionierenden
   alten Konfiguration;
9. ausdrueckliche Bestaetigung vor dem Commit;
10. Erfolg, Fehler, Abbruch, Timeout, Neustart und Recovery ohne Secret-Leak.

Ein fehlender automatischer Captive-Redirect ist als
`ANDROID_CAPTIVE_PORTAL_AUTO_OPEN=NOT_OBSERVED` zu protokollieren und kein
Transportfehler, wenn der direkte-IP-Fallback funktioniert. Ein fehlender
direkter-IP-Fallback waere fuer die Produktintegration ein R1-Gap; in der
aktuellen Evidence ist die Android-Direkt-IP PASS. Ob der Browservertrag
weiterhin eine harte Pflicht fuer die Produktintegration bleibt, entscheidet
ausschliesslich der Owner.

### 3.3 Owner-akzeptierte minimale Phase-B-Gate-Matrix

Vor dem Owner-Kandidatengate gelten ausschliesslich folgende Statuswerte als
erforderlich:

```text
PHASE_A_6_1_REVALIDATION=PASS
PHASE_B_FLASH_BOOT_EVIDENCE=PASS
ANDROID_CLIENT_EVIDENCE=PASS
ANDROID_DIRECT_IP_TEST=PASS
ANDROID_CAPTIVE_PORTAL_AUTO_OPEN=NOT_OBSERVED
OFFICIAL_NETWORK_PROVISIONING_SOFTAP=PASS
OFFICIAL_BROWSER_R1_CONTRACT=GAP
DIRECT_PROTOCOMM_SOFTAP=PASS
DIRECT_PROTOCOMM_BROWSER_R1_CONTRACT=GAP
NATIVE_HTTP_SOFTAP=PASS
NATIVE_HTTP_BROWSER_TRANSPORT=PASS
IOS_CLIENT_EVIDENCE=WAIVED_BY_OWNER
WINDOWS_CLIENT_EVIDENCE=WAIVED_BY_OWNER
PHYSICAL_DISPLAY_QR_TEST=DEFERRED_NOT_BLOCKING_ISSUE89_SELECTION
POWER_CUT_TESTS=WAIVED_BY_OWNER
PHASE_B_COMPARABLE_CLIENT_EVIDENCE=PASS_MINIMAL_PROPORTIONAL
OWNER_CANDIDATE_SELECTION_GATE=READY_FOR_OWNER_DECISION
```

Nicht vorhandene DNS-, Captive-, Scan-, Formular-, Credential-Apply-, Commit-,
Reconnect- oder Recovery-Capabilities werden als
`CAPABILITY_NOT_PRESENT` beziehungsweise dokumentierte Produkt-/
Integrationsluecke gefuehrt. Sie sind kein Anlass, den Spike vor der Auswahl
zu erweitern. Die vollstaendige WLAN-/Web-/Reconnect-/Safety-
Ressourcenqualifikation erfolgt erst nach produktiver Integration des
gewaehlten Pfads im Gesamtsystem.

### 3.2 Lebenszyklus, Recovery und Safety

- Ein kurzer Heim-WLAN-Ausfall startet kein Onboarding und mutiert keine
  Credentials.
- Ein spaeterer Ersatz-WLAN-Lebenszyklus bleibt von Ersteinrichtung und
  Kandidatenscreen getrennt; Warte- und Uebergangszeiten bleiben
  `TBD_COMMISSIONING`.
- Read-, Write-, Readback-, Reset- und Commit-Unsicherheit wird fail-closed
  klassifiziert. `NotFound` wird nicht mit einem beschaedigten oder unklaren
  Store verwechselt.
- Ein unbekannter Commitausgang wird weder als alter noch als neuer gueltiger
  Zustand geraten. Es gibt keine stille Factory-Neuanlage und keine
  Reaktivierung einer alten Epoch.
- Netzwerk, Portal und Client blockieren weder Regelung noch Safety. Bei Boot,
  Reset, Fehler, unbekanntem Zustand oder unbestaetigter Hardware bleiben
  Aktoren gesperrt.

## 4. Evidence-Konversion: was bleibt gueltig, was muss neu laufen

Die nachfolgende Matrix ist verbindlich. `HISTORICAL` bedeutet: behalten und
zitierbar, aber nicht als aktueller 6.1-PASS ausgeben. `REUSE` bedeutet: die
Evidence beantwortet weiterhin dieselbe IDF-unabhaengige Frage und wird nach
Source-/Artefaktintegritaet weiterverwendet. `RERUN_6_1` bedeutet: ein neuer
Nachweis mit ESP-IDF 6.1 ist erforderlich. `PENDING` bleibt offen, bis der
separate Lauf erfolgt.

| Bestehender Nachweis | Einordnung in dieser Revision | Begruendung / Aktion |
|---|---|---|
| R1-Anforderungen aus Issue #89 und `docs/NETWORK.md` | `REUSE` | Fachvertrag ist nicht von der IDF-Minorversion abhaengig; unveraendert als Vergleichsorakel verwenden |
| Vier-Kandidaten-Gate und keine Vorabentscheidung | `REUSE` | Owner-/Governancegrenze; kein technischer Buildnachweis |
| `host_contract_test.py`, historisch 5/5 | `REUSE_WITH_INTEGRITY_CHECK` | Kandidatenneutraler Python-Oracle fuer Bytes, volatile Commitgrenze, Redaction und QR-Escaping; IDF-unabhaengig. Nicht als ESP-IDF-6.1-Buildnachweis ausgeben |
| Secret-/Diff-/Produktionsgraph-Screen der alten PR | `HISTORICAL`, gezielte aktuelle Pruefung nach Freigabe | Der alte Lauf bleibt historisch; aktuelle Branch-/Graphintegritaet wird nach dem Sync erneut geprueft, ohne daraus Kandidatenauswahl abzuleiten |
| Offizieller Probe: 894912 B, 15 % Partitionsreserve | `HISTORICAL_6_0_2` | Nur Baseline. Unter 6.1 neu bauen und mit neuer Source-SHA, Lockfile und Ressourcenwerten dokumentieren |
| Direkter Protocomm-Probe: 825712 B, 21 % Reserve | `HISTORICAL_6_0_2` | Nur Baseline. Unter 6.1 neu bauen; Handlergrenze bleibt zu pruefen |
| Nativer HTTP-Probe: 792900 B, 24 % Reserve | `HISTORICAL_6_0_2` | Nur Baseline. Unter 6.1 neu bauen; direkte-IP-Capability bleibt zu pruefen |
| Offizieller Component-Hash und `idf 6.0.2`-Lockfile | `HISTORICAL_6_0_2` | Nicht in die aktuelle Evidence kopieren; Lockfile fuer 6.1 mit Component Manager neu erzeugen |
| `network_provisioning` 1.2.4 als Version | `REUSE_AS_CANDIDATE_VERSION`, `RERUN_6_1` fuer Kompatibilitaet | Version ist weiterhin der Kandidat, aber Manifest-/IDF-Grenze und transitive Abhaengigkeiten muessen unter 6.1 verifiziert werden |
| `protocomm`, ESP-IDF HTTPD/WiFi/Netif im direkten Probe | `RERUN_6_1` | Built-in APIs, Header und Linkgraph muessen gegen den fixierten 6.1-Checkout gebaut werden |
| WiFiManager- und weitere Drittanbieter-Screens | `HISTORICAL_CANDIDATE_SCREEN`, `RERUN_6_1` vor vertieftem Gate | Commit, Lizenz, Manifest, IDF-/Arduino-Pfad, Storage- und Lifecycle-Risiken erneut aus exakten Quellen pruefen; kein automatisches Shortlisting |
| Issue-159-Produktionsbuilds, Ressourcen, esp-clang und Hardware-Smokes | `REUSE_AS_ISSUE159_PROVENANCE_ONLY` | Belegt ESP-IDF 6.1 fuer die Produktionsbasis, aber keinen Issue-89-Kandidaten-, Browser- oder Clientnachweis |
| Android, Browser, UART-/Reset-Recovery | `OWNER_ACCEPTED_MINIMAL_EVIDENCE` | Android-Beitritt und direkte IP PASS; Captive-Auto-Open nicht beobachtet; Official/Direct Root 404 sind Browser-/Capability-Gaps, Native HTTP liefert die Transportseite; vorhandene Reset-/Reconnect-Evidence bleibt gueltig |
| iOS/iPadOS und Windows | `WAIVED_BY_OWNER` | Vor dem Owner-Kandidatengate nicht mehr verpflichtend; kein Test wird simuliert |
| physischer Display-/Kamera-QR | `DEFERRED_NOT_BLOCKING_ISSUE89_SELECTION` | Synthetischer QR-Encoding-/Escaping-Nachweis bleibt gueltig; realer Displaytest folgt erst mit angeschlossener Displayhardware |
| vollstaendige Fehler-, Commit-, Recovery-, Stress- und Clientlast-Ressourcenmatrix | `DEFERRED_AFTER_CANDIDATE_SELECTION` | Keine weitere Qualifikation vor der Auswahl; fehlende Spike-Capabilities bleiben dokumentierte Produkt-/Integrationsluecken |
| Konservativer Phase-B-Stop vor dem Ownerentscheid | `HISTORICAL_PRE_OWNER_DECISION` | Der damalige Stop wegen Test-NVS- und Power-Cut-Absicherung bleibt als historische Evidence erhalten; er ist nach der ausdruecklichen Ownerfreigabe keine aktuelle Voraussetzung mehr |

Die historischen Werte werden in der neuen Evidence als `ESP_IDF=6.0.2`,
`SOURCE_SHA=2f64a1c...` beziehungsweise der damals dokumentierten Source-/Tool-
Provenienz zitiert. Der neue 6.1-Lauf erhaelt eigene Buildverzeichnisse,
Provenienzzeilen und Tabellen. Alte Logs oder Buildausgaben werden nicht
ueberschrieben.

## 5. Kandidaten- und Komponentenstrategie

### 5.1 Gleichrangige Hauptkandidaten

| Kandidat | Was unter 6.1 zu verifizieren ist | Vorabstatus |
|---|---|---|
| `espressif/network_provisioning` 1.2.4 | Component-Manager-Aufloesung, `protocomm`-/HTTP-Transport, SoftAP-Start, Browsergrenze und Ressourcen | `SOFTAP=PASS`; `BROWSER_R1_CONTRACT=GAP`; kein Browser-PASS behauptet; Android-Evidence fuer das Gate akzeptiert |
| direkter `protocomm`-Pfad | oeffentliche 6.1-APIs, Security-/Version-/Endpointgrenzen, RAM-only-SoftAP, direkte IP und fehlende Credentialsemantik | `SOFTAP=PASS`; `BROWSER_R1_CONTRACT=GAP`; fehlende Set/Test/Commit-Capability bleibt `CAPABILITY_NOT_PRESENT` |
| nativer ESP-IDF-Adapter | ESP-IDF-6.1-SoftAP/HTTP-Pfad, direkte IP, Lifecycle und bewusst fehlende DNS-/Committeile | `SOFTAP=PASS`; `BROWSER_TRANSPORT=PASS`; fehlende DNS-/Scan-/Commit-Capability bleibt `CAPABILITY_NOT_PRESENT` |
| WiFiManager v2.0.17 | nur falls ein nachvollziehbarer nativer 6.1-Pfad ohne ungeplanten Arduino-Produktionswechsel besteht; Manifest, Lizenz, Storage, Webserver und Ressourcen | konditional, weder bevorzugt noch verworfen |

Die bereits gescreenten Drittanbieter `thorrak/esp_wifi_config`,
`tuanpmt/esp_wifi_manager` und `nordesems/esp-captive-portal` bleiben
historische Screen-Evidence. Sie werden nur dann in die vertiefte 6.1-Matrix
aufgenommen, wenn ein dokumentierter, realer Vorteil gegen die vier
Hauptkandidaten besteht und der Owner diesen Zusatzscope bestaetigt. Ein
Screening ist keine Auswahl und kein produktiver Dependency-Entscheid.

### 5.2 Aktueller Lockfile-Befund und geplante Korrektur

`spikes/issue89_wlan_onboarding/official_network_provisioning/` wurde fuer die
vorhandene Phase-B-Evidence bereits auf der exakten ESP-IDF-6.1-Basis gebaut
und dokumentiert. Dieser Planrevisionsschnitt erzeugt keinen weiteren Build
und aendert kein Lockfile. Die bereits gesicherte Component-/Lockfile-
Provenienz bleibt Bestandteil der Evidence:

1. Eine spaetere erneute Build-/Lockfile-Pruefung ist nur bei einem neuen
   Integrationsscope oder einer materiellen Toolchainaenderung erforderlich;
2. die aktuelle Kandidatenevaluation wird nicht durch einen weiteren Lauf
   veraendert;
3. keine Lockfile- oder Component-Aenderung wird in den Produktionsgraphen
   uebernommen.

Die vorhandenen 6.1-Builds und das dokumentierte `HTTP_404` am Root sind
Evidence fuer diesen Kandidaten und kein Anlass fuer eine
Kompatibilitaets-Wrapper-Architektur. Ein normaler Browser-GET/POST wird nicht
als `esp_prov`-Client umgedeutet.

## 6. Abschluss nach der Planfreigabe

Vor dem Owner-Kandidatengate sind keine weiteren Builds, Flash-/Client-/QR-
oder Hardwaretests vorgesehen. Nach Freigabe der exakten neuen Plan-SHA sind
nur noch diese Schritte zulaessig:

1. PR #158, Issue #89, Roadmap, Evidence und den genau einen aktuellen
   SESSION-HANDOVER gegen die freigegebene Plan-SHA verifizieren;
2. die drei dokumentierten Kandidatenbefunde und die Owner-Waiver als
   `PASS_MINIMAL_PROPORTIONAL`-Entscheidungsgrundlage vorlegen;
3. am `OWNER_CANDIDATE_SELECTION_GATE=READY_FOR_OWNER_DECISION` anhalten;
4. nach der Owner-Auswahl einen eigenen Integrationsplan fuer den kleinsten
   verbleibenden produktiven Scope erstellen.

Es wird in diesem Abschluss weder ein Kandidat ausgewaehlt noch produktive
Connectivity-Persistenz, Web-/DNS-/QR-Logik, Recovery oder Aktorfreigabe
implementiert. Die vollstaendige WLAN-/Web-/Reconnect-/Safety-
Ressourcenqualifikation und der reale Display-/Kamera-QR-Test gehoeren in die
spaetere Integrations- beziehungsweise Hardwarephase.

### Phase B – abgeschlossene minimale Client-Evidence

Die vorhandene Phase-B-Evidence ist fuer das Owner-Kandidatengate
ausreichend. Es wurden keine Android-Tests wiederholt. Die drei ausgefuehrten
Kandidaten sind mit demselben autorisierten Testtraeger, ESP-IDF-6.1-
Provenienz und Android-/Linux-Host-Nachweis dokumentiert:

| Bereich | Akzeptierter Status fuer dieses Gate |
|---|---|
| Android | `ANDROID_CLIENT_EVIDENCE=PASS`; WLAN-Beitritt und direkte IP PASS |
| Android Captive-Angebot | `ANDROID_CAPTIVE_PORTAL_AUTO_OPEN=NOT_OBSERVED`; kein Transportfehler |
| Official `network_provisioning` | `SOFTAP=PASS`; `BROWSER_R1_CONTRACT=GAP`; Root-404 ist ein Browser-/Capability-Befund |
| Direkter Protocomm-Pfad | `SOFTAP=PASS`; `BROWSER_R1_CONTRACT=GAP`; Root-404 ist ein Browser-/Capability-Befund |
| Nativer HTTP-Pfad | `SOFTAP=PASS`; `BROWSER_TRANSPORT=PASS`; Root-HTTP-200 mit Setup-Seite |
| iOS/iPadOS | `IOS_CLIENT_EVIDENCE=WAIVED_BY_OWNER` |
| Windows | `WINDOWS_CLIENT_EVIDENCE=WAIVED_BY_OWNER` |
| Physischer Display-/Kamera-QR | `PHYSICAL_DISPLAY_QR_TEST=DEFERRED_NOT_BLOCKING_ISSUE89_SELECTION`; synthetischer Encoding-/Escaping-Nachweis bleibt gueltig |
| Power-Cut | `POWER_CUT_TESTS=WAIVED_BY_OWNER`; `Reset != Power-Cut` |
| Start-/Reset-Ressourcen | vorhandene Werte als Fruehindikator ausreichend; Vollqualifikation nach Integration |

Die drei Kandidatenbefunde werden nicht durch Nachimplementierung fehlender
Funktionen vereinheitlicht. Fehlende DNS-, Captive-, Scan-, Formular-,
Credential-Apply-, Commit-, Reconnect- oder Recovery-Funktionen sind
`CAPABILITY_NOT_PRESENT` beziehungsweise dokumentierte Produkt-/
Integrationsluecken. Ein normaler Browser-POST ersetzt keinen gueltigen
`esp_prov`-/Protocomm-Client.

Vor dem Owner-Kandidatengate sind iOS/iPadOS, Windows, physischer Display-QR,
Power-Cut, die vollstaendige falsche-Passwort-/Abbruch-/Timeout-Matrix,
produktive Commit-/Recovery-Cutpoints, Reconnect-Stress, Handles-/Leak-
Langzeitnachweise, Watchdog unter Clientlast, Jitter sowie die vollstaendige
Heap-/Stack-Matrix nicht mehr verpflichtend. Diese Punkte werden nicht als
`BLOCKED` oder `FAILED` gefuehrt und nicht simuliert. Die vollstaendige
WLAN-/Web-/Reconnect-/Safety-Ressourcenqualifikation erfolgt erst nach
produktiver Integration des gewaehlten Pfads.

### Phase C – Owner-Kandidatengate

Nach der proportionalen Evidence haelt der Builder an und legt ausschliesslich
eine Entscheidungsgrundlage vor. Der Owner entscheidet explizit:

1. `BROWSER_ONLY_REMAINS_HARD_REQUIREMENT=YES|NO`;
2. Kandidat oder Ablehnung aller Kandidaten;
3. Besitzer von Credential-Persistenz, Lifecycle, Reset und Recovery;
4. notwendige Anpassungen an #57, Security, Backup, Reset und #27;
5. minimaler weiterer Integrationsscope.

Die Gate-Voraussetzungen sind erreicht; bis zur tatsaechlichen Auswahl bleiben:

```text
PHASE_A_6_1_REVALIDATION=PASS
PHASE_B_FLASH_BOOT_EVIDENCE=PASS
ANDROID_CLIENT_EVIDENCE=PASS
IOS_CLIENT_EVIDENCE=WAIVED_BY_OWNER
WINDOWS_CLIENT_EVIDENCE=WAIVED_BY_OWNER
PHYSICAL_DISPLAY_QR_TEST=DEFERRED_NOT_BLOCKING_ISSUE89_SELECTION
POWER_CUT_TESTS=WAIVED_BY_OWNER
PHASE_B_COMPARABLE_CLIENT_EVIDENCE=PASS_MINIMAL_PROPORTIONAL
OWNER_CANDIDATE_SELECTION_GATE=READY_FOR_OWNER_DECISION
CANDIDATE_SELECTION=OWNER_DECISION_PENDING
PRODUCTIVE_CONNECTIVITY_PERSISTENCE=NOT_STARTED
ACTUATOR_RELEASE=NO
```

Ein materieller Unterschied bei Schema, Persistenz, Wireformat, Security,
Recovery, Architektur, Bibliothek, Hardware oder Acceptance Criteria stoppt
die Ausfuehrung und erfordert eine neue Planrevision vor Code.

### Phase D – ausdruecklich nicht Teil dieses Freigabepakets

Die produktive Integration wird in dieser Planrevision weder ausgefuehrt noch
implementiert. Erst nach Phase C kann ein eigener Detailplan den kleinsten
verbleibenden Delta-Vertrag bestimmen. Dieser muss explizit festlegen, ob
native Bibliotheks-/ESP-WiFi-Persistenz als eine Wahrheit uebernommen wird
oder ob #57 einen neuen, ownergenehmigten Vertrag benoetigt. Ein zweiter
Credentialstore, zweiter Webserver oder stiller Auto-Commit ist nicht
zulaessig.

## 7. Ergebnis- und Evidence-Status

Die Evidence-Datei `docs/audits/ISSUE_89_WLAN_ONBOARDING_EVIDENCE.md` fuehrt
historische und aktuelle Tabellen getrennt und enthaelt mindestens:

- `HISTORICAL_ESP_IDF=6.0.2` mit alter Source-/Toolchainprovenienz;
- `CURRENT_ESP_IDF=6.1` mit exakter IDF-SHA und aktuellem Source-Head;
- Host-Oracle als kandidatenneutrale Evidence;
- drei getrennte 6.1-Probe-Builds mit Komponenten-/Lockfile-/Ressourcen-
  Evidence;
- drei ausgefuehrte Kandidaten mit gleicher Bewertungslogik; der konditionale
  WiFiManager-Screen bleibt separat und wird nicht in die Auswahl simuliert;
- `PHASE_B_COMPARABLE_CLIENT_EVIDENCE=PASS_MINIMAL_PROPORTIONAL` auf Basis der
  vorhandenen Android-Evidence und der Owner-Waiver;
- `PHASE_B_TEST_SETUP=OWNER_AUTHORIZED` und
  `POWER_CUT_TESTS=WAIVED_BY_OWNER` nach dem aktuellen Ownerentscheid;
- `IOS_CLIENT_EVIDENCE=WAIVED_BY_OWNER`,
  `WINDOWS_CLIENT_EVIDENCE=WAIVED_BY_OWNER` und
  `PHYSICAL_DISPLAY_QR_TEST=DEFERRED_NOT_BLOCKING_ISSUE89_SELECTION`;
- dokumentierte `CAPABILITY_NOT_PRESENT`-Befunde fuer nicht implementierte
  DNS-, Captive-, Scan-, Formular-, Apply-, Commit- und Recovery-Funktionen;
- keine produktive Kandidaten- oder Persistenzentscheidung.

Es gelten die Begriffe aus `docs/CI_AND_QUALITY_GATES.md`:

- `PASS`: ausgefuehrt und erfolgreich;
- `FAILED`: ausgefuehrt, fehlgeschlagen und blockierend;
- `BLOCKED`: konkrete Voraussetzung fehlt;
- `NOT_RUN`: nicht ausgefuehrt;
- `SKIPPED` und fehlende Angaben sind nie `PASS`.

Ein Phase-A-Build-PASS ist weder Clientakzeptanz noch Hardwareakzeptanz. Fuer
das aktuelle minimale Owner-Gate ist jedoch die reale Android-/Linux-
Transport-Evidence zusammen mit den Owner-Waivern massgeblich. Die spaetere
produktive Integration darf daraus keine vollstaendige Browser-, QR-,
Recovery- oder Ressourcenqualifikation ableiten.

## 8. Commits, Dokumentation und Stopregeln

### 8.1 Aktueller Plan-Commit

Dieser Plan-Commit darf nur Planinhalt und die erforderliche aktuelle
Status-/Evidence-/Roadmap-Synchronisierung enthalten. Es gibt in diesem
Schnitt:

- keine neue Produktionsabhaengigkeit;
- keine Code-, Build- oder Lockfile-Aenderung fuer einen Evidence-Lauf;
- keine neuen Tests oder neue technische Evidence; die Statuswerte
  `PASS_MINIMAL_PROPORTIONAL`, `WAIVED_BY_OWNER` und `DEFERRED...` spiegeln
  ausschliesslich bereits vorhandene Evidence und den aktuellen Ownerentscheid
  wider;
- keine Kandidatenauswahl und keine Connectivity-Persistenz.

Nach dem Commit werden exakte Plan-SHA, aktualisierter PR-HEAD und offene
Entscheidungen im PR-Body und im genau einen aktuellen SESSION-HANDOVER
ausgewiesen. Danach haelt der Builder an und wartet auf die Ownerfreigabe der
exakten Plan-SHA.

### 8.2 Stopregeln

Sofort anhalten und `BLOCKED` oder einen Ownerentscheid einholen bei:

- abweichender `main`-, PR-, Plan- oder ESP-IDF-SHA;
- fehlendem Live-Issue-/PR-/Roadmap-Abgleich;
- fehlender oder nicht sauberer 6.1-Toolchain;
- Component-Manager-/Lockfile-Aufloesung, die nicht reproduzierbar ist;
- zweiter Credential-, Storage- oder Webserverwahrheit;
- automatischem NVS-Erase, unredigiertem Secret oder unkontrolliertem
  Auto-Commit;
- fehlendem direktem-IP-Fallback, App-/Cloud-/CLI-Zwang oder nicht
  vergleichbarer Kandidaten-Evidence;
- fehlendem Board-, UART- oder Reset-Nachweis fuer einen beanspruchten realen
  Lauf; der Owner-Waiver fuer Power-Cut ist kein offener Testpunkt;
- materieller Abweichung von #57, ADR-013, ADR-016, `NETWORK.md`,
  `SYSTEM_SAFETY_AND_RECOVERY.md` oder diesem Plan.

Issue #89 ist aus Evaluationssicht bereit fuer die Ownerentscheidung, sobald
die exakte neue Plan-SHA freigegeben ist. Bis zur Auswahl bleiben
`CANDIDATE_SELECTION=OWNER_DECISION_PENDING`,
`PRODUCTIVE_CONNECTIVITY_PERSISTENCE=NOT_STARTED` und
`ACTUATOR_RELEASE=NO` unveraendert.
