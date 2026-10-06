#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

#include <drogon/HttpFilter.h>
#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>

#include <nodepulse/config/config.hpp>

namespace nodepulse::middleware {

struct RateLimitResult {
    bool allowed{true};
    uint64_t retry_after_seconds{0};
};

class RateLimiter {
  public:
    using ClockFunc = std::function<std::chrono::steady_clock::time_point()>;

    explicit RateLimiter(uint32_t requests_per_minute = 120, uint32_t burst_capacity = 30,
                         ClockFunc clock = nullptr);

    [[nodiscard]] RateLimitResult try_acquire(std::string_view client_ip);

    void set_config(const config::RateLimitConfig& config);
    void set_clock(ClockFunc clock);
    void reset();

    [[nodiscard]] size_t entry_count() const;
    size_t cleanup_stale_entries(std::chrono::steady_clock::time_point now);

    [[nodiscard]] uint32_t requests_per_minute() const;
    [[nodiscard]] uint32_t burst_capacity() const;

  private:
    struct TokenBucket {
        double tokens{0.0};
        std::chrono::steady_clock::time_point last_refill;
    };

    mutable std::mutex mutex_;
    uint32_t requests_per_minute_{120};
    uint32_t burst_capacity_{30};
    double refill_rate_{2.0};
    ClockFunc clock_;
    std::unordered_map<std::string, TokenBucket> buckets_;
    static constexpr size_t kMaxEntries = 10000;

    void update_refill_rate_locked();
    [[nodiscard]] std::chrono::steady_clock::time_point now_locked() const;
};

class RateLimitFilter : public drogon::HttpFilter<RateLimitFilter> {
  public:
    RateLimitFilter() = default;

    static void init(const config::RateLimitConfig& config, RateLimiter::ClockFunc clock = nullptr);
    static void set_enabled(bool enabled);
    [[nodiscard]] static bool is_enabled();
    static void reset();
    static void set_clock(RateLimiter::ClockFunc clock);
    [[nodiscard]] static RateLimiter& get_limiter();

    static void handle_request(const drogon::HttpRequestPtr& req, drogon::AdviceCallback&& acb,
                               drogon::AdviceChainCallback&& accb);

    void doFilter(const drogon::HttpRequestPtr& req, drogon::FilterCallback&& fcb,
                  drogon::FilterChainCallback&& fccb) override;

  private:
    static std::mutex mutex_;
    static bool enabled_;
    static std::unique_ptr<RateLimiter> limiter_;
};

}  // namespace nodepulse::middleware
