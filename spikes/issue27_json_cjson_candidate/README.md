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
g++ -std=c++17 -Wall -Wextra -Werror \
  -I lib/fermentation_app/src \
  -c lib/fermentation_app/src/configuration_text.cpp \
  -o /tmp/issue27-configuration-text.o
g++ -std=c++17 -Wall -Wextra -Werror -DCJSON_NESTING_LIMIT=4 \
  -I "$component/cJSON" -I lib/fermentation_app/src \
  spikes/issue27_json_cjson_candidate/main/cjson_candidate_probe.cpp \
  /tmp/issue27-cjson.o /tmp/issue27-configuration-text.o -lm \
  -o /tmp/issue27-cjson-probe
/tmp/issue27-cjson-probe
```

The host executable reports current R1-MUST capability evidence and retained
historical hardening observations. A zero exit means the bounded R1 properties
in this probe pass; it is not a claim of final product-code selection. It covers the actual
maximum mutation DTO/body ceiling, root/revision/intent/nested-candidate
duplicates, closed field-shape checks, the bounded pre-parse raw/escaped-NUL
gate, canonical Program-ID validation through the existing project validator,
malformed input, nesting, numeric edge cases, and preallocated serialization
of the three bounded read-only response shapes.

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
ORIGINAL_NUMERIC_REVISION_FIXTURE_BYTES=337
CURRENT_DECIMAL_STRING_REVISION_FIXTURE_BYTES=341_OF_480
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
HISTORICAL_ESCAPED_AND_RAW_NUL=ACCEPTED_AND_C_STRING_TRUNCATED
RAW_NUL_BOUNDED_GATE=PASS
DECODED_NUL_ESCAPE_BOUNDED_GATE=PASS
VALID_ASCII_INTENT_AND_ENUMS=PASS
VALID_CANONICAL_PROGRAM_ID=PASS
HISTORICAL_INVALID_UTF8=ACCEPTED_NOT_R1_MUST
HISTORICAL_SPIKE_RESULT=FAIL_CANDIDATE_UNDER_STRICT_DUPLICATE_NUL_UTF8_CONTRACT
CURRENT_R1_CJSON_REASSESSMENT=PASS_CANDIDATE_FOR_R1_MUST
PLAN_REVISION_SHA=aa695b43f5b68edefea23669678d877f5b830c17
FINAL_SELECTION_PENDING=YES
```

The original native probe exited nonzero for the demonstrated contract
failures under its historical stricter contract; this current candidate-
completion probe records those duplicate/UTF-8 outcomes as non-MUST
observations and exits zero only when the current bounded R1 properties pass.
The current R1 comparison changes only which properties are MUST;
the old duplicate/UTF-8 observations and probe outcomes are not rewritten.
The new probe gate first enforces the 480-byte body bound, then rejects raw NUL
bytes and the bounded six-byte sequence backslash + `u0000` before cJSON. It neither
tokenizes JSON nor tracks strings, escapes, or structure. The current closed
ASCII allowlists and canonical Program-ID validator remain responsible for
post-parse values; they accept the actual valid intent/enum samples and a
canonical maximum-length Program-ID. cJSON remains a candidate only under this
proportional R1 contract; no product selection or codec change is made.
