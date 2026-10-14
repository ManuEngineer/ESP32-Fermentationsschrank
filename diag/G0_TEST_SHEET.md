# PR #203 / RES-01 – next G0 test sheet (2026-10-14)

**Software correction only; hardware repeat NOT_AUTHORIZED.** Approved plan:
`6222efce976dc3bea3cf397c2dca56a5fb21859d`. This sheet replaces the prepared
sheet for a future run; it does not rewrite the sealed 2026-10-13 G0 evidence
or authorize an exception to the approved profile.
`G0=STOPPED`, `RAM_GAIN_COMPARISON=NOT_RUN`, `RES-01=BLOCKER`,
`PR203_FINAL_GO=HOLD`, `G1_G2=NOT_AUTHORIZED`, `ACTUATOR_RELEASE=NO`.

## Fixed login and load budgets

| Operation | Client socket timeout | Failure outcome |
|---|---|---|
| `POST /api/v1/login` only | **8 s** | immediate FAIL/HOLD; no retry |
| API GETs, shell GET and all other requests | **3 s**, unchanged | immediate FAIL/HOLD; no retry |
| Fresh first UART heartbeat and subsequent heartbeat gaps | **5 s**, monotonic host time | `uart-heartbeat-timeout`, FAIL/HOLD |

The 8-s login value is fixed before the next run, justified by regular
3.03–3.08-s logins in [PR-170 hardware evidence](../docs/audits/PR170_HW_TOUCH_PATH_FINDING.md)
and the roughly 3.2-s PR-203 observations cited by the Owner. It changes only
`HTTPConnection`'s client socket timeout, not KDF, server, session or firmware
configuration, and is not a new total-request deadline. Preserved v3
`diag/run/load_D1..D3.json` additionally records eleven successful logins at
3445.9–3575.1 ms and one at 8225.7 ms. The 8-s value does not guarantee every
historical outlier will succeed; any new timeout is FAIL/HOLD, without raising
the budget or retrying during a run. A fresh heartbeat is
still required from the capture's current EOF before reading the password or
sending any request. Old log lines are insufficient; other log output cannot
extend the heartbeat deadline. Monitoring remains active through login,
idle, load and recovery. The 8-s login timeout never relaxes the 5-s UART gate.

Failed request samples contain only a fixed error class: `timeout`,
`connection-error`, `http-protocol-error` or `http-status-error`; successful
HTTP 200 samples have null. Preserve status/timing and the first abort reason.
Do not serialize exception messages, credentials, headers, cookies, CSRF or
bodies. Do not retry or attempt logout after a failure.

## Unchanged authorized-profile preparation

- P-A: four fresh sessions, three concurrent API GETs every 5 s per session,
  1.2-s session offsets; shell GET every 30 s per session, first on its second
  tick. **1200 s load**, new connection per request, `Connection: close`.
  Same exact runner SHA/hash for baseline and candidate; no re-login/logout.
- Before load: 60 s idle with the same four sessions and closed runner sockets.
  Record current free heap and largest block from `diag_30s`, session validity,
  boot/session age, WLAN/client placement and matching server/network state.
- After load: close sockets, retain four sessions, 300 s recovery. The 90%/120-s
  recovery criterion uses comparable session/socket/network idle references.
  Client closure alone is insufficient; if server cleanup/comparability cannot
  be established, recovery is NOT_PROVEN/HOLD. Never use boot `min_free` or
  the old unloaded 42.3-KB value as an automatic four-session recovery reference.
- UART monitoring is independent of request ticks (50-ms poll). First allocation
  failure, unexpected reset/abort/panic/watchdog/brownout, non-200, transport
  failure or heartbeat timeout aborts: close active connections, no new requests,
  no next phase, reset, flash or rollback. A pending connect retains its bounded
  socket timeout (8 s for login, 3 s otherwise); no replacement request follows.
- API/Shell latency p50/p95/p99/max are separate per profile/minute. No GET timeout
  or connection failure is accepted; latency trends are findings without an
  invented D1 API-only p99 gate. Any additional bound must be fixed before runs.
- C0 requires a measurable shell peak reduction and comparable heap/block reserve
  under the same load, plus a fault-free candidate. TX-buffer counts remain
  NOT_INSTRUMENTED (hypothesis only). G0 supplies no G1/G2 or actuator release.
- Before any newly approved hardware action, reverify complete baseline/candidate
  image and ELF hashes, partition and rollback artifacts. Original firmware
  source remains v3 `3e94cea` and candidate `9a9a8bd`; no rebuild for this fix.

## Early baseline failure: evidence limit and Owner decision

The v3 root-cause report (`hw-evidence-pr203/diag/RES01_ROOT_CAUSE_RESULT.md`)
records failures within about 162/220 s in earlier runs; a complete 20-min
baseline may be impossible. At the first new anomaly stop and seal evidence.
The failed baseline stays FAIL/HOLD and its 20-min reference is not established.
There is **no automatic candidate flash/run after that stop**.

Existing firmware logs `res01_http phase=pre/built/sent`: current `free`,
`largest8`, `internal_largest`, body bytes and send result; `diag_30s` adds
periodic context. These allow an **observed shell-phase peak** comparison over
matched successful cycles before the anomaly: report `free(pre)-free(built)`,
`free(pre)-free(sent)`, and minima of current free heap/largest blocks, with
range/median/sample counts. They do not sample every allocation/ACK instant;
concurrent network activity also contributes to differences. Neither a 30-s
sample nor monotonic boot `min_free` proves the exact instantaneous peak or a
same-duration full-run benefit. Exclude failed/incomplete cycles from the
successful-cycle comparison, retain them as failure evidence.

**Proposal, NOT_AUTHORIZED:** predeclare a 120-s reference window starting at
P-A load base, requiring all scheduled GETs to succeed and four complete shell
responses per session (16 ordered UART pre/built/sent triplets, `sent result=0`
and client HTTP 200). Require both images to complete this identical window
with the same four-session idle, boot/session age, client/network state and
schedule. Match phase ordering and timestamps against client response records;
if triplets or state cannot be matched, comparison is NOT_PROVEN/HOLD. Do not
select a better-looking window after seeing results. A failure before completion
makes this proposed reference unusable; do not shrink it retrospectively.

Under that proposal the unchanged runner would still attempt 1200 s; an early
baseline failure remains a test failure. Its already completed first 120 s
could become the explicitly limited resource reference only after Owner approval.
A **separate Owner-authorized candidate run after baseline STOP** would need to
complete 1200 s plus recovery without faults; compare its first 120 s for the
limited RAM finding and report its remaining stability separately. Successful
prefix data would not turn the failed baseline into PASS or prove universal
stability/renderer OOM safety. Existing C0 criteria otherwise remain in force.

Accepting this shorter reference and separate candidate execution is a
**material deviation from the approved paired 20-min comparison**. The Owner
must decide it before a new G0 gate request/run. Until then the original
20-min comparison stands, `RAM_GAIN_COMPARISON=NOT_RUN`, C0 decision HOLD.
Independent Fix Verification of this software correction comes first; this
sheet neither requests nor implies the new hardware authorization.
