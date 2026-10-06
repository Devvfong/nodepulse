#pragma once

#include <functional>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <trantor/utils/TaskQueue.h>

#include <nodepulse/collectors/service_collector.hpp>
#include <nodepulse/domain/service_info.hpp>

namespace nodepulse::services {

class ServiceManagerService {
  public:
    explicit ServiceManagerService(
        std::shared_ptr<collectors::ServiceCollector> collector = nullptr,
        std::shared_ptr<trantor::TaskQueue> task_queue = nullptr);
    virtual ~ServiceManagerService() = default;

    ServiceManagerService(const ServiceManagerService&) = delete;
    ServiceManagerService& operator=(const ServiceManagerService&) = delete;

    [[nodiscard]] virtual std::optional<std::vector<domain::ServiceInfo>> list_services(
        const std::string& state_filter = "all", int limit = 50);

    [[nodiscard]] virtual collectors::ServiceDetailResult get_service_detail(
        const std::string& name);

    void list_services_async(
        const std::string& state_filter, int limit,
        std::function<void(std::optional<std::vector<domain::ServiceInfo>>)> callback);

    void get_service_detail_async(const std::string& name,
                                  std::function<void(collectors::ServiceDetailResult)> callback);

  private:
    std::shared_ptr<collectors::ServiceCollector> collector_;
    std::shared_ptr<trantor::TaskQueue> task_queue_;
};

}  // namespace nodepulse::services
