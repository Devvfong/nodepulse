#include <cctype>
#include <chrono>
#include <future>
#include <memory>
#include <sstream>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <nodepulse/collectors/cpu_collector.hpp>
#include <nodepulse/collectors/disk_collector.hpp>
#include <nodepulse/collectors/memory_collector.hpp>
#include <nodepulse/collectors/network_collector.hpp>
#include <nodepulse/collectors/process_collector.hpp>
#include <nodepulse/collectors/service_collector.hpp>
#include <nodepulse/collectors/system_collector.hpp>
#include <nodepulse/config/config.hpp>
#include <nodepulse/controllers/cpu_controller.hpp>
#include <nodepulse/controllers/disk_controller.hpp>
#include <nodepulse/controllers/memory_controller.hpp>
#include <nodepulse/controllers/network_controller.hpp>
#include <nodepulse/controllers/process_controller.hpp>
#include <nodepulse/controllers/service_controller.hpp>
#include <nodepulse/controllers/system_controller.hpp>
#include <nodepulse/server/server.hpp>
#include <nodepulse/services/cpu_service.hpp>
#include <nodepulse/services/disk_service.hpp>
#include <nodepulse/services/memory_service.hpp>
#include <nodepulse/services/network_service.hpp>
#include <nodepulse/services/process_service.hpp>
#include <nodepulse/services/service_manager_service.hpp>
#include <nodepulse/services/system_service.hpp>
#include <nodepulse/utils/logger.hpp>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {

struct SimpleHttpResponse {
    int status_code{0};
    std::unordered_map<std::string, std::string> headers;
    std::vector<std::pair<std::string, std::string>> all_headers;
    std::string body;

    [[nodiscard]] std::string get_header(std::string_view name) const {
        std::string lower_name;
        lower_name.reserve(name.size());
        for (char c : name) {
            lower_name.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
        auto it = headers.find(lower_name);
        if (it != headers.end()) {
            return it->second;
        }
        return "";
    }

    [[nodiscard]] bool has_header(std::string_view name) const {
        return !get_header(name).empty();
    }

    [[nodiscard]] size_t count_header(std::string_view name) const {
        std::string lower_name;
        lower_name.reserve(name.size());
        for (char c : name) {
            lower_name.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
        }
        size_t count = 0;
        for (const auto& [k, v] : all_headers) {
            if (k == lower_name) {
                ++count;
            }
        }
        return count;
    }
};

SimpleHttpResponse send_http_get(
    const std::string& host, uint16_t port, const std::string& path,
    const std::unordered_map<std::string, std::string>& custom_headers = {}) {
    SimpleHttpResponse result;

    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        return result;
    }

    struct timeval tv {};
    tv.tv_sec = 2;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, host.c_str(), &server_addr.sin_addr);

    if (connect(sock, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr)) < 0) {
        close(sock);
        return result;
    }

    std::ostringstream req_stream;
    req_stream << "GET " << path << " HTTP/1.1\r\n"
               << "Host: " << host << ":" << port << "\r\n"
               << "User-Agent: NodePulse-IntegrationTest/1.0\r\n"
               << "Connection: close\r\n";

    for (const auto& [name, val] : custom_headers) {
        req_stream << name << ": " << val << "\r\n";
    }
    req_stream << "\r\n";

    std::string req_str = req_stream.str();
    if (send(sock, req_str.data(), req_str.size(), 0) < 0) {
        close(sock);
        return result;
    }

    std::string response_data;
    char buffer[4096];
    ssize_t bytes_read = 0;
    while ((bytes_read = recv(sock, buffer, sizeof(buffer), 0)) > 0) {
        response_data.append(buffer, static_cast<size_t>(bytes_read));
    }
    close(sock);

    auto header_end = response_data.find("\r\n\r\n");
    if (header_end == std::string::npos) {
        return result;
    }

    std::string header_part = response_data.substr(0, header_end);
    result.body = response_data.substr(header_end + 4);

    std::istringstream stream(header_part);
    std::string status_line;
    if (std::getline(stream, status_line)) {
        std::istringstream status_stream(status_line);
        std::string http_version;
        status_stream >> http_version >> result.status_code;
    }

    std::string header_line;
    while (std::getline(stream, header_line) && header_line != "\r") {
        auto colon = header_line.find(':');
        if (colon != std::string::npos) {
            std::string key = header_line.substr(0, colon);
            std::string value = header_line.substr(colon + 1);
            for (char& c : key) {
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
            while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) {
                value.erase(value.begin());
            }
            while (!value.empty() && (value.back() == '\r' || value.back() == ' ')) {
                value.pop_back();
            }
            result.headers[key] = value;
            result.all_headers.emplace_back(key, value);
        }
    }

    return result;
}

const std::string kValidApiKey = "np_test_key_phase9_secret_xyz123";

SimpleHttpResponse send_auth_get(const std::string& host, uint16_t port, const std::string& path,
                                 std::unordered_map<std::string, std::string> custom_headers = {}) {
    if (custom_headers.find("X-API-Key") == custom_headers.end() &&
        custom_headers.find("x-api-key") == custom_headers.end()) {
        custom_headers["X-API-Key"] = kValidApiKey;
    }
    return send_http_get(host, port, path, custom_headers);
}

class HttpIntegrationTest : public ::testing::Test {
  protected:
    static inline std::unique_ptr<std::thread> server_thread_;
    static inline std::unique_ptr<nodepulse::server::Server> server_;
    static constexpr uint16_t kTestPort = 18080;
    static constexpr const char* kTestHost = "127.0.0.1";

    static void SetUpTestSuite() {
        nodepulse::utils::Logger::init("warn", false);

        nodepulse::config::Config cfg;
        cfg.server.host = kTestHost;
        cfg.server.port = kTestPort;
        cfg.server.threads = 2;
        cfg.security.api_key = kValidApiKey;

        server_ = std::make_unique<nodepulse::server::Server>(cfg);
        server_->setup();

        server_thread_ = std::make_unique<std::thread>([]() { server_->run(); });

        // Wait up to 3 seconds for server to be ready
        bool ready = false;
        for (int i = 0; i < 30; ++i) {
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
            auto resp = send_http_get(kTestHost, kTestPort, "/api/v1/health");
            if (resp.status_code == 200) {
                ready = true;
                break;
            }
        }
        ASSERT_TRUE(ready) << "HTTP server did not become ready within timeout";
    }

    static void TearDownTestSuite() {
        if (server_) {
            server_->stop();
        }
        if (server_thread_ && server_thread_->joinable()) {
            server_thread_->join();
        }
        server_thread_.reset();
        server_.reset();
    }
};

}  // namespace

TEST_F(HttpIntegrationTest, HealthEndpointReturns200AndValidSchema) {
    auto resp = send_http_get(kTestHost, kTestPort, "/api/v1/health");
    EXPECT_EQ(resp.status_code, 200);

    EXPECT_TRUE(resp.has_header("content-type"));
    EXPECT_NE(resp.get_header("content-type").find("application/json"), std::string::npos);

    EXPECT_TRUE(resp.has_header("x-request-id"));
    EXPECT_FALSE(resp.get_header("x-request-id").empty());

    auto json_body = nlohmann::json::parse(resp.body);
    EXPECT_EQ(json_body["status"], "healthy");
    EXPECT_EQ(json_body["version"], "0.1.0");
    EXPECT_TRUE(json_body.contains("uptime_seconds"));
    EXPECT_GE(json_body["uptime_seconds"].get<double>(), 0.0);
}

TEST_F(HttpIntegrationTest, PreservesCustomRequestIdHeader) {
    const std::string custom_id = "test-custom-uuid-abc-123";
    std::unordered_map<std::string, std::string> headers = {{"X-Request-ID", custom_id}};

    auto resp = send_http_get(kTestHost, kTestPort, "/api/v1/health", headers);
    EXPECT_EQ(resp.status_code, 200);

    EXPECT_TRUE(resp.has_header("x-request-id"));
    EXPECT_EQ(resp.get_header("x-request-id"), custom_id);
}

TEST_F(HttpIntegrationTest, UnsupportedRouteReturnsStandard404ErrorEnvelope) {
    auto resp = send_http_get(kTestHost, kTestPort, "/api/v1/non_existent_route");
    EXPECT_EQ(resp.status_code, 404);

    EXPECT_TRUE(resp.has_header("content-type"));
    EXPECT_NE(resp.get_header("content-type").find("application/json"), std::string::npos);

    EXPECT_TRUE(resp.has_header("x-request-id"));
    EXPECT_FALSE(resp.get_header("x-request-id").empty());

    auto json_body = nlohmann::json::parse(resp.body);
    ASSERT_TRUE(json_body.contains("error"));
    EXPECT_EQ(json_body["error"]["code"], "RESOURCE_NOT_FOUND");
    EXPECT_FALSE(json_body["error"]["message"].get<std::string>().empty());
    EXPECT_FALSE(json_body["error"]["timestamp"].get<std::string>().empty());
    EXPECT_TRUE(json_body["error"]["details"].is_array());
}

TEST_F(HttpIntegrationTest, ConcurrentRequestsDoNotCrashServer) {
    constexpr int kNumThreads = 10;
    constexpr int kRequestsPerThread = 5;

    std::vector<std::future<bool>> futures;
    for (int t = 0; t < kNumThreads; ++t) {
        futures.push_back(std::async(std::launch::async, []() {
            for (int r = 0; r < kRequestsPerThread; ++r) {
                auto resp = send_http_get(kTestHost, kTestPort, "/api/v1/health");
                if (resp.status_code != 200) {
                    return false;
                }
                auto j = nlohmann::json::parse(resp.body);
                if (j["status"] != "healthy") {
                    return false;
                }
            }
            return true;
        }));
    }

    for (auto& f : futures) {
        EXPECT_TRUE(f.get());
    }
}

TEST_F(HttpIntegrationTest, SystemEndpointReturns200AndValidSchema) {
    auto resp = send_auth_get(kTestHost, kTestPort, "/api/v1/system");
    EXPECT_EQ(resp.status_code, 200);

    EXPECT_TRUE(resp.has_header("content-type"));
    EXPECT_NE(resp.get_header("content-type").find("application/json"), std::string::npos);

    EXPECT_TRUE(resp.has_header("x-request-id"));
    EXPECT_FALSE(resp.get_header("x-request-id").empty());

    auto json_body = nlohmann::json::parse(resp.body);
    EXPECT_TRUE(json_body.is_object());

    ASSERT_TRUE(json_body.contains("hostname"));
    EXPECT_TRUE(json_body["hostname"].is_string());
    EXPECT_FALSE(json_body["hostname"].get<std::string>().empty());

    ASSERT_TRUE(json_body.contains("os_name"));
    EXPECT_TRUE(json_body["os_name"].is_string());
    EXPECT_FALSE(json_body["os_name"].get<std::string>().empty());

    ASSERT_TRUE(json_body.contains("os_version"));
    EXPECT_TRUE(json_body["os_version"].is_string());

    ASSERT_TRUE(json_body.contains("kernel_version"));
    EXPECT_TRUE(json_body["kernel_version"].is_string());
    EXPECT_FALSE(json_body["kernel_version"].get<std::string>().empty());

    ASSERT_TRUE(json_body.contains("architecture"));
    EXPECT_TRUE(json_body["architecture"].is_string());
    EXPECT_FALSE(json_body["architecture"].get<std::string>().empty());

    ASSERT_TRUE(json_body.contains("boot_time_utc"));
    EXPECT_TRUE(json_body["boot_time_utc"].is_number_unsigned());
    EXPECT_GT(json_body["boot_time_utc"].get<uint64_t>(), 0ULL);

    ASSERT_TRUE(json_body.contains("uptime_seconds"));
    EXPECT_TRUE(json_body["uptime_seconds"].is_number());
    EXPECT_GE(json_body["uptime_seconds"].get<double>(), 0.0);
}

TEST_F(HttpIntegrationTest, RejectsInvalidRequestIdHeaderAndGeneratesSafeUuid) {
    const std::string invalid_id = "malicious;id<script>";
    std::unordered_map<std::string, std::string> headers = {{"X-Request-ID", invalid_id}};

    auto resp = send_auth_get(kTestHost, kTestPort, "/api/v1/system", headers);
    EXPECT_EQ(resp.status_code, 200);

    EXPECT_TRUE(resp.has_header("x-request-id"));
    std::string returned_id = resp.get_header("x-request-id");
    EXPECT_NE(returned_id, invalid_id);
    EXPECT_EQ(returned_id.find(';'), std::string::npos);
    EXPECT_EQ(returned_id.find('<'), std::string::npos);
    EXPECT_EQ(returned_id.find('>'), std::string::npos);
    EXPECT_FALSE(returned_id.empty());

    // Test overly long request ID (> 64 chars)
    const std::string long_id(100, 'a');
    headers = {{"X-Request-ID", long_id}};
    resp = send_auth_get(kTestHost, kTestPort, "/api/v1/system", headers);
    EXPECT_EQ(resp.status_code, 200);
    EXPECT_TRUE(resp.has_header("x-request-id"));
    returned_id = resp.get_header("x-request-id");
    EXPECT_NE(returned_id, long_id);
    EXPECT_LE(returned_id.length(), 64U);
    EXPECT_FALSE(returned_id.empty());
}

TEST_F(HttpIntegrationTest, SystemEndpointHandlesCollectorFailureGracefully) {
    auto failing_collector = std::make_shared<nodepulse::collectors::SystemCollector>(
        "/etc/os-release", "/nonexistent/proc/uptime", "/proc/stat");
    auto failing_service = std::make_shared<nodepulse::services::SystemService>(failing_collector);
    nodepulse::controllers::SystemController::set_system_service(failing_service);

    auto resp = send_auth_get(kTestHost, kTestPort, "/api/v1/system");
    EXPECT_EQ(resp.status_code, 500);

    EXPECT_TRUE(resp.has_header("content-type"));
    EXPECT_NE(resp.get_header("content-type").find("application/json"), std::string::npos);

    auto json_body = nlohmann::json::parse(resp.body);
    ASSERT_TRUE(json_body.contains("error"));
    EXPECT_EQ(json_body["error"]["code"], "COLLECTOR_FAILURE");
    EXPECT_FALSE(json_body["error"]["message"].get<std::string>().empty());
    EXPECT_TRUE(json_body["error"]["details"].is_array());
    ASSERT_EQ(json_body["error"]["details"].size(), 1U);
    EXPECT_EQ(json_body["error"]["details"][0]["collector"], "system_collector");

    nodepulse::controllers::SystemController::set_system_service(nullptr);
}

TEST_F(HttpIntegrationTest, ConcurrentSystemAndHealthRequests) {
    constexpr int kNumThreads = 8;
    constexpr int kRequestsPerThread = 5;

    std::vector<std::future<bool>> futures;
    for (int t = 0; t < kNumThreads; ++t) {
        futures.push_back(std::async(std::launch::async, [t]() {
            for (int r = 0; r < kRequestsPerThread; ++r) {
                std::string path = (t % 2 == 0) ? "/api/v1/system" : "/api/v1/health";
                auto resp = send_auth_get(kTestHost, kTestPort, path);
                if (resp.status_code != 200) {
                    return false;
                }
                auto j = nlohmann::json::parse(resp.body);
                if (path == "/api/v1/system") {
                    if (!j.contains("hostname") || !j.contains("uptime_seconds")) {
                        return false;
                    }
                } else {
                    if (j["status"] != "healthy") {
                        return false;
                    }
                }
            }
            return true;
        }));
    }

    for (auto& f : futures) {
        EXPECT_TRUE(f.get());
    }
}

TEST_F(HttpIntegrationTest, CpuEndpointReturns200AndValidSchema) {
    auto resp = send_auth_get(kTestHost, kTestPort, "/api/v1/cpu");
    EXPECT_EQ(resp.status_code, 200);

    EXPECT_TRUE(resp.has_header("content-type"));
    EXPECT_NE(resp.get_header("content-type").find("application/json"), std::string::npos);

    EXPECT_TRUE(resp.has_header("x-request-id"));
    EXPECT_FALSE(resp.get_header("x-request-id").empty());

    auto json_body = nlohmann::json::parse(resp.body);
    EXPECT_TRUE(json_body.is_object());

    ASSERT_TRUE(json_body.contains("measurement_status"));
    EXPECT_TRUE(json_body["measurement_status"].is_string());
    std::string status = json_body["measurement_status"].get<std::string>();
    EXPECT_TRUE(status == "warming_up" || status == "ready" || status == "cached");

    ASSERT_TRUE(json_body.contains("usage_percent"));
    if (json_body["usage_percent"].is_null()) {
        EXPECT_EQ(status, "warming_up");
    } else {
        EXPECT_TRUE(json_body["usage_percent"].is_number());
        EXPECT_GE(json_body["usage_percent"].get<double>(), 0.0);
        EXPECT_LE(json_body["usage_percent"].get<double>(), 100.0);
    }

    ASSERT_TRUE(json_body.contains("model_name"));
    EXPECT_TRUE(json_body["model_name"].is_string());

    ASSERT_TRUE(json_body.contains("physical_cores"));
    EXPECT_TRUE(json_body["physical_cores"].is_number_unsigned());
    EXPECT_GE(json_body["physical_cores"].get<uint32_t>(), 1U);

    ASSERT_TRUE(json_body.contains("logical_cores"));
    EXPECT_TRUE(json_body["logical_cores"].is_number_unsigned());
    EXPECT_GE(json_body["logical_cores"].get<uint32_t>(), 1U);

    ASSERT_TRUE(json_body.contains("load_average"));
    EXPECT_TRUE(json_body["load_average"].is_object());
    ASSERT_TRUE(json_body["load_average"].contains("one_minute"));
    EXPECT_TRUE(json_body["load_average"]["one_minute"].is_number());
    EXPECT_GE(json_body["load_average"]["one_minute"].get<double>(), 0.0);
    ASSERT_TRUE(json_body["load_average"].contains("five_minute"));
    EXPECT_TRUE(json_body["load_average"]["five_minute"].is_number());
    EXPECT_GE(json_body["load_average"]["five_minute"].get<double>(), 0.0);
    ASSERT_TRUE(json_body["load_average"].contains("fifteen_minute"));
    EXPECT_TRUE(json_body["load_average"]["fifteen_minute"].is_number());
    EXPECT_GE(json_body["load_average"]["fifteen_minute"].get<double>(), 0.0);

    ASSERT_TRUE(json_body.contains("cores"));
    EXPECT_TRUE(json_body["cores"].is_array());
    EXPECT_GE(json_body["cores"].size(), 1U);

    for (const auto& core : json_body["cores"]) {
        ASSERT_TRUE(core.contains("core_id"));
        EXPECT_TRUE(core["core_id"].is_number_unsigned());
        ASSERT_TRUE(core.contains("usage_percent"));
        if (!core["usage_percent"].is_null()) {
            EXPECT_TRUE(core["usage_percent"].is_number());
            EXPECT_GE(core["usage_percent"].get<double>(), 0.0);
            EXPECT_LE(core["usage_percent"].get<double>(), 100.0);
        }
    }
}

TEST_F(HttpIntegrationTest, CpuEndpointHandlesCollectorFailureGracefully) {
    auto failing_collector = std::make_shared<nodepulse::collectors::CpuCollector>(
        "/nonexistent/proc/stat", "/proc/loadavg", "/proc/cpuinfo");
    auto failing_service = std::make_shared<nodepulse::services::CpuService>(failing_collector);
    nodepulse::controllers::CpuController::set_cpu_service(failing_service);

    auto resp = send_auth_get(kTestHost, kTestPort, "/api/v1/cpu");
    EXPECT_EQ(resp.status_code, 500);

    EXPECT_TRUE(resp.has_header("content-type"));
    EXPECT_NE(resp.get_header("content-type").find("application/json"), std::string::npos);

    auto json_body = nlohmann::json::parse(resp.body);
    ASSERT_TRUE(json_body.contains("error"));
    EXPECT_EQ(json_body["error"]["code"], "COLLECTOR_FAILURE");
    EXPECT_FALSE(json_body["error"]["message"].get<std::string>().empty());
    EXPECT_TRUE(json_body["error"]["details"].is_array());
    ASSERT_EQ(json_body["error"]["details"].size(), 1U);
    EXPECT_EQ(json_body["error"]["details"][0]["collector"], "cpu_collector");
    EXPECT_EQ(json_body["error"]["details"][0]["target_file"], "/proc/stat");

    nodepulse::controllers::CpuController::set_cpu_service(nullptr);
}

TEST_F(HttpIntegrationTest, MemoryEndpointReturns200AndValidSchema) {
    auto resp = send_auth_get(kTestHost, kTestPort, "/api/v1/memory");
    EXPECT_EQ(resp.status_code, 200);

    EXPECT_TRUE(resp.has_header("content-type"));
    EXPECT_NE(resp.get_header("content-type").find("application/json"), std::string::npos);

    auto json_body = nlohmann::json::parse(resp.body);

    // Verify all required memory telemetry fields
    ASSERT_TRUE(json_body.contains("total_bytes"));
    EXPECT_TRUE(json_body["total_bytes"].is_number_unsigned());
    EXPECT_GT(json_body["total_bytes"].get<uint64_t>(), 0ULL);

    ASSERT_TRUE(json_body.contains("used_bytes"));
    EXPECT_TRUE(json_body["used_bytes"].is_number_unsigned());
    EXPECT_LE(json_body["used_bytes"].get<uint64_t>(), json_body["total_bytes"].get<uint64_t>());

    ASSERT_TRUE(json_body.contains("free_bytes"));
    EXPECT_TRUE(json_body["free_bytes"].is_number_unsigned());
    EXPECT_LE(json_body["free_bytes"].get<uint64_t>(), json_body["total_bytes"].get<uint64_t>());

    ASSERT_TRUE(json_body.contains("available_bytes"));
    EXPECT_TRUE(json_body["available_bytes"].is_number_unsigned());
    EXPECT_LE(json_body["available_bytes"].get<uint64_t>(),
              json_body["total_bytes"].get<uint64_t>());

    ASSERT_TRUE(json_body.contains("buffers_bytes"));
    EXPECT_TRUE(json_body["buffers_bytes"].is_number_unsigned());

    ASSERT_TRUE(json_body.contains("cached_bytes"));
    EXPECT_TRUE(json_body["cached_bytes"].is_number_unsigned());

    ASSERT_TRUE(json_body.contains("usage_percent"));
    EXPECT_TRUE(json_body["usage_percent"].is_number());
    EXPECT_GE(json_body["usage_percent"].get<double>(), 0.0);
    EXPECT_LE(json_body["usage_percent"].get<double>(), 100.0);

    ASSERT_TRUE(json_body.contains("swap_total_bytes"));
    EXPECT_TRUE(json_body["swap_total_bytes"].is_number_unsigned());

    ASSERT_TRUE(json_body.contains("swap_free_bytes"));
    EXPECT_TRUE(json_body["swap_free_bytes"].is_number_unsigned());

    ASSERT_TRUE(json_body.contains("swap_used_bytes"));
    EXPECT_TRUE(json_body["swap_used_bytes"].is_number_unsigned());

    ASSERT_TRUE(json_body.contains("swap_usage_percent"));
    EXPECT_TRUE(json_body["swap_usage_percent"].is_number());
    EXPECT_GE(json_body["swap_usage_percent"].get<double>(), 0.0);
    EXPECT_LE(json_body["swap_usage_percent"].get<double>(), 100.0);
}

TEST_F(HttpIntegrationTest, MemoryEndpointHandlesCollectorFailureGracefully) {
    auto failing_collector =
        std::make_shared<nodepulse::collectors::MemoryCollector>("/nonexistent/proc/meminfo");
    auto failing_service = std::make_shared<nodepulse::services::MemoryService>(failing_collector);
    nodepulse::controllers::MemoryController::set_memory_service(failing_service);

    auto resp = send_auth_get(kTestHost, kTestPort, "/api/v1/memory");
    EXPECT_EQ(resp.status_code, 500);

    EXPECT_TRUE(resp.has_header("content-type"));
    EXPECT_NE(resp.get_header("content-type").find("application/json"), std::string::npos);

    auto json_body = nlohmann::json::parse(resp.body);
    ASSERT_TRUE(json_body.contains("error"));
    EXPECT_EQ(json_body["error"]["code"], "COLLECTOR_FAILURE");
    EXPECT_FALSE(json_body["error"]["message"].get<std::string>().empty());
    EXPECT_TRUE(json_body["error"]["details"].is_array());
    ASSERT_EQ(json_body["error"]["details"].size(), 1U);
    EXPECT_EQ(json_body["error"]["details"][0]["collector"], "memory_collector");
    EXPECT_EQ(json_body["error"]["details"][0]["target_file"], "/proc/meminfo");

    nodepulse::controllers::MemoryController::set_memory_service(nullptr);
}

TEST_F(HttpIntegrationTest, DisksEndpointReturns200AndValidSchema) {
    auto resp = send_auth_get(kTestHost, kTestPort, "/api/v1/disks");
    EXPECT_EQ(resp.status_code, 200);

    EXPECT_TRUE(resp.has_header("content-type"));
    EXPECT_NE(resp.get_header("content-type").find("application/json"), std::string::npos);

    auto json_body = nlohmann::json::parse(resp.body);
    ASSERT_TRUE(json_body.is_array());
    EXPECT_GE(json_body.size(), 1U);

    bool found_root = false;
    for (const auto& partition : json_body) {
        ASSERT_TRUE(partition.contains("filesystem"));
        EXPECT_TRUE(partition["filesystem"].is_string());
        EXPECT_FALSE(partition["filesystem"].get<std::string>().empty());

        ASSERT_TRUE(partition.contains("mount_point"));
        EXPECT_TRUE(partition["mount_point"].is_string());
        EXPECT_FALSE(partition["mount_point"].get<std::string>().empty());

        ASSERT_TRUE(partition.contains("fstype"));
        EXPECT_TRUE(partition["fstype"].is_string());
        EXPECT_FALSE(partition["fstype"].get<std::string>().empty());

        ASSERT_TRUE(partition.contains("total_bytes"));
        EXPECT_TRUE(partition["total_bytes"].is_number_unsigned());

        ASSERT_TRUE(partition.contains("used_bytes"));
        EXPECT_TRUE(partition["used_bytes"].is_number_unsigned());
        EXPECT_LE(partition["used_bytes"].get<uint64_t>(),
                  partition["total_bytes"].get<uint64_t>());

        ASSERT_TRUE(partition.contains("free_bytes"));
        EXPECT_TRUE(partition["free_bytes"].is_number_unsigned());
        EXPECT_LE(partition["free_bytes"].get<uint64_t>(),
                  partition["total_bytes"].get<uint64_t>());

        ASSERT_TRUE(partition.contains("available_bytes"));
        EXPECT_TRUE(partition["available_bytes"].is_number_unsigned());
        EXPECT_LE(partition["available_bytes"].get<uint64_t>(),
                  partition["free_bytes"].get<uint64_t>());

        ASSERT_TRUE(partition.contains("usage_percent"));
        EXPECT_TRUE(partition["usage_percent"].is_number());
        EXPECT_GE(partition["usage_percent"].get<double>(), 0.0);
        EXPECT_LE(partition["usage_percent"].get<double>(), 100.0);

        ASSERT_TRUE(partition.contains("inodes_total"));
        EXPECT_TRUE(partition["inodes_total"].is_number_unsigned());

        ASSERT_TRUE(partition.contains("inodes_free"));
        EXPECT_TRUE(partition["inodes_free"].is_number_unsigned());
        EXPECT_LE(partition["inodes_free"].get<uint64_t>(),
                  partition["inodes_total"].get<uint64_t>());

        if (partition["mount_point"].get<std::string>() == "/") {
            found_root = true;
            EXPECT_GT(partition["total_bytes"].get<uint64_t>(), 0ULL);
        }
    }
    EXPECT_TRUE(found_root);
}

TEST_F(HttpIntegrationTest, DisksEndpointHandlesCollectorFailureGracefully) {
    auto failing_collector =
        std::make_shared<nodepulse::collectors::DiskCollector>("/nonexistent/proc/mounts");
    auto failing_service = std::make_shared<nodepulse::services::DiskService>(failing_collector);
    nodepulse::controllers::DiskController::set_disk_service(failing_service);

    auto resp = send_auth_get(kTestHost, kTestPort, "/api/v1/disks");
    EXPECT_EQ(resp.status_code, 500);

    EXPECT_TRUE(resp.has_header("content-type"));
    EXPECT_NE(resp.get_header("content-type").find("application/json"), std::string::npos);

    auto json_body = nlohmann::json::parse(resp.body);
    ASSERT_TRUE(json_body.contains("error"));
    EXPECT_EQ(json_body["error"]["code"], "COLLECTOR_FAILURE");
    EXPECT_FALSE(json_body["error"]["message"].get<std::string>().empty());
    EXPECT_TRUE(json_body["error"]["details"].is_array());
    ASSERT_EQ(json_body["error"]["details"].size(), 1U);
    EXPECT_EQ(json_body["error"]["details"][0]["collector"], "disk_collector");
    EXPECT_EQ(json_body["error"]["details"][0]["target_file"], "/proc/mounts");

    nodepulse::controllers::DiskController::set_disk_service(nullptr);
}

TEST_F(HttpIntegrationTest, NetworkEndpointReturns200AndValidSchema) {
    auto resp = send_auth_get(kTestHost, kTestPort, "/api/v1/network");
    EXPECT_EQ(resp.status_code, 200);

    EXPECT_TRUE(resp.has_header("content-type"));
    EXPECT_NE(resp.get_header("content-type").find("application/json"), std::string::npos);
    EXPECT_TRUE(resp.has_header("x-request-id"));
    EXPECT_FALSE(resp.get_header("x-request-id").empty());

    auto json_body = nlohmann::json::parse(resp.body);
    ASSERT_TRUE(json_body.is_array());
    EXPECT_FALSE(json_body.empty());

    bool found_lo = false;
    for (const auto& iface : json_body) {
        ASSERT_TRUE(iface.contains("name"));
        EXPECT_TRUE(iface["name"].is_string());
        EXPECT_FALSE(iface["name"].get<std::string>().empty());

        ASSERT_TRUE(iface.contains("mac_address"));
        EXPECT_TRUE(iface["mac_address"].is_string());

        ASSERT_TRUE(iface.contains("operstate"));
        EXPECT_TRUE(iface["operstate"].is_string());
        EXPECT_FALSE(iface["operstate"].get<std::string>().empty());

        ASSERT_TRUE(iface.contains("speed_mbps"));
        EXPECT_TRUE(iface["speed_mbps"].is_number_unsigned());

        ASSERT_TRUE(iface.contains("rx_bytes"));
        EXPECT_TRUE(iface["rx_bytes"].is_number_unsigned());

        ASSERT_TRUE(iface.contains("tx_bytes"));
        EXPECT_TRUE(iface["tx_bytes"].is_number_unsigned());

        ASSERT_TRUE(iface.contains("rx_packets"));
        EXPECT_TRUE(iface["rx_packets"].is_number_unsigned());

        ASSERT_TRUE(iface.contains("tx_packets"));
        EXPECT_TRUE(iface["tx_packets"].is_number_unsigned());

        ASSERT_TRUE(iface.contains("rx_errors"));
        EXPECT_TRUE(iface["rx_errors"].is_number_unsigned());

        ASSERT_TRUE(iface.contains("tx_errors"));
        EXPECT_TRUE(iface["tx_errors"].is_number_unsigned());

        ASSERT_TRUE(iface.contains("rx_bytes_per_sec"));
        EXPECT_TRUE(iface["rx_bytes_per_sec"].is_null() || iface["rx_bytes_per_sec"].is_number());

        ASSERT_TRUE(iface.contains("tx_bytes_per_sec"));
        EXPECT_TRUE(iface["tx_bytes_per_sec"].is_null() || iface["tx_bytes_per_sec"].is_number());

        if (iface["name"].get<std::string>() == "lo") {
            found_lo = true;
        }
    }
    EXPECT_TRUE(found_lo);
}

TEST_F(HttpIntegrationTest, NetworkEndpointHandlesCollectorFailureGracefully) {
    auto failing_collector = std::make_shared<nodepulse::collectors::NetworkCollector>(
        "/nonexistent/proc/net_dev", "/nonexistent/sys/class/net");
    auto failing_service = std::make_shared<nodepulse::services::NetworkService>(failing_collector);
    nodepulse::controllers::NetworkController::set_network_service(failing_service);

    auto resp = send_auth_get(kTestHost, kTestPort, "/api/v1/network");
    EXPECT_EQ(resp.status_code, 500);

    EXPECT_TRUE(resp.has_header("content-type"));
    EXPECT_NE(resp.get_header("content-type").find("application/json"), std::string::npos);

    auto json_body = nlohmann::json::parse(resp.body);
    ASSERT_TRUE(json_body.contains("error"));
    EXPECT_EQ(json_body["error"]["code"], "COLLECTOR_FAILURE");
    EXPECT_FALSE(json_body["error"]["message"].get<std::string>().empty());
    EXPECT_TRUE(json_body["error"]["details"].is_array());
    ASSERT_EQ(json_body["error"]["details"].size(), 1U);
    EXPECT_EQ(json_body["error"]["details"][0]["collector"], "network_collector");
    EXPECT_EQ(json_body["error"]["details"][0]["target_file"], "/proc/net/dev");

    nodepulse::controllers::NetworkController::set_network_service(nullptr);
}

TEST_F(HttpIntegrationTest, ProcessesEndpointReturnsExpectedSchema) {
    auto resp = send_auth_get(kTestHost, kTestPort, "/api/v1/processes");
    EXPECT_EQ(resp.status_code, 200);
    EXPECT_EQ(resp.count_header("content-type"), 1U);
    EXPECT_NE(resp.get_header("content-type").find("application/json"), std::string::npos);

    auto json_body = nlohmann::json::parse(resp.body);
    ASSERT_TRUE(json_body.is_array());
    EXPECT_FALSE(json_body.empty());

    for (const auto& proc : json_body) {
        ASSERT_TRUE(proc.contains("pid"));
        EXPECT_TRUE(proc["pid"].is_number_integer());
        EXPECT_GE(proc["pid"].get<int32_t>(), 1);

        ASSERT_TRUE(proc.contains("name"));
        EXPECT_TRUE(proc["name"].is_string());

        ASSERT_TRUE(proc.contains("user"));
        EXPECT_TRUE(proc["user"].is_string());

        ASSERT_TRUE(proc.contains("state"));
        EXPECT_TRUE(proc["state"].is_string());

        ASSERT_TRUE(proc.contains("cpu_percent"));
        EXPECT_TRUE(proc["cpu_percent"].is_number());

        ASSERT_TRUE(proc.contains("memory_rss_bytes"));
        EXPECT_TRUE(proc["memory_rss_bytes"].is_number_unsigned());

        ASSERT_TRUE(proc.contains("cmdline"));
        EXPECT_TRUE(proc["cmdline"].is_string());
    }
}

TEST_F(HttpIntegrationTest, ProcessesEndpointQueryParametersValidation) {
    // Valid sorting and limiting
    auto resp_sorted = send_auth_get(kTestHost, kTestPort, "/api/v1/processes?sort=memory&limit=5");
    EXPECT_EQ(resp_sorted.status_code, 200);
    auto j_sorted = nlohmann::json::parse(resp_sorted.body);
    ASSERT_TRUE(j_sorted.is_array());
    EXPECT_LE(j_sorted.size(), 5U);
    if (j_sorted.size() > 1) {
        for (size_t i = 1; i < j_sorted.size(); ++i) {
            EXPECT_GE(j_sorted[i - 1]["memory_rss_bytes"].get<uint64_t>(),
                      j_sorted[i]["memory_rss_bytes"].get<uint64_t>());
        }
    }

    // Invalid sort parameter (400)
    auto resp_bad_sort = send_auth_get(kTestHost, kTestPort, "/api/v1/processes?sort=unknown");
    EXPECT_EQ(resp_bad_sort.status_code, 400);
    auto j_bad_sort = nlohmann::json::parse(resp_bad_sort.body);
    EXPECT_EQ(j_bad_sort["error"]["code"], "INVALID_REQUEST");
    EXPECT_EQ(j_bad_sort["error"]["details"][0]["field"], "sort");

    // Invalid limit parameter: out of range (400)
    auto resp_bad_limit0 = send_auth_get(kTestHost, kTestPort, "/api/v1/processes?limit=0");
    EXPECT_EQ(resp_bad_limit0.status_code, 400);
    auto j_bad_limit0 = nlohmann::json::parse(resp_bad_limit0.body);
    EXPECT_EQ(j_bad_limit0["error"]["code"], "INVALID_REQUEST");
    EXPECT_EQ(j_bad_limit0["error"]["details"][0]["field"], "limit");

    auto resp_bad_limit250 = send_auth_get(kTestHost, kTestPort, "/api/v1/processes?limit=250");
    EXPECT_EQ(resp_bad_limit250.status_code, 400);

    // Invalid limit parameter: non-numeric (400)
    auto resp_bad_limit_str = send_auth_get(kTestHost, kTestPort, "/api/v1/processes?limit=abc");
    EXPECT_EQ(resp_bad_limit_str.status_code, 400);
}

TEST_F(HttpIntegrationTest, ProcessDetailEndpointReturnsExpectedSchema) {
    pid_t my_pid = getpid();
    auto resp = send_auth_get(kTestHost, kTestPort, "/api/v1/processes/" + std::to_string(my_pid));
    EXPECT_EQ(resp.status_code, 200);
    EXPECT_EQ(resp.count_header("content-type"), 1U);
    EXPECT_NE(resp.get_header("content-type").find("application/json"), std::string::npos);

    auto j = nlohmann::json::parse(resp.body);
    EXPECT_EQ(j["pid"].get<int32_t>(), static_cast<int32_t>(my_pid));
    EXPECT_GE(j["ppid"].get<int32_t>(), 0);
    EXPECT_FALSE(j["name"].get<std::string>().empty());
    EXPECT_FALSE(j["user"].get<std::string>().empty());
    EXPECT_FALSE(j["state"].get<std::string>().empty());
    EXPECT_TRUE(j["cpu_percent"].is_number());
    EXPECT_TRUE(j["memory_rss_bytes"].is_number_unsigned());
    EXPECT_TRUE(j["memory_vms_bytes"].is_number_unsigned());
    EXPECT_GE(j["thread_count"].get<uint32_t>(), 1U);
    EXPECT_GE(j["open_fd_count"].get<uint32_t>(), 1U);
    EXPECT_GT(j["start_time_epoch"].get<uint64_t>(), 0U);
    EXPECT_TRUE(j["cmdline"].is_string());
    EXPECT_FALSE(j["working_directory"].get<std::string>().empty());
}

TEST_F(HttpIntegrationTest, ProcessDetailEndpointErrorValidation) {
    // Negative PID (400)
    auto resp_neg = send_auth_get(kTestHost, kTestPort, "/api/v1/processes/-5");
    EXPECT_EQ(resp_neg.status_code, 400);
    auto j_neg = nlohmann::json::parse(resp_neg.body);
    EXPECT_EQ(j_neg["error"]["code"], "INVALID_REQUEST");
    EXPECT_EQ(j_neg["error"]["details"][0]["field"], "pid");

    // Zero PID (400)
    auto resp_zero = send_auth_get(kTestHost, kTestPort, "/api/v1/processes/0");
    EXPECT_EQ(resp_zero.status_code, 400);

    // Non-numeric PID (400)
    auto resp_str = send_auth_get(kTestHost, kTestPort, "/api/v1/processes/notanumber");
    EXPECT_EQ(resp_str.status_code, 400);

    // PID exceeding pid_max (400)
    auto resp_overflow = send_auth_get(kTestHost, kTestPort, "/api/v1/processes/99999999");
    EXPECT_EQ(resp_overflow.status_code, 400);

    // Non-existent PID within pid_max (404)
    auto resp_404 = send_auth_get(kTestHost, kTestPort, "/api/v1/processes/999999");
    EXPECT_EQ(resp_404.status_code, 404);
    auto j_404 = nlohmann::json::parse(resp_404.body);
    EXPECT_EQ(j_404["error"]["code"], "RESOURCE_NOT_FOUND");
    EXPECT_EQ(j_404["error"]["details"][0]["resource_type"], "process");
    EXPECT_EQ(j_404["error"]["details"][0]["identifier"], "999999");
}

TEST_F(HttpIntegrationTest, ServicesEndpointReturnsExpectedSchema) {
    auto collector = std::make_shared<nodepulse::collectors::ServiceCollector>();
    collector->set_custom_providers(
        [](const std::string& filter)
            -> std::optional<std::vector<nodepulse::domain::ServiceInfo>> {
            std::vector<nodepulse::domain::ServiceInfo> list = {
                {"nodepulse.service", "NodePulse Host Monitoring Agent", "loaded", "active",
                 "running", "enabled"},
                {"ssh.service", "OpenSSH Server", "loaded", "active", "running", "enabled"},
                {"cron.service", "Cron Daemon", "loaded", "inactive", "dead", "enabled"}};
            if (filter == "all") {
                return list;
            }
            std::vector<nodepulse::domain::ServiceInfo> filtered;
            for (const auto& s : list) {
                if (s.active_state == filter) {
                    filtered.push_back(s);
                }
            }
            return filtered;
        },
        nullptr);

    auto service = std::make_shared<nodepulse::services::ServiceManagerService>(collector);
    nodepulse::controllers::ServiceController::set_service_manager_service(service);

    auto resp = send_auth_get(kTestHost, kTestPort, "/api/v1/services");
    EXPECT_EQ(resp.status_code, 200);
    EXPECT_EQ(resp.count_header("content-type"), 1U);
    EXPECT_NE(resp.get_header("content-type").find("application/json"), std::string::npos);

    auto json_body = nlohmann::json::parse(resp.body);
    ASSERT_TRUE(json_body.is_array());
    EXPECT_EQ(json_body.size(), 3U);
    EXPECT_EQ(json_body[0]["name"], "nodepulse.service");
    EXPECT_EQ(json_body[0]["load_state"], "loaded");
    EXPECT_EQ(json_body[0]["active_state"], "active");
    EXPECT_EQ(json_body[0]["sub_state"], "running");
    EXPECT_EQ(json_body[0]["unit_file_state"], "enabled");

    // Filter by state=active
    auto resp_active = send_auth_get(kTestHost, kTestPort, "/api/v1/services?state=active");
    EXPECT_EQ(resp_active.status_code, 200);
    auto j_active = nlohmann::json::parse(resp_active.body);
    EXPECT_EQ(j_active.size(), 2U);

    nodepulse::controllers::ServiceController::set_service_manager_service(nullptr);
}

TEST_F(HttpIntegrationTest, ServicesEndpointQueryParametersValidation) {
    // Invalid state filter (400)
    auto resp_bad_state = send_auth_get(kTestHost, kTestPort, "/api/v1/services?state=broken");
    EXPECT_EQ(resp_bad_state.status_code, 400);
    auto j_bad_state = nlohmann::json::parse(resp_bad_state.body);
    EXPECT_EQ(j_bad_state["error"]["code"], "INVALID_REQUEST");
    EXPECT_EQ(j_bad_state["error"]["details"][0]["field"], "state");

    // Invalid limit parameter (400)
    auto resp_bad_limit = send_auth_get(kTestHost, kTestPort, "/api/v1/services?limit=-1");
    EXPECT_EQ(resp_bad_limit.status_code, 400);
    auto j_bad_limit = nlohmann::json::parse(resp_bad_limit.body);
    EXPECT_EQ(j_bad_limit["error"]["code"], "INVALID_REQUEST");
    EXPECT_EQ(j_bad_limit["error"]["details"][0]["field"], "limit");

    // Failure to communicate with D-Bus (500)
    auto failing_collector = std::make_shared<nodepulse::collectors::ServiceCollector>();
    failing_collector->set_custom_providers(
        [](const std::string&) -> std::optional<std::vector<nodepulse::domain::ServiceInfo>> {
            return std::nullopt;
        },
        nullptr);
    auto failing_service =
        std::make_shared<nodepulse::services::ServiceManagerService>(failing_collector);
    nodepulse::controllers::ServiceController::set_service_manager_service(failing_service);

    auto resp_500 = send_auth_get(kTestHost, kTestPort, "/api/v1/services");
    EXPECT_EQ(resp_500.status_code, 500);
    auto j_500 = nlohmann::json::parse(resp_500.body);
    EXPECT_EQ(j_500["error"]["code"], "COLLECTOR_FAILURE");
    EXPECT_EQ(j_500["error"]["details"][0]["collector"], "service_collector");

    nodepulse::controllers::ServiceController::set_service_manager_service(nullptr);
}

TEST_F(HttpIntegrationTest, ServiceDetailEndpointReturnsExpectedSchema) {
    auto collector = std::make_shared<nodepulse::collectors::ServiceCollector>();
    collector->set_custom_providers(
        nullptr, [](const std::string& name) -> nodepulse::collectors::ServiceDetailResult {
            if (name == "nodepulse.service") {
                nodepulse::domain::ServiceDetail d;
                d.name = "nodepulse.service";
                d.description = "NodePulse Linux Host Monitoring Agent";
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
            return {nodepulse::collectors::ServiceStatusResult::kNotFound, std::nullopt};
        });

    auto service = std::make_shared<nodepulse::services::ServiceManagerService>(collector);
    nodepulse::controllers::ServiceController::set_service_manager_service(service);

    // Exact name match
    auto resp = send_auth_get(kTestHost, kTestPort, "/api/v1/services/nodepulse.service");
    EXPECT_EQ(resp.status_code, 200);
    auto j = nlohmann::json::parse(resp.body);
    EXPECT_EQ(j["name"], "nodepulse.service");
    EXPECT_EQ(j["main_pid"], 1248);
    EXPECT_EQ(j["restart_count"], 0);
    EXPECT_EQ(j["active_enter_timestamp_utc"], 1728211200ULL);
    EXPECT_EQ(j["memory_current_bytes"], 34500000ULL);

    // Auto-append .service extension
    auto resp_short = send_auth_get(kTestHost, kTestPort, "/api/v1/services/nodepulse");
    EXPECT_EQ(resp_short.status_code, 200);
    auto j_short = nlohmann::json::parse(resp_short.body);
    EXPECT_EQ(j_short["name"], "nodepulse.service");

    nodepulse::controllers::ServiceController::set_service_manager_service(nullptr);
}

TEST_F(HttpIntegrationTest, ServiceDetailEndpointErrorValidation) {
    auto collector = std::make_shared<nodepulse::collectors::ServiceCollector>();
    collector->set_custom_providers(
        nullptr, [](const std::string& name) -> nodepulse::collectors::ServiceDetailResult {
            if (name == "broken.service") {
                return {nodepulse::collectors::ServiceStatusResult::kCollectorFailure,
                        std::nullopt};
            }
            return {nodepulse::collectors::ServiceStatusResult::kNotFound, std::nullopt};
        });

    auto service = std::make_shared<nodepulse::services::ServiceManagerService>(collector);
    nodepulse::controllers::ServiceController::set_service_manager_service(service);

    // Invalid service name character (400)
    auto resp_bad = send_auth_get(kTestHost, kTestPort, "/api/v1/services/invalid;reboot");
    EXPECT_EQ(resp_bad.status_code, 400);
    auto j_bad = nlohmann::json::parse(resp_bad.body);
    EXPECT_EQ(j_bad["error"]["code"], "INVALID_REQUEST");
    EXPECT_EQ(j_bad["error"]["details"][0]["field"], "name");

    // Service not found (404)
    auto resp_404 = send_auth_get(kTestHost, kTestPort, "/api/v1/services/nonexistent.service");
    EXPECT_EQ(resp_404.status_code, 404);
    auto j_404 = nlohmann::json::parse(resp_404.body);
    EXPECT_EQ(j_404["error"]["code"], "RESOURCE_NOT_FOUND");
    EXPECT_EQ(j_404["error"]["details"][0]["resource_type"], "service");
    EXPECT_EQ(j_404["error"]["details"][0]["identifier"], "nonexistent.service");

    // Collector failure (500)
    auto resp_500 = send_auth_get(kTestHost, kTestPort, "/api/v1/services/broken.service");
    EXPECT_EQ(resp_500.status_code, 500);
    auto j_500 = nlohmann::json::parse(resp_500.body);
    EXPECT_EQ(j_500["error"]["code"], "COLLECTOR_FAILURE");
    EXPECT_EQ(j_500["error"]["details"][0]["collector"], "service_collector");

    nodepulse::controllers::ServiceController::set_service_manager_service(nullptr);
}

TEST_F(HttpIntegrationTest, ConcurrentAllEndpointsRequests) {
    constexpr int kNumThreads = 24;
    constexpr int kRequestsPerThread = 5;

    std::vector<std::future<bool>> futures;
    for (int t = 0; t < kNumThreads; ++t) {
        futures.push_back(std::async(std::launch::async, [t]() {
            for (int r = 0; r < kRequestsPerThread; ++r) {
                std::string path;
                if (t % 8 == 0) {
                    path = "/api/v1/health";
                } else if (t % 8 == 1) {
                    path = "/api/v1/system";
                } else if (t % 8 == 2) {
                    path = "/api/v1/cpu";
                } else if (t % 8 == 3) {
                    path = "/api/v1/memory";
                } else if (t % 8 == 4) {
                    path = "/api/v1/disks";
                } else if (t % 8 == 5) {
                    path = "/api/v1/network";
                } else if (t % 8 == 6) {
                    path = "/api/v1/processes";
                } else {
                    path = "/api/v1/services";
                }
                auto resp = send_auth_get(kTestHost, kTestPort, path);
                if (resp.status_code != 200 && resp.status_code != 500) {
                    return false;
                }
                auto j = nlohmann::json::parse(resp.body);
                if (path == "/api/v1/system") {
                    if (!j.contains("hostname") || !j.contains("uptime_seconds")) {
                        return false;
                    }
                } else if (path == "/api/v1/cpu") {
                    if (!j.contains("usage_percent") || !j.contains("cores")) {
                        return false;
                    }
                } else if (path == "/api/v1/memory") {
                    if (!j.contains("total_bytes") || !j.contains("usage_percent")) {
                        return false;
                    }
                } else if (path == "/api/v1/disks") {
                    if (!j.is_array()) {
                        return false;
                    }
                } else if (path == "/api/v1/network") {
                    if (!j.is_array()) {
                        return false;
                    }
                } else if (path == "/api/v1/processes") {
                    if (!j.is_array()) {
                        return false;
                    }
                } else if (path == "/api/v1/services") {
                    if (resp.status_code == 200 && !j.is_array()) {
                        return false;
                    }
                } else {
                    if (j["status"] != "healthy") {
                        return false;
                    }
                }
            }
            return true;
        }));
    }

    for (auto& f : futures) {
        EXPECT_TRUE(f.get());
    }
}

TEST_F(HttpIntegrationTest, RegressionSingleContentTypeHeaderEmission) {
    // 1. Health endpoint (HTTP 200)
    auto resp_health = send_http_get(kTestHost, kTestPort, "/api/v1/health");
    EXPECT_EQ(resp_health.status_code, 200);
    EXPECT_EQ(resp_health.count_header("content-type"), 1U);
    EXPECT_NE(resp_health.get_header("content-type").find("application/json"), std::string::npos);

    // 2. System endpoint (HTTP 200)
    auto resp_system = send_auth_get(kTestHost, kTestPort, "/api/v1/system");
    EXPECT_EQ(resp_system.status_code, 200);
    EXPECT_EQ(resp_system.count_header("content-type"), 1U);
    EXPECT_NE(resp_system.get_header("content-type").find("application/json"), std::string::npos);

    // 3. Error response (HTTP 404)
    auto resp_404 = send_http_get(kTestHost, kTestPort, "/api/v1/non_existent_route");
    EXPECT_EQ(resp_404.status_code, 404);
    EXPECT_EQ(resp_404.count_header("content-type"), 1U);
    EXPECT_NE(resp_404.get_header("content-type").find("application/json"), std::string::npos);

    // 4. System collector failure error response (HTTP 500)
    auto failing_collector = std::make_shared<nodepulse::collectors::SystemCollector>(
        "/etc/os-release", "/nonexistent/proc/uptime", "/proc/stat");
    auto failing_service = std::make_shared<nodepulse::services::SystemService>(failing_collector);
    nodepulse::controllers::SystemController::set_system_service(failing_service);

    auto resp_500 = send_auth_get(kTestHost, kTestPort, "/api/v1/system");
    EXPECT_EQ(resp_500.status_code, 500);
    EXPECT_EQ(resp_500.count_header("content-type"), 1U);
    EXPECT_NE(resp_500.get_header("content-type").find("application/json"), std::string::npos);

    nodepulse::controllers::SystemController::set_system_service(nullptr);

    // 5. CPU endpoint (HTTP 200)
    auto resp_cpu = send_auth_get(kTestHost, kTestPort, "/api/v1/cpu");
    EXPECT_EQ(resp_cpu.status_code, 200);
    EXPECT_EQ(resp_cpu.count_header("content-type"), 1U);
    EXPECT_NE(resp_cpu.get_header("content-type").find("application/json"), std::string::npos);

    // 6. CPU collector failure error response (HTTP 500)
    auto failing_cpu_collector = std::make_shared<nodepulse::collectors::CpuCollector>(
        "/nonexistent/proc/stat", "/proc/loadavg", "/proc/cpuinfo");
    auto failing_cpu_service =
        std::make_shared<nodepulse::services::CpuService>(failing_cpu_collector);
    nodepulse::controllers::CpuController::set_cpu_service(failing_cpu_service);

    auto resp_cpu_500 = send_auth_get(kTestHost, kTestPort, "/api/v1/cpu");
    EXPECT_EQ(resp_cpu_500.status_code, 500);
    EXPECT_EQ(resp_cpu_500.count_header("content-type"), 1U);
    EXPECT_NE(resp_cpu_500.get_header("content-type").find("application/json"), std::string::npos);

    nodepulse::controllers::CpuController::set_cpu_service(nullptr);

    // 7. Memory endpoint (HTTP 200)
    auto resp_mem = send_auth_get(kTestHost, kTestPort, "/api/v1/memory");
    EXPECT_EQ(resp_mem.status_code, 200);
    EXPECT_EQ(resp_mem.count_header("content-type"), 1U);
    EXPECT_NE(resp_mem.get_header("content-type").find("application/json"), std::string::npos);

    // 8. Memory collector failure error response (HTTP 500)
    auto failing_mem_collector =
        std::make_shared<nodepulse::collectors::MemoryCollector>("/nonexistent/proc/meminfo");
    auto failing_mem_service =
        std::make_shared<nodepulse::services::MemoryService>(failing_mem_collector);
    nodepulse::controllers::MemoryController::set_memory_service(failing_mem_service);

    auto resp_mem_500 = send_auth_get(kTestHost, kTestPort, "/api/v1/memory");
    EXPECT_EQ(resp_mem_500.status_code, 500);
    EXPECT_EQ(resp_mem_500.count_header("content-type"), 1U);
    EXPECT_NE(resp_mem_500.get_header("content-type").find("application/json"), std::string::npos);

    nodepulse::controllers::MemoryController::set_memory_service(nullptr);

    // 9. Disks endpoint (HTTP 200)
    auto resp_disks = send_auth_get(kTestHost, kTestPort, "/api/v1/disks");
    EXPECT_EQ(resp_disks.status_code, 200);
    EXPECT_EQ(resp_disks.count_header("content-type"), 1U);
    EXPECT_NE(resp_disks.get_header("content-type").find("application/json"), std::string::npos);

    // 10. Disks collector failure error response (HTTP 500)
    auto failing_disks_collector =
        std::make_shared<nodepulse::collectors::DiskCollector>("/nonexistent/proc/mounts");
    auto failing_disks_service =
        std::make_shared<nodepulse::services::DiskService>(failing_disks_collector);
    nodepulse::controllers::DiskController::set_disk_service(failing_disks_service);

    auto resp_disks_500 = send_auth_get(kTestHost, kTestPort, "/api/v1/disks");
    EXPECT_EQ(resp_disks_500.status_code, 500);
    EXPECT_EQ(resp_disks_500.count_header("content-type"), 1U);
    EXPECT_NE(resp_disks_500.get_header("content-type").find("application/json"),
              std::string::npos);

    nodepulse::controllers::DiskController::set_disk_service(nullptr);

    // 11. Network endpoint (HTTP 200)
    auto resp_net = send_auth_get(kTestHost, kTestPort, "/api/v1/network");
    EXPECT_EQ(resp_net.status_code, 200);
    EXPECT_EQ(resp_net.count_header("content-type"), 1U);
    EXPECT_NE(resp_net.get_header("content-type").find("application/json"), std::string::npos);

    // 12. Network collector failure error response (HTTP 500)
    auto failing_net_collector = std::make_shared<nodepulse::collectors::NetworkCollector>(
        "/nonexistent/proc/net_dev", "/nonexistent/sys/class/net");
    auto failing_net_service =
        std::make_shared<nodepulse::services::NetworkService>(failing_net_collector);
    nodepulse::controllers::NetworkController::set_network_service(failing_net_service);

    auto resp_net_500 = send_auth_get(kTestHost, kTestPort, "/api/v1/network");
    EXPECT_EQ(resp_net_500.status_code, 500);
    EXPECT_EQ(resp_net_500.count_header("content-type"), 1U);
    EXPECT_NE(resp_net_500.get_header("content-type").find("application/json"), std::string::npos);

    nodepulse::controllers::NetworkController::set_network_service(nullptr);

    // 13. Processes endpoint (HTTP 200)
    auto resp_procs = send_auth_get(kTestHost, kTestPort, "/api/v1/processes");
    EXPECT_EQ(resp_procs.status_code, 200);
    EXPECT_EQ(resp_procs.count_header("content-type"), 1U);
    EXPECT_NE(resp_procs.get_header("content-type").find("application/json"), std::string::npos);

    // 14. Processes 400 error
    auto resp_procs_400 = send_auth_get(kTestHost, kTestPort, "/api/v1/processes?sort=bad");
    EXPECT_EQ(resp_procs_400.status_code, 400);
    EXPECT_EQ(resp_procs_400.count_header("content-type"), 1U);
    EXPECT_NE(resp_procs_400.get_header("content-type").find("application/json"),
              std::string::npos);

    // 15. Process detail 404 error
    auto resp_proc_404 = send_auth_get(kTestHost, kTestPort, "/api/v1/processes/999999");
    EXPECT_EQ(resp_proc_404.status_code, 404);
    EXPECT_EQ(resp_proc_404.count_header("content-type"), 1U);
    EXPECT_NE(resp_proc_404.get_header("content-type").find("application/json"), std::string::npos);

    // 16. Services endpoint (HTTP 200)
    auto mock_service_collector = std::make_shared<nodepulse::collectors::ServiceCollector>();
    mock_service_collector->set_custom_providers(
        [](const std::string&) -> std::optional<std::vector<nodepulse::domain::ServiceInfo>> {
            return std::vector<nodepulse::domain::ServiceInfo>{
                {"nodepulse.service", "Desc", "loaded", "active", "running", "enabled"}};
        },
        nullptr);
    auto mock_service_mgr =
        std::make_shared<nodepulse::services::ServiceManagerService>(mock_service_collector);
    nodepulse::controllers::ServiceController::set_service_manager_service(mock_service_mgr);

    auto resp_svcs = send_auth_get(kTestHost, kTestPort, "/api/v1/services");
    EXPECT_EQ(resp_svcs.status_code, 200);
    EXPECT_EQ(resp_svcs.count_header("content-type"), 1U);
    EXPECT_NE(resp_svcs.get_header("content-type").find("application/json"), std::string::npos);

    // 17. Services 400 error
    auto resp_svcs_400 = send_auth_get(kTestHost, kTestPort, "/api/v1/services?state=bad");
    EXPECT_EQ(resp_svcs_400.status_code, 400);
    EXPECT_EQ(resp_svcs_400.count_header("content-type"), 1U);
    EXPECT_NE(resp_svcs_400.get_header("content-type").find("application/json"), std::string::npos);

    // 18. Services 500 error
    mock_service_collector->set_custom_providers(
        [](const std::string&) -> std::optional<std::vector<nodepulse::domain::ServiceInfo>> {
            return std::nullopt;
        },
        nullptr);
    auto resp_svcs_500 = send_auth_get(kTestHost, kTestPort, "/api/v1/services");
    EXPECT_EQ(resp_svcs_500.status_code, 500);
    EXPECT_EQ(resp_svcs_500.count_header("content-type"), 1U);
    EXPECT_NE(resp_svcs_500.get_header("content-type").find("application/json"), std::string::npos);

    nodepulse::controllers::ServiceController::set_service_manager_service(nullptr);
}

TEST_F(HttpIntegrationTest, HealthEndpointExemptFromAuthentication) {
    // 1. Without any auth header
    auto resp_no_auth = send_http_get(kTestHost, kTestPort, "/api/v1/health");
    EXPECT_EQ(resp_no_auth.status_code, 200);

    // 2. With valid auth header
    auto resp_valid_auth = send_auth_get(kTestHost, kTestPort, "/api/v1/health");
    EXPECT_EQ(resp_valid_auth.status_code, 200);

    // 3. With invalid auth header (still 200 because health is exempt per BR-001)
    std::unordered_map<std::string, std::string> invalid_header = {{"X-API-Key", "invalid_secret"}};
    auto resp_invalid_auth = send_http_get(kTestHost, kTestPort, "/api/v1/health", invalid_header);
    EXPECT_EQ(resp_invalid_auth.status_code, 200);
}

TEST_F(HttpIntegrationTest, AllOperationalEndpointsRejectMissingApiKey) {
    const std::vector<std::string> operational_paths = {
        "/api/v1/system",      "/api/v1/cpu",      "/api/v1/memory",
        "/api/v1/disks",       "/api/v1/network",  "/api/v1/processes",
        "/api/v1/processes/1", "/api/v1/services", "/api/v1/services/test.service"};

    for (const auto& path : operational_paths) {
        auto resp = send_http_get(kTestHost, kTestPort, path);
        EXPECT_EQ(resp.status_code, 401) << "Path failed 401 check: " << path;
        EXPECT_EQ(resp.count_header("content-type"), 1U) << "Path content-type count: " << path;
        EXPECT_NE(resp.get_header("content-type").find("application/json"), std::string::npos);
        EXPECT_TRUE(resp.has_header("x-request-id")) << "Path missing request id: " << path;
        EXPECT_FALSE(resp.get_header("x-request-id").empty());

        auto json_body = nlohmann::json::parse(resp.body);
        ASSERT_TRUE(json_body.contains("error")) << "Path missing error object: " << path;
        EXPECT_EQ(json_body["error"]["code"], "UNAUTHORIZED");
        EXPECT_FALSE(json_body["error"]["message"].get<std::string>().empty());
        EXPECT_FALSE(json_body["error"]["timestamp"].get<std::string>().empty());
        EXPECT_TRUE(json_body["error"]["details"].is_array());
        EXPECT_TRUE(json_body["error"]["details"].empty());

        // Zero secret leakage in response body
        EXPECT_EQ(resp.body.find(kValidApiKey), std::string::npos);
    }
}

TEST_F(HttpIntegrationTest, AllOperationalEndpointsRejectInvalidApiKey) {
    const std::unordered_map<std::string, std::string> invalid_header = {
        {"X-API-Key", "wrong_api_key_np_invalid"}};

    const std::vector<std::string> operational_paths = {
        "/api/v1/system",      "/api/v1/cpu",      "/api/v1/memory",
        "/api/v1/disks",       "/api/v1/network",  "/api/v1/processes",
        "/api/v1/processes/1", "/api/v1/services", "/api/v1/services/test.service"};

    for (const auto& path : operational_paths) {
        auto resp = send_http_get(kTestHost, kTestPort, path, invalid_header);
        EXPECT_EQ(resp.status_code, 401) << "Path failed invalid key check: " << path;
        auto json_body = nlohmann::json::parse(resp.body);
        ASSERT_TRUE(json_body.contains("error"));
        EXPECT_EQ(json_body["error"]["code"], "UNAUTHORIZED");
        EXPECT_EQ(resp.body.find(kValidApiKey), std::string::npos);
    }
}

TEST_F(HttpIntegrationTest, RejectsEmptyAndOversizedApiKeyHeaders) {
    // Empty key header
    std::unordered_map<std::string, std::string> empty_key_header = {{"X-API-Key", ""}};
    auto resp_empty = send_http_get(kTestHost, kTestPort, "/api/v1/system", empty_key_header);
    EXPECT_EQ(resp_empty.status_code, 401);
    auto json_empty = nlohmann::json::parse(resp_empty.body);
    EXPECT_EQ(json_empty["error"]["code"], "UNAUTHORIZED");

    // Oversized key header (10,000 characters)
    std::string oversized_key(10000, 'X');
    std::unordered_map<std::string, std::string> oversized_key_header = {
        {"X-API-Key", oversized_key}};
    auto resp_oversized =
        send_http_get(kTestHost, kTestPort, "/api/v1/system", oversized_key_header);
    EXPECT_EQ(resp_oversized.status_code, 401);
    auto json_oversized = nlohmann::json::parse(resp_oversized.body);
    EXPECT_EQ(json_oversized["error"]["code"], "UNAUTHORIZED");
}

TEST_F(HttpIntegrationTest, CaseInsensitiveApiKeyHeaderLookup) {
    std::unordered_map<std::string, std::string> lower_header = {{"x-api-key", kValidApiKey}};
    auto resp = send_http_get(kTestHost, kTestPort, "/api/v1/system", lower_header);
    EXPECT_EQ(resp.status_code, 200);
}

TEST_F(HttpIntegrationTest, UnknownRouteReturns404RatherThan401WithoutAuth) {
    const std::vector<std::string> unknown_paths = {"/api/v1/unknown_non_existent_route",
                                                    "/api/v1/healthy", "/api/v1/healthcheck",
                                                    "/api/v1/health/status"};

    for (const auto& path : unknown_paths) {
        auto resp = send_http_get(kTestHost, kTestPort, path);
        EXPECT_EQ(resp.status_code, 404) << "Path failed 404 check: " << path;
        auto json_body = nlohmann::json::parse(resp.body);
        ASSERT_TRUE(json_body.contains("error")) << "Path missing error object: " << path;
        EXPECT_EQ(json_body["error"]["code"], "RESOURCE_NOT_FOUND")
            << "Path returned unexpected error code: " << path;
    }
}

TEST_F(HttpIntegrationTest, AllNineProtectedRoutesReturnNormalResponseWithValidKey) {
    auto collector = std::make_shared<nodepulse::collectors::ServiceCollector>();
    collector->set_custom_providers(
        [](const std::string&) -> std::optional<std::vector<nodepulse::domain::ServiceInfo>> {
            return std::vector<nodepulse::domain::ServiceInfo>{
                {"nodepulse.service", "Desc", "loaded", "active", "running", "enabled"}};
        },
        [](const std::string& name) -> nodepulse::collectors::ServiceDetailResult {
            if (name == "nodepulse.service") {
                nodepulse::domain::ServiceDetail d;
                d.name = "nodepulse.service";
                d.description = "NodePulse Linux Host Monitoring Agent";
                d.load_state = "loaded";
                d.active_state = "active";
                d.sub_state = "running";
                d.unit_file_state = "enabled";
                d.main_pid = 1;
                d.restart_count = 0;
                d.active_enter_timestamp_utc = 1728211200;
                d.memory_current_bytes = 34500000;
                return {nodepulse::collectors::ServiceStatusResult::kOk, d};
            }
            return {nodepulse::collectors::ServiceStatusResult::kNotFound, std::nullopt};
        });
    auto service_mgr = std::make_shared<nodepulse::services::ServiceManagerService>(collector);
    nodepulse::controllers::ServiceController::set_service_manager_service(service_mgr);

    const std::vector<std::string> nine_protected_paths = {
        "/api/v1/system",      "/api/v1/cpu",      "/api/v1/memory",
        "/api/v1/disks",       "/api/v1/network",  "/api/v1/processes",
        "/api/v1/processes/1", "/api/v1/services", "/api/v1/services/nodepulse.service"};

    for (const auto& path : nine_protected_paths) {
        auto resp = send_auth_get(kTestHost, kTestPort, path);
        EXPECT_EQ(resp.status_code, 200) << "Path failed 200 with valid key: " << path;
        auto json_body = nlohmann::json::parse(resp.body);
        EXPECT_FALSE(json_body.is_null()) << "Path returned empty body: " << path;
        EXPECT_FALSE(json_body.contains("error")) << "Path returned error with valid key: " << path;
    }

    nodepulse::controllers::ServiceController::set_service_manager_service(nullptr);
}

TEST_F(HttpIntegrationTest, ConcurrentAuthenticatedAndUnauthenticatedRequests) {
    constexpr int kNumThreads = 12;
    constexpr int kRequestsPerThread = 5;

    std::vector<std::future<bool>> futures;
    for (int t = 0; t < kNumThreads; ++t) {
        futures.push_back(std::async(std::launch::async, [t]() {
            for (int r = 0; r < kRequestsPerThread; ++r) {
                if (t % 2 == 0) {
                    // Valid auth request -> expect 200
                    auto resp = send_auth_get(kTestHost, kTestPort, "/api/v1/system");
                    if (resp.status_code != 200) {
                        return false;
                    }
                } else {
                    // Unauthenticated request -> expect 401
                    auto resp = send_http_get(kTestHost, kTestPort, "/api/v1/system");
                    if (resp.status_code != 401) {
                        return false;
                    }
                    auto j = nlohmann::json::parse(resp.body);
                    if (j["error"]["code"] != "UNAUTHORIZED") {
                        return false;
                    }
                }
            }
            return true;
        }));
    }

    for (auto& f : futures) {
        EXPECT_TRUE(f.get());
    }
}

TEST(ServerSecurityTest, RejectsNonLoopbackHostBinding) {
    // 0.0.0.0 wildcard binding
    nodepulse::config::Config cfg_wildcard;
    cfg_wildcard.server.host = "0.0.0.0";
    nodepulse::server::Server server_wildcard(cfg_wildcard);
    EXPECT_THROW(server_wildcard.setup(), std::runtime_error);
    EXPECT_THROW(server_wildcard.run(), std::runtime_error);

    // LAN / Private IP binding
    nodepulse::config::Config cfg_lan;
    cfg_lan.server.host = "192.168.1.100";
    nodepulse::server::Server server_lan(cfg_lan);
    EXPECT_THROW(server_lan.setup(), std::runtime_error);
    EXPECT_THROW(server_lan.run(), std::runtime_error);

    // Public IP binding
    nodepulse::config::Config cfg_public;
    cfg_public.server.host = "198.51.100.1";
    nodepulse::server::Server server_public(cfg_public);
    EXPECT_THROW(server_public.setup(), std::runtime_error);
    EXPECT_THROW(server_public.run(), std::runtime_error);
}

TEST(ServerSecurityTest, RejectsEmptyApiKeyStartup) {
    nodepulse::config::Config cfg;
    cfg.server.host = "127.0.0.1";
    cfg.security.api_key = "";
    nodepulse::server::Server server(cfg);
    EXPECT_THROW(server.setup(), std::runtime_error);
    EXPECT_THROW(server.run(), std::runtime_error);
}
