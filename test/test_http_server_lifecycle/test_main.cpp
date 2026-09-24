#include <string>

#include <unity.h>

#include "http_server_lifecycle.hpp"

namespace {

void test_bounded_http_metadata_accepts_known_values() {
    device_platform::HttpRequestMetadata metadata;
    metadata.host = "fermentation.local";
    metadata.contentType = "application/json; charset=utf-8";
    metadata.cookie = "FSSESSION=0123456789abcdef0123456789abcdef";
    metadata.csrfToken = std::string(32U, 'a');
    metadata.mutationSeq = "9";
    metadata.origin = "http://fermentation.local";
    metadata.referer = "http://fermentation.local/";
    metadata.secFetchSite = "same-origin";
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::HttpMetadataValidation::Valid),
        static_cast<int>(
            device_platform::validateHttpRequestMetadata(metadata)));
}

void test_http_metadata_rejects_control_and_capacity_values() {
    device_platform::HttpRequestMetadata metadata;
    metadata.host = "bad\r\nvalue";
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::HttpMetadataValidation::Invalid),
        static_cast<int>(
            device_platform::validateHttpRequestMetadata(metadata)));
    metadata.host.reset();
    metadata.cookie = std::string(513U, 'x');
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::HttpMetadataValidation::Invalid),
        static_cast<int>(
            device_platform::validateHttpRequestMetadata(metadata)));
    metadata.cookie.reset();
    metadata.mutationSeq = "000000000000000000001";
    TEST_ASSERT_EQUAL_INT(
        static_cast<int>(device_platform::HttpMetadataValidation::Invalid),
        static_cast<int>(
            device_platform::validateHttpRequestMetadata(metadata)));
}

void test_response_metadata_is_bounded_and_ascii() {
    device_platform::HttpResponseMetadata metadata;
    metadata.setCookie = "FSSESSION=x; HttpOnly; SameSite=Strict; Path=/";
    metadata.retryAfter = "30";
    TEST_ASSERT_TRUE(device_platform::validateHttpResponseMetadata(metadata));
    metadata.retryAfter = "3\n0";
    TEST_ASSERT_FALSE(device_platform::validateHttpResponseMetadata(metadata));
}

}  // namespace

void setup() {}
void loop() {}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_bounded_http_metadata_accepts_known_values);
    RUN_TEST(test_http_metadata_rejects_control_and_capacity_values);
    RUN_TEST(test_response_metadata_is_bounded_and_ascii);
    return UNITY_END();
}
