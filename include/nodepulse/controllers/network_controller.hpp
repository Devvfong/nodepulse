#pragma once

#include <functional>
#include <memory>

#include <drogon/HttpController.h>

#include <nodepulse/services/network_service.hpp>

namespace nodepulse::controllers {

class NetworkController : public drogon::HttpController<NetworkController> {
  public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(NetworkController::get_network, "/api/v1/network", drogon::Get);
    METHOD_LIST_END

    void get_network(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    static void set_network_service(std::shared_ptr<services::NetworkService> service) {
        network_service_ = std::move(service);
    }

    [[nodiscard]] static std::shared_ptr<services::NetworkService> get_network_service();

  private:
    static std::shared_ptr<services::NetworkService> network_service_;
};

}  // namespace nodepulse::controllers
