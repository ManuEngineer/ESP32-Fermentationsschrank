# Issue #89: isolated WLAN onboarding spikes

This directory contains the Phase A/B evidence harness authorized by the
approved plan. It is deliberately outside the production CMake graph and has
no production dependency, storage namespace, credential record, or runtime
lifecycle contract.

The two ESP-IDF projects are build-only/actor-free probes:

- `official_network_provisioning/` locks Espressif
  `network_provisioning` 1.2.4 and starts its standard SoftAP/protocomm
  manager in the isolated firmware. Its output redacts the generated AP
  password. The probe is not a product portal and does not commit credentials.
- `native_http_adapter/` uses only ESP-IDF `esp_wifi`, `esp_netif`,
  `esp_event`, and `esp_http_server` to expose a minimal direct-IP page. It
  also keeps generated SoftAP credentials volatile and redacts them. This is
  an isolated capability probe, not permission to build the production
  adapter or DNS/portal contract.

`host_contract_test.py` exercises the candidates-neutral, volatile test oracle
for field validation, commit-before/after behavior, redaction, and the
synthetic Wi-Fi QR payload. It never writes credentials or a project storage
record.

## Reproduce

From the repository root, source the pinned ESP-IDF 6.0.2 environment and run:

```text
python3 spikes/issue89_wlan_onboarding/host_contract_test.py
idf.py -C spikes/issue89_wlan_onboarding/official_network_provisioning \
  -B build/issue89_official_network_provisioning build
idf.py -C spikes/issue89_wlan_onboarding/native_http_adapter \
  -B build/issue89_native_http_adapter build
```

The generated build trees and managed components are local ignored artefacts.
They are not evidence until the corresponding command result and source
revision are recorded in
`docs/audits/ISSUE_89_WLAN_ONBOARDING_EVIDENCE.md`.

No flash command is part of the default reproduction. Any later actor-free
hardware run needs a separately recorded target, flash layout, UART/reset
path, client matrix, and secret-safe evidence.
