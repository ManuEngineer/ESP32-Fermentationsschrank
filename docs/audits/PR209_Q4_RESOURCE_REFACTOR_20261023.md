# PR #209 – Q4-Ressourcen-Refactor: begrenzte Entscheidungsevidence

Aktueller Stand: [Head-Recycling und RAM-Duplikatbilanz unten](#head-recycling-und-priorisierte-ram-bilanz-2026-10-23).
Die folgende urspruengliche Refactor-Bilanz bleibt historische Ausgangsevidence.

**IMPLEMENTATION_READY=HOLD; FULL_CHAIN_RESERVE=NOT_PROVEN.** Planungsbasis
`c41a8076e9bafd5ab9040ba16b58200150dddf7b`, Produkt/Firmware unveraendert
`095f393f0eed4eb95944c164a8e967ce973c3fc9`. Verbindlicher Plan bleibt
[§13 des Gesamtplans](../tasks/issue-27-r1-web-remaining-scope-plan.md#q4-ressourcen-refactor-planungsdelta-2026-10-23).
Diese Evidence ergaenzt den [gemeinsamen Ownership-Prototyp](PR209_Q4_OWNERSHIP_PROTOTYPE_20261023.md);
keine Produktimplementierung oder semantisch verifizierte Alternative.

## Eine gemeinsame temporaere Variante

139 gemeinsame App-Quellen unter `/tmp/pr209-q4-resource-plan/src`, davon genau
fuenf Dateien gegen den Ownership-Prototyp geaendert. Der [Delta-Patch](PR209_Q4_RESOURCE_20261023.patch)
ist mit `git apply --unidiff-zero` ausschliesslich auf dessen rekonstruierte
Sourcekopie anzuwenden, nicht auf Produktdateien. [Quellmanifest](PR209_Q4_RESOURCE_20261023_SOURCES.tsv),
Bundle-SHA256 `2e896e0fa4d8b7febfbf81f11ee882c03ac977815589f490af0cdfea67795511`
(gleiche Namens-/NUL-/Byte-Rezeptur wie die Ausgangsevidence); Rekonstruktion PASS.
Alle Ownership-/Factory-/Web-Aenderungen sind gleichzeitig enthalten.

Je Profil elf gemeinsame App-TUs sowie 26 unveraenderte Partition-/SPI-Flash-/
HAL-TUs mit den jeweiligen Originalbefehlen aus `compile_commands.json`:
74 finale Compileobjekte insgesamt. Nur Source-/Outputpfade und gemeinsames
Include zuerst ersetzt, `-fstack-usage -fcallgraph-info=su` hinzugefuegt;
Originaloptimierung/ABI/Defines beibehalten. Keine Links oder Ausfuehrung.
[Exakte Befehle, Arbeitsverzeichnisse und Source-/Objekthashes](PR209_Q4_RESOURCE_20261023_COMMANDS.jsonl);
Response-Dateien/Toolchain wie in der Ausgangsevidence. Die zwei abschliessenden
Program-Model-Kompilierungen erhalten den fruehen Unsupported-Version-Return
vor jeder Value-Kopie. Bekannte Probe-Warnung: unbenutztes altes `findProgram`.

## Stack: Phasentrennung hilft, Storezweig bleibt zu eng

[Rohe neue Frames](PR209_Q4_RESOURCE_20261023_FRAMES.tsv),
[bekannte Pfade und Phasenarithmetik](PR209_Q4_RESOURCE_20261023_PATHS.tsv),
[konkrete moegliche Targets](PR209_Q4_RESOURCE_20261023_EDGES.tsv),
[verbliebene unbekannte Symbole](PR209_Q4_RESOURCE_20261023_UNKNOWN.tsv).
Beide Profile liefern dieselben hier genannten statischen Frames:

| Funktion | Ownership-Prototyp B | gemeinsame Ressourcenprobe B |
|---|---:|---:|
| persistCommandInternal, View / Value | 768 / 768 | 768 / 768 |
| writeSnapshot | 208 | 208 |
| writeSnapshotCore | 1168 | 224 |
| prepareSnapshotRecord | im Core | 128 |
| writeSnapshotTransaction | im Core | **976** |
| readProgram | 448 | 240 |
| migrateProgramToCurrentSchema / Into | 448 | Into32 |
| Migrationsschritt4→5 /5→6 | 240 / Value-Pfad | Into je32 |
| validateProgramCatalogPayload | 1088 | 1088 |

Der Core fuehrt denselben UTC-/Zeit-/Zaehler-/Highwater-/Fallback-Preflight aus.
Er ruft erst Record-Validierung/Encoding, danach die bisherige gesamte
PreparedHead→Slot→CommittedHead-Transaktion auf. Deren Reihenfolge und
Fehlerentscheidungen werden nicht ersetzt. Im Encoding liegen Headobjekte
nicht mehr darueber; beim Store bleiben aber **224+976=1200 B**, gegen zuvor
1168 B sogar32 B mehr. Internal768/Write208 wurden nicht wegoptimiert.

| Phase, nur gleichzeitig aktive bekannte Frames | berechnete Teilsumme B |
|---|---:|
| HTTPD-Prefix inkl. Taskwrapper/SDK/Adapter/Dispatcher/Handler | 1856 |
| Prepare / Confirm / Decision / Ack-Mute | 6016 /4288 /6144 /4816 |
| Run-Validierung, normaler malloc-Pfad | **7040** |
| Run-Validierung, konservativer ausgerichteter Heapzweig | 7072 |
| Run-Store/NVS/Flash, unten beschriebener moeglicher Zweig | **7760** |
| Settings, jetzt Factory-/Validierungszweig statt Migration | **6960** |

Run-Validierung normal:1856+1200+1312+48+768+208+224+1424=7040 B.
Run-Store:1856+1200+1312+48+768+208+224+2144=7760 B. Die2144 B beginnen bei
Transaction976, danach Storewrapper32/Exact64/NVS32. Der betrachtete
Set-Blob-Zweig vergleicht vorhandene MultiPage-Blobs, erreicht Page::findItem /
EraseEntryAndSpan / ReadEntry, dann NVSPartition::read32, PartitionRead48,
FlashRead64, StartRead32, StartCore64, Spi1Start32 und Cache-Unterpfade.
Das ist ein datenabhaengiger Vergleichs-/Bereinigungszweig, nicht jeder normale
Commit. Readback nach unbekanntem Write, andere Write-/Erase-/GC-Zweige und
jeweilige Fehlerrueckwege sind gesondert zu begrenzen, nicht dazu zu addieren.

Die Produktpartition `state_store` ist in beiden Profilen gemaess sdkconfig /
`partitions/issue_90_state_store.csv` unverschluesselt; der Composition Root
initialisiert normales NVS. Deshalb wurde der verschluesselte Mmap-Unterzweig
von PartitionRead/Write fuer diese Projektion ausgeschlossen. Keine globale
Streichung anderer indirekter Ziele. Page→NVSPartition und moegliche generische
Flash-/Default-API-Targets wurden quellbezogen ergaenzt. Chip-/Host-/OS- und
ROM-Tabellen sind damit **nicht** vollstaendig aufgeloest.

Settings:1856+5104=6960 B; unter CatalogValidator1088 liegen jetzt
FactoryProgramCatalog::programs256, MakeFactory256 und Vektor-/Allocatorpfade.
Die bisherige Kopier-Migrationskette ist kein zusaetzlicher gleichzeitiger
Aufschlag mehr; ein anderer Zweig dominiert. Vier Factorydokumente und deren
Unterpuffer bleiben vorhanden. Post-Commit-Pruefung/Move und Token-Cleanup
laufen nach Rueckkehr von Write/Core; deren Frames werden nicht doppelt addiert.
Ihre Destruktor-/Freigabe-/Fehlerunterpfade bleiben ebenfalls NOT_PROVEN.

**Keine dieser Summen ist eine Vollketten-Obergrenze oder HWM.** Unbekannte
libstdc++-String-/Destruktor-/Aliasziele, Envelope-Encoder-Unterpfade, indirekte
Flash-/ROM-/Hostziele, Port/Assembler, libc, OOM/Abort und geschnittene Zyklen
bleiben NOT_PROVEN. Die mechanische Pfadauswahl verwendet nur bekannte
Frameanteile; fehlende Symbole bekommen keine Ressourcenfreigabe. Der explizite
HTTPD-Prefix vermeidet weiterhin die falsche Accept-/Session-Callback-Kombination.

Planungsziel: vollstaendiges S_Run und S_Settings jeweils **≤7680 B**, also
mindestens **512 B Reserve** im unveraenderten8192-B-Stack. Das ist ein kleiner
6,25%-Planungsboden fuer Frame-/Callsite-Abweichungen: zweimal der gerade
beim Aufloesen des bekannten NVS-Read-Unterpfads sichtbar gewordene Zuwachs
von256 B (848→1104 B). Das ist eine begrenzte Sensitivitaetsreserve nach
Pfadabschluss, keine Ersatzpauschale fuer unbekannte SDK-Frames oder
Interrupt-/Hardwareevidence. Alle erreichbaren Unterpfade muessen vorher
in S enthalten sein; gezielter Review muss die Reserve bestaetigen.
Schon7760 B lassen nur432 B, **80 B weniger als der Reserveboden**, bevor
unbekannte Anteile geklaert sind. Daher STOP, kein IMPLEMENTATION_READY.

## Gesamt-RAM: weniger Bytekopien, kein bewiesener Peakgewinn

Keine neuen Felder/Owner gegen den Ownership-Prototyp; Coordinator weiterhin
21352 B inklusive WorkingSet12752 B, nicht zusaetzlich addieren. Current4896 B
bleibt separat. View536 B inklusive Guard216 B und Token/optional4/8 B bleiben
Stackwerte. Q3-Scratch1856 B / Alignment8 / konservativ1936 B bleibt fuer
Settings unveraendert. Run-/Settings-Requests auf demselben HTTPD-Dispatch
sind verschiedene Phasen; daraus folgt keine globale Exklusivitaet aller
Konfigurationsgenerationen/anderen Tasks. Keine neue grosse feste Allokation,
kein neuer fester Block, keine erneute ABI-Messung dieser unveraenderten Owner.

P=Payloadlaenge≤8192, R=neuer Record≤8240, O=alter physischer Slot≤8240,
U=Readback≤8240, H=Head≤256. **Logische Byteinhalte**, keine Kapazitaets- oder
Allocatorobergrenzen und keine unabhaengig erreichten Maxima:

| gleichzeitig lebende Teilflaechen | Produkt095f393 | vorgeschlagene Variante |
|---|---|---|
| Decision/Request | volle before/after-/Visitor-/Factorykopien; zwei Webresultate | vorhandener Kandidat, Guard/View, ein Webresultat; keine neue volle Statekopie |
| Encoding | PayloadP + EnvelopekopieP + EncoderneubauR + alter Output; Snapshot-Unterpuffer | Payload nach Envelope verschoben; EncoderneubauR neben altem WS-Record; Snapshotkopie bleibt |
| Slot-Write/Unknown-Readback | 2P+2R+2O+U, zusaetzlich Heads/Snapshots/residente Slots | R+O+U; Payload/Encoder bereits zerstoert, physisches OLD nach oldSlot verschoben |
| Slot-Adoption nach Written | statt U ggf. neue Slot-Kapazitaet neben alter Slotbelegung | dito; Record→Slots bleibt notwendige Kopie, kein Move aus noch benoetigtem Record |
| Head/Abschluss | prepared/committed und mehrere Bytekopien bis Core-Ende | beide Headwerte bleiben; preparedBytes als OLD, committedBytes erst nach Write verschoben; keine zweite identische Slotzuweisung |
| Settings | Graphmodelle/Preview/CommitScratch, geladener Record samt Payload, Factoryarray, Migration-/Katalog-/Setpuffer | dieselben Owner/Puffer; nur unpublished ProgramCandidate statt weiterer Migrationskopien |
| Fehler/Indeterminate/Cleanup | alte Current-/Slot-/Head-Evidenz und unbekannter Readback bleiben bis Entscheidung | OLD/NEW bis writeExact-Rueckkehr behalten; Candidate-Cleanup erst danach; kein frueher Current-Apply |

Die grob getrennte Slotphase ergibt bei den Formatlimits bis57584 B logische
Teilpuffer im Produkt gegen24720 B in der Variante (U bzw. getrennt spaetere
Slotkopie); **keine** gemessene Gesamtheapdifferenz von32864 B. Hinzu kommen in
beiden Faellen residente Current-/Slots-/Head-/Konfigurationsunterpuffer,
Request-/Programmstrings, Vektorkapazitaeten, Snapshot-/Record-Projektionen,
Heads, NVS-Page-/Hash-/GC-/Flashscratch, Serializer und Allokatorverwaltung.
Wiederbelegung kann alte und neue Kapazitaeten ueberlappen lassen. Ablehnung
setzt candidate zurueck, Fehlercleanup/Destruktoren haben eigene Lebensdauern.
Nur Source-Kopierstellen wurden entfernt, kein gemessener Allokatorpeakgewinn.

Groesster **neuer fester** Block0 B; groesster insgesamt erforderlicher
zusammenhaengender dynamischer Block weiter NOT_PROVEN. Konkrete bestehende
Requests: ByteWriter reserviert8192 Zeichen (mit NUL mindestens8193 B);
Settings-Read erlaubt32768+45 B Record, also bei diesem Eingang mindestens
32814 B fuer einen String einschliesslich NUL. Beim Envelope-Decoding koennen
Record und Payload nebeneinander liegen (bis32813+32768 logische Bytes), auch
fuer spaeter verworfene/defekte Daten. Diese Limits begrenzen weder
Stringkapazitaet/Realloc noch NVS-Scratch. Historische H-RAM-1-Werte und
F4-13312/16640 B garantieren solche Bloecke nicht; F4 bleibt PROVISIONAL.

**Absolute/differentielle Peak-RAM- und groesster-Block-Obergrenzen NOT_PROVEN.**
Standardcontainer/ByteWriter unter -fno-exceptions liefern hier keinen
allgemeinen falliblen OOM-Return; ein ResourceUnavailable-Enum allein behebt
keinen abortenden String-/Vektorrequest. Kein ehrliches Unavailable-/Replay-
PASS ohne gezielte Fehler-/Allokationspfadpruefung. D9/reale Heap/HWM bleiben
NOT_RUN/NOT_PROVEN. Produkt-RAM-Aenderung durch diesen Auftrag0 B.

## Abschluss dieser Planung

Eine Variante ist konkret abgegrenzt, aber Storeframe976 B plus die weiterhin
aktiven224/208/768-B-Aufrufer verhindern bereits im bekannten Pfad den
Reserveboden. **Einzige technische Owneroption:** innerhalb desselben
Ressourcen-Refactors die gleichzeitigen Prepared-/Committed-Head-Valueobjekte
auf einen phasenweise verwendeten Headwert begrenzen; beide kodierten NEW-/
OLD-Headbytes muessen vor dem ersten Write bereitstehen und erhalten bleiben.
Das braucht eine explizite Head-Lebensdauergrenze, keinen zweiten Schreibkern.
Einsparung/Restpfade/Heap sind noch nicht bewiesen; keine automatische weitere
Probe oder Produktumsetzung. Ohne diesen erweiterten Scope bleibt HOLD.

PASS: beide finalen Compilerprofile, Patchrekonstruktion, Hash-/Frame-/
Phasenarithmetik und Dokumentationsabgleich. Native-/Golden-/Heaptrace-/
HWM-Tests nicht ausgefuehrt; Produkt-/Testquellen und Firmware unveraendert.

## Head-Recycling und priorisierte RAM-Bilanz (2026-10-23)

**Aktueller Entscheid: IMPLEMENTATION_READY=HOLD; FULL_CHAIN_RESERVE=NOT_PROVEN.**
Auftrag/Basis `a38ba893091350f1c325b0fc379f25883d87703c`. Die vorstehenden
Abschnitte sind die historische Ausgangsbilanz. Produkt/Firmware095f393,
Q3 und Budgets bleiben unveraendert. Eine einzige fortgesetzte Quellvariante,
keine neue isolierte Optimierungsreihe oder Produkt-/Testcodeaenderung.

### Ein Speicherplatz, zwei notwendige Bytefolgen

`writeSnapshotTransaction` verwendet genau
`std::optional<RunPersistenceHead> headStorage{std::in_place}`. Normaler
Commandpfad: Prepared befuellen und unveraendert encodieren; Byteergebnis
besitzt seinen Puffer; `reset()`/argumentloses `emplace()` bauen Committed
in **derselben** Speicherflaeche auf. Keine Headreferenz ueberlebt den Reset.
Ein eingefrorener8-B-Wert `firstHeadRevision` erhaelt die zuvor im Prepared
gehaltene Revision: Committed bekommt exakt diesen Wert+1, keinen erneut
gelesenen Coordinatorzaehler. Der bestehende Preflight prueft Overflow.
Der ohnehin vorhandene konstante40-B-Checkpointverweis bleibt. Keine zweite
vollstaendige Headkopie oder zusaetzliche Metadatensammlung.

PreparedBytes und CommittedBytes sind weiterhin **unterschiedliche owning
Strings**, beide vor dem ersten Storewrite vorhanden. Selbst wenn Prepared-
Encoding scheitert, wird Committed wie vorher noch encodiert; die bisherige
Fehlerstufenauswahl bleibt. Der periodische Pfad benoetigt wie vorher nur
Committed. Physisches OLD, altes Head-OLD und unabhaengiges Unknown-Readback
bleiben. Die Storeaufrufe referenzieren Bytefolgen, nicht den recycelten
Prepared-Wert. Prepared→Slot→Committed, fruehe Slotadoption, Zaehler,
Durability/Indeterminate, Post-Write-Guard und RAM-Apply bleiben derselbe
Kern. Committed wird erst nach Written nach currentHead verschoben.
Das ist ein quellseitiger Vertragsabgleich, **kein semantischer Test-PASS**.

### Gemeinsamer finaler Compiler-/ABI-Nachweis

Quelle `/tmp/pr209-q4-head-recycling/src`,139 Dateien, genau eine weitere
Dateiaenderung gegen a38ba893-Prototyp. [Delta-Patch](PR209_Q4_HEAD_20261023.patch)
nur auf dessen rekonstruierte temporaere Kopie, `git apply --unidiff-zero`.
[Quellmanifest](PR209_Q4_HEAD_20261023_SOURCES.tsv), Bundle-SHA256
`4289b3ac95178c3c267e158583cfe6cc379915537396c955cb1d7c4d014f70fc`.
Alle Borrow/Guard/View-/Factory-/Web-/Record-/Settings-Migrationsaenderungen
sind gleichzeitig enthalten. Je Profil elf App-/Konsumenten-TUs mit echten
Originalflags erneut kompiliert;22 finale TU-Objekte plus zwei ABI-Objekte.
[Exakte Befehle/Objekthashes](PR209_Q4_HEAD_20261023_COMMANDS.jsonl).
Unveraenderte SDK-/Adapter-/Codec-Evidence aus den vorigen Messungen wird
hashgeprueft wiederverwendet; keine angeblich neu ausgefuehrten SDK-/Firmwarebuilds.

[Target-ABI](PR209_Q4_HEAD_20261023_ABI.tsv) aus sizeof/alignof-Konstanten in
Xtensa-Objekten, [Probequelle](PR209_Q4_HEAD_20261023_ABI.cpp.txt), nicht Host64.
Head320 B/Alignment8, optionaler Head328/8, optionale Stringrueckgabe28/4.
Zwei Headwerte640 B werden durch328+8=336 B ersetzt:304 B weniger lokale
Objektflaeche. [GIMPLE-Deklarationen](PR209_Q4_HEAD_20261023_GIMPLE.txt) zeigen
nur diesen Headowner; weitere Headnamen sind Pointer/Referenzen, die drei
Codec-Rueckgabetemporaries sind optionale Strings, keine versteckten Headwerte.
Der finale Stand friert die Revision explizit ein; ein Zwischenstand ohne
8-B-Freeze mit656-B-Frame ist **nicht** die hier bilanzierte Quelle.

| gemessener Frame, beide Profile | a38ba893 B | final B |
|---|---:|---:|
| Transaction | 976 | **672** |
| Core / Record-Prepare / Internal / Write | 224 /128 /768 /208 | unveraendert |
| Application / Web / Handler | 1312 /1200 /544 | unveraendert |
| Head-Defaultkonstruktor / Reset | keine Gutschrift | je32 |
| Settings ReadProgram / CatalogValidator | 240 /1088 | unveraendert |

[Rohe gemeinsame Frames](PR209_Q4_HEAD_20261023_FRAMES.tsv) enthalten auch
Value/View und alle sieben Web-Decisionkerne: Program288, Manual320,
Stop/Completion256, Adjustment416, Ack/Mute48 B; Visitoren32 B.
[Phasenpfade](PR209_Q4_HEAD_20261023_PATHS.tsv),
[unaufgeloeste Symbole je erreichbarer Wurzel](PR209_Q4_HEAD_20261023_UNKNOWN.tsv).

| berechneter gleichzeitig aktiver bekannter Pfad | final B | Aenderung zu a38ba893 |
|---|---:|---:|
| Run-Store/NVS/Flash | **7456** | **−304** |
| Run-Recordvalidierung normal / ausgerichtet | 7040 /7072 | 0 |
| Head-Encoding, bekannte Anteile | 6560 | kein Obergrenzennachweis |
| Post-Commit-Guardvalidierung, ohne folgenden Move | 5344 | kein Obergrenzennachweis |
| Settings-Factory-/Validierungszweig | **6960** | 0 |
| Prepare / Confirm / Decision / Ack-Mute | 6016 /4288 /6144 /4816 | 0 |

Store7456=Prefix1856+Web1200+App1312+Borrow48+Internal768+Write208+Core224
+Transaction-/Store-/NVS-/Flashunterpfad1840. Head-Encoding und Write/Readback
sind **verschiedene** Phasen; Write/Core sind beim Post-Commit-Check bereits
zurueckgekehrt. Cleanup hat einen bekannten Release-Unterpfad352 B, jedoch
keine abgeschlossene Destruktor-/Freigabeobergrenze. Der echte kontextbezogene
HTTPD-Prefix und die unverschluesselte Produktpartition bleiben wie oben.
Keine Addition isolierter Probengewinne.

8192−7456=736 B Rest; gegen den unveraenderten512-B-Reserveboden bleiben
**224 B**, Settings hat entsprechend720 B oberhalb des Bodens. Fehlende
Envelope-/String-/Alias-/libc-/Port-/ROM-/indirekte Flash-/NVS-/Readback-/OOM-/
Abort-/Cleanupziele und geschnittene Zyklen muessen erst vollstaendig begrenzt
werden. Insbesondere encodeHead160 B allein begrenzt dessen Envelope- und
Allocatorzweige nicht. Unknown bleibt NOT_PROVEN, keine0-B-Freigabe.

**Das Head-Recycling beseitigt den zuvor bekannten Reservefehlbetrag. Eine
vollstaendige Loesung des8-kB-Problems ist damit nicht belegt:** nur224 B
zusaetzlicher Spielraum vor unbekannten Anteilen erlauben keine belastbare
Vollkettenempfehlung. Keine Behauptung,8 kB seien prinzipiell unmoeglich;
keine HWM/Hardware-/OOM-/Heap- oder semantische Qualifikation. Keine weitere
rein diagnostische Variante aus diesem Auftrag.

### Hoechstens fuenf priorisierte Massnahmen / Duplikate

[Vollstaendige kurze Bilanz mit Besitzer, Quellstelle, Groesse, Lebensdauer,
Safetybedarf, Vermeidbarkeit, Nettoeffekt und Verifikation](PR209_Q4_HEAD_20261023_RAM.tsv).
Prioritaet ist eine Arbeitsrangfolge nach plausiblem gleichzeitigen Peak und
verbindlichem Stackblocker, keine Messung eines absoluten System-RAM-Peaks:

1. **Settings Read/Decode, hoechste Heapprioritaet.**
   `scanGroupMetadata` haelt bei gleicher Revision `read.value` und den erneut
   gelesenen `previous.value` bis zum **exakten** Bytevergleich. Bei den
   Eingangsgrenzen sind das2×32813=65626 logische Zeichen, mindestens zwei
   Requests zu je32814 B inklusive NUL, selbst bei erst spaeter semantisch
   verworfenen Daten. Dieser Zweig ersetzt Identitaet nicht durch CRC-Gleichheit.
   Separat entstehen in `loadReferencedRecord`/`decodeEnvelope` R+P bis
   32813+32768=65581 logische Bytes. Die beiden Phasen **nicht addieren**.
   Vor der Factoryvalidierung wird der rohe Record bereits per leerem Swap
   freigegeben: positives vorhandenes Lifetime-Management.
   Der Metadata-Scan materialisiert selbst keine Payload. Ein unbenutztes
   `validateStoredReference`-Template mit Bytekopie wird nicht als aktiver
   dritter grosser Puffer gezaehlt. Kleiner Follow-up: konsumierendes Envelope-
   Decode nur fuer den semantischen Lader, derselbe Header-/CRC-/Referenzkern;
   Payloadpuffer bis32769 B Request einsparbar, groesster Readblock und2R-
   Kollisionsphase bleiben! Q3-Scratch1856=Graph552+PrepareResult1304 B bleibt.
   Prepared1288 B enthaelt Graphmetadaten/shared_ptr; nicht jeden Graph als
   erneuten Vollkatalog zaehlen. Factoryarray800 B und gestreamtes Program200 B
   koennen neben Payload/Runtime/Preview leben; Kapazitaeten/Totalpeak NOT_MEASURED.
2. **Run-Decision/ActiveRun, verbindlicher Stackblocker.** Produkt besitzt
   owning before/after in Decision10104 B, weitere Domain-/Visitor-/Factory-
   Value-/Optionalflaechen. Der Prototyp ersetzt nur den privaten Webpfad durch
   vorhandenen4896-B-Kandidaten, View536 inklusive Guard216 und direkte
   ActiveRun-Konstruktion (ActiveRun2816/optional2824 B). Kein grosser neuer
   Heapowner, externe Value-APIs bleiben. Current/Guard sind notwendige
   Safety-/Stale-Evidenz. Stackverbesserung ist gemeinsam gemessen; dynamische
   Run-/Programmstrings, Vektoren und Ruecksetz-/Movepeaks NOT_MEASURED.
   Massnahme bleibt die bereits gebaute gemeinsame Ownershipvariante, keine
   globale Abschaffung von before/after oder weitere Probeserie.
3. **Run-Record/Head/Projektionen.** Payloadreserve8192 B ist auch bei kleinem
   Payloadinhalt vorhanden; in095f393 haelt der Core sie waehrend des Stores.
   In der gemeinsamen Variante endet Encoding samt Payload vor der Storephase.
   Payload→Envelope und Record-/OLD-Kopien sind bereits wie oben reduziert;
   physisches OLD, NEW, Readback und zwei Headbytes bleiben notwendig.
   Head-Recycling spart final304 B Stack gegen a38ba893, fester Heap0 B neu,
   dynamischer Headheap0 B zusaetzlich. WS-Snapshot3904 und Record3952 B samt
   Slots bleiben getrennte Owner; ihre Programm-Unterpuffer sind noch Kopien.
   Kein pauschales Freisprechen der12752-B-WS-Flaeche. Ein Zusammenschieben
   der Projektionen benoetigte alle Recovery-/Load-/Fallback-Lebensdauern und
   ist jetzt nicht umgesetzt. Konkreter begrenzter Ansatz fuer Stackscope A
   unten: bestehendes256-B-IDfenster im Snapshot statt lokale zweite IDliste.
4. **UI-/Web-Praesentationskatalog, separates Follow-up.**
   `uiPresentationSource` kopiert den ganzen ProgramCatalog, auch fuer einen
   Web-GET eines einzelnen Programms. Cache-Refill haelt alten und neuen
   Katalog bis zur erfolgreichen Uebernahme; ein Webread kann daneben eine
   weitere Kopie besitzen. Je Katalog bis16×200 B logische Document-Slots,
   Benutzer-Notes bis12×1024 B plus Factorynotes/Namen/IDs/Stufen/Kapazitaeten;
   kein daraus addierter angeblich gemessener Peak. Snapshot464 B,
   ProjectionInput272 B, PresentationSource96 B. `published_=snapshot` ist
   die unabhaengige semantische Revisionsbasis, kein blind entfernbarer Cache.
   Massnahme nur als Follow-up: fuer synchrones Web-Read/Encode bestehenden
   Runtime-Lease statt Vollkopie bewerten; keine neue langlebige Lease, die
   Settingsgenerationen festhaelt. Ein Katalog weniger koennte dynamischen
   Heap sparen, Totalpeak wegen Generationslebensdauern NOT_PROVEN. Touch-
   Cache/Locale/Revision/Refillvertrag bleibt. Positiv: projectInto recycelt
   Vektoren/Run-ID/Programmname, Application den ProjectionInput, HeaderNetwork
   evictiert die Katalogkopie bereits vor dem Commit. Das nicht zurueckbauen.
5. **Web Prepare/Confirm, bereits gemeinsam reduziert.** Produkt zwei856-B-
   Resultate im2048-B-Webframe; Variante ein Result und In-place-Confirm im
   1200-B-Frame, PreparedRequest824 B. Requestprogramm und seine Strings/
   Vektoren leben neben Runtime/Kandidat, keine unbewiesene0-B-Dynamik.
   Kein zusaetzliches Delta gegen a38ba893. Nur die zweite Resultatkopie und
   grossen Factorytemporaries entfallen; die bestaetigte Requestidentitaet
   darf unter heutiger synchroner API nicht beliebig frueh freigegeben werden.
   Value-Confirm/Revalidation/Origin/Replay bleiben; keine weitere Request-
   Fortsetzungsarchitektur. Verifikation: gezielte Identitaets-/Stale-/Replay-
   Aequivalenz und Peaktrace von Decode bis Cleanup.

**Gesamtbilanz/OOM:** neuer fester Heap/Block0 B gegen a38ba893; Coordinator
21352 B unveraendert. Keine neue allokierende Head-Metadatenstruktur. Der
groesste insgesamt benoetigte zusammenhaengende Block und absoluter/
differentieller Allocatorpeak bleiben NOT_PROVEN. Die nachgewiesene moegliche
32814-B-Stringanforderung ist eine Untergrenze fuer diesen Eingang, keine
allgemeine Kapazitaetsobergrenze. F4/H-RAM-1 sind keine Reservierung. Bei
-fno-exceptions sind String-/Vektor-/ByteWriter-Requests nicht allgemein
fallible Statusreturns; ResourceUnavailable heilt keinen abortenden Allocator.
Ein ausbleibender Write vor OOM ist kein Persistenzerfolg. OOM nach
Prepared/Slot/Committed kann bereits dauerhafte Bytes hinterlassen; weder
Unchanged noch ein neuer Versuch darf daraus gefolgert werden. Bestehende
Recovery-/Durabilityregeln bleiben; DecisionOnly nur vor Owner-Eintritt,
danach keine Umdeutung echter Owning-/Indeterminate-Ergebnisse. Status-/Replay-
Vertrag bleibt Vorschlag; keine neue Enum, keine automatische Wiederholung.

### Stop und zwei bezifferte Scopeoptionen fuer den Owner

**A – priorisiert fuer Run:** bestehende Persistenzphasen/Parametergrenze
verkleinern: vorhandenes Snapshot-IDfenster256 B statt lokaler zweiter32-ID-
Liste, danach IDs nur nach durablem Write **und** erfolgreichem Post-Check
uebernehmen. Writewrapper208 B durch schmale referenzierte Parametergrenze
im selben Kern begrenzen, keine neue grosse Ownerflaeche. Messbare spaetere
Zielwerte Internal≤512 statt768 B, Write≤80 statt208 B; **unbewiesene Ziele,
keine verbuchten Gewinne oder neue kombinierte Kettensumme**. Stackpotential
ist durch das nachgewiesene256-B-Objekt konkret, Compilerresultat offen;
fester/dynamischer Heap0 neu beabsichtigt. Risiko: Aliasing beim Snapshotreset,
Persisted-ID-/Fehler-/Post-Commit-Vertrag; keine Anwendung auf sich selbst.
Erfordert einen zusammenhaengenden Implementierungsscope mit abschliessendem
Vollketten-/Semantik-/Heapgate, keine weitere isolierte Compiler-Probeserie.

**B – hoechste Heapprioritaet, separater Settingsscope:** den nur semantisch
benoetigten Record im bestehenden Envelopepruefkern konsumieren statt eine
zweite Payload zu allokieren. Potentiell ein Request bis32769 B weniger in
**dieser** Decodephase, fester Heap0 neu; Stackeffekt NOT_MEASURED. R bleibt
bis32813 Zeichen gross;2R fuer exakte Identitaetskollision bleibt unter der
heutigen Ganzrecord-Read-API. Daher keine behauptete Loesung des globalen
Settingspeaks oder des Run-Stackgates. Risiko: rohe Bytebeweise an falscher
Stelle zerstoeren; consuming nur nach Header/CRC/Referenzpruefung, nicht fuer
OLD/NEW/Root-/Indeterminate-/Recovery-Vergleiche. Kein paralleler Codec,
Buffer-Manager oder geaendertes Format. Beide Optionen sind Entscheidungen,
keine Freigabe weiterer Diagnoseiteration oder Produktimplementierung.

Spaeter nur unmittelbar betroffene Regressionen gemaess §13: Prepared- und
Committed-Goldenbytes und jeweilige Encodefehler **vor jedem Write**;
periodisch/Command/Transition/Recovery/Fallback ohne Public-API-Bruch;
OLD/NEW/Absent/Other/Readback-/Writefehler an jeder Stufe mit identischer
Durability/Recovery; Write-before-Apply, Post-Commit-Stale/Safety und
CommittedApplyFailed; Value/View/Borrow/Reentranz/Replay. Differentialtraces
muessen Allokationsgroessen/-anzahl, Kapazitaet+NUL, groessten Request,
Allocatorverwaltung und Livepeak bis Fehlercleanup einschliessen, gerade
Katalogduplikate und konsumierendes Decode. Keine dieser Semantik-/Heap-/
OOM-/Golden-/HWM-Pruefungen jetzt ausgefuehrt. Compile-/Target-ABI-/Hash-/
Patch-/Frameparitaets-/Arithmetik-/Linkpruefung PASS ist davon getrennt.
**STOP; keine zusaetzliche Produktimplementierungsfreigabe und keine weitere
rein diagnostische Iteration ohne neuen Ownerentscheid.**
