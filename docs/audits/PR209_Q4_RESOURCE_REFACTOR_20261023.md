# PR #209 – Q4-Ressourcen-Refactor: begrenzte Entscheidungsevidence

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
