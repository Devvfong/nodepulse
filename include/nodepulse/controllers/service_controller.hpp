#pragma once

#include <functional>
#include <memory>
#include <string>

#include <drogon/HttpController.h>

#include <nodepulse/services/service_manager_service.hpp>

namespace nodepulse::controllers {

class ServiceController : public drogon::HttpController<ServiceController> {
  public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(ServiceController::get_services, "/api/v1/services", drogon::Get);
    ADD_METHOD_TO(ServiceController::get_service_by_name, "/api/v1/services/{1}", drogon::Get);
    METHOD_LIST_END

    void get_services(const drogon::HttpRequestPtr& req,
                      std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void get_service_by_name(const drogon::HttpRequestPtr& req,
                             std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                             std::string name_param);

    static void set_service_manager_service(
        std::shared_ptr<services::ServiceManagerService> service) {
        service_manager_service_ = std::move(service);
    }

    [[nodiscard]] static std::shared_ptr<services::ServiceManagerService>
    get_service_manager_service();

  private:
    static std::shared_ptr<services::ServiceManagerService> service_manager_service_;
};

}  // namespace nodepulse::controllers
