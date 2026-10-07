#pragma once

#include <chrono>
#include <cstdint>
#include <ctime>
#include <optional>
#include <string>

#include <nlohmann/json.hpp>

namespace nodepulse::domain {

/**
 * @brief Host telemetry snapshot representing a point-in-time state of host metrics
 *        for PostgreSQL archival persistence (Phase 14).
 */
struct HostSnapshot {
    std::string hostname;
    std::chrono::system_clock::time_point captured_at{std::chrono::system_clock::now()};
    std::optional<double> cpu_usage_percent;
    double load_1m{0.0};
    double load_5m{0.0};
    double load_15m{0.0};
    uint64_t mem_total_bytes{0};
    uint64_t mem_used_bytes{0};
    uint64_t mem_available_bytes{0};
    uint64_t swap_total_bytes{0};
    uint64_t swap_used_bytes{0};
    std::optional<double> net_rx_bytes_per_sec;
    std::optional<double> net_tx_bytes_per_sec;
};

/**
 * @brief Persisted host metric record retrieved from the database.
 */
struct HostMetricRecord {
    int64_t id{0};
    std::chrono::system_clock::time_point captured_at;
    std::string hostname;
    std::optional<double> cpu_usage_percent;
    double load_1m{0.0};
    double load_5m{0.0};
    double load_15m{0.0};
    uint64_t mem_total_bytes{0};
    uint64_t mem_used_bytes{0};
    uint64_t mem_available_bytes{0};
    uint64_t swap_total_bytes{0};
    uint64_t swap_used_bytes{0};
    std::optional<double> net_rx_bytes_per_sec;
    std::optional<double> net_tx_bytes_per_sec;

    [[nodiscard]] nlohmann::json to_json() const {
        nlohmann::json j;
        j["id"] = id;

        auto s = std::chrono::duration_cast<std::chrono::seconds>(captured_at.time_since_epoch())
                     .count();
        std::time_t tt = static_cast<std::time_t>(s);
        std::tm tm_buf{};
        gmtime_r(&tt, &tm_buf);
        char buf[32];
        std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm_buf);
        j["captured_at"] = std::string(buf);

        j["hostname"] = hostname;
        if (cpu_usage_percent.has_value()) {
            j["cpu_usage_percent"] = *cpu_usage_percent;
        } else {
            j["cpu_usage_percent"] = nullptr;
        }
        j["load_1m"] = load_1m;
        j["load_5m"] = load_5m;
        j["load_15m"] = load_15m;
        j["mem_total_bytes"] = mem_total_bytes;
        j["mem_used_bytes"] = mem_used_bytes;
        j["mem_available_bytes"] = mem_available_bytes;
        j["swap_total_bytes"] = swap_total_bytes;
        j["swap_used_bytes"] = swap_used_bytes;
        if (net_rx_bytes_per_sec.has_value()) {
            j["net_rx_bytes_per_sec"] = *net_rx_bytes_per_sec;
        } else {
            j["net_rx_bytes_per_sec"] = nullptr;
        }
        if (net_tx_bytes_per_sec.has_value()) {
            j["net_tx_bytes_per_sec"] = *net_tx_bytes_per_sec;
        } else {
            j["net_tx_bytes_per_sec"] = nullptr;
        }
        return j;
    }
};

/**
 * @brief Criteria for querying historical host metric records.
 */
struct HistoryQuery {
    std::optional<std::string> hostname;
    std::optional<std::chrono::system_clock::time_point> start_time;
    std::optional<std::chrono::system_clock::time_point> end_time;
    size_t limit{50};
};

}  // namespace nodepulse::domain
