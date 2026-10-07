#pragma once

#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

#include <nodepulse/domain/host_snapshot.hpp>

// Forward declaration for libpqxx connection
namespace pqxx {
class connection;
}

namespace nodepulse::repositories {

/**
 * @brief Abstract interface for host metric history persistence (Phase 14).
 */
class IPostgresRepository {
  public:
    virtual ~IPostgresRepository() = default;

    /**
     * @brief Check whether the repository is currently connected and healthy.
     */
    [[nodiscard]] virtual bool is_connected() const noexcept = 0;

    /**
     * @brief Initialize database schema (create host_metrics table and indices if not exists).
     * @return true on success, false on failure (logs error without throwing).
     */
    virtual bool init_schema() = 0;

    /**
     * @brief Save a single host snapshot record into host_metrics table.
     * @param snapshot The host snapshot to persist.
     * @return true on success, false on failure (drops sample and logs error without crashing).
     */
    virtual bool save_snapshot(const domain::HostSnapshot& snapshot) = 0;

    /**
     * @brief Query historical metric records matching filter criteria.
     * @param query The filter criteria (time range, hostname, limit).
     * @return Vector of historical records, or nullopt if query failed / disconnected.
     */
    [[nodiscard]] virtual std::optional<std::vector<domain::HostMetricRecord>> query_history(
        const domain::HistoryQuery& query) = 0;
};

/**
 * @brief Production PostgreSQL repository implementation using libpqxx.
 */
class PostgresRepository : public IPostgresRepository {
  public:
    explicit PostgresRepository(std::string connection_string);
    ~PostgresRepository() override;

    PostgresRepository(const PostgresRepository&) = delete;
    PostgresRepository& operator=(const PostgresRepository&) = delete;
    PostgresRepository(PostgresRepository&&) = delete;
    PostgresRepository& operator=(PostgresRepository&&) = delete;

    [[nodiscard]] bool is_connected() const noexcept override;
    bool init_schema() override;
    bool save_snapshot(const domain::HostSnapshot& snapshot) override;
    [[nodiscard]] std::optional<std::vector<domain::HostMetricRecord>> query_history(
        const domain::HistoryQuery& query) override;

    [[nodiscard]] const std::string& redacted_connection_string() const noexcept {
        return redacted_connection_string_;
    }

  private:
    std::string connection_string_;
    std::string redacted_connection_string_;
    mutable std::mutex mutex_;
    std::unique_ptr<pqxx::connection> conn_;

    bool ensure_connected_locked();
    void disconnect_locked();
};

/**
 * @brief Thread-safe mock repository for deterministic unit tests.
 */
class MockPostgresRepository : public IPostgresRepository {
  public:
    MockPostgresRepository() = default;
    ~MockPostgresRepository() override = default;

    void set_connected(bool connected) {
        std::lock_guard<std::mutex> lock(mutex_);
        connected_ = connected;
    }

    [[nodiscard]] bool is_connected() const noexcept override {
        std::lock_guard<std::mutex> lock(mutex_);
        return connected_;
    }

    bool init_schema() override {
        std::lock_guard<std::mutex> lock(mutex_);
        schema_initialized_ = true;
        return connected_;
    }

    [[nodiscard]] bool is_schema_initialized() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return schema_initialized_;
    }

    bool save_snapshot(const domain::HostSnapshot& snapshot) override;

    [[nodiscard]] std::optional<std::vector<domain::HostMetricRecord>> query_history(
        const domain::HistoryQuery& query) override;

    void clear() {
        std::lock_guard<std::mutex> lock(mutex_);
        records_.clear();
        next_id_ = 1;
    }

    [[nodiscard]] size_t record_count() const {
        std::lock_guard<std::mutex> lock(mutex_);
        return records_.size();
    }

  private:
    mutable std::mutex mutex_;
    bool connected_{true};
    bool schema_initialized_{false};
    int64_t next_id_{1};
    std::vector<domain::HostMetricRecord> records_;
};

}  // namespace nodepulse::repositories
