# Issue #27 cJSON candidate probe

This is an isolated, non-product spike for the exact Espressif Registry
component `espressif/cjson=1.7.19~2`. It does not change the product dependency,
codec, routes, or composition. The registry package is pinned by its manifest
and generated lockfile. Its source is `idf-extra-components/cjson` at commit
`1387cec28a9b40654be7892114bd7d26fcd3869c`; the component SBOM identifies its
upstream `DaveGamble/cJSON` 1.7.19 submodule at commit
`b2890c8d76bbb64e710585ebc0a917196b9c67e7`.

## ESP-IDF 6.1 / ESP32 compile

From the repository root, source the project's ESP-IDF 6.1 environment and run:

```sh
idf.py -C spikes/issue27_json_cjson_candidate \
  -B build/issue27_json_cjson_candidate_esp32 build
```

This resolves the exact registry version, compiles the cJSON C component, and
compiles/links the probe as C++17 for target `esp32`. `sdkconfig.defaults`
configures cJSON's public Kconfig nesting cap to the product DTO limit of 4.
This is a build-only target check; it does not flash or run on hardware.

## Native host run

After the IDF dependency resolve has populated the spike's
`managed_components/espressif__cjson` directory, compile the exact same probe
source with the host C compiler for cJSON and C++17 for the consumer:

```sh
component=spikes/issue27_json_cjson_candidate/managed_components/espressif__cjson
gcc -std=c99 -Wall -Wextra -Werror -DCJSON_NESTING_LIMIT=4 \
  -I "$component/cJSON" -c "$component/cJSON/cJSON.c" \
  -o /tmp/issue27-cjson.o
g++ -std=c++17 -Wall -Wextra -Werror -DCJSON_NESTING_LIMIT=4 \
  -I "$component/cJSON" \
  spikes/issue27_json_cjson_candidate/main/cjson_candidate_probe.cpp \
  /tmp/issue27-cjson.o -lm -o /tmp/issue27-cjson-probe
/tmp/issue27-cjson-probe
```

The host executable reports contract-capability evidence. Some assertions are
deliberately expected to fail: the executable is an evaluation probe, not a
claim that cJSON passes the complete #27 contract. It covers the actual
maximum mutation DTO/body ceiling, root/revision/intent/nested-candidate
duplicates, closed field-shape checks, NUL/UTF-8 behavior, malformed input,
nesting, numeric edge cases, and preallocated serialization of the three
bounded read-only response shapes.

## Recorded result

```text
REGISTRY_VERSION=1.7.19~2
COMPONENT_COMMIT=1387cec28a9b40654be7892114bd7d26fcd3869c
UPSTREAM_CJSON_VERSION=1.7.19
UPSTREAM_CJSON_COMMIT=b2890c8d76bbb64e710585ebc0a917196b9c67e7
COMPONENT_HASH=e788323270d90738662d66fffa910bfe1fba019bba087f01557e70c40485b469
LICENSE=MIT
LICENSE_SHA256=a36dda207c36db5818729c54e7ad4e8b0c6fba847491ba64f372c1a2037b6d5c
ESP_IDF_6_1_ESP32_CXX17_BUILD=PASS
PUBLIC_TREE_DUPLICATE_CHECK=PASS_ROOT_REVISION_INTENT_CANDIDATE
MAX_MUTATION_DTO_BYTES=337_OF_480
BODY_AT_480_BYTES_ACCEPTED=PASS
OVERSIZED_BODY_REJECTED_BEFORE_PARSE=PASS
STRICT_SCHEMA_ROOT_SHAPE=PASS_UNKNOWN_MISSING_WRONG_ROOT_TYPE_REJECTABLE
NESTING_4=PASS_NESTING_5=REJECT
MALFORMED_TRUNCATED=REJECT
UINT32_OVERFLOW=RANGE_REJECTABLE
NONFINITE_1E999=PARSES_INFINITY_REQUIRES_EXPLICIT_ISFINITE_REJECTION
MAX_READ_ONLY_RESPONSE_BYTES=346_STATUS_247_TEMPERATURES_3048_ALERTS
MAX_READ_ONLY_RESPONSE_LIMIT=3072
UINT64_MAX=ROUNDS_TO_18446744073709551616_SAME_AS_UINT64_MAX_PLUS_1
ESCAPED_AND_RAW_NUL=ACCEPTED_AND_C_STRING_TRUNCATED
INVALID_UTF8=ACCEPTED
SPIKE_RESULT=FAIL_CANDIDATE
FINAL_SELECTION_PENDING=YES
```

The native probe exits nonzero for the demonstrated contract failures; this is
expected evidence, not a successful codec test. Although the public cJSON
`child`/`next`/`string` tree retains repeated object members and permits a
bounded duplicate walk, cJSON `double` values cannot represent the valid
`uint64` maximum revision distinctly from overflow. Escaped/raw NUL and invalid
UTF-8 are also accepted by this version. Rejecting NUL before the tree loses
the distinction would require a second JSON-aware source scanner, which is
outside this spike's allowed approach. Therefore cJSON is not a candidate for
the unchanged #27 JSON contract; no replacement or product selection is made.
