#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <nodepulse/utils/logger.hpp>

TEST(SmokeTest, TestHarnessOperational) {
    EXPECT_TRUE(true);
    EXPECT_EQ(1 + 1, 2);
}

TEST(SmokeTest, LoggerInitialization) {
    nodepulse::utils::Logger::init("debug");
    auto logger = nodepulse::utils::Logger::get();
    ASSERT_NE(logger, nullptr);
    EXPECT_EQ(logger->name(), "nodepulse");
}

TEST(SmokeTest, JsonSerialization) {
    nlohmann::json j = {{"status", "ok"}};
    EXPECT_EQ(j["status"], "ok");
}
