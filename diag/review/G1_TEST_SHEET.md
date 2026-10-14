> **REVIEW ONLY / NOT FOR MERGE.** Publication copy of the source-pinned test
> sheet, with publication links and local credential-file wording normalized.
> Actual source/test-sheet hashes in the manifest refer to the original files
> at firmware commit `bb4f43a69740f234d26a951707991f792cc3ffde`.
> No profile, budget, measurement or acceptance rule changed. No flash authorized.

# PR203 RES-01 – G1 first image / first measurement gate

Status: **PREPARED, OWNER_G1_IMAGE_AUTHORIZATION=PENDING**. This file is the
versioned protocol; its concrete source/image/ELF/runner/instrumentation hashes
are bound in the published `G1_ARTIFACT_MANIFEST.json` and `G1_GATE.md` after a clean
build. These files do not grant their own approval. No hardware action yet.

Product base is exactly reviewed `d35ee85edf4f93f20e9edbe13f8851e406d1b82b`;
Owner reports Independent Software Review PASS/GO. Approved plan6222efc and
explicit Option1 remain binding: **PROVISIONAL_PENDING_G1**, ESP32
13312 B skip /16640 B resume; vector-block bound6656 B.
G1 result review → bounded corrections/Fix Verification if needed → separately
approved G2 pure product → Pre-Ready on final qualified PR HEAD. No Pre-Ready now.

## Exact proposed hardware scope (requires concrete Owner G1 confirmation)

One G1 diagnostic app-only flash at **0x10000** on the permanently attached,
Owner-confirmed actor-free Devboard, USB FT232R A5069RR4. Expected ESP32 MAC
**20:50:0d:1b:2f:34**. Expected partition-table SHA256
**d7f180e4ea98d457222bf134454694937dc7d3ca31a80623765ad5d18d7ccd9d**.
USB symlink is enumerated now; MAC/partition/current C0 firmware are historical
G0-R3 evidence, **not a new device observation**. Before write, under this
concrete Gate, reverify MAC, partition, current app and full artifact hashes;
any unexpected identity/state => HOLD, no repair. Historical last image is C0
9a9a8bd; do not assume it without fresh preflight. Readback0..0xffff remains
private. Compare this full region before/after app write: bootloader, partition,
NVS/PHY byte-identical. No erase-all, non-app write, calibration provisioning,
NVS repair or configuration changes. App-sector erase inherent to writing the
app is confined to that partition; no explicit erase operation.

Expected flash layout is also decoded offline from the candidate partition
artifact and compared bytewise with the pinned G0 table before Gate submission.
Pinned rollback artifacts are **available only**, never automatically flashed:
last successful C0 app+ELF, older ed29c9c app and original factory readback;
all complete hashes are in the Gate manifest. Any rollback requires new consent.

Gate covers only necessary controlled preflight boot/ROM reads, this one app
write and candidate boot/UART port-open (known FT232R reset-on-open). No extra
image, automatic next boot/run or rollback. Known source/boot identities must
match; intentional initial boot/reset is marked before monitoring the load.
Do not open UART or issue device requests until this concrete Gate is approved.

## Fixed first G1 run and later acceptance matrix

- Four fresh sessions; no re-login/logout, same board/AP/channel/bandwidth/host
  placement as documented preflight; source/release/ready/actors-disabled and
  HOME_WIFI/HomeConnected confirmed from UART. No network probe to discover IP;
  fixed expected host192.168.1.68 must agree with actual preflight.
- Capture `diag/g1_capture.py --run --gate-id <actual approved id>
  --allow-port-open-boot --expected-source <pinned diagnostic SHA>
  --host 192.168.1.68 --output <new private run directory> --load` only after
  Gate/ROM/artifact verification. This opens UART and is a gated hardware action.
  Use the existing esptool/pyserial Python environment. No script invocation now.
- Host `START_LOAD` file releases the single load only after source/boot checks,
  board uptime≥180000 ms and any controlled reference coverage. Its absence
  causes no requests. UART safety remains active throughout that waiting phase.
- Runner `diag/c0_load.py` retains the reviewed cfcf282 P-A schedule: four sessions,
  60 s four-session idle, 3 parallel API GETs/session every5s, offsets1.2s;
  shell/session every30s, first on second tick; nominal1200s,240 API ticks+
  40 shell ticks/session, then300s recovery. New connection per request,
  Connection:close, no retry. Latest response may end before nominal1200s as in
  G0; document actual scheduling/response boundaries, never invent extra ticks.
- Only login POST8s client socket budget; GET/other3s; no product timeout change.
  These are per-socket-operation limits, **not total request deadlines**.
  Fresh heartbeat from current EOF before first request/login; max5s monotonic
  host gap throughout setup/idle/load/recovery/manual coverage. Other lines do
  not extend the deadline. Errors contain fixed classes, never bodies, password,
  cookie, CSRF or exception messages. Secret remains only in memory/read from the protected local credential file.
- After successful recovery the capture stays supervised for manual coverage;
  host STOP ends it. No second load, reconnect, reboot or new phase automatically.
- This first G1 Gate is one diagnostic P-A/P-R plus reference/coverage measurements,
  **not full acceptance**. The overall matrix retains P-A3×20min fresh boots,
  P-A2×20min synchronized shells, P-B×20min four real Chromium contexts/
  Keep-Alive/reloads, and P-R after each P-A. Remaining diagnostic profile/resource
  evidence stays OPEN_G1; final pure-product confirmation stays OPEN_G2. This
  first run does not silently transfer missing diagnostic measurements to G2.
  Pin exact runners/browser versions and G1/G2 allocation before later separate
  Gates; those runs are not authorized here.
  Missing G1 coverage or needed extra diagnostic profiles require a reviewed
  follow-up Gate, not a silent reduced matrix or automatic next run.

## Latency, memory, stack, recovery fixed before run

Report API/shell p50/p95/p99/max separately per minute/profile; record per-request
wall time and socket error classes. No retrospective p99 cutoff or API-only-D1
comparison. No HTTP error/timeout/transport failure is accepted. G0-R3 five
GET totals>3s (max5431.081ms) are findings, not proof of socket timeout. Preserve
this distinction and all new long requests; no raised budget/retry to hide them.

G0-R3 boot min_free5228 B remains visible as a finding; the14KB expectation is
not a new hard threshold. Current free internal8-bit heap and largest internal8-bit
block determine reserves/recovery. Same four valid sessions before/after,
60-s idle reference and calming server/client sockets; close client connections,
observe server cleanup/host socket states including TIME_WAIT, same network mode.
Capture records host ss snapshots before/throughout the run at30-s intervals;
correlate these with UART and client phase marks (no extra device queries).
Within120s after last request/closed client connections: both current metrics
≥90% of their own comparable four-session idle. If comparability is absent,
recovery NOT_PROVEN/HOLD. Never use boot min_free as current recovery heap.

Main stack≥11160 B (11416−256), HTTPD≥2080 B (2336−256) by approved plan;
otherwise stop/HOLD and retain instrumentation stack overhead as a finding.
Report BSS/stack/flash overhead from this overlay, not as pure-product reserve.

## UI measurement and real coverage

`g1_ui` records phase0=whole UI step (includes snapshot/presentation and caller
argument preparation),1=contact-touch including clock/model/dispatch,2=renderer
including model, widgets and model destruction. `page` is canonical
FermentationUiPage enum ordinal, locale1=de/2=en/3=es/0=other. `home`, `hold`,
`kind` are canonical snapshot enums; fresh0 on skips is explicitly cached,
not an allocating fresh snapshot. Counts/lengths of visible text only, no
content/credentials. Touch and render may span different pages in one step;
use their own per-phase page, not step's entry page, as coverage identity.

For each complete measured window log internal8-bit pre/post free+largest;
checkpoints after snapshot/model/reserve/draw commands and LVGL commands track
sampled low free/block. Samples propagate to enclosing step. Integer-only
fixed-capacity aggregates retain max observed demand and minima/count/text
length ranges per context; bounded UART flush at most once/s **after all
measured scopes/models are destroyed**. Aggregate overflow is explicit
`g1_probe_error`, fail-closed. Per-page skip totals, gate count/pause/resume, the exact largest-block value
returned to the actual product gate (gate_block, gate_at_us),
last actually observed calibrated contact/release/hold allowance and aging
sample evidence are logged without resampling or inventing physical input.

No instrumentation dynamic buffers/allocation hooks/local-min monitor. Pinned
IDF heap_caps_monitor_local_minimum_free_size_start allocates and stop frees a
bookkeeping array, so it is deliberately not used. Queries/fixed records do not
allocate. Reporting/locks/UART cost time; parallel HTTP logging/network tasks
still contribute to the observed system heap. Fields pre/low/post are samples,
**not exact transient peaks**. Boot/per-region minima may precede the window
and occur at different times; never use them as the step's precise minimum.
LVGL's preallocated internal pool is separately sampled by existing resources_lvgl;
capability-heap checks do not measure every transient inside that pool. Aborted
in-flight windows may never flush. Any such gaps remain explicit.

Coverage ledger, tied to raw timestamps/manual case marks (never secret values):

| Cases | Required evidence / initial status |
|---|---|
| Actual boot/home redraw, HeaderNetwork/QR available+unavailable, clock, web access, settings, languages | Real phase2 plus source/page/locale/state/command/text length evidence; initially NOT_RUN |
| Program list/summary/edit, pagers/dialogs, value keypad, text keyboard modes/long unsaved text, messages/detail, status/technical/diagnostics/service/recovery/completion/run states | Actual reachable paths with real state; unavailable owning states stay NOT_PROVEN, no fake app/snapshot bridge |
| de/en/es and bounded text variants | Must be genuinely measured; no NVS/config changes implicit in this Gate. Existing locale/read-only unsaved candidates only; missing other variants NOT_PROVEN, later specific consent if needed |
| Raw/calibrated contact and release, hold variants A/B, skip→resume→still held>5s | Real sampled evidence plus physical Owner marks and unchanged app/network/HTTP/reset history; NOT_PROVEN until correlated |
| Skipped contexts | Not demand0. Match actually measured touch/render cases at enough reserve, including state/language/text differences; cached skip context alone cannot prove complete variant coverage |

Owner is available for physical operation. Before each actual touch/hold sequence,
give concrete steps and wait for the Owner confirmation; record both marks.
Actual reset or other destructive operation requires separate explicit consent.

No simulated touch/calibration may become hardware PASS. If persisted calibration
is missing/untrusted, leave it fail-closed and mark contact/hold NOT_PROVEN;
no calibration/NVS writes to repair this test. Read-only navigation/unsaved
edit candidates only; no save/provision/mutation API/program start or actual
factory reset. For a hold negative test, contact must be released/cancelled
before5s unless a real skip during that contact and active inhibition are
already evidenced. Never deliberately perform a new uninterrupted5-s contact:
that would execute reset/NVS changes and is outside this Gate. Positive reset
execution remains native evidence, not a claimed hardware result.

Use actual root/phase measurements, not canned native snapshots. If not enough
real paths/text/state/touch or transient bounds are established, G1 outcome is
**HOLD / THRESHOLD_NOT_QUALIFIED**, even after a fault-free HTTP load.

## First-anomaly stop and review

First heap_alloc_failed, panic/abort/watchdog/brownout/unexpected reset,
UART gap>5s, HTTP status/transport/timeout failure, probe overflow, insufficient
stack or identity mismatch: stop, no new requests; close active connections,
retain UART/raw/client/marks/timing/artifact evidence, **FAIL/HOLD**. Pending
socket connect remains bounded8/3s; it cannot submit a request after stop.
Capture forwards supervisor failures through a local abort file to the same
runner guard; no credential-bearing exception output. No logout, next run,
reset, flash, repair or rollback after an anomaly without new Owner consent.

Offline `g1_analyze.py` reports checkpoint maximum and arithmetic candidate
2×max(6656, observed demand) with explicit NOT_QUALIFIED, skipped/invalid/missing
coverage and touch NOT_PROVEN. Resume margin starts25% of the provisional
threshold; document any final justified margin. No automatic threshold edit,
G1-GO, G2 or Pre-Ready. Independent G1 result review and separate Owner steps.
`RES-01=BLOCKER`, `PR203_FINAL_GO=HOLD`, `G2=NOT_AUTHORIZED`, `ACTUATOR_RELEASE=NO`.
