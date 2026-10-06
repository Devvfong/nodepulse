#include <charconv>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <drogon/HttpResponse.h>
#include <nlohmann/json.hpp>

#include <nodepulse/controllers/process_controller.hpp>
#include <nodepulse/services/process_service.hpp>
#include <nodepulse/utils/error_response.hpp>

namespace nodepulse::controllers {

std::shared_ptr<services::ProcessService> ProcessController::process_service_ = nullptr;

std::shared_ptr<services::ProcessService> ProcessController::get_process_service() {
    if (!process_service_) {
        process_service_ = std::make_shared<services::ProcessService>();
    }
    return process_service_;
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

void ProcessController::get_processes(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
    std::string req_id;
    if (req && req->getAttributes()) {
        req_id = req->getAttributes()->get<std::string>("request_id");
    }

    std::string sort_param = "cpu";
    auto sort_opt = req->getParameter("sort");
    if (!sort_opt.empty()) {
        if (sort_opt != "cpu" && sort_opt != "memory" && sort_opt != "pid") {
            nlohmann::json details = nlohmann::json::array(
                {{{"field", "sort"}, {"reason", "must_be_one_of_cpu_memory_pid"}}});
            auto err_resp = utils::make_error_response(
                drogon::k400BadRequest, utils::error_codes::kInvalidRequest,
                "Invalid sort parameter. Allowed values: cpu, memory, pid.", details, req_id);
            callback(err_resp);
            return;
        }
        sort_param = sort_opt;
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

    auto service = get_process_service();
    service->get_processes_async(
        sort_param, limit_val,
        [callback = std::move(callback), req_id](std::vector<domain::ProcessInfo> processes) {
            nlohmann::json body = nlohmann::json::array();
            for (const auto& proc : processes) {
                body.push_back({{"pid", proc.pid},
                                {"name", proc.name},
                                {"user", proc.user},
                                {"state", proc.state},
                                {"cpu_percent", proc.cpu_percent},
                                {"memory_rss_bytes", proc.memory_rss_bytes},
                                {"cmdline", proc.cmdline}});
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

void ProcessController::get_process_by_pid(
    const drogon::HttpRequestPtr& req,
    std::function<void(const drogon::HttpResponsePtr&)>&& callback, std::string pid_param) {
    std::string req_id;
    if (req && req->getAttributes()) {
        req_id = req->getAttributes()->get<std::string>("request_id");
    }

    auto service = get_process_service();

    int64_t parsed_pid = 0;
    int64_t pid_max = service->get_pid_max();

    if (!parse_integer(pid_param, parsed_pid) || parsed_pid < 1 || parsed_pid > pid_max) {
        nlohmann::json details =
            nlohmann::json::array({{{"field", "pid"}, {"reason", "must_be_positive_integer"}}});
        auto err_resp =
            utils::make_error_response(drogon::k400BadRequest, utils::error_codes::kInvalidRequest,
                                       "Process ID must be a positive integer.", details, req_id);
        callback(err_resp);
        return;
    }

    int32_t pid = static_cast<int32_t>(parsed_pid);

    service->get_process_detail_async(pid, [callback = std::move(callback), req_id,
                                            pid](std::optional<domain::ProcessDetail> detail_opt) {
        if (!detail_opt.has_value()) {
            nlohmann::json details = nlohmann::json::array(
                {{{"resource_type", "process"}, {"identifier", std::to_string(pid)}}});
            auto err_resp = utils::make_error_response(
                drogon::k404NotFound, utils::error_codes::kResourceNotFound,
                "Process with PID " + std::to_string(pid) + " was not found.", details, req_id);
            callback(err_resp);
            return;
        }

        const auto& detail = *detail_opt;
        nlohmann::json body = {{"pid", detail.pid},
                               {"ppid", detail.ppid},
                               {"name", detail.name},
                               {"user", detail.user},
                               {"state", detail.state},
                               {"cpu_percent", detail.cpu_percent},
                               {"memory_rss_bytes", detail.memory_rss_bytes},
                               {"memory_vms_bytes", detail.memory_vms_bytes},
                               {"thread_count", detail.thread_count},
                               {"open_fd_count", detail.open_fd_count},
                               {"start_time_epoch", detail.start_time_epoch},
                               {"cmdline", detail.cmdline},
                               {"working_directory", detail.working_directory}};

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
