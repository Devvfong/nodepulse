#include <chrono>
#include <cmath>
#include <functional>
#include <string>

#include <drogon/HttpResponse.h>
#include <nlohmann/json.hpp>

#include <nodepulse/controllers/health_controller.hpp>

namespace nodepulse::controllers {

std::chrono::steady_clock::time_point HealthController::start_time_ =
    std::chrono::steady_clock::now();

void HealthController::get_health(const drogon::HttpRequestPtr& req,
                                  std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    auto now = std::chrono::steady_clock::now();
    double uptime = std::chrono::duration<double>(now - start_time_).count();
    uptime = std::round(uptime * 10.0) / 10.0;

    nlohmann::json body = {{"status", "healthy"}, {"version", "0.1.0"}, {"uptime_seconds", uptime}};

    auto resp = drogon::HttpResponse::newHttpResponse();
    resp->setStatusCode(drogon::k200OK);
    resp->setContentTypeCode(drogon::CT_APPLICATION_JSON);
    resp->setBody(body.dump());

    if (req && req->getAttributes()) {
        std::string req_id = req->getAttributes()->get<std::string>("request_id");
        if (!req_id.empty()) {
            resp->addHeader("X-Request-ID", req_id);
        }
    }

    callback(resp);
}

}  // namespace nodepulse::controllers
