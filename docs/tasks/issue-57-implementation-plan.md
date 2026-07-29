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
- Ueberholte Planstaende:
  `fb4e4f39e080cb472c85ed14c2e8e136d0f2083e` und
  `f313e9f82c07843714b9f36e9a885ca73046ff83`; diese Gesamtkorrektur
  benoetigt einen neuen commitgebundenen Ownerkommentar.

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

- `test/test_configuration_codecs/test_configuration_codecs.cpp`
  - Copy-Migration laesst die vollstaendig erfasste Quelle byte- und
    wertgleich unveraendert;
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

### Objekt- und Instanzinvarianten

Ein gueltiger Produktionsaufbau besitzt genau eine
`ConfigurationMutationCoordinator`-Instanz fuer #56 und #57 sowie genau eine
logische `IStateStore`-Instanz fuer Bootstrapstore, Factory-Neuheitsorakel und
Graphstore. `ConfigurationRecoveryService` wird ueber eine schmale statische
Factory konstruiert, die Referenzen auf den vorhandenen Graphstore,
`ConfigurationService`, Koordinator und `ITimeZoneResolver` prueft und nur bei
identischen, kompatiblen Abhaengigkeiten einen nicht kopierbaren Dienstbesitz
liefert. Die dafuer benoetigten internen Identitaetsabfragen sind konkrete
Friend-/Private-Hilfen der bestehenden Klassen, kein oeffentliches Provider-
oder Contextframework.

Verbindlich gilt:

- Bootstrapstore und Factory-Neuheitsorakel verwenden dieselbe Store-Referenz;
- der Recoverydienst und `ConfigurationService` verwenden dasselbe
  `ConfigurationGraphStore`-Objekt, dieselbe Koordinatorinstanz und kompatible
  Runtime-/Zeitzonenvorbereitung;
- Split-Store- oder Split-Coordinator-Komposition wird bei der Konstruktion
  typisiert abgelehnt und kann keine Recoveryoperation beginnen;
- die spaetere Composition Root bleibt ausserhalb #57; lokale native Tests
  beweisen die Konstruktionsinvariante mit passenden und absichtlich
  verschiedenen Instanzen;
- es entsteht weder ein allgemeines Repository-/Providerframework noch eine
  zweite Mutationskoordination.

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

### Formale Schema-1-Historienrelation

Schema 1 besitzt absichtlich eine geschlossene mathematische Zuordnung, damit
auch ein einzelner nach Stromausfall verbliebener gueltiger Bootstraprecord
ohne fluechtige Historie als moeglicher oder unmoeglicher Zustand bewertet
werden kann. Fuer `StorageEpoch E` und `BootstrapSequence S` gilt exakt:

```text
Initializing: E = 1 und S = 1
Resetting:    E >= 2 und S = 2 * E - 1
Initialized:  E >= 1 und S = 2 * E
```

Multiplikation, Addition und Subtraktion erfolgen checked. Daraus folgt eine
Schema-1-Obergrenze fuer die Epoche; ein Wert, fuer den `2 * E` nicht mehr als
`uint64_t` darstellbar ist, kann nicht `Initialized` werden und blockiert den
vorhergehenden Reset vor seinem ersten Write mit `CounterOverflow`.

Die einzig erlaubten persistenten Fortschreibungen sind:

| Ausgang | Ziel | Epochenrelation | Sequenzrelation |
|---|---|---|---|
| kein Bootstrap und nachweislich fabrikneu | `Initializing` | `1` | `1` |
| `Initializing(E=1,S=1)` | `Initialized(E=1,S=2)` | gleich | exakt `S + 1` |
| `Initialized(E,S=2*E)` | `Resetting(E+1,S+1)` | checked naechste Epoche | exakt `2*(E+1)-1` |
| `Resetting(E,S=2*E-1)` | `Initialized(E,S+1)` | gleich | exakt `2*E` |

`Initializing` ist ausserhalb von Epoche 1/Sequence 1 unzulaessig. Es gibt
keine direkte Fortschreibung `Initializing -> Resetting`,
`Resetting -> Resetting`, `Initialized -> Initialized` oder einen Sprung ueber
einen Zustand. Idempotente Wiederaufnahme schreibt keinen neuen Zustand,
sondern verwendet nur exakt dieselben kanonischen Bytes derselben Identitaet.

Sind beide Slots vorhanden und nicht byteidentische Duplikate, muessen der
niedrigere und der hoehere Kandidat in der Tabelle eine unmittelbar erlaubte
Fortschreibung bilden: Sequenzdifferenz exakt 1, keine Epochenregression und
die dort definierte Zustandsfolge. Eine Luecke, Regression, hoehere Sequence
mit kleinerer Epoche, kleinere Sequence mit scheinbar neuerer Epoche oder eine
sonst widerspruechliche Kombination ist
`ConfigurationIntegrityFailure`, selbst wenn jeder Einzelrecord Envelope und
CRC technisch besteht. Ein einzelner Kandidat muss mindestens die
mathematische Zuordnung erfuellen.

Ein byteidentisches Duplikat derselben Epoche, Sequence und kanonischen Bytes
darf deterministisch mit Duplikatdiagnose gewaehlt werden. Gleiche Identitaet
mit abweichenden Bytes bleibt Integritaetsfehler. Ein unbekanntes neueres
Schema wird vor dieser Relation erkannt, bleibt fail closed und wird niemals
mit Schema-1-Regeln interpretiert.

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
5. Jeder Schema-1-Kandidat muss die formale Einzelrecordrelation erfuellen.
6. Zwei unterschiedliche Kandidaten muessen zusaetzlich eine unmittelbar
   erlaubte Historienfortschreibung bilden.
7. Erst unter danach vollstaendig gueltigen Kandidaten gewinnt die hoechste
   BootstrapSequence unabhaengig vom Slot.

Gleichstand:

- gleiche BootstrapSequence und exakt identische kanonische Recordbytes:
  deterministische Auswahl des kleineren Slots mit Duplikatdiagnose;
- gleiche BootstrapSequence, aber unterschiedliche kanonische Bytes, Epoche,
  Formatversion oder Zustand: `ConfigurationIntegrityFailure`, keine
  Runtimefreigabe und kein Slot-Tiebreak als fachliche Entscheidung.

### High-Water und naechster Wert

Alle vollstaendig technisch, fachlich und historisch gueltigen
Bootstraprecords tragen zu einem globalen BootstrapSequence-High-Water bei,
auch der unmittelbar vorherige Record. Ein isolierter Record mit formal
unmoeglicher Epochen-/Sequence-/Zustandskombination ist nicht verwaist
brauchbar, sondern Integritaetsfehler. Der naechste Wert wird aus der einzig
erlaubten Zielrelation abgeleitet und muss zugleich checked `max + 1` sein.

- 0 ist reserviert.
- `UINT64_MAX` blockiert vor jedem Write.
- Unbekanntes neueres Schema, Read-/Capacity- oder Integritaetsfehler macht den
  High-Water unbekannt und blockiert die Mutation.
- Eine bestehende Identitaet darf nur fuer exakt dieselben kanonischen
  Recordbytes wiederverwendet werden, wenn ein unterbrochener Ablauf genau
  diesen Zustand idempotent fortsetzt.
- Unterschiedliche Inhalte erhalten niemals dieselbe BootstrapSequence.
- Ein Wert groesser als der kanonische Record darf nur dessen exakt erlaubter
  direkter Nachfolger sein; Luecken oder fremde Historien blockieren.

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

Vor dem ersten Bootstrapscan wird dieselbe gemeinsame
`ConfigurationMutationLease` erworben, die bis zum bestaetigten
`Initializing` oder zum Abbruch gehalten bleibt. Automatische Initialisierung
ist nur erlaubt, wenn genau alle folgenden bekannten Konfigurationsslots unter
dieser Lease, derselben Dienstrevision und derselben nicht wiederverwendbaren
Orakelinstanz gelesen wurden und jeder einzelne `NotFound` liefert:

- 2 Bootstrap-Slots: `cb0`, `cb1`;
- 2 Rootslots: `cr0`, `cr1`;
- 3 Manifest-Slots: `cm0`, `cm1`, `cm2`;
- 4 UserConfiguration-Slots: `uc0` bis `uc3`;
- 4 ServiceConfiguration-Slots: `sc0` bis `sc3`;
- 4 ProgramCatalog-Slots: `pc0` bis `pc3`.

Das sind insgesamt genau 19 eindeutige, begrenzte Key-Reads. Die zwei zuerst
gelesenen Bootstrapbeobachtungen werden als gebundene Eingabe in das Orakel
uebernommen; danach werden nur die verbleibenden 17 Root-, Manifest- und
Dokumentslots je einmal gelesen. Die Bootstrapslots werden innerhalb desselben
Nachweises nicht erneut gelesen. Die gebundenen Beobachtungen koennen weder in
einem zweiten Versuch noch unter einer anderen Lease oder Dienstrevision
verwendet werden. Es gibt weder Listing noch einen unbeschraenkten
Namespace-Scan.

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

### Phasenbesitz und unumgehbare persistente Grenzen

Vorplanung, Graphwrite, Rootcommit und Runtimepublish werden durch vier
verschiedene nicht kopierbare, intern konstruierbare Besitze getrennt:

1. `PreparedInitialConfigurationGraph` ist ausschliesslich ein nicht
   ausfuehrbarer Vorbereitungsbesitz. Er darf geteilte typisierte
   Factorymodelle, Zielidentitaeten, kleine Slot-/Recorddeskriptoren,
   Modell-/Recordreservierungen und eine fallibel vorbereitete Runtime halten,
   aber weder Storewrites noch einen Publisherwechsel ausloesen. Bei
   Initialisierung entsteht er nach gebundener Factory-Neuheit, beim Reset in
   `ResetPreparing` beziehungsweise dem resetberechtigten No-Runtime-Modus.
2. Die interne `ConfigurationEpochGraphWriteCapability` entsteht bei
   Initialisierung erst nach exakt bestaetigtem
   `Initializing(Epoch 1, Sequence 1)` und beim Reset erst nach exakt
   bestaetigtem `Resetting` der Zielepoche. Sie bindet Bootstrapslot,
   kanonische Bootstrapbytes, Zustand, Epoche, Sequence, Planidentitaet,
   Dienstrevision und dieselbe gehaltene `ConfigurationMutationLease`. Sie ist
   genau einmal verwendbar. Ohne sie ist kein Dokument-, Manifest- oder
   Rootwrite des epocheninitialen Pfads aufrufbar.
3. Die vorbereitete Runtimeaktivierung ist vor und waehrend des Rootwrites
   nicht publish-faehig. Sie bindet lediglich den vollstaendig vorbereiteten
   Snapshot an die erwartete Root-/Manifest-/Graphidentitaet.
4. Erst ein eindeutig neu committedes Rootergebnis und ein danach
   vollstaendig verifizierter Zielgraph wandeln diesen Besitz intern in eine
   `CommittedRecoveryActivation` um. Nur dieser an Rootslot, kanonische
   Rootbytes, Graphidentitaet, Epoche, Planidentitaet und Dienstrevision
   gebundene Einmalbesitz darf den nicht fehlschlagenden Publisherwechsel
   ausloesen. Normale Runtimeakquisition bleibt bis zum exakt bestaetigten
   Bootstrapzustand `Initialized` gesperrt.

Konstruktoren und Konvertierungen bleiben private beziehungsweise Friend-
gebunden. Fremde, stale, doppelt verwendete oder fuer einen anderen Plan,
Bootstrap, Root oder eine andere Epoche ausgestellte Besitze werden ohne
Store- oder Publishwirkung abgelehnt. Produktionscode kann keinen dieser
Besitze frei konstruieren. Factory-Neuheit allein autorisiert damit niemals
einen Graphwrite und ein vorbereiteter Snapshot niemals einen Publish.

### Deskriptor- und Ressourcenbesitz

`PreparedInitialConfigurationGraph` haelt die zwischen Vorbereitung und
Runtime-Snapshot geteilten typisierten Factorymodelle sowie feste kleine
Deskriptoren je Zielrecord: Dokumentart, Slot, Epoche, Identitaet, Schema,
Payloadlaenge, CRC und Referenz. Er haelt nicht gleichzeitig alle
Dokument-, Manifest- oder Rootrecordstrings.

Der Ressourcenvertrag unterscheidet ausdruecklich:

- das geteilte typisierte `ProgramCatalog`-Vollmodell und den daraus
  vorbereiteten Snapshot innerhalb des bestehenden Zwei-Modell-Budgets;
- hoechstens eine kanonische ProgramCatalog-Payload;
- hoechstens einen application-held vollstaendigen maximalen
  Dokument-Envelopeworkspace;
- einen Store-Readbackwert, der erst erzeugt wird, nachdem der erwartete
  Envelopeworkspace seine Bytes und Kapazitaet mit
  `std::string{}.swap(workspace)` nachweislich freigegeben hat;
- kleine, fest begrenzte kanonische Manifest-/Root-/Bootstrapbytes und deren
  Deskriptoren, separat vom maximalen Dokumentrecord;
- einen Indeterminate-Kontext, der nur kleine erwartete Root- oder
  Bootstrapbytes und Identitaeten, niemals einen zweiten maximalen
  Dokumentrecord, haelt.

Vor jedem neuen `encodeEnvelope()` ist der Ausgabestring frisch leer und
besitzt keine als Vollrecord zaehlende Restkapazitaet. Ein blosses `clear()`
genuegt nicht. Nach Write und vor Readback wird der erwartete Vollrecord auf
kleine Laengen-/CRC-/Identitaetsdeskriptoren reduziert und seine Kapazitaet per
Swap mit einem leeren String freigegeben. Der Readback wird mit vorhandenem
Envelope-/CRC- und Schema-Codec sowie bei `ProgramCatalog` recordweise gegen
das geteilte typisierte Factorymodell geprueft; dadurch entsteht kein zweites
Katalogmodell. Payload-, Envelope- und Readbackphasen laufen sequenziell, und
vor dem naechsten maximalen Record wird auch die Payloadkapazitaet nachweislich
freigegeben.

`encodeEnvelope()` darf intern gemaess #54 genau einen neuen Record aufbauen
und per `swap()` veroeffentlichen. Der #57-Aufrufer verhindert durch den leeren
Outputhalter, dass dabei zugleich ein alter Vollrecord gehalten wird;
`device_platform` wird nicht geaendert. Kleine Root-/Manifestrecords duerfen
parallel zu Deskriptoren leben, wachsen aber niemals auf ProgramCatalog-
Groesse. Instrumentierte Tests bilanzieren gleichzeitig lebende Groessen und
Kapazitaeten statt nur Stringobjekte.

Die zulaessige Gleichzeitigkeit fuer den maximalen Katalogrecord ist exakt:

| Phase | Gleichzeitig zulaessiger Besitz |
|---|---|
| Modell-/Runtimevorbereitung | geteiltes typisiertes Factorymodell und vorbereiteter Snapshot innerhalb der Zwei-Modell-Grenze; kein Payload-/Envelopeworkspace |
| Payloadkodierung | geteiltes Modell plus genau eine kanonische Payload; kein alter Envelope oder Readback |
| Envelopekodierung | geteiltes Modell, eine Payload und der interne neue #54-Encoderpuffer; der application-held Outputhalter ist leer und kapazitaetsfrei |
| Write | geteiltes Modell plus genau ein application-held Envelope; die Payloadkapazitaet ist zuvor freigegeben |
| Readback | geteiltes Modell plus genau ein Store-Readback-Envelope; der erwartete Envelopeworkspace ist zuvor freigegeben |
| fachliche Readbackpruefung | geteiltes Modell, ein Readback-Envelope und hoechstens ein begrenzter Programrecord-Scratch; kein zweites `ProgramCatalog`-Vollmodell |
| Root-/Manifest-/Indeterminate-Phase | geteilte Modelle plus fest begrenzte kleine Graph-/Bootstrapbytes; kein maximaler Dokumentrecord im Kontext |

### Wiederverwendung der #56-Graphbausteine

Neue interne Hilfsfunktionen werden nur aus vorhandenen #56-Operationen
extrahiert beziehungsweise von beiden Pfaden gemeinsam verwendet:

| Interne Hilfsverantwortung | Bestehender Baustein | Konkreter #57-Wiederverwendungszweck |
|---|---|---|
| Identitaets-, Schema-, Read-/Capacity- und Kollisionsscan | #56 `validationScan()` und gebundene Recordscans | Zielepochen-Slots vor jedem Write pruefen, ohne normalen Active vorauszusetzen |
| kanonische Dokumentkodierung und fachliche Validierung | #55-Dokumentcodecs sowie #56-Inhaltsvergleich | Factoryrecord nacheinander in den einen Arbeitsbereich kodieren und an den Deskriptor binden |
| exakter Write-/Readbackvertrag | #56-Writehilfen auf `IStateStore` | `Success`, garantierte Fehler und `CommitOutcomeUnknown` fuer jeden Zielrecord unveraendert behandeln |
| Manifest-/Referenzpruefung | #56-Manifestcodec und Zielgraphvalidierung | neue und wiederverwendete Referenzen der Zielepoche vollstaendig pruefen |
| Rootwrite und Aufloesung | #56 Rootcodec und `resolveCommitDetailed()`-Semantik | exakte Zielrootbytes zuerst, danach gesamten Zielgraph alt/neu/unaufloesbar bestimmen |
| vollstaendiger Graphload | #56 kanonischer Loader/`ValidationOnly`-Pfad | Boot und Vor-Root-Pruefung ohne zweite Graphinterpretation |

Falls eine dieser Verantwortungen nicht ohne duplizierten allgemeinen
Commitworkflow teilbar ist, ist dies eine materielle Planabweichung. Dann wird
vor Codeaenderung angehalten und der Plan erneut ownerfreigegeben.

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
gescannt und der vollstaendige Zielgraph ueber begrenzte Deskriptoren geplant.
Fuer jede Identitaet gilt:

- existieren mehrere byteidentische, technisch und fachlich gueltige
  Zielrecords, wird deterministisch der kleinste passende Slot gebunden und
  kein weiteres Duplikat geschrieben;
- existieren gleiche Identitaeten mit unterschiedlichen kanonischen Bytes,
  blockiert `ConfigurationIntegrityFailure` jede Wiederverwendung und jeden
  Write;
- fehlt der Zielrecord, wird deterministisch der kleinste verfuegbare Slot
  gebunden und in der festen Reihenfolge User, Service, ProgramCatalog,
  Manifest, Root ergaenzt.

`Verfuegbar` ist kein Synonym fuer beliebig ueberschreibbar. Vor
`Initializing` beziehungsweise `Resetting` findet kein Graphwrite statt. Nach
der bestaetigten Epochengrenze sind nur `NotFound` oder vollstaendig gelesene
Records einer anderen Epoche overwrite-faehig; exakte Records der Zielepoche
werden idempotent wiederverwendet. Korrupte, unlesbare, kollidierende oder
unbekannte Zielepochenkandidaten sind niemals exakte Zielrecords und duerfen
nicht als freie Slots behandelt werden. Reicht die so bewiesene sichere
Slotmenge nicht, endet der Ablauf fail closed, ohne Loesch-, Reparatur- oder
Factory-Fallbackvertrag. Nicht referenzierte Altbytes duerfen physisch
bestehen bleiben.

Der Plan bindet jede Zielreferenz vor dem ersten Graphwrite. Bei Wiederaufnahme
werden vorhandene exakte Zielrecords gefunden und wiederverwendet; fehlende
Records werden mit identischen kanonischen Bytes vervollstaendigt. Der Ablauf
erzeugt keine zweite Generation, keine gemischte Epoche und keinen Fallback.

Ein unterbrochener Ablauf vergibt fuer denselben Zielinhalt keine neue
Identitaet. Er scannt erneut, bindet den deterministisch kleinsten exakten
Zielrecord und schreibt nur weiterhin fehlende Records. Die
Graphstore-Erweiterung folgt dem oben definierten sequenziellen Payload-,
Envelope- und Readbackbesitz und teilt die typisierten Factorymodelle zwischen
Prepared Graph und Runtime-Snapshot.

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
  exakt neu; unaufloesbarer Readback ist ein eigener redigierter
  `ConfigurationRecordOutcomeIndeterminate` unter
  `ConfigurationUnavailable`, beendet den Versuch fail closed und behauptet
  weder Bootstrap- noch Rootcommit;
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
- ein unbestimmter Rootwrite verwendet unveraendert den bestehenden #56-
  Vertrag `ConfigurationCommitIndeterminate` mit
  `resolveCommitDetailed()`; er wird nie als `BootstrapCommitIndeterminate`
  umbenannt;
- es gibt keinen automatischen Rollback und keine Reaktivierung einer alten
  Epoche.

### Dreistufiger `CommitOutcomeUnknown`-Vertrag

| Ebene | Gebundene Aufloesung | Dienstmodus/Fortsetzung | Runtimewirkung | Oeffentlicher Status | #57-Safety-Producer |
|---|---|---|---|---|---|
| Bootstraprecord (`Initializing`, `Resetting`, `Initialized`) | Zielslot exakt alt/nicht wirksam, exakt neu oder fremd/unlesbar | nur der an Bootstrapidentitaet und Lease gebundene Ablauf darf fortsetzen; unaufloesbar -> `RuntimeFailure` mit Bootstrapkontext | gemaess Bootstrapgrenze; nie Rootpublish durch diesen Status | `BootstrapCommitIndeterminate` nur im unaufloesbaren Fall | `ConfigurationUnavailable`, bei nachgewiesener Bootstrapintegritaetsverletzung `ConfigurationIntegrityFailure` |
| Dokument/Manifest vor Root | exakter Readback alt, exakt neu oder unaufloesbar | alt/neu darf nur denselben Epochenplan fortsetzen; unaufloesbar -> `RuntimeFailure` mit Record-/Planidentitaet und ohne Slotfreigabe | kein Root wird behauptet, keine Runtime | `ConfigurationRecordOutcomeIndeterminate` im unaufloesbaren Fall | `ConfigurationUnavailable`, bei nachgewiesener Kollision/Integritaet `ConfigurationIntegrityFailure` |
| Root | vorhandener #56-Zielslot- und Vollgraphscan ueber `resolveCommitDetailed()` | bestehender #56-Modus `CommitIndeterminate`; alt, neu und fremd/unaufloesbar bleiben getrennt | nur exakt neuer Root plus vollstaendig gueltiger Zielgraph kann `CommittedRecoveryActivation` erzeugen | vorhandener `ConfigurationCommitIndeterminate`-/Resolutionstatus | `ConfigurationUnavailable` beziehungsweise ursachentreu `ConfigurationIntegrityFailure` |

Bootstrap-Unknown erzeugt niemals einen #56-Rootcommitstatus. Ein Vor-Root-
Record-Unknown behauptet niemals einen persistenten Root. Ein Root-Unknown
wird weder umbenannt noch durch eine Bootstrapursache verdeckt. Der redigierte
#57-Ergebnisvertrag darf die Phase nennen, reicht aber den realen #56-
Rootproducer unveraendert an das spaetere #24-Integrationsgate weiter.

## Runtime-Handoff und Modellbudget

### Vorbereiteter Handoff

Der `ConfigurationService` erhaelt einen schmalen internen
vorbereiteten Recoverybesitz. Er ist:

- nur durch `ConfigurationRecoveryService` unter gehaltener gemeinsamer
  Mutationslease erzeugbar;
- nicht kopierbar, aber ausdruecklich noch nicht publish-faehig;
- an den exakten Zielgraphen, die `StorageEpoch`, Manifestreferenz,
  Runtimebindungsidentitaet und eine Zustandsrevision gebunden;
- vollstaendig fallibel vorbereitet, bevor der Zielroot geschrieben wird;
- Teil des bestehenden Zwei-Modell- und Acht-Reader-Budgets;
- frei von Bootstrap-, Reset- oder Storesemantik.

Der Besitz verwendet intern dieselbe Snapshotvorbereitung wie #56. Erst die
vollstaendige Aufloesung des Rootwrites als exakt neu und die erneute
vollstaendige Zielgraphpruefung erzeugen daraus die
`CommittedRecoveryActivation`. Es gibt keinen zweiten Runtime-Snapshot-Typ
und keinen zweiten Publisher.

### Modell- und Lesergrenzen

- Beim ersten Boot existiert hoechstens das neue Factorymodell.
- Beim Werksreset existieren hoechstens die alte aktive und die vorbereitete
  neue Modellgeneration.
- Ist die zweite Modellposition durch Preview, Commit, eine bereits retired
  Generation oder deren alte Read-Leases belegt, wird der Reset vor
  `Resetting` und vor jedem Write mit `ConfigurationModelBudgetBusy`
  abgelehnt. Ein bis acht Leases der aktuell aktiven Generation belegen keine
  dritte Generation und duerfen in den gebundenen Retirementbesitz uebergehen.
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

## Gemeinsame ConfigurationService-/Recovery-Zustandsmaschine

### Geplante konkrete Dienstmodi

Der bestehende `ConfigurationServiceMode` wird fuer #57 erweitert, ohne die
#56-Modi `CommitInProgress` und `CommitIndeterminate` umzudeuten. Die
Recoveryintegration besitzt genau folgende zusaetzliche Modi und
Zugriffsgrenzen:

| Dienstmodus | Persistente Bedeutung | Neue Runtimeakquisition | Preview | normale Mutation | Erlaubter naechster Modus |
|---|---|---|---|---|---|
| `NoRuntime` | noch kein bestaetigter Bootstrap/Graph gebunden | gesperrt | gesperrt | gesperrt | `RecoveryPreparing` oder `RuntimeFailure` |
| `RecoveryPreparing` | Factory-Neuheit, `Initializing` oder Bootgraph wird fallibel geprueft; vor bestaetigtem `Initializing` besteht nur nicht ausfuehrbarer Vorbereitungsbesitz | gesperrt | gesperrt | nur gebundene Recoveryfortsetzung; Graphwrite erst nach Bootstrapgrenze | bleibt gleich, `BootstrapFinalizationPending`, `Operational` bei reinem bestaetigtem Boot oder `RuntimeFailure` |
| `Operational` | Bootstrap `Initialized`, Runtime exakt an dessen Epoche und kanonischen Graph gebunden | erlaubt innerhalb Readerlimit | erlaubt | #56 normal erlaubt; Reset darf `ResetPreparing` beginnen | `CommitInProgress`, `ResetPreparing`, `RuntimeFailure` |
| `CommitInProgress` | #56-Normalmutation exklusiv in Arbeit | gesperrt; alte Leases bleiben sicher | gesperrt | nur erfasster #56-Commit | `Operational`, `CommitIndeterminate` oder `RuntimeFailure` gemaess #56 |
| `CommitIndeterminate` | #56-Rootausgang noch nicht eindeutig aufgeloest | gesperrt | gesperrt | nur gebundene #56-Aufloesung | bleibt gleich, `Operational` oder `RuntimeFailure` gemaess #56 |
| `ResetPreparing` | alter Bootstrap/Graph weiterhin kanonisch; noch kein bestaetigtes `Resetting` | gesperrt, bereits gehaltene Leases bleiben speichersicher | Installation gesperrt und bestehende Previewbesitze werden kontrolliert geleert/widerrufen | nur Resetvorbereitung unter gemeinsamer Lease | bleibt gleich, `Operational`, `EpochResetting` oder `RuntimeFailure` |
| `ResetEligibleNoRuntime` | kanonischer unterstuetzter Bootstrap ist `Initialized`, Epoche und High-Water sind eindeutig, aber der alte Graph ist unavailable oder unbrauchbar | gesperrt | gesperrt | nur ausdruecklich autorisierte Resetvorbereitung unter derselben Lease | bleibt gleich, `EpochResetting` oder ursachentreu `RuntimeFailure` |
| `EpochResetting` | `Resetting` der checked naechsten Epoche ist bestaetigt; alte Epoche irreversibel verlassen | gesperrt | gesperrt | nur exakt gebundene Resetfortsetzung | bleibt gleich, `BootstrapFinalizationPending` oder `RuntimeFailure` |
| `BootstrapFinalizationPending` | neuer Root und interner Snapshotpublish sind bestaetigt; passendes `Initialized` fehlt oder ist noch unbestimmt | gesperrt | gesperrt | nur exakt gebundener Bootstrapabschluss | bleibt gleich, `Operational` derselben neuen Epoche oder `RuntimeFailure` |
| `RuntimeFailure` | stabil fail closed; ein separat klassifizierter resetberechtigter No-Runtime-Befund ist nicht mit einem beliebigen Fehler gleichgesetzt | gesperrt | gesperrt | gesperrt, ausser einem im #56-Vertrag ausdruecklich erlaubten gebundenen Recoveryversuch | nur vertraglich erlaubte Recovery, erneute persistente Klassifikation oder Neustart/#57-Rekonstruktion |

`Operational` nach erfolgreichem Reset ist derselbe Modus, aber eine neue,
unverwechselbar an `StorageEpoch`, Root, Manifest und Runtimegeneration
gebundene Dienstgeneration. Es gibt keinen Modus, in dem die alte und die neue
Epoche zugleich normal freigegeben sind.

Ein neu konstruierter `ConfigurationService` beginnt in `NoRuntime`, nicht in
einem scheinbar bereits klassifizierten `RuntimeFailure`. Erst der gebundene
Boot-/Recoverypfad entscheidet zwischen `Operational` und einer stabilen
Fehlerursache. Bestehende #56-Tests, die bislang direkt `initialize(...)`
aufrufen, werden auf einen testintern erzeugten gueltigen Bootbesitz
umgestellt; der Produktions-Bypass bleibt dennoch unmoeglich.

`ResetEligibleNoRuntime` entsteht niemals durch einen direkten Wechsel aus
einem beliebigen `RuntimeFailure`. Er wird nur durch einen neuen vollstaendigen
Bootstrapscan klassifiziert: beide Bootstrapslots sind eindeutig lesbar,
Schema und Speicherformat unterstuetzt, der kanonische Zustand ist
`Initialized`, aktuelle Epoche und BootstrapSequence-High-Water sind bestimmt,
und es liegt weder `Initializing`, `Resetting` noch ein unbestimmter
Bootstrapwrite vor. Nur der Graph darf fehlen oder integritaetsfehlerhaft sein.
Bootstrapkorruption, -kollision, unbekanntes Format sowie Read-/Capacityfehler
blockieren diese Klassifikation.

### Produktions-Bypass wird geschlossen

Der heutige oeffentliche Pfad
`ConfigurationService::initialize(const LoadedConfigurationGraph&)` wird im
#57-Dateischnitt aus der frei aufrufbaren Produktionsschnittstelle entfernt.
Die gemeinsame Snapshotvorbereitung bleibt als interne Implementierung
erhalten. Ein normaler Runtimepublish ist danach nur noch moeglich durch:

1. einen bereits `Initialized` bestaetigten Bootstrap-/Bootbesitz; oder
2. eine nicht kopierbare, genau einmal verwendbare
   `CommittedRecoveryActivation` fuer den exakt gebundenen, eindeutig
   committeden und vollstaendig verifizierten Initialisierungs-/Resetroot.

Der Handoff ist nur durch `ConfigurationRecoveryService` unter derselben
`ConfigurationMutationLease` erhaeltlich und bindet mindestens
Dienstzustandsrevision, `StorageEpoch`, Rootslot und kanonische Rootbytes,
Manifestreferenz, Graphidentitaet, Runtimebindungsidentitaet sowie die
vorbereitete Modellgeneration. Seine Konstruktoren bleiben intern/private;
Tests erhalten nur den vorhandenen schmalen Friend-Testzugang. Stale, fremde,
bereits verwendete oder doppelt publizierte Handoffs werden vor Publish ohne
Zustands- oder Storewirkung abgelehnt.

Auch `PreparedInitialConfigurationGraph` wird kein frei aufrufbarer zweiter
Commitpfad. Seine Erzeugung nach Factory-Neuheit ist nur Vorbereitung. Seine
Ausfuehrung verlangt die nicht kopierbare interne
`ConfigurationEpochGraphWriteCapability`, die ausschliesslich nach exakt
bestaetigtem `Initializing` beziehungsweise `Resetting` ausgestellt wird.
`ConfigurationGraphStore` kann weder selbst einen Bootstrapzustand erfinden
noch eine beliebige Zielepoche initialisieren.

### Zustandsrevision und Revisionsreserve

Jeder Moduswechsel und jede Aenderung der gebundenen Recoverygeneration
erhoeht `stateRevision` unter `stateMutex_` checked. Kein direkter Assignment-
Bypass ist erlaubt. Bereits der Eintritt in `RecoveryPreparing` oder
`ResetPreparing` muss die erste Erhoehung erfolgreich abschliessen.

Unmittelbar vor dem ersten persistenten Bootstrapwrite wird ein Headroom von
mindestens drei weiteren Zustandsrevisionen nachgewiesen. Dieser deckt fuer
den laengsten Resetpfad exakt ab:

1. `ResetPreparing -> EpochResetting`;
2. `EpochResetting -> BootstrapFinalizationPending`;
3. `BootstrapFinalizationPending -> Operational` oder alternativ den
   fail-closed Wechsel zu `RuntimeFailure`.

Initialisierung benoetigt nicht mehr Schritte, verwendet aber dieselbe feste
Reserve. Jeder in-process Wiederaufnahmeversuch prueft vor seinem ersten
falliblen Schritt den fuer seine verbleibenden Moduswechsel erforderlichen
Headroom. Reicht die Reserve nicht, erfolgt vor jedem Write der typisierte
`CounterOverflow`; ein unerwarteter Ueberlauf nach einer persistenten Grenze
ist `ServiceStateInvariantViolation`, liefert niemals Erfolg und bleibt fail
closed.

### Resetvorbereitung, Preview- und Modellbesitz

Der Eintritt `Operational -> ResetPreparing` beziehungsweise die gebundene
Klassifikation `NoRuntime -> ResetEligibleNoRuntime` ist der fluechtige
Linearisierungspunkt, ab dem neue Preview-Builds, Previewinstallationen,
Bestätigungen, normale Mutationen und neue Runtimeakquisitionen abgelehnt
werden. Unter dem kurzen Zustandsmutex werden:

- sichtbare `NoChange`- und Changed-Previews identitaetsgebunden entfernt;
- laufende Preview-Build-Reservierungen widerrufen, aber ihr Modellbudget erst
  nach tatsaechlichem Besitzende freigegeben;
- erfasste Preview-/Commitbesitze, bestehendes Retirement und alte
  Modellgenerationen bilanziert;
- neue Installationen durch Zustandsrevision und Modus sicher stale gemacht.

Teure Destruktion geschieht ausserhalb des Mutex. Erst nachdem kein
Previewkandidat, erfasster Commit oder vorhandenes Retirement eine dritte
Vollmodellgeneration erzeugen kann, darf die Factorygeneration vorbereitet
werden. Ein bis acht Leases der aktuell aktiven Generation duerfen bestehen;
sie werden nach Publish zu gebundenen alten Leserbesitzen und blockieren jede
weitere Modellgeneration, bis der letzte Besitz real zerstoert ist. Existiert
bereits eine andere retired Generation oder ist die zweite Modellposition
sonst belegt, endet die Resetvorbereitung vor jedem Write typisiert busy.

Scheitert die Vorbereitung vor bestaetigtem `Resetting`, werden alle
#57-Reservierungen besitzsicher freigegeben. Ist der alte Bootstrap-/Graphstand
vollstaendig und eindeutig unveraendert, wechselt der Dienst checked zurueck
zu `Operational`; die alte Runtime bleibt dieselbe gueltige Generation. Kann
dieser alte Stand nicht eindeutig bestaetigt werden, folgt stattdessen
`RuntimeFailure` mit dem passenden Safety-Producer.

Beim Ausgang `ResetEligibleNoRuntime` gibt es keine alte Runtime
wiederherzustellen: ein Fehler vor `Resetting` kehrt checked zum gleichen
gebundenen No-Runtime-Befund beziehungsweise dessen stabiler
`RuntimeFailure`-Diagnose zurueck. Er loescht weder den vorhandenen Producer
noch erlaubt er normale Preview, Mutation oder Runtimeakquisition.

### Verhalten nach der persistenten Epochengrenze

Ein `CommitOutcomeUnknown` des `Resetting`-Writes wird noch in
`ResetPreparing` durch exakten Bootstrap-Slotreadback aufgeloest:

- exakt alte Bytes beziehungsweise nachweislich nicht wirksam: alte Epoche
  bleibt kanonisch; bei Runtime-Ausgang wird `Operational`, beim
  No-Runtime-Ausgang derselbe gebundene No-Runtime-/Safetybefund checked
  wiederhergestellt;
- exakt neue `Resetting`-Bytes: checked Wechsel zu `EpochResetting`, niemals
  alte Runtimefreigabe;
- fremde Bytes, Read-/Capacityfehler oder unaufloesbares Ergebnis:
  `RuntimeFailure` und `ConfigurationUnavailable`.

Nach bestaetigtem `Resetting` bleibt jede normale Runtime-, Preview- und
Mutationsfreigabe gesperrt. Nach neuem Root wird der bereits vollstaendig
vorbereitete Snapshot intern genau einmal publiziert und der Dienst wechselt
checked zu `BootstrapFinalizationPending`. Schlaegt `Initialized` fehl oder
bleibt sein Write unbestimmt, bleiben neuer Publisherbesitz,
Recoveryidentitaet, Zustandsrevision und Modellreservierungen an diesen
exakten Zielgraphen gebunden; es gibt keine normale Runtimeakquisition und
keinen Rueckfall zur alten Epoche.

Im selben Prozess darf nur der identitaetsgebundene Recoverybesitz den
fehlenden Abschluss erneut lesen beziehungsweise exakt fortsetzen. Nach
Neustart existiert kein fluechtiger Handoff: Bootstrap- und Graphscan muessen
den persistenten Zustand neu eindeutig bestimmen, Snapshot und Plattformwerte
vollstaendig neu vorbereiten und einen neuen internen Einmal-Handoff binden.
Kein lokaler Recoveryerfolg loescht eine bereits von #24 gefuehrte
Verriegelung.

Alte und vorbereitete Modellbesitzer werden ausserhalb kurzer kritischer
Abschnitte zerstoert. Ihre Modellreservierung bleibt jedoch bis zum
tatsaechlichen Besitzende belegt; `Operational` und freies Modellbudget werden
nicht vorzeitig gekoppelt. Dadurch bleiben Zwei-Modell-Grenze, Readerlimit und
Retirementvertrag aus #56 unveraendert.

## Initialisierung von StorageEpoch 1

### Voraussetzungen vor dem ersten Write

Unter einer gemeinsamen Mutationslease werden vor dem ersten Write:

1. der vollstaendige Factory-Neuheitsscan abgeschlossen;
2. Factory-UserConfiguration erzeugt und fachlich validiert;
3. die leere Factory-ServiceConfiguration validiert;
4. vier Factory-Arbeitskopien erzeugt und der Katalog vollvalidiert;
5. Zielslots, Identitaets-/Laengen-/CRC-/Referenzdeskriptoren und die
   kanonische Schema-1-Kodierreihenfolge vorgeplant; vollstaendige
   Recordbytes werden dabei nur nacheinander im einen Arbeitsbereich erzeugt;
6. Zeitzone und Runtime-Ressourcen soweit ohne persistente Referenzen moeglich
   vorbereitet;
7. BootstrapSequence 1, `StorageEpoch 1` und alle Zielidentitaeten checked
   gebunden;
8. Modell-, Record- und Zustandsrevisionsbudgets reserviert.

Erst danach darf `Initializing` geschrieben werden.
Bis zu dessen exakter Bestaetigung bleibt der gesamte Besitz nicht
ausfuehrbar; insbesondere existiert keine Graphwrite-Capability.

### Persistenter Ablauf

1. `ConfigurationBootstrapRecord(Sequence 1, Epoch 1, Initializing)` schreiben
   und exakt ruecklesen;
2. aus exakt diesem Bootstrapreadback die einmalige
   `ConfigurationEpochGraphWriteCapability` erzeugen;
3. UserConfiguration Revision 1 mit `de`, `Europe/Zurich` und
   `Fermentationsschrank` schreiben;
4. ServiceConfiguration Revision 1 mit exakt leerer Payload schreiben;
5. ProgramCatalog Revision 1 mit vier Factory-Arbeitskopien schreiben;
6. Manifest Generation 1 mit `InternalSystem` / `FactoryInitialization`
   schreiben und vollstaendig pruefen;
7. Zielgraph und nicht publish-faehige Runtimeaktivierung vollstaendig
   vorbereiten;
8. Root Sequence 1 mit Active Generation 1 und ohne Fallback schreiben und
   den gesamten Graphen validieren;
9. erst nach eindeutig neu committedem Root und vollstaendiger Graphpruefung
   die `CommittedRecoveryActivation` erzeugen und den Snapshot intern nicht
   fehlschlagend publizieren, normale
   Akquisition aber noch sperren;
10. BootstrapSequence 2 als `Initialized` unter Epoche 1 schreiben und exakt
   bestaetigen;
11. normale Runtimefreigabe ohne weitere fallible Arbeit linearisiert
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

1. gemeinsame Mutationslease erwerben und beide Bootstrap-Slots vollstaendig
   scannen;
2. fehlt ein Bootstrap, diese zwei gebundenen Beobachtungen in dasselbe
   Factory-Neuheitsorakel uebernehmen und genau die verbleibenden 17 Keys
   lesen; insgesamt bleiben es exakt 19 eindeutige Reads;
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
  - ConfigurationRecordOutcomeIndeterminate
  - ConfigurationCommitIndeterminate
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
- naechste `StorageEpoch = E + 1`, `Resetting`-Sequence
  `2 * (E + 1) - 1 = currentSequence + 1` und abschliessende
  `Initialized`-Sequence `2 * (E + 1)` checked berechnet;
- Factorymodelle, Zielslots, Zielidentitaeten, Recordgroessen,
  Runtimevorbereitung und Modellbudget vollstaendig vorgeplant;
- alle normalen neuen Preview- und Runtimeakquisitionen fuer den Uebergang
  kontrolliert gesperrt.

Der autorisierte Reset darf aus zwei exakt klassifizierten Ausgangslagen
beginnen:

1. `Operational` mit eindeutig gebundener alter Runtime; oder
2. `ResetEligibleNoRuntime`, wenn ein unterstuetzter kanonischer
   `Initialized`-Bootstrap, die aktuelle Epoche, beide Bootstrap-Slotbefunde
   und der vollstaendige BootstrapSequence-High-Water eindeutig sind, aber der
   alte Active-/Fallback-Graph fehlt oder integritaetsfehlerhaft ist.

Der zweite Pfad verlangt denselben vollstaendigen Bootstrapscan, dieselbe
globale Mutationslease und dieselbe Vorplanung wie der erste, setzt jedoch
keinen nutzbaren alten Graphen voraus. Er ist verboten bei fehlendem oder
nicht kanonischem Bootstrap trotz vorhandener Daten, Bootstrapkorruption oder
-kollision, unbekanntem neuerem Bootstrap-/Speicherformat, Read-/Capacity-
Unklarheit, `Initializing`, bereits laufendem `Resetting` oder weiterhin
unbestimmtem Bootstrapwrite. `Initializing` und `Resetting` werden nur als
derselbe gebundene Ablauf fortgesetzt; sie starten keinen zweiten Reset.

Ohne ausdruecklichen autorisierten Auftrag startet weder Graphkorruption noch
ein anderer Fehler automatisch einen Reset. Fehler vor dem neuen
`Resetting`-Linearisierungspunkt lassen beim No-Runtime-Pfad den bisherigen
No-Runtime- und Safetybefund bestehen. Ein erfolgreicher Reset meldet nur die
neue lokale Konfiguration; er loescht weder einen bereits erzeugten #57-
Producer noch eine spaetere #24-Verriegelung.

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

Fuer Zielslots gilt danach: exakte Zielrecords der neuen Epoche werden
idempotent wiederverwendet; `NotFound` und vollstaendig gelesene Records einer
anderen Epoche duerfen als Zielslot dienen. Korrupte, unlesbare,
kollidierende oder unbekannte Zielepochenkandidaten werden weder
wiederverwendet noch als frei behauptet. Reicht die sichere Slotmenge nicht,
bleibt der Reset fail closed. Es wird kein Loesch-, Reparatur- oder
Ueberschreibvertrag fuer unbestimmbare Bytes erfunden.

### Persistenter Resetablauf

1. `Resetting` unter der checked naechsten Epoche schreiben und bestaetigen;
2. aus exakt diesem Bootstrapreadback die einmalige
   `ConfigurationEpochGraphWriteCapability` erzeugen;
3. UserConfiguration Revision 1 mit Factorywerten schreiben;
4. ServiceConfiguration Revision 1 schreiben;
5. ProgramCatalog Revision 1 mit vier neuen Factory-Arbeitskopien schreiben;
6. Manifest Generation 1 mit `InternalSystem` / `FactoryReset` schreiben;
7. gesamten neuen Zielgraphen und die nicht publish-faehige
   Runtimeaktivierung binden;
8. Root Sequence 1 ohne Fallback schreiben und vollstaendig validieren;
9. erst danach die `CommittedRecoveryActivation` erzeugen und den Snapshot
   intern nicht fehlschlagend publizieren, normale
   Akquisition weiterhin sperren;
10. naechste BootstrapSequence als `Initialized` derselben neuen Epoche
   schreiben und bestaetigen; sie ist exakt die `Resetting`-Sequence plus 1
   und erfuellt `Sequence = 2 * StorageEpoch`;
11. normale Runtimefreigabe ohne weitere fallible Arbeit erteilen.

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
- unbestimmter Bootstrap-, Vor-Root-Record- oder Rootwrite mit getrennten
  Ursachenvertraegen;
- BootstrapSequence-, StorageEpoch-, Revision-, Generation- oder
  RootSequence-Ueberlauf;
- Runtimevorbereitungs- oder interner Zustandsfehler.

Nur `NotFound` ist leer. Weder andere Epoche, unbekanntes Schema noch
Korruption oder Storefehler werden zu Factory-Neuheit normalisiert.

### Vollständige Fehler- und Safety-Producer-Zuordnung

`ConfigurationUnavailable` und `ConfigurationIntegrityFailure` sind die
einzigen Ausgaben von #57 an das spaetere
`CONFIGURATION_SAFETY_INTEGRATION_GATE`. Der konkrete oeffentliche Status
bleibt fuer Diagnose und aufrufende Anwendung genauer; Store-, Codec-, Key-
und Bibliothekstypen werden dabei in stabile Projektkategorien uebersetzt.

| Phase | Ursache | Persistenter Stand | Runtimewirkung | Öffentlicher Status | Safety-Producer |
|---|---|---|---|---|---|
| Resetvorbereitung mit eindeutig altem `Operational` | Mutationslease busy | unveraendert alt, kein Write | alte Runtime bleibt normal gueltig | `ConfigurationMutationBusy` | keiner |
| Boot/Initialisierung ohne Runtime | Mutationslease busy | unveraendert, kein Write | weiterhin keine Runtime | `ConfigurationMutationBusy` | `ConfigurationUnavailable` |
| Resetvorbereitung mit eindeutig altem `Operational` | Modellbudget, Previewbesitz oder Retirement busy | unveraendert alt, kein Write | alte Runtime wird checked wieder freigegeben | `ConfigurationModelBudgetBusy` | keiner |
| Boot/Initialisierung ohne Runtime | Modellbudget busy | unveraendert, kein Write | keine Runtime | `ConfigurationModelBudgetBusy` | `ConfigurationUnavailable` |
| Resetvorbereitung vor `Resetting` | Bootstrap-/Epochen-/Record-Counteroverflow | unveraendert alt, kein Write | alte Runtime bleibt gueltig | `CounterOverflow` | keiner |
| Initialisierung/Boot ohne Runtime | Counteroverflow | unveraendert, kein Write | keine Runtime | `CounterOverflow` | `ConfigurationUnavailable` |
| beliebiger Dienstmodus | Zustandsrevisionsoverflow oder interne Counterinvariante | nicht als sicher fortsetzbar behauptet | fail closed | `RuntimePreparationFailure` mit stabiler Invariantenursache | `ConfigurationIntegrityFailure` |
| Resetvorbereitung, `Resetting` garantiert nicht geschrieben | `WriteError` | eindeutig alter Bootstrap | alte Runtime checked wieder `Operational` | `PersistenceWriteFailure` | keiner |
| Resetvorbereitung, `Resetting` garantiert nicht geschrieben | Write-`CapacityError` | eindeutig alter Bootstrap | alte Runtime checked wieder `Operational` | `PersistenceCapacityFailure` | keiner |
| Initialisierung oder nach bestaetigtem `Resetting` | `WriteError` | Zielzustand unvollstaendig, kein behaupteter Fortschritt | keine normale Runtime | `PersistenceWriteFailure` | `ConfigurationUnavailable` |
| Initialisierung oder nach bestaetigtem `Resetting` | Write-`CapacityError` | Zielzustand unvollstaendig | keine normale Runtime | `PersistenceCapacityFailure` | `ConfigurationUnavailable` |
| Resetvorpruefung vor jedem Write | `ReadError` oder Read-`CapacityError` eines fuer den kanonischen Nachweis erforderlichen Slots | alter persistenter Stand nicht mehr vollstaendig bestaetigbar | fail closed statt nur auf die geladene alte Runtime zu vertrauen | `PersistenceReadFailure` oder `PersistenceCapacityFailure` | `ConfigurationUnavailable` |
| Boot, Initialisierung, `EpochResetting` oder Finalisierung | `ReadError` oder Read-`CapacityError` | kanonischer Zielstand nicht bestimmbar | keine normale Runtime | `PersistenceReadFailure` oder `PersistenceCapacityFailure` | `ConfigurationUnavailable` |
| `Resetting`-Write unbestimmt | exakter Readback alt/nicht wirksam | eindeutig alte Epoche | vorhandene alte Runtime checked wieder `Operational`; ohne Runtime bleibt derselbe No-Runtime-Befund | Resetoperation abgelehnt mit `PersistenceWriteFailure` | bei Runtime keiner; vorhandener No-Runtime-Producer bleibt |
| `Resetting`-Write unbestimmt | exakter Readback neu | neue Epoche `Resetting` kanonisch | alte Runtime bleibt gesperrt; gebundene Fortsetzung | interner Fortschritt, danach finaler Aufrufstatus | keiner, solange die Fortsetzung erfolgreich endet |
| Bootstrapwrite (`Initializing`, `Resetting`, `Initialized`) unbestimmt | exakter Readback alt oder exakt neu | Bootstrapgrenze eindeutig nicht wirksam oder eindeutig fortgeschritten | ursachengemaess alte Freigabe oder gebundene Fortsetzung; nie Rootstatus | interner Fortschritt beziehungsweise konkrete Ablehnung | phasenabhaengig; bei vollstaendig erfolgreicher Fortsetzung keiner |
| Bootstrapwrite unbestimmt | fremde Bytes, `ReadError`, Read-`CapacityError` oder unaufloesbar | Bootstrapzustand nicht bestimmbar | fail closed | `BootstrapCommitIndeterminate` | `ConfigurationUnavailable`, bei nachgewiesener Bootstrapintegritaet `ConfigurationIntegrityFailure` |
| Dokument-/Manifestwrite vor Root unbestimmt | exakter Readback alt oder exakt neu | Root noch nicht committed; Record fehlt oder ist exakt erwartet vorhanden | keine Runtime; gebundene Fortsetzung darf nur denselben Plan fortsetzen | interner Fortschritt beziehungsweise konkrete Persistence-Ablehnung | bei nicht abgeschlossenem No-Runtime-Pfad `ConfigurationUnavailable` |
| Dokument-/Manifestwrite vor Root unbestimmt | fremde Bytes, `ReadError`, Read-`CapacityError` oder unaufloesbar | kein Rootcommit behauptet; Recordausgang unbestimmt | fail closed, keine Runtime und kein weiterer Write | `ConfigurationRecordOutcomeIndeterminate` | `ConfigurationUnavailable` beziehungsweise bei nachgewiesener Integritaetsverletzung `ConfigurationIntegrityFailure` |
| Rootwrite nach `Initializing`/`Resetting` unbestimmt | Zielslot nachweislich alt/nicht wirksam | neuer Epochenroot noch nicht committed | keine Runtime; gebundene Wiederaufnahme darf fehlenden Root spaeter schreiben | bestehender #56-`ConfigurationCommitIndeterminate` wird als alter Ausgang aufgeloest | `ConfigurationUnavailable`, falls der Aufruf ohne vollstaendigen Abschluss endet |
| Rootwrite nach `Initializing`/`Resetting` unbestimmt | exakt neuer Root und Zielgraph vollstaendig gueltig | neuer Root kanonisch committed | `CommittedRecoveryActivation`, Einmal-Publish und Bootstrapfinalisierung; keine Teilruntime | bestehender #56-Rootstatus wird `ResolutionRecoveredNew` | keiner bei vollstaendig erfolgreichem Abschluss |
| Rootwrite nach `Initializing`/`Resetting` unbestimmt | Zielslot fremd oder Root-/Graphscan unaufloesbar | weder alter noch neuer Root sicher kanonisch | fail closed | bestehender #56-`ConfigurationCommitIndeterminate` bleibt unaufgeloest | `ConfigurationUnavailable` beziehungsweise bei nachgewiesener Integritaetsverletzung `ConfigurationIntegrityFailure` |
| Boot/Recovery/Targetscan | CRC-, Envelope- oder Recordtypfehler | vorhandene Bytes technisch korrupt | keine Runtime beziehungsweise alte Runtime nur vor unberuehrter Resetgrenze | `ConfigurationIntegrityFailure` | `ConfigurationIntegrityFailure` |
| Graph-/Zielpruefung | Referenz-, Epochen- oder fachlicher Semantikfehler | Graph nicht kanonisch gueltig | keine Runtime | `ConfigurationIntegrityFailure` | `ConfigurationIntegrityFailure` |
| Bootstrap-, Dokument-, Manifest- oder Rootscan | gleiche Identitaet mit unterschiedlichen kanonischen Bytes | persistente Identitaetskollision | keine Runtime und keine Slotwiederverwendung | `ConfigurationIntegrityFailure` mit Kollisionsursache | `ConfigurationIntegrityFailure` |
| jeder Lesepfad | technisch gueltiges unbekanntes neueres Schema oder Speicherformat | nicht mit Schema 1 interpretierbar, unveraendert | keine kompatible Runtimefreigabe | `UnsupportedNewerConfigurationSchema` | `ConfigurationUnavailable` |
| normaler Boot | weder Active noch vollstaendig gueltiger Fallback | Bootstrap vorhanden, kein nutzbarer Graph | keine Runtime | `ConfigurationUnavailable` | `ConfigurationUnavailable` |
| autorisierter Reset aus `ResetEligibleNoRuntime` vor jedem Write | Bootstrap und High-Water eindeutig, Graph unavailable/integritaetsfehlerhaft | alte Epoche eindeutig, Graph nicht nutzbar | keine Runtime; Resetvorbereitung zulaessig | Resetauftrag angenommen oder konkrete Vorbereitungsablehnung | vorhandener #57-Producer bleibt bestehen |
| versuchter Reset ohne Runtime | Bootstrap korrupt, unbekannt, unlesbar, `Initializing`, `Resetting` oder Bootstrapwrite unbestimmt | Epochengrenze nicht fuer neuen Reset beweisbar | keine Runtime, kein Write | ursachentreue Integrity-/Unsupported-/Persistence-Ablehnung | `ConfigurationUnavailable` oder `ConfigurationIntegrityFailure` |
| Boot/Initialisierung vor Runtimepublish | Runtime-/Zeitzonenvorbereitung scheitert | Root je nach Phase noch nicht geschrieben oder bereits Zielstand | ohne alte gebundene Runtime keine Freigabe | `RuntimePreparationFailure` | `ConfigurationUnavailable` |
| Resetvorbereitung vor `Resetting` | Runtimevorbereitung scheitert, alter Stand eindeutig | unveraendert alt | alte Runtime checked wieder `Operational` | `RuntimePreparationFailure` | keiner |
| nach bestaetigtem `Resetting` | Runtimevorbereitung scheitert | neue Epoche unvollstaendig | alte Epoche bleibt gesperrt, keine neue Runtime | `RuntimePreparationFailure` | `ConfigurationUnavailable` |
| nach neuem Root, vor `Initialized` | `Initialized`-WriteError, Capacity oder Bootstrap-Unknown | neuer Root bleibt kanonisch, Bootstrapabschluss fehlt | interner neuer Publisher bleibt gebunden; normale Akquisition gesperrt | jeweiliger Persistence-Status oder `BootstrapCommitIndeterminate` | `ConfigurationUnavailable` |
| Publish/Zustandsmaschine | stale/fremder Handoff vor Publish | kein Publish und kein persistenter Fortschritt durch den Handoff | bisheriger eindeutiger Zustand bleibt | typisierte `StateChanged`-/Invariantenablehnung | keiner, sofern alter `Operational`-Stand eindeutig bleibt |
| Publish/Zustandsmaschine | doppelter Publish, Bindungsbruch oder interne Zustandsinvariante | persistent moeglicherweise bereits neue Grenze | fail closed, niemals normaler Erfolg | `RuntimePreparationFailure` mit `ServiceStateInvariantViolation` | `ConfigurationIntegrityFailure` |
| allgemeiner Fehler vor bestaetigtem `Resetting` | alter Bootstrap und Graph exakt bestaetigt | eindeutig alt | alte Runtime checked wieder `Operational` | konkrete Ablehnung aus obigen Zeilen | keiner |
| allgemeiner Fehler vor bestaetigtem `Resetting` | alter Stand nicht mehr eindeutig bestaetigbar | unklar | fail closed | konkrete Unavailable-/Integrity-Kategorie | gemaess Ursache `ConfigurationUnavailable` oder `ConfigurationIntegrityFailure` |
| allgemeiner Fehler nach bestaetigtem `Resetting` | Verfuegbarkeits-/Storeursache | neue Epoche kanonisch, Ziel unvollstaendig | keine normale Runtime | konkrete Unavailable-/Persistence-Kategorie | `ConfigurationUnavailable` |
| allgemeiner Fehler nach bestaetigtem `Resetting` | Korruption, Kollision oder Semantik | neue Epoche widerspruechlich | keine Runtime | `ConfigurationIntegrityFailure` | `ConfigurationIntegrityFailure` |

Ein erfolgreicher lokaler Retry oder Boot-Recovery aktualisiert nur den lokalen
Konfigurationsstatus. Er sendet niemals einen impliziten Fehlerreset und
loescht keine spaetere #24-Verriegelung.

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
| Historienrelation | alle vier erlaubten Start-/Fortschreibungsrelationen; Einzelrecordformel fuer jeden Zustand |
| Regressionen | hoehere Sequence mit kleinerer Epoche; kleinere Sequence mit neuerer Epoche; alte Epoche nie reaktivieren |
| Unmoegliche Zustaende | `Initializing` ausser Epoche 1/Sequence 1; `Resetting` ohne checked Epochenwechsel; Zustands-/Sequenzsprung und Luecke |
| High-Water | direkt erlaubter Vorgaenger/Nachfolger, byteidentisches Duplikat, unmoeglicher hoeherer Record, Maximum und checked Overflow |
| Write | Success, WriteError, CapacityError, `CommitOutcomeUnknown` alt/neu/unaufloesbar |
| Wiederaufnahme | exakter Record wird unter gleicher Identitaet wiederverwendet; anderer Inhalt nie |

### Factory-Neuheitsorakel

Tabellengetrieben fuer jeden der 19 Slots:

- ein Zugriffsjournal beweist exakt einen Read je bekanntem Key, insgesamt 19;
- die zwei Bootstrapbeobachtungen werden nicht doppelt gelesen, sondern unter
  derselben Lease, Dienstrevision und Orakelidentitaet weiterverwendet;
- genau `NotFound` fuer alle Slots erlaubt Factory-Neuheit;
- genau ein vorhandener beliebiger Bytewert blockiert;
- genau ein gueltiger Record anderer Epoche blockiert;
- genau ein unbekanntes Schema blockiert;
- genau ein CRC-/Envelopefehler blockiert;
- genau ein `ReadError` blockiert;
- genau ein `CapacityError` blockiert;
- kein Write findet vor vollstaendig positivem Orakel statt;
- ein paralleler Neuheits-/Mutationsversuch erhaelt
  `ConfigurationMutationBusy` ohne Storezugriff;
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

- Factory-Neuheit allein erzeugt keine ausfuehrbare Graphwrite-Capability;
- `Initializing`-WriteError, sicher nicht wirksames Unknown und unaufloesbares
  Unknown lassen keinen Dokument-, Manifest- oder Rootwrite zu;
- die vorbereitete Runtimeaktivierung ist vor Rootcommit nicht
  publish-faehig; erst exakt neuer Root plus vollstaendig verifizierter Graph
  erzeugt die `CommittedRecoveryActivation`;
- fremde, stale, doppelte oder an andere Bootstrap-/Rootidentitaet gebundene
  Besitze bleiben wirkungslos; Produktionskonstruktion ist nicht moeglich;
- Bootstrap-, Dokument-/Manifest- und Root-Unknown werden jeweils fuer alt,
  neu und unaufloesbar getrennt getestet; nur Root-Unknown verwendet #56-
  `ConfigurationCommitIndeterminate`/`resolveCommitDetailed()`;
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
- ein byteidentisches Bootstrapduplikat ist diagnostizierbar und
  deterministisch; gleiche Identitaet mit abweichenden Bytes blockiert;
- keine Schema-1-Historie kann eine bereits verlassene Epoche reaktivieren.

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
- `Initialized` plus fehlender Graph, kein nutzbarer Active/Fallback oder
  CRC-/Semantikfehler im alten Graph erlaubt einen ausdruecklich
  autorisierten Reset, sofern Bootstrap und sichere Zielslotmenge eindeutig
  sind;
- derselbe Stand startet ohne Resetauftrag nie automatisch;
- Bootstrapkorruption, unbekanntes Bootstrapformat und unlesbare
  Bootstrapslots lehnen den Reset vor jedem Write ab;
- `Initializing` und `Resetting` starten keinen zweiten Reset;
- `Resetting`-WriteError sowie nicht wirksames oder unaufloesbares Unknown
  erzeugen keine Zielepochen-Graphwrite-Capability;
- sichere Zielslots unterscheiden exakten Zielepochenrecord,
  andere Epoche/`NotFound` sowie korrupte, unlesbare, kollidierende und
  unbekannte Kandidaten; unzureichende Slotmenge bleibt fail closed;
- erfolgreicher Reset setzt einen simulierten #24-Latch nicht zurueck.

### Touchkalibrierung

- Sentinel vor Initialisierung, vor Reset und nach jedem Cut byteidentisch;
- Zugriffsjournal beweist null Reads und null Writes auf den Sentinelkey;
- Storefehler anderer Keys aendern den Sentinel nicht;
- erfolgreicher und fehlgeschlagener Reset erhalten ihn;
- kein Produktionskey oder Touchmodell wird aus dem Test abgeleitet.

### Konkurrenz, Runtime und Modellbudget

- Konstruktion mit identischem Store, Graphstore, Service, Koordinator und
  Zeitzonenresolver gelingt; Split-Store und Split-Coordinator werden vor
  jeder Operation abgelehnt;
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

### ConfigurationService-/Recovery-Zustandsmaschine

Tabellengetrieben werden jeder in der Zustandsmaschine erlaubte Uebergang und
jeder andere direkte Uebergang als verboten geprueft. Zusaetzlich:

- ein direkter Aufruf des bisherigen `initialize(...)`-/Publish-Bypasses ist
  in Produktionscode nicht mehr kompilierbar beziehungsweise ohne interne
  Capability nicht aufrufbar;
- der epocheninitiale Graphwrite ist ohne gueltige
  `ConfigurationEpochGraphWriteCapability` nicht ausfuehrbar;
- Fehler vor `Resetting` mit exakt altem Readback stellen dieselbe alte
  `Operational`-Runtime checked wieder her;
- `CommitOutcomeUnknown` beim `Resetting`-Write wird fuer alt, neu und
  unaufloesbar geprueft;
- `CommitOutcomeUnknown` beim neuen Root wird fuer alt, neu mit vollstaendig
  gueltigem Graph und unaufloesbaren/fremden Root geprueft;
- jeder Fehler nach bestaetigtem `Resetting` sperrt die alte Epoche;
- neuer Root plus fehlgeschlagenes oder unbestimmtes `Initialized` bleibt in
  `BootstrapFinalizationPending` beziehungsweise stabil fail closed und gibt
  keine normale Runtime aus;
- Wiederaufnahme im selben Prozess verwendet nur den exakt gebundenen Besitz;
  vollstaendiger Neustart rekonstruiert ohne fluechtige Capability;
- sichtbare `NoChange`- und Changed-Preview sowie laufende Previewerstellung
  werden beim Resetstart identitaets- und reservierungsrichtig behandelt;
- ein bis acht alte Read-Leases, bestehendes Retirement und verzoegerte
  Besitzerzerstoerung halten die Zwei-Modell-Grenze ein;
- Zustandsrevisionsheadroom nahe `UINT64_MAX` blockiert vor dem ersten Write;
- stale, doppelte und fremde Recovery-Handoffs publizieren nie;
- zu keinem kontrollierten Interleaving existieren mehr als zwei
  Vollmodellgenerationen oder eine normal sichtbare Teilruntime.

### Fehler- und Safety-Zuordnung

Jede Zeile der Fehler-/Safety-Tabelle wird mindestens einmal als
tabellengetriebener Testfall abgebildet. Besonders werden gleiche Ursachen vor
und nach der bestaetigten Epochengrenze paarweise geprueft:

- Busy, Modellbudget und Vorbedingungsablehnung vor jedem Write lassen eine
  eindeutig alte Runtime ohne Safety-Producer bestehen;
- Read-, Capacity-, Write- und Unknown-Befunde ohne vorhandene Runtime oder
  nach `Resetting` produzieren `ConfigurationUnavailable`;
- CRC-, Envelope-, Referenz-, Semantik- und Identitaetskollision produzieren
  `ConfigurationIntegrityFailure`;
- unbekanntes neueres Schema liefert oeffentlich
  `UnsupportedNewerConfigurationSchema` und als Safety-Producer
  `ConfigurationUnavailable`;
- interner Zustands-/Publishbruch liefert niemals Erfolg und produziert
  `ConfigurationIntegrityFailure`;
- ein #24-Testdouble erhaelt bei Bootstrap-Unknown, Vor-Root-Record-Unknown
  und #56-Root-Unknown jeweils die reale getrennte redigierte Ursache; kein
  Status wird als anderer Committyp umbenannt;
- erfolgreicher lokaler Retry aendert keinen simulierten #24-Latchzustand.

### Recordpuffer, Duplikate und idempotente Wiederaufnahme

- maximaler ProgramCatalog bei Initialisierung und Reset;
- Instrumentierung bilanziert gleichzeitig lebende Bytes und Kapazitaeten fuer
  typisierte Vollmodelle, ProgramCatalog-Payload, maximalen Dokument-Envelope,
  Store-Readback, kleine Root-/Manifest-/Bootstrapbytes und Runtime-Handoff;
- vor jedem `encodeEnvelope()` ist der Outputhalter einschliesslich Kapazitaet
  per Swap freigegeben; `clear()` allein besteht den Nachweis nicht;
- nie leben zwei application-held vollstaendige ProgramCatalog-Envelopes oder
  zwei ProgramCatalog-Vollpayloadkopien gleichzeitig; Readback beginnt erst
  nach Freigabe des erwarteten Vollrecordworkspace;
- ProgramCatalog-Readback wird mit einem Record nach dem anderen gegen das
  geteilte Factorymodell validiert, ohne zweites Vollmodell;
- Cuts vor/nach Payloadkodierung, Envelopekodierung, Write, Workspacefreigabe
  und Readback halten dieselbe Obergrenze;
- byteidentische Zielrecords derselben Identitaet in mehreren Slots waehlen
  den kleinsten Slot und erzeugen keinen Rewrite;
- gleiche Identitaet mit unterschiedlichen Bytes blockiert vor jedem Write;
- fehlende Records werden ausschliesslich in der festen Reihenfolge ergaenzt;
- wiederholte Wiederaufnahme schreibt exakt vorhandene Records nicht neu,
  vergibt keine neue Identitaet und zeigt keinen wachsenden Live-Heap;
- die vollstaendige bestehende #56-Normalmutationsmatrix bleibt unveraendert
  gruen und beweist, dass kein zweiter allgemeiner Commitworkflow entstanden
  ist.

### Additiver Ausbauvertrag

Diese eigene Testgruppe korreliert die Erweiterbarkeit aus Live-Issue #57,
ohne produktive Zukunftsstrukturen anzulegen:

- Ein unbekannter spaeterer Recordtyp in einem geprueften bekannten Kontext
  fuehrt zu keinem Write, Publish, keiner Runtimefreigabe und keiner sonstigen
  Teilwirkung.
- Ein unbekanntes neueres fachliches Schema bleibt fail closed; eine
  unbekannte generische Envelope-Version liefert einen stabilen
  Unsupported-/Unavailable-Befund und wird weder als CRC-Fehler noch als
  `NotFound` umgedeutet.
- Golden-Vektoren beweisen die bytegenaue Lesbarkeit bestehender Schema-1-
  Dokument-, Manifest- und Rootrecords sowie der neuen Bootstrapbytes. Die
  bestehenden #54/#55/#56-Golden- und Regressionstests bleiben gruen.
- In `test/test_configuration_codecs/test_configuration_codecs.cpp` wird die
  Copy-Migrationsquelle vor dem Aufruf vollstaendig byte- und wertbezogen
  erfasst und danach unveraendert nachgewiesen; nur der neue Kandidat besitzt
  das Zielschema.
- `test/test_configuration_bootstrap_store/test_configuration_bootstrap_store.cpp`
  enthaelt den rein testlokalen zusaetzlichen epochengebundenen Recordtyp sowie
  Unknown-Type- und Unknown-Envelope-Version-Faelle. Diese Datei ist passend,
  weil sie Envelope-, Epochen- und Slotklassifikation zusammen prueft. Der
  Test reserviert weder produktive Record-Type-ID noch produktiven Key und
  erfordert keine Aenderung bestehender Schema-1-Decoder oder -Bytes.
- Der Testtyp erzeugt keine Connectivity-, Authentication-, Credential- oder
  Secretstruktur; fuer den Nachweis entsteht keine Produktionsdatei.

Der Copy-Migrationsnachweis veraendert keine Migrationssemantik. Sollte er
eine Produktionsaenderung statt nur den fehlenden Vorher-/Nachher-Test
erfordern, waere dies ausserhalb des geplanten Dateischnitts und vorab erneut
ownerfreizugeben.

### Ressourcen und Builds nach Planfreigabe

- maximal ein application-held vollstaendiger maximaler Dokument-Envelope-
  workspace; Payload, Envelope, Store-Readback und kleine Graphrecords werden
  als getrennte Kategorien mit ihrer maximal erlaubten Gleichzeitigkeit
  ausgewiesen;
- Bootstrapcodec exakt 5 Payload- beziehungsweise 42 Recordbytes;
- Factory-Neuheit exakt 19 begrenzte Reads und keine unbeschraenkte Sammlung;
- maximal zwei Vollmodellgenerationen und acht Runtime-Read-Leases wie #56;
- wiederholte Boot-/Initialisierungs-/Resetzyklen ohne wachsenden Live-Heap;
- Peak-Allokationsbericht fuer typisierte Vollmodelle, ProgramCatalog-Payload,
  maximalen Dokument-Envelopeworkspace, Store-Readback, kleine kanonische
  Root-/Manifest-/Bootstrapbytes, Indeterminate-Kontext und Runtime-Handoff;
- vollstaendiges `pio test -e native`;
- Builds `native`, `esp32_bringup`, `esp32_release`;
- Base-/Head-Bericht fuer Hostbinaer, statisches RAM, Flash,
  `firmware.elf` und `firmware.bin` mit identischer Toolchain;
- `clang-format`, `clang-tidy`, Architektur-, PlatformIO-, Quality- und
  Secretpruefungen;
- `git diff --check`.

Reale Heap-, NVS-, Jitter-, Watchdog-, Flashatomizitaets- und
Lebensdauermessungen bleiben offen und werden nicht durch Hosttests ersetzt.

## Anforderungs-zu-Plan-Matrix

Jede verbindliche Gruppe des Live-Issues besitzt einen konkreten Plan-, Datei-
und Testbezug. Ein pauschaler Verweis auf den gesamten nativen Testlauf gilt
nicht als Nachweis.

| Issue-Gruppe | Planabschnitt | Produktion/Wiederverwendung | Testdatei/Testgruppe | Erfolgskriterium | Fehler-/Safetywirkung | Offenes Gate |
|---|---|---|---|---|---|---|
| BootstrapRecord | BootstrapRecord; kanonische Auswahl | neue Bootstraptypen/-codecs/-store auf #54-Envelope/CRC | Bootstrapcodec/-store | Schema 1, zwei Slots, Historienrelation, Unknown-Aufloesung | Bootstrapintegritaet -> `ConfigurationIntegrityFailure`; Unverfuegbarkeit -> `ConfigurationUnavailable` | reales Backend `SPIKE_REQUIRED` |
| Factory-Neuheit | Nachweislich fabrikneuer Speicher | Recoveryservice plus derselbe Store/Koordinator | Recoveryservice: 19-Key-Zugriffsjournal | genau ein Read je Key, nur 19-mal `NotFound`, kein Write vorher | jeder andere Befund fail closed | reale Storemessung `MEASUREMENT_REQUIRED` |
| Initialisierung | StorageEpoch 1; Phasenbesitz | Bootstrapstore, epocheninitialer Graph, Service-Handoff | Recoveryservice/Graphstore Cut-Points | `Initializing` vor Graphwrite, Root vor Publish, `Initialized` vor Runtime | unvollstaendig -> `ConfigurationUnavailable`; Integritaet ursachentreu | Flash-Cut-Points `SPIKE_REQUIRED` |
| Normaler Boot und Fallback | Normaler Boot | #56 kanonischer Loader, Active/Fallback, Runtimevorbereitung | Recoveryservice Bootmatrix | nur vollstaendig gueltiger kanonischer Active/Fallback wird freigegeben | unavailable/integrity getrennt | Composition Root `FINAL_SELECTION_PENDING` |
| zuvor unbestimmter Rootcommit | Normaler Boot; Unknown-Trennung | #56 `ConfigurationCommitIndeterminate` und `resolveCommitDetailed()` | Graphstore/Recoveryservice alt-neu-unaufloesbar | reale #56-Rootsemantik bleibt sichtbar | unaufloesbar -> `ConfigurationUnavailable` oder Integritaetsproducer | #24-Integration offen |
| StorageEpoch | Historienrelation; Reset | #54 starker Typ/checked Increment | Bootstrapstore und Reset-Cut-Points | monotone Epoche, alte nie reaktiviert | Regression/Kollision -> Integritaetsproducer | reales Backend `MEASUREMENT_REQUIRED` |
| Korruption und unbekannte Versionen | Fehlerklassifikation; additiver Ausbau | #54/#55/#56 Decoder und neue Bootstrapklassifikation | Codec-, Store-, Recovery-Negativmatrix | kein `NotFound`-/CRC-Umetikettieren, keine Teilwirkung | Korruption -> Integrity; Unsupported -> Unavailable | keine Auswahl offen |
| autorisierter Reset mit Runtime | Werksreset; Zustandsmaschine | vorhandene Runtime/Reader/Retirement plus Recoveryservice | Reset- und Konkurrenzmatrix | alte Runtime bis `Resetting` eindeutig, neue erst nach `Initialized` | Fehler vor Grenze ohne Producer bei sicher altem Stand; danach fail closed | #24-Resetvertrag offen |
| autorisierter Reset ohne Runtime | Werksreset; `ResetEligibleNoRuntime` | vollständiger Bootstrapscan ohne alten Graph als Voraussetzung | No-Runtime-Resetmatrix | kanonischer `Initialized`-Bootstrap erlaubt ausdruecklichen Reset | bestehender Producer/Latch bleibt bestehen | #24-Resetvertrag offen |
| Resetwiederaufnahme | Reset-Cut-Point-Orakel | idempotenter Bootstrap-/Graphpfad | alle Cuts und Neustartorakel | exakt dieselbe Zielepoche und Identitaet wird fortgesetzt | unklar -> fail closed, kein Rollback | reale Stromunterbrueche `SPIKE_REQUIRED` |
| Touchkalibrierung | Touchkalibrierung | keine Produktivdatei/kein Key | Recoveryservice Touch-Sentinel | null Reads/Writes, byteidentisch nach jedem Cut | keine lokale Safety-Neudeutung | Hardware-Recovery `TBD_HARDWARE` ausserhalb #57 |
| spaetere Secret-Domaenen | Nicht-Ziele; Security | keine Manifeste, Roots, Keys oder Dummyrecords | Keyjournal und additiver Testtyp | keinerlei produktive Secretstruktur | keine Secretwerte in Diagnose | erster realer Konsument `FINAL_SELECTION_PENDING` |
| Safety-Gate #24 | Fehler-/Safety-Zuordnung | nur stabile #57-Producer | tabellengetriebener Producer-/Latch-Testdouble | genau `ConfigurationUnavailable` oder `ConfigurationIntegrityFailure`; kein Latchreset | Integration bleibt bei #24 | `CONFIGURATION_SAFETY_INTEGRATION_GATE` |
| additiver Ausbauvertrag | eigene Testgruppe | unveraenderte generische Envelope-/Epochenbausteine | Configuration-Codecs und Bootstrapstore | Golden-Lesbarkeit, source-preserving Copy-Migration, testlokaler Zusatztyp | Unsupported ohne Teilwirkung | keine produktive ID/kein Key |
| Ressourcen und Builds | Ressourcenvertrag | #54-Encoder, #56-Modell-/Readerbudget | Allokationsinstrumentierung und Buildbericht | definierte gleichzeitige Bytes/Kapazitaeten, zwei Modelle, drei Builds | Budgetfehler vor Write, keine Teilruntime | Heap/NVS/Jitter/Watchdog `MEASUREMENT_REQUIRED` |

### Zuordnung aller Akzeptanzkriterien

| Akzeptanzkriterium aus #57 | Nachweis im Plan | Konkretes Orakel |
|---|---|---|
| fabrikneu eindeutig von Altbestand/Korruption unterscheiden | Factory-Neuheitsorakel | 19 gebundene Einzelreads; nur durchgehend `NotFound` initialisiert |
| Bootstrap und Epoche stromausfallsicher fortsetzen | Schema-1-Historie, Bootstrapstore, Cut-Points | jeder Bootstrapwrite alt/neu/unaufloesbar und Neustart |
| Initialisierung niemals teilweise aktivieren | Phasenbesitz und Initialisierungsablauf | kein Graphwrite vor `Initializing`, kein Publish vor Root, keine Runtime vor `Initialized` |
| normaler Boot nutzt kanonischen Active/Fallback | Normaler Boot | vollstaendige #56-Graphvalidierung einschliesslich Fallback |
| Root-Unknown korrekt aufloesen | dreistufiger Unknown-Vertrag | #56-`ConfigurationCommitIndeterminate` bleibt Rootvertrag |
| Werksreset vorwaertsgerichtet und wiederaufnehmbar | Resetablauf und beide Reset-Ausgangslagen | `Resetting` als irreversible Epochengrenze, alle Cuts |
| Werksreset erhaelt Touchkalibrierung | Touch-Sentinel | null Zugriffe und byteidentischer Sentinel |
| keine vorbereiteten Secret-Domaenen | Security und additiver Ausbau | Produktionskey-/Recordjournal ohne Secretstrukturen |
| stabile #57-Fehler fuer #24 | Fehler-/Safety-Tabelle | jede Ursache auf hoechstens einen der zwei Producer abgebildet |
| begrenzte Ressourcen und keine allgemeine Plattform | Ressourcen-/Adopt-or-build-Vertrag | Allokationsbericht, lokaler Testtyp, keine neue Abhaengigkeit |
| additive Erweiterbarkeit ohne Schema-1-Umdeutung | Additiver Ausbauvertrag | Goldenbytes, unveraenderte Migrationsquelle, testlokaler Epochenrecord |
| alle Profile und Regressionen | Ressourcen und Builds | native Tests und Builds `native`, `esp32_bringup`, `esp32_release` nach Freigabe |

## Geplanter kleiner Commit-Schnitt nach Planfreigabe

Alle Commits bleiben im selben Draft-PR. Jeder Commit baut und besteht die bis
dahin zutreffenden nativen Tests.

### Commit 1 – Bootstrapmodell, Wireformat und redundanter Store

- Record-Type-ID 6, Keys `cb0`/`cb1` und Limits;
- Bootstraptypen, Schema-1-Codec, formale Epochen-/Sequence-/Zustandsrelation
  und kanonische Auswahl;
- High-Water, Gleichstand, Read-/Capacity-/Schema-/Integritaetsdiagnosen;
- Bootstrapwrite und Unknown-Aufloesung;
- Golden-, Roundtrip-, Slot- und Negativtests.

### Commit 2 – Factory-Neuheitsorakel und epocheninitialer Graph

- gebundenes 19-Key-Factory-Neuheitsorakel ohne doppelten Bootstrapread;
- schmale `ConfigurationGraphStore`-Erweiterung fuer neuen Graph ohne
  Fallback;
- nicht ausfuehrbarer `PreparedInitialConfigurationGraph`, Graphwrite-
  Capability erst nach bestaetigter Bootstrapgrenze und sequenzieller,
  kapazitaetsgemessener Payload-/Envelope-/Readbackbesitz;
- idempotente Zielrecordwiederverwendung und vollstaendige Vor-Root-Pruefung;
- getrennte Dokument-/Manifest-/Root-Unknown- und Cut-Point-Tests;
- additiver Testrecord sowie Copy-Migrations-Quellnachweis in den genannten
  bestehenden beziehungsweise neuen Testdateien.

### Commit 3 – Runtime-Handoff, normaler Boot und Initialisierung

- frei aufrufbaren `initialize(...)`-Bypass schliessen und schmalen
  capability-gebundenen Recovery-Handoff im bestehenden
  `ConfigurationService`;
- vollstaendige gemeinsame Dienstzustandsmaschine mit checked
  Zustandsrevisionen;
- `CommittedRecoveryActivation` erst nach eindeutigem Rootcommit und
  vollstaendiger Zielgraphpruefung;
- `ConfigurationRecoveryService` und stabile Ergebnisursachen;
- `Initializing`-Wiederaufnahme und `Initialized`-Boot;
- Active-/Fallback-, Unknown-Reboot-, Modell- und Runtimefreigabetests.

### Commit 4 – Werksreset, Toucherhalt und Konkurrenzmatrix

- autorisierten Reset-Eingang und checked Epochenwechsel;
- resetberechtigten, persistent neu klassifizierten No-Runtime-Pfad ohne
  automatischen Reset und mit sicherer Zielslotpolitik;
- `Resetting`-Wiederaufnahme an allen Cut-Points;
- Touch-Sentinel-, alte-Epochen-, Koordinator-, Reader- und
  Modellbudgettests;
- phasenbezogene Fehler-/Safety-Producer-Matrix;
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
- die mathematische Schema-1-Historienrelation bei einem oder zwei
  Bootstraprecords Epochenregression, Luecken und unmoegliche Zustaende
  ausschliesst;
- Factory-Neuheits-, Initialisierungs-, Boot- und Resetorakel jeden
  persistenten Cut abdecken;
- gemeinsame Mutationskoordination und #56-Runtime-Handoff ohne zweiten
  Mechanismus festgelegt sind;
- Factory-Neuheit nur nicht ausfuehrbaren Vorbereitungsbesitz erzeugt und
  `Initializing` beziehungsweise `Resetting` unumgehbare persistente
  Graphwritegrenzen sind;
- Runtimepublish erst nach eindeutig committedem und vollstaendig
  verifiziertem Root und normale Runtimeakquisition erst nach
  `Initialized` moeglich sind;
- ein ausdruecklich autorisierter Reset aus dem vollstaendig klassifizierten
  `ResetEligibleNoRuntime`-Graphfehlerzustand moeglich bleibt, waehrend
  Bootstrapfehler und unbekannte Formate ihn blockieren;
- Bootstrap-, Vor-Root-Record- und Root-Unknown getrennte Status-,
  Fortsetzungs- und Safetyvertraege besitzen und der #56-
  `ConfigurationCommitIndeterminate`-Vertrag erhalten bleibt;
- das Factory-Neuheitsorakel insgesamt exakt 19 einmalige, an Lease,
  Dienstrevision und Orakel gebundene Keybeobachtungen verwendet;
- kein oeffentlicher Initialisierungs- oder epocheninitialer Graphwrite-Bypass
  die Bootstrapfreigabe umgehen kann;
- alle ConfigurationService-/Recoverymodi, Zustandsrevisionen, Preview-,
  Reader-, Retirement- und Zwei-Modell-Grenzen testbar definiert sind;
- jede Fehlerursache und Ablaufphase eindeutig auf Runtimewirkung,
  oeffentlichen Status und hoechstens einen der zwei #57-Safety-Producer
  abgebildet ist;
- `PreparedInitialConfigurationGraph` nur geteilte Modelle und begrenzte
  Deskriptoren haelt und der sequenzielle Payload-/Envelope-/Readbackvertrag
  mit dem realen #54-Encoder technisch und messbar begrenzt ist;
- Touchkalibrierung nachweislich unangetastet bleibt;
- #17 und #24 weder vorgezogen noch implementiert werden;
- keine Secret-, Pending-, Intent- oder allgemeine Persistenzinfrastruktur
  vorgesehen ist;
- Test-, Ressourcen-, Dokumentations- und offene Messgates vollstaendig sind;
- der additive Ausbauvertrag auf konkrete Codec-/Bootstrapstoretests sowie die
  unveraenderte Copy-Migrationsquelle abgebildet ist;
- die Anforderungs-zu-Plan-Matrix jede Live-Issue-Gruppe und jedes
  Akzeptanzkriterium konkret korreliert;
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
