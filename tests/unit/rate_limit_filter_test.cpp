#include <chrono>
#include <future>
#include <string>
#include <vector>

#include <drogon/HttpRequest.h>
#include <drogon/HttpResponse.h>
#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <nodepulse/config/config.hpp>
#include <nodepulse/middleware/rate_limit_filter.hpp>
#include <nodepulse/utils/error_response.hpp>

namespace nodepulse::middleware {

class RateLimiterTest : public ::testing::Test {
  protected:
    std::chrono::steady_clock::time_point mock_time_{std::chrono::steady_clock::now()};

    RateLimiter::ClockFunc get_clock() {
        return [this]() { return mock_time_; };
    }
};

TEST_F(RateLimiterTest, RequestsBelowLimitSucceed) {
    RateLimiter limiter(120, 10, get_clock());
    const std::string ip = "192.168.1.50";

    for (int i = 0; i < 10; ++i) {
        auto result = limiter.try_acquire(ip);
        EXPECT_TRUE(result.allowed) << "Request " << i << " failed unexpectedly";
        EXPECT_EQ(result.retry_after_seconds, 0U);
    }
}

TEST_F(RateLimiterTest, ExactBoundaryAndImmediateRejection) {
    RateLimiter limiter(60, 3, get_clock());
    const std::string ip = "10.0.0.1";

    // 3 requests allowed (burst capacity)
    EXPECT_TRUE(limiter.try_acquire(ip).allowed);
    EXPECT_TRUE(limiter.try_acquire(ip).allowed);
    EXPECT_TRUE(limiter.try_acquire(ip).allowed);

    // 4th and 5th requests immediately rejected
    auto res4 = limiter.try_acquire(ip);
    EXPECT_FALSE(res4.allowed);
    EXPECT_GE(res4.retry_after_seconds, 1U);

    auto res5 = limiter.try_acquire(ip);
    EXPECT_FALSE(res5.allowed);
    EXPECT_GE(res5.retry_after_seconds, 1U);
}

TEST_F(RateLimiterTest, RefillWindowRecovery) {
    // 60 req/min = 1.0 token/second
    RateLimiter limiter(60, 2, get_clock());
    const std::string ip = "10.0.0.2";

    EXPECT_TRUE(limiter.try_acquire(ip).allowed);
    EXPECT_TRUE(limiter.try_acquire(ip).allowed);
    EXPECT_FALSE(limiter.try_acquire(ip).allowed);

    // Advance clock by 1 second (refills 1 token)
    mock_time_ += std::chrono::seconds(1);

    // One request should succeed
    EXPECT_TRUE(limiter.try_acquire(ip).allowed);
    // Next request should fail
    EXPECT_FALSE(limiter.try_acquire(ip).allowed);
}

TEST_F(RateLimiterTest, FullRefillRestoresCapacity) {
    // 120 req/min = 2.0 tokens/second, burst capacity 4 (full refill takes 2 seconds)
    RateLimiter limiter(120, 4, get_clock());
    const std::string ip = "10.0.0.3";

    for (int i = 0; i < 4; ++i) {
        EXPECT_TRUE(limiter.try_acquire(ip).allowed);
    }
    EXPECT_FALSE(limiter.try_acquire(ip).allowed);

    // Advance clock by 10 seconds (well past full refill time)
    mock_time_ += std::chrono::seconds(10);

    // All 4 burst tokens must be restored
    for (int i = 0; i < 4; ++i) {
        EXPECT_TRUE(limiter.try_acquire(ip).allowed)
            << "Burst request " << i << " failed after full refill";
    }
    EXPECT_FALSE(limiter.try_acquire(ip).allowed);
}

TEST_F(RateLimiterTest, IndependentClients) {
    RateLimiter limiter(60, 2, get_clock());
    const std::string client_a = "192.168.1.10";
    const std::string client_b = "192.168.1.20";

    // Client A consumes both tokens
    EXPECT_TRUE(limiter.try_acquire(client_a).allowed);
    EXPECT_TRUE(limiter.try_acquire(client_a).allowed);
    EXPECT_FALSE(limiter.try_acquire(client_a).allowed);

    // Client B must still have full burst capacity
    EXPECT_TRUE(limiter.try_acquire(client_b).allowed);
    EXPECT_TRUE(limiter.try_acquire(client_b).allowed);
    EXPECT_FALSE(limiter.try_acquire(client_b).allowed);
}

TEST_F(RateLimiterTest, StateCleanupStaleEntries) {
    // 60 req/min = 1 token/sec, burst 2 -> refill time is 2 seconds
    RateLimiter limiter(60, 2, get_clock());
    EXPECT_TRUE(limiter.try_acquire("10.0.0.1").allowed);
    EXPECT_TRUE(limiter.try_acquire("10.0.0.2").allowed);
    EXPECT_EQ(limiter.entry_count(), 2U);

    // Not stale yet
    EXPECT_EQ(limiter.cleanup_stale_entries(mock_time_), 0U);
    EXPECT_EQ(limiter.entry_count(), 2U);

    // Advance past refill duration (10 seconds)
    mock_time_ += std::chrono::seconds(10);
    EXPECT_EQ(limiter.cleanup_stale_entries(mock_time_), 2U);
    EXPECT_EQ(limiter.entry_count(), 0U);
}

TEST_F(RateLimiterTest, ResetClearsAllEntries) {
    RateLimiter limiter(60, 5, get_clock());
    EXPECT_TRUE(limiter.try_acquire("10.0.0.1").allowed);
    EXPECT_TRUE(limiter.try_acquire("10.0.0.2").allowed);
    EXPECT_EQ(limiter.entry_count(), 2U);

    limiter.reset();
    EXPECT_EQ(limiter.entry_count(), 0U);
}

TEST_F(RateLimiterTest, ConcurrentAccessNoDataRaces) {
    RateLimiter limiter(600, 100, get_clock());
    constexpr int kNumThreads = 16;
    constexpr int kRequestsPerThread = 50;

    std::vector<std::future<int>> futures;
    for (int t = 0; t < kNumThreads; ++t) {
        futures.push_back(std::async(std::launch::async, [&limiter, t]() {
            int allowed_count = 0;
            // Distribute requests across 4 client IPs
            std::string ip = "192.168.1." + std::to_string(t % 4);
            for (int r = 0; r < kRequestsPerThread; ++r) {
                if (limiter.try_acquire(ip).allowed) {
                    ++allowed_count;
                }
            }
            return allowed_count;
        }));
    }

    int total_allowed = 0;
    for (auto& f : futures) {
        total_allowed += f.get();
    }

    // Each of the 4 IPs has burst capacity 100, so at most 4 * 100 = 400 allowed without refill
    EXPECT_LE(total_allowed, 400);
    EXPECT_GT(total_allowed, 0);
}

class RateLimitFilterTest : public ::testing::Test {
  protected:
    std::chrono::steady_clock::time_point mock_time_{std::chrono::steady_clock::now()};

    void SetUp() override {
        config::RateLimitConfig cfg;
        cfg.enabled = true;
        cfg.requests_per_minute = 60;
        cfg.burst_capacity = 2;
        RateLimitFilter::init(cfg, [this]() { return mock_time_; });
    }

    void TearDown() override {
        RateLimitFilter::reset();
        RateLimitFilter::set_enabled(true);
    }
};

TEST_F(RateLimitFilterTest, DisabledFilterPassesAllRequests) {
    RateLimitFilter::set_enabled(false);
    auto req = drogon::HttpRequest::newHttpRequest();

    for (int i = 0; i < 10; ++i) {
        bool chain_called = false;
        bool advice_called = false;
        RateLimitFilter::handle_request(
            req, [&advice_called](const drogon::HttpResponsePtr&) { advice_called = true; },
            [&chain_called]() { chain_called = true; });
        EXPECT_TRUE(chain_called);
        EXPECT_FALSE(advice_called);
    }
}

TEST_F(RateLimitFilterTest, FilterPassesUnderLimit) {
    auto req = drogon::HttpRequest::newHttpRequest();

    bool chain_called = false;
    bool advice_called = false;
    RateLimitFilter::handle_request(
        req, [&advice_called](const drogon::HttpResponsePtr&) { advice_called = true; },
        [&chain_called]() { chain_called = true; });

    EXPECT_TRUE(chain_called);
    EXPECT_FALSE(advice_called);
}

TEST_F(RateLimitFilterTest, FilterRejectsOverLimitWith429Envelope) {
    auto req = drogon::HttpRequest::newHttpRequest();
    req->getAttributes()->insert("request_id", std::string("test-req-id-rl-123"));

    // Burst capacity is 2
    for (int i = 0; i < 2; ++i) {
        bool chain_called = false;
        RateLimitFilter::handle_request(
            req, [](const drogon::HttpResponsePtr&) {}, [&chain_called]() { chain_called = true; });
        EXPECT_TRUE(chain_called) << "Request " << i << " was not allowed";
    }

    // 3rd request must be rejected with 429
    bool chain_called = false;
    drogon::HttpResponsePtr intercepted_resp;
    RateLimitFilter::handle_request(
        req, [&intercepted_resp](const drogon::HttpResponsePtr& resp) { intercepted_resp = resp; },
        [&chain_called]() { chain_called = true; });

    EXPECT_FALSE(chain_called);
    ASSERT_NE(intercepted_resp, nullptr);
    EXPECT_EQ(intercepted_resp->statusCode(), drogon::k429TooManyRequests);
    EXPECT_EQ(intercepted_resp->contentType(), drogon::CT_APPLICATION_JSON);
    EXPECT_EQ(intercepted_resp->getHeader("X-Request-ID"), "test-req-id-rl-123");
    EXPECT_FALSE(intercepted_resp->getHeader("Retry-After").empty());

    auto json = nlohmann::json::parse(intercepted_resp->getBody());
    ASSERT_TRUE(json.contains("error"));
    EXPECT_EQ(json["error"]["code"], "RATE_LIMITED");
    EXPECT_EQ(json["error"]["message"], "Too many requests. Please slow down.");
    EXPECT_FALSE(json["error"]["timestamp"].get<std::string>().empty());
    ASSERT_TRUE(json["error"]["details"].is_array());
    ASSERT_FALSE(json["error"]["details"].empty());
    EXPECT_TRUE(json["error"]["details"][0].contains("retry_after_seconds"));
    EXPECT_GE(json["error"]["details"][0]["retry_after_seconds"].get<uint64_t>(), 1U);
}

TEST_F(RateLimitFilterTest, FilterRecoversAfterTimeAdvance) {
    auto req = drogon::HttpRequest::newHttpRequest();

    // Consume 2 tokens
    for (int i = 0; i < 2; ++i) {
        RateLimitFilter::handle_request(req, [](const drogon::HttpResponsePtr&) {}, []() {});
    }

    // 3rd request rejected
    bool rejected = false;
    RateLimitFilter::handle_request(
        req, [&rejected](const drogon::HttpResponsePtr&) { rejected = true; }, []() {});
    EXPECT_TRUE(rejected);

    // Advance mock time by 1 second (refills 1 token at 60 req/min)
    mock_time_ += std::chrono::seconds(1);

    // 4th request must now succeed
    bool chain_called = false;
    RateLimitFilter::handle_request(
        req, [](const drogon::HttpResponsePtr&) {}, [&chain_called]() { chain_called = true; });
    EXPECT_TRUE(chain_called);
}

}  // namespace nodepulse::middleware
