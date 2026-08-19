# Issue #90 – ESP-IDF-NVS-Adapter für `IStateStore`

## Status, Basis und Ziel

Issue #90 hat bis zur Ownerfreigabe der exakten Plan-Commit-SHA ausschließlich
den Status:

`IMPLEMENTATION_NOT_STARTED_PENDING_PLAN_APPROVAL`

Die Software-/Buildphase wird erst nach dieser Freigabe ausführbar. Dieser
Plan führt noch keine Firmware-, Partitionierungs- oder Hardwareänderung aus.

Der Plan wird auf dem live verifizierten PR #116 gestapelt:

- Branch: `agent/issue-90-nvs-adapter-plan`
- Base-Branch: `agent/issue-29-esp32-bringup-plan`
- Base-SHA: `c4c8b33f4dbaef727200ea410d887ec5417aa1b0`
- Planpfad: `docs/tasks/issue-90-esp-idf-nvs-adapter-plan.md`
- Abhängigkeit: PR #116 bleibt Draft; Issue #29 bleibt offen und hardwareblockiert.

PR #116 hat den Software-/Build-Bring-up von Issue #29 dokumentiert. Die
reale Board-, Flash-, UART-, Reset-, Ressourcen- und Hardwareverifikation ist
weiterhin offen. Die dort erzeugte NVS-Tabelle ist nur Buildbaseline.

## Scope und unveränderte Verträge

Implementiert wird ein produktiver ESP-IDF-Adapter für den bestehenden
`device_platform::IStateStore`-Port in `device_platform_esp_idf`.

Unverändert bleiben:

- der direkte, verlustfreie `StateStoreKey`-zu-NVS-Schlüssel gemäß ADR-016;
- die bestehenden Read-/Write-Status einschließlich `CommitOutcomeUnknown`;
- bestehende Configuration-, Run-, Wire- und Schema-Verträge;
- Active-/Fallback-/Head-Semantik;
- keine Fachlogikänderung, keine neue Persistenzarchitektur,
  keine CBOR-/LittleFS-Migration und kein Austausch des bestehenden Stores.

Die Produktionsverschlüsselung von NVS, Flashverschlüsselung und der Schutz
gespeicherter Secrets sind nicht Bestandteil dieses Issues. Das bestehende
separate Security-/Releasegate bleibt unverändert.

## Eindeutiger Partitionsvertrag

Issue #90 verwendet eine dedizierte IStateStore-NVS-Partition:

- Partitionslabel: `state_store`;
- Namespace: `fermentation`;
- aktuelle IStateStore-Schlüssel bleiben direkt und unverändert;
- die Defaultpartition `nvs` wird nicht für IStateStore-Records verwendet.

Die Trennung verhindert eine unkontrollierte Kapazitätsvermischung mit
ESP-IDF-Konsumenten. Die gepinnte ESP-IDF-Basis aktiviert
`CONFIG_ESP_WIFI_NVS_ENABLED` standardmäßig und dokumentiert
`WIFI_STORAGE_FLASH` als Standard. Sollte WLAN später produktiv in den
Composition Root aufgenommen werden, initialisiert dessen eigener Verbraucher
die Defaultpartition `nvs` separat und erhält ein eigenes Kapazitäts- und
Hardwaregate. Die aktuelle 24-KB-Defaultpartition wird nicht als ausreichendes
WLAN-Budget behauptet.

## Lifecycle und Composition Root

`NvsStateStore` besitzt weder `nvs_flash_init_partition()` noch
`nvs_flash_deinit_partition()`.

Der Composition Root besitzt den Lebenszyklus der dedizierten Partition:

1. `nvs_flash_init_partition("state_store")` genau einmal aufrufen;
2. nur bei `ESP_OK` `NvsStateStore` konstruieren;
3. danach die bestehenden Store-Verbraucher konstruieren;
4. Verbraucher zuerst zerstören;
5. `NvsStateStore` zerstören;
6. danach `nvs_flash_deinit_partition("state_store")` aufrufen.

Jeder andere Initialisierungs- oder Partitionsstatus, insbesondere
`ESP_ERR_NVS_NO_FREE_PAGES`, `ESP_ERR_NVS_NEW_VERSION_FOUND`,
`ESP_ERR_NOT_FOUND`, `ESP_ERR_NO_MEM`, `ESP_ERR_NVS_INVALID_STATE` und
unbekannte Flashfehler, verhindert die Store-Konstruktion. Der Composition
Root meldet den Startupfehler, startet keine normale Anwendungsschleife und
führt kein Löschen, Neuformatieren, automatisches Reinitialisieren oder blindes
Wiederholen aus.

`NvsStateStore` öffnet je `read`-/`write`-Operation einen passenden
`nvs_handle_t` für `state_store`/`fermentation` und schließt ihn vor der
Rückkehr. Ein langlebiges Handle wird nicht verwendet: R1 hat keine konkrete
Anforderung für eine dauerhafte Handle-Lebensdauer oder handleübergreifende
Transaktion; Öffnen und Schließen je Operation ist die einfachere Besitz- und
Ressourcenregel. Daraus wird keine unzutreffende Garantie über uncommittete
Zustände abgeleitet.

Der aktuelle `main/app_main.cpp`-Stand konstruiert keinen
`IStateStore`-Verbraucher. `ConfigurationBootstrapStore` und
`RunPersistenceCoordinator` sind vorhandene Fach-/Testkonsumenten, werden
aber vom aktuellen `FermentationApplication::begin()` nicht produktiv
verdrahtet. Daher wird in #90 kein öffentlicher Anwendungskonstruktor und kein
aktueller Composition Root stillschweigend erweitert. Die spätere Übergabe
des Adapters an einen tatsächlich produktiv konstruierten Verbraucher bleibt
als ausdrücklich ausstehender Integrationsschnitt dokumentiert; sie benötigt
bei einer fachlichen API- oder Ablaufänderung einen eigenen Scope-/Planentscheid.

Der Testnachweis trennt deshalb:

- `nvs_state_store_test_seam.hpp` für adapterinterne Open-/Set-/Get-/Commit-/Close-Fehler;
- `nvs_lifecycle_test_fixture.hpp` für die rootseitige Init-/Konstruktions-/Zerstörungsreihenfolge.

Die Produktionsklasse enthält keine Init-/Deinit-Aufrufe; der testseitige
Lifecycle-Fixture ist kein öffentlicher Persistenzport.

## Adapter- und Fehlervertrag

Produktionspfade:

- `lib/device_platform_esp_idf/src/nvs_state_store.hpp`
- `lib/device_platform_esp_idf/src/nvs_state_store.cpp`
- `lib/device_platform_esp_idf/private/nvs_state_store_test_seam.hpp`
- `lib/device_platform_esp_idf/private/nvs_lifecycle_test_fixture.hpp`

Der öffentliche Adapterheader enthält keine ESP-IDF-Handletypen. Die Klasse
implementiert ausschließlich `device_platform::IStateStore`.

In ESP-IDF `v6.0.2 @ 7101770dc6db2667b3c477cc31365dd1acd6db4e` führt
`nvs_set_blob()` die relevanten Flashmutationen aus:

1. neue `BLOB_DATA`-Chunks schreiben;
2. neuen `BLOB_IDX` schreiben;
3. alten Blob-Index entfernen;
4. alte Blob-Datenchunks entfernen.

`nvs_commit()` ist in dieser gepinnten Version aktuell ein No-op: Der Aufruf
prüft das Handle und `NVSHandleSimple::commit()` liefert bei gültigem Handle
`ESP_OK`, ohne weitere Flashmutation. Der Adapter ruft `nvs_commit()` dennoch
auf, weil der unveränderte Portvertrag diesen Schritt verlangt. Jeder nicht
erfolgreiche Commit wird konservativ als `CommitOutcomeUnknown` behandelt.

Fehlerabbildung:

- Initialisierung: Jeder nicht eindeutige Fehler macht den Composition Root
  unbenutzbar; keine Reparatur und kein Retry.
- Open read-only `ESP_ERR_NVS_NOT_FOUND`: `NotFound`.
- Übrige Read-Open-Fehler: `ReadError`.
- Write-Open-Fehler: `WriteError`.
- `nvs_set_blob()` mit sicherer Vorbedingungs- oder Größenverletzung vor einer
  Flashmutation: `WriteError` beziehungsweise `CapacityError`.
- `ESP_ERR_NVS_VALUE_TOO_LONG` oder ein vorgelagerter `maxBytes`-/Kapazitäts-
  fehler darf als `CapacityError` ausgegeben werden.
- `ESP_ERR_NVS_NOT_ENOUGH_SPACE`, `ESP_ERR_NVS_NO_FREE_PAGES`,
  `ESP_ERR_NVS_INVALID_STATE`, Flashfehler, `ESP_ERR_NVS_REMOVE_FAILED` und
  unbekannte Setfehler nach möglichem Mutationsbeginn werden als
  `CommitOutcomeUnknown` ausgegeben, weil der vollständige alte Zustand nicht
  sicher behauptet werden kann.
- `nvs_commit() == ESP_OK` lässt bei erfolgreichem Set den Status `Success` zu.
- Jeder nicht erfolgreiche `nvs_commit()` ist `CommitOutcomeUnknown`.
- Größenabfrage `NOT_FOUND`: `NotFound`.
- geprüfte Größe oberhalb `maxBytes`: `CapacityError`.
- übrige Größen- oder Readfehler: `ReadError`.

Der Read-Pfad ist immer zweistufig:

1. `nvs_get_blob(handle, key, nullptr, &requiredBytes)`;
2. Prüfung von `requiredBytes <= maxBytes` vor jeder Allokation;
3. Allokation höchstens der geprüften Größe;
4. zweiter Read mit exakt dieser Länge.

Ein leerer Blob wird mit einem nicht-nulligen Sentinel und Länge 0 geschrieben.
Eine erfolgreiche Größenabfrage mit Länge 0 liefert ohne Allokation
`Success` und einen leeren `std::string`. Ändert sich die Größe zwischen den
beiden Reads, wird nicht blind wiederholt: eine neue Größe über `maxBytes` ist
`CapacityError`; `INVALID_LENGTH`, nachträgliches `NOT_FOUND` oder eine
abweichende nicht vollständige Rückgabe ist `ReadError`. Teilwerte werden
immer verworfen.

## Kapazitätsrechnung

Das aktuelle vollständige IStateStore-Inventar lautet:

| Gruppe | Anzahl | Maximalrecord | NVS-Einträge je Record |
|---|---:|---:|---:|
| User-Konfiguration | 4 | 301 B | 12 |
| Service-Konfiguration | 4 | 45 B | 4 |
| Program-Katalog | 4 | 32.813 B | 1.036 |
| Manifest | 3 | 149 B | 7 |
| Root | 2 | 114 B | 6 |
| Bootstrap | 2 | 42 B | 4 |
| Run-Checkpoint | 2 | 8.240 B | 262 |
| Run-Head | 1 | 256 B | 10 |

Die maximal gleichzeitig vorhandenen Records umfassen 22 Schlüssel und
150.131 B Recorddaten. Ein zusätzlicher maximaler Austauschrecord benötigt
32.813 B beziehungsweise 1.036 NVS-Einträge.

Die gepinnte NVS-Implementierung verwendet 4.096 B pro Seite, 32 B pro
Eintrag und 126 physische Eintragsslots pro Seite. Für variable Daten stehen
je Chunk höchstens 125 Dateneinträge, also 4.000 B, zur Verfügung. Ein Blob
benötigt je Datenchunk einen Overheadeintrag und zusätzlich einen
`BLOB_IDX`-Eintrag. `nvs_get_stats().available_entries` zieht eine vollständige
126-Einträge-Seite als Reserve ab.

Damit ergeben sich:

- Records: 4.783 Einträge;
- Namespace: 1 Eintrag;
- persistenter Bestand: 4.784 Einträge;
- Peak einschließlich größtem Austausch: 5.820 Einträge;
- zwei freie Seiten als Update-/GC-Reserve: 252 Einträge;
- Untergrenze: `ceil((4.784 + 1.036 + 252) / 126) = 49` Seiten;
- Untergrenze: 196 KiB.

Die 49 Seiten beziehungsweise 196 KiB sind ausdrücklich nur eine Untergrenze.
Vor Auswahl einer Größe wird zusätzlich die gepinnte Seitenpackung inklusive
Blob-Chunk-Grenzen, Fragmentierung, Updatepfad und PageManager-GC nachgebildet.
Ein konservativer nicht packungsoptimierter Oberwert von 69 Seiten beziehungs-
weise 276 KiB ist ebenfalls nur ein Rechenwert, keine freigegebene Größe.

Die aktuelle 24-KB-Buildbaseline ist bereits rechnerisch FAIL:

- `24.576 B < 32.813 B`;
- die gepinnte maximale Blobgröße der 24-KB-Partition liegt ebenfalls unter
  32.813 B.

Der Nachweis wird später reproduzierbar erzeugt durch:

- `scripts/issue_90_nvs_capacity.py`
- `docs/ISSUE_90_CAPACITY_REPORT.md`

Das Skript verwendet das vollständige Inventar, die gepinnten
Entry-/Chunkkonstanten und die Update-/GC-Reserven. Die Recordlimits bleiben
aus den bestehenden Fachquellen abzuleiten und werden nicht still dupliziert
oder verändert.

## Weg zur projektspezifischen 4-MB-Tabelle

Die aktuelle Baseline verwendet `CONFIG_PARTITION_TABLE_SINGLE_APP=y`, keine
projektspezifische CSV und die ESP-IDF-Datei
`components/partition_table/partitions_singleapp.csv` mit 24-KB-`nvs`,
4-KB-`phy_init` und 1-MB-`factory`.

Nach einer materiellen Planrevision mit neuer Plan-SHA und erneuter
Ownerfreigabe wird folgende projektspezifische Datei angelegt:

- `partitions/issue_90_state_store.csv`

Die Profile verwenden dann über `sdkconfig.defaults` die Kconfig-Einstellungen:

```text
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions/issue_90_state_store.csv"
```

`main/CMakeLists.txt` enthält keine eigene CSV-Logik. Die Tabelle wird über
ESP-IDF-Kconfig/CMake eingebunden. `lib/device_platform_esp_idf/CMakeLists.txt`
erhält `PRIV_REQUIRES nvs_flash`.

Untergrenzenlayout mit 49 Seiten:

```text
0x000000–0x000FFF  reservierter Flashbereich
0x001000–0x007FFF  ESP-IDF-Bootloaderbereich
0x008000–0x008FFF  Partitionstabelle
0x009000–0x00EFFF  nvs,         data,nvs,  24 KiB
0x00F000–0x00FFFF  phy_init,    data,phy,  4 KiB
0x010000–0x040FFF  state_store, data,nvs, 196 KiB
0x050000–0x14FFFF  factory,     app,factory, 1 MiB
0x150000–0x3FFFFF  verbleibend  2.75 MiB
```

Beim konservativen 69-Seiten-Rechenwert wäre `state_store` 276 KiB groß, die
Factory-App würde auf `0x60000` ausgerichtet und bis zum 4-MB-Ende blieben
2.625 MiB frei.

Die aktuelle Firmwarebaseline benötigt 194.304 B App-BIN im Bring-up-Profil
und 126.592 B im Release-Profil bei einer 1-MB-Factory-App. Nach
Adapterintegration werden beide Profile erneut gebaut; Appgröße, Alignment,
Bootloader-/Partitionstabellenbereich und verbleibende Flashreserve werden im
Buildbericht festgehalten.

Solange die konkrete R1-Größe nicht durch die gepinnte Rechnung, den
Adapter-Build und den Hardware-/Readbacknachweis belastbar bestätigt ist, wird
keine Partitionstabelle geändert. Jede Tabellenänderung bleibt bis zu einer
materiellen Planrevision mit neuer Plan-SHA und erneuter Ownerfreigabe gesperrt.

## Ausführbarer Testpfad

Primärer Testpfad ist die gepinnte ESP-IDF-NVS-Hostimplementation für Linux,
nicht ein IDF-unabhängiger Rückgabecode-Mock. Die Tests verwenden ein
zustandsbehaftetes Block-Device-Double, das persistente Flashbytes, Schreib-/
Löschoperationen, Operationnummern und absichtliche Unterbrechungen hält.

Neue Testpfade:

- `test/esp_idf_nvs_adapter_host/CMakeLists.txt`
- `test/esp_idf_nvs_adapter_host/sdkconfig.defaults`
- `test/esp_idf_nvs_adapter_host/partitions.csv`
- `test/esp_idf_nvs_adapter_host/main/CMakeLists.txt`
- `test/esp_idf_nvs_adapter_host/main/test_nvs_state_store_host.cpp`
- `test/esp_idf_nvs_adapter_host/main/power_cut_block_device.hpp`
- `test/esp_idf_nvs_adapter_host/main/power_cut_block_device.cpp`

Ausführung nach Planfreigabe:

```bash
export IDF_PATH=/var/lib/docker/data/ESP32-Projekte/opt/espressif/esp-idf-v6.0.2
. "$IDF_PATH/export.sh"
cd /var/lib/docker/data/ESP32-Projekte/ESP32-Fermentationsschrank/test/esp_idf_nvs_adapter_host
idf.py --preview set-target linux
idf.py build
./build/issue_90_nvs_adapter_host.elf
```

Der Hosttest weist Init-/Open-/Close-/Deinit-Reihenfolge, leere und binäre
Werte, Read-Races, Grenzgrößen, Statusabbildung und tatsächliche alte/neue
Werte nach. Ein reiner Rückgabecode-Mock genügt nicht.

## Power-Cut-Matrix und Orakel

Der Hosttest enumeriert für `nvs_set_blob()` vor und nach jedem relevanten
Mutationspunkt:

- jeden `BLOB_DATA`-Chunk;
- den `BLOB_IDX`;
- den alten Blob-Index;
- jeden alten Datenchunk;
- relevante PageManager-/GC-Schreib- und Löschoperationen;
- `nvs_commit()` als No-op-Kontrollpunkt.

Szenarien sind vorhandener alter Wert, nicht vorhandener alter Wert, kleiner
neuer Wert und maximaler 32.813-B-Wert. Drei deterministische Bytepattern und
drei Wiederholungen je Unterbrechungsposition werden ausgeführt.

Nach jedem simulierten Unterbruch wird die Partition neu initialisiert und
gelesen. Zulässig sind ausschließlich der vollständige alte Wert oder der
vollständige neue Wert; bei vorher nicht vorhandenem Wert zusätzlich
`NotFound`. Teilwerte, Mischwerte, abweichende Länge oder ein Erfolg ohne
eindeutigen Readback sind FAIL.

Die Matrix erzeugt je Fall mindestens Szenario, Pattern, Mutationsphase,
Operationnummer, Set-/Commitstatus, Reinitialisierungsstatus, Reset-/Hoststatus,
beobachtete Länge sowie SHA-256 von altem, neuem und gelesenem Wert.

Auf realer Hardware wird nach #29-Hardwarefreigabe ein Owner-verifizierter
Power-Cut-Aufbau verwendet. Die Versorgungsschaltung und Steuerleitung werden
nicht geraten, sondern als Hardwarefixture dokumentiert. Pro Szenario werden
mindestens zehn zeitlich verteilte Unterbrechungen über die kalibrierte Dauer
des `nvs_set_blob()`-Aufrufs sowie drei saubere Neustartkontrollen ausgeführt.
Die Hostmatrix liefert die interne Phasenabdeckung; die Hardwarematrix liefert
den realen Reinitialisierungs-, Flash- und Readbacknachweis.

## Konkrete Umsetzungs- und Commit-Schnitte

Nach Freigabe des Plancommits werden die Implementierungsänderungen in diesen
inhaltlich getrennten Schnitten erstellt:

1. **Adapter und Buildabhängigkeit**
   - Produktionsadapter und `PRIV_REQUIRES nvs_flash` in
     `lib/device_platform_esp_idf`;
   - Nachweis: keine ESP-IDF-Typen im portablen Fachkern und erfolgreicher
     Komponentenbuild.

2. **Privater Testseam und Host-NVS-Test**
   - private Adapter-/Lifecycle-Fixtures und Hostprojekt unter
     `test/esp_idf_nvs_adapter_host`;
   - Nachweis: stateful NVS-Hosttest, Statusmatrix, Read-Race und A/B-Orakel.

3. **Lifecycle-/Composition-Root-Schnitt**
   - rootseitige Init-/Konstruktions-/Zerstörungsreihenfolge als Fixture;
   - keine Änderung an `main/app_main.cpp`, solange dort kein bestehender
     produktiver IStateStore-Verbraucher vorhanden ist;
   - Nachweis: Initfehler verhindert Storekonstruktion und normale Anwendung.

4. **Kapazitäts- und Partitionsnachweis**
   - Kapazitätsskript und Report;
   - `partitions/issue_90_state_store.csv` und Kconfigdefaults erst nach
     materieller Planrevision und Ownerfreigabe;
   - Nachweis: Inventar, Entry-/Seitenrechnung, Update-/GC-Reserve, 4-MB-
     Layout, Firmwaregröße und Flashreserve.

5. **Softwareverifikation**
   - Hosttest wie oben;
   - danach gezielte Profile und statische Prüfungen:

     ```bash
     python3 scripts/build_esp_idf_profiles.py all
     python3 scripts/run_esp_idf_static_analysis.py all
     python3 scripts/build_report.py --append \
       --esp-idf-profiles bringup release \
       --source-git-sha <exakter-Implementierungs-HEAD>
     python3 scripts/check_architecture_boundaries.py
     python3 scripts/check_secrets.py
     python3 scripts/selftest_quality_gates.py
     git diff --check
     ```

   - Nachweis: Build-, Static-Analysis-, Architektur-, Secret- und Diffstatus.

6. **Hardwareverifikation**
   - exakt verifiziertes ESP32-Ziel, dedizierte Partition, Neustart,
     Readback und Power-Cut-Fixture;
   - Nachweis: UART-Logs, Resetursachen, Partitionsdump, A/B-Hashes und
     vollständige Matrix.

7. **Ressourcen und Wear**
   - `docs/ISSUE_90_BUILD_REPORT.md` und späterer Verifikationsreport;
   - Flash-/Appgröße, DRAM, Heap, größter Block, Stack-HWM, NVS-Statistiken,
     Initzeit und Schreiblastbudget;
   - repräsentativer Belastungstest mit dokumentierter Nachweisgrenze;
   - kein unbegrenzter Wear-Leveling- oder Lebensdauernachweis.

8. **Lizenz und Notices**
   - Aktualisierung von `docs/THIRD_PARTY_COMPONENTS.md`,
     `docs/audits/THIRD_PARTY_SOURCE_AND_LICENSE_REVIEW.md`,
     `docs/audits/COMPONENT_EVALUATIONS.md` und bei Bedarf
     `docs/ADOPT_OR_BUILD.md`;
   - Nachweis des konkret verwendeten ESP-IDF-`nvs_flash`-Dateisatzes,
     Commitpins, Apache-2.0-Status und der Notices.

Der aktuelle Plancommit bleibt davon getrennt und enthält ausschließlich Plan
und minimale Roadmapänderung.

## Ressourcen, Wear und Security

Der Ressourcenreport erfasst Appgröße, Flashreserve, DRAM, Heap, größten
zusammenhängenden Block, Stack-HWM, NVS-Statistiken und Initialisierungszeit.
Das Schreiblastbudget wird aus den tatsächlichen Konfigurations- und
Checkpointfrequenzen berechnet. Der repräsentative Belastungstest weist nur das
geprüfte Lastfenster nach.

Es wird kein allgemeiner NVS-Lebensdauer- oder unbegrenzter Wear-Leveling-
Nachweis behauptet. Herstellervertrag, berechnetes Schreiblastbudget,
`nvs_get_stats()` und der Belastungstest werden getrennt dokumentiert.

In #90 wird keine NVS-/Flashverschlüsselung aktiviert, keine `nvs_keys`-
Partition angelegt und kein Schutz gespeicherter Secrets behauptet. Das
separate Security-/Releasegate `EVALUATE_BEFORE_RELEASE` bleibt bestehen.

## Lizenz- und Herkunftsnachweis

Verwendet wird die eingebaute ESP-IDF-Komponente `nvs_flash` aus dem gepinnten
Dateisatz, ohne Quelltext in das Repository zu kopieren:

- `components/nvs_flash/include/nvs.h`
- `components/nvs_flash/include/nvs_flash.h`
- `components/nvs_flash/src/nvs_api.cpp`
- `components/nvs_flash/src/nvs_handle_simple.cpp`
- `components/nvs_flash/src/nvs_storage.cpp`
- `components/nvs_flash/src/nvs_pagemanager.cpp`
- `components/nvs_flash/src/nvs_page.cpp`
- `components/nvs_flash/src/nvs_partition.cpp`
- `components/nvs_flash/src/nvs_partition_manager.cpp`
- `components/nvs_flash/src/nvs_partition_lookup.cpp`
- `components/nvs_flash/src/nvs_types.cpp`
- `components/nvs_flash/src/nvs_item_hash_list.cpp`
- `components/nvs_flash/src/nvs_platform.cpp`
- `components/nvs_flash/private_include/nvs_constants.h`

Der Hosttest verwendet zusätzlich den gepinnten NVS-Host-/Block-Device-Pfad.
Die Lizenz ist ESP-IDF/Apache-2.0; konkrete Notices und der tatsächliche
Dateisatz werden in den Repository-Auditdokumenten festgehalten. Die
Projektlizenz in `docs/LICENSE_STATUS.md` wird nicht entschieden oder geändert.

## Gepinnte Herstellerquellen

- [`nvs.h`](https://github.com/espressif/esp-idf/blob/7101770dc6db2667b3c477cc31365dd1acd6db4e/components/nvs_flash/include/nvs.h)
- [`nvs_flash.h`](https://github.com/espressif/esp-idf/blob/7101770dc6db2667b3c477cc31365dd1acd6db4e/components/nvs_flash/include/nvs_flash.h)
- [`nvs_api.cpp`](https://github.com/espressif/esp-idf/blob/7101770dc6db2667b3c477cc31365dd1acd6db4e/components/nvs_flash/src/nvs_api.cpp)
- [`nvs_handle_simple.cpp`](https://github.com/espressif/esp-idf/blob/7101770dc6db2667b3c477cc31365dd1acd6db4e/components/nvs_flash/src/nvs_handle_simple.cpp)
- [`nvs_storage.cpp`](https://github.com/espressif/esp-idf/blob/7101770dc6db2667b3c477cc31365dd1acd6db4e/components/nvs_flash/src/nvs_storage.cpp)
- [`nvs_constants.h`](https://github.com/espressif/esp-idf/blob/7101770dc6db2667b3c477cc31365dd1acd6db4e/components/nvs_flash/private_include/nvs_constants.h)
- [`nvs_pagemanager.cpp`](https://github.com/espressif/esp-idf/blob/7101770dc6db2667b3c477cc31365dd1acd6db4e/components/nvs_flash/src/nvs_pagemanager.cpp)
- [`partitions_singleapp.csv`](https://github.com/espressif/esp-idf/blob/7101770dc6db2667b3c477cc31365dd1acd6db4e/components/partition_table/partitions_singleapp.csv)
- [`esp_wifi.h`](https://github.com/espressif/esp-idf/blob/7101770dc6db2667b3c477cc31365dd1acd6db4e/components/esp_wifi/include/esp_wifi.h)
- [`esp_wifi_types_generic.h`](https://github.com/espressif/esp-idf/blob/7101770dc6db2667b3c477cc31365dd1acd6db4e/components/esp_wifi/include/esp_wifi_types_generic.h)
- [`esp_wifi/Kconfig`](https://github.com/espressif/esp-idf/blob/7101770dc6db2667b3c477cc31365dd1acd6db4e/components/esp_wifi/Kconfig)

## Roadmap, Draft-PR und Owner-Gate

`docs/ROADMAP.md` wird nur minimal angepasst:

- Stand auf den aktuellen Planbeginn datieren;
- Issue #90 auf `IMPLEMENTATION_NOT_STARTED_PENDING_PLAN_APPROVAL` setzen;
- dedizierte `state_store`-Partition und Namespace `fermentation` als
  Planentscheidung ausweisen;
- NVS-/Partitions-/Power-Cut-/Readback-/Ressourcen-/Wear- und Securitygates
  offen lassen;
- Reihenfolge, Stacking und Issue #29 unverändert lassen.

Nach dem einzigen Plancommit enthält der Draft-PR exakt:

```text
Plan: docs/tasks/issue-90-esp-idf-nvs-adapter-plan.md
Plan commit: <exakte SHA des einzigen Plan-Commits>
Base branch: agent/issue-29-esp32-bringup-plan
Base SHA: c4c8b33f4dbaef727200ea410d887ec5417aa1b0
Dependency: STACKED_ON_PR_116; PR #116 Draft; Issue #29 offen und hardwareblockiert
Implementation: IMPLEMENTATION_NOT_STARTED_PENDING_PLAN_APPROVAL
```

Es gibt kein Ready-for-review, keinen Merge, kein Auto-Merge, kein
Issueschließen, kein Branchlöschen und keinen Force-Push. Nach Veröffentlichung
wird genau ein aktueller `SESSION HANDOVER`-Kommentar für den neuen Draft-PR
geführt.

## Retarget nach PR #116

Nach dem Merge von PR #116 wird live geprüft, dass dessen identische Commits in
`main` enthalten sind.

Ein neuer Branchverweis oder ein reines Retarget des unveränderten #90-Branches
auf `main` verändert weder Commit-SHA noch Plan-SHA und benötigt keine neue
Planfreigabe. Eine neue Plan-SHA entsteht erst durch neu erzeugte Commits,
insbesondere nach Rebase, Cherry-Pick, Branch-Neuerzeugung oder materieller
Plan-/Grundlagenänderung. In diesen Fällen ist vor der Umsetzung eine erneute
Ownerfreigabe erforderlich.
