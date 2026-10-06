#pragma once

#include <cstdint>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include <nodepulse/domain/service_info.hpp>

namespace nodepulse::collectors {

enum class ServiceStatusResult { kOk, kNotFound, kCollectorFailure };

struct ServiceDetailResult {
    ServiceStatusResult status{ServiceStatusResult::kCollectorFailure};
    std::optional<domain::ServiceDetail> detail{std::nullopt};
};

class ServiceCollector {
  public:
    using ListUnitsProvider =
        std::function<std::optional<std::vector<domain::ServiceInfo>>(const std::string&)>;
    using GetUnitDetailProvider = std::function<ServiceDetailResult(const std::string&)>;

    ServiceCollector() = default;
    virtual ~ServiceCollector() = default;

    // Validation & normalization helper
    static bool is_valid_unit_name(std::string_view name) noexcept;
    static std::string normalize_service_name(std::string_view name);

    // Queries
    [[nodiscard]] virtual std::optional<std::vector<domain::ServiceInfo>> list_services(
        const std::string& state_filter = "all") const;

    [[nodiscard]] virtual ServiceDetailResult get_service_detail(
        const std::string& unit_name) const;

    // Test mocking support
    void set_custom_providers(ListUnitsProvider list_provider,
                              GetUnitDetailProvider detail_provider) noexcept {
        list_provider_ = std::move(list_provider);
        detail_provider_ = std::move(detail_provider);
    }

  private:
    ListUnitsProvider list_provider_{nullptr};
    GetUnitDetailProvider detail_provider_{nullptr};
};

}  // namespace nodepulse::collectors
