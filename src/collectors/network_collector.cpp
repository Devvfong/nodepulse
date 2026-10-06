#include <cctype>
#include <charconv>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <nodepulse/collectors/network_collector.hpp>

namespace nodepulse::collectors {

namespace {

std::string_view trim_view(std::string_view s) noexcept {
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.front())) != 0) {
        s.remove_prefix(1);
    }
    while (!s.empty() && std::isspace(static_cast<unsigned char>(s.back())) != 0) {
        s.remove_suffix(1);
    }
    return s;
}

bool is_valid_interface_name(std::string_view name) noexcept {
    if (name.empty() || name.length() > 64) {
        return false;
    }
    for (char c : name) {
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
              c == '_' || c == '-' || c == '.' || c == '@' || c == ':')) {
            return false;
        }
    }
    return true;
}

bool parse_uint64_token(std::string_view token, uint64_t& out) noexcept {
    if (token.empty()) {
        return false;
    }
    for (char c : token) {
        if (c < '0' || c > '9') {
            return false;
        }
    }
    auto [ptr, ec] = std::from_chars(token.data(), token.data() + token.size(), out);
    return ec == std::errc{} && ptr == token.data() + token.size();
}

}  // namespace

NetworkCollector::NetworkCollector(std::string net_dev_path, std::string sys_class_net_path)
    : net_dev_path_(std::move(net_dev_path)), sys_class_net_path_(std::move(sys_class_net_path)) {}

std::vector<domain::NetworkInterfaceMetrics> NetworkCollector::parse_net_dev_stream(
    std::istream& stream) {
    std::vector<domain::NetworkInterfaceMetrics> interfaces;
    std::string line;

    while (std::getline(stream, line)) {
        auto colon_pos = line.find(':');
        if (colon_pos == std::string::npos) {
            // Header line or malformed
            continue;
        }

        std::string_view name_view = trim_view(std::string_view(line).substr(0, colon_pos));
        if (!is_valid_interface_name(name_view)) {
            continue;
        }

        std::string_view data_view = std::string_view(line).substr(colon_pos + 1);
        std::istringstream iss{std::string(data_view)};
        std::vector<std::string> tokens;
        std::string token;
        while (iss >> token) {
            tokens.push_back(token);
        }

        if (tokens.size() < 16) {
            // Malformed record: /proc/net/dev specifies 16 numeric columns
            continue;
        }

        uint64_t rx_bytes = 0;
        uint64_t rx_packets = 0;
        uint64_t rx_errors = 0;
        uint64_t rx_drops = 0;
        uint64_t rx_fifo = 0;
        uint64_t rx_frame = 0;
        uint64_t rx_compressed = 0;
        uint64_t rx_multicast = 0;
        uint64_t tx_bytes = 0;
        uint64_t tx_packets = 0;
        uint64_t tx_errors = 0;
        uint64_t tx_drops = 0;
        uint64_t tx_fifo = 0;
        uint64_t tx_colls = 0;
        uint64_t tx_carrier = 0;
        uint64_t tx_compressed = 0;

        if (!parse_uint64_token(tokens[0], rx_bytes) ||
            !parse_uint64_token(tokens[1], rx_packets) ||
            !parse_uint64_token(tokens[2], rx_errors) || !parse_uint64_token(tokens[3], rx_drops) ||
            !parse_uint64_token(tokens[4], rx_fifo) || !parse_uint64_token(tokens[5], rx_frame) ||
            !parse_uint64_token(tokens[6], rx_compressed) ||
            !parse_uint64_token(tokens[7], rx_multicast) ||
            !parse_uint64_token(tokens[8], tx_bytes) ||
            !parse_uint64_token(tokens[9], tx_packets) ||
            !parse_uint64_token(tokens[10], tx_errors) ||
            !parse_uint64_token(tokens[11], tx_drops) || !parse_uint64_token(tokens[12], tx_fifo) ||
            !parse_uint64_token(tokens[13], tx_colls) ||
            !parse_uint64_token(tokens[14], tx_carrier) ||
            !parse_uint64_token(tokens[15], tx_compressed)) {
            // One or more columns contain non-numeric or malformed data
            continue;
        }

        domain::NetworkInterfaceMetrics metrics;
        metrics.name = std::string(name_view);
        metrics.rx_bytes = rx_bytes;
        metrics.rx_packets = rx_packets;
        metrics.rx_errors = rx_errors;
        metrics.rx_drops = rx_drops;
        metrics.tx_bytes = tx_bytes;
        metrics.tx_packets = tx_packets;
        metrics.tx_errors = tx_errors;
        metrics.tx_drops = tx_drops;

        interfaces.push_back(std::move(metrics));
    }

    return interfaces;
}

std::string NetworkCollector::read_sysfs_string(const std::string& iface,
                                                const std::string& property) const {
    std::string path = sys_class_net_path_ + "/" + iface + "/" + property;
    std::ifstream file(path);
    if (!file.is_open()) {
        return "";
    }
    std::string line;
    if (std::getline(file, line)) {
        return std::string(trim_view(line));
    }
    return "";
}

uint64_t NetworkCollector::read_sysfs_uint64(const std::string& iface,
                                             const std::string& property) const {
    std::string str = read_sysfs_string(iface, property);
    if (str.empty()) {
        return 0;
    }
    uint64_t val = 0;
    if (parse_uint64_token(str, val)) {
        return val;
    }
    return 0;
}

std::optional<std::vector<domain::NetworkInterfaceMetrics>> NetworkCollector::collect() const {
    std::ifstream file(net_dev_path_);
    if (!file.is_open()) {
        return std::nullopt;
    }

    auto interfaces = parse_net_dev_stream(file);

    for (auto& iface : interfaces) {
        iface.mac_address = read_sysfs_string(iface.name, "address");
        auto oper = read_sysfs_string(iface.name, "operstate");
        iface.operstate = oper.empty() ? "unknown" : oper;
        iface.speed_mbps = read_sysfs_uint64(iface.name, "speed");
    }

    return interfaces;
}

}  // namespace nodepulse::collectors
