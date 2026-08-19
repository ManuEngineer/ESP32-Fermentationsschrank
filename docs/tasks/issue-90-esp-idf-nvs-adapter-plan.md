# Issue #90 – ESP-IDF-NVS-Adapter für `IStateStore`

## Status, Basis und Freigabegrenze

Der einzig gültige Status dieses Plans und der geplanten #90-Umsetzung ist bis
zur Ownerfreigabe der exakten Plan-Commit-SHA:

`IMPLEMENTATION_NOT_STARTED_PENDING_PLAN_APPROVAL`

Die Software-/Host-/Buildphase wird erst nach dieser Freigabe ausführbar.
Dieser Plan führt noch keine Firmware-, Partitionierungs-, Test-, CI- oder
Hardwareänderung aus.

Der Plan ist auf PR #116 gestapelt:

- Branch: `agent/issue-90-nvs-adapter-plan`
- Base-Branch: `agent/issue-29-esp32-bringup-plan`
- Base-SHA: `c4c8b33f4dbaef727200ea410d887ec5417aa1b0`
- Planpfad: `docs/tasks/issue-90-esp-idf-nvs-adapter-plan.md`
- Abhängigkeit: `STACKED_ON_PR_116`; PR #116 bleibt Draft, Issue #29 bleibt
  offen.

PR #116 liefert die aktuelle Software-/Build-Basis. Sein reales Board-,
Flash-, UART-, Reset-, Standard-Flash-, Ressourcen- und Hardwaregate ist nicht
geschlossen. Deshalb sind nach Planfreigabe Adapter-, Hosttest-, Kapazitäts-
und Buildarbeiten zulässig; die reale Standard-Flash-/Partitions-, Power-Cut-,
Readback-, Ressourcen- und Wear-Verifikation bleibt vom offenen #29-Gate
abhängig. #29 blockiert damit nicht pauschal jede #90-Softwarearbeit.

## Scope und unveränderte Verträge

Issue #90 implementiert den konkreten ESP-IDF-Adapter für den bestehenden
`device_platform::IStateStore`-Port in `device_platform_esp_idf`. Es wird weder
ein neuer Persistenzport noch eine neue Fach- oder Persistenzarchitektur
eingeführt.

Unverändert bleiben:

- der direkte, verlustfreie `StateStoreKey`-zu-NVS-Schlüssel gemäß ADR-016;
- Namespace- und Partitionsabgrenzung als Adapterdetail;
- `StateStoreReadStatus`, `StateStoreWriteStatus` einschließlich
  `CommitOutcomeUnknown`;
- Envelope-, Schema-, Active-/Fallback-/Head-, Lauf- und Wire-Verträge;
- `IStateStore` als generischer Schlüssel-/Binärwert-Port;
- keine Fachlogikänderung, kein Ersatz des bestehenden Stores, keine CBOR-/
  LittleFS-Migration und keine Änderung des Schema-1-Wireformats.

Nicht Bestandteil sind NVS-/Flashverschlüsselung, eine `nvs_keys`-Partition
oder ein Schutzversprechen für gespeicherte Secrets. Das separate
Security-/Releasegate `EVALUATE_BEFORE_RELEASE` bleibt unverändert bestehen.

## Repository- und Fachquellen

Die folgenden Quellen sind vor Umsetzung erneut auf dem freigegebenen Base-/PR-
Stand zu prüfen. Die Links zu Repositorydateien sind zugleich die Rückverfol-
gung für die Kapazitätsinventur.

| Quelle | Verbindliche Aussage für #90 |
|---|---|
| [`state_store.hpp`](../../lib/device_platform/src/state_store.hpp) | getrennte Read-/Write-Enums, `maxBytes` nur im Read-Vertrag, alte/neue Atomizitätsgarantie und konservatives `CommitOutcomeUnknown` |
| [`state_store_key.hpp`](../../lib/device_platform/src/state_store_key.hpp) | gültige Schlüssel: 1–15 Bytes, `[A-Za-z0-9_.-]`; daher direkte verlustfreie NVS-Abbildung ohne Adapter-Hash |
| [`storage_envelope.hpp`](../../lib/device_platform/src/storage_envelope.hpp) | Envelope-Version, Header-/Payloadgrößen und technische Maximalrecordgröße |
| [`storage_slot_candidates.hpp`](../../lib/device_platform/src/storage_slot_candidates.hpp) | begrenzte Slotgruppen und Read-/Capacity-Propagation |
| [`configuration_storage_contract.hpp`](../../lib/fermentation_app/src/configuration_storage_contract.hpp) | exakter Konfigurationsschlüsselbestand und Recordtypen: `uc0..uc3`, `sc0..sc3`, `pc0..pc3`, `cm0..cm2`, `cr0..cr1`, `cb0..cb1` |
| [`configuration_limits.hpp`](../../lib/fermentation_app/src/configuration_limits.hpp) | User-/Programmpayload, Envelope-Maxima und Slotanzahlen |
| [`configuration_document_codec.cpp`](../../lib/fermentation_app/src/configuration_document_codec.cpp) und [`configuration_graph_store.cpp`](../../lib/fermentation_app/src/configuration_graph_store.cpp) | tatsächliche Konfigurations-Recordgrenzen und Aufrufer-`maxBytes` |
| [`run_persistence_contract.hpp`](../../lib/fermentation_app/src/run_persistence_contract.hpp) | 8.192-B-Run-Payload und zwei Checkpointslots |
| [`run_persistence_codec.cpp`](../../lib/fermentation_app/src/run_persistence_codec.cpp) und [`run_persistence_coordinator.cpp`](../../lib/fermentation_app/src/run_persistence_coordinator.cpp) | 8.240-B-Checkpointrecord, 256-B-Headrecord, `rc0`, `rc1`, `rh0` und Schreibreihenfolge |
| [`CONFIGURATION_PERSISTENCE.md`](../../docs/CONFIGURATION_PERSISTENCE.md) | generischer Store-, Recovery-, Readback- und Ressourcenvertrag |
| [`RUN_PERSISTENCE.md`](../../docs/RUN_PERSISTENCE.md) und [`RECOVERY_AND_INTERRUPTION.md`](../../docs/RECOVERY_AND_INTERRUPTION.md) | Lauf-/Unterbrechungs-/Recoverygrenzen; kein zweiter Adaptervertrag |
| [`ADR-016_KONFIGURATIONSSPEICHER_BACKEND.md`](../../docs/ADR-016_KONFIGURATIONSSPEICHER_BACKEND.md) und [`DECISIONS.md`](../../docs/DECISIONS.md) | ESP-IDF-NVS als produktives Backend, direkter begrenzter Schlüsselraum, keine Eigenimplementierung von Recordspeicher/GC |
| [`ADR-013_REUSABLE_DEVICE_PLATFORM.md`](../../docs/ADR-013_REUSABLE_DEVICE_PLATFORM.md), [`ARCHITECTURE.md`](../../docs/ARCHITECTURE.md), lokale [`AGENTS.md`](../../lib/device_platform_esp_idf/AGENTS.md) | Produktionsadapter ausschließlich in `device_platform_esp_idf`; keine Fach- oder Composition-Root-Logik dort |
| [`HARDWARE.md`](../../docs/HARDWARE.md), [`OPEN_POINTS.md`](../../docs/OPEN_POINTS.md), [`RESOURCE_BUDGET_AND_MAINTENANCE.md`](../../docs/RESOURCE_BUDGET_AND_MAINTENANCE.md) | 4-MB-/kein-PSRAM-Basis, offene reale Flash-/Ressourcen-/Hardwaregates und Wear-/Schreiblastgate |
| [`ACCEPTANCE_TESTS.md`](../../docs/ACCEPTANCE_TESTS.md), [`ESP_IDF_UPGRADE_CONTRACT.md`](../../docs/ESP_IDF_UPGRADE_CONTRACT.md) | PASS/BLOCKED/NOT_RUN-Orakel, aktorfreie Hardwaregrenze und gepinnte ESP-IDF-Profile |
| [`CI_AND_QUALITY_GATES.md`](../../docs/CI_AND_QUALITY_GATES.md), [`.github/workflows/build.yml`](../../.github/workflows/build.yml) | zulässige Befehle, CI-Einbindung, Draft-/Owner-Gate und Artefakte |
| [`CMakeLists.txt`](../../CMakeLists.txt), [`main/CMakeLists.txt`](../../main/CMakeLists.txt), [`main/app_main.cpp`](../../main/app_main.cpp), [`sdkconfig.defaults`](../../sdkconfig.defaults) | aktueller ESP-IDF-Kompositions-/Build-Stand; aktuell kein produktiver `IStateStore`-Verbraucher und Single-App-Baseline |
| [`ISSUE_29_BUILD_REPORT.md`](../../docs/ISSUE_29_BUILD_REPORT.md) und Roadmap | aktuelle Buildpartition nur als Baseline; reale #29-Nachweise bleiben offen |

Die Live-Anforderungen sind [Issue #90](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/issues/90), die Hardwareabhängigkeit [Issue #29](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/issues/29) und der aktuelle Stack [PR #116](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/pull/116). Eine historische Planfassung ersetzt diese konsolidierte Fassung nicht.

## Eindeutiger Partitions- und Namespacevertrag

Der Adapter verwendet ausschließlich eine dedizierte NVS-Partition:

- Partitionslabel: `state_store`;
- Partitionstyp/Subtyp: `data,nvs`;
- Namespace: `fermentation`;
- IStateStore-Schlüssel: direkt und unverändert als NVS-Key;
- Defaultpartition `nvs`: nicht für IStateStore-Records.

Die Trennung verhindert, dass die Adapterkapazität mit anderen Konsumenten
vermischt wird. Der gepinnte ESP-IDF-WLAN-Vertrag aktiviert
`CONFIG_ESP_WIFI_NVS_ENABLED` standardmäßig und verwendet standardmäßig
`WIFI_STORAGE_FLASH`; dieser Verbraucher gehört zur separaten Defaultpartition
`nvs` und erhält kein stillschweigendes Budget aus `state_store`. Eine spätere
produktive WLAN-Verdrahtung muss `nvs` mit eigenem Kapazitäts- und Hardwaregate
initialisieren.

### Vorgesehener R1-Auswahlentscheid (wird mit exakter Planfreigabe verbindlich)

Mit der Ownerfreigabe der exakten neuen Plan-Commit-SHA soll für den #90-
eigenen Adapter `state_store = 69 * 4096 = 282.624 B` (276 KiB) verbindlich
werden. Die Rechnung unten beweist eine Untergrenze von 49 Seiten; 69 Seiten
sollen der mit dieser exakten Planfreigabe verbindliche Auswahlwert sein und
enthalten 20 zusätzliche Seiten für Seitenpackung, Fragmentierung und die
begrenzten Update-/GC-Situationen. Bis zu dieser Ownerfreigabe bleibt der
Status `IMPLEMENTATION_NOT_STARTED_PENDING_PLAN_APPROVAL`; weder Adapter- noch
R1-Partitionsentscheidung gilt bereits als freigegeben.

Nach dieser Ownerfreigabe soll die Umsetzung dieser normalen
Partitionstabellenänderung ohne zweiten Planentscheid zulässig sein, wenn alle
folgenden Annahmen bestätigt sind:

1. #29 bestätigt den realen 4-MB-Flash und die ESP32-Standardpartitionierung;
2. die vollständige Inventarrechnung bleibt bei höchstens 69 Seiten und die
   deterministische Vorbefüllungs-/Rotationsprüfung erreicht die geforderten
   freien Seiten und den GC-Nachweis;
3. beide ESP-IDF-Profile bauen mit der in diesem Plan beschriebenen
   1-MB-Factory-App, dem `state_store`-Layout und einer positiven
   Flashreserve;
4. es werden keine NVS-Secrets, keine Verschlüsselung und keine zusätzlichen
   #90-fremden Verbraucher in `state_store` aufgenommen.

Eine abweichende reale Flashbasis, ein rechnerisches Ergebnis über 69 Seiten,
ein nicht passendes App-/Alignmentbudget oder ein zusätzlicher Verbraucher ist
eine materielle Abweichung: Umsetzung anhalten, neue Plan-SHA erzeugen und
erneute Ownerfreigabe einholen. Bei bestätigten Annahmen bleibt nach der
Ownerfreigabe kein pauschaler zweiter Planstopp vor der Tabellenänderung
bestehen.

## Lifecycle, Eigentum und Produktionsbindung

`NvsStateStore` führt weder `nvs_flash_init_partition()` noch
`nvs_flash_deinit_partition()` aus und besitzt keine allgemeine Lease-,
Referenz- oder Laufzeit-Shutdown-Infrastruktur.

Sobald ein tatsächlicher produktiver `IStateStore`-Verbraucher verdrahtet wird,
besitzt der jeweilige ESP-IDF Composition Root diesen einfachen R1-Ablauf:

1. `nvs_flash_init_partition("state_store")` genau einmal vor der
   Store-Konstruktion aufrufen;
2. nur bei `ESP_OK` `NvsStateStore` konstruieren;
3. danach den tatsächlichen Verbraucher konstruieren und ihm den Store
   übergeben;
4. bei jeder Abweichung von `ESP_OK` fail-closed starten: Fehler loggen,
   Store/Verbraucher nicht konstruieren, keine normale Anwendungsschleife und
   kein Aktorpfad;
5. beim kontrollierten Ende zuerst Verbraucher, dann `NvsStateStore`, zuletzt
   `nvs_flash_deinit_partition("state_store")` zerstören.

`ESP_ERR_NVS_NO_FREE_PAGES`, `ESP_ERR_NVS_NEW_VERSION_FOUND`,
`ESP_ERR_NVS_NOT_FOUND`, `ESP_ERR_INVALID_ARG`, `ESP_ERR_NO_MEM`,
`ESP_ERR_NVS_INVALID_STATE`, `ESP_ERR_NVS_NOT_INITIALIZED` und unbekannte
Flashfehler werden nicht gelöscht, nicht formatiert, nicht automatisch
reinitialisiert und nicht blind wiederholt. Ein Deinitfehler wird diagnostisch
festgehalten und beendet die weitere Nutzung; er erzeugt keine Reparatur.

Der aktuelle [`main/app_main.cpp`](../../main/app_main.cpp) konstruiert keinen
produktiven `IStateStore`-Verbraucher. Deshalb ändert #90 diesen Composition
Root nicht stillschweigend. Die produktive Root-Integration ist ein ausdrücklich
bedingter Implementierungsschnitt am Ort des ersten realen Verbrauchers und
wird dort gemeinsam mit diesem Verbraucher getestet. Testseitige Init-/Deinit-
Orchestrierung liegt ausschließlich unter `test/`, nicht im Produktionsmodul.

Ein Handle wird pro `read`-/`write`-Operation geöffnet und vor der Rückkehr
geschlossen. Ein langlebiges Handle ist in R1 nicht erforderlich; es gibt keine
konkrete Nebenläufigkeits-, Transaktions- oder Recoveryanforderung dagegen.
Die Wahl ist die einfachste Besitz- und Ressourcenregel und behauptet nicht,
uncommittete Zustände zu verhindern. Der Store besitzt nur seine
Operationslebensdauer; der Root besitzt Partition und übergeordnete Objekte.

## Produktionsadapter und Fehlervertrag

Vorgesehene Produktionsdateien:

- `lib/device_platform_esp_idf/src/nvs_state_store.hpp`;
- `lib/device_platform_esp_idf/src/nvs_state_store.cpp`;
- `lib/device_platform_esp_idf/CMakeLists.txt` mit `PRIV_REQUIRES nvs_flash`.

Der Adapter implementiert ausschließlich `device_platform::IStateStore`. Der
portseitige öffentliche Header und der Fachkern bleiben frei von ESP-IDF. Die
konkrete `NvsStateStore`-Klasse exponiert keinen rohen Handle und keine
Lifecycle-Operation; ihr Handle bleibt ausschließlich innerhalb der
Operationsimplementierung.

`nvs_set_blob()` mutiert in der gepinnten Basis tatsächlich während des
Aufrufs: neue `BLOB_DATA`-Chunks, neuer `BLOB_IDX`, Entfernung des alten
Blob-Index und Entfernung alter Datenchunks. `nvs_commit()` ist in dieser
Version aktuell ein No-op: Bei gültigem Handle prüft `nvs_api.cpp` das Handle
und `NVSHandleSimple::commit()` liefert `ESP_OK`, ohne weitere Flashmutation.
Der Adapter ruft `nvs_commit()` dennoch auf, weil dieser Aufruf Teil des
Adapterablaufs ist; jeder nicht erfolgreiche Commit bleibt konservativ
`CommitOutcomeUnknown`.

### Vollständige Fehlerabbildung

`maxBytes` wird ausschließlich im Read-Pfad verwendet. Es ist kein
Schreibfehlerargument und darf in der Set-/Commit-Matrix nicht als Grund für
`WriteError`, `CapacityError` oder `CommitOutcomeUnknown` erscheinen.

| Phase / ESP-IDF-Aufruf | Ergebnisabbildung | Garantie / Begründung |
|---|---|---|
| Root: `nvs_flash_init_partition` | `ESP_OK` erlaubt Root-Fortsetzung; jeder andere Status ist Startup-Failure und fail-closed | Kein Store existiert; kein Löschen, Formatieren oder Retry |
| Read-open: `nvs_open_from_partition(..., NVS_READONLY, ...)` | `ESP_ERR_NVS_NOT_FOUND` → `NotFound`; jeder andere Fehler → `ReadError` | Vor `nvs_get_blob` keine Mutation; gültige feste Partition/Namespace/Keys machen `INVALID_NAME` und `INVALID_ARG` im Normalpfad unmöglich |
| Write-open: `nvs_open_from_partition(..., NVS_READWRITE, ...)` | Jeder Fehler → `WriteError` | `nvs_set_blob` wurde nicht erreicht; alter Wert sicher unverändert. `INVALID_NAME`/`INVALID_ARG` sind bei validierten Konstanten unmöglich |
| Größenabfrage: `nvs_get_blob(handle, key, nullptr, &requiredBytes)` | `ESP_OK` → weiter; `NOT_FOUND` → `NotFound`; jeder andere Fehler → `ReadError` | `INVALID_LENGTH` ist bei nicht-nulligem `length` und nulligem Ausgabepuffer ein kontrolliert unmöglicher Normalpfad; ein injizierter oder unbekannter Fehler bleibt ReadError |
| lokale Read-Grenze nach Größenabfrage | `requiredBytes > maxBytes` → `CapacityError` | Keine Allokation und kein zweiter Read; der gespeicherte alte Wert bleibt unangetastet |
| zweiter Read: `nvs_get_blob(handle, key, buffer, &readBytes)` | `ESP_OK` nur bei exakt `readBytes == requiredBytes` → `Success`; `NOT_FOUND` nach erfolgreicher Größenabfrage → `ReadError`; `INVALID_LENGTH` mit beobachteter Größe `> maxBytes` → `CapacityError`, sonst `ReadError`; alles andere → `ReadError` | Keine Wiederholung und kein Teilwert. Eine Änderung zwischen beiden Abfragen wird nicht verschleiert |
| lokale Write-Vorprüfung | Länge oberhalb der aus gepinnten NVS-Konstanten abgeleiteten maximalen Blobgröße → `CapacityError` | `nvs_set_blob` wird nicht aufgerufen; alter Wert sicher unverändert. `maxBytes` ist hier unzulässig |
| Write-open erfolgreich, `nvs_set_blob` → `ESP_OK` | Danach `nvs_commit` aufrufen | Set hat bereits mutiert; erst der Commitstatus entscheidet den API-Status |
| `nvs_set_blob` → `ESP_ERR_NVS_VALUE_TOO_LONG` | `CapacityError`, nur entsprechend der gepinnten Vorprüfungs-/Chunkgrenze | Dieser Fehler liegt vor dem erfolgreichen neuen Blob-Index; die alte logische Version bleibt. Ein andersherum nicht eindeutig auflösbarer Längenfehler wird als Unknown behandelt |
| `nvs_set_blob` → `ESP_ERR_NVS_NOT_ENOUGH_SPACE`, `ESP_ERR_NVS_NO_FREE_PAGES`, `ESP_ERR_NO_MEM`, `ESP_ERR_NVS_INVALID_STATE`, `ESP_ERR_FLASH_OP_FAIL`, `ESP_ERR_NVS_REMOVE_FAILED` oder unbekannter Fehler | `CommitOutcomeUnknown` | Der Aufruf kann bereits neue Chunks, Index, Page-GC oder Entfernung begonnen haben. Der alte Wert darf nicht sicher behauptet werden; `ESP_ERR_NVS_REMOVE_FAILED` ist ausdrücklich konservativ Unknown |
| `nvs_set_blob` → `ESP_ERR_NVS_INVALID_HANDLE`, `ESP_ERR_NVS_READ_ONLY`, kontrolliert unmögliche `INVALID_ARG`/`INVALID_NAME` vor Storage-Mutation | `WriteError` nur, wenn der Testnachweis die Vor-Mutationsprüfung dieses konkreten Pfads bestätigt; sonst `CommitOutcomeUnknown` | Feste Keys, RW-Handle, nicht-nulliger Empty-Sentinel und Lifecycle machen diese Fehler im Produktionsnormalpfad unmöglich. Ein nicht eindeutig phasenauflösbarer Fehler nach Mutationsbeginn bleibt Unknown |
| `nvs_commit` → `ESP_OK` nach erfolgreichem Set | `Success` | Der gepinnte Commit ist No-op; der erfolgreiche Set-Pfad hat den vollständigen neuen Wert gespeichert |
| `nvs_commit` → jeder nicht erfolgreiche Status | `CommitOutcomeUnknown` | Auch ein injizierter oder zukünftiger Commitfehler darf nicht als sicher unveränderter Zustand ausgegeben werden |

Ein leerer Wert wird mit einem nicht-nulligen privaten Sentinel und Länge 0 an
`nvs_set_blob` übergeben. Damit werden keine null-pointer-spezifischen Annahmen
eingeführt. Die Read-Größenabfrage liefert 0; danach wird höchstens ein
Sentinelpuffer ohne Wertallokation verwendet und ein leerer `std::string` mit
`Success` zurückgegeben.

## Deterministischer Read-Pfad

Jeder Read besteht aus genau zwei `nvs_get_blob`-Aufrufen:

1. `nvs_get_blob(handle, key, nullptr, &requiredBytes)`;
2. `requiredBytes <= maxBytes` prüfen;
3. nur dann einen Puffer von exakt `requiredBytes` Bytes allokieren;
4. `nvs_get_blob(handle, key, buffer, &readBytes)` mit exakt dieser Länge;
5. nur bei vollständiger, exakt gleich großer Rückgabe den Wert ausgeben.

Es gibt niemals eine Allokation oberhalb `maxBytes`, keine proportional größere
Reserve und keine blinde Wiederholung. Wächst der Wert zwischen Abfrage und
Lesen, wird bei einer beobachteten neuen Größe oberhalb des Limits
`CapacityError` geliefert, sonst `ReadError`; ein zwischenzeitliches
`NOT_FOUND`, eine unerwartete Längenänderung oder ein Teilwert ist `ReadError`.
Schrumpft er, wird ebenfalls kein verkürzter Wert still akzeptiert. Das
zustandsbehaftete Host-Double erzwingt alle diese Races.

## Vollständige Kapazitätsinventur

Die folgende Inventur verwendet die kanonischen Schlüssel und Maxima. Die
angegebenen Bytes sind die maximal zulässigen gespeicherten Envelope-Records,
nicht neue Adapterlimits.

| Recordgruppe | konkrete Schlüsselquelle | Anzahl | maximales Record | gepinnte NVS-Entries je Record |
|---|---|---:|---:|---:|
| User-Konfiguration | [`configuration_storage_contract.hpp`](../../lib/fermentation_app/src/configuration_storage_contract.hpp) → `uc0..uc3`; [`configuration_limits.hpp`](../../lib/fermentation_app/src/configuration_limits.hpp) `kMaximumUserConfigurationPayloadBytes + 45` | 4 | 301 B | 12 |
| Service-Konfiguration | [`configuration_storage_contract.hpp`](../../lib/fermentation_app/src/configuration_storage_contract.hpp) → `sc0..sc3`; [`configuration_graph_store.cpp`](../../lib/fermentation_app/src/configuration_graph_store.cpp) ruft das leere Servicepayload mit 45-B-Envelopegrenze auf | 4 | 45 B | 4 |
| Program-Katalog | [`configuration_storage_contract.hpp`](../../lib/fermentation_app/src/configuration_storage_contract.hpp) → `pc0..pc3`; [`configuration_limits.hpp`](../../lib/fermentation_app/src/configuration_limits.hpp) `kMaximumProgramCatalogPayloadBytes + 45` | 4 | 32.813 B | 1.036 |
| Manifest | [`configuration_storage_contract.hpp`](../../lib/fermentation_app/src/configuration_storage_contract.hpp) → `cm0..cm2`; [`configuration_limits.hpp`](../../lib/fermentation_app/src/configuration_limits.hpp) `kMaximumConfigurationManifestEnvelopeBytes` | 3 | 149 B | 7 |
| Root | [`configuration_storage_contract.hpp`](../../lib/fermentation_app/src/configuration_storage_contract.hpp) → `cr0..cr1`; [`configuration_limits.hpp`](../../lib/fermentation_app/src/configuration_limits.hpp) `kMaximumConfigurationRootEnvelopeBytes` | 2 | 114 B | 6 |
| Bootstrap | [`configuration_storage_contract.hpp`](../../lib/fermentation_app/src/configuration_storage_contract.hpp) → `cb0..cb1`; [`configuration_limits.hpp`](../../lib/fermentation_app/src/configuration_limits.hpp) `kMaximumConfigurationBootstrapEnvelopeBytes` | 2 | 42 B | 4 |
| Run-Checkpoint | [`run_persistence_store.cpp`](../../lib/fermentation_app/src/run_persistence_store.cpp) → `rc0`, `rc1`; [`run_persistence_coordinator.cpp`](../../lib/fermentation_app/src/run_persistence_coordinator.cpp) `kMaximumCheckpointRecordBytes` | 2 | 8.240 B | 262 |
| Run-Head | [`run_persistence_store.cpp`](../../lib/fermentation_app/src/run_persistence_store.cpp) → `rh0`; [`run_persistence_coordinator.cpp`](../../lib/fermentation_app/src/run_persistence_coordinator.cpp) `kMaximumHeadRecordBytes` | 1 | 256 B | 10 |

Damit gelten 22 gleichzeitig mögliche Schlüssel und:

```text
4*12 + 4*4 + 4*1036 + 3*7 + 2*6 + 2*4 + 2*262 + 1*10 = 4.783 Entries
Namespace fermentation                                      =     1 Entry
persistenter Maximalbestand                                 = 4.784 Entries
größter simultaner Austauschrecord (pc0..pc3)              = 1.036 Entries
Peak vor alter Recordentfernung                             = 5.820 Entries
zwei freie Seiten für Update-/GC-Reserve                    =   252 Entries
Mindestbudget                                               = 6.072 Entries
ceil(6.072 / 126 Entries je Seite)                          =    49 Seiten
49 * 4.096 B                                                = 196 KiB Untergrenze
```

Die Entrywerte folgen ausschließlich den gepinnten Konstanten
`NVS_CONST_ENTRY_SIZE = 32`, `NVS_CONST_ENTRY_COUNT = 126` und
`NVS_CONST_CHUNK_MAX_SIZE = 4.000`:

- ein einseitiger variabler Blob benötigt eine variable Metadaten-Entry plus
  `ceil(bytes / 32)` Payload-Entries und zusätzlich einen separaten
  `BLOB_IDX`-Entry: `2 + ceil(bytes / 32)` Entries;
- ein mehrseitiger Blob benötigt je Datenchunk Metadaten plus gerundete
  Datenentries sowie genau einen zusätzlichen `BLOB_IDX`;
- 32.813 B ergeben acht 4.000-B-Chunks, einen 813-B-Rest und einen Index:
  `8*126 + (1+ceil(813/32)) + 1 = 1.036`;
- 8.240 B ergeben `2*4.000 B + 240 B` und einschließlich des separaten
  `BLOB_IDX` `2*126 + (1+ceil(240/32)) + 1 = 262` Entries;
- ein leerer Blob benötigt den BLOB-Daten-/Indexpfad, wird aber nicht als
  Nullpointer geschrieben.

`nvs_get_stats().available_entries` zieht gemäß `nvs_pagemanager.cpp` eine
volle Seite als NVS-Reserve ab. Deshalb fordert der Nachweis zwei freie Seiten
im stabilen Vorbefüllungszustand: eine Seite für die NVS-Reserve und eine
zusätzliche Seite für den Update-/GC-Übergang. Die Capacity-Prüfung muss sowohl
den Entrybestand als auch die PageManager-Situation und die tatsächliche
Seitenpackung prüfen.

Die aktuelle 24-KB-Buildbaseline ist schon rechnerisch FAIL:

- 24.576 B sind kleiner als der einzelne maximale Konfigurationsrecord von
  32.813 B;
- bei sechs NVS-Seiten lässt `writeMultiPageBlob` höchstens fünf nutzbare
  Datenpages zu, also höchstens 20.000 B vor der weiteren Chunk-/Indexlogik;
- damit kann der maximale einzelne `ProgramCatalog`-Record dort nicht
  aufgenommen werden.

Der spätere Nachweis wird durch `scripts/issue_90_nvs_capacity.py` erzeugt und
als `docs/ISSUE_90_CAPACITY_REPORT.md` abgelegt. Das Skript importiert oder
liest die kanonischen Limits nicht als zweite Fachwahrheit: Es prüft die
Inventur gegen die Quellen, die gepinnten NVS-Konstanten, die Chunkgrenzen,
den Namespace-Entry, zwei freie Seiten und das 69-Seiten-Auswahlfenster.

## Konkreter 4-MB-Weg und Tabelle

Die aktuelle Single-App-Baseline wird durch die gepinnte
`partitions_singleapp.csv` beschrieben: `nvs` 24 KiB ab `0x9000`, `phy_init`
4 KiB ab `0xf000`, `factory` ab `0x10000`. Nach Planfreigabe und bestätigten
Annahmen wird sie projektspezifisch ersetzt durch:

- CSV: `partitions/issue_90_state_store.csv`;
- gemeinsame Produktionsdefaults: `sdkconfig.defaults`;
- Kconfig:

  ```text
  CONFIG_PARTITION_TABLE_CUSTOM=y
  CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions/issue_90_state_store.csv"
  CONFIG_NVS_BDL_STACK=n
  CONFIG_NVS_FLASH_VERIFY_ERASE=y
  CONFIG_NVS_FLASH_ERASE_ATTEMPTS=2
  ```

- `main/CMakeLists.txt` erhält keine eigene Tabellenlogik; ESP-IDF-Kconfig/
  CMake bindet die CSV ein;
- `lib/device_platform_esp_idf/CMakeLists.txt` erhält nur die notwendige
  private Laufzeitabhängigkeit `nvs_flash`.

Das vollständige 4-MB-Kandidatenlayout für die #90-Adapterentscheidung ist:

```text
0x000000–0x000FFF  Bootloader-reservierter Anfang
0x001000–0x007FFF  Bootloaderbereich gemäß Build
0x008000–0x008FFF  Partitionstabelle
0x009000–0x00EFFF  nvs,         data,nvs,    24 KiB
0x00F000–0x00FFFF  phy_init,    data,phy,     4 KiB
0x010000–0x054FFF  state_store, data,nvs,   276 KiB / 69 Seiten
0x055000–0x05FFFF  Alignment-Lücke vor App
0x060000–0x15FFFF  factory,     app,factory,   1 MiB
0x160000–0x3FFFFF  verbleibender Flash      2.625 MiB
```

Die Appgröße wird nicht geraten: beide Profile bauen nach Adapterintegration
mit `python3 scripts/build_esp_idf_profiles.py all`; der Report dokumentiert
Firmware-BIN, ELF-/Partition-Offsets, Alignment und verbleibende Reserve. Ein
Ergebnis außerhalb des 4-MB-/69-Seiten-/1-MB-App-Fensters öffnet das oben
beschriebene materielle Owner-Gate.

## Ausführbarer Hosttest: gepinnte NVS-BDL-Basis

Der primäre Testpfad ist ein ESP-IDF-v6.0.2-Linux-Hostprojekt mit der
gepinnten NVS-Hostimplementation, nicht ein IDF-unabhängiger Rückgabecode-
Mock. Der Testbaum verwendet strikt eine eigene Konfiguration:

```text
CONFIG_IDF_TARGET="linux"
CONFIG_NVS_BDL_STACK=y
CONFIG_PARTITION_TABLE_CUSTOM=y
CONFIG_PARTITION_TABLE_CUSTOM_FILENAME="partitions_nvs_host_test.csv"
```

Die Testinitialisierung ruft ausschließlich
`nvs_flash_init_partition_bdl("state_store", bdl)` auf. Das Double muss den
`esp_blockdev`-Vertrag erfüllen: `read_size=1`, `write_size=1`,
`erase_size=4096`, Partitionsgröße als 4096-Vielfaches, `0xff` nach Erase,
Read/Write/Erase/Release-Callbacks und keine Verschlüsselungsflagge. Es wird
ein zustandsbehafteter Byte-/Erase-Trace geführt; die Test-BDL-Lebensdauer
gehört dem Testbaum und endet erst nach `nvs_flash_deinit_partition`.

`CONFIG_NVS_BDL_STACK=y` und `nvs_flash_init_partition_bdl()` dürfen nicht in
den Produktionsdefaults oder in den normalen ESP32-Profilen erscheinen. Der
Produktionspfad bleibt die normale NVS-Partition mit
`nvs_flash_init_partition()`; der BDL-Hosttest prüft NVS-Storage-/Recovery-
Semantik, nicht den Standard-ESP32-Flash-/Partitionspfad. Diesen realen Pfad
belegt ausschließlich die spätere Hardwarematrix.

Vorgesehene Testdateien:

- `test/esp_idf_nvs_adapter_host/CMakeLists.txt`;
- `test/esp_idf_nvs_adapter_host/sdkconfig.defaults`;
- `test/esp_idf_nvs_adapter_host/partitions_nvs_host_test.csv`;
- `test/esp_idf_nvs_adapter_host/main/CMakeLists.txt`;
- `test/esp_idf_nvs_adapter_host/main/test_nvs_state_store_host.cpp`;
- `test/esp_idf_nvs_adapter_host/main/stateful_block_device.hpp/.cpp`;
- `test/esp_idf_nvs_adapter_host/main/nvs_api_fault_seam.hpp/.cpp`.

Die Host-CSV enthält mindestens die Testpartition
`state_store,data,nvs,0x9000,0x45000`; sie ist ein BDL-Testmedium und keine
Produktionspartitionstabelle.

Der Fault-Seam bleibt testbaumprivat: Er verwendet nur testseitige Linker-
Wrapper (`--wrap`) für `nvs_open_from_partition`, `nvs_get_blob`,
`nvs_set_blob` und `nvs_commit`, delegiert standardmäßig an die echten
ESP-IDF-Funktionen und injiziert nur ausgewählte Fehler/Races. Er ersetzt
keinen Port, definiert keine zweite NVS-Schnittstelle, besitzt keine
Produktionslebensdauer und wird nicht in `lib/device_platform_esp_idf/private/`
abgelegt. Insbesondere entsteht dort kein
`nvs_lifecycle_test_fixture.hpp` und keine Composition-Root-Logik.

Testseitige Init-/Deinit-Reihenfolge, BDL-Besitz, Fehler-Injection und
Reinitialisierung werden in `test/` orchestriert. Damit werden alle Open-,
Größen-, Read-, Set- und Commit-Fehler ausführbar geprüft, ohne das
Produktionsmodul zu einer Testabstraktion oder einem zweiten öffentlichen
Persistenzport zu machen.

Reproduzierbarer Hostlauf nach Planfreigabe:

```bash
test -n "${IDF_PATH:-}" && test -f "$IDF_PATH/export.sh"
. "$IDF_PATH/export.sh"
cd test/esp_idf_nvs_adapter_host
idf.py --preview set-target linux
idf.py build
./build/issue_90_nvs_adapter_host.elf --ci-regression
```

Der vollständige Verifikationslauf verwendet denselben Build und
`./build/issue_90_nvs_adapter_host.elf --exhaustive --seed 0`; alle erzeugten
Logs und JSON-Messartefakte werden mit Commit-SHA, IDF-SHA, Seed und Szenario
beschriftet.

## Power-Cut-, Readback-, Erase- und GC-Nachweise

### Hostmatrix und Orakel

Das BDL-Double friert bei einem konfigurierten Callback-Zähler die persistente
Byteabbildung ein und gibt für alle folgenden Operationen einen Flashfehler
zurück. Es werden keine Exceptions über die C-ABI geworfen. Der Test zerstört
anschließend Handles/Storage, initialisiert dieselben Bytes neu und führt den
Readback durch.

Die exhaustive Matrix enumeriert mindestens:

- jeden `BLOB_DATA`-Chunk des neuen Werts;
- den neuen `BLOB_IDX`;
- jeden GC-/PageManager-Copy-Write;
- die Entfernung des alten Blob-Index;
- jeden alten Datenchunk und jeden Page-Erase-Callback;
- den `nvs_commit()`-Kontrollpunkt, obwohl er in v6.0.2 keine Flashmutation
  ausführt.

Die Traceklassifikation verwendet Callbackposition, Adresse/Länge und den
gepinnten NVS-Seiten-/Entryinhalt; sie behauptet keine interne Phase, die nicht
aus dem Artefakt ableitbar ist.

Pflichtszenarien sind: absent→empty, empty→binary, kleiner→größer,
32/33-B-Grenze, 4.000/4.001-B-Chunkgrenze, maximaler 32.813-B-Programmkatalog,
8.240-B-Checkpoint, bestehender maximaler Wert→neuer maximaler Wert und ein
vorbefüllter GC-Fall. Drei feste Bytepatterns (`00`, `a5`, `ff`) und drei
Wiederholungsdurchläufe werden im Vollauf verwendet.

Nach jedem Unterbruch gilt ausschließlich:

- vollständiger alter Wert; oder
- vollständiger neuer Wert; bei vorher absent zusätzlich `NotFound`.

Teilwerte, Mischwerte, fremde Länge, CRC-/Envelope-Fehler, ein nicht
reinitialisierbarer Storage oder ein nicht eindeutig lesbarer Zustand sind
FAIL. Ein `CommitOutcomeUnknown` wird nie als unverändert ausgegeben; der
Readback löst den tatsächlichen alten/neuen Stand auf. Bei unauflösbarem
Readback bleibt die Anwendung fail-closed.

Jeder Fall protokolliert mindestens Scenario, Seed, Pattern, Callback-/
Operationnummer, abgeleitete Mutationsphase, Setstatus, Commitstatus,
Reinitialisierungsstatus, Readstatus, Länge und SHA-256 von erwartetem alt,
erwartetem neuem und gelesenem Wert.

### Reproduzierbarer GC-/Erase-Fall

Der Hosttest führt zuerst die 22 Schlüssel mit allen oben genannten Maxima in
die 69-Seiten-Testpartition ein. Danach rotiert er in fester Reihenfolge
`pc0`, `pc1`, `rc0`, `rc1`, `rh0` mit neuen, byteverschiedenen Maximalrecords,
bis der erste Page-Erase-Callback beobachtet wird, höchstens 2.048
Schreiboperationen. Kein beobachteter Erase innerhalb dieser Grenze ist FAIL.
Der Fall wird anschließend an jedem Copy-/Erase-Callback unterbrochen und
reinitialisiert. Erwartet werden `ESP_OK` bei der Reinitialisierung sowie das
vollständige Alt-oder-Neu-Orakel für jeden betroffenen Schlüssel; jeder andere
Ausgang ist FAIL beziehungsweise bei unauflösbarem Zustand fail-closed.

Damit basiert der Power-Cut-Nachweis nicht nur auf einer frischen Partition
oder einem einfachen Blobwrite. Die Produktionsoption
`CONFIG_NVS_FLASH_VERIFY_ERASE=y` und exakt zwei Erase-Versuche wird aus der
gepinnten Kconfig/`nvs_partition.cpp` übernommen. Die Verifikation liest bei
der normalen ESP-Partition den gelöschten Bereich zurück und versucht einen
Fehler höchstens zweimal; das ist eine begrenzte Hersteller-/Konfigurations-
entscheidung, kein automatisches Löschen, Formatieren oder Init-Retry. Der
BDL-Zweig ruft dagegen direkt den BDL-Erase-Callback auf und testet deshalb
nicht die Standard-Flash-Erase-Verifikation.

### Schmaler On-Target-Harness für den realen ESP32-Pfad

Der reale Nachweis benötigt neben dem Runner eine testseitige Firmwareseite.
Nach Planfreigabe werden dafür ausschließlich im bestehenden
`esp32_bringup`-Profil folgende Pfade ergänzt:

- `main/issue_90_nvs_hardware_verification.hpp` und
  `main/issue_90_nvs_hardware_verification.cpp` enthalten den schmalen
  UART-Harness, die deterministische Arbeitslast, die Ressourcen-/NVS-
  Statusausgabe und die testseitige Raw-Page-Evidenz;
- `main/CMakeLists.txt` nimmt diese Quellen nur unter
  `CONFIG_APP_PROFILE_ESP32_BRINGUP` auf und setzt ausschließlich dort
  `APP_ISSUE_90_NVS_HARDWARE_TEST=1`;
- `main/app_main.cpp` ruft den Harness nur unter dieser Compile-Time-Guard
  auf und startet in diesem Profilpfad danach keine normale
  Anwendungsschleife. Das Releaseprofil erhält weder Quelle noch Definition;
  ein Profilisolationscheck prüft das zusätzlich in beiden
  `compile_commands.json` und im Release-ELF auf fehlende `ISSUE90`-Symbole /
  Marker.

Damit folgt #90 dem vorhandenen `esp32_bringup`-/Compile-Time-Isolationsmuster
aus `main/CMakeLists.txt` und `app_main.cpp`. Es entsteht keine zweite
Wegwerf-Anwendung, kein Testfixture unter
`lib/device_platform_esp_idf/private/` und kein öffentlicher Persistenzport.
Der Harness verwendet im Testbaum den echten `NvsStateStore` über den
unveränderten `IStateStore`-Vertrag.

Solange noch kein produktiver `IStateStore`-Verbraucher existiert, besitzt
dieser Harness als Test-Orchestrator den Partitionslebenszyklus: Er ruft
`nvs_flash_init_partition("state_store")` über den normalen ESP-IDF-
Flashpfad auf, prüft strikt `ESP_OK`, konstruiert erst danach den echten
`NvsStateStore` und zerstört bei normalem Ende zuerst den Store, dann den
Handle-/Operationskontext und zuletzt `nvs_flash_deinit_partition`.
`NvsStateStore` selbst erhält keine Init-/Deinit-Methode. Nach einem
Power-Cut gibt es kein Deinit; der nächste Boot initialisiert die Partition
erneut. Jede Initialisierungsabweichung ist fail-closed und führt weder zu
Erase, Formatierung, Retry noch zur normalen Anwendungs-/Aktor-Schleife.

#### Deterministisches UART-Protokoll

Der Runner
`scripts/issue_90_nvs_hardware_verification.py` spricht ausschließlich ein
versioniertes, zeilenorientiertes Protokoll. Unerwartete, fehlende oder nicht
parsebare Marker sind `FAIL`/`NOT_RUN`, niemals ein impliziter Erfolg.

Der Harness akzeptiert genau diese Befehle:

```text
PREFILL seed=0
ROTATE max_writes=2048
READBACK_ALL
REBOOT
STOP
```

Für einen externen Power-Cut wird zusätzlich `CUT_ARM token=<deterministic-token>`
akzeptiert. Der Harness antwortet mit mindestens diesen vollständigen Markern
(Statuscodes sind ESP-IDF-Codes in Hex):

```text
ISSUE90 READY protocol=1 idf=7101770dc6db2667b3c477cc31365dd1acd6db4e profile=esp32_bringup partition=state_store pages=69
ISSUE90 PREFILL_DONE keys=22
ISSUE90 CUT_ARMED token=<token>
ISSUE90 ROTATE_BEGIN seq=<n> key=<key> old_sha256=<sha> new_sha256=<sha>
ISSUE90 ROTATE_RESULT seq=<n> set=<esp_err> commit=<esp_err>
ISSUE90 GC_ERASE_DETECTED page=<n> old_seq=<n> new_seq=<n> evidence_sha256=<sha>
ISSUE90 READBACK key=<key> status=<status> len=<n> sha256=<sha>
ISSUE90 STATS used_entries=<n> free_entries=<n> available_entries=<n> total_entries=<n>
ISSUE90 RESOURCE stage=<name> free_heap=<n> largest_block=<n> stack_hwm=<n>
ISSUE90 REBOOTING
ISSUE90 PASS reason=<reason>
ISSUE90 FAIL reason=<reason>
```

`PREFILL` schreibt deterministisch die vollständige Inventur aus 19
Konfigurationsschlüsseln (`uc0..uc3`, `sc0..sc3`, `pc0..pc3`, `cm0..cm2`,
`cr0..cr1`, `cb0..cb1`) und drei Lauf-/Checkpointschlüsseln (`rc0`, `rc1`,
`rh0`), jeweils mit dem berechneten Maximalrecord und festem Seed. `ROTATE`
verwendet danach die feste Sequenz `pc0`, `pc1`, `rc0`, `rc1`, `rh0` mit
byteverschiedenen Maximalrecords bis zum ersten nachgewiesenen GC/Erase oder
höchstens 2.048 Schreiboperationen. Jede Operation ruft den echten Adapter
auf und meldet Set-/Commitstatus, `nvs_get_stats()` und Ressourcenstände an
den Runner. `READBACK_ALL` liefert für alle 22 Records exakte Länge und
SHA-256; der Runner vergleicht ausschließlich vollständige alte oder neue
Bytes. `REBOOT` veranlasst einen echten `esp_restart()`, worauf ein neuer
`READY`-Marker und derselbe vollständige Readback folgen.

Die Partition- und NVS-Statusmeldung kommt testseitig aus
`esp_partition_find_first()`/`esp_partition_get()` und `nvs_get_stats()` und
enthält Label, Adresse, Größe, Typ/Subtyp, Eintragsstatistik sowie die
Messpunkte `startup`, `prefill`, `rotation`, `readback` und `post-reboot`.
Ressourcen werden mit den bestehenden ESP-IDF-Statusquellen für freien Heap,
größten freien 8-Bit-Block und Stack-High-Water-Mark erhoben. Diese
Testausgaben sind Artefakte, kein neuer produktiver API-Vertrag.

#### Nachweis eines tatsächlichen Page-GC/Erase

Der Harness liest ausschließlich testseitig die 69-Seiten-Partition über
`esp_partition_read()` in 4-KiB-Seitensnapshots und dekodiert die gepinnte
NVS-Seiten-/Entry-Struktur anhand von `nvs_constants.h`. Für jeden
Rotationsschritt werden Vorher-/Nachher-Snapshot, Seite, Sequenznummer,
Seitenstatus, belegte Entries und SHA-256 des 4-KiB-Inhalts archiviert.

`GC_ERASE_DETECTED` darf nur ausgegeben werden, wenn alle drei Bedingungen
gemeinsam erfüllt sind: (a) eine zuvor gültige, nichtleere NVS-Seite mit
Sequenznummer und Einträgen ist nach genau diesem `nvs_set_blob()`-Schritt
vollständig `0xff`/gelöscht, (b) eine andere Seite besitzt danach die erwartete
neue Sequenz-/Belegungsstruktur und mindestens die kopierten lebenden Records,
und (c) `nvs_get_stats()`, der vollständige Readback und die gespeicherten
Vorher-/Nachher-Hashes sind konsistent. Ein leerer Vorrat, eine bloße
Statistikänderung oder ein erwarteter Rotationszähler ist kein GC-/Erase-
Nachweis. Der reale Standard-Flashpfad wird damit über auslesbare
Partitions-/Page-Evidenz und nicht über einen erfundenen produktiven Hook
belegt.

Der Harness speichert bei `GC_ERASE_DETECTED` und nach jeder Reinitialisierung
die Rohsnapshots, Statuszeilen, Resetursache, A/B-Erwartungen und Hashes unter
`build/issue_90_hardware_verification/`. Ein Power-Cut während einer
`ROTATE_BEGIN`-bis-`ROTATE_RESULT`-Operation muss nach dem nächsten Boot
entweder den vollständigen alten oder den vollständigen neuen Record liefern;
bei einem Cut während GC/Erase wird zusätzlich die oben definierte Recovery-
Evidenz erwartet. Ein nicht lesbarer, teilweiser oder nicht eindeutig
zuordenbarer Zustand ist FAIL und führt zu fail-closed.

### Reale ESP32-Matrix

Nach dem offenen #29-Hardwaregate führt `scripts/issue_90_nvs_hardware_verification.py`
über den beschriebenen On-Target-Harness denselben vorbefüllten 69-Seiten-
Workload über die normale ESP32-Partition aus. Die Matrix umfasst:

1. saubere Vorbefüllung und Readback aller 22 Schlüssel;
2. die deterministische Rotationsfolge bis zum nachweislichen Page-GC/Erase;
3. Power-Cut-Fenster über die kalibrierte Dauer der Blob-/GC-/Erasefolgen;
4. mindestens zehn Wiederholungen je Fenster und drei saubere Neustart-
   Kontrollen je Szenario;
5. Reset-/Bootlogs, reale Partitionserfassung, `nvs_get_stats`, Set-/Commit-
   Status und A/B-SHA-256-Readbacks.

Ein externer Cut-Aufbau wird nur mit ownerverifizierter Versorgung, Reset-
und Steuerleitung verwendet; keine GPIO-, Pegel- oder Zeitannahme wird im
Plan erfunden. Der Runner erhält dafür über `--power-cut-hook
"${POWER_CUT_HOOK}"` ein vorbereitetes, vom Repository-Root aus
repository-relativ aufrufbares Testwerkzeug (beispielsweise
`./scripts/issue_90_power_cut_hook.py`; absolute Maschinenpfade werden
abgelehnt). Dieses Werkzeug akzeptiert auf stdin `ARM token=<token>`,
`TRIP` und `RESTORE` und muss jeweils exakt `ARMED`, `TRIPPED` und `RESTORED`
bestätigen. Der Runner sendet `ARM` vor `CUT_ARM`, wartet auf den zugehörigen
`ROTATE_BEGIN`-Marker, löst `TRIP` aus, erwartet UART-Verlust vor
`ROTATE_RESULT`, stellt mit `RESTORE` die Versorgung wieder her und wartet
auf den neuen `READY`-Marker. Der vollständige Cut-/Reboot-/Readbackdatensatz
enthält Token, Marker, Hookantworten und Zeitstempel. Fehlt eine
ownerverifizierte Hook-/Versorgungsbindung, ist der Fall BLOCKED/NOT_RUN.

Der Hosttest belegt interne Storage-/Recovery-Semantik und die vollständige
Mutationsphasenmatrix; der ESP32-Test belegt den normalen
Flash-/Partition-/Erasepfad einschließlich echter Page-Evidenz. Ein
Init-/Readback-Fehler, eine Teil-/Mischversion, fehlender GC-/Erase-Nachweis
oder ein unsicherer Hardwareaufbau ist BLOCKED/FAIL und niemals PASS.

Der reproduzierbare Ablauf nach Freigabe der Hardwarefixture lautet:

```bash
test -n "${IDF_PATH:-}" && test -f "$IDF_PATH/export.sh"
. "$IDF_PATH/export.sh"
test -n "${ESP_PORT:-}"
test -n "${POWER_CUT_HOOK:-}"
python3 scripts/build_esp_idf_profiles.py all
python3 scripts/issue_90_nvs_hardware_verification.py \
  --port "$ESP_PORT" --profile esp32_bringup --scenario prefilled_gc \
  --repetitions 10 --power-cut-hook "$POWER_CUT_HOOK" \
  --artifact-dir build/issue_90_hardware_verification
```

Das neue Runner-Skript setzt den aktorfreien Testmodus, wartet auf die
bekannte Testsequenz, koordiniert die ownerverifizierte Cut-Steuerung und
schreibt nur die oben definierten Logs/Hashes/Reset-/Partitions-/NVS-Stats.
Ein manueller `idf.py monitor`-Lauf allein ist kein Power-Cut- oder Readback-
Nachweis.

## Qualitätsgates und CI-Regressionsschutz

Der neue Hosttest wird nach der Umsetzung verbindlich in die kanonischen
Definitionen aufgenommen:

- [`docs/CI_AND_QUALITY_GATES.md`](../../docs/CI_AND_QUALITY_GATES.md) erhält
  den ESP-IDF-Linux-BDL-Hosttest, die genaue `$IDF_PATH`-Aktivierung, die
  Statusbegriffe und die zwei Reproduktionsläufe;
- [`.github/workflows/build.yml`](../../.github/workflows/build.yml) erhält
  nach ESP-IDF-Installation/`export.sh` einen Schritt, der im Repositorypfad
  `test/esp_idf_nvs_adapter_host` `idf.py --preview set-target linux`,
  `idf.py build` und `./build/issue_90_nvs_adapter_host.elf
  --ci-regression` ausführt;
- Hostlog, JSON-Matrix und IDF-/Source-SHA werden als getrennte Artefakte
  gesichert und durch Secret-/Pfadscan abgedeckt; erforderliche Anpassungen an
  `scripts/check_ci_artifact_scan_coverage.py` werden im Implementierungsschnitt
  mitgeführt.

Der verbindliche CI-Regressionssatz ist deterministisch und umfasst die
Grenzgrößen, empty/binary, Read-Race, Open-/Size-/Read-/Set-/Commit-Fehler,
einen maximalen Programmkatalog und einen vorbefüllten GC-Erase-Fall mit
festem Seed. Die exhaustive Matrix ist für jeden CI-Lauf nicht erforderlich;
sie bleibt aber als reproduzierbarer Owner-/Hardware-Verifikationslauf
`--exhaustive --seed 0` verpflichtend vor dem Hardwaregate. Ein fehlender
Hostlauf ist `BLOCKED`/`NOT_RUN`, nicht PASS.

## Umsetzungs- und Commit-Schnitte nach Planfreigabe

Die Umsetzung bleibt in diesen nachweisbaren Schnitten. Der aktuelle Plan-
Commit enthält keinen dieser Produktions- oder Testpfade.

1. **Adapterkern und Abhängigkeit**
   - `nvs_state_store.hpp/.cpp`, direkte Key-/Namespace-Abbildung,
     per-operation Handle, zweistufiger Read, vollständige Statusmatrix;
   - `lib/device_platform_esp_idf/CMakeLists.txt` mit `PRIV_REQUIRES nvs_flash`;
   - Nachweis: portabler Fachkern ohne ESP-IDF-Leak, gezielter Komponentenbuild,
     Format-/Diffprüfung.

2. **Testbaum und BDL-Seam**
   - Hostprojekt, `esp_blockdev`-Double, testseitige Linker-Wrappers und
     stateful A/B-/Race-/Error-Tests;
   - kein `private/nvs_lifecycle_test_fixture.hpp`, kein Produktionsfixture,
     kein zweiter öffentlicher Port;
   - Nachweis: `--ci-regression` PASS und vollständiger Hostlauf als eigener
     Verifikationsartefakt.

3. **On-Target-Hardwaretest und Profilisolation**
   - `main/issue_90_nvs_hardware_verification.hpp/.cpp`, die Guard-
     Einbindung in `main/app_main.cpp` und die
     `CONFIG_APP_PROFILE_ESP32_BRINGUP`-Erweiterung in `main/CMakeLists.txt`;
   - `scripts/issue_90_nvs_hardware_verification.py` sowie der schmale
     testseitige `scripts/issue_90_power_cut_hook.py` mit dem versionierten
     UART-/Hook-Protokoll und den repository-relativen Artefaktpfaden;
   - Nachweis: echter `NvsStateStore`, 22-Schlüssel-Vorbefüllung,
     deterministische Rotation, Neustart, A/B-Hash-Readback, NVS-/Ressourcen-
     Marker und Raw-Page-Beweis für tatsächlichen GC/Erase; Release enthält
     weder Harness-Quelle noch `ISSUE90`-Marker.

4. **Lifecycle-/Composition-Root-Gate**
   - Testbaum beweist Init-/Deinit-/BDL-Besitz und Zerstörungsreihenfolge;
   - der produktive Pfad in `main/app_main.cpp` bleibt unverändert, solange
     kein produktiver `IStateStore`-Verbraucher existiert; die ausschließlich
     testseitige Guard-/Harness-Einbindung ist Schnitt 3;
   - bei späterer echter Verbraucherbindung: Root-Init/Storekonstruktion/
     Verbraucher-/Store-/Deinit-Reihenfolge gemeinsam im betroffenen Root;
   - Nachweis: Initfehler verhindert Konstruktion und Runtime; kein Erase/
     Format/Retry.

5. **Kapazität und Partition**
   - `scripts/issue_90_nvs_capacity.py`,
     `docs/ISSUE_90_CAPACITY_REPORT.md`,
     `partitions/issue_90_state_store.csv`, `sdkconfig.defaults`;
   - 49-Seiten-Untergrenze, 69-Seiten-Auswahl, maximaler Einzelrecord,
     zwei freie Seiten, GC-/Erase-Workload, Appgröße und Flashreserve;
   - Nachweis: bei bestätigten Annahmen direkt umsetzbar; bei Abweichung
     materielles Owner-Gate mit neuer Plan-SHA.

6. **Software-/CI-Integration**
   - `docs/CI_AND_QUALITY_GATES.md`, `.github/workflows/build.yml` und bei
     Artefaktbedarf `scripts/check_ci_artifact_scan_coverage.py`;
   - gezielte Befehle:

     ```bash
     . "$IDF_PATH/export.sh"
     python3 scripts/build_esp_idf_profiles.py all
     python3 scripts/run_esp_idf_static_analysis.py all
     python3 scripts/build_report.py --output build-report.md --append \
       --esp-idf-profiles bringup release --source-git-sha "$(git rev-parse HEAD)"
     python3 scripts/check_architecture_boundaries.py
     python3 scripts/check_secrets.py
     python3 scripts/selftest_quality_gates.py
     git diff --check
     ```

   - Nachweis: beide Profile, Static Analysis, Architektur, Secrets, Gate-
     Selbsttests und Host-CI-Regressionssatz mit PASS/BLOCKED/NOT_RUN.

7. **Hardware-/Power-Cut-Verifikation**
   - ownerverifizierte normale ESP32-Partition, UART-/Reset-/Power-Cut-
     Fixture, den On-Target-Harness aus Schnitt 3,
     `scripts/issue_90_nvs_hardware_verification.py`, vorbefüllten GC-Fall
     und A/B-Orakel;
   - Nachweis: vollständige Matrix, Logs, Partitionsdump, Resetursachen,
     Readbacks, Raw-Page-Evidenz und reale Resource-/Erase-Artefakte. Kein
     Build ersetzt diesen Nachweis.

8. **Ressourcen, Schreiblast und Wear-Grenze**
   - `docs/ISSUE_90_BUILD_REPORT.md` und
     `docs/ISSUE_90_HARDWARE_VERIFICATION.md`;
   - Flash-/Appgröße, DRAM, Heap, größter Block, Stack-HWM, NVS-Stats,
     Initzeit, Set-/Read-Latenz und Schreiblastbudget;
   - repräsentativer Test mit 10.000 deterministischen Rotationen oder dem
     dokumentierten kleineren Testfenster, falls die Hardwaregrenze vorher
     fail-closed greift;
   - Herstellervertrag aus gepinnter NVS-Implementierung, berechnetes
     Schreiblastbudget, NVS-Statistiken und Belastungstest werden getrennt
     ausgewiesen. Es wird kein unbegrenzter Wear-Leveling-/Lebensdauernachweis
     behauptet.

9. **Lizenz und Herkunft**
   - `docs/THIRD_PARTY_COMPONENTS.md`,
     `docs/audits/THIRD_PARTY_SOURCE_AND_LICENSE_REVIEW.md`,
     `docs/audits/COMPONENT_EVALUATIONS.md` und bei Bedarf
     `docs/ADOPT_OR_BUILD.md` aktualisieren;
   - verwendeter Dateisatz, Herkunft, exakter Commit, Apache-2.0-Lizenz,
     Host-BDL-Referenz und Repository-Notice dokumentieren;
   - `docs/LICENSE_STATUS.md` bleibt unverändert, sofern die Prüfung keine
     konkrete Projektlizenzabweichung findet.

## Ressourcen-, Wear- und Securitygrenze

Das tatsächliche Schreiblastbudget wird aus den kanonischen Aufrufern
abgeleitet: Konfigurationsmutationen, Manifest-/Rootfolge und die in
`RUN_PERSISTENCE.md`/`run_persistence_coordinator.cpp` festgelegten
Checkpoint-/Head-Schreibfolgen. Es werden keine Einzelmessungen aus dem
Zwei-Sekunden-Zyklus, keine unbounded Historie und keine OTA-/PSRAM-Reserve
hinzugefügt.

Der Herstellervertrag ist begrenzt: ESP-IDF NVS bietet die gepinnte
Seiten-/Entry-/GC-/Erase-Mechanik. Der #90-Nachweis ergänzt ihn um die
berechnete konkrete Schreiblast, `nvs_get_stats()` vor/während/nach dem
Belastungsfenster und den repräsentativen 10.000-Rotationen-Test. Die
Nachweisgrenze lautet ausdrücklich: beobachtetes Verhalten in diesem
Workload-/Hardwarefenster, keine garantierte Lebensdauer über alle Geräte-
temperaturen, Flashchargen oder unbeschränkte Laufzeiten.

NVS-Verschlüsselung, Flashverschlüsselung, `nvs_keys` und Secret-at-rest-
Schutz werden in #90 weder aktiviert noch behauptet. Das separate
Security-/Releasegate bleibt offen.

## Gepinnte ESP-IDF-Herstellerquellen

Alle Herstellerverträge verwenden ausschließlich
`ESP-IDF v6.0.2 @ 7101770dc6db2667b3c477cc31365dd1acd6db4e`, niemals `stable`
oder `master`:

- [`nvs.h`](https://github.com/espressif/esp-idf/blob/7101770dc6db2667b3c477cc31365dd1acd6db4e/components/nvs_flash/include/nvs.h)
- [`nvs_flash.h`](https://github.com/espressif/esp-idf/blob/7101770dc6db2667b3c477cc31365dd1acd6db4e/components/nvs_flash/include/nvs_flash.h)
- [`esp_partition.h`](https://github.com/espressif/esp-idf/blob/7101770dc6db2667b3c477cc31365dd1acd6db4e/components/esp_partition/include/esp_partition.h)
- [`nvs_api.cpp`](https://github.com/espressif/esp-idf/blob/7101770dc6db2667b3c477cc31365dd1acd6db4e/components/nvs_flash/src/nvs_api.cpp)
- [`nvs_handle_simple.cpp`](https://github.com/espressif/esp-idf/blob/7101770dc6db2667b3c477cc31365dd1acd6db4e/components/nvs_flash/src/nvs_handle_simple.cpp)
- [`nvs_storage.cpp`](https://github.com/espressif/esp-idf/blob/7101770dc6db2667b3c477cc31365dd1acd6db4e/components/nvs_flash/src/nvs_storage.cpp)
- [`nvs_constants.h`](https://github.com/espressif/esp-idf/blob/7101770dc6db2667b3c477cc31365dd1acd6db4e/components/nvs_flash/private_include/nvs_constants.h)
- [`nvs_pagemanager.cpp`](https://github.com/espressif/esp-idf/blob/7101770dc6db2667b3c477cc31365dd1acd6db4e/components/nvs_flash/src/nvs_pagemanager.cpp)
- [`nvs_partition.cpp`](https://github.com/espressif/esp-idf/blob/7101770dc6db2667b3c477cc31365dd1acd6db4e/components/nvs_flash/src/nvs_partition.cpp)
- [`nvs_page.cpp`](https://github.com/espressif/esp-idf/blob/7101770dc6db2667b3c477cc31365dd1acd6db4e/components/nvs_flash/src/nvs_page.cpp)
- [`Kconfig`](https://github.com/espressif/esp-idf/blob/7101770dc6db2667b3c477cc31365dd1acd6db4e/components/nvs_flash/Kconfig)
- [`esp_blockdev.h`](https://github.com/espressif/esp-idf/blob/7101770dc6db2667b3c477cc31365dd1acd6db4e/components/esp_blockdev/include/esp_blockdev.h)
- [`bdl_ramdisk.hpp`](https://github.com/espressif/esp-idf/blob/7101770dc6db2667b3c477cc31365dd1acd6db4e/components/nvs_flash/host_test/nvs_host_test/main/bdl_ramdisk.hpp)
- [`bdl_ramdisk.cpp`](https://github.com/espressif/esp-idf/blob/7101770dc6db2667b3c477cc31365dd1acd6db4e/components/nvs_flash/host_test/nvs_host_test/main/bdl_ramdisk.cpp)
- [`nvs_host_test/sdkconfig.ci.esp_blockdev`](https://github.com/espressif/esp-idf/blob/7101770dc6db2667b3c477cc31365dd1acd6db4e/components/nvs_flash/host_test/nvs_host_test/sdkconfig.ci.esp_blockdev)
- [`partitions_singleapp.csv`](https://github.com/espressif/esp-idf/blob/7101770dc6db2667b3c477cc31365dd1acd6db4e/components/partition_table/partitions_singleapp.csv)
- [`esp_wifi.h`](https://github.com/espressif/esp-idf/blob/7101770dc6db2667b3c477cc31365dd1acd6db4e/components/esp_wifi/include/esp_wifi.h)
- [`esp_wifi_types_generic.h`](https://github.com/espressif/esp-idf/blob/7101770dc6db2667b3c477cc31365dd1acd6db4e/components/esp_wifi/include/esp_wifi_types_generic.h)
- [`esp_wifi/Kconfig`](https://github.com/espressif/esp-idf/blob/7101770dc6db2667b3c477cc31365dd1acd6db4e/components/esp_wifi/Kconfig)

## Roadmap, Draft-PR und Owner-Gate

Die Roadmap wird nur so weit geändert, dass #90 die parallele Software-/Host-
phase und das weiterhin offene #29-Standard-Flash-/Hardwaregate korrekt zeigt.
Anforderungen und die vollständige Kapazitätsrechnung bleiben in diesem Plan.

Der Draft-PR führt nach dem Plancommit exakt aus:

```text
Plan: docs/tasks/issue-90-esp-idf-nvs-adapter-plan.md
Plan commit: <exakte SHA dieser Planrevision>
Base branch: agent/issue-29-esp32-bringup-plan
Base SHA: c4c8b33f4dbaef727200ea410d887ec5417aa1b0
Dependency: STACKED_ON_PR_116; PR #116 Draft; Issue #29 offen und Hardware-/Standard-Flash-Gates offen
Implementation: IMPLEMENTATION_NOT_STARTED_PENDING_PLAN_APPROVAL
Proposed partition decision: `state_store = 69 pages / 276 KiB`; becomes
binding with Owner approval of this exact plan SHA and remains usable without
a second Owner gate only while the explicit assumptions above hold
```

Der PR bleibt Draft. Es gibt kein Ready for review, keinen Merge, kein
Auto-Merge, kein Issueschließen, kein Branchlöschen und keinen Force-Push.
Nach Veröffentlichung wird genau ein aktueller `SESSION HANDOVER` geführt.

## Retarget nach PR #116

Nach dem Merge von PR #116 wird live verifiziert, dass dessen identische
Commits in `main` enthalten sind. Ein neuer Branchverweis oder ein reines
Retarget des unveränderten #90-Branches auf `main` verändert weder Commit-SHA
noch Plan-SHA und benötigt keine neue Freigabe. Eine neue Plan-SHA entsteht
erst durch neu erzeugte Commits, etwa Rebase, Cherry-Pick, Branch-Neuerzeugung
mit neuen Commits oder materielle Plan-/Grundlagenänderung; dann ist vor
Umsetzung erneut die exakte neue Plan-SHA freizugeben.
