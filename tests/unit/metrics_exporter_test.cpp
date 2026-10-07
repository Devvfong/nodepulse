#include <chrono>
#include <future>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>

#include <nodepulse/collectors/cpu_collector.hpp>
#include <nodepulse/collectors/disk_collector.hpp>
#include <nodepulse/collectors/memory_collector.hpp>
#include <nodepulse/collectors/network_collector.hpp>
#include <nodepulse/config/config.hpp>
#include <nodepulse/services/cpu_service.hpp>
#include <nodepulse/services/disk_service.hpp>
#include <nodepulse/services/memory_service.hpp>
#include <nodepulse/services/metrics_exporter.hpp>
#include <nodepulse/services/network_service.hpp>
#include <nodepulse/services/stream_service.hpp>

namespace {

const std::string kFixturesDir = NODEPULSE_TEST_FIXTURES_DIR;

class MockSseClient : public nodepulse::services::ISseClient {
  public:
    bool send_data(const std::string&) override {
        return true;
    }
    void close() override {
        alive_ = false;
    }
    [[nodiscard]] bool is_alive() const override {
        return alive_;
    }

  private:
    std::atomic<bool> alive_{true};
};

}  // namespace

TEST(MetricsExporterTest, ExporterRespectsEnabledDisabledFlag) {
    nodepulse::config::PrometheusConfig cfg;
    cfg.enabled = true;
    nodepulse::services::MetricsExporter exporter(nullptr, nullptr, nullptr, nullptr, nullptr, cfg);

    EXPECT_TRUE(exporter.is_enabled());
    exporter.set_enabled(false);
    EXPECT_FALSE(exporter.is_enabled());
}

TEST(MetricsExporterTest, NullCollectorsProduceValidBasicExpositionWithoutCrash) {
    nodepulse::services::MetricsExporter exporter;
    std::string text = exporter.export_metrics();

    EXPECT_FALSE(text.empty());
    EXPECT_EQ(text.back(), '\n');

    // Build info must be present
    EXPECT_NE(text.find("# HELP nodepulse_build_info"), std::string::npos);
    EXPECT_NE(text.find("# TYPE nodepulse_build_info gauge"), std::string::npos);
    EXPECT_NE(text.find("nodepulse_build_info{"), std::string::npos);

    // Uptime must be present
    EXPECT_NE(text.find("# HELP nodepulse_process_uptime_seconds"), std::string::npos);
    EXPECT_NE(text.find("# TYPE nodepulse_process_uptime_seconds counter"), std::string::npos);
    EXPECT_NE(text.find("nodepulse_process_uptime_seconds "), std::string::npos);

    // Collector failures must be present
    EXPECT_NE(text.find("# HELP nodepulse_collector_failures_total"), std::string::npos);
    EXPECT_NE(text.find("# TYPE nodepulse_collector_failures_total counter"), std::string::npos);

    // Host metrics should NOT be present when services are null
    EXPECT_EQ(text.find("nodepulse_cpu_usage_ratio"), std::string::npos);
    EXPECT_EQ(text.find("nodepulse_memory_used_bytes"), std::string::npos);
    EXPECT_EQ(text.find("nodepulse_disk_used_bytes"), std::string::npos);
    EXPECT_EQ(text.find("nodepulse_network_receive_bytes_total"), std::string::npos);
}

TEST(MetricsExporterTest, CpuUsageRatioOmittedDuringWarmup) {
    auto cpu_collector = std::make_shared<nodepulse::collectors::CpuCollector>(
        kFixturesDir + "/proc/stat", kFixturesDir + "/proc/loadavg",
        kFixturesDir + "/proc/cpuinfo");
    auto cpu_service = std::make_shared<nodepulse::services::CpuService>(cpu_collector);

    // Fresh CpuService without sampling has usage_percent = nullopt
    auto metrics = cpu_service->get_cpu_metrics();
    ASSERT_TRUE(metrics.has_value());
    EXPECT_FALSE(metrics->usage_percent.has_value());

    nodepulse::services::MetricsExporter exporter(cpu_service);
    std::string text = exporter.export_metrics();

    // Must omit nodepulse_cpu_usage_ratio completely during warm-up (no NaN, no null, no fake 0.0)
    EXPECT_EQ(text.find("nodepulse_cpu_usage_ratio"), std::string::npos);
    EXPECT_EQ(text.find("NaN"), std::string::npos);
    EXPECT_EQ(text.find("null"), std::string::npos);
}

TEST(MetricsExporterTest, MemoryMetricsExportedWithExactValues) {
    auto mem_collector = std::make_shared<nodepulse::collectors::MemoryCollector>(
        kFixturesDir + "/proc/meminfo_valid");
    auto mem_service = std::make_shared<nodepulse::services::MemoryService>(mem_collector);

    nodepulse::services::MetricsExporter exporter(nullptr, mem_service);
    std::string text = exporter.export_metrics();

    EXPECT_NE(
        text.find("# HELP nodepulse_memory_used_bytes Host RAM memory currently in use in bytes\n"),
        std::string::npos);
    EXPECT_NE(text.find("# TYPE nodepulse_memory_used_bytes gauge\n"), std::string::npos);
    EXPECT_NE(text.find("# HELP nodepulse_memory_total_bytes Total physical host RAM in bytes\n"),
              std::string::npos);
    EXPECT_NE(text.find("# TYPE nodepulse_memory_total_bytes gauge\n"), std::string::npos);

    auto mem_metrics = mem_service->get_memory_metrics();
    ASSERT_TRUE(mem_metrics.has_value());
    std::string expected_used =
        "nodepulse_memory_used_bytes " + std::to_string(mem_metrics->used_bytes);
    std::string expected_total =
        "nodepulse_memory_total_bytes " + std::to_string(mem_metrics->total_bytes);
    EXPECT_NE(text.find(expected_used), std::string::npos);
    EXPECT_NE(text.find(expected_total), std::string::npos);
}

TEST(MetricsExporterTest, DiskMetricsExportedWithLabelsAndEscaping) {
    auto disk_collector =
        std::make_shared<nodepulse::collectors::DiskCollector>(kFixturesDir + "/proc/mounts_valid");
    auto disk_service = std::make_shared<nodepulse::services::DiskService>(disk_collector);

    nodepulse::services::MetricsExporter exporter(nullptr, nullptr, disk_service);
    std::string text = exporter.export_metrics();

    EXPECT_NE(text.find("# HELP nodepulse_disk_used_bytes Disk space used in bytes\n"),
              std::string::npos);
    EXPECT_NE(text.find("# TYPE nodepulse_disk_used_bytes gauge\n"), std::string::npos);
    EXPECT_NE(text.find("# HELP nodepulse_disk_total_bytes Total disk space in bytes\n"),
              std::string::npos);
    EXPECT_NE(text.find("# TYPE nodepulse_disk_total_bytes gauge\n"), std::string::npos);

    // Root filesystem should be present
    EXPECT_NE(text.find("nodepulse_disk_used_bytes{filesystem="), std::string::npos);
    EXPECT_NE(text.find("mount_point=\"/\""), std::string::npos);
}

TEST(MetricsExporterTest, LabelEscapingHandlesSpecialCharacters) {
    EXPECT_EQ(nodepulse::services::MetricsExporter::escape_label_value("normal"), "normal");
    EXPECT_EQ(nodepulse::services::MetricsExporter::escape_label_value("path\\with\\backslash"),
              "path\\\\with\\\\backslash");
    EXPECT_EQ(nodepulse::services::MetricsExporter::escape_label_value("quote\"in\"val"),
              "quote\\\"in\\\"val");
    EXPECT_EQ(nodepulse::services::MetricsExporter::escape_label_value("line1\nline2"),
              "line1\\nline2");
}

TEST(MetricsExporterTest, NetworkMetricsExportedWithInterfaceLabels) {
    auto net_collector = std::make_shared<nodepulse::collectors::NetworkCollector>(
        kFixturesDir + "/proc/net_dev_valid", kFixturesDir + "/sys/class/net");
    auto net_service = std::make_shared<nodepulse::services::NetworkService>(net_collector);

    nodepulse::services::MetricsExporter exporter(nullptr, nullptr, nullptr, net_service);
    std::string text = exporter.export_metrics();

    EXPECT_NE(
        text.find("# HELP nodepulse_network_receive_bytes_total Total network bytes received\n"),
        std::string::npos);
    EXPECT_NE(text.find("# TYPE nodepulse_network_receive_bytes_total counter\n"),
              std::string::npos);
    EXPECT_NE(
        text.find(
            "# HELP nodepulse_network_transmit_bytes_total Total network bytes transmitted\n"),
        std::string::npos);
    EXPECT_NE(text.find("# TYPE nodepulse_network_transmit_bytes_total counter\n"),
              std::string::npos);

    EXPECT_NE(text.find("nodepulse_network_receive_bytes_total{interface=\"eth0\"}"),
              std::string::npos);
    EXPECT_NE(text.find("nodepulse_network_transmit_bytes_total{interface=\"eth0\"}"),
              std::string::npos);
    EXPECT_NE(text.find("nodepulse_network_receive_bytes_total{interface=\"lo\"}"),
              std::string::npos);
}

TEST(MetricsExporterTest, ProcessSelfStatsProviderIntegration) {
    nodepulse::services::MetricsExporter exporter;

    nodepulse::services::ProcessSelfStats mock_stats;
    mock_stats.rss_bytes = 41943040;  // 40 MB
    mock_stats.cpu_user_seconds = 2.75;
    mock_stats.cpu_system_seconds = 0.85;

    exporter.set_self_stat_provider([mock_stats]() { return mock_stats; });

    std::string text = exporter.export_metrics();

    EXPECT_NE(text.find("# HELP nodepulse_process_resident_memory_bytes Resident set size (RSS) of "
                        "NodePulse process in bytes\n"),
              std::string::npos);
    EXPECT_NE(text.find("# TYPE nodepulse_process_resident_memory_bytes gauge\n"),
              std::string::npos);
    EXPECT_NE(text.find("nodepulse_process_resident_memory_bytes 41943040\n"), std::string::npos);

    EXPECT_NE(text.find("# HELP nodepulse_process_cpu_seconds_total Total CPU seconds consumed by "
                        "NodePulse process\n"),
              std::string::npos);
    EXPECT_NE(text.find("# TYPE nodepulse_process_cpu_seconds_total counter\n"), std::string::npos);
    EXPECT_NE(text.find("nodepulse_process_cpu_seconds_total{mode=\"system\"} 0.85\n"),
              std::string::npos);
    EXPECT_NE(text.find("nodepulse_process_cpu_seconds_total{mode=\"user\"} 2.75\n"),
              std::string::npos);
}

TEST(MetricsExporterTest, SseActiveConnectionsCountExported) {
    nodepulse::config::SseConfig sse_cfg;
    sse_cfg.enabled = true;
    sse_cfg.max_clients = 10;
    auto stream_svc =
        std::make_shared<nodepulse::services::StreamService>(nullptr, nullptr, nullptr, sse_cfg);

    auto client1 = std::make_shared<MockSseClient>();
    auto client2 = std::make_shared<MockSseClient>();
    EXPECT_TRUE(stream_svc->try_add_client(client1));
    EXPECT_TRUE(stream_svc->try_add_client(client2));

    nodepulse::services::MetricsExporter exporter(nullptr, nullptr, nullptr, nullptr, stream_svc);
    std::string text = exporter.export_metrics();

    EXPECT_NE(text.find("# HELP nodepulse_sse_active_connections Current number of open SSE "
                        "streaming clients\n"),
              std::string::npos);
    EXPECT_NE(text.find("# TYPE nodepulse_sse_active_connections gauge\n"), std::string::npos);
    EXPECT_NE(text.find("nodepulse_sse_active_connections 2\n"), std::string::npos);
}

TEST(MetricsExporterTest, HttpRequestEndpointNormalizationAndCounters) {
    nodepulse::services::MetricsExporter exporter;

    exporter.record_http_request("/api/v1/cpu", "GET", 200);
    exporter.record_http_request("/api/v1/cpu", "GET", 200);
    exporter.record_http_request("/api/v1/processes/1234", "GET", 200);
    exporter.record_http_request("/api/v1/processes/5678", "GET", 200);
    exporter.record_http_request("/api/v1/services/nginx.service", "GET", 200);
    exporter.record_http_request("/api/v1/containers/abc123def456", "GET", 404);
    exporter.record_http_request("/nonexistent/endpoint", "GET", 404);

    std::string text = exporter.export_metrics();

    EXPECT_NE(
        text.find("# HELP nodepulse_http_requests_total Total number of HTTP requests processed\n"),
        std::string::npos);
    EXPECT_NE(text.find("# TYPE nodepulse_http_requests_total counter\n"), std::string::npos);

    // /api/v1/cpu count 2
    EXPECT_NE(text.find("nodepulse_http_requests_total{endpoint=\"/api/v1/"
                        "cpu\",method=\"GET\",status=\"200\"} 2\n"),
              std::string::npos);

    // Dynamic PID normalized to {pid} with count 2
    EXPECT_NE(text.find("nodepulse_http_requests_total{endpoint=\"/api/v1/processes/"
                        "{pid}\",method=\"GET\",status=\"200\"} 2\n"),
              std::string::npos);
    EXPECT_EQ(text.find("1234"), std::string::npos);
    EXPECT_EQ(text.find("5678"), std::string::npos);

    // Dynamic service normalized to {name} with count 1
    EXPECT_NE(text.find("nodepulse_http_requests_total{endpoint=\"/api/v1/services/"
                        "{name}\",method=\"GET\",status=\"200\"} 1\n"),
              std::string::npos);

    // Dynamic container normalized to {id} with count 1
    EXPECT_NE(text.find("nodepulse_http_requests_total{endpoint=\"/api/v1/containers/"
                        "{id}\",method=\"GET\",status=\"404\"} 1\n"),
              std::string::npos);

    // Unknown endpoint normalized to unmatched
    EXPECT_NE(text.find("nodepulse_http_requests_total{endpoint=\"unmatched\",method=\"GET\","
                        "status=\"404\"} 1\n"),
              std::string::npos);
}

TEST(MetricsExporterTest, CollectorFailuresRecordedAccurately) {
    nodepulse::services::MetricsExporter exporter;

    exporter.record_collector_failure("disk");
    exporter.record_collector_failure("disk");
    exporter.record_collector_failure("network");

    std::string text = exporter.export_metrics();

    EXPECT_NE(
        text.find(
            "# HELP nodepulse_collector_failures_total Count of collector execution errors\n"),
        std::string::npos);
    EXPECT_NE(text.find("# TYPE nodepulse_collector_failures_total counter\n"), std::string::npos);

    EXPECT_NE(text.find("nodepulse_collector_failures_total{collector=\"disk\"} 2\n"),
              std::string::npos);
    EXPECT_NE(text.find("nodepulse_collector_failures_total{collector=\"network\"} 1\n"),
              std::string::npos);
    EXPECT_NE(text.find("nodepulse_collector_failures_total{collector=\"cpu\"} 0\n"),
              std::string::npos);
}

TEST(MetricsExporterTest, NoDuplicateTypeDeclarations) {
    auto mem_collector = std::make_shared<nodepulse::collectors::MemoryCollector>(
        kFixturesDir + "/proc/meminfo_valid");
    auto mem_service = std::make_shared<nodepulse::services::MemoryService>(mem_collector);

    nodepulse::services::MetricsExporter exporter(nullptr, mem_service);
    exporter.record_http_request("/api/v1/health", "GET", 200);
    exporter.record_http_request("/api/v1/system", "GET", 200);
    exporter.record_collector_failure("cpu");

    std::string text = exporter.export_metrics();

    auto count_occurrences = [](const std::string& str, const std::string& sub) {
        size_t count = 0;
        size_t pos = 0;
        while ((pos = str.find(sub, pos)) != std::string::npos) {
            ++count;
            pos += sub.size();
        }
        return count;
    };

    EXPECT_EQ(count_occurrences(text, "# TYPE nodepulse_build_info"), 1U);
    EXPECT_EQ(count_occurrences(text, "# TYPE nodepulse_process_uptime_seconds"), 1U);
    EXPECT_EQ(count_occurrences(text, "# TYPE nodepulse_memory_used_bytes"), 1U);
    EXPECT_EQ(count_occurrences(text, "# TYPE nodepulse_memory_total_bytes"), 1U);
    EXPECT_EQ(count_occurrences(text, "# TYPE nodepulse_http_requests_total"), 1U);
    EXPECT_EQ(count_occurrences(text, "# TYPE nodepulse_collector_failures_total"), 1U);
}

TEST(MetricsExporterTest, ConcurrentScrapesAndRecordingAreThreadSafe) {
    nodepulse::services::MetricsExporter exporter;

    constexpr int kThreads = 8;
    constexpr int kIterations = 200;

    std::vector<std::thread> workers;
    workers.reserve(kThreads);

    for (int t = 0; t < kThreads; ++t) {
        workers.emplace_back([&exporter, t]() {
            for (int i = 0; i < kIterations; ++i) {
                if (t % 2 == 0) {
                    exporter.record_http_request("/api/v1/cpu", "GET", 200);
                    exporter.record_collector_failure("cpu");
                } else {
                    auto res = exporter.export_metrics();
                    EXPECT_FALSE(res.empty());
                }
            }
        });
    }

    for (auto& w : workers) {
        w.join();
    }

    std::string final_metrics = exporter.export_metrics();
    EXPECT_FALSE(final_metrics.empty());
    EXPECT_NE(final_metrics.find("nodepulse_http_requests_total"), std::string::npos);
}

TEST(MetricsExporterTest, AsyncExportMatchesSyncExport) {
    nodepulse::services::MetricsExporter exporter;
    exporter.record_http_request("/api/v1/health", "GET", 200);

    std::promise<std::string> promise;
    auto future = promise.get_future();

    exporter.export_metrics_async(
        [&promise](std::string body) { promise.set_value(std::move(body)); });

    auto async_result = future.get();
    auto sync_result = exporter.export_metrics();

    EXPECT_FALSE(async_result.empty());
    EXPECT_FALSE(sync_result.empty());

    // Both should contain the recorded HTTP request
    EXPECT_NE(async_result.find("nodepulse_http_requests_total{endpoint=\"/api/v1/"
                                "health\",method=\"GET\",status=\"200\"} 1"),
              std::string::npos);
    EXPECT_NE(sync_result.find("nodepulse_http_requests_total{endpoint=\"/api/v1/"
                               "health\",method=\"GET\",status=\"200\"} 1"),
              std::string::npos);

    // Both should contain build info and process metrics
    EXPECT_NE(async_result.find("# TYPE nodepulse_build_info gauge"), std::string::npos);
    EXPECT_NE(async_result.find("# TYPE nodepulse_process_uptime_seconds counter"),
              std::string::npos);
}
