#include <functional>
#include <memory>
#include <string>

#include <drogon/HttpResponse.h>
#include <nlohmann/json.hpp>

#include <nodepulse/controllers/cpu_controller.hpp>
#include <nodepulse/services/cpu_service.hpp>
#include <nodepulse/utils/error_response.hpp>

namespace nodepulse::controllers {

std::shared_ptr<services::CpuService> CpuController::cpu_service_ = nullptr;

std::shared_ptr<services::CpuService> CpuController::get_cpu_service() {
    if (!cpu_service_) {
        cpu_service_ = std::make_shared<services::CpuService>();
    }
    return cpu_service_;
}

void CpuController::get_cpu(const drogon::HttpRequestPtr& req,
                            std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    std::string req_id;
    if (req && req->getAttributes()) {
        req_id = req->getAttributes()->get<std::string>("request_id");
    }

    auto service = get_cpu_service();
    auto metrics_opt = service->get_cpu_metrics();

    if (!metrics_opt.has_value()) {
        nlohmann::json details = nlohmann::json::array(
            {{{"collector", "cpu_collector"}, {"target_file", "/proc/stat"}}});
        auto err_resp = utils::make_error_response(
            drogon::k500InternalServerError, utils::error_codes::kCollectorFailure,
            "Failed to collect CPU metrics from /proc/stat.", details, req_id);
        callback(err_resp);
        return;
    }

    const auto& metrics = *metrics_opt;

    nlohmann::json cores_json = nlohmann::json::array();
    for (const auto& core : metrics.cores) {
        cores_json.push_back(
            {{"core_id", core.core_id},
             {"usage_percent", core.usage_percent.has_value() ? nlohmann::json(*core.usage_percent)
                                                              : nlohmann::json(nullptr)}});
    }

    nlohmann::json body = {
        {"usage_percent", metrics.usage_percent.has_value() ? nlohmann::json(*metrics.usage_percent)
                                                            : nlohmann::json(nullptr)},
        {"measurement_status", metrics.measurement_status},
        {"model_name", metrics.model_name},
        {"physical_cores", metrics.physical_cores},
        {"logical_cores", metrics.logical_cores},
        {"load_average",
         {{"one_minute", metrics.load_average.one_minute},
          {"five_minute", metrics.load_average.five_minute},
          {"fifteen_minute", metrics.load_average.fifteen_minute}}},
        {"cores", cores_json}};

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
