#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>
#include <string>
#include <string_view>

#include <drogon/HttpResponse.h>
#include <nlohmann/json.hpp>

#include <nodepulse/utils/error_response.hpp>

namespace nodepulse::utils {

std::string get_current_iso8601_timestamp() {
    auto now = std::chrono::system_clock::now();
    std::time_t tt = std::chrono::system_clock::to_time_t(now);
    std::tm tm_buf{};
    gmtime_r(&tt, &tm_buf);

    std::ostringstream ss;
    ss << std::put_time(&tm_buf, "%Y-%m-%dT%H:%M:%SZ");
    return ss.str();
}

nlohmann::json make_error_json(std::string_view code, std::string_view message,
                               const nlohmann::json& details, std::string_view timestamp) {
    std::string ts = timestamp.empty() ? get_current_iso8601_timestamp() : std::string(timestamp);
    nlohmann::json error_obj = {{"code", std::string(code)},
                                {"message", std::string(message)},
                                {"timestamp", ts},
                                {"details", details.is_null() ? nlohmann::json::array() : details}};
    return nlohmann::json{{"error", error_obj}};
}

drogon::HttpResponsePtr make_error_response(drogon::HttpStatusCode status_code,
                                            std::string_view code, std::string_view message,
                                            const nlohmann::json& details,
                                            std::string_view request_id) {
    auto json_body = make_error_json(code, message, details);
    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setStatusCode(status_code);
    resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    resp->addHeader("Content-Type", "application/json");
    resp->setBody(json_body.dump());

    if (!request_id.empty()) {
        resp->addHeader("X-Request-ID", std::string(request_id));
    }

    return resp;
}

}  // namespace nodepulse::utils
