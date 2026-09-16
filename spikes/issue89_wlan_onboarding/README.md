# Issue #89: isolated WLAN onboarding spikes

This directory contains the Phase A capability-evidence harness and the
authorized Phase-B hardware/transport probes. Comparable client evidence is
still pending. The artefacts are deliberately outside the production CMake
graph and have no
production dependency, storage namespace, credential record, or runtime
lifecycle contract.

The three ESP-IDF projects are build-only/actor-free probes:

- `official_network_provisioning/` locks Espressif
  `network_provisioning` 1.2.4 and starts its standard SoftAP/protocomm
  manager in the isolated firmware. Its output redacts the generated AP
  password. In the recorded build-only result no client sent Set/Apply
  credentials, so no credential commit was executed or observed. This is not
  a read-only or volatile credential path: the unchanged manager uses native
  ESP-WiFi/NVS persistence when a client actually applies credentials.
- `native_http_adapter/` uses only ESP-IDF `esp_wifi`, `esp_netif`,
  `esp_event`, `esp_http_server`, and the required `nvs_flash` initialization
  to expose a minimal direct-IP page. It
  initializes NVS fail-closed without automatic erase, selects
  `WIFI_STORAGE_RAM` before applying the SoftAP configuration, and keeps the
  generated credentials volatile and redacted. For a controlled client run,
  an optional untracked `main/issue89_test_credentials.local` file can provide
  one ephemeral WPA2 password; it is never logged or committed. This is an
  isolated capability probe, not permission to build the production adapter or
  DNS/portal contract.
- `direct_protocomm/` uses the public ESP-IDF `protocomm` and HTTPD transport
  APIs without `network_provisioning`. Its Set/Test/Commit endpoint names are
  static handler-boundary probes only: request data is not interpreted,
  applied, persisted, or selected.

`host_contract_test.py` exercises the candidates-neutral, volatile test oracle
for field validation, commit-before/after behavior, redaction, and the
synthetic Wi-Fi QR payload. It never writes credentials or a project storage
record.

## Controlled client credential

The default probes generate a fresh protected SoftAP password and only print
`key=<redacted>`. For one authorized Issue-89 run, the operator may create the
same local-only file in each candidate's `main/` directory:

```text
export ISSUE89_TEST_AP_PASSWORD
read -r -s -p 'Enter a new 8-16 character test value: ' ISSUE89_TEST_AP_PASSWORD
printf '\n'
for project in \
  official_network_provisioning direct_protocomm native_http_adapter; do
  printf '#define ISSUE89_TEST_AP_PASSWORD "%s"\n' \
    "$ISSUE89_TEST_AP_PASSWORD" \
    > "spikes/issue89_wlan_onboarding/$project/main/issue89_test_credentials.local"
done
```

Use a new value for each controlled run and pass it to the private test
operator or client out of band. The value must be 8 to 16 characters so the
probes remain WPA2-protected. Do not echo it into UART captures, shell
transcripts, Evidence, PR text, or other durable logs. Remove all three local
files after the run:

```text
rm -f spikes/issue89_wlan_onboarding/official_network_provisioning/main/issue89_test_credentials.local \
  spikes/issue89_wlan_onboarding/direct_protocomm/main/issue89_test_credentials.local \
  spikes/issue89_wlan_onboarding/native_http_adapter/main/issue89_test_credentials.local
unset ISSUE89_TEST_AP_PASSWORD
```

The local file only selects the password for the isolated firmware build; it
does not add a credential domain, persistence path, recovery behavior, or
production dependency. A missing file restores the random-password default.

## Manual client sequence

After flashing one candidate to the authorized disposable board, use the SSID
printed by the probe and the private `ISSUE89_TEST_AP_PASSWORD` value. Keep the
UART capture redacted. For each available platform, execute this sequence and
record each result separately:

1. Join the WPA2 SoftAP and verify the board's direct address, normally
   `192.168.4.1`.
2. For `network_provisioning` 1.2.4, use the POST endpoints
   `/prov-session`, `/prov-config`, `/proto-ver`, `/prov-scan` and
   `/prov-ctrl`; for direct Protocomm, use POST `/r1-session`,
   `/r1-version`, `/r1-set`, `/r1-test` and `/r1-commit`. For native HTTP,
   use GET `/` and record the page response. The official endpoints follow
   the pinned component's `esp_prov` protocol; the direct handlers are
   boundary-only and do not apply credentials.
3. Check whether a captive offer or DNS interception exists, then run scan,
   form, false-password, abort/timeout, test-before-commit and commit-boundary
   cases without entering production credentials.
4. Record disconnect/reconnect, browser reload, firmware restart, and DTR/RTS
   reset behavior. Reset is not a power-cut test; power-cut is waived by the
   Owner.
5. Record redaction, recovery, free/minimum/free-block heap, relevant stack
   watermarks, handles/leaks, watchdog/errors and a jitter measurement only
   when a real measurement point exists.

Repeat the complete sequence for `network_provisioning` 1.2.4, direct
Protocomm and native HTTP before any candidate-selection decision. WiFiManager
remains `BLOCKED_FOR_NATIVE_IDF_6_1_SPIKE` and is not part of this client run.

## Reproduce

From the repository root, set both variables to explicitly verified local
checkouts, source the pinned ESP-IDF 6.1 environment, and run:

```text
export IDF_PATH=/path/to/verified/esp-idf-v6.1
export IDF_TOOLS_PATH=/path/to/verified/espressif-tools
test "$(git -C "$IDF_PATH" rev-parse HEAD)" = \
  "fff9895c82d744c7237be8847347bdd1b07c6643"
source "$IDF_PATH/export.sh"
python3 spikes/issue89_wlan_onboarding/host_contract_test.py
idf.py -C spikes/issue89_wlan_onboarding/official_network_provisioning \
  -B build/issue89_official_network_provisioning_idf61 build
idf.py -C spikes/issue89_wlan_onboarding/native_http_adapter \
  -B build/issue89_native_http_adapter_idf61 build
idf.py -C spikes/issue89_wlan_onboarding/direct_protocomm \
  -B build/issue89_direct_protocomm_idf61 build
```

The generated build trees and managed components are local ignored artefacts.
They are not evidence until the corresponding command result and source
revision are recorded in
`docs/audits/ISSUE_89_WLAN_ONBOARDING_EVIDENCE.md`.

No flash or client command is part of the default build reproduction. For the
approved Issue-#89 Phase-B run, only the explicitly authorized
ESP32-WROOM-32E-Development-Testtraeger may be used. Its complete flash and
Default-NVS may be erased and overwritten before each controlled candidate
run; no additional test NVS, separate physical test partition, or pre-test
backup is required. Record the exact target, partition layout, UART/reset
path, client matrix, and secret-safe evidence. Never add automatic NVS erase
to product code or run this flow against a non-authorized project/user device.
Power-cut tests are waived by the Owner and are not part of the Phase-B
acceptance criteria; DTR/RTS reset remains a separate reset test.
