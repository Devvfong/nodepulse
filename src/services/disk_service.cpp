#include <functional>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include <nodepulse/collectors/disk_collector.hpp>
#include <nodepulse/domain/disk_info.hpp>
#include <nodepulse/services/disk_service.hpp>
#include <nodepulse/utils/bounded_task_queue.hpp>

namespace nodepulse::services {

DiskService::DiskService(std::shared_ptr<collectors::DiskCollector> collector,
                         std::shared_ptr<trantor::TaskQueue> task_queue)
    : collector_(std::move(collector)), task_queue_(std::move(task_queue)) {
    if (!collector_) {
        collector_ = std::make_shared<collectors::DiskCollector>();
    }
    if (!task_queue_) {
        task_queue_ = std::make_shared<utils::BoundedTaskQueue>(
            2, utils::BoundedTaskQueue::kDefaultMaxQueueSize, "disk_worker");
    }
}

std::optional<std::vector<domain::DiskPartitionMetrics>> DiskService::get_disk_metrics() const {
    return collector_->collect();
}

bool DiskService::get_disk_metrics_async(MetricsCallback callback) const {
    if (!task_queue_) {
        callback(get_disk_metrics());
        return true;
    }

    auto bounded_q = std::dynamic_pointer_cast<utils::BoundedTaskQueue>(task_queue_);
    if (bounded_q) {
        auto collector = collector_;
        return bounded_q->tryRunTaskInQueue([collector, cb = std::move(callback)]() mutable {
            try {
                if (!collector) {
                    cb(std::nullopt);
                    return;
                }
                cb(collector->collect());
            } catch (...) {
                cb(std::nullopt);
            }
        });
    }

    auto collector = collector_;
    task_queue_->runTaskInQueue([collector, cb = std::move(callback)]() mutable {
        try {
            if (!collector) {
                cb(std::nullopt);
                return;
            }
            cb(collector->collect());
        } catch (...) {
            cb(std::nullopt);
        }
    });
    return true;
}

}  // namespace nodepulse::services
