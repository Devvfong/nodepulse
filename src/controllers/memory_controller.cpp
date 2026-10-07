#include <functional>
#include <memory>
#include <mutex>
#include <string>

#include <drogon/HttpResponse.h>
#include <nlohmann/json.hpp>

#include <nodepulse/controllers/memory_controller.hpp>
#include <nodepulse/services/memory_service.hpp>
#include <nodepulse/utils/error_response.hpp>

namespace nodepulse::controllers {

std::shared_ptr<services::MemoryService> MemoryController::memory_service_ = nullptr;
std::mutex MemoryController::mutex_{};

void MemoryController::set_memory_service(std::shared_ptr<services::MemoryService> service) {
    std::lock_guard<std::mutex> lock(mutex_);
    memory_service_ = std::move(service);
}

std::shared_ptr<services::MemoryService> MemoryController::get_memory_service() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!memory_service_) {
        memory_service_ = std::make_shared<services::MemoryService>();
    }
    return memory_service_;
}

void MemoryController::get_memory(const drogon::HttpRequestPtr& req,
                                  std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    std::string req_id;
    if (req && req->getAttributes()) {
        req_id = req->getAttributes()->get<std::string>("request_id");
    }

    auto service = get_memory_service();
    auto metrics_opt = service->get_memory_metrics();

    if (!metrics_opt.has_value()) {
        nlohmann::json details = nlohmann::json::array(
            {{{"collector", "memory_collector"}, {"target_file", "/proc/meminfo"}}});
        auto err_resp = utils::make_error_response(
            drogon::k500InternalServerError, utils::error_codes::kCollectorFailure,
            "Failed to collect memory metrics from /proc/meminfo.", details, req_id);
        callback(err_resp);
        return;
    }

    const auto& metrics = *metrics_opt;
    nlohmann::json body = {{"total_bytes", metrics.total_bytes},
                           {"used_bytes", metrics.used_bytes},
                           {"free_bytes", metrics.free_bytes},
                           {"available_bytes", metrics.available_bytes},
                           {"buffers_bytes", metrics.buffers_bytes},
                           {"cached_bytes", metrics.cached_bytes},
                           {"usage_percent", metrics.usage_percent},
                           {"swap_total_bytes", metrics.swap_total_bytes},
                           {"swap_free_bytes", metrics.swap_free_bytes},
                           {"swap_used_bytes", metrics.swap_used_bytes},
                           {"swap_usage_percent", metrics.swap_usage_percent}};

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
