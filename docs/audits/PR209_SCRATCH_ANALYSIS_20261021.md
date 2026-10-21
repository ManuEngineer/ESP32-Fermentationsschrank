# PR #209 – quantitative Scratch-Analyse vor Q3

2026-10-21. **ANALYSIS_ONLY; Q3 NICHT FREIGEGEBEN.** Untersucht wurden
Produktquellen auf `3816450f87aabc0a0ed3e00aa606cf4ab6644718`, Branch
`agent/issue-27-r1-web-navigation-plan`, offener Draft-PR #209 / Issue #27.
Freigegebene Implementierungsbaseline bleibt Planrevision 2.2
`b7d5e2dbc1f5281c764d4a75f2c37f1c4b6397c2`. Diese Evidence ergaenzt ausschliesslich
[Gesamtplan Revision 2.3 §12](../tasks/issue-27-r1-web-remaining-scope-plan.md#12-httpd-settings-stackbefund-und-vorgeschlagene-korrektur-q3-nicht-freigegeben).
Kein Produktcode, keine Stack-/Heapbudgets und keine Commitvertraege geaendert.
Keine Regressionen, Profilbuilds, Pre-Ready- oder Geraetelaeufe ausgefuehrt.

## Messung und Provenienz

**XTENSA_ABI_MEASUREMENT=PASS** fuer beide bestehenden Profile
`esp32_release`/`esp32_bringup`. Compiler:
`xtensa-esp32-elf-g++`, GCC 15.2.0, crosstool-NG `esp-15.2.0_20251204`, aus
`/var/lib/docker/data/engineering/home/manuel/.espressif/tools/xtensa-esp-elf/esp-15.2.0_20251204/xtensa-esp-elf/bin/`.
ESP-IDF v6.1 `fff9895c82d744c7237be8847347bdd1b07c6643`.
Jeweils der reale `configuration_service.cpp`-Eintrag aus
`build/<profil>/compile_commands.json`, einschliesslich Includes, Defines,
`@toolchain/cxxflags` und Picolibc-Specs; nur Quell- und Objektziel nach `/tmp`
ersetzt. Wirksames `-std=gnu++17`, `-Og`, `-mlongcalls`, Exceptions/RTTI aus.
Kein Hostcompiler, keine Ausfuehrung von Zielcode, kein Linken/Flashen.

| SHA-256 | Release | Bring-up |
|---|---|---|
| Originaler Compile-Befehl (UTF-8 ohne Zusatznewline) | `72217cebe7d5ef7e750b3c49520ac9adf9fe4ac723d59ae21c1bac52395640ee` | `ce1fa5dc6436fe091b94c1ce8053d1e34e2691e7635500422ddf32c596b00450` |
| `compile_commands.json` | `c9bf8e3f4bb3cd5c73b1bf4be26760944de5134e172755c1bf42e1c21ec279b5` | `4218355744f47b5a56dec71f70be2cb61ec38f2439dea228841ac8ae371f2df1` |

[Rohe Symbolgroessen](PR209_SCRATCH_ABI_20261021.tsv): Die Laenge eines
uninitialisierten Bytearrays ist exakt der jeweilige Compilerwert von
`sizeof`/`alignof`; ausgewertet mit Xtensa `nm -S --radix=d`, ohne Host-ABI.

| Typ | `sizeof` B | `alignof` B, beide Profile |
|---|---:|---:|
| `LoadedConfigurationGraph` | 552 | 8 |
| `PreparedConfigurationCommit` | 1288 | 8 |
| `ConfigurationCommitPrepareResult` | 1304 | 8 |
| `ProposedCommitScratch` (nur analytisches Layout) | 1856 | 8 |
| `ResolutionLayout` (Feldspiegel des vorhandenen privaten Typs) | 1512 | 8 |
| `BindingLayout` (Feldspiegel des vorhandenen Runtime-Bindings) | 200 | 8 |
| `void*` / `std::string` / `unique_ptr<ProposedCommitScratch>` | 4 / 24 / 4 | 4 / 4 / 4 |

Zusaetzlicher Offline-Abgleich an beiden vorhandenen Profil-ELFs auf `3816450` mit
Xtensa-GDB 17.1: `sizeof` der drei Produktionstypen = 552/1288/1304;
`sizeof('fermentation::ConfigurationService::ResolutionContext')` = **1512**;
`sizeof(fermentation::RuntimePreparationBinding)` = **200**. Damit sind die
Feldspiegelgroessen am echten privaten Debugtyp gegengeprueft.

## Layout und Lebensdauerbilanz des Vorschlags

Der vorgesehene analytische Scratch ist ausschliesslich:

```cpp
struct ProposedCommitScratch {
    LoadedConfigurationGraph current;
    ConfigurationCommitPrepareResult prepared;
};
```

`prepared` enthaelt bereits `optional<PreparedConfigurationCommit>`; dessen
1288 B duerfen nicht nochmals auf 1856 B addiert werden. Der lokale
`unique_ptr`-Handle hat 4 B, keinen Heapkontrollblock. Die spaetere fehlertolerante
Allokation muss **8-B-Alignment** einhalten. Es gibt hoechstens einen Scratch
pro durch den vorhandenen Mutation-Lease serialisiertem Commit. Kein Pool,
keine Queue, kein ueber einen Aufruf hinaus gehaltener Scratch.

Quellen des Ist-Vergleichs: `ConfigurationService::confirmPreview` in
`configuration_service.cpp` (lokaler `current`, Ergebnis `prepared`,
`preparedRuntime`, `publishedGraph`, `runtimeBinding`, `resolution`,
`retired`/`retiredGraph`); `ConfigurationGraphStore::prepareCommit` und
`executePreparedCommit` in `configuration_graph_store.cpp`.

| Phase/Pfad | Gleichzeitig lebende transiente Objekte | Zusaetzlicher angeforderter Heap gegen Ist |
|---|---|---:|
| NoChange/stale/abgewiesen vor Scratch | Vorhandener Preview-/Kandidatenowner, Pruefwerte/Lease; kein Scratch | 0 B |
| Scratch-Allokation scheitert | Vorhandene Owner/Lease; kein konstruierter Scratch; Busy ohne persistentes Schreiben und mit Leasefreigabe ist zu verifizieren | 0 B erfolgreich belegt |
| Normaler Prepare | Scratch: `current` und Ergebnisoptional; vorhandene Kandidaten-Shared-Pointer; Scan/Slotplan, Validierung und Codec-/Read-Puffer | 1856 B |
| Prepare-Fehler | Derselbe Scratch, je nach Fehler nur teilweise gefuelltes Ergebnis, vorhandene Pruef-/Read-Puffer; Freigabe bei Ruecklauf | maximal 1856 B, danach 0 B |
| Runtimevorbereitung und Handoff | Scratch **1856 B**, vorbereiteter Publish-Graph **552 B**, vorhandener Resolution-Owner **1512 B**, Runtime-Snapshot/PreparedTimeZone und Binding; kurzzeitig verschobenes Prepared-Quellobjekt im Scratch weiterhin lebend | **1856 B**, Objektablagen insgesamt **3920 B** |
| Execute, Aktivierung/Retirement | Scratch plus bestehender Resolution-Owner mit Prepared, Publish-Graph/Runtime; Payload/Record/Readback/Verifikation; alter Graph/Runtime bis Retirement freigegeben | maximal 1856 B; alte veroeffentlichte Owner und Puffer gehoeren bereits zum Ist-Pfad |
| Normaler Ausfuehrungs-/Runtimevorbereitungsfehler | Bis zum Ruecklauf hoechstens dieselbe Ueberlappung, kein zweiter Scratch; vorhandener Cleanup/Retirement-Vertrag | maximal 1856 B, danach 0 B |
| `ConfigurationCommitIndeterminate` / aufbewahrter `RuntimeFailure` | Scratch und Resolution ueberlappen beim Transfer; vorhandener Resolution-Owner wird in `resolutionContext_` verschoben und bleibt; Scratch stirbt beim Ruecklauf | maximal 1856 B, **0 B nach Rueckkehr**; bestehender Resolutionbedarf bleibt |
| OOM in nachgelagertem bestehendem Puffer/Runtime/Graph/Resolution | Ein Scratch plus bis dahin vorhandene Owner/Teilallokationen; kein automatischer zweiter Versuch/Scratch | Scratchanteil maximal 1856 B; allgemeine OOM-Behandlung dieses Ist-Pfads **NOT_PROVEN** |

In-place bedeutet: Ergebnisoptional direkt befuellen, `newGraph` und seine
Branches dort aufbauen; kein gleich grosser Ergebnis-Rueckgabewert und keine
zusaetzliche Prepared-/Graph-Heapkopie. Bei Uebergabe in den bereits vorhandenen
Resolution-Owner werden die String-/Shared-/Unique-Owner verschoben, ohne
neue String-/Modellkopien oder grosse Stack-Aggregattemporary. `current` und
bereits heute notwendige Stringkopien behalten ihre Ist-Lebensdauer. Die
Objektverlagerung erzeugt somit **0 B zusaetzliche dynamische Unterpuffer**;
deren bestehender Absolutbedarf ist damit ausdruecklich nicht null.
Das ist eine pruefbare Begrenzung des vorgeschlagenen Deltas, noch kein
Implementierungsnachweis. Mehr/länger lebende Kopien invalidieren die Bilanz.

**Peak-Delta der angeforderten Objektablagen: 1856 B**, nicht
1856 + 1288 + 1304 B und nicht 3920 B. Die schon vorhandenen 552 + 1512 =
2064 B Heapobjekte sowie Runtime-, Modell-, String-, SDK-/NVS-/Netzpuffer sind
in beiden Vergleichspfaden enthalten. Das absolute Gesamtheapmaximum dieses
Commits ist offline nicht aus den `sizeof`-Werten ableitbar.

### Allocatoraufschlag und zusammenhaengender Block

Gepinnter IDF-Quellpfad: `components/heap/tlsf/{tlsf.c,
tlsf_control_functions.h,tlsf_block_functions.h}`, `heap_caps_base.c`,
`multi_heap_platform.h`. Beide Profilconfigs: Poisoning aus, Tracing aus,
Tasktracking aus; kein per-Block-Owneraufschlag. TLSF-Basisalignment 4 B,
Header 4 B, freier Blockheader 16 B; unsplittbarer Rest <16 B, also maximal
12 B. Je nach Pool ist `sl_index_count_log2` 3/4/5. Bei 1856 B Request ist die
schlechteste Klassenausrundung 1920 B (128-B-Schritt). Fuer explizites
8-B-Alignment sucht `tlsf_memalign_offs` zunaechst 1856+8+16 = 1880 B,
ebenfalls hoechstens auf 1920 B gerundet; die fuehrende Luecke wird wieder
freigegeben, kein zweites Scratchobjekt angefordert.

Damit konservativ **1920 + 12 + 4 = 1936 B** zusaetzliche Allocator-Belegung,
einschliesslich temporaerer Allocator-Ueberlappung. Dies gilt fuer genau den
genannten uninstrumentierten, normalen internen 8-Bit-Heap-Pfad; kein
DMA-/PSRAM-Sonderpfad. Eine zusammenhaengende freie Luecke kann fuer die
Alignment-/Klassensuche groesser als 1856 B sein muessen. 1936 B sind eine
analytische Aufschlagsgrenze fuer diesen einen Block, **keine Allokations- oder
Fragmentierungsgarantie**, keine neue Budgetfreigabe und kein gemessenes
`heap_caps_get_minimum_free_size`-Delta. Aenderungen an Layout, Allocatorpfad,
Instrumentation oder zusaetzlichen Pufferlebensdauern verlangen Neubilanz.

## Einordnung und begrenzte Empfehlung

[H-RAM-1](PR206_H_RAM_1_EVIDENCE_20261019.md) misst ein anderes Overlayimage M:
Idle intern 42760 B frei / 40960 B groesster Block; historisches Minimum
19896 B; beobachteter kleinster API-/Shell-Block 26624/22528 B. 1936 B sind
rund **9,7 %** jenes Heapminimums; 19896−1936 = **17960 B** ist allein eine
Rechengroesse. Die Messzeitpunkte und Phasen stimmen nicht mit diesem
Namenscommit ueberein; daraus folgt keine aktuelle nutzbare Reserve.

[Historische Machbarkeit/F4-Risiken](PR206_RAM_FEASIBILITY_20261019.md): F4
PROVISIONAL, Skip bei groesstem Block <13312 B, Wiederaufnahme ab 16640 B.
Das ist ein UI-Skip, keine Heapreservierung fuer HTTP-Commits. TOCTOU,
Fragmentierung und kumulative WLAN-RX-/HTTP-/LVGL-Allokation bleiben bestehen;
F1+F4 hatten unter Vier-Session-Stress bereits `heap_alloc_failed`.
Die neue einzelne Allokation benoetigt einen geeigneten Block und kann
andere Allokationen verdraengen. Keine Erhoehung/Umdeutung der F4-Schwellen.

**Empfehlung:** Hoechstens einen begrenzten Q3-Implementierungsversuch mit
diesem Ein-Scratch-Layout und unveraenderten Commit-/Recovery-/Golden-Byte-
Vertraegen autorisieren; keine Ressourcen- oder Hardwarefreigabe ableiten.
Erst nach Ownerfreigabe der neuen exakten vollstaendigen Plan-SHA werden
Produktdelta und bereits in §12 vorgesehene Regressionen ausgefuehrt.
Vollketten-Stackgate einschliesslich SDK/NVS gegen 8192 B bleibt unveraendert;
fehlender Reservebeleg bedeutet erneut STOP. Keine Budgeterhoehung.

**HTTPD_SETTINGS_STACK=FAIL** (Ist-Untergrenze 8528 B),
**C5_RESOURCE_GATE=HOLD**, **OPEN_BLOCKERS=1**, **RAM_PASS=NOT_PROVEN**.
Offline nicht bewiesen: Implementierung der Allokations-/Move-/Cleanupgrenzen,
allgemeine OOM-Festigkeit nach Scratch-Allokation, aktueller kumulativer
Maximalheap, geeigneter Block zur Laufzeit, Fragmentierung/F4-Wirkung,
korrigierte Vollkettenreserve und reale HTTPD-HWM. Historische HWM 2196 B ist
kein Nachweis fuer diesen Pfad. D9/Hardware/Flash/Browser/UART/Reset NOT_RUN;
ACTUATOR_RELEASE=NO. Q1/Q2 und alle bisherigen Gates bleiben unveraendert.

## Reproduzierbarer Offline-Probe (kein Produkttest)

Folgenden Text als `/tmp/pr209_scratch_probe.cpp` speichern. Die beiden
Layoutspiegel dienen nur der Zusatzbilanz; die vier geforderten Groessen
verwenden unveraenderte Produktionsheader bzw. das oben definierte Ownerlayout.

```cpp
#include "configuration_graph_store.hpp"
#include "runtime_configuration_snapshot.hpp"
#include "configuration_service.hpp"
using namespace fermentation;
struct ProposedCommitScratch {
    LoadedConfigurationGraph current;
    ConfigurationCommitPrepareResult prepared;
};
struct BindingLayout {
    device_platform::StorageEpoch storageEpoch;
    ConfigurationRootSequence rootSequence;
    ConfigurationManifestReference manifestReference;
    UserConfigurationReference userConfiguration;
    ServiceConfigurationReference serviceConfiguration;
    ProgramCatalogReference programCatalog;
    std::string preparedTimeZoneIdentifier;
};
struct ResolutionLayout {
    PreparedConfigurationCommit persistent;
    std::unique_ptr<LoadedConfigurationGraph> preparedGraph;
    std::shared_ptr<const RuntimeConfigurationSnapshot> preparedRuntime;
    BindingLayout runtimeBinding;
    ConfigurationCommitIndeterminateCause indeterminateCause;
    bool runtimePreparationRetryConsumed;
};
#define MEASURE(name, type) extern "C" { unsigned char size_##name[sizeof(type)]; unsigned char align_##name[alignof(type)]; }
MEASURE(LoadedConfigurationGraph, LoadedConfigurationGraph)
MEASURE(PreparedConfigurationCommit, PreparedConfigurationCommit)
MEASURE(ConfigurationCommitPrepareResult, ConfigurationCommitPrepareResult)
MEASURE(ProposedCommitScratch, ProposedCommitScratch)
MEASURE(ResolutionLayout, ResolutionLayout)
MEASURE(BindingLayout, BindingLayout)
MEASURE(pointer, void*)
MEASURE(string, std::string)
MEASURE(unique_owner, std::unique_ptr<ProposedCommitScratch>)
```

Im Repository den bestehenden Compile-Befehl beider Profile benutzen:

```python
import json, pathlib, shlex, subprocess
root = pathlib.Path.cwd()
for profile in ("esp32_release", "esp32_bringup"):
    entries = json.loads((root / "build" / profile /
                          "compile_commands.json").read_text())
    entry = next(e for e in entries
                 if e["file"].endswith("/configuration_service.cpp"))
    args = shlex.split(entry["command"])
    target = "/tmp/pr209_scratch_" + profile + ".o"
    args[args.index("-o") + 1] = target
    args[args.index("-c") + 1] = "/tmp/pr209_scratch_probe.cpp"
    subprocess.run(args, cwd=entry["directory"], check=True)
    nm = pathlib.Path(args[0]).with_name("xtensa-esp32-elf-nm")
    print(profile)
    print(subprocess.check_output([str(nm), "-S", "--radix=d", target],
                                  text=True))
```

Die dezimale Symbolgroesse von `size_<Typ>` bzw. `align_<Typ>` ist der
Messwert. Keine Hostausfuehrung/keine Hostgroesse wird dafuer verwendet.
