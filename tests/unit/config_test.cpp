#include <cstdlib>
#include <string>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <nodepulse/config/config.hpp>

namespace nodepulse::config {

TEST(ConfigTest, DefaultsAreValid) {
    Config cfg;
    auto errors = cfg.validate();
    EXPECT_TRUE(errors.empty());
    EXPECT_EQ(cfg.server.host, "127.0.0.1");
    EXPECT_EQ(cfg.server.port, 8080);
    EXPECT_EQ(cfg.server.threads, 4);
    EXPECT_EQ(cfg.server.log_level, "info");
    EXPECT_EQ(cfg.server.log_format, "text");
    EXPECT_TRUE(cfg.rate_limiting.enabled);
    EXPECT_TRUE(cfg.collectors.system.enabled);
    EXPECT_FALSE(cfg.collectors.docker.enabled);
}

TEST(ConfigTest, LoadsCustomJsonCorrectly) {
    nlohmann::json j = {
        {"server",
         {{"host", "0.0.0.0"},
          {"port", 9090},
          {"threads", 8},
          {"log_level", "debug"},
          {"log_format", "json"}}},
        {"security", {{"api_key", "secret123"}, {"constant_time_comparison", true}}},
        {"rate_limiting",
         {{"enabled", false}, {"requests_per_minute", 200}, {"burst_capacity", 50}}},
        {"collectors",
         {{"docker",
           {{"enabled", true}, {"socket_path", "/custom/docker.sock"}, {"timeout_ms", 3000}}}}}};

    auto cfg = Config::load_from_json(j);
    EXPECT_EQ(cfg.server.host, "0.0.0.0");
    EXPECT_EQ(cfg.server.port, 9090);
    EXPECT_EQ(cfg.server.threads, 8);
    EXPECT_EQ(cfg.server.log_level, "debug");
    EXPECT_EQ(cfg.server.log_format, "json");
    EXPECT_EQ(cfg.security.api_key, "secret123");
    EXPECT_FALSE(cfg.rate_limiting.enabled);
    EXPECT_EQ(cfg.rate_limiting.requests_per_minute, 200);
    EXPECT_TRUE(cfg.collectors.docker.enabled);
    EXPECT_EQ(cfg.collectors.docker.socket_path, "/custom/docker.sock");
    EXPECT_EQ(cfg.collectors.docker.timeout_ms, 3000);

    auto errors = cfg.validate();
    EXPECT_TRUE(errors.empty());
}

TEST(ConfigTest, ValidationCatchesInvalidHost) {
    Config cfg;
    cfg.server.host = "";
    auto errors = cfg.validate();
    EXPECT_FALSE(errors.empty());
    bool found_host_err = false;
    for (const auto& err : errors) {
        if (err.find("server.host") != std::string::npos) {
            found_host_err = true;
        }
    }
    EXPECT_TRUE(found_host_err);
}

TEST(ConfigTest, ValidationCatchesInvalidPort) {
    Config cfg;
    cfg.server.port = 0;
    auto errors = cfg.validate();
    EXPECT_FALSE(errors.empty());
}

TEST(ConfigTest, ValidationCatchesInvalidThreads) {
    Config cfg;
    cfg.server.threads = 0;
    auto errors = cfg.validate();
    EXPECT_FALSE(errors.empty());

    cfg.server.threads = 1000;
    errors = cfg.validate();
    EXPECT_FALSE(errors.empty());
}

TEST(ConfigTest, ValidationCatchesInvalidLogLevel) {
    Config cfg;
    cfg.server.log_level = "invalid_level";
    auto errors = cfg.validate();
    EXPECT_FALSE(errors.empty());
}

TEST(ConfigTest, ValidationCatchesInvalidLogFormat) {
    Config cfg;
    cfg.server.log_format = "xml";
    auto errors = cfg.validate();
    EXPECT_FALSE(errors.empty());
}

TEST(ConfigTest, ValidationCatchesInvalidIntervals) {
    Config cfg;
    cfg.collectors.cpu.sample_interval_ms = 50;  // Minimum is 100ms
    auto errors = cfg.validate();
    EXPECT_FALSE(errors.empty());

    cfg.collectors.cpu.sample_interval_ms = 1000;
    cfg.sse.interval_ms = 10;  // Minimum is 100ms
    errors = cfg.validate();
    EXPECT_FALSE(errors.empty());
}

TEST(ConfigTest, EnvironmentOverridesApply) {
    setenv("NODEPULSE_HOST", "127.0.0.2", 1);
    setenv("NODEPULSE_PORT", "8888", 1);
    setenv("NODEPULSE_THREADS", "12", 1);
    setenv("NODEPULSE_LOG_LEVEL", "warn", 1);
    setenv("NODEPULSE_API_KEY", "env_secret_key", 1);

    Config cfg;
    cfg.apply_env_overrides();

    EXPECT_EQ(cfg.server.host, "127.0.0.2");
    EXPECT_EQ(cfg.server.port, 8888);
    EXPECT_EQ(cfg.server.threads, 12);
    EXPECT_EQ(cfg.server.log_level, "warn");
    EXPECT_EQ(cfg.security.api_key, "env_secret_key");

    unsetenv("NODEPULSE_HOST");
    unsetenv("NODEPULSE_PORT");
    unsetenv("NODEPULSE_THREADS");
    unsetenv("NODEPULSE_LOG_LEVEL");
    unsetenv("NODEPULSE_API_KEY");
}

TEST(ConfigTest, LoadFromFileNonExistentThrows) {
    EXPECT_THROW(Config::load_from_file("/path/does/not/exist/config.json"), std::runtime_error);
}

TEST(ConfigTest, SseMaxClientsDefaultsTo64) {
    Config cfg;
    EXPECT_EQ(cfg.sse.max_clients, 64U);
    EXPECT_TRUE(cfg.validate().empty());
}

TEST(ConfigTest, SseMaxClientsConfigurationOverride) {
    nlohmann::json j = {{"sse", {{"enabled", true}, {"interval_ms", 500}, {"max_clients", 128}}}};
    auto cfg = Config::load_from_json(j);
    EXPECT_EQ(cfg.sse.max_clients, 128U);
    EXPECT_TRUE(cfg.validate().empty());

    setenv("NODEPULSE_SSE_MAX_CLIENTS", "256", 1);
    cfg.apply_env_overrides();
    EXPECT_EQ(cfg.sse.max_clients, 256U);
    EXPECT_TRUE(cfg.validate().empty());
    unsetenv("NODEPULSE_SSE_MAX_CLIENTS");
}

TEST(ConfigTest, SseMaxClientsValidationRejectsZeroAndOversized) {
    Config cfg_zero;
    cfg_zero.sse.max_clients = 0;
    auto errors_zero = cfg_zero.validate();
    EXPECT_FALSE(errors_zero.empty());

    Config cfg_huge;
    cfg_huge.sse.max_clients = 20000;
    auto errors_huge = cfg_huge.validate();
    EXPECT_FALSE(errors_huge.empty());
}

}  // namespace nodepulse::config
