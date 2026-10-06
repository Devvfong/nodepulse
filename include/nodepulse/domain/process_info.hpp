#pragma once

#include <cstdint>
#include <string>

namespace nodepulse::domain {

struct ProcessInfo {
    int32_t pid{0};
    std::string name;
    std::string user;
    std::string state{"S"};
    double cpu_percent{0.0};
    uint64_t memory_rss_bytes{0};
    std::string cmdline;
};

struct ProcessDetail {
    int32_t pid{0};
    int32_t ppid{0};
    std::string name;
    std::string user;
    std::string state{"S"};
    double cpu_percent{0.0};
    uint64_t memory_rss_bytes{0};
    uint64_t memory_vms_bytes{0};
    uint32_t thread_count{0};
    uint32_t open_fd_count{0};
    uint64_t start_time_epoch{0};
    std::string cmdline;
    std::string working_directory;
};

}  // namespace nodepulse::domain
