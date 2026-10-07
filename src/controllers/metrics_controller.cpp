#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <utility>

#include <drogon/HttpResponse.h>
#include <drogon/utils/Utilities.h>
#include <nlohmann/json.hpp>

#include <nodepulse/controllers/metrics_controller.hpp>
#include <nodepulse/utils/error_response.hpp>

namespace nodepulse::controllers {

std::shared_ptr<services::MetricsExporter> MetricsController::metrics_exporter_ = nullptr;
config::PrometheusConfig MetricsController::config_{};
std::mutex MetricsController::mutex_{};

void MetricsController::set_metrics_exporter(std::shared_ptr<services::MetricsExporter> exporter) {
    std::lock_guard<std::mutex> lock(mutex_);
    metrics_exporter_ = std::move(exporter);
}

std::shared_ptr<services::MetricsExporter> MetricsController::get_metrics_exporter() {
    std::lock_guard<std::mutex> lock(mutex_);
    return metrics_exporter_;
}

void MetricsController::set_config(config::PrometheusConfig config) {
    std::lock_guard<std::mutex> lock(mutex_);
    config_ = config;
}

config::PrometheusConfig MetricsController::get_config() {
    std::lock_guard<std::mutex> lock(mutex_);
    return config_;
}

void MetricsController::get_metrics(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    std::string req_id;
    if (req && req->getAttributes()) {
        req_id = req->getAttributes()->get<std::string>("request_id");
    }
    if (req_id.empty() && req) {
        req_id = req->getHeader("X-Request-ID");
        if (req_id.empty()) {
            req_id = req->getHeader("x-request-id");
        }
    }
    if (req_id.empty()) {
        req_id = drogon::utils::getUuid();
    }

    auto cfg = get_config();
    auto exporter = get_metrics_exporter();

    if (!cfg.enabled || !exporter || !exporter->is_enabled()) {
        auto resp = utils::make_error_response(
            drogon::k503ServiceUnavailable, utils::error_codes::kServiceUnavailable,
            "Prometheus metrics exposition is disabled by configuration.", nlohmann::json::array(),
            req_id);
        callback(resp);
        return;
    }

    bool enqueued = exporter->export_metrics_async([callback, req_id](std::string body) {
        auto resp = drogon::HttpResponse::newHttpResponse();
        resp->setStatusCode(drogon::k200OK);
        resp->setContentTypeCodeAndCustomString(drogon::CT_CUSTOM,
                                                "text/plain; version=0.0.4; charset=utf-8");
        resp->setBody(std::move(body));
        resp->addHeader("X-Request-ID", req_id);
        callback(resp);
    });

    if (!enqueued) {
        auto resp = utils::make_error_response(
            drogon::k503ServiceUnavailable, utils::error_codes::kServiceUnavailable,
            "Metrics exporter worker queue is saturated.", nlohmann::json::array(), req_id);
        callback(resp);
    }
}

}  // namespace nodepulse::controllers
