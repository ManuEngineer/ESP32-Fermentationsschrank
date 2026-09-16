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
  generated credentials volatile and redacted. This is an isolated capability
  probe, not permission to build the production adapter or DNS/portal contract.
- `direct_protocomm/` uses the public ESP-IDF `protocomm` and HTTPD transport
  APIs without `network_provisioning`. Its Set/Test/Commit endpoint names are
  static handler-boundary probes only: request data is not interpreted,
  applied, persisted, or selected.

`host_contract_test.py` exercises the candidates-neutral, volatile test oracle
for field validation, commit-before/after behavior, redaction, and the
synthetic Wi-Fi QR payload. It never writes credentials or a project storage
record.

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
