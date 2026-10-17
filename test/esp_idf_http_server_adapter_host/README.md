# ESP-IDF HTTP adapter host regression

This ESP-IDF Linux-target test links the pinned ESP-IDF 6.1 `esp_http_server`
and the production `EspIdfHttpServerLifecycle` adapter. It sends real loopback
HTTP requests through the public server API and verifies dispatch, metadata,
duplicates, empty values, bounds, and invalid values. A test-only linker
wrapper changes the server port from 80 to 8080; it forwards to the real
`httpd_start()` implementation. No device flash is involved.

With the repository's ESP-IDF 6.1 environment active:

```bash
idf.py -C test/esp_idf_http_server_adapter_host \
  -B /tmp/build_issue27_http_server_adapter_host build
/tmp/build_issue27_http_server_adapter_host/issue27_http_server_adapter_host.elf
```

RES-01 extends this test with complete chunk framing (including an empty head),
503 JSON bytes/status/type without a suffix, and test-only public API wrappers
that inject head/suffix/terminator errors and observe the adapter return value.
The wrappers forward normal calls to the real pinned IDF implementation.
