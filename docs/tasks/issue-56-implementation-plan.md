# Implementierungsplan fuer Issue #56

## Planstatus

- Issue: #56 – `[E2.1c] Active-/Fallback-Manifeste, Vorschau und Runtimeaktivierung implementieren`
- Ausgangsbranch: `main`
- Ausgangs-Commit: `bcdd3b3fbf956ccbcff3f70b4f359e61d9529fb7`
- Planbranch: `plan/issue-56-active-fallback-runtime`
- Planstatus: `PLAN_DRAFT`
- Ueberholte Plan-Commits:
  - `48f342857a17e6ada2f0b4a7d147fd86b5489b83`
  - `e9d3fdc1b93f2feb8bf5a0c1cdaf3908d635b8b3`
  - `18bf1c0fa68c7606ba669496445b128c13458f13`
- Live-Issue-Status bei Planerstellung: `READY`
- Harte Abhaengigkeiten: #54 und #55, beide `COMPLETED`

```text
IMPLEMENTATION_BLOCKED_PENDING_PLAN_APPROVAL
```

Dieser Plan autorisiert noch keine Implementierung. Die Umsetzung darf erst
nach folgendem eindeutigen Ownerkommentar fuer genau den Commit beginnen, der
diese Planversion enthaelt:

```text
PLAN APPROVED
Approved plan commit: <commit-sha>
```

## Ziel

Issue #56 implementiert den in ADR-018 beschlossenen Variante-B-Kern der
Konfigurationspersistenz auf den bereits gemergten Grundlagen aus #54 und #55:

1. deterministische Schema-1-Codecs fuer `ConfigurationManifest` und
   `ConfigurationRootRecord`;
2. kanonische Validierung des vollstaendigen Active-/Fallback-Graphen;
3. sichere, referenzbasierte Rotation der Dokument-, Manifest- und Rootslots;
4. genau eine fluechtige, vollstaendig validierte Konfigurationsvorschau;
5. optimistischen Revisions- und Konfliktschutz fuer Display und Web;
6. einen vor dem Root-Commit vollstaendig vorbereiteten unveraenderlichen
   `RuntimeConfigurationSnapshot`;
7. den Root-Commit als einzigen persistenten Linearisierungspunkt und einen
   anschliessenden nicht allokierenden, nicht serialisierenden und
   vertraglich nicht fehlschlagenden Publish;
8. stabile typisierte Fehlerproducer fuer den spaeteren
   `CONFIGURATION_SAFETY_INTEGRATION_GATE`;
9. einen vollstaendigen nativen Cut-Point-, Korruptions-, Konflikt- und
   Ressourcen-Nachweis sowie Builds aller drei Profile.

Das Ergebnis bleibt hardwareunabhaengig und benutzt ausschliesslich den
bereits vorhandenen `IStateStore`-Port. Es bindet noch keinen NVS-Adapter in
den Composition Root ein.

## Nicht-Ziele

Issue #56 implementiert ausdruecklich nicht:

- Bootstrap, `BootstrapRecord`, `BootstrapSequence`, `StorageEpoch`-Wechsel,
  Werksreset oder Korruptions-Startupsteuerung aus #57;
- Laufpersistenz, Kontrollpunkte oder einen Integrationsport zu #17;
- Fehlerklassifizierung, persistente Verriegelung, `SAFE_BOOT`, Fehlerreset,
  Aktorfreigabe, GPIO oder andere Teile aus #24;
- persistentes Pending, einen `PendingRoot` oder einen parallelen
  voraktivierten Graphen;
- ein Aktivierungsintent oder `ConfigurationActivationRunAssessment`;
- neustartpflichtige Konfigurationsaktivierung;
- persistente Preview-Slots, Preview-Owner, Autorisierungstokens oder
  zeitbasierte Preview-Ablaufregeln;
- Connectivity-/Authentication-Manifeste, -Roots, -Slots oder Reservepayloads;
- WLAN-Secrets, Credentialdaten, `CredentialEpoch`, KDF, Sessions oder CSRF;
- kombinierte Konfigurations-/Secret-Transaktionen;
- Import-Run-Gates aus #19-D;
- einen produktiven NVS-Adapter, Partitionierung, Flashverschluesselung oder
  eine reale Flash-Atomizitaets- beziehungsweise Lebensdauergarantie;
- neue Abhaengigkeiten, Buildflags, Toolchain- oder Hardwarekonfiguration;
- eine allgemeine Persistenz-, Transaktions-, Preview-, Provider- oder
  Pluginplattform.

## Verbindliche Quellen und Entscheidungen

Die Dokumentationsprioritaet aus
[`docs/SPECIFICATION_REVIEW.md`](../SPECIFICATION_REVIEW.md) ist verbindlich.
Fuer diesen Plan wurden insbesondere vollstaendig geprueft:

- das Live-Issue #56 einschliesslich des Synchronisationskommentars nach PR
  #64;
- das Repository-`AGENTS.md` sowie die Modulregeln in
  `lib/device_platform`, `lib/device_platform_test_support` und
  `lib/fermentation_app`;
- ADR-016 in
  [`docs/ADR-016_KONFIGURATIONSSPEICHER_BACKEND.md`](../ADR-016_KONFIGURATIONSSPEICHER_BACKEND.md);
- ADR-018 im zentralen Register
  [`docs/DECISIONS.md`](../DECISIONS.md);
- [`docs/CONFIGURATION_PERSISTENCE.md`](../CONFIGURATION_PERSISTENCE.md);
- [`docs/SETTINGS_AND_STORAGE.md`](../SETTINGS_AND_STORAGE.md);
- [`docs/BACKUP_SECURITY_RETENTION.md`](../BACKUP_SECURITY_RETENTION.md);
- [`docs/PR38_REVIEW_CORRECTIONS.md`](../PR38_REVIEW_CORRECTIONS.md);
- [`docs/IMPLEMENTATION_ISSUES.md`](../IMPLEMENTATION_ISSUES.md);
- den gemergten Neuschnitt in
  [`docs/tasks/issue-16-implementation-plan.md`](issue-16-implementation-plan.md);
- die bestehenden #54-/55-Speicher-, Dokument-, Codec-, Migrations- und
  Testbausteine im Code.

Verbindlich bleiben insbesondere:

- ADR-013 fuer die Modulgrenzen;
- ADR-016 fuer `IStateStore`, NVS-kompatiblen Schluesselraum, Envelope,
  CRC-32/ISO-HDLC und atomaren Replace pro Schluessel;
- ADR-018 fuer Variante B, Root-Linearisierung, Active/Fallback und den
  fail-closed unbestimmten Commitzustand;
- der bestehende Dokumentvertrag aus #55 fuer `UserConfiguration`,
  `ServiceConfiguration` und `ProgramCatalog`;
- genau vier Slots je Dokumenttyp, drei Manifest- und zwei Rootslots;
- `StorageEpoch`, Revision, Generation und Sequenz beginnen bei 1, reservieren
  0 und laufen nie still ueber;
- reale Flash-, NVS- und Heapgarantien bleiben Messgates.

## Aktuelle Ausgangslage

### Bereits umgesetzt

`device_platform` stellt aus #54 bereit:

- `IStateStore` mit getrennten Lese- und Schreibstatus;
- `CommitOutcomeUnknown` als expliziten Schreibergebniszustand;
- gueltig-by-construction `StateStoreKey` mit maximal 15 Zeichen;
- starke technische Typen, checked increment, Big-Endian-Primitiven und CRC;
- `StorageEnvelope` mit begrenztem Schema- und Payloadvertrag;
- technische Slotkandidatensuche mit sichtbaren Lese-, Kapazitaets- und
  Integritaetsproblemen;
- gezieltes Nachladen und erneutes Validieren einer Slotpayload;
- rein technische Round-Robin-Slotwahl ohne fachliche Schutzmenge.

`device_platform_test_support` stellt bereit:

- einen binaersicheren `SimulatedPersistentStateStore`;
- Writefehler und Power-Cuts vor beziehungsweise nach dem atomaren Replace;
- `CommitOutcomeUnknown` nach dauerhaftem Commit;
- gezielte Read-, NotFound- und Korruptionsinjektion;
- Neustart mit Erhalt ausschliesslich committeter Daten.

`fermentation_app` stellt aus #55 bereit:

- die drei typisierten Dokumentmodelle und ihre starken Revisionstypen;
- vollstaendige fachliche Validierung;
- deterministische Schema-1-Codecs mit festen Payloadgrenzen;
- Copy-Migration als Grundlage;
- Record-Type-IDs und vier kurze Slotschluessel je Dokumenttyp;
- zentrale Konfigurationsgrenzen.

### Noch nicht umgesetzt

Es existieren noch keine Produktionsbausteine fuer:

- Manifest- oder Rootmodelle und deren Wireformat;
- fachliche Graphvalidierung und kanonische Rootauswahl;
- referenzbasierte Schutzmenge und Slotplanung;
- Vorschau, Konfigurationsdienst und Konfliktschutz;
- `RuntimeConfigurationSnapshot` und dessen Publish;
- `ConfigurationRuntimeFailure` oder den unbestimmten Commitzustand.

`src/main.cpp`, `DevicePlatform` und `FermentationApplication` besitzen noch
keinen produktiven Store. Diese Composition-Root-Integration bleibt ausserhalb
von #56 und folgt erst mit dem realen Persistenzadapter und #57.

## Modul- und Dateischnitt

### `device_platform`

Keine Produktionsaenderung ist geplant. Die vorhandenen generischen
Speicherbausteine genuegen. Insbesondere werden keine Manifest-, Root-,
Preview- oder Fermentationsbegriffe in dieses Modul verschoben.

Falls die Umsetzung wider Erwarten eine Aenderung am generischen Storeport,
Envelope oder Slot-Scan erfordern sollte, ist dies eine materielle
Planabweichung. Die Arbeit wird dann vor der Aenderung angehalten und der Plan
erneut ownerfreigegeben.

### `device_platform_test_support`

Keine Produktions- oder Test-Support-Aenderung ist geplant. Spezifische
Write-/Read-Skripte fuer die #56-Matrix werden lokal im Anwendungstest als
schmaler `IStateStore`-Decorator implementiert. Dadurch werden keine
fachlichen #56-Zustaende in das anwendungsneutrale Testmodul eingebaut.

### `fermentation_app`

Voraussichtlich geaenderte bestehende Dateien:

- `lib/fermentation_app/src/configuration_storage_contract.hpp`
  - Record-Type-IDs und ADR-016-konforme Keys fuer drei Manifest- und zwei
    Rootslots;
- `lib/fermentation_app/src/configuration_limits.hpp`
  - feste Manifest-/Root-Payload- und Envelopegrenzen sowie die verbindlichen
    Workflow-Anzahlgrenzen;
- `lib/fermentation_app/src/configuration_documents.hpp`
  - nur falls fuer exakte typisierte Inhaltsvergleiche kleine
    wertsemantische Hilfen benoetigt werden; keine neue Dokumentsemantik;
- `lib/fermentation_app/src/configuration_documents.cpp`
  - nur die zugehoerigen exakten Inhaltsvergleiche.

Neue Produktionsdateien:

- `lib/fermentation_app/src/configuration_mutation_coordinator.hpp`
- `lib/fermentation_app/src/configuration_mutation_coordinator.cpp`
- `lib/fermentation_app/src/configuration_graph.hpp`
- `lib/fermentation_app/src/configuration_graph.cpp`
- `lib/fermentation_app/src/configuration_graph_codec.hpp`
- `lib/fermentation_app/src/configuration_graph_codec.cpp`
- `lib/fermentation_app/src/configuration_graph_store.hpp`
- `lib/fermentation_app/src/configuration_graph_store.cpp`
- `lib/fermentation_app/src/runtime_configuration_snapshot.hpp`
- `lib/fermentation_app/src/runtime_configuration_snapshot.cpp`
- `lib/fermentation_app/src/configuration_service.hpp`
- `lib/fermentation_app/src/configuration_service.cpp`

Verantwortungen:

- `configuration_mutation_coordinator.*`: genau eine konkrete gemeinsame
  Mutationskoordination fuer alle persistenten Konfigurationsmutationen, ohne
  Store-, Bootstrap-, Reset- oder Transaktionssemantik;
- `configuration_graph.*`: starke fachliche Referenz-, Manifest-, Root-,
  Schutzmengen-, Graph- und Diagnosemodelle sowie rein fachliche
  Gleichheits-/Plausibilitaetsregeln;
- `configuration_graph_codec.*`: einziges kanonisches Schema-1-Wireformat fuer
  Manifest und Root, ohne Storezugriff;
- `configuration_graph_store.*`: begrenztes Lesen, vollstaendige
  Graphvalidierung, kanonische Auswahl, High-Water-Scans, Slotvorplanung,
  Identitaetskollisionspruefung, modellfreier `ValidationOnly`-Scan waehrend
  einer Mutation, Writes und Readbacks ueber `IStateStore`;
- `runtime_configuration_snapshot.*`: unveraenderlicher Runtime-Snapshot,
  vorab vorbereiteter Publish-Handle, begrenzte
  `RuntimeConfigurationReadLease` und schmaler Publisher ohne nach aussen frei
  kopierbare `shared_ptr`-Handles;
- `configuration_service.*`: genau eine fluechtige Vorschau, der konkrete
  serviceweite Betriebszustand, die feste Modell-/Lease-Budgetierung,
  `ConfigurationPreviewBuildLease`, serialisierte Mutation, Konfliktpruefung,
  Commitorchestrierung und fail-closed Zustandsuebergaenge.

Es werden keine Arduino-, NVS-, GPIO-, WLAN-, Webserver-, JSON-, Lauf- oder
Safetytypen in diese Dateien aufgenommen.

### Native Tests

Neue Testdateien:

- `test/test_configuration_mutation_coordinator/test_configuration_mutation_coordinator.cpp`
- `test/test_configuration_graph_codecs/test_configuration_graph_codecs.cpp`
- `test/test_configuration_graph_store/test_configuration_graph_store.cpp`
- `test/test_configuration_service/test_configuration_service.cpp`

Die erste Datei deckt den gemeinsamen, nicht blockierenden Lease-Vertrag ab.
Die zweite deckt Golden-/Negativtests des Wireformats ab. Die dritte deckt
Rootauswahl, Graphvalidierung und Slotrotation ab. Die vierte deckt Vorschau,
Konflikt, Commit, Cut-Points, Runtime-Publish,
`ConfigurationCommitIndeterminate` und Ressourcenvertraege ab.

Alle anwendungsspezifischen Store-Decorator, Fehlerinjektionen und Fakes fuer
diese Tests liegen lokal in den jeweiligen `test/test_configuration_*`-
Verzeichnissen. Weder `fermentation_app` noch seine Anwendungstests haengen von
`device_platform_test_support` ab; sie verwenden ausschliesslich die schmalen
Ports aus `device_platform` und lokale Testimplementierungen.

### Dokumentation nach freigegebener Implementierung

Voraussichtlich anzupassen:

- `docs/CONFIGURATION_PERSISTENCE.md`
  - finaler Typname, abgeschlossener `MutationSequence`-Nachweis, konkrete
    Workflow-Anzahl- und Wiregrenzen sowie Implementierungsnachweis;
- `docs/IMPLEMENTATION_ISSUES.md`
  - nur eine sachliche Umsetzungsreferenz; kein vorzeitiges Schliessen oder
    Entsperren von #57;
- `CHANGELOG.md`
  - #56-Implementierung und Nachweise.

Keine Live-Issue-, Label-, Milestone- oder ADR-Aenderung ist Bestandteil des
Umsetzungs-PRs.

## Gemeinsame Mutationskoordination fuer #56 und #57

### Ort und schmale Schnittstelle

`ConfigurationMutationCoordinator` lebt als konkrete, anwendungsinterne
Klasse in `fermentation_app`. Sie kapselt genau ein exklusives Gate fuer alle
persistent zustandsaendernden Konfigurationsablaeufe. Ihre schmale
Produktionsschnittstelle besteht sinngemaess aus:

```text
tryAcquire() noexcept
  -> ConfigurationMutationLease
  -> ConfigurationMutationBusy
```

Die erfolgreiche Rueckgabe ist eine nicht kopierbare, verschiebbare
RAII-Lease. Intern darf der Koordinator genau ein nicht blockierend erworbenes
Mutex- beziehungsweise gleichwertiges Exklusivitaetsprimitiv verwenden. Er
kennt weder Store, Records, Rootslots noch Preview, Bootstrap, Migration,
Werksreset oder einzelne Operationstypen. Er ist insbesondere keine
Transaktionsplattform und fuehrt keine Callbacks, Warteschlange oder
dynamische Providerregistrierung ein.

### Gemeinsame Nutzung

- Der `ConfigurationService` aus #56 erhaelt den Koordinator als Referenz und
  besitzt weder einen eigenen Mutationsmutex noch ein zweites persistentes
  Mutationsgate.
- Eine normale Aktivierung und eine durch #56 ausgefuehrte Schema-1-Migration
  erwerben dieselbe Lease vor den exklusiven Basis-, Konflikt-, Zaehler- und
  Slotpruefungen.
- Der spaetere #57-Bootstrap-/Recoverydienst erhaelt im Composition Root exakt
  dieselbe Koordinatorinstanz. Bootstrap und Werksreset erwerben diese Lease,
  ohne dass #56 dafuer umgebaut oder mit #57-Logik ergaenzt wird.
- Die Composition-Root-Erzeugung und die produktive #57-Verkabelung bleiben
  ausserhalb dieses Issues. #56 stellt nur den wiederverwendbaren konkreten
  Koordinator sowie seine Nutzung durch den `ConfigurationService` bereit.
- Reine Reads und die rechenintensive Vorbereitung einer Vorschau benoetigen
  keine Lease. Die Bestaetigung erwirbt sie vor der erneuten exklusiven
  Pruefung und haelt sie durch alle Writes, die Bestimmung des
  Root-Commitresultats und den minimalen Snapshot-Publish. Sie wird erst nach
  dem erreichten Endzustand freigegeben.
- `ConfigurationMutationBusy` beendet den Versuch ohne Storezugriff und ohne
  Teilwirkung. Der Aufrufer darf spaeter bewusst erneut versuchen; #56 wartet
  nicht blockierend und startet keine zweite Mutation.

Damit gilt global hoechstens eine persistente Konfigurationsmutation
gleichzeitig, einschliesslich normaler Aktivierung, Migration sowie spaeterem
Bootstrap und Werksreset. Ein zweiter unabhaengiger Mutex in #57 oder im
Graphstore waere eine materielle Planabweichung. Der spaetere #57-Plan muss die
gemeinsame Instanz explizit als Abhaengigkeit uebernehmen.

## Persistenter Schema-1-Vertrag

### Starke Typen

`fermentation_app` fuehrt getrennte starke Typen ein:

- `ConfigurationManifestGeneration`;
- `ConfigurationRootSequence`.

Sie duerfen weder untereinander noch mit Dokumentrevisionen, `StorageEpoch`
oder einer spaeteren `BootstrapSequence` implizit vermischt werden. Die rohen
`versionValue`-Bytes des generischen Envelopes werden erst nach technischer
Pruefung in den erwarteten fachlichen Typ ueberfuehrt.

### Record-Type-IDs und Keys

Die bestehenden IDs 1 bis 3 und Dokumentkeys bleiben unveraendert. Schema 1
ergaenzt:

| Record | `RecordTypeId` | Slots | Keys |
|---|---:|---:|---|
| `ConfigurationManifest` | 4 | 3 | `cm0`, `cm1`, `cm2` |
| `ConfigurationRootRecord` | 5 | 2 | `cr0`, `cr1` |

ID 6 und weitere IDs werden nicht fuer #57 oder Secrets vorreserviert.

### Exakte Referenzbindung

Jede Dokumentreferenz und jede Manifestreferenz enthaelt in kanonischer
Big-Endian-Reihenfolge:

| Feld | Breite |
|---|---:|
| Record-Type-ID | `uint16` |
| Slot-ID | `uint32` |
| Revision beziehungsweise Generation | `uint64` |
| Schema-Version | `uint32` |
| Payloadlaenge | `uint32` |
| Payload-CRC-32/ISO-HDLC | `uint32` |
| `StorageEpoch` | `uint64` |

Eine Referenz ist damit exakt 34 Byte breit. Die referenzierte Payload-CRC
bezieht sich ausschliesslich auf die kanonischen Payloadbytes. Zusaetzlich
bleibt die Envelope-CRC ueber Header und Payload verbindlich. Beim Laden
werden beide Ebenen geprueft:

1. der generische Envelope-Decoder prueft seinen eigenen CRC- und
   Strukturvertrag;
2. `fermentation_app` prueft Recordtyp, Slot, Version, Schema, Epoche,
   Payloadlaenge und neu berechnete Payload-CRC gegen die Referenz;
3. erst danach wird die Payload fachlich dekodiert und validiert.

Die Payload-CRC ist Integritaets- und Referenzbindung, kein
Authentifizierungs- oder Manipulationsschutz. Inhaltsgleichheit und
`NoChange` werden nie allein aus einem CRC abgeleitet.

### `ConfigurationManifest` Schema 1

Der Envelope traegt:

- Record-Type-ID 4;
- Schema-Version 1;
- die aktuelle `StorageEpoch`;
- die `ConfigurationManifestGeneration` als `versionValue`;
- optional vertrauenswuerdige UTC-Metadaten ohne Ordnungswirkung.

Die Payload enthaelt exakt:

1. `ChangeOrigin` als `uint8`-Wire-ID;
2. `ChangeOperation` als `uint8`-Wire-ID;
3. eine `UserConfiguration`-Referenz;
4. eine `ServiceConfiguration`-Referenz;
5. eine `ProgramCatalog`-Referenz.

Bekannte Origin-IDs bleiben `InternalSystem`, `LocalDisplay` und
`WebInterface`; bekannte Operationen bleiben `NormalEdit`,
`FactoryInitialization`, `BackupImport`, `SchemaMigration`, `FactoryReset`
und `StandardProgramReset`. Unbekannte IDs werden als `Unknown` zusammen mit
ihrem rohen Byte erhalten und nicht still normalisiert.

Die Manifestpayload ist exakt 104 Byte gross. Das maximale Envelope mit
optionalem UTC-Feld ist 149 Byte gross. Fehlende oder zusaetzliche Bytes,
reservierte Nullwerte, falsche Recordtypen, ungueltige Slots und
Epochenabweichungen werden abgelehnt.

Schema 1 referenziert weder Connectivity noch Authentication, Secrets,
Pending oder einen Lauf.

### `ConfigurationRootRecord` Schema 1

Der Envelope traegt:

- Record-Type-ID 5;
- Schema-Version 1;
- dieselbe `StorageEpoch` wie der Graph;
- die `ConfigurationRootSequence` als `versionValue`;
- optional vertrauenswuerdige UTC-Metadaten ohne Ordnungswirkung.

Die Payload enthaelt exakt:

1. die Active-Manifestreferenz;
2. einen Optionaltag `0x00` oder `0x01`;
3. nur bei `0x01` die Fallback-Manifestreferenz.

Ohne Fallback ist die Payload exakt 35 Byte, mit Fallback exakt 69 Byte. Das
maximale Envelope mit optionalem UTC-Feld ist 114 Byte gross. Andere Tags,
Trailing Bytes oder Active und Fallback mit identischer Referenz werden
abgelehnt.

Ein fehlender Fallback ist nur fuer den von #57 spaeter erzeugten ersten
Bootstrapgraphen gueltig. Jeder normale erfolgreiche #56-Commit setzt:

```text
Active   = neues vollstaendig validiertes Manifest
Fallback = vorheriger kanonischer Active- beziehungsweise Fallback-Zweig
```

War der bisher kanonisch verwendete Zweig der Fallback eines Rootkandidaten,
wird genau dieser tatsaechlich verwendete vollstaendige Graph neuer Fallback.
Ein technisch vorhandener, aber unbrauchbarer Active-Zweig wird nicht als
Rueckfallgeneration weitergereicht.

### Fachliche Identitaet und High-Water-Marks

Revision, Generation und Sequenz sind innerhalb einer `StorageEpoch` keine
blossen Zaehlerstaende des aktuell kanonischen Graphen, sondern stabile
fachliche Identitaeten:

- Eine Dokumentrevision identifiziert je Dokumenttyp und `StorageEpoch` genau
  einen kanonischen Dokumentinhalt.
- Eine `ConfigurationManifestGeneration` identifiziert je `StorageEpoch`
  genau eine kanonische Manifestpayload.
- Eine `ConfigurationRootSequence` identifiziert je `StorageEpoch` genau einen
  kanonischen Rootrecordinhalt.
- Unterschiedliche Inhalte duerfen deshalb auch nach einem abgebrochenen
  Vor-Root-Versuch niemals denselben Wert erhalten.
- Derselbe Wert darf nur fuer exakt denselben kanonischen Recordinhalt im
  selben Dokumenttyp- beziehungsweise Manifest-/Root-Identitaetsvertrag
  erneut verwendet werden. Eine solche byte- und identitaetsgleiche
  Wiederverwendung ist ein explizit erkannter Wiederholungsfall, keine freie
  Neuvergabe.

#56 sucht verwaiste Records nicht als allgemeine Deduplizierung. Die erlaubte
Wiederverwendung desselben Werts ist auf den Readback beziehungsweise die
idempotente Aufloesung des exakt vorbereiteten Writeversuchs mit identischen
erwarteten kanonischen Bytes begrenzt. Jeder spaetere neue Writeversuch fuer
einen anderen Inhalt verwendet `HighWaterMark + 1`.

Vor dem ersten Write bestimmt der Graphstore unter der gemeinsamen
Mutationslease folgende High-Water-Marks:

1. fuer jeden tatsaechlich geaenderten Dokumenttyp ueber alle vier
   zugehoerigen Dokumentslots;
2. fuer `ConfigurationManifest` ueber alle drei Manifest-Slots;
3. fuer `ConfigurationRootRecord` ueber beide Rootslots.

Jeder vollstaendig gelesene und technisch gueltige Record derselben
`StorageEpoch` traegt mit seinem starken `versionValue` zum zugehoerigen
Maximum bei. Das gilt auch fuer verwaiste, nicht kanonische, fachlich
ungueltige oder nicht aktive Records. `NotFound` traegt nichts bei. Der neue
Wert fuer einen anderen Inhalt ist jeweils `maximal beobachteter gueltiger
Wert + 1`; der checked increment wird vor jedem Write ausgefuehrt und lehnt
Null sowie Ueberlauf typisiert ab.

Ein `ReadError` oder `CapacityError` in einem fuer diesen Nachweis
erforderlichen Slot macht den High-Water-Mark unbekannt und blockiert die
gesamte Mutation vor jedem Write. Ein vollstaendig technisch gueltiger Record
mit unbekanntem neuerem Schema wird weder still ignoriert noch ueberschrieben:
Er traegt zwar mit seinem Envelope-`versionValue` zum High-Water-Mark bei,
sperrt aber die Mutation mit einem typisierten
`UnsupportedNewerConfigurationSchema`, weil seine fachliche Identitaet und
sichere Wiederverwendbarkeit nicht nachgewiesen werden koennen.

Die High-Water-Scans und die darauf basierenden Werte werden innerhalb
derselben Mutationslease nicht aus einem Cache oder nur aus dem aktuell
verwendeten aelteren Graphen abgeleitet. Unveraenderte Dokumenttypen behalten
ihre bestehende exakte Referenz und benoetigen keinen neuen Revisionswert;
ihre Slots bleiben dennoch Teil der normalen Graph- und Schutzmengenpruefung.

### Persistente Identitaetskollisionspruefung

Der normale Graphload und jeder High-Water-Scan bilden fuer dieselbe
`StorageEpoch` einen begrenzten Identitaetsindex ueber alle vier Slots jedes
Dokumenttyps, alle drei Manifest-Slots und beide Rootslots. Der Index haelt nur
Recordart, starken Identitaetswert, Slot und kanonischen Recordfingerprint.
Treffen zwei technisch gueltige Records auf dieselbe starke Identitaet, werden
beide Records fuer den abschliessenden bytegenauen Vergleich nochmals mit
ihrem jeweiligen harten Recordlimit gelesen. Nur fuer diesen Vergleich duerfen
gleichzeitig exakt zwei begrenzte Recordbytepuffer leben; danach werden beide
vor weiterer Dekodierung oder jedem Write freigegeben. Ein `ReadError` oder
`CapacityError` beim Vergleich ist ein fail-closed Scanfehler, kein
Gleichheitsresultat; im bereits operationalen Dienst wird er als
`PersistentGraphVerificationFailure` behandelt. Ein Fingerprint oder CRC
allein beweist niemals das byteidentische Duplikat.

Verbindlich gilt:

- Gleiche Dokumentart und gleiche Revision mit exakt identischen kanonischen
  Envelope-/Payloadbytes sind ein diagnostizierbares exaktes Duplikat. Die
  Identitaet bleibt belegt und wird keinem anderen Inhalt neu zugeteilt.
- Gleiche Dokumentart und gleiche Revision mit unterschiedlichen kanonischen
  Bytes sind `ConfigurationGraphIntegrityFailure` beziehungsweise bei einem
  bereits laufenden Dienst ein stabiler
  `PersistentConfigurationIdentityCollision`. Das gilt fuer
  `UserConfiguration`, `ServiceConfiguration` und `ProgramCatalog` unabhaengig
  davon, ob beide, einer oder keiner der Records referenziert sind.
- Gleiche `ConfigurationManifestGeneration` mit identischen kanonischen
  Manifestbytes ist ein diagnostizierbares exaktes Duplikat; unterschiedliche
  Bytes unter derselben Generation sind derselbe fail-closed
  Integritaetsfehler.
- Fuer gleiche `ConfigurationRootSequence` gilt die unten definierte identische
  Rootregel entsprechend.

Eine bestehende Kollision unterschiedlicher Inhalte verhindert normale
Runtimefreigabe, weitere Mutation und Slotwiederverwendung. Wird sie beim
Pre-Write-Scan eines zuvor operationalen Dienstes entdeckt, wechselt dieser
am serviceweiten Zustandslinearisierungspunkt in `RuntimeFailure` mit der
redigierten Ursache `PersistentConfigurationIdentityCollision`; die spaetere
#24-Verarbeitung bleibt davon getrennt. Die Pruefung ist kein blosser
Erzeugungstest fuer neue Records, sondern erkennt bereits vorhandene
technisch gueltige Kollisionsrecords in allen Slots.

## Kanonische Graphvalidierung

### Ergebnisvertrag

Der Graphloader liefert genau einen der typisierten Zustaende:

- `ConfigurationGraphAvailable` mit kanonischem Graph,
  `RuntimeConfigurationSnapshot`-Vorbereitung und Diagnosen;
- `ConfigurationGraphUnavailable`, wenn kein vollstaendiger Graph nutzbar ist;
- `ConfigurationGraphIntegrityFailure`, wenn technische Kandidaten vorhanden,
  aber nicht vollstaendig und widerspruchsfrei validierbar sind;
- `ConfigurationGraphLoadFailure` mit stabil unterscheidbarer Ursache
  `RootReadError` beziehungsweise `RootCapacityError`, wenn bereits ein
  Rootslot nicht sicher lesbar ist und deshalb die globale Rootreihenfolge
  nicht bestimmt werden kann;
- weitere typisierte Graph-Read-/Capacity-Fehler fuer Storefehler beim Laden
  eines bereits eindeutig geordneten Rootkandidaten, die ebenfalls nicht als
  `NotFound` umgedeutet werden duerfen.

Die spaeteren extern sichtbaren #57-Namen `ConfigurationUnavailable` und
`ConfigurationIntegrityFailure` werden in #56 weder vorweggenommen noch
implementiert. #57 bildet seine Bootstrap-/Bootsemantik auf den
Graphloadervertrag ab.

### Auswahlalgorithmus

Fuer eine vom Aufrufer vorgegebene gueltige `StorageEpoch` gilt diese
Prioritaetsregel:

1. beide Rootslots technisch und voneinander unabhaengig lesen;
2. `NotFound` als sicher abwesenden Slot diagnostizieren und ueberspringen;
3. liefert auch nur ein Rootslot `ReadError` oder `CapacityError`, sofort den
   typisierten `ConfigurationGraphLoadFailure` liefern: Der moeglicherweise
   unlesbare Slot koennte den global neuesten Root enthalten, daher darf weder
   ein anderer aelterer Root als kanonisch behauptet noch eine Runtime
   freigegeben werden;
4. vollstaendig gelesene Rootrecords technisch pruefen; ein vollstaendig
   gelesener, aber wegen Envelope, CRC, Schema, Payload oder Fachsemantik
   ungueltiger Rootkandidat darf diagnostiziert und zugunsten eines aelteren
   vollstaendig gueltigen Kandidaten uebersprungen werden;
5. technisch passende Kandidaten absteigend nach `rootSequence` untersuchen;
   tragen beide Rootslots denselben Sequenzwert, ist eine deterministische
   Slotauswahl nur bei exakt identischen kanonischen Rootbytes des gesamten
   Envelope- und Payloadrecords zulaessig und wird diagnostiziert;
   unterscheiden sich Rootpayload, Referenzen oder kanonische Bytes, liefert
   der Loader
   `ConfigurationGraphIntegrityFailure`, gibt keine Runtime frei und benutzt
   keinen Slot-Tiebreak als Aktivierungsentscheid;
6. Rootpayload und jede Referenz vollstaendig pruefen;
7. zuerst den Active-Zweig des Kandidaten laden;
8. fuer jede Dokumentkante Envelope, Referenzbindung, Schema, Payload und
   fachliches Dokument validieren;
9. ist Active unbrauchbar, den Fallback desselben Roots vollstaendig pruefen;
10. den ersten vollstaendig nutzbaren Zweig als kanonisch waehlen;
11. bei gueltigem Active einen vorhandenen Fallback zusaetzlich vollstaendig
    validieren, bevor er als geschuetzte nutzbare Rueckfallgeneration gilt;
12. Fallbacknutzung, unbrauchbaren Fallback und uebersprungene hoehere
    Rootkandidaten stabil und ohne Payloaddaten diagnostizieren.

Ein hoher Sequenzwert, ein gueltiger Envelope, ein gueltiges Manifest oder ein
einzeln gueltiges Dokument aktiviert niemals allein einen Graphen. Ein
`ReadError` oder `CapacityError` ist niemals fabrikleerer Speicher. Speziell
beim Rootscan besitzt der fail-closed Ladefehler Vorrang vor jedem anderen
vollstaendig gueltigen, aber moeglicherweise aelteren Root. #57 bildet diesen
Producer spaeter auf seine Boot-/Recovery- und Safetygrenze ab; #56 startet
weder Bootstrap noch Factory-Fallback.

Der Loader haelt nur den schliesslich ausgewaehlten typisierten Graphen. Beim
Pruefen verworfener Kandidaten und des Fallbacks werden grosse Payloads nach
der jeweiligen Validierung freigegeben; es wird keine Sammlung aller
ProgramCatalog-Payloads aufgebaut.

Waehrend einer #56-Mutation mit bereits publiziertem aktivem Snapshot und
reserviertem Previewkandidaten verwendet der Graphstore nicht erneut diesen
vollmodellbildenden Ladepfad. Sein `ValidationOnly`-Scan liest alle benoetigten
Slots und prueft Rootordnung, exakte Active-Bindung, Referenzen, technische
Integritaet, High-Water-Marks und Identitaetskollisionen mit begrenzten
Recordbytepuffern gegen den bereits validierten aktiven Snapshot. Ein neuerer
technisch vorhandener Root, der nicht exakt als erwartete Active-Basis
nachgewiesen ist, blockiert die Mutation und Runtimefreigabe fail closed; er
wird nicht als drittes Vollmodell geladen oder still uebersprungen.

### Runtime-Snapshot aus einem Graphen

Der unveraenderliche Snapshot enthaelt mindestens:

- `StorageEpoch`;
- vollstaendige Active-Manifestreferenz und `ConfigurationManifestGeneration`;
- die drei konkreten Dokumentrevisionen;
- unveraenderliche typisierte `UserConfiguration`, `ServiceConfiguration`
  und `ProgramCatalog`;
- die bereits erfolgreich vorbereitete `PreparedTimeZone`;
- die fuer Leser erforderlichen, nicht geheimen Aktivierungsmetadaten.

Der Snapshot enthaelt keine Store-, Slot-, Request-, Web-, Lauf-, Safety- oder
Secretobjekte. Grosse unveraenderliche Dokumente werden zwischen kanonischem
Graphen, Vorschaukandidat und vorbereitetem Snapshot ueber unveraenderliche
Besitzobjekte geteilt; sie werden nicht fuer jeden Schritt erneut vollstaendig
kopiert.

## Schutzmenge und Slotrotation

### Kanonische Schutzmenge

Vor einer Mutation wird eine unveraenderliche Schutzmenge gebildet aus:

- allen Dokument- und Manifestslots des aktuell kanonisch verwendeten Graphen;
- allen Dokument- und Manifestslots seines vollstaendig validierten nutzbaren
  Fallbacks, soweit vorhanden;
- waehrend der Mutation allen bereits gewaehlten oder geschriebenen
  Zielslots.

Aeltere Rootkopien sind nur technische Bootkandidaten. Sie erweitern die
fachliche Schutzmenge nicht. Ein ausschliesslich von einer aelteren,
nichtkanonischen Rootkopie referenzierter Slot darf wiederverwendet werden.

### Vollstaendige Vorplanung

Noch vor dem ersten Write werden unter der exklusiven Mutation:

1. Basis und Vorschau erneut geprueft;
2. geaenderte Dokumente exakt bestimmt;
3. die High-Water-Scans fuer jeden geaenderten Dokumenttyp, alle Manifest-Slots
   und beide Rootslots vollstaendig und ohne Lese-/Kapazitaetsfehler
   abgeschlossen;
4. unbekannte neuere technisch gueltige Schemas ausgeschlossen;
5. alle benoetigten Revisionen, die Manifestgeneration und `rootSequence` als
   checked `HighWaterMark + 1` fuer den jeweils neuen Inhalt berechnet;
6. fuer jeden geaenderten Dokumenttyp ein ungeschuetzter Zielslot bestimmt;
7. ein ungeschuetzter Manifestplatz bestimmt;
8. der andere der beiden Rootslots als Ziel bestimmt;
9. alle ausgewaehlten Ziele zur lokalen Mutationsschutzmenge hinzugefuegt;
10. alle Payload- und Envelopegroessen geprueft;
11. der vollstaendige Runtime-Snapshot samt Plattformwerten vorbereitet;
12. die neue Manifest- und Rootstruktur im RAM vollstaendig validiert.

Fehlt fuer irgendeinen Recordtyp ein sicherer Platz, endet die Mutation vor
jedem Write mit `NoUnreferencedSlotAvailable` und dem betroffenen Recordtyp.
Eine teilweise Slotreservierung wird nicht sichtbar und schreibt nichts.

### Deterministische Wahl

- Fuer einen geaenderten Dokumenttyp beginnt die Suche zyklisch hinter dessen
  Slot im aktuell kanonischen Graphen und ueberspringt jeden geschuetzten oder
  bereits reservierten Slot.
- Fuer das neue Manifest beginnt sie zyklisch hinter dem aktuell kanonischen
  Manifest und ueberspringt Active-/Fallback-/Mutationsschutz.
- Der Rootwrite verwendet immer den anderen der zwei Rootslots als den Slot,
  aus dem der kanonische Zweig geladen wurde.
- Ein unveraendertes Dokument behaelt Revision und exakte Referenz; es wird
  weder neu kodiert noch geschrieben.

Der Test startet mit einem gueltigen Graphen ohne Fallback und fuehrt
mindestens fuenf aufeinanderfolgende Aktivierungen mit wechselnden
Dokumentkombinationen aus. Nach jedem Commit muessen Active, exakt eine
nutzbare vorherige Generation als Fallback und die freigegebenen
Wiederverwendungsslots eindeutig sein.

## Serviceweiter Betriebs-, Modell- und Lebensdauervertrag

### Konkreter Betriebszustand und Linearisierung

`ConfigurationService` besitzt einen kleinen internen, mit genau einem
serviceeigenen Mutex geschuetzten Zustandsblock. Dieser Mutex ist kein zweites
persistent wirksames Mutationsgate: Storewrites bleiben ausschliesslich durch
die gemeinsame `ConfigurationMutationLease` serialisiert. Der Zustandsblock
enthaelt mindestens:

```text
ConfigurationServiceMode:
  Operational
  CommitInProgress
  CommitIndeterminate
  RuntimeFailure

ConfigurationServiceStateRevision
sichtbarer Preview-Handle
aktive Runtimegeneration
Reader- und Modellbudgetzaehler
```

Die fluechtige `ConfigurationServiceStateRevision` beginnt bei 1 und wird bei
jedem Moduswechsel checked fortgeschrieben. Ein Ueberlauf ist eine interne
Vertragsverletzung und wechselt fail closed in `RuntimeFailure`; er wird nicht
auf 1 zurueckgesetzt. Previewberechnung erfasst Modus und Revision. Direkt vor
der Installation prueft sie unter demselben Mutex erneut, dass der Modus
`Operational` und die Revision unveraendert ist.

- `Operational` erlaubt innerhalb der unten definierten Budgets neue
  Previewberechnung, Previewinstallation, normale Runtime-Read-Leases und den
  Start einer Mutation.
- Der Commitstart erfasst den bestaetigten Preview-Handle, erwirbt die
  gemeinsame Mutationslease und wechselt unter dem Zustandsmutex atomar zu
  `CommitInProgress`. Die dabei vor Root erworbene Zustandslease bleibt bis
  zum eindeutigen Commit-, Publish- oder fail-closed Endzustand gehalten. In
  diesem Zustand starten oder installieren keine neuen Previews und es werden
  keine neuen normalen Runtime-Read-Leases ausgegeben; bereits ausgegebene
  Leases bleiben gueltig.
- Ein eindeutig vor Root abgebrochener Commit wechselt nur dann mit neuer
  Zustandsrevision zu `Operational`, wenn der bisherige kanonische Graph
  weiterhin positiv und widerspruchsfrei feststeht, etwa bei Kandidatenfehler,
  Konflikt, normaler Budgetauslastung oder eindeutig unwirksamem Write. Ein
  `ConfigurationGraphLoadFailure`, `ConfigurationGraphIntegrityFailure`, eine
  persistente Identitaetskollision oder interne Invariantenverletzung wechselt
  dagegen fail closed zu `RuntimeFailure`. Die identitaetsgebundene
  Previewbereinigung darf in keinem Fall eine fremde Vorschau entfernen.
- Bestaetigter Root-Commit, Snapshottausch und Rueckkehr zu `Operational`
  werden unter derselben bereits vor Root erworbenen Zustandslease
  linearisiert. Eine Akquisition sieht eindeutig den alten Snapshot vor
  Commitstart oder den neuen nach Freigabe der Zustandslease.
- Der Eintritt in `CommitIndeterminate` oder `RuntimeFailure` ist ein einziger
  serviceweiter Linearisierungspunkt unter diesem Mutex: Modus und Revision
  wechseln, neue Previewinstallation und normale Snapshotakquisition sind
  gesperrt, und der sichtbare Previewslot wird vollstaendig geleert. Ein vor
  dem Fehler gestarteter Previewaufbau scheitert spaeter an Modus oder
  Revision und installiert nichts.

Der sichtbare Previewslot, Modus, Zustandsrevision, Runtimepublisher und alle
Budgetzaehler werden nur unter diesem Zustandsmutex veraendert. Atomare
Snapshotbesitzoperationen duerfen intern Teil des Publish sein, ersetzen aber
nicht diese gemeinsame Zustandslinearisierung. Damit gibt es keine
ungeschuetzte Data Race zwischen Preview, Runtimeakquisition, Publish und
fail-closed Uebergang.

Bereits vor einem fail-closed Uebergang ausgegebene unveraenderliche
`RuntimeConfigurationReadLease`-Objekte bleiben speichersicher und lesbar.
Sie sind keine neue normale Runtimefreigabe und erlauben keine Mutation. Nach
dem Linearisierungspunkt werden keine neuen normalen Read-Leases ausgegeben.
Die spaetere persistente Verriegelung, `SAFE_BOOT`- und Aktorwirkung bleiben
beim `CONFIGURATION_SAFETY_INTEGRATION_GATE` in #24.

### Erzwingbares Modell- und Readerbudget

Fuer #56 gelten folgende festen Softwareobergrenzen; die Konstanten liegen in
`configuration_limits.hpp`:

```text
kMaxDistinctConfigurationModelGenerations = 2
kMaxRuntimeConfigurationReadLeases = 8
kMaxConcurrentFullPreviewBuilds = 1
```

Eine vollstaendige Modellgeneration bezeichnet insbesondere einen
vollstaendigen `ProgramCatalog` samt zugehoerigem typisiertem
Konfigurationsmodell. Der aktive Runtime-Snapshot belegt eine Generation.
Genau eine weitere unterschiedliche vollstaendige Generation darf gleichzeitig
existieren und ist exklusiv einem der folgenden Zustaende zugeordnet:

- einer reservierten vollen Previewerstellung;
- einem sichtbaren oder vom Commit erfassten geaenderten Previewkandidaten;
- dem vorbereiteten neuen Runtime-Snapshot waehrend des Commits;
- einer nach Publish noch durch Publisher oder Reader-Leases gepinnten alten
  Runtimegeneration.

Preview, Commit und Runtimepublisher verwenden dafuer einen kleinen konkreten
`ConfigurationModelReservation`-Vertrag innerhalb von
`ConfigurationService`; er ist weder Persistenztransaktion noch allgemeiner
Ressourcenprovider. Die Reservierung wird **vor** Aufbau oder tiefer Kopie
eines vollen Kandidaten nicht blockierend erworben. Ist bereits eine andere
volle Previewerstellung, ein geaenderter Previewkandidat oder eine alte
Runtimegeneration die zweite Modellgeneration, endet die neue Previewanfrage
typisiert mit `ConfigurationModelBudgetBusy`, bevor ein weiteres Vollmodell
entsteht. Zwei unterschiedliche maximale Previewkandidaten werden daher nicht
gleichzeitig aufgebaut oder gehalten.

Der oeffentliche Previeweinstieg erwirbt dazu zuerst eine nicht kopierbare
`ConfigurationPreviewBuildLease`, welche diese Modellreservierung besitzt.
Erst ueber diese Lease darf aus begrenzten Aenderungsdaten ein voller Kandidat
aufgebaut oder ein bereits unter derselben Reservierung eindeutig besessener
Kandidat per Move uebergeben werden. Es gibt keine #56-Schnittstelle, die einen
ausserhalb der Reservierung bereits vollstaendig kopiert gehaltenen
ProgramCatalog nochmals tief kopiert. Damit gilt die Modellobergrenze nicht
nur fuer serviceinterne Handles, sondern bereits am Kandidatenaufbau.

Der Graphstore besitzt zwei klar getrennte Lesemodi. Beim initialen Laden ohne
aktive Runtime darf er genau die eine kuenftige Runtimegeneration aufbauen.
Waehrend einer Mutation mit aktiver Runtime und reserviertem Kandidaten arbeitet
er dagegen im `ValidationOnly`-Modus: Root-, Referenz-, High-Water- und
Kollisionspruefungen verwenden begrenzte Recordbytes, Fingerprints und den
bereits validierten aktiven Snapshot, bauen aber kein drittes typisiertes
ProgramCatalog-/Konfigurationsmodell auf. Ein nicht exakt an die erwartete
Active-Basis bindbarer neuerer Graph blockiert vor jedem Write, statt waehrend
dieser Mutation als drittes Vollmodell geladen zu werden.

Beim erfolgreichen Publish wechselt die eine Zusatzreservierung ohne
Allokation vom vorbereiteten neuen Kandidaten auf die alte Runtimegeneration:
Der Kandidat wird neuer Active, die bisherige Active-Generation bleibt nur
solange als zweite Generation registriert, wie der uebernommene
Publisher-Handle oder mindestens eine Read-Lease sie haelt. Erst wenn beide
weg sind, ist die Zusatzreservierung wieder frei. Solange eine alte Generation
gepinnt ist, kann keine weitere unterschiedliche Previewgeneration aufgebaut
und damit kein weiterer Root-Commit vorbereitet werden. Der Dienst prueft
dieses Post-Publish-Budget vor dem ersten Write; ein unbekanntes oder
ausgeschoepftes Budget liefert `ConfigurationModelBudgetBusy` ohne Write.

Normale Leser erhalten keinen frei kopierbaren `shared_ptr`. Sie erwerben ueber
den Dienst eine nicht kopierbare, verschiebbare
`RuntimeConfigurationReadLease`, die genau eine registrierte Generation und
einen von maximal acht Leserslots haelt. Lease-Erwerb und -Freigabe aktualisieren
die festen Zaehler unter dem Zustandsmutex. Der neunte gleichzeitige Leser wird
nicht blockierend mit `RuntimeReadLeaseBusy` abgelehnt. Eine Lease darf lange
leben; dadurch bleibt hoechstens eine alte Generation gepinnt und jede weitere
Aktivierung wird vor ihrem ersten Write blockiert. Nach Freigabe der letzten
Lease und des alten Publisher-Handles wird die Generation ausserhalb des
kritischen Publish-Schritts zerstoerbar und die naechste Aktion wieder
zulaessig.

Die Akquisition liefert ein stabiles getaggtes Ergebnis und niemals einen
mehrdeutigen Nullzeiger: `RuntimeLeaseGranted` mit der Lease,
`RuntimeReadLeaseBusy` bei erschoepftem Leserbudget oder
`ConfigurationRuntimeUnavailable`, sobald der Servicezustand keine neue
normale Runtimefreigabe erlaubt.

Damit existieren zu keinem Zeitpunkt drei unterschiedliche vollstaendige
ProgramCatalog-/Konfigurationsgenerationen. Besitzhandles auf dieselbe
Generation erzeugen keine Modellkopie, sind aber durch die feste Readerzahl
ebenfalls begrenzt. Der Base-/Head- und Peak-Allokationsbericht muss die
tatsaechliche maximale Zahl unterschiedlicher Vollmodelle, Read-Leases,
Previewreservierungen und Besitzhandles nennen.

## Fluechtige Vorschau und Konfliktschutz

### Lebenszyklus und Anzahlgrenze

Der `ConfigurationService` haelt global genau eine sichtbare Vorschau. Das ist
die verbindliche R1-Obergrenze fuer #56.

- Jede sichtbare Vorschau besitzt einen unveraenderlichen, rein fluechtigen
  internen Handle, der ihre Identitaet und Lebensdauer bindet. Der Handle ist
  weder Authentisierung noch persistentes Preview-, Wiederaufnahme- oder
  Autorisierungstoken und wird nach Neustart verworfen.
- Der sichtbare Slot ist konkret ein intern besessener
  `shared_ptr<const ConfigurationPreview>`. Installation, Erfassung und
  identitaetsgebundene Entfernung erfolgen unter dem serviceweiten
  Zustandsmutex. Das entspricht fuer den Slot einer klaren
  Compare-and-exchange-Semantik mit dem erfassten Handle als Erwartungswert,
  ohne ein zweites Mutationsgate oder eine Previewplattform einzufuehren.
- Eine ungueltige oder nicht vorbereitbare neue Anfrage laesst eine bereits
  sichtbare Vorschau unveraendert.
- Genau eine volle Previewerstellung darf ihre Modellreservierung halten. Eine
  zweite unterschiedliche volle Previewanfrage endet nicht blockierend als
  `ConfigurationModelBudgetBusy`, bevor ein weiterer maximaler Kandidat tief
  aufgebaut wird. Der Installationslinearisierungspunkt hinterlaesst genau
  einen eindeutig aktuellen sichtbaren Handle.
- Die Bestaetigung erfasst unter derselben Preview-Synchronisation exakt den
  unveraenderlichen Handle, den sie committen will. Dieser Besitz verhindert
  Use-after-free, auch wenn eine neuere Vorschau parallel sichtbar wird.
- Eine alte Bestaetigung, deren erfasster Handle bereits vor der exklusiven
  erneuten Pruefung ersetzt wurde, endet als typisierter
  `PreviewSuperseded`-/Konfliktzustand ohne Write.
- `Abbrechen`, erfolgreicher Commit oder erkannter Konflikt entfernt per
  Compare-and-exchange ausschliesslich den dafuer erfassten Handle. Ist
  inzwischen eine neuere Vorschau sichtbar, bleibt sie unveraendert erhalten.
- Neustart verwirft den gesamten fluechtigen Previewslot.
- Es gibt keine zeitbasierte Ablaufregel und keine Abhaengigkeit von UTC.
- Es gibt keine persistente Preview, keinen Preview-Owner und kein
  Autorisierungs- oder Wiederaufnahmetoken.

Display- und Webadapter duerfen eine redigierte Darstellung halten, aber nicht
den Kandidaten rekonstruieren. Der Konfigurationsdienst committet nur seinen
eigenen aktuell gehaltenen unveraenderlichen Kandidaten.

### Inhalt

Die Vorschau enthaelt:

- einen tief besessenen und danach unveraenderlichen vollstaendigen Kandidaten;
- die exakte erwartete Active-Manifestreferenz einschliesslich Epoche,
  Generation, Slot, Laenge und CRC;
- eine `ConfigurationCandidateIntegrity` aus Schema, Laenge und CRC jeder
  kanonisch kodierten Dokumentpayload;
- eine typisierte Aenderungsmaske fuer User-, Service- und Programmdokument;
- eine typisierte redigierte Aenderungszusammenfassung;
- die Aktivierungswirkung `Immediate`, die einzige R1-Wirkung in #56;
- alle bereits erfolgreich vorbereitbaren Plattformwerte.

Die Integritaetskennung ist keine Authentisierung. Sie bindet die
serviceeigene unveraenderliche Vorschau an die Bestaetigung. Autorisierung,
Session und CSRF bleiben ausserhalb von #56.

### Erstellung

1. vollstaendigen typisierten Kandidaten uebernehmen und von
   aufruferveraenderbaren Objekten entkoppeln;
2. UserConfiguration inklusive Zeitzone validieren und vorbereiten;
3. ServiceConfiguration und ProgramCatalog vollstaendig validieren;
4. Kandidateninhalte exakt, nicht nur per CRC, gegen den aktiven Snapshot
   vergleichen;
5. kanonische Payloads einzeln kodieren und deren Integritaet bestimmen;
6. Aenderungsmaske, Aktivierungswirkung und redigierte Zusammenfassung bilden;
7. alle Anzahl- und Ressourcenobergrenzen pruefen;
8. einen unveraenderlichen, lebensdauerbesitzenden Preview-Handle bilden;
9. unmittelbar vor Installation unter dem Zustandsmutex `Operational`, die
   erfasste `ConfigurationServiceStateRevision` und die Modellreservierung
   erneut pruefen;
10. erst danach den sichtbaren Previewslot am eindeutigen
    Zustandslinearisierungspunkt ersetzen und die neue Vorschau sichtbar
    machen.

Ist der Kandidat exakt identisch, wird ein sichtbarer leichter `NoChange`-
Preview-Handle installiert. Er referenziert den aktiven unveraenderlichen
Snapshot statt ein zweites volles Konfigurationsmodell zu besitzen; die fuer
den Vergleich voruebergehend erworbene Modellreservierung wird vor der
Installation freigegeben. Bestaetigung erfasst diesen Handle, prueft Basis,
Identitaet, Modus und Zustandsrevision und liefert `NoChange` ohne Revision,
Manifestgeneration, `rootSequence` oder Storezugriff. Danach entfernt sie per
Identitaetsvergleich nur diesen `NoChange`-Handle. Abbruch oder Ersetzung tun
dasselbe; eine inzwischen neuere Vorschau bleibt stets unberuehrt.

### Bestaetigung unter exklusiver Mutation und parallele Ersetzung

Die Bestaetigung erfasst zuerst unter der Preview-Synchronisation den exakt zu
bestaetigenden unveraenderlichen Handle. Nach Erwerb der gemeinsamen
Mutationslease werden unmittelbar vor jedem Commit erneut geprueft:

- die Identitaet des erfassten Handles gegen den am Prueflinearisierungspunkt
  noch sichtbaren serviceeigenen Previewslot;
- `StorageEpoch` und exakte Active-Manifestbasis;
- der ausschliesslich aus dem erfassten Handle gelesene unveraenderliche
  Kandidat und seine neu berechnete Kandidatenintegritaet;
- vollstaendige technische und fachliche Validierung;
- unveraenderte Aktivierungswirkung;
- Ressourcen, Zaehler und gesamte Slotvorplanung.

Jede Abweichung liefert einen stabilen Konflikt ohne automatisches Merge,
ohne Last-write-wins und ohne Write. Bei zwei konkurrierenden Display-/Web-
Bestaetigungen gewinnt unter der exklusiven Mutation hoechstens eine; die
zweite sieht die neue Basis oder eine verworfene Vorschau und wird abgelehnt.

Beginnt eine begrenzte Vorpruefung der Previewanfrage B noch in `Operational`,
waehrend Preview A anschliessend zu `CommitInProgress` wechselt, darf B wegen
geaendertem Modus beziehungsweise geaenderter
`ConfigurationServiceStateRevision` weder eine volle Modellreservierung
erwerben noch installiert werden. A schreibt weiterhin ausschliesslich
Kandidat A aus seinem erfassten Handle. Erst nach erfolgreichem Publish,
Rueckkehr zu `Operational` und freiem Modellbudget darf Preview C gegen die
neue Active-Basis beginnen und installiert werden. Jede Bereinigung von A
vergleicht weiterhin die
Handleidentitaet und kann einen in einem anderen zulaessigen Interleaving
bereits neueren leichten Handle nicht loeschen. Es gibt keine ungeschuetzte
Data Race und keinen Zugriff auf einen vom letzten Besitzer bereits
freigegebenen Kandidaten.

## Entscheidung zur persistenten `MutationSequence`

### Entscheidung

Issue #56 fuehrt **keine separate persistente `MutationSequence`** ein.

Diese Entscheidung ist auf den Variante-B-R1-Scope von ADR-018 begrenzt. Der
vorhandene generische Typ `RecordSequence` bleibt unveraendert; es entsteht
weder ein neuer Mutationsrecord noch ein zusaetzlicher Slot oder Root.

### Gleichwertigkeitsnachweis

| Erforderliche Eigenschaft | Variante-B-Nachweis ohne separate Sequenz |
|---|---|
| Konflikt zwischen zwei Vorschauen | Exakte `StorageEpoch` und Active-Manifestreferenz werden unter derselben exklusiven Mutation erneut verglichen. Nur eine Bestaetigung kann dieselbe Basis erfolgreich verwenden. |
| Ordnung geaenderter Dokumentinhalte | Jeder geaenderte Dokumenttyp erhaelt eine Revision oberhalb des High-Water-Marks aller technisch gueltigen Records dieses Typs und dieser Epoche; unveraenderte Dokumente behalten ihre exakte Referenz. Verwaiste Writes koennen Luecken, aber keine Identitaetswiederverwendung erzeugen. |
| Ordnung vollstaendiger Graphkandidaten | Jedes neue Manifest erhaelt eine Generation oberhalb des High-Water-Marks aller technisch gueltigen Manifestrecords derselben Epoche. Ein verwaistes Manifest verbraucht seine fachliche Identitaet dauerhaft fuer andere Inhalte. |
| Persistente Gesamtordnung erfolgreicher Aktivierungen | Jeder neue Rootinhalt erhaelt eine `ConfigurationRootSequence` oberhalb des High-Water-Marks beider technisch gueltigen Rootrecords derselben Epoche; sie wird nicht nur aus dem aktuell verwendeten Graphen abgeleitet. In #56 existiert kein anderer persistenter Konfigurationscommit ausserhalb dieses Root-Linearisierungspunkts. |
| `NoChange` | Schreibt nichts und verbraucht keinen Zaehlerwert. Eine separate Mutationsnummer duerfte ebenfalls nicht fortgeschrieben werden und enthaelt daher keine Zusatzinformation. |
| Abbruch vor Root-Commit | Neue Dokumente oder ein Manifest bleiben unreferenziert und nicht kanonisch, ihre technisch gueltigen Revisionen beziehungsweise Generationen tragen aber zum naechsten High-Water-Mark bei. Eine separate Mutationsnummer waere ebenfalls nicht wirksam und verhindert die notwendige Inhaltsidentitaetsregel nicht. |
| Wiederholung nach eindeutig altem Ausgang | Der fuer den Versuch erfasste Preview-Handle wird identitaetsgebunden entfernt; eine inzwischen neuere Vorschau bleibt erhalten. Ein neuer Versuch benoetigt eine bestaetigbare Vorschau gegen die weiterhin kanonische Basis. Verwaiste Slots duerfen nach Referenzanalyse sicher ueberschrieben werden, aber ein anderer Inhalt erhaelt stets einen Wert oberhalb des beobachteten High-Water-Marks. |
| Eindeutig neuer Ausgang | Exakte Ziel-Rootreferenz, Ziel-`rootSequence`, Manifestgeneration und vollstaendiger Zielgraph bestimmen den neuen kanonischen Zustand. |
| `CommitOutcomeUnknown` | Beide Rootslots und die benoetigten Graphrecords werden vollstaendig gescannt. Die kanonische Rootordnung plus exakte Zielidentitaet ergibt eindeutig alt oder neu; bei fehlender Eindeutigkeit folgt `ConfigurationCommitIndeterminate`. |
| Neustart | Vorschauen existieren nicht mehr. Derselbe kanonische Root-/Graphscan bestimmt ausschliesslich aus persistenten Daten den Zustand; es gibt nichts wiederzugeben oder fortzusetzen. |
| Bootstrap und Reset | #57 besitzt dafuer `BootstrapSequence` und `StorageEpoch`; diese Vorgaenge sind keine zusaetzlichen #56-Graphcommits, die eine gemeinsame Mutationsnummer benoetigen. Sie teilen aber zwingend dieselbe fluechtige `ConfigurationMutationCoordinator`-Instanz mit #56. |
| Spaetere Secret-Domaenen | Sie erhalten erst mit ihrem ersten Konsumenten eigene epochengebundene Commitvertraege und duerfen keine jetzt leere Kreuzdomaenensequenz voraussetzen. |

Im #56-Scope besitzt damit jeder erfolgreiche persistente
Konfigurationscommit genau eine eindeutige neue `ConfigurationRootSequence`
oberhalb aller beobachteten technisch gueltigen Rootrecords. Verwaiste Writes
duerfen Luecken erzeugen; Eindeutigkeit verlangt keine lueckenlose Folge. Eine
weitere persistente Sequenz im Root waere dieselbe Ordnungsinformation. Ein
separater Sequenzrecord ausserhalb des Roots wuerde dagegen einen neuen Write,
einen weiteren unklaren Commitausgang und neue Recoveryordnung erzeugen. Er
koennte weder die Inhaltsidentitaet verwaister Dokument-/Manifestwrites noch
einen unklaren Rootwrite aufloesen, weil weiterhin alle betroffenen Slots,
der Root und sein Zielgraph gelesen werden muessen.

### Verbindliche Gegenbeispieltests

Der Nachweis gilt nur, wenn Tests mindestens beweisen:

- zwei Vorschauen auf gleicher Basis, genau eine Aktivierung;
- Wiederholung derselben bestaetigten Aktion nach eindeutig altem, eindeutig
  neuem und unbestimmtem Rootwrite;
- `NoChange` ohne jede Sequenzaenderung;
- Abbruch nach einem Dokumentwrite und danach ein anderer Kandidat: andere
  Payload erhaelt eine hoehere Dokumentrevision;
- Abbruch nach einem Manifestwrite und danach ein anderer Kandidat: andere
  Manifestpayload erhaelt eine hoehere Manifestgeneration;
- verwaiste hoehere Dokumentrevision und verwaiste hoehere
  Manifestgeneration werden in den jeweiligen High-Water-Mark aufgenommen;
- ein hoeherer technisch gueltiger, aber fachlich nicht nutzbarer Root wird in
  den Root-High-Water-Mark aufgenommen;
- `ReadError`, `CapacityError`, unbekanntes neueres Schema und High-Water-
  Ueberlauf blockieren vor jedem Write;
- unterschiedliche Inhalte erhalten in derselben Epoche niemals dieselbe
  Dokumentrevision, Manifestgeneration oder Rootsequenz;
- gleiche numerische Werte verschiedener starker Revisions-/Generationstypen
  werden nie vermischt;
- Root- und Manifestueberlauf werden vor dem ersten Write abgelehnt;
- ein zusaetzlicher persistenter Zaehler waere in keinem Orakel erforderlich.

Scheitert einer dieser Nachweise, ist das keine lokale Implementierungsdetails,
sondern eine materielle Planabweichung. Die Implementierung wird angehalten,
dieser Plan ergaenzt und erneut commitgebunden ownerfreigegeben.

## Commit- und Runtimeaktivierungsvertrag

### Commitvorbereitung

Unter der exklusiven Mutation erfolgt vor dem ersten Write:

1. Erwerb der gemeinsamen `ConfigurationMutationLease` und erneute Vorschau-,
   Basis-, Integritaets- und Vollvalidierungspruefung;
2. exakter `NoChange`-Entscheid;
3. vollstaendiger Identitaetskollisions- und High-Water-Scan aller
   erforderlichen Dokument-, Manifest- und Rootslots mit Sperre bei
   unterschiedlichem Inhalt unter gleicher Identitaet, unbekanntem Ergebnis
   oder neuerem unbekanntem Schema;
4. checked `HighWaterMark + 1` fuer jede benoetigte Dokumentrevision, die
   Manifestgeneration und die Rootsequenz;
5. vollstaendige Schutzmengen- und Slotvorplanung;
6. Groessenpruefung jedes benoetigten Payloads und Envelopes;
7. Vorbereitung aller fachlichen und Plattformwerte;
8. Aufbau eines unveraenderlichen `RuntimeConfigurationSnapshot`;
9. vollstaendige Ressourcenreservierung fuer den Snapshot-Publish,
   einschliesslich der nach Publish benoetigten Modellgeneration, aller
   Reader-/Besitzzaehler und eines leeren Besitzers fuer den beim Austausch
   uebernommenen alten Publisher-Handle; ein ausgeschoepftes oder unbekanntes
   Lebensdauerbudget blockiert vor dem ersten Write;
10. Aufbau und Validierung der kuenftigen Manifest- und Rootmodelle;
11. Nachweis aller Publisher-Praekonditionen, sodass nach bestaetigtem
    Root-Commit kein normaler fachlicher Fehlerzweig mehr verbleibt.

Kein Kandidat wird vor vollstaendiger erfolgreicher Vorbereitung sichtbar
oder persistent wirksam.

### Persistente Schreibreihenfolge

1. fuer jedes tatsaechlich geaenderte Dokument in stabiler Reihenfolge
   UserConfiguration, ServiceConfiguration, ProgramCatalog:
   - kanonische Payload in einen leeren, wiederverwendeten Recordarbeitsbereich
     kodieren;
   - Envelope bilden;
   - Zielslot schreiben;
   - bei `CommitOutcomeUnknown` genau diesen Slot vollstaendig ruecklesen und
     als eindeutig alt oder exakt neu bestimmen;
   - neue Bytes ruecklesen, Referenzbindung pruefen und fachlich validieren;
   - Recordarbeitsbereich vor dem naechsten vollstaendigen Record leeren;
2. Manifest auf dieselbe Weise schreiben und den gesamten neuen Zielgraphen
   ohne Root als vollstaendig nutzbar pruefen;
3. Rootpayload und -envelope in einem leeren Arbeitsbereich fertig kodieren
   und lokal validieren;
4. Root in den anderen Rootslot schreiben.

Bei Dokument- oder Manifestwrites mit eindeutig altem oder nicht wirksamem
Ausgang bleibt der kanonische Root unveraendert. Der Commit endet typisiert;
verwaiste neue Records sind nicht aktiv und spaeter nach Schutzanalyse
wiederverwendbar. Kann ein nichtlinearisierender Write nicht rueckgelesen
werden, bleibt die alte Runtime kanonisch; es erfolgt kein Rootwrite.

### Root-Commit als Linearisierungspunkt

Ein neuer Root wird nur geschrieben, nachdem Zielgraph, Snapshot und alle
normalen falliblen Ressourcenarbeiten erfolgreich vorbereitet sind. Der
persistente Linearisierungspunkt ist:

- `StateStoreWriteStatus::Success` fuer die exakt vorbereiteten Rootbytes,
  gefolgt von verpflichtendem Readback; oder
- bei `CommitOutcomeUnknown` ein vollstaendiger Aufloesungsscan, der den exakt
  erwarteten neuen Root und dessen vollstaendigen Zielgraphen eindeutig als
  kanonisch bestimmt.

Der verpflichtende Readback beziehungsweise Aufloesungsscan ist kein
nachgelagerter fachlicher Vorbereitungsschritt, sondern die Bestimmung des
tatsaechlichen persistenten Commitresultats. Ein Fehler dabei wird niemals
als normaler Fehler vor dem Root-Commit umgedeutet.

Ergibt der Unknown-Scan eindeutig den alten kanonischen Graphen, wird nichts
publiziert, die alte Runtime bleibt verfuegbar und das Ergebnis ist ein
typisierter Persistenzfehler. Ergibt er eindeutig den neuen Graphen, folgt der
nicht fehlschlagende Publish. Ist keine eindeutige Bestimmung moeglich, gilt
der unten definierte unbestimmte Zustand.

Ein nach `Success` nicht abschliessbarer verpflichtender Root-/Graph-Readback
ist ein `ConfigurationRuntimeFailure`: der Store hat den neuen Root laut
Portvertrag dauerhaft angenommen, aber der #56-Gesamtvertrag kann nicht
vollstaendig bestaetigt werden. Es erfolgen weder Publish noch Rollback;
normale Runtimefreigabe und weitere Mutationen bleiben gesperrt.

### Nicht fehlschlagender Publish

Der vorbereitete Snapshot wird ueber einen kleinen app-internen Publisher
sichtbar gemacht:

- der neue unveraenderliche Snapshot und sein Besitz-/Controlblock entstehen
  vollstaendig vor dem Rootwrite;
- der Publisher tauscht nach bestaetigtem Root-Commit unter der bereits vor
  Root erworbenen serviceweiten Zustandslease nur den vorbereiteten internen
  `shared_ptr` aus und uebergibt den dabei entfernten alten Publisher-Handle
  an den bereits vorbereiteten leeren Besitzer;
- `publishPrepared(...)` ist `noexcept` und allokiert, serialisiert, validiert,
  reserviert, protokolliert und liest keinen Store;
- Leser erhalten ausschliesslich eine begrenzte, nicht kopierbare
  `RuntimeConfigurationReadLease` und beobachten nur die vollstaendig alte
  oder vollstaendig neue Generation;
- Leser, die eine alte Lease bereits besitzen, koennen den alten Snapshot
  unveraendert bis zur Lease-Freigabe weiterverwenden; es koennen weder weitere
  freie Handles kopiert noch mehr als acht Leases gleichzeitig ausgegeben
  werden;
- der alte Publisher-Handle wird im kritischen Post-Root-Commit-Schritt weder
  zerstoert noch freigegeben. Nach abgeschlossenem Austausch und Verlassen des
  kritischen Zustandsabschnitts wird der uebernommene Publisher-Handle in einem
  verpflichtenden nicht kritischen Retirement-Schritt noch vor Rueckkehr der
  Commitoperation freigegeben. Seine potenziell umfangreiche Zerstoerung liegt
  damit ausserhalb des Publish-Linearisierungspunkts; vorhandene Read-Leases
  halten dieselbe alte Generation weiterhin sicher;
- das Freigeben des uebernommenen Handles ausserhalb des kritischen Schritts
  darf keinen fachlichen Fehlerpfad erzeugen und zerstoert den alten Snapshot
  erst, wenn auch kein Leserhandle mehr besteht.

Der kritische Publish nach bestaetigtem Root-Commit besteht damit
ausschliesslich aus atomarem Austausch und nicht allokierenden Besitzmoves
beziehungsweise `swap`-Operationen. Danach erfolgen in diesem kritischen Pfad
keine Allokation, Freigabe des alten Snapshots, Serialisierung, Validierung,
Storeoperation, fachliche Entscheidung oder recoverbare Fehlerbehandlung.
Die gemeinsame Mutationslease bleibt bis zum abgeschlossenen Austausch und der
stabilen Uebernahme des alten Publisher-Handles gehalten.

Die C++17-Toolchainunterstuetzung von internem `shared_ptr`-Besitz,
nichtkopierbarer Read-Lease und vor Root erworbener Zustandslease wird in
`native`, `esp32_bringup` und `esp32_release` gebaut und getestet. Eine andere
Publishertechnik, die das oeffentliche Lebensdauer-, Budget- oder
Nichtfehlschlagenversprechen veraendert, waere eine materielle Planabweichung.

Alle in Produktion pruefbaren Publisher-Praekonditionen werden vor dem
Rootwrite ausgewertet. `publishPrepared(...)` besitzt danach keinen normalen
Fehlerrueckgabepfad. Eine absichtlich injizierte interne Vertragsverletzung
wird in Tests weiterhin als `ConfigurationRuntimeFailure` und fail closed
nachgewiesen; sie ist kein fachlich recoverbarer Publishzweig, fuehrt weder
Rollback noch Factory-Fallback ein und darf den Produktionsaustausch nicht um
eine fallible Nacharbeit erweitern.

## Vollstaendige Aufloesungs- und Recoveryzustandsmaschine

### `ConfigurationCommitIndeterminate`

Der endgueltige Produktions-Typname bleibt:

```text
ConfigurationCommitIndeterminate
```

Der Typ ist ein eigener stabiler fachlicher Zustand und weder ein Alias fuer
`PersistenceFailure` noch ein boolesches Flag. Beim Eintritt legt der Dienst
vor Freigabe der Mutationslease einen unveraenderlichen rein fluechtigen
`ConfigurationCommitResolutionContext` an. Er bindet mindestens:

- die `StorageEpoch`;
- die exakte alte kanonische Rootidentitaet, den vollstaendigen alten
  Graphfingerprint und den intern gehaltenen alten Runtime-Snapshot;
- den erwarteten neuen Rootslot, die exakten vorbereiteten kanonischen
  Rootbytes sowie die vollstaendige erwartete Manifest-/Dokumentgraphidentitaet;
- den bereits vorbereiteten neuen `RuntimeConfigurationSnapshot` samt einer
  `RuntimePreparationBinding` aus Zielgraphfingerprint, Plattformwerten und
  deren Gueltigkeitsidentitaet;
- versuchte `ConfigurationRootSequence` und
  `ConfigurationManifestGeneration`;
- `ConfigurationCommitIndeterminateCause`.

Die Ursache unterscheidet mindestens Root-/Graph-`ReadError`,
`CapacityError`, Envelope-/CRC-/Referenzintegritaet, fachliche
Graphungueltigkeit und nicht eindeutige Aufloesung. Oeffentliche Diagnose
enthaelt nur redigierte Sequenz-, Phasen- und Ursachenangaben. Der interne
Kontext enthaelt Konfigurationsgraphen, aber keine Connectivity-,
Authentication- oder sonstigen Secrets.

Der Zustand entsteht ausschliesslich, wenn der Rootwrite
`CommitOutcomeUnknown` liefert und der vollstaendige Scan beider Rootslots
sowie aller fuer alten und erwarteten neuen Graphen benoetigten Records nicht
eindeutig als alt oder neu abgeschlossen werden kann. Am serviceweiten
fail-closed Linearisierungspunkt werden Modus und Zustandsrevision gesetzt,
der gesamte sichtbare Previewslot geleert und neue normale Preview-, Runtime-
und Mutationsfreigaben gesperrt. Der vorbereitete neue Snapshot wird nicht
publiziert. Bereits ausgegebene Read-Leases bleiben nur speichersicher.

Eine explizite in-process Aufloesung darf ausschliesslich die gemeinsame
Mutationslease erwerben und im Modus `CommitIndeterminate` den gebundenen
Kontext verwenden. Sie ist keine normale Konfigurationsmutation, schreibt
nichts und gibt keine Slots frei.

#### Vollscan ergibt eindeutig alt

- Nur der exakt im Kontext gebundene alte Root-/Graphzustand wird akzeptiert.
- Der bereits gebundene alte Runtime-Snapshot wird ohne neuen Publish wieder
  als lokal normal akquirierbare Runtime gesetzt.
- Der neue Snapshot wird unter der Zustandslease in einen vorbereiteten
  Retirement-Besitzer verschoben und ausserhalb des kritischen
  Zustandsabschnitts vor Rueckkehr der Aufloesungsoperation verworfen;
  verwaiste neue Dokument- und Manifestrecords bleiben fuer alle spaeteren
  High-Water-Marks sichtbar.
- Der Previewslot bleibt leer. Modus und Zustandsrevision wechseln unter der
  gehaltenen Zustandslease zu `Operational`.
- Es erfolgt kein Rollbackwrite und keine Slotwiederverwendung im
  Aufloesungsschritt.

#### Vollscan ergibt eindeutig neu

- Nur der exakt erwartete neue Rootslot und die im Kontext gebundene exakte
  Root-/Graphidentitaet werden akzeptiert. Ein anderer gueltiger neuer Graph
  ist kein Aufloesungserfolg.
- Der vorbereitete Snapshot darf nur publiziert werden, wenn
  `RuntimePreparationBinding`, Graphfingerprint und alle vorbereiteten
  Plattformwerte weiterhin exakt gueltig sind.
- Ist diese Bindung nicht mehr verwendbar, bleibt der Dienst fail closed und
  verschiebt den unbrauchbaren vorbereiteten neuen Snapshot unter der
  Zustandslease in einen vorbereiteten Retirement-Besitzer. Er wird bei
  weiterhin `CommitIndeterminate` ausserhalb des kritischen Zustandsabschnitts
  freigegeben. Danach verwendet die Neuerstellung genau dieselbe dadurch
  freigewordene zweite Modellposition und bereitet den Runtime-Snapshot
  vollstaendig neu und fallibel aus dem exakt verifizierten neuen Graphen vor.
  Vor Publish werden Root-/Graphidentitaet, Modellbudget und Plattformwerte
  unter Mutations- und Zustandslease erneut geprueft. Eine dritte
  Vollmodellgeneration entsteht auch waehrend der Aufloesung nicht.
- Scheitert die Neuerstellung oder erneute Bindungspruefung, wechselt der
  Dienst zu `RuntimeFailure` mit Ursache
  `RuntimePreparationAfterResolutionFailure`.
- Bei Erfolg wird ausschliesslich der exakt gebundene neue Snapshot
  nicht fehlschlagend publiziert; Preview bleibt leer. Es gibt keinen
  automatischen Rollback oder Factory-Fallback.

#### Vollscan bleibt unklar oder schlaegt fehl

- Modus und Aufloesungskontext bleiben `CommitIndeterminate` unveraendert.
- Es gibt keine normale Runtime-, Preview-, Mutations- oder Slotfreigabe.
- Wiederholte explizite Scans duerfen spaeter erneut versuchen, aber weder
  Records schreiben noch den erwarteten alten/neuen Kontext umdeuten.

Alle drei Ergebnisse sind stabil typisiert als
`ResolutionRecoveredOld`, `ResolutionRecoveredNew`,
`ResolutionStillIndeterminate` oder `ResolutionRuntimeFailure`.

### Neustartgrenze zu #57

Nach Neustart existieren weder Preview, Prepared-Snapshot noch der fluechtige
Aufloesungskontext. #57 rekonstruiert ausschliesslich aus persistenten Daten
ueber denselben vollstaendigen Root-, Graph-, High-Water- und
Identitaetskollisionsscan. #56 stellt dafuer die Loader- und Producervertraege
bereit, implementiert aber keinen #57-Bootstrap oder Recoveryablauf. Ein
Neustart hebt die spaetere persistente #24-Verriegelung nicht auf und setzt
keinen Fehler automatisch zurueck.

## Weitere Fehler- und Ergebnisvertraege

### Commitergebnisse

Der Dienst bildet Ergebnisse auf stabile projektspezifische Kategorien ab:

- `ConfigurationCommitSuccess`
  - `NoChange`
  - `Activated`
- `ConfigurationValidationFailure`
- `ConfigurationConflictFailure`
- `PersistenceFailure`
- `ActivationFailure`
- `MigrationFailure`
- `ConfigurationCommitIndeterminate` als eigener Zustand, nicht als
  Unterfall eines normalen Fehlers vor Commit.

Store-, Codec- oder Bibliotheksdetails werden nicht ungefiltert an UI- oder
Fachkonsumenten durchgereicht. Die Fehler tragen hoechstens Recordart, Phase
und redigierte stabile Ursache.

### `ConfigurationRuntimeFailure`

`ConfigurationRuntimeFailure` ist der zweite stabile #56-Producer und deckt
mindestens ab:

- bestaetigter Root-Commit, dessen verpflichtender Nachweis nicht
  abgeschlossen werden kann;
- unerwartete Verletzung des vertraglich nicht fehlschlagenden
  Snapshot-Publish;
- fehlgeschlagene Runtime-Neuvorbereitung nach eindeutig neuer
  Indeterminate-Aufloesung;
- nicht eindeutig lesbarer oder integrer persistenter Graph bei einem
  `ValidationOnly`-Scan eines zuvor operationalen Dienstes;
- Servicezustands- oder Modellbudget-Invariantenverletzung sowie persistente
  Identitaetskollision. Eine normale typisierte Budgetauslastung ist dagegen
  `ConfigurationModelBudgetBusy` und kein Runtimefehler.

In diesem Zustand wird kein Rollback versucht, keine normale Runtime mehr
freigegeben, keine neue Read-Lease ausgegeben, kein Preview installiert und
keine normale Mutation erlaubt. Der sichtbare Previewslot ist am
fail-closed Linearisierungspunkt vollstaendig geleert; spaeter fertig werdende
Previewberechnungen scheitern an Modus und Zustandsrevision. Der Zustand ist von
`ConfigurationCommitIndeterminate` getrennt: Beim Runtimefehler ist der
Rootwrite laut Storevertrag beziehungsweise erfolgreichem Scan bestimmt; beim
unbestimmten Zustand ist gerade diese persistente Bestimmung nicht moeglich.

Die Aufloesungsregel ist ursachenspezifisch:

Fuer die beiden in-process aufloesbaren Ursachen haelt der Dienst einen
unveraenderlichen rein fluechtigen
`ConfigurationRuntimeFailureResolutionContext`. Er bindet Epoche, exakte
erwartete persistente Root-/Graphidentitaet, Ursache und eine noch gueltige
Runtimevorbereitungsbindung beziehungsweise die eindeutige Regel zur
vollstaendigen Neuerstellung. Der Kontext enthaelt keine Secrets und wird bei
einem Scan nicht auf einen anderen Graphen umgebogen. Nicht in-process
aufloesbare Ursachen halten nur die fuer eine redigierte Diagnose benoetigten
stabilen Angaben; Neustart/#57 rekonstruiert ausschliesslich aus persistenten
Daten.

| `ConfigurationRuntimeFailureCause` | In-process erlaubt | Verbindliche Aufloesung |
|---|---|---|
| `PersistentGraphVerificationFailure` | expliziter vollstaendiger Verifikationsscan | Nur die weiterhin exakt erwartete Active-Root-/Graphidentitaet plus gueltige lokale Runtimebindung duerfen zu `Operational` fuehren; ein anderer Graph verlangt Neustart/#57. |
| `PostCommitVerificationFailure` | expliziter vollstaendiger Verifikationsscan | Nur exakt erwarteter persistenter Graph plus erfolgreiche vollstaendige Runtimevorbereitung duerfen zu `Operational` fuehren. |
| `RuntimePreparationAfterResolutionFailure` | expliziter erneuter Vollscan und genau ein neuer begrenzter Vorbereitungsversuch | Erfolg nur bei weiterhin exakt gebundenem neuen Graphen; sonst `RuntimeFailure`. |
| `PublishContractViolation` | nein | Neustart und #57-Rekonstruktion; kein in-process Wiederpublish. |
| `ServiceStateInvariantViolation` oder `ConfigurationModelBudgetInvariantViolation` | nein | Neustart und #57-Rekonstruktion; interne Invariante wird nicht lokal zurueckgesetzt. |
| `PersistentConfigurationIdentityCollision` | nein | #57-/Recoverypfad bleibt fail closed, bis die persistente Integritaet durch einen ausdruecklich freigegebenen Recoveryvertrag wiederhergestellt ist. |

Auch ein erlaubter in-process Verifikationsscan erwirbt die gemeinsame
Mutationslease, schreibt keine Records und haelt Preview, normale
Snapshotakquisition sowie Slotwiederverwendung bis zur vollstaendigen
Aufloesung gesperrt. Bei einem erlaubten Erfolg werden exakte Graphbindung,
vollstaendige fallible Runtimevorbereitung, Modellbudget und nicht
fehlschlagender Publish abgeschlossen, bevor Modus und Zustandsrevision unter
derselben Zustandslease wieder `Operational` werden; der Previewslot bleibt
leer. Jeder Fehlschlag bleibt `RuntimeFailure`. Bereits gehaltene Read-Leases
bleiben speichersicher, sind aber keine normale Runtimefreigabe. Die spaetere
#24-Verriegelung wird durch keinen dieser lokalen Erfolge automatisch
geloescht; deren Reset bleibt ausschliesslich beim #24-Fehlerresetvertrag.

## Grenze zum `CONFIGURATION_SAFETY_INTEGRATION_GATE`

#56 implementiert ausschliesslich Producer-Vertraege:

- `ConfigurationRuntimeFailure`;
- `ConfigurationCommitIndeterminate`;
- fail-closed Zustand und keine normale Runtimefreigabe.

#56 implementiert nicht:

- eine Safetyfehlerklasse;
- eine persistente Verriegelung;
- `SAFE_BOOT`-Prioritaet;
- Fehlerreset;
- Aktorsperre oder Aktorfreigabe;
- GPIO- oder Hardwaretests.

#24 konsumiert spaeter diese realen Typen zusammen mit
`ConfigurationUnavailable` und `ConfigurationIntegrityFailure` aus #57. Das
nachgelagerte `CONFIGURATION_SAFETY_INTEGRATION_GATE` muss mindestens
nachweisen:

- `ConfigurationRuntimeFailure` fuehrt zur persistenten Verriegelung;
- `ConfigurationCommitIndeterminate` fuehrt zur persistenten Verriegelung;
- Bootprioritaet beziehungsweise `SAFE_BOOT` bleibt sicher;
- keine normale Aktorfreigabe entsteht;
- Neustart umgeht die notwendige Verriegelung nicht;
- Recovery hebt sie nur gemaess #24-Fehlerresetvertrag auf;
- alle Producer sind reproduzierbar injizierbar.

#56 darf seine Producer-Vertraege unabhaengig abschliessen. #24 darf jedoch
nicht als vollstaendig abgeschlossen gelten, bevor dieses Gate mit den realen
#56-/57-Typen bestanden ist. Es entsteht keine zyklische oder pauschale
Abhaengigkeit von #56 auf #24.

## Ressourcen- und Lebensdauervertrag

### Verbindliche Softwareobergrenzen

- genau eine sichtbare fluechtige Vorschau global;
- genau eine volle Previewerstellung gleichzeitig;
- genau eine Konfigurationsmutation gleichzeitig;
- hoechstens zwei unterschiedliche vollstaendige
  ProgramCatalog-/Konfigurationsmodellgenerationen gleichzeitig;
- hoechstens acht gleichzeitig registrierte
  `RuntimeConfigurationReadLease`-Objekte;
- vier Slots je Dokumenttyp, drei Manifest- und zwei Rootslots;
- maximal acht technische Kandidaten je vorhandener
  `device_platform`-Scanoperation;
- maximal 256 Byte UserConfiguration-Payload;
- exakt 0 Byte ServiceConfiguration-Payload;
- maximal 32.768 Byte ProgramCatalog-Payload;
- exakt 104 Byte Manifestpayload;
- maximal 69 Byte Rootpayload;
- maximal 32.813 Byte fuer einen Dokumentrecord inklusive UTC-Envelope;
- maximal 149 Byte fuer ein Manifest-Envelope;
- maximal 114 Byte fuer ein Root-Envelope;
- im normalen Load-/Encode-/Commitworkflow hoechstens ein vollstaendiger
  Recordarbeitsbereich; ausschliesslich der bytegenaue Vergleich zweier Records
  mit gleicher persistenter Identitaet darf genau zwei begrenzte
  Recordbytepuffer gleichzeitig halten;
- genau eine gemeinsame Mutationslease fuer alle persistenten
  Konfigurationsmutationen;
- hoechstens ein vom Publisher uebernommener alter Snapshot-Handle zur
  verzoegerten Freigabe ausserhalb des kritischen Publish-Schritts.

Der Workflow ruft `encodeEnvelope()` nur mit einem leeren oder nicht mehr
benoetigten Ausgabepuffer auf. Nach jedem Write und Readback wird dieser
Arbeitsbereich geleert beziehungsweise wiederverwendet, bevor der naechste
vollstaendige Record kodiert wird. Ein alter Ausgaberecord bleibt nie parallel
zum neuen Encoderpuffer erhalten. Die einzige Ausnahme sind die zwei
read-only Recordbytepuffer eines Identitaetskollisionsvergleichs; in diesem
Moment gibt es weder vollen Encoderpuffer noch tief dekodiertes zusaetzliches
Konfigurationsmodell. Der Peakbericht weist diese maximal zwei Recordpuffer
separat von den maximal zwei Modellgenerationen aus.

Der aktive Snapshot und genau eine weitere reservierte Modellgeneration teilen
unveraenderte Dokumente, wo ihre Inhalte identisch sind. Ein sichtbarer und
vom Commit erfasster Handle auf denselben Previewkandidaten zaehlen als eine
Generation, nicht als Kopie. Nach Publish zaehlen der uebernommene alte
Publisher-Handle und alle registrierten alten Read-Leases ebenfalls als eine
alte Generation. Solange sie lebt, ist die zweite Modellposition belegt und
jeder weitere unterschiedliche Kandidatenaufbau wird vor tiefer Kopie und vor
jedem Write mit `ConfigurationModelBudgetBusy` abgelehnt. Es gibt weder eine
dritte Katalogkopie noch eine dritte alte oder neue Modellgeneration. Die
verzoegerte Freigabe, feste Readerzahl und Modellreservierung werden im
Base-/Head- sowie Peak-Allokationsnachweis explizit gemessen.

### Messpflichtige Werte

Folgende Werte bleiben bis zum Implementierungsnachweis
`TBD_IMPLEMENTATION_BUDGET` beziehungsweise `MEASUREMENT_REQUIRED`:

- absolute Host- und ESP32-Heapspitze;
- niedrigster freier Heap und groesster freier Block auf realem ESP32;
- tatsaechliche NVS-Belegung und Replace-Atomizitaet;
- reale Commitdauer, Jitter und Flashlebensdauer;
- endgueltige Flash- und statische RAM-Reserve.

Die Softwaregrenzen duerfen nicht als reale Hardwaregarantie formuliert
werden.

### Base-/Head-Vergleich

Nach Planfreigabe und vor der ersten Produktionsaenderung wird der
freigegebene Plan-Commit als Code-Baseline gebaut. Nach der Implementierung wird
der Head mit exakt derselben Toolchain und denselben Umgebungsvariablen gebaut.

Fuer beide Staende werden mit `scripts/build_report.py` mindestens erfasst:

- Host-Testbinaer fuer `native`;
- statisches RAM und Flash fuer `esp32_bringup`;
- statisches RAM und Flash fuer `esp32_release`;
- Groesse von `firmware.bin`;
- Groesse von `firmware.elf`.

Der Abschlussbericht nennt Base-SHA, Head-SHA, absolute Werte und Deltas. Die
Berichte werden als CI-/PR-Nachweis verwendet und nicht als neue dauerhaft
versionierte Buildartefakte eingecheckt. Unterschiede gelten informativ, bis
reale Budgets ownerfreigegeben sind; unerwartete oder nicht erklaerbare
Spruenge blockieren den Abschluss.

## Geplanter kleiner Commit-Schnitt nach Planfreigabe

Alle Commits bleiben im selben Draft-PR. Jeder Schritt muss fuer sich bauen
und die bis dahin vorhandenen Tests bestehen.

### Commit 1 – Manifest-, Root- und Wirevertraege

- Record-IDs, Keys und Limits ergaenzen;
- starke Referenz-, Manifest- und Rootmodelle einfuehren;
- kanonische Schema-1-Codecs implementieren;
- Golden-, Roundtrip-, Grenz- und Negativtests ergaenzen.

Keine Storeorchestrierung und keine Vorschau in diesem Commit.

### Commit 2 – Gemeinsame Mutationskoordination, Graphladen und Schutzmenge

- konkreten `ConfigurationMutationCoordinator` mit nicht blockierender,
  verschiebbarer RAII-Lease und isolierten Koordinationstests einfuehren;
- vollstaendigen Graphloader implementieren;
- Rootslot-`ReadError`/`CapacityError` vor jeder kanonischen Auswahl fail
  closed priorisieren;
- kanonische Root-/Active-/Fallback-Auswahl und Diagnosen implementieren;
- High-Water-Scans fuer Dokumentrevisionen, Manifestgeneration und Rootsequenz
  einschliesslich unbekannter neuerer Schemas und Ueberlauf implementieren;
- Identitaetskollisionsscan fuer alle Dokument-, Manifest- und Rootslots
  implementieren; byteidentische Duplikate diagnostizieren und verschiedene
  Inhalte unter derselben Identitaet fail closed sperren;
- initialen Graphaufbau und modellfreien `ValidationOnly`-Scan fuer Mutationen
  trennen; fuer exakte Kollisionsvergleiche hoechstens zwei begrenzte
  Recordbytepuffer verwenden;
- Root-Gleichstand nur bei identischen kanonischen Bytes deterministisch
  aufloesen, unterschiedliche Bytes als Integritaetsfehler sperren;
- exakte Referenzbindung und fachliche Dokumentvalidierung integrieren;
- Schutzmenge und deterministische Slotvorplanung implementieren;
- Graph-, Korruptions- und Rotationsmodelltests ergaenzen.

Noch kein persistenter Aktivierungsworkflow.

### Commit 3 – Runtime-Snapshot und fluechtige Vorschau

- unveraenderlichen `RuntimeConfigurationSnapshot`, begrenzte nicht kopierbare
  `RuntimeConfigurationReadLease` und den internen Publisher implementieren;
- serviceweiten Zustandsblock mit `Operational`, `CommitInProgress`,
  `CommitIndeterminate`, `RuntimeFailure` und checked
  `ConfigurationServiceStateRevision` implementieren;
- exakt begrenzte Modellreservierung fuer hoechstens zwei Generationen, acht
  Read-Leases und eine volle Previewerstellung implementieren;
- `ConfigurationPreviewBuildLease` als zwingenden Besitzvertrag vor jedem
  Vollkandidatenaufbau implementieren;
- Uebernahme des alten Publisher-Handles ohne Freigabe im kritischen
  Post-Root-Schritt sowie verzoegerte Zerstoerung ausserhalb dieses Schritts
  implementieren;
- genau eine serviceeigene Vorschau, Kandidatenintegritaet, exakte
  Inhaltsvergleiche, `NoChange` und Konfliktvertrag implementieren;
- rein fluechtige unveraenderliche Preview-Handles, zustandslinearisiertes
  Installieren und identitaetsgebundene Bereinigung implementieren;
- Preview-, Lebensdauer-, Konkurrenz- und Allokationstests ergaenzen.

Noch kein Rootwrite aus dem Konfigurationsdienst.

### Commit 4 – Serialisierter Commit und fail-closed Zustaende

- `ConfigurationService` an die gemeinsame Koordinatorreferenz binden und jede
  interne zweite Mutationssperre ausschliessen;
- vollstaendige Vorplanung und geordnete Dokument-/Manifestwrites;
- Root-Commit als Linearisierungspunkt;
- vollstaendigen fluechtigen Unknown-Aufloesungskontext und die typisierten
  Uebergaenge eindeutig alt, eindeutig neu, weiterhin unklar und
  Runtimevorbereitungsfehler;
- `ConfigurationCommitIndeterminate` und `ConfigurationRuntimeFailure`;
- nicht fehlschlagenden Snapshot-Publish;
- serviceweiten atomaren fail-closed Uebergang mit Previewleerung und
  Akquisitionssperre;
- ursachenspezifische in-process beziehungsweise Neustart-Aufloesung fuer
  `ConfigurationRuntimeFailure`;
- vollstaendige Cut-Point-, Wiederholungs-, Aufloesungs- und Publishmatrix.

### Commit 5 – Ressourcen, Gesamtnachweis und Dokumentation

- Maximalmodell-, Readerlimit- und Peak-Allokationstests mit Nachweis der
  Obergrenzen zwei Modellgenerationen, acht Read-Leases und eine volle
  Previewerstellung;
- fuenf aufeinanderfolgende Active-Commits;
- alle drei Buildprofile und Base-/Head-Ressourcenvergleich;
- Spezifikation, Implementierungsuebersicht und Changelog aktualisieren;
- Diff gegen diesen freigegebenen Plan dokumentieren.

Falls der tatsaechliche Dateischnitt ausserhalb der oben genannten Dateien
waechst oder ein Commit eine neue Modul-/Vertragsentscheidung benoetigt, gilt
dies als materielle Planabweichung und erfordert vor der Aenderung einen neuen
Plan-Commit mit erneuter Ownerfreigabe.

## Vollstaendige Teststrategie

### Gemeinsame Mutationskoordination

- erste `tryAcquire()`-Operation liefert genau eine gueltige, nicht kopierbare
  Lease;
- ein zweiter Erwerb waehrend der gehaltenen Lease liefert
  `ConfigurationMutationBusy` ohne Blockieren und ohne Storezugriff;
- verschobene Lease gibt die Exklusivitaet genau einmal frei;
- nach Freigabe kann die naechste Mutation dieselbe Koordinatorinstanz
  erwerben;
- zwei getrennte `ConfigurationService`-aehnliche Testkonsumenten sowie ein
  #57-aehnlicher Mockkonsument teilen dieselbe Instanz und koennen nie
  gleichzeitig mutieren;
- normale Aktivierung, Migration und simulierte Bootstrap-/Resetoperation
  verwenden den gleichen Lease-Vertrag, ohne Bootstrap- oder Resetlogik in
  #56 zu implementieren;
- Repositorysuche weist nach, dass `ConfigurationService` und Graphstore keinen
  zweiten unabhaengigen Mutationsmutex besitzen.

### Wire- und Codec-Tests

- Golden Bytes fuer Manifest mit allen bekannten Origin-/Operation-IDs;
- unbekannte Origin-/Operation-ID bleibt mit Rohwert erhalten;
- Root ohne Fallback und Root mit Fallback;
- exakte Payloadlaengen 104, 35 und 69 Byte;
- Envelopegrenzen 149 und 114 Byte;
- Roundtrip fuer Minimal-/Maximalwerte;
- Nullwerte fuer Epoche, Revision, Generation und Sequenz;
- ungueltige Slot-ID je Recordgruppe;
- falscher Recordtyp, falsches Schema, falsche Epoche;
- Truncation an jeder Feldgrenze und Trailing Bytes;
- ungueltiger Optionaltag;
- Active und Fallback identisch;
- Payload-CRC-Abweichung bei gueltiger Envelope-CRC;
- Envelope-CRC-Abweichung;
- unbekannte neuere Manifest-/Rootschemas ohne Teilwirkung;
- Encoder laesst Ausgabepuffer bei jedem Fehler byteidentisch unveraendert.

### Kanonische Graph- und Korruptionstests

- gueltiger Active-Graph ohne Fallback;
- gueltiger Active-Graph mit gueltigem Fallback;
- ungueltiges Active mit gueltigem Fallback;
- ungueltiges Active und fehlender beziehungsweise ungueltiger Fallback;
- hoeherer unbrauchbarer Root, danach niedrigerer vollstaendig nutzbarer Root;
- vollstaendig gelesener, aber technisch oder fachlich ungueltiger hoeherer
  Root darf zugunsten eines aelteren vollstaendig gueltigen Roots
  uebersprungen werden;
- `NotFound` in einem Rootslot und ein vollstaendig gueltiger anderer Root
  erlauben dessen normale Auswahl;
- ein Rootslot nicht lesbar (`ReadError`) und der andere aeltere Root
  vollstaendig gueltig: `ConfigurationGraphLoadFailure`, keine
  Runtimefreigabe;
- ein Rootslot mit `CapacityError` und der andere aeltere Root vollstaendig
  gueltig: `ConfigurationGraphLoadFailure`, keine Runtimefreigabe;
- gleicher Sequenzwert und exakt identische kanonische Rootbytes in beiden
  Rootslots: deterministische Auswahl mit sichtbarer Diagnose;
- gleicher Sequenzwert, aber unterschiedliche Rootpayload, Referenzen oder
  kanonische Bytes: `ConfigurationGraphIntegrityFailure`, keine
  Runtimefreigabe und kein Slot-Tiebreak;
- hohes `rootSequence` ohne gueltigen Graph aktiviert nichts;
- jede Referenzkante einzeln mit falschem Typ, Slot, Revision/Generation,
  Schema, Laenge, Payload-CRC und Epoche;
- technisch gueltige, CRC-korrekte aber fachlich ungueltige UserConfiguration;
- technisch gueltiger, CRC-korrekter aber fachlich ungueltiger ProgramCatalog;
- `NotFound`, `ReadError` und `CapacityError` bleiben unterscheidbar;
- Fallbacknutzung und unbrauchbarer Fallback sind diagnostizierbar;
- verworfene Kandidaten halten keine grossen Payloadsammlungen.

### Persistente Identitaetskollisionstests

- zwei UserConfiguration-Slots derselben Epoche mit gleicher Revision und
  unterschiedlichen kanonischen Recordbytes;
- dieselbe Kollision fuer ServiceConfiguration und ProgramCatalog;
- zwei Manifest-Slots derselben Epoche mit gleicher Generation und
  unterschiedlichen kanonischen Manifestbytes;
- gleiche Identitaet und exakt byteidentische kanonische Records je
  Dokumenttyp, Manifest und Root werden als Duplikat diagnostiziert, bleiben
  aber fuer High-Water und Slotbelegung besetzt;
- `ReadError` oder `CapacityError` beim bytegenauen Zweitread eines
  moeglichen Duplikatpaars blockiert fail closed und gilt nicht als
  byteidentischer Nachweis;
- Kollision eines referenzierten mit einem verwaisten Record sowie zweier
  verwaister Records;
- jede Kollision unterschiedlicher Inhalte liefert
  `ConfigurationGraphIntegrityFailure` beziehungsweise im bereits laufenden
  Dienst `PersistentConfigurationIdentityCollision`;
- bei einer solchen Kollision gibt es keine normale Runtimefreigabe, Mutation
  oder Slotwiederverwendung;
- der Kollisionsscan laeuft beim normalen Graphload und vor jeder Mutation und
  erkennt vorhandene Kollisionen, statt nur die neu zu erzeugenden Werte zu
  vergleichen.

### Schutzmengen- und Rotationstests

- Ausgangsgraph ohne Fallback;
- exakt geschuetzte Active-/Fallback-Dokument- und Manifestplaetze;
- Altroot schuetzt keine nur noch von ihm referenzierte Generation;
- geaendertes einzelnes Dokument;
- zwei geaenderte Dokumente;
- alle drei geaenderten Dokumente;
- unveraenderte Dokumente behalten exakte Referenz und Revision;
- echter Slotmangel je Dokumenttyp und beim Manifest;
- kein Rootwrite bei `NoUnreferencedSlotAvailable`;
- mindestens fuenf aufeinanderfolgende Active-Commits;
- nach jedem Commit neues Active und vorheriger tatsaechlich kanonischer Graph
  als genau ein nutzbarer Fallback;
- Wiederverwendung verwaister Vor-Root-Records ohne Schutzverletzung;
- Abbruch nach Dokumentwrite, danach anderer Kandidat: hoehere Revision fuer
  den anderen Inhalt;
- Abbruch nach Manifestwrite, danach anderer Kandidat: hoehere Generation fuer
  den anderen Manifestinhalt;
- verwaiste hoehere Dokumentrevision je Dokumenttyp bestimmt den
  Dokument-High-Water-Mark;
- verwaiste hoehere Manifestgeneration bestimmt den Manifest-High-Water-Mark;
- hoeherer technisch gueltiger, aber nicht nutzbarer Root bestimmt den
  Root-High-Water-Mark;
- `NotFound` traegt zu keinem High-Water-Mark bei;
- `ReadError` und `CapacityError` in jedem erforderlichen High-Water-Slot
  blockieren die Mutation vor jedem Write;
- unbekanntes neueres technisch gueltiges Dokument-, Manifest- oder Rootschema
  wird nicht ueberschrieben und blockiert vor jedem Write;
- unterschiedliche kanonische Inhalte erhalten nie denselben fachlichen
  Revisions-, Generations- oder Sequenzwert;
- Revision, Generation und Rootsequenz mit High-Water-Mark an und ueber der
  Ueberlaufgrenze.

### Preview-, Validierungs- und Konflikttests

- gueltige User-, Service- und ProgramCatalog-Aenderung einzeln und kombiniert;
- maximal gueltiger ProgramCatalog-Kandidat;
- ungueltiger Kandidat wird nie sichtbar;
- exakt ein sichtbares Preview;
- ein sichtbares geaendertes Preview A mit maximalem ProgramCatalog belegt die
  zweite Modellgeneration; eine unterschiedliche maximale Previewanfrage B
  endet vor tiefer Kopie mit `ConfigurationModelBudgetBusy` und A bleibt
  sichtbar;
- aktiver maximaler ProgramCatalog plus bestaetigter Kandidat A plus parallele
  neue volle Previewanfrage B erzeugen weiterhin exakt zwei Modellgenerationen;
  B wird typisiert abgelehnt;
- eine ungueltige neue Anfrage erhaelt die bereits sichtbare Vorschau;
- Abbrechen entfernt nur den erfassten Preview-Handle ohne persistente
  Wirkung und laesst eine inzwischen neuere Vorschau bestehen;
- Neustart verwirft ohne Persistenz;
- sichtbares leichtes `NoChange` besitzt kein zweites Vollmodell;
- Bestaetigung eines `NoChange` prueft Handle, Basis, Modus und Zustandsrevision,
  liefert `NoChange` ohne Storezugriff oder Zaehlerfortschritt und entfernt nur
  genau diesen Handle;
- Abbruch oder Ersetzung eines `NoChange` entfernt keine inzwischen neuere
  Vorschau;
- exakter Inhaltsvergleich erkennt unterschiedliche Inhalte auch bei
  absichtlich gleicher testseitiger CRC-Metadatenvorgabe;
- veraltete Epoche;
- veraltete Active-Manifestreferenz;
- geaenderte Kandidatenintegritaet;
- erneuter fachlicher oder Plattformvalidierungsfehler vor Commit;
- geaenderte Aktivierungswirkung;
- zwei Display-/Webaehnliche Bestaetigungen: genau eine gewinnt;
- zwei parallele volle Previewanfragen: genau eine erwirbt die Modellreservierung
  und kann sichtbar werden; die andere endet vor Vollmodellaufbau als
  `ConfigurationModelBudgetBusy`;
- eine Previewanfrage B beginnt ihre begrenzte Vorpruefung noch in
  `Operational`, A wechselt anschliessend zu `CommitInProgress`, und B wird vor
  Vollmodellinstallation wegen Modus-/Zustandsrevisionsabweichung abgelehnt;
- Commit A liest und schreibt ausschliesslich Kandidat A aus seinem erfassten
  unveraenderlichen Handle;
- eine kontrollierte interne State-Machine-Testnaht setzt unmittelbar vor dem
  fail-closed Linearisierungspunkt eine sichtbare Vorschau B: Eintritt in
  `CommitIndeterminate` leert B atomar; dieselbe Matrix gilt fuer
  `RuntimeFailure`;
- Preview B beginnt vor einem injizierten fail-closed Uebergang und beendet die
  Berechnung danach: Installation scheitert fuer `CommitIndeterminate` und
  `RuntimeFailure` jeweils an Modus beziehungsweise Zustandsrevision;
- eine Vorschau auf alter Active-Basis wird nach einem erfolgreichen Commit
  nicht installiert beziehungsweise bei Bestaetigung sicher als stale
  abgelehnt;
- Preview C, das erst nach erfolgreichem Commit und Freigabe eines allenfalls
  gepinnten alten Modells gegen die neue Active-Basis installiert wird, bleibt
  sichtbar und wird von der identitaetsgebundenen Bereinigung von A nicht
  entfernt;
- kontrollierte Erstellungs-, Ersetzungs-, Erfassungs-, Abbruch- und
  Commitinterleavings besitzen keine ungeschuetzte Datenrace-Situation und
  keinen Use-after-free oder Deadlock;
- keine RunAssessment-, Auth- oder Safetyentscheidung im Dienst.

### Servicezustands- und Fail-closed-Tests

- `Operational -> CommitInProgress -> Operational` bei eindeutigem Fehler vor
  Root mit weiterhin positiv feststehendem alten Graphen sowie bei
  erfolgreichem Publish;
- Root-`ReadError`/`CapacityError`, Graphintegritaetsfehler,
  Identitaetskollision und interne Invariantenverletzung vor Root wechseln
  dagegen zu `RuntimeFailure`, leeren Preview und sperren neue Runtimefreigabe;
- `CommitInProgress -> CommitIndeterminate` sowie
  `CommitInProgress -> RuntimeFailure` wechseln Modus und Zustandsrevision und
  leeren den gesamten sichtbaren Previewslot unter demselben Zustandsmutex;
- Preview B beginnt vor dem Fehler und endet danach: keine Installation;
- Preview B ist an der internen State-Machine-Testnaht sichtbar und der Fehler
  tritt ein: B ist danach fuer beide fail-closed Modi entfernt;
- ein neuer Snapshotleser am fail-closed Linearisierungspunkt erhaelt entweder
  eindeutig die alte Read-Lease davor oder eindeutig
  `ConfigurationRuntimeUnavailable` danach;
- nach dem Linearisierungspunkt werden weder normale Preview- noch
  Snapshotfreigaben erteilt;
- bereits vorher ausgegebene Read-Leases bleiben speichersicher, begruenden
  aber keine neue normale Runtimefreigabe;
- Modus, Zustandsrevision, Previewslot, Publisher sowie Reader- und
  Modellbudget bleiben bei erzwungenen Interleavings frei von Data Races,
  Use-after-free und Deadlocks.

### Cut-Point- und Persistenzmatrix

Fuer jede moegliche Writeposition der konkreten Mutation:

- `WriteError` vor Wirkung;
- `CapacityError` vor Wirkung;
- `CommitOutcomeUnknown` mit eindeutig altem Record;
- `CommitOutcomeUnknown` mit eindeutig neuem Record;
- Neustart direkt vor dem Write;
- Neustart direkt nach dauerhaftem Write.

Positionen umfassen mindestens:

- jede geaenderte Dokumentrevision;
- Manifest;
- Root.

Fuer Dokument und Manifest wird bewiesen:

- vor Root bleibt der alte kanonische Graph wirksam;
- ein eindeutig neuer unreferenzierter Record aktiviert nichts;
- ein unaufloesbarer Readback vor Root erzeugt keinen Rootwrite;
- spaetere sichere Wiederverwendung bleibt moeglich.

Fuer Root wird bewiesen:

- Unknown + Vollscan eindeutig exakt alt: alter Graph und alter Snapshot werden
  lokal wieder normal akquirierbar, kein neuer Publish, Preview bleibt leer,
  verwaiste neue Records bleiben in den High-Water-Marks und die spaetere
  #24-Verriegelung bleibt unangetastet;
- Unknown + Vollscan eindeutig exakt neu und weiterhin gueltige
  `RuntimePreparationBinding`: exakt der gebundene vorbereitete Snapshot wird
  publiziert;
- Unknown + Vollscan eindeutig exakt neu, aber ungueltige vorbereitete Bindung:
  vollstaendige fallible Neuerstellung vor Runtimefreigabe;
- Fehlschlag dieser Neuerstellung wechselt ohne Rollback in
  `ConfigurationRuntimeFailure`;
- Unknown + Root-`ReadError`;
- Unknown + Root-`CapacityError`;
- Unknown + Graph-`ReadError`;
- Unknown + Graph-`CapacityError`;
- Unknown + Envelope-/CRC-Fehler;
- Unknown + Referenzintegritaetsfehler;
- Unknown + fachlicher Semantikfehler;
- wiederholter fehlgeschlagener Aufloesungsscan;
- spaeterer erfolgreicher Scan loest eindeutig alt oder neu auf;
- Neustartscan loest eindeutig alt oder neu auf;
- Neustartscan bleibt unklar;
- der fluechtige Aufloesungskontext bindet Epoche, alten Graphen, erwarteten
  Zielroot/-graphen, Sequenz, Manifestgeneration, Prepared-Snapshot,
  Runtimevorbereitungsbindung und Ursache und wird bei keinem Scan umgedeutet;
- Neustart setzt weder Preview, Prepared-Snapshot noch fluechtigen
  Aufloesungskontext voraus;
- kein Publish, keine Mutation und keine Slotwiederverwendung im unbestimmten
  Zustand;
- kein automatischer Rollback und kein Factory-Fallback;
- Root-`Success` mit nachgelagertem Readbackfehler erzeugt
  `ConfigurationRuntimeFailure`, nicht einen normalen Vor-Commit-Fehler.

Fuer `ConfigurationRuntimeFailure` wird zusaetzlich bewiesen:

- `PersistentGraphVerificationFailure` darf nur nach einem fehlerfreien
  Vollscan der weiterhin exakt erwarteten Active-Root-/Graphidentitaet und
  gueltiger lokaler Runtimebindung zu `Operational` zurueckkehren;
- `PostCommitVerificationFailure` darf nur nach exaktem Vollscan des erwarteten
  Graphen und vollstaendiger neuer Runtimevorbereitung zu `Operational`
  zurueckkehren;
- `RuntimePreparationAfterResolutionFailure` erlaubt hoechstens den
  spezifizierten begrenzten erneuten Vorbereitungsversuch;
- `PublishContractViolation`, `ServiceStateInvariantViolation` und
  `ConfigurationModelBudgetInvariantViolation` lassen sich in-process nicht
  zuruecksetzen und verlangen Neustart/#57-Rekonstruktion;
- `PersistentConfigurationIdentityCollision` bleibt bis zu einem spaeter
  ausdruecklich freigegebenen Recoveryvertrag fail closed;
- jeder erlaubte Verifikationsscan schreibt nichts und gibt weder Runtime,
  Preview, Mutation noch Slots vor seinem vollstaendigen Erfolg frei;
- kein lokaler Aufloesungsweg loescht automatisch die spaetere #24-Verriegelung.

### Runtime- und Publish-Tests

- alle falliblen Kandidaten-, Plattform-, Groessen- und Snapshotarbeiten sind
  vor dem Rootwrite abgeschlossen;
- Allokationszaehler meldet null neue Allokationen innerhalb
  `publishPrepared(...)`;
- Allokations- und Destruktionszaehler melden im kritischen
  `publishPrepared(...)` weder neue Allokation noch Freigabe des alten
  Snapshots;
- Publish serialisiert, validiert, reserviert, protokolliert und liest nichts;
- der atomare Austausch uebergibt den alten Publisher-Handle an den vorab
  leeren Besitzer, ohne ihn im kritischen Schritt zu zerstoeren;
- Leser sehen unter kontrollierten Interleavings nur vollstaendig alt oder
  vollstaendig neu;
- alte `RuntimeConfigurationReadLease`-Objekte bleiben fuer bereits laufende
  Leser unveraendert;
- ein Leser haelt die alte Read-Lease ueber den Publish hinaus und kann den
  alten Snapshot vollstaendig lesen;
- die verzoegerte Freigabe des Publisher-Handles ausserhalb des kritischen
  Schritts zerstoert den alten Snapshot erst nach Freigabe des letzten
  Reader-Lease;
- ohne alte Read-Lease gibt der verpflichtende nicht kritische
  Retirement-Schritt den alten Publisher-Handle vor Rueckkehr der
  Commitoperation frei und macht die zweite Modellposition wieder verfuegbar;
- bis zu acht nicht kopierbare Read-Leases koennen dieselbe registrierte
  Generation halten; die neunte Akquisition endet mit
  `RuntimeReadLeaseBusy`;
- nach dem ersten Publish mit absichtlich gehaltener alter Read-Lease ist eine
  weitere unterschiedliche Preview beziehungsweise Aktivierung vor tiefer
  Kopie und vor jedem Write mit `ConfigurationModelBudgetBusy` gesperrt;
- nach Freigabe der alten Lease und des ausserhalb des kritischen Schritts
  uebernommenen Publisher-Handles ist die naechste Aktivierung wieder moeglich;
- mehrere erfolgreiche Aktivierungszyklen halten jeweils absichtlich den alten
  Leser, pruefen die voruebergehende Sperre und geben ihn vor dem naechsten
  Erfolg frei;
- kein oeffentlicher frei kopierbarer `shared_ptr` kann den Reader- oder
  Generationszaehler umgehen;
- kontrollierte Interleavings aus neuer Read-Lease, alter Read-Lease, Publish,
  Modellreservierung und verzoegerter Freigabe erzeugen weder Use-after-free,
  Deadlock noch eine dritte Modellgeneration;
- simulierte Publish-Vertragsverletzung erzeugt
  `ConfigurationRuntimeFailure`;
- nach Runtimefehler kein normaler Snapshot und keine weitere Mutation;
- ein aktiver `ProcessRunSnapshot` wird weder referenziert noch umgeschrieben;
- Safety- und Aktorports werden nicht aufgerufen.

### `MutationSequence`-Nachweistests

- jede erfolgreiche Aktivierung verwendet eine Manifestgeneration und
  Rootsequenz oberhalb aller technisch gueltigen gleichartigen Records der
  Epoche;
- nur geaenderte Dokumenttypen erhalten eine neue Revision oberhalb ihres
  High-Water-Marks;
- fehlgeschlagene Vor-Root-Versuche veraendern die kanonische Gesamtordnung
  nicht, ihre technisch gueltigen verwaisten Werte verhindern aber eine
  spaetere Identitaetswiederverwendung fuer andere Inhalte;
- Rootsequenz ordnet jede persistent sichtbare #56-Mutation eindeutig;
- Wiederholung nach altem/neuem/unbestimmtem Ausgang folgt den oben
  definierten Orakeln;
- kein Test benoetigt einen zusaetzlichen persistenten Mutationswert;
- Repositorysuche weist nach, dass #56 keinen `MutationSequence`-Record,
  -Key, -Slot oder Wirewert einfuehrt.

### Ressourcen- und Buildtests

- Peak-Allokationsmessung fuer leeres, typisches und maximal gueltiges Preview;
- zwei unterschiedliche maximale Previewanfragen: genau eine darf die zweite
  Modellgeneration reservieren und aufbauen, die andere endet vor tiefer Kopie
  mit `ConfigurationModelBudgetBusy`;
- ein voller Previewkandidat kann nur unter einer gueltigen
  `ConfigurationPreviewBuildLease` aufgebaut beziehungsweise per Move
  uebergeben werden; Compile-/Schnittstellentests weisen nach, dass keine
  unregistrierte tiefe Kandidatenkopie in den Dienst gelangt;
- aktiver maximaler ProgramCatalog plus bestaetigter maximaler Kandidat plus
  parallele neue Previewanfrage: exakt zwei Vollmodelle und typisierte
  Ablehnung der dritten Anforderung;
- aktiver neuer Snapshot plus absichtlich gepinnte alte Generation: weitere
  Preview- und Aktivierungsanfragen enden vor jedem Write mit
  `ConfigurationModelBudgetBusy`;
- nach Freigabe der letzten Read-Lease und des alten Publisher-Handles wird die
  Modellreservierung frei und die naechste Anfrage kann erfolgreich werden;
- Peak-Allokationsmessung fuer Laden, Fallbackpruefung und Commit eines maximalen
  ProgramCatalog;
- Mutation mit maximalem Active und maximalem Kandidaten verwendet den
  `ValidationOnly`-Graphscan und baut beim Pre-Write-Scan kein drittes
  typisiertes Vollmodell;
- genau ein vollstaendiger Recordarbeitsbereich waehrend normalem
  Load/Encode/Commit; beim bytegenauen Kollisionsvergleich exakt zwei
  begrenzte read-only Recordpuffer und gleichzeitig kein Encoderpuffer oder
  zusaetzliches Vollmodell;
- zu keinem Messpunkt mehr als zwei unterschiedliche vollstaendige
  ProgramCatalog-/Konfigurationsgenerationen und mehr als acht registrierte
  Read-Leases;
- wiederholte Preview-/Abbruch-/Commit-/Readerzyklen ohne wachsenden Live-Heap
  im nativen Allokationszaehler;
- Base-/Head- und Peakbericht nennt explizit
  `kMaxDistinctConfigurationModelGenerations`,
  `kMaxRuntimeConfigurationReadLeases`,
  `kMaxConcurrentFullPreviewBuilds`, tatsaechlich beobachtete Modellzahl,
  Reservierungen und Besitzhandles;
- `pio test -e native` vollstaendig;
- zielgerichtete neue Testgruppen;
- `pio run -e native -e esp32_bringup -e esp32_release`;
- Base-/Head-Bericht mit identischer Toolchain;
- `clang-format`-Pruefung aller betroffenen C++-Dateien;
- `clang-tidy` fuer die betroffenen Produktionsdateien, soweit die bestehende
  Toolchain dies ohne neue Konfiguration unterstuetzt;
- `git diff --check` und Secretpruefung.

Native Allokations- und Timingwerte beweisen keine reale ESP32-Heapreserve,
NVS-Latenz oder Watchdogfreiheit. Diese bleiben sichtbar als spaetere
Messgates.

## Fehler-, Recovery-, Security- und Safetygrenzen

### Fehler und Recovery

- Kein Lese-, Kapazitaets-, CRC-, Referenz-, Schema- oder Fachfehler wird als
  `NotFound` oder fabrikneu umgedeutet.
- Ein unbekannter High-Water-Mark, ein High-Water-Ueberlauf oder ein
  technisch gueltiges neueres unbekanntes Schema sperrt die Mutation vor jedem
  Write und wird nicht durch Wiederverwendung eines scheinbar freien Slots
  umgangen.
- Ein `ReadError` oder `CapacityError` eines Rootslots sperrt die kanonische
  Auswahl auch dann, wenn der andere Rootslot einen vollstaendig gueltigen,
  aber moeglicherweise aelteren Graphen enthaelt.
- Kein Fehler startet Bootstrap, Werksreset oder Factoryinitialisierung.
- Vor Root-Commit bleibt der alte kanonische Graph wirksam.
- Nach bestaetigtem Root-Commit gibt es keinen automatischen Rollback.
- Ein unbestimmter Rootausgang bleibt bis zum erfolgreichen Vollscan fail
  closed.
- #56 erzeugt keine persistente Recoveryabsicht und setzt keinen Fehler zurueck.

### Security

- Konfigurationsgraph, Vorschau, Fingerprint, Diagnose und Fehler enthalten
  keine WLAN-, Passwort-, PIN-, Token- oder Sessiondaten.
- CRC ist kein Securitymechanismus und wird nicht als solcher beschrieben.
- Keine Connectivity-/Authentication-Domaene wird vorbereitet.
- Keine Flashverschluesselungs- oder physische Loeschgarantie wird behauptet.
- Plattformverschluesselung bleibt `EVALUATE_BEFORE_RELEASE` ausserhalb #56.

### Safety

- #56 steuert keine Hardware und gibt keine Aktoren frei.
- Unbekannter, unaufgeloester oder runtimeinkonsistenter
  Konfigurationszustand liefert keine normale Runtimefreigabe.
- Service-PIN, Webzugang oder ChangeOrigin veraendern diese Grenze nicht.
- Der unveraenderliche Laufschnappschuss bleibt ausserhalb der
  Konfigurationsmutation.
- Persistente Verriegelung und `SAFE_BOOT` werden nicht provisorisch in #56
  nachgebaut, sondern spaeter zwingend ueber das benannte Gate integriert.

## Offene Entscheidungen und Gates

| Punkt | Status nach diesem Plan | Behandlung |
|---|---|---|
| separate persistente `MutationSequence` | entschieden: fuer #56/R1 nicht erforderlich | High-Water-basierter Gleichwertigkeits- und Gegenbeispielnachweis in diesem Plan; Scheitern ist materielle Planabweichung |
| Servicezustand und Preview-Synchronisation | entschieden: ein konkreter serviceeigener Zustandsmutex mit checked Zustandsrevision; Previewinstallation und -bereinigung sind darunter identitaetsgebunden linearisiert | Konkurrenz-, Fail-closed-, Lebensdauer- und Data-Race-Nachweis in `native` und allen Buildprofilen |
| Modell- und Readerobergrenzen | entschieden: hoechstens zwei unterschiedliche Vollmodellgenerationen, acht nicht kopierbare Read-Leases und eine volle Previewerstellung | typisierte Ablehnung vor tiefer Kopie beziehungsweise jedem Write sowie Base-/Head- und Peak-Nachweis; absolute Hardwarebudgets bleiben messpflichtig |
| finaler unbestimmter Typname | entschieden: `ConfigurationCommitIndeterminate` | eigener stabiler Zustand und Safety-Producer |
| atomarer Snapshot-Publish | entschieden: vorbereiteter interner C++17-Besitzhandle, vor Root gehaltene Zustandslease und begrenzte `RuntimeConfigurationReadLease`; kein frei kopierbarer Leserhandle | Build-, Allokations-, Lebensdauer- und Interleavingnachweis in allen Profilen; Vertragsaenderung erfordert neuen Plan |
| In-process Aufloesung nach fail closed | ursachenspezifisch entschieden | `CommitIndeterminate` nur ueber gebundenen Vollscan alt/neu; `RuntimeFailure` nur fuer explizit erlaubte Ursachen, sonst Neustart/#57; kein automatisches Loeschen der #24-Verriegelung |
| gemeinsame Mutationskoordination | konkreter `ConfigurationMutationCoordinator` in `fermentation_app` | #56 nutzt eine injizierte Referenz; #57 muss spaeter dieselbe Instanz nutzen; keine zweite Sperre oder Transaktionsplattform |
| reale NVS-Kapazitaet und Replace-Eigenschaften | `MEASUREMENT_REQUIRED` | spaeterer Adapter-/Hardwaretest, keine Hostgarantie |
| absolute Heap-/Flashreserve | `TBD_IMPLEMENTATION_BUDGET`, `MEASUREMENT_REQUIRED` | Base-/Head-Bericht plus spaetere reale Messung |
| reale Commitdauer, Jitter und Watchdogwirkung | `MEASUREMENT_REQUIRED` | spaeter mit realem NVS-/ESP32-Adapter |
| produktiver Storeadapter und Composition Root | `FINAL_SELECTION_PENDING` innerhalb der bereits geplanten Hardwareintegration | nicht in #56 |
| Bootstrap und Reset | #57, `BLOCKED_DEPENDENCY` bis #56 gemergt | eigener Plan-first-Draft-PR |
| Safetyintegration | verbindliches `CONFIGURATION_SAFETY_INTEGRATION_GATE` | #24 muss reale Producer integrieren; nicht in #56 |
| Plattformverschluesselung | `EVALUATE_BEFORE_RELEASE` | separates Security-Gate vor #37 |
| spaeteres Pending/Intent | `FINAL_SELECTION_PENDING` bis realer neustartpflichtiger Bedarf | kein R1-Baustein |
| reale Connectivity-/Authentication-Domaenen | `FINAL_SELECTION_PENDING` bis erster Konsument | keine Reserveinfrastruktur in #56 |

Es verbleibt keine offene Ownerentscheidung, die den Start der exakt hier
geplanten #56-Implementierung nach commitgebundener Planfreigabe blockiert.
Die aufgefuehrten Mess- und Integrationsgates muessen jedoch sichtbar offen
bleiben und duerfen nicht als bestanden behauptet werden.

## Ausdruecklich verbotene Vorwegnahmen

Auch nach Planfreigabe sind verboten:

- Aenderungen ausserhalb der oben genannten Produktions-, Test- und
  Dokumentdateien ohne neue Planfreigabe;
- Aenderung von `IStateStore`, Envelope oder generischer Slotmechanik ohne
  aktualisierten Plan;
- Einfuehrung einer persistenten `MutationSequence` ohne materiellen neuen
  Plan und Ownerfreigabe;
- Ableitung neuer Revisions-, Generations- oder Sequenzwerte nur aus dem
  aktuell kanonischen Graphen, Wiederverwendung einer fachlichen Identitaet
  fuer anderen Inhalt oder Ueberschreiben eines unbekannten neueren Schemas;
- Pending, PendingRoot, Aktivierungsintent oder RunAssessment;
- persistente Preview-Slots, Owner-, Auth- oder Ablaufmetadaten;
- pauschales Entfernen des sichtbaren Previewslots ohne Identitaetsvergleich
  oder Zugriff auf Previewdaten ohne lebensdauerbesitzenden Handle;
- frei kopierbare externe Runtime-Snapshot-Handles, unregistrierte Leser oder
  eine Umgehung der festen Read-Lease-Obergrenze;
- Aufbau einer dritten unterschiedlichen Vollmodellgeneration, Installation
  einer zweiten vollen Previewerstellung oder Rootwrite bei ausgeschoepftem
  Modell-/Lebensdauerbudget;
- Previewinstallation oder normale Snapshotakquisition ohne atomare Pruefung
  von Servicezustand und Zustandsrevision;
- Ignorieren bestehender Dokument-, Manifest- oder Rootkollisionen mit
  unterschiedlichem Inhalt unter derselben persistenten Identitaet;
- automatisches Zuruecksetzen eines fail-closed Servicezustands ausserhalb der
  festgelegten ursachenspezifischen Aufloesungszustaende oder automatisches
  Loeschen der spaeteren #24-Verriegelung;
- Connectivity-/Authentication-Records, Secret-Roots oder Credentialslots;
- Implementierung von #57, #17 oder #24;
- automatische Factoryinitialisierung, Werksreset oder Rollback bei Fehlern;
- Publish mit Allokation, Serialisierung, Validierung, Reservierung oder
  normalem Fehlerpfad nach bestaetigtem Root-Commit;
- Freigabe oder Zerstoerung des alten Publisher-Snapshots im kritischen
  Post-Root-Commit-Austausch;
- zweiter unabhaengiger Mutationsmutex in `ConfigurationService`, Graphstore
  oder der spaeteren #57-Integration;
- Vermischung von CRC und Authentifizierung;
- neue Bibliothek, Toolchain-, Build-, Partitions-, Hardware-, GPIO- oder
  Pinentscheidung;
- Live-Issue-, ADR-, Label-, Milestone- oder Projektstatusaenderung;
- PR auf Ready setzen, mergen, Auto-Merge aktivieren, force-pushen oder Branch
  loeschen.

## Abschlussnachweis vor einem spaeteren Review

Vor dem unabhängigen Abschlussreview dokumentiert der Agent im Draft-PR:

- den freigegebenen Plan-Commit-SHA;
- jeden Planpunkt und den zugehoerigen Umsetzungscommit;
- alle tatsaechlich geaenderten Dateien;
- Base- und Head-SHA des Ressourcenvergleichs;
- alle ausgefuehrten Tests, Builds und Quality Gates;
- Ergebnis der Cut-Point-, Korruptions-, Konflikt- und
  `MutationSequence`-Nachweismatrix;
- gemessene Ressourcenwerte und verbleibende reale Messgates;
- jede Abweichung; bei materieller Abweichung den neu freigegebenen
  Plan-Commit;
- Bestaetigung, dass kein Pending-, Intent-, RunAssessment- oder Secret-Scope
  eingefuehrt wurde;
- offene Reviewthreads;
- Bestaetigung, dass keine nicht freigegebene Entscheidung getroffen wurde.

Mindestens auszufuehren sind:

- alle projektspezifischen neuen nativen Tests;
- vollstaendiges `pio test -e native`;
- Builds `native`, `esp32_bringup`, `esp32_release`;
- Base-/Head-Ressourcenbericht;
- Format- und Konsistenzpruefungen;
- `git diff --check`;
- `python3 scripts/check_secrets.py`;
- relative Markdown-Link- und Tabellenpruefung;
- Schweizer Schreibweise ohne scharfes S;
- Pruefung des tatsaechlichen Diffs gegen diesen freigegebenen Plan und
  `AGENTS.md`.

Der PR bleibt bis zu einem unabhaengigen Abschlussreview Draft.

## Abnahmekriterien des Plan-PRs

Der Planungsauftrag ist abgeschlossen, wenn:

- der aktuelle `main`-Stand und das Live-Issue #56 geprueft sind;
- alle Repository- und Modul-Anweisungen sowie ADR-016, ADR-018 und die
  referenzierten Spezifikationen geprueft sind;
- in dieser Planpraezisierung ausschliesslich diese Plan-Datei geaendert ist;
- konkrete Module, Dateien und der kleine Commit-Schnitt festgelegt sind;
- die gemeinsame Mutationskoordination fuer #56 und die spaetere #57-Nutzung
  mit genau einer konkreten Koordinatorinstanz festgelegt und testbar ist;
- Manifest-, Root-, Active-/Fallback- und Slotrotationsvertrag vollstaendig
  festgelegt sind;
- High-Water-Marks fuer Dokumentrevisionen, Manifestgeneration und
  Rootsequenz verwaiste technisch gueltige Records einschliessen, unbekannte
  Scans fail closed blockieren und Inhaltsidentitaeten nie fuer andere Inhalte
  wiederverwenden;
- gleiche Rootsequenzen nur bei identischen kanonischen Bytes deterministisch
  aufgeloest und bei unterschiedlichen Bytes als Integritaetsfehler gesperrt
  werden;
- derselbe Kollisionsvertrag fuer Dokumentrevisionen und Manifestgenerationen
  gilt, bestehende referenzierte wie verwaiste Kollisionen erkannt werden und
  byteidentische Duplikate ihre Identitaet belegt halten;
- fluechtige Vorschau, `NoChange`-Lebenszyklus, Konfliktschutz und die
  erzwingbaren Obergrenzen von zwei Vollmodellgenerationen, acht Read-Leases
  und einer vollen Previewerstellung festgelegt sind;
- Preview-Ersetzung, Commit-Erfassung und Bereinigung mit unveraenderlichen
  fluechtigen Handles servicezustandslinearisiert und identitaetsgebunden sind,
  sodass parallele Anfragen oder ein laufender Commit weder Kandidaten
  verwechseln noch neuere Vorschauen loeschen;
- ein ausgeschoepftes Modell- oder Readerbudget jede weitere betroffene Aktion
  typisiert vor tiefer Kopie beziehungsweise Rootwrite sperrt und nach
  Lease-/Handlefreigabe wieder nutzbar wird;
- Root-Commit und nicht fehlschlagender Publish eindeutig getrennt sind;
- Rootslot-`ReadError` und `CapacityError` gegen die Verwendung eines
  moeglicherweise aelteren gueltigen Roots fail closed priorisiert sind;
- der alte Publisher-Handle im kritischen Publish nur uebernommen und erst
  ausserhalb dieses Schritts potenziell freigegeben wird;
- `ConfigurationCommitIndeterminate` als endgueltiger Typname und fail-closed
  Verhalten samt gebundenem Aufloesungskontext und vollstaendigen
  Alt-/Neu-/Unklar-Uebergaengen festgelegt ist;
- `ConfigurationRuntimeFailure` ursachenspezifisch nur ueber die festgelegten
  in-process Verifikationswege beziehungsweise Neustart/#57 aufloesbar ist;
- Servicezustand, Previewslot, Runtimepublisher und Budgetzaehler am
  fail-closed Linearisierungspunkt atomar gekoppelt sind und danach keine neue
  normale Preview- oder Runtimefreigabe erfolgt;
- der Gleichwertigkeitsnachweis gegen eine separate persistente
  `MutationSequence` dokumentiert und testbar ist;
- die Grenze zum `CONFIGURATION_SAFETY_INTEGRATION_GATE` zyklusfrei bleibt;
- Cut-Point-, Korruptions-, Konflikt-, Ressourcen- und Base-/Head-Strategie
  vollstaendig beschrieben sind;
- Pending, Intent, RunAssessment, Secret-Scope sowie #57/#17/#24-Umsetzung
  ausdruecklich ausgeschlossen sind;
- `git diff --check`, Secretpruefung, Links, Tabellen und Schreibweise bestanden
  sind;
- ein aktueller Plan-Commit gepusht ist und alle aelteren Plan-Commits als
  ueberholt markiert sind;
- ein Draft-PR mit Plan-Datei, Plan-Commit-SHA, offenen Gates und
  `IMPLEMENTATION_BLOCKED_PENDING_PLAN_APPROVAL` erstellt ist;
- der Agent danach anhaelt und auf die exakte commitgebundene Ownerfreigabe
  wartet.
