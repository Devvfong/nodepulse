#include <memory>
#include <optional>
#include <utility>

#include <nodepulse/collectors/system_collector.hpp>
#include <nodepulse/domain/system_info.hpp>
#include <nodepulse/services/system_service.hpp>

namespace nodepulse::services {

SystemService::SystemService(std::shared_ptr<collectors::SystemCollector> collector)
    : collector_(std::move(collector)) {
    if (!collector_) {
        collector_ = std::make_shared<collectors::SystemCollector>();
    }
}

std::optional<domain::SystemInfo> SystemService::get_system_info() const {
    return collector_->collect();
}

}  // namespace nodepulse::services
