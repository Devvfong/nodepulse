#pragma once

#include <chrono>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <trantor/utils/ConcurrentTaskQueue.h>

#include <nodepulse/config/config.hpp>
#include <nodepulse/services/cpu_service.hpp>
#include <nodepulse/services/disk_service.hpp>
#include <nodepulse/services/memory_service.hpp>
#include <nodepulse/services/network_service.hpp>
#include <nodepulse/services/stream_service.hpp>

namespace nodepulse::services {

struct ProcessSelfStats {
    uint64_t rss_bytes{0};
    double cpu_user_seconds{0.0};
    double cpu_system_seconds{0.0};
};

struct HttpRequestKey {
    std::string endpoint;
    std::string method;
    int status{0};

    bool operator==(const HttpRequestKey& other) const noexcept {
        return endpoint == other.endpoint && method == other.method && status == other.status;
    }
};

struct HttpRequestKeyHash {
    size_t operator()(const HttpRequestKey& k) const noexcept {
        size_t h1 = std::hash<std::string>{}(k.endpoint);
        size_t h2 = std::hash<std::string>{}(k.method);
        size_t h3 = std::hash<int>{}(k.status);
        return h1 ^ (h2 << 1) ^ (h3 << 2);
    }
};

class MetricsExporter {
  public:
    using SelfStatProvider = std::function<std::optional<ProcessSelfStats>()>;

    explicit MetricsExporter(std::shared_ptr<CpuService> cpu_service = nullptr,
                             std::shared_ptr<MemoryService> memory_service = nullptr,
                             std::shared_ptr<DiskService> disk_service = nullptr,
                             std::shared_ptr<NetworkService> network_service = nullptr,
                             std::shared_ptr<StreamService> stream_service = nullptr,
                             config::PrometheusConfig config = {},
                             std::shared_ptr<trantor::TaskQueue> task_queue = nullptr);

    [[nodiscard]] std::string export_metrics() const;
    void export_metrics_async(std::function<void(std::string)> callback) const;

    [[nodiscard]] bool is_enabled() const noexcept;
    void set_enabled(bool enabled) noexcept;

    void record_http_request(std::string_view raw_path, std::string_view method, int status);
    void record_collector_failure(std::string_view collector_name);
    void reset_internal_metrics();

    void set_self_stat_provider(SelfStatProvider provider);
    void set_start_time(std::chrono::steady_clock::time_point start_time);

    static std::string normalize_endpoint(std::string_view raw_path);
    static std::string escape_label_value(std::string_view val);

  private:
    std::shared_ptr<CpuService> cpu_service_;
    std::shared_ptr<MemoryService> memory_service_;
    std::shared_ptr<DiskService> disk_service_;
    std::shared_ptr<NetworkService> network_service_;
    std::shared_ptr<StreamService> stream_service_;
    config::PrometheusConfig config_;
    std::shared_ptr<trantor::TaskQueue> task_queue_;

    mutable std::mutex internal_metrics_mutex_;
    std::unordered_map<HttpRequestKey, uint64_t, HttpRequestKeyHash> http_requests_;
    std::unordered_map<std::string, uint64_t> collector_failures_;

    std::chrono::steady_clock::time_point start_time_;
    SelfStatProvider self_stat_provider_;
};

}  // namespace nodepulse::services
