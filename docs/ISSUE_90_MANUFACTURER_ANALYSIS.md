# Issue #90 – ESP-IDF-NVS-Herstelleranalyse (Phase A)

## Ergebnis

- Ursprünglicher Phase-A-Befund (Berichtcommit `ce09d4feb2a8fa47fb5db7a8f769ba4f39e1e1ad`): `NO_PROVEN_MANUFACTURER_CANDIDATE`.
- Phase-A.1-Korrekturergebnis: `CANDIDATE_SOURCE_SEMANTICALLY_RULED_OUT` für `60561b6d...` im konkreten Callback-12-/BDL-Power-Cut-Modell; damit bleibt das aktualisierte Gesamtergebnis `NO_PROVEN_MANUFACTURER_CANDIDATE`.
- Empfohlener aktueller Status von Issue #90: `BLOCKED_NO_PROVEN_MANUFACTURER_CANDIDATE`
- Diese Datei ist ein versionierter Evidenz-/Provenienzbericht. Sie erweitert
  weder den freigegebenen R5.4-Plan noch erzeugt sie einen zweiten normativen
  Persistenz-, Recovery- oder Testvertrag.
- Die Ownerfreigabe des exakten R5.4-Plans
  `docs/tasks/issue-90-esp-idf-nvs-adapter-plan.md @
  c35ce0342898f0e19d3cce5e6a7eaa077f73bad6` autorisierte ausschließlich diese
  Phase A. Es wurden keine Projektabhängigkeit, kein ESP-IDF-Pin, kein
  Vendor-/Backport-Patch, kein Produktionsadapter, kein Host-Harness, kein
  Oracle, kein Runner und keine Hardware geändert oder getestet.
- Ausgangspunkt der Phase-A-Arbeit war PR #117 HEAD
  `94c773da937608fcbccc98892819a5e522329b80`. Der neue Berichtcommit und
  dessen HEAD werden ausschließlich in den dynamischen Statusquellen
  veröffentlicht.

## Exakte Herstellerprovenienz

### Gepinnter Projektstand

Der lokal verfügbare ESP-IDF-Checkout war unverändert auf:

```text
v6.0.2
7101770dc6db2667b3c477cc31365dd1acd6db4e
```

Die offizielle Release-Provenienz ist [ESP-IDF v6.0.2](https://github.com/espressif/esp-idf/releases/tag/v6.0.2).
Die für verbindliche Aussagen verwendeten Quellen werden am exakten Commit
referenziert:

- [NVS-Dokumentation am Pin](https://github.com/espressif/esp-idf/blob/7101770dc6db2667b3c477cc31365dd1acd6db4e/docs/en/api-reference/storage/nvs_flash.rst)
  einschließlich Power-Off-/Recovery-Vertrag und Page-State-Beschreibung;
- [File-System-/NVS-Power-Loss-Vertrag am Pin](https://github.com/espressif/esp-idf/blob/7101770dc6db2667b3c477cc31365dd1acd6db4e/docs/en/api-guides/file-system-considerations.rst);
- [`requestNewPage()` am Pin](https://github.com/espressif/esp-idf/blob/7101770dc6db2667b3c477cc31365dd1acd6db4e/components/nvs_flash/src/nvs_pagemanager.cpp);
- [Page-State-/Recoveryimplementierung am Pin](https://github.com/espressif/esp-idf/tree/7101770dc6db2667b3c477cc31365dd1acd6db4e/components/nvs_flash/src);
- [offizielle NVS-Hosttests am Pin](https://github.com/espressif/esp-idf/tree/7101770dc6db2667b3c477cc31365dd1acd6db4e/components/nvs_flash/host_test).

Der offizielle [`release/v6.0`-Branch](https://github.com/espressif/esp-idf/tree/release/v6.0)
ist als mögliche Vergleichsquelle vermerkt. Branch- oder Commitdaten werden
nicht als lineare Versionsbeziehung und nicht als unveränderliche
Herstellerprovenienz verwendet.

### Relevanter Codepfad

In `components/nvs_flash/src/nvs_pagemanager.cpp::requestNewPage()` des
exakten Pins ist die wirksame Reihenfolge:

1. `erasedPage->markFreeing()`;
2. danach `activatePage()`;
3. danach `erasedPage->copyItems(*newPage)`;
4. danach `erasedPage->erase()`.

Der Quelltext enthält unmittelbar vor `activatePage()` den Recovery-Kommentar,
dass die Existenz des transienten `FREEING`-Zustands die Recovery nach
Power-Loss steuert. Die lokale Source-Analyse erfolgte über den exakten
Checkout; es wurde kein Hersteller- oder Projekt-Testlauf gestartet.

### Offizielle Tests als Hersteller-Evidenz

Analysiert wurden die Testquellen, nicht ausgeführt:

- [`test recovery from sudden poweroff`](https://github.com/espressif/esp-idf/blob/7101770dc6db2667b3c477cc31365dd1acd6db4e/components/nvs_flash/host_test/nvs_host_test/main/test_nvs.cpp)
  mit wiederholter Fehlerpositionierung über Flash-Write-/Erase-Operationen;
- Recovery während einer Löschung auf einer nicht aktiven Page und
  [Recovery während des Page-Freeing](https://github.com/espressif/esp-idf/blob/7101770dc6db2667b3c477cc31365dd1acd6db4e/components/nvs_flash/host_test/nvs_host_test/main/test_nvs.cpp);
- [Page-/Flash-Fehler-Injektionstests](https://github.com/espressif/esp-idf/blob/7101770dc6db2667b3c477cc31365dd1acd6db4e/components/nvs_flash/host_test/nvs_page_test/main/nvs_page_test.cpp);
- [BDL-Ramdisk-/Hosttest-Hilfen](https://github.com/espressif/esp-idf/blob/7101770dc6db2667b3c477cc31365dd1acd6db4e/components/nvs_flash/host_test/nvs_host_test/main/bdl_ramdisk.cpp).

Diese Hersteller-Tests belegen die Existenz offizieller Recovery-Evidenz und
der Fehler-Injektionsmechanik. Sie sind nicht der projektseitige vollständige
BDL-Cut-Vertrag und ersetzen dessen späteren Vergleich nicht.

## Abgleich mit der bestehenden Projekt-Evidenz

Die vorhandene PR-/Plan-Evidenz wurde wiederverwendet, nicht neu erzeugt:

```text
PASS binary-empty
PASS invalid-configuration
PASS bounded-read
PASS blob-boundaries
PASS maximum-record
PASS error-mapping
FAIL old-or-new-bdl-cut: old-or-new read status=1 callback=12
```

Der bestehende Befund ist ein Callback-12-/`NotFound`-Fehler nach einem
mutierenden 4-Byte-Entry-State-Write. Er bleibt ein echter FAIL und wird nicht
als Hersteller-PASS, `old-or-new`-PASS oder vollständige Exhaustive-Evidenz
umetikettiert. Die vollständige Projekt-Cut-Matrix wurde in Phase A nicht
ausgeführt.

Damit ist die relevante Evidenzlage:

- der gepinnte Herstellerpfad enthält bereits die dokumentierte
  `FREEING`-Recovery-Reihenfolge;
- der bestehende direkte Projektpfad zeigt trotzdem den Callback-12-/
  `NotFound`-Befund;
- die allgemeine Herstellerdokumentation zur Power-Off-Recovery beweist
  deshalb nicht, dass der konkrete Projektadapterpfad bereits den
  unveränderten `IStateStore`-Old-or-New-Vertrag erfüllt.

## Herstellerkandidaten und Ausschlüsse

### `f0c7d9b6c603658c832858d0a4f25b5a05ea1760`

- Offizielle Quelle:
  [Espressif-Commit `f0c7d9b6...`](https://github.com/espressif/esp-idf/commit/f0c7d9b6c603658c832858d0a4f25b5a05ea1760).
- Commitmeldung: `fix(nvs_flash): Fixed order of page state change to allow recovery`.
- Der Commit verschiebt in `requestNewPage()` `markFreeing()` vor
  `activatePage()` und enthält den Recovery-Kommentar.
- Die lokale Analyse des exakten v6.0.2-Pins zeigt dieselbe wirksame
  Reihenfolge und denselben Kommentar. Ein Source-Diff zwischen dem
  `requestNewPage()`-Pfad des f0-Commits und dem v6.0.2-Pin ergibt dort keine
  weitere relevante Abweichung; die übrigen Unterschiede in der Datei
  betreffen andere NVS-Änderungen.
- Die lokale Provenienz zeigt `f0c7d9b6...` in der v6.1-Beta-Linie, während
  `7101770d...` der v6.0.2-Tag ist. Daraus wird keine lineare
  „neuere Version“-Beziehung abgeleitet.
- Ergebnis: bereits im gepinnten Verhalten enthalten/equivalent; **kein
  offener Lösungskandidat**.

### `0fa490524a73b5d6ae54506b74bb9ed7ae92d964`

- Offizielle Quelle:
  [Espressif-Merge `0fa490...`](https://github.com/espressif/esp-idf/commit/0fa490524a73b5d6ae54506b74bb9ed7ae92d964).
- Die Merge-Meldung nennt ebenfalls die Korrektur der Page-State-Reihenfolge
  für Recovery bei Power-Unterbrechung.
- Der offizielle Diff ist exakt derselbe `requestNewPage()`-Diff wie bei
  `f0c7d9b6...`; die lokale Commit-Provenienz weist `f0c7d9b6...` als
  Merge-Elternteil aus.
- Ergebnis: zusätzliche offizielle Provenienz derselben Änderung, aber kein
  weiterer Herstellerfix und kein neuer Vergleichskandidat.

### `a3e65eb7f8104a6ee3368bc0c3fceb8fe838a378`

- Offizielle Quelle:
  [Espressif-Commit `a3e65e...`](https://github.com/espressif/esp-idf/commit/a3e65eb7f8104a6ee3368bc0c3fceb8fe838a378).
- Dieser Commit ergänzt Purging von gelöschten Einträgen auf Namespace-Ebene,
  neue API-/Open-Mode-Semantik und zugehörige Tests.
- Er ändert nicht den für den Befund relevanten Recovery-Übergang
  `markFreeing()` → `activatePage()` → `copyItems()` und enthält keinen
  Nachweis, dass Callback 12 / `NotFound` dadurch den Old-or-New-Vertrag
  erfüllt.
- Ergebnis: funktional anderer NVS-Scope; kein belastbarer Kandidat für den
  aktuellen Recoverybefund.

### Offizielle Releases und Branchstände

Die offizielle Release-/Branchanalyse hat keinen weiteren eindeutigen
Herstellerstand mit exakter SHA ergeben, der

1. eine **weitere** Recovery-/NVS-Korrektur gegenüber dem bereits äquivalenten
   f0-/v6.0.2-Verhalten enthält und
2. den konkreten Callback-12-/`NotFound`-Befund im unveränderten
   Projektvertrag nachweislich behebt.

Der v6.0-Release-Branch bleibt eine mögliche spätere Vergleichsquelle, ist
aber ohne exakten ausgewählten Commit/Release und vollständige identische
Matrix kein Kandidat. Ein Branch- oder Release-Label allein ist kein
Recovery-Nachweis.

**Gesamtergebnis der Kandidatenprüfung:
`NO_PROVEN_MANUFACTURER_CANDIDATE`.**

## Erforderliche Vergleichsevidenz bei einem späteren Kandidaten

Falls der Owner später einen weiteren exakten Herstellerstand benennt, müssen
Phase B/C mindestens folgende Vergleichsfragen beantworten:

- identische vollständige Cut-/Szenariomatrix:
  `absent->empty`, `empty->binary`, `smaller->larger`, 32/33,
  4000/4001, 32813, 8240, `max->max` und vorbefüllter GC-Fall;
- jeder mutierende Write-/Erase-Callback einschließlich aller 4-Byte-Cuts;
- unverändertes Old-or-New-Orakel einschließlich `NotFound`- und
  `CommitOutcomeUnknown`-Semantik;
- per-Cut JSON-, FAIL- und Provenienzartefakte;
- exakte Source-/IDF-SHA und der konkrete Release-/Branchbezug;
- minimaler Reproducer und Callback-12-Ausgang;
- On-Flash-/Recovery-Verhalten, Page-State-/GC-/Erase-Folgen und
  Kompatibilität bereits geschriebener Daten;
- Build-, API-, Konfigurations-, Ressourcen- und Partitionskompatibilität;
- erforderlicher Abgleich der Projektpartition
  `state_store,data,nvs,0x9000,0x45000`.

Diese Liste beschreibt benötigte Evidenz für einen späteren Vergleich; sie
autorisiert weder Phase B noch eine Test- oder Dependency-Änderung.

## Empfehlung an den Owner

Phase A hat keinen belastbaren **weiteren** Herstellerpfad gefunden.
`f0c7d9b6...` und der offizielle Merge `0fa490...` sind dieselbe bereits
im gepinnten v6.0.2-Verhalten enthaltene Korrektur; der bestehende
Callback-12-/`NotFound`-Befund ist damit nicht gelöst.

Empfehlung:

- Issue #90 jetzt als `BLOCKED` führen;
- Phase B nicht freigeben und keine Host-Evidence-Foundation beginnen;
- keinen ESP-IDF-Pin und keinen Vendor-/Backport-Patch ändern;
- nur bei Vorlage eines neuen exakten, belastbaren Herstellerstands ein
  separates Owner-Subgate für dessen Vergleich beziehungsweise Zielstand
  eröffnen.

Der freigegebene R5.4-Plan bleibt unverändert auf
`c35ce0342898f0e19d3cce5e6a7eaa077f73bad6`. Phase B ist nicht vorab
freigegeben.


## Phase A.1 Korrektur: Multi-Page-BLOB-Cleanup und vollständiger Post-Pin-Sweep

### Anlass und Korrektur des ursprünglichen Befunds

Der ursprüngliche Phase-A-Bericht erklärte NO_PROVEN_MANUFACTURER_CANDIDATE,
ohne den später identifizierten offiziellen Cleanup-Fix
60561b6d08824608d2f5db89f33f71aad7258d5b vollständig zu prüfen. Dieser
Abschnitt korrigiert diese Lücke. Er ist Evidenz und Provenienz, keine neue
normative Persistenz- oder Testarchitektur. Der ursprüngliche Befund bleibt
als historische Ausgangslage erhalten und wird durch das nachfolgende
source-semantische Ergebnis ersetzt.

Die produktive Projektbaseline bleibt unverändert ESP-IDF v6.0.2 bei
7101770dc6db2667b3c477cc31365dd1acd6db4e. Die lokale v6.0.2-Installation
wurde nicht ersetzt oder aktualisiert; kein Projekt-Pin und kein Vendor- oder
Backport-Patch wurde geändert oder übernommen.

### Exakte Source-Provenienz des Pflichtkandidaten

Geprüft wurden:

- gepinntes nvs_storage.cpp:
  https://github.com/espressif/esp-idf/blob/7101770dc6db2667b3c477cc31365dd1acd6db4e/components/nvs_flash/src/nvs_storage.cpp
  mit Dateiblob 49883cf6a8103913ede837f11ad004c5d3eec724;
- Fix 60561b6d:
  https://github.com/espressif/esp-idf/commit/60561b6d08824608d2f5db89f33f71aad7258d5b
  mit korrigiertem nvs_storage.cpp-Dateiblob
  f1080d20b142641abdff67de867f07cf17b3d6fc;
- offizieller Master-Merge c8da362:
  https://github.com/espressif/esp-idf/commit/c8da36235184d2a57315eb363b8abb838d9a14a2;
- offizielle NVS-Hosttests am Fix:
  https://github.com/espressif/esp-idf/blob/60561b6d08824608d2f5db89f33f71aad7258d5b/components/nvs_flash/host_test/nvs_host_test/main/test_nvs_storage.cpp.

60561b6d entfernt in Storage::writeMultiPageBlob() die alte
TUsedPageList-/Page-Zeiger-Cleanup-Verfolgung. Der neue Pfad bestimmt die
geschriebenen Chunks aus chunkStart und chunkCount, sucht jeden Chunk nach
möglichem Page-Reclaim erneut über findItem() und ruft danach
eraseEntryAndSpan() auf. Der offizielle Hosttest deckt dabei auch die
Erhaltung des vorherigen Multi-Page-BLOB-Werts nach einem fehlgeschlagenen
Update ab. Der Fix wurde deshalb nicht allein wegen seines
ESP_ERR_NVS_NOT_ENOUGH_SPACE-Triggers ausgeschlossen.

### Exakte Release-/Backport-Provenienz

Die Source-Gleichheit und die Commit-Ancestry werden getrennt bewertet:

- Master-Fix: 60561b6d08824608d2f5db89f33f71aad7258d5b, gemergt mit
  c8da36235184d2a57315eb363b8abb838d9a14a2.
- Offizieller v6.0-Backport:
  daf4796ef66efa4311353d0b44021863eca2ee3e, gemergt mit
  7b3c4fca5b0d8391b177e8ca3a60ff3c85ebfc6c in release/v6.0.
- Offizieller v6.1-Backport:
  f26db8177ed0ad4f779f8ad4eee7ba74e66b9641, gemergt mit
  b136957d7b5df1731a812214c7fdeade64be5ec3 in release/v6.1.
- Exakte Merge-Seiten:
  https://github.com/espressif/esp-idf/commit/7b3c4fca5b0d8391b177e8ca3a60ff3c85ebfc6c
  und
  https://github.com/espressif/esp-idf/commit/b136957d7b5df1731a812214c7fdeade64be5ec3.
- Die v6.0- und v6.1-Backports sind separate Backport-Commits, nicht die
  Ancestry des Master-Commits. Die aktuell sichtbaren offiziellen Linien
  release/v6.0, release/v6.1 und master zeigen jeweils den identischen
  nvs_storage.cpp-Blob f1080d20b142641abdff67de867f07cf17b3d6fc; bewegliche
  Branchrefs werden nur als Discovery-/Kontextbeleg verwendet.
- Der unveränderliche offizielle Tag v6.1-rc1:
  https://github.com/espressif/esp-idf/releases/tag/v6.1-rc1
  enthält den v6.1-Backport; sein Zielcommit ist
  44f0c59f7c81a72a5868a52d5f6dfbbf88829704 und der nvs_storage.cpp-Blob ist
  ebenfalls f1080d20....
- Nach dem vorhandenen Tag v6.0.2 wurde kein stabiler v6.0.x-Tag mit diesem
  Fix gefunden; insbesondere existiert kein v6.0.3-Tag im geprüften
  offiziellen Tagstand. Die offizielle release/v6.0-Backport-Provenienz ist
  dokumentiert, ohne eine noch nicht veröffentlichte Release-Zuordnung als
  Tatsache zu behaupten.
- Der kleinste sinnvolle spätere Vergleichsstand wäre der exakte offizielle
  release/v6.0-Backport-Merge 7b3c4fca... beziehungsweise dessen exakter
  Patch daf4796e.... Eine produktive Verwendung bleibt einem separaten
  Owner-Subgate vorbehalten. v6.1-rc1 ist ausschließlich Vergleichsevidenz
  und keine Projektzielversion.

### Source-/Control-Flow-Analyse für 60561b6d

Der relevante Ablauf im Fix und im Projektbefund ist:

1. writeMultiPageBlob() schreibt neue BLOB_DATA-Chunks; bei Page-Grenzen kann
   page.markFull() den Page-State mit einem 4-Byte-Write ändern, danach kann
   requestNewPage() Reclaim/Copy auslösen. Der BLOB_IDX wird erst nach den
   Datenchunks geschrieben.
2. Der bestehende Callback-12-Befund verlässt den Pfad an dem mutierenden
   4-Byte-Write in Page::markFull(). Der markFull()-Fehler propagiert in
   writeMultiPageBlob(), das den Fehler- und Cleanup-Block tatsächlich
   erreicht, bevor Storage::writeItem() den Fehler zurückgibt.
3. Im neuen offiziellen Cleanup würden zunächst findItem()-Reads für die
   Chunk-Indizes ab chunkStart und anschließend eraseEntryAndSpan()-Mutationen
   versucht.
4. Das projektseitige StatefulBlockDevice modelliert den BDL-Cut an der
   Callback-Grenze: Der geschnittene Write/Erase wird nicht committed,
   powerCutTriggered bleibt aktiv, und danach liefern auch Reads, Writes und
   Erases ESP_ERR_FLASH_OP_FAIL. Damit können die nachfolgenden Cleanup-Reads
   die Chunks nicht mehr lokalisieren und keine Cleanup-Erases persistieren.
5. Der bekannte persistente Befund — bereits persistierte neue BLOB_DATA-
   Chunks, aber noch kein neuer BLOB_IDX, anschließend NotFound — entsteht aus
   den Mutationen vor dem geschnittenen markFull()-Write. Der neue Cleanup-Code
   kann in diesem BDL-Modell nach dem Cut keine weitere persistente Abbildung
   erzeugen.
6. Die dokumentierte alternative Harness-Reihenfolge, bei der der Fehler auf
   einen früheren 4-Byte-Cut wandert, hat dieselbe Grenze: Der Cut blockiert
   die nachfolgenden Cleanup-Reads/-Erases bereits. Auch dort kann
   60561b6d... die persistente Callback-12-/NotFound-Abbildung nicht
   verändern.
7. Der offizielle Regressionstest ist trotzdem fachlich relevant: Er prüft
   natürliche ESP_ERR_NVS_NOT_ENOUGH_SPACE-Fehler nach teilweise
   geschriebenen/verschobenen Chunks und die Erhaltung des alten BLOB-Werts.
   Er verwendet weder den projektseitigen permanenten BDL-Power-Cut noch das
   vollständige Projekt-Old-or-New-Orakel, die 4-Byte-Cut-Matrix,
   Callback-12-Semantik oder den Reboot-/Recoverypfad.

Die belastbare Klassifikation für den konkreten Projektbefund lautet daher:

CANDIDATE_SOURCE_SEMANTICALLY_RULED_OUT

Das bedeutet nicht, dass 60561b6d ein schlechter oder allgemein irrelevanter
ESP-IDF-Bugfix ist. Es bedeutet nur, dass seine geänderte Cleanup-Mutation
den konkreten Callback-12-/BDL-Power-Cut nicht mehr erreichen kann. Ein
separater Vergleich dieses Fixes ist für die allgemeine ESP-IDF-
Kompatibilitätsbewertung nachvollziehbar, aber er ist kein belastbarer
Herstellerpfad zur Behebung des hier beobachteten Befunds.

### Post-v6.0.2-NVS-Sweep

Die exakten offiziellen Linien wurden für nvs_storage.cpp, nvs_page.cpp,
nvs_pagemanager.cpp, relevante private Header und NVS-Hosttests nach
BLOB-/Multi-Page-, Update-, Cleanup-/Rollback-, Reclaim-/Copy-,
Recovery-/Power-Off-, Duplicate-, Page-State-, Erase-, BLOB_DATA-,
BLOB_IDX- und writeMultiPageBlob()-Berührung durchsucht.

Klassifiziert wurden:

- 60561b6d..., c8da362..., daf4796e..., 7b3c4fca...,
  f26db817... und b136957d...: derselbe geprüfte Multi-Page-BLOB-Cleanup-
  Fix; source-semantisch für den konkreten BDL-Cut ausgeschlossen wie oben
  beschrieben.
- 78e78d7996c744da90fee804442c87740bca747a, gemergt mit
  036cee4e6a99eee7ce5b75169211fe9d70b5d297:
  Page::cmpItem() initialisiert accumulatedCRC32 mit 0xffffffff und verwendet
  den Seed auch im ersten Chunk. Das korrigiert Read-/CRC-Vergleichsverhalten,
  ändert aber weder writeMultiPageBlob()-Cleanup noch Page-Reclaim, Page-State
  oder Power-Cut-Mutationsgrenzen; kein Kandidat für Callback 12.
- ff13da5e6c3e87ca4e7096b9c095fe1ad28b552d: offene NVS-Handles bei
  Partition-Deinit freigeben; API-/Lebenszykluskorrektur ohne BLOB-, Reclaim-,
  Recovery- oder writeMultiPageBlob()-Semantik.
- 1efc077ed9051f3f611cc41d90dda08ca1f49e1b: reine Dokumentationskorrektur
  eines nicht möglichen Returncodes von nvs_get_i8(); keine Persistenz- oder
  Recoveryänderung.
- a3e65eb7f8104a6ee3368bc0c3fceb8fe838a378 und die offiziellen Purge-Merges
  0fa490524a73b5d6ae54506b74bb9ed7ae92d964,
  dce25c5708c076c7db383d8f024fbdb0c92cce3c und
  c5309ede7d1e253d2085aeb07b808736fdc4194f betreffen Namespace-Purging und
  Erase-Policy. a3e65e... liegt bereits in der Ancestry des gepinnten
  v6.0.2-Stands; f0c7d9b6.../0fa490... bleiben die bereits im Bericht
  eingeordnete Page-State-Recovery-Provenienz und kein weiterer Fix.
- Die weiteren gefundenen BLOB-/Performance-/Overwrite-Änderungen liegen
  vor dem gepinnten v6.0.2-Stand oder betreffen nicht den untersuchten
  Fehler-/Recoverypfad. Für nvs_pagemanager.cpp und die relevanten privaten
  Header wurde nach dem Pin kein zusätzlicher, callback-12-relevanter
  Recoveryfix gefunden.

Damit wurden nicht nur 60561b6d..., sondern auch die späteren offiziellen NVS-
Änderungen auf dem untersuchten Pfad klassifiziert. Kein Sweep-Fund begründet
einen weiteren belastbaren Herstellerpfad für den konkreten
Callback-12-/NotFound-Befund.

### Aktualisiertes Gesamtergebnis und Empfehlung

- Kandidatenklassifikation: CANDIDATE_SOURCE_SEMANTICALLY_RULED_OUT für
  60561b6d...; f0c7d9b6... bleibt bereits im gepinnten Verhalten
  enthalten/equivalent; 78e78d... ist ein unabhängiger CRC-Readfix.
- Aktualisiertes Gesamtergebnis: NO_PROVEN_MANUFACTURER_CANDIDATE.
- Dynamischer Status: BLOCKED_NO_PROVEN_MANUFACTURER_CANDIDATE; der vorhandene
  Callback-12-/NotFound-Befund bleibt bestehende FAIL-Evidenz und wurde in
  dieser Korrekturrunde nicht neu ausgeführt.
- R5.4 bleibt unverändert auf
  c35ce0342898f0e19d3cce5e6a7eaa077f73bad6.
- Phase B, Host-Evidence-Foundation, Vergleichsbuilds,
  Dependency-/Pin-Änderungen, Vendor-/Backport-Patches,
  Produktions-/Harness-/Oracle-/Runneränderungen sowie Hardware- und
  Power-Cut-Tests sind weiterhin nicht autorisiert.

Nächster einziger Ownerentscheid: den korrigierten Phase-A.1-Bericht prüfen
und entscheiden, ob der dokumentierte Status
BLOCKED_NO_PROVEN_MANUFACTURER_CANDIDATE bestehen bleibt oder ein separates
Owner-Subgate für einen ausdrücklich benannten nächsten Schritt eröffnet
wird. Eine Agenten-PASS-Evidenz autorisiert keine Folgephase.
