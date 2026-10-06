#include <fstream>
#include <memory>
#include <sstream>
#include <string>

#include <gtest/gtest.h>

#include <nodepulse/collectors/cpu_collector.hpp>
#include <nodepulse/domain/cpu_info.hpp>
#include <nodepulse/services/cpu_service.hpp>

using nodepulse::collectors::CpuCollector;
using nodepulse::collectors::CpuSnapshot;
using nodepulse::collectors::CpuStaticInfo;
using nodepulse::domain::CpuMetrics;
using nodepulse::domain::LoadAverage;
using nodepulse::services::CpuService;

namespace {

#ifndef NODEPULSE_TEST_FIXTURES_DIR
#define NODEPULSE_TEST_FIXTURES_DIR "tests/fixtures"
#endif

std::string fixture_path(const std::string& subpath) {
    return std::string(NODEPULSE_TEST_FIXTURES_DIR) + "/" + subpath;
}

}  // namespace

// ============================================================================
// CpuCollector::parse_stat_stream Tests
// ============================================================================

TEST(CpuCollectorTest, ParseStatStreamValid) {
    std::string content =
        "cpu  1000 20 500 8500 100 10 5 2 0 0\n"
        "cpu0 500 10 250 4250 50 5 2 1 0 0\n"
        "cpu1 500 10 250 4250 50 5 3 1 0 0\n"
        "intr 12345\n";
    std::istringstream stream(content);

    auto snapshot_opt = CpuCollector::parse_stat_stream(stream);
    ASSERT_TRUE(snapshot_opt.has_value());

    const auto& snap = *snapshot_opt;
    EXPECT_EQ(snap.aggregate.user, 1000ULL);
    EXPECT_EQ(snap.aggregate.nice, 20ULL);
    EXPECT_EQ(snap.aggregate.system, 500ULL);
    EXPECT_EQ(snap.aggregate.idle, 8500ULL);
    EXPECT_EQ(snap.aggregate.iowait, 100ULL);
    EXPECT_EQ(snap.aggregate.irq, 10ULL);
    EXPECT_EQ(snap.aggregate.softirq, 5ULL);
    EXPECT_EQ(snap.aggregate.steal, 2ULL);
    EXPECT_EQ(snap.aggregate.guest, 0ULL);
    EXPECT_EQ(snap.aggregate.guest_nice, 0ULL);

    // Total should NOT double count guest/guest_nice
    EXPECT_EQ(snap.aggregate.total(), 1000 + 20 + 500 + 8500 + 100 + 10 + 5 + 2);
    EXPECT_EQ(snap.aggregate.idle_all(), 8500 + 100);

    ASSERT_EQ(snap.cores.size(), 2U);
    EXPECT_EQ(snap.cores[0].core_id, 0U);
    EXPECT_EQ(snap.cores[0].time.user, 500ULL);
    EXPECT_EQ(snap.cores[1].core_id, 1U);
    EXPECT_EQ(snap.cores[1].time.user, 500ULL);
}

TEST(CpuCollectorTest, ParseStatStreamMissingAggregateReturnsNullopt) {
    std::string content =
        "intr 12345\n"
        "ctxt 54321\n"
        "btime 1700000000\n";
    std::istringstream stream(content);

    auto snapshot_opt = CpuCollector::parse_stat_stream(stream);
    EXPECT_FALSE(snapshot_opt.has_value());
}

TEST(CpuCollectorTest, ParseStatStreamEmptyReturnsNullopt) {
    std::string content;
    std::istringstream stream(content);

    auto snapshot_opt = CpuCollector::parse_stat_stream(stream);
    EXPECT_FALSE(snapshot_opt.has_value());
}

TEST(CpuCollectorTest, ParseStatStreamShortLineHandlesPartialCounters) {
    // Older Linux kernels output fewer fields
    std::string content =
        "cpu  1000 0 500 8500\n"
        "cpu0 1000 0 500 8500\n";
    std::istringstream stream(content);

    auto snapshot_opt = CpuCollector::parse_stat_stream(stream);
    ASSERT_TRUE(snapshot_opt.has_value());
    EXPECT_EQ(snapshot_opt->aggregate.user, 1000ULL);
    EXPECT_EQ(snapshot_opt->aggregate.idle, 8500ULL);
    EXPECT_EQ(snapshot_opt->aggregate.iowait, 0ULL);
    EXPECT_EQ(snapshot_opt->aggregate.total(), 10000ULL);
}

// ============================================================================
// CpuCollector::parse_loadavg_stream Tests
// ============================================================================

TEST(CpuCollectorTest, ParseLoadavgStreamValid) {
    std::string content = "0.45 0.62 0.58 2/850 12345\n";
    std::istringstream stream(content);

    auto load_opt = CpuCollector::parse_loadavg_stream(stream);
    ASSERT_TRUE(load_opt.has_value());
    EXPECT_DOUBLE_EQ(load_opt->one_minute, 0.45);
    EXPECT_DOUBLE_EQ(load_opt->five_minute, 0.62);
    EXPECT_DOUBLE_EQ(load_opt->fifteen_minute, 0.58);
}

TEST(CpuCollectorTest, ParseLoadavgStreamEmptyReturnsNullopt) {
    std::string content;
    std::istringstream stream(content);

    auto load_opt = CpuCollector::parse_loadavg_stream(stream);
    EXPECT_FALSE(load_opt.has_value());
}

TEST(CpuCollectorTest, ParseLoadavgStreamNonNumericReturnsNullopt) {
    std::string content = "not_a_number 0.5 0.2\n";
    std::istringstream stream(content);

    auto load_opt = CpuCollector::parse_loadavg_stream(stream);
    EXPECT_FALSE(load_opt.has_value());
}

TEST(CpuCollectorTest, ParseLoadavgStreamNegativeValueReturnsNullopt) {
    std::string content = "-0.1 0.5 0.2\n";
    std::istringstream stream(content);

    auto load_opt = CpuCollector::parse_loadavg_stream(stream);
    EXPECT_FALSE(load_opt.has_value());
}

// ============================================================================
// CpuCollector::parse_cpuinfo_stream Tests
// ============================================================================

TEST(CpuCollectorTest, ParseCpuinfoStreamStandardMultiCore) {
    std::string content =
        "processor\t: 0\n"
        "model name\t: Intel Core i7-12700K\n"
        "physical id\t: 0\n"
        "core id\t: 0\n"
        "cpu cores\t: 2\n"
        "\n"
        "processor\t: 1\n"
        "model name\t: Intel Core i7-12700K\n"
        "physical id\t: 0\n"
        "core id\t: 1\n"
        "cpu cores\t: 2\n";
    std::istringstream stream(content);

    auto info = CpuCollector::parse_cpuinfo_stream(stream);
    EXPECT_EQ(info.model_name, "Intel Core i7-12700K");
    EXPECT_EQ(info.logical_cores, 2U);
    EXPECT_EQ(info.physical_cores, 2U);
}

TEST(CpuCollectorTest, ParseCpuinfoStreamFallbackWhenMissing) {
    std::string content;
    std::istringstream stream(content);

    auto info = CpuCollector::parse_cpuinfo_stream(stream);
    EXPECT_EQ(info.model_name, "Unknown CPU");
    EXPECT_GT(info.logical_cores, 0U);
    EXPECT_GT(info.physical_cores, 0U);
}

// ============================================================================
// CpuService Delta Calculation & Sampling Tests
// ============================================================================

TEST(CpuServiceTest, FirstSampleReturnsNullUsageAndWarmingUpPerDec014) {
    auto collector =
        std::make_shared<CpuCollector>(fixture_path("proc/stat_sample1"),
                                       fixture_path("proc/loadavg"), fixture_path("proc/cpuinfo"));
    CpuService service(collector);

    auto metrics_opt = service.get_cpu_metrics();
    ASSERT_TRUE(metrics_opt.has_value());

    const auto& metrics = *metrics_opt;
    EXPECT_FALSE(metrics.usage_percent.has_value());
    EXPECT_EQ(metrics.measurement_status, "warming_up");
    EXPECT_EQ(metrics.model_name, "12th Gen Intel(R) Core(TM) i5-12450H");
    EXPECT_EQ(metrics.logical_cores, 2U);
    EXPECT_EQ(metrics.physical_cores, 2U);
    EXPECT_DOUBLE_EQ(metrics.load_average.one_minute, 0.45);

    ASSERT_EQ(metrics.cores.size(), 2U);
    EXPECT_FALSE(metrics.cores[0].usage_percent.has_value());
    EXPECT_FALSE(metrics.cores[1].usage_percent.has_value());
}

// Subclass helper to dynamically supply samples
class DynamicCpuCollector : public CpuCollector {
  public:
    std::string current_stat_content;
    std::string current_loadavg_content{"0.45 0.62 0.58 2/850 12345"};
    std::string current_cpuinfo_content{
        "processor: 0\nmodel name: Mock CPU\nphysical id: 0\ncore id: 0\n"};

    [[nodiscard]] std::optional<CpuSnapshot> read_stat() const override {
        std::istringstream stream(current_stat_content);
        return parse_stat_stream(stream);
    }
    [[nodiscard]] std::optional<nodepulse::domain::LoadAverage> read_loadavg() const override {
        std::istringstream stream(current_loadavg_content);
        return parse_loadavg_stream(stream);
    }
    [[nodiscard]] CpuStaticInfo read_cpuinfo() const override {
        std::istringstream stream(current_cpuinfo_content);
        return parse_cpuinfo_stream(stream);
    }
};

TEST(CpuServiceTest, FiftyPercentUsageScenario) {
    auto collector = std::make_shared<DynamicCpuCollector>();
    // Baseline: total = 10000, idle = 8500
    collector->current_stat_content =
        "cpu  1000 0 500 8500 0 0 0 0 0 0\n"
        "cpu0 500 0 250 4250 0 0 0 0 0 0\n"
        "cpu1 500 0 250 4250 0 0 0 0 0 0\n";
    CpuService service(collector);

    auto m1 = service.get_cpu_metrics();
    ASSERT_TRUE(m1.has_value());
    EXPECT_FALSE(m1->usage_percent.has_value());
    EXPECT_EQ(m1->measurement_status, "warming_up");

    // Delta: +1000 total, +500 busy, +500 idle -> 50.0%
    // core0: +500 total, +300 busy, +200 idle -> 60.0%
    // core1: +500 total, +200 busy, +300 idle -> 40.0%
    collector->current_stat_content =
        "cpu  1300 0 700 9000 0 0 0 0 0 0\n"
        "cpu0 700 0 350 4450 0 0 0 0 0 0\n"
        "cpu1 600 0 350 4550 0 0 0 0 0 0\n";

    auto m2 = service.get_cpu_metrics();
    ASSERT_TRUE(m2.has_value());
    ASSERT_TRUE(m2->usage_percent.has_value());
    EXPECT_DOUBLE_EQ(*m2->usage_percent, 50.0);
    EXPECT_EQ(m2->measurement_status, "ready");
    ASSERT_EQ(m2->cores.size(), 2U);
    ASSERT_TRUE(m2->cores[0].usage_percent.has_value());
    EXPECT_DOUBLE_EQ(*m2->cores[0].usage_percent, 60.0);
    ASSERT_TRUE(m2->cores[1].usage_percent.has_value());
    EXPECT_DOUBLE_EQ(*m2->cores[1].usage_percent, 40.0);
}

TEST(CpuServiceTest, HundredPercentUsageScenario) {
    auto collector = std::make_shared<DynamicCpuCollector>();
    collector->current_stat_content = "cpu  1000 0 500 8500 0 0 0 0 0 0\n";
    CpuService service(collector);
    (void)service.get_cpu_metrics();

    // All delta goes into busy (+1000 total, +0 idle) -> 100.0%
    collector->current_stat_content = "cpu  1500 0 1000 8500 0 0 0 0 0 0\n";
    auto m2 = service.get_cpu_metrics();
    ASSERT_TRUE(m2.has_value());
    ASSERT_TRUE(m2->usage_percent.has_value());
    EXPECT_DOUBLE_EQ(*m2->usage_percent, 100.0);
    EXPECT_EQ(m2->measurement_status, "ready");
}

TEST(CpuServiceTest, ZeroPercentUsageScenario) {
    auto collector = std::make_shared<DynamicCpuCollector>();
    collector->current_stat_content = "cpu  1000 0 500 8500 0 0 0 0 0 0\n";
    CpuService service(collector);
    (void)service.get_cpu_metrics();

    // All delta goes into idle (+1000 total, +1000 idle) -> 0.0%
    collector->current_stat_content = "cpu  1000 0 500 9500 0 0 0 0 0 0\n";
    auto m2 = service.get_cpu_metrics();
    ASSERT_TRUE(m2.has_value());
    ASSERT_TRUE(m2->usage_percent.has_value());
    EXPECT_DOUBLE_EQ(*m2->usage_percent, 0.0);
    EXPECT_EQ(m2->measurement_status, "ready");
}

TEST(CpuServiceTest, ZeroElapsedTotalJiffiesPreservesLastCalculatedUsageAsCached) {
    auto collector = std::make_shared<DynamicCpuCollector>();
    collector->current_stat_content = "cpu  1000 0 500 8500 0 0 0 0 0 0\n";
    CpuService service(collector);
    (void)service.get_cpu_metrics();

    collector->current_stat_content = "cpu  1300 0 700 9000 0 0 0 0 0 0\n";
    auto m2 = service.get_cpu_metrics();
    ASSERT_TRUE(m2.has_value());
    ASSERT_TRUE(m2->usage_percent.has_value());
    EXPECT_DOUBLE_EQ(*m2->usage_percent, 50.0);
    EXPECT_EQ(m2->measurement_status, "ready");

    // Third call with identical stat (zero elapsed total time)
    auto m3 = service.get_cpu_metrics();
    ASSERT_TRUE(m3.has_value());
    ASSERT_TRUE(m3->usage_percent.has_value());
    EXPECT_DOUBLE_EQ(*m3->usage_percent, 50.0);
    EXPECT_EQ(m3->measurement_status, "cached");
}

TEST(CpuServiceTest, CounterResetResetsBaselineAndReportsWarmingUp) {
    auto collector = std::make_shared<DynamicCpuCollector>();
    collector->current_stat_content = "cpu  10000 0 5000 85000 0 0 0 0 0 0\n";
    CpuService service(collector);
    (void)service.get_cpu_metrics();

    // Reset: counters become smaller than prior sample (reboot or counter wrap)
    collector->current_stat_content = "cpu  100 0 50 850 0 0 0 0 0 0\n";
    auto m2 = service.get_cpu_metrics();
    ASSERT_TRUE(m2.has_value());
    EXPECT_FALSE(m2->usage_percent.has_value());
    EXPECT_EQ(m2->measurement_status, "warming_up");

    // Next sample from the new baseline computes delta accurately
    collector->current_stat_content =
        "cpu  150 0 100 850 0 0 0 0 0 0\n";  // +100 total, +100 busy -> 100%
    auto m3 = service.get_cpu_metrics();
    ASSERT_TRUE(m3.has_value());
    ASSERT_TRUE(m3->usage_percent.has_value());
    EXPECT_DOUBLE_EQ(*m3->usage_percent, 100.0);
    EXPECT_EQ(m3->measurement_status, "ready");
}

TEST(CpuServiceTest, CoreCountChangeResetsBaselineAndReportsWarmingUp) {
    auto collector = std::make_shared<DynamicCpuCollector>();
    // Baseline with 2 cores
    collector->current_stat_content =
        "cpu  1000 0 500 8500 0 0 0 0 0 0\n"
        "cpu0 500 0 250 4250 0 0 0 0 0 0\n"
        "cpu1 500 0 250 4250 0 0 0 0 0 0\n";
    CpuService service(collector);
    (void)service.get_cpu_metrics();

    // CPU hotplug: 3rd core added
    collector->current_stat_content =
        "cpu  1500 0 750 9750 0 0 0 0 0 0\n"
        "cpu0 500 0 250 4250 0 0 0 0 0 0\n"
        "cpu1 500 0 250 4250 0 0 0 0 0 0\n"
        "cpu2 500 0 250 1250 0 0 0 0 0 0\n";
    auto m2 = service.get_cpu_metrics();
    ASSERT_TRUE(m2.has_value());
    EXPECT_FALSE(m2->usage_percent.has_value());
    EXPECT_EQ(m2->measurement_status, "warming_up");
    EXPECT_EQ(m2->logical_cores, 3U);
    EXPECT_EQ(m2->cores.size(), 3U);
}

TEST(CpuServiceTest, StealTimeAndGuestAccountingSemantics) {
    auto collector = std::make_shared<DynamicCpuCollector>();
    // user: 1000 (includes guest: 200)
    // steal: 500
    // total = 1000 + 0 + 500 + 8000 + 0 + 0 + 0 + 500 = 10000
    collector->current_stat_content = "cpu  1000 0 500 8000 0 0 0 500 200 0\n";
    CpuService service(collector);
    (void)service.get_cpu_metrics();

    // Delta: +1000 total (+500 steal, +500 idle)
    // Steal is busy time (hypervisor stole CPU capacity from runnable vCPU)
    // busy = delta_total - delta_idle = 1000 - 500 = 500 -> 50%
    collector->current_stat_content = "cpu  1000 0 500 8500 0 0 0 1000 200 0\n";
    auto m2 = service.get_cpu_metrics();
    ASSERT_TRUE(m2.has_value());
    ASSERT_TRUE(m2->usage_percent.has_value());
    EXPECT_DOUBLE_EQ(*m2->usage_percent, 50.0);
    EXPECT_EQ(m2->measurement_status, "ready");
}

TEST(CpuServiceTest, BackgroundSamplingThreadEstablishesUsableInterval) {
    auto collector = std::make_shared<DynamicCpuCollector>();
    collector->current_stat_content = "cpu  1000 0 500 8500 0 0 0 0 0 0\n";
    CpuService service(collector);

    // Start background sampling at 30ms interval
    service.start_sampling(std::chrono::milliseconds(30));
    EXPECT_TRUE(service.is_sampling());

    // Immediate read while sampling is active: should be warming_up
    auto m1 = service.get_cpu_metrics();
    ASSERT_TRUE(m1.has_value());
    EXPECT_FALSE(m1->usage_percent.has_value());
    EXPECT_EQ(m1->measurement_status, "warming_up");

    // Advance stat content
    collector->current_stat_content = "cpu  1300 0 700 9000 0 0 0 0 0 0\n";

    // Wait 70ms for background thread to take at least one sample
    std::this_thread::sleep_for(std::chrono::milliseconds(70));

    auto m2 = service.get_cpu_metrics();
    ASSERT_TRUE(m2.has_value());
    ASSERT_TRUE(m2->usage_percent.has_value());
    EXPECT_DOUBLE_EQ(*m2->usage_percent, 50.0);
    EXPECT_TRUE(m2->measurement_status == "ready" || m2->measurement_status == "cached");

    service.stop_sampling();
    EXPECT_FALSE(service.is_sampling());
}

TEST(CpuServiceTest, HandlesCollectorStatFailureGracefully) {
    auto collector = std::make_shared<DynamicCpuCollector>();
    collector->current_stat_content = "";  // empty -> parse_stat_stream returns nullopt
    CpuService service(collector);

    auto m = service.get_cpu_metrics();
    EXPECT_FALSE(m.has_value());
}

TEST(CpuServiceTest, HandlesCollectorLoadavgFailureGracefully) {
    auto collector = std::make_shared<DynamicCpuCollector>();
    collector->current_stat_content = "cpu  1000 0 500 8500 0 0 0 0 0 0\n";
    collector->current_loadavg_content = "invalid loadavg";
    CpuService service(collector);

    auto m = service.get_cpu_metrics();
    EXPECT_FALSE(m.has_value());
}
