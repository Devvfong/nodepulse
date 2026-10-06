#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace nodepulse::domain {

struct LoadAverage {
    double one_minute{0.0};
    double five_minute{0.0};
    double fifteen_minute{0.0};
};

struct CpuCoreMetrics {
    uint32_t core_id{0};
    std::optional<double> usage_percent{std::nullopt};
    uint64_t user_jiffies{0};
    uint64_t system_jiffies{0};
    uint64_t idle_jiffies{0};
    uint64_t iowait_jiffies{0};
};

struct CpuMetrics {
    std::optional<double> usage_percent{std::nullopt};
    LoadAverage load_average;
    std::string model_name;
    uint32_t physical_cores{0};
    uint32_t logical_cores{0};
    std::vector<CpuCoreMetrics> cores;
    std::string measurement_status{"warming_up"};
};

}  // namespace nodepulse::domain
