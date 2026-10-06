#pragma once

#include <cstdint>
#include <functional>
#include <iosfwd>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <nodepulse/domain/disk_info.hpp>

#include <sys/statvfs.h>

namespace nodepulse::collectors {

struct MountEntry {
    std::string device;
    std::string mount_point;
    std::string fstype;
};

using StatvfsFunc = std::function<int(const char*, struct statvfs*)>;

class DiskCollector {
  public:
    explicit DiskCollector(std::string mounts_path = "/proc/mounts",
                           std::vector<std::string> ignored_fstypes = {"proc", "sysfs", "cgroup",
                                                                       "devpts", "tmpfs",
                                                                       "overlay"},
                           StatvfsFunc statvfs_fn = nullptr);
    virtual ~DiskCollector() = default;

    [[nodiscard]] virtual std::optional<std::vector<domain::DiskPartitionMetrics>> collect() const;

    [[nodiscard]] static std::vector<MountEntry> parse_mounts_stream(std::istream& stream);
    [[nodiscard]] static std::string unescape_mount_path(std::string_view str);
    [[nodiscard]] static bool is_ignored_fstype(std::string_view fstype,
                                                const std::vector<std::string>& ignored_types);

    [[nodiscard]] static std::optional<domain::DiskPartitionMetrics> compute_partition_metrics(
        const MountEntry& mount, const struct statvfs& stat);

  private:
    std::string mounts_path_;
    std::vector<std::string> ignored_fstypes_;
    StatvfsFunc statvfs_fn_;
};

}  // namespace nodepulse::collectors
