#pragma once

#include <istream>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <nodepulse/domain/network_info.hpp>

namespace nodepulse::collectors {

class NetworkCollector {
  public:
    explicit NetworkCollector(std::string net_dev_path = "/proc/net/dev",
                              std::string sys_class_net_path = "/sys/class/net");
    virtual ~NetworkCollector() = default;

    NetworkCollector(const NetworkCollector&) = default;
    NetworkCollector& operator=(const NetworkCollector&) = default;
    NetworkCollector(NetworkCollector&&) = default;
    NetworkCollector& operator=(NetworkCollector&&) = default;

    [[nodiscard]] virtual std::optional<std::vector<domain::NetworkInterfaceMetrics>> collect()
        const;

    static std::vector<domain::NetworkInterfaceMetrics> parse_net_dev_stream(std::istream& stream);

  private:
    std::string read_sysfs_string(const std::string& iface, const std::string& property) const;
    uint64_t read_sysfs_uint64(const std::string& iface, const std::string& property) const;

    std::string net_dev_path_;
    std::string sys_class_net_path_;
};

}  // namespace nodepulse::collectors

