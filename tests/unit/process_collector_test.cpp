#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include <nodepulse/collectors/process_collector.hpp>
#include <nodepulse/domain/process_info.hpp>
#include <nodepulse/services/process_service.hpp>

namespace fs = std::filesystem;

class ProcessCollectorTest : public ::testing::Test {
  protected:
    void SetUp() override {
        fixture_proc_path_ = std::string(NODEPULSE_TEST_FIXTURES_DIR) + "/proc";
    }

    std::string fixture_proc_path_;
};

TEST_F(ProcessCollectorTest, ParseStatLineStandard) {
    std::string line =
        "1248 (nodepulse_server) S 1 1248 1248 0 -1 4194304 100 0 0 0 50 25 0 0 20 0 8 0 1000 "
        "154000000 8422 18446744073709551615 0 0 0 0 0 0 0 2147483647 0 0 0 0 17 0 0 0 0 0 0 0 "
        "0 0 0 0 0 0";
    nodepulse::collectors::ProcessStatFields fields;
    bool ok = nodepulse::collectors::ProcessCollector::parse_stat_line(line, fields);
    ASSERT_TRUE(ok);
    EXPECT_EQ(fields.pid, 1248);
    EXPECT_EQ(fields.comm, "nodepulse_server");
    EXPECT_EQ(fields.state, "S");
    EXPECT_EQ(fields.ppid, 1);
    EXPECT_EQ(fields.utime, 50);
    EXPECT_EQ(fields.stime, 25);
    EXPECT_EQ(fields.num_threads, 8);
    EXPECT_EQ(fields.rss_pages, 8422);
}

TEST_F(ProcessCollectorTest, ParseStatLineWithSpacesAndParentheses) {
    std::string line =
        "2345 (proc (with) spaces) R 1 2345 2345 0 -1 4194304 200 0 0 0 120 40 0 0 20 0 4 0 2000 "
        "80000000 2000 18446744073709551615 0 0 0 0 0 0 0 2147483647 0 0 0 0 17 0 0 0 0 0 0 0 0 "
        "0 0 0 0 0";
    nodepulse::collectors::ProcessStatFields fields;
    bool ok = nodepulse::collectors::ProcessCollector::parse_stat_line(line, fields);
    ASSERT_TRUE(ok);
    EXPECT_EQ(fields.pid, 2345);
    EXPECT_EQ(fields.comm, "proc (with) spaces");
    EXPECT_EQ(fields.state, "R");
    EXPECT_EQ(fields.ppid, 1);
    EXPECT_EQ(fields.utime, 120);
    EXPECT_EQ(fields.stime, 40);
    EXPECT_EQ(fields.num_threads, 4);
    EXPECT_EQ(fields.rss_pages, 2000);
}

TEST_F(ProcessCollectorTest, ParseStatLineMalformed) {
    nodepulse::collectors::ProcessStatFields fields;
    EXPECT_FALSE(nodepulse::collectors::ProcessCollector::parse_stat_line("", fields));
    EXPECT_FALSE(nodepulse::collectors::ProcessCollector::parse_stat_line("1234", fields));
    EXPECT_FALSE(
        nodepulse::collectors::ProcessCollector::parse_stat_line("1234 (unclosed", fields));
    EXPECT_FALSE(
        nodepulse::collectors::ProcessCollector::parse_stat_line("not_a_pid (test) S", fields));
}

TEST_F(ProcessCollectorTest, ParseStatusFileContent) {
    std::string content =
        "Name:\tnodepulse_server\n"
        "State:\tS (sleeping)\n"
        "Pid:\t1248\n"
        "PPid:\t1\n"
        "Uid:\t1000\t1000\t1000\t1000\n"
        "VmSize:\t  150390 kB\n"
        "VmRSS:\t   33691 kB\n"
        "Threads:\t8\n";
    std::istringstream stream(content);
    nodepulse::collectors::ProcessStatusFields fields;
    bool ok = nodepulse::collectors::ProcessCollector::parse_status_stream(stream, fields);
    ASSERT_TRUE(ok);
    EXPECT_EQ(fields.uid, 1000);
    EXPECT_EQ(fields.vm_size_bytes, 150390ULL * 1024);
    EXPECT_EQ(fields.vm_rss_bytes, 33691ULL * 1024);
    EXPECT_EQ(fields.threads, 8);
}

TEST_F(ProcessCollectorTest, CollectProcessDetailFromFixtures) {
    nodepulse::collectors::ProcessCollector collector(fixture_proc_path_);
    auto detail_opt = collector.collect_process_detail(1248, 1.5);
    ASSERT_TRUE(detail_opt.has_value());
    EXPECT_EQ(detail_opt->pid, 1248);
    EXPECT_EQ(detail_opt->ppid, 1);
    EXPECT_EQ(detail_opt->name, "nodepulse_server");
    EXPECT_EQ(detail_opt->state, "S");
    EXPECT_EQ(detail_opt->cpu_percent, 1.5);
    EXPECT_EQ(detail_opt->thread_count, 8);
    EXPECT_EQ(detail_opt->memory_vms_bytes, 150390ULL * 1024);
    EXPECT_EQ(detail_opt->memory_rss_bytes, 33691ULL * 1024);
    EXPECT_FALSE(detail_opt->cmdline.empty());
}

TEST_F(ProcessCollectorTest, CollectProcessDetailNonExistent) {
    nodepulse::collectors::ProcessCollector collector(fixture_proc_path_);
    auto detail_opt = collector.collect_process_detail(999999, 0.0);
    EXPECT_FALSE(detail_opt.has_value());
}

TEST_F(ProcessCollectorTest, ProcessServiceSortingAndLimits) {
    auto collector = std::make_shared<nodepulse::collectors::ProcessCollector>(fixture_proc_path_);
    nodepulse::services::ProcessService service(collector);

    // Trigger one synchronous collection
    service.sample_now();

    auto procs_by_pid = service.get_processes("pid", 10);
    ASSERT_GE(procs_by_pid.size(), 2);
    // Should be sorted by pid ascending
    for (size_t i = 1; i < procs_by_pid.size(); ++i) {
        EXPECT_LE(procs_by_pid[i - 1].pid, procs_by_pid[i].pid);
    }

    auto procs_by_mem = service.get_processes("memory", 10);
    ASSERT_GE(procs_by_mem.size(), 2);
    // Should be sorted by memory descending
    for (size_t i = 1; i < procs_by_mem.size(); ++i) {
        EXPECT_GE(procs_by_mem[i - 1].memory_rss_bytes, procs_by_mem[i].memory_rss_bytes);
    }

    auto procs_limited = service.get_processes("pid", 1);
    EXPECT_EQ(procs_limited.size(), 1);
}

TEST_F(ProcessCollectorTest, ProcessServicePidValidation) {
    nodepulse::services::ProcessService service;
    EXPECT_FALSE(service.is_valid_pid(0));
    EXPECT_FALSE(service.is_valid_pid(-1));
    EXPECT_TRUE(service.is_valid_pid(1));
    EXPECT_TRUE(service.is_valid_pid(1248));
    EXPECT_GT(service.get_pid_max(), 0);
}

TEST_F(ProcessCollectorTest, CollectProcessDetailPidReuseProtection) {
    nodepulse::collectors::ProcessCollector collector(fixture_proc_path_);
    // In fixture 1248/stat, starttime is 1000
    // Matching starttime should preserve cpu_percent
    auto matching = collector.collect_process_detail(1248, 15.5, 1000);
    ASSERT_TRUE(matching.has_value());
    EXPECT_DOUBLE_EQ(matching->cpu_percent, 15.5);

    // Mismatched starttime denotes PID reuse by a new process: cpu_percent must be reset to 0.0
    auto reused = collector.collect_process_detail(1248, 15.5, 99999);
    ASSERT_TRUE(reused.has_value());
    EXPECT_DOUBLE_EQ(reused->cpu_percent, 0.0);
}
