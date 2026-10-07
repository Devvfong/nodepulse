#include <chrono>
#include <exception>
#include <functional>
#include <string>

#include <drogon/drogon.h>
#include <drogon/utils/Utilities.h>
#include <nlohmann/json.hpp>

#include <nodepulse/collectors/docker_collector.hpp>
#include <nodepulse/controllers/cpu_controller.hpp>
#include <nodepulse/controllers/disk_controller.hpp>
#include <nodepulse/controllers/docker_controller.hpp>
#include <nodepulse/controllers/events_controller.hpp>
#include <nodepulse/controllers/health_controller.hpp>
#include <nodepulse/controllers/metrics_controller.hpp>
#include <nodepulse/controllers/network_controller.hpp>
#include <nodepulse/controllers/process_controller.hpp>
#include <nodepulse/controllers/service_controller.hpp>
#include <nodepulse/middleware/auth_filter.hpp>
#include <nodepulse/middleware/rate_limit_filter.hpp>
#include <nodepulse/server/server.hpp>
#include <nodepulse/services/docker_service.hpp>
#include <nodepulse/services/memory_service.hpp>
#include <nodepulse/services/metrics_exporter.hpp>
#include <nodepulse/services/stream_service.hpp>
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
        throw std::runtime_error("Binding to non-loopback address ('" + config_.server.host +
                                 "') is rejected: plain HTTP loopback-only binding is enforced.");
    }

    if (config_.security.api_key.empty()) {
        throw std::runtime_error(
            "Server startup rejected: security.api_key cannot be empty. "
            "Set it in config or via NODEPULSE_API_KEY environment variable.");
    }
    middleware::AuthFilter::set_api_key(config_.security.api_key);
    middleware::AuthFilter::set_metrics_require_auth(config_.prometheus.require_auth);
    middleware::RateLimitFilter::init(config_.rate_limiting);

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

    if (config_.collectors.processes.enabled) {
        controllers::ProcessController::get_process_service()->start_sampling(
            std::chrono::milliseconds(1000));
    }

    auto docker_transport = std::make_shared<collectors::DockerUnixSocketTransport>(
        config_.collectors.docker.socket_path, config_.collectors.docker.timeout_ms,
        config_.collectors.docker.enabled);
    auto docker_collector = std::make_shared<collectors::DockerCollector>(
        docker_transport, config_.collectors.docker.socket_path);
    auto docker_service = std::make_shared<services::DockerService>(docker_collector);
    controllers::DockerController::set_docker_service(docker_service);

    auto stream_service = std::make_shared<services::StreamService>(
        controllers::CpuController::get_cpu_service(), std::make_shared<services::MemoryService>(),
        controllers::NetworkController::get_network_service(), config_.sse);
    controllers::EventsController::set_stream_service(stream_service);
    if (config_.sse.enabled) {
        stream_service->start_streaming(std::chrono::milliseconds(config_.sse.interval_ms));
    }

    auto metrics_exporter = std::make_shared<services::MetricsExporter>(
        controllers::CpuController::get_cpu_service(), std::make_shared<services::MemoryService>(),
        controllers::DiskController::get_disk_service(),
        controllers::NetworkController::get_network_service(), stream_service, config_.prometheus);
    metrics_exporter->set_start_time(start_time_);
    controllers::MetricsController::set_metrics_exporter(metrics_exporter);
    controllers::MetricsController::set_config(config_.prometheus);

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

    drogon::app().registerPostRoutingAdvice([](const drogon::HttpRequestPtr& req,
                                               drogon::AdviceCallback&& acb,
                                               drogon::AdviceChainCallback&& accb) {
        middleware::AuthFilter::handle_request(
            req, std::move(acb), [req, acb_cb = acb, accb_cb = std::move(accb)]() mutable {
                middleware::RateLimitFilter::handle_request(req, std::move(acb_cb),
                                                            std::move(accb_cb));
            });
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

        if (req && resp) {
            if (auto exp = controllers::MetricsController::get_metrics_exporter()) {
                exp->record_http_request(req->path(), req->methodString(),
                                         static_cast<int>(resp->statusCode()));
            }
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
        throw std::runtime_error("Binding to non-loopback address ('" + config_.server.host +
                                 "') is rejected: plain HTTP loopback-only binding is enforced.");
    }
    if (config_.security.api_key.empty()) {
        throw std::runtime_error(
            "Server startup rejected: security.api_key cannot be empty. "
            "Set it in config or via NODEPULSE_API_KEY environment variable.");
    }
    utils::Logger::get()->info("Starting NodePulse HTTP server on {}:{} with {} threads",
                               config_.server.host, config_.server.port, config_.server.threads);
    drogon::app().run();
}

void Server::stop() {
    utils::Logger::get()->info("Stopping NodePulse HTTP server");
    if (auto stream_svc = controllers::EventsController::get_stream_service()) {
        stream_svc->stop_streaming();
    }
    controllers::CpuController::get_cpu_service()->stop_sampling();
    controllers::NetworkController::get_network_service()->stop_sampling();
    controllers::ProcessController::get_process_service()->stop_sampling();
    controllers::MetricsController::set_metrics_exporter(nullptr);
    middleware::AuthFilter::reset();
    middleware::RateLimitFilter::reset();
    drogon::app().quit();
}

}  // namespace nodepulse::server
