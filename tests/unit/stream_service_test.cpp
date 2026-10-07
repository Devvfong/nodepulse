#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <nodepulse/collectors/cpu_collector.hpp>
#include <nodepulse/collectors/memory_collector.hpp>
#include <nodepulse/collectors/network_collector.hpp>
#include <nodepulse/domain/metric_pulse.hpp>
#include <nodepulse/services/cpu_service.hpp>
#include <nodepulse/services/memory_service.hpp>
#include <nodepulse/services/network_service.hpp>
#include <nodepulse/services/stream_service.hpp>

namespace {

class MockSseClient : public nodepulse::services::ISseClient {
  public:
    explicit MockSseClient(bool succeed = true) : return_success(succeed) {}

    bool send_data(const std::string& chunk) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!alive || !return_success) {
            alive = false;
            return false;
        }
        sent_chunks.push_back(chunk);
        return true;
    }

    void close() override {
        std::lock_guard<std::mutex> lock(mutex_);
        alive = false;
        closed = true;
    }

    [[nodiscard]] bool is_alive() const override {
        std::lock_guard<std::mutex> lock(mutex_);
        return alive;
    }

    bool return_success{true};
    bool alive{true};
    bool closed{false};
    std::vector<std::string> sent_chunks;

  private:
    mutable std::mutex mutex_;
};

}  // namespace

// =============================================================================
// MetricPulse Domain Model & JSON Tests
// =============================================================================

TEST(StreamServiceTest, MetricPulseJsonSerializationPreservesNullsDuringWarmup) {
    nodepulse::domain::MetricPulse pulse;
    pulse.timestamp = "2026-10-06T15:30:00Z";
    pulse.cpu_usage_percent = std::nullopt;  // warming up
    pulse.memory_usage_percent = 45.5;
    pulse.memory_used_bytes = 4000000000ULL;
    pulse.network_rx_bytes_sec = std::nullopt;  // warming up
    pulse.network_tx_bytes_sec = std::nullopt;  // warming up

    nlohmann::json j = pulse;
    EXPECT_EQ(j["timestamp"], "2026-10-06T15:30:00Z");
    EXPECT_TRUE(j["cpu_usage_percent"].is_null());
    EXPECT_DOUBLE_EQ(j["memory_usage_percent"].get<double>(), 45.5);
    EXPECT_EQ(j["memory_used_bytes"].get<uint64_t>(), 4000000000ULL);
    EXPECT_TRUE(j["network_rx_bytes_sec"].is_null());
    EXPECT_TRUE(j["network_tx_bytes_sec"].is_null());
}

TEST(StreamServiceTest, MetricPulseJsonSerializationWithValues) {
    nodepulse::domain::MetricPulse pulse;
    pulse.timestamp = "2026-10-06T15:30:00Z";
    pulse.cpu_usage_percent = 25.8;
    pulse.memory_usage_percent = 60.2;
    pulse.memory_used_bytes = 8000000000ULL;
    pulse.network_rx_bytes_sec = 1048576.0;
    pulse.network_tx_bytes_sec = 524288.0;

    nlohmann::json j = pulse;
    EXPECT_EQ(j["timestamp"], "2026-10-06T15:30:00Z");
    ASSERT_TRUE(j["cpu_usage_percent"].is_number());
    EXPECT_DOUBLE_EQ(j["cpu_usage_percent"].get<double>(), 25.8);
    EXPECT_DOUBLE_EQ(j["memory_usage_percent"].get<double>(), 60.2);
    EXPECT_EQ(j["memory_used_bytes"].get<uint64_t>(), 8000000000ULL);
    ASSERT_TRUE(j["network_rx_bytes_sec"].is_number());
    EXPECT_DOUBLE_EQ(j["network_rx_bytes_sec"].get<double>(), 1048576.0);
    ASSERT_TRUE(j["network_tx_bytes_sec"].is_number());
    EXPECT_DOUBLE_EQ(j["network_tx_bytes_sec"].get<double>(), 524288.0);

    // Round-trip deserialization
    auto deserialized = j.get<nodepulse::domain::MetricPulse>();
    EXPECT_EQ(deserialized.timestamp, pulse.timestamp);
    ASSERT_TRUE(deserialized.cpu_usage_percent.has_value());
    EXPECT_DOUBLE_EQ(*deserialized.cpu_usage_percent, 25.8);
    EXPECT_DOUBLE_EQ(deserialized.memory_usage_percent, 60.2);
    EXPECT_EQ(deserialized.memory_used_bytes, 8000000000ULL);
    ASSERT_TRUE(deserialized.network_rx_bytes_sec.has_value());
    EXPECT_DOUBLE_EQ(*deserialized.network_rx_bytes_sec, 1048576.0);
    ASSERT_TRUE(deserialized.network_tx_bytes_sec.has_value());
    EXPECT_DOUBLE_EQ(*deserialized.network_tx_bytes_sec, 524288.0);
}

// =============================================================================
// SSE Framing Tests
// =============================================================================

TEST(StreamServiceTest, FormatPulseFrameProducesValidSseFraming) {
    nodepulse::domain::MetricPulse pulse;
    pulse.timestamp = "2026-10-06T12:00:00Z";
    pulse.cpu_usage_percent = 12.5;
    pulse.memory_usage_percent = 30.0;
    pulse.memory_used_bytes = 2048ULL;
    pulse.network_rx_bytes_sec = 100.0;
    pulse.network_tx_bytes_sec = 200.0;

    std::string frame = nodepulse::services::StreamService::format_pulse_frame(42, pulse);

    // Must start with id, followed by event, followed by data, ending in \n\n
    EXPECT_NE(frame.find("id: 42\n"), std::string::npos);
    EXPECT_NE(frame.find("event: metric_pulse\n"), std::string::npos);
    EXPECT_NE(frame.find("data: {"), std::string::npos);
    EXPECT_TRUE(frame.ends_with("\n\n"));

    // Extract the json data line
    auto data_pos = frame.find("data: ");
    ASSERT_NE(data_pos, std::string::npos);
    auto data_end = frame.find("\n\n", data_pos);
    ASSERT_NE(data_end, std::string::npos);
    std::string json_str = frame.substr(data_pos + 6, data_end - (data_pos + 6));

    auto j = nlohmann::json::parse(json_str);
    EXPECT_EQ(j["timestamp"], "2026-10-06T12:00:00Z");
    EXPECT_DOUBLE_EQ(j["cpu_usage_percent"].get<double>(), 12.5);
    EXPECT_DOUBLE_EQ(j["memory_usage_percent"].get<double>(), 30.0);
    EXPECT_EQ(j["memory_used_bytes"].get<uint64_t>(), 2048ULL);
    EXPECT_DOUBLE_EQ(j["network_rx_bytes_sec"].get<double>(), 100.0);
    EXPECT_DOUBLE_EQ(j["network_tx_bytes_sec"].get<double>(), 200.0);
}

TEST(StreamServiceTest, FormatHeartbeatFrameProducesComment) {
    std::string frame = nodepulse::services::StreamService::format_heartbeat_frame();
    EXPECT_EQ(frame, ": keepalive\n\n");
}

// =============================================================================
// StreamService Subscriber Management & Broadcast Tests
// =============================================================================

TEST(StreamServiceTest, AddClientSendsInitialPulseAndIncrementsCount) {
    nodepulse::config::SseConfig cfg;
    cfg.enabled = true;
    cfg.interval_ms = 1000;

    nodepulse::services::StreamService stream_svc(nullptr, nullptr, nullptr, cfg);
    EXPECT_EQ(stream_svc.client_count(), 0U);

    auto client = std::make_shared<MockSseClient>();
    stream_svc.add_client(client);

    EXPECT_EQ(stream_svc.client_count(), 1U);
    ASSERT_EQ(client->sent_chunks.size(), 1U);
    EXPECT_NE(client->sent_chunks[0].find("event: metric_pulse\n"), std::string::npos);
    EXPECT_NE(client->sent_chunks[0].find("id: 1\n"), std::string::npos);
}

TEST(StreamServiceTest, BroadcastPulseTransmitsToAllActiveSubscribers) {
    nodepulse::config::SseConfig cfg;
    cfg.enabled = true;
    cfg.interval_ms = 1000;

    nodepulse::services::StreamService stream_svc(nullptr, nullptr, nullptr, cfg);

    auto client1 = std::make_shared<MockSseClient>();
    auto client2 = std::make_shared<MockSseClient>();
    stream_svc.add_client(client1);
    stream_svc.add_client(client2);

    EXPECT_EQ(stream_svc.client_count(), 2U);
    EXPECT_EQ(client1->sent_chunks.size(), 1U);  // Initial pulse
    EXPECT_EQ(client2->sent_chunks.size(), 1U);  // Initial pulse

    size_t broadcast_count = stream_svc.broadcast_pulse();
    EXPECT_EQ(broadcast_count, 2U);
    EXPECT_EQ(client1->sent_chunks.size(), 2U);
    EXPECT_EQ(client2->sent_chunks.size(), 2U);

    // Sequence ID increments monotonically
    EXPECT_NE(client1->sent_chunks[1].find("id: 3\n"), std::string::npos);
    EXPECT_NE(client2->sent_chunks[1].find("id: 3\n"), std::string::npos);
}

TEST(StreamServiceTest, DisconnectedSubscriberIsPromptlyRemovedAndClosed) {
    nodepulse::config::SseConfig cfg;
    cfg.enabled = true;
    cfg.interval_ms = 1000;

    nodepulse::services::StreamService stream_svc(nullptr, nullptr, nullptr, cfg);

    auto live_client = std::make_shared<MockSseClient>(true);
    auto flaky_client = std::make_shared<MockSseClient>(true);

    stream_svc.add_client(live_client);
    stream_svc.add_client(flaky_client);
    EXPECT_EQ(stream_svc.client_count(), 2U);

    // Simulate connection drop for flaky_client
    flaky_client->return_success = false;

    size_t success_count = stream_svc.broadcast_pulse();
    EXPECT_EQ(success_count, 1U);
    EXPECT_EQ(stream_svc.client_count(), 1U);
    EXPECT_TRUE(flaky_client->closed);
    EXPECT_FALSE(flaky_client->is_alive());
    EXPECT_TRUE(live_client->is_alive());
}

TEST(StreamServiceTest, SendHeartbeatTransmitsKeepaliveCommentToAllSubscribers) {
    nodepulse::config::SseConfig cfg;
    cfg.enabled = true;
    cfg.interval_ms = 1000;

    nodepulse::services::StreamService stream_svc(nullptr, nullptr, nullptr, cfg);

    auto client = std::make_shared<MockSseClient>();
    stream_svc.add_client(client);
    EXPECT_EQ(client->sent_chunks.size(), 1U);  // Initial pulse

    size_t sent = stream_svc.send_heartbeat();
    EXPECT_EQ(sent, 1U);
    ASSERT_EQ(client->sent_chunks.size(), 2U);
    EXPECT_EQ(client->sent_chunks[1], ": keepalive\n\n");
}

TEST(StreamServiceTest, RemoveClientExplicitlyUnregistersAndClosesSubscriber) {
    nodepulse::config::SseConfig cfg;
    cfg.enabled = true;
    cfg.interval_ms = 1000;

    nodepulse::services::StreamService stream_svc(nullptr, nullptr, nullptr, cfg);

    auto client = std::make_shared<MockSseClient>();
    stream_svc.add_client(client);
    EXPECT_EQ(stream_svc.client_count(), 1U);

    stream_svc.remove_client(client);
    EXPECT_EQ(stream_svc.client_count(), 0U);
    EXPECT_TRUE(client->closed);
}

TEST(StreamServiceTest, StopStreamingGracefullyClosesAllSubscribers) {
    nodepulse::config::SseConfig cfg;
    cfg.enabled = true;
    cfg.interval_ms = 1000;

    nodepulse::services::StreamService stream_svc(nullptr, nullptr, nullptr, cfg);

    auto client1 = std::make_shared<MockSseClient>();
    auto client2 = std::make_shared<MockSseClient>();
    stream_svc.add_client(client1);
    stream_svc.add_client(client2);
    EXPECT_EQ(stream_svc.client_count(), 2U);

    stream_svc.stop_streaming();
    EXPECT_EQ(stream_svc.client_count(), 0U);
    EXPECT_TRUE(client1->closed);
    EXPECT_TRUE(client2->closed);
}

TEST(StreamServiceTest, DisabledSseRejectsClientsAndDoesNotBroadcast) {
    nodepulse::config::SseConfig cfg;
    cfg.enabled = false;
    cfg.interval_ms = 1000;

    nodepulse::services::StreamService stream_svc(nullptr, nullptr, nullptr, cfg);
    EXPECT_FALSE(stream_svc.is_enabled());

    auto client = std::make_shared<MockSseClient>();
    stream_svc.add_client(client);

    EXPECT_EQ(stream_svc.client_count(), 0U);
    EXPECT_TRUE(client->closed);
    EXPECT_TRUE(client->sent_chunks.empty());

    size_t broadcast_count = stream_svc.broadcast_pulse();
    EXPECT_EQ(broadcast_count, 0U);
}

TEST(StreamServiceTest, PulseCreationWithLiveCollectors) {
    // Tests create_pulse() coordinating real collectors/services
    auto cpu_collector = std::make_shared<nodepulse::collectors::CpuCollector>();
    auto cpu_service = std::make_shared<nodepulse::services::CpuService>(cpu_collector);

    auto mem_collector = std::make_shared<nodepulse::collectors::MemoryCollector>();
    auto mem_service = std::make_shared<nodepulse::services::MemoryService>(mem_collector);

    auto net_collector = std::make_shared<nodepulse::collectors::NetworkCollector>();
    auto net_service = std::make_shared<nodepulse::services::NetworkService>(net_collector);

    nodepulse::config::SseConfig cfg;
    cfg.enabled = true;
    cfg.interval_ms = 1000;

    nodepulse::services::StreamService stream_svc(cpu_service, mem_service, net_service, cfg);

    auto pulse = stream_svc.create_pulse();
    EXPECT_FALSE(pulse.timestamp.empty());
    EXPECT_GE(pulse.memory_usage_percent, 0.0);
    EXPECT_LE(pulse.memory_usage_percent, 100.0);
    EXPECT_GT(pulse.memory_used_bytes, 0ULL);

    // Initial CPU and Network may be null (warming up)
    // Verify that create_pulse does not invent 0.0 for warming up
    if (!pulse.cpu_usage_percent.has_value()) {
        nlohmann::json j = pulse;
        EXPECT_TRUE(j["cpu_usage_percent"].is_null());
    }
}

TEST(StreamServiceTest, ConcurrentAddAndBroadcastThreadSafety) {
    nodepulse::config::SseConfig cfg;
    cfg.enabled = true;
    cfg.interval_ms = 50;

    nodepulse::services::StreamService stream_svc(nullptr, nullptr, nullptr, cfg);

    constexpr int kClients = 20;
    std::atomic<bool> stop{false};
    std::vector<std::shared_ptr<MockSseClient>> clients;
    for (int i = 0; i < kClients; ++i) {
        clients.push_back(std::make_shared<MockSseClient>());
    }

    // Thread 1: Add and remove clients
    std::thread t1([&]() {
        for (int i = 0; i < kClients && !stop; ++i) {
            stream_svc.add_client(clients[static_cast<size_t>(i)]);
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    });

    // Thread 2: Broadcast pulses
    std::thread t2([&]() {
        for (int i = 0; i < 20 && !stop; ++i) {
            stream_svc.broadcast_pulse();
            std::this_thread::sleep_for(std::chrono::milliseconds(2));
        }
    });

    t1.join();
    t2.join();

    EXPECT_LE(stream_svc.client_count(), static_cast<size_t>(kClients));
    stream_svc.stop_streaming();
    EXPECT_EQ(stream_svc.client_count(), 0U);
}

TEST(StreamServiceTest, ExactlyMaxClientsSubscribersAcceptedAndMaxPlusOneRejected) {
    nodepulse::config::SseConfig cfg;
    cfg.enabled = true;
    cfg.max_clients = 3;

    nodepulse::services::StreamService stream_svc(nullptr, nullptr, nullptr, cfg);

    auto c1 = std::make_shared<MockSseClient>();
    auto c2 = std::make_shared<MockSseClient>();
    auto c3 = std::make_shared<MockSseClient>();
    auto c4 = std::make_shared<MockSseClient>();

    EXPECT_TRUE(stream_svc.try_add_client(c1));
    EXPECT_TRUE(stream_svc.try_add_client(c2));
    EXPECT_TRUE(stream_svc.try_add_client(c3));
    EXPECT_FALSE(stream_svc.try_add_client(c4));

    EXPECT_EQ(stream_svc.client_count(), 3U);
    EXPECT_TRUE(c4->closed);
}

TEST(StreamServiceTest, ConcurrentAdmissionsNeverExceedMaxClients) {
    nodepulse::config::SseConfig cfg;
    cfg.enabled = true;
    cfg.max_clients = 10;

    nodepulse::services::StreamService stream_svc(nullptr, nullptr, nullptr, cfg);

    constexpr int kTotalAttempts = 60;
    std::vector<std::shared_ptr<MockSseClient>> test_clients;
    test_clients.reserve(kTotalAttempts);
    for (int i = 0; i < kTotalAttempts; ++i) {
        test_clients.push_back(std::make_shared<MockSseClient>());
    }

    std::atomic<int> accepted{0};
    std::atomic<int> rejected{0};
    std::vector<std::thread> threads;
    threads.reserve(kTotalAttempts);

    for (int i = 0; i < kTotalAttempts; ++i) {
        threads.emplace_back([&, i] {
            if (stream_svc.try_add_client(test_clients[static_cast<size_t>(i)])) {
                ++accepted;
            } else {
                ++rejected;
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    EXPECT_EQ(accepted.load(), 10);
    EXPECT_EQ(rejected.load(), 50);
    EXPECT_EQ(stream_svc.client_count(), 10U);
    EXPECT_LE(stream_svc.client_count(), cfg.max_clients);
}

TEST(StreamServiceTest, DisconnectReleasesCapacitySlot) {
    nodepulse::config::SseConfig cfg;
    cfg.enabled = true;
    cfg.max_clients = 2;

    nodepulse::services::StreamService stream_svc(nullptr, nullptr, nullptr, cfg);

    auto c1 = std::make_shared<MockSseClient>();
    auto c2 = std::make_shared<MockSseClient>();
    auto c3 = std::make_shared<MockSseClient>();

    EXPECT_TRUE(stream_svc.try_add_client(c1));
    EXPECT_TRUE(stream_svc.try_add_client(c2));
    EXPECT_FALSE(stream_svc.try_add_client(c3));

    stream_svc.remove_client(c1);
    EXPECT_EQ(stream_svc.client_count(), 1U);

    auto c4 = std::make_shared<MockSseClient>();
    EXPECT_TRUE(stream_svc.try_add_client(c4));
    EXPECT_EQ(stream_svc.client_count(), 2U);
}

TEST(StreamServiceTest, DeadClientPruningReleasesCapacitySlot) {
    nodepulse::config::SseConfig cfg;
    cfg.enabled = true;
    cfg.max_clients = 2;

    nodepulse::services::StreamService stream_svc(nullptr, nullptr, nullptr, cfg);

    auto c1 = std::make_shared<MockSseClient>();
    auto c2 = std::make_shared<MockSseClient>();
    auto c3 = std::make_shared<MockSseClient>();

    EXPECT_TRUE(stream_svc.try_add_client(c1));
    EXPECT_TRUE(stream_svc.try_add_client(c2));
    EXPECT_FALSE(stream_svc.try_add_client(c3));

    // Simulate c1 socket failure / disconnection
    c1->alive = false;

    // Next admission attempt with fresh client prunes dead c1 and succeeds
    auto c4 = std::make_shared<MockSseClient>();
    EXPECT_TRUE(stream_svc.try_add_client(c4));
    EXPECT_EQ(stream_svc.client_count(), 2U);
}

TEST(StreamServiceTest, ServerShutdownReleasesAllCapacitySlots) {
    nodepulse::config::SseConfig cfg;
    cfg.enabled = true;
    cfg.max_clients = 2;

    nodepulse::services::StreamService stream_svc(nullptr, nullptr, nullptr, cfg);

    auto c1 = std::make_shared<MockSseClient>();
    auto c2 = std::make_shared<MockSseClient>();
    EXPECT_TRUE(stream_svc.try_add_client(c1));
    EXPECT_TRUE(stream_svc.try_add_client(c2));
    EXPECT_EQ(stream_svc.client_count(), 2U);

    stream_svc.stop_streaming();
    EXPECT_EQ(stream_svc.client_count(), 0U);
    EXPECT_EQ(stream_svc.active_slot_count(), 0U);

    auto c3 = std::make_shared<MockSseClient>();
    EXPECT_TRUE(stream_svc.try_add_client(c3));
    EXPECT_EQ(stream_svc.client_count(), 1U);
}

TEST(StreamServiceTest, SlotReservationRollbackOnDestruction) {
    nodepulse::config::SseConfig cfg;
    cfg.enabled = true;
    cfg.max_clients = 1;

    nodepulse::services::StreamService stream_svc(nullptr, nullptr, nullptr, cfg);

    {
        auto reservation = stream_svc.try_reserve_slot();
        ASSERT_NE(reservation, nullptr);
        EXPECT_EQ(stream_svc.active_slot_count(), 1U);

        // While reserved, another reservation is rejected
        auto reservation2 = stream_svc.try_reserve_slot();
        EXPECT_EQ(reservation2, nullptr);
    }

    EXPECT_EQ(stream_svc.active_slot_count(), 0U);
    auto new_res = stream_svc.try_reserve_slot();
    EXPECT_NE(new_res, nullptr);
}
