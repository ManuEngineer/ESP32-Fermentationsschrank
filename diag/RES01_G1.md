# PR203 RES-01 – temporary G1 overlay

Product base: independently reviewed d35ee85edf4f93f20e9edbe13f8851e406d1b82b.
Plan: 6222efce976dc3bea3cf397c2dca56a5fb21859d plus Owner Option1.
G1 is prepared only. Concrete image approval PENDING; no UART open, reset,
HTTP request or flash during preparation. No product PR/main changes.

Protocol: [G1_TEST_SHEET.md](G1_TEST_SHEET.md). External sealed preparation packet
contains concrete complete source/image/ELF/rollback/runner/probe hashes and diff.
Canonical release build on a clean commit; no F2/httpd/WLAN/product config change.
Owner is available for real touch. Give instructions and wait for each confirmation.
Calibration, NVS and actual factory reset remain outside this Gate.

Reuses reviewed C0 allocation-failure hook and HTTP pre/built/sent checkpoints,
adapted to product segmented sending without changing wire semantics. New
main-task UI probes use fixed records and checkpoint queries, never heap buffers;
report after outermost models/scopes end. Sampled peaks are lower bounds, not
proof of all transients. Parallel network activity contributes to observed heap.
IDF local-min monitor allocates its own bookkeeping, so it is not used.
Skipped paths remain unmeasured; no gate bypass, synthetic UI or calibration.

Offline regressions: C0 heartbeat/login/status/secret rules retained; new supervisor
abort and watchdog/probe markers; analyzer refuses qualification even with an
arithmetic threshold candidate; fake UART timeout without real wait/device; native
probe nested minima and deferred logging with zero C++ allocations/explicit overflow.
Targeted Builder self-check is not the final Pre-Ready run. Pre-Ready remains
separately Owner-gated on the final product HEAD after hardware qualification.

First proposed G1 Gate: one diagnostic P-A/P-R plus controlled real reference
coverage, then independent result review. Overall matrix remains
P-A3x20min/P-A2/P-B/P-R; missing diagnostic evidence stays OPEN_G1, final
pure-product confirmation OPEN_G2. No silent transfer of missing G1 evidence. Missing paths/touch/transient bounds => NOT_PROVEN /
THRESHOLD_NOT_QUALIFIED/HOLD. No automatic threshold/image edit or next run.

RES-01=BLOCKER; PR203_FINAL_GO=HOLD; G2=NOT_AUTHORIZED; ACTUATOR_RELEASE=NO.
