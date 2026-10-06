#pragma once

#include <memory>
#include <optional>

#include <nodepulse/collectors/memory_collector.hpp>
#include <nodepulse/domain/memory_info.hpp>

namespace nodepulse::services {

class MemoryService {
  public:
    explicit MemoryService(std::shared_ptr<collectors::MemoryCollector> collector =
                               std::make_shared<collectors::MemoryCollector>());
    ~MemoryService() = default;

    [[nodiscard]] std::optional<domain::MemoryMetrics> get_memory_metrics() const;

  private:
    std::shared_ptr<collectors::MemoryCollector> collector_;
};

}  // namespace nodepulse::services
