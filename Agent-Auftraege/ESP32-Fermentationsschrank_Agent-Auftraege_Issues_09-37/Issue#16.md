# Agent-Auftrag fuer Issue #16

## Issue

**[E2.1] Konfigurationsebenen, Validierung und atomare Revisionen**

Aktueller Snapshot-Status: `TRACKING`

Epic: #4

GitHub: https://github.com/ManuEngineer/ESP32-Fermentationsschrank/issues/16

> Der Status und Inhalt auf GitHub sind die aktuelle Wahrheit. Lies das
> Live-Issue vor jeder Arbeit erneut. Dieser Auftrag ist eine Arbeitsanweisung,
> kein Ersatz fuer das Issue oder die Spezifikationsquellen.

## Aktueller Stand

Die fachliche und technische Spezifikation ist in
`docs/CONFIGURATION_PERSISTENCE.md` konsolidiert. Der Vertrag ist in vier
strikt abhaengige Teilissues zerlegt:

1. #54 – Plattformpersistenz und Wireformat
2. #55 – typisierte Konfigurationsdokumente
3. #56 – Manifeste, Preview und Runtimeaktivierung
4. #57 – Bootstrap, Secret-Manifeste und Recovery

Issue #16 bleibt als Tracking-Issue offen und wird nicht direkt implementiert.

Verbindliche Reihenfolge:

`#54 -> #55 -> #56 -> #57`

Nur das jeweils erste technisch ausfuehrbare Teilissue darf `READY` sein.
Aktuell ist ausschliesslich #54 `READY`; #55 bis #57 sind
`BLOCKED_DEPENDENCY`.

## Verbindliche Teilissues

### #54 – [E2.1a] Plattformpersistenz und Wireformat implementieren

Status: `READY`

Agent-Auftrag: [Issue#54.md](Issue#54.md)

Scope:

- begrenztes binaersicheres `IStateStore`
- starke technische Typen
- Big-Endian-Codecs, CRC-32/ISO-HDLC und Envelope-Version 1
- generische feste Revisions- und redundante Recordslots
- `ISecureRandomSource` und `ITimeZoneResolver`
- `SimulatedPersistentStateStore` und Golden-/Cut-Point-Tests

### #55 – [E2.1b] Typisierte Konfigurationsdokumente implementieren

Status: `BLOCKED_DEPENDENCY`

Abhaengigkeit: #54

Agent-Auftrag: [Issue#55.md](Issue#55.md)

Scope:

- UserConfiguration Schema 1
- ServiceConfiguration Schema 1
- ProgramCatalog Schema 1
- ID-, Text-, Anzahl- und Payloadgrenzen
- fachliche Codecs, Validierung und Copy-Migration

### #56 – [E2.1c] Konfigurationsmanifeste, Preview und Runtimeaktivierung implementieren

Status: `BLOCKED_DEPENDENCY`

Abhaengigkeiten: #54 und #55

Agent-Auftrag: [Issue#56.md](Issue#56.md)

Scope:

- Active/Fallback/Pending
- kanonische Rootauswahl
- korrigierte Schutzwurzeln und Slotrotation
- Preview, Owner, Token und Konfliktsemantik
- Aktivierungsintent
- RuntimeConfigurationSnapshot und Prepare/Commit/Publish
- RunAssessment-Port zu #17
- ConfigurationSafetyIntent zu #24

### #57 – [E2.1d] Bootstrap, Secret-Manifeste und Recovery integrieren

Status: `BLOCKED_DEPENDENCY`

Abhaengigkeiten: #54, #55 und #56

Agent-Auftrag: [Issue#57.md](Issue#57.md)

Scope:

- Bootstrap und StorageEpoch
- Connectivity-/Authentication-Manifeste Schema 1 als `NotProvisioned`
- vorwaertsgerichtete Authentication-Roots
- wiederaufnehmbarer Werksreset
- vollstaendige Cut-Point-, Korruptions-, Slotrotations- und Ressourcenmatrix

## Verbindliche Architektur

- `FactoryConfiguration` bleibt unveraenderlich in der Firmware.
- UserConfiguration, ServiceConfiguration und ProgramCatalog bleiben getrennte,
  typisierte, schema-versionierte Dokumente.
- Dokumente werden ueber vollstaendig validierte Manifeste gemeinsam aktiviert.
- Nur geaenderte Dokumente erhalten neue Revisionen.
- Unveraenderte Dokumentrevisionen duerfen gemeinsam referenziert werden.
- Der aktive Lauf bleibt ausserhalb der Konfiguration und behaelt seinen
  unveraenderlichen Laufschnappschuss.
- anwendungsneutrale Wire-, Envelope-, Slot-, Speicher-, Zeit-, Zufalls- und
  Testbausteine liegen in `device_platform` beziehungsweise
  `device_platform_test_support`.
- konkrete Dokumente, Graphvalidierung, Aktivierung, Bootstrap und Recovery
  liegen in `fermentation_app`.
- `src/main.cpp` bleibt reine Composition Root.

## Kanonische Schutzwurzelregel

Nicht jeder physisch gueltige alte Root schuetzt dauerhaft seine nur dort
referenzierten Generationen.

Die Schutzmenge besteht ausschliesslich aus:

- Active und Fallback des kanonischen ConfigurationRoot
- Pending des kanonischen PendingRoot
- exakt passendem gueltigem Aktivierungsintent samt Pending-Graph
- gerade ausgefuehrter serialisierter Mutation
- kanonischem committed AuthenticationRoot und laufender
  Authentication-Transaktion

Aeltere redundante Roots bleiben technische Bootkandidaten, aber keine
dauerhaft schuetzenden fachlichen Wurzeln. Nur noch von ihnen referenzierte
Slots duerfen nach erfolgreichem neuem Root-Commit wiederverwendet werden.

Verbindliche Gesamttests:

- mindestens fuenf Active-Commits
- mindestens drei Pending-Ersetzungen
- Pending verwerfen und erneut erzeugen
- wiederholte Authentication-Rootwechsel
- kein vorzeitiges `NoUnreferencedSlotAvailable`

## Grenze zu Issue #17

#56 definiert und konsumiert nur einen schmalen externen
`ConfigurationActivationRunAssessment`-Port mit mindestens:

- `Unknown`
- `NoActiveOrRecoverableRun`
- `ActiveRunPresent`
- `RecoverableRunPresent`

Nur `NoActiveOrRecoverableRun` erlaubt eine Pending-Aktivierung. `Unknown`
blockiert sicher. Die reale Laufpersistenz und Recoverable-Run-Erkennung bleiben
#17.

## Grenze zu Issue #24

#56 darf bei einem Konfigurations- oder Publish-Vertragsfehler nur einen
typisierten `ConfigurationSafetyIntent` beziehungsweise
`ConfigurationRuntimeFailure` erzeugen und keine normale RuntimeConfiguration
freigeben.

Nicht vorwegnehmen:

- systemweite Fehlerklassen
- persistente Verriegelungen
- vollstaendige SAFE_BOOT-Politik
- reale Aktor- oder GPIO-Sperren

Diese Semantik bleibt #24.

## Ausdruecklicher Nicht-Scope des Trackings

- Laufpersistenz und Kontrollpunkte aus #17
- portable Backups, Journale und Aufbewahrung aus #19
- systemweite Fehler- und Aktorpolitik aus #24
- reale Secrets, Netzwerk und Authentifizierung aus #27
- noch nicht definierte Display-, Ton-, Sensor-, Regel-, Sicherheits- und
  Hardwarefelder
- reale GPIO-/Aktorlogik
- physische Recovery-Geste
- unbewiesene reale Flash-, Heap- oder Lebensdauergarantie

## Agentenregel fuer die Reihenfolge

Vor Auswahl eines Teilissues:

1. Live-Issue #16 und #54 bis #57 lesen.
2. INDEX mit GitHub synchronisieren.
3. niedrigstes technisch ausfuehrbares Teilissue waehlen.
4. nur arbeiten, wenn dessen Live-Status `READY` ist und alle Abhaengigkeiten
   geschlossen sind.
5. eigener Branch, eigener kleiner PR, genau ein Teilissue.
6. PR mit `Closes #<Teilissue>` erstellen.
7. nicht mergen, kein Auto-Merge, Branch nicht loeschen.
8. nach PR-Erstellung anhalten.

Nach Merge eines Teilissues:

- abgeschlossenes Teilissue im INDEX auf `COMPLETED` setzen
- genau das unmittelbar folgende Teilissue auf `READY` setzen
- alle spaeteren Teilissues blockiert lassen
- Issue #16 offen und `TRACKING` lassen

## Spezifikationsquellen

- `docs/CONFIGURATION_PERSISTENCE.md`
- `docs/SETTINGS_AND_STORAGE.md`
- `docs/BACKUP_SECURITY_RETENTION.md`
- `docs/PR38_REVIEW_CORRECTIONS.md`

## Definition of Done fuer das Tracking-Issue

Issue #16 wird erst abgeschlossen, wenn:

- #54, #55, #56 und #57 gemergt und abgeschlossen sind
- die vollstaendige End-to-End-Cut-Point-Matrix besteht
- die korrigierte Slotrotation und Schutzwurzeldefinition nachgewiesen ist
- Produktionsprofile und Quality Gates bestehen
- Ressourcenwirkungen je Teil-PR und abschliessend dokumentiert sind
- reale Hardware-/Adaptermessungen weiterhin als spaetere Gates sichtbar sind

## Naechster ausfuehrbarer Branch

`feat/issue-54-platformpersistenz-und-wireformat`
