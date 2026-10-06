#pragma once

#include <cstdint>
#include <string>

namespace nodepulse::domain {

struct SystemInfo {
    std::string hostname;
    std::string os_name;
    std::string os_version;
    std::string kernel_version;
    std::string architecture;
    uint64_t boot_time_utc{0};
    double uptime_seconds{0.0};
};

}  // namespace nodepulse::domain
