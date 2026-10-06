#pragma once

#include <mutex>
#include <string>
#include <string_view>

#include <drogon/HttpFilter.h>
#include <drogon/drogon_callbacks.h>

namespace nodepulse::middleware {

/**
 * @brief Authentication filter enforcing header-based API key validation (X-API-Key).
 *
 * Intercepts incoming HTTP requests, checks for path exemption (e.g. /api/v1/health),
 * and validates the client's X-API-Key header against the configured secret key
 * using constant-time comparison to prevent timing side-channel attacks.
 */
class AuthFilter : public drogon::HttpFilter<AuthFilter> {
  public:
    AuthFilter() = default;
    ~AuthFilter() override = default;

    /**
     * @brief Drogon filter execution hook.
     */
    void doFilter(const drogon::HttpRequestPtr& req, drogon::FilterCallback&& fcb,
                  drogon::FilterChainCallback&& fccb) override;

    /**
     * @brief Configure the expected API key secret.
     */
    static void set_api_key(std::string key);

    /**
     * @brief Determine whether a given URL path is exempt from authentication.
     *
     * /api/v1/health is strictly exempt per BR-001.
     */
    [[nodiscard]] static bool is_exempt_path(std::string_view path) noexcept;

    /**
     * @brief Check whether a request carries a valid X-API-Key header.
     */
    [[nodiscard]] static bool authenticate(const drogon::HttpRequestPtr& req) noexcept;

    /**
     * @brief Central request interceptor called by Drogon AOP post-routing advice.
     */
    static void handle_request(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& acb,
                               drogon::AdviceChainCallback&& accb);

    /**
     * @brief Reset authentication configuration (primarily for unit testing).
     */
    static void reset();

  private:
    static inline std::string api_key_;
    static inline std::mutex mutex_;
};

}  // namespace nodepulse::middleware
