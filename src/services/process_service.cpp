#include <algorithm>
#include <chrono>
#include <cmath>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include <drogon/drogon.h>

#include <nodepulse/collectors/process_collector.hpp>
#include <nodepulse/services/process_service.hpp>
#include <nodepulse/utils/bounded_task_queue.hpp>

#include <unistd.h>

namespace nodepulse::services {

ProcessService::ProcessService(std::shared_ptr<collectors::ProcessCollector> collector,
                               std::shared_ptr<trantor::TaskQueue> task_queue)
    : collector_(std::move(collector)), task_queue_(std::move(task_queue)) {
    if (!collector_) {
        collector_ = std::make_shared<collectors::ProcessCollector>();
    }
    if (!task_queue_) {
        task_queue_ = std::make_shared<utils::BoundedTaskQueue>(
            2, utils::BoundedTaskQueue::kDefaultMaxQueueSize, "process_worker");
    }
}

ProcessService::~ProcessService() {
    stop_sampling();
}

void ProcessService::start_sampling(std::chrono::milliseconds interval) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (is_sampling_) {
        return;
    }
    is_sampling_ = true;
    sampling_thread_ = std::thread([this, interval]() { sampling_loop(interval); });
}

void ProcessService::stop_sampling() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!is_sampling_) {
            return;
        }
        is_sampling_ = false;
        stop_cv_.notify_all();
    }
    if (sampling_thread_.joinable()) {
        sampling_thread_.join();
    }
}

bool ProcessService::is_sampling() const noexcept {
    return is_sampling_.load();
}

bool ProcessService::is_valid_pid(int32_t pid) const {
    if (pid < 1) {
        return false;
    }
    int64_t max_val = get_pid_max();
    return static_cast<int64_t>(pid) <= max_val;
}

int64_t ProcessService::get_pid_max() const {
    if (collector_) {
        return collector_->get_pid_max();
    }
    return 4194304;
}

void ProcessService::sample_now() {
    std::lock_guard<std::mutex> lock(mutex_);
    sample_locked();
}

void ProcessService::sampling_loop(std::chrono::milliseconds interval) {
    std::unique_lock<std::mutex> lock(mutex_);
    sample_locked();

    while (is_sampling_) {
        if (stop_cv_.wait_for(lock, interval, [this]() { return !is_sampling_.load(); })) {
            break;
        }
        if (!is_sampling_) {
            break;
        }
        sample_locked();
    }
}

void ProcessService::sample_locked() {
    if (!collector_) {
        return;
    }

    auto processes = collector_->collect_processes();
    auto now = std::chrono::steady_clock::now();
    std::unordered_set<int32_t> active_pids;

    long clk_tck = sysconf(_SC_CLK_TCK);
    if (clk_tck <= 0) {
        clk_tck = 100;
    }

    for (auto& proc : processes) {
        active_pids.insert(proc.pid);

        // Read stat to get ticks for this PID
        std::string stat_path = collector_->proc_dir() + "/" + std::to_string(proc.pid) + "/stat";
        std::ifstream stat_file(stat_path);
        collectors::ProcessStatFields fields;
        std::string line;
        uint64_t total_ticks = 0;
        uint64_t starttime = 0;
        bool stat_read_ok = false;
        if (stat_file.is_open() && std::getline(stat_file, line) &&
            collectors::ProcessCollector::parse_stat_line(line, fields)) {
            total_ticks = fields.utime + fields.stime;
            starttime = fields.starttime;
            stat_read_ok = true;
        }

        if (!stat_read_ok) {
            proc.cpu_percent = 0.0;
            continue;
        }

        auto it = baselines_.find(proc.pid);
        if (it == baselines_.end() || it->second.starttime != starttime) {
            // First sample or PID reuse detected: establish fresh baseline
            baselines_[proc.pid] = PidCpuBaseline{total_ticks, starttime, now};
            latest_cpu_percents_[proc.pid] = 0.0;
            proc.cpu_percent = 0.0;
        } else {
            auto& prev = it->second;
            double elapsed_sec = std::chrono::duration<double>(now - prev.timestamp).count();

            if (elapsed_sec > 0.001) {
                if (total_ticks < prev.total_ticks) {
                    // Counter reset or wrap
                    latest_cpu_percents_[proc.pid] = 0.0;
                    proc.cpu_percent = 0.0;
                } else {
                    uint64_t delta_ticks = total_ticks - prev.total_ticks;
                    double usage =
                        (static_cast<double>(delta_ticks) / static_cast<double>(clk_tck)) /
                        elapsed_sec * 100.0;
                    double rounded = std::round(usage * 10.0) / 10.0;
                    latest_cpu_percents_[proc.pid] = rounded;
                    proc.cpu_percent = rounded;
                }
                prev = PidCpuBaseline{total_ticks, starttime, now};
            } else {
                proc.cpu_percent = latest_cpu_percents_[proc.pid];
            }
        }
    }

    // Clean up terminated PIDs
    for (auto it = baselines_.begin(); it != baselines_.end();) {
        if (!active_pids.contains(it->first)) {
            latest_cpu_percents_.erase(it->first);
            it = baselines_.erase(it);
        } else {
            ++it;
        }
    }

    cached_processes_ = std::move(processes);
}

std::vector<domain::ProcessInfo> ProcessService::get_processes(const std::string& sort_field,
                                                               int limit) {
    std::vector<domain::ProcessInfo> result;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (cached_processes_.empty()) {
            sample_locked();
        }
        result = cached_processes_;
    }

    // Sort
    if (sort_field == "memory") {
        std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
            if (a.memory_rss_bytes != b.memory_rss_bytes) {
                return a.memory_rss_bytes > b.memory_rss_bytes;
            }
            return a.pid < b.pid;
        });
    } else if (sort_field == "pid") {
        std::sort(result.begin(), result.end(),
                  [](const auto& a, const auto& b) { return a.pid < b.pid; });
    } else {
        // Default "cpu"
        std::sort(result.begin(), result.end(), [](const auto& a, const auto& b) {
            if (a.cpu_percent != b.cpu_percent) {
                return a.cpu_percent > b.cpu_percent;
            }
            return a.pid < b.pid;
        });
    }

    if (limit > 0 && static_cast<size_t>(limit) < result.size()) {
        result.resize(static_cast<size_t>(limit));
    }

    return result;
}

std::optional<domain::ProcessDetail> ProcessService::get_process_detail(int32_t pid) {
    double cpu_percent = 0.0;
    uint64_t expected_starttime = 0;
    {
        std::lock_guard<std::mutex> lock(mutex_);
        auto it = baselines_.find(pid);
        if (it != baselines_.end()) {
            expected_starttime = it->second.starttime;
            auto cpu_it = latest_cpu_percents_.find(pid);
            if (cpu_it != latest_cpu_percents_.end()) {
                cpu_percent = cpu_it->second;
            }
        }
    }

    if (!collector_) {
        return std::nullopt;
    }
    return collector_->collect_process_detail(pid, cpu_percent, expected_starttime);
}

bool ProcessService::get_processes_async(
    const std::string& sort_field, int limit,
    std::function<void(std::vector<domain::ProcessInfo>)> callback) {
    if (!task_queue_) {
        callback(get_processes(sort_field, limit));
        return true;
    }

    auto bounded_q = std::dynamic_pointer_cast<utils::BoundedTaskQueue>(task_queue_);
    if (bounded_q) {
        return bounded_q->tryRunTaskInQueue([this, sort_field, limit, cb = std::move(callback)]() {
            cb(get_processes(sort_field, limit));
        });
    }

    task_queue_->runTaskInQueue([this, sort_field, limit, callback = std::move(callback)]() {
        callback(get_processes(sort_field, limit));
    });
    return true;
}

bool ProcessService::get_process_detail_async(
    int32_t pid, std::function<void(std::optional<domain::ProcessDetail>)> callback) {
    if (!task_queue_) {
        callback(get_process_detail(pid));
        return true;
    }

    auto bounded_q = std::dynamic_pointer_cast<utils::BoundedTaskQueue>(task_queue_);
    if (bounded_q) {
        return bounded_q->tryRunTaskInQueue(
            [this, pid, cb = std::move(callback)]() { cb(get_process_detail(pid)); });
    }

    task_queue_->runTaskInQueue(
        [this, pid, callback = std::move(callback)]() { callback(get_process_detail(pid)); });
    return true;
}

}  // namespace nodepulse::services
