#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <nodepulse/domain/host_snapshot.hpp>
#include <nodepulse/repositories/postgres_repository.hpp>

namespace nodepulse::repositories {

class MockPostgresRepositoryTest : public ::testing::Test {
  protected:
    MockPostgresRepository repo_;

    domain::HostSnapshot create_sample_snapshot(const std::string& hostname,
                                                std::chrono::system_clock::time_point tp,
                                                std::optional<double> cpu = 25.5,
                                                std::optional<double> net_rx = 1024.0,
                                                std::optional<double> net_tx = 2048.0) {
        domain::HostSnapshot s;
        s.hostname = hostname;
        s.captured_at = tp;
        s.cpu_usage_percent = cpu;
        s.load_1m = 0.50;
        s.load_5m = 0.75;
        s.load_15m = 1.00;
        s.mem_total_bytes = 16000000000ULL;
        s.mem_used_bytes = 8000000000ULL;
        s.mem_available_bytes = 8000000000ULL;
        s.swap_total_bytes = 2000000000ULL;
        s.swap_used_bytes = 100000000ULL;
        s.net_rx_bytes_per_sec = net_rx;
        s.net_tx_bytes_per_sec = net_tx;
        return s;
    }
};

TEST_F(MockPostgresRepositoryTest, SchemaInitialization) {
    EXPECT_FALSE(repo_.is_schema_initialized());
    EXPECT_TRUE(repo_.init_schema());
    EXPECT_TRUE(repo_.is_schema_initialized());

    repo_.set_connected(false);
    EXPECT_FALSE(repo_.init_schema());
}

TEST_F(MockPostgresRepositoryTest, SaveSnapshotSuccess) {
    auto now = std::chrono::system_clock::now();
    auto s = create_sample_snapshot("node-1", now);

    EXPECT_TRUE(repo_.save_snapshot(s));
    EXPECT_EQ(repo_.record_count(), 1U);

    domain::HistoryQuery q;
    auto res = repo_.query_history(q);
    ASSERT_TRUE(res.has_value());
    ASSERT_EQ(res->size(), 1U);

    const auto& rec = (*res)[0];
    EXPECT_EQ(rec.id, 1);
    EXPECT_EQ(rec.hostname, "node-1");
    ASSERT_TRUE(rec.cpu_usage_percent.has_value());
    EXPECT_DOUBLE_EQ(*rec.cpu_usage_percent, 25.5);
    EXPECT_DOUBLE_EQ(rec.load_1m, 0.50);
    EXPECT_DOUBLE_EQ(rec.load_5m, 0.75);
    EXPECT_DOUBLE_EQ(rec.load_15m, 1.00);
    EXPECT_EQ(rec.mem_total_bytes, 16000000000ULL);
    EXPECT_EQ(rec.mem_used_bytes, 8000000000ULL);
    EXPECT_EQ(rec.mem_available_bytes, 8000000000ULL);
    EXPECT_EQ(rec.swap_total_bytes, 2000000000ULL);
    EXPECT_EQ(rec.swap_used_bytes, 100000000ULL);
    ASSERT_TRUE(rec.net_rx_bytes_per_sec.has_value());
    EXPECT_DOUBLE_EQ(*rec.net_rx_bytes_per_sec, 1024.0);
    ASSERT_TRUE(rec.net_tx_bytes_per_sec.has_value());
    EXPECT_DOUBLE_EQ(*rec.net_tx_bytes_per_sec, 2048.0);
}

TEST_F(MockPostgresRepositoryTest, SaveSnapshotNullCpuAndNetworkPreserved) {
    auto now = std::chrono::system_clock::now();
    auto s = create_sample_snapshot("node-2", now, std::nullopt, std::nullopt, std::nullopt);

    EXPECT_TRUE(repo_.save_snapshot(s));

    domain::HistoryQuery q;
    auto res = repo_.query_history(q);
    ASSERT_TRUE(res.has_value());
    ASSERT_EQ(res->size(), 1U);

    const auto& rec = (*res)[0];
    EXPECT_FALSE(rec.cpu_usage_percent.has_value());
    EXPECT_FALSE(rec.net_rx_bytes_per_sec.has_value());
    EXPECT_FALSE(rec.net_tx_bytes_per_sec.has_value());

    auto json = rec.to_json();
    EXPECT_TRUE(json["cpu_usage_percent"].is_null());
    EXPECT_TRUE(json["net_rx_bytes_per_sec"].is_null());
    EXPECT_TRUE(json["net_tx_bytes_per_sec"].is_null());
    EXPECT_EQ(json["hostname"], "node-2");
    EXPECT_TRUE(json.contains("captured_at"));
}

TEST_F(MockPostgresRepositoryTest, QueryHistoryDescendingOrder) {
    auto t0 = std::chrono::system_clock::now() - std::chrono::minutes(10);
    auto t1 = std::chrono::system_clock::now() - std::chrono::minutes(5);
    auto t2 = std::chrono::system_clock::now();

    EXPECT_TRUE(repo_.save_snapshot(create_sample_snapshot("node-1", t0)));
    EXPECT_TRUE(repo_.save_snapshot(create_sample_snapshot("node-1", t2)));
    EXPECT_TRUE(repo_.save_snapshot(create_sample_snapshot("node-1", t1)));

    domain::HistoryQuery q;
    auto res = repo_.query_history(q);
    ASSERT_TRUE(res.has_value());
    ASSERT_EQ(res->size(), 3U);

    // Must be strictly descending by captured_at
    EXPECT_GE((*res)[0].captured_at, (*res)[1].captured_at);
    EXPECT_GE((*res)[1].captured_at, (*res)[2].captured_at);
}

TEST_F(MockPostgresRepositoryTest, QueryHistoryFilterByHostname) {
    auto now = std::chrono::system_clock::now();
    EXPECT_TRUE(repo_.save_snapshot(create_sample_snapshot("alpha", now)));
    EXPECT_TRUE(repo_.save_snapshot(create_sample_snapshot("beta", now)));
    EXPECT_TRUE(repo_.save_snapshot(create_sample_snapshot("alpha", now)));

    domain::HistoryQuery q;
    q.hostname = "alpha";
    auto res = repo_.query_history(q);
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(res->size(), 2U);
    for (const auto& r : *res) {
        EXPECT_EQ(r.hostname, "alpha");
    }

    q.hostname = "nonexistent";
    auto empty_res = repo_.query_history(q);
    ASSERT_TRUE(empty_res.has_value());
    EXPECT_TRUE(empty_res->empty());
}

TEST_F(MockPostgresRepositoryTest, QueryHistoryFilterByTimeRange) {
    auto now = std::chrono::system_clock::now();
    auto t_old = now - std::chrono::hours(3);
    auto t_mid = now - std::chrono::hours(1);
    auto t_new = now;

    EXPECT_TRUE(repo_.save_snapshot(create_sample_snapshot("node-1", t_old)));
    EXPECT_TRUE(repo_.save_snapshot(create_sample_snapshot("node-1", t_mid)));
    EXPECT_TRUE(repo_.save_snapshot(create_sample_snapshot("node-1", t_new)));

    domain::HistoryQuery q;
    q.start_time = now - std::chrono::hours(2);
    q.end_time = now - std::chrono::minutes(30);

    auto res = repo_.query_history(q);
    ASSERT_TRUE(res.has_value());
    ASSERT_EQ(res->size(), 1U);
    EXPECT_EQ((*res)[0].captured_at, t_mid);
}

TEST_F(MockPostgresRepositoryTest, QueryHistoryLimitClamping) {
    auto now = std::chrono::system_clock::now();
    for (int i = 0; i < 10; ++i) {
        EXPECT_TRUE(
            repo_.save_snapshot(create_sample_snapshot("node-1", now + std::chrono::seconds(i))));
    }

    domain::HistoryQuery q;
    q.limit = 3;
    auto res = repo_.query_history(q);
    ASSERT_TRUE(res.has_value());
    EXPECT_EQ(res->size(), 3U);
}

TEST_F(MockPostgresRepositoryTest, DisconnectedBehavior) {
    repo_.set_connected(false);
    EXPECT_FALSE(repo_.is_connected());

    auto now = std::chrono::system_clock::now();
    EXPECT_FALSE(repo_.save_snapshot(create_sample_snapshot("node-1", now)));

    domain::HistoryQuery q;
    auto res = repo_.query_history(q);
    EXPECT_FALSE(res.has_value());
}

TEST_F(MockPostgresRepositoryTest, ConcurrentInsertAndQuery) {
    const int num_writers = 4;
    const int writes_per_thread = 50;
    std::vector<std::thread> threads;

    for (int t = 0; t < num_writers; ++t) {
        threads.emplace_back([this, t, writes_per_thread]() {
            for (int i = 0; i < writes_per_thread; ++i) {
                auto now = std::chrono::system_clock::now();
                repo_.save_snapshot(create_sample_snapshot("node-" + std::to_string(t), now));
                if (i % 10 == 0) {
                    domain::HistoryQuery q;
                    q.limit = 10;
                    auto res = repo_.query_history(q);
                    EXPECT_TRUE(res.has_value());
                }
            }
        });
    }

    for (auto& th : threads) {
        th.join();
    }

    EXPECT_EQ(repo_.record_count(), static_cast<size_t>(num_writers * writes_per_thread));
}

// -----------------------------------------------------------------------------
// Real PostgresRepository offline resilience tests
// -----------------------------------------------------------------------------

TEST(PostgresRepositoryTest, RedactsConnectionStringInConstructor) {
    PostgresRepository repo("postgresql://nodepulse:mysecretpass@127.0.0.1:5432/nodepulse_db");
    EXPECT_EQ(repo.redacted_connection_string(),
              "postgresql://nodepulse:***@127.0.0.1:5432/nodepulse_db");
}

TEST(PostgresRepositoryTest, OfflineDatabaseHandledGracefullyWithoutCrash) {
    // Unreachable loopback port with short connect timeout to test failure modes
    PostgresRepository repo("postgresql://user:pass@127.0.0.1:59999/dummy?connect_timeout=1");

    EXPECT_FALSE(repo.is_connected());
    EXPECT_FALSE(repo.init_schema());

    domain::HostSnapshot s;
    s.hostname = "offline-node";
    s.captured_at = std::chrono::system_clock::now();
    EXPECT_FALSE(repo.save_snapshot(s));

    domain::HistoryQuery q;
    auto res = repo.query_history(q);
    EXPECT_FALSE(res.has_value());
}

}  // namespace nodepulse::repositories
