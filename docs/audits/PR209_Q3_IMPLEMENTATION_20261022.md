# PR #209 – begrenzte Q3-Implementierung und Stackgate

2026-10-22. **Q3_IMPLEMENTED; HTTPD_FULL_CHAIN_STACK_GATE=FAIL;
C5_RESOURCE_GATE=HOLD; OPEN_BLOCKERS=1.**

Freigegebener Gesamtplan Revision 2.3:
`1b00baea77a6168f29172ffa91852e10fdfd0c7e`, nach Review vom Owner zur
begrenzten Umsetzung freigegeben und am 2026-10-22 nochmals exakt bestaetigt.
Implementierungs-/Build-SHA:
**`095f393f0eed4eb95944c164a8e967ce973c3fc9`**. Produktquellen seitdem unveraendert.
Die fruehere C0–C5-Freigabe bleibt `b7d5e2dbc1f5281c764d4a75f2c37f1c4b6397c2`.

## Umsetzung innerhalb von §12

- Privater `ConfigurationService::CommitScratch` mit genau `current` und
  `prepared`, pro serialisiertem Commit ein transienter Owner. Explizites
  aligned/nothrow-new; passendes aligned-delete im zustandslosen Deleter.
  Kein Pool, Worker, Task, Queue, weiterer Modellowner oder Reservebudget.
- `ConfigurationGraphStore::prepareCommitInto` verwendet dieselbe Validierung,
  Scan-/Slot-/Generations-/Herkunftslogik wie bisher. Ergebnisoptional und
  Graphbranches werden direkt darin aufgebaut. Der bisherige Return-by-value-
  Eintritt delegiert an diese eine Implementierung; alle Fehler leeren das
  Ergebnisoptional. Persistente Bytes und Execute-/Recovery-Vertrag unveraendert.
- `confirmPreview` verwendet Referenzen in den Scratch; keine grossen
  Rueckgabe-/Prepared-/Graph-Stacktemporaries. Der vorhandene Resolution-Owner
  wird direkt im Heap konstruiert und durch Moves gefuellt. Indeterminate/
  RuntimeFailure behalten nur diesen bisherigen Owner, keinen Scratch.
- Scratch-OOM vor jeder Schreiboperation liefert ConfigurationMutationBusy;
  die Vorschau wird wie beim vorhandenen Mutation-Busy wieder sichtbar,
  der Mutation-Lease wird freigegeben. Kein neuer Runtime-/Persistenzzustand.
  NoChange/stale und bisherige Fehler-/Fail-closed-/Retirement-Gates bleiben.
- Kein Run-Commit-Fix, keine 8192-B-Stackaenderung, keine F4-/Heapbudgetaenderung,
  keine schreibende Zeitzonenfunktion, keine Plattform-/Drahtformatneuerung.

## Ressourcen und Grenzen

[Neue reale Xtensa-ABI-Werte](PR209_Q3_ABI_20261022.tsv) aus den unveraenderten
Compile-Befehlen der beiden auf `095f393` gebauten Profile, GCC 15.2.0
`esp-15.2.0_20251204`, IDF v6.1 `fff9895c82d744c7237be8847347bdd1b07c6643`:

| Produktionsobjekt | `sizeof` B | `alignof` B |
|---|---:|---:|
| `CommitScratch` | **1856** | **8** |
| `unique_ptr<CommitScratch, CommitScratchDeleter>` | **4** | **4** |

Der zustandslose Deleter vergroessert den Handle nicht. Zusatzrequest,
Lebensdauern und die konservative Allocatorgrenze **1936 B** bleiben innerhalb
[der freigegebenen Bilanz](PR209_SCRATCH_ANALYSIS_20261021.md). Der
Resolution-Owner ist weiter 1512 B (Xtensa-GDB am echten privaten Debugtyp),
Publish-Graph 552 B; deren 2064 B gehoeren schon zum Ist-Heap. Keine weitere
Graph-/Stringkopie eingefuehrt; Moves und direkte Memberkonstruktion reduzieren
bisherige Temporaries. **Dies ist kein absolutes Gesamtheapmaximum.**

Hostfenster mit vorhandenem maximalen gueltigen 16-Eintrag-Katalog, maximalen
Programmpayloads und 48 zweibyteigen Namensskalaren (96 B): genau eine Scratch-
Allokation, Peak gleichzeitig ein Scratch, kein Scratch nach Rueckkehr;
Runtime-/Modellgenerationen wieder 1. Hostmessung mit instrumentiertem new/
aligned-new (angeforderte Bytes, Allocatoraufschlag nicht erfasst):

```text
Q3_HOST64_MAX_CATALOG_NAME: scratch=2024 peak_delta=48104 final_delta=576
```

**HOST-64-Bit**, keine ESP32-Groessen. Peak/Finaldelta beziehen sich auf den
vorbereiteten Previewzustand; der aktive Name und persistierte Zustand aendern
sich. Keine Umdeutung der 2024/48104/576 B zu ESP32-Bedarf oder Heapfreigabe.
Die bestehende wiederholte Preview-/Cancel-Allokationsregression besteht.
Scratch-OOM und Indeterminate-Rueckkehr werden zusaetzlich mit 0 B lebendem
Scratch geprueft. Nachgelagerte allgemeine OOM-Festigkeit bleibt NOT_PROVEN.

### Vollketten-Stackgate: FAIL, keine Teilframe-Freigabe

[Rohe Framewerte beider Profile](PR209_Q3_STACK_20261022.tsv), alle `static`;
kanonische `.su`/`.ci` auf `095f393`. HTTP-Adapter mit exakt seinem jeweiligen
Compile-Befehl plus `-fstack-usage -fcallgraph-info=su` nach `/tmp` uebersetzt.

| Funktion | Vorher B | Q3 B (beide Profile) |
|---|---:|---:|
| HTTP-Adapter `handleRequest` | 704 | 704 |
| Gemeinsamer Web-Mutationshandler | 544 | 544 |
| `commitConfigurationChange` (Settings) | 272 | 272 |
| `confirmPreview` | 3728 | **368** |
| `prepareCommit` / jetzt `prepareCommitInto` | 3280 | **1040** |
| Ausgewaehlte Settings-Teilframes | 8528 | **2928** |

Der kompatible Return-by-value-Wrapper hat 48 B, wird im Service-Schreibpfad
nicht mehr aufgerufen. `validationScan` 464 B, `executePreparedCommit` 240 B.
**2928 B sind eine Teil-Summe**, ohne weitere Validierungs-/Scan-/Codec-/
Dispatcher-/SDK-/NVS-Frames und ohne IRQ-/indirekte-/rekursive Pfadaufloesung.
**SETTINGS_FULL_CHAIN_STACK=NOT_PROVEN**, keine behauptete Reserve.

Das vorgeschriebene bestehende Run-Gate liefert bereits einen eindeutigen
Fehler, unabhaengig von weiteren SDK-/NVS-Frames:

| Gleichzeitig aktive Run-Frames | B |
|---|---:|
| `WebRunMutationHandler::handle` | 544 |
| `WebRunMutationHandler::applyRunMutation` | 2048 |
| `FermentationApplication::applyConfirmedPrepared` | **10880** |
| Untergrenze nur dieser drei Frames | **13472 >8192** |

Beide direkten Kanten stehen in der jeweiligen `web_application_routes.cpp.ci`;
Callsites verarbeiten das Ergebnis weiter, kein ersetzender Tailcall.
`applyConfirmedPrepared` ist auf einem bestaetigten Run-Intent erreichbar und
ueberschreitet mit seinem einzelnen festen Frame bereits den gesamten HTTPD-
Stack. Source: `fermentation_application.cpp:646`; Run-Callsite
`web_application_routes.cpp:644`. Diese Dateien/Pfade wurden durch Q3 **nicht
geaendert**. Adapter-, Dispatcher-, HTTPD-SDK-, Store-/NVS- und weitere
Run-Unterframes fehlen in 13472 B; sie koennen den Fehler nicht korrigieren.

**RUN_HTTPD_STACK=FAIL; HTTPD_FULL_CHAIN_STACK_GATE=FAIL.** Nach dem harten
Fehler wurde keine Vollkettenobergrenze/Reserve behauptet und der Produktfix
nicht auf Run-/SDK-Pfade ausgeweitet. Dieser begrenzte Q3-Ansatz erteilt somit
keinen C5-Ressourcenabschluss. Ein Run-Fix oder eine andere Loesung benoetigt
neue versionierte Planung und ausdruecklichen Ownerentscheid. 8192 B bleiben.

Historisches H-RAM-1, F4 PROVISIONAL (13312/16640 B), TOCTOU und kumulative
WLAN-/HTTP-/LVGL-Last bleiben wie in der Vorfreigabe-Analyse eingeordnet.
Aktueller Maximalheap, Blockverfuegbarkeit/Fragmentierung, F4-Wirkung, reale
Heap-/HWMqualifikation: **NOT_PROVEN**. Kein Hardwarelauf.

## Gezielte Verifikation

- Konfigurationsservice / Graph-Store: **95/95 PASS**, einschliesslich neuem
  Scratch-OOM/Retry ohne Schreiben/Generationswechsel/verlorenen Mutation-Lease,
  NoChange/stale ohne Scratch, maximalem Katalog/Name und Rueckkehr ohne Scratch
  bei Indeterminate; In-place/Value-Entry mit denselben Manifest-/Root-/Fallback-
  Bytes, Ausfuehrung/Laden und geleertem wiederverwendeten Fehlerergebnis.
  Bestehende Golden-/Slot-/CRC-/Root-/Indeterminate-/Retirementfehlerfaelle PASS.
- Direkt betroffene Konsumenten: **303/303 PASS** in Web-Routen, Touch,
  Mutation-Coordinator, Bootstrap-Store, Konfigurationscodecs/-dokumenten,
  Graph-Codecs, Migration und Recovery-Service. Darunter bestehende Herkunft
  Web/Touch, Replay, Q1/Q2 und Settingsgates; keine dritte Modellgeneration.
- Beide kanonischen ESP-IDF-Profile inkl. Profil-/Herkunft-/Drift-/Source-Clean-
  Pruefung **PASS** auf sauberem Implementierungscommit `095f393`.
- ABI 1856/8 und Ownerhandle 4/4 in beiden Profilen PASS; Run-Callgraphkanten
  in beiden Profilen bestaetigt. Stackgate ist **FAIL**, nicht Buildfehler.
- Builder-Self-Check wird auf dem finalen Dokumentations-HEAD ausgefuehrt;
  exakte SHA/Ergebnis im kanonischen SESSION HANDOVER. Full Pre-Ready NOT_RUN.
  Keine Umdeutung alter JS-/Browser-/Geraetelaeufe zu neu ausgefuehrten Tests.

Ausgefuehrte gezielte Testbefehle (seriell, nicht der vollstaendige Hostlauf):

```bash
pio test -e native -f test_configuration_service -f test_configuration_graph_store
pio test -e native -f test_web_application_routes -f test_local_touch_ui -f test_configuration_codecs -f test_configuration_graph_codecs -f test_configuration_documents -f test_configuration_recovery_service -f test_configuration_migration -f test_configuration_mutation_coordinator -f test_configuration_bootstrap_store
python3 scripts/build_esp_idf_profiles.py all
```

Die kanonische ESP-IDF-Umgebung und `IDF_TOOLS_PATH`/Python aus dem Buildcache
wurden wie im Projektvertrag verwendet. Firmware weder geflasht noch gestartet.

## Artefaktbindung und Handoff

| Profil / Artefakt auf `095f393` | SHA-256 |
|---|---|
| Release BIN | `e1a776ff99524bd46756e6f5b4797fc11ab51caf254c2e237bc96ad2ef02aca2` |
| Release ELF | `5af3f3fb6dfd794f63ac842fa8def0e6801da88117337612a06283e1c3c078df` |
| Bring-up BIN | `ec7f06a60df97c5a20741fa62642871065f3238a0b1a4148a200d5e4368dbbe5` |
| Bring-up ELF | `67167927410456f4c7ed0cdeb7658b2c88fa585534506f4ea4e46e45b43e7f02` |

Die lokalen kurzen Artefaktmanifeste wurden mit dem kanonischen
`build_report.write_artifact_manifest` und der verifizierten Build-/Source-SHA
`095f393` aktualisiert; kein erneuter Firmwarebuild.

Das neue Q3-Ressourcendelta ist innerhalb der freigegebenen Objektbilanz
umgesetzt; Ressourcenblocker bleibt offen. **STOP fuer weitere Produktarbeit**;
Builder-Self-Check und unabhaengiger Full Review bewerten den bestehenden Diff,
geben aber keinen Run-Fix und keine Hardware frei. PR bleibt Draft, Issue OPEN.
**RAM_PASS=NOT_PROVEN; ACTUATOR_RELEASE=NO; D9/Hardware/Flash/Browser/UART/Reset
NOT_RUN; PRE_READY_LOCAL_GATES=NOT_RUN.**
