#include <chrono>
#include <cstdint>
#include <memory>
#include <string>
#include <thread>
#include <vector>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

#include <nodepulse/collectors/docker_collector.hpp>
#include <nodepulse/config/config.hpp>
#include <nodepulse/services/docker_service.hpp>

#include <poll.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

namespace nodepulse::collectors {

namespace {

class MockDockerTransport : public IDockerTransport {
  public:
    DockerHttpResponse next_response;
    std::string last_requested_path;
    int call_count{0};

    DockerHttpResponse get(const std::string& path) override {
        last_requested_path = path;
        call_count++;
        return next_response;
    }
};

}  // namespace

TEST(DockerCollectorTest, ContainerIdValidation) {
    EXPECT_TRUE(DockerCollector::is_valid_container_id(
        "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"));
    EXPECT_TRUE(DockerCollector::is_valid_container_id("e3b0c44298fc"));
    EXPECT_TRUE(DockerCollector::is_valid_container_id("redis-cache"));
    EXPECT_TRUE(DockerCollector::is_valid_container_id("my_app.service-1"));

    EXPECT_FALSE(DockerCollector::is_valid_container_id(""));
    EXPECT_FALSE(DockerCollector::is_valid_container_id("."));
    EXPECT_FALSE(DockerCollector::is_valid_container_id(".."));
    EXPECT_FALSE(DockerCollector::is_valid_container_id("container/with/slashes"));
    EXPECT_FALSE(DockerCollector::is_valid_container_id("container with spaces"));
    EXPECT_FALSE(DockerCollector::is_valid_container_id("container;rm -rf"));

    std::string too_long(130, 'a');
    EXPECT_FALSE(DockerCollector::is_valid_container_id(too_long));
}

TEST(DockerCollectorTest, ParseIso8601ToEpoch) {
    // 2026-10-05T13:33:29Z -> 1791207209
    uint64_t epoch = DockerCollector::parse_iso8601_to_epoch("2026-10-05T13:33:29.865825273Z");
    EXPECT_GT(epoch, 1700000000ULL);

    EXPECT_EQ(DockerCollector::parse_iso8601_to_epoch(""), 0ULL);
    EXPECT_EQ(DockerCollector::parse_iso8601_to_epoch("invalid-date-format"), 0ULL);
    EXPECT_EQ(DockerCollector::parse_iso8601_to_epoch("2026/10/05 13:33:29"), 0ULL);
}

TEST(DockerCollectorTest, CpuCalculationSemantics) {
    // Delta busy = 500, Delta system = 1000, 2 cores -> (500/1000) * 2 * 100 = 100.0%
    double cpu = DockerCollector::calculate_cpu_percent(1500, 1000, 2000, 1000, 2);
    EXPECT_DOUBLE_EQ(cpu, 100.0);

    // Single core 50%
    double cpu_single = DockerCollector::calculate_cpu_percent(1500, 1000, 2000, 1000, 1);
    EXPECT_DOUBLE_EQ(cpu_single, 50.0);

    // Zero delta (idle)
    EXPECT_DOUBLE_EQ(DockerCollector::calculate_cpu_percent(1000, 1000, 2000, 1000, 2), 0.0);

    // Zero system delta (denominator guard)
    EXPECT_DOUBLE_EQ(DockerCollector::calculate_cpu_percent(1500, 1000, 1000, 1000, 2), 0.0);

    // Counter regression / wrap
    EXPECT_DOUBLE_EQ(DockerCollector::calculate_cpu_percent(500, 1000, 2000, 1000, 2), 0.0);
    EXPECT_DOUBLE_EQ(DockerCollector::calculate_cpu_percent(1500, 1000, 500, 1000, 2), 0.0);

    // Zero online CPUs fallback to 1
    double cpu_zero_cores = DockerCollector::calculate_cpu_percent(1500, 1000, 2000, 1000, 0);
    EXPECT_DOUBLE_EQ(cpu_zero_cores, 50.0);
}

TEST(DockerCollectorTest, MemoryCalculationSemantics) {
    // Usage 500, limit 1000, no cache -> 50.0%
    EXPECT_DOUBLE_EQ(DockerCollector::calculate_memory_percent(500, 1000, 0), 50.0);

    // Usage 700, limit 1000, cache 200 -> (500/1000) * 100 = 50.0%
    EXPECT_DOUBLE_EQ(DockerCollector::calculate_memory_percent(700, 1000, 200), 50.0);

    // Zero limit (denominator guard)
    EXPECT_DOUBLE_EQ(DockerCollector::calculate_memory_percent(500, 0, 0), 0.0);

    // Cache greater than usage
    EXPECT_DOUBLE_EQ(DockerCollector::calculate_memory_percent(100, 1000, 200), 0.0);

    // Usage exceeds limit (clamped to 100.0%)
    EXPECT_DOUBLE_EQ(DockerCollector::calculate_memory_percent(1200, 1000, 0), 100.0);
}

TEST(DockerCollectorTest, ListContainersValidResponse) {
    auto mock_transport = std::make_shared<MockDockerTransport>();
    nlohmann::json mock_json = nlohmann::json::array(
        {{{"Id", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"},
          {"Names", {"/redis-cache"}},
          {"Image", "redis:7-alpine"},
          {"Status", "Up 3 days"},
          {"State", "running"},
          {"Created", 1727952000}},
         {{"Id", "a1b2c3d4e5f61c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"},
          {"Names", {"/postgres-db"}},
          {"Image", "postgres:16-alpine"},
          {"Status", "Exited (0) 2 hours ago"},
          {"State", "exited"},
          {"Created", 1727900000}}});

    mock_transport->next_response = {DockerTransportStatus::kOk, 200, mock_json.dump(), ""};

    DockerCollector collector(mock_transport);
    auto res = collector.list_containers();

    EXPECT_EQ(res.status, DockerStatusResult::kOk);
    ASSERT_TRUE(res.containers.has_value());
    ASSERT_EQ(res.containers->size(), 2);

    EXPECT_EQ(res.containers->at(0).id,
              "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    EXPECT_EQ(res.containers->at(0).names, std::vector<std::string>{"/redis-cache"});
    EXPECT_EQ(res.containers->at(0).image, "redis:7-alpine");
    EXPECT_EQ(res.containers->at(0).status, "Up 3 days");
    EXPECT_EQ(res.containers->at(0).state, "running");
    EXPECT_EQ(res.containers->at(0).created, 1727952000ULL);

    EXPECT_EQ(res.containers->at(1).id,
              "a1b2c3d4e5f61c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    EXPECT_EQ(res.containers->at(1).state, "exited");
}

TEST(DockerCollectorTest, ListContainersEmptyResponse) {
    auto mock_transport = std::make_shared<MockDockerTransport>();
    mock_transport->next_response = {DockerTransportStatus::kOk, 200, "[]", ""};

    DockerCollector collector(mock_transport);
    auto res = collector.list_containers();

    EXPECT_EQ(res.status, DockerStatusResult::kOk);
    ASSERT_TRUE(res.containers.has_value());
    EXPECT_TRUE(res.containers->empty());
}

TEST(DockerCollectorTest, GetContainerDetailValidResponse) {
    auto mock_transport = std::make_shared<MockDockerTransport>();
    nlohmann::json mock_json = {
        {"Id", "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855"},
        {"Name", "/redis-cache"},
        {"Config", {{"Image", "redis:7-alpine"}}},
        {"State", {{"Status", "running"}, {"Running", true}, {"ExitCode", 0}}},
        {"Status", "Up 3 days"},
        {"NetworkSettings",
         {{"Ports",
           {{"6379/tcp",
             nlohmann::json::array({{{"HostIp", "0.0.0.0"}, {"HostPort", "6379"}}})}}}}},
        {"Mounts", nlohmann::json::array({{{"Source", "/var/data/redis"}}})},
        {"Created", 1727952000}};

    mock_transport->next_response = {DockerTransportStatus::kOk, 200, mock_json.dump(), ""};

    DockerCollector collector(mock_transport);
    auto res = collector.get_container("e3b0c44298fc");

    EXPECT_EQ(res.status, DockerStatusResult::kOk);
    ASSERT_TRUE(res.detail.has_value());
    const auto& d = *res.detail;
    EXPECT_EQ(d.id, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    EXPECT_EQ(d.name, "/redis-cache");
    EXPECT_EQ(d.image, "redis:7-alpine");
    EXPECT_EQ(d.status, "Up 3 days");
    EXPECT_EQ(d.state, "running");
    EXPECT_TRUE(d.running);
    EXPECT_EQ(d.exit_code, 0);
    EXPECT_EQ(d.port_mappings, std::vector<std::string>{"0.0.0.0:6379->6379/tcp"});
    EXPECT_EQ(d.mount_sources, std::vector<std::string>{"/var/data/redis"});
    EXPECT_EQ(d.created, 1727952000ULL);
}

TEST(DockerCollectorTest, GetContainerDetailNotFoundReturns404Status) {
    auto mock_transport = std::make_shared<MockDockerTransport>();
    mock_transport->next_response = {DockerTransportStatus::kOk, 404,
                                     "{\"message\":\"No such container: unknown123\"}", ""};

    DockerCollector collector(mock_transport);
    auto res = collector.get_container("unknown123");

    EXPECT_EQ(res.status, DockerStatusResult::kNotFound);
    EXPECT_FALSE(res.detail.has_value());
}

TEST(DockerCollectorTest, MalformedDockerJsonReturnsCollectorFailure) {
    auto mock_transport = std::make_shared<MockDockerTransport>();
    mock_transport->next_response = {DockerTransportStatus::kOk, 200,
                                     "{\"corrupted_json\": [unclosed_array", ""};

    DockerCollector collector(mock_transport);
    auto list_res = collector.list_containers();
    EXPECT_EQ(list_res.status, DockerStatusResult::kCollectorFailure);

    auto detail_res = collector.get_container("c123");
    EXPECT_EQ(detail_res.status, DockerStatusResult::kCollectorFailure);
}

TEST(DockerCollectorTest, NonArrayOrNonObjectResponsesHandledGracefully) {
    auto mock_transport = std::make_shared<MockDockerTransport>();
    // Non-array for list
    mock_transport->next_response = {DockerTransportStatus::kOk, 200,
                                     "{\"error\":\"not an array\"}", ""};
    DockerCollector collector(mock_transport);
    auto list_res = collector.list_containers();
    EXPECT_EQ(list_res.status, DockerStatusResult::kCollectorFailure);

    // Non-object for detail
    mock_transport->next_response = {DockerTransportStatus::kOk, 200, "[\"not an object\"]", ""};
    auto detail_res = collector.get_container("c123");
    EXPECT_EQ(detail_res.status, DockerStatusResult::kCollectorFailure);
}

TEST(DockerCollectorTest, TransportErrorsMapToUnavailable) {
    auto mock_transport = std::make_shared<MockDockerTransport>();

    const std::vector<DockerTransportStatus> unavailable_statuses = {
        DockerTransportStatus::kDisabled, DockerTransportStatus::kSocketNotFound,
        DockerTransportStatus::kPermissionDenied, DockerTransportStatus::kConnectionRefused,
        DockerTransportStatus::kTimeout};

    for (auto status : unavailable_statuses) {
        mock_transport->next_response = {status, 0, "", "Failure simulation"};
        DockerCollector collector(mock_transport);

        auto list_res = collector.list_containers();
        EXPECT_EQ(list_res.status, DockerStatusResult::kUnavailable);

        auto detail_res = collector.get_container("c123");
        EXPECT_EQ(detail_res.status, DockerStatusResult::kUnavailable);
    }
}

TEST(DockerCollectorTest, ResponseTooLargeMapsToCollectorFailure) {
    auto mock_transport = std::make_shared<MockDockerTransport>();
    mock_transport->next_response = {DockerTransportStatus::kResponseTooLarge, 0, "",
                                     "Exceeded 10MB limit"};

    DockerCollector collector(mock_transport);
    auto res = collector.list_containers();
    EXPECT_EQ(res.status, DockerStatusResult::kCollectorFailure);
}

TEST(DockerCollectorTest, DockerUnixSocketTransportNonExistentSocket) {
    DockerUnixSocketTransport transport("/tmp/non_existent_nodepulse_docker_test.sock", 1000, true);
    auto resp = transport.get("/containers/json");
    EXPECT_EQ(resp.status, DockerTransportStatus::kSocketNotFound);
}

TEST(DockerCollectorTest, DockerUnixSocketTransportDisabledReturnsDisabledStatus) {
    DockerUnixSocketTransport transport("/var/run/docker.sock", 1000, false);
    auto resp = transport.get("/containers/json");
    EXPECT_EQ(resp.status, DockerTransportStatus::kDisabled);
}

TEST(DockerCollectorTest, UnixSocketChunkedAndPartialReadHandling) {
    // Test actual POSIX socket connect and chunked response handling using a socketpair
    int sv[2];
    ASSERT_EQ(::socketpair(AF_UNIX, SOCK_STREAM, 0, sv), 0);

    // Server thread sends chunked HTTP response in fragments to test partial-read handling
    std::thread server_thread([sv]() {
        std::string part1 =
            "HTTP/1.1 200 OK\r\n"
            "Content-Type: application/json\r\n"
            "Transfer-Encoding: chunked\r\n\r\n"
            "4\r\nWiki\r\n";
        std::string part2 =
            "5\r\npedia\r\n"
            "0\r\n\r\n";

        ::write(sv[1], part1.data(), part1.size());
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
        ::write(sv[1], part2.data(), part2.size());
        ::close(sv[1]);
    });

    // Client reads from sv[0]
    std::string raw_response;
    char buffer[256];
    while (true) {
        struct pollfd pfd {};
        pfd.fd = sv[0];
        pfd.events = POLLIN;
        int pret = ::poll(&pfd, 1, 1000);
        if (pret <= 0)
            break;
        ssize_t n = ::read(sv[0], buffer, sizeof(buffer));
        if (n <= 0)
            break;
        raw_response.append(buffer, static_cast<size_t>(n));
    }
    ::close(sv[0]);
    server_thread.join();

    // Verify response contains both chunks and can be decoded
    EXPECT_TRUE(raw_response.find("Transfer-Encoding: chunked") != std::string::npos);
    EXPECT_TRUE(raw_response.find("Wikipedia") == std::string::npos);  // Encoded format
    EXPECT_TRUE(raw_response.find("4\r\nWiki\r\n") != std::string::npos);
}

TEST(DockerCollectorTest, ServiceDelegationAndOffloading) {
    auto mock_transport = std::make_shared<MockDockerTransport>();
    mock_transport->next_response = {DockerTransportStatus::kOk, 200, "[]", ""};

    auto collector = std::make_shared<DockerCollector>(mock_transport, "/mock/docker.sock");
    services::DockerService service(collector);

    EXPECT_EQ(service.socket_path(), "/mock/docker.sock");

    auto list_sync = service.list_containers();
    EXPECT_EQ(list_sync.status, DockerStatusResult::kOk);

    // Asynchronous test
    bool async_done = false;
    service.list_containers_async([&async_done](auto res) {
        EXPECT_EQ(res.status, DockerStatusResult::kOk);
        async_done = true;
    });

    // Wait for worker task queue
    for (int i = 0; i < 50 && !async_done; ++i) {
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    EXPECT_TRUE(async_done);
}

TEST(DockerCollectorTest, ConfigValidationForDocker) {
    config::Config cfg;
    cfg.collectors.docker.enabled = true;
    cfg.collectors.docker.socket_path = "/var/run/docker.sock";
    cfg.collectors.docker.timeout_ms = 2000;

    auto errors = cfg.validate();
    EXPECT_TRUE(errors.empty());

    // Empty socket path
    cfg.collectors.docker.socket_path = "";
    errors = cfg.validate();
    EXPECT_FALSE(errors.empty());
    bool found_empty_err = false;
    for (const auto& err : errors) {
        if (err.find("socket_path cannot be empty") != std::string::npos) {
            found_empty_err = true;
        }
    }
    EXPECT_TRUE(found_empty_err);

    // Remote HTTP URL rejected
    cfg.collectors.docker.socket_path = "http://localhost:2375";
    errors = cfg.validate();
    EXPECT_FALSE(errors.empty());
    bool found_http_err = false;
    for (const auto& err : errors) {
        if (err.find("remote HTTP/TCP URLs are not allowed") != std::string::npos) {
            found_http_err = true;
        }
    }
    EXPECT_TRUE(found_http_err);

    // Remote TCP URL rejected
    cfg.collectors.docker.socket_path = "tcp://127.0.0.1:2375";
    errors = cfg.validate();
    EXPECT_FALSE(errors.empty());

    // Zero timeout rejected
    cfg.collectors.docker.socket_path = "/var/run/docker.sock";
    cfg.collectors.docker.timeout_ms = 0;
    errors = cfg.validate();
    EXPECT_FALSE(errors.empty());
}

}  // namespace nodepulse::collectors
