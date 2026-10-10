# ESP-IDF shared-enable bridge adapter host test (Issue #33)

This ESP-IDF Linux-target test links the production `EspIdfSharedEnableBridge`
adapter (three `EspIdfBinaryOutputSink` plus the portable
`SharedEnableBridgeSink`) against the CMock GPIO driver mock shipped with
ESP-IDF (`$IDF_PATH/tools/mocks/driver`). It checks the exact `gpio_set_level` /
`gpio_config` call order of the boot initialisation (enable GPIO25, RPWM GPIO13,
LPWM GPIO14, each inactive first, no pulls), the failure behaviour at every
initialisation stage (bridge stays unstarted, one best-effort OFF per output,
no later command reaches a GPIO), the enable/leg sequences, mutual exclusion and
fail-closed latching at the pin level. No device flash and no hardware are
involved.

With the repository's ESP-IDF 6.1 environment active (Ruby is required by CMock):

```bash
idf.py -C test/esp_idf_shared_enable_bridge_host \
  -B /tmp/build_issue33_shared_enable_bridge_host build
/tmp/build_issue33_shared_enable_bridge_host/issue33_shared_enable_bridge_host.elf
```

This test is not part of `scripts/run_pre_ready_gates.sh` (like the other
`test/esp_idf_*_host` projects).
