#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <memory>
#include <mutex>
#include <optional>
#include <thread>

#include <nodepulse/collectors/cpu_collector.hpp>
#include <nodepulse/domain/cpu_info.hpp>

namespace nodepulse::services {

class CpuService {
  public:
    explicit CpuService(std::shared_ptr<collectors::CpuCollector> collector =
                            std::make_shared<collectors::CpuCollector>());
    ~CpuService();

    CpuService(const CpuService&) = delete;
    CpuService& operator=(const CpuService&) = delete;
    CpuService(CpuService&&) = delete;
    CpuService& operator=(CpuService&&) = delete;

    [[nodiscard]] std::optional<domain::CpuMetrics> get_cpu_metrics();

    // Background sampling methods (non-blocking for Drogon event loops)
    void start_sampling(std::chrono::milliseconds interval = std::chrono::milliseconds(1000));
    void stop_sampling();
    [[nodiscard]] bool is_sampling() const noexcept;

    // Trigger synchronous sample on demand (e.g. for deterministic unit testing)
    std::optional<domain::CpuMetrics> sample();

    // Reset internal snapshot baseline (useful for testing)
    void reset_baseline();

  private:
    std::optional<domain::CpuMetrics> sample_locked();
    void sampling_loop(std::chrono::milliseconds interval);

    std::shared_ptr<collectors::CpuCollector> collector_;
    mutable std::mutex mutex_;
    std::condition_variable stop_cv_;
    std::thread sampling_thread_;
    std::atomic<bool> is_sampling_{false};

    std::optional<collectors::CpuSnapshot> previous_snapshot_;
    std::optional<domain::CpuMetrics> last_computed_metrics_;
    std::optional<domain::CpuMetrics> latest_metrics_;
    collectors::CpuStaticInfo cached_static_info_;
    bool has_cached_static_info_{false};
};

}  // namespace nodepulse::services
