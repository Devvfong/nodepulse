#include <algorithm>
#include <chrono>
#include <cmath>
#include <memory>
#include <mutex>
#include <optional>
#include <utility>

#include <nodepulse/collectors/cpu_collector.hpp>
#include <nodepulse/domain/cpu_info.hpp>
#include <nodepulse/services/cpu_service.hpp>

namespace nodepulse::services {

CpuService::CpuService(std::shared_ptr<collectors::CpuCollector> collector)
    : collector_(std::move(collector)) {
    if (!collector_) {
        collector_ = std::make_shared<collectors::CpuCollector>();
    }
}

CpuService::~CpuService() {
    stop_sampling();
}

void CpuService::start_sampling(std::chrono::milliseconds interval) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (is_sampling_.load()) {
        return;
    }
    is_sampling_.store(true);
    // Take initial baseline sample immediately
    latest_metrics_ = sample_locked();
    sampling_thread_ = std::thread([this, interval]() { sampling_loop(interval); });
}

void CpuService::stop_sampling() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!is_sampling_.load()) {
            return;
        }
        is_sampling_.store(false);
        stop_cv_.notify_all();
    }
    if (sampling_thread_.joinable()) {
        sampling_thread_.join();
    }
}

bool CpuService::is_sampling() const noexcept {
    return is_sampling_.load();
}

void CpuService::sampling_loop(std::chrono::milliseconds interval) {
    while (true) {
        {
            std::unique_lock<std::mutex> lock(mutex_);
            if (stop_cv_.wait_for(lock, interval, [this]() { return !is_sampling_.load(); })) {
                break;
            }
        }
        std::lock_guard<std::mutex> lock(mutex_);
        if (!is_sampling_.load()) {
            break;
        }
        latest_metrics_ = sample_locked();
    }
}

void CpuService::reset_baseline() {
    std::lock_guard<std::mutex> lock(mutex_);
    previous_snapshot_.reset();
    last_computed_metrics_.reset();
    latest_metrics_.reset();
}

std::optional<domain::CpuMetrics> CpuService::sample() {
    std::lock_guard<std::mutex> lock(mutex_);
    return sample_locked();
}

std::optional<domain::CpuMetrics> CpuService::get_cpu_metrics() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (is_sampling_.load() && latest_metrics_.has_value()) {
        return latest_metrics_;
    }
    return sample_locked();
}

std::optional<domain::CpuMetrics> CpuService::sample_locked() {
    auto stat_opt = collector_->read_stat();
    if (!stat_opt.has_value()) {
        return std::nullopt;
    }

    auto loadavg_opt = collector_->read_loadavg();
    if (!loadavg_opt.has_value()) {
        return std::nullopt;
    }

    if (!has_cached_static_info_) {
        cached_static_info_ = collector_->read_cpuinfo();
        has_cached_static_info_ = true;
    }

    const auto& current_snapshot = *stat_opt;
    uint32_t current_logical_cores = static_cast<uint32_t>(current_snapshot.cores.size());
    if (current_logical_cores == 0) {
        current_logical_cores = cached_static_info_.logical_cores;
    }

    domain::CpuMetrics metrics;
    metrics.load_average = *loadavg_opt;
    metrics.model_name = cached_static_info_.model_name;
    metrics.physical_cores = cached_static_info_.physical_cores;
    metrics.logical_cores = current_logical_cores;

    // Check if this is the first sample
    if (!previous_snapshot_.has_value()) {
        metrics.usage_percent = std::nullopt;
        metrics.measurement_status = "warming_up";
        for (const auto& core : current_snapshot.cores) {
            domain::CpuCoreMetrics core_m;
            core_m.core_id = core.core_id;
            core_m.usage_percent = std::nullopt;
            core_m.user_jiffies = core.time.user;
            core_m.system_jiffies = core.time.system;
            core_m.idle_jiffies = core.time.idle;
            core_m.iowait_jiffies = core.time.iowait;
            metrics.cores.push_back(core_m);
        }

        previous_snapshot_ = current_snapshot;
        last_computed_metrics_ = metrics;
        return metrics;
    }

    // Check for CPU core changes (hotplug)
    if (current_snapshot.cores.size() != previous_snapshot_->cores.size()) {
        metrics.usage_percent = std::nullopt;
        metrics.measurement_status = "warming_up";
        for (const auto& core : current_snapshot.cores) {
            domain::CpuCoreMetrics core_m;
            core_m.core_id = core.core_id;
            core_m.usage_percent = std::nullopt;
            core_m.user_jiffies = core.time.user;
            core_m.system_jiffies = core.time.system;
            core_m.idle_jiffies = core.time.idle;
            core_m.iowait_jiffies = core.time.iowait;
            metrics.cores.push_back(core_m);
        }
        previous_snapshot_ = current_snapshot;
        last_computed_metrics_ = metrics;
        return metrics;
    }

    uint64_t prev_total = previous_snapshot_->aggregate.total();
    uint64_t curr_total = current_snapshot.aggregate.total();
    uint64_t prev_idle = previous_snapshot_->aggregate.idle_all();
    uint64_t curr_idle = current_snapshot.aggregate.idle_all();

    // Check for counter reset / wrap / anomaly
    bool counter_wrap = (curr_total < prev_total || curr_idle < prev_idle);
    bool delta_anomaly = (curr_total >= prev_total && curr_idle >= prev_idle &&
                          (curr_idle - prev_idle) > (curr_total - prev_total));

    if (counter_wrap || delta_anomaly) {
        metrics.usage_percent = std::nullopt;
        metrics.measurement_status = "warming_up";
        for (const auto& core : current_snapshot.cores) {
            domain::CpuCoreMetrics core_m;
            core_m.core_id = core.core_id;
            core_m.usage_percent = std::nullopt;
            core_m.user_jiffies = core.time.user;
            core_m.system_jiffies = core.time.system;
            core_m.idle_jiffies = core.time.idle;
            core_m.iowait_jiffies = core.time.iowait;
            metrics.cores.push_back(core_m);
        }
        previous_snapshot_ = current_snapshot;
        last_computed_metrics_ = metrics;
        return metrics;
    }

    // Zero elapsed total time (rapid back-to-back queries)
    if (curr_total == prev_total) {
        if (last_computed_metrics_.has_value() &&
            last_computed_metrics_->measurement_status != "warming_up") {
            metrics.usage_percent = last_computed_metrics_->usage_percent;
            metrics.measurement_status = "cached";
            metrics.cores = last_computed_metrics_->cores;
            for (auto& core_m : metrics.cores) {
                for (const auto& core : current_snapshot.cores) {
                    if (core.core_id == core_m.core_id) {
                        core_m.user_jiffies = core.time.user;
                        core_m.system_jiffies = core.time.system;
                        core_m.idle_jiffies = core.time.idle;
                        core_m.iowait_jiffies = core.time.iowait;
                        break;
                    }
                }
            }
        } else {
            metrics.usage_percent = std::nullopt;
            metrics.measurement_status = "warming_up";
            for (const auto& core : current_snapshot.cores) {
                domain::CpuCoreMetrics core_m;
                core_m.core_id = core.core_id;
                core_m.usage_percent = std::nullopt;
                core_m.user_jiffies = core.time.user;
                core_m.system_jiffies = core.time.system;
                core_m.idle_jiffies = core.time.idle;
                core_m.iowait_jiffies = core.time.iowait;
                metrics.cores.push_back(core_m);
            }
        }
        return metrics;
    }

    // Normal delta calculation
    uint64_t delta_total = curr_total - prev_total;
    uint64_t delta_idle = curr_idle - prev_idle;
    uint64_t delta_busy = (delta_total >= delta_idle) ? (delta_total - delta_idle) : 0;

    double agg_pct = (static_cast<double>(delta_busy) / static_cast<double>(delta_total)) * 100.0;
    agg_pct = std::clamp(agg_pct, 0.0, 100.0);
    metrics.usage_percent = std::round(agg_pct * 100.0) / 100.0;
    metrics.measurement_status = "ready";

    // Per-core deltas
    for (const auto& curr_core : current_snapshot.cores) {
        domain::CpuCoreMetrics core_m;
        core_m.core_id = curr_core.core_id;
        core_m.user_jiffies = curr_core.time.user;
        core_m.system_jiffies = curr_core.time.system;
        core_m.idle_jiffies = curr_core.time.idle;
        core_m.iowait_jiffies = curr_core.time.iowait;

        const collectors::CpuCoreSnapshot* prev_core_ptr = nullptr;
        for (const auto& prev_core : previous_snapshot_->cores) {
            if (prev_core.core_id == curr_core.core_id) {
                prev_core_ptr = &prev_core;
                break;
            }
        }

        if (prev_core_ptr != nullptr) {
            uint64_t c_prev_tot = prev_core_ptr->time.total();
            uint64_t c_curr_tot = curr_core.time.total();
            uint64_t c_prev_idl = prev_core_ptr->time.idle_all();
            uint64_t c_curr_idl = curr_core.time.idle_all();

            if (c_curr_tot > c_prev_tot && c_curr_idl >= c_prev_idl) {
                uint64_t c_delta_tot = c_curr_tot - c_prev_tot;
                uint64_t c_delta_idl = c_curr_idl - c_prev_idl;
                if (c_delta_idl <= c_delta_tot) {
                    uint64_t c_delta_busy = c_delta_tot - c_delta_idl;
                    double c_pct =
                        (static_cast<double>(c_delta_busy) / static_cast<double>(c_delta_tot)) *
                        100.0;
                    c_pct = std::clamp(c_pct, 0.0, 100.0);
                    core_m.usage_percent = std::round(c_pct * 100.0) / 100.0;
                } else {
                    core_m.usage_percent = std::nullopt;
                }
            } else if (c_curr_tot == c_prev_tot) {
                core_m.usage_percent = 0.0;
            } else {
                core_m.usage_percent = std::nullopt;
            }
        } else {
            core_m.usage_percent = std::nullopt;
        }

        metrics.cores.push_back(core_m);
    }

    previous_snapshot_ = current_snapshot;
    last_computed_metrics_ = metrics;
    return metrics;
}

}  // namespace nodepulse::services
