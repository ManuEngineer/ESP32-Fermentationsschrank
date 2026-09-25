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

void test_raw_request_header_validation_rejects_security_header_duplicates() {
    const std::string valid =
        "POST /internal/ui/run HTTP/1.1\r\n"
        "Host: fermentation.local\r\n"
        "Cookie: FSSESSION=0123456789abcdef0123456789abcdef\r\n"
        "X-CSRF-Token: 0123456789abcdef0123456789abcdef\r\n"
        "\r\n";
    TEST_ASSERT_TRUE(
        device_platform::validateUniqueHttpRequestMetadataHeaders(valid));

    const std::string duplicate =
        "POST /internal/ui/run HTTP/1.1\r\n"
        "X-CSRF-Token: first\r\n"
        "x-csrf-token: second\r\n"
        "\r\n";
    TEST_ASSERT_FALSE(
        device_platform::validateUniqueHttpRequestMetadataHeaders(duplicate));
}

void test_raw_request_header_validation_fails_closed_on_malformed_blocks() {
    TEST_ASSERT_FALSE(device_platform::validateUniqueHttpRequestMetadataHeaders(
        "POST / HTTP/1.1\r\n X-CSRF-Token: folded\r\n\r\n"));
    TEST_ASSERT_FALSE(device_platform::validateUniqueHttpRequestMetadataHeaders(
        "POST / HTTP/1.1\r\nX-CSRF-Token:\t \r\n\r\n"));
    TEST_ASSERT_FALSE(device_platform::validateUniqueHttpRequestMetadataHeaders(
        "POST / HTTP/1.1\r\nHost: missing-terminator\r\n"));
    TEST_ASSERT_FALSE(device_platform::validateUniqueHttpRequestMetadataHeaders(
        "POST / HTTP/1.1\r\nX-Unknown: " + std::string(2049U, 'x') +
        "\r\n\r\n"));
}

}  // namespace

void setup() {}
void loop() {}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_bounded_http_metadata_accepts_known_values);
    RUN_TEST(test_http_metadata_rejects_control_and_capacity_values);
    RUN_TEST(test_response_metadata_is_bounded_and_ascii);
    RUN_TEST(
        test_raw_request_header_validation_rejects_security_header_duplicates);
    RUN_TEST(
        test_raw_request_header_validation_fails_closed_on_malformed_blocks);
    return UNITY_END();
}
