#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <istream>
#include <limits>
#include <optional>
#include <sstream>
#include <string>
#include <string_view>
#include <unordered_map>
#include <utility>
#include <vector>

#include <nodepulse/collectors/disk_collector.hpp>

#include <sys/statvfs.h>

namespace nodepulse::collectors {

namespace {

uint64_t safe_block_multiply(uint64_t blocks, uint64_t block_size) noexcept {
    if (block_size == 0 || blocks == 0) {
        return 0;
    }
    constexpr uint64_t kMax = std::numeric_limits<uint64_t>::max();
    if (blocks > kMax / block_size) {
        return kMax;
    }
    return blocks * block_size;
}

}  // namespace

DiskCollector::DiskCollector(std::string mounts_path, std::vector<std::string> ignored_fstypes,
                             StatvfsFunc statvfs_fn)
    : mounts_path_(std::move(mounts_path)),
      ignored_fstypes_(std::move(ignored_fstypes)),
      statvfs_fn_(std::move(statvfs_fn)) {}

std::string DiskCollector::unescape_mount_path(std::string_view str) {
    std::string result;
    result.reserve(str.size());
    for (size_t i = 0; i < str.size(); ++i) {
        if (str[i] == '\\' && i + 3 < str.size()) {
            char c1 = str[i + 1];
            char c2 = str[i + 2];
            char c3 = str[i + 3];
            if (c1 >= '0' && c1 <= '7' && c2 >= '0' && c2 <= '7' && c3 >= '0' && c3 <= '7') {
                int octal_val = (c1 - '0') * 64 + (c2 - '0') * 8 + (c3 - '0');
                result.push_back(static_cast<char>(octal_val));
                i += 3;
                continue;
            }
        }
        result.push_back(str[i]);
    }
    return result;
}

bool DiskCollector::is_ignored_fstype(std::string_view fstype,
                                      const std::vector<std::string>& ignored_types) {
    for (const auto& ignored : ignored_types) {
        if (fstype == ignored) {
            return true;
        }
        if (ignored == "cgroup" && fstype == "cgroup2") {
            return true;
        }
    }
    return false;
}

std::vector<MountEntry> DiskCollector::parse_mounts_stream(std::istream& stream) {
    std::vector<MountEntry> mounts;
    std::unordered_map<std::string, size_t> mount_indices;
    std::string line;

    while (std::getline(stream, line)) {
        if (line.empty() || line.front() == '#') {
            continue;
        }

        std::istringstream iss(line);
        std::string raw_device;
        std::string raw_mount_point;
        std::string fstype;

        if (!(iss >> raw_device >> raw_mount_point >> fstype)) {
            // Malformed line (fewer than 3 tokens) is safely skipped
            continue;
        }

        std::string device = unescape_mount_path(raw_device);
        std::string mount_point = unescape_mount_path(raw_mount_point);

        // Deduplicate duplicate or shadowed mount points:
        // Later mounts in Linux mount table shadow earlier ones, so update in place
        auto it = mount_indices.find(mount_point);
        if (it != mount_indices.end()) {
            mounts[it->second] = MountEntry{std::move(device), mount_point, std::move(fstype)};
        } else {
            mount_indices[mount_point] = mounts.size();
            mounts.push_back(
                MountEntry{std::move(device), std::move(mount_point), std::move(fstype)});
        }
    }

    return mounts;
}

std::optional<domain::DiskPartitionMetrics> DiskCollector::compute_partition_metrics(
    const MountEntry& mount, const struct statvfs& stat) {
    domain::DiskPartitionMetrics metrics;
    metrics.filesystem = mount.device;
    metrics.mount_point = mount.mount_point;
    metrics.fstype = mount.fstype;

    uint64_t block_size = (stat.f_frsize > 0) ? static_cast<uint64_t>(stat.f_frsize)
                                              : static_cast<uint64_t>(stat.f_bsize);

    if (stat.f_blocks == 0 || block_size == 0) {
        metrics.total_bytes = 0;
        metrics.used_bytes = 0;
        metrics.free_bytes = 0;
        metrics.available_bytes = 0;
        metrics.usage_percent = 0.0;
        metrics.inodes_total = stat.f_files;
        metrics.inodes_free = (stat.f_ffree <= stat.f_files) ? stat.f_ffree : stat.f_files;
        return metrics;
    }

    uint64_t total_blocks = stat.f_blocks;
    uint64_t free_blocks = stat.f_bfree;
    if (free_blocks > total_blocks) {
        free_blocks = total_blocks;
    }

    uint64_t avail_blocks = stat.f_bavail;
    if (avail_blocks > free_blocks) {
        avail_blocks = free_blocks;
    }

    uint64_t used_blocks = total_blocks - free_blocks;

    metrics.total_bytes = safe_block_multiply(total_blocks, block_size);
    metrics.free_bytes = safe_block_multiply(free_blocks, block_size);
    metrics.available_bytes = safe_block_multiply(avail_blocks, block_size);
    metrics.used_bytes = safe_block_multiply(used_blocks, block_size);

    if (metrics.total_bytes > 0) {
        double pct =
            (static_cast<double>(metrics.used_bytes) / static_cast<double>(metrics.total_bytes)) *
            100.0;
        pct = std::clamp(pct, 0.0, 100.0);
        metrics.usage_percent = std::round(pct * 100.0) / 100.0;
    } else {
        metrics.usage_percent = 0.0;
    }

    metrics.inodes_total = stat.f_files;
    metrics.inodes_free = (stat.f_ffree <= stat.f_files) ? stat.f_ffree : stat.f_files;

    return metrics;
}

std::optional<std::vector<domain::DiskPartitionMetrics>> DiskCollector::collect() const {
    std::ifstream stream(mounts_path_);
    if (!stream.is_open()) {
        return std::nullopt;
    }

    auto mounts = parse_mounts_stream(stream);
    std::vector<domain::DiskPartitionMetrics> result;
    result.reserve(mounts.size());

    for (const auto& mount : mounts) {
        if (is_ignored_fstype(mount.fstype, ignored_fstypes_)) {
            continue;
        }

        struct statvfs stat {};
        int ret = -1;
        if (statvfs_fn_) {
            ret = statvfs_fn_(mount.mount_point.c_str(), &stat);
        } else {
            ret = ::statvfs(mount.mount_point.c_str(), &stat);
        }

        if (ret != 0) {
            // Gracefully skip inaccessible or disappearing mount points
            continue;
        }

        auto partition_opt = compute_partition_metrics(mount, stat);
        if (partition_opt.has_value()) {
            result.push_back(std::move(*partition_opt));
        }
    }

    return result;
}

}  // namespace nodepulse::collectors
