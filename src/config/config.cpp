#include <cstdlib>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

#include <nlohmann/json.hpp>

#include <nodepulse/config/config.hpp>

namespace nodepulse::config {

namespace {

std::optional<std::string> get_env_var(const char* name) {
    const char* val = std::getenv(name);
    if (val != nullptr && *val != '\0') {
        return std::string(val);
    }
    return std::nullopt;
}

}  // namespace

Config Config::load_from_json(const nlohmann::json& json_data) {
    Config cfg;

    if (json_data.contains("server") && json_data["server"].is_object()) {
        const auto& s = json_data["server"];
        if (s.contains("host") && s["host"].is_string()) {
            cfg.server.host = s["host"].get<std::string>();
        }
        if (s.contains("port") && s["port"].is_number_integer()) {
            cfg.server.port = s["port"].get<uint16_t>();
        }
        if (s.contains("threads") && s["threads"].is_number_integer()) {
            cfg.server.threads = s["threads"].get<uint32_t>();
        }
        if (s.contains("log_level") && s["log_level"].is_string()) {
            cfg.server.log_level = s["log_level"].get<std::string>();
        }
        if (s.contains("log_format") && s["log_format"].is_string()) {
            cfg.server.log_format = s["log_format"].get<std::string>();
        }
    }

    if (json_data.contains("security") && json_data["security"].is_object()) {
        const auto& sec = json_data["security"];
        if (sec.contains("api_key") && sec["api_key"].is_string()) {
            cfg.security.api_key = sec["api_key"].get<std::string>();
        }
        if (sec.contains("constant_time_comparison") &&
            sec["constant_time_comparison"].is_boolean()) {
            cfg.security.constant_time_comparison = sec["constant_time_comparison"].get<bool>();
        }
    }

    if (json_data.contains("rate_limiting") && json_data["rate_limiting"].is_object()) {
        const auto& rl = json_data["rate_limiting"];
        if (rl.contains("enabled") && rl["enabled"].is_boolean()) {
            cfg.rate_limiting.enabled = rl["enabled"].get<bool>();
        }
        if (rl.contains("requests_per_minute") && rl["requests_per_minute"].is_number_integer()) {
            cfg.rate_limiting.requests_per_minute = rl["requests_per_minute"].get<uint32_t>();
        }
        if (rl.contains("burst_capacity") && rl["burst_capacity"].is_number_integer()) {
            cfg.rate_limiting.burst_capacity = rl["burst_capacity"].get<uint32_t>();
        }
    }

    if (json_data.contains("collectors") && json_data["collectors"].is_object()) {
        const auto& col = json_data["collectors"];
        if (col.contains("system") && col["system"].is_object() &&
            col["system"].contains("enabled")) {
            cfg.collectors.system.enabled = col["system"]["enabled"].get<bool>();
        }
        if (col.contains("cpu") && col["cpu"].is_object()) {
            if (col["cpu"].contains("enabled")) {
                cfg.collectors.cpu.enabled = col["cpu"]["enabled"].get<bool>();
            }
            if (col["cpu"].contains("sample_interval_ms")) {
                cfg.collectors.cpu.sample_interval_ms =
                    col["cpu"]["sample_interval_ms"].get<uint32_t>();
            }
        }
        if (col.contains("memory") && col["memory"].is_object() &&
            col["memory"].contains("enabled")) {
            cfg.collectors.memory.enabled = col["memory"]["enabled"].get<bool>();
        }
        if (col.contains("disks") && col["disks"].is_object()) {
            if (col["disks"].contains("enabled")) {
                cfg.collectors.disks.enabled = col["disks"]["enabled"].get<bool>();
            }
            if (col["disks"].contains("ignored_fstypes") &&
                col["disks"]["ignored_fstypes"].is_array()) {
                cfg.collectors.disks.ignored_fstypes =
                    col["disks"]["ignored_fstypes"].get<std::vector<std::string>>();
            }
        }
        if (col.contains("network") && col["network"].is_object() &&
            col["network"].contains("enabled")) {
            cfg.collectors.network.enabled = col["network"]["enabled"].get<bool>();
        }
        if (col.contains("processes") && col["processes"].is_object()) {
            if (col["processes"].contains("enabled")) {
                cfg.collectors.processes.enabled = col["processes"]["enabled"].get<bool>();
            }
            if (col["processes"].contains("max_process_limit")) {
                cfg.collectors.processes.max_process_limit =
                    col["processes"]["max_process_limit"].get<uint32_t>();
            }
        }
        if (col.contains("services") && col["services"].is_object() &&
            col["services"].contains("enabled")) {
            cfg.collectors.services.enabled = col["services"]["enabled"].get<bool>();
        }
        if (col.contains("docker") && col["docker"].is_object()) {
            if (col["docker"].contains("enabled")) {
                cfg.collectors.docker.enabled = col["docker"]["enabled"].get<bool>();
            }
            if (col["docker"].contains("socket_path") && col["docker"]["socket_path"].is_string()) {
                cfg.collectors.docker.socket_path = col["docker"]["socket_path"].get<std::string>();
            }
            if (col["docker"].contains("timeout_ms")) {
                cfg.collectors.docker.timeout_ms = col["docker"]["timeout_ms"].get<uint32_t>();
            }
        }
    }

    if (json_data.contains("sse") && json_data["sse"].is_object()) {
        const auto& sse_obj = json_data["sse"];
        if (sse_obj.contains("enabled")) {
            cfg.sse.enabled = sse_obj["enabled"].get<bool>();
        }
        if (sse_obj.contains("interval_ms")) {
            cfg.sse.interval_ms = sse_obj["interval_ms"].get<uint32_t>();
        }
    }

    if (json_data.contains("prometheus") && json_data["prometheus"].is_object()) {
        const auto& prom = json_data["prometheus"];
        if (prom.contains("enabled")) {
            cfg.prometheus.enabled = prom["enabled"].get<bool>();
        }
        if (prom.contains("require_auth")) {
            cfg.prometheus.require_auth = prom["require_auth"].get<bool>();
        }
    }

    if (json_data.contains("postgres") && json_data["postgres"].is_object()) {
        const auto& pg = json_data["postgres"];
        if (pg.contains("enabled")) {
            cfg.postgres.enabled = pg["enabled"].get<bool>();
        }
        if (pg.contains("connection_string") && pg["connection_string"].is_string()) {
            cfg.postgres.connection_string = pg["connection_string"].get<std::string>();
        }
        if (pg.contains("snapshot_interval_seconds")) {
            cfg.postgres.snapshot_interval_seconds =
                pg["snapshot_interval_seconds"].get<uint32_t>();
        }
    }

    return cfg;
}

Config Config::load_from_file(const std::string& filepath) {
    std::ifstream file(filepath);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open configuration file: " + filepath);
    }

    nlohmann::json json_data;
    try {
        file >> json_data;
    } catch (const nlohmann::json::parse_error& ex) {
        throw std::runtime_error("Malformed JSON in configuration file '" + filepath +
                                 "': " + ex.what());
    }

    return load_from_json(json_data);
}

void Config::apply_env_overrides() {
    if (auto host = get_env_var("NODEPULSE_HOST")) {
        server.host = *host;
    }
    if (auto port_str = get_env_var("NODEPULSE_PORT")) {
        try {
            server.port = static_cast<uint16_t>(std::stoul(*port_str));
        } catch (const std::exception&) {
            // Preserved for validation to catch
            server.port = 0;
        }
    }
    if (auto threads_str = get_env_var("NODEPULSE_THREADS")) {
        try {
            server.threads = static_cast<uint32_t>(std::stoul(*threads_str));
        } catch (const std::exception&) {
            server.threads = 0;
        }
    }
    if (auto level = get_env_var("NODEPULSE_LOG_LEVEL")) {
        server.log_level = *level;
    }
    if (auto format = get_env_var("NODEPULSE_LOG_FORMAT")) {
        server.log_format = *format;
    }
    if (auto key = get_env_var("NODEPULSE_API_KEY")) {
        security.api_key = *key;
    }
    if (auto limit_str = get_env_var("NODEPULSE_RATE_LIMIT")) {
        try {
            rate_limiting.requests_per_minute = static_cast<uint32_t>(std::stoul(*limit_str));
        } catch (const std::exception&) {
            rate_limiting.requests_per_minute = 0;
        }
    }
    if (auto docker_enabled = get_env_var("NODEPULSE_DOCKER_ENABLED")) {
        collectors.docker.enabled = (*docker_enabled == "true" || *docker_enabled == "1");
    }
    if (auto docker_sock = get_env_var("NODEPULSE_DOCKER_SOCKET")) {
        collectors.docker.socket_path = *docker_sock;
    }
    if (auto prom_auth = get_env_var("NODEPULSE_PROMETHEUS_AUTH")) {
        prometheus.require_auth = (*prom_auth == "true" || *prom_auth == "1");
    }
}

Config Config::load(const std::optional<std::string>& file_override) {
    Config cfg;

    std::optional<std::string> config_path = file_override;
    if (!config_path.has_value()) {
        config_path = get_env_var("NODEPULSE_CONFIG");
    }

    if (config_path.has_value()) {
        cfg = load_from_file(*config_path);
    } else {
        // Try default locations if they exist, otherwise fallback to defaults
        const std::vector<std::string> search_paths = {
            "/etc/nodepulse/config.json", "config/config.json", "config/config.example.json"};
        for (const auto& path : search_paths) {
            std::ifstream check_file(path);
            if (check_file.is_open()) {
                check_file.close();
                cfg = load_from_file(path);
                break;
            }
        }
    }

    cfg.apply_env_overrides();
    return cfg;
}

std::vector<std::string> Config::validate() const {
    std::vector<std::string> errors;

    if (server.host.empty()) {
        errors.emplace_back("server.host cannot be empty");
    }

    if (server.port == 0) {
        errors.emplace_back("server.port must be between 1 and 65535");
    }

    if (server.threads == 0 || server.threads > 256) {
        errors.emplace_back("server.threads must be between 1 and 256");
    }

    const std::vector<std::string> valid_log_levels = {"trace", "debug", "info",
                                                       "warn",  "error", "critical"};
    bool valid_level = false;
    for (const auto& level : valid_log_levels) {
        if (server.log_level == level) {
            valid_level = true;
            break;
        }
    }
    if (!valid_level) {
        errors.emplace_back(
            "server.log_level must be one of: trace, debug, info, warn, error, critical");
    }

    if (server.log_format != "text" && server.log_format != "json") {
        errors.emplace_back("server.log_format must be either 'text' or 'json'");
    }

    if (rate_limiting.enabled) {
        if (rate_limiting.requests_per_minute == 0) {
            errors.emplace_back("rate_limiting.requests_per_minute must be greater than 0");
        }
        if (rate_limiting.burst_capacity == 0) {
            errors.emplace_back("rate_limiting.burst_capacity must be greater than 0");
        }
    }

    if (collectors.cpu.enabled && collectors.cpu.sample_interval_ms < 100) {
        errors.emplace_back("collectors.cpu.sample_interval_ms must be at least 100ms");
    }

    if (sse.enabled && sse.interval_ms < 100) {
        errors.emplace_back("sse.interval_ms must be at least 100ms");
    }

    return errors;
}

}  // namespace nodepulse::config
