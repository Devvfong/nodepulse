#include <algorithm>
#include <cctype>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <memory>
#include <mutex>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <nodepulse/services/metrics_exporter.hpp>

#include <unistd.h>

namespace nodepulse::services {

namespace {

#if defined(__clang__)
#define NP_COMPILER_NAME "clang-" __clang_version__
#elif defined(__GNUC__)
#define NP_COMPILER_NAME "gcc-" __VERSION__
#else
#define NP_COMPILER_NAME "unknown"
#endif

constexpr const char* kVersion = "0.1.0";
constexpr const char* kCommit = "c4c4fe1";

std::string format_double(double val) {
    if (std::isnan(val) || std::isinf(val)) {
        return "0";
    }
    char buf[64];
    auto [ptr, ec] = std::to_chars(buf, buf + sizeof(buf), val);
    if (ec == std::errc{}) {
        return std::string(buf, ptr);
    }
    return "0";
}

std::optional<ProcessSelfStats> read_default_self_stats() {
    std::ifstream stat_file("/proc/self/stat");
    if (!stat_file.is_open()) {
        return std::nullopt;
    }

    std::string line;
    if (!std::getline(stat_file, line) || line.empty()) {
        return std::nullopt;
    }

    auto close_paren = line.rfind(')');
    if (close_paren == std::string::npos) {
        return std::nullopt;
    }

    std::string_view rest = std::string_view(line).substr(close_paren + 1);
    std::vector<std::string_view> tokens;
    size_t i = 0;
    while (i < rest.size()) {
        while (i < rest.size() && std::isspace(static_cast<unsigned char>(rest[i])) != 0) {
            ++i;
        }
        if (i >= rest.size()) {
            break;
        }
        size_t start = i;
        while (i < rest.size() && std::isspace(static_cast<unsigned char>(rest[i])) == 0) {
            ++i;
        }
        tokens.push_back(rest.substr(start, i - start));
    }

    // Need at least 22 tokens after comm:
    // token 11: utime, token 12: stime, token 21: rss_pages
    if (tokens.size() < 22) {
        return std::nullopt;
    }

    uint64_t utime_ticks = 0;
    uint64_t stime_ticks = 0;
    int64_t rss_pages = 0;

    auto [p1, ec1] =
        std::from_chars(tokens[11].data(), tokens[11].data() + tokens[11].size(), utime_ticks);
    auto [p2, ec2] =
        std::from_chars(tokens[12].data(), tokens[12].data() + tokens[12].size(), stime_ticks);
    auto [p3, ec3] =
        std::from_chars(tokens[21].data(), tokens[21].data() + tokens[21].size(), rss_pages);

    if (ec1 != std::errc{} || ec2 != std::errc{} || ec3 != std::errc{}) {
        return std::nullopt;
    }

    long clk_tck = sysconf(_SC_CLK_TCK);
    if (clk_tck <= 0) {
        clk_tck = 100;
    }

    long page_size = sysconf(_SC_PAGESIZE);
    if (page_size <= 0) {
        page_size = 4096;
    }

    ProcessSelfStats stats{};
    stats.cpu_user_seconds = static_cast<double>(utime_ticks) / static_cast<double>(clk_tck);
    stats.cpu_system_seconds = static_cast<double>(stime_ticks) / static_cast<double>(clk_tck);

    if (rss_pages > 0) {
        stats.rss_bytes = static_cast<uint64_t>(rss_pages) * static_cast<uint64_t>(page_size);
    } else {
        // Fallback to /proc/self/statm
        std::ifstream statm_file("/proc/self/statm");
        if (statm_file.is_open()) {
            uint64_t total_pages = 0;
            uint64_t res_pages = 0;
            if (statm_file >> total_pages >> res_pages) {
                stats.rss_bytes = res_pages * static_cast<uint64_t>(page_size);
            }
        }
    }

    return stats;
}

}  // namespace

MetricsExporter::MetricsExporter(std::shared_ptr<CpuService> cpu_service,
                                 std::shared_ptr<MemoryService> memory_service,
                                 std::shared_ptr<DiskService> disk_service,
                                 std::shared_ptr<NetworkService> network_service,
                                 std::shared_ptr<StreamService> stream_service,
                                 config::PrometheusConfig config,
                                 std::shared_ptr<trantor::TaskQueue> task_queue)
    : cpu_service_(std::move(cpu_service)),
      memory_service_(std::move(memory_service)),
      disk_service_(std::move(disk_service)),
      network_service_(std::move(network_service)),
      stream_service_(std::move(stream_service)),
      config_(config),
      task_queue_(std::move(task_queue)),
      start_time_(std::chrono::steady_clock::now()) {
    if (!task_queue_) {
        task_queue_ = std::make_shared<trantor::ConcurrentTaskQueue>(2, "prom_worker");
    }
}

bool MetricsExporter::is_enabled() const noexcept {
    return config_.enabled;
}

void MetricsExporter::set_enabled(bool enabled) noexcept {
    config_.enabled = enabled;
}

void MetricsExporter::set_start_time(std::chrono::steady_clock::time_point start_time) {
    start_time_ = start_time;
}

void MetricsExporter::set_self_stat_provider(SelfStatProvider provider) {
    self_stat_provider_ = std::move(provider);
}

std::string MetricsExporter::normalize_endpoint(std::string_view raw_path) {
    if (raw_path == "/api/v1/health" || raw_path == "/api/v1/health/") {
        return "/api/v1/health";
    }
    if (raw_path == "/api/v1/system" || raw_path == "/api/v1/system/") {
        return "/api/v1/system";
    }
    if (raw_path == "/api/v1/cpu" || raw_path == "/api/v1/cpu/") {
        return "/api/v1/cpu";
    }
    if (raw_path == "/api/v1/memory" || raw_path == "/api/v1/memory/") {
        return "/api/v1/memory";
    }
    if (raw_path == "/api/v1/disks" || raw_path == "/api/v1/disks/") {
        return "/api/v1/disks";
    }
    if (raw_path == "/api/v1/network" || raw_path == "/api/v1/network/") {
        return "/api/v1/network";
    }
    if (raw_path == "/api/v1/processes" || raw_path == "/api/v1/processes/") {
        return "/api/v1/processes";
    }
    if (raw_path.starts_with("/api/v1/processes/")) {
        return "/api/v1/processes/{pid}";
    }
    if (raw_path == "/api/v1/services" || raw_path == "/api/v1/services/") {
        return "/api/v1/services";
    }
    if (raw_path.starts_with("/api/v1/services/")) {
        return "/api/v1/services/{name}";
    }
    if (raw_path == "/api/v1/containers" || raw_path == "/api/v1/containers/") {
        return "/api/v1/containers";
    }
    if (raw_path.starts_with("/api/v1/containers/")) {
        return "/api/v1/containers/{id}";
    }
    if (raw_path == "/api/v1/events" || raw_path == "/api/v1/events/") {
        return "/api/v1/events";
    }
    if (raw_path == "/metrics" || raw_path == "/metrics/") {
        return "/metrics";
    }
    return "unmatched";
}

std::string MetricsExporter::escape_label_value(std::string_view val) {
    std::string out;
    out.reserve(val.size() + 8);
    for (char c : val) {
        if (c == '\\') {
            out.append("\\\\");
        } else if (c == '\"') {
            out.append("\\\"");
        } else if (c == '\n') {
            out.append("\\n");
        } else {
            out.push_back(c);
        }
    }
    return out;
}

void MetricsExporter::record_http_request(std::string_view raw_path, std::string_view method,
                                          int status) {
    std::string ep = normalize_endpoint(raw_path);
    std::string m(method);
    HttpRequestKey key{std::move(ep), std::move(m), status};

    std::lock_guard<std::mutex> lock(internal_metrics_mutex_);
    ++http_requests_[key];
}

void MetricsExporter::record_collector_failure(std::string_view collector_name) {
    std::string c(collector_name);
    std::lock_guard<std::mutex> lock(internal_metrics_mutex_);
    ++collector_failures_[c];
}

void MetricsExporter::reset_internal_metrics() {
    std::lock_guard<std::mutex> lock(internal_metrics_mutex_);
    http_requests_.clear();
    collector_failures_.clear();
}

std::string MetricsExporter::export_metrics() const {
    std::string out;
    out.reserve(4096);

    // 1. Host CPU usage ratio: nodepulse_cpu_usage_ratio (Gauge)
    if (cpu_service_) {
        auto cpu = cpu_service_->get_cpu_metrics();
        if (cpu.has_value() && cpu->usage_percent.has_value()) {
            double ratio = *cpu->usage_percent / 100.0;
            if (ratio < 0.0) {
                ratio = 0.0;
            }
            if (ratio > 1.0) {
                ratio = 1.0;
            }
            out.append(
                "# HELP nodepulse_cpu_usage_ratio Current host CPU usage ratio (0.0 to 1.0)\n");
            out.append("# TYPE nodepulse_cpu_usage_ratio gauge\n");
            out.append("nodepulse_cpu_usage_ratio ");
            out.append(format_double(ratio));
            out.push_back('\n');
        }
    }

    // 2. Host RAM memory: nodepulse_memory_used_bytes, nodepulse_memory_total_bytes (Gauges)
    if (memory_service_) {
        auto mem = memory_service_->get_memory_metrics();
        if (mem.has_value()) {
            out.append(
                "# HELP nodepulse_memory_used_bytes Host RAM memory currently in use in bytes\n");
            out.append("# TYPE nodepulse_memory_used_bytes gauge\n");
            out.append("nodepulse_memory_used_bytes ");
            out.append(std::to_string(mem->used_bytes));
            out.push_back('\n');

            out.append("# HELP nodepulse_memory_total_bytes Total physical host RAM in bytes\n");
            out.append("# TYPE nodepulse_memory_total_bytes gauge\n");
            out.append("nodepulse_memory_total_bytes ");
            out.append(std::to_string(mem->total_bytes));
            out.push_back('\n');
        }
    }

    // 3. Disk space: nodepulse_disk_used_bytes, nodepulse_disk_total_bytes (Gauges)
    if (disk_service_) {
        auto disks = disk_service_->get_disk_metrics();
        if (disks.has_value() && !disks->empty()) {
            out.append("# HELP nodepulse_disk_used_bytes Disk space used in bytes\n");
            out.append("# TYPE nodepulse_disk_used_bytes gauge\n");
            for (const auto& part : *disks) {
                out.append("nodepulse_disk_used_bytes{filesystem=\"");
                out.append(escape_label_value(part.filesystem));
                out.append("\",mount_point=\"");
                out.append(escape_label_value(part.mount_point));
                out.append("\"} ");
                out.append(std::to_string(part.used_bytes));
                out.push_back('\n');
            }

            out.append("# HELP nodepulse_disk_total_bytes Total disk space in bytes\n");
            out.append("# TYPE nodepulse_disk_total_bytes gauge\n");
            for (const auto& part : *disks) {
                out.append("nodepulse_disk_total_bytes{filesystem=\"");
                out.append(escape_label_value(part.filesystem));
                out.append("\",mount_point=\"");
                out.append(escape_label_value(part.mount_point));
                out.append("\"} ");
                out.append(std::to_string(part.total_bytes));
                out.push_back('\n');
            }
        }
    }

    // 4. Network traffic: nodepulse_network_receive_bytes_total,
    // nodepulse_network_transmit_bytes_total (Counters)
    if (network_service_) {
        auto net = network_service_->get_network_metrics();
        if (net.has_value() && !net->empty()) {
            out.append(
                "# HELP nodepulse_network_receive_bytes_total Total network bytes received\n");
            out.append("# TYPE nodepulse_network_receive_bytes_total counter\n");
            for (const auto& iface : *net) {
                out.append("nodepulse_network_receive_bytes_total{interface=\"");
                out.append(escape_label_value(iface.name));
                out.append("\"} ");
                out.append(std::to_string(iface.rx_bytes));
                out.push_back('\n');
            }

            out.append(
                "# HELP nodepulse_network_transmit_bytes_total Total network bytes transmitted\n");
            out.append("# TYPE nodepulse_network_transmit_bytes_total counter\n");
            for (const auto& iface : *net) {
                out.append("nodepulse_network_transmit_bytes_total{interface=\"");
                out.append(escape_label_value(iface.name));
                out.append("\"} ");
                out.append(std::to_string(iface.tx_bytes));
                out.push_back('\n');
            }
        }
    }

    // 5. Agent Build Info: nodepulse_build_info (Gauge)
    out.append("# HELP nodepulse_build_info NodePulse agent build and version metadata\n");
    out.append("# TYPE nodepulse_build_info gauge\n");
    out.append("nodepulse_build_info{commit=\"");
    out.append(escape_label_value(kCommit));
    out.append("\",compiler=\"");
    out.append(escape_label_value(NP_COMPILER_NAME));
    out.append("\",version=\"");
    out.append(escape_label_value(kVersion));
    out.append("\"} 1\n");

    // 6. Agent Uptime: nodepulse_process_uptime_seconds (Counter)
    {
        double uptime =
            std::chrono::duration<double>(std::chrono::steady_clock::now() - start_time_).count();
        if (uptime < 0.0) {
            uptime = 0.0;
        }
        out.append(
            "# HELP nodepulse_process_uptime_seconds Total seconds NodePulse agent has been "
            "running\n");
        out.append("# TYPE nodepulse_process_uptime_seconds counter\n");
        out.append("nodepulse_process_uptime_seconds ");
        out.append(format_double(uptime));
        out.push_back('\n');
    }

    // 7. Agent Process Stats: RSS and CPU seconds
    ProcessSelfStats self_stats{};
    bool has_self_stats = false;
    if (self_stat_provider_) {
        auto opt = self_stat_provider_();
        if (opt.has_value()) {
            self_stats = *opt;
            has_self_stats = true;
        }
    } else {
        auto opt = read_default_self_stats();
        if (opt.has_value()) {
            self_stats = *opt;
            has_self_stats = true;
        }
    }

    if (has_self_stats) {
        out.append(
            "# HELP nodepulse_process_resident_memory_bytes Resident set size (RSS) of NodePulse "
            "process in bytes\n");
        out.append("# TYPE nodepulse_process_resident_memory_bytes gauge\n");
        out.append("nodepulse_process_resident_memory_bytes ");
        out.append(std::to_string(self_stats.rss_bytes));
        out.push_back('\n');

        out.append(
            "# HELP nodepulse_process_cpu_seconds_total Total CPU seconds consumed by NodePulse "
            "process\n");
        out.append("# TYPE nodepulse_process_cpu_seconds_total counter\n");
        out.append("nodepulse_process_cpu_seconds_total{mode=\"system\"} ");
        out.append(format_double(self_stats.cpu_system_seconds));
        out.push_back('\n');
        out.append("nodepulse_process_cpu_seconds_total{mode=\"user\"} ");
        out.append(format_double(self_stats.cpu_user_seconds));
        out.push_back('\n');
    }

    // 8. HTTP Requests: nodepulse_http_requests_total (Counter)
    {
        std::vector<std::pair<HttpRequestKey, uint64_t>> sorted_reqs;
        {
            std::lock_guard<std::mutex> lock(internal_metrics_mutex_);
            sorted_reqs.reserve(http_requests_.size());
            for (const auto& [k, v] : http_requests_) {
                sorted_reqs.emplace_back(k, v);
            }
        }
        std::sort(sorted_reqs.begin(), sorted_reqs.end(), [](const auto& a, const auto& b) {
            if (a.first.endpoint != b.first.endpoint) {
                return a.first.endpoint < b.first.endpoint;
            }
            if (a.first.method != b.first.method) {
                return a.first.method < b.first.method;
            }
            return a.first.status < b.first.status;
        });

        if (!sorted_reqs.empty()) {
            out.append(
                "# HELP nodepulse_http_requests_total Total number of HTTP requests processed\n");
            out.append("# TYPE nodepulse_http_requests_total counter\n");
            for (const auto& [k, v] : sorted_reqs) {
                out.append("nodepulse_http_requests_total{endpoint=\"");
                out.append(escape_label_value(k.endpoint));
                out.append("\",method=\"");
                out.append(escape_label_value(k.method));
                out.append("\",status=\"");
                out.append(std::to_string(k.status));
                out.append("\"} ");
                out.append(std::to_string(v));
                out.push_back('\n');
            }
        }
    }

    // 9. Collector Failures: nodepulse_collector_failures_total (Counter)
    {
        std::vector<std::pair<std::string, uint64_t>> sorted_fails;
        {
            std::lock_guard<std::mutex> lock(internal_metrics_mutex_);
            static const char* kDefaultCollectors[] = {"cpu",     "disk",    "docker",  "memory",
                                                       "network", "process", "service", "system"};
            for (const char* c : kDefaultCollectors) {
                auto it = collector_failures_.find(c);
                uint64_t val = (it != collector_failures_.end()) ? it->second : 0;
                sorted_fails.emplace_back(c, val);
            }
        }
        std::sort(sorted_fails.begin(), sorted_fails.end());

        out.append(
            "# HELP nodepulse_collector_failures_total Count of collector execution errors\n");
        out.append("# TYPE nodepulse_collector_failures_total counter\n");
        for (const auto& [collector, count] : sorted_fails) {
            out.append("nodepulse_collector_failures_total{collector=\"");
            out.append(escape_label_value(collector));
            out.append("\"} ");
            out.append(std::to_string(count));
            out.push_back('\n');
        }
    }

    // 10. SSE Active Connections: nodepulse_sse_active_connections (Gauge)
    if (stream_service_) {
        out.append(
            "# HELP nodepulse_sse_active_connections Current number of open SSE streaming "
            "clients\n");
        out.append("# TYPE nodepulse_sse_active_connections gauge\n");
        out.append("nodepulse_sse_active_connections ");
        out.append(std::to_string(stream_service_->client_count()));
        out.push_back('\n');
    }

    return out;
}

void MetricsExporter::export_metrics_async(std::function<void(std::string)> callback) const {
    if (!task_queue_) {
        callback(export_metrics());
        return;
    }
    task_queue_->runTaskInQueue([this, cb = std::move(callback)]() mutable {
        try {
            cb(export_metrics());
        } catch (...) {
            cb("");
        }
    });
}

}  // namespace nodepulse::services
