#include <memory>
#include <utility>

#include <nodepulse/services/docker_service.hpp>
#include <nodepulse/utils/bounded_task_queue.hpp>

namespace nodepulse::services {

DockerService::DockerService(std::shared_ptr<collectors::DockerCollector> collector,
                             std::shared_ptr<trantor::TaskQueue> task_queue)
    : collector_(std::move(collector)), task_queue_(std::move(task_queue)) {
    if (!collector_) {
        collector_ = std::make_shared<collectors::DockerCollector>();
    }
    if (!task_queue_) {
        task_queue_ = std::make_shared<utils::BoundedTaskQueue>(
            2, utils::BoundedTaskQueue::kDefaultMaxQueueSize, "docker_worker");
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

bool DockerService::list_containers_async(
    std::function<void(collectors::DockerContainersResult)> callback) {
    if (!task_queue_) {
        callback(list_containers());
        return true;
    }
    auto bounded_q = std::dynamic_pointer_cast<utils::BoundedTaskQueue>(task_queue_);
    if (bounded_q) {
        return bounded_q->tryRunTaskInQueue(
            [this, cb = std::move(callback)]() mutable { cb(list_containers()); });
    }
    task_queue_->runTaskInQueue(
        [this, cb = std::move(callback)]() mutable { cb(list_containers()); });
    return true;
}

bool DockerService::get_container_async(
    const std::string& id, std::function<void(collectors::DockerContainerDetailResult)> callback) {
    if (!task_queue_) {
        callback(get_container(id));
        return true;
    }
    auto bounded_q = std::dynamic_pointer_cast<utils::BoundedTaskQueue>(task_queue_);
    if (bounded_q) {
        return bounded_q->tryRunTaskInQueue(
            [this, id, cb = std::move(callback)]() mutable { cb(get_container(id)); });
    }
    task_queue_->runTaskInQueue(
        [this, id, cb = std::move(callback)]() mutable { cb(get_container(id)); });
    return true;
}

}  // namespace nodepulse::services
