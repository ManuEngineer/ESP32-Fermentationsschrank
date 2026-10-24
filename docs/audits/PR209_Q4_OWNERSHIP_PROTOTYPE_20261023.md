# PR #209 – begrenzter Q4-Ownership-Prototyp

**Entscheidung (b): IMPLEMENTATION_READY=HOLD / FULL_CHAIN_RESERVE=NOT_PROVEN.**
Basis285d8378633f4d7a05f7c691997df4deb94bd63f, Produkt095f393f0eed4eb95944c164a8e967ce973c3fc9.
Eine koharente temporaere Quellvariante wurde jetzt tatsaechlich kompiliert.
Der [vorherige Ownership-Stop](PR209_Q4_WORKSPACE_20261023.md) wurde damit
bearbeitet; fehlende globale Locks sind **nicht** der neue Haltegrund.
Produktquellen, Tests, Q3, Budgets und Firmwareartefakte bleiben unveraendert.

## Eine gemeinsame Variante, reproduzierbare Compile-Evidence

Vollstaendige gemeinsame Kopie von `lib/fermentation_app/src` unter
`/tmp/pr209-q4-ownership/src`, elf geaenderte Dateien in
[genau einem Patch](PR209_Q4_OWNERSHIP_20261023.patch). Nur in einer temporaeren
Kopie anwenden. [Alle gemeinsamen Quellhashes](PR209_Q4_OWNERSHIP_20261023_SOURCES.tsv),
Bundle-SHA256 `9ea6ea92d4aa4f999ea8f5e68c327fa6286f803ca316d24f400df9bc6a79e86c`
(sortierte relative Namen, NUL, Bytes, NUL). Patchrekonstruktion dieser
gesamten Variante PASS; keine Kombination isolierter Optimierungsgewinne.

Finaler Compile-Stand: je Profil neun gemeinsame App-TUs
(Application, Bridge, Commands, Snapshot, Coordinator, Web, Temperature-
Orchestrator, Recovery, Qualification), unveraenderte HTTPD-/NVS-/Heap-/Port-
SDK-TUs und zwei Plattformadapter; zusammen **74 TU-Kommandos**, zusaetzlich
vier ABI-Objektkommandos. Waehrend der Erstellung wurden Vertragskorrekturen
in derselben Variante nachgezogen; die Artefakte beschreiben deren finalen
gemeinsamen Stand, keine unabhaengigen A/B/C-Varianten.

Originalbefehle aus `build/{esp32_release,esp32_bringup}/compile_commands.json`:
nur `-c`/`-o` nach `/tmp`, gemeinsames temporaeres Includeverzeichnis zuerst;
App zusaetzlich RTL-/Tree-Dumps, SDK/Adapter `-fstack-usage -fcallgraph-info=su`.
Optimierung/ABI/Defines bleiben die tatsaechlichen Profilflags (u.a. -Og).
GCC15.2.0 `esp-15.2.0_20251204`, IDF6.1
`fff9895c82d744c7237be8847347bdd1b07c6643`.
[Kommando-/Quell-/Objektfingerprints](PR209_Q4_OWNERSHIP_20261023_COMMANDS.tsv),
[exakte Argumentlisten und Arbeitsverzeichnisse](PR209_Q4_OWNERSHIP_20261023_COMMANDS.jsonl),
[Inhalte/Fingerprints der vier referenzierten Response-Dateien](PR209_Q4_OWNERSHIP_20261023_RESPONSE_FILES.jsonl).
Keine Links, Firmwarebuilds, Ausfuehrung, semantischen Tests oder Hardwarelaeufe.

## Besitzvertrag nur fuer den gemeinsamen Coordinator

Der Entwurf fuehrt ein **privates**, nur fuer die Application zugaengliches
Borrow-Protokoll dieses Coordinators ein. Kein globaler Lock und kein neuer
Threadsafe-Vertrag fuer unabhaengige Domaininstanzen:

1. `beginCommandOperation()` prueft das instanzlokale Flag/Busy **vor** jedem
   Kandidatenzugriff. Erst ein erfolgreich erzeugtes, nicht kopierbares
   `CommandOperation`-Token erlaubt `commandCandidate()`; dort wird die
   Zugehoerigkeit zum selben Coordinator nochmals geprueft.
2. Token und Application-Serializer leben bis Decision, Persistenz/Readback,
   RAM-Uebernahme oder Abbruch beendet sind. Guard/View entstehen erst nach
   erfolgreichem Borrow. Ein zweiter Application-Request scheitert vorher.
3. Die eigene Fortsetzung ueber `persistBorrowedCommand(token,...)` prueft
   Token, Coordinator und Current-Identitaet und delegiert **privat** an den
   gemeinsamen Persistenzkern. Sie laeuft nicht erneut durch den externen
   Borrow-/Mutationsschutz. `state_` wird fuer den Borrow nicht kuenstlich
   umgeschaltet; die vorhandenen Ready-/Busy-/Recovery-Pruefungen bleiben.
4. Das Token loest den Borrow auf allen Rueckwegen. Vor Freigabe werden
   dynamische Kandidateninhalte durch `clearActiveRunState` und einen leeren
   String-Swap freigegeben. Kein voller RunCommandState-Temporary beim Cleanup.
   Nach erfolgreicher Uebernahme sind die Unterpuffer bereits verschoben.

Der Schutz steht am Anfang **dieser konkreten oeffentlichen Eingange**:
PersistCommand/FreshStart, Transition, SensorSelection, Periodic,
ActivateLoaded/R1Eligible/Fallback, ResolveRecoveryOutcome, Discard,
ReevaluateRecoveryEvaluation, EvaluateCurrentFermentingRecovery,
PrepareRecoveryEvaluationState, PersistRecoveryCandidate,
Prepare/FinalizeAuthorizedEpochHandoff und LoadAndInitializeInto.
`loadAndInitialize()` delegiert. Load/Bool-/Recovery-Ergebnisse verwenden
ihre vorhandenen Ergebnisformen; kein neuer Persistenzstatus. Read-only
`state()` und `commandIdHighWater()` koennen weiter den alten aktuellen Stand
projizieren. Application-Begin/BeginPersistent und FactoryReset duerfen
waehrend dieser Operation den privaten Coordinator nicht ersetzen.

Das Flag ist kein Mutex. Die private Instanz bleibt durch den bestehenden
Application-Serializer geschuetzt; Initialisierung/Destruktion behalten ihre
bisherigen Lebenszyklusvoraussetzungen. Unsynchronisierte Fremdthreads an
beliebigen Domaininstanzen werden nicht neu erlaubt. Unabhaengige
Coordinator-Instanzen und direkte Value-Konsumenten erwerben keinen Borrow
und erhalten keine neuen globalen Locks. Ob der Entwurf diese Eigenschaften
auch bei Fehlerinjektion einhaelt, ist **NOT_TESTED**, kein Ownership-Runtime-PASS.

## Eingefrorene Basis, gemeinsamer Fach- und Schreibkern

`CommandApplyGuard` kopiert genau die im bestehenden Apply verglichenen
Altwerte: ProcessRuntimeState, Command-/Run-/Message-/Faultrevision, kritisches
Safety-Pending, Run-ID, aktiver Sensormodus, SensorSelectionRuntime,
persistierte SensorSelection und RecoveryEpisodeRevision. Die ID verwendet
48 feste Bytes plus Laenge/Gueltigkeit, keine Stringallokation. Voraussetzung
ist die bestehende validierte private Run-ID-Grenze; ungepruefte externe
Value-Zustaende wechseln nicht still auf diese Ansicht.

Processed-ID, aktuelle kritische Safety, Proposed, alle alten Stale-Vergleiche
und `after.commandSequence == current + 1` stehen in **einem** gemeinsamen
`validateRunCommandCore`. Value verwendet seinen unabhaengigen before-Wert,
View den eingefrorenen Guard. Der before-Verweis der View dient dem
Entscheidungskern/Rollback unter dem Besitzvertrag, nicht als veraenderliche
Apply-Pruefbasis. Ein eigener8-B-Wert `RunPersistenceBefore` transportiert
die alten Run-/Transitionrevisionen aus dem Guard in PreparedHead; dort wird
nicht wieder das moeglicherweise geaenderte Current als Altwert gelesen.

Die sieben Web-Kommandokerne werden je einmal als gemeinsamer Into-Kern fuer
Value/View instanziiert. Keine zweite Fachregel. Ablehnung/Unconfirmed stellt
after aus dem geschuetzten before wieder her, ohne Summary/Notice/Effects
neu zu interpretieren. ActiveRun wird nach derselben Anfangsvalidierung direkt
in `after.activeProgramRun` konstruiert; Value-Factory delegiert daran.
Kein Factory-ActiveRun-/Optional-Temporary und keine Selbst-Move-Zuweisung.
Restore bleibt unveraendert. RecoveryTimeCorrection/SensorSelection bleiben
im separaten bestehenden Value-Eintritt, den der geschlossene Web-Codec nicht
erzeugen kann; diese grossen Nicht-Web-Ketten gelten nicht als optimiert.

Beim Borrow wird der bereits von `beginMutation` vorbereitete Kandidat nur
noch validiert, **nicht auf sich selbst angewendet**. Dieselbe Templatefunktion
`persistCommandInternal` behaelt ID-/Zeit-/Provenienz-/Snapshot-/Storechecks.
Der vorhandene Write/Readback-Kern wird genau einmal verwendet. Nach durablem
Applied erfolgt erneut die eingefrorene Stale-/Safety-Pruefung; bei Ablehnung
weiter `PersistenceCommittedApplyFailed`, ohne vorgetaeuschten RAM-Erfolg.
Erst danach wird der Kandidat in Current verschoben. Direct-Value-Pfade
verwenden denselben Pruefkern und ihre bisherige Kopieruebernahme.

Ack/Mute validieren und uebernehmen weiterhin ausschliesslich RAM, ohne
Storewrite/Persisted-ID. Ein unbenutzter/gesperrter Persistenzstand sperrt
diese normalen RAM-Kommandos nicht pauschal; nur eine bereits laufende
Mutation darf den gemeinsam verwendeten Kandidaten nicht nochmals anfassen.

## Gemeinsame Messwerte, beide echten Xtensa-Profile

[ABI](PR209_Q4_OWNERSHIP_20261023_ABI.tsv) wurde als konstante sizeof/alignof-
Daten in einer `.q4abi`-Sektion eines **Xtensa-Objekts** ermittelt und als
Little-Endian-Werte gelesen. Keine Host-Ausfuehrung/64-Bit-Groessen.

| Typ / Owner | sizeof B | Alignment B |
|---|---:|---:|
| CommandDecisionView, einschliesslich Guard | 536 | 8 |
| CommandApplyGuard | 216 | 8 |
| CommandOperation / optionales Token | 4 / 8 | 4 / 4 |
| RunPersistenceBefore | 8 | 4 |
| Coordinator: Produkt / Prototyp | 21352 / 21352 | 8 / 8 |

Das Flag nutzt vorhandenes Padding. Guard/View/Token sind Stackwerte, keine
Heapallokation. Die bereits bekannten4896/10104/12752-B-Typflaechen werden
nicht nochmals als neue Heapowner eingefuehrt.

[300 ausgewaehlte rohe Framezeilen](PR209_Q4_OWNERSHIP_20261023_FRAMES.tsv),
alle static, mit Quelldatei/Funktion; Profilparitaet getrennt pruefbar:

| Eintritt / gemeinsamer Kern | Produkt B | Prototyp B |
|---|---:|---:|
| Application applyConfirmedPrepared | 10880 | 1312 |
| Web applyRunMutation | 2048 | 1200 |
| ProgramPrepare / makePreparedRequest | 2592 / 1680 | 2064 / 48 |
| Confirm / In-place-Confirm | 896 | 48 |
| Program / Manual Into-Kern | 8000 / 5216 | 288 / 320 |
| Stop / Completion / Adjustment | 5136 / 5136 / 5280 | 256 / 256 / 416 |
| Ack / Mute Into-Kern | keine isolierte Gutschrift | 48 / 48 |
| Alle sieben Web-Visitoren | 20240 | 32 |
| ActiveRun-Startfactory auf dem Webpfad | 3136 | startInto96; direkter Konstruktor32 |
| PersistCommandInternal | 720 | **768** |
| writeSnapshot / Core | 208 / 1168 | 208 / 1168 |

Die Webroute haelt nur noch **einen**856-B-Requestresult. In-place-Confirm
revalidiert denselben identitaetsgebundenen Request und setzt dasselbe Bit;
die bestehende const Value-API kopiert weiterhin unter dem Serializer und
delegiert. Die 528-B-Optional/Prepared-Konstruktion unter makePreparedRequest
und die Revalidate-Unterpfade verschwinden nicht und sind mitbilanziert.
Keine Behauptung, ein syntaktisches Move spare automatisch Heap oder Frames.

## Vollketten: gemessene Frames, berechnete Teilketten, offene Obergrenzen

[Phasenpfade](PR209_Q4_OWNERSHIP_20261023_PATHS.tsv),
[konkret zugeordnete indirekte Ziele](PR209_Q4_OWNERSHIP_20261023_RESOLVED_EDGES.tsv).
Nur gleichzeitig aktive Frames summieren. Fehlende Frames/Targets bleiben
**NOT_PROVEN**, keine0-B-Buchung. Die Werte sind keine gemessene HWM und keine
vollstaendigen oberen Stackgrenzen.

Tatsaechlicher Dispatchprefix bis einschliesslich Mutation-Handler:
Taskwrapper32 + httpd_thread32 + server80 + sess_enum32 + process_session32
+ sess_process32 + req_new32 + parse_req144 + uri64 + Adapter704
+ Dispatcher96 + handleRun32 + Handler544 = **1856 B**. Der konfigurierte
Taskwrapper ist eingeschaltet. Die Session-Enumeration verwendet hier den
process_session-Callback; die andere Verwendung aus accept/sess_get ist
**kein** zusaetzlicher gleichzeitig aktiver Requestprefix. Ein kontextloser
Callgraph kann diese Kanten falsch kombinieren und wird nicht als Beweis benutzt.

| Phase | Auswahl bekannter gleichzeitig aktiver Frames / Unterpfade | Berechnete Teilsumme B |
|---|---|---:|
| Program-Prepare | Prefix1856 + Web1200 + Prepare-Unterpfad2960 | 6016 |
| Confirm | Prefix1856 + Web1200 + In-place/Revalidate-Unterpfad1232 | 4288 |
| Decision | Prefix1856 + Web1200 + App1312 + View-Bridge/Domain-Unterpfad1776 | 6144 |
| Ack/Mute RAM-Apply | Prefix1856 + Web1200 + App1312 + Apply-View-Unterpfad448 | 4816 |
| Persist/Validierungszweig mit normalem Allocatorpfad | Prefix1856 + Web1200 + App1312 + Borrow48 + Internal768 + Write208 + Core1168 + Validator-/Allocatorunterpfade | **7856** |
| Settings-Commit, konservativer bekannter Graphpfad | Prefix1856 + ApplyWebConfiguration-Unterpfad5760 | **7616** |
| Post-Commit-Apply | App/Internal bleiben aktiv; Write/Core sind bereits zurueckgekehrt. Zweiter Guardcheck und Move, keine Addition des Write/Core-Pfads | NOT_PROVEN als Vollkette |
| Abbruch/Cleanup | Release-Unterpfad352 B unter Application/Token-Destruktion; keine gleichzeitige Addition bereits beendeter Decision-/Write-Unterpfade | NOT_PROVEN als Vollkette |

Im expliziten7856-B-Pfad liegen unter Core u.a. Encode112, Snapshotvalidate128,
Programvalidate96, Targetvalidate384, Programvalidation48 und der
Validierungsfehler-/Vektorpufferpfad. Fuer dessen normalen malloc-Zweig sind
operator new32 und malloc32 aus den unveraenderten [Profil-ELF-Prologen](PR209_Q4_OWNERSHIP_20261023_ELF_FRAMES.tsv) belegt;
die Heap-/TLSF-/Port-TUs wurden mit echten Flags nachgemessen. Der Pfad geht
ueber heap_caps_malloc_default32, malloc32, malloc_base32, aligned_base48,
aligned_or_unaligned_alloc32, multi_heap_malloc32, internal_lock32 und
xPortEnterCriticalTimeout32. Alternative ausgerichtete Graphzweige ergeben
einen konservativen bekannten Subtotal7888 B; daraus wird keine zusaetzliche
tatsaechliche Runtime-Untergrenze konstruiert. Datenabhaengige Validator-
Fehlerzweige sind konservativ enthalten, nicht fuer jeden gueltigen Run
behauptet.

**8192−7856 =336 B** ist nur ein arithmetischer Rest vor noch unaufgeloesten
Zielen und alternativen Pfaden, keine begruendete Reserve. NVS-Read/Write,
Page-/Partition-/Flash-Targets, C++-Konstruktoraliase, weitere libc-/Port-
Unterpfade und OOM-/Abortzweige sind nicht vollstaendig begrenzt. Der
gemessene NVS-Write-Unterpfad864 B bzw. Read848 B darf weder mit dem
Validierungszweig noch miteinander als gleichzeitig laufend addiert werden;
er umfasst auch nicht alle indirekten Flashziele. Settings hat ebenfalls
keine abgeschlossene Obergrenze. Q3 bleibt unveraendert; sein2928-B-Teilpfad
war und ist keine Vollkettenqualifikation.

## Peak-/Lifetime-/Heapbilanz gegen095f393

| Flaeche / Lebensdauer | Produkt | Kombinierte Variante |
|---|---|---|
| Current + vorhandener WS-Kandidat | je1×4896 B dauerhaft | Dieselben beiden Flaechen; nur Kandidat wird bis Commit veraendert |
| App/Visitor-Decision | App10104 B, zusaetzliche lokale/Return-Storage in20240-B-Visitorframes; je zwei volle Runzustaende | View536 B statt zusaetzlichem vollen before/after; Guard ist enthalten, nicht nochmals addieren |
| Domain-Candidate/Start | weiterer voller lokaler Runzustand und Optional-ActiveRun2824 B; Factory3136 B gleichzeitig | vorhandenes WS-after und dessen ActiveRun-Speicher; keine neue volle State-/ActiveRun-Flaeche |
| Prepare/Confirm | zwei856-B-Resultate im Webframe, Program-/Requesttemporaries | ein856-B-Resultat; Under-construction/Move-/Optional-Unterframes weiter bilanziert |
| Persist/Readback | WS-Snapshot/Record/Slots, payload/env-Kopie, target/record-Bytes, Heads, alter physischer Slot, oldSlot-Kopie, ggf. Readback | unveraenderter Schreib-/Readback-Kern; Unterpuffer bleiben und werden nicht als frei betrachtet |
| Erfolgreiche Uebernahme | Decision-after wird kopiert; WS-Kandidat kann Unterpuffer behalten | vorhandener Kandidat nach zweitem Check verschoben; Token-Cleanup gibt verbleibende Kandidatenunterpuffer frei |
| Ablehnung/OOM/Writefehler/Indeterminate | Current bis durablem Apply unveraendert | gleicher beabsichtigter Vertrag; Token-Cleanup unabhaengig vom Storestatus; keine behauptete Runtime-Verifikation |

**Neue feste Heaprequests:0; zusaetzlicher fester Heap:0 B; groesster neuer
fester Allokationsblock:0 B.** Coordinator21352 B bleibt gleich gross. Das
ist ausdruecklich keine Aussage, alle dynamischen Heaprequests seien0.
Es gibt keinen neuen expliziten Deepcopy-/Scratch-Allokator im Webpfad;
Candidate-Befuellen ist die vorgezogene bestehende Kopie, Requests/Decision
besitzen weniger volle Kopien. Move-Uebernahme und Cleanup sind im Entwurf
sichtbar, ihre tatsaechliche Peak-Heapersparnis wurde **nicht** gemessen.

Insbesondere werden beim frueheren Candidate-Copy Strings/Vektoren frueher
belegt, alte Kapazitaeten koennen ueberlappen und Rollback kann erneut
zuweisen. Absolute und differentielle dynamische Requests/Allocatorbelegung,
maximaler gleichzeitiger Unterpufferpeak und groesster insgesamt erforderlicher
freier Block sind deshalb **NOT_PROVEN**, keine unterstellte0-B-Mehrlast.
8240-B-Record-/256-B-Headlimits sind keine Kapazitaets-/Realloc-/NVS-Scratch-
Obergrenzen. Gegen die Produktbasis sind die angegebenen Frameaenderungen
gemessen, der feste Heapdelta0 B belegt, der gesamte Netto-RAM-Peak bleibt
offen. **Im Produkt realisierte Aenderung aus diesem Auftrag:0 B**, da nur
eine temporaere Variante gebaut wurde. Historisches H-RAM-1/F4 ist weder
aktuelle freie Reserve noch Heapreservierung; D9/HWM bleiben getrennt.

## Entscheidung, einzige Alternative und spaetere Regressionen

**HOLD.** Der kleine instanzlokale Besitzmechanismus ist als Compile-Prototyp
konkret, kein globaler Architekturzwang. Die gemeinsame Variante gibt aber
keine belastbare positive8192-B-Vollkettenreserve her: bereits7856 B in der
betrachteten Run-Kette, weitere Obergrenzen fehlen. Kein Ressourcen-PASS,
kein als Implementierungsfreigabe verpackter Teilframe-PASS.

**Eine benannte Alternativentscheidung fuer den Owner:** einen
**zusammenhaengenden Ressourcen-Refactor des vorhandenen Run-Persistenz- und
Validierungskerns** als groesseren Scope freigeben oder HOLD beibehalten.
Der messbare Ansatzpunkt ist nun Internal768 + Write208 + Core1168 B samt
nachgeordneten Validierungs-/Serialisierungslebensdauern, bei unveraenderter
Write-before-Apply-/Readback-/Recovery-Verantwortung. Das ist keine weitere
isolierte Factory-/Webframe-Mikroprobe und wird hier nicht automatisch
begonnen. Keine Queue, kein Worker, weiterer grosser Owner oder Budgetanstieg.

Bei spaeter autorisiertem Scope: gezielte Value/View-Aequivalenz aller sieben
Webvarianten und direkten Domain-/Touch-Konsumenten; komplette
before/after/Status/Effects/Summary/Notice-Werte bei confirmed/unconfirmed,
Rejected/NoChange/Stale/Safety/Overflow; rekursive Mutation vor Candidate-
Touch abgewiesen, eigene Fortsetzung akzeptiert, fremdes Token abgewiesen,
unabhaengige Coordinator-Instanz unbeeinflusst, Cleanup auf allen Fehlerwegen.
Processed-/Persisted-IDs, Origin/Run-ID, Trusted-UTC, unveraendertes Replay,
Ack/Mute ohne Store, Writefehler/OLD/NEW/Indeterminate/Recovery und
post-durable Safety/Stale mit CommittedApplyFailed sind gezielt zu pruefen.
Golden-Bytes und benoetigte OOM-/Kapazitaetsfehlerfaelle bleiben Pflicht.
Kein allgemeiner C0–C5-Review und keine Wiederholung unveraenderter Q3-Suites.

Reentranz-Abweisung im Compile-Prototyp verwendet einen leeren
DecisionOnly/Unavailable-Resultwert als **Detail-Stub**, kein vorgetaeuschtes
OwningOutcome. Ein ehrlicher interner ResourceUnavailable-Detailstatus und
OOM-/Replay-Mapping bleiben nur Vorschlag; keine neue Enum im Produkt.
Vorhandene String-/Vektorallokationen sind dadurch nicht als OOM-sicher
qualifiziert. Identischer Web-Replay darf weiterhin keinen zweiten Versuch
starten oder verbrauchte Identitaeten zuruecksetzen.

**Zusaetzliche Produktimplementierung NICHT FREIGEGEBEN.** Nach diesem
Entscheidungsdelta anhalten; keine weitere automatische Optimierungsschleife.

## Verifikation dieses Prototyp-/Dokumentationsdeltas

PASS: finale Compileobjekte/ABI-Auswertung,150 ausgewaehlte Funktionsframes
mit Profilparitaet, Quell-/Kommando-/Objektfingerprints, Rekonstruktion genau
eines gemeinsamen Patches, Phasenarithmetik und relative Links. Originale
Produkt-/Testquellen gegen095f393 und Release-/Bring-up-BIN-SHA256 unveraendert;
`git diff --check` PASS. Diese Pruefungen ersetzen keine der oben benannten
semantischen, dynamischen Heap- oder vollstaendigen Stack-/Hardwarepruefungen.
