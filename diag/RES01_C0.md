# PR #203 / RES-01 – C0 software preparation (not for merge)

Owner-approved plan: `6222efce976dc3bea3cf397c2dca56a5fb21859d`,
`docs/tasks/issue-27-res01-heap-hardening-plan.md`. D1–D5 approved by the Owner
in the 2026-10-13 Markdown instruction. Product PR remains separate from this
throwaway diagnosis branch. No permission for flash, serial-port opening,
reset, device HTTP requests or hardware tests follows from software approval.
`RES-01=BLOCKER`, `PR203_FINAL_GO=HOLD`, `ACTUATOR_RELEASE=NO`.

Baseline: diagnosis v3 `3e94ceadc78b6ce2b4c5430be527d2a35dfa78d8` on
`agent/pr203-res01-diag-local`, based on product `ed29c9c`. This C0 spike
prototypes F1-4 only. C1/F4 is not installed: an actual OOM/abort/reset remains
possible and must stop a later hardware run. No sdkconfig, socket limit,
network setting, GPIO, actuator, storage or reset-flow changes are made.

The owned shell head reserves 1536 B; the original immutable script suffix
is a static literal and is sent through the pinned IDF `httpd_resp_send_chunk`.
The response field is diagnostic-only until C0-Go and subsequent product C2.
The 8192-B bound applies to head plus suffix. Answer helpers clear the suffix;
route dispatch also clears it on response reuse. Provisioning shell bytes and
its existing non-chunked path stay unchanged in this spike. This is not the
complete C2 implementation or its forced shell-overflow regression.

Native checks compare dechunked shell bytes to a test-only copy of the v3
shell generator, including existing state/sequence coverage; the golden file
is never part of the ESP firmware build. The real ESP-IDF Linux server tests
exercise head+suffix, an empty head, complete JSON 503/status/type without
HTML/JS, exactly one terminator, and error propagation at each chunk call.
Linux heap/stack stub values are not ESP32 resource evidence.

Local commands (diagnosis worktree; not full Pre-Ready):

```sh
pio test -e native -f test_web_application_routes -f test_network_configuration -f test_web_session -f test_web_browser_policy -f test_http_server_lifecycle
python3 -m unittest discover -s diag -p test_c0_load.py -v
python3 scripts/check_architecture_boundaries.py
export IDF_TOOLS_PATH=/var/lib/docker/data/engineering/home/manuel/.espressif
source /var/lib/docker/data/ESP32-Projekte/start.sh
idf.py -C test/esp_idf_http_server_adapter_host -B /tmp/pr203-c0-http-host-build build
/tmp/pr203-c0-http-host-build/issue27_http_server_adapter_host.elf
python3 scripts/build_esp_idf_profiles.py release --require-clean-source-tree
```

## Login driver correction after G0 STOP (2026-10-14)

The Owner's bounded correction instruction uses product PR HEAD
`8f32c97a688b26b121cf3c14405717ef2a8de493`, driver baseline
`70b3059ad5f429ff483d8e8aefab8300e6054dc4` and the same approved plan.
Only `POST /api/v1/login` now has an 8-s client socket timeout. API/shell
GETs and all other requests retain 3 s; no product timeout/KDF/session/HTTPD
configuration changes. These are `HTTPConnection` socket timeouts, not a new
wall-clock deadline implementation. HTTP errors and transport failures still
stop immediately, without retries. Evidence stores only the fixed classes
`timeout`, `connection-error`, `http-protocol-error`, `http-status-error`
(or null on HTTP 200), never exception messages, passwords, cookies, CSRF or bodies.

Rationale: [PR-170 hardware evidence](../docs/audits/PR170_HW_TOUCH_PATH_FINDING.md)
records regular login durations of 3.03–3.08 s. The Owner also cites roughly
3.2 s in earlier PR-203 runs. Additional preserved v3 `diag/run/load_D1..D3.json`
logs show eleven successful logins at 3445.9–3575.1 ms and one at 8225.7 ms.
That outlier is not a guaranteed success under the new 8-s socket budget; the
budget stays fixed and any new timeout remains FAIL/HOLD. The first G0 login failed after 3113.678 ms with
a 3-s socket timeout; the unrecorded exception type leaves its cause open.
The sealed old run `hw-evidence-pr203/diag/c0/g0_20261013_run01/` and product
[result](https://github.com/ManuEngineer/ESP32-Fermentationsschrank/blob/8f32c97a688b26b121cf3c14405717ef2a8de493/docs/audits/PR203_G0_20261013_RESULT.md)
remain unchanged: `G0=STOPPED`, `BASELINE_SETUP=FAIL`, `RAM_GAIN_COMPARISON=NOT_RUN`.

[The versioned G0 test sheet](G0_TEST_SHEET.md) defines the different login/load
budgets, monitoring and unchanged P-A schedule. Its early-baseline comparison
option is an **unapproved proposal for Owner decision**, not an active profile.
The same exact corrected runner must be used for baseline and candidate.
Offline regressions model elapsed socket time without real 5/8-s waits and
exercise a real HTTP 503 on host loopback. No new device interaction is authorized.

The new review packet is `hw-evidence-pr203/diag/c0/login-fix-20261014/`.
It contains the correction diff, source/test snapshots, logs, artifact hashes
and verification that all existing C0/G0 evidence files stayed byte-identical.
Frozen baseline/C0 firmware images and ELFs are unchanged; no firmware rebuild.

Next: independent Fix Verification of driver/tests/test sheet/evidence, then
Owner decision on reference comparability and a **new concrete G0 authorization**.
No UART-port opening, reset, flash, device requests, C1–C3 or G1/G2 in this correction.
`RES-01=BLOCKER`, `PR203_FINAL_GO=HOLD`, `G1_G2=NOT_AUTHORIZED`, `ACTUATOR_RELEASE=NO`.
