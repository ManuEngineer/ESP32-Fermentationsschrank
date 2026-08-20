# Issue #90 – Architektur-Neubewertung nach dem NVS-Atomizitätsblocker (R5.5)

## Status, Zweck und Owner-Gate

Diese Datei ist die vollständige kanonische Planrevision R5.5 für PR #117.
Sie ersetzt nach ihrer eigenen exakten Ownerfreigabe die normative
Planwahrheit für die weitere #90-Entscheidung. Bis dahin bleibt sie ein
unfreigegebener Architekturplan.

Der Status dieser Runde ist bis zur Freigabe der exakten Plan-Commit-SHA:

`ARCHITECTURE_REPLAN_PENDING_R5_5_PLAN_APPROVAL`

Verifizierte Baseline bei Beginn dieser Planrevision:

- PR: #117, Draft, Branch `agent/issue-90-nvs-adapter-plan`;
- PR-Head: `bfa5c810ae684bfdce875ae057fe714c2aeb126a`;
- Base-Branch: `agent/issue-29-esp32-bringup-plan`;
- Base-SHA: `30fa0a8264e2c4564d324340c6bebc204147f477`;
- vorherige ownerfreigegebene Planrevision: R5.4,
  `docs/tasks/issue-90-esp-idf-nvs-adapter-plan.md @
  c35ce0342898f0e19d3cce5e6a7eaa077f73bad6`;
- korrigierter Herstellerbericht:
  `docs/ISSUE_90_MANUFACTURER_ANALYSIS.md @
  440180f5b712a1eda427292207ebc7e9861cd284`;
- produktive ESP-IDF-Baseline: `v6.0.2 @
  7101770dc6db2667b3c477cc31365dd1acd6db4e`.

R5.4 bleibt unverändert erhalten. R5.4 hat seinen ausdrücklich definierten
`BLOCKED`-Endpunkt erreicht; R5.5 ist keine nachträgliche Ergänzung, keine
verdeckte Freigabe von Phase B und keine Neuinterpretation der R5.4-SHA.

## Ziel und Nichtziele

R5.5 plant ausschließlich die Architekturentscheidung nach dem bestätigten
NVS-Befund. Die Runde umfasst:

- Repository-, Verbraucher-, Record- und Vertragserfassung;
- Prüfung der bereits vorhandenen Konfigurations- und Run-Redundanz;
- Espressif-first- und Komponentenrecherche einschließlich Version, Commit,
  Lizenz, Kompatibilität und Power-Loss-Semantik;
- objektiven Vergleich der Optionen A–F;
- genau eine klare Entscheidungsempfehlung für das nächste Owner-Gate;
- Planung der später erforderlichen ADR-016-Aktualisierung oder Supersession;
- minimale Synchronisierung von Roadmap, PR, Issue #90 und dem bestehenden
  aktuellen `SESSION HANDOVER`.

In dieser Planrunde werden ausdrücklich nicht ausgeführt:

- Produktionscode-, `IStateStore`-API- oder NVS-Adapteränderungen;
- Harness-, Oracle-, Runner- oder Fault-Injection-Änderungen;
- Einbau oder Download einer neuen Backendbibliothek;
- ESP-IDF-Pin- oder Minor-Version-Wechsel;
- Partitionstabellen-, Ressourcen- oder Hardwareänderungen;
- ADR-016-Umschreibung oder -Ersetzung;
- Upstream-Issue-Veröffentlichung;
- Host-, ESP-IDF-, Flash-, UART- oder Hardwaretests;
- PR-Ready-Wechsel, Merge, Auto-Merge, Issue-Schließung, Branch-Löschung
  oder Force-Push.

## Ausgangsbefund und Architekturtrigger

Phase A.1 ist durch das Ownerreview fachlich akzeptiert und bleibt durch den
bestehenden Herstellerbericht belegt:

- `60561b6d...` ist für den konkreten Callback-12-/BDL-Power-Cut
  `CANDIDATE_SOURCE_SEMANTICALLY_RULED_OUT`;
- `f0c7d9b6...` ist im gepinnten v6.0.2-Verhalten bereits äquivalent enthalten;
- der Post-v6.0.2-Sweep hat keinen weiteren belastbaren offiziellen Fix für
  genau diesen Befund ergeben;
- der belastbare Herstellerbefund ist
  `NO_PROVEN_MANUFACTURER_CANDIDATE`;
- Phase B ist damit nicht freigegeben.

Der bestätigte Testfall ist kein abstrakter Eintragstest. Er betrifft den
realen Projektvertrag bei:

1. einem bereits vorhandenen Multi-Page-BLOB;
2. dem Überschreiben desselben logischen NVS-Keys;
3. einem Power-Cut in einem mutierenden 4-Byte-Callback;
4. Reboot beziehungsweise Deinit/Reinit;
5. `NotFound` statt des vollständigen alten oder neuen Werts.

Damit ist mindestens folgende bisherige ADR-016-Annahme widerlegt:

> NVS-Atomizität erfüllt im relevanten Multi-Page-BLOB-Fall den unveränderten
> `IStateStore`-Old-or-New-Vertrag.

Das ist ein materieller Architekturtrigger. Die bisher akzeptierte ADR-016
behauptet unter Variante A, dass NVS atomare Ersetzung je Eintrag,
Integrität und Wear-Leveling bereitstellt und damit Atomizität, Integrität
und Flashlebensdauer nicht als Eigenentwicklung benötigt werden. Der aktuelle
#90-Nachweis zeigt dagegen, dass diese Aussage nicht genügt, um die konkrete
Sichtbarkeit des unveränderten Projektvertrags zu beweisen. NVS-
Eintragsintegrität und Wear-Leveling werden nicht mit einem vollständigen
Old-or-New-Recoveryvertrag für diesen BLOB-Overwrite gleichgesetzt.

ADR-016 wird in R5.5 nicht still geändert. Die spätere Aktualisierung oder
Supersession ist ein eigenes Owner-Gate und wird unten verbindlich geplant.

## Unveränderter Sicherheits- und Persistenzvertrag

Bis zu einer ausdrücklichen neuen Ownerentscheidung bleibt der bestehende
starke Vertrag vollständig erhalten:

- Nach `CommitOutcomeUnknown` ist ein zuvor vorhandener Key nur dann
  erfolgreich, wenn Readback exakt den vollständigen alten oder vollständigen
  neuen Wert ergibt.
- `NotFound` nach zuvor vorhandenem Wert ist `FAIL` beziehungsweise ein
  indeterminer Persistenzzustand, nicht der alte Wert.
- Teilwert, Mischwert, beschädigter Blob oder fremder Wert ist `FAIL`.
- `CommitOutcomeUnknown` bleibt unbekannt; Readback darf ihn nur in einen
  exakt nachgewiesenen alten oder neuen Zustand auflösen.
- Ein fehlender Wert darf nur dann als „nicht vorhanden“ gelten, wenn der
  fachliche Vertrag den Key vor dem Commit als nicht vorhanden belegt.
- Fehler bleiben fail-closed: kein stilles Aktivieren, kein stilles Reset,
  keine Aktorfreigabe und kein fachliches „Erfolg“-Ergebnis aus einem
  unaufgelösten Zustand.

Eine Änderung, Aufteilung oder Abschwächung des `IStateStore`-Vertrags ist
nicht Teil der Bestandsanalyse. Falls sie nach dem Verbraucherbeweis
empfohlen wird, ist sie eine eigene materielle Architekturentscheidung mit
vollständiger Semantik-, Migrations- und Safety-Begründung.

## Schichten- und Verbrauchergrenze

Die Analyse trennt drei Ebenen:

```text
Konfigurations-/Run-Fachvertrag
    -> vorhandene Generationen, Manifest, Root, Fallback und Run-Auswahl
        -> IStateStore-Vertrag einschließlich Unknown-Auflösung
            -> physisches Backend und dessen Flash-/GC-Semantik
```

Die vorhandenen höheren Ebenen dürfen den technischen Vertrag nicht
nachträglich schwächen. Sie können einen unauflösbaren Storezustand sicher
blockieren; daraus folgt aber nicht automatisch, dass ein fehlender oder
beschädigter Schlüssel als fachlich korrekter alter Zustand wiederhergestellt
wurde.

Die Plattformgrenze bleibt unverändert:

- `device_platform` bleibt portabel und anwendungsneutral;
- `device_platform_esp_idf` enthält konkrete ESP-IDF-Adapter;
- `fermentation` bleibt Owning Context und Namespace, keine Plattform-API;
- keine GPIO-, Board- oder Fermentationsdetails wandern in den Store;
- `device_platform_test_support` bleibt Produktionscode-frei;
- `StorageEpoch`, Konfigurationsgenerationen, Manifest/Root/Fallback und
  `rc0`/`rc1`/`rh0` werden nicht entfernt;
- OTA bleibt `FUTURE_OPTIONAL` und ist für diese Entscheidung irrelevant.

## Vollständige reale `IStateStore`-Verbraucherinventur

### Gemeinsame Backend- und Portgrenzen

`IStateStore` akzeptiert Keys mit 1–15 ASCII-Zeichen aus
`[A-Za-z0-9_.-]`. Der ESP-IDF-NVS-Adapter nutzt aktuell einen Blob pro Key;
die im Adapter abgebildete NVS-Grenze ist 508000 Bytes. Das ist eine
Darstellungsgrenze, kein Nachweis der geforderten Power-Loss-Semantik.

Der Adapter ordnet Fehler vor der sicheren Commitbestätigung wie folgt zu:

- Vor-Mutationsfehler werden als `WriteError` beziehungsweise
  `CapacityError` gemeldet.
- Ein Commit-Fehler bleibt `CommitOutcomeUnknown`, weil NVS intern bereits
  Daten-, Entry-State-, Page-State- oder Indexschritte ausgeführt haben kann.
- Erfolg wird erst nach erfolgreichem `nvs_commit()` als `Success` gemeldet.
- Readback liefert `NotFound`, `ReadError` oder `CapacityError` getrennt.

Genau diese Zuordnung ist für die Analyse nützlich: Sie bewahrt Unknown,
kann aber die im R5.4-Befund beobachtete spätere `NotFound`-Lücke nicht
beheben.

### Konfigurationspersistenz

Die Konfiguration hat vier Dokumentgruppen, eine Aktivierungsmanifeststruktur,
Root-/Fallback-Schutz und eine zweistufige Bootstrap-Persistenz. Die maximalen
Store-Readbackgrößen ergeben sich aus dem bestehenden Record-Envelope und
sind keine neu erfundenen Kandidatengrößen.

| Produktiver Verbraucher | Physische Keys | Maximaler Wert pro Key | Überschreiben desselben Keys | Schreibfrequenz | Höhere Schicht / Recovery | `Unknown` / `NotFound` |
|---|---|---:|---|---|---|---|
| User-Konfigurationsdokumente | `uc0`–`uc3` | 301 B | Ja, wenn derselbe Dokument-Slot wiederverwendet wird | Konfigurationsänderung, Initialisierung, Reset; nicht im Regelzyklus | Dokumentrevision, Manifestreferenz, aktive Generation, genau eine vorherige Fallbackgeneration, `StorageEpoch` | Exaktes Readback bindet die Dokumentversion. Fehlender referenzierter Datensatz ist Graph-/Integritätsfehler; unreferenzierter leerer Slot darf beim Scan fehlen. |
| Service-Konfigurationsdokumente | `sc0`–`sc3` | 45 B | Ja, bei Slot-Reuse | Wie User-Dokumente | Wie oben | Wie User-Dokumente; kein stilles Factory-New bei beschädigtem/unerreichbarem bestehendem Zustand. |
| Programm-Katalogdokumente | `pc0`–`pc3` | 32813 B | Ja, bei Slot-Reuse; größter produktiver Konfigurationsblob | Katalog-/Konfigurationsänderung, Initialisierung, Reset | Wie oben; Record bleibt über Manifest und StorageEpoch gebunden | Teil-/Misch-/beschädigter Wert ist ungültig. Fehlender referenzierter Katalog blockiert die Graphauflösung; unreferenzierter Slot darf fehlen. |
| `ActiveConfigurationManifest` | `cm0`–`cm2` | 149 B | Ja, Manifest-Slot-Reuse | Aktivierung nach vollständig validierten Dokumenten, Initialisierung, Reset | Manifest bindet exakte Dokumentrevisionen und gemeinsame `StorageEpoch` | Fehlendes oder nicht exakt validierbares Manifest aktiviert keine neue Generation. |
| `RootRecord` | `cr0`, `cr1` | 114 B | Ja, alternierender Root-Slot | Aktivierung, Initialisierung, Reset; nicht pro Sensorzyklus | Root wählt aktive beziehungsweise eine vorherige Fallbackgeneration; bei Unknown vollständiger Scan von Roots, Manifesten und Graphrecords | Unknown wird nur durch vollständige exakte alte oder neue Graphsicht aufgelöst. Fehlender erforderlicher Root/Graphrecord ist indeterminat beziehungsweise Safe-Boot. |
| Konfigurations-Bootstrap | `cb0`, `cb1` | 42 B | Ja, alternierender Bootstrap-Slot | Boot-/Initialisierungs-/Reset-Übergänge; nicht im Regelzyklus | Zwei Slots, Sequenz/Succession, Status und `StorageEpoch` | `NotFound` ist nur für einen zuvor nicht vorhandenen Zielzustand zulässig; ein zuvor vorhandener fehlender Slot ist kein Erfolg. |

Die Konfigurationsschicht nutzt bereits eine echte fachliche Generations- und
Selektionslogik. Sie fängt fehlende oder beschädigte referenzierte Keys sicher
als Blocker ab und verhindert stille Aktivierung. Sie beweist aber nicht, dass
die technische Einzel-Key-Operation den unveränderten Old-or-New-Vertrag
erfüllt. Die Root-/Manifest-Schicht ist kein pauschaler Ersatz für jeden
einzelnen Store-Key-Vertrag.

Erhalten bleiben müssen insbesondere Dokumentrevisionen, die gemeinsame
Manifestaktivierung, die neue Generation erst nach Readbacks sichtbarmachende
Root-Operation, genau eine vorherige Fallbackgeneration, `StorageEpoch`,
Copy-Migrationen und der bestehende Schema-/Wire-Vertrag. Vorhandene
beschädigte Daten werden nicht als Factory-New behandelt.

### Run-Persistenz

| Produktiver Verbraucher | Physische Keys | Maximaler Wert pro Key | Überschreiben desselben Keys | Schreibfrequenz | Höhere Schicht / Recovery | `Unknown` / `NotFound` |
|---|---|---:|---|---|---|---|
| Run-Checkpoint-Slots | `rc0`, `rc1` | 8240 B | Ja, alternierend; 8192 B Nutzlast plus Envelope | Periodisch, Default 5, begrenzt 1–60, sowie bei relevanten Zustands-/Commandübergängen; nicht jede 2-s-Sensormessung | `rc0`/`rc1` plus `rh0`-Referenz; Schema 3, Revisionen und maximal 32 Command-IDs | Exakter neuer Wert bedeutet Written, exakter alter Wert NotWritten. Fehlendes zuvor vorhandenes Ziel ist Indeterminate und blockiert Rekonstruktion; ein orphanierter Slot wird nicht still aktiviert. |
| Persistenzkopf / Auswahl | `rh0` | 256 B | Ja, derselbe logische Key wird bei jedem Kopfupdate überschrieben | Nach erfolgreichem Checkpoint und bei Run-Kommandos, Transitions- und Recoveryentscheidungen | Kopf referenziert Checkpoints; Boot prüft Referenzen, High-Water und Orphans | Unknown wird nur durch exakte alte/neue Kopfbytes und konsistente Slotauflösung geklärt. `NotFound` eines erforderlichen Kopfes oder Slots führt zu Block/`NotReconstructible`, nicht zu stillem Reset. |

Die Run-Persistenz hat damit eine höhere Auswahlmechanik, aber keinen zweiten
persistenten Kopf-Key. Ein fehlendes `rh0` oder ein fehlender referenzierter
`rc`-Wert wird sicher blockiert; daraus folgt keine vollständige Recovery des
vorherigen oder neuen Laufs. Die hohe Schreiblast des Kopfes und die
Checkpoint-Schreiblast müssen getrennt von der seltenen Konfigurationslast
bewertet werden.

Die Safetyfolge bleibt erhalten: untrusted oder nicht rekonstruierbarer
Persistenzzustand führt zu `SAFE_BOOT` beziehungsweise blockierter
Run-Aktivierung; `NoActiveRun` wird nur Write-before-Apply kanonisiert; RAM
wird nicht vor bestätigter Persistenz produktiv angewendet; Persistenzfehler
blockieren die Aktorfreigabe.

### Weitere direkte Verbraucher und Test-/Simulationsbackends

Die Repository-Suche nach `IStateStore` und den konkreten Methoden ergibt
keinen weiteren produktiven Fachverbraucher außerhalb der Konfigurations- und
Run-Pfade.

- `ConfigurationGraphStore`, `ConfigurationBootstrapStore` und
  `ConfigurationRecoveryService` sind produktiv und erfasst.
- `run_persistence_store` und `run_persistence_coordinator` sind produktiv
  und erfasst.
- `storage_slot_candidates` ist eine generische technische Plattformhilfe mit
  maximal acht Kandidaten; sie besitzt keine Fachsemantik und ist kein
  zusätzlicher Speicherpfad.
- `issue_90_nvs_hardware_verification.cpp` ist Harness-/Hardware-Evidenz,
  nicht Produktionsverbraucher. Seine 22 Testkeys entsprechen den 19
  Konfigurations-/Bootstrap-Keys plus `rc0`, `rc1`, `rh0`.
- `issue_29_bringup_probe.cpp` ist ein Bring-up-/Fault-Probe ohne produktive
  Dauerspeicherung.
- `SimulatedPersistentStateStore` und lokale Store-Doubles dienen nur der
  Vertragsanalyse. Sie modellieren den idealen vollständigen Alt-/Neuzustand
  und sind kein Beweis für das reale NVS-Mehrseitenverhalten.

Damit ist die vorhandene Redundanz erfasst, ohne aus Testdouble-, Slot- oder
Generationsnamen automatisch einen abgeschwächten Portvertrag abzuleiten.

## Kandidaten- und Quellenrecherche

### Produktive Baseline und Herstellerpfad

Die produktive Baseline bleibt unverändert:

`ESP-IDF v6.0.2 @ 7101770dc6db2667b3c477cc31365dd1acd6db4e`

Die NVS-Dokumentation beschreibt NVS als Key-Value-Speicher mit 15-Zeichen-
Keys, Blob-Grenzen und Wear-Leveling; für größere Blobs verweist sie auf
Dateisysteme mit Wear-Leveling. Diese allgemeinen Eigenschaften sind nicht
ausreichend, um den konkret beobachteten Old-or-New-Vertrag zu beweisen.

`v6.1-rc1` darf in einem späteren Quellenvergleich als nichtproduktive
Referenz betrachtet werden. Ein Minor-Version-Wechsel ist in R5.5 keine
Lösung. Die zulässige Reihenfolge bleibt: geeigneter stabiler 6.0.x-Pfad,
dann nur nach Ownerentscheidung ein exakter offizieller Backport, danach
gegebenenfalls eine separate stabile 6.1.x-Migration mit eigenem Issue/PR.

### Option-D-Kandidaten mit Provenienz

| Kandidat | Exakte Quelle / Version / Commit | Lizenz | ESP-IDF-6.0.x-Status | Power-Loss-/Replace-Bewertung | Weitere Kosten / offene Beweise |
|---|---|---|---|---|---|
| ESP-IDF NVS + `nvs_flash`/Wear-Leveling | Bestandteil von Espressif ESP-IDF `v6.0.2 @ 7101770dc6db2667b3c477cc31365dd1acd6db4e` | Espressif-Komponenten Apache-2.0; NVS ist Teil des Herstellerframeworks | Baseline, produktiv vorhanden | Im konkreten Multi-Page-BLOB-Same-Key-Cut durch R5.4 nachgewiesen nicht ausreichend für den unveränderten Vertrag | Kein plausibler Herstellerfix; Option A bleibt möglich, aber ohne Zeitplan |
| FatFs + ESP-IDF Wear-Leveling | FatFs `R0.15 w/patch2` im genannten ESP-IDF-Commit; ESP-IDF-Adapter/Wear-Leveling im selben Commit | FatFs eigene Redistributionsbedingung; Espressif Teile Apache-2.0; Notices müssen separat geprüft werden | Eingebaut und technisch integrierbar | Offizielle ESP-IDF-Dokumentation bewertet FatFs bei plötzlichem Stromverlust als wenig resilient; zwei FAT-Kopien sind keine Beweisführung für atomaren Datei-Replace-Commit | Dateisystem-, Verzeichnis- und Partitionsemantik; zusätzliche RAM-/Flash-/GC-Messung |
| SPIFFS | ESP-IDF `v6.0.2 @ 7101770dc6db2667b3c477cc31365dd1acd6db4e`, eingebettetes SPIFFS-Submodul `ad902cadceb39d0825a97e25ecb5867641f606ba` (`V0.3.7`) | SPIFFS MIT; Espressif VFS-Adapter Apache-2.0 | Eingebaut | Offizielle ESP-IDF-Dokumentation nennt partielle Power-Failure-Resilienz; die Komponente ist nicht mehr aktiv gepflegt. Kein Beweis für den vollständigen Datei-/Record-Old-or-New-Vertrag | Statische Wear-Leveling-/GC-Eigenschaften, flache Namensstruktur, Wartungs-/Migrationsrisiko |
| `joltwallet/littlefs` | ESP Component Registry `1.22.3`; Repository `https://github.com/joltwallet/esp_littlefs`; Tag `v1.22.3`, peeled Commit `bde79e13be971a1f06d5e5f55e6f3e8568fa32b5` | MIT | Registry nennt ESP-IDF `>=5.0`; damit Kandidat für 6.0.x, aber exakte Projektkompatibilität muss erst gegen den gepinnten Build nachgewiesen werden | Upstream LittleFS beschreibt Metadaten-Paare, CRC, alte Blöcke bis zum Commit und Power-Loss-Resilienz. Das beweist noch nicht die konkrete ESP-IDF-Integration, `rename`-/Replace-Reihenfolge, GC-Cuts oder den Projekt-`IStateStore`-Vertrag | Externe Komponente, ca. 374.81-KB-Archiv in Registry, 4-KB-Blockbeispiele; direkte KV-/Record-Abbildung, maximale Recordgröße, RAM/Stack, Partition und exakte Fault-Injection sind offen |
| Weitere offizielle Espressif-Komponente | Im v6.0.2-Bestand wurde kein eigenständiges offizielles LittleFS-/transaktionales KV-Backend mit bereits passendem Vertrag gefunden | Erst bei konkretem Kandidaten bestimmbar | Kein ausgewählter Kandidat | Nicht behauptet | Ein späterer offizieller Kandidat braucht dieselbe exakte Quell-, Lizenz-, Power-Loss- und Ressourcenprüfung |

Für LittleFS ist die beobachtete „fail-safe“-Ausrichtung ein plausibler
Kandidatenhinweis, keine Freigabe. Insbesondere werden `rename`, Directory-
Commit, GC während eines Cuts, Mount-Recovery und ein vollständiger
Record-Readback in einem späteren Host- und realen Boardnachweis gegen alle
Schreibpfade geprüft. Eine Aussage „LittleFS ist power-loss-safe“ wird nicht
als Projektbeweis übernommen.

Es wurde kein Produktiv-Pin und keine Dependency-Änderung vorgenommen. Die
angegebene LittleFS-Version und der Commit sind Rechercheprovenienz für einen
späteren, ausdrücklich owner-gateten Spike; sie sind keine Freigabe, sie in
den bestehenden Build aufzunehmen.

## Bewertungsmatrix der Architekturpfade

Die Matrix bewertet die Optionen gegen den unveränderten Vertrag. `Nein`
bedeutet, dass der Pfad in der vorliegenden Form nicht genügt; `Zusatz`
bedeutet, dass eine neue, noch zu beweisende Schicht erforderlich wäre.

| Kriterium | A: NVS warten | B: App-Generationen als Ersatz | C: generische Schicht oberhalb Raw-Store | D: alternatives etabliertes Backend | E: Größenrouting | F: eigener Flash-Store |
|---|---|---|---|---|---|---|
| Old-or-New / Power-Cut | Nur nach Herstellerkorrektur; aktuell Nein | Nein als pauschale Ableitung; Zusatzvertrag nötig | Nur mit echter Transaktions-/Selectorsemantik; Zusatz | LittleFS plausibel, aber projektspezifisch unbewiesen; FatFs/SPIFFS Nein | Kleine NVS-Werte nur wie heute; große Records Zusatzbackend | Potenziell Ja, vollständig selbst zu beweisen |
| Safe-Boot / Recovery | Sicher blockiert, keine neue Recovery | Bestehende Blocker bleiben, bei Vertrags-Split Gefahr von Lücken | Neue Schicht kann Fehler zentralisieren, aber neue Recoveryfehler möglich | Backendabhängig; kein Kandidat aktuell bewiesen | Zwei Fehler-/Recoverypfade und Routingfehler | Nur durch eigene vollständige Recovery |
| Wear / GC | Herstellerverantwortung, aktueller Befund offen | Keine Entlastung des physischen Overwrite-Pfads | Zusätzliche Writes/Erases und eigene Wearanalyse | LittleFS/FatFs/SPIFFS jeweils eigene GC/WL-Semantik | Zwei Backends, unterschiedliche Wearmodelle | Vollständig Eigenverantwortung |
| Komplexität / Wartung | Niedrig, aber unbekannter Stillstand | Hoch: jeder Verbraucher braucht eigenen Beweis | Mittel bis hoch; Gefahr einer zweiten A/B-Schicht | Mittel; Adapter, Partition, Migration und externe Pflege | Hoch wegen Routing/Lifecycle/Fehlerkombination | Sehr hoch und jahrzehntelange Last |
| Device-Platform-Wiederverwendung | Bestehende Portgrenze bleibt | Portvertrag würde fachlich aufgespalten | Gute Wiederverwendung nur bei wirklich generischer Semantik | Gute Wiederverwendung mit engem Backendadapter | Gemischt; Verbraucher kennen Backendklasse oder Router | Prinzipiell, aber proprietäre Semantik |
| 4-MB-Flash | Keine Änderung | Keine Änderung, aber kein Beweisgewinn | Zusatzrecords/-keys verbrauchen Budget | LittleFS/FatFs brauchen eigene Partition; SPIFFS ebenfalls | Doppelter Infrastrukturbedarf | Format-/Journal-/GC-Overhead schwer planbar |
| RAM / Stack | Keine neue Last | Keine neue Last, aber höhere Fachkomplexität | Journal-/Selectorpuffer zusätzlich | Dateisystem-/Cache-/Mountkosten zu messen | Zwei Mount-/Adapterkosten möglich | Puffer und Recoverylog selbst zu messen |
| Schreiblast | Bestehende Last | Bestehende Last | Zusätzliche Commit-/Selectorwrites wahrscheinlich | Backendabhängig; Run-Kopf besonders kritisch | Große Writes entlastet, Routing erhöht Verwaltung | Protokoll und GC können hohe Last erzeugen |
| Migration | Keine | Keine, aber keine Lösung | Neue Format-/Key-Semantik und Copy-Migration | Bestehende NVS-Daten müssen lesbar bleiben oder einmalig sicher migrieren | Zwei Datenformate und Routingmigration | Vollständige Formatmigration erforderlich |
| Testbarkeit | Herstellerfix müsste reproduzierbar beweisen | Viele fachliche Kombinationen | Host-FI plus Integration und Board-FI | Host-FI, GC-/Rename-Cuts und Board-FI | Matrix aus Backend x Recordgröße | Vollständige eigene Fault-Matrix |
| Bibliothek / Hersteller | Espressif, aber blockiert | Keine neue Bibliothek | Eigene Plattformkomponente | LittleFS extern; FatFs/SPIFFS integriert, aber unterschiedliche Reife | Mehrere Abhängigkeiten | Keine Bibliothek, hohe Eigenverantwortung |
| Lizenz / Notices | Bestehende Espressif-Notices | Keine Änderung | Neue Komponente muss lizenziert werden | FatFs/SPIFFS/LittleFS und Espressif getrennt prüfen | Mehrere Notices | Eigene Lizenz, keine Fremdlizenz |
| ESP-IDF-Upgrades | Gering, aber Fixzeitpunkt unbekannt | Gering, bestehender Vertrag | Kopplung durch Adapter und Backend | Externe/integrierte APIs und Minor-Upgrade-Risiko | Höchste Kombination | Geringe API-, aber hohe Plattformkopplung |

## Objektive Bewertung der Optionen A–F

### Option A – NVS unverändert lassen und Herstellerkorrektur abwarten

Technisch ist dies der sicherste unmittelbare Zustand: kein unbewiesenes
Backend wird produktiv gemacht und #90 bleibt bei einem reproduzierten
Vertragsbruch blockiert. Ein Espressif-Issue beziehungsweise eine präzise
Herstellerreport-Erweiterung ist sinnvoll, weil der Befund einen konkreten
internen BDL-/Page-State-Recoverypfad beschreibt. Ein Issue-Text darf in einer
späteren Owner-gateten Runde vorbereitet werden; in R5.5 wird nichts extern
veröffentlicht.

Kosten und Grenzen:

- kein belastbarer Zeitplan für Fix, Backport oder Release;
- #90-Produktivadapter, reale NVS-Abnahme und davon abhängige
  Persistenzfreigaben bleiben blockiert;
- unabhängige Arbeiten ohne produktive #90-Persistenz können weiter geplant
  oder analysiert werden, insbesondere Vertrags-/UI-Arbeit ohne Speicher-
  beziehungsweise Hardwareabhängigkeit;
- #25/#26 dürfen die in der Roadmap vorgesehenen rendererunabhängigen
  Verträge nicht durch einen Ersatzspeicher vorwegnehmen;
- die Option löst das Architekturproblem nicht, sondern verschiebt die
  Entscheidung in die Herstellerabhängigkeit.

Bewertung: sicherer Wartezustand, aber kein vertretbarer alleiniger
Projektabschluss. Als Fallback und Upstream-Pfad erhalten; nicht als
Produktivfreigabe empfehlen.

### Option B – vorhandene Generationen-/Slotmechanik gezielt nutzen

Die Analyse bestätigt, dass die Konfiguration bereits Dokumentrevisionen,
Manifest, Root, eine aktive plus eine vorherige Generation, `StorageEpoch`
und Copy-Migrationen besitzt. Run-Persistenz besitzt alternierende
`rc0`/`rc1`-Slots und den Auswahlkopf `rh0`.

Diese Mechaniken erfüllen jedoch nicht pauschal den starken technischen
Vertrag:

- Bei Konfiguration kann ein fehlender unreferenzierter Slot als absent
  gelten, ein fehlender referenzierter Datensatz aber nicht. Bei Root-Unknown
  ist eine vollständige alte/neue Graphauflösung erforderlich. Eine Änderung
  des Storevertrags würde sonst Indeterminate-Zustände in Fachlogik
  verschieben.
- Bei Run-Persistenz schützt `rc0`/`rc1` die Auswahl nicht vor einem
  unauflösbaren `rh0`-Overwrite. Ein fehlender erforderlicher Checkpoint oder
  Kopf wird sicher blockiert, aber nicht sicher als alt oder neu rekonstruiert.
- Eine Aussage „die App hat ohnehin A/B“ würde die hohe Schicht mit dem
  technischen Commitvertrag verwechseln.

Option B ist daher nur als eigene, per-Verbraucher zu beweisende
Vertragsneuschneidung zulässig. Sie ist keine Empfehlung für R5.5.

### Option C – generische Transaktions-/Redundanzschicht oberhalb eines Raw-Stores

Eine anwendungsneutrale Plattformkomponente wäre architektonisch sauberer als
Transaktionslogik im `NvsStateStore`, wenn sie eine klar begrenzte zusätzliche
Semantik liefert. Sie dürfte keine Fermentationskeys und keine
Konfigurations-/Run-Auswahl besitzen.

Vor einer Empfehlung muss jedoch beantwortet werden:

- Reicht die Wiederverwendung der vorhandenen Manifest-/Root-/Slotmechanik,
  oder wird tatsächlich ein generischer Selector/Journal benötigt?
- Wie werden mehrere Records, `CommitOutcomeUnknown`, GC und Mount-Recovery
  ohne zweite parallele Generationsarchitektur abgewickelt?
- Wie viele zusätzliche Keys/Writes/Erases entstehen bei seltenen
  Konfigurationsänderungen und häufigen `rh0`-Updates?
- Welche Partition, RAM-/Stack-Puffer und 4-MB-Reserven sind nach Messung
  verfügbar?

Ein generischer Decorator könnte die Portgrenze schützen, ist in der
vorliegenden Analyse aber noch kein kleiner KISS-Baustein. Auf NVS allein
würde er das beobachtete interne BLOB-Problem nicht automatisch beheben; als
allgemeiner Journal-/Selectorpfad würde er eine zweite
Generationsarchitektur einführen. Option C bleibt daher eine mögliche
Kompositionsform für einen späteren Spike, nicht die R5.5-Empfehlung.

### Option D – alternatives etabliertes Speicherbackend

LittleFS ist der einzige recherchierte Kandidat mit einem plausiblen
Power-Loss-Design, begrenztem RAM-Ansatz, dynamischem Wear-Leveling und einer
aktiven ESP-IDF-Komponente. Seine Metadaten-Paarsemantik ist ein guter
Ausgangspunkt für einen späteren Beweis. Sie ersetzt aber nicht die
projektspezifische Prüfung der gesamten Recordoperation einschließlich
Datei-/Rename-/GC-Cuts. Die externe Komponente, ihr exakter Pin, ihre
Dependency, die Partition und die Notices erhöhen die Wartungslast.

FatFs plus Wear-Leveling ist Hersteller-nah und Apache-/FatFs-lizenziert,
aber die offizielle Dokumentation warnt vor geringer Resilienz bei abruptem
Power-Loss. Zwei FAT-Kopien sind kein atomarer Replace-Vertrag. SPIFFS bietet
Wear-Leveling und eine kleine statische Struktur, ist laut offizieller
Dokumentation aber nur partiell power-failure-resilient und nicht mehr aktiv
gepflegt. Beide sind für den unveränderten Vertrag nicht bewiesen.

Option D ist daher der einzige sinnvolle Kandidatenpfad für einen
owner-gateten Spike, aber noch keine sichere Architekturentscheidung.

### Option E – Aufteilung nach Recordgröße

Kleine Konfigurationswerte könnten theoretisch NVS behalten, während
Multi-Page-Records in ein anderes Backend wechseln. Das reduziert weder die
Anzahl der Verträge noch die Lebenszyklusfragen: Routing, Partition, Mount,
Fehlerabbildung, Migration, Readback und Recovery müssten für jeden
Verbraucher explizit getrennt werden. Run-Checkpoint und `rh0` liegen zudem
beide im Bereich, in dem häufige und kritische Writes eine einheitliche
Recoverybetrachtung benötigen.

Option E ist nur zulässig, wenn Messungen nachweisen, dass eine solche
Mischung deutlich einfacher ist als ein einziger generischer Backendpfad. Das
ist derzeit nicht bewiesen und wird nicht empfohlen.

### Option F – eigener Flash-Recordspeicher

Ein eigener Speicher könnte den Vertrag formal passend entwerfen, müsste
aber Wear-Leveling, Stromausfall-Commitprotokoll, GC, Recovery,
Formatmigration, BDL-/Host-Fault-Injection und reale Boardtests vollständig
selbst verantworten. Er wäre über Jahrzehnte zu pflegen und würde die
kritische Persistenz in projektinternen Code verschieben.

Option F ist nur eine letzte Referenzoption, falls alle etablierten
Komponenten und kleineren Kompositionen nachweislich scheitern. R5.5 findet
keinen solchen Nachweis und empfiehlt ausdrücklich nicht „selbst bauen“ als
Default.

## Klare Empfehlung für R5.5

### Empfehlung: `ARCHITECTURE_BLOCKED`

Keine der geprüften Optionen erfüllt den unveränderten
`IStateStore`-Old-or-New-Vertrag für alle produktiven Verbraucher mit
bereits belastbarer Evidenz und vertretbarer Änderungs-/Wartungslast.

Deshalb wird für R5.5 genau eine Empfehlung ausgesprochen:

`ARCHITECTURE_BLOCKED`

Es wird keine produktive Backendwahl, keine Vertragsschwächung und keine
Implementation freigegeben. Option A bleibt der sichere Betriebszustand.
Für die nächste Ownerentscheidung ist Option D mit dem exakt gepinnten
LittleFS-Kandidaten `joltwallet/littlefs v1.22.3 @
bde79e13be971a1f06d5e5f55e6f3e8568fa32b5` der bevorzugte *Untersuchungspfad*,
nicht die beschlossene Architektur. Diese Unterscheidung verhindert, dass
eine positive Bibliotheksbeschreibung als Produktionsbeweis missverstanden
wird.

Der LittleFS-Spike darf nur dann in einen Implementierungsplan übergehen,
wenn er für jeden oben inventarisierten produktiven Verbraucher alle
folgenden Punkte belegt:

- exakter vollständiger alter oder neuer Record nach jedem relevanten
  Host-/GC-/Rename-/Commit-Cut;
- keine stille Änderung von `NotFound`, Unknown, Integrität, Safe-Boot oder
  Recovery;
- sichere Behandlung von `rc0`, `rc1` und insbesondere `rh0` bei hoher
  Schreiblast;
- Erhalt der Konfigurationsgenerationen, Manifest-/Root-/Fallbacklogik und
  `StorageEpoch` ohne zweite unnötige A/B-Schicht;
- 4-MB-Partition, Firmware, RAM/Stack, Heap und Wear-Budget mit Messdaten;
- lesbare/migrierbare Bestandsdaten oder ein ausdrücklich freigegebener
  einmaliger Migrationspfad;
- Host-Fault-Injection und reale kontrollierte Board-Power-Cuts auf dem
  finalen exakten Head;
- festgeschriebene Source-/Dependency-/License-/Notice-Provenienz und
  ESP-IDF-6.0.2-Kompatibilität.

Scheitert dieser Spike, bleiben `ARCHITECTURE_BLOCKED` und Option A bestehen;
danach wäre erst ein Owner-Gate für eine kleinere Komposition oder als letzte
Referenz Option F zulässig.

## Geplante Owner-gatete Folgeslices

Diese Slices sind Umsetzungsvorbereitung und keine Freigabe für die aktuelle
Runde:

1. **Vertragsentscheidung:** Owner entscheidet ausdrücklich, ob der starke
   Portvertrag unverändert bleibt (empfohlen) oder ob eine per-Verbraucher-
   Vertragsaufteilung überhaupt untersucht werden darf. Ohne diese
   Entscheidung bleibt `ARCHITECTURE_BLOCKED`.
2. **Kandidatenspike:** Für den bevorzugten LittleFS-Untersuchungspfad werden
   Komponente, Tag/Commit, transitive Dependencies, Lizenz-/Noticeumfang,
   ESP-IDF-6.0.2-Build, Mount-/Partitionseigenschaften und Grenzfälle exakt
   fixiert. Kein Floating-Tag und kein unbeabsichtigter Minor-Upgrade.
3. **Generische Adaptersemantik:** Erst nach Slice 1/2 wird entschieden, ob
   ein kleiner anwendungsneutraler Plattformadapter genügt oder ob eine
   zusätzliche Journal-/Selectorsemantik erforderlich ist. Die vorhandene
   App-Generationslogik wird wiederverwendet; eine parallele zweite
   Generationsarchitektur ist verboten, solange sie nicht nachweislich nötig
   ist.
4. **Host-Vertragsnachweis:** Alle produktiven Recordgrößen und Schreibpfade,
   einschließlich Root-/Manifest-Unknown, Bootstrap, `rc0`/`rc1` und `rh0`,
   werden mit jedem mutierenden Backendschritt, GC- und Rename-Cut geprüft.
   Das Orakel bleibt stark; `NotFound` nach vorher vorhandenem Wert bleibt
   Fail/Indeterminate.
5. **Ressourcen und Migration:** Erst nach einem grünen Hostvertrag werden
   Partition, 4-MB-Budget, Firmware-/RAM-/Stack-Peaks, Write-/Erase-Last,
   Wear-Projektion und Bestandsdatenmigration geplant. Eine Partitionstabelle
   oder ein Schema wird in R5.5 nicht geändert.
6. **Reale Verifikation:** Ein späterer owner-gateter Hardware-Slice prüft
   kontrollierte Power-Cuts und Reboot/Recovery am finalen Board und finalen
   Head. Host-Fault-Injection ersetzt diese Hardwareevidenz nicht.
7. **ADR-Folge:** Erst wenn Owner die Architektur und die Nachweise akzeptiert,
   wird ADR-016 in einem eigenen Plan/Commit entweder materiell aktualisiert
   oder durch eine klar referenzierte neue ADR supersediert.

Jeder Slice erhält ein eigenes PASS/FAIL/BLOCKED/NOT_RUN-Ergebnis. Ein
teilweise geprüfter Kandidat wird nicht als produktive Lösung bezeichnet.

## Erforderliche ADR-016-Folge

ADR-016 bleibt in R5.5 unverändert `accepted` und wird nur als historische,
durch den neuen Befund materiell zu aktualisierende Entscheidung referenziert.
Die spätere ADR-Aktualisierung/Supersession muss mindestens enthalten:

- den konkreten Multi-Page-BLOB-/Same-Key-/Callback-12-/`NotFound`-Befund;
- die Abgrenzung zwischen NVS-Eintragsintegrität/Wear-Leveling und dem
  `IStateStore`-Old-or-New-Recoveryvertrag;
- den finalen Ownerentscheid für Portvertrag und bevorzugtes Backend;
- alle betroffenen Verbraucher, Keys, Recordgrößen und Recoverypfade;
- alte/neue Datenformat- und Copy-Migrationsfolgen;
- exakte Backend-/Komponenten-/Dependency-Versionen, Commits, Lizenzen und
  Noticeanforderungen;
- 4-MB-, RAM-/Stack-, Write-/Erase-/Wear- und Wartungskosten;
- Host-Fault-Injection- und reale Board-Power-Cut-Nachweise;
- die Begründung, warum die Device-Platform-Grenze erhalten bleibt und keine
  zweite unnötige Generationsarchitektur entsteht.

Bis zu diesem eigenen ADR-Gate gibt es keine materielle Änderung an
`docs/ADR-016_KONFIGURATIONSSPEICHER_BACKEND.md`.

## R5.5-Abnahmekriterien und Nachweise

R5.5 ist als Planrevision vollständig, wenn:

- der aktuelle PR-/Issue-/Roadmap-/Handover-Stand auf den neuen R5.5-Status
  und die R5.4-Provenienz zeigt;
- die vollständige produktive Verbraucher-/Recordinventur einschließlich
  Test-/Simulationsabgrenzung vorliegt;
- die höhere Konfigurations- und Run-Redundanz gegen den technischen Vertrag
  abgegrenzt ist;
- die Optionen A–F und die Matrix alle geforderten Kriterien abdecken;
- die externen Kandidaten mit exakter Quelle, Version, Commit und Lizenz
  bewertet sind;
- `ARCHITECTURE_BLOCKED` als genau eine Empfehlung und LittleFS nur als
  Untersuchungsrichtung ausgewiesen ist;
- die ADR-016-Folge, Ownerentscheidungen und spätere Slices explizit sind;
- kein Produktions-, Test-, Harness-, Oracle-, Dependency-, Pin-,
  Partition- oder Hardwareartefakt geändert wurde.

Für die aktuelle Runde gilt:

- Plananalyse: `PASS`;
- Herstellerbefund: `PASS` für den Befund
  `NO_PROVEN_MANUFACTURER_CANDIDATE`, Evidenz unverändert referenziert;
- Implementierung: `NOT_RUN` und nicht freigegeben;
- Host-/ESP-IDF-/Hardwaretests: `NOT_RUN` und nicht freigegeben;
- Dependency-/Pin-/Partitionsänderung: `NOT_RUN`, keine Änderung;
- Produktionsfreigabe: `BLOCKED` bis Architektur- und anschließendem
  Implementierungs-Owner-Gate.

## Referenzen

Interne Primärquellen:

- `docs/ISSUE_90_MANUFACTURER_ANALYSIS.md`;
- `docs/ADR-016_KONFIGURATIONSSPEICHER_BACKEND.md`;
- `docs/CONFIGURATION_PERSISTENCE.md`;
- `docs/RUN_PERSISTENCE.md`;
- `docs/SETTINGS_AND_STORAGE.md`;
- die kanonischen `device_platform`-Storeverträge;
- `docs/AGENT_WORKFLOW.md`;
- `docs/ENGINEERING_PRINCIPLES.md`;
- `docs/ADOPT_OR_BUILD.md`;
- `docs/THIRD_PARTY_COMPONENTS.md`;
- `docs/ESP_IDF_UPGRADE_CONTRACT.md`;
- `docs/RESOURCE_BUDGET_AND_MAINTENANCE.md`;
- `docs/CI_AND_QUALITY_GATES.md`.

Externe Recherchequellen:

- [ESP-IDF v6.0.2 NVS-Dokumentation](https://docs.espressif.com/projects/esp-idf/en/stable/esp32/api-reference/storage/nvs_flash.html);
- [ESP-IDF File-System Considerations](https://docs.espressif.com/projects/esp-idf/en/v5.5/esp32c5/api-guides/file-system-considerations.html)
  (offizielle Vergleichsquelle; die produktive Baseline bleibt v6.0.2);
- [ESP-IDF FatFS-Dokumentation](https://docs.espressif.com/projects/esp-idf/en/latest/esp32/api-reference/storage/fatfs.html);
- [ESP-IDF Issue #8197 zu abruptem Power-Loss bei FAT/SPIFFS](https://github.com/espressif/esp-idf/issues/8197);
- [ESP Component Registry: `joltwallet/littlefs` 1.22.3](https://components.espressif.com/components/joltwallet/littlefs/versions/1.22.3/readme);
- [`joltwallet/esp_littlefs`, Tag v1.22.3](https://github.com/joltwallet/esp_littlefs/tree/v1.22.3);
- [LittleFS Design: Power-Loss, Metadata-Pairs, GC und Wear-Leveling](https://raw.githubusercontent.com/littlefs-project/littlefs/master/DESIGN.md).

Diese Quellen werden als Recherchebeleg verwendet, nicht als Ersatz für den
späteren projektbezogenen Fault-Injection- und Hardwarevertrag.
