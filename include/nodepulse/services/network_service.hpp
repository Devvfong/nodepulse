#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <nodepulse/collectors/network_collector.hpp>
#include <nodepulse/domain/network_info.hpp>

namespace nodepulse::services {

class NetworkService {
  public:
    explicit NetworkService(std::shared_ptr<collectors::NetworkCollector> collector =
                                std::make_shared<collectors::NetworkCollector>());
    ~NetworkService();

    NetworkService(const NetworkService&) = delete;
    NetworkService& operator=(const NetworkService&) = delete;
    NetworkService(NetworkService&&) = delete;
    NetworkService& operator=(NetworkService&&) = delete;

    [[nodiscard]] std::optional<std::vector<domain::NetworkInterfaceMetrics>> get_network_metrics();

    // Background sampling methods (non-blocking for Drogon event loops)
    void start_sampling(std::chrono::milliseconds interval = std::chrono::milliseconds(1000));
    void stop_sampling();
    [[nodiscard]] bool is_sampling() const noexcept;

    // Trigger synchronous sample on demand (e.g. for deterministic unit testing)
    std::optional<std::vector<domain::NetworkInterfaceMetrics>> sample();

    // Reset internal snapshot baseline (useful for testing)
    void reset_baseline();

  private:
    struct InterfaceBaseline {
        uint64_t rx_bytes{0};
        uint64_t tx_bytes{0};
        std::chrono::steady_clock::time_point timestamp;
    };

    std::optional<std::vector<domain::NetworkInterfaceMetrics>> sample_locked();
    void sampling_loop(std::chrono::milliseconds interval);

    std::shared_ptr<collectors::NetworkCollector> collector_;
    mutable std::mutex mutex_;
    std::condition_variable stop_cv_;
    std::thread sampling_thread_;
    std::atomic<bool> is_sampling_{false};

    std::unordered_map<std::string, InterfaceBaseline> baselines_;
    std::optional<std::vector<domain::NetworkInterfaceMetrics>> latest_metrics_;
};

}  // namespace nodepulse::services

