#include <charconv>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <drogon/HttpResponse.h>
#include <nlohmann/json.hpp>

#include <nodepulse/collectors/service_collector.hpp>
#include <nodepulse/controllers/service_controller.hpp>
#include <nodepulse/services/service_manager_service.hpp>
#include <nodepulse/utils/error_response.hpp>

namespace nodepulse::controllers {

std::shared_ptr<services::ServiceManagerService> ServiceController::service_manager_service_ =
    nullptr;

std::shared_ptr<services::ServiceManagerService> ServiceController::get_service_manager_service() {
    if (!service_manager_service_) {
        service_manager_service_ = std::make_shared<services::ServiceManagerService>();
    }
    return service_manager_service_;
}

namespace {

bool parse_integer(std::string_view s, int64_t& out) noexcept {
    if (s.empty()) {
        return false;
    }
    auto [ptr, ec] = std::from_chars(s.data(), s.data() + s.size(), out);
    return ec == std::errc{} && ptr == s.data() + s.size();
}

}  // namespace

void ServiceController::get_services(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    std::string req_id;
    if (req && req->getAttributes()) {
        req_id = req->getAttributes()->get<std::string>("request_id");
    }

    std::string state_param = "all";
    auto state_opt = req->getParameter("state");
    if (!state_opt.empty()) {
        if (state_opt != "active" && state_opt != "inactive" && state_opt != "failed" &&
            state_opt != "all") {
            nlohmann::json details = nlohmann::json::array(
                {{{"field", "state"}, {"reason", "must_be_one_of_active_inactive_failed_all"}}});
            auto err_resp = utils::make_error_response(
                drogon::k400BadRequest, utils::error_codes::kInvalidRequest,
                "Invalid state filter. Allowed values: active, inactive, failed, all.", details,
                req_id);
            callback(err_resp);
            return;
        }
        state_param = state_opt;
    }

    int limit_val = 50;
    auto limit_opt = req->getParameter("limit");
    if (!limit_opt.empty()) {
        int64_t parsed_limit = 0;
        if (!parse_integer(limit_opt, parsed_limit) || parsed_limit < 1 || parsed_limit > 200) {
            nlohmann::json details = nlohmann::json::array(
                {{{"field", "limit"}, {"reason", "must_be_between_1_and_200"}}});
            auto err_resp = utils::make_error_response(
                drogon::k400BadRequest, utils::error_codes::kInvalidRequest,
                "Invalid limit parameter. Must be between 1 and 200.", details, req_id);
            callback(err_resp);
            return;
        }
        limit_val = static_cast<int>(parsed_limit);
    }

    auto service = get_service_manager_service();
    service->list_services_async(
        state_param, limit_val, [callback = std::move(callback), req_id](auto services_opt) {
            if (!services_opt.has_value()) {
                nlohmann::json details =
                    nlohmann::json::array({{{"collector", "service_collector"},
                                            {"reason", "dbus_communication_failure"}}});
                auto err_resp = utils::make_error_response(
                    drogon::k500InternalServerError, utils::error_codes::kCollectorFailure,
                    "Failed to communicate with systemd via D-Bus.", details, req_id);
                callback(err_resp);
                return;
            }

            nlohmann::json body = nlohmann::json::array();
            for (const auto& s : *services_opt) {
                body.push_back({{"name", s.name},
                                {"description", s.description},
                                {"load_state", s.load_state},
                                {"active_state", s.active_state},
                                {"sub_state", s.sub_state},
                                {"unit_file_state", s.unit_file_state}});
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

void ServiceController::get_service_by_name(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback, std::string name_param) {
    std::string req_id;
    if (req && req->getAttributes()) {
        req_id = req->getAttributes()->get<std::string>("request_id");
    }

    if (!collectors::ServiceCollector::is_valid_unit_name(name_param)) {
        nlohmann::json details =
            nlohmann::json::array({{{"field", "name"}, {"reason", "invalid_format"}}});
        auto err_resp = utils::make_error_response(
            drogon::k400BadRequest, utils::error_codes::kInvalidRequest,
            "Invalid service name. Must match '^[a-zA-Z0-9_\\-\\.\\@]+$'.", details, req_id);
        callback(err_resp);
        return;
    }

    auto service = get_service_manager_service();
    service->get_service_detail_async(name_param, [callback = std::move(callback), req_id,
                                                   name_param](auto res) {
        if (res.status == collectors::ServiceStatusResult::kNotFound) {
            nlohmann::json details =
                nlohmann::json::array({{{"resource_type", "service"}, {"identifier", name_param}}});
            auto err_resp = utils::make_error_response(
                drogon::k404NotFound, utils::error_codes::kResourceNotFound,
                "Service '" + name_param + "' was not found.", details, req_id);
            callback(err_resp);
            return;
        }

        if (res.status == collectors::ServiceStatusResult::kCollectorFailure ||
            !res.detail.has_value()) {
            nlohmann::json details = nlohmann::json::array(
                {{{"collector", "service_collector"}, {"reason", "dbus_communication_failure"}}});
            auto err_resp = utils::make_error_response(
                drogon::k500InternalServerError, utils::error_codes::kCollectorFailure,
                "Failed to communicate with systemd via D-Bus.", details, req_id);
            callback(err_resp);
            return;
        }

        const auto& d = *res.detail;
        nlohmann::json body = {{"name", d.name},
                               {"description", d.description},
                               {"load_state", d.load_state},
                               {"active_state", d.active_state},
                               {"sub_state", d.sub_state},
                               {"unit_file_state", d.unit_file_state},
                               {"main_pid", d.main_pid},
                               {"restart_count", d.restart_count},
                               {"active_enter_timestamp_utc", d.active_enter_timestamp_utc},
                               {"memory_current_bytes", d.memory_current_bytes}};

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
