#pragma once

#include <string>
#include <string_view>

#include <drogon/HttpResponse.h>
#include <nlohmann/json.hpp>

namespace nodepulse::utils {

namespace error_codes {

inline constexpr std::string_view kInvalidRequest = "INVALID_REQUEST";
inline constexpr std::string_view kUnauthorized = "UNAUTHORIZED";
inline constexpr std::string_view kForbidden = "FORBIDDEN";
inline constexpr std::string_view kResourceNotFound = "RESOURCE_NOT_FOUND";
inline constexpr std::string_view kRateLimited = "RATE_LIMITED";
inline constexpr std::string_view kCollectorFailure = "COLLECTOR_FAILURE";
inline constexpr std::string_view kDockerUnavailable = "DOCKER_UNAVAILABLE";
inline constexpr std::string_view kServiceUnavailable = "SERVICE_UNAVAILABLE";
inline constexpr std::string_view kInternalError = "INTERNAL_ERROR";

}  // namespace error_codes

[[nodiscard]] std::string get_current_iso8601_timestamp();

[[nodiscard]] nlohmann::json make_error_json(
    std::string_view code, std::string_view message,
    const nlohmann::json& details = nlohmann::json::array(), std::string_view timestamp = "");

[[nodiscard]] drogon::HttpResponsePtr make_error_response(
    drogon::HttpStatusCode status_code, std::string_view code, std::string_view message,
    const nlohmann::json& details = nlohmann::json::array(), std::string_view request_id = "");

}  // namespace nodepulse::utils
