#pragma once

#include <cstdint>

namespace nodepulse::domain {

struct MemoryMetrics {
    uint64_t total_bytes{0};
    uint64_t used_bytes{0};
    uint64_t free_bytes{0};
    uint64_t available_bytes{0};
    uint64_t buffers_bytes{0};
    uint64_t cached_bytes{0};
    double usage_percent{0.0};

    uint64_t swap_total_bytes{0};
    uint64_t swap_free_bytes{0};
    uint64_t swap_used_bytes{0};
    double swap_usage_percent{0.0};
};

}  // namespace nodepulse::domain
