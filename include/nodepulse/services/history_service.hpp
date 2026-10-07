#pragma once

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <thread>
#include <vector>

#include <nodepulse/config/config.hpp>
#include <nodepulse/domain/host_snapshot.hpp>
#include <nodepulse/repositories/postgres_repository.hpp>
#include <nodepulse/services/cpu_service.hpp>
#include <nodepulse/services/memory_service.hpp>
#include <nodepulse/services/network_service.hpp>
#include <nodepulse/services/system_service.hpp>

namespace nodepulse::services {

/**
 * @brief Service coordinating periodic host telemetry persistence to PostgreSQL (Phase 14).
 *
 * Runs an isolated background persistence thread to periodically sample cached
 * metrics from SystemService, CpuService, MemoryService, and NetworkService
 * and insert them into PostgreSQL without starving Drogon's HTTP event loop.
 */
class HistoryService {
  public:
    HistoryService(std::shared_ptr<repositories::IPostgresRepository> repository,
                   std::shared_ptr<SystemService> system_service,
                   std::shared_ptr<CpuService> cpu_service,
                   std::shared_ptr<MemoryService> memory_service,
                   std::shared_ptr<NetworkService> network_service, config::PostgresConfig config);

    ~HistoryService();

    HistoryService(const HistoryService&) = delete;
    HistoryService& operator=(const HistoryService&) = delete;
    HistoryService(HistoryService&&) = delete;
    HistoryService& operator=(HistoryService&&) = delete;

    /**
     * @brief Start the background snapshot persistence thread.
     */
    void start();

    /**
     * @brief Stop the background persistence thread gracefully.
     */
    void stop();

    [[nodiscard]] bool is_running() const noexcept {
        return is_running_.load();
    }

    [[nodiscard]] bool is_enabled() const noexcept {
        return config_.enabled;
    }

    [[nodiscard]] const config::PostgresConfig& get_config() const noexcept {
        return config_;
    }

    /**
     * @brief Collect a current snapshot from domain services without blocking event loops.
     */
    [[nodiscard]] domain::HostSnapshot collect_snapshot() const;

    /**
     * @brief Manually trigger a snapshot persistence cycle (useful for unit testing).
     * @return true if snapshot was saved, false otherwise.
     */
    bool trigger_snapshot();

  private:
    std::shared_ptr<repositories::IPostgresRepository> repository_;
    std::shared_ptr<SystemService> system_service_;
    std::shared_ptr<CpuService> cpu_service_;
    std::shared_ptr<MemoryService> memory_service_;
    std::shared_ptr<NetworkService> network_service_;
    config::PostgresConfig config_;

    std::atomic<bool> is_running_{false};
    std::thread worker_thread_;
    std::mutex loop_mutex_;
    std::condition_variable loop_cv_;

    void worker_loop();
};

}  // namespace nodepulse::services
