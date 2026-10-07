#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include <trantor/utils/TaskQueue.h>

#include <nodepulse/collectors/process_collector.hpp>
#include <nodepulse/domain/process_info.hpp>

namespace nodepulse::services {

class ProcessService {
  public:
    explicit ProcessService(std::shared_ptr<collectors::ProcessCollector> collector = nullptr,
                            std::shared_ptr<trantor::TaskQueue> task_queue = nullptr);
    virtual ~ProcessService();

    ProcessService(const ProcessService&) = delete;
    ProcessService& operator=(const ProcessService&) = delete;

    // Background sampling lifecycle
    void start_sampling(std::chrono::milliseconds interval = std::chrono::milliseconds(1000));
    void stop_sampling();
    [[nodiscard]] bool is_sampling() const noexcept;

    // Core methods
    [[nodiscard]] virtual bool is_valid_pid(int32_t pid) const;
    [[nodiscard]] virtual int64_t get_pid_max() const;

    [[nodiscard]] virtual std::vector<domain::ProcessInfo> get_processes(
        const std::string& sort_field = "cpu", int limit = 50);

    [[nodiscard]] virtual std::optional<domain::ProcessDetail> get_process_detail(int32_t pid);

    // Asynchronous worker offloading (DEC-013)
    bool get_processes_async(const std::string& sort_field, int limit,
                             std::function<void(std::vector<domain::ProcessInfo>)> callback);

    bool get_process_detail_async(
        int32_t pid, std::function<void(std::optional<domain::ProcessDetail>)> callback);

    // Manual sampling helper for testing
    void sample_now();

  private:
    struct PidCpuBaseline {
        uint64_t total_ticks{0};
        uint64_t starttime{0};
        std::chrono::steady_clock::time_point timestamp;
    };

    std::shared_ptr<collectors::ProcessCollector> collector_;
    std::shared_ptr<trantor::TaskQueue> task_queue_;
    mutable std::mutex mutex_;
    std::unordered_map<int32_t, PidCpuBaseline> baselines_;
    std::unordered_map<int32_t, double> latest_cpu_percents_;
    std::vector<domain::ProcessInfo> cached_processes_;

    std::atomic<bool> is_sampling_{false};
    std::thread sampling_thread_;
    std::condition_variable stop_cv_;

    void sampling_loop(std::chrono::milliseconds interval);
    void sample_locked();
};

}  // namespace nodepulse::services
