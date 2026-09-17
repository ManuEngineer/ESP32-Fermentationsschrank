#include "network_setup_routes.hpp"

#include <cctype>
#include <cstdint>
#include <string>
#include <utility>

namespace fermentation {
namespace {

void setText(device_platform::HttpResponse& response, std::uint16_t status,
             std::string body) {
    response.statusCode = status;
    response.contentType = "text/plain; charset=utf-8";
    response.body = std::move(body);
}

int hexValue(char value) {
    if (value >= '0' && value <= '9') {
        return value - '0';
    }
    if (value >= 'a' && value <= 'f') {
        return value - 'a' + 10;
    }
    if (value >= 'A' && value <= 'F') {
        return value - 'A' + 10;
    }
    return -1;
}

bool decodeFormValue(const std::string& encoded, std::string& decoded) {
    std::string value;
    value.reserve(encoded.size());
    for (std::size_t index = 0U; index < encoded.size(); ++index) {
        const char current = encoded[index];
        if (current == '+') {
            value.push_back(' ');
        } else if (current == '%' && index + 2U < encoded.size()) {
            const int high = hexValue(encoded[index + 1U]);
            const int low = hexValue(encoded[index + 2U]);
            if (high < 0 || low < 0) {
                return false;
            }
            value.push_back(static_cast<char>((high << 4) | low));
            index += 2U;
        } else if (current == '%') {
            return false;
        } else {
            value.push_back(current);
        }
    }
    decoded = std::move(value);
    return true;
}

bool findFormValue(const std::string& body, const std::string& name,
                   std::string& value) {
    std::size_t start = 0U;
    while (start <= body.size()) {
        const auto end = body.find('&', start);
        const auto fieldEnd = end == std::string::npos ? body.size() : end;
        const auto separator = body.find('=', start);
        if (separator != std::string::npos && separator < fieldEnd &&
            body.compare(start, separator - start, name) == 0) {
            return decodeFormValue(
                body.substr(separator + 1U, fieldEnd - separator - 1U), value);
        }
        if (end == std::string::npos) {
            break;
        }
        start = end + 1U;
    }
    return false;
}

}  // namespace

bool NetworkSetupRoutes::handle(const device_platform::HttpRequest& request,
                                device_platform::HttpResponse& response) {
    if (request.method == "GET" && request.path == "/") {
        response.statusCode = 200U;
        response.contentType = "text/html; charset=utf-8";
        if (!networkService_.setupFlowActive()) {
            response.body =
                "<!doctype html><meta charset=utf-8>"
                "<title>Network status</title><h1>Network status</h1>"
                "<p>Setup is not available in the active network mode.</p>";
            return true;
        }
        response.body =
            "<!doctype html><meta charset=utf-8><title>Network setup</title>"
            "<h1>Network setup</h1>"
            "<form method=post action=/api/network/candidate>"
            "<label>SSID <input name=ssid required maxlength=32></label>"
            "<label>Password <input name=password type=password required "
            "maxlength=63></label>"
            "<button type=button id=scan>Scan</button>"
            "<label>Scanned SSID <select id=scan-ssid size=4>"
            "<option value=''>Select scanned SSID</option></select></label>"
            "<button type=submit>Test and commit</button></form>"
            "<pre id=scan-result aria-live=polite></pre>"
            "<p id=result aria-live=polite></p>"
            "<script>const r=document.getElementById('scan-result');"
            "const input=document.querySelector('input[name=ssid]');"
            "const select=document.getElementById('scan-ssid');"
            "select.onchange=()=>{if(select.value)input.value=select.value;};"
            "document.getElementById('scan').onclick=async()=>{"
            "const x=await fetch('/api/network/scan');"
            "const names=(await x.text()).split('\\n').filter(Boolean);"
            "select.replaceChildren(...names.map(name=>{"
            "const option=document.createElement('option');"
            "option.value=name;option.textContent=name;return option;}));"
            "r.textContent=names.join('\\n');};"
            "document.querySelector('form').onsubmit=async "
            "e=>{e.preventDefault();"
            "const x=await fetch(e.target.action,{method:'POST',body:new "
            "URLSearchParams(new FormData(e.target))});"
            "document.getElementById('result').textContent=await x.text();};"
            "</script>";
        return true;
    }
    if (request.method == "GET" && request.path == "/api/network/status") {
        const auto status = networkService_.status();
        setText(response, 200U,
                std::string("mode=") +
                    std::to_string(static_cast<unsigned>(status.selectedMode)) +
                    " state=" +
                    std::to_string(static_cast<unsigned>(status.state)));
        return true;
    }
    if (request.method == "GET" && request.path == "/api/network/scan") {
        const auto scan = networkService_.scan();
        if (scan.status != NetworkConfigurationStatus::Applied) {
            setText(response, 503U, "scan unavailable");
            return true;
        }
        std::string body;
        for (const auto& entry : scan.entries) {
            body += entry.ssid;
            body += '\n';
        }
        setText(response, 200U, std::move(body));
        return true;
    }
    if (request.method == "POST" && request.path == "/api/network/candidate") {
        std::string ssid;
        std::string password;
        if (!findFormValue(request.body, "ssid", ssid) ||
            !findFormValue(request.body, "password", password)) {
            setText(response, 400U, "ssid and password required");
            return true;
        }
        const auto candidate = networkService_.beginCandidate(
            std::move(ssid), std::move(password));
        if (candidate.status != NetworkConfigurationStatus::Applied) {
            setText(response,
                    candidate.status ==
                            NetworkConfigurationStatus::SetupNotAvailable
                        ? 503U
                        : 400U,
                    candidate.status ==
                            NetworkConfigurationStatus::SetupNotAvailable
                        ? "setup unavailable"
                        : "invalid candidate");
            return true;
        }
        const auto result = networkService_.testCandidate();
        if (result.status != NetworkConfigurationStatus::Applied) {
            switch (result.status) {
                case NetworkConfigurationStatus::CandidateRejected:
                    setText(response, 422U,
                            "candidate rejected; no credential commit");
                    break;
                case NetworkConfigurationStatus::PersistenceFailure:
                    setText(response, 500U,
                            "candidate tested; credential write failed");
                    break;
                case NetworkConfigurationStatus::CommitIndeterminate:
                    setText(
                        response, 503U,
                        "credential commit indeterminate; recovery required");
                    break;
                case NetworkConfigurationStatus::RecoveryRequired:
                    setText(response, 503U,
                            "credential committed; runtime recovery required");
                    break;
                default:
                    setText(response, 503U, "network recovery required");
                    break;
            }
            return true;
        }
        // Never echo the password or the complete credential in a response.
        setText(response, 200U, "candidate committed");
        return true;
    }
    return false;
}

}  // namespace fermentation
