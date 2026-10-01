#include <unity.h>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

#include "mock_secure_random_source.hpp"
#include "softap_credentials.hpp"

namespace {

std::string bytes(const std::array<std::uint8_t, 16U>& values) {
    return {reinterpret_cast<const char*>(values.data()), values.size()};
}

void test_softap_credentials_use_device_name_and_approved_password_contract() {
    device_platform_test_support::MockSecureRandomSource source;
    source.setNextBytes(bytes({0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U, 10U, 11U,
                               12U, 13U, 14U, 15U}));
    const auto credentials =
        fermentation::makeSoftApCredentials(source, "Fermentationsschrank");

    TEST_ASSERT_TRUE(credentials.has_value());
    TEST_ASSERT_EQUAL_STRING("Fermentationsschrank", credentials->ssid.c_str());
    TEST_ASSERT_EQUAL_UINT(16U, credentials->password.size());
    TEST_ASSERT_EQUAL_UINT(42U, fermentation::kSoftApPasswordAlphabet.size());
    for (const auto character : credentials->password) {
        TEST_ASSERT_TRUE(fermentation::kSoftApPasswordAlphabet.find(
                             character) != std::string_view::npos);
    }
}

void test_softap_credentials_different_random_sequences_differ() {
    device_platform_test_support::MockSecureRandomSource firstSource;
    device_platform_test_support::MockSecureRandomSource secondSource;
    firstSource.setNextBytes(bytes(
        {0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U, 0U}));
    secondSource.setNextBytes(bytes(
        {1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U, 1U}));

    const auto first = fermentation::makeSoftApCredentials(
        firstSource, "Fermentationsschrank");
    const auto second = fermentation::makeSoftApCredentials(
        secondSource, "Fermentationsschrank");
    TEST_ASSERT_TRUE(first.has_value());
    TEST_ASSERT_TRUE(second.has_value());
    TEST_ASSERT_TRUE(first->password != second->password);
}

void test_softap_credentials_reject_random_source_failure_without_fallback() {
    device_platform_test_support::MockSecureRandomSource source;
    source.injectFailure(true);
    TEST_ASSERT_FALSE(
        fermentation::makeSoftApCredentials(source, "Fermentationsschrank")
            .has_value());
}

void test_softap_credentials_rejection_sampling_skips_out_of_range_bytes() {
    device_platform_test_support::MockSecureRandomSource source;
    source.setNextBytes(bytes({255U, 254U, 253U, 252U, 0U, 1U, 2U, 3U, 4U, 5U,
                               6U, 7U, 8U, 9U, 10U, 11U}));
    const auto credentials =
        fermentation::makeSoftApCredentials(source, "Fermentationsschrank");
    TEST_ASSERT_TRUE(credentials.has_value());
    TEST_ASSERT_EQUAL_UINT(16U, credentials->password.size());
}

void test_ssid_truncation_preserves_complete_utf8_codepoints_and_raw_budget() {
    const std::string deviceName = std::string(22U, 'a') + "\xC3\xA4" + "z";
    const auto truncated = fermentation::deriveSoftApSsid(deviceName);
    TEST_ASSERT_TRUE(truncated.has_value());
    TEST_ASSERT_EQUAL_UINT(24U, truncated->size());
    TEST_ASSERT_EQUAL_STRING((std::string(22U, 'a') + "\xC3\xA4").c_str(),
                             truncated->c_str());

    const auto asciiLimited =
        fermentation::deriveSoftApSsid(std::string(24U, 'b') + "z");
    TEST_ASSERT_TRUE(asciiLimited.has_value());
    TEST_ASSERT_EQUAL_UINT(24U, asciiLimited->size());
    TEST_ASSERT_EQUAL_STRING(std::string(24U, 'b').c_str(),
                             asciiLimited->c_str());
}

void test_ssid_reserved_wifi_bytes_and_escaped_budget_are_bounded() {
    const std::string reserved = R"(A\;,:\")";
    const auto preserved = fermentation::deriveSoftApSsid(reserved);
    TEST_ASSERT_TRUE(preserved.has_value());
    TEST_ASSERT_EQUAL_STRING(reserved.c_str(), preserved->c_str());

    const auto escapedLimited =
        fermentation::deriveSoftApSsid(std::string(15U, '\\'));
    TEST_ASSERT_TRUE(escapedLimited.has_value());
    TEST_ASSERT_EQUAL_UINT(14U, escapedLimited->size());
    TEST_ASSERT_TRUE(*escapedLimited == std::string(14U, '\\'));
}

}  // namespace

// Native tests use the production helper without compiling the ESP-IDF
// composition root or introducing a second random source.
int main() {
    UNITY_BEGIN();
    RUN_TEST(
        test_softap_credentials_use_device_name_and_approved_password_contract);
    RUN_TEST(test_softap_credentials_different_random_sequences_differ);
    RUN_TEST(
        test_softap_credentials_reject_random_source_failure_without_fallback);
    RUN_TEST(
        test_softap_credentials_rejection_sampling_skips_out_of_range_bytes);
    RUN_TEST(
        test_ssid_truncation_preserves_complete_utf8_codepoints_and_raw_budget);
    RUN_TEST(test_ssid_reserved_wifi_bytes_and_escaped_budget_are_bounded);
    return UNITY_END();
}
