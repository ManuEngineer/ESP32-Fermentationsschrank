# PR203 G1-prime: a9 diagnostic overlay (non-merge)

Product base a9a1156a1d44d4679957526bbc34a30be6422e4e. Firmware overlay
reused byte-for-byte from bb4f43a69740f234d26a951707991f792cc3ffde on d35ee85;
the five affected product files have not changed between d35ee85 and a9a1156.
Host capture and offline tests reused from independently verified monitor
893f3bb73a50b484ce79b3b80722873452d998fc. No NETPROBE1, forced heap pressure,
product gate bypass, session load or network configuration changes.

This branch is diagnostic only: never merge into PR203 or main.
The separate concrete gate pins source, app/ELF hashes and allowed actions.
Preparation authorizes no UART opening, flash, reset, browser or hardware run.

Future proposed profile: one normal browser, ordinary 5-s polling and 15-20 min
physical screen/touch coverage; do not pass --load or create START_LOAD.
The imported c0_load module is an existing capture dependency, not authorization
to use its load driver. Main stack policy remains strict (11160 B); the Owner
g2-trend exception applies only to the exact G2-Web gate, not G1-prime.

Zero-allocation probe records shared internal 8-bit heap/block checkpoints.
Observed changes include concurrent WLAN/HTTP tasks and miss transients between
checkpoints. They do not isolate causal UI demand b, certify max(a,b)*2 or
justify a 33536-B threshold. Analyzer arithmetic is NOT_QUALIFIED, never a
threshold recommendation. Skipped paths are unmeasured. Hardware skip/resume/
hold needs actual matching events; existing native tests are software evidence.

F4 skip 13312 B / resume 16640 B stay PROVISIONAL_PENDING_G1.
RES-01=BLOCKER; PR203_FINAL_GO=HOLD; ACTUATOR_RELEASE=NO.
