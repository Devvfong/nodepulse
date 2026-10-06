#pragma once

#include <cstdint>
#include <optional>
#include <string>

namespace nodepulse::domain {

struct NetworkInterfaceMetrics {
    std::string name;
    std::string mac_address;
    std::string operstate{"unknown"};
    uint64_t speed_mbps{0};
    uint64_t rx_bytes{0};
    uint64_t tx_bytes{0};
    uint64_t rx_packets{0};
    uint64_t tx_packets{0};
    uint64_t rx_errors{0};
    uint64_t tx_errors{0};
    uint64_t rx_drops{0};
    uint64_t tx_drops{0};
    std::optional<double> rx_bytes_per_sec{std::nullopt};
    std::optional<double> tx_bytes_per_sec{std::nullopt};
};

}  // namespace nodepulse::domain
