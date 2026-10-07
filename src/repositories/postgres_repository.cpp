#include <algorithm>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <exception>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include <nodepulse/domain/host_snapshot.hpp>
#include <nodepulse/repositories/postgres_repository.hpp>
#include <nodepulse/utils/logger.hpp>
#include <nodepulse/utils/security.hpp>

#include <pqxx/pqxx>

namespace nodepulse::repositories {

namespace {

std::string format_utc_timestamp(std::chrono::system_clock::time_point tp) {
    auto s = std::chrono::duration_cast<std::chrono::seconds>(tp.time_since_epoch()).count();
    std::time_t tt = static_cast<std::time_t>(s);
    std::tm tm_buf{};
    gmtime_r(&tt, &tm_buf);
    char buf[32];
    std::strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &tm_buf);
    return std::string(buf);
}

}  // namespace

PostgresRepository::PostgresRepository(std::string connection_string)
    : connection_string_(std::move(connection_string)),
      redacted_connection_string_(utils::redact_connection_string(connection_string_)) {}

PostgresRepository::~PostgresRepository() {
    std::lock_guard<std::mutex> lock(mutex_);
    disconnect_locked();
}

bool PostgresRepository::is_connected() const noexcept {
    std::lock_guard<std::mutex> lock(mutex_);
    return conn_ && conn_->is_open();
}

bool PostgresRepository::ensure_connected_locked() {
    if (conn_ && conn_->is_open()) {
        return true;
    }

    try {
        conn_ = std::make_unique<pqxx::connection>(connection_string_);
        if (conn_->is_open()) {
            utils::Logger::get()->info("Connected to PostgreSQL database at {}",
                                       redacted_connection_string_);
            return true;
        }
    } catch (const std::exception& ex) {
        disconnect_locked();
        utils::Logger::get()->warn("PostgreSQL connection error: {}",
                                   utils::redact_connection_string(ex.what()));
    }
    return false;
}

void PostgresRepository::disconnect_locked() {
    if (conn_) {
        try {
            conn_->close();
        } catch (...) {
            // Ignore close errors during disconnect
        }
        conn_.reset();
    }
}

bool PostgresRepository::init_schema() {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!ensure_connected_locked()) {
        return false;
    }

    try {
        pqxx::work tx(*conn_);
        tx.exec(R"(
            CREATE TABLE IF NOT EXISTS host_metrics (
                id BIGSERIAL PRIMARY KEY,
                captured_at TIMESTAMPTZ NOT NULL DEFAULT NOW(),
                hostname VARCHAR(255) NOT NULL,
                cpu_usage_percent DOUBLE PRECISION,
                load_1m DOUBLE PRECISION NOT NULL,
                load_5m DOUBLE PRECISION NOT NULL,
                load_15m DOUBLE PRECISION NOT NULL,
                mem_total_bytes BIGINT NOT NULL,
                mem_used_bytes BIGINT NOT NULL,
                mem_available_bytes BIGINT NOT NULL,
                swap_total_bytes BIGINT NOT NULL,
                swap_used_bytes BIGINT NOT NULL,
                net_rx_bytes_per_sec DOUBLE PRECISION,
                net_tx_bytes_per_sec DOUBLE PRECISION
            );

            CREATE INDEX IF NOT EXISTS idx_host_metrics_hostname_time
                ON host_metrics (hostname, captured_at DESC);
        )");
        tx.commit();
        utils::Logger::get()->info("PostgreSQL host_metrics schema initialized successfully");
        return true;
    } catch (const std::exception& ex) {
        disconnect_locked();
        utils::Logger::get()->error("Failed to initialize PostgreSQL schema: {}",
                                    utils::redact_connection_string(ex.what()));
        return false;
    }
}

bool PostgresRepository::save_snapshot(const domain::HostSnapshot& snapshot) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!ensure_connected_locked()) {
        return false;
    }

    std::string time_str = format_utc_timestamp(snapshot.captured_at);

    try {
        pqxx::work tx(*conn_);
        pqxx::params p;
        p.append(time_str);
        p.append(snapshot.hostname);
        p.append(snapshot.cpu_usage_percent);
        p.append(snapshot.load_1m);
        p.append(snapshot.load_5m);
        p.append(snapshot.load_15m);
        p.append(static_cast<int64_t>(snapshot.mem_total_bytes));
        p.append(static_cast<int64_t>(snapshot.mem_used_bytes));
        p.append(static_cast<int64_t>(snapshot.mem_available_bytes));
        p.append(static_cast<int64_t>(snapshot.swap_total_bytes));
        p.append(static_cast<int64_t>(snapshot.swap_used_bytes));
        p.append(snapshot.net_rx_bytes_per_sec);
        p.append(snapshot.net_tx_bytes_per_sec);

        tx.exec(
            R"(
            INSERT INTO host_metrics (
                captured_at, hostname, cpu_usage_percent,
                load_1m, load_5m, load_15m,
                mem_total_bytes, mem_used_bytes, mem_available_bytes,
                swap_total_bytes, swap_used_bytes,
                net_rx_bytes_per_sec, net_tx_bytes_per_sec
            ) VALUES (
                $1, $2, $3, $4, $5, $6, $7, $8, $9, $10, $11, $12, $13
            );
            )",
            p);
        tx.commit();
        return true;
    } catch (const std::exception& ex) {
        disconnect_locked();
        utils::Logger::get()->warn("Failed to persist host snapshot: {}",
                                   utils::redact_connection_string(ex.what()));
        return false;
    }
}

std::optional<std::vector<domain::HostMetricRecord>> PostgresRepository::query_history(
    const domain::HistoryQuery& query) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!ensure_connected_locked()) {
        return std::nullopt;
    }

    try {
        pqxx::read_transaction tx(*conn_);
        pqxx::params p;
        std::string sql =
            "SELECT id, EXTRACT(EPOCH FROM captured_at)::BIGINT AS captured_at_epoch, "
            "hostname, cpu_usage_percent, load_1m, load_5m, load_15m, "
            "mem_total_bytes, mem_used_bytes, mem_available_bytes, "
            "swap_total_bytes, swap_used_bytes, "
            "net_rx_bytes_per_sec, net_tx_bytes_per_sec "
            "FROM host_metrics WHERE 1=1";

        int param_idx = 1;
        if (query.hostname.has_value()) {
            sql += " AND hostname = $" + std::to_string(param_idx++);
            p.append(*query.hostname);
        }

        if (query.start_time.has_value()) {
            auto start_s = std::chrono::duration_cast<std::chrono::seconds>(
                               query.start_time->time_since_epoch())
                               .count();
            sql += " AND captured_at >= to_timestamp($" + std::to_string(param_idx++) + ")";
            p.append(start_s);
        }

        if (query.end_time.has_value()) {
            auto end_s =
                std::chrono::duration_cast<std::chrono::seconds>(query.end_time->time_since_epoch())
                    .count();
            sql += " AND captured_at <= to_timestamp($" + std::to_string(param_idx++) + ")";
            p.append(end_s);
        }

        sql += " ORDER BY captured_at DESC LIMIT $" + std::to_string(param_idx++);
        size_t limit = (query.limit > 0 && query.limit <= 200) ? query.limit : 50;
        p.append(static_cast<int64_t>(limit));

        auto res = tx.exec(sql, p);
        std::vector<domain::HostMetricRecord> records;
        records.reserve(res.size());

        for (const auto& row : res) {
            domain::HostMetricRecord rec;
            rec.id = row["id"].as<int64_t>();
            rec.captured_at =
                std::chrono::system_clock::from_time_t(row["captured_at_epoch"].as<int64_t>());
            rec.hostname = row["hostname"].as<std::string>();

            if (!row["cpu_usage_percent"].is_null()) {
                rec.cpu_usage_percent = row["cpu_usage_percent"].as<double>();
            } else {
                rec.cpu_usage_percent = std::nullopt;
            }

            rec.load_1m = row["load_1m"].as<double>();
            rec.load_5m = row["load_5m"].as<double>();
            rec.load_15m = row["load_15m"].as<double>();
            rec.mem_total_bytes = static_cast<uint64_t>(row["mem_total_bytes"].as<int64_t>());
            rec.mem_used_bytes = static_cast<uint64_t>(row["mem_used_bytes"].as<int64_t>());
            rec.mem_available_bytes =
                static_cast<uint64_t>(row["mem_available_bytes"].as<int64_t>());
            rec.swap_total_bytes = static_cast<uint64_t>(row["swap_total_bytes"].as<int64_t>());
            rec.swap_used_bytes = static_cast<uint64_t>(row["swap_used_bytes"].as<int64_t>());

            if (!row["net_rx_bytes_per_sec"].is_null()) {
                rec.net_rx_bytes_per_sec = row["net_rx_bytes_per_sec"].as<double>();
            } else {
                rec.net_rx_bytes_per_sec = std::nullopt;
            }

            if (!row["net_tx_bytes_per_sec"].is_null()) {
                rec.net_tx_bytes_per_sec = row["net_tx_bytes_per_sec"].as<double>();
            } else {
                rec.net_tx_bytes_per_sec = std::nullopt;
            }

            records.push_back(std::move(rec));
        }

        return records;
    } catch (const std::exception& ex) {
        disconnect_locked();
        utils::Logger::get()->warn("Failed to query host metric history: {}",
                                   utils::redact_connection_string(ex.what()));
        return std::nullopt;
    }
}

// -----------------------------------------------------------------------------
// MockPostgresRepository implementation
// -----------------------------------------------------------------------------

bool MockPostgresRepository::save_snapshot(const domain::HostSnapshot& snapshot) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!connected_) {
        return false;
    }

    domain::HostMetricRecord rec;
    rec.id = next_id_++;
    rec.captured_at = snapshot.captured_at;
    rec.hostname = snapshot.hostname;
    rec.cpu_usage_percent = snapshot.cpu_usage_percent;
    rec.load_1m = snapshot.load_1m;
    rec.load_5m = snapshot.load_5m;
    rec.load_15m = snapshot.load_15m;
    rec.mem_total_bytes = snapshot.mem_total_bytes;
    rec.mem_used_bytes = snapshot.mem_used_bytes;
    rec.mem_available_bytes = snapshot.mem_available_bytes;
    rec.swap_total_bytes = snapshot.swap_total_bytes;
    rec.swap_used_bytes = snapshot.swap_used_bytes;
    rec.net_rx_bytes_per_sec = snapshot.net_rx_bytes_per_sec;
    rec.net_tx_bytes_per_sec = snapshot.net_tx_bytes_per_sec;

    records_.push_back(std::move(rec));
    return true;
}

std::optional<std::vector<domain::HostMetricRecord>> MockPostgresRepository::query_history(
    const domain::HistoryQuery& query) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!connected_) {
        return std::nullopt;
    }

    std::vector<domain::HostMetricRecord> filtered;
    for (const auto& r : records_) {
        if (query.hostname.has_value() && r.hostname != *query.hostname) {
            continue;
        }
        if (query.start_time.has_value() && r.captured_at < *query.start_time) {
            continue;
        }
        if (query.end_time.has_value() && r.captured_at > *query.end_time) {
            continue;
        }
        filtered.push_back(r);
    }

    std::sort(filtered.begin(), filtered.end(),
              [](const auto& a, const auto& b) { return a.captured_at > b.captured_at; });

    size_t limit = (query.limit > 0 && query.limit <= 200) ? query.limit : 50;
    if (filtered.size() > limit) {
        filtered.resize(limit);
    }

    return filtered;
}

}  // namespace nodepulse::repositories
