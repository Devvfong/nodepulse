#pragma once

#include <iosfwd>
#include <optional>
#include <string>
#include <utility>

#include <nodepulse/domain/system_info.hpp>

namespace nodepulse::collectors {

class SystemCollector {
  public:
    explicit SystemCollector(std::string os_release_path = "/etc/os-release",
                             std::string uptime_path = "/proc/uptime",
                             std::string stat_path = "/proc/stat");
    ~SystemCollector() = default;

    [[nodiscard]] std::optional<domain::SystemInfo> collect() const;

    [[nodiscard]] static std::pair<std::string, std::string> parse_os_release_stream(
        std::istream& stream);
    [[nodiscard]] static std::optional<double> parse_uptime_stream(std::istream& stream);
    [[nodiscard]] static std::optional<uint64_t> parse_btime_stream(std::istream& stream);
    [[nodiscard]] static std::string collect_hostname();
    [[nodiscard]] static std::pair<std::string, std::string> collect_uname();

  private:
    std::string os_release_path_;
    std::string uptime_path_;
    std::string stat_path_;
};

}  // namespace nodepulse::collectors
