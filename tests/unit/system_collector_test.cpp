#include <chrono>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>

#include <gtest/gtest.h>

#include <nodepulse/collectors/system_collector.hpp>
#include <nodepulse/domain/system_info.hpp>
#include <nodepulse/services/system_service.hpp>

using nodepulse::collectors::SystemCollector;
using nodepulse::domain::SystemInfo;
using nodepulse::services::SystemService;

namespace {

#ifndef NODEPULSE_TEST_FIXTURES_DIR
#define NODEPULSE_TEST_FIXTURES_DIR "tests/fixtures"
#endif

std::string fixture_path(const std::string& subpath) {
    return std::string(NODEPULSE_TEST_FIXTURES_DIR) + "/" + subpath;
}

}  // namespace

// ============================================================================
// SystemCollector::parse_os_release_stream Tests
// ============================================================================

TEST(SystemCollectorTest, ParseOsReleaseStandard) {
    std::string content =
        "# This is a comment\n"
        "NAME=\"Ubuntu\"\n"
        "VERSION=\"24.04 LTS (Noble Numbat)\"\n"
        "ID=ubuntu\n"
        "PRETTY_NAME=\"Ubuntu 24.04 LTS\"\n"
        "VERSION_ID=\"24.04\"\n";
    std::istringstream stream(content);

    auto [os_name, os_version] = SystemCollector::parse_os_release_stream(stream);
    EXPECT_EQ(os_name, "Ubuntu 24.04 LTS");
    EXPECT_EQ(os_version, "24.04");
}

TEST(SystemCollectorTest, ParseOsReleaseFallbackToNameWhenPrettyNameMissing) {
    std::string content =
        "NAME='Arch Linux'\n"
        "ID=arch\n";
    std::istringstream stream(content);

    auto [os_name, os_version] = SystemCollector::parse_os_release_stream(stream);
    EXPECT_EQ(os_name, "Arch Linux");
    EXPECT_EQ(os_version, "");
}

TEST(SystemCollectorTest, ParseOsReleaseFallbackToVersionWhenVersionIdMissing) {
    std::string content =
        "PRETTY_NAME=\"Debian GNU/Linux 12 (bookworm)\"\n"
        "NAME=\"Debian GNU/Linux\"\n"
        "VERSION=\"12 (bookworm)\"\n";
    std::istringstream stream(content);

    auto [os_name, os_version] = SystemCollector::parse_os_release_stream(stream);
    EXPECT_EQ(os_name, "Debian GNU/Linux 12 (bookworm)");
    EXPECT_EQ(os_version, "12 (bookworm)");
}

TEST(SystemCollectorTest, ParseOsReleaseEmptyStreamDefaultsToLinux) {
    std::string content;
    std::istringstream stream(content);

    auto [os_name, os_version] = SystemCollector::parse_os_release_stream(stream);
    EXPECT_EQ(os_name, "Linux");
    EXPECT_EQ(os_version, "");
}

TEST(SystemCollectorTest, ParseOsReleaseUnquotedAndMixedQuotes) {
    std::string content =
        "PRETTY_NAME=Alpine Linux v3.19\n"
        "VERSION_ID=3.19.1\n";
    std::istringstream stream(content);

    auto [os_name, os_version] = SystemCollector::parse_os_release_stream(stream);
    EXPECT_EQ(os_name, "Alpine Linux v3.19");
    EXPECT_EQ(os_version, "3.19.1");
}

// ============================================================================
// SystemCollector::parse_uptime_stream Tests
// ============================================================================

TEST(SystemCollectorTest, ParseUptimeValid) {
    std::string content = "12345.67 98765.43\n";
    std::istringstream stream(content);

    auto uptime_opt = SystemCollector::parse_uptime_stream(stream);
    ASSERT_TRUE(uptime_opt.has_value());
    EXPECT_DOUBLE_EQ(*uptime_opt, 12345.67);
}

TEST(SystemCollectorTest, ParseUptimeZero) {
    std::string content = "0.00 0.00\n";
    std::istringstream stream(content);

    auto uptime_opt = SystemCollector::parse_uptime_stream(stream);
    ASSERT_TRUE(uptime_opt.has_value());
    EXPECT_DOUBLE_EQ(*uptime_opt, 0.0);
}

TEST(SystemCollectorTest, ParseUptimeEmptyStream) {
    std::string content;
    std::istringstream stream(content);

    auto uptime_opt = SystemCollector::parse_uptime_stream(stream);
    EXPECT_FALSE(uptime_opt.has_value());
}

TEST(SystemCollectorTest, ParseUptimeMalformedNonNumeric) {
    std::string content = "invalid_uptime 123.45\n";
    std::istringstream stream(content);

    auto uptime_opt = SystemCollector::parse_uptime_stream(stream);
    EXPECT_FALSE(uptime_opt.has_value());
}

TEST(SystemCollectorTest, ParseUptimeNegativeValue) {
    std::string content = "-100.50 200.00\n";
    std::istringstream stream(content);

    auto uptime_opt = SystemCollector::parse_uptime_stream(stream);
    EXPECT_FALSE(uptime_opt.has_value());
}

// ============================================================================
// SystemCollector::parse_btime_stream Tests
// ============================================================================

TEST(SystemCollectorTest, ParseBtimeValid) {
    std::string content =
        "cpu  1011423 918 257918 13710834 18113 0 12868 0 34008 0\n"
        "ctxt 276434145\n"
        "btime 1700000000\n"
        "processes 282436\n";
    std::istringstream stream(content);

    auto btime_opt = SystemCollector::parse_btime_stream(stream);
    ASSERT_TRUE(btime_opt.has_value());
    EXPECT_EQ(*btime_opt, 1700000000ULL);
}

TEST(SystemCollectorTest, ParseBtimeMissingFromStream) {
    std::string content =
        "cpu  1011423 918 257918 13710834 18113 0 12868 0 34008 0\n"
        "ctxt 276434145\n"
        "processes 282436\n";
    std::istringstream stream(content);

    auto btime_opt = SystemCollector::parse_btime_stream(stream);
    EXPECT_FALSE(btime_opt.has_value());
}

TEST(SystemCollectorTest, ParseBtimeMalformedNonNumeric) {
    std::string content = "btime invalid_timestamp\n";
    std::istringstream stream(content);

    auto btime_opt = SystemCollector::parse_btime_stream(stream);
    EXPECT_FALSE(btime_opt.has_value());
}

// ============================================================================
// Syscall Helpers (collect_hostname, collect_uname)
// ============================================================================

TEST(SystemCollectorTest, SyscallHelpersReturnNonEmptyValues) {
    auto hostname = SystemCollector::collect_hostname();
    EXPECT_FALSE(hostname.empty());

    auto [kernel_ver, arch] = SystemCollector::collect_uname();
    EXPECT_FALSE(kernel_ver.empty());
    EXPECT_FALSE(arch.empty());
}

// ============================================================================
// SystemCollector End-to-End Fixture Collection Tests
// ============================================================================

TEST(SystemCollectorTest, CollectFromFixtures) {
    SystemCollector collector(fixture_path("etc/os-release"), fixture_path("proc/uptime"),
                              fixture_path("proc/stat"));

    auto info_opt = collector.collect();
    ASSERT_TRUE(info_opt.has_value());

    const auto& info = *info_opt;
    EXPECT_FALSE(info.hostname.empty());
    EXPECT_EQ(info.os_name, "Ubuntu 24.04 LTS");
    EXPECT_EQ(info.os_version, "24.04");
    EXPECT_FALSE(info.kernel_version.empty());
    EXPECT_FALSE(info.architecture.empty());
    EXPECT_DOUBLE_EQ(info.uptime_seconds, 12345.67);
    EXPECT_EQ(info.boot_time_utc, 1700000000ULL);
}

TEST(SystemCollectorTest, CollectFailsWhenUptimeFileMissing) {
    SystemCollector collector(fixture_path("etc/os-release"), "/nonexistent/proc/uptime",
                              fixture_path("proc/stat"));

    auto info_opt = collector.collect();
    EXPECT_FALSE(info_opt.has_value());
}

TEST(SystemCollectorTest, CollectFallbackBootTimeWhenStatMissingBtime) {
    // /proc/uptime exists, but stat file does not contain btime
    SystemCollector collector(fixture_path("etc/os-release"), fixture_path("proc/uptime"),
                              "/nonexistent/proc/stat");

    auto info_opt = collector.collect();
    ASSERT_TRUE(info_opt.has_value());

    const auto& info = *info_opt;
    // Fallback boot time should be calculated using current epoch - uptime_seconds
    auto now_epoch = static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::seconds>(
                                               std::chrono::system_clock::now().time_since_epoch())
                                               .count());
    EXPECT_GT(info.boot_time_utc, 0ULL);
    EXPECT_LE(info.boot_time_utc, now_epoch);
    EXPECT_NEAR(static_cast<double>(info.boot_time_utc),
                static_cast<double>(now_epoch - static_cast<uint64_t>(info.uptime_seconds)), 5.0);
}

TEST(SystemCollectorTest, CollectFallbackOsWhenOsReleaseMissing) {
    SystemCollector collector("/nonexistent/etc/os-release", fixture_path("proc/uptime"),
                              fixture_path("proc/stat"));

    auto info_opt = collector.collect();
    ASSERT_TRUE(info_opt.has_value());

    const auto& info = *info_opt;
    EXPECT_EQ(info.os_name, "Linux");
    EXPECT_EQ(info.os_version, "unknown");
}

// ============================================================================
// SystemService Tests
// ============================================================================

TEST(SystemServiceTest, ServiceDelegatesToCollector) {
    auto collector = std::make_shared<SystemCollector>(
        fixture_path("etc/os-release"), fixture_path("proc/uptime"), fixture_path("proc/stat"));
    SystemService service(collector);

    auto info_opt = service.get_system_info();
    ASSERT_TRUE(info_opt.has_value());
    EXPECT_EQ(info_opt->os_name, "Ubuntu 24.04 LTS");
    EXPECT_DOUBLE_EQ(info_opt->uptime_seconds, 12345.67);
}

TEST(SystemServiceTest, ServiceHandlesCollectorFailure) {
    auto collector = std::make_shared<SystemCollector>(
        fixture_path("etc/os-release"), "/nonexistent/proc/uptime", fixture_path("proc/stat"));
    SystemService service(collector);

    auto info_opt = service.get_system_info();
    EXPECT_FALSE(info_opt.has_value());
}

TEST(SystemServiceTest, ServiceDefaultConstructorCreatesLiveCollector) {
    SystemService service;
    auto info_opt = service.get_system_info();
    ASSERT_TRUE(info_opt.has_value());
    EXPECT_FALSE(info_opt->hostname.empty());
    EXPECT_GT(info_opt->uptime_seconds, 0.0);
    EXPECT_GT(info_opt->boot_time_utc, 0ULL);
}
