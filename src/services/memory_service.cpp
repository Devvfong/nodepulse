#include <memory>
#include <optional>
#include <utility>

#include <nodepulse/collectors/memory_collector.hpp>
#include <nodepulse/domain/memory_info.hpp>
#include <nodepulse/services/memory_service.hpp>

namespace nodepulse::services {

MemoryService::MemoryService(std::shared_ptr<collectors::MemoryCollector> collector)
    : collector_(std::move(collector)) {
    if (!collector_) {
        collector_ = std::make_shared<collectors::MemoryCollector>();
    }
}

std::optional<domain::MemoryMetrics> MemoryService::get_memory_metrics() const {
    return collector_->collect();
}

}  // namespace nodepulse::services
