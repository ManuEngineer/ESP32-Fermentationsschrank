# PR203 / RES-01 – independent G1 overlay and Gate review

**REVIEW ONLY – NOT FOR MERGE. No PR/merge/release/hardware authorization.**

This separate GitHub branch contains the existing diagnostic commit
`bb4f43a69740f234d26a951707991f792cc3ffde` plus documentation only. It must never be merged into
PR203 or main. Product PR203 remains unchanged at `d35ee85edf4f93f20e9edbe13f8851e406d1b82b`.
The exact build/image source is **bb4f43a69740f234d26a951707991f792cc3ffde**, not the newer publication HEAD.
No source, runtime configuration, runner behavior, build or artifact was changed
for this publication; no hardware action or new tests/builds performed.

- [Concrete G1 Gate](G1_GATE.md)
- [Test sheet / load, stop, recovery and real touch rules](G1_TEST_SHEET.md)
- [Artifact / rollback / source / runner manifest](G1_ARTIFACT_MANIFEST.json)
- [Measurement limits and NOT_RUN states](NOT_RUN_AND_LIMITS.md)
- [Prior preparation commands](COMMANDS.md)
- [Owner publication instruction](OWNER_REVIEW_ORDER.md)
- [Publication checksums](SHA256SUMS)
- [Exact diagnostic overlay vs reviewed product](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/compare/d35ee85edf4f93f20e9edbe13f8851e406d1b82b...bb4f43a69740f234d26a951707991f792cc3ffde)
- [Original pinned source test sheet](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/blob/bb4f43a69740f234d26a951707991f792cc3ffde/diag/G1_TEST_SHEET.md)

Independent review should cover the whole overlay, the tests and the concrete
Gate. Prior Builder evidence is not an independent PASS or Hardware PASS.
Source/runner byte hashes refer to the original files at `bb4f43a69740f234d26a951707991f792cc3ffde`;
the normalized publication copies are identified separately by `SHA256SUMS`.
Images/ELFs/config binaries, private device readbacks and raw local logs are
not uploaded. Their hashes/sizes and offline-verified rollback availability
remain reviewable. The original sealed45-file packet stays unchanged locally;
its seal and original Gate hashes are metadata in the published manifest.

Await Owner review/decision before ANY UART, device query, reset or flash.
Calibration/NVS and actual factory reset remain outside this Gate; each physical
Touch/Hold sequence later needs instructions and Owner confirmation.
First proposed Gate covers only one P-A/Recovery plus controlled real reference
coverage; missing diagnostic evidence stays OPEN_G1, pure-product confirmation
OPEN_G2. Full P-A3x/P-A2/P-B/P-R matrix remains open. Missing real peak/path/touch
coverage => NOT_PROVEN / threshold HOLD; sampled demand is not an OOM guarantee.

OWNER_G1_IMAGE_AUTHORIZATION=PENDING; RES-01=BLOCKER; PR203_FINAL_GO=HOLD;
G2=NOT_AUTHORIZED; ACTUATOR_RELEASE=NO; PRE_READY=NOT_RUN.
