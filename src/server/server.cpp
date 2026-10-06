#include <chrono>
#include <exception>
#include <functional>
#include <string>

#include <drogon/drogon.h>
#include <drogon/utils/Utilities.h>
#include <nlohmann/json.hpp>

#include <nodepulse/controllers/cpu_controller.hpp>
#include <nodepulse/controllers/disk_controller.hpp>
#include <nodepulse/controllers/health_controller.hpp>
#include <nodepulse/controllers/network_controller.hpp>
#include <nodepulse/server/server.hpp>
#include <nodepulse/utils/error_response.hpp>
#include <nodepulse/utils/logger.hpp>

namespace nodepulse::server {

namespace {

bool is_valid_request_id(std::string_view id) noexcept {
    if (id.empty() || id.length() > 64) {
        return false;
    }
    for (char c : id) {
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
              c == '-' || c == '_' || c == '.')) {
            return false;
        }
    }
    return true;
}

}  // namespace

std::chrono::steady_clock::time_point Server::start_time_ = std::chrono::steady_clock::now();

Server::Server(config::Config config) : config_(std::move(config)) {}

Server::~Server() {
    stop();
}

void Server::setup() {
    if (config_.server.host != "127.0.0.1" && config_.server.host != "::1" &&
        config_.server.host != "localhost") {
        throw std::runtime_error(
            "Binding to non-loopback address ('" + config_.server.host +
            "') is rejected: authentication is not implemented prior to Phase 9.");
    }

    start_time_ = std::chrono::steady_clock::now();
    controllers::HealthController::set_start_time(start_time_);

    if (config_.collectors.cpu.enabled) {
        controllers::CpuController::get_cpu_service()->start_sampling(
            std::chrono::milliseconds(config_.collectors.cpu.sample_interval_ms));
    }

    if (config_.collectors.disks.enabled) {
        auto collector = std::make_shared<collectors::DiskCollector>(
            "/proc/mounts", config_.collectors.disks.ignored_fstypes);
        auto service = std::make_shared<services::DiskService>(collector);
        controllers::DiskController::set_disk_service(service);
    }

    if (config_.collectors.network.enabled) {
        controllers::NetworkController::get_network_service()->start_sampling(
            std::chrono::milliseconds(1000));
    }

    drogon::app().addListener(config_.server.host, config_.server.port);
    drogon::app().setThreadNum(config_.server.threads);

    drogon::app().registerPreRoutingAdvice([](const drogon::HttpRequestPtr& req,
                                              drogon::AdviceCallback&& /*acb*/,
                                              drogon::AdviceChainCallback&& accb) {
        std::string req_id = req->getHeader("X-Request-ID");
        if (req_id.empty()) {
            req_id = req->getHeader("x-request-id");
        }
        if (!is_valid_request_id(req_id)) {
            req_id = drogon::utils::getUuid();
        }
        req->getAttributes()->insert("request_id", req_id);
        req->getAttributes()->insert("start_time", std::chrono::steady_clock::now());
        accb();
    });

    drogon::app().registerPostHandlingAdvice([](const drogon::HttpRequestPtr& req,
                                                const drogon::HttpResponsePtr& resp) {
        std::string req_id;
        if (req && req->getAttributes()) {
            req_id = req->getAttributes()->get<std::string>("request_id");
        }
        if (!req_id.empty() && resp) {
            resp->addHeader("X-Request-ID", req_id);
        }

        if (req && resp && req->getAttributes()) {
            auto req_start =
                req->getAttributes()->get<std::chrono::steady_clock::time_point>("start_time");
            auto duration = std::chrono::duration<double, std::milli>(
                                std::chrono::steady_clock::now() - req_start)
                                .count();
            utils::Logger::get()->info("HTTP {} {} -> {} ({:.2f}ms) [request_id: {}] [client: {}]",
                                       req->methodString(), req->path(),
                                       static_cast<int>(resp->statusCode()), duration, req_id,
                                       req->peerAddr().toIp());
        }
    });

    drogon::app().setDefaultHandler(
        [](const drogon::HttpRequestPtr& req,
           std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
            std::string req_id;
            if (req && req->getAttributes()) {
                req_id = req->getAttributes()->get<std::string>("request_id");
            }
            auto resp = utils::make_error_response(
                drogon::k404NotFound, utils::error_codes::kResourceNotFound,
                "The requested endpoint was not found.", nlohmann::json::array(), req_id);
            callback(resp);
        });

    drogon::app().setCustomErrorHandler(
        [](drogon::HttpStatusCode code, const drogon::HttpRequestPtr& req) {
            std::string req_id;
            if (req && req->getAttributes()) {
                req_id = req->getAttributes()->get<std::string>("request_id");
            }
            if (code == drogon::k404NotFound) {
                return utils::make_error_response(
                    drogon::k404NotFound, utils::error_codes::kResourceNotFound,
                    "The requested endpoint was not found.", nlohmann::json::array(), req_id);
            }
            return utils::make_error_response(code, utils::error_codes::kInternalError,
                                              "An unexpected error occurred.",
                                              nlohmann::json::array(), req_id);
        });

    drogon::app().setExceptionHandler(
        [](const std::exception& e, const drogon::HttpRequestPtr& req,
           std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
            std::string req_id;
            if (req && req->getAttributes()) {
                req_id = req->getAttributes()->get<std::string>("request_id");
            }
            utils::Logger::get()->error("Unhandled exception processing request: {}", e.what());
            auto resp = utils::make_error_response(
                drogon::k500InternalServerError, utils::error_codes::kInternalError,
                "An unexpected error occurred processing your request.", nlohmann::json::array(),
                req_id);
            callback(resp);
        });
}

void Server::run() {
    if (config_.server.host != "127.0.0.1" && config_.server.host != "::1" &&
        config_.server.host != "localhost") {
        throw std::runtime_error(
            "Binding to non-loopback address ('" + config_.server.host +
            "') is rejected: authentication is not implemented prior to Phase 9.");
    }
    utils::Logger::get()->info("Starting NodePulse HTTP server on {}:{} with {} threads",
                               config_.server.host, config_.server.port, config_.server.threads);
    drogon::app().run();
}

void Server::stop() {
    utils::Logger::get()->info("Stopping NodePulse HTTP server");
    controllers::CpuController::get_cpu_service()->stop_sampling();
    controllers::NetworkController::get_network_service()->stop_sampling();
    drogon::app().quit();
}

}  // namespace nodepulse::server
