# Planrevision – Issue #89: WLAN-Onboarding und Provisionierung auf ESP-IDF 6.1

## Planstatus, Revision und harte Basis

Dies ist die vollstaendige, eigenstaendig ausfuehrbare Planrevision fuer die
Fortsetzung von Issue #89 nach dem gemergten ESP-IDF-Upgrade. Sie ist ein
Plan-only-Artefakt. Vor ihrer ausdruecklichen Freigabe der exakten Commit-SHA
werden keine Evidence-Laeufe, Builds, Clienttests, Hardwaretests,
Kandidatenentscheidungen oder produktiven WLAN-/Connectivity-Aenderungen
ausgefuehrt.

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
PLAN_REVISION=ESP_IDF_6_1_PHASE_B_OWNER_TEST_BOUNDARY
PLAN_STATUS=OWNER_APPROVAL_REQUIRED
PLAN_SHA=EXACT_COMMIT_RECORDED_IN_PR_AND_SESSION_HANDOVER
IMPLEMENTATION=PHASE_A_EVIDENCE_COMPLETE
EVIDENCE_EXECUTION=NOT_AUTHORIZED_BEFORE_PLAN_APPROVAL
HISTORICAL_6_0_2_EVIDENCE=RETAIN_AS_HISTORICAL_ONLY
PHASE_A_6_1_REVALIDATION=PASS
PHASE_B_TEST_SETUP=OWNER_AUTHORIZED
PHASE_B_COMPARABLE_CLIENT_EVIDENCE=PENDING
OWNER_CANDIDATE_SELECTION_GATE=NOT_READY
CANDIDATE_SELECTION=OWNER_PENDING_AFTER_COMPARABLE_EVIDENCE
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
UART-, Reset-, Neustart-, Credential-, Commit- und Recoverytests bleiben
verpflichtend.

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

Issue #89 liefert eine vergleichbare, reproduzierbare Entscheidungsgrundlage
fuer den R1-Onboardingpfad. Dieselbe fachliche Anforderung wird gegen vier
Kandidaten bewertet, ohne eine Vorabentscheidung:

1. `espressif/network_provisioning` 1.2.4 auf Basis von `protocomm`;
2. direkter `protocomm`-/ESP-IDF-SoftAP-/HTTP-/DNS-Pfad ohne
   `network_provisioning`;
3. kleiner eigener nativer ESP-IDF-SoftAP-/DNS-/HTTP-Adapter;
4. WiFiManager v2.0.17 als konditionaler zusaetzlicher
   Drittanbieter-Kandidat.

Die Revision aktualisiert die technische Evidence-Basis auf ESP-IDF 6.1 und
trennt dabei:

- historisch gueltige 6.0.2-Evidence;
- fachliche und hostseitige Evidence, die IDF-unabhaengig wiederverwendet
  werden kann;
- ESP-IDF-, Build-, Ressourcen-, Komponenten- und Kandidaten-Evidence, die
  unter 6.1 gezielt neu auszufuehren ist;
- noch nicht erbrachte Phase-B-Client-, Browser-, QR-, Recovery- und
  Laufzeitnachweise.

Der gemeinsame R1-Vertrag bleibt: lokale Einrichtung ohne Cloudzwang,
individueller geschuetzter Einrichtungszugang, QR- und direkter-IP-Fallback,
WLAN-Scan und Eingabe, Test vor bestaetigtem Commit, Secret-Redaction,
definierte Recovery sowie vollstaendige Unabhaengigkeit von Regelung und
Safety.

### Nicht-Ziele dieser Planrevision

- keine Kandidatenauswahl und kein implizites Shortlisting vor vergleichbarer
  Evidence und Owner-Gate;
- keine produktive Connectivity-Domaene, kein Credential-Record, keine
  Slotrotation, keine `StorageEpoch`-Mutation und keine produktive WLAN-
  Persistenz;
- kein produktiver Webserver-, DNS-, QR-, Reconnect- oder Portalpfad;
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

## 3. Geltende R1-Vergleichsanforderung

Jeder Kandidat wird im gleichen, actor-free Aufbau und mit gleichen Eingaben,
Zeitgrenzen, Fehlern und Cut-Points bewertet. Der Spike ist Evidence und keine
zweite Produktionsanwendung.

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

Ein fehlender automatischer Captive-Redirect ist zu protokollieren und nicht
automatisch ein Gesamtfail, wenn der direkte-IP-Fallback voll funktioniert.
Ein fehlender direkter-IP-Fallback ist ein R1-FAIL. Ob der Browservertrag
weiterhin eine harte Pflicht bleibt, entscheidet ausschliesslich der Owner.

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
| Android/iOS/iPadOS/Windows, Browser, QR-Kamera, UART-/Reset-Recovery | `PENDING/NOT_RUN` | Im bisherigen Stand nicht erbracht; keine Umdeklaration als PASS |
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
| `espressif/network_provisioning` 1.2.4 | Component-Manager-Aufloesung, `protocomm`-/HTTP-Transport, SoftAP-Start, NVS-/WiFi-Semantik, Browsergrenze, Ressourcen und Lizenz | offen, kein Browser-PASS behauptet |
| direkter `protocomm`-Pfad | oeffentliche 6.1-APIs, Security-/Version-/Endpointgrenzen, RAM-only-SoftAP im Probe, Ressourcen und fehlende Credentialsemantik | offen, Capability allein ist kein Client-PASS |
| nativer ESP-IDF-Adapter | ESP-IDF-6.1-SoftAP/HTTP-Pfad, direkte IP, Lifecycle, Ressourcen und die bewusst fehlenden DNS-/Committeile | offen, kein Eigenbau vor Gate |
| WiFiManager v2.0.17 | nur falls ein nachvollziehbarer nativer 6.1-Pfad ohne ungeplanten Arduino-Produktionswechsel besteht; Manifest, Lizenz, Storage, Webserver und Ressourcen | konditional, weder bevorzugt noch verworfen |

Die bereits gescreenten Drittanbieter `thorrak/esp_wifi_config`,
`tuanpmt/esp_wifi_manager` und `nordesems/esp-captive-portal` bleiben
historische Screen-Evidence. Sie werden nur dann in die vertiefte 6.1-Matrix
aufgenommen, wenn ein dokumentierter, realer Vorteil gegen die vier
Hauptkandidaten besteht und der Owner diesen Zusatzscope bestaetigt. Ein
Screening ist keine Auswahl und kein produktiver Dependency-Entscheid.

### 5.2 Aktueller Lockfile-Befund und geplante Korrektur

`spikes/issue89_wlan_onboarding/official_network_provisioning/` besitzt aktuell
ein Manifest mit `>=6.0.2,<6.1` und ein Lockfile mit `idf` `6.0.2`. Nach
Planfreigabe wird dieser isolierte Spike auf die exakte 6.1-Basis umgestellt:

1. Manifestconstraint auf eine 6.1-kompatible, weiterhin eng begrenzte
   Bedingung aktualisieren;
2. Component Manager mit dem exakten `IDF_PATH` ausfuehren;
3. `dependencies.lock` neu erzeugen, nicht manuell editieren;
4. `idf`, `network_provisioning`, transitive `cjson`-Version/-Hash,
   Manifest-Hash, Zielchip und Quellquelle festhalten;
5. keine Lockfile- oder Component-Aenderung in den Produktionsgraphen
   uebernehmen.

Wenn die Komponente unter 6.1 nicht reproduzierbar aufloest oder baut, ist das
ein Evidence-Befund fuer diesen Kandidaten und kein Anlass fuer eine
Kompatibilitaets-Wrapper-Architektur.

## 6. Ausfuehrungsplan nach Owner-Freigabe

### Phase 0 – Gate, Checkout und Toolchain-Provenienz

1. PR #158, Issue #89, `docs/ROADMAP.md`, neuer Plan-Commit und der aktuelle
   SESSION-HANDOVER live erneut lesen.
2. Verifizieren, dass der Branch nicht `main` ist, der Arbeitsbaum sauber ist,
   `origin/main` `7029df3997...` entspricht und der PR-Head der freigegebene
   Plan-/Implementierungsstand ist.
3. Die Planfreigabe muss exakt die neue `PLAN_SHA` nennen. Bei einer anderen
   `main`-, PR-, Roadmap-, Issue- oder Planbasis anhalten und neu abgleichen.
4. ESP-IDF aus
   `/var/lib/docker/data/ESP32-Projekte/opt/espressif/esp-idf-v6.1` oder einem
   gleichwertig verifizierten Checkout aktivieren. Vor jedem Build sind
   `git -C "$IDF_PATH" rev-parse HEAD`, exakter Tag, sauberer Zustand,
   `idf.py --version` und `IDF_TOOLS_PATH` zu protokollieren.
5. Bei fehlender oder abweichender Toolchain `BLOCKED` melden. Kein Wechsel
   auf 6.0.2 und kein Ersatzcheckout wird als aktuelle Evidence verwendet.

### Phase A – Host- und 6.1-Capability-Evidence

Die Phasenreihenfolge bleibt billig vor teuer. Jeder Status wird mit exakter
Source-SHA, Toolchainprovenienz, Befehl und Ergebnis dokumentiert.

#### A1. IDF-unabhaengige Integritaet

- Source-Integritaet von `host_contract_test.py` und den drei Probegraphen
  gegen den freigegebenen Planstand pruefen.
- Den vorhandenen Host-Oracle nur als kandidatenneutrale Regression ausfuehren
  und das Ergebnis nicht als Firmware- oder 6.1-Build-PASS ausgeben.
- Produktionsgraph, Secrets und Diffgrenzen gezielt pruefen. Ein Secret,
  eine produktive WLAN-Kopplung, ein `nvs_flash_erase()`-Conveniencepfad oder
  eine unredigierte Credentialausgabe ist ein Stop-/FAIL-Befund.

#### A2. Offizieller `network_provisioning`-Probe unter 6.1

- Manifest und Lockfile wie in Abschnitt 5.2 beschrieben regenerieren.
- Mit eigenem ignorierten Buildverzeichnis bauen, zum Beispiel:

  ```bash
  idf.py -C spikes/issue89_wlan_onboarding/official_network_provisioning \
    -B build/issue89_official_network_provisioning_idf61 build
  ```

- `idf.py size`, Binary-/ELF-/Partition-/RAM-/IRAM-Werte, Dependency-Graph,
  Component-Hash, Manifest-/Lockfile-Hash und Source-SHA sichern.
- Pruefen, dass der Standard-Build-only-/No-Client-Lauf keinen Credential-
  Commit ausfuehrt, dass bei NVS-Initfehlern kein automatisches Erase erfolgt
  und dass Secrets redigiert bleiben.
- Die native Semantik des unveraenderten Managers nicht als volatil oder
  read-only verkuerzen: ein spaeterer echter Set/Apply kann native
  ESP-WiFi-/NVS-Persistenz vor dem erfolgreichen Verbindungstest beruehren.
  Dieser Befund bleibt fuer das spaetere #57-/Recovery-Gate offen.

#### A3. Direkter Protocomm-Probe unter 6.1

- Mit ESP-IDF 6.1 bauen:

  ```bash
  idf.py -C spikes/issue89_wlan_onboarding/direct_protocomm \
    -B build/issue89_direct_protocomm_idf61 build
  ```

- `WIFI_STORAGE_RAM`, SoftAP, HTTPD-Transport, Security-, Versions- und
  `r1-set`/`r1-test`/`r1-commit`-Boundary erneut aus dem 6.1-Build verifizieren.
- Festhalten, dass Handler keine Request-Credentials interpretieren,
  anwenden, persistieren oder einen Kandidaten auswaehlen. Ein erfolgreicher
  Build ist kein Browser-, DNS-, Scan-, Reconnect- oder Recovery-PASS.

#### A4. Nativer HTTP-Probe unter 6.1

- Mit eigenem Buildverzeichnis bauen:

  ```bash
  idf.py -C spikes/issue89_wlan_onboarding/native_http_adapter \
    -B build/issue89_native_http_adapter_idf61 build
  ```

- SoftAP, direkte HTTP-IP-Seite, redigierte volatile Zugangsdaten und die
  bewusst fehlenden DNS-/Captive-/Scan-/Reconnect-/Committeile aus dem neuen
  Build verifizieren.
- Die alte 6.0.2-Groesse nur als Baseline gegenueberstellen; keine fixe
  Budgetgrenze erfinden.

#### A5. Vergleich und Komponenten-/Kandidaten-Evidence

- Fuer alle drei ESP-IDF-Spikes dieselbe Tabelle mit v6.0.2-Baseline,
  v6.1-Wert, Delta, Source-SHA, Toolchain-SHA, Partition, IRAM, DRAM,
  Binary-Hash und Ergebnisstatus erstellen.
- Die vier Hauptkandidaten mit exakter Quelle, Version/Commit, Lizenz,
  transitive Abhaengigkeiten, IDF-/Arduino-Annahme, HTTP-Server-Sharing,
  NVS-/Commitsemantik, Reset-/Recoverygrenze, Wartung und Ressourcenrisiko
  dokumentieren.
- Nur die unter 6.1 tatsaechlich geprueften Eigenschaften als `PASS`
  bezeichnen. Nicht gebaute, nicht geflashte oder nicht getestete Teile
  bleiben `NOT_RUN`, fehlende Toolchain/Hardware bleibt `BLOCKED`.

### Phase B – vergleichbare Client-, Browser- und Recovery-Evidence

Phase B bleibt bis heute `PENDING`; sie darf nach Freigabe der exakten neuen
Plan-SHA auf dem vom Owner ausdruecklich freigegebenen, entbehrlichen
ESP32-WROOM-32E-Development-Testtraeger starten. Fuer diesen kontrollierten
Issue-#89-Spike sind Flash- und Default-NVS-Ueberschreibung beziehungsweise
-Loeschung erlaubt. Ein zusaetzliches Test-NVS, eine separate physische
Testpartition oder ein Backup des bisherigen Development-Stands sind dafuer
nicht erforderlich. Das Projekt-/Benutzer-NVS eines nicht freigegebenen
Produktivgeraets darf weiterhin nicht verwendet werden, und automatisches oder
unbeabsichtigtes Loeschen bleibt unzulaessig.

Die vier Kandidaten erhalten denselben Testaufbau, dieselbe Firmware-
Provenienz je Kandidat und dieselben Cut-Points:

| Bereich | Pflichtnachweis |
|---|---|
| Android | QR-Beitritt, Captive-Angebot, direkte IP, Scan/Formular, falsches Passwort, Abbruch, Commitgrenze, Reconnect, Neustart |
| iOS/iPadOS | dieselben Punkte; OS-Captive-Ansicht und Safari/direkte IP getrennt protokollieren |
| Windows | WLAN-Beitritt, Standardbrowser, Captive-Angebot, direkte IP, lange Eingabe, Fehler/Abbruch, Reconnect und Neustart |
| Browservertrag | Portalstart/-stop, QR-Encoding/Decoding, sichtbare lokale Adresse, no-store/Redaction, Erfolg/Fehler/Timeout |
| Recovery | Write-/Readback-/Reset-/CommitOutcomeUnknown-Cutpoints, alte Konfiguration erhalten, keine alte Epoch reaktivieren |
| Runtime/Safety | actor-free Regel-/Safety-Simulation bleibt bei allen Netzwerkfehlern unabhaengig und fail-closed |
| Ressourcen | Heap, niedrigster Heap, groesster Block, Stack-Watermark, Start/Stop, Scan, Formular, Reconnect, Jitter, Watchdog, Leaks und Handles |

Fuer reale Tests muessen der ausdruecklich freigegebene Dev-Testtraeger, UART,
Resetpfad, Clientgeraete, Partitionierung und Firmware-SHA vorab dokumentiert
sein. Ein zusaetzliches Test-NVS, eine Backupgrenze und ein verifizierter
Power-Cut-Pfad sind keine Voraussetzungen dieses Ownerentscheids. Power-Cut-
Tests sind vollstaendig als `WAIVED_BY_OWNER` aus den verpflichtenden
Acceptance Criteria entfernt. `EN/RTS`-Reset ist kein Power-Cut; UART-, Reset-,
Neustart-, Credential- und Recoverytests bleiben verpflichtend. Fehlt Board-,
UART- oder Resetvoraussetzung oder meldet esptool keine seriellen Daten, lautet
der Nachweis `BLOCKED`; es wird kein Hardware- oder Client-PASS behauptet.

Ein physischer QR-Scan am vorgesehenen Display ist von synthetischem
QR-Encoding und Kamera-/Browsertests getrennt. Ohne bestaetigte Display-
Hardware bleibt der physische Nachweis `BLOCKED_HARDWARE` oder `NOT_RUN`.

### Phase C – Owner-Gate

Nach vergleichbarer Evidence haelt der Builder an und legt ausschliesslich
eine Entscheidungsgrundlage vor. Der Owner entscheidet explizit:

1. `BROWSER_ONLY_REMAINS_HARD_REQUIREMENT=YES|NO`;
2. Kandidat oder Ablehnung aller Kandidaten;
3. Besitzer von Credential-Persistenz, Lifecycle, Reset und Recovery;
4. notwendige Anpassungen an #57, Security, Backup, Reset und #27;
5. minimaler weiterer Integrationsscope.

Bis zur Entscheidung bleiben:

```text
PHASE_A_6_1_REVALIDATION=PASS|FAILED|BLOCKED
PHASE_B_COMPARABLE_CLIENT_EVIDENCE=PENDING
OWNER_CANDIDATE_SELECTION_GATE=NOT_READY
CANDIDATE_SELECTION=OWNER_PENDING_AFTER_COMPARABLE_EVIDENCE
PRODUCTIVE_CONNECTIVITY_PERSISTENCE=NOT_STARTED
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

Die Evidence-Datei `docs/audits/ISSUE_89_WLAN_ONBOARDING_EVIDENCE.md` wird erst
nach autorisierten Laeufen aktualisiert. Sie fuehrt historische und aktuelle
Tabellen getrennt und enthaelt mindestens:

- `HISTORICAL_ESP_IDF=6.0.2` mit alter Source-/Toolchainprovenienz;
- `CURRENT_ESP_IDF=6.1` mit exakter IDF-SHA und aktuellem Source-Head;
- Host-Oracle als kandidatenneutrale Evidence;
- drei getrennte 6.1-Probe-Builds mit Komponenten-/Lockfile-/Ressourcen-
  Evidence;
- vier Kandidaten mit gleicher Bewertungslogik;
- `PHASE_B_COMPARABLE_CLIENT_EVIDENCE=PENDING`, solange kein vergleichbarer
  Clientlauf vollstaendig erfasst ist;
- `PHASE_B_TEST_SETUP=OWNER_AUTHORIZED` und
  `POWER_CUT_TESTS=WAIVED_BY_OWNER` nach dem aktuellen Ownerentscheid;
- keine produktive Kandidaten- oder Persistenzentscheidung.

Es gelten die Begriffe aus `docs/CI_AND_QUALITY_GATES.md`:

- `PASS`: ausgefuehrt und erfolgreich;
- `FAILED`: ausgefuehrt, fehlgeschlagen und blockierend;
- `BLOCKED`: konkrete Voraussetzung fehlt;
- `NOT_RUN`: nicht ausgefuehrt;
- `SKIPPED` und fehlende Angaben sind nie `PASS`.

Ein Phase-A-Build-PASS ist weder Clientakzeptanz noch Hardwareakzeptanz.
Issue-159-Produktions-Smokes, Hosttests und statische Screens ersetzen keine
Browser-, QR-, Recovery- oder Ressourcenmessung des jeweiligen Kandidaten.

## 8. Commits, Dokumentation und Stopregeln

### 8.1 Aktueller Plan-Commit

Dieser Plan-Commit darf nur Planinhalt und die erforderliche aktuelle
Roadmap-Synchronisierung enthalten. Es gibt in diesem Schnitt:

- keine neue Produktionsabhaengigkeit;
- keine Code-, Build- oder Lockfile-Aenderung fuer einen Evidence-Lauf;
- keine neue Evidence und keinen neuen PASS-Claim;
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

Issue #89 ist aus Evaluationssicht erst bereit fuer die Ownerentscheidung,
wenn Phase-A-6.1-Evidence und die vergleichbare Phase-B-Matrix fuer die
bewerteten Kandidaten mit exakten Statuswerten vorliegen. Bis dahin bleiben
`CANDIDATE_SELECTION=OWNER_PENDING_AFTER_COMPARABLE_EVIDENCE`,
`PRODUCTIVE_CONNECTIVITY_PERSISTENCE=NOT_STARTED` und
`ACTUATOR_RELEASE=NO` unveraendert.
