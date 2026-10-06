#pragma once

#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <nodepulse/domain/container_info.hpp>

namespace nodepulse::collectors {

enum class DockerTransportStatus {
    kOk,
    kSocketNotFound,
    kPermissionDenied,
    kConnectionRefused,
    kTimeout,
    kMalformedResponse,
    kResponseTooLarge,
    kDisabled
};

struct DockerHttpResponse {
    DockerTransportStatus status{DockerTransportStatus::kOk};
    int http_status{0};
    std::string body;
    std::string error_message;
};

class IDockerTransport {
  public:
    virtual ~IDockerTransport() = default;
    [[nodiscard]] virtual DockerHttpResponse get(const std::string& path) = 0;
};

class DockerUnixSocketTransport : public IDockerTransport {
  public:
    explicit DockerUnixSocketTransport(std::string socket_path = "/var/run/docker.sock",
                                       uint32_t timeout_ms = 2000, bool enabled = true);
    ~DockerUnixSocketTransport() override = default;

    [[nodiscard]] DockerHttpResponse get(const std::string& path) override;

    [[nodiscard]] const std::string& socket_path() const noexcept {
        return socket_path_;
    }
    [[nodiscard]] uint32_t timeout_ms() const noexcept {
        return timeout_ms_;
    }
    [[nodiscard]] bool is_enabled() const noexcept {
        return enabled_;
    }
    void set_enabled(bool enabled) noexcept {
        enabled_ = enabled;
    }

  private:
    std::string socket_path_;
    uint32_t timeout_ms_;
    bool enabled_;
};

enum class DockerStatusResult { kOk, kNotFound, kUnavailable, kCollectorFailure };

struct DockerContainersResult {
    DockerStatusResult status{DockerStatusResult::kOk};
    std::optional<std::vector<domain::ContainerSummary>> containers{std::nullopt};
    std::string error_message;
};

struct DockerContainerDetailResult {
    DockerStatusResult status{DockerStatusResult::kOk};
    std::optional<domain::ContainerDetail> detail{std::nullopt};
    std::string error_message;
};

class DockerCollector {
  public:
    explicit DockerCollector(std::shared_ptr<IDockerTransport> transport = nullptr,
                             std::string socket_path = "/var/run/docker.sock");
    virtual ~DockerCollector() = default;

    [[nodiscard]] virtual DockerContainersResult list_containers();
    [[nodiscard]] virtual DockerContainerDetailResult get_container(const std::string& id);

    [[nodiscard]] static bool is_valid_container_id(std::string_view id) noexcept;
    [[nodiscard]] static uint64_t parse_iso8601_to_epoch(std::string_view str) noexcept;

    // Docker Stats Semantics Calculations
    [[nodiscard]] static double calculate_cpu_percent(uint64_t cpu_total, uint64_t prev_cpu_total,
                                                      uint64_t sys_total, uint64_t prev_sys_total,
                                                      uint32_t online_cpus = 1) noexcept;
    [[nodiscard]] static double calculate_memory_percent(uint64_t usage, uint64_t limit,
                                                         uint64_t cache = 0) noexcept;

    [[nodiscard]] const std::string& socket_path() const noexcept {
        return socket_path_;
    }

  private:
    std::shared_ptr<IDockerTransport> transport_;
    std::string socket_path_;
};

}  // namespace nodepulse::collectors
