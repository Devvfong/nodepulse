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

#include <nodepulse/collectors/system_collector.hpp>
#include <nodepulse/config/config.hpp>
#include <nodepulse/controllers/system_controller.hpp>
#include <nodepulse/server/server.hpp>
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
    auto resp = send_http_get(kTestHost, kTestPort, "/api/v1/system");
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

    auto resp = send_http_get(kTestHost, kTestPort, "/api/v1/system", headers);
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
    resp = send_http_get(kTestHost, kTestPort, "/api/v1/system", headers);
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

    auto resp = send_http_get(kTestHost, kTestPort, "/api/v1/system");
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
                auto resp = send_http_get(kTestHost, kTestPort, path);
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

TEST_F(HttpIntegrationTest, RegressionSingleContentTypeHeaderEmission) {
    // 1. Health endpoint (HTTP 200)
    auto resp_health = send_http_get(kTestHost, kTestPort, "/api/v1/health");
    EXPECT_EQ(resp_health.status_code, 200);
    EXPECT_EQ(resp_health.count_header("content-type"), 1U);
    EXPECT_NE(resp_health.get_header("content-type").find("application/json"), std::string::npos);

    // 2. System endpoint (HTTP 200)
    auto resp_system = send_http_get(kTestHost, kTestPort, "/api/v1/system");
    EXPECT_EQ(resp_system.status_code, 200);
    EXPECT_EQ(resp_system.count_header("content-type"), 1U);
    EXPECT_NE(resp_system.get_header("content-type").find("application/json"), std::string::npos);

    // 3. Error response (HTTP 404)
    auto resp_404 = send_http_get(kTestHost, kTestPort, "/api/v1/non_existent_route");
    EXPECT_EQ(resp_404.status_code, 404);
    EXPECT_EQ(resp_404.count_header("content-type"), 1U);
    EXPECT_NE(resp_404.get_header("content-type").find("application/json"), std::string::npos);

    // 4. Collector failure error response (HTTP 500)
    auto failing_collector = std::make_shared<nodepulse::collectors::SystemCollector>(
        "/etc/os-release", "/nonexistent/proc/uptime", "/proc/stat");
    auto failing_service = std::make_shared<nodepulse::services::SystemService>(failing_collector);
    nodepulse::controllers::SystemController::set_system_service(failing_service);

    auto resp_500 = send_http_get(kTestHost, kTestPort, "/api/v1/system");
    EXPECT_EQ(resp_500.status_code, 500);
    EXPECT_EQ(resp_500.count_header("content-type"), 1U);
    EXPECT_NE(resp_500.get_header("content-type").find("application/json"), std::string::npos);

    nodepulse::controllers::SystemController::set_system_service(nullptr);
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
