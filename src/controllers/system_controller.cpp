#include <functional>
#include <memory>
#include <string>

#include <drogon/HttpResponse.h>
#include <nlohmann/json.hpp>

#include <nodepulse/controllers/system_controller.hpp>
#include <nodepulse/services/system_service.hpp>
#include <nodepulse/utils/error_response.hpp>

namespace nodepulse::controllers {

std::shared_ptr<services::SystemService> SystemController::system_service_ = nullptr;

std::shared_ptr<services::SystemService> SystemController::get_system_service() {
    if (!system_service_) {
        system_service_ = std::make_shared<services::SystemService>();
    }
    return system_service_;
}

void SystemController::get_system(const drogon::HttpRequestPtr& req,
                                  std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    std::string req_id;
    if (req && req->getAttributes()) {
        req_id = req->getAttributes()->get<std::string>("request_id");
    }

    auto service = get_system_service();
    auto sys_info_opt = service->get_system_info();

    if (!sys_info_opt.has_value()) {
        nlohmann::json details = nlohmann::json::array(
            {{{"collector", "system_collector"}, {"target_file", "/proc/uptime"}}});
        auto err_resp = utils::make_error_response(
            drogon::k500InternalServerError, utils::error_codes::kCollectorFailure,
            "Failed to collect system metrics from /proc/uptime.", details, req_id);
        callback(err_resp);
        return;
    }

    const auto& info = *sys_info_opt;
    nlohmann::json body = {{"hostname", info.hostname},
                           {"os_name", info.os_name},
                           {"os_version", info.os_version},
                           {"kernel_version", info.kernel_version},
                           {"architecture", info.architecture},
                           {"boot_time_utc", info.boot_time_utc},
                           {"uptime_seconds", info.uptime_seconds}};

    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setStatusCode(drogon::k200OK);
    resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    resp->setBody(body.dump());

    if (!req_id.empty()) {
        resp->addHeader("X-Request-ID", req_id);
    }

    callback(resp);
}

}  // namespace nodepulse::controllers
