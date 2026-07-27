# Agent-Auftrag fuer Issue #56

## Issue

**[E2.1c] Active-/Fallback-Manifeste, Vorschau und Runtimeaktivierung implementieren**

Zielstatus nach Live-Synchronisierung: `READY`

Tracking-Issue: #16

Epic: #4

GitHub: https://github.com/ManuEngineer/ESP32-Fermentationsschrank/issues/56

## Abhaengigkeiten

- #54 – `COMPLETED`
- #55 – `COMPLETED`

#17 und #24 sind keine blockierenden Abhaengigkeiten. Ihre fachliche Semantik
wird nicht vorweggenommen.

## Ausfuehrungsgate

Der Zielstatus `READY` autorisiert noch keine Implementierung. Vor Code muss
fuer #56 ein eigener Draft-PR mit versionierter Plan-Datei erstellt, committed
und gepusht werden. Die Umsetzung beginnt erst nach:

```text
PLAN APPROVED
Approved plan commit: <commit-sha>
```

Der #56-Plan muss insbesondere den Gleichwertigkeitsnachweis zur offenen
`MutationSequence`-Frage enthalten. Kann er diesen nicht erbringen, ist vor
Implementierung eine materielle Planergaenzung und neue Ownerfreigabe noetig.

## Verbindliche Architekturentscheidung

Release 1 folgt ADR-018 mit schlanker Variante-B-Persistenz. ADR-016 bleibt
fuer NVS, Schluesselraum und Envelope verbindlich.

## Ziel

Die vorhandenen typisierten Dokumente als vollstaendig validierten
Active-/Fallback-Graphen atomar aktivieren. Eine fluechtige Vorschau und ein
optimistischer Konfliktvertrag fuehren zu einem vorbereiteten
`RuntimeConfigurationSnapshot`; der Root-Commit linearisiert persistent und
der anschliessende Publish ist vertraglich nicht fehlschlagend.

## Verbindlicher Scope

### Manifest und Root

- drei `ConfigurationManifest`-Slots und zwei redundante
  `ConfigurationRootRecord`-Slots
- ein Manifest referenziert exakt je eine `UserConfiguration`,
  `ServiceConfiguration` und `ProgramCatalog`-Revision
- jede Referenz bindet Recordtyp, Slot, Revision, Schema-Version,
  Payloadlaenge, CRC und `StorageEpoch`
- der Root bindet Active, optional genau eine Fallbackgeneration und eine
  monotone `rootSequence`
- Active und Fallback sind vollstaendige Graphen
- keine Connectivity-/Authentication-Referenz in Schema 1

### Kanonische Graphvalidierung

1. Rootkandidaten technisch pruefen und nach `rootSequence` absteigend liefern;
2. je Kandidat den Active-Zweig vollstaendig technisch und fachlich validieren;
3. bei unbrauchbarem Active den Fallback desselben Roots vollvalidieren;
4. den ersten vollstaendig nutzbaren Zweig kanonisch waehlen;
5. Fallbacknutzung stabil diagnostizieren;
6. ohne vollstaendig nutzbaren Graphen keine Runtime freigeben.

Ein hoher Sequenzwert, gueltiger CRC oder einzeln gueltiges Dokument aktiviert
niemals allein eine Generation.

### Sichere Slotrotation und Schutzmenge

Geschuetzt sind ausschliesslich:

- Active des kanonischen Roots;
- dessen genau eine vollstaendig nutzbare Fallbackgeneration, falls vorhanden;
- alle Records der gerade ausgefuehrten serialisierten Mutation.

Aeltere redundante Rootkopien bleiben technische Bootkandidaten, schuetzen
aber keine nur noch von ihnen referenzierten Generationen. Ein Slot wird erst
wiederverwendet, wenn er ausserhalb der kanonischen Schutzmenge liegt. Fehlt
ein sicherer Slot, wird vor jeder Teilaktivierung typisiert mit
`NoUnreferencedSlotAvailable` und betroffenem Recordtyp abgelehnt.

Ein neuer Commit schreibt und validiert zuerst geaenderte Dokumente und das
neue Manifest. Danach schreibt er in den nicht kanonischen Rootslot:

```text
Active   = neues Manifest
Fallback = bisheriges Active
```

Erst nach Readback und Vollvalidierung des neuen Rootgraphen wird dieser
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
- `NoChange` erzeugt keine neue Revision, Generation, Rootsequenz oder
  Storeoperation

Nicht Bestandteil sind persistente Preview-Slots, Owner-/Token-Metadaten oder
Ablaufzeiten. Es entsteht keine allgemeine Preview-, Provider- oder
Pluginplattform.

### Serialisierte Mutation und Revisionsschutz

- global hoechstens eine Konfigurationsmutation gleichzeitig
- Dokumentrevisionen ordnen Inhalte je Dokumenttyp
- Manifestgeneration ordnet vollstaendige Kandidatengraphen
- `rootSequence` ordnet erfolgreiche kanonische Commits
- erwartete Active-Generation und Kandidatenintegritaet sichern Konflikte
- `CommitOutcomeUnknown` wird durch Readback der Rootslots und Vollvalidierung
  des erwarteten Graphen aufgeloest
- Zaehlerwert 0 bleibt reserviert; Ueberlauf wird vor Writes abgelehnt

Eine eigene persistente `MutationSequence` wird nur dann nicht eingefuehrt,
wenn der freigegebene #56-Plan die Gleichwertigkeit dieser Mechanismen fuer
Konflikt, Wiederholung und eindeutiges Commit-Ergebnis nachweist.

### RuntimeConfigurationSnapshot und atomare Aktivierung

Vor dem Root-Commit werden Kandidat, Plattformwerte, Ressourcen und
Recordgroessen vollstaendig geprueft beziehungsweise vorbereitet. Der Ablauf:

1. Kandidat und Basis unter exklusiver Mutation erneut pruefen;
2. alle Runtimewerte und Ressourcen vorbereiten;
3. geaenderte Dokumente schreiben, ruecklesen und validieren;
4. Manifest schreiben, ruecklesen und als Graph validieren;
5. neuen Root schreiben;
6. `CommitOutcomeUnknown` durch Readback bestimmen;
7. Root und gesamten Zielgraphen ruecklesen und validieren;
8. erfolgreichen Root-Commit als persistenten Linearisierungspunkt behandeln;
9. vorbereiteten Snapshot ohne Allokation, Serialisierung, Validierung oder
   Reservierung atomar sichtbar machen;
10. Mutation freigeben.

Leser sehen nur den vollstaendig alten oder neuen Snapshot. Fehler vor
Root-Commit lassen kanonischen Graph und Runtime unveraendert. Stromausfall vor
Root-Commit laedt den alten, nach Root-Commit den neuen Graphen.

Publish nach bestaetigtem Root-Commit ist vertraglich nicht fehlschlagend. Eine
unerwartete Vertragsverletzung erzeugt nur einen stabil typisierten
`ConfigurationRuntimeFailure`, gibt keine weitere normale Konfigurationsruntime
frei und fuehrt nicht zu automatischem Rollback.

### Grenze zu #24

#56 definiert nur den typisierten Fehler und den fail-closed Zustand des
Konfigurationsdienstes. Systemweite Fehlerklasse, persistente Verriegelung,
`SAFE_BOOT`, Fehlerreset und reale Aktor-/GPIO-Sperren bleiben #24. Die spaetere
#24-Integration konsumiert den Fehler ohne zyklische Implementierungsabhaengigkeit.

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
- fachlich ungueltiges Active mit gueltigem Fallback
- ungueltiges Active und ungueltiger/fehlender Fallback: keine Runtime
- mehrere Rootkandidaten; erster vollstaendig nutzbarer Graph gewinnt
- hoehere Sequenz ohne gueltigen Graph aktiviert nichts
- Abweichungen bei Typ, Slot, Revision, Generation, Schema, Laenge, CRC und
  `StorageEpoch`
- unbekannte neuere Root-/Manifest-Schemas ohne Teilwirkung

### Rotation und Schutz

- Bootstrapgraph ohne Fallback als Ausgang
- mindestens fuenf aufeinanderfolgende Active-Commits
- nach jedem Commit Active neu und Fallback exakt vorheriges Active
- Altroots schuetzen keine ausschliesslich referenzierten Generationen
- echter Slotmangel liefert typisierten Fehler ohne Rootwrite
- unveraenderte Dokumentrevisionen koennen sicher gemeinsam referenziert werden

### Vorschau und Konflikte

- gueltiger Kandidat und bestaetigte unveraenderte Basis
- `NoChange` ohne Revision, Generation, Rootsequenz oder Write
- veraltete Active-Generation
- veraenderter Kandidat oder Fingerprint
- erneuter Validierungsfehler unmittelbar vor Commit
- konkurrierende Display-/Webmutation: genau eine gewinnt
- Neustart verwirft jede fluechtige Vorschau

### Commit und Runtime-Publish

- Fehler vor und nach jedem Dokument-, Manifest- und Rootwrite
- `CommitOutcomeUnknown` fuer jeden Write mit Readback-Orakel
- Ressourcen-, Kapazitaets- und Ueberlauffehler vor Root-Commit
- alle falliblen Arbeiten vor Root-Commit nachweisbar abgeschlossen
- Publish nach Root-Commit ohne Allokation, Serialisierung oder Validierung
- Leser beobachten nie Teilgenerationen
- simulierte Publish-Vertragsverletzung erzeugt nur typisierten Fehler
- kein automatischer Rollback nach bestaetigtem Root-Commit

### Ressourcen und Builds

- vertraglich begrenzter vollstaendiger Recordarbeitsbereich
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
- Grenzen zu #17 und #24 bleiben eingehalten.
- Tests, Buildprofile, Quality Gates und Ressourcenvergleich sind bestanden.

## Git- und PR-Regeln

Nach commitgebundener Planfreigabe ausschliesslich #56 bearbeiten. Kleiner
Draft-PR, Dokumentation und `CHANGELOG.md` aktualisieren, `Closes #56` erst im
Umsetzungs-PR. Nicht selbst auf Ready setzen, mergen, Auto-Merge aktivieren,
force-pushen oder Branch loeschen.

## Vorgeschlagener Branch

`feat/issue-56-active-fallback-manifeste-vorschau-runtimeaktivierung`
