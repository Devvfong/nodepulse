#include <gtest/gtest.h>

#include <nodepulse/collectors/service_collector.hpp>
#include <nodepulse/domain/service_info.hpp>
#include <nodepulse/services/service_manager_service.hpp>

class ServiceCollectorTest : public ::testing::Test {};

TEST_F(ServiceCollectorTest, UnitNameValidation) {
    EXPECT_TRUE(nodepulse::collectors::ServiceCollector::is_valid_unit_name("nodepulse.service"));
    EXPECT_TRUE(nodepulse::collectors::ServiceCollector::is_valid_unit_name("ssh"));
    EXPECT_TRUE(nodepulse::collectors::ServiceCollector::is_valid_unit_name("user@1000.service"));
    EXPECT_TRUE(
        nodepulse::collectors::ServiceCollector::is_valid_unit_name("systemd-udevd.service"));
    EXPECT_TRUE(nodepulse::collectors::ServiceCollector::is_valid_unit_name("app_1.service"));

    EXPECT_FALSE(nodepulse::collectors::ServiceCollector::is_valid_unit_name(""));
    EXPECT_FALSE(nodepulse::collectors::ServiceCollector::is_valid_unit_name("../ssh.service"));
    EXPECT_FALSE(nodepulse::collectors::ServiceCollector::is_valid_unit_name("ssh;reboot"));
    EXPECT_FALSE(nodepulse::collectors::ServiceCollector::is_valid_unit_name("ssh\nservice"));
    EXPECT_FALSE(nodepulse::collectors::ServiceCollector::is_valid_unit_name("ssh service"));

    // Length boundary tests
    std::string valid_max_exact = std::string(248, 'a') + ".service";  // 256 chars
    EXPECT_TRUE(nodepulse::collectors::ServiceCollector::is_valid_unit_name(valid_max_exact));

    std::string valid_max_unnormalized = std::string(248, 'a');  // 248 chars + 8 = 256
    EXPECT_TRUE(
        nodepulse::collectors::ServiceCollector::is_valid_unit_name(valid_max_unnormalized));

    std::string too_long_exact = std::string(249, 'a') + ".service";  // 257 chars
    EXPECT_FALSE(nodepulse::collectors::ServiceCollector::is_valid_unit_name(too_long_exact));

    std::string too_long_unnormalized = std::string(249, 'a');  // 249 chars + 8 = 257
    EXPECT_FALSE(
        nodepulse::collectors::ServiceCollector::is_valid_unit_name(too_long_unnormalized));
}

TEST_F(ServiceCollectorTest, UnitNameNormalization) {
    EXPECT_EQ(nodepulse::collectors::ServiceCollector::normalize_service_name("nodepulse"),
              "nodepulse.service");
    EXPECT_EQ(nodepulse::collectors::ServiceCollector::normalize_service_name("nodepulse.service"),
              "nodepulse.service");
    EXPECT_EQ(nodepulse::collectors::ServiceCollector::normalize_service_name("user@1000"),
              "user@1000.service");
}

TEST_F(ServiceCollectorTest, MockListUnits) {
    auto collector = std::make_shared<nodepulse::collectors::ServiceCollector>();

    collector->set_custom_providers(
        [](const std::string& state_filter)
            -> std::optional<std::vector<nodepulse::domain::ServiceInfo>> {
            std::vector<nodepulse::domain::ServiceInfo> all = {
                {"nodepulse.service", "NodePulse Monitoring", "loaded", "active", "running",
                 "enabled"},
                {"ssh.service", "OpenSSH Server", "loaded", "active", "running", "enabled"},
                {"failed-job.service", "Failed Test Service", "loaded", "failed", "failed",
                 "disabled"}};

            if (state_filter == "all") {
                return all;
            }

            std::vector<nodepulse::domain::ServiceInfo> filtered;
            for (const auto& s : all) {
                if (s.active_state == state_filter) {
                    filtered.push_back(s);
                }
            }
            return filtered;
        },
        nullptr);

    auto all_services = collector->list_services("all");
    ASSERT_TRUE(all_services.has_value());
    EXPECT_EQ(all_services->size(), 3);

    auto active_services = collector->list_services("active");
    ASSERT_TRUE(active_services.has_value());
    EXPECT_EQ(active_services->size(), 2);

    auto failed_services = collector->list_services("failed");
    ASSERT_TRUE(failed_services.has_value());
    EXPECT_EQ(failed_services->size(), 1);
    EXPECT_EQ((*failed_services)[0].name, "failed-job.service");
}

TEST_F(ServiceCollectorTest, MockGetUnitDetail) {
    auto collector = std::make_shared<nodepulse::collectors::ServiceCollector>();

    collector->set_custom_providers(
        nullptr, [](const std::string& unit_name) -> nodepulse::collectors::ServiceDetailResult {
            if (unit_name == "nodepulse.service") {
                nodepulse::domain::ServiceDetail d;
                d.name = "nodepulse.service";
                d.description = "NodePulse Monitoring";
                d.load_state = "loaded";
                d.active_state = "active";
                d.sub_state = "running";
                d.unit_file_state = "enabled";
                d.main_pid = 1248;
                d.restart_count = 0;
                d.active_enter_timestamp_utc = 1728211200;
                d.memory_current_bytes = 34500000;
                return {nodepulse::collectors::ServiceStatusResult::kOk, d};
            }
            if (unit_name == "nonexistent.service") {
                return {nodepulse::collectors::ServiceStatusResult::kNotFound, std::nullopt};
            }
            return {nodepulse::collectors::ServiceStatusResult::kCollectorFailure, std::nullopt};
        });

    auto ok_res = collector->get_service_detail("nodepulse.service");
    EXPECT_EQ(ok_res.status, nodepulse::collectors::ServiceStatusResult::kOk);
    ASSERT_TRUE(ok_res.detail.has_value());
    EXPECT_EQ(ok_res.detail->main_pid, 1248);

    auto not_found_res = collector->get_service_detail("nonexistent.service");
    EXPECT_EQ(not_found_res.status, nodepulse::collectors::ServiceStatusResult::kNotFound);
    EXPECT_FALSE(not_found_res.detail.has_value());

    auto failure_res = collector->get_service_detail("broken.service");
    EXPECT_EQ(failure_res.status, nodepulse::collectors::ServiceStatusResult::kCollectorFailure);
    EXPECT_FALSE(failure_res.detail.has_value());
}

TEST_F(ServiceCollectorTest, ServiceManagerServiceLimitsAndNormalization) {
    auto collector = std::make_shared<nodepulse::collectors::ServiceCollector>();
    collector->set_custom_providers(
        [](const std::string&) -> std::optional<std::vector<nodepulse::domain::ServiceInfo>> {
            std::vector<nodepulse::domain::ServiceInfo> list;
            for (int i = 0; i < 10; ++i) {
                list.push_back({"svc" + std::to_string(i) + ".service", "Desc", "loaded", "active",
                                "running", "enabled"});
            }
            return list;
        },
        [](const std::string& name) -> nodepulse::collectors::ServiceDetailResult {
            if (name == "test.service") {
                nodepulse::domain::ServiceDetail d;
                d.name = "test.service";
                return {nodepulse::collectors::ServiceStatusResult::kOk, d};
            }
            return {nodepulse::collectors::ServiceStatusResult::kNotFound, std::nullopt};
        });

    nodepulse::services::ServiceManagerService service(collector);

    auto list = service.list_services("all", 5);
    ASSERT_TRUE(list.has_value());
    EXPECT_EQ(list->size(), 5);

    // Test automatic normalization of "test" -> "test.service"
    auto detail = service.get_service_detail("test");
    EXPECT_EQ(detail.status, nodepulse::collectors::ServiceStatusResult::kOk);
    ASSERT_TRUE(detail.detail.has_value());
    EXPECT_EQ(detail.detail->name, "test.service");
}
