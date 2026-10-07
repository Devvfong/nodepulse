#pragma once

#include <functional>
#include <memory>
#include <string>
#include <vector>

#include <trantor/utils/TaskQueue.h>

#include <nodepulse/collectors/docker_collector.hpp>
#include <nodepulse/domain/container_info.hpp>

namespace nodepulse::services {

class DockerService {
  public:
    explicit DockerService(std::shared_ptr<collectors::DockerCollector> collector = nullptr,
                           std::shared_ptr<trantor::TaskQueue> task_queue = nullptr);
    virtual ~DockerService() = default;

    DockerService(const DockerService&) = delete;
    DockerService& operator=(const DockerService&) = delete;

    [[nodiscard]] virtual collectors::DockerContainersResult list_containers();
    [[nodiscard]] virtual collectors::DockerContainerDetailResult get_container(
        const std::string& id);

    [[nodiscard]] bool list_containers_async(
        std::function<void(collectors::DockerContainersResult)> callback);
    [[nodiscard]] bool get_container_async(
        const std::string& id,
        std::function<void(collectors::DockerContainerDetailResult)> callback);

    [[nodiscard]] const std::string& socket_path() const noexcept {
        return collector_ ? collector_->socket_path() : default_socket_path_;
    }

  private:
    std::shared_ptr<collectors::DockerCollector> collector_;
    std::shared_ptr<trantor::TaskQueue> task_queue_;
    inline static const std::string default_socket_path_ = "/var/run/docker.sock";
};

}  // namespace nodepulse::services
