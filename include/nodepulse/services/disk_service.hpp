#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include <trantor/utils/TaskQueue.h>

#include <nodepulse/collectors/disk_collector.hpp>
#include <nodepulse/domain/disk_info.hpp>

namespace nodepulse::services {

class DiskService {
  public:
    using MetricsCallback =
        std::function<void(std::optional<std::vector<domain::DiskPartitionMetrics>>)>;

    explicit DiskService(std::shared_ptr<collectors::DiskCollector> collector =
                             std::make_shared<collectors::DiskCollector>(),
                         std::shared_ptr<trantor::TaskQueue> task_queue = nullptr);
    ~DiskService() = default;

    DiskService(const DiskService&) = delete;
    DiskService& operator=(const DiskService&) = delete;
    DiskService(DiskService&&) = default;
    DiskService& operator=(DiskService&&) = default;

    [[nodiscard]] std::optional<std::vector<domain::DiskPartitionMetrics>> get_disk_metrics() const;

    void get_disk_metrics_async(MetricsCallback callback) const;

  private:
    std::shared_ptr<collectors::DiskCollector> collector_;
    std::shared_ptr<trantor::TaskQueue> task_queue_;
};

}  // namespace nodepulse::services
