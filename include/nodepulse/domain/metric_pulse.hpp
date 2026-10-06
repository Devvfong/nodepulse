#pragma once

#include <cstdint>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

namespace nodepulse::domain {

struct MetricPulse {
    std::string timestamp;                                  // ISO 8601 UTC
    std::optional<double> cpu_usage_percent{std::nullopt};  // null during warming_up
    double memory_usage_percent{0.0};
    uint64_t memory_used_bytes{0};
    std::optional<double> network_rx_bytes_sec{
        std::nullopt};  // null during initial sampling / reset
    std::optional<double> network_tx_bytes_sec{
        std::nullopt};  // null during initial sampling / reset
};

inline void to_json(nlohmann::json& j, const MetricPulse& p) {
    j = nlohmann::json{{"timestamp", p.timestamp},
                       {"memory_usage_percent", p.memory_usage_percent},
                       {"memory_used_bytes", p.memory_used_bytes}};
    if (p.cpu_usage_percent.has_value()) {
        j["cpu_usage_percent"] = *p.cpu_usage_percent;
    } else {
        j["cpu_usage_percent"] = nullptr;
    }
    if (p.network_rx_bytes_sec.has_value()) {
        j["network_rx_bytes_sec"] = *p.network_rx_bytes_sec;
    } else {
        j["network_rx_bytes_sec"] = nullptr;
    }
    if (p.network_tx_bytes_sec.has_value()) {
        j["network_tx_bytes_sec"] = *p.network_tx_bytes_sec;
    } else {
        j["network_tx_bytes_sec"] = nullptr;
    }
}

inline void from_json(const nlohmann::json& j, MetricPulse& p) {
    p.timestamp = j.value("timestamp", "");
    if (j.contains("cpu_usage_percent") && !j["cpu_usage_percent"].is_null()) {
        p.cpu_usage_percent = j["cpu_usage_percent"].get<double>();
    } else {
        p.cpu_usage_percent = std::nullopt;
    }
    p.memory_usage_percent = j.value("memory_usage_percent", 0.0);
    p.memory_used_bytes = j.value("memory_used_bytes", 0ULL);
    if (j.contains("network_rx_bytes_sec") && !j["network_rx_bytes_sec"].is_null()) {
        p.network_rx_bytes_sec = j["network_rx_bytes_sec"].get<double>();
    } else {
        p.network_rx_bytes_sec = std::nullopt;
    }
    if (j.contains("network_tx_bytes_sec") && !j["network_tx_bytes_sec"].is_null()) {
        p.network_tx_bytes_sec = j["network_tx_bytes_sec"].get<double>();
    } else {
        p.network_tx_bytes_sec = std::nullopt;
    }
}

}  // namespace nodepulse::domain
