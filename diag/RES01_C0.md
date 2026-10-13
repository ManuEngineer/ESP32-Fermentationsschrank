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

## Prepared G0 test sheet (no hardware run yet)

- P-A: four fresh sessions, three concurrent API GETs per session every 5 s,
  sessions offset by 1.2 s; a shell GET every 30 s per session, first at its
  second tick. 1200 s load. New connection per request, explicit client close,
  3-s timeout. No re-login, no logout on any outcome.
- Before load: 60 s idle with those same four sessions, all runner sockets
  closed. Use current free heap/largest block in the v3 `diag_30s` UART samples,
  not boot minima. Record actual session validity/network/connection state.
- After load: stop all requests, close sockets, retain four sessions, observe
  300 s recovery. The 90%/120-s recovery comparison needs a comparable server
  socket/network state as well as client closure. A client-side close alone
  does not prove the server has reclaimed resources: document the server's
  cleanup and matching idle state; if comparability cannot be established,
  mark recovery NOT_PROVEN/HOLD rather than assuming 42.3 KB is the reference.
- UART anomalies are watched every 50 ms independently of request ticks;
  close active sockets on the first anomaly/non-200/request failure. Do not
  perform logout, re-login, reset, next phase or fallback flash after an anomaly.
  Bounded in-progress connect attempts have the same 3-s timeout.
- Use the same runner hash, image profile, AP/client placement, boot/session
  age, request schedule and measurements for v3 baseline and chunk candidate.
  Historical D1 API-only data are context; historical short D2/D3 runs are
  not an automatic same-load reference for the entire 20-min campaign.
  Any baseline anomaly also stops work; do not automatically continue with
  the next image. Go needs an established comparable resource reference.
- Latency decision fixed before test: HTTP must be 200, no failed connections
  or 3-s timeouts. API/Shell p50/p95/p99/max are separate per minute/profile;
  latency distributions/trends are findings, not a hard D1 p99 gate. No extra
  p99 threshold is invented after the run. If the Owner chooses a stricter
  same-load bound, record it in the test sheet before starting either run.
- Go: measurable shell peak reduction, no worse heap/block reserve under
  identical comparable load, candidate without allocation failure/reset or
  HTTP/timeout failure. TX-buffer counts are NOT_INSTRUMENTED, hypothesis
  only; no claim about unchanged counts. G0 does not satisfy G1/G2.
- Exact candidate commit/image/ELF hashes, immutable image paths, rollback
  image/partition hashes, stop conditions and local results are recorded in
  `hw-evidence-pr203/diag/c0/REVIEW_PACKAGE.md` before requesting G0.

The prepared runner never captures UART or flashes by itself. The actual
run requires separate Owner G0 approval and a pre-existing live UART capture.
Opening FT232 can reset the board and is itself hardware-gated. Example only:

```sh
python3 diag/c0_load.py --run --gate-id OWNER_APPROVED_G0 --host 192.168.1.68 --uart /approved/capture/uart.log --output /new/unique/run-directory
```

No command above has been executed against the ESP32 in C0 preparation.
Independent review, C0 hardware Go/No-Go, final product Pre-Ready and G1/G2
remain outstanding. C1–C3 start after the C0 decision according to the plan.
