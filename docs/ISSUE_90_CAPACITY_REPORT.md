# Issue #90 NVS capacity evidence

- source git SHA: `2f6b2c67a3c880caac10efd1acbc8df7c0eab976`
- ESP-IDF: `v6.0.2 @ 7101770dc6db2667b3c477cc31365dd1acd6db4e`
- pinned NVS constants: `32` B/entry, `126` entries/page, `4000` B/chunk
- status: arithmetic evidence only; real NVS statistics and GC remain hardware/BDL evidence
- inventory provenance: keys and limits are mechanically parsed from the canonical sources below; contract drift fails this command

| Record group | Key inventory | Slots | Max bytes | Entries/record | Entries | Canonical sources (keys / limits) |
|---|---|---:|---:|---:|---:|---|
| configuration.user | `uc0, uc1, uc2, uc3` | 4 | 301 | 12 | 48 | `lib/fermentation_app/src/configuration_storage_contract.hpp; lib/fermentation_app/src/configuration_limits.hpp` |
| configuration.service | `sc0, sc1, sc2, sc3` | 4 | 45 | 4 | 16 | `lib/fermentation_app/src/configuration_storage_contract.hpp; lib/fermentation_app/src/configuration_graph_store.cpp` |
| configuration.program | `pc0, pc1, pc2, pc3` | 4 | 32813 | 1036 | 4144 | `lib/fermentation_app/src/configuration_storage_contract.hpp; lib/fermentation_app/src/configuration_limits.hpp` |
| configuration.manifest | `cm0, cm1, cm2` | 3 | 149 | 7 | 21 | `lib/fermentation_app/src/configuration_storage_contract.hpp; lib/fermentation_app/src/configuration_limits.hpp` |
| configuration.root | `cr0, cr1` | 2 | 114 | 6 | 12 | `lib/fermentation_app/src/configuration_storage_contract.hpp; lib/fermentation_app/src/configuration_limits.hpp` |
| configuration.bootstrap | `cb0, cb1` | 2 | 42 | 4 | 8 | `lib/fermentation_app/src/configuration_storage_contract.hpp; lib/fermentation_app/src/configuration_limits.hpp` |
| run.checkpoint | `rc0, rc1` | 2 | 8240 | 262 | 524 | `lib/fermentation_app/src/run_persistence_store.cpp; lib/fermentation_app/src/run_persistence_coordinator.cpp; lib/fermentation_app/src/run_persistence_codec.cpp` |
| run.head | `rh0` | 1 | 256 | 10 | 10 | `lib/fermentation_app/src/run_persistence_store.cpp; lib/fermentation_app/src/run_persistence_coordinator.cpp; lib/fermentation_app/src/run_persistence_codec.cpp` |

Persistent recordbestand (without namespace): `4783` entries.
Persistent inventory including namespace: `4784` entries.
Peak with one simultaneous `32813`-byte replacement: `5820` entries.
Two free pages for update/GC reserve: `252` entries.
Deterministic minimum: `6072` entries = `49` pages = `196` KiB.
Selected R1 adapter partition: `69` pages = `276` KiB; `20` pages remain above the computed minimum.

The existing 24 KiB baseline is smaller than the single 32,813-byte program record before NVS metadata/chunking; it cannot host that record. The selected size is not a claim about unrelated production reservations. The report must be followed by real partition, readback, GC/erase, resource and flash verification.
