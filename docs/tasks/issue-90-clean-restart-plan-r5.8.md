# Issue #90 – sauberer Neustart auf aktueller PR-116-Basis (R5.8)

## Status, Ziel und Owner-Gate

Diese Datei ist die vollständige, eigenständig ausführbare kanonische
Planrevision R5.8 für den sauberen Neustart von Issue #90. Sie ist kein
Nachfolger-Commit im historischen PR #117 und enthält keine Umsetzung.

Bis zur ausdrücklichen Freigabe der exakten Commit-SHA dieser Datei gilt:

`CLEAN_RESTART_PLAN_PENDING_R5_8_OWNER_APPROVAL`

Die Freigabe dieser Plan-SHA autorisiert noch keinen Umsetzungsslice. Jeder
spätere Slice erhält zusätzlich ein eigenes ausdrückliches Owner-Gate. Ohne
die jeweilige Freigabe bleiben Produktionscode, Tests, Orakel, Runner,
Harness, CI, Backend, Partition und Hardwareverifikation `NOT_RUN`.

### Verifizierte Live-Basis

Die Live-Prüfung vor dieser Revision ergab:

| Quelle | Live-Stand | Bedeutung für diesen Neustart |
|---|---|---|
| PR #116 | OPEN, Draft; Base `main @ 87dd593fcdc8d26831873a4163b174340b4347c0`; aktueller Head `30fa0a8264e2c4564d324340c6bebc204147f477` | PR #116 ist nicht in `main` enthalten. Sein aktueller ownerreviewter Head ist die neue Arbeitsbasis. |
| PR #117 | OPEN, Draft; Base `agent/issue-29-esp32-bringup-plan @ 30fa0a8264e2c4564d324340c6bebc204147f477`; aktueller Head `5cc1fcdc7fe7014192592e188e1c5c331a7f507e` | Historische, nicht weiterzuverwendende Umsetzung. Unverändert lassen; nicht mergen, rebasen, force-pushen oder schließen. |
| Issue #90 | OPEN, Titel `[E5.7] ESP-IDF-NVS-Adapter fuer IStateStore implementieren und verifizieren` | Bleibt offen. Die alte Umsetzung ist keine neue Codebasis. |
| Roadmap | Stand `2026-08-18` vor dieser Revision; #29 bleibt offen und blockiert die weitere reale Basis | Nur die notwendige neue #90-Planstatuszeile wird synchronisiert. Anforderungen werden nicht in die Roadmap kopiert. |

Die Arbeitsbaseline dieses Plans ist damit:

```text
CONTEXT_BASELINE_BRANCH: agent/issue-29-esp32-bringup-plan
CONTEXT_BASELINE_SHA: 30fa0a8264e2c4564d324340c6bebc204147f477
CONTEXT_HEAD_SHA: 30fa0a8264e2c4564d324340c6bebc204147f477
CONTEXT_PLAN_SHA: Commit-SHA der vorliegenden Datei; die exakte SHA steht im
  neuen PR-Body und im aktuellen SESSION HANDOVER
CONTEXT_REFRESH_MODE: FULL
CONTEXT_DELTA: Live-PR/Issue/Branch-Abgleich, Roadmap, Root- und lokale AGENTS.md,
  bindende ADR-/Recovery-/Safety-/CI-Quellen, Baselineinventur auf PR-116-HEAD
SOURCE_OF_TRUTH_CONFLICT: NONE für die Arbeitsbasis; R5.7 bleibt historische
  fachliche Quelle und ist kein Bestandteil der neuen Branchbasis.
```

### Historische Provenienz

R5.7 bleibt die ownerfreigegebene historische Produkt-Recovery-Revision aus
PR #117:

`docs/tasks/issue-90-product-recovery-replan-r5.7.md @ 4b4053bc229f279f5b30fc16ed60c765d1ee151e`

Der aktuelle #117-Head ist `5cc1fcdc7fe7014192592e188e1c5c331a7f507e`.
Die R5.7-Produktentscheidungen werden in dieser neuen Revision fachlich
übernommen und auf der verifizierten sauberen Baseline erneut abgeleitet.
Historische Commit-SHAs werden nicht als neue Basis, neue Umsetzung oder neue
Plan-SHA ausgegeben.

R5.4 (`c35ce0342898f0e19d3cce5e6a7eaa077f73bad6`), R5.5
(`1dfa7eb390ededec4dc58efee7138d3fffdd39ff`) und R5.6
(`a84ae061abc7199d8285cd142c139cd829cb66e9`) bleiben nachvollziehbare
historische Revisionen. Keine davon wird mit dieser neuen Datei
zusammengesetzt oder als neue Planwahrheit vorausgesetzt.

## Scope dieser Plan-only-Runde

Im neuen Draft-PR sind ausschließlich diese Dokumentationsänderungen
zulässig:

1. diese vollständige kanonische Planrevision unter `docs/tasks/`;
2. die minimale `docs/ROADMAP.md`-Synchronisierung, die den neuen Plan-PR und
   das Owner-Gate sichtbar macht;
3. der PR-Body und genau ein aktueller `SESSION HANDOVER` als dynamische
   GitHub-Metadaten.

Nicht Bestandteil dieser Runde sind Produktionscode, öffentliche API- oder
Statusänderungen, Konfigurations-/Run-Schemaänderungen, Testcode,
Fault-Injection, Host-BDL, Oracle, Runner, Harness, Workflow, Dependency,
ESP-IDF-, Backend-, Partition- oder Hardwareänderungen. Es werden keine Tests
ausgeführt, weil der neue PR bis zur Planfreigabe ausschließlich ein
Plan-/Restart-PR ist.

PR #117 und seine Commits bleiben als historische Evidenz unverändert. Keine
alte #117-Datei wird kopiert, cherry-picked oder in-place korrigiert.

## Verbindliche Quellen und Architekturgrenzen

Die spätere Umsetzung verwendet die vorhandenen Quellen und Verträge in dieser
Reihenfolge:

- `AGENTS.md` und die lokalen Regeln unter `lib/device_platform/`,
  `lib/device_platform_esp_idf/`, `lib/fermentation_app/` und
  `lib/device_platform_test_support/`;
- `docs/AGENT_WORKFLOW.md` und `docs/ENGINEERING_PRINCIPLES.md`;
- `docs/SPECIFICATION_REVIEW.md`, `docs/DECISIONS.md`, ADR-013 und ADR-016;
- `docs/CONFIGURATION_PERSISTENCE.md`, `docs/RUN_PERSISTENCE.md`,
  `docs/SYSTEM_SAFETY_AND_RECOVERY.md`, `docs/SAFETY_AND_FAULTS.md`,
  `docs/SETTINGS_AND_STORAGE.md` und `docs/ACCEPTANCE_TESTS.md`;
- `docs/CI_AND_QUALITY_GATES.md` für die tatsächlich zulässigen Befehle und
  Gatedefinitionen;
- die bestehenden Ports, Recoverykomponenten, Tests und Testhilfen aus der
  Baselineinventur unten.

ADR-013 bleibt verbindlich:

- `device_platform` bleibt portabel und anwendungsneutral;
- `device_platform_esp_idf` enthält ausschließlich den konkreten ESP-IDF-
  Adapter;
- `fermentation_app` enthält die Recovery-/Fachlogik nur gegen abstrakte
  Ports;
- `device_platform_test_support` bleibt reine Testhilfe und wird nie
  Produktionsabhängigkeit.

ESP-IDF `v6.0.2 @ 7101770dc6db2667b3c477cc31365dd1acd6db4e`, 4-MB-Flash und
No-PSRAM bleiben die zu prüfende Release-1-Basis. Eine Änderung davon braucht
einen separaten belegten Ownerentscheid.

## Saubere Baselineinventur auf PR #116-HEAD

Die folgenden Aussagen wurden auf `30fa0a8264...` geprüft. Sie behaupten
nichts, was erst durch PR #117 eingeführt wurde.

### Tatsächlich vorhandene Komponenten

| Bereich | Vorhanden auf der sauberen Basis | Relevanz für #90 |
|---|---|---|
| Generischer Port | `lib/device_platform/src/state_store.hpp` mit getrennten `StateStoreReadStatus` (`Success`, `NotFound`, `ReadError`, `CapacityError`) und `StateStoreWriteStatus` (`Success`, `WriteError`, `CapacityError`, `CommitOutcomeUnknown`) | Kanonischer Portvertrag; keine neue parallele Store-Schnittstelle planen. |
| Schlüssel-/Wire-Grundlagen | `state_store_key.hpp`, `storage_types.hpp`, `storage_envelope.*`, `storage_slot_candidates.*`, `storage_slot_limits.hpp`, CRC- und Bytecodec-Komponenten | Anwendungsneutrale Schlüssel-/Envelope-/Slotbausteine sind vorhanden; konkrete Recordbedeutung bleibt im owning context. |
| Native Persistenztesthilfe | `device_platform_test_support::SimulatedPersistentStateStore` mit committed/pending/restart-Trennung sowie Write-Faults, Read-/NotFound-Injektion und simulierten Power-Cut-Fenstern | Ausgangspunkt für das Produkt-Recovery-Orakel; keine ESP-IDF- oder Produktionsabhängigkeit. |
| Konfigurationsvertrag | `configuration_storage_contract.hpp` mit Recordtypen und Keys `uc0..uc3`, `sc0..sc3`, `pc0..pc3`, `cm0..cm2`, `cr0/cr1`, `cb0/cb1` | Bestehende Generationen-, Manifest-, Root-, Fallback-, Bootstrap- und `StorageEpoch`-Semantik wiederverwenden. |
| Konfigurationspersistenz | `configuration_bootstrap*`, `configuration_graph*`, `configuration_recovery_service*`, `configuration_service*`, `configuration_mutation_coordinator*`, vorhandene Codecs und viele native Konsumententests | Zuerst unverändert gegen das neue Orakel prüfen; keine neue Recoveryplattform. |
| Run-Persistenz | `run_persistence_store.*`, `run_persistence_coordinator.*`, `run_persistence_codec.*`, `run_persistence_contract.*`, `run_recovery.*` und bestehende Tests | `rc0`/`rc1`/`rh0`, `Prepared`-/`Committed`-Schritte, Current-/Fallback-/Orphan- und Indeterminate-Semantik prüfen, nicht ersetzen. |
| Safety-/Gateprojektion | `SafetyCore`, `FaultCode`, `SafetyDisposition`, `SafetyBootDisposition`, `RunLoadDisposition`, `RunPersistenceLoadStatus` und `RunPersistenceCoordinatorState` | Unklarer Recoveryzustand bleibt logisch fail-closed; `Allowed` darf erst nach vollständiger Validierung und `Applied` entstehen. |
| ESP-IDF-Basis | `device_platform_esp_idf` enthält nur `EspTimerTimeSource` und `EspResetCauseSource`; Root-CMake pinnt ESP-IDF `v6.0.2`; `app_main` enthält den vorhandenen #29-actor-free Bring-up-Pfad | Lifecycle ist vorhanden, aber kein NVS-Storeadapter und keine #90-Komposition. |
| Ressourcen-/Hardwarebasis | #29-Profile setzen `APP_TARGET_FLASH_MB=4`, `APP_REQUIRE_PSRAM=0`, `APP_REAL_ACTUATORS_ENABLED=0`; PR #116 bleibt Draft und Issue #29 offen | #90 darf keine Hardware- oder Aktorannahmen ergänzen. Boardgates bleiben separat und fail-closed. |

### Tatsächlich nicht vorhandene Komponenten

Auf der sauberen Basis existieren nicht:

- `NvsStateStore` und `NvsStateStoreConfig`;
- `nvs_flash`-/NVS-Produktadapter oder ESP-IDF-NVS-Lifecycle-Integration;
- ein #90-Host-BDL-/Fault-Seam-Test;
- ein #90-Hardware-Harness, UART-Runner oder Power-Cut-Runner;
- #90-spezifische Capacity-, Build-Provenance-, Harness-Layout-, Release-
  Isolation- oder Hardwareverifikationsskripte;
- eine #90-Produktpartition oder Testpartition;
- #90-spezifische Workflow-/CI-Änderungen;
- die #90-spezifischen Hersteller-, Build- oder Hardwareberichte und die
  historischen R5.5–R5.7-Plan-Dateien.

Insbesondere enthält die saubere Basis keine Datei unter `issue_90` und keine
NVS-Datei. Die in PR #117 vorhandenen Dateien wie
`lib/device_platform_esp_idf/src/nvs_state_store.*`,
`main/issue_90_nvs_hardware_verification.*`,
`test/esp_idf_nvs_adapter_host/*`,
`scripts/issue_90_*`, `partitions/issue_90_state_store.csv` und die zugehörigen
CMake-/Workflowänderungen sind auf diesem Branch nicht vorhanden.

### Vorhandene Verträge, die neu abgeglichen werden müssen

Der vorhandene Port kommentiert eine per-Key-Old-or-New-Eigenschaft und
`CommitOutcomeUnknown`; R5.7 schränkt die Release-1-Produktgarantie bewusst
ein. Der neue Plan darf diesen Widerspruch nicht still übergehen:

- Der Port bleibt klein und anwendungsneutral.
- `CommitOutcomeUnknown` bleibt unbekannt und wird nie als Erfolg, OLD oder
  NEW geraten.
- Die konkrete Produktgarantie nach Stromunterbruch wird auf Record-,
  Generation-, Slot- und Recoveryebene definiert, nicht durch eine
  unbelegte Multi-Page-Same-Key-Garantie.
- Eine materielle Vertragsänderung wird erst im ownerfreigegebenen
  Vertrags-Slice umgesetzt; diese Runde ändert die Quellen nicht.

## Übernommener R5.7-Release-1-Produktvertrag

Die folgenden Entscheidungen bleiben verbindlich und werden auf der neuen
Baseline erneut geprüft:

1. NVS bleibt die Default-Hypothese. Callback 12/`NotFound` bleibt sichtbare
   `BACKEND_POWER_CUT_CHARACTERIZATION` / `KNOWN_BACKEND_LIMITATION`.
2. Der Callback-12-Befund darf weder zu Backend-PASS umetikettiert noch
   weggefiltert werden. Backendcharakterisierung und Produkt-Recovery-Gate
   werden maschinenlesbar getrennt ausgegeben.
3. Same-Key-Overwrite muss nach Stromunterbruch nicht als Release-1-
   Produktgarantie immer exakt OLD oder NEW liefern. Ein betroffener Record
   kann fehlen oder unlesbar sein; die höhere Ebene muss das erkennen.
4. Nur vollständig validierte Records, Generationen, Slots, Referenzen und
   Root-/Manifestgraphen dürfen aktiv werden.
5. Ein unklarer Zustand bleibt fail-closed: kein falsches Resume, kein
   stilles Factory-New, kein stilles Löschen eines unklaren Runs und keine
   produktive Aktorfreigabe.
6. `Prepared`, Orphan-, Partial-, Mixed-, beschädigte und unklar referenzierte
   Zustände werden nie als aktivierbarer Zustand behandelt.
7. Recoverystatus muss für eine spätere Projektion beobachtbar sein
   (`RECOVERY_STATUS_OBSERVABLE`), aber eine reale UI, ein Recovery-Screen,
   Display oder Touch ist kein #90-Gate.
8. Reale #90-Boardverifikation bleibt actor-free. Sie darf Boot, Reboot,
   Persistenz, Recoveryklassifikation und logischen Gate beobachten, nicht
   physische Aktorsicherheit behaupten.
9. Es gibt keine unnötige Vollprodukt-/Composition-Root-Integration in #90.
   `app_main` und `FermentationApplication` werden nur bei einem belegten,
   separat ownerfreizugebenden Bedarf berührt.
10. Kein Backendwechsel erfolgt allein wegen Callback 12. Backend-,
    Dependency- oder Architekturwechsel erfordern zuerst einen belegten
    Produkt-FAIL und einen neuen Ownerentscheid.
11. ESP-IDF `v6.0.2`, 4-MB-/No-PSRAM-Basis, Espressif-first und Hersteller-/
    Lizenzprovenienz bleiben Bewertungsgrundlage.

Zulässige Produktoutcomes sind:

```text
Configuration: NEW_VALID_CONFIGURATION
              OLD_VALID_CONFIGURATION / FALLBACK_VALID_CONFIGURATION
              CONFIGURATION_RECOVERY_REQUIRED

Run:          NEW_VALID_RESUME
              OLDER_VALID_CHECKPOINT_RESUME
              RUN_RECOVERY_REQUIRED / RUN_ABORT_REQUIRED
```

Keines dieser Outcomes darf einen Partial-/Mixed-/Corrupt-/Prepared-/Orphan-
Record, ein falsches Resume, stilles Factory-New oder eine logische
Aktorfreigabe aus unklarer Recovery verdecken.

## Historische #117-Befunde als neue Review- und Verifikationscheckliste

Diese Punkte sind Reviewwissen und Prüfanforderungen, keine Autorisierung zum
Übernehmen der alten Lösung:

- Ein zweiter `nvs_get_blob()` nach erfolgreicher Größenabfrage muss einen
  zwischenzeitlichen `ESP_ERR_NVS_NOT_FOUND` als eigenen aktuellen Befund
  erhalten; er darf nicht als ursprüngliches `NotFound` verschleiert werden.
- Partitionslabel- und Namespace-Limits sind exakt gegen den gepinnten
  ESP-IDF-Vertrag zu prüfen. Es darf keine erfundene zusätzliche
  Zeichenmengenrestriktion in Adapter, Namespace oder Partition entstehen.
- Backend-Characterization muss den bekannten Callback-12-Befund
  reproduzierbar und maschinenlesbar erhalten.
- Kein angeblich vollständiger `exhaustive`-Nachweis, wenn nur ein
  8.240-B-Szenario oder nur ein Teil der erforderlichen Mutations-/Produktfälle
  ausgeführt wurde.
- Fehlerartefakte müssen auch bei frühem FAIL geschrieben werden.
- Testpartition und Produktionspartition bleiben getrennt.
- Ein Raw-NVS-Pageparser darf nur mit korrekter `span`-/Continuation-Semantik
  verwendet werden.
- Die Baseline eines Cut-Orakels wird unmittelbar vor dem tatsächlich
  geschnittenen Zielwrite bestimmt.
- UART-/Runner-Vertrag darf nicht still erweitert oder umbenannt werden.
- Eine reale 4-MB-Hardwareannahme wird fail-closed geprüft, wenn sie Teil eines
  späteren Boardgates ist.
- Source-SHA und Firmware-BIN-SHA werden getrennt ausgewiesen.
- Release-Isolation wird nicht stärker behauptet, als ELF und Buildgraph
  tatsächlich beweisen.
- Stack-/Scratch-Nachweis folgt dem realen Task-/Callgraph-Vertrag.
- Lizenz- und Herkunftsinformation gehört in die vorgesehenen
  Third-Party-/Auditquellen.
- Keine Hardware-, GC-/Erase-, Wear- oder Power-Cut-PASS-Aussage ohne den
  jeweils geforderten realen Nachweis.

## Minimaler Adaptervertrag und erstmalige Einführung

Da `NvsStateStore` auf der sauberen Basis nicht existiert, darf der Plan ihn
nicht voraussetzen. Die erstmalige Einführung ist ein eigener späterer
Umsetzungsschritt:

1. Slice 1–4 arbeiten zunächst mit den vorhandenen abstrakten Ports und dem
   nativen `SimulatedPersistentStateStore`. Sie führen keinen ESP-IDF-
   Adapter ein.
2. Erst nach einem belegten Slice-3-Ergebnis und nach Umsetzung eventuell
   belegter minimaler Slice-4-Korrekturen wird ein **separates, ausdrückliches
   Owner-Gate für Slice 5 – NVS-Adaptereinführung** eingeholt. Die Freigabe
   muss den exakten vorherigen HEAD, die Produktorakel-Ergebnisse und die
   geplanten Adapter-/Partitiongrenzen nennen.
3. Slice 5 führt genau einen kleinen konkreten Adapter in
   `device_platform_esp_idf` ein. Er implementiert ausschließlich den
   vorhandenen `IStateStore`-Port, bleibt anwendungsneutral und hängt nicht
   von `fermentation_app` oder Testhilfe ab.
4. Partition und Namespace werden durch den owning context als explizite,
   validierte Konfiguration geliefert. Die R1-Konfiguration ist
   `state_store` / `fermentation`; das sind keine generischen Adapterdefaults.
   Der Adapter übernimmt weder `init` noch `deinit` des globalen NVS-
   beziehungsweise ESP-IDF-Lifecycles.
5. Kapazität, Schlüsselraum, Label-/Namespace-Limits, 4-MB-Layout, Test-
   versus Produktionspartition und die tatsächliche ESP-IDF-API werden vor
   jeder Umsetzung gegen `v6.0.2` belegt. Keine alte #117-CMake-, Partition-
   oder Harnesslösung wird automatisch übernommen.

Wenn Slice 3 statt einer kleinen Adapterintegration einen neuen Storeport,
eine Recoveryplattform, eine andere Architektur, einen Backendwechsel oder
eine Vollproduktkomposition nahelegt, wird angehalten und ein neuer
Ownerentscheid beziehungsweise eine neue Planrevision verlangt.

## Owner-gatete Umsetzungsslices

Die Reihenfolge ist eine Umsetzungsvorlage, keine Vorautorisierung.

### Slice 1 – kanonischer Vertrags- und ADR-Abgleich

Nach Freigabe der exakten R5.8-Plan-SHA werden zunächst die aktuelle saubere
Baseline und R5.7 gegen ADR-013/ADR-016, Konfigurations-/Run-/Safety- und
Quality-Verträge abgeglichen. Dabei werden insbesondere
`CommitOutcomeUnknown`, `NotFound`, Recoverystatus, `Prepared`/Orphan,
Generation/Fallback, `rc0`/`rc1`/`rh0`, Safe-Boot und logischer Gate geklärt.

Der Slice darf nur die vorgesehenen kanonischen Dokumente und nötige
portseitige Kommentare synchronisieren. Keine neue API, kein NVS-Adapter,
keine Partition, kein Oracle und keine Hardwareverifikation.

Gate: keine normative Widersprüchlichkeit, bestehende Statusfamilien sind
ausreichend oder ein konkreter Consumerbedarf ist belegt, Callback 12 bleibt
sichtbar getrennt und die actor-free Grenze ist dokumentiert.

### Slice 2 – Produkt-Recovery-Orakel

Nach separater Ownerfreigabe entsteht ein hostseitiges Produktorakel auf dem
vorhandenen `SimulatedPersistentStateStore`. Es trennt:

- Backend-/Fault-Characterization;
- vollständigen Reboot/Reload und Record-/Envelope-/CRC-/Schema-/Epoch-/
  Referenzvalidierung;
- Konfigurationsoutcomes;
- Runoutcomes für Current, Fallback, PreparedInterrupted,
  NotReconstructible und Orphan;
- `SafetyCore`-/Safe-Boot-/logische-Gate-Projektion.

Die Matrix umfasst mutierende Konfigurations- und Run-Schreibpfade, Fehler
vor Commit, `CommitOutcomeUnknown`, Read-/Capacityfehler, fehlende Records,
Partial/Mixed/Corrupt-Records, Referenzfehler, Neustart und kontrollierten
Discard/Abort-Pfad. Ein Backend-Callback-12-Fall bleibt als Charakterisierung
sichtbar und wird nicht in das Produktorakel hineingefiltert.

Gate: alle erlaubten und verbotenen Produktoutcomes sind reproduzierbar,
maschinenlesbar und ohne UI/physische Aktorannahme klassifiziert.

### Slice 3 – vorhandene Produktionskomponenten unverändert prüfen

Das Orakel wird zuerst gegen den vorhandenen Produktionsbestand ausgeführt:

- Konfigurations-Bootstrap, Root-/Manifest-/Graphauflösung, genau eine
  validierte Fallbackgeneration, `StorageEpoch`, Slot-/Referenzschutz und
  Migration;
- Run-Store, `rc0`/`rc1`/`rh0`, Current-/Fallback-Referenzen, Prepared-/Committed-
  Zustände und Nichtrekonstruierbarkeit;
- bestehende Status-/Kommentarverträge und `SafetyCore`-Projektion;
- die Bedingung, dass `Allowed` erst nach vollständiger Persistenzvalidierung
  und `Applied` erreicht wird.

Die Ergebnisse werden als konkrete `PASS`, `FAIL`, `BLOCKED` oder `NOT_RUN`
mit reproduzierbarem Fall, betroffenem Vertrag und minimaler Auswirkung
festgehalten. Callback 12 ist dabei kein Produkt-FAIL, sofern das Produkt-
Recovery-Gate korrekt klassifiziert und fail-closed bleibt.

Gate: belegte Lückenliste und Ownerentscheidung, ob der vorhandene Bestand
genügt. Keine prophylaktische Recoveryplattform und kein stiller API-
beziehungsweise Backendwechsel.

### Slice 4 – nur belegte minimale Produktionskorrekturen

Nur echte rote Produktfälle aus Slice 3 dürfen korrigiert werden:

1. zuerst vorhandene Generation-/Fallback-/Slotlogik;
2. danach kleine bestehende Status-/Recoveryprojektion;
3. nur bei nachgewiesenem Bedarf eine weitere Abstraktion mit eigenem
   Owner-Gate.

Unklare Zustände bleiben erhalten und beobachtbar; sie werden nicht als
`NoActiveRun`, Factory-New, alter Record oder erfolgreicher Commit
umetikettiert. Jede Änderung an API, Schema, Composition Root, Backend,
Dependency, Partition oder Recoveryarchitektur stoppt den Slice und verlangt
neue Freigabe.

### Slice 5 – erstmaliger ESP-IDF-NVS-Adapter und getrennte Backendcharakterisierung

Dieser Slice beginnt erst nach dem oben beschriebenen eigenen NVS-
Einführungsgate. Er umfasst nur den minimalen konkreten Adapter, seinen
korrekten ESP-IDF-Lifecycle-Anschluss durch den owning context und die
gezielte technische Charakterisierung. Die Produktorakel bleiben eine
separate Ebene.

Zu verifizieren sind mindestens:

- Fehlergetreue Abbildung der ESP-IDF-NVS-Read-/Write-Ergebnisse;
- zweistufige Blob-Lesevorgänge einschließlich eines zwischenzeitlichen
  `ESP_ERR_NVS_NOT_FOUND` nach erfolgreicher Größenabfrage;
- Partition-/Namespace-/Key-Limits nach dem gepinnten Herstellervertrag;
- NVS-Blobgrößen, Kapazität, reale interne Write-/Erase-/GC-Fenster und
  Callback-12-Maschinenstatus;
- Fehlerartefakte bei frühem und spätem FAIL;
- getrennte Test- und Produktionspartitionen;
- keine `fermentation_app`-Abhängigkeit und keine Adapter-Lifecycle-
  Verantwortungsübernahme.

Ein Host-BDL oder Raw-NVS-Parser darf nur eingeführt werden, wenn sein
Vertrag, seine Continuation-/`span`-Semantik, Zielwrite-Baseline und
Artefaktablage vorab als Teil dieses Slice-Gates feststehen. Ein
`exhaustive`-Label ist nur zulässig, wenn die komplette definierte Matrix
tatsächlich gelaufen ist.

Gate: Adapterbuild und Charakterisierung sind reproduzierbar; bekannte
Callback-12-Evidenz bleibt `FAIL_CALLBACK_12_NOT_FOUND` /
`KNOWN_BACKEND_LIMITATION` und separat vom Produktgate; kein Backend-PASS wird
behauptet.

### Slice 6 – finale Softwareverifikation

Nach Review der vorherigen Slices und ohne offene Befunde werden die in
`docs/CI_AND_QUALITY_GATES.md` vorgesehenen gezielten Nachweise auf dem finalen
HEAD ausgeführt:

- betroffene Native- und Produkt-Recovery-Fault-Matrix;
- getrennte Backendcharakterisierung und Produkt-Recovery-Gate;
- ESP-IDF-`v6.0.2`-Bring-up-/Releaseprofile und Hersteller-Static-Analysis;
- Kapazität, 4-MB-/No-PSRAM-Budget, RAM-/Stack-/Scratch-/Write-/Wearbudget;
- Architektur-, Release-Isolation-, Buildgraph-, Provenienz-, Lizenz- und
  Artefaktprüfungen.

Source-SHA, Firmware-BIN-SHA, Testpartition und Produktionspartition werden
separat ausgewiesen. Teilruns, frühe Abbrüche und fehlende reale Nachweise
bleiben `FAIL`, `BLOCKED` oder `NOT_RUN`; sie werden nicht zu einem
vollständigen PASS zusammengefasst.

### Slice 7 – gezielte actor-free reale Boardverifikation

Nur nach bestandenem Software-/Orakel-Gate und eigener Ownerfreigabe:

- Boardidentität, reale 4-MB-Flashbasis, No-PSRAM und reproduzierbaren
  UART-/EN-/BOOT- beziehungsweise bestätigten DTR-/RTS-Vertrag prüfen;
- sauberer Boot-/Reboot-/NVS-Kontrolllauf;
- repräsentative Konfigurations- und Run-Records, Recoveryklassifikation und
  logischen actor-free Gate über UART/Logs/Harness beobachten;
- mindestens drei manuelle Power-Unterbrechungen je ausgewähltem Szenario
  während aktiver Schreiblast, ohne eine interne Cutposition zu behaupten.

Der Slice behauptet keine physische Pegel-, MOSFET-, Lüfter-, Summer-,
BTS7960- oder Peltier-Sicherheit, keine UI-/Display-/Touch-Abnahme und keinen
vollständigen Fermentations-End-to-End-Lauf. Fehlende Boardidentität,
Kontrollierbarkeit, 4-MB-Nachweis, sichere Resetsteuerung oder geforderte
Messmittel bedeuten `BLOCKED/NOT_RUN`.

## Dynamische Status- und Provenienzbereinigung nach Ownerfreigabe

Nach der Freigabe der neuen exakten Plan-SHA werden die dynamischen Quellen
konsistent und in dieser Reihenfolge gepflegt:

1. Issue #90 erhält den neuen Restartstatus, die neue PR-/Branch-/Base-
   Provenienz und den Status der einzelnen ownergateten Slices.
2. `docs/ROADMAP.md` nennt die neue Planrevision, den sauberen PR-116-Head,
   den neuen Draft-PR und das nächste Owner-Gate; Anforderungen bleiben im
   Issue/Plan.
3. Der neue PR-Body enthält genau dieselben Branch-, Base-, HEAD-, Plan-SHA-
   und Statusdaten sowie die klare Aussage, dass bis zur Planfreigabe keine
   Implementation existiert.
4. Genau ein aktueller `SESSION HANDOVER` wird im neuen PR veröffentlicht.
   Er ersetzt nur frühere Handover desselben neuen PR; die historische
   Handover-Historie wird nicht gelöscht.
5. PR #117 bleibt eindeutig OPEN/Draft, historische/supersedierte und
   **nicht gemergte** Umsetzung. Das Schließen von PR #117 ist ausschließlich
   eine Owneraktion. Issue #90 wird durch den Agenten nicht geschlossen.

Nach jedem späteren Slice werden HEAD, Plan-SHA, Ergebnisstatus, offene
Befunde und der nächste Owner-Schritt erneut synchronisiert. Kein dynamischer
Status darf einen alten #117-Commit als neuen Produktionsnachweis verwenden.

## Abnahmekriterien und Status des neuen Plan-PR

Der neue R5.8-Plan ist erst nach Ownerfreigabe seiner exakten SHA eine
Umsetzungsgrundlage. Für diesen Plan-PR gelten bis dahin:

| Nachweis | Status |
|---|---|
| Live-PR-/Issue-/Roadmap-/Basisabgleich | `PASS` |
| Saubere Baselineinventur ohne PR #117 | `PASS` |
| R5.7-Produktentscheidungen vollständig übernommen | `PASS` als Planinhalt |
| Historische #117-Reviewbefunde als Prüfcheckliste | `PASS` als Planinhalt |
| Neue Produktionsimplementation | `NOT_RUN` / nicht enthalten |
| Neue Test-/Oracle-/Harness-/Runnerimplementation | `NOT_RUN` / nicht enthalten |
| Neue CI-/Dependency-/Backend-/Partitionimplementation | `NOT_RUN` / nicht enthalten |
| Firmware-, ESP-IDF-, Host- und Hardwaretests | `NOT_RUN` |
| Reale Boardverifikation und Power-Cut-Nachweis | `BLOCKED/NOT_RUN` bis eigenem Boardgate |
| Exakte Planfreigabe | `BLOCKED` bis Ownerentscheidung |

Der abschließende Diff dieses Plan-PR darf nur die neue Planrevision und die
minimale Roadmap-Synchronisierung enthalten. Vor Übergabe sind `git diff
--check`, Status, Branch, Base-SHA, Plan-SHA, PR-Status und PR-#117-Status live
zu verifizieren. Danach STOP.
