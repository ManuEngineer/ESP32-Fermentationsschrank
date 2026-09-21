#include <unity.h>

#include "web_browser_policy.hpp"

namespace {

device_platform::HttpRequest browserRequest() {
    device_platform::HttpRequest request{"POST", "/login", "{}"};
    request.metadata.host = "fermentation.local";
    request.metadata.origin = "http://fermentation.local";
    request.metadata.secFetchSite = "same-origin";
    return request;
}

void test_same_origin_requires_fetch_metadata_and_origin_or_referer() {
    auto request = browserRequest();
    TEST_ASSERT_TRUE(fermentation::web_browser_policy::sameOrigin(request));

    request.metadata.secFetchSite.reset();
    TEST_ASSERT_FALSE(fermentation::web_browser_policy::sameOrigin(request));

    request = browserRequest();
    request.metadata.origin.reset();
    request.metadata.referer = "http://fermentation.local/login";
    TEST_ASSERT_TRUE(fermentation::web_browser_policy::sameOrigin(request));

    request.metadata.referer.reset();
    TEST_ASSERT_FALSE(fermentation::web_browser_policy::sameOrigin(request));
}

void test_same_origin_rejects_opaque_foreign_and_https_contexts() {
    auto request = browserRequest();
    request.metadata.origin = "null";
    TEST_ASSERT_FALSE(fermentation::web_browser_policy::sameOrigin(request));

    request = browserRequest();
    request.metadata.origin = "http://foreign.local";
    TEST_ASSERT_FALSE(fermentation::web_browser_policy::sameOrigin(request));

    request = browserRequest();
    request.metadata.origin = "https://fermentation.local";
    TEST_ASSERT_FALSE(fermentation::web_browser_policy::sameOrigin(request));

    request = browserRequest();
    request.metadata.secFetchSite = "cross-site";
    TEST_ASSERT_FALSE(fermentation::web_browser_policy::sameOrigin(request));
}

void test_json_content_type_is_exact_and_bounded() {
    TEST_ASSERT_TRUE(fermentation::web_browser_policy::exactJsonContentType(
        "application/json"));
    TEST_ASSERT_TRUE(fermentation::web_browser_policy::exactJsonContentType(
        " Application/JSON; charset=\"utf-8\" "));
    TEST_ASSERT_FALSE(fermentation::web_browser_policy::exactJsonContentType(
        "application/jsonp"));
    TEST_ASSERT_FALSE(fermentation::web_browser_policy::exactJsonContentType(
        "application/json; charset=utf-8; profile=unsafe"));
    TEST_ASSERT_FALSE(fermentation::web_browser_policy::exactJsonContentType(
        "text/plain"));
}

}  // namespace

int main() {
    UNITY_BEGIN();
    RUN_TEST(test_same_origin_requires_fetch_metadata_and_origin_or_referer);
    RUN_TEST(test_same_origin_rejects_opaque_foreign_and_https_contexts);
    RUN_TEST(test_json_content_type_is_exact_and_bounded);
    return UNITY_END();
}
