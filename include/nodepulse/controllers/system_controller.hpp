#pragma once

#include <functional>
#include <memory>

#include <drogon/HttpController.h>

#include <nodepulse/services/system_service.hpp>

namespace nodepulse::controllers {

class SystemController : public drogon::HttpController<SystemController> {
  public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(SystemController::get_system, "/api/v1/system", drogon::Get);
    METHOD_LIST_END

    void get_system(const drogon::HttpRequestPtr& req,
                    std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    static void set_system_service(std::shared_ptr<services::SystemService> service) {
        system_service_ = std::move(service);
    }

    [[nodiscard]] static std::shared_ptr<services::SystemService> get_system_service();

  private:
    static std::shared_ptr<services::SystemService> system_service_;
};

}  // namespace nodepulse::controllers
