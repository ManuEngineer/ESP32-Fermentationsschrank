# Review reproduction on the pinned firmware source

No commands in this publication open UART, issue requests to the Devboard or flash.
No rebuild/test rerun was performed for publication; prior results are metadata
in the manifest, not new tests on the documentation HEAD.

Use a separate clone/worktree at `bb4f43a69740f234d26a951707991f792cc3ffde` and the project's canonical pinned
ESP-IDF6.1/toolchain environment (full versions in the manifest). The original
source test sheet is at `diag/G1_TEST_SHEET.md` on that source commit.

Prior executed preparation checks:

```sh
python3 -m unittest discover -s diag -p 'test_*.py' -v
c++ -std=gnu++17 -Wall -Wextra -Werror -Idiag/host_stubs -Imain diag/test_g1_probe.cpp -o /tmp/pr203_g1_probe_test
/tmp/pr203_g1_probe_test
python3 scripts/check_architecture_boundaries.py
python3 scripts/check_secrets.py
python3 scripts/build_esp_idf_profiles.py release --require-clean-source-tree
# clang21 tools on PATH; PRE_READY_EXPECTED_HEAD=bb4f43a69740f234d26a951707991f792cc3ffde
bash scripts/run_pre_ready_gates.sh self-check
```

`self-check` is Builder static analysis, not full Pre-Ready. No Pre-Ready authorized.
The published branch adds only review documentation after the firmware commit;
its HEAD is NOT a replacement build/source pin for the already hashed image.
Source/runner pins are independently checkable against GitHub blobs at `bb4f43a69740f234d26a951707991f792cc3ffde`.
Private raw tool logs, paths, images, ELF and device readbacks stay local.
The original local packet remains sealed and unchanged; publication hashes
are separately recorded in `SHA256SUMS`, not substituted for its original seal.
