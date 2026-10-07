#include <chrono>
#include <future>
#include <memory>
#include <optional>
#include <vector>

#include <gtest/gtest.h>

#include <nodepulse/config/config.hpp>
#include <nodepulse/domain/host_snapshot.hpp>
#include <nodepulse/repositories/postgres_repository.hpp>
#include <nodepulse/services/cpu_service.hpp>
#include <nodepulse/services/history_service.hpp>
#include <nodepulse/services/memory_service.hpp>
#include <nodepulse/services/network_service.hpp>
#include <nodepulse/services/system_service.hpp>

namespace nodepulse::services {

class HistoryServiceTest : public ::testing::Test {
  protected:
    void SetUp() override {
        mock_repo_ = std::make_shared<repositories::MockPostgresRepository>();
        system_service_ = std::make_shared<SystemService>();
        cpu_service_ = std::make_shared<CpuService>();
        memory_service_ = std::make_shared<MemoryService>();
        network_service_ = std::make_shared<NetworkService>();

        config_.enabled = true;
        config_.connection_string = "postgresql://test:pass@localhost:5432/test_db";
        config_.snapshot_interval_seconds = 1;
    }

    std::shared_ptr<repositories::MockPostgresRepository> mock_repo_;
    std::shared_ptr<SystemService> system_service_;
    std::shared_ptr<CpuService> cpu_service_;
    std::shared_ptr<MemoryService> memory_service_;
    std::shared_ptr<NetworkService> network_service_;
    config::PostgresConfig config_;
};

TEST_F(HistoryServiceTest, LifecycleStartAndStop) {
    HistoryService service(mock_repo_, system_service_, cpu_service_, memory_service_,
                           network_service_, config_);

    EXPECT_FALSE(service.is_running());
    EXPECT_TRUE(service.is_enabled());

    service.start();
    EXPECT_TRUE(service.is_running());

    service.stop();
    EXPECT_FALSE(service.is_running());
}

TEST_F(HistoryServiceTest, DisabledDoesNotStart) {
    config_.enabled = false;
    HistoryService service(mock_repo_, system_service_, cpu_service_, memory_service_,
                           network_service_, config_);

    EXPECT_FALSE(service.is_enabled());
    service.start();
    EXPECT_FALSE(service.is_running());
}

TEST_F(HistoryServiceTest, CollectSnapshotDerivesMetrics) {
    HistoryService service(mock_repo_, system_service_, cpu_service_, memory_service_,
                           network_service_, config_);

    auto snapshot = service.collect_snapshot();
    EXPECT_FALSE(snapshot.hostname.empty());
    EXPECT_GE(snapshot.mem_total_bytes, 0ULL);
}

TEST_F(HistoryServiceTest, TriggerSnapshotSavesToRepository) {
    HistoryService service(mock_repo_, system_service_, cpu_service_, memory_service_,
                           network_service_, config_);

    EXPECT_EQ(mock_repo_->record_count(), 0U);
    EXPECT_TRUE(service.trigger_snapshot());
    EXPECT_EQ(mock_repo_->record_count(), 1U);
}

TEST_F(HistoryServiceTest, TriggerSnapshotDropsSampleWhenRepoFailsWithoutCrash) {
    HistoryService service(mock_repo_, system_service_, cpu_service_, memory_service_,
                           network_service_, config_);

    mock_repo_->set_connected(false);
    EXPECT_FALSE(service.trigger_snapshot());
    EXPECT_EQ(mock_repo_->record_count(), 0U);
}

}  // namespace nodepulse::services
