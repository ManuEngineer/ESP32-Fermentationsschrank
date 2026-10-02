# ESP-IDF replay digest adapter test

This ESP-IDF Linux-target test links the production PSA-backed
`EspIdfSha256ReplayDigest` adapter and verifies the full SHA-256 digest for the
canonical request framing. It uses no device flash and no product mutation.

With the repository's ESP-IDF 6.1 environment active:

```bash
idf.py -C test/esp_idf_replay_digest_host \
  -B /tmp/build_issue27_replay_digest_host build
/tmp/build_issue27_replay_digest_host/issue27_replay_digest_host.elf
```
