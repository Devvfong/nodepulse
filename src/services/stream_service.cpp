#include <algorithm>
#include <cmath>
#include <memory>
#include <mutex>
#include <string>
#include <utility>
#include <vector>

#include <nodepulse/services/stream_service.hpp>
#include <nodepulse/utils/error_response.hpp>

namespace nodepulse::services {

// -----------------------------------------------------------------------------
// DrogonSseClient
// -----------------------------------------------------------------------------

DrogonSseClient::DrogonSseClient(drogon::ResponseStreamPtr stream) : stream_(std::move(stream)) {
    alive_ = (stream_ != nullptr);
}

DrogonSseClient::~DrogonSseClient() {
    do_close();
}

bool DrogonSseClient::send_data(const std::string& chunk) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!alive_ || !stream_) {
        return false;
    }
    try {
        bool ok = stream_->send(chunk);
        if (!ok) {
            alive_ = false;
        }
        return ok;
    } catch (...) {
        alive_ = false;
        return false;
    }
}

void DrogonSseClient::do_close() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (stream_) {
        try {
            stream_->close();
        } catch (...) {}
        stream_.reset();
    }
    alive_ = false;
}

void DrogonSseClient::close() {
    do_close();
}

bool DrogonSseClient::is_alive() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return alive_;
}

// -----------------------------------------------------------------------------
// SseSlotReservation
// -----------------------------------------------------------------------------

SseSlotReservation::SseSlotReservation(StreamService* service) : service_(service) {}

SseSlotReservation::~SseSlotReservation() {
    release();
}

SseSlotReservation::SseSlotReservation(SseSlotReservation&& other) noexcept
    : service_(other.service_), committed_(other.committed_) {
    other.service_ = nullptr;
    other.committed_ = true;
}

SseSlotReservation& SseSlotReservation::operator=(SseSlotReservation&& other) noexcept {
    if (this != &other) {
        release();
        service_ = other.service_;
        committed_ = other.committed_;
        other.service_ = nullptr;
        other.committed_ = true;
    }
    return *this;
}

bool SseSlotReservation::commit(std::shared_ptr<ISseClient> client) {
    if (!service_ || committed_) {
        return false;
    }
    committed_ = true;
    StreamService* svc = service_;
    service_ = nullptr;
    return svc->commit_reservation(std::move(client));
}

void SseSlotReservation::release() {
    if (service_ && !committed_) {
        StreamService* svc = service_;
        service_ = nullptr;
        svc->release_reservation();
    }
}

// -----------------------------------------------------------------------------
// StreamService
// -----------------------------------------------------------------------------

StreamService::StreamService(std::shared_ptr<CpuService> cpu_service,
                             std::shared_ptr<MemoryService> memory_service,
                             std::shared_ptr<NetworkService> network_service,
                             config::SseConfig config)
    : cpu_service_(std::move(cpu_service)),
      memory_service_(std::move(memory_service)),
      network_service_(std::move(network_service)),
      config_(config) {}

StreamService::~StreamService() {
    stop_streaming();
}

size_t StreamService::client_count() const {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    size_t count = 0;
    for (const auto& c : clients_) {
        if (c && c->is_alive()) {
            ++count;
        }
    }
    return count;
}

size_t StreamService::active_slot_count() const {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    size_t count = reserved_slots_;
    for (const auto& c : clients_) {
        if (c && c->is_alive()) {
            ++count;
        }
    }
    return count;
}

std::unique_ptr<SseSlotReservation> StreamService::try_reserve_slot() {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    if (!config_.enabled) {
        return nullptr;
    }

    // Prune dead clients before admission check
    clients_.erase(std::remove_if(clients_.begin(), clients_.end(),
                                  [](const auto& c) {
                                      if (!c || !c->is_alive()) {
                                          if (c) {
                                              c->close();
                                          }
                                          return true;
                                      }
                                      return false;
                                  }),
                   clients_.end());

    if (clients_.size() + reserved_slots_ >= config_.max_clients) {
        return nullptr;
    }

    ++reserved_slots_;
    return std::make_unique<SseSlotReservation>(this);
}

void StreamService::release_reservation() {
    std::lock_guard<std::mutex> lock(clients_mutex_);
    if (reserved_slots_ > 0) {
        --reserved_slots_;
    }
}

bool StreamService::commit_reservation(std::shared_ptr<ISseClient> client) {
    if (!client) {
        release_reservation();
        return false;
    }

    std::string initial_frame;
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        if (reserved_slots_ > 0) {
            --reserved_slots_;
        }
        if (!config_.enabled || !client->is_alive()) {
            client->close();
            return false;
        }
        clients_.push_back(client);
        auto pulse = create_pulse();
        uint64_t id = ++sequence_counter_;
        initial_frame = format_pulse_frame(id, pulse);
    }

    if (!client->send_data(initial_frame)) {
        client->close();
        return false;
    }
    return true;
}

bool StreamService::try_add_client(std::shared_ptr<ISseClient> client) {
    if (!client) {
        return false;
    }

    std::string initial_frame;
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        if (!config_.enabled) {
            client->close();
            return false;
        }

        // Prune dead clients before checking capacity
        clients_.erase(std::remove_if(clients_.begin(), clients_.end(),
                                      [](const auto& c) {
                                          if (!c || !c->is_alive()) {
                                              if (c) {
                                                  c->close();
                                              }
                                              return true;
                                          }
                                          return false;
                                      }),
                       clients_.end());

        if (clients_.size() + reserved_slots_ >= config_.max_clients) {
            client->close();
            return false;
        }

        clients_.push_back(client);
        auto pulse = create_pulse();
        uint64_t id = ++sequence_counter_;
        initial_frame = format_pulse_frame(id, pulse);
    }

    if (!client->send_data(initial_frame)) {
        client->close();
        return false;
    }
    return true;
}

void StreamService::add_client(std::shared_ptr<ISseClient> client) {
    try_add_client(std::move(client));
}

void StreamService::remove_client(const std::shared_ptr<ISseClient>& client) {
    if (!client) {
        return;
    }
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        std::erase(clients_, client);
    }
    client->close();
}

void StreamService::start_streaming(std::chrono::milliseconds interval) {
    if (!config_.enabled) {
        return;
    }
    std::lock_guard<std::mutex> lock(loop_mutex_);
    if (is_running_) {
        return;
    }
    is_running_ = true;
    worker_thread_ = std::thread([this, interval] { streaming_loop(interval); });
}

void StreamService::stop_streaming() {
    {
        std::lock_guard<std::mutex> lock(loop_mutex_);
        if (is_running_) {
            is_running_ = false;
            loop_cv_.notify_all();
        }
    }
    if (worker_thread_.joinable()) {
        worker_thread_.join();
    }

    std::vector<std::shared_ptr<ISseClient>> remaining;
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        remaining = std::move(clients_);
        clients_.clear();
    }
    for (const auto& client : remaining) {
        client->close();
    }
}

bool StreamService::is_streaming() const noexcept {
    return is_running_.load();
}

void StreamService::streaming_loop(std::chrono::milliseconds interval) {
    std::unique_lock<std::mutex> lock(loop_mutex_);
    while (is_running_) {
        loop_cv_.wait_for(lock, interval, [this] { return !is_running_; });
        if (!is_running_) {
            break;
        }
        broadcast_pulse();
    }
}

size_t StreamService::broadcast_pulse() {
    std::vector<std::shared_ptr<ISseClient>> active_clients;
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        if (clients_.empty()) {
            return 0;
        }
        active_clients = clients_;
    }

    auto pulse = create_pulse();
    uint64_t id = ++sequence_counter_;
    std::string frame = format_pulse_frame(id, pulse);

    std::vector<std::shared_ptr<ISseClient>> dead_clients;
    size_t successful_sends = 0;

    for (const auto& client : active_clients) {
        if (!client->is_alive() || !client->send_data(frame)) {
            dead_clients.push_back(client);
        } else {
            ++successful_sends;
        }
    }

    if (!dead_clients.empty()) {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        for (const auto& dead : dead_clients) {
            std::erase(clients_, dead);
        }
    }

    for (const auto& dead : dead_clients) {
        dead->close();
    }

    return successful_sends;
}

size_t StreamService::send_heartbeat() {
    std::vector<std::shared_ptr<ISseClient>> active_clients;
    {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        if (clients_.empty()) {
            return 0;
        }
        active_clients = clients_;
    }

    std::string frame = format_heartbeat_frame();
    std::vector<std::shared_ptr<ISseClient>> dead_clients;
    size_t successful_sends = 0;

    for (const auto& client : active_clients) {
        if (!client->is_alive() || !client->send_data(frame)) {
            dead_clients.push_back(client);
        } else {
            ++successful_sends;
        }
    }

    if (!dead_clients.empty()) {
        std::lock_guard<std::mutex> lock(clients_mutex_);
        for (const auto& dead : dead_clients) {
            std::erase(clients_, dead);
        }
    }

    for (const auto& dead : dead_clients) {
        dead->close();
    }

    return successful_sends;
}

domain::MetricPulse StreamService::create_pulse() const {
    domain::MetricPulse pulse;
    pulse.timestamp = utils::get_current_iso8601_timestamp();

    if (cpu_service_) {
        auto cpu = cpu_service_->get_cpu_metrics();
        if (cpu.has_value() && cpu->usage_percent.has_value()) {
            pulse.cpu_usage_percent = cpu->usage_percent;
        } else {
            pulse.cpu_usage_percent = std::nullopt;
        }
    } else {
        pulse.cpu_usage_percent = std::nullopt;
    }

    if (memory_service_) {
        auto mem = memory_service_->get_memory_metrics();
        if (mem.has_value()) {
            pulse.memory_usage_percent = mem->usage_percent;
            pulse.memory_used_bytes = mem->used_bytes;
        }
    }

    if (network_service_) {
        auto net = network_service_->get_network_metrics();
        if (net.has_value() && !net->empty()) {
            bool has_non_loopback = false;
            for (const auto& iface : *net) {
                if (iface.name != "lo") {
                    has_non_loopback = true;
                    break;
                }
            }

            bool any_rx = false;
            bool any_tx = false;
            double sum_rx = 0.0;
            double sum_tx = 0.0;

            for (const auto& iface : *net) {
                if (has_non_loopback && iface.name == "lo") {
                    continue;
                }
                if (iface.rx_bytes_per_sec.has_value()) {
                    any_rx = true;
                    sum_rx += *iface.rx_bytes_per_sec;
                }
                if (iface.tx_bytes_per_sec.has_value()) {
                    any_tx = true;
                    sum_tx += *iface.tx_bytes_per_sec;
                }
            }

            if (any_rx) {
                pulse.network_rx_bytes_sec = std::round(sum_rx * 100.0) / 100.0;
            } else {
                pulse.network_rx_bytes_sec = std::nullopt;
            }

            if (any_tx) {
                pulse.network_tx_bytes_sec = std::round(sum_tx * 100.0) / 100.0;
            } else {
                pulse.network_tx_bytes_sec = std::nullopt;
            }
        }
    }

    return pulse;
}

std::string StreamService::format_pulse_frame(uint64_t id, const domain::MetricPulse& pulse) {
    nlohmann::json data_json = pulse;
    std::string data_str = data_json.dump();
    std::string frame;
    frame.reserve(data_str.size() + 64);
    frame += "id: ";
    frame += std::to_string(id);
    frame += "\nevent: metric_pulse\ndata: ";
    frame += data_str;
    frame += "\n\n";
    return frame;
}

std::string StreamService::format_heartbeat_frame() {
    return ": keepalive\n\n";
}

}  // namespace nodepulse::services
