# Issue #89: isolated WLAN onboarding spikes

This directory contains the Phase A capability-evidence harness authorized by
the approved plan. Phase B comparable client evidence is still pending. The
artefacts are deliberately outside the production CMake graph and have no
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
  `esp_event`, and `esp_http_server` to expose a minimal direct-IP page. It
  also keeps generated SoftAP credentials volatile and redacts them. This is
  an isolated capability probe, not permission to build the production
  adapter or DNS/portal contract.
- `direct_protocomm/` uses the public ESP-IDF `protocomm` and HTTPD transport
  APIs without `network_provisioning`. Its Set/Test/Commit endpoint names are
  static handler-boundary probes only: request data is not interpreted,
  applied, persisted, or selected.

`host_contract_test.py` exercises the candidates-neutral, volatile test oracle
for field validation, commit-before/after behavior, redaction, and the
synthetic Wi-Fi QR payload. It never writes credentials or a project storage
record.

## Reproduce

From the repository root, source the pinned ESP-IDF 6.1 environment and run:

```text
export IDF_TOOLS_PATH=/var/lib/docker/data/engineering/home/manuel/.espressif
source /var/lib/docker/data/ESP32-Projekte/opt/espressif/esp-idf-v6.1/export.sh
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

No flash or client command is part of the default reproduction. The official
manager probe must never be run against a project/user NVS for a real
credential test: before any client Set/Apply test, prepare an explicitly
disposable or secured test NVS and record the isolated target, partition
layout, backup/reset boundary, UART/reset path, client matrix, and
secret-safe evidence. Do not add an automatic NVS erase to make the test
convenient; existing project/user data must remain untouched.
