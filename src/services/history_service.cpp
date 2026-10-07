#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <nodepulse/domain/host_snapshot.hpp>
#include <nodepulse/services/history_service.hpp>
#include <nodepulse/utils/logger.hpp>

namespace nodepulse::services {

HistoryService::HistoryService(std::shared_ptr<repositories::IPostgresRepository> repository,
                               std::shared_ptr<SystemService> system_service,
                               std::shared_ptr<CpuService> cpu_service,
                               std::shared_ptr<MemoryService> memory_service,
                               std::shared_ptr<NetworkService> network_service,
                               config::PostgresConfig config)
    : repository_(std::move(repository)),
      system_service_(std::move(system_service)),
      cpu_service_(std::move(cpu_service)),
      memory_service_(std::move(memory_service)),
      network_service_(std::move(network_service)),
      config_(std::move(config)) {}

HistoryService::~HistoryService() {
    stop();
}

void HistoryService::start() {
    if (!config_.enabled) {
        return;
    }

    std::lock_guard<std::mutex> lock(loop_mutex_);
    if (is_running_) {
        return;
    }

    is_running_ = true;
    worker_thread_ = std::thread([this] { worker_loop(); });
}

void HistoryService::stop() {
    {
        std::lock_guard<std::mutex> lock(loop_mutex_);
        if (is_running_) {
            is_running_ = false;
            loop_cv_.notify_all();
        }
    }

    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }
}

domain::HostSnapshot HistoryService::collect_snapshot() const {
    domain::HostSnapshot snapshot;
    snapshot.captured_at = std::chrono::system_clock::now();

    if (system_service_) {
        auto sys = system_service_->get_system_info();
        if (sys.has_value() && !sys->hostname.empty()) {
            snapshot.hostname = sys->hostname;
        } else {
            snapshot.hostname = "localhost";
        }
    } else {
        snapshot.hostname = "localhost";
    }

    if (cpu_service_) {
        auto cpu = cpu_service_->get_cpu_metrics();
        if (cpu.has_value()) {
            snapshot.cpu_usage_percent = cpu->usage_percent;
            snapshot.load_1m = cpu->load_average.one_minute;
            snapshot.load_5m = cpu->load_average.five_minute;
            snapshot.load_15m = cpu->load_average.fifteen_minute;
        }
    }

    if (memory_service_) {
        auto mem = memory_service_->get_memory_metrics();
        if (mem.has_value()) {
            snapshot.mem_total_bytes = mem->total_bytes;
            snapshot.mem_used_bytes = mem->used_bytes;
            snapshot.mem_available_bytes = mem->available_bytes;
            snapshot.swap_total_bytes = mem->swap_total_bytes;
            snapshot.swap_used_bytes = mem->swap_used_bytes;
        }
    }

    if (network_service_) {
        auto net = network_service_->get_network_metrics();
        if (net.has_value() && !net->empty()) {
            bool has_non_loopback = false;
            for (const auto& iface : *net) {
                if (iface.name != "lo") {
                    has_non_loopback = true;
                    break;
                }
            }

            bool any_rx = false;
            bool any_tx = false;
            double sum_rx = 0.0;
            double sum_tx = 0.0;

            for (const auto& iface : *net) {
                if (has_non_loopback && iface.name == "lo") {
                    continue;
                }
                if (iface.rx_bytes_per_sec.has_value()) {
                    any_rx = true;
                    sum_rx += *iface.rx_bytes_per_sec;
                }
                if (iface.tx_bytes_per_sec.has_value()) {
                    any_tx = true;
                    sum_tx += *iface.tx_bytes_per_sec;
                }
            }

            if (any_rx) {
                snapshot.net_rx_bytes_per_sec = std::round(sum_rx * 100.0) / 100.0;
            } else {
                snapshot.net_rx_bytes_per_sec = std::nullopt;
            }

            if (any_tx) {
                snapshot.net_tx_bytes_per_sec = std::round(sum_tx * 100.0) / 100.0;
            } else {
                snapshot.net_tx_bytes_per_sec = std::nullopt;
            }
        }
    }

    return snapshot;
}

bool HistoryService::trigger_snapshot() {
    if (!repository_) {
        return false;
    }
    auto snapshot = collect_snapshot();
    return repository_->save_snapshot(snapshot);
}

void HistoryService::worker_loop() {
    if (repository_) {
        repository_->init_schema();
    }

    std::unique_lock<std::mutex> lock(loop_mutex_);
    while (is_running_) {
        auto interval = std::chrono::seconds(std::max(1U, config_.snapshot_interval_seconds));
        loop_cv_.wait_for(lock, interval, [this] { return !is_running_; });
        if (!is_running_) {
            break;
        }

        // Drop lock during I/O operations
        lock.unlock();
        trigger_snapshot();
        lock.lock();
    }
}

}  // namespace nodepulse::services
