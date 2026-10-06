#include <fstream>
#include <memory>
#include <sstream>
#include <string>

#include <gtest/gtest.h>

#include <nodepulse/collectors/memory_collector.hpp>
#include <nodepulse/domain/memory_info.hpp>
#include <nodepulse/services/memory_service.hpp>

using nodepulse::collectors::MemoryCollector;
using nodepulse::domain::MemoryMetrics;
using nodepulse::services::MemoryService;

namespace {

#ifndef NODEPULSE_TEST_FIXTURES_DIR
#define NODEPULSE_TEST_FIXTURES_DIR "tests/fixtures"
#endif

std::string fixture_path(const std::string& subpath) {
    return std::string(NODEPULSE_TEST_FIXTURES_DIR) + "/" + subpath;
}

}  // namespace

// ============================================================================
// MemoryCollector::parse_meminfo_stream Tests
// ============================================================================

TEST(MemoryCollectorTest, ParseMeminfoStreamValid) {
    std::string content =
        "MemTotal:       16384000 kB\n"
        "MemFree:         4096000 kB\n"
        "MemAvailable:   10240000 kB\n"
        "Buffers:          512000 kB\n"
        "Cached:          5632000 kB\n"
        "SwapCached:            0 kB\n"
        "Active:          6000000 kB\n"
        "Inactive:        4000000 kB\n"
        "Active(anon):    4500000 kB\n"
        "Inactive(anon):  1500000 kB\n"
        "Active(file):    1500000 kB\n"
        "Inactive(file):  2500000 kB\n"
        "SwapTotal:       8192000 kB\n"
        "SwapFree:        8192000 kB\n";
    std::istringstream stream(content);

    auto metrics_opt = MemoryCollector::parse_meminfo_stream(stream);
    ASSERT_TRUE(metrics_opt.has_value());

    const auto& m = *metrics_opt;
    EXPECT_EQ(m.total_bytes, 16384000ULL * 1024ULL);
    EXPECT_EQ(m.free_bytes, 4096000ULL * 1024ULL);
    EXPECT_EQ(m.available_bytes, 10240000ULL * 1024ULL);
    EXPECT_EQ(m.buffers_bytes, 512000ULL * 1024ULL);
    EXPECT_EQ(m.cached_bytes, 5632000ULL * 1024ULL);
    EXPECT_EQ(m.used_bytes, (16384000ULL - 10240000ULL) * 1024ULL);
    EXPECT_DOUBLE_EQ(m.usage_percent, 37.5);

    EXPECT_EQ(m.swap_total_bytes, 8192000ULL * 1024ULL);
    EXPECT_EQ(m.swap_free_bytes, 8192000ULL * 1024ULL);
    EXPECT_EQ(m.swap_used_bytes, 0ULL);
    EXPECT_DOUBLE_EQ(m.swap_usage_percent, 0.0);
}

TEST(MemoryCollectorTest, ParseMeminfoStreamZeroSwap) {
    std::string content =
        "MemTotal:       16384000 kB\n"
        "MemFree:         4096000 kB\n"
        "MemAvailable:   10240000 kB\n"
        "Buffers:          512000 kB\n"
        "Cached:          5632000 kB\n"
        "SwapTotal:             0 kB\n"
        "SwapFree:              0 kB\n";
    std::istringstream stream(content);

    auto metrics_opt = MemoryCollector::parse_meminfo_stream(stream);
    ASSERT_TRUE(metrics_opt.has_value());

    const auto& m = *metrics_opt;
    EXPECT_EQ(m.swap_total_bytes, 0ULL);
    EXPECT_EQ(m.swap_free_bytes, 0ULL);
    EXPECT_EQ(m.swap_used_bytes, 0ULL);
    EXPECT_DOUBLE_EQ(m.swap_usage_percent, 0.0);
    EXPECT_DOUBLE_EQ(m.usage_percent, 37.5);
}

TEST(MemoryCollectorTest, ParseMeminfoStreamMissingAvailableFallback) {
    std::string content =
        "MemTotal:       16384000 kB\n"
        "MemFree:         4096000 kB\n"
        "Buffers:          512000 kB\n"
        "Cached:          5632000 kB\n"
        "SwapTotal:       8192000 kB\n"
        "SwapFree:        4096000 kB\n";
    std::istringstream stream(content);

    auto metrics_opt = MemoryCollector::parse_meminfo_stream(stream);
    ASSERT_TRUE(metrics_opt.has_value());

    const auto& m = *metrics_opt;
    // Fallback: 4096000 + 512000 + 5632000 = 10240000 kB
    EXPECT_EQ(m.available_bytes, 10240000ULL * 1024ULL);
    EXPECT_EQ(m.used_bytes, (16384000ULL - 10240000ULL) * 1024ULL);
    EXPECT_DOUBLE_EQ(m.usage_percent, 37.5);

    EXPECT_EQ(m.swap_total_bytes, 8192000ULL * 1024ULL);
    EXPECT_EQ(m.swap_free_bytes, 4096000ULL * 1024ULL);
    EXPECT_EQ(m.swap_used_bytes, 4096000ULL * 1024ULL);
    EXPECT_DOUBLE_EQ(m.swap_usage_percent, 50.0);
}

TEST(MemoryCollectorTest, ParseMeminfoStreamMalformed) {
    std::string content =
        "NotAValidMeminfoFile\n"
        "Corrupted: 123\n"
        "InvalidField: not_a_number kB\n";
    std::istringstream stream(content);

    auto metrics_opt = MemoryCollector::parse_meminfo_stream(stream);
    EXPECT_FALSE(metrics_opt.has_value());
}

TEST(MemoryCollectorTest, ParseMeminfoStreamOverflow) {
    std::string content =
        "MemTotal:       18446744073709551615 kB\n"
        "MemFree:         4096000 kB\n"
        "MemAvailable:   10240000 kB\n"
        "Buffers:          512000 kB\n"
        "Cached:          5632000 kB\n"
        "SwapTotal:       8192000 kB\n"
        "SwapFree:        8192000 kB\n";
    std::istringstream stream(content);

    auto metrics_opt = MemoryCollector::parse_meminfo_stream(stream);
    EXPECT_FALSE(metrics_opt.has_value());
}

TEST(MemoryCollectorTest, ParseMeminfoStreamMissingMemTotal) {
    std::string content =
        "MemFree:         4096000 kB\n"
        "MemAvailable:    2048000 kB\n";
    std::istringstream stream(content);

    auto metrics_opt = MemoryCollector::parse_meminfo_stream(stream);
    EXPECT_FALSE(metrics_opt.has_value());
}

TEST(MemoryCollectorTest, ParseMeminfoStreamMissingMemFree) {
    std::string content =
        "MemTotal:       16384000 kB\n"
        "MemAvailable:   10240000 kB\n";
    std::istringstream stream(content);

    auto metrics_opt = MemoryCollector::parse_meminfo_stream(stream);
    EXPECT_FALSE(metrics_opt.has_value());
}

TEST(MemoryCollectorTest, ParseMeminfoStreamZeroMemTotal) {
    std::string content =
        "MemTotal:              0 kB\n"
        "MemFree:               0 kB\n";
    std::istringstream stream(content);

    auto metrics_opt = MemoryCollector::parse_meminfo_stream(stream);
    EXPECT_FALSE(metrics_opt.has_value());
}

TEST(MemoryCollectorTest, ParseMeminfoStreamNegativeValue) {
    std::string content =
        "MemTotal:       -1638400 kB\n"
        "MemFree:         4096000 kB\n";
    std::istringstream stream(content);

    auto metrics_opt = MemoryCollector::parse_meminfo_stream(stream);
    EXPECT_FALSE(metrics_opt.has_value());
}

TEST(MemoryCollectorTest, ParseMeminfoStreamInvalidUnit) {
    std::string content =
        "MemTotal:       16384000 MB\n"
        "MemFree:         4096000 kB\n";
    std::istringstream stream(content);

    auto metrics_opt = MemoryCollector::parse_meminfo_stream(stream);
    EXPECT_FALSE(metrics_opt.has_value());
}

TEST(MemoryCollectorTest, ParseMeminfoStreamClampingAndNoUnderflow) {
    // MemAvailable greater than MemTotal, SwapFree greater than SwapTotal
    std::string content =
        "MemTotal:       1000 kB\n"
        "MemFree:        2000 kB\n"
        "MemAvailable:   2500 kB\n"
        "SwapTotal:       500 kB\n"
        "SwapFree:        800 kB\n";
    std::istringstream stream(content);

    auto metrics_opt = MemoryCollector::parse_meminfo_stream(stream);
    ASSERT_TRUE(metrics_opt.has_value());

    const auto& m = *metrics_opt;
    EXPECT_EQ(m.total_bytes, 1000ULL * 1024ULL);
    EXPECT_EQ(m.free_bytes, 1000ULL * 1024ULL);       // clamped to total
    EXPECT_EQ(m.available_bytes, 1000ULL * 1024ULL);  // clamped to total
    EXPECT_EQ(m.used_bytes, 0ULL);                    // total - available clamped
    EXPECT_DOUBLE_EQ(m.usage_percent, 0.0);

    EXPECT_EQ(m.swap_total_bytes, 500ULL * 1024ULL);
    EXPECT_EQ(m.swap_free_bytes, 500ULL * 1024ULL);  // clamped to swap total
    EXPECT_EQ(m.swap_used_bytes, 0ULL);              // swap_total - swap_free clamped
    EXPECT_DOUBLE_EQ(m.swap_usage_percent, 0.0);
}

// ============================================================================
// Fixture File Tests
// ============================================================================

TEST(MemoryCollectorTest, CollectFromValidFixtureFile) {
    MemoryCollector collector(fixture_path("proc/meminfo_valid"));
    auto metrics_opt = collector.collect();
    ASSERT_TRUE(metrics_opt.has_value());
    EXPECT_EQ(metrics_opt->total_bytes, 16384000ULL * 1024ULL);
    EXPECT_DOUBLE_EQ(metrics_opt->usage_percent, 37.5);
    EXPECT_DOUBLE_EQ(metrics_opt->swap_usage_percent, 0.0);
}

TEST(MemoryCollectorTest, CollectFromZeroSwapFixtureFile) {
    MemoryCollector collector(fixture_path("proc/meminfo_zero_swap"));
    auto metrics_opt = collector.collect();
    ASSERT_TRUE(metrics_opt.has_value());
    EXPECT_EQ(metrics_opt->swap_total_bytes, 0ULL);
    EXPECT_EQ(metrics_opt->swap_used_bytes, 0ULL);
    EXPECT_DOUBLE_EQ(metrics_opt->swap_usage_percent, 0.0);
}

TEST(MemoryCollectorTest, CollectFromMissingAvailableFixtureFile) {
    MemoryCollector collector(fixture_path("proc/meminfo_missing_available"));
    auto metrics_opt = collector.collect();
    ASSERT_TRUE(metrics_opt.has_value());
    EXPECT_EQ(metrics_opt->available_bytes, 10240000ULL * 1024ULL);
    EXPECT_DOUBLE_EQ(metrics_opt->usage_percent, 37.5);
    EXPECT_DOUBLE_EQ(metrics_opt->swap_usage_percent, 50.0);
}

TEST(MemoryCollectorTest, CollectFromMalformedFixtureFile) {
    MemoryCollector collector(fixture_path("proc/meminfo_malformed"));
    auto metrics_opt = collector.collect();
    EXPECT_FALSE(metrics_opt.has_value());
}

TEST(MemoryCollectorTest, CollectFromOverflowFixtureFile) {
    MemoryCollector collector(fixture_path("proc/meminfo_overflow"));
    auto metrics_opt = collector.collect();
    EXPECT_FALSE(metrics_opt.has_value());
}

TEST(MemoryCollectorTest, CollectFromNonexistentFile) {
    MemoryCollector collector("/nonexistent/proc/meminfo");
    auto metrics_opt = collector.collect();
    EXPECT_FALSE(metrics_opt.has_value());
}

// ============================================================================
// MemoryService Tests
// ============================================================================

TEST(MemoryServiceTest, ServiceDelegatesToCollector) {
    auto collector = std::make_shared<MemoryCollector>(fixture_path("proc/meminfo_valid"));
    MemoryService service(collector);

    auto metrics_opt = service.get_memory_metrics();
    ASSERT_TRUE(metrics_opt.has_value());
    EXPECT_EQ(metrics_opt->total_bytes, 16384000ULL * 1024ULL);
    EXPECT_DOUBLE_EQ(metrics_opt->usage_percent, 37.5);
}

TEST(MemoryServiceTest, ServiceHandlesNullptrCollectorSafely) {
    MemoryService service(nullptr);
    // When nullptr passed, defaults to real collector (/proc/meminfo)
    auto metrics_opt = service.get_memory_metrics();
#ifdef __linux__
    EXPECT_TRUE(metrics_opt.has_value());
#endif
}

// ============================================================================
// Live Linux Host Sanity Test
// ============================================================================

#ifdef __linux__
TEST(MemoryCollectorTest, LiveLinuxHostMeminfoSanity) {
    MemoryCollector collector;
    auto metrics_opt = collector.collect();
    ASSERT_TRUE(metrics_opt.has_value());

    EXPECT_GT(metrics_opt->total_bytes, 0ULL);
    EXPECT_GT(metrics_opt->available_bytes, 0ULL);
    EXPECT_LE(metrics_opt->available_bytes, metrics_opt->total_bytes);
    EXPECT_LE(metrics_opt->free_bytes, metrics_opt->total_bytes);
    EXPECT_LE(metrics_opt->used_bytes, metrics_opt->total_bytes);
    EXPECT_GE(metrics_opt->usage_percent, 0.0);
    EXPECT_LE(metrics_opt->usage_percent, 100.0);
    EXPECT_GE(metrics_opt->swap_usage_percent, 0.0);
    EXPECT_LE(metrics_opt->swap_usage_percent, 100.0);
}
#endif
