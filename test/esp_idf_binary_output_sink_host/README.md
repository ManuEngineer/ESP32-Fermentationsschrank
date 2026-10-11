# ESP-IDF binary output adapter host test (Issue #32)

This ESP-IDF Linux-target test links the production `EspIdfBinaryOutputSink`
adapter against the CMock GPIO driver mock shipped with ESP-IDF
(`$IDF_PATH/tools/mocks/driver`). It checks the exact `gpio_set_level` /
`gpio_config` call sequence: `Unconfirmed` performs no GPIO call at all, the
inactive level is preset before the pad becomes an output (no pulls), and every
failure is fail-closed. No device flash and no hardware are involved.

With the repository's ESP-IDF 6.1 environment active:

```bash
idf.py -C test/esp_idf_binary_output_sink_host \
  -B /tmp/build_issue32_binary_output_sink_host build
/tmp/build_issue32_binary_output_sink_host/issue32_binary_output_sink_host.elf
```

This test is not part of `scripts/run_pre_ready_gates.sh` (like the other
`test/esp_idf_*_host` projects).
