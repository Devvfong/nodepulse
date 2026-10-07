#include <functional>
#include <memory>
#include <mutex>
#include <string>

#include <drogon/HttpResponse.h>
#include <nlohmann/json.hpp>

#include <nodepulse/controllers/disk_controller.hpp>
#include <nodepulse/services/disk_service.hpp>
#include <nodepulse/utils/error_response.hpp>

namespace nodepulse::controllers {

std::shared_ptr<services::DiskService> DiskController::disk_service_ = nullptr;
std::mutex DiskController::mutex_{};

void DiskController::set_disk_service(std::shared_ptr<services::DiskService> service) {
    std::lock_guard<std::mutex> lock(mutex_);
    disk_service_ = std::move(service);
}

std::shared_ptr<services::DiskService> DiskController::get_disk_service() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!disk_service_) {
        disk_service_ = std::make_shared<services::DiskService>();
    }
    return disk_service_;
}

void DiskController::get_disks(const drogon::HttpRequestPtr& req,
                               std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    std::string req_id;
    if (req && req->getAttributes()) {
        req_id = req->getAttributes()->get<std::string>("request_id");
    }

    auto service = get_disk_service();
    bool enqueued = service->get_disk_metrics_async([callback, req_id](auto metrics_opt) {
        if (!metrics_opt.has_value()) {
            nlohmann::json details = nlohmann::json::array(
                {{{"collector", "disk_collector"}, {"target_file", "/proc/mounts"}}});
            auto err_resp = utils::make_error_response(
                drogon::k500InternalServerError, utils::error_codes::kCollectorFailure,
                "Failed to collect disk metrics from /proc/mounts.", details, req_id);
            callback(err_resp);
            return;
        }

        nlohmann::json body = nlohmann::json::array();
        for (const auto& partition : *metrics_opt) {
            body.push_back({{"filesystem", partition.filesystem},
                            {"mount_point", partition.mount_point},
                            {"fstype", partition.fstype},
                            {"total_bytes", partition.total_bytes},
                            {"used_bytes", partition.used_bytes},
                            {"free_bytes", partition.free_bytes},
                            {"available_bytes", partition.available_bytes},
                            {"usage_percent", partition.usage_percent},
                            {"inodes_total", partition.inodes_total},
                            {"inodes_free", partition.inodes_free}});
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

    if (!enqueued) {
        auto err_resp = utils::make_error_response(
            drogon::k503ServiceUnavailable, utils::error_codes::kServiceUnavailable,
            "Disk worker queue is saturated.", nlohmann::json::array(), req_id);
        callback(err_resp);
    }
}

}  // namespace nodepulse::controllers
