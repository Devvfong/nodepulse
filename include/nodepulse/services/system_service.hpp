#pragma once

#include <memory>
#include <optional>

#include <nodepulse/collectors/system_collector.hpp>
#include <nodepulse/domain/system_info.hpp>

namespace nodepulse::services {

class SystemService {
  public:
    explicit SystemService(std::shared_ptr<collectors::SystemCollector> collector =
                               std::make_shared<collectors::SystemCollector>());
    ~SystemService() = default;

    [[nodiscard]] std::optional<domain::SystemInfo> get_system_info() const;

  private:
    std::shared_ptr<collectors::SystemCollector> collector_;
};

}  // namespace nodepulse::services
