#pragma once

#include <functional>
#include <memory>
#include <string>

#include <drogon/HttpController.h>

#include <nodepulse/services/docker_service.hpp>

namespace nodepulse::controllers {

class DockerController : public drogon::HttpController<DockerController> {
  public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(DockerController::get_containers, "/api/v1/containers", drogon::Get);
    ADD_METHOD_TO(DockerController::get_container_by_id, "/api/v1/containers/{1}", drogon::Get);
    METHOD_LIST_END

    void get_containers(const drogon::HttpRequestPtr& req,
                        std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    void get_container_by_id(const drogon::HttpRequestPtr& req,
                             std::function<void(const drogon::HttpResponsePtr&)>&& callback,
                             std::string id_param);

    static void set_docker_service(std::shared_ptr<services::DockerService> service) {
        docker_service_ = std::move(service);
    }

    [[nodiscard]] static std::shared_ptr<services::DockerService> get_docker_service();

  private:
    static std::shared_ptr<services::DockerService> docker_service_;
};

}  // namespace nodepulse::controllers
