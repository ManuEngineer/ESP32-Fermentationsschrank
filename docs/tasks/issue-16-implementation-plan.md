# Implementierungsplan: Variante-B-Neuschnitt von #16, #56 und #57

## Planstatus

- Status: `PLAN_DRAFT`
- Ausgangsbranch: `main`
- Ausgangs-Commit: `311761495a60ad7c6ffba7ef533e0ea3b980128e`
- Planbranch: `plan/issue-16-variante-b-neuschnitt`
- Betroffene Tracking-/Teilissues: #16, #56 und #57
- Abhaengigkeitskorrekturen: #17 und #24
- PR #63 ist mit dem Ausgangs-Commit nach `main` gemergt.
- Dieser Plan aendert noch keine Live-Issues, ADRs, Spezifikationen,
  Produktionsdateien, Tests oder Abhaengigkeiten.

```text
IMPLEMENTATION_BLOCKED_PENDING_PLAN_APPROVAL
```

Die Umsetzung darf erst nach folgendem Ownerkommentar beginnen:

```text
PLAN APPROVED
Approved plan commit: <commit-sha>
```

Die Freigabe gilt nur fuer die Planversion des genannten Commits.

## Ziel

Den mit PR #63 verbindlich entschiedenen OD-01-Vertrag fuer Release 1 in einen
umsetzbaren, kleinen und widerspruchsfreien Persistenzschnitt ueberfuehren:

1. Tracking-Issue #16 auf den bereits umgesetzten Grundlagen #54/#55 und die
   verbleibenden Variante-B-Teilissues #56/#57 ausrichten;
2. #56 auf den Active-/Fallback-Kern, fluechtige vollstaendig validierte
   Vorschau, Konfliktschutz und atomare Runtimeaktivierung reduzieren;
3. #57 auf Bootstrap, `StorageEpoch`, Korruptionssperre und wiederaufnehmbaren
   Werksreset reduzieren;
4. Pending-, Intent- und leere Secret-Infrastruktur aus Release 1 entfernen,
   ohne den additiven spaeteren Ausbaupfad zu verbauen;
5. #17 und #24 nur an die tatsaechlich benoetigten schmalen Vertraege binden;
6. die spaetere Spezifikations- und Live-Issue-Korrektur in einer sicheren,
   ownerfreizugebenden Reihenfolge vorbereiten.

## Nicht-Ziele

Dieser Plan und der nach seiner Freigabe vorgesehene Dokumentations-/Issue-
Neuschnitt implementieren nicht:

- Produktionscode oder produktive Tests;
- Manifeste, Roots, Bootstrap oder Recovery als C++-Implementierung;
- einen NVS-Adapter oder eine Partitionsaenderung;
- neue Abhaengigkeiten, Buildflags oder Toolchainaenderungen;
- persistentes Pending, einen Pending-Root oder ein Aktivierungsintent;
- `ConfigurationActivationRunAssessment` im Konfigurationskern;
- persistente Preview-Slots, Preview-Owner, Preview-Tokens oder Ablaufzeiten;
- Connectivity- oder Authentication-Manifeste ohne realen Konsumenten;
- Credentialslots, Authentication-Roots oder eine `CredentialEpoch`;
- kombinierte Konfigurations-/Secret-Transaktionen;
- reale WLAN-, Passwort-, PIN-, Session- oder CSRF-Daten;
- neue Folgeissues in der Planungsphase;
- eine Aenderung von ADR-016;
- eine Bibliotheks-, Hardware-, GPIO-, Pin- oder Partitionsentscheidung;
- eine Umsetzung oder Neuentscheidung der Import-Run-Gates aus #19-D.

## Verbindliche Quellen und Entscheidungen

### Prioritaet

Die Dokumentationsprioritaet aus
[`docs/SPECIFICATION_REVIEW.md`](../SPECIFICATION_REVIEW.md) gilt. Fuer diesen
Neuschnitt sind insbesondere massgeblich:

1. spaeter datierte akzeptierte ADRs in
   [`docs/DECISIONS.md`](../DECISIONS.md);
2. [`docs/PR38_REVIEW_CORRECTIONS.md`](../PR38_REVIEW_CORRECTIONS.md);
3. spezialisierte Spezifikationen;
4. der mit PR #63 gemergte, vom Owner entschiedene OD-01-Vertrag als Anlass
   fuer die noch ausstehende formale ADR- und Spezifikationskorrektur.

### Unveraendert gueltige Entscheidungen

- ADR-010: Ein vollstaendiger Werksreset behaelt die geraetespezifische
  Touchkalibrierung.
- ADR-013: anwendungsneutrale Persistenzbausteine liegen in
  `device_platform`, fachliche Persistenzsemantik in `fermentation_app` und
  Testadapter in `device_platform_test_support`.
- ADR-016: NVS bleibt produktives Backend; `StateStoreKey`, Envelope, CRC und
  `StorageEpoch` im generischen Envelope bleiben unveraendert gueltig.
- OD-01 aus PR #63: Variante B ist der verbindliche R1-Kern; Variante A ist ein
  spaeterer additiver Ausbaupfad.
- OD-09: reale Connectivity- und Authentication-Domaenen entstehen erst mit
  ihrem ersten produktiven Credentialkonsumenten; #57 erzeugt keine leeren
  Authentifizierungsstrukturen.
- Ein kritischer Konfigurations- oder Publish-Vertragsfehler wird als
  typisiertes Ereignis an den spaeteren Fehler-/Safety-Kern uebergeben. #56 und
  #57 implementieren keine systemweite Fehlerklasse und keine Aktorfreigabe.

### Gepruefte Live-Ausgangslage am 2026-07-27

| Issue | GitHub-Zustand | Body-Status | Befund fuer den Neuschnitt |
|---|---|---|---|
| #16 | offen | `TRACKING` | Body beschreibt noch Pending, Intent und leere Secret-Domaenen |
| #54 | geschlossen | veraltet `READY` | mit PR #59 umgesetzt und abgeschlossen |
| #55 | geschlossen | veraltet `READY` | mit PR #61 umgesetzt und abgeschlossen |
| #56 | offen | `BLOCKED_DEPENDENCY` | Abhaengigkeiten #54/#55 sind erfuellt, Body ist aber Variante A und darf nicht umgesetzt werden |
| #57 | offen | `BLOCKED_DEPENDENCY` | bleibt nach Neuschnitt von #56 abhaengig; Body ist Variante A |
| #17 | offen | `PLANNED_SPEC_PENDING` | pauschale Abhaengigkeit auf Tracking-Issue #16 ist zu breit |
| #24 | offen | `PLANNED_SPEC_PENDING` | braucht spaeter typisierte Persistenz-/Publishfehler, aber keine Variante-A-Infrastruktur |

Auf `main` sind die Fundamente aus #54/#55 vorhanden:

- `device_platform`: Storeport, starke Speichertypen, Envelope, CRC,
  Slotkandidaten und feste technische Grenzen;
- `device_platform_test_support`: simulierter persistenter Store, Cut-Point-
  und Korruptionsinjektion;
- `fermentation_app`: typisierte Konfigurationsdokumente, Codecs, Limits,
  Migration und Storagevertrag fuer User-, Service- und Programmdokumente.

Manifeste, Roots, fachliche Graphvalidierung, Runtime-Snapshot, Bootstrap und
Werksreset sind noch nicht implementiert.

## Notwendige formale ADR

Eine neue ergaenzende ADR ist erforderlich. Grund:

- Der gemergte Audit dokumentiert den Ownerentscheid, waehrend die kanonische
  technische Spezifikation und die Live-Issues weiterhin Variante A fordern.
- Der Unterschied betrifft persistente Recordtypen, Rootstruktur,
  Recoverypfade, Abhaengigkeiten und den Release-1-Scope und ist damit eine
  materielle Architekturentscheidung.
- ADR-016 entscheidet nur Backend und Schluesselraum und darf nicht mit dem
  fachlichen Graphschnitt ueberladen oder umgedeutet werden.

Nach Planfreigabe wird deshalb die zum Ausfuehrungszeitpunkt naechste freie ADR
im zentralen Register angelegt, nach aktuellem Stand voraussichtlich:

```text
ADR-018: Variante-B-Konfigurationspersistenz fuer Release 1
```

Die ADR muss mindestens festhalten:

- Active-/Fallback-Graph als R1-Kern;
- genau eine geschuetzte vorherige vollstaendig nutzbare Fallbackgeneration;
- fluechtige vollstaendig validierte Vorschau;
- Root-Commit als persistenter Linearisierungspunkt und vorbereiteten,
  vertraglich nicht fehlschlagenden Runtime-Publish;
- Bootstrap, `StorageEpoch`, Korruptionssperre und wiederaufnehmbaren Reset;
- keine Pending-/Intent-/leere Secret-Infrastruktur in R1;
- reale Connectivity-/Authentication-Domaenen erst mit erstem Konsumenten;
- additiven Ausbau ueber neue Recordtypen, neue Schema-/Rootversionen und
  Copy-Migrationen;
- ausdrueckliche Fortgeltung von ADR-010, ADR-013 und ADR-016.

Die ADR wird in dieser Planungsphase weder angelegt noch als akzeptiert
vorweggenommen. Die Planfreigabe fuer den konkreten Plan-Commit autorisiert den
dokumentierten ADR-Scope; die Nummer wird unmittelbar vor der Umsetzung gegen
das Live-Register geprueft.

## Zu korrigierende Spezifikations- und Planungsstellen

### `docs/CONFIGURATION_PERSISTENCE.md`

Dieses Dokument benoetigt die groesste Korrektur:

- Status und Geltungsbereich auf den Variante-B-Kern ausrichten.
- `ActiveConfigurationManifest` in R1 nur auf `UserConfiguration`,
  `ServiceConfiguration` und `ProgramCatalog` derselben `StorageEpoch`
  beziehen; keine vorbereitete Connectivity-Referenz.
- Connectivity-/Authentication-Schemas aus der R1-Schemaliste entfernen und
  als spaetere konsumentennahe Domaenen dokumentieren.
- Migrationsvertrag nur fuer Active-/Fallback-Graph und Copy-Migration
  beschreiben; keine Active-/Pending-Doppelmigration.
- physische R1-Slots auf Dokumente, ConfigurationManifest,
  ConfigurationRootRecord und BootstrapRecord beschraenken.
- Schutzmenge auf Active, Fallback und die laufende serialisierte Mutation
  beschraenken; keine Pending-, Intent- oder Authentication-Wurzeln.
- Abschnitte `Pending`, `Aktivierungsabsicht` und `Neustartpflichtige
  Konfiguration` aus dem R1-Vertrag entfernen und in einen klar markierten
  additiven Ausbaupfad verschieben.
- die Vorschau auf einen fluechtigen, vollstaendig validierten Kandidaten mit
  erwarteter Active-Basis und unveraendertem Kandidaten begrenzen; keine
  verpflichtenden persistenten Owner-/Token-/Ablaufdaten oder Preview-Slots.
- Commitresultate `StoredAsPending`, `ReplacedPending` und pendingbezogene
  Aktivierungsfehler aus dem R1-Vertrag entfernen.
- Runtime-Publish ohne Intent-Abschluss beschreiben.
- den aktuellen Abschnitt `Secret-Domaene` durch einen konsumentennahen
  Ausbauvertrag ersetzen; keine R1-Slotzahlen, `NotProvisioned`-Manifeste,
  Prepared-/Committed-Roots oder `CredentialEpoch` vorwegnehmen.
- Bootstrap erzeugt nur die drei Konfigurationsdokumente, Manifest, Root und
  Bootstrapabschluss.
- Werksreset wechselt die `StorageEpoch`, erzeugt die Initialkonfiguration neu,
  erhaelt Touchkalibrierung und erzeugt keine leeren Secret-Manifeste.
- Modulgrenzen von Pending-, Intent- und vorbereiteter Secretsemantik
  bereinigen.
- Ressourcenvertrag und Cut-Point-Matrix auf Variante B reduzieren.
- Pakete C/D auf den neuen #56-/57-Schnitt umbenennen und korrigieren.

### `docs/SETTINGS_AND_STORAGE.md`

- Architekturdiagramm auf Active-/Fallback plus getrennte spaetere reale
  Secret-Domaenen korrigieren.
- Abschnitt `Vorschau, Speichern und Abbrechen` von einem globalen
  15-Minuten-Owner-/Tokenvertrag auf die fluechtige vollvalidierte
  Variante-B-Vorschau reduzieren.
- `Active noch Pending`, Pending-Ersetzung und parallelen Pending-Zweig aus dem
  R1-Vertrag entfernen.
- Abschnitt `Neustartpflichtige Einstellungen` als spaeteren, noch nicht
  implementierten Variante-A-Ausbau markieren. R1 besitzt noch keinen
  neustartpflichtigen produktiven Konfigurationswert.
- atomare Speicherung nur ueber neuen Active-/Fallback-Root und vorbereiteten
  Runtime-Snapshot beschreiben.
- Checkliste `Akzeptierte Entscheidungen` von Pending- und vorbereiteter
  Secretsemantik bereinigen und auf Variante B sowie additiven Ausbaupfad
  verweisen.

### `docs/BACKUP_SECURITY_RETENTION.md`

- Aussage entfernen, #16 stelle `NotProvisioned`-Manifeste bereit.
- Connectivity- und Authentication-Records erst mit dem ersten realen
  Konsumenten spezifizieren; sie bleiben ausserhalb der drei
  Konfigurationsdokumente und ausserhalb normaler Backups.
- Import auf Active-/Fallback-Aktivierung mit dem separaten synchronen
  #19-D-Run-Gate ausrichten; keine Pending-Basis.
- Werksreset als `StorageEpoch`-Wechsel ohne leere Secret-Manifeste
  beschreiben. Spaetere reale Domaenen muessen an die aktuelle Epoche gebunden
  sein und ihre konsumentenspezifischen Recoverytests liefern.
- Erhaltung der Touchkalibrierung unveraendert lassen.

### `docs/IMPLEMENTATION_ISSUES.md`

- Abhaengigkeitsgraph #13/#14/#16 -> #17 ersetzen: #17 wartet nicht mehr auf
  den Abschluss des gesamten Tracking-Issues.
- #16 als Tracking fuer #54/#55/#56/#57 und die neue Reihenfolge darstellen.
- #56 vor #57, #17 parallel auf den bereits gemergten Grundlagen und nur den
  tatsaechlich benoetigten schmalen Vertraegen.
- #24 ohne Abhaengigkeit von Pending, Intent oder Secret-Infrastruktur
  darstellen.

### Zentrales ADR-Register und Aufgabensteuerung

Nach Planfreigabe und vor einer Live-Issue-Aenderung sind ausserdem zu
korrigieren:

- `docs/DECISIONS.md`: neue ergaenzende ADR registrieren;
- `Agent-Auftraege/ESP32-Fermentationsschrank_Agent-Auftraege_Issues_09-37/Issue#16.md`;
- `Agent-Auftraege/ESP32-Fermentationsschrank_Agent-Auftraege_Issues_09-37/Issue#56.md`;
- `Agent-Auftraege/ESP32-Fermentationsschrank_Agent-Auftraege_Issues_09-37/Issue#57.md`;
- der zugehoerige `INDEX.md` fuer Status, Reihenfolge und Links.

Die abgeschlossenen Ausfuehrungsnachweise von #54/#55 werden nicht
nachtraeglich auf Variante B umgeschrieben. Auditdokumente aus PR #63 bleiben
Entscheidungsnachweis und benoetigen fuer diesen Neuschnitt voraussichtlich
keine inhaltliche Korrektur.

## Architektur- und Vertragsgrenzen

### Vorgeschlagene Live-Issue-Titel

- #16 bleibt: `[E2.1] Konfigurationsebenen, Validierung und atomare Revisionen`.
- #56 wird praezisiert zu:
  `[E2.1c] Active-/Fallback-Manifeste, Vorschau und Runtimeaktivierung implementieren`.
- #57 wird korrigiert zu:
  `[E2.1d] Bootstrap, StorageEpoch und Recovery implementieren`.

Damit verschwindet insbesondere `Secret-Manifeste` aus dem Titel von #57. Die
Titel von #17 und #24 bleiben unveraendert.

### Persistenter R1-Graph

```text
ConfigurationRootRecord
  active   -> vollstaendig validiertes ActiveConfigurationManifest
  fallback -> genau eine vorherige vollstaendig nutzbare Generation oder leer

ActiveConfigurationManifest
  -> UserConfiguration-Revision
  -> ServiceConfiguration-Revision
  -> ProgramCatalog-Revision
```

Jede Referenz bindet mindestens Recordtyp, Slot, Revision/Generation,
Schema-Version, Payloadlaenge, CRC und `StorageEpoch`. Ein technisch gueltiger
Record oder ein hoher Sequenzwert allein aktiviert nichts.

### Mutation und Konflikt

Alle Konfigurationsmutationen werden serialisiert. Eine fluechtige Vorschau
enthaelt den vollstaendig typisierten, validierten Kandidaten, die erwartete
Active-Generation und eine stabile Kandidatenintegritaetskennung. Vor Commit
werden Basis, Kandidat, technische/fachliche Validierung und Wirkung erneut
geprueft. Eine Abweichung endet als typisierter Konflikt ohne Write oder
Teilwirkung.

#### Empfehlung zur bisherigen `MutationSequence`

Der Plan empfiehlt, im R1-Variante-B-Kern keine eigene persistente
`MutationSequence` einzufuehren, sofern die nachfolgenden Nachweise im
#56-Plan und in dessen Tests verbindlich erbracht werden:

- Dokumentrevisionen ordnen Aenderungen je Dokumenttyp.
- `ConfigurationRootRecord.rootSequence` ordnet jeden erfolgreichen
  kanonischen Konfigurationscommit.
- erwartete Active-Manifestgeneration plus Kandidatenintegritaetskennung decken
  den optimistischen Konfliktvertrag ab.
- `CommitOutcomeUnknown` wird durch Ruecklesen beider Rootslots und des
  vollstaendigen Zielgraphen aufgeloest.
- vor dem Root-Commit abgebrochene Versuche benoetigen keine dauerhaft sichtbare
  globale Nummer; Luecken oder eine Kreuzdomaenenordnung sind in Variante B
  nicht fachlich beobachtbar.
- BootstrapSequence und `StorageEpoch` decken Bootstrap-/Resetreihenfolge ab.
- reale spaetere Credentialdomaenen erhalten erst mit ihrem Konsumenten eine
  eigene vorwaertsgerichtete Epoche und duerfen keine R1-`MutationSequence`
  voraussetzen.

Falls einer dieser Gleichwertigkeitsnachweise im nachfolgenden #56-Plan nicht
erbracht werden kann, ist dies eine materielle Planabweichung: #56 bleibt
blockiert, diese Plan-Datei wird aktualisiert und erneut ownerfreigegeben. Eine
globale Sequenz darf weder still entfernt noch vorsorglich ohne nachgewiesene
Funktion eingefuehrt werden.

### Runtimeaktivierung

```text
Kandidat bilden
  -> technisch und fachlich validieren
  -> alle falliblen Runtimewerte, Ressourcen und den Snapshot vorbereiten
  -> geaenderte Dokumente schreiben und ruecklesen
  -> Manifest schreiben und vollstaendig validieren
  -> neuen Root mit Active=neu, Fallback=alt committen und ruecklesen
  -> vorbereiteten Snapshot innerhalb derselben Mutation atomar publizieren
```

Der erfolgreiche Root-Commit ist der persistente Linearisierungspunkt. Publish
danach allokiert, serialisiert, validiert und reserviert nichts und ist
vertraglich nicht fehlschlagend. Leser sehen nur den alten oder den neuen
vollstaendigen Snapshot.

Ein unerwarteter Bruch dieses Publishvertrags fuehrt nicht zu automatischem
Rollback. #56 stellt nur einen stabil typisierten
`ConfigurationRuntimeFailure` beziehungsweise gleichwertigen Fehlerintent
bereit und sperrt weitere normale Konfigurationsfreigaben. Fehlerklasse,
persistente Verriegelung, `SAFE_BOOT` und Aktorsperre bleiben #24.

### Bootstrap, Reset und spaetere Domaenen

`StorageEpoch` ist die gemeinsame Resetgrenze. #57 erzeugt und verwaltet nur
den fuer Bootstrap und Konfigurationsgraph notwendigen Zustand. Spaetere reale
Connectivity-/Authentication-Domaenen muessen bei ihrem ersten Konsumenten:

- eigene stark typisierte, versionierte Records und Schluessel definieren;
- jeden persistenten Record an die aktuelle `StorageEpoch` binden;
- eigene Atomizitaets-, Widerrufs-, Cut-Point- und Recoveryvertraege besitzen;
- alte Epochen nach Werksreset nie reaktivieren;
- ihre Daten aus normalen Backups und Diagnosen ausschliessen;
- den zentralen Resetvertrag ergaenzen, ohne #57 rueckwirkend mit Dummyrecords
  oder freien Zukunftsports zu versehen.

Es werden jetzt keine Slots, Keys, Manifeste, Roots, Ports oder leeren Payloads
fuer diese Domaenen reserviert.

## Vollstaendiger vorgeschlagener neuer Body fuer #16

````markdown
## Status

`TRACKING`

Issue #16 bleibt als Tracking-Issue offen und wird nicht direkt implementiert.
Der Release-1-Vertrag folgt der mit OD-01 entschiedenen Variante B. #54 und #55
sind abgeschlossen; #56 und #57 bilden die verbleibenden kleinen
Umsetzungsissues.

## Epic

#4

## Abhaengigkeiten

- #9 – abgeschlossen
- #10 – abgeschlossen
- #12 – abgeschlossen

## Ziel

Eine hardwareunabhaengige, nativ testbare und stromausfallsichere
Konfigurationspersistenz mit getrennten typisierten Dokumenten, einem
vollstaendig validierten Active-/Fallback-Graphen, sicherem Bootstrap und
atomarer Runtimeaktivierung bereitstellen.

## Kanonische Spezifikation

- zentrale akzeptierte ADR zur Variante-B-Konfigurationspersistenz
- `docs/ADR-016_KONFIGURATIONSSPEICHER_BACKEND.md`
- `docs/CONFIGURATION_PERSISTENCE.md`
- `docs/SETTINGS_AND_STORAGE.md`
- `docs/BACKUP_SECURITY_RETENTION.md`
- `docs/PR38_REVIEW_CORRECTIONS.md`

Spaeter datierte akzeptierte ADRs haben gemaess
`docs/SPECIFICATION_REVIEW.md` Vorrang.

## Verbindlicher Release-1-Kern

- `FactoryConfiguration` bleibt unveraenderlich in der Firmware.
- `UserConfiguration`, `ServiceConfiguration` und `ProgramCatalog` sind
  getrennte, typisierte und schema-versionierte Dokumente.
- Ein `ActiveConfigurationManifest` referenziert genau eine vollstaendig
  validierte Kombination dieser Dokumente derselben `StorageEpoch`.
- Ein kanonischer `ConfigurationRootRecord` enthaelt Active und genau eine
  vorherige vollstaendig nutzbare Fallbackgeneration.
- Der Root-Commit ist der einzige persistente Linearisierungspunkt.
- Sichere Slotrotation schuetzt nur Active, Fallback und die laufende
  serialisierte Mutation.
- Vorschau bleibt fluechtig, vollstaendig typisiert und validiert.
- Bestaetigung prueft erwartete Active-Basis, unveraenderten Kandidaten und
  erneute technische/fachliche Validierung.
- Alle falliblen Runtimewerte und Ressourcen werden vor Root-Commit vorbereitet.
- Ein unveraenderlicher `RuntimeConfigurationSnapshot` wird nach Root-Commit
  nicht allokierend, nicht serialisierend und vertraglich nicht fehlschlagend
  publiziert.
- Leser sehen nur die vollstaendig alte oder neue Runtimegeneration.
- Bootstrap initialisiert nur nachweislich fabrikneuen, vollstaendig fehlerfrei
  lesbaren Speicher.
- `StorageEpoch`, Korruptionssperre und ein wiederaufnehmbarer Werksreset sind
  Bestandteil von R1.
- Der Werksreset behaelt gemaess ADR-010 die geraetespezifische
  Touchkalibrierung.

## Bereits abgeschlossen

### #54 – Plattformpersistenz und Wireformat

Status: `COMPLETED`

- `IStateStore`, NVS-faehiger Schluesselraum und starke technische Typen
- Big-Endian-Codecs, CRC-32/ISO-HDLC und Envelope
- generische begrenzte Slot-/Recordmechanik
- `SimulatedPersistentStateStore` und technische Golden-/Cut-Point-Tests

Umgesetzt mit PR #59.

### #55 – Typisierte Konfigurationsdokumente

Status: `COMPLETED`

- UserConfiguration Schema 1
- ServiceConfiguration Schema 1
- ProgramCatalog Schema 1
- fachliche Limits, Codecs, Validierung und Copy-Migration
- schmaler Zeitzonenresolver mit Testadapter

Umgesetzt mit PR #61.

## Verbleibende Teilissues

### #56 – Active-/Fallback-Manifeste, Vorschau und Runtimeaktivierung

Status nach Neuschnitt: `READY`

- vollstaendig validierter Active-/Fallback-Graph
- kanonischer Root und sichere Slotrotation
- fluechtige vollstaendig validierte Vorschau
- Revisions- und Konfliktschutz
- vorbereiteter unveraenderlicher Runtime-Snapshot
- atomarer Root-Commit und vertraglich nicht fehlschlagender Publish
- typisierter Konfigurations-/Publishfehler fuer die spaetere #24-Integration

### #57 – Bootstrap, StorageEpoch und Recovery

Status nach Neuschnitt: `BLOCKED_DEPENDENCY`

Abhaengig von #56.

- sicherer Bootstrap und gespeicherte Bootstrapzustaende
- `StorageEpoch`
- Korruptionssperre ohne stillen Factory-Fallback
- wiederaufnehmbarer Werksreset
- Erhaltung der Touchkalibrierung
- End-to-End-Cut-Point-, Korruptions- und Ressourcenmatrix fuer Variante B

## Ausdruecklich nicht in Release 1 vorbereitet

- persistentes Pending und Pending-Root
- Aktivierungsintent und `ConfigurationActivationRunAssessment`
- persistente Preview-Slots, Preview-Owner, Preview-Tokens oder Ablaufzeiten
- leere Connectivity-/Authentication-Manifeste oder Secret-Roots
- vorbereitete Credentialslots oder Authentication-Roots
- `CredentialEpoch` ohne reale Credentials
- kombinierte Konfigurations-/Secret-Transaktionen

Diese Funktionen werden erst mit einem realen neustartpflichtigen Wert,
WLAN-Secret, Webpasswort oder Service-PIN-Konsumenten additiv ueber neue
Recordtypen, Schema-/Rootversionen und Copy-Migrationen geplant. Es entstehen
keine Dummyrecords, Slots, Keys oder Zukunftsports.

## Grenze zu #17

#17 verwendet die bereits gemergten anwendungsneutralen Persistenzbausteine
und seine eigenen typisierten Laufrecords. Es haengt nicht pauschal vom
Abschluss dieses Tracking-Issues und nicht von #57, Pending, Intent oder
Secret-Domaenen ab.

Der aktive Lauf bleibt ausserhalb der Konfiguration und verwendet seinen
unveraenderlichen Laufschnappschuss. Der synchrone Import-Run-Gate mit
`Unknown`, `NoActiveOrRecoverableRun`, `ActiveRunPresent` und
`RecoverableRunPresent` gehoert spaeter zu #19-D und wird nicht in #56
vorbereitet.

## Grenze zu #24

#56/#57 liefern bei Konfigurations-, Integritaets- oder Publishvertragsfehlern
nur stabile typisierte Fehlerdaten und keine Runtimefreigabe.

Systemweite Fehlerklasse, persistente Verriegelung, `SAFE_BOOT`,
Fehlerresetpolitik und reale Aktor-/GPIO-Sperren bleiben #24. #24 benoetigt
kein Pending, Intent und keine vorbereitete Secret-Domaene.

## Verbindliche Reihenfolge

```text
#54 COMPLETED + #55 COMPLETED
  -> #56 READY
  -> #57 BLOCKED_DEPENDENCY bis #56 gemergt
  -> abschliessende Tracking-Abnahme #16
```

#17 darf nach seinem eigenen Plan-first-Gate auf den bereits gemergten
Grundlagen parallel vorbereitet werden. #24 folgt seinen eigenen fachlichen
Abhaengigkeiten und integriert spaeter die typisierten Konfigurationsfehler.

## Ausdruecklicher Nicht-Scope

- Laufpersistenz und Kontrollpunkte aus #17
- Journal, Historie, Backup und Import aus #19
- systemweite Fehlerklassen und Aktorsperren aus #24
- reale Connectivity-, Authentifizierungs- und Webvertraege aus #27
- Produktions-NVS-Adapter, Partitionierung und reale Flashgarantien
- physische Recovery-Geste

## Definition of Done des Tracking-Issues

Issue #16 wird erst abgeschlossen, wenn:

- #54, #55, #56 und #57 gemergt und abgeschlossen sind;
- der Active-/Fallback-Graph und mindestens fuenf aufeinanderfolgende
  Active-Commits ohne vorzeitigen Slotmangel nachgewiesen sind;
- Bootstrap, Korruptionssperre und Werksreset an jedem Commit-Cut ein
  konkretes Recovery-Orakel besitzen;
- Runtime vor dem Root-Commit vollstaendig vorbereitet und danach nur atomar
  publiziert wird;
- Touchkalibrierung beim Werksreset erhalten bleibt;
- unbekannte neuere Schemas ohne Teilwirkung abgelehnt werden und der additive
  Ausbaupfad ohne Dummyinfrastruktur nachgewiesen ist;
- native Tests, Buildprofile, Quality Gates und Ressourcenvergleiche der
  Teilissues bestanden sind;
- reale NVS-/Partitions-/Flashmessungen als spaetere Gates sichtbar bleiben.
````

## Vollstaendiger vorgeschlagener neuer Body fuer #56

````markdown
## Status

`READY`

## Tracking und Epic

- Teil von #16
- Epic #4

## Abhaengigkeiten

- #54 – abgeschlossen
- #55 – abgeschlossen

#17 und #24 sind keine blockierenden Abhaengigkeiten dieses Issues. Ihre
fachliche Semantik wird nicht vorweggenommen.

## Verbindliche Architekturentscheidung

Release 1 folgt der mit OD-01 und der zentralen Variante-B-ADR entschiedenen
schlanken Konfigurationspersistenz. ADR-016 bleibt fuer NVS, Schluesselraum und
Envelope verbindlich.

## Ziel

Die vorhandenen typisierten Dokumente als vollstaendig validierten
Active-/Fallback-Graphen atomar aktivieren. Eine fluechtige Vorschau und ein
optimistischer Konfliktvertrag fuehren zu einem vorbereiteten
`RuntimeConfigurationSnapshot`; der Root-Commit linearisiert persistent und
der anschliessende Publish ist vertraglich nicht fehlschlagend.

## Scope

### Manifest und Root

- drei `ConfigurationManifest`-Slots und zwei redundante
  `ConfigurationRootRecord`-Slots
- ein `ActiveConfigurationManifest` referenziert exakt je eine
  `UserConfiguration`, `ServiceConfiguration` und `ProgramCatalog`-Revision
- jede Referenz bindet Recordtyp, Slot, Revision, Schema-Version,
  Payloadlaenge, CRC und `StorageEpoch`
- der Root bindet Active, optional genau eine Fallbackgeneration und eine
  monotone `rootSequence`
- Active und Fallback sind vollstaendige Graphen, keine lose Sammlung
  technisch gueltiger Records
- keine Connectivity-/Authentication-Referenz in Schema 1

### Kanonische Graphvalidierung beim Boot und Commit

1. Rootkandidaten technisch pruefen und nach `rootSequence` absteigend liefern;
2. fuer jeden Kandidaten den Active-Zweig vollstaendig technisch und fachlich
   validieren;
3. bei unbrauchbarem Active den Fallback-Zweig desselben Roots vollstaendig
   validieren;
4. den ersten vollstaendig nutzbaren Zweig kanonisch waehlen;
5. Fallbacknutzung stabil diagnostizieren;
6. ohne vollstaendig nutzbaren Graphen keine `RuntimeConfiguration` freigeben.

Ein hoher Sequenzwert, ein gueltiger CRC oder ein einzeln gueltiges Dokument
aktiviert niemals allein eine Generation.

### Sichere Slotrotation und Schutzmenge

Die kanonische Schutzmenge besteht ausschliesslich aus:

- Active des kanonischen Roots;
- dessen genau einer vollstaendig nutzbaren Fallbackgeneration, falls vorhanden;
- allen Records der gerade ausgefuehrten serialisierten Mutation bis zu ihrem
  Abschluss.

Aeltere redundante Rootkopien bleiben technische Bootkandidaten, schuetzen aber
keine nur noch von ihnen referenzierten Generationen. Ein Slot wird erst
wiederverwendet, wenn er ausserhalb der kanonischen Schutzmenge liegt. Fehlt ein
sicherer Slot, wird vor jeder Teilaktivierung typisiert mit
`NoUnreferencedSlotAvailable` und betroffenem Recordtyp abgelehnt.

Ein neuer Active-Commit schreibt und prueft zuerst geaenderte Dokumente und ein
neues Manifest. Danach schreibt er in den nicht kanonischen Rootslot:

```text
Active   = neues Manifest
Fallback = bisheriges Active
```

Erst nach Ruecklesen und Vollvalidierung des neuen Rootgraphen wird dieser
kanonisch. Mindestens fuenf aufeinanderfolgende Active-Commits muessen ohne
vorzeitigen Slotmangel funktionieren.

### Fluechtige validierte Vorschau

- vollstaendig typisierter unveraenderlicher Kandidat im RAM
- erwartete Active-Manifestgeneration
- stabile Integritaetskennung des Kandidaten
- typisierte redigierte Aenderungszusammenfassung
- vollstaendige technische und fachliche Validierung vor Anzeige
- erneute Basis-, Kandidaten- und Validierungspruefung unmittelbar vor Commit
- veraltete Basis oder veraenderter Kandidat: typisierter Konflikt ohne Write
  und ohne Teilwirkung
- `NoChange` erzeugt keine neue Dokumentrevision, Manifestgeneration,
  Rootsequenz oder Storeoperation

Nicht Bestandteil sind persistente Preview-Slots, persistente Owner-/Token-
Metadaten oder Ablaufzeiten. Eine allgemeine Preview-, Provider- oder
Pluginplattform entsteht nicht.

### Serialisierte Mutation und Revisionsschutz

- global hoechstens eine Konfigurationsmutation gleichzeitig
- Dokumentrevisionen ordnen Inhalte je Dokumenttyp
- Manifestgeneration ordnet vollstaendige Kandidatengraphen
- `rootSequence` ordnet erfolgreiche kanonische Commits
- erwartete Active-Generation und Kandidatenintegritaet sichern Konflikte
- `CommitOutcomeUnknown` wird durch Ruecklesen der Rootslots und
  Vollvalidierung des erwarteten Graphen aufgeloest
- Zaehlerwert 0 bleibt reserviert; Ueberlauf wird vor Writes typisiert abgelehnt

Eine eigene persistente `MutationSequence` wird nicht eingefuehrt, solange der
freigegebene #56-Implementierungsplan den Gleichwertigkeitsnachweis aus dem
Neuschnittplan erbringt. Kann er das nicht, ist vor Implementierung eine neue
Ownerfreigabe erforderlich.

### RuntimeConfigurationSnapshot und atomare Aktivierung

Vor dem Root-Commit werden:

- der vollstaendige Kandidat erneut validiert;
- Plattformwerte wie die Zeitzone aufgeloest;
- alle falliblen Ressourcen reserviert;
- alle benoetigten Recordgroessen geprueft;
- ein vollstaendiger unveraenderlicher `RuntimeConfigurationSnapshot`
  vorbereitet.

Der Ablauf lautet:

1. Kandidat und Basis unter exklusiver Mutation erneut pruefen;
2. alle Runtimewerte und Ressourcen vorbereiten;
3. geaenderte Dokumente schreiben, ruecklesen und validieren;
4. Manifest schreiben, ruecklesen und als Graph validieren;
5. neuen Root schreiben;
6. bei `CommitOutcomeUnknown` den Ausgang durch Readback bestimmen;
7. Root und gesamten Zielgraphen ruecklesen und validieren;
8. erfolgreichen Root-Commit als persistenten Linearisierungspunkt behandeln;
9. vorbereiteten Snapshot ohne Allokation, Serialisierung, Validierung oder
   weitere Reservierung atomar sichtbar machen;
10. Mutation freigeben.

Leser sehen nur den vollstaendig alten oder neuen Snapshot. Fehler vor
Root-Commit lassen persistenten kanonischen Graph und Runtime unveraendert.
Stromausfall vor Root-Commit laedt den alten, nach Root-Commit den neuen Graphen.

Publish nach bestaetigtem Root-Commit ist vertraglich nicht fehlschlagend. Eine
unerwartete Vertragsverletzung erzeugt nur einen stabil typisierten
`ConfigurationRuntimeFailure` beziehungsweise gleichwertigen Fehlerintent,
gibt keine weitere normale Konfigurationsruntime frei und fuehrt nicht zu
automatischem Rollback.

### Grenze zu #24

#56 definiert ausschliesslich den typisierten Konfigurations-/Publishfehler und
den fail-closed Zustand des Konfigurationsdienstes. Systemweite Fehlerklasse,
persistente Verriegelung, `SAFE_BOOT`, Fehlerreset und reale Aktor-/GPIO-Sperren
bleiben #24.

Die spaetere #24-Integration konsumiert den typisierten Fehler. Es besteht keine
zyklische Implementierungsabhaengigkeit.

## Ausdruecklicher Nicht-Scope

- persistentes Pending oder Pending-Root
- Aktivierungsintent oder `ConfigurationActivationRunAssessment`
- neustartpflichtige Konfigurationsaktivierung
- persistente Preview-Slots, Owner, Tokens oder Ablaufzeiten
- Bootstrap und Werksreset aus #57
- Connectivity-/Authentication-Manifeste, Secretslots oder Roots
- Credentialdaten, `CredentialEpoch` oder kombinierte Secret-Transaktionen
- Laufpersistenz und Import-Run-Gate
- systemweite Fehler-/Safety- oder Aktorpolitik
- produktiver NVS-Adapter, Partitionierung und reale Flashgarantien

## Architekturgrenzen

- `fermentation_app` besitzt Dokument-, Manifest-, Root-, Graph-, Vorschau-,
  Konflikt- und Runtimebedeutung.
- `device_platform` bleibt bei technischen Slots, Envelopes, Kandidaten,
  Speichertypen und Storefehlern.
- `device_platform_test_support` liefert nur anwendungsneutrale Store-,
  Cut-Point- und Korruptionsadapter.
- `src/main.cpp` bleibt Composition Root.
- aktiver Lauf und Laufschnappschuss bleiben ausserhalb der Konfiguration.

## Verbindliche Tests

### Graph und Boot

- gueltiger Active-Graph
- technisch gueltiger Root mit fachlich ungueltigem Active und gueltigem
  Fallback
- ungueltiger Active und ungueltiger/fehlender Fallback: keine Runtime
- mehrere Rootkandidaten: erster vollstaendig nutzbarer Graph gewinnt
- hoehere Sequenz ohne gueltigen Graph aktiviert nichts
- Abweichungen bei Typ, Slot, Revision, Generation, Schema, Laenge, CRC und
  `StorageEpoch`
- unbekannte neuere Root-/Manifest-Schemas ohne Teilwirkung

### Rotation und Schutz

- Bootstrapgraph ohne Fallback als Ausgang
- mindestens fuenf aufeinanderfolgende Active-Commits
- nach jedem Commit Active neu und Fallback exakt vorheriges Active
- nur kanonische Schutzmenge blockiert Slots
- Altroots schuetzen keine ausschliesslich referenzierten Generationen
- echter Slotmangel liefert typisierten Fehler ohne Write am Root
- unveraenderte Dokumentrevisionen koennen sicher gemeinsam referenziert werden

### Vorschau und Konflikte

- gueltiger Kandidat und bestaetigte unveraenderte Basis
- `NoChange` ohne Revision/Generation/Rootsequenz/Write
- veraltete Active-Generation
- veraenderter Kandidat oder Fingerprint
- erneuter Validierungsfehler unmittelbar vor Commit
- konkurrierende Display-/Webmutation: genau eine gewinnt, die andere erhaelt
  Konflikt ohne Teilwirkung
- Neustart verwirft jede fluechtige Vorschau

### Commit und Runtime-Publish

- Fehler vor und nach jedem Dokument-, Manifest- und Rootwrite
- `CommitOutcomeUnknown` fuer jeden Write mit Readback-Orakel
- Ressourcen-, Kapazitaets- und Ueberlauffehler vor Root-Commit
- alle falliblen Arbeiten vor Root-Commit nachweisbar abgeschlossen
- Publish nach Root-Commit ohne Allokation, Serialisierung oder Validierung
- Leser beobachten unter wiederholten Zugriffen nie Teilgenerationen
- simulierte Publish-Vertragsverletzung erzeugt nur typisierten Fehler und
  keine normale neue Freigabe
- kein automatischer Rollback nach bestaetigtem Root-Commit

### Ressourcen und Builds

- waehrend des Commit-Workflows hoechstens der vertraglich begrenzte
  vollstaendige Recordarbeitsbereich
- Base-/Head-Vergleich fuer Flash, statisches RAM, `firmware.bin` und
  `firmware.elf`
- native Tests und alle drei Buildprofile
- keine Heap-, NVS- oder Flashgarantie ohne reale Messung

## Akzeptanzkriterien

- Active/Fallback sind jederzeit vollstaendige validierte Graphen.
- Genau eine vorherige vollstaendig nutzbare Generation bleibt geschuetzt.
- Fuenf Active-Commits beweisen sichere Slotrotation.
- Vorschau und Commit sind fluechtig, vollvalidiert und konfliktgesichert.
- Root-Commit ist der einzige persistente Linearisierungspunkt.
- Alle falliblen Arbeiten liegen vor dem Root-Commit.
- Publish danach ist nicht allokierend, nicht serialisierend und vertraglich
  nicht fehlschlagend.
- Kein Pending-, Intent-, RunAssessment- oder Secretbaustein wurde eingefuehrt.
- Grenzen zu #17 und #24 sind dokumentiert, ohne deren Fachlogik vorwegzunehmen.
- Tests, Buildprofile, Quality Gates und Ressourcenvergleich sind bestanden.

## Definition of Done

- Variante-B-Manifeste, Root, Graphvalidierung, Rotation, fluechtige Vorschau,
  Konfliktschutz und Runtime-Publish implementiert
- alle verbindlichen Tests gruen
- #54/#55 abgeschlossen
- Dokumentation und `CHANGELOG.md` aktualisiert
- kleiner eigener Plan-first-Draft-PR fuer #56, unabhaengig reviewed und nicht
  durch den Agenten gemergt

## Spezifikationsquellen

- zentrale akzeptierte Variante-B-ADR
- `docs/CONFIGURATION_PERSISTENCE.md`
- `docs/SETTINGS_AND_STORAGE.md`
- `docs/BACKUP_SECURITY_RETENTION.md`
- `docs/PR38_REVIEW_CORRECTIONS.md`
````

## Vollstaendiger vorgeschlagener neuer Body fuer #57

````markdown
## Status

`BLOCKED_DEPENDENCY`

## Tracking und Epic

- Teil von #16
- Epic #4

## Abhaengigkeiten

- #54 – abgeschlossen
- #55 – abgeschlossen
- #56 – nach Variante-B-Neuschnitt zu implementieren und zu mergen

## Verbindliche Architekturentscheidung

Release 1 implementiert sicheren Bootstrap, `StorageEpoch`,
Korruptionssperre und wiederaufnehmbaren Werksreset auf dem Variante-B-
Active-/Fallback-Kern. Connectivity- und Authentication-Domaenen entstehen erst
mit ihrem ersten realen Konsumenten.

## Ziel

Den Variante-B-Konfigurationsgraphen bei fabrikneuem Speicher sicher
initialisieren, bei Boot eindeutig laden oder fail closed sperren und einen
ausdruecklich ausgeloesten Werksreset nach jedem Stromunterbruch idempotent
fortsetzen. Geraetespezifische Touchkalibrierung bleibt erhalten.

## Scope

### BootstrapRecord

- zwei redundante `ConfigurationBootstrapRecord`-Slots
- Felder mindestens: `BootstrapSequence`, Speicherformatversion,
  `StorageEpoch` und Zustand
- gespeicherte Zustaende mindestens:
  - `Initializing`
  - `Initialized`
  - `Resetting`
- `NotFound` ist kein gespeicherter Zustand
- Lese-, Kapazitaets- und Integritaetsfehler werden nie wie `NotFound`
  behandelt
- BootstrapSequence und `StorageEpoch` beginnen bei 1; 0 ist reserviert und
  Ueberlauf wird vor Writes abgelehnt

### Nachweislich fabrikneuer Speicher

Automatische Initialisierung ist nur erlaubt, wenn:

- alle erforderlichen Reads technisch erfolgreich abgeschlossen wurden;
- alle Bootstrap-, Root-, Manifest- und Konfigurationsslots eindeutig
  `NotFound` melden;
- keine gueltigen, unbekannten, beschaedigten oder unlesbaren Altbytes erkannt
  wurden;
- kein BootstrapRecord existiert.

Ein Readfehler, CRC-Fehler, unbekanntes Schema, ungueltiger Root oder
widerspruechlicher Slotzustand ist niemals fabrikneu.

### Initialisierung unter `StorageEpoch` 1

1. `Initializing` dauerhaft schreiben und ruecklesen;
2. UserConfiguration Revision 1 mit den bestaetigten Factory-Startwerten
   erzeugen;
3. ServiceConfiguration Revision 1 erzeugen;
4. ProgramCatalog Revision 1 mit vier Factory-Arbeitskopien erzeugen;
5. ConfigurationManifest Generation 1 schreiben und vollstaendig validieren;
6. ConfigurationRootRecord rootSequence 1 mit Active Generation 1 und ohne
   Fallback schreiben, ruecklesen und als Graph validieren;
7. vorbereiteten Factory-`RuntimeConfigurationSnapshot` nach dem #56-Vertrag
   publizieren;
8. Bootstrap auf `Initialized` fortschreiben.

Nach einem Stromausfall wird der Ablauf aus `Initializing` anhand der
persistierten Records idempotent fortgesetzt. Es entstehen keine
Connectivity-/Authentication-Manifeste, Secret-Roots oder Dummyrecords.

### Normaler Boot

- kanonischen BootstrapRecord technisch und fachlich bestimmen
- `StorageEpoch` und Speicherformat validieren
- bei `Initialized` den vollstaendigen Variante-B-Rootgraphen aus #56 laden
- Active, ersatzweise genau einen gueltigen Fallback verwenden
- Fallbacknutzung sichtbar diagnostizieren
- Runtime-Snapshot vor Freigabe vollstaendig vorbereiten
- bei `Initializing` Initialisierung idempotent fortsetzen
- bei `Resetting` Werksreset idempotent fortsetzen
- bei Korruption, unbekanntem Schema, unlesbarem Speicher oder fehlendem
  nutzbarem Active/Fallback keine Runtime freigeben

### StorageEpoch

- jeder R1-Konfigurations- und Bootstraprecord ist an die aktuelle Epoche
  gebunden
- Referenzen ueber Epochen hinweg sind ungueltig
- ein abgeschlossener Werksreset macht alte Epochen logisch unerreichbar
- ohne Plattformnachweis wird keine sichere physische Loeschung alter Flashbytes
  behauptet
- spaetere reale Connectivity-/Authentication-Domaenen muessen ihre Records
  bei ihrem ersten Konsumenten an dieselbe Resetgrenze binden

### Beschaedigte oder unbekannte Daten

- vorhandene ungueltige, beschaedigte oder unlesbare Daten loesen keinen
  Factory-Bootstrap und keinen automatischen Werksreset aus
- unbekannte oder nicht migrierbare Schemas werden ohne Teilwirkung abgelehnt
- ohne nutzbaren Active/Fallback entsteht ein typisierter
  `ConfigurationUnavailable` beziehungsweise `ConfigurationIntegrityFailure`
- keine `RuntimeConfiguration` und keine Aktorfreigabe
- systemweite Fehlerklasse, persistente Verriegelung und `SAFE_BOOT` bleiben #24

### Wiederaufnehmbarer Werksreset

Der Reset wird nur durch einen bereits fachlich autorisierten, ausdruecklichen
lokalen Resetauftrag gestartet. UI, PIN-Pruefung, Raw-Touch-Geste, Laufgate,
Historienloeschung und Safetyfreigabe liegen bei ihren zustaendigen Issues.

Persistenter Ablauf:

1. naechste `StorageEpoch` und BootstrapSequence ohne Ueberlauf bestimmen;
2. `Resetting` unter der neuen Epoche dauerhaft schreiben und ruecklesen;
3. damit alle Records alter Epochen logisch unerreichbar machen;
4. neue UserConfiguration, ServiceConfiguration und ProgramCatalog-
   Anfangsrevisionen schreiben und pruefen;
5. neues ConfigurationManifest und neuen Root ohne Fallback schreiben und den
   gesamten Graphen validieren;
6. vorbereiteten Factory-Runtime-Snapshot nach dem #56-Vertrag publizieren;
7. Bootstrap als `Initialized` abschliessen.

Nach jedem Cut wird aus `Resetting` idempotent fortgesetzt. Vor dem
Linearisierungspunkt bleibt kein teilweise neuer Graph wirksam; nach dem
Root-Commit bleibt die neue Epoche kanonisch.

Der Reset:

- erhaelt gemaess ADR-010 die geraetespezifische Touchkalibrierung;
- schreibt, loescht oder ueberschreibt keine Touchkalibrierungsrecords;
- erzeugt keine leeren Connectivity-/Authentication-Manifeste oder Secret-Roots;
- reaktiviert keine Records einer alten `StorageEpoch`;
- behauptet keine physische Flashloeschung;
- wird nie automatisch durch Korruption ausgeloest.

Ein gesonderter Recoveryfall fuer unbrauchbare Touchkalibrierung bleibt vom
normalen Werksreset getrennt.

### Grenze zu spaeteren realen Connectivity-/Authentication-Domaenen

#57 reserviert keine Keys, Slots, Records, Manifeste, Roots, CredentialEpoch
oder Ports fuer Secrets. Der erste produktive WLAN-, Webpasswort- oder
Service-PIN-Konsument muss in einem eigenen ownerfreigegebenen Plan:

- sein typisiertes Schema und seine Schluessel definieren;
- Records an die aktuelle `StorageEpoch` binden;
- atomaren Commit, Widerruf und fail-closed Recovery festlegen;
- Werksreset- und Cut-Point-Verhalten ergaenzen;
- verhindern, dass alte Epochen oder Credentials reaktiviert werden;
- Secret-, Redaction-, Backup- und Plattformschutzvertraege nachweisen.

### Grenze zu #24

#57 liefert bei nicht verfuegbarer oder beschaedigter Konfiguration nur stabile
typisierte Fehlerdaten und keine Runtimefreigabe. Fehlerklasse, persistente
Verriegelung, Bootprioritaet, `SAFE_BOOT` und reale Aktorsperren bleiben #24.

## Ausdruecklicher Nicht-Scope

- persistentes Pending, PendingRoot oder Aktivierungsintent
- `ConfigurationActivationRunAssessment`
- persistente Previewdaten
- Connectivity-/Authentication-Manifeste oder Secretslots
- Prepared-/Committed-Authentication-Roots
- `CredentialEpoch`, Credentialwechsel, KDF, Sessions oder CSRF
- kombinierte Konfigurations-/Secret-Transaktionen
- Lauf-, Journal- und Historienpersistenz
- physische Resetgeste oder Touchkalibrierungsmathematik
- systemweite Fehler-/Safety-/Aktorlogik
- produktiver NVS-Adapter, Partitionierung oder physische Loeschgarantie

## Architekturgrenzen

- Bootstrap-, Epoch-, Reset- und Konfigurations-Recoverysemantik liegt in
  `fermentation_app`.
- technische Store-, Envelope-, Slot- und Cut-Point-Bausteine bleiben in
  `device_platform`.
- Testadapter bleiben in `device_platform_test_support`.
- `src/main.cpp` bleibt Composition Root.
- es entsteht kein allgemeines Domaenen-, Reset-Plugin- oder Providerframework.

## Verbindliche Tests

### Bootstrap

- vollstaendig leerer simulierter Speicher mit erfolgreichen Reads
- `Initializing` vor erster Dokumentrevision
- Cut vor und nach jedem Write bis `Initialized`
- Wiederaufnahme aus jedem Cut ohne doppelte oder gemischte aktive Generation
- Readfehler, beschaedigte Altbytes, unbekanntes Schema oder ungueltiger Root
  verhindern automatische Initialisierung
- BootstrapSequence-/StorageEpoch-Ueberlauf wird vor Write abgelehnt

### Boot und Korruption

- normaler `Initialized`-Boot mit Active
- unbrauchbares Active mit gueltigem Fallback
- kein nutzbarer Graph: typisiertes Unavailable und keine Runtime
- rohe Bytekorruption ohne passende CRC
- semantisch ungueltiger, CRC-korrekt kodierter Record
- falsche `StorageEpoch` an jeder Referenzkante
- unbekannte Bootstrap-, Root-, Manifest- und Dokumentversion
- Lese-/Kapazitaetsfehler niemals als `NotFound`
- Korruption startet nie Reset oder Factoryinitialisierung

### Werksreset

- Cut vor und nach jedem persistenten Resetschritt
- Wiederaufnahme von `Resetting` ist idempotent
- alte Active-/Fallback-/Dokumentrecords bleiben logisch unerreichbar
- neue Anfangskonfiguration ist vollstaendig oder noch nicht aktiv, nie gemischt
- Touchkalibrierungs-Sentinel bleibt an jedem Cut und nach Abschluss unveraendert
- keine Connectivity-/Authentication-/Secretrecords werden erzeugt
- `StorageEpoch`-Ueberlauf wird fail closed abgelehnt
- Neustart nach Root-Commit und vor `Initialized` behaelt die neue Epoche
- Korruption waehrend Reset fuehrt zu sicherem Unavailable, nicht zum Neustart
  einer unkontrollierten Factoryinitialisierung

### Additiver Ausbauvertrag

- unbekannte spaetere Recordtypen oder Root-/Manifest-Schemas werden ohne
  Teilwirkung abgelehnt
- Schema-1-Variante-B-Daten bleiben deterministisch lesbar
- Copy-Migration veraendert Quelldaten nicht in place
- ein Testmodell kann spaeter einen neuen epochengebundenen Recordtyp
  hinzufuegen, ohne bestehende Schema-1-Bytes umzudeuten
- keine produktiven Dummy-Secretrecords fuer diesen Nachweis

### Ressourcen und Builds

- vollstaendige Cut-Point-Matrix fuer Bootstrap, Boot und Reset
- begrenzte Record-/Scanpuffer gemaess bestehendem Vertrag
- Base-/Head-Vergleich fuer Flash, statisches RAM, `firmware.bin` und
  `firmware.elf`
- native Tests und alle drei Buildprofile
- keine reale Heap-, NVS-, Flashatomizitaets- oder Flashlebensdauergarantie ohne
  Messung

## Akzeptanzkriterien

- fabrikneu, `Initializing`, `Initialized`, `Resetting`, korrupt und unbekannt
  sind eindeutig unterscheidbar.
- Nur nachweislich leerer, fehlerfrei lesbarer Speicher wird initialisiert.
- Jeder Cut besitzt ein konkretes fachliches Recovery-Orakel.
- Korruption erzeugt keinen stillen Factory-Fallback oder automatischen Reset.
- Werksreset ist wiederaufnehmbar und behaelt Touchkalibrierung.
- Alte Epochen werden nie reaktiviert.
- Es existieren keine leeren Connectivity-/Authentication-Manifeste,
  Secret-Roots oder vorbereiteten Credentialstrukturen.
- Tests, Buildprofile, Quality Gates und Ressourcenvergleich sind bestanden.

## Definition of Done

- Bootstrap, `StorageEpoch`, Korruptionssperre und wiederaufnehmbarer
  Werksreset implementiert
- End-to-End-Boot-/Recoverydienst fuer Variante B integriert
- #54/#55/#56 abgeschlossen
- vollstaendige Cut-Point-, Korruptions- und Ressourcenmatrix gruen
- Touchkalibrierungserhalt nachgewiesen
- Dokumentation und `CHANGELOG.md` aktualisiert
- kleiner eigener Plan-first-Draft-PR fuer #57, unabhaengig reviewed und nicht
  durch den Agenten gemergt

## Spezifikationsquellen

- zentrale akzeptierte Variante-B-ADR
- ADR-010 und ADR-016
- `docs/CONFIGURATION_PERSISTENCE.md`
- `docs/SETTINGS_AND_STORAGE.md`
- `docs/BACKUP_SECURITY_RETENTION.md`
- `docs/PR38_REVIEW_CORRECTIONS.md`
````

## Korrigierte Abhaengigkeiten und Zielstatus

### #16

- Status bleibt `TRACKING`.
- #54 und #55 werden im Body als `COMPLETED` gefuehrt.
- #56 und #57 bleiben die einzigen noch offenen direkten Teilissues.
- #16 wird erst nach #57 und der Gesamtmatrix geschlossen.

### #56

- harte Abhaengigkeiten: #54 und #55, beide abgeschlossen;
- Zielstatus nach Spezifikations-/Bodykorrektur: `READY`;
- keine Abhaengigkeit auf #17, #24 oder einen Secretkonsumenten;
- die eigentliche Implementierung beginnt trotzdem erst nach einem eigenen
  #56-Plan-first-Draft-PR und dessen commitgebundener Ownerfreigabe.

### #57

- harte Abhaengigkeit: der gemergte Variante-B-Kern aus #56;
- #54/#55 sind abgeschlossene transitive Grundlagen;
- Zielstatus bis zum Merge von #56: `BLOCKED_DEPENDENCY`;
- nach #56-Merge erfolgt ein eigener #57-Plan-first-Draft-PR.

### #17

- pauschale Abhaengigkeit auf Tracking-Issue #16 entfernen;
- korrigierte harte Abhaengigkeiten: #13, #14, #15 und #54; alle vier sind
  abgeschlossen und stellen Laufschnappschuss, Zustands-/Kommandokern und die
  generischen Persistenzbausteine bereit;
- #55 darf als bereits gemergte Konfigurationsgrundlage verwendet werden, ist
  aber kein fachlicher Blocker fuer Laufrecords und Kontrollpunkte;
- kein Bezug auf Pending, Intent, `ConfigurationActivationRunAssessment` oder
  Secret-Infrastruktur;
- Status bleibt bis zu seinem eigenen freigegebenen Implementierungsplan
  `PLANNED_SPEC_PENDING`; danach kann #17 unabhaengig von #56/#57 auf `READY`
  gesetzt werden;
- die spaetere Aufloesung eines Programmstarts gegen den aktuellen
  `RuntimeConfigurationSnapshot` ist ein schmaler Integrationspunkt zu #56,
  kein Blocker fuer das Laufrecord-/Checkpointfundament.

### #24

- bestehende fachliche Abhaengigkeiten #14, #15, #17, #20, #21 und #23 bleiben;
- keine neue pauschale Abhaengigkeit auf #16, #56 oder #57;
- #56/#57 stellen spaeter nur typisierte
  `ConfigurationRuntimeFailure`-/`ConfigurationUnavailable`-Eingaben bereit;
- #24 integriert Fehlerklasse, persistente Verriegelung, Bootprioritaet,
  `SAFE_BOOT` und Aktorsperre nach seinen eigenen Gates;
- Status bleibt `PLANNED_SPEC_PENDING`, bis seine bestehenden Abhaengigkeiten
  und sein eigener Plan-first-Schritt abgeschlossen sind;
- keine zyklische Abhaengigkeit: #56/#57 koennen typisierte Fehler produzieren,
  ohne #24 zu implementieren; #24 konsumiert sie spaeter.

## Geplanter kleiner PR-/Commit-Schnitt nach Planfreigabe

Dieser Plan autorisiert keine Produktionsimplementierung von #56 oder #57.
Nach commitgebundener Planfreigabe ist folgender kleine Schnitt vorgesehen:

### Dokumentationscommit 1: Entscheidung formalisieren

- naechste freie ADR-Nummer live pruefen;
- neue ergaenzende Variante-B-ADR in `docs/DECISIONS.md` anlegen;
- ADR-016 unveraendert lassen;
- `docs/CONFIGURATION_PERSISTENCE.md`, `docs/SETTINGS_AND_STORAGE.md` und
  `docs/BACKUP_SECURITY_RETENTION.md` auf Variante B korrigieren.

### Dokumentationscommit 2: Planung und Aufgaben synchronisieren

- `docs/IMPLEMENTATION_ISSUES.md` korrigieren;
- die bestehenden Agent-Auftraege fuer #16/#56/#57 und deren INDEX auf die
  copy-ready Issue-Bodies, Status und Reihenfolge synchronisieren;
- keine Produktionsdatei und keinen Test aendern.

Beide Commits bleiben im selben Draft-PR wie dieser Plan und werden gegen den
freigegebenen Plan-Commit nachgewiesen. Eine Zusammenfassung dokumentiert
Planpunkt, tatsaechlichen Commit und Abweichungen.

### Live-Issue-Metadaten erst nach gemergter Spezifikation

Live-Issues werden nicht auf ungemergte Spezifikationen umgestellt. Nach
unabhaengigem Review und Owner-Merge des Dokumentations-PRs folgt in einem
separaten, ausdruecklich ownerbeauftragten Metadatenschritt:

1. Live-Head und gemergte ADR/Spezifikation erneut pruefen.
2. #16 mit dem vollstaendigen Body oben aktualisieren.
3. #56 mit dem praezisierten Titel und dem vollstaendigen Body oben
   aktualisieren und auf `READY` setzen.
4. #57 mit dem korrigierten Titel und dem vollstaendigen Body oben aktualisieren und
   `BLOCKED_DEPENDENCY` belassen.
5. #17 nur hinsichtlich Abhaengigkeiten/Grenzen korrigieren;
   `PLANNED_SPEC_PENDING` bis zu seinem eigenen Plan beibehalten.
6. #24 nur hinsichtlich des schmalen Fehlervertrags praezisieren;
   `PLANNED_SPEC_PENDING` beibehalten.
7. Keine neuen Issues, Labels, Milestones oder Projektstatus ohne separate
   Ownerfreigabe erzeugen.

## Genaue Reihenfolge der spaeteren Umsetzung

Nach Spezifikationsmerge und Live-Issue-Synchronisation:

1. fuer #56 einen eigenen Branch und Draft-PR mit
   `docs/tasks/issue-56-implementation-plan.md` erstellen;
2. #56-Plan einschliesslich MutationSequence-Gleichwertigkeitsnachweis vom Owner
   commitgebunden freigeben lassen;
3. #56 in kleinen Commits umsetzen: Wirevertraege -> Graphladen/Rotation ->
   Vorschau/Konflikt -> Commit/Runtime-Publish -> Gesamtmatrix;
4. #56 unabhaengig reviewen, Ownermerge abwarten und Issue abschliessen;
5. #57 von `BLOCKED_DEPENDENCY` in den Planungsstatus ueberfuehren;
6. fuer #57 einen eigenen Branch und Draft-PR mit
   `docs/tasks/issue-57-implementation-plan.md` erstellen;
7. #57-Plan commitgebunden freigeben lassen;
8. #57 umsetzen: Bootstraprecord -> fabrikneue Initialisierung -> normaler
   Boot/Korruptionssperre -> StorageEpoch/Reset -> End-to-End-Matrix;
9. #57 unabhaengig reviewen, Ownermerge abwarten und Issue abschliessen;
10. Tracking-#16 gegen Gesamtmatrix, Ressourcen und Dokumentation abnehmen und
    erst danach ownerseitig schliessen.

#17 kann nach seinem eigenen Plan parallel zum #56/#57-Pfad auf den bereits
gemergten Persistenzfundamenten arbeiten. #24 folgt seinen eigenen
Abhaengigkeiten; die typisierten Konfigurationsfehler werden integriert, sobald
beide Seiten verfuegbar sind. Reale Connectivity-/Authentication-Domaenen
folgen jeweils erst mit ihrem ersten produktiven Konsumenten und eigenem Plan.

## Fehler-, Recovery-, Security- und Safetygrenzen

### Fehler und Recovery

- `NotFound`, Readfehler, Integritaetsfehler, unbekanntes Schema und fachlich
  ungueltiger Graph bleiben unterscheidbar.
- Kein Fehler wird als fabrikneuer Speicher umgedeutet.
- Jeder persistente Write-Cut besitzt ein konkretes altes/neues/Unavailable-
  Recovery-Orakel.
- `CommitOutcomeUnknown` wird immer per Readback aufgeloest.
- Kein automatischer Werksreset und kein automatischer Rollback nach
  bestaetigtem Root-Commit.
- Fallback ist genau eine vorherige vollstaendig nutzbare Generation, keine
  beliebige Sammlung alter Roots.

### Security

- keine Secrets, Credentialverifier, Salts, PINs oder Passwoerter in
  Konfigurationsdokumenten, Vorschauen, Logs, Diagnosen oder Backups;
- keine leere Auth-/Connectivity-Domaene in #57;
- spaetere Domaenen sind versioniert, epochengebunden und vorwaertsgerichtet,
  wo OD-09 dies fordert;
- keine Behauptung physischer Flashloeschung oder Verschluesselung;
- Plattformverschluesselung bleibt das getrennte
  `EVALUATE_BEFORE_RELEASE`-Gate.

### Safety

- kein Konfigurations-, Bootstrap- oder Resetpfad steuert GPIOs oder Aktoren;
- bei unklarem Graphen oder Publish-Vertragsverletzung keine normale
  Runtimefreigabe;
- Peltier und leistungsbezogene Aktoren bleiben bei Bootstrap-/Recoveryfehlern
  gesperrt;
- #24 entscheidet Fehlerklasse, Verriegelung, `SAFE_BOOT` und Aktorwirkung;
- Service-PIN und Resetautorisation umgehen keine Safetygrenze;
- normaler Werksreset behaelt Touchkalibrierung, ein gesonderter
  Kalibrierungs-Recoveryfall bleibt getrennt.

## Teststrategie fuer die spaeteren Implementierungsissues

### Native Vertrags- und Modelltests

- Golden-/Roundtriptests fuer neue Manifest-, Root- und Bootstraprecords;
- vollstaendige fachliche Graphvalidierung jeder Referenzkante;
- Schema-, Epoch-, Revisions-, Generation-, Laengen- und CRC-Fehler;
- unbekannte neuere Schemas und Copy-Migration;
- Konflikt- und `NoChange`-Semantik;
- Leser beobachten nie eine Teilgeneration.

### Cut-Point- und Korruptionstests

- Cut unmittelbar vor und nach jedem Storecommit;
- `CommitOutcomeUnknown` mit altem oder neuem Readback;
- fuenf aufeinanderfolgende Active-Commits;
- Bootstrap von leer bis `Initialized`;
- Werksreset von `Initialized` ueber jeden `Resetting`-Cut;
- rohe Bytekorruption und semantisch ungueltige CRC-korrekte Records;
- keine stille Factoryinitialisierung, kein automatischer Reset;
- Touchkalibrierung bleibt an allen Reset-Cuts unveraendert.

### Ressourcen- und Buildnachweis

- kontrollierte Record- und Snapshotarbeitsbereiche;
- kein fallibler Schritt nach Root-Commit;
- native Tests;
- Builds `native`, `esp32_bringup`, `esp32_release`;
- Base-/Head-Vergleich fuer Flash, statisches RAM, Firmwareartefakte und soweit
  sinnvoll Host-/Zielmesswerte;
- reale NVS-Kapazitaet, Heap, groesster Block, Replace-Atomizitaet und
  Flashlebensdauer bleiben `MEASUREMENT_REQUIRED` beziehungsweise
  `TBD_IMPLEMENTATION_BUDGET` bis zum realen Nachweis.

## Dokumentationspruefungen nach der spaeteren Umsetzung

Mindestens:

- Repositorysuche nach `Pending`, `PendingRoot`, `Aktivierungsintent`,
  `ConfigurationActivationRunAssessment`, `NotProvisioned`,
  `AuthenticationRoot`, `CredentialEpoch` und `MutationSequence`;
- jede verbleibende Nennung entweder als klarer spaeterer Ausbau oder als
  separate #19-D-/Auth-/Connectivity-Semantik klassifizieren;
- relative Markdown-Links;
- Markdown-Tabellen;
- Schweizer Schreibweise ohne scharfes S;
- `git diff --check`;
- `python3 scripts/check_secrets.py`;
- tatsaechlichen Diff gegen den freigegebenen Plan-Commit pruefen.

## Offene Entscheidungen und Gates

| Punkt | Status | Behandlung |
|---|---|---|
| separate persistente `MutationSequence` | `FINAL_SELECTION_PENDING` innerhalb des #56-Detailplans | Plan empfiehlt Wegfall nur bei dokumentiertem Gleichwertigkeitsnachweis; sonst materielle Planabweichung |
| NVS-Partitionsgroesse | `TBD_IMPLEMENTATION_BUDGET` / `MEASUREMENT_REQUIRED` | #29 und reale Ressourcenmessung |
| reale NVS-Replace- und Power-Cut-Eigenschaften | `MEASUREMENT_REQUIRED` | Hardware-/Adaptertest, keine Hostgarantie |
| Plattformverschluesselung | `EVALUATE_BEFORE_RELEASE` | getrenntes Security-Gate vor #37 |
| spaeteres Pending/Intent | `FINAL_SELECTION_PENDING` bis erster realer neustartpflichtiger Wert | neue additive Planung, keine R1-Vorwegnahme |
| Connectivity-Schema/Commit | `FINAL_SELECTION_PENDING` bis erster realer WLAN-Konsument | eigener Plan mit OD-06-/Secret-/Recoveryvertrag |
| Authentication-Schema/Commit | `FINAL_SELECTION_PENDING` bis erster Credentialkonsument | eigener Plan nach Authspike und Ownerentscheid |
| physischer PIN-unabhaengiger Recoveryausloeser | `TBD_HARDWARE`, `SPIKE_REQUIRED` | #31/#57-Integration, keine Geste in diesem Plan |
| Resetumfang fremder Lauf-/Journal-/Historiendomaenen | spaetere konsumentennahe Integration | #17/#19, kein leerer Reset-Pluginrahmen in #57 |

## Ausdruecklich verbotene Vorwegnahmen

- Plan still nach bereits geschriebenem Code umdeuten;
- Live-Issues vor gemergter ADR/Spezifikation aendern;
- #56 vor seinem eigenen freigegebenen Plan implementieren;
- #57 vor Merge von #56 implementieren;
- Pending, Intent, Previewpersistenz oder leere Secretrecords als Reserve bauen;
- eine globale Persistenz-, Domaenen-, Provider- oder Pluginplattform schaffen;
- #17 oder #24 mit einem pauschalen #16-Blocker versehen;
- Touchkalibrierung beim normalen Werksreset loeschen;
- Korruption als leeren Speicher behandeln;
- nach Root-Commit einen falliblen Publish oder automatischen Rollback planen;
- reale Connectivity-/Authentication-Daten in allgemeine Konfiguration oder
  normale Backups einbetten;
- neue Issues, ADRs oder Statusaenderungen ohne die jeweils beschriebene
  Ownerfreigabe vornehmen;
- Branch oder PR durch den Agenten mergen, auf Ready setzen, force-pushen oder
  loeschen.

## Abnahmekriterien dieses Plan-PRs

Der Planungsauftrag ist abgeschlossen, wenn:

- Live-Stand von `main`, PR #63 und #16/#17/#24/#54–#57 geprueft ist;
- nur diese Plan-Datei neu angelegt wurde;
- die noetigen Spezifikationsstellen vollstaendig benannt sind;
- die Notwendigkeit einer neuen ergaenzenden ADR begruendet ist;
- die vollstaendigen copy-ready Bodies fuer #16, #56 und #57 enthalten sind;
- #56 keine Pending-/Intent-/Secret-Vorwegnahme enthaelt;
- #57 keine leeren Connectivity-/Authentication-Strukturen enthaelt und
  Touchkalibrierung erhaelt;
- #17 und #24 schmal und zyklusfrei abgegrenzt sind;
- Test-, Cut-Point-, Recovery-, Security-, Safety- und Ressourcengates
  dokumentiert sind;
- die genaue Reihenfolge nach Planfreigabe dokumentiert ist;
- relative Links und Tabellen geprueft sind;
- Schweizer Schreibweise ohne scharfes S geprueft ist;
- `git diff --check` und `python3 scripts/check_secrets.py` bestanden sind;
- ein einzelner Plan-Commit gepusht ist;
- ein Draft-PR mit Plan-Datei, Plan-Commit-SHA, offenen Entscheidungen und
  `IMPLEMENTATION_BLOCKED_PENDING_PLAN_APPROVAL` erstellt ist;
- der Agent danach anhaelt und auf die commitgebundene Ownerfreigabe wartet.
