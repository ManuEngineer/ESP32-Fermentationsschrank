# Issue #90 – ESP-IDF-NVS-Herstelleranalyse (Phase A)

## Ergebnis

- Phase-A-Ergebnis: `NO_PROVEN_MANUFACTURER_CANDIDATE`
- Empfohlener aktueller Status von Issue #90: `BLOCKED`
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
