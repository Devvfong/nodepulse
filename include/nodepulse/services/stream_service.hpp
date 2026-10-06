#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include <drogon/HttpResponse.h>

#include <nodepulse/config/config.hpp>
#include <nodepulse/domain/metric_pulse.hpp>
#include <nodepulse/services/cpu_service.hpp>
#include <nodepulse/services/memory_service.hpp>
#include <nodepulse/services/network_service.hpp>

namespace nodepulse::services {

class ISseClient {
  public:
    virtual ~ISseClient() = default;
    virtual bool send_data(const std::string& chunk) = 0;
    virtual void close() = 0;
    [[nodiscard]] virtual bool is_alive() const = 0;
};

class DrogonSseClient final : public ISseClient {
  public:
    explicit DrogonSseClient(drogon::ResponseStreamPtr stream);
    ~DrogonSseClient() override;

    DrogonSseClient(const DrogonSseClient&) = delete;
    DrogonSseClient& operator=(const DrogonSseClient&) = delete;
    DrogonSseClient(DrogonSseClient&&) = delete;
    DrogonSseClient& operator=(DrogonSseClient&&) = delete;

    bool send_data(const std::string& chunk) override;
    void close() override;
    [[nodiscard]] bool is_alive() const override;

  private:
    void do_close();

    mutable std::mutex mutex_;
    drogon::ResponseStreamPtr stream_;
    bool alive_{true};
};

class StreamService;

class SseSlotReservation final {
  public:
    explicit SseSlotReservation(StreamService* service);
    ~SseSlotReservation();

    SseSlotReservation(const SseSlotReservation&) = delete;
    SseSlotReservation& operator=(const SseSlotReservation&) = delete;

    SseSlotReservation(SseSlotReservation&& other) noexcept;
    SseSlotReservation& operator=(SseSlotReservation&& other) noexcept;

    bool commit(std::shared_ptr<ISseClient> client);
    void release();

  private:
    StreamService* service_{nullptr};
    bool committed_{false};
};

class StreamService {
  public:
    explicit StreamService(std::shared_ptr<CpuService> cpu_service = nullptr,
                           std::shared_ptr<MemoryService> memory_service = nullptr,
                           std::shared_ptr<NetworkService> network_service = nullptr,
                           config::SseConfig config = {});
    ~StreamService();

    StreamService(const StreamService&) = delete;
    StreamService& operator=(const StreamService&) = delete;
    StreamService(StreamService&&) = delete;
    StreamService& operator=(StreamService&&) = delete;

    [[nodiscard]] bool is_enabled() const noexcept {
        return config_.enabled;
    }
    [[nodiscard]] size_t max_clients() const noexcept {
        return config_.max_clients;
    }
    [[nodiscard]] size_t client_count() const;
    [[nodiscard]] size_t active_slot_count() const;

    // Atomic admission control
    [[nodiscard]] std::unique_ptr<SseSlotReservation> try_reserve_slot();
    bool try_add_client(std::shared_ptr<ISseClient> client);

    void add_client(std::shared_ptr<ISseClient> client);
    void remove_client(const std::shared_ptr<ISseClient>& client);

    void start_streaming(std::chrono::milliseconds interval = std::chrono::milliseconds(1000));
    void stop_streaming();
    [[nodiscard]] bool is_streaming() const noexcept;

    // Single step broadcast (for background loop or deterministic testing)
    size_t broadcast_pulse();
    size_t send_heartbeat();

    // Pulse generation & framing helpers
    [[nodiscard]] domain::MetricPulse create_pulse() const;
    [[nodiscard]] static std::string format_pulse_frame(uint64_t id,
                                                        const domain::MetricPulse& pulse);
    [[nodiscard]] static std::string format_heartbeat_frame();

  private:
    friend class SseSlotReservation;
    void release_reservation();
    bool commit_reservation(std::shared_ptr<ISseClient> client);

    void streaming_loop(std::chrono::milliseconds interval);

    std::shared_ptr<CpuService> cpu_service_;
    std::shared_ptr<MemoryService> memory_service_;
    std::shared_ptr<NetworkService> network_service_;
    config::SseConfig config_;

    mutable std::mutex clients_mutex_;
    std::vector<std::shared_ptr<ISseClient>> clients_;
    size_t reserved_slots_{0};

    std::atomic<uint64_t> sequence_counter_{0};
    std::atomic<bool> is_running_{false};
    std::thread worker_thread_;
    std::mutex loop_mutex_;
    std::condition_variable loop_cv_;
};

}  // namespace nodepulse::services
