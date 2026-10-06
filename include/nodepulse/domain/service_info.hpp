#pragma once

#include <cstdint>
#include <string>

namespace nodepulse::domain {

struct ServiceInfo {
    std::string name;
    std::string description;
    std::string load_state{"unknown"};
    std::string active_state{"unknown"};
    std::string sub_state{"unknown"};
    std::string unit_file_state{"unknown"};
};

struct ServiceDetail {
    std::string name;
    std::string description;
    std::string load_state{"unknown"};
    std::string active_state{"unknown"};
    std::string sub_state{"unknown"};
    std::string unit_file_state{"unknown"};
    int32_t main_pid{0};
    uint32_t restart_count{0};
    uint64_t active_enter_timestamp_utc{0};
    uint64_t memory_current_bytes{0};
};

}  // namespace nodepulse::domain
