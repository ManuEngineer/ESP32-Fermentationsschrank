# PR #209 – Q4-Ressourcen-Refactor: begrenzte Entscheidungsevidence

Aktueller Stand: [Q4 Option A, Produktimplementierung mit nachgewiesenem Value-Fallback-Stackblocker](#q4-option-a-produktimplementierung-2026-10-23).
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

## Q4 Option A: Produktimplementierung (2026-10-23)

**Ergebnis: implementiert; HTTPD_FULL_CHAIN_STACK=FAIL, C5_RESOURCE_GATE=HOLD.**
Ownerfreigabe exakt `623bc4520bcf3601d3666e7c717ae00213047901`.
Produktcommits `832075fcc22b2ed4d98e907ef197e3a6e358ecf8` und
`e31e952def3264ca419187b86a2988bd20e3a487`; letzterer ist die tatsaechlich
gebaute Produktbasis. Vergleichsprodukt `095f393f0eed4eb95944c164a8e967ce973c3fc9`.
Die spaeteren Test-/Markdown-Nachtraege aendern diesen Produktcode nicht.

**Wesentlicher Reachability-Befund:** Die bisherige Annahme, der Webpfad
koenne nur die sieben umgestellten Decision-Varianten erreichen, ist falsch.
`web_json_codec.cpp::readIntent` akzeptiert auch `recovery-time-correction`
und `sensor-selection`; der bestehende Test
`test_mutation_codec_decodes_every_closed_application_intent` bestaetigt dies.
`prepareAdditionalEnvelope` erzeugt diese beiden Requesttypen,
`confirmPreparedInPlace` bestaetigt sie; `applyConfirmedPrepared` verzweigt
**vor dem Borrow** in `applyConfirmedPreparedValue`. Der neue native
Dispatcher-/Prepare-/Confirm-/Replaytest weist genau diese Kette nach,
einschliesslich normaler Ablehnung in Standby. Ein negativer Domainausgang
verhindert den Eintritt in den grossen Frame nicht.

In **beiden echten Profilen** misst der Value-Fallback **10880 B statischen
Xtensa-Frame**, allein bereits groesser als8192 B. Nachgewiesene gleichzeitig
aktive Eintrittskette: HTTPD/Adapter/Dispatcher/Mutation1856 + Web1200 +
Application1312 + Value10880 = **mindestens15248 B**, noch ohne dessen Domain-,
Persistenz- oder SDK-Unterpfade. **7056 B ueber Budget; 7568 B ueber dem7680-B-
Planungsziel.** Deshalb ein statisch nachgewiesenes FAIL, unabhaengig von den
weiteren unbekannten SDK-/ROM-Anteilen. Der Ausschluss der Value-Kante ist
nur fuer die explizit eingeschraenkte Sieben-Varianten-Projektion zulaessig,
nie fuer den gesamten Web-Runpfad. Die historischen Proben oben erbringen
keinen Vollkettennachweis. Hier keine Produktkorrektur an diesen weiteren
Requesttypen, kein Abschalten bestehender Routen und keine Scopeausweitung.

### Tatsächlich umgesetzter Besitz- und Datenfluss

- Ein instanzlokaler, nicht kopierbarer `CommandOperation` leiht ausschliesslich
  `workingSet_.candidate` des privaten Application-Coordinators. Zweiter Borrow,
  fremdes Token, weitere mutierende Coordinator-Eingaenge und rekursives
  Application-Begin/Factory-Reset werden vor Kandidatenzugriff abgewiesen;
  dieselbe Operation darf den privaten Persistenzkern fortsetzen. Andere
  Coordinator-Instanzen bleiben unabhaengig. Das ist ein operationeller
  Reentranzschutz unter dem bestehenden Application-Serializer, keine neue
  globale Sperre oder allgemeine Thread-Safety-Garantie fuer direkte Aufrufer.
- `CommandApplyGuard` friert die tatsaechlichen Apply-Praedikate ein:
  ProcessRuntimeState, Command-/Run-/Message-/Faultrevision, Critical-Safety,
  bounded Run-ID, aktiver Sensormodus, SensorRuntime-/persistierter Auswahlzustand
  und Recoveryrevision. Kein `before=current`-Selbstvergleich. Vor Write nur
  validieren; den aufgebauten Kandidaten nicht erneut auf sich anwenden.
  Nach durablem Write den unveraenderlichen Guard erneut pruefen; bei Safety/
  Stale bleibt `PersistenceCommittedApplyFailed` samt Changed-Durability.
  Erst danach RAM-Move und IDuebernahme.
- Die sieben Into-Entscheidungen teilen Fachkerne mit den Value-APIs.
  Ablehnung/Unconfirmed restauriert die owning `after`-Projektion ohne Verlust
  von Summary/Notice/Effects. `ActiveRun::startInto` konstruiert direkt am
  Ziel; bestehende Start-Value-APIs bleiben erhalten. Ack/Mute bleibt RAM-only.
  Kein neuer Persistenzkern und keine zweite State Machine.
- Nur der private Borrow-Pfad benutzt das vorhandene 256-B-IDarray in
  `workingSet_.snapshot`. `resetRunPersistenceSnapshotForMake` leert es nicht;
  `makeRunPersistenceSnapshotInto` darf es deshalb auf sich kopieren.
  Kandidaten-IDs werden nicht vorzeitig zu processed/persisted IDs im
  autoritativen Zustand. Das Array ueberlebt Write/Readback und Post-Check;
  andere Snapshot-schreibende Eingaenge sind im Borrowfenster gesperrt.
  Direkte Value-Konsumenten behalten die lokale Liste und ihren bisherigen
  Synchronisationsvertrag. Fehler/Indeterminate uebernehmen keine IDs;
  Operation-Cleanup gibt dynamische Kandidatenkinder und Run-ID frei.
- Ein identitaetsgebundenes Web-Requestresult mit In-place-Confirm;
  kein neuer CommandId/RunId beim Confirm. Value-Confirm bleibt verfuegbar.
  Record-Payload/Envelope-Encoding endet vor Storewrites; vorhandene Strings
  werden bewegt. Ein optionaler Head wird Prepared→reset/emplace→Committed
  wiederverwendet;8 B eingefrorene erste Revision und beide unabhaengigen
  kodierten Headbytes stehen vor dem ersten Write bereit. Exaktes OLD/NEW/
  Unknown-Readback, Prepared→Slot→Committed, fruehe Slot-Adoption fuer Recovery,
  Golden-Bytes, UTC/Fallback/Origin/Replay bleiben im bestehenden Kern.
- 4→5→6-Migration mutiert nur unveroeffentlichte Kandidaten; die Value-API
  kopiert einmal und benutzt dieselben Schritte. Kein konsumierender
  Settings-Envelope/Record-Decode (Option B) und keine Katalog-/UI-Reorganisation.
- Borrow-unverfuegbar bleibt DecisionOnly/Unavailable mit vorhandenem
  `CommandStatus::ContextMissing`-Detail; keine neue OOM-Enum und kein
  faelschliches OwningOutcome. Die generische ContextMissing-Domainabbildung
  fuer direkte Value-Konsumenten wird nicht umgedeutet.

### Reale Profile, Target-ABI und phasenbezogene Stackbilanz

[Provenienz/BIN-/ELF-/DB-/Response-/Sourcehashes](PR209_Q4_IMPLEMENTATION_20261023_PROVENANCE.json),
[Target-ABI](PR209_Q4_IMPLEMENTATION_20261023_ABI.tsv),
[Compiler-/Objekt-/SU-/Callgraphnachweise](PR209_Q4_IMPLEMENTATION_20261023_COMPILE.tsv),
[Reproduktionsbefehle als Evidence-Text](PR209_Q4_IMPLEMENTATION_20261023_COMPILE.py.txt).
Kanonischer realer Build beider Profile auf sauberem Produktcommit e31e952:

```bash
source /var/lib/docker/data/ESP32-Projekte/start.sh
export IDF_TOOLS_PATH=/var/lib/docker/data/engineering/home/manuel/.espressif
export IDF_PYTHON_ENV_PATH="$IDF_TOOLS_PATH/python_env/idf6.1_py3.13_env"
export PATH="$IDF_PYTHON_ENV_PATH/bin:$PATH"
python3 scripts/build_esp_idf_profiles.py all --require-clean-source-tree
```

**PASS:** beide Firmwarelinks, kanonische Profil-/Herkunftspruefung,
SOURCE_TREE_CLEAN=YES. IDF6.1 Commit
`fff9895c82d744c7237be8847347bdd1b07c6643`, GCC15.2.0
`esp-15.2.0_20251204`, Xtensa windowed ELF32 little endian, reale `-Og`/
C++17/`-fno-exceptions`-Flags und jeweilige Response-Dateien.
`project_description.json` und `APP_SOURCE_GIT_SHA` in beiden echten
App-Compilebefehlen binden an e31e952. Zusätzliche SDK-Analyse kompiliert
je Profil54 **unveraenderte originale** SDK-/Adapter-TUs mit ihren tatsächlichen
Build-DB-Argumenten plus ausschliesslich `-fstack-usage -fcallgraph-info=su`;
keine temporaere Produktvariante. ABI-Konstanten kommen aus Targetobjekten.

Targetwerte in beiden Profilen: State4896/8, owning Decision10104/8,
Guard216/8, View536/8, Coordinator21352/8, Token4/4 (optional8/4),
ActiveRun2816/8, Snapshot3904/8, Requestresult856/8 B.
**Zusaetzlicher grosser fester Heapowner/allokierter Block:0 B**;
Coordinatorgroesse unveraendert, IDfenster256 B in bestehender Snapshotflaeche.
Q3-Scratch1856 B/Alignment8, konservativ1936 B bleibt unveraendert und wird
hier nicht erneut als vermeintliche neue Heapreserve vermessen.

[Gemessene Frames](PR209_Q4_IMPLEMENTATION_20261023_FRAMES.tsv),
[Phasen/Summen](PR209_Q4_IMPLEMENTATION_20261023_SUMMARY.tsv),
[geordnete bekannte Unterpfade](PR209_Q4_IMPLEMENTATION_20261023_PATHS.tsv),
[konkrete SDK-/NVS-Ziele](PR209_Q4_IMPLEMENTATION_20261023_EDGES.tsv),
[unaufgeloeste Ziele](PR209_Q4_IMPLEMENTATION_20261023_UNKNOWN.tsv),
[Callgraphhashes](PR209_Q4_IMPLEMENTATION_20261023_SOURCES.tsv),
[Auswertungsrezept mit ausdruecklich begrenzter Sieben-Varianten-Projektion](PR209_Q4_IMPLEMENTATION_20261023_PATHS.py.txt).

| Gleichzeitig aktive Phase | bekannte Bytes, beide Profile | Einordnung |
|---|---:|---|
| Prepare / Confirm | 6016 /4288 | NOT_PROVEN |
| Sieben Into-Decision-Varianten: Program/Manual/Stop/Completion/Adjustment/Ack/Mute | 6000 /6112 /6048 /6048 /6176 /5744 /5744 | NOT_PROVEN, Unterpfade eingeschlossen |
| Borrow-Run Record-Encode/Validierung | 6704 | NOT_PROVEN |
| Borrow-Run Store einschliesslich Unknown-Write-Readback | 7216 | NOT_PROVEN |
| Settings-Namenscommit, Q3 unveraendert | 6960 | NOT_PROVEN |
| Post-Commit-Guard / RAM-Move / Borrowfreigabe | 5088 /5344 /4720 | eigene Phasen; NOT_PROVEN |
| **Web-Recovery/Sensor: Eintritt in Value-Fallback** | **mindestens15248** | **FAIL schon vor Unterpfaden** |

Prefix1856 B: Taskwrapper32 + httpd_thread32 + httpd_server80 + sess_enum32
+ process_session32 + sess_process32 + req_new32 + parse_req144 + uri64
+ Adapter704 + Dispatcher96 + handleRun32 + Mutation544. Der Callback ist
sourcegebunden, kein unpassender sess_get-/accept-Kontext. Application1312,
Web1200, Borrow48, Internal(View)512, Write160, Core160,
RecordPrepare128, Transaction672 B. Direkte Value-Internalvariante768 B.
Untersuchungsziel Internal512 erreicht, Write80 nicht erreicht (real160 B).
NVS Write1168 /Read1200 B bekannte Unterpfade; die bisher ausgelassene virtuelle
Readbackkante aus `writeExact` ist enthalten und bestimmt jetzt den7216-B-Pfad.
Kein Addieren isolierter A/B/C-Gewinne. 8192−7216=976, nach512-B-Reserveboden
464 B; Settings720 B. Diese Rechnungen sind **keine Reserveobergrenzen**.

Die Auswertung addiert ausschliesslich gemessene Teile einer Pfadprojektion.
Unbekannte Frames bleiben explizit unbekannt; die numerische Summe laesst
sie offen und ist kein Nullbedarfsansatz/PASS. SDK/NVS Flash-Defaulttabellen,
Chip-/ROM-/libc-Ziele, `__indirect_call`, Constructor-/Destructor-/Allocator-
Aliase, New-Handler/Abort, Mutex-/Critical-/Cleanup-/Fehlerzweige und vier
Callgraphzyklen haben keine vollstaendige Obergrenze. Instrumentierung liefert
keine Task-HWM oder Interrupt-/Registerspill-/Parallelitaetsqualifikation.
Das bewiesene Value-Frame-FAIL benoetigt keine Annahme ueber diese offenen Anteile.

### Dynamische Heapfenster gegen095f393 und Fehlergrenzen

[Identische native Fixtures und rohe Werte](PR209_Q4_IMPLEMENTATION_20261023_HEAP.tsv):
bestehender Allocation-Window-Helfer, zusaetzlich Messgeneration und kontrollierte
Fehlinjektion/Exception-Cleanup; keine parallele Testarchitektur. Vergleich auf
isoliertem095f393-Worktree mit denselben portablen Testzusaetzen, keine dortige
Produktquellenaenderung. Beide Produkte:16 Programme (4 Factory/12 User),
User-ID48 B, Name96 B/48 UTF-8-Scalars, Notes1024 B/512 Scalars. Adjustment-
Fixture mit Run-ID48 B und31 vorhandenen Revisionen plus32. Revision.
Je Messung Response/Fixturecleanup geprueft, anschliessend0 gemessene Livebytes.

| Fixture | Requests alt→neu | angeforderte Bytes alt→neu | Livepeak alt→neu | groesster Request alt→neu |
|---|---:|---:|---:|---:|
| Start + exaktes Replay |196→174|68811→60854|47484→45094|32769→32769|
| Identity-Writefehler vor Decision |109→94|58570→52099|47484→45094|32769→32769|
| Identity Unknown/Readbackfehler |112→97|58877→52406|47484→45094|32769→32769|
| Settings-Namenscommit |1159→1159|516410→516410|69724→69724|19890→19890|
| Adjustment/Replay mit32 Revisionen |581→556|230331→217808|46088→43698|32769→32769|

Diese Werte zaehlen host64 angeforderte Plain-New-/cJSON-Bytes und deren
Ueberlappungen, nicht Allocatorheader/Fragmentierung oder unveraenderte aligned
Q3-Scratchallokation. Live-after-Operation ist absichtlich positiv fuer
persistente Store-/Runtime-/Replayobjekte; erst nach Fixtureabbau0.
Keine gemessene Heapregression oder groessere neue Blockanforderung in diesen
Fixtures. **Kein absoluter ESP32-Peak-/Allokations-/Largest-Free-Block-Nachweis.**
Der bestehende volle Program-Catalog-Writer reserviert bis32768 Nutzbytes
(Request mindestens32769 inkl NUL), unabhaengig von einer einzelnen neuen
Decisionstruktur. Settings Read/Decode/2R-Kollision und UI-Katalogkopien aus
der historischen RAM-Bilanz bleiben ausserhalb Option A; kein Verschweigen
dieser Spitzen durch Betrachtung nur eines Owners.

Historische H-RAM-1-Daten (Minimum19896 B, anderszeitige Largest-Free-Werte
26624/22528 B) und F4 PROVISIONAL Skip13312/Resume16640 B sind keine
Reservierung fuer diese Requests und kein Stand des neuen Produkts. Ein
32-kB-Request passt nicht allein aufgrund des freien Gesamtheaps; zeitgleicher
interner groesster Block, bestehende Allokationen/NVS-Puffer/Allocatoraufwand,
Heap-Regionen und reale Fehler-/Langzeitspitzen bleiben **NOT_PROVEN/NOT_RUN**.

OOM: native erste Plain-New-Fehlinjektion wirft `std::bad_alloc`; erstes
cJSON-Null liefert den bestehenden400-Ausgang. Sechs gezielte Allokations-
Fehlerpunkte im privaten Kandidaten-/ActiveRun-Aufbau geben Borrow und Kinder
frei, ohne Apply oder neue processed IDs. Das beweist Host-Unwinding, **keinen**
ESP32-OOM-Return bei `-fno-exceptions`. Insbesondere String-/Vektor-Kopien bei
Encoding, Slot-Adoption, Readback oder nach durablem Write sind weiterhin
keine allgemein rueckgebbare OOM-Situation. Keine neue Enum, Budgeterhoehung,
vermeintlicher Persistenz-Busy-/Capacity-Erfolg oder Replay-Neustart daraus.

### Gezielte Regression und Uebergabe

[696/696 native Faelle in18 direkt betroffenen Suites; Builder-Nachweis](PR209_Q4_IMPLEMENTATION_20261023_TESTS.txt).
Erweiterungen in bestehenden Tests: sieben Value/View-Kerne mit je acht
Normal-/Rejected-/Unconfirmed-/Stale-/Safety-/Overflow-/Idempotenzsituationen
und Metadaten/Apply-Paritaet; elf Guardfelder und Alias-/Fremdzielschutz;
54 Value/Borrow-Transaktionsszenarien (leer/vorhandener Root, drei Writes,
neun Fehlerkonstellationen) mit exakten rh0/rc0/rc1-Bytes, Return/Durability/IDs;
rekursive/fremde/andere Coordinator und postdurable Safety/Sensorlock mit
CommittedApplyFailed; In-place-ActiveRun und unveraenderte Invalid-Destination;
Confirm-Identitaet, Web Origin/Replay/Ack/Mute; alter/neuer Recovery-/Sensor-
Wirepfad;4→5→6-Erfolg und spaeter Katalog-Teilabbruch ohne publiziertes Resultat;
bestehende Codec-/OLD/NEW-/Unknown-/UTC-/Recovery-/Fallback-/Goldenmatrizen;
oben begrenzte native Allocation-/Cleanup-Faelle. Keine allgemeine C0–C5-
Neubewertung, keine JS-/Hardware-/Pre-Ready-/schwere-CI-Wiederholung.

Kanonischer Builder-Self-Check auf Produkt-e31e952:
`PRE_READY_EXPECTED_HEAD=e31e952def3264ca419187b86a2988bd20e3a487 bash scripts/run_pre_ready_gates.sh self-check`:
clang-format21/clang-tidy21 **PASS**. Die ersten Stil-/Parameter-/RAIIbefunde
sind behoben; keine Warnunterdrueckung oder Runneraenderung. Builder-PASS ist
kein Independent Review. D9, reales HWM/Largest-Free-/Heap und Flash **NOT_RUN**;
keine Ready-/Merge-/Aktorfreigabe.

**STOP; genau eine Ownerentscheidung:** Ob die ebenfalls Web-erreichbaren
Recovery-/Sensorrequests mit ihrer vorhandenen Value-Grenze in denselben
zusammenhaengenden Ressourcenrefactor aufgenommen werden sollen. Dieser
zusatzliche Produktumfang ist nicht freigegeben; keine weitere Mikrooptimierung
oder Scopeentscheidung automatisch. Aktueller Diff ist unabhaengig reviewbar,
aber `HTTPD_FULL_CHAIN_STACK=FAIL`, `C5_RESOURCE_GATE=HOLD`,
`FULL_CHAIN_RESERVE=NOT_PROVEN`, `RAM_PASS=NOT_PROVEN`, `OPEN_BLOCKERS=1`,
`ACTUATOR_RELEASE=NO`. Hardware kann den bereits statisch bewiesenen
10880-B-Frame nicht als8192-B-PASS qualifizieren.
