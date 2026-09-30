#include <unity.h>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>

#include "mock_secure_random_source.hpp"
#include "../../main/softap_credentials.hpp"

namespace {

std::string bytes(const std::array<std::uint8_t, 16U>& values) {
    return {reinterpret_cast<const char*>(values.data()), values.size()};
}

void test_softap_credentials_use_fixed_ssid_and_approved_password_contract() {
    device_platform_test_support::MockSecureRandomSource source;
    source.setNextBytes(bytes({0U, 1U, 2U, 3U, 4U, 5U, 6U, 7U, 8U, 9U, 10U, 11U,
                               12U, 13U, 14U, 15U}));
    const auto credentials = fermentation::makeSoftApCredentials(source);

    TEST_ASSERT_TRUE(credentials.has_value());
    TEST_ASSERT_EQUAL_STRING("Fermentation", credentials->ssid.c_str());
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

    const auto first = fermentation::makeSoftApCredentials(firstSource);
    const auto second = fermentation::makeSoftApCredentials(secondSource);
    TEST_ASSERT_TRUE(first.has_value());
    TEST_ASSERT_TRUE(second.has_value());
    TEST_ASSERT_TRUE(first->password != second->password);
}

void test_softap_credentials_reject_random_source_failure_without_fallback() {
    device_platform_test_support::MockSecureRandomSource source;
    source.injectFailure(true);
    TEST_ASSERT_FALSE(fermentation::makeSoftApCredentials(source).has_value());
}

void test_softap_credentials_rejection_sampling_skips_out_of_range_bytes() {
    device_platform_test_support::MockSecureRandomSource source;
    source.setNextBytes(bytes({255U, 254U, 253U, 252U, 0U, 1U, 2U, 3U, 4U, 5U,
                               6U, 7U, 8U, 9U, 10U, 11U}));
    const auto credentials = fermentation::makeSoftApCredentials(source);
    TEST_ASSERT_TRUE(credentials.has_value());
    TEST_ASSERT_EQUAL_UINT(16U, credentials->password.size());
}

}  // namespace

// Native tests use the production helper without compiling the ESP-IDF
// composition root or introducing a second random source.
#include "../../main/softap_credentials.cpp"

int main() {
    UNITY_BEGIN();
    RUN_TEST(
        test_softap_credentials_use_fixed_ssid_and_approved_password_contract);
    RUN_TEST(test_softap_credentials_different_random_sequences_differ);
    RUN_TEST(
        test_softap_credentials_reject_random_source_failure_without_fallback);
    RUN_TEST(
        test_softap_credentials_rejection_sampling_skips_out_of_range_bytes);
    return UNITY_END();
}
