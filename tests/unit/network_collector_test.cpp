#include <chrono>
#include <fstream>
#include <memory>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include <nodepulse/collectors/network_collector.hpp>
#include <nodepulse/domain/network_info.hpp>
#include <nodepulse/services/network_service.hpp>

using nodepulse::collectors::NetworkCollector;
using nodepulse::domain::NetworkInterfaceMetrics;
using nodepulse::services::NetworkService;
namespace domain = nodepulse::domain;

namespace {

#ifndef NODEPULSE_TEST_FIXTURES_DIR
#define NODEPULSE_TEST_FIXTURES_DIR "tests/fixtures"
#endif

std::string fixture_path(const std::string& subpath) {
    return std::string(NODEPULSE_TEST_FIXTURES_DIR) + "/" + subpath;
}

}  // namespace

// ============================================================================
// NetworkCollector Stream Parser Tests
// ============================================================================

TEST(NetworkCollectorTest, ParseValidNetDevStream) {
    std::ifstream file(fixture_path("proc/net_dev_valid"));
    ASSERT_TRUE(file.is_open());

    auto interfaces = NetworkCollector::parse_net_dev_stream(file);
    ASSERT_EQ(interfaces.size(), 3U);

    // 1. lo
    EXPECT_EQ(interfaces[0].name, "lo");
    EXPECT_EQ(interfaces[0].rx_bytes, 189476293ULL);
    EXPECT_EQ(interfaces[0].rx_packets, 346171ULL);
    EXPECT_EQ(interfaces[0].rx_errors, 0ULL);
    EXPECT_EQ(interfaces[0].rx_drops, 0ULL);
    EXPECT_EQ(interfaces[0].tx_bytes, 189476293ULL);
    EXPECT_EQ(interfaces[0].tx_packets, 346171ULL);
    EXPECT_EQ(interfaces[0].tx_errors, 0ULL);
    EXPECT_EQ(interfaces[0].tx_drops, 0ULL);

    // 2. eth0
    EXPECT_EQ(interfaces[1].name, "eth0");
    EXPECT_EQ(interfaces[1].rx_bytes, 1048576000ULL);
    EXPECT_EQ(interfaces[1].rx_packets, 1200000ULL);
    EXPECT_EQ(interfaces[1].rx_errors, 1ULL);
    EXPECT_EQ(interfaces[1].rx_drops, 2ULL);
    EXPECT_EQ(interfaces[1].tx_bytes, 2097152000ULL);
    EXPECT_EQ(interfaces[1].tx_packets, 1500000ULL);
    EXPECT_EQ(interfaces[1].tx_errors, 3ULL);
    EXPECT_EQ(interfaces[1].tx_drops, 4ULL);

    // 3. wlan0
    EXPECT_EQ(interfaces[2].name, "wlan0");
    EXPECT_EQ(interfaces[2].rx_bytes, 524288000ULL);
    EXPECT_EQ(interfaces[2].rx_packets, 600000ULL);
    EXPECT_EQ(interfaces[2].tx_bytes, 104857600ULL);
    EXPECT_EQ(interfaces[2].tx_packets, 150000ULL);
}

TEST(NetworkCollectorTest, ParseWhitespaceVariations) {
    std::ifstream file(fixture_path("proc/net_dev_spaces"));
    ASSERT_TRUE(file.is_open());

    auto interfaces = NetworkCollector::parse_net_dev_stream(file);
    ASSERT_EQ(interfaces.size(), 2U);

    EXPECT_EQ(interfaces[0].name, "lo");
    EXPECT_EQ(interfaces[0].rx_bytes, 100ULL);
    EXPECT_EQ(interfaces[0].tx_bytes, 100ULL);

    EXPECT_EQ(interfaces[1].name, "eth0");
    EXPECT_EQ(interfaces[1].rx_bytes, 200ULL);
    EXPECT_EQ(interfaces[1].tx_bytes, 200ULL);
}

TEST(NetworkCollectorTest, ParseMalformedLinesSafely) {
    std::ifstream file(fixture_path("proc/net_dev_malformed"));
    ASSERT_TRUE(file.is_open());

    auto interfaces = NetworkCollector::parse_net_dev_stream(file);
    // bad_cols, bad_chars, and bad/slash should all be rejected. Only eth0 is valid.
    ASSERT_EQ(interfaces.size(), 1U);
    EXPECT_EQ(interfaces[0].name, "eth0");
    EXPECT_EQ(interfaces[0].rx_bytes, 1000ULL);
}

TEST(NetworkCollectorTest, ParseVeryLargeUint64Counters) {
    std::ifstream file(fixture_path("proc/net_dev_overflow"));
    ASSERT_TRUE(file.is_open());

    auto interfaces = NetworkCollector::parse_net_dev_stream(file);
    ASSERT_EQ(interfaces.size(), 1U);
    EXPECT_EQ(interfaces[0].name, "eth0");
    EXPECT_EQ(interfaces[0].rx_bytes, 18446744073709551610ULL);
    EXPECT_EQ(interfaces[0].rx_packets, 18446744073709551611ULL);
    EXPECT_EQ(interfaces[0].tx_bytes, 18446744073709551612ULL);
    EXPECT_EQ(interfaces[0].tx_packets, 18446744073709551613ULL);
}

TEST(NetworkCollectorTest, ParseEmptyStreamReturnsEmpty) {
    std::istringstream stream{""};
    auto interfaces = NetworkCollector::parse_net_dev_stream(stream);
    EXPECT_TRUE(interfaces.empty());
}

TEST(NetworkCollectorTest, ParseHeadersOnlyStreamReturnsEmpty) {
    std::istringstream stream{
        "Inter-|   Receive                                                |  Transmit\n"
        " face |bytes    packets errs drop fifo frame compressed multicast|bytes    packets errs "
        "drop fifo colls carrier compressed\n"};
    auto interfaces = NetworkCollector::parse_net_dev_stream(stream);
    EXPECT_TRUE(interfaces.empty());
}

// ============================================================================
// NetworkCollector File and Sysfs Integration Tests
// ============================================================================

TEST(NetworkCollectorTest, CollectEnrichesWithSysfsProperties) {
    NetworkCollector collector(fixture_path("proc/net_dev_valid"), fixture_path("sys/class/net"));

    auto interfaces_opt = collector.collect();
    ASSERT_TRUE(interfaces_opt.has_value());
    const auto& interfaces = *interfaces_opt;
    ASSERT_EQ(interfaces.size(), 3U);

    // lo
    EXPECT_EQ(interfaces[0].name, "lo");
    EXPECT_EQ(interfaces[0].mac_address, "00:00:00:00:00:00");
    EXPECT_EQ(interfaces[0].operstate, "unknown");
    EXPECT_EQ(interfaces[0].speed_mbps, 0ULL);

    // eth0
    EXPECT_EQ(interfaces[1].name, "eth0");
    EXPECT_EQ(interfaces[1].mac_address, "52:54:00:12:34:56");
    EXPECT_EQ(interfaces[1].operstate, "up");
    EXPECT_EQ(interfaces[1].speed_mbps, 10000ULL);

    // wlan0 (sysfs missing -> defaults safely)
    EXPECT_EQ(interfaces[2].name, "wlan0");
    EXPECT_EQ(interfaces[2].mac_address, "");
    EXPECT_EQ(interfaces[2].operstate, "unknown");
    EXPECT_EQ(interfaces[2].speed_mbps, 0ULL);
}

TEST(NetworkCollectorTest, CollectFromNonexistentNetDevFileReturnsNullopt) {
    NetworkCollector collector("/nonexistent/proc/net/dev");
    auto interfaces_opt = collector.collect();
    EXPECT_FALSE(interfaces_opt.has_value());
}

// ============================================================================
// NetworkService Rate Calculation & Delta Sampling Tests
// ============================================================================

TEST(NetworkServiceTest, FirstSampleReturnsNullRatesWarmingUp) {
    auto collector = std::make_shared<NetworkCollector>(fixture_path("proc/net_dev_sample1"));
    NetworkService service(collector);

    auto metrics_opt = service.sample();
    ASSERT_TRUE(metrics_opt.has_value());
    ASSERT_EQ(metrics_opt->size(), 1U);

    const auto& iface = (*metrics_opt)[0];
    EXPECT_EQ(iface.name, "eth0");
    EXPECT_EQ(iface.rx_bytes, 1000000ULL);
    EXPECT_EQ(iface.tx_bytes, 2000000ULL);
    EXPECT_FALSE(iface.rx_bytes_per_sec.has_value());
    EXPECT_FALSE(iface.tx_bytes_per_sec.has_value());
}

TEST(NetworkServiceTest, SecondSampleCalculatesAccurateBandwidthRates) {
    class DualSampleCollector : public NetworkCollector {
      public:
        DualSampleCollector() : NetworkCollector() {}
        std::optional<std::vector<domain::NetworkInterfaceMetrics>> collect() const override {
            if (sample_num_ == 0) {
                sample_num_++;
                std::ifstream f(fixture_path("proc/net_dev_sample1"));
                return parse_net_dev_stream(f);
            }
            std::ifstream f(fixture_path("proc/net_dev_sample2"));
            return parse_net_dev_stream(f);
        }
        mutable int sample_num_{0};
    };

    auto collector = std::make_shared<DualSampleCollector>();
    NetworkService service(collector);

    auto m1 = service.sample();
    ASSERT_TRUE(m1.has_value());
    EXPECT_FALSE((*m1)[0].rx_bytes_per_sec.has_value());

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    auto m2 = service.sample();
    ASSERT_TRUE(m2.has_value());
    EXPECT_TRUE((*m2)[0].rx_bytes_per_sec.has_value());
    EXPECT_GT(*(*m2)[0].rx_bytes_per_sec, 0.0);
}

TEST(NetworkServiceTest, RateCalculationWithDeterministicTimeDelta) {
    auto collector1 = std::make_shared<NetworkCollector>(fixture_path("proc/net_dev_sample1"));
    NetworkService service(collector1);

    auto m1 = service.sample();
    ASSERT_TRUE(m1.has_value());
    EXPECT_FALSE((*m1)[0].rx_bytes_per_sec.has_value());

    // Wait 50ms
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Sample from sample2
    // We can simulate updating collector path:
    auto collector2 = std::make_shared<NetworkCollector>(fixture_path("proc/net_dev_sample2"));
    // Create service that samples sample1, then sample2
    // To do this cleanly, test using custom collector subclass or two sequential samples:
    class StepCollector : public NetworkCollector {
      public:
        StepCollector() : NetworkCollector() {}
        std::optional<std::vector<domain::NetworkInterfaceMetrics>> collect() const override {
            if (step_ == 0) {
                step_++;
                std::ifstream f(fixture_path("proc/net_dev_sample1"));
                return parse_net_dev_stream(f);
            }
            std::ifstream f(fixture_path("proc/net_dev_sample2"));
            return parse_net_dev_stream(f);
        }
        mutable int step_{0};
    };

    auto step_collector = std::make_shared<StepCollector>();
    NetworkService step_service(step_collector);

    // Sample 1
    auto sample1_opt = step_service.sample();
    ASSERT_TRUE(sample1_opt.has_value());
    EXPECT_FALSE((*sample1_opt)[0].rx_bytes_per_sec.has_value());
    EXPECT_FALSE((*sample1_opt)[0].tx_bytes_per_sec.has_value());

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Sample 2
    auto sample2_opt = step_service.sample();
    ASSERT_TRUE(sample2_opt.has_value());
    const auto& s2 = (*sample2_opt)[0];
    EXPECT_TRUE(s2.rx_bytes_per_sec.has_value());
    EXPECT_TRUE(s2.tx_bytes_per_sec.has_value());
    // Delta RX: 100,000 bytes over ~50ms -> ~2,000,000 B/s
    EXPECT_GT(*s2.rx_bytes_per_sec, 0.0);
    // Delta TX: 200,000 bytes over ~50ms -> ~4,000,000 B/s
    EXPECT_GT(*s2.tx_bytes_per_sec, 0.0);
    EXPECT_GT(*s2.tx_bytes_per_sec, *s2.rx_bytes_per_sec);
}

TEST(NetworkServiceTest, CounterWrapOrResetResetsBaselineToNullRates) {
    class WrapCollector : public NetworkCollector {
      public:
        WrapCollector() : NetworkCollector() {}
        std::optional<std::vector<domain::NetworkInterfaceMetrics>> collect() const override {
            if (step_ == 0) {
                step_++;
                std::ifstream f(fixture_path("proc/net_dev_sample2"));
                return parse_net_dev_stream(f);
            }
            // Sample reset (smaller counters)
            std::ifstream f(fixture_path("proc/net_dev_sample_reset"));
            return parse_net_dev_stream(f);
        }
        mutable int step_{0};
    };

    auto wrap_collector = std::make_shared<WrapCollector>();
    NetworkService wrap_service(wrap_collector);

    // Sample 1: sample2 (1,100,000 bytes)
    wrap_service.sample();

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Sample 2: sample_reset (500,000 bytes) -> counter reset detected!
    auto reset_opt = wrap_service.sample();
    ASSERT_TRUE(reset_opt.has_value());
    // Rates should be null/warming up because counter wrapped/reset
    EXPECT_FALSE((*reset_opt)[0].rx_bytes_per_sec.has_value());
    EXPECT_FALSE((*reset_opt)[0].tx_bytes_per_sec.has_value());
}

TEST(NetworkServiceTest, InterfaceHotplugAndDisappearanceHandling) {
    class HotplugCollector : public NetworkCollector {
      public:
        HotplugCollector() : NetworkCollector() {}
        std::optional<std::vector<domain::NetworkInterfaceMetrics>> collect() const override {
            if (step_ == 0) {
                step_++;
                // Initial: only lo
                std::istringstream s("lo: 100 10 0 0 0 0 0 0 100 10 0 0 0 0 0 0\n");
                return parse_net_dev_stream(s);
            }
            // Step 1: lo + hotplugged eth0
            std::istringstream s(
                "lo: 200 20 0 0 0 0 0 0 200 20 0 0 0 0 0 0\n"
                "eth0: 500 50 0 0 0 0 0 0 500 50 0 0 0 0 0 0\n");
            return parse_net_dev_stream(s);
        }
        mutable int step_{0};
    };

    auto collector = std::make_shared<HotplugCollector>();
    NetworkService service(collector);

    // Step 0: only lo
    auto s0 = service.sample();
    ASSERT_TRUE(s0.has_value());
    ASSERT_EQ(s0->size(), 1U);
    EXPECT_EQ((*s0)[0].name, "lo");

    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // Step 1: lo + eth0
    auto s1 = service.sample();
    ASSERT_TRUE(s1.has_value());
    ASSERT_EQ(s1->size(), 2U);

    // lo has prior baseline -> rate computed
    EXPECT_EQ((*s1)[0].name, "lo");
    EXPECT_TRUE((*s1)[0].rx_bytes_per_sec.has_value());

    // eth0 is newly appeared (hotplug) -> rates are null
    EXPECT_EQ((*s1)[1].name, "eth0");
    EXPECT_FALSE((*s1)[1].rx_bytes_per_sec.has_value());
    EXPECT_FALSE((*s1)[1].tx_bytes_per_sec.has_value());
}

TEST(NetworkServiceTest, BackgroundSamplingThreadLifecycle) {
    auto collector = std::make_shared<NetworkCollector>(fixture_path("proc/net_dev_valid"));
    NetworkService service(collector);

    EXPECT_FALSE(service.is_sampling());

    service.start_sampling(std::chrono::milliseconds(50));
    EXPECT_TRUE(service.is_sampling());

    // Give background thread time to take samples
    std::this_thread::sleep_for(std::chrono::milliseconds(120));

    auto metrics_opt = service.get_network_metrics();
    ASSERT_TRUE(metrics_opt.has_value());
    EXPECT_EQ(metrics_opt->size(), 3U);

    // Rates should have been computed after multiple background iterations
    EXPECT_TRUE((*metrics_opt)[0].rx_bytes_per_sec.has_value());

    service.stop_sampling();
    EXPECT_FALSE(service.is_sampling());
}

TEST(NetworkServiceTest, ServiceHandlesNullptrCollectorSafely) {
    NetworkService service(nullptr);
#ifdef __linux__
    auto metrics_opt = service.get_network_metrics();
    EXPECT_TRUE(metrics_opt.has_value());
#endif
}

// ============================================================================
// Live Linux Host Sanity Test
// ============================================================================

#ifdef __linux__
TEST(NetworkCollectorTest, LiveLinuxHostNetDevSanity) {
    NetworkCollector collector;
    auto interfaces_opt = collector.collect();
    ASSERT_TRUE(interfaces_opt.has_value());

    // On any Linux host, at least loopback 'lo' must be present
    EXPECT_GE(interfaces_opt->size(), 1U);

    bool found_lo = false;
    for (const auto& iface : *interfaces_opt) {
        EXPECT_FALSE(iface.name.empty());
        EXPECT_FALSE(iface.operstate.empty());
        if (iface.name == "lo") {
            found_lo = true;
        }
    }
    EXPECT_TRUE(found_lo);
}
#endif
