#include <chrono>
#include <cmath>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#include <nodepulse/collectors/network_collector.hpp>
#include <nodepulse/domain/network_info.hpp>
#include <nodepulse/services/network_service.hpp>

namespace nodepulse::services {

NetworkService::NetworkService(std::shared_ptr<collectors::NetworkCollector> collector)
    : collector_(std::move(collector)) {
    if (!collector_) {
        collector_ = std::make_shared<collectors::NetworkCollector>();
    }
}

NetworkService::~NetworkService() {
    stop_sampling();
}

std::optional<std::vector<domain::NetworkInterfaceMetrics>> NetworkService::get_network_metrics() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (is_sampling_) {
        return latest_metrics_;
    }
    return sample_locked();
}

void NetworkService::start_sampling(std::chrono::milliseconds interval) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (is_sampling_) {
        return;
    }
    is_sampling_ = true;
    sampling_thread_ = std::thread([this, interval]() { sampling_loop(interval); });
}

void NetworkService::stop_sampling() {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        if (!is_sampling_) {
            return;
        }
        is_sampling_ = false;
        stop_cv_.notify_all();
    }
    if (sampling_thread_.joinable()) {
        sampling_thread_.join();
    }
}

bool NetworkService::is_sampling() const noexcept {
    return is_sampling_.load();
}

std::optional<std::vector<domain::NetworkInterfaceMetrics>> NetworkService::sample() {
    std::lock_guard<std::mutex> lock(mutex_);
    return sample_locked();
}

void NetworkService::reset_baseline() {
    std::lock_guard<std::mutex> lock(mutex_);
    baselines_.clear();
    latest_metrics_ = std::nullopt;
}

std::optional<std::vector<domain::NetworkInterfaceMetrics>> NetworkService::sample_locked() {
    if (!collector_) {
        return std::nullopt;
    }

    auto collected = collector_->collect();
    if (!collected.has_value()) {
        latest_metrics_ = std::nullopt;
        return std::nullopt;
    }

    auto now = std::chrono::steady_clock::now();
    std::unordered_set<std::string> active_ifaces;

    for (auto& iface : *collected) {
        active_ifaces.insert(iface.name);
        auto it = baselines_.find(iface.name);
        if (it == baselines_.end()) {
            // First sample for interface (warming up)
            iface.rx_bytes_per_sec = std::nullopt;
            iface.tx_bytes_per_sec = std::nullopt;
            baselines_[iface.name] = InterfaceBaseline{iface.rx_bytes, iface.tx_bytes, now};
        } else {
            auto& prev = it->second;
            double elapsed_sec = std::chrono::duration<double>(now - prev.timestamp).count();

            if (elapsed_sec > 0.001) {
                // Check counter reset or wrap
                if (iface.rx_bytes >= prev.rx_bytes) {
                    double rx_rate =
                        static_cast<double>(iface.rx_bytes - prev.rx_bytes) / elapsed_sec;
                    iface.rx_bytes_per_sec = std::round(rx_rate * 100.0) / 100.0;
                } else {
                    iface.rx_bytes_per_sec = std::nullopt;
                }

                if (iface.tx_bytes >= prev.tx_bytes) {
                    double tx_rate =
                        static_cast<double>(iface.tx_bytes - prev.tx_bytes) / elapsed_sec;
                    iface.tx_bytes_per_sec = std::round(tx_rate * 100.0) / 100.0;
                } else {
                    iface.tx_bytes_per_sec = std::nullopt;
                }

                prev = InterfaceBaseline{iface.rx_bytes, iface.tx_bytes, now};
            }
        }
    }

    // Clean up disappeared interfaces
    for (auto it = baselines_.begin(); it != baselines_.end();) {
        if (!active_ifaces.contains(it->first)) {
            it = baselines_.erase(it);
        } else {
            ++it;
        }
    }

    latest_metrics_ = collected;
    return latest_metrics_;
}

void NetworkService::sampling_loop(std::chrono::milliseconds interval) {
    std::unique_lock<std::mutex> lock(mutex_);
    sample_locked();

    while (is_sampling_) {
        if (stop_cv_.wait_for(lock, interval, [this]() { return !is_sampling_.load(); })) {
            break;
        }
        if (!is_sampling_) {
            break;
        }
        sample_locked();
    }
}

}  // namespace nodepulse::services

