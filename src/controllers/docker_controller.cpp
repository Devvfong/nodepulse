#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <drogon/HttpResponse.h>
#include <nlohmann/json.hpp>

#include <nodepulse/collectors/docker_collector.hpp>
#include <nodepulse/controllers/docker_controller.hpp>
#include <nodepulse/services/docker_service.hpp>
#include <nodepulse/utils/error_response.hpp>

namespace nodepulse::controllers {

std::shared_ptr<services::DockerService> DockerController::docker_service_ = nullptr;

std::shared_ptr<services::DockerService> DockerController::get_docker_service() {
    if (!docker_service_) {
        docker_service_ = std::make_shared<services::DockerService>();
    }
    return docker_service_;
}

void DockerController::get_containers(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    std::string req_id;
    if (req && req->getAttributes()) {
        req_id = req->getAttributes()->get<std::string>("request_id");
    }

    auto service = get_docker_service();
    std::string socket_path = service->socket_path();

    service->list_containers_async([callback = std::move(callback), req_id, socket_path](auto res) {
        if (res.status == collectors::DockerStatusResult::kUnavailable) {
            nlohmann::json details = nlohmann::json::array({{{"socket_path", socket_path}}});
            auto err_resp = utils::make_error_response(
                drogon::k503ServiceUnavailable, utils::error_codes::kDockerUnavailable,
                "Docker daemon is not running or socket " + socket_path + " is inaccessible.",
                details, req_id);
            callback(err_resp);
            return;
        }

        if (res.status == collectors::DockerStatusResult::kCollectorFailure ||
            !res.containers.has_value()) {
            nlohmann::json details = nlohmann::json::array(
                {{{"collector", "docker_collector"}, {"reason", res.error_message}}});
            auto err_resp = utils::make_error_response(
                drogon::k500InternalServerError, utils::error_codes::kCollectorFailure,
                "Failed to collect Docker container information.", details, req_id);
            callback(err_resp);
            return;
        }

        nlohmann::json body = nlohmann::json::array();
        for (const auto& c : *res.containers) {
            body.push_back({{"id", c.id},
                            {"names", c.names},
                            {"image", c.image},
                            {"status", c.status},
                            {"state", c.state},
                            {"created", c.created}});
        }

        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k200OK);
        resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
        resp->setBody(body.dump());

        if (!req_id.empty()) {
            resp->addHeader("X-Request-ID", req_id);
        }

        callback(resp);
    });
}

void DockerController::get_container_by_id(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback, std::string id_param) {
    std::string req_id;
    if (req && req->getAttributes()) {
        req_id = req->getAttributes()->get<std::string>("request_id");
    }

    if (!collectors::DockerCollector::is_valid_container_id(id_param)) {
        nlohmann::json details =
            nlohmann::json::array({{{"field", "id"}, {"reason", "invalid_identifier"}}});
        auto err_resp = utils::make_error_response(
            drogon::k400BadRequest, utils::error_codes::kInvalidRequest,
            "Container ID must be a valid alphanumeric identifier.", details, req_id);
        callback(err_resp);
        return;
    }

    auto service = get_docker_service();
    std::string socket_path = service->socket_path();

    service->get_container_async(id_param, [callback = std::move(callback), req_id, socket_path,
                                            id_param](auto res) {
        if (res.status == collectors::DockerStatusResult::kNotFound) {
            nlohmann::json details =
                nlohmann::json::array({{{"resource_type", "container"}, {"identifier", id_param}}});
            auto err_resp = utils::make_error_response(
                drogon::k404NotFound, utils::error_codes::kResourceNotFound,
                "Container '" + id_param + "' was not found.", details, req_id);
            callback(err_resp);
            return;
        }

        if (res.status == collectors::DockerStatusResult::kUnavailable) {
            nlohmann::json details = nlohmann::json::array({{{"socket_path", socket_path}}});
            auto err_resp = utils::make_error_response(
                drogon::k503ServiceUnavailable, utils::error_codes::kDockerUnavailable,
                "Docker daemon is not running or socket " + socket_path + " is inaccessible.",
                details, req_id);
            callback(err_resp);
            return;
        }

        if (res.status == collectors::DockerStatusResult::kCollectorFailure ||
            !res.detail.has_value()) {
            nlohmann::json details = nlohmann::json::array(
                {{{"collector", "docker_collector"}, {"reason", res.error_message}}});
            auto err_resp = utils::make_error_response(
                drogon::k500InternalServerError, utils::error_codes::kCollectorFailure,
                "Failed to collect Docker container details.", details, req_id);
            callback(err_resp);
            return;
        }

        const auto& d = *res.detail;
        nlohmann::json body = {{"id", d.id},
                               {"name", d.name},
                               {"image", d.image},
                               {"status", d.status},
                               {"state", d.state},
                               {"running", d.running},
                               {"exit_code", d.exit_code},
                               {"port_mappings", d.port_mappings},
                               {"mount_sources", d.mount_sources},
                               {"created", d.created}};

        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k200OK);
        resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
        resp->setBody(body.dump());

        if (!req_id.empty()) {
            resp->addHeader("X-Request-ID", req_id);
        }

        callback(resp);
    });
}

}  // namespace nodepulse::controllers
