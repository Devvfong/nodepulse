#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace nodepulse::domain {

struct ContainerSummary {
    std::string id;  // 64-char hex or container identifier
    std::vector<std::string> names;
    std::string image;
    std::string status;  // e.g. "Up 3 days"
    std::string state;   // e.g. "running", "exited"
    uint64_t created{0};

    bool operator==(const ContainerSummary& other) const = default;
};

struct ContainerDetail {
    std::string id;
    std::string name;
    std::string image;
    std::string status;
    std::string state;
    bool running{false};
    int32_t exit_code{0};
    std::vector<std::string> port_mappings;
    std::vector<std::string> mount_sources;
    uint64_t created{0};

    bool operator==(const ContainerDetail& other) const = default;
};

}  // namespace nodepulse::domain
