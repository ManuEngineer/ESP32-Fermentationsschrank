#include <cJSON.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <initializer_list>
#include <string>

namespace {

constexpr std::size_t kMutationBodyLimit = 480U;
constexpr std::size_t kResponseBodyLimit = 3072U;
constexpr std::size_t kNestingLimit = 4U;

bool require(bool condition, const char* evidence) {
    std::printf("%s=%s\n", evidence, condition ? "PASS" : "FAIL");
    return condition;
}

cJSON* parseExact(const std::string& body) {
    const char* end = nullptr;
    auto* root = cJSON_ParseWithLengthOpts(body.data(), body.size(), &end, 0);
    if (root == nullptr || end == nullptr) {
        cJSON_Delete(root);
        return nullptr;
    }
    const char* const limit = body.data() + body.size();
    while (end < limit &&
           (*end == ' ' || *end == '\t' || *end == '\r' || *end == '\n')) {
        ++end;
    }
    if (end != limit) {
        cJSON_Delete(root);
        return nullptr;
    }
    return root;
}

cJSON* parseMutationBody(const std::string& body) {
    if (body.size() > kMutationBodyLimit) return nullptr;
    return parseExact(body);
}

bool hasDuplicateMembers(const cJSON* value, std::size_t depth = 1U) {
    if (value == nullptr || depth > kNestingLimit) return true;
    if (cJSON_IsObject(value)) {
        for (const auto* left = value->child; left != nullptr;
             left = left->next) {
            if (left->string == nullptr) return true;
            for (const auto* right = left->next; right != nullptr;
                 right = right->next) {
                if (right->string == nullptr ||
                    std::strcmp(left->string, right->string) == 0) {
                    return true;
                }
            }
        }
    }
    if (cJSON_IsObject(value) || cJSON_IsArray(value)) {
        for (const auto* child = value->child; child != nullptr;
             child = child->next) {
            if (cJSON_IsObject(child) || cJSON_IsArray(child)) {
                if (hasDuplicateMembers(child, depth + 1U)) return true;
            }
        }
    }
    return false;
}

bool hasOnlyKeys(const cJSON* object, const char* const* allowed,
                 std::size_t allowedCount) {
    if (!cJSON_IsObject(object)) return false;
    for (const auto* member = object->child; member != nullptr;
         member = member->next) {
        if (member->string == nullptr) return false;
        bool found = false;
        for (std::size_t index = 0U; index < allowedCount; ++index) {
            if (std::strcmp(member->string, allowed[index]) == 0) {
                found = true;
                break;
            }
        }
        if (!found) return false;
    }
    return true;
}

bool hasKeys(const cJSON* object, const char* const* required,
             std::size_t requiredCount) {
    if (!cJSON_IsObject(object)) return false;
    for (std::size_t index = 0U; index < requiredCount; ++index) {
        if (cJSON_GetObjectItemCaseSensitive(object, required[index]) ==
            nullptr) {
            return false;
        }
    }
    return true;
}

std::string maximumMutationBody() {
    return std::string(
               R"({"v":1,"r":{"s":4294967295,"r":4294967295,"m":4294967295,"f":4294967295,"e":4294967295,"u":18446744073709551615,"c":18446744073709551615},"i":{"t":"start-program","c":{"p":")") +
           std::string(48U, 'p') +
           R"(","x":42.75,"d":4294967295,"h":false,"s":"product","c":"cool-and-hold-until-manual-stop","k":-12.5,"l":4294967295}}})";
}

bool appendNumber(cJSON* object, const char* key, double value) {
    return cJSON_AddNumberToObject(object, key, value) != nullptr;
}

bool printWithinResponseLimit(cJSON* root, const char* label) {
    // cJSON documents a five-byte safety margin for PrintPreallocated.
    char output[kResponseBodyLimit + 5U]{};
    const bool printed = cJSON_PrintPreallocated(
        root, output, static_cast<int>(sizeof(output)), 0);
    const std::size_t length = printed ? std::strlen(output) : 0U;
    std::printf("%s_PREALLOC=%s;BYTES=%zu;LIMIT=%zu\n", label,
                printed ? "PASS" : "FAIL", length, kResponseBodyLimit);
    if (!printed) {
        char* const diagnostic = cJSON_PrintUnformatted(root);
        if (diagnostic != nullptr) {
            std::printf("%s_DIAGNOSTIC_BYTES=%zu\n", label,
                        std::strlen(diagnostic));
            cJSON_free(diagnostic);
        }
        return false;
    }
    return length <= kResponseBodyLimit;
}

bool maxResponsesFitBoundedBuffers() {
    bool ok = true;
    auto* status = cJSON_CreateObject();
    auto* revisions = cJSON_CreateObject();
    ok = status != nullptr && revisions != nullptr;
    if (ok) {
        ok = appendNumber(status, "version", 1) &&
             cJSON_AddBoolToObject(status, "ready", 1) != nullptr &&
             cJSON_AddStringToObject(status, "homeMode", "restricted") !=
                 nullptr &&
             cJSON_AddStringToObject(status, "processState", "waiting") !=
                 nullptr &&
             cJSON_AddStringToObject(status, "networkMode",
                                     "selection-required") != nullptr &&
             cJSON_AddBoolToObject(status, "selectionRequired", 1) != nullptr;
    }
    if (ok) {
        cJSON_AddItemToObject(status, "revisions", revisions);
        revisions = nullptr;
        auto* ownedRevisions =
            cJSON_GetObjectItemCaseSensitive(status, "revisions");
        ok = appendNumber(ownedRevisions, "stateSequence", UINT32_MAX) &&
             appendNumber(ownedRevisions, "run", UINT32_MAX) &&
             appendNumber(ownedRevisions, "messages", UINT32_MAX) &&
             appendNumber(ownedRevisions, "fault", UINT32_MAX) &&
             appendNumber(ownedRevisions, "recoveryEpisode", UINT32_MAX) &&
             appendNumber(ownedRevisions, "userConfiguration",
                          static_cast<double>(UINT64_MAX)) &&
             appendNumber(ownedRevisions, "programCatalog",
                          static_cast<double>(UINT64_MAX));
        ok = ok && printWithinResponseLimit(status, "STATUS");
    }
    cJSON_Delete(revisions);
    cJSON_Delete(status);

    auto* temperatures = cJSON_CreateObject();
    auto* temperatureArray = cJSON_CreateArray();
    ok = ok && temperatures != nullptr && temperatureArray != nullptr;
    if (ok) {
        cJSON_AddItemToObject(temperatures, "temperatures", temperatureArray);
        temperatureArray = nullptr;
        ok = appendNumber(temperatures, "version", 1);
        const char* const roles[] = {"cabinet-air", "product", "cooling"};
        const char* const qualities[] = {"valid", "stale", "failed"};
        const double values[] = {21.5, 18.25, 0.0};
        for (std::size_t index = 0U; index < 3U; ++index) {
            auto* item = cJSON_CreateObject();
            ok = item != nullptr &&
                 cJSON_AddStringToObject(item, "role", roles[index]) !=
                     nullptr &&
                 cJSON_AddStringToObject(item, "quality", qualities[index]) !=
                     nullptr &&
                 cJSON_AddBoolToObject(item, "valid", index == 0U) != nullptr;
            if (ok) {
                ok = index == 0U
                         ? appendNumber(item, "valueCelsius", values[index])
                         : cJSON_AddNullToObject(item, "valueCelsius") !=
                               nullptr;
            }
            if (!ok) {
                cJSON_Delete(item);
                break;
            }
            cJSON_AddItemToArray(temperatureArray == nullptr
                                     ? cJSON_GetObjectItemCaseSensitive(
                                           temperatures, "temperatures")
                                     : temperatureArray,
                                 item);
        }
        ok = ok && printWithinResponseLimit(temperatures, "TEMPERATURES");
    }
    cJSON_Delete(temperatureArray);
    cJSON_Delete(temperatures);

    auto* alerts = cJSON_CreateObject();
    auto* alertArray = cJSON_CreateArray();
    ok = ok && alerts != nullptr && alertArray != nullptr;
    if (ok) {
        cJSON_AddItemToObject(alerts, "alerts", alertArray);
        alertArray = nullptr;
        ok = appendNumber(alerts, "version", 1);
        auto* ownedArray = cJSON_GetObjectItemCaseSensitive(alerts, "alerts");
        for (std::uint32_t index = 0U; index < 16U; ++index) {
            auto* item = cJSON_CreateObject();
            ok =
                item != nullptr &&
                appendNumber(item, "id", UINT32_MAX - index) &&
                cJSON_AddStringToObject(
                    item, "code", "product-insertion-requested") != nullptr &&
                cJSON_AddStringToObject(item, "severity",
                                        "decision-required") != nullptr &&
                cJSON_AddBoolToObject(item, "active", 1) != nullptr &&
                cJSON_AddBoolToObject(item, "acknowledged", 0) != nullptr &&
                cJSON_AddBoolToObject(item, "resolved", 0) != nullptr &&
                cJSON_AddBoolToObject(item, "decisionRequired", 1) != nullptr &&
                cJSON_AddBoolToObject(item, "muted", 0) != nullptr &&
                appendNumber(item, "revision", 0);
            if (!ok) {
                cJSON_Delete(item);
                break;
            }
            cJSON_AddItemToArray(ownedArray, item);
        }
        ok = ok && printWithinResponseLimit(alerts, "ALERTS");
    }
    cJSON_Delete(alertArray);
    cJSON_Delete(alerts);
    return ok;
}

int runProbe() {
    bool ok = true;
    ok &= require(__cplusplus >= 201703L, "CXX17_HOST_OR_TARGET");

    const char* const duplicates[] = {
        R"({"v":1,"v":2,"r":{"s":0},"i":{"t":"reset-fault"}})",
        R"({"v":1,"r":{"s":0,"s":1},"i":{"t":"reset-fault"}})",
        R"({"v":1,"r":{"s":0},"i":{"t":"reset-fault","t":"ack-message"}})",
        R"({"v":1,"r":{"s":0},"i":{"t":"start-program","c":{"p":"old","p":"new"}}})"};
    const char* const duplicateNames[] = {
        "DUPLICATE_ROOT", "DUPLICATE_REVISIONS", "DUPLICATE_INTENT",
        "DUPLICATE_NESTED_CANDIDATE"};
    for (std::size_t index = 0U; index < 4U; ++index) {
        auto* parsed = parseExact(duplicates[index]);
        const bool detected = parsed != nullptr && hasDuplicateMembers(parsed);
        ok &= require(detected, duplicateNames[index]);
        cJSON_Delete(parsed);
    }

    const char* const rootAllowed[] = {"v", "r", "i"};
    const char* const rootRequired[] = {"v", "r", "i"};
    auto* valid = parseExact(R"({"v":1,"r":{"s":0},"i":{"t":"reset-fault"}})");
    const bool closedSchema =
        valid != nullptr && !hasDuplicateMembers(valid) &&
        hasOnlyKeys(valid, rootAllowed, 3U) &&
        hasKeys(valid, rootRequired, 3U) &&
        cJSON_IsNumber(cJSON_GetObjectItemCaseSensitive(valid, "v"));
    ok &= require(closedSchema, "KNOWN_SCHEMA_ACCEPTED");
    cJSON_Delete(valid);
    auto* unknown =
        parseExact(R"({"v":1,"r":{"s":0},"i":{"t":"reset-fault"},"x":1})");
    ok &= require(unknown != nullptr && !hasOnlyKeys(unknown, rootAllowed, 3U),
                  "UNKNOWN_FIELD_REJECTABLE");
    cJSON_Delete(unknown);
    auto* missing = parseExact(R"({"v":1,"r":{"s":0}})");
    ok &= require(missing != nullptr && !hasKeys(missing, rootRequired, 3U),
                  "MISSING_FIELD_REJECTABLE");
    cJSON_Delete(missing);
    auto* wrongType =
        parseExact(R"({"v":"1","r":{"s":0},"i":{"t":"reset-fault"}})");
    ok &= require(
        wrongType != nullptr &&
            !cJSON_IsNumber(cJSON_GetObjectItemCaseSensitive(wrongType, "v")),
        "WRONG_TYPE_REJECTABLE");
    cJSON_Delete(wrongType);

    const auto maximumBody = maximumMutationBody();
    auto* maximum = parseMutationBody(maximumBody);
    const auto* revisions =
        maximum == nullptr ? nullptr
                           : cJSON_GetObjectItemCaseSensitive(maximum, "r");
    const auto* userRevision = cJSON_GetObjectItemCaseSensitive(revisions, "u");
    const bool maximumBodyWithinLimit =
        maximumBody.size() <= kMutationBodyLimit;
    std::printf("MAX_PRODUCT_MUTATION_BYTES=%zu;LIMIT=%zu\n",
                maximumBody.size(), kMutationBodyLimit);
    ok &= require(maximumBodyWithinLimit, "MAX_PRODUCT_MUTATION_BODY_480B");
    ok &= require(maximum != nullptr, "MAX_PRODUCT_MUTATION_PARSE");
    std::string bodyAtLimit = maximumBody;
    bodyAtLimit.append(kMutationBodyLimit - bodyAtLimit.size(), ' ');
    auto* atLimit = parseMutationBody(bodyAtLimit);
    ok &=
        require(bodyAtLimit.size() == kMutationBodyLimit && atLimit != nullptr,
                "BODY_AT_480_BYTES_ACCEPTED");
    cJSON_Delete(atLimit);
    const std::string oversizedBody(kMutationBodyLimit + 1U, ' ');
    auto* overLimit = parseMutationBody(oversizedBody);
    ok &= require(overLimit == nullptr, "OVERSIZED_BODY_REJECTED_BEFORE_PARSE");
    cJSON_Delete(overLimit);
    const auto maxValue =
        userRevision == nullptr ? 0.0 : userRevision->valuedouble;
    const std::string overflowBody = R"({"x":18446744073709551616})";
    auto* overflow = parseExact(overflowBody);
    const auto* overflowValue =
        overflow == nullptr ? nullptr
                            : cJSON_GetObjectItemCaseSensitive(overflow, "x");
    const bool uint64AliasesOverflow = userRevision != nullptr &&
                                       overflowValue != nullptr &&
                                       maxValue == overflowValue->valuedouble;
    std::printf("U64_MAX_AS_DOUBLE=%.0f\n", maxValue);
    ok &= require(!uint64AliasesOverflow,
                  "UINT64_MAX_DISTINGUISHABLE_FROM_OVERFLOW");
    cJSON_Delete(overflow);
    cJSON_Delete(maximum);

    auto* uint32Overflow = parseExact(R"({"x":4294967296})");
    const auto* uint32Value =
        uint32Overflow == nullptr
            ? nullptr
            : cJSON_GetObjectItemCaseSensitive(uint32Overflow, "x");
    ok &=
        require(uint32Value != nullptr && cJSON_IsNumber(uint32Value) &&
                    uint32Value->valuedouble > static_cast<double>(UINT32_MAX),
                "UINT32_OVERFLOW_REJECTABLE_BY_RANGE_CHECK");
    cJSON_Delete(uint32Overflow);

    const std::string nulValue = R"({"x":"a\u0000b"})";
    auto* nul = parseExact(nulValue);
    const auto* nulString =
        nul == nullptr ? nullptr : cJSON_GetObjectItemCaseSensitive(nul, "x");
    const bool nulWouldBeAcceptedAsPrefix =
        nulString != nullptr && cJSON_IsString(nulString) &&
        std::strcmp(nulString->valuestring, "a") == 0;
    ok &= require(!nulWouldBeAcceptedAsPrefix,
                  "ESCAPED_NUL_REJECTED_WITHOUT_SOURCE_SCANNER");
    cJSON_Delete(nul);

    std::string rawNul = R"({"x":"a)";
    rawNul.push_back('\0');
    rawNul += R"(b"})";
    auto* rawNulParsed = parseExact(rawNul);
    ok &= require(rawNulParsed == nullptr, "RAW_NUL_REJECTED");
    cJSON_Delete(rawNulParsed);

    std::string invalidUtf8 = R"({"x":")";
    invalidUtf8.push_back(static_cast<char>(0xc3));
    invalidUtf8 += R"("})";
    auto* utf8 = parseExact(invalidUtf8);
    const bool invalidUtf8Accepted = utf8 != nullptr;
    ok &= require(!invalidUtf8Accepted, "INVALID_UTF8_REJECTED_BY_PARSER");
    cJSON_Delete(utf8);

    auto* truncated = parseExact(R"({"x":1)");
    auto* malformed = parseExact(R"({"x":})");
    ok &= require(truncated == nullptr && malformed == nullptr,
                  "MALFORMED_TRUNCATED_REJECTED");
    cJSON_Delete(truncated);
    cJSON_Delete(malformed);

    auto* depth4 = parseExact(R"({"a":{"b":{"c":{"d":0}}}})");
    auto* depth5 = parseExact(R"({"a":{"b":{"c":{"d":{"e":0}}}}})");
    ok &= require(depth4 != nullptr && depth5 == nullptr,
                  "CONFIGURED_NESTING_LIMIT_4");
    cJSON_Delete(depth4);
    cJSON_Delete(depth5);

    auto* nonFinite = parseExact(R"({"x":1e999})");
    const auto* nonFiniteNumber =
        nonFinite == nullptr ? nullptr
                             : cJSON_GetObjectItemCaseSensitive(nonFinite, "x");
    const bool nonFiniteNeedsExplicitCheck =
        nonFiniteNumber != nullptr && cJSON_IsNumber(nonFiniteNumber) &&
        !std::isfinite(nonFiniteNumber->valuedouble);
    ok &= require(nonFiniteNeedsExplicitCheck,
                  "NONFINITE_REQUIRES_EXPLICIT_FINITE_GATE");
    cJSON_Delete(nonFinite);

    ok &= require(maxResponsesFitBoundedBuffers(),
                  "MAX_READ_ONLY_RESPONSES_PREALLOCATED_3072B");
    return ok ? 0 : 1;
}

}  // namespace

#ifdef ESP_PLATFORM
extern "C" void app_main() { (void)runProbe(); }
#else
int main() { return runProbe(); }
#endif
