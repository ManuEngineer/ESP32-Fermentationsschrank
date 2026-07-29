# Implementierungsplan fuer Issue #57

## Planstatus

- Issue: `#57 – [E2.1d] Bootstrap, StorageEpoch und Recovery implementieren`
- Planbranch: `plan/issue-57-bootstrap-storageepoch-recovery`
- Basisbranch: `main`
- Basis-Commit: `95caef6441809148f09dbe43413df70a7a4202e5`
- Ausgangslage: PR #65 ist gemergt; #54, #55 und #56 sind abgeschlossen.
- Live-Status #57: `READY` ausschliesslich fuer diesen Plan-first-Schritt.
- Planstatus: `IMPLEMENTATION_BLOCKED_PENDING_PLAN_APPROVAL`
- Implementierung: nicht begonnen

Dieser Plan ist die einzige Repositoryaenderung der Planungsphase. Er wird erst
nach Commit und Push durch den folgenden exakten Ownerkommentar zur
Implementierungsgrundlage:

```text
PLAN APPROVED
Approved plan commit: <commit-sha>
```

Die Freigabe gilt nur fuer den genannten Plan-Commit. Jede materielle
Planaenderung macht sie gemaess `AGENTS.md` ungueltig.

## Ziel

Issue #57 soll den mit #54, #55 und #56 aufgebauten Variante-B-
Konfigurationskern um genau diese anwendungsspezifischen Verantwortungen
ergaenzen:

- einen redundanten, kanonisch auswaehlbaren
  `ConfigurationBootstrapRecord`;
- die eindeutige Unterscheidung zwischen nachweislich fabrikneuem Speicher,
  vorhandenem Altbestand, anderer Epoche, unbekanntem Schema, Korruption und
  Storefehler;
- die stromausfallsichere, idempotent wiederaufnehmbare Initialisierung von
  `StorageEpoch 1`;
- den normalen Boot mit vollstaendiger Active-/Fallback-Graphpruefung und
  vorbereiteter Runtimefreigabe;
- einen ausdruecklich fachlich autorisierten, wiederaufnehmbaren Werksreset als
  vorwaertsgerichteten Epochenwechsel;
- stabile typisierte Producerfehler fuer das nachgelagerte
  `CONFIGURATION_SAFETY_INTEGRATION_GATE` in #24;
- den nachweisbaren Erhalt der geraetespezifischen Touchkalibrierung.

Der Plan konkretisiert den bereits beschlossenen Vertrag. Er fuehrt weder ein
neues Persistenzmodell noch einen alternativen Active-/Fallback-Kern ein.

## Nicht-Ziele

Nicht Bestandteil von #57 sind:

- Laufpersistenz, Kontrollpunkte oder Wiederanlaufsemantik aus #17;
- systemweite Safetyfehlerklassen, persistente Verriegelungen, `SAFE_BOOT`,
  Fehlerreset oder Aktorfreigaben aus #24;
- UI, Service-PIN, Raw-Touch-Geste oder sonstige Autorisierung eines
  Werksresets;
- Touchcontroller, Kalibrierungsmathematik oder Kalibrierungspersistenz;
- Connectivity-, Authentication-, Credential- oder sonstige Secretrecords;
- Pending, `PendingRoot`, Aktivierungsintent oder
  `ConfigurationActivationRunAssessment`;
- persistente Previewdaten;
- produktiver NVS-/Preferences-Adapter und Composition-Root-Verkabelung;
- Partitionierung, physische Flashloeschung, reale Flashatomizitaet oder
  Flashlebensdauer;
- Hardware-, GPIO-, Pin-, Pegel- oder Bibliotheksauswahl;
- ein allgemeines Repository-, Provider-, Plugin-, Datenbank-, Journaling-
  oder Transaktionsframework;
- Live-Issue-, ADR-, Label-, Milestone- oder Projektstatusaenderungen im
  spaeteren Implementierungs-PR.

## Verbindliche Quellen und Entscheidungen

Die Dokumentationsprioritaet folgt
[`SPECIFICATION_REVIEW.md`](../SPECIFICATION_REVIEW.md), insbesondere der dort
festgelegten Reihenfolge. Fuer diesen Plan sind verbindlich:

- ADR-010, ADR-016 und ADR-018 im zentralen
  [`DECISIONS.md`](../DECISIONS.md);
- [`CONFIGURATION_PERSISTENCE.md`](../CONFIGURATION_PERSISTENCE.md);
- [`SETTINGS_AND_STORAGE.md`](../SETTINGS_AND_STORAGE.md);
- [`BACKUP_SECURITY_RETENTION.md`](../BACKUP_SECURITY_RETENTION.md);
- [`PR38_REVIEW_CORRECTIONS.md`](../PR38_REVIEW_CORRECTIONS.md);
- [`IMPLEMENTATION_ISSUES.md`](../IMPLEMENTATION_ISSUES.md);
- der gemergte #56-Plan
  [`issue-56-implementation-plan.md`](issue-56-implementation-plan.md);
- Live-Issues #16, #56, #57, #17 und #24 am Planungsdatum 2026-07-29;
- das Root-`AGENTS.md` sowie die Modulregeln unter `lib/`.

Fuer Adopt-or-build und Drittkomponentengrenzen wurden ausserdem vollstaendig
geprueft:

- [`RELEASE_1_ADOPT_OR_BUILD_AUDIT.md`](../audits/RELEASE_1_ADOPT_OR_BUILD_AUDIT.md);
- [`THIRD_PARTY_COMPONENTS.md`](../THIRD_PARTY_COMPONENTS.md);
- [`THIRD_PARTY_SOURCE_AND_LICENSE_REVIEW.md`](../audits/THIRD_PARTY_SOURCE_AND_LICENSE_REVIEW.md);
- [`COMPONENT_EVALUATIONS.md`](../audits/COMPONENT_EVALUATIONS.md).

Das Third-Party-Komponentenregister ist ein Entscheidungs- und
Pruefregister. Ein Eintrag darin ist keine automatisch freigegebene
Abhaengigkeit.

## Aktuelle Ausgangslage

### Bereits vorhandene technische Basis

Aus #54 stehen in `device_platform` bereit:

- das begrenzte binaersichere `IStateStore` mit getrennten Lese- und
  Schreibstatus;
- `Success`, `WriteError`, `CapacityError` und
  `CommitOutcomeUnknown` als eindeutiger Writevertrag;
- `StateStoreKey` mit ADR-016-konformen Grenzen;
- starke Typen einschliesslich `StorageEpoch` und checked Increment;
- Big-Endian-Codecs, CRC-32/ISO-HDLC und Envelope Version 1;
- technische Slotkandidatensuche mit sichtbaren `NotFound`-, Read-, Capacity-
  und Integritaetsbefunden;
- erneutes gebundenes Laden einzelner Slotpayloads;
- eine begrenzte generische Round-Robin-Slotmechanik.

Aus #55 stehen in `fermentation_app` bereit:

- `UserConfiguration`, `ServiceConfiguration` und `ProgramCatalog`;
- starke Dokumentrevisionen, Schema-1-Codecs und fachliche Validierung;
- `makeFactoryProgramCatalog()` sowie die bestaetigten Factorywerte `de`,
  `Europe/Zurich` und `Fermentationsschrank`;
- vier Dokumentslots je Dokumenttyp und deren kurzen Keys;
- `ITimeZoneResolver` und die fallible Vorbereitung der Zeitzone.

Aus #56 stehen in `fermentation_app` bereit:

- `ConfigurationManifest`, `ConfigurationRootRecord` und ihre Schema-1-Codecs;
- `ConfigurationGraphStore` mit vollstaendiger kanonischer
  Active-/Fallback-Auswahl, Identitaetskollisions- und High-Water-Pruefung;
- vier Dokument-, drei Manifest- und zwei Rootslots;
- `RuntimeConfigurationSnapshot` und begrenzte Read-Leases;
- `ConfigurationService` mit modellbegrenzter Runtimevorbereitung,
  nicht fehlschlagendem Publish und fail-closed Betriebszustaenden;
- `ConfigurationCommitIndeterminate` und `ConfigurationRuntimeFailure`;
- genau einen konkreten `ConfigurationMutationCoordinator` fuer normale
  Aktivierung, Migration sowie spaeter Bootstrap und Werksreset.

### Noch fehlender #57-Anteil

Es existieren noch keine Produktionsbausteine fuer:

- Bootstrapmodell, Bootstrapcodec, Bootstrap-Slots oder kanonische
  Bootstrapauswahl;
- das vollstaendige Factory-Neuheitsorakel;
- Initialisierung und Wiederaufnahme von `Initializing`;
- die Bootorchestrierung der Bootstrapzustaende;
- den Epochenwechsel und die Wiederaufnahme von `Resetting`;
- den schmalen vorbereiteten Runtime-Handoff fuer Initialisierung und Reset;
- die stabilen #57-Producer `ConfigurationUnavailable` und
  `ConfigurationIntegrityFailure`.

`src/main.cpp` und ein produktiver ESP32-Storeadapter werden in #57 nicht
verdrahtet. Tests instanziieren die Dienste mit lokalen `IStateStore`-Fakes und
derselben `ConfigurationMutationCoordinator`-Instanz.

## Adopt-or-build-Prüfung

### Ergebnis

Fuer #57 wird keine neue externe Bibliothek und kein neues Frameworkmodul
benoetigt. Vorhandene technische Persistenzbausteine werden adaptiert und die
projektbezogene Bootstrap-, Epochen-, Recovery- und Resetsemantik wird im
Anwendungskern implementiert.

NVS/Preferences bleibt gemaess ADR-016 das spaetere produktive Backend hinter
`IStateStore`. Der produktive Adapter, sein Toolchain-/Lizenznachweis und reale
Messungen sind nicht Bestandteil von #57. Eine eigene Flashdatenbank waere
gegen den Adopt-or-build-Entscheid und ist ausgeschlossen.

### Prüfung je technische Funktion

Die sieben verlangten Adopt-or-build-Fragen werden pro Funktion explizit in
derselben Zeile beantwortet. Spalte 1 nennt den vorhandenen Stand, Spalte 2 die
Wiederverwendung, Spalte 3 die Lizenz-, Toolchain-, Ressourcen- und
Wartungsgrenzen, Spalte 4 die schmale Integrationsgrenze, Spalte 5 die bewusst
projektspezifische Logik, Spalte 6 die Begründung für neuen eigenen Code und
Spalte 7 den noch erforderlichen Nachweis vor einer späteren Produktivauswahl.

| Funktion | 1. Bereits vorhanden? | 2. Wiederverwendung | 3. Grenzen | 4. Schmale Grenze | 5. Projektspezifisch | 6. Warum eigener Code? | 7. Späterer Spike/Nachweis |
|---|---|---|---|---|---|---|---|
| binärsicherer Storezugriff | ja, #54 `IStateStore` | unveränderter Port | Backendkapazität, Atomizität und Flashlebensdauer sind real zu messen | `ConfigurationRecoveryService` kennt nur `IStateStore` | fachliche Keys, Reihenfolge und Fehlerabbildung | nur Orchestrierung fehlt; kein Backendcode | NVS-/Preferences-Adapter: `SPIKE_REQUIRED`, `MEASUREMENT_REQUIRED` |
| Envelope und CRC | ja, #54 Envelope Version 1 und CRC-32/ISO-HDLC | unveränderte Codecs | fixierte C++17-Toolchain; CRC ist Integrität, keine Authentisierung | Bootstrapcodec baut und liest das vorhandene Envelope | Bootstrap-Schema und Plausibilität | der neue fachliche Recordtyp braucht einen kleinen Codec | Golden-, Negativ- und Roundtriptests; kein neuer Bibliotheksspike |
| Slot-/Recordmechanik | ja, #54 Slotkandidatenscan und gebundener Payloadload | vorhandene begrenzte Scan-/Readbackbausteine | feste Slotzahl und Recordgrösse; keine dynamische Datenbank | zwei konkrete Bootstrapkeys sowie bestehende Dokument-/Manifest-/Rootkeys | epochenübergreifende Bootstrapauswahl und Factory-Neuheitsorakel | vorhandener generischer Scan kennt diese Fachentscheidung nicht | Cut-Point-, Kapazitäts- und Ressourcenmessung; reales Backend später `SPIKE_REQUIRED` |
| Dokumentcodecs | ja, #55 | unverändert für Factorydokumente und Readback | bestehende Schema- und Längengrenzen bleiben bindend | typisierte Factorymodelle am vorhandenen Codec | Auswahl und Reihenfolge der Factoryinhalte | kein neuer allgemeiner Codec; nur Aufrufsteuerung | bestehende Tests plus #57-Readback-/Semantikmatrix |
| Manifest-/Rootgraph | ja, #56 | Graphcodecs, kanonischer Loader, Active-/Fallback- und Kollisionsprüfung | bestehendes Zwei-Modell- und Recordbudget | schmale epocheninitiale Graphoperation im `ConfigurationGraphStore` | Graph ohne vorherigen Active für Initialisierung/Reset | #56 setzt bislang einen vorhandenen Active voraus | native Cut-Points und Base-/Head-Bericht; keine Bibliotheksauswahl |
| Runtime-Snapshot | ja, #56 | Snapshotmodell, Plattformvorbereitung, Reader- und Modellbudget | fallible Vorbereitung muss vor Freigabe abgeschlossen sein | vorbereiteter Boot-/Reset-Handoff an `ConfigurationService` | Freigabe abhängig von Bootstrapzustand | #56 besitzt keinen Bootstrapabschluss | native Lebensdauer-/Fehler-/Retirementtests; reale Heapwerte `MEASUREMENT_REQUIRED` |
| Mutationskoordination | ja, #56 `ConfigurationMutationCoordinator` | exakt dieselbe injizierte Instanz | keine zweite Lease, kein allgemeiner Transaktionsdienst | #57 erwirbt die bestehende nicht kopierbare Lease | welche #57-Abläufe die Lease benötigen | nur Nutzung, kein neuer Koordinatorcode | Konkurrenztests mit #56 und #57; kein Hardware- oder Bibliotheksspike |
| Bootstrapzustandsmaschine | nein | Store-, Codec- und Coordinatorbausteine darunter | drei feste Zustände, zwei Slots, keine Frameworkabhängigkeit | konkrete `ConfigurationRecoveryService`-Operationen | `Initializing`, `Initialized`, `Resetting` und Cut-Point-Semantik | kein Plattformmodul darf diese Produkt-/Recoveryentscheidung treffen | vollständige native Zustands-/Cut-Point-Matrix |
| Epochengrenze | `StorageEpoch`, Envelopebindung und checked Increment aus #54 | vorhandener starker Typ und bestehende Bindungsprüfung | Überlauf ist fail closed; alte Epochen bleiben physisch möglich | Bootstraprecord bindet die kanonische Epoche | nächste Epoche, Epochensperre und Wiederaufnahme | technische Typen entscheiden keine kanonische Epoche | Überlauf-, Fremdepoche- und Reset-Cut-Point-Tests |
| Boot-Recovery | kanonischer Graphloader aus #56 vorhanden | vollständige Graphprüfung und Runtimevorbereitung | kein produktiver Adapter oder Composition Root in #57 | Bootstrapstore -> Graphstore -> Runtime-Handoff | Factory-Neuheitsorakel, Zustandsentscheidung und Ergebnisabbildung | diese Anwendungsorchestrierung fehlt | native Bootmatrix; reale Flash-/Watchdogmessung später `MEASUREMENT_REQUIRED` |
| Werksreset-Recovery | technische Writes und Graphbausteine vorhanden | Store-, Envelope-, Graph-, Snapshot- und Koordinatorbausteine | keine physische Löschgarantie, keine UI-/PIN- oder Touchlogik | schmaler bereits autorisierter fachlicher Reset-Eingang | vorwärtsgerichteter Epochenwechsel und idempotente Wiederaufnahme | kein Framework darf Resetpolicy oder Toucherhalt festlegen | native Reset-Cut-Points; reale Stromunterbrüche später `SPIKE_REQUIRED` |

### Wiederverwendungsnachweis

| Benötigte Funktion | Wiederverwendeter Baustein | Eigener Anteil | Begründung |
|---|---|---|---|
| Storezugriff | #54 `IStateStore` | konkrete #57-Aufrufreihenfolge und Fehlerabbildung | Backend und Port werden nicht neu entwickelt |
| Envelope und CRC | #54 Envelope Version 1 und CRC-32/ISO-HDLC | Bootstrap-Payloadcodec und Plausibilität | neuer fachlicher Record, unveränderte technische Hülle |
| Slot-/Recordmechanik | #54 Slotscan, Payloadload und Writevertrag | zwei Bootstrapkeys und epochenübergreifende Auswahl | keine Datenbank- oder Repositoryabstraktion nötig |
| Dokumentcodecs | #55 Schema-1-Codecs | Factorymodelle erzeugen, kodieren und rücklesen | bestehende Wiresemantik bleibt allein kanonisch |
| Manifest-/Rootgraph | #56 `ConfigurationGraphStore` | epocheninitialer Graph ohne vorherigen Active | Bootstrap und Reset benötigen genau diesen noch fehlenden Einstieg |
| Runtime-Snapshot | #56 `RuntimeConfigurationSnapshot` und `ConfigurationService` | vorbereiteter Boot-/Reset-Handoff | keine konkurrierende Runtimeplattform |
| Mutationskoordination | #56 `ConfigurationMutationCoordinator` | dieselbe Lease für Initialisierung, Boot-Recovery und Reset nutzen | global genau eine Konfigurationsmutation |
| Bootstrapzustandsmaschine | kein vorhandener fachlicher Baustein | drei feste Zustände und Cut-Point-Orakel | projektspezifische Recoverysemantik |
| Epochengrenze | #54 `StorageEpoch` und Envelopebindung | kanonische nächste Epoche und alte-Epochen-Sperre | Produkt- und Recoveryentscheidung, keine Storefunktion |
| Boot-Recovery | #56 Graphprüfung und Runtimevorbereitung | Bootstrapentscheidung, Factory-Neuheitsorakel und stabile Ergebnisse | verbindet vorhandene Bausteine ohne allgemeines Framework |
| Werksreset-Recovery | #54/#55/#56 Store-, Dokument-, Graph- und Runtimebausteine | vorwärtsgerichteter Epochenwechsel und Wiederaufnahme | zentrale Resetpolicy bleibt Anwendungskern |

### Lizenz-, Toolchain-, Ressourcen- und Wartungsgrenzen

- Der #57-Anwendungscode selbst verwendet nur Repositorycode und die bereits
  fixierte C++17-Toolchain.
- Arduino-ESP32 Preferences/NVS ist Bestandteil des fixierten Frameworkpfads
  Arduino-ESP32 `2.0.17`; LGPL-2.1- und eingebettete ESP-IDF-
  Drittkomponentenpflichten werden erst beim realen Adapter konkret geprueft.
- Die NVS-Partitionsgroesse bleibt `TBD_IMPLEMENTATION_BUDGET`.
- Reale Replace-Atomizitaet, Kapazitaet und Flashlebensdauer bleiben
  `MEASUREMENT_REQUIRED`.
- Kein Framework- oder Bibliothekstyp gelangt in `fermentation_app`.
- Ein direkter ESP-IDF-NVS-Adapter wird nur spaeter bei nachgewiesenem
  Vertragsvorteil gegen Preferences bewertet; #57 trifft diese Auswahl nicht.

### Adapter- und Integrationsgrenze

```text
spaeterer NVS-/Preferences-Adapter
  -> IStateStore
  -> vorhandene Envelope-/Slotbausteine
  -> ConfigurationBootstrapStore / ConfigurationGraphStore
  -> ConfigurationRecoveryService
  -> vorbereiteter ConfigurationService-Runtime-Handoff
```

Der neue Code endet an `IStateStore`, `ConfigurationGraphStore`,
`ConfigurationService` und `ConfigurationMutationCoordinator`. Er kennt weder
Preferences-, NVS-, Arduino- noch ESP-IDF-Typen.

### Warum eigener Code erforderlich ist

Kein technischer Store oder Frameworkbaustein darf entscheiden, ob Speicher
fabrikneu ist, welche `StorageEpoch` kanonisch ist, ob `Initializing` oder
`Resetting` fortgesetzt wird, wann alte Epochen logisch unerreichbar werden
oder ob ein Fehler die Runtimefreigabe sperrt. Diese Entscheidungen verbinden
Factorywerte, den Variante-B-Graphen und den Safety-Producervertrag und bleiben
deshalb bewusst kleine projektspezifische Fachlogik.

### Spaetere Spikes und Nachweise

Vor einer spaeteren produktiven Adapterauswahl beziehungsweise Releasefreigabe
bleiben erforderlich:

- NVS-/Preferences-Build und Base-/Head-Ressourcenbericht;
- reale NVS-Kapazitaets- und Replace-Atomizitaetspruefung;
- Stromunterbruch-Cut-Points auf realem ESP32;
- Heap, niedrigster freier Heap und groesster freier Block;
- Commitdauer, Regelzyklus-Jitter und Watchdogwirkung;
- Flashverschleiss und Langzeitzyklen;
- Plattformverschluesselung als separates `EVALUATE_BEFORE_RELEASE`-Gate.

Keines dieser Gates wird im #57-Plan- oder Implementierungs-PR als bestanden
behauptet.

## Modul- und Dateischnitt

### `device_platform`

Keine Aenderung ist geplant. `IStateStore`, Envelope, CRC, starke Typen und
Slotmechanik genuegen. Jede spaetere Aenderung dieses Moduls waere eine
materielle Planabweichung.

### `device_platform_test_support`

Keine Aenderung und keine Abhaengigkeit aus `fermentation_app`. Alle fuer #57
benoetigten Store-Decorator, Cut-Point-Fakes, Zugriffsjournale und
Touch-Sentinels liegen lokal in den Anwendungstests und implementieren nur den
Port aus `device_platform`.

### `fermentation_app`

Neue Produktionsdateien:

- `lib/fermentation_app/src/configuration_bootstrap.hpp`
- `lib/fermentation_app/src/configuration_bootstrap.cpp`
  - starke Bootstraptypen, Zustandswerte, Plausibilitaet, kanonische
    Gleichheit und stabile Diagnoseursachen;
- `lib/fermentation_app/src/configuration_bootstrap_codec.hpp`
- `lib/fermentation_app/src/configuration_bootstrap_codec.cpp`
  - einziges kanonisches Bootstrap-Schema-1-Wireformat ohne Storezugriff;
- `lib/fermentation_app/src/configuration_bootstrap_store.hpp`
- `lib/fermentation_app/src/configuration_bootstrap_store.cpp`
  - zwei Bootstrap-Slots, epochenuebergreifender Scan, kanonische Auswahl,
    High-Water, Schreiben, Readback und Unknown-Aufloesung;
- `lib/fermentation_app/src/configuration_recovery_service.hpp`
- `lib/fermentation_app/src/configuration_recovery_service.cpp`
  - Factory-Neuheitsorakel, `Initializing`-/`Initialized`-/`Resetting`-
    Orchestrierung, normaler Boot, Werksreset und stabile #57-Ergebnisse.

Jede neue konkrete Klasse beziehungsweise Typgruppe ist gegen vorhandene
Bausteine abgegrenzt:

| Neuer Baustein | Warum kein vorhandener Baustein genügt |
|---|---|
| `ConfigurationBootstrapRecord` und starke Bootstraptypen | `ConfigurationRootRecord` beschreibt einen Graphen, nicht den epochenübergreifenden Initialisierungs-/Resetzustand; eine Zweckentfremdung würde ADR-018 vermischen |
| `ConfigurationBootstrapCodec` | die vorhandenen Codecs kennen diesen neuen fachlichen Payload nicht; Envelope, CRC und Big-Endian-Primitiven werden dennoch wiederverwendet |
| `ConfigurationBootstrapStore` | der generische Slotreader entscheidet weder globale Bootstrapsequenz noch Zustands-/Epochenkollisionen; es entsteht nur ein konkreter Zwei-Slot-Store |
| `ConfigurationRecoveryService` | `ConfigurationService` aktiviert bestehende Graphen, besitzt aber bewusst keine Factory-Neuheits-, Bootstrap-, Epochengrenz- oder Werksresetsemantik |

Voraussichtlich geaenderte bestehende Produktionsdateien:

- `lib/fermentation_app/src/configuration_storage_contract.hpp`
  - Bootstrap-Record-Type-ID und zwei kurze Bootstrap-Slotkeys;
- `lib/fermentation_app/src/configuration_limits.hpp`
  - Bootstrap-Payload-/Envelope- und Factory-Scan-Obergrenzen;
- `lib/fermentation_app/src/configuration_graph_store.hpp`
- `lib/fermentation_app/src/configuration_graph_store.cpp`
  - schmale, modellbegrenzte Vorbereitung und Ausfuehrung eines vollstaendigen
    epocheninitialen Graphen ohne vorherigen Active-Graphen;
- `lib/fermentation_app/src/configuration_service.hpp`
- `lib/fermentation_app/src/configuration_service.cpp`
  - schmaler, besitzsicherer vorbereiteter Boot-/Reset-Runtime-Handoff unter
    den bestehenden Zwei-Modell-, Reader-, Retirement- und fail-closed-
    Vertraegen.

Nicht geplant sind Aenderungen an Dokumentmodellen, Dokumentcodecs,
Manifest-/Root-Wireformat, `IStateStore`, `src/main.cpp`, `platformio.ini` oder
einem Hardwareadapter. Ergibt die Detailimplementierung dennoch eine solche
Notwendigkeit, wird vor der Aenderung der Plan aktualisiert und erneut
ownerfreigegeben.

### Native Tests

Neue Testdateien:

- `test/test_configuration_bootstrap_codec/test_configuration_bootstrap_codec.cpp`
- `test/test_configuration_bootstrap_store/test_configuration_bootstrap_store.cpp`
- `test/test_configuration_recovery_service/test_configuration_recovery_service.cpp`

Voraussichtlich erweiterte Tests:

- `test/test_configuration_graph_store/test_configuration_graph_store.cpp`
  - epocheninitialer Graph, Root-Unknown und vollstaendige Zielgraphbindung;
- `test/test_configuration_service/test_configuration_service.cpp`
  - vorbereiteter Recovery-Handoff, Modellbudget, Retirement und keine
    Runtimefreigabe bei Fehlern.

### Dokumentation nach freigegebener Implementierung

Voraussichtlich anzupassen:

- `docs/CONFIGURATION_PERSISTENCE.md` fuer exakte Bootstrap-/Recoverytypen und
  den Umsetzungsnachweis;
- `docs/IMPLEMENTATION_ISSUES.md` fuer den sachlichen #57-Umsetzungsstand;
- `CHANGELOG.md` fuer #57 und die nachgewiesenen Grenzen;
- diese Plan-Datei bleibt mindestens bis zum Abschlussreview erhalten.

Keine ADR- oder Live-Issue-Aenderung ist Bestandteil der spaeteren
Implementierung.

## Gemeinsame Mutationskoordination

`ConfigurationRecoveryService` erhaelt per Konstruktor dieselbe konkrete
`ConfigurationMutationCoordinator&`, die auch der `ConfigurationService`
verwendet.

Verbindlicher Vertrag:

- Boot-Recovery, Initialisierung und Werksreset erwerben vor jeder
  persistenten Konfigurationsmutation eine nicht kopierbare
  `ConfigurationMutationLease` per `tryAcquire()`;
- die Lease bleibt von der exklusiven erneuten Basispruefung bis zum eindeutig
  erreichten End- oder fail-closed Zustand gehalten;
- `ConfigurationMutationBusy` beendet den Versuch ohne Storewrite und ohne
  Teilwirkung;
- kein zweiter Mutex, keine zweite Leaseart und kein persistenter
  Mutationszaehler entstehen;
- normale #56-Vorschau und reine Read-Pruefungen bleiben ausserhalb, solange
  keine Mutation begonnen wird;
- #57 implementiert keine Bootstrap- oder Resetlogik im Koordinator;
- die spaetere Composition Root muss genau eine Instanz an beide Dienste
  uebergeben, wird aber in diesem Issue nicht implementiert.

Boot ohne konkurrierende Laufzeit bleibt ebenfalls ueber denselben Vertrag
testbar. Damit wird die globale Aussage "hoechstens eine persistente
Konfigurationsmutation" auch fuer Initialisierung und Reset real statt nur
dokumentarisch eingehalten.

## BootstrapRecord Schema 1

### Typen und stabile IDs

Vorgesehen sind:

```text
ConfigurationBootstrapSequence : uint64_t, 0 reserviert
ConfigurationStorageFormatVersion : uint32_t, 0 reserviert
ConfigurationBootstrapState : uint8_t
```

Stabile Werte:

| Bedeutung | Wirewert |
|---|---:|
| `Initializing` | 1 |
| `Initialized` | 2 |
| `Resetting` | 3 |

Der Record verwendet:

- `RecordTypeId = 6`;
- `SchemaVersion = 1`;
- zwei ADR-016-konforme Keys `cb0` und `cb1`;
- Envelope-`StorageEpoch` als Epoche des Zustands;
- Envelope-`versionValue` als `ConfigurationBootstrapSequence`;
- keine UTC-Metadaten in Schema 1.

Die IDs 1 bis 5 bleiben unveraendert bei Dokumenten, Manifest und Root.
Record-Type-ID 6 sowie `cb0`/`cb1` werden erst durch die commitgebundene
Freigabe dieses Plans verbindlich.

### Kanonisches Payload-Wireformat

Die Schema-1-Payload besitzt exakt fuenf Bytes:

| Reihenfolge | Feld | Wirebreite |
|---:|---|---:|
| 1 | Speicherformatversion | `uint32`, Big Endian |
| 2 | Bootstrapzustand | `uint8` |

`StorageEpoch` und BootstrapSequence werden nicht redundant in die Payload
kopiert, weil sie bereits verpflichtende, CRC-geschuetzte Envelopefelder sind.
Die Speicherformatversion fuer R1 ist exakt 1. Unbekannte, fehlende oder
zusaetzliche Bytes und unbekannte Zustandswerte sind ungueltig.

Ohne UTC umfasst der Record 42 Bytes: 37 Byte Envelope einschliesslich CRC plus
5 Byte Payload. Das Workflowlimit bleibt exakt 42 Byte; ein Record mit UTC-Tag
oder zusaetzlichen Bytes ist fuer Bootstrap-Schema 1 fachlich ungueltig, auch
wenn der generische Envelope ihn technisch lesen koennte.

### Plausibilitaet

Ein gueltiger Bootstraprecord besitzt:

- Record-Type 6;
- Envelope- und Bootstrap-Schema 1;
- `StorageEpoch >= 1`;
- `BootstrapSequence >= 1`;
- Speicherformatversion 1;
- genau einen bekannten Zustand;
- exakt das kanonische Wireformat und eine gueltige CRC.

Ein technisch gueltiges neueres Bootstrap- oder Speicherformat wird als
`UnsupportedNewerConfigurationSchema` diagnostiziert und niemals
ueberschrieben oder als leer behandelt.

## Kanonische Bootstrapauswahl und High-Water

### Vollstaendiger Zwei-Slot-Scan

Der Bootstrapstore liest immer beide Keys vollstaendig. Weil die aktuelle
Epoche erst aus dem Bootstrap folgt, prueft dieser kleine anwendungsspezifische
Scan beide Envelopes zunaechst ohne erwartete Epoche. Er verwendet
`IStateStore::read()`, `decodeEnvelopeMetadata()` und den normalen
Envelopedecoder, baut aber keinen allgemeinen epochenuebergreifenden
Plattformscan.

Prioritaet:

1. `ReadError` oder `CapacityError` eines Slots blockiert jede Auswahl und
   Mutation fail closed.
2. `NotFound` ist nur fuer genau diesen Slot leer.
3. Vorhandene technisch korrupte, typfremde oder fachlich ungueltige Bytes
   ergeben einen Integritaetsbefund; sie werden nicht uebersprungen, um einen
   scheinbar brauchbaren aelteren Bootstrap zu aktivieren.
4. Ein unbekanntes neueres Schema oder Speicherformat blockiert mit eigener
   Diagnose.
5. Unter vollstaendig gueltigen Kandidaten gewinnt die hoechste
   BootstrapSequence unabhaengig vom Slot.

Gleichstand:

- gleiche BootstrapSequence und exakt identische kanonische Recordbytes:
  deterministische Auswahl des kleineren Slots mit Duplikatdiagnose;
- gleiche BootstrapSequence, aber unterschiedliche kanonische Bytes, Epoche,
  Formatversion oder Zustand: `ConfigurationIntegrityFailure`, keine
  Runtimefreigabe und kein Slot-Tiebreak als fachliche Entscheidung.

### High-Water und naechster Wert

Alle vollstaendig technisch und fachlich gueltigen Bootstraprecords tragen zu
einem globalen BootstrapSequence-High-Water bei, auch alte Epochen und
verwaiste, aber valide Records. Der naechste Wert ist checked `max + 1`.

- 0 ist reserviert.
- `UINT64_MAX` blockiert vor jedem Write.
- Unbekanntes neueres Schema, Read-/Capacity- oder Integritaetsfehler macht den
  High-Water unbekannt und blockiert die Mutation.
- Eine bestehende Identitaet darf nur fuer exakt dieselben kanonischen
  Recordbytes wiederverwendet werden, wenn ein unterbrochener Ablauf genau
  diesen Zustand idempotent fortsetzt.
- Unterschiedliche Inhalte erhalten niemals dieselbe BootstrapSequence.

Der neue Bootstraprecord wird in den nicht kanonischen Slot geschrieben. Bei
nur einem vorhandenen Kandidaten ist dies der andere Slot; bei identischem
Duplikatgleichstand wird der nicht ausgewaehlte Slot verwendet. Beide Slots
werden nie in einem logischen Schritt beschrieben.

### Bootstrap-Write und `CommitOutcomeUnknown`

Jeder Bootstrapwrite wird exakt zurueckgelesen und an Slot, Recordtyp, Schema,
Epoche, Sequenz, Laenge, CRC und kanonische Bytes gebunden.

- `Success` plus exakter Readback bestaetigt den neuen Zustand.
- `WriteError` oder `CapacityError` ist sicher nicht wirksam und wird ohne
  Fortschritt gemeldet.
- `CommitOutcomeUnknown` wird nur durch exakten Readback des Zielslots
  aufgeloest.
- Exakt neue Bytes bedeuten neuer Zustand; exakt vorherige Bytes oder
  `NotFound` bedeuten nicht wirksam.
- Fremde Bytes, unvollstaendiger Readback oder Read-/Capacity-Fehler bleiben
  `ConfigurationUnavailable` mit stabiler Ursache
  `BootstrapCommitIndeterminate`; es gibt keinen Rollback, keinen
  Factory-Fallback und keine weitere Mutation in diesem Prozessversuch.

Ein Neustart fuehrt den normalen Bootstrapscan erneut aus und setzt den
kanonisch persistenten Zustand fort. Es gibt kein persistentes Intent neben
dem Bootstraprecord.

## Nachweislich fabrikneuer Speicher

### Vollstaendiges Orakel

Automatische Initialisierung ist nur erlaubt, wenn genau alle folgenden
bekannten Konfigurationsslots gelesen wurden und jeder einzelne `NotFound`
liefert:

- 2 Bootstrap-Slots: `cb0`, `cb1`;
- 2 Rootslots: `cr0`, `cr1`;
- 3 Manifest-Slots: `cm0`, `cm1`, `cm2`;
- 4 UserConfiguration-Slots: `uc0` bis `uc3`;
- 4 ServiceConfiguration-Slots: `sc0` bis `sc3`;
- 4 ProgramCatalog-Slots: `pc0` bis `pc3`.

Das sind genau 19 begrenzte Reads. Es gibt weder Listing noch einen
unbeschraenkten Namespace-Scan.

### Negatives Orakel

Factoryinitialisierung ist verboten, sobald mindestens eines gilt:

- ein Slot liefert vorhandene Bytes, auch wenn sie zu einer anderen Epoche
  gehoeren;
- ein Slot enthaelt einen unbekannten Typ, ein unbekanntes Schema oder eine
  unbekannte Speicherformatversion;
- ein Envelope, CRC, Typ, Schema, Laenge oder fachlicher Inhalt ist korrupt;
- ein Read liefert `ReadError` oder `CapacityError`;
- Bootstrap fehlt, aber ein Root-, Manifest- oder Dokumentrecord ist vorhanden;
- die Slotbefunde sind widerspruechlich.

Vorhandene Daten ohne kanonischen Bootstrap sind nicht fabrikneu. Sie ergeben
`ConfigurationIntegrityFailure` beziehungsweise bei Storefehler
`ConfigurationUnavailable` und werden weder geloescht noch ueberschrieben.

### Grenze zu anderen Domaenen und Touchkalibrierung

Das Factory-Neuheitsorakel prueft nur die 19 expliziten
Konfigurations-/Bootstrapkeys. Es erfindet keine Keys fuer spaetere
Connectivity-/Authentication-Domaenen und verwendet kein Store-Listing.

Touchkalibrierung liegt ausserhalb dieses Keyregisters. #57 kennt ihren
Produktionskey nicht und liest, schreibt, loescht oder rotiert ihn nicht. Ein
rein testinterner Sentinel unter einem lokalen Fake-Key beweist nur die
Nichtberuehrung und reserviert keinen Produktionskey.

## Epocheninitialer Graphvertrag

### Zweck der Graphstore-Erweiterung

Der bestehende `ConfigurationGraphStore::prepareCommit()` setzt einen
kanonischen Active-Graphen derselben Epoche voraus. Initialisierung und
Werksreset benoetigen dagegen genau einen neuen vollstaendigen Graphen in einer
noch nicht aktiven Epoche und ohne Fallback.

Deshalb wird der Graphstore um einen schmalen, konkreten Ablauf fuer einen
`PreparedInitialConfigurationGraph` erweitert. Er verwendet vorhandene
Dokument-, Manifest-, Root- und Envelopecodecs. Er ist weder allgemeiner
Transaktionsbuilder noch zweiter Commitpfad: Er akzeptiert ausschliesslich die
drei Factorydokumente, `FactoryInitialization` oder `FactoryReset`, eine
explizite neue Epoche und den festen Identitaetsstart 1.

### Identitaeten innerhalb einer neuen Epoche

Je neuer `StorageEpoch` beginnen folgende fachliche Identitaeten bei 1:

- UserConfigurationRevision 1;
- ServiceConfigurationRevision 1;
- ProgramCatalogRevision 1;
- ConfigurationManifestGeneration 1;
- ConfigurationRootSequence 1.

Die Identitaet besteht immer aus Epoche, Recordart und Wert. Alte Epochen
werden nicht in die neuen High-Water-Werte eingerechnet. Innerhalb der
Zielepoche wird vor jedem Write trotzdem jeder zugehoerige Slot vollstaendig
gescannt:

- exakt derselbe kanonische Record unter Identitaet 1 wird idempotent
  wiederverwendet;
- unterschiedliche Bytes unter derselben Identitaet sind
  `ConfigurationIntegrityFailure`;
- ein technisch gueltiger Wert groesser als 1 in der neuen, noch nicht
  abgeschlossenen Epoche ist widerspruechlich und blockiert;
- ein unbekanntes neueres Schema blockiert;
- `ReadError` oder `CapacityError` blockiert vor jedem weiteren Write.

Es gibt keine separate `MutationSequence`. Der #56-High-Water-Nachweis bleibt
unveraendert fuer normale Mutationen; der neue Epochenstart ist durch die neue
`StorageEpoch` fachlich eindeutig.

### Deterministische Slotwahl und Idempotenz

Vor dem ersten Graphwrite werden alle Dokument-, Manifest- und Rootslots
gescannt und der vollstaendige Zielgraph geplant. Fuer jeden noch fehlenden
Zielrecord wird deterministisch der kleinste Slot gewaehlt, der keinen bereits
exakt passenden Zielrecord enthaelt. Alte Epochen sind nach bestaetigtem
`Initializing` beziehungsweise `Resetting` logisch nicht mehr aktiv und
duerfen fuer den neuen Epochenaufbau ueberschrieben werden.

Der Plan bindet jede Zielreferenz vor dem ersten Graphwrite. Bei Wiederaufnahme
werden vorhandene exakte Zielrecords gefunden und wiederverwendet; fehlende
Records werden mit identischen kanonischen Bytes vervollstaendigt. Der Ablauf
erzeugt keine zweite Generation, keine gemischte Epoche und keinen Fallback.

Die Graphstore-Erweiterung haelt hoechstens einen vollstaendigen Recordpuffer
und teilt die typisierten Factorymodelle zwischen Prepared Graph und
Runtime-Snapshot.

### Schreibreihenfolge

1. UserConfiguration Revision 1 schreiben, exakt ruecklesen und validieren;
2. ServiceConfiguration Revision 1 schreiben, exakt ruecklesen und validieren;
3. ProgramCatalog Revision 1 schreiben, recordweise und fachlich validieren;
4. Manifest Generation 1 schreiben, ruecklesen und alle Referenzen pruefen;
5. den gesamten Zielgraphen inklusive unveraenderter beziehungsweise bereits
   wiederverwendeter Records unmittelbar vor Rootwrite erneut validieren;
6. Root Sequence 1 ohne Fallback schreiben;
7. bei `CommitOutcomeUnknown` zuerst exakte Zielrootbytes und danach den
   gesamten Zielgraphen bestimmen;
8. Root und vollstaendigen Graphen erneut laden und an den vorbereiteten
   Zielgraphen binden.

Ein Root wird niemals geschrieben, solange ein Dokument oder Manifest fehlt,
unlesbar, kapazitaetsbegrenzt, korrupt, fachlich ungueltig oder nicht exakt an
den vorbereiteten Runtimekandidaten gebunden ist.

### Writeergebnisse vor dem Root

Fuer Dokument und Manifest gilt:

- `Success`: exakter Readback erforderlich;
- `WriteError`/`CapacityError`: kein Rootwrite, idempotente spaetere
  Wiederaufnahme;
- `CommitOutcomeUnknown`: exakter Readback unterscheidet nicht wirksam und
  exakt neu; unaufloesbarer Readback beendet den Versuch fail closed;
- ein exakt neu persistierter Vor-Root-Record bleibt bei spaeterem Abbruch
  verwaist, wird bei Wiederaufnahme aber nur fuer denselben Zielinhalt
  wiederverwendet.

Fuer den Root gilt zusaetzlich:

- exakt neuer Root plus vollstaendig gueltiger Zielgraph bedeutet persistent
  committed;
- exakt neuer Root mit nicht abschliessbar pruefbarem Zielgraph bleibt
  `ConfigurationUnavailable`, niemals alter Erfolg;
- fremder Root oder andere Payload ist weder alt noch neu und bleibt fail
  closed;
- es gibt keinen automatischen Rollback und keine Reaktivierung einer alten
  Epoche.

## Runtime-Handoff und Modellbudget

### Vorbereiteter Handoff

Der `ConfigurationService` erhaelt einen schmalen internen
`ConfigurationRecoveryActivation`-Besitzvertrag. Er ist:

- nur durch `ConfigurationRecoveryService` unter gehaltener gemeinsamer
  Mutationslease erzeugbar;
- nicht kopierbar und genau einmal publizierbar oder verwerfbar;
- an den exakten Zielgraphen, die `StorageEpoch`, Manifestreferenz,
  Runtimebindungsidentitaet und eine Zustandsrevision gebunden;
- vollstaendig fallibel vorbereitet, bevor der Zielroot geschrieben wird;
- Teil des bestehenden Zwei-Modell- und Acht-Reader-Budgets;
- frei von Bootstrap-, Reset- oder Storesemantik.

Der Handoff verwendet intern dieselbe Snapshotvorbereitung wie #56. Es gibt
keinen zweiten Runtime-Snapshot-Typ und keinen zweiten Publisher.

### Modell- und Lesergrenzen

- Beim ersten Boot existiert hoechstens das neue Factorymodell.
- Beim Werksreset existieren hoechstens die alte aktive und die vorbereitete
  neue Modellgeneration.
- Ist die zweite Modellposition durch Preview, Commit, Retirement oder alte
  Read-Leases belegt, wird der Reset vor `Resetting` und vor jedem Write mit
  `ConfigurationModelBudgetBusy` abgelehnt.
- Neue normale Preview-, Runtime- oder Mutationsfreigaben werden nach
  bestaetigtem `Resetting` gesperrt.
- Bereits gehaltene unveraenderliche Read-Leases bleiben speichersicher, gelten
  aber nicht als Freigabe der neuen Epoche.
- Rootwrite erfolgt nur, wenn Snapshot, Graph, Runtimewerte, Retirementbesitz
  und alle notwendigen Zustandsrevisionsschritte vorbereitet sind.
- Nach Rootcommit publiziert der bestehende Publisher ohne Allokation,
  Serialisierung, Validierung oder Storezugriff.
- Der alte Publisherbesitz wird ausserhalb des kritischen Publish-Schritts
  zerstoert; die Modellposition wird erst nach tatsaechlichem Besitzende frei.

### Runtimefreigabe und Bootstrapabschluss

`RuntimeConfigurationSnapshot` wird fachlich erst als normale #57-Runtime
freigegeben, wenn der neue Graph vollstaendig verifiziert und der zugehoerige
Bootstrapzustand `Initialized` eindeutig bestaetigt ist.

Fuer einen neu geschriebenen Root gilt ein zweiphasiger Handoff:

1. Der nicht fehlschlagende Publishertausch bindet intern den vorbereiteten
   Snapshot an den persistenten Zielroot, waehrend normale neue Akquisitionen
   weiterhin gesperrt bleiben.
2. Der Bootstrapstore schreibt und bestaetigt `Initialized`.
3. Erst danach wird der Dienst ohne weitere Allokation oder Storearbeit fuer
   normale Runtime-Read-Leases auf `Operational` freigegeben.

Scheitert oder bleibt der `Initialized`-Write unbestimmt, ist zwar kein
gemischter Graph entstanden, aber #57 liefert keine normale Runtimefreigabe.
Der Dienst bleibt fail closed; ein Neustart rekonstruiert aus Root und
Bootstrap und setzt den Ablauf idempotent fort. Es erfolgt kein Rollback des
neuen Roots und kein Rueckfall in die alte Epoche.

Bei Wiederaufnahme nach Neustart existiert kein fluechtiger Prepared-Handoff.
Der Zielgraph wird erneut vollstaendig geladen, der Snapshot neu fallibel
vorbereitet und erst nach dem fuer den jeweiligen Zustand erlaubten
persistentem Abschluss freigegeben.

## Initialisierung von StorageEpoch 1

### Voraussetzungen vor dem ersten Write

Unter einer gemeinsamen Mutationslease werden vor dem ersten Write:

1. der vollstaendige Factory-Neuheitsscan abgeschlossen;
2. Factory-UserConfiguration erzeugt und fachlich validiert;
3. die leere Factory-ServiceConfiguration validiert;
4. vier Factory-Arbeitskopien erzeugt und der Katalog vollvalidiert;
5. Zielslots und alle Schema-1-Recordbytes begrenzt vorgeplant;
6. Zeitzone und Runtime-Ressourcen soweit ohne persistente Referenzen moeglich
   vorbereitet;
7. BootstrapSequence 1, `StorageEpoch 1` und alle Zielidentitaeten checked
   gebunden;
8. Modell-, Record- und Zustandsrevisionsbudgets reserviert.

Erst danach darf `Initializing` geschrieben werden.

### Persistenter Ablauf

1. `ConfigurationBootstrapRecord(Sequence 1, Epoch 1, Initializing)` schreiben
   und exakt ruecklesen;
2. UserConfiguration Revision 1 mit `de`, `Europe/Zurich` und
   `Fermentationsschrank` schreiben;
3. ServiceConfiguration Revision 1 mit exakt leerer Payload schreiben;
4. ProgramCatalog Revision 1 mit vier Factory-Arbeitskopien schreiben;
5. Manifest Generation 1 mit `InternalSystem` / `FactoryInitialization`
   schreiben und vollstaendig pruefen;
6. Zielgraph und Runtime-Handoff vollstaendig vorbereiten;
7. Root Sequence 1 mit Active Generation 1 und ohne Fallback schreiben und
   den gesamten Graphen validieren;
8. vorbereiteten Snapshot intern nicht fehlschlagend publizieren, normale
   Akquisition aber noch sperren;
9. BootstrapSequence 2 als `Initialized` unter Epoche 1 schreiben und exakt
   bestaetigen;
10. normale Runtimefreigabe ohne weitere fallible Arbeit linearisiert
    erteilen.

Jeder Schritt prueft zuerst, ob exakt sein erwarteter kanonischer Record bereits
persistiert ist. Wiederaufnahme wiederholt nie blind einen Write und vergibt
keine neue Identitaet fuer denselben Zielzustand.

### Cut-Point-Orakel fuer `Initializing`

| Persistenter Stand nach Neustart | Verbindliches Orakel |
|---|---|
| kein Bootstrap, alle 19 Slots `NotFound` | Factory-Neuheit erneut beweisen und Initialisierung beginnen |
| `Initializing`, kein Dokument | Factorymodelle neu erzeugen, ab User Revision 1 fortsetzen |
| `Initializing`, Teilmenge exakter Dokumente | exakte Records wiederverwenden, fehlende in fester Reihenfolge ergaenzen |
| `Initializing`, Manifest fehlt | Dokumente vollvalidieren, Manifest schreiben |
| `Initializing`, Manifest vorhanden, Root fehlt | Zielgraph und Runtime vorbereiten, vor Root nochmals vollvalidieren |
| `Initializing`, Root exakt vorhanden | gesamten Graphen laden, Runtime vorbereiten, niemals alte Epoche aktivieren |
| Root intern publiziert, `Initialized` nicht bestaetigt | bei Neustart aus persistentem Root neu vorbereiten und Abschluss fortsetzen |
| `Initialized` exakt bestaetigt | normalen Bootpfad verwenden |
| irgendein erwarteter Record kollidiert, ist korrupt oder unlesbar | fail closed, kein weiterer Write und keine Runtimefreigabe |

## Normaler Boot

### Bootentscheidung

Der Bootpfad besitzt genau diese Reihenfolge:

1. beide Bootstrap-Slots vollstaendig scannen;
2. fehlt ein Bootstrap, das 19-Slot-Factory-Neuheitsorakel ausfuehren;
3. kanonischen Bootstrap auf Schema, Speicherformat, Epoche, Sequenz und
   Zustand pruefen;
4. `Initializing`, `Initialized` und `Resetting` getrennt behandeln;
5. fuer die Bootstrap-Epoche `ConfigurationGraphStore::loadCanonicalGraph()`
   verwenden;
6. Active, ersatzweise genau den vollstaendig gueltigen Fallback aus #56
   akzeptieren und Fallbacknutzung diagnostizieren;
7. Runtime-Snapshot vollstaendig fallibel vorbereiten;
8. erst nach eindeutig erfolgreichem Pfad normale Runtime freigeben.

### `Initialized`

- Bootstrap und Graph muessen dieselbe Epoche besitzen.
- Ein Rootslot-`ReadError` oder `CapacityError` verhindert die kanonische
  Reihenfolgebestimmung und damit jede Runtimefreigabe.
- Ein unbrauchbares Active darf nur auf den vollstaendig validierten Fallback
  desselben kanonischen Roots fallen.
- Ohne nutzbaren Active/Fallback entsteht `ConfigurationUnavailable`.
- Identitaetskollision, CRC-, Referenz-, Epochen- oder semantischer Fehler
  entsteht als `ConfigurationIntegrityFailure`.
- Ein unbekanntes neueres Bootstrap-, Root-, Manifest-, Dokument- oder
  Speicherformat wird separat als `UnsupportedNewerConfigurationSchema`
  abgelehnt.
- Store-Read-/Capacityfehler bleiben stabile Storeursachen und werden nicht als
  `NotFound` oder Integritaetskollision umgedeutet.

### Zuvor unbestimmter Root-Commit

Nach Neustart existiert kein fluechtiger #56-Aufloesungskontext. Der #57-
Bootpfad verwendet deshalb den vollstaendigen kanonischen Root-/Graphscan aus
#56:

- beide Rootslots muessen eindeutig gelesen werden;
- globale Rootreihenfolge und byteidentische Gleichstaende werden geprueft;
- jeder benoetigte Active-/Fallback-Record wird technisch, referenziell und
  fachlich vollstaendig validiert;
- ein fremder hoeherer Root, eine Identitaetskollision oder ein unlesbarer
  benoetigter Record bleibt fail closed;
- nur ein eindeutig kanonischer vollstaendiger Graph darf eine neue Runtime
  erzeugen.

Damit wird weder ein alter noch ein neuer Graph aus einem fluechtigen
Vorwissen behauptet. Ein erneut unklarer Zustand liefert keine Runtime und
wird an das spaetere Safety-Gate gemeldet.

### Stabile Ergebnisse

Der geplante oeffentliche Ergebnisvertrag unterscheidet mindestens:

```text
ConfigurationRecoverySuccess
  - RuntimeReady
  - FactoryInitializationCompleted
  - FactoryResetCompleted

ConfigurationRecoveryFailure
  - ConfigurationMutationBusy
  - ConfigurationModelBudgetBusy
  - ConfigurationUnavailable
  - ConfigurationIntegrityFailure
  - UnsupportedNewerConfigurationSchema
  - PersistenceReadFailure
  - PersistenceCapacityFailure
  - PersistenceWriteFailure
  - BootstrapCommitIndeterminate
  - CounterOverflow
  - RuntimePreparationFailure
```

Die beiden Safety-Producer bleiben stabil `ConfigurationUnavailable` und
`ConfigurationIntegrityFailure`. Die anderen Kategorien sind konkrete
redigierte Ursachen beziehungsweise aufrufbare Ergebnisse und keine neue
Safetyklassifikation. Store-, Key-, Codec- oder Bibliothekstypen werden nicht
ungefiltert an UI oder #24 durchgereicht.

## Werksreset als vorwaertsgerichteter Epochenwechsel

### Eingang und Vorbedingungen

Der Produktionsdienst bietet nur einen schmalen typisierten Eingang sinngemaess
`beginAuthorizedFactoryReset()`. Der Name bestaetigt keine UI- oder
Berechtigungsimplementierung: Der Aufrufer muss die fachliche Autorisierung
bereits erbracht haben.

#57 prueft nicht:

- Service-PIN oder Websession;
- Raw-Touch oder physische Anwesenheit;
- aktiven beziehungsweise recoverable Lauf;
- Safety- oder Hardwarefreigabe;
- Historien- oder Journalbereinigung.

Diese Gates bleiben bei ihren Issues. Der #57-Eingang darf keine dieser
Verantwortungen durch einen booleschen Bypass nachbauen.

Vor `Resetting` werden unter gemeinsamer Mutationslease:

- kanonischer `Initialized`-Bootstrap und aktuelle Epoche erneut gelesen;
- BootstrapSequence-High-Water vollstaendig bestimmt;
- naechste `StorageEpoch` und BootstrapSequence checked berechnet;
- Factorymodelle, Zielslots, Zielidentitaeten, Recordgroessen,
  Runtimevorbereitung und Modellbudget vollstaendig vorgeplant;
- alle normalen neuen Preview- und Runtimeakquisitionen fuer den Uebergang
  kontrolliert gesperrt.

### Linearisierung der Epochengrenze

Der bestaetigte Write von

```text
ConfigurationBootstrapRecord(
  nextBootstrapSequence,
  nextStorageEpoch,
  Resetting)
```

ist der persistente Linearisierungspunkt der neuen Resetepoche. Erst danach
duerfen Records der neuen Epoche geschrieben oder alte-Epochen-Slots fuer den
neuen Aufbau wiederverwendet werden.

Nach bestaetigtem `Resetting`:

- wird die alte Epoche nie mehr als aktuelle Konfiguration reaktiviert;
- gibt es keinen automatischen Rollback auf den alten Bootstrap oder Graphen;
- bleiben alte Bytes physisch moeglicherweise vorhanden, sind aber logisch
  unerreichbar;
- bleiben weitere normale Konfigurationsmutationen und Runtimefreigaben bis
  zum Abschluss gesperrt;
- fuehrt jeder Fehler zu einem wiederaufnehmbaren oder fail-closed Ergebnis,
  niemals zu Factoryinitialisierung aufgrund von Korruption.

### Persistenter Resetablauf

1. `Resetting` unter der checked naechsten Epoche schreiben und bestaetigen;
2. UserConfiguration Revision 1 mit Factorywerten schreiben;
3. ServiceConfiguration Revision 1 schreiben;
4. ProgramCatalog Revision 1 mit vier neuen Factory-Arbeitskopien schreiben;
5. Manifest Generation 1 mit `InternalSystem` / `FactoryReset` schreiben;
6. gesamten neuen Zielgraphen und vorbereiteten Runtime-Snapshot binden;
7. Root Sequence 1 ohne Fallback schreiben und vollstaendig validieren;
8. vorbereiteten Snapshot intern nicht fehlschlagend publizieren, normale
   Akquisition weiterhin sperren;
9. naechste BootstrapSequence als `Initialized` derselben neuen Epoche
   schreiben und bestaetigen;
10. normale Runtimefreigabe ohne weitere fallible Arbeit erteilen.

Die BootstrapSequence bleibt epochenuebergreifend monoton; Dokumentrevision,
Manifestgeneration und Rootsequenz beginnen innerhalb der neuen Epoche bei 1.

### Resetwiederaufnahme

Bei `Resetting` wird ausschliesslich die im kanonischen Bootstrap gebundene
Zielepoche fortgesetzt. Der Dienst:

- scannt alle Zielslots erneut;
- akzeptiert nur exakt gebundene Factoryrecords der Zielepoche;
- verwendet exakte vorhandene Zielrecords idempotent wieder;
- ergaenzt nur fehlende Zielrecords in der festen Reihenfolge;
- publiziert nie einen gemischten Graphen;
- setzt `Initialized` erst nach vollstaendig bestaetigtem Zielroot und
  vorbereitetem Runtime-Handoff;
- interpretiert Records alter Epochen nie als Zielrecords;
- startet weder eine weitere Epoche noch einen zweiten Reset, solange der
  bestehende Reset nicht eindeutig abgeschlossen ist.

### Reset-Cut-Point-Orakel

| Persistenter Stand | Verbindliches Orakel |
|---|---|
| vor `Resetting` | alte Epoche bleibt kanonisch, kein Zielrecord wurde geschrieben |
| `Resetting` bestaetigt, keine Zielrecords | nur neue Epoche fortsetzen; alte Runtime nicht neu freigeben |
| Teilmenge Ziel-Dokumente | exakte Records wiederverwenden, fehlende ergaenzen |
| Zielmanifest vorhanden | alle Referenzen und Factoryinhalte erneut pruefen |
| Zielroot fehlt | Runtime vorbereiten, Zielgraph vor Root vollvalidieren |
| Zielroot exakt persistent | nie auf alte Epoche zurueck; Zielgraph vollstaendig pruefen |
| Snapshot intern publiziert, `Initialized` fehlt | normale Akquisition gesperrt; nach Neustart neu rekonstruieren und abschliessen |
| `Initialized` bestaetigt | neue Epoche normal booten; alte Epochen unerreichbar |
| Kollision, unbekanntes Schema oder Storefehler | fail closed; kein automatischer Rollback oder neuer Reset |

### Keine physische Loeschbehauptung

Der Reset ueberschreibt nur die fuer den neuen Graphen benoetigten Slots und
wechselt logisch die Epoche. Er verspricht weder Secure Erase noch das
Entfernen aller Altbytes aus Flash, Wear-Leveling oder freien NVS-Seiten.
Spaetere reale Secret-Domaenen muessen mit ihrem ersten Konsumenten eigene
epochengebundene Reset-, Widerrufs- und Redactionvertraege ergaenzen.

## Touchkalibrierung

ADR-010 verlangt, dass ein normaler vollstaendiger Werksreset die
geraetespezifische Touchkalibrierung behaelt.

Verbindlich fuer #57:

- kein Touchkalibrierungskey wird in
  `configuration_storage_contract.hpp` aufgenommen;
- kein #57-Produktionscode liest, schreibt, loescht, rotiert oder migriert
  Touchkalibrierung;
- Factory-Neuheit, Initialisierung und Reset listen keine Touchkeys;
- ein lokaler Teststore protokolliert alle Keys und haelt einen testinternen
  Sentinelwert ausserhalb des Produktionsregisters;
- vor und nach jedem Initialisierungs-/Reset-Cut bleibt der Sentinel
  byteidentisch;
- ein Test schlaegt fehl, sobald #57 den Sentinelkey auch nur liest;
- der gesonderte Recoveryfall fuer unbrauchbare Kalibrierung bleibt ausserhalb;
- keine Touchmathematik, Geste, Koordinate oder Rohwertschwelle wird geplant.

## Fehler-, Recovery-, Security- und Safetygrenzen

### Fehlerklassifikation

Mindestens folgende Ursachen bleiben unterscheidbar:

- vollstaendig leerer Speicher;
- Daten nur einer anderen Epoche;
- kein kanonischer Graph fuer die Bootstrap-Epoche;
- allgemeine Root-/Graph-/Referenz-/CRC-/Semantikintegritaet;
- persistente Identitaetskollision;
- unbekanntes neueres Schema oder Speicherformat;
- `ReadError`, `CapacityError`, `WriteError`;
- unbestimmter Bootstrap- oder Rootwrite;
- BootstrapSequence-, StorageEpoch-, Revision-, Generation- oder
  RootSequence-Ueberlauf;
- Runtimevorbereitungs- oder interner Zustandsfehler.

Nur `NotFound` ist leer. Weder andere Epoche, unbekanntes Schema noch
Korruption oder Storefehler werden zu Factory-Neuheit normalisiert.

### Fail-closed-Regel

Bei jedem unbekannten, unlesbaren, widerspruechlichen oder weiterhin
unbestimmten Zustand gilt:

- keine normale Runtimefreigabe;
- keine neue Previewinstallation;
- keine weitere normale Konfigurationsmutation;
- keine Slotwiederverwendung ausser als eindeutig gebundene idempotente
  Fortsetzung desselben `Initializing`-/`Resetting`-Ziels;
- kein automatischer Werksreset;
- kein Factory-Fallback;
- kein Rollback auf eine alte Epoche;
- stabile redigierte Diagnose fuer das Safety-Gate.

### Grenze zu #24

#57 produziert ausschliesslich:

- `ConfigurationUnavailable`;
- `ConfigurationIntegrityFailure`;
- keine normale Runtimefreigabe im Fehlerfall.

#57 implementiert keine systemweite Fehlerklasse, persistente Verriegelung,
Bootprioritaet, `SAFE_BOOT`, Fehlerreset oder Aktorsperre. #24 muss diese realen
Producer spaeter zusammen mit `ConfigurationRuntimeFailure` und
`ConfigurationCommitIndeterminate` aus #56 im
`CONFIGURATION_SAFETY_INTEGRATION_GATE` integrieren und testen.

Ein Neustart der #57-Dienste loescht keine spaetere #24-Verriegelung. Eine
erfolgreiche lokale Rekonstruktion meldet nur den Konfigurationszustand; sie
setzt keinen Safetyfehler zurueck.

### Security

- Bootstrap-, Graph- und Recoverydiagnosen enthalten keine Secrets.
- CRC wird nicht als Authentisierung oder Verschluesselung dargestellt.
- #57 erzeugt keine Connectivity-/Authentication-Manifeste, Secret-Roots,
  Credentialslots, KDF-, Session- oder Tokenwerte.
- Alte Epochen sind logisch unerreichbar, aber nicht nachweislich physisch
  geloescht.
- Plattformverschluesselung bleibt ein separates
  `EVALUATE_BEFORE_RELEASE`-Gate.

## Vollstaendige Teststrategie

Alle Anwendungstests verwenden lokale, schmale `IStateStore`-Fakes oder
Decorator. `fermentation_app` haengt weder in Produktion noch in Tests von
`device_platform_test_support` ab.

### Bootstrapcodec

| Gruppe | Faelle |
|---|---|
| Golden | exakt 5 Payloadbytes; bekannte kanonische Bytes fuer alle drei Zustaende |
| Roundtrip | `Initializing`, `Initialized`, `Resetting`; Grenzsequenz und Epoche |
| Negativ | 0/ueberlaufrelevante Werte, unbekannter Zustand, Format 0/2, fehlende/zusaetzliche Bytes, UTC-Tag |
| Envelopebindung | falscher Typ, Schema, Epoche, Sequence, Laenge, CRC und Recordbytes |

### Bootstrapstore und kanonische Auswahl

| Gruppe | Faelle |
|---|---|
| Slots | beide `NotFound`, je ein Kandidat pro Slot, hoechste Sequence gewinnt |
| Gleichstand | byteidentisches Duplikat; gleiche Sequence mit anderem Zustand, Epoche oder Bytes fail closed |
| Fehler | `ReadError`, `CapacityError`, CRC-/Envelope-/Typ-/Schemafehler je Slot |
| Schema | unbekanntes neueres Bootstrap- und Speicherformat blockiert |
| High-Water | alte Epoche, verwaister hoeherer Record, Maximum und checked Overflow |
| Write | Success, WriteError, CapacityError, `CommitOutcomeUnknown` alt/neu/unaufloesbar |
| Wiederaufnahme | exakter Record wird unter gleicher Identitaet wiederverwendet; anderer Inhalt nie |

### Factory-Neuheitsorakel

Tabellengetrieben fuer jeden der 19 Slots:

- genau `NotFound` fuer alle Slots erlaubt Factory-Neuheit;
- genau ein vorhandener beliebiger Bytewert blockiert;
- genau ein gueltiger Record anderer Epoche blockiert;
- genau ein unbekanntes Schema blockiert;
- genau ein CRC-/Envelopefehler blockiert;
- genau ein `ReadError` blockiert;
- genau ein `CapacityError` blockiert;
- kein Write findet vor vollstaendig positivem Orakel statt;
- ein Touch-Sentinel ausserhalb der 19 Keys wird nicht gelesen und bleibt
  unveraendert.

### Initialisierung und Cut-Points

Fuer jeden persistenten Write wird aus identischem Ausgang je ein Cut direkt
vor und direkt nach dem atomaren Replace ausgefuehrt:

1. `Initializing`;
2. UserConfiguration;
3. ServiceConfiguration;
4. ProgramCatalog;
5. Manifest;
6. Root;
7. `Initialized`.

Je Phase werden `WriteError`, `CapacityError`, `CommitOutcomeUnknown` mit altem,
neuem und unaufloesbarem Readback sowie Readback-`ReadError` und
`CapacityError` getestet. Reboot-Orakel baut Dienste und fluechtige Runtime
vollstaendig neu auf.

Zusaetzlich:

- unterschiedliche Abbruchpunkte erzeugen keine gemischte Generation;
- verwaiste Vor-Root-Records werden nur fuer exakt denselben Inhalt
  wiederverwendet;
- CRC-korrekte fachlich ungueltige Factorydokumente blockieren;
- falsche Epoche an jeder Referenzkante blockiert;
- unbekanntes Dokument-, Manifest- oder Rootschema blockiert;
- BootstrapSequence-, Dokumentrevisions-, Manifest- und Rootueberlauf wird vor
  Write abgelehnt;
- Root ohne Fallback ist exakt der erwartete Epochenstart;
- `Initialized` wird niemals vor vollstaendig gebundenem Zielroot bestaetigt.

### Normaler Boot

| Fall | Erwartung |
|---|---|
| `Initialized` + gueltiges Active | `RuntimeReady` mit exakt diesem Graphen |
| ungueltiges Active + gueltiger Fallback | `RuntimeReady`, Fallbackdiagnose sichtbar |
| kein nutzbarer Zweig | `ConfigurationUnavailable`, keine Runtime |
| Rootslot unlesbar + anderer aelterer Root gueltig | keine Runtime |
| Root-Gleichstand unterschiedliche Bytes | `ConfigurationIntegrityFailure` |
| Root/Manifest/Dokument andere Epoche | keine Runtime |
| CRC-korrekt fachlich ungueltiges Dokument | Integritaetsfehler, keine Runtime |
| unbekanntes neueres Schema | eigene Unsupported-Diagnose, keine Runtime |
| Rootausgang nach Neustart eindeutig alt/neu | genau kanonischen vollstaendigen Graphen laden |
| Rootausgang weiterhin unklar | fail closed, keine Runtime |
| Runtimevorbereitung scheitert | `RuntimePreparationFailure`, keine Runtime |

### Werksreset und Resetwiederaufnahme

Cut-Points vor/nach:

1. `Resetting`;
2. neue UserConfiguration;
3. neue ServiceConfiguration;
4. neuer ProgramCatalog;
5. neues Manifest;
6. neuer Root;
7. `Initialized`.

Fuer jede Phase werden dieselben Write-/Unknown-/Readbackkombinationen wie bei
Initialisierung getestet. Zusaetzlich:

- `StorageEpoch` und BootstrapSequence werden checked erhoeht;
- Overflow beider Werte blockiert vor `Resetting`;
- nach bestaetigtem `Resetting` wird alte Epoche nie reaktiviert;
- Root der neuen Epoche besitzt keinen Fallback;
- neue Anfangskonfiguration ist vollstaendig oder noch nicht normal
  freigegeben, nie gemischt;
- Neustart nach Root und vor `Initialized` behaelt die neue Epoche;
- wiederholte Wiederaufnahme bleibt idempotent;
- ein zweiter Reset waehrend `Resetting` wird abgelehnt;
- Korruption startet keinen neuen Reset und keinen Factoryfallback;
- alte Records koennen physisch vorhanden bleiben, werden aber nie
  referenziert;
- keine Connectivity-/Authentication-/Secretrecords werden erzeugt.

### Touchkalibrierung

- Sentinel vor Initialisierung, vor Reset und nach jedem Cut byteidentisch;
- Zugriffsjournal beweist null Reads und null Writes auf den Sentinelkey;
- Storefehler anderer Keys aendern den Sentinel nicht;
- erfolgreicher und fehlgeschlagener Reset erhalten ihn;
- kein Produktionskey oder Touchmodell wird aus dem Test abgeleitet.

### Konkurrenz, Runtime und Modellbudget

- #56-Normalmutation haelt die gemeinsame Lease: Initialisierung/Reset liefert
  `ConfigurationMutationBusy` ohne Storezugriff;
- #57 haelt die Lease: normale Mutation erhaelt denselben Busyvertrag;
- aktive Maximal-Konfiguration plus neue Factorygeneration bleibt bei genau
  zwei Vollmodellen;
- belegte zweite Modellposition blockiert vor `Resetting` und jedem Write;
- ein bis acht alte Runtime-Leases bleiben speichersicher;
- Retirement gibt die Modellposition erst nach tatsaechlichem Besitzerende
  frei;
- danach kann der Reset erneut beginnen;
- kein Publish, Preview oder neue Runtimeakquisition waehrend fail closed;
- interner Publishertausch plus `Initialized`-Abschluss gibt nie eine
  Teilgeneration frei;
- keine Allokation, Serialisierung, Validierung oder Storeoperation im
  eigentlichen Snapshottausch;
- kein Deadlock, Use-after-free oder ungeschuetztes Data Race in kontrollierten
  Interleavings.

### Ressourcen und Builds nach Planfreigabe

- maximal ein vollstaendiger Recordarbeitsbereich im Commitworkflow;
- Bootstrapcodec exakt 5 Payload- beziehungsweise 42 Recordbytes;
- Factory-Neuheit exakt 19 begrenzte Reads und keine unbeschraenkte Sammlung;
- maximal zwei Vollmodellgenerationen und acht Runtime-Read-Leases wie #56;
- wiederholte Boot-/Initialisierungs-/Resetzyklen ohne wachsenden Live-Heap;
- Peak-Allokationsbericht fuer maximales ProgramCatalog-Modell und
  Runtime-Handoff;
- vollstaendiges `pio test -e native`;
- Builds `native`, `esp32_bringup`, `esp32_release`;
- Base-/Head-Bericht fuer Hostbinaer, statisches RAM, Flash,
  `firmware.elf` und `firmware.bin` mit identischer Toolchain;
- `clang-format`, `clang-tidy`, Architektur-, PlatformIO-, Quality- und
  Secretpruefungen;
- `git diff --check`.

Reale Heap-, NVS-, Jitter-, Watchdog-, Flashatomizitaets- und
Lebensdauermessungen bleiben offen und werden nicht durch Hosttests ersetzt.

## Geplanter kleiner Commit-Schnitt nach Planfreigabe

Alle Commits bleiben im selben Draft-PR. Jeder Commit baut und besteht die bis
dahin zutreffenden nativen Tests.

### Commit 1 – Bootstrapmodell, Wireformat und redundanter Store

- Record-Type-ID 6, Keys `cb0`/`cb1` und Limits;
- Bootstraptypen, Schema-1-Codec und kanonische Auswahl;
- High-Water, Gleichstand, Read-/Capacity-/Schema-/Integritaetsdiagnosen;
- Bootstrapwrite und Unknown-Aufloesung;
- Golden-, Roundtrip-, Slot- und Negativtests.

### Commit 2 – Factory-Neuheitsorakel und epocheninitialer Graph

- 19-Slot-Factory-Neuheitsorakel;
- schmale `ConfigurationGraphStore`-Erweiterung fuer neuen Graph ohne
  Fallback;
- idempotente Zielrecordwiederverwendung und vollstaendige Vor-Root-Pruefung;
- Dokument-/Manifest-/Root-Write- und Cut-Point-Tests.

### Commit 3 – Runtime-Handoff, normaler Boot und Initialisierung

- schmalen vorbereiteten Recovery-Handoff im bestehenden
  `ConfigurationService`;
- `ConfigurationRecoveryService` und stabile Ergebnisursachen;
- `Initializing`-Wiederaufnahme und `Initialized`-Boot;
- Active-/Fallback-, Unknown-Reboot-, Modell- und Runtimefreigabetests.

### Commit 4 – Werksreset, Toucherhalt und Konkurrenzmatrix

- autorisierten Reset-Eingang und checked Epochenwechsel;
- `Resetting`-Wiederaufnahme an allen Cut-Points;
- Touch-Sentinel-, alte-Epochen-, Koordinator-, Reader- und
  Modellbudgettests;
- keine Secret-/Pending-/Intent-Records.

### Commit 5 – Gesamtnachweis und Dokumentation

- vollstaendige native Matrix und alle drei Buildprofile;
- Base-/Head- und Peak-Ressourcenbericht;
- Spezifikation, Implementierungsuebersicht und Changelog aktualisieren;
- Diff gegen den freigegebenen Plan und verbleibende Messgates dokumentieren.

Neue oder geaenderte Dateien ausserhalb des in diesem Plan genannten Schnitts,
ein neuer Port, eine neue Persistenzidentitaet oder eine geaenderte
Runtime-/Recoverysemantik sind materielle Planabweichungen und erfordern vor
der Aenderung eine neue Planrevision und Ownerfreigabe.

## Dokumentationsaenderungen nach Implementierung

Der Implementierungs-PR dokumentiert mindestens:

- finalen Bootstrap-Typ- und Wirevertrag;
- exakte Slotkeys, Record-Type-ID, Groessen und Sequenzregeln;
- Factory-Neuheits- und Negativorakel;
- Initialisierungs-, Boot- und Reset-Cut-Point-Orakel;
- Runtime-Handoff und fail-closed Ergebnisse;
- Touchkalibrierung als unangetastete Domaene;
- umgesetzte Planpunkte und Commitzuordnung;
- Testgruppen, Base-/Head- und Peak-Ressourcenwerte;
- offene reale Mess- und Safety-Integrationsgates;
- Bestaetigung, dass keine neue Drittkomponente eingebunden wurde.

`Closes #57` wird erst im vollstaendig umgesetzten PR-Kontext ergaenzt, nicht
im Planstatus.

## Offene Entscheidungen und Gates

| Punkt | Status | Behandlung |
|---|---|---|
| Planfreigabe | offen | exakter Ownerkommentar mit Plan-Commit-SHA; bis dahin keine Implementierung |
| Record-Type 6, `cb0`/`cb1`, Bootstrap-Wireformat | vorgeschlagener konkreter Planvertrag | wird erst mit commitgebundener Planfreigabe verbindlich |
| neue externe Bibliothek | nicht erforderlich | jede spaetere Notwendigkeit ist materielle Planabweichung und Ownerfrage |
| NVS/Preferences-Adapter | ausserhalb #57 | ADR-016-Richtung beibehalten; spaetere Adapter-/Hardwarearbeit |
| reales NVS-/Preferences-Backend und Stromunterbruch-Cut-Points | `SPIKE_REQUIRED`, `FINAL_SELECTION_PENDING` | erst im zugeordneten Adapter-/Integrationsschritt; keine Produktauswahl in #57 |
| reale NVS-Kapazitaet und Replace-Atomizitaet | `MEASUREMENT_REQUIRED` | realer ESP32-/Adapterspike |
| absolute Heap-/Flashreserve | `TBD_IMPLEMENTATION_BUDGET`, `MEASUREMENT_REQUIRED` | Base-/Head plus spaetere reale Messung |
| Jitter, Watchdog und Flashlebensdauer | `MEASUREMENT_REQUIRED` | spaeter mit realem Backend und Hardware |
| Plattformverschluesselung | `EVALUATE_BEFORE_RELEASE` | separates Security-Gate, keine Auswahl in #57 |
| produktive Composition Root | `FINAL_SELECTION_PENDING` im zugeordneten Integrationsschritt | keine Verkabelung in #57 |
| Safetywirkung | nachgelagertes `CONFIGURATION_SAFETY_INTEGRATION_GATE` | #24 integriert reale #56/#57-Producer; nicht in #57 |
| physische Resetgeste | `TBD_HARDWARE` | #57 nimmt nur bereits autorisierten fachlichen Auftrag an |
| Inbetriebnahmewerte | kein #57-Wert `TBD_COMMISSIONING` | #57 fuehrt weder Zeit-, Thermik- noch Regelparameter ein |
| reale Connectivity-/Authentication-Domaenen | `FINAL_SELECTION_PENDING` bis erster Konsument | keine vorbereiteten Records oder Ports |

Es wurde kein Widerspruch gefunden, der eine neue ADR oder eine vorgelagerte
Ownerentscheidung ausser der commitgebundenen Freigabe dieses Plans erfordert.
Sollte die Implementierung zeigen, dass der schmale Graphstore- oder
Runtime-Handoff den gemergten #56-Vertrag nicht ohne materielle Aenderung
erfuellen kann, wird angehalten und dieser Befund als neue Ownerfrage in einer
Planrevision dokumentiert.

## Ausdruecklich verbotene Vorwegnahmen

Auch nach Planfreigabe bleiben verboten:

- Produktions- oder Testdateien ausserhalb des genannten Dateischnitts ohne
  neue Planfreigabe;
- Aenderungen an `IStateStore`, Envelope, CRC oder generischer Slotmechanik;
- produktiver NVS-/Preferences-Adapter oder `src/main.cpp`-Verkabelung;
- zweite Mutationskoordination oder allgemeine Transaktionsplattform;
- eigene Flashdatenbank oder allgemeines Persistenzrepository;
- neue externe Bibliothek, Build-, Toolchain- oder Partitionsaenderung;
- Pending, Intent, RunAssessment oder persistente Previewdaten;
- Connectivity-/Authentication-Manifeste, Secret-Roots, Credentialslots oder
  `CredentialEpoch`;
- Lauf-, Journal-, Historien- oder Importpersistenz;
- #17-, #24-, UI-, Auth-, PIN-, Raw-Touch- oder Safetyimplementierung;
- automatische Factoryinitialisierung bei irgendeinem Befund ausser exakt 19
  erfolgreichen `NotFound`-Reads;
- automatischer Werksreset bei Korruption;
- Rollback oder Reaktivierung einer alten Epoche nach bestaetigtem
  `Resetting`;
- Behauptung physischer Loeschung, realer Flashatomizitaet oder Hardwarebudgets;
- Lesen, Schreiben, Loeschen oder Rotieren der Touchkalibrierung;
- PR auf Ready setzen, mergen, Auto-Merge aktivieren, force-pushen oder Branch
  loeschen.

## Abnahmekriterien des Plan-PRs

Die Planungsphase ist abgeschlossen, wenn:

- aktuelles `main`, Live-Issues, Regeln, ADRs, Spezifikationen und
  Adopt-or-build-Register geprueft sind;
- #16/#56/#57 ausschliesslich innerhalb der ausdruecklich freigegebenen
  Metadatensynchronisierung aktualisiert und kommentiert wurden;
- diese Datei die einzige Repositoryaenderung ist;
- Wiederverwendung und eigener Anteil je benoetigter Funktion dokumentiert
  sind;
- Bootstraprecord, Wireformat, Slots, High-Water und Gleichstaende eindeutig
  geplant sind;
- Factory-Neuheits-, Initialisierungs-, Boot- und Resetorakel jeden
  persistenten Cut abdecken;
- gemeinsame Mutationskoordination und #56-Runtime-Handoff ohne zweiten
  Mechanismus festgelegt sind;
- Touchkalibrierung nachweislich unangetastet bleibt;
- #17 und #24 weder vorgezogen noch implementiert werden;
- keine Secret-, Pending-, Intent- oder allgemeine Persistenzinfrastruktur
  vorgesehen ist;
- Test-, Ressourcen-, Dokumentations- und offene Messgates vollstaendig sind;
- `git diff --check`, Link-, Tabellen-, Schreibweisen- und Secretpruefung der
  Plan-Datei bestanden sind;
- genau ein aktueller Plan-Commit gepusht ist;
- der Draft-PR Plan-Datei, exakten Plan-SHA, offene Entscheidungen,
  `IMPLEMENTATION_BLOCKED_PENDING_PLAN_APPROVAL` und den fehlenden
  Implementierungsstand nennt;
- PR und Branch weder gemergt noch auf Ready gesetzt oder geloescht werden.

Nach Commit und Push haelt der Agent an und wartet auf die exakte
commitgebundene Ownerfreigabe.

In dieser Planungsphase wurden bewusst keine Tests, Builds, Spikes oder
Produktionsmessungen ausgefuehrt; die ausgefuehrten Pruefungen betreffen nur
diese Markdown-Plan-Datei.
