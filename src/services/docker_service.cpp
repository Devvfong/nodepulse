#include <memory>
#include <utility>

#include <trantor/utils/ConcurrentTaskQueue.h>

#include <nodepulse/services/docker_service.hpp>

namespace nodepulse::services {

DockerService::DockerService(std::shared_ptr<collectors::DockerCollector> collector,
                             std::shared_ptr<trantor::TaskQueue> task_queue)
    : collector_(std::move(collector)), task_queue_(std::move(task_queue)) {
    if (!collector_) {
        collector_ = std::make_shared<collectors::DockerCollector>();
    }
    if (!task_queue_) {
        task_queue_ = std::make_shared<trantor::ConcurrentTaskQueue>(2, "docker_worker");
    }
}

collectors::DockerContainersResult DockerService::list_containers() {
    if (!collector_) {
        return {collectors::DockerStatusResult::kUnavailable, std::nullopt,
                "Docker collector is not initialized"};
    }
    return collector_->list_containers();
}

collectors::DockerContainerDetailResult DockerService::get_container(const std::string& id) {
    if (!collector_) {
        return {collectors::DockerStatusResult::kUnavailable, std::nullopt,
                "Docker collector is not initialized"};
    }
    return collector_->get_container(id);
}

void DockerService::list_containers_async(
    std::function<void(collectors::DockerContainersResult)> callback) {
    if (!task_queue_) {
        callback(list_containers());
        return;
    }
    task_queue_->runTaskInQueue(
        [this, cb = std::move(callback)]() mutable { cb(list_containers()); });
}

void DockerService::get_container_async(
    const std::string& id, std::function<void(collectors::DockerContainerDetailResult)> callback) {
    if (!task_queue_) {
        callback(get_container(id));
        return;
    }
    task_queue_->runTaskInQueue(
        [this, id, cb = std::move(callback)]() mutable { cb(get_container(id)); });
}

}  // namespace nodepulse::services
