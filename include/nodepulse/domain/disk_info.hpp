#pragma once

#include <cstdint>
#include <string>

namespace nodepulse::domain {

struct DiskPartitionMetrics {
    std::string filesystem;   // e.g. "/dev/nvme0n1p2"
    std::string mount_point;  // e.g. "/"
    std::string fstype;       // e.g. "ext4"
    uint64_t total_bytes{0};
    uint64_t used_bytes{0};
    uint64_t free_bytes{0};
    uint64_t available_bytes{0};
    double usage_percent{0.0};
    uint64_t inodes_total{0};
    uint64_t inodes_free{0};
};

}  // namespace nodepulse::domain
