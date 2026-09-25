#include <unity.h>

#include "web_browser_policy.hpp"

namespace {

device_platform::HttpRequest sameOriginRequest() {
    device_platform::HttpRequest request;
    request.metadata.host = "fermentation.local";
    request.metadata.origin = "http://fermentation.local";
    request.metadata.secFetchSite = "same-origin";
    return request;
}

void test_same_origin_requires_fetch_metadata_and_matching_origin() {
    auto request = sameOriginRequest();
    TEST_ASSERT_TRUE(fermentation::web_browser_policy::sameOrigin(request));

    request.metadata.origin = "http://other.local";
    TEST_ASSERT_FALSE(fermentation::web_browser_policy::sameOrigin(request));
    request.metadata.origin = "null";
    request.metadata.referer = "http://fermentation.local/";
    TEST_ASSERT_FALSE(fermentation::web_browser_policy::sameOrigin(request));
    request.metadata.origin.reset();
    TEST_ASSERT_TRUE(fermentation::web_browser_policy::sameOrigin(request));
    request.metadata.secFetchSite = "cross-site";
    TEST_ASSERT_FALSE(fermentation::web_browser_policy::sameOrigin(request));
}

void test_json_content_type_accepts_only_supported_form() {
    using fermentation::web_browser_policy::exactJsonContentType;
    TEST_ASSERT_TRUE(exactJsonContentType("application/json"));
    TEST_ASSERT_TRUE(
        exactJsonContentType(" Application/JSON ; charset=utf-8 "));
    TEST_ASSERT_TRUE(
        exactJsonContentType("application/json; charset=\"utf-8\""));
    TEST_ASSERT_FALSE(exactJsonContentType("application/jsonp"));
    TEST_ASSERT_FALSE(
        exactJsonContentType("application/json; charset=utf-8; extra=yes"));
}

}  // namespace

void setup() {}
void loop() {}

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_same_origin_requires_fetch_metadata_and_matching_origin);
    RUN_TEST(test_json_content_type_accepts_only_supported_form);
    return UNITY_END();
}
