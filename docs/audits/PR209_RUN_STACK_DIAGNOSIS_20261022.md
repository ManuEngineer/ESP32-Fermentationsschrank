# PR #209 – HTTPD-Run-Stack: Diagnose und Korrekturbewertung

2026-10-22. **DIAGNOSIS_ONLY; Q4_NOT_AUTHORIZED; HTTPD_STACK_GATE=FAIL.**
Gepruefter HEAD `84fa6696559ef89f9e46afb0157f818c92365cf8`, Produkt-/Artefakt-SHA
`095f393f0eed4eb95944c164a8e967ce973c3fc9`. Branch/PR unveraendert, Draft/open.
Dies ist Evidence zum einzigen [Gesamtplan, §13](../tasks/issue-27-r1-web-remaining-scope-plan.md#13-httpd-run-stackdiagnose-und-minimaler-korrekturkandidat-q4-nicht-freigegeben), kein zweiter Plan.

Q3 bleibt unveraendert: 1856-B-CommitScratch, Alignment 8 B, konservativ
+1936 B Allocator-Belegung; Settings-Teilframes 8528 →2928 B. Die gezielte
Q3-Fix-Verifikation ergab laut Ownerauftrag keinen zusaetzlichen Persistenz-/
Recovery-Blocker. Keine Wiederholung der Q3-Regressionen oder eines C0–C5-Reviews.

## Messmethode und Abgrenzung

Tatsaechliche Xtensa-GCC-15.2.0-Befehle aus beiden vorhandenen
`compile_commands.json`, `esp-15.2.0_20251204`, IDF v6.1
`fff9895c82d744c7237be8847347bdd1b07c6643`, `-Og`, GNU++17,
Exceptions/RTTI aus, unveraendertes ABI-/Profil-/Include-/Defines-Profil.
Nur Analyseobjekte und isolierte Quellkopien unter `/tmp`; kein Produktfile
geaendert, kein Profil-/Firmwarebuild, kein Linken, kein Produkt-/Hardwaretest.

| Originaler Application-Compile-Befehl, SHA-256 (ohne Zusatznewline) | Wert |
|---|---|
| Release | `79bec4a20ad6987337fa6eb8811803e21a955c86a10e40c9639a0dba989d5f85` |
| Bring-up | `de2514935817451541aff1180b8db7d05524041f58af23672ff0eb6ec48ab8b1` |

Messung mit `-fstack-usage -fcallgraph-info=su`; zusaetzlich Application-
`-fdump-rtl-expand -fdump-tree-optimized -fdump-ipa-inline -fopt-info-inline`.
Originalbefehl beibehalten, `-o` nach `/tmp` ersetzen; bei Varianten auch `-c`
auf die isolierte Kopie setzen. [Frame-Rohwerte](PR209_RUN_FRAMES_20261022.tsv),
[ABI-Werte](PR209_RUN_ABI_20261022.tsv), [direkte Kanten](PR209_RUN_EDGES_20261022.tsv).
Alle gemeldeten Frames `static`, alle Tabellenwerte in beiden Profilen gleich.

`sizeof`/`alignof`: Bytearray-Symbole der jeweiligen Laenge in einer temporaeren
TU mit Produktionsheadern, Xtensa `nm -S --radix=d`; keine Hostausfuehrung.
Abgleich zentraler Groessen auch am vorhandenen Release-ELF mit Xtensa-GDB.

## Ursache: grosse Decision, darunter weitere Kopien

| Vorhandener Typ | Xtensa `sizeof` B | Alignment B |
|---|---:|---:|
| `CommandDecision` | **10104** | 8 |
| `RunCommandState` | **4896** | 8 |
| `RunPersistenceResult` | 112 | 8 |
| `FermentationUiCommandResult` | 208 | 8 |
| Prepared-Request / Application-Request-Result | 824 / 856 | 8 |
| `FreshStartSnapshotProvenance` / CheckpointTime | 88 / 24 | 8 |
| `ActiveRun` / `optional<ActiveRun>` | 2816 / 2824 | 8 |
| `ProcessRunSnapshot` / Optional | 36 / 40 | 4 |
| `unique_ptr<CommandDecision>`-Handle | 4 | 4 |

`applyConfirmedPrepared` haelt lokal eine vollstaendige `CommandDecision`:
`before` und `after` sind je ein `RunCommandState`, zusammen 9792 B; weitere
312 B enthalten Umschlag, Status, optionale Zusammenfassungen/Effekte usw.
Die Decision allein entspricht ca. 92,9 % des **gemessenen 10880-B-Frames**.
`confirmed` ist eine Referenz, keine weitere 824-B-Kopie in dieser Funktion.
Das UI-Ergebnis und der persistierte Rueckgabewert sind mit 208/112 B viel
kleiner; eine reine Aenderung der Ergebnisprojektion trifft nicht die Ursache.

Optimierter Tree/RTL bestaetigt `decision`, `decisionResult`, `persisted`,
`checkpointTime`, `provenance`, Guard sowie Rueckgabetemporaries. GCC ueberlagert
u.a. Runtime-Lease-/Optional- und Persistence-Return-Temporaries in einer
184-B-Partition, Provenance in einer 88-B-Partition: **nicht alle `sizeof`-
Werte gleichzeitig addieren**. Frame minus Decision =776 B ist nur die
arithmetische Differenz, keine isoliert gemessene Summe anderer Objekte.
Accessors/Destruktoren/Serializer-Guard werden inline expandiert; die grossen
Decision-/Persistenzkerne bleiben Aufrufe. Kein Nachweis, dass andere
Optimierungsflags oder Abschalten von Inlining diese lokalen 10104 B beheben.
Keine Toolchain-/Optimierungsvertragsaenderung vorgeschlagen.

`FermentationUiCommandBridge::decidePrepared` besitzt trotz `decisionOut`
noch eine lokale Decision im typisierten Visitor. `decision = decideX(...)`
erzeugt zusaetzlich das grosse Return-Objekt, danach folgt die Kopie nach
`*decisionOut`. Gemessen **20240 B pro Visitor-Spezialisierung**: 2×10104 B
plus 32 B. Rueckgabe-ABI/Assignment-Temporary, keine dritte neue Modellart.

Darunter kopieren Startentscheidungen `auto candidate = decision.before`
nochmals 4896 B. Program-Start haelt gleichzeitig u.a. `optional<ActiveRun>`
2824 B und Snapshot-Optional 40 B: **8000 B** in `decideProgramStartInto`;
Manual-Start **5216 B**. Stop/Completion je5136 B, Adjustment5280 B. Ein bereits
vorhandener In-place-Name bedeutet somit noch keinen kleinen Stackframe.

## Tatsaechlich erreichbare Web-Kette und Phasen

In beiden Profilen belegen die originalen `.ci` die direkten Kanten:

```text
WebRunMutationHandler::handle (544)
  -> applyRunMutation (2048)
     -> FermentationApplication::applyConfirmedPrepared (10880)
```

**Gemessene Einzelwerte**, **errechnete Untergrenze 13472 B**, Task8192 B.
Callsites werten Rueckgaben aus und zerstoeren lebende Requests/Decision;
kein ersetzender Tailcall dieser drei Funktionen. Aufruf ueber den HTTP-
Adapter/Dispatcher ist produktiv registriert; Session/CSRF/Origin/Sequenz/
Digest und bestaetigter Request sind Vorbedingungen, kein toter Diagnosepfad.

Web-Codec erlaubt start-program, start-manual-timed, start-manual-holding,
stop-run, complete-run, adjust-run, ack-message und mute-message. Prepare-
und Confirm-Phase (Confirm-Frame896 B) laufen **vor** Apply und werden nicht
faelschlich gleichzeitig zur Apply-Kette addiert. Program-Start gelangt im
Visitor zu `decideProgramStart` → `decideProgramStartInto`; die statische
Visitor-Tabelle ist ein typisierter indirekter Dispatch. Variantenbezug wird
zusaetzlich durch Codec, `request.storage()` und Template-Spezialisierung
belegt; allgemeine indirekte Calls sind damit nicht vollstaendig aufgeloest.

Fuer einen bestaetigten Program-Start ergibt sich bereits die konservative
weitere Untergrenze **13472+20240+8000 =41712 B**, ohne Bridge-/Visitwrapper,
SDK oder weitere Domainunterframes. Kein Gesamtheap-/Stackmaximum. Fuer
Ack/Mute ist bereits der Visitor gross, obwohl deren owning Apply RAM-only
ist. Physische Aktorwirkung wird daraus nicht abgeleitet.

Nach Ende der Decision-Phase fuehrt Application entweder den bestehenden
RAM-Ack/Mute-Owner aus oder den Persistence-Owner. Letzterer verwendet bereits
`RunPersistenceCoordinator::workingSet_` (Candidate/Snapshot/Record, **12752 B**
inline im vorhandenen Coordinator), kein neuer Stack-WorkingSet. Frames:
FreshStart48 / persistCommand160 → Internal720 → writeSnapshot208 → Core1168 B;
danach bestehender Store-/Readback-/NVS-Pfad. Decision-Visitor und Persistenz-
Phase sind nacheinander: deren Maxima **nicht miteinander aufsummieren**.

SDK-/NVS-/Port-vtables, dynamische Bibliotheksaufrufe, Validation/Codec-
Rekursion, weitere Callbacks und vollstaendige Taskpraefixe sind nicht als
abgeschlossene Obergrenze aufgeloest. Die angegebenen Kanten/Untergrenzen
beweisen FAIL bereits ohne sie; fehlende Frames sind nie 0 B oder PASS.
Settings bleiben SETTINGS_FULL_CHAIN_STACK=NOT_PROVEN.

## Isolierte Compilerexperimente, kein Implementierungsnachweis

| Variante | Kontrollierte Aenderung ausschliesslich in `/tmp` | Frame vorher → nach B |
|---|---|---:|
| A | Nur Application-Decision in typed aligned/nothrow-Heapowner, lokaler Zugriff per Referenz | 10880 → **784** |
| B Start/Manual | Visitor nutzt vorhandenen Destination statt lokaler Decision; Aufruf vorhandener `decide*StartInto` | 20240 → **32** |
| B andere Varianten | Destination statt lokaler Decision, bisherige Value-Returns bleiben | 20240 → **10144** |
| C Program | Kandidat aliasiert `decision.after`, keine Selbst-Move-Zuweisung, Restore aus `before` auf Fehler/unconfirmed | 8000 → **3104** |
| C Manual | Dieselbe isolierte Kandidatenverwendung mit Restore | 5216 → **320** |

A wurde nur uebersetzt: der OOM-Zweig liefert im Probe ein leeres Ergebnis;
**dieser Stub ist kein genehmigter OOM-Vertrag**. B setzt einen nicht-null
Destination voraus und ersetzt nicht die kompatible Nullable-API. C entfernt
`decision.after = std::move(candidate)` bei Alias und stellt `after` vor
fehlgeschlagenem/unbestaetigtem Ruecklauf aus `before` wieder her. Sonst waeren
Selbst-Move oder eine veraenderte abgelehnte Entscheidung moeglich.
**Keine dieser Varianten wurde gelinkt oder semantisch getestet.** Die
Reduktionen sind reale Compilerwerte, keine Behauptung lauffaehiger Korrektheit.
Daraus folgt kein Produkt-PASS.

Nach A+B+C haetten bereits nur Handler544 + Run2048 + Application784 +
Programvisitor32 + Start3104 = **6512 B** gleichzeitig Platzbedarf. **Maximal
1680 B arithmetischer Rest**, noch ohne weitere Wrapper/Validation/SDK/NVS.
Das ist keine begruendete Reserve und keine Vollkettenobergrenze. B allein
laesst andere Web-Varianten sogar einzeln ueber8192 B. **Root-only-Fix verworfen;
eine kleine sichere Gesamtloesung ist derzeit nicht bewiesen.**

## Bevorzugter begrenzter Kandidat und Heapbilanz

Nur als zusammenhaengende Hypothese fuer Plan §13: ein transienter Owner des
**bestehenden `CommandDecision`**, non-null In-place-Bridge, vorhandene Into-
Startpfade und gleichwertige Into-Kerne fuer die anderen erreichbaren
Command-Varianten; Kandidaten nach Moeglichkeit in vorhandenem `after`, mit
identischen Erfolgs- und Ablehnungswerten. Value-/Nullable-Konsumenten bleiben
kompatibel und delegieren; die Application ruft direkt den kleinen Eintritt.
Kein neuer Command-/Persistenztyp, kein zweiter Execute-/Writepfad.

Betroffene Produkte: `fermentation_application.*`, `fermentation_ui_commands.*`,
`run_commands.*`; direkte Tests. Web/Run-Persistence bleiben fachlich dieselben
Verbraucher, keine produktive Aenderung an Coordinator-/Store-/Wirevertrag
in diesem Kandidaten. Reicht dies nicht, STOP/Neubilanz statt Scopeausweitung.

Normalfall: hoechstens **ein** neuer Decision-Block, **10104 B** Request,
Alignment8, Handle4 B. Bestehende before/after-Unterpuffer werden von ihren
Stackcontainern in denselben Heapowner verlagert, nicht als weitere ganze
Modelle kopiert. Candidate-Kopien sollen entfallen. Keine dauerhafte
Heapreservation und kein Nachallokieren nach dem Root-Commit.

Gepinnter uninstrumentierter interner TLSF-Pfad wie Q3: 4-B-Header,
unsplittbarer Rest maximal12 B; groesste Klassenausrundung fuer10104 B bzw.
8-B-Alignment-Suche10104+8+16=10128 B ist **10240 B**. Konservative Belegung
**10240+12+4 =10256 B**. Zusammenhaengender Request10104 B; Allocator-Suchblock
kann mindestens10240 B verlangen, konservativ10256 B vorsehen, trotzdem keine
Fragmentierungs-/Allokationserfolgsgarantie. Es ist **keine neue Budgetfreigabe**.

| Pfad | Neuer fixer Request gegen Produkt095f393 |
|---|---:|
| unconfirmed/fehlender Kontext vor Scratch | 0 B |
| Scratch-OOM | 0 B erfolgreich, keine Persistenz/Run-RAM-Anwendung |
| Decision, Ablehnung, Ack/Mute, Persistenz/Readback, Indeterminate-Ruecklauf | maximal10104 B, danach0 B |
| spaetere Aufloesung/Recovery | kein gehaltener Decision-Owner, bestehender Recoveryowner unveraendert |

Vorhandene Coordinator-WorkingSet-/Snapshot-/Recordpuffer bleiben; sie werden
nicht als neuer Heap doppelt bilanziert. Neue Deepcopies, versteckte Value-
Temporaries oder laengere Unterpufferueberlappung sind ausserhalb dieser
Objektbilanz und muessen vor Freigabebewertung vermessen werden. Ein absolutes
Maximalheap-Delta aller dynamischen Unterpuffer ist noch NOT_PROVEN;10104/
10256 B sind die **fixe Ownerkomponente**, keine solche Gesamtzusage.
Falls Q3 und dieser Owner in anderen Kontexten ueberlappen koennen, konservativ
11960 B Objektrequests bzw.12192 B Allocatorbelegung gegen Vor-Q3 rechnen;
keine globale Exklusivitaet aller Konfigurations-/Recoveryclients unterstellen.

Alternativen: permanenter Application-Decision-Owner kostet dieselben10104 B
ueber die ganze Laufzeit; keine automatische Empfehlung. Vorhandenes Coordinator-
WorkingSet kann nicht blind als Decision oder freier Speicher ueberlagert
werden: es wird beim Persistieren noch neben der Decision benoetigt. Ein
zweiter WorkingSet waere weiterer12752-B-Bedarf. Request-Queue/Worker und
pauschale Stack-/Heapvergroesserung werden nicht vorgeschlagen.

Historisches H-RAM-1 (anderes Overlay M) Minimum19896 B:10256 B entsprechen
ca.51,5 %, rechnerischer Rest9640 B **keine aktuelle Prognose**; aktiver Run
war dort nicht qualifiziert. F4 bleibt PROVISIONAL, UI-Skip <13312 B / Resume
16640 B, keine Reservierung. WLAN/HTTP/LVGL, TOCTOU, Fragmentierung und
nachgelagerte OOMs bleiben Risiken. **RAM_PASS=NOT_PROVEN.**

## Vertragsgrenzen und Entscheidung

Run-/Command-/Replay-Vertrag bleibt Grundlage. OOM vor Owning-Apply darf nicht
zu `fromRunPersistenceResult(Busy)` mit vorgespiegeltem OwningOutcome werden;
CommandStatus::CapacityReached bedeutet im Web TooLarge und ist kein OOM-
Ersatz. Bevorzugte explizite Planoption: neues internes Detail
`FermentationUiDetailStatus::ResourceUnavailable` mit bestehender Kategorie
Unavailable und **DecisionOnly**, unveraendertem Web-Unavailable-Wireoutcome.
Auch diese kleine Typenergaenzung ist **nicht freigegeben**, nicht im Probe
implementiert. Ohne akzeptierten ehrlichen OOM-Vertrag keine Produktumsetzung.

Die HTTP-Sequenz ist bei Application-Aufruf bereits reserviert. Der bestehende
Handler schliesst sie mit Unavailable ab und speichert dieses Replay-Ergebnis;
identischer Replay darf nicht neu allokieren/persistieren. Keine Ruecknahme
bereits vergebener Request-IDs oder neue Wiederholungsregel. Persistierte
CommandIDs, AlreadyPersisted/AlreadyProcessed, Indeterminate, Root-/Readback,
Write-before-Apply, bestaetigte Requests und RAM-only Ack/Mute bleiben.
Kein Safety-/GPIO-/Boot-/Recovery-/Aktorfreigabeverhalten aendern.

**Ownerempfehlung:** Den Root-only-Fix nicht freigeben. Der oben quantifizierte
zusammenhaengende Kandidat ist die kleinste hier untersuchte plausible Richtung,
aber **SAFE_SMALL_FIX=NOT_PROVEN; IMPLEMENTATION_READY=NO**. Restkettenreserve,
alle Web-Varianten, Unterpufferpeak und der neue interne OOM-Detailvertrag sind
nicht als sicher qualifiziert. Vor einer Produktfreigabe sind genau diese
Punkte im gezielten Planreview zu entscheiden/nachzuweisen; keine allgemeine
C0–C5-Neubewertung. Weitergehende Eingriffe erfordern Neubilanz/Ownerentscheid.

Keine Regressionen/Pre-Ready/Builder-Self-Check oder neuen Firmwarebuilds in
diesem Diagnoseauftrag. Vorhandene Q3-Nachweise bleiben unveraendert gueltig.
D9/Hardware/HWM ist getrennt und NOT_RUN; ACTUATOR_RELEASE=NO.


## Fixe Verifikation dieses Diagnose-Deltas

ABI-Symbole und kontrollierte Frameproben beider Profile reproduziert;
vier direkte Run-Kanten (zwei je Profil) gegen originale `.ci` geprueft.
Produktdateien/Tests gegen HEAD84fa669 und bestehende BIN-Digests gegen
Produkt095f393 unveraendert. Lokale Dokumentlinks/Anchors und Diff pruefen;
keine neuen Produkt-PASS-Aussagen. Die fruehere Builder-/Test-Evidence bleibt
auf ihren alten SHAs, kein neuer Self-Check fuer dieses reine Plandelta.

Reproduktion der ABI-Methode: Produktionsheader `fermentation_application.hpp`,
`run_commands.hpp`, `run_persistence_coordinator.hpp` in temporaerer TU;
je Typ `extern "C" { unsigned char size_T[sizeof(T)];
unsigned char align_T[alignof(T)]; }`. Fuer den privaten WorkingSet dient
nur in der Probe der schon deklarierte Friend-TestAccess als Aliaszugriff.
Den originalen Application-Compile-Eintrag des jeweiligen Profils mittels
`shlex.split` auswerten, `-c` und `-o` nach `/tmp` ersetzen, Ziel-nm
`-S --radix=d`; keine Ausfuehrung/kein Link. A/B/C verwenden dieselben Flags
und die oben exakt beschriebenen isolierten Quelltransformationen; GCC-SU
enthaelt die gemessenen Frames. Kein Source-/Toolchainwechsel fuer die Proben.
