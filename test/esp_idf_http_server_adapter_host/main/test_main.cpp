#include <cstdlib>
#include <cstring>
#include <cerrno>
#include <cstdio>
#include <iostream>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>
#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>

#include "esp_http_server.h"
#include "esp_event.h"
#include "esp_idf_http_server_lifecycle.hpp"

namespace adapter_test {

struct PublicApiObservation {
    std::string rawData;
    std::size_t csrfLength{0U};
    esp_err_t csrfResult{ESP_FAIL};
    std::string csrfValue;
};

std::mutex observationsMutex;
std::vector<PublicApiObservation> observations;

}  // namespace adapter_test

extern "C" esp_err_t __real_httpd_start(httpd_handle_t* handle,
                                        const httpd_config_t* config);
extern "C" esp_err_t __wrap_httpd_start(httpd_handle_t* handle,
                                        const httpd_config_t* config) {
    auto unprivilegedConfig = *config;
    unprivilegedConfig.server_port = 8080U;
    return __real_httpd_start(handle, &unprivilegedConfig);
}

extern "C" esp_err_t __real_httpd_get_raw_req_data(httpd_req_t* request,
                                                   char* buffer, size_t length);
extern "C" esp_err_t __wrap_httpd_get_raw_req_data(httpd_req_t* request,
                                                   char* buffer,
                                                   size_t length) {
    const auto result = __real_httpd_get_raw_req_data(request, buffer, length);
    if (result == ESP_OK) {
        adapter_test::PublicApiObservation observation;
        observation.rawData.assign(buffer, length);
        observation.csrfLength =
            httpd_req_get_hdr_value_len(request, "X-CSRF-Token");
        std::string csrf(observation.csrfLength + 1U, '\0');
        observation.csrfResult = httpd_req_get_hdr_value_str(
            request, "X-CSRF-Token", csrf.data(), csrf.size());
        observation.csrfValue = csrf.c_str();
        std::lock_guard<std::mutex> lock(adapter_test::observationsMutex);
        adapter_test::observations.push_back(std::move(observation));
    }
    return result;
}

namespace {

class RecordingRouteSink final : public device_platform::IHttpRouteSink {
   public:
    bool handle(const device_platform::HttpRequest& request,
                device_platform::HttpResponse& response) override {
        requests.push_back(request);
        response.statusCode = 200U;
        response.contentType = "text/plain; charset=utf-8";
        response.body = "route-sink-reached";
        return true;
    }

    std::vector<device_platform::HttpRequest> requests;
};

unsigned failures = 0U;

void expect(bool condition, const char* label) {
    std::cout << "HTTP_ADAPTER_CASE=" << label << "="
              << (condition ? "PASS" : "FAIL") << std::endl;
    if (!condition) ++failures;
}

void printStatus(const char* label, const std::string& response) {
    const auto lineEnd = response.find("\r\n");
    std::cout << "HTTP_ADAPTER_RESPONSE=" << label << "="
              << response.substr(0U, lineEnd) << std::endl;
}

std::optional<std::size_t> responseBodyLength(const std::string& response);

std::string exchange(const std::string& request) {
    const int socketFd = ::socket(AF_INET, SOCK_STREAM, IPPROTO_IP);
    if (socketFd < 0) {
        std::perror("host test socket");
        return {};
    }

    timeval timeout{};
    timeout.tv_sec = 2;
    static_cast<void>(::setsockopt(socketFd, SOL_SOCKET, SO_RCVTIMEO, &timeout,
                                   sizeof(timeout)));

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_port = htons(8080U);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    int connectResult = -1;
    do {
        connectResult = ::connect(
            socketFd, reinterpret_cast<sockaddr*>(&address), sizeof(address));
    } while (connectResult != 0 && errno == EINTR);
    if (connectResult != 0 && errno != EISCONN) {
        std::perror("host test connect");
        static_cast<void>(::close(socketFd));
        return {};
    }

    std::size_t sent = 0U;
    while (sent < request.size()) {
        auto count =
            ::send(socketFd, request.data() + sent, request.size() - sent, 0);
        while (count < 0 && errno == EINTR) {
            count = ::send(socketFd, request.data() + sent,
                           request.size() - sent, 0);
        }
        if (count <= 0) {
            std::perror("host test send");
            static_cast<void>(::close(socketFd));
            return {};
        }
        sent += static_cast<std::size_t>(count);
    }

    std::string response;
    char buffer[512];
    for (;;) {
        auto count = ::recv(socketFd, buffer, sizeof(buffer), 0);
        while (count < 0 && errno == EINTR) {
            count = ::recv(socketFd, buffer, sizeof(buffer), 0);
        }
        if (count <= 0) {
            if (count < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
                std::perror("host test recv");
            }
            break;
        }
        response.append(buffer, static_cast<std::size_t>(count));
        const auto bodyLength = responseBodyLength(response);
        const auto headerEnd = response.find("\r\n\r\n");
        if (bodyLength.has_value() && headerEnd != std::string::npos &&
            response.size() >= headerEnd + 4U + *bodyLength) {
            break;
        }
        if (response.size() > 8192U) break;
    }
    static_cast<void>(::close(socketFd));
    return response;
}

bool hasStatus(const std::string& response, const char* status) {
    return response.find(status) != std::string::npos;
}

std::optional<std::size_t> responseBodyLength(const std::string& response) {
    const auto headerEnd = response.find("\r\n\r\n");
    if (headerEnd == std::string::npos) return std::nullopt;
    const auto lengthStart = response.find("Content-Length:");
    if (lengthStart == std::string::npos || lengthStart > headerEnd) {
        return std::nullopt;
    }
    const auto valueStart = lengthStart + std::strlen("Content-Length:");
    const auto valueEnd = response.find("\r\n", valueStart);
    if (valueEnd == std::string::npos || valueEnd > headerEnd) {
        return std::nullopt;
    }
    const auto value = response.substr(valueStart, valueEnd - valueStart);
    return static_cast<std::size_t>(std::strtoul(value.c_str(), nullptr, 10));
}

std::string requestWithHeaders(const char* method, const char* path,
                               const std::string& headers,
                               const std::string& body = {}) {
    return std::string(method) + " " + path + " HTTP/1.1\r\n" + headers +
           "Content-Length: " + std::to_string(body.size()) +
           "\r\nConnection: close\r\n\r\n" + body;
}

void runAdapterDispatchCases() {
    RecordingRouteSink routes;
    device_platform_esp_idf::EspIdfHttpServerLifecycle server;
    static_cast<void>(esp_event_loop_create_default());
    expect(server.start(routes), "server-start");
    if (!server.running()) return;

    const auto getResponse = exchange(
        requestWithHeaders("GET", "/",
                           "Host: esp.local\r\nOrigin: http://esp.local\r\n"
                           "Sec-Fetch-Site: same-origin\r\n"));
    printStatus("GET_ROOT", getResponse);
    expect(hasStatus(getResponse, "200 OK") && routes.requests.size() == 1U,
           "get-root-reaches-route-sink");
    if (!routes.requests.empty()) {
        const auto& request = routes.requests.back();
        expect(request.method == "GET" && request.path == "/" &&
                   request.metadata.host == "esp.local" &&
                   request.metadata.origin == "http://esp.local" &&
                   request.metadata.secFetchSite == "same-origin",
               "allowed-get-metadata-extracted");
    }

    const auto postResponse = exchange(requestWithHeaders(
        "POST", "/api/network/candidate",
        "Host: esp.local\r\nContent-Type: application/x-www-form-urlencoded\r\n"
        "Cookie: FSSESSION=0123456789abcdef0123456789abcdef\r\n"
        "X-CSRF-Token: 0123456789abcdef0123456789abcdef\r\n"
        "X-UI-Mutation-Seq: 7\r\nOrigin: http://esp.local\r\n"
        "Referer: http://esp.local/\r\nSec-Fetch-Site: same-origin\r\n",
        "ssid=test&password=secret"));
    printStatus("POST", postResponse);
    expect(hasStatus(postResponse, "200 OK") && routes.requests.size() == 2U,
           "post-reaches-route-sink");
    if (routes.requests.size() >= 2U) {
        const auto& request = routes.requests.back();
        expect(request.method == "POST" &&
                   request.path == "/api/network/candidate" &&
                   request.body == "ssid=test&password=secret" &&
                   request.metadata.cookie ==
                       "FSSESSION=0123456789abcdef0123456789abcdef" &&
                   request.metadata.csrfToken ==
                       "0123456789abcdef0123456789abcdef" &&
                   request.metadata.mutationSeq == "7" &&
                   request.metadata.referer == "http://esp.local/",
               "allowed-post-metadata-extracted");
    }

    const auto duplicateResponse =
        exchange(requestWithHeaders("POST", "/internal/ui/run",
                                    "Host: esp.local\r\nX-CSRF-Token: first\r\n"
                                    "x-csrf-token: second\r\n"));
    printStatus("DUPLICATE", duplicateResponse);
    expect(hasStatus(duplicateResponse, "431") && routes.requests.size() == 2U,
           "duplicate-security-header-rejected-before-route");

    const auto oversizedResponse =
        exchange(requestWithHeaders("GET", "/",
                                    "Host: esp.local\r\nOrigin: http://" +
                                        std::string(260U, 'a') + ".local\r\n"));
    printStatus("OVERSIZED", oversizedResponse);
    expect(hasStatus(oversizedResponse, "431") && routes.requests.size() == 2U,
           "oversized-metadata-rejected-before-route");

    const auto emptyResponse = exchange(requestWithHeaders(
        "POST", "/internal/ui/run", "Host: esp.local\r\nX-CSRF-Token:\r\n"));
    printStatus("EMPTY", emptyResponse);
    expect(hasStatus(emptyResponse, "431") && routes.requests.size() == 2U,
           "empty-security-metadata-rejected-before-route");

    const auto invalidResponse = exchange(requestWithHeaders(
        "GET", "/", "Host: esp.local\r\nOrigin: http://esp.local\tbad\r\n"));
    printStatus("INVALID", invalidResponse);
    expect((hasStatus(invalidResponse, "400") ||
            hasStatus(invalidResponse, "431")) &&
               routes.requests.size() == 2U,
           "invalid-control-metadata-fails-closed");

    {
        std::lock_guard<std::mutex> lock(adapter_test::observationsMutex);
        expect(adapter_test::observations.size() == 6U,
               "public-api-observed-each-dispatch");
        if (adapter_test::observations.size() == 6U) {
            const auto& get = adapter_test::observations[0U];
            const auto& duplicate = adapter_test::observations[2U];
            const auto& empty = adapter_test::observations[4U];
            const auto headerTerminator = std::string(4U, '\0');
            const auto duplicateEnd = duplicate.rawData.find(headerTerminator);
            expect(get.rawData.find("GET / HTTP/1.1") == std::string::npos &&
                       get.rawData.find("Host: esp.local") != std::string::npos,
                   "public-raw-api-handler-data-is-header-only");
            expect(duplicateEnd != std::string::npos &&
                       duplicate.rawData.find("X-CSRF-Token: first") <
                           duplicateEnd &&
                       duplicate.rawData.find("x-csrf-token: second") <
                           duplicateEnd &&
                       duplicate.csrfLength == 5U &&
                       duplicate.csrfResult == ESP_OK &&
                       duplicate.csrfValue == "first",
                   "public-raw-api-preserves-duplicates-single-value-api-"
                   "returns-first");
            expect(get.csrfLength == 0U &&
                       get.csrfResult == ESP_ERR_NOT_FOUND &&
                       empty.csrfLength == 0U && empty.csrfResult == ESP_OK &&
                       empty.csrfValue.empty(),
                   "public-value-api-distinguishes-absent-from-empty-header");
        }
    }

    expect(server.stop(), "server-stop");
}

}  // namespace

extern "C" void app_main() {
    runAdapterDispatchCases();
    std::cout << "ESP_IDF_HTTP_ADAPTER_DISPATCH="
              << (failures == 0U ? "PASS" : "FAIL") << std::endl;
    std::exit(failures == 0U ? 0 : 1);
}
