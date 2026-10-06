#include <functional>
#include <memory>
#include <string>

#include <drogon/HttpResponse.h>
#include <nlohmann/json.hpp>

#include <nodepulse/controllers/network_controller.hpp>
#include <nodepulse/services/network_service.hpp>
#include <nodepulse/utils/error_response.hpp>

namespace nodepulse::controllers {

std::shared_ptr<services::NetworkService> NetworkController::network_service_ = nullptr;

std::shared_ptr<services::NetworkService> NetworkController::get_network_service() {
    if (!network_service_) {
        network_service_ = std::make_shared<services::NetworkService>();
    }
    return network_service_;
}

void NetworkController::get_network(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    std::string req_id;
    if (req && req->getAttributes()) {
        req_id = req->getAttributes()->get<std::string>("request_id");
    }

    auto service = get_network_service();
    auto metrics_opt = service->get_network_metrics();

    if (!metrics_opt.has_value()) {
        nlohmann::json details = nlohmann::json::array(
            {{{"collector", "network_collector"}, {"target_file", "/proc/net/dev"}}});
        auto err_resp = utils::make_error_response(
            drogon::k500InternalServerError, utils::error_codes::kCollectorFailure,
            "Failed to collect network metrics from /proc/net/dev.", details, req_id);
        callback(err_resp);
        return;
    }

    nlohmann::json body = nlohmann::json::array();
    for (const auto& iface : *metrics_opt) {
        nlohmann::json iface_json = {
            {"name", iface.name},
            {"mac_address", iface.mac_address},
            {"operstate", iface.operstate},
            {"speed_mbps", iface.speed_mbps},
            {"rx_bytes", iface.rx_bytes},
            {"tx_bytes", iface.tx_bytes},
            {"rx_packets", iface.rx_packets},
            {"tx_packets", iface.tx_packets},
            {"rx_errors", iface.rx_errors},
            {"tx_errors", iface.tx_errors},
            {"rx_bytes_per_sec", iface.rx_bytes_per_sec.has_value()
                                     ? nlohmann::json(*iface.rx_bytes_per_sec)
                                     : nlohmann::json(nullptr)},
            {"tx_bytes_per_sec", iface.tx_bytes_per_sec.has_value()
                                     ? nlohmann::json(*iface.tx_bytes_per_sec)
                                     : nlohmann::json(nullptr)}};
        body.push_back(std::move(iface_json));
    }

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
