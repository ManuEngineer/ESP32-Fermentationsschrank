# Agent-Auftrag fuer Issue #16

## Issue

**[E2.1] Konfigurationsebenen, Validierung und atomare Revisionen**

Snapshot-Status: `TRACKING`

Epic: #4

GitHub: https://github.com/ManuEngineer/ESP32-Fermentationsschrank/issues/16

> Das Live-Issue bleibt die aktuelle Arbeitsquelle. Dieser Auftrag beschreibt
> den nach ADR-018 verbindlichen Variante-B-Schnitt und autorisiert keine
> direkte Gesamtimplementierung von #16.

## Ziel

Eine hardwareunabhaengige, nativ testbare und stromausfallsichere
Konfigurationspersistenz mit getrennten typisierten Dokumenten, einem
vollstaendig validierten Active-/Fallback-Graphen, sicherem Bootstrap und
atomarer Runtimeaktivierung bereitstellen.

## Kanonische Quellen

- ADR-018 in `docs/DECISIONS.md`
- ADR-010 und ADR-016
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
- Ein kanonischer `ConfigurationRootRecord` enthaelt Active und optional genau
  eine vorherige vollstaendig nutzbare Fallbackgeneration.
- Der Root-Commit ist der einzige persistente Linearisierungspunkt.
- Sichere Slotrotation schuetzt nur Active, Fallback und die laufende
  serialisierte Mutation.
- Vorschau bleibt fluechtig, vollstaendig typisiert und validiert.
- Bestaetigung prueft erwartete Active-Basis, unveraenderten Kandidaten und
  erneute technische sowie fachliche Validierung.
- Alle falliblen Runtimewerte und Ressourcen werden vor Root-Commit
  vorbereitet.
- Ein unveraenderlicher `RuntimeConfigurationSnapshot` wird nach Root-Commit
  nicht allokierend, nicht serialisierend und vertraglich nicht fehlschlagend
  publiziert.
- Ein `CommitOutcomeUnknown`, dessen vollstaendiger Root-/Graph-Readback nicht
  eindeutig alt oder neu bestimmen kann, fuehrt zu einem stabil typisierten
  fail-closed Zustand ohne Publish, weitere Mutation oder Slotwiederverwendung.
- Bootstrap initialisiert nur nachweislich fabrikneuen, vollstaendig
  fehlerfrei lesbaren Speicher.
- `StorageEpoch`, Korruptionssperre und wiederaufnehmbarer Werksreset sind R1.
- Der Werksreset behaelt gemaess ADR-010 die geraetespezifische
  Touchkalibrierung.

## Teilissues und Reihenfolge

### #54 – Plattformpersistenz und Wireformat

Status: `COMPLETED`, umgesetzt mit PR #59.

- `IStateStore`, NVS-faehiger Schluesselraum und starke technische Typen
- Big-Endian-Codecs, CRC-32/ISO-HDLC und Envelope
- generische begrenzte Slot-/Recordmechanik
- `SimulatedPersistentStateStore` und technische Golden-/Cut-Point-Tests

### #55 – Typisierte Konfigurationsdokumente

Status: `COMPLETED`, umgesetzt mit PR #61.

- UserConfiguration Schema 1
- ServiceConfiguration Schema 1
- ProgramCatalog Schema 1
- fachliche Limits, Codecs, Validierung und Copy-Migration
- schmaler Zeitzonenresolver mit Testadapter

### #56 – Active-/Fallback-Manifeste, Vorschau und Runtimeaktivierung

Zielstatus nach Live-Synchronisierung: `READY`.

- vollstaendig validierter Active-/Fallback-Graph
- kanonischer Root und sichere Slotrotation
- fluechtige vollstaendig validierte Vorschau
- Revisions- und Konfliktschutz
- vorbereiteter unveraenderlicher Runtime-Snapshot
- atomarer Root-Commit und vertraglich nicht fehlschlagender Publish
- typisierter Konfigurations-/Publishfehler fuer die spaetere #24-Integration

Die Umsetzung beginnt trotzdem erst nach einem eigenen Plan-first-Draft-PR und
der commitgebundenen Ownerfreigabe fuer #56.

### #57 – Bootstrap, StorageEpoch und Recovery

Status: `BLOCKED_DEPENDENCY` bis #56 gemergt ist.

- sicherer Bootstrap und gespeicherte Bootstrapzustaende
- `StorageEpoch`
- Korruptionssperre ohne stillen Factory-Fallback
- wiederaufnehmbarer Werksreset
- Erhaltung der Touchkalibrierung
- End-to-End-Cut-Point-, Korruptions- und Ressourcenmatrix fuer Variante B

```text
#54 COMPLETED + #55 COMPLETED
  -> #56 READY nach Live-Synchronisierung und eigenem Plangate
  -> #57 BLOCKED_DEPENDENCY bis #56 gemergt
  -> abschliessende Tracking-Abnahme #16
```

## Ausdruecklich nicht in Release 1 vorbereitet

- persistentes Pending oder Pending-Root
- Aktivierungsintent oder `ConfigurationActivationRunAssessment`
- persistente Preview-Slots, Preview-Owner, Preview-Tokens oder Ablaufzeiten
- leere Connectivity-/Authentication-Manifeste oder Secret-Roots
- vorbereitete Credentialslots oder Authentication-Roots
- `CredentialEpoch` ohne reale Credentials
- kombinierte Konfigurations-/Secret-Transaktionen

Diese Funktionen werden erst mit einem realen neustartpflichtigen Wert,
WLAN-Secret, Webpasswort oder Service-PIN-Konsumenten additiv ueber neue
Recordtypen, Schema-/Rootversionen und Copy-Migrationen geplant. Es entstehen
keine Dummyrecords, Slots, Keys oder Zukunftsports.

## MutationSequence-Gate

Eine eigene persistente `MutationSequence` ist noch nicht entschieden. Der
Detailplan von #56 darf sie nur weglassen, wenn er fuer Dokumentrevisionen,
Manifestgeneration, `rootSequence`, erwartete Basis, Kandidatenintegritaet,
Commit-Readback und Wiederholung eine gleichwertige testbare Semantik
nachweist. Andernfalls ist dies eine materielle Planaenderung mit neuer
Ownerfreigabe. Status: `FINAL_SELECTION_PENDING`.

## Grenze zu #17

#17 verwendet die gemergten anwendungsneutralen Persistenzbausteine und seine
eigenen typisierten Laufrecords. Es haengt nicht pauschal von #16, #56 oder
#57 ab. Harte Grundlagen sind #13, #14, #15 und #54; #55 darf verwendet werden,
ist aber kein fachlicher Blocker.

Der aktive Lauf bleibt ausserhalb der Konfiguration. Der synchrone Import-
Run-Gate mit `Unknown`, `NoActiveOrRecoverableRun`, `ActiveRunPresent` und
`RecoverableRunPresent` gehoert spaeter zu #19-D und wird nicht in #56
vorbereitet.

## Grenze zu #24

#56/#57 liefern bei Konfigurations-, Integritaets- oder
Publishvertragsfehlern sowie einem unbestimmten Root-Commitzustand nur stabile
typisierte Fehlerdaten und keine Runtimefreigabe. Systemweite Fehlerklasse,
persistente Verriegelung, `SAFE_BOOT`, Fehlerresetpolitik und reale
Aktor-/GPIO-Sperren bleiben #24. #24 erhaelt keine neue pauschale oder zyklische
Abhaengigkeit auf #16, #56 oder #57.

Das nachgelagerte `CONFIGURATION_SAFETY_INTEGRATION_GATE` ist ein verbindliches
Abschlusskriterium: #56 produziert `ConfigurationRuntimeFailure` und den
unbestimmten Commitzustand; #57 produziert `ConfigurationUnavailable` und
`ConfigurationIntegrityFailure`; #24 muss diese realen Eingaben auf
persistente Verriegelung, sichere Bootprioritaet beziehungsweise `SAFE_BOOT`,
keine normale Aktorfreigabe und reproduzierbare Fehlerinjektion abbilden.

#56/#57 duerfen ihre Producer-Vertraege und #24 darf seinen Fehlerkern
unabhaengig umsetzen. Wird der #24-Core zuerst gemergt, bleibt #24 jedoch offen,
bis die Gate-Integration einschliesslich Neustart-, Verriegelungs- und
Recoverytests bestanden ist. Reale Aktoradapter duerfen das Gate nicht
umgehen.

## Ausdruecklicher Nicht-Scope

- Laufpersistenz und Kontrollpunkte aus #17
- Journal, Historie, Backup und Import aus #19
- systemweite Fehlerklassen und Aktorsperren aus #24
- reale Connectivity-, Authentication- und Webvertraege
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
- `CommitOutcomeUnknown` mit altem, neuem und nicht aufloesbarem Readback sowie
  Neustart in eindeutigem und weiterhin unklarem Zustand getestet ist;
- der unbestimmte Zustand Publish, weitere Mutation und Slotrotation sperrt;
- das `CONFIGURATION_SAFETY_INTEGRATION_GATE` als verpflichtender Abschluss von
  #24 mit allen realen Producer-Vertraegen dokumentiert und getestet ist;
- Touchkalibrierung beim Werksreset erhalten bleibt;
- unbekannte neuere Schemas ohne Teilwirkung abgelehnt werden und der additive
  Ausbaupfad ohne Dummyinfrastruktur nachgewiesen ist;
- native Tests, Buildprofile, Quality Gates und Ressourcenvergleiche der
  Teilissues bestanden sind;
- reale NVS-/Partitions-/Flashmessungen als spaetere Gates sichtbar bleiben.

## Agentenregel

Vor jeder Arbeit Live-Issues, Abhaengigkeiten, `AGENTS.md`, Modulregeln und
kanonische Quellen lesen. Pro Teilissue gilt ein eigener Plan-first-Draft-PR.
Ohne exakte commitgebundene Planfreigabe keine Implementierung. PR weder auf
Ready setzen noch mergen, Auto-Merge aktivieren, force-pushen oder Branch
loeschen.
