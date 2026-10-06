#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <string_view>
#include <unordered_map>

#include <drogon/HttpResponse.h>
#include <nlohmann/json.hpp>

#include <nodepulse/middleware/rate_limit_filter.hpp>
#include <nodepulse/utils/error_response.hpp>

namespace nodepulse::middleware {

RateLimiter::RateLimiter(uint32_t requests_per_minute, uint32_t burst_capacity, ClockFunc clock)
    : requests_per_minute_(requests_per_minute),
      burst_capacity_(std::max(1U, burst_capacity)),
      clock_(std::move(clock)) {
    update_refill_rate_locked();
}

void RateLimiter::update_refill_rate_locked() {
    if (requests_per_minute_ > 0) {
        refill_rate_ = static_cast<double>(requests_per_minute_) / 60.0;
    } else {
        refill_rate_ = 0.0;
    }
}

std::chrono::steady_clock::time_point RateLimiter::now_locked() const {
    if (clock_) {
        return clock_();
    }
    return std::chrono::steady_clock::now();
}

uint32_t RateLimiter::requests_per_minute() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return requests_per_minute_;
}

uint32_t RateLimiter::burst_capacity() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return burst_capacity_;
}

void RateLimiter::set_config(const config::RateLimitConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    requests_per_minute_ = config.requests_per_minute;
    burst_capacity_ = std::max(1U, config.burst_capacity);
    update_refill_rate_locked();
}

void RateLimiter::set_clock(ClockFunc clock) {
    std::lock_guard<std::mutex> lock(mutex_);
    clock_ = std::move(clock);
}

void RateLimiter::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    buckets_.clear();
}

size_t RateLimiter::entry_count() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return buckets_.size();
}

size_t RateLimiter::cleanup_stale_entries(std::chrono::steady_clock::time_point now) {
    const double max_refill_seconds =
        (refill_rate_ > 0.0) ? (static_cast<double>(burst_capacity_) / refill_rate_) : 60.0;
    size_t count = 0;
    for (auto it = buckets_.begin(); it != buckets_.end();) {
        double elapsed = std::chrono::duration<double>(now - it->second.last_refill).count();
        if (elapsed >= max_refill_seconds) {
            it = buckets_.erase(it);
            ++count;
        } else {
            ++it;
        }
    }
    return count;
}

RateLimitResult RateLimiter::try_acquire(std::string_view client_ip) {
    std::lock_guard<std::mutex> lock(mutex_);
    const auto now = now_locked();

    auto it = buckets_.find(std::string(client_ip));
    if (it == buckets_.end()) {
        if (buckets_.size() >= kMaxEntries) {
            cleanup_stale_entries(now);
            if (buckets_.size() >= kMaxEntries) {
                // Evict oldest entry to guarantee bounded memory
                auto oldest_it = buckets_.begin();
                for (auto scan_it = buckets_.begin(); scan_it != buckets_.end(); ++scan_it) {
                    if (scan_it->second.last_refill < oldest_it->second.last_refill) {
                        oldest_it = scan_it;
                    }
                }
                buckets_.erase(oldest_it);
            }
        }

        TokenBucket bucket;
        bucket.tokens = static_cast<double>(burst_capacity_) - 1.0;
        bucket.last_refill = now;
        buckets_.emplace(std::string(client_ip), bucket);
        return {true, 0};
    }

    auto& bucket = it->second;
    double elapsed = std::chrono::duration<double>(now - bucket.last_refill).count();
    if (elapsed > 0.0) {
        bucket.tokens =
            std::min(static_cast<double>(burst_capacity_), bucket.tokens + elapsed * refill_rate_);
        bucket.last_refill = now;
    }

    if (bucket.tokens >= 1.0) {
        bucket.tokens -= 1.0;
        return {true, 0};
    }

    double deficit = 1.0 - bucket.tokens;
    double wait_seconds = (refill_rate_ > 0.0) ? (deficit / refill_rate_) : 1.0;
    uint64_t retry_after = std::max<uint64_t>(1ULL, static_cast<uint64_t>(std::ceil(wait_seconds)));
    return {false, retry_after};
}

std::mutex RateLimitFilter::mutex_;
bool RateLimitFilter::enabled_{true};
std::unique_ptr<RateLimiter> RateLimitFilter::limiter_;

void RateLimitFilter::init(const config::RateLimitConfig& config, RateLimiter::ClockFunc clock) {
    std::lock_guard<std::mutex> lock(mutex_);
    enabled_ = config.enabled;
    limiter_ = std::make_unique<RateLimiter>(config.requests_per_minute, config.burst_capacity,
                                             std::move(clock));
}

void RateLimitFilter::set_enabled(bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);
    enabled_ = enabled;
}

bool RateLimitFilter::is_enabled() {
    std::lock_guard<std::mutex> lock(mutex_);
    return enabled_;
}

void RateLimitFilter::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (limiter_) {
        limiter_->reset();
    }
}

void RateLimitFilter::set_clock(RateLimiter::ClockFunc clock) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!limiter_) {
        limiter_ = std::make_unique<RateLimiter>();
    }
    limiter_->set_clock(std::move(clock));
}

RateLimiter& RateLimitFilter::get_limiter() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!limiter_) {
        limiter_ = std::make_unique<RateLimiter>();
    }
    return *limiter_;
}

void RateLimitFilter::handle_request(const drogon::HttpRequestPtr& req,
                                     drogon::AdviceCallback&& acb,
                                     drogon::AdviceChainCallback&& accb) {
    if (!req) {
        accb();
        return;
    }

    bool enabled = false;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        enabled = enabled_;
    }

    if (!enabled) {
        accb();
        return;
    }

    std::string client_ip = req->peerAddr().toIp();
    if (client_ip.empty()) {
        client_ip = "127.0.0.1";
    }

    RateLimitResult result;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!limiter_) {
            limiter_ = std::make_unique<RateLimiter>();
        }
        result = limiter_->try_acquire(client_ip);
    }

    if (result.allowed) {
        accb();
        return;
    }

    std::string req_id;
    if (req->getAttributes()) {
        req_id = req->getAttributes()->get<std::string>("request_id");
    }

    nlohmann::json details =
        nlohmann::json::array({{{"retry_after_seconds", result.retry_after_seconds}}});

    auto resp =
        utils::make_error_response(drogon::k429TooManyRequests, utils::error_codes::kRateLimited,
                                   "Too many requests. Please slow down.", details, req_id);

    resp->addHeader("Retry-After", std::to_string(result.retry_after_seconds));
    acb(resp);
}

void RateLimitFilter::doFilter(const drogon::HttpRequestPtr& req, drogon::FilterCallback&& fcb,
                               drogon::FilterChainCallback&& fccb) {
    handle_request(req, std::move(fcb), std::move(fccb));
}

}  // namespace nodepulse::middleware
