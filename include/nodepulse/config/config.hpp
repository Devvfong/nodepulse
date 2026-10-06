#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace nodepulse::config {

struct ServerConfig {
    std::string host{"127.0.0.1"};
    uint16_t port{8080};
    uint32_t threads{4};
    std::string log_level{"info"};
    std::string log_format{"text"};
};

struct SecurityConfig {
    std::string api_key;
    bool constant_time_comparison{true};
};

struct RateLimitConfig {
    bool enabled{true};
    uint32_t requests_per_minute{120};
    uint32_t burst_capacity{30};
};

struct CollectorConfig {
    bool enabled{true};
};

struct CpuCollectorConfig {
    bool enabled{true};
    uint32_t sample_interval_ms{1000};
};

struct DisksCollectorConfig {
    bool enabled{true};
    std::vector<std::string> ignored_fstypes{"proc",   "sysfs", "cgroup",
                                             "devpts", "tmpfs", "overlay"};
};

struct ProcessesCollectorConfig {
    bool enabled{true};
    uint32_t max_process_limit{200};
};

struct DockerCollectorConfig {
    bool enabled{false};
    std::string socket_path{"/var/run/docker.sock"};
    uint32_t timeout_ms{2000};
};

struct CollectorsConfig {
    CollectorConfig system;
    CpuCollectorConfig cpu;
    CollectorConfig memory;
    DisksCollectorConfig disks;
    CollectorConfig network;
    ProcessesCollectorConfig processes;
    CollectorConfig services;
    DockerCollectorConfig docker;
};

struct SseConfig {
    bool enabled{true};
    uint32_t interval_ms{1000};
    uint32_t max_clients{64};
};

struct PrometheusConfig {
    bool enabled{true};
    bool require_auth{true};
};

struct PostgresConfig {
    bool enabled{false};
    std::string connection_string{"postgresql://nodepulse:password@localhost:5432/nodepulse_db"};
    uint32_t snapshot_interval_seconds{60};
};

struct Config {
    ServerConfig server;
    SecurityConfig security;
    RateLimitConfig rate_limiting;
    CollectorsConfig collectors;
    SseConfig sse;
    PrometheusConfig prometheus;
    PostgresConfig postgres;

    static Config load_from_file(const std::string& filepath);
    static Config load_from_json(const nlohmann::json& json_data);
    static Config load(const std::optional<std::string>& file_override = std::nullopt);
    void apply_env_overrides();
    [[nodiscard]] std::vector<std::string> validate() const;
};

}  // namespace nodepulse::config
