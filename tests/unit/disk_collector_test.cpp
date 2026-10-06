#include <cerrno>
#include <chrono>
#include <fstream>
#include <future>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include <nodepulse/collectors/disk_collector.hpp>
#include <nodepulse/domain/disk_info.hpp>
#include <nodepulse/services/disk_service.hpp>

#include <sys/statvfs.h>

using nodepulse::collectors::DiskCollector;
using nodepulse::collectors::MountEntry;
using nodepulse::domain::DiskPartitionMetrics;
using nodepulse::services::DiskService;

namespace {

#ifndef NODEPULSE_TEST_FIXTURES_DIR
#define NODEPULSE_TEST_FIXTURES_DIR "tests/fixtures"
#endif

std::string fixture_path(const std::string& subpath) {
    return std::string(NODEPULSE_TEST_FIXTURES_DIR) + "/" + subpath;
}

}  // namespace

// ============================================================================
// Mount Stream Parser Tests
// ============================================================================

TEST(DiskCollectorTest, UnescapeMountPath) {
    EXPECT_EQ(DiskCollector::unescape_mount_path("/media/USB\\040Drive"), "/media/USB Drive");
    EXPECT_EQ(DiskCollector::unescape_mount_path("/mnt/Path\\040With\\040Spaces"),
              "/mnt/Path With Spaces");
    EXPECT_EQ(DiskCollector::unescape_mount_path("/mnt/Tab\\011And\\134Backslash"),
              "/mnt/Tab\tAnd\\Backslash");
    EXPECT_EQ(DiskCollector::unescape_mount_path("/normal/path"), "/normal/path");
    EXPECT_EQ(DiskCollector::unescape_mount_path("/trailing\\04"), "/trailing\\04");
    EXPECT_EQ(DiskCollector::unescape_mount_path(""), "");
}

TEST(DiskCollectorTest, ParseMountsStreamValid) {
    std::string content =
        "/dev/nvme0n1p2 / ext4 rw,relatime 0 0\n"
        "/dev/sda1 /data xfs rw,relatime 0 0\n"
        "proc /proc proc rw,nosuid,nodev,noexec 0 0\n";
    std::istringstream stream(content);

    auto mounts = DiskCollector::parse_mounts_stream(stream);
    ASSERT_EQ(mounts.size(), 3U);

    EXPECT_EQ(mounts[0].device, "/dev/nvme0n1p2");
    EXPECT_EQ(mounts[0].mount_point, "/");
    EXPECT_EQ(mounts[0].fstype, "ext4");

    EXPECT_EQ(mounts[1].device, "/dev/sda1");
    EXPECT_EQ(mounts[1].mount_point, "/data");
    EXPECT_EQ(mounts[1].fstype, "xfs");

    EXPECT_EQ(mounts[2].device, "proc");
    EXPECT_EQ(mounts[2].mount_point, "/proc");
    EXPECT_EQ(mounts[2].fstype, "proc");
}

TEST(DiskCollectorTest, ParseMountsStreamEscapedPaths) {
    std::string content =
        "/dev/sdb1 /media/USB\\040Drive vfat rw,relatime 0 0\n"
        "/dev/sdc1 /mnt/Path\\040With\\040Spaces ext4 rw,relatime 0 0\n";
    std::istringstream stream(content);

    auto mounts = DiskCollector::parse_mounts_stream(stream);
    ASSERT_EQ(mounts.size(), 2U);

    EXPECT_EQ(mounts[0].mount_point, "/media/USB Drive");
    EXPECT_EQ(mounts[1].mount_point, "/mnt/Path With Spaces");
}

TEST(DiskCollectorTest, ParseMountsStreamDuplicateMountPoints) {
    std::string content =
        "/dev/sda1 /mnt ext4 rw,relatime 0 0\n"
        "/dev/sdb1 /mnt ext4 rw,relatime 0 0\n"
        "/dev/sdc1 /data ext4 rw,relatime 0 0\n"
        "/dev/sdc1 /data ext4 rw,relatime 0 0\n";
    std::istringstream stream(content);

    auto mounts = DiskCollector::parse_mounts_stream(stream);
    ASSERT_EQ(mounts.size(), 2U);

    // Later mount shadows earlier one
    EXPECT_EQ(mounts[0].mount_point, "/mnt");
    EXPECT_EQ(mounts[0].device, "/dev/sdb1");

    EXPECT_EQ(mounts[1].mount_point, "/data");
    EXPECT_EQ(mounts[1].device, "/dev/sdc1");
}

TEST(DiskCollectorTest, ParseMountsStreamMalformedLines) {
    std::string content =
        "SingleTokenLine\n"
        "/dev/sda1\n"
        "/dev/sda2 /only/two/tokens\n"
        "/dev/nvme0n1p2 / ext4 rw,relatime 0 0\n";
    std::istringstream stream(content);

    auto mounts = DiskCollector::parse_mounts_stream(stream);
    ASSERT_EQ(mounts.size(), 1U);
    EXPECT_EQ(mounts[0].device, "/dev/nvme0n1p2");
    EXPECT_EQ(mounts[0].mount_point, "/");
}

TEST(DiskCollectorTest, PseudoFilesystemFiltering) {
    std::vector<std::string> ignored = {"proc", "sysfs", "cgroup", "devpts", "tmpfs", "overlay"};

    EXPECT_TRUE(DiskCollector::is_ignored_fstype("proc", ignored));
    EXPECT_TRUE(DiskCollector::is_ignored_fstype("sysfs", ignored));
    EXPECT_TRUE(DiskCollector::is_ignored_fstype("cgroup", ignored));
    EXPECT_TRUE(DiskCollector::is_ignored_fstype("cgroup2", ignored));
    EXPECT_TRUE(DiskCollector::is_ignored_fstype("devpts", ignored));
    EXPECT_TRUE(DiskCollector::is_ignored_fstype("tmpfs", ignored));
    EXPECT_TRUE(DiskCollector::is_ignored_fstype("overlay", ignored));

    EXPECT_FALSE(DiskCollector::is_ignored_fstype("ext4", ignored));
    EXPECT_FALSE(DiskCollector::is_ignored_fstype("xfs", ignored));
    EXPECT_FALSE(DiskCollector::is_ignored_fstype("btrfs", ignored));
    EXPECT_FALSE(DiskCollector::is_ignored_fstype("vfat", ignored));
}

// ============================================================================
// statvfs Calculation Tests
// ============================================================================

TEST(DiskCollectorTest, ComputePartitionMetricsNormal) {
    MountEntry mount{"/dev/nvme0n1p2", "/", "ext4"};
    struct statvfs stat {};
    stat.f_frsize = 4096;
    stat.f_bsize = 4096;
    stat.f_blocks = 131072000;  // 500 GiB
    stat.f_bfree = 104857600;   // 400 GiB
    stat.f_bavail = 99614720;   // 380 GiB (unprivileged free)
    stat.f_files = 32768000;
    stat.f_ffree = 31200000;

    auto metrics_opt = DiskCollector::compute_partition_metrics(mount, stat);
    ASSERT_TRUE(metrics_opt.has_value());

    const auto& m = *metrics_opt;
    EXPECT_EQ(m.filesystem, "/dev/nvme0n1p2");
    EXPECT_EQ(m.mount_point, "/");
    EXPECT_EQ(m.fstype, "ext4");
    EXPECT_EQ(m.total_bytes, 536870912000ULL);
    EXPECT_EQ(m.free_bytes, 429496729600ULL);
    EXPECT_EQ(m.available_bytes, 408021893120ULL);
    EXPECT_EQ(m.used_bytes, 107374182400ULL);
    EXPECT_DOUBLE_EQ(m.usage_percent, 20.0);
    EXPECT_EQ(m.inodes_total, 32768000ULL);
    EXPECT_EQ(m.inodes_free, 31200000ULL);
}

TEST(DiskCollectorTest, ComputePartitionMetricsZeroCapacity) {
    MountEntry mount{"none", "/empty", "ramfs"};
    struct statvfs stat {};
    stat.f_frsize = 4096;
    stat.f_bsize = 4096;
    stat.f_blocks = 0;
    stat.f_bfree = 0;
    stat.f_bavail = 0;
    stat.f_files = 0;
    stat.f_ffree = 0;

    auto metrics_opt = DiskCollector::compute_partition_metrics(mount, stat);
    ASSERT_TRUE(metrics_opt.has_value());

    const auto& m = *metrics_opt;
    EXPECT_EQ(m.total_bytes, 0ULL);
    EXPECT_EQ(m.used_bytes, 0ULL);
    EXPECT_EQ(m.free_bytes, 0ULL);
    EXPECT_EQ(m.available_bytes, 0ULL);
    EXPECT_DOUBLE_EQ(m.usage_percent, 0.0);
}

TEST(DiskCollectorTest, ComputePartitionMetricsClampingAndSafety) {
    MountEntry mount{"/dev/sda1", "/test", "ext4"};
    struct statvfs stat {};
    stat.f_frsize = 1024;
    stat.f_bsize = 1024;
    stat.f_blocks = 1000;
    stat.f_bfree = 1500;   // Free > total (inconsistent)
    stat.f_bavail = 1600;  // Avail > free (inconsistent)
    stat.f_files = 100;
    stat.f_ffree = 150;  // Inodes free > total

    auto metrics_opt = DiskCollector::compute_partition_metrics(mount, stat);
    ASSERT_TRUE(metrics_opt.has_value());

    const auto& m = *metrics_opt;
    EXPECT_EQ(m.total_bytes, 1000ULL * 1024ULL);
    EXPECT_EQ(m.free_bytes, 1000ULL * 1024ULL);       // Clamped to total
    EXPECT_EQ(m.available_bytes, 1000ULL * 1024ULL);  // Clamped to free
    EXPECT_EQ(m.used_bytes, 0ULL);                    // No underflow
    EXPECT_DOUBLE_EQ(m.usage_percent, 0.0);
    EXPECT_EQ(m.inodes_total, 100ULL);
    EXPECT_EQ(m.inodes_free, 100ULL);  // Clamped
}

TEST(DiskCollectorTest, ComputePartitionMetricsOverflowProtection) {
    MountEntry mount{"/dev/sda1", "/test", "ext4"};
    struct statvfs stat {};
    stat.f_frsize = 4096;
    stat.f_bsize = 4096;
    stat.f_blocks = std::numeric_limits<uint64_t>::max();
    stat.f_bfree = std::numeric_limits<uint64_t>::max();
    stat.f_bavail = std::numeric_limits<uint64_t>::max();
    stat.f_files = 100;
    stat.f_ffree = 50;

    auto metrics_opt = DiskCollector::compute_partition_metrics(mount, stat);
    ASSERT_TRUE(metrics_opt.has_value());
    EXPECT_EQ(metrics_opt->total_bytes, std::numeric_limits<uint64_t>::max());
}

// ============================================================================
// Fixture File and Mock Collection Tests
// ============================================================================

TEST(DiskCollectorTest, CollectFromValidFixtureWithMockStatvfs) {
    auto mock_statvfs = [](const char* path, struct statvfs* buf) -> int {
        std::string p(path);
        if (p == "/") {
            buf->f_frsize = 4096;
            buf->f_bsize = 4096;
            buf->f_blocks = 131072000;  // 500 GiB
            buf->f_bfree = 104857600;   // 400 GiB
            buf->f_bavail = 99614720;   // 380 GiB
            buf->f_files = 32768000;
            buf->f_ffree = 31200000;
            return 0;
        }
        if (p == "/data") {
            buf->f_frsize = 4096;
            buf->f_bsize = 4096;
            buf->f_blocks = 262144000;  // 1 TiB
            buf->f_bfree = 131072000;   // 512 GiB
            buf->f_bavail = 131072000;
            buf->f_files = 65536000;
            buf->f_ffree = 60000000;
            return 0;
        }
        if (p == "/boot/efi") {
            buf->f_frsize = 4096;
            buf->f_bsize = 4096;
            buf->f_blocks = 131072;  // 512 MiB
            buf->f_bfree = 100000;
            buf->f_bavail = 100000;
            buf->f_files = 10000;
            buf->f_ffree = 9900;
            return 0;
        }
        return -1;
    };

    DiskCollector collector(fixture_path("proc/mounts_valid"),
                            {"proc", "sysfs", "cgroup", "devpts", "tmpfs", "overlay"},
                            mock_statvfs);

    auto partitions_opt = collector.collect();
    ASSERT_TRUE(partitions_opt.has_value());
    const auto& partitions = *partitions_opt;

    // 3 non-ignored filesystems: /, /data, /boot/efi
    ASSERT_EQ(partitions.size(), 3U);

    EXPECT_EQ(partitions[0].mount_point, "/");
    EXPECT_EQ(partitions[0].filesystem, "/dev/nvme0n1p2");
    EXPECT_EQ(partitions[0].fstype, "ext4");
    EXPECT_EQ(partitions[0].total_bytes, 536870912000ULL);
    EXPECT_DOUBLE_EQ(partitions[0].usage_percent, 20.0);

    EXPECT_EQ(partitions[1].mount_point, "/data");
    EXPECT_EQ(partitions[1].filesystem, "/dev/sda1");
    EXPECT_EQ(partitions[1].fstype, "xfs");
    EXPECT_DOUBLE_EQ(partitions[1].usage_percent, 50.0);

    EXPECT_EQ(partitions[2].mount_point, "/boot/efi");
    EXPECT_EQ(partitions[2].fstype, "vfat");
}

TEST(DiskCollectorTest, CollectHandlesInaccessibleOrDisappearingMount) {
    auto mock_statvfs = [](const char* path, struct statvfs* buf) -> int {
        std::string p(path);
        if (p == "/") {
            buf->f_frsize = 4096;
            buf->f_bsize = 4096;
            buf->f_blocks = 100000;
            buf->f_bfree = 50000;
            buf->f_bavail = 50000;
            buf->f_files = 1000;
            buf->f_ffree = 500;
            return 0;
        }
        // /data is inaccessible or disappeared
        errno = ENOENT;
        return -1;
    };

    DiskCollector collector(fixture_path("proc/mounts_valid"),
                            {"proc", "sysfs", "cgroup", "devpts", "tmpfs", "overlay", "vfat"},
                            mock_statvfs);

    auto partitions_opt = collector.collect();
    ASSERT_TRUE(partitions_opt.has_value());

    // Only / should be collected, /data skipped gracefully
    ASSERT_EQ(partitions_opt->size(), 1U);
    EXPECT_EQ((*partitions_opt)[0].mount_point, "/");
}

TEST(DiskCollectorTest, CollectFromNonexistentMountsFile) {
    DiskCollector collector("/nonexistent/proc/mounts");
    auto partitions_opt = collector.collect();
    EXPECT_FALSE(partitions_opt.has_value());
}

// ============================================================================
// DiskService Tests
// ============================================================================

TEST(DiskServiceTest, ServiceDelegatesToCollector) {
    auto mock_statvfs = [](const char*, struct statvfs* buf) -> int {
        buf->f_frsize = 4096;
        buf->f_bsize = 4096;
        buf->f_blocks = 100000;
        buf->f_bfree = 50000;
        buf->f_bavail = 50000;
        buf->f_files = 1000;
        buf->f_ffree = 500;
        return 0;
    };

    auto collector = std::make_shared<DiskCollector>(
        fixture_path("proc/mounts_valid"),
        std::vector<std::string>{"proc", "sysfs", "cgroup", "devpts", "tmpfs", "overlay", "vfat",
                                 "xfs"},
        mock_statvfs);
    DiskService service(collector);

    auto metrics_opt = service.get_disk_metrics();
    ASSERT_TRUE(metrics_opt.has_value());
    ASSERT_EQ(metrics_opt->size(), 1U);
    EXPECT_EQ((*metrics_opt)[0].mount_point, "/");
}

TEST(DiskServiceTest, ServiceHandlesNullptrCollectorSafely) {
    DiskService service(nullptr);
#ifdef __linux__
    auto metrics_opt = service.get_disk_metrics();
    EXPECT_TRUE(metrics_opt.has_value());
#endif
}

TEST(DiskServiceTest, AsyncCollectionExecutesOnWorkerThread) {
    auto mock_statvfs = [](const char*, struct statvfs* buf) -> int {
        buf->f_frsize = 4096;
        buf->f_bsize = 4096;
        buf->f_blocks = 100000;
        buf->f_bfree = 50000;
        buf->f_bavail = 50000;
        buf->f_files = 1000;
        buf->f_ffree = 500;
        return 0;
    };

    auto collector = std::make_shared<DiskCollector>(
        fixture_path("proc/mounts_valid"),
        std::vector<std::string>{"proc", "sysfs", "cgroup", "devpts", "tmpfs", "overlay", "vfat",
                                 "xfs"},
        mock_statvfs);
    DiskService service(collector);

    std::promise<std::thread::id> worker_thread_id_promise;
    std::promise<std::optional<std::vector<DiskPartitionMetrics>>> metrics_promise;
    auto worker_thread_id_future = worker_thread_id_promise.get_future();
    auto metrics_future = metrics_promise.get_future();

    auto calling_thread_id = std::this_thread::get_id();

    service.get_disk_metrics_async([&worker_thread_id_promise, &metrics_promise](auto metrics_opt) {
        worker_thread_id_promise.set_value(std::this_thread::get_id());
        metrics_promise.set_value(std::move(metrics_opt));
    });

    ASSERT_EQ(worker_thread_id_future.wait_for(std::chrono::seconds(2)), std::future_status::ready);
    ASSERT_EQ(metrics_future.wait_for(std::chrono::seconds(2)), std::future_status::ready);

    auto worker_thread_id = worker_thread_id_future.get();
    auto metrics_opt = metrics_future.get();

    // Verify collection did not execute on the calling thread
    EXPECT_NE(worker_thread_id, calling_thread_id);

    ASSERT_TRUE(metrics_opt.has_value());
    ASSERT_EQ(metrics_opt->size(), 1U);
    EXPECT_EQ((*metrics_opt)[0].mount_point, "/");
}

// ============================================================================
// Live Linux Host Sanity Test
// ============================================================================

#ifdef __linux__
TEST(DiskCollectorTest, LiveLinuxHostDisksSanity) {
    DiskCollector collector;
    auto partitions_opt = collector.collect();
    ASSERT_TRUE(partitions_opt.has_value());

    // On Linux hosts, at least root '/' must be present
    EXPECT_GE(partitions_opt->size(), 1U);

    bool found_root = false;
    for (const auto& partition : *partitions_opt) {
        EXPECT_FALSE(partition.filesystem.empty());
        EXPECT_FALSE(partition.mount_point.empty());
        EXPECT_FALSE(partition.fstype.empty());
        EXPECT_GE(partition.usage_percent, 0.0);
        EXPECT_LE(partition.usage_percent, 100.0);
        EXPECT_LE(partition.used_bytes, partition.total_bytes);
        EXPECT_LE(partition.available_bytes, partition.free_bytes);

        if (partition.mount_point == "/") {
            found_root = true;
            EXPECT_GT(partition.total_bytes, 0ULL);
        }
    }
    EXPECT_TRUE(found_root);
}
#endif
