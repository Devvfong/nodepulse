#include <mutex>
#include <string>
#include <string_view>

#include <drogon/HttpResponse.h>
#include <nlohmann/json.hpp>

#include <nodepulse/middleware/auth_filter.hpp>
#include <nodepulse/utils/error_response.hpp>
#include <nodepulse/utils/security.hpp>

namespace nodepulse::middleware {

void AuthFilter::set_api_key(std::string key) {
    std::lock_guard<std::mutex> lock(mutex_);
    api_key_ = std::move(key);
}

void AuthFilter::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    api_key_.clear();
    metrics_require_auth_ = true;
}

void AuthFilter::set_metrics_require_auth(bool require_auth) noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    metrics_require_auth_ = require_auth;
}

bool AuthFilter::metrics_require_auth() noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return metrics_require_auth_;
}

bool AuthFilter::is_exempt_path(std::string_view path) noexcept {
    if (path == "/api/v1/health" || path == "/api/v1/health/") {
        return true;
    }
    if (path.starts_with("/api/v1/health?")) {
        return true;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    if (!metrics_require_auth_) {
        if (path == "/metrics" || path == "/metrics/") {
            return true;
        }
        if (path.starts_with("/metrics?")) {
            return true;
        }
    }
    return false;
}

bool AuthFilter::authenticate(const drogon::HttpRequestPtr& req) noexcept {
    if (!req) {
        return false;
    }

    std::string expected_key;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        expected_key = api_key_;
    }

    if (expected_key.empty()) {
        return false;
    }

    std::string candidate = req->getHeader("X-API-Key");
    if (candidate.empty()) {
        candidate = req->getHeader("x-api-key");
    }

    return utils::constant_time_equals(expected_key, candidate);
}

void AuthFilter::handle_request(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& acb,
                                drogon::AdviceChainCallback&& accb) {
    if (!req) {
        accb();
        return;
    }

    // Health check endpoint is explicitly exempt per BR-001
    if (is_exempt_path(req->path())) {
        accb();
        return;
    }

    // Bypass if already validated by an upstream filter
    if (req->getAttributes() && req->getAttributes()->find("is_authenticated")) {
        if (req->getAttributes()->get<bool>("is_authenticated")) {
            accb();
            return;
        }
    }

    if (authenticate(req)) {
        if (req->getAttributes()) {
            req->getAttributes()->insert("is_authenticated", true);
        }
        accb();
        return;
    }

    // Authentication failure: return standard 401 UNAUTHORIZED envelope
    std::string req_id;
    if (req->getAttributes()) {
        req_id = req->getAttributes()->get<std::string>("request_id");
    }

    auto resp =
        utils::make_error_response(drogon::k401Unauthorized, utils::error_codes::kUnauthorized,
                                   "Authentication required. Provide a valid X-API-Key header.",
                                   nlohmann::json::array(), req_id);

    acb(resp);
}

void AuthFilter::doFilter(const drogon::HttpRequestPtr& req, drogon::FilterCallback&& fcb,
                          drogon::FilterChainCallback&& fccb) {
    handle_request(req, std::move(fcb), std::move(fccb));
}

}  // namespace nodepulse::middleware
