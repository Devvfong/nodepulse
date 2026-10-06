#include <algorithm>
#include <utility>

#include <trantor/utils/ConcurrentTaskQueue.h>

#include <nodepulse/services/service_manager_service.hpp>

namespace nodepulse::services {

ServiceManagerService::ServiceManagerService(
    std::shared_ptr<collectors::ServiceCollector> collector,
    std::shared_ptr<trantor::TaskQueue> task_queue)
    : collector_(std::move(collector)), task_queue_(std::move(task_queue)) {
    if (!collector_) {
        collector_ = std::make_shared<collectors::ServiceCollector>();
    }
    if (!task_queue_) {
        task_queue_ = std::make_shared<trantor::ConcurrentTaskQueue>(2, "service_worker");
    }
}

std::optional<std::vector<domain::ServiceInfo>> ServiceManagerService::list_services(
    const std::string& state_filter, int limit) {
    if (!collector_) {
        return std::nullopt;
    }
    auto result = collector_->list_services(state_filter);
    if (!result.has_value()) {
        return std::nullopt;
    }
    if (limit > 0 && static_cast<size_t>(limit) < result->size()) {
        result->resize(static_cast<size_t>(limit));
    }
    return result;
}

collectors::ServiceDetailResult ServiceManagerService::get_service_detail(const std::string& name) {
    if (!collector_) {
        return {collectors::ServiceStatusResult::kCollectorFailure, std::nullopt};
    }
    std::string normalized = collectors::ServiceCollector::normalize_service_name(name);
    return collector_->get_service_detail(normalized);
}

void ServiceManagerService::list_services_async(
    const std::string& state_filter, int limit,
    std::function<void(std::optional<std::vector<domain::ServiceInfo>>)> callback) {
    if (!task_queue_) {
        callback(list_services(state_filter, limit));
        return;
    }
    task_queue_->runTaskInQueue([this, state_filter, limit, callback = std::move(callback)]() {
        callback(list_services(state_filter, limit));
    });
}

void ServiceManagerService::get_service_detail_async(
    const std::string& name, std::function<void(collectors::ServiceDetailResult)> callback) {
    if (!task_queue_) {
        callback(get_service_detail(name));
        return;
    }
    task_queue_->runTaskInQueue(
        [this, name, callback = std::move(callback)]() { callback(get_service_detail(name)); });
}

}  // namespace nodepulse::services
