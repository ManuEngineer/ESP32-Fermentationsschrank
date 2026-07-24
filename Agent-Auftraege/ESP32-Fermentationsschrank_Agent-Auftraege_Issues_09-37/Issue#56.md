# Agent-Auftrag fuer Issue #56

## Issue

**[E2.1c] Konfigurationsmanifeste, Preview und Runtimeaktivierung implementieren**

Aktueller Snapshot-Status: `BLOCKED_DEPENDENCY`

Tracking-Issue: #16

Epic: #4

GitHub: https://github.com/ManuEngineer/ESP32-Fermentationsschrank/issues/56

> Dieser Auftrag darf erst ausgefuehrt werden, wenn #54 und #55 gemergt und
> abgeschlossen sind und das Live-Issue #56 auf `READY` steht.

## Sperrregel

Solange eine Abhaengigkeit offen oder #56 nicht `READY` ist:

- keinen Implementierungsbranch erstellen
- keine Produktionsdatei aendern
- keinen PR erstellen
- keine provisorischen Plattform- oder Dokumentmodelle parallel einfuehren
- Blocker berichten und anhalten

## Ziel nach Freigabe

Verbinde die typisierten Dokumentrevisionen zu vollstaendig validierten
Konfigurationsgenerationen und implementiere:

- Active/Fallback/Pending
- kanonische Roots und korrigierte Schutzwurzeln
- Slotrotation
- Preview, Owner, Token und Konfliktsemantik
- Aktivierungsintent
- RuntimeConfigurationSnapshot
- Prepare/Root-Commit/Publish
- schmale Ports zu #17 und #24

Bootstrap, Secret-Manifeste, Werksreset und die abschliessende End-to-End-Matrix
bleiben #57.

## Vor jeder Arbeit lesen

- Live-Issue #56
- Tracking-Issue #16
- gemergte Ergebnisse von #54 und #55
- `AGENTS.md`
- Modul-AGENTS.md fuer `device_platform`, `device_platform_test_support` und
  `fermentation_app`
- `docs/CONFIGURATION_PERSISTENCE.md`
- `docs/SETTINGS_AND_STORAGE.md`
- `docs/BACKUP_SECURITY_RETENTION.md`
- `docs/PR38_REVIEW_CORRECTIONS.md`
- `docs/RUN_COMMANDS.md`
- `docs/CI_AND_QUALITY_GATES.md`
- Agent-INDEX

Berichte vor Codeaenderungen:

1. Manifest-, Root- und Referenzmodelle
2. kanonische Rootauswahl
3. Schutzmengen- und Slotwahlalgorithmus
4. Preview-/Bestaetigungs- und Konfliktmodell
5. Pending-/Intent-Ablauf
6. Prepare/Commit/Publish-Linearisierung
7. Integrationsports zu #17/#24
8. Cut-Point- und Ressourcenplan

Nur bei echter fachlicher Ownerentscheidung, Sicherheitswiderspruch oder
widerspruechlicher Spezifikation anhalten.

## Git- und PR-Ablauf nach Freigabe

1. aktuellen `main` und Abschluss von #54/#55 pruefen
2. Branch erstellen:
   `feat/issue-56-konfigurationsmanifeste-preview-runtimeaktivierung`
3. INDEX zuerst gegen Live-Status pruefen; nur bei Abweichung synchronisieren
4. ausschliesslich #56 bearbeiten
5. PR mit `Closes #56` erstellen
6. nicht mergen, kein Auto-Merge, Branch nicht loeschen
7. danach anhalten

## Verbindlicher Scope

### Physische Slots und Referenzen

- je 4 Slots fuer UserConfiguration, ServiceConfiguration und ProgramCatalog
- 3 ConfigurationManifest-Slots
- 2 ConfigurationRootRecord-Slots
- 2 PendingConfigurationManifest-Slots
- 2 PendingRootRecord-Slots
- 2 Aktivierungsintent-Slots
- konkrete feste Anwendungsschluessel und Record-Type-IDs in `fermentation_app`
- Referenzen mit Typ, Slot, Revision, Schema, erwarteter Payloadlaenge und CRC
- exakte Uebereinstimmung mit Envelope und Payload

### Kanonischer ConfigurationRoot

Beim Boot:

1. technische Rootkandidaten nach Sequenz absteigend beziehen
2. Active-Graph des Kandidaten vollstaendig validieren
3. bei ungueltigem Active Fallback vollstaendig validieren
4. ersten nutzbaren Zweig als kanonisch waehlen
5. verwendeten Fallback stabil diagnostizieren

Manifestvorhandensein oder hohe Sequenz allein aktiviert nichts.

### Korrigierte Schutzwurzeln

Dauerhaft geschuetzt sind ausschliesslich:

- Active und Fallback des kanonischen ConfigurationRoot
- Pending des kanonischen PendingRoot
- exakt passender gueltiger Aktivierungsintent samt Pending-Graph
- gerade ausgefuehrte serialisierte Mutation

Aeltere redundante Roots sind technische Bootkandidaten, aber keine dauerhaft
schuetzenden fachlichen Wurzeln. Nur Slots ausserhalb der aktuellen Schutzmenge
duerfen wiederverwendet werden. Slotwahl erfolgt per Referenzanalyse.

Fehlt ein sicherer Slot:

- stabiler typisierter `NoUnreferencedSlotAvailable`
- betroffener Dokument-/Recordtyp enthalten
- keine Teilwirkung

### Commit und MutationSequence

- exklusive serialisierte Mutation
- nur geaenderte Dokumente neu schreiben
- unveraenderte Revisionen gemeinsam referenzieren
- Inhaltsgleichheit nie nur anhand CRC annehmen
- MutationSequence je StorageEpoch erst nach Basis-, Validierungs-,
  Aktivierungsvorbereitungs- und No-op-Pruefungen reservieren
- reservierte Sequenzen nie wiederverwenden; Luecken erlaubt
- Revisionen/Generationen/Sequenzen als getrennte starke Typen

### Preview und Konflikte

- unveraenderlicher vollstaendig validierter Kandidat
- exakte Basisgeneration
- Owner/Quelle
- Token und Spezifikations-Ablaufsemantik
- Bestaetigung nur fuer denselben Kandidaten und dieselbe Basis
- NoChange ohne Revision, MutationSequence oder Write
- veraltete Basis, falscher Owner/Token oder veraenderter Kandidat typisiert und
  ohne Teilwirkung ablehnen
- Ergebnisse mindestens: NoChange, Activated, StoredAsPending,
  ReplacedPending, ValidationFailure, PersistenceFailure,
  ConfigurationConflictFailure, ActivationFailure und MigrationFailure

### Pending und Aktivierungsintent

- hoechstens ein vollstaendig validiertes Pending
- weitere dauerhafte Aenderungen bauen auf Pending auf
- gemischte sofort-/neustartwirksame Aenderung wird als Ganzes Pending
- Pending-Ersetzung und Verwerfen atomar ueber PendingRoot
- Intent gebunden an erwartete aktive Generation, exakte Pending-Generation,
  Manifestintegritaet, monotone Intent-Sequenz und Status
- bei gueltigem Intent Pending und referenzierte Dokumente sperren
- unerwarteter Neustart ohne passenden Intent aktiviert nie
- bereits aktives Pending-Ziel nur idempotent abschliessen

### Port zu #17

Definiere und konsumiere ausschliesslich:

- Unknown
- NoActiveOrRecoverableRun
- ActiveRunPresent
- RecoverableRunPresent

Nur NoActiveOrRecoverableRun erlaubt Aktivierung. Unknown blockiert sicher.
Keine Laufpersistenz oder reale Run-Erkennung implementieren.

### RuntimeConfigurationSnapshot und Publish

- FactoryConfiguration und aktive Dokumentgeneration zusammenfuehren
- Zeitzone und alle falliblen Ressourcen vor Root-Commit vorbereiten
- unveraenderlichen RuntimeConfigurationSnapshot erzeugen
- neuer gueltiger Root mit hoeherer rootSequence ist einziger persistenter
  Linearisierungspunkt
- danach Publish innerhalb derselben Mutation nicht allokierend,
  nicht serialisierend und vertraglich nicht fehlschlagend
- Leser sehen nur vollstaendig alt oder neu
- Fehler vor Root-Commit lassen persistenten Graph und Runtime unveraendert

### Port zu #24

Bei unerwarteter Publish-Vertragsverletzung nach Root-Commit:

- kein automatischer Rollback
- typisierten ConfigurationSafetyIntent beziehungsweise
  ConfigurationRuntimeFailure erzeugen
- keine weitere normale RuntimeConfiguration freigeben

Keine systemweite Fehlerklasse, Verriegelung, SAFE_BOOT-Policy oder Aktorsperre
implementieren.

## Ausdruecklicher Nicht-Scope

- Bootstrap und automatische Ersteinrichtung
- Connectivity-/Authentication-Manifeste und Roots
- reale Secrets oder Authentifizierung
- StorageEpoch-Werksreset
- abschliessende Gesamtmatrix aus #57
- Laufpersistenz aus #17
- Backup aus #19
- systemweite Fehlerpolitik aus #24
- UI-/Webtransport aus #25–#27
- reale Hardware-, Flash- oder Heapgarantien

## Architekturgrenzen

- fachliche Bedeutung in `fermentation_app`
- `device_platform` kennt nur technische Slots, Records und Kandidaten
- Test-Support nur anwendungsneutrale Adapter; fachliche Orakel in App-Tests
- `src/main.cpp` bleibt Composition Root
- aktiver Lauf bleibt ausserhalb der Konfiguration

## Verbindliche Tests

### Root/Graph

- Active gueltig
- Active ungueltig, Fallback gueltig
- beide ungueltig
- mehrere Rootkandidaten
- hoher Root ohne gueltigen Graph
- jede Referenzkante mit Typ-, Slot-, Revision-, Schema-, Laengen-, CRC- und
  StorageEpoch-Abweichung

### Slotrotation

- mindestens 5 aufeinanderfolgende Active-Commits
- nach jedem Commit korrekter Active/Fallback-Graph
- Altroots blockieren keine nur noch von ihnen referenzierten Slots
- kein vorzeitiges NoUnreferencedSlotAvailable
- echter Slotmangel stabil ohne Teilwirkung

### Pending/Intent

- mindestens 3 Pending-Ersetzungen
- Pending verwerfen und erneut erzeugen
- weitere Aenderung baut auf Pending auf
- Intent sperrt Ersetzung
- Neustart ohne Intent aktiviert nicht
- passender Intent aktiviert genau einmal
- bereits aktives Ziel idempotent abschliessen
- alle vier RunAssessment-Werte

### Preview/Konflikte

- NoChange ohne Write/Sequenz
- veraltete Basis
- falscher Owner
- falscher Token
- Kandidat nach Preview veraendert
- MutationSequence erst nach allen Vorpruefungen
- reservierte Sequenz nach Cut nicht wiederverwenden

### Cut-Points und Publish

- Stromausfall vor und nach jedem Write/Commit
- konkretes Recovery-Orakel je Cut
- Ressourcenfehler vor Root-Commit ohne Teilaktivierung
- Root-Commit als Linearisierungspunkt
- keine Allokation/Serialisierung nach Root-Commit
- simulierte Publish-Vertragsverletzung erzeugt nur den schmalen #24-Intent
- Leser sehen keine Teilgeneration

## Qualitaets- und Ressourcenpruefung

- native Tests und alle drei Buildprofile
- alle Quality-Gate-Skripte
- clang-format und clang-tidy LLVM 18
- `git diff --check`
- Base-/Head-Ressourcenvergleich
- waehrend Commit hoechstens ein vollstaendig kodierter Recordpuffer
- keine reale Heap-/Flashgarantie behaupten

## Definition of Done

- #54 und #55 gemergt und abgeschlossen
- kompletter Scope von #56 umgesetzt
- Root-, Schutzmengen-, Slotrotations-, Pending-, Preview-, Cut-Point- und
  Publish-Tests gruen
- Grenzen zu #17 und #24 eingehalten
- kein Scope von #57 vorweggenommen
- Dokumentation und CHANGELOG aktualisiert
- PR mit `Closes #56` erstellt
- nicht gemergt und Branch nicht geloescht

## Vorgeschlagener Branch

`feat/issue-56-konfigurationsmanifeste-preview-runtimeaktivierung`
