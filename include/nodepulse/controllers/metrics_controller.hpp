#pragma once

#include <functional>
#include <memory>
#include <mutex>
#include <string>

#include <drogon/HttpController.h>

#include <nodepulse/config/config.hpp>
#include <nodepulse/services/metrics_exporter.hpp>

namespace nodepulse::controllers {

class MetricsController : public drogon::HttpController<MetricsController> {
  public:
    METHOD_LIST_BEGIN
    ADD_METHOD_TO(MetricsController::get_metrics, "/metrics", drogon::Get);
    METHOD_LIST_END

    void get_metrics(const drogon::HttpRequestPtr& req,
                     std::function<void(const drogon::HttpResponsePtr&)>&& callback);

    static void set_metrics_exporter(std::shared_ptr<services::MetricsExporter> exporter);
    [[nodiscard]] static std::shared_ptr<services::MetricsExporter> get_metrics_exporter();

    static void set_config(config::PrometheusConfig config);
    [[nodiscard]] static config::PrometheusConfig get_config();

  private:
    static std::shared_ptr<services::MetricsExporter> metrics_exporter_;
    static config::PrometheusConfig config_;
    static std::mutex mutex_;
};

}  // namespace nodepulse::controllers
